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
#include "runtime/mutexLocker.hpp"
#include "runtime/mutex.hpp"
#include "runtime/vmThread.hpp"
#include "runtime/vm_operations.hpp"
#include "runtime/safepoint.hpp"
#include "runtime/handles.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/biasedLocking.hpp"
#include "runtime/thread.hpp"
#include "runtime/thread.inline.hpp"
#include "runtime/deoptimization.hpp"
#include "utilities/exceptions.hpp"
#include "memory/iterator.hpp"
#include "memory/memRegion.hpp"
#include "memory/space.hpp"
#include "memory/genMarkSweep.hpp"
#include "memory/genCollectedHeap.hpp"
#include "memory/referenceProcessor.hpp"
#include "memory/genOopClosures.inline.hpp"
#include "oops/klass.hpp"
#include "oops/oop.inline.hpp"
#include "oops/oop.inline2.hpp"
#include "oops/fieldStreams.hpp"
#include "code/codeCache.hpp"
#include "prims/jvmtiExport.hpp"
#include "gc_implementation/shared/isGCActiveMark.hpp"
#include <map>
#include <vector>
#include <string>

#include "trace.hpp"
#include "HitMap.hpp"
#include "PerfCollector.hpp"
#include "KlassCollector.hpp"
#include "Policy.hpp"
#include "Rebuilder.hpp"
#include "RelayoutVMOperation.hpp"
#include "UnsafeUpdate.hpp"
#include "Timing.hpp"
#include "FalseSharingFinder.hpp"
#include "HitmEventProf.hpp"
#include "HitData.hpp"

int KlassRebuilder::_created_count = 0;
int KlassRebuilder::_move_count = 0;
int KlassRebuilder::_init_count = 0;
unsigned int threads_created = 0;

class FSImp
{
public:
    static FSImp& getInstance()
    {
        static FSImp* instanceptr = NULL;
        if(instanceptr == NULL)
        {
            MutexLockerEx ml(FalseSharingFinder_lock);
            if(instanceptr == NULL)
            {
                static FSImp instance;
                instanceptr = &instance;
                __sync_synchronize();
            }
        }
        return *instanceptr;
    }

    FSImp() : _clock(STAGE_TOTAL)
    {
        if(REMIXVerbose)
        {
            tty->print("REMIX: SamplingRatio is 1:%i.\n", REMIXSamplingRatio);
        }    
    }

    ~FSImp()
    {
    }

    void begin_vmop_safepoint()
    {
        MYTRACE("begin_vmop_safepoint(): start.\n");
        Clock clock(STAGE_SAFEPOINT_OP); 

        MutexLockerEx ml_fs(FalseSharingFinder_lock);
       
        {
            Clock clock(STAGE_PERF_COLLECT); 
            _perf_collector.update_heap_bounds();
            _perf_collector.merge_live_threads();
        }

        ResourceMark rm;
        if(_perf_collector.unprocessed_hits() >= UNPROCESSED_THRESHOLD)
        {
            if(REMIXHitVerbose)
                tty->print("REMIX: Processing %i new hits.\n", _perf_collector.unprocessed_hits());
            _klass_collector.gather(_perf_collector);
            _perf_collector.clear();

            if(should_do_relayout())
                do_relayout();

        }
        MYTRACE("begin_vmop_safepoint(): done.\n");
    }
   
    inline void end_vmop_safepoint()
    {
        MYTRACE("end_vmop_safepoint(): start.\n");
        _perf_collector.restore_thread_perf();
        MYTRACE("end_vmop_safepoint(): done.\n");
    }

    void init_vm()
    {
        if(REMIXTiming)
            Clock::print_now("REMIX@START_TIME");
    }

    void done_vm()
    {
        MYTRACE("done_vm(): start.\n");
        ResourceMark rm;
        MutexLockerEx ml_fs(FalseSharingFinder_lock);
        if(REMIXLevel == REMIX_DETECT)
        {
            _perf_collector.merge_live_threads();
            _klass_collector.gather(_perf_collector);
            display_results();
        }
        _clock.stop();
        Timing::getInstance().print_timings();
        if(REMIXTiming)
        {
            tty->print("REMIX: Statistics:\n");
            tty->print("       Total hits:    %i\n", _perf_collector.total_hits());
            tty->print("       Heap hits:     %i\n", _perf_collector.heap_hits());
            long clock = Timing::getInstance().get_clock(STAGE_TOTAL);
            tty->print("       Total time:    %li ns\n", clock);
            tty->print("       Total hits/ms: %.3f\n", ((double)_perf_collector.total_hits() * (double)1000000) / (double)clock) ;
            tty->print("       Heap hits/ms: %.3f\n", ((double)_perf_collector.heap_hits() * (double)1000000) / (double)clock) ;
        }

        KlassRebuilder::print_counts();
        MYTRACE("done_vm(): done.\n");
    }

