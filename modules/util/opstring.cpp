/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/util/str.h"
#include "modules/util/tempbuf.h"
#include "modules/unicode/utf8.h"
#include "modules/stdlib/src/thirdparty_printf/uni_printf.h"

#if !defined(va_copy) && defined(__va_copy)
# define va_copy __va_copy
#endif // !va_copy && __va_copy


// OpStringC8 class

int OpStringC8::Compare(const char* aCStr,int aLength) const
{
	if (aCStr == NULL || *aCStr == '\0')
		return (iBuffer == NULL || *iBuffer == '\0') ? 0 : 1;
	if (iBuffer == NULL)
		return -1;
	if (aLength == KAll)
		return op_strcmp(iBuffer, aCStr);
	return op_strncmp(iBuffer, aCStr, aLength);
}

int OpStringC8::CompareI(const char* aCStr,int aLength) const
{
	if (aCStr == NULL || *aCStr == '\0')
		return (iBuffer == NULL || *iBuffer == '\0') ? 0 : 1;
	if (iBuffer == NULL)
		return -1;
	if (aLength == KAll)
		return op_stricmp(iBuffer, aCStr);
	return op_strnicmp(iBuffer, aCStr, aLength);
}

int OpStringC8::Find(const char* aCStr, int aStartIndex) const
{
	if (aCStr && iBuffer)
	{
		OP_ASSERT(0 <= aStartIndex && aStartIndex <= Length());
		const char* s = op_strstr(iBuffer + aStartIndex, aCStr);
		if (s)
		{
			return s - iBuffer;
		}
	}

	return KNotFound;
}

int OpStringC8::FindI(const char* aCStr, int aStartIndex) const
{
	if (aCStr && iBuffer)
	{
		OP_ASSERT(0 <= aStartIndex && aStartIndex <= Length());
		const char* s = op_stristr(iBuffer + aStartIndex, aCStr);
		if (s)
		{
			return s - iBuffer;
		}
	}

	return KNotFound;
}

int OpStringC8::FindFirstOf(char aChar) const
{
	if (iBuffer)
	{
		const char* s = op_strchr(iBuffer, aChar);
		if (s)
		{
			return s - iBuffer;
		}
	}

	return KNotFound;
}

int OpStringC8::FindFirstOf(const OpStringC8& aCharsString, int idx) const
{
	if (iBuffer && (unsigned int)idx < op_strlen(iBuffer))
	{
		const char* s = op_strpbrk(iBuffer + idx, aCharsString.CStr());
		if (s)
		{
			return s - iBuffer;
		}
	}

	return KNotFound;
}

int OpStringC8::FindLastOf(char aChar) const
{
	if (iBuffer)
	{
		const char* s = op_strrchr(iBuffer, aChar);
		if (s)
		{
			return s - iBuffer;
		}
	}

	return KNotFound;
}

int OpStringC8::SpanOf(const OpStringC8 &aCharsString) const
{
	const char* str = CStr();
	const char* chars = aCharsString.CStr();
	if (!str || *str == 0 || !chars || *chars == 0)
	{
		return 0;
	}
	return op_strspn(str, chars);
}

int OpStringC8::SpanNotOf(const OpStringC8 &aCharsString) const
{
	const char* str = CStr();
	const char* chars = aCharsString.CStr();
	if (!str || *str == 0)
	{
		return 0;
	}
	else if (!chars || chars == 0)
	{
		return Length();
	}
	return op_strcspn(CStr(), aCharsString.CStr());
}

OP_STATUS
OpString8::AppendFormat(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	OP_STATUS result = AppendVFormat(format, args);
	va_end(args);
	return result;
}

OP_STATUS
OpString8::AppendVFormat(const char* format, va_list args)
{
#ifdef va_copy
	// One cannot assume (according to manual) that args will
	// remain unchanged after the call to OpPrintf::Format()

	va_list args_copy;
	va_copy(args_copy, args);

	int length = op_vsnprintf(NULL, 0, format, args_copy);

	va_end(args_copy);
#else // va_copy
	int length = op_vsnprintf(NULL, 0, format, args);
#endif // va_copy
	// NB: the printf functions return the length without the null.

	if (length < 0)
		return OpStatus::ERR;
	int prior_length = Length();

	RETURN_IF_ERROR(Grow(prior_length + length));
	op_vsnprintf(iBuffer + prior_length, length + 1, format, args);
	return OpStatus::OK;
}

