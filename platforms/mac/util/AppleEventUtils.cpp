/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/util/AppleEventUtils.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "platforms/mac/util/CTextConverter.h"
#include "platforms/mac/quick_support/CocoaQuickSupport.h"
#include "platforms/mac/pi/CocoaOpWindow.h"
#include "platforms/mac/File/FileUtils_Mac.h"

OpVector<DesktopWindow> topWindowList;
OpINT32Vector			topRefList;
OpVector<DesktopWindow> docWindowList;
OpINT32Vector			docRefList;


void CloseEveryWindow()
{
	g_application->GetDesktopWindowCollection().GentleClose();
}

unsigned int GetExportIDForDesktopWindow(DesktopWindow* win, Boolean toplevel)
{
	long index;
	static int top_unique = 1;
	static int doc_unique = 1;
	if (toplevel)
	{
		index = topWindowList.Find(win);
		if (index < 0)
		{
			index = topWindowList.GetCount();
			topWindowList.Insert(index, win);
			topRefList.Insert(index, top_unique++);
		}
		return topRefList.Get(index);
	}
	else
	{
		index = docWindowList.Find(win);
		if (index < 0)
		{
			index = docWindowList.GetCount();
			docWindowList.Insert(index, win);
			docRefList.Insert(index, doc_unique++);
		}
		return docRefList.Get(index);
	}
}

DesktopWindow* GetDesktopWindowForExportID(unsigned int export_id, Boolean toplevel)
{
	DesktopWindowCollection& collection = g_application->GetDesktopWindowCollection();

	if (toplevel)
	{
		for (DesktopWindowCollectionItem* item = collection.GetFirstToplevel(); item; item = item->GetSiblingItem())
		{
			DesktopWindow* window = item->GetDesktopWindow();
			if (window && GetExportIDForDesktopWindow(window, toplevel) == export_id)
				return window;
		}
	}
	else
	{
		OpVector<DesktopWindow> windows;
		OpStatus::Ignore(collection.GetDesktopWindows(OpTypedObject::WINDOW_TYPE_DOCUMENT, windows));
	
		for (unsigned i = 0; i < windows.GetCount(); i++)
		{
			DesktopWindow* window = windows.Get(i);
			if (GetExportIDForDesktopWindow(window, toplevel) == export_id)
				return window;
		}
	}

	return NULL;
}

void InformDesktopWindowCreated(DesktopWindow* new_win)
{
	if (new_win)
	{
		if (!new_win->GetParentWorkspace())
			GetExportIDForDesktopWindow(new_win, true);
		if (new_win->GetType() == OpTypedObject::WINDOW_TYPE_DOCUMENT)
			GetExportIDForDesktopWindow(new_win, false);
	}
}

void InformDesktopWindowRemoved(DesktopWindow* dead_win)
{
	long index = topWindowList.Find(dead_win);
	if (index >= 0)
	{
		topWindowList.Remove(index);
		topRefList.Remove(index);
	}
	index = docWindowList.Find(dead_win);
	if (index >= 0)
	{
		docWindowList.Remove(index);
		docRefList.Remove(index);
	}
}

#pragma mark -

// A proper object descriptor has 4 parts:
// - What class it is
// - How it is being identified (in our case an ID number, specified by formUniqueID)
// - The identification itself
// - The parent of the object
OSErr CreateAEDescFromID(unsigned int id_num, DescType object_type, AEDesc* item_desc)
{
	AEDesc item_record;
	unsigned int form = formUniqueID;
	OSStatus err;
	err = AECreateList(NULL, 0, true, &item_record);
	if (err == noErr) {
		AERecord * record = &item_record;
		err = AEPutKeyPtr(record, keyAEDesiredClass, cType, &object_type, sizeof(object_type));
		err |= AEPutKeyPtr(record, keyAEContainer, typeNull, NULL, 0);	// No parent
		err |= AEPutKeyPtr(record, keyAEKeyForm, cEnumeration, &form, sizeof(form));
		err |= AEPutKeyPtr(record, keyAEKeyData, typeUInt32, &id_num, sizeof(id_num));
		if (err == noErr)
		{
			err = AECoerceDesc(record, typeObjectSpecifier, item_desc);
		}
		AEDisposeDesc(&item_record);
	}
	return err;
}

