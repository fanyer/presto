/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/util/LanguageCodes.h"

//#define SYNTHESIZE_Q_VALUE

const uni_char* GetSystemAcceptLanguage()
{
	static OpString languageCodes;
	bool first = true;

	if (languageCodes.HasContent())
	{
		return languageCodes.CStr();
	}
		//CFPreferencesAppSynchronize(CFSTR("Apple Global Domain"));	// this crashed at least once for me! <ed>
	CFPropertyListRef appleLanguages = CFPreferencesCopyAppValue(CFSTR("AppleLanguages"), CFSTR("Apple Global Domain")); //kCFPreferencesAnyApplication);
	if (appleLanguages)
	{
		if (CFGetTypeID(appleLanguages) == CFArrayGetTypeID())
		{
			CFArrayRef languages = (CFArrayRef) appleLanguages;
			CFIndex languageCount = CFArrayGetCount(languages);
			bool yankish_added = false;

			for(CFIndex languageIndex = 0; languageIndex < languageCount; languageIndex++)
			{
				CFPropertyListRef value = (CFPropertyListRef) CFArrayGetValueAtIndex(languages, languageIndex);
				if(CFGetTypeID(value) == CFStringGetTypeID())
				{
					CFStringRef stringValue = (CFStringRef) value;
					CFIndex stringLength = CFStringGetLength(stringValue);
					UniChar localValue[1024];
					CFStringGetCharacters(stringValue, CFRangeMake(0, stringLength), localValue);
					localValue[stringLength] = 0;
					const uni_char *isoLanguageCode = ConvertAppleLanguageNameToCode((uni_char *) localValue);

					if(isoLanguageCode)
					{
						bool is_yankish = uni_str_eq(isoLanguageCode, UNI_L("en-US"));
						bool is_english = (uni_strncmp(isoLanguageCode, UNI_L("en"), 2) == 0);
						if(!first)
						{
							if (!is_yankish || !yankish_added)
							{
								languageCodes.Append(UNI_L(","));
								languageCodes.Append(isoLanguageCode);
							}
						} else {
							first = false;
							languageCodes.Set(isoLanguageCode);
						}
						yankish_added |= is_yankish;
						if (is_english && !yankish_added)
						{
							languageCodes.Append(",en-US");
							yankish_added = TRUE;
						}
					}
				}
			}
		}
		CFRelease(appleLanguages);
	}
	return languageCodes.CStr();
}

