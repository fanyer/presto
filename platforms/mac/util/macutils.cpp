/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/util/macutils.h"
#include "platforms/mac/util/CTextConverter.h"
#include "platforms/mac/File/FileUtils_Mac.h"
#include "platforms/mac/folderdefines.h"
#include "platforms/mac/Resources/buildnum.h"
#include "platforms/mac/bundledefines.h"
#ifndef NO_CARBON
#include "platforms/mac/model/MacSound.h"
#endif
#include "adjunct/embrowser/EmBrowser_main.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick/hotlist/HotlistModel.h"
#include "platforms/mac/pi/MacOpSystemInfo.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/url/url_tools.h"
#include "modules/util/opfile/opfile.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/encodings/encoders/utf8-encoder.h"
#include "modules/encodings/decoders/utf8-decoder.h"

#ifdef USE_COMMON_RESOURCES
#include "adjunct/desktop_util/resources/pi/opdesktopresources.h"
#endif // USE_COMMON_RESOURCES

#include "adjunct/desktop_pi/desktop_pi_util.h"

#include "platforms/mac/pi/CocoaOpWindow.h"
#include "modules/libgogi/pi_impl/mde_opview.h"
#include "platforms/mac/util/MachOCompatibility.h"

#include "modules/scope/src/scope_manager.h"
#include "adjunct/desktop_scope/src/scope_desktop_window_manager.h"
#include "platforms/mac/pi/desktop/MacSystemInputPI.h"

#include <OpenGL/OpenGL.h>

#define kBundleNoSubDir NULL
#define MAX_DISPLAYS	8

char *GetVersionString()
{
	return VER_NUM_STR;
#if 0
	Handle		versionHandle;
	static char	str[32] = {'\0'};		// We don't need much space for this string.
	long		l;

	versionHandle = GetResource('vers', 1);
	if(versionHandle)
	{
		l = (*versionHandle)[6];
		memcpy(str, &(*versionHandle)[7], l);
		str[l] = '\0';
		ReleaseResource(versionHandle);
	}
	return(str);	//	"5.0 tp4.303"
#endif
}

long GetBuildNumber()
{
	return(VER_BUILD_NUMBER);
}

int GetVideoRamSize()
{
	static int videomemory = 0;

	if (videomemory == 0)
	{
		CGLError err;
		GLint rendCount;
		CGLRendererInfoObj rendInfo;
		GLint value;

		err = CGLQueryRendererInfo(CGDisplayIDToOpenGLDisplayMask(CGMainDisplayID()), &rendInfo, &rendCount);
		if (err == kCGLNoError)
		{
			/* Not interested in multiple renderers, just get the first */
			err = CGLDescribeRenderer(rendInfo, 0, kCGLRPVideoMemory, &value);
			if (err == kCGLNoError)
				videomemory = value;
			CGLDestroyRendererInfo(rendInfo);
		}
	}

	return videomemory;
}

int GetMaxScreenSize()
{
	static int maxscreensize = 0;
	
	if (maxscreensize == 0)
	{
		CGDirectDisplayID displayId[MAX_DISPLAYS];
		CGError err;
		uint32_t count, i = 0;
		
		err = CGGetActiveDisplayList(MAX_DISPLAYS, displayId, &count);
		while (err == kCGErrorSuccess && i < count)
		{
			CGRect r = CGDisplayBounds(displayId[i++]);
			maxscreensize = MAX(maxscreensize, r.size.width*r.size.height);
		}
	}
	
	return maxscreensize;
}

int GetMaxScreenDimension()
{
	static int maxscreendim = 0;
	
	if (maxscreendim == 0)
	{
		CGDirectDisplayID displayId[MAX_DISPLAYS];
		CGError err;
		uint32_t count, i = 0;
		
		err = CGGetActiveDisplayList(MAX_DISPLAYS, displayId, &count);
		while (err == kCGErrorSuccess && i < count)
		{
			CGRect r = CGDisplayBounds(displayId[i++]);
			maxscreendim = MAX(MAX(maxscreendim, r.size.width), r.size.height);
		}
	}
	
	return maxscreendim;
}

OSErr FindPSN(ProcessSerialNumber& inoutViewerProcess, FourCharCode inAppCreatorCode, FourCharCode inType)
{
	ProcessInfoRec info = {sizeof(ProcessInfoRec),};

	while (noErr == GetNextProcess(&inoutViewerProcess))
	{
		if (noErr == GetProcessInformation(&inoutViewerProcess, &info))
		{
			if (info.processSignature == inAppCreatorCode)
			{
				return noErr;
			}
		}
	}

	return -1;
}

void ActivateFinder(Boolean firstWindowOnly)
{
	ProcessSerialNumber	psnFinder = {kNoProcess, kNoProcess};

	OSStatus	status = FindPSN(psnFinder,
								 FOUR_CHAR_CODE('MACS'),
								 FOUR_CHAR_CODE('FNDR') );

	if (status == noErr) {
		if (firstWindowOnly)
			::SetFrontProcessWithOptions(&psnFinder, kSetFrontProcessFrontWindowOnly);
		else
			::SetFrontProcess(&psnFinder);
	}
}

