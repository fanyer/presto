/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1999-2006
 *
 * Stub core/pch.h for standalone builds of the ECMAScript engine.
 *
 * Tested on MS Windows with Visual Studio 6 and on MacOS X (Intel only) with Xcode 2.3.
 */

// The project must define one of:
//    ECMASCRIPT_STANDALONE_COMPILER for compilejs
//    Nothing special for jsshell
//
// The project should define _DEBUG in debug configurations.
#ifdef MSWIN
# define NOMINMAX
# include <windows.h>
// Undefine some name clashes
# undef Yield
# undef NO_ERROR
# undef REG_NONE
#endif
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <ctype.h>
#include <float.h>
#include <time.h>
#include <stddef.h>
#include <stdarg.h>
#include <math.h>
#if defined _MACINTOSH_ || defined UNIX
#  include <sys/time.h>
#endif
#ifdef UNIX
#  include "core/unix_system.h"
#endif

#define _STANDALONE
#define STDLIB_DTOA_CONVERSION
#define DTOA_FLOITSCH_DOUBLE_CONVERSION
#define EBERHARD_MATTES_PRINTF
#define STDLIB_64BIT_STRING_CONVERSIONS
#define HAVE_MALLOC
#define HAVE_TOLOWER
#define HAVE_TOUPPER
#define USE_UNICODE_LINEBREAK
#define USE_UNICODE_HANGUL_SYLLABLE
#define USE_UNICODE_SEGMENTATION
#define USE_UNICODE_INC_DATA
//#ifndef UNIX
//# define USE_CXX_EXCEPTIONS
//#endif

#define OP_EXTERN_C extern "C"

#include "modules/encodings/encodings-capabilities.h"
#include "modules/memory/memory_capabilities.h"

#define MEMORY_EXTERNAL_DECLARATIONS "modules/memory/src/memory_macros.h"
#define MEMORY_GROPOOL_ALLOCATOR
#define MEMORY_ALIGNMENT DEFAULT_MEMORY_ALIGNMENT
#include "modules/memory/memory_fundamentals.h"

#if !defined IEEE_8087 && !defined IEEE_MC68k
# define NAN_EQUALS_EVERYTHING
# define IEEE_8087
# define IEEE_DENORMALS
#endif

#define NAN_ARITHMETIC_OK
#define QUIET_NAN_IS_ONE

#define HAVE_FMOD
#define HAVE_POW
#define HAVE_SQRT
#define HAVE_EXP
#define HAVE_LOG
#define HAVE_SIN
#define HAVE_COS
#define HAVE_TAN
#define HAVE_ASIN
#define HAVE_ACOS
#define HAVE_ATAN
#define HAVE_ATAN2
#define HAVE_TIME

#define op_fmod  fmod
#define op_pow(x,y)   pow(static_cast<double>(x),static_cast<double>(y))
#define op_sqrt  sqrt
#define op_exp   exp
#define op_log   log
#define op_sin   sin
#define op_cos   cos
#define op_tan   tan
#define op_asin  asin
#define op_acos  acos
#define op_atan  atan
#define op_atan2 atan2

#ifdef MSWIN
  typedef wchar_t uni_char;
# define HAVE_TIME
# define HAVE_CLOCK
# define HAVE_UINT64
# define HAVE_INT64
#elif _MACHINTOSH_
  typedef unsigned short uni_char;
#endif
typedef int OP_STATUS;
typedef int OP_BOOLEAN;
typedef int BOOL;

#define OP_MEMORY_VAR

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef LOCAL_ARRAY_SIZE
#define LOCAL_ARRAY_SIZE(x) ARRAY_SIZE(x)
#endif

#ifndef TRUE
#  define TRUE 1
#endif
#ifndef FALSE
#  define FALSE 0
#endif

#ifndef OP_DELETE
#define OP_DELETE(ptr) delete ptr
#endif

#ifndef OP_DELETEA
#define OP_DELETEA(ptr) delete[] ptr
#endif

#include <new>

#undef OP_NEW
#define OP_NEW(obj, args) new (std::nothrow) obj args

