/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/hostoverride.h"
#include "modules/prefs/prefsmanager/collections/prefs_macros.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/url/url_api.h"
#include "modules/pi/ui/OpUiInfo.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/bookmarks/bookmark_manager.h"

#include "modules/prefs/prefsmanager/collections/pc_core_c.inl"

PrefsCollectionCore *PrefsCollectionCore::CreateL(PrefsFile *reader)
{
	if (g_opera->prefs_module.m_pccore)
		LEAVE(OpStatus::ERR);
	g_opera->prefs_module.m_pccore = OP_NEW_L(PrefsCollectionCore, (reader));
	return g_opera->prefs_module.m_pccore;
}

PrefsCollectionCore::~PrefsCollectionCore()
{
#ifdef PREFS_COVERAGE
	CoverageReport(
		m_stringprefdefault, PCCORE_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCCORE_NUMBEROFINTEGERPREFS);
#endif

	g_opera->prefs_module.m_pccore = NULL;
}

int PrefsCollectionCore::GetDefaultIntegerPref(integerpref which) const
{
	// Need to handle special cases here
	switch (which)
	{
#ifdef MOUSE_SUPPORT
	case ReverseButtonFlipping:
		return !g_op_ui_info->IsMouseRightHanded();
#endif

	default:
		return m_integerprefdefault[which].defval;
	}
}

#ifdef PREFS_HAVE_STRING_API
int PrefsCollectionCore::GetDefaultIntegerInternal(int which, const struct integerprefdefault *)
{
	return GetDefaultIntegerPref(integerpref(which));
}
#endif // PREFS_HAVE_STRING_API

void PrefsCollectionCore::ReadAllPrefsL(PrefsModule::PrefsInitInfo *)
{
	// Read everything
	OpPrefsCollection::ReadAllPrefsL(
		m_stringprefdefault, PCCORE_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCCORE_NUMBEROFINTEGERPREFS);

	// Fix-ups
#ifdef MOUSE_SUPPORT
	if (-1 == m_intprefs[ReverseButtonFlipping])
	{
		m_intprefs[ReverseButtonFlipping] =
			GetDefaultIntegerPref(ReverseButtonFlipping);
	}
#endif
}

#ifdef PREFS_HOSTOVERRIDE
void PrefsCollectionCore::ReadOverridesL(const uni_char *host, PrefsSection *section, BOOL active, BOOL from_user)
{
	ReadOverridesInternalL(host, section, active, from_user,
	                       m_integerprefdefault, m_stringprefdefault);
}
#endif // PREFS_HOSTOVERRIDE

