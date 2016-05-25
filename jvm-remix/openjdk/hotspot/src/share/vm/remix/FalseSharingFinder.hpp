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

//REMIX
#ifndef __REMIX_FALSE_SHARING_FINDER_HPP__
#define __REMIX_FALSE_SHARING_FINDER_HPP__

#include "runtime/thread.hpp"
#include "runtime/globals.hpp"
#include "Timing.hpp"

class FalseSharingFinder
{
public:
    static void begin_vmop_safepoint();
    static void end_vmop_safepoint();
    static void init_vm();
    static void done_vm();

    static inline void thread_starting(HitmEventProf& thread_perf)
    {
        if(REMIXLevel != REMIX_NONE && thread_perf.is_ok())
        {
            Clock clock(STAGE_THREAD_START);
            thread_perf.open();
        }
    }

    static inline void thread_ending(HitmEventProf& thread_perf)
    {
        if(REMIXLevel != REMIX_NONE && thread_perf.is_ok())
        {
            Clock clock(STAGE_THREAD_END);
            merge_thread_data(thread_perf, false);
            thread_perf.close();
        }
    }

    static bool should_repair_fs();
    static void repair_fs();
    static void test_speed(jint count);
    
    static int get_object_size(Klass* k);

    static void parallel_merge_fs_data(HitmEventProf& thread_perf);
    static void merge_thread_data(HitmEventProf& thread_perf, bool reenable);

    static bool register_unsafe_field_offset(InstanceKlass* tgt_k, int tgt_static_slot, int src_static_slot);
    static bool register_unsafe_field_offset_taken(InstanceKlass* k, int src_slot);
        
};

#endif // __REMIX_FALSE_SHARING_FINDER_HPP__
