/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MACINTOSH_SYSTEM_H
#define MACINTOSH_SYSTEM_H

/* wcstombs/mbstowcs considered poisoned. Re-define before anything else. */
#define wcstombs mac_wcstombs
#define mbstowcs mac_mbstowcs

/* For task DSK-295401 */
#define NO_CARBON

#ifdef NO_CARBON
#define NO_CARBON_HEADERS
#endif

#ifdef __cplusplus
#include "platforms/mac/util/MacTLS.h"
#define g_component_manager (MacTLS::Get()->component_manager)
#define g_cleanup_stack (MacTLS::Get()->cleanup_stack)
extern class CoreComponent* g_opera;
#endif // __cplusplus

/* Mac headers include things like Line(short,short), typedef long Size, and typedef unsigned char Style,
 * which has at times conflicted with core use of the same names. Re-define prior to include.
*/
#define Size MacSize
#define Style MacFontStyle
#define Line MacLine
#ifdef NO_CARBON_HEADERS
struct EventRecord;
enum { osEvt = 15 };
# include <ApplicationServices/ApplicationServices.h>
# include <CoreFoundation/CoreFoundation.h>
#else
# include <Carbon/Carbon.h>
#endif
#undef Line
#undef Style
#undef Size
/* undef problematic things that the previous system headers went and defined. */
#undef SYSTEM_CLOCK
#undef check
#undef verify
#undef TYPE_BOOL

#define XP_MAC 1
#define XP_MACOSX 1

/* posix system preamble: */

#include "platforms/posix/sys/include_early.h"
#include <assert.h>
#include "platforms/posix/sys/posix_choice.h"
#include "adjunct/quick/quick-version.h"

/* Endianness aand float format defined by make on unix. Not part of the posix system. */
#define SYSTEM_BIG_ENDIAN			((TARGET_CPU_PPC||TARGET_CPU_PPC64) ? YES : NO)
#define SYSTEM_IEEE_8087			((TARGET_CPU_X86||TARGET_CPU_X86_64) ? YES : NO)
#define SYSTEM_IEEE_MC68K			((TARGET_CPU_PPC||TARGET_CPU_PPC64) ? YES : NO)
/* Unicode-ify */
#define WCHAR_IS_UNICHAR
#define SYSTEM_NOTHROW								NO

/* Standard stuff from the unix system.h */
#define SYSTEM_NAMESPACE					YES
#undef SYSTEM_STRTOD /* does not conform to specification, should be locale-independent */
#define SYSTEM_STRTOD						NO
#undef SYSTEM_GETPHYSMEM
#define SYSTEM_GETPHYSMEM					YES /* SYS_GETPHYSMEM, OpSystemInfo::GetPhysicalMemorySizeMB */
#undef SYSTEM_GUID
#define SYSTEM_GUID							YES
#undef SYSTEM_OPFILELENGTH_IS_SIGNED
#define SYSTEM_OPFILELENGTH_IS_SIGNED		NO /* core has issues with this, assumes OpFileLength is unsigned - see DSK-295495 */
#undef SYSTEM_LOCAL_TYPE_AS_TEMPLATE_ARG 
#define SYSTEM_LOCAL_TYPE_AS_TEMPLATE_ARG	NO
#undef SYSTEM_MEMMEM
#define SYSTEM_MEMMEM						NO /* Mac tweak. We don't have this*/

#define __FreeBSD__ /* reverse parameter order to qsort_r */
#include "platforms/posix/sys/posix_declare.h" /* *after* fixing up SYSTEM_* defines. */
#undef __FreeBSD__

/* Disable plugin options not available in 64-bit. */
#if SYSTEM_64BIT
# define NP_NO_QUICKDRAW
#endif

#ifndef _NO_THREADS_ /* desktop_pi's threading: */
typedef pthread_t OpThreadId;
typedef pthread_mutex_t OpMutexId;
typedef struct sema_struct_t* OpSemaphoreId;
#endif /* _NO_THREADS_ */

