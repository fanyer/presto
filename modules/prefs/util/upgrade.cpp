/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#if defined UPGRADE_SUPPORT && defined PREFS_WRITE

#include "modules/prefs/util/upgrade.h"
#include "modules/url/url_sn.h"
#include "modules/idna/idna.h"
#include "modules/util/opfile/opfile.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/prefs/prefsmanager/collections/pc_m2.h"
#include "modules/prefs/prefsmanager/collections/pc_mswin.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"

/*static*/ int PrefsUpgrade::Upgrade(PrefsFile *reader, int lastversion)
{
	OP_MEMORY_VAR int retval = PREFSUPGRADE_CURRENT;

	OP_ASSERT(lastversion < PREFSUPGRADE_CURRENT);

	if (lastversion < 2)
	{
#if defined PREFS_UPGRADE_UAINI
		// Upgrade ua.ini
		OpFile * OP_MEMORY_VAR uaini = OP_NEW(OpFile, ());
		if (uaini &&
		    OpStatus::IsSuccess(uaini->Construct(UNI_L("ua.ini"),
		                        OPFILE_USERPREFS_FOLDER)))
		{
			TRAPD(rc, UpgradeUAINIL(uaini));
			if (OpStatus::IsMemoryError(rc))
				retval = -1;
		}
		OP_DELETE(uaini);
#endif

#ifdef PREFS_UPGRADE_FROM_CORE1
		// Upgrade settings from core-1 and earlier
		TRAPD(rc, UpgradeCore1L(reader));
		if (OpStatus::IsMemoryError(rc))
		{
			retval = -1;
		}
#endif
	}

	if (lastversion < 3)
	{
#ifdef PREFS_UPGRADE_UNITE
		TRAPD(rc, UpgradeUniteL(reader));
		if (OpStatus::IsMemoryError(rc))
		{
			retval = -1;
		}
#endif
	}

	if (lastversion < 4)
	{
#ifdef PREFS_UPGRADE_LIBSSL_VERSIONS
		TRAPD(rc, UpgradeSSLVersionsL(reader));
		if (OpStatus::IsMemoryError(rc))
		{
			retval = -1;
		}
#endif
	}

	if (lastversion < 5)
	{
#ifdef URL_PER_SITE_PROXY_POLICY
		TRAPD(rc, UpgradeNoProxyServersL());
		if (OpStatus::IsMemoryError(rc))
		{
			retval = -1;
		}
#endif
	}

	if (lastversion < 6)
	{
		TRAPD(rc, UpgradeDoubleNegativesL(reader));
		if (OpStatus::IsMemoryError(rc))
		{
			retval = -1;
		}
	}

	return retval;
}

#ifdef PREFS_UPGRADE_UAINI
/**
 * Upgrade the ua.ini file from Opera 8
 *
 * @param file File object describing the ua.ini file
 */
/*static*/ void PrefsUpgrade::UpgradeUAINIL(OpFileDescriptor *file)
{
	// Check if the file exists, if it doesn't we just ignore it--the user
	// might be upgrading from an older version of Opera, have deleted the
	// file manually or has never used the check for upgrades feature.
	BOOL exists = FALSE;
	if (OpStatus::IsSuccess(file->Exists(exists)) && exists)
	{
		// Import it
		OpStackAutoPtr<PrefsFile> uaini(OP_NEW_L(PrefsFile, (PREFS_INI)));
		uaini->ConstructL();
		uaini->SetFileL(file);
		if (OpStatus::IsSuccess(uaini->LoadAllL()))
		{
			OpStackAutoPtr<PrefsSection>
				ids(uaini->ReadSectionL(UNI_L("Identity")));
			if (ids.get())
			{
				PrefsCollectionNetwork *pcnet = g_pcnet;

				const PrefsEntry *entry = ids->Entries();
				while (entry)
				{
					// The key is the domain name (ASCII form), and the value
					// is the spoof setting
					const uni_char *val = entry->Value();
					const uni_char *domain = entry->Key();
					int identity = val ? uni_atoi(val) : 0;

					if (domain && *domain && identity)
					{
						OpStatus::Ignore(pcnet->OverridePrefL(domain,
							PrefsCollectionNetwork::UABaseId,
							identity, FALSE));
					}

					entry = entry->Suc();
				}
			}
		}
	}
}
#endif // PREFS_UPGRADE_UAINI

