/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef WINDOWS_SYSTEM_H
#define WINDOWS_SYSTEM_H

// Defines to make it more readable when we turn 
// on/off code for different VS versions.
#define VS2012 1700 // VC++ 11.0 (2012)
#define VS2010 1600 // VC++ 10.0 (2010)
#define VS2008 1500 // VC++  9.0 (2008)
#define VS2005 1400 // VC++  8.0 (2005)
#define VS2003 1310 // VC++  7.1 (2003)
#define VS2002 1300 // VC++  7.0 (2002)
#define VS6    1200 // VC++  6.0 (1998)

// NTDDI version constants
#define NTDDI_WIN6                          0x06000000
#define NTDDI_WIN6SP1                       0x06000100
#define NTDDI_WIN6SP2                       0x06000200
#define NTDDI_WIN6SP3                       0x06000300
#define NTDDI_WIN6SP4                       0x06000400

#define NTDDI_VISTA                         NTDDI_WIN6   
#define NTDDI_VISTASP1                      NTDDI_WIN6SP1
#define NTDDI_VISTASP2                      NTDDI_WIN6SP2
#define NTDDI_VISTASP3                      NTDDI_WIN6SP3
#define NTDDI_VISTASP4                      NTDDI_WIN6SP4

#define NTDDI_LONGHORN						NTDDI_VISTA
					    
#define NTDDI_WS08                          NTDDI_WIN6SP1
#define NTDDI_WS08SP2                       NTDDI_WIN6SP2
#define NTDDI_WS08SP3                       NTDDI_WIN6SP3
#define NTDDI_WS08SP4                       NTDDI_WIN6SP4

#define NTDDI_WIN7                          0x06010000

// Let's use special code for VC++ 8.0 (2005) and higher
#if _MSC_VER >= VS2005
#define WIN_USE_STDLIB_64BIT
#ifndef _WIN64
#define _USE_32BIT_TIME_T	// see http://securityvulns.com/advisories/year3000.asp
#endif // _WIN64
#endif

#include <stdint.h>

#if defined(DEBUG_ALLOCATIONS) && defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif // _DEBUG

#ifndef _WINRESRC_
# ifndef _WIN32_IE
#  define _WIN32_IE 0x0600
# else
#  if (_WIN32_IE < 0x0400) && defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0500)
#   error _WIN32_IE setting conflicts with _WIN32_WINNT setting
#  endif
# endif // _WIN32_IE
#endif // _WINRESRC_

#ifndef XP_WIN
# define XP_WIN
#endif // !XP_WIN


//
// Disabled warnings
//


// 'identifier' : unreferenced formal parameter
#pragma warning (disable:4100)

// 'symbol' : alignment of a member was sensitive to packing
#pragma warning (disable:4121)

// conditional expression is constant
#pragma warning (disable:4127)

// 'conversion' conversion from 'type1' to 'type2', possible loss of data
#pragma warning (disable:4244)

#ifdef _WIN64
// 	'initializing' : conversion from 'size_t' to 'unsigned int', possible loss of data
#pragma warning(disable : 4267)
#endif // _WIN64

// 'function' : inconsistent DLL linkage
#pragma warning (disable:4273)

// behavior change: an object of POD type constructed with an initializer of the form () will be default-initialized
#pragma warning (disable:4345)

// 'this' : used in base member initializer list
#pragma warning (disable:4355)

// Nonstandard extension used: 'function' uses SEH and 'object' has destructor
#pragma warning (disable:4509)

// 'class' : assignment operator could not be generated
#pragma warning (disable:4512)

// interaction between 'function' and C++ object destruction is non-portable
#pragma warning (disable:4611)

// assignment within conditional expression
#pragma warning (disable:4706)


//
// Warnings turned into errors
// These warnings are either critical,
// or just warnings that are easy to fix, but noone wants to do it unless being forced (DSK-369100).
// Please keep them ordered by the warning number to avoid duplicates in the future.
//

// signed/unsigned mismatch with operators (level 3)
#pragma warning( error : 4018 )

// case is not a valid value for switch of enum
#pragma warning( error : 4063 )

