/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/desktop_util/string/stringutils.h"

#include "adjunct/desktop_pi/desktop_menu_context.h"
#include "adjunct/desktop_util/datastructures/StreamBuffer.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/managers/DesktopClipboardManager.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

#include "modules/dochand/winman.h"
#include "modules/formats/uri_escape.h"
#include "modules/libcrypto/include/CryptoHash.h"
#include "modules/libopeay/openssl/md5.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/util/gen_math.h"
#include "modules/util/handy.h"
#include "modules/util/opguid.h"
#include "modules/pi/OpLocale.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/search_engine/WordHighlighter.h"
#include "modules/xmlutils/xmlutils.h"
#include "modules/formats/base64_decode.h"

/***********************************************************************************
**
**  StrFormatByteSize
**
***********************************************************************************/

uni_char * StrFormatByteSize(OpString &buffer, OpFileLength nBytes, unsigned int flags)
{
	return StrFormatByteSize(buffer.CStr(), buffer.Capacity(), nBytes, flags);
}

/***********************************************************************************
**
**  StrFormatByteSize
**
**  Returns a string that might look like "26.1 KB" or "26.1 KB (26 899 bytes)"
**  depending on the 'fDetails" argument
**
**  NOTE		There is no check on the size of szBuf
**   			Make sure it's large enough.
***********************************************************************************/

uni_char * StrFormatByteSize(uni_char *szBuf, size_t max, OpFileLength nBytes, unsigned int flags)
{
	if (max<=0)
		return szBuf;

	//	Validate params
	OP_ASSERT( szBuf);
	BOOL fDetails = flags & SFBS_DETAILS;
	BOOL fForceKB = flags & SFBS_FORCEKB;
	BOOL fNoComma = flags & SFBS_NOCOMMA;

	static BOOL fFirstCall = TRUE;
	static uni_char szByteAbr[4];
	static uni_char szbytes[32];
	static uni_char szbyte[32];
	static uni_char szKB[32];
	static uni_char szMB[32];
	static uni_char szGB[32];

	if (fFirstCall)
	{
		fFirstCall = FALSE;
		OpString tmp;

		OP_STATUS err = g_languageManager->GetString(Str::SI_IDSTR_BYTE, tmp);
		if (OpStatus::IsSuccess(err) && tmp.CStr())
			uni_strlcpy(szByteAbr, tmp.CStr(), ARRAY_SIZE(szByteAbr));
		else
			szByteAbr[0] = '\0';

		err = g_languageManager->GetString(Str::SI_IDSTR_BYTES, tmp);
		if (OpStatus::IsSuccess(err) && tmp.CStr())
			uni_strlcpy(szbytes, tmp.CStr(), ARRAY_SIZE(szbytes));
		else
			szbytes[0] = '\0';

		err = g_languageManager->GetString(Str::SI_IDSTR_1BYTE, tmp);
		if (OpStatus::IsSuccess(err) && tmp.CStr())
			uni_strlcpy(szbyte, tmp.CStr(), ARRAY_SIZE(szbyte));
		else
			szbyte[0] = '\0';

		err = g_languageManager->GetString(Str::SI_IDSTR_KILOBYTE, tmp);
		if (OpStatus::IsSuccess(err) && tmp.CStr())
			uni_strlcpy(szKB, tmp.CStr(), ARRAY_SIZE(szKB));
		else
			szKB[0] = '\0';

		err = g_languageManager->GetString(Str::SI_IDSTR_MEGABYTE, tmp);
		if (OpStatus::IsSuccess(err) && tmp.CStr())
			uni_strlcpy(szMB, tmp.CStr(), ARRAY_SIZE(szMB));
		else
			szMB[0] = '\0';

		err = g_languageManager->GetString(Str::SI_IDSTR_GIGABYTE, tmp);
		if (OpStatus::IsSuccess(err) && tmp.CStr())
			uni_strlcpy(szGB, tmp.CStr(), ARRAY_SIZE(szGB));
		else
			szGB[0] = '\0';
	}

	BOOL fAppendDetails = fDetails;
	const unsigned long KB = 1024;
	const unsigned long MB = 1024*1024;
	const unsigned long GB = 1024*1024*1024;

	double value = (double)(INT64)nBytes;
	int nFracDigits = 0;
	const uni_char *pszUnit = NULL;	//	ptr to unit string (bytes, byte, KB, MB etc)

	if (fForceKB)
	{
		pszUnit = szKB;
		nFracDigits = 0;
		if (value != 0)
		{
			if (value>=KB)
				value /= KB;
			else
				value = 1;
		}
	}
	else
	{
		if (nBytes<1024)
		{
			fAppendDetails = FALSE;
			if( nBytes == 1 )
				pszUnit = szbyte;	// 1 byte
			else
				pszUnit = szbytes;	// 0 bytes ... 2 bytes ... 1023 bytes
		}
		else
		{
			//
			//	Dont like stuff below. Length of fraction part should be fixed, not "smart" as below
			//	DG
			//
			if( nBytes < KB*10 )
			{
				// 1.0KB ... 1.1 KB ... 9.9 KB
				pszUnit = szKB;
				value /= KB;
				nFracDigits = 1;
			}
			else if( nBytes < MB)
			{
				// 10KB ... 11 KB ... 1023 KB
				pszUnit = szKB;
				value /= KB;
				nFracDigits = 0;
			}
			else if( nBytes >= GB)
			{
				pszUnit = szGB;
				value /= GB;
				nFracDigits = 1;
			}
			else
			{
				// 1.0MB ... 1.1MB ... 999.9MB
				pszUnit = szMB;
				value /= MB;
				nFracDigits = 1;

			}
		}
	}

	//
	//	Format string
	//
	value = Round( value, nFracDigits, ROUND_UP);
	if (flags & SFBS_ABBRIVATEBYTES && (pszUnit== szbytes || pszUnit==szbyte))
		pszUnit = szByteAbr;

	const BOOL fComma = !fNoComma && nFracDigits;

	if (fComma)
	{
		uni_char formatS[24];
		if (g_oplocale->NumberFormat(formatS, 23, (float)value, 1, FALSE) == 0)
			formatS[0] = '\0';
		uni_snprintf(szBuf, max, UNI_L("%s %s"), formatS, pszUnit);
	}
	else
	{
		uni_char formatS[24];
		if (g_oplocale->NumberFormat(formatS, 23, (int)value, FALSE) == 0)
			formatS[0] = '\0'; // formatS is not touched by the call to NumberFormat
		uni_snprintf(szBuf, max, UNI_L("%s %s"), formatS, pszUnit);
	}
	//
	//	if requested, append exact size in bytes,
	//	if not already written in bytes
	//
	if( fAppendDetails)
	{
		uni_char szTmp[32];
		uni_char szNum[32];

		if (g_oplocale->NumberFormat(szNum, 31, nBytes, TRUE) == 0)
			szNum[0] = '\0';
		uni_sprintf(szTmp, UNI_L(" (%s %s)"), szNum, szbytes);
		if (uni_strlen(szBuf) + uni_strlen(szTmp) <= max)
			uni_strcat( szBuf, szTmp );
	}
	return szBuf;
}

