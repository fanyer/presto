#include <Carbon.h>
#include "EmBrowser.h"
#include "MachOCFMGlue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PathConverter.h"

/**
* We have to be very careful with EmBrowser API methods that take callbacks as arguments!
* All callbacks have to be translated or we will crash when Opera tries to call the callback.
*/
typedef struct CallbackMethods
{
	EmBrowserJSEvaluateCodeProc			jsEvaluateCode;				// 1
	EmBrowserJSGetPropertyProc			jsGetProperty;				// 2
	EmBrowserJSSetPropertyProc			jsSetProperty;				// 3
	EmBrowserJSCallMethodProc			jsCallMethod;				// 4
	EmBrowserJSCreateMethodProc			jsCreateMethod;				// 5
	EmBrowserThumbnailRequestProc       thumbnailRequest;			// 6
	EmBrowserSetUILanguage				setUILanguage;				// 7
	EmBrowserSetUserStylesheetProc		setUserStylesheet;			// 8
	long filler[16 - 8];
} CallbackMethods;

void InitLibraryGlue();
void ShutdownLibraryGlue();

EmBrowserInitParameterBlock *gInitParamBlock;
EmBrowserAppNotification *gAppNotification;
EmBrowserMethods *gMethods;
CallbackMethods* gCallbackMethods;

EmBrowserInitLibraryProc gInitLibraryProc;
CFBundleRef gLibraryBundle = 0;
AliasHandle gAlias;
FSSpec gGlueFSSpec;

int main(int argc, char **argv)
{
	// Do nothing
	return emBrowserNoErr;
}

EmJavascriptStatus JSEvaluateCode(EmDocumentRef doc, EmBrowserString code, EmBrowserJSResultProc callback, void *clientData)
{
	if(gCallbackMethods->jsEvaluateCode)
		return gCallbackMethods->jsEvaluateCode(doc, code, (EmBrowserJSResultProc)(callback ? MachOFunctionPointerForCFMFunctionPointer(callback) : NULL), clientData);

	return emBrowserGenericErr;
}

EmJavascriptStatus JSGetProperty(EmDocumentRef doc, EmJavascriptObjectRef object, EmBrowserString propertyName,
                                 EmBrowserJSResultProc callback, void *clientData)
{
	if(gCallbackMethods->jsGetProperty)
		return gCallbackMethods->jsGetProperty(doc, object, propertyName, (EmBrowserJSResultProc)(callback ? MachOFunctionPointerForCFMFunctionPointer(callback) : NULL), clientData);

	return emBrowserGenericErr;
}

EmJavascriptStatus JSSetProperty(EmDocumentRef doc, EmJavascriptObjectRef object, EmBrowserString propertyName, EmJavascriptValue *value,
                                 EmBrowserJSResultProc callback, void *clientData)
{
	if(gCallbackMethods->jsSetProperty)
		return gCallbackMethods->jsSetProperty(doc, object, propertyName, value, (EmBrowserJSResultProc)(callback ? MachOFunctionPointerForCFMFunctionPointer(callback) : NULL), clientData);

	return emBrowserGenericErr;
}

EmJavascriptStatus JSCallMethod(EmDocumentRef doc, EmJavascriptObjectRef object, EmBrowserString methodName,
                                unsigned argumentsCount, EmJavascriptValue *argumentsArray,
                                EmBrowserJSResultProc callback, void *clientData)
{
	if(gCallbackMethods->jsCallMethod)
		return gCallbackMethods->jsCallMethod(doc, object, methodName, argumentsCount, argumentsArray, (EmBrowserJSResultProc)(callback ? MachOFunctionPointerForCFMFunctionPointer(callback) : NULL), clientData);

	return emBrowserGenericErr;
}

