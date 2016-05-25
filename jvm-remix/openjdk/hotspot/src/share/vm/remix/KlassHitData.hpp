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

#ifndef __KLASS_HIT_DATA_HPP__
#define __KLASS_HIT_DATA_HPP__

#include "FieldHitData.hpp"

class KlassHitData
{
public:
    enum { HEADER_SIZE = (2 * sizeof(HeapWord)) };
private:
    KlassHitData();
    KlassHitData(Klass* klass);
    KlassHitData(const KlassHitData&) /*= delete*/;
    KlassHitData& operator=(const KlassHitData&) /*= delete*/;

public:
    static KlassHitData* create(Klass* klass);

    void update_hit(unsigned int offset, const HitData& count);
    const inline HitData& header_hits() const { return _header_hits; }
    bool inline should_relayout_header() const { return Policy::should_handle_hit(_header_hits); }
    const inline HitData& total_hits() const { return _total_hits; }
  
    void print_out();
 
    void serialize(FILE* file);
    static KlassHitData* deserialize(FILE* file);

    inline const char* name() const { return _klass_name.c_str(); }

    inline FieldArray::iterator begin() { return _hits.begin(); }
    inline FieldArray::iterator end() { return _hits.end(); }

    inline bool should_relayout(Klass* klass) { return 
            Policy::under_klass_blank_limit(klass->get_fs_blank_count()) &&
            Policy::should_handle_hit(_total_hits); 
    }

private:
    std::string _klass_name;
    HitData _total_hits, _header_hits;
    FieldArray _hits;
    FieldArray _ts_hits;
};

#endif // __KLASS_HIT_DATA_HPP__
