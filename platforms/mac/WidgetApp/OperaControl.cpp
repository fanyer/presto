/*
	File:		OperaControl.c

	Controlling Opera 7.2 Remotely...
*/

#ifdef __MACH__
#include <Carbon/Carbon.h>
#else
#include <Carbon.h>
#endif

#include <stdio.h>

#include "OperaControl.h"
#include "WidgetApp.h"
#include "embrowsertestsuite.h"

#include <stdlib.h>
#include <string.h>

EmBrowserRef gWidgetPointer = NULL;
Boolean gLibraryInitialized = FALSE;
EmBrowserInitParameterBlock * params = NULL;
EmBrowserMethods * widgetMethods = NULL;
WindowRef gWindow = NULL;
EmBrowserInitLibraryProc gInitLibraryProc = NULL;
Boolean gUseWackyCursors = false;
TSMDocumentID gDoc = 0;

// Import some CoreGraphics functions
#ifndef __MACH__
CGImageGetTypeIDProcPtr 			CGImageGetTypeIDPtr;
CGImageReleaseProcPtr				CGImageReleasePtr;
CGContextReleaseProcPtr				CGContextReleasePtr;
CGContextFlushProcPtr				CGContextFlushPtr;
CGContextDrawImageProcPtr			CGContextDrawImagePtr;
SyncCGContextOriginWithPortProcPtr	SyncCGContextOriginWithPortPtr;
CreateCGContextForPortProcPtr		CreateCGContextForPortPtr;
CGImageRetainProcPtr				CGImageRetainPtr;
RunStandardAlertProcPtr				RunStandardAlertPtr;
CreateStandardAlertProcPtr			CreateStandardAlertPtr;
Str255 gOperaLibraryFileName;
#else
CGImageGetTypeIDProcPtr 			CGImageGetTypeIDPtr = CGImageGetTypeID;
CGImageReleaseProcPtr				CGImageReleasePtr = CGImageRelease;
CGContextReleaseProcPtr				CGContextReleasePtr = CGContextRelease;
CGContextFlushProcPtr				CGContextFlushPtr = CGContextFlush;
CGContextDrawImageProcPtr			CGContextDrawImagePtr = CGContextDrawImage;
SyncCGContextOriginWithPortProcPtr	SyncCGContextOriginWithPortPtr = SyncCGContextOriginWithPort;
CreateCGContextForPortProcPtr		CreateCGContextForPortPtr = CreateCGContextForPort;
CGImageRetainProcPtr				CGImageRetainPtr = CGImageRetain;
RunStandardAlertProcPtr				RunStandardAlertPtr = RunStandardAlert;
CreateStandardAlertProcPtr			CreateStandardAlertPtr = CreateStandardAlert;
CFStringRef							gOperaFrameworkName = NULL;
#endif

#pragma mark -
#pragma mark ----- SET-UP FUNCTIONS -----
#pragma mark -


EmBrowserRef gTestWidget = NULL;

void JSEvaluateCodeCallback(void *clientData, EmJavascriptStatus status, EmJavascriptValue *value);
void JSEvaluateCodeCallback2(void *clientData, EmJavascriptStatus status, EmJavascriptValue *value);
void JSEvaluateCodeCallback3(void *clientData, EmJavascriptStatus status, EmJavascriptValue *value);
void JSGetPropertyCallback(void *clientData, EmJavascriptStatus status, EmJavascriptValue *value);
EmJavascriptStatus JSMethodCallBack(void *inData, EmDocumentRef inDoc, EmJavascriptObjectRef inObject,
                      unsigned argumentsCount, EmJavascriptValue *argumentsArray,
                      EmJavascriptValue *returnValue);
void thumbnailCallback (IN void *imageData, IN EmThumbnailStatus status, IN EmThumbnailRef ref);

#ifdef WIDGET_APP_ORIGIN_TEST
void TestPort()
{
	static RgnHandle clip = 0;
	GrafPtr port;
	Rect clipBounds;
	Rect portRect;
	if (!clip)
		clip = NewRgn();

	if (!gWindow)
		return;

	GetPort(&port);
	if (port != GetWindowPort(gWindow))
		Debugger();
	GetClip(clip);
	GetRegionBounds(clip, &clipBounds);
	if ((clipBounds.top != 21) || (clipBounds.left != 27) || (clipBounds.bottom != 121) || (clipBounds.right != 127))
		Debugger();
	::GetPortBounds(port, &portRect);
	if ((portRect.left != 17) || (portRect.top != 27))
		Debugger();
}
#endif


EmBrowserStatus InitLibrary()
{
	/*********************************************
	** STEP 1:  FIND OPERA AND OPEN THE LIBRARY **
	**********************************************/
	Boolean operaFound = DiscoverOpera();

	/************************************
	** STEP 2:  INITIALIZE THE LIBRARY **
	************************************/
	if (operaFound)
	{
		int widgetVerNo = 0;
		long retVal = emBrowserNoErr;

		if (!params)
		{
			params = (EmBrowserInitParameterBlock *)malloc(sizeof(EmBrowserInitParameterBlock));
		}
		if (params && gInitLibraryProc && !gLibraryInitialized)
		{
			memset(params, 0, sizeof(EmBrowserInitParameterBlock));
			params->majorVersion = EMBROWSER_MAJOR_VERSION;
			params->minorVersion = EMBROWSER_MINOR_VERSION;
			params->malloc = NULL;
			params->free = NULL;
			params->realloc = NULL;
			params->createInstance = NULL;
			params->destroyInstance = NULL;
			params->vendorDataID = 'wApp';
			params->vendorData = NULL;

			params->notifyCallbacks = (EmBrowserAppNotification*) malloc(sizeof(EmBrowserAppNotification));
			if (params->notifyCallbacks)
			{
				memset(params->notifyCallbacks, 0, sizeof(EmBrowserAppNotification));
				params->notifyCallbacks->majorVersion = EMBROWSER_MAJOR_VERSION;
				params->notifyCallbacks->minorVersion = EMBROWSER_MINOR_VERSION;
				params->notifyCallbacks->notification = Notify;
				params->notifyCallbacks->beforeNavigation = BeforeNavigation;
				params->notifyCallbacks->handleProtocol = NULL;
				params->notifyCallbacks->newBrowser= BeforeNewBrowser;
				params->notifyCallbacks->posChange = PositionChangeNotify;
				params->notifyCallbacks->sizeChange = SizeChangeNotify;
				params->notifyCallbacks->setCursor = WackyCursors;
			}
			params->browserMethods = NULL;

			/*****************************************************
			** ACTUAL CALL TO OPERA LIBRARY INITIALIZATION HERE **
			*****************************************************/
			retVal = gInitLibraryProc(params);

			if (retVal == emBrowserNoErr)
			{
				widgetMethods = params->browserMethods;
				gLibraryInitialized = TRUE;
			}
			else
				return emBrowserGenericErr;
		}

		return emBrowserNoErr;
	}

	return emBrowserGenericErr;
}

#define kTSMDocumentSupportDocumentAccessPropertyTag ('dapy')

