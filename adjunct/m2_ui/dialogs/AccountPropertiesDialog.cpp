/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

# include "adjunct/m2/src/engine/accountmgr.h"
# include "adjunct/m2_ui/dialogs/SignatureEditor.h"
# include "adjunct/m2_ui/dialogs/AccountPropertiesDialog.h"
# include "adjunct/m2_ui/dialogs/OfflineMessagesDialog.h"
# include "adjunct/quick/Application.h"
# include "adjunct/quick/controller/SimpleDialogController.h"
# include "adjunct/quick/widgets/DesktopFileChooserEdit.h"
# include "adjunct/quick_toolkit/widgets/DesktopOpDropDown.h"
# include "adjunct/quick_toolkit/widgets/OpLabel.h"
# include "modules/widgets/WidgetContainer.h"
# include "modules/widgets/OpWidgetFactory.h"
# include "modules/util/handy.h" // ARRAY_SIZE
# include "modules/encodings/utility/charsetnames.h"
# include "modules/locale/oplanguagemanager.h"
# include "modules/prefs/prefsmanager/collections/pc_m2.h"


/*****************************************************************************
**
**	AccountPropertiesDialog
**
*****************************************************************************/

AccountPropertiesDialog::AccountPropertiesDialog(BOOL modality = FALSE)
	: m_modality(modality)
{
}


