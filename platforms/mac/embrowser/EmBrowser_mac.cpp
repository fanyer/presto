/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"
#include <OpenGL/CGLContext.h>
#include "platforms/mac/embrowser/EmBrowser_mac.h"
#include "adjunct/embrowser/EmBrowser_main.h"
#include "adjunct/desktop_util/resources/pi/opdesktopproduct.h"
#include "platforms/mac/subclasses/MacWidgetPainter.h"
#include "platforms/mac/pi/MacOpPrinterController.h"
#include "platforms/mac/subclasses/MacWandDataStorage.h"
#include "platforms/mac/pi/other/MacOpSkinElement.h"
#include "platforms/mac/pi/desktop/CocoaFileChooser.h"
#include "platforms/mac/pi/CocoaOpWindow.h"
#include "platforms/mac/File/FileUtils_Mac.h"
# include "platforms/mac/model/CocoaOperaListener.h"
# include "platforms/mac/model/OperaApplicationDelegate.h"
#include "platforms/mac/Specific/Platform.h"
#ifdef NO_CARBON
#include "platforms/mac/quick_support/CocoaInternalQuickMenu.h"
#endif // NO_CARBON	

#include "platforms/mac/quick_support/MacHostQuickMenu.h"
#include "platforms/mac/quick_support/MacApplicationListener.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "platforms/mac/util/macutils.h"
#include "platforms/mac/util/WidgetFirstRunOperations.h"
#include "platforms/mac/model/AppleEventManager.h"
#include "platforms/mac/pi/resources/macopdesktopresources.h"
#include "platforms/mac/util/MacAsyncLoader.h"
#define Media MacMedia
#include "platforms/mac/model/MacSound.h"
#undef Media
#ifdef SUPPORT_OSX_SERVICES
#include "platforms/mac/model/MacServices.h"
#endif
#include "platforms/posix/posix_thread_util.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/quick/managers/LaunchManager.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"
#include "adjunct/desktop_util/sound/SoundUtils.h"
#include "adjunct/quick/Application.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"

#include "adjunct/quick/dialogs/CrashLoggingDialog.h"
#include "platforms/crashlog/crashlog.h"

#ifdef USE_COMMON_RESOURCES
#include "adjunct/desktop_util/resources/pi/opdesktopresources.h"
#endif // USE_COMMON_RESOURCES

#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

#include <mach-o/dyld.h>

#ifdef SELFTEST
#include "modules/selftest/optestsuite.h"
#endif

#ifdef USE_COMMON_RESOURCES
#include "adjunct/desktop_util/resources/ResourceDefines.h"
#endif // USE_COMMON_RESOURCES

#include "platforms/mac/util/CocoaPlatformUtils.h"
#include "platforms/mac/util/CTextConverter.h"
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>
#include "adjunct/quick/managers/SleepManager.h"
#include "platforms/posix_ipc/posix_ipc_component_platform.h"

#include "platforms/mac/util/OpenGLContextWrapper.h"

#include "modules/pi/OpThreadTools.h"

#include <pwd.h>
extern const char* GetContainerPath();
extern void TearDownNotifications();

CFURLRef gCFMWidgetLibURL = 0;
ThreadID gMainThreadID = 0;

EmBrowserStatus MacGetSubdocRect(EmDocumentRef doc,FramesDocument* inDoc, EmBrowserRect* outRect)
{
//	FIXME: Implement!
	return emBrowserGenericErr;
}

MacHostQuickMenuBar * gMenuBar = NULL;
OpThreadTools* g_thread_tools = NULL;
CocoaOperaListener* gCocoaOperaListener = NULL;
OperaApplicationDelegate* gOperaApplicationDelegate = NULL;
MacApplicationListener* g_appListener = NULL;
MacApplicationMenuListener* g_appMenuListener = NULL;
Boolean				gHandleKeyboardEvent;
extern OpWidgetInfo* g_MacWidgetInfo;
#ifndef NO_CARBON
EventQueueRef gMainEventQueue;
EventLoopRef gMainEventLoop;
#endif
EmQuickBrowserInitParameterBlock *gQuickBlock = NULL;
EmQuickBrowserAppProcs *gQuickAppMethods = NULL;
EmQuickBrowserLibProcs *gQuickLibMethods = NULL;
Boolean gIsRebuildingMenu = false;
#ifndef USE_COMMON_RESOURCES
BOOL g_first_run_after_install = FALSE;
#endif // !USE_COMMON_RESOURCES

AppMallocProc		extMalloc = 0;
AppFreeProc			extFree = 0;
AppReallocProc		extRealloc = 0;
Boolean				extMemInitialized = false;


#include "platforms/mac/util/MachOCompatibility.h"

#include <PMErrors.h>

AppleEventManager * gAppleEvents = NULL;

#ifdef _MAC_DEBUG
//# define _HTTP_ECHO_LISTENING_SOCKET_DEBUG
#endif


