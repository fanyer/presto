/*
*	(c) Copyright IBM Corp. 1991, 2002 All Rights Reserved
*/
#ifndef jniport_h
#define jniport_h

#if defined(WIN32) || defined(J9WINCE) || defined(RIM386) || (defined(BREW) && defined(AEE_SIMULATOR))

#define JNIEXPORT __declspec(dllexport)
#define JNICALL __stdcall
typedef signed char jbyte;
typedef int jint;
typedef __int64 jlong;

#else

#define JNIEXPORT 

#ifdef OS2
#define JNICALL _System
#define PJNICALL * JNICALL
#else
#define JNICALL
#endif /* OS2 */

typedef signed char jbyte;

typedef long long jlong;

#ifdef BREW
#include "AEEFile.h"
#define FILE IFile
#endif

#if defined (PILOT)
#include <stddef.h>	/* <stdio.h> doesn't define NULL on PalmOS, so pull in stddef to make users happier */
#define FILE void
typedef long jint;
#else
typedef int jint;
#endif

#endif /* WIN32 */

#endif     /* jniport_h */