Boolean OpenFileOrFolder(FSRefPtr inSpec)
{
	OSErr err;
	AppleEvent theAEvent, theReply;
	AEAddressDesc fndrAddress;
	AEDescList targetListDesc;
	OSType fndrCreator = 'MACS';
	Boolean eventSent = false;
	AliasHandle targetAlias = NULL;

		/* set up locals */
	AECreateDesc(typeNull, NULL, 0, &theAEvent);
	AECreateDesc(typeNull, NULL, 0, &fndrAddress);
	AECreateDesc(typeNull, NULL, 0, &theReply);
	AECreateDesc(typeNull, NULL, 0, &targetListDesc);

		/* create an open documents event targeting the finder */
	err = AECreateDesc(typeApplSignature, (Ptr) &fndrCreator, sizeof(fndrCreator), &fndrAddress);
	if (noErr == err)
	{
		err = AECreateAppleEvent(kCoreEventClass, kAEOpenDocuments, &fndrAddress, kAutoGenerateReturnID, kAnyTransactionID, &theAEvent);
		if (noErr == err)
		{
				/* create the list of files to open */
			err = AECreateList(NULL, 0, false, &targetListDesc);
			if (noErr == err)
			{
				err = FSNewAlias(NULL, inSpec, &targetAlias);
				if (noErr == err)
				{
					HLock((Handle) targetAlias);
					err = AEPutPtr(&targetListDesc, 1, typeAlias, *targetAlias, GetHandleSize((Handle) targetAlias));
					HUnlock((Handle) targetAlias);
					if (noErr == err)
					{
							/* add the file list to the apple event */
						err = AEPutParamDesc(&theAEvent, keyDirectObject, &targetListDesc);
						if (noErr == err)
						{
								/* send the event to the Finder */
#ifdef NO_CARBON
							err = AESendMessage(&theAEvent, &theReply, kAENoReply, kAEDefaultTimeout);
#else
							err = AESend(&theAEvent, &theReply, kAENoReply, kAENormalPriority, kAEDefaultTimeout, NULL, NULL);
#endif
							if (err == noErr)
							{
								eventSent = true;
							}
						}
					}
					DisposeHandle((Handle) targetAlias);
				}
			}
		}
	}
		/* clean up and leave */
	AEDisposeDesc(&targetListDesc);
	AEDisposeDesc(&theAEvent);
	AEDisposeDesc(&fndrAddress);
	AEDisposeDesc(&theReply);
	return eventSent;
}

Boolean MacGetDefaultApplicationPath(IN const uni_char* url, OUT OpString& appName, OUT OpString& appPath)
{
	CFURLRef macURL = 0;
	Boolean  result = false;
	FSRef    appFSRef;

	if(url)
	{
		macURL = CFURLCreateWithBytes(NULL, (const UInt8*)url, sizeof(uni_char)*uni_strlen(url), kCFStringEncodingUnicode, NULL);
		if(macURL)
		{
			if(LSGetApplicationForURL(macURL, kLSRolesAll, &appFSRef, NULL) == noErr)
			{
				char p[MAX_PATH];
				memset(p, '\0', MAX_PATH*sizeof(char));
				if (noErr != FSRefMakePath(&appFSRef, (UInt8*)p, MAX_PATH-1)) return result;
				RETURN_VALUE_IF_ERROR(appPath.SetFromUTF8(p), result);
				const uni_char* uni_appname = uni_strrchr(appPath.CStr(), UNI_L(PATHSEPCHAR));
				if(uni_appname)
				{
					RETURN_VALUE_IF_ERROR(appName.Set(++uni_appname), result);	// skip PATHSEPCHAR
				}
				result = true;
			}
			CFRelease(macURL);
		}
	}

	return result;
}

Boolean MacOpenFileInApplication(const uni_char *file, const uni_char *application)
{
	FSRef				appFSRef;
	LSLaunchFSRefSpec	launchInfo;
	FSRef				docFSRef;
	OpString			filepath;

#ifdef WIDGET_RUNTIME_SUPPORT
	bool asApp = false;
	bool asWidget = false;

	OpString rawParams;
	rawParams.Set(file, uni_strlen(file));
	int pos;
	
	if ((pos = rawParams.Find("-widget")) != KNotFound)
	{
		asWidget = true;
	}
	else if ((pos = rawParams.Find("-app")) != KNotFound)
	{
		asApp = true;
	}
	
	if (asApp)
	{
		filepath.Set(rawParams.SubString((unsigned int)op_strlen("-app"), KAll, NULL).CStr());
	}
	else if (asWidget)
	{
		filepath.Set(rawParams.SubString((unsigned int)op_strlen("-widget"), KAll, NULL).CStr());
	}
	else
	{
		filepath.Set(file);
	}
	
	filepath.Strip(TRUE, TRUE);
#else
	if(OpStatus::IsError(filepath.Set(file)))
	{
		return false;
	}
#endif
	
	// if we get enclosed in quotes this is the time to remove them...
	if(filepath.Length() > 2)
	{
		uni_char qoute = UNI_L('"');
		if( (filepath[0] == qoute) && (filepath[filepath.Length() - 1] == qoute) )
		{
			filepath.Set(filepath.SubString(1, filepath.Length()-2).CStr());
		}
	}

	if(OpFileUtils::ConvertUniPathToFSRef(filepath, docFSRef))
	{
		FSCatalogInfoBitmap what = kFSCatInfoNodeFlags;
		FSCatalogInfo folder_info;
		// FIXME: ismailp - following fsspec may be invalid. check folder_fss
		FSGetCatalogInfo(&docFSRef, what, &folder_info, NULL, NULL, NULL);
		if (folder_info.nodeFlags & (1 << 4))
		{
			// This is a folder. Open it in Finder and activate
			if (OpenFileOrFolder(&docFSRef /*&folder_fss*/))
				ActivateFinder(true);
		}
#ifdef WIDGET_RUNTIME_SUPPORT
		else if (application && (asApp || asWidget))
		{
			{
				CFStringRef param1Ref = asApp ? 
				CFSTR("-app") : CFSTR("-widget");
				
				CFStringRef param2Ref = CFStringCreateWithCharacters(NULL,
								(UniChar *)filepath.CStr(), filepath.Size());
				
				CFMutableArrayRef paramRef = CFArrayCreateMutable(NULL, 2, NULL);
				
				CFArrayInsertValueAtIndex(paramRef, 0, param1Ref);
				CFArrayInsertValueAtIndex(paramRef, 1, param2Ref);
				
				LSApplicationParameters params;
				
				OpFileUtils::ConvertUniPathToFSRef(application, appFSRef);
				
				params.version = 0; 
				params.flags = kLSLaunchNewInstance;
				params.environment = NULL;
				params.application = &appFSRef;
				params.asyncLaunchRefCon = NULL;
				params.initialEvent = NULL;
				params.argv = paramRef;
				
				LSOpenApplication(&params, NULL);
				
				CFRelease(param1Ref);
				CFRelease(param2Ref);
				CFRelease(paramRef);
				
				return true;				
			}
		}    
#endif // WIDGET_RUNTIME_SUPPORT
		else
		{
			if(application)
			{
				if(!OpFileUtils::ConvertUniPathToFSRef(application, appFSRef))
				{
					return false;
				}
			}
			else
			{
				OpString content_type;
				OpString file_handler;
				if(OpStatus::IsError(g_op_system_info->GetFileHandler(&filepath, content_type, file_handler)))
				{
					return false;
				}
				if(!OpFileUtils::ConvertUniPathToFSRef(file_handler, appFSRef))
				{
					return false;
				}
			}
			launchInfo.appRef = &appFSRef;
			launchInfo.numDocs = 1;
			launchInfo.itemRefs = &docFSRef;
			launchInfo.passThruParams = NULL;
			launchInfo.launchFlags = kLSLaunchDefaults;
			launchInfo.asyncRefCon = 0;

			if(noErr == LSOpenFromRefSpec(&launchInfo, NULL))
			{
				return true;
			}
		}
	}
	else if (uni_strchr(file, ':'))
	{
		return MacOpenURLInApplication(file, application);
	}
	return false;
}

