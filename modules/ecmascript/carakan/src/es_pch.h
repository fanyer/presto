/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1999 - 2006
 *
 * Common header for the internals of the ECMAScript engine.
 * Lars T Hansen
 */

/**
 * @section Overview              Overview of the ECMAScript engine
 *
 * The ECMAScript engine has a clearly layered design, consisting of:
 *
 * <ul>
 * <li>  a kernel run-time system, in the directory kernel/;
 * <li>  an object system, in the directory object/;
 * <li>  a compiler, in the directory compiler/;
 * <li>  a bytecode interpreter, in the directory vm/; and
 * <li>  a standard library, in the directory stdlib/.
 * </ul>
 *
 * In addition, some utility code is collected in the directory util/.
 *
 * The kernel provides initialization, representations of values, string,
 * and ECMAScript objects, heap allocation, and garbage collection.
 * See kernel/es_rts.h.
 *
 * The object system provides operations on the ECMAScript 'Object' type
 * and its subtypes, as well as representations of property names and
 * property lists.
 * See object/es_object.h.
 *
 * The compiler transforms ECMAScript source code to executable bytecode,
 * and provides related services like bytecode disassembly.
 * See compiler/es_compiler.h.
 *
 * The bytecode interpreter defines the instruction set and executes programs
 * represented in that instruction set.
 * See compiler/es_bytecode.h and compiler/es_context.h.
 *
 * The standard library provides implementations of the functions described
 * in chapter 15 of the ECMAScript specification, and code to initialize a
 * new global object with these values.  The library is written largely in
 * ECMAScript and compiled to bytecode.
 * See stdlib/es_library.h
 *
 * The layers break down in a couple of places, eg, the vm provides error
 * services that are used by the compiler.  This can probably be cleaned up
 * by moving commonalities into lower layers or into utility code.
 */

#ifndef ES_PCH_H
#define ES_PCH_H

#include "modules/ecmascript/carakan/ecmascript_parameters.h"
#include "modules/ecmascript/carakan/src/ecmascript_types.h"
#include "modules/ecmascript/carakan/src/ecmascript_object.h"
#include "modules/ecmascript/carakan/src/ecmascript_manager.h"
#include "modules/ecmascript/carakan/src/ecmascript_runtime.h"
#include "modules/ecmascript/carakan/src/ecmascript_private.h"
#include "modules/ecmascript/carakan/src/ecma_pi.h"

#include "modules/ecmascript/carakan/src/util/es_tempbuf.h"
#include "modules/ecmascript/oppseudothread/oppseudothread.h"

#include "modules/util/tempbuf.h"
#include "modules/util/adt/bytebuffer.h"

/* Roughly how many times each codeword in a function needs to be "executed"
   before the function is compiled as machine code.  A call from the function to
   a previously machine coded function is also counted towards this, as
   ES_JIT_WEIGHT_CALLED_FUNCTION. */
#define ES_JIT_GENERATE_FACTOR 32

/* Script function that could have a native dispatcher called a script function
   that has a native dispatcher. */
#define ES_JIT_WEIGHT_CALLED_FUNCTION 10

/* Roughly how many times each codeword in a function needs to be "executed" as
   a slow case in a function compiled as machine code before the machine code
   for that function is regenerated.  Certain things, like property cache
   updating and calling inlineable built-in functions count as more than their
   weight in codewords, according to the macros defined below. */
#define ES_JIT_REGENERATE_FACTOR 8

/* Cached get and put performed via slow-case: updated native dispatcher would
   inline the property access. */
#define ES_JIT_WEIGHT_CACHED_GET 10
#define ES_JIT_WEIGHT_CACHED_PUT 10

/* Script function called built-in function that can be inlined, or a call that
   was previously profiled as a call to a built-in that can be inlined turns out
   not to be that. */
#define ES_JIT_WEIGHT_INLINED_BUILTIN 10

/* Number of calls to match a regular expression until we try to JIT compile
   it. */
#define ES_REGEXP_JIT_CALLS_THRESHOLD 10