EmJavascriptStatus JSCreateMethod(EmDocumentRef doc, EmJavascriptObjectRef *method, EmBrowserJSMethodCallProc callback, void *callbackClientData)
{
	if(gCallbackMethods->jsCreateMethod)
		return gCallbackMethods->jsCreateMethod(doc, method, (EmBrowserJSMethodCallProc)(callback ? MachOFunctionPointerForCFMFunctionPointer(callback) : NULL), callbackClientData);

	return emBrowserGenericErr;
}

EmThumbnailStatus ThumbnailRequest(EmBrowserString inUrl, long inWidth, long inHeight, long inScale, EmBrowserThumbnailResultProc callback, EmThumbnailRef *outRef)
{
	if(gCallbackMethods->thumbnailRequest)
		return gCallbackMethods->thumbnailRequest(inUrl, inWidth, inHeight, inScale, (EmBrowserThumbnailResultProc)(callback ? MachOFunctionPointerForCFMFunctionPointer(callback) : NULL), outRef);

	return emThumbnailGenericErr;
}

EmBrowserStatus SetUILanguage(IN const EmBrowserString languageFile)
{
	EmBrowserStatus result = emBrowserGenericErr;

	if(gCallbackMethods->setUILanguage)
	{
		EmBrowserString ptr = 0;
		uni_char* cpy = 0;

		if(languageFile)
		{
			const uni_char* volumes = UNI_L("/Volumes/");
			const size_t volumesLen = 9;
			size_t len = uni_strlen((uni_char*)languageFile);
			cpy = new uni_char[1 + len + volumesLen];

			if(cpy)
			{
				uni_char* uptr = cpy;

				memcpy(cpy, (uni_char*)languageFile, len*sizeof(uni_char));
				cpy[len] = 0;

				while(uptr && *uptr)
				{
					if(*uptr == ':')
					{
						*uptr = '/';
					}

					uptr++;
				}

				uptr = uni_strchr(cpy, UNI_L('/'));
				if(uptr)
				{
					if(uni_strncmp(cpy, GetBootVolumeName(), uptr - cpy) == 0)
					{
						ptr = (EmBrowserString) uptr;
					}
					else
					{
						memmove(cpy + volumesLen, cpy, (len+1)*sizeof(uni_char));
						memcpy(cpy, volumes, volumesLen*sizeof(uni_char));

						ptr = (EmBrowserString) cpy;
					}
				}
			}
		}

		result = gCallbackMethods->setUILanguage(ptr);

		if(cpy)
		{
			delete[] cpy;
		}
	}

	return result;
}

EmBrowserStatus SetBrowserStylesheet(IN const EmBrowserString stylesheetFilePath)
{
	EmBrowserStatus result = emBrowserGenericErr;

	if(gCallbackMethods->setUserStylesheet)
	{
		EmBrowserString ptr = 0;
		uni_char* cpy = 0;

		if(stylesheetFilePath)
		{
			const uni_char* volumes = UNI_L("/Volumes/");
			const size_t volumesLen = 9;
			size_t len = uni_strlen((uni_char*)stylesheetFilePath);
			cpy = new uni_char[1 + len + volumesLen];

			if(cpy)
			{
				uni_char* uptr = cpy;

				memcpy(cpy, (uni_char*)stylesheetFilePath, len*sizeof(uni_char));
				cpy[len] = 0;

				while(uptr && *uptr)
				{
					if(*uptr == ':')
					{
						*uptr = '/';
					}

					uptr++;
				}

				uptr = uni_strchr(cpy, UNI_L('/'));
				if(uptr)
				{
					if(uni_strncmp(cpy, GetBootVolumeName(), uptr - cpy) == 0)
					{
						ptr = (EmBrowserString) uptr;
					}
					else
					{
						memmove(cpy + volumesLen, cpy, (len+1)*sizeof(uni_char));
						memcpy(cpy, volumes, volumesLen*sizeof(uni_char));

						ptr = (EmBrowserString) cpy;
					}
				}
			}
		}

		result = gCallbackMethods->setUserStylesheet(ptr);

		if(cpy)
		{
			delete[] cpy;
		}
	}

	return result;
}

