/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 * 
  */
#include "core/pch.h"

#include "adjunct/quick/widgets/DesktopFileChooserEdit.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_util/actions/action_utils.h"
#include "adjunct/desktop_util/file_chooser/file_chooser_fun.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpEdit.h"

OP_STATUS DesktopFileChooserEdit::Construct(DesktopFileChooserEdit** obj, SelectorMode mode)
{
	*obj = OP_NEW(DesktopFileChooserEdit, (mode));
	if (!*obj)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS rc = (*obj)->init_status;
	if (OpStatus::IsError(rc))
	{
		OP_DELETE(*obj);
		return rc;
	}
	return OpStatus::OK;
}

DesktopFileChooserEdit::DesktopFileChooserEdit(SelectorMode mode)
	: OpFileChooserEdit()
	, m_selector_mode(mode)
	, m_chooser(NULL)
{
}

DesktopFileChooserEdit::~DesktopFileChooserEdit()
{
	OP_DELETE(m_chooser);
}

BOOL DesktopFileChooserEdit::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_OPEN_FOLDER:
				{
					OpString filename;
					GetText(filename);
					BOOL exists = FALSE;
					if (filename.HasContent())
					{
						OpFile file;
						if (OpStatus::IsSuccess(file.Construct(filename.CStr())))
						{
							if (OpStatus::IsError(file.Exists(exists)))
							{
								exists = FALSE;
							}
						}
					}
					child_action->SetEnabled(exists);
					return TRUE;
				}
			}
		}
		break;

		case OpInputAction::ACTION_OPEN_FOLDER:
		{
			OpString filename;
			GetText(filename);
			BOOL treat_folder_as_file = (m_selector_mode != FolderSelector);
			((DesktopOpSystemInfo*)g_op_system_info)->OpenFileFolder(filename, treat_folder_as_file);
			return TRUE;
		}
		break;
	}
	return OpWidget::OnInputAction(action);
}

void DesktopFileChooserEdit::GetPreferedSize(INT32* w, INT32* h,
		INT32 cols, INT32 rows)
{
	INT32 edit_width = 0;
	INT32 edit_height = 0;
	GetEdit()->GetPreferedSize(&edit_width, &edit_height, cols, rows);

	INT32 button_width = 0;
	INT32 button_height = 0;
	GetButton()->GetPreferedSize(&button_width, &button_height, cols, rows);

	*w = edit_width + button_width;
	*h = max(edit_height, button_height);
}

void DesktopFileChooserEdit::OnLayout()
{
	// OpFileChooserEdit::OnResize() does the actual layout, and it accounts
	// for UI direction.  Must call it from OnLayout(), because the direction
	// is not set yet when OnResize() is called.
	INT32 width = rect.width;
	INT32 height = rect.height;
	OnResize(&width, &height);
	OpFileChooserEdit::OnLayout();
}

void DesktopFileChooserEdit::OnClick(OpWidget *object, UINT32 id)
{
	if (object != GetButton())
	   return;

	OpWindow* parentwindow = GetParentOpWindow();

	if (!m_chooser)
		RETURN_VOID_IF_ERROR(DesktopFileChooser::Create(&m_chooser));

	// Use directory specified in the edit field as start point for the folder chooser
	// The validity of the path should be checked by the underlying folder chooser

	RETURN_VOID_IF_ERROR(GetText(m_request.initial_path));
	if (m_request.initial_path.IsEmpty())
		RETURN_VOID_IF_ERROR(m_request.initial_path.Set(m_initial_path));

	static_cast<DesktopOpSystemInfo*>(g_op_system_info)->RemoveIllegalPathCharacters(m_request.initial_path, FALSE);
	if (m_request.initial_path.IsEmpty())
	{
		// Fallback. We assume g_folder_manager does not provide paths with illegal characters
		RETURN_VOID_IF_ERROR(g_folder_manager->GetFolderPath(m_selector_mode == SaveSelector ? OPFILE_SAVE_FOLDER : OPFILE_OPEN_FOLDER, m_request.initial_path));
	}

	RETURN_VOID_IF_ERROR(m_request.caption.Set( m_title.CStr() ));

	if (m_selector_mode == FolderSelector)
	{
		if (m_request.caption.IsEmpty())
		{
			g_languageManager->GetString(Str::D_BROWSE_FOR_FOLDER, m_request.caption);
		}
		m_request.action = DesktopFileChooserRequest::ACTION_DIRECTORY;
	}
	else
	{
		if (m_filter_string.HasContent())
			RETURN_VOID_IF_ERROR(StringFilterToExtensionFilter(m_filter_string.CStr(), &m_request.extension_filters));

		if (m_selector_mode == OpenSelector)
			m_request.action = DesktopFileChooserRequest::ACTION_FILE_OPEN;
		else
			m_request.action = DesktopFileChooserRequest::ACTION_FILE_SAVE;
	}

	m_chooser->Execute(parentwindow, this, m_request);
}


BOOL DesktopFileChooserEdit::OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked)
{
	OP_ASSERT(widget->GetParent() == this);

	if (widget->GetParent() == this)
	{
		// the widget is the edit field inside the file chooser.
		// handle a file chooser specific menu, rather than a generic edit field menu
		return HandleWidgetContextMenu(this, menu_point);
	}
	else
	{
		return HandleWidgetContextMenu(widget, menu_point);
	}
}


void DesktopFileChooserEdit::OnFileChoosingDone(DesktopFileChooser *chooser, const DesktopFileChooserResult& result)
{
	const OpVector<OpString>& files = result.files;
	if (files.GetCount() == 0) // Dialog has typically been canceled
		return;
	
	if (files.GetCount() > 1)
	{
		for (UINT32 i = 0; i < files.GetCount(); i++)
		{
			if (files.Get(i) && files.Get(i)->HasContent())
			{
				// Quote the string
				OpString quoted_file_name;
				RETURN_VOID_IF_ERROR(quoted_file_name.AppendFormat(UNI_L("\"%s\""), files.Get(i)->CStr()));
				if (GetMaxNumberOfFiles() > 1)
					AppendFileName(quoted_file_name.CStr());
				else
					RETURN_VOID_IF_ERROR(SetText(quoted_file_name.CStr()));
			}
		}
	}
	else
	{
		RETURN_VOID_IF_ERROR(SetText(*files.Get(0)));
	}

	if (listener)
		listener->OnChange(this, FALSE);

	FileChooserUtil::SavePreferencePath(m_request, result);
}
