/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/layout/numbers.h"
#include "modules/style/css.h"

static int
MakeDecimalStr(int number, uni_char* buf, int buf_len)
{
	if (buf_len > 32)
	{
		uni_ltoan(number, buf, 10, 31);
		return uni_strlen(buf);
	}
	return 0;
}

CONST_ARRAY(DecimalLeadingZeroStr, uni_char*)
	CONST_ENTRY(UNI_L("-09")),
	CONST_ENTRY(UNI_L("-08")),
	CONST_ENTRY(UNI_L("-07")),
	CONST_ENTRY(UNI_L("-06")),
	CONST_ENTRY(UNI_L("-05")),
	CONST_ENTRY(UNI_L("-04")),
	CONST_ENTRY(UNI_L("-03")),
	CONST_ENTRY(UNI_L("-02")),
	CONST_ENTRY(UNI_L("-01"))
CONST_END(DecimalLeadingZeroStr)

static int
MakeDecimalLeadingZeroStr(int number, uni_char* buf, int buf_len)
{
	if (number >= -9 && number <= -1)
	{
		if (buf_len > 3)
		{
			uni_strcat(buf, DecimalLeadingZeroStr[number + 9]);
			return 3;
		}
		return 0;
	}
	else
	{
		int str_len = 0;
		if (number >= 0 && number <= 9)
		{
			if (buf_len < 1)
				return 0;

			buf[str_len++] = '0';
			buf[str_len] = '\0';
		}
		return MakeDecimalStr(number, buf + str_len, buf_len - str_len) + str_len;
	}
}

static int
MakeAlphaStr(int number, uni_char* buf, int buf_len, BOOL upper_case)
{
	if (number <= 0)
		return MakeDecimalStr(number, buf, buf_len);

	*buf = '\0';
	int i = 0;
	uni_char base_char = (upper_case) ? 'A' : 'a';
	int n = number - 1;

	while (n >= 0 && i < buf_len - 1)
	{
		for (int j = i; j >= 0; j--)
			buf[j + 1] = buf[j];

		buf[0] = base_char + (uni_char) (n % 26);
		n = (n / 26) - 1;
		i++;
	}

	OP_ASSERT(n < 0);

	return i;
}

CONST_ARRAY(RomanStr, uni_char*)
	CONST_ENTRY(UNI_L("M")),
	CONST_ENTRY(UNI_L("CM")),
	CONST_ENTRY(UNI_L("D")),
	CONST_ENTRY(UNI_L("CD")),
	CONST_ENTRY(UNI_L("C")),
	CONST_ENTRY(UNI_L("XC")),
	CONST_ENTRY(UNI_L("L")),
	CONST_ENTRY(UNI_L("XL")),
	CONST_ENTRY(UNI_L("X")),
	CONST_ENTRY(UNI_L("IX")),
	CONST_ENTRY(UNI_L("V")),
	CONST_ENTRY(UNI_L("IV")),
	CONST_ENTRY(UNI_L("I")),
	CONST_ENTRY(UNI_L(""))
CONST_END(RomanStr)

const unsigned short RomanInt[ROMAN_SIZE] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1, 0};

int
MakeRomanStr(int number, uni_char* buf, int buf_len, BOOL upper_case)
{
	if (number <= 0)
		return MakeDecimalStr(number, buf, buf_len);

	OP_ASSERT(buf_len >= 5);

	if (buf_len < 6)
		return 0;

	int str_len = 0;

	if (number >= 4000)
	{
		uni_strcpy(buf, UNI_L("...."));
		return 4;
	}

	*buf = '\0';
	int n = number;

	while (n > 0)
		for (int j=0; j<ROMAN_SIZE; j++)
			if (n >= RomanInt[j])
			{
				int len = uni_strlen(RomanStr[j]);

				if (len < buf_len)
				{
					uni_strcat(buf + str_len, RomanStr[j]);
					n -= RomanInt[j];
					str_len += len;
					buf_len -= len;
					break;
				}
				else
					return str_len;
			}

	if (!upper_case)
		uni_strlwr(buf);

	return str_len;
}

// this implements the alphabetic numbering system using the characters a-z, but for
// alphabets other than the Latin one.

