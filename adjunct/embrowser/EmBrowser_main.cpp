/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
/** @file embrowser_main.cpp
  *
  * This file represents the platform independent glue code for embedding
  * opera using the EmBrowser API as a rendering library inside desktop applications.
  *
  * The EmBrowser API was co-developed with Charles McBrian of Macromedia.
  *
  * A corresponding embrowser_mac.cpp, and embrowser_win.cpp are created for the
  * platform specific portions.
  *
  * @author Jason Hazlett
  * @author Anders Markussen
  */
#include "core/pch.h"

#ifdef EMBROWSER_SUPPORT

#include "modules/logdoc/html_col.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/dochand/winman.h"
#include "modules/display/prn_info.h"
#include "modules/searchmanager/searchmanager.h"
#include "modules/util/winutils.h"
#include "modules/viewers/viewers.h"
#include "modules/history/direct_history.h"
#include "modules/url/uamanager/ua.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/display/styl_man.h"
#include "modules/display/vis_dev.h"
#include "modules/display/VisDevListeners.h"
#include "modules/doc/doc.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/dochand/fdelm.h"
#include "modules/dochand/win.h"
#include "modules/dom/src/domobj.h"
#include "modules/locale/src/opprefsfilelanguagemanager.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/util/gen_str.h"
#include "modules/cookies/url_cm.h"

#include "adjunct/desktop_util/boot/DesktopBootstrap.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/models/DesktopHistoryModel.h"
#include "adjunct/embrowser/EmBrowser_main.h"
#include "adjunct/embrowser/EmBrowser.h"

#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick/managers/PrivacyManager.h"
#include "adjunct/quick/managers/CommandLineManager.h"

#include "modules/windowcommander/src/WindowCommander.h"

#include "modules/ecmascript/ecmascript.h"

#if defined(_MACINTOSH_)
#include "platforms/mac/model/CocoaOperaListener.h" 
#include "platforms/mac/embrowser/EmBrowser_mac.h"
#include "platforms/mac/folderdefines.h"
#include "platforms/mac/util/macutils.h"
#include "platforms/mac/Specific/Platform.h"
#include "platforms/mac/embrowser/EmBrowserQuick.h"
#include "platforms/mac/File/FileUtils_Mac.h"
#include "platforms/mac/pi/MacOpView.h"
#include "platforms/mac/pi/ui/MacOpUiInfo.h"
#include "adjunct/desktop_util/opfile/desktop_opfile.h"
#include "adjunct/desktop_util/file_utils/FileUtils.h"
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"
const uni_char* DEFAULT_SOURCE_VIEWER = UNI_L("/Applications/TextEdit.app");
#endif

#if !defined(_MACINTOSH_)
#  include "modules/softcore/misc/usereval.h"
#endif // !defined(_MACINTOSH_)

#ifdef MSWIN
#include "platforms/windows/WindowsOpWindow.h"
#include "platforms/windows/WindowsOpSystemInfo.h"
#include "platforms/windows/windows_ui/msghook.h"
#include "platforms/windows/WindowsOpMessageLoop.h"
#include "platforms/windows/WindowsOpScreenInfo.h"
#include "modules/url/url_rel.h"
#include "platforms/windows/lib/OUniAnsi/OpUAInit.h"
#include "platforms/windows/windows_ui/IntMouse.h"
#include "platforms/windows/windows_ui/printwin.h"

#include "platforms/windows/user.h"
# include "platforms/windows/WindowsOpPluginWindow.h"

#ifndef _NO_GLOBALS_

extern BOOL BLDMainRegClass(HINSTANCE, BOOL use_dll_main_window_proc);
extern HWND BLDCreateWindow(HINSTANCE);

extern HINSTANCE			hInst;			// Handle to instance.
extern OpString g_spawnPath;

#endif

extern void OnDestroyMainWindow();

#endif

#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/util/adt/opvector.h"

#ifndef _MACINTOSH_
#include <wchar.h>
#endif

#ifdef _AM_DEBUG
# include "platforms/mac/debug/anders/settings.anders.h"
#endif


#if defined(HAVE_LTH_MALLOC)
#include "modules/memtools/happy-malloc.h"
#endif

#define EMBEDDED_DEFAULT_PREFS_FIX

#include "modules/dochand/winman.h"

#if defined(USE_LEA_MALLOC)
void osCleanupMem(void);
#endif

EmBrowserAppNotification *gApplicationProcs = NULL;
EmBrowserMethods *gBrowserMethods = NULL;
	// List of valid EmDocumentRefs
uint32 gEmbeddedRegistration = 42589437;
int gActiveModalDialogs = 0;
EmBrowserRef gLastActiveEmBrowser = NULL;

long gVendorDataID = 0;
BOOL gInternalFontSwitching = TRUE;

#ifdef EMBEDDED_DEFAULT_PREFS_FIX
struct EmbeddedDefaultsRec
{
	BOOL				trustMIMETypes;
	UA_BaseStringId		userAgentId;
	int					userAgentVersion;
} gEmbeddedDefaults;
#endif

CoreComponent *g_opera;

#if !defined(EM_BROWSER_FUNCTION_BEGIN)
#define EM_BROWSER_FUNCTION_BEGIN
#endif
#if !defined(EM_BROWSER_FUNCTION_END)
#define EM_BROWSER_FUNCTION_END
#endif


#ifdef AUTOSAVE_WINDOWS
bool dont_autosave = true;
#endif

#ifdef _MACINTOSH_
#pragma mark -
#endif

EmBrowserStatus WidgetVersion(long *majorVersion,long *minorVersion)
{
	*majorVersion = EMBROWSER_MAJOR_VERSION;
	*minorVersion = EMBROWSER_MINOR_VERSION;

	return emBrowserNoErr;
}

/*DEPRECATED*/
WindowCommander* MakeNewWidget(BOOL background, int width, int height, BOOL is_client_size, BOOL3 scrollbars, BOOL3 toolbars, BOOL open_in_page, const uni_char* destURL, Window* parent)
{
	return NULL;
}

/*DEPRECATED*/
OpWindowCommander* GetActiveEmBrowserWindowCommander()
{
	return windowManager->FirstWindow() ? windowManager->FirstWindow()->GetWindowCommander() : 0;
}

/*DEPRECATED*/
EmBrowserRef GetBrowserRefFromWindow(Window* window)
{
	return NULL;
}

