/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand, Patricia Aas
 */
#include "core/pch.h"
#include "adjunct/quick/dialogs/TrustedProtocolDialog.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick/managers/WebmailManager.h"
#include "adjunct/quick/widgets/DesktopFileChooserEdit.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpDropDown.h"

#if defined(MSWIN)
#include "platforms/windows/windows_ui/winshell.h"
#endif //MSWIN

#if defined(_UNIX_DESKTOP_)
#include "platforms/unix/base/common/applicationlauncher.h"
#endif //_UNIX_DESKTOP_

# include "adjunct/desktop_util/string/stringutils.h"

/***********************************************************************************
 ** TrustedProtocolDialog - constructor
 **
 **
 **
 **
 ***********************************************************************************/
TrustedProtocolDialog::TrustedProtocolDialog( TrustedProtocolList *list, INT32 index )
{
	m_list = list;
	if( m_list && index >= 0 && (UINT32)index < list->GetCount() )
	{
		m_index = index;
	}
	else
	{
		m_index = -1;
	}
}

/***********************************************************************************
 ** TrustedProtocolDialog::OnInit
 **
 **
 **
 **
 ***********************************************************************************/
void TrustedProtocolDialog::OnInit()
{
	SetWidgetEnabled("Edit_default_application", FALSE);
	SetWidgetValue("Radio_other_application", TRUE); // default to other app
	SetWidgetEnabled("Radio_use_webmail", FALSE);
	SetWidgetEnabled("Dropdown_webmail_service", FALSE);
	ShowWidget("Radio_use_Opera", FALSE); // Only turn this on explicitly

	DesktopFileChooserEdit* chooser = GetWidgetByName<DesktopFileChooserEdit>(
			"Edit_other_application", WIDGET_TYPE_FILECHOOSER_EDIT);
	if (chooser)
	{
		chooser->GetEdit()->SetOnChangeOnEnter(FALSE);
		chooser->GetEdit()->SetForceTextLTR(TRUE);
		chooser->SetListener(this);
	}

#ifdef MSWIN
	if (chooser)
	{
		OpString filterstring;
		g_languageManager->GetString(Str::SI_PROGS_TYPES, filterstring);

		if (filterstring.HasContent())
		{
			chooser->SetFilterString(filterstring.CStr());
		}
	}
#endif

	TrustedProtocolEntry* entry = m_list ? m_list->Get(m_index) : NULL;
	if (entry)
	{
		if (!entry->IsProtocol())
		{
			OpString task;
			g_languageManager->GetString(entry->GetEntryType() == TrustedProtocolEntry::SOURCE_VIEWER_ENTRY ?
										 Str::LocaleString(Str::DI_IDM_SRC_VIEWER_BOX) : 
										 Str::LocaleString(Str::D_TRUSTED_PROTOCOL_TASK), task);
			SetTitle(task.CStr());
			SetWidgetText("label_protocol", task.CStr());
		}

#if defined(_UNIX_DESKTOP_)
		OpString text, protocol;

		protocol.Set(entry->GetProtocol());
		ApplicationLauncher::GetCommandLineOptions(protocol, text);

		OpEdit* edit = GetWidgetByName<OpEdit>("Other_parameter_edit", WIDGET_TYPE_EDIT);
		if (edit)
		{
			edit->SetToolTipText(text);
			edit->SetForceTextLTR(TRUE);
		}
#endif

		if (entry->GetProtocol().IsEmpty())
		{
			SetWidgetEnabled("Protocol_edit",TRUE);
			ShowWidget("Protocol_label", FALSE);
		}
		else
		{
			SetWidgetText("Protocol_label", entry->GetDisplayText());
			SetWidgetEnabled("Protocol_label", TRUE);
			ShowWidget("Protocol_edit", FALSE);
		}

		if (entry->GetEntryType() == TrustedProtocolEntry::MAILTO_ENTRY)
		{
			ShowWidget("Edit_webhandler", FALSE);
			SetWidgetEnabled("Radio_use_webmail", TRUE);
			SetWidgetEnabled("Dropdown_webmail_service", TRUE);
			// Fill the web service selection dropdown
			OpDropDown* dropdown = static_cast<OpDropDown*>(GetWidgetByName("Dropdown_webmail_service"));
			if (dropdown)
			{
				for (UINT32 i=0; i < WebmailManager::GetInstance()->GetCount(); i++)
				{
					UINT32 id = WebmailManager::GetInstance()->GetIdByIndex(i);
					OpString name;
					if (OpStatus::IsSuccess(WebmailManager::GetInstance()->GetName(id, name)))
					{
						int got_index;
						dropdown->AddItem(name.CStr(), -1, &got_index, id);
						if (entry->IsWebHandlerBlocked(id))
							dropdown->SetTextDecoration(got_index, WIDGET_LINE_THROUGH, TRUE);

						// Set the favicon based on the URL
						OpString url;
						if (OpStatus::IsSuccess(WebmailManager::GetInstance()->GetURL(id,url)))
						{
							OpWidgetImage widget_image;
							widget_image.SetRestrictImageSize(TRUE);
							Image img = g_favicon_manager->Get(url.CStr());
							if (img.IsEmpty())
								widget_image.SetImage("Mail Unread");
							else
								widget_image.SetBitmapImage(img);
							dropdown->ih.SetImage(got_index, &widget_image, dropdown);
						}
						if (id == entry->GetWebmailId())
							dropdown->SelectItem(got_index, TRUE);
					}
				}

				// Dropdown is only enabled when selecting web service
				dropdown->SetEnabled(entry->GetViewerMode() == UseWebService);
			}
		}
		else
		{
			ShowWidget("Dropdown_webmail_service", FALSE);
			SetWidgetText("Radio_use_webmail", UNI_L("Open with web handler"));
			SetWidgetEnabled("Edit_webhandler", FALSE);
			OpString application;
			entry->GetWebServiceName(application);
			SetWidgetText("Edit_webhandler", application);

			// The ID parameter is not important for NON MAILTO_ENTRY types
			if (entry->IsWebHandlerBlocked(0))
			{
				OpEdit* edit = (OpEdit*)GetWidgetByName("Edit_webhandler");
				if (edit)
					edit->SetTextDecoration(WIDGET_LINE_THROUGH, TRUE);
			}

			SetWidgetEnabled("Radio_use_webmail", application.HasContent());
		}


		SetWidgetText("Edit_default_application", entry->GetDefaultApplication());
		SetWidgetText("Edit_other_application", entry->GetCustomApplication());
		SetWidgetText("Other_parameter_edit", entry->GetCustomParameters());
		SetWidgetValue("terminal_checkbox", entry->InTerminal());

		switch (entry->GetViewerMode())
		{
		case UseDefaultApplication:
			SetWidgetValue("Radio_default_application", TRUE);
			break;
		case UseInternalApplication:
			SetWidgetValue("Radio_use_opera", TRUE);
			break;
		case UseWebService:
			SetWidgetValue("Radio_use_webmail", TRUE);
			break;
		default:
			SetWidgetValue("Radio_other_application", TRUE);
		}
	}
	else
	{
		ShowWidget("Dropdown_webmail_service", FALSE);
		SetWidgetText("Radio_use_webmail", UNI_L("Open with web handler"));
		SetWidgetEnabled("Radio_use_webmail", FALSE);
		SetWidgetEnabled("Edit_webhandler", FALSE);
	}



	ShowWidget("Radio_use_Opera", entry && entry->HasNativeSupport());
}