// switch statement contains 'default' but no 'case' labels
#pragma warning( error : 4065 )

// Make sure "Deletion of incomplete type, Destructor not called" is treated as an error.
#pragma warning( error : 4150 )

// only enable in "Debug", because some variables are used by OP_ASSERT(), which is excluded in "Release"
#ifdef _DEBUG
// local variable is initialized but not referenced
#pragma warning( error : 4189 )
#endif // _DEBUG

// nonstandard extension used : conversion from 'type1' to 'type2'
#pragma warning( error : 4239 )

// signed/unsigned mismatch : conversion from 'type1' to 'type2'
#pragma warning( error : 4245 )

// cast truncates constant value
#pragma warning( error : 4310 )

// pointer truncation (64-bit unsafe) - Use when compiling for 64-bit to catch truncated pointers
#pragma warning( error : 4311 )

// You attempted to assign a 32-bit value to a 64-bit type. For example, casting a 32-bit int or 32-bit long to a 64-bit pointer.
#pragma warning( error : 4312 )

// signed/unsigned mismatch with operators (level 4)
#pragma warning( error : 4389 )

// Not all control paths return a value.
#pragma warning( error : 4715 )

// forcing value to bool 'true' or 'false' (performance warning)
#pragma warning( error : 4800 )


#ifdef _DEBUG
// Turn on debug info for DX9 back-end.
// http://msdn.microsoft.com/en-us/library/bb173355.aspx
// Runtime warnings/errors can be turned on with dxcpl.exe from the DirectX SDK
// -- NoteMe
# define D3D_DEBUG_INFO
#endif

#define SYSTEM_BIG_ENDIAN							NO
#define SYSTEM_IEEE_8087							YES
#define SYSTEM_IEEE_MC68K							NO
#define SYSTEM_INT64								YES
#define SYSTEM_INT64_LITERAL						YES
#define SYSTEM_STRICMP								NO
#define SYSTEM_STRTOD								NO
#define SYSTEM_STRCASE								NO
#define SYSYEM_STRTOD								NO
#define SYSTEM_ITOA									YES
#define SYSTEM_NAN_ARITHMETIC_OK					YES
#define SYSTEM_NAN_EQUALS_EVERYTHING				NO
#define SYSTEM_QUIET_NAN_IS_ONE						YES
#define SYSTEM_LOCALECONV							YES
#define SYSTEM_LONGLONG								YES
#define SYSTEM_RECT                                 YES
#define SYSTEM_POINT                                YES
#define SYSTEM_COLORREF                             YES
#define SYSTEM_SIZE                                 YES
#define SYSTEM_MAKELONG                             YES
#define SYSTEM_RGB                                  YES
#define SYSTEM_ALLOCA								YES
#define SYSTEM_DTOA									YES
#define SYSTEM_ECVT									YES
#define SYSTEM_ETOA									NO
#define SYSTEM_FCVT									YES
#define SYSTEM_GCVT									YES
#define SYSTEM_LONGJMP_VOLATILE_LOCALS				NO
#define SYSTEM_COMPLEX_GLOBALS						YES
#define SYSTEM_DLL_NAME_LOOKUP						YES
#define SYSTEM_GLOBALS								YES
#define SYSTEM_SETJMP								YES
#define SYSTEM_BLOCK_CON_FILE						YES
#define SYSTEM_DRIVES								YES
#define SYSTEM_MULTIPLE_DRIVES						YES
#define SYSTEM_NETWORK_BACKSLASH_PATH				YES
#define SYSTEM_OWN_OP_STATUS_VALUES					NO
#define SYSTEM_IEEE_DENORMALS						YES
#define SYSTEM_GETPHYSMEM							YES
#define SYSTEM_UINT64								YES
#define SYSTEM_UINT64_LITERAL						YES

