/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/quick/windows/DesktopTab.h"

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_util/file_chooser/file_chooser_fun.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/util/gen_str.h"
#include "modules/widgets/WidgetContainer.h"
#include "adjunct/desktop_util/actions/action_utils.h"
#include "adjunct/quick/widgets/OpFindTextBar.h"
#include "adjunct/quick/managers/PrivacyManager.h"



UINT32 DesktopTab::s_default_filter = 2;


/***********************************************************************************
** Constructor
**
** DesktopTab::DesktopTab
***********************************************************************************/
DesktopTab::DesktopTab()
	: m_save_focused_frame(FALSE)
#ifdef DEBUG_ENABLE_OPASSERT
	, m_waiting_for_filechooser(0)
#endif
	, m_chooser(0)
	, m_search_bar(0)
{
	OpFindTextBar::Construct(&m_search_bar);
	m_search_bar->SetAlignment(OpBar::ALIGNMENT_OFF);
}

/***********************************************************************************
** Destructor
**
** DesktopTab::~DesktopTab
***********************************************************************************/
DesktopTab::~DesktopTab()
{
	OP_ASSERT(m_waiting_for_filechooser == 0);
	OP_DELETE(m_chooser);
}

/***********************************************************************************
** Listener callback
**
** DesktopTab::OnFileChoosingDone
***********************************************************************************/
void DesktopTab::OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
{
	OP_ASSERT(m_waiting_for_filechooser > 0);
#ifdef DEBUG_ENABLE_OPASSERT
	m_waiting_for_filechooser -= 1;
#endif
	UnblockWindowClosing();

	if (result.files.GetCount())
	{
		OpWindowCommander::SaveAsMode mode = OpWindowCommander::SAVE_ONLY_DOCUMENT;
		if (m_request.fixed_filter)
		{
			// Here is a piece of hack: with fixed filters, one of two things may
			// be the case, we either look at a document we want to save, or we
			// are looking at a mhtml archive, in which case we only offer mhtml
			// as an alternative. This is a fragile hack, so if there can be other
			// fixed filters (defined in the GetFilter implementation) this code must
			// be reworked. Hint: Keep the mediatype around and use the description
			if (m_request.extension_filters.GetCount() == 1)
				mode = OpWindowCommander::SAVE_AS_MHTML;
			else
			{
				if (result.selected_filter == 0)
					mode = OpWindowCommander::SAVE_ONLY_DOCUMENT;
				else if (result.selected_filter == 1)
					mode = OpWindowCommander::SAVE_DOCUMENT_AND_INLINES;
				else if (result.selected_filter == 2)
					mode = OpWindowCommander::SAVE_AS_MHTML;
				else if (result.selected_filter == 3)
					mode = OpWindowCommander::SAVE_AS_TEXT;

				// Don't update the saved filter number if the previous choice didn't apply
				if (s_default_filter < m_request.extension_filters.GetCount())
					s_default_filter = result.selected_filter;
			}
		}
		GetWindowCommander()->SaveDocument(result.files.Get(0)->CStr(), mode, m_save_focused_frame);
		if (result.active_directory.HasContent())
		{
			if (PrivacyMode())
			{
				PrivacyManager::GetInstance()->SetPrivateWindowSaveFolderLocation(result.active_directory);
			}
			else
			{
				TRAPD(err, g_pcfiles->WriteDirectoryL(OPFILE_SAVE_FOLDER, result.active_directory.CStr()));
			}
		}
	}
}