#ifdef _HTTP_ECHO_LISTENING_SOCKET_DEBUG
class Pingy : OpSocketListener
{
public:
	Pingy() : OpSocketListener(), m_socket(NULL)
	{
		if (OpStatus::IsSuccess(OpSocket::Create(&m_socket, OpSocket::TCP, this)))
		{
			OpSocketAddress *addy = NULL;
			if (OpStatus::IsSuccess(OpSocketAddress::Create(&addy)))
			{
				addy->SetPort(2008);
				m_socket->Listen(addy, 20);
			}
		}
	}
	~Pingy() {}
	void OnSocketConnected(OpSocket* socket) {}
	void OnSocketDataReady(OpSocket* socket) {
		char * buffer = new char[4096];
		UINT len = 0;
		UINT strnlen;
		strcpy(buffer, "HTTP/1.0 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length:        0\r\n\r\n");
		strnlen = strlen(buffer);
		socket->Recv(buffer+strnlen, 4096-strnlen, &len);
		if (len)
		{
			char lengthb[9];
			char* lenoff = strstr(buffer, "Content-Length:")+16;
			sprintf(lengthb, "%8d", len);
			memcpy(lenoff, lengthb, 8);
			socket->Send(buffer, strnlen+len);
		}
		delete [] buffer;
	}
	void OnSocketDataSent(OpSocket* socket, UINT bytes_sent) {
		printf("sent!\r");
	}
	void OnSocketClosed(OpSocket* socket) {
		delete socket;
		printf("closed!\r");
	}
#ifdef SOCKET_LISTEN_SUPPORT
	void OnSocketConnectionRequest(OpSocket* listening_socket) {
		OpSocket* socket = NULL;
		RETURN_VOID_IF_ERROR(OpSocket::Create(&(socket), OpSocket::TCP, this));
		listening_socket->Accept(socket);
	};
	void OnSocketListenError(OpSocket* socket, OpSocket::Error error) {printf("ERROR!\r");}
#endif
	void OnSocketConnectError(OpSocket* socket, OpSocket::Error error) {printf("ERROR!\r");}
	void OnSocketReceiveError(OpSocket* socket, OpSocket::Error error) {printf("ERROR!\r");}
	void OnSocketSendError(OpSocket* socket, OpSocket::Error error) {printf("ERROR!\r");}
	void OnSocketCloseError(OpSocket* socket, OpSocket::Error error) {printf("ERROR!\r");}
private:
	OpSocket* m_socket;
};
#endif // _HTTP_ECHO_LISTENING_SOCKET_DEBUG

OSStatus MyRegisterPDE (CFStringRef plugin)
{
	OSStatus result = kPMInvalidPDEContext;

	CFURLRef pluginsURL = CFBundleCopyBuiltInPlugInsURL (
		CFBundleGetMainBundle()
	);

	if (pluginsURL != NULL)
	{
		CFURLRef myPluginURL = CFURLCreateCopyAppendingPathComponent (
			NULL,
			pluginsURL,
			plugin,
			FALSE
		);

		if (myPluginURL != NULL)
		{
			// we intentionally discard the return value
			if (CFPlugInCreate (NULL, myPluginURL) != NULL) {
				result = noErr;
			}

			CFRelease (myPluginURL);
		}

		CFRelease (pluginsURL);
	}

	return result;
}


#ifdef QUICK
#pragma mark -
#pragma mark ----- QUICK -----
#pragma mark -

io_connect_t root_port = 0;

void SleepCallBack( void * refCon, io_service_t service, natural_t messageType, void * messageArgument )
{
// When selecting sleep from the Apple menu or closing a laptop, we will get this:
//	kIOMessageSystemWillSleep
//	...
//	kIOMessageSystemWillPowerOn
//	kIOMessageSystemHasPoweredOn

// When Idle sleep kicks in, we will get this
//	kIOMessageCanSystemSleep
//	kIOMessageSystemWillSleep
//	...
//	kIOMessageSystemWillPowerOn
//	kIOMessageSystemHasPoweredOn

// When idle sleep tries to kick in, but is denied by a running process, we will get this:
//	kIOMessageCanSystemSleep
//	kIOMessageSystemWillNotSleep

	switch ( messageType )
	{
		case kIOMessageCanSystemSleep:
			/*
			Idle sleep is about to kick in.
			Applications have a chance to prevent sleep by calling IOCancelPowerChange.
			Most applications should not prevent idle sleep.

			Power Management waits up to 30 seconds for you to either allow or deny idle sleep.
			If you don't acknowledge this power change by calling either IOAllowPowerChange
			or IOCancelPowerChange, the system will wait 30 seconds then go to sleep.
			*/

			if (g_sleep_manager->SystemCanGoToSleep())
				IOAllowPowerChange( root_port, (long)messageArgument );
			else
				IOCancelPowerChange( root_port, (long)messageArgument );
			break;

		case kIOMessageSystemWillSleep:
			/* The system WILL go to sleep. If you do not call IOAllowPowerChange or
				IOCancelPowerChange to acknowledge this message, sleep will be
			delayed by 30 seconds.

			NOTE: If you call IOCancelPowerChange to deny sleep it returns kIOReturnSuccess,
			however the system WILL still go to sleep.
			*/

			// we cannot deny forced sleep
			g_sleep_manager->Sleep();
			IOAllowPowerChange( root_port, (long)messageArgument );
			break;

		case kIOMessageSystemHasPoweredOn:
			g_sleep_manager->WakeUp();
			break;

		default:
			break;

	}
}


