/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

/** @brief This file contains macro expansions for compile-time hashing
  * Below you'll find an inline version of djbhash, suitable for strings
  * of a length up to HASH_LITERAL_DEPTH.
  */

#define DJBHASH_LITERAL_DEPTH 32

// If string is too long, use the runtime function instead.
// Start with djbhash start value, 5381
#define DJBHASH_LITERAL(string) (ARRAY_SIZE(string) - 1 > DJBHASH_LITERAL_DEPTH ? Hash::String(string) : DJBHASH_LITERAL_00(string, (unsigned)5381))

// Execute the hashing function
#define DJBHASH_LITERAL_00(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_01(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_01(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_02(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_02(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_03(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_03(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_04(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_04(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_05(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_05(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_06(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_06(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_07(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_07(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_08(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_08(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_09(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_09(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_10(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_10(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_11(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_11(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_12(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_12(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_13(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_13(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_14(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_14(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_15(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_15(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_16(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_16(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_17(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_17(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_18(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_18(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_19(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_19(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_20(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_20(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_21(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_21(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_22(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_22(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_23(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_23(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_24(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_24(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_25(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_25(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_26(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_26(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_27(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_27(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_28(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_28(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_29(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_29(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_30(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_30(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_31(string+1, (hash * 33 + *(string))))
#define DJBHASH_LITERAL_31(string, hash) (*(string) == 0 ? hash : DJBHASH_LITERAL_32(string+1, (hash * 33 + *(string))))

// Last entry, should have same count as DJBHASH_LITERAL_DEPTH
#define DJBHASH_LITERAL_32(string, hash) (hash)