/* String length limit, above which we always try to JIT compile the regular
   expression before performing the match. */
#define ES_REGEXP_JIT_LENGTH_THRESHOLD 256

/* If a RegExp object should be callable, that is if RE(string) should be treated
   as RE.exec(string). A curious special case of the language and its implementations,
   but one that's going away. Uncomment if retiring its support causes unacceptable
   compatibility problems. */
/* #define ES_REGEXP_IS_CALLABLE */

/* Only emit code for the for the first N elements in a property
   cache: */
#define MAX_PROPERTY_CACHE_SIZE 10

#ifdef NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT
#define ANNOTATE(msg) cg.Annotate(UNI_L(msg "\n"))
#else // NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT
#define ANNOTATE(msg)
#endif // NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT

//#define USE_LOTS_OF_MEMORY
#define ES_SCRIPT_SOURCE_IS_CHUNKED
#define ES_LEXER_SOURCE_LOCATION_SUPPORT

/* How many nodes a compact class needs to have to start skiping nodes
   for properties. */
#define ES_CLASS_LINEAR_GROWTH_LIMIT 10

/* How much larger a table in a compact class needs to be to force the
   insertion of a node. */
#define ES_CLASS_GROWTH_RATE 2

/* Number of properties an object with a node based class can have
   before being converted to a singleton. */
#define ES_OBJECT_PROPERTY_COUNT_LIMIT 200

#ifdef ACOVEA
#undef ES_CLASS_LINEAR_GROWTH_LIMIT
#undef ES_CLASS_GROWTH_RATE
#undef MAX_PROPERTY_CACHE_SIZE

extern unsigned g_class_linear_growth, g_class_growth_rate, g_max_property_cache_size;

#define ES_CLASS_LINEAR_GROWTH_LIMIT g_class_linear_growth
#define ES_CLASS_GROWTH_RATE g_class_growth_rate
#define MAX_PROPERTY_CACHE_SIZE g_max_property_cache_size
#endif

//#define ES_FEATURE__ERROR_MESSAGES

// You wouldn't want to define this on platforms where sizeof(void*) >= sizeof(double):
#ifndef SIXTY_FOUR_BIT
#	define ES_VALUES_AS_NANS
#endif

#define SOURCE_CHUNK_SIZE 1024

#ifdef _STANDALONE
class URL;
#else
#  include "modules/url/url2.h"
#endif

#if defined _STANDALONE && !defined MSWIN
# define CONSTANT_DATA_IS_EXECUTABLE
#endif // _STANDALONE && !MSWIN

#if defined OPPSEUDOTHREAD_STACK_SWAPPING || defined OPPSEUDOTHREAD_THREADED
# define ES_STACK_RECONSTRUCTION_SUPPORTED
#endif // OPPSEUDOTHREAD_STACK_SWAPPING || OPPSEUDOTHREAD_THREADED

/* Clean up the definitions as necessary; in core-2 the feature system handles some of these */

#ifndef ECMASCRIPT_DEBUGINFO
#  define ECMASCRIPT_DEBUGINFO	// Decompiler and debugger both require debuginfo
#endif

#if defined ECMASCRIPT_DISASSEMBLER && !defined _DEBUG
#  undef ECMASCRIPT_DISASSEMBLER	// Never allow the disassembler to escape into a release build
#endif

#ifndef ECMASCRIPT_STANDALONE_COMPILER
#  define ES_EXECUTE_SUPPORT
#endif

#if defined ECMASCRIPT_STANDALONE_COMPILER || defined ECMASCRIPT_DISASSEMBLER
#  define ES_INLINE_ASSEMBLER		// If a call is $NAME(a1,...) where NAME is a known instruction,
									// then emit NAME as an instruction rather than calling $NAME
									// as a function.  NAME must leave a result and its encoding
									// must be a simple opcode, no arguments.
#endif

#ifndef LIMITED_FOOTPRINT_DEVICE
#  define ECMASCRIPT_OPTIMIZER		// Enable a number of conforming optimizations: makes the compiler larger & a little slower, produces faster code
#  ifdef ECMASCRIPT_DISASSEMBLER
#    define ECMASCRIPT_NONSTD_OPTIMIZER// Enable a number of nonconforming optimizations: ditto
#  endif
#endif