#pragma GCC visibility push(default)

extern "C" int RunComponent(const char *token)
{
	// Initialize TLS
	if (MacTLS::Init() != 0)
		return -1;

	// Start sub-process
	PosixIpcComponentPlatform* platform;
	if (OpStatus::IsError((PosixIpcComponentPlatform::Create(platform, token))))
		return -1;
	int retval = platform->Run();
	OP_DELETE(platform);

	return retval;
}

#pragma GCC visibility pop

EmBrowserStatus	StartupQuick(int argc, const char** argv)
{
	EM_BROWSER_FUNCTION_BEGIN

	MacOpSkinElement::Init();

	if (g_application)
	{
#ifdef SELFTEST
	TestSuite::main(&argc, (char**)argv);
#endif
		gAppleEvents = new AppleEventManager;

		return emBrowserNoErr;
	}
	return emBrowserOutOfMemoryErr;
}

EmBrowserStatus StartupQuickSecondPhase(int argc, const char** argv)
{
	EM_BROWSER_FUNCTION_BEGIN

 	Application::RunType firstRun = g_application->DetermineFirstRunType();

	if (firstRun == Application::RUNTYPE_FIRSTCLEAN)
	{
		WidgetFirstRunOperations firstRunOperations;
		firstRunOperations.setDefaultSystemFonts();
		firstRunOperations.importSafariBookmarks();
	}

#ifdef CRASHLOG_DEBUG
	fprintf(stderr, "opera [crashlog debug %d]: about to show dialog\n", time(NULL));
#endif
	// whether a crash log upload is intended
	BOOL show_upload_dlg = g_pcui->GetIntegerPref(PrefsCollectionUI::ShowCrashLogUploadDlg);
	CommandLineArgument *crash_log = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::CrashLog);

	if (crash_log)
	{
		if(show_upload_dlg)
		{
			// show crash report dialog
			BOOL continue_restart = FALSE;
			BOOL minimal_restart = FALSE;
			CrashLoggingDialog* dialog = OP_NEW(CrashLoggingDialog, (crash_log->m_string_value, continue_restart, minimal_restart));
			dialog->Init(NULL);
#ifdef CRASHLOG_DEBUG
			fprintf(stderr, "opera [crashlog debug %d]: dialog done, continue_restart is %d\n", time(NULL), continue_restart);
#endif
			if(!continue_restart)
			{
				g_desktop_global_application->Exit();
				return emBrowserExitingErr;
			}	
			if(!minimal_restart)
			{
				// Full restart requires reenabling HW acceleration, so restart Opera from scratch
				g_desktop_bootstrap->ShutDown();
				execl(argv[0], argv[0], NULL);
			}	
		}
		else 
		{
			g_desktop_global_application->Exit();
			return emBrowserExitingErr;
		}
		if (gQuickAppMethods->installSighandlers)
			gQuickAppMethods->installSighandlers();
	}

	// update speeddial for sandboxed environment
	// but only on condition that this file was in default Caches directory
	// otherwise it wont be migrated in container migration process.
	// however we should move it only for sandbox environment
	// which is made first time after upgrading.
	if (g_desktop_op_system_info->IsSandboxed() && (
		g_application->DetermineFirstRunType() == Application::RUNTYPE_FIRST_NEW_BUILD_NUMBER ||
		g_application->DetermineFirstRunType() == Application::RUNTYPE_FIRSTCLEAN ||
		g_application->DetermineFirstRunType() == Application::RUNTYPE_FIRST))
	{
		OpFile speeddial_file;
		BOOL speeddialfile_exists = FALSE;

		g_pcfiles->GetFileL(PrefsCollectionFiles::SpeedDialFile, speeddial_file);
		RETURN_VALUE_IF_ERROR(speeddial_file.Exists(speeddialfile_exists), emBrowserGenericErr);

		// if it doesn't exist ignore it
		if(speeddialfile_exists) {
			PrefsFile sd_file(PREFS_STD);
			OpString background_path;

			sd_file.ConstructL();
			sd_file.SetFileL(&speeddial_file);
			RETURN_VALUE_IF_ERROR(sd_file.LoadAllL(), emBrowserGenericErr);
			background_path.SetL(sd_file.ReadStringL("Background", "Filename"));

			CFStringRef non_sandboxed_path = CFStringCreateWithFormat(kCFAllocatorDefault,
			                                                          NULL,
			                                                          CFSTR("%s/Library/Caches/Opera%s/"),
			                                                          getpwuid(getuid())->pw_dir,
#ifdef _DEBUG
			                                                          " Debug"
#else
			                                                          g_desktop_product->GetProductType() == PRODUCT_TYPE_OPERA_NEXT ? " Next" : ""
#endif // _DEBUG
			                                                          );
			if (non_sandboxed_path == NULL) return emBrowserOutOfMemoryErr;
			char path[MAX_PATH];
			if (!CFStringGetCString(non_sandboxed_path, path, MAX_PATH, kCFStringEncodingUTF8)) return emBrowserGenericErr;
			if (background_path.Compare(path, CFStringGetLength(non_sandboxed_path)) == 0)
			{
				OpString new_path;
				RETURN_VALUE_IF_ERROR(new_path.Set(GetContainerPath()), emBrowserGenericErr);
				new_path.Append(background_path.SubString(strlen(getpwuid(getuid())->pw_dir)));
				background_path.Set(new_path);
				RETURN_VALUE_IF_ERROR(sd_file.WriteStringL("Background", "Filename", background_path), emBrowserGenericErr);
				sd_file.CommitL();
			}
		}
	}

	if (!CocoaPlatformUtils::IsGadgetInstallerStartup())
		g_main_message_handler->PostMessage(MSG_QUICK_APPLICATION_START, 0, 0);

	// This code is taken from http://developer.apple.com/qa/qa2004/qa1340.html
	IONotificationPortRef	notifyPortRef = NULL;	// notification port allocated by IORegisterForSystemPower
	io_object_t				notifierObject = 0;		// notifier object, used to deregister later
	// register to receive system sleep notifications
	root_port = IORegisterForSystemPower(NULL, &notifyPortRef, SleepCallBack, &notifierObject);
	if (root_port)
	{
		// add the notification port to the application runloop
		CFRunLoopAddSource(CFRunLoopGetCurrent(),
						IONotificationPortGetRunLoopSource(notifyPortRef),
						kCFRunLoopCommonModes);
	}

	OpString soundfile;
	TRAPD(rc, g_pcui->GetStringPrefL(PrefsCollectionUI::StartSound, soundfile));
	OpStatus::Ignore(SoundUtils::SoundIt(soundfile, FALSE));

	KioskManager* kiosk = KioskManager::GetInstance();
	if (kiosk && kiosk->GetEnabled() && (kiosk->GetNoExit() || kiosk->GetNoKeys()))
	{
#ifdef NO_CARBON	
		DisableMenuShortcut(kHICommandHide);
#else
		MenuRef			applicationMenu;
		MenuItemIndex	outIndex;

		//Disable Command-H keyboard equivalent on the Hide menu
		OSStatus error = GetIndMenuItemWithCommandID(NULL, kHICommandHide, 1, &applicationMenu, &outIndex);
		if (error == noErr)
			SetItemCmd(applicationMenu, outIndex, 0);
#endif // NO_CARBON	
	}

