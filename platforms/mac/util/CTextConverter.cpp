/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/util/CTextConverter.h"
#include "platforms/mac/util/macutils.h"
#include "platforms/mac/util/MachOCompatibility.h"

#define _UNICODE_UTF16_

CTextConverter *gTextConverter = NULL;

CTextConverter::CTextConverter()
{
	mMacToUTF16		= NULL;
	mMacToUTF8		= NULL;
	mMacToLatin1	= NULL;
	mUTF16ToMac		= NULL;
	mUTF8ToMac		= NULL;
	mLatin1ToMac	= NULL;

	mEncodingUTF16	= 0;
	mEncodingUTF8	= 0;
	mEncodingLatin1	= 0;
	mEncodingMac	= 0;

	mConversionError = false;

	if (NULL != ::TECGetInfo)
	{
		mEncodingUTF16	= ::CreateTextEncoding(kTextEncodingUnicodeDefault, kTextEncodingDefaultVariant, kUnicode16BitFormat);
		mEncodingUTF8	= ::CreateTextEncoding(kTextEncodingUnicodeDefault, kTextEncodingDefaultVariant, kUnicodeUTF8Format);
		mEncodingLatin1	= ::CreateTextEncoding(kTextEncodingWindowsLatin1, kTextEncodingDefaultVariant, kTextEncodingDefaultFormat);
		mEncodingMac = GetApplicationTextEncoding();

		::TECCreateConverter(&mMacToUTF16,	mEncodingMac,		mEncodingUTF16);
		::TECCreateConverter(&mMacToUTF8,	mEncodingMac,		mEncodingUTF8);
		::TECCreateConverter(&mMacToLatin1,	mEncodingMac,		mEncodingLatin1);
		::TECCreateConverter(&mUTF16ToMac,	mEncodingUTF16,		mEncodingMac);
		::TECCreateConverter(&mUTF8ToMac,	mEncodingUTF8,		mEncodingMac);
		::TECCreateConverter(&mLatin1ToMac,	mEncodingLatin1,	mEncodingMac);
	}
}

CTextConverter::~CTextConverter()
{
	if (NULL != ::TECGetInfo)
	{
		TECDisposeConverter(mMacToUTF16);
		TECDisposeConverter(mMacToUTF8);
		TECDisposeConverter(mMacToLatin1);
		TECDisposeConverter(mUTF16ToMac);
		TECDisposeConverter(mUTF8ToMac);
		TECDisposeConverter(mLatin1ToMac);
	}
}

UInt32 CTextConverter::ConvertStringToMacC(const void* inSrcStr, char* ioDestString, UInt32 inDestLen, TextCoverterOptions inOptions, StringEncoding inSrcType)
{
	UInt32 srcLen;
	UInt32 newLen;
	switch (inSrcType) {
		case kStringEncodingUniChar:
#if defined(_UNICODE_UTF16_)
			srcLen = uni_strlen((uni_char*)inSrcStr);
#else
			srcLen = strlen((const char*) inSrcStr);
#endif
			break;
		case kStringEncodingUTF16:
			srcLen = uni_strlen((uni_char*)inSrcStr);
			break;
		case kStringEncodingUTF8:
		case kStringEncodingLatin1:
			srcLen = strlen((const char*) inSrcStr);
			break;
	}
	newLen = ConvertBufferToMac(inSrcStr, srcLen, ioDestString, inDestLen - 1, inOptions, inSrcType);
	ioDestString[newLen] = '\0';
	return newLen;
}

UInt32 CTextConverter::ConvertStringToMacP(const void* inSrcStr, Str255 ioDestString, TextCoverterOptions inOptions, StringEncoding inSrcType)
{
	UInt32 srcLen;
	switch (inSrcType) {
		case kStringEncodingUniChar:
#if defined(_UNICODE_UTF16_)
			srcLen = uni_strlen((uni_char*)inSrcStr);
#else
			srcLen = strlen((const char*) inSrcStr);
#endif
			break;
		case kStringEncodingUTF16:
			srcLen = uni_strlen((uni_char*)inSrcStr);
			break;
		case kStringEncodingUTF8:
		case kStringEncodingLatin1:
			srcLen = strlen((const char*) inSrcStr);
			break;
	}
	ioDestString[0] = ConvertBufferToMac(inSrcStr, srcLen, (char*)&ioDestString[1], 255, inOptions, inSrcType);
	return ioDestString[0];
}

UInt32 CTextConverter::ConvertBufferToMac(const void* inSrcStr, UInt32 inSrcLen, char* ioDestString, UInt32 inDestLen, TextCoverterOptions inOptions, StringEncoding inSrcType)
{
	TECObjectRef converter = 0;
	ByteCount realSrcLen;
	ByteCount realDestLen;
	switch (inSrcType) {
		case kStringEncodingUniChar:
#if defined(_UNICODE_UTF16_)
			converter = mUTF16ToMac;
			inSrcLen *= 2;
#elif defined(_UNICODE_UTF8_)
			converter = mUTF8ToMac;
#else
			converter = mLatin1ToMac;
#endif
			break;
		case kStringEncodingUTF16:
			converter = mUTF16ToMac;
			inSrcLen *= 2;
			break;
		case kStringEncodingUTF8:
			converter = mUTF8ToMac;
			break;
		case kStringEncodingLatin1:
			converter = mLatin1ToMac;
			break;
	}
	if (converter)
	{
		OSStatus err = TECConvertText(converter, (ConstTextPtr)inSrcStr, inSrcLen, &realSrcLen, (TextPtr)ioDestString, inDestLen, &realDestLen);
		mConversionError = (err != noErr);
		return CleanupMacText(ioDestString, realDestLen, inOptions);
	}
	mConversionError = true;
	return 0;
}