#define SYSTEM_ABS									YES
#define SYSTEM_ACOS									YES
#define SYSTEM_ASIN									YES
#define SYSTEM_ATAN									YES
#define SYSTEM_ATAN2								YES
#define SYSTEM_ATOI									YES
#define SYSTEM_BSEARCH								YES
#define SYSTEM_CEIL									YES
#define SYSTEM_COS									YES
#define SYSTEM_EXP									YES
#define SYSTEM_FABS									YES
#define SYSTEM_FLOOR								YES
#define SYSTEM_FMOD									YES
#define SYSTEM_FREXP								NO
#define SYSTEM_GMTIME								YES
#define SYSTEM_LDEXP								YES
#define SYSTEM_LEAVE_WITH_JMP						YES
#define SYSTEM_LOCALTIME							YES
#define SYSTEM_LOG									YES
#define SYSTEM_LTOA									NO
#define SYSTEM_MALLOC								YES
#define SYSTEM_MEMCHR								YES
#define SYSTEM_MEMCMP								YES
#define SYSTEM_MEMCPY								YES
#define SYSTEM_MEMMOVE								YES
#define SYSTEM_MEMSET								YES
#define SYSTEM_MKTIME								YES
#define SYSTEM_MODF									YES
#define SYSTEM_POW									YES
#define SYSTEM_QSORT								YES
#define SYSTEM_QSORT_S								YES
#define SYSTEM_RAND									YES
#define SYSTEM_SETLOCALE							YES
#define SYSTEM_SIN									YES
#define SYSTEM_SNPRINTF								YES
#define SYSTEM_SPRINTF								YES
#define SYSTEM_SQRT									YES
#define SYSTEM_SSCANF								YES
#define SYSTEM_STRCAT								YES
#define SYSTEM_STRCHR								YES
#define SYSTEM_STRCMP								NO
#define SYSTEM_STRCPY								YES
#define SYSTEM_STRCSPN								YES
#define SYSTEM_STRDUP								YES
#define SYSTEM_STRLCAT								NO
#define SYSTEM_STRLCPY								NO
#define SYSTEM_STRLEN								YES
#define SYSTEM_STRNCAT								YES
#define SYSTEM_STRNCMP								NO
#define SYSTEM_STRNCPY								YES
#define SYSTEM_STRNICMP								NO
#define SYSTEM_STRPBRK								YES
#define SYSTEM_STRRCHR								YES
#define SYSTEM_STRREV								YES
#define SYSTEM_STRSPN								YES
#define SYSTEM_STRSTR								YES
#define SYSTEM_STRTOL								YES
#define SYSTEM_STRTOUL								YES
#define SYSTEM_TAN									YES
#define SYSTEM_TIME									YES
#define SYSTEM_VSNPRINTF							YES
#define SYSTEM_VSPRINTF								YES
#define SYSTEM_VSSCANF								NO
#define SYSTEM_YIELD								NO
#define SYSTEM_RISC_ALIGNMENT						NO