Boolean MacOpenURLInTrustedApplication(const uni_char *url)
{
	OpString protocol;
	OpString appname;
	BOOL bHandled = FALSE;

	uni_char *p = uni_strstr(url,UNI_L("://"));

	if(p)
	{
		OpString urlcopy;
		urlcopy.Set(url);

		protocol.Set(urlcopy.SubString(0, p - url).CStr());

		TrustedProtocolData data;
		int index = g_pcdoc->GetTrustedProtocolInfo(protocol, data);
		if (index >= 0)
		{
			if (data.viewer_mode == UseDefaultApplication)
			{
				OpString appname;
				if (OpStatus::IsSuccess(appname.Set(data.filename)))
				{
					OpString defaultAppName;
					OpString app;
					OpString protocolstring;

					protocolstring.Set(protocol);
					protocolstring.Append(UNI_L(":"));

					if(MacGetDefaultApplicationPath(protocolstring.CStr(), app, defaultAppName))
					{
						appname.Set(defaultAppName);
					}

					bHandled = MacOpenURLInApplication(url, appname.CStr());
				}
			}
		}
	}

	return bHandled;
}

void SetMailtoString(OpString &mailto, const uni_char* to, const uni_char* cc, const uni_char* bcc, const uni_char* subject, const uni_char* message)
{
	mailto.Set(UNI_L("mailto:"));

	if(to)
	{
		mailto.Append(to);
	}

	mailto.Append(UNI_L("?"));

	if(cc)
	{
		mailto.AppendFormat(UNI_L("cc=%s"),cc);
		mailto.Append(UNI_L("&"));
	}

	if(bcc)
	{
		mailto.AppendFormat(UNI_L("bcc=%s"),bcc);
		mailto.Append(UNI_L("&"));
	}

	if(subject)
	{
		mailto.Append(UNI_L("subject="));

		uni_char* escaped = new uni_char[uni_strlen(subject)*3+1];

		EscapeFileURL(escaped, (uni_char *)subject);

		mailto.Append(escaped);

		delete [] escaped;

		mailto.Append(UNI_L("&"));
	}

	if(message)
	{
		mailto.Append(UNI_L("body="));

		uni_char* escaped = new uni_char[uni_strlen(message)*3+1];

		EscapeFileURL(escaped, (uni_char*)message, TRUE);

		mailto.Append(escaped);

		delete [] escaped;
	}
}

void ComposeExternalMail(const uni_char* to, const uni_char* cc, const uni_char* bcc, const uni_char* subject, const uni_char* message, const uni_char* raw_address, int mailhandler)
{
#ifdef M2_SUPPORT
	if (mailhandler == _MAILHANDLER_FIRST_ENUM)
	{
		mailhandler = g_pcui->GetIntegerPref(PrefsCollectionUI::MailHandler);
#else
		mailhandler = MAILHANDLER_SYSTEM;
#endif // M2_SUPPORT
	}

	switch (mailhandler)
	{
		case MAILHANDLER_SYSTEM:
			{
				OpString mailto;
				if (raw_address)
					mailto.Set(raw_address);
				else
					SetMailtoString(mailto,to,cc,bcc,subject,message);
				MacOpenURLInDefaultApplication(mailto);
				break;
			}

		case MAILHANDLER_APPLICATION:
			{
				OpStringC extApp;
				extApp = g_pcapp->GetStringPref(PrefsCollectionApp::ExternalMailClient);
				if(0 == extApp.Length())
				{
					OpStatus::Ignore(SimpleDialog::ShowDialog(WINDOW_NAME_MESSAGE_DIALOG, NULL, Str::SI_IDSTR_MSG_NO_EXT_MAIL_APP_DEFINED_TXT, Str::SI_IDSTR_MSG_NO_EXT_MAIL_APP_DEFINED_CAP, Dialog::TYPE_OK, Dialog::IMAGE_ERROR));
					return;
				}

				OpString mailto;
				if (raw_address)
					mailto.Set(raw_address);
				else
					SetMailtoString(mailto,to,cc,bcc,subject,message);

				if(!MacOpenURLInApplication(mailto, extApp.CStr()))
				{
					OpString errMsg;
					TRAPD(err, g_languageManager->GetStringL(Str::SI_IDSTR_MSG_FAILED_STARTING_EXT_MAIL_APP_TXT, errMsg));
					errMsg.Append(UNI_L("\n"));
					errMsg.Append(extApp.CStr());

					OpString title, msg;
					if (OpStatus::IsSuccess(g_languageManager->GetString(Str::SI_IDSTR_MSG_FAILED_STARTING_EXT_MAIL_APP_CAP, title)))
						OpStatus::Ignore(SimpleDialog::ShowDialog(WINDOW_NAME_MESSAGE_DIALOG, NULL, errMsg.CStr(), title.CStr(), Dialog::TYPE_OK, Dialog::IMAGE_ERROR));
				}
			}
			break;
	}
}

