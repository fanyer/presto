/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/util/WidgetFirstRunOperations.h"
#include "platforms/mac/util/LanguageCodes.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"

#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/hotlist/hotlistparser.h"
#include "platforms/mac/util/macutils.h"

WidgetFirstRunOperations::WidgetFirstRunOperations()
{
}

WidgetFirstRunOperations::~WidgetFirstRunOperations()
{
}

void WidgetFirstRunOperations::loadSystemLanguagePreferences()
{
	OpString languageCodes;
	bool first = true;

	CFPropertyListRef appleLanguages = CFPreferencesCopyAppValue(CFSTR("AppleLanguages"), kCFPreferencesAnyApplication);
	if (appleLanguages)
	{
		if (CFGetTypeID(appleLanguages) == CFArrayGetTypeID())
		{
			CFArrayRef languages = (CFArrayRef) appleLanguages;
			CFIndex languageCount = CFArrayGetCount(languages);

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
						if(!first)
						{
							if(!uni_strstr(languageCodes.CStr(), isoLanguageCode))
							{
								languageCodes.Append(UNI_L(","));
								languageCodes.Append(isoLanguageCode);
							}
						} else {
							first = false;
							languageCodes.Set(isoLanguageCode);
						}
					}
				}
			}

			if(languageCodes.Length() > 0)
			{
				TRAPD(rc, g_pcnet->WriteStringL(PrefsCollectionNetwork::AcceptLanguage, languageCodes));
			}
		}
		CFRelease(appleLanguages);
	}
}

void WidgetFirstRunOperations::setDefaultSystemFonts()
{
	const char *acceptType = g_pcnet->GetAcceptLanguage();

	if(!strnicmp(acceptType, "ja", 2))
	{
		TRAPD(rc, g_pcdisplay->WriteStringL(PrefsCollectionDisplay::DefaultEncoding, UNI_L("shift_jis")));
	} else if(!strnicmp(acceptType, "zh_CN", 5)) {
		TRAPD(rc, g_pcdisplay->WriteStringL(PrefsCollectionDisplay::DefaultEncoding, UNI_L("gb2312")));
	} else if(!strnicmp(acceptType, "zh_TW", 5)) {
		TRAPD(rc, g_pcdisplay->WriteStringL(PrefsCollectionDisplay::DefaultEncoding, UNI_L("big5")));
	} else if(!strnicmp(acceptType, "ko", 5)) {
		TRAPD(rc, g_pcdisplay->WriteStringL(PrefsCollectionDisplay::DefaultEncoding, UNI_L("euc-ko")));
	}
}

void WidgetFirstRunOperations::importSafariBookmarks()
{
	OpString filename;

	if(g_hotlist_manager)
	{
		MacGetBookmarkFileLocation(HotlistModel::KonquerorBookmark, filename);
		if(filename.Length() > 0)
		{
			HotlistParser parser;
			HotlistModel* bookmarksModel = g_hotlist_manager->GetBookmarksModel();

			if(bookmarksModel)
			{
				if( parser.Parse( filename, *bookmarksModel, -1, HotlistModel::KonquerorBookmark, TRUE ) )
				{
					bookmarksModel->SetDirty(TRUE);
				}
			}
		}
	}
}
