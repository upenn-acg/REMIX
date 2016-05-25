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
#ifndef __REMIX_REBUILDER_HPP__
#define __REMIX_REBUILDER_HPP__

#include "Timing.hpp"
#include "Policy.hpp"
#include "CacheLine.hpp"
#include "UnsafeUpdate.hpp"
#include "HitData.hpp"
#include "KlassHitData.hpp"
#include "KlassCollector.hpp"
#include "oops/fieldStreams.hpp"
#include <set>
#include <vector>
#include <algorithm>

class KlassRebuilder
{
public:
    KlassRebuilder() : _klass(NULL), _parent(NULL), _flattened(true), _old_size(0), _new_size(0), _header_hit(false), _direct_hit(false), _fields_diff_size(0), _unsafe_fixed(false)
    {
    }

    ~KlassRebuilder()
    {
        if(_klass != NULL)
            revert_klass();
    }

    static bool is_terminal(Klass* klass)
    {
        Klass* super = klass->super();
        return super == NULL || super->super() == NULL;
    }

    void init(InstanceKlass* klass, KlassRebuilder* parent, KlassHitData* hits, bool direct_hit = false)
    {
        _klass = klass;
        _parent = parent;
        _flattened = false;
        _old_size = klass->layout_helper() & ~(sizeof(HeapWord) - 1);
        assert(_old_size > 0, "Size error");
        _new_size = _old_size;
        _direct_hit = direct_hit;

        if(UnsafeUpdater::getInstance().is_fixed(klass))
        {
            assert(!parent->unsafe_fixed(), "Can't deal yet with fixed parent!");
    
            _unsafe_fixed = true;
            if(REMIXVerbose)
                tty->print("REMIX: Klass in unsafe, ignoring (this=%p klass=%s (old=%i %p, parent=%p)\n", this, _klass->signature_name(), _old_size, _klass, _parent);
            return;
        }
        _unsafe_fixed = false;

        EXCEPTION_MARK;
        _backup_klass = klass->create_same_size_placeholder(THREAD);
        assert(_backup_klass != NULL, "Object does not have cloneable klass!");

        if(REMIXVerbose)
            tty->print("REMIX: Considering padding for this=%p klass=%s (direct=%i, old=%i %p, backup=%p, parent=%p)\n", this, _klass->signature_name(), direct_hit?1:0, _old_size, _klass, _backup_klass, _parent);

        ++ _created_count;

        if(hits != NULL)
            merge_hits(hits);
	}

    inline bool is_marker() const { return _klass == NULL; }
    inline bool unsafe_fixed() const { return _unsafe_fixed; }

    inline unsigned int old_size_words() const { return _old_size / sizeof(HeapWord); }
    inline unsigned int old_size_bytes() const { return _old_size; }
    inline unsigned int new_size_words() const { return _new_size / sizeof(HeapWord); }
    inline unsigned int new_size_bytes() const { return _new_size; }
    
    void move_object(oop o, void* new_loc)
    {
//        tty->print("Moving OOP: %p->%p (%s)\n", o, new_loc, _klass->signature_name());
        if(REMIXDebug)
            tty->print("REMIX: Moving OOP: %p->%p (%s)\n", o, new_loc, _klass->signature_name());
        char* src = (char*)o;
        char* dst = (char*)new_loc;
        char* src_end = src + old_size_bytes();
        char* dst_end = dst + new_size_bytes();
        unsigned int offset = 0, tdiff = 0;
        for(PosDiffArray::iterator current = _pos_diffs.begin(), end = _pos_diffs.end();
            current != end;
            ++ current)
        {
            unsigned int size = current->offset - offset; 
            unsigned int diff = current->tdiff - tdiff;
            memcpy(dst, src, size);
            src += size;
            dst += size;
            offset = current->offset;
            tdiff = current->tdiff;
            memset(dst, 0, diff);
            dst += diff;
        }
        if(src < src_end)
        {
            unsigned int size = src_end - src;
            memcpy(dst, src, size);
            dst += size;
        }
        if(dst < dst_end)
        {
            unsigned size = dst_end - dst;
            memset(dst, 0, size);
        }
        ++ _move_count;
    }
   
    Klass* klass() const { return _klass; }
    Klass* backup_klass() const { return _backup_klass; }
     
