/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "platforms/mac/File/FileUtils_Mac.h"
#include "platforms/mac/pi/desktop/CocoaFileChooser.h"
#include "platforms/mac/pi/CocoaOpWindow.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/util/opfile/opfile.h"

BOOL SetOpString(OpString& dest, CFStringRef source);

#define BOOL NSBOOL
#import <AppKit/NSOpenPanel.h>
#import <AppKit/NSSavePanel.h>
#import <AppKit/NSPopUpButton.h>
#import <AppKit/NSTextField.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSEnumerator.h>
#import <Foundation/NSString.h>
#undef BOOL

static NSText *curr_editor = nil;

extern OP_STATUS AddTemporaryException(const OpString* exception);

// TODO: Set dialog prompt
// TODO: Memory leaks

@interface FileChooserDelegate : NSObject {
	id _dialog;
	DesktopFileChooserListener* _listener;
	NSArray* _deftypes;
	NSArray* _types;
	NSArray* _allowedtypes;
	int _selType;
	DesktopFileChooserRequest::Action _action;
}
- (id)initWithDialog:(id)dialog types:(NSArray*)types deftypes:(NSArray*)deftypes allowedtypes:(NSArray*)allowedtypes firstType:(int)selType action:(DesktopFileChooserRequest::Action)action andListener:(DesktopFileChooserListener*) listener;
@end

@implementation FileChooserDelegate
- (id)initWithDialog:(id)dialog types:(NSArray*)types deftypes:(NSArray*)deftypes allowedtypes:(NSArray*)allowedtypes firstType:(int)selType action:(DesktopFileChooserRequest::Action)action andListener:(DesktopFileChooserListener*) listener
{
	[super init];
	_dialog = dialog;
	_listener = listener;
	_types = types;
	_allowedtypes = allowedtypes;
	_deftypes = deftypes;
	_selType = selType;
	_action = action;
	return self;
}
- (void)panelDidEnd:(NSOpenPanel*)panel returnCode:(int)returnCode contextInfo:(void*)contextInfo
{
	DesktopFileChooserResult result;
	CocoaFileChooser* chooser = (CocoaFileChooser*)contextInfo;
	if (returnCode == NSOKButton) {
		NSString* file;
		if ([panel respondsToSelector:@selector(filenames)]) {
			NSEnumerator* numer = [[panel filenames] objectEnumerator];
			while ((file = [numer nextObject]) != nil) {
				OpString * sel = new OpString();
				SetOpString(*sel, (CFStringRef) file);
				AddTemporaryException(sel);
				OpFile file;
				OpFileInfo::Mode mode;
				file.Construct(sel->CStr(), OPFILE_ABSOLUTE_FOLDER, 0);
				if (!_action == DesktopFileChooserRequest::ACTION_CHOOSE_APPLICATION && ![panel canChooseDirectories] && OpStatus::IsSuccess(file.GetMode(mode)) && mode==OpFileInfo::DIRECTORY) {
					// bundle selected
					OpString8 sys, arch, src;
					char *path, *file;
					src.SetUTF8FromUTF16(sel->CStr());
					path = src.CStr();
					if (*path && path[strlen(path)-1] == '/')
						path[strlen(path)-1] = 0;
					file = strrchr(path, '/');
					if (file)
						*file++ = '\0';
					int t = 0;
					BOOL exists = TRUE;
					do {
						OpString path;
						path.Append(file);
						if (t)
							path.AppendFormat(UNI_L("-%d.zip"), t);
						else
							path.Append(UNI_L(".zip"));
						t++;
						OpFile destfile;
						destfile.Construct(path.CStr(), OPFILE_CACHE_FOLDER, 0);
						if (OpStatus::IsError(destfile.Exists(exists)))
							exists = TRUE;
						else if (!exists)
							arch.SetUTF8FromUTF16(destfile.GetFullPath());
					} while (exists);
					sys.AppendFormat("cd \"%s\"; zip -r \"%s\" \"%s\"", path, arch.CStr(), file);
					system(sys.CStr());
					sel->SetFromUTF8(arch);
				}
				result.files.Add(sel);
			}
		} else {
			file = [panel filename];
			OpString * sel = new OpString();
			SetOpString(*sel, (CFStringRef) file);
			result.files.Add(sel);
			AddTemporaryException(sel);
		}
		NSString* directory = [panel directory];
		SetOpString(result.active_directory, (CFStringRef) directory);
		result.selected_filter = _selType;
	}
	_listener->OnFileChoosingDone(chooser, result);
	_dialog = nil;
	_listener = NULL;
	curr_editor = nil;
	[self release];
}
- (void)popupChanged:(id)sender
{
	_selType = [((NSPopUpButton*)sender) indexOfSelectedItem];
	[_dialog setAllowedFileTypes:[_allowedtypes objectAtIndex:_selType]];
}
- (void)dealloc
{
	if (_dialog)
		[_dialog close];
	if(_deftypes)
		[_deftypes release];
	if(_types)
		[_types release];
	if(_allowedtypes)
		[_allowedtypes release];
	[super dealloc];
}
@end