OP_STATUS AccountPropertiesDialog::Init(UINT16 account_id, DesktopWindow* parent_window)
{
    m_account_id = account_id;
	Dialog::Init(parent_window, 0);

	OpEdit* edit = GetWidgetByName<OpEdit>("Inpassword_edit", WIDGET_TYPE_EDIT);
	if (edit != 0)
		edit->SetPasswordMode(TRUE);

	edit = GetWidgetByName<OpEdit>("Outpassword_edit", WIDGET_TYPE_EDIT);
	if (edit != 0)
		edit->SetPasswordMode(TRUE);

	edit = GetWidgetByName<OpEdit>("Serverpass_edit", WIDGET_TYPE_EDIT);
	if (edit != 0)
		edit->SetPasswordMode(TRUE);

	const char* ltr_text_widgets[] = {
		"Mail_edit",
		"Replyto_edit",
		"Auto_cc_edit",
		"Auto_bcc_edit",
		"Inserver_edit",
		"Inusername_edit",
		"Outserver_edit",
		"Outusername_edit",
		"Rootpath_edit",
		"ConnectPerform_edit",
	};
	for (unsigned i = 0; i < ARRAY_SIZE(ltr_text_widgets); ++i)
	{
		OpEdit* edit = GetWidgetByName<OpEdit>(ltr_text_widgets[i], WIDGET_TYPE_EDIT);
		if (edit)
			edit->SetForceTextLTR(TRUE);
	}

	Account* account = 0;
	RETURN_IF_ERROR(g_m2_engine->GetAccountManager()->GetAccountById(m_account_id, account));

	if (account)
	{
		OpString edit_text;
		OpString click_to_edit_label;
		OpString8 edit_text8;

		g_languageManager->GetString(Str::D_PASSWORD_NOT_DISPLAYED,click_to_edit_label);
		SetWidgetText("Accountname_edit", account->GetAccountName().CStr());

		OpDropDown* category_dropdown = (OpDropDown*)GetWidgetByName("Category_dropdown");
		if (category_dropdown)
		{
			category_dropdown->SetEditableText(TRUE);

			AccountManager* account_manager = g_m2_engine->GetAccountManager();
			Account* account = NULL;

			OpAutoVector<OpString> categories;

			for (INT32 i = 0; i < account_manager->GetAccountCount(); i++)
			{
				account = account_manager->GetAccountByIndex(i);

				if (account)
				{
					OpString category;
					account->GetAccountCategory(category);

					if (category.HasContent())
					{
						BOOL used_already = FALSE;

						for (UINT32 j = 0; j < categories.GetCount(); j++)
						{
							if (categories.Get(j)->CompareI(category.CStr()) == 0)
							{
								used_already = TRUE;
								break;
							}
						}

						if (!used_already)
						{
							OpString* string = OP_NEW(OpString, ());
							if (string)
							{
								string->Set(category.CStr());

								categories.Add(string);

								category_dropdown->AddItem(category.CStr());
							}
						}
					}
				}
			}
		}

		account->GetAccountCategory(edit_text);
		SetWidgetText("Category_dropdown", edit_text.CStr());

		account->GetRealname(edit_text);
		SetWidgetText("Name_edit", edit_text.CStr());

		account->GetEmail(edit_text);
		SetWidgetText("Mail_edit", edit_text.CStr());

		account->GetOrganization(edit_text);
		SetWidgetText("Organization_edit", edit_text.CStr());

		account->GetReplyTo(edit_text);
		SetWidgetText("Replyto_edit", edit_text.CStr());

		account->GetAutoCC(edit_text);
		SetWidgetText("Auto_cc_edit", edit_text.CStr());

		account->GetAutoBCC(edit_text);
		SetWidgetText("Auto_bcc_edit", edit_text.CStr());

		SetWidgetText("Inserver_edit", account->GetIncomingServername().CStr());

		edit_text.Set(Account::GetProtocolName(account->GetIncomingProtocol()));

		OpString format, formatted_text;

		const AccountTypes::AccountType account_type = account->GetIncomingProtocol();
		if (account_type > AccountTypes::_FIRST_NEWS &&
			account_type < AccountTypes::_LAST_NEWS)
		{
			g_languageManager->GetString(Str::S_ACCOUNT_PROPERTIES_NNTP_SERVER, format);
		}
		else if (account_type > AccountTypes::_FIRST_CHAT &&
			account_type < AccountTypes::_LAST_CHAT)
		{
			g_languageManager->GetString(Str::S_ACCOUNT_PROPERTIES_CHAT_SERVER, format);
		}
		else
			g_languageManager->GetString(Str::S_ACCOUNT_PROPERTIES_INCOMING_SERVER, format);

		RETURN_IF_ERROR(formatted_text.AppendFormat(format.CStr(),edit_text.CStr()));
		RETURN_IF_ERROR(edit_text.Set(formatted_text));
		SetWidgetText("Incoming_label", edit_text.CStr());

		SetWidgetText("Inusername_edit", account->GetIncomingUsername().CStr());
		SetWidgetText("Nickname_edit", account->GetIncomingUsername().CStr());

		OpLabel * illegal_nick_label = (OpLabel*) GetWidgetByName("label_for_nick_warning");
		illegal_nick_label->SetForegroundColor(OP_RGB(255, 0, 0));

		OpEdit* inPasswordField = (OpEdit*)(GetWidgetByName("Inpassword_edit"));
		inPasswordField->SetGhostText(click_to_edit_label.CStr());

		UINT32 supported_authentication = account->GetIncomingAuthenticationSupported();
		OpDropDown* Inauthentication_dropdown_widget = static_cast<OpDropDown*>(GetWidgetByName("Inauthentication_dropdown"));
		PopulateAuthenticationDropdown(Inauthentication_dropdown_widget, account, supported_authentication, account->GetIncomingAuthenticationMethod());

		SetWidgetText("Outserver_edit", account->GetOutgoingServername().CStr());

		UINT16 port_no = account->GetIncomingPort();
		edit_text.Empty();
		RETURN_IF_ERROR(edit_text.AppendFormat(UNI_L("%u"), port_no));
		SetWidgetText("Inport_edit", edit_text.CStr());

		port_no = account->GetOutgoingPort();
		edit_text.Empty();
		RETURN_IF_ERROR(edit_text.AppendFormat(UNI_L("%u"), port_no));
		SetWidgetText("Outport_edit", edit_text.CStr());

		edit_text.Set(Account::GetProtocolName(account->GetOutgoingProtocol()));

		g_languageManager->GetString(Str::S_ACCOUNT_PROPERTIES_OUTGOING_SERVER, format);
		formatted_text.Empty();
		RETURN_IF_ERROR(formatted_text.AppendFormat(format.CStr(),edit_text.CStr()));
		RETURN_IF_ERROR(edit_text.Set(formatted_text));
		SetWidgetText("Outgoing_label", edit_text.CStr());
		edit_text.Empty();
		account->GetFolderPath(AccountTypes::FOLDER_ROOT, edit_text);
		SetWidgetText("Rootpath_edit", edit_text.CStr());

		OpString spam_path, trash_path;
		account->GetFolderPath(AccountTypes::FOLDER_SENT, edit_text);
		account->GetFolderPath(AccountTypes::FOLDER_SPAM, spam_path);
		account->GetFolderPath(AccountTypes::FOLDER_TRASH, trash_path);

		// Find all IMAP folders, add them to inbox, and choose the right one.
		OpString16 folderName;
		OpDropDown* Sentpath_dropdown_widget = (OpDropDown*)GetWidgetByName("Sentpath_dropdown");
		OpDropDown* Spampath_dropdown_widget = (OpDropDown*)GetWidgetByName("Spampath_dropdown");
		OpDropDown* Trashpath_dropdown_widget = (OpDropDown*)GetWidgetByName("Trashpath_dropdown");

		// First, add an empty alternative
		RETURN_IF_ERROR(Sentpath_dropdown_widget->AddItem(UNI_L("")));
		RETURN_IF_ERROR(Spampath_dropdown_widget->AddItem(UNI_L("")));
		RETURN_IF_ERROR(Trashpath_dropdown_widget->AddItem(UNI_L("")));
		// Then add all the rest
		UINT32 folderCount = account->GetSubscribedFolderCount();
		for (UINT32 folderIndex = 0; folderIndex < folderCount; folderIndex++)
		{
			INT32 got_index;
			RETURN_IF_ERROR(account->GetSubscribedFolderName(folderIndex, folderName));

			// Don't include INBOX in this list, not selectable as sent folder
			if (!folderName.Compare(UNI_L("INBOX")))
				continue;

			RETURN_IF_ERROR(Sentpath_dropdown_widget->AddItem(folderName.CStr(), -1, &got_index));
			
			// Set the current sent mail folder
			if (!folderName.Compare(edit_text))
				Sentpath_dropdown_widget->SelectItem(got_index, TRUE);

			RETURN_IF_ERROR(Spampath_dropdown_widget->AddItem(folderName.CStr(), -1, &got_index));
			
			// Set the current spam mail folder
			if (!folderName.Compare(spam_path))
				Spampath_dropdown_widget->SelectItem(got_index, TRUE);

			RETURN_IF_ERROR(Trashpath_dropdown_widget->AddItem(folderName.CStr(), -1, &got_index));
			
			// Set the current trash mail folder
			if (!folderName.Compare(trash_path))
				Trashpath_dropdown_widget->SelectItem(got_index, TRUE);
		}

		SetWidgetValue("Disable_spamfilter_for_account_checkbox", account->GetServerHasSpamFilter());

		SetWidgetText("Outusername_edit", account->GetOutgoingUsername().CStr());

		OpEdit* outPasswordField = (OpEdit*)(GetWidgetByName("Outpassword_edit"));
		outPasswordField->SetGhostText(click_to_edit_label.CStr());

		supported_authentication = account->GetOutgoingAuthenticationSupported();
		OpDropDown* Outauthentication_dropdown_widget = static_cast<OpDropDown*>(GetWidgetByName("Outauthentication_dropdown"));
		PopulateAuthenticationDropdown(Outauthentication_dropdown_widget, account, supported_authentication, account->GetOutgoingAuthenticationMethod());

		SetWidgetValue("Insecure_checkbox", account->GetUseSecureConnectionIn());
		SetWidgetValue("Outsecure_checkbox", account->GetUseSecureConnectionOut());
		SetWidgetValue("Leaveonserver_checkbox", account->GetLeaveOnServer(TRUE));
		SetWidgetValue("Delete_permanently_checkbox", account->GetPermanentDelete());
		SetWidgetValue("Checkevery_checkbox", account->GetPollInterval() != 0);
		SetWidgetValue("Delayed_server_remove_checkbox", account->IsDelayedRemoveFromServerEnabled());
		SetWidgetValue("Only_if_marked_read_checkbox",account->IsRemoveFromServerOnlyIfMarkedAsRead());
		SetWidgetValue("Only_if_full_local_body_checkbox",account->IsRemoveFromServerOnlyIfCompleteMessage());
		
		UINT32 removal_delay = account->GetRemoveFromServerDelay();
		if (removal_delay)
		{
			removal_delay = removal_delay / (60*60*24);
		}
		else
		{
			removal_delay = 7;
		}

		edit_text.Empty();
		RETURN_IF_ERROR(edit_text.AppendFormat(UNI_L("%d"),removal_delay));
		SetWidgetText("Server_remove_delay",edit_text.CStr());

		SetWidgetEnabled("Delayed_server_remove_checkbox",account->GetLeaveOnServer());
		SetWidgetEnabled("Server_remove_delay",account->GetLeaveOnServer() && account->IsDelayedRemoveFromServerEnabled());
		SetWidgetEnabled("Delete_permanently_checkbox", account->GetLeaveOnServer());
		SetWidgetEnabled("Only_if_marked_read_checkbox",account->GetLeaveOnServer() && account->IsDelayedRemoveFromServerEnabled());
		SetWidgetEnabled("Only_if_full_local_body_checkbox",account->GetLeaveOnServer() && account->IsDelayedRemoveFromServerEnabled());
		UINT32 check_every = account->GetPollInterval();

		if (check_every)
		{
			check_every = check_every / 60;
		}
		else
		{
			check_every = 5;
		}

		edit_text.Empty();
		RETURN_IF_ERROR(edit_text.AppendFormat(UNI_L("%d"),check_every));
		SetWidgetText("Checkevery_edit", edit_text.CStr());
		SetWidgetEnabled("Checkevery_edit",GetWidgetValue("Checkevery_checkbox"));
		// Offlinemessages_dropdown
		OpDropDown* offline_dropdown = static_cast<OpDropDown*>(GetWidgetByName("Offlinemessages_dropdown"));
		if (offline_dropdown)
		{
			OpString offline_string;
			int got_index;
			int current_flags = 0;
			int select_item = 0;

			if (account->GetDownloadBodies(TRUE))
				current_flags |= DOWNLOAD_BODY;
			if (account->GetDefaultStore(TRUE) != 0)
				current_flags |= KEEP_BODY;

			// Add available options
			g_languageManager->GetString(Str::D_MAIL_DONT_MAKE_MESSAGES_AVAILABLE_OFFLINE, offline_string);
			offline_dropdown->AddItem(offline_string.CStr(), -1, &got_index, 0);
			if (current_flags == 0)
				select_item = got_index;

			g_languageManager->GetString(Str::D_MAIL_MAKE_MESSAGES_AVAILABLE_OFFLINE_ON_CLICK, offline_string);
			offline_dropdown->AddItem(offline_string.CStr(), -1, &got_index, KEEP_BODY);
			if (current_flags == KEEP_BODY)
				select_item = got_index;

			g_languageManager->GetString(Str::D_MAIL_MAKE_ALL_MESSAGES_AVAILABLE_OFFLINE, offline_string);
			offline_dropdown->AddItem(offline_string.CStr(), -1, &got_index, (KEEP_BODY | DOWNLOAD_BODY));
			if (current_flags == (KEEP_BODY | DOWNLOAD_BODY))
				select_item = got_index;

			offline_dropdown->SelectItem(select_item, TRUE);
		}

		// HTML_composing_dropdown
		DesktopOpDropDown* html_composing_dropdown = static_cast<DesktopOpDropDown*>(GetWidgetByName("HTML_composing_dropdown"));
		if (html_composing_dropdown)
		{
			OpString text;
			int got_index;
			int select_item = 0;

			// Add available options
			g_languageManager->GetString(Str::D_HTML_MAIL_PREFER_FORMATTED_COMPOSING, text);
			html_composing_dropdown->AddItem(text.CStr(), -1, &got_index, Message::PREFER_TEXT_HTML);
			
			g_languageManager->GetString(Str::M_PREFER_PLAIN_TEXT, text);
			html_composing_dropdown->AddItem(text.CStr(), -1, &got_index, Message::PREFER_TEXT_PLAIN);
			if (account->PreferHTMLComposing() == Message::PREFER_TEXT_PLAIN)
				select_item = got_index;
			
			g_languageManager->GetString(Str::D_PREFER_PLAIN_TEXT_FOLLOW_ORIG_FORMATTING, text);
			html_composing_dropdown->AddItem(text.CStr(), -1, &got_index, Message::PREFER_TEXT_PLAIN_BUT_REPLY_WITH_SAME_FORMATTING);
			if (account->PreferHTMLComposing() == Message::PREFER_TEXT_PLAIN_BUT_REPLY_WITH_SAME_FORMATTING)
				select_item = got_index;
			
			html_composing_dropdown->SelectItem(select_item, TRUE);
		}

		SetWidgetValue("Lowbandwidthmode_checkbox", account->GetLowBandwidthMode());
		SetWidgetValue("Readifseen_checkbox", account->GetMarkReadIfSeen());
		SetWidgetValue("Dontfetchattachments_checkbox", account->GetFetchOnlyText(TRUE));		
		SetWidgetValue("Wordwrap_checkbox", account->GetLinelength() > 0);
		SetWidgetValue("Queuemessages_checkbox", account->GetQueueOutgoing());
		SetWidgetValue("Sendaftercheck_checkbox", account->GetSendQueuedAfterChecking());
		SetWidgetValue("Addcontact_checkbox", account->GetAddContactWhenSending());
		SetWidgetValue("Subjectwarning_checkbox", account->GetWarnIfEmptySubject());
		SetWidgetValue("Playsound_checkbox", account->GetSoundEnabled());
		SetWidgetValue("Checkmanually_checkbox", account->GetManualCheckEnabled());
		
		if (account->GetLowBandwidthMode())
		{
			SetWidgetEnabled("Offlinemessages_dropdown", FALSE);
			SetWidgetEnabled("Dontfetchattachments_checkbox", FALSE);
			SetWidgetEnabled("Leaveonserver_checkbox", FALSE);
		}

		if (!account->GetQueueOutgoing())
		{
			SetWidgetEnabled("Sendaftercheck_checkbox",FALSE);
		}

		DesktopFileChooserEdit* chooser = (DesktopFileChooserEdit*) GetWidgetByName("Playsound_chooser");
		if (chooser)
		{
			OpString file;
			account->GetSoundFile(file);
			chooser->SetText(file.CStr());

			OpString caption;
			g_languageManager->GetString(Str::D_SELECT_SOUND_FILE, caption);

			chooser->SetTitle(caption.CStr());

			OpString filter;
			g_languageManager->GetString(Str::SI_SOUND_TYPES,filter);
			chooser->SetFilterString(filter.CStr());
		}

		UINT16 dcc_poolport_no = account->GetStartOfDccPool();
		edit_text.Empty();
		RETURN_IF_ERROR(edit_text.AppendFormat(UNI_L("%u"),dcc_poolport_no));
		SetWidgetText("DccPortpoolstart_edit", edit_text.CStr());

		dcc_poolport_no = account->GetEndOfDccPool();
		edit_text.Empty();
		RETURN_IF_ERROR(edit_text.AppendFormat(UNI_L("%u"),dcc_poolport_no));
		SetWidgetText("DccPortpoolend_edit", edit_text.CStr());

		SetWidgetValue("DccIncomingConnections_checkbox", account->GetCanAcceptIncomingConnections());
		SetWidgetValue("PrivateChatInBackground_checkbox", account->GetOpenPrivateChatsInBackground());

		OpString perform_commands;
		account->GetPerformWhenConnected(perform_commands);

		SetWidgetText("ConnectPerform_edit", perform_commands.CStr());

		OpEdit* IRCPasswordField = (OpEdit*)(GetWidgetByName("Serverpass_edit"));
		IRCPasswordField->SetGhostText(click_to_edit_label.CStr());
	}

	//Fill the charset dropdown

	OpString default_encoding;
	g_languageManager->GetString(Str::S_DEFAULT_ENCODING, default_encoding);

	SetWidgetText("label_for_Outcharset_dropdown", default_encoding.CStr());
	OpDropDown* charsets = (OpDropDown*) GetWidgetByName("Outcharset_dropdown");
	const char* const* charsets_array =
		g_charsetManager->GetCharsetNames();
	if (charsets && charsets_array)
	{
		OpString account_charset;
		if (account)
		{
			OpString8 account_charset8;
			if (account->GetCharset(account_charset8)==OpStatus::OK)
				account_charset.Set(account_charset8);
		}

		if (account_charset.IsEmpty())
			account_charset.Set(UNI_L("utf-8"));

		int index = 0, default_index = -1;
		INT32 tmp_index;
		OpString tmp_item;
		while (charsets_array[index] && tmp_item.Set(charsets_array[index])==OpStatus::OK)
		{
			if (tmp_item.CompareI(UNI_L("UTF-16"))!=0 &&
				tmp_item.CompareI(UNI_L("UTF-32"))!=0)
			{
				charsets->AddItem(tmp_item.CStr(), -1, &tmp_index);

				if (account_charset.CompareI(tmp_item)==0)
				{
					default_index = tmp_index;
				}
			}

			index++;
		}

		if (default_index != -1)
		{
			charsets->SelectItem(default_index, TRUE);
		}
	}

	// Fill the text direction dropdown
	OpDropDown* direction = (OpDropDown*) GetWidgetByName("Direction_dropdown");
	if (direction)
	{
		OpString direction_text;

		RETURN_IF_ERROR(g_languageManager->GetString(Str::D_MAIL_DIRECTION_AUTOMATIC, direction_text));
		RETURN_IF_ERROR(direction->AddItem(direction_text.CStr())); // DIR_AUTOMATIC
		RETURN_IF_ERROR(g_languageManager->GetString(Str::D_MAIL_DIRECTION_LEFT_TO_RIGHT, direction_text));
		RETURN_IF_ERROR(direction->AddItem(direction_text.CStr())); // DIR_LEFT_TO_RIGHT
		RETURN_IF_ERROR(g_languageManager->GetString(Str::D_MAIL_DIRECTION_RIGHT_TO_LEFT, direction_text));
		RETURN_IF_ERROR(direction->AddItem(direction_text.CStr())); // DIR_RIGHT_TO_LEFT

		if (account)
			direction->SelectItem(account->GetDefaultDirection(), TRUE);
	}

	SetWidgetFocus("Accountname_edit");

    return OpStatus::OK;
}