TSMDocumentID CreateTSMDocument()
{
	TSMDocumentID doc = 0;
	OSType supportedTypes [] = {kUnicodeDocument};
	OSErr err = NewTSMDocument(1, supportedTypes, &doc, 0);
#if __MACH__	// TSMSetDocumentProperty not in the carbon headers. Of course we could pull the functions out manually, but that is beyond the scope of this example.
	if ((err == noErr) && doc)
	{
		long macOSVersion;
		Gestalt(gestaltSystemVersion, &macOSVersion);	// OK, so most people would have this thing cached somewhere...
		if (macOSVersion >= 0x1030) {
			// This is to inform the TextServicesManager that we're supporting the documentaccess protocol, we won't get any of those events otherwise.
			// Also note that TSMSetDocumentProperty is available from 10.2 and up, but that kTSMDocumentSupportDocumentAccessPropertyTag is valid only for TSM 2.2 (10.3 headers)
			UInt32 yes = 0;
			OSStatus status = TSMSetDocumentProperty(doc, kTSMDocumentSupportDocumentAccessPropertyTag, sizeof(yes), &yes);
		}
	}
#endif
	return doc;
}

void CreateWidget()
{
	Rect widgetPos = {0, 0, 1024, 1280};
	if (gLibraryInitialized)
	{
		if (!gWindow)
		{
			WindowRef window = 0;
			MakeWindow(&window);
#ifdef WIDGET_APP_ORIGIN_TEST
			SetPortWindowPort(window);
			SetOrigin(17,27);
			Rect cr = {21,27,121,127};
			ClipRect(&cr);
#endif
			gWindow = window;
		}
		if (!gDoc)
		{
			gDoc = CreateTSMDocument();
			if (gDoc)
				ActivateTSMDocument(gDoc);
		}
		if (gWindow && !gWidgetPointer)
		{

			GetWindowPortBounds(gWindow, &widgetPos);

			ShowWindow(gWindow);


			/************************************
			** STEP 3:  CREATE INSTANCE(S)	 **
			************************************/
			if (params && params->createInstance)
			{
				params->createInstance(gWindow, emBrowserOptionUseDefaults, &gWidgetPointer);
TEST_PORT
				if (gWidgetPointer && widgetMethods->setLocation)
				{
					EmBrowserRect widgetPosRect;
					widgetPosRect.top = widgetPos.top;
					widgetPosRect.left = widgetPos.left;
					widgetPosRect.bottom = widgetPos.bottom;
					widgetPosRect.right = widgetPos.right;

					widgetMethods->setLocation(gWidgetPointer, &widgetPosRect,gWindow);
TEST_PORT
				}
			}

			if (gWidgetPointer && widgetMethods && widgetMethods->setVisible)
				widgetMethods->setVisible(gWidgetPointer, true);
TEST_PORT
		}
	}
}

Boolean DiscoverOpera()
{
	CFragConnectionID library;
	OSStatus retVal = noErr;
	Str255 name = "\pOpera.app";
	CInfoPBRec cinfo;
	short vRefNum = 0;
	long dirID = 0;
	CFURLRef	operaURL;
	CFBundleRef operaBundle;
	FSSpec file;
	FSSpec file2;
	FSRef operaRef;

#ifdef __MWERKS__
	// In this App we assume Opera is beside the host application
	retVal = HGetVol(NULL, &vRefNum, &dirID);

	FSMakeFSSpec(vRefNum,dirID,name,&file2);
	FSpMakeFSRef(&file2,&operaRef);
	operaURL = CFURLCreateFromFSRef(kCFAllocatorSystemDefault, &operaRef);
#else
	// Same, but here the host app is actually a bundle.
	CFURLRef widgetAppURL = CFBundleCopyBundleURL(CFBundleGetMainBundle());
	CFURLRef parentFolderURL = CFURLCreateCopyDeletingLastPathComponent(kCFAllocatorDefault, widgetAppURL);
	CFRelease(widgetAppURL);
	operaURL = CFURLCreateCopyAppendingPathComponent( kCFAllocatorDefault, parentFolderURL, CFSTR("Opera.app"), true);
	CFRelease(parentFolderURL);
#endif
	operaBundle = CFBundleCreate( kCFAllocatorDefault, operaURL );
	if (operaBundle)
	{
		// Check to see if this Opera meets our criteria.
		Boolean operaOK = AnalyzeOperaAPIVersion(operaBundle);

		if (operaOK) // We've decided this copy of Opera is embeddable in our application
		{
#ifdef __MACH__
			/**
			* Since Opera7 is MachO we load the framework directly instead of going through the glue CFM-Widget
			*/
			int version = CFBundleGetVersionNumber(operaBundle);
			char key[64];
			CFStringRef versionStr = (CFStringRef)CFBundleGetValueForInfoDictionaryKey(operaBundle, kCFBundleVersionKey);

			CFURLRef frameworksURL = CFBundleCopyPrivateFrameworksURL( operaBundle );
			CFRelease( operaURL );
			CFRelease( operaBundle );

			if(frameworksURL)
			{
				CFURLRef operaFrameworkURL = CFURLCreateCopyAppendingPathComponent(NULL, frameworksURL, gOperaFrameworkName, false);
				CFRelease(frameworksURL);
				if(operaFrameworkURL)
				{
					CFBundleRef operaFrameworkBundle = CFBundleCreate(NULL, operaFrameworkURL);
					CFRelease(operaFrameworkURL);
					if(operaFrameworkBundle)
					{
//						CFStringGetCString(kCFBundleVersionKey, key, 64, kCFStringEncodingMacRoman);
						gInitLibraryProc = (EmBrowserInitLibraryProc)CFBundleGetFunctionPointerForName(operaFrameworkBundle, CFSTR("WidgetInitLibrary"));
						CFRelease(operaFrameworkBundle);
						if(gInitLibraryProc)
						{
							return operaOK;
						}
					}
				}
			}

#else
			CFRelease( operaURL );
			CFRelease( operaBundle );

			cinfo.dirInfo.ioCompletion = NULL;
			cinfo.dirInfo.ioNamePtr = name;
			cinfo.dirInfo.ioVRefNum = vRefNum;
			cinfo.dirInfo.ioDrDirID = dirID;
			cinfo.dirInfo.ioFDirIndex = 0; // Get info from name.
			retVal = PBGetCatInfoSync(&cinfo);
			memcpy(name, "\pContents", 9);
			retVal |= PBGetCatInfoSync(&cinfo);
			memcpy(name, "\pMacOS", 6);
			retVal |= PBGetCatInfoSync(&cinfo);

			if (noErr == retVal)
			{
				dirID = cinfo.dirInfo.ioDrDirID;
				if (noErr == FSMakeFSSpec(vRefNum, dirID, gOperaLibraryFileName, &file))
				{

					FSpMakeFSRef(&file,&operaRef);

					retVal = GetDiskFragment(&file, 0, kCFragGoesToEOF, NULL, kReferenceCFrag, &library, NULL, NULL);

					if (retVal == noErr)
					{
						// Getting the function pointer to the Opera initialization function.
						retVal = FindSymbol(library, "\pWidgetInitLibrary", (char**)&gInitLibraryProc, NULL);

						if (retVal == noErr)
							return operaOK;
					}
				}
			}
#endif // __MACH__
		}
	}

	return false;
}

