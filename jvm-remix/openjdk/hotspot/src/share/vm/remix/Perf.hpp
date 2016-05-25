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
#ifndef __REMIX_PERF_HPP__
#define __REMIX_PERF_HPP__

#include <stdint.h>
#include "perf_event_v4.h"

struct DataRecord
{
//    void* ip;
    uint64_t time;
    void* data;
};

class Perf
{
public:
    Perf();
    ~Perf();

public:
    void init(int buf_bits, int sample_period, int evt, bool time);
    bool is_ok() const { return _ok; }

    bool open(bool enable = true);
    void close();

    bool enable();
    bool disable();
    bool is_enabled();

    inline void store_disable() 
    { 
        if(_disable_count == 0) 
        {
            _old_enabled = _enabled; 
            _enabled = false; 
        }
        ++ _disable_count;
    }
    inline void restore_disable() 
    { 
        if(_disable_count > 0) 
            -- _disable_count;
        else
            _enabled = _old_enabled;
    }

public:
    bool start_iterate();
    bool next(DataRecord* dr);
    bool next_timed(DataRecord* dr);
    void abort_iterate();
 
private:
    friend class PerfIterator;

    bool _ok, _enabled, _old_enabled;
    int _disable_count;
    unsigned _page_size, _buf_size, _buf_mask;
    struct perf_event_attr _attr;
    struct perf_event_mmap_page *_mpage;
    char *_data, *_data_end;
    int _fd;

//    int _lost;

    uint64_t _head, _tail; 
    
};


#endif // __REMIX_PERF_HPP__
