/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2005-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#include "core/pch.h"

#ifdef M2_SUPPORT
#include "adjunct/desktop_util/prefs/PrefsCollectionM2.h"
#include "modules/prefs/prefsmanager/collections/prefs_macros.h"
#include "adjunct/m2/src/include/enums.h"

#include "adjunct/desktop_util/prefs/PrefsCollectionM2_c.inl"

PrefsCollectionM2 *PrefsCollectionM2::CreateL(PrefsFile *reader)
{
	if (g_opera->prefs_module.m_pcm2)
		LEAVE(OpStatus::ERR);
	g_opera->prefs_module.m_pcm2 = OP_NEW_L(PrefsCollectionM2, (reader));
	return g_opera->prefs_module.m_pcm2;
}

PrefsCollectionM2::~PrefsCollectionM2()
{
#ifdef PREFS_COVERAGE
	CoverageReport(
		m_stringprefdefault, PCM2_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCM2_NUMBEROFINTEGERPREFS);
#endif

	g_opera->prefs_module.m_pcm2 = NULL;
}

void PrefsCollectionM2::ReadAllPrefsL(PrefsModule::PrefsInitInfo *)
{
	// Read everything
	OpPrefsCollection::ReadAllPrefsL(
		m_stringprefdefault, PCM2_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCM2_NUMBEROFINTEGERPREFS);
}

#ifdef PREFS_VALIDATE
void PrefsCollectionM2::CheckConditionsL(int which, int *value, const uni_char *host)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<integerpref>(which))
	{
	case ShowEmailClient:
	case MailViewListSplitter:
	case MailViewMode:
	case ChatRoomSplitter:
	case ShowDeleteMailDialog:
	case AccountInfoSplitter:
	case MailBodyMode:
	case ShowQuickReply:
	case MarkAsReadAutomatically:
	case ShowEncodingMismatchDialog:
	case ShowAttachmentsInline:
	case ShowQuickHeaders:
	case LastDatabaseCheck:
	case DefaultMailSearch:
	case AutoMailPanelToggle:
	case MailTwoLinedItems:
	case DefaultMailSorting:
	case DefaultMailSortingAscending:
	case DefaultMailFlatThreadedView:
	case MailGroupingMethod:
	case FitToWidth:
	case PaddingInComposeWindow:
	case ShowDefaultMailClientDialog:
	case LoadMailDatabasesAsynchronously:
	case SplitUpThreadWhenSubjectChanged:
		break; // Nothing to do.

	case DefaultMailStoreType:
		if (*value <= 0 || *value >= AccountTypes::MBOX_TYPE_COUNT)
			*value = GetDefaultIntegerPref(DefaultMailStoreType);
		break;
	case MessageWidthListOnTop:
	case MessageWidthListOnRight:
		if (*value <=0)
			*value = 0;
		break;
	case MailComposeHeaderDisplay:
		*value &= 0xFFF;
		break;
	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}
}

BOOL PrefsCollectionM2::CheckConditionsL(int which, const OpStringC &invalue,
                                          OpString **outvalue, const uni_char *host)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<stringpref>(which))
	{
	case WebmailAddress:
		break; // Nothing to do.

	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}

	// When FALSE is returned, no OpString is created for outvalue
	return FALSE;
}
#endif // PREFS_VALIDATE

#endif // M2_SUPPORT
