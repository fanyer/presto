/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/util/opstring.h"
#include "modules/stdlib/src/thirdparty_printf/uni_printf.h"

#define NewChar(N)     new char[N]
#define NewUnichar(N)  new uni_char[N]
#define DeleteArray(N) delete [] N

/** Size in bytes of work buffer for set from/to encoding */
#define OPSTRING_ENCODE_WORKBUFFER_SIZE 1024

#if !defined(va_copy) && defined(__va_copy)
# define va_copy __va_copy
#endif // !va_copy && __va_copy

/** Grow() will round its requested size up to a multiple of this #bytes
 *
 * This should be at least as large as the quantum size/alignment of the
 * underlying allocator (otherwise, we're just hammering the allocator
 * unnecessarily). However, if it's too large, we start wasting memory.
 */
#ifdef MEMORY_ALIGNMENT
#define OPSTRING_GROW_QUANTUM ((size_t) MAX(MEMORY_ALIGNMENT, 16))
#else // MEMORY_ALIGNMENT
#define OPSTRING_GROW_QUANTUM ((size_t) 16)
#endif // MEMORY_ALIGNMENT
#define OPSTRING_GROW_QUANTUM_MASK (OPSTRING_GROW_QUANTUM - 1)

// OpStringC8 class

int OpStringC8::Compare(const char* aCStr,int aLength) const
{
    if ((aCStr==NULL) || (*aCStr == '\0'))
        return (((iBuffer == NULL) || (*iBuffer == '\0')) ? 0:1);
    if (iBuffer==NULL)
        return -1;
	return aLength==KAll ? op_strcmp(iBuffer,aCStr) : op_strncmp(iBuffer,aCStr,aLength);
}

int OpStringC8::CompareI(const char* aCStr,int aLength) const
{
    if ((aCStr==NULL) || (*aCStr == '\0'))
        return (((iBuffer == NULL) || (*iBuffer == '\0')) ? 0:1);
    if (iBuffer==NULL)
        return -1;
    return aLength==KAll ? op_stricmp(iBuffer,aCStr) : op_strnicmp(iBuffer,aCStr,aLength);
}

