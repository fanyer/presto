/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/display/vis_dev.h"
#include "modules/dochand/win.h"
#include "modules/forms/piforms.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/viewers/viewers.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpFileChooserEdit.h"
#include "modules/locale/locale-enum.h"

#ifdef _FILE_UPLOAD_SUPPORT_
# include "modules/forms/formmanager.h"
# include "modules/formats/argsplit.h"
#ifdef WIDGETS_HIDE_FILENAMES_IN_FILE_CHOOSER
# include "modules/util/opfile/opfile.h"
#endif // WIDGETS_HIDE_FILENAMES_IN_FILE_CHOOSER
#endif // _FILE_UPLOAD_SUPPORT_

#ifdef _FILE_UPLOAD_SUPPORT_

/**
 * Gathers all matching extensions. Duplicates will occur.
 */
OP_STATUS AppendExtensions(OpAutoVector<OpString> &extensions, const uni_char* mime_type_str, UINT32 len)
{
	while (len > 0 && mime_type_str[len-1] == '*')
		len--; // remove trailing *

	if (len == 0)
		return OpStatus::OK;

	// Run through the viewers and append file extensions if we match the MIME type.
	ChainedHashIterator *viewer_iterator = NULL;
	OP_STATUS rc = g_viewers->CreateIterator(viewer_iterator);
	if (rc == OpStatus::ERR)
		return OpStatus::OK; // no viewers available
	RETURN_IF_ERROR(rc);
	ANCHOR_PTR(ChainedHashIterator, viewer_iterator);

	while (Viewer* viewer = g_viewers->GetNextViewer(viewer_iterator))
	{
		if (uni_strnicmp(mime_type_str, viewer->GetContentTypeString(), len) == 0)
		{
			const uni_char* viewer_exts = viewer->GetExtensions();

			while (viewer_exts)
			{
				int viewer_exts_len = 0;
				while (viewer_exts[viewer_exts_len] != '\0' &&
					viewer_exts[viewer_exts_len] != ',')
				{
					viewer_exts_len++;
				}

				if (viewer_exts_len > 0)
				{
					unsigned int insert_before;
					int last_diff = 1;

					for (insert_before = 0; insert_before < extensions.GetCount(); ++insert_before)
					{
						OpString* str = extensions.Get(insert_before);
						int this_len = str->Length();

						last_diff = uni_strncmp(str->CStr(), viewer_exts, MIN(this_len, viewer_exts_len));

						if (last_diff == 0 && viewer_exts_len != this_len)
							last_diff = this_len < viewer_exts_len ? -1 : 1;

						if (last_diff >= 0)
							break;
					}

					if (last_diff != 0)
					{
						OpString* str = OP_NEW(OpString, ());

						if (!str)
							return OpStatus::ERR_NO_MEMORY;

						OP_STATUS res;

						if (insert_before < extensions.GetCount())
							res = extensions.Insert(insert_before, str);
						else
							res = extensions.Add(str);

						if (OpStatus::IsError(res))
						{
							OP_DELETE(str);
							return res;
						}

						RETURN_IF_ERROR(str->Set(viewer_exts, viewer_exts_len));
					}
				}

				if (viewer_exts[viewer_exts_len] == '\0')
					break; // finished with this mime type

				viewer_exts += viewer_exts_len + 1;
			}
		}
	}

	return OpStatus::OK;
}

#endif // _FILE_UPLOAD_SUPPORT_

static inline WindowCommander* GetWindowCommander(VisualDevice* vis_dev)
{
	if (vis_dev)
	{
		Window* w = vis_dev->GetWindow();

		if (w)
			return w->GetWindowCommander();
	}

	return 0;
}

// == OpFileChooserEdit ===========================================================