Boolean AnalyzeOperaAPIVersion(CFBundleRef operaBundle)
{	Boolean operaOK = false;

	CFURLRef supportFilesCFURLRef = CFBundleCopySupportFilesDirectoryURL(operaBundle);
	CFShow(supportFilesCFURLRef);
	CFURLRef versionURLRef = CFURLCreateCopyAppendingPathComponent(
						kCFAllocatorDefault, supportFilesCFURLRef, CFSTR("Info.plist"),false);
	//CFShow(versionURLRef);

	// Read the XML file.
	CFDataRef xmlCFDataRef;
	SInt32 errorCode;

	OSStatus retVal = CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault, versionURLRef, &xmlCFDataRef, NULL, NULL, &errorCode);
	CFStringRef errorString;

	// Reconstitute the dictionary using the XML data.
	CFPropertyListRef myCFPropertyListRef = CFPropertyListCreateFromXMLData(kCFAllocatorDefault, xmlCFDataRef, kCFPropertyListImmutable, &errorString);

	//CFShow(myCFPropertyListRef);

	 if (CFDictionaryGetTypeID() == CFGetTypeID(myCFPropertyListRef))
	 {
		CFStringRef emBrowserMajorVersion; // 3
		CFStringRef emBrowserMinorVersion; // 0
		CFStringRef emBrowserLibraryFileName;	// Opera 9 Widget
		CFStringRef emBrowserLibraryFragmentName; // OperaRenderingLib
		CFStringRef emBrowserEntryPoint;	// WidgetInitLibrary

		Boolean foundValue = false;
		foundValue = CFDictionaryGetValueIfPresent( (CFDictionaryRef)myCFPropertyListRef,
		(const void*)CFSTR("EmBrowserMajorVersion"), (const void**) &emBrowserMajorVersion);

		foundValue = CFDictionaryGetValueIfPresent( (CFDictionaryRef)myCFPropertyListRef,
		(const void*)CFSTR("EmBrowserMinorVersion"), (const void**) &emBrowserMinorVersion);

		foundValue = CFDictionaryGetValueIfPresent( (CFDictionaryRef)myCFPropertyListRef,
		(const void*)CFSTR("EmBrowserLibraryFileName"), (const void**) &emBrowserLibraryFileName);

		foundValue = CFDictionaryGetValueIfPresent( (CFDictionaryRef)myCFPropertyListRef,
		(const void*)CFSTR("EmBrowserLibraryFragmentName"), (const void**) &emBrowserLibraryFragmentName);

		foundValue = CFDictionaryGetValueIfPresent( (CFDictionaryRef)myCFPropertyListRef,
		(const void*)CFSTR("EmBrowserEntryPoint"), (const void**) &emBrowserEntryPoint);

#ifdef __MACH__
// NEW: Get the actual framework name.
		foundValue = CFDictionaryGetValueIfPresent( (CFDictionaryRef)myCFPropertyListRef,
		(const void*)CFSTR("EmBrowserFrameworkName"), (const void**) &gOperaFrameworkName);
		if (foundValue) {
			CFRetain(gOperaFrameworkName);
		} else {
			// Opera 7 did not have this entry.
			gOperaFrameworkName = CFStringCreateCopy(kCFAllocatorDefault, CFSTR("Opera 7.framework"));
		}
#endif

#ifndef __MACH__
		CFStringGetPascalString(emBrowserLibraryFileName, gOperaLibraryFileName, 256, kCFStringEncodingMacRoman);
#endif
		// Test these values and set answer to be true/false based on the embeddability of
		// the found version of Opera.
		operaOK = foundValue;
	 }


	if (NULL != myCFPropertyListRef)
		CFRelease(myCFPropertyListRef);

	if (NULL != xmlCFDataRef)
		CFRelease(xmlCFDataRef);

	if (NULL != versionURLRef)
		CFRelease(versionURLRef);

	if (NULL != supportFilesCFURLRef)
		CFRelease(supportFilesCFURLRef);

	return operaOK;

}

#pragma mark -
#pragma mark ----- DESTROY FUNCTIONS -----
#pragma mark -

void DestroyWidget()
{
	/*********************************
	** STEP 5:  DESTROY INSTANCE(S) **
	**********************************/
	if (gWidgetPointer && params->destroyInstance)
		params->destroyInstance(gWidgetPointer);
	gWidgetPointer = NULL;

	if (gWindow)
		DisposeWindow(gWindow);
	gWindow = NULL;
}


#pragma mark -
#pragma mark ----- NOTIFICATION/CALLBACK FUNCTIONS -----
#pragma mark -

void Notify(EmBrowserRef inst, EmBrowserAppMessage messageIn, long value)
{
	/***********************************************************************************
	** STEP 4A:  PROCESSING AND INTERACTION WITH OPERA BASED ON MESSAGES _FROM_ OPERA **
	***********************************************************************************/
	int i = 0;
	long currentRemaining = 0;
	long totalAmount = 0;
	long bufferSize = 256;
	EmBrowserString textBuffer = new EmBrowserChar[bufferSize];
	long textLength = 0;
	EmBrowserTextOptions textOption;
	EmBrowserChar windowLocationStr[] = { 'w', 'i', 'n', 'd', 'o', 'w', '.', 'l', 'o', 'c', 'a', 't', 'i', 'o', 'n', 0 };
	EmBrowserChar myMethodStr[] = { 'm', 'y', 'M', 'e', 't', 'h', 'o', 'd', '(', ')', ';', 0 };
	EmBrowserChar myObjectStr[] = { 'm', 'y', 'O', 'b', 'j', 'e', 'c', 't', ',', 0 };
	EmBrowserChar dingsStr[] = { 'd', 'i', 'n', 'g', 's', 0 };

	switch (messageIn)
	{
		case emBrowser_app_msg_status_text_changed:
			// This is when you move the mouse over a link.
			textOption = emBrowserCombinedStatusText; //kEmBrowserMouseContextText;
			if (gWidgetPointer && widgetMethods && widgetMethods->getText)
			{
				widgetMethods->getText(gWidgetPointer,textOption,bufferSize,textBuffer, &textLength);
			}
			if ((textBuffer) && (textLength >0))
			{
				CFStringRef str = CFStringCreateWithCharacters(NULL, (UniChar *) textBuffer, textLength);
				SetWindowTitleWithCFString(gWindow,str);
			}

			break;
		case emBrowser_app_msg_page_busy_changed:  // Implemented...
			if (gWidgetPointer && widgetMethods && widgetMethods->pageBusy)
			{
				EmLoadStatus pageLoadStatus = widgetMethods->pageBusy(gWidgetPointer);
				switch (pageLoadStatus)
				{
					case emLoadStatusEmpty:
						i = 1;
						break;
					case emLoadStatusLoaded:
						i = 2;
						EmDocumentRef document; widgetMethods->getRootDoc(gWidgetPointer, &document);

						/* Example use of Javascript API.  See callbacks below. */

						/* Get the object "window.location". */
						widgetMethods->jsEvaluateCode(document, windowLocationStr, JSEvaluateCodeCallback, (void *) document);


						/* Check if this is our test page */
						if(inst == gTestWidget)
						{
			    			widgetMethods->jsEvaluateCode(document, myMethodStr, 0, (void *) document);
    						widgetMethods->jsEvaluateCode(document, myObjectStr, JSEvaluateCodeCallback2, (void *) document);
							gTestWidget = NULL;
						}

						// Create javascript function
						widgetMethods->jsEvaluateCode(document, dingsStr, JSEvaluateCodeCallback3, (void *) document);

						break;
					case emLoadStatusLoading:
						i = 3;
						break;
					case emLoadStatusStopped:
						i = 4;
						break;
					case emLoadStatusFailed:
						i = 5;
						break;
				}
			}
			break;
		case emBrowser_app_msg_progress_changed:   // Implemented...
			textOption = emBrowserCombinedStatusText; // kEmBrowserLoadStatusText;
			if (gWidgetPointer && widgetMethods && widgetMethods->getText)
			{
				widgetMethods->getText(gWidgetPointer,textOption,bufferSize,textBuffer, &textLength);
			}
			if ((textBuffer) && (textLength >0))
			{
				CFStringRef str = CFStringCreateWithCharacters(NULL, (UniChar *) textBuffer, textLength);
				SetWindowTitleWithCFString(gWindow,str);
			}
			// Get the numbers...
			if (gWidgetPointer && widgetMethods && widgetMethods->downloadProgressNums)
			{
				widgetMethods->downloadProgressNums(gWidgetPointer,&currentRemaining,&totalAmount);
			}
			break;
		case emBrowser_app_msg_url_changed:
			break;
		case emBrowser_app_msg_title_changed: // Implemented
			textOption = emBrowserTitleText;
			if (gWidgetPointer && widgetMethods && widgetMethods->getText)
			{
				widgetMethods->getText(gWidgetPointer,textOption,bufferSize,textBuffer, &textLength);
			}
			if ((textBuffer) && (textLength >0))
			{
				CFStringRef str = CFStringCreateWithCharacters(NULL, (UniChar *) textBuffer, textLength);
				SetWindowTitleWithCFString(gWindow,str);
			}
			break;
		case emBrowser_app_msg_security_changed:
			break;
		case emBrowser_app_msg_going_away:
			break;
		case emBrowser_app_msg_request_focus:
			if (widgetMethods && widgetMethods->setFocus)
			{
				// VERY IMPORTANT TO GIVE OPERA EXPLICIT FOCUS.
				// This is done when requested by this message, or
				// the host application can decide when issue setFocus(true/false)
				widgetMethods->setFocus(inst, true);
			}
			break;
		case emBrowser_app_msg_request_activation:
			break;
		case emBrowser_app_msg_request_close:
			break;
		case emBrowser_app_msg_request_show:
			break;
		case emBrowser_app_msg_request_hide:
			break;
		case emBrowser_app_msg_edit_commands_changed:
		// Since we do this on a timer it is not necessary here.  It is much
		// more efficient to do it based on this notifier though.
			break;
	}
}

