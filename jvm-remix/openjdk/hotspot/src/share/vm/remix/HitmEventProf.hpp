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
#ifndef __REMIX_HITM_EVENT_PROF_HPP__
#define __REMIX_HITM_EVENT_PROF_HPP__

#include "runtime/globals.hpp"
#include "remix/HWDetect.hpp"
#include "remix/Perf.hpp"

#define BUF_BITS        6
#define SAMPLE_PERIOD   10000
 
class HitmEventProf 
{
public:
    HitmEventProf() : _perfs(NULL), _perfs_count(0), _iterate_id(-1)
    {
    }

    ~HitmEventProf()
    {
        if(_perfs != NULL)
            delete[] _perfs;
    }

    void init()
    {
        SystemDetect::RemixSystemDetect& det = SystemDetect::RemixSystemDetect::instance();
        _perfs_count = det.events_count();
        _perfs = new Perf[_perfs_count];
        for(int i = 0; i < _perfs_count; ++ i)
            _perfs[i].init(BUF_BITS, REMIXSamplingRatio, det.event_id(i), REMIXTiming);
    }

    bool is_ok()
    {  
        int count = 0;
        for(int i = 0; i < _perfs_count; ++ i)
            if(_perfs[i].is_ok())
                ++ count;
        if(REMIXVerbose && count != _perfs_count)
        {
            tty->print("REMIX: Error: created only %i/%i counters.\n", count, _perfs_count);
            return false;
        }
        return true;
    }

    bool open(bool enable = true)
    {
        int count = 0;
        for(int i = 0; i < _perfs_count; ++ i)
            if(_perfs[i].open(enable))
                ++ count;
        if(REMIXVerbose && count != _perfs_count)
            tty->print("REMIX: Warning: opened only %i/%i counters.\n", count, _perfs_count);
        return count > 0;
    }

    void close()
    {
        for(int i = 0; i < _perfs_count; ++ i)
            _perfs[i].close();
    }

    bool enable()
    {
        int count = 0;
        for(int i = 0; i < _perfs_count; ++ i)
            if(_perfs[i].enable())
                ++ count;
        if(REMIXVerbose && count != _perfs_count)
            tty->print("REMIX: Warning: enabled only %i/%i counters.\n", count, _perfs_count);
        return count > 0;
    }
    bool disable()
    {
        int count = 0;
        for(int i = 0; i < _perfs_count; ++ i)
            if(_perfs[i].disable())
                ++ count;
        if(REMIXVerbose && count != _perfs_count)
            tty->print("REMIX: Warning: disabled only %i/%i counters.\n", count, _perfs_count);
        return count > 0;
    }
    bool is_enabled()
    {
        int count = 0;
        for(int i = 0; i < _perfs_count; ++ i)
            if(_perfs[i].is_enabled())
                return true;
        return false;
    }

    inline void store_disable()
    {   
        for(int i = 0; i < _perfs_count; ++ i)
            _perfs[i].store_disable();
    }

    inline void restore_disable()
    {   
        for(int i = 0; i < _perfs_count; ++ i)
            _perfs[i].restore_disable();
    }

    bool start_iterate()
    {
        _iterate_id = 0;
        for(int i = _iterate_id; i < _perfs_count; ++ i)
            if(_perfs[i].start_iterate())
                return true;
        return false;
    }

    bool next(DataRecord* dr)
    {
        while(_iterate_id < _perfs_count)
        {
            if(_perfs[_iterate_id].next(dr))
                return true;
                
            for(++_iterate_id; _iterate_id < _perfs_count; ++_iterate_id)
                if(_perfs[_iterate_id].start_iterate())
                    break;
        }
        return false;
    }
        
    bool next_timed(DataRecord* dr)
    {
        while(_iterate_id < _perfs_count)
        {
            if(_perfs[_iterate_id].next_timed(dr))
                return true;
                
            for(++_iterate_id; _iterate_id < _perfs_count; ++_iterate_id)
                if(_perfs[_iterate_id].start_iterate())
                    break;
        }
        return false;
    }

    void abort_iterate()
    {
        _perfs[_iterate_id].abort_iterate();
    }
    
private:
    Perf* _perfs;
    int _perfs_count;
    int _iterate_id;
};

#endif // __REMIX_HITM_EVENT_PROF_HPP__

