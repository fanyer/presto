/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
\mainpage Standard porting library.

This is the auto-generated API documentation for the stdlib module. For additional
information about the module, see the module's
<a href="http://wiki/developerwiki/index.php/Modules/stdlib">Wiki page</a>.


\author Lars T Hansen, aided by a cast of thousands

\section quickref                   Quick reference

Go to opera_stdlib.h.

\section important                  Important restriction

Even though this file defines functions for the C99 library, and all
functions are declared as "extern C" so that they can be called from
C code, it is not a C header file!  Including it into your C files
is not possible.

The proper fix for this problem will be to have a C version of
opera_stdlib.h to be used with C projects.


\section intro                      Introduction

The stdlib module provides implementations of a subset the C99 library
to Opera ports whose platform-provided libraries are broken, missing,
or otherwise inadeaquate.  The functions exported from stdlib are
intended to be of at least reasonable quality, and highly portable.

Functions can be enabled independently of each other in the SYSTEM system,
where SYSTEM_MEMCMP = NO means that the system does not have memcmp and
that the stdlib function should be used instead.

There are three groups of functions exported from this module:

  <ul>
  <li> functions defined in C99, with an op_ prefix, eg op_strcpy

  <li> functions not defined in C99, but closely related.  For example,
       C99 defines many functions on floating-point numbers like
	   frexp, ldexp, and isnan.  So this library exports related functions
	   like op_explode_double, which picks a part a double-precision
	   number into high and low words.

  <li> unicode-equivalents of functions defined in C99 or closely
       related functionality, eg, uni_strcpy.
  </ul>

The API is defined in the header file opera_stdlib.h


\section normativity               Normative implications

<b>PLEASE NOTE VERY CAREFULLY THE FOLLOWING.</b>

The next section describes some incompatibilities between stdlib and the
C99 standard.  The implication of these incompatibilities is that
unless the platform library is compatible with our interpretation,
then functions from stdlib must be used on the platform, even if the
platform library is compatible with C99.



\section incompatibilities         Incompatibilities and implementation dependencies

In a few cases, the functions we implement here are defined as having
implementation-dependent behavior.  For example, the argument to the
nan() function has an implementation-dependent interpretation, and
whether malloc() returns NULL when passed the argument 0 is also
implementation-dependent.

In order to make it easier to write code, this library assumes that
these implementation dependencies are resolved in certain ways.

In other cases, we wish to assume less than the C standard requires,
and our own implementations of these functions will therefore not be
entirely compliant with the standard.

These instances are noted with "INCOMPATIBILITY NOTE" sections in the
function documentation.  A summary is:

  <ul>
  <li> malloc(0) is not allowed to return NULL except when signalling OOM

  <li> calloc(m,n) is not allowed to return NULL if m*n==0, except when
       signalling OOM

  <li> strncpy(s1,s2,n) does not have to NUL-pad s1 if less than n bytes
       is copied, though it may.

  <li> toupper() and tolower() operate correctly on the entire Unicode
       range, not just ASCII

  <li> printf() and scanf() implement subsets of C99, because not all C99
       data types are supported by all Opera ports

  <li> we require a certain interpretation for the argument to nan()
  </ul>



\section oom-handling OOM handling and reporting

Be aware that some of these functions may allocate memory for working
storage and thus may experience out-of-memory events.  However, since
the standard "errno" interface is not available on all platforms, those
events are not always detectable.

In those cases where it is known that the function may experience an
OOM condition and it is clear that it can report that reliably, that
behavior is documented in this library.

In cases where it is known that the function may experience an OOM
condition but the reporting status is unknown, some sort of likely
behavior may be documented instead.

*/

/* DOCUMENTATION NOTE: Every function should be annotated with a reference to
   its defining specification, if such a specification exists.  If a specification
   does not exist, then the documentation should be clear and complete.

   Many of these functions are defined in ANSI C99.  A copy of the ANSI spec may
   be found in the	CVS module "ansi_standards".  */

/* IMPLEMENTATION NOTE ABOUT INLINE FUNCTIONS: Even though these are functions,
   they are macros on some platforms, so we never need to be able to take their
   address.  So they can be inline.

   Note that because of the order of include files in hardcore/base/baseincludes.h,
   inline functions cannot reference stdlib global variables.  The way to fix this
   is to move inline functions to a separate file and include that following
   the setup of global variables in baseincludes.h.  */

#ifndef OPERA_STDLIB_H
#define OPERA_STDLIB_H