/***********************************************************************************
** Save the current document in this window
**
** DesktopTab::SaveDocument
***********************************************************************************/
OP_STATUS DesktopTab::SaveDocument(BOOL save_focused_frame)
{
	OpWindowCommander* commander      = GetWindowCommander();
	m_save_focused_frame              = save_focused_frame;
	DesktopFileChooser* chooser       = 0;

	if (!commander)
		return OpStatus::ERR;

	RETURN_IF_ERROR(GetChooser(chooser));

	OpString filename;

	RETURN_IF_ERROR(commander->GetSuggestedFileName(save_focused_frame, &filename));
	if (!g_desktop_op_system_info->RemoveIllegalFilenameCharacters(filename, TRUE))
		return OpStatus::ERR;

	if (PrivacyMode())
	{
		m_request.initial_path.Set(PrivacyManager::GetInstance()->GetPrivateWindowSaveFolderLocation());	
	}
	else
	{
		OpString tmp_storage;
		m_request.initial_path.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_SAVE_FOLDER, tmp_storage));
	}

	if (filename.HasContent())
	{
		m_request.initial_path.Append(filename.CStr());
	}

	// File extensions/filters, start.........
	OpString extensions;
	Str::LocaleString num(Str::NOT_A_STRING);

	OpWindowCommander::DocumentType doc_type = commander->GetDocumentType(save_focused_frame);
	switch(doc_type)
	{
		case OpWindowCommander::DOC_PLAINTEXT:
			num = Str::D_SAVE_DOCUMENT_TYPES;
			break;
		case OpWindowCommander::DOC_HTML:
			num = Str::D_SAVE_DOCUMENT_TYPES;
			break;
		case OpWindowCommander::DOC_MHTML:
			num = Str::D_SAVE_DOCUMENT_MHTML_TYPES;
			break;
		case OpWindowCommander::DOC_XHTML:
			num = Str::D_SAVE_DOCUMENT_TYPES;
			break;
		case OpWindowCommander::DOC_WML:
			num = Str::D_SAVE_DOCUMENT_TYPES;
			break;
		case OpWindowCommander::DOC_SVG:
			break;
#ifdef WIC_CAP_DOCUMENTTYPE_WEBFEED
		case OpWindowCommander::DOC_WEBFEED:
			num = Str::D_SAVE_DOCUMENT_WEBFEED_TYPES;
			break;
#endif // WIC_CAP_DOCUMENTTYPE_WEBFEED
		case OpWindowCommander::DOC_OTHER_TEXT:
		// this is XML (not XHTML)
			num = Str::D_SAVE_DOCUMENT_XML_TYPES;
			break;
		case OpWindowCommander::DOC_OTHER_GRAPHICS:
			break;
		case OpWindowCommander::DOC_OTHER:
			break;
		default:
			num = Str::D_SAVE_DOCUMENT_TYPES;
			break;
	}

	if (num != Str::NOT_A_STRING)
		RETURN_IF_ERROR(g_languageManager->GetString(num, extensions));

	// Fix incompatibility in OpWindowCommander
	m_request.fixed_filter = commander->IsWebDocumentType(doc_type) || doc_type == OpWindowCommander::DOC_OTHER_TEXT;
	if (m_request.fixed_filter)
	{
		OpString initial_extension;
		StringFilterToExtensionFilter(extensions.CStr(), &(m_request.extension_filters));
		// for MHTML documents, we get only one extension filter, don't apply the save/restore logic
		m_request.initial_filter = s_default_filter < m_request.extension_filters.GetCount() ? s_default_filter : 0;
		GetInitialExtension(m_request, initial_extension);
		if (initial_extension.HasContent())
		{
			int lastDot = m_request.initial_path.FindLastOf('.');
			if (lastDot != KNotFound)
			{
				m_request.initial_path.Delete(lastDot);
			}
			m_request.initial_path.Append(initial_extension.CStr() + 1 /*skipping * */);
		}
	}
	else
	{
		StringFilterToExtensionFilter(extensions.CStr(), &(m_request.extension_filters));

		OpAutoPtr<OpString> file_extension (OP_NEW(OpString, ()));
		if (!file_extension.get())
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(file_extension->Set(StrFileExt(m_request.initial_path.CStr())));

		// Add extension of filename, if it's not already in the xtension_filters
		if (extensions.FindI(*file_extension) != KNotFound)
		{
			INT32 filter_pos = FindExtension(*file_extension, &(m_request.extension_filters));
			if (filter_pos != -1)
				m_request.initial_filter = filter_pos;
			else
				m_request.initial_filter = 0;
		}
		else if (file_extension->HasContent())
		{
			OpAutoPtr<OpFileSelectionListener::MediaType> filter (OP_NEW(OpFileSelectionListener::MediaType, ()));
			if (!filter.get())
				return OpStatus::ERR_NO_MEMORY;

			m_request.initial_filter = m_request.extension_filters.GetCount() - 1; 

			RETURN_IF_ERROR(filter->file_extensions.Add(file_extension.get()));
			file_extension.release();

			RETURN_IF_ERROR(m_request.extension_filters.Add(filter.get()));
			filter.release();
		}

		// Add all files...
		RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_IDSTR_ALL_FILES_ASTRIX, extensions));
		if (extensions.HasContent())
		{
			extensions.Append("|*|");
			StringFilterToExtensionFilter(extensions.CStr(), &(m_request.extension_filters));
		}
	}
	// End of extension work...

	m_request.action = DesktopFileChooserRequest::ACTION_FILE_SAVE_PROMPT_OVERWRITE;
	m_request.get_selected_filter = TRUE;

	/* Keep the tab from being closed.  There are two reasons for this:
	 *
	 * - Since we are setting up a callback, this object must not be
	 *   deleted until the callback has been called.
	 *
	 * - From a UI design point of view: If the window is removed
	 *   while the file dialog is up, the user will have an
	 *   inconsistent UI state.  Alternatively, we could remove the
	 *   file dialog when its window disappears.  But that is also
	 *   likely to be quite surprising to the user.  So disallowing
	 *   the window from disappearing at all is probably the least of
	 *   the available evils.
	 *
	 * The dangling pointer issue could be solved much more easily and
	 * safer in other ways.  But as long as the UI considerations
	 * above are considered important, we'll have to do the
	 * complicated stuff instead.
	 */
#ifdef DEBUG_ENABLE_OPASSERT
	m_waiting_for_filechooser += 1;
#endif
	BlockWindowClosing();

	return chooser->Execute(commander->GetOpWindow(), this, m_request);
}


/***********************************************************************************
** Get a file chooser object
**
** DesktopTab::GetChooser
***********************************************************************************/
OP_STATUS DesktopTab::GetChooser(DesktopFileChooser*& chooser)
{
	if (!m_chooser)
		RETURN_IF_ERROR(DesktopFileChooser::Create(&m_chooser));

	chooser = m_chooser;
	return OpStatus::OK;
}

/***********************************************************************************
**
**	ShowFindTextBar
**
***********************************************************************************/

void DesktopTab::ShowFindTextBar(BOOL show)
{
	if (m_search_bar)
	{
		if (m_search_bar->SetAlignmentAnimated(show ? OpBar::ALIGNMENT_OLD_VISIBLE : OpBar::ALIGNMENT_OFF))
			SyncLayout(); // make sure the search bar is visible
		if (show && (!g_input_manager->GetKeyboardInputContext() || !g_input_manager->GetKeyboardInputContext()->IsChildInputContextOf(m_search_bar)))
			m_search_bar->SetFocus(FOCUS_REASON_OTHER);
	}
}

/***********************************************************************************
**
**	OnContextMenu
**
***********************************************************************************/

BOOL DesktopTab::OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked)
{
	return HandleWidgetContextMenu(widget, menu_point);
}
