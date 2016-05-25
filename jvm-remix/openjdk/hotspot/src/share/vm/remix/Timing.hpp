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

#ifndef __AE_TIMING_HPP__
#define __AE_TIMING_HPP__

// Kills build:
//#include <time.h>
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC         1
extern int clock_gettime(int clk_id, struct timespec *tp);
struct timespec {
               time_t   tv_sec;        /* seconds */
               long     tv_nsec;       /* nanoseconds */
};
#endif

#define DEFINE_STAGES(DEFINER)                              \
    DEFINER(STAGE_TOTAL)                                    \
    DEFINER(STAGE_THREAD_START)                             \
    DEFINER(STAGE_THREAD_END)                               \
    DEFINER(STAGE_PERF_COLLECT_PARALLEL)                    \
    DEFINER(STAGE_PERF_COLLECT_PARALLEL_BLOCKED)            \
    DEFINER(STAGE_PERF_COLLECT)                             \
    DEFINER(STAGE_MERGE_LIVE_THREADS)                       \
    DEFINER(STAGE_RESTORE_LIVE_THREADS)                     \
    DEFINER(STAGE_SAFEPOINT_OP)                             \
    DEFINER(STAGE_FIND_KLASSES)                             \
    DEFINER(STAGE_KLASS_GATHER)                             \
    DEFINER(STAGE_RELAYOUT_OP)                              \
    DEFINER(STAGE_REBUILDER_GENERATOR)                      \
    DEFINER(STAGE_REBUILDER_GENERATOR_DRILL_DOWN)           \
    DEFINER(STAGE_REBUILDER_GENERATOR_FLATTEN)              \
    DEFINER(STAGE_REBUILDER_GENERATOR_SUBCLASSES_AND_CP)    \
    DEFINER(STAGE_RELAYOUT_PROLOGUE)                        \
    DEFINER(STAGE_RELAYOUT_MARK_OBJECTS)                    \
    DEFINER(STAGE_RELAYOUT_MOVE_OBJECTS)                    \
    DEFINER(STAGE_RELAYOUT_ADJUST_POINTERS)                 \
    DEFINER(STAGE_RELAYOUT_EPILOGUE)                        \
    DEFINER(STAGE_RELAYOUT_DEOPTIMIZE)                      \


#define ENUM_DEFINER(x) x,
#define TOSTRING_DEFINER(x) case x: return #x;

enum LifeStage
{
    DEFINE_STAGES(ENUM_DEFINER)
    NUM_STAGES
};

class Timing
{
public:
    static inline Timing& getInstance()
    {
        static Timing timing;
        return timing;
    }

    long get_clock(int id)
    {
        return _total_time[id];
    }

    void record(LifeStage stage, long time)
    {
        _counts[stage] += 1;
        _total_time[stage] += time;
        if(_max_time[stage] < time)
            _max_time[stage] = time; 
    }

    void print_timings()
    {
        if(!REMIXTiming)
            return;
        for(int i = 0; i < NUM_STAGES; ++ i)
        {
            int countdiv = _counts[i];
            if(countdiv == 0) countdiv = 1;
            tty->print("CLOCK\t%s\t%li\t%li\t%li\t%i\n", stage_name(i), _total_time[i], _total_time[i] / countdiv, _max_time[i], _counts[i]);
        }
    }

    static const char* stage_name(int stage)
    {
        switch(stage)
        {
        DEFINE_STAGES(TOSTRING_DEFINER)
        }
        return "Unknown";
    }
private:
    Timing()
    {
        for(int i = 0; i < NUM_STAGES; ++ i)
            _total_time[i] = 0;
        for(int i = 0; i < NUM_STAGES; ++ i)
            _max_time[i] = 0;
        for(int i = 0; i < NUM_STAGES; ++ i)
            _counts[i] = 0;
    }

private:
    long _total_time[NUM_STAGES];
    long _max_time[NUM_STAGES];
    int  _counts[NUM_STAGES];
};

class Clock
{
public:
    Clock(LifeStage stage, bool always = false) 
    {
        if(!REMIXTiming && !always)
        {
            _stage = (LifeStage)-1;
            return;
        }
        _stage = stage;
        do_start();
    }

    ~Clock()
    {
        do_stop();
    }

    long stop()
    {
        return do_stop();
    }

    static void print_now(const char* name)
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        long total = ts.tv_sec * 1000000000L;    
        total += ts.tv_nsec; 
        tty->print("%s: %li\n", name, total);
    }
    static void print_now(const char* name, int stage)
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        long total = ts.tv_sec * 1000000000L;    
        total += ts.tv_nsec; 
        tty->print("%s %s: %li\n", name, Timing::stage_name(stage), total);
    }

private:
    inline void do_start()
    {
        clock_gettime(CLOCK_MONOTONIC, &_start);
        print_now("STARTING ", _stage);
    }
    inline long do_stop()
    {
        if((int)_stage == -1)
            return 0;
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        ts.tv_sec -= _start.tv_sec;
        ts.tv_nsec -= _start.tv_nsec;
        long total = ts.tv_sec * 1000000000L;    
        total += ts.tv_nsec;
        Timing::getInstance().record(_stage, total);
        print_now("DONE ", _stage);
        _stage = (LifeStage)-1;
        return total;
    }
    
    struct timespec _start;
    LifeStage _stage;
};

#endif // __AE_TIMING_HPP__