OP_STATUS OpFileChooserEdit::Construct(OpFileChooserEdit** obj)
{
	*obj = OP_NEW(OpFileChooserEdit, ());
	if (!*obj)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

OpFileChooserEdit::OpFileChooserEdit()
	: button(NULL)
	, edit(NULL)
# ifdef _FILE_UPLOAD_SUPPORT_
	, m_media_types(NULL)
	, m_is_upload(FALSE)
	, m_max_no_of_files(1)
	, m_callback(NULL)
# endif // _FILE_UPLOAD_SUPPORT_
{
	OP_STATUS status;

	status = OpEdit::Construct(&edit);
	CHECK_STATUS(status);
	AddChild(edit, TRUE);
	edit->SetForceTextLTR(TRUE);

	status = OpButton::Construct(&button);
	CHECK_STATUS(status);
	AddChild(button, TRUE);
	// Center text in button and use TRUE so forms can't change it back again.
	button->SetJustify(JUSTIFY_CENTER, TRUE);

	// now everybody is supposed to use the GUI for manipulation of paths on web pages
	// see OnAdded() for re-enabling it in native UI
	edit->SetReadOnly(TRUE);
	edit->SetForceAllowClear(TRUE);

# ifdef WIDGETS_FILE_UPLOAD_IMAGE
	button->SetText(UNI_L("-"));
	button->GetForegroundSkin()->SetImage(UNI_L("File upload"));
	button->SetButtonStyle(OpButton::STYLE_IMAGE);
# else // WIDGETS_FILE_UPLOAD_IMAGE
	OpString browse;
	TRAP(status, g_languageManager->GetStringL(Str::SI_BROWSE_TEXT, browse));
	init_status = status;

	button->SetText(browse.CStr());
# endif // WIDGETS_FILE_UPLOAD_IMAGE

	button->SetListener(this);
	edit->SetListener(this);
}

OpFileChooserEdit::~OpFileChooserEdit()
{
# ifdef _FILE_UPLOAD_SUPPORT_
	if (m_callback)
		m_callback->Closing(this);
	OP_DELETE(m_media_types);
# endif // _FILE_UPLOAD_SUPPORT_
}

#ifdef WIDGETS_CLONE_SUPPORT

OP_STATUS OpFileChooserEdit::CreateClone(OpWidget **obj, OpWidget *parent, INT32 font_size, BOOL expanded)
{
	*obj = NULL;
	OpFileChooserEdit *widget;

	RETURN_IF_ERROR(OpFileChooserEdit::Construct(&widget));
	parent->AddChild(widget);

	if (OpStatus::IsError(widget->CloneProperties(this, font_size)))
	{
		widget->Remove();
		OP_DELETE(widget);
		return OpStatus::ERR;
	}
	*obj = widget;
	return OpStatus::OK;
}

#endif // WIDGETS_CLONE_SUPPORT

INT32 OpFileChooserEdit::GetButtonWidth()
{
	INT32 button_width = 0;
	INT32 button_height = 0;
	button->GetRequiredSize(button_width, button_height);
	return button_width;
}


void OpFileChooserEdit::OnResize(INT32* new_w, INT32* new_h)
{
	INT32 button_width = GetButtonWidth();
	int width = *new_w;
	int height = *new_h;
	int spacing = 2;
	int edit_width = width - button_width - spacing;

	if (GetRTL())
	{
		button->SetRect(OpRect(0, 0, button_width, height));
		edit->SetRect(OpRect(button_width + spacing, 0, edit_width, height));
	}
	else
	{
		edit->SetRect(OpRect(0, 0, edit_width, height));
		button->SetRect(OpRect(edit_width + spacing, 0, button_width, height));
	}
}

void OpFileChooserEdit::OnFocus(BOOL focus, FOCUS_REASON reason)
{
# ifdef _FILE_UPLOAD_SUPPORT_
	// *** SECURITY ***
	// This mustn't be changed since it currently prevents the following:
	// 1. User pressing key
	// 2. Page setting focus to file upload in keydown/up/press handler
	// 3. Character appearing in file upload
	// By doing the operation above selectively and by restoring focus a page
	// can enter any file name if only the user writes those letters.
	if (focus)
	{
		button->SetFocus(reason);
	}
# endif // _FILE_UPLOAD_SUPPORT_
}

void OpFileChooserEdit::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	edit->SetHasCssBackground(HasCssBackgroundImage());
	//button->SetHasCssBackground(HasCssBackgroundImage());
	edit->SetHasCssBorder(HasCssBorder());
	button->SetHasCssBorder(HasCssBorder());
	edit->SetPadding(GetPaddingLeft(), GetPaddingTop(), GetPaddingRight(), GetPaddingBottom());
	button->SetPadding(GetPaddingLeft(), GetPaddingTop(), GetPaddingRight(), GetPaddingBottom());

	// We don't want any style to affect the button
	button->UnsetForegroundColor();
	button->UnsetBackgroundColor();
}