// OpString8 class

OpString8::OpString8(OpString8& aString)
	: OpStringC8()
{
#ifdef MEMTEST
	OOM_REPORT_UNANCHORED_OBJECT();
#endif // MEMTEST
	iBuffer = aString.iBuffer;
	iSize = aString.iSize;
	aString.iBuffer = NULL;
	aString.iSize = 0;
}

OP_STATUS OpString8::TakeOver(OpString8& aString)
{
	Empty();

	iBuffer = aString.iBuffer;
	iSize = aString.iSize;

	aString.iBuffer = NULL;
	aString.iSize = 0;

	return OpStatus::OK;
}

OpString8& OpString8::Delete(int aPos,int aLength)
{
	OP_ASSERT((int)aPos <= Length());
	if (aLength == KAll)
		aLength = Length() - aPos;
	OP_ASSERT((int)aPos + aLength <= Length());
	if (iBuffer && aLength > 0)
		op_memmove(iBuffer + aPos, iBuffer + aPos + aLength, Length() - aPos - aLength + 1);
	return *this;
}

OpString8& OpString8::Strip(BOOL aLeading, BOOL aTrailing)
{
	if (iBuffer)
	{
		if (aLeading)
		{
			char *p = iBuffer;
			while (op_isspace(*(unsigned char *) p)) ++p;
			if (p > iBuffer)
			{
				op_memmove(iBuffer, p, op_strlen(p) + 1);
			}
		}
		if (aTrailing)
		{
			char *p = iBuffer + op_strlen(iBuffer) - 1;
			while (p >= iBuffer && op_isspace(*(unsigned char *) p)) *(p --) = 0;
		}
	}
	return *this;
}

OpString8 OpStringC8::SubString(unsigned int aPos, int aLength, OP_STATUS *err) const
{
	int my_length = Length();
	OP_ASSERT(0 <= my_length && aPos <= (unsigned int)my_length);
	if (aLength==KAll || (int)aPos + aLength > my_length)
		aLength = my_length - aPos;
	OP_ASSERT(0 <= (int)aPos+aLength && (int)aPos+aLength <= my_length);
	OpString8 s;
	OP_STATUS e = s.Set(&iBuffer[aPos], aLength);
	if (err)
		*err = e;
	return s;
}

OP_STATUS OpString8::SetUTF8FromUTF16(const uni_char* aUTF16str, int aLength /*=KAll*/)
{
	if (NULL == aUTF16str)
	{
		Empty();
		return OpStatus::OK;
	}

	if (aLength == KAll)
	{
		aLength = uni_strlen(aUTF16str);
	}

	UTF8Encoder encoder;
	int size = encoder.Measure(aUTF16str, UNICODE_SIZE(aLength));
	encoder.Reset();

	RETURN_IF_ERROR(Grow(size));

	int read_bytes;
	int written = encoder.Convert(aUTF16str, UNICODE_SIZE(aLength), iBuffer, size, &read_bytes);
	iBuffer[written] = 0;
	return OpStatus::OK;
}

OpString8& OpString8::SetUTF8FromUTF16L(const uni_char* aUTF16str, int aLength /*=KAll*/)
{
	LEAVE_IF_ERROR(SetUTF8FromUTF16(aUTF16str, aLength));
	return *this;
}

OP_STATUS OpString8::Set(const char* aCStr, int aLength)
{
	if (aCStr==NULL)
	{
		Empty();
		return OpStatus::OK;
	}
	if (aLength==KAll)
		aLength=Length(aCStr);
	OP_ASSERT(aCStr != iBuffer || iSize >= aLength); // Grow should not invalidate aCStr
	RETURN_IF_ERROR(Grow(aLength));
	if (aCStr!=iBuffer)
		op_strncpy(iBuffer, aCStr, aLength);
	iBuffer[aLength] = '\0';
	return OpStatus::OK;
}

OP_STATUS OpString8::Set(const uni_char* aCStr, int aLength)
{
	if (aCStr==NULL)
	{
		Empty();
		return OpStatus::OK;
	}
	if (aLength == KAll)
		aLength = uni_strlen(aCStr);
	RETURN_IF_ERROR(Grow(aLength));
	const uni_char* s = aCStr;
	char* t = iBuffer;
	for (int i=0; i<aLength && *s; i++)
		*t++ = (char) *s++;
	*t = '\0';
	return OpStatus::OK;
}