extern "C" {

/* In stdlib_integer.cpp */

#ifndef HAVE_ATOI
/** C99 7.20.1.2: convert the initial portion of the string to int */
int op_atoi(const char* p);
#endif

#ifndef HAVE_STRTOL
/** C99 7.20.1.4: convert the initial portion of the string to long */
long op_strtol(const char* nptr, char** endptr, int radix);
#endif

#ifndef HAVE_STRTOUL
/** C99 7.20.1.4: convert the initial portion of the string to unsigned long */
unsigned long op_strtoul(const char* nptr, char** endptr, int base);
#endif

#ifdef STDLIB_64BIT_STRING_CONVERSIONS
/** Not in standard, behavior similar to op_strtol */
INT64 op_strtoi64(const char* nptr, char** endptr, int base);

/** Not in standard, behavior similar to op_strtoul */
UINT64 op_strtoui64(const char* nptr, char** endptr, int base);
#endif // STDLIB_64BIT_STRING_CONVERSIONS

#ifndef HAVE_ITOA
/** Convert the value to its string representation in the given buffer,
    using the given radix, where 2 <= radix <= 36.  The buffer must be
    large enough to hold the value.  Return a pointer to the buffer.

    The buffer is always NUL-terminated.

    If radix is 10 and the value is negative, then the output is '-'
    followed by the representation of the absolute value of 'value'.
    For all other radices, 'value' is treated as unsigned.  */
char* op_itoa(int value, char* string, int radix);
#endif

#ifndef HAVE_LTOA
/** Convert the value to its string representation in the given buffer,
    using the given radix, where 2 <= radix <= 36.  The buffer must be
    large enough to hold the value.  Return a pointer to the buffer.

    The buffer is always NUL-terminated.

    If radix is 10 and the value is negative, then the output is '-'
    followed by the representation of the absolute value of 'value'.
    For all other radices, 'value' is treated as unsigned.  */
char* op_ltoa(long value, char* string, int radix);
#endif

#ifdef STDLIB_64BIT_STRING_CONVERSIONS
/** Like itoa, but with an 'INT64' value */
char* op_i64toa(INT64 value, char* string, int radix);
#endif // STDLIB_64BIT_STRING_CONVERSIONS

#ifndef HAVE_ABS

/** C99 7.20.6.1: compute the absolute value of n */
static inline int op_abs(int n)
{
    return n < 0 ? -n : n;
}

#endif // HAVE_ABS

#ifndef HAVE_RAND	// srand and rand

/* IMPLEMENTATION NOTE: There are two alternative implementations
   of the random number generator in the library.

   If FEATURE_3P_RANDOM is defined then the generator is a fast
   Mersenne Twister RNG with a period of 2^19937-1.

   If FEATURE_3P_RANDOM is not defined then the generator is a simple
   linear congruential algorithm with "adequate" properties.  */

#undef RAND_MAX
#define RAND_MAX (0x7FFFFFFF)

extern UINT32 stdlib_rand(void);
extern void	  stdlib_srand(UINT32 s);

/** C99 7.20.2.2: seed the random number generator */
static inline void op_srand(unsigned int seed)
{
	stdlib_srand((UINT32)seed);
}

/** C99 7.20.2.1: return the next pseudo-random number in the sequence
    determined initially by a call to op_srand. */
static inline int op_rand()
{
	return (int)stdlib_rand();
}

#endif

#ifndef HAVE_NTOHS
/** BSD 4.3: Convert short from network byte order to host byte order. */
UINT16 op_ntohs(UINT16 netshort);
#endif

#ifndef HAVE_NTOHL
/** BSD 4.3: Convert long from network byte order to host byte order. */
UINT32 op_ntohl(UINT32 netlong);
#endif

#ifndef HAVE_HTONS
/** BSD 4.3: Convert short from host byte order to network byte order. */
UINT16 op_htons(UINT16 hostshort);
#endif

#ifndef HAVE_HTONL
/** BSD 4.3: Convert long from host byte order to network byte order. */
UINT32 op_htonl(UINT32 hostlong);
#endif


/* In stdlib_memory.cpp */

#ifndef HAVE_MEMCMP
/** C99 7.21.4.1: compare memory regions of size n */
int op_memcmp(const void* a, const void* b, size_t n);
#endif

#ifndef HAVE_MEMCPY
/** C99 7.21.2.1: copy n bytes from src to dest; src and dest may
    not overlap.  Return src. */
void* op_memcpy(void *dest, const void *src, size_t n);
#endif

#ifndef HAVE_MEMMOVE
/** C99 7.21.2.2: copy n bytes from src to dest; src and dest may
    overlap.  Return src.  */
void* op_memmove(void *dest, const void *src, size_t n);
#endif

#ifndef HAVE_MEMSET
/** C99 7.21.6.1: initialize the first n bytes of s to the value c */
void* op_memset(void *s, int c, size_t n);
#endif

#ifndef HAVE_MEMCHR
/** C99 7.21.5.1: return the address of the first occurence of c in the
    n bytes starting at buf, or NULL if c does not occur.  */
void* op_memchr(const void* buf, int c, size_t n);
#endif

#ifndef HAVE_MEMMEM
/** GNU: search the memory area haystack for the substring needle,
    and return a pointer to its first occurence or NULL if needle is
    not found in haystack.  */
void* op_memmem(const void* haystack, size_t haystacklen, const void* needle, size_t needlelen);
#endif

#ifndef HAVE_UNI_MEMCHR
/** C99 7.21.5.1: return the address of the first occurence of c in the
    n characters starting at buf, or NULL if c does not occur.  */
uni_char* uni_memchr(const uni_char* buf, uni_char c, size_t n) PURE_FUNCTION;
#endif


/* In stdlib_string.cpp */

#ifndef HAVE_STRLEN
/** C99 7.21.6.3: return the number of characters in s before the
    first NUL byte */
size_t op_strlen(const char* s);
#endif

#ifndef HAVE_STRCPY
/** C99 7.21.2.3: copy the string from src to dest, including the NUL.
    The strings may not overlap.  Return dest. */
char* op_strcpy(char* dest, const char* src);
#endif

#ifndef HAVE_STRNCPY
/** C99 7.21.2.4: copy at most n characters from src to dest.  The strings
    may not overlap.  Return dest.

    INCOMPATIBILITY NOTE: the C99 spec requires that dest is NUL-filled
	if less than n bytes is copied.  The Opera implementation of this
	function will append one NUL (as if it copied the terminating NUL of
	src), but will not NUL-fill.  This is an efficiency issue.

    If the performance implications of the standard behavior bothers you,
    then either set SYSTEM_STRNCPY=NO in all products, or use strlcpy in
    your code instead.  */
char* op_strncpy(char* dest, const char* src, size_t n);
#endif

#ifndef HAVE_STRLCPY
/** OpenBSD: copy the string src to the destination buffer dest of size
	dest_size (in chars). The resulting string is always NUL terminated
	except for when dest_size is zero.

	If the src string is longer than the dest buffer the resulting string
	will be truncated to fit the dest buffer.

	A return value greater than or equal to dest_size means that the
	resulting string was truncated.

	@param dest the destination buffer.
	@param src the string to be copied to dest.
	@param dest_size the size of dest in chars.
	@return the length of src. */
size_t op_strlcpy(char *dest, const char *src, size_t dest_size);
#endif

#ifndef HAVE_STRCAT
/** C99 7.21.3.1: append src to dest.  The strings may not overlap.
    Returns dest.  */
char* op_strcat(char* dest, const char* src);
#endif

#ifndef HAVE_STRNCAT
/** C99 7.21.3.1: append not more than n characters of src to dest,
    and append a terminating NUL if one was not copied from src.
    The strings may not overlap.  Returns dest.

    Note that a NUL byte is appended even if n reached 0. */
char* op_strncat(char* dest, const char* src, size_t n);
#endif

#ifndef HAVE_STRLCAT
/** OpenBSD: append the string src to the NUL terminated string in the
	buffer dest of size dest_size (in chars). The resulting string is
	always NUL terminated except for when dest_size is zero.

	If the resulting string is longer than the dest buffer it will be
	truncated to fit the dest buffer.

	A return value greater than or equal to dest_size means that the
	resulting string was truncated.
	@param dest the destination buffer containing a NUL terminated string.
	@param src the string to be appended to the string in dest.
	@param dest_size the size of dest in chars.
	@return the length of dest + src. */
size_t op_strlcat(char * dest, const char * src, size_t dest_size);
#endif

#ifndef HAVE_STRCMP
/** C99 7.21.4.2: compare s1 and s2 */
int op_strcmp(const char* s1, const char* s2);
#endif

#ifndef HAVE_STRNCMP
/** C99 7.21.4.4: compare prefixes of size not more than n of s1 and s2 */
int op_strncmp(const char* s1, const char* s2, size_t n);
#endif

#ifndef HAVE_STRICMP
/** Like strcmp, but case-insensitively by folding each character to
    lower case with op_tolower before comparing */
int op_stricmp(const char* s1, const char* s2);
#endif

#ifndef HAVE_STRNICMP
/** Like strncmp, but case-insensitively by folding each character to
    lower case with op_tolower before comparing */
int op_strnicmp(const char* s1, const char* s2, size_t n);
#endif

#ifndef HAVE_STRCHR
/** C99 7.21.5.2: search s for c, returning a pointer to the
    first occurence or NULL if c does not occur in s.  c may be
    the NUL byte. */
char* op_strchr(const char* s, int c);
#endif

#ifndef HAVE_STRRCHR
/** C99 7.21.5.5: search s for c, returning a pointer to the
    last occurence or NULL if c does not occur in s.  c may be
    the NUL byte. */
char* op_strrchr(const char* s, int c);
#endif

#ifndef HAVE_STRSTR
/** C99 7.21.5.7: search the string haystack for the substring needle,
    and return a pointer to its first occurence or NULL if needle is
	not found in haystack.  */
char* op_strstr(const char* haystack, const char* needle);
#endif

#ifndef HAVE_STRISTR
/** A case-insensitive version of op_strstr.
    \see op_strstr */
const char* op_stristr(const char* haystack, const char* needle);
#endif

#ifndef HAVE_STRSPN
/** C99 7.21.5.6: compute the length of the initial segment of s consisting
    of characters from the set accept.  */
size_t op_strspn(const char* s, const char* accept);
#endif

#ifndef HAVE_STRCSPN
/** C99 7.21.5.3: compute the length of the initial segment of s consisting
    of characters not from the set reject.  */
size_t op_strcspn(const char* s, const char* reject);
#endif

#ifndef HAVE_STRPBRK
/** C99 7.21.5.4: compute the first location in s of any character from
    the string accept, or NULL if none of the characters occur in s */
char* op_strpbrk(const char *s, const char *accept);
#endif

#ifndef HAVE_STRDUP
/** Return a heap-allocated copy of s, or NULL if memory could not
    be allocated.  The copy is allocated with op_malloc.  */
char* op_lowlevel_strdup(const char* s);
#endif

#ifndef HAVE_STRCASE
/** Convert s in-place to upper case */
void op_strupr(char* s);

/** Convert s in-place to lower case */
void op_strlwr(char* s);
#endif

#ifndef HAVE_STRREV
/** Reverse s in-place */
void op_strrev(char *s);
#endif

/* Not implemented:
     op_strtok --  but we have uni_strtok, so easy enough  */


/* In stdlib_float.cpp */

/* Some constants and data structures used for floating point manipulation,
   some of it in inline functions */

#define EXPMASK 0x7ff00000U		/**< Mask for exponent field of high word */
#define EXPSHIFT 20				/**< Number of bits to shift exponent */
#define EXPBIAS 1023			/**< Bias of the exponent */
#define FRACBITS 52				/**< Bits in the significand */
#define SIGNBIT 0x80000000U		/**< Mask for sign bit of high word */

#ifdef IEEE_MC68k				/* Hi word at lower address */
#  define IEEE_BITS_LO 1
#  define IEEE_BITS_HI 0
#elif defined IEEE_8087			/* Low word at lower address */
#  define IEEE_BITS_LO 0
#  define IEEE_BITS_HI 1
#endif

/** This union is used to extract the high and low words of a double.
    The high word is at index IEEE_BITS_HI in the bits array, the low
    word at IEEE_BITS_LO.  */
union double_and_bits
{
    UINT32 bits[2];
    double d;
};

/** Return the integer part of a floating-point number.  Not defined on
    NaN or Infinity.  */
double stdlib_intpart(double x);

#ifndef HAVE_CEIL
/** C99 7.12.9.1: computes the smallest integer value not less than x */
static inline double op_ceil(double x)
{
	if (x < 0)
		return stdlib_intpart(x);
	else
	{
		double i = stdlib_intpart(x);
		return (x == i) ? x : i+1;
	}
}
#endif

#ifndef HAVE_FLOOR
/** C99 7.12.9.2: computes the largest integer value not greater than x */
double op_floor(double x);
#endif

#ifndef HAVE_MODF
/** C99 7.12.6.12: breaks x into fractional and integral parts, each of
    which has the same sign as the argument.  Stores the integral part
	in iptr, returns the fractional part.  */
double op_modf(double x, double* iptr);
#endif

#ifndef HAVE_FMOD
/** C99 7.12.10.1: computes the floating-point remainder of x/y, ie,
    a number r = x - ny where r has the same sign as x and magnitude
	less than that of y.  If y is zero, returns a domain error or zero.  */
double op_fmod(double x, double y);
#endif

#ifndef HAVE_FREXP
/** C99 7.12.6.4: break a floating-point number into a normalized fraction
    and an integral power of 2. */
double op_frexp(double value, int* exp);
#endif

#ifndef HAVE_LDEXP
/** C99 7.12.6.6: multiply a floating-point number by an integral power
    of 2.  */
double op_ldexp(double x, int exp);
#endif

#ifndef HAVE_FABS
/** C99 7.12.7.2: computes the absolute value of x.  */
static inline double op_fabs(double x)
{
#ifdef HARDWARE_FLOATS
	return (x < 0.0) ? -x : x;
#else
	double_and_bits u;
	u.d = x;
    u.bits[IEEE_BITS_HI] = u.bits[IEEE_BITS_HI] & ~SIGNBIT;
	return u.d;
#endif
}
#endif // HAVE_FABS

#ifndef HAVE_STRTOD
/** C99 7.20.1.3: Parse a floating-point number from a string.

    \param  nptr    the string to parse
	\param  endptr  pointer to location into which to store a pointer
	                to the first unconsumed character from the input
	\return the number parsed
	\todo   what happens on OOM or input error?

   OOM NOTE: this function may allocate memory for working storage.
     If an OOM event occurs, g_stdlib_infinity is returned
     and *endptr == nptr.  However, this behavior is not reliable.

   IMPLEMENTATION NOTE: There are three alternative implementations
     of strtod in the library.

     If FEATURE_3P_DTOA_DAVID_M_GAY is defined then op_strtod uses
     David Gay's code based on Will Clinger's algorithm. This is
     considered the gold standard.

     If FEATURE_3P_FLOITSCH_DOUBLE_CONVERSION is defined then op_strtod
     uses Florian Loitsch's code (based on his GRISU algorithm.) Slightly
     faster than FEATURE_3P_DTOA_DAVID_M_GAY overall.

     If FEATURE_3P_DTOA_DAVID_M_GAY nor FEATURE_3P_FLOITSCH_DOUBLE_CONVERSION
     is defined, and the platform does not provide its own strtod(), this
     function will be implemented using the porting interface provided
     by the StdlibPI class. */
double op_strtod(const char *nptr, char **endptr);
#endif

/** Given high and low parts, construct a double and return it. */
double op_implode_double( UINT32 hi, UINT32 lo );

/** Given a double, return high and low parts. */
void op_explode_double( double r, UINT32& hiword, UINT32& loword );

/** Given a double, return the high word (useful in expressions) */
UINT32 op_double_high( double r );

/** Given a double, return the low word (useful in expressions) */
UINT32 op_double_low( double r );

/** ECMA-262 9.5: convert a double to an integer.

    Use this only if you need results according to the ECMAScript spec
	or need reliable results for values that are NaN, Infinity, or
	outside the range of an INT32.  In other cases use a standard cast.  */
INT32 op_double2int32(double r);

/** ECMA-262 9.6: convert a double to an unsigned integer.

    Use this only if you need results according to the ECMAScript spec
	or need reliable results for values that are NaN, Infinity, or
	outside the range of an UINT32.  In other cases use a standard cast.  */
UINT32 op_double2uint32(double r);

/** C99 7.12.11.2: Generate a quiet NaN, possibly with some extra bits.

    INCOMPATIBILITY NOTE: This is not protected by HAVE_NAN, and there
	is no SYSTEM_NAN to allow you to use the system's function.  The
	reason is that the interpretation of the tagp argument is
	implementation-defined, and we give a definite interpretation to
	it here, to allow portable code to be written elsewhere in Opera.

    If tagp is NULL, then the low 51 bits of the returned NaN are
	unspecified; some of them will be nonzero.

	If tagp is not NULL then it is a pointer to an 8-byte area which
	represents a 64-bit quantity as an array of two native-endianness
	32-bit unsigned integers: first the most significant part, then
	the least significant part.  The upper 13 bits of the 64-bit number
	are ignored; the lower 51 bits are installed in the NaN.

    If the lower 51 bits are all zero, and the system uses a 0 in the
	52nd bit to represent a quiet NaN, then the 51st bit will be set to 1
	to prevent the number from being interpreted as an infinity.

	For example:

	   UINT32 bits[2];
	   bits[0] = ...;
	   bits[1] = ...;
	   d = op_nan((char*)&bits)  */
double op_nan(const char* tagp);

/** C99 7.12.3.4: Test whether a number is NaN.

    A number is NaN if the exponent is 2047 (0x7FF) and the fraction
    is nonzero */
static inline int op_isnan( double d )
{
	double_and_bits u;
    u.d = d;

	return (u.bits[IEEE_BITS_HI] & EXPMASK) == EXPMASK &&
		   ((u.bits[IEEE_BITS_HI] & ~(EXPMASK|SIGNBIT)) != 0 || u.bits[IEEE_BITS_LO] != 0);
}

/** C99 7.12.3.2: Test whether a number is finite.

    A number is finite if it is not NaN or infinite.

    A number is infinite if the exponent is 2047 and the fraction
	is zero. Put another way, a number is finite if the exponent
    is not 2047. */
static inline int op_isfinite(double d)
{
	double_and_bits u;
	u.d = d;

	return (u.bits[IEEE_BITS_HI] & EXPMASK) != EXPMASK;
}

/** C99 7.12.3.3: Test whether a number is infinite.

    A number is infinite if the exponent is 2047 and
    the fraction is zero. */
static inline int op_isinf(double d)
{
	double_and_bits u;
	u.d = d;

	return (u.bits[IEEE_BITS_HI] & ~SIGNBIT) == EXPMASK && u.bits[IEEE_BITS_LO] == 0;
}

/** C99 7.12.3.6: Extract the signbit from any number. */
static inline int op_signbit(double d)
{
	double_and_bits u;
	u.d = d;

	return (u.bits[IEEE_BITS_HI] >> 31) & 1;
}

/** C99 7.12.11.1: Return 'a' with the sign of 'b'.  */
static inline double op_copysign(double a, double b)
{
	double_and_bits ua, ub;
	ua.d = a;
	ub.d = b;

    ua.bits[IEEE_BITS_HI] = (ua.bits[IEEE_BITS_HI] & ~SIGNBIT) | (ub.bits[IEEE_BITS_HI] & SIGNBIT);
	return ua.d;
}

/** Test whether a floating-point number represents an integer.
	Handles NaN and Infinity.  */
static inline int op_isintegral( double d )
{
	return op_isfinite(d) && stdlib_intpart(d) == d;
}

/** Truncate a number (round toward zero).
    Handles NaN and Infinity.  */
static inline double op_truncate(double d)
{
	if (op_isfinite(d))
		return stdlib_intpart(d);
	else
		return d;
}


/* In stdlib_math.cpp */

#ifndef HAVE_POW
/** C99 7.12.7.4: compute x raised to the power of y */
double op_pow(double x, double y);
#endif

#ifndef HAVE_SQRT
/** C99 7.12.7.5: compute the nonnegative square root of a nonnegative x */
double op_sqrt(double x);
#endif

#ifndef HAVE_EXP
/** C99 7.12.6.1: compute the base-e exponential of x */
double op_exp(double x);
#endif

#ifndef HAVE_LOG
/** C99 7.12.6.7: compute the base-e logarithm of x */
double op_log(double x);
#endif

#ifndef HAVE_SIN
/** C99 7.12.4.6: compute the sine of x */
double op_sin(double x);
#endif

#ifndef HAVE_COS
/** C99 7.12.4.5: compute the cosine of x */
double op_cos(double x);
#endif

#ifndef HAVE_TAN
/** C99 7.12.4.7: compute the tangent of x */
double op_tan(double x);
#endif

#ifndef HAVE_ASIN
/** C99 7.12.4.2: compute the arc sine of x */
double op_asin(double x);
#endif

#ifndef HAVE_ACOS
/** C99 7.12.4.1: compute the arc cosine of x */
double op_acos(double x);
#endif

#ifndef HAVE_ATAN
/** C99 7.12.4.3: compute the arc tangent of x */
double op_atan(double x);
#endif

#ifndef HAVE_ATAN2
/** C99 7.12.4.4: compute the arc tangent of y/x */
double op_atan2(double y, double x);
#endif


/* In stdlib_time.cpp. */

/* Use these definitions in system.h if your system headers do not
   otherwise provide them.  See C99 7.23.

    typedef long int time_t;

    struct tm {
	  int tm_sec;
	  int tm_min;
	  int tm_hour;
	  int tm_mday;
	  int tm_mon;
	  int tm_year;
	  int tm_wday;
	  int tm_yday;
	  int tm_isdst;
	};

	typedef long int clock_t;
	*/

/* Set the default size of 'time_t' so e.g. gmtime() can do the right
   thing (have a bigger range for 64-bit architectures). */

#ifdef SIXTY_FOUR_BIT
#  define DEFAULT_STDLIB_SIXTY_FOUR_BIT_TIME_T 1
#else
#  define DEFAULT_STDLIB_SIXTY_FOUR_BIT_TIME_T 0
#endif

#ifndef HAVE_TIME
/** C99 7.23.2.4: returns the current calendar time in an unspecified
    format, both as return value and optionally through *t. */
time_t op_time(time_t* t);
#endif

#ifndef HAVE_LOCALTIME
/** C99 7.23.3.4: converts 'clock' to local time and returns a pointer
    to the broken-down local time, or NULL. */
struct tm* op_localtime(const time_t* clock);
#endif

#ifndef HAVE_GMTIME
/** C99 7.23.3.3: returns a pointer to the broken-down UTC time, or NULL. */
struct tm* op_gmtime(const time_t* clock);
#endif

#ifndef HAVE_MKTIME
/** C99 7.23.2.3: convert the broken-down time to a timestamp */
time_t op_mktime(struct tm* timeptr);
#endif


/* In stdlib_array.cpp */

#ifndef HAVE_QSORT
/** C99 7.20.5.2: sort an array starting at base containing nmemb elements
    of the given size.  The sort is not stable.  compar() is called with
	the addresses of two elements of the array and must return a number
	indicating their relationship.

    qsort() does not allocate memory in the stdlib implementation, but it
	has been known to do so in some platform libraries.  (This can
	occasionally baffle happy-malloc.)  An OOM in qsort is not signalled
	in any way (except perhaps through errno).

    The Opera implementation of this function does not allocate any memory.  */
void op_qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*));
#endif

