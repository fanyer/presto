#include "platforms/mac/QuickOperaApp/CocoaApplication.h"
#include "platforms/mac/QuickOperaApp/QuickWidgetLibHandler.h"
#include "platforms/mac/QuickOperaApp/QuickWidgetManager.h"
#include <string.h>
#include "platforms/mac/embrowser/EmBrowserInit.h"
#include "platforms/mac/bundledefines.h"

#include "adjunct/quick/managers/LaunchManager.h"

#include "platforms/crashlog/crashlog.h"

#if defined(_AM_DEBUG)
#include "platforms/mac/debug/anders/settings.anders.h"
#endif
#ifdef WIDGET_RUNTIME_SUPPORT
BOOL IsBrowserStartup();
CFStringRef GetOperaBundleID();
#endif 
QuickWidgetLibHandler *gWidgetLibHandler = NULL;

void StrFilterOutChars(char *szArg, const char* szCharsToRemove)
{
	if( szArg && *szArg)
	{
		int i = 0;
		int j = 0;

		while( szArg[i])
		{
			if( !strchr( szCharsToRemove, szArg[i]))
				szArg[j++] = szArg[i++];
			else
				i++;
		}

		szArg[j] = 0;
	}
}


QuickWidgetLibHandler::QuickWidgetLibHandler(int argc, const char** argv)
{
	Pool swim;

	initProc = NULL;
	success = false;

	memset(&commonParams, 0, sizeof(commonParams));
	memset(&quickParams, 0, sizeof(quickParams));

	commonParams.majorVersion = EMBROWSER_MAJOR_VERSION;
	commonParams.minorVersion = EMBROWSER_MINOR_VERSION;

	quickParams.majorVersion = EMBROWSER_MAJOR_VERSION;
	quickParams.minorVersion = EMBROWSER_MINOR_VERSION;

	commonParams.malloc = NULL;
	commonParams.realloc = NULL;
	commonParams.free = NULL;

	commonParams.vendorDataID = 'OPRA';
	char primaryKey[]= KEY_PRIMARY_NAME;
	StrFilterOutChars( primaryKey, KEY_STRIPCHARS);
	strcpy((char*)quickParams.registrationCode,primaryKey);
	memset(primaryKey,0,sizeof(primaryKey));

	commonParams.vendorData = &quickParams;
	commonParams.vendorDataMajorVersion = 2;
	commonParams.vendorDataMinorVersion = 0;

	commonParams.notifyCallbacks = new EmBrowserAppNotification();
	if (commonParams.notifyCallbacks)
	{
		memset(commonParams.notifyCallbacks, 0, sizeof(EmBrowserAppNotification));

		commonParams.notifyCallbacks->majorVersion = EMBROWSER_MAJOR_VERSION;
		commonParams.notifyCallbacks->minorVersion = EMBROWSER_MINOR_VERSION;

		commonParams.notifyCallbacks->notification = QuickWidgetManager::Notification;
	}

	// Save the command line options
	quickParams.argv = argv;
	quickParams.argc = argc;

	quickParams.appProcs = new EmQuickBrowserAppProcs();
	if (quickParams.appProcs)
	{
		memset(quickParams.appProcs, 0, sizeof(EmQuickBrowserAppProcs));

		quickParams.appProcs->majorVersion	= EMBROWSER_MAJOR_VERSION;
		quickParams.appProcs->minorVersion	= EMBROWSER_MINOR_VERSION;

		quickParams.appProcs->createMenu	= QuickWidgetManager::CreateQuickMenu;
		quickParams.appProcs->disposeMenu	= QuickWidgetManager::DisposeQuickMenu;
		quickParams.appProcs->addMenuItem	= QuickWidgetManager::AddQuickMenuItem;
		quickParams.appProcs->removeMenuItem= QuickWidgetManager::RemoveQuickMenuItem;
		quickParams.appProcs->insertMenu	= QuickWidgetManager::InsertQuickMenuInMenuBar;
		quickParams.appProcs->removeMenu	= QuickWidgetManager::RemoveQuickMenuFromMenuBar;

		quickParams.appProcs->refreshMenu	= QuickWidgetManager::RefreshMenu;
		// These 2 aren't used in the DLL. Might as well save the overhead.
//		quickParams.appProcs->refreshMenuBar= QuickWidgetManager::RefreshMenuBar;
//		quickParams.appProcs->refreshCommand= QuickWidgetManager::RefreshCommand;

		quickParams.appProcs->removeMenuItems = QuickWidgetManager::RemoveAllQuickMenuItemsFromMenu;

		quickParams.appProcs->installSighandlers = InstallCrashSignalHandler;
	}

#ifdef AUTO_UPDATE_SUPPORT
	// Set the Launch Manager
	quickParams.launch_manager = g_launch_manager;
#endif
	
	// Set the address of the GPU info
	extern GpuInfo *g_gpu_info;
	quickParams.pGpuInfo = g_gpu_info;

	// try to locate the widget.
#if defined(_XCODE_)
	CFBundleRef bundle = CFBundleGetMainBundle(), opera_framework_bundle = NULL;
	// See comment in LoadOperaFramework to understand why this is here
	opera_framework_bundle = QuickWidgetLibHandler::LoadOperaFramework(bundle);
#ifdef WIDGET_RUNTIME_SUPPORT
   	// This code looks for an Opera.framework inside the Frameworks folder in the parent bundle. This is intended to make Widgetinstaller run with the framework it was built with.
	if (!opera_framework_bundle)
	{
		CFURLRef bundleURL = CFBundleCopyBundleURL(bundle);
		if (bundleURL) {
			bundle = CFBundleCreate(kCFAllocatorSystemDefault, bundleURL);
			if (bundle) {
				CFURLRef bundle_frameworks_url = CFBundleCopyPrivateFrameworksURL(bundle);
				CFRelease(bundle);
				if (bundle_frameworks_url) {
					CFURLRef opera_framework_url = CFURLCreateCopyAppendingPathComponent(kCFAllocatorSystemDefault, bundle_frameworks_url, CFSTR("Opera.framework"), false);
					CFRelease(bundle_frameworks_url);
					if (opera_framework_url)
					{
						opera_framework_bundle = CFBundleCreate(kCFAllocatorSystemDefault, opera_framework_url);
						CFRelease(opera_framework_url);
					}
				}
			}
			CFRelease(bundleURL);
		}
	}
    // It is possible that we won't be able to locate framework, because we are inside of the widget bundle (without Opera.framework
    // In that case we have to locate framework outside and then run it
    if (!opera_framework_bundle)
	{
		// This is the bundleID that is in the widget so we should go for first
		CFURLRef bundleURL = NULL;
		LSFindApplicationForInfo(kLSUnknownCreator,GetOperaBundleID(), NULL, NULL, &bundleURL);
		
		CFBundleRef bundle = CFBundleCreate(kCFAllocatorSystemDefault, bundleURL);
		if (!bundle)
		{
			// Didn't find the targeted Framework so go for the fallbacks Opera then Opera Next
			LSFindApplicationForInfo(kLSUnknownCreator,CFSTR(XSTRINGIFY(OPERA_BUNDLE_ID)), NULL, NULL, &bundleURL);
			
			bundle = CFBundleCreate(kCFAllocatorSystemDefault, bundleURL);
			if (!bundle)
			{
				// Look for Opera Next
				LSFindApplicationForInfo(kLSUnknownCreator,CFSTR(XSTRINGIFY(OPERA_NEXT_BUNDLE_ID)), NULL, NULL, &bundleURL);
			
				bundle = CFBundleCreate(kCFAllocatorSystemDefault, bundleURL);
			}
		}

		CFURLRef operaurl = CFURLCreateCopyAppendingPathComponent(kCFAllocatorSystemDefault, bundleURL, CFSTR("Contents/MacOS/Opera"), false);
		char* path = new char[PATH_MAX];
		CFStringRef oppath = NULL;
		if (CFURLGetFileSystemRepresentation(bundleURL, true, (UInt8*)path, PATH_MAX))
			oppath = CFStringCreateWithBytes(kCFAllocatorSystemDefault, (const UInt8*)path, strlen(path), kCFStringEncodingUTF8, false);

		CFURLGetFileSystemRepresentation(operaurl, true, (UInt8*)path, PATH_MAX);
		CFRelease(operaurl);
		quickParams.opera_bundle = bundleURL;
		quickParams.argv[0] = path;

		CFURLRef bundle_frameworks_url = CFBundleCopyPrivateFrameworksURL(bundle);
        CFRelease(bundle);

		if (bundle_frameworks_url)
	    {
			// If we are here - we need to check one more thing. We are linking to the external Frameworks folder. We need to create a link in our local folder
			// (Frameworks folder) to Growl.framework. This is somewhat crucial so we can start the stuff up
			CFURLRef opera_framework_url = CFURLCreateCopyAppendingPathComponent(kCFAllocatorSystemDefault, bundle_frameworks_url, CFSTR("Opera.framework"), false);
            CFRelease(bundle_frameworks_url);
			if (opera_framework_url)
			{
				opera_framework_bundle = CFBundleCreate(kCFAllocatorSystemDefault, opera_framework_url);
				if (opera_framework_bundle)
				{
					if (!CFBundleIsExecutableLoaded(opera_framework_bundle))
						CFBundleLoadExecutable(opera_framework_bundle);
				}
				CFRelease(opera_framework_url);
			}
        }
	}
#endif //WIDGET_RUNTIME_SUPPORT
	if (opera_framework_bundle)
	{
		EmBrowserInitLibraryProc widgetInitLibrary = (EmBrowserInitLibraryProc) CFBundleGetFunctionPointerForName(opera_framework_bundle, CFSTR("WidgetInitLibrary"));
		if (widgetInitLibrary)
		{
			success = (emBrowserNoErr == widgetInitLibrary(&commonParams));
		}
		CFRelease(opera_framework_bundle);
	}
#else
	success = (emBrowserNoErr == WidgetInitLibrary(&commonParams));
#endif
}

