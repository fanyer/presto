/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/managers/FindTextManager.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick/windows/SourceDesktopWindow.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/util/opfile/unistream.h"
#include "adjunct/desktop_pi/desktop_file_chooser.h"
#include "adjunct/desktop_util/file_chooser/file_chooser_fun.h"
#include "adjunct/quick/widgets/OpFindTextBar.h"

/***********************************************************************************
**
**	SourceDesktopWindow
**
***********************************************************************************/

SourceDesktopWindow::SourceDesktopWindow(OpWorkspace* parent_workspace, URL *url, const uni_char *title, unsigned long window_id, BOOL frame_source) :
	m_url(url),
	m_edited(FALSE),
	m_window_id(window_id)
{

// Beware!!
// Changing the order in which widget children are added will change TAB selection order!!

	OP_STATUS status = DesktopWindow::Init(OpWindow::STYLE_DESKTOP, parent_workspace);
	CHECK_STATUS(status);

	status = OpMultilineEdit::Construct(&m_source_editfield);
	CHECK_STATUS(status);

	// Add some widget init stuff
	OpWidget* root_widget = GetWidgetContainer()->GetRoot();
	root_widget->AddChild(m_source_editfield);

	// Force the source edit field to be LTR.  Flagging it as "special form"
	// causes QuickOpWidgetBase::Relayout() to skip it, and thus it doesn't
	// inherit the direction of the root widget.
	m_source_editfield->SetRTL(FALSE);
	m_source_editfield->SetSpecialForm(TRUE);

	m_source_editfield->SetSystemFont(OP_SYSTEM_FONT_EMAIL_COMPOSE);

	m_source_editfield->SetText(UNI_L(""));
	m_source_editfield->SetWantsTab(TRUE);
#ifdef COLOURED_MULTIEDIT_SUPPORT
	m_source_editfield->SetColoured(TRUE);
#endif
	m_source_editfield->SetListener(this);

#ifdef WIDGETS_CAP_HAS_DISABLE_SPELLCHECK
	m_source_editfield->DisableSpellcheck();
#endif // WIDGETS_CAP_HAS_DISABLE_SPELLCHECK

	status = OpToolbar::Construct(&m_toolbar);
	CHECK_STATUS(status);
	root_widget->AddChild(m_toolbar);
	m_toolbar->SetName("Source View Toolbar");

	OpFindTextBar* search_bar = GetFindTextBar();
	root_widget->AddChild(search_bar);
	// Remove the "find links" item from the dropdown
	if (OpDropDown *dropdown = (OpDropDown *) search_bar->GetWidgetByType(OpTypedObject::WIDGET_TYPE_DROPDOWN))
		if (dropdown->CountItems() == 3)
			dropdown->RemoveItem(2);

	TRAPD(err, m_url->GetAttributeL(URL::KSuggestedFileName_L, m_save_filename, TRUE));
	if (err!=OpStatus::OK || m_save_filename.IsEmpty())
		m_save_filename.Set(UNI_L("index.html"));

	// Grab the filename, if this fails don't let them save the file
	TRAP(err, m_url->GetAttributeL(URL::KFilePathName_L, m_filename, TRUE));
	if (err!=OpStatus::OK || m_filename.IsEmpty())
		m_edited = FALSE;

	// Fill out the edit control with the source
	if (m_url)
	{
		URL_DataDescriptor *desc = NULL;

		if (frame_source)
			desc = m_url->GetDescriptor(NULL, FALSE);
		else
			desc = m_url->GetDescriptor(NULL, FALSE, FALSE, TRUE, NULL, URL_UNDETERMINED_CONTENT, 0, TRUE);

		if (desc)
		{
			BOOL more = TRUE;

			while(more && desc)
			{
				OP_MEMORY_VAR int data_len = 0;
				TRAPD(err, data_len = desc->RetrieveDataL(more));
				if (OpStatus::IsError(err))
				{
					break;	// an error occurred
				}
				else if (data_len)
				{
					m_source.Append((uni_char*)desc->GetBuffer(), UNICODE_DOWNSIZE(data_len));
					desc->ConsumeData(data_len);
				}
			}

			// Save the charset encoding so we can write out the file the same way
			m_charset.Set(desc->GetCharacterSet());

			m_source_editfield->SetText(m_source.CStr());

			// Put the cursor at the start
			m_source_editfield->SetCaretOffset(0);

			OP_DELETE(desc);
		}
	}

	if (title)
		SetTitle(title);
	else
	{
		OpString url_string;
		m_url->GetAttribute(URL::KUniName_Username_Password_Hidden, url_string);
		SetTitle(url_string.CStr());
	}
	SetIcon("Window Document Icon");

	g_languageManager->GetString(Str::S_SAVE_AS_CAPTION, m_request.caption);
	m_request.action = DesktopFileChooserRequest::ACTION_FILE_SAVE;

	OpString filter;
	g_languageManager->GetString(Str::SI_SAVE_FILE_TYPES, filter);
	if (filter.HasContent())
	{
		filter.Append(UNI_L("|*.*|"));
		StringFilterToExtensionFilter(filter.CStr(), &m_request.extension_filters);
	}

	m_request.initial_path.Set(m_save_filename.CStr());
}