#ifndef NAN_ARITHMETIC_OK
#  define ES_NEED_GUARD_AGAINST_NAN
#endif

#define ARRAY_DESTRUCTURING_SUPPORT	// Array destructuring assignment:
									//    [a,b,,c] = <expr>
									// This is precisely the same as
									//    tmp = <expr>,
									//    a = tmp[0],
									//    b = tmp[1],
									//    c = tmp[3],
									//    tmp
									// for a fresh temporary tmp.  Observe there is no reason why the
									// a, b, c cannot be any legitimate left-hand-side expression:
									// variable or property reference, or even nested array expressions.

//#define MATCH_ASSIGNMENT_SUPPORT	// This leads to general destructuring assignments:
									//    {fisk:a, fnys:b} = <expr>
									// which is the same as
									//    var tmp = <expr>
									//    a = tmp.fisk
									//    b = tmp.fnys
									// but the syntax is ambiguous since object literals cannot appear
									// at the beginning of statements.  But perhaps we can introduce
									// this as a new statement _match_:
									//    match {fisk:a, fnys:b} = <expr>;
									// and from there it is really only a short step to
									//    match (<expr>) {
									//    case {fisk:a, fnys:b} : <expr>
									//    ... }
									// which matches the first case that can be destructured.  This
									// will be especially useful if the a, b can be constants as well
									// as variables.

#define ES_ENCODE_FOREIGN_TRACE_BIT	// Efficient encoding of a gc mark bit in the object header rather
									// than in the ES_Foreign_Object, but fairly gross hack so currently
									// possible to turn it off.  NOTE: if you turn it off, you also remove
									// the fix for bug #161421.

#define INTSTORAGE
#define BASIC_BLOCKS
//#define ES_DEBUGINFO_WORKAROUND

//#define ES_GC_REGIONS

#define ES_REGEX_NUL_WORKAROUND     // Hack that allows input to the RegExp constructor to contain NUL characters

//#define ES_DBG_GC					// Enable some (not very expensive) debugging functionality
//#define ES_DBG_TRACE_GC			// Trace collections in ES_DBG_TRACE_GC_FILENAME
//#define ES_DBG_DEMOGRAPHICS		// Collect property demographics (not yet supported with new GC)
#define ES_DBG_DISASSEMBLE		// Disassemble all top-level programs and dump the disassembly to ES_DBG_DISASSEMBLE_FILENAME

//#if defined _DEBUG && !defined ES_DBG_GC
//#  define ES_DBG_GC
//#endif

#ifndef HAVE_LONGLONG
#  define NO_LONG_LONG
#endif

#ifndef OP_MEMORY_VAR
#  define OP_MEMORY_VAR volatile
#endif

#ifdef ES_USE_STDLIB
#  include <stdlib.h>
#  include <stddef.h>
#  include <ctype.h>
#  include <string.h>
#  include <math.h>
#  include <time.h>
#  include <errno.h>
#  include <locale.h>
#  ifdef HAVE_LIMITS_H
#    include <limits.h>
#  endif
#  ifdef HAVE_FLOAT_H
#    include <float.h>
#  endif
#endif

#if defined(EPOC)
#  define USE_RANDOM_NOT_RAND  // FIXME: this belongs in stdlib, and with a SYSTEM setting, maybe
#endif // EPOC

/* At the request of Markus */
#ifdef WINGOGI
#  define ES_USE_PI_GETTIMEZONE_EVEN_IF_BROKEN
#endif

#ifndef _DEBUG
# if defined _MSC_VER
#  define ALWAYS_INLINE __forceinline
# elif defined __GNUC__
#  ifdef ES_GCC_BROKEN_ALWAYS_INLINE
#   define ALWAYS_INLINE inline
#  else
#   define ALWAYS_INLINE __attribute__ ((always_inline))
#  endif // TWEAK_ES_GCC_BROKEN_ALWAYS_INLINE
# endif // _MSC_VER
#else // _DEBUG
# define ALWAYS_INLINE inline
#endif // _DEBUG

