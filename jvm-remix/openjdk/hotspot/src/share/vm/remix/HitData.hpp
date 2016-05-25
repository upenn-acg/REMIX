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
#ifndef __REMIX_HIT_DATA_HPP__
#define __REMIX_HIT_DATA_HPP__


#include "oops/klass.hpp"
#include "oops/oop.inline.hpp"
#include "oops/oop.inline2.hpp"

struct HitData
{
    inline HitData() : shared(0), single(0) { }
    inline explicit HitData(bool shared_, int value) { if(shared_) { shared = value; single = 0; } else { shared  = 0; single = value; } }
    inline HitData(const HitData& other) : shared(other.shared), single(other.single) { }
    inline HitData& operator=(const HitData& other) { shared = other.shared; single = other.single; }

    inline void merge(const HitData& other) { shared += other.shared; single += other.single; }

    unsigned int shared;
    unsigned int single;
};


#endif // __REMIX_HIT_DATA_HPP__
