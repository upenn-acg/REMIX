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

#ifndef __FIELD_HIT_DATA_HPP__
#define __FIELD_HIT_DATA_HPP__

#include "oops/klass.hpp"
#include "oops/oop.inline.hpp"
#include "oops/oop.inline2.hpp"
#include "Policy.hpp"
#include "HitData.hpp"
#include <string>
#include <vector>
#include <stdio.h>

class FieldHitData
{

private:
    FieldHitData(bool inherited, unsigned int offset, const std::string& field_name) : _inherited(inherited), _offset(offset), _field_name(field_name) { }
    FieldHitData(const FieldHitData&) /*= delete*/;
    FieldHitData& operator=(const FieldHitData&) /*= delete*/;

public:
    static inline FieldHitData* create(bool inherited, unsigned int offset, const std::string& field_name)
    {
        FieldHitData* result = new FieldHitData(inherited, offset, field_name);
        assert(result != NULL, "ALLOCATION ERROR!");
        return result;
    }

    inline bool inherited() const 
    {
        return _inherited;
    }

    inline const char* name() const
    {
        return _field_name.c_str();
    }

    inline const HitData& hit_count() const
    {
        return _hit_count;
    }
   
    inline void hit(const HitData& data)
    {
        _hit_count.merge(data);
    }

    inline unsigned int offset() const
    {
        return _offset;
    }

    inline bool should_relayout() const 
    {
        return Policy::should_handle_field_hit(_hit_count);
    }

private:
    bool _inherited;
    unsigned int _offset;
    HitData _hit_count;
    std::string _field_name;
};
typedef std::vector<FieldHitData*> FieldArray;

#endif // __FIELD_HIT_DATA_HPP__
