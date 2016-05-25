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
#ifndef __REMIX_RELAYOUT_VM_OPERATION__HPP__
#define __REMIX_RELAYOUT_VM_OPERATION__HPP__

#include "Timing.hpp"
#include "Rebuilder.hpp"

enum { GEN_LEVEL = 1 };

class SpaceFinderClosure: public SpaceClosure
{
public:
    SpaceFinderClosure(ContiguousSpace* current) : _current(current), _found(NULL)
    {
    }

    virtual void do_space(Space* space)
    {
        if(_found != NULL)
            return;
        ContiguousSpace* sp = space->toContiguousSpace();
        if(sp->bottom() == MarkSweep::getMarksAddr())
        {
            if(REMIXDebug)
                tty->print("Skipping zone due to holding marks (%x)\n", sp->bottom());
            return;
        }
        if(_current == NULL)
            _found = sp;
        else
        if(sp == _current)
            _current = NULL;
    }
   
    inline ContiguousSpace* get() const { return _found; }

private: 
    ContiguousSpace *_current, *_found;
};
class RelayoutObjects: public ObjectClosure
{
public:
    RelayoutObjects(ContiguousSpace* space, KlassRebuilderMap& rebuilders) :
        _space(space), _alloc_space(space), _rebuilders(rebuilders), _cur_marker(RebuilderMarkers::marker()), _ref_marker(RebuilderMarkers::ref_marker())
    { 
    }

    virtual void do_object(oop o) 
    {
        Klass* k = o->klass();
        KlassRebuilder* rebuilder = (KlassRebuilder*)k->rebuilder();
        if(rebuilder == NULL /* non eop */ || rebuilder == _cur_marker /* non interesting */)
        {
//            tty->print("Object at %p klass=%p mark=%lx NOREB\n", o, k, *(uint64_t*)o);
            o->init_mark();  // make sure object is not modified
            return;
        }
        if(rebuilder == _ref_marker)
        {
//            tty->print("Object at %p klass=%p mark=%lx REF\n", o, k, *(uint64_t*)o);
            o->init_mark();  // make sure object is not modified
//            handle_ref_obj(o);
            return;
        }
/*        oop ff = o->forwardee();
        if(ff != NULL && ff != o)
        {
            tty->print("Object at %p klass=%p mark=%lx FF\n", o, k, (*uint64_t*)o);
            return;
        }*/


        assert(!rebuilder->is_marker(), "Rebuilder cannot be older marker!"); // remove spurious check
             
        bool is_marked = o->is_gc_marked();
        if(!is_marked)
        {
//            tty->print("Ignoring unmarked object at %p\n", o);
            o->set_klass(rebuilder->backup_klass()); 
            o->init_mark();  // make sure object is not modified
            //fprintf(tmp, "Ignoring unmarked oop at %p\n", o);
            return;
        }
        oop hw = alloc_and_move(o, rebuilder);
        hw->init_mark();
    }

private:
    oop alloc_and_move(oop o, KlassRebuilder* rebuilder)
    {
        assert((unsigned int)o->size() == rebuilder->old_size_words(), "Old size discrepency");
        HeapWord* hw = _alloc_space->allocate(rebuilder->new_size_words());
        if(hw == NULL)
        {
            do
            {
                SpaceFinderClosure sfc(_alloc_space);
                GenCollectedHeap* gch = GenCollectedHeap::heap();
                gch->space_iterate(&sfc);
                if(sfc.get() == NULL)
                {
                    tty->print("REMIX: Failed to allocate space for object (heap1=%p heap2=%p, %i)\n", _space, _alloc_space, (int)rebuilder->new_size_words());
                    exit(-1);
                }
                _alloc_space = sfc.get();
                if(REMIXDebug)
                    tty->print("Trying from space (%p %p-%p-%p)\n", _alloc_space, _alloc_space->bottom(), _alloc_space->top(), _alloc_space->end());
                hw = _alloc_space->allocate(rebuilder->new_size_words());
            }
            while(hw == NULL);
        }

        rebuilder->move_object(o, hw);
        o->forward_to((oop)hw);
        o->set_klass(rebuilder->backup_klass()); 

        return (oop)hw;
    }