void TrustedProtocolDialog::OnInitVisibility()
{
#if !defined(_UNIX_DESKTOP_)
	ShowWidget("terminal_checkbox", FALSE);
#endif
#if defined(_MACINTOSH_)
	ShowWidget("label_for_Other_parameter_edit", FALSE);
	ShowWidget("Other_parameter_edit", FALSE);
#endif
}



/***********************************************************************************
 ** TrustedProtocolDialog::OnInputAction
 **
 **
 **
 **
 ***********************************************************************************/
BOOL TrustedProtocolDialog::OnInputAction(OpInputAction* action)
{
	if (action->GetAction() == OpInputAction::ACTION_OK && m_list)
	{
		TrustedProtocolEntry* entry = m_list->Get(m_index);
		if (!entry && m_index == -1)
		{
			// New entry
			OpWidget* widget = GetWidgetByName("Protocol_edit");
			if (widget)
			{
				OpString protocol;
				widget->GetText(protocol);
				protocol.Strip();
				if (protocol.HasContent())
				{
					// Prevent duplicates
					if (m_list->GetByString(protocol))
						return Dialog::OnInputAction(action);

					entry = m_list->Add();
					if (entry)
					{
						entry->SetCanDelete(TRUE);
						entry->Init(protocol, OpStringC(), OpStringC(), TRUE);
					}
				}
			}
		}

		if (entry)
		{
			OpString default_application, custom_application, custom_parameters;
			UINT32 webmail_id = 0;

			GetWidgetText("Edit_default_application", default_application);
			GetWidgetText("Edit_other_application", custom_application);
			GetWidgetText("Other_parameter_edit", custom_parameters);

			BOOL in_terminal = GetWidgetValue("terminal_checkbox", FALSE);

			ViewerMode viewer_mode = UseCustomApplication;
			if (GetWidgetValue("Radio_default_application", FALSE))
				viewer_mode = UseDefaultApplication;
			else if (GetWidgetValue("Radio_use_Opera", FALSE))
				viewer_mode = UseInternalApplication;
			else if (GetWidgetValue("Radio_other_application", FALSE))
				viewer_mode = UseCustomApplication;
			else if (GetWidgetValue("Radio_use_webmail", FALSE))
				viewer_mode = UseWebService;

			OpDropDown* dropdown = static_cast<OpDropDown*>(GetWidgetByName("Dropdown_webmail_service"));
			if (dropdown)
			{
				if (entry->GetEntryType() == TrustedProtocolEntry::MAILTO_ENTRY)
				{
					if (dropdown->GetSelectedItem() != -1)
					{
						webmail_id = dropdown->GetItemUserData(dropdown->GetSelectedItem());
					}
				}
			}

			// Sanity assurance - if there is no custom handler
			// there should not be stored any parameters or
			// terminal setting Bug 264005
			UnQuote(custom_application);
			if (custom_application.IsEmpty())
			{
				custom_parameters.Empty();
				in_terminal = FALSE;
			}

			OpString command;
			entry->JoinCommand(command, custom_application, custom_parameters);
			entry->Set(command, viewer_mode, in_terminal, webmail_id);
			entry->SetDefaultApplication(default_application);
		}
	}

	return Dialog::OnInputAction(action);
}