#ifndef HAVE_QSORT_S
/** Like op_qsort, but allows passing an argument to the sort
	function. To be used when the comparison function requires data
	that is not part of the elements to sort. Due to the extra
	argument the comparison function does not need to use global
	state, and this function is thus reentrant.

    The function signature of op_qsort_s matches that of Microsoft's
	qsort_s. Note that this differs from POSIX qsort_r, which in turn
	differs from qsort_r on FreeBSD and Mac. See section
	SYSTEM_QSORT_S in modules/hardcore/base/system.txt for more
	information.

    The Opera implementation of this function does not allocate any memory.  */
void op_qsort_s(void* base, size_t nmemb, size_t size, int (*compar)(void*, const void*, const void*), void* arg);
#endif

#ifndef HAVE_BSEARCH
/** C99 7.20.5.1: search a partitioned array starting at base containing nmemb
    elements of the given size for the given key.  compar() is called with
	key and the address of an element of the array (in that order) and must
	return a number indicating their relationship.

    The array is partitioned in the sense that all elements less than key
    precede all elements equal to key, which in turn precede all elements
    greater than key.

    Returns a pointer to an element matching KEY, or NULL if there is no such
    element.  */
void* op_bsearch(const void* key, const void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*));
#endif


/* In stdlib_locale.cpp */