    void post_relayout()
    {
        _klass->set_layout_helper(new_size_bytes() | 1); // XXX slow path!
        _klass->set_nonstatic_field_size(_klass->nonstatic_field_size() + (_fields_diff_size / sizeof(HeapWord)));
        
        PosDiffArray::iterator begin = _pos_diffs.begin(), end = _pos_diffs.end();
        if(begin == end)
            return;
        modify_java_fields(begin, end);
        update_unsafe_offsets(begin, end);
        fix_oop_maps(begin, end);

        //_klass->print_on(tty);
    }

    bool flatten()
    {
        if(_flattened)
            return !unused();
        _flattened = true;

        unsigned int diff = 0;
        bool force = _direct_hit;
        if(_parent != NULL)
        {
            if(_parent->flatten())
            {
                diff = _parent->_new_size - _parent->_old_size;
                _pos_diffs.insert(_pos_diffs.begin(), _parent->_pos_diffs.begin(), _parent->_pos_diffs.end());
                _pos_diffs.push_back(PosDiff(_parent->_old_size, diff));
            }
            else
                _parent = NULL;
        }

        if(REMIXDebug)
        {
            if(_header_hit)
                tty->print("REMIX: Header hit for %p %p\n", this, _klass);
            for(PosHitArray::iterator it = _hit_offsets.begin();
                it != _hit_offsets.end();
                ++ it)
                tty->print("REMIX: Hits for %p %p at %i\n", this, _klass, *it);
        }
        
        if(_header_hit)
        {
            if(_hit_offsets.size() > 0)
            {
                int first_hit = _hit_offsets[0];
                if(first_hit + diff < KlassHitData::HEADER_SIZE + CACHE_LINE_SIZE)
                {
                    diff = CACHE_LINE_SIZE;
                    _pos_diffs.push_back(PosDiff(first_hit, diff));
                }
            }
            else if(_direct_hit)
            {
                if(_old_size >= (CACHE_LINE_SIZE * 2))
                {
                    if(REMIXDebug)
                        tty->print("REMIX: True sharing on klass=%s (%p, %p)\n", _klass->signature_name(), _klass, this);
                    // XXX discard backup
                    return false;
                }
                //_new_size = (_old_size + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1);
            }
        }

        if(_hit_offsets.size() > 0)
        {
            force = true;
            PosHitArray::iterator it = _hit_offsets.begin();
            for(;;)
            {
                unsigned int offset1 = *it;
                ++ it;
                if(it == _hit_offsets.end())
                {
                    diff += CACHE_LINE_SIZE - sizeof(HeapWord);
                    break; 
                }
                unsigned int offset2 = *it;
                if((offset1 + CACHE_LINE_SIZE) > offset2)
                {
                    diff += CACHE_LINE_SIZE - sizeof(HeapWord);
                    _pos_diffs.push_back(PosDiff(offset2, diff));
                }
            }
        }

//        if(REMIXVerbose)
//            tty->print("Diff = %i force=%i _hit_offsets=%i _pos_diffs=%i blank_count=%i\n", (int)diff, (int)force, _hit_offsets.size(), _pos_diffs.size(), _klass->get_fs_blank_count());
        if(diff == 0 && !force)
        {
            _klass->inc_fs_blank_count();
            return false;
        }

        _new_size = (_old_size + diff + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1);
        if(_new_size == _old_size)
        {
            _klass->inc_fs_blank_count();
            return false;
        }

        _fields_diff_size += diff;

        if(REMIXDebug)
        {
            tty->print("REMIX Initialized rebuilder for klass=%s (size: %i -> %i (%i) %p, backup=%p)\n", _klass->signature_name(), _old_size, _new_size, _new_size - _old_size, _klass, _backup_klass);
            for (PosDiffArray::iterator it = _pos_diffs.begin();
                 it != _pos_diffs.end();
                ++ it)
                    tty->print("  POS DIFF: %i => %i (%i)\n", it->offset, it->offset + it->tdiff, it->tdiff);
        }


        ++ _init_count;
        return true;
    }

    
    FieldArray::iterator merge_hits(KlassHitData* hit)
    {
        return merge_hits(hit, hit->should_relayout_header());
    }

    bool unused() const
    {
        return _old_size == _new_size;
    }

    void revert_klass()
    {
        _klass->clear_rebuilder();
    }
    
    void fix_field_offset(int* offset)
    {
        PosDiffArray::iterator begin = _pos_diffs.begin(), end = _pos_diffs.end();
        if(begin == end)
            return;

        PosDiffArray::iterator pos = std::upper_bound(begin, end, *offset, pos_diff_compare);
        if(pos == begin)
            return;
        pos -= 1;
 
        *offset += pos->tdiff;
    }