/***********************************************************************************
 ** TrustedProtocolDialog::OnChange
 **
 **
 **
 **
 ***********************************************************************************/
void TrustedProtocolDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if(widget->IsNamed("Protocol_edit"))
	{
		OpString handler;

		OpString protocol_string;
		widget->GetText(protocol_string);

		OpString uri_string;

		g_op_system_info->GetProtocolHandler(uri_string, protocol_string, handler);
		SetWidgetText("Edit_default_application", handler.CStr());
	}
	// Only enable the parameter field and the terminal checkbox if the user has specified an
	// application. Bug 264005
	else if(widget->IsNamed("Radio_other_application") || widget->IsNamed("Edit_other_application"))
	{
		BOOL value = FALSE;

		if(widget->IsNamed("Radio_other_application"))
			value = widget->GetValue();
		else if(widget->IsNamed("Edit_other_application"))
			value = TRUE;

		SetWidgetEnabled("Edit_other_application", value);

		OpString custom_handler;
		GetWidgetText("Edit_other_application", custom_handler);

		BOOL enable_param = value && custom_handler.HasContent();

		SetWidgetEnabled("label_for_Other_parameter_edit", enable_param);
		SetWidgetEnabled("Other_parameter_edit", enable_param);
		SetWidgetEnabled("terminal_checkbox", enable_param);
	}
	else if (widget->IsNamed("Radio_use_webmail"))
	{
		SetWidgetEnabled("Dropdown_webmail_service", widget->GetValue());
	}

	Dialog::OnChange(widget, changed_by_mouse);
}


//-----------------------------------------------------------
// TrustedProtocolList
//-----------------------------------------------------------