// NOTE: AELists have 1-based indexes
OSErr AddIDToAEList(unsigned int id_num, long index, DescType object_type, AEDescList* list)
{
	AEDesc item_desc;
	AECreateDesc(typeNull, NULL, 0, &item_desc);
	OSStatus err = CreateAEDescFromID(id_num, object_type, &item_desc);
	if (noErr == err)
	{
		err = AEPutDesc(list, index, &item_desc);
		AEDisposeDesc(&item_desc);
	}
	return err;
}

OSErr CreateAEListFromIDs(unsigned int* id_nums, long count, DescType object_type, AEDescList* list)
{
	int i;
	OSStatus err;
	err = AECreateList(NULL, 0, false, list);
	for (i = 0; i < count && err == noErr; i++)
	{
		err = AddIDToAEList(id_nums[i], i+1, object_type, list);
	}
	return err;
}

#pragma mark -

int GetWindowCount(Boolean toplevel, BrowserDesktopWindow* container)
{
	if (toplevel)
		return g_application->GetDesktopWindowCollection().GetToplevelCount();
	else if (container)
		return container->GetModelItem().CountDescendants(OpTypedObject::WINDOW_TYPE_DOCUMENT);

	return g_application->GetDesktopWindowCollection().GetCount(OpTypedObject::WINDOW_TYPE_DOCUMENT);
}

int GetTopLevelWindowNumber(DesktopWindow* window)
{
	DesktopWindowCollection& windows = g_application->GetDesktopWindowCollection();

	int win_num = 0;	
	int this_index = 1;
	for (void* theWin = GetNSWindowWithNumber(win_num++); theWin; theWin = GetNSWindowWithNumber(win_num++))
	{
		for (DesktopWindowCollectionItem* item = windows.GetFirstToplevel(); item; item = item->GetSiblingItem())
		{
			DesktopWindow* win = item->GetDesktopWindow();
			CocoaOpWindow* opwin = (CocoaOpWindow*)(win ? win->GetOpWindow() : NULL);
			if (opwin && (opwin->IsSameWindow(theWin)))
			{
				if (win == window)
					return this_index;
				this_index++;
			}
		}
	}
	return 0;
}

Boolean SetTopLevelWindowNumber(DesktopWindow* window, int number)
{
	// IMPLEMENTME: rearrange window
	return false;
}
	// Main code to find a window from it's z-order number.