void OpFileChooserEdit::EndChangeProperties()
{
	// propagate background color to edit field
	if (!m_color.use_default_background_color)
		edit->SetBackgroundColor(m_color.background_color);
}

# ifdef _FILE_UPLOAD_SUPPORT_

OP_STATUS OpFileChooserEdit::SetButtonText(const uni_char* text)
{
	return button->SetText(text);
}

OP_STATUS OpFileChooserEdit::SetText(const uni_char* text)
{
#ifdef WIDGETS_HIDE_FILENAMES_IN_FILE_CHOOSER
	OpString absolute_path;
	RETURN_IF_ERROR(absolute_path.Set(text));
	UniParameterList filelist;
	RETURN_IF_ERROR(FormManager::ConfigureForFileSplit(filelist, absolute_path));

	const uni_char* sep = UNI_L("; %s");
	OpString locPaths;
	UniParameters* file = filelist.First();
	while (file)
	{
		if (const uni_char* file_name = file->Name())
		{
			// try  to get the localized path
			OpFile opfile;
			RETURN_IF_ERROR(opfile.Construct(file_name));
			// GetLocalizedPath may return OpStatus::ERR for
			// non-localizable paths. such errors should not be
			// reported, instead absolute path is used.
			OpString localizedPath;
			OP_STATUS status = opfile.GetLocalizedPath(&localizedPath);
			if (OpStatus::IsSuccess(status))
				file_name = localizedPath.CStr();
			else
				RETURN_IF_MEMORY_ERROR(status);

			// append path
			if (locPaths.HasContent())
				status = locPaths.AppendFormat(sep, file_name);
			else
				status = locPaths.Append(file_name);
			RETURN_IF_ERROR(status);
		}
		file = file->Suc();
	}

	RETURN_IF_ERROR(edit->SetText(locPaths));
	m_path.TakeOver(absolute_path);
	return OpStatus::OK;
#else // WIDGETS_HIDE_FILENAMES_IN_FILE_CHOOSER
	return edit->SetText(text);
#endif //WIDGETS_HIDE_FILENAMES_IN_FILE_CHOOSER
}

INT32 OpFileChooserEdit::GetTextLength()
{
#ifdef WIDGETS_HIDE_FILENAMES_IN_FILE_CHOOSER
	return m_path.Length();
#else // WIDGETS_HIDE_FILENAMES_IN_FILE_CHOOSER
	return edit->GetTextLength();
#endif // WIDGETS_HIDE_FILENAMES_IN_FILE_CHOOSER
}

OP_STATUS OpFileChooserEdit::GetText(OpString &str)
{
#ifdef WIDGETS_HIDE_FILENAMES_IN_FILE_CHOOSER
	return str.Set(m_path);
#else // WIDGETS_HIDE_FILENAMES_IN_FILE_CHOOSER
	return edit ? edit->GetText(str) : OpStatus::ERR_NULL_POINTER;
#endif // WIDGETS_HIDE_FILENAMES_IN_FILE_CHOOSER
}

void OpFileChooserEdit::GetText(uni_char *buf)
{
#ifdef WIDGETS_HIDE_FILENAMES_IN_FILE_CHOOSER
	uni_strcpy(buf, m_path.CStr());
#else // WIDGETS_HIDE_FILENAMES_IN_FILE_CHOOSER
	edit->GetText(buf, GetTextLength(), 0);
#endif // WIDGETS_HIDE_FILENAMES_IN_FILE_CHOOSER
}

void OpFileChooserEdit::SetIsFileUpload(BOOL is_upload)
{
	m_is_upload = is_upload;
	SetEnabled(IsEnabled());
}

