/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * This file is used as PRODUCT_SYSTEM_FILE by Desktop on Unix.
 */
#ifndef UNIX_SYSTEM_H
#define UNIX_SYSTEM_H __FILE__
#include "platforms/posix/sys/include_early.h"
#include <assert.h>
#include <sys/types.h>
#include "platforms/unix/base/common/preamble.h"
#include "platforms/posix/sys/posix_choice.h"
#include "adjunct/quick/quick-version.h"

#define NATIVE_BOOL

#ifndef BYTE_ORDER
# if defined(_BYTE_ORDER)
#  define BYTE_ORDER _BYTE_ORDER
# elif defined(__BYTE_ORDER)
#  define BYTE_ORDER __BYTE_ORDER
# elif defined(__BYTE_ORDER__)
#  define BYTE_ORDER __BYTE_ORDER__
# else
#  error Undefined BYTE_ORDER
# endif
#endif

#ifndef BIG_ENDIAN
# if defined(_BIG_ENDIAN)
#  define BIG_ENDIAN _BIG_ENDIAN
# elif defined(__BIG_ENDIAN)
#  define BIG_ENDIAN __BIG_ENDIAN
# elif defined(__BIG_ENDIAN__)
#  define BIG_ENDIAN __BIG_ENDIAN__
# else
#  error Undefined BIG_ENDIAN
# endif
#endif

#ifndef LITTLE_ENDIAN
# if defined(_LITTLE_ENDIAN)
#  define LITTLE_ENDIAN _LITTLE_ENDIAN
# elif defined(__LITTLE_ENDIAN)
#  define LITTLE_ENDIAN __LITTLE_ENDIAN
# elif defined(__LITTLE_ENDIAN__)
#  define LITTLE_ENDIAN __LITTLE_ENDIAN__
# else
#  error Undefined LITTLE_ENDIAN
# endif
#endif

#ifndef FLOAT_WORD_ORDER
# if defined(_FLOAT_WORD_ORDER)
#  define FLOAT_WORD_ORDER _FLOAT_WORD_ORDER
# elif defined(__FLOAT_WORD_ORDER)
#  define FLOAT_WORD_ORDER __FLOAT_WORD_ORDER
# elif defined(__FLOAT_WORD_ORDER__)
#  define FLOAT_WORD_ORDER __FLOAT_WORD_ORDER__
# else
#  define FLOAT_WORD_ORDER BYTE_ORDER
# endif
#endif

#if BYTE_ORDER == BIG_ENDIAN
#define SYSTEM_BIG_ENDIAN				YES
#elif BYTE_ORDER == LITTLE_ENDIAN
#define SYSTEM_BIG_ENDIAN				NO
#else
#error Unsupported BYTE_ORDER
#endif

#if FLOAT_WORD_ORDER == BIG_ENDIAN
#define SYSTEM_IEEE_8087				NO
#define SYSTEM_IEEE_MC68K				YES
#elif FLOAT_WORD_ORDER == LITTLE_ENDIAN
#define SYSTEM_IEEE_8087				YES
#define SYSTEM_IEEE_MC68K				NO
#else
#error Unsupported FLOAT_WORD_ORDER
#endif

#define SYSTEM_NAMESPACE				YES
#undef SYSTEM_STRTOD /* does not conform to specification, should be locale-independent */
#define SYSTEM_STRTOD					NO
#undef SYSTEM_GETPHYSMEM
#define SYSTEM_GETPHYSMEM				YES	/* SYS_GETPHYSMEM */
#undef SYSTEM_OPFILELENGTH_IS_SIGNED
#define SYSTEM_OPFILELENGTH_IS_SIGNED	NO /* core has issues with this, assumes OpFileLength is unsigned - see DSK-295495 */
#undef SYSTEM_VSNPRINTF
#define SYSTEM_VSNPRINTF				NO  /* preventing locale settings to make PS printing output commas in PS code */
#undef SYSTEM_LOCAL_TYPE_AS_TEMPLATE_ARG 
#define SYSTEM_LOCAL_TYPE_AS_TEMPLATE_ARG	NO

#include "platforms/posix/sys/posix_declare.h" /* *after* fixing up SYSTEM_* defines. */

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
#define IGNORE_SZ_PARAM	NULL
#define __export

#ifndef NEWLINE
# define NEWLINE "\n"
#endif /* NEWLINE */

#define USED_IN_BITFIELD /* Enums are unsigned by default in gcc */
#define PATH_CASE_INSENSITIVE FALSE

/* we use non-signal-mask saving versions for performance reasons;
 * signal masks are assumed to not change inside leaving funcions */
#undef op_setjmp
#undef op_longjmp
#define op_setjmp(e)			_setjmp(e)
#define op_longjmp(e, v)		_longjmp(e, v)

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

#ifdef __cplusplus
extern __thread class OpComponentManager* g_component_manager;
extern class CoreComponent* g_opera;
extern class OpThreadTools* g_thread_tools;
extern __thread class CleanupItem* g_cleanup_stack;
#endif // __cplusplus

#endif /* !UNIX_SYSTEM_H */