#undef OP_NEWA
#define OP_NEWA(obj, count) new (std::nothrow) obj[count]

#ifndef OP_NEW_L
#define OP_NEW_L(obj, args) new (ELeave) obj args
#endif

#ifndef OP_NEWA_L
#define OP_NEWA_L(obj, count) new (ELeave) obj[count]
#endif

enum BOOL3
{
	MAYBE=-1,
	NO=0,
	YES=1
};
void *operator new(size_t, TLeave);
#if defined USE_CXX_EXCEPTIONS && defined _DEBUG
	void operator delete( void *loc, TLeave );
#endif
#ifdef _MSC_VER
#if !defined _DEBUG
#  pragma warning( disable: 4291 ) // "no matching operator delete found""
#endif
#pragma warning(disable:4244) // "conversion from '__w64 int' to 'unsigned int', possible loss of data"
#pragma warning(disable:4267) // "conversion from 'size_t' to 'unsigned int', possible loss of data"
#pragma warning(disable:4311) // "pointer truncation from 'ES_Runtime *const ' to 'unsigned long'"
#pragma warning(disable:4312) // "conversion from 'INT32' to 'void *' of greater size"
#pragma warning(disable:4996) // "This function or variable may be unsafe."
#pragma warning(disable:4345) // "behavior change: an object of POD type constructed with an initializer of the form () will be default-initialized"
#endif

#ifndef _MSC_VER
void *operator new[] (size_t, TLeave);
#endif

#  ifdef __GNUC__
#    if __GNUC__ >= 3
#      if __GNUC__ > 3 || __GNUC_MINOR__ > 0 // 3.0 doesn't like deprecated
#        define DEPRECATED(X) X __attribute__((__deprecated__))
#      endif
#      define PURE_FUNCTION   __attribute__((__pure__))
#    endif
#  elif defined(_MSC_VER)
#    if _MSC_VER >= 1400
#      define DEPRECATED(X) __declspec(deprecated) X
#    endif
#  endif

#ifndef DEPRECATED
# define DEPRECATED(X) X
#endif
#ifndef PURE_FUNCTION
# define PURE_FUNCTION
#endif

class OpStatus
{
public:
	enum {
		IS_TRUE=2,
		IS_FALSE=1,
		OK=0,
		ERR=-1,
		ERR_NO_MEMORY=-2,
		ERR_NOT_SUPPORTED=-3,
		ERR_FILE_NOT_FOUND=-7,
		USER_ERROR=-4000
	};
	static void Ignore(int e) {}
	static BOOL IsError(int e) { return e < 0; }
	static BOOL IsSuccess(int e) { return e >= 0; }
	static BOOL IsMemoryError(int e) { return e == ERR_NO_MEMORY; }
};

#define OpBoolean OpStatus

typedef unsigned int UINT32;
typedef unsigned short UINT16;
typedef unsigned char UINT8;
#ifndef UNIX
// Note: we'll have to change this in 64bit non-Linux environment:
typedef unsigned int UINTPTR;
typedef   signed int  INTPTR; // referenced (but undefined) in OpHashtable.cpp
#endif

#ifndef MSWIN
typedef int INT32;
#endif

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
extern "C" void Debug_OpAssert(const char* expr, const char* file, int line);