OP_STATUS DesktopFileChooser::Create(DesktopFileChooser** new_object)
{
	DesktopFileChooser* obj = new CocoaFileChooser();
	*new_object = obj;
	return obj ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

CocoaFileChooser::CocoaFileChooser()
{
}

CocoaFileChooser::~CocoaFileChooser()
{
}

OP_STATUS CocoaFileChooser::Execute(OpWindow* parent, DesktopFileChooserListener* listener, const DesktopFileChooserRequest& request)
{
	id dialog = nil, delegate = nil;
	CocoaOpWindow* win = NULL;
	const uni_char* init_name = request.initial_path.CStr();
	init_name += request.initial_path.FindLastOf('/') + 1;
	NSMutableString *filename = init_name ? [[NSMutableString alloc] initWithCharacters:(const unichar*)init_name length:uni_strlen(init_name)] : nil;
	NSString *prompt = request.caption.CStr() ? [[NSString alloc] initWithCharacters:(const unichar*)request.caption.CStr() length:request.caption.Length()] : nil;
	NSMutableArray* deftypes = [[NSMutableArray alloc] initWithCapacity:0];
	NSMutableArray* types = [[NSMutableArray alloc] initWithCapacity:0];
	NSMutableArray* titles = [[NSMutableArray alloc] initWithCapacity:0];
	NSString *dir = nil;
	NSMutableArray* allowedtypes = [[NSMutableArray alloc] initWithCapacity:0];
	BOOL supportsZip = FALSE;
	if (parent)
		win = (CocoaOpWindow*) parent->GetRootWindow();
	const OpAutoVector<OpFileSelectionListener::MediaType> &media = request.extension_filters;
	for (unsigned int i = 0; i < media.GetCount(); i++) {
		const OpAutoVector<OpString> &exts = media.Get(i)->file_extensions;
		const OpString &desc = media.Get(i)->media_type;
		if (desc.Find(UNI_L("(*.*)")) != KNotFound)
		{
			if (request.action == DesktopFileChooserRequest::ACTION_FILE_OPEN ||
				request.action == DesktopFileChooserRequest::ACTION_FILE_OPEN_MULTI ||
#ifdef DESKTOP_FILECHOOSER_DIRFILE
				request.action == DesktopFileChooserRequest::ACTION_DIR_FILE_OPEN_MULTI ||
#endif
				request.action == DesktopFileChooserRequest::ACTION_DIRECTORY)
			{
				supportsZip = TRUE;
				[types release];
				types = nil;
				break;
			}
			else
				continue;	// We do not need this
		}
		if (desc.CStr()) {
			[titles addObject:[NSString stringWithCharacters:(const unichar*)desc.CStr() length:desc.Length()]];
		}
		NSMutableArray *allowedtypesForItem = [NSMutableArray arrayWithCapacity:0];

		for (unsigned int j = 0; j < exts.GetCount(); j++) {
			const OpString* ext = exts.Get(j);
			const uni_char* ext_str;
			if (ext && ((ext_str = ext->CStr()) != NULL)) {
				ext_str += (ext->FindLastOf('.') + 1);
				if (uni_strcmp(ext_str, UNI_L("zip")) == 0)
					supportsZip = TRUE;
				id string = [NSString stringWithCharacters:(const unichar*)ext_str length:uni_strlen(ext_str)];
				if ((j==0 && uni_strcmp(ext_str, UNI_L("htm"))) || !uni_strcmp(ext_str, UNI_L("html")))
				{
					[deftypes addObject:string];
					if (!uni_strcmp(ext_str, UNI_L("html")) && static_cast<unsigned int>(request.initial_filter) == i && [filename hasSuffix:@".htm"])
						[filename appendString:@"l"];
				}
				if (j==0)
					[allowedtypes addObject:allowedtypesForItem];
				if (!uni_strcmp(ext_str, UNI_L("html"))) {
					[allowedtypesForItem insertObject:string atIndex:0];
					[allowedtypesForItem addObject:@"xml"];
					[allowedtypesForItem addObject:@"xhtml"];
					[allowedtypesForItem addObject:@"xht"];
				}
				else
					[allowedtypesForItem addObject:string];
				[types addObject:string];
			}
		}
	}

	if(types && ![types count])
	{
		supportsZip = TRUE;
		[types release];
		types = nil;
	}
	id sheet_parent = win ? (id)win->GetNativeWindow() : nil;
	switch (request.action) {
		case DesktopFileChooserRequest::ACTION_CHOOSE_APPLICATION:
		case DesktopFileChooserRequest::ACTION_FILE_OPEN:
		case DesktopFileChooserRequest::ACTION_FILE_OPEN_MULTI:
#ifdef DESKTOP_FILECHOOSER_DIRFILE
		case DesktopFileChooserRequest::ACTION_DIR_FILE_OPEN_MULTI:
#endif
		case DesktopFileChooserRequest::ACTION_DIRECTORY:
			dialog = [[NSOpenPanel alloc] init];
			if (prompt)
				[dialog setPrompt:prompt];
			[dialog setCanChooseDirectories:
#ifdef DESKTOP_FILECHOOSER_DIRFILE
				request.action == DesktopFileChooserRequest::ACTION_DIR_FILE_OPEN_MULTI ||
#endif
				request.action == DesktopFileChooserRequest::ACTION_DIRECTORY];
			if (![dialog canChooseDirectories] && !supportsZip && request.action != DesktopFileChooserRequest::ACTION_CHOOSE_APPLICATION)
				[dialog setTreatsFilePackagesAsDirectories:YES];
			[dialog setCanChooseFiles:request.action != DesktopFileChooserRequest::ACTION_DIRECTORY];
			[dialog setAllowsMultipleSelection:request.action != DesktopFileChooserRequest::ACTION_FILE_OPEN &&
				request.action != DesktopFileChooserRequest::ACTION_DIRECTORY &&
				request.action != DesktopFileChooserRequest::ACTION_CHOOSE_APPLICATION];
			delegate = [[FileChooserDelegate alloc] initWithDialog:dialog types:types deftypes:deftypes allowedtypes:allowedtypes firstType:request.initial_filter action:request.action andListener:listener];

			if (request.action == DesktopFileChooserRequest::ACTION_CHOOSE_APPLICATION)
			{
				OpString path;
				if (OpFileUtils::FindFolder(kApplicationsFolderType, path, false, -1))
				{
					dir = [NSString stringWithCharacters:(unichar*)path.CStr() length:path.Length()];
				}
			}

			if (sheet_parent && !g_desktop_op_system_info->IsSandboxed())
				[dialog beginSheetForDirectory:dir file:nil types:types modalForWindow:sheet_parent modalDelegate:delegate didEndSelector:@selector(panelDidEnd:returnCode:contextInfo:) contextInfo:this];
			else
				[dialog beginForDirectory:dir file:nil types:types modelessDelegate:delegate didEndSelector:@selector(panelDidEnd:returnCode:contextInfo:) contextInfo:this];
			// this assigment must be avoided because of mac crash in sandbox env
			// bug to apple submited  Bug ID# 11263389
			if (!g_desktop_op_system_info->IsSandboxed())
				curr_editor = [dialog fieldEditor:YES forObject:nil];
			break;
		case DesktopFileChooserRequest::ACTION_FILE_SAVE:
		case DesktopFileChooserRequest::ACTION_FILE_SAVE_PROMPT_OVERWRITE:
			dialog = [[NSSavePanel alloc] init];
			if (prompt)
				[dialog setMessage:prompt];
			delegate = [[FileChooserDelegate alloc] initWithDialog:dialog types:types deftypes:deftypes allowedtypes:allowedtypes firstType:request.initial_filter>=0?request.initial_filter:0 action:request.action andListener:listener];
			if ([types count] > 1) {
				// This should ideally be done in a .nib, not done by hand.
				id view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 350, 26)];
				id button = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(80, 0, 200, 26) pullsDown:NO];
				id caption = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 5, 80, 17)];
				[view addSubview:button];
				[view addSubview:caption];
				[button release];
				[caption release];
				[caption setEditable:NO];
				[caption setDrawsBackground:NO];
				[caption setBordered:NO];
				
				OpString format_label;
				g_languageManager->GetString(Str::D_FILE_DIALOG_FORMAT, format_label);
				[caption setStringValue:[NSString stringWithCharacters:(const unichar *)format_label.CStr() length:format_label.Length()]];
				
				[button addItemsWithTitles:titles];
				[button setTarget:delegate];
				[button setAction:@selector(popupChanged:)];
				[button selectItemAtIndex:request.initial_filter];
				[dialog setAccessoryView:view];
				[view release];
				if (media.GetCount() > 1)
					[dialog setAllowedFileTypes:[allowedtypes objectAtIndex:request.initial_filter]];

			}
			[dialog setCanSelectHiddenExtension:YES];
			if (sheet_parent)
			{
				[dialog beginSheetForDirectory:nil file:filename modalForWindow:sheet_parent modalDelegate:delegate didEndSelector:@selector(panelDidEnd:returnCode:contextInfo:) contextInfo:this];
				// this assigment must be avoided because of mac crash in sandbox env
				// bug to apple submited  Bug ID# 11263389
				if (!g_desktop_op_system_info->IsSandboxed())
					curr_editor = [dialog fieldEditor:YES forObject:nil];
			}
			else
			{
				int result = [dialog runModalForDirectory:nil file:filename];
				[delegate panelDidEnd:dialog returnCode:result contextInfo:this];
			}
			break;
	}
	if (prompt)
		[prompt release];
	if (filename)
		[filename release];
	[titles release];
	return OpStatus::OK;
}

void CocoaFileChooser::Cancel()
{
	
}

/* static */
bool CocoaFileChooser::HandlingCutCopyPaste()
{
	return curr_editor != nil;
}

/* static */
void CocoaFileChooser::Cut()
{
	if (curr_editor)
		[curr_editor cut:nil];
}

/* static */
void CocoaFileChooser::Copy()
{
	if (curr_editor)
		[curr_editor copy:nil];
}

/* static */
void CocoaFileChooser::Paste()
{
	if (curr_editor)
		[curr_editor paste:nil];
}

/* static */
void CocoaFileChooser::SelectAll()
{
	if (curr_editor)
		[curr_editor selectAll:nil];
}

