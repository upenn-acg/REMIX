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
#ifndef __REMIX_POLICY_HPP__
#define __REMIX_POLICY_HPP__

#define HIT_THRESHOLD 25
#define UNPROCESSED_THRESHOLD   25
#define MIN_COLLECT_THRESHOLD  2
#define FIELD_HIT_THRESHOLD  5
#define KLASS_BLANK_LIMIT 3
//#define HIT_THRESHOLD           5
//#define UNPROCESSED_THRESHOLD   5

#include "HitData.hpp"

class Policy
{
public:
    static inline bool should_handle_hit(const HitData& hd)
    {
        return hd.shared >= HIT_THRESHOLD;
        //return (hd.shared +hd.single) >= HIT_THRESHOLD;
    }
    static inline bool should_handle_field_hit(const HitData& hd)
    {
        return hd.shared >= FIELD_HIT_THRESHOLD;
        //return (hd.shared +hd.single) >= HIT_THRESHOLD;
    }
    static inline bool under_klass_blank_limit(int count)
    {
        return count < KLASS_BLANK_LIMIT;
    }
    
private:
    Policy() { }
};

#endif // __REMIX_POLICY_HPP__