    static void print_counts()
    {
        tty->print("REMIX: Transformed %i klasses, moved %i instances.\n", _created_count, _move_count);
    }

private:
    struct PosDiff
    {
        inline PosDiff() : offset(0), tdiff(0) {}
        inline PosDiff(unsigned int offset_, unsigned int tdiff_) : offset(offset_), tdiff(tdiff_) { }
        inline PosDiff(const PosDiff& other) : offset(other.offset), tdiff(other.tdiff) { }
        inline PosDiff& operator=(const PosDiff& other) { offset = other.offset; tdiff = other.tdiff; }
        inline bool operator<(const PosDiff& rhs) const { return offset < rhs.offset; }
        inline bool operator<(const unsigned int rhs_offset) const { return offset < rhs_offset; }

        unsigned int offset;
        unsigned int tdiff;
    };
    struct PosDiffCompare : public std::binary_function<PosDiff&, unsigned int, bool>
    { 
        inline bool operator()(const PosDiff& lhs, unsigned int rhs) const { return lhs < rhs; } 
        inline bool operator()(unsigned int lhs, const PosDiff& rhs) const { return lhs < rhs.offset; } 
    };
    typedef std::vector<PosDiff> PosDiffArray;

    FieldArray::iterator merge_hits(KlassHitData* hit, bool header_hit)
    {
        FieldArray::iterator current = hit->begin();
        if(_parent != NULL)
            current = _parent->merge_hits(hit, header_hit);

        _header_hit = _header_hit || header_hit;
        FieldArray::iterator end = hit->end();

        for(; current != end; ++ current)
        {
            FieldHitData* fd = *current;
            unsigned int offset = fd->offset();
            if(offset >= _old_size)
                break;
            if(fd->should_relayout())
            {
                PosHitArray::iterator it = std::lower_bound(_hit_offsets.begin(), _hit_offsets.end(), offset);
                if(it == _hit_offsets.end() || *it != offset)
                    _hit_offsets.insert(it, offset);
            }
        }

        return current;
    }

    void modify_java_fields(PosDiffArray::iterator begin, PosDiffArray::iterator end)
    {
        for(AllFieldStream afs(_klass); !afs.done(); afs.next())
        { 
            if (!afs.access_flags().is_static())
            {
                unsigned int offset = afs.offset();
               
                PosDiffArray::iterator pos = std::upper_bound(begin, end, offset, pos_diff_compare);
                if(pos == begin)
                    continue;
                pos -= 1;
            
                afs.set_offset(offset + pos->tdiff);    
            }
        }
    }
    
    void update_unsafe_offsets(PosDiffArray::iterator begin, PosDiffArray::iterator end)
    {
        UnsafeUpdater::MapType::iterator upd_current = UnsafeUpdater::getInstance().begin((InstanceKlass*)_klass);
        UnsafeUpdater::MapType::iterator upd_end = UnsafeUpdater::getInstance().end((InstanceKlass*)_klass);
        for(; upd_current != upd_end; ++ upd_current)
        {
            PosDiffArray::iterator pos = std::upper_bound(begin, end, upd_current->second.src_offset, pos_diff_compare);
            if(pos == begin)
                continue;
            pos -= 1;

            UnsafeUpdater::update_entry(_klass, upd_current->second, pos->tdiff);
        }
        
    }

    void fix_oop_maps(PosDiffArray::iterator begin, PosDiffArray::iterator end)
    {
        int count = _klass->nonstatic_oop_map_count();
        if(count > 0)
        {
            OopMapBlock* block = _klass->start_of_nonstatic_oop_maps();
            for(int i = 0; i < count; ++ i, ++block)
            {
                int offset = block->offset();
                PosDiffArray::iterator pos = std::upper_bound(begin, end, offset, pos_diff_compare);
                if(pos == begin)
                    continue;
                pos -= 1;
                block->set_offset(offset + pos->tdiff);
           }
        } 
    }

private:
    static int _created_count, _init_count, _move_count;
    bool _unsafe_fixed;
    InstanceKlass* _klass;
    Klass* _backup_klass;
    KlassRebuilder *_parent;
    bool _flattened, _header_hit, _direct_hit;
    unsigned int _old_size, _new_size, _fields_diff_size;

    typedef std::vector<unsigned int> PosHitArray;
    PosHitArray _hit_offsets;

    PosDiffCompare pos_diff_compare;
    PosDiffArray _pos_diffs;
};

class RebuilderMarkers
{
public:
    static inline RebuilderMarkers& Instance() 
    {
        static RebuilderMarkers instance;
        return instance;
    }