/*DEPRECATED*/
EmBrowserRef GetBrowserRefFromDocumentRef(struct OpaqueEmBrowserDocument*)
{
	return NULL;
}

// Public Name: EmBrowserShutdown
EmBrowserStatus WidgetShutdownLibrary()
{
	EM_BROWSER_FUNCTION_BEGIN

	PROFILE_OPERA_PUSH(3500);
	PROFILE_OPERA_PUSH(3501);

#ifdef MSWIN
	OnDestroyMainWindow();
	RemoveMSWinMsgHooks();
#endif

	PROFILE_OPERA_POP_THEN_PUSH(3510);

	if (globalHistory)
	{
		globalHistory->Save(TRUE);
	}

	PROFILE_OPERA_POP_THEN_PUSH(3520);

	if (directHistory)
	{
		directHistory->Save(TRUE);
	}

	PROFILE_OPERA_POP_THEN_PUSH(3530);

	PROFILE_OPERA_POP_THEN_PUSH(3540);

#ifdef _MACINTOSH_
	if (g_desktop_bootstrap->GetApplication())
	{
		g_desktop_bootstrap->ShutDown();
#endif

	PROFILE_OPERA_POP_THEN_PUSH(3550);

	//::Terminate();								// call Opera's termination handler

	OP_DELETE(gBrowserMethods);
	gBrowserMethods = NULL;

	PROFILE_OPERA_POP_THEN_PUSH(3560);

	g_opera->Destroy();

#ifdef _MACINTOSH_
	MacWidgetShutdownLibrary();
#endif

	OP_DELETE(g_component_manager);

	g_component_manager = NULL;
	g_opera = NULL;

#ifdef _MACINTOSH_
	// Main app listener must not be killed until after the g_component_manager is
	delete gCocoaOperaListener;
	gCocoaOperaListener = NULL;
	MacTLS::Destroy();
#endif
	}

	PROFILE_OPERA_POP_THEN_PUSH(3570);

#if defined(HAVE_LTH_MALLOC)
//	OutputExitStatus();
#else
#if defined(USE_LEA_MALLOC)
	osCleanupMem();
#endif
#endif

#ifdef MSWIN
	OP_DELETE(g_windows_message_loop);
#endif

#ifdef _MACINTOSH_
	if(gVendorDataID == 'OPRA')
		OP_PROFILE_EXIT();
#endif // _MACINTOSH_

	PROFILE_OPERA_POP(); // 3570
	PROFILE_OPERA_POP(); // 3500

	return emBrowserNoErr;
}

#ifdef _MACINTOSH_
#pragma mark -
#endif //_MACINTOSH_

#ifdef _MACINTOSH_

#ifndef USE_COMMON_RESOURCES