#ifdef PREFS_VALIDATE
void PrefsCollectionCore::CheckConditionsL(int which, int *value, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<integerpref>(which))
	{
#ifdef VISITED_PAGES_SEARCH
	case VisitedPages:
		break;
#endif // VISITED_PAGES_SEARCH
#ifdef DIRECT_HISTORY_SUPPORT
	case MaxDirectHistory:
#endif
	case MaxWindowHistory:
		if (*value > MAX_HISTORY_LINES)
			*value = MAX_HISTORY_LINES;
		else if (*value < - MAX_HISTORY_LINES)
			*value = - MAX_HISTORY_LINES;
		break;

	case MaxGlobalHistory:
		if (*value > MAX_GLOBAL_HISTORY_LINES)
			*value = MAX_GLOBAL_HISTORY_LINES;
		else if (*value < - MAX_GLOBAL_HISTORY_LINES)
			*value = - MAX_GLOBAL_HISTORY_LINES;
		break;

#ifdef OPERA_CONSOLE
	case MaxConsoleMessages:
		if (*value < 1)
			*value = 1;
		break;
#endif

#ifdef PHONE_SN_HANDLER
	case ScrollStep:
	case ScrollStepHandheld:
	case ScrollStepHorizontal:
		/* Percentage */
		if (*value < 0)
			*value = 0;
		else if (*value > 100)
			*value = 100;
		break;
#endif

#if defined CORE_BOOKMARKS_SUPPORT && defined PREFS_HAVE_BOOKMARK
	case BookmarksSaveTimeout:
		if (*value < 0)
			*value = BOOKMARKS_DEFAULT_SAVE_TIMEOUT;
		break;

	case BookmarksSavePolicy:
		switch (*value)
		{
			case BookmarkManager::NO_AUTO_SAVE:
			case BookmarkManager::SAVE_IMMEDIATELY:
			case BookmarkManager::DELAY_AFTER_FIRST_CHANGE:
			case BookmarkManager::DELAY_AFTER_LAST_CHANGE:
				break;

			default:
				*value = BookmarkManager::DELAY_AFTER_FIRST_CHANGE;
				break;
		}
		break;

	case BookmarksMaxCount:
	case BookmarksMaxFolderDepth:
	case BookmarksMaxCountPerFolder:
	case BookmarksUrlMaxLength:
	case BookmarksTitleMaxLength:
	case BookmarksDescMaxLength:
	case BookmarksSnMaxLength:
	case BookmarksFaviconFileMaxLength:
	case BookmarksThumbnailFileMaxLength:
	case BookmarksCreatedMaxLength:
	case BookmarksVisitedMaxLength:
#endif // CORE_BOOKMARKS_SUPPORT && PREFS_HAVE_BOOKMARK

#ifdef NEARBY_ELEMENT_DETECTION
	case NearbyElementsRadius:
		if (*value < 0)
			*value = 0;
		break;
	case DetectNearbyElements:
	case AlwaysExpandLinks:
		break;
	case ExpandedMinimumSize:
		if (*value < 1)
			*value = 1;
		break;
#endif // NEARBY_ELEMENT_DETECTION

#ifdef PREFS_HAVE_ANIMATION_THROTTLING
	case SwitchAnimationThrottlingInterval:
		if (*value < 0)
			*value = 2000;
		break;
	case LagThresholdForAnimationThrottling:
		if (*value < 0)
			*value = 50;
		break;
#endif // PREFS_HAVE_ANIMATION_THROTTLING

#ifdef MOUSE_SUPPORT
	case GestureThreshold:
		if (*value < 1)
			*value = 1;
		break;
#endif

	case ImageRAMCacheSize:
	case RamCacheFigs:
#ifdef WAND_SUPPORT
	case EnableWand:
	case AutocompleteOffDisablesWand:
	case WandAutosubmit:
#endif
#ifdef MOUSE_SUPPORT
	case EnableGesture:
	case ReverseButtonFlipping:
	case EnableMouseFlips:
#endif
#ifdef SEARCH_ENGINES
	case PreferredNumberOfHits:
#endif
	case ColorSchemeMode:
	case SkinScale:
	case SpecialEffects:
	case RamCacheDocs:
	case DocumentCacheSize:
#ifdef OPERA_CONSOLE_LOGFILE
	case UseErrorLog:
#endif
#ifdef COMPONENT_IN_UASTRING_SUPPORT
	case AllowComponentsInUAStringComment:
#endif
#ifdef OPERACONFIG_URL
	case OperaConfigEnabled:
#endif
#if defined UPGRADE_SUPPORT && defined PREFS_WRITE
	case PreferenceUpgrade:
#endif
#ifdef PREFS_HAVE_SPOOF_TIMESTAMP
	case SpoofTimeStamp:
	case SpoofServerTimeStamp:
#endif
#ifdef PREFS_HAVE_LAST_SITEPATCH_UPDATE_CHECK
	case LastSitepatchUpdateCheck:
#endif
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	case SpellcheckEnabledByDefault:
#endif
#ifdef APPLICATION_CACHE_SUPPORT
	case DefaultApplicationCacheQuota:
#endif // APPLICATION_CACHE_SUPPORT
#ifdef CORE_SPEED_DIAL_SUPPORT
        case EnableSpeedDial:
#endif // CORE_SPEED_DIAL_SUPPORT
#ifdef DOM_LOAD_TV_APP
	case EnableTVStore:
#endif // DOM_LOAD_TV_APP
		break; // Nothing to do.

#ifdef SVG_SUPPORT
	case SVGRAMCacheSize:
		if (*value < 0)
			value = 0;
		break;
#endif // SVG_SUPPORT
#ifdef AB_ERROR_SEARCH_FORM
	case EnableSearchFieldErrorPage:
		*value = !!*value;
		break;
#endif //AB_ERROR_SEARCH_FORM
#ifdef WEB_HANDLERS_SUPPORT
	case ProtocolsListTimeStamp:
	case ProtocolsListServerTimeStamp:
		break;
#endif

#if defined(VEGA_3DDEVICE) || defined(CANVAS3D_SUPPORT)
# ifdef VEGA_3DDEVICE
	case EnableHardwareAcceleration:
# endif // VEGA_3DDEVICE
# ifdef CANVAS3D_SUPPORT
	case EnableWebGL:
# endif // CANVAS3D_SUPPORT
		if (*value < Disable || *value > Force)
			*value = Auto;
		break;
#endif // VEGA_3DDEVICE || CANVAS3D_SUPPORT

#ifdef VEGA_BACKENDS_BLOCKLIST_FETCH
	case BlocklistRetryDelay:
		if (*value < 0)
			*value = 10;
		break;
	case BlocklistRefetchDelay:
		if (*value < 0)
			*value = 600;
		break;
#endif // VEGA_BACKENDS_BLOCKLIST_FETCH

#ifdef PREFS_HAVE_PREFERRED_RENDERER
	case PreferredRenderer:
		if (*value < OpenGL || *value > DirectX10Warp)
			*value = DEFAULT_PREFERRED_RENDERER;
		break;
#endif // PREFS_HAVE_PREFERRED_RENDERER

	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}
}