/***********************************************************************************
 ** TrustedProtocolList::Read
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TrustedProtocolList::Read( ListContent content )
{
	switch (content)
	{
		case APPLICATIONS:
		{
			// Standard entry: source viewer
			TrustedProtocolEntry* entry = OP_NEW(TrustedProtocolEntry, ());
			if (!entry || OpStatus::IsError(MakeSourceViewerEntry(*entry)) || OpStatus::IsError(m_list.Add(entry)))
			{
				OP_DELETE(entry);
				return OpStatus::ERR_NO_MEMORY;
			}
		}
		break;

		case PROTOCOLS:
		{
			TrustedProtocolEntry* entry;

#if defined(_UNIX_DESKTOP_)
			BOOL nntp_in_list = FALSE;
#endif
			BOOL mailto_in_list = FALSE;

			INT32 num_trusted_protocols = g_pcdoc->GetNumberOfTrustedProtocols();

			TrustedProtocolData data;
			for (INT32 i=0; i<num_trusted_protocols; i++)
			{
				if (!g_pcdoc->GetTrustedProtocolInfo(i, data))
					continue;

				// Look up entry for given protocol. Create a new if it does not
				// exists or replace the current content with what we have just read
				entry = GetByString(data.protocol);
				BOOL new_entry = !entry;

				if (new_entry)
				{
					entry = OP_NEW(TrustedProtocolEntry, ());
					if (!entry)
						return OpStatus::ERR_NO_MEMORY;
				}

				if (!data.protocol.CompareI("mailto"))
				{
					// Use data from prefs instead of what we have just read
					if (OpStatus::IsError(MakeMailtoEntry(*entry)))
					{
						if (new_entry)
							OP_DELETE(entry);
						return OpStatus::ERR_NO_MEMORY;
					}
					mailto_in_list = TRUE;
				}
				else
				{
					entry->SetCanDelete(TRUE);
					entry->SetHasNativeSupport(FALSE);
					if (OpStatus::IsError(entry->Init(data.protocol, data.web_handler, data.description, TRUE)) ||
						OpStatus::IsError(entry->Set(data.filename, data.viewer_mode, data.in_terminal, 0)))
					{
						if (new_entry)
							OP_DELETE(entry);
						return OpStatus::ERR_NO_MEMORY;
					}
#if defined(_UNIX_DESKTOP_)
					if (!nntp_in_list && entry->IsNewsProtocol())
						nntp_in_list = TRUE;
#endif
				}

				if (new_entry)
				{
					if (OpStatus::IsError(m_list.Add(entry)))
					{
						OP_DELETE(entry);
						return OpStatus::ERR_NO_MEMORY;
					}
				}
			}

			if (!mailto_in_list)    // mailto shall always be present
			{
				entry = OP_NEW(TrustedProtocolEntry, ());
				if (!entry || OpStatus::IsError(MakeMailtoEntry(*entry)) || OpStatus::IsError(m_list.Add(entry)))
				{
					OP_DELETE(entry);
					return OpStatus::ERR_NO_MEMORY;
				}
				mailto_in_list = TRUE;
			}

#if defined(_UNIX_DESKTOP_)
			// backward compatibilty
			if (!nntp_in_list && g_pcapp->GetStringPref(PrefsCollectionApp::ExternalNewsClient).HasContent())
			{
				entry = OP_NEW(TrustedProtocolEntry, ());
				if (!entry || OpStatus::IsError(MakeNntpEntry(*entry)) || OpStatus::IsError(m_list.Add(entry)))
				{
					OP_DELETE(entry);
					return OpStatus::ERR_NO_MEMORY;
				}
			}
#endif
		}
		break;
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** TrustedProtocolList::MakeMailtoEntry
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TrustedProtocolList::MakeMailtoEntry(TrustedProtocolEntry& entry)
{
	// ---------------
	// Get the mode :
	// ---------------

	ViewerMode viewer_mode;
	switch (g_pcui->GetIntegerPref(PrefsCollectionUI::MailHandler))
	{
		case MAILHANDLER_APPLICATION:
			viewer_mode = UseCustomApplication;
			break;
		case MAILHANDLER_SYSTEM:
			viewer_mode = UseDefaultApplication;
			break;
		case MAILHANDLER_OPERA:
			viewer_mode = UseInternalApplication;
			break;
		case MAILHANDLER_WEBMAIL:
			viewer_mode = UseWebService;
			break;
		default:
			viewer_mode = UseCustomApplication;
			break;
	}

	// ---------------
	// Get the external handler :
	// ---------------

	OpStringC custom_application = g_pcapp->GetStringPref(PrefsCollectionApp::ExternalMailClient);

	// ---------------
	// Make a string with the protocol :
	// ---------------

	OpString protocol;
	RETURN_IF_ERROR(protocol.Set(UNI_L("mailto")));

	// ---------------
	// Get the system default handler :
	// ---------------

	OpString empty, default_application;
	OP_STATUS rc = g_op_system_info->GetProtocolHandler(empty, protocol, default_application);
	if (OpStatus::IsError(rc) && rc != OpStatus::ERR_NOT_SUPPORTED)
		return rc;

	// ---------------
	// Get Web Service handler :
	// ---------------

    UINT32 webmail_id = g_pcui->GetIntegerPref(PrefsCollectionUI::WebmailService);

	// ---------------
	// Check if it should run in terminal :
	// ---------------

	BOOL in_terminal = FALSE;
#if defined(_UNIX_DESKTOP_)
	in_terminal = g_pcapp->GetIntegerPref(PrefsCollectionApp::PrefsCollectionApp::RunEmailInTerminal);
#endif

	// ---------------
	// Initialize :
	// ---------------

	RETURN_IF_ERROR(entry.Init(protocol, OpStringC(), OpStringC(), TRUE));
	RETURN_IF_ERROR(entry.Set(custom_application, viewer_mode, in_terminal, webmail_id));

	// ---------------
	// Set up the rest :
	// ---------------

	RETURN_IF_ERROR(entry.SetDefaultApplication(default_application));
	entry.SetEntryType(TrustedProtocolEntry::MAILTO_ENTRY);
	entry.SetCanDelete(FALSE);
	entry.SetHasNativeSupport(TRUE);

	return OpStatus::OK;
}

/***********************************************************************************
 ** TrustedProtocolList::MakeNntpEntry
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TrustedProtocolList::MakeNntpEntry(TrustedProtocolEntry& entry)
{
#if defined(_UNIX_DESKTOP_)
	// ---------------
	// Get the custom application :
	// ---------------

	OpString custom_application;
	RETURN_IF_ERROR(custom_application.Set(g_pcapp->GetStringPref(PrefsCollectionApp::ExternalNewsClient)));

	// ---------------
	// Check if it should run in terminal :
	// ---------------

	BOOL in_terminal = g_pcapp->GetIntegerPref(PrefsCollectionApp::RunNewsInTerminal);

	// ---------------
	// Initialize :
	// ---------------

	RETURN_IF_ERROR(entry.Init(UNI_L("nntp"), OpStringC(), OpStringC(), TRUE));
	RETURN_IF_ERROR(entry.Set(custom_application, UseCustomApplication, in_terminal, 0));


	// ---------------
	// Set up the rest :
	// ---------------

	entry.SetEntryType(TrustedProtocolEntry::NNTP_ENTRY);
	entry.SetCanDelete(FALSE);
#endif

	return OpStatus::OK;
}

/***********************************************************************************
 ** TrustedProtocolList::MakeSourceViewerEntry
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TrustedProtocolList::MakeSourceViewerEntry(TrustedProtocolEntry& entry)
{
	// ---------------
	// Get text :
	// ---------------

	OpString displayed_text;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::D_TRUSTED_PROTOCOL_VIEW_SOURCE, displayed_text));

	// ---------------
	// Get the custom application :
	// ---------------

	OpStringC custom_application = g_pcapp->GetStringPref(PrefsCollectionApp::SourceViewer);

	// ---------------
	// Get the mode :
	// ---------------

	ViewerMode viewer_mode;
	switch (g_pcui->GetIntegerPref(PrefsCollectionUI::SourceViewerMode)) // Check sanity of value
	{
		case UseInternalApplication: viewer_mode = UseInternalApplication; break;
		case UseDefaultApplication : viewer_mode = UseDefaultApplication;  break;
		case UseCustomApplication  : viewer_mode = UseCustomApplication;   break;
		default: viewer_mode = UseInternalApplication; break;
	}

	// ---------------
	// Check if it should run in terminal :
	// ---------------

	BOOL in_terminal =
#if defined(_UNIX_DESKTOP_)
		g_pcapp->GetIntegerPref(PrefsCollectionApp::RunSourceViewerInTerminal)
#else
		FALSE
#endif
	;

	// ---------------
	// Initialize :
	// ---------------

	RETURN_IF_ERROR(entry.Init(UNI_L("view page source"), OpStringC(), OpStringC(), FALSE));
	RETURN_IF_ERROR(entry.Set(custom_application, viewer_mode, in_terminal, 0));

	// ---------------
	// Get the system default handler :
	// ---------------

	OpString dummy_filename, dummy_content_type, default_application;
	RETURN_IF_ERROR(dummy_filename.Set("txt.txt"));
	RETURN_IF_ERROR(dummy_content_type.Set("text/plain"));
	RETURN_IF_ERROR(g_op_system_info->GetFileHandler(&dummy_filename, dummy_content_type, default_application));

	// ---------------
	// Set up the rest :
	// ---------------

	RETURN_IF_ERROR(entry.SetDisplayedText(displayed_text));
	RETURN_IF_ERROR(entry.SetDefaultApplication(default_application));
	entry.SetCanDelete(FALSE);
	entry.SetHasNativeSupport(TRUE);
	entry.SetEntryType(TrustedProtocolEntry::SOURCE_VIEWER_ENTRY);

	return OpStatus::OK;
}


/***********************************************************************************
 ** TrustedProtocolList::Save
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TrustedProtocolList::Save(ListContent content) const
{
	if (content == TrustedProtocolList::APPLICATIONS)
	{
		// We only support SOURCE_VIEWER_ENTRY in application mode
		for (UINT32 i=0; i<m_list.GetCount(); i++)
		{
			TrustedProtocolEntry* entry = m_list.Get(i);
			if (entry->GetEntryType() == TrustedProtocolEntry::SOURCE_VIEWER_ENTRY)
			{
				RETURN_IF_ERROR(SaveSourceViewerEntry(*entry));
				break;
			}
		}
	}
	else if (content == TrustedProtocolList::PROTOCOLS)
	{
		OP_MEMORY_VAR int num_trusted_protocols = 0;
#if defined(_UNIX_DESKTOP_)
		OP_MEMORY_VAR BOOL news_in_list = FALSE;
#endif
		for (OP_MEMORY_VAR UINT32 i=0; i<m_list.GetCount(); i++)
		{
			TrustedProtocolEntry* entry = m_list.Get(i);

			// Save special types to another set of prefs as well
			switch (entry->GetEntryType())
			{
				case TrustedProtocolEntry::SOURCE_VIEWER_ENTRY:
					RETURN_IF_ERROR(SaveSourceViewerEntry(*entry));
				break;

				case TrustedProtocolEntry::MAILTO_ENTRY:
					RETURN_IF_ERROR(SaveMailtoEntry(*entry));
				break;
			}

			OpString custom_application_with_parameters;
			RETURN_IF_ERROR(entry->GetCommand(custom_application_with_parameters));

			TrustedProtocolData data;
			g_pcdoc->GetTrustedProtocolInfo(entry->GetProtocol(), data);
			data.flags = TrustedProtocolData::TP_All & ~TrustedProtocolData::TP_UserDefined;

			data.protocol     = entry->GetProtocol();
			data.filename     = custom_application_with_parameters;
			// We do not read web handlers for mail so do not save them either
			if (entry->GetEntryType() != TrustedProtocolEntry::MAILTO_ENTRY)
				data.web_handler  = entry->GetWebHandler();
			data.viewer_mode  = entry->GetViewerMode();
			data.in_terminal  = entry->InTerminal();

			if (entry->HasWebHandler())
			{
				// 'data.user_defined' is used as a "always use this application" flag by core
				// and core will thus not open a web handler bar if set. We only set it to TRUE
				// if core has a web handler. Otherwise, if we use this dialog before Opera visits a
				// page that provides a web handler, we will not be asked to use the web handler
				data.user_defined = TRUE;
				data.flags |= TrustedProtocolData::TP_UserDefined;
			}

			// This is the desktop side fix for DSK-345152. Core now supports web handlers
			// and will start them if told so. But, core does not know about desktop's
			// preinstalled web mail handlers so if we choose to use any of those we write
			// UseCustomApplication to prefs so that core hands the request to desktop.
			// MailCompose::ComposeMail() will change it back to webmail. We use the
			// PrefsCollectionUI::MailHandler entry for that operation
			if (entry->GetEntryType() == TrustedProtocolEntry::MAILTO_ENTRY)
			{
				if (entry->GetViewerMode() == UseWebService)
				{
					WebmailManager::HandlerSource source;
					OP_STATUS rc = WebmailManager::GetInstance()->GetSource(entry->GetWebmailId(), source);
					if (OpStatus::IsSuccess(rc) && source == WebmailManager::HANDLER_PREINSTALLED)
					{
						data.viewer_mode = UseCustomApplication;
					}
				}
			}

			RETURN_IF_LEAVE(g_pcdoc->SetTrustedProtocolInfoL(num_trusted_protocols, data));

#if defined(_UNIX_DESKTOP_)
			if (entry->IsNewsProtocol())
				news_in_list = TRUE;
#endif

			num_trusted_protocols ++;
		}

#if defined(_UNIX_DESKTOP_)
		// Be backward compatible
		if (!news_in_list)
		{
			RETURN_IF_LEAVE(g_pcapp->WriteStringL(PrefsCollectionApp::ExternalNewsClient, OpStringC()));
			RETURN_IF_LEAVE(g_pcapp->WriteIntegerL(PrefsCollectionApp::RunNewsInTerminal, FALSE));
		}
#endif

		RETURN_IF_LEAVE(g_pcdoc->WriteTrustedProtocolsL(num_trusted_protocols));
	}

	return OpStatus::OK;
}

/***********************************************************************************
 ** TrustedProtocolList::SaveMailtoEntry
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TrustedProtocolList::SaveMailtoEntry(const TrustedProtocolEntry& entry) const
{
	OP_MEMORY_VAR MAILHANDLER mail_handler;
	switch (entry.GetViewerMode())
	{
		case UseInternalApplication:
			mail_handler = MAILHANDLER_OPERA;
		break;
		case UseDefaultApplication:
			mail_handler = MAILHANDLER_SYSTEM;
		break;
		case UseWebService:
			mail_handler = MAILHANDLER_WEBMAIL;
		break;
		default:
			mail_handler = MAILHANDLER_APPLICATION;
		break;
	}

	OpString handler;
	RETURN_IF_ERROR(entry.GetCommand(handler));

	RETURN_IF_LEAVE(g_pcapp->WriteStringL(PrefsCollectionApp::ExternalMailClient, handler));
	RETURN_IF_LEAVE(g_pcui->WriteIntegerL(PrefsCollectionUI::MailHandler, mail_handler));
	RETURN_IF_LEAVE(g_pcui->WriteIntegerL(PrefsCollectionUI::WebmailService, entry.GetWebmailId()));
#if defined(_UNIX_DESKTOP_)
	RETURN_IF_LEAVE(g_pcapp->WriteIntegerL(PrefsCollectionApp::RunEmailInTerminal, entry.InTerminal() ));
#endif

	return OpStatus::OK;
}

/***********************************************************************************
 ** TrustedProtocolList::SaveSourceViewerEntry
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TrustedProtocolList::SaveSourceViewerEntry(const TrustedProtocolEntry& entry) const
{
	OpString handler;
	RETURN_IF_ERROR(entry.GetCommand(handler));

	RETURN_IF_LEAVE(g_pcapp->WriteStringL(PrefsCollectionApp::SourceViewer, handler));
	RETURN_IF_LEAVE(g_pcui->WriteIntegerL(PrefsCollectionUI::SourceViewerMode, entry.GetViewerMode()));
#if defined(_UNIX_DESKTOP_)
	RETURN_IF_LEAVE(g_pcapp->WriteIntegerL(PrefsCollectionApp::RunSourceViewerInTerminal, entry.InTerminal()));
#endif
	return OpStatus::OK;
}


/***********************************************************************************
 ** TrustedProtocolList::ResetBlockedWebHandlers
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TrustedProtocolList::ResetBlockedWebHandlers()
{
	RETURN_IF_ERROR(Read(TrustedProtocolList::PROTOCOLS));

	BOOL modified = FALSE;

	for (UINT32 i=0; i<m_list.GetCount(); i++)
	{
		if (m_list.Get(i)->ResetBlockedWebHandler())
			modified = TRUE;
	}

	if (modified)
		RETURN_IF_ERROR(Save(TrustedProtocolList::PROTOCOLS));

	return OpStatus::OK;
}


/***********************************************************************************
 ** TrustedProtocolList::Delete
 **
 **
 **
 **
 ***********************************************************************************/