QuickWidgetLibHandler::~QuickWidgetLibHandler()
{
	if (commonParams.shutdown)
	{
		commonParams.shutdown();
	}
	delete commonParams.notifyCallbacks;
	delete quickParams.appProcs;
}

Boolean QuickWidgetLibHandler::Initialized()
{
	return success;
}

EmBrowserStatus QuickWidgetLibHandler::CallStartupQuickMethod(int argc, const char** argv)
{
	if (quickParams.libProcs && quickParams.libProcs->startupQuick)
	{
		return quickParams.libProcs->startupQuick(argc, argv);
	}
	return emBrowserGenericErr;
}

EmBrowserStatus QuickWidgetLibHandler::CallStartupQuickSecondPhaseMethod(int argc, const char** argv)
{
	if (quickParams.libProcs && quickParams.libProcs->startupQuickSecondPhase)
	{
		return quickParams.libProcs->startupQuickSecondPhase(argc, argv);
	}
	return emBrowserGenericErr;
}

EmBrowserStatus QuickWidgetLibHandler::CallHandleQuickMenuCommandMethod(EmQuickMenuCommand command)
{
	if (quickParams.libProcs && quickParams.libProcs->handleQuickCommand)
	{
		return quickParams.libProcs->handleQuickCommand(command);
	}
	return emBrowserGenericErr;
}