#ifdef _HTTP_ECHO_LISTENING_SOCKET_DEBUG
	new Pingy;
#endif // _HTTP_ECHO_LISTENING_SOCKET_DEBUG

	return emBrowserNoErr;
}

EmBrowserStatus	HandleQuickMenuCommand(EmQuickMenuCommand command)
{
	EM_BROWSER_FUNCTION_BEGIN

	RESET_OPENGL_CONTEXT

	OpInputAction* action;
	if (gMenuBar)
	{
		action = gMenuBar->FindAction(command);
		if (action)
		{
			// first we update action to fit menu or click invocation
			// but if we create action copy on this level it wont't be updated
			// after action invocation, thats why first we need to change
			// action method, and than restore its original invocation method
			OpInputAction::ActionMethod original_action_mathod = action->GetActionMethod();
			if (!IsEventInvokedFromMenu())
				action->SetActionMethod(OpInputAction::METHOD_KEYBOARD);
			if (CocoaFileChooser::HandlingCutCopyPaste())
			{
				switch (action->GetAction())
				{
					case OpInputAction::ACTION_COPY:
						CocoaFileChooser::Copy();
						return emBrowserNoErr;
					case OpInputAction::ACTION_PASTE:
						CocoaFileChooser::Paste();
						return emBrowserNoErr;
					case OpInputAction::ACTION_CUT:
						CocoaFileChooser::Cut();
						return emBrowserNoErr;
					case OpInputAction::ACTION_SELECT_ALL:
						CocoaFileChooser::SelectAll();
						return emBrowserNoErr;
					default:;
				}
			}
			OpInputContext * context = g_input_manager->GetKeyboardInputContext();
			if (!context) {
				context = g_application->GetInputContext();
			}
			if (action->GetAction() == OpInputAction::ACTION_ENTER_FULLSCREEN ||
				action->GetAction() == OpInputAction::ACTION_LEAVE_FULLSCREEN)
			{
				if (CocoaOpWindow::HandleEnterLeaveFullscreen())
				{
					return emBrowserNoErr;
				}
			}

			if (action->GetAction() == OpInputAction::ACTION_NEW_PAGE)
			{
				CocoaOpWindow::DeminiaturizeAnyWindow();
			}

			//BOOL handled = g_input_manager->InvokeAction(action, g_application->GetActiveDesktopWindow(FALSE));
			BOOL handled = g_input_manager->InvokeAction(action, context);

			if (original_action_mathod != action->GetActionMethod())
				action->SetActionMethod(original_action_mathod);

			if (handled)
			{
				gMenuBar->ActionExecuted(command);
			}

			return emBrowserNoErr;
		}
	}
	return emBrowserGenericErr;
}

