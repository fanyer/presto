/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
   On Compiling this file COMPILING_ENCODINGS_CHARCONV_TESTSUITE is
   defined. On a windows or linux platform some system headers are
   needed if FEATURE_SELFTEST is enabled.

   On a WIN32 platform (i.e. WIN32 is defined), the header file
   <windows.h> is included to use the functions ::VirtualAlloc()
   and ::VirtualFree().

   On a linux platform (i.e. linux is defined), the header file
   <sys/mman.h> is included to use the function mprotect().

   The platform's system.h might need to prepare for these system
   header files to be included.
 */
#define COMPILING_ENCODINGS_CHARCONV_TESTSUITE
#include "core/pch_system_includes.h"
#if defined SELFTEST || defined ENCODINGS_REGTEST
#include "modules/encodings/testsuite/charconv/chartest.h"

#ifdef SELFTEST
// Input converters ------------------------------------------------------
#include "modules/encodings/decoders/inputconverter.h"
#include "modules/encodings/decoders/identityconverter.h"
#include "modules/encodings/decoders/iso-8859-1-decoder.h"
#include "modules/encodings/decoders/utf7-decoder.h"
#include "modules/encodings/decoders/utf8-decoder.h"
#include "modules/encodings/decoders/utf16-decoder.h"
#include "modules/encodings/decoders/big5-decoder.h"
#include "modules/encodings/decoders/big5hkscs-decoder.h"
#include "modules/encodings/decoders/euc-jp-decoder.h"
#include "modules/encodings/decoders/euc-kr-decoder.h"
#include "modules/encodings/decoders/euc-tw-decoder.h"
#include "modules/encodings/decoders/gbk-decoder.h"
#include "modules/encodings/decoders/hz-decoder.h"
#include "modules/encodings/decoders/iso-2022-cn-decoder.h"
#include "modules/encodings/decoders/iso-2022-jp-decoder.h"
#include "modules/encodings/decoders/sbcs-decoder.h"
#include "modules/encodings/decoders/sjis-decoder.h"

// Output converters -----------------------------------------------------
#include "modules/encodings/encoders/outputconverter.h"
#include "modules/encodings/encoders/utf7-encoder.h"
#include "modules/encodings/encoders/utf8-encoder.h"
#include "modules/encodings/encoders/utf16-encoder.h"
#include "modules/encodings/encoders/iso-8859-1-encoder.h"
#include "modules/encodings/encoders/big5hkscs-encoder.h"
#include "modules/encodings/encoders/dbcs-encoder.h"
#include "modules/encodings/encoders/euc-tw-encoder.h"
#include "modules/encodings/encoders/gb18030-encoder.h"
#include "modules/encodings/encoders/hz-encoder.h"
#include "modules/encodings/encoders/iso-2022-cn-encoder.h"
#include "modules/encodings/encoders/jis-encoder.h"
#include "modules/encodings/encoders/sbcs-encoder.h"
#endif // SELFTEST

#if defined WIN32
# ifdef ENCODINGS_REGTEST
#  undef BOOL
# endif
# include <windows.h>
# ifdef ENCODINGS_REGTEST
#  define BOOL bool
# endif
#endif // WIN32

#ifdef ENCODINGS_REGTEST
# include "tablemanager/tablemanager.h"
#endif

#if defined linux
# include <sys/mman.h>
#endif

#include "modules/encodings/testsuite/charconv/utility.h"

#ifdef OPMEMORY_VIRTUAL_MEMORY
# include "modules/pi/system/OpMemory.h"
#endif //OPMEMORY_VIRTUAL_MEMORY

#ifdef SELFTEST
# define encodings_buffer     g_opera->encodings_module.buffer
# define encodings_bufferback g_opera->encodings_module.bufferback
#else
extern char *encodings_buffer, *encodings_bufferback;
#endif

int fliputf16endian(const void *src, void *dest, size_t bytes)
{
	const char *input = reinterpret_cast<const char *>(src);
	char *output = reinterpret_cast<char *>(dest);
	encodings_check((bytes & 1) == 0);
	while (bytes)
	{
		output[1] = *(input ++);
		output[0] = *(input ++);
		output += 2;
		bytes -= 2;
	}

	return 1;
}