int OpStringC8::Find(const char* aCStr) const
{
	if (aCStr && iBuffer)
	{
		const char* s = op_strstr(iBuffer, aCStr);
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

	int length = op_vsnprintf(NULL, 0, format, args_copy)+1; // The function returns the length without the null so we need to add 1

	va_end(args_copy);
#else // va_copy
	int length = op_vsnprintf(NULL, 0, format, args)+1; // The function returns the length without the null so we need to add 1
#endif // va_copy

	if (length <= 0)
		return OpStatus::ERR;

	char *buf = NewChar(length);
    if (buf == NULL)
        return OpStatus::ERR_NO_MEMORY;

	op_vsnprintf(buf, length, format, args);

	if (!iBuffer)
	{
		iBuffer = buf;
		iSize = length-1;
		return OpStatus::OK;
	}

	OP_STATUS result = Append(buf);
	DeleteArray(buf);
	return result;
}

// OpString8 class

OpString8::OpString8(const OpString8& aString)
  : OpStringC8()
{
#ifdef MEMTEST
    OOM_REPORT_UNANCHORED_OBJECT();
#endif // MEMTEST
	OpString8& aMutableString = (OpString8&)aString;
	iBuffer = aMutableString.iBuffer;
	aMutableString.iBuffer = NULL;
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
    OP_ASSERT(aPos<=Length());
    if (aLength==KAll)
        aLength=Length()-aPos;
    OP_ASSERT(aPos+aLength<=Length());
	if (iBuffer && aLength > 0)
		op_memmove(iBuffer+aPos,iBuffer+aPos+aLength,Length()-(aPos+aLength)+1);
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

OpString8 OpStringC8::SubString(int aPos, int aLength, OP_STATUS *err) const
{
    OP_ASSERT(aPos<=Length());
    if (aLength==KAll)
        aLength=Length()-aPos;
    OP_ASSERT(aPos+aLength<=Length());
	OpString8 s;
	OP_STATUS e = s.Set(&iBuffer[aPos],aLength);
	if (err) *err = e;
	return s;
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
	RETURN_IF_ERROR(Grow(aLength));
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
    if (aLength==KAll)
        aLength = uni_strlen(aCStr);
	RETURN_IF_ERROR(Grow(aLength));
    const uni_char* s = aCStr;
    char* t = iBuffer;
	for (int i=0; i<aLength && *s; i++)
        *t++ = (char) *s++;
	*t = '\0';
	return OpStatus::OK;
}

OpString8& OpString8::SetL(const OpStringC8& aString)
{
	LEAVE_IF_ERROR(Set(aString.CStr(), KAll));
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
    return (OpStatus::IsSuccess(Grow(aMaxLength))?CStr():NULL);
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

OP_STATUS OpString8::SetConcat(const OpStringC8& str1, const OpStringC8& str2, const OpStringC8& str3,
                               const OpStringC8& str4)
{
	if (iBuffer)
		*iBuffer = '\0';
	return AppendFormat("%s%s%s%s", GetS(str1), GetS(str2), GetS(str3), GetS(str4));
}

OP_STATUS OpString8::SetConcat(const OpStringC8& str1, const OpStringC8& str2, const OpStringC8& str3,
                               const OpStringC8& str4, const OpStringC8& str5, const OpStringC8& str6,
                               const OpStringC8& str7, const OpStringC8& str8)
{
	if (iBuffer)
		*iBuffer = '\0';
	return AppendFormat("%s%s%s%s%s%s%s%s", GetS(str1), GetS(str2), GetS(str3), GetS(str4),
											GetS(str5), GetS(str6), GetS(str7), GetS(str8));
}

void OpString8::Wipe()
{
	if(iBuffer)
	{
		op_memset(iBuffer, 0, iSize);
	}
    iSize = 0;
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
	newSize = (newSize + OPSTRING_GROW_QUANTUM_MASK) & ~OPSTRING_GROW_QUANTUM_MASK;

	char *newBuffer = NewChar(newSize);
	if (!newBuffer)
		return OpStatus::ERR_NO_MEMORY;

	if (iBuffer)
	{
		op_strncpy(newBuffer, iBuffer, iSize);
		DeleteArray(iBuffer);
	}
	iBuffer = newBuffer;
	iBuffer[iSize] = 0;
	iSize = newSize - 1;

    return OpStatus::OK;
}

void OpString8::Empty()
{
	if (iBuffer)
        DeleteArray(iBuffer);

    iBuffer = NULL;
	iSize = 0;
}

// OpStringC16 class

uni_char OpStringC16::operator[](int aIndex) const
{
    OP_ASSERT(0<=aIndex && aIndex<(int)(Length()+1));
    return iBuffer[aIndex];
}

int OpStringC16::Length() const
{
    if (iBuffer==NULL)
        return 0;
    return uni_strlen(iBuffer);
}

int OpStringC16::Length(const char* aCStr)
{
    if (aCStr==NULL)
        return 0;
    return op_strlen(aCStr);
}

int OpStringC16::Length(const uni_char* aCStr)
{
    if (aCStr==NULL)
        return 0;
    return uni_strlen(aCStr);
}

int OpStringC16::Compare(const char* aCStr, int aLength) const
{
    if ((aCStr==NULL) || (*aCStr == '\0'))
        return (((iBuffer == NULL) || (*iBuffer == '\0')) ? 0:1);
    if (iBuffer==NULL)
        return -1;
	return aLength==KAll ? uni_strcmp(iBuffer, aCStr) : uni_strncmp(iBuffer, aCStr, aLength);
}

int OpStringC16::Compare(const uni_char* aCStr, int aLength) const
{
    if ((aCStr==NULL) || (*aCStr == '\0'))
        return (((iBuffer == NULL) || (*iBuffer == '\0')) ? 0:1);
    if (iBuffer==NULL)
        return -1;
	return aLength==KAll ? uni_strcmp(iBuffer,aCStr) : uni_strncmp(iBuffer,aCStr,aLength);
}

int OpStringC16::CompareI(const char* aCStr, int aLength) const
{
    if ((aCStr==NULL) || (*aCStr == '\0'))
        return (((iBuffer == NULL) || (*iBuffer == '\0')) ? 0:1);
    if (iBuffer==NULL)
        return -1;
	return aLength==KAll ? uni_stricmp(iBuffer, aCStr) : uni_strnicmp(iBuffer, aCStr, aLength);
}

int OpStringC16::CompareI(const uni_char* aCStr, int aLength) const
{
    if ((aCStr==NULL) || (*aCStr == '\0'))
        return (((iBuffer == NULL) || (*iBuffer == '\0')) ? 0:1);
    if (iBuffer==NULL)
        return -1;
    return aLength==KAll ? uni_stricmp(iBuffer,aCStr) : uni_strnicmp(iBuffer,aCStr,aLength);
}

int OpStringC16::Find(const char* aCStr) const
{
	if (aCStr && iBuffer)
	{
		const uni_char* s = uni_strstr(iBuffer, aCStr);
		if (s)
		{
			return s - iBuffer;
		}
	}

	return KNotFound;
}

int OpStringC16::Find(const uni_char* aCStr) const
{
	if (aCStr && iBuffer)
	{
		const uni_char* s = uni_strstr(iBuffer,aCStr);
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

OpString16::OpString16(const OpString16& aString)
	: OpStringC16()
{
#ifdef MEMTEST
    OOM_REPORT_UNANCHORED_OBJECT();
#endif // MEMTEST
    OpString16& aMutableString = (OpString16&)aString;
    iBuffer = aMutableString.iBuffer;
    aMutableString.iBuffer = 0;
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
	RETURN_IF_ERROR(Grow(aLength));
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
    if (aLength==KAll)
        aLength=Length(aCStr);
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

OpString16& OpString16::InsertL(int aPos,const OpStringC8& aString)
{
	LEAVE_IF_ERROR(Insert(aPos, aString.CStr(), KAll));
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

const uni_char* GetS(const OpStringC16& s)
{
	return s.CStr() ? s.CStr() : UNI_L("");
}

OP_STATUS OpString16::SetConcat(const OpStringC16& str1, const OpStringC16& str2, const OpStringC16& str3,
                               const OpStringC16& str4)
{
	if (iBuffer)
		*iBuffer = '\0';
	return AppendFormat(UNI_L("%s%s%s%s"), GetS(str1), GetS(str2), GetS(str3), GetS(str4));
}

OpString16& OpString16::Delete(int aPos,int aLength)
{
    OP_ASSERT(aPos<=Length());
    if (aLength==KAll)
        aLength=Length()-aPos;
    OP_ASSERT(aPos+aLength<=Length());
	if (iBuffer && aLength > 0)
	    op_memmove(iBuffer+aPos,iBuffer+aPos+aLength,(Length()-(aPos+aLength)+1)*sizeof(uni_char));
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

OpString16 OpStringC16::SubStringL(int aPos, int aLength) const
{
    OP_ASSERT(aPos<=Length());
    if (aLength==KAll)
        aLength=Length()-aPos;
    OP_ASSERT(aPos+aLength<=Length());
	OpString16 r;
	r.SetL(&iBuffer[aPos],aLength);
    return r;
}

OpString16 OpStringC16::SubString(int aPos, int aLength, OP_STATUS *err) const
{
    OP_ASSERT(aPos<=Length());
    if (aLength==KAll)
        aLength=Length()-aPos;
    OP_ASSERT(aPos+aLength<=Length());
	OpString16 s;
	OP_STATUS e = s.Set(&iBuffer[aPos],aLength);
	if (err) *err = e;
	return s;
}

void OpString16::Wipe()
{
	if(iBuffer)
	{
		op_memset(iBuffer, 0, iSize * sizeof(uni_char));
	}
    iSize = 0;
}

// private

OP_STATUS OpString16::Grow(int aLength)
{
    if (iBuffer && iSize >= aLength)
        return OpStatus::OK;

	// Round size up to nearest quantum
	size_t newSize = aLength + 1; // add NUL
	newSize = (newSize + (OPSTRING_GROW_QUANTUM_MASK / 2)) & ~(OPSTRING_GROW_QUANTUM_MASK / 2);

	uni_char *newBuffer = NewUnichar(newSize);
	if (!newBuffer)
		return OpStatus::ERR_NO_MEMORY;

	if (iBuffer)
	{
		uni_strncpy(newBuffer, iBuffer, iSize);
		DeleteArray(iBuffer);
	}
	iBuffer = newBuffer;
	iBuffer[iSize] = 0;
	iSize = newSize - 1;

    return OpStatus::OK;
}

void OpString16::Empty()
{
	if (iBuffer)
        DeleteArray(iBuffer);

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
#ifdef va_copy
	// One cannot assume (according to manual) that args will
	// remain unchanged after the call to OpPrintf::Format()

	va_list args_copy;
	va_copy(args_copy, args);

	OpPrintf printer(OpPrintf::PRINTF_UNICODE);
	int len = printer.Format(NULL, 0, format, args_copy);
	int current_length = Length();

	va_end(args_copy);
#else // va_copy
	OpPrintf printer(OpPrintf::PRINTF_UNICODE);
	int len = printer.Format(NULL, 0, format, args);
	int current_length = Length();
#endif // va_copy

	RETURN_IF_ERROR(Grow(current_length + len));

	uni_char* buf = iBuffer + current_length;
    uni_vsnprintf(buf, len+1, format, args);
	return OpStatus::OK;
}