OpString8& OpString8::SetL(const OpStringC8& aString, int aLength)
{
	LEAVE_IF_ERROR(Set(aString.CStr(), MIN(aLength, aString.Length())));
	return *this;
}

OpString8& OpString8::SetL(const char* aCStr, int aLength)
{
	LEAVE_IF_ERROR(Set(aCStr, aLength));
	return *this;
}

OpString8& OpString8::SetL(const uni_char* aCStr, int aLength)
{
	LEAVE_IF_ERROR(Set(aCStr, aLength));
	return *this;
}

OP_STATUS OpString8::Insert(int aPos, const char* aCStr, int aLength)
{
	OP_ASSERT( aPos >= 0 && aPos <= Length() );
	if (aCStr == NULL)
		return OpStatus::OK;

	if (aLength == KAll)
		aLength = op_strlen(aCStr);

	int len = Length();

	RETURN_IF_ERROR(Grow(len + aLength));
	op_memmove(iBuffer+aPos+aLength, iBuffer+aPos, (len-aPos+1)*sizeof(char));	// Always includes terminating NUL
	op_memmove(iBuffer+aPos, aCStr, aLength);

	return OpStatus::OK;
}

OpString8& OpString8::InsertL(int aPos,const char* aCStr, int aLength)
{
	LEAVE_IF_ERROR(Insert(aPos, aCStr, aLength));
	return *this;
}

OP_STATUS OpString8::Append(const char* aCStr,int aLength)
{
	if (aCStr == NULL)
		return OpStatus::OK;

	if (aLength == KAll)
		aLength = op_strlen(aCStr);

	int len = Length();
	int new_len = len + aLength;

	RETURN_IF_ERROR(Grow(new_len));
	op_strncpy(iBuffer + len, aCStr, aLength);
	iBuffer[new_len] = 0;

	return OpStatus::OK;
}

OpString8& OpString8::AppendL(const char* aCStr,int aLength)
{
	LEAVE_IF_ERROR(Append(aCStr, aLength));
	return *this;
}

char* OpString8::Reserve(int aMaxLength)
{
	return OpStatus::IsSuccess(Grow(aMaxLength)) ? CStr() : NULL;
}

char* OpString8::ReserveL(int aMaxLength)
{
	LEAVE_IF_ERROR(Grow(aMaxLength));
	return CStr();
}

const char* GetS(const OpStringC8& s)
{
	return s.CStr() ? s.CStr() : "";
}

OP_STATUS OpString8::SetConcat(
	const OpStringC8& str1, const OpStringC8& str2,
	const OpStringC8& str3, const OpStringC8& str4)
{
	if (iBuffer)
		*iBuffer = '\0';
	return AppendFormat("%s%s%s%s", GetS(str1), GetS(str2), GetS(str3), GetS(str4));
}

OP_STATUS OpString8::SetConcat(
	const OpStringC8& str1, const OpStringC8& str2,
	const OpStringC8& str3, const OpStringC8& str4,
	const OpStringC8& str5, const OpStringC8& str6,
	const OpStringC8& str7, const OpStringC8& str8)
{
	if (iBuffer)
		*iBuffer = '\0';
	return AppendFormat("%s%s%s%s%s%s%s%s", GetS(str1), GetS(str2), GetS(str3), GetS(str4),
											GetS(str5), GetS(str6), GetS(str7), GetS(str8));
}

void OpString8::Wipe()
{
	if (iBuffer)
	{
		op_memset(iBuffer, 0, iSize);
	}
}

void OpString8::TakeOver(char* aString)
{
	Empty();
	iBuffer = aString;
	iSize = aString ? op_strlen(aString) : 0;
}
void OpString8::MakeUpper()
{
	if (iBuffer)
		op_strupr(iBuffer);
}

// private

OP_STATUS OpString8::Grow(int aLength)
{
	if (iBuffer && iSize >= aLength)
		return OpStatus::OK;

	// Round size up to nearest quantum
	size_t newSize = aLength + 1; // add NUL
	newSize = ROUND_UP_TO_NEXT_MALLOC_QUANTUM(newSize);

	char *newBuffer = OP_NEWA(char, newSize);
	if (!newBuffer)
		return OpStatus::ERR_NO_MEMORY;

	if (iBuffer)
	{
		op_strncpy(newBuffer, iBuffer, iSize);
		OP_DELETEA(iBuffer);
	}
	iBuffer = newBuffer;
	iBuffer[iSize] = 0;
	iSize = newSize - 1;

	return OpStatus::OK;
}

