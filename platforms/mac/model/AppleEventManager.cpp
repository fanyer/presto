/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/model/AppleEventManager.h"
#include "platforms/mac/model/CocoaAppleEventHandler.h"
#include "platforms/mac/util/AppleEventUtils.h"
#include "platforms/mac/File/FileUtils_Mac.h"
#include "platforms/mac/util/CTextConverter.h"
#include "platforms/mac/util/PathConverter.h"

#ifdef SUPPORT_SHARED_MENUS
#include "platforms/mac/model/menusharing.h"
#endif

#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/desktop_util/actions/delayed_action.h"
#include "adjunct/quick/Application.h"

#include "modules/prefs/prefsmanager/collections/pc_doc.h"

#ifdef WIDGET_RUNTIME_SUPPORT
#include "adjunct/quick/application/ApplicationAdapter.h"
#include "platforms/mac/util/CocoaPlatformUtils.h"
extern OpString g_widgetPath;
#endif 
OpINT32Vector gInformApps;

class DelayedOpenURL : OpDelayedAction
{
public:
	DelayedOpenURL(const uni_char* url, BOOL3 new_window, BOOL3 new_page, DesktopWindow* win)
	: OpDelayedAction(1000)
	, new_window(new_window)
	, new_page(new_page)
	, win(win)
	{
		m_url.Set(url);
	}
	virtual void DoAction()
	{
		if (!g_application->IsBrowserStarted())
		{
			(void) new DelayedOpenURL(m_url.CStr(), new_window, new_page, win);
			return;
		}
		if (win)
			g_application->GoToPage(m_url, FALSE, TRUE, FALSE, win);
		else
			g_application->OpenURL(m_url, new_window, new_page, MAYBE, 0, FALSE, TRUE);
	}
private:
	BOOL3 new_window;
	BOOL3 new_page;
	DesktopWindow* win;
	OpString m_url;
};

AppleEventManager::AppleEventManager()
{
}

AppleEventManager::~AppleEventManager()
{
	gInformApps.Clear();
}

#pragma mark -

pascal OSErr AppleEventManager::Make(const AppleEvent *msgin, AppleEvent *reply, long inRef)
{
	OSErr err = noErr;
	Boolean handled = false;

	DescType data_desc;
	MacSize data_size;
	DescType create_type;
	DocumentDesktopWindow * new_page = NULL;
	DesktopWindow* new_win = NULL;

	DesktopWindow* parent_win = NULL;
	AEDesc parent_desc;
	err = AEGetParamDesc(msgin, keyAEInsertHere, typeObjectSpecifier, &parent_desc);
	if (err == noErr)
	{
		parent_win = GetWindowFromAEDesc(&parent_desc);
		AEDisposeDesc(&parent_desc);
		if (!parent_win || !parent_win->GetWorkspace())
			return paramErr;
	}

	err = AEGetParamPtr(msgin, keyAEObjectClass, cType, &data_desc, &create_type, sizeof(create_type), &data_size);
	if ((noErr == err) && (create_type == cWindow))
	{
		new_win = g_application->GetBrowserDesktopWindow(TRUE, FALSE, TRUE);
	}
	else if ((noErr == err) && (create_type == cDocument))
	{
		if (parent_win && parent_win->GetWorkspace())
			new_win = g_application->CreateDocumentDesktopWindow(parent_win->GetWorkspace());
		else
		{
			g_application->GetBrowserDesktopWindow(FALSE, FALSE, TRUE, &new_page);
			new_win = new_page;
		}
	}

	if (new_win)
	{
		AEDesc reply_desc;
		AECreateDesc(typeNull, NULL, 0, &reply_desc);
		err = CreateAEDescFromID(GetExportIDForDesktopWindow(new_win, (create_type == cWindow)), create_type, &reply_desc);
		if (err == noErr)
		{
			handled = true;
			AEPutParamDesc(reply, keyAEResult, &reply_desc);
			AEDisposeDesc(&reply_desc);
		}
	}
#ifdef _MAC_DEBUG
	ObjectTraverse(msgin);
#endif
	if ((err == noErr) && !handled)
	{
		err = errAEEventNotHandled;
	}
	return err;
}

pascal OSErr AppleEventManager::Close(const AppleEvent *msgin, AppleEvent *reply, long inRef)
{
	AEDesc		aDesc;
	OSErr err = noErr;
	Boolean handled = false;

	err = AEGetParamDesc(msgin, keyDirectObject, typeObjectSpecifier, &aDesc);
	if (noErr == err)
	{
		DescType data_desc;
		MacSize data_size;
		DescType close_type;
		long get_index;

		err = AEGetKeyPtr(&aDesc, keyAEDesiredClass, cType, &data_desc, &close_type, sizeof(close_type), &data_size);
		if (noErr == err && (close_type == cWindow || close_type == cDocument))
		{
			DesktopWindow* win = GetWindowFromAEDesc(&aDesc);
			if (win)
			{
				win->Close(TRUE);
				handled = true;
			}
			else
			{
				err = AEGetKeyPtr(&aDesc, keyAEKeyData, typeAbsoluteOrdinal, &data_desc, &get_index, sizeof(get_index), &data_size);
				if (err == noErr && get_index == kAEAll)
				{
					CloseEveryWindow();
					handled = true;
				}
				else
				{
					err = paramErr;
				}
			}
		}
		AEDisposeDesc(&aDesc);
	}
#ifdef _MAC_DEBUG
	ObjectTraverse(msgin);
#endif
	if ((err == noErr) && !handled)
	{
		err = errAEEventNotHandled;
	}
	return err;
}