void SetFoldersL(OperaInitInfo& info)
{
	for (int i = 0; i < OPFILE_FOLDER_COUNT; i++)
	{
		// Set all to something safe.
		info.default_folder_parents[i] = OPFILE_ABSOLUTE_FOLDER;
	}
	OpString path;

	if(OpFileUtils::SetToSpecialFolder(OPFILE_TEMP_FOLDER, path))
		info.default_folders[OPFILE_TEMP_FOLDER].SetL(path);

	if(OpFileUtils::SetToSpecialFolder(OPFILE_RESOURCES_FOLDER, path))
		info.default_folders[OPFILE_RESOURCES_FOLDER].SetL(path);

	if(OpFileUtils::SetToSpecialFolder(OPFILE_INI_FOLDER, path))
		info.default_folders[OPFILE_INI_FOLDER].SetL(path);

	info.default_folder_parents[OPFILE_STYLE_FOLDER] = OPFILE_RESOURCES_FOLDER;
	info.default_folders[OPFILE_STYLE_FOLDER].SetL(UNI_L("Styles/"));

	info.default_folder_parents[OPFILE_USERSTYLE_FOLDER] = OPFILE_STYLE_FOLDER;
	info.default_folders[OPFILE_USERSTYLE_FOLDER].SetL(UNI_L("user/"));

#ifdef PREFS_USE_CSS_FOLDER_SCAN
	info.default_folder_parents[OPFILE_USERPREFSSTYLE_FOLDER] = OPFILE_HOME_FOLDER;
	info.default_folders[OPFILE_USERPREFSSTYLE_FOLDER].SetL(UNI_L("Styles/user/"));
#endif // PREFS_USE_CSS_FOLDER_SCAN

#ifdef SESSION_SUPPORT
	info.default_folder_parents[OPFILE_SESSION_FOLDER] = OPFILE_HOME_FOLDER;
	info.default_folders[OPFILE_SESSION_FOLDER].SetL(UNI_L("Sessions/"));
#endif // SESSION_SUPPORT

	if(OpFileUtils::SetToSpecialFolder(OPFILE_HOME_FOLDER, path))
		info.default_folders[OPFILE_HOME_FOLDER].SetL(path);

#ifdef UTIL_CAP_LOCAL_HOME_FOLDER
	if(OpFileUtils::SetToSpecialFolder(OPFILE_LOCAL_HOME_FOLDER, path))
		info.default_folders[OPFILE_LOCAL_HOME_FOLDER].SetL(path);
#endif // UTIL_CAP_LOCAL_HOME_FOLDER

	if(OpFileUtils::SetToSpecialFolder(OPFILE_USERPREFS_FOLDER, path))
		info.default_folders[OPFILE_USERPREFS_FOLDER].SetL(path);

	if(OpFileUtils::SetToSpecialFolder(OPFILE_GLOBALPREFS_FOLDER, path))
		info.default_folders[OPFILE_GLOBALPREFS_FOLDER].SetL(path);

	if(OpFileUtils::SetToSpecialFolder(OPFILE_FIXEDPREFS_FOLDER, path))
		info.default_folders[OPFILE_FIXEDPREFS_FOLDER].SetL(path);

	if(OpFileUtils::SetToSpecialFolder(OPFILE_CACHE_FOLDER, path))
		info.default_folders[OPFILE_CACHE_FOLDER].SetL(path);

	if(OpFileUtils::SetToSpecialFolder(OPFILE_VPS_FOLDER, path))
		info.default_folders[OPFILE_VPS_FOLDER].SetL(path);

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	if(OpFileUtils::SetToSpecialFolder(OPFILE_OCACHE_FOLDER, path))
		info.default_folders[OPFILE_OCACHE_FOLDER].SetL(path);
#endif // __OEM_EXTENDED_CACHE_MANAGEMENT __OEM_OPERATOR_CACHE_MANAGEMENT

	info.default_folder_parents[OPFILE_LANGUAGE_FOLDER] = OPFILE_RESOURCES_FOLDER;
	info.default_folders[OPFILE_LANGUAGE_FOLDER].SetL(GetDefaultLanguage());
	info.default_folders[OPFILE_LANGUAGE_FOLDER].AppendL(UNI_L(".lproj/"));

/*
	info.default_folder_parents[OPFILE_LANGUAGE_FALLBACK_FOLDER] = OPFILE_RESOURCES_FOLDER;
	info.default_folders[OPFILE_LANGUAGE_FALLBACK_FOLDER].SetL(UNI_L(""));
*/

	info.default_folder_parents[OPFILE_IMAGE_FOLDER] = OPFILE_RESOURCES_FOLDER;
	info.default_folders[OPFILE_IMAGE_FOLDER].SetL(UNI_L("Images/"));

#ifdef _FILE_UPLOAD_SUPPORT_
	if(OpFileUtils::SetToSpecialFolder(OPFILE_UPLOAD_FOLDER, path))
		info.default_folders[OPFILE_UPLOAD_FOLDER].SetL(path);
#endif // _FILE_UPLOAD_SUPPORT_

#ifdef SKIN_SUPPORT
	info.default_folder_parents[OPFILE_SKIN_FOLDER] = OPFILE_HOME_FOLDER;
	info.default_folders[OPFILE_SKIN_FOLDER].SetL(UNI_L("Skin/"));

	info.default_folder_parents[OPFILE_DEFAULT_SKIN_FOLDER] = OPFILE_RESOURCES_FOLDER;
	info.default_folders[OPFILE_DEFAULT_SKIN_FOLDER].SetL(UNI_L("Skin/"));
#endif // SKIN_SUPPORT

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	if(OpFileUtils::SetToSpecialFolder(OPFILE_DICTIONARY_FOLDER, path))
		info.default_folders[OPFILE_DICTIONARY_FOLDER].SetL(path);
#endif // INTERNAL_SPELLCHECK_SUPPORT

#ifdef WEBSERVER_SUPPORT
	info.default_folder_parents[OPFILE_WEBSERVER_FOLDER] = OPFILE_HOME_FOLDER;
	info.default_folders[OPFILE_WEBSERVER_FOLDER].SetL(UNI_L("Webserver/"));

#ifdef GADGET_SUPPORT
	if(OpFileUtils::SetToSpecialFolder(OPFILE_UNITE_PACKAGE_FOLDER, path))
		info.default_folders[OPFILE_UNITE_PACKAGE_FOLDER].SetL(path);
#endif // GADGET_SUPPORT
#endif // WEBSERVER_SUPPORT
	
#ifdef _FILE_UPLOAD_SUPPORT_
	if(OpFileUtils::SetToSpecialFolder(OPFILE_OPEN_FOLDER, path))
		info.default_folders[OPFILE_OPEN_FOLDER].SetL(path);

	if(OpFileUtils::SetToSpecialFolder(OPFILE_SAVE_FOLDER, path))
		info.default_folders[OPFILE_SAVE_FOLDER].SetL(path);
#endif // _FILE_UPLOAD_SUPPORT_

	if(OpFileUtils::SetToSpecialFolder(OPFILE_DOWNLOAD_FOLDER, path))
		info.default_folders[OPFILE_DOWNLOAD_FOLDER].SetL(path);

	info.default_folder_parents[OPFILE_BUTTON_FOLDER] = OPFILE_HOME_FOLDER;
	info.default_folders[OPFILE_BUTTON_FOLDER].SetL(UNI_L("Buttons/"));

	info.default_folder_parents[OPFILE_ICON_FOLDER] = OPFILE_HOME_FOLDER;
	info.default_folders[OPFILE_ICON_FOLDER].SetL(UNI_L("Icons/"));

# if defined SUPPORT_GENERATE_THUMBNAILS && defined SUPPORT_SAVE_THUMBNAILS
	info.default_folder_parents[OPFILE_THUMBNAIL_FOLDER] = OPFILE_HOME_FOLDER;
	info.default_folders[OPFILE_THUMBNAIL_FOLDER].SetL(UNI_L("Thumbnails/"));
# endif // defined SUPPORT_GENERATE_THUMBNAILS && defined SUPPORT_SAVE_THUMBNAILS

	info.default_folder_parents[OPFILE_MENUSETUP_FOLDER] = OPFILE_HOME_FOLDER;
	info.default_folders[OPFILE_MENUSETUP_FOLDER].SetL(UNI_L("Menu/"));

	info.default_folder_parents[OPFILE_TOOLBARSETUP_FOLDER] = OPFILE_HOME_FOLDER;
	info.default_folders[OPFILE_TOOLBARSETUP_FOLDER].SetL(UNI_L("Toolbars/"));

	info.default_folder_parents[OPFILE_MOUSESETUP_FOLDER] = OPFILE_HOME_FOLDER;
	info.default_folders[OPFILE_MOUSESETUP_FOLDER].SetL(UNI_L("Mouse/"));

	info.default_folder_parents[OPFILE_KEYBOARDSETUP_FOLDER] = OPFILE_HOME_FOLDER;
	info.default_folders[OPFILE_KEYBOARDSETUP_FOLDER].SetL(UNI_L("Keyboard/"));

#ifdef M2_SUPPORT
	if(OpFileUtils::SetToSpecialFolder(OPFILE_MAIL_FOLDER, path))
		info.default_folders[OPFILE_MAIL_FOLDER].SetL(path);
#endif // M2_SUPPORT

#ifdef JS_PLUGIN_SUPPORT
	if(OpFileUtils::SetToSpecialFolder(OPFILE_JSPLUGIN_FOLDER, path))
		info.default_folders[OPFILE_JSPLUGIN_FOLDER].SetL(path);
#endif // JS_PLUGIN_SUPPORT

	if(OpFileUtils::SetToSpecialFolder(OPFILE_DESKTOP_FOLDER, path))
		info.default_folders[OPFILE_DESKTOP_FOLDER].SetL(path);

#ifdef GADGET_SUPPORT
	if(OpFileUtils::SetToSpecialFolder(OPFILE_GADGET_FOLDER, path))
		info.default_folders[OPFILE_GADGET_FOLDER].SetL(path);
	if(OpFileUtils::SetToSpecialFolder(OPFILE_GADGET_PACKAGE_FOLDER, path))
		info.default_folders[OPFILE_GADGET_PACKAGE_FOLDER].SetL(path);
#endif // GADGET_SUPPORT

#ifndef NO_EXTERNAL_APPLICATIONS
	if(OpFileUtils::SetToSpecialFolder(OPFILE_TEMPDOWNLOAD_FOLDER, path))
		info.default_folders[OPFILE_TEMPDOWNLOAD_FOLDER].SetL(path);
#endif // NO_EXTERNAL_APPLICATIONS

#ifdef _BITTORRENT_SUPPORT_
	if(OpFileUtils::SetToSpecialFolder(OPFILE_BITTORRENT_METADATA_FOLDER, path))
		info.default_folders[OPFILE_BITTORRENT_METADATA_FOLDER].SetL(path);
#endif // _BITTORRENT_SUPPORT_

	if(OpFileUtils::SetToSpecialFolder(OPFILE_SECURE_FOLDER, path))
		info.default_folders[OPFILE_SECURE_FOLDER].SetL(path);

#ifdef SELFTEST
	if(OpFileUtils::SetToSpecialFolder(OPFILE_SELFTEST_DATA_FOLDER, path))
		info.default_folders[OPFILE_SELFTEST_DATA_FOLDER].SetL(path);
#endif // SELFTEST

#ifdef WEBFEEDS_BACKEND_SUPPORT
	info.default_folder_parents[OPFILE_WEBFEEDS_FOLDER] = OPFILE_HOME_FOLDER;
	info.default_folders[OPFILE_WEBFEEDS_FOLDER].SetL(UNI_L("Webfeeds/"));
#endif // WEBFEEDS_BACKEND_SUPPORT	
}