/***********************************************************************************
**
**  EscapeAmpersand
**
**  Escape '&' characters into '&&' before using in menus,
**  so they won't be interpreted as "underline/access key"
**  return FALSE if dest buffer isn't big enough
**
***********************************************************************************/

unsigned int EscapeAmpersand( uni_char *dst, unsigned int len, const uni_char *src )
{
	if (!src)
		return TRUE;

	uni_char *end = dst + len - 1;

	while( 1 ) {
		if( dst+1 >= end ) {
			*dst = '\0';
			return(FALSE);
		}
		if( '&' == *src ) {
			*dst = '&';
			dst++;
		}
		*dst = *src;
		if( '\0' == *src )
			break;
		src++; dst++;
	}
	return(TRUE);
}

/***********************************************************************************
**
**  AppendToList
**
***********************************************************************************/

OP_STATUS StringUtils::AppendToList( OpVector<OpString>& list, const uni_char *str, int len )
{
	OpString* s = OP_NEW(OpString, ());
	if (!s)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(s->Set(str, len));
	return list.Add(s);
}

/***********************************************************************************
**
**  EscapeURIAttributeValue
**
***********************************************************************************/

OP_STATUS StringUtils::EscapeURIAttributeValue(const OpStringC8& value, OpString8& escaped_value)
{
	escaped_value.Empty();

	if (value.IsEmpty())
		return OpStatus::OK;

	const char* src = value.CStr();
	char*       dst = escaped_value.Reserve(value.Length() * 3);

	if (!dst)
		return OpStatus::ERR_NO_MEMORY;

	while (*src)
	{
		switch (*src)
		{
			case ';':
			case '/':
			case '?':
			case ':':
			case '@':
			case '&':
			case '=':
			case '+':
			case ',':
			case '$':
			case '[':
			case ']':
				// These need to be escaped in URI attribute values, since
				// they are reserved within a query.
				// See RFC 2396, section 3.4.
				*(dst++) = '%';
				*(dst++) = UriEscape::EscapeFirst(*src);
				*(dst++) = UriEscape::EscapeLast(*src);
				break;
			default:
				// For other charactes use default escaping
				if (UriEscape::NeedEscape(*src, UriEscape::Hash | UriEscape::Percent))
				{
					*(dst++) = '%';
					*(dst++) = UriEscape::EscapeFirst(*src);
					*(dst++) = UriEscape::EscapeLast(*src);
				}
				else
				{
					*(dst++) = *src;
				}
		}
		src++;
	}
	*dst = 0;

	return OpStatus::OK;
}

/***********************************************************************************
**
**  EscapeURIAttributeValue
**
***********************************************************************************/

OP_STATUS StringUtils::EscapeURIAttributeValue16(const OpStringC& value, OpString& escaped_value)
{
	escaped_value.Empty();

	if (value.IsEmpty())
		return OpStatus::OK;

	const uni_char* src = value.CStr();
	uni_char*       dst = escaped_value.Reserve(value.Length() * 3);

	if (!dst)
		return OpStatus::ERR_NO_MEMORY;

	while (*src)
	{
		switch (*src)
		{
			case ';':
			case '/':
			case '?':
			case ':':
			case '@':
			case '&':
			case '=':
			case '+':
			case ',':
			case '$':
			case '[':
			case ']':
				// These need to be escaped in URI attribute values, since
				// they are reserved within a query.
				// See RFC 2396, section 3.4.
				*(dst++) = '%';
				*(dst++) = UriEscape::EscapeFirst(*src);
				*(dst++) = UriEscape::EscapeLast(*src);
				break;
			default:
				// For other charactes use default escaping
				if (UriEscape::NeedEscape(*src, UriEscape::Hash | UriEscape::Percent))
				{
					*(dst++) = '%';
					*(dst++) = UriEscape::EscapeFirst(*src);
					*(dst++) = UriEscape::EscapeLast(*src);
				}
				else
				{
					*(dst++) = *src;
				}
		}
		src++;
	}
	*dst = 0;

	return OpStatus::OK;
	}

/***********************************************************************************
**
**  SplitString
**
***********************************************************************************/