static int
MakeAlphaStr(int number, uni_char* buf, int buf_len, const uni_char* digits, int digitCount)
{
	if (number <= 0)
		return MakeDecimalStr(number, buf, buf_len);

	*buf = '\0';
	int i = 0;
	int n = number - 1;

	while (n >= 0 && i < buf_len - 1)
	{
		for (int j = i; j >= 0; j--)
			buf[j+1] = buf[j];

		buf[0] = digits[(int)(n % digitCount)];
		n = (n / digitCount) - 1;
		i++;
	}

	OP_ASSERT(n < 0);

	return i;
}

const short numericValues[] =
{
	20000, 10000, 9000, 8000, 7000, 6000, 5000, 4000, 3000, 2000, 1000, 900, 800, 700, 600, 500, 400,
	300, 200, 100, 90, 80, 70, 60, 50, 40, 30, 20, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, -32000
};

// this implements numbering in the system used by Armenian and Georgian.
// The numeric values for the letters are taken from the book 'Writing
// systems of the world', by Akira Nakanishi.

static int
MakeAlphaNumeralStr(int number, uni_char* buf, int buf_len, const uni_char *letters)
{
	OP_ASSERT(buf_len > 0);
	OP_ASSERT(number >= 0);

	int str_len = 0;
	int ix = 0;

	while (number && str_len < buf_len - 1)
	{
		for (; number < numericValues[ix]; ix++)
			;

		buf[str_len++] = letters[ix];
		number -= numericValues[ix];
	}

	OP_ASSERT(!number);

	buf[str_len] = 0;

	return str_len;
}

const uni_char lowerGreek[] = {
	0x3B1, 0x3B2, 0x3B3, 0x3B4, 0x3B5, 0x3B6, 0x3B7, 0x3B8, 0x3B9, 0x3BA, 0x3BB, 0x3BC,
	0x3BD, 0x3BE, 0x3BF, 0x3C0, 0x3C1, 0x3C3, 0x3C4, 0x3C5, 0x3C6, 0x3C7, 0x3C8, 0x3C9
};

// Note: Greek only exists in the lowercase variety in CSS2
static int MakeGreekStr(int number, uni_char* buf, int buf_len, BOOL upper_case) {
	return MakeAlphaStr(number, buf, buf_len, (const uni_char*) &lowerGreek, 24);
}

const uni_char armenianLetters[] = {
	0x556, 0x555, 0x554, 0x553, 0x552, 0x551, 0x550, 0x54F, 0x54E, 0x54D, 0x54C, 0x54B, 0x54A, 0x549,
	0x548, 0x547, 0x546, 0x545, 0x544, 0x543, 0x542, 0x541, 0x540, 0x53F, 0x53E, 0x53D, 0x53C, 0x53B,
	0x53A, 0x539, 0x538, 0x537, 0x536, 0x535, 0x534, 0x533, 0x532, 0x531, '?'
};

const uni_char georgianLetters[] = {
	'?', 0x10F5, 0x10F0, 0x10EF, 0x10F4, 0x10EE, 0x10ED, 0x10EC, 0x10EB, 0x10EA, 0x10E9, 0x10E8,
	0x10E7, 0x10E6, 0x10E5, 0x10E4, 0x10E3, 0x10E2, 0x10E1, 0x10E0, 0x10DF, 0x10DE, 0x10DD, 0x10F2,
	0x10DC, 0x10DB, 0x10DA, 0x10D9, 0x10D8, 0x10D7, 0x10F1, 0x10D6, 0x10D5, 0x10D4, 0x10D3, 0x10D2,
	0x10D1, 0x10D0, '?'
};

// Note: Armenian only exists in the uppercase variety in CSS2
static int
MakeArmenianStr(int number, uni_char* buf, int buf_len, BOOL upper_case)
{
	// From the CSS3 spec:
	// "Numbers outside the range of the armenian system are rendered using the decimal numbering style."
	// The CSS3 spec uses a different number system from CSS2. We implement the CSS2 system which
	// is limited at about 39999.
	if (number < 1 || number > 39999)
		return MakeDecimalStr(number, buf, buf_len);

	return MakeAlphaNumeralStr(number, buf, buf_len, armenianLetters);
}