#ifdef ES_GCC_STACK_REALIGNMENT
#  define REALIGN_STACK __attribute__ ((force_align_arg_pointer))
#endif // ES_GCC_STACK_REALIGNMENT

#if defined _MSC_VER
#  define NOINLINE __declspec(noinline)
#elif defined __GNUC__
#  define NOINLINE __attribute__ ((noinline))
#endif

#ifndef ALWAYS_INLINE
#  define ALWAYS_INLINE
#endif // ALWAYS_INLIN
#ifndef NOINLINE
#  define NOINLINE
#endif // NOINLINE
#ifndef REALIGN_STACK
#  define REALIGN_STACK
#endif // REALIGN_STACK

#ifndef M_E
#  define M_E			2.7182818284590452354	/* e */
#endif

#ifndef M_PI
#  define M_PI			3.14159265358979323846	/* pi */
#endif

#ifndef M_LOG2E
#  define M_LOG2E		1.4426950408889634074   /* log_2 e */
#endif

#ifndef M_LOG10E
#  define M_LOG10E		0.43429448190325182765  /* log_10 e */
#endif

#ifndef M_LN2
#  define M_LN2			0.69314718055994530942  /* log_e 2 */
#endif

#ifndef M_LN10
#  define M_LN10		2.30258509299404568402  /* log_e 10 */
#endif

#ifndef M_SQRT2
#  define M_SQRT2		1.41421356237309504880  /* sqrt(2) */
#endif

#ifndef M_SQRT1_2
#  define M_SQRT1_2		0.70710678118654752440  /* 1/sqrt(2) */
#endif

#ifndef SPLICE
#  define SPLICE(a, b)	a ## b
#endif

#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#if defined HAS_COMPLEX_GLOBALS
// Use a proper table when possible
#  define ST_HEADER(name) static const char * const name[] = {
#  define ST_HEADER_UNI(name) static const uni_char * const name[] = {
#  define ST_ENTRY(name,value) value,
#  define ST_FOOTER 0 };
#  define ST_FOOTER_UNI 0 };
#  define ST_REF(name,x) name[x]
#else
// Otherwise use a function that switches on the index
#  define ST_HEADER(name) static const char * name(int x) { switch (x) {
#  define ST_HEADER_UNI(name) static const uni_char * name(int x) { switch (x) {
#  define ST_ENTRY(name,value) case name : return value;
#  define ST_FOOTER default: OP_ASSERT(0); return "***"; }; }
#  define ST_FOOTER_UNI default: OP_ASSERT(0); return UNI_L("***"); }; }
#  define ST_REF(name,x) name(x)
#endif

// basically the same as CONST_ARRAY and friends, but with length checking
#if defined HAS_COMPLEX_GLOBALS
# define ES_CONST_ARRAY(name,type,length) const type const name[length] = {
# define ES_CONST_ENTRY(x)         x
# define ES_CONST_END(name)            };
# define ES_CONST_ARRAY_INIT(name) ((void)0)
#else
# define ES_CONST_ARRAY(name,type,length) void init_##name () { OP_ASSERT((ARRAY_SIZE(name) == length)); const type *local = name; int i=0;
# define ES_CONST_ENTRY(x)         local[i++] = x
# define ES_CONST_END(name)        ; OP_ASSERT(i <= (ARRAY_SIZE(name))); }
# define ES_CONST_ARRAY_INIT(name) init_##name()
#endif // HAS_COMPLEX_GLOBALS

/* Double to integral conversions:

   The problem is the result produced by conversions not defined
   by the std, eg -1.0 -> unsigned or NaN -> integral.  Some systems
   do what the ECMAScript std asks for, others do not.
*/

#define DOUBLE2INT32(x)			op_double2int32(x)
#define DOUBLE2UINT32(x)		op_double2uint32(x)