#ifndef HAVE_SETLOCALE
/** C99 ?.?.?.?: set the locale for the given category to 'locale'. */
static inline char* op_setlocale(int category, const char* locale)
{
	return const_cast<char *>("C");
}
#endif


/* In stdlib_printf.cpp */

/** OOM NOTE: the printf functions may allocate memory for working
    storage, especially when converting floating-point numbers.  An
	error is signalled by a return value of 0, but this is not
	reliable.

    INCOMPATIBILITY NOTE: sprintf, snprintf, vsprintf, and vsnprintf
    deviate from C99 in the following way:

    <ul>
	<li>  The size specifiers "hh", "j", "z", "t", "ll", and "L" may
	      not be supported in all implementations

	<li>  The format specifiers "a" and "A" may not be supported in
	      all implementations

    <li>  The locale-specific decimal point is never used: the
	      decimal point is always ".".  On systems that rely on the
		  native sprintf() for formatting numbers--typically Symbian
		  builds for Nokia--there are some workaround in src/stdlib_strtod.cpp
		  to try to convert other decimal point representations to "."
		  following the use of sprintf().  Those workarounds are not
		  exhaustive, and will be improved as necessary.

    <li>  Even though C99 does not specify it, the Single Unix Specification
	      allows the single-quote character "'" to be used to signify
		  that formatting should include thousands separators, eg "%'f".  We
		  do not support this.
	</ul>

    We do not mandate a particular printed representation of infinity
    and nan.  Client code that needs specific behavior for these values
    should use special-case code to test for these values.

    Note that these incompatibilities probably bring the printf functions
    closer to C89 than to C99.

    Though C99 7.19.6.1.13 says that the printed representation "should"
    be the shortest representation that allows the number to be represented
    exactly, in practice many printfs do not obey that.  Client code
    might want to be a little paranoiod about that if it matters.  However,
    if FEATURE_3P_DTOA_DAVID_M_GAY or FEATURE_3P_FLOITSCH_DOUBLE_CONVERSION
    is defined then thirdparty-provided code will be used. Both libraries
    are known to output the shortest form. */

#ifndef HAVE_SPRINTF
/** C99 7.19.6.5: format output to buffer, with arguments passed directly.  */
int op_sprintf(char *buffer, const char *format, ...);
#endif