DesktopWindow* GetNumberedWindow(int number, Boolean toplevel, BrowserDesktopWindow* container)
{
	int count;
	count = GetWindowCount(toplevel, container);
	if (number < 0)
		number = count + number;
	else
		number -= 1;
	if (number < 0 || number >= count)
	{
		return NULL;
	}

	int this_index = 0;
	if (container)
	{
		OpWorkspace* workspace = container->GetWorkspace();
		DesktopWindow* sub_win;
		int sub_count;
		if (workspace)
		{
			sub_count = workspace->GetDesktopWindowCount();
			for (int j = 0; j < sub_count; j++)
			{
				sub_win = workspace->GetDesktopWindowFromStack(j);
				if (sub_win && sub_win->GetType() == OpTypedObject::WINDOW_TYPE_DOCUMENT)
				{
					if (this_index == number)
						return sub_win;
					this_index++;
				}
			}
		}
		return NULL;
	}

	DesktopWindowCollection& windows = g_application->GetDesktopWindowCollection();
	int win_num = 0;
	void* theWin;
	while ((theWin = GetNSWindowWithNumber(win_num++)))
	{
		for (DesktopWindowCollectionItem* item = windows.GetFirstToplevel(); item; item = item->GetSiblingItem())
		{
			DesktopWindow* win = item->GetDesktopWindow();
			if (win)
			{
				CocoaOpWindow* opwin = (CocoaOpWindow*)win->GetOpWindow();
				if (opwin && (opwin->IsSameWindow(theWin)))
				{
					// It is a desktop window. Great!
					if (toplevel || win->GetType() == OpTypedObject::WINDOW_TYPE_DOCUMENT)
					{
						if (this_index == number)
							return win;
						this_index++;
					}
					else if (win->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
					{
						OpWorkspace* workspace = win->GetWorkspace();
						DesktopWindow* sub_win;
						int sub_count;
						if (workspace)
						{
							sub_count = workspace->GetDesktopWindowCount();
							for (int j = 0; j < sub_count; j++)
							{
								sub_win = workspace->GetDesktopWindowFromStack(j);
								if (sub_win && sub_win->GetType() == OpTypedObject::WINDOW_TYPE_DOCUMENT)
								{
									if (this_index == number)
										return sub_win;
									this_index++;
								}
							}
						}
					}
				}
			}
		}
	}
	return NULL;
}

DesktopWindow* GetOrdinalWindow(DescType ordinal, Boolean toplevel, BrowserDesktopWindow* container)
{
	int index = -1;
	int count = 0;
	if (ordinal == kAEMiddle || ordinal == kAEAny)
	{
		count = GetWindowCount(toplevel, container);
	}
	switch (ordinal)
	{
		case kAEFirst:
			index = 1;
			break;
		case kAELast:
			index = -1;
			break;
		case kAEMiddle:
			if (count == 0)
				return NULL;
			index = (count+1) / 2;
			break;
		case kAEAny:
			if (count == 0)
				return NULL;
			index = (random() % count) + 1;
			break;
		default:
			return NULL;
	}
	return GetNumberedWindow(index, toplevel, container);
}

DesktopWindow* GetNamedWindow(const uni_char* name, Boolean toplevel, BrowserDesktopWindow* container)
{
	DesktopWindowCollection& windows = g_application->GetDesktopWindowCollection();
	if (toplevel)
	{
		for (DesktopWindowCollectionItem* item = windows.GetFirstToplevel(); item; item = item->GetSiblingItem())
		{
			DesktopWindow* win = item->GetDesktopWindow();
			if (win && uni_strcmp(win->GetTitle(), name) == 0)
			{
				return win;
			}
		}
	}
	else
	{
		OpVector<DesktopWindow> document_windows;

		g_application->GetDesktopWindowCollection().GetDesktopWindows(OpTypedObject::WINDOW_TYPE_DOCUMENT, document_windows);

		for (unsigned int i = 0; i < document_windows.GetCount(); i++)
		{
			DesktopWindow* win = document_windows.Get(i);

			if (uni_strcmp(win->GetTitle(), name) == 0)
			{
				return win;
			}
		}
	}
	return NULL;
}

DesktopWindow* GetWindowFromAEDesc(const AEDesc *inDesc)
{
	DescType data_desc;
	MacSize data_size;
	DescType get_type;
	SInt32 get_index;
	DescType get_form;
	Boolean toplevel;
	OSStatus err;
	DesktopWindow* win = NULL;
	err = AEGetKeyPtr(inDesc, keyAEDesiredClass, cType, &data_desc, &get_type, sizeof(get_type), &data_size);
	if (noErr == err && (get_type == cWindow || get_type == cDocument))
	{
		// OK, looks like the request for a window, let's look closer.
		toplevel = (get_type == cWindow);
		err = AEGetKeyPtr(inDesc, keyAEKeyForm, cEnumeration, &data_desc, &get_form, sizeof(get_form), &data_size);
		if (err == noErr)
		{
			err = AESizeOfKeyDesc(inDesc, keyAEKeyData, &data_desc, &data_size);
			if (err == noErr)
			{
				if ((get_form == formUniqueID) && (data_desc == cLongInteger) && (data_size == 4))
				{
					err = AEGetKeyPtr(inDesc, keyAEKeyData, cLongInteger, &data_desc, &get_index, sizeof(get_index), &data_size);
					if (err == noErr)
						win = GetDesktopWindowForExportID(get_index, toplevel);
				}
				else if ((get_form == formAbsolutePosition) && (data_desc == cLongInteger) && (data_size == 4))
				{
					err = AEGetKeyPtr(inDesc, keyAEKeyData, cLongInteger, &data_desc, &get_index, sizeof(get_index), &data_size);
					if (err == noErr)
						win = GetNumberedWindow(get_index, toplevel, NULL);
				}
				else if ((get_form == formAbsolutePosition) && (data_desc == typeAbsoluteOrdinal) && (data_size == 4))
				{
					err = AEGetKeyPtr(inDesc, keyAEKeyData, typeAbsoluteOrdinal, &data_desc, &get_index, sizeof(get_index), &data_size);
					if (err == noErr)
						win = GetOrdinalWindow(get_index, toplevel, NULL);
				}
				else if ((get_form == formName) && (data_desc == typeUnicodeText || data_desc == typeChar))
				{
					uni_char* uniName = GetNewStringFromObjectAndKey(inDesc, keyAEKeyData);
					if (uniName) {
						win = GetNamedWindow(uniName, toplevel, NULL);
						delete [] uniName;
					}
				}
			}
		}
	}
	return win;
}

#pragma mark -

OSErr CreateDescForWindow(DesktopWindow* win, Boolean toplevel, AEDesc* item_desc)
{
	DescType data_desc = toplevel ? (DescType)cWindow : (DescType)cDocument;
	if (win)
		return CreateAEDescFromID(GetExportIDForDesktopWindow(win, toplevel), data_desc, item_desc);
	return paramErr;
}

// "get window n"
// if n is positive, get the nth window from the front
// if n is negative, -1 is the last window, -2 is second-to-last and so on
OSErr CreateDescForNumberedWindow(int number, Boolean toplevel, BrowserDesktopWindow* container, AEDesc* item_desc)
{
	DescType data_desc = toplevel ? (DescType)cWindow : (DescType)cDocument;
	DesktopWindow* win = GetNumberedWindow(number, toplevel, container);
	if (win)
		return CreateAEDescFromID(GetExportIDForDesktopWindow(win, toplevel), data_desc, item_desc);
	return paramErr;
}


// "get first window"
// "get last window"
// "get middle window"
// "get some window"
// "get windows"
OSErr CreateDescForOrdinalWindow(DescType ordinal, Boolean toplevel, BrowserDesktopWindow* container, AEDesc* item_desc)
{
	DescType data_desc = toplevel ? (DescType)cWindow : (DescType)cDocument;
	OSStatus err;
	DesktopWindow* win;
	if (ordinal == kAEAll)
	{
		// Special case. GetOrdinalWindow can only return one window.
		int count = 1;
		err = AECreateList(NULL, 0, false, item_desc);
		while (err == noErr && (win = GetNumberedWindow(count, toplevel, container)))
		{
			err = AddIDToAEList(GetExportIDForDesktopWindow(win, toplevel), count, data_desc, item_desc);
			count++;
		}
		return err;
	}
	win = GetOrdinalWindow(ordinal, toplevel, container);
	if (win)
		return CreateAEDescFromID(GetExportIDForDesktopWindow(win, toplevel), data_desc, item_desc);
	return paramErr;
}

// "get window \"Foo\""
OSErr CreateDescForNamedWindow(const uni_char* name, Boolean toplevel, BrowserDesktopWindow* container, AEDesc* item_desc)
{
	DescType data_desc = toplevel ? (DescType)cWindow : (DescType)cDocument;
	DesktopWindow* win = GetNamedWindow(name, toplevel, container);
	if (win)
		return CreateAEDescFromID(GetExportIDForDesktopWindow(win, toplevel), data_desc, item_desc);
	return paramErr;
}


#pragma mark -

uni_char* GetNewStringFromObjectAndKey(const AEDescList* desc, AEKeyword keyword)
{
	uni_char* uniText = NULL;
	DescType data_desc;
	MacSize data_size;
	OSStatus err;
	err = AESizeOfKeyDesc(desc, keyword, &data_desc, &data_size);
	if ((err == noErr) && (data_desc == typeUnicodeText || data_desc == typeChar))
	{
		if (data_desc == typeUnicodeText)
		{
			uniText = new uni_char[data_size/2 + 1];
			err = AEGetKeyPtr(desc, keyword, data_desc, &data_desc, uniText, data_size, &data_size);
			uniText[data_size/2] = 0;
			if (err != noErr)
			{
				delete [] uniText;
				uniText = NULL;
			}
		}
		else if (data_desc == typeChar)
		{
			char* macText = new char[data_size + 1];
			if (macText)
			{
				err = AEGetKeyPtr(desc, keyword, data_desc, &data_desc, macText, data_size, &data_size);
				if (err == noErr)
				{
					macText[data_size] = 0;
					uniText = new uni_char[data_size + 1];
					gTextConverter->ConvertStringFromMacC(macText, uniText, data_size+1);
					if (gTextConverter->GotConversionError())
					{
						delete [] uniText;
						uniText = NULL;
					}
				}
				delete [] macText;
			}
		}
	}
	return uniText;
}

DesktopWindow* GetWindowFromObjectAndKey(const AEDescList* desc, AEKeyword keyword)
{
	DescType data_desc;
	MacSize data_size;
	OSErr err;
	DesktopWindow* win = NULL;
	err = AESizeOfKeyDesc(desc, keyword, &data_desc, &data_size);
	if (err == noErr)
	{
		if (data_desc == cLongInteger) {
			// window ID
			int winID = 0;
			err = AEGetKeyPtr(desc, keyword, cLongInteger, &data_desc, &winID, sizeof(winID), &data_size);
			if (err == noErr) {
				if (winID == -1)
					win = g_application->GetActiveDesktopWindow(FALSE);
				else
					win = GetDesktopWindowForExportID(winID, true);
			}
		}
		else if (data_desc == typeObjectSpecifier) {
			// window object
			AEDesc winObject;
			err = AEGetKeyDesc(desc, keyword, typeObjectSpecifier, &winObject);
			if (err == noErr) {
				win = GetWindowFromAEDesc(&winObject);
				AEDisposeDesc(&winObject);
			}
		}
		else if (data_desc == typeUnicodeText || data_desc == typeChar) {
			// window name
			uni_char* name = GetNewStringFromObjectAndKey(desc, keyword);
			if (name) {
				win = GetNamedWindow(name, true, NULL);
				delete [] name;
			}
		}
	}
	else
	{
		win = g_application->GetActiveDesktopWindow(FALSE);
	}
	return win;
}

Boolean GetFileFromObjectAndKey(const AEDescList* desc, AEKeyword keyword, FSRef* file, Boolean create)
{
	DescType data_desc;
	MacSize data_size;
	OSErr err;
	err = AESizeOfKeyDesc(desc, keyword, &data_desc, &data_size);
	if (err == noErr)
	{
		if (data_desc == typeObjectSpecifier) {
			// file object...
			AEDesc fileObject;
			err = AEGetKeyDesc(desc, keyword, typeObjectSpecifier, &fileObject);
			if (err == noErr)
			{
				Boolean ok = GetFileFromObjectAndKey(&fileObject, keyAEKeyData, file, create);
				AEDisposeDesc(&fileObject);
				return ok;
			}
		}
		else if (data_desc == typeChar && data_size <= 255) {
			// file path...
			FSRef spec;
			Str255 path;
			err = AEGetKeyPtr(desc, keyword, typeChar, &data_desc, &path[1], 255, &data_size);
			path[0] = data_size;
			if (err == noErr)
			{
				FSRef folderRef;
				OpString unicode_path;
				unicode_path.SetL((const char*)path);
				err = FSFindFolder(kLocalDomain, kVolumeRootFolderType, kDontCreateFolder, &folderRef);
				// FIXME: ismailp - test rhoroughly
				err = FSMakeFSRefUnicode(&folderRef, unicode_path.Length(), (const UniChar*)unicode_path.CStr(), kTextEncodingUnknown, &spec);    //FSMakeFSSpec(0, 0, path, &spec);
				if (create && (err == noErr || err == fnfErr))
				{
					if (noErr == err)
					{
						FSDeleteObject(&spec);
					}
					err = FSCreateFileUnicode(&spec, 0, NULL, kFSCatInfoNone, NULL, file, NULL);
				}
			}
		}
		else if (data_desc == typeAlias) {
			// file alias...
			Handle alias = NewHandle(data_size);
			FSRef spec;
			HLock(alias);
			err = AEGetKeyPtr(desc, keyword, typeAlias, &data_desc, *alias, data_size, &data_size);
			if (err == noErr)
			{
				Boolean changed;
				err = FSResolveAlias(NULL, (AliasHandle)alias, &spec, &changed);
				if (create && (err == noErr || err == fnfErr))
				{
					if (noErr == err)
					{
						FSDeleteObject(&spec);
					}
					err = FSCreateFileUnicode(&spec, 0, NULL, kFSCatInfoNone, NULL, file, NULL); //FSpCreate(&spec, '\?\?\?\?', '\?\?\?\?', NULL);
				}
			}
			DisposeHandle(alias);
		}
		else {
			err = paramErr;
		}
	}
	return (err == noErr);
}

uni_char* GetNewStringFromListAndIndex(const AEDescList* desc, long index)
{
	uni_char* uniText = NULL;
	DescType data_desc;
	MacSize data_size;
	OSStatus err;
	AEKeyword keyword;
	err = AESizeOfNthItem(desc, index, &data_desc, &data_size);
	if ((err == noErr) && (data_desc == typeUnicodeText || data_desc == typeChar))
	{
		if (data_desc == typeUnicodeText)
		{
			uniText = new uni_char[data_size/2 + 1];
			err = AEGetNthPtr(desc, index, data_desc, &keyword, &data_desc, uniText, data_size, &data_size);
			uniText[data_size/2] = 0;
			if (err != noErr)
			{
				delete [] uniText;
				uniText = NULL;
			}
		}
		else if (data_desc == typeChar)
		{
			char* macText = new char[data_size + 1];
			if (macText)
			{
				err = AEGetNthPtr(desc, index, data_desc, &keyword, &data_desc, macText, data_size, &data_size);
				if (err == noErr)
				{
					macText[data_size] = 0;
					uniText = new uni_char[data_size + 1];
					gTextConverter->ConvertStringFromMacC(macText, uniText, data_size+1);
					if (gTextConverter->GotConversionError())
					{
						delete [] uniText;
						uniText = NULL;
					}
				}
				delete [] macText;
			}
		}
	}
	return uniText;
}

#pragma mark -


#ifdef _MAC_DEBUG
// Just walk through the event.
FILE* logfile = NULL;
void ObjectTraverse(const AEDescList *object)
{
	static int recurse=0;
	long count;
	AEKeyword keyword;
	DescType data_desc;
	MacSize data_size;
	char buffer[1024];
	OSStatus err;
	if (noErr == AECountItems(object, &count))
	{
		for (int i = 1; i <= count; i++)	// AERecord starts at index 1
		{
			err = AESizeOfNthItem(object, i, &data_desc, &data_size);
			if (noErr == err)
			{
				err = AEGetNthPtr(object, i, data_desc, &keyword, &data_desc, buffer, 1024, &data_size);
				if (logfile)
				{
					long r;
					for (r = 0; r < recurse; r++)
						fprintf(logfile,"  ");

					fprintf(logfile,"key:'%c%c%c%c' desc:'%c%c%c%c' size: %ld", FCC_TO_CHARS(keyword), FCC_TO_CHARS(data_desc), data_size);
					if (err == noErr) {
						if (data_size == 4) {
							int val = *((int*) buffer);
							if ((data_desc == typeSInt32) || (data_desc == typeUInt32))
								fprintf(logfile," value: %d", val);
							if ((data_desc == typeSInt32) || (data_desc == typeUInt32))
								fprintf(logfile," value:'%c%c%c%c'", FCC_TO_CHARS(val));
						}
					}
					fprintf(logfile,"\r");
				}
				if ((err == noErr) && ((data_desc == cObjectSpecifier) || (data_desc == cAEList) || (data_desc == typeAERecord))) {
					AEDesc obj2;
					err = AEGetNthDesc(object, i, data_desc, &keyword, &obj2);
					if (err == noErr) {
						recurse++;
						ObjectTraverse(&obj2);
						recurse--;
					}
				}
			}
		}
	}
}
#endif