EmBrowserStatus GetQuickMenuCommandStatus(EmQuickMenuCommand command, EmQuickMenuFlags *flags)
{
	EM_BROWSER_FUNCTION_BEGIN

	OpInputAction * action;
	EmQuickMenuFlags localflags = 0;
	OpInputAction *getState = NULL;

	*flags = 0;
	if (gMenuBar)
	{
		action = gMenuBar->FindAction(command);
		if (action)
		{
			getState = new OpInputAction(action, OpInputAction::ACTION_GET_ACTION_STATE);
			if (getState)
			{
				OpInputContext * context = g_input_manager->GetKeyboardInputContext();
				if (!context) {
					context = g_application->GetInputContext();
				}
				g_input_manager->InvokeAction(getState, context);
				INT32 state = action->GetActionState();
				BOOL disabled = state & OpInputAction::STATE_DISABLED && !(state & OpInputAction::STATE_ENABLED);
				BOOL checked = state & OpInputAction::STATE_SELECTED && action->GetActionOperator() != OpInputAction::OPERATOR_AND && action->GetNextInputAction();
				BOOL selected = state & OpInputAction::STATE_SELECTED && !action->GetNextInputAction();
				if (disabled)
					localflags |= emQuickMenuFlagDisabled;
				if (checked)
					localflags |= emQuickMenuFlagHasMark;
				if (selected)
					localflags |= emQuickMenuFlagHasMark; // | emQuickMenuFlagUseBulletMark;

				*flags = localflags;
				delete getState;
				return emBrowserNoErr;
			}
		}
	}
	return emBrowserGenericErr;
}

EmBrowserStatus RebuildQuickMenu(EmQuickMenuRef menu)
{
	RESET_OPENGL_CONTEXT

	if(menu)
	{
		if(gMenuBar)
		{
			gMenuBar->RebuildMenu(menu);
			return emBrowserNoErr;
		}
	}

	return emBrowserGenericErr;
}

EmBrowserStatus IsSharedMenuHit(UInt16 menuID, UInt16 menuItemIndex)
{
#ifdef SUPPORT_SHARED_MENUS
	if(gVendorDataID == 'OPRA')
	{
		if(SharedMenuHit(menuID, menuItemIndex))
		{
			return emBrowserNoErr;
		}
	}
#endif
	return emBrowserGenericErr;
}

EmBrowserStatus UpdateMenuTracking(BOOL tracking)
{
	CocoaOperaListener::GetListener()->SetTracking(tracking);
	SendMenuTrackingToScope(tracking);
	return emBrowserNoErr;
}

EmBrowserStatus MenuItemHighlighted(CFStringRef item_title, BOOL main_menu, BOOL is_submenu)
{
	SendMenuHighlightToScope(item_title, main_menu, is_submenu);
	return emBrowserNoErr;
}

EmBrowserStatus SetActiveNSMenu(void *nsmenu)
{
	SendSetActiveNSMenu(nsmenu);
	return emBrowserNoErr;
}

EmBrowserStatus OnMenuShown(void *nsmenu, BOOL shown)
{
	OpScopeDesktopWindowManager_SI::QuickMenuInfo info;
	SetQuickExternalMenuInfo(nsmenu, info);
	g_application->GetMenuHandler()->OnMenuShown(shown, info);
	
	return emBrowserNoErr;
}

#endif

#pragma mark -
#pragma mark ----- INIT/SHUTDOWN -----
#pragma mark -

static CFURLRef CreateCFURLRefFromImageName (CFAllocatorRef allocator, const char* image_name)
{
	const char* frameworkStart = strstr(image_name, "/Opera.framework/");
	size_t image_len = frameworkStart ? (frameworkStart - image_name) : strlen(image_name);

	CFURLRef theFrameworksFolderURL = CFURLCreateFromFileSystemRepresentation(allocator, (UInt8*)image_name, image_len, true);

	CFURLRef theOperaAppBundleURL = CFURLCreateCopyDeletingLastPathComponent (allocator, theFrameworksFolderURL); // Delete "Frameworks"
	CFRelease (theFrameworksFolderURL);
	if (theOperaAppBundleURL == 0)
		return 0;

	// Append "MacOS/Opera" (to make it work with the old package-traversing code in Loc_Mac.cpp)
	CFURLRef theOperaAppBundleMacOSFolderURL = CFURLCreateCopyAppendingPathComponent (allocator, theOperaAppBundleURL, CFSTR("MacOS/Opera"), false);
	CFRelease (theOperaAppBundleURL);
	if (theOperaAppBundleMacOSFolderURL == 0)
		return 0;

	return theOperaAppBundleMacOSFolderURL;
}