Boolean stripquotes(OpString& str)
{
	static const uni_char qoute = UNI_L('"');

	if(str.Length() > 2)
	{
		if (str[0] == qoute)
		{
			const uni_char* start = str.CStr() + 1;
			const uni_char* end = uni_strchr(start, qoute);
			if (end)
				str.Set(start, end-start);
			else
				return false;
		}
	}

	return true;
}

Boolean MacOpenURLInApplication(const uni_char* url, const uni_char* appPath, bool new_instance)
{
	CFURLRef			macURL = 0;
	CFURLRef			appURL = 0;
	FSRef				appFSRef;
	CFMutableArrayRef	urlArray;
	Boolean				result = false;
	LSLaunchURLSpec		launchInfo;
	OpString			filepath;
	OpString			strippedurl;

	// if we get enclosed in quotes this is the time to remove them...
	if(OpStatus::IsSuccess(filepath.Set(appPath)))
	{
		if (!stripquotes(filepath))
			return false;
	}
	if(OpStatus::IsSuccess(strippedurl.Set(url)))
	{
		if (!stripquotes(strippedurl))
			return false;
	}

	if(url)
	{
		macURL = CFURLCreateWithBytes(NULL, (const UInt8*)strippedurl.CStr(), 2*uni_strlen(strippedurl.CStr()), kCFStringEncodingUnicode, NULL);
		CFShow(macURL);
		
		if(macURL)
		{
			appURL = CFURLCreateWithBytes(NULL, (const UInt8*)filepath.CStr(), 2*uni_strlen(filepath.CStr()), kCFStringEncodingUnicode, NULL);
			CFShow(appURL);
			if(appURL)
			{
				// This is just to test that the appPath is valid
				if(CFURLGetFSRef(appURL, &appFSRef))
				{
					urlArray = CFArrayCreateMutable(NULL, 1, NULL);
					if(urlArray)
					{
						CFArrayAppendValue(urlArray, macURL);

						launchInfo.appURL = appURL;
						launchInfo.itemURLs = urlArray;
						launchInfo.passThruParams = NULL;
						launchInfo.launchFlags = kLSLaunchDefaults;
						if (new_instance) {
							launchInfo.launchFlags |= kLSLaunchNewInstance;
						}
						launchInfo.asyncRefCon = 0;

						if(noErr == LSOpenFromURLSpec(&launchInfo, NULL))
						{
							result = true;
						}

						CFRelease(urlArray);
					}
				}

				CFRelease(appURL);
			}
		}
	}

	if(macURL)
		CFRelease(macURL);

	return result;
}

Boolean MacOpenURLInDefaultApplication(const uni_char* url, bool inOpera)
{
	CFURLRef			macURL = 0;
	OSStatus			err;
	CFURLRef			appURL = 0;
	CFMutableArrayRef	urlArray;
	Boolean				result = false;
	LSLaunchURLSpec		launchInfo;

	if(url)
	{
		macURL = CFURLCreateWithBytes(NULL, (const UInt8*)url, 2*uni_strlen(url), kCFStringEncodingUnicode, NULL);
		if(macURL)
		{
			if (inOpera)
			{
				err = LSFindApplicationForInfo(kLSUnknownCreator, GetCFBundleID(), NULL, NULL, &appURL);
			}
			else
				err = LSGetApplicationForURL(macURL, kLSRolesAll, NULL, &appURL);
			if(err == noErr)
			{
				urlArray = CFArrayCreateMutable(NULL, 1, NULL);
				if(urlArray)
				{
					CFArrayAppendValue(urlArray, macURL);

					launchInfo.appURL = appURL;
					launchInfo.itemURLs = urlArray;
					launchInfo.passThruParams = NULL;
					launchInfo.launchFlags = kLSLaunchDefaults;
					launchInfo.asyncRefCon = 0;

					if(noErr == LSOpenFromURLSpec(&launchInfo, NULL))
					{
						result = true;
					}

					CFRelease(urlArray);
				}
			}
			else if(err == kLSApplicationNotFoundErr)
			{
				result = false;
			}
		}
	}

	if(macURL)
		CFRelease(macURL);

	return result;
}

Boolean MacExecuteProgram(const uni_char* program, const uni_char* parameters)
{
	if (program && uni_strstr(program, UNI_L("://")))
	{
		return MacOpenURLInDefaultApplication(program);
	}
	else
	{
		if (parameters && uni_strstr(parameters, UNI_L("://")))
		{
			if (program)
				return MacOpenURLInApplication(parameters, program);
			else
				return MacOpenURLInDefaultApplication(parameters);
		}
		else if (program && parameters)
		{
			return MacOpenFileInApplication(parameters, program);
		}
		else if (program)
		{
			// Without parameters it's trying to just open a folder
			// so dump it into the parameters without an application, confused? :)
			return MacOpenFileInApplication(program, NULL);
		}
	}

	return false;
}