void OpString8::Empty()
{
	if (iBuffer)
		OP_DELETEA(iBuffer);

	iBuffer = NULL;
	iSize = 0;
}

OP_STATUS
OpString8::ReplaceAll(const char* needle, const char* subject, int number_of_occurrences)
{
	if (!needle || !*needle)
		return OpStatus::OK;

	int needle_position = Find(needle);
	if (needle_position != KNotFound && number_of_occurrences != 0)
	{
		int current_index = 0, needle_length = op_strlen(needle);
		TempBuffer str_temp;//temp buffer is more efficient when Appending, because it caches the length
		do
		{
			RETURN_IF_ERROR(str_temp.Append(CStr() + current_index, needle_position - current_index));
			RETURN_IF_ERROR(str_temp.Append(subject));

			current_index = needle_position + needle_length;
			number_of_occurrences--;
			needle_position = Find(needle, current_index);
		}
		while (needle_position != KNotFound && number_of_occurrences != 0);

		RETURN_IF_ERROR(str_temp.Append(CStr() + current_index));

		RETURN_IF_ERROR(Set(str_temp.GetStorage(), str_temp.Length()));
	}
	return OpStatus::OK;
}

// OpStringC16 class

int OpStringC16::Length(const char* aCStr)
{
	return aCStr == NULL ? 0 : op_strlen(aCStr);
}

int OpStringC16::Length(const uni_char* aCStr)
{
	return aCStr == NULL ? 0 : uni_strlen(aCStr);
}

char *OpStringC16::UTF8AllocL() const
{
	char *s;
	LEAVE_IF_ERROR(UTF8Alloc(&s));
	return s;
}

OP_STATUS OpStringC16::UTF8Alloc(char **result) const
{
	if (NULL == iBuffer) {
		*result = NULL;
		return OpStatus::OK;
	}

	UTF8Encoder encoder;
	int ibuflenbytes = (uni_strlen(iBuffer) + 1) * sizeof (uni_char);
	int bytes = encoder.Measure(iBuffer, ibuflenbytes);
	encoder.Reset();

	char *rc = OP_NEWA(char, bytes);
	if (!rc)
		return OpStatus::ERR_NO_MEMORY;
	encoder.Convert((char *) iBuffer, ibuflenbytes, rc, bytes, &bytes);
	*result = rc;
	return OpStatus::OK;
}

int OpStringC16::UTF8(char *buf, int size) const
{
	if (NULL == iBuffer)
		return 0;

	UTF8Encoder encoder;
	int ibuflenbytes = (uni_strlen(iBuffer) + 1) * sizeof (uni_char);
	if (-1 == size)
	{
		return encoder.Measure(iBuffer, ibuflenbytes);
	}
	else
	{
		int dummy;
		int rc = encoder.Convert((char *) iBuffer, ibuflenbytes, buf, size, &dummy);
		return rc;
	}
}

int OpStringC16::Compare(const char* aCStr, int aLength) const
{
	if (aCStr == NULL || *aCStr == '\0')
		return (iBuffer == NULL || *iBuffer == '\0') ? 0 : 1;
	if (iBuffer == NULL)
		return -1;
	if (aLength == KAll)
		return uni_strcmp(iBuffer, aCStr);
	return uni_strncmp(iBuffer, aCStr, aLength);
}

int OpStringC16::Compare(const uni_char* aCStr, int aLength) const
{
	if (aCStr == NULL || *aCStr == '\0')
		return (iBuffer == NULL || *iBuffer == '\0') ? 0 : 1;
	if (iBuffer == NULL)
		return -1;
	if (aLength == KAll)
		return uni_strcmp(iBuffer, aCStr);
	return uni_strncmp(iBuffer, aCStr, aLength);
}

int OpStringC16::CompareI(const char* aCStr, int aLength) const
{
	if (aCStr == NULL || *aCStr == '\0')
		return (iBuffer == NULL || *iBuffer == '\0') ? 0 : 1;
	if (iBuffer == NULL)
		return -1;
	if (aLength == KAll)
		return uni_stricmp(iBuffer, aCStr);
	return uni_strnicmp(iBuffer, aCStr, aLength);
}

