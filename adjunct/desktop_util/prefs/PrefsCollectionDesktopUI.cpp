/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#include "core/pch.h"

#ifdef PREFS_HAVE_DESKTOP_UI
#include "adjunct/desktop_util/prefs/PrefsCollectionDesktopUI.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/prefsfile/impl/backend/prefssectioninternal.h" // Yeah, it's ugly
#include "modules/prefsfile/impl/backend/prefsentryinternal.h" // This, too
#include "modules/prefsfile/prefsfile.h"
#include "modules/util/gen_str.h"
#include "modules/prefs/prefsmanager/collections/prefs_macros.h"

#include "adjunct/desktop_util/prefs/PrefsCollectionDesktopUI_c.inl"

#include "adjunct/quick/managers/SpeedDialManager.h"

PrefsCollectionUI *PrefsCollectionUI::CreateL(PrefsFile *reader)
{
	if (g_opera->prefs_module.m_pcui)
		LEAVE(OpStatus::ERR);
	g_opera->prefs_module.m_pcui = OP_NEW_L(PrefsCollectionUI, (reader));
	return g_opera->prefs_module.m_pcui;
}

PrefsCollectionUI::~PrefsCollectionUI()
{
#ifdef PREFS_COVERAGE
	CoverageReport(
		m_stringprefdefault, PCUI_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCUI_NUMBEROFINTEGERPREFS);
#endif

	OP_DELETE(m_windowsizeprefs);
	OP_DELETE(m_treeview_columns);
	OP_DELETE(m_treeview_matches);

	g_opera->prefs_module.m_pcui = NULL;
}

void PrefsCollectionUI::ReadAllPrefsL(PrefsModule::PrefsInitInfo *)
{
	// Read everything
	OpPrefsCollection::ReadAllPrefsL(
		m_stringprefdefault, PCUI_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCUI_NUMBEROFINTEGERPREFS);

	ReadCachedQuickSectionsL();
}