/* Integral to double conversions: use our own code, or platform code.
   Our own code is slower, so should only be used when required.

   The problem is that the platform treats unsigned numbers with the
   high bit set as signed, or conversely, signed numbers with the high
   bit set as unsigned.

   ECMASCRIPT_INTEGRAL_TO_FLOATING is the official #define covering this.
*/

/* EPOC turned on as a result of discoveries on the Papagena project
   MSWIN && _DEBUG to ensure that it's being tested
   BREW at the request of William
*/

#if defined EPOC || defined BREW || defined MSWIN && defined _DEBUG
#  ifndef ECMASCRIPT_INTEGRAL_TO_FLOATING
#    define ECMASCRIPT_INTEGRAL_TO_FLOATING // Can/should also be controlled externally
#  endif
#endif

/* FIXME: We probably want this to be handled in stdlib, with some suitable inline
   function for the cast case.  */

#if defined ECMASCRIPT_INTEGRAL_TO_FLOATING
#  define UNSIGNED2DOUBLE(x)            unsigned2double(x)
#else
#  define UNSIGNED2DOUBLE(x)            ((double)(x))
#endif
#define INT2DOUBLE(x)                 ((double)(x))

#ifdef USE_RANDOM_NOT_RAND
extern double random();
#endif

#ifdef ES_DBG_GC
extern int ilog2(unsigned int k);
#endif

typedef int sourceloc_t;
#define NO_SOURCELOC ((int)~0)				// All 1 bits means no information
#define NO_SOURCEPOS ((unsigned int)~0)		// ditto

// Temporary architecture definitions
#ifndef _STANDALONE
# ifdef ARCHITECTURE_IA32
#  ifdef SIXTY_FOUR_BIT
#   define ARCHITECTURE_AMD64
#   if defined(MSWIN) || defined(WINGOGI)
#    define ARCHITECTURE_AMD64_WINDOWS
#   else
#    define ARCHITECTURE_AMD64_UNIX
#   endif // MSWIN || WINGOGI
#  endif // SIXTY_FOUR_BIT
#  define ARCHITECTURE_SSE
# endif // ARCHITECTURE_IA32
#
# ifdef ARCHITECTURE_ARM
#  define ARCHITECTURE_ARM_VFP
# endif // ARCHITECTURE_ARM

#if defined _STANDALONE && !defined MSWIN
# define CONSTANT_DATA_IS_EXECUTABLE
#endif // _STANDALONE && !MSWIN

// Other things always set by the standalone Makefile.
# define ES_COMBINED_ADD_SUPPORT

# ifdef ECMASCRIPT_NATIVE_SUPPORT
#  define ES_NATIVE_SUPPORT
# endif // ECMASCRIPT_NATIVE_SUPPORT
#endif // _STANDALONE

#if !defined ES_STACK_ALIGNMENT || ES_STACK_ALIGNMENT == 0
#  undef ES_STACK_ALIGNMENT
#  if defined ARCHITECTURE_AMD64
#    define ES_STACK_ALIGNMENT 16
#  else // ARCHITECTURE_AMD64
#    define ES_STACK_ALIGNMENT 8
#  endif // ARCHITECTURE_AMD64
#endif // ES_STACK_ALIGNMENT

#ifdef _MSC_VER
#define ES_DECLARE_NOTHING() (void)0
#define ES_OFFSETOF(cls, member) offsetof(cls, member)
#else
#define ES_DECLARE_NOTHING() void *nothing = NULL
#define ES_OFFSETOF(cls, member) reinterpret_cast<UINTPTR>(&static_cast<cls *>(nothing)->member)
#endif // _MSC_VER


struct Bigint;
struct CGEnv;
class  ESMM;
class  ESMM_DynamicRoots;
struct ES_Activation;
class  ES_Allocation_Context;
class  ES_Arguments_Object;
class  ES_Array;
class  ES_Boxed;
class  ES_Class;
class  ES_Compiler;
struct ES_CompilerOptions;
class  ES_Context;
struct ES_DebugControl;
struct ES_DebugInfo;
class  ES_Foreign_Function;
class  ES_Foreign_Object;
class  ES_Function;
class  ES_Global_Object;
class  ES_MarkStackSegment;
class  ES_Object;
class  ES_PageHeader;
class  ES_Program;
class  ES_Property_Object;
struct ES_Property_Segment;
class  ES_RegExp_Object;
class  ES_TempBuffer;
class  ES_Value;
class  JString;