#ifndef max /* Deprecated in the system system, in favour of MAX, MIN. */
#ifdef __cplusplus
template<class T,class T2> static inline T min(T a, T2 b) { if( a < b ) return a; else return b; }
template<class T,class T2> static inline T max(T a, T2 b) { if( a > b ) return a; else return b; }
#define max max
#define min min
#else /* __cplusplus */
# define min(a,b) (((a) < (b)) ? (a) : (b))
# define max(a,b) (((a) > (b)) ? (a) : (b))
#endif /* __cplusplus */
#endif /* max */

#define IN
#define OUT
#define IO
#define OPT
#define far
#define MAX_PATH PATH_MAX
#define huge
#define __export

#ifndef NEWLINE
# define NEWLINE "\n"
#endif /* NEWLINE */

#define USED_IN_BITFIELD /* Enums are unsigned by default in gcc */
#define PATH_CASE_INSENSITIVE TRUE /* Mac difference */

/* op_* defines not actually specified by the system system: */
#define op_offsetof(s,f)		offsetof(s,f)
#define op_sleep(s)				sleep(s)
/* The system system doesn't define op_stristr; but util/str.cpp does !  In
 * fact, stristr isn't defined on Linux, but we always got away with some calls
 * to it because we defined op_stristr to stristr, causing util_str.cpp to
 * define stristr, so various calls to stristr worked !  This (reversing the
 * direction of the define) doesn't really belong in !TYPE_SYSTEMATIC, but is
 * here so that normal builds should still work, while those trying to enforce
 * standards still get errors over this. */
#define stristr(s,f)			op_stristr(s,f)

/* The following two macros are used to catch call-stack information.  These are
 * implemented as macros to work in more environments, i.e. provide useful
 * information where stack-frames are not available and only one return-address
 * can be provided.
 *
 * This API for call-stack information is currently used by the memory module.
 */
#ifdef __GNUC__ /* TODO: what does the size parameter mean ? */
#define OPGETCALLSTACK(size) \
	void* opx_ret_address_hidden = __builtin_return_address(0); \
	void* opx_frm_address_hidden = __builtin_frame_address(0)
#endif /* else: you need to implement an equivalent ! */

#define OPCOPYCALLSTACK(ptr, size) \
	OpCopyCallStack(reinterpret_cast<UINTPTR*>(ptr), opx_ret_address_hidden, opx_frm_address_hidden, size)

#ifdef __cplusplus
extern "C"
#endif
void OpCopyCallStack(UINTPTR* dst, void* pc, void* fp, int size);


/* used by modules/olddebug/tstdump.cpp */
#ifndef DEBUG_OUTPUTDIRECTORY
#define DEBUG_OUTPUTDIRECTORY	""
#endif


#ifndef NS4P_COMPONENT_PLUGINS
#define RESET_OPENGL_CONTEXT OpenGLContextWrapper::SetSharedContextAsCurrent();
#else
#define RESET_OPENGL_CONTEXT
#endif

/* Cursor support */
#ifdef __cplusplus
#include "platforms/mac/subclasses/cursor_mac.h"
#endif

/* These four lines of code are needed (used by posix), does not however compile for 64-bit... */
size_t mac_wcstombs(char *, const wchar_t *, size_t);
size_t mac_mbstowcs(wchar_t *, const char *, size_t);

#ifdef __cplusplus
class OpThreadTools;
extern OpThreadTools* g_thread_tools;
extern "C" {
#endif
int stricmp(const char*, const char*);
int strnicmp(const char*, const char*, size_t);
#ifdef __cplusplus
}
#endif

/* Behave like Linux: On FreeBSD systems, setjmp sets and restores signal mask, on Linux it doesn't
 * _setjmp on FreeBSD behaves as setjmp on Linux, i.e. it doesn't restore signal mask.
 * hopefully this should be a bit faster.
 */
#define setjmp _setjmp
#define longjmp _longjmp

/* Needed by filefun */
BOOL MacPlaySound(const uni_char* fullfilepath, BOOL async);

/* SQLITE3 can do a magic locking stuff specific to Mac OS X..
 * Problem is - only since 10.6 on... we're linking against 10.4,
 * so we have to disable it. At least for now */
#define SQLITE_ENABLE_LOCKING_STYLE 0

#endif // !MACINTOSH_SYSTEM_H
