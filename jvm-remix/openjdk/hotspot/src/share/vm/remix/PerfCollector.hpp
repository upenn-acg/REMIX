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
#ifndef __REMIX_PERF_COLLECTOR_HPP__
#define __REMIX_PERF_COLLECTOR_HPP__

#include "remix/HitmEventProf.hpp"
#include "remix/HitMap.hpp"
#include "remix/CacheLine.hpp"
#include "remix/Timing.hpp"
#include "remix/Policy.hpp"

class PerfCollector
{
public:
    PerfCollector()
    {
        _unprocessed_hits = 0;
        _total_hits = _heap_hits = _metaspace_hits = 0;
    }

    unsigned int total_hits() const { return _total_hits; }
    unsigned int heap_hits() const { return _heap_hits; }
    unsigned int metaspace_hits() const { return _metaspace_hits; }

    inline unsigned int unprocessed_hits() const { return _unprocessed_hits; }
    inline unsigned int unprocessed_meta_hits() const { return _metaspace_hits; }
    void clear()
    {
        _unprocessed_hits = 0;
        _hits.clear();
    }
    void metaspace_clear()
    {
        _metaspace_hits = 0;
        _meta_hits.clear();
    }

    void create_speed_test_data(int count)
    {
        update_heap_bounds();
        _metaspace_hits += count;
        _unprocessed_hits += count;
        _total_hits += count;
        _heap_hits += count;
        unsigned int rpos = 0x348930A;
        size_t heap_size = ((char*)_heap_end - (char*)_heap_start) / sizeof(char*) - sizeof(char*);
        tty->print("Creating %i objects\n", count);
        for(int i = 0; i < count; ++ i)
        {
            void* loc = ((char*)_heap_start) + sizeof(char*) * (rpos % heap_size);
            _hits.add(loc, 1); 
            rpos = (rpos * 0x4848FA41) + 0x1837AB11;
        }
    }
 
    void update_heap_bounds()
    {
        MemRegion rgn = Universe::heap()->reserved_region();
        _heap_start = rgn.start();
        _heap_end = rgn.end();
        //printf("Heap boundary %p - %p.\n", _heap_start, _heap_end);
    }

    void merge_live_threads()
    {
        Clock clock(STAGE_MERGE_LIVE_THREADS);
        //MutexLockerEx ml_thread(Threads_lock);
        //MYTRACE("merge_live_threads(): starting.\n");
        for (JavaThread* jt = Threads::first(); jt != NULL; jt = jt->next()) 
        {
            jt->getPerf().store_disable();
            //printf("Merging thread %p\n", jt);
            if (jt->threadObj() == NULL || jt->is_hidden_from_external_view() || jt->is_jvmti_agent_thread())  
                continue;
            merge_thread_data_locked(jt->getPerf(), true); 
        }
        //MYTRACE("merge_live_threads(): done.\n");
    }

    void stop()
    {
        for (JavaThread* jt = Threads::first(); jt != NULL; jt = jt->next()) 
        {
//            jt->getPerf().store_disable();
//            jt->getPerf().disable();
              jt->getPerf().disable();
              jt->getPerf().close();
        }
    }
    void merge_thread_data(HitmEventProf& perf, bool reenable)
    {
        MutexLockerEx ml(FalseSharingFinder_lock);
        perf.store_disable();
        merge_thread_data_locked(perf, reenable);
        perf.restore_disable();
    }

    void parallel_merge_thread_data(HitmEventProf& perf)
    {
        HitMap hits, meta_hits;
        unsigned int unprocessed_hits = 0, metaspace_hits = 0, total_hits = 0, heap_hits = 0;
        merge_thread_data_imp(perf, hits, meta_hits, STAGE_PERF_COLLECT_PARALLEL, 
            &unprocessed_hits, &metaspace_hits, &total_hits, &heap_hits);
        
        Clock clock2(STAGE_PERF_COLLECT_PARALLEL_BLOCKED);
        MutexLockerEx ml(FalseSharingFinder_lock);
        _hits.merge(hits);
        if(REMIXMetaspace)
        {
            _meta_hits.merge(meta_hits);
            _metaspace_hits += metaspace_hits;
        }
        _unprocessed_hits += unprocessed_hits;
        _total_hits += total_hits;
        _heap_hits += heap_hits;

        clock2.stop();
    }

    void restore_thread_perf()
    {
        Clock clock(STAGE_RESTORE_LIVE_THREADS);
        //MutexLockerEx ml_thread(Threads_lock);
        //MYTRACE("restore_thread_perf(): starting.\n");
        for (JavaThread* jt = Threads::first(); jt != NULL; jt = jt->next()) 
        {
            jt->getPerf().restore_disable();
        }
        //MYTRACE("restore_thread_perf(): done.\n");
    }