int OpStringC16::CompareI(const uni_char* aCStr, int aLength) const
{
	if (aCStr == NULL || *aCStr == '\0')
		return (iBuffer == NULL || *iBuffer == '\0') ? 0 : 1;
	if (iBuffer == NULL)
		return -1;
	if (aLength == KAll)
		return uni_stricmp(iBuffer, aCStr);
	return uni_strnicmp(iBuffer, aCStr, aLength);
}

int OpStringC16::Find(const char* aCStr, int aStartIndex) const
{
	if (aCStr && iBuffer)
	{
		OP_ASSERT(0 <= aStartIndex && aStartIndex <= Length());
		const uni_char* s = uni_strstr(iBuffer + aStartIndex, aCStr);
		if (s)
		{
			return s - iBuffer;
		}
	}

	return KNotFound;
}

int OpStringC16::Find(const uni_char* aCStr, int aStartIndex) const
{
	if (aCStr && iBuffer)
	{
		OP_ASSERT(0 <= aStartIndex && aStartIndex <= Length());
		const uni_char* s = uni_strstr(iBuffer + aStartIndex,aCStr);
		if (s)
		{
			return s - iBuffer;
		}
	}

	return KNotFound;
}

int OpStringC16::FindI(const char* aCStr, int aLength) const
{
	OP_ASSERT(aLength == KAll); // aLength parameter is DEPRECATED
	return FindI(aCStr);
}

int OpStringC16::FindI(const char* aCStr) const
{
	if (aCStr && iBuffer)
	{
		const uni_char* s = uni_stristr(iBuffer, aCStr);
		if (s)
		{
			return s - iBuffer;
		}
	}

	return KNotFound;
}

int OpStringC16::FindI(const uni_char* aCStr) const
{
	if (aCStr && iBuffer)
	{
		const uni_char* s = uni_stristr(iBuffer,aCStr);
		if (s)
		{
			return s - iBuffer;
		}
	}

	return KNotFound;
}

int OpStringC16::FindFirstOf(uni_char aChar) const
{
	if (iBuffer==NULL)
		return KNotFound;
	const uni_char *s = uni_strchr(iBuffer,aChar);
	if (s==NULL)
		return KNotFound;
	return s-iBuffer;
}

int OpStringC16::FindFirstOf(const OpStringC16& aCharsString, int idx) const
{
	if (iBuffer==NULL || (unsigned int)idx >= uni_strlen(iBuffer))
		return KNotFound;
	const uni_char *s = uni_strpbrk(iBuffer + idx, aCharsString.CStr());
	if (s==NULL)
		return KNotFound;
	return s-iBuffer;
}

int OpStringC16::FindLastOf(uni_char aChar) const
{
	if (iBuffer==NULL)
		return KNotFound;
	const uni_char *s = uni_strrchr(iBuffer,aChar);
	if (s==NULL)
		return KNotFound;
	return s-iBuffer;
}

int OpStringC16::SpanOf(const OpStringC16& aCharsString) const
{
	int num_chars = 0;
	const uni_char* chars = (uni_char*) aCharsString.CStr();
	uni_char* str = iBuffer;
	if (!str || *str == 0 || !chars || *chars == 0)
	{
		return 0;
	}
	int chars_len = uni_strlen(chars);
	while (*str)
	{
		BOOL found_char = FALSE;
		for (int i = 0; i < chars_len; i++)
		{
			if (*str == chars[i])
			{
				num_chars++;
				found_char = TRUE;
				break;
			}
		}
		if (!found_char)
		{
			return num_chars;
		}
		str++;
	}
	return num_chars;
}

// OpString16 class

OpString16::OpString16(OpString16& aString)
	: OpStringC16()
{
#ifdef MEMTEST
	OOM_REPORT_UNANCHORED_OBJECT();
#endif // MEMTEST
	iBuffer = aString.iBuffer;
	iSize = aString.iSize;
	aString.iBuffer = 0;
	aString.iSize = 0;
}

OP_STATUS OpString16::SetFromUTF8(const char* aUTF8str, int aLength /*=KAll */)
{
	if (NULL == aUTF8str)
	{
		Empty();
		return OpStatus::OK;
	}

	if (aLength == KAll)
		aLength = op_strlen(aUTF8str);

	UTF8Decoder decoder;
	int size = decoder.Measure(aUTF8str, aLength);
	decoder.Reset();

	// size is already number of bytes, but does not include trailing null
	RETURN_IF_ERROR(Grow(UNICODE_DOWNSIZE(size)));
	decoder.Convert((char *) aUTF8str, aLength, (char *) iBuffer, size, NULL);
	iBuffer[UNICODE_DOWNSIZE(size)] = 0;
	return OpStatus::OK;
}