#ifdef PREFS_VALIDATE
void PrefsCollectionUI::CheckConditionsL(int which, int *value, const uni_char *host)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<integerpref>(which))
	{
	case ShowTrayIcon:
		break;

	case SecondsBeforeAutoHome:
		if (*value < 30)
			*value = 30;
		else if (*value > 6000)
			*value = 6000;
		break;

	case StartupType:
		if (*value <= _STARTUP_FIRST_ENUM || *value >= _STARTUP_LAST_ENUM)
			*value = STARTUP_CONTINUE;
		break;

#ifdef SUPPORT_SPEED_DIAL
	case NumberOfSpeedDialColumns:
		if (*value < 0) // non-positive integers will be interpreted as "auto"
			*value = 0;
		break;
#endif

#ifdef PREFS_HAVE_DISABLE_OPEN_SAVE
	case NoSave:
	case NoOpen:
#endif
	case ShowProblemDlg:
	case ShowGestureUI:
#ifdef _INTERNAL_BUTTONSET_SUPPORT_
	case UseExternalButtonSet:
#endif
#ifdef LINK_BAR
	case LinkBarPosition:
#endif
		break;
	case ShowExitDialog:
		if (*value < _CONFIRM_EXIT_STRATEGY_FIRST_ENUM || *value >= _CONFIRM_EXIT_STRATEGY_LAST_ENUM)
			*value = ExitStrategyExit;
		break;
	case OperaUniteExitPolicy:
	case WarnAboutActiveTransfersOnExit:
		if (*value < _CONFIRM_EXIT_STRATEGY_FIRST_ENUM || *value >= _CONFIRM_EXIT_STRATEGY_LAST_ENUM)
			*value = ExitStrategyConfirm;
		break;
	case ShowDefaultBrowserDialog:
	case ShowStartupDialog:
#ifndef _NO_MENU_TOGGLE_MENU_
	case ShowMenu:
#endif
	case MaximizeNewWindowsWhenAppropriate:
	case SDI:
	case AllowEmptyWorkspace:
	case ShowWindowMenu:
	case ShowCloseButtons:
	case ClickToMinimize:
	case WindowCycleType:
	case ShowLanguageFileWarning:
	case PersonalbarAlignment:
	case PersonalbarInlineAlignment:
	case MainbarAlignment:
	case PagebarAlignment:
	case AddressbarAlignment:
	case AddressbarInlineAutocompletion:
	case TreeViewDropDownMaxLines:
	case StatusbarAlignment:
#ifdef SUPPORT_GENERATE_THUMBNAILS
	case UseThumbnailsInWindowCycle:
	case UseThumbnailsInTabTooltips:
#endif // SUPPORT_GENERATE_THUMBNAILS
#ifdef _VALIDATION_SUPPORT_
	case ShowValidationDialog:
#endif // _VALIDATION_SUPPORT_
	case ProgressPopup:
	case PopupButtonHelp:
	case WindowRecoveryStrategy:
	case ShowProgressDlg:
#ifndef TARGETED_BANNER_SUPPORT
	case ShowSetupdialogOnStart:
#endif
#ifdef M2_SUPPORT
	case LimitAttentionToPersonalChatMessages:
	case ShowNotificationForNewMessages:
	case ShowNotificationForNoNewMessages:
#endif
	case ShowNotificationForBlockedPopups:
	case ShowNotificationForFinishedTransfers:
	case ShowNotificationsForWidgets:
	case PagebarAutoAlignment:
	case PagebarOpenURLOnMiddleClick:
	case ShowHiddenToolbarsWhileCustomizing:
	case ShowPanelToggle:
	case ColorListRowMode:
	case Running:
	case HotlistVisible:
	case HotlistSingleClick:
	case HotlistSplitter:
	case HotlistSortBy:
	case HotlistSortAsc:
#ifdef M2_SUPPORT
	case HotlistShowAccountInfoDetails:
#endif
	case HotlistBookmarksManagerSplitter:
	case HotlistBookmarksManagerStyle:
	case HotlistBookmarksSplitter:
	case HotlistBookmarksStyle:
	case HotlistContactsManagerSplitter:
	case HotlistContactsManagerStyle:
	case HotlistContactsSplitter:
	case HotlistContactsStyle:
	case HotlistNotesSplitter:
//	case TransWinShowTransferWindow:
	case TransWinLogEntryDaysToLive:
	case TransWinActivateOnNewTransfer:
	case TransWinShowDetails:
#if defined BRANDED_BANNER_WITHOUT_REGISTRATION || defined TARGETED_BANNER_SUPPORT
	case UseBrandedBanner:
	case BrandedBannerWidth:
	case BrandedBannerHeight:
#endif
	case ClearPrivateDataDialog_CheckFlags:
	case NavigationbarAlignment:
	case NavigationbarAutoAlignment:
	case HotlistAlignment:
#ifdef M2_SUPPORT
	case MailHandler:
#endif
	case WebmailService:
	case UseIntegratedSearch:
	case CenterMouseButtonAction:
#ifdef PREFS_HAVE_MIDDLE_MOUSEBUTTON_EXT
	case ExtendedCenterMouseButtonAction:
#endif
	case HasShownCenterClickInfo:
	case OpenPageNextToCurrent:
	case ConfirmOpenBookmarkLimit:
	case EllipsisInCenter:
	case EnableDrag:
	case ShowCloseAllDialog:
	case ShowCloseAllButActiveDialog:
	case AlternativePageCycleMode:
	case ShowAddressInCaption:
	case CheckForNewOpera:
	case ShowNewOperaDialog:
	case TransferItemsAddedOnTop:
	case ImportedCustomBookmarks:
	case FirstRunTimestamp:
	case MaxWidthForBookmarksInMenu:
	case AllowContextMenus:
	case SourceViewerMode:
	case HistoryViewStyle:
	case AskAboutFlashDownload:
#ifdef PERMANENT_HOMEPAGE_SUPPORT
	case PermanentHomepage:
#endif
#ifdef ENABLE_USAGE_REPORT
	case EnableUsageStatistics:
	case ReportTimeoutForUsageStatistics:
	case AskForUsageStatsPercentage:
#endif
	case DisableBookmarkImport:
	case ShowMailErrorDialog:
#ifdef SUPPORT_SPEED_DIAL
	case SpeedDialState:
	case ShowAddSpeedDialButton:
#endif
#ifdef FEATURE_SCROLL_MARKER
	case EnableScrollMarker:
#endif
	case ExtendedKeyboardShortcuts:
	case ShowDisableJSCheckbox:
	case ActivateTabOnClose:
#ifdef WEBSERVER_SUPPORT
	case EnableUnite:
	case RestartUniteServicesAfterCrash:
#ifdef SHOW_DISCOVERED_DEVICES_SUPPORT
	case EnableServiceDiscoveryNotifications:
#endif // SHOW_DISCOVERED_DEVICES_SUPPORT
#endif // WEBSERVER_SUPPORT
#ifdef INTEGRATED_DEVTOOLS_SUPPORT
	case DevToolsSplitter:
	case DevToolsIsAttached:
#endif // INTEGRATED_DEVTOOLS_SUPPORT
#ifdef AUTO_UPDATE_SUPPORT
	case TimeOfLastUpdateCheck:
	case LevelOfUpdateAutomation:
	case AutoUpdateState:
	case AutoUpdateResponded:
	case BrowserJSTime:
	case SpoofTime:
	case SaveAutoUpdateXML:
	case DownloadAllSnapshots:
	case DictionaryTime:
	case HardwareBlocklistTime:
	case HandlersIgnoreTime:
	case CountryCheck:
#if defined INTERNAL_SPELLCHECK_SUPPORT
     case AskAboutMissingDictionary:
#endif
#endif // AUTO_UPDATE_SUPPORT
	case ShowSearchesInAddressfieldAutocompletion:
	case VirtualKeyboardType:
	case OpenDraggedLinkInBackground:
	case DoubleclickToClose:
	case AcceptLicense:
	case AddressSearchDropDownWeightedWidth:
	case DimSearchOpacity:
	case ChromeIntegrationDragArea:
	case ChromeIntegrationDragAreaMaximized:
	case EnableAddressbarFavicons:
	case AddressDropdownMixSuggestions:
		break;  // Nothing to do.

#ifdef SUPPORT_SPEED_DIAL
	case SpeedDialZoomLevel:
	{
		if (*value == 0) // automatic zoom
			break;
		if (*value < static_cast<int>(SpeedDialManager::MinimumZoomLevel * 100) ||
			*value > static_cast<int>(SpeedDialManager::MaximumZoomLevel * 100))
		{
			*value = static_cast<int>(SpeedDialManager::DefaultZoomLevel * 100);
		}
		break;
	}
#endif // SUPPORT_SPEED_DIAL

#ifdef AUTO_UPDATE_SUPPORT
	case UpdateCheckInterval:
	{
		if(*value < 300)
		{
			*value = 300;
		}
		break;
	}
	case DelayedUpdateCheckInterval:
	{
		if (*value < 0)
		{
			*value = 0;
		}
		break;
	}
	case ThrottleLevel:
		if (*value < 0)
			*value = 0;
		else if(*value > 9)
			*value = 9;
		break;
	case ThrottlePeriod:
		if (*value < 1)
			*value = 1;
		else if(*value > 10000)
			*value = 10000;
		break;
	case UpdateCheckIntervalGadgets:
#endif
#ifdef PAGEBAR_THUMBNAILS_SUPPORT
	case PagebarHeight:
#ifdef TAB_THUMBNAILS_ON_SIDES
	case PagebarWidth:
#endif // TAB_THUMBNAILS_ON_SIDES
	case EnableUIAnimations:
	case UseThumbnailsInsideTabs:
		break;
#endif // PAGEBAR_THUMBNAILS_SUPPORT
#ifdef WEB_TURBO_MODE
	case ShowNetworkSpeedNotification:
	case ShowOperaTurboInfo:
	case TurboSNNotificationLeft:
		break;
	case OperaTurboMode:
		if (*value < 0)
			*value = 0;
		else if (*value > 2)
			*value = 2;
		break;
#endif // WEB_TURBO_MODE
#ifdef SKIN_SUPPORT
	case DebugSkin:
		break;
#endif // SKIN_SUPPORT
	case GoogleTLDDownloaded:
	case ShowCrashLogUploadDlg:
	case UpgradeCount:
	case ShowPrivateIntroPage:
	case UseExternalDownloadManager:
	case ShowDownloadManagerSelectionDialog:
	case UIPropertyExaminer:
	case DisableOperaPackageAutoUpdate:
	case HideURLParameter:
	case ShowFullURL:
	case TotalUptime:
	case StartupTimestamp:
	case UseHTTPProxyForAllProtocols:
		break;
#ifdef MSWIN
	case UseWindows7TaskbarThumbnails:
		break;
#endif
#ifdef WIDGET_RUNTIME_SUPPORT
	case DisableWidgetRuntime:
#ifdef _MACINTOSH_
		*value = TRUE;	
#endif // _MACINTOSH_
	case ShowWidgetDebugInfoDialog:
		break;
#endif // WIDGET_RUNTIME_SUPPORT
#ifdef DOM_GEOLOCATION_SUPPORT
	case ShowGeolocationLicenseDialog:
		break;
#endif // DOM_GEOLOCATION_SUPPORT
	case StrategyOnApplicationCache:
		if (*value < 0)
			*value = 0;
		else if(*value > 2)
			*value = 2;
		break;
#ifdef PLUGIN_AUTO_INSTALL
	case PluginAutoInstallMaxItems:
	case PluginAutoInstallEnabled:
		break;
#endif // PLUGIN_AUTO_INSTALL

	case CollectionStyle:
	case CollectionSortType:
		 if (*value < 0)
			 *value = 0;
		 else if(*value > 1)
			 *value = 1;
		 break;

	case ShowDropdownButtonInAddressfield:
	case SoundsEnabled:
	case CollectionZoomLevel:
	case CollectionSplitter:
	case ShowFullscreenExitInformation:
		break;

	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
		break;
	}
}