#ifdef __cplusplus
extern "C" {
#endif

#pragma export on

EmBrowserStatus WidgetInitLibrary(EmBrowserInitParameterBlock *initPB)
{
	InitLibraryGlue();

	if(!gInitLibraryProc)
	{
		return emBrowserGenericErr;
	}

	gInitParamBlock = initPB;

	gCallbackMethods = (CallbackMethods *)malloc(sizeof(CallbackMethods));
	if(gCallbackMethods)
	{
		memset(gCallbackMethods, 0, sizeof(CallbackMethods));
	}


	// IN translation (CFM -> MachO)
	if(initPB->malloc)
		initPB->malloc = (AppMallocProc)MachOFunctionPointerForCFMFunctionPointer(initPB->malloc);
	if(initPB->realloc)
		initPB->realloc = (AppReallocProc)MachOFunctionPointerForCFMFunctionPointer(initPB->realloc);
	if(initPB->free)
		initPB->free = (AppFreeProc)MachOFunctionPointerForCFMFunctionPointer(initPB->free);


	if(initPB->notifyCallbacks)
	{
		gAppNotification = initPB->notifyCallbacks;
		void** appNotificationMethodList = (void**)gAppNotification;
		for (int i = 2; i < (sizeof(EmBrowserAppNotification) / sizeof(void*)); i++)
			if (appNotificationMethodList[i])
				appNotificationMethodList[i] = MachOFunctionPointerForCFMFunctionPointer(appNotificationMethodList[i]);

		/*
		if(gAppNotification->notification)
			gAppNotification->notification = (EmBrowserAppSimpleNotificationProc)MachOFunctionPointerForCFMFunctionPointer(gAppNotification->notification);
		if(gAppNotification->beforeNavigation)
			gAppNotification->beforeNavigation = (EmBrowserAppBeforeNavigationProc)MachOFunctionPointerForCFMFunctionPointer(gAppNotification->beforeNavigation);
		if(gAppNotification->handleProtocol)
			gAppNotification->handleProtocol = (EmBrowserAppHandleUnknownProtocolProc)MachOFunctionPointerForCFMFunctionPointer(gAppNotification->handleProtocol);
		if(gAppNotification->newBrowser)
			gAppNotification->newBrowser = (EmBrowserAppRequestNewBrowserProc)MachOFunctionPointerForCFMFunctionPointer(gAppNotification->newBrowser);
		if(gAppNotification->posChange)
			gAppNotification->posChange = (EmBrowserAppRequestSetPositionProc)MachOFunctionPointerForCFMFunctionPointer(gAppNotification->posChange);
		if(gAppNotification->sizeChange)
			gAppNotification->sizeChange = (EmBrowserAppRequestSetSizeProc)MachOFunctionPointerForCFMFunctionPointer(gAppNotification->sizeChange);
		*/
	}

	// Call the real Opera7 WidgetInitProc to get the browserMethods
	gInitLibraryProc(initPB);

	// OUT translation (MachO -> CFM)
	if(initPB->createInstance)
		initPB->createInstance = (EmBrowserCreateProc)CFMFunctionPointerForMachOFunctionPointer(initPB->createInstance);
	if(initPB->destroyInstance)
		initPB->destroyInstance = (EmBrowserDestroyProc)CFMFunctionPointerForMachOFunctionPointer(initPB->destroyInstance);
	if(initPB->shutdown)
		initPB->shutdown = (EmBrowserShutdown)CFMFunctionPointerForMachOFunctionPointer(initPB->shutdown);

	if(initPB->browserMethods)
	{
		gMethods = initPB->browserMethods;
		void** methodList = (void**)gMethods;
		for (int i = 2; i < (sizeof(EmBrowserMethods) / sizeof(void*)); i++)
			if (methodList[i])
				methodList[i] = CFMFunctionPointerForMachOFunctionPointer(methodList[i]);

		if(gCallbackMethods)
		{
			gCallbackMethods->jsEvaluateCode = gMethods->jsEvaluateCode;
			gMethods->jsEvaluateCode = JSEvaluateCode;

			gCallbackMethods->jsGetProperty = gMethods->jsGetProperty;
			gMethods->jsGetProperty = JSGetProperty;

			gCallbackMethods->jsSetProperty = gMethods->jsSetProperty;
			gMethods->jsSetProperty = JSSetProperty;

			gCallbackMethods->jsCallMethod = gMethods->jsCallMethod;
			gMethods->jsCallMethod = JSCallMethod;

			gCallbackMethods->jsCreateMethod = gMethods->jsCreateMethod;
			gMethods->jsCreateMethod = JSCreateMethod;

			gCallbackMethods->thumbnailRequest = gMethods->thumbnailRequest;
			gMethods->thumbnailRequest = ThumbnailRequest;

			gCallbackMethods->setUILanguage = gMethods->setUILanguage;
			gMethods->setUILanguage = SetUILanguage;

			gCallbackMethods->setUserStylesheet = gMethods->setUserStylesheet;
			gMethods->setUserStylesheet = SetBrowserStylesheet;
		}

		/*
		if(gMethods->setLocation)
			gMethods->setLocation = (EmBrowserSetPortLocationProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->setLocation);
		if(gMethods->getLocation)
			gMethods->getLocation = (EmBrowserGetPortLocationProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->getLocation);
		if(gMethods->draw)
			gMethods->draw = (EmBrowserDrawProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->draw);
		if(gMethods->scrollToSelection)
			gMethods->scrollToSelection = (EmBrowserScrollToSelectionProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->scrollToSelection);
		if(gMethods->pageBusy)
			gMethods->pageBusy = (EmBrowserGetPageBusyProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->pageBusy);
		if(gMethods->downloadProgressNums)
			gMethods->downloadProgressNums = (EmBrowserGetDLProgressNumsProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->downloadProgressNums);
		if(gMethods->getText)
			gMethods->getText = (EmBrowserGetTextProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->getText);
		if(gMethods->getSource)
			gMethods->getSource = (EmBrowserGetSourceProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->getSource);
		if(gMethods->getSourceSize)
			gMethods->getSourceSize = (EmBrowserGetSourceSizeProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->getSourceSize);
		if(gMethods->canHandleMessage)
			gMethods->canHandleMessage = (EmBrowserCanHandleMessageProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->canHandleMessage);
		if(gMethods->handleMessage)
			gMethods->handleMessage = (EmBrowserDoHandleMessageProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->handleMessage);
		if(gMethods->openURL)
			gMethods->openURL = (EmBrowserOpenURLProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->openURL);
		if(gMethods->openURLInDoc)
			gMethods->openURLInDoc = (EmBrowserOpenURLinDocProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->openURLInDoc);
		if(gMethods->setFocus)
			gMethods->setFocus = (EmBrowserSetFocusProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->setFocus);
		if(gMethods->setVisible)
			gMethods->setVisible = (EmBrowserSetVisibilityProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->setVisible);
		if(gMethods->getVisible)
			gMethods->getVisible = (EmBrowserGetVisibilityProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->getVisible);
		if(gMethods->setActive)
			gMethods->setActive = (EmBrowserSetActiveProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->setActive);
		if(gMethods->getSecurityDesc)
			gMethods->getSecurityDesc = (EmBrowserGetSecurityDescProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->getSecurityDesc);
		if(gMethods->getRootDoc)
			gMethods->getRootDoc = (EmBrowserGetRootDocProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->getRootDoc);
		if(gMethods->getURL)
			gMethods->getURL = (EmBrowserGetURLProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->getURL);
		if(gMethods->getNumberOfSubDocuments)
			gMethods->getNumberOfSubDocuments = (EmBrowserGetNumberOfSubDocumentsProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->getNumberOfSubDocuments);
		if(gMethods->getParentDocument)
			gMethods->getParentDocument = (EmBrowserGetParentDocumentProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->getParentDocument);
		if(gMethods->getBrowser)
			gMethods->getBrowser = (EmBrowserGetBrowserProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->getBrowser);
		if(gMethods->getSubDocuments)
			gMethods->getSubDocuments = (EmBrowserGetSubDocumentsProc)CFMFunctionPointerForMachOFunctionPointer(gMethods->getSubDocuments);
*/
	}

	return emBrowserNoErr;
}

pascal OSErr __glue_initialize(const CFragInitBlock *initBlock)
{
	// Save the FSSpec so we know where we are %-)
	memcpy(&gGlueFSSpec, initBlock->fragLocator.u.onDisk.fileSpec, sizeof(gGlueFSSpec));

	return(noErr);
}

pascal void __glue_terminate(void)
{
	ShutdownLibraryGlue();
}

#pragma export off

#ifdef __cplusplus
}
#endif