#ifdef PREFS_UPGRADE_FROM_CORE1
/**
 * Upgrade settings from core-1 and earlier.
 *
 * @param reader The PrefsFile object used to get data.
 */
/*static*/ void PrefsUpgrade::UpgradeCore1L(PrefsFile *reader)
{
	// Handle settings that are changed or combined ------

	// [User Prefs]
	// Show Images=3 (FIGS_ON)
	// In core-1, the default image state was stored in two booleans.
	BOOL loadimages =
		reader->ReadBoolL(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SUserPrefs),
						  "Load Figures",
						  TRUE);
	BOOL showimages =
		reader->ReadBoolL(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SUserPrefs),
						  "Show Figures",
						  TRUE);

	SHOWIMAGESTATE state =
		showimages ? (loadimages ? FIGS_ON : FIGS_SHOW) : FIGS_OFF;
	if (state != g_pcdoc->GetDefaultIntegerPref(PrefsCollectionDoc::ShowImageState))
	{
		g_pcdoc->WriteIntegerL(PrefsCollectionDoc::ShowImageState, state);
	}

	// [Disk Cache]
	// Docs Expiry=DEFAULT_DOCUMENTS_EXPIRY
	// Imgs Expiry=DEFAULT_IMAGES_EXPIRY
	// Other Expiry=DEFAULT_OTHERS_EXPIRY
	// In core-1, the settings were stored as three integers (days, hours,
	// minutes).
	int docs_expiry =
		ReadMinutesHoursDaysValueL(reader,
		                           OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SDiskCache),
		                           "Docs Exp Days",
		                           "Docs Exp Hours",
		                           "Docs Exp Minutes",
		                           DEFAULT_DOCUMENTS_EXPIRY);
	if (docs_expiry != g_pcnet->GetDefaultIntegerPref(PrefsCollectionNetwork::DocExpiry))
	{
		g_pcnet->WriteIntegerL(PrefsCollectionNetwork::DocExpiry, docs_expiry);
	}

	int imgs_expiry =
		ReadMinutesHoursDaysValueL(reader,
		                           OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SDiskCache),
		                           "Figs Exp Days",
		                           "Figs Exp Hours",
		                           "Figs Exp Minutes",
		                           DEFAULT_IMAGES_EXPIRY);
	if (imgs_expiry != g_pcnet->GetDefaultIntegerPref(PrefsCollectionNetwork::FigsExpiry))
	{
		g_pcnet->WriteIntegerL(PrefsCollectionNetwork::FigsExpiry, imgs_expiry);
	}

	int others_expiry =
		ReadMinutesHoursDaysValueL(reader,
		                           OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SDiskCache),
		                           "Other Exp Days",
		                           "Other Exp Hours",
		                           "Other Exp Minutes",
		                           DEFAULT_OTHERS_EXPIRY);
	if (others_expiry != g_pcnet->GetDefaultIntegerPref(PrefsCollectionNetwork::OtherExpiry))
	{
		g_pcnet->WriteIntegerL(PrefsCollectionNetwork::OtherExpiry, others_expiry);
	}

	// Fonts are now read as a single key, instead of having their own
	// section.
	g_pcfontscolors->ReadFontsOldFormatL();

#ifdef PREFS_HAVE_DESKTOP_UI
	// SDI/MDI settings have been split
	if (g_pcui->GetIntegerPref(PrefsCollectionUI::SDI) == 2)
	{
		g_pcui->WriteIntegerL(PrefsCollectionUI::SDI, 0);
		g_pcui->WriteIntegerL(PrefsCollectionUI::AllowEmptyWorkspace, TRUE);
		g_pcui->WriteIntegerL(PrefsCollectionUI::ClickToMinimize, TRUE);
		g_pcui->WriteIntegerL(PrefsCollectionUI::ShowCloseButtons, FALSE);
		g_pcui->WriteIntegerL(PrefsCollectionUI::ShowWindowMenu, TRUE);
	}