long BeforeNavigation(EmBrowserRef inst, const EmBrowserString destURL, EmBrowserString newURL)
{
	EmBrowserChar appleURL[] = { 'h', 't', 't', 'p', ':', '/', '/', 'w', 'w', 'w', '.', 'a', 'p', 'p', 'l', 'e', '.', 'c', 'o', 'm', '/', 0 };
	EmBrowserChar replacementURL[] = { 'h', 't', 't', 'p', ':', '/', '/', 'h', 'o', 'm', 'e', '.', 'p', 'a', 'c', 'b', 'e', 'l', 'l', '.', 'n', 'e', 't', '/', 'd', 'i', 'a', 'n', 'a', '_', 'd', 'o', '/', 'k', 'n', 'o', 'w', 'j', 'a', 'c', 'k', '.', 'h', 't', 'm', 0 };
	EmBrowserChar microsoftURL[] = { 'h', 't', 't', 'p', ':', '/', '/', 'w', 'w', 'w', '.', 'm', 'i', 'c', 'r', 'o', 's', 'o', 'f', 't', '.', 'c', 'o', 'm', '/', 0 };

	if (0 == EmBrowserStringCompare(destURL, appleURL))
	{
		// Here we change the URL to something else we want.
		memcpy(newURL, replacementURL, sizeof(EmBrowserChar) * (EmBrowserStringLen(replacementURL)+1));
		return true; // This means we want to let the URL load.
	}

	if (0 == EmBrowserStringCompare(destURL, microsoftURL))
	{
		return false; // This means we want to stop.
	}

	// By default we just want to let the URL load.
	return true;
}

// EmBrowser v. 2.0
//long BeforeNewBrowser(EmBrowserRef caller, EmBrowserOptions browserOptions, EmBrowserWindowOptions windowOptions, IN Rect *bounds, const EmBrowserString destURL, EmBrowserRef* result)
// EmBrowser v. 3.0 -- Rect changed to EmBrowserRect
long BeforeNewBrowser(EmBrowserRef caller, EmBrowserOptions browserOptions, EmBrowserWindowOptions windowOptions, EmBrowserRect *bounds, const EmBrowserString destURL, EmBrowserRef* result)
{	//Just to demonstrate squelching of a pop-up window from thedog-club.com.
	EmBrowserChar popupURL[] = { 'h', 't', 't', 'p', ':', '/', '/', 'w', 'w', 'w', '.', 't', 'h', 'e', 'd', 'o', 'g', '-', 'c', 'l', 'u', 'b', '.', 'c', 'o', 'm', '/', 'c', 'a', 'l', '_', 'p', 'o', 'p', '.', 'h', 't', 'm', 'l', 0 };
	if (0 == EmBrowserStringCompare(destURL, popupURL))
		return 0;

	// For apps that handle multiple browser windows, you would create a new window, and return an
	// EmBrowserRef here.

	// Return zero anyway because WidgetApp does not handle multiple browser windows.
	return 0;
}

long PositionChangeNotify(EmBrowserRef widget, long x_pos, long y_pos)
{
	return true;
}

long SizeChangeNotify(EmBrowserRef widget, long width, long height)
{
	return true;
}

#pragma mark -
#pragma mark ----- SAMPLE CODE -----
#pragma mark -

void LoadURL()
{
	long retVal = 0;

	if (gWidgetPointer)
	{
		EmBrowserChar inURL[] = { 'h', 't', 't', 'p', ':', '/', '/', 'w', 'w', 'w', '.', 'o', 'p', 'e', 'r', 'a', '.', 'c', 'o', 'm', '/', 0 };

		if (widgetMethods && widgetMethods->openURL)
		{
			retVal = widgetMethods->openURL(gWidgetPointer, inURL, emBrowserURLOptionDefault);
TEST_PORT
		}
	}
}

void SetWidgetBounds(const Rect *rect)
{
	if (gWidgetPointer && widgetMethods->setLocation)
	{
		EmBrowserRect widgetPosRect;
		widgetPosRect.top = rect->top;
		widgetPosRect.left = rect->left;
		widgetPosRect.bottom = rect->bottom;
		widgetPosRect.right = rect->right;

		widgetMethods->setLocation(gWidgetPointer, &widgetPosRect,gWindow);
	}
}

