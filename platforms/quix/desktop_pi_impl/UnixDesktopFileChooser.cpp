/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "platforms/quix/desktop_pi_impl/UnixDesktopFileChooser.h"


#include "adjunct/desktop_util/file_chooser/file_chooser_fun.h" // StringFilterToExtensionFilter
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/OpLocale.h"
#include "modules/prefs/prefsmanager/collections/pc_unix.h"
#include "modules/util/opfile/opfile.h"
#include "platforms/quix/toolkits/ToolkitFileChooser.h"
#include "platforms/quix/toolkits/ToolkitLibrary.h"
#include "platforms/unix/base/common/unixutils.h"
#include "platforms/unix/base/x11/x11_dialog.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_opwindow.h"
#include "platforms/unix/base/x11/x11_windowwidget.h"
#include "platforms/unix/base/x11/x11_widgetlist.h"

namespace ConvertToToolkit
{
	ToolkitFileChooser::DialogType GetDialogType(DesktopFileChooserRequest::Action action);
	OP_STATUS Extensions(const OpVector<OpFileSelectionListener::MediaType>& extensions, const OpString& initial_path, BOOL fixed, ToolkitFileChooser* chooser);
};


UnixDesktopFileChooser::UnixDesktopFileChooser(ToolkitFileChooser* toolkit_chooser)
  : m_chooser(toolkit_chooser)
  , m_listener(0)
{
	OP_ASSERT(m_chooser);
}


UnixDesktopFileChooser::~UnixDesktopFileChooser()
{
	g_main_message_handler->UnsetCallBacks(this);
	m_chooser->Destroy();
}


OP_STATUS UnixDesktopFileChooser::Execute(OpWindow* parent, 
		DesktopFileChooserListener* listener, 
		const DesktopFileChooserRequest& request)
{
	X11Dialog::PrepareForShowingDialog(false);

	OP_ASSERT(listener);
	m_listener = listener;

	m_chooser->InitDialog();

	m_chooser->SetDialogType(ConvertToToolkit::GetDialogType(request.action));

	OpString8 caption;
	if (request.caption.HasContent())
	{
		RETURN_IF_ERROR(caption.SetUTF8FromUTF16(request.caption));
	}
	else
	{
		OpString caption_string;
		if (request.action == DesktopFileChooserRequest::ACTION_FILE_SAVE ||
			request.action == DesktopFileChooserRequest::ACTION_FILE_SAVE_PROMPT_OVERWRITE)
		{
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_SAVE_AS_CAPTION, caption_string));
		}
		else
		{
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_OPEN_FILE, caption_string));
		}
		RETURN_IF_ERROR(caption.SetUTF8FromUTF16(caption_string));
	}
	m_chooser->SetCaption(caption.CStr());

	// Strip quotes, if any, from the initial path
	OpString initial_path;
	int len = request.initial_path.Length();
	if (len > 1 && request.initial_path.CStr()[0] == '"' && request.initial_path.CStr()[len-1] == '"')
		RETURN_IF_ERROR(initial_path.Set(&request.initial_path.CStr()[1], len-2));
	else
		RETURN_IF_ERROR(initial_path.Set(request.initial_path));

	if (initial_path.HasContent())
	{
		OpString8 path;
		RETURN_IF_ERROR(path.SetUTF8FromUTF16(initial_path));
		m_chooser->SetInitialPath(path.CStr());
	}

	RETURN_IF_ERROR(ConvertToToolkit::Extensions(request.extension_filters, initial_path, request.fixed_filter, m_chooser));

	m_chooser->SetDefaultFilter(request.initial_filter);

	m_chooser->SetFixedExtensions(request.fixed_filter);

	m_chooser->ShowHiddenFiles(g_pcunix->GetIntegerPref(PrefsCollectionUnix::FileSelectorShowHiddenFiles));

	MH_PARAM_2 parent_id = 0;

	if (parent)
	{
		// GetNativeWindow() should not be used on something that is already a topplevel window
		// That will result in a grandfather parent in a dialog chain
		X11OpWindow* native_window = static_cast<X11OpWindow*>(parent);
		if (native_window && !native_window->IsToplevel())
		{
			native_window = static_cast<X11OpWindow*>(parent)->GetNativeWindow();
		}
		parent_id = native_window && native_window->GetOuterWidget() ? native_window->GetOuterWidget()->GetWindowHandle() : 0;
	}

	g_main_message_handler->SetCallBack(this, MSG_FILE_CHOOSER_SHOW, (MH_PARAM_1)this);
	g_main_message_handler->PostMessage(MSG_FILE_CHOOSER_SHOW, (MH_PARAM_1)this, parent_id);

	return OpStatus::OK;
}