EmBrowserStatus QuickWidgetLibHandler::CallGetQuickMenuCommandStatusMethod(EmQuickMenuCommand command, EmQuickMenuFlags &flags)
{
	if (quickParams.libProcs && quickParams.libProcs->getQuickCommandStatus)
	{
		return quickParams.libProcs->getQuickCommandStatus(command, &flags);
	}
	return emBrowserGenericErr;
}

EmBrowserStatus QuickWidgetLibHandler::CallRebuildQuickMenuMethod(EmQuickMenuRef menu)
{
	if (quickParams.libProcs && quickParams.libProcs->rebuildQuickMenu)
	{
		return quickParams.libProcs->rebuildQuickMenu(menu);
	}
	return emBrowserGenericErr;
}

EmBrowserStatus QuickWidgetLibHandler::CallSharedMenuHitMethod(UInt16 menuID, UInt16 menuItemIndex)
{
	if (quickParams.libProcs && quickParams.libProcs->sharedMenuHit)
	{
		return quickParams.libProcs->sharedMenuHit(menuID, menuItemIndex);
	}
	return emBrowserGenericErr;
}

EmBrowserStatus QuickWidgetLibHandler::CallUpdateMenuTracking(BOOL tracking)
{
	if (quickParams.libProcs && quickParams.libProcs->updateMenuTracking)
	{
		return quickParams.libProcs->updateMenuTracking(tracking);
	}
	return emBrowserGenericErr;
}

