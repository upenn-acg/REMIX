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
#ifndef __REMIX_HITMAP_HPP__
#define __REMIX_HITMAP_HPP__

#include <vector>
//#include <unordered_map> // jdk doesn't like C++11
#include <tr1/unordered_map> // jdk doesn't like C++11
#include <algorithm>
#include "CacheLine.hpp"
#include "HitData.hpp"

class HitPair : public std::pair<const void*, HitData>
{
public:
    HitPair() { }
    HitPair(const void* addr, const HitData& hitdata) : std::pair<const void*, HitData>(addr, hitdata){ }
    bool operator<(const HitPair& rhs) const
    { return first < rhs.first; }
    bool operator<(const void* rhs) const
    { return first < rhs; }
};

typedef std::vector<HitPair> HitVector;

struct HitAddrCompare : public std::binary_function<HitPair&, const char*, bool>
{ 
    bool operator()(const HitPair& lhs, const char* rhs) const { return lhs.first < rhs; } 
    bool operator()(const HitVector::const_iterator& lhs, const char* rhs) const { return lhs->first < rhs; } 
} ;

class HitMap
{
public:
    HitMap() { }
    ~HitMap() { }

    typedef std::tr1::unordered_map<const void*, int> AddrMapType;
    
    class HitMapIterator
    {
    public:
        HitMapIterator(AddrMapType::const_iterator begin, AddrMapType::const_iterator end, const HitMap& hm) : _it(begin), _end(end), _hm(hm)
        { 
        }

        bool done() const 
        {
             return _it == _end; 
        }

        void next()
        {
            ++ _it;
        }
      
        inline HitPair value() const
        {
            const void* addr = _it->first;
            bool shared =  _hm.is_shared(addr);
            unsigned int hits = _it->second;
            return HitPair(addr, HitData(shared, hits));
        }
 
    private:
        AddrMapType::const_iterator _it;
        AddrMapType::const_iterator _end;
        const HitMap& _hm;
    };

    HitMapIterator iterator() const { return HitMapIterator(_addrs.begin(), _addrs.end(), *this); }

    inline static uintptr_t cacheline(const void* addr) { return ((uintptr_t)addr) >> CACHE_LINE_BITS; }
    inline static uintptr_t cachebit(const void* addr) { return ((uintptr_t)addr) & (CACHE_LINE_SIZE - 1); }

    void add(const void* addr, unsigned int count)
    {
       _addrs[addr] += count;
       _clines[cacheline(addr)] |= 1 << cachebit(addr);
    }

    void merge(HitMap& other)
    {
        for(AddrMapType::iterator it = other._addrs.begin(), end = other._addrs.end();
            it != end;
            ++ it)
            _addrs[it->first] += it->second;
        
        for(CacheLineBitmapMap::iterator it = other._clines.begin(), end = other._clines.end();
            it != end;
            ++ it)
            _clines[it->first] |= it->second;
    }

    void clear()
    {
        _addrs.clear();
        _clines.clear();
    }

    inline bool is_shared(const void* addr) const 
    {
        CacheLineBitmapMap::const_iterator it =  _clines.find(cacheline(addr));
        assert(it != _clines.end(), "Missing bitmap");
        CacheLineBitmap  value = it->second;
        bool result = (value & (~value + 1)) != value;
        //printf("Bitmap for %p [%lx] is %lx shared=%i\n", addr, it->first, it->second, (int)result);
        return result;
    }

private:
    typedef uint64_t CacheLineBitmap;
    typedef std::tr1::unordered_map<uintptr_t, CacheLineBitmap> CacheLineBitmapMap;

    AddrMapType _addrs;
    CacheLineBitmapMap _clines;
};


#endif // __REMIX_HITMAP_HPP__