    static KlassRebuilder* ref_marker()
    {
        return &Instance()._ref_marker;
    }

    static KlassRebuilder* marker()
    {
        return Instance()._cur_marker;
    }
    static KlassRebuilder* prev_marker()
    {
        return Instance()._prev_marker;
    }
    
    static void swap()
    {
        Instance().do_swap();
    }

private:
    RebuilderMarkers()
    {
        _cur_marker_idx = 0;
        _cur_marker = &_markers[0];
        _prev_marker = &_markers[1];
    }
    void do_swap()
    {
        _cur_marker_idx = (_cur_marker_idx + 1) & 1;
        _prev_marker = _cur_marker;
        _cur_marker = &_markers[_cur_marker_idx];
    }

    KlassRebuilder _markers[2];
    KlassRebuilder _ref_marker;
    KlassRebuilder *_cur_marker, *_prev_marker;
    int _cur_marker_idx;
};

class KlassRebuilderMap
{
public:
    KlassRebuilderMap()
    {
    }

    ~KlassRebuilderMap()
    {
        for(MapType::iterator it = _map.begin(); it != _map.end(); ++ it)
        {
            KlassRebuilder* rebuilder = it->second;
            delete rebuilder;
        }
        _map.clear();
    }

    inline bool empty() const { return _map.size() == 0; }

    inline void add(Klass* klass, KlassRebuilder* rebuilder)
    {
        std::pair<MapType::iterator, bool> result =_map.insert(MapType::value_type(klass, rebuilder));
        assert(result.second, "Duplicate klass in rebuilder.");
    }

    KlassRebuilder* find(Klass* k)
    {
        MapType::iterator it = _map.find(k);
        if(it == _map.end())
            return NULL;
        return it->second;
    }

    void flatten()
    {
        for(MapType::iterator it = _map.begin(); it != _map.end(); ++ it)
            it->second->flatten();

        for(MapType::iterator it = _map.begin(); it != _map.end(); )
        {
            MapType::iterator cur = it;
            ++ it;
            KlassRebuilder* rebuilder = cur->second;
            if(rebuilder->unused())
            {
                if(REMIXDebug)
                    tty->print("Prunded rebuilder %p for %p\n", rebuilder, cur->first);
                delete rebuilder;
                _map.erase(cur);
            }
        }
    }

    void post_relayout()
    {
        for(MapType::iterator it = _map.begin(); it != _map.end(); ++ it)
            it->second->post_relayout();
    }

private:
    typedef std::map<Klass*, KlassRebuilder*> MapType;
    MapType _map;
};

class SubklassSearchClosure : public KlassClosure 
{
 public:
    SubklassSearchClosure(KlassRebuilderMap& rebuilders) : 
        _rebuilders(rebuilders), 
        _cur_marker(RebuilderMarkers::marker()), 
        _prev_marker(RebuilderMarkers::prev_marker()),
        _ref_marker(RebuilderMarkers::ref_marker()), _count_searched(0)
      //  ,_timer(STAGE_FIND_KLASSES, true)
    {
        f2_offset = in_bytes(ConstantPoolCacheEntry::f2_offset());
    }

    virtual void do_klass(Klass* k)
    {
        if(k->oop_is_instance())
            process_klass((InstanceKlass*)k);
    }

    ~SubklassSearchClosure()
    {
        if(REMIXVerbose) tty->print("REMIX: processed %i klasses.\n", _count_searched);
        // XXX
        //CollectedHeap* ch = (CollectedHeap*)Universe::heap();
        //long total = _timer.stop();
        //tty->print("@REMIX@ %i %li %li %li\n", _count_searched, total, ch->capacity(), ch->used());

    }

    //Clock _timer; // XXX

private:
    KlassRebuilder* process_klass(InstanceKlass* k)
    {
        ++ _count_searched; //XXX
        KlassRebuilder *kbuilder = (KlassRebuilder*)k->rebuilder();
        if(kbuilder == NULL || kbuilder == _prev_marker)
        {
            kbuilder = _cur_marker;
            if(!KlassRebuilder::is_terminal(k))
            {
                KlassRebuilder* parent = process_klass(k->java_super());
                if(parent != _cur_marker)    
                {
                    kbuilder = new KlassRebuilder();
                    assert(kbuilder != NULL, "ALLOCATION ERROR!");
                    kbuilder->init(k, parent, NULL, false);
                    kbuilder->flatten(); // the heirarchy has already been established!
                    _rebuilders.add(k, kbuilder);
                }
            }
        
            k->set_rebuilder(kbuilder); // valid markers are cleared by KlassRebuilderMap
        } 
        if(kbuilder == _ref_marker)
            return NULL; 

        std::pair<InstanceKlassSet::iterator, bool> status = _fixed_caches.insert(k);
        if(status.second)
            fix_constant_pool(k);

        return kbuilder;
    }