#define SYSTEM_UNI_ITOA								YES
#define SYSTEM_UNI_LTOA								YES
#define SYSTEM_UNI_MEMCHR							YES
#define SYSTEM_UNI_SNPRINTF							NO
#define SYSTEM_UNI_SPRINTF							YES
#define SYSTEM_UNI_SSCANF							YES
#define SYSTEM_UNI_STRCAT							YES
#define SYSTEM_UNI_STRCHR							YES
#define SYSTEM_UNI_STRCMP							NO
#define SYSTEM_UNI_STRCPY							YES
#define SYSTEM_UNI_STRCSPN							YES
#define SYSTEM_UNI_STRDUP							YES
#define SYSTEM_UNI_STRICMP							NO
#define SYSTEM_UNI_STRISTR							NO
#define SYSTEM_UNI_STRLCAT							NO
#define SYSTEM_UNI_STRLCPY							NO
#define SYSTEM_UNI_STRLEN							YES
#define SYSTEM_UNI_STRNCAT							YES
#define SYSTEM_UNI_STRNCMP							NO
#define SYSTEM_UNI_STRNCPY							YES
#define SYSTEM_UNI_STRNICMP							NO
#define SYSTEM_UNI_STRPBRK							YES
#define SYSTEM_UNI_STRRCHR							YES
#define SYSTEM_UNI_STRREV							YES
#define SYSTEM_UNI_STRSPN							YES
#define SYSTEM_UNI_STRSTR							YES
#define SYSTEM_UNI_STRTOK							YES
#define SYSTEM_UNI_VSNPRINTF						NO
#define SYSTEM_UNI_VSPRINTF							YES
#define SYSTEM_MEMMEM								NO
#define SYSTEM_HTONL								YES
#define SYSTEM_HTONS								YES
#define SYSTEM_NTOHL								YES
#define SYSTEM_NTOHS								YES
#define SYSTEM_GUID									YES
#define SYSTEM_FORCE_INLINE							YES
#define SYSTEM_ARCHITECTURE_ARM						NO
#define SYSTEM_ARCHITECTURE_IA32					YES
#define SYSTEM_ARCHITECTURE_MIPS					NO
#define SYSTEM_ARCHITECTURE_POWERPC					NO
#define SYSTEM_ARCHITECTURE_SUPERH					NO
#define SYSTEM_CAN_DELETE_OPEN_FILE					NO
#define SYSTEM_GETENV								YES
#define SYSTEM_PUTENV								YES
#define SYSTEM_OPFILELENGTH_IS_SIGNED				NO
#define SYSTEM_ERRNO								YES
#define SYSTEM_NAMESPACE							YES
#define SYSTEM_TEMPLATE_ARG_DEDUCTION				YES
#define SYSTEM_LOCAL_TYPE_AS_TEMPLATE_ARG			YES
#define SYSTEM_NEWA_CHECKS_OVERFLOW					YES
#define SYSTEM_CONSTANT_DATA_IS_EXECUTABLE			NO
#define SYSTEM_NOTHROW								NO

#ifdef _WIN64
// temporary, should be SYSTEM_ARCHITECTURE_*
#define ARCHITECTURE_AMD64_WINDOWS
#define ARCHITECTURE_AMD64
#endif // _WIN64

// 
// WIN_NT Compiler define used to select winsock2.h in windows.h. This
// enables NT only API:s that we must take care not to use.
//
#ifndef _WIN32_WINNT
# define _WIN32_WINNT 0x501
# define INCL_WINSOCK_API_TYPEDEFS 1
#endif

#include <string.h>
#include <tchar.h>
#include <wchar.h>
#include <sys/types.h>
#include <sys/stat.h>

#define stat _stat

// Windows.h includes winsock.h and we need winsock2.h. Or something.
// It won't compile in Visual Studio.NET if not winsock2.h is included before windows.h anyway.
#include <winsock2.h>
#include <windows.h>

#include <direct.h>		// Library functions related to directory handling and creation
#include <time.h>		// Declarations of time routines and defines the structure returned by the localtime and gmtime routines and used by asctime.
#include <shlobj.h>
#include <shellapi.h>

#include "adjunct/quick/quick-version.h"
#include "platforms/windows/windows_ui/res/opera.h"
#include "platforms/windows/shlwapi_replace.h"

#ifdef __cplusplus
# include "platforms/windows_common/utils/OpAutoHandle.h"
# include "platforms/windows/utils/shared.h"
# include "platforms/windows/utils/proc_loader_headers.h"
#endif

#undef COMPILE_MULTIMON_STUBS
#include <multimon.h>	// Get back default values from GetSystemMetrics() for new metrics, and fakes multiple monitor apis on Win32 OSes without them.

#include <setjmp.h>
#include <stdio.h>
#include <float.h>
#include <math.h>
#include <limits.h>
#include <stddef.h>
#include <locale.h>
#include <errno.h>
#include <assert.h>

#ifdef __cplusplus
# include <new>
#endif

typedef BOOL WINBOOL;
typedef unsigned __int64 UINT64;
typedef unsigned int UINT32;
typedef unsigned short UINT16;
typedef unsigned char UINT8;
typedef __int64 INT64;
typedef int INT32;
typedef short INT16;
typedef signed char INT8;

typedef UINT32 uint32;
typedef UINT16 uint16;
typedef UINT8 uint8;
typedef INT32 int32;
typedef INT16 int16;
typedef INT8 int8;

typedef unsigned int UINT;

typedef intptr_t INTPTR;
typedef uintptr_t UINTPTR;