void TrustedProtocolList::Delete(INT32 index)
{
	if (CanDelete(index))
		m_list.Delete(index);
}

/***********************************************************************************
 ** TrustedProtocolList::CanDelete
 **
 **
 **
 **
 ***********************************************************************************/
BOOL TrustedProtocolList::CanDelete(INT32 index) const
{
	TrustedProtocolEntry* entry = Get(index);
	return entry && entry->CanDelete();
}

/***********************************************************************************
 ** TrustedProtocolList::Add
 **
 **
 **
 **
 ***********************************************************************************/
TrustedProtocolEntry* TrustedProtocolList::Add()
{
	TrustedProtocolEntry* entry = OP_NEW(TrustedProtocolEntry, ());
	if (!entry || OpStatus::IsError(m_list.Add(entry)))
	{
		OP_DELETE(entry);
		entry = NULL;
	}
	return entry;
}

/***********************************************************************************
 ** TrustedProtocolList::Get
 **
 **
 **
 **
 ***********************************************************************************/
TrustedProtocolEntry* TrustedProtocolList::Get(INT32 index) const
{
	return index < 0 || (UINT32)index >= GetCount() ? 0 : m_list.Get(index);
}

/***********************************************************************************
 ** TrustedProtocolList::Get
 **
 **
 **
 **
 ***********************************************************************************/