void TestDocumentHandling2()
{
	EmBrowserChar operaURL[] = { 'h', 't', 't', 'p', ':', '/', '/', 'w', 'w', 'w', '.', 'o', 'p', 'e', 'r', 'a', '.', 'c', 'o', 'm', '/', 0 };
	EmBrowserString urlStringBuffer = new EmBrowserChar[512];
	long numDocs = 0;
	EmDocumentRef rootDocRef = NULL;
	EmDocumentRef parentDocRef = NULL;
	EmBrowserRef browserRefBack = NULL;

	// Get root document
	EmBrowserStatus retVal = widgetMethods->getRootDoc(gWidgetPointer,&rootDocRef);

	// Get the browser ref back.
	retVal = widgetMethods->getBrowser(rootDocRef,&browserRefBack);
	if (retVal == emBrowserNoErr)
	{
		// Get number of sub-documents
		numDocs = widgetMethods->getNumberOfSubDocuments(rootDocRef);

		EmDocumentRef* subDocArray = new EmDocumentRef[numDocs];
		EmBrowserRect* subDocRectArray = new EmBrowserRect[numDocs];

		// Fetch sub-documents and rect coordinates
		widgetMethods->getSubDocuments(rootDocRef,numDocs,subDocArray,subDocRectArray);

		// Draw rectangles around the various frames.
		RGBColor color = {0x7FFF,0x7FFF,0xFFFF};
		RGBForeColor(&color);
		PenSize(3,3);
		int x=0;
		SetPortWindowPort(gWindow);
		SetOrigin(0,0);
		Rect clipRect = {-32767,-32767,32767,32767};
		ClipRect(&clipRect);
		for (x=0;x<numDocs;x++)
		{
			Rect inRect = {	(short)subDocRectArray[x].top,
							(short)subDocRectArray[x].left,
							(short)subDocRectArray[x].bottom,
							(short)subDocRectArray[x].right};
			FrameRect(&inRect);
			ValidWindowRect(gWindow, &inRect);
		}
		PenSize(1,1);
		// Get root document source.
/*		GetDocumentSource(rootDocRef);

		int x=0;
		// Get source for all frame documents.
		for (x=0;x<numDocs;x++)
			GetDocumentSource(subDocArray[x]);

		// Re-target all frames to Opera.com
		for (x=0;x<numDocs;x++)
			widgetMethods->openURLInDoc(subDocArray[x],operaURL);
*/
		delete [] subDocArray;
		delete [] subDocRectArray;

		// A couple more tests.
		retVal = widgetMethods->getParentDocument(rootDocRef,&parentDocRef);
		retVal = widgetMethods->getURL(rootDocRef,512,urlStringBuffer,NULL);

		// Test giving a junk EmDocumentRef to a document method.
		EmDocumentRef junkRef = (EmDocumentRef)0xDEADBEEF;
		retVal = widgetMethods->getParentDocument(junkRef,&parentDocRef);
		if (retVal == emBrowserParameterErr)
		{
			// Successfully and gracefully handled junk. :)
			for (x=0;x<numDocs;x++)
				widgetMethods->openURLInDoc(subDocArray[x],operaURL, emBrowserURLOptionDefault);
		}
	}
	// De-allocate memory.
	delete [] urlStringBuffer;

	// Completely unrelated, but test security here.
	long securityOnOff = 0;
	EmBrowserString securityDesc = new EmBrowserChar[20];
	widgetMethods->getSecurityDesc(gWidgetPointer,&securityOnOff,20,securityDesc);

}

void GetDocumentSource(EmDocumentRef inDoc)
{
	long urlBufferLen = 0;

	if (widgetMethods && widgetMethods->getSource && widgetMethods->getSourceSize)
	{
		widgetMethods->getSourceSize(inDoc, &urlBufferLen);

		if (urlBufferLen)
		{
			EmBrowserString urlSourceBuffer = new EmBrowserChar[urlBufferLen];

			widgetMethods->getSource(inDoc, urlBufferLen,urlSourceBuffer);

			// Do something here...

			// De-allocate memory.
			delete [] urlSourceBuffer;
		}

	}
}

#pragma mark -
#pragma mark ----- EmBrowser API TEST FUNCTIONS -----
#pragma mark -

struct ClientDataStruct
{
	EmDocumentRef document;
	EmJavascriptObjectRef location;
};

//
//  FUNCTION: JSEvaluateCodeCallback()
//
//  PURPOSE:  Example callback for calls to JSEvaluateCode.
//

void JSEvaluateCodeCallback(void *clientData, EmJavascriptStatus status, EmJavascriptValue *value)
{
	EmBrowserChar hrefStr[] = {	'h', 'r', 'e', 'f', 0 };
	if(status == emJavascriptNoErr && value->type == emJavascriptObject)
	{
		ClientDataStruct *cds = new ClientDataStruct;
		cds->document = (EmDocumentRef) clientData;
		cds->location = value->value.object;
		widgetMethods->jsProtectObject(cds->location);

		/* Get the property "href" from the location object. */
		widgetMethods->jsGetProperty(cds->document, cds->location, (EmBrowserString)hrefStr, JSGetPropertyCallback, cds);
	}
}

//
//  FUNCTION: JSEvaluateCodeCallback2()
//
//  PURPOSE:  Example callback for calls to JSEvaluateCode. Call methods on a object
//

void JSEvaluateCodeCallback2(void *clientData, EmJavascriptStatus status, EmJavascriptValue *value)
{
	EmBrowserChar methodStr[] = { 'm', 'e', 't', 'h', 'o', 'd', 0 };
	if(status == emJavascriptNoErr && value->type == emJavascriptObject)
	{
		/* Call 'method' on the returned object */

		widgetMethods->jsCallMethod((EmDocumentRef) clientData, value->value.object, methodStr, 0, NULL, NULL, NULL);
	}
}

//
//  FUNCTION: JSEvaluateCodeCallback3()
//
//  PURPOSE:  Example callback for calls to JSEvaluateCode. Call methods on a object
//

void JSEvaluateCodeCallback3(void *clientData, EmJavascriptStatus status, EmJavascriptValue *value)
{
	EmBrowserChar methodStr[] = { 'm', 'e', 't', 'h', 'o', 'd', 0 };
	EmBrowserChar abrakadabraStr[] = { 'a', 'b', 'r', 'a', 'k', 'a', 'd', 'a', 'b', 'r', 'a', 0 };
	EmBrowserChar strStr[] = { 's', 't', 'r', 0 };

	if(status == emJavascriptNoErr && value->type == emJavascriptObject)
	{
		if (widgetMethods->jsCreateMethod && widgetMethods->jsSetProperty)
		{
            EmJavascriptObjectRef method;
            EmJavascriptValue object;
            widgetMethods->jsCreateMethod((EmDocumentRef) clientData, &method, JSMethodCallBack, NULL );
            // Give the returned window object a property called 'eksos' that has the value of our function
            object.type = emJavascriptObject;
            object.value.object = method;
       		widgetMethods->jsSetProperty((EmDocumentRef) clientData, value->value.object, methodStr, (EmJavascriptValue *)&object, NULL, NULL);
            object.type = emJavascriptString;
            object.value.string = abrakadabraStr;
       		widgetMethods->jsSetProperty((EmDocumentRef) clientData, value->value.object, strStr, &object, NULL, NULL);
        }
	}
}

//
//  FUNCTION: JSMethodCallBack()
//
//  PURPOSE:  Example callback for calls to JSEvaluateCode. Call methods on a object
//