typedef wchar_t uni_char;
#define _UNI_L_FORCED_WIDE(s)			L ## s
#define UNI_L(s)						_UNI_L_FORCED_WIDE(s)	//	both needed

#define OP_INT64(x)						x ## i64
#define OP_UINT64(x)					x ## ui64

#ifndef MIN
# define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif // !MIN

#ifndef MAX
# define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif // !MAX

#ifndef LODWORD
#define LODWORD(_qw)    ((DWORD)(_qw))
#endif // !LODWORD

#define PATH_CASE_INSENSITIVE	TRUE
#define PATHSEP                 "\\"
#define PATHSEPCHAR             '\\'
#define CLASSPATHSEP            ";"
#define CLASSPATHSEPCHAR        ';'
#define SYS_CAP_WILDCARD_SEARCH "*.*"

#ifndef NEWLINE
# define NEWLINE "\r\n"
#endif 

// For URL module
#define SYS_CAP_FILENAMES_IN_SYSTEM_CODEPAGE

typedef unsigned __int64 OpFileLength;

#define FILE_LENGTH_NONE _UI64_MAX
#define FILE_LENGTH_MAX _UI64_MAX

#define vsnprintf _vsnprintf
#define snprintf _snprintf

int mswin_snprintf(char *, size_t, const char *, ...); //MSVC differs from ANSI C, so we need wrappers
int mswin_vsnprintf(char *, size_t, const char *, va_list); //MSVC differs from ANSI C, so we need wrappers
char* mswin_setlocale(int category, const char *locale); //setlocale in windows causes registry calls when requesting the environment locale. We want to cache that.
void mswin_clear_locale_cache();

#define SUBROUTINE(x)
#define USED_IN_BITFIELD :unsigned int

#define IO
#define IN
#define OUT
#define OPT
#define BLD_CALLBACK CALLBACK
#define __export
#define huge

#define uni_tempnam _wtempnam

// Threads
typedef HANDLE OpThreadId;
typedef CRITICAL_SECTION OpMutexId;
typedef HANDLE OpSemaphoreId;

#define op_lowlevel_malloc malloc
#define op_lowlevel_free free
#define op_lowlevel_calloc calloc
#define op_lowlevel_realloc realloc
#define op_setlocale mswin_setlocale
#define op_getenv getenv
#define op_putenv putenv
#define op_strlen strlen
#define op_memmove memmove
#define op_memset memset
#define op_memcpy memcpy
#define op_memcmp memcmp
#define op_memchr memchr
#define op_strcpy strcpy
#define op_strncpy strncpy
#define op_strcat strcat
#define op_strncat strncat
//#define op_strcmp strcmp
//#define op_strncmp strncmp
//#define op_stricmp stricmp
#define op_stristr stristr
//#define op_strnicmp strnicmp
#define op_strchr (char *)strchr
#define op_strrchr strrchr
#define op_strtol strtol
#define op_strtoul strtoul
#define op_strrev strrev
#define op_atoi atoi
#define op_itoa itoa
#define op_abs(x) abs(static_cast<int>(x))
#define op_floor floor
#define op_ceil ceil
#define op_modf modf
#define op_offsetof offsetof
#define op_fmod fmod
#define op_ldexp ldexp

#ifdef __cplusplus
  inline double op_dummy_fabs(double x) { return ::abs(x); }
# define op_fabs(x) op_dummy_fabs(static_cast<double>(x))
#else
#  define op_fabs(x) abs(static_cast<double>(x))
#endif // __cplusplus

