/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

// Test bodies for CharConverter tests. These are referenced both from the
// external test suite and the selftests.

#include "core/pch.h"

#if defined SELFTEST || defined ENCODINGS_REGTEST
#include "modules/encodings/testsuite/charconv/chartest.h"
#include "modules/encodings/testsuite/charconv/utility.h"
#include "modules/encodings/tablemanager/optablemanager.h"

#ifdef SELFTEST
# include "modules/encodings/decoders/inputconverter.h"
# include "modules/encodings/encoders/outputconverter.h"
# include "modules/encodings/decoders/utf8-decoder.h"
# include "modules/encodings/encoders/utf8-encoder.h"
# define wcslen(s) uni_strlen(s)
#endif

// Common test data
inline const uni_char *invalid_asian_input() { return UNI_L("Not in Asian: \xE5\xE4\xF6"); }
inline const uni_char *invalids_asian() { return UNI_L("\xE5\xE4\xF6"); }
static const char entity_asian[] = "Not in Asian: &#229;&#228;&#246;";
inline const uni_char *invalid_jis0212_input() { return UNI_L("Not in JIS X 0212: \xAB\xAD\xB2"); }
inline const uni_char *invalids_jis0212() { return UNI_L("\xAB\xAD\xB2"); }
static const char entity_jis0212[] = "Not in JIS X 0212: &#171;&#173;&#178;";
inline const uni_char *tricky_asian_input() { return UNI_L("\x3000\xE5\xE4\xF6"); }
inline const uni_char *tricky_jis0212_input() { return UNI_L("\x3000\xAB\xAD\xB2"); }
inline const uni_char *fullwidth_ascii() { return UNI_L("\xFF00\xFF21\xFF22\xFF23\xFF5E"); }
static const char fullwidth_expect[] = " ABC~";
static const char input22813[] = "~\\";
inline const uni_char *expect22813() { return UNI_L("~\\"); }
inline const uni_char *nonbmp() { return UNI_L("\xD800\xDC00\xd844\xde34"); }
inline const uni_char *input21093() { return UNI_L("\xFF0D\x2212"); }
inline const uni_char *expectunicode21093() { return UNI_L("\xFF0D\xFF0D"); }
static const char input45016[] = "\x7F";
inline const uni_char *expect45016() { return UNI_L("\x7F"); }

#ifdef SELFTEST
# define encodings_buffer     g_opera->encodings_module.buffer
# define encodings_bufferback g_opera->encodings_module.bufferback
#else
extern char *encodings_buffer, *encodings_bufferback;
#endif

// Return size of Unicode string buffer (including nul), in bytes
static size_t unicode_bytelen(const uni_char *s)
{
	return (wcslen(s) + 1) * sizeof (uni_char);
}