EmJavascriptStatus JSMethodCallBack(void *inData, EmDocumentRef inDoc, EmJavascriptObjectRef inObject,
                      unsigned argumentsCount, EmJavascriptValue *argumentsArray,
                      EmJavascriptValue *returnValue)
{
    unsigned i;
    DialogRef dlgRef;
    DialogItemIndex itemHit;

#ifndef __MACH__
	if(noErr != LoadFrameworks())
	{
		return emBrowserGenericErr;
	}
#endif

    for(i=0; i<argumentsCount; i++)
    {
        EmJavascriptValue a = argumentsArray[i];
		CFStringRef cfstr = NULL;

        if(a.type == emJavascriptString)
        {
        	cfstr = CFStringCreateWithCharacters(NULL, (UniChar*)a.value.string, EmBrowserStringLen(a.value.string));
        	if(cfstr)
        	{
        		if(noErr == CreateStandardAlertPtr(kAlertPlainAlert, CFSTR("Message from page"), cfstr, NULL, &dlgRef))
	       		{
	       			RunStandardAlertPtr(dlgRef, NULL, &itemHit);
	       		}
	       		CFRelease(cfstr);
        	}
        }
    }
	return emBrowserNoErr;
}

//
//  FUNCTION: JSGetPropertyCodeCallback()
//
//  PURPOSE:  Example callback for calls to JSGetProperty.
//

void JSGetPropertyCallback(void *clientData, EmJavascriptStatus status, EmJavascriptValue *value)
{
	ClientDataStruct *cds = (ClientDataStruct *) clientData;
	EmBrowserChar operaURL[] = { 'h', 't', 't', 'p', ':', '/', '/', 'w', 'w', 'w', '.', 'o', 'p', 'e', 'r', 'a', '.', 'c', 'o', 'm', '/', 0};
	EmBrowserChar hrefStr[] = {	'h', 'r', 'e', 'f', 0 };
	EmBrowserChar microsoftURL[] = { 'h', 't', 't', 'p', ':', '/', '/', 'w', 'w', 'w', '.', 'm', 'i', 'c', 'r', 'o', 's', 'o', 'f', 't', '.', 'c', 'o', 'm', '/', 0};

	if(status == emJavascriptNoErr && value->type == emJavascriptString)
	{
		/* If the property "href" begins with "http://www.microsoft.com/" ... */
		if (EmBrowserStringLen(value->value.string) >= EmBrowserStringLen(microsoftURL) &&
			EmBrowserStringCompare(microsoftURL, value->value.string, EmBrowserStringLen(microsoftURL)) == 0)
		{
			EmJavascriptValue new_value;
			new_value.type = emJavascriptString;
			new_value.value.string = operaURL;

			/* ... assign the new value "http://www.opera.com/" to it instead. */
			widgetMethods->jsSetProperty(cds->document, cds->location, hrefStr, &new_value, NULL, NULL);
		}
	}

	widgetMethods->jsUnprotectObject(cds->location);
	delete cds;
}


pascal void AutoScrollCallback(EventLoopTimerRef theTimer, void* userData)
{
	long x,y;
	widgetMethods->getScrollPos(gWidgetPointer, &x, &y);
	widgetMethods->setScrollPos(gWidgetPointer, x, y+20);
}



void TestAPI1()
{
	long w,h;
	widgetMethods->getContentSize(gWidgetPointer, &w, &h);

	EventTimerInterval 	timer4Interval = kEventDurationSecond;
	static EventLoopTimerUPP	timerUPP = NewEventLoopTimerUPP(AutoScrollCallback);
	static EventLoopTimerRef 	timerRef = 0;
	if (timerRef)
	{
		RemoveEventLoopTimer(timerRef);
		timerRef = 0;
	}
	else
	{
		InstallEventLoopTimer(GetMainEventLoop(), 0, kEventDurationSecond,timerUPP,NULL,&timerRef);
	}
}

void TestAPI2()
{
	const char* testFileName = "script_test.html";
	int fileNameLen = strlen(testFileName);
	if (widgetMethods->openURL)
	{
		// Correct url argument when using this
		CFBundleRef bundle = CFBundleGetMainBundle();
		if (bundle)
		{
			CFURLRef url = CFBundleCopyBundleURL(bundle);
			CFStringRef urlstr = CFURLGetString(url);
			if (urlstr)
			{
				UniChar buffer[1024];
				int len = CFStringGetLength(urlstr);
				CFStringGetCharacters(urlstr, CFRangeMake(0,len), buffer);
				for (int i=0; i<fileNameLen; i++)
				{
					buffer[len++] = testFileName[i];
				}
				buffer[len] = 0;
				CFRelease(url);
				widgetMethods->openURL(gWidgetPointer, buffer, emBrowserURLOptionDefault);
				gTestWidget = gWidgetPointer;
			}
		}
	}
}


typedef struct {
	Rect srcRect;
	CGrafPtr snapshot;
} SnapshotInfo;

pascal OSStatus	ThumbDisplayWindowHandler(EventHandlerCallRef nextHandler, EventRef inEvent, void *inUserData)
{
	UInt32 		carbonEventClass = GetEventClass(inEvent);
	UInt32 		carbonEventKind = GetEventKind(inEvent);
	SnapshotInfo* info = (SnapshotInfo*)inUserData;
	if ((carbonEventClass == kEventClassWindow))
	{
		WindowRef window = 0;
		CGrafPtr snapshot = info->snapshot;
		GetEventParameter(inEvent, kEventParamDirectObject, typeWindowRef, NULL, sizeof(window), NULL, &window);
		switch (carbonEventKind)
		{
			case kEventWindowDrawContent:
				{
					Rect destBounds;
					Point offset = {0,0};
					SetPortWindowPort(window);
					GlobalToLocal(&offset);
					GetWindowBounds(window, kWindowContentRgn, &destBounds);
					OffsetRect(&destBounds, offset.h, offset.v);
					::CopyBits(GetPortBitMapForCopyBits(snapshot), GetPortBitMapForCopyBits(GetWindowPort(window)), &info->srcRect, &destBounds, srcCopy, 0);
				}
				return noErr;
			case kEventWindowBoundsChanged:
				{
					UInt32 attr;
					GetEventParameter(inEvent, kEventParamAttributes, typeUInt32, NULL, sizeof(attr), NULL, &attr);
					if (attr & kWindowBoundsChangeSizeChanged) {
						Rect bigRect = {-32767, -32767, 32766, 32766};
						InvalWindowRect(window, &bigRect);
					}
				}
				break;
			case kEventWindowClosed:
				{
					DisposeGWorld(info->snapshot);
					delete info;
				}
				break;
		}
	}
	return eventNotHandledErr;
}

void TestAPI3()
{
	if (widgetMethods->useGrafPtr && widgetMethods->draw) // support test
	{
		EmBrowserRect bounds = {0,0,200,200};
		Rect winBounds = {48,8,248,208};

		if (widgetMethods->getLocation)
			widgetMethods->getLocation(gWidgetPointer, &bounds);

		Rect imgBounds = {0,0,bounds.bottom,bounds.right}; // NOTE: Make top/left 0,0 to make bitmap immune to SetOrigin.
		SnapshotInfo *info = new SnapshotInfo;
		info->srcRect.top = bounds.top;
		info->srcRect.left = bounds.left;
		info->srcRect.bottom = bounds.bottom;
		info->srcRect.right = bounds.right;
			//Create an offscreen to paint to
		NewGWorld(&info->snapshot, 32, &imgBounds, NULL, NULL, 0);

		if (info->snapshot) {
				// Create a window to display result in...
			WindowRef win = 0;
			static EventTypeSpec sWindowEventList[] = { {kEventClassWindow, kEventWindowDrawContent},  {kEventClassWindow, kEventWindowBoundsChanged},  {kEventClassWindow, kEventWindowClosed} };
			static EventHandlerUPP sWindowHandler = NewEventHandlerUPP(ThumbDisplayWindowHandler);
			CreateNewWindow(kDocumentWindowClass, kWindowStandardDocumentAttributes | kWindowLiveResizeAttribute | kWindowStandardHandlerAttribute, &winBounds, &win);
			InstallEventHandler(GetWindowEventTarget(win), sWindowHandler, GetEventTypeCount(sWindowEventList), sWindowEventList, info, NULL);

			// INTERESTING CODE START
			LockPixels(GetGWorldPixMap(info->snapshot));
			widgetMethods->useGrafPtr(gWidgetPointer, info->snapshot);
			widgetMethods->draw(gWidgetPointer, 0);
			widgetMethods->useGrafPtr(gWidgetPointer, 0);
			UnlockPixels(GetGWorldPixMap(info->snapshot));
			// INTERESTING CODE END

			ShowWindow(win);
		}
	}
}