#ifndef HAVE_SNPRINTF
/** C99 7.19.6.5: format output to buffer, limited by a buffer size */
int op_snprintf(char *buffer, size_t count, const char *format, ...);
#endif

#ifndef HAVE_VSPRINTF
/** C99 7.19.6.13: format output to buffer, with arguments passed in a list */
int op_vsprintf(char *buffer, const char *format, va_list argptr);
#endif

#ifndef HAVE_VSNPRINTF
/** C99 7.19.6.12: format output to buffer, limited by a buffer size, with
    arguments passed in a list */
int op_vsnprintf(char *buffer, size_t count, const char *format, va_list argptr);
#endif

#ifndef HAVE_SSCANF
/** C99 7.19.6.7: read input according to format from buffer.

    OOM note: sscanf may allocate heap memory for working storage, and
	returns 0 if such an allocation failed.  But this is not reliable.

    INCOMPATIBILITY NOTE: sscanf deviates from C99 in the following way:

    <ul>
	<li>  The size specifiers "hh", "j", "z", "t", "ll", and "L" may
	      not be supported in all implementations

	<li>  The string nan(n-char-sequence) may not be recognized in all
	      implementations

	<li>  The format specifiers "a" and "A" may not be supported in
	      all implementations

    <li>  Whatever the spec says: we do not accept as input any other
	      characters for the decimal point than ".".

    <li>  Whatever the spec says: we do not accept as input any
	      thousands separators at all.
    </ul>

    We do not mandate whether Inf or Infinity is recognized as infinity,
	client code that needs specific behavior needs to handle this.

	Note that these incompatibilities probably brings sscanf closer to
    C89 than to C99.  */
int op_sscanf(const char *buffer, const char *format, ...);
#endif

#ifndef HAVE_VSSCANF
/** C99 7.19.6.14: read from buffer according to format, with arguments passed
    in a list.

    Note: See documentation of op_sscanf, as the same scanning engine is used
    internally. */
int op_vsscanf(const char *buffer, const char *format, va_list arg_ptr);
#endif


/* Defined here only */

#ifndef HAVE_TOLOWER
/** C99 7.4.2.1: if isupper(c) is true then return a lowercase equivalent
    of c, otherwise return c.

    INCOMPATIBILITY NOTE: In Opera, this operates on the entire Unicode range.  */
static inline int op_tolower(unsigned int c)
{
	// This function pays of to inline for ASCII operation

	if ( c < 128 )
	{
		if ( c >= 'A' && c <= 'Z' )
			c += 0x20;
		return c;
	}

	return Unicode::ToLower((UnicodePoint)c);
}

#endif // !HAVE_TOLOWER

#ifndef HAVE_TOUPPER
/** C99 7.4.2.1: if islower(c) is true then return an uppercase equivalent
    of c, otherwise return c.

    INCOMPATIBILITY NOTE: In Opera, this operates on the entire Unicode range.  */
static inline int op_toupper(int c)
{
	return Unicode::ToUpper((UnicodePoint)c);
}

#endif // !HAVE_TOUPPER

#ifdef __cplusplus

/* The uni_ names are defined in hardcore/unicode/unicode.h */

#define op_isdigit uni_isdigit
#define op_isxdigit uni_isxdigit
#define op_isspace uni_isspace
#define op_isupper uni_isupper
#define op_islower uni_islower
#define op_isalnum uni_isalnum
#define op_isalpha uni_isalpha
#define op_iscntrl uni_iscntrl
#define op_ispunct uni_ispunct

int op_isprint(int c);

#else /* __cplusplus */

/* The functions are defined in stdlib/src/stdlib_string.cpp */

/** Equivalent to uni_isdigit */
int op_isdigit(int c);

/** Equivalent to uni_isxdigit */
int op_isxdigit(int c);

/** Equivalent to uni_isspace */
int op_isspace(int c);

/** Equivalent to uni_isupper */
int op_isupper(int c);

/** Equivalent to uni_islower */
int op_islower(int c);

/** Equivalent to uni_isalnum */
int op_isalnum(int c);

/** Equivalent to uni_isalpha */
int op_isalpha(int c);

/** Equivalent to uni_iscntrl */
int op_iscntrl(int c);

/** Equivalent to uni_ispunct */
int op_ispunct(int c);

int op_isprint(int c);

#endif /* __cplusplus */

}  // extern "C"

/* In stdlib/src/stdlib_unicode.cpp */

/** Convert the specified string in-place from UTF-16 to ISO 8859-1
    single byte by stripping the high byte. The string must be
	nul-terminated, and the resulting string will also be
	nul-terminated.

    Conversion is lossy, avoid using this function unless you know
	what you are doing.

    @param buf Buffer holding uni_char data to be converted to char.
	@return A pointer to the converted string (buf). */
char *make_singlebyte_in_place(char* buf);

/** Convert the first len + 1 characters of the specified string in-place
    from ISO 8859-1 single-byte to UTF-16. The buffer must be able to hold
	at least len + 1 characters. If the source string was nul-terminated
	within len + 1 characters, so will the destination string, to use on a
	regular C-string, pass op_strlen() of the source string as len.

    @param buf Buffer holding char data to be converted to uni_char.
	@param len Length minus one of data to be converted.
	@return A pointer to the converted string (buf). */
uni_char *make_doublebyte_in_place(uni_char* buf, int len);

/** Convert the specified string from UTF-16 to ISO 8859-1
    single-byte (lossy) using the memory manager's TempBufUni.

    @deprecated Conversion is lossy, avoid using this function unless
	you know what you are doing.

    @return A pointer to the converted string (tempbuffer). */
char* make_singlebyte_in_tempbuffer(const uni_char* str, int len);

/** Convert the specified string from ISO 8859-1 single-byte
    to UTF-16 using the memory manager's TempBufUni.

	@deprecated Avoid using this function unless you know what you
	are doing.

    @return A pointer to the converted string (tempbuffer). */
uni_char* make_doublebyte_in_tempbuffer(const char* str, int len);

/** Convert the specified string from UTF-16 to ISO 8859-1 single-byte
    (lossy) in the provided buffer. The result will always be
	nul-terminated. This is the same as calling uni_cstrlcpy().

    Conversion is lossy, avoid using this function unless you know
	what you are doing. */
void make_singlebyte_in_buffer(const uni_char* str, int str_len, char* buf, int buf_len);

/** Convert the specified string from ISO 8859-1 single-byte to UTF-16
    in the provided buffer. The result will always be nul-terminated. */
void make_doublebyte_in_buffer(const char* str, int str_len, uni_char* buf, int buf_len);

/** The number of bytes needed to represent 'len' unicode characters */
#define UNICODE_SIZE(len)		        ((len)*(int)sizeof(uni_char))

/** The number of unicode character that will fit in 'len' bytes of buffer */
#define UNICODE_DOWNSIZE(len)	        ((len)/(int)sizeof(uni_char))


/* In stdlib/src/stdlib_string.cpp */

/**
 * Compares two Uniode strings for equality.
 * @returns negative if str1 is smaller than str2, 0 if equal, positive otherwise
 */
int uni_strcmp(const uni_char* str1, const uni_char* str2) PURE_FUNCTION;

/** Compares a Unicode and an ASCII string for equality.
 * @returns negative if str1 is smaller than str2, 0 if equal, positive otherwise
 */
int uni_strcmp(const uni_char* str1, const char* str2) PURE_FUNCTION;

/** Compares an ASCII and a Unicode string for equality.
 * @returns negative if str1 is smaller than str2, 0 if equal, positive otherwise
 */
static inline int uni_strcmp(const char* str1, const uni_char* str2) {
	return -uni_strcmp( str2, str1 );
}

/**
 * Compares two unicode strings for equality, with a limit on the number
 * of characters to compare.
 * @returns negative if str1 is smaller than str2, 0 if equal, positive otherwise
 */
int uni_strncmp(const uni_char* str1, const uni_char* str2, size_t n) PURE_FUNCTION;