OpString16& OpString16::SetFromUTF8L(const char* aUTF8str, int aLength /*=KAll */)
{
	LEAVE_IF_ERROR(SetFromUTF8(aUTF8str,aLength));
	return *this;
}

OP_STATUS OpString16::Set(const uni_char* aCStr, int aLength)
{
	if (aCStr==NULL)
	{
		Empty();
		return OpStatus::OK;
	}
	if (aLength==KAll)
		aLength=Length(aCStr);
	OP_ASSERT(aCStr != iBuffer || iSize >= aLength); // Grow should not invalidate aCStr
	RETURN_IF_ERROR(Grow(aLength));
	if (aCStr!=iBuffer)
		uni_strncpy(iBuffer, aCStr, aLength);
	iBuffer[aLength] = 0;
	return OpStatus::OK;
}

OpString16& OpString16::SetL(const uni_char* aCStr, int aLength)
{
	LEAVE_IF_ERROR(Set(aCStr,aLength));
	return *this;
}

OP_STATUS OpString16::Set(const char* aCStr, int aLength)
{
	if (aCStr==NULL)
	{
		Empty();
		return OpStatus::OK;
	}
	if (aLength == KAll)
		aLength = Length(aCStr);
	RETURN_IF_ERROR(Grow(aLength));
	const unsigned char *s = (const unsigned char *) aCStr;
	uni_char *t = iBuffer;
	for (int i=0; i<aLength && *s; i++)
        *t++ = (uni_char) *s++;
	*t = '\0';
	return OpStatus::OK;
}

OpString16& OpString16::SetL(const char* aCStr, int aLength)
{
	LEAVE_IF_ERROR(Set(aCStr,aLength));
	return *this;
}

OP_STATUS OpString16::Insert(int aPos,const char* aCStr, int aLength)
{
	OP_ASSERT( aPos >= 0 && aPos <= Length() );
	if (aCStr == NULL)
		return OpStatus::OK;

	if (aLength == KAll)
		aLength = op_strlen(aCStr);

	int len = Length();

	RETURN_IF_ERROR(Grow(len + aLength));
	op_memmove(iBuffer+aPos+aLength, iBuffer+aPos, (len-aPos+1)*sizeof(uni_char));	// Always includes terminating NUL
	uni_char* t = iBuffer + aPos;
	const unsigned char* s = (const unsigned char*)aCStr;
	for (int i=0; i < aLength; ++i)
	{
		*t++ = (uni_char)*s++;
	}

	return OpStatus::OK;
}

OP_STATUS OpString16::Insert(int aPos,const uni_char* aCStr, int aLength)
{
	OP_ASSERT( aPos >= 0 && aPos <= Length() );
	if (aCStr == NULL)
		return OpStatus::OK;

	if (aLength == KAll)
		aLength = uni_strlen(aCStr);

	int len = Length();

	RETURN_IF_ERROR(Grow(len + aLength));
	op_memmove(iBuffer+aPos+aLength, iBuffer+aPos, (len-aPos+1)*sizeof(uni_char));	// Always includes terminating NUL
	op_memmove(iBuffer+aPos, aCStr, aLength*sizeof(uni_char));

	return OpStatus::OK;
}

OpString16& OpString16::InsertL(int aPos, const OpStringC8& aString, int aLength)
{
	LEAVE_IF_ERROR(Insert(aPos, aString.CStr(), MIN(aLength, aString.Length())));
	return *this;
}

OpString16& OpString16::AppendL(const char* aCStr, int aLength)
{
	LEAVE_IF_ERROR(Append(aCStr, aLength));
	return *this;
}

OP_STATUS OpString16::Append(const char* aCStr, int aLength)
{
	if (aCStr == NULL)
		return OpStatus::OK;

	if (aLength == KAll)
		aLength = op_strlen(aCStr);

	int len = Length();
	RETURN_IF_ERROR(Grow(len + aLength));
	uni_char* t = iBuffer + len;
	const unsigned char* s = (const unsigned char*)aCStr;
	for (int i=0; i < aLength && *s; ++i)
	{
		*t++ = (uni_char)*s++;
	}
	*t = 0;

	return OpStatus::OK;
}