//
//  FUNCTION: WackyCursor
//
//  PURPOSE:  Replace standard Opera cursors with your own
//

long WackyCursors(EmMouseCursor cursorKind)
{
	long useMyCursor = gUseWackyCursors;

	if(gUseWackyCursors)
	{
		OSStatus err = noErr;

		switch(cursorKind)
		{
			case emCursorText:
				err = SetThemeCursor(kThemeResizeRightCursor);
				break;
			case emCursorCurrentPointer:
			case emCursorURI:
				err = SetThemeCursor(kThemePlusCursor);
				break;
			case emCursorDefaultArrow:
				err = SetThemeCursor(kThemeOpenHandCursor);
				break;
			default:
				useMyCursor = 0;
				break;
		}

		if(noErr != err)
		{
			useMyCursor = 0;
		}
	}

	return useMyCursor;
}

//
//  FUNCTION: TestSuite
//
//  PURPOSE:  Run testsuite.
//

EmBrowserStatus TestSuite()
{
	EmBrowserStatus result = emBrowserGenericErr;

    if (widgetMethods && widgetMethods->openURL && gWidgetPointer)
    {
    	FSRef 		widgetappFSRef;
    	FSRef		widgetappParentFolderFSRef;
    	CFURLRef 	widgetappCFURL;
    	CFURLRef	testlogCFURL;
    	CFStringRef testlogPath;

		ProcessSerialNumber currentProcess = { 0, kCurrentProcess };
		if(noErr == GetProcessBundleLocation( &currentProcess, &widgetappFSRef ))
		{
			if(noErr == FSGetCatalogInfo(&widgetappFSRef, kFSCatInfoNone, NULL, NULL, NULL, &widgetappParentFolderFSRef))
			{
				widgetappCFURL = CFURLCreateFromFSRef(NULL, &widgetappParentFolderFSRef);
				if(widgetappCFURL)
				{
					testlogCFURL = CFURLCreateCopyAppendingPathComponent(NULL, widgetappCFURL, CFSTR("emtestlog.html"), false);
					if(testlogCFURL)
					{
#ifdef __MACH__
						testlogPath = CFURLCopyFileSystemPath(testlogCFURL, kCFURLPOSIXPathStyle);
#else
						testlogPath = CFURLCopyFileSystemPath(testlogCFURL, kCFURLHFSPathStyle);
#endif
						if(testlogPath)
						{
							char* 		testlogPathCstr;
							UniChar*	testlogURLUnistr;
							CFStringRef testlogURLstr = CFURLGetString(testlogCFURL);

							CFIndex len = CFStringGetLength(testlogPath);
							CFIndex pathlen = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8);

							testlogPathCstr = new char[pathlen];
							if(testlogPathCstr)
							{
								if(CFStringGetCString(testlogPath, testlogPathCstr, pathlen, kCFStringEncodingUTF8))
								{
									InitTestSuite(testlogPathCstr, widgetMethods, gWidgetPointer);
							        TestSuitePart(1);
							        TestSuitePart(2);
									ExitTestSuite();

									len = CFStringGetLength(testlogURLstr);
									testlogURLUnistr = new UniChar[len+1];
									if(testlogURLUnistr)
									{
										CFStringGetCharacters(testlogURLstr, CFRangeMake(0, len), testlogURLUnistr);
										testlogURLUnistr[len] = 0;

										widgetMethods->openURL(gWidgetPointer, (EmBrowserString)testlogURLUnistr, emBrowserURLOptionDefault);
										result = emBrowserNoErr;

										delete[] testlogURLUnistr;
									}


								}

								delete[] testlogPathCstr;
							}

							CFRelease(testlogPath);
						}

						CFRelease(testlogCFURL);
					}

					CFRelease(widgetappCFURL);
				}
			}
		}
    }

    return result;
}

//
//  FUNCTION: ShowThumbnail()
//
//  PURPOSE:  Create a thumbnail of the current page and display it on the screen.
//            Demonstrates the use of thumbnailRequest


EmBrowserStatus ShowThumbnail()
{
    if (
        widgetMethods->thumbnailRequest &&
        widgetMethods->getRootDoc)
    {
		EmDocumentRef document;
        unsigned short url[128];

        widgetMethods->getRootDoc(gWidgetPointer, &document);
        widgetMethods->getURL(document,128,url,NULL);
        widgetMethods->thumbnailRequest(url,1000,800,20,thumbnailCallback,NULL);
    }
	return emBrowserNoErr;
}

//
//  FUNCTION: thumbnailCallback()
//
//  PURPOSE:  Example callback for calls to thumbnailRequest.
//

void thumbnailCallback (IN void *imageData, IN EmThumbnailStatus status, IN EmThumbnailRef ref)
{
    static int i = 0;
    int w = 200;
    int h = 160;
    int x = (i%6)*(w+10)+10;
    int y = (i/6)*(h+10)+10; i++;

	CGImageRef 	cgimage = 0;
	CGrafPtr	grafptr = 0;

#ifndef __MACH__
	if(noErr != LoadFrameworks())
	{
		widgetMethods->thumbnailKill(ref);
		return;
	}
#endif

	if(!imageData)
	{
		return;
	}

	if(CFGetTypeID(imageData) == CGImageGetTypeIDPtr())
	{
		cgimage = (CGImageRef)imageData;
		CGImageRetainPtr(cgimage);
	}
	else
	{
		grafptr = (CGrafPtr)imageData;
	}

	Rect wRect;
	wRect.top = 100;
	wRect.left = 100;
	wRect.right = 100 + w;
	wRect.bottom = 100 + h;
	WindowRef window;

	if(grafptr || cgimage)
	{
		OSStatus err = CreateNewWindow(kDocumentWindowClass, kWindowCloseBoxAttribute | kWindowStandardHandlerAttribute, &wRect, &window);
		InstallStandardEventHandler(GetWindowEventTarget(window));
	    ShowWindow(window);

		if(cgimage)
	    {
	    	CGContextRef context;
	    	CGRect cgrect = CGRectMake(0,0,w,h);
	    	if(noErr == CreateCGContextForPortPtr(GetWindowPort(window), &context))
	    	{
	    		SyncCGContextOriginWithPortPtr(context, GetWindowPort(window));
	    		//CGContextSetRGBFillColor(context, 1.0, 0, 0, 0.5);
	    		//CGContextFillRect(context, cgrect);
		    	CGContextDrawImagePtr(context, cgrect, cgimage);
		    	CGContextFlushPtr(context);
		    	CGContextReleasePtr(context);
	    	}
	    }
	    else if(grafptr)
		{
		    CGrafPtr oldPort;
		    Boolean changedPort = QDSwapPort(GetWindowPort(window),&oldPort);
			Rect r = { 0, 0, h, w };

			SetOrigin(0,0);
			ForeColor(blackColor);
			BackColor(whiteColor);
			//::PaintRect(&r);

			CopyBits(GetPortBitMapForCopyBits(grafptr), GetPortBitMapForCopyBits(GetWindowPort(window)), &r, &r, srcCopy, NULL);

			if(changedPort)
			{
				SetPort(oldPort);
	    	}
		}
	}

	if(cgimage)
		CGImageReleasePtr(cgimage);

	widgetMethods->thumbnailKill(ref);
}