#endif


	// Handle settings that have been renamed ------
	// [User Prefs]
#ifdef PREFS_HAVE_DESKTOP_UI
	// AutomaticSelectMenu => Automatic Select Menu
	if (reader->IsKey(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SUserPrefs), "AutomaticSelectMenu"))
	{
		g_pcdisplay->WriteIntegerL(PrefsCollectionDisplay::AutomaticSelectMenu,
			reader->ReadIntL(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SUserPrefs), "AutomaticSelectMenu"));
	}
#endif

#ifndef NO_SAVE_SUPPORT
	// SaveTxtCharsPerLine => Save As Text Line Length
	if (reader->IsKey(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SUserPrefs), "SaveTxtCharsPerLine"))
	{
		g_pcdoc->WriteIntegerL(PrefsCollectionDoc::TxtCharsPerLine,
			reader->ReadIntL(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SUserPrefs), "SaveTxtCharsPerLine"));
	}
#endif

#ifdef PREFS_HAVE_MSWIN
	// AskClosingDialUpConnections => Ask Closing DialUp Connections
	if (reader->IsKey(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SUserPrefs), "AskClosingDialUpConnections"))
	{
		g_pcmswin->WriteIntegerL(PrefsCollectionMSWIN::ShowCloseRasDialog,
			reader->ReadIntL(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SUserPrefs), "AskClosingDialUpConnections"));
	}
#endif

#ifdef PREFS_HAVE_DESKTOP_UI
	// MaximizeNewWindowsWhenAppropriate => Maximize New Windows

	if (reader->IsKey(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SUserPrefs), "MaximizeNewWindowsWhenAppropriate"))
	{
		g_pcui->WriteIntegerL(PrefsCollectionUI::MaximizeNewWindowsWhenAppropriate,
			reader->ReadIntL(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SUserPrefs), "MaximizeNewWindowsWhenAppropriate"));
	}

#endif

#ifdef SKIN_SUPPORT
	// Color Scheme Color => [Color] Skin
	if (reader->IsKey(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SUserPrefs), "Color Scheme Color"))
	{
		g_pcfontscolors->WriteColorL(OP_SYSTEM_COLOR_SKIN,
			reader->ReadIntL(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SUserPrefs), "Color Scheme Color"));
	}

#endif

#ifdef PREFS_HAVE_DESKTOP_UI
	// [TransferWindow]
	// LogEntryDaysToLive => Keep Entries Days
	if (reader->IsKey(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::STransferWindow), "LogEntryDaysToLive"))
	{
		g_pcui->WriteIntegerL(PrefsCollectionUI::TransWinLogEntryDaysToLive,
			reader->ReadIntL(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::STransferWindow), "LogEntryDaysToLive"));
	}
#endif

#ifdef PREFS_HAVE_DESKTOP_UI
	// ActivateOnNewTransfer => Activate On New Transfer
	if (reader->IsKey(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::STransferWindow), "ActivateOnNewTransfer"))
	{
		g_pcui->WriteIntegerL(PrefsCollectionUI::TransWinActivateOnNewTransfer,
			reader->ReadIntL(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::STransferWindow), "ActivateOnNewTransfer"));
	}
#endif

/*
	if (reader->IsKey(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::
	{
		g_pc->WriteIntegerL(PrefsCollection
			reader->ReadIntL(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::
	}
*/

	// [Adv User Prefs] -> [Network]
	OpStackAutoPtr<PrefsSection> advuserprefs(reader->ReadSectionL(UNI_L("Adv User Prefs")));
	const PrefsEntry * OP_MEMORY_VAR entry = advuserprefs.get() ? advuserprefs->Entries() : NULL;
	char sbkey[64]; /* ARRAY OK 2009-02-26 adame */
	while (entry)
	{
#ifdef PREFSFILE_WRITE_GLOBAL
		if (reader->IsKeyLocal(UNI_L("Adv User Prefs"), entry->Key()))
#endif // PREFSFILE_WRITE_GLOBAL
		{
			uni_cstrlcpy(sbkey, entry->Key(), ARRAY_SIZE(sbkey));
#ifdef PREFS_HAVE_STRING_API
			TRAPD(rc, g_prefsManager->WritePreferenceL(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SNetwork), sbkey, entry->Value()));
			OpStatus::Ignore(rc); // Ignore unknown settings
#else
			// FIXME: Update prefsmanager state
			reader->WriteStringL(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SNetwork), sbkey, entry->Value());