OpString16& OpString16::AppendL(const uni_char* aCStr, int aLength)
{
	LEAVE_IF_ERROR(Append(aCStr, aLength));
	return *this;
}

OP_STATUS OpString16::Append(const uni_char* aCStr, int aLength)
{
	if (aCStr == NULL)
		return OpStatus::OK;

	if (aLength == KAll)
		aLength = uni_strlen(aCStr);

	int len = Length();
	RETURN_IF_ERROR(Grow(len + aLength));
	uni_char* t = iBuffer + len;
	const uni_char* s = aCStr;
	for (int i=0; i < aLength && *s; ++i)
	{
		*t++ = *s++;
	}
	*t = 0;

	return OpStatus::OK;
}

uni_char* OpString16::Reserve(int aMaxLength)
{
	return (OpStatus::IsSuccess(Grow(aMaxLength))?CStr():NULL);
}

uni_char* OpString16::ReserveL(int aMaxLength)
{
	LEAVE_IF_ERROR(Grow(aMaxLength));
	return CStr();
}

OP_STATUS OpString16::TakeOver(OpString16& aString)
{
	Empty();

	iBuffer = aString.iBuffer;
	iSize = aString.iSize;

	aString.iBuffer = NULL;
	aString.iSize = 0;

	return OpStatus::OK;
}
void OpString16::TakeOver(uni_char *aString)
{
	Empty();
	iBuffer = aString;
	iSize = aString ? uni_strlen(aString) : 0;
}

const uni_char* GetS(const OpStringC16& s)
{
	return s.CStr() ? s.CStr() : UNI_L("");
}

OP_STATUS OpString16::SetConcat(
	const OpStringC16& str1, const OpStringC16& str2,
	const OpStringC16& str3, const OpStringC16& str4)
{
	if (iBuffer)
		*iBuffer = '\0';
	return AppendFormat(UNI_L("%s%s%s%s"), GetS(str1), GetS(str2), GetS(str3), GetS(str4));
}

OpString16& OpString16::Delete(int aPos,int aLength)
{
	OP_ASSERT((int)aPos <= Length());
	if (aLength == KAll)
		aLength = Length() - aPos;
	OP_ASSERT((int)aPos + aLength <= Length());
	if (iBuffer && aLength > 0)
	    op_memmove(iBuffer + aPos, iBuffer + aPos + aLength,
				   (Length() - aPos - aLength + 1) * sizeof(uni_char));
	return *this;
}

OpString16& OpString16::Strip(BOOL aLeading, BOOL aTrailing)
{
	if (iBuffer)
	{
		if (aLeading)
		{
			uni_char *p = iBuffer;
			while (uni_isspace(*p)) ++p;
			if (p > iBuffer)
			{
				op_memmove(iBuffer, p, sizeof(uni_char) * (uni_strlen(p) + 1));
			}
		}
		if (aTrailing)
		{
			int len = uni_strlen(iBuffer);
			if (len)
			{
				uni_char *p = iBuffer + len - 1;
				while (p >= iBuffer && uni_isspace(*p)) *(p --) = 0;
			}
		}
	}
	return *this;
}

OpString16 OpStringC16::SubStringL(unsigned int aPos, int aLength) const
{
	int my_length = Length();
	OP_ASSERT(0 <= my_length && aPos <= (unsigned int)my_length);
	if (aLength==KAll || (int)aPos + aLength > my_length)
		aLength = my_length - aPos;
	OP_ASSERT(0 <= (int)aPos+aLength && (int)aPos+aLength <= my_length);
	OpString16 r;
	r.SetL(&iBuffer[aPos], aLength);
	return r;
}

OpString16 OpStringC16::SubString(unsigned int aPos, int aLength, OP_STATUS *err) const
{
	int my_length = Length();
	OP_ASSERT(0 <= my_length && aPos <= (unsigned int)my_length);
	if (aLength==KAll || (int)aPos + aLength > my_length)
		aLength = my_length - aPos;
	OP_ASSERT(0 <= (int)aPos+aLength && (int)aPos+aLength <= my_length);
	OpString16 s;
	OP_STATUS e = s.Set(&iBuffer[aPos], aLength);
	if (err)
		*err = e;
	return s;
}