void OpFileChooserEdit::SetMaxNumberOfFiles(unsigned int max_count)
{
	Str::LocaleString id = Str::NOT_A_STRING;
	if (max_count > 1)
	{
		if (m_max_no_of_files <= 1)
		{
			// Need to change button text
			// Set "Add" text on button
			id = Str::D_ADD_FILE_BUTTON_TEXT;
		}
	}
	// else we want a multi file widget
	else if (m_max_no_of_files > 1)
	{
		// Need to change button text
		// Set "Browse" text on button
		id = Str::SI_BROWSE_TEXT;
	}
	m_max_no_of_files = max_count;

	if (id != Str::NOT_A_STRING)
	{
		OpString label;
		if (OpStatus::IsSuccess(g_languageManager->GetString(id, label)))
		{
			button->SetText(label.CStr());
		}
	}
}

void OpFileChooserEdit::SetEnabled(BOOL enabled)
{
	OpWidget::SetEnabled(enabled);
	if (m_is_upload)
	{
		WindowCommander* wc = GetWindowCommander(GetVisualDevice());

		if (wc && !wc->GetFileSelectionListener()->OnRequestPermission(wc))
			enabled = FALSE;

		if(button)
			button->SetEnabled(enabled);
	}
}

void OpFileChooserEdit::OnAdded()
{
	if (!IsForm())
	{
		edit->SetReadOnly(FALSE);
	}
}

void OpFileChooserEdit::OnChangeWhenLostFocus(OpWidget *widget)
{
	if (listener && widget == edit)
		listener->OnChange(this, FALSE);
}

void OpFileChooserEdit::OnClick()
{
	WindowCommander* wc = GetWindowCommander(GetVisualDevice());

	if (!wc || !GetFormObject() || m_callback)
	   return;

	OP_DELETE(m_media_types);
	m_media_types = 0;

	OpFileChooserEditCallback* callback = OP_NEW(OpFileChooserEditCallback, (this));

	if (!callback)
	{
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return;
	}

	m_callback = callback;
	wc->GetFileSelectionListener()->OnUploadFilesRequest(wc, callback);
}

void OpFileChooserEdit::OnGeneratedClick(OpWidget *widget, UINT32 id)
{
	if (widget == button)
	{
		// Called on keypresses or actions
		OnClick();
	}
}

void OpFileChooserEdit::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (listener && widget == edit)
		listener->OnChange(this, changed_by_mouse);
}

void OpFileChooserEdit::OnFilesSelected(const OpVector<OpString> *files)
{
	const uni_char quote = '"';
	if (files->GetCount() > 1)
	{
		for (unsigned int i = 0; i < files->GetCount(); i++)
		{
			if (files->Get(i) && files->Get(i)->HasContent()) // XXX Is this necessary?
			{
				// Quote the string
				OpString quoted_file_name;
				if (OpStatus::IsError(quoted_file_name.AppendFormat(UNI_L("\"%s\""), (files->Get(i))->CStr())))
					return;
				if (m_max_no_of_files > 1)
				{
					// Append the file
					AppendFileName(quoted_file_name.CStr());
				}
				else if (OpStatus::IsError(SetText(quoted_file_name.CStr())))
				{
					return;
				}
			}
		}
	}
	else
	{
		if( IsInWebForm() )
		{
			OpString tmp;
			RETURN_VOID_IF_ERROR(tmp.Set(files->Get(0)->CStr()));
			tmp.Strip();

			int length = tmp.Length();

			// See if we need to quote the string
			if( length > 0 && tmp.CStr()[0] != quote && tmp.CStr()[length-1] != quote )
			{
				OpString tmp2;
				RETURN_VOID_IF_ERROR(tmp2.AppendFormat(UNI_L("\"%s\""), tmp.CStr()));
				RETURN_VOID_IF_ERROR(tmp.Set( tmp2.CStr() ));
			}
			SetText(tmp.CStr());
		}
		else
		{
			SetText(files->Get(0)->CStr());
		}
	}

	if (listener)
	{
		listener->OnChange(this, FALSE);
	}
}