    void fix_constant_pool(InstanceKlass* k)
    {
        ConstantPool* cp = k->constants();
        if(cp == NULL)
            return;
        ConstantPoolCache* cpcache = cp->cache();
        if(cpcache == NULL)
            return;
        
        Klass* last = NULL;
        int length = cpcache->length();
        for(int i = 0; i < length; ++ i)
        {
            ConstantPoolCacheEntry* entry = cpcache->entry_at(i);
            if(entry->is_field_entry() && entry->bytecode_1() == Bytecodes::_getfield )
            {
                Klass* ek = entry->f1_as_klass();
                if(ek != last && k != ek)
                {
                    if(!ek->oop_is_instance())
                        continue;
                    last = ek;
                    process_klass((InstanceKlass*)ek);
                }
                KlassRebuilder* rb = (KlassRebuilder*)ek->rebuilder();
                if(rb == _cur_marker)
                    continue;
                   
                int* offset = (int*)(((char*)entry) + f2_offset);
//                printf("Rebuilder %p fixing offset %p %i %i %i\n", k, offset, *offset, entry->bytecode_1(), entry->bytecode_2());
                rb->fix_field_offset(offset);
            }
        }
    }

private:
    KlassRebuilderMap& _rebuilders;
    KlassRebuilder *_cur_marker, *_prev_marker, *_ref_marker;
    typedef std::set<InstanceKlass*> InstanceKlassSet;
    InstanceKlassSet _fixed_caches; 
    int _count_searched;
    int f2_offset;
};

class RebuilderGenerator
{
public:
    static void generate(KlassHitMap& hits, KlassRebuilderMap& rebuilders) 
    {
        Clock clock(STAGE_REBUILDER_GENERATOR);
        RebuilderMarkers::swap();
        RebuilderGenerator generator(hits, rebuilders);
       
        generator.drill_down();
        generator.flatten();
        generator.find_subclasses_and_cp();
    }

private:
    void drill_down()
    {
        Clock clock(STAGE_REBUILDER_GENERATOR_DRILL_DOWN);
        for(KlassHitMap::iterator it = _hits.begin(); 
            it != _hits.end();
            ++ it)
        {
            if(it->second->should_relayout(it->first) && !it->first->oop_is_instanceRef())
                allocate(it->first, it->second, true);
            _hits.erase(it);
        }
    }

    void flatten()
    {
        Clock clock(STAGE_REBUILDER_GENERATOR_FLATTEN);
        _rebuilders.flatten();
    }

    void find_subclasses_and_cp()
    {
        Clock clock(STAGE_REBUILDER_GENERATOR_SUBCLASSES_AND_CP);
        
        // now process subclasses
        SubklassSearchClosure ksc(_rebuilders);
        ClassLoaderDataGraph::classes_do(&ksc);
    }

private:
    KlassRebuilder* allocate(Klass *k, KlassHitData* hit, bool direct_hit)
    {
        KlassRebuilder *kbuilder = (KlassRebuilder*)k->rebuilder();
        if(kbuilder == NULL || kbuilder == RebuilderMarkers::prev_marker())
        {
            assert(k->oop_is_instance(), "Should not try to modify non instance klass!");
            kbuilder = new KlassRebuilder();
            KlassRebuilder* parent = NULL;
            if(!KlassRebuilder::is_terminal(k)) {
//                if(REMIXVerbose)
//                    tty->print("Found parent for %s : %s, preallocating.\n", k->name()->as_C_string(), k->java_super()->name()->as_C_string());
                parent = allocate(k->java_super(), hit, false);
            }

            kbuilder->init((InstanceKlass*)k, parent, hit, direct_hit);
            assert(kbuilder != NULL, "Allocation failure");
            _rebuilders.add(k, kbuilder);
            k->set_rebuilder(kbuilder); // valid markers are cleared by KlassRebuilderMap
        }
        else
            kbuilder->merge_hits(hit);
        return kbuilder;
    }

private:
    RebuilderGenerator(KlassHitMap& hits, KlassRebuilderMap& rebuilders) : _hits(hits), _rebuilders(rebuilders)
    {
    }

    KlassHitMap& _hits;
    KlassRebuilderMap& _rebuilders;
};


#endif // __REMIX_REBUILDER_HPP__

