/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#if !defined PCH_INCLUDED && defined ENCODINGS_REGTEST
#define PCH_INCLUDED

#include <cassert>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <climits>

// Configuration options
#define ENCODINGS_HAVE_SPECIFY_INVALID
#define ENCODINGS_HAVE_CHECK_ENDSTATE
#define ENCODINGS_HAVE_ENTITY_ENCODING

#define ENCODINGS_HAVE_CHINESE
#define ENCODINGS_HAVE_JAPANESE
#define ENCODINGS_HAVE_KOREAN
#define ENCODINGS_HAVE_UTF7
#define ENCODINGS_HAVE_UTF7IMAP
#define ENCODINGS_HAVE_CYRILLIC

// #define ENC_BIG_HKSCS_TABLE
// #define HAS_COMPLEX_GLOBALS

// Tweaks
#define ENCODINGS_LRU_SIZE 8

// These settings require different encoding.bin files
#define TABLEMANAGER_DYNAMIC_REV_TABLES
// #define ENCODINGS_HAVE_ROM_TABLES

// Settings for the test driver
#define HEXDUMP

// These must always be enabled for the test suite
#define ENCODINGS_HAVE_TABLE_DRIVEN
#define IANA_DATA_TABLES
#define USE_HKSCS_DATA
#define USE_MOZTW_DATA
#define USE_ICU_DATA

// Various defines to mimic the Opera code
#define OP_ASSERT assert
#define ANCHOR(x,y)
#define BOOL bool
#define FALSE false
#define TRUE true
#define ARRAY_SIZE(s) (sizeof s/sizeof s[1])
#define NOT_A_CHARACTER 0xFFFD
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#ifdef MSWIN
typedef __int64 OpFileLength;
#else
typedef long long int OpFileLength;
typedef long long int UINTPTR;
#endif
typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned int UINT32;
typedef UINT32 UnicodePoint;
#define OUTPUT_STR(s) OutputDebugStringA(s);
#define UNICODE_SIZE(l) ((l) * sizeof(wchar_t))
#define ENC_TABLE_FILE_NAME "encoding.bin"
#define NOT_A_CHARACTER 0xFFFD
#define DEPRECATED(X) __declspec(deprecated) X

// Opera stdlib compatibility
typedef wchar_t uni_char;
#define UNI_L(s) L ## s
#define uni_fopen(name, mode) _wfopen(name, mode)
#ifdef MSWIN
#define uni_strlen(s) wcslen(s)
#else
extern "C" size_t my_wcslen(const wchar_t *s);
#define wcslen(s) my_wcslen(s)
#define uni_strlen(s) my_wcslen(s)
#endif

#define op_bsearch(a,b,c,d,e) bsearch(a,b,c,d,e)
#define op_free(a) free(a)
#define op_isdigit(c) isdigit(c)
#define op_isspace(c) isspace(c)
#define op_malloc(a) malloc(a)
#define op_memcmp(a,b,c) memcmp(a,b,c)
#define op_memcpy(a,b,c) memcpy(a,b,c)
#define op_memmove(a,b,c) memmove(a,b,c)
#define op_memset(a,b,c) memset(a,b,c)
#define op_sprintf sprintf
#ifdef MSWIN
#define op_snprintf _snprintf
#define op_vsnprintf _vsnprintf
#else
#define op_snprintf snprintf
#define op_vsnprintf vsnprintf
#endif
#define op_strcat(a,b) strcat(a,b)
#define op_strcmp(a,b) strcmp(a,b)
#define op_strcpy(a,b) strcpy(a,b)
#ifdef MSWIN
#define op_stricmp(a,b) _stricmp(a,b)
#else
#define op_stricmp(a,b) strcasecmp(a,b)
#endif
#define op_strlen(s) strlen(s)
#define op_strncmp(a,b,c) strncmp(a,b,c)
#define op_strncpy(a,b,c) strncpy(a,b,c)
#define op_strstr(a,b) strstr(a,b)
#define op_tolower(c) tolower(c)
#define op_toupper(c) toupper(c)
#define op_qsort(a,b,c,d) qsort(a,b,c,d)

