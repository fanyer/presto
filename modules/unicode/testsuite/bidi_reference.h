/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef BIDI_REFERENCE_H
#define BIDI_REFERENCE_H

#if defined(SELFTEST) && defined(SUPPORT_TEXT_DIRECTION)

#include "modules/unicode/unicode.h"
#include "modules/unicode/unicode_bidiutils.h"


/** 
 * This file contains headers for utilities used by the bidi selftests.
 * 
 * These utilities operate on a phony alphabet and match phony strings between 
 * bidi reference code and our implementation of the bidi algorithm.
 *
 */

/**
 * Definition of pseudo alphabet
 *
 *
 * BIDI_UNDEFINED	?	// DOES not work ATM		
 * BIDI_L			'a' - 'z'
 * BIDI_AL			'A' - 'M'
 * BIDI_R			'N' - 'Z'
 * BIDI_LRE			<
 * BIDI_LRO			[
 * BIDI_RLE			>
 * BIDI_RLO			]
 * BIDI_PDF			^
 * BIDI_EN			'0' - '4'
 * BIDI_ES			'.', '/'
 * BIDI_ET			'+', '$'
 * BIDI_AN			'5' - '9'
 * BIDI_CS			',', ':'
 * BIDI_NSM			'`'
 * BIDI_BN			(undefined)
 * BIDI_B			'\'
 * BIDI_S			'|'
 * BIDI_WS			(space)
 * BIDI_ON			*,",(,! etc
 *
 */


/* Send in a paragraph of pseudo characters and return a string containing 
 * the designated levels.
 *
 * @param pseudo_chars. Null terminated string of pseudo alphabet
 * @param array containing the reolved levels. One entry per character.
 * @return 
 *
**/

int ResolvePhonyString(int baselevel, const char* phony_str, int* out, int max_chars);

/**
 * Resolve a character in the phony alphabet to an opera BidiCategory
 *
 * @param character
 * @return category
 */

BidiCategory PhonyCharToBidiCategory(char character);

/**
 * Verify a list of ParagraphBidiSegments versus an array of levels
 *
 * Each position in the level array corresponds to one character
 *
 * @param levels Array of levels
 * @param bidi_segments list of segments
 *
 * @return TRUE if the levels match
 */

BOOL VerifyLevelsVersusBidiSegments(int* levels, Head& bidi_segments);


/**
 * Verify the levels of a phony alphabet string against
 * the bidi reference code
 *
 * @param str the string to verify
 *
 * @return TRUE if the levels in the opera implementation and 
 *         the reference code match
 */

BOOL VerifyReferenceVersusCalculation(const char* str, BOOL ltr_paragraph);

#endif // define(SELFTEST) && defined(SUPPORT_TEXT_DIRECTION)

#endif // BIDI_REFERENCE_H