/**
 * Compares a unicode and an ASCII string for equality, with a limit on the number
 * of characters to compare.
 * @returns negative if str1 is smaller than str2, 0 if equal, positive otherwise
 */
int uni_strncmp(const uni_char* str1, const char* str2, size_t n) PURE_FUNCTION;

/**
 * Compares an ASCII and a unicode string for equality, with a limit on the number
 * of characters to compare.
 * @returns negative if str1 is smaller than str2, 0 if equal, positive otherwise
 */
static inline int uni_strncmp(const char* str1, const uni_char* str2, size_t n) {
	return -uni_strncmp( str2, str1, n );
}

/**
 * Compares two unicode strings for equality, case insensitively.
 * @returns negative if str1 is smaller than str2, 0 if equal, positive otherwise
 */
int uni_stricmp(const uni_char* str1, const uni_char* str2) PURE_FUNCTION;

/**
 * Compares a unicode and an ASCII string for equality, case insensitively.
 * @returns negative if str1 is smaller than str2, 0 if equal, positive otherwise
 */
int uni_stricmp(const uni_char* str1, const char* str2) PURE_FUNCTION;

/**
 * Compares an ASCII and a unicode string for equality, case insensitively.
 * @returns negative if str1 is smaller than str2, 0 if equal, positive otherwise
 */
static inline int uni_stricmp(const char* str1, const uni_char* str2) {
	return -uni_stricmp( str2, str1 );
}

/**
 * Compares two unicode strings for equality, case insensitively, with a limit on
 * the number of characters to compare.
 * @returns negative if str1 is smaller than str2, 0 if equal, positive otherwise
 */
int uni_strnicmp(const uni_char* str1, const uni_char* str2, size_t n) PURE_FUNCTION;

/**
 * Compares a unicode and an ASCII string for equality, case insensitively, with a
 * limit on the number of characters to compare.
 * @returns negative if str1 is smaller than str2, 0 if equal, positive otherwise
 */
int uni_strnicmp(const uni_char* str1, const char* str2, size_t n) PURE_FUNCTION;

/**
 * Compares an ASCII and a unicode string for equality, case insensitively, with a
 * limit on the number of characters to compare.
 * @returns negative if str1 is smaller than str2, 0 if equal, positive otherwise
 */
static inline int uni_strnicmp(const char* str1, const uni_char* str2, size_t n) {
	return -uni_strnicmp( str2, str1, n );
}

/** Compares a Unicode string and an ASCII string for equality.
  * @returns TRUE if they are equal, FALSE if they are not.
  */
BOOL uni_str_eq( const uni_char *str1, const char *str2 ) PURE_FUNCTION;

/** Compares a two unicode strings for equality.
  * @returns TRUE if they are equal, FALSE if they are not.
  */
BOOL uni_str_eq( const uni_char *str1, const uni_char *str2 ) PURE_FUNCTION;

/** Compares an ASCII string and a unicode string for equality.
  * @returns TRUE if they are equal, FALSE if they are not.
  */
static inline BOOL uni_str_eq( const char *str1, const uni_char *str2 )
{
	return uni_str_eq( str2,str1);
}

/** Compares a Unicode string and an 8-bit or unicode string case-insensitively.
  *
  * @param str1 A Unicode string in any combination of cases.
  * @param str2 An ASCII string or unicode string
  * @returns TRUE if the strings are equal, FALSE otherwise.
  */
#define uni_stri_eq(str1,str2) (BOOL(0==uni_stricmp((str1),(str2))))

/** Compares the first n characters of a Unicode string and
  * an 8-bit string, case-insensitively.
  * @param str1 A Unicode or ASCII string in any combination of cases.
  * @param str2 An ASCII or unicode string in any combination of cases.
  * @param len The number of characters to compare.
  * @returns TRUE if the strings are equal, FALSE otherwise.
  */
#define uni_strni_eq(str1,str2,len) (BOOL(0==uni_strnicmp((str1),(str2),(len))))

/**
 * Compares a Unicode string and an 8-bit or unicode string case-insensitively.
 *
 * @param str1 A Unicode string in any combination of cases.
 * @param str2 An ASCII string all-lowercase
 * @returns TRUE if the strings are equal, FALSE otherwise.
 */
BOOL uni_stri_eq_lower(const uni_char* str1, const char* str2);

/**
 * Compares a Unicode string and an 8-bit or unicode string case-insensitively.
 *
 * @param str1 A Unicode string in any combination of cases.
 * @param str2 An ASCII string, all-uppercase
 * @returns TRUE if the strings are equal, FALSE otherwise.
 */
BOOL uni_stri_eq_upper(const uni_char* str1, const char* str2);

/** Compares the first n characters of two strings,
  * case-insensitively. The 2nd string is assumed to be in
  * lower-case. This function is much much faster than the
  * standard library functions.
  *
  * @param str1 A Unicode string in any combination of cases.
  * @param str2 An 8bit string in all lowercase.
  * @param len The number of characters to compare.
  * @returns TRUE if the strings are equal, FALSE otherwise.
  */
BOOL uni_strni_eq_lower(const uni_char* str1, const char* str2, size_t len);

/** Compares the first n characters of two strings,
  * case-insensitively. The 2nd string is assumed to be in
  * lower-case. This function is much much faster than the
  * standard library functions.
  *
  * @param str1 A Unicode string in any combination of cases.
  * @param str2 A Unicode string in all lowercase.
  * @param len The number of characters to compare.
  * @returns TRUE if the strings are equal, FALSE otherwise.
  */
BOOL uni_strni_eq_lower(const uni_char* str1, const uni_char* str2, size_t len);

/** Compares the first n characters of two Unicode or 8bit strings,
  * case-insensitively. The 2nd string is assumed to be in
  * upper-case. This function is much much faster than the
  * standard library functions.
  * @param str1 A Unicode string in any combination of cases.
  * @param str2 An 8bit string in all uppercase.
  * @param len The number of characters to compare.
  * @returns TRUE if the strings are equal, FALSE otherwise.
  */
BOOL uni_strni_eq_upper(const uni_char* str1, const char* str2, size_t len);

/** Compares the first n characters of two Unicode or 8bit strings,
  * case-insensitively. The 2nd string is assumed to be in
  * upper-case. This function is much much faster than the
  * standard library functions.
  * @param str1 A Unicode string in any combination of cases.
  * @param str2 A Unicode string in all uppercase.
  * @param len The number of characters to compare.
  * @returns TRUE if the strings are equal, FALSE otherwise.
  */
BOOL uni_strni_eq_upper(const uni_char* str1, const uni_char* str2, size_t len);

/** Compares the first n characters of two strings,
  * case-insensitively. The 2nd string is assumed to be in
  * lower-case containing only ASCII characters (<128).  No
  * proper unicode case-folding from non-ascii to ascii is
  * performed during the comparison.  This is on purpose.
  *
  * @param str1 A Unicode string in any combination of cases.
  * @param str2 A Unicode string in all lowercase.
  * @param len The number of characters to compare.
  * @returns TRUE if the strings are equal, FALSE otherwise.
  */
BOOL uni_strni_eq_lower_ascii(const uni_char* str1, const uni_char* str2, size_t len);

/**
  * Compares the first n characters of two 7-bit strings,
  * case-insensitively. The second parameter is assumed to
  * be all upper-case.
  *
  * Warning: only 7bit strings are handled correctly
  *
  * @param str1 An ASCII string in any combination of cases.
  * @param str2 An ASCII string in all uppercase.
  * @param len The number of characters to compare.
  * @returns TRUE if the strings are equal, FALSE otherwise.
  */
BOOL strni_eq(const char* str1, const char* str2, size_t len);

/**
 * Allocates a latin1 copy of the Unicode string by removing
 * the high byte of each character. If str contains characters
 * outside of the range U+0000 - U+00FF, output will be
 * incorrect. The copy is allocated with op_malloc.
 */
char* uni_down_strdup(const uni_char* str);

/**
 * Creates a Unicode copy of the latin1 string. The copy is allocated
 * with op_malloc.
 */
