/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 *
 */

#ifndef NEW_BOOKMARK_FOLDER_DIALOG_H
#define NEW_BOOKMARK_FOLDER_DIALOG_H

#include "adjunct/quick/dialogs/BookmarkDialog.h"

#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/OpEdit.h"

#include "adjunct/quick/managers/DesktopBookmarkManager.h"

/***********************************************************************************
 **
 **	NewBookmarkFolderDialog
 **
 ***********************************************************************************/

class NewBookmarkFolderDialog : public Dialog
{
public:

	Type	    GetType()		{ return DIALOG_TYPE_ASKTEXT; }
	const char* GetWindowName()	{ return "Ask Text Dialog"; }
	const char* GetDialogImage()	{ return "New folder"; }

	OP_STATUS Init(DesktopWindow* parent_window,
				   INT32 parent_id)
	{
		m_parent_id = parent_id;
		return Dialog::Init(parent_window);
	}

	void OnInit()
	{
		OpMultilineEdit* label = (OpMultilineEdit*)GetWidgetByName("Description_label");//label_for_Description_edit");

		if(label)
		{
			label->SetLabelMode();

			OpString bookmarks_translated;

			g_languageManager->GetString(Str::SI_IDSTR_HL_BOOKMARKS_ROOT_FOLDER_NAME, bookmarks_translated);
			const uni_char* OP_MEMORY_VAR parent_name = bookmarks_translated.CStr();

			HotlistModelItem* item = g_desktop_bookmark_manager->GetItemByID(m_parent_id);

			if (item)
			{
				parent_name = item->GetName().CStr();
			}

			OpString folder_name_question;
			g_languageManager->GetString(Str::D_BOOKMARKS_PROPERTIES_NAME_OF_NEW_FOLDER, folder_name_question);
			OpString text;
			text.AppendFormat(folder_name_question.CStr(), parent_name);
			label->SetText(text.CStr());
		}

		OpString new_folder_text;
		g_languageManager->GetString(Str::SI_NEW_FOLDER_BUTTON_TEXT, new_folder_text);

		SetWidgetText("Text_edit", new_folder_text.CStr());
		SetTitle(new_folder_text.CStr());
	}


	UINT32 OnOk()
	{
		OpString value;
		GetWidgetText("Text_edit", value);

		if (value.HasContent())
		{
			g_desktop_bookmark_manager->NewBookmarkFolder(value, m_parent_id);

		}

		return 0;
	}

private:

	INT32 m_parent_id;
};

#endif // NEW_BOOKMARK_FOLDER_DIALOG_H