UINT32 AccountPropertiesDialog::OnOk()
{

	Account* account;
	RETURN_IF_ERROR(g_m2_engine->GetAccountManager()->GetAccountById(m_account_id, account));

	if (account == NULL)
	{
		return 0;
	}

	OpString edit_text;

	GetWidgetText("Accountname_edit", edit_text);
	account->SetAccountName(edit_text);

	GetWidgetText("Category_dropdown", edit_text);
	account->SetAccountCategory(edit_text);

	GetWidgetText("Name_edit", edit_text);
	account->SetRealname(edit_text);

	GetWidgetText("Mail_edit", edit_text);
	account->SetEmail(edit_text);

	GetWidgetText("Organization_edit", edit_text);
	account->SetOrganization(edit_text);

	GetWidgetText("Replyto_edit", edit_text);
	account->SetReplyTo(edit_text);

	GetWidgetText("Auto_cc_edit", edit_text);
	account->SetAutoCC(edit_text);

	GetWidgetText("Auto_bcc_edit", edit_text);
	account->SetAutoBCC(edit_text);

	BOOL changed_servername = FALSE;
	GetWidgetText("Inserver_edit", edit_text);
	if (account->GetIncomingServername().Compare(edit_text.CStr()) != 0)
		changed_servername = TRUE;
	
	account->SetIncomingServername(edit_text);
	

	GetWidgetText("Inusername_edit", edit_text);
	account->SetIncomingUsername(edit_text);

	GetWidgetText("Nickname_edit", edit_text);
	if (edit_text.HasContent() && GetWidgetByName("Nickname_edit")->IsVisible())
		account->SetIncomingUsername(edit_text);

	OpString ghost_text;
	OpEdit* in_password_edit = ((OpEdit*)(GetWidgetByName("Inpassword_edit")));
	if(in_password_edit)
		in_password_edit->GetGhostText(ghost_text);

	//only save the password if it has changed (not changed when the ghost text is displayed)
	if (ghost_text.IsEmpty() || changed_servername)
	{
		in_password_edit->GetText(edit_text);
		if (ghost_text.IsEmpty())
		{
			account->SetIncomingPassword(edit_text);
		}
		account->SetSaveIncomingPassword(TRUE);
	}
	
	changed_servername = FALSE;
	// Server name and user name has to be set before setting a new password (Wand uses those two to store a password)
	GetWidgetText("Outserver_edit", edit_text);
	if (account->GetOutgoingServername().Compare(edit_text.CStr()) != 0)
		changed_servername = TRUE;
	account->SetOutgoingServername(edit_text);

	GetWidgetText("Outusername_edit", edit_text);
	account->SetOutgoingUsername(edit_text);

	OpEdit * out_password_edit = ((OpEdit*)(GetWidgetByName("Outpassword_edit")));
	if(out_password_edit)
		out_password_edit->GetGhostText(ghost_text);
	if (IsWidgetEnabled("Outpassword_edit") && ((ghost_text.IsEmpty()) || changed_servername))
	{
		out_password_edit->GetText(edit_text);
		if (ghost_text.IsEmpty())
			account->SetOutgoingPassword(edit_text);
		account->SetSaveOutgoingPassword(TRUE);
	}

	GetWidgetText("Rootpath_edit", edit_text);
	account->SetFolderPath(AccountTypes::FOLDER_ROOT, edit_text);

	// This setting must be done before setting sent folder (since it affects if contacts will be added)
	account->SetAddContactWhenSending(GetWidgetValue("Addcontact_checkbox"));

	OpDropDown* dropdown = (OpDropDown*)GetWidgetByName("Sentpath_dropdown");
	account->SetFolderPath(AccountTypes::FOLDER_SENT, dropdown->GetItemText(dropdown->GetSelectedItem()));
	dropdown = (OpDropDown*)GetWidgetByName("Spampath_dropdown");
	account->SetFolderPath(AccountTypes::FOLDER_SPAM, dropdown->GetItemText(dropdown->GetSelectedItem()));
	dropdown = (OpDropDown*)GetWidgetByName("Trashpath_dropdown");
	account->SetFolderPath(AccountTypes::FOLDER_TRASH, dropdown->GetItemText(dropdown->GetSelectedItem()));

	account->SetServerHasSpamFilter(GetWidgetValue("Disable_spamfilter_for_account_checkbox", FALSE));

	GetWidgetText("Server_remove_delay", edit_text);
	int removal_delay = uni_atoi(edit_text.CStr());
	
	if (removal_delay)
	{
		removal_delay = removal_delay * 60 * 60 * 24;
	}

	account->SetRemoveFromServerDelay(removal_delay);
	account->SetDelayedRemoveFromServerEnabled(GetWidgetValue("Delayed_server_remove_checkbox"));
	account->SetRemoveFromServerOnlyIfCompleteMessage(GetWidgetValue("Only_if_full_local_body_checkbox"));
	account->SetRemoveFromServerOnlyIfMarkedAsRead(GetWidgetValue("Only_if_marked_read_checkbox"));
	
	GetWidgetText("Checkevery_edit", edit_text);
	int check_every = uni_atoi(edit_text.CStr());

	if (check_every)
	{
		check_every = check_every * 60;
	}

	if (!GetWidgetValue("Checkevery_checkbox"))
	{
		check_every = 0;
	}

	account->SetPollInterval(check_every);

	GetWidgetText("Outport_edit", edit_text);
	int outgoing_port = uni_atoi(edit_text.CStr());

	account->SetOutgoingPort(outgoing_port);

	GetWidgetText("Inport_edit", edit_text);
	int incoming_port = uni_atoi(edit_text.CStr());

	account->SetIncomingPort(incoming_port);

	account->SetUseSecureConnectionIn(GetWidgetValue("Insecure_checkbox"));
	account->SetUseSecureConnectionOut(GetWidgetValue("Outsecure_checkbox"));

	OpDropDown* authentication_dropdown_widget = (OpDropDown*)GetWidgetByName("Inauthentication_dropdown");
	if (authentication_dropdown_widget)
	{
		AccountTypes::AuthenticationType method = (AccountTypes::AuthenticationType)(authentication_dropdown_widget->GetItemUserData(authentication_dropdown_widget->GetSelectedItem()));
		account->SetIncomingAuthenticationMethod(method);
	}
	authentication_dropdown_widget = (OpDropDown*)GetWidgetByName("Outauthentication_dropdown");
	if (authentication_dropdown_widget)
	{
		AccountTypes::AuthenticationType method = (AccountTypes::AuthenticationType)(authentication_dropdown_widget->GetItemUserData(authentication_dropdown_widget->GetSelectedItem()));
		account->SetOutgoingAuthenticationMethod(method);
	}

	int offline_flags = 0;
	OpDropDown* offline_dropdown = static_cast<OpDropDown*>(GetWidgetByName("Offlinemessages_dropdown"));

	if (offline_dropdown && offline_dropdown->GetSelectedItem() >= 0)
		offline_flags = offline_dropdown->GetItemUserData(offline_dropdown->GetSelectedItem());

	// If we change to making all messages available offline, download messages without a body
	int offline_flags_old = GetOfflineFlags(account);
	if (offline_flags != offline_flags_old && offline_flags == (KEEP_BODY | DOWNLOAD_BODY))
	{
		OfflineMessagesDialog* dialog = OP_NEW(OfflineMessagesDialog, ());
		if (dialog)
			dialog->Init(GetParentDesktopWindow(), account->GetAccountId());
	}

	DesktopOpDropDown* html_formatting_dropdown = static_cast<DesktopOpDropDown*>(GetWidgetByName("HTML_composing_dropdown"));
	if (html_formatting_dropdown)
	{
		account->SetPreferHTMLComposing(static_cast<Message::TextpartSetting>(html_formatting_dropdown->GetItemUserData(html_formatting_dropdown->GetSelectedItem())));
	}

	account->SetLeaveOnServer(GetWidgetValue("Leaveonserver_checkbox"));
	account->SetPermanentDelete(GetWidgetValue("Delete_permanently_checkbox"));
	account->SetLowBandwidthMode(GetWidgetValue("Lowbandwidthmode_checkbox"));
	account->SetDownloadBodies(offline_flags & DOWNLOAD_BODY);
	account->SetFetchOnlyText(GetWidgetValue("Dontfetchattachments_checkbox"));
	account->SetDefaultStore((offline_flags & KEEP_BODY) ? g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailStoreType) : AccountTypes::MBOX_NONE);
	account->SetLinelength(GetWidgetValue("Wordwrap_checkbox") ? 76 : -1);
	account->SetMarkReadIfSeen(GetWidgetValue("Readifseen_checkbox"));
	account->SetQueueOutgoing(GetWidgetValue("Queuemessages_checkbox"));
	account->SetWarnIfEmptySubject(GetWidgetValue("Subjectwarning_checkbox"));
	account->SetSoundEnabled(GetWidgetValue("Playsound_checkbox"));
	account->SetManualCheckEnabled(GetWidgetValue("Checkmanually_checkbox"));

	if (IsWidgetEnabled("Sendaftercheck_checkbox"))
		account->SetSendQueuedAfterChecking(GetWidgetValue("Sendaftercheck_checkbox"));
	else
		account->SetSendQueuedAfterChecking(FALSE);

	GetWidgetText("Playsound_chooser", edit_text);

	// strip "filename" quotes if any
	if (edit_text.HasContent() && edit_text[0] == '"')
	{
		edit_text.Set((uni_char*)(edit_text.CStr()+1));
	}
	if (edit_text.HasContent() && edit_text[edit_text.Length()-1] == '"')
	{
		edit_text[edit_text.Length()-1] = '\0';
	}
	account->SetSoundFile(edit_text);

	//Get charset
	OpDropDown* charsets = (OpDropDown*) GetWidgetByName("Outcharset_dropdown");
	INT32 selected_charset = charsets ? charsets->GetSelectedItem() : -1;
	if (selected_charset != -1)
	{
		OpStringItem* charset_item = charsets->ih.GetItemAtIndex(selected_charset);
		if (charset_item)
		{
			const uni_char* charset_string = charset_item->string.Get();
			if (charset_string && *charset_string)
			{
				OpString8 charset_string8;
				if (charset_string8.Set(charset_string)==OpStatus::OK)
				{
					account->SetCharset(charset_string8);
				}
			}
		}
	}

	OpDropDown* direction = (OpDropDown*) GetWidgetByName("Direction_dropdown");
	AccountTypes::TextDirection default_direction = direction ? (AccountTypes::TextDirection)direction->GetSelectedItem() : AccountTypes::DIR_AUTOMATIC;
	account->SetDefaultDirection(default_direction);

	GetWidgetText("DccPortpoolstart_edit", edit_text);
	int pool_port = uni_atoi(edit_text.CStr());
	account->SetStartOfDccPool(pool_port);

	GetWidgetText("DccPortpoolend_edit", edit_text);
	pool_port = uni_atoi(edit_text.CStr());
	account->SetEndOfDccPool(pool_port);

	const BOOL can_accept_incoming_connections = GetWidgetValue("DccIncomingConnections_checkbox");
	account->SetCanAcceptIncomingConnections(can_accept_incoming_connections);

	const BOOL open_in_background = GetWidgetValue("PrivateChatInBackground_checkbox");
	account->SetOpenPrivateChatsInBackground(open_in_background);

	GetWidgetText("ConnectPerform_edit", edit_text);
	account->SetPerformWhenConnected(edit_text);

	if (GetWidgetByName("Serverpass_edit") &&
		GetWidgetByName("Serverpass_edit")->IsVisible())
	{
		OpEdit * server_pass_edit=((OpEdit*)(GetWidgetByName("Serverpass_edit")));
		if(server_pass_edit)
			server_pass_edit->GetGhostText(ghost_text);
		if (ghost_text.IsEmpty())
		{
			server_pass_edit->GetText(edit_text);
			account->SetIncomingPassword(edit_text);
			account->SetSaveIncomingPassword(TRUE);
		}
	}

    account->SetIsTemporary(FALSE);

	account->SaveSettings(TRUE);

	account->SettingsChanged();

	g_application->SettingsChanged(SETTINGS_ACCOUNT_SELECTOR);

	return 0;
}