BOOL PrefsCollectionUI::CheckConditionsL(int which, const OpStringC &invalue,
                                         OpString **outvalue, const uni_char *host)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<stringpref>(which))
	{
	case HotlistActiveTab:
	case LanguageCodesFileName:
	case ValidationURL:
	case ValidationForm:
	case NewestSeenVersion:
	case AuCountryCode:
	case MaxVersionRun:
	case FirstVersionRun:
	case UserEmail:
#if defined BRANDED_BANNER_WITHOUT_REGISTRATION || defined TARGETED_BANNER_SUPPORT
	case BrandedBannerURL:
#endif
	case OneTimePage:
	case CustomBookmarkImportFilename:
	case HotlistBookmarksYSplitter:
	case HotlistContactsYSplitter:
#ifdef OPERA_CONSOLE
	case ErrorConsoleFilter:
#endif
	case IntranetHosts:
#ifdef AUTO_UPDATE_SUPPORT
	case AutoUpdateServer:
	case AutoUpdateGeoServer:
#endif // AUTO_UPDATE_SUPPORT
	case GoogleTLDDefault:
	case GoogleTLDServer:
	case CrashFeedbackPage:
	case UpgradeFromVersion:
	case DownloadManager:
	case ExtensionSetParent:
	case IdOfHardcodedDefaultSearch:
	case HashOfDefaultSearch:
	case IdOfHardcodedSpeedDialSearch:
	case HashOfSpeedDialSearch:
	case TipsConfigMetaFile:
	case TipsConfigFile:
	case CountryCode:
	case DetectedCountryCode:
	case ActiveCountryCode:
	case ActiveRegion:
	case ActiveDefaultLanguage:
	case ClickedSound:
	case EndSound:
	case FailureSound:
	case LoadedSound:
	case StartSound:
	case TransferDoneSound:
	case ProgramTitle:
	case NewestUsedBetaName:

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

BOOL PrefsCollectionUI::GetWindowInfo(const OpStringC &which, OpRect &size, WinSizeState &state)
{
	PrefsEntry *entry = m_windowsizeprefs->FindEntry(which);
	if (entry)
	{
		// A valid entry was found in the cache; retrieve the data from it
		const uni_char *values = entry->Get();
		if (values && *values)
		{
			int x, y, w, h, read_state;
			switch (uni_sscanf(values, UNI_L("%d,%d,%d,%d,%d"),
			                   &x, &y, &w, &h, &read_state))
			{
			case 5:
				// Valid window size state and rectangle
				state = WinSizeState(read_state);
				size.Set(x, y, w, h);
				return TRUE;

			case 4:
				// Valid rectangle
				state = INVALID_WINSIZESTATE;
				size.Set(x, y, w, h);
				return TRUE;
			}
		}
	}

	return FALSE;
}

#ifdef PREFS_WRITE
OP_STATUS PrefsCollectionUI::WriteWindowInfoL(const OpStringC &which, const OpRect &size, WinSizeState state)
{
	if (which.IsEmpty())
	{
		OP_ASSERT(!"Must call WriteWindowInfoL with a named window");
		LEAVE(OpStatus::ERR_NULL_POINTER);
	}

	uni_char inival[64]; /* ARRAY OK 2009-02-26 adame */
	uni_snprintf(inival, ARRAY_SIZE(inival), UNI_L("%d,%d,%d,%d,%d"),
	             static_cast<int>(size.x), static_cast<int>(size.y),
	             static_cast<int>(size.width), static_cast<int>(size.height),
				 static_cast<int>(state));

#ifdef PREFS_READ
	OP_STATUS rc = m_reader->WriteStringL(UNI_L("Windows"), which, inival);
#else
	const OP_STATUS rc = OpStatus::OK;
#endif
	if (OpStatus::IsSuccess(rc))
	{
		// Store in cache
		m_windowsizeprefs->SetL(which, inival);
	}
	return rc;
}
#endif // !PREFS_WRITE

void PrefsCollectionUI::ReadCachedQuickSectionsL()
{
	// Cache the window size preferences section, since we do not know
	// what is inside it :-(
	OP_ASSERT(NULL == m_treeview_columns); // Make sure we are not called twice
	OP_ASSERT(NULL == m_windowsizeprefs);
	OP_ASSERT(NULL == m_treeview_matches);

#ifdef PREFS_READ
	m_treeview_columns = m_reader->ReadSectionInternalL(UNI_L("Columns"));
	/*DOC
	 *section=Columns
	 *name=m_treeview_columns
	 *key=name of treeview
	 *type=string
	 *value=-
	 *description=List of columns for treeview
	 */
	m_windowsizeprefs  = m_reader->ReadSectionInternalL(UNI_L("Windows"));
	/*DOC
	 *section=Windows
	 *name=m_windowsizeprefs
	 *key=name of window
	 *type=string
	 *value=x,y,width,height,state
	 *description=Saved size and state of window
	 */
	m_treeview_matches = m_reader->ReadSectionInternalL(UNI_L("Matches"));
	/*DOC
	 *section=Matches
	 *name=m_treeview_matches
	 *key=name of treeview
	 *type=string
	 *value="list", "of", "search", "words"
	 *description=Remembered search words for quick search
	 */
#else
	m_treeview_columns = OP_NEW_L(PrefsSectionInternal, ());
	m_treeview_columns->ConstructL(NULL);

	m_windowsizeprefs = OP_NEW_L(PrefsSectionInternal, ());
	m_windowsizeprefs->ConstructL(NULL);

	m_treeview_matches = OP_NEW_L(PrefsSectionInternal, ());
	m_treeview_matches->ConstructL(NULL);
#endif
}

#ifdef PREFS_HOSTOVERRIDE
void PrefsCollectionUI::ReadOverridesL(const uni_char *host, PrefsSection *section, BOOL active, BOOL from_user)
{
	ReadOverridesInternalL(host, section, active, from_user,
                       m_integerprefdefault, m_stringprefdefault);
}
#endif // PREFS_HOSTOVERRIDE

const uni_char *PrefsCollectionUI::GetColumnSettings(const OpStringC &which)
{
	return m_treeview_columns->Get(which);
}

#ifdef PREFS_WRITE
void PrefsCollectionUI::WriteColumnSettingsL(const OpStringC &which, const uni_char *column_string)
{
# ifdef PREFS_READ
	m_reader->WriteStringL(UNI_L("Columns"), which, column_string);
# endif
	m_treeview_columns->SetL(which, column_string);
}
#endif // PREFS_WRITE

const uni_char *PrefsCollectionUI::GetMatchSettings(const OpStringC &which)
{
	return m_treeview_matches->Get(which);
}

#ifdef PREFS_WRITE
void PrefsCollectionUI::WriteMatchSettingsL(const OpStringC &which, const uni_char *match_string)
{
# ifdef PREFS_READ
	m_reader->WriteStringL(UNI_L("Matches"), which, match_string);
# endif
	m_treeview_matches->SetL(which, match_string);
}
#endif // PREFS_WRITE

void PrefsCollectionUI::ClearMatchSettingsL()
{
#ifdef PREFS_READ
# ifdef PREFS_WRITE
	m_reader->ClearSectionL(UNI_L("Matches"));
# endif
	// We might still have global or forced values, so re-read
	PrefsSectionInternal *new_treeview_matches =
		m_reader->ReadSectionInternalL(UNI_L("Matches"));
	OP_DELETE(m_treeview_matches); m_treeview_matches = new_treeview_matches;
#endif
}

#endif
