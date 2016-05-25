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
#include "oops/klass.hpp"
#include "memory/space.hpp"
#include "oops/oop.inline.hpp"
#include "oops/oop.inline2.hpp"
#include <stdio.h>

class HeapObjDump : public ObjectClosure
{
public:
    Space* _space;
    FILE* _file;
    HeapObjDump(Space* space, FILE* file) : _space(space), _file(file) { }
    virtual void do_object(oop o) 
    {
        Klass* k = o->klass();
        fprintf(_file, "  OOP: %p size=0x%lx osize=0x%x rs=0x%lx mark=0x%lx klass=%p\n", o, _space->block_size((HeapWord*)o), o->size(), o->size() * sizeof(HeapWord),(uintptr_t)o->mark(), k);
        while(k != NULL)
        {
            fprintf(_file, "  KLASS: %s\n", k->signature_name());
            k = k->super();
        }
//        o->print_value();
        fprintf(_file, "\n");
    }
};

class HeapDump : public SpaceClosure
{
public:
    static int count, count2;
    FILE* _file;
    int _spcount;
    HeapDump()
    {
        char name[100];
        sprintf(name, "dump_%i_%i.txt", getpid(), count ++);
        printf("Dumping heap to %s\n", name);
        _file = fopen(name, "w");
        _spcount = 0;
    }
    ~HeapDump()
    {
        fclose(_file);
    }


    virtual void do_space(Space* space)
    {
        ContiguousSpace* sp = space->toContiguousSpace();
        assert(sp != NULL, "space->toContiguousSpace()");
        fprintf(_file, "= Space[%i] %p - %p - %p, used=%lxp\n", _spcount++, space->bottom(), sp->top(), space->end(), space->used());
        HeapObjDump od(space,  _file);
        sp->object_iterate(&od, sp->top());

        char* sp_bottom = (char*)sp->bottom();
        char* sp_end = (char*)sp->end();
        char name[100];
        sprintf(name, "%lx-%lx-%i\n", (uintptr_t)sp_bottom, (uintptr_t)sp_end, count2 ++ );
        FILE* f = fopen(name, "w");
        fwrite(sp_bottom, sp_end - sp_bottom, 1, f);
        fclose(f);
    }

};
int HeapDump::count = 0;
int HeapDump::count2 = 0;