extern "C" void InitMachOLibrary() __attribute__ ((constructor));

extern "C"
#if (__GNUC__ < 4)
{
#endif
void InitMachOLibrary()
{

/**
* Notes from MWRon:
* The Mach-O image header has a different name
* depending on what you are building:
*
* 	case linkApplication:
*		header_name = "__mh_execute_header";
*		break;
* 	case linkBundle:
*		header_name = "__mh_bundle_header";
*		break;
* 	case linkSharedLibrary:
*		header_name = "__mh_dylib_header";
*		break;
* 	case linkDyLinker:
*		header_name = "__mh_dylinker_header";
*		break;
*
***/

	extern mach_header _mh_dylib_header;
	mach_header* header = &_mh_dylib_header;

	const char* imagename = 0;
	/* determine the image name */
	int cnt = _dyld_image_count();
	for (int idx1 = 0; idx1 < cnt; idx1++) {
	if (_dyld_get_image_header(idx1) == header)
		imagename = _dyld_get_image_name(idx1);
	}
	if (imagename == 0)
		return;
	else
	{
#ifdef _MAC_DEBUG
		fprintf(stderr, "Image found: %s.\n", imagename);
#endif
	}
	gCFMWidgetLibURL = CreateCFURLRefFromImageName(NULL, imagename);
}
#if (__GNUC__ < 4)
}
#endif

//#pragma CALL_ON_LOAD InitMachOLibrary	// Note: You can't put this pragma anywhere else, it won't work. <ED>

OpString gOperaPrefsLocation;

#ifndef NO_CARBON
#include "platforms/mac/pi/MacOpMessageHandler.h"

EmBrowserAppBeforeNavigationProc gOriginalBeforeNavigation;

long BeforeNavigationHack(IN EmBrowserRef inst, IN const EmBrowserString destURL, OUT EmBrowserString newURL)
{
// set up an event in case someone calls RunAppModalLoopForWindow, or similar
	EventRef operaEvent = NULL;
	CreateEvent(NULL, kEventClassOperaPlatformIndependent, kEventOperaMessage,
				GetCurrentEventTime(), kEventAttributeUserEvent, &operaEvent);
	PostEventToQueue(GetMainEventQueue(), operaEvent, kEventPriorityLow);	// refcount: 2

// then call Adobe's function
	EmBrowserStatus reply = gOriginalBeforeNavigation(inst, destURL, newURL);

// and clean up
	RemoveEventFromQueue(GetMainEventQueue(), operaEvent);
	ReleaseEvent(operaEvent);
	return reply;
}
#endif

#if defined(VEGA_MDEFONT_SUPPORT)
#include "modules/mdefont/mdefont.h"
OP_STATUS AddFontsFromDir(const char *dir)
{
	if (!dir)
		return OpStatus::ERR_NULL_POINTER;

	if (dir[0] != '/') {		// relative path
		return OpStatus::ERR;	// unsupported
	}

	OpString uni_dir;
	RETURN_IF_ERROR(uni_dir.SetFromUTF8(dir));

	OpFolderLister* lister = NULL;
	RETURN_IF_ERROR(OpFolderLister::Create(&lister));

	OpAutoPtr<OpFolderLister> lister_ptr(lister);
	RETURN_IF_ERROR(lister->Construct(uni_dir.CStr(), UNI_L("*.ttf")));
	while (lister->Next())
	{
		const uni_char* font_file = lister->GetFullPath();
		OpString8 font_file_utf8;
		RETURN_IF_ERROR(font_file_utf8.SetUTF8FromUTF16(font_file));
		/* TODO: Recurse into subdirectories */
		if (strstr(font_file_utf8.CStr(), "AppleMyungjo.ttf"))
		{
			// This one crashes, for some reason...
			continue;
		}
		RETURN_IF_ERROR(MDF_AddFontFile(font_file_utf8));
	}
	return OpStatus::OK;
}

OP_STATUS AddSystemFonts()
{
//	return AddFontsFromDir("/System/Library/Fonts/");
	return AddFontsFromDir("/Library/Fonts/");
}
#endif

