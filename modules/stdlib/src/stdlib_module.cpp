/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef STDLIB_MODULE_REQUIRED

#include "modules/stdlib/stdlib_module.h"
#include "modules/stdlib/include/double_format.h"
#include "modules/stdlib/src/stdlib_internal.h"
#include "modules/util/handy.h"
#ifdef DTOA_DAVID_M_GAY
#include "modules/stdlib/src/thirdparty_dtoa_dmg/double_format.h"
#endif // DTOA_DAVID_M_GAY

StdlibModule::StdlibModule()
{
#ifndef HAVE_UNI_STRTOK
	m_uni_strtok_datum = NULL;
#endif
#ifdef DTOA_DAVID_M_GAY
	m_dtoa_p5s = NULL;
	m_dtoa_result = NULL;
#endif // DTOA_DAVID_M_GAY
#ifdef DTOA_FLOITSCH_DOUBLE_CONVERSION
	op_memset(m_dtoa_buffer, 0, ARRAY_SIZE(m_dtoa_buffer));
#endif // DTOA_FLOITSCH_DOUBLE_CONVERSION
#ifdef STDLIB_DTOA_LOCKS
	m_dtoa_lock = 0;
#endif // STDLIB_DTOA_LOCKS

	/* Phase 1: set up state */

#ifdef DTOA_DAVID_M_GAY
	for (size_t i = 0 ; i < ARRAY_SIZE(m_dtoa_freelist); i ++)
	{
		m_dtoa_freelist[i] = NULL;
	}

# ifdef _DEBUG
	for (size_t j = 0 ; j < ARRAY_SIZE(m_dtoa_blocks); j ++)
	{
		m_dtoa_blocks[j] = 0;
	}
	m_dtoa_blocks_allocated = 0;
	m_dtoa_blocks_freed = 0;
# endif // _DEBUG
#endif // DTOA_DAVID_M_GAY

#ifndef HAVE_RAND
# ifdef MERSENNE_TWISTER_RNG
	op_memset(m_mt, 0, sizeof(m_mt));
	m_mti = static_cast<int>(ARRAY_SIZE(m_mt) + 1);
    m_mag01[0] = 0;;
	m_mag01[1] = 0x9908b0dfU;
# endif
#endif

	m_infinity = op_implode_double(0x7ff00000U, 0U);
#if !defined STDLIB_DTOA_CONVERSION && !defined HAVE_STRTOD && !defined HAVE_SSCANF
	m_negative_infinity = op_implode_double(0xfff00000UL, 0UL);
#endif
#ifdef QUIET_NAN_IS_ONE
	m_quiet_nan = op_implode_double(0x7fffffffUL, 0xffffffffUL);
#else
	m_quiet_nan = op_implode_double(0x7ff7ffffUL, 0xffffffffUL);
#endif

	/* Phase 2: initialization of subsystems */

	// Cannot call op_srand() directly since it requires that the
	// g_opera pointer is set up correctly, which it might not
	// be until after we return.
#ifndef HAVE_RAND
	/* C99 7.20.2.2 mandates that we should call srand(1) */
# ifdef MERSENNE_TWISTER_RNG
	stdlib_srand_init(1, m_mt, &m_mti);
# else
	stdlib_srand_init(1, &m_random_state);
# endif
#endif

	/* Phase 3: Rudimentary self-tests for typical porting problems. */

	// If you've misconfigured the floating point format for the target platform
	// it will get you here.
#ifdef EPOC
    __ASSERT_ALWAYS(op_implode_double(0x40000000, 0x0) == 2.0, User::Panic(_L("stdlib_module.cpp"), __LINE__));
#else
	OP_ASSERT(op_implode_double( 0x40000000, 0x0) == 2.0);
#endif

#ifndef NAN_EQUALS_EVERYTHING
	// Typically it is a problem under Microsoft Visual C/C++ that NaN compares
	// equal with everything.  This is bizarre and very wrong.  If you hit the
	// assert below you must set SYSTEM_NAN_EQUALS_EVERYTHING = YES.
	//
	// Note you cannot change !(.. == ..) to (.. != ..).
# ifdef EPOC
	__ASSERT_ALWAYS(!(m_quiet_nan == 1.0), User::Panic(_L("stdlib_module.cpp"), __LINE__ ));
# else
	if (m_quiet_nan == 1)
	{
		OP_ASSERT(0);
	}
# endif
#endif

#ifdef NAN_ARITHMETIC_OK
	// On some platforms NaN cannot be used for computation, it will crash the
	// program.  If your program crashes here, then set SYSTEM_NAN_ARITHMETIC_OK = NO.
	double d = m_quiet_nan;
	d = d + 37;
#endif
}

void
StdlibModule::InitL(const OperaInitInfo &)
{
}

void
StdlibModule::Destroy()
{
#ifdef DTOA_DAVID_M_GAY
	stdlib_dtoa_shutdown(1);
#endif // DTOA_DAVID_M_GAY
}

BOOL
StdlibModule::FreeCachedData(BOOL)
{
#ifdef DTOA_DAVID_M_GAY
	// This is safe even while inside Balloc()
	stdlib_dtoa_shutdown(0);
	return TRUE;
#else
	return FALSE;
#endif // DTOA_DAVID_M_GAY
}

#ifdef STDLIB_DTOA_LOCKS
bool StdlibModule::LockDtoa(int lock)
{
	if (0 == lock)
	{
		return 0 == m_dtoa_lock ++;
	}
	return true;
}

void StdlibModule::UnlockDtoa(int lock)
{
	if (0 == lock)
	{
		-- m_dtoa_lock;
	}
}
#endif

#endif // STDLIB_MODULE_REQUIRED
