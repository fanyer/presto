/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#if !defined ENCODINGS_TESTSUITE_UTILITY_H && (defined SELFTEST || defined ENCODINGS_REGTEST)
#define ENCODINGS_TESTSUITE_UTILITY_H

#ifdef SELFTEST
# include "modules/selftest/testutils.h"
# include "modules/selftest/optestsuite.h"

# define encodings_check(what)											\
	do {																\
		if (!(what))													\
		{																\
			TEST_FAILED(__FILE__, __LINE__, "Condition failed: " #what); \
			return 0;													\
		}																\
	} while(0)
#else
# define encodings_check(what) assert(what)
#endif

int fliputf32endian(const void *src, void *dest, size_t bytes);
int fliputf16endian(const void *src, void *dest, size_t bytes);
int unichar_compare(const void *key, const void *entry);
InputConverter *encodings_new_forward(const char *encoding);
OutputConverter *encodings_new_reverse(const char *encoding);

// Perform a test, converting in both directions
int encodings_perform_test(InputConverter *fw, OutputConverter *rev,
						   const void *input, int inputlen,
						   const void *expected, int expectedlen,
						   const void *expectback = NULL,
						   int expectbacklen = -1,
						   BOOL hasinvalid = FALSE,
						   const uni_char *invalids = NULL);

// Perform several boundary condition tests, converting in both directions
int encodings_perform_tests(const char *encoding,
						    const void *input, int inputlen,
						    const void *expected, int expectedlen,
						    const void *expectback = NULL,
							int expectbacklen = -1,
						    BOOL hasinvalid = FALSE,
						    BOOL mayvary = FALSE,
						    const uni_char *invalids = NULL);

// Test buffer handling
int encodings_test_buffers(const char *encoding,
		                   const void *input, int inputlen,
		                   const void *expected, int expectedlen,
		                   const void *expectback = NULL,
						   int expectbacklen = -1);

// Test handling of invalid characters in OutputConverter
int encodings_test_invalid(const char *encoding,
		                   const uni_char *input, size_t inplen,
		                   const uni_char *invalids, size_t invlen,
		                   int numinvalids, int firstinvalid,
		                   const char *entitystring = NULL,
						   size_t entitylen = 0);
#endif