TrustedProtocolEntry* TrustedProtocolList::GetByString(const OpStringC& protocol) const
{
	for(UINT32 i = 0; i < GetCount(); i++)
	{
		TrustedProtocolEntry* entry = m_list.Get(i);
		if (!entry->GetProtocol().CompareI(protocol))
			return entry;
	}
	return 0;
}


/***********************************************************************************
 ** TrustedProtocolList::GetSourceViewer
 **
 **
 **
 **
 ***********************************************************************************/
//static
OP_STATUS TrustedProtocolList::GetSourceViewer(OpString& handler)
{
	switch (g_pcui->GetIntegerPref(PrefsCollectionUI::SourceViewerMode)) // Check sanity of value
	{
		case UseInternalApplication:
		{
			return handler.Set(UNI_L("Opera"));
		}
		case UseDefaultApplication :
		{
			OpString dummy_filename;
			dummy_filename.Set("txt.txt");
			OpString dummy_content_type;
			dummy_content_type.Set("text/plain");
			return g_op_system_info->GetFileHandler(&dummy_filename, dummy_content_type, handler);
		}
		default:
		{
			return handler.Set(g_pcapp->GetStringPref(PrefsCollectionApp::SourceViewer));
		}
	}
}

//-----------------------------------------------------------
// TrustedProtocolEntry
//-----------------------------------------------------------