    inline void relayout()
    {
        do_relayout();
    }

    inline void merge_thread_data(HitmEventProf& perf, bool reenable)
    {
        _perf_collector.merge_thread_data(perf, reenable);
    }

    inline void parallel_merge_thread_data(HitmEventProf& perf)
    {
        _perf_collector.parallel_merge_thread_data(perf);
    }

    void display_results()
    {
        _klass_collector.print_out();
    }

    bool should_do_relayout() const
    {
        if(REMIXLevel < REMIX_REPAIR_ONLINE)
            return false;

        for(KlassHitMap::const_iterator it = _klass_collector.hits_begin() ;
            it != _klass_collector.hits_end();
            ++ it)
        {
            if(it->second->should_relayout(it->first))
                return true;
        }

        return false;
    }

private:

    void do_relayout()
    {
        if(REMIXTiming)
            Clock::print_now("REMIX@RELAYOUT_TIME");
        if(REMIXVerbose)
            tty->print("REMIX: relayout phase starting.\n");
        if(SafepointSynchronize::is_at_safepoint())
        {
            RelayoutVMOperation op(_klass_collector.hits());
            op.doit();
        }
        else
        {
            MutexLockerEx ml_heap(Heap_lock);
            RelayoutVMOperation op(_klass_collector.hits());
            VMThread::execute(&op);
        }
        if(REMIXVerbose)
            tty->print("REMIX: relayout phase done.\n");
    }

public:
    void test_speed(jint count)
    {
        _klass_collector.test_speed(count);
    }

private: // FIELDS
    PerfCollector _perf_collector;
    KlassCollector _klass_collector;
    Clock _clock;
};

void FalseSharingFinder::merge_thread_data(HitmEventProf& thread_perf, bool reenable)
{
    if(REMIXLevel != REMIX_NONE)
        FSImp::getInstance().merge_thread_data(thread_perf, reenable);
}

void FalseSharingFinder::parallel_merge_fs_data(HitmEventProf& thread_perf)
{
    FSImp::getInstance().parallel_merge_thread_data(thread_perf);
}

void FalseSharingFinder::begin_vmop_safepoint()
{
    if(REMIXLevel != REMIX_NONE)
        FSImp::getInstance().begin_vmop_safepoint();
}

void FalseSharingFinder::end_vmop_safepoint()
{
    if(REMIXLevel != REMIX_NONE)
        FSImp::getInstance().end_vmop_safepoint();
}

void FalseSharingFinder::init_vm()
{
    if(REMIXLevel != REMIX_NONE)
        FSImp::getInstance().init_vm();
}

void FalseSharingFinder::done_vm()
{
    if(REMIXLevel != REMIX_NONE)
        FSImp::getInstance().done_vm();
}

void FalseSharingFinder::repair_fs()
{
    if(REMIXLevel == REMIX_REPAIR_ONLINE)
        FSImp::getInstance().relayout();

}

bool FalseSharingFinder::should_repair_fs()
{
    if(REMIXLevel == REMIX_REPAIR_ONLINE)
        return FSImp::getInstance().should_do_relayout();
    return false;
}

bool FalseSharingFinder::register_unsafe_field_offset(InstanceKlass* tgt_k, int tgt_static_slot, int src_slot)
{
    if(REMIXLevel == REMIX_REPAIR_ONLINE)
        return UnsafeUpdater::getInstance().register_unsafe_field_offset(tgt_k, tgt_static_slot, src_slot);

    return UnsafeUpdater::set_unsafe_field_offset(tgt_k, tgt_static_slot, src_slot);
}

bool FalseSharingFinder::register_unsafe_field_offset_taken(InstanceKlass* tgt_k, int src_slot)
{
    if(REMIXLevel == REMIX_REPAIR_ONLINE)
        return UnsafeUpdater::getInstance().register_unsafe_field_offset_taken(tgt_k, src_slot);
}


void FalseSharingFinder::test_speed(jint count)
{
    tty->print("REMIX: Testing speed, count=%i\n", count);
    FSImp::getInstance().test_speed(count);
}

