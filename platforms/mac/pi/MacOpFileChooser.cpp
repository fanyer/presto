/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/pi/MacOpFileChooser.h"
#include "platforms/mac/File/FileUtils_Mac.h"
#include "platforms/mac/pi/MacOpWindow.h"
#include "platforms/mac/model/CMacView.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "platforms/mac/util/PatternMatch.h"
#include "platforms/mac/util/ModalSystemDialogScope.h"

#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/prefs/prefsmanager/collections/pc_java.h"
#include "modules/prefs/prefsmanager/collections/pc_mswin.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

OP_STATUS OpFileChooser::Create(OpFileChooser** new_object)
{
	*new_object = new MacOpFileChooser;
	if (*new_object == NULL)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

struct DialogInfo
{
	OpFileChooserListener *listener;
	Boolean newStyleDialog;
	Boolean mustExist;
	Boolean filtersWereAllocated;
	Boolean isMultiselect;
	OpVector<OpString> *filterExtensions;
	AEDesc	*startLocation;
	AEDescList	*startSelection;
};

OSType GetMacTypeFromExtension(uni_char* extension);
OSType GetMacType(OpFileTypesList* types, int i = 0);

//There was an error in the documentation for OpFileChooser, so we shouldn't really return files with quotes.
//Note: defining this will break fopen, unlink etc. since the URL-code will get save-filenames from here and they
//      don't work at all if the filename is quoted. Bug #130897. <ED>
//Note#2: it should be defined for now, but we have to be aware of the quirks. Any multiselection filechooser SHOULD quote the list,
//        but the singleselection filechoosers SHOULD NOT quote the list. This is how windows does it. *sigh* <ED>
#define ENCLOSE_FILES_IN_QUOTES

pascal void MacOpFileChooser::NavigationDialogEventProc(NavEventCallbackMessage inSelector, NavCBRecPtr ioParams, void *ioUserData)
{
	OSStatus err;
	NavReplyRecord navReply;
	DialogInfo *dialog = (DialogInfo *)ioUserData;
	if (dialog) {
		switch (inSelector)
		{
			case kNavCBStart:
				NavCustomControl(ioParams->context, kNavCtlSetLocation, (void*)dialog->startLocation);
				if(dialog->startSelection)
					NavCustomControl(ioParams->context, kNavCtlSetSelection, (void*)dialog->startSelection);
				break;
			case kNavCBUserAction:
				if (ioParams->userAction == kNavUserActionOpen || ioParams->userAction == kNavUserActionSaveAs || ioParams->userAction == kNavUserActionChoose) {
					err = NavDialogGetReply(ioParams->context, &navReply);
					if ((err == noErr) && navReply.validRecord)
					{
						FSRef parentFSRef;
						FSRef fileFSRef;
						int i = 1;
						OpString paths;
						OpString filepath;
						DescType actualType;
						long actualSize;
						AEKeyword theKeyword;
						Boolean first = true;
						HFSUniStr255 hfsfilename;

						while (noErr == AEGetNthPtr(&(navReply.selection), i++, typeFSRef, &theKeyword, &actualType, &fileFSRef, sizeof(FSRef), &actualSize)) {
							if (ioParams->userAction == kNavUserActionSaveAs) {

								parentFSRef = fileFSRef; // this is ok for save file only

								hfsfilename.length = min(255, CFStringGetLength(navReply.saveFileName));
								CFStringGetCharacters(navReply.saveFileName, CFRangeMake(0, hfsfilename.length), hfsfilename.unicode);

								if (navReply.replacing) {
									FSRef fsRef;
									if(noErr == FSMakeFSRefUnicode(&parentFSRef, hfsfilename.length, hfsfilename.unicode, kTextEncodingUnicodeDefault, &fsRef))
									{
										FSDeleteObject(&fsRef);
									}
								}

								if (dialog->mustExist) {
									FSCatalogInfo catInfo;
									((FInfo *)catInfo.finderInfo)->fdType = '\?\?\?\?';
									((FInfo *)catInfo.finderInfo)->fdCreator = 'OPRA';

									err = FSCreateFileUnicode(&parentFSRef, hfsfilename.length, (UniChar*)hfsfilename.unicode, kFSCatInfoFinderInfo, &catInfo, &fileFSRef, NULL);
								}
							}
							if(OpStatus::IsSuccess(OpFileUtils::ConvertFSRefToUniPath(&fileFSRef, &filepath)))
							{
								if (ioParams->userAction == kNavUserActionSaveAs)
								{
									filepath.Append(PATHSEP);

									if (first)
									{
										TRAPD(rc, g_pcfiles->WriteDirectoryL(OPFILE_SAVE_FOLDER, filepath.CStr()));
									}
									filepath.Append((uni_char*)hfsfilename.unicode, hfsfilename.length);
								}
								else if (ioParams->userAction == kNavUserActionOpen)
								{
									TRAPD(rc, g_pcfiles->WriteDirectoryL(OPFILE_OPEN_FOLDER, filepath.CStr()));
								}
		#ifdef ENCLOSE_FILES_IN_QUOTES
								if(dialog->isMultiselect)	// this is a temp hack
								{
									filepath.Insert(0, UNI_L("\""));
									filepath.Append(UNI_L("\""));
								}
		#endif
							}
							if (!first)
							{
								filepath.Insert(0, UNI_L(", "));
							}

							paths.Append(filepath);
							first = false;
						}
						if(dialog->listener)
						{
							dialog->listener->OnFileSelected(paths.CStr());
						}
					}
					if (dialog->newStyleDialog) {
						NavDialogDispose(ioParams->context);
					}
					if(dialog->filtersWereAllocated)
					{
						if(dialog->filterExtensions)
						{
							dialog->filterExtensions->DeleteAll();
						}
					}
					delete dialog;
				}
				else if (ioParams->userAction == kNavUserActionCancel) {
					if (dialog->newStyleDialog) {
						NavDialogDispose(ioParams->context);
					}
					if(dialog->filtersWereAllocated)
					{
						if(dialog->filterExtensions)
						{
							dialog->filterExtensions->DeleteAll();
						}
					}
					delete dialog;
				}
				break;
		}
	}
}

pascal Boolean MacOpFileChooser::NavigationDialogFilterProc(AEDesc *theItem, void *navFileInfo, void *ioUserData, NavFilterModes filterMode)
{
	DialogInfo *dialog = (DialogInfo *)ioUserData;
	NavFileOrFolderInfo *info = (NavFileOrFolderInfo *)navFileInfo;

	FSRef fsref;
	OSErr err;
	HFSUniStr255 hfsfilename;
	uni_char* unifilename;
	FSCatalogInfo catInfo;
	uni_char *ext;

	if(filterMode != kNavFilteringShortCutVolumes)
	{
		// Note: navFileInfo is ONLY valid if theItem has a descriptorType of typeFSS or typeFSRef.
		if(theItem && (/*(theItem->descriptorType == typeFSS) ||*/ (theItem->descriptorType == typeFSRef)))
		{
			err = AEGetDescData(theItem, &fsref, sizeof(FSRef));
			if(err != noErr)
				return true;

			// Only filter out files
			if(!info->isFolder)
			{
				if(noErr == FSGetCatalogInfo(&fsref, kFSCatInfoFinderInfo, &catInfo, &hfsfilename, NULL, NULL))
				{
					for(unsigned int i = 0; i < dialog->filterExtensions->GetCount(); i++)
					{
						unifilename = (uni_char*)hfsfilename.unicode;
						unifilename[min(254, hfsfilename.length)] = 0;	// this may cut away the last character
						ext = dialog->filterExtensions->Get(i)->CStr();

						// if the filename doesn't have a matching extension...
						if(!UniWildCompare(ext, unifilename, false))
						{
							// ...and it's not a matched mac filetype then don't show it
							if(GetMacTypeFromExtension(ext) != ((FInfo *)catInfo.finderInfo)->fdType)
							{
								return false;
							}
						}
					}
				}
			}
		}
	}

	return true;
}

void CreateDialogOptions(const OpFileChooserSettings& settings, NavDialogOptions &dialogOptions, NavDialogCreationOptions &createOptions)
{
	Boolean				show_popup = false;

	NavGetDefaultDialogOptions(&dialogOptions);
	dialogOptions.dialogOptionFlags = show_popup ? (dialogOptions.dialogOptionFlags & ~kNavNoTypePopup) : (dialogOptions.dialogOptionFlags | kNavNoTypePopup);
	dialogOptions.dialogOptionFlags &= ~kNavAllowStationery;
	NavGetDefaultDialogCreationOptions(&createOptions);
	createOptions.optionFlags = show_popup ? (createOptions.optionFlags & ~kNavNoTypePopup) : (createOptions.optionFlags | kNavNoTypePopup);
	createOptions.optionFlags &= ~kNavAllowStationery;
	createOptions.optionFlags |= kNavSupportPackages; // kNavAllowOpenPackages

	if (!settings.m_multi_selection) {
		dialogOptions.dialogOptionFlags &= ~kNavAllowMultipleFiles;
		createOptions.optionFlags &= ~kNavAllowMultipleFiles;
	}
}

OSType GetMacTypeFromExtension(uni_char* extension)
{
	OSType result = 'TEXT';
	if (extension) {
		if (uni_strcmp(extension, UNI_L(".gif")) == 0)
			result = 'GIFf';
		else if (uni_strcmp(extension, UNI_L(".png")) == 0)
			result = 'PNGf';
		else if (uni_strcmp(extension, UNI_L(".bmp")) == 0)
			result = 'BMPf';
		else if ((uni_strcmp(extension, UNI_L(".jpg")) == 0) ||
				(uni_strcmp(extension, UNI_L(".jpe")) == 0) ||
				(uni_strcmp(extension, UNI_L(".jpeg")) == 0))
			result = 'JPEG';
	}
	return result;
}

OSType GetMacType(OpFileTypesList* types, int i)
{
	OpString * str = 0;
	OSType result = 'TEXT';
	if (types) {
		str = types->extensions.Get(i);
		if (str) {
			result = GetMacTypeFromExtension(str->CStr());
		}
	}
	return result;
}

OpVector<OpString> *GetFilterExtensions(OpStringC filterExtensions, OpVector<OpString> *extList)
{
	OpString *ext;
	int start;
	int end;
	Boolean done = false;
	OpString extStrList;
	extStrList.Set(filterExtensions);

	if(extStrList.Length() > 0)
	{
		do
		{
			start = extStrList.FindFirstOf(UNI_L('('));
			end = extStrList.FindFirstOf(UNI_L(')'));
			if((start != KNotFound) && (end != KNotFound))
			{
				ext = new OpString;
				if(ext)
				{
					ext->Set(extStrList.SubString(start+1, (end-start)-1));
					OpStatus::Ignore(extList->Add(ext));
				}
				extStrList.Delete(0, end+1);
			}
			else
			{
				done = true;
			}
		} while(!done);
	}

	return extList;
}

void MakeDefaultLocationDescriptor(const OpFileChooserSettings& settings, AEDesc*& desc, AEDescList*& selection)
{
	const FSRef * fsref = NULL;
	desc = new AEDesc;
	AECreateDesc(typeNull, NULL, 0, desc);
	selection = NULL;
	int last = settings.m_initial_file.FindLastOf(UNI_L(PATHSEPCHAR));
	if (last != KNotFound)
	{
		OpFile file;
		file.Construct(settings.m_initial_file);
		{
			FSRef ref;
			fsref = NULL;
			if(OpFileUtils::ConvertUniPathToFSRef(file.GetFullPath(),ref))
			{
				fsref = &ref;
			}
			if (fsref)
				AECreateDesc(typeFSRef, fsref, sizeof(FSRef), desc);
		}

		BOOL exists = FALSE;
		file.Exists(exists);
		if(exists)
		{
			selection = new AEDescList;
			FSRef ref;
			fsref = NULL;
			if(OpFileUtils::ConvertUniPathToFSRef(file.GetFullPath(),ref))
			{
				fsref = &ref;
			}
			if(fsref)
			{
				if (noErr == AECreateDesc(typeFSRef, fsref, sizeof(FSRef), desc))
				{
					if (noErr == AECreateList(NULL, 0, false, selection))
					{
						AEPutDesc(selection, 0, desc);
					}
				}
			}
		}
	}
	else/* if (settings.m_directory.IsValid())*/
	{
		FSRef ref;
		fsref = NULL;
		if(OpFileUtils::ConvertUniPathToFSRef(settings.m_directory, ref))
		{
			fsref = &ref;
		}
		if (fsref)
			AECreateDesc(typeFSRef, fsref, sizeof(FSRef), desc);
	}
}

OP_STATUS MacOpFileChooser::ShowOpenSelector(const OpWindow* parent, const OpString& caption, const OpFileChooserSettings& settings)
{
	OSErr						err;
	NavDialogOptions 			dialogOptions;
	NavDialogCreationOptions 	createOptions;
	WindowRef					win = NULL;
	NavEventUPP					eventProc;
	NavObjectFilterUPP			filterProc = NULL;
	NavReplyRecord				navReply;
	Boolean						handled = false;
	OpVector<OpString>			extensions;
	ModalSystemDialogScope		sys_scope;

	DialogInfo * info = new DialogInfo;
	info->listener = settings.m_listener;
	info->newStyleDialog = false;
	info->mustExist = settings.m_must_exist;
	info->filtersWereAllocated = false;
	info->isMultiselect = settings.m_multi_selection;
	info->filterExtensions = NULL;
	info->startLocation = NULL;
	info->startSelection = NULL;

	if(settings.m_filter_list)
	{
		info->filterExtensions = &settings.m_filter_list->extensions;
	}
	else if(settings.m_filter_string)
	{
		// We need to parse the filter string into file extension wildcards ("*.jpg", "*.gif" and so on)
		info->filterExtensions = GetFilterExtensions(settings.m_filter_string, &extensions);
		info->filtersWereAllocated = true;
	}

	eventProc = NewNavEventUPP(NavigationDialogEventProc);
	//filterProc = NewNavObjectFilterUPP(NavigationDialogFilterProc);
	CreateDialogOptions(settings, dialogOptions, createOptions);

	if(caption.Length() > 0)
	{
		createOptions.windowTitle = CFStringCreateWithCharacters(NULL, (UniChar*)caption.CStr(), caption.Length());
	}

	if (parent) {
		MacOpWindow* opwin = (MacOpWindow*)parent;
		if (opwin) {
			CMacView* view = opwin->GetBrowserSuperView();
			if (view && view->Visible()) {	// don't sheet when panel/page invisible, reserve sheet for active tab.
				win = view->GetWindowRef();
				createOptions.parentWindow = win;
				createOptions.modality = kWindowModalityAppModal; //kWindowModalityWindowModal;
			}
		}
	}

	AEDesc *defLocation;
	AEDescList *defSelection;
	MakeDefaultLocationDescriptor(settings, defLocation, defSelection);
	info->startLocation = defLocation;
	info->startSelection = defSelection;

	if (win) {
		NavDialogRef dialog;
		err = NavCreateGetFileDialog(&createOptions, NULL, eventProc, NULL, filterProc, info, &dialog);
		if (err == noErr) {
			info->newStyleDialog = true;
			err = NavDialogRun(dialog);
			handled = (err == noErr);
		}
	}

	if(createOptions.windowTitle)
		CFRelease(createOptions.windowTitle);

	if (!handled) {
		info->newStyleDialog = false;
		err = NavGetFile(defLocation, &navReply, &dialogOptions, eventProc, NULL, filterProc, NULL, info);
	}

	if(eventProc)
	{
		DisposeNavEventUPP(eventProc);
	}

	return OpStatus::OK;
	//return Show(settings.m_directory, settings.m_listener, FALSE, settings.m_multi_selection, settings.m_must_exist, NULL, parent);
}

OP_STATUS MacOpFileChooser::ShowSaveSelector(const OpWindow* parent, const OpString& caption, const OpFileChooserSettings& settings)
{
	OSErr						err;
	NavDialogOptions 			dialogOptions;
	NavDialogCreationOptions 	createOptions;
	WindowRef					win = NULL;
	NavEventUPP					eventProc;
	NavReplyRecord				navReply;
	Boolean						handled = false;
	const uni_char*				saveFilename = 0;
	ModalSystemDialogScope		sys_scope;

	DialogInfo * info = new DialogInfo;
	info->listener = settings.m_listener;
	info->newStyleDialog = false;
	info->mustExist = settings.m_must_exist;
	info->filtersWereAllocated = false;
	info->isMultiselect = settings.m_multi_selection;
	info->filterExtensions = NULL;
	info->startLocation = NULL;
	info->startSelection = NULL;

	eventProc = NewNavEventUPP(NavigationDialogEventProc);
	CreateDialogOptions(settings, dialogOptions, createOptions);

	// Paths paths paths...
	int last = settings.m_initial_file.FindLastOf(UNI_L(PATHSEPCHAR));
	if(last != KNotFound)
	{
		saveFilename = &settings.m_initial_file.CStr()[last+1];
	}
	else
	{
		saveFilename = settings.m_initial_file.CStr();
	}

	if(saveFilename)
	{
		createOptions.saveFileName = CFStringCreateWithCharacters(NULL, (UniChar*)saveFilename, uni_strlen(saveFilename));
	}
	if(caption.Length() > 0)
	{
		createOptions.windowTitle = CFStringCreateWithCharacters(NULL, (UniChar*)caption.CStr(), caption.Length());
	}

	if (parent) {
		MacOpWindow* opwin = (MacOpWindow*)parent;
		if (opwin) {
			CMacView* view = opwin->GetBrowserSuperView();
			if (view && view->Visible()) {	// don't sheet when panel/page invisible, reserve sheet for active tab.
				win = view->GetWindowRef();
				createOptions.parentWindow = win;
				createOptions.modality = kWindowModalityAppModal; //kWindowModalityWindowModal;
			}
		}
	}

	AEDesc *defLocation;
	AEDescList *defSelection;
	MakeDefaultLocationDescriptor(settings, defLocation, defSelection);
	info->startLocation = defLocation;
	info->startSelection = defSelection;

//	if (win)
	{
		NavDialogRef dialog;
		err = NavCreatePutFileDialog(&createOptions, GetMacType(settings.m_filter_list), 'OPRA', eventProc, info, &dialog);
		if (err == noErr) {
			info->newStyleDialog = true;
			err = NavDialogRun(dialog);
			handled = (err == noErr);
		}
	}

	if(createOptions.saveFileName)
		CFRelease(createOptions.saveFileName);
	if(createOptions.windowTitle)
		CFRelease(createOptions.windowTitle);

	if (!handled) {
		info->newStyleDialog = false;
		err = NavPutFile(defLocation, &navReply, &dialogOptions, eventProc, GetMacType(settings.m_filter_list), NULL, info);
	}
	if(eventProc)
	{
		DisposeNavEventUPP(eventProc);
	}

	return OpStatus::OK;

	//return Show(settings.m_directory, settings.m_listener, TRUE, settings.m_multi_selection, settings.m_must_exist, NULL, parent);
}

OP_STATUS MacOpFileChooser::ShowFolderSelector(const OpWindow* parent, const OpString& caption, const OpFileChooserSettings& settings)
{
	OSErr						err;
	NavDialogOptions 			dialogOptions;
	NavDialogCreationOptions 	createOptions;
	WindowRef					win = NULL;
	NavEventUPP					eventProc;
	NavReplyRecord				navReply;
	Boolean						handled = false;
	ModalSystemDialogScope		sys_scope;

	DialogInfo * info = new DialogInfo;
	info->listener = settings.m_listener;
	info->newStyleDialog = false;
	info->mustExist = settings.m_must_exist;
	info->filtersWereAllocated = false;
	info->isMultiselect = settings.m_multi_selection;
	info->filterExtensions = NULL;
	info->startLocation = NULL;
	info->startSelection = NULL;


	eventProc = NewNavEventUPP(NavigationDialogEventProc);
	CreateDialogOptions(settings, dialogOptions, createOptions);

	if(caption.Length() > 0)
	{
		createOptions.windowTitle = CFStringCreateWithCharacters(NULL, (UniChar*)caption.CStr(), caption.Length());
	}

	if (parent) {
		MacOpWindow* opwin = (MacOpWindow*)parent;
		if (opwin) {
			CMacView* view = opwin->GetBrowserSuperView();
			if (view && view->Visible()) {	// don't sheet when panel/page invisible, reserve sheet for active tab.
				win = view->GetWindowRef();
				createOptions.parentWindow = win;
				createOptions.modality = kWindowModalityAppModal; //kWindowModalityWindowModal;
			}
		}
	}

	if (win) {
		NavDialogRef dialog;
		err = NavCreateChooseFolderDialog(&createOptions, eventProc, NULL, info, &dialog);
		if (err == noErr) {
			info->newStyleDialog = true;
			err = NavDialogRun(dialog);
			handled = (err == noErr);
		}
	}

	if(createOptions.windowTitle)
		CFRelease(createOptions.windowTitle);

	if (!handled) {
		info->newStyleDialog = false;
		err = NavChooseFolder(NULL, &navReply, &dialogOptions, eventProc, NULL, info);
	}

	if(eventProc)
	{
		DisposeNavEventUPP(eventProc);
	}

	return OpStatus::OK;
}

#if 0
OP_STATUS MacOpFileChooser::Show(const OpLocation &dir, OpFileChooserListener* listener, BOOL save,
								BOOL multi, BOOL mustexist, OpFileTypesList* types/* = NULL*/, DesktopWindow* parent/*=NULL*/)
{
	OSErr				err;
	NavDialogOptions	dialogOptions;
	NavDialogCreationOptions	createOptions;
	NavEventUPP			eventProc;
	Boolean				show_popup = false;
	NavReplyRecord		navReply;
	WindowRef			win = NULL;
	Boolean				handled = false;
	ModalSystemDialogScope sys_scope;

	DialogInfo * info = new DialogInfo;
	info->listener = listener;
	info->mustExist = mustexist;
	info->newStyleDialog = false;

	eventProc = NewNavEventUPP(NavigationDialogEventProc);
	NavGetDefaultDialogOptions(&dialogOptions);
	dialogOptions.dialogOptionFlags = show_popup ? (dialogOptions.dialogOptionFlags & ~kNavNoTypePopup) : (dialogOptions.dialogOptionFlags | kNavNoTypePopup);
	dialogOptions.dialogOptionFlags &= ~kNavAllowStationery;
	NavGetDefaultDialogCreationOptions(&createOptions);
	createOptions.optionFlags = show_popup ? (createOptions.optionFlags & ~kNavNoTypePopup) : (createOptions.optionFlags | kNavNoTypePopup);
	createOptions.optionFlags &= ~kNavAllowStationery;
	if (!multi) {
		dialogOptions.dialogOptionFlags &= ~kNavAllowMultipleFiles;
		createOptions.optionFlags &= ~kNavAllowMultipleFiles;
	}
	if (parent) {
		MacOpWindow* opwin = (MacOpWindow*)parent;
		if (opwin) {
			CMacView* view = opwin->GetBrowserSuperView();
			if (view && view->Visible()) {	// don't sheet when panel/page invisible, reserve sheet for active tab.
				win = view->GetWindowRef();
				createOptions.parentWindow = win;
				createOptions.modality = kWindowModalityAppModal; //kWindowModalityWindowModal;
			}
		}
	}

	if (save)
	{
		if (win) {
			NavDialogRef dialog;
			err = NavCreatePutFileDialog(&createOptions, GetMacType(types), 'OPRA', eventProc, info, &dialog);
			if (err == noErr) {
				info->newStyleDialog = true;
				err = NavDialogRun(dialog);
				handled = (err == noErr);
			}
		}

		if (!handled) {
			info->newStyleDialog = false;
			err = NavPutFile(NULL, &navReply, &dialogOptions, eventProc, GetMacType(types), NULL, info);
		}
	}
	else
	{
		NavTypeListHandle list = NULL;
		if (types) {
			int count = types->extensions.GetCount();
			list = (NavTypeListHandle)NewHandleClear(sizeof(NavTypeList) + (count - 1) * sizeof(OSType));
			(*list)->componentSignature = kNavGenericSignature;
			(*list)->osTypeCount = count;
			for (int i = 0; i < count; i++) {
				(*list)->osType[i] = GetMacType(types, i);
			}
		}

		if (win) {
			NavDialogRef dialog;
			err = NavCreateGetFileDialog(&createOptions, list, eventProc, NULL, NULL, info, &dialog);
			if (err == noErr) {
				info->newStyleDialog = true;
				err = NavDialogRun(dialog);
				handled = (err == noErr);
			}
		}

		if (!handled) {
			info->newStyleDialog = false;
			err = NavGetFile(NULL, &navReply, &dialogOptions, eventProc, NULL, NULL, list, info);
		}
	}
	return OpStatus::OK;
}
#endif