void ShutdownLibraryGlue()
{
	// free library glue code
	if(gLibraryBundle)
	{
		CFBundleUnloadExecutable(gLibraryBundle);
		CFRelease(gLibraryBundle);
	}

	// IN free
	if(gInitParamBlock)
	{
		if(gInitParamBlock->malloc)
			DisposeMachOFunctionPointer(gInitParamBlock->malloc);
		if(gInitParamBlock->realloc)
			DisposeMachOFunctionPointer(gInitParamBlock->realloc);
		if(gInitParamBlock->free)
			DisposeMachOFunctionPointer(gInitParamBlock->free);
	}

	if(gAppNotification)
	{
		if(gAppNotification->notification)
			DisposeMachOFunctionPointer(gAppNotification->notification);
		if(gAppNotification->beforeNavigation)
			DisposeMachOFunctionPointer(gAppNotification->beforeNavigation);
		if(gAppNotification->handleProtocol)
			DisposeMachOFunctionPointer(gAppNotification->handleProtocol);
		if(gAppNotification->newBrowser)
			DisposeMachOFunctionPointer(gAppNotification->newBrowser);
		if(gAppNotification->posChange)
			DisposeMachOFunctionPointer(gAppNotification->posChange);
		if(gAppNotification->sizeChange)
			DisposeMachOFunctionPointer(gAppNotification->sizeChange);
	}

	// OUT free
	if(gInitParamBlock)
	{
		if(gInitParamBlock->createInstance)
			DisposeCFMFunctionPointer(gInitParamBlock->createInstance);
		if(gInitParamBlock->destroyInstance)
			DisposeCFMFunctionPointer(gInitParamBlock->destroyInstance);
		if(gInitParamBlock->shutdown)
			DisposeCFMFunctionPointer(gInitParamBlock->shutdown);
	}

	if(gCallbackMethods)
	{
		free(gCallbackMethods);
	}

#if 0
	// Deleted from widget already when we get here
	if(gMethods)
	{
		if(gMethods->setLocation)
			DisposeCFMFunctionPointer(gMethods->setLocation);
		if(gMethods->getLocation)
			DisposeCFMFunctionPointer(gMethods->getLocation);
		if(gMethods->draw)
			DisposeCFMFunctionPointer(gMethods->draw);
		if(gMethods->scrollToSelection)
			DisposeCFMFunctionPointer(gMethods->scrollToSelection);
		if(gMethods->pageBusy)
			DisposeCFMFunctionPointer(gMethods->pageBusy);
		if(gMethods->downloadProgressNums)
			DisposeCFMFunctionPointer(gMethods->downloadProgressNums);
		if(gMethods->getText)
			DisposeCFMFunctionPointer(gMethods->getText);
		if(gMethods->getSource)
			DisposeCFMFunctionPointer(gMethods->getSource);
		if(gMethods->getSourceSize)
			DisposeCFMFunctionPointer(gMethods->getSourceSize);
		if(gMethods->canHandleMessage)
			DisposeCFMFunctionPointer(gMethods->canHandleMessage);
		if(gMethods->handleMessage)
			DisposeCFMFunctionPointer(gMethods->handleMessage);
		if(gMethods->openURL)
			DisposeCFMFunctionPointer(gMethods->openURL);
		if(gMethods->openURLInDoc)
			DisposeCFMFunctionPointer(gMethods->openURLInDoc);
		if(gMethods->setFocus)
			DisposeCFMFunctionPointer(gMethods->setFocus);
		if(gMethods->setVisible)
			DisposeCFMFunctionPointer(gMethods->setVisible);
		if(gMethods->getVisible)
			DisposeCFMFunctionPointer(gMethods->getVisible);
		if(gMethods->setActive)
			DisposeCFMFunctionPointer(gMethods->setActive);
		if(gMethods->getSecurityDesc)
			DisposeCFMFunctionPointer(gMethods->getSecurityDesc);
		if(gMethods->getRootDoc)
			DisposeCFMFunctionPointer(gMethods->getRootDoc);
		if(gMethods->getURL)
			DisposeCFMFunctionPointer(gMethods->getURL);
		if(gMethods->getNumberOfSubDocuments)
			DisposeCFMFunctionPointer(gMethods->getNumberOfSubDocuments);
		if(gMethods->getParentDocument)
			DisposeCFMFunctionPointer(gMethods->getParentDocument);
		if(gMethods->getBrowser)
			DisposeCFMFunctionPointer(gMethods->getBrowser);
		if(gMethods->getSubDocuments)
			DisposeCFMFunctionPointer(gMethods->getSubDocuments);
	}
#endif
}

