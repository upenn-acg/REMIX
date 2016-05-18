/*
 * Copyright (c) 1994, 2000, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
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
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

/* Code Modified for REMIX by Ariel Eizenberg, arieleiz@seas.upenn.edu.
 * ACG group, University of Pennsylvania.
 */

/*
 *      Link foreign methods.  This first half of this file contains the
 *      machine independent dynamic linking routines.
 *      See "BUILD_PLATFORM"/java/lang/linker_md.c to see
 *      the implementation of this shared dynamic linking
 *      interface.
 *
 *      NOTE - source in this file is POSIX.1 compliant, host
 *             specific code lives in the platform specific
 *             code tree.
 */

#include "jni.h"
#include "jni_util.h"
#include "jvm.h"

#include "java_lang_Runtime.h"

// REMIX start
JNIEXPORT void JNICALL
Java_java_lang_Runtime_remixRepairFalseSharing(JNIEnv *env, jobject this)
{
    JVM_remix_repair_false_sharing();
}
JNIEXPORT void JNICALL
Java_java_lang_Runtime_remixTestHeapScanSpeed(JNIEnv *env, jobject this, jint count)
{
    JVM_remix_test_heap_scan_speed(count);
}
JNIEXPORT void JNICALL
Java_java_lang_Runtime_remixPrintObject(JNIEnv *env, jobject this, jobject obj)
{
    JVM_remix_print_object(obj);
}
JNIEXPORT jint JNICALL
Java_java_lang_Runtime_remixObjectSize(JNIEnv *env, jobject this, jobject obj)
{
    return JVM_remix_object_size(obj);
}
// REMIX end

JNIEXPORT jlong JNICALL
Java_java_lang_Runtime_freeMemory(JNIEnv *env, jobject this)
{
    return JVM_FreeMemory();
}

JNIEXPORT jlong JNICALL
Java_java_lang_Runtime_totalMemory(JNIEnv *env, jobject this)
{
    return JVM_TotalMemory();
}

JNIEXPORT jlong JNICALL
Java_java_lang_Runtime_maxMemory(JNIEnv *env, jobject this)
{
    return JVM_MaxMemory();
}

JNIEXPORT void JNICALL
Java_java_lang_Runtime_gc(JNIEnv *env, jobject this)
{
    JVM_GC();
}

JNIEXPORT void JNICALL
Java_java_lang_Runtime_traceInstructions(JNIEnv *env, jobject this, jboolean on)
{
    JVM_TraceInstructions(on);
}

JNIEXPORT void JNICALL
Java_java_lang_Runtime_traceMethodCalls(JNIEnv *env, jobject this, jboolean on)
{
    JVM_TraceMethodCalls(on);
}

JNIEXPORT void JNICALL
Java_java_lang_Runtime_runFinalization0(JNIEnv *env, jobject this)
{
    jclass cl;
    jmethodID mid;

    if ((cl = (*env)->FindClass(env, "java/lang/ref/Finalizer"))
        && (mid = (*env)->GetStaticMethodID(env, cl,
                                            "runFinalization", "()V"))) {
        (*env)->CallStaticVoidMethod(env, cl, mid);
    }
}

JNIEXPORT jint JNICALL
Java_java_lang_Runtime_availableProcessors(JNIEnv *env, jobject this)
{
    return JVM_ActiveProcessorCount();
}
