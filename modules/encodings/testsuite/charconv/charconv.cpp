/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

// Regression test for the Opera character converter classes.

#include "core/pch.h"
#include <cassert>
#include <cstdarg>
#include "tablemanager/filetablemanager.h"
#include "tablemanager/romtablemanager.h"
#include "chartest.h"
#ifdef MSWIN
#undef BOOL
#include <windows.h>
#endif

#include "encoders/encoder-utility.h"

char *encodings_buffer = NULL, *encodings_bufferback = NULL;
PrefsCollectionDisplay *g_pcdisplay = NULL;
MemoryManager *g_memory_manager = NULL;
OpTableManager *g_table_manager = NULL;
CharsetManager *g_charsetManager = NULL;
Opera *g_opera = NULL;

void test_encodings(void);

#ifndef MSWIN
void OutputDebugStringA(const char *s)
{
	fputs(s, stdout);
}

size_t my_wcslen(const wchar_t *s)
{
	const wchar_t *t = s;
	while (*t++ != 0)
		;
	return t - s - 1;
}
#endif

int main(void)
{
	// Buffers for testing
	encodings_buffer = OP_NEWA(char, ENC_TESTBUF_SIZE);
	encodings_bufferback = OP_NEWA(char, ENC_TESTBUF_SIZE);

	// Fake Opera
	g_opera = OP_NEW(Opera, ());
	g_pcdisplay = OP_NEW(PrefsCollectionDisplay, ());
	g_memory_manager = OP_NEW(MemoryManager, ());

	// Run the tests
	test_encodings();

	OP_DELETE(g_memory_manager);
	OP_DELETE(g_pcdisplay);
	OP_DELETEA(encodings_buffer);
	OP_DELETEA(encodings_bufferback);
	OP_DELETE(g_table_manager);
	OP_DELETE(g_opera);

	return 0;
}

// ====================================================================================
// MAIN DRIVER
// ====================================================================================

void test_encodings(void)
{
	OutputDebugStringA("Encodings:\n");

#ifdef ENCODINGS_HAVE_ROM_TABLES
	RomTableManager *new_table_manager = OP_NEW(RomTableManager, ());
#else
	FileTableManager *new_table_manager = OP_NEW(FileTableManager, ());
#endif
	new_table_manager->ConstructL();
	new_table_manager->Load();
	g_table_manager = new_table_manager;

	struct
	{
		const char *name;
		int (*entry)(void);
	} tests[] =
	{
		{	"iso-8859-1",	encodings_test_iso_8859_1	},
		{	"ascii",		encodings_test_ascii		},
#ifdef ENCODINGS_HAVE_UTF7
		{	"utf-7",		encodings_test_utf7			},
#endif
		{	"utf-16",		encodings_test_utf16		},
		{	"utf-8",		encodings_test_utf8			},
#ifdef ENCODINGS_HAVE_JAPANESE
		{	"iso-2022-jp",	encodings_test_iso2022jp	},
		{	"iso-2022-jp-1", encodings_test_iso2022jp1	},
		{	"shift-jis",	encodings_test_shiftjis		},
		{	"euc-jp",		encodings_test_eucjp		},
#endif
#ifdef ENCODINGS_HAVE_CHINESE
		{	"big5",			encodings_test_big5			},
		{	"big5:2003",	encodings_test_big5_2003	},
		{	"big5-hkscs",	encodings_test_big5hkscs	},
		{	"big5-hkscs:2004", encodings_test_big5hkscs_2004 },
		{	"big5-hkscs:2008", encodings_test_big5hkscs_2008 },
		{	"gbk",			encodings_test_gbk			},
		{	"gb-18030",		encodings_test_gb18030		},
		{	"HZ, aka hz-gb-2312", encodings_test_hz		},
		{	"euc-tw",		encodings_test_euctw		},
		{	"iso-2022-cn",	encodings_test_iso2022cn	},
#endif
#ifdef ENCODINGS_HAVE_KOREAN
		{	"euc-kr",		encodings_test_euckr		},
		{	"iso-2022-kr",	encodings_test_iso2022kr	},
#endif
		{	"iso-8859-7:2003", encodings_test_iso_8859_7_2003 },
		{	"iso-8859-2",	encodings_test_iso_8859_2		},
		{	"iso-8859-3",	encodings_test_iso_8859_3		},
		{	"iso-8859-4",	encodings_test_iso_8859_4		},
		{	"iso-8859-5",	encodings_test_iso_8859_5		},
		{	"iso-8859-6",	encodings_test_iso_8859_6		},
		{	"iso-8859-7",	encodings_test_iso_8859_7		},
		{	"iso-8859-8",	encodings_test_iso_8859_8		},
//		{	"iso-8859-9",	encodings_test_iso_8859_9		}, // windows-1254 is used instead
		{	"iso-8859-10",	encodings_test_iso_8859_10		},
		{	"iso-8859-11",	encodings_test_iso_8859_11		},
		{	"iso-8859-13",	encodings_test_iso_8859_13		},
		{	"iso-8859-14",	encodings_test_iso_8859_14		},
		{	"iso-8859-15",	encodings_test_iso_8859_15		},
		{	"iso-8859-16",	encodings_test_iso_8859_16		},
		{	"ibm866",		encodings_test_ibm866			},
		{	"windows-1250",	encodings_test_windows_1250		},
		{	"windows-1251",	encodings_test_windows_1251		},
		{	"windows-1252",	encodings_test_windows_1252		},
		{	"windows-1253",	encodings_test_windows_1253		},
		{	"windows-1254",	encodings_test_windows_1254		},
		{	"windows-1255",	encodings_test_windows_1255		},
		{	"windows-1256",	encodings_test_windows_1256		},
		{	"windows-1257",	encodings_test_windows_1257		},
		{	"windows-1258",	encodings_test_windows_1258		},
		{	"macintosh",	encodings_test_mac_roman		},
		{	"x-mac-ce",		encodings_test_mac_ce			},
		{	"x-mac-cyrillic", encodings_test_mac_cyrillic	},
		{	"x-mac-greek",	encodings_test_mac_greek		},
		{	"x-mac-turkish", encodings_test_mac_turkish		},
		{	"viscii",		encodings_test_viscii			},
		{	"koi8-r",		encodings_test_koi8r			},
		{	"koi8-u",		encodings_test_koi8u			},
	};

	for (int i = 0; i < ARRAY_SIZE(tests); ++ i)
	{
		OutputDebugStringA(tests[i].name);
		OutputDebugStringA("\n");
		if (tests[i].entry())
		{
			OutputDebugStringA(" ... passed\n");
		}
		else
		{
			OutputDebugStringA(" ... failed\n");
#ifdef MSWIN
			_asm { int 3 };
#else
			abort();
#endif
		}
	}
}