#ifdef SELFTEST
/** Factory for the test suite */
InputConverter *encodings_new_forward(const char *encoding)
{
	InputConverter *out = NULL;

	// Special handling for UTF-7
#ifdef ENCODINGS_HAVE_UTF7
# ifdef ENCODINGS_HAVE_UTF7IMAP
	if (0 == op_strncmp(encoding, "utf-7-imap", 11))
	{
		out = OP_NEW(UTF7toUTF16Converter, (UTF7toUTF16Converter::IMAP));
		if (out && OpStatus::IsError(out->Construct()))
		{
			OP_DELETE(out);
			out = NULL;
		}
		return out;
	}
	else
# endif // ENCODINGS_HAVE_UTF7IMAP
	if (0 == op_strncmp(encoding, "utf-7-safe", 11))
	{
		out = OP_NEW(UTF7toUTF16Converter, (UTF7toUTF16Converter::STANDARD));
		if (out && OpStatus::IsError(out->Construct()))
		{
			OP_DELETE(out);
			out = NULL;
		}
		return out;
	}
	else if (0 == op_strncmp(encoding, "utf-7", 6))
	{
		out = OP_NEW(UTF7toUTF16Converter, (UTF7toUTF16Converter::STANDARD));
		if (out && OpStatus::IsError(out->Construct()))
		{
			OP_DELETE(out);
			out = NULL;
		}
	}
	else
#endif // ENCODINGS_HAVE_UTF7
	// For the rest, use the default factory
	{
		OP_STATUS rc = InputConverter::CreateCharConverter(encoding, &out);
		if (OpStatus::IsError(rc))
		{
			TEST_FAILED(__FILE__, __LINE__, "Error received in encodings_new_forward");
		}
	}

	if (out)
	{
		if (0 == op_strcmp(encoding, "iso-8859-1") || 0 == op_strcmp(encoding, "us-ascii"))
		{
			encodings_check(op_strcmp(out->GetCharacterSet(), encoding) == 0 || op_strcmp(out->GetCharacterSet(), "windows-1252") == 0);
		}
		else
		{
			encodings_check(op_strcmp(out->GetCharacterSet(), encoding) == 0);
		}
		encodings_check(op_strcmp(out->GetDestinationCharacterSet(), "utf-16") == 0);
		return out;
	}

	TEST_FAILED(__FILE__, __LINE__, "Invalid encoding in encodings_new_forward");
	return NULL;
}

OutputConverter *encodings_new_reverse(const char *encoding)
{
	OutputConverter *out = NULL;

	// Special handling for UTF-7
#ifdef ENCODINGS_HAVE_UTF7
	if (0 == op_strncmp(encoding, "utf-7", 6))
	{
		out = OP_NEW(UTF16toUTF7Converter, (UTF16toUTF7Converter::STANDARD));
		if (out && OpStatus::IsError(out->Construct()))
		{
			OP_DELETE(out);
			out = NULL;
		}
	}
	else if (0 == op_strncmp(encoding, "utf-7-safe", 11))
	{
		out = OP_NEW(UTF16toUTF7Converter, (UTF16toUTF7Converter::SAFE));
		if (out && OpStatus::IsError(out->Construct()))
		{
			OP_DELETE(out);
			out = NULL;
		}
		return out;
	}
# ifdef ENCODINGS_HAVE_UTF7IMAP
	else if (0 == op_strncmp(encoding, "utf-7-imap", 11))
	{
		out = OP_NEW(UTF16toUTF7Converter, (UTF16toUTF7Converter::IMAP));
		if (out && OpStatus::IsError(out->Construct()))
		{
			OP_DELETE(out);
			out = NULL;
		}
		return out;
	}
# endif // ENCODINGS_HAVE_UTF7IMAP
	else
#endif // ENCODINGS_HAVE_UTF7
    {
		OP_STATUS rc = OutputConverter::CreateCharConverter(encoding, &out, TRUE, TRUE);
		if (OpStatus::IsError(rc))
		{
			TEST_FAILED(__FILE__, __LINE__, "Error received in encodings_new_reverse");
		}
	}
	if (out)
	{
		if (0 == op_strcmp(encoding, "iso-8859-6") || 0 == op_strcmp(encoding, "iso-8859-8"))
		{
			encodings_check(op_strncmp(out->GetDestinationCharacterSet(), encoding, op_strlen(encoding)) == 0);
		}
		else
		{
			encodings_check(op_strcmp(out->GetDestinationCharacterSet(), encoding) == 0);
		}
		encodings_check(op_strcmp(out->GetCharacterSet(), "utf-16") == 0);
		return out;
	}

	TEST_FAILED(__FILE__, __LINE__, "Invalid encoding in encodings_new_reverse");
	return NULL;
}
#endif // SELFTEST