#endif
		}

		entry = entry->Suc();
	}

	// Reset settings that have been removed from the UI ------
#ifdef M2_SUPPORT
	// [User Prefs]
	// Show E-mail Client
	g_pcm2->ResetIntegerL(PrefsCollectionM2::ShowEmailClient);
#endif

	// Reset important settings that have changed defaults ------
	// [User Agent]
	// Spoof UserAgent ID
	g_pcnet->ResetIntegerL(PrefsCollectionNetwork::UABaseId);

}

/** Read the expiry time in the pre-9 format (split days, hours, minutes).
  *
  * @return Value in seconds
  */
/*static */int PrefsUpgrade::ReadMinutesHoursDaysValueL(
	PrefsFile *reader,
	const char *section, const char *daykey,
	const char *hourkey, const char *minutekey,
	int defval)
{
	int days = reader->ReadIntL(section, daykey,    (defval / 60 / 60 / 24));
	int hours= reader->ReadIntL(section, hourkey,   (defval / 60 / 60) % 24);
	int mins = reader->ReadIntL(section, minutekey, (defval / 60) % 60);
	return (mins + (hours + days * 24) * 60) * 60;
}
#endif // PREFS_UPGRADE_FROM_CORE1

#ifdef PREFS_UPGRADE_UNITE
/**
 * Upgrade Unite settings from versions prior to core 2.6.
 *
 * @param reader The PrefsFile object used to get data.
 */
/*static*/void PrefsUpgrade::UpgradeUniteL(PrefsFile *reader)
{
	// CORE-29516:
	// [WebServer]
	// Do Not Use Opera Account  ==>  Use Opera Account (in reverse)
	// The new setting is also default FALSE, and we need to set it to
	// TRUE if there are configured Unite settings (WebserverDevice is
	// set).

	// Check if we had a "Do Not Use..." setting, if previously set to
	// TRUE, we do not want to enable it automatically
	BOOL opera_account_explicitely_disabled =
		reader->ReadBoolL(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SWebServer), "Do Not Use Opera Account");

	// Check if there is a web server configuration
	OpStringC webserver_device(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverDevice));
	if (!opera_account_explicitely_disabled && !webserver_device.IsEmpty())
	{
		// Opera Account was previously not explicitely disabled, and we had
		// WebserverDevice set, so enable the new setting.
		g_pcwebserver->WriteIntegerL(PrefsCollectionWebserver::UseOperaAccount, TRUE);
	}
}
#endif // PREFS_UPGRADE_UNITE

#ifdef PREFS_UPGRADE_LIBSSL_VERSIONS
/*static*/ void PrefsUpgrade::UpgradeSSLVersionsL(PrefsFile *reader)
{
	// Make sure tls v1.0 is (default)
	g_pcnet->ResetIntegerL(PrefsCollectionNetwork::EnableSSL3_1);

	// Make sure TLS 1.1 and TLS 1.2 is OFF (default)
	g_pcnet->ResetIntegerL(PrefsCollectionNetwork::EnableTLS1_1);

#ifdef _SUPPORT_TLS_1_2
	g_pcnet->ResetIntegerL(PrefsCollectionNetwork::EnableTLS1_2);
#endif // _SUPPORT_TLS_1_2
}
#endif // PREFS_UPGRADE_LIBSSL_VERSIONS