// ====================================================================================
// FACTORIES
// ====================================================================================

InputConverter *encodings_new_forward(const char *encoding)
{
	InputConverter *out = NULL;
	if (g_table_manager->Has(encoding))
	{
		out = OP_NEW(SingleBytetoUTF16Converter, (encoding));
	}
	else if (0 == strncmp(encoding, "iso-8859-1", 11))
	{
		out = OP_NEW(ISOLatin1toUTF16Converter, ());
	}
	else if (0 == strncmp(encoding, "us-ascii", 9))
	{
		return OP_NEW(ISOLatin1toUTF16Converter, ());
	}
	else if (0 == strncmp(encoding, "utf-8", 6))
	{
		out = OP_NEW(UTF8toUTF16Converter, ());
	}
#ifdef ENCODINGS_HAVE_JAPANESE
	else if (0 == strncmp(encoding, "iso-2022-jp", 12))
	{
		out = OP_NEW(ISO2022JPtoUTF16Converter, (ISO2022JPtoUTF16Converter::ISO_2022_JP));
	}
	else if (0 == strncmp(encoding, "iso-2022-jp-1", 14))
	{
		out = OP_NEW(ISO2022JPtoUTF16Converter, (ISO2022JPtoUTF16Converter::ISO_2022_JP_1));
	}
	else if (0 == strncmp(encoding, "shift_jis", 10))
	{
		out = OP_NEW(SJIStoUTF16Converter, ());
	}
	else if (0 == strncmp(encoding, "euc-jp", 7))
	{
		out = OP_NEW(EUCJPtoUTF16Converter, ());
	}
#endif
#ifdef ENCODINGS_HAVE_CHINESE
	else if (0 == strncmp(encoding, "iso-2022-cn", 12))
	{
		out = OP_NEW(ISO2022toUTF16Converter, (ISO2022toUTF16Converter::ISO2022CN));
	}
	else if (0 == strncmp(encoding, "big5", 5))
	{
		out = OP_NEW(Big5toUTF16Converter, ());
	}
	else if (0 == strncmp(encoding, "big5-hkscs", 11))
	{
		out = OP_NEW(Big5HKSCStoUTF16Converter, ());
	}
	else if (0 == strncmp(encoding, "hz-gb-2312", 11))
	{
		out = OP_NEW(HZtoUTF16Converter, ());
	}
	else if (0 == strncmp(encoding, "gbk", 4))
	{
		out = OP_NEW(GBKtoUTF16Converter, (GBKtoUTF16Converter::GBK));
	}
	else if (0 == strncmp(encoding, "gb18030", 7))
	{
		out = OP_NEW(GBKtoUTF16Converter, (GBKtoUTF16Converter::GB18030));
	}
	else if (0 == strncmp(encoding, "euc-tw", 7))
	{
		out = OP_NEW(EUCTWtoUTF16Converter, ());
	}
#endif
#ifdef ENCODINGS_HAVE_KOREAN
	else if (0 == strncmp(encoding, "iso-2022-kr", 12))
	{
		out = OP_NEW(ISO2022toUTF16Converter, (ISO2022toUTF16Converter::ISO2022KR));
	}
	else if (0 == strncmp(encoding, "euc-kr", 7))
	{
		out = OP_NEW(EUCKRtoUTF16Converter, ());
	}
#endif
	else if (0 == strncmp(encoding, "utf-16", 7))
	{
		out = OP_NEW(UTF16toUTF16Converter, ());
	}
#ifdef ENCODINGS_HAVE_UTF7
# ifdef ENCODINGS_HAVE_UTF7IMAP
	else if (0 == strncmp(encoding, "utf-7-imap", 11))
	{
		return OP_NEW(UTF7toUTF16Converter, (UTF7toUTF16Converter::IMAP));
	}
#endif
	else if (0 == strncmp(encoding, "utf-7-safe", 11))
	{
		return OP_NEW(UTF7toUTF16Converter, (UTF7toUTF16Converter::STANDARD));
	}
	else if (0 == strncmp(encoding, "utf-7", 6))
	{
		out = OP_NEW(UTF7toUTF16Converter, (UTF7toUTF16Converter::STANDARD));
	}
#endif

	if (out)
	{
		assert(OpStatus::IsSuccess(out->Construct()));
		assert(strcmp(out->GetCharacterSet(), encoding) == 0);
		assert(strcmp(out->GetDestinationCharacterSet(), "utf-16") == 0);
		return out;
	}

	TEST_FAILED(__FILE__, __LINE__, "Invalid encoding in encodings_new_forward");
	return NULL;
}

