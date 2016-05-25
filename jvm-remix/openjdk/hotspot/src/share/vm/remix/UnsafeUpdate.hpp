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
#ifndef __REMIX_UNSAFE_UPDATE_HPP__
#define __REMIX_UNSAFE_UPDATE_HPP__

#include <map>
#include <set>

struct UnsafeUpdateEntry
{
    unsigned int tgt_offset;
    unsigned int src_offset;
};

class UnsafeUpdater
{
private:
    UnsafeUpdater()
    {
    }

public:
    static UnsafeUpdater& getInstance()
    {
        static UnsafeUpdater updater;
        return updater;
    }

    ~UnsafeUpdater()
    {
    }

    bool is_fixed(InstanceKlass* k)
    {
        return _fixed.find(k) != _fixed.end();
    }

    static bool set_unsafe_field_offset(InstanceKlass* k, int tgt_static_slot, int src_slot)
    {
        UnsafeUpdateEntry entry;
        entry.src_offset = k->field_offset(src_slot);
        entry.tgt_offset = k->field_offset(tgt_static_slot);
//        printf("Setting unsafe entry: %p %d %d\n", k, entry.tgt_offset, entry.src_offset);
        set_entry(k, entry);
        return true;
    }

    bool register_unsafe_field_offset(InstanceKlass* k, int tgt_static_slot, int src_slot)
    {
        UnsafeUpdateEntry entry;
        entry.src_offset = k->field_offset(src_slot);
        entry.tgt_offset = k->field_offset(tgt_static_slot);

//        printf("New unsafe entry: %p %d %d\n", k, entry.tgt_offset, entry.src_offset);
        _map.insert(MapType::value_type(k, entry));
        set_entry(k, entry);

        return true;
    }

    bool register_unsafe_field_offset_taken(InstanceKlass* k, int src_slot)
    {
        UnsafeUpdateEntry entry;
        entry.src_offset = k->field_offset(src_slot);

        if(REMIXDebug)
        {
            ResourceMark rm;
            tty->print("REMIX: New fixed unsafe entry: %s\n", k->name()->as_C_string());
        }
        _fixed.insert(k);

        return true;
    }
    static inline uint64_t* entry_addr(Klass* k, UnsafeUpdateEntry& entry)
    {
        return (uint64_t*)(((char*)k->java_mirror()) + entry.tgt_offset);
    }

    static void set_entry(Klass* k, UnsafeUpdateEntry& entry)
    {
        uint64_t* addr = entry_addr(k, entry);
        *addr = entry.src_offset;
        if(REMIXDebug)
            tty->print("REMIX: UnsafeUpdater: will set (%p-%p) %p %lu %i\n", k, k->java_mirror(), addr, *addr, entry.src_offset);
    }
    
    static void update_entry(Klass* k, UnsafeUpdateEntry& entry, int diff)
    {
        uint64_t* addr = entry_addr(k, entry);
        entry.src_offset += diff;
        if(REMIXDebug)
            tty->print("REMIX: Unsafe updater: Will change (%p-%p) %p %lu %lu\n", k, k->java_mirror(), addr, *addr, *addr + diff);
        *addr += diff;
        assert(*addr == entry.src_offset, "Offset mismatch!");
    }

    typedef std::set<InstanceKlass*> SetType;
    typedef std::multimap<InstanceKlass*, UnsafeUpdateEntry> MapType;
    MapType::iterator begin(InstanceKlass* k)
    {
        return _map.lower_bound(k);
    }

    MapType::iterator end(InstanceKlass* k)
    {
        return _map.upper_bound(k);
    }
    
private:
    SetType _fixed;
    MapType _map;
};

#endif // __REMIX_UNSAFE_UPDATE_HPP__