void AccountPropertiesDialog::OnCancel()
{
}


void AccountPropertiesDialog::OnClick(OpWidget *widget, UINT32 id)
{
	if (widget->IsNamed("Insecure_checkbox") || widget->IsNamed("Outsecure_checkbox"))
	{
	    Account* account_ptr;
	    g_m2_engine->GetAccountManager()->GetAccountById(m_account_id, account_ptr);
        if (!account_ptr)
			return;

        //Find old port-number
        BOOL incoming = widget->IsNamed("Insecure_checkbox");
	    OpString old_port_text;
        GetWidgetText(incoming ? "Inport_edit" : "Outport_edit", old_port_text);
    	int old_port = uni_atoi(old_port_text.CStr());

        //Find default values
        BOOL secure = (widget->GetValue() != 0);
        UINT16 old_default_port = incoming ? account_ptr->GetDefaultIncomingPort(!secure) : account_ptr->GetDefaultOutgoingPort(!secure);
        UINT16 new_default_port = incoming ? account_ptr->GetDefaultIncomingPort(secure)  : account_ptr->GetDefaultOutgoingPort(secure);

        //Update port-number if needed
        if (old_port==old_default_port && old_port!=new_default_port)
        {
            OpString port_str;
            if (port_str.Reserve(7))
            {
    	        uni_sprintf(port_str.CStr(), UNI_L("%u"), new_default_port);
		        SetWidgetText(incoming ? "Inport_edit" : "Outport_edit", port_str.CStr());
            }
        }

		return;
    }

    Dialog::OnClick(widget, id);
}


void AccountPropertiesDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget->IsNamed("Queuemessages_checkbox"))
	{
		if (widget->GetValue() == TRUE)
		{
			SetWidgetEnabled("Sendaftercheck_checkbox",TRUE);
		}
		else
		{
			SetWidgetEnabled("Sendaftercheck_checkbox",FALSE);
			SetWidgetValue("Sendaftercheck_checkbox",FALSE);
		}
	}
	else if (widget->IsNamed("Inauthentication_dropdown"))
	{
        AccountTypes::AuthenticationType method = (AccountTypes::AuthenticationType)((UINTPTR)(((OpDropDown*)widget)->GetItemUserData(((OpDropDown*)widget)->GetSelectedItem())));
		SetWidgetEnabled("Inusername_edit", method != AccountTypes::NONE);
		SetWidgetEnabled("Inpassword_edit", method != AccountTypes::NONE);
	}
	else if (widget->IsNamed("Outauthentication_dropdown"))
	{
        AccountTypes::AuthenticationType method = (AccountTypes::AuthenticationType)((UINTPTR)(((OpDropDown*)widget)->GetItemUserData(((OpDropDown*)widget)->GetSelectedItem())));
		SetWidgetEnabled("Outusername_edit", method != AccountTypes::NONE);
		SetWidgetEnabled("Outpassword_edit", method != AccountTypes::NONE);
	}
	else if (widget->IsNamed("Inpassword_edit")|| widget->IsNamed("Outpassword_edit")||widget->IsNamed("Serverpass_edit"))
	{
		((OpEdit*)widget)->SetGhostText(UNI_L(""));
	}
	else if (widget->IsNamed("Lowbandwidthmode_checkbox") || widget->IsNamed(""))
	{
		BOOL low_bandwidth = GetWidgetValue("Lowbandwidthmode_checkbox");
		BOOL leave_on_server = GetWidgetValue("Leaveonserver_checkbox");

		SetWidgetEnabled("Offlinemessages_dropdown", !low_bandwidth);
		SetWidgetEnabled("Dontfetchattachments_checkbox", !low_bandwidth);
		SetWidgetEnabled("Leaveonserver_checkbox", !low_bandwidth);
		SetWidgetEnabled("Delayed_server_remove_checkbox",low_bandwidth || leave_on_server);
		SetWidgetEnabled("Server_remove_delay",low_bandwidth || leave_on_server);
		if (widget->IsNamed("Lowbandwidthmode_checkbox") && low_bandwidth)
			SetWidgetValue("Only_if_full_local_body_checkbox",TRUE);
	}
	else if (widget->IsNamed("Checkevery_checkbox"))
	{
		SetWidgetEnabled("Checkevery_edit",GetWidgetValue("Checkevery_checkbox"));
	}
	else if (widget->IsNamed("Leaveonserver_checkbox"))
	{
		BOOL leave_on_server = GetWidgetValue("Leaveonserver_checkbox") || GetWidgetValue("Lowbandwidthmode_checkbox");
		BOOL daxd_enabled = GetWidgetValue("Delayed_server_remove_checkbox");
		// Leave on server options:
		SetWidgetEnabled("Delayed_server_remove_checkbox", leave_on_server);
		SetWidgetEnabled("Delete_permanently_checkbox", leave_on_server);
		// Delete after X days options:
		SetWidgetEnabled("Server_remove_delay",daxd_enabled && leave_on_server);
		SetWidgetEnabled("Only_if_marked_read_checkbox",daxd_enabled && leave_on_server);
		SetWidgetEnabled("Only_if_full_local_body_checkbox",daxd_enabled && leave_on_server);
	}
	else if (widget->IsNamed("Delayed_server_remove_checkbox"))
	{
		BOOL daxd_enabled = GetWidgetValue("Delayed_server_remove_checkbox");
		SetWidgetEnabled("Server_remove_delay",daxd_enabled);
		SetWidgetEnabled("Only_if_marked_read_checkbox",daxd_enabled);
		SetWidgetEnabled("Only_if_full_local_body_checkbox",daxd_enabled);
	}
	else if (widget->IsNamed("Only_if_full_local_body_checkbox"))
	{
		if (GetWidgetValue("Lowbandwidthmode_checkbox") && !GetWidgetValue("Only_if_full_local_body_checkbox"))
		{
			SimpleDialogController* controller = OP_NEW(SimpleDialogController,	(SimpleDialogController::TYPE_OK,SimpleDialogController::IMAGE_WARNING,
				 WINDOW_NAME_POP_REMOVE,Str::D_MAIL_WARNING_LOW_BANDWIDTH_AND_REMOVE_FROM_SERVER,Str::SI_IDSTR_MSG_MAILFREPFS_INVALID_SETTINGS_CAP));
			if (controller)
			{
				controller->SetHelpAnchor(UNI_L("mail.html#low-bandwidth"));
				OpStatus::Ignore(ShowDialog(controller,g_global_ui_context,this));
			}
		}
	}
	Dialog::OnChange(widget, changed_by_mouse);
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL AccountPropertiesDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			OP_ASSERT(child_action != 0);

			switch (child_action->GetAction())
			{
			case OpInputAction::ACTION_OK:
				{
					OpString nick_name;
					GetWidgetText("Nickname_edit",nick_name);
					OpString8 nick8;
					nick8.Set(nick_name.CStr());
					ShowWidget("label_for_nick_warning", !!nick_name.Compare(nick8));
					child_action->SetEnabled(!nick_name.Compare(nick8));
					return TRUE;
				}
			case OpInputAction::ACTION_OPEN_SIGNATURE_DIALOG:
				{
					if (g_m2_engine->GetAccountById(m_account_id)->GetOutgoingProtocol() == AccountTypes::SMTP)
					{
						child_action->SetEnabled(TRUE);
						return TRUE;
					}
				}
			default:
				break;
			}

			break;
		}
		case OpInputAction::ACTION_OPEN_SIGNATURE_DIALOG:
			{
				if (g_m2_engine->GetAccountById(m_account_id)->GetOutgoingProtocol() == AccountTypes::SMTP)
				{
					SignatureEditor * signature_editor = OP_NEW(SignatureEditor,());
					signature_editor->Init(this, m_account_id);
					return TRUE;
				}
			}
	}
	return Dialog::OnInputAction(action);
}