/***********************************************************************************
 ** TrustedProtocolEntry::GetActiveApplication
 **
 **
 **
 **
 ***********************************************************************************/

OP_STATUS TrustedProtocolEntry::GetActiveApplication(OpString& application, BOOL* is_formatted)
{
	switch (m_viewer_mode)
	{
		case UseInternalApplication:
			return application.Set("Opera");
		break;

		case UseDefaultApplication:
			if (m_default_application.IsEmpty())
			{
				OpString empty;
				RETURN_IF_ERROR(g_op_system_info->GetProtocolHandler(empty, m_protocol, m_default_application));
			}
			return application.Set(m_default_application);
		break;

		case UseWebService:
			return GetWebServiceName(application, is_formatted);
		break;

		default:
			return application.Set(m_custom_application);
	}
}


/***********************************************************************************
 ** TrustedProtocolEntry::GetWebServiceName
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TrustedProtocolEntry::GetWebServiceName(OpString& web_service_name, BOOL* is_formatted)
{
	RETURN_IF_ERROR(web_service_name.Set("Opera"));

	OpString name;
	if (m_entry_type == TrustedProtocolEntry::MAILTO_ENTRY)
		RETURN_IF_ERROR(WebmailManager::GetInstance()->GetName(m_webmail_id, name));
	else
		RETURN_IF_ERROR(name.Set(m_description.HasContent() ? m_description : m_web_handler));

	if (name.HasContent())
		RETURN_IF_ERROR(web_service_name.AppendFormat(UNI_L(" (%s)"), name.CStr()));

	if (is_formatted && IsWebHandlerBlocked(m_webmail_id))
	{
		RETURN_IF_ERROR(web_service_name.Insert(0,UNI_L("<o>")));
		RETURN_IF_ERROR(web_service_name.Append(UNI_L("</o>")));
		*is_formatted = TRUE;
	}

	return OpStatus::OK;
}

/***********************************************************************************
 ** TrustedProtocolEntry::ResetBlockedWebHandler
 **
 **
 **
 **
 ***********************************************************************************/
BOOL TrustedProtocolEntry::ResetBlockedWebHandler()
{
	if (GetViewerMode() == UseWebService && IsWebHandlerBlocked(m_webmail_id))
	{
		SetViewerMode(UseInternalApplication);
		return TRUE;
	}

	return FALSE;
}


/***********************************************************************************
 ** TrustedProtocolEntry::IsWebHandlerBlocked
 **
 **
 **
 **
 ***********************************************************************************/
BOOL TrustedProtocolEntry::IsWebHandlerBlocked(UINT32 id)
{
	if (!m_web_handler_support)
	{
		if (m_entry_type == TrustedProtocolEntry::MAILTO_ENTRY)
		{
			// We only block a handler that is added by the user
			WebmailManager::HandlerSource source;
			OP_STATUS rc = WebmailManager::GetInstance()->GetSource(id, source);
			return OpStatus::IsSuccess(rc) && source == WebmailManager::HANDLER_CUSTOM;
		}
		else
			return TRUE;
	}

	return FALSE;
}

/***********************************************************************************
 ** TrustedProtocolEntry::HasWebHandler
 **
 **
 **
 **
 ***********************************************************************************/
BOOL TrustedProtocolEntry::HasWebHandler()
{
	if (m_entry_type == TrustedProtocolEntry::MAILTO_ENTRY)
	{
		for (UINT32 i=0; i < WebmailManager::GetInstance()->GetCount(); i++)
		{
			WebmailManager::HandlerSource source;
			UINT32 id = WebmailManager::GetInstance()->GetIdByIndex(i);
			RETURN_VALUE_IF_ERROR(WebmailManager::GetInstance()->GetSource(id, source), FALSE);
			if (source == WebmailManager::HANDLER_CUSTOM)
				return TRUE;
		}
		return FALSE;
	}
	else
	{
		return m_web_handler.HasContent();
	}
}