/***********************************************************************************
**
**	OnLayout
**
***********************************************************************************/

void SourceDesktopWindow::OnLayout()
{
	UINT32 width, height;

	GetInnerSize(width, height);

	INT32 toolbar_height = m_toolbar->GetHeightFromWidth(width);

	int search_bar_height = 0;
	OpFindTextBar* search_bar = GetFindTextBar();
	if (search_bar->GetAlignment() != OpBar::ALIGNMENT_OFF)
	{
		search_bar_height = search_bar->GetHeightFromWidth(width);
		search_bar->SetRect(OpRect(0, toolbar_height, width, search_bar_height));
	}

	// Layout the edit control and the toolbar
	m_toolbar->SetRect(OpRect(0, 0, width, toolbar_height));
	m_source_editfield->SetRect(OpRect(0, toolbar_height + search_bar_height, width, height - toolbar_height - search_bar_height));
}





/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL SourceDesktopWindow::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
    	case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
                case OpInputAction::ACTION_SAVE_DOCUMENT:
					child_action->SetEnabled(TRUE);
					return TRUE;
                case OpInputAction::ACTION_RELOAD:
					child_action->SetEnabled(m_edited);
					return TRUE;
				case OpInputAction::ACTION_FIND_NEXT:
				case OpInputAction::ACTION_FIND_PREVIOUS:
				case OpInputAction::ACTION_FIND:
					return TRUE;
 			}
		}
		break;

		case OpInputAction::ACTION_SHOW_FINDTEXT:
		{
			ShowFindTextBar(action->GetActionData());
			return TRUE;
		}
		case OpInputAction::ACTION_SAVE_DOCUMENT:
		{
			DesktopFileChooser *chooser;
			if (OpStatus::IsSuccess(GetChooser(chooser)))
				OpStatus::Ignore(chooser->Execute(GetOpWindow(), this, m_request));
			return TRUE;
		}
		break;

		case OpInputAction::ACTION_RELOAD:
		{
			BOOL written = FALSE;
			{
				UnicodeFileOutputStream cachefile;
				OpString				new_text;

				if (OpStatus::IsSuccess(cachefile.Construct(m_filename.CStr(), m_charset.CStr())))
				{
					m_source_editfield->GetText(new_text);
					cachefile.WriteStringL(new_text.CStr());
					written = TRUE;


				}
			}

			if (written)
			{
				// Reload the original page from cache
				if (m_window_id) {
					g_input_manager->InvokeAction(OpInputAction::ACTION_REFRESH_DISPLAY, m_window_id);

					// Look for the window (i.e they haven't closed it already) and re assign the title
					DesktopWindow *desktop_window = g_application->GetDesktopWindowCollection().GetDesktopWindowByID((INT32)m_window_id);

					if (desktop_window)
						SetTitle(desktop_window->GetTitle());
				}

				// Reset to not edited
				m_edited = FALSE;

				return TRUE;
			}
		}
		break;
	}

	return DesktopWindow::OnInputAction(action);
}

void SourceDesktopWindow::OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
{
	if (result.files.GetCount())
	{
		// Save the file
		UnicodeFileOutputStream file;
		OpString				new_text;

		if (OpStatus::IsSuccess(file.Construct(result.files.Get(0)->CStr(), m_charset.CStr())))
		{
			m_source_editfield->GetText(new_text);
			file.WriteStringL(new_text.CStr());
		}
	}
}

void SourceDesktopWindow::SetTitle(const uni_char *title)
{
	// Set the window title to Source: plus the actual title of the page
	OpString strTitle;

	g_languageManager->GetString(Str::S_SOURCE_VIEWER_LABEL, strTitle);

	strTitle.Append(UNI_L(" "));
	strTitle.Append(title);

	DesktopWindow::SetTitle(strTitle.CStr());
}


void SourceDesktopWindow::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	// If the state is changing we need to update the input states
	if (!m_edited && g_input_manager)
		g_input_manager->UpdateAllInputStates();

	m_edited = TRUE;

	DesktopWindow::OnChange(widget, changed_by_mouse);
}