// Note: Georgian exists in two varieties: Khutsuri (dead) and
// Mkhedruli (still alive). Mkhedruli only has one case, while
// Khutsuri had two. We implement Mkhedruli, while Mozilla
// implements upper-case Khutsuri...

static int
MakeGeorgianStr(int number, uni_char* buf, int buf_len)
{
	// From the CSS3 spec:
	// "Numbers outside the range of the georgian system are rendered using the decimal numbering style."
	if (number < 1 || number > 19999)
		return MakeDecimalStr(number, buf, buf_len);

	return MakeAlphaNumeralStr(number, buf, buf_len, georgianLetters);
}

int
MakeListNumberStr(int number, short list_type, const uni_char* add_str, uni_char* buf, int buf_len, ListNumberStrMode mode)
{
	*buf = '\0';
	int str_len = 0;

#ifdef SUPPORT_TEXT_DIRECTION
	if (mode != LIST_NUMBER_STR_MODE_NO_CONTROL)
	{
		if (buf_len < 1)
			return str_len;

		// RLE or LRE. Force correct directional embedding for the whole numeration string.
		buf[str_len++] = mode == LIST_NUMBER_STR_MODE_RTL ? 0x202B : 0x202A;
		buf[str_len] = '\0';
	}
#else // !SUPPORT_TEXT_DIRECTION
	(void) mode; // Silence compiler warning about unused param.
#endif // SUPPORT_TEXT_DIRECTION

	switch (list_type)
	{
	case CSS_VALUE_disc:
	case CSS_VALUE_circle:
	case CSS_VALUE_square:
	case CSS_VALUE_box:
	case CSS_VALUE_none:
		break;

	case CSS_VALUE_lower_alpha:
	case CSS_VALUE_lower_latin:
		str_len += MakeAlphaStr(number, buf + str_len, buf_len - str_len, FALSE);
		break;

	case CSS_VALUE_upper_alpha:
	case CSS_VALUE_upper_latin:
		str_len += MakeAlphaStr(number, buf + str_len, buf_len - str_len, TRUE);
		break;

	case CSS_VALUE_lower_roman:
		str_len += MakeRomanStr(number, buf + str_len, buf_len - str_len, FALSE);
		break;

	case CSS_VALUE_upper_roman:
		str_len += MakeRomanStr(number, buf + str_len, buf_len - str_len, TRUE);
		break;

	case CSS_VALUE_lower_greek:
		str_len += MakeGreekStr(number, buf + str_len, buf_len - str_len, FALSE);
		break;

	case CSS_VALUE_armenian:
		str_len += MakeArmenianStr(number, buf + str_len, buf_len - str_len, TRUE);
		break;

	case CSS_VALUE_georgian:
		str_len += MakeGeorgianStr(number, buf + str_len, buf_len - str_len);
		break;

	case CSS_VALUE_decimal_leading_zero:
		str_len += MakeDecimalLeadingZeroStr(number, buf + str_len, buf_len - str_len);
		break;

	case CSS_VALUE_hebrew:
	case CSS_VALUE_cjk_ideographic:
	case CSS_VALUE_hiragana:
	case CSS_VALUE_katakana:
	case CSS_VALUE_hiragana_iroha:
	case CSS_VALUE_katakana_iroha:
	default:
		// We do not handle these types. CSS 2 12.6.2 says that we
		// should then treat them as "decimal", which we do:

	case CSS_VALUE_decimal:
		str_len += MakeDecimalStr(number, buf + str_len, buf_len - str_len);
		break;
	}

	if (add_str)
	{
		int len = uni_strlen(add_str);

		if (len < buf_len - str_len)
		{
			uni_strcat(buf + str_len, add_str);
			str_len += len;
		}
		else
			return str_len;
	}

#ifdef SUPPORT_TEXT_DIRECTION
	if (mode != LIST_NUMBER_STR_MODE_NO_CONTROL && str_len < buf_len)
	{
		// PDF. Pop RLE or LRE.
		buf[str_len++] = 0x202C;
		buf[str_len] = '\0';
	}
#endif // SUPPORT_TEXT_DIRECTION

	return str_len;
}