#endif // !USE_COMMON_RESOURCES

#endif

#ifdef _MACINTOSH_
#pragma mark -
#pragma mark ----- INIT PROC -----
#pragma mark -
#endif //_MACINTOSH_

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MACINTOSH_)
# if defined(__MWERKS__)
#  pragma export on
# else
#  pragma GCC visibility push(default)
# endif
#elif WIN32
__declspec(dllexport)
#else
#pragma PRAGMAMSG("(Export this function)")
#endif //_MACINTOSH_
// Public name: EmBrowserInitLibraryProc
EmBrowserStatus WidgetInitLibrary(EmBrowserInitParameterBlock *initPB)
{
	EM_BROWSER_FUNCTION_BEGIN

	PROFILE_OPERA_PREPARE();

#ifdef MSWIN
	BOOL compLayerOk = InitUnicodeAPI(hInst);	// load & init unicode comp. layer
	if (!compLayerOk)
	{
		return emBrowserGenericErr;
	}
#endif //MSWIN

	PROFILE_OPERA_PUSH(3000);
	PROFILE_OPERA_PUSH(3001);
	
	// Create the main component manager
#ifdef _MACINTOSH_
	if (MacTLS::Init() != 0)
		return -1;
#endif
	if (OpStatus::IsError(OpComponentManager::Create(&g_component_manager)))
		return -1;

	OpComponent* singleton;
	if (OpStatus::IsError(g_component_manager->CreateComponent(singleton, COMPONENT_SINGLETON)))
	{
		OP_DELETE(g_component_manager);
#ifdef _MACINTOSH_
		MacTLS::Destroy();
#endif
		fprintf(stderr, "Out of memory on initialization");
		return -1;
	}
	g_opera = static_cast<CoreComponent*>(singleton);

//	OperaInitInfo info;
#ifdef _MACINTOSH_
	// This is way up here away from the code because this variable must exist for the
	// lifetime of the OperaInitInfo info one so putting it next to it is the safest solution
	OpString default_language;
#endif

#ifdef MSWIN
	g_windows_message_loop = OP_NEW(WindowsOpMessageLoop, ());
	g_windows_message_loop->Init();
	g_opera->SetListener(g_windows_message_loop);
#endif // MSWIN

	PROFILE_OPERA_POP_THEN_PUSH(3020);

	initPB->shutdown        = WidgetShutdownLibrary;
	gApplicationProcs = initPB->notifyCallbacks;
	gVendorDataID     = initPB->vendorDataID;

	gBrowserMethods = OP_NEW(EmBrowserMethods, ());
	initPB->browserMethods = gBrowserMethods;
	if (initPB->browserMethods)
	{
		memset(initPB->browserMethods, 0, sizeof(EmBrowserMethods));
		WidgetVersion(&initPB->browserMethods->majorVersion,&initPB->browserMethods->minorVersion);
	}

	PROFILE_OPERA_POP_THEN_PUSH(3030);

#if defined(_MACINTOSH_)
	// This inits the Contents folder based on argv! not HGetVol
	if (gVendorDataID == 'OPRA')
	{
		EmQuickBrowserInitParameterBlock * quickdata = (EmQuickBrowserInitParameterBlock *)initPB->vendorData;
		// This needs to be called before the MacWidgetInitLibrary
		OpFileUtils::InitHomeDir(quickdata->argv[0]);

		// Init the command line argument manager
		if (OpStatus::IsError(CommandLineManager::GetInstance()->Init()))
			return emBrowserOutOfMemoryErr;
	}

	RETURN_IF_ERROR(MacWidgetInitLibrary(initPB));

	if(gVendorDataID != 'OPRA')
	{
		OpFileUtils::InitHomeDir(NULL);
	}

	OP_STATUS ret_val = OpStatus::OK;
#endif // _MACINTOSH_

	PROFILE_OPERA_POP_THEN_PUSH(3040);

	gInternalFontSwitching = !(initPB->initParams & emBrowserInitNoFontEnumeration);

	if (gVendorDataID == 'OPRA')
	{
		gEmbeddedRegistration = 12993590;
	}

#ifdef MSWIN

	/*HRESULT olestat = */OleInitialize(NULL);

	//Note: We need a window to handle our messages; this function should be split into platform specific versions
	BLDMainRegClass(hInst, TRUE);

	PROFILE_OPERA_POP_THEN_PUSH(3050);

	g_main_hwnd = BLDCreateWindow(hInst);

	PROFILE_OPERA_POP_THEN_PUSH(3060);

    WindowsOpPluginWindow::PluginRegClassDef(hInst);
#endif

#if defined(_INTELLIMOUSE_SUPPORT_) && defined(MSWIN)
	IntelliMouse_Init();
#endif

#ifdef _MACINTOSH_
#ifdef USE_COMMON_RESOURCES
	OpFileUtils::UpgradePreferencesFolder();	// For DSK-334964
	RETURN_IF_ERROR(g_desktop_bootstrap->Init());
#else
	TRAP(ret_val, SetFoldersL(info));
	RETURN_IF_ERROR(ret_val);

	// The preferences folder MUST exist before the preferences object is created otherwise
	// we are unable to write to the preferences file
	OpFileUtils::MakePath(info.default_folders[OPFILE_USERPREFS_FOLDER], info.default_folders[info.default_folder_parents[OPFILE_USERPREFS_FOLDER]]);
#endif // USE_COMMON_RESOURCES
#endif

	PROFILE_OPERA_POP_THEN_PUSH(3070);

#ifndef USE_COMMON_RESOURCES
	OpFile *userPrefs = NULL;

	// This writeLocation is used by embedded versions of Opera (i.e. Adobe) to put the preferences
	// for Opera where they want to. This should only be used from embedded products
	if(initPB->writeLocation != 0)
	{
		OpString writelocation;
		OpString newinifile;

		writelocation.Set((const uni_char*)initPB->writeLocation);

		newinifile.Set(writelocation);
		if(newinifile.FindLastOf(PATHSEPCHAR) != newinifile.Length())	//no ending pathsep
		{
			newinifile.Append(PATHSEP);
		}

		newinifile.Append(PREFERENCES_FILENAME);

		userPrefs = OP_NEW(OpFile, ());

		TRAPD(err, userPrefs->Construct(newinifile.CStr()));

		PrefsFile* tmp_prefsfile = OP_NEW(PrefsFile, (PREFS_STD));
		TRAP(ret_val, tmp_prefsfile->ConstructL());
		TRAP(ret_val, tmp_prefsfile->SetFileL(userPrefs));
		TRAP(ret_val, tmp_prefsfile->WriteStringL(UNI_L("User Prefs"), UNI_L("Opera Directory"), writelocation.CStr()));
		OP_DELETE(tmp_prefsfile);
	}
#endif // !USE_COMMON_RESOURCES

	PROFILE_OPERA_POP_THEN_PUSH(3080);

#ifdef MSWIN
#if defined( WINDOWS_PROFILE_SUPPORT )
	if(gVendorDataID != 'OPRA')
	{
		// Set spawn path so that GetExePath() will return the path of opera.dll
		OpString dllpath;
		if(dllpath.Reserve(MAX_PATH))
		{
			if(GetModuleFileName(hInst, dllpath.CStr(), MAX_PATH) > 0)
			{
				g_spawnPath.Set(dllpath);
			}
		}
	}

	OpString inifile;
	WindowsProfileManager* profilemanager = OP_NEW(WindowsProfileManager, (FALSE, &inifile));

	PROFILE_OPERA_POP_THEN_PUSH(3081);

	RETURN_IF_ERROR(profilemanager->Init());

	if(NULL == userPrefs)
	{
		userPrefs = profilemanager->GetUserOperaIni();
	}

	PROFILE_OPERA_POP_THEN_PUSH(3082);

	profilemanager->SetFoldersL(info); // added because info wasn't initialized

	OP_DELETE(profilemanager);

	PROFILE_OPERA_POP_THEN_PUSH(3083);

#endif //WINDOWS_PROFILE_SUPPORT

#else
#ifndef USE_COMMON_RESOURCES
	userPrefs = OP_NEW(OpFile, ());
	if (userPrefs) {
#ifdef _MACINTOSH_
		if (gVendorDataID != 'OPRA') {
			// Build the OpFile path (we have to do this because the OpFolderManager is not setup yet)
			OpString embeddedinipath;
			RETURN_IF_ERROR(embeddedinipath.SetConcat(info.default_folders[OPFILE_USERPREFS_FOLDER], PREFERENCES_FILENAME UNI_L(" Embedded")));
			RETURN_IF_ERROR(userPrefs->Construct(embeddedinipath, OPFILE_ABSOLUTE_FOLDER));
		}
		else
		{
			// Build the OpFile path (we have to do this because the OpFolderManager is not setup yet)
			OpString opfileinipath;
			RETURN_IF_ERROR(opfileinipath.SetConcat(info.default_folders[OPFILE_USERPREFS_FOLDER], PREFERENCES_FILENAME));
			RETURN_IF_ERROR(userPrefs->Construct(opfileinipath, OPFILE_ABSOLUTE_FOLDER));

			BOOL exists = FALSE;
			userPrefs->Exists(exists);
			if (!exists)
			{
				OpFile src_file;
				OpString opfilealtinipath;
				BOOL exists;
				OpString lang_folder;
				
				// Build the language folder since it's tricky and you only want to do it once
				lang_folder.Set(info.default_folders[info.default_folder_parents[OPFILE_LANGUAGE_FOLDER]]);
				lang_folder.Append(info.default_folders[OPFILE_LANGUAGE_FOLDER]);
				
				RETURN_IF_ERROR(opfilealtinipath.SetConcat(info.default_folders[OPFILE_LANGUAGE_FOLDER], UNI_L("Opera6.ini")));
				RETURN_IF_ERROR(src_file.Construct(opfilealtinipath, OPFILE_ABSOLUTE_FOLDER));

				src_file.Exists(exists);
				if (!exists) {
					RETURN_IF_ERROR(opfilealtinipath.SetConcat(lang_folder.CStr(), UNI_L("Opera6.ini")));
					RETURN_IF_ERROR(src_file.Construct(opfilealtinipath, OPFILE_ABSOLUTE_FOLDER));
				}

				src_file.Exists(exists);
				if (exists)
					userPrefs->CopyContents(&src_file, FALSE);
			}
		}
#else
		TRAP(ret_val, userPrefs->ConstructL(kUserPreferencesFolder, PREFERENCES_FILENAME));
#endif
		if(OpStatus::IsError(ret_val)) {
			OP_DELETE(userPrefs);
			userPrefs = NULL;
		}
	}
#endif // !USE_COMMON_RESOURCES
#endif //MSWIN

	PROFILE_OPERA_POP_THEN_PUSH(3090);

#ifndef USE_COMMON_RESOURCES
	// Global ini file
	OpFile *globaliniopfile = NULL; //rootPrefs
	{
		OpString globalinipath;
#ifndef _MACINTOSH_
		RETURN_IF_ERROR(globalinipath.SetConcat(info.default_folders[OPFILE_GLOBALPREFS_FOLDER], UNI_L(PATHSEP), GLOBAL_PREFERENCES));
#else
		RETURN_IF_ERROR(globalinipath.SetConcat(info.default_folders[OPFILE_GLOBALPREFS_FOLDER], GLOBAL_PREFERENCES));
#endif

		globaliniopfile = OP_NEW(OpFile, ());
		RETURN_IF_ERROR(globaliniopfile->Construct(globalinipath, OPFILE_ABSOLUTE_FOLDER));
	}

	// Fixed ini file
	OpFile *fixediniopfile = NULL; //fixedRootPrefs
	{
		OpString fixedinipath;
#ifndef _MACINTOSH_
		RETURN_IF_ERROR(fixedinipath.SetConcat(info.default_folders[OPFILE_FIXEDPREFS_FOLDER], UNI_L(PATHSEP), FIXED_PREFERENCES));
#else
		RETURN_IF_ERROR(fixedinipath.SetConcat(info.default_folders[OPFILE_FIXEDPREFS_FOLDER], FIXED_PREFERENCES));
#endif

		fixediniopfile =  OP_NEW(OpFile, ());
		RETURN_IF_ERROR(fixediniopfile->Construct(fixedinipath, OPFILE_ABSOLUTE_FOLDER));
	}

	OP_STATUS op_err = OpStatus::OK;

	// Create the PrefsFile object
	OpStackAutoPtr<PrefsFile> pf(OP_NEW(PrefsFile, (PREFS_INI)));
	if (!pf.get()) return OpStatus::ERR_NO_MEMORY;
	TRAP(op_err, pf->ConstructL());
	RETURN_IF_ERROR(op_err);
	TRAP(op_err, pf->SetFileL(userPrefs));
		// SetFileL creates a copy. Delete local.
	OP_DELETE(userPrefs);
	userPrefs = NULL;
	RETURN_IF_ERROR(op_err);
	TRAP(op_err, pf->SetGlobalFileL(globaliniopfile));
		// SetGlobalFileL creates a copy. Delete local.
	OP_DELETE(globaliniopfile);
	globaliniopfile = NULL;
	RETURN_IF_ERROR(op_err);
	TRAP(op_err, pf->SetGlobalFixedFileL(fixediniopfile));
		// SetGlobalFixedFileL creates a copy. Delete local.
	OP_DELETE(fixediniopfile);
	fixediniopfile = NULL;
	RETURN_IF_ERROR(op_err);

#ifdef _MACINTOSH_
	// This Mac section is to make sure that we are not referencing an old language file.

	/* PrefsManager 3 requires StyleManager to be created.  However,
	 * StyleManager needs to read a preference on startup.  We solve this by
	 * reading manually from the PrefsFile here.  Loading it earlier does not
	 * induce a penalty, since it will be cached when calling
	 * PrefsManager::ReadAllPrefsL() later. */
	TRAPD(err, pf->LoadAllL());

	OpString saved_language_file_path;

	TRAP(err, pf->ReadStringL("User Prefs", "Language File", saved_language_file_path));
	if( !saved_language_file_path.IsEmpty())
	{
		INT32 version = -1;

		OpFile file;
		file.Construct(saved_language_file_path);
		if (OpStatus::IsSuccess(file.Open(OPFILE_READ)))
		{
			for( INT32 i=0; i<100; i++ )
			{
				OpString8 buf;
				if( OpStatus::IsError(file.ReadLine(buf)) )
					break;
				buf.Strip(TRUE, FALSE);
				if (buf.CStr() && strncasecmp(buf.CStr(), "DB.version", 10) == 0)
				{
					for (const char *p = &buf.CStr()[10]; *p; p++)
					{
						if (isdigit(*p))
						{
							sscanf(p,"%d",&version);
							break;
						}
					}
					break;
				}
			}
		}

		// If the language file is less than version 811 throw it away!
		if (version < 811)
		{
			TRAP(err, pf->DeleteKeyL("User Prefs", "Language File"));
			TRAP(err, pf->CommitL(TRUE, FALSE)); // Keep data loaded
		}
	}
	
	// Save the speed dial file name for later use
	OpString	speeddial_filename;

	// Get the speeddial filename
	TRAP(err, pf->ReadStringL("User Prefs", "Speed Dial File", speeddial_filename));
	if (speeddial_filename.IsEmpty())
		speeddial_filename.Set(UNI_L("speeddial.ini"));

	// Translate any ini filenames and fix prefs
	FileUtils::TranslateIniFilenames(pf.get(), &info);
#endif // _MACINTOSH_
	
# ifdef PREFS_READ
	info.prefs_reader = pf.release();
# endif

	PROFILE_OPERA_POP_THEN_PUSH(3100);

#ifdef _MACINTOSH
	TRAP(ret_val, cacheFolder.ConstructL(kCacheFolder));
#endif //_MACINTOSH
#endif // !USE_COMMON_RESOURCES

#ifdef MSWIN
	PROFILE_OPERA_POP_THEN_PUSH(3110);

	g_desktop_bootstrap->GetInitInfo()->def_lang_file_folder
			= OPFILE_LANGUAGE_FOLDER;
	g_desktop_bootstrap->GetInitInfo()->def_lang_file_name
			= UNI_L("english.lng");
	
	// needed before OnCreateMainWindow
	TRAPD(init_error, g_opera->InitL(*g_desktop_bootstrap->GetInitInfo()));
	if (OpStatus::IsError(init_error))
	{
		return 0;
	}

	PROFILE_OPERA_POP_THEN_PUSH(3120);

	OnCreateMainWindow();

	PROFILE_OPERA_POP_THEN_PUSH(3130);

	InstallMSWinMsgHooks();

	PROFILE_OPERA_POP_THEN_PUSH(3140);

#else // MSWIN

# ifndef _MACINTOSH_
	TRAP(ret_val, InitializeL());
# else

#ifndef USE_COMMON_RESOURCES
	// Setup the default language based on what the system is currently set to
	// Hackish
	default_language.Set(GetDefaultLanguage());
	default_language.Append(".lng");
	info.def_lang_file_folder = OPFILE_LANGUAGE_FOLDER;
	info.def_lang_file_name = default_language.CStr();

	OpString 	filename, app_support_opera_folder;
	uni_char	*sep = 0;
	OpFile 		bookmark_file;
	BOOL		exists;

	// Create all the prefs folders that we need if they don't exist
	OpFileUtils::MakePath(info.default_folders[OPFILE_KEYBOARDSETUP_FOLDER], info.default_folders[info.default_folder_parents[OPFILE_KEYBOARDSETUP_FOLDER]]);
	OpFileUtils::MakePath(info.default_folders[OPFILE_MOUSESETUP_FOLDER], info.default_folders[info.default_folder_parents[OPFILE_MOUSESETUP_FOLDER]]);
	OpFileUtils::MakePath(info.default_folders[OPFILE_TOOLBARSETUP_FOLDER], info.default_folders[info.default_folder_parents[OPFILE_TOOLBARSETUP_FOLDER]]);
	OpFileUtils::MakePath(info.default_folders[OPFILE_SESSION_FOLDER], info.default_folders[info.default_folder_parents[OPFILE_SESSION_FOLDER]]);
	OpFileUtils::MakePath(info.default_folders[OPFILE_IMAGE_FOLDER], info.default_folders[info.default_folder_parents[OPFILE_IMAGE_FOLDER]]);
	OpFileUtils::MakePath(info.default_folders[OPFILE_SKIN_FOLDER], info.default_folders[info.default_folder_parents[OPFILE_SKIN_FOLDER]]);
	OpFileUtils::MakePath(info.default_folders[OPFILE_GADGET_FOLDER], info.default_folders[info.default_folder_parents[OPFILE_GADGET_FOLDER]]);

	// This is a bit YUK but oh well
	// Grab the mail folder.
	app_support_opera_folder.Set(info.default_folders[OPFILE_MAIL_FOLDER]);
	// Rip off the mail part
	if ((app_support_opera_folder.Length() > 0) && (app_support_opera_folder[app_support_opera_folder.Length() - 1] == PATHSEPCHAR))
		app_support_opera_folder[app_support_opera_folder.Length() - 1] = 0;
	if ((sep = uni_strrchr(app_support_opera_folder.CStr(), PATHSEPCHAR)) != 0)
		*(sep + 1) = 0;
	OpFileUtils::MakePath(app_support_opera_folder);

#ifdef M2_SUPPORT
	OpFile mail_folder;

	// Does a new mail folder exist?
	mail_folder.Construct(info.default_folders[OPFILE_MAIL_FOLDER]);
	if (OpStatus::IsSuccess(mail_folder.Exists(exists)) && !exists)
	{
		OpString old_mail_folder_name;
		OpFile 	 old_mail_folder;

		// Construct a version 8 or less mail folder
		old_mail_folder_name.Set(info.default_folders[OPFILE_USERPREFS_FOLDER]);
		TRAP(ret_val, OpFileUtils::AppendFolderL(old_mail_folder_name, UNI_L("Mail")));

		// Does version 8 or less mail folder exist?
		old_mail_folder.Construct(old_mail_folder_name);
		if (OpStatus::IsSuccess(old_mail_folder.Exists(exists)) && exists)
		{
			// Yes, so MOVE the folder to the new location
			DesktopOpFileUtils::Move(&old_mail_folder, &mail_folder);
		}
		else
		{
			// No, so just create the new folder
			OpFileUtils::MakePath(info.default_folders[OPFILE_MAIL_FOLDER], info.default_folders[info.default_folder_parents[OPFILE_MAIL_FOLDER]]);
		}
	}
#endif // M2_SUPPORT

	OpString lang_folder;

	// Build the language folder since it's tricky and you only want to do it once
	lang_folder.Set(info.default_folders[info.default_folder_parents[OPFILE_LANGUAGE_FOLDER]]);
	lang_folder.Append(info.default_folders[OPFILE_LANGUAGE_FOLDER]);
	
	// Copy over default bookmark file if there are no bookmarks
	filename.Set(info.default_folders[OPFILE_USERPREFS_FOLDER]);
	filename.Append(DEFAULT_BOOKMARKS_FILE);

	bookmark_file.Construct(filename);
	if (OpStatus::IsSuccess(bookmark_file.Exists(exists)) && !exists)
	{
		OpFile default_bookmark_file;

		filename.Set(lang_folder.CStr());
		filename.Append(UNI_L("default.adr"));

		default_bookmark_file.Construct(filename);
		if (OpStatus::IsSuccess(default_bookmark_file.Exists(exists)) && !exists)
		{
			filename.Set(info.default_folders[OPFILE_RESOURCES_FOLDER]);
			filename.Append(UNI_L("default.adr"));

			default_bookmark_file.Construct(filename);
		}
		if (OpStatus::IsSuccess(default_bookmark_file.Exists(exists)) && exists)
			bookmark_file.CopyContents(&default_bookmark_file, FALSE);
	}
	PROFILE_OPERA_POP_THEN_PUSH(3150);
	
	OpFile speeddial_default_file;

	// Copy over the speeddial_default.ini file if it's there for the specified local
	filename.Set(info.default_folders[OPFILE_USERPREFS_FOLDER]);
	filename.Append(speeddial_filename.CStr());

	speeddial_default_file.Construct(filename);
	if (OpStatus::IsSuccess(speeddial_default_file.Exists(exists)) && !exists)
	{
		OpFile default_speeddial_default_file;
		
		filename.Set(lang_folder.CStr());
		filename.Append(UNI_L("speeddial_default.ini"));
		
		default_speeddial_default_file.Construct(filename);
		if (OpStatus::IsSuccess(default_speeddial_default_file.Exists(exists)) && !exists)
		{
			filename.Set(info.default_folders[OPFILE_RESOURCES_FOLDER]);
			filename.Append(UNI_L("speeddial_default.ini"));
			
			default_speeddial_default_file.Construct(filename);
		}
		if (OpStatus::IsSuccess(default_speeddial_default_file.Exists(exists)) && exists)
			speeddial_default_file.CopyContents(&default_speeddial_default_file, FALSE);
	}
#endif // !USE_COMMON_RESOURCES
	
#ifdef _DEBUG
	uni_char* debug_result = Debug::InitSettings("../../platforms/mac/debug/debug.txt");
	if (debug_result)
		OP_DELETEA(debug_result);
#endif
	
	TRAPD(init_error, g_opera->InitL(*g_desktop_bootstrap->GetInitInfo()));
	if (OpStatus::IsError(init_error))
	{
		return 0;
	}

	PROFILE_OPERA_POP_THEN_PUSH(3160);

# endif
	if(OpStatus::IsMemoryError(ret_val)) {
		return emBrowserOutOfMemoryErr;
	} else if(OpStatus::IsError(ret_val)) {
		return emBrowserGenericErr;
	}
#endif // MSWIN

#if defined(EMBEDDED_DEFAULT_PREFS_FIX)
	if (gVendorDataID != 'OPRA')
	{
		OP_STATUS rc;
		gEmbeddedDefaults.trustMIMETypes = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::TrustServerTypes) ? true : false;
		if(gEmbeddedDefaults.trustMIMETypes)
		{
			g_pcnet->WriteIntegerL(PrefsCollectionNetwork::TrustServerTypes, 0);
		}

#ifdef _MACINTOSH_
		gEmbeddedDefaults.userAgentId = g_uaManager->GetUABaseId();
		if((gEmbeddedDefaults.userAgentId != UA_MSIE))
		{
			TRAP(rc, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UABaseId, UA_MSIE));
		}