extern "C" void *op_memmem(const void *, size_t, const void *, size_t);
extern "C" size_t op_strlcpy(char *dest, const char *src, size_t dest_size);
extern "C" size_t uni_strlcpy(uni_char *dest, const uni_char *src, size_t dest_size);

// MSVC settings
#ifdef MSWIN
#define _CRT_SECURE_NO_WARNINGS
#endif

// Hackery to compile on non-MSWIN
#ifndef MSWIN
void OutputDebugStringA(const char *s);
#endif

// Fake memory and error handling
#define new(cookie) new
#define OP_NEW(type, args) new type args
#define OP_NEW_L(type, args) new type args
#define OP_NEWA(type, num) new type[num]
#define OP_NEWA_L(type, num) new type[num]
#define OP_DELETE(var) delete var
#define OP_DELETEA(var) delete[] var
#define OP_STATUS OpStatus::OpStatusEnum
#define LEAVE(condition) assert(condition == OpStatus::OK)
#define LEAVE_IF_ERROR(condition) assert(condition == OpStatus::OK)
#define RETURN_IF_ERROR(condition) assert(condition == OpStatus::OK)
#define TRAP(var, statement) { statement; var = OpStatus::OK; }

namespace OpStatus
{
	enum OpStatusEnum
	{
		OK = 0,
		ERR,
		ERR_FILE_NOT_FOUND,
		ERR_PARSING_FAILED,
		ERR_NO_MEMORY
	};

	inline bool IsMemoryError(OpStatusEnum x)
	{
		return x == ERR_NO_MEMORY;
	};
	inline bool IsError(OpStatusEnum x)
	{
		return x != OK;
	};
	inline bool IsSuccess(OpStatusEnum x)
	{
		return !IsError(x);
	};
	void Ignore(OpStatusEnum x);
};

namespace Unicode
{
	static inline BOOL IsSurrogate(UINT32 x)
	{
		return (x & 0xFFFFF800) == 0xD800;
	}

	static inline BOOL IsHighSurrogate(UINT32 x)
	{
		return x >= 0xD800 && x <= 0xDBFF;
	}

	static inline BOOL IsLowSurrogate(UINT32 x)
	{
		return x >= 0xDC00 && x <= 0xDFFF;
	}

	static inline UINT32 DecodeSurrogate(uni_char high, uni_char low)
	{
		return 0x10000 + (((high & 0x03FF) << 10) | (low & 0x03FF));
	}

	static inline void MakeSurrogate(UINT32 ucs, uni_char &high, uni_char &low)
	{
		// Surrogates spread out the bits of the UCS value shifted down by 0x10000
		ucs -= 0x10000;
		high = 0xD800 | uni_char(ucs >> 10);	// 0xD800 -- 0xDBFF
		low  = 0xDC00 | uni_char(ucs & 0x03FF);	// 0xDC00 -- 0xDFFF
	}
}

// Simple OpFileFolder for the test suite
enum OpFileFolder
{
	OPFILE_RESOURCES_FOLDER
};

// Simple OpFile for the test suite
class OpFile
{
public:
	OP_STATUS Construct(const char *filename, OpFileFolder);
	bool IsOpen();
	OP_STATUS ReadBinByte(unsigned char &dest);
	OP_STATUS Read(void *p, OpFileLength len, OpFileLength *bytes_read);
	OP_STATUS GetFilePos(OpFileLength &pos);
	void SetFilePos(OpFileLength pos);
	void Close() {}

	~OpFile();

private:
	FILE *f;
	char *name;
};

// Simple MemoryManager for the test suite
class MemoryManager
{
public:
	void *GetTempBuf();
	int GetTempBufLen();
	void RaiseCondition(OP_STATUS condition)
	{ assert(condition == OpStatus::OK); }
};

// Dummy PrefsCollectionDisplay
class PrefsCollectionDisplay
{
public:
	const char *GetForceEncoding() { return NULL; };
};
extern PrefsCollectionDisplay *g_pcdisplay;

