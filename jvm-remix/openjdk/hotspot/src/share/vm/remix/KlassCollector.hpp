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
#ifndef __REMIX_KLASS_COLLECTOR__HPP__
#define __REMIX_KLASS_COLLECTOR__HPP__

#include <map>
#include "PerfCollector.hpp"
#include "KlassHitData.hpp"
#include "Timing.hpp"

class KlassHitMap : public std::map<Klass*, KlassHitData*>
{
public:
    KlassHitMap() { }
    ~KlassHitMap()
    {
        clear_all();
    }

    void clear_all()
    {   
        for(iterator it = begin(); it != end(); ++ it)
            delete it->second;
        clear();
    }
};

class KlassCollector
{
public:
    KlassCollector()
    {
    }

    ~KlassCollector()
    {
    }

    inline size_t klass_count() const { return _hits.size(); }
    inline KlassHitMap::const_iterator hits_begin() const { return  _hits.begin(); }
    inline KlassHitMap::const_iterator hits_end() const { return  _hits.end(); }
    inline KlassHitMap& hits() { return _hits; }

    void print_out()
    {
        size_t count = _hits.size();
        if(count > 0)
        {
            tty->print("REMIX: Processed %li classes:\n", count); 
            for(KlassHitMap::iterator it = _hits.begin();
                it != _hits.end();
                ++ it)
            {
               it->second->print_out();
            }
        }
        else
        {
            tty->print("REMIX: Processed 0 classes.\n");
        }
    }

    void test_speed(jint count)
    {
        tty->print("test count=%i\n", count);
        PerfCollector perf;
        perf.create_speed_test_data(count);
        tty->print("Gathering objects.\n");
        gather(perf);
        tty->print("Done Gathering objects.\n");
    }

    void gather(PerfCollector& perf)
    {
        Clock clock(STAGE_KLASS_GATHER); 
        //MYTRACE("gather_fs_data(): starting.\n");
        assert(Thread::current()->is_VM_thread(), "must be running in a VM thread.");
        //MutexLockerEx ml_heap(Heap_lock);

        Universe::heap()->ensure_parsability(false);
        if (VerifyBeforeIteration) 
            Universe::verify();
      
        if(UseG1GC)
            gather_fs_data_g1(perf);
        else if(UseSerialGC || UseParNewGC)
            gather_fs_data_gen_gc(perf);
        else
        {
            tty->print("Heap kind: %i\n", Universe::heap()->kind());
            assert(false, "REMIX: Unsupported GC!");
        }
        //MYTRACE("gather_fs_data(): done.\n");
    }

private:
    void gather_fs_data_for_klass_hit(oop o, unsigned int offset, const HitData& hit_count)
    {
        Klass* klass = o->klass();
        bool is_array = o->is_array();
        if(is_array)
        {
            if(REMIXHitVerbose)
                tty->print("REMIX: HIT-ARRAY: oop=%p offset=0x%x hit_count=[%i,%i] klass=%p %s\n", o, offset, hit_count.shared, hit_count.single, klass, klass->external_name());
        }
        else
        {
            //printf("OBJECT: addr=%p oop=%p offset=0x%x klass=%s\n", ((char*)o) + offset, o, offset, klass->name()->as_C_string());
//            char* name = klass->name()->as_C_string();
//            if(strncmp(name, "java", 4) == 0)
//                return;
            if(strncmp(klass->name()->as_C_string(), "java/lang/", 10) == 0)
//            if(strncmp(klass->name()->as_C_string(), "java/", 5) == 0)
            {
                if(REMIXHitVerbose)
                    tty->print("REMIX: HIT-IGNORE: %p klass=%s:%d %i:%i\n", o, klass->name()->as_C_string(), offset, hit_count.shared, hit_count.single);
                return;
            }
            if(REMIXHitVerbose)
                tty->print("REMIX: HIT-OBJECT: %p klass=%s:%d %i:%i\n", o, klass->name()->as_C_string(), offset, hit_count.shared, hit_count.single);
//            char* name = klass->name()->as_C_string();
             
            gather_fs_data_for_instanceklass_hit(_hits, klass, offset, hit_count);
        }
    }

    void gather_fs_data_for_instanceklass_hit(KlassHitMap& kmp, Klass* klass, unsigned int offset, const HitData& hit_count)
    {
        if(hit_count.shared == 0)
            return;
        KlassHitMap::iterator kit = kmp.find(klass);
        if(kit == kmp.end())
        {
            KlassHitData* kdata = KlassHitData::create(klass);
            std::pair<KlassHitMap::iterator, bool> status = kmp.insert(KlassHitMap::value_type(klass, kdata));
            kit = status.first;
        }

        kit->second->update_hit(offset, hit_count);
    }