#pragma mark -

//
// These functions are just here as a simple example of how to handle unicode strings, other ways to handle strings
// could be to use CFString methods or wchar-functions.
//
size_t EmBrowserStringLen(EmBrowserString inStr)
{
	size_t result = 0;
	if(inStr)
	{
		while(*inStr++)
		{
			result++;
		}
	}

	return result;
}

int EmBrowserStringCompare(EmBrowserString inStr1, EmBrowserString inStr2, size_t inNumChars)
{
	int result = 0;
	if(inStr1 && inStr2)
	{
		while(inNumChars--)
		{
			if(*inStr1 && *inStr2)
			{
				if(*inStr1 == *inStr2)
				{
					inStr1++;
					inStr2++;
				}
				else
				{
					break;
				}
			}
			else
			{
				break;
			}
		}

		result = *inStr1 - *inStr2;
	}
	return result;
}

EmBrowserString EmBrowserStringChr(EmBrowserString str, EmBrowserChar c)
{
	EmBrowserString result = 0;
	EmBrowserString curPtr = str;

	if(str)
	{
		while((*curPtr != c) && (*curPtr != 0))
		{
			curPtr++;
		}

		result = curPtr;
	}

	return result;
}

Boolean ConvertUTF8ToEmBrowserString(const char* inutf8str, EmBrowserString outBuffer, size_t outBufferLen)
{
	CFStringRef cfstr;

	if(inutf8str && outBuffer)
	{
		cfstr = CFStringCreateWithCString(NULL, inutf8str, kCFStringEncodingUTF8);
		if(cfstr)
		{
			CFIndex len = CFStringGetLength(cfstr);
			CFIndex maxLen = (len < (outBufferLen-1)) ? len : outBufferLen - 1;

			CFStringGetCharacters(cfstr, CFRangeMake(0, maxLen), outBuffer);
			outBuffer[maxLen] = 0;

			CFRelease(cfstr);
			return true;
		}
	}

	return false;
}

OSStatus LoadFrameworks()
{
	static Boolean hasLoaded = false;
	FSRef frameworksFolderRef;
	CFURLRef baseURL;
	CFURLRef AppServicesBundleURL;
	CFURLRef CarbonBundleURL;
	CFBundleRef AppServicesBundle;
	CFBundleRef CarbonBundle;

	if(hasLoaded)
		return noErr;

	// Find the frameworks folder and create a URL for it
	OSStatus err = FSFindFolder(kOnAppropriateDisk, kFrameworksFolderType, true, &frameworksFolderRef);
	if(err == noErr)
	{
		baseURL = CFURLCreateFromFSRef(kCFAllocatorSystemDefault, &frameworksFolderRef);
		if(!baseURL)
			err = coreFoundationUnknownErr;
	}

	if(err == noErr)
	{
		AppServicesBundleURL = CFURLCreateCopyAppendingPathComponent(kCFAllocatorSystemDefault, baseURL, CFSTR("ApplicationServices.framework"), false);
		if(!AppServicesBundleURL)
			err = coreFoundationUnknownErr;
	}

	if(err == noErr)
	{
		CarbonBundleURL = CFURLCreateCopyAppendingPathComponent(kCFAllocatorSystemDefault, baseURL, CFSTR("Carbon.framework"), false);
		if(!CarbonBundleURL)
			err = coreFoundationUnknownErr;
	}

	if(err == noErr)
	{
		CarbonBundle = CFBundleCreate(kCFAllocatorSystemDefault, CarbonBundleURL);
		if(!CarbonBundle)
			err = coreFoundationUnknownErr;
	}

	if(err == noErr)
	{
		AppServicesBundle = CFBundleCreate(kCFAllocatorSystemDefault, AppServicesBundleURL);
		if(!AppServicesBundle)
			err = coreFoundationUnknownErr;
	}

	if(err == noErr)
	{
		if(!CFBundleLoadExecutable(AppServicesBundle))
			err = coreFoundationUnknownErr;
	}

	if(err == noErr)
	{
		if(!CFBundleLoadExecutable(CarbonBundle))
			err = coreFoundationUnknownErr;
	}

	if(err == noErr)
	{
		RunStandardAlertPtr = (RunStandardAlertProcPtr) CFBundleGetFunctionPointerForName(CarbonBundle, CFSTR("RunStandardAlert"));
		CreateStandardAlertPtr = (CreateStandardAlertProcPtr) CFBundleGetFunctionPointerForName(CarbonBundle, CFSTR("CreateStandardAlert"));

		if(!RunStandardAlertPtr || !CreateStandardAlertPtr)
		{
			err = coreFoundationUnknownErr;
		}
	}

	if(err == noErr)
	{
		void* funcTabl[8];

		const CFStringRef strTabl[8] =
		{
			CFSTR("CGImageGetTypeID"),
			CFSTR("CGImageRelease"),
			CFSTR("CGContextRelease"),
			CFSTR("CGContextFlush"),
			CFSTR("CGContextDrawImage"),
			CFSTR("SyncCGContextOriginWithPort"),
			CFSTR("CreateCGContextForPort"),
			CFSTR("CGImageRetain")
		};

		CFArrayRef funcArray = CFArrayCreate(NULL, (const void**)strTabl, 8, NULL);
		if(!funcArray)
			err = coreFoundationUnknownErr;

		if(err == noErr)
		{
			CFBundleGetFunctionPointersForNames(AppServicesBundle, funcArray, funcTabl);
			for(int i = 0; i < 8; i++)
			{
				if(!funcTabl[i])
				{
					err = -1;
					break;
				}
			}

			if(err == noErr)
			{
				CGImageGetTypeIDPtr				= (CGImageGetTypeIDProcPtr) funcTabl[0];
				CGImageReleasePtr				= (CGImageReleaseProcPtr) funcTabl[1];
				CGContextReleasePtr				= (CGContextReleaseProcPtr) funcTabl[2];
				CGContextFlushPtr				= (CGContextFlushProcPtr) funcTabl[3];
				CGContextDrawImagePtr			= (CGContextDrawImageProcPtr) funcTabl[4];
				SyncCGContextOriginWithPortPtr	= (SyncCGContextOriginWithPortProcPtr) funcTabl[5];
				CreateCGContextForPortPtr		= (CreateCGContextForPortProcPtr) funcTabl[6];
				CGImageRetainPtr 				= (CGImageRetainProcPtr) funcTabl[7];
			}

			CFRelease(funcArray);
		}
	}

	if(err == noErr)
		hasLoaded = true;

	return err;
}