    struct HitVectorSorter
    {
        inline bool operator()(const HitPair& first, const HitPair& second)
        {
            return first.first < second.first;
        }
    };
    void to_hit_vector(HitVector& vector)
    {
        to_hit_vector(_hits, vector);
    }

    inline HitMap::HitMapIterator iterator()
    {
        return _hits.iterator();
    }

    // XXX wrongplace
    void to_metaspace_hitvector(HitVector& vector)
    {
        to_hit_vector(_meta_hits, vector);
    }

private:
    void to_hit_vector(HitMap& map, HitVector& vector)
    {
        for(HitMap::HitMapIterator it = map.iterator();
            !it.done();
            it.next())
        {
            HitPair data = it.value();
            if (data.second.shared > FIELD_HIT_THRESHOLD)
//            if (data.second.shared > FIELD_HIT_THRESHOLD || data.second.single > FIELD_HIT_THRESHOLD)
            {
                vector.push_back(data);
                //tty->print("Adding hit to %p %i:%i\n", it.value().first, it.value().second.shared, it.value().second.single);
            }
        }
        sort(vector.begin(), vector.end(), HitVectorSorter()); // XXX iterating map should yield sorted results!
    }

    void merge_thread_data_locked(HitmEventProf& perf, bool reenable)   
    {
        unsigned int unprocessed_hits = 0, metaspace_hits = 0, total_hits = 0, heap_hits = 0;
        merge_thread_data_imp(perf, _hits, _meta_hits, STAGE_PERF_COLLECT, 
            &unprocessed_hits, &metaspace_hits, &total_hits, &heap_hits);
        _metaspace_hits += metaspace_hits;
        _unprocessed_hits += unprocessed_hits;
        _total_hits += total_hits;
        _heap_hits += heap_hits;
    }
    void print_out_of_heap_hit(void * addr)
    {
        if(Metaspace::contains(addr))
        {
            tty->print("REMIX: HIT-METASPACE: %p  [%p - %p]\n", addr, _heap_start, _heap_end);
            return;
        }
        tty->print("REMIX: HIT-EXTERNAL: %p  [%p - %p]\n", addr, _heap_start, _heap_end);
    }

    void merge_thread_data_imp(HitmEventProf& perf, HitMap& hits, HitMap& meta_hits, 
                        LifeStage clock_id,
                        unsigned int* unproc_hit_count, 
                        unsigned int* metaspace_hit_count,
                        unsigned int* total_hit_count, 
                        unsigned int* heap_hit_count)
    {
        Clock clock(clock_id);
        if(!perf.start_iterate())
            return;

        int unprocessed_hits = 0, metaspace_hits = 0, total_hits = 0, heap_hits = 0;
        if(REMIXTiming)
        {
            DataRecord dr;
            while(perf.next_timed(&dr))
            {
                ++ total_hits;
                if(REMIXHitVerbose)
                    tty->print("REMIX: HIT-RAW: %lu [line 0x%lx] %p\n", dr.time, ((uintptr_t)dr.data) & ~((uintptr_t)(CACHE_LINE_SIZE - 1)), dr.data);
                if(dr.data < _heap_start || dr.data >= _heap_end)
                {
                    if(REMIXHitVerbose)
                        print_out_of_heap_hit(dr.data);
                    if(REMIXMetaspace)
                    {
                        meta_hits.add(dr.data, 1); 
                        ++ metaspace_hits;
                    }
                    continue;
                }
                ++ heap_hits;
                hits.add(dr.data, 1); 
                unprocessed_hits += 1;
                //dr.ip = dr.data = NULL;
                dr.data = NULL;
            }
        }
        else
        {
            DataRecord dr;
            while(perf.next(&dr))
            {
                ++ total_hits;
                if(REMIXHitVerbose)
                    tty->print("REMIX: HIT-RAW: %lu [line 0x%lx] %p\n", dr.time, ((uintptr_t)dr.data) & ~((uintptr_t)(CACHE_LINE_SIZE - 1)), dr.data);
                if(dr.data < _heap_start || dr.data >= _heap_end)
                {
                    if(REMIXHitVerbose)
                        print_out_of_heap_hit(dr.data);
                    if(REMIXMetaspace)
                    {
                        meta_hits.add(dr.data, 1); 
                        ++ metaspace_hits;
                    }
                    continue;
                }
                ++ heap_hits;
                hits.add(dr.data, 1); 
                unprocessed_hits += 1;
                //dr.ip = dr.data = NULL;
                dr.data = NULL;
            }
        }
        *unproc_hit_count = unprocessed_hits;
        *metaspace_hit_count = metaspace_hits;
        *total_hit_count = total_hits;
        *heap_hit_count = heap_hits;
    }
private:
    void *_heap_start, *_heap_end;
    unsigned int _unprocessed_hits;
    unsigned int _total_hits, _heap_hits, _metaspace_hits;
    HitMap _hits;
    HitMap _meta_hits;
};

#endif // __REMIX_PERF_COLLECTOR_HPP__

