/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Allowed #if-ery: SYSTEM_* defines, *_BOOL, __cplusplus.
 */
#ifndef POSIX_SYS_DECLARE_BOOL_H
#define POSIX_SYS_DECLARE_BOOL_H __FILE__

/** @file declare_bool.h Chose a type to use as BOOL.
 *
 * We should aim to move to using bool wherever possible.  However, assorted
 * Opera code abuses BOOL in a delightful diversity of ways (treating it as a
 * synonym for OP_STATUS, whose semantics are opposite, and even using it to
 * hold a many-bit unsigned int), so such builds are still prone to bugs, some
 * of which are hard to track down.  We should fix that.
 *
 * Generally, test-compiling with perverse alternatives for BOOL is quite a good
 * way of catching code that mis-uses BOOL and perverse suggestions for
 * alternatives are encouraged.
 *
 * Pointer is not an option since x != y doesn't behave nicely as a pointer.
 *
 * Using an enum also doesn't work: we get "cannot convert 'bool' to 'BOOL' in
 * return" messages all over the place; we'd need to change lots of BOOL
 * expressions to say ? TRUE : FALSE to convert themselves to BOOL.  We'd need
 * the same change in order to use a char-sized integral type: we have plenty of
 * code which uses, as a BOOL, expression & MASK of some int-sized type; if MASK
 * & 0xff is zero, this will always evaluate to false.  However, test-compiling
 * with such types can reveal (among the perfectly legitimate warnings due to
 * these types' unsuitability as BOOL) real problems that are worth fixing.
 *
 * See make variable BOOL_TYPE in the unix-build module.
 */

#ifdef  __cplusplus

#ifdef NATIVE_BOOL /* we'd like this to be the default */

typedef bool BOOL;
#define TRUE	true
#define FALSE	false

#elif defined(CLASS_BOOL) /* BOOL debugging class */

class BOOL
{
public:
	BOOL() {}
	BOOL(bool b) : value(b) {}
	operator bool() const { return value; }

	/* The bitwise assignment operators are almost, but not quite
	 * exactly, equivalent to their expanded forms:
	 *
	 * var &= expr; <~> var = expr && var;
	 * var |= expr; <~> var = expr || var;
	 *
	 * The exceptions are e.g. (1 & 2 == 0) and (1 | 2 == 3), but any
	 * such BOOL/int abuse will fail to compile since this class can
	 * only be constructed from bool. */
	BOOL& operator &= (const BOOL& other) { value &= other.value; return *this; }
	BOOL& operator |= (const BOOL& other) { value |= other.value; return *this; }

private:
	/* Require explicit conversion for integer and pointer types. */
	BOOL(int);
	BOOL(void*);

	/* The following unary operators are disallowed. */
	BOOL operator + () const;
	BOOL operator - () const;
	BOOL& operator ++ ();
	BOOL& operator ++ (int);
	BOOL& operator -- ();
	BOOL& operator -- (int);
	BOOL operator ~ () const;

	bool value;
};

/* Ensuring that TRUE/FALSE are of type BOOL helps to avoid ambiguity
 * in some overloaded functions.  Unfortunately it also inhibits GCC's
 * ability to see that these are constant expressions, which in some
 * cases results in warnings due to not being able to statically see
 * which code path(s) will be taken. */
#define TRUE	(BOOL(true))
#define FALSE	(BOOL(false))

/* The bitwise & and | operators have no advantage over && and ||.
 * They do guarantee that both operands are evaluated, but this is
 * obscure and not very useful. */
int operator & (const BOOL&, const BOOL&);
int operator & (const BOOL&, int);
int operator & (int, const BOOL&);
int operator | (const BOOL&, const BOOL&);
int operator | (const BOOL&, int);
int operator | (int, const BOOL&);

#if 0
/* The bitwise ^ operator is equivalent with !=, but which is clearer
 * depends on the context and personal preference. */
int operator ^ (const BOOL&, const BOOL&);
int operator ^ (const BOOL&, int);
int operator ^ (int, const BOOL&);
#endif

/* The arithmetic operators seldom make sense. Using BOOL as int in
 * arithmetic should be done with explicit type conversion. */
int operator + (const BOOL&, const BOOL&);
int operator + (const BOOL&, int);
int operator + (int, const BOOL&);
int operator - (const BOOL&, const BOOL&);
int operator - (const BOOL&, int);
int operator - (int, const BOOL&);
int operator * (const BOOL&, const BOOL&);
int operator * (const BOOL&, int);
int operator * (int, const BOOL&);
int operator / (const BOOL&, const BOOL&);
int operator / (const BOOL&, int);
int operator / (int, const BOOL&);
int operator % (const BOOL&, const BOOL&);
int operator % (const BOOL&, int);
int operator % (int, const BOOL&);

/* The comparison operators (other than == and !=) don't make sense.
 * This may also reveal operator precedence errors. */
bool operator < (const BOOL&, const BOOL&);
bool operator < (const BOOL&, int);
bool operator < (int, const BOOL&);
bool operator <= (const BOOL&, const BOOL&);
bool operator <= (const BOOL&, int);
bool operator <= (int, const BOOL&);
bool operator >= (const BOOL&, const BOOL&);
bool operator >= (const BOOL&, int);
bool operator >= (int, const BOOL&);
bool operator > (const BOOL&, const BOOL&);
bool operator > (const BOOL&, int);
bool operator > (int, const BOOL&);

#elif defined(CHAR_BOOL) /* untried */

typedef char BOOL;
#define TRUE	'\1'
#define FALSE	'\0'

#elif defined(UCHAR_BOOL) /* Catch use of TRUE and FALSE as #if conditionals ... */

typedef unsigned char BOOL;
#define TRUE	((BOOL)(1>0))
#define FALSE	((BOOL)(1<0))

#elif defined(ENUM_BOOL) /* Experimental: elegant simplicity, but unusable. */

typedef enum { FALSE, TRUE } BOOL;

#elif defined(UNSIGNED_BOOL) /* this is surprisingly good at revealing glitches in code */

typedef unsigned int BOOL;
#define TRUE	1U
#define FALSE	0U

#else /* this is what we have traditionally used ... */

typedef int BOOL;
#define TRUE	1
#define FALSE	0

#endif

#else /* C compilation units must not use BOOL */

typedef struct { unsigned int i : 1; } unusable_bool;
#define BOOL unusable_bool

#endif /* __cplusplus */

#endif /* POSIX_SYS_DECLARE_BOOL_H */
