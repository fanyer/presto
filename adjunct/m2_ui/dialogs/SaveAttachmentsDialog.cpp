/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2_ui/dialogs/SaveAttachmentsDialog.h"

#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick/widgets/DesktopFileChooserEdit.h"
#include "modules/util/filefun.h"
#include "modules/locale/oplanguagemanager.h"
#include "adjunct/desktop_util/adt/hashiterator.h"
#include "modules/webserver/webserver_common.h"

// initialize flag to true, one time every opera session
BOOL SaveAttachmentsDialog::m_new_save_session = true;

SaveAttachmentsDialog::SaveAttachmentsDialog(OpAutoVector<URL>* attachmentlist)
: m_attachments_model(1),
  m_tree_view(NULL),
  m_attachments(attachmentlist)
{
}

SaveAttachmentsDialog::~SaveAttachmentsDialog()
{
}

void SaveAttachmentsDialog::Init(DesktopWindow* parent_window)
{
	Dialog::Init(parent_window);
}

void SaveAttachmentsDialog::OnInit()
{
	m_tree_view = (OpTreeView*)GetWidgetByName("Attachement_treeview");

	if (m_tree_view)
	{
		OpString attch_str;
		g_languageManager->GetString(Str::D_MAILER_SAVE_ATTATCHMENTS_HEADER, attch_str);
		m_attachments_model.SetColumnData(0, attch_str.CStr());
		m_tree_view->SetTreeModel(&m_attachments_model);
		m_tree_view->SetCheckableColumn(0);
	}

	// use default download folder as start folder when opera is restarted (last used directory is remember afterwards)
	/* Only one of the GetFolderPathIgnoreErrors() calls will be
	 * executed, so it is safe to use the same storage object for
	 * both.
	 */
	OpString tmp_storage;
	const uni_char * safetodir_path = 0;
	if (m_new_save_session)
		safetodir_path = g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_DOWNLOAD_FOLDER, tmp_storage);
	else
		safetodir_path = g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_SAVE_FOLDER, tmp_storage);
	SetWidgetText("Safetodir_chooser", safetodir_path);

	DesktopFileChooserEdit* chooser = static_cast<DesktopFileChooserEdit*>(GetWidgetByName("Safetodir_chooser"));
	if (chooser)
	{
		OpString caption_string;
		g_languageManager->GetString(Str::D_MAILER_SAVE_ATTACHMENTS, caption_string);
		chooser->SetTitle(caption_string.CStr());
	}

	for(unsigned int i = 0; i < m_attachments->GetCount(); i++)
	{
		URL* attachment_url = m_attachments->Get(i);

		OpString suggested_filename;
		TRAPD(err, attachment_url->GetAttributeL(URL::KSuggestedFileName_L, suggested_filename, TRUE));
		RETURN_VOID_IF_ERROR(err);

		m_attachments_model.AddItem(suggested_filename.CStr(), NULL, 0, -1, (void*)attachment_url, 0, OpTypedObject::UNKNOWN_TYPE, TRUE);
	}
}

UINT32 SaveAttachmentsDialog::OnOk()
{
	OP_STATUS ret;
	// ignoring all errors
	TRAP_AND_RETURN_VALUE_IF_ERROR(ret, OnOkL(), 0);
	return 0;
}

void SaveAttachmentsDialog::OnOkL()
{
	//first check if the target directory exist
	OpString directory;
 	int seppos;

	GetWidgetText("Safetodir_chooser", directory);

	// Add a trailing slash if it's not there
	seppos = directory.FindLastOf(PATHSEPCHAR);
	if(seppos != KNotFound)
	{
		if(seppos + 1 != directory.Length())
		{
			directory.AppendL(PATHSEP);
		}
	}

	BOOL 	exists = FALSE;
	OpFile 	file;

	LEAVE_IF_ERROR(file.Construct(directory.CStr()));
	LEAVE_IF_ERROR(file.Exists(exists));

	if(!exists)
	{
		OpString message, title;

		g_languageManager->GetStringL(Str::D_SAVEDIRECTORY_NOT_FOUND, message);
		g_languageManager->GetStringL(Str::D_SAVEDIRECTORY_NOT_FOUND_TITLE, title);

		INT32 result = SimpleDialog::ShowDialogFormatted(WINDOW_NAME_DIR_NOT_FOUND, 0, message.CStr(), title.CStr(), Dialog::TYPE_YES_NO, Dialog::IMAGE_QUESTION, NULL, directory.CStr());

		if (result != Dialog::DIALOG_RESULT_YES)
			return;

		// Do not need to create the folder here as the OpFile call below does it.
	}

	// create the file names for saving attachments
	OpAutoStringVector file_names;
	OpStringHashTable<URL> attachment_file_names;
	BOOL is_found_existing_file_name = FALSE;

	for (INT32 i = 0, count = m_tree_view->GetItemCount(); i < count; ++i)
	{
		if (!m_tree_view->IsItemChecked(i))
			continue;

		URL* attachment_url = (URL *)(m_attachments_model.GetItemUserData(i));

		OpString suggested_filename;
		attachment_url->GetAttributeL(URL::KSuggestedFileName_L, suggested_filename, TRUE);

		OpAutoPtr<OpString> full_file_name(OP_NEW_L(OpString, ()));
		full_file_name->SetL(directory);
		full_file_name->AppendL(suggested_filename);

		OpString suggested_filename_name, suggested_filename_extension;
		SplitFilenameIntoComponentsL(suggested_filename, NULL, &suggested_filename_name, &suggested_filename_extension);

		// create a unique name, because sometimes we can have several attachments with exactly the same name
		int duplicate_num = 0;
		while (attachment_file_names.Contains(full_file_name->CStr()))
		{
			full_file_name->SetL(directory);
			LEAVE_IF_ERROR(full_file_name->AppendFormat(UNI_L("%s (%d)"), suggested_filename_name.CStr(), ++duplicate_num));
			if (suggested_filename_extension.HasContent())
				LEAVE_IF_ERROR(full_file_name->AppendFormat(UNI_L(".%s"), suggested_filename_extension.CStr()));
		}

		// check if the file exists
		OpFile file;
		LEAVE_IF_ERROR(file.Construct(full_file_name->CStr()));
		BOOL file_exists;
		LEAVE_IF_ERROR(file.Exists(file_exists));
		if (file_exists)
			is_found_existing_file_name = TRUE;

		attachment_file_names.AddL(full_file_name->CStr(), attachment_url);
		LEAVE_IF_ERROR(file_names.Add(full_file_name.get()));
		full_file_name.release();
	}

	// ask if we want to overwrite existing files
	if (is_found_existing_file_name && SimpleDialog::ShowDialog(WINDOW_NAME_YES_NO, this,
			Str::S_MAIL_ATTACHMENTS_SAVE_PROMPT_OVERWRITE, Str::S_OPERA,
			Dialog::TYPE_YES_NO, Dialog::IMAGE_WARNING) == Dialog::DIALOG_RESULT_NO)
		return;

	// save the attachments to files
	for (StringHashIterator<URL> it(attachment_file_names); it; it++)
	{
		const uni_char* full_file_name = it.GetKey();
		URL* attachment_url = it.GetData();
		long error_code = attachment_url->SaveAsFile(full_file_name);
		if (error_code)
			LEAVE(error_code);
	}

	// save location for next time the user is saving something
	LEAVE_IF_ERROR(g_folder_manager->SetFolderPath(OPFILE_SAVE_FOLDER, directory.CStr()));
	m_new_save_session = false;

}


void SaveAttachmentsDialog::OnCancel()
{
}

#endif // SPELLCHECK_SUPPORT