// Common code
int encodings_test_iso_8859_1(void)
{
	int rc = 1;
	int read, written;

	const char input1[] = "abcdef\xE6\xF8\xE5";
	const uni_char * const expect1 = UNI_L("abcdef\x00E6\x00F8\x00E5");
	rc &= encodings_perform_tests("iso-8859-1", input1, sizeof input1, expect1, unicode_bytelen(expect1));

	const uni_char * const invalid1_input = UNI_L("Outside latin-1: \x0100\x0101");
	const uni_char * const invalids1 = UNI_L("\x0100\x0101");
	const char entity1[] = "Outside latin-1: &#256;&#257;";
	rc &= encodings_test_invalid("iso-8859-1", invalid1_input, unicode_bytelen(invalid1_input), invalids1, unicode_bytelen(invalids1), 2, 17, entity1, sizeof entity1);

	OutputConverter *fullwidth_conv_latin1 = encodings_new_reverse("iso-8859-1");
	encodings_check(fullwidth_conv_latin1);
	written = fullwidth_conv_latin1->Convert(fullwidth_ascii(), unicode_bytelen(fullwidth_ascii()), encodings_bufferback, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(fullwidth_conv_latin1);
	encodings_check(read == int(unicode_bytelen(fullwidth_ascii())));
	encodings_check(written == sizeof fullwidth_expect);
	encodings_check(op_memcmp(encodings_bufferback, fullwidth_expect, sizeof fullwidth_expect) == 0);

	// CORE-45016
	rc &= encodings_perform_tests("iso-8859-1", input45016, sizeof input45016, expect45016(), unicode_bytelen(expect45016()));

	return rc;
}

int encodings_test_ascii(void)
{
	int rc = 1;
	int read, written;

	const char input1[] = "abcdef\xE6\xF8\xE5";
	const uni_char * const expect1 = UNI_L("abcdef\x00E6\x00F8\x00E5");
	const char back1ascii[] = "abcdef???";
	rc &= encodings_perform_tests("us-ascii", input1, sizeof input1, expect1, unicode_bytelen(expect1), back1ascii, sizeof back1ascii, TRUE, FALSE, UNI_L("\x00E6\x00F8\x00E5"));

	OutputConverter *fullwidth_conv_ascii = encodings_new_reverse("us-ascii");
	encodings_check(fullwidth_conv_ascii);
	written = fullwidth_conv_ascii->Convert(fullwidth_ascii(), unicode_bytelen(fullwidth_ascii()), encodings_bufferback, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(fullwidth_conv_ascii);
	encodings_check(read == int(unicode_bytelen(fullwidth_ascii())));
	encodings_check(written == sizeof fullwidth_expect);
	encodings_check(op_memcmp(encodings_bufferback, fullwidth_expect, sizeof fullwidth_expect) == 0);

	// CORE-45016
	rc &= encodings_perform_tests("us-ascii", input45016, sizeof input45016, expect45016(), unicode_bytelen(expect45016()));

	return rc;
}

int encodings_test_utf8(void)
{
	int rc = 1;
	int read, written;

	// ordinary

	const char input2[] = "abcdef";
	const uni_char * const expect2 = UNI_L("abcdef");
	rc &= encodings_perform_tests("utf-8", input2, sizeof input2, expect2, unicode_bytelen(expect2));

	InputConverter *test2 = encodings_new_forward("utf-8");
	encodings_check(test2);
	written = test2->Convert(input2, sizeof input2, NULL, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test2);
	encodings_check(read == sizeof input2);
	encodings_check(written == int(unicode_bytelen(expect2)));

	// with two-byte escape
	const char input3[] = "Fran\xC3\xA7ois";
	const uni_char * const expect3 = UNI_L("Fran\x00E7ois");
	rc &= encodings_perform_tests("utf-8", input3, sizeof input3, expect3, unicode_bytelen(expect3));

	InputConverter *test3 = encodings_new_forward("utf-8");
	encodings_check(test3);
	written = test3->Convert(input3, sizeof input3, NULL, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test3);
	encodings_check(read == sizeof input3);
	encodings_check(written == int(unicode_bytelen(expect3)));

#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	// end state check
	InputConverter *test3b = encodings_new_forward("utf-8");
	encodings_check(test3b);
	written = test3b->Convert(input3, 5, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
	BOOL is_end_state = test3b->IsValidEndState();
	OP_DELETE(test3b);
	encodings_check(!is_end_state);
#endif

	// with three-byte escape
	const char input5[] = "Haba\xE1\x88\x92xbaba";
	const uni_char * const expect5 = UNI_L("Haba\x1212xbaba");
	rc &= encodings_perform_tests("utf-8", input5, sizeof input5, expect5, unicode_bytelen(expect5));

	InputConverter *test5 = encodings_new_forward("utf-8");
	encodings_check(test5);
	written = test5->Convert(input5, sizeof input5, NULL, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test5);
	encodings_check(read == sizeof input5);
	encodings_check(written == int(unicode_bytelen(expect5)));

#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	// end state check
	InputConverter *test5b = encodings_new_forward("utf-8");
	encodings_check(test5b);
	written = test5b->Convert(input5, 6, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
	is_end_state = test5b->IsValidEndState();
	OP_DELETE(test5b);
	encodings_check(!is_end_state);
#endif

	// with two-byte escape at end-of-string
	const char input40[] = "Fran\xC3\xA7";
	const uni_char * const expect40 = UNI_L("Fran\x00E7");
	rc &= encodings_perform_tests("utf-8", input40, sizeof input40, expect40, unicode_bytelen(expect40));

	InputConverter *test40 = encodings_new_forward("utf-8");
	encodings_check(test40);
	written = test40->Convert(input40, sizeof input40, NULL, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test40);
	encodings_check(read == sizeof input40);
	encodings_check(written == int(unicode_bytelen(expect40)));

	// test surrogate handling
	const char input41[] = "Outside BMP: \xF4\x80\x80\x80";
	const uni_char * const expect41 = UNI_L("Outside BMP: \xDBC0\xDC00");
	rc &= encodings_perform_tests("utf-8", input41, sizeof input41, expect41, unicode_bytelen(expect41));

	InputConverter *test41 = encodings_new_forward("utf-8");
	encodings_check(test41);
	written = test41->Convert(input41, sizeof input41, NULL, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test41);
	encodings_check(read == sizeof input41);
	encodings_check(written == int(unicode_bytelen(expect41)));

	// boundary tests
	//                      <--sb-->U+0080  U+07FF  U+0800 ---->U+FFFF ---->U+10000 ------->U+10FFFF ------>
	const char input78[] = "\x00\x7F\xC2\x80\xDF\xBF\xE0\xA0\x80\xEF\xBF\xBF\xF0\x90\x80\x80\xF4\x8F\xBF\xBF";
	const uni_char expect78[] = { 0x0000, 0x007F, 0x0080, 0x07FF, 0x0800, 0xFFFF, 0xD800, 0xDC00, 0xDBFF, 0xDFFF, 0x0000 };
	rc &= encodings_perform_tests("utf-8", input78, sizeof input78, expect78, sizeof expect78);

	InputConverter *test78 = encodings_new_forward("utf-8");
	encodings_check(test78);
	written = test78->Convert(input78, sizeof input78, NULL, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test78);
	encodings_check(read == sizeof input78);
	encodings_check(written == sizeof expect78);

	//                      <-- U+10000 ---><-- U+103FE ---><-- U+10FC00 --><-- U+10FFFE -->
	const char input82[] = "\xF0\x90\x80\x80\xF0\x90\x8F\xBE\xF4\x8F\xB0\x80\xF4\x8F\xBF\xBE";
	const uni_char * const expect82 = UNI_L("\xD800\xDC00\xD800\xDFFE\xDBFF\xDC00\xDBFF\xDFFE");
	rc &= encodings_perform_tests("utf-8", input82, sizeof input82, expect82, unicode_bytelen(expect82));

	InputConverter *test82 = encodings_new_forward("utf-8");
	encodings_check(test82);
	written = test82->Convert(input82, sizeof input82, NULL, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test82);
	encodings_check(read == sizeof input82);
	encodings_check(written == int(unicode_bytelen(expect82)));

	// CORE-45840: Stress UTF-8 syntax.
	// a) overlong sequences with invalid prefixes: U+003F and U+007F in three
	// bytes should produce U+FFFD per octet.
	const char input45840a[] = "\xC0\xBF\xE0\xC1\xBF\xE0";
	const uni_char * const expect45840a = UNI_L("\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD");
	InputConverter *conv45840a = encodings_new_forward("utf-8");
	encodings_check(conv45840a);
	written = conv45840a->Convert(input45840a, sizeof input45840a, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	int number_of_invalid = conv45840a->GetNumberOfInvalid();
	int first_invalid = conv45840a->GetFirstInvalidOffset();
	OP_DELETE(conv45840a);
	encodings_check(op_memcmp(encodings_buffer, expect45840a, unicode_bytelen(expect45840a)) == 0);
	encodings_check(number_of_invalid > 0);
	encodings_check(first_invalid == 0);
	encodings_check(read == sizeof input45840a);
	encodings_check(written == int(unicode_bytelen(expect45840a)));

	// b) overlong sequences with valid prefixes: U+007F in three and four
	// bytes, U+07FF in three and four bytes and U+FFFF in four bytes should
	// produce one U+FFFD per sequence.
	// overlong sequences       <--- U+007F in 3/4 bytes --><--- U+07FF in 3/4 bytes --><--U+FFFF in 4->
	const char input45840b[] = "\xE0\x81\xBF\xF0\x80\x81\xBF\xE0\x9F\xBF\xF0\x80\x9F\xBF\xF0\x8F\xBF\xBF";
	const uni_char * const expect45840b = UNI_L("\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD");
	InputConverter *conv45840b = encodings_new_forward("utf-8");
	encodings_check(conv45840b);
	written = conv45840b->Convert(input45840b, sizeof input45840b, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	number_of_invalid = conv45840b->GetNumberOfInvalid();
	first_invalid = conv45840b->GetFirstInvalidOffset();
	OP_DELETE(conv45840b);
	encodings_check(op_memcmp(encodings_buffer, expect45840b, unicode_bytelen(expect45840b)) == 0);
	encodings_check(number_of_invalid > 0);
	encodings_check(first_invalid == 0);
	encodings_check(read == sizeof input45840b);
	encodings_check(written == int(unicode_bytelen(expect45840b)));

	// c) characters outside Unicode; four-byte forms beyond valid prefixes
	// and disallowed five and six-byte forms should produce one U+FFFD
	// per octet <-- 0x140000 --><-- 0x200000 ------><-- 0x3FFFFFF -----><-- 0x4000000 ---------><-- 0x7FFFFFFF -------->
	const char input45840c[] = "\xF5\x80\x80\x80\xF8\x82\x80\x80\x80\xFB\xBF\xBF\xBF\xBF\xFC\x84\x80\x80\x80\x80\xFD\xBF\xBF\xBF\xBF\xBF";
	const uni_char * const expect45840c = UNI_L("\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD\xFFFD");
	InputConverter *conv45840c = encodings_new_forward("utf-8");
	encodings_check(conv45840c);
	written = conv45840c->Convert(input45840c, sizeof input45840c, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	number_of_invalid = conv45840c->GetNumberOfInvalid();
	first_invalid = conv45840c->GetFirstInvalidOffset();
	OP_DELETE(conv45840c);
	encodings_check(op_memcmp(encodings_buffer, expect45840c, unicode_bytelen(expect45840c)) == 0);
	encodings_check(number_of_invalid > 0);
	encodings_check(first_invalid == 0);
	encodings_check(read == sizeof input45840c && written == int(unicode_bytelen(expect45840c)));

	// d) characters outside Unicode, but with valid lead byte should
	// produce one U+FFFD
	const char input45840d[] = "\xF4\x90\x80\x80"; // "U+1100000"
	const uni_char * const expect45840d = UNI_L("\xFFFD");
	InputConverter *conv45840d = encodings_new_forward("utf-8");
	encodings_check(conv45840d);
	written = conv45840d->Convert(input45840d, sizeof input45840d, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	number_of_invalid = conv45840d->GetNumberOfInvalid();
	first_invalid = conv45840d->GetFirstInvalidOffset();
	OP_DELETE(conv45840d);
	encodings_check(op_memcmp(encodings_buffer, expect45840d, unicode_bytelen(expect45840d)) == 0);
	encodings_check(number_of_invalid > 0);
	encodings_check(first_invalid == 0);
	encodings_check(read == sizeof input45840d && written == int(unicode_bytelen(expect45840d)));

	// Surrogate codepoints < D800 ---->< DBFF ---->< DC00 ---->< DFFF ---->
	const char input81[] = "\xED\xA0\x80\xED\xAF\xBF\xED\xB0\x80\xED\xBF\xBF";
	const uni_char * const expect81 = UNI_L("\xFFFD\xFFFD\xFFFD\xFFFD");
	InputConverter *conv81 = encodings_new_forward("utf-8");
	encodings_check(conv81);
	written = conv81->Convert(input81, sizeof input81, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	number_of_invalid = conv81->GetNumberOfInvalid();
	first_invalid = conv81->GetFirstInvalidOffset();
	OP_DELETE(conv81);
	encodings_check(op_memcmp(encodings_buffer, expect81, unicode_bytelen(expect81)) == 0);
	encodings_check(number_of_invalid > 0);
	encodings_check(first_invalid == 0);
	encodings_check(read == sizeof input81);
	encodings_check(written == int(unicode_bytelen(expect81)));

	// Correct handling (including resuming parsing at correct position) of
	// invalid byte sequence within otherwise valid text

	// Example from http://www.unicode.org/versions/Unicode5.1.0/#Conformance_Changes
	const char input90[] = "\x41\xc2\xc3\xb1\x42";
	const uni_char * const expect90 = UNI_L("\x41\xFFFD\xf1\x42");
	InputConverter *conv90 = encodings_new_forward("utf-8");
	encodings_check(conv90);
	written = conv90->Convert(input90, sizeof input90, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	number_of_invalid = conv90->GetNumberOfInvalid();
	first_invalid = conv90->GetFirstInvalidOffset();
	OP_DELETE(conv90);
	encodings_check(op_memcmp(encodings_buffer, expect90, unicode_bytelen(expect90)) == 0);
	encodings_check(number_of_invalid == 1);
	encodings_check(first_invalid == 1);
	encodings_check(read == sizeof input90);
	encodings_check(written == int(unicode_bytelen(expect90)));

	// Example from http://unicode.org/reports/tr36/#UTF-8_Exploit
	const char input91[] = "\x41\xc2\x3e\x42";
	const uni_char * const expect91 = UNI_L("\x41\xFFFD\x3e\x42");
	InputConverter *conv91 = encodings_new_forward("utf-8");
	encodings_check(conv91);
	written = conv91->Convert(input91, sizeof input91, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	number_of_invalid = conv91->GetNumberOfInvalid();
	first_invalid = conv91->GetFirstInvalidOffset();
	OP_DELETE(conv91);
	encodings_check(op_memcmp(encodings_buffer, expect91, unicode_bytelen(expect91)) == 0);
	encodings_check(number_of_invalid == 1);
	encodings_check(first_invalid == 1);
	encodings_check(read == sizeof input91);
	encodings_check(written == int(unicode_bytelen(expect91)));

	// Example missing the last byte in a 3-byte sequence
	const char input92[] = "pqr\xe1\x81stu";
	const uni_char * const expect92 = UNI_L("pqr\xFFFDstu");
	InputConverter *conv92 = encodings_new_forward("utf-8");
	encodings_check(conv92);
	written = conv92->Convert(input92, sizeof input92, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	number_of_invalid = conv92->GetNumberOfInvalid();
	first_invalid = conv92->GetFirstInvalidOffset();
	OP_DELETE(conv92);
	encodings_check(op_memcmp(encodings_buffer, expect92, unicode_bytelen(expect92)) == 0);
	encodings_check(number_of_invalid == 1);
	encodings_check(first_invalid == 3);
	encodings_check(read == sizeof input92);
	encodings_check(written == int(unicode_bytelen(expect92)));

	// Buffer calculations
	UTF16toUTF8Converter buffertest_encode;
	UTF8toUTF16Converter buffertest_decode;
	int buffertest_read;
	encodings_check(buffertest_encode.BytesNeeded(expect78, 2,  INT_MAX, &buffertest_read) == 1   && buffertest_read == 2);
	encodings_check(buffertest_decode.Convert(input78, 1,  NULL, INT_MAX, &buffertest_read) == 2  && buffertest_read == 1);
	buffertest_encode.Reset();
	buffertest_decode.Reset();

	encodings_check(buffertest_encode.BytesNeeded(expect78, 4,  INT_MAX, &buffertest_read) == 2   && buffertest_read == 4);
	encodings_check(buffertest_decode.Convert(input78, 2,  NULL, INT_MAX, &buffertest_read) == 4  && buffertest_read == 2);
	buffertest_encode.Reset();
	buffertest_decode.Reset();

	encodings_check(buffertest_encode.BytesNeeded(expect78, 6,  INT_MAX, &buffertest_read) == 4   && buffertest_read == 6);
	encodings_check(buffertest_decode.Convert(input78, 4,  NULL, INT_MAX, &buffertest_read) == 6  && buffertest_read == 4);
	buffertest_encode.Reset();
	buffertest_decode.Reset();

	encodings_check(buffertest_encode.BytesNeeded(expect78, 8,  INT_MAX, &buffertest_read) == 6   && buffertest_read == 8);
	encodings_check(buffertest_decode.Convert(input78, 6,  NULL, INT_MAX, &buffertest_read) == 8  && buffertest_read == 6);
	buffertest_encode.Reset();
	buffertest_decode.Reset();

	encodings_check(buffertest_encode.BytesNeeded(expect78, 10, INT_MAX, &buffertest_read) == 9   && buffertest_read == 10);
	encodings_check(buffertest_decode.Convert(input78, 9,  NULL, INT_MAX, &buffertest_read) == 10 && buffertest_read == 9);
	buffertest_encode.Reset();
	buffertest_decode.Reset();

	encodings_check(buffertest_encode.BytesNeeded(expect78, 12, INT_MAX, &buffertest_read) == 12  && buffertest_read == 12);
	encodings_check(buffertest_decode.Convert(input78, 12, NULL, INT_MAX, &buffertest_read) == 12 && buffertest_read == 12);
	buffertest_encode.Reset();
	buffertest_decode.Reset();

	encodings_check(buffertest_encode.BytesNeeded(expect78, 16, INT_MAX, &buffertest_read) == 16  && buffertest_read == 16);
	encodings_check(buffertest_decode.Convert(input78, 16, NULL, INT_MAX, &buffertest_read) == 16 && buffertest_read == 16);
	buffertest_encode.Reset();
	buffertest_decode.Reset();

	encodings_check(buffertest_encode.BytesNeeded(expect78, 20, INT_MAX, &buffertest_read) == 20  && buffertest_read == 20);
	encodings_check(buffertest_decode.Convert(input78, 20, NULL, INT_MAX, &buffertest_read) == 20 && buffertest_read == 20);
	buffertest_encode.Reset();
	buffertest_decode.Reset();

	// CORE-45016
	rc &= encodings_perform_tests("utf-8", input45016, sizeof input45016, expect45016(), unicode_bytelen(expect45016()));

	return rc;
}

int encodings_test_utf16()
{
	int rc = 1;
	const uni_char * const input55 = UNI_L("Simple ASCII-area only text without BOM");
	const uni_char * const back55 = UNI_L("\xFEFFSimple ASCII-area only text without BOM");

	rc &= encodings_perform_tests("utf-16", input55, unicode_bytelen(input55), input55, unicode_bytelen(input55), back55, unicode_bytelen(back55));
	rc &= encodings_perform_tests("utf-16", back55, unicode_bytelen(back55), input55, unicode_bytelen(input55), back55, unicode_bytelen(back55));

	// Opposite byte order
	uni_char input56[64]; /* ARRAY OK 2011-11-09 peter */
	encodings_check(sizeof input56 >= sizeof input55);
	fliputf16endian(input55, input56, unicode_bytelen(input55));
	rc &= encodings_perform_tests("utf-16", input56, unicode_bytelen(input56), input55, unicode_bytelen(input55), back55, unicode_bytelen(back55));

	// Text with BOM in native encoding
	const uni_char * const input57 = UNI_L("\xFEFF\x3000\x3001\x3002");
	rc &= encodings_perform_tests("utf-16", input57, unicode_bytelen(input57), &input57[1], unicode_bytelen(input57) - sizeof (uni_char));

	// Same test for opposite byte order
	uni_char input59[64]; /* ARRAY OK 2011-11-09 peter */
	encodings_check(sizeof input59 >= sizeof input57);
	fliputf16endian(input57, input59, unicode_bytelen(input57));

	rc &= encodings_perform_tests("utf-16", input57, unicode_bytelen(input57),
	                              &input57[1],
	                              unicode_bytelen(input57) - sizeof (uni_char),
	                              input57, unicode_bytelen(input57));

	return rc;
}

#ifdef ENCODINGS_HAVE_JAPANESE
int encodings_test_iso2022jp(void)
{
	int rc = 1;
	int read, written;

	// basic
	const char input8[] = "abcdef";
	const uni_char * const expect8 = UNI_L("abcdef");

	rc &= encodings_perform_tests("iso-2022-jp", input8, sizeof input8, expect8, unicode_bytelen(expect8));

	// w/ JIS 0208 characters
	const char input9[] = "abc\x1B$B%$\x1B(Bxdef";
	const uni_char * const expect9 = UNI_L("abc\x30A4xdef");

	rc &= encodings_perform_tests("iso-2022-jp", input9, sizeof input9, expect9, unicode_bytelen(expect9));

#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	// end state check
	int i;
	for (i = 4; i <= 10; ++ i)
	{
		InputConverter *test9b = encodings_new_forward("iso-2022-jp");
		encodings_check(test9b);
		written = test9b->Convert(input9, i, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
		BOOL is_end_state = test9b->IsValidEndState();
		OP_DELETE(test9b);
		encodings_check(!is_end_state);
	}
#endif

	// Test that behaviour is conformant to expectations listed in CORE-9031
	const char input11[] = "onm\x1b\x24\x42\x1b\x28\x42ouseover";
	const uni_char * const expect11 = UNI_L("onmouseover");
	const char rev_expect11[] = "onmouseover";
	rc &= encodings_perform_tests("iso-2022-jp", input11, sizeof input11, expect11, unicode_bytelen(expect11), rev_expect11, sizeof rev_expect11);

	// w/ JIS 0201 characters
	const char input13[] = "abc\x1B(Jg\x1B(Bdef";
	const uni_char * const expect13 = UNI_L("abcgdef");
	const char back13[] = "abcgdef";
	rc &= encodings_perform_tests("iso-2022-jp", input13, sizeof input13, expect13, unicode_bytelen(expect13), back13, sizeof back13);

	const char input13b[] = "\x1B(J\\~\x1B(B";
	const uni_char * const expect13b = UNI_L("\xA5\x203E");
	rc &= encodings_perform_tests("iso-2022-jp", input13b, sizeof input13b, expect13b, unicode_bytelen(expect13b), NULL, -1, FALSE, TRUE);

	// code points above last assigned code point
	const char input31[] = "Verre: \x1B$B%$\x74\x7E\x1B(B";
	const uni_char * const expect31 = UNI_L("Verre: \x30a4\xfffd");
	const char back31[] = "Verre: \x1B$B%$\x21\x29\x1B(B";
	// cannot perform the entire encodings_perform_tests suite here since the
	// at-buffer-boundary tests are expected to fail
	rc &= encodings_perform_test(encodings_new_forward("iso-2022-jp"),
	                             encodings_new_reverse("iso-2022-jp"),
	                             input31, sizeof input31,
	                             expect31, unicode_bytelen(expect31),
	                             back31, sizeof back31, TRUE);

	// buffer ending in JIS-0208 mode
	InputConverter *conv39 = encodings_new_forward("iso-2022-jp");
	encodings_check(conv39);
	const char input39[] = "\x1B$B%$";
	const uni_char * const expect39 = UNI_L("\x30a4");
	const char back39[] = "\x1B$B%$\x1B(B";
	// cannot perform standard tests since input is an error condition
	// (input must end in ASCII mode to be complete)
	written =  conv39->Convert(input39, 5, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(conv39);
	encodings_check(op_memcmp(encodings_buffer, expect39, 2) == 0);
	encodings_check(read == 5);
	encodings_check(written == 2);

	OutputConverter *conv39back = encodings_new_reverse("iso-2022-jp");
	encodings_check(conv39back);
	written = conv39back->Convert(encodings_buffer, 2, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(conv39back);
	encodings_check(op_memcmp(encodings_bufferback, back39, 8) == 0);
	encodings_check(read == 2);
	encodings_check(written == 8);

	// half-width katakana (can only be converted FROM Unicode since they
	// are not available in ISO 2022-JP)
	const uni_char * const unicode77 = UNI_L("Kana: \xFF76\xFF85");
	const char expect77[] = "Kana: \x1B$B\x25\x2B\x25\x4A\x1B(B";
	const uni_char * const back77 = UNI_L("Kana: \x30AB\x30CA");
	OutputConverter *conv77 = encodings_new_reverse("iso-2022-jp");
	encodings_check(conv77);
	written = conv77->Convert(unicode77, unicode_bytelen(unicode77), encodings_bufferback, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(conv77);
	encodings_check(read == int(unicode_bytelen(unicode77)));
	encodings_check(written == sizeof expect77);
	encodings_check(op_memcmp(encodings_bufferback, expect77, sizeof expect77) == 0);
	rc &= encodings_perform_tests("iso-2022-jp", expect77, sizeof expect77, back77, unicode_bytelen(back77), NULL, -1, FALSE, TRUE);

	rc &= encodings_test_invalid("iso-2022-jp", invalid_asian_input(), unicode_bytelen(invalid_asian_input()), invalids_asian(), unicode_bytelen(invalids_asian()), 3, 14, entity_asian, sizeof entity_asian);

	const char entity_iso2022jp[] = "\x1B$B\x21\x21\x1B(B&#229;&#228;&#246;";
	rc &= encodings_test_invalid("iso-2022-jp", tricky_asian_input(), unicode_bytelen(tricky_asian_input()),  invalids_asian(), unicode_bytelen(invalids_asian()), 3, 1, entity_iso2022jp, sizeof entity_iso2022jp);

	// We do however allow <ESC>(I to switch to JIS-0201 right mode
	// to read half-width katakana in input
	const char input77[] = "Kana: \x1B(I\x36\x45\x1B(B";
	rc &= encodings_perform_test(encodings_new_forward("iso-2022-jp"),
	                             encodings_new_reverse("iso-2022-jp"),
								 input77, sizeof input77,
								 unicode77, unicode_bytelen(unicode77),
								 expect77, sizeof expect77);

	// Failure tests
	// - Line break where lead byte expected
	const char input84[] = "abc\x1B$B\x0Axdef";
	const uni_char * const expect84 = UNI_L("abc\x0Axdef");

	InputConverter *conv84 = encodings_new_forward("iso-2022-jp");
	encodings_check(conv84);
	op_memset(encodings_buffer, 0, ENC_TESTBUF_SIZE);
	written = conv84->Convert(input84, 11, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	int number_of_invalid = conv84->GetNumberOfInvalid();
	int first_invalid = conv84->GetFirstInvalidOffset();
	OP_DELETE(conv84);
	encodings_check(number_of_invalid > 0);
	encodings_check(first_invalid == 3);
	encodings_check(read == 11);
	encodings_check(0 == op_memcmp(expect84, encodings_buffer, 16));
	encodings_check(written == int(unicode_bytelen(expect84) - 2));

	encodings_test_buffers("iso-2022-jp", input84, sizeof input84, expect84, unicode_bytelen(expect84));

	// - Line break where trail byte expected
	const char input84b[] = "abc\x1B$B%\x0Axdef";
	const uni_char * const expect84b = UNI_L("abc\xFFFDxdef");

	InputConverter *conv84b = encodings_new_forward("iso-2022-jp");
	encodings_check(conv84b);
	written = conv84b->Convert(input84b, 12, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	number_of_invalid = conv84b->GetNumberOfInvalid();
	first_invalid = conv84b->GetFirstInvalidOffset();
	OP_DELETE(conv84b);
	encodings_check(number_of_invalid > 0);
	encodings_check(first_invalid == 3);
	encodings_check(read == 12);
	encodings_check(0 == op_memcmp(expect84b, encodings_buffer, 16));
	encodings_check(written == int(unicode_bytelen(expect84b) - 2));

	encodings_test_buffers("iso-2022-jp", input84b, sizeof input84b, expect84, unicode_bytelen(expect84));

	// DSK-22126, DSK-79804 et.al: Assume single-byte codeset is ASCII
	rc &= encodings_perform_tests("iso-2022-jp", input22813, sizeof input22813,
	                              expect22813(), unicode_bytelen(expect22813()));

	// CORE-21093: U+FF0D and U+2212 should both map to SJIS=817C.
	const char expect21093[] = "\x1B$B\x21\x5D\x21\x5D\x1B(B";
	OutputConverter *test21903 = encodings_new_reverse("iso-2022-jp");
	encodings_check(test21903);
	written = test21903->Convert(input21093(), unicode_bytelen(input21093()),
	                             encodings_bufferback, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test21903);
	encodings_check(read == int(unicode_bytelen(input21093())) && written == sizeof expect21093);
	encodings_check(op_strcmp(encodings_bufferback, expect21093) == 0);

	// ...but forward conversion should prefer the CP932 mapping (U+FF0D)
	InputConverter *test21093i = encodings_new_forward("iso-2022-jp");
	encodings_check(test21093i);
	written = test21093i->Convert(encodings_bufferback, written,
	                              encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test21093i);
	encodings_check(read == sizeof expect21093 && written == int(unicode_bytelen(expectunicode21093())));
	encodings_check(op_memcmp(encodings_buffer, expectunicode21093(), written) == 0);

	// CORE-43484: Decoder should not be greedy
	const uni_char * const expect43484 = UNI_L("\xFFFDxyz");
	// 1. Escape character in trail byte
	const char input43484_1[] = "\x1B$B\x21\x1B(Bxyz";
	InputConverter *test43484_1 = encodings_new_forward("iso-2022-jp");
	encodings_check(test43484_1);
	written = test43484_1->Convert(input43484_1, sizeof input43484_1,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_1);
	encodings_check(read == sizeof input43484_1 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// 2. Truncated escape sequence (invalid first)
	const char input43484_2[] = "\x1Bxyz";
	InputConverter *test43484_2 = encodings_new_forward("iso-2022-jp");
	encodings_check(test43484_2);
	written = test43484_2->Convert(input43484_2, sizeof input43484_2,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_2);
	encodings_check(read == sizeof input43484_2 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// 3. Truncated escape sequence (invalid second; single-byte)
	const char input43484_3[] = "\x1B(xyz";
	InputConverter *test43484_3 = encodings_new_forward("iso-2022-jp");
	encodings_check(test43484_3);
	written = test43484_3->Convert(input43484_3, sizeof input43484_3,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_3);
	encodings_check(read == sizeof input43484_3 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// 4. Truncated escape sequence (invalid second; multi-byte)
	const char input43484_4[] = "\x1B$xyz";
	InputConverter *test43484_4 = encodings_new_forward("iso-2022-jp");
	encodings_check(test43484_4);
	written = test43484_4->Convert(input43484_4, sizeof input43484_4,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_4);
	encodings_check(read == sizeof input43484_4 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// CORE-45016
	rc &= encodings_perform_tests("iso-2022-jp", input45016, sizeof input45016, expect45016(), unicode_bytelen(expect45016()));

	return rc;
}
#endif

#ifdef ENCODINGS_HAVE_JAPANESE
int encodings_test_iso2022jp1(void)
{
	int rc = 1;
	int read, written;

	// basic
	const char input8[] = "abcdef";
	const uni_char * const expect8 = UNI_L("abcdef");

	rc &= encodings_perform_tests("iso-2022-jp-1", input8, sizeof input8, expect8, unicode_bytelen(expect8));

	// w/ JIS 0208 characters
	const char input9[] = "abc\x1B$B%$\x1B(Bxdef";
	const uni_char * const expect9 = UNI_L("abc\x30A4xdef");

	rc &= encodings_perform_tests("iso-2022-jp-1", input9, sizeof input9, expect9, unicode_bytelen(expect9));

#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	// end state check
	int i;
	for (i = 4; i <= 10; ++ i)
	{
		InputConverter *test9b = encodings_new_forward("iso-2022-jp-1");
		encodings_check(test9b);
		written = test9b->Convert(input9, i, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
		BOOL is_end_state = test9b->IsValidEndState();
		OP_DELETE(test9b);
		encodings_check(!is_end_state);
	}
#endif

	// w/ JIS 0201 characters
	const char input13[] = "abc\x1B(Jg\x1B(Bdef";
	const uni_char * const expect13 = UNI_L("abcgdef");
	const char back13[] = "abcgdef";
	rc &= encodings_perform_tests("iso-2022-jp-1", input13, sizeof input13, expect13, unicode_bytelen(expect13), back13, sizeof back13);

	// JIS 0212
	const char input0212[] = "H\x1B$(D\x2B\x29\x1B(Bkon Wium Lie";
	const uni_char * const expect0212 = UNI_L("H\xE5kon Wium Lie");
	rc &= encodings_perform_tests("iso-2022-jp-1", input0212, sizeof input0212, expect0212, unicode_bytelen(expect0212));

	// code points above last assigned code point
	const char input31[] = "Verre: \x1B$B%$\x74\x7E\x1B(B";
	const uni_char * const expect31 = UNI_L("Verre: \x30a4\xfffd");
	const char back31[] = "Verre: \x1B$B%$\x21\x29\x1B(B";
	// cannot perform the entire encodings_perform_tests suite here since the
	// at-buffer-boundary tests are expected to fail
	rc &= encodings_perform_test(encodings_new_forward("iso-2022-jp-1"),
	                             encodings_new_reverse("iso-2022-jp-1"),
	                             input31, sizeof input31,
	                             expect31, unicode_bytelen(expect31),
	                             back31, sizeof back31, TRUE);

	// buffer ending in JIS-0208 mode
	InputConverter *conv39 = encodings_new_forward("iso-2022-jp-1");
	encodings_check(conv39);
	const char input39[] = "\x1B$B%$";
	const uni_char * const expect39 = UNI_L("\x30a4");
	const char back39[] = "\x1B$B%$\x1B(B";
	// cannot perform standard tests since input is an error condition
	// (input must end in ASCII mode to be complete)
	written =  conv39->Convert(input39, 5, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(conv39);
	encodings_check(op_memcmp(encodings_buffer, expect39, 2) == 0);
	encodings_check(read == 5);
	encodings_check(written == 2);

	OutputConverter *conv39back = encodings_new_reverse("iso-2022-jp-1");
	encodings_check(conv39back);
	written = conv39back->Convert(encodings_buffer, 2, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(conv39back);
	encodings_check(op_memcmp(encodings_bufferback, back39, 8) == 0);
	encodings_check(read == 2);
	encodings_check(written == 8);

	// half-width katakana (can only be converted FROM Unicode since they
	// are not available in ISO 2022-JP)
	const uni_char * const unicode77 = UNI_L("Kana: \xFF76\xFF85");
	const char expect77[] = "Kana: \x1B$B\x25\x2B\x25\x4A\x1B(B";
	const uni_char * const back77 = UNI_L("Kana: \x30AB\x30CA");
	OutputConverter *conv77 = encodings_new_reverse("iso-2022-jp-1");
	encodings_check(conv77);
	written = conv77->Convert(unicode77, unicode_bytelen(unicode77), encodings_bufferback, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(conv77);
	encodings_check(read == int(unicode_bytelen(unicode77)));
	encodings_check(written == sizeof expect77);
	encodings_check(op_memcmp(encodings_bufferback, expect77, sizeof expect77) == 0);
	rc &= encodings_perform_tests("iso-2022-jp-1", expect77, sizeof expect77, back77, unicode_bytelen(back77), NULL, -1, FALSE, TRUE);

	// We do however allow <ESC>(I to switch to JIS-0201 right mode
	// to read half-width katakana in input
	const char input77[] = "Kana: \x1B(I\x36\x45\x1B(B";
	rc &= encodings_perform_test(encodings_new_forward("iso-2022-jp-1"),
	                             encodings_new_reverse("iso-2022-jp-1"),
								 input77, sizeof input77,
								 unicode77, unicode_bytelen(unicode77),
								 expect77, sizeof expect77);

//	The characters in invalid_asian_input are available in JIS 0212
//	rc &= encodings_test_invalid("iso-2022-jp-1", invalid_asian_input(), unicode_bytelen(invalid_asian_input()), invalids_asian(), unicode_bytelen(invalids_asian()), 3, 14, entity_asian, sizeof entity_asian);

//	const char entity_iso2022jp[] = "\x1B$B\x21\x21\x1B(B&#229;&#228;&#246;";
//	rc &= encodings_test_invalid("iso-2022-jp-1", tricky_asian_input(), unicode_bytelen(tricky_asian_input()),  invalids_asian(), unicode_bytelen(invalids_asian()), 3, 1, entity_iso2022jp, sizeof entity_iso2022jp);

	rc &= encodings_test_invalid("iso-2022-jp-1", invalid_jis0212_input(), unicode_bytelen(invalid_jis0212_input()), invalids_jis0212(), unicode_bytelen(invalids_jis0212()), 3, 19, entity_jis0212, sizeof entity_jis0212);

	const char entity_iso2022jp1_tricky[] = "\x1B$B\x21\x21\x1B(B&#171;&#173;&#178;";
	rc &= encodings_test_invalid("iso-2022-jp-1", tricky_jis0212_input(), unicode_bytelen(tricky_jis0212_input()), invalids_jis0212(), unicode_bytelen(invalids_jis0212()), 3, 1, entity_iso2022jp1_tricky, sizeof entity_iso2022jp1_tricky);

	// Failure tests
	// - Line break where lead byte expected
	const char input84[] = "abc\x1B$B\x0Axdef";
	const uni_char * const expect84 = UNI_L("abc\x0Axdef");

	InputConverter *conv84 = encodings_new_forward("iso-2022-jp-1");
	encodings_check(conv84);
	written = conv84->Convert(input84, 11, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	int number_of_invalid = conv84->GetNumberOfInvalid();
	int first_invalid = conv84->GetFirstInvalidOffset();
	OP_DELETE(conv84);
	encodings_check(number_of_invalid > 0);
	encodings_check(first_invalid == 3);
	encodings_check(read == 11);
	encodings_check(0 == op_memcmp(expect84, encodings_buffer, 16));
	encodings_check(written == int(unicode_bytelen(expect84) - 2));

	encodings_test_buffers("iso-2022-jp-1", input84, sizeof input84, expect84, unicode_bytelen(expect84));

	// - Line break where trail byte expected
	const char input84b[] = "abc\x1B$B%\x0Axdef";
	const uni_char * const expect84b = UNI_L("abc\xFFFDxdef");

	InputConverter *conv84b = encodings_new_forward("iso-2022-jp-1");
	encodings_check(conv84b);
	written = conv84b->Convert(input84b, 12, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	number_of_invalid = conv84b->GetNumberOfInvalid();
	first_invalid = conv84b->GetFirstInvalidOffset();
	OP_DELETE(conv84b);
	encodings_check(number_of_invalid > 0);
	encodings_check(first_invalid == 3);
	encodings_check(read == 12);
	encodings_check(0 == op_memcmp(expect84b, encodings_buffer, 16));
	encodings_check(written == int(unicode_bytelen(expect84b) - 2));

	encodings_test_buffers("iso-2022-jp-1", input84b, sizeof input84b, expect84, unicode_bytelen(expect84));

	// DSK-22126, DSK-79804 et.al: Assume single-byte codeset is ASCII
	rc &= encodings_perform_tests("iso-2022-jp-1", input22813, sizeof input22813,
	                              expect22813(), unicode_bytelen(expect22813()));

	// CORE-43484: Decoder should not be greedy
	const uni_char * const expect43484 = UNI_L("\xFFFDxyz");
	// 1. Escape character in trail byte
	const char input43484_1[] = "\x1B$B\x21\x1B(Bxyz";
	InputConverter *test43484_1 = encodings_new_forward("iso-2022-jp-1");
	encodings_check(test43484_1);
	written = test43484_1->Convert(input43484_1, sizeof input43484_1,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_1);
	encodings_check(read == sizeof input43484_1 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// 2. Truncated escape sequence (invalid first)
	const char input43484_2[] = "\x1Bxyz";
	InputConverter *test43484_2 = encodings_new_forward("iso-2022-jp-1");
	encodings_check(test43484_2);
	written = test43484_2->Convert(input43484_2, sizeof input43484_2,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_2);
	encodings_check(read == sizeof input43484_2 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// 3. Truncated escape sequence (invalid second; single-byte)
	const char input43484_3[] = "\x1B(xyz";
	InputConverter *test43484_3 = encodings_new_forward("iso-2022-jp-1");
	encodings_check(test43484_3);
	written = test43484_3->Convert(input43484_3, sizeof input43484_3,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_3);
	encodings_check(read == sizeof input43484_3 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// 4. Truncated escape sequence (invalid second; multi-byte)
	const char input43484_4[] = "\x1B$xyz";
	InputConverter *test43484_4 = encodings_new_forward("iso-2022-jp-1");
	encodings_check(test43484_4);
	written = test43484_4->Convert(input43484_4, sizeof input43484_4,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_4);
	encodings_check(read == sizeof input43484_4 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	return rc;
}
#endif

#ifdef ENCODINGS_HAVE_JAPANESE
int encodings_test_shiftjis(void)
{
	int rc = 1;
	int read, written;

	// basic
	const char input14[] = "Hu og hei";
	const uni_char * const expect14 = UNI_L("Hu og hei");
	rc &= encodings_perform_tests("shift_jis", input14, sizeof input14, expect14, unicode_bytelen(expect14));

	// half-width katakana
	const char input15[] = "Kana: \xB6\xC5";
	const uni_char * const expect15 = UNI_L("Kana: \xFF76\xFF85");
	rc &= encodings_perform_tests("shift_jis", input15, sizeof input15, expect15, unicode_bytelen(expect15));

	// unsplit JIS 0208
	const char input16[] = "Ugga: \x82\xa9\x82\xc8\x8a\xbf\x8e\x9a";
	const uni_char * const expect16 = UNI_L("Ugga: \x304b\x306a\x6f22\x5b57");
	rc &= encodings_perform_tests("shift_jis", input16, sizeof input16, expect16, unicode_bytelen(expect16));

#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	// end state check
	int i;
	for (i = 7; i <= 13; i += 2)
	{
		InputConverter *test16b = encodings_new_forward("shift_jis");
		encodings_check(test16b);
		written = test16b->Convert(input16, i, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
		BOOL is_end_state = test16b->IsValidEndState();
		OP_DELETE(test16b);
		encodings_check(!is_end_state);
	}
#endif

	// code points above last assigned code point
	const char input30[] = "Verre: \x82\xa9\xef\xfc";
	const uni_char * const expect30 = UNI_L("Verre: \x304b\xfffd");
	const char back30[]  = "Verre: \x82\xa9?";
	rc &= encodings_perform_tests("shift_jis", input30, sizeof input30,
	                              expect30, unicode_bytelen(expect30),
	                              back30, sizeof back30, TRUE);

	rc &= encodings_test_invalid("shift_jis", invalid_asian_input(), unicode_bytelen(invalid_asian_input()), invalids_asian(), unicode_bytelen(invalids_asian()), 3, 14, entity_asian, sizeof entity_asian);
	const char entity_sjis[] = "\x81\x40&#229;&#228;&#246;";
	rc &= encodings_test_invalid("shift_jis", tricky_asian_input(), unicode_bytelen(tricky_asian_input()),  invalids_asian(), unicode_bytelen(invalids_asian()), 3, 1, entity_sjis, sizeof entity_sjis);

	// DSK-22126, DSK-79804 et.al: Assume single-byte codeset is ASCII
	rc &= encodings_perform_tests("shift_jis", input22813, sizeof input22813,
	                              expect22813(), unicode_bytelen(expect22813()));

	// Test that behaviour is conformant to expectations listed in CORE-9031
	const char input50[] = "on\x80mouseover";
	const uni_char * const expect50 = UNI_L("on\xfffdmouseover");
	const char rev_expect50[] = "on\x3fmouseover";
	rc &= encodings_perform_tests("shift_jis", input50, sizeof input50, expect50, unicode_bytelen(expect50), rev_expect50, sizeof rev_expect50);

	// CORE-21093: U+FF0D and U+2212 should both map to SJIS=817C.
	const char expect21093[] = "\x81\x7C\x81\x7C";
	OutputConverter *test21903 = encodings_new_reverse("shift_jis");
	encodings_check(test21903);
	written = test21903->Convert(input21093(), unicode_bytelen(input21093()),
	                             encodings_bufferback, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test21903);
	encodings_check(read == int(unicode_bytelen(input21093())) && written == sizeof expect21093);
	encodings_check(op_strcmp(encodings_bufferback, expect21093) == 0);

	// ...but forward conversion should prefer the CP932 mapping (U+FF0D)
	InputConverter *test21093i = encodings_new_forward("shift_jis");
	encodings_check(test21093i);
	written = test21093i->Convert(encodings_bufferback, written,
	                              encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test21093i);
	encodings_check(read == sizeof expect21093 && written == int(unicode_bytelen(expectunicode21093())));
	encodings_check(op_memcmp(encodings_buffer, expectunicode21093(), written) == 0);

	// CORE-43416: JIS X 0208 reverse mapping, Microsoft Shift-JIS duplication
	const char input43416[] = "\xEE\xEF\xFA\x40\xEE\xEC\xFC\x4B\xED\xFA\xFB\x59";
	const uni_char * const expect43416 = UNI_L("\x2170\x2170\x9ED1\x9ED1\x71C1\x71C1");
	const char expectback43416[] = "\xEE\xEF\xEE\xEF\xEE\xEC\xEE\xEC\xED\xFA\xED\xFA";
	rc &= encodings_perform_tests("shift_jis", input43416, sizeof input43416,
								  expect43416, unicode_bytelen(expect43416),
								  expectback43416, sizeof expectback43416);

	// CORE-43484: Decoder should not be greedy
	const char input43484[] = "\x82\x22";
	const uni_char * const expect43484 = UNI_L("\xFFFD\x22");
	InputConverter *test43484 = encodings_new_forward("shift_jis");
	encodings_check(test43484);
	written = test43484->Convert(input43484, sizeof input43484, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484);
	encodings_check(read == sizeof input43484 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, unicode_bytelen(expect43484)) == 0);

	// CORE-45016
	rc &= encodings_perform_tests("iso-2022-jp-1", input45016, sizeof input45016, expect45016(), unicode_bytelen(expect45016()));

	return rc;
}
#endif

// These tests are used for both Big5 variants
static const char input18[] = "Dette er en enkel test.";
inline const uni_char *expect18() { return UNI_L("Dette er en enkel test."); }
static const char input19[] = "Dette er \xA2\x5C\xAC\xCC\xC3\x57\xF9\xD5";
inline const uni_char *expect19() { return UNI_L("Dette er \x515D\x75AB\x9BC8\x9F98"); }

#ifdef ENCODINGS_HAVE_CHINESE
int encodings_test_big5(void)
{
	int rc = 1;
	int read, written;

	// simple ASCII
	rc &= encodings_perform_tests("big5", input18, sizeof input18, expect18(), unicode_bytelen(expect18()));

	// ascii and unsplit Big Five
	rc &= encodings_perform_tests("big5", input19, sizeof input19, expect19(), unicode_bytelen(expect19()));

#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	int i;
	// end state check
	for (i = 10; i <= 16; i += 2)
	{
		InputConverter *test19b = encodings_new_forward("big5");
		encodings_check(test19b);
		written = test19b->Convert(input19, i, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
		BOOL is_end_state = test19b->IsValidEndState();
		OP_DELETE(test19b);
		encodings_check(!is_end_state);
	}
#endif

	// code points above last assigned code point
	InputConverter *conv32 = encodings_new_forward("big5");
	encodings_check(conv32);
	written =  conv32->Convert("Verre: \xA2\x5C\xFE\xFE", 12, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	int number_of_invalid = conv32->GetNumberOfInvalid();
	int first_invalid = conv32->GetFirstInvalidOffset();
	OP_DELETE(conv32);

	encodings_check(op_memcmp(encodings_buffer, UNI_L("Verre: \x515D\xfffd"), 20) == 0);
	encodings_check(read == 12);
	encodings_check(written == 20);
	encodings_check(number_of_invalid == 1);
	encodings_check(first_invalid == 8);

	OutputConverter *conv32back = encodings_new_reverse("big5");
	encodings_check(conv32back);
	written = conv32back->Convert(encodings_buffer, 20, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
	number_of_invalid = conv32back->GetNumberOfInvalid();
	first_invalid = conv32back->GetFirstInvalidOffset();
#ifdef ENCODINGS_HAVE_SPECIFY_INVALID
	uni_char invalids[16]; /* ARRAY OK 2012-01-06 peter */
	uni_strlcpy(invalids, conv32back->GetInvalidCharacters(), 16);
#endif
	OP_DELETE(conv32back);
//	assert(op_memcmp(bufferback, "Verre: \xa2\x5c?", 11) == 0 && read == 20 && written == 11);
	encodings_check(number_of_invalid == 1);
	encodings_check(first_invalid == 8);
#ifdef ENCODINGS_HAVE_SPECIFY_INVALID
	encodings_check(op_memcmp(invalids, UNI_L("\xFFFD"), 2 * sizeof (uni_char)) == 0);
#endif

	// Windows codepage 950 extension (not in Unicode.org's mapping table)
	const char input64[] = "\xF9\xD6\xF9\xFE";
	const uni_char * const expect64 = UNI_L("\x7881\x2593");
	rc &= encodings_perform_tests("big5", input64, sizeof input64, expect64, unicode_bytelen(expect64));

	// ETen extension (not in Microsoft's mapping table)
#if 0 // Invalidated by Big5-2003 mapping table
	const char input65[] = "\xC6\xA1\xC7\xFC";
	const uni_char * const expect65 = UNI_L("\x30FE\x247D");
	rc &= encodings_perform_tests("big5", input65, sizeof input65, expect65, unicode_bytelen(expect65));
#endif

	rc &= encodings_test_invalid("big5", invalid_asian_input(), unicode_bytelen(invalid_asian_input()), invalids_asian(), unicode_bytelen(invalids_asian()), 3, 14, entity_asian, sizeof entity_asian);

	// DSK-22126, DSK-79804 et.al: Assume single-byte codeset is ASCII
	rc &= encodings_perform_tests("big5", input22813, sizeof input22813,
	                              expect22813(), unicode_bytelen(expect22813()));

	// CORE-43484: Decoder should not be greedy
	const char input43484[] = "\xA1\x22";
	const uni_char * const expect43484 = UNI_L("\xFFFD\x22");
	InputConverter *test43484 = encodings_new_forward("big5");
	encodings_check(test43484);
	written = test43484->Convert(input43484, sizeof input43484, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484);
	encodings_check(read == sizeof input43484 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, unicode_bytelen(expect43484)) == 0);

	// CORE-45016
	rc &= encodings_perform_tests("big5", input45016, sizeof input45016, expect45016(), unicode_bytelen(expect45016()));

	return rc;
}
#endif

#ifdef ENCODINGS_HAVE_CHINESE
int encodings_test_big5_2003(void)
{
	int rc = 1;

	// A3C0--A3E0: Control pictures
	// A3E1: Euro sign
	const char inputA3[] = "\xA3\xC0\xA3\xDF\xA3\xE0\xA3\xE1";
	const uni_char * const expectA3 = UNI_L("\x2400\x241F\x2421\x20AC");
	rc &= encodings_perform_tests("big5", inputA3, sizeof inputA3, expectA3, unicode_bytelen(expectA3));

	// C6A1--C7F2: Japanese (CORE-1138).
	const char inputC6[] = "\xC6\xA1\xC6\xE7\xC7\xF2";
	const uni_char * const expectC6 = UNI_L("\x2460\x3041\x30F6");
	rc &= encodings_perform_tests("big5", inputC6, sizeof inputC6, expectC6, unicode_bytelen(expectC6));

	return rc;
}
#endif

#ifdef ENCODINGS_HAVE_CHINESE
int encodings_test_big5hkscs(void)
{
	int rc = 1;
#if defined ENCODINGS_HAVE_SPECIFY_INVALID || defined ENCODINGS_HAVE_ENTITY_ENCODING || defined ENCODINGS_HAVE_CHECK_ENDSTATE
	int read, written;
#endif

	// Run standard Big5 tests through HKSCS code
	rc &= encodings_perform_tests("big5-hkscs", input18, sizeof input18, expect18(), unicode_bytelen(expect18()));
	rc &= encodings_perform_tests("big5-hkscs", input19, sizeof input19, expect19(), unicode_bytelen(expect19()));

	// Test string:
	// U+0041 U+0042 U+0043 U+0100 U+2010C U+79D4 U+9F98 U+7881 U+7C72 U+2460 U+2013
	const char input61[] = "ABC\x88\x56\x88\x45\xFE\xFE\xF9\xD5\xF9\xD6\xC6\x7E\xC6\xA1\xA1\x56";
	const uni_char * const expect61 = UNI_L("ABC\x0100\xD840\xDD0C\x79D4\x9F98\x7881\x7C72\x2460\x2013");

	rc &= encodings_perform_tests("big5-hkscs", input61, sizeof input61, expect61, unicode_bytelen(expect61));

#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	int i;
	// end state check
	for (i = 4; i <= 18; i += 2)
	{
		InputConverter *test61b = encodings_new_forward("big5-hkscs");
		encodings_check(test61b);
		written = test61b->Convert(input61, i, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
		BOOL is_end_state = test61b->IsValidEndState();
		OP_DELETE(test61b);
		encodings_check(!is_end_state);
	}
#endif

	// Test string:
	// U+0041 U+2010C U+00D3 U+2910D U+0042
	const char input63[] = "\x41\x88\x45\x88\x5F\xFE\xFD\x42";
	const uni_char * const expect63 = UNI_L("\x0041\xD840\xDD0C\x00D3\xD864\xDD0D\x0042");

	rc &= encodings_perform_tests("big5-hkscs", input63, sizeof input63, expect63, unicode_bytelen(expect63));

	// Test compatibility mappings
	// Test string: 92 B0  92 B1  FE AA  FE DD  8E 69  8E 6F  A0 E4
	// maps to:     A2 5A  A2 5C  A2 60  CF F1  BA E6  ED CA  94 55
	// decodes to:  U+515B U+515D U+74E9 U+7809 U+7BB8 U+7C07 U+7250
	const char input72[] =  "\x92\xB0\x92\xB1\xFE\xAA\xFE\xDD\x8E\x69\x8E\x6F\xA0\xE4";
	const char compat72[] = "\xA2\x5A\xA2\x5C\xA2\x60\xCF\xF1\xBA\xE6\xED\xCA\x94\x55";
	const uni_char * const expect72 = UNI_L("\x515B\x515D\x74E9\x7809\x7BB8\x7C06\x7250");

	rc &= encodings_perform_tests("big5-hkscs", input72, sizeof input72, expect72, unicode_bytelen(expect72), compat72, sizeof compat72);
	rc &= encodings_perform_tests("big5-hkscs", compat72, sizeof compat72, expect72, unicode_bytelen(expect72), compat72, sizeof compat72);

	rc &= encodings_test_invalid("big5-hkscs", invalid_asian_input(), unicode_bytelen(invalid_asian_input()), invalids_asian(), unicode_bytelen(invalids_asian()), 3, 14, entity_asian, sizeof entity_asian);

	// DSK-22126, DSK-79804 et.al: Assume single-byte codeset is ASCII
	rc &= encodings_perform_tests("big5-hkscs", input22813, sizeof input22813,
	                              expect22813(), unicode_bytelen(expect22813()));

	// DSK-146984: Make sure GetInvalidChars() list surrogates
#ifdef ENCODINGS_HAVE_SPECIFY_INVALID
	OutputConverter *testnonbmp = encodings_new_reverse("big5-hkscs");
	encodings_check(testnonbmp);
	written = testnonbmp->Convert(nonbmp(), unicode_bytelen(nonbmp()), encodings_bufferback, ENC_TESTBUF_SIZE, &read);
	int number_of_invalid = testnonbmp->GetNumberOfInvalid();
	int first_invalid = testnonbmp->GetFirstInvalidOffset();
	uni_char invalids[16]; /* ARRAY OK 2012-01-06 peter */
	uni_strlcpy(invalids, testnonbmp->GetInvalidCharacters(), 16);
	OP_DELETE(testnonbmp);
	encodings_check(number_of_invalid == 4);
	encodings_check(first_invalid == 0);
	encodings_check(op_memcmp(invalids, nonbmp(), 8) == 0);
#endif

#ifdef ENCODINGS_HAVE_ENTITY_ENCODING
	OutputConverter *testnonbmp2 = encodings_new_reverse("big5-hkscs");
	encodings_check(testnonbmp2);
	testnonbmp2->EnableEntityEncoding(TRUE);
	written = testnonbmp2->Convert(nonbmp(), unicode_bytelen(nonbmp()), encodings_bufferback, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(testnonbmp2);
	encodings_check(written >= 0 && op_memcmp(encodings_bufferback, "&#65536;&#135732;", written) == 0);
#endif

	// CORE-43484: Decoder should not be greedy
	const char input43484[] = "\xA1\x22";
	const uni_char * const expect43484 = UNI_L("\xFFFD\x22");
	InputConverter *test43484 = encodings_new_forward("big5-hkscs");
	encodings_check(test43484);
	written = test43484->Convert(input43484, sizeof input43484, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484);
	encodings_check(read == sizeof input43484 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, unicode_bytelen(expect43484)) == 0);

	// CORE-45016
	rc &= encodings_perform_tests("big5-hkscs", input45016, sizeof input45016, expect45016(), unicode_bytelen(expect45016()));

	return rc;
}
#endif

#ifdef ENCODINGS_HAVE_CHINESE
int encodings_test_big5hkscs_2004(void)
{
	int rc = 1;

	// Test string:
	// U+31C0 U+31CE
	//   8840   8855
	// These were previously mapped to the PUA.
	const char input1[] = "\x88\x40\x88\x55";
	const uni_char * const expect1 = UNI_L("\x31C0\x31CE");

	rc &= encodings_perform_tests("big5-hkscs", input1, sizeof input1, expect1, unicode_bytelen(expect1));

	// Test string:
	// U+43F0 U+749D
	//   8740   8779
	// These were previously outside the mapped area.
	const char input2[] = "\x87\x40\x87\x79";
	const uni_char * const expect2 = UNI_L("\x43F0\x749D");

	rc &= encodings_perform_tests("big5-hkscs", input2, sizeof input2, expect2, unicode_bytelen(expect2));

	return rc;
}
#endif

#ifdef ENCODINGS_HAVE_CHINESE
int encodings_test_big5hkscs_2008(void)
{
	int rc = 1;

	// Test string:
	// U+3875 U+21D53 U+9FCB
	//   877A    877B   87FD
	// These were previously outside the mapped area.
	const char input1[] = "\x87\x7A\x87\x7B\x87\xDF";
	const uni_char * const expect1 = UNI_L("\x3875\xD847\xDD53\x9FCB");

	rc &= encodings_perform_tests("big5-hkscs", input1, sizeof input1, expect1, unicode_bytelen(expect1));

	return rc;
}
#endif

#ifdef ENCODINGS_HAVE_JAPANESE
int encodings_test_eucjp(void)
{
	int rc = 1;
	int read, written;

	// simple JIS-Roman
	const char input21[] = "Dette er en enkel test.";
	const uni_char * const expect21 = UNI_L("Dette er en enkel test.");
	rc &= encodings_perform_tests("euc-jp", input21, sizeof input21, expect21, unicode_bytelen(expect21));

	// JIS 0208
	const char input22[] = "Verre: \xA4\xAB\xA4\xCA\xB4\xC1\xBB\xFA";
	const uni_char * const expect22 = UNI_L("Verre: \x304b\x306a\x6f22\x5b57");
	rc &= encodings_perform_tests("euc-jp", input22, sizeof input22, expect22, unicode_bytelen(expect22));

#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	int i;
	// end state check
	for (i = 8; i <= 14; i += 2)
	{
		InputConverter *test22b = encodings_new_forward("euc-jp");
		encodings_check(test22b);
		written = test22b->Convert(input22, i, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
		BOOL is_end_state = test22b->IsValidEndState();
		OP_DELETE(test22b);
		encodings_check(!is_end_state);
	}
#endif

	// half-width kana
	const char input24[] = "Kana: \x8E\xB6\x8E\xC5";
	const uni_char * const expect24 = UNI_L("Kana: \xFF76\xFF85");
	rc &= encodings_perform_tests("euc-jp", input24, sizeof input24, expect24, unicode_bytelen(expect24));

	// JIS 0212
	const char input0212[] = "H\x8F\xAB\xA9kon Wium Lie";
	const uni_char * const expect0212 = UNI_L("H\xE5kon Wium Lie");
	rc &= encodings_perform_tests("euc-jp", input0212, sizeof input0212, expect0212, unicode_bytelen(expect0212));

	const char input0212b[] =
		"\xbd\xc1\xca\xb4 [\xa4\xb7\xa4\xeb\xa4\xb3] "
		"/{Kochk.}/Shiruko/(s\x8f\xab\xe4\x8f\xa9\xce"
		"e Bohnensuppe mit Reiskuchen)/";
	const uni_char * const expect0212b =
		UNI_L("\x6C41\x7C89 [\x3057\x308B\x3053] /{Kochk.}/Shiruko/(s\x00FC\x00DF\x65 Bohnensuppe mit Reiskuchen)/");
	rc &= encodings_perform_tests("euc-jp", input0212b, sizeof input0212b, expect0212b, unicode_bytelen(expect0212b));

	// code points above last assigned code point
	const char input33[] = "Verre: \xA4\xAB\xfe\xfe";
	const uni_char * const expect33 = UNI_L("Verre: \x304b\xfffd");
	const char back33[] = "Verre: \xa4\xab?";
	rc &= encodings_perform_tests("euc-jp", input33, sizeof input33, expect33, unicode_bytelen(expect33),
	                        back33, sizeof back33, TRUE);

//	The characters in invalid_asian_input are available in JIS 0212
//	rc &= encodings_test_invalid("euc-jp", invalid_asian_input(), unicode_bytelen(invalid_asian_input()), invalids_asian(), unicode_bytelen(invalids_asian()), 3, 14, entity_asian, sizeof entity_asian);
//	const char entity_eucjp[] = "\xA1\xA1&#229;&#228;&#246;";
//	rc &= encodings_test_invalid("euc-jp", tricky_asian_input(), unicode_bytelen(tricky_asian_input()),  invalids_asian(), unicode_bytelen(invalids_asian()), 3, 1, entity_eucjp, sizeof entity_eucjp);

	rc &= encodings_test_invalid("euc-jp", invalid_jis0212_input(), unicode_bytelen(invalid_jis0212_input()), invalids_jis0212(), unicode_bytelen(invalids_jis0212()), 3, 19, entity_jis0212, sizeof entity_jis0212);

	const char entity_eucjp_tricky[] = "\xA1\xA1&#171;&#173;&#178;";
	rc &= encodings_test_invalid("euc-jp", tricky_jis0212_input(), unicode_bytelen(tricky_jis0212_input()), invalids_jis0212(), unicode_bytelen(invalids_jis0212()), 3, 1, entity_eucjp_tricky, sizeof entity_eucjp_tricky);

	// DSK-22126, DSK-79804 et.al: Assume single-byte codeset is ASCII
	rc &= encodings_perform_tests("euc-jp", input22813, sizeof input22813,
	                              expect22813(), unicode_bytelen(expect22813()));

	// DSK-146984: Make sure GetInvalidChars() list surrogates
	int number_of_invalid, first_invalid;
#ifdef ENCODINGS_HAVE_SPECIFY_INVALID
	OutputConverter *testnonbmp = encodings_new_reverse("euc-jp");
	encodings_check(testnonbmp);
	written = testnonbmp->Convert(nonbmp(), unicode_bytelen(nonbmp()), encodings_bufferback, ENC_TESTBUF_SIZE, &read);
	number_of_invalid = testnonbmp->GetNumberOfInvalid();
	first_invalid = testnonbmp->GetFirstInvalidOffset();
	uni_char invalids[16]; /* ARRAY OK 2012-01-06 peter */
	uni_strlcpy(invalids, testnonbmp->GetInvalidCharacters(), 16);
	OP_DELETE(testnonbmp);
	encodings_check(number_of_invalid == 4);
	encodings_check(first_invalid == 0);
	encodings_check(op_memcmp(invalids, nonbmp(), 8) == 0);
#endif

#ifdef ENCODINGS_HAVE_ENTITY_ENCODING
	OutputConverter *testnonbmp2 = encodings_new_reverse("euc-jp");
	encodings_check(testnonbmp2);
	testnonbmp2->EnableEntityEncoding(TRUE);
	written = testnonbmp2->Convert(nonbmp(), unicode_bytelen(nonbmp()), encodings_bufferback, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(testnonbmp2);
	encodings_check(op_memcmp(encodings_bufferback, "&#65536;&#135732;", written) == 0);
#endif

	// DLPHN-1965, DLPHN-2901: Broken JIS 0212
	const char broken0212[] = "\x8F\x81\x81\x8F\xA1\x81\x8F\x81\xA1\x8F\x41\x41\x8F\xA1\x41\x8F\x41\xA1\x8F\x20\x20";
	InputConverter *testbroken0212 = encodings_new_forward("euc-jp");
	encodings_check(testbroken0212);
	op_memset(encodings_buffer, 0, ENC_TESTBUF_SIZE);
	written = testbroken0212->Convert(broken0212, sizeof broken0212, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	number_of_invalid = testbroken0212->GetNumberOfInvalid();
	first_invalid = testbroken0212->GetFirstInvalidOffset();
	OP_DELETE(testbroken0212);
	encodings_check(read == sizeof broken0212);
	encodings_check(number_of_invalid >= 6);
	encodings_check(first_invalid == 0);
	encodings_check(written == int(UNICODE_SIZE(uni_strlen(reinterpret_cast<uni_char *>(encodings_buffer)) + 1)));

	// Broken JIS 0201
	const char broken0201[] = "\x8E\x81\x8E\xE0\x8E\x41";
	InputConverter *testbroken0201 = encodings_new_forward("euc-jp");
	encodings_check(testbroken0201);
	op_memset(encodings_buffer, 0, ENC_TESTBUF_SIZE);
	written = testbroken0201->Convert(broken0201, sizeof broken0201, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	number_of_invalid = testbroken0201->GetNumberOfInvalid();
	first_invalid = testbroken0201->GetFirstInvalidOffset();
	OP_DELETE(testbroken0201);
	encodings_check(read == sizeof broken0201);
	encodings_check(number_of_invalid >= 3);
	encodings_check(first_invalid == 0);
	encodings_check(written == int(UNICODE_SIZE(uni_strlen(reinterpret_cast<uni_char *>(encodings_buffer)) + 1)));

	// CORE-21093: U+FF0D and U+2212 should both map to SJIS=817C.
	const char expect21093[] = "\xA1\xDD\xA1\xDD";
	OutputConverter *test21903 = encodings_new_reverse("euc-jp");
	encodings_check(test21903);
	written = test21903->Convert(input21093(), unicode_bytelen(input21093()),
	                             encodings_bufferback, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test21903);
	encodings_check(read == int(unicode_bytelen(input21093())) && written == sizeof expect21093);
	encodings_check(op_strcmp(encodings_bufferback, expect21093) == 0);

	// ...but forward conversion should prefer the CP932 mapping (U+FF0D)
	InputConverter *test21093i = encodings_new_forward("euc-jp");
	encodings_check(test21093i);
	written = test21093i->Convert(encodings_bufferback, written,
	                              encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test21093i);
	encodings_check(read == sizeof expect21093 && written == int(unicode_bytelen(expectunicode21093())));
	encodings_check(op_memcmp(encodings_buffer, expectunicode21093(), written) == 0);

	// CORE-43416: JIS X 0208 reverse mapping
	const uni_char * const input43416 = UNI_L("\x2170\x9ED1\x71C1");
	const char expect43416[] = "\xFC\xF1\xFC\xEE\xFA\xFC";
	OutputConverter *test43416 = encodings_new_reverse("euc-jp");
	encodings_check(test43416);
	written = test43416->Convert(input43416, unicode_bytelen(input43416),
								 encodings_bufferback, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43416);
	encodings_check(read == int(unicode_bytelen(input43416)));
	encodings_check(written == sizeof expect43416);
	encodings_check(op_strcmp(encodings_bufferback, expect43416) == 0);

	// CORE-43484: Decoder should not be greedy
	const uni_char * const expect43484 = UNI_L("\xFFFD\x22");
	// Incorrect code set 1 ([0xA1..0xFE] and ![0xA1..0xFE])
	const char input43484_1[] = "\xA1\x22";
	InputConverter *test43484_1 = encodings_new_forward("euc-jp");
	encodings_check(test43484_1);
	written = test43484_1->Convert(input43484_1, sizeof input43484_1,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_1);
	encodings_check(read == sizeof input43484_1 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// Incorrect code set 2 (0x8E and ![0xA1..0xDF])
	const char input43484_2[] = "\x8E\x22";
	InputConverter *test43484_2 = encodings_new_forward("euc-jp");
	encodings_check(test43484_2);
	written = test43484_2->Convert(input43484_2, sizeof input43484_2,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_2);
	encodings_check(read == sizeof input43484_2 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// Incorrect code set 3; invalid second (0x8F and ![0xA1..0xFE])
	const char input43484_3[] = "\x8F\x22";
	InputConverter *test43484_3 = encodings_new_forward("euc-jp");
	encodings_check(test43484_3);
	written = test43484_3->Convert(input43484_3, sizeof input43484_3,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_3);
	encodings_check(read == sizeof input43484_3 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// Incorrect code set 3; invalid third (0x8F and [0xA1..0xFe] and ![0xA1..0xFE])
	const char input43484_4[] = "\x8F\xA1\x22";
	InputConverter *test43484_4 = encodings_new_forward("euc-jp");
	encodings_check(test43484_4);
	written = test43484_4->Convert(input43484_4, sizeof input43484_4,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_4);
	encodings_check(read == sizeof input43484_4 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// CORE-45016
	rc &= encodings_perform_tests("euc-jp", input45016, sizeof input45016, expect45016(), unicode_bytelen(expect45016()));

	return rc;
}
#endif

#ifdef ENCODINGS_HAVE_CHINESE
int encodings_test_gbk(void)
{
	int rc = 1;
	// simple ascii
	const char input26[] = "Dette er en enkel test.";
	const uni_char * const expect26 = UNI_L("Dette er en enkel test.");
	rc &= encodings_perform_tests("gbk", input26, sizeof input26, expect26, unicode_bytelen(expect26));

	// GB 2312
	const char input27[] = "Verre: \xa1\xa1\xb9\xa7\xd3\xd5";
	const uni_char * const expect27 = UNI_L("Verre: \x3000\x606D\x8BF1");
	rc &= encodings_perform_tests("gbk", input27, sizeof input27, expect27, unicode_bytelen(expect27));

	int read, written;
#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	int i;
	// end state check
	for (i = 8; i <= 12; i += 2)
	{
		InputConverter *test27b = encodings_new_forward("gbk");
		encodings_check(test27b);
		written = test27b->Convert(input27, i, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
		BOOL is_end_state = test27b->IsValidEndState();
		OP_DELETE(test27b);
		encodings_check(!is_end_state);
		encodings_check(0 <= written);
		encodings_check((size_t)written <= unicode_bytelen(expect27));
	}
#endif

	// Microsoft GBK extension
	const char input54[] = "Microsoft Euro: \x80";
	const uni_char * const expect54 = UNI_L("Microsoft Euro: \x20AC");
	const char back54[]  = "Microsoft Euro: \xA2\xE3";
	rc &= encodings_perform_tests("gbk", input54, sizeof input54, expect54, unicode_bytelen(expect54),
	                        back54, sizeof back54);

/*
	// code points above last assigned code point
	EUCtoUTF16Converter *conv29 = new EUCtoUTF16Converter(EUCtoUTF16Converter::EUC_CN);
	assert(conv29 && OpStatus.IsSuccess(conv29->Construct()));
	written =  conv29->Convert("Verre: \xa1\xa1\xfb\x7d", 12, buffer, ENC_TESTBUF_SIZE, &read);

	assert(op_memcmp(buffer, L"Verre: \x3000\xfffd", 20) == 0 && read == 12 && written == 20);

	UTF16toDBCSConverter *conv29back = new UTF16toDBCSConverter(UTF16toDBCSConverter::GBK);
	assert(conv29back && OpStatus.IsSuccess(conv29back->Construct()));
	written = conv29back->Convert(buffer, 20, bufferback, ENC_TESTBUF_SIZE, &read);
	assert(op_memcmp(bufferback, "Verre: \xa1\xa1?", 11) == 0 && read == 20 && written == 11);
*/

	rc &= encodings_test_invalid("gbk", invalid_asian_input(), unicode_bytelen(invalid_asian_input()), invalids_asian(), unicode_bytelen(invalids_asian()), 3, 14, entity_asian, sizeof entity_asian);
	const char entity_gbk[] = "\xA1\xA1&#229;&#228;&#246;";
	rc &= encodings_test_invalid("gbk", tricky_asian_input(), unicode_bytelen(tricky_asian_input()),  invalids_asian(), unicode_bytelen(invalids_asian()), 3, 1, entity_gbk, sizeof entity_gbk);

	// DSK-22126, DSK-79804 et.al: Assume single-byte codeset is ASCII
	rc &= encodings_perform_tests("gbk", input22813, sizeof input22813,
	                              expect22813(), unicode_bytelen(expect22813()));

	// CORE-43484: Decoder should not be greedy
	const char input43484[] = "\xA1\x22";
	const uni_char * const expect43484 = UNI_L("\xFFFD\x22");
	InputConverter *test43484 = encodings_new_forward("gbk");
	encodings_check(test43484);
	written = test43484->Convert(input43484, sizeof input43484, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484);
	encodings_check(read == sizeof input43484 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, unicode_bytelen(expect43484)) == 0);

	// CORE-45016
	rc &= encodings_perform_tests("gbk", input45016, sizeof input45016, expect45016(), unicode_bytelen(expect45016()));

	return rc;
}
#endif

#ifdef ENCODINGS_HAVE_CHINESE
int encodings_test_gb18030(void)
{
	int rc = 1;
	// Test string:
	// U+0041 U+0042 U+0043 [ASCII]
	// U+3000 U+606D U+8BF1 [GBK]
	// U+0080 U+00A5 U+00AA U+FFDF U+200F [GB18030 BMP range]
	// U+10000 U+10FFFD [GB18030 non-BMP range]
	const char input62[] = "ABC\xa1\xa1\xb9\xa7\xd3\xd5\x81\x30\x81\x30\x81\x30\x84\x36"
	                       "\x81\x30\x84\x39\x84\x31\xA2\x33\x81\x36\xA5\x31\x90\x30\x81\x30"
						   "\xE3\x32\x9A\x33";
	const uni_char expect62[] =
	{ L'A', L'B', L'C', 0x3000, 0x606D, 0x8BF1, 0x0080, 0x00A5, 0x00AA, 0xFFDF, 0x200F,
	  0xD800, 0xDC00, 0xDBFF, 0xDFFD, 0x0000 };

	rc &= encodings_perform_tests("gb18030", input62, sizeof input62, expect62, unicode_bytelen(expect62));

	int read, written;
#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	int i;
	// end state check
	for (i = 4; i <= 10; i += 2)
	{
		InputConverter *test62b = encodings_new_forward("gb18030");
		encodings_check(test62b);
		written = test62b->Convert(input62, i, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
		BOOL is_end_state = test62b->IsValidEndState();
		OP_DELETE(test62b);
		encodings_check(!is_end_state);
		encodings_check(0 <= written);
		encodings_check((size_t)written <= unicode_bytelen(expect62));
	}

	for (i = 10; i <= 34; i += 4)
	{
		// Four-byte sequences
		for (int j = 0; j < 3; ++ j)
		{
			InputConverter *test62c = encodings_new_forward("gb18030");
			encodings_check(test62c);
			written = test62c->Convert(input62, i + j, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
			BOOL is_end_state = test62c->IsValidEndState();
			OP_DELETE(test62c);
			encodings_check(!is_end_state);
			encodings_check(0 <= written &&
							(size_t)written <= unicode_bytelen(expect62));
		}
	}
#endif

	// DSK-22126, DSK-79804 et.al: Assume single-byte codeset is ASCII
	rc &= encodings_perform_tests("gb18030", input22813, sizeof input22813,
	                              expect22813(), unicode_bytelen(expect22813()));

	// CORE-43484: Decoder should not be greedy
	const uni_char * const expect43484 = UNI_L("\xFFFD\x22");
	// 1: GBK compatibility
	const char input43484_1[] = "\xA1\x22";
	InputConverter *test43484_1 = encodings_new_forward("gb18030");
	encodings_check(test43484_1);
	written = test43484_1->Convert(input43484_1, sizeof input43484_1, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_1);
	encodings_check(read == sizeof input43484_1 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, unicode_bytelen(expect43484)) == 0);

	// 2: Invalid third byte
	const char input43484_2[] = "\x81\x30\x22";
	InputConverter *test43484_2 = encodings_new_forward("gb18030");
	encodings_check(test43484_2);
	written = test43484_2->Convert(input43484_2, sizeof input43484_2, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_2);
	encodings_check(read == sizeof input43484_2 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, unicode_bytelen(expect43484)) == 0);

	// 3: Invalid fourth byte
	const char input43484_3[] = "\x81\x30\x81\x22";
	InputConverter *test43484_3 = encodings_new_forward("gb18030");
	encodings_check(test43484_3);
	written = test43484_3->Convert(input43484_3, sizeof input43484_3, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_3);
	encodings_check(read == sizeof input43484_3 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, unicode_bytelen(expect43484)) == 0);

	// CORE-45016
	rc &= encodings_perform_tests("gb18030", input45016, sizeof input45016, expect45016(), unicode_bytelen(expect45016()));

	return rc;
}
#endif

#ifdef ENCODINGS_HAVE_CHINESE
int encodings_test_hz(void)
{
	int rc = 1;
	int read, written;

	// plain ascii
	const char input34[] = "Liten test";
	const uni_char * const expect34 = UNI_L("Liten test");
	rc &= encodings_perform_tests("hz-gb-2312", input34, sizeof input34, expect34, unicode_bytelen(expect34));

	// GB 2312
	const char input35[] = "This is GB: ~{<:Ky2;!#~}xBye.";
	const uni_char * const expect35 = UNI_L("This is GB: \x5df1\x6240\x4e0d\x3002xBye.");
	rc &= encodings_perform_tests("hz-gb-2312", input35, sizeof input35,
	                              expect35, unicode_bytelen(expect35),
	                              NULL, -1, FALSE, TRUE);

#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	// end state check
	int i;
	for (i = 13; i <= 23; ++ i)
	{
		InputConverter *test35b = encodings_new_forward("hz-gb-2312");
		encodings_check(test35b);
		written = test35b->Convert(input35, i, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
		BOOL is_end_state = test35b->IsValidEndState();
		OP_DELETE(test35b);
		encodings_check(!is_end_state);
	}
#endif

	// Test line continuation
	const char inputlf[] = "abc~\ndef~\nghi~\n\n";
	const uni_char * const expectlf = UNI_L("abcdefghi\n");
	const char rev_expectlf[] = "abcdefghi\n";
	rc &= encodings_perform_tests("hz-gb-2312", inputlf, sizeof inputlf, expectlf, unicode_bytelen(expectlf), rev_expectlf, sizeof rev_expectlf);

	// Test that behaviour is conformant to expectations listed in CORE-9031
	const char input50[] = "onm\x7e\x7b\x7e\x7douseover";
	const uni_char * const expect50 = UNI_L("onmouseover");
	const char rev_expect50[] = "onmouseover";
	rc &= encodings_perform_tests("hz-gb-2312", input50, sizeof input50, expect50, unicode_bytelen(expect50), rev_expect50, sizeof rev_expect50);

	// CORE-43613: ~~ should map to ~ also in GBK mode.
	const char input43613[] = "~~~{~~~}~~";
	const uni_char * const expect43613 = UNI_L("~~~");
	const char rev_expect43613[] = "~~~~~~";
	rc &= encodings_perform_tests("hz-gb-2312", input43613, sizeof input43613, expect43613, unicode_bytelen(expect43613), rev_expect43613, sizeof rev_expect43613);

	// Failure tests
	// - Line break where lead byte expected
	const char input88[] = "~{<:Ky2;!#\x0Axyz";
	const uni_char * const output88 = UNI_L("\x5df1\x6240\x4e0d\x3002\x0Axyz");
	InputConverter *conv88 = encodings_new_forward("hz-gb-2312");
	encodings_check(conv88);
	written = conv88->Convert(input88, sizeof input88, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	int number_of_invalid = conv88->GetNumberOfInvalid();
	int first_invalid = conv88->GetFirstInvalidOffset();
	OP_DELETE(conv88);
	encodings_check(number_of_invalid == 1);
	encodings_check(first_invalid == 4);
	encodings_check(read == sizeof input88);
	encodings_check(0 == op_memcmp(output88, encodings_buffer, unicode_bytelen(output88)));
	encodings_check(written == int(unicode_bytelen(output88)));

	encodings_test_buffers("hz-gb-2312", input88, sizeof input88, output88, unicode_bytelen(output88));

	// - Line break where trail byte expected
	const char input89[] = "~{<:Ky2;!#<\x0Axyz";
	const uni_char * const output89 = UNI_L("\x5df1\x6240\x4e0d\x3002\x0Axyz");
	InputConverter *conv89 = encodings_new_forward("hz-gb-2312");
	encodings_check(conv89);
	written = conv89->Convert(input89, sizeof input89, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	number_of_invalid = conv89->GetNumberOfInvalid();
	first_invalid = conv89->GetFirstInvalidOffset();
	OP_DELETE(conv89);
	encodings_check(number_of_invalid == 1);
	encodings_check(first_invalid == 4);
	encodings_check(read == sizeof input89);
	encodings_check(0 == op_memcmp(output89, encodings_buffer, unicode_bytelen(output89)));
	encodings_check(written == int(unicode_bytelen(output89)));

	encodings_test_buffers("hz-gb-2312", input89, sizeof input89, output89, unicode_bytelen(output89));

	// CORE-43630: Unknown mode switches should emit error sequences
	const char input43630[] = "~Z";
	InputConverter *conv43630 = encodings_new_forward("hz-gb-2312");
	encodings_check(conv43630);
	written = conv43630->Convert(input43630, sizeof input43630, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	number_of_invalid = conv43630->GetNumberOfInvalid();
	first_invalid = conv43630->GetFirstInvalidOffset();
	OP_DELETE(conv43630);
	encodings_check(number_of_invalid == 1);
	encodings_check(first_invalid == 0);
	encodings_check(read == sizeof input43630);

	// Non-ASCII characters should be ignored
	const char input43610[] = "\xA1\xA1xyz";
	const uni_char * const output43610 = UNI_L("\xFFFD\xFFFDxyz");
	InputConverter *conv43610 = encodings_new_forward("hz-gb-2312");
	encodings_check(conv43610);
	written = conv43610->Convert(input43610, sizeof input43610, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	number_of_invalid = conv43610->GetNumberOfInvalid();
	first_invalid = conv43610->GetFirstInvalidOffset();
	OP_DELETE(conv43610);
	encodings_check(number_of_invalid == 2);
	encodings_check(first_invalid == 0);
	encodings_check(read == sizeof input43610);
	encodings_check(0 == op_memcmp(output43610, encodings_buffer, unicode_bytelen(output43610)));
	encodings_check(written == int(unicode_bytelen(output43610)));

	// Handling of unknown characters
	rc &= encodings_test_invalid("hz-gb-2312", invalid_asian_input(), unicode_bytelen(invalid_asian_input()), invalids_asian(), unicode_bytelen(invalids_asian()), 3, 14, entity_asian, sizeof entity_asian);
	const char entity_hz[] = "~{\x21\x21~}&#229;&#228;&#246;";
	rc &= encodings_test_invalid("hz-gb-2312", tricky_asian_input(), unicode_bytelen(tricky_asian_input()),  invalids_asian(), unicode_bytelen(invalids_asian()), 3, 1, entity_hz, sizeof entity_hz);

	// CORE-43484: Decoder should not be greedy
	const char input43484[] = "~|";
	const uni_char * const expect43484 = UNI_L("\xFFFD|");
	InputConverter *test43484 = encodings_new_forward("hz-gb-2312");
	encodings_check(test43484);
	written = test43484->Convert(input43484, sizeof input43484, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484);
	encodings_check(read == sizeof input43484 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, unicode_bytelen(expect43484)) == 0);

	// CORE-45016
	rc &= encodings_perform_tests("hz-gb-2312", input45016, sizeof input45016, expect45016(), unicode_bytelen(expect45016()));

	return rc;
}
#endif

#ifdef ENCODINGS_HAVE_KOREAN
int encodings_test_euckr(void)
{
	int rc = 1;
	int read, written;

	const char input48[] = "ABC\xc7\xd1\xb1\xb9\xbe\xee";
	const uni_char * const expect48 = UNI_L("ABC\xD55C\xAD6D\xC5B4");
	rc &= encodings_perform_tests("euc-kr", input48, sizeof input48, expect48, unicode_bytelen(expect48));

#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	// end state check
	int i;
	for (i = 4; i <= 8; i += 2)
	{
		InputConverter *test48b = encodings_new_forward("euc-kr");
		encodings_check(test48b);
		written = test48b->Convert(input48, i, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
		BOOL is_end_state = test48b->IsValidEndState();
		OP_DELETE(test48b);
		encodings_check(!is_end_state);
	}
#endif

	// Undefined codepoints
	InputConverter *conv49 = encodings_new_forward("euc-kr");
	encodings_check(conv49);
	const char input49[] = "\xFD\xFF\x81\xFF\xFE\x81";
	op_memset(encodings_buffer, 0, ENC_TESTBUF_SIZE);
	written = conv49->Convert(input49, sizeof input49, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	int number_of_invalid = conv49->GetNumberOfInvalid();
	int first_invalid = conv49->GetFirstInvalidOffset();
	OP_DELETE(conv49);
	encodings_check(number_of_invalid >= 3);
	encodings_check(first_invalid == 0);
	encodings_check(read == sizeof input49);
	encodings_check(written == int(UNICODE_SIZE(uni_strlen(reinterpret_cast<uni_char *>(encodings_buffer)) + 1)));

	rc &= encodings_test_invalid("euc-kr", invalid_asian_input(), unicode_bytelen(invalid_asian_input()), invalids_asian(), unicode_bytelen(invalids_asian()), 3, 14, entity_asian, sizeof entity_asian);

	// DSK-22126, DSK-79804 et.al: Assume single-byte codeset is ASCII
	rc &= encodings_perform_tests("euc-kr", input22813, sizeof input22813,
	                              expect22813(), unicode_bytelen(expect22813()));

	// CORE-43484: Decoder should not be greedy
	const char input43484[] = "\xA1\x22";
	const uni_char * const expect43484 = UNI_L("\xFFFD\x22");
	InputConverter *test43484 = encodings_new_forward("euc-kr");
	encodings_check(test43484);
	written = test43484->Convert(input43484, sizeof input43484, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484);
	encodings_check(read == sizeof input43484 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, unicode_bytelen(expect43484)) == 0);

	// CORE-45016
	rc &= encodings_perform_tests("euc-kr", input45016, sizeof input45016, expect45016(), unicode_bytelen(expect45016()));

	return rc;
}
#endif

#ifdef ENCODINGS_HAVE_CHINESE
int encodings_test_euctw(void)
{
	int rc = 1;
	int read, written;

	// two-byte sequences

	const char input50[] = "ABC\xA1\xA1\xFD\xA1"; // 12121 17D21
	const uni_char * const output50 = UNI_L("ABC\x3000\x9F72");
	rc &= encodings_perform_tests("euc-tw", input50, sizeof input50, output50, unicode_bytelen(output50));

#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	// end state check
	int i;
	for (i = 4; i <= 6; i += 2)
	{
		InputConverter *test50b = encodings_new_forward("euc-tw");
		encodings_check(test50b);
		written = test50b->Convert(input50, i, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
		BOOL is_end_state = test50b->IsValidEndState();
		OP_DELETE(test50b);
		encodings_check(!is_end_state);
	}
#endif

	// four-byte sequences, plane 1 only => two-byte in output
	const char input51[] = "ABC\x8E\xA1\xA1\xA1\x8E\xA1\xFD\xA1"; // 12121 17D21 same as input50
	rc &= encodings_perform_tests("euc-tw", input51, sizeof input51,
	                              output50, unicode_bytelen(output50),
	                              input50, sizeof input50);

#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	// end state check
	for (i = 4; i <= 8; i += 4)
	{
		for (int j = 0; j < 3; ++ j)
		{
			InputConverter *test51b = encodings_new_forward("euc-tw");
			encodings_check(test51b);
			written = test51b->Convert(input51, i + j, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
			BOOL is_end_state = test51b->IsValidEndState();
			OP_DELETE(test51b);
			encodings_check(!is_end_state);
		}
	}
#endif

	// four-byte sequences in plane 2 and 14
	const char input52[] = "\x8E\xA2\xA1\xA1\x8E\xAE\xE7\xAA"; // 22121 E672A
	const uni_char * const output52 = UNI_L("\x4E42\x92B0");

	rc &= encodings_perform_tests("euc-tw", input52, sizeof input52, output52, unicode_bytelen(output52));

	// Undefined codepoints
	const char input53[] = "ABC\xFE\xA1\x8E\xA1\xFE\xA1\x8E\xA2\xF3\xA1\x8E\xA3\xA1\xA1\x8E\xAE\xF0\xA1";
	                      //     17E21   17E21           27321           32121           E7021
	InputConverter *conv53 = encodings_new_forward("euc-tw");
	encodings_check(conv53);
	op_memset(encodings_buffer, 0, ENC_TESTBUF_SIZE);
	written = conv53->Convert(input53, sizeof input53, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	int number_of_invalid = conv53->GetNumberOfInvalid();
	int first_invalid = conv53->GetFirstInvalidOffset();
	OP_DELETE(conv53);
	encodings_check(number_of_invalid == 5);
	encodings_check(first_invalid == 3);
	encodings_check(written == int(UNICODE_SIZE(uni_strlen(reinterpret_cast<uni_char *>(encodings_buffer)) + 1)));

	rc &= encodings_test_invalid("euc-tw", invalid_asian_input(), unicode_bytelen(invalid_asian_input()), invalids_asian(), unicode_bytelen(invalids_asian()), 3, 14, entity_asian, sizeof entity_asian);

	// DSK-22126, DSK-79804 et.al: Assume single-byte codeset is ASCII
	rc &= encodings_perform_tests("euc-tw", input22813, sizeof input22813,
	                              expect22813(), unicode_bytelen(expect22813()));

	// CORE-43484: Decoder should not be greedy
	const uni_char * const expect43484 = UNI_L("\xFFFD\x22");

	// Incorrect code set 1 ([0xA1..0xFE] and ![0xA1..0xFE])
	const char input43484_1[] = "\xA1\x22";
	InputConverter *test43484_1 = encodings_new_forward("euc-tw");
	encodings_check(test43484_1);
	written = test43484_1->Convert(input43484_1, sizeof input43484_1,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_1);
	encodings_check(read == sizeof input43484_1 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	const char input49134[] = "\xA1\x81\x22";
	const uni_char * const expect49134 = UNI_L("\xFFFD\xFFFD\x22");
	InputConverter *test49134 = encodings_new_forward("euc-tw");
	encodings_check(test49134);
	written = test49134->Convert(input49134, sizeof input49134,
	                             encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test49134);
	encodings_check(read == sizeof input49134 && written == int(unicode_bytelen(expect49134)));
	encodings_check(op_memcmp(encodings_buffer, expect49134, written) == 0);

	// Incorrect code set 2; invalid second (0x8E and ![0xA1..0xB0])
	const char input43484_2[] = "\x8E\x22";
	InputConverter *test43484_2 = encodings_new_forward("euc-tw");
	encodings_check(test43484_2);
	written = test43484_2->Convert(input43484_2, sizeof input43484_2,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_2);
	encodings_check(read == sizeof input43484_2 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// Incorrect code set 2; invalid third (0x8E and [0xA1..0xB0] and ![0xA1..0xFE])
	const char input43484_3[] = "\x8E\xA1\x22";
	InputConverter *test43484_3 = encodings_new_forward("euc-tw");
	encodings_check(test43484_3);
	written = test43484_3->Convert(input43484_3, sizeof input43484_3,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_3);
	encodings_check(read == sizeof input43484_3 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// Incorrect code set 2; invalid fourth (0x8E and [0xA1..0xB0] and [0xA1..0xB0] and ![0xA1..0xFE])
	const char input43484_4[] = "\x8E\xA1\xA1\x22";
	InputConverter *test43484_4 = encodings_new_forward("euc-tw");
	encodings_check(test43484_4);
	written = test43484_4->Convert(input43484_4, sizeof input43484_4,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_4);
	encodings_check(read == sizeof input43484_4 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// CORE-45016
	rc &= encodings_perform_tests("euc-tw", input45016, sizeof input45016, expect45016(), unicode_bytelen(expect45016()));

	return rc;
}
#endif

#ifdef ENCODINGS_HAVE_UTF7
int encodings_test_utf7(void)
{
	int rc = 1;
	int read, written;

	// ASCII
	const char input66[] = "This is a simple ASCII string.";
	const uni_char * const output66 = UNI_L("This is a simple ASCII string.");
	rc &= encodings_perform_tests("utf-7", input66, sizeof input66, output66, unicode_bytelen(output66));
	rc &= encodings_perform_tests("utf-7-safe", input66, sizeof input66, output66, unicode_bytelen(output66));

	// Sample from RFC 2152
	const char input67[] = "Hi Mom -+Jjo--!";
	const uni_char * const output67 = UNI_L("Hi Mom -\x263A-!");
	rc &= encodings_perform_tests("utf-7", input67, sizeof input67, output67, unicode_bytelen(output67),
	                        NULL, -1, FALSE, TRUE);

#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	// end state check
	int i;
	for (i = 9; i <= 12; ++ i)
	{
		InputConverter *test67b = encodings_new_forward("utf-7");
		encodings_check(test67b);
		written = test67b->Convert(input67, i, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
		BOOL is_end_state = test67b->IsValidEndState();
		OP_DELETE(test67b);
		encodings_check(!is_end_state);
	}
#endif

	const char input68[] = "+ZeVnLIqe-";
	const uni_char * const output68 = UNI_L("\x65E5\x672C\x8A9E");
	rc &= encodings_perform_tests("utf-7", input68, sizeof input68,
	                              output68, unicode_bytelen(output68),
	                              NULL, -1, FALSE, TRUE);
	rc &= encodings_perform_tests("utf-7-safe", input68, sizeof input68,
	                              output68, unicode_bytelen(output68),
	                              NULL, -1, FALSE, TRUE);

#ifdef ENCODINGS_HAVE_UTF7IMAP
	// Sample from RFC 2060
	const char input69[] = "~peter/mail/&ZeVnLIqe-/&U,BTFw-";
	const uni_char * const output69 = UNI_L("~peter/mail/\x65E5\x672C\x8A9E/\x53F0\x5317");
	rc &= encodings_perform_tests("utf-7-imap", input69, sizeof input69, output69, unicode_bytelen(output69),
	                        NULL, -1, FALSE, TRUE);
#endif

	// Test with backslash and tilde characters
	const char input70[] = "abc+AFwAfg-abc";
	const uni_char * const output70 = UNI_L("abc\\~abc");
	rc &= encodings_perform_tests("utf-7", input70, sizeof input70,
	                              output70, unicode_bytelen(output70),
	                              NULL, -1, FALSE, TRUE);
	rc &= encodings_perform_tests("utf-7-safe", input70, sizeof input70,
	                              output70, unicode_bytelen(output70),
	                              NULL, -1, FALSE, TRUE);

	// Test header safe conversion
	const char input71[] = "abc+AEA-def+ACQ-";
	const uni_char * const output71 = UNI_L("abc@def$");
	rc &= encodings_perform_tests("utf-7-safe", input71, sizeof input71, output71, unicode_bytelen(output71));

	// Compare to standard conversion
	const char input71std[] = "abc@def$";
	rc &= encodings_perform_tests("utf-7", input71std, sizeof input71std, output71, unicode_bytelen(output71));

	// Check invalid counter
	char input73[] = "High-bit: \xE5\xE4\xF6";
	InputConverter *conv73 = encodings_new_forward("utf-7");
	encodings_check(conv73);
	written = conv73->Convert(input73, sizeof input73, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	int number_of_invalid = conv73->GetNumberOfInvalid();
	int first_invalid = conv73->GetFirstInvalidOffset();
	OP_DELETE(conv73);
	encodings_check(read == sizeof input73);
	encodings_check(written == sizeof input73 * 2);
	encodings_check(number_of_invalid == 3);
	encodings_check(first_invalid == 10);
	encodings_check(op_memcmp(encodings_buffer, UNI_L("High-bit: \xFFFD\xFFFD\xFFFD"), 28) == 0);

	// Test escape character
	const char input77[] = "+-";
	const uni_char * const output77 = UNI_L("+");
	rc &= encodings_perform_tests("utf-7", input77, sizeof input77, output77, unicode_bytelen(output77));

	return rc;
}
#endif

#ifdef ENCODINGS_HAVE_CHINESE
int encodings_test_iso2022cn(void)
{
	int rc = 1;
	int read, read2, written;

	// Sample from RFC 1922. The same text in GB and CNS encoding,
	// but where the last character variant is only in CNS
	const char input74[] = "\x1B\x24\x29\x41\x0E\x3D\x3B\x3B\x3B\x1B\x24\x29\x47\x47\x28\x5F\x50\x0F";
	//                      < GB2312       ><SO><U+4EA4><U+6362>< CNS 11643 p1 ><U+4EA4><U+63DB><SI>
	const uni_char * const output74 = UNI_L("\x4EA4\x6362\x4EA4\x63DB");
	const char back74[] =  "\x1B\x24\x29\x41\x0E\x3D\x3B\x3B\x3B\x3D\x3B\x1B\x24\x29\x47\x5F\x50\x0F";
	//                      < GB2312       ><SO><U+4EA4><U+6362><U+4EA4>< CNS 11643 p1 ><U+63DB><SI>

	rc &= encodings_perform_tests("iso-2022-cn", input74, sizeof input74,
	                              output74, unicode_bytelen(output74),
	                              back74, sizeof back74, FALSE, TRUE);

#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	// end state check
	int i;
	for (i = 1; i <= 17; ++ i)
	{
		InputConverter *test74b = encodings_new_forward("iso-2022-cn");
		encodings_check(test74b);
		written = test74b->Convert(input74, i, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
		BOOL is_end_state = test74b->IsValidEndState();
		OP_DELETE(test74b);
		if (i == 4)
			encodings_check(is_end_state);
		else
			encodings_check(!is_end_state);
	}
#endif

	// String with CNS 11643 plane 2 and ASCII intermixed, followed by GB code
	const char input75[] = "\x1B$*HXYZ\x1B\x4E\x21\x21\x1B\x4E\x72\x44""XYZ\x1B$)A\x0E\x21\x21\x77\x7E\x0F.";
	//                      <CNSp2>XYZ<SS2   ><U+4E42><SS2   ><U+9F98>  XYZ<GB   ><SO><U+3000><U+9F44><SI>.";
	const uni_char * const output75 = UNI_L("XYZ\x4E42\x9F98XYZ\x3000\x9F44.");
	const char back75[] =  "XYZ\x1B$*H\x1B\x4E\x21\x21\x1B\x4E\x72\x44""XYZ\x1B$)A\x0E\x21\x21\x77\x7E\x0F.";
	//                      XYZ<CNSp2><SS2   ><U+4E42><SS2   ><U+9F98>  XYZ<GB   ><SO><U+3000><U+9F44><SI>.";
	rc &= encodings_perform_tests("iso-2022-cn", input75, sizeof input75,
	                              output75, unicode_bytelen(output75),
	                              back75, sizeof back75, FALSE, TRUE);

	// String with a CNS 11643 character followed by a GB character, which
	// when converted back will give two valid CNS 11643 characters
	const char input76[] = "\x1B\x24\x29\x47\x0E\x5F\x50\x1B\x24\x29\x41\x3D\x3B\x0F";
	//                      < CNS 11643 p1 ><SO><U+63DB>< GB2312       ><U+4EA4><SI>";
	const uni_char * const output76 = UNI_L("\x63DB\x4EA4");
	const char back76[]  = "\x1B\x24\x29\x47\x0E\x5F\x50\x47\x28\x0F";
	//                      < CNS 11643 p1 ><SO><U+63DB><U+4EA4><SI>";
	rc &= encodings_perform_tests("iso-2022-cn", input76, sizeof input76,
	                              output76, unicode_bytelen(output76),
	                              back76, sizeof back76, FALSE, TRUE);

	// Border case tests:
	// Split escape sequence
	InputConverter *conv76b = encodings_new_forward("iso-2022-cn");
	encodings_check(conv76b);
	int orig_written = conv76b->Convert(input76, 1, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	written = orig_written + conv76b->Convert(&input76[read], sizeof input76 - read, &encodings_buffer[orig_written], ENC_TESTBUF_SIZE, &read2);
	OP_DELETE(conv76b);
	encodings_check(read == 1);
	encodings_check(orig_written == 0);
	encodings_check(read + read2 == sizeof input76);
	encodings_check(written == int(unicode_bytelen(output76)));
	encodings_check(op_memcmp(encodings_buffer, output76, unicode_bytelen(output76)) == 0);

	InputConverter *conv76c = encodings_new_forward("iso-2022-cn");
	encodings_check(conv76c);
	orig_written = conv76c->Convert(input76, 2, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	written = orig_written + conv76c->Convert(&input76[read], sizeof input76 - read, &encodings_buffer[orig_written], ENC_TESTBUF_SIZE, &read2);
	OP_DELETE(conv76c);
	encodings_check(read == 2);
	encodings_check(orig_written == 0);
	encodings_check(read + read2 == sizeof input76);
	encodings_check(written == int(unicode_bytelen(output76)));
	encodings_check(op_memcmp(encodings_buffer, output76, unicode_bytelen(output76)) == 0);

	// Escape sequence just beyond buffer

	InputConverter *conv76d = encodings_new_forward("iso-2022-cn");
	encodings_check(conv76d);
	orig_written = conv76d->Convert(input76, 7, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	written = orig_written + conv76d->Convert(&input76[read], sizeof input76 - read, &encodings_buffer[orig_written], ENC_TESTBUF_SIZE, &read2);
	OP_DELETE(conv76d);
	encodings_check(read == 7);
	encodings_check(orig_written == 2);
	encodings_check(read + read2 == sizeof input76);
	encodings_check(written == int(unicode_bytelen(output76)));
	encodings_check(op_memcmp(encodings_buffer, output76, unicode_bytelen(output76)) == 0);

	rc &= encodings_test_invalid("iso-2022-cn", invalid_asian_input(), unicode_bytelen(invalid_asian_input()), invalids_asian(), unicode_bytelen(invalids_asian()), 3, 14, entity_asian, sizeof entity_asian);

	const char entity_iso2022cn[] = "\x1B\x24\x29\x41\x0E\x21\x21\x0F&#229;&#228;&#246;";
	//                               < GB2312       ><SO><U+3000><SI><U+E5><U+E4><U+F6>
	rc &= encodings_test_invalid("iso-2022-cn", tricky_asian_input(), unicode_bytelen(tricky_asian_input()),  invalids_asian(), unicode_bytelen(invalids_asian()), 3, 1, entity_iso2022cn, sizeof entity_iso2022cn);

	// Failure tests
	// - Line break where lead byte expected
	const char input83[] = "\x1B\x24\x29\x47\x0E\x0Axyz";
	//                      < CNS 11643 p1 ><SO><xx>xyz";
	const uni_char * const output83 = UNI_L("\x0Axyz");
	InputConverter *conv83 = encodings_new_forward("iso-2022-cn");
	encodings_check(conv83);
	written = conv83->Convert(input83, 10, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	int number_of_invalid = conv83->GetNumberOfInvalid();
	int first_invalid = conv83->GetFirstInvalidOffset();
	OP_DELETE(conv83);
	encodings_check(number_of_invalid == 1);
	encodings_check(first_invalid == 0);
	encodings_check(read == 10);
	encodings_check(0 == op_memcmp(output83, encodings_buffer, 10));
	encodings_check(written == int(unicode_bytelen(output83)));

	encodings_test_buffers("iso-2022-cn", input83, sizeof input83, output83, unicode_bytelen(output83));

	// - Line break where trail byte expected
	const char input83b[] = "\x1B\x24\x29\x47\x0E\x5F\x0Axyz";
	//                       < CNS 11643 p1 ><SO><xxxxxx>xyz";
	const uni_char * const output83b = UNI_L("\xFFFDxyz");
	InputConverter *conv83b = encodings_new_forward("iso-2022-cn");
	encodings_check(conv83b);
	written = conv83b->Convert(input83b, 10, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	number_of_invalid = conv83b->GetNumberOfInvalid();
	first_invalid = conv83b->GetFirstInvalidOffset();
	OP_DELETE(conv83b);
	encodings_check(number_of_invalid == 1);
	encodings_check(first_invalid == 0);
	encodings_check(read == 10);
	encodings_check(0 == op_memcmp(output83b, encodings_buffer, 10));
	encodings_check(written == int(unicode_bytelen(output83b) - 2));

	encodings_test_buffers("iso-2022-cn", input83b, sizeof input83b, output83, unicode_bytelen(output83));

	// DSK-22126, DSK-79804 et.al: Assume single-byte codeset is ASCII
	rc &= encodings_perform_tests("iso-2022-cn", input22813, sizeof input22813,
	                              expect22813(), unicode_bytelen(expect22813()));

	// CORE-43484: Decoder should not be greedy
	const uni_char * const expect43484 = UNI_L("\xFFFDxyz");
	// 1. Escape character in trail byte
	const char input43484_1[] = "\x1B$)A\x0E\x21\x1B$)G\x0Fxyz";
	InputConverter *test43484_1 = encodings_new_forward("iso-2022-cn");
	encodings_check(test43484_1);
	written = test43484_1->Convert(input43484_1, sizeof input43484_1,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_1);
	encodings_check(read == sizeof input43484_1 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// 2. Truncated escape sequence (invalid first)
	const char input43484_2[] = "\x1Bxyz";
	InputConverter *test43484_2 = encodings_new_forward("iso-2022-cn");
	encodings_check(test43484_2);
	written = test43484_2->Convert(input43484_2, sizeof input43484_2,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_2);
	encodings_check(read == sizeof input43484_2 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// 3. Truncated escape sequence (invalid second)
	const char input43484_3[] = "\x1B$xyz";
	InputConverter *test43484_3 = encodings_new_forward("iso-2022-cn");
	encodings_check(test43484_3);
	written = test43484_3->Convert(input43484_3, sizeof input43484_3,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_3);
	encodings_check(read == sizeof input43484_3 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// 4. Truncated escape sequence (invalid third)
	const char input43484_4[] = "\x1B$)xyz";
	InputConverter *test43484_4 = encodings_new_forward("iso-2022-cn");
	encodings_check(test43484_4);
	written = test43484_4->Convert(input43484_4, sizeof input43484_4,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_4);
	encodings_check(read == sizeof input43484_4 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// 5. Shift to ASCII in trail byte
	const char input43484_5[] = "\x1B$)A\x0E\x21\x0Fxyz";
	InputConverter *test43484_5 = encodings_new_forward("iso-2022-cn");
	encodings_check(test43484_5);
	written = test43484_5->Convert(input43484_5, sizeof input43484_5,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_5);
	encodings_check(read == sizeof input43484_5 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// 6. Escape character in SS2 lead byte
	const char input43484_6[] = "\x1B$*H\x1B\x4E\x1B$)Axyz";
	InputConverter *test43484_6 = encodings_new_forward("iso-2022-cn");
	encodings_check(test43484_6);
	written = test43484_6->Convert(input43484_6, sizeof input43484_6,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_6);
	encodings_check(read == sizeof input43484_6 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// 7. Escape character in SS2 trail byte
	const char input43484_7[] = "\x1B$*H\x1B\x4E\x1B$)G\x0Fxyz";
	InputConverter *test43484_7 = encodings_new_forward("iso-2022-cn");
	encodings_check(test43484_7);
	written = test43484_7->Convert(input43484_7, sizeof input43484_7,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_7);
	encodings_check(read == sizeof input43484_7 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// 8. Shift to ASCII in SS2 lead byte
	const char input43484_8[] = "\x1B$*H\x1B\x4E\x0Fxyz";
	InputConverter *test43484_8 = encodings_new_forward("iso-2022-cn");
	encodings_check(test43484_8);
	written = test43484_8->Convert(input43484_8, sizeof input43484_8,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_8);
	encodings_check(read == sizeof input43484_8 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// 9. Shift to ASCII in SS2 trail byte
	const char input43484_9[] = "\x1B$*H\x1B\x4E\x21\x0Fxyz";
	InputConverter *test43484_9 = encodings_new_forward("iso-2022-cn");
	encodings_check(test43484_9);
	written = test43484_9->Convert(input43484_9, sizeof input43484_9,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_9);
	encodings_check(read == sizeof input43484_9 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// CORE-45016
	rc &= encodings_perform_tests("iso-2022-cn", input45016, sizeof input45016, expect45016(), unicode_bytelen(expect45016()));

	return rc;
}
#endif

#ifdef ENCODINGS_HAVE_KOREAN
int encodings_test_iso2022kr(void)
{
	int rc = 1;
	int read, written;

	// Sample from Lunde
	const char input85[] = "\x1B\x24\x29\x43\x0E\x31\x68\x44\x21\x0F";
	//                      < Designator   ><SO><U+AE40><U+CE58><SI>";
	const uni_char * const output85 = UNI_L("\xAE40\xCE58");
	rc &= encodings_perform_tests("iso-2022-kr", input85, sizeof input85,
	                              output85, unicode_bytelen(output85),
	                              NULL, 0, FALSE, TRUE);

#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	// end state check
	int i;
	for (i = 1; i <= 9; ++ i)
	{
		InputConverter *test85b = encodings_new_forward("iso-2022-kr");
		encodings_check(test85b);
		written = test85b->Convert(input85, i, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
		BOOL is_end_state = test85b->IsValidEndState();
		OP_DELETE(test85b);
		encodings_check(0 <= written);
		encodings_check((size_t)written <= unicode_bytelen(output85));
		if (i == 4)
			encodings_check(is_end_state);
		else
			encodings_check(!is_end_state);
	}
#endif

	// Mixing SBCS and DBCS
	const char input86[] = "\x1B\x24\x29\x43TEST\x0AY\x0E\x21\x21\x0F.";
	//                      < Designator   >TEST<NL>Y<SO><U+3000><SI>.
	const uni_char * const output86 = UNI_L("TEST\x000AY\x3000.");
	rc &= encodings_perform_tests("iso-2022-kr", input86, sizeof input86,
	                              output86, unicode_bytelen(output86),
	                              NULL, 0, FALSE, TRUE);

	// Failure tests
	// - Line break where lead byte expected
	const char input87[] = "\x1B\x24\x29\x43\x0E\x0Axyz";
	//                      < Designator   ><SO><xx>xyz";
	const uni_char * const output87 = UNI_L("\x0Axyz");
	InputConverter *conv87 = encodings_new_forward("iso-2022-kr");
	encodings_check(conv87);
	written = conv87->Convert(input87, sizeof input87, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	int number_of_invalid = conv87->GetNumberOfInvalid();
	int first_invalid = conv87->GetFirstInvalidOffset();
	OP_DELETE(conv87);
	encodings_check(number_of_invalid == 1);
	encodings_check(first_invalid == 0);
	encodings_check(read == sizeof input87);
	encodings_check(0 <= written);
	encodings_check((size_t)written == unicode_bytelen(output87));
	encodings_check(0 == op_memcmp(output87, encodings_buffer, written));

	encodings_test_buffers("iso-2022-kr", input87, sizeof input87, output87, unicode_bytelen(output87));

	// - Line break where trail byte expected
	const char input87b[] = "\x1B\x24\x29\x43\x0E\x5F\x0Axyz";
	//                       < Designator   ><SO><xxxxxx>xyz";
	const uni_char * const output87b = UNI_L("\xFFFDxyz");
	InputConverter *conv87b = encodings_new_forward("iso-2022-kr");
	encodings_check(conv87b);
	written = conv87b->Convert(input87b, sizeof input87b, encodings_buffer, ENC_TESTBUF_SIZE, &read);
	number_of_invalid = conv87b->GetNumberOfInvalid();
	first_invalid = conv87b->GetFirstInvalidOffset();
	OP_DELETE(conv87b);
	encodings_check(number_of_invalid == 1);
	encodings_check(first_invalid == 0);
	encodings_check(read == sizeof input87b);
	encodings_check(0 <= written);
	encodings_check((size_t)written == unicode_bytelen(output87b));
	encodings_check(0 == op_memcmp(output87b, encodings_buffer, written));

	encodings_test_buffers("iso-2022-kr", input87b, sizeof input87b, output87b, unicode_bytelen(output87b));

	const char entity_iso2022kr_invalid[] = "\x1B\x24\x29\x43Not in Asian: &#229;&#228;&#246;";
	rc &= encodings_test_invalid("iso-2022-kr", invalid_asian_input(), unicode_bytelen(invalid_asian_input()), invalids_asian(), unicode_bytelen(invalids_asian()), 3, 14, entity_iso2022kr_invalid, sizeof entity_iso2022kr_invalid);
	const char entity_iso2022kr_tricky[] = "\x1B\x24\x29\x43\x0E\x21\x21\x0F&#229;&#228;&#246;";
	//                                      < Designator   ><SO><U+3000><SO><U+E5><U+E4><U+F6>
	rc &= encodings_test_invalid("iso-2022-kr", tricky_asian_input(), unicode_bytelen(tricky_asian_input()),  invalids_asian(), unicode_bytelen(invalids_asian()), 3, 1, entity_iso2022kr_tricky, sizeof entity_iso2022kr_tricky);

	// DSK-22126, DSK-79804 et.al: Assume single-byte codeset is ASCII
	const char iso2022kr_22813_back[] = "\x1B\x24\x29\x43~\\";
	rc &= encodings_perform_tests("iso-2022-kr", input22813, sizeof input22813,
	                              expect22813(), unicode_bytelen(expect22813()),
	                              iso2022kr_22813_back, sizeof iso2022kr_22813_back,
	                              FALSE, TRUE);

	// CORE-43484: Decoder should not be greedy
	const uni_char * const expect43484 = UNI_L("\xFFFDxyz");
	// 1. Escape character in trail byte
	const char input43484_1[] = "\x1B$)C\x0E\x21\x1B$)C\x0Fxyz";
	InputConverter *test43484_1 = encodings_new_forward("iso-2022-kr");
	encodings_check(test43484_1);
	written = test43484_1->Convert(input43484_1, sizeof input43484_1,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_1);
	encodings_check(read == sizeof input43484_1 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// 2. Truncated escape sequence (invalid first)
	const char input43484_2[] = "\x1Bxyz";
	InputConverter *test43484_2 = encodings_new_forward("iso-2022-kr");
	encodings_check(test43484_2);
	written = test43484_2->Convert(input43484_2, sizeof input43484_2,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_2);
	encodings_check(read == sizeof input43484_2 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// 3. Truncated escape sequence (invalid second)
	const char input43484_3[] = "\x1B$xyz";
	InputConverter *test43484_3 = encodings_new_forward("iso-2022-kr");
	encodings_check(test43484_3);
	written = test43484_3->Convert(input43484_3, sizeof input43484_3,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_3);
	encodings_check(read == sizeof input43484_3 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// 4. Truncated escape sequence (invalid third)
	const char input43484_4[] = "\x1B$)xyz";
	InputConverter *test43484_4 = encodings_new_forward("iso-2022-kr");
	encodings_check(test43484_4);
	written = test43484_4->Convert(input43484_4, sizeof input43484_4,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_4);
	encodings_check(read == sizeof input43484_4 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// 5. Shift to ASCII in trail byte
	const char input43484_5[] = "\x1B$)C\x0E\x21\x0Fxyz";
	InputConverter *test43484_5 = encodings_new_forward("iso-2022-kr");
	encodings_check(test43484_5);
	written = test43484_5->Convert(input43484_5, sizeof input43484_5,
	                               encodings_buffer, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(test43484_5);
	encodings_check(read == sizeof input43484_5 && written == int(unicode_bytelen(expect43484)));
	encodings_check(op_memcmp(encodings_buffer, expect43484, written) == 0);

	// CORE-45016
	const char input45016kr[] = "\x1B$)C\x7F";
	rc &= encodings_perform_tests("iso-2022-kr", input45016kr, sizeof input45016kr, expect45016(), unicode_bytelen(expect45016()));

	return rc;
}
#endif

int encodings_test_iso_8859_7_2003(void)
{
	// New mappings in iso-8859-7:2003 compared to iso-8859-7:1987:
	// "2.0 version updates 1.0 version by adding mappings for the
	//  three newly added characters 0xA4, 0xA5, 0xAA."

	int rc = 1;

	const char input[] = "\xA4\xA5\xAA";
	const uni_char * const expect = UNI_L("\x20AC\x20AF\x037A");
	rc &= encodings_perform_tests("iso-8859-7", input, sizeof input,
	                              expect, unicode_bytelen(expect));

	return rc;
}

// Single-byte tests
static const char sbinput[] =  "\x93\xC0\xC1\xC2\x94\xFF";
static const char sbinput2[] = "\x93\xC0\xC1\xC2\x94";
static const char sbinput3[] = "\x93\xC1\xC2\x94";
static const char macinput[] = "\x80\x81\x82\x83\x84\x85";
static const char macturkish[] = "\xDA\xDB\xDC\xDD\xDE\xDF";

// Test helper
int encodings_test_sbcs(const char *encoding, const uni_char *utf16, const char *legacy, size_t legacy_len)
{
	const uni_char * const invalid_sb_input = UNI_L("Invalid in singlebyte: \x3000\x3105\x3400");
	const uni_char * const invalids_sb = UNI_L("\x3000\x3105\x3400");
	const char entity_sb[] = "Invalid in singlebyte: &#12288;&#12549;&#13312;";

#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined SELFTEST
	if (!g_table_manager->Has(encoding))
	{
		OUTPUT(__FILE__, __LINE__, 1, "\nConversion table \"%s\" MISSING  ", encoding);
		return 0;
	}
#endif

	int ok = 1;
	ok &= encodings_perform_tests(encoding, legacy, legacy_len, utf16, legacy_len * 2);
	ok &= encodings_perform_tests(encoding, "ABC", 4, UNI_L("ABC"), 8);
	ok &= encodings_test_invalid(encoding, invalid_sb_input, unicode_bytelen(invalid_sb_input), invalids_sb, unicode_bytelen(invalids_sb), 3, 23, entity_sb, sizeof entity_sb);

	int read, written;
	OutputConverter *fullwidth_conv_sbcs = encodings_new_reverse(encoding);
	encodings_check(fullwidth_conv_sbcs);
	written = fullwidth_conv_sbcs->Convert(fullwidth_ascii(), unicode_bytelen(fullwidth_ascii()), encodings_bufferback, ENC_TESTBUF_SIZE, &read);
	OP_DELETE(fullwidth_conv_sbcs);
	encodings_check(read == int(unicode_bytelen(fullwidth_ascii())));
	encodings_check(written == sizeof fullwidth_expect);
	encodings_check(op_memcmp(encodings_bufferback, fullwidth_expect, sizeof fullwidth_expect) == 0);

	// CORE-45016
	ok &= encodings_perform_tests(encoding, input45016, sizeof input45016, expect45016(), unicode_bytelen(expect45016()));

	return ok;
}

int encodings_test_iso_8859_2(void)
{
	const uni_char iso_8859_2[] = { 0x0093, 0x0154, 0x00C1, 0x00C2, 0x0094, 0x02D9, 0 };
	return encodings_test_sbcs("iso-8859-2", iso_8859_2, sbinput, sizeof sbinput);
}

int encodings_test_iso_8859_3(void)
{
	const uni_char iso_8859_3[] = { 0x0093, 0x00C0, 0x00C1, 0x00C2, 0x0094, 0x02D9, 0 };
	return encodings_test_sbcs("iso-8859-3", iso_8859_3, sbinput, sizeof sbinput);
}

int encodings_test_iso_8859_4(void)
{
	const uni_char iso_8859_4[] = { 0x0093, 0x0100, 0x00C1, 0x00C2, 0x0094, 0x02D9, 0 };
	return encodings_test_sbcs("iso-8859-4", iso_8859_4, sbinput, sizeof sbinput);
}

int encodings_test_iso_8859_5(void)
{
	const uni_char iso_8859_5[] = { 0x0093, 0x0420, 0x0421, 0x0422, 0x0094, 0x045F, 0 };
	return encodings_test_sbcs("iso-8859-5", iso_8859_5, sbinput, sizeof sbinput);
}

int encodings_test_iso_8859_6(void)
{
	const uni_char iso_8859_6[] = { 0x0093, 0x0621, 0x0622, 0x0094,                 0 };
	return encodings_test_sbcs("iso-8859-6", iso_8859_6, sbinput3, sizeof sbinput3);
}

int encodings_test_iso_8859_7(void)
{
	const uni_char iso_8859_7[] = { 0x0093, 0x0390, 0x0391, 0x0392, 0x0094,         0 };
	return encodings_test_sbcs("iso-8859-7", iso_8859_7, sbinput2, sizeof sbinput2);
}

int encodings_test_iso_8859_8(void)
{
	const char iso_8859_8_input[]      = "\xFE\xE0\xE1\xE2\xFD";
	const uni_char iso_8859_8_output[] = { 0x200F, 0x05D0, 0x05D1, 0x05D2, 0x200E, 0 };
	return encodings_test_sbcs("iso-8859-8", iso_8859_8_output, iso_8859_8_input, sizeof iso_8859_8_input);
}

/* windows-1254 is used instead
int encodings_test_iso_8859_9(void)
{
	const uni_char iso_8859_9[] = { 0x0093, 0x00C0, 0x00C1, 0x00C2, 0x0094, 0x00FF, 0 };
	return encodings_test_sbcs("iso-8859-9", iso_8859_9, sbinput, sizeof sbinput);
}
*/

int encodings_test_iso_8859_10(void)
{
	const uni_char iso_8859_10[]= { 0x0093, 0x0100, 0x00C1, 0x00C2, 0x0094, 0x0138, 0 };
	return encodings_test_sbcs("iso-8859-10", iso_8859_10, sbinput, sizeof sbinput);
}

int encodings_test_iso_8859_11(void)
{
	const char iso_8859_11_input[]      = "\xA1\xA2\xA3";
	const uni_char iso_8859_11_output[] = { 0x0E01, 0x0E02, 0x0E03, 0 };
	return encodings_test_sbcs("iso-8859-11", iso_8859_11_output, iso_8859_11_input, sizeof iso_8859_11_input);
}

int encodings_test_iso_8859_13(void)
{
	const uni_char iso_8859_13[]= { 0x0093, 0x0104, 0x012E, 0x0100, 0x0094, 0x2019, 0 };
	return encodings_test_sbcs("iso-8859-13", iso_8859_13, sbinput, sizeof sbinput);
}

int encodings_test_iso_8859_14(void)
{
	const uni_char iso_8859_14[]= { 0x0093, 0x00C0, 0x00C1, 0x00C2, 0x0094, 0x00FF, 0 };
	return encodings_test_sbcs("iso-8859-14", iso_8859_14, sbinput, sizeof sbinput);
}

int encodings_test_iso_8859_15(void)
{
	const uni_char iso_8859_15[]= { 0x0093, 0x00C0, 0x00C1, 0x00C2, 0x0094, 0x00FF, 0 };
	return encodings_test_sbcs("iso-8859-15", iso_8859_15, sbinput, sizeof sbinput);
}

int encodings_test_iso_8859_16(void)
{
	const uni_char iso_8859_16[]= { 0x0093, 0x00C0, 0x00C1, 0x00C2, 0x0094, 0x00FF, 0 };
	return encodings_test_sbcs("iso-8859-16", iso_8859_16, sbinput, sizeof sbinput);
}

int encodings_test_ibm866(void)
{
	return encodings_test_sbcs("ibm866", UNI_L("\x0423\x2514\x2534\x252C\x0424\x00A0"), sbinput, sizeof sbinput);
}

int encodings_test_windows_1250(void)
{
	return encodings_test_sbcs("windows-1250", UNI_L("\x201C\x0154\x00C1\x00C2\x201D\x02D9"), sbinput, sizeof sbinput);
}

int encodings_test_windows_1251(void)
{
	return encodings_test_sbcs("windows-1251", UNI_L("\x201C\x0410\x0411\x0412\x201D\x044F"), sbinput, sizeof sbinput);
}

int encodings_test_windows_1252(void)
{
	return encodings_test_sbcs("windows-1252", UNI_L("\x201C\x00C0\x00C1\x00C2\x201D\x00FF"), sbinput, sizeof sbinput);
}

int encodings_test_windows_1253(void)
{
	return encodings_test_sbcs("windows-1253", UNI_L("\x201C\x0390\x0391\x0392\x201D"), sbinput2, sizeof sbinput2);
}

int encodings_test_windows_1254(void)
{
	return encodings_test_sbcs("windows-1254", UNI_L("\x201C\x00C0\x00C1\x00C2\x201D\x00FF"), sbinput, sizeof sbinput);
}

int encodings_test_windows_1255(void)
{
	return encodings_test_sbcs("windows-1255", UNI_L("\x201C\x05B0\x05B1\x05B2\x201D"), sbinput2, sizeof sbinput2);
}

int encodings_test_windows_1256(void)
{
	return encodings_test_sbcs("windows-1256", UNI_L("\x201C\x06C1\x0621\x0622\x201D\x06D2"), sbinput, sizeof sbinput);
}

int encodings_test_windows_1257(void)
{
	return encodings_test_sbcs("windows-1257", UNI_L("\x201C\x0104\x012E\x0100\x201D\x02D9"), sbinput, sizeof sbinput);
}

int encodings_test_windows_1258(void)
{
	return encodings_test_sbcs("windows-1258", UNI_L("\x201C\x00C0\x00C1\x00C2\x201D\x00FF"), sbinput, sizeof sbinput);
}

int encodings_test_mac_roman(void)
{
	const uni_char mac_roman[]= { 0x00C4, 0x00C5, 0x00C7, 0x00C9, 0x00D1, 0x00D6, 0 };
	return encodings_test_sbcs("macintosh", mac_roman, macinput, sizeof macinput);
}

int encodings_test_mac_ce(void)
{
	const uni_char mac_ce[]= { 0x00C4, 0x0100, 0x0101, 0x00C9, 0x0104, 0x00D6, 0 };
	return encodings_test_sbcs("x-mac-ce", mac_ce, macinput, sizeof macinput);
}

int encodings_test_mac_cyrillic(void)
{
	const uni_char mac_cyrillic[]= { 0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0 };
	return encodings_test_sbcs("x-mac-cyrillic", mac_cyrillic, macinput, sizeof macinput);
}

int encodings_test_mac_greek(void)
{
	const uni_char mac_greek[]= { 0x00C4, 0x00B9, 0x00B2, 0x00C9, 0x00B3, 0x00D6, 0 };
	return encodings_test_sbcs("x-mac-greek", mac_greek, macinput, sizeof macinput);
}

int encodings_test_mac_turkish(void)
{
	const uni_char mac_turkish[]= { 0x011E, 0x011F, 0x0130, 0x0131, 0x015E, 0x015F, 0 };
	return encodings_test_sbcs("x-mac-turkish", mac_turkish, macturkish, sizeof macturkish);
}

int encodings_test_viscii(void)
{
	return encodings_test_sbcs("viscii", UNI_L("\x1ED8\x00C0\x00C1\x00C2\x1EE2\x1EEE"), sbinput, sizeof sbinput);
}

int encodings_test_koi8r(void)
{
	return encodings_test_sbcs("koi8-r", UNI_L("\x2320\x044E\x0430\x0431\x25A0\x042A"), sbinput, sizeof sbinput);
}

int encodings_test_koi8u(void)
{
	return encodings_test_sbcs("koi8-u", UNI_L("\x2320\x044E\x0430\x0431\x25A0\x042A"), sbinput, sizeof sbinput);
}

#endif // SELFTEST || ENCODINGS_REGTEST
