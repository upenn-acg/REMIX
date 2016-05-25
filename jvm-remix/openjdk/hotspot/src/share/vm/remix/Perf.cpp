/*
 * Copyright (c) 2015, Ariel Eizenberg, <arieleiz@seas.upenn.edu>
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

// REMIX
#include <asm/unistd.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "utilities/exceptions.hpp"
#include "utilities/ostream.hpp"
#include "Perf.hpp"

#define EXPECTED_SIZE_TIME (sizeof(perf_event_header) + 2 * sizeof(void*))
#define EXPECTED_SIZE (sizeof(perf_event_header) + 1 * sizeof(void*))
static inline void mb() { asm volatile ("":::"memory"); }

Perf::Perf() : _ok(false), _fd(-1), 
    _mpage((struct perf_event_mmap_page*)-1), _data(NULL), _data_end(NULL),
    /*_lost(0),*/ _head(0), _tail(0), _enabled(false), _old_enabled(false), _disable_count(0)
{
}

void Perf::init(int buf_bits, int sample_period, int evt, bool time) 
{
    _page_size = sysconf(_SC_PAGESIZE);
    _buf_size = (1U << buf_bits) * _page_size;

    memset(&_attr, 0, sizeof(_attr));
    _attr.type = PERF_TYPE_RAW;
    _attr.size = sizeof(_attr);
    _attr.sample_period = sample_period;
    if(time)
        _attr.sample_type = PERF_SAMPLE_ADDR | PERF_SAMPLE_TIME;
    else
        _attr.sample_type = PERF_SAMPLE_ADDR;

    //_attr.sample_type = PERF_SAMPLE_ADDR | PERF_SAMPLE_IP;
    _attr.exclude_kernel = 1;
    _attr.precise_ip = 2;
    _attr.config = evt;
    _attr.disabled = 1;

    _ok = true;

//    if(REMIXVerbose)
//        tty->print("Opening event: 0x%x\n", evt);
}

Perf::~Perf()
{
    close();
}

void Perf::close()
{
    if(_ok)
    {
        _ok = false;
        if(_enabled)
            disable();
        if (_mpage != (struct perf_event_mmap_page *)-1L)
            munmap(_mpage, _page_size + _buf_size);
        if (_fd != -1)
            ::close(_fd);
    }
}

bool Perf::open(bool do_enable)
{
    _fd = syscall(__NR_perf_event_open, &_attr, 0, -1, -1, 0);
    if (_fd < 0)
    {
        tty->print("REMIX: Error: perf_event_open() error: %i\n", errno);
        _ok = false;
        return false;
    }

    _mpage = (struct perf_event_mmap_page*)mmap(NULL,  _page_size + _buf_size, PROT_READ|PROT_WRITE, MAP_SHARED, _fd, 0);
    if (_mpage == (struct perf_event_mmap_page *)-1L)
    {
        tty->print("REMIX: Error: mmap() error: %i\n", errno);
//        return false;
        ::close(_fd);
        _fd = -1;
        return true;
    }
    _data = ((char*)_mpage) + _page_size;
    _data_end = _data + _buf_size;
    _buf_mask = _buf_size - 1;
    assert(_mpage->data_size == _buf_size, "Invalid data_size of mapped page");
    assert(_mpage->data_offset == _page_size, "Invalid data_offset of mapped page");
    if(do_enable)
    {
        tty->print("REMIX: Enabled!\n");
        return enable();
    }

    return true;
}

bool Perf::is_enabled()
{
    return _enabled;
}

bool Perf::enable()
{
    if(_fd < 0) return true;

    if(ioctl(_fd, PERF_EVENT_IOC_ENABLE, 0) != 0)
        return false;
    _enabled = true;
//    printf("ENABLED %p\n", this);
    return true;
}

bool Perf::disable()
{
    if(_fd < 0) return true;
    _enabled = false;
//    printf("DISABLED %p\n", this);
    return ioctl(_fd, PERF_EVENT_IOC_DISABLE, 0) == 0;
}

bool Perf::start_iterate()
{
    if(_ok && _fd < 0) return true;

    if(!_ok || _mpage == (struct perf_event_mmap_page*)-1)
        return false;
    _head = _mpage->data_head;
    _tail = _mpage->data_tail;
    mb();
    return true;
}

bool Perf::next(DataRecord* dr)
{
    if(_ok && _fd < 0) return false;
    while(_tail != _head)
    { 
        struct perf_event_header* hdr = (struct perf_event_header*)(_data + (_tail & _buf_mask));
        size_t size = hdr->size;
        _tail += size;

        if(hdr->type != PERF_RECORD_SAMPLE)
        {
            // XXX
            //_lost += 1;
            continue;
        }

        if(size != EXPECTED_SIZE)
            break;
    
        void** base = (void**)(hdr + 1);
/*        if((void*)&base[1] >= _data_end)
        {
            dr->ip = base[0];
            base = (void**)_data;
            dr->data = base[1];
        } 
        else 
        {*/
            if((void*)base >= _data_end)
                base = (void**)_data;
           // dr->ip = base[0];
           // dr->data = base[1];
           dr->data = base[0];
        /*}*/

        return true;
    }
    abort_iterate();
    return false;
}   

bool Perf::next_timed(DataRecord* dr)
{
    if(_ok && _fd < 0) return false;
    while(_tail != _head)
    { 
        struct perf_event_header* hdr = (struct perf_event_header*)(_data + (_tail & _buf_mask));
        size_t size = hdr->size;
        _tail += size;

        if(hdr->type != PERF_RECORD_SAMPLE)
        {
            // XXX
            //_lost += 1;
            continue;
        }

        if(size != EXPECTED_SIZE_TIME)
            break;
    
        char* base = (char*)(hdr + 1);
        if(base >= _data_end) base = (char*)_data;
        dr->time = *((uint64_t*)base);
        base += sizeof(uint64_t);

        if(base >= _data_end) base = (char*)_data;
        dr->data = *((void**)base);

        return true;
    }
    abort_iterate();
    return false;
}   
    
void Perf::abort_iterate()
{
    _mpage->data_tail = _head;
    mb();
}