OP_STATUS StringUtils::SplitString( OpVector<OpString>& list, const OpStringC &candidate, int sep, BOOL keep_strings )
{
	if (candidate.IsEmpty())
		return OpStatus::OK;

	const uni_char* scanner = candidate.CStr();
	const uni_char* start = NULL;
	BOOL inside_string = FALSE;
	BOOL last_char_was_separator = TRUE;

	while (uni_char curr_char = *scanner)
	{
		if( keep_strings && (inside_string || last_char_was_separator) && curr_char == '\"' )
		{
			if( inside_string )
			{
				if( start )
				{
					int length  = scanner - start;
					RETURN_IF_ERROR(AppendToList(list, start, length));
					start = NULL;
				}
			}
			else
			{
				start = scanner + 1;
			}
			inside_string = !inside_string;
		}
		else if(!inside_string && curr_char == sep )
		{
			if( start )
			{
				int length = scanner - start;
				RETURN_IF_ERROR(AppendToList(list, start, length));
				start = NULL;
			}
		}
		else if(!inside_string)
		{
			if( !start )
			{
				start = scanner;
			}
		}

		scanner++;
		last_char_was_separator = (curr_char == sep);
	}

	if (start && start < scanner)
	{
		int length = scanner - start;
		RETURN_IF_ERROR(AppendToList(list, start, length));
	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**  ContainsAll
**
***********************************************************************************/

BOOL StringUtils::ContainsAll(const OpStringC& needles, const OpStringC& haystack)
{
	if (haystack.IsEmpty())
		return needles.IsEmpty();

	OpString word;
	const uni_char* next = needles.CStr();
	while ((next = StringUtils::SplitWord(next, ' ', word)))
	{
		if (!uni_stristr(haystack.CStr(), word.CStr()))
			return FALSE;
	}

	return TRUE;
}

/***********************************************************************************
**
**  ExtractExcerpt
**
***********************************************************************************/

OP_STATUS ExtractExcerpt(OpString & excerpt, OpVector<OpString>& needles, OpString & haystack, int length)
{
	if(length < 20)
		return OpStatus::ERR;

	int start_length = MIN(length / 2, 20);

	for( UINT i = 0; i < needles.GetCount(); i++ )
	{
		OpString * word = needles.Get(i);

		if(word)
		{
			int index = haystack.FindI(word->CStr());

			if (index == KNotFound)
				continue;

			uni_char* index_word  = haystack.CStr() + index;
			uni_char* search_from = haystack.CStr() + (index - start_length > 0 ? index - start_length : 0);

			// Find the first space :
			while (search_from < index_word && !uni_isspace(*search_from))
				search_from++;

			// If we found one, find the first non-space after that :
			if (search_from != index_word)
			{
				while (search_from < index_word && uni_isspace(*search_from))
					search_from++;
			}

			// Get your characters igoring whitespaces:

			excerpt.Empty();
			uni_char* excerpt_ptr    = excerpt.Reserve(length);

			if (!excerpt_ptr)
				return OpStatus::ERR_NO_MEMORY;

			while (*search_from && excerpt_ptr - excerpt.CStr() < length)
			{
				BOOL space = FALSE;

				// Ignore whitespace
				while (uni_isspace(*search_from))
				{
					search_from++;
					space = TRUE;
				}

				if(space)
				{
					// If we found whitespace, insert a regular space
					*excerpt_ptr++ = 0x20;
				}
				else
				{
					// Copy character
					*excerpt_ptr++ = *search_from++;
				}
			}

			// NULL-termination
			*excerpt_ptr = 0;

			return OpStatus::OK;
		}
	}

	return OpStatus::ERR;
}

/***********************************************************************************
**
**  ExtractFormattedExcerpt
**
***********************************************************************************/

OP_STATUS ExtractFormattedExcerpt(OpString & excerpt,
								  OpVector<OpString> & words,
								  OpString & haystack,
								  int length)
{
	RETURN_IF_ERROR(ExtractExcerpt(excerpt, words, haystack, length));

	// Make a list of word lengths so we don't have to calculate this later

	unsigned needlecount = words.GetCount();
	int*     lengths     = OP_NEWA(int, needlecount);

	if (!lengths)
		return OpStatus::ERR_NO_MEMORY;

	for (unsigned i = 0; i < needlecount; i++)
	{
		OpString* word = words.Get(i);
		lengths[i] = word ? word->Length() : 0;
	}

	// Scan the excerpt for words and make them bold

	uni_char* scanner = excerpt.CStr();

	while (scanner)
	{
		for (unsigned j = 0; j < needlecount; j++)
		{
			OpString* word = words.Get(j);

			if (word && word->HasContent() && !uni_strnicmp(word->CStr(), scanner, lengths[j]))
			{
				// Found a match, make bold
				int pos = scanner - excerpt.CStr();

				OP_STATUS stat;
				stat = excerpt.Insert(pos, UNI_L("<b>"));
				if (OpStatus::IsError(stat))
				{
					OP_DELETEA(lengths);
					return stat;
				}
				stat = excerpt.Insert(pos + 3 + lengths[j], UNI_L("</b>"));
				if (OpStatus::IsError(stat))
				{
					OP_DELETEA(lengths);
					return stat;
				}
				scanner = excerpt.CStr() + pos + lengths[j] + 7; // Put scanner behind word just found
				break;
			}
		}

		// Find next space
		scanner = uni_strchr(scanner, ' ');
		// Go past space to find the start of a word
		if (scanner)
			scanner++;
	}

	OP_DELETEA(lengths);
	return OpStatus::OK;
}

/***********************************************************************************
**
**  UnQuote
**
***********************************************************************************/

OP_STATUS UnQuote(OpString &input)
{
    OpAutoVector<OpString> substrings;
    RETURN_IF_ERROR(StringUtils::SplitString( substrings, input, ' ', TRUE ));

    input.Wipe();

    for(UINT32 i = 0; i < substrings.GetCount(); i++)
    {
		OpString * str = substrings.Get(i);
		RETURN_IF_ERROR(input.Append(str->CStr()));
		RETURN_IF_ERROR(input.Append(" "));
    }

    input.Strip();
	return OpStatus::OK;
}

/***********************************************************************************
**
**  StartsWith
**
***********************************************************************************/

BOOL StartsWith(const uni_char * text, const uni_char * possible_start, BOOL case_insensitive)
{
	return StringUtils::StartsWith(text, possible_start, case_insensitive);
}


/***********************************************************************************
**
**  StrRevStr
**
***********************************************************************************/

uni_char* StrRevStr(const uni_char* haystack, const uni_char* needle)
{
	if (!haystack || !needle || !*needle || !*haystack)
		return NULL;

	uni_char* haystack_end = (uni_char*)haystack;
	while (haystack_end[1]) haystack_end++;
	uni_char* needle_end = (uni_char*)needle;
	while (needle_end[1]) needle_end++;

	uni_char* needle_runner = needle_end;
	uni_char* haystack_runner = haystack_end;

	do
	{
		if (*needle_runner == *haystack_runner)
		{
			if (needle_runner == needle)
				return haystack_runner;

			needle_runner--;
			haystack_runner--;
		}
		else
		{
			needle_runner = needle_end;
			haystack_runner = --haystack_end;
		}
	}
	while (haystack_runner >= haystack);

	return NULL;
}

/***********************************************************************************
**
**  FormatTime
**
***********************************************************************************/

OP_STATUS FormatTime(OpString& result, const uni_char* format, time_t time)
{
	if (!format)
	{
		result.Empty();
		return OpStatus::OK;
	}

	OpString buf;
	if (buf.Reserve(128) == NULL)
		return OpStatus::ERR_NO_MEMORY;

	// Make the struct :
	struct tm *date = op_localtime(&time);
	if (date)
	{
		unsigned int len = g_oplocale->op_strftime(buf.CStr(), buf.Capacity(), format, date);
		buf.CStr()[len] = 0;
	}

	return result.Set(buf);
}

/***********************************************************************************
**
**  SecondsSinceMidnight
**
***********************************************************************************/

time_t SecondsSinceMidnight(time_t now)
{
	struct tm* time_struct = op_localtime(&now);
	if (!time_struct)
		return 0;

	time_t sec = time_struct->tm_sec + (time_struct->tm_min*60) + (time_struct->tm_hour*3600);
	return sec;
}

/***********************************************************************************
**
**  FormatTimeStandard
**
***********************************************************************************/

OP_STATUS FormatTimeStandard(OpString& result, time_t time, int time_format)
{
	if(!time)
		return OpStatus::ERR;

	if (time_format == TIME_FORMAT_DEFAULT)
	{
		// Get current time :
		time_t now = g_timecache->CurrentTime();
		time_t sec_since_midnight = SecondsSinceMidnight(now);
		time_t sec_last_two_weeks      = sec_since_midnight + (14*24*60*60);

		if (time >= (now - sec_since_midnight))
		{
			time_format = TIME_FORMAT_ONLY_TIME;
		}
		else if (time >= (now - sec_last_two_weeks))
		{
			time_format = TIME_FORMAT_WEEKDAY_AND_DATE;
		}
		else
		{
			// older or in the future
			time_format = TIME_FORMAT_UNIX_FORMAT;
		}
	}

	OpString format;
	switch (time_format)
	{
	case TIME_FORMAT_ONLY_TIME:
	{
		if (g_oplocale->Use24HourClock())
			format.Set("%H:%M");
		else
			format.Set("%I:%M %p");
	}
	break;
	case TIME_FORMAT_WEEKDAY_AND_TIME:
	{
		if (g_oplocale->Use24HourClock())
			format.Set("%a %H:%M");
		else
			format.Set("%a %I:%M %p");
	}
	break;
	case TIME_FORMAT_WEEKDAY_AND_DATE:
	{
		format.Set("%a %d %b");
	}
	break;
	default:
		format.Set(UNI_L("%x"));
	}

	return FormatTime(result, format, time);
}

/***********************************************************************************
**
**  FormatByteSize
**
***********************************************************************************/

OP_STATUS FormatByteSize(OpString& result, OpFileLength bytes)
{
	OpString buf;
	if (!buf.Reserve(128))
		return OpStatus::ERR_NO_MEMORY;

	buf.CStr()[0] = 0;

	StrFormatByteSize(buf, bytes, /*SFBS_FORCEKB SFBS_DEFAULT*/ SFBS_ABBRIVATEBYTES);

	return result.Set(buf);
}

/***********************************************************************************
**
**  HexStrToByte
**
***********************************************************************************/

OP_STATUS HexStrToByte(const OpStringC8& hex_string, BYTE*& byte_buffer, unsigned int& buffer_len)
{
	unsigned int buf_len = hex_string.Length();
	buffer_len = (buf_len+1)/2; // will cope with uneven lengths
	byte_buffer = OP_NEWA(BYTE, buffer_len);
	if (!byte_buffer)
	{
		buffer_len = 0;
		return OpStatus::ERR_NO_MEMORY;
	}

	const char *hex_str = hex_string.CStr();
	for (unsigned int i = 0; i < buf_len; i++)
	{
		char c1 = (hex_str[i] | 0x20) - 0x30;
		if (c1 > 9) c1 -= 0x27;
		if (c1 < 0 || c1 > 15)
		{
			OP_DELETEA(byte_buffer);
			byte_buffer = NULL;
			buffer_len = 0;
			return OpStatus::ERR_OUT_OF_RANGE;
		}
		BYTE *dest = &byte_buffer[i/2];

		if (i & 1)
			*dest |= c1;
		else
			*dest= c1<<4;
	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**  ByteToHexStr
**
***********************************************************************************/

OP_STATUS ByteToHexStr(const BYTE* byte_buffer, unsigned int buffer_len, OpString8& hex_string)
{
	buffer_len *= 2;
	if (!hex_string.Reserve(buffer_len))
		return OpStatus::ERR_NO_MEMORY;

	char *hex_str = hex_string.CStr();
	for(unsigned int i = 0; i < buffer_len; i++)
	{
		unsigned char c1 = byte_buffer[i/2];

		if (i & 1)
			c1 &= 0xf;
		else
			c1 >>= 4;

		c1 += 0x30;
		if (c1>0x39) c1 += 0x27;
		hex_str[i] = c1;
	}
	hex_str[buffer_len] = 0;

	return OpStatus::OK;
}

unsigned int CalculateHash(const char* text, unsigned int hash_size)
{
	unsigned int full_index;
	return LinkObjectStore::GetHashIdx(text, hash_size, full_index);
}

unsigned int CalculateHash(const OpStringC8& text, unsigned int hash_size)
{
	return CalculateHash(text.CStr(), hash_size);
}


OP_STATUS StringUtils::GetFormattedLanguageString(OpString& output, Str::LocaleString string_id, ...)
{
	OpString format_string;
	va_list	 arg_list;

	output.Empty();

	// Get the format string
	RETURN_IF_ERROR(g_languageManager->GetString(string_id, format_string));
	if (format_string.IsEmpty())
		return OpStatus::OK;

	// Start variable argument list
	va_start(arg_list, string_id);

	// do the append
	OP_STATUS ret = output.AppendVFormat(format_string.CStr(), arg_list);

	// Cleanup: end variable argument list
	va_end(arg_list);

	return ret;
}

OP_STATUS DecodeBase64(OpStringC8 data, unsigned char*& output, unsigned long& len)
{
	unsigned long pos = 0;
	BOOL warn = FALSE;
	const unsigned char* src = (const unsigned char*)data.CStr();
	unsigned long calc_len = ((data.Length() + 3) / 4) * 3;

	output = OP_NEWA(unsigned char, calc_len);
	if(output == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	len = GeneralDecodeBase64(src, data.Length(), pos, output, warn);
	OP_ASSERT(len <= calc_len);

	return OpStatus::OK;
}

int StringUtils::Find(const OpStringC& find_in, const OpStringC& find_what,
				  UINT find_from)
{
	OP_ASSERT(find_from <= UINT(find_in.Length()));
	const OpStringC& input = find_in.CStr() + find_from;

	// Search for the first occurance.
	int find_pos = input.Find(find_what);

	// Adjust find_pos with respect to what we just cut away.
	if (find_pos != KNotFound)
		find_pos += find_from;

	return find_pos;
}


int StringUtils::FindLastOf(const OpStringC& find_in, const OpStringC& find_what)
{
	int last_pos = -1;

	const int find_what_length = find_what.Length();
	for (int index = 0; index < find_what_length; ++index)
	{
		const uni_char cur_char = find_what[index];
		const int find_pos = find_in.FindLastOf(cur_char);

		if (find_pos > last_pos)
			last_pos = find_pos;
	}

	return last_pos;
}

namespace StringUtils{
	static OP_STATUS ReplaceInternal(OpString &replace_in,
		const OpStringC& replace_what, const OpStringC& replace_with, bool replace_crlf);
}

/* Internal function that has a special mode for replacing both \r\n and \n if replace_crlf is set to true
 */
static OP_STATUS StringUtils::ReplaceInternal(OpString &replace_in,
						   const OpStringC& replace_what, const OpStringC& replace_with, bool replace_crlf)
{
	if (replace_in.IsEmpty() || replace_what.IsEmpty())
		return OpStatus::OK;

	const uni_char* curpos = replace_in.CStr();
	const uni_char* foundpos = uni_strstr(curpos, replace_what.CStr());

	if (!foundpos)
		return OpStatus::OK;

	const int replace_what_length = replace_what.Length();
	const int replace_with_length = replace_with.Length();
	const int replace_in_length = replace_in.Length();
	const int reserve_space = replace_with_length > replace_what_length ? replace_in_length * 2 : replace_in_length;

	StreamBuffer<uni_char> buffer;
	RETURN_IF_ERROR(buffer.Reserve(reserve_space));

	do
	{
		if (foundpos > curpos)
		{
			if (replace_crlf && *(foundpos-1) == '\r')
				RETURN_IF_ERROR(buffer.Append(curpos, foundpos - 1 - curpos));
			else
				RETURN_IF_ERROR(buffer.Append(curpos, foundpos - curpos));
		}

		if (replace_with.HasContent())
			RETURN_IF_ERROR(buffer.Append(replace_with.CStr(), replace_with_length));

		curpos = foundpos + replace_what_length;
		foundpos = uni_strstr(curpos, replace_what.CStr());
	} while (foundpos);

	RETURN_IF_ERROR(buffer.Append(curpos, replace_in_length - (curpos - replace_in.CStr())));

	StreamBufferToOpString(buffer, replace_in);

	return OpStatus::OK;
}

OP_STATUS StringUtils::Replace(OpString &replace_in,
						   const OpStringC& replace_what, const OpStringC& replace_with)
{ 
	return StringUtils::ReplaceInternal(replace_in, replace_what, replace_with, false);  
}

OP_STATUS StringUtils::ReplaceNewLines(OpString &replace_in)
{ 
	return StringUtils::ReplaceInternal(replace_in, UNI_L("\n"), UNI_L("<BR>"), true);  
}

UINT StringUtils::NumberFromString(const OpStringC& find_in, UINT find_from)
{
	const int find_in_length = find_in.Length();
	OP_ASSERT(int(find_from) <= find_in_length);

	OpString number_string;

	for (int index = find_from; index < find_in_length; ++index)
	{
		const uni_char cur_char = find_in[index];

		if (uni_isdigit(cur_char))
			number_string.Append(&cur_char, 1);
		else
			break;
	}

	return uni_atoi(number_string.CStr());
}


BOOL StringUtils::IsNumber(const OpStringC& input)
{
	if (input.IsEmpty())
		return FALSE;

	for (int index = 0, length = input.Length(); index < length; ++index)
	{
		const uni_char cur_char = input[index];
		if (!uni_isdigit(cur_char))
			return FALSE;
	}

	return TRUE;
}


OP_STATUS StringUtils::Strip(OpString& to_strip,
						 const OpStringC& strip_characters, BOOL strip_leading,
	   BOOL strip_trailing)
{
	if (strip_leading)
		while (to_strip.HasContent() &&
					 strip_characters.FindFirstOf(to_strip[0]) != KNotFound)
			to_strip.Delete(0, 1);

	if (strip_trailing)
		while (to_strip.HasContent())
	{
		const int last = to_strip.Length() - 1;
		if (strip_characters.FindFirstOf(to_strip[last]) != KNotFound)
			to_strip.Delete(last);
		else
			break;
	}

	return OpStatus::OK;
}

OP_STATUS StringUtils::StripToValidXMLName(XMLVersion version, const uni_char* in_name, OpString& out_name)
{
    if (!in_name)
		return OpStatus::ERR;
	
	unsigned name_length = uni_strlen(in_name);
		
	if (name_length == 0)
		return OpStatus::ERR;

    unsigned ch = XMLUtils::GetNextCharacter(in_name, name_length);
    INT32 out_index = 0;
    OpString out_tmp;
    out_tmp.Reserve(name_length + 1);
    
    if (XMLUtils::IsNameFirst(version, ch))
        out_tmp.CStr()[out_index++] = ch;
        
	while (name_length != 0)
	{
		ch = XMLUtils::GetNextCharacter(in_name, name_length);
		
		if (XMLUtils::IsName(version, ch))
			out_tmp.CStr()[out_index++] = ch;
	}
			
	out_tmp.DataPtr()[out_index] = '\0';
	out_name.Set(out_tmp);

	return TRUE;
}

UINT StringUtils::SubstringCount(const OpStringC& input, const OpStringC& substring)
{
	UINT count = 0;

	int find_pos = StringUtils::Find(input, substring, 0);
	while (find_pos != KNotFound)
	{
		++count;
		find_pos = StringUtils::Find(input, substring, find_pos + 1);
	}

	return count;
}


int StringUtils::FindNthOf(UINT n, const OpStringC& input, const OpStringC& substring)
{
	UINT count = 0;
	int find_pos = StringUtils::Find(input, substring, 0);
	while (find_pos != KNotFound && count < n)
	{
		++count;
		find_pos = StringUtils::Find(input, substring, find_pos + 1);
	}

	if (count != n)
		return KNotFound;

	return find_pos;
}


OP_STATUS StringUtils::DecodeEscapedChars(const uni_char* in, OpString& result, BOOL hex_only)
{
	if(!in)
	{
		return OpStatus::ERR_NULL_POINTER;
	}

	int len = uni_strlen(in);
	uni_char* text = result.Reserve(len+1);

	if(!text)
	{
        return OpStatus::ERR_NO_MEMORY;
	}

	int new_len = 0;
    for (int i=0; i<len; i++, new_len++)
	{
		if (in[i] == '%' && i+2 < len)
		{
			unsigned char token = 0;

			BOOL can_decode = FALSE;
			if( hex_only )
			{
				can_decode = op_isxdigit(in[i+1]) && op_isxdigit(in[i+2]);
			}
			else
			{
				can_decode = in[i+1] < 256 && in[i+2] < 256;
			}

			if( can_decode )
			{
				token = UriUnescape::Decode((char)in[i+1], (char)in[i+2]);
			}
			if( token >= 0x20 || (token >= 0x9 && token <= 0xd) )
			{
				text[new_len] = token;
				i += 2;
			}
			else
			{
				text[new_len] = '%';
			}
		}
		else
		{
			text[new_len] = in[i];
		}
	}
	text[new_len] = 0;

	return OpStatus::OK;
}

OP_STATUS StringUtils::FindDomain(const OpStringC& address, OpString& domain, unsigned& starting_position)
{
	int starting_pos = 0;
	int domain_length = 0;
	int protocol_length = 0;
	bool has_protocol = false;

	OpString url;
	RETURN_IF_ERROR(url.Set(address));

	url.MakeLower();

	int colon_index = address.FindFirstOf(':');

	if (colon_index != KNotFound)
	{
		url[colon_index] = '\0';
		URLType protocol = urlManager->MapUrlType(url.CStr());

		if (protocol != URL_UNKNOWN && protocol != URL_NULL_TYPE)
		{
			has_protocol = true;
			const char* protocol_str = urlManager->MapUrlType(protocol);
			protocol_length = op_strlen(protocol_str) + 3; // 3 == ://
		}
		url[colon_index] = ':';
	}

	if (!has_protocol)
	{
		RETURN_IF_ERROR(url.Insert(0, "http://")); // Assume http
		protocol_length = 7;
	}

	URL tmp = urlManager->GetURL(url);

	INTPTR dummy;
	ServerName* server = (ServerName*)tmp.GetAttribute(URL::KServerName, &dummy);
	if (server)
	{
		bool ansi = url.Find(server->GetName().CStr()) != KNotFound;
		domain_length = ansi ? server->GetName().Length() : server->GetUniName().Length();

		// Calculate starting position. We must take into consideration two things: protocol and possible "www."
		if (has_protocol)
		{
			starting_pos = ansi ?
					url.Find(server->GetName().CStr(), protocol_length) :
					url.Find(server->GetUniName().CStr(), protocol_length);

			OP_ASSERT(starting_pos != KNotFound);
		}
		if (address.Find("www.", starting_pos) == starting_pos)
		{
			starting_pos += 4;
			domain_length -= 4;
		}
	}
	RETURN_IF_ERROR(domain.Set(address.SubString(starting_pos).CStr(), domain_length));
	starting_position = starting_pos;
	return OpStatus::OK;
}

/***********************************************************************************
**
**  AppendColor
**
***********************************************************************************/
OP_STATUS StringUtils::AppendColor(OpString& input, COLORREF color, unsigned starting_pos, unsigned length)
{
	if (!length)
	{
		return OpStatus::OK;
	}
	OP_ASSERT(input.Length() >= static_cast<int>(starting_pos + length));
	RETURN_IF_ERROR(input.Insert(starting_pos + length, "</c>"));
	OpString starting_tag;
	RETURN_IF_ERROR(starting_tag.AppendFormat("<c=%d>", color));
	RETURN_IF_ERROR(input.Insert(starting_pos, starting_tag));
	return OpStatus::OK;
}

/***********************************************************************************
**
**  AppendHighlight
**
***********************************************************************************/
OP_STATUS StringUtils::AppendHighlight(OpString& input, const OpStringC& search_terms)
{
	RETURN_IF_ERROR(RemoveHighlight(input));

	if(search_terms.IsEmpty())
		return OpStatus::OK;

	OpString output;
	WordHighlighter word_highlighter;
	RETURN_IF_ERROR(word_highlighter.Init(search_terms.CStr()));
	RETURN_IF_ERROR(word_highlighter.AppendHighlight(output,
											input.CStr(),
											KAll,
											UNI_L("<b>"),
											UNI_L("</b>"),
											-1));

	return input.Set(output.CStr());
}

/***********************************************************************************
**
**  RemoveHighlight
**
***********************************************************************************/
OP_STATUS StringUtils::RemoveHighlight(OpString& input)
{
	RETURN_IF_ERROR(StringUtils::Replace(input,
										 UNI_L("<b>"),
										 UNI_L("")));

	return StringUtils::Replace(input,
								UNI_L("</b>"),
								UNI_L(""));
}


BOOL StringUtils::StartsWith(const OpStringC& text, const OpStringC& candidate, BOOL case_insensitive)
{
	if (text.IsEmpty() || candidate.IsEmpty())
		return FALSE;

	size_t candidate_len = candidate.Length();

	if (case_insensitive)
		return uni_strnicmp(candidate.CStr(), text.CStr(), candidate_len) == 0;
	return uni_strncmp(candidate.CStr(), text.CStr(), candidate_len) == 0;
}


BOOL StringUtils::EndsWith(const OpStringC& text, const OpStringC& candidate, BOOL case_insensitive)
{
	if (text.IsEmpty() || candidate.IsEmpty())
		return FALSE;

	size_t text_len = text.Length();
	size_t candidate_len = candidate.Length();

	if (candidate_len>text_len)
		return FALSE;

	size_t offset = text_len - candidate_len;

	if (case_insensitive)
		return uni_strnicmp(candidate.CStr(), &(text.CStr()[offset]), candidate_len) == 0;
	return uni_strncmp(candidate.CStr(), &(text.CStr()[offset]), candidate_len) == 0;
}


void StringUtils::Unquote(OpString& text)
{
	int text_length = text.Length();
	if (text_length > 1 && StringUtils::StartsWith(text, UNI_L("\""), FALSE) && StringUtils::EndsWith(text, UNI_L("\""), FALSE))
	{
		text.Delete(text_length-1);
		text.Delete(0,1);
	}
}

/***********************************************************************************
**
**  UniToUTF8
**
***********************************************************************************/
char *StringUtils::UniToUTF8(const uni_char *uni_string)
{
	if (!uni_string)
		return NULL;

	OpString8 str;
	if (OpStatus::IsError(str.SetUTF8FromUTF16(uni_string)))
		return NULL; // OOM

	return op_strdup(str.CStr());
}


/***********************************************************************************
**
**  SplitWord
**
***********************************************************************************/
const uni_char* StringUtils::SplitWord(const uni_char* string, uni_char separator, OpString& word)
{
	OP_ASSERT(separator != '\0');
	if (!string)
		return NULL;

	// Skip separators at the start
	while (*string == separator)
		string++;

	if (!*string)
		return NULL;

	const uni_char* word_end = string;
	if (*string == '"')
	{
		// Find the end quote in a quoted string
		word_end = ++string;
		while (*word_end && *word_end != '"')
			word_end++;
	}
	else
	{
		// Find the ending separator
		while (*word_end && *word_end != separator)
			word_end++;
	}

	RETURN_VALUE_IF_ERROR(word.Set(string, word_end - string), NULL);

	// Skip past the separator / quote if we found one for the next word
	return *word_end ? word_end + 1 : word_end;
}



/***********************************************************************************
**
**  EllipticText
**
***********************************************************************************/
OP_STATUS StringUtils::EllipticText( OpString& text, UINT32 max_width, BOOL center, OpFont* font )
{
	UINT32 length = text.Length();
	UINT32 width  = font->StringWidth(text.CStr(), length);
	if( width <= max_width )
	{
		return OpStatus::OK;
	}
	
	OpString s;
	UINT32 start_offset = 0;
	int d_width = font->StringWidth(UNI_L("..."), 3);
	
	if( center )
	{
		if( start_offset > length ) { start_offset = 0; }
		
		UINT32 l_offset  = 0;
		UINT32 l_len     = start_offset;
		UINT32 r_offset  = length - start_offset;
		UINT32 r_len     = start_offset;
		
		UINT32 s_width = max_width / 2 - d_width;
		UINT32 l_width = font->StringWidth(&text.CStr()[l_offset], l_len);
		UINT32 r_width = font->StringWidth(&text.CStr()[r_offset], r_len);
		
		while( l_width < s_width )
		{
			l_len ++;
			l_width = font->StringWidth(&text.CStr()[l_offset], l_len);
			if( l_width + r_width + d_width > max_width )
			{
				l_len --;
				l_width = font->StringWidth(&text.CStr()[l_offset], l_len);
				break;
			}
		}
		
		while( l_width + r_width + d_width < max_width )
		{
			r_offset --;
			r_len ++;
			r_width = font->StringWidth(&text.CStr()[r_offset], r_len);
			if( l_width + r_width + d_width > max_width )
			{
				r_offset ++;
				r_len --;
				r_width = font->StringWidth(&text.CStr()[r_offset], r_len);
				break;
			}
		}
		
		RETURN_IF_ERROR(s.Set( &text.CStr()[l_offset], l_len));
		RETURN_IF_ERROR(s.Append("..."));
		RETURN_IF_ERROR(s.Append(&text.CStr()[r_offset], r_len));
	}
	else
	{
		while( width > max_width - d_width)
		{
			length--;
			width -= font->StringWidth(&text.CStr()[length], 1);
		}
		
		RETURN_IF_ERROR(s.Set(text.CStr(), length));
		RETURN_IF_ERROR(s.Append("..."));
	}
	
	return text.Set(s);
}








/***********************************************************************************
**
**  GenerateClientID
**
***********************************************************************************/
OP_STATUS StringUtils::GenerateClientID(OpString& outstr)
{
	OpGuid guid;
	// for compatibility reason with Opera Link, the generated UUID MUST be upper case!
	const static char *pszHex = "0123456789ABCDEF";	// DO NOT CHANGE, SEE ABOVE!
	byte *hash = NULL;
	OpString8 strHash;
	char *pszHash = strHash.Reserve((sizeof(OpGuid) * 2) + 1);

	RETURN_IF_ERROR(g_opguidManager->GenerateGuid(guid));

	hash = (byte *)&guid;

	for ( size_t nByte = 0 ; nByte < sizeof(OpGuid) ; nByte++, hash++ )
	{
		*pszHash++ = pszHex[ *hash >> 4 ];
		*pszHash++ = pszHex[ *hash & 15 ];
	}
	*pszHash = '\0';

	RETURN_IF_ERROR(outstr.Set(strHash));
	return OpStatus::OK;
}

/***********************************************************************************
**
**  GetDefaultWritingSystem
**
***********************************************************************************/
WritingSystem::Script StringUtils::GetDefaultWritingSystem()
{
	OpString8 lang;
	WritingSystem::Script script = WritingSystem::Unknown;
	if (OpStatus::IsSuccess(lang.Set(g_languageManager->GetLanguage().CStr())))
		script = WritingSystem::FromLanguageCode(lang.CStr());
	return script;
}

OP_STATUS StringUtils::ExpandParameters(const ExpansionProvider& expander,
		const uni_char* parameters, ExpansionResult& expansion_result)
{
	if (!parameters)
		return OpStatus::OK;

	while (*parameters)
	{
		if (*parameters != '%' || *(++parameters) == '%')
		{
			expansion_result.WriteRawChar(*parameters);
			parameters++;
			continue;
		}

		if (*parameters == 0)
			break;

		BOOL enquote = *parameters == uni_toupper(*parameters);

		OpString expansion;
		switch (uni_toupper(*parameters))
		{
			case 'C':
				RETURN_IF_ERROR(expander.GetClipboardText(expansion));
				break;

			case 'S':
				RETURN_IF_ERROR(expander.GetCurrentDocFilePath(expansion));
				break;

			case 'U':
				RETURN_IF_ERROR(expander.GetCurrentDocUrlName(expansion));
				break;

			case 'L':
				RETURN_IF_ERROR(expander.GetCurrentClickedUrlName(expansion));
				break;

			case 'T':
				RETURN_IF_ERROR(expander.GetSelectedText(expansion));
				break;
		}

		if (enquote && expansion.HasContent())
		{
			expansion.Insert(0, "\"");
			expansion.Append("\"");
		}

		if (expansion.HasContent())
			RETURN_IF_ERROR(expansion_result.WriteArgument(expansion));

		parameters++;
	}

	return expansion_result.Done();
}

OP_STATUS StringUtils::ExpandParameters(OpWindowCommander* window_commander
		, const uni_char* parameters
		, OpString& expanded_parameters
		, BOOL can_expand_environment_variables
		, DesktopMenuContext* menu_context
)
{
	StringExpansionResult result(expanded_parameters);
	RETURN_IF_ERROR(ExpandParameters(
					DefaultExpansionProvider(menu_context, window_commander),
					parameters,	
					result));
	
#ifdef PREFS_CAN_EXPAND_ENV_VARIABLES
	if (can_expand_environment_variables)
    {
		// Shall only be done on local program paths. Fixes bug #210848
		OpString outparam;
		outparam.Reserve(2048);
		RETURN_IF_ERROR(g_op_system_info->ExpandSystemVariablesInString(
					expanded_parameters.CStr(), &outparam));
		RETURN_IF_ERROR(expanded_parameters.Set(outparam.CStr()));
	}
#endif // PREFS_CAN_EXPAND_ENV_VARIABLES
	return OpStatus::OK;
}

// This function use default expander and output to ExpansionResult
OP_STATUS StringUtils::ExpandParameters(OpWindowCommander* window_commander
		, const uni_char* parameters
		, ExpansionResult& expanded_parameters
		, DesktopMenuContext* menu_context
)
{
	return	ExpandParameters(
			DefaultExpansionProvider(menu_context, window_commander),
			parameters,	
			expanded_parameters);
}

namespace StringUtils
{
	ArrayExpansionResult::~ArrayExpansionResult()
	{
		OP_DELETEA(m_argv);
	}

	OP_STATUS ArrayExpansionResult::WriteRawChar(uni_char c)
	{
		if (c != UNI_L(' '))
			return m_current_arg.Append(&c, 1);
		else 
			return SaveArg();
	}

	OP_STATUS ArrayExpansionResult::WriteArgument(OpStringC arg)
	{
		RETURN_IF_ERROR(m_current_arg.Append(arg));
		return SaveArg();
	}

	OP_STATUS ArrayExpansionResult::Done()
	{
		return SaveArg();
	}

	OP_STATUS ArrayExpansionResult::GetResult(int& argc, const char**& argv)
	{
		argc = m_args.GetCount();
		if (!m_argv && argc)
		{
			m_argv = OP_NEWA(const char*, argc);
			RETURN_OOM_IF_NULL(m_argv);
			for (int i=0; i<argc; i++)
			{
				m_argv[i] = m_args.Get(i)->CStr();
			}
		}
		argv = m_argv;
		return OpStatus::OK;
	}

	OP_STATUS ArrayExpansionResult::SaveArg()
	{
		m_current_arg.Strip();
		if (m_current_arg.HasContent())
		{
			OpString8* new_arg = OP_NEW(OpString8, ());
			RETURN_OOM_IF_NULL(new_arg);
			RETURN_IF_ERROR(new_arg->SetUTF8FromUTF16(m_current_arg));
			RETURN_IF_ERROR(m_args.Add(new_arg));
			m_current_arg.Empty();
		}

	#ifdef PREFS_CAN_EXPAND_ENV_VARIABLES
		if (m_expand_environment_variables)
		{
			// Shall only be done on local program paths. Fixes bug #210848
			OpString outparam;
			outparam.Reserve(2048);
			RETURN_IF_ERROR(g_op_system_info->ExpandSystemVariablesInString(
						m_current_arg.CStr(), &outparam));
			RETURN_IF_ERROR(m_current_arg.Set(outparam.CStr()));
		}
	#endif // PREFS_CAN_EXPAND_ENV_VARIABLES

		return OpStatus::OK;
	}


	OP_STATUS DefaultExpansionProvider::GetClipboardText(OpString& text) const
	{
		return g_desktop_clipboard_manager->GetText(text);
	}

	OP_STATUS DefaultExpansionProvider::GetSelectedText(OpString& text) const
	{
		return m_window_commander ? text.Set(m_window_commander->GetSelectedText()) : OpStatus::OK;
	}

	OP_STATUS DefaultExpansionProvider::GetCurrentDocFilePath(OpString& path) const
	{
		if (WindowCommanderProxy::HasCurrentDoc(m_window_commander))
		{
			URL url = WindowCommanderProxy::GetCurrentDocURL(m_window_commander);
#ifdef URL_CAP_PREPAREVIEW_FORCE_TO_FILE
			url.PrepareForViewing(URL::KFollowRedirect, TRUE, TRUE, TRUE);
#else // URL_CAP_PREPAREVIEW_FORCE_TO_FILE
			url.PrepareForViewing(TRUE);
#endif
			RETURN_IF_ERROR(
					url.GetAttribute(URL::KFilePathName_L, path, TRUE));
		}
		return OpStatus::OK;
	}

	OP_STATUS DefaultExpansionProvider::GetCurrentDocUrlName(OpString& name) const
	{
		if (WindowCommanderProxy::HasCurrentDoc(m_window_commander))
		{
			URL url = WindowCommanderProxy::GetCurrentDocURL(m_window_commander);
			RETURN_IF_ERROR(
					url.GetAttribute(URL::KUniName_With_Fragment, name));
		}
		return OpStatus::OK;
	}

	OP_STATUS DefaultExpansionProvider::GetCurrentClickedUrlName(OpString& name) const
	{
		if (m_menu_context && m_menu_context->GetDocumentContext())
		{
			RETURN_IF_ERROR(
					m_menu_context->GetDocumentContext()->GetLinkAddress(&name));
		}
		return OpStatus::OK;
	}
}

#ifdef CRYPTO_HASH_MD5_USE_CORE_IMPLEMENTATION


/***********************************************************************************
**
**  CalculateMD5Checksum
**	- Calculate a MD5 checksum out of a string
**
***********************************************************************************/
OP_STATUS StringUtils::CalculateMD5Checksum(OpString8& str, OpString8& md5_hash)
{
	CryptoHash *hasher = CryptoHash::CreateMD5();
	RETURN_OOM_IF_NULL(hasher);
	ANCHOR_PTR(CryptoHash, hasher);

	unsigned char hash_buffer[MD5_DIGEST_LENGTH];
	RETURN_OOM_IF_NULL(md5_hash.Reserve(MD5_DIGEST_LENGTH * 2));
	RETURN_IF_ERROR(hasher->InitHash());

	hasher->CalculateHash(str.CStr());
	hasher->ExtractHash(hash_buffer);
	RETURN_IF_ERROR(ByteToHexStr(hash_buffer, MD5_DIGEST_LENGTH, md5_hash));

	return OpStatus::OK;
}

#endif // CRYPTO_HASH_MD5_USE_CORE_IMPLEMENTATION