void OpString16::Wipe()
{
	if(iBuffer)
	{
		op_memset(iBuffer, 0, iSize * sizeof(uni_char));
	}
}

// private

OP_STATUS OpString16::Grow(int aLength)
{
	if (iBuffer && iSize >= aLength)
		return OpStatus::OK;

	// Round size up to nearest quantum
	size_t newSize = aLength + 1; // add NUL
	newSize = ROUND_UP_TO_NEXT_MALLOC_QUANTUM(newSize * 2) / 2;

	uni_char *newBuffer = OP_NEWA(uni_char, newSize);
	if (!newBuffer)
		return OpStatus::ERR_NO_MEMORY;

	if (iBuffer)
	{
		uni_strncpy(newBuffer, iBuffer, iSize);
		OP_DELETEA(iBuffer);
	}
	iBuffer = newBuffer;
	iBuffer[iSize] = 0;
	iSize = newSize - 1;

	return OpStatus::OK;
}

void OpString16::Empty()
{
	if (iBuffer)
		OP_DELETEA(iBuffer);

	iBuffer = NULL;
	iSize = 0;
}

void OpString16::MakeLower()
{
	if (iBuffer)
		uni_strlwr(iBuffer);
}

void OpString16::MakeUpper()
{
	if (iBuffer)
		uni_strupr(iBuffer);
}

OP_STATUS
OpString16::AppendFormat(const uni_char* format, ...)
{
	va_list args;
	va_start(args, format);
	OP_STATUS result = AppendVFormat(format, args);
	va_end(args);
	return result;
}

OP_STATUS
OpString16::AppendVFormat(const uni_char* format, va_list args)
{
	OpPrintf printer(OpPrintf::PRINTF_UNICODE);
#ifdef va_copy
	// One cannot assume (according to manual) that args will
	// remain unchanged after the call to OpPrintf::Format()

	va_list args_copy;
	va_copy(args_copy, args);

	int len = printer.Format(NULL, 0, format, args_copy);

	va_end(args_copy);
#else // va_copy
	int len = printer.Format(NULL, 0, format, args);
#endif // va_copy
	if (len < 0)
		return OpStatus::ERR;
	int prior_length = Length();

	RETURN_IF_ERROR(Grow(prior_length + len));
	printer.Format(iBuffer + prior_length, len + 1, format, args);
	return OpStatus::OK;
}

OP_STATUS
OpString16::AppendFormat(const char* format, ...)
{
	OpString16 fmt;
	RETURN_IF_ERROR(fmt.SetFromUTF8(format));
	va_list args;
	va_start(args, format);
	OP_STATUS result = AppendVFormat(fmt.CStr(), args);
	va_end(args);
	return result;
}

OP_STATUS
OpString16::AppendVFormat(const char* format, va_list args)
{
	OpString16 fmt;
	RETURN_IF_ERROR(fmt.SetFromUTF8(format));
	return AppendVFormat(fmt.CStr(), args);
}

OP_STATUS
OpString16::ReplaceAll(const uni_char* needle, const uni_char* subject, int number_of_occurrences)
{
	if (!needle || !*needle)
		return OpStatus::OK;

	int needle_position = Find(needle);
	if (needle_position != KNotFound && number_of_occurrences != 0)
	{
		int current_index = 0, needle_length = uni_strlen(needle);
		TempBuffer str_temp;//temp buffer is more efficient when Appending, because it caches the length
		do
		{
			RETURN_IF_ERROR(str_temp.Append(CStr() + current_index, needle_position - current_index));
			RETURN_IF_ERROR(str_temp.Append(subject));

			current_index = needle_position + needle_length;
			number_of_occurrences--;
			needle_position = Find(needle, current_index);
		}
		while (needle_position != KNotFound && number_of_occurrences != 0);

		RETURN_IF_ERROR(str_temp.Append(CStr() + current_index));

		RETURN_IF_ERROR(Set(str_temp.GetStorage(), str_temp.Length()));
	}
	return OpStatus::OK;
}

template <>
void op_swap(OpString8& x, OpString8& y)
{
	OpString8 tmp;
	tmp.TakeOver(x);
	x.TakeOver(y);
	y.TakeOver(tmp);
}

template <>
void op_swap(OpString16& x, OpString16& y)
{
	OpString16 tmp;
	tmp.TakeOver(x);
	x.TakeOver(y);
	y.TakeOver(tmp);
}