#ifdef DEBUG_ENABLE_OPASSERT
#define OP_ASSERT(expr) do { \
	if (!(expr)) Debug_OpAssert(#expr, __FILE__, __LINE__); } while (0)
#define OP_STATIC_ASSERT(expr)                       \
	do {                                             \
		int op_static_assert_dummy[(expr) ? 1 : -1]; \
		(void)op_static_assert_dummy;                \
	} while(0)
#else // DEBUG_ENABLE_OPASSERT
#  define OP_ASSERT(x)  ((void)0)
#  define OP_STATIC_ASSERT(expr) ((void)0)
#endif // DEBUG_ENABLE_OPASSERT

#include "modules/util/opautoptr.h"
#include "modules/util/excepts.h"

#define OpStackAutoPtr OpAutoPtr

#ifndef LEAVE
# define LEAVE(x)  User::Leave(x)
#endif

#ifdef MSWIN
# define UNI_L(x)  L ## x
#elif !defined WCHAR_IS_UNICHAR
# define UNI_L_IS_FUNCTION
extern const uni_char* UNI_L(const char* s);
#endif

class TempBuffer;

OP_STATUS read_file( const char *filename, TempBuffer *t );
int uni_snprintf( uni_char *buffer, size_t size, const uni_char *fmt, ... );
int uni_str_eq( const uni_char *, const char * );
int uni_strni_eq( const uni_char *, const char *, size_t);
int uni_atoi( const uni_char * );
size_t uni_strlen( const uni_char * );
void Char2Unichar( const char *, int, uni_char *, int );
uni_char *MakeDoubleByte(uni_char * buf, int len);
int uni_strcmp(const uni_char * str1, const uni_char * str2);
int uni_strncmp(const uni_char * str1, const uni_char * str2, size_t n);
int uni_strnicmp(const uni_char *, const uni_char *, size_t n);
uni_char *uni_strcpy(uni_char * dest, const uni_char * src);
uni_char *uni_strncpy(uni_char * dest, const uni_char * src, size_t n);
uni_char *uni_strchr(const uni_char *, uni_char);
uni_char *uni_strdup(const uni_char *);
uni_char *uni_buflwr(uni_char* str, size_t nchars);
uni_char *uni_bufupr(uni_char* str, size_t nchars);

#include "modules/unicode/unicode.h"
#define op_lowlevel_malloc malloc
#define op_lowlevel_free free
#define op_tolower  uni_tolower
#define op_toupper  uni_toupper
#ifndef UNIX
#define op_clock    clock
#endif
#define op_time     time
#define op_tan		tan
#define op_sqrt		sqrt
#define op_sin		sin
#define op_log		log
#define op_exp		exp
#define op_cos		cos
#define op_atan2	atan2
#define op_atan		atan
#define op_asin		asin
#define op_acos		acos
#define op_pow(x,y)   pow(static_cast<double>(x),static_cast<double>(y))

#include "modules/stdlib/stdlib_module.h"
#include "modules/unicode/unicode_module.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpTimeInfo.h"

class Opera
{
public:
	~Opera();

	void InitL();
	void Destroy();

#ifdef STDLIB_MODULE_REQUIRED
	StdlibModule stdlib_module;
#endif // STDLIB_MODULE_REQUIRED
#ifdef UNICODE_MODULE_REQUIRED
	UnicodeModule unicode_module;
#endif // UNICODE_MODULE_REQUIRED
};

extern Opera *g_opera;
extern OpSystemInfo *g_op_system_info;
extern OpTimeInfo *g_op_time_info;

#include "modules/stdlib/include/opera_stdlib.h"

extern OpSystemInfo *g_op_system_info;

// tweaks

#define ES_PARM_SMALLEST_HEAP_SIZE  (256*1024)
#define ES_PARM_SMALLEST_GROWTH_BETWEEN_GCS (32*1024)
#define ES_PARM_LARGEST_GROWTH_BETWEEN_GCS (1024*1024*1024)
#define ES_PARM_INVERSE_LOAD_FACTOR 4.0
#define ES_PARM_INVERSE_OFFLINE_LOAD_FACTOR 2.0
#define ES_PARM_MAINTENANCE_GC_HEAP_INACTIVE_TIME 10000
#define ES_PARM_MAINTENANCE_GC_SINCE_LAST_GC 10000

#define ES_MINIMUM_STACK_REMAINING (3 * 1024)

#define REGEXP_PARM_STORE_SEG_STORAGE_SIZE 2000
#define REGEXP_PARM_CAPTURE_SEG_STORAGE_SIZE 32
#define REGEXP_PARM_CAPTURE_SEG_STORAGE_LOAD 1.2
#define REGEXP_PARM_FRAME_SEGMENT_SIZE 50

#define op_yield() ((void)0)

class URL
{
public:
    const char *string;
};