    void gather_fs_data_g1(PerfCollector& perf)
    { 
        CollectedHeap *heap = Universe::heap();

        for(HitMap::HitMapIterator it = perf.iterator();
            !it.done();
            it.next())
        {
            HitPair hp = it.value();
            char* ptr = (char*)hp.first;

            HeapWord* bs = heap->block_start(ptr);
            if(bs == NULL || !heap->block_is_obj(bs))
                continue;
            oopDesc* oop = (oopDesc*)bs;
            int offset = ((char*)ptr) - ((char*)bs);
            gather_fs_data_for_klass_hit((oopDesc*)bs, offset, hp.second);
        }

    }

    class ContiguousSpaceHitSearch : public BoolObjectClosure
    {
    public:
        ContiguousSpaceHitSearch(KlassCollector& imp, ContiguousSpace* space, HitVector& hit_vector) : _imp(imp), _space(space), _hit_vector(hit_vector) 
        { 
            const char* space_start = (const char*)space->bottom();
            const char* space_top = (const char*)space->top();

//            tty->print("Browsing heap: %p - %p, size=%lu\n", space_start, space_top, hit_vector.size());
            _hit_vector_pos = hit_vector.begin();
            _hit_vector_end = hit_vector.end();
//            if(_hit_vector_pos != _hit_vector_end)
//                tty->print(" first: %p\n", _hit_vector_pos->first);
    
            HitAddrCompare compare;
            _hit_vector_pos = std::__lower_bound(_hit_vector_pos, _hit_vector_end, space_start, compare);
            if(_hit_vector_pos != _hit_vector_end)
                _hit_vector_end = std::__lower_bound(_hit_vector_pos, _hit_vector_end, space_top, compare);
            
//            if(_hit_vector_pos != _hit_vector_end)    
//            {
//                tty->print(" real first: %p\n", _hit_vector_pos->first);
//                tty->print(" real end: %p\n",( _hit_vector_end-1)->first);
//            }
        }

        virtual bool do_object_b(oop o) 
        {
            if(_hit_vector_pos == _hit_vector_end)
                return false;
            char* start = (char*)o;
            char* end = start + o->size() * sizeof(HeapWord);
            //tty->print("Checking object %p\n", o);

            _hit_vector_pos = lower_bound(_hit_vector_pos, _hit_vector_end, start);
            if(_hit_vector_pos == _hit_vector_end)
            {
                //tty->print("No lower bound\n", o);
                return false;
            }
            //tty->print("Lower bound at %p, obj at %p - %p\n", _hit_vector_pos->first, start, end);
            for(;;)
            {
                char* addr = (char*)_hit_vector_pos->first;
                if(addr >= end)
                    break;
                _imp.gather_fs_data_for_klass_hit(o, addr - start, _hit_vector_pos->second);
                _hit_vector_pos ++;
                if(_hit_vector_pos == _hit_vector_end)
                    return false;
            }
            return true;
        }

    private:
        KlassCollector& _imp;
        ContiguousSpace* _space;
        HitVector& _hit_vector;
        HitVector::const_iterator _hit_vector_pos, _hit_vector_end;
    };
    friend class ContiguousSpaceHitSearch;
    
    class GenHeapHitSearch : public SpaceClosure
    {
    public:
        GenHeapHitSearch(KlassCollector& imp, HitVector& hit_vector) : _imp(imp), _hit_vector(hit_vector) { }

        virtual void do_space(Space* space)
        {
            ContiguousSpace* sp = space->toContiguousSpace();
            assert(sp != NULL, "space->toContiguousSpace()");
            ContiguousSpaceHitSearch srch(_imp, sp, _hit_vector);
            sp->object_iterate(&srch);
        }

    private:
        KlassCollector& _imp;
        HitVector& _hit_vector;
    };

    void gather_fs_data_gen_gc(PerfCollector& perf)
    { 
        GenCollectedHeap* gch = GenCollectedHeap::heap();
        HitVector hv;
        perf.to_hit_vector(hv);
        if(hv.size() == 0)
            return;

        GenHeapHitSearch ghhs(*this, hv);
        gch->space_iterate(&ghhs);
    }

public:
    void clear()
    {
        _hits.clear_all();
    }

private:
    KlassHitMap _hits;
};

#endif // __REMIX_KLASS_COLLECTOR__HPP__