#ifdef HEXDUMP
void hexdump(const char *s, const void *_p, int n)
{
	OUTPUT_STR("\n");
	OUTPUT_STR(s);
	OUTPUT_STR(": ");
	const unsigned char *p = reinterpret_cast<const unsigned char *>(_p);
	char o[4] = { 0, 0, ' ', 0 };
	while (n --)
	{
		const unsigned char cur = *(p ++);
		o[0] = (cur >> 4)["0123456789ABCDEF"];
		o[1] = (cur & 15)["0123456789ABCDEF"];
		OUTPUT_STR(o);
	}
}
#endif // HEXDUMP

// Perform a test, converting in both directions
int encodings_perform_test(InputConverter *fw, OutputConverter *rev,
				           const void *input, int inputlen,
				           const void *expected, int expectedlen,
				           const void *expectback/* = NULL*/,
				           int expectbacklen/* = -1*/,
				           BOOL hasinvalid/* = FALSE*/,
				           const uni_char *invalids/* = NULL*/)
{
	int read, written;

	if (!expectback || expectbacklen < 0)
	{
		expectback = input;
		expectbacklen = inputlen;
	}

	// Verify preconditions
	encodings_check(fw && rev && input && inputlen > 0 && expected && expectedlen > 0 && expectback && expectbacklen > 0);

	// Check forward conversion
	written = fw->Convert(input, inputlen, encodings_buffer, ENC_TESTBUF_SIZE, &read);

#ifdef HEXDUMP
	if (read != inputlen || written != expectedlen || op_memcmp(encodings_buffer, expected, expectedlen))
	{
		hexdump("input   ", input, inputlen);
		hexdump("expected", expected, expectedlen);
		hexdump("output  ", encodings_buffer, written);
	}
#endif
	encodings_check(read == inputlen);
	encodings_check(written == expectedlen);
	encodings_check(0 == op_memcmp(encodings_buffer, expected, expectedlen));
#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	encodings_check(fw->IsValidEndState());
#endif

	// Check reverse conversion
	written = rev->Convert(encodings_buffer, expectedlen, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
#ifdef HEXDUMP
	if (read != expectedlen || written != expectbacklen || op_memcmp(encodings_bufferback, expectback, expectbacklen))
	{
		hexdump("expectback", expectback, expectbacklen);
		hexdump("reverse   ", encodings_bufferback, written);
	}
#endif
	encodings_check(read == expectedlen);
	encodings_check(written == expectbacklen);
	encodings_check(0 == op_memcmp(encodings_bufferback, expectback, expectbacklen));
	if (hasinvalid) {
		encodings_check(rev->GetNumberOfInvalid() > 0);
		encodings_check(rev->GetFirstInvalidOffset() >= 0);
	}
#ifdef ENCODINGS_HAVE_SPECIFY_INVALID
	if (invalids) encodings_check(0 == op_memcmp(rev->GetInvalidCharacters(), invalids, uni_strlen(invalids) * sizeof (uni_char)));
#endif

	OP_DELETE(fw);
	OP_DELETE(rev);

	return 1;
}

int encodings_perform_tests(const char *encoding,
						     const void *input, int inputlen,
						     const void *expected, int expectedlen,
						     const void *expectback/* = NULL*/,
						     int expectbacklen/* = -1*/,
						     BOOL hasinvalid/* = FALSE*/,
						     BOOL mayvary/* = FALSE*/,
						     const uni_char *invalids/* = NULL*/)
{
	int read, read2, written;
	const unsigned char *inp = reinterpret_cast<const unsigned char *>(input);
	const unsigned char *exp = reinterpret_cast<const unsigned char *>(expected);
	bool is_utf = !op_strcmp(encoding, "utf-32") || !op_strcmp(encoding, "utf-16");

	if (!expectback || expectbacklen < 0)
	{
		expectback = input;
		expectbacklen = inputlen;
	}

	// Verify preconditions
	encodings_check(encoding && input && inputlen > 0 && expected && expectedlen > 0 && expectback && expectbacklen > 0);

	// Create first encoder and decoder and check that encoding tags are
	// correct.
	InputConverter *fw1 = encodings_new_forward(encoding);
	encodings_check(fw1 != NULL);
	if (op_strncmp(fw1->GetCharacterSet(), encoding, op_strlen(fw1->GetCharacterSet())))
	{
		if (op_strcmp(encoding, "us-ascii") != 0 && op_strcmp(encoding, "iso-8859-1") != 0)
		{
			TEST_FAILED(__FILE__, __LINE__, "Asked for \"%s\" got \"%s\"", encoding, fw1->GetCharacterSet());
			return 0;
		}
	}
	encodings_check(0 == op_strcmp(fw1->GetDestinationCharacterSet(), "utf-16"));
	OutputConverter *rev1 = encodings_new_reverse(encoding);
	encodings_check(rev1 != NULL);
	encodings_check(0 == op_strcmp(rev1->GetCharacterSet(), "utf-16"));
	if (op_strncmp(rev1->GetDestinationCharacterSet(), encoding, op_strlen(rev1->GetDestinationCharacterSet())))
	{
		if (0 != op_strncmp(encoding, fw1->GetCharacterSet(), op_strlen(encoding) &&
			0 != op_strcmp(fw1->GetCharacterSet() + op_strlen(encoding), "-i")))
		{
			encodings_check(0 == op_strncmp(rev1->GetDestinationCharacterSet(), encoding, op_strlen(rev1->GetDestinationCharacterSet())));
		}
	}

	// Standard test
	encodings_perform_test(fw1, rev1, input, inputlen,
	                       expected, expectedlen, expectback, expectbacklen,
	                       hasinvalid);

	// Try to provoke a bus error
	char *expectback_buserror = 0;
#ifdef SELFTEST
# define encodings_check_buserror(what) \
		do { \
			if (!(what)) { \
				TEST_FAILED(__FILE__, __LINE__, "Condition failed: " #what); \
				OP_DELETEA(input_buserror);	\
				OP_DELETEA(expected_buserror); \
				OP_DELETEA(expectback_buserror); \
				return 0; \
			} \
		} while(0)
#else
# define encodings_check_buserror(what) assert(what)
#endif

	char *input_buserror = OP_NEWA(char, inputlen + 1);
	encodings_check(input_buserror != NULL);
	char *expected_buserror = OP_NEWA(char, expectedlen + 1);
	encodings_check_buserror(expected_buserror != NULL);
	expectback_buserror = OP_NEWA(char, expectbacklen + 1);
	encodings_check_buserror(expectback_buserror != NULL);
	op_memcpy(input_buserror + 1, input, inputlen);
	op_memcpy(expected_buserror + 1, expected, expectedlen);
	op_memcpy(expectback_buserror + 1, expectback, expectbacklen);
	InputConverter *fw2 = encodings_new_forward(encoding);
	encodings_check_buserror(fw2 != NULL);
	OutputConverter *rev2 = encodings_new_reverse(encoding);
	encodings_check_buserror(rev2 != NULL);
	encodings_perform_test(fw2, rev2,
		input_buserror + 1, inputlen,
		expected_buserror + 1, expectedlen,
		expectback_buserror + 1, expectbacklen,
		hasinvalid);
	OP_DELETEA(input_buserror);
	OP_DELETEA(expected_buserror);
	OP_DELETEA(expectback_buserror);

	// Split boundary tests
	// 1. Not entire input on first go
	//   a) forward conversion
	for (int i = 1; i < inputlen; i ++)
	{
		InputConverter *fw = encodings_new_forward(encoding);
		encodings_check(fw != NULL);
		written = fw->Convert(input, i, encodings_buffer, ENC_TESTBUF_SIZE, &read);
		encodings_check(read <= i);
		encodings_check(written <= expectedlen);
		if (read < i && !is_utf)
		{
			OUTPUT(__FILE__, __LINE__, 1, "Not all data read: %d < %d", read, i);
		}
		written += fw->Convert(&inp[read], inputlen - read, &encodings_buffer[written], ENC_TESTBUF_SIZE - written, &read2);
		encodings_check(read + read2 == inputlen);
		encodings_check(written == expectedlen);
		encodings_check(0 == op_memcmp(encodings_buffer, expected, expectedlen));
#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
		encodings_check(fw->IsValidEndState());
#endif
		OP_DELETE(fw);
	}

	BOOL debug = FALSE;

	//   b) reverse conversion
	for (int j = 1; j < expectedlen; j ++)
	{
		OutputConverter *rev = encodings_new_reverse(encoding);
		encodings_check(rev != NULL);
		written = rev->Convert(expected, j, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
		encodings_check(read <= j);
		encodings_check(written <= expectbacklen);
		if (read < j && !(j & 1) && !is_utf)
		{
			OUTPUT(__FILE__, __LINE__, 1, "Not all data read: %d < %d", read, j);
		}
		written += rev->Convert(&exp[read], expectedlen - read, &encodings_bufferback[written], ENC_TESTBUF_SIZE - written, &read2);
		encodings_check(read + read2 == expectedlen);
		if (mayvary)
		{
			encodings_check(written >= expectbacklen);
			if (written > expectbacklen || op_memcmp(encodings_bufferback, expectback, expectbacklen) != 0)
			{
				// If output is different, make sure it still converts to
				// the same Unicode stream
				CharConverter *fw = encodings_new_forward(encoding);
				encodings_check(fw != NULL);
				int written2 = fw->Convert(encodings_bufferback, written, encodings_buffer, ENC_TESTBUF_SIZE, &read2);
				encodings_check(read2 == written);
				encodings_check(written2 == expectedlen);
				encodings_check(0 == op_memcmp(encodings_buffer, expected, expectedlen));
#ifdef ENCODINGS_REGTEST
				output1(NULL, 0, " ... accepting \"%s\"\n", reinterpret_cast<char *>(encodings_bufferback));
#endif
				debug = TRUE;
				OP_DELETE(fw);
			}
		}
		else
		{
#ifdef HEXDUMP
			if (written != expectbacklen)
			{
				hexdump("output", encodings_bufferback, written);
				hexdump("expect", expectback, expectbacklen);
			}
#endif
			encodings_check(written == expectbacklen);
			encodings_check(0 == op_memcmp(encodings_bufferback, expectback, expectbacklen));
		}
		if (hasinvalid) {
			encodings_check(rev->GetNumberOfInvalid() > 0);
			encodings_check(rev->GetFirstInvalidOffset() >= 0);
		}
#ifdef ENCODINGS_HAVE_SPECIFY_INVALID
		if (invalids) encodings_check(0 == op_memcmp(rev->GetInvalidCharacters(), invalids, uni_strlen(invalids) * sizeof (uni_char)));
#endif
		OP_DELETE(rev);
	}

	// 2. Not enough space on first go
	//   a) forward conversion
	for (int k = 1; k < expectedlen; k ++)
	{
		InputConverter *fw = encodings_new_forward(encoding);
		encodings_check(fw != NULL);
		written = fw->Convert(input, inputlen, encodings_buffer, k, &read);
		encodings_check(read <= inputlen);
		encodings_check(written <= k);
		written += fw->Convert(&inp[read], inputlen - read, &encodings_buffer[written], ENC_TESTBUF_SIZE - written, &read2);
		encodings_check(read + read2 == inputlen);
		encodings_check(written == expectedlen);
		encodings_check(0 == op_memcmp(encodings_buffer, expected, expectedlen));
#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
		encodings_check(fw->IsValidEndState());
#endif
		OP_DELETE(fw);
	}

	//   b) reverse conversion
	for (int l = 1; l < expectbacklen; l ++)
	{
		OutputConverter *rev = encodings_new_reverse(encoding);
		encodings_check(rev != NULL);
		written = rev->Convert(expected, expectedlen, encodings_bufferback, l, &read);
		encodings_check(read <= expectedlen);
		encodings_check(written <= l);
		written += rev->Convert(&exp[read], expectedlen - read, &encodings_bufferback[written], ENC_TESTBUF_SIZE - written, &read2);
		encodings_check(read + read2 == expectedlen);
		if (mayvary)
		{
			encodings_check(written >= expectbacklen);
			if (written > expectbacklen || op_memcmp(encodings_bufferback, expectback, expectbacklen) != 0)
			{
				// If output is different, make sure it still converts to
				// the same Unicode stream
				CharConverter *fw = encodings_new_forward(encoding);
				encodings_check(fw != NULL);
				int written2 = fw->Convert(encodings_bufferback, written, encodings_buffer, ENC_TESTBUF_SIZE, &read2);
				encodings_check(read2 == written);
				encodings_check(written2 == expectedlen);
				encodings_check(0 == op_memcmp(encodings_buffer, expected, expectedlen));
#ifdef ENCODINGS_REGTEST
				output1(NULL, 0, " ... accepting \"%s\"\n", reinterpret_cast<char *>(encodings_bufferback));
#endif
				debug = TRUE;
				OP_DELETE(fw);
			}
		}
		else
		{
			encodings_check(written == expectbacklen);
			encodings_check(0 == op_memcmp(encodings_bufferback, expectback, expectbacklen));
		}
		if (hasinvalid) {
			encodings_check(rev->GetNumberOfInvalid() > 0);
			encodings_check(rev->GetFirstInvalidOffset() >= 0);
		}
#ifdef ENCODINGS_HAVE_SPECIFY_INVALID
		if (invalids) encodings_check(0 == op_memcmp(rev->GetInvalidCharacters(), invalids, uni_strlen(invalids) * sizeof (uni_char)));
#endif
		OP_DELETE(rev);
	}

	// Test return to initial state functionality
	if (op_strcmp(encoding, "utf-16"))
	{
		OutputConverter *rev = encodings_new_reverse(encoding);
		encodings_check(rev != NULL);
		written = 0;
		read = 0;
		int retlen;
		int longest;
		while (read < expectedlen)
		{
			longest = rev->LongestSelfContainedSequenceForCharacter();
			encodings_check(rev->ReturnToInitialState(NULL) < longest);
			written += retlen = rev->ReturnToInitialState(&encodings_bufferback[written]);
			encodings_check(retlen <= longest);
			written += rev->Convert(&exp[read], sizeof (uni_char), &encodings_bufferback[written], ENC_TESTBUF_SIZE - written, &read2);
			read += read2;
			encodings_check(read <= expectedlen);
		}
		longest = rev->LongestSelfContainedSequenceForCharacter();
		written += retlen = rev->ReturnToInitialState(&encodings_bufferback[written]);
		encodings_check(retlen <= longest);
		OP_DELETE(rev);

		if (mayvary)
		{
			encodings_check(written >= expectbacklen);
			if (written > expectbacklen || op_memcmp(encodings_bufferback, expectback, expectbacklen) != 0)
			{
				// If output is different, make sure it still converts to
				// the same Unicode stream
				CharConverter *fw = encodings_new_forward(encoding);
				encodings_check(fw != NULL);
				int written2 = fw->Convert(encodings_bufferback, written, encodings_buffer, ENC_TESTBUF_SIZE, &read2);
				encodings_check(read2 == written);
				encodings_check(written2 == expectedlen);
				encodings_check(0 == op_memcmp(encodings_buffer, expected, expectedlen));
#ifdef ENCODINGS_REGTEST
				output1(NULL, 0, " ... accepting \"%s\"\n", reinterpret_cast<char *>(encodings_bufferback));
#endif
				debug = TRUE;
				OP_DELETE(fw);
			}
		}
		else
		{
			encodings_check(written == expectbacklen);
			encodings_check(0 == op_memcmp(encodings_bufferback, expectback, expectbacklen));
		}
	}

	// Test reset functionality
	for (int m = 1; m <= expectedlen; m ++)
	{
		// Convert a part (or all) of the buffer
		OutputConverter *rev = encodings_new_reverse(encoding);
		encodings_check(rev != NULL);
		written = rev->Convert(expected, m, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
		encodings_check(read <= m);
		encodings_check(written <= expectbacklen);

		// Reset and convert all of the buffer, this behave the same
		// if the previous call didn't happen
		rev->Reset();
		written = rev->Convert(expected, expectedlen, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
		encodings_check(read == expectedlen);
		encodings_check(written == expectbacklen);
		encodings_check(0 == op_memcmp(encodings_bufferback, expectback, expectbacklen));
		if (hasinvalid) {
			encodings_check(rev->GetNumberOfInvalid() > 0);
			encodings_check(rev->GetFirstInvalidOffset() >= 0);
		}
#ifdef ENCODINGS_HAVE_SPECIFY_INVALID
		if (invalids) encodings_check(0 == op_memcmp(rev->GetInvalidCharacters(), invalids, uni_strlen(invalids) * sizeof (uni_char)));
#endif
		OP_DELETE(rev);
	}

	if (debug)
	{
#ifdef ENCODINGS_REGTEST
		output1(NULL, 0, " ... (instead of \"%s\")\n", reinterpret_cast<const char *>(expectback));
#endif
	}

	// Buffer overrun tests
	encodings_test_buffers(encoding, input, inputlen, expected, expectedlen, expectback, expectbacklen);

	return 1;
}

int encodings_test_buffers(const char *encoding,
                           const void *input, int inputlen,
                           const void *expected, int expectedlen,
                           const void *expectback, int expectbacklen)
{
#if defined WIN32 || defined linux
	// Allocate two consecutive memory pages, where the second is
	// write-protected.

# ifdef WIN32
	static const size_t page_size = 65536;

	char *inptr = reinterpret_cast<char *>(::VirtualAlloc(NULL, page_size * 2, MEM_RESERVE, PAGE_NOACCESS));
	::VirtualAlloc(inptr, page_size, MEM_COMMIT, PAGE_READWRITE);

	char *outptr = reinterpret_cast<char *>(::VirtualAlloc(NULL, page_size * 2, MEM_RESERVE, PAGE_NOACCESS));
	::VirtualAlloc(outptr, page_size, MEM_COMMIT, PAGE_READWRITE);
# elif defined linux
	static const size_t page_size =
#  ifdef OPMEMORY_VIRTUAL_MEMORY
		OpMemory::GetPageSize();
#  elif defined(PAGESIZE)
		PAGESIZE;
#  else
		4096;
#  endif
	void *real_inptr = op_malloc(page_size * 3);
	char *inptr =
		reinterpret_cast<char *>((reinterpret_cast<UINTPTR>(real_inptr) + page_size - 1) & ~(page_size - 1));
	mprotect(inptr + page_size, page_size, PROT_NONE);

	void *real_outptr = op_malloc(page_size * 3);
	char *outptr =
		reinterpret_cast<char *>((reinterpret_cast<UINTPTR>(real_outptr) + page_size - 1) & ~(page_size - 1));
	mprotect(outptr + page_size, page_size, PROT_NONE);
# endif // linux

	encodings_check(static_cast<size_t>(inputlen) <= page_size);
	encodings_check(static_cast<size_t>(expectedlen) <= page_size);

	void *new_input = inptr + page_size - inputlen;
	op_memcpy(new_input, input, inputlen);

	// Test InputConverter
	for (int i = 1; i <= expectedlen; ++ i)
	{
# ifdef NEEDS_RISC_ALIGNMENT
		if (i % 2) ++ i;
# endif
		InputConverter *fw = encodings_new_forward(encoding);
		encodings_check(fw != NULL);
		int read;
		int written = fw->Convert(new_input, inputlen, outptr + page_size - i, i, &read);
		encodings_check(read <= inputlen);
		encodings_check(written <= i);
		// Ignore conversion results, we've already checked that
		OP_DELETE(fw);
	}

	if (expectback && expectbacklen != -1)
	{
		void *new_expected = inptr + page_size - expectedlen;
		op_memcpy(new_expected, expected, expectedlen);

		// Test OutputConverter
		for (int j = 1; j <= expectbacklen; ++ j)
		{
# ifdef NEEDS_RISC_ALIGNMENT
			if (j % 2) ++ j;
# endif
			OutputConverter *rev = encodings_new_reverse(encoding);
			encodings_check(rev != NULL);
			int read;
			int written = rev->Convert(new_expected, expectedlen, outptr + page_size - j, j, &read);
			encodings_check(read <= expectedlen);
			encodings_check(written <= j);
			// Ignore conversion results, we've already checked that
			OP_DELETE(rev);
		}

		// Test OutputConverter using Reset() instead of recreating object
		OutputConverter *rev2 = encodings_new_reverse(encoding);
		encodings_check(rev2 != NULL);
		for (int k = 1; k <= expectbacklen; ++ k)
		{
# ifdef NEEDS_RISC_ALIGNMENT
			if (k % 2) ++ k;
# endif
			rev2->Reset();
			int read;
			int written = rev2->Convert(new_expected, expectedlen, outptr + page_size - k, k, &read);
			encodings_check(read <= expectedlen);
			encodings_check(written <= k);
			// Ignore conversion results, we've already checked that
		}
		OP_DELETE(rev2);
	}

	// Clean up
# ifdef WIN32
	::VirtualFree(inptr, page_size, MEM_DECOMMIT);
	::VirtualFree(inptr, 0, MEM_RELEASE);
	::VirtualFree(outptr, page_size, MEM_DECOMMIT);
	::VirtualFree(outptr, 0, MEM_RELEASE);
# elif defined linux
	mprotect(inptr  + page_size, page_size, PROT_READ | PROT_WRITE);
	op_free(real_inptr);
	mprotect(outptr + page_size, page_size, PROT_READ | PROT_WRITE);
	op_free(real_outptr);
# endif // linux
#else // neither WIN32 nor linux
# ifdef SELFTEST
	// FIXME: For other platforms, we should just set up a bait around the
	// allocated buffer, and check that it remains unchanged after the test
	// run.
	OUTPUT_STR("\nencodings_test_buffers() not ported to your platform; not testing for buffer overruns. ");
# endif
#endif // WIN32 || linux

	return 1;
}

int encodings_test_invalid(const char *encoding,
                           const uni_char *input, size_t inplen,
                           const uni_char *invalids, size_t invlen,
                           int numinvalids, int firstinvalid,
                           const char *entitystring/* = NULL*/,
                           size_t entitylen/* = 0*/)
{
#if defined ENCODINGS_HAVE_SPECIFY_INVALID || defined ENCODINGS_HAVE_ENTITY_ENCODING
	int read;
#endif
#ifdef ENCODINGS_HAVE_SPECIFY_INVALID
	OutputConverter *iconv = encodings_new_reverse(encoding);
	encodings_check(iconv != NULL);
	int written = iconv->Convert(input, inplen, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
	(void)written;
#ifdef HEXDUMP
	if (iconv->GetNumberOfInvalid() != numinvalids ||
	    iconv->GetFirstInvalidOffset() != firstinvalid)
	{
		hexdump("input   ", input, inplen);
		hexdump("output  ", encodings_bufferback, written);
		OUTPUT(__FILE__, __LINE__, 1, "\ngot %d invalid, expected %d invalid ", iconv->GetNumberOfInvalid(), numinvalids);
	}
#endif
	encodings_check(read == int(inplen));
	encodings_check(iconv->GetNumberOfInvalid() == numinvalids);
	encodings_check(iconv->GetFirstInvalidOffset() == firstinvalid);
	encodings_check(op_memcmp(iconv->GetInvalidCharacters(), invalids, invlen) == 0);
	OP_DELETE(iconv);
#endif // ENCODINGS_HAVE_SPECIFY_INVALID

#ifdef ENCODINGS_HAVE_ENTITY_ENCODING
	if (entitylen)
	{
		OutputConverter *iconv = encodings_new_reverse(encoding);
		encodings_check(iconv != NULL);
		iconv->EnableEntityEncoding(TRUE);
		int written = iconv->Convert(input, inplen, encodings_bufferback, ENC_TESTBUF_SIZE, &read);
		encodings_check(read == int(inplen) && written == int(entitylen) &&
		                op_memcmp(encodings_bufferback, entitystring, entitylen) == 0);
		OP_DELETE(iconv);
	}
#endif // ENCODINGS_HAVE_ENTITY_ENCODING

	return 1;
}

#ifdef ENCODINGS_REGTEST
// Copied from core/unicode.cpp:
int strni_eq(const char * str1, const char *str2, size_t len)
{
	while (len-- && *str1)
	{
		char c = *str1;

		if (c >= 'a' && c <= 'z')
		{
			if (*str2 != (c & 0xDF))
				return 0;
		}
		else if (*str2 != c)
			return 0;

		str1++, str2++;
	}

	return !*str2 || len == -1;
}
#endif // ENCODINGS_REGTEST

#endif // SELFTEST || ENCODINGS_REGTEST
