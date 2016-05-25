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

#include "oops/fieldStreams.hpp"
#include "oops/klass.hpp"
#include "oops/oop.inline.hpp"
#include "oops/oop.inline2.hpp"
#include "runtime/handles.hpp"
#include "runtime/handles.inline.hpp"
#include "HitData.hpp"
#include "KlassHitData.hpp"
#include "FieldHitData.hpp"
#include <algorithm>
#include <ctype.h>

struct FieldArraySorter
{
    inline bool operator()(const FieldHitData* first, const FieldHitData* second)
    {
        return first->offset() < second->offset();
    }
};

KlassHitData::KlassHitData()  
{
}

KlassHitData::KlassHitData(Klass* klass) : _klass_name(klass->name()->as_C_string())
{ 
    bool inherited = false;
    do
    {
        for(AllFieldStream afs(klass); !afs.done(); afs.next())
        { 
            if (!afs.access_flags().is_static())
            {
                std::string name;
                Symbol* sym = afs.name();
                if(sym != NULL)
                    name = sym->as_C_string();
                _hits.push_back(FieldHitData::create(inherited, afs.offset(), name)); 
            }
        }
        klass = klass->java_super();
        inherited = true;
    }
    while(klass != NULL);

    std::sort(_hits.begin(), _hits.end(), FieldArraySorter());
    
    for(FieldArray::const_iterator it = _hits.begin();
        it != _hits.end();
        ++ it)
    {
        FieldHitData* hd = *it;
        if(REMIXHitVerbose)
            tty->print("REMIX: Hit at %s::%s[%i]%s\n", _klass_name.c_str(), hd->name(), hd->offset(), hd->inherited() ? " <INHERITED>" : "");
    }
}

KlassHitData* KlassHitData::create(Klass* klass)
{
    KlassHitData* result = new KlassHitData(klass);
    assert(result != NULL, "Memory allocaton error!\n");
    return result;
}

void KlassHitData::update_hit(unsigned int offset, const HitData& hit_count)
{
    unsigned int start = 0;
    unsigned int end = _hits.size();
    unsigned int count = end;
    bool exact = false;

    _total_hits.merge(hit_count);
    if(offset < HEADER_SIZE)
    {
        _header_hits.merge(hit_count);
        return;
    }

    while(start < end)
    {
        int middle = (start + end) / 2;
        FieldHitData *hd = _hits[middle];
        unsigned int hdoffset = hd->offset();
        if(offset == hdoffset)
        {
            start = middle;
            exact = true;
            break;
        }
        else if (offset > hdoffset)
            start = middle + 1;
        else
            end = middle;
    }

    if(start >= count)
        start = count - 1;

    if(!exact)
    {
        if(start == 0)
        {
            _header_hits.merge(hit_count);
            tty->print("REMIX: *** WARNING ***: HIT AT NON_EXISTING FIELD!\n");
            return;
        }
        start -= 1;
    }

    if(REMIXHitVerbose)
        tty->print("REMIX: Hit to %s:%i -> %i %i:%i\n", _klass_name.c_str(), offset, start,hit_count.shared, hit_count.single);
    _hits[start]->hit(hit_count);
}
  
void KlassHitData::print_out()
{
    tty->print("REMIX: Hits for class '%s':\n", _klass_name.c_str());

    if(_header_hits.single > 0 || _header_hits.shared > 0)
        tty->print("REMIX: - Hit %s::header: %i:%i\n", _klass_name.c_str(), _header_hits.single, _header_hits.shared);

    for(FieldArray::const_iterator it = _hits.begin();
        it != _hits.end();
        ++ it)
    {
        FieldHitData* hd = *it;
        HitData count = hd->hit_count();
        if(count.single > 0 || count.shared > 0)
        {
            tty->print("REMIX: - Hit %s::%s[%i]: %i:%i\n", _klass_name.c_str(), hd->name(), hd->offset(), count.shared, count.single);
        }
    }
}

void KlassHitData::serialize(FILE* file)
{
    bool first = true;

    for(FieldArray::const_iterator it = _hits.begin();
        it != _hits.end();
        ++ it)
    {
        FieldHitData* hd = *it;
        HitData count = hd->hit_count();
        if(count.single > 0 || count.shared > 0)
        {
            if(first)
            {
                first = false;
                fprintf(file, "%s", _klass_name.c_str());
            }
            fprintf(file, "\t%s", hd->name());
        }
    }
    if(!first)
        fprintf(file, "\n");
}

KlassHitData* KlassHitData::deserialize(FILE* file)
{
    char buffer[2048];
    if(fgets(buffer, sizeof(buffer), file) == NULL)
        return NULL;

    char* pos = buffer;
    char* nameptr = pos;
    for(;*pos && !isspace(*pos); ++pos)
        ;
    std::string name(nameptr, pos);

    for(;*pos && isspace(*pos); ++pos)
        ;
    if(*pos == 0)
        return NULL;

    KlassHitData* result = new KlassHitData();
    assert(result != NULL, "Memory allocation error!");
    result->_klass_name = name;
    while(*pos)
    {
        nameptr = pos;
        for(;*pos && !isspace(*pos); ++pos)
            ;
        std::string name(nameptr, pos);
        result->_hits.push_back(FieldHitData::create(false /*XXX */, 0, name));
        
        for(;*pos && isspace(*pos); ++pos)
            ;
    }

    return result;        
}