BOOL PrefsCollectionCore::CheckConditionsL(int which,
                                           const OpStringC &invalue,
                                           OpString **outvalue,
                                           const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<stringpref>(which))
	{
	case HomeURL:
		{
			OpString result; ANCHOR(OpString, result);
			if (g_url_api && g_url_api->ResolveUrlNameL(invalue, result, TRUE) && invalue.Compare(result) != 0)
			{
				// The resulting string can be much over-allocated, so
				// copy it over. If that gets fixed, change to
				// allocate directly to *outvalue and call TakeOver().
				OpStackAutoPtr<OpString> newvalue(OP_NEW_L(OpString, ()));
				newvalue->SetL(result);
				*outvalue = newvalue.release();
				return TRUE;
			}
			break;
		}

	case Firstname:
	case Surname:
	case Address:
	case City:
	case State:
	case Zip:
	case Country:
	case Telephone:
	case Telefax:
	case EMail:
	case Home:
	case Special1:
	case Special2:
	case Special3:
#ifdef PREFS_DOWNLOAD
	case PreferenceServer:
#endif
#ifdef OPERA_CONSOLE_LOGFILE
	case ErrorLogFilter:
#endif
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	case LastUsedSpellcheckLanguage:
#endif
#ifdef VEGA_BACKENDS_BLOCKLIST_FETCH
	case BlocklistLocation:
#endif // VEGA_BACKENDS_BLOCKLIST_FETCH
		break; // Nothing to do.

#ifdef PREFS_HAVE_CUSTOM_UA
	case CustomUAString:
		{
			// String cannot contain any non-printable or non-ASCII characters
			for (const uni_char *p = invalue.CStr(); *p; ++ p)
			{
				if (*p < 0x20 || *p > 0x7F)
				{
					OpStackAutoPtr<OpString> newvalue(OP_NEW_L(OpString, ()));
					newvalue->SetL(invalue);
					// Strip
					for (uni_char *p2 = newvalue->CStr(); *p2; ++ p2)
					{
						if (*p2 < 0x20 || *p2 > 0x7F)
						{
							*p2 = ' ';
						}
					}
					*outvalue = newvalue.release();
					return TRUE;
				}
			}
		}
		break;
#endif

	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}
	// When FALSE is returned, no OpString is created for outvalue
	return FALSE;
}

void PrefsCollectionCore::DelayedInitL()
{
	// We could not do validation of the home URL on startup since
	// the URL module hadn't been properly initialized. Do that now.
	OpString *new_string;
	if (CheckConditionsL(HomeURL, m_stringprefs[HomeURL], &new_string, NULL))
	{
		m_stringprefs[HomeURL].TakeOver(*new_string);
		OP_DELETE(new_string);
	}
}
#endif // PREFS_VALIDATE