OutputConverter *encodings_new_reverse(const char *encoding)
{
	OutputConverter *out = NULL;
	if (0 == strncmp(encoding, "iso-8859-1", 11))
	{
		out = OP_NEW(UTF16toISOLatin1Converter, (FALSE));
	}
	else if (0 == strncmp(encoding, "us-ascii", 9))
	{
		out = OP_NEW(UTF16toISOLatin1Converter, (TRUE));
	}
	else if (0 == strncmp(encoding, "utf-16", 6) /* also matches 16le and 16be */)
	{
		out = OP_NEW(UTF16toUTF16OutConverter, ());
	}
	else if (0 == strncmp(encoding, "utf-8", 6))
	{
		out = OP_NEW(UTF16toUTF8Converter, ());
	}
#ifdef ENCODINGS_HAVE_JAPANESE
	else if (0 == strncmp(encoding, "iso-2022-jp", 12))
	{
		out = OP_NEW(UTF16toJISConverter, (UTF16toJISConverter::ISO_2022_JP));
	}
	else if (0 == strncmp(encoding, "iso-2022-jp-1", 14))
	{
		out = OP_NEW(UTF16toJISConverter, (UTF16toJISConverter::ISO_2022_JP_1));
	}
	else if (0 == strncmp(encoding, "euc-jp", 7))
	{
		out = OP_NEW(UTF16toJISConverter, (UTF16toJISConverter::EUC_JP));
	}
	else if (0 == strncmp(encoding, "shift_jis", 10))
	{
		out = OP_NEW(UTF16toJISConverter, (UTF16toJISConverter::SHIFT_JIS));
	}
#endif
#ifdef ENCODINGS_HAVE_CHINESE
	else if (0 == strncmp(encoding, "big5", 5))
	{
		out = OP_NEW(UTF16toDBCSConverter, (UTF16toDBCSConverter::BIG5));
	}
	else if (0 == strncmp(encoding, "big5-hkscs", 11))
	{
		out = OP_NEW(UTF16toBig5HKSCSConverter, ());
	}
	else if (0 == strncmp(encoding, "gbk", 6))
	{
		out = OP_NEW(UTF16toDBCSConverter, (UTF16toDBCSConverter::GBK));
	}
	else if (0 == strncmp(encoding, "gb18030", 7))
	{
		out = OP_NEW(UTF16toGB18030Converter, ());
	}
	else if (0 == strncmp(encoding, "euc-tw", 7))
	{
		out = OP_NEW(UTF16toEUCTWConverter, ());
	}
	else if (0 == strncmp(encoding, "hz-gb-2312", 11))
	{
		out = OP_NEW(UTF16toDBCS7bitConverter, (UTF16toDBCS7bitConverter::HZGB2312));
	}
	else if (0 == strncmp(encoding, "iso-2022-cn", 12))
	{
		out = OP_NEW(UTF16toISO2022CNConverter, ());
	}
#endif
#ifdef ENCODINGS_HAVE_KOREAN
	else if (0 == strncmp(encoding, "euc-kr", 7))
	{
		out = OP_NEW(UTF16toDBCSConverter, (UTF16toDBCSConverter::EUCKR));
	}
	else if (0 == strncmp(encoding, "iso-2022-kr", 12))
	{
		out = OP_NEW(UTF16toDBCS7bitConverter, (UTF16toDBCS7bitConverter::ISO2022KR));
	}
#endif
#ifdef ENCODINGS_HAVE_UTF7
	else if (0 == strncmp(encoding, "utf-7", 6))
	{
		out = OP_NEW(UTF16toUTF7Converter, (UTF16toUTF7Converter::STANDARD));
	}
	else if (0 == strncmp(encoding, "utf-7-safe", 11))
	{
		return OP_NEW(UTF16toUTF7Converter, (UTF16toUTF7Converter::SAFE));
	}
# ifdef ENCODINGS_HAVE_UTF7IMAP
	else if (0 == strncmp(encoding, "utf-7-imap", 11))
	{
		return OP_NEW(UTF16toUTF7Converter, (UTF16toUTF7Converter::IMAP));
	}
# endif
#endif
	else
	{
		// Check if we have a table for a single-byte character set
		char revtablename[32]; /* ARRAY OK 2011-11-09 peter */
		size_t namelen = strlen(encoding);
		if (namelen > sizeof revtablename - 5)
		{
			// This should never occur, since we don't have names that are
			// this long. But better safe than sorry.
			namelen = sizeof revtablename - 5;
		}
		memcpy(revtablename, encoding, namelen);
		strcpy(revtablename + namelen, "-rev");

		if (g_table_manager->Has(revtablename))
		{
			out = OP_NEW(UTF16toSBCSConverter, (encoding, revtablename));
		}
	}

	if (out)
	{
		assert(OpStatus::IsSuccess(out->Construct()));
		assert(strcmp(out->GetDestinationCharacterSet(), encoding) == 0);
		assert(strcmp(out->GetCharacterSet(), "utf-16") == 0);
		return out;
	}

	TEST_FAILED(__FILE__, __LINE__, "Invalid encoding in encodings_new_reverse");
	return NULL;
}


