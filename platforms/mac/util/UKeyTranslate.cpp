/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/util/UKeyTranslate.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "platforms/mac/util/CTextConverter.h"
#include "platforms/mac/util/MachOCompatibility.h"

OP_STATUS UKeyTranslate::GetUnicharFromVirtualKey(UInt32 virtualKeyCode, uni_char &outUniChar, UInt8 modifierKeyState)
{
#ifdef SIXTY_FOUR_BIT
	UInt32 deadKeyState = 0;
	OP_STATUS result = OpStatus::ERR;
	TISInputSourceRef kbInputSourceRef = TISCopyCurrentKeyboardLayoutInputSource();
	CFDataRef uchrDataRef = (CFDataRef)TISGetInputSourceProperty(kbInputSourceRef, kTISPropertyUnicodeKeyLayoutData);
	Boolean existsUchr = ((uchrDataRef != NULL) && (CFDataGetBytePtr(uchrDataRef) != NULL) && (CFDataGetLength(uchrDataRef) != 0));
	if (existsUchr)
	{
		UniCharCount actualLength = 0;
		UniChar outChar[2] = {0,0};

		OSStatus status = UCKeyTranslate((const UCKeyboardLayout *)CFDataGetBytePtr(uchrDataRef),
										 virtualKeyCode, kUCKeyActionDown,
										 modifierKeyState, LMGetKbdType(), kNilOptions,
										 &deadKeyState, 2, &actualLength, outChar);
		if (status == noErr && actualLength && outChar[0] != kFunctionKeyCharCode)
		{
			outUniChar = outChar[0];
			result = OpStatus::OK;
		}
	}
	CFRelease(kbInputSourceRef);

	return result;
#else	// !SIXTY_FOUR_BIT
	static KeyboardLayoutRef lastKbdLayout = 0;
	static const UCKeyboardLayout* ucharData = NULL;
	static const void* charData = NULL;
	KeyboardLayoutRef currentKbdLayout = 0;

	if (noErr != KLGetCurrentKeyboardLayout(&currentKbdLayout) || !currentKbdLayout)
	{
		return OpStatus::ERR;
	}

	short	keyCode;
	OSStatus error = noErr;
	UInt32 deadKeyState = 0;
	OP_STATUS result = OpStatus::ERR;

	keyCode = virtualKeyCode;

	if (!ucharData || (currentKbdLayout != lastKbdLayout))
	{
		// Don't fetch this unless we have to: Because of the KeyScript issue handled below this may in some cases return 0
		// KeyScript is an EXPENSIVE call, so by caching ucharData as long as possible we minimise the number of times
		// we need to call it.
		error = KLGetKeyboardLayoutProperty(currentKbdLayout, kKLuchrData, (const void**)&ucharData);
	}
	if (!ucharData)
	{
		static Boolean try_again = true;
		if (try_again && (smRoman == GetScriptManagerVariable(smKeyScript)))
		{
			// This is required for roman scripts in order to get something from KLGetCurrentKeyboardLayout
			KeyScript(smRoman | smKeyForceKeyScriptMask);
			error = KLGetKeyboardLayoutProperty(currentKbdLayout, kKLuchrData, (const void**)&ucharData);
			if (error || !ucharData)
				try_again = false;
		}
	}

	if ((error == noErr) && (ucharData != 0))
	{
		UniCharCount actualLength = 0;
		UniChar outChar[2] = {0,0};
		charData = NULL;
		error = UCKeyTranslate(ucharData, (unsigned short)keyCode,
									kUCKeyActionDown, modifierKeyState, LMGetKbdType(), 0,
									&deadKeyState, 2, &actualLength, outChar);
		if (error == noErr && actualLength && outChar[0] != kFunctionKeyCharCode)
		{
			outUniChar = outChar[0];
			result = OpStatus::OK;
#ifdef DEBUG_UNI_KEYSTROKES
//			if (outUniChar & 0xff00)
//				fprintf(stderr, "UKC:%x\n", outChar[0]);
#endif
		}
#ifdef DEBUG_UNI_KEYSTROKES
		else
		{
			fprintf(stderr, "UKCe:%li-%x-%lu-%x\n", error, outChar[0], virtualKeyCode, modifierKeyState);
		}
		if (actualLength != 1)
			fprintf(stderr, "UKCl:%lu-%x-%x-%lu-%x\n", actualLength, outChar[0], outChar[1], virtualKeyCode, modifierKeyState);
#endif
	}
	else
	{
#ifdef DEBUG_UNI_KEYSTROKES
		fprintf(stderr, "KLP:%li\n", error);
#endif
		error = noErr;
		if (!charData || (currentKbdLayout != lastKbdLayout))
		{
			error = KLGetKeyboardLayoutProperty(currentKbdLayout, kKLKCHRData, &charData);
		}
		if ((error == noErr) && (charData != 0))
		{
			unsigned long charcs = KeyTranslate(charData, (keyCode & 0xFF) | (modifierKeyState << 8), &deadKeyState);
			if (charcs & 0xFF)
			{
				char src = charcs & 0x00FF;
				gTextConverter->ConvertBufferFromMac(&src, 1, &outUniChar, 1);
				result = OpStatus::OK;
			}
#ifdef DEBUG_UNI_KEYSTROKES
			else
			{
				fprintf(stderr, "KTe\n", outUniChar);
			}
			if (charcs & 0xFF0000)
			{
				fprintf(stderr, "KTe:%x-%lu-%x\n", charcs, virtualKeyCode, modifierKeyState);
			}
#endif
		}
		else
		{
#ifdef DEBUG_UNI_KEYSTROKES
			fprintf(stderr, "KLPe:%li\n", error);
#endif
		}
	}
	lastKbdLayout = currentKbdLayout;
	return result;
#endif	// SIXTY_FOUR_BIT
}