#ifndef NO_CARBON
BOOL MacPlaySound(const uni_char* fullfilepath, BOOL async)
{
	MacOpSound *snd = MacOpSound::GetSound(fullfilepath);
	if(snd)
	{
		if(OpStatus::IsSuccess(snd->Init()))
		{
			if(OpStatus::IsSuccess(snd->Play(async)))
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}
#endif

#pragma mark -

Boolean FindUniqueFileName(OpString &path, const OpString &suggested)
{
	OpString8 utf8;
	int val;
	if (suggested.HasContent())
	{
		path.Reserve(suggested.Length()+8);
		path.Append(suggested.CStr());
		OpFile file;
		BOOL exists;
		file.Construct(path);
		if (OpStatus::IsSuccess(file.Exists(exists)) && !exists)
		{
			return TRUE;
		}
		else
		{
			path.Delete(path.FindLastOf(UNI_L('/'))+1);
			int index = suggested.FindLastOf(UNI_L('.'));
			if (index >= 0) {
				path.Append(suggested.CStr(), index);
				path.Append(UNI_L("XXXXXX"));
				path.Append(suggested.CStr()+index);
				utf8.SetUTF8FromUTF16(path.CStr());
				val = mkstemps(utf8.CStr(), uni_strlen(suggested.CStr()+index));
				path.SetFromUTF8(utf8.CStr());
				return (val != -1);
			} else {
				path.Append(suggested.CStr());
				path.Append(UNI_L("XXXXXX"));
				utf8.SetUTF8FromUTF16(path.CStr());
				val = mkstemp(utf8.CStr());
				path.SetFromUTF8(utf8.CStr());
				return (val != -1);
			}
		}
	}
	else
	{
		path.Append(UNI_L("XXXXXX"));
		utf8.SetUTF8FromUTF16(path.CStr());
		val = mkstemp(utf8.CStr());
		path.SetFromUTF8(utf8.CStr());
		return (val != -1);
	}
}

#pragma mark -

Boolean IsCacheFolderOwner()
{
	static Boolean initialized = false;
	static Boolean owner = true;
	if (!initialized)
	{
		ProcessSerialNumber psn = {0,0};
		ProcessSerialNumber this_psn = {0,0};
		FSRef folderref, fileref;
		Boolean dir;
		OpString folder, file;
		FSIORefNum refNum;
		ByteCount len;
		Boolean same = false;
		if (gVendorDataID == 'OPRA') {
			GetCurrentProcess(&this_psn);
			OpFileUtils::SetToMacFolder(MAC_OPFOLDER_CACHES, folder);
			OpString8 utf8;
			HFSUniStr255 dataForkName;
			FSGetDataForkName(&dataForkName);
			utf8.SetUTF8FromUTF16(folder.CStr());
			if (noErr == FSPathMakeRef((const UInt8*)utf8.CStr(), &folderref, &dir)) {
				file.Set(folder);
				file.Append(OPERA_CACHE_LOCKFILE_NAME);
				utf8.SetUTF8FromUTF16(file.CStr());
				if (noErr == FSPathMakeRef((const UInt8*)utf8.CStr(), &fileref, &dir)) {
					if (noErr == FSOpenFork(&fileref, dataForkName.length, dataForkName.unicode, fsRdPerm, &refNum)) {
						len = sizeof(psn);
						if (noErr == FSReadFork(refNum, fsFromStart, 0, len, &psn, &len) && (len == sizeof(psn))) {
							if (noErr == SameProcess(&psn, &this_psn, &same) && !same) {
								ProcessInfoRec info = {sizeof(ProcessInfoRec),};
								if (noErr == GetProcessInformation(&psn, &info)) {
									if (info.processSignature == 'OPRA') {
										owner = false;
									}
								}
							}
						}
						FSCloseFork(refNum);
					}
				}
				if (owner) {
					file.Set(OPERA_CACHE_LOCKFILE_NAME);
					if (noErr == FSCreateFileUnicode(&folderref, file.Length(), (const UniChar*)file.CStr(), kFSCatInfoNone, NULL, NULL, NULL)) {
						file.Set(folder);
						file.Append(OPERA_CACHE_LOCKFILE_NAME);
						utf8.SetUTF8FromUTF16(file.CStr());
						if (noErr == FSPathMakeRef((const UInt8*)utf8.CStr(), &fileref, &dir)) {
							if (noErr == FSOpenFork(&fileref, dataForkName.length, dataForkName.unicode, fsWrPerm, &refNum)) {
								len = sizeof(psn);
								FSWriteFork(refNum, fsFromStart, 0, len, &this_psn, &len);
								FSCloseFork(refNum);
							}
						}
					}
				}
			}
		} else {
			owner = false;
		}
		initialized = true;
	}
	return owner;
}

class DeleteFolder {
public:
	DeleteFolder() {}
	void operator()(size_t count, const FSCatalogInfo* info, const FSRef* item) {
		if (info->nodeFlags & kFSNodeIsDirectoryMask)
			MacForEachItemInFolder(*item, DeleteFolder());
		FSDeleteObject(item);
	}
};

class DeleteUnusedCacheFolder {
public:
	DeleteUnusedCacheFolder(OpStringC& cachePath, int iCache) {
		mCachePath.Set(cachePath.CStr());
		mCache = iCache;
	}
	DeleteUnusedCacheFolder(const DeleteUnusedCacheFolder& copy) {
		mCachePath.Set(copy.mCachePath.CStr());
		mCache = copy.mCache;
	}

	void operator()(size_t count, const FSCatalogInfo* info, const FSRef* item) {
		OpString path;
		Boolean same = false;
		ProcessSerialNumber psn = {0,0};
		ProcessSerialNumber this_psn = {0,kCurrentProcess};
		OpFileUtils::ConvertFSRefToUniPath(item, &path);
#ifdef USE_COMMON_RESOURCES
		const uni_char* format = mCache ? UNI_L("opcache %08X%08X") : UNI_L("cache %08X%08X");
#else
		const uni_char* format = UNI_L("Cache %08X%08X");
#endif // USE_COMMON_RESOURCES
		int name_off = path.FindLastOf(PATHSEPCHAR);
		if (name_off != KNotFound) {
			uni_char* name = path.CStr()+name_off+1;
			if (2 == uni_sscanf(name, format, (unsigned int*)&psn.highLongOfPSN, (unsigned int*)&psn.lowLongOfPSN))
			{
				if (noErr == SameProcess(&psn, &this_psn, &same) && !same)
				{
					ProcessInfoRec pinfo = {sizeof(ProcessInfoRec),};
					OSStatus err = GetProcessInformation(&psn, &pinfo);
					if ((noErr != err) || (pinfo.processSignature != 'OPRA'))
					{
						MacForEachItemInFolder(*item, DeleteFolder());
						FSDeleteObject(item);
					}
				}
			}
		}
	}

	OpString mCachePath;
	int mCache;
};

void RemoveCacheLock()
{
	OSStatus err = noErr;
	if (IsCacheFolderOwner()) {
		OpString folder;
		
		OpFileUtils::SetToMacFolder(MAC_OPFOLDER_CACHES, folder);
		
		FSRef cache_folder, to_delete;
		OpFileUtils::ConvertUniPathToFSRef(folder, cache_folder);
		static OpStringC cache_file(UNI_L(OPERA_CACHE_LOCKFILE_NAME));
		err = FSMakeFSRefUnicode(&cache_folder, cache_file.Length(), (const UniChar*)cache_file.CStr(), kTextEncodingUnknown, &to_delete);
		if (noErr == err) {
			err = FSDeleteObject(&to_delete);
		}
	} else {
		// Loop twice for the Cache and OpCache folders
		for (int iCache = 0; iCache <= 1; iCache++)
		{
			OpString cachePath;
			FSRef out_folder;
			
			GetOperaCacheFolderName(cachePath, (BOOL)iCache);
			if (OpFileUtils::ConvertUniPathToFSRef(cachePath, out_folder))
				MacForEachItemInFolder(out_folder, DeleteUnusedCacheFolder(cachePath, iCache));
		}
	}
}

void CleanupOldCacheFolders()
{
	// Loop twice for the Cache and OpCache folders
	for (int iCache = 0; iCache <= 1; iCache++)
	{
		OpString cachePath;
		
		GetOperaCacheFolderName(cachePath, (BOOL)iCache, TRUE);
		FSRef cache_folder;
		
		if (OpFileUtils::ConvertUniPathToFSRef(cachePath, cache_folder))
			MacForEachItemInFolder(cache_folder, DeleteUnusedCacheFolder(cachePath, iCache));
	}
}

extern long gVendorDataID;
void RandomCharFill(void *dest, int size);

void GetOperaCacheFolderName(OpString &path, BOOL bOpCache, BOOL bParentOnly)
{
	OpFileUtils::SetToMacFolder(MAC_OPFOLDER_CACHES, path);

	//FIXME: Look at this code and work out what it was supposed to do...
	//if (bOpCache)
	//	OpFileUtils::AppendFolder(path, UNI_L("CacheOp"));

	if (!bParentOnly)
	{
		// Avoid cache conflicts for vendors of Opera.
		if (!gVendorDataID)
		{
			RandomCharFill(&gVendorDataID, 4);
		}

		if (gVendorDataID != 'OPRA')
		{
			uni_char destBuff[8];
			int len = gTextConverter->ConvertBufferFromMac((char*)&gVendorDataID,4,destBuff,7);
			destBuff[len] = 0;
			OpFileUtils::AppendFolder(path, destBuff);
		}
		else
		{
			if (IsCacheFolderOwner()) {
				OpFileUtils::AppendFolder(path, UNI_L("Cache"));
			} else {
				uni_char destBuff[32];
				ProcessSerialNumber psn;
				MacGetCurrentProcess(&psn);
				uni_sprintf(destBuff, UNI_L("Cache %08X%08X"), psn.highLongOfPSN, psn.lowLongOfPSN);
				OpFileUtils::AppendFolder(path, destBuff);
			}
		}
	}
}

void CheckAndFixCacheFolderName(OpString &path)
{
	// Avoid cache conflicts for vendors of Opera.
	if (!gVendorDataID)
	{
		RandomCharFill(&gVendorDataID, 4);
	}
	
	if (gVendorDataID != 'OPRA')
	{
		uni_char destBuff[8];
		int len = gTextConverter->ConvertBufferFromMac((char*)&gVendorDataID,4,destBuff,7);
		destBuff[len] = 0;

		// Remove any slashes from the end
		if (path[path.Length() - 1] == PATHSEPCHAR)
			path.Delete(path.Length() - 1);
		
		OpFileUtils::AppendFolder(path, destBuff);
	}
	else
	{
		// Only add to the end when the cache folder is already owned by another
		// instance of Opera
		if (!IsCacheFolderOwner()) 
		{
			uni_char destBuff[32];
			ProcessSerialNumber psn;
			MacGetCurrentProcess(&psn);
			uni_sprintf(destBuff, UNI_L(" %08X%08X"), psn.highLongOfPSN, psn.lowLongOfPSN);

			// Remove any slashes from the end
			if (path[path.Length() - 1] == PATHSEPCHAR)
				path.Delete(path.Length() - 1);
				
			// Append the process id
			path.Append(destBuff);
		}
	}
}

#pragma mark -

const uni_char * GetDefaultLanguage()
{
	static uni_char language[6];
	*language = 0;

	CFBundleRef	mainBundle = NULL;
	CFURLRef	url;
	FSRef		fsref;
	OpString	resPath;

#ifdef USE_COMMON_RESOURCES
	// Create the PI interface object
	OpDesktopResources *dr_temp; // Just for the autopointer
	RETURN_VALUE_IF_ERROR(OpDesktopResources::Create(&dr_temp), NULL);
	OpAutoPtr<OpDesktopResources> desktop_resources(dr_temp);
	if(desktop_resources.get() && OpStatus::IsSuccess(desktop_resources->GetResourceFolder(resPath)))
	{
#else
		OpFileUtils::SetToSpecialFolder(OPFILE_RESOURCES_FOLDER, resPath);
#endif // USE_COMMON_RESOURCES
		if (OpFileUtils::ConvertUniPathToFSRef(resPath, fsref))
		{
			url = CFURLCreateFromFSRef(NULL, &fsref);
			if (url)
			{
				mainBundle = CFBundleCreate(NULL, url);
				CFRelease(url);
			}
		}
#ifdef USE_COMMON_RESOURCES
	}
#endif // USE_COMMON_RESOURCES
	if (mainBundle)
	{
		CFURLRef langFileURL = CFBundleCopyResourceURL(mainBundle, CFSTR(""), CFSTR("lng"), kBundleNoSubDir);
		if (langFileURL)
		{
			CFStringRef langFilePath = CFURLCopyPath(langFileURL);
			if (langFilePath)
			{
				CFIndex len = CFStringGetLength(langFilePath);
				uni_char* pathStr = new uni_char[len + 1];
				uni_char* search, * search2;
				CFStringGetCharacters(langFilePath, CFRangeMake(0,len), (UniChar *)pathStr);
				pathStr[len] = 0;
				search = uni_strrchr(pathStr, '/');
				if (!search)
				{
					search = pathStr;
				}
				search2 = uni_strchr(search, '.');
				if (search2)
				{
					*search2 = 0;
					if (uni_strlen(search) <= 5)
					{
						uni_strcpy(language, search);
					}
				}
				delete [] pathStr;
				CFRelease(langFilePath);
			}
			CFRelease(langFileURL);
		}
		CFRelease(mainBundle);
	}
	return language;
}

OP_STATUS SetBackgroundUsingAppleEvents(URL& url)
{
	AEDesc                  tAEDesc = {typeNull, nil}; // always init AEDescs
	OSErr                   anErr = noErr;
	AliasHandle             aliasHandle=nil;
	OpFile                  pictureOpFile;
	Boolean                 wasSuccess = false;
	FSRef                   pictureFSRef;
	OpString                pictureFullPath;

	if(OpFileUtils::FindFolder(kPictureDocumentsFolderType, pictureFullPath, TRUE))
	{
		OpString tmp;
		TRAPD(err, url.GetAttributeL(URL::KSuggestedFileName_L, tmp, TRUE));
		RETURN_IF_ERROR(err);
		if(FindUniqueFileName(pictureFullPath, tmp))
		{
			if( OpStatus::IsSuccess(url.SaveAsFile(pictureFullPath)) )
			{
				wasSuccess = true;
			}
		}

		if(wasSuccess)
		{
			if (OpFileUtils::ConvertUniPathToFSRef(pictureFullPath.CStr(), pictureFSRef))
			{
				// Now we create an AEDesc containing the alias to the picture
				if(noErr == FSNewAlias(NULL, &pictureFSRef, &aliasHandle))
				{
					if(aliasHandle)
					{
						char handleState = HGetState( (Handle) aliasHandle );
						HLock( (Handle) aliasHandle);
						anErr = AECreateDesc( typeAlias, *aliasHandle, GetHandleSize((Handle) aliasHandle), &tAEDesc);
						HSetState( (Handle) aliasHandle, handleState );
						DisposeHandle( (Handle)aliasHandle );

						if(anErr == noErr)
						{
							// Now we need to build the actual Apple Event that we're going to send the Finder
							AppleEvent tAppleEvent;
							OSType sig = 'MACS'; // The app signature for the Finder
							AEBuildError tAEBuildError;
							anErr = AEBuildAppleEvent(kAECoreSuite,kAESetData,typeApplSignature,&sig,sizeof(OSType),
							                          kAutoGenerateReturnID,kAnyTransactionID,&tAppleEvent,&tAEBuildError,
							                          "'----':'obj '{want:type(prop),form:prop,seld:type('dpic'),from:'null'()},data:(@)",&tAEDesc);
							// Finally we can go ahead and send the Apple Event using AESend
							if (noErr == anErr)
							{
								AppleEvent theReply = {typeNull, nil};
#ifdef NO_CARBON
								anErr = AESendMessage(&tAppleEvent,&theReply,kAENoReply,kAEDefaultTimeout);
#else
								anErr = AESend(&tAppleEvent,&theReply,kAENoReply,kAENormalPriority,kNoTimeOut,nil,nil);
#endif
								AEDisposeDesc(&tAppleEvent); // Always dispose ASAP
								return OpStatus::OK;
							}
						}
					}
				}
			}
		}
	}
	return OpStatus::ERR;
}

void MacGetBookmarkFileLocation(int format, OpString &oplocationStr)
{
	FSRef fsref;
	OSErr err;

	switch(format)
	{
		case HotlistModel::OperaBookmark:
			err = FSFindFolder(kUserDomain, kPreferencesFolderType, kDontCreateFolder, &fsref);
			if(err == noErr)
			{
				if(OpStatus::IsSuccess(OpFileUtils::ConvertFSRefToUniPath(&fsref, &oplocationStr)))
				{
					oplocationStr.Append(PATHSEP);
					oplocationStr.Append("Opera 6 Preferences");
					oplocationStr.Append(PATHSEP);
					oplocationStr.Append("Bookmarks");
				}
			}
			break;

		case HotlistModel::ExplorerBookmark:
			err = FSFindFolder(kUserDomain, kPreferencesFolderType, kDontCreateFolder, &fsref);
			if(err == noErr)
			{
				if(OpStatus::IsSuccess(OpFileUtils::ConvertFSRefToUniPath(&fsref, &oplocationStr)))
				{
					oplocationStr.Append(PATHSEP);
					oplocationStr.Append("Explorer");
					oplocationStr.Append(PATHSEP);
					oplocationStr.Append("Favorites.html");
				}
			}
			break;

		case HotlistModel::NetscapeBookmark:
			// Since FireFox is more popular now it will try and grab their bookmarks first then,
			// if the folder doesn't exist it will just flip back to the preferences folder
			err = FSFindFolder(kUserDomain, kApplicationSupportFolderType, kDontCreateFolder, &fsref);
			if(err == noErr)
			{
				if(OpStatus::IsSuccess(OpFileUtils::ConvertFSRefToUniPath(&fsref, &oplocationStr)))
				{
					oplocationStr.Append(PATHSEP);
					oplocationStr.Append("Firefox");
					oplocationStr.Append(PATHSEP);

					// Check if the folder exists
					OpFile	file;
					BOOL	exists = FALSE;

					file.Construct(oplocationStr);
					if(OpStatus::IsSuccess(file.Exists(exists)) && !exists)
					{
						// Could be in "Phoenix" (firebird), "Mozilla", "Netscape" or the OmniWeb folder so let's just stop here.
						err = FSFindFolder(kUserDomain, kPreferencesFolderType, kDontCreateFolder, &fsref);
						if (err == noErr)
							OpFileUtils::ConvertFSRefToUniPath(&fsref, &oplocationStr);
					}
				}
			}
			break;

		case HotlistModel::KonquerorBookmark:
			err = FSFindFolder(kUserDomain, kDomainLibraryFolderType, kDontCreateFolder, &fsref);
			if(err == noErr)
			{
				if(OpStatus::IsSuccess(OpFileUtils::ConvertFSRefToUniPath(&fsref, &oplocationStr)))
				{
					oplocationStr.Append(PATHSEP);
					oplocationStr.Append("Safari");
					oplocationStr.Append(PATHSEP);
					oplocationStr.Append("Bookmarks.plist");
				}
			}
			break;
		default:
			break;
	}
}

#ifdef HC_CAP_NEWUNISTRLIB

char *StrToLocaleEncoding(const uni_char *str)
{
	if (!str) return NULL;
	int len = uni_strlen(str);
	UTF16toUTF8Converter converter;
	int len2 = converter.BytesNeeded(str, len*sizeof(uni_char));
	char * result = new char [len2 + 1];
	int read = 0;
	len2 = converter.Convert(str, len*sizeof(uni_char), result, len2, &read);
	result[len2] = '\0';
	return result;
}

uni_char *StrFromLocaleEncoding(const char *str)
{
	if (!str) return NULL;
	int len = strlen(str) + 1;
	uni_char * result = new uni_char[len];
	UTF8toUTF16Converter converter;
	int read = 0;
	int written = converter.Convert(str, len-1, result, (len-1)*sizeof(uni_char), &read);
	result[written/sizeof(uni_char)] = '\0';
	return result;
}
#endif

/**
* Fills a buffer with random characters A - Z.
*
* @param dest The destination buffer
* @param size The number of characters to put into the buffer.
*/
void RandomCharFill(void *dest, int size)
{
	const int range = 'z' - 'a';

	char *pch = (char*) dest;
//	srand( (unsigned)time( NULL ));

	for( int i=0; i<size; i++)
	{
		pch[i] = 'a' + (rand() % range);
	}
}

BOOL SetOpString(OpString& dest, CFStringRef source)
{
	if (source)
	{
		int len = CFStringGetLength(source);

		if (dest.Reserve(len + 1))
		{
			CFStringGetCharacters(source, CFRangeMake(0,len), (UniChar*)dest.CStr());
			dest.CStr()[len] = 0;
			return TRUE;
		}
	}

	return FALSE;
}

bool GetSignatureAndVersionFromBundle(CFURLRef bundleURL, OpString *signature, OpString *short_version)
{
	// We need to grab the signature and
	CFBundleRef bundle_ref = CFBundleCreate(NULL, bundleURL);
	if(bundle_ref != NULL)
	{
		// CFBundleShortVersionString && CFBundleSignature
		CFStringRef bundleSignature		= (CFStringRef)::CFBundleGetValueForInfoDictionaryKey(bundle_ref, CFSTR("CFBundleSignature"));
		CFStringRef bundleShortVersion	= (CFStringRef)::CFBundleGetValueForInfoDictionaryKey(bundle_ref, CFSTR("CFBundleShortVersionString"));

		// Save the values into the local arrays for checking
		SetOpString(*signature, bundleSignature);
		SetOpString(*short_version, bundleShortVersion);

		CFRelease(bundle_ref);

		return true;
	}

	return false;
}

OpPoint ConvertToScreen(const OpPoint &point, OpWindow* pw, MDE_OpView* pv)
{
	OpPoint p = point;
	INT32 dx, dy;
	while(pv || pw)
	{
		if(pv)
		{
			pv->GetPos(&dx, &dy);
			pw = pv->GetParentWindow();
			pv = pv->GetParentView();
		}
		else
		{
			pw->GetInnerPos(&dx, &dy);
			BOOL native = ((CocoaOpWindow*)pw)->IsNativeWindow();
			if(!native)
			{
				pv = (MDE_OpView*)pw->GetParentView();
				pw = pw->GetParentWindow();
			}
			else
			{
				pv = NULL;
				pw = NULL;
			}
		}
		p.x += dx;
		p.y += dy;
	}
	
	return p;
}	

void SendMenuHighlightToScope(CFStringRef item_title, BOOL main_menu, BOOL is_submenu)
{
	// If scope is up and running send the information about the menu item that was just highlighted to it
	if (g_scope_manager->desktop_window_manager && g_scope_manager->desktop_window_manager->GetSystemInputPI())
	{
		OpString menu_item_title;
		// Convert to OpString
		if (SetOpString(menu_item_title, item_title))
			static_cast<MacSystemInputPI*>(g_scope_manager->desktop_window_manager->GetSystemInputPI())->MenuItemHighlighted(menu_item_title, main_menu, is_submenu);
	}
}

void SendMenuTrackingToScope(BOOL tracking)
{
	// If scope is up and running send the information if a menu is up
	if (g_scope_manager->desktop_window_manager && g_scope_manager->desktop_window_manager->GetSystemInputPI())
	{
		static_cast<MacSystemInputPI*>(g_scope_manager->desktop_window_manager->GetSystemInputPI())->MenuTracking(tracking);
	}
}

void SendSetActiveNSMenu(void *nsmenu)
{
	// If scope is up and running send the information about which is the currently active menu
	if (g_scope_manager->desktop_window_manager && g_scope_manager->desktop_window_manager->GetSystemInputPI())
	{
		static_cast<MacSystemInputPI*>(g_scope_manager->desktop_window_manager->GetSystemInputPI())->SetActiveNSMenu(nsmenu);
	}
}

void *GetActiveMenu()
{
	void *active_menu = NULL;
	
	// Need to loop the menu and list all the items in it
	if (g_scope_manager->desktop_window_manager && g_scope_manager->desktop_window_manager->GetSystemInputPI())
		active_menu = static_cast<MacSystemInputPI*>(g_scope_manager->desktop_window_manager->GetSystemInputPI())->GetActiveNSMenu();
	
	return active_menu;
}

void SendSetActiveNSPopUpButton(void *nspopupbutton)
{
	// If scope is up and running send the information about which is the currently active menu
	if (g_scope_manager->desktop_window_manager && g_scope_manager->desktop_window_manager->GetSystemInputPI())
	{
		static_cast<MacSystemInputPI*>(g_scope_manager->desktop_window_manager->GetSystemInputPI())->SetActiveNSPopUpButton(nspopupbutton);
	}
}

void PressScopeKey(OpKey::Code key, ShiftKeyState modifier)
{
	if (g_scope_manager->desktop_window_manager && g_scope_manager->desktop_window_manager->GetSystemInputPI())
	{
		g_scope_manager->desktop_window_manager->GetSystemInputPI()->PressKey(0, key, modifier);
	}
}