// ====================================================================================
// MISC DUMMY FUNCTIONS
// ====================================================================================

void *MemoryManager::GetTempBuf()
{
	return encodings_buffer;
}

int MemoryManager::GetTempBufLen()
{
	return ENC_TESTBUF_SIZE;
}

void OpStatus::Ignore(OpStatusEnum x)
{
	if (IsError(x))
	{
		char str[256]; /* ARRAY OK 2011-11-09 peter */
		op_snprintf(str, sizeof str, " ... ignored OpStatus %d\n", int(x));
		OutputDebugStringA(str);
	}
}

void output(const char *file, unsigned int line, const char *format, va_list args)
{
	char str[256], msg[256]; /* ARRAY OK 2011-11-09 peter */
	op_vsnprintf(msg, sizeof msg, format, args);
	if (file)
		op_snprintf(str, sizeof str, "%s(%u): %s\n", file, line, args);
	OutputDebugStringA(file ? str : msg);
}

void output1(const char *file, unsigned int line, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	output(file, line, format, args);
	va_end(args);
}

void output2(const char *file, unsigned int line, int, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	output(file, line, format, args);
	va_end(args);
}

void *op_memmem(const void *haystack, size_t hl, const void *needle, size_t nl)
{
	unsigned int no = 0;
	unsigned char *h = (unsigned char*) haystack;
	unsigned char *n = (unsigned char*) needle;

	while (hl--)
	{
		if (*h++ != n[no++])
			no = 0;
		else if (no == nl)
			return h - no;
	}
	return NULL;
}

size_t op_strlcpy(char *dest, const char *src, size_t dest_size)
{
	size_t srclen = 0;
	if (dest_size > 0)
	{
		dest_size --; // Leave space for nul
		while (*src && dest_size)
		{
			*(dest ++) = *(src ++);
			dest_size --;
			srclen ++;
		}
		// Always nul-terminate
		*dest = 0;
	}
	while (*(src ++))
	{
		srclen ++;
	}
	return srclen;
}

size_t uni_strlcpy(uni_char *dest, const uni_char *src, size_t dest_size)
{
	size_t srclen = 0;
	if (dest_size > 0)
	{
		dest_size --; // Leave space for nul
		while (*src && dest_size)
		{
			*(dest ++) = *(src ++);
			dest_size --;
			srclen ++;
		}
		// Always nul-terminate
		*dest = 0;
	}
	while (*(src ++))
	{
		srclen ++;
	}
	return srclen;
}