    template <class T>
    oop get_oop(oop obj)
    {
        T* referent_addr = (T*)java_lang_ref_Reference::referent_addr(obj);
        T heap_oop = oopDesc::load_heap_oop(referent_addr);
        if (oopDesc::is_null(heap_oop))
            return NULL;
        return oopDesc::decode_heap_oop_not_null(heap_oop);
    }
    void set_oop(oop obj, oop val)
    {
        oop* referent_addr = (oop*)java_lang_ref_Reference::referent_addr(obj);
        oopDesc::store_heap_oop(referent_addr, val);
    }
    void set_oop(oop obj, narrowOop val)
    {
        narrowOop* referent_addr = (narrowOop*)java_lang_ref_Reference::referent_addr(obj);
        //oopDesc::store_heap_oop(referent_addr, oopDesc::encode_heap_oop_not_null(val));
        oopDesc::store_heap_oop(referent_addr, val);
    }

    oop get_any_oop(oop obj)
    {
        if (UseCompressedOops)
            return get_oop<narrowOop>(obj);
        else
            return get_oop<oop>(obj);
    }
    oop set_any_oop(oop obj, oop val)
    {
        if (UseCompressedOops)
            set_oop(obj, val);
        else
            set_oop(obj, val);
     }

#if 0
    void handle_ref_obj(oop refo)
    {
        oop o = get_any_oop(refo);
        if(o == NULL || o->is_gc_marked())
            return;
        Klass* k = o->klass();
        KlassRebuilder* rebuilder = (KlassRebuilder*)k->rebuilder();
        if(rebuilder == NULL /* non eop */ || rebuilder == _cur_marker /* non interesting */)
            return;
        if(rebuilder == _ref_marker)
        {
            handle_ref_obj(o);
            return;
        }
        oop new_obj = alloc_and_move(o, rebuilder);
        // important: don't init mark here!
        //java_lang_ref_Reference::set_referent(refo, new_obj);
        set_any_oop(refo, new_obj);

        // XXX need smarter handle Phantom/Weak!
         
    }
#endif

private:
    ContiguousSpace *_space, *_alloc_space;
    KlassRebuilderMap& _rebuilders;
    KlassRebuilder * _cur_marker, *_ref_marker;
};

class RelayoutSpace: public SpaceClosure
{
public:
    RelayoutSpace(KlassRebuilderMap& rebuilders) : _rebuilders(rebuilders)
    {
    }

    virtual void do_space(Space* space)
    {
        ContiguousSpace* sp = space->toContiguousSpace();
        if(REMIXDebug)
            tty->print("***Relayout space: %p - %p - %p\n", sp->bottom(), sp->top(), sp->end());    
        assert(sp != NULL, "space->toContiguousSpace()");
        RelayoutObjects ro(sp, _rebuilders);
        sp->aefs_object_iterate(&ro);
        sp->aefs_prep_for_compact();
//        tty->print("***AFTER_SPACET %p - %p - %p\n", sp->bottom(), sp->top(), sp->end());    
    }
   
private: 
    KlassRebuilderMap& _rebuilders;
};
class TopSetterClosure: public SpaceClosure
{
public:
    TopSetterClosure() { }

    virtual void do_space(Space* space)
    {
        ContiguousSpace* sp = space->toContiguousSpace();
        assert(sp != NULL, "Should be ContiguousSpace");
        sp->set_top_for_aefs();
    }
private: 
};

class RelayoutVMOperation : public VM_Operation 
{
public:

    RelayoutVMOperation(KlassHitMap& hitmap)  : _clock(STAGE_RELAYOUT_OP)
    {
        RebuilderGenerator::generate(hitmap, _rebuilders);
    }

    VMOp_Type type() const { return VMOp_FSFixer; }
    virtual Mode evaluation_mode() const { return _safepoint; }

    void doit() 
    { 
        if(_rebuilders.empty())
            return;

        Universe::heap()->ensure_parsability(false);
        GenCollectedHeap* gch = GenCollectedHeap::heap();

        HandleMark hm;
        ReferenceProcessor* rp = gch->get_gen(GEN_LEVEL)->ref_processor();
        ReferenceProcessorSpanMutator x(rp, gch->reserved_region());
        rp->enable_discovery(true, true);
        ReferencePolicy* old_policy = rp->get_soft_ref_policy();
        rp->set_no_clear_ref_policies();
        MarkSweep::set_ref_processor(rp);

        prologue();
        mark_objects(rp);
        move_objects();
        adjust_pointers();
        epiloge();
        deoptimize();
//            gch->set_incremental_collection_failed(); // force next GC to be full! XXX
        rp->restore_soft_ref_policy(old_policy);

    }