/***********************************************************************************
 ** TrustedProtocolEntry::GetDefaultApplication
 **
 **
 **
 **
 ***********************************************************************************/
const OpStringC& TrustedProtocolEntry::GetDefaultApplication()
{
	if (m_default_application.IsEmpty())
	{
		OpString uri_string;
		g_op_system_info->GetProtocolHandler(uri_string, m_protocol, m_default_application);
	}

	return m_default_application;
}

/***********************************************************************************
 ** TrustedProtocolEntry::SetCustomParameters
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TrustedProtocolEntry::SetCustomParameters(const OpVector<OpString> & parameters)
{
	m_custom_parameters.Empty();

	for(UINT32 i = 0; i < parameters.GetCount(); i++)
	{
		RETURN_IF_ERROR(m_custom_parameters.Append(*parameters.Get(i)));
		RETURN_IF_ERROR(m_custom_parameters.Append(" "));
    }

	m_custom_parameters.Strip();
	return OpStatus::OK;
}


/***********************************************************************************
 ** TrustedProtocolEntry::GetCommand
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TrustedProtocolEntry::GetCommand(OpString & command) const
{
	// Quote the application string if it is has content and has one or more quotes inside the string
	if (GetCustomApplication().HasContent())
	{
		BOOL quote = GetCustomApplication().FindFirstOf('"') > 0;
		if (quote)
			RETURN_IF_ERROR(command.AppendFormat(UNI_L("\"%s\""), GetCustomApplication().CStr()));
		else
			RETURN_IF_ERROR(command.Append(GetCustomApplication()));
		if (GetCustomParameters().HasContent())
			RETURN_IF_ERROR(command.AppendFormat(UNI_L(" %s"), GetCustomParameters().CStr()));
		command.Strip();
	}
	return OpStatus::OK;
}


/***********************************************************************************
 ** TrustedProtocolEntry::Init
 **
 **
 **
 **
 ***********************************************************************************/

OP_STATUS TrustedProtocolEntry::Init(OpStringC protocol, OpStringC web_handler, OpStringC description, BOOL is_protocol)
{
	RETURN_IF_ERROR(SetProtocol(protocol));
	RETURN_IF_ERROR(SetWebHandler(web_handler));
	RETURN_IF_ERROR(SetDescription(description));
	SetIsProtocol(is_protocol);

	if (is_protocol)
	{
		// Verify that a web handler for this protocol is allowed
		URL url;
		OpSecurityContext source(url);
		OpSecurityContext target(url);
		target.AddText(m_protocol);
		if (OpStatus::IsError(g_secman_instance->CheckSecurity(OpSecurityManager::WEB_HANDLER_REGISTRATION, source, target, m_web_handler_support)))
			m_web_handler_support = FALSE;
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** TrustedProtocolEntry::Set
 **
 **
 **
 **
 ***********************************************************************************/

OP_STATUS TrustedProtocolEntry::Set(OpStringC filename, ViewerMode viewer_mode, BOOL in_terminal, UINT32 webmail_id)
{
	OpString custom_application;
	OpAutoVector<OpString> custom_parameters;

	RETURN_IF_ERROR(SplitCommand(filename, custom_application, custom_parameters));
	RETURN_IF_ERROR(SetCustomApplication(custom_application));
	RETURN_IF_ERROR(SetCustomParameters(custom_parameters));
	SetViewerMode(viewer_mode);
	SetInTerminal(in_terminal);
	SetWebmailId(webmail_id);

	return OpStatus::OK;
}


/***********************************************************************************
 ** TrustedProtocolEntry::SplitCommand
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TrustedProtocolEntry::SplitCommand(const OpStringC & command, OpString & handler, OpVector<OpString> & parameters)
{
	BOOL quote = command.FindFirstOf('"') != 0;

	if (quote && command.CStr())
		RETURN_IF_ERROR(handler.AppendFormat(UNI_L("\"%s\""), command.CStr()));
	else
		RETURN_IF_ERROR(handler.Set(command));

	RETURN_IF_ERROR(StringUtils::SplitString( parameters, handler, ' ', TRUE ));

	if (parameters.GetCount() > 0)
	{
		RETURN_IF_ERROR(handler.Set(*parameters.Get(0)));
		parameters.Delete(0,1);
	}
	return OpStatus::OK;
}

/***********************************************************************************
 ** TrustedProtocolEntry::JoinCommand
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TrustedProtocolEntry::JoinCommand(OpString & command, const OpStringC & handler, const OpStringC & parameters)
{
	if (handler.HasContent())
		RETURN_IF_ERROR(command.AppendFormat(UNI_L("\"%s\""), handler.CStr()));
	if (parameters.HasContent())
		RETURN_IF_ERROR(command.AppendFormat(UNI_L(" %s"), parameters.CStr()));
	return OpStatus::OK;
}