#ifdef URL_PER_SITE_PROXY_POLICY
/*static*/ void PrefsUpgrade::UpgradeNoProxyServersL()
{
	OpStringC no_proxy_servers_tmp = g_pcnet->GetStringPref(PrefsCollectionNetwork::NoProxyServers);
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::HasNoProxyServers) 
		&& no_proxy_servers_tmp.HasContent())
	{
		OpString no_proxy_servers; ANCHOR(OpString, no_proxy_servers);
		no_proxy_servers.SetL(no_proxy_servers_tmp);
		uni_strlwr(no_proxy_servers.CStr());

		uni_char* no_proxy;
		const uni_char * const separator = UNI_L(" ;,\r\n\t");
		for (no_proxy = uni_strtok(no_proxy_servers.CStr(), separator); no_proxy;
			no_proxy = uni_strtok(NULL, separator))
		{
			uni_char *port_str = uni_strchr(no_proxy, ':');
			if (port_str)
				*(port_str++) = '\0';

			g_pcnet->OverridePrefL(no_proxy, PrefsCollectionNetwork::EnableProxy, FALSE, TRUE);
		}
	}

	g_pcnet->ResetStringL(PrefsCollectionNetwork::NoProxyServers);
	g_pcnet->ResetIntegerL(PrefsCollectionNetwork::HasNoProxyServers);
}
#endif

/*static*/ void PrefsUpgrade::UpgradeDoubleNegativesL(PrefsFile *reader)
{
	/* CORE-37371
	 *
	 *    [User Prefs] Do Not Show No Domain IP Addess Errors
	 * => [User Prefs] Set-Cookie No IP Address Policy
	 */
#if defined _ASK_COOKIE && !defined PUBSUFFIX_ENABLED
	int old_cookie_policy =
		reader->ReadIntL(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SUserPrefs), "Do Not Show No Domain IP Addess Errors", 2);
	if (old_cookie_policy != 2)
		g_pcnet->WriteIntegerL(PrefsCollectionNetwork::DontShowCookieNoIPAddressErr, old_cookie_policy);
#endif

	/*    [User Prefs] Do Not Show Cookie Domain Errors
	 * => [User Prefs] Show Cookie Domain Errors
	 *    (with inverted value)
	 */
#ifdef _ASK_COOKIE
	int old_cookie_domain_error_setting =
		reader->ReadIntL(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SUserPrefs), "Do Not Show Cookie Domain Errors", 1);
	if (!old_cookie_domain_error_setting)
		g_pcnet->WriteIntegerL(PrefsCollectionNetwork::ShowCookieDomainErr, !old_cookie_domain_error_setting);
#endif

	/*    [User Prefs] Do Not Show Cookie Path Errors
	 * => [User Prefs] Cookie Path Error Mode
	 *    (this is not a boolean setting)
	 */
#ifdef _ASK_COOKIE
	int old_cookie_path_error_mode =
		reader->ReadIntL(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SUserPrefs), "Do Not Show Cookie Path Errors", 2);
	if (old_cookie_path_error_mode != 2)
		g_pcnet->WriteIntegerL(PrefsCollectionNetwork::CookiePathErrMode, old_cookie_path_error_mode);
#endif

	/*    [User Prefs] Disable Client Refresh To Same Location
	 * => [User Prefs] Client Refresh To Same Location
	 *    (with inverted value)
	 */
	int old_refresh_to_same =
		reader->ReadIntL(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SUserPrefs), "Disable Client Refresh To Same Location", !DEFAULT_ALLOW_CLIENT_REFRESH_TO_SAME);
	if (old_refresh_to_same == DEFAULT_ALLOW_CLIENT_REFRESH_TO_SAME /* tweak is now inverted as well */)
		g_pcnet->WriteIntegerL(PrefsCollectionNetwork::ClientRefreshToSame, !old_refresh_to_same);

#ifdef IMAGE_TURBO_MODE
	/*    [User Prefs] Turbo Mode
	 * => [Performance] Draw Images Instantly
	 */
	int old_turbo_mode =
		reader->ReadIntL(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SUserPrefs), "Turbo Mode", DEFAULT_TURBO_MODE);
	if (old_turbo_mode != DEFAULT_TURBO_MODE)
		g_pcdoc->WriteIntegerL(PrefsCollectionDoc::TurboMode, old_turbo_mode);
#endif
}


#endif // UPGRADE_SUPPORT