    void prologue()
    {
        Clock clock(STAGE_RELAYOUT_PROLOGUE);
        if(REMIXDebug)
            tty->print("Relayout: prologue\n");
        GenCollectedHeap* gch = GenCollectedHeap::heap();
        BiasedLocking::preserve_marks();
        COMPILER2_PRESENT(DerivedPointerTable::clear());

        SpecializationStats::clear();    
        CodeCache::gc_prologue();
        Threads::gc_prologue();
        GenMarkSweep::allocate_stacks();

        // These two kill next GC!
        //gch->save_used_regions(GEN_LEVEL);
        //gch->save_marks();

        MarkSweep::follow_root_closure.set_orig_generation(gch->get_gen(GEN_LEVEL));
        ClassLoaderDataGraph::clear_claimed_marks();
    }

    void mark_objects(ReferenceProcessor* rp)
    {    
        Clock clock(STAGE_RELAYOUT_MARK_OBJECTS);
        IsGCActiveMark active; // lose at end of func

        if(REMIXDebug)
            tty->print("Relayout: markobjects\n");
        GenCollectedHeap* gch = GenCollectedHeap::heap();
        // mark objects
        gch->gen_process_strong_roots(GEN_LEVEL,
                              false, // Younger gens are not roots.
                              true,  // activate StrongRootsScope
                              false, // not scavenging
                              SharedHeap::SO_SystemClasses,
                              &MarkSweep::follow_root_closure,
                              true,   // walk code active on stacks
                              &MarkSweep::follow_root_closure,
                              &MarkSweep::follow_klass_closure);

        rp->process_discovered_references(&MarkSweep::is_alive, &MarkSweep::keep_alive, &MarkSweep::follow_stack_closure, NULL, NULL);

        MarkSweep::assert_marking_stack_empty();
    }
   
    void move_objects()
    { 
        Clock clock(STAGE_RELAYOUT_MOVE_OBJECTS);
        if(REMIXDebug)
            tty->print("Relayout: move objects\n");
        GenCollectedHeap* gch = GenCollectedHeap::heap();
        TopSetterClosure tsc;
        RelayoutSpace rs(_rebuilders);
        gch->space_iterate(&tsc);
        gch->space_iterate(&rs);
         
        _rebuilders.post_relayout();
    }   

    void adjust_pointers()
    {
        Clock clock(STAGE_RELAYOUT_ADJUST_POINTERS);
        if(REMIXDebug)
            tty->print("Relayout: adjust objects\n");
        IsGCActiveMark active; // lose at end of func
        GenMarkSweep::mark_sweep_phase3(GEN_LEVEL);
    }

    void epiloge()
    {
        Clock clock(STAGE_RELAYOUT_EPILOGUE);
        if(REMIXDebug)
            tty->print("Relayout: epilogue\n");
        GenCollectedHeap* gch = GenCollectedHeap::heap();
        MarkSweep::restore_marks();
        GenMarkSweep::deallocate_stacks();
        Threads::gc_epilogue();
        CodeCache::gc_epilogue();
        JvmtiExport::gc_epilogue();

        COMPILER2_PRESENT(DerivedPointerTable::update_pointers());
        BiasedLocking::restore_marks();

        Universe::update_heap_info_at_gc();
        MemoryService::track_memory_usage();
        ClassLoaderDataGraph::purge();
        MetaspaceAux::verify_metrics();
        MarkSweep::set_ref_processor(NULL);
        gch->resize_tlabs();
    }

    void deoptimize()
    {
        Clock clock(STAGE_RELAYOUT_DEOPTIMIZE);
        if(REMIXDebug)
            tty->print("Deoptimizing.\n");
        CodeCache::mark_all_nmethods_for_deoptimization();
        ResourceMark rm;
        DeoptimizationMarker dm;
        Deoptimization::deoptimize_dependents();
        CodeCache::make_marked_nmethods_not_entrant();
        JvmtiExport::set_all_dependencies_are_recorded(true);
    }

/*
    void print_frames()
    {
        for (JavaThread* thread = Threads::first(); thread != NULL; thread = thread->next())
        {
            tty->print("THREAD %p\n", thread);
            if(thread->has_last_Java_frame())
                thread->trace_frames();
        }
    }
*/

private:    
    KlassRebuilderMap _rebuilders;
    Clock _clock;
};

#endif // __REMIX_RELAYOUT_VM_OPERATION__HPP__