UInt32 CTextConverter::ConvertStringFromMacC(const char* inSrcStr, void* ioDestString, UInt32 inDestLen, TextCoverterOptions inOptions, StringEncoding inDestType)
{
	UInt32 destLen = ConvertBufferFromMac(inSrcStr, strlen(inSrcStr), ioDestString, inDestLen - 1, inOptions, inDestType);
	if (kStringEncodingUTF16 == inDestType
#if defined(_UNICODE_UTF16_)
			|| kStringEncodingUniChar == inDestType
#endif
	) {
		((short*)ioDestString)[destLen] = 0;
	} else {
		((char*)ioDestString)[destLen] = '\0';
	}
	return destLen;
}

UInt32 CTextConverter::ConvertStringFromMacP(ConstStr255Param inSrcStr, void* ioDestString,  UInt32 inDestLen, TextCoverterOptions inOptions, StringEncoding inDestType)
{
	UInt32 destLen = ConvertBufferFromMac((const char*)&inSrcStr[1], inSrcStr[0], ioDestString, inDestLen - 1, inOptions, inDestType);
	if (kStringEncodingUTF16 == inDestType
#if defined(_UNICODE_UTF16_)
			|| kStringEncodingUniChar == inDestType
#endif
	) {
		((short*)ioDestString)[destLen] = 0;
	} else {
		((char*)ioDestString)[destLen] = '\0';
	}
	return destLen;
}

UInt32 CTextConverter::ConvertBufferFromMac(const char* inSrcStr, UInt32 inSrcLen, void* ioDestString, UInt32 inDestLen, TextCoverterOptions inOptions, StringEncoding inDestType)
{
	TECObjectRef converter = 0;
	ByteCount realSrcLen;
	ByteCount realDestLen;
	switch (inDestType) {
		case kStringEncodingUniChar:
#if defined(_UNICODE_UTF16_)
			converter = mMacToUTF16;
			inDestLen *= 2;
#elif defined(_UNICODE_UTF8_)
			converter = mMacToUTF8;
#else
			converter = mMacToLatin1;
#endif
			break;
		case kStringEncodingUTF16:
			converter = mMacToUTF16;
			inDestLen *= 2;
			break;
		case kStringEncodingUTF8:
			converter = mMacToUTF8;
			break;
		case kStringEncodingLatin1:
			converter = mMacToLatin1;
			break;
	}
	if (converter)
	{
		OSStatus err = TECConvertText(converter, (ConstTextPtr)inSrcStr, inSrcLen, &realSrcLen, (TextPtr)ioDestString, inDestLen, &realDestLen);
		mConversionError = (err != noErr);
		if (kStringEncodingUTF16 == inDestType
#if defined(_UNICODE_UTF16_)
				|| kStringEncodingUniChar == inDestType
#endif
		) {
			realDestLen = realDestLen / 2;
		}
		return realDestLen; //Error! Do text cleanup!
//		return CleanupMacText(ioDestString, realDestLen, inOptions);
	}
	mConversionError = true;
	return 0;
}
Boolean CTextConverter::GotConversionError()
{
	return mConversionError;
}

UInt32 CTextConverter::CleanupMacText(char* ioStr, UInt32 inLen, TextCoverterOptions inOptions)
{
	char *reader = ioStr;
	char *writer = ioStr;
	char c;
	Boolean lastCharSpace = false;
	Boolean isSpace = false;
	while (reader < ioStr+inLen) {
		c = *reader++;
		isSpace = false;
		if (c == 0) {
			break;
		}
		if (0x0D == c && (reader < ioStr+inLen) && 0x0A == *reader && !(inOptions & kTextCoverterOptionLinebreaksToSpace)) {
			reader++;
			isSpace = true;
		}
		else if (0x0D == c || 0x0A == c)
		{
			if (inOptions & kTextCoverterOptionLinebreaksToSpace) {
				c = ' ';
			} else {
				c = 0x0D;
			}
			isSpace = true;
		}
		else if (c == (char)202) { //kNonBreakingSpaceCharCode
			if (inOptions & kTextCoverterOptionNBSPToSpace) {
				c = ' ';
			}
		}
		else if (c == 0x09) {
			if (inOptions & kTextCoverterOptionTabToSpace) {
				c = ' ';
			}
			isSpace = true;
		}
		else if (c == ' ') {
			isSpace = true;
		}
		if (c == ' ' && (inOptions & kTextCoverterOptionCollapseWhiteSpace) && lastCharSpace) {
			continue;
		}
		if (c < 32 && c != 9 && c != 13 && (inOptions & kTextCoverterOptionRemoveControlChars)) {
			continue;
		}
		*writer++ = c;
		lastCharSpace = isSpace;

	}
	return (writer - ioStr);
}

