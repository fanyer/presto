/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_STDLIB_STDLIB_MODULE_H
#define MODULES_STDLIB_STDLIB_MODULE_H

#define STDLIB_MODULE_REQUIRED

#ifdef STDLIB_MODULE_REQUIRED

#include "modules/hardcore/opera/module.h"

#ifdef DTOA_DAVID_M_GAY
struct Bigint;
#endif // DTOA_DAVID_M_GAY

#ifdef STDLIB_DTOA_CONVERSION
# if defined(DTOA_DAVID_M_GAY) && defined(DTOA_FLOITSCH_DOUBLE_CONVERSION)
# error "DTOA_DAVID_M_GAY and DTOA_FLOITSCH_DOUBLE_CONVERSION are mutually exclusive 3P features"
# endif
#endif // STDLIB_DTOA_CONVERSION

#if defined CORE_GOGI_MEMORY_DEBUG && defined DTOA_DAVID_M_GAY
/** Enable dtoa locking if op_malloc() can call dtoa functions, or op_printf
  * with floating point numbers. */
# define STDLIB_DTOA_LOCKS
#endif // CORE_GOGI_MEMORY_DEBUG || DTOA_DAVID_M_GAY

class StdlibModule : public OperaModule
{
public:
	StdlibModule();

	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();
	virtual BOOL FreeCachedData(BOOL toplevel_context);

#ifndef HAVE_GMTIME
	struct tm	m_gmtime_result;
#endif
#ifndef HAVE_UNI_STRTOK
	uni_char	*m_uni_strtok_datum;
#endif
#ifdef DTOA_DAVID_M_GAY
	Bigint		*m_dtoa_freelist[16];		// Lifted from dtoa.cpp
	Bigint		*m_dtoa_p5s;				// ditto
	char		*m_dtoa_result;				// ditto
# ifdef _DEBUG
	int			m_dtoa_blocks_allocated;	// ditto
	int			m_dtoa_blocks_freed;		// ditto
	int			m_dtoa_blocks[16];			// ditto
# endif // _DEBUG
#endif // DTOA_DAVID_M_GAY
#ifndef HAVE_RAND
# ifdef MERSENNE_TWISTER_RNG
	UINT32		m_mt[624];					// the array for the state vector
	int			m_mti;						// mti==N+1 means mt[N] is not initialized
    UINT32		m_mag01[2];					// working storage for the generator
# else
	int			m_random_state;				// state of simple RNG
# endif // MERSENNE_TWISTER_RNG
#endif // !HAVE_RAND
	double		m_infinity;					// +Infinity
#if !defined STDLIB_DTOA_CONVERSION && !defined HAVE_STRTOD && !defined HAVE_SSCANF
	double		m_negative_infinity;		// -Infinity
#endif
	double		m_quiet_nan;				// QNaN

#ifdef DTOA_FLOITSCH_DOUBLE_CONVERSION
	// dtoa result buffer.
	char        m_dtoa_buffer[33];          // ARRAY OK 2011-10-27 sof
#endif // DTOA_FLOITSCH_DOUBLE_CONVERSION

#ifdef STDLIB_DTOA_LOCKS
	bool LockDtoa(int lock);
	void UnlockDtoa(int lock);
	int m_dtoa_lock;
#endif // STDLIB_DTOA_LOCKS
};

#define g_stdlib_infinity			g_opera->stdlib_module.m_infinity
#define g_stdlib_quiet_nan			g_opera->stdlib_module.m_quiet_nan
#ifdef DTOA_FLOITSCH_DOUBLE_CONVERSION
#define g_stdlib_dtoa_buffer		g_opera->stdlib_module.m_dtoa_buffer
#endif // DTOA_FLOITSCH_DOUBLE_CONVERSION

#endif // STDLIB_MODULE_REQUIRED

#endif // !MODULES_STDLIB_STDLIB_MODULE_H