uni_char* uni_up_strdup(const char* str);

/** strlcpy replacement that copies a Unicode string to a char
  * pointer by removing the high byte of each character. If str contains
  * characters outside of the range U+0000 - U+00FF, output will be
  * incorrect. The destination buffer is always nul terminated.
  * @return Number of bytes it tried to write (i.e length of src)
  */
size_t uni_cstrlcpy(char* dest, const uni_char* src, size_t len);

#ifndef HAVE_UNI_STRLCPY
/**
 * Copies the string src to the destination buffer dest of size
 * dest_size (in uni_chars).
 *
 * The resulting string is always NUL terminated except for when
 * dest_size is zero.
 *
 * If the src string is longer than the dest buffer the resulting string
 * will be truncated to fit the dest buffer.
 *
 * A return value greater than or equal to dest_size means that the
 * resulting string was truncated.
 *
 * @param dest the destination buffer.
 * @param src the string to be copied to dest.
 * @param dest_size the size of dest in uni_chars.
 *
 * @return the length of src.
 */
size_t uni_strlcpy(uni_char *dest, const uni_char *src, size_t dest_size);
#endif

#ifndef HAVE_UNI_STRLCAT
/**
 * Appends the string src to the NUL terminated string in the buffer
 * dest of size dest_size (in uni_chars).
 *
 * The resulting string is always NUL terminated except for when
 * dest_size is zero.
 *
 * If the resulting string is longer than the dest buffer it will be
 * truncated to fit the dest buffer.
 *
 * A return value greater than or equal to dest_size means that the
 * resulting string was truncated.
 *
 * @param dest the destination buffer containing a NUL terminated string.
 * @param src the string to be appended to the string in dest.
 * @param dest_size the size of dest in uni_chars.
 *
 * @return the length of dest + src.
 */
size_t uni_strlcat(uni_char * dest, const uni_char * src, size_t dest_size);
#endif

#ifndef HAVE_UNI_STRNCPY
/** C99 7.21.2.4: copy at most n characters from src to dest.  The strings
    may not overlap.  Return dest.

    INCOMPATIBILITY NOTE: the C99 spec requires that dest is NUL-filled
	if less than n bytes is copied.  The Opera implementation of this
	function will append one NUL (as if it copied the terminating NUL of
	src), but will not NUL-fill.  This is an efficiency issue. */
uni_char* uni_strncpy(uni_char* dest, const uni_char* src, size_t n);
#endif

#ifndef HAVE_UNI_STRCAT
/** C99 7.21.3.1: append src to dest.  The strings may not overlap.
    Returns dest.  */
uni_char* uni_strcat(uni_char* dest, const uni_char* src);
#endif

#ifndef HAVE_UNI_STRNCAT
/** C99 7.21.3.1: append not more than n characters of src to dest,
    and append a terminating NUL if one was not copied from src.
    The strings may not overlap.  Returns dest.

    Note that a NUL byte is appended even if n reached 0. */
uni_char* uni_strncat(uni_char* dest, const uni_char* src, size_t len);
#endif

#ifndef HAVE_UNI_STRREV
/** Reverse s in-place */
void uni_strrev(uni_char *s);
#endif

#ifndef HAVE_UNI_STRDUP
/** Return a heap-allocated copy of s, or NULL if memory could not
    be allocated.  The copy is allocated with op_malloc.  */
uni_char* uni_lowlevel_strdup(const uni_char* s);
#endif

#ifndef HAVE_UNI_STRPBRK
/** C99 7.21.5.4: compute the first location in s of any character from
    the string accept, or NULL if none of the characters occur in s */
uni_char* uni_strpbrk(const uni_char* s, const uni_char* accept) PURE_FUNCTION;
#endif

#ifndef HAVE_UNI_STRLEN
/** C99 7.21.6.3: return the number of characters in s before the
    first NUL byte */
size_t uni_strlen(const uni_char* s) PURE_FUNCTION;
#endif

#ifndef HAVE_UNI_STRCHR
/** C99 7.21.5.2: search s for c, returning a pointer to the
    first occurence or NULL if c does not occur in s.  c may be
    the NUL byte. */
uni_char* uni_strchr(const uni_char* s, uni_char c) PURE_FUNCTION;
#endif

#ifndef HAVE_UNI_STRRCHR
/** C99 7.21.5.5: search s for c, returning a pointer to the
    last occurence or NULL if c does not occur in s.  c may be
    the NUL byte. */
uni_char* uni_strrchr(const uni_char* s, uni_char c) PURE_FUNCTION;
#endif

/** C99 7.21.5.7: search the string haystack for the substring needle,
    and return a pointer to its first occurence or NULL if needle is
	not found in haystack.  */
uni_char* uni_strstr(const uni_char* haystack, const uni_char* needle) PURE_FUNCTION;

/** C99 7.21.5.7: search the string haystack for the substring needle,
    and return a pointer to its first occurence or NULL if needle is
	not found in haystack.  */
uni_char* uni_strstr(const uni_char* haystack, const char* needle) PURE_FUNCTION;

/** A case-insensitive version of uni_strstr.
    \see uni_strstr */
const uni_char* uni_stristr(const uni_char* str, const uni_char* cs);

/** A case-insensitive mixed-representation version of op_strstr.
    \see op_strstr */
const uni_char* uni_stristr(const uni_char* str, const char* cs);

#ifndef HAVE_UNI_STRTOK
/** C99 7.21.5.8: break str into a sequence of tokens delimited by a character
    from the string delimit.  */
uni_char* uni_strtok(uni_char* str, const uni_char* delimit);
#endif

#ifndef HAVE_UNI_STRCPY
/** C99 7.21.2.3: copy the string from src to dest, including the NUL.
    The strings may not overlap.  Return dest. */
uni_char* uni_strcpy(uni_char* dest, const uni_char* src);
#endif

#ifndef HAVE_UNI_STRSPN
/** C99 7.21.5.6: compute the length of the initial segment of s consisting
    of characters from the set accept.  */
size_t uni_strspn(const uni_char *s, const uni_char *accept) PURE_FUNCTION;
#endif

#ifndef HAVE_UNI_STRCSPN
/** C99 7.21.5.3: compute the length of the initial segment of s consisting
    of characters not from the set reject.  */
size_t uni_strcspn(const uni_char *s, const uni_char *reject) PURE_FUNCTION;
#endif

#ifndef HAVE_UNI_SPRINTF
/** C99 7.19.6.5: format output to buffer, with arguments passed directly.
    \see op_sprintf for information about restrictions.  */
int uni_sprintf(uni_char* buffer, const uni_char* format, ...);
#endif

#ifndef HAVE_UNI_SNPRINTF
/** C99 7.19.6.5: format output to buffer, limited by a buffer size
    \see op_snprintf for information about restrictions. */
int uni_snprintf(uni_char* buffer, size_t count, const uni_char* format, ...);
#endif

#ifndef HAVE_UNI_VSPRINTF
/** C99 7.19.6.13: format output to buffer, with arguments passed in a list
    \see op_vsprintf for information about restrictions. */
int	uni_vsprintf(uni_char *, const uni_char *, va_list);
#endif

#ifndef HAVE_UNI_VSNPRINTF
/** C99 7.19.6.12: format output to buffer, limited by a buffer size, with
    arguments passed in a list
    \see op_vsnprintf for information about restrictions.  */
int	uni_vsnprintf(uni_char *, size_t count, const uni_char *, va_list);
#endif

#ifndef HAVE_UNI_SSCANF
/** C99 7.19.6.7: read input according to format from buffer.
    \see op_sscanf for information about restrictions. */
int uni_sscanf(const uni_char* string, const uni_char* format, ...);
#endif

/** C99 7.20.1.2: convert the initial portion of the string to int */
int uni_atoi(const uni_char* str) PURE_FUNCTION;

/** C99 7.20.1.1: convert initial prefix of str to a double */
double uni_atof(const uni_char* str) PURE_FUNCTION;