EmBrowserStatus QuickWidgetLibHandler::CallMenuItemHighlighted(CFStringRef item_title, BOOL main_menu, BOOL is_submenu)
{
	if (quickParams.libProcs && quickParams.libProcs->menuItemHighlighted)
	{
		return quickParams.libProcs->menuItemHighlighted(item_title, main_menu, is_submenu);
	}
	return emBrowserGenericErr;
}

EmBrowserStatus QuickWidgetLibHandler::CallSetActiveNSMenu(void *nsmenu)
{
	if (quickParams.libProcs && quickParams.libProcs->setActiveNSMenu)
	{
		return quickParams.libProcs->setActiveNSMenu(nsmenu);
	}
	return emBrowserGenericErr;
}

EmBrowserStatus QuickWidgetLibHandler::CallOnMenuShown(void *nsmenu, BOOL shown)
{
        if (quickParams.libProcs && quickParams.libProcs->onMenuShown)
	{
		return quickParams.libProcs->onMenuShown(nsmenu, shown);
	}
	return emBrowserGenericErr;
}

CFBundleRef QuickWidgetLibHandler::LoadOperaFramework(CFBundleRef main_bundle)
{
	// This steaming pile is sadly needed, because Xcode's linker doesn't know how to properly
	// link against a private framework. It tries, but when ZeroLink is off (i.e Release)
	// it will generate dependencies to symbols that cannot be loaded.
	// Additionally, it fails when it attemtemps to link a non-native binary against a
	// universal framework. I Hope we can get rid of this, once Apple makes a proper linker.
	
	// This code looks for an Opera.framework inside the Frameworks folder of the running application. This will fail for widgets and widgetinstaller.
	CFBundleRef bundle = main_bundle, opera_framework_bundle = NULL;
	CFURLRef bundle_frameworks_url = NULL, opera_framework_url = NULL;
	if (bundle)
	{
		bundle_frameworks_url = CFBundleCopyPrivateFrameworksURL(bundle);
		if (bundle_frameworks_url)
		{
			opera_framework_url = CFURLCreateCopyAppendingPathComponent(kCFAllocatorSystemDefault, bundle_frameworks_url, CFSTR("Opera.framework"), false);
			CFRelease(bundle_frameworks_url);
			if (opera_framework_url)
			{
				opera_framework_bundle = CFBundleCreate(kCFAllocatorSystemDefault, opera_framework_url);
				CFRelease(opera_framework_url);
			}
		}
	}
	
	return opera_framework_bundle;
}

#pragma mark -

Boolean QuickWidgetLibHandler::InitWidgetLibrary()
{
	if (initProc)
	{
		return (emBrowserNoErr == initProc(&commonParams));
	}
	return false;
}