EmBrowserStatus MacWidgetInitLibrary(EmBrowserInitParameterBlock *initPB)
{
	OpenGLContextWrapper::InitSharedContext();
	OpenGLContextWrapper::SetSharedContextAsCurrent();

#ifndef NO_CARBON
	gOriginalBeforeNavigation = gApplicationProcs->beforeNavigation;
	gApplicationProcs->beforeNavigation = BeforeNavigationHack;
#endif

	// GetOSVersion() is initialized in the following call.
	InitOperatingSystemVersion();

	gHandleKeyboardEvent = true;
	GetCurrentThread(&gMainThreadID);

#ifdef USE_COMMON_RESOURCES
	// Create the PI interface object
	OpDesktopResources *dr_temp; // Just for the autopointer
	RETURN_VALUE_IF_ERROR(OpDesktopResources::Create(&dr_temp), NULL);
	static_cast<MacOpDesktopResources*>(dr_temp)->FixBrokenSandboxing();
	OpAutoPtr<OpDesktopResources> desktop_resources(dr_temp);
	if(!desktop_resources.get())
		return emBrowserOutOfMemoryErr;
#endif // USE_COMMON_RESOURCES

#if defined(VEGA_MDEFONT_SUPPORT)
	MDF_InitFont();
	AddSystemFonts();
#endif // VEGA_MDEFONT_SUPPORT

	// Collect all the command line parameters prefs hack and screen resolution changes
	// Make aure we are Opera
	if (initPB->vendorDataID == 'OPRA')
	{
		// Find the command line arguments
		EmQuickBrowserInitParameterBlock* quickBlock = (EmQuickBrowserInitParameterBlock *)initPB->vendorData;
		int argc = quickBlock->argc;
		const char** argv = quickBlock->argv;

		// Parse the command line args
		// On mac we don't care about wrong command line args, we just have to check for the help
		// return so we must exit straight away
		if (CommandLineManager::GetInstance()->ParseArguments(argc, argv) == CommandLineStatus::CL_HELP)
			return emBrowserParameterErr;

		// Init start/shutdown profiling
		OP_PROFILE_INIT();

		// Setup the launch manager so it uses the same instance and the quick app
#ifdef AUTO_UPDATE_SUPPORT
		if (!LaunchManager::SetInstance(quickBlock->launch_manager))
			return emBrowserParameterErr;
#endif
		
		// Set the address of the GPU info declared by the crash logger
		extern GpuInfo *g_gpu_info;
		g_gpu_info = quickBlock->pGpuInfo;

		OpFileUtils::SetOperaBundle(quickBlock->opera_bundle);
	}

	PosixThread::CreateThread(MacAsyncLoader::LoadElements, NULL);

	// NOTE! It is important to do HSetVol before we get any OpLocation-specialfolder calls
	// otherwise the special folders will be initialized incorrectly!
	
	// FIXME: I think this code might trigger in cases where argv[0] isn't valid, such as when launching Opera from the Terminal.
	// Replace with CFBundleGetMainBundle->CFURL->path->OpFileUtils::InitHomeDir, or something to that effect.
	if(gCFMWidgetLibURL)
	{
		CFStringRef str = CFURLGetString(gCFMWidgetLibURL);
		OpString opstr;
		OpString8 opstr8;
		if (SetOpString(opstr, str)) {
			if (OpStatus::IsSuccess(opstr8.SetUTF8FromUTF16(opstr)))
				OpFileUtils::InitHomeDir(opstr8.CStr());
		}
	}

#ifndef NO_CARBON
	// Populate this global variable to avoid thousands of calls to get this
	// value which never changes in the course of running Opera.
	gMainEventQueue = GetMainEventQueue();
	gMainEventLoop = GetMainEventLoop();
#endif

	if (initPB->vendorDataID == 'OPRA')
	{
#ifdef QUICK
		if (initPB->vendorDataMajorVersion == 2)
		{
			gQuickBlock = (EmQuickBrowserInitParameterBlock *)initPB->vendorData;
			if (gQuickBlock)
			{
				gQuickAppMethods = gQuickBlock->appProcs;
				gQuickLibMethods = new EmQuickBrowserLibProcs;
				if (gQuickLibMethods)
				{
					gQuickLibMethods->startupQuick = StartupQuick;
					gQuickLibMethods->handleQuickCommand = HandleQuickMenuCommand;
					gQuickLibMethods->getQuickCommandStatus = GetQuickMenuCommandStatus;
					gQuickLibMethods->rebuildQuickMenu = RebuildQuickMenu;
					gQuickLibMethods->sharedMenuHit = IsSharedMenuHit;
					gQuickLibMethods->startupQuickSecondPhase = StartupQuickSecondPhase;
					gQuickLibMethods->updateMenuTracking = UpdateMenuTracking;
					gQuickLibMethods->menuItemHighlighted = MenuItemHighlighted;
					gQuickLibMethods->setActiveNSMenu = SetActiveNSMenu;
					gQuickLibMethods->onMenuShown = OnMenuShown;
					gQuickBlock->libProcs = gQuickLibMethods;
				}
			}
		}
#endif
	}

	InitializeCoreGraphics();
	InitializeIOSurface();
	InitializeLaunchServices();
	
	CleanupOldCacheFolders();

	if (OpStatus::IsError(OpThreadTools::Create(&g_thread_tools)))
		return emBrowserGenericErr;

	gCocoaOperaListener = new CocoaOperaListener;
	if (!gCocoaOperaListener)
		return emBrowserOutOfMemoryErr;

	gOperaApplicationDelegate = new OperaApplicationDelegate;
	if (!gOperaApplicationDelegate)
		return emBrowserOutOfMemoryErr;

	gTextConverter = new CTextConverter;
	if (!gTextConverter)
		return emBrowserOutOfMemoryErr;

	//MacOpPrinterController::InitStatic();

#ifndef USE_COMMON_RESOURCES
	// IsFirstRunAfterInstall hack
	// We will have to do add more checks here (check if this is the first install of a major version)
	OpFile iniFile;
	OpString ini_filename;

	// Yuck yuck yuck but unavoidable since the OpFolder is not inited yet.
	OpFileUtils::SetToSpecialFolder(OPFILE_USERPREFS_FOLDER, ini_filename);
	ini_filename.Append(PREFERENCES_FILENAME);

	iniFile.Construct(ini_filename);
	BOOL exists = FALSE;
	iniFile.Exists(exists);
	if(!exists)
	{
		g_first_run_after_install = TRUE;
	}
#endif // !USE_COMMON_RESOURCES

#ifdef SUPPORT_SHARED_MENUS
	if (initPB->vendorDataID == 'OPRA')
	{
		InitializeSharedMenusStuff();
	}
#endif
	// This writeLocation is used by embedded versions of Opera (i.e. Adobe) to put the preferences
	// for Opera where they want to. This should only be used from embedded products
	gOperaPrefsLocation.Set((const uni_char*)initPB->writeLocation);

	return emBrowserNoErr;
}