// Simple EncodingsModule for the test suite
class EncodingsModule
{
public:
	EncodingsModule() : m_hkscs_table(NULL) {}
	~EncodingsModule() { OP_DELETEA(m_hkscs_table); }

	UINT32 *m_hkscs_table;
};

// Simple g_opera for the test suite
class Opera
{
public:
	class EncodingsModule encodings_module;
};

// Simple testfailed for the test suite
void output1(const char *file, unsigned int line, const char *format, ...);
void output2(const char *file, unsigned int line, int, const char *format, ...);
#define TEST_FAILED output1
#define OUTPUT output2

// Various globals
extern class Opera *g_opera;
#include "tablemanager/optablemanager.h"
#undef g_table_manager
extern class OpTableManager *g_table_manager;
extern class CharsetManager *g_charsetManager;
extern class MemoryManager *g_memory_manager;

// Copied from url2/encoder.cpp:
// Reverse table versions Base64_Encoding_char.
// 0-63 normal encoded values
// 64	legal termination character
// 65	ignore
// 66	illegal,ignore
const unsigned char Base64_decoding_table[128] = {
	66, 66, 66, 66,  66, 66, 66, 66,  66, 66, 65, 66,  66, 65, 66, 66,
	66, 66, 66, 66,  66, 66, 66, 66,  66, 66, 66, 66,  66, 66, 66, 66, 
	66, 66, 66, 66,  66, 66, 66, 66,  66, 66, 66, 62,  66, 66, 66, 63, 
	52, 53, 54, 55,  56, 57, 58, 59,  60, 61, 66, 66,  66, 64, 66, 66, 
	66,  0,  1,  2,   3,  4,  5,  6,   7,  8,  9, 10,  11, 12, 13, 14, 
	15, 16, 17, 18,  19, 20, 21, 22,  23, 24, 25, 66,  66, 66, 66, 66,
	66, 26, 27, 28,  29, 30, 31, 32,  33, 34, 35, 36,  37, 38, 39, 40,
	41, 42, 43, 44,  45, 46, 47, 48,  49, 50, 51, 66,  66, 66, 66, 66
};

// Copied from url2/encoder.cpp:
const char * const Base64_Encoding_chars =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

// Copied from core/unicode.h:
int strni_eq(const char * str1, const char *str2, size_t len);

// Input converters ------------------------------------------------------
#include "decoders/inputconverter.h"
#include "decoders/identityconverter.h"
#include "decoders/iso-8859-1-decoder.h"
#include "decoders/utf7-decoder.h"
#include "decoders/utf8-decoder.h"
#include "decoders/utf16-decoder.h"
#include "decoders/big5-decoder.h"
#include "decoders/big5hkscs-decoder.h"
#include "decoders/euc-jp-decoder.h"
#include "decoders/euc-kr-decoder.h"
#include "decoders/euc-tw-decoder.h"
#include "decoders/gbk-decoder.h"
#include "decoders/hz-decoder.h"
#include "decoders/iso-2022-cn-decoder.h"
#include "decoders/iso-2022-jp-decoder.h"
#include "decoders/sbcs-decoder.h"
#include "decoders/sjis-decoder.h"

// Output converters -----------------------------------------------------
#include "encoders/outputconverter.h"
#include "encoders/utf7-encoder.h"
#include "encoders/utf8-encoder.h"
#include "encoders/utf16-encoder.h"
#include "encoders/iso-8859-1-encoder.h"
#include "encoders/big5hkscs-encoder.h"
#include "encoders/dbcs-encoder.h"
#include "encoders/euc-tw-encoder.h"
#include "encoders/gb18030-encoder.h"
#include "encoders/hz-encoder.h"
#include "encoders/iso-2022-cn-encoder.h"
#include "encoders/jis-encoder.h"
#include "encoders/sbcs-encoder.h"

#endif // !PCH_INCLUDED && ENCODINGS_REGTEST