#ifndef HAVE_UNI_ITOA
/** Convert the value to its string representation in the given buffer,
    using the given radix, where 2 <= radix <= 36.  The buffer must be
    large enough to hold the value.  Return a pointer to the buffer.

    The buffer is always NUL-terminated.

    If radix is 10 and the value is negative, then the output is '-'
	followed by the representation of the absolute value of 'value'.
    For all other radices, 'value' is treated as unsigned.  */
uni_char* uni_itoa(int i, uni_char * buffer, int radix);
#endif

#ifdef STDLIB_64BIT_STRING_CONVERSIONS
/** Like itoa, but for an 'INT64' value */
uni_char* uni_i64toa(INT64 i, uni_char* buffer, int radix);

/** Like ultoa, but for an 'UINT64' value */
uni_char* uni_ui64toa(UINT64 i, uni_char *buffer, int radix);
#endif // STDLIB_64BIT_STRING_CONVERSIONS

#ifndef HAVE_UNI_LTOA
/** Like itoa, but for a 'long' value */
uni_char* uni_ltoa(long value, uni_char* string, int radix);
#endif

/** Like ltoa, but with a limit on the max number of characters. */
uni_char* uni_ltoan(long value, uni_char* str, int radix, int max);

/** Like ltoa, but for an 'unsigned long' value */
uni_char* uni_ultoa(unsigned long value, uni_char* string, int radix);

/** C99 7.20.1.4: convert the initial portion of the string to long */
long uni_strtol(const uni_char* nptr, uni_char** endptr, int base, BOOL* overflow=NULL);

/** C99 7.20.1.4: convert the initial portion of the string to unsigned long */
unsigned long uni_strtoul(const uni_char* nptr, uni_char** endptr, int base, BOOL* overflow=NULL);

#ifdef STDLIB_64BIT_STRING_CONVERSIONS
/** Not in standard, behavior similar to uni_strtol */
INT64 uni_strtoi64(const uni_char* nptr, uni_char** endptr, int base, BOOL* overflow=NULL);

/** Not in standard, behavior similar to uni_strtoul */
UINT64 uni_strtoui64(const uni_char* nptr, uni_char** endptr, int base, BOOL* overflow=NULL);
#endif // STDLIB_64BIT_STRING_CONVERSIONS

/** C99 7.20.1.3: Parse a floating-point number from a string.  */
double uni_strtod(const uni_char* nptr, uni_char** endptr);

/** Parse a floating-point number from a string, taking no more than nchars
    characters from the input. */
double uni_strntod(const uni_char * nptr, uni_char ** endptr, int nchars);

/** @return TRUE if the argument is a control character */
#define uni_iscntrl(X) Unicode::IsCntrl(X)

/** @return TRUE if the argument is a space character */
#define uni_isspace(X) Unicode::IsSpace(X)

/** Used to decide whether or not a character should be considered
    whitespace in HTML. Its identical to uni_isspace().
    @param X The character to test.
    @returns TRUE if the character is whitespace, FALSE otherwise.  */
#define uni_html_space(X) uni_isspace(X)

/** Return TRUE if the argument is a alphabetical character (this
    includes ideographical characters) */
#define uni_isalpha(X) Unicode::IsAlpha(X)

/** Return TRUE if the argument is a alphabetical or numerical
    character (this includes ideographical characters and non-standard
    digits, such as 1-with-ring-around) */
#define uni_isalnum(X) Unicode::IsAlphaOrDigit(X)

/** Returns TRUE if the argument is a punctuation character */
#define uni_ispunct(X) Unicode::IsCSSPunctuation(X)

/** Return TRUE if the argument is between '0' and '9', inclusively. */
#define uni_isdigit(X) Unicode::IsDigit(X)

/** Return TRUE if the argument is considered a digit (Has the Nd, Nl or No class) */
#define uni_isunidigit(X) Unicode::IsUniDigit(X)

/** Return TRUE if the argument is a lowercase character.
    Only returns TRUE for characters with uppercase and lowercase
    versions. Hence, not for ideographical characters, as an example. */
#define uni_islower(X) Unicode::IsLower(X)

/** Return TRUE if the argument is a uppercase character.
    Only returns TRUE for characters with uppercase and lowercase
    versions. Hence, not for ideographical characters, as an example. */
#define uni_isupper(X) Unicode::IsUpper(X)

/** Convert the argument to it's lowercase form, if any. If none
    exists, returns the argument. */
#define uni_tolower(X) Unicode::ToLower(X)

/** Convert the argument to it's uppercase form, if any. If none
    exists, returns the argument. */
#define uni_toupper(X) Unicode::ToUpper(X)

/** Convert the argument in-place to lowercase */
#define uni_strlwr(X)  Unicode::Lower(X,TRUE)

/** Convert the argument in-place to upperacse */
#define uni_strupr(X)  Unicode::Upper(X,TRUE)

/** Returns TRUE if the argument is smaller than 128. */
#define uni_isascii(c) ((uni_char)(c) < 128)

/** Returns TRUE if the argument is a hex-digit (0-9, a-f and A-F). */
static inline BOOL uni_isxdigit(uni_char c)
{
	return (c>='0'&&c<='9') || (c>='a'&&c<='f') || (c>='A'&&c<='F');
}

/* There are two special whitespace characters in HTML:
    - nbsp, which does not collapse, and which you cannot
      break the line before or after, and
    - idsp, which does not collapse, and which you cannot
      break the line before, but you can break after it.
   The next four macros deal with these two characters. */

/** is this an NBSP character? */
#define uni_isnbsp(c)	                ((c) == 0x00A0 || (c) == 0x202F || (c) == 0xFEFF)

/** is this the ideographic space character? */
#define uni_isidsp(c)					((c) == 0x3000)

/** is this an uncollapsing space? */
#define uni_nocollapse_sp(c)			(uni_isnbsp(c) || uni_isidsp(c) || ((c) >= 0x2000 && (c) <=0x200B))

/** is this a collapsing space? */
#define uni_collapsing_sp(c)			(uni_isspace(c) && !uni_nocollapse_sp(c))

/** Convert the buffer to uppercase. Does not stop at null-characters */
uni_char *uni_bufupr(uni_char *str, size_t nchars);

/** Convert the buffer to lowercase. Does not stop at null-characters */
uni_char *uni_buflwr(uni_char *str, size_t nchars);

/** Convert characters from the unicode string src to utf-8
    in the string dest, where dest can hold maxlen characters.

    The destination is always NUL-terminated, and will be silently
    truncated if the destination buffer is not big enough. To be
    able to tell if a truncation occurred, use the services of the
    encodings module instead.

    The maximum size of dest is uni_strlen(src) * 4 + 1 bytes,
    use a NULL dest parameter to get the exact size in bytes.

	@param dest Destination buffer or NULL to calculate required length.
	@param src Source text in UTF-16 encoding, NUL-terminated.
	@param maxlen Size of dest in bytes (including NUL termination).
    @return number of bytes used or needed for conversion. */
int to_utf8(char* dest, const uni_char * src, int maxlen);

/** convert characters from the utf-8 string src to unicode
    in the string dest, where dest can hold maxlen bytes.

    The destination is always NUL-terminated, and will be silently
    truncated if the destination buffer is not big enough. To be
    able to tell if a truncation occurred, use the services of the
    encodings module instead.

    The maximum size of dest is op_strlen(src) * 2 + 2 bytes,
    use a NULL dest parameter to get the exact size in bytes.

	@param dest Destination buffer or NULL to calculate required length.
	@param src Source text in UTF-8 encoding, NUL-terminated.
	@param maxlen Size of dest in bytes (including NUL termination).
    @return number of bytes used or needed for conversion. */
int from_utf8(uni_char * dest, const char * src, int maxlen);

/** Retrieves the offset of a member from the beginning of its parent structure. */
#ifndef op_offsetof
#define op_offsetof(s,m)   (size_t)&(((s *)0)->m)
#endif // !op_offsetof


#endif // !OPERA_STDLIB_H
