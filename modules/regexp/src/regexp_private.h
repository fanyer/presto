/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2002-2006
 *
 * ECMAScript regular expression matcher -- private definitions
 * Lars T Hansen
 *
 * See regexp_advanced_api.cpp for overall documentation.
 */

#ifndef _REGEXP_PRIVATE_H_
#define _REGEXP_PRIVATE_H_

#define REGEXP_EXECUTE_SUPPORT

#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(a)  (sizeof(a)/sizeof(a[0]))
#endif

/* Whitespace character values.
 *
 * Do not use char constants here, they may be sign extended.
 */
static const uni_char CHAR_NUL   = 0x00;
static const uni_char CHAR_BS    = 0x08;
static const uni_char CHAR_HT    = 0x09;
static const uni_char CHAR_LF    = 0x0A;
static const uni_char CHAR_VT    = 0x0B;
static const uni_char CHAR_FF    = 0x0C;
static const uni_char CHAR_CR    = 0x0D;
static const uni_char CHAR_CTRL  = 0x1F;    // Upper limit on traditional control chars
static const uni_char CHAR_NBSP  = 0xA0;    // Non-breaking space mark
// USP is "Unicode space mark" or Unicode class Zs, cf ECMA-262 p11.
static const uni_char CHAR_USP1  = 0x1680;	// Ogham space mark
static const uni_char CHAR_USP2  = 0x2000;	// En quad
static const uni_char CHAR_USP3  = 0x2001;	// Em quad
static const uni_char CHAR_USP4  = 0x2002;	// En space
static const uni_char CHAR_USP5  = 0x2003;	// Em space
static const uni_char CHAR_USP6  = 0x2004;	// Three-per-em space
static const uni_char CHAR_USP7  = 0x2005;	// Four-per-em space
static const uni_char CHAR_USP8  = 0x2006;	// Six-per-em space
static const uni_char CHAR_USP9  = 0x2007;	// Figure space
static const uni_char CHAR_USP10 = 0x2008;	// Punctuation space
static const uni_char CHAR_USP11 = 0x2009;	// Thin space
static const uni_char CHAR_USP12 = 0x200A;	// Hair space
static const uni_char CHAR_USP13 = 0x200B;	// Zero width space
static const uni_char CHAR_LS    = 0x2028;	// Line separator
static const uni_char CHAR_PS    = 0x2029;	// Paragraph separator
static const uni_char CHAR_USP14 = 0x202F;	// Narrow no-break space
static const uni_char CHAR_USP15 = 0x3000;	// Ideographic space

extern const uni_char regexp_space_values[]; // Table of all the space values

/* Formatting character values.
 *
 * Do not use char constants here, they may be sign extended.
 */
static const uni_char CHAR_SHYP  = 0x00AD;  // Soft hyphen
static const uni_char CHAR_ZWNJ  = 0x200C;  // Zero-width non-joiner
static const uni_char CHAR_ZWJ   = 0x200D;  // Zero-width joiner
static const uni_char CHAR_LTR   = 0x200E;  // Left-to-right mark
static const uni_char CHAR_RTL   = 0x200F;  // Right-to-left mark
static const uni_char CHAR_BIDI1 = 0x202A;  // Bidi control
static const uni_char CHAR_BIDI2 = 0x202B;  // Bidi control
static const uni_char CHAR_BIDI3 = 0x202C;  // Bidi control
static const uni_char CHAR_BIDI4 = 0x202D;  // Bidi control
static const uni_char CHAR_BIDI5 = 0x202E;  // Bidi control
static const uni_char CHAR_MATH1 = 0x2061;  // Math operator
static const uni_char CHAR_MATH2 = 0x2062;  // Math operator
static const uni_char CHAR_MATH3 = 0x2063;  // Math operator
static const uni_char CHAR_DEPR1 = 0x206A;  // Deprecated formatting
static const uni_char CHAR_DEPR2 = 0x206B;  // Deprecated formatting
static const uni_char CHAR_DEPR3 = 0x206C;  // Deprecated formatting
static const uni_char CHAR_DEPR4 = 0x206D;  // Deprecated formatting
static const uni_char CHAR_DEPR5 = 0x206E;  // Deprecated formatting
static const uni_char CHAR_DEPR6 = 0x206F;  // Deprecated formatting

extern const uni_char regexp_formatting_values[]; // Table of all the formatting values

inline bool RE_IsIgnorableSpace (int ch)
{
	return uni_strchr(regexp_space_values, ch) || uni_strchr(regexp_formatting_values, ch);
}

#endif // _REGEXP_PRIVATE_H_