#endif //_MACINTOSH_

		TRAP(rc, g_pcui->WriteIntegerL(PrefsCollectionUI::AddressbarAlignment, 0));
		TRAP(rc, g_pcui->WriteIntegerL(PrefsCollectionUI::NavigationbarAlignment, 0));
		TRAP(rc, g_pcui->WriteIntegerL(PrefsCollectionUI::ProgressPopup, 2));
		if (initPB->initParams & emBrowserInitDisableAllDrags)
			TRAP(rc, g_pcui->WriteIntegerL(PrefsCollectionUI::EnableDrag, 0));
		else if (initPB->initParams & emBrowserInitDisableOtherDrags)
			TRAP(rc, g_pcui->WriteIntegerL(PrefsCollectionUI::EnableDrag, DRAG_LOCATION | DRAG_LINK));
		else if (initPB->initParams & emBrowserInitDisableLinkDrags)
			TRAP(rc, g_pcui->WriteIntegerL(PrefsCollectionUI::EnableDrag, DRAG_LOCATION | DRAG_BOOKMARK));
		else
			TRAP(rc, g_pcui->WriteIntegerL(PrefsCollectionUI::EnableDrag, DRAG_ALL&(~DRAG_SAMEWINDOW)));
		TRAP(rc, g_pccore->WriteIntegerL(PrefsCollectionCore::EnableGesture, !(initPB->initParams & emBrowserInitDisableMouseGesures)));
		TRAP(rc, g_pcdoc->WriteIntegerL(PrefsCollectionDoc::ShowScrollbars, !(initPB->initParams & emBrowserInitNoScrollbars)));
		TRAP(rc, g_pcui->WriteIntegerL(PrefsCollectionUI::AllowContextMenus, !(initPB->initParams & emBrowserInitNoContextMenu)));
		TRAP(rc, g_pcdisplay->WriteIntegerL(PrefsCollectionDisplay::AutomaticSelectMenu, !(initPB->initParams & emBrowserInitDisableHotclickMenu)));

		// Ensures that right clicks also request the focus from the host application
		TRAP(rc, g_pcdisplay->WriteIntegerL(PrefsCollectionDisplay::AllowScriptToReceiveRightClicks, TRUE));