EmBrowserStatus MacPostInit()
{
#ifdef WAND_EXTERNAL_STORAGE
	if(g_wand_manager)
		g_wand_manager->SetDataStorage(new(ELeave) MacWandDataStorage());
#endif
	if (g_application)
	{
		g_application->AddSettingsListener(static_cast<MacApplicationListener*>(g_desktop_global_application));
	}
#ifdef QUICK

	// FIXME: VegaOpPainter
	if (g_widgetpaintermanager) {
		g_widgetpaintermanager->SetPrimaryWidgetPainter(new(ELeave) MacWidgetPainter());
	}
#endif

	return emBrowserNoErr;
}

EmBrowserStatus MacWidgetShutdownLibrary()
{
	if(gCFMWidgetLibURL)
	{
		CFRelease(gCFMWidgetLibURL);
		gCFMWidgetLibURL = NULL;
	}
/*	OpString endsound;
	if ((gVendorDataID == 'OPRA') && prefsManager && (OpStatus::OK == prefsManager->GetStringPref(PrefsManager::EndSound, endsound)))
	{
		OpStatus::Ignore(SoundIt(endsound, FALSE, FALSE));
	}
*/
#ifdef EMBEDDED_DEFAULT_PREFS_FIX
	if(gVendorDataID != 'OPRA')
	{
		BOOL currentSetting = prefsManager->GetIntegerPrefDirect(PrefsManager::TrustServerTypes) ? true : false;
		if(gEmbeddedDefaults.trustMIMETypes != currentSetting)
			prefsManager->WriteIntegerPref(PrefsManager::TrustServerTypes, gEmbeddedDefaults.trustMIMETypes ? 1 : 0);

		UA_BaseStringId id = g_uaManager->GetUABaseId();
		if(id != gEmbeddedDefaults.userAgentVersion)
		{
			TRAPD(rc, prefsManager->WriteUASettingsL(gEmbeddedDefaults.userAgentId, gEmbeddedDefaults.userAgentVersion));
		}
	}
#endif

#ifndef NO_CARBON
	MacOpSound::Terminate();
#endif

    TearDownNotifications();
	TearDownCocoaSpecificElements();

	// Remove any badges from the icon
	RestoreApplicationDockTileImage();

	delete gMenuBar;
	gMenuBar = NULL;

	delete g_appListener;
	g_appListener = NULL;

	delete g_appMenuListener;
	g_appMenuListener = NULL;

	delete gOperaApplicationDelegate;
	gOperaApplicationDelegate = NULL;

	delete g_MacWidgetInfo;
	g_MacWidgetInfo = NULL;

#ifdef QUICK
	delete gQuickLibMethods;
	gQuickLibMethods = NULL;
#endif

	delete gAppleEvents;
	gAppleEvents = NULL;

	delete gMouseCursor;
	gMouseCursor = NULL;

//	endsound.Empty();

	RemoveCacheLock();

	MacOpSkinElement::Free();

	MacOpPrinterController::FreeStatic();

#ifdef SUPPORT_OSX_SERVICES
	MacOpServices::Free();
#endif

	OpenGLContextWrapper::Uninitialize();

	return emBrowserNoErr;
}

#pragma mark -

/*
#ifdef __cplusplus
extern "C" {
#endif

int main(int argc, char **argv)
{
	// Do nothing
	return 0;
}

#ifdef __cplusplus
}
#endif
*/