void AccountPropertiesDialog::OnInitVisibility()
{
	Account* account;
	g_m2_engine->GetAccountManager()->GetAccountById(m_account_id, account);
	OP_ASSERT(account != 0);

	if (account->GetIncomingProtocol() != AccountTypes::IRC)
	{
		ShowWidget("label_for_Nickname_edit", FALSE);
		ShowWidget("Nickname_edit", FALSE);
		ShowWidget("label_for_Serverpass_edit", FALSE);
		ShowWidget("Serverpass_edit", FALSE);
		ShowWidget("label_for_DccPortpoolstart_edit", FALSE);
		ShowWidget("DccPortpoolstart_edit", FALSE);
		ShowWidget("label_for_DccPortpoolend_edit", FALSE);
		ShowWidget("DccPortpoolend_edit", FALSE);
		ShowWidget("DccIncomingConnections_checkbox", FALSE);
		ShowWidget("PrivateChatInBackground_checkbox", FALSE);
		ShowWidget("label_for_ConnectPerform_edit", FALSE);
		ShowWidget("ConnectPerform_edit", FALSE);
		ShowWidget("label_for_nick_warning", FALSE);
	}

	switch (account->GetIncomingProtocol())
	{
		case AccountTypes::NEWS:
			ShowWidget("Lowbandwidthmode_checkbox", FALSE);
			ShowWidget("Leaveonserver_checkbox", FALSE);
			ShowWidget("Delayed_server_remove_checkbox",FALSE);
			ShowWidget("Server_remove_delay",FALSE);
			ShowWidget("Delete_permanently_checkbox", FALSE);
			ShowWidget("Readifseen_checkbox", FALSE);
			ShowWidget("Rootpath_edit", FALSE);
			ShowWidget("label_for_Rootpath_edit", FALSE);
			ShowWidget("Sentpath_dropdown", FALSE);
			ShowWidget("label_for_Sentpath_dropdown", FALSE);
			ShowWidget("Dontfetchattachments_checkbox", FALSE);
			ShowWidget("Only_if_marked_read_checkbox",FALSE);
			ShowWidget("Only_if_full_local_body_checkbox",FALSE);
			ShowWidget("tab_account_IMAP",FALSE);
			ShowWidget("HTML_composing_dropdown", FALSE);
			break;

		case AccountTypes::IRC:
			ShowWidget("Lowbandwidthmode_checkbox", FALSE);
			ShowWidget("Readifseen_checkbox", FALSE);
			ShowWidget("Leaveonserver_checkbox", FALSE);
			ShowWidget("label_for_Replyto_edit", FALSE);
			ShowWidget("Replyto_edit", FALSE);
			ShowWidget("label_for_Auto_cc_edit", FALSE);
			ShowWidget("Auto_cc_edit", FALSE);
			ShowWidget("label_for_Auto_bcc_edit", FALSE);
			ShowWidget("Auto_bcc_edit", FALSE);
			ShowWidget("label_for_Inusername_edit", FALSE);
			ShowWidget("Inusername_edit", FALSE);
			ShowWidget("label_for_Inauthentication_dropdown", FALSE);
			ShowWidget("Inauthentication_dropdown", FALSE);
			ShowWidget("label_for_Inpassword_edit", FALSE);
			ShowWidget("Inpassword_edit", FALSE);
			ShowWidget("Outgoing_label", FALSE);
			ShowWidget("label_for_Outserver_edit", FALSE);
			ShowWidget("Outserver_edit", FALSE);
			ShowWidget("label_for_Outport_edit", FALSE);
			ShowWidget("Outport_edit", FALSE);
			ShowWidget("Outsecure_checkbox", FALSE);
			ShowWidget("label_for_Outauthentication_dropdown", FALSE);
			ShowWidget("Outauthentication_dropdown", FALSE);
			ShowWidget("label_for_Outusername_edit", FALSE);
			ShowWidget("Outusername_edit", FALSE);
			ShowWidget("label_for_Outpassword_edit", FALSE);
			ShowWidget("Outpassword_edit", FALSE);
			ShowWidget("Readifseen_checkbox", FALSE);
			ShowWidget("Label_for_Offlinemessages_dropdown", FALSE);
			ShowWidget("Offlinemessages_dropdown", FALSE);
			ShowWidget("Checkevery_checkbox", FALSE);
			ShowWidget("Delayed_server_remove_checkbox",FALSE);
			ShowWidget("Server_remove_delay",FALSE);
			ShowWidget("Delete_permanently_checkbox", FALSE);
			ShowWidget("Checkevery_edit", FALSE);
			ShowWidget("Checkmanually_checkbox", FALSE);
			ShowWidget("Playsound_checkbox", FALSE);
			ShowWidget("Playsound_chooser", FALSE);
			ShowWidget("Rootpath_edit", FALSE);
			ShowWidget("label_for_Rootpath_edit", FALSE);
			ShowWidget("Wordwrap_checkbox", FALSE);
			ShowWidget("Queuemessages_checkbox", FALSE);
			ShowWidget("Sendaftercheck_checkbox", FALSE);
			ShowWidget("Addcontact_checkbox", FALSE);
			ShowWidget("Subjectwarning_checkbox", FALSE);
			ShowWidget("label_for_Signature_edit", FALSE);
			ShowWidget("Signature_edit", FALSE);
			ShowWidget("label_for_Sentpath_dropdown", FALSE);
			ShowWidget("Sentpath_dropdown", FALSE);
			ShowWidget("Dontfetchattachments_checkbox", FALSE);
			ShowWidget("Only_if_marked_read_checkbox",FALSE);
			ShowWidget("Only_if_full_local_body_checkbox",FALSE);
			ShowWidget("tab_account_IMAP",FALSE);
			ShowWidget("HTML_composing_dropdown", FALSE);
			break;

		case AccountTypes::IMAP:
			ShowWidget("Leaveonserver_checkbox", FALSE);
			ShowWidget("Delayed_server_remove_checkbox",FALSE);
			ShowWidget("Server_remove_delay",FALSE);
			ShowWidget("Delete_permanently_checkbox", FALSE);
			ShowWidget("Readifseen_checkbox", FALSE);
			ShowWidget("Sendaftercheck_checkbox",FALSE);
			ShowWidget("Only_if_marked_read_checkbox",FALSE);
			ShowWidget("Only_if_full_local_body_checkbox",FALSE);
			break;

		default:
			ShowWidget("Dontfetchattachments_checkbox", FALSE);
			ShowWidget("Label_for_Offlinemessages_dropdown", FALSE);
			ShowWidget("Offlinemessages_dropdown", FALSE);
			ShowWidget("Rootpath_edit", FALSE);
			ShowWidget("label_for_Rootpath_edit", FALSE);
			ShowWidget("Sentpath_dropdown", FALSE);
			ShowWidget("label_for_Sentpath_dropdown", FALSE);
			ShowWidget("label_for_Spampath_dropdown", FALSE);
			ShowWidget("label_for_Trashpath_dropdown", FALSE);
			ShowWidget("Spampath_dropdown", FALSE);
			ShowWidget("Trashpath_dropdown", FALSE);
			ShowWidget("Disable_spamfilter_for_account_checkbox", FALSE);
			ShowWidget("tab_account_IMAP",FALSE);
			break;
	}
}