void InitLibraryGlue(void)
{
	OSStatus retVal;
	Boolean loaded = false;
	CFBundleRef bundleLibrary;
	CFURLRef frameworkURL;
	CFURLRef operaFrameworkURL;
	SInt16 vRefNum;
	SInt32 dirID;
	FSRef widgetLocation;

	retVal = FSpMakeFSRef(&gGlueFSSpec, &widgetLocation);
	if(retVal == noErr)
	{
		frameworkURL = CFURLCreateFromFSRef(kCFAllocatorSystemDefault, &widgetLocation);
		if(frameworkURL)
		{
			operaFrameworkURL = CFURLCreateCopyDeletingLastPathComponent(kCFAllocatorSystemDefault, frameworkURL);

			if(operaFrameworkURL)
			{
				CFRelease(frameworkURL);

				frameworkURL = CFURLCreateCopyDeletingLastPathComponent(kCFAllocatorSystemDefault, operaFrameworkURL);

				if(frameworkURL)
				{
					CFRelease(operaFrameworkURL);

					operaFrameworkURL = CFURLCreateCopyAppendingPathComponent(kCFAllocatorSystemDefault, frameworkURL,
																CFSTR("Frameworks/Opera.framework"), false);

					if(operaFrameworkURL)
					{
						bundleLibrary = CFBundleCreate(kCFAllocatorSystemDefault, operaFrameworkURL);
						if(bundleLibrary)
						{
							if (CFBundleLoadExecutable(bundleLibrary))
							{
								gInitLibraryProc = (EmBrowserInitLibraryProc)CFBundleGetFunctionPointerForName(bundleLibrary, CFSTR("WidgetInitLibrary"));
								if(gInitLibraryProc)
								{
									// Success!
									loaded = true;
								}
							}
							else
							{
								loaded = false;
								CFRelease(bundleLibrary);
							}
						}
						CFRelease(operaFrameworkURL);
					}
				}
			}
		}
	}

	if(loaded)
	{
		gLibraryBundle = bundleLibrary;
	}
}