ToolkitFileChooser::DialogType ConvertToToolkit::GetDialogType(DesktopFileChooserRequest::Action action)
{
	switch (action)
	{
		case DesktopFileChooserRequest::ACTION_FILE_OPEN: return ToolkitFileChooser::FILE_OPEN;
		case DesktopFileChooserRequest::ACTION_FILE_OPEN_MULTI: return ToolkitFileChooser::FILE_OPEN_MULTI;
		case DesktopFileChooserRequest::ACTION_FILE_SAVE: return ToolkitFileChooser::FILE_SAVE;
		case DesktopFileChooserRequest::ACTION_FILE_SAVE_PROMPT_OVERWRITE: return ToolkitFileChooser::FILE_SAVE_PROMPT_OVERWRITE;
		case DesktopFileChooserRequest::ACTION_DIRECTORY: return ToolkitFileChooser::DIRECTORY;
		default: OP_ASSERT(!"Unknown request type!"); return ToolkitFileChooser::FILE_OPEN;
	}
}


OP_STATUS ConvertToToolkit::Extensions(const OpVector<OpFileSelectionListener::MediaType>& extensions, const OpString& initial_path, BOOL fixed, ToolkitFileChooser* chooser)
{
	BOOL have_allfiles_item = FALSE;
	for (unsigned i = 0; !have_allfiles_item && i < extensions.GetCount(); i++)
	{
		OpFileSelectionListener::MediaType* media = extensions.Get(i);
		for (unsigned j = 0; j < media->file_extensions.GetCount(); j++)
		{
			OpString* s = media->file_extensions.Get(j);
			if (!s->Compare(UNI_L("*.*")) || !s->Compare(UNI_L("*")))
				have_allfiles_item = TRUE;
		}
	}

	int id = 0;

	// Start with the filter for the applied string if we only receive an "All Files" item
	if (extensions.GetCount() == 1 && initial_path.HasContent() && !fixed && have_allfiles_item)
	{
		const uni_char* p = uni_strrchr(initial_path.CStr(), '.');
		if (p && uni_strlen(p) > 1 && !uni_strpbrk(p, UNI_L("*/")))
		{
			// No '*' or '/' after dot. If there is no '/' in front of the dot we are fine
			// (name in /path/.name shall not be accepted as an extension)
			if (p == initial_path.CStr() || (p > initial_path.CStr() && p[-1] != '/'))
			{
				OpString msg, media, ext;
				g_languageManager->GetString(Str::D_FILEDIALOG_CUSTOM_FILTER_ITEM, msg);
				if (msg.HasContent())
				{
					RETURN_IF_ERROR(ext.Set(&p[1]));

					ext.MakeUpper();
					RETURN_IF_ERROR(media.AppendFormat(msg.CStr(), ext.CStr()));
					RETURN_IF_ERROR(ext.Set(&p[1]));
					RETURN_IF_ERROR(ext.Insert(0,UNI_L("*.")));

					OpString8 media_type, extension;
					RETURN_IF_ERROR(media_type.SetUTF8FromUTF16(media));
					RETURN_IF_ERROR(extension.SetUTF8FromUTF16(ext));

					chooser->AddFilter(id, media_type.CStr());
					chooser->AddExtension(id, extension.CStr());
					id++;
				}
			}
		}
	}

	for (unsigned i = 0; i < extensions.GetCount(); i++, id++)
	{
		OpFileSelectionListener::MediaType* media = extensions.Get(i);
		OpString8 media_type;
		RETURN_IF_ERROR(media_type.SetUTF8FromUTF16(media->media_type));
		chooser->AddFilter(id, media_type.CStr());

		for (unsigned j = 0; j < media->file_extensions.GetCount(); j++)
		{
			OpString8 extension;
			if (media->file_extensions.Get(j)->Compare(UNI_L("*.*")) == 0) // Special case on Unix
				RETURN_IF_ERROR(extension.Set("*"));
			else
				RETURN_IF_ERROR(extension.SetUTF8FromUTF16(*media->file_extensions.Get(j)));

			chooser->AddExtension(id, extension.CStr());
		}
	}

	// Add an "All Files" entry as the last item if provied filter did not do that
	if (!have_allfiles_item)
	{
		OpString msg;
		g_languageManager->GetString(Str::S_ALL_FILES_FILTER, msg);
		if (msg.HasContent())
		{
			OpAutoVector<OpFileSelectionListener::MediaType> extensions;
			RETURN_IF_ERROR(StringFilterToExtensionFilter(msg, &extensions));

			for (unsigned i = 0; i < extensions.GetCount(); i++, id++)
			{
				OpFileSelectionListener::MediaType* media = extensions.Get(i);

				OpString8 media_type;
				RETURN_IF_ERROR(media_type.SetUTF8FromUTF16(media->media_type));
				chooser->AddFilter(id, media_type.CStr());

				for (unsigned j = 0; j < media->file_extensions.GetCount(); j++)
				{
					OpString8 extension;

					if (media->file_extensions.Get(j)->Compare(UNI_L("*.*")) == 0) // Special case on Unix
						RETURN_IF_ERROR(extension.Set("*"));
					else
						RETURN_IF_ERROR(extension.SetUTF8FromUTF16(*media->file_extensions.Get(j)));
					chooser->AddExtension(id, extension.CStr());
				}
			}
		}
	}

	return OpStatus::OK;
}