#define op_pow(x, y) pow(static_cast<double>(x), static_cast<double>(y))
#define op_exp exp
#define op_log log
#define op_sqrt(x) sqrt((double)(x))
#define op_sin sin
#define op_cos cos
#define op_tan tan
#define op_asin asin
#define op_acos acos
#define op_atan atan
#define op_atan2 atan2
#define op_srand srand
#define op_rand rand
//#define op_isdigit isdigit
//#define op_isxdigit isxdigit
//#define op_isspace isspace
//#define op_isupper isupper
//#define op_isalnum isalnum
//#define op_isalpha isalpha
//#define op_isprint isprint
//#define op_iscntrl iscntrl
//#define op_ispunct ispunct
#define op_sprintf sprintf
#define op_snprintf mswin_snprintf //In case buffer is too small, op_snprintf must (according to ANSI C) return how large the buffer needs to be (not including \0)
#define op_vsprintf vsprintf
#define op_vsnprintf mswin_vsnprintf //In case buffer is too small, op_vsnprintf must (according to ANSI C) return how large the buffer needs to be (not including \0)
#define op_sscanf sscanf
#define op_strstr(x, y)  const_cast<char*>(strstr(x, y))
#define op_strspn strspn
#define op_strcspn strcspn
#define op_strpbrk strpbrk
#define op_lowlevel_strdup strdup
#define op_time time
#define op_localtime localtime
#define op_gmtime gmtime
#define op_mktime mktime
#define op_qsort qsort
#define op_qsort_s qsort_s
#define op_bsearch bsearch
#define op_htonl htonl
#define op_htons htons
#define op_ntohl ntohl
#define op_ntohs ntohs
#define op_setjmp setjmp
#define op_longjmp longjmp

#define uni_itoa _itow
#define uni_ltoa _ltow
#define uni_memchr wmemchr
#define uni_sprintf swprintf
#define uni_sscanf swscanf
#define uni_strcat wcscat
#define uni_strchr (uni_char *)wcschr
//#define real_uni_strcmp wcscmp
#define uni_strcpy wcscpy
#define uni_strcspn wcscspn
#define uni_lowlevel_strdup _wcsdup
//#define real_uni_stricmp _wcsicmp
#define uni_strlen wcslen
#define uni_strncat wcsncat
//#define real_uni_strncmp wcsncmp
#define uni_strncpy wcsncpy
//#define real_uni_strnicmp _wcsnicmp
#define uni_strpbrk wcspbrk
#define uni_strrchr (uni_char *)wcsrchr
#define uni_strrev _wcsrev
#define uni_strspn wcsspn
#define real_uni_strstr wcsstr
#define uni_strtok wcstok
#define uni_vsprintf vswprintf

#ifdef WIN_USE_STDLIB_64BIT
# define uni_access _access
# define op_force_inline __forceinline
#endif // WIN_USE_STDLIB_64BIT

#define EXCEPTION_CONTINUE_EXECUTION -1	/* Exception is dismissed. Continue execution at the point where the exception occurred. */
#define EXCEPTION_CONTINUE_SEARCH 0		/* Exception is not recognized. Continue to search up the stack for a handler, first for containing try-except statements, then for handlers with the next highest precedence. */
#define EXCEPTION_EXECUTE_HANDLER 1		/* Exception is recognized. Transfer control to the exception handler by executing the __except compound statement, then continue execution after the __except block. */
#define PLUGIN_TRY __try
#define PLUGIN_CATCH __except(EXCEPTION_EXECUTE_HANDLER)
#define PLUGIN_CATCH_CONDITIONAL(catch_if_true) __except(catch_if_true ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)

#define __PRETTY_FUNCTION__ __FUNCSIG__

// Support integration with Windows 7 taskbar thumbnails.
#define SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS
#define SUPPORT_WINDOWS7_TASKBAR_JUMPLIST

#if defined(__cplusplus)
# ifndef PLUGIN_WRAPPER
#  include "platforms/windows/WindowsTLS.h"
#  define g_component_manager (WindowsTLS::Get()->component_manager)
#  define g_cleanup_stack (WindowsTLS::Get()->cleanup_stack)
# else //!PLUGIN_WRAPPER
extern __declspec(thread) class OpComponentManager* g_component_manager;
# endif //!PLUGIN_WRAPPER
#endif // __cplusplus

#if defined(__cplusplus)
extern class CoreComponent* g_opera;
extern class OpThreadTools* g_thread_tools;
#endif // __cplusplus

#if _MSC_VER > VS2010
#define __sse2_available IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE)
#endif // _MSC_VER > VS2010

#endif // !WINDOWS_SYSTEM_H