#ifndef UINT16_MAX
#define UINT16_MAX 0xffffu
#endif // UINT16_MAX

#if defined _MSC_VER
# define ES_CALLING_CONVENTION __fastcall
# define ES_CDECL_CALLING_CONVENTION __cdecl
# define ES_USE_FASTCALL
#else // _MSC_VER
# define ES_CALLING_CONVENTION
# define ES_CDECL_CALLING_CONVENTION
# define ES_USE_STDCALL
#endif // _MSC_VER

/* Kernel: storage management; ECMAScript value representation */

#include "modules/ecmascript/carakan/src/kernel/es_heap_object.h"
#include "modules/ecmascript/carakan/src/kernel/es_page.h"
#include "modules/ecmascript/carakan/src/kernel/es_collector.h"
#include "modules/ecmascript/carakan/src/kernel/es_mark_sweep_heap.h"
#include "modules/ecmascript/carakan/src/kernel/es_string.h"
#include "modules/ecmascript/carakan/src/kernel/es_value.h"
#include "modules/ecmascript/carakan/src/vm/es_bytecode.h"
#include "modules/ecmascript/carakan/src/vm/es_instruction.h"
#include "modules/ecmascript/carakan/src/kernel/es_rts.h"

/* ECMAScript 'Object' representation */

#include "modules/ecmascript/carakan/src/object/es_property.h"
#include "modules/ecmascript/carakan/src/object/es_object.h"
#include "modules/ecmascript/carakan/src/object/es_global_object.h"

#include "modules/ecmascript/carakan/src/object/es_boolean_object.h"
#include "modules/ecmascript/carakan/src/object/es_date_object.h"
#include "modules/ecmascript/carakan/src/object/es_error_object.h"
#include "modules/ecmascript/carakan/src/object/es_number_object.h"
#include "modules/ecmascript/carakan/src/object/es_string_object.h"

/* Utility functions */

#include "modules/ecmascript/carakan/src/util/es_util.h"

#include "modules/ecmascript/carakan/src/vm/es_context.h"
#include "modules/ecmascript/carakan/src/vm/es_execution_context.h"
#include "modules/ecmascript/carakan/src/vm/es_allocation_context.h"
#include "modules/ecmascript/carakan/src/kernel/es_boxed.h"

/* Inline methods */

#include "modules/ecmascript/carakan/src/object/es_array_object.h"
#include "modules/ecmascript/carakan/src/kernel/es_boxed_inlines.h"
#include "modules/ecmascript/carakan/src/kernel/es_collector_inlines.h"
#include "modules/ecmascript/carakan/src/kernel/es_page_inlines.h"
#include "modules/ecmascript/carakan/src/kernel/es_mark_sweep_heap_inlines.h"
#include "modules/ecmascript/carakan/src/kernel/es_string_inlines.h"
#include "modules/ecmascript/carakan/src/kernel/es_value_inlines.h"
#include "modules/ecmascript/carakan/src/object/es_class_inlines.h"
#include "modules/ecmascript/carakan/src/object/es_function_inlines.h"
#include "modules/ecmascript/carakan/src/object/es_property_table_inlines.h"
#include "modules/ecmascript/carakan/src/vm/es_block_inlines.h"
#include "modules/ecmascript/carakan/src/vm/es_suspended_call_inlines.h"
#include "modules/ecmascript/carakan/src/kernel/es_box_inlines.h"

#ifndef _DEBUG
# include "modules/ecmascript/carakan/src/object/es_object_inlines.h"
# include "modules/ecmascript/carakan/src/object/es_indexed_inlines.h"
# include "modules/ecmascript/carakan/src/vm/es_frame_inlines.h"
#endif // _DEBUG

#endif  // ES_PCH_H