BOOL AccountPropertiesDialog::GetShowPage(INT32 page_number)
{
	if (page_number != 2 && page_number != 4)
		return TRUE;

	Account* account = 0;
	g_m2_engine->GetAccountManager()->GetAccountById(m_account_id, account);
	OP_ASSERT(account != 0);

	if (page_number == 2 && account->GetIncomingProtocol() == AccountTypes::IRC) // Incoming.
	{
		return FALSE;
	}
	if (page_number == 4 && account->GetIncomingProtocol() != AccountTypes::IMAP) // IMAP
	{
		return FALSE;
	}

	return TRUE;
}


void AccountPropertiesDialog::PopulateAuthenticationDropdown(OpDropDown* dropdown, const Account* account, UINT32 supported_authentication, AccountTypes::AuthenticationType selected_method)
{
	if (!dropdown || !account)
		return;

	INT32 dropdown_index = 0;
	OpString authentication_string;
	for (INTPTR i = 31; i >= 0; i--)
	{
		if (supported_authentication & (1<<i))
		{
			if (account->GetAuthenticationString((AccountTypes::AuthenticationType)(i), authentication_string)==OpStatus::OK)
			{
				if ((AccountTypes::AuthenticationType)(i) == AccountTypes::AUTOSELECT)
				{
					AccountTypes::AuthenticationType detected_type = AccountTypes::AUTOSELECT;

					if (dropdown == GetWidgetByName("Inauthentication_dropdown"))
						detected_type = account->GetCurrentIncomingAuthMethod();
					else if (dropdown == GetWidgetByName("Outauthentication_dropdown"))
						detected_type = account->GetCurrentOutgoingAuthMethod();

					if (detected_type != AccountTypes::AUTOSELECT)
					{
						OpString detected_string;
						OpStatus::Ignore(account->GetAuthenticationString(detected_type, detected_string));
						if (detected_string.HasContent())
						{
							OpStatus::Ignore(authentication_string.Append(UNI_L("  (")));
							OpStatus::Ignore(authentication_string.Append(detected_string));
							OpStatus::Ignore(authentication_string.Append(UNI_L(")")));
						}
					}
				}

				OpStatus::Ignore(dropdown->AddItem(authentication_string.CStr(), -1, &dropdown_index, i));
			}

			if (i == (int)selected_method)
				dropdown->SelectItem(dropdown_index, TRUE);
		}
	}
    OnChange(dropdown, FALSE); //Update username/password enabled state
}

int AccountPropertiesDialog::GetOfflineFlags(Account* account) const
{
	int flags = 0;

	if (account->GetDownloadBodies())
		flags |= DOWNLOAD_BODY;
	if (account->GetDefaultStore(TRUE) != 0)
		flags |= KEEP_BODY;	
	
	return flags;
}

#endif //M2_SUPPORT