pascal OSErr AppleEventManager::GetData(const AppleEvent *msgin, AppleEvent *reply, long inRef)
{
	AEDesc		aDesc;
	OSErr err = noErr;
	Boolean handled = false;

	if (!reply || reply->descriptorType == typeNull)
	{
		return errAEEventNotHandled;
	}

	err = AEGetParamDesc(msgin, keyDirectObject, typeObjectSpecifier, &aDesc);
	if (noErr == err)
	{
		AEDesc reply_desc;
		DescType data_desc;
		MacSize data_size;
		DescType get_type;
		long get_index;
		AECreateDesc(typeNull, NULL, 0, &reply_desc); // prepare an empty reply

		// Find out what object the sender desired
		err = AEGetKeyPtr(&aDesc, keyAEDesiredClass, cType, &data_desc, &get_type, sizeof(get_type), &data_size);
		if (noErr == err)
		{
			if (get_type == cWindow || get_type == cDocument)
			{
				/* "get windows"
						-> 'form', 'enum', 4, 'indx' (keyAEKeyForm, cEnumeration, 4, formAbsolutePosition)
						-> 'want', 'type', 4, 'cwin' (keyAEDesiredClass, cType, 4, cWindow)
						-> 'seld', 'abso', 4, 'all ' (keyAEKeyData, typeAbsoluteOrdinal, 4, kAEAll)
						-> 'from', 'null', 0,  NULL  (keyAEContainer, typeNull, 0, NULL)
				*/
				/* "get some window"
						-> 'form', 'enum', 4, 'indx' (keyAEKeyForm, cEnumeration, 4, formAbsolutePosition)
						-> 'want', 'type', 4, 'cwin' (keyAEDesiredClass, cType, 4, cWindow)
						-> 'seld', 'abso', 4, 'any ' (keyAEKeyData, typeAbsoluteOrdinal, 4, kAEAny)
						-> 'from', 'null', 0,  NULL  (keyAEContainer, typeNull, 0, NULL)
				*/
				/* "get window n"
						-> 'form', 'enum', 4, 'indx' (keyAEKeyForm, cEnumeration, 4, formAbsolutePosition)
						-> 'want', 'type', 4, 'cwin' (keyAEDesiredClass, cType, 4, cWindow)
						-> 'seld', 'long', 4,     n  (keyAEKeyData, cLongInteger, 4, n)
						-> 'from', 'null', 0,  NULL  (keyAEContainer, typeNull, 0, NULL)
				*/
				/* "get window \"Foo\" "
						-> 'form', 'enum', 4, 'name' (keyAEKeyForm, cEnumeration, 4, formName)
						-> 'want', 'type', 4, 'cwin' (keyAEDesiredClass, cType, 4, cWindow)
						-> 'seld', 'TEXT', 3, "Foo"  (keyAEKeyData, typeChar, 3, n)
						-> 'from', 'null', 0,  NULL  (keyAEContainer, typeNull, 0, NULL)
				*/
				err = AESizeOfKeyDesc(&aDesc, keyAEKeyData, &data_desc, &data_size);
				if (err == noErr)
				{
					Boolean toplevel = (get_type == cWindow);

					AEDesc container_desc;
					BrowserDesktopWindow* container_win = NULL;
					err = AEGetKeyDesc(&aDesc, keyAEContainer, typeObjectSpecifier, &container_desc);
					if (err == noErr)
					{
						// "get first document in last window"
						container_win = (BrowserDesktopWindow*)GetWindowFromAEDesc(&container_desc);
						if (container_win && container_win->GetType() != OpTypedObject::WINDOW_TYPE_BROWSER)
						{
							container_win = NULL;
							err = paramErr;	// bad window, don't pretend we did not get a window, but report error.
						}
						AEDisposeDesc(&container_desc);
					}
					else
						err = noErr;

					if (err == noErr)
					{
						if ((data_desc == cLongInteger) && (data_size == 4))
						{
							err = AEGetKeyPtr(&aDesc, keyAEKeyData, cLongInteger, &data_desc, &get_index, sizeof(get_index), &data_size);
							if (err == noErr)
								err = CreateDescForNumberedWindow(get_index, toplevel, container_win, &reply_desc);
						}
						else if ((data_desc == typeAbsoluteOrdinal) && (data_size == 4))
						{
							err = AEGetKeyPtr(&aDesc, keyAEKeyData, typeAbsoluteOrdinal, &data_desc, &get_index, sizeof(get_index), &data_size);
							if (err == noErr)
								err = CreateDescForOrdinalWindow(get_index, toplevel, container_win, &reply_desc);
						}
						else if (data_desc == typeChar || data_desc == typeUnicodeText)
						{
							uni_char* uniName = GetNewStringFromObjectAndKey(&aDesc, keyAEKeyData);
							if (uniName) {
								err = CreateDescForNamedWindow(uniName, toplevel, container_win, &reply_desc);
								delete [] uniName;
							}
						}
						else
						{
							err = paramErr;
						}
					}
				}
				if (err == noErr)
				{
					handled = true;
					AEPutParamDesc(reply, keyAEResult, &reply_desc);
					AEDisposeDesc(&reply_desc);
				}
			}
			else if (get_type == formPropertyID)
			{
				/* "get [something] of [parent]"
						-> 'want', 'type', 4, 'prop'     (keyAEDesiredClass, cType, 4, formPropertyID)
						-> 'seld', 'type', 4, [something](keyAEKeyData, cType, 4, something)
						-> 'from', 'obj ', 4, [parent]   (keyAEContainer, typeObjectSpecifier, 4, parent)
				*/
				DescType get_prop;
				err = AEGetKeyPtr(&aDesc, keyAEKeyData, cType, &data_desc, &get_prop, sizeof(get_prop), &data_size);
				if (err == noErr)
				{
					AEDesc get_from;
					err = AEGetKeyDesc(&aDesc, keyAEContainer, typeObjectSpecifier, &get_from);
					if (err == noErr)
					{
						DesktopWindow *win = GetWindowFromAEDesc(&get_from);
						if (win)
						{
							DesktopWindow *result_win = NULL;
							const uni_char* title = NULL;
							handled = true;
							Boolean value;

							OP_ASSERT(!"Implement support for window properties");

							switch (get_prop)
							{
								case pName:	// itxt, name
									title = win->GetTitle();
									if (!title)
										title = UNI_L("");
									AEPutParamPtr(reply, keyAEResult, typeUnicodeText, title, uni_strlen(title) * sizeof(uni_char));
									break;
								case 'url ':	// itxt, url
									if (win->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
									{
										win = ((BrowserDesktopWindow*)win)->GetActiveDocumentDesktopWindow();
									}
									if (win && win->GetType() == OpTypedObject::WINDOW_TYPE_DOCUMENT)
									{
										OpString url;
										((DocumentDesktopWindow*)win)->GetURL(url);
										title = url.CStr();
										if (!title)
											title = UNI_L("");
										AEPutParamPtr(reply, keyAEResult, typeUnicodeText, title, uni_strlen(title) * sizeof(uni_char));
									}
									else
									{
										handled = false;
									}
									break;

								case pHasTitleBar:	// bool, titled
									{
										title = win->GetTitle();
										if (!title)
											title = UNI_L("");
										value = (uni_strlen(title) > 0);
										err = AEPutParamPtr(reply, keyAEResult, typeBoolean, &value, sizeof(value));
									}
									break;
								case pIndex:	// long, index
									{
										int index = GetTopLevelWindowNumber(win);
										if (index > 0)
											err = AEPutParamPtr(reply, keyAEResult, typeSInt32, &index, sizeof(index));
										else
											err = paramErr;
									}
									break;

								case pID:	// long, id
									{
										int ident = GetExportIDForDesktopWindow(win, true);
										err = AEPutParamPtr(reply, keyAEResult, typeSInt32, &ident, sizeof(ident));
									}
									break;
								case 'LOad':	// bool, loading
									if (win->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
									{
										win = ((BrowserDesktopWindow*)win)->GetActiveDocumentDesktopWindow();
									}
									if (win && win->GetType() == OpTypedObject::WINDOW_TYPE_DOCUMENT)
									{
										value = ((DocumentDesktopWindow*)win)->IsLoading();
										err = AEPutParamPtr(reply, keyAEResult, typeBoolean, &value, sizeof(value));
									}
									else
										handled = false;
									break;
								case cWindow:	// bool, window
									{
										result_win = win->GetParentDesktopWindow();
										if (!result_win)
											result_win = win;
										CreateDescForWindow(result_win, true, &reply_desc);
										AEPutParamDesc(reply, keyAEResult, &reply_desc);
										AEDisposeDesc(&reply_desc);
									}
									break;
								case cDocument:	// bool, document
									{
										if (win->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
											result_win = ((BrowserDesktopWindow*)win)->GetActiveDocumentDesktopWindow();
										else
											result_win = win;
										if (result_win && result_win->GetType() == OpTypedObject::WINDOW_TYPE_DOCUMENT)
										{
											CreateDescForWindow(result_win, false, &reply_desc);
											AEPutParamDesc(reply, keyAEResult, &reply_desc);
											AEDisposeDesc(&reply_desc);
										}
									}
									break;
								default:
									handled = false;
									break;
							}
						}
						AEDisposeDesc(&get_from);
					}
				}
			}
		}
		AEDisposeDesc(&aDesc);
	}
#ifdef _MAC_DEBUG
	ObjectTraverse(msgin);
#endif
	if ((err == noErr) && !handled)
	{
		err = errAEEventNotHandled;
	}
	return err;
}

pascal OSErr AppleEventManager::SetData(const AppleEvent *msgin, AppleEvent *reply, long inRef)
{
	AEDesc		aDesc;
	OSErr err = noErr;
	Boolean handled = false;

	if (!reply || reply->descriptorType == typeNull)
	{
		return errAEEventNotHandled;
	}

	err = AEGetParamDesc(msgin, keyDirectObject, typeObjectSpecifier, &aDesc);
	if (noErr == err)
	{
		DescType data_desc;
		MacSize data_size;
		DescType set_type;

		// Find out what object the sender desired
		err = AEGetKeyPtr(&aDesc, keyAEDesiredClass, cType, &data_desc, &set_type, sizeof(set_type), &data_size);
		if (noErr == err)
		{
			if (set_type == formPropertyID)
			{
				/* "get [something] of [parent]"
					-> 'data',  **** , x, [data]         (keyAEData, ...)
					-> '----', 'obj ', ...               (keyDirectObject, typeObjectSpecifier, ...)
						-> 'form', 'enum', 4, 'prop'     (keyAEKeyForm, cEnumeration, 4, formPropertyID)
						-> 'want', 'type', 4, 'prop'     (keyAEDesiredClass, cType, 4, formPropertyID)
						-> 'seld', 'type', 4, [something](keyAEKeyData, cType, 4, something)
						-> 'from', 'obj ',84, [parent]   (keyAEContainer, typeObjectSpecifier, 4, parent)
				*/
				DescType set_prop;
				err = AEGetKeyPtr(&aDesc, keyAEKeyData, cType, &data_desc, &set_prop, sizeof(set_prop), &data_size);
				if (err == noErr)
				{
					AEDesc get_from;
					err = AEGetKeyDesc(&aDesc, keyAEContainer, typeObjectSpecifier, &get_from);
					if (err == noErr)
					{
						DesktopWindow *win = GetWindowFromAEDesc(&get_from);
						if (win)
						{
							handled = true;

							OP_ASSERT(!"Implement support for window properties");

							switch (set_prop)
							{
								case 'url ':	// TEXT, url
									if (win->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
									{
										win = ((BrowserDesktopWindow*)win)->GetActiveDocumentDesktopWindow();
									}
									if (win && win->GetType() == OpTypedObject::WINDOW_TYPE_DOCUMENT)
									{
										uni_char* uniText = GetNewStringFromObjectAndKey(msgin, keyAEData);
										if (uniText) {
											g_application->GoToPage(uniText, FALSE, TRUE, FALSE, win);
											delete [] uniText;
										}
										else
											err = paramErr;
									}
									else
									{
										handled = false;
									}
									break;

								case pIndex:	// long, index
									{
										SInt32 index;
										err = AEGetParamPtr(msgin, keyAEData, typeSInt32, &data_desc, &index, sizeof(index), &data_size);
										if (err == noErr)
										{
											if (index > 0)
												handled = SetTopLevelWindowNumber(win, index);
											else
												err = paramErr;
										}
									}
									break;

								default:
									handled = false;
									break;
							}
						}
						AEDisposeDesc(&get_from);
					}
				}
			}
		}
		AEDisposeDesc(&aDesc);
	}
#ifdef _MAC_DEBUG
	ObjectTraverse(msgin);
#endif
	if ((err == noErr) && !handled)
	{
		err = errAEEventNotHandled;
	}
	return err;
}

pascal OSErr AppleEventManager::Count(const AppleEvent *msgin, AppleEvent *reply, long inRef)
{
	AEDesc		aDesc;
	OSErr err = noErr;
	Boolean handled = false;
	int count = -1;

	err = AEGetParamDesc(msgin, keyDirectObject, typeObjectSpecifier, &aDesc);
	BrowserDesktopWindow* container_win = NULL;
	if (noErr == err)
	{
		container_win = (BrowserDesktopWindow*)GetWindowFromAEDesc(&aDesc);
		if (!container_win)
		{
			DescType data_desc;
			MacSize data_size;
			DescType get_type;
			long get_index;

			count = 0;
			err = AEGetKeyPtr(&aDesc, keyAEDesiredClass, cType, &data_desc, &get_type, sizeof(get_type), &data_size);
			if (noErr == err)
			{
				if (get_type == cWindow || get_type == cDocument)
				{
					err = AESizeOfKeyDesc(&aDesc, keyAEKeyData, &data_desc, &data_size);
					if (err == noErr)
					{
						Boolean toplevel = (get_type == cWindow);

						AEDesc container_desc;
						BrowserDesktopWindow* container_win = NULL;
						err = AEGetKeyDesc(&aDesc, keyAEContainer, typeObjectSpecifier, &container_desc);
						if (err == noErr)
						{
							// "get first document in last window"
							container_win = (BrowserDesktopWindow*)GetWindowFromAEDesc(&container_desc);
							if (container_win && container_win->GetType() != OpTypedObject::WINDOW_TYPE_BROWSER)
							{
								container_win = NULL;
								err = paramErr;	// bad window, don't pretend we did not get a window, but report error.
							}
							AEDisposeDesc(&container_desc);
						}
						else
							err = noErr;

						if (err == noErr)
						{
							if ((data_desc == typeAbsoluteOrdinal) && (data_size == 4))
							{
								err = AEGetKeyPtr(&aDesc, keyAEKeyData, typeAbsoluteOrdinal, &data_desc, &get_index, sizeof(get_index), &data_size);
								if (err == noErr)
									count = GetWindowCount(toplevel, container_win);
							}
						}
					}
				}
			}
		}
		else if (container_win->GetType() != OpTypedObject::WINDOW_TYPE_BROWSER)
		{
			container_win = NULL;
			count = 0;
		}
		AEDisposeDesc(&aDesc);
	}
	err = noErr;

	if (count == -1)
	{
		DescType data_desc;
		MacSize data_size;
		DescType count_type;
		err = AEGetParamPtr(msgin, keyAEObjectClass, cType, &data_desc, &count_type, sizeof(count_type), &data_size);
		if (noErr == err)
		{
			if (count_type == cWindow || count_type == cDocument)
			{
				count = GetWindowCount(count_type == cWindow, container_win);
			}
		}
	}
	if (count >= 0)
	{
		err = AEPutParamPtr(reply, keyAEResult, typeSInt32, &count, sizeof(count));
		handled = true;
	}

#ifdef _MAC_DEBUG
	ObjectTraverse(msgin);
#endif
	if ((err == noErr) && !handled)
	{
		err = errAEEventNotHandled;
	}
	return err;
}

pascal OSErr AppleEventManager::Exists(const AppleEvent *msgin, AppleEvent *reply, long inRef)
{
	AEDesc		aDesc;
	OSErr err = noErr;
	Boolean handled = false;

	err = AEGetParamDesc(msgin, keyDirectObject, typeObjectSpecifier, &aDesc);
	if (noErr == err)
	{
		DesktopWindow* win = GetWindowFromAEDesc(&aDesc);
		Boolean value = (win != NULL);
		err = AEPutParamPtr(reply, keyAEResult, typeBoolean, &value, sizeof(value));
		AEDisposeDesc(&aDesc);
		handled = true;
	}
#ifdef _MAC_DEBUG
	ObjectTraverse(msgin);
#endif
	if ((err == noErr) && !handled)
	{
		err = errAEEventNotHandled;
	}
	return err;
}

pascal OSErr AppleEventManager::Save(const AppleEvent *msgin, AppleEvent *reply, long inRef)
{
	AEDesc		aDesc;
	OSErr err = noErr;
	Boolean handled = false;

	err = AEGetParamDesc(msgin, keyDirectObject, typeObjectSpecifier, &aDesc);
	if (noErr == err)
	{
		DescType data_desc;
		MacSize data_size;
		DescType save_type;

		err = AEGetKeyPtr(&aDesc, keyAEDesiredClass, cType, &data_desc, &save_type, sizeof(save_type), &data_size);
		if (noErr == err && (save_type == cWindow || save_type == cDocument))
		{
			DesktopWindow* win = GetWindowFromAEDesc(&aDesc);
			if (win)
			{
				if (win->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
				{
					win = ((BrowserDesktopWindow*)win)->GetActiveDocumentDesktopWindow();
				}
				if (win && win->GetType() == OpTypedObject::WINDOW_TYPE_DOCUMENT)
				{
					FSRef file;
					Boolean ok = GetFileFromObjectAndKey(msgin, keyAEFile, &file, true);
					if (ok)
					{
						OpString path;
						OpFileUtils::ConvertFSRefToUniPath(&file, &path);
						((DocumentDesktopWindow*)win)->SaveActiveDocumentAtLocation(path);
						handled = true;
					}
					else
					{
						// Not actually an error
						err = noErr;
						if (OpStatus::IsSuccess(((DocumentDesktopWindow*)win)->SaveActiveDocumentAsFile()))
							handled = true;
					}
				}
			}
		}
		AEDisposeDesc(&aDesc);
	}
#ifdef _MAC_DEBUG
	ObjectTraverse(msgin);
#endif
	if ((err == noErr) && !handled)
	{
		err = errAEEventNotHandled;
	}
	return err;
}

pascal OSErr AppleEventManager::ReopenApp(const AppleEvent *msgin, AppleEvent *reply, long inRef)
{
	if (g_application && g_application->IsBrowserStarted())
	{
		if (g_application->GetActiveDesktopWindow(FALSE))
		{
			// Bring potentially docked window up
			g_application->GetActiveDesktopWindow(FALSE)->Activate(TRUE);
		}
		else
		{
			OpVector<DesktopWindow> windows;
			g_application->GetDesktopWindowCollection().GetDesktopWindows(OpTypedObject::WINDOW_TYPE_BROWSER, windows);
			for (UINT32 i = 0; i < windows.GetCount(); i++)
			{
				DesktopWindow* dw = windows.Get(i);
				if(dw && dw->IsMinimized())
				{
					dw->Activate(TRUE);
					return noErr;
				}
			}
		
			// Force new page for both SDI and MDI 
			g_application->GetBrowserDesktopWindow(TRUE, FALSE, TRUE);
		}
	}

	return noErr;
}

pascal OSErr AppleEventManager::Open(const AppleEvent *msgin, AppleEvent *reply, long inRef)
{
	OSErr err = noErr;
	FSRef ref;
//	uni_char url[2048];
	uni_char *url = NULL;
	AEDesc		aDesc;
	AEDescList	docList;
	long		x, numDocs;
	AEKeyword	keyword;
	OpString url_str;

	if (AEGetParamDesc( msgin, keyDirectObject, typeAEList, &docList) == noErr &&
		AECountItems( &docList, &numDocs ) == noErr)
	{
		// Go through list and open all  items:
		for (x = 1; x <= numDocs; x++)
		{
			// Get the current descriptor
			err = AEGetNthDesc(&docList, x, typeObjectSpecifier, &keyword, &aDesc);
			//err = AEGetParamDesc(msgin, keyDirectObject, typeObjectSpecifier, &aDesc);
			if (err == errAECoercionFail)
			{
				// Not a file... hmm... try to open it ayway
				url = GetNewStringFromListAndIndex(&docList, x);
				err = noErr;
			}
			if (noErr != err)
			{
				AEInitializeDesc(&aDesc);
			}
			//err = AEGetParamDesc(msgin, keyDirectObject, typeObjectSpecifier, &aDesc);
			if(noErr == err && g_application)
			{
				DesktopWindow * win = NULL;
				if (!url)
				{
					AEDesc winDesc;
					err = AEGetKeyDesc(&aDesc, keyAEContainer, typeObjectSpecifier, &winDesc);
					if (err == noErr) {
						win = GetWindowFromAEDesc(&winDesc);
						AEDisposeDesc(&winDesc);
						if (win && win->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
						{
							win = ((BrowserDesktopWindow*)win)->GetActiveDocumentDesktopWindow();
						}
					}
					// Failing the previous block is not an error
					err = noErr;

					AECreateDesc(typeNull, NULL, 0, &winDesc);
					AEDeleteParam(&aDesc, keyAEContainer);
					AEPutParamDesc(&aDesc, keyAEContainer, &winDesc);

					AEDesc fileDesc;
					err = AECoerceDesc(&aDesc, typeFSRef, &fileDesc);
					if (err == noErr)
					{
						err = AEGetDescData(&fileDesc, &ref, sizeof(ref));
						if (err == noErr)
							OpFileUtils::ConvertFSRefToUniPath(&ref, &url_str);
					}
					if (err == errAECoercionFail)
					{
						// Hmm
						url = GetNewStringFromObjectAndKey(&aDesc, keyAEKeyData);
						if (url)
							err = noErr;
					}
				}

				if (err == noErr)
				{
					if (url_str.IsEmpty())
						url_str.Set(url);

					BOOL3 newWindow = MAYBE;
					BOOL3 newPage = MAYBE;
					if( g_pcui->GetIntegerPref(PrefsCollectionUI::SDI) == 1)
					{
						newWindow = YES;
					}
					else
					{
						if(numDocs > 1 || g_pcdoc->GetIntegerPref(PrefsCollectionDoc::NewWindow))
						{
							newPage = YES;
						}
					}

					if(g_application->IsBrowserStarted() 
#ifdef WIDGET_RUNTIME_SUPPORT					   
					   || CocoaPlatformUtils::IsGadgetInstallerStartup()
#endif					   
					   )
					{
						if (win)
						{
							g_application->GoToPage(url, FALSE, TRUE, FALSE, win);
						}
						else
						{
#ifdef WIDGET_RUNTIME_SUPPORT
							if (CocoaPlatformUtils::IsGadgetInstallerStartup())
							{
								g_widgetPath.Set(url_str);
								g_main_message_handler->PostMessage(MSG_QUICK_APPLICATION_START, 0, 0);								
							}
							else	
#endif							
							g_application->OpenURL(url_str, newWindow, newPage, NO, 0, FALSE, TRUE);
						}
					}
					else
					{
						(void) new DelayedOpenURL(url_str.CStr(), newWindow, newPage, win);
					}
				}
				AEDisposeDesc(&aDesc);
			}
			delete [] url;
			url = NULL;
		}
	}
	AEDisposeDesc(&docList);
#ifdef _MAC_DEBUG
	ObjectTraverse(msgin);
#endif
	return err;
}

pascal OSErr AppleEventManager::Print(const AppleEvent *msgin, AppleEvent *reply, long inRef)
{
	AEDesc		aDesc;
	OSErr err = noErr;
	Boolean handled = false;

	err = AEGetParamDesc(msgin, keyDirectObject, typeObjectSpecifier, &aDesc);
	if (noErr == err)
	{
		DescType data_desc;
		MacSize data_size;
		DescType close_type;

		err = AEGetKeyPtr(&aDesc, keyAEDesiredClass, cType, &data_desc, &close_type, sizeof(close_type), &data_size);
		if (noErr == err && (close_type == cWindow || close_type == cDocument))
		{
			DesktopWindow* win = GetWindowFromAEDesc(&aDesc);
			if (win)
			{
				if (win->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
				{
					win = ((BrowserDesktopWindow*)win)->GetActiveDocumentDesktopWindow();
				}
				if (win && win->GetType() == OpTypedObject::WINDOW_TYPE_DOCUMENT)
				{
					OpInputAction start_print(OpInputAction::ACTION_PRINT_DOCUMENT);
					handled = g_input_manager->InvokeAction(&start_print, g_application->GetInputContext());
				}
			}
		}
		AEDisposeDesc(&aDesc);
	}
#ifdef _MAC_DEBUG
	ObjectTraverse(msgin);
#endif
	if ((err == noErr) && !handled)
	{
		err = errAEEventNotHandled;
	}
	return err;
}

#ifdef _MAC_DEBUG
#pragma mark -

pascal OSErr AppleEventManager::DoNothing(const AppleEvent *msgin, AppleEvent *reply, long inRef)
{
	ObjectTraverse(msgin);
	return noErr;
}
#endif

#pragma mark -

pascal OSErr AppleEventManager::OpenURL(const AppleEvent *msgin, AppleEvent *reply, long inRef)
{
	AEDesc		aDesc;
	char * url = NULL;
	int url_len;
	OSErr err = noErr;	
	
	err = AEGetParamDesc(msgin, keyDirectObject, typeChar, &aDesc);
	if(noErr == err && g_application)
	{
		if(aDesc.dataHandle != nil)
		{
			url_len = AEGetDescDataSize(&aDesc);
			url = new char[url_len + 1];
			AEGetDescData(&aDesc, url, url_len);
			url[url_len] = '\0';
			OpString url_str;
			url_str.Set(url);

#if (defined(_MACINTOSH_))
			if (0 == strncmp(url, "file://", 7))
			{
				// BBEdit sends oldstyle URLs, need to convert
				url_str.Set(ConvertClassicToXPathStyle(url_str.CStr()));
			}
#endif
			BOOL3 newWindow = MAYBE;
			BOOL3 newPage = MAYBE;
			if( g_pcui->GetIntegerPref(PrefsCollectionUI::SDI) == 1)
			{
				newWindow = YES;
			}
			else
			{
				if((STARTUPTYPE)g_pcui->GetIntegerPref(PrefsCollectionUI::StartupType) != STARTUP_NOWIN)
				{
					newPage = YES;
				}
			}

			if(g_application->IsBrowserStarted()
#ifdef WIDGET_RUNTIME_SUPPORT					   
			   || CocoaPlatformUtils::IsGadgetInstallerStartup()
#endif					   			   
			   )
			{
#ifdef WIDGET_RUNTIME_SUPPORT
				if (CocoaPlatformUtils::IsGadgetInstallerStartup())
				{
					g_widgetPath.Set(url_str);
					g_main_message_handler->PostMessage(MSG_QUICK_APPLICATION_START, 0, 0);								
				}
				else	
#endif												
					g_application->OpenURL(url_str, newWindow, newPage, NO, 0, FALSE, TRUE);
			}
			else
			{
				(void) new DelayedOpenURL(url_str.CStr(), newWindow, newPage, NULL);
			}
			delete [] url;
		}
		AEDisposeDesc(&aDesc);
	}
	return err;
	return noErr;
}

pascal OSErr AppleEventManager::ListWindows(const AppleEvent *msgin, AppleEvent *reply, long inRef)
{
	int count = 1;
	AEDesc item_desc;
	OSErr err;
	DesktopWindow* win;
	err = AECreateList(NULL, 0, false, &item_desc);
	while (err == noErr && (win = GetNumberedWindow(count, true, NULL)))
	{
		int id = GetExportIDForDesktopWindow(win, true);
		err = AEPutPtr(&item_desc, count, cLongInteger, &id, sizeof(id));
		count++;
	}
	err = AEPutParamDesc(reply, keyAEResult, &item_desc);
	AEDisposeDesc(&item_desc);
	return err;
}

pascal OSErr AppleEventManager::GetWindowInfo(const AppleEvent *msgin, AppleEvent *reply, long inRef)
{
	OSErr			err = -1;
	long			len;
	AEDescList		resultList;
	AEDesc			aDesc;
	Str255			macString;
	OpString		urlStr;
	const uni_char*	url = NULL;
	const uni_char*	title = NULL;

	if(g_application && !g_application->IsEmBrowser())
	{
		DesktopWindow* win = GetWindowFromObjectAndKey(msgin, keyDirectObject);

		if (win && win->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
			win = ((BrowserDesktopWindow*)win)->GetActiveDocumentDesktopWindow();

		if (win && win->GetType() == OpTypedObject::WINDOW_TYPE_DOCUMENT)
		{
			((DocumentDesktopWindow*)win)->GetURL(urlStr);
			url = urlStr.CStr();
			title = win->GetTitle();
		}

		err = AECreateList(NULL, 0L, false, &resultList);
		if(noErr == err)
		{
			if(url)
			{
				gTextConverter->ConvertStringToMacP(url, macString);
			}
			else
			{
				macString[0] = 0;
			}

			len = macString[0];
			err = AECreateDesc(typeChar, macString + 1, len, &aDesc);
			if(noErr == err)
			{
				AEPutDesc(&resultList, 0, &aDesc);
				AEDisposeDesc(&aDesc);
			}

			if(title)
			{
				gTextConverter->ConvertStringToMacP(title, macString);
			}
			else
			{
				macString[0] = 0;
			}

			len = macString[0];
			err = AECreateDesc(typeChar, macString + 1, len, &aDesc);
			if(noErr == err)
			{
				AEPutDesc(&resultList, 0, &aDesc);
				AEDisposeDesc(&aDesc);
			}
			err = AEPutParamDesc(reply, keyAEResult, &resultList);
			AEDisposeDesc(&resultList);
		}
	}

	return(err);
}

pascal OSErr AppleEventManager::CloseWindow(const AppleEvent *msgin, AppleEvent *reply, long inRef)
{
	Boolean success = false;
	OSErr err = noErr;
	DesktopWindow* win = GetWindowFromObjectAndKey(msgin, keyDirectObject);
	if (win)
	{
		win->Close(TRUE);
		success = true;
	}
	err = AEPutParamPtr(reply, keyAEResult, typeBoolean, &success, sizeof(success));
	return err;
}

pascal OSErr AppleEventManager::CloseAllWindows(const AppleEvent *msgin, AppleEvent *reply, long inRef)
{
	Boolean success = true;
	OSErr err = noErr;
	CloseEveryWindow();
	err = AEPutParamPtr(reply, keyAEResult, typeBoolean, &success, sizeof(success));
	return err;
}

#pragma mark -

#ifdef SUPPORT_SHARED_MENUS
// --------------------------------------------------------------------------------------------------------------
//	errordialog
// --------------------------------------------------------------------------------------------------------------

static pascal void errordialog(Str255 s)
{
	//ParamText(s, "\p", "\p", "\p");
	//Alert(2300, nil);
}

// --------------------------------------------------------------------------------------------------------------
//	eventfilter
// --------------------------------------------------------------------------------------------------------------

static pascal void eventfilter(EventRecord *ev, ...)
{
	//gOperaApp->DispatchEvent(*ev); /*could receive an update, activate, OS, or null event*/
}

// --------------------------------------------------------------------------------------------------------------
//	InitializeSharedMenusStuff
// --------------------------------------------------------------------------------------------------------------

OSErr InitializeSharedMenusStuff()
{
	if(!InitSharedMenus(errordialog, eventfilter)) /*See Step #1*/
	{
		return(-1);
	}
	return(noErr);
}

OSErr InformApplication(long signature)
{
#define RETURN_IF_OSERR(err)	\
	do 							\
	{							\
		OSErr __theErr = err;	\
		if(__theErr != noErr) 	\
		{						\
			return(__theErr);	\
		}						\
	} while(false)

	OSErr			err = noErr;
	AEAddressDesc	pgmAddr;
	AEDesc			aDesc;
	AppleEvent		theEvent;
	AppleEvent		theAEReply;

	OpString		urlStr;
	const uni_char	*url;
	char			macurl[2048];

	DesktopWindow* win = g_application->GetActiveDesktopWindow(FALSE);

	if (win && win->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
		win = ((BrowserDesktopWindow*)win)->GetActiveDocumentDesktopWindow();

	if (win && win->GetType() == OpTypedObject::WINDOW_TYPE_DOCUMENT)
	{
		((DocumentDesktopWindow*)win)->GetURL(urlStr);
		url = urlStr.CStr();
	}

	err = AECreateDesc(typeApplSignature, (Ptr) &signature, sizeof(signature), &pgmAddr);
	RETURN_IF_OSERR(err);
	err = AECreateAppleEvent('WWW?', 'URLE', &pgmAddr, kAutoGenerateReturnID, kAnyTransactionID, &theEvent);
	AEDisposeDesc(&pgmAddr);
	RETURN_IF_OSERR(err);

// --- URL
	if(url)
	{
		gTextConverter->ConvertStringToMacC(url, macurl, ARRAY_SIZE(macurl));
	}
	else
	{
		macurl[0] = 0;
	}
	const char *up = macurl;
	long len = strlen(up);
	err = AECreateDesc(typeChar, up, len, &aDesc);
	RETURN_IF_OSERR(err);
	err = AEPutParamDesc(&theEvent, keyAEResult, &aDesc);
	AEDisposeDesc(&aDesc);
	RETURN_IF_OSERR(err);

// --- MIME
	const char *cp = "text/html"; //GetContentMimeType();
	err = AECreateDesc(typeChar, cp, strlen(cp), &aDesc);
	RETURN_IF_OSERR(err);
	err = AEPutParamDesc(&theEvent, 'MIME', &aDesc);
	AEDisposeDesc(&aDesc);
	RETURN_IF_OSERR(err);

// --- RFRR
	err = AECreateDesc(typeChar, nil, 0l, &aDesc);
	RETURN_IF_OSERR(err);
	err = AEPutParamDesc(&theEvent, 'RFRR', &aDesc);
	AEDisposeDesc(&aDesc);
	RETURN_IF_OSERR(err);

// --- URL
//		long aLong = (long) this;
	long	aLong = -1; //WindowID();
	err = AECreateDesc(typeLongInteger, &aLong, sizeof(aLong), &aDesc);
	RETURN_IF_OSERR(err);
	err = AEPutParamDesc(&theEvent, 'WIND', &aDesc);
	AEDisposeDesc(&aDesc);
	RETURN_IF_OSERR(err);
#ifdef NO_CARBON
	err = AESendMessage(&theEvent, &theAEReply, kAENoReply, kNoTimeOut);
#else
	err = AESend(&theEvent, &theAEReply, kAENoReply, kAENormalPriority, kNoTimeOut, nil, nil);
#endif
	return(err);
}

#endif	// SUPPORT_SHARED_MENUS

// --------------------------------------------------------------------------------------------------------------
//	AERegisterURLEcho
// --------------------------------------------------------------------------------------------------------------

pascal OSErr AppleEventManager::RegisterURLEcho(const AppleEvent *msgin, AppleEvent *reply, long inRef)
{
	OSErr			err;
	long			signature;
	long			returnedSize;
	DescType		returnedType;
	Boolean			result;

	err = AEGetParamPtr(msgin, keyDirectObject, typeApplSignature, &returnedType, (Ptr) &signature, sizeof(signature), &returnedSize);
	if(noErr == err)
	{
		INT32 itsPosition = gInformApps.Find(signature);
		result = true;
		if(itsPosition == -1)
		{
			result = OpStatus::IsSuccess(gInformApps.Add(signature));
		}
		err = AEPutParamPtr(reply, keyAEResult, typeBoolean, &result, sizeof(result));
	}
	return(err);
}

// --------------------------------------------------------------------------------------------------------------
//	AEUnregisterURLEcho
// --------------------------------------------------------------------------------------------------------------

pascal OSErr AppleEventManager::UnregisterURLEcho(const AppleEvent *msgin, AppleEvent *reply, long inRef)
{
	OSErr			err;
	long			signature;
	long			returnedSize;
	DescType		returnedType;

	err = AEGetParamPtr(msgin, keyDirectObject, typeApplSignature, &returnedType, (Ptr) &signature, sizeof(signature), &returnedSize);
	if(noErr == err)
	{
		gInformApps.RemoveByItem(signature);
	}
	return(err);
}