void UnixDesktopFileChooser::Cancel()
{
	m_listener = 0;
	m_chooser->Cancel();
	g_main_message_handler->UnsetCallBacks(this);
}


bool UnixDesktopFileChooser::OnSaveAsConfirm(ToolkitFileChooser* chooser)
{
	int count = chooser->GetFileCount();
	if (count != 1)
		return false;

	OpString filename;
	if (OpStatus::IsError(filename.SetFromUTF8(chooser->GetFileName(0))))
		return false;

	OpFile file;
	if (OpStatus::IsError(file.Construct(filename)))
		return false;

	BOOL exists;
	if (OpStatus::IsError(file.Exists(exists)))
		return false;

	bool can_replace_file;
	if (!exists)
		can_replace_file = TRUE;
	else
	{
		X11Widget* w = g_x11->GetWidgetList()->GetModalToolkitParent();
		g_x11->GetWidgetList()->SetModalToolkitParent(0);
		can_replace_file = UnixUtils::PromptFilenameAlreadyExist(0, filename, TRUE);
		g_x11->GetWidgetList()->SetModalToolkitParent(w);
	}

	return can_replace_file;
}


void UnixDesktopFileChooser::OnChoosingDone(ToolkitFileChooser* chooser)
{
	if (!m_listener)
		return;

	DesktopFileChooserResult desktop_result;

	for (int i = 0; i < chooser->GetFileCount(); i++)
	{
		OpString* file = OP_NEW(OpString, ());
		file->SetFromUTF8(chooser->GetFileName(i));
		desktop_result.files.Add(file);
	}

	desktop_result.active_directory.SetFromUTF8(chooser->GetActiveDirectory());
	desktop_result.selected_filter = chooser->GetSelectedFilter();

	X11Widget* w = g_x11->GetWidgetList()->GetModalToolkitParent();
	g_x11->GetWidgetList()->SetModalToolkitParent(0);
	m_listener->OnFileChoosingDone(this, desktop_result);
	m_listener = 0;
	g_x11->GetWidgetList()->SetModalToolkitParent(w);
}

void UnixDesktopFileChooser::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(msg == MSG_FILE_CHOOSER_SHOW);
	if (msg == MSG_FILE_CHOOSER_SHOW)
	{
		X11Types::Window parent = static_cast<X11Types::Window>(par2);
		if (g_toolkit_library->BlockOperaInputOnDialogs())
			g_x11->GetWidgetList()->SetModalToolkitParent(g_x11->GetWidgetList()->FindWidget(parent));
		g_x11->GetWidgetList()->SetCursor(CURSOR_DEFAULT_ARROW);

		g_main_message_handler->UnsetCallBacks(this);
		m_chooser->OpenDialog(parent, this);

		g_x11->GetWidgetList()->SetModalToolkitParent(0);
		g_x11->GetWidgetList()->RestoreCursor();
	}
}