void OpFileChooserEdit::AppendFileName(const uni_char* new_file_name)
{
	OpString contents;
	OP_STATUS status = GetText(contents);
	if (OpStatus::IsSuccess(status) && !contents.IsEmpty())
	{
		status = contents.Append(";"); // file delimiter
		if (OpStatus::IsSuccess(status))
		{
			status = contents.Append(new_file_name);
		}
	}
	else
	{
		// We didn't get an existing file name
		status = contents.Set(new_file_name);
	}

	if (OpStatus::IsSuccess(status))
	{
		SetText(contents.CStr());
	}
}

OP_STATUS OpFileChooserEdit::InitializeMediaTypes()
{
	if (m_media_types)
		return OpStatus::OK;

	HTML_Element* helm = GetFormObject()->GetHTML_Element();
	OP_ASSERT(helm);

	const uni_char* accept = helm->GetStringAttr(ATTR_ACCEPT);

	m_media_types = OP_NEW(OpAutoVector<OpFileSelectionListener::MediaType>, ());

	if (!m_media_types)
		return OpStatus::ERR_NO_MEMORY;

	if (accept)
	{
		const uni_char* str = accept;

		while (TRUE)
		{
			int len = 0;
			while(str[len] != ',' && str[len] != 0)
				len++;

			OpFileSelectionListener::MediaType* media_type = OP_NEW(OpFileSelectionListener::MediaType, ());
			if (!media_type)
				return OpStatus::ERR_NO_MEMORY;

			RETURN_IF_ERROR(m_media_types->Add(media_type));
			RETURN_IF_ERROR(media_type->media_type.Set(str, len));

			// Add the extensions
			RETURN_IF_ERROR(AppendExtensions(media_type->file_extensions, str, len));

			if (str[len] == '\0')
				break;

			str += len + 1;
			len = 0;
		}
	}

	return OpStatus::OK;
}

const OpFileSelectionListener::MediaType* OpFileChooserEdit::GetMediaType(unsigned int index)
{
	if (!m_media_types)
	{
		OP_STATUS res = InitializeMediaTypes();

		if (OpStatus::IsError(res))
		{
			OP_DELETE(m_media_types);
			m_media_types = 0;
			g_memory_manager->RaiseCondition(res);
			return 0;
		}
	}

	if (index < m_media_types->GetCount())
		return m_media_types->Get(index);
	else
		return 0;
}

unsigned int OpFileChooserEditCallback::GetMaxFileCount()
{
	if (m_fc_edit)
		return MAX(1, m_fc_edit->m_max_no_of_files);
	else
		return 1;
}

const uni_char* OpFileChooserEditCallback::GetInitialPath()
{
	if (m_fc_edit)
	{
		if (OpStatus::IsError(m_fc_edit->GetText(m_initial_path)))
			return NULL;

		return m_initial_path.CStr();
	}
	else
		return NULL;
}

const uni_char* GetAcceptAttribute(HTML_Element * helm)
{
	OP_ASSERT(helm);
	const uni_char* accept = helm->GetStringAttr(ATTR_ACCEPT);
	
	// If we have an accept attribute on the forms element, that
	// will function as a fallback
	if (!accept)
	{
		FramesDocument* frames_doc = NULL; // XXX Must get a real document
		HTML_Element* form_elm = FormManager::FindFormElm(frames_doc, helm);
		if (form_elm)
		{
			accept = form_elm->GetStringAttr(ATTR_ACCEPT);
		}
	}
	return accept;
}

const OpFileSelectionListener::MediaType* OpFileChooserEditCallback::GetMediaType(unsigned int index)
{
	OpFileSelectionListener::MediaType media_type;

	if (m_fc_edit)
		return m_fc_edit->GetMediaType(index);
	else
		return 0;
}

void OpFileChooserEditCallback::OnFilesSelected(const OpAutoVector<OpString>* files)
{
	if (files && files->GetCount() > 0)
	{
		if (m_fc_edit && files->GetCount())
			m_fc_edit->OnFilesSelected(files);
	}

	if (m_fc_edit)
		m_fc_edit->ResetFileChooserCallback();

	OP_DELETE(this);
}

# endif // _FILE_UPLOAD_SUPPORT_
