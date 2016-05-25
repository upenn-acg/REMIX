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
#ifndef __REMIX_HWDETECT_HPP__
#define __REMIX_HWDETECT_HPP__

#include "runtime/globals.hpp"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <set>
#include <sys/utsname.h>

namespace SystemDetect
{
    class BaseDetect
    {
    public:
        bool parse_line(const char* line)
        {
            return parse_line_imp(line);
        }

        bool ok()
        {
            if(_ok)
            {
                if(_nexts.size() == 0)
                    return true;
                for(std::vector<BaseDetect*>::iterator it = _nexts.begin(); it != _nexts.end(); ++ it)
                    if((*it)->ok())
                        return true;
            }
            return false;
        }

    protected:
        typedef enum { VALUE_STRING, VALUE_INT } VALUE_TYPE;
        BaseDetect(const std::string& caption, VALUE_TYPE value_type) :
            _value_type(value_type),  
            _caption(caption), 
            _ok(false)
        { 
            _cap_len = _caption.size();
        }

        bool parse_line_imp(const char* line)
        {
            const char* colon = strchr(line, ':');
            if(colon == NULL)
                return false;
            const char* end = colon;
            while(end > line && isspace(end[-1]))
                -- end;
            colon += 1;
            while(isspace(colon[0]))
                ++ colon;
            if((end - line) == _cap_len && strncmp(line, _caption.c_str(), _cap_len) == 0)
            {
                long value;
                const char* end = NULL;

                switch(_value_type)
                {
                case VALUE_STRING:
                    end = colon + strlen(colon);
                    while(end > colon && isspace(end[-1]))
                        --end;
                    return test_value(std::string(colon, end).c_str());
                case VALUE_INT:    
                    value = strtol(colon, (char**)&end, 10);
                    if(end == NULL)
                        return false;
                    while(*end && isspace(*end))
                        ++ end;
                    if(*end)
                        return false;
                    return test_value(value);
                default:
                    return false;
                }
            }

            for(std::vector<BaseDetect*>::iterator it = _nexts.begin(); it != _nexts.end(); ++ it)
                if((*it)->parse_line_imp(line))
                    return true;
            
            return false;
        }

    protected:
        virtual bool test_value(const char* value) {return false; }
        virtual bool test_value(int value) { return false; }

    protected:
        VALUE_TYPE _value_type;
        std::string _caption;
        int _cap_len;
        std::vector<BaseDetect*> _nexts;
        bool _ok;
    };

    class IntelModelDetect : public BaseDetect
    {
    public:
        IntelModelDetect(const int* models, size_t model_count, const int* events, size_t event_count) :
             BaseDetect("model", VALUE_INT)
        {
            for(size_t i = 0; i < model_count; ++ i)
                _models.insert(models[i]);
            for(size_t i = 0; i < event_count; ++ i)
                _events.push_back(events[i]);
        }
        virtual bool test_value(int value) /*override*/
        {
            if(_models.find(value) != _models.end())
            {
                _ok = true;
                return true;
            }
            return false;
        }

        virtual void get_tracked_event_ids(std::vector<unsigned int>& event_ids) 
        { 
            if(_ok)
                event_ids.insert(event_ids.end(), _events.begin(), _events.end());
        }

    private:
        std::set<unsigned int> _models;
        std::vector<unsigned int> _events;
    };

    class ModelDetectUntested : public IntelModelDetect
    {
    public:
        ModelDetectUntested(const int* models, size_t model_count, const int* events, size_t event_count) :
            IntelModelDetect(models, model_count, events, event_count),
            _warned(false)
        {
        }

    protected:
        virtual bool test_value(int value) /*override*/
        {
            if(IntelModelDetect::test_value(value))
            {
                if(!_warned)
                {
                    tty->print("REMIX: Warning: This model has not been tested!\n");
                    _warned = true;
                }
                return true;
            }
            return false;
        }

    protected:
        bool _warned;
    };

    static const int MODELS_HASWELL[] = { 0x3F };
    static const int MODELS_HASWELL_UNTESTED[] = { 0x3E, 0x45, 0x46 };
    static const int EVENTS_HASWELL[] = { 0x4D2, 0x10D3 };
            // MEM_LOAD_UOPS_L3_HIT_RETIRED.XSNP_HITM
            // MEM_LOAD_UOPS_L3_MISS_RETIRED.REMOTE_HITM