#if !defined(_NO_MAIL_PREFS_)
		TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::MailHandler, MAILHANDLER_SYSTEM));
		TRAP(err, g_pcapp->WriteIntegerL(PrefsCollectionApp::ExtAppParamSpaceSubst, FALSE));
#endif
		// never load favicon.ico
		TRAP(rc, g_pcdoc->WriteIntegerL(PrefsCollectionDoc::AlwaysLoadFavIcon, 0));

		// flush cache on exit, embrowser instances shouldn't require persistent cache
		// add setting to EmBrowserInitParams if necessary
		TRAP(rc, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::EmptyCacheOnExit, TRUE));

		TRAP(rc, g_pccore->WriteIntegerL(PrefsCollectionCore::AllowComponentsInUAStringComment, TRUE));
	}
#endif //EMBEDDED_DEFAULT_PREFS_FIX

	PROFILE_OPERA_POP_THEN_PUSH(3200);

#if defined(_MACINTOSH_)

	gMouseCursor = OP_NEW(MouseCursor, ());

	// start autosaving windows
	dont_autosave = false;

	// Boot() is called in OnCreateMainWindow on Windows
	RETURN_VALUE_IF_ERROR(g_desktop_bootstrap->Boot(), emBrowserGenericErr);

	OpFile wand_file;
	g_pcfiles->GetFileL(PrefsCollectionFiles::WandFile, wand_file);
	g_wand_manager->Open(wand_file, TRUE);

	PROFILE_OPERA_POP_THEN_PUSH(3210);

	MacPostInit();

	PROFILE_OPERA_POP_THEN_PUSH(3220);

	// Attempt to start Java VM before RAEL. Not successful so far,therefore commented away.
	//OP_STATUS stat = g_JavaVM->CreateVM();

#endif //_MACINTOSH_

	PROFILE_OPERA_POP(); // 3220
	PROFILE_OPERA_POP(); // 3000
	
	return emBrowserNoErr;
}

#if defined(_MACINTOSH_)
# if defined(__MWERKS__)
#  pragma export off
# else
#  pragma GCC visibility pop
# endif
#endif //_MACINTOSH_

#ifdef WIN32
__declspec(dllexport)
int TranslateOperaMessage(MSG* pmsg)
{
	return g_windows_message_loop->TranslateOperaMessage(pmsg);
}

#endif //WIN32

#ifdef __cplusplus
}
#endif

#endif // EMBROWSER_SUPPORT
