/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef STDLIB_INTEGER_H
#define STDLIB_INTEGER_H

// Internal header for internal functions

#ifndef HAVE_RAND

# ifdef MERSENNE_TWISTER_RNG
// Defined in "modules/stdlib/src/thirdparty_rng/stdlib_mt19937ar.cpp"
void stdlib_srand_init(UINT32 seed, UINT32 mt_p[], int *mti_p);

# else

// Defined in "modules/stdlib/src/stdlib_integer.cpp"
extern "C" void stdlib_srand_init(UINT32 seed, int *random_state_p);
# endif
#endif

#endif