    static const int MODELS_BDW_SKL_UNTESTED[] = { 0x3D,0x4E,0x5E,0x47,0x56 };
    static const int EVENTS_BDW_SKL[] = { 0x10D3 };
            // MEM_LOAD_UOPS_L3_HIT_RETIRED.XSNP_HITM

    template< typename T, size_t N >
    size_t __remix_countof( const T (&)[N] ) { return N; }

    class IntelFamilyDetect : public BaseDetect
    {
    public:
        IntelFamilyDetect() : BaseDetect("cpu family", VALUE_INT), 
                _next_hsw(MODELS_HASWELL, __remix_countof(MODELS_HASWELL), 
                          EVENTS_HASWELL, __remix_countof(EVENTS_HASWELL)),
                _next_hsw_untested(MODELS_HASWELL_UNTESTED, __remix_countof(MODELS_HASWELL_UNTESTED), 
                                   EVENTS_HASWELL, __remix_countof(EVENTS_HASWELL)),
                _next_bdw_skl_untested(MODELS_BDW_SKL_UNTESTED, __remix_countof(MODELS_BDW_SKL_UNTESTED), 
                                       EVENTS_BDW_SKL, __remix_countof(EVENTS_BDW_SKL))
        {
            _nexts.push_back(&_next_hsw);
            _nexts.push_back(&_next_hsw_untested);
            _nexts.push_back(&_next_bdw_skl_untested);
        }


        virtual void get_tracked_event_ids(std::vector<unsigned int>& event_ids) 
        { 
            _next_hsw.get_tracked_event_ids(event_ids); 
            _next_hsw_untested.get_tracked_event_ids(event_ids); 
            _next_bdw_skl_untested.get_tracked_event_ids(event_ids); 
        }

    protected:
        virtual bool test_value(int value) /*override*/
        {
            if(value == 6)
            {
                _ok = true;
                return true;
            }
            return false;
        }
    private:
        IntelModelDetect _next_hsw;
        ModelDetectUntested _next_hsw_untested;
        ModelDetectUntested _next_bdw_skl_untested;
    };

    class VendorDetect : public BaseDetect
    {
    public:
        VendorDetect() : BaseDetect("vendor_id", VALUE_STRING)
        {
            _nexts.push_back(&_next);
        }
        virtual bool test_value(const char* value) /*override*/
        {
            if(strcmp(value, "GenuineIntel") == 0)
            {
                _ok = true;
                return true;
            }
            return false;
        }

        virtual void get_tracked_event_ids(std::vector<unsigned int>& event_ids) { _next.get_tracked_event_ids(event_ids); }

    private:
        IntelFamilyDetect _next;
    };

    class RemixSystemDetect
    {
    public:
        static RemixSystemDetect& instance()
        {
            static RemixSystemDetect instance;
            return instance;
        }

        size_t events_count()
        {
            return _events.size();
        }

        int event_id(int index)
        {
            return _events[index];
        }

    private:
        RemixSystemDetect()
        {
            test_cpu();
            test_kernel();
            if(REMIXVerbose)
                tty->print("REMIX: System tests passed.\n");
        }

        void test_cpu()
        {
            VendorDetect vd;       
            char buffer[260];

            FILE* f = fopen("/proc/cpuinfo", "r");
            if(f == NULL)
                fatal("Failed opening /proc/cpuinfo");

            while(fgets(buffer, sizeof(buffer), f) != NULL)
            {
                vd.parse_line(buffer);
            }

            if(!vd.ok())
            {
                fatal("Unsupported hardware!\n");
            }

            vd.get_tracked_event_ids(_events);
        }

        void test_kernel()
        {
            struct utsname uts;
            uname(&uts);
            int major = 0, minor = 0, rel = 0;
            if(3 != sscanf(uts.release, "%i.%i.%i", &major, &minor, &rel))
                fatal("Could not parse kernel release!\n");
            if(major < 4 || (major == 4 && minor < 1))
                fatal("Kernel version < 4.1.0!\n");
        }

    private:
        std::vector<unsigned int> _events;
    };
};

#endif // __REMIX_HWDETECT_HPP__

