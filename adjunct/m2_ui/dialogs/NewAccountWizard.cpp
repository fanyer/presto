/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

# include "adjunct/desktop_util/file_chooser/file_chooser_fun.h"
# include "adjunct/desktop_util/mail/mailcompose.h"
#ifdef IRC_SUPPORT
# include "adjunct/m2/src/backend/irc/chat-networks.h"
#endif
# include "adjunct/m2/src/engine/accountmgr.h"
# include "adjunct/m2/src/engine/engine.h"
# include "adjunct/m2/src/import/importer.h"
# include "adjunct/m2/src/import/AppleMailImporter.h"
# include "adjunct/m2/src/import/ImporterModel.h"
# include "adjunct/m2/src/import/ImportFactory.h"
# include "adjunct/m2/src/util/accountcreator.h"
# include "adjunct/m2/src/util/str/strutil.h"
# include "adjunct/quick/Application.h"
# include "adjunct/m2_ui/dialogs/NewAccountWizard.h"
# include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
# include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
# include "modules/locale/oplanguagemanager.h"
# include "modules/prefs/prefsmanager/collections/pc_m2.h"
# include "modules/prefs/prefsmanager/prefsmanager.h"
# include "modules/util/handy.h"
# include "modules/widgets/OpDropDown.h"
# include "modules/widgets/OpListBox.h"
# include "modules/widgets/OpEdit.h"

/***********************************************************************************
**
**	NewAccountWizard
**
***********************************************************************************/

NewAccountWizard::NewAccountWizard()
	: m_account_creator(NULL)
	, m_found_provider(FALSE)
	, m_has_guessed_fields(FALSE)
	, m_chooser(NULL)
	, m_mailto_force_background(FALSE)
	, m_mailto_new_window(FALSE)
	, m_has_mailto(FALSE)
	, m_reset_mail_panel(0)
{
}


NewAccountWizard::~NewAccountWizard()
{
	g_m2_engine->RemoveEngineListener(this);

	OpListBox* listbox = GetWidgetByName<OpListBox>("Account_type_listbox", WIDGET_TYPE_LISTBOX);
	if (listbox)
		listbox->SetListener(NULL);

	OpTreeView* accountTreeView = (OpTreeView*)GetWidgetByName("Import_setimportaccount");
	if(accountTreeView)
		accountTreeView->SetTreeModel(NULL);

	OP_DELETE(m_importer_object);
	g_m2_engine->SetImportInProgress(FALSE);

	OP_DELETE(m_account_creator);
	OP_DELETE(m_chooser);
}

void NewAccountWizard::Init(AccountTypes::AccountType type,
	DesktopWindow* parent_window, const OpStringC& incoming_dropdown_value)
{
	m_init_type = type == AccountTypes::UNDEFINED ? AccountTypes::POP : type;
	m_type = m_init_type;
	m_import_type = EngineTypes::NONE;
	m_importer_object = NULL;

	m_incoming_dropdown_value.Set(incoming_dropdown_value);

	g_m2_engine->AddEngineListener(this);

	m_reset_mail_panel = g_m2_engine->GetAccountManager()->GetMailNewsAccountCount() == 0;

	Dialog::Init(parent_window, type == AccountTypes::UNDEFINED ? 0 : OnForward(0));
}

void NewAccountWizard::SetMailToInfo(const MailTo& mailto, BOOL force_background, BOOL new_window, const OpStringC *attachment)
{
	m_mailto.Init(mailto.GetRawMailTo().CStr());
	m_mailto_force_background = force_background;
	m_mailto_new_window = new_window;
	if (attachment)
	{
		m_mailto_attachment.Set(attachment->CStr());
	}
	m_has_mailto = TRUE;
}

/***********************************************************************************
**
**	NewAccountWizard
**
***********************************************************************************/

void NewAccountWizard::OnInit()
{
	OpEdit* password = GetWidgetByName<OpEdit>("Password_edit", WIDGET_TYPE_EDIT);
	if (password)
		password->SetPasswordMode(TRUE);

	const char* ltr_text_widgets[] = {
		"Mail_edit",
		"Username_edit",
		"Incoming_edit",
		"Outgoing_edit",
	};
	for (unsigned i = 0; i < ARRAY_SIZE(ltr_text_widgets); ++i)
	{
		OpEdit* edit = GetWidgetByName<OpEdit>(ltr_text_widgets[i], WIDGET_TYPE_EDIT);
		if (edit)
			edit->SetForceTextLTR(TRUE);
	}

	OpListBox* listbox = GetWidgetByName<OpListBox>("Account_type_listbox", WIDGET_TYPE_LISTBOX);
	listbox->SetListener(this);
	OpString loc_str;

	g_languageManager->GetString(Str::D_NEW_ACCOUNT_EMAIL, loc_str);
	listbox->AddItem(loc_str.CStr(), -1, NULL, AccountTypes::POP, m_init_type == AccountTypes::POP || m_init_type == AccountTypes::IMAP, "Account POP");
/*	g_languageManager->GetString(Str::S_NEW_ACCOUNT_POP, loc_str);
	listbox->AddItem(loc_str.CStr(), -1, NULL, (void*) AccountTypes::POP, m_init_type == AccountTypes::POP, "Account POP");
	g_languageManager->GetString(Str::S_NEW_ACCOUNT_IMAP, loc_str);
	listbox->AddItem(loc_str.CStr(), -1, NULL, (void*) AccountTypes::IMAP, m_init_type == AccountTypes::IMAP, "Account IMAP"); */

	g_languageManager->GetString(Str::S_NEW_ACCOUNT_NEWS, loc_str);
	listbox->AddItem(loc_str.CStr(), -1, NULL, AccountTypes::NEWS, m_init_type == AccountTypes::NEWS, "Account News");
	if (!g_m2_engine->ImportInProgress())
	{
		g_languageManager->GetString(Str::S_NEW_ACCOUNT_IMPORT, loc_str);
		listbox->AddItem(loc_str.CStr(), -1, NULL, AccountTypes::IMPORT, m_init_type == AccountTypes::IMPORT, "Account Import");
	}
#ifdef OPERAMAIL_SUPPORT
	g_languageManager->GetString(Str::S_NEW_ACCOUNT_OPERAMAIL, loc_str);
	listbox->AddItem(loc_str.CStr(), -1, NULL, AccountTypes::WEB, m_init_type == AccountTypes::WEB, "Account Operamail");
#endif // OPERAMAIL_SUPPORT
#ifdef IRC_SUPPORT
	g_languageManager->GetString(Str::S_NEW_ACCOUNT_CHAT_IRC, loc_str);
	listbox->AddItem(loc_str.CStr(), -1, NULL, AccountTypes::IRC, m_init_type == AccountTypes::IRC, "Account IRC");
#endif
#ifdef JABBER_SUPPORT
	listbox->AddItem(UNI_L("Chat (JABBER)"), -1, NULL, AccountTypes::JABBER, m_init_type == AccountTypes::JABBER, "Account IRC");
#endif //JABBER_SUPPORT

	listbox = GetWidgetByName<OpListBox>("Import_type_listbox", WIDGET_TYPE_LISTBOX);

	INT32 opera_index	 = -1;
	INT32 eudora_index	 = -1;
	INT32 netscape_index = -1;
#ifdef MSWIN
	INT32 oe_index		 = -1;
#endif
	INT32 mbox_index	 = -1;
#ifdef _MACINTOSH_
	INT32 apple_index	 = -1;
#endif
	INT32 thunderbird_index = -1;
	INT32 opera7_index	= -1;

	g_languageManager->GetString(Str::S_IMPORT_FROM_MBOX, loc_str);
	listbox->AddItem(loc_str.CStr(), -1, &mbox_index, EngineTypes::MBOX);
	g_languageManager->GetString(Str::S_IMPORT_FROM_OPERA7, loc_str);
	listbox->AddItem(loc_str.CStr(), -1, &opera7_index, EngineTypes::OPERA7);
	g_languageManager->GetString(Str::S_IMPORT_FROM_OPERA6, loc_str);
	listbox->AddItem(loc_str.CStr(), -1, &opera_index, EngineTypes::OPERA, TRUE);
	g_languageManager->GetString(Str::S_IMPORT_FROM_EUDORA, loc_str);
	listbox->AddItem(loc_str.CStr(), -1, &eudora_index, EngineTypes::EUDORA);
	g_languageManager->GetString(Str::S_IMPORT_FROM_NETSCAPE6, loc_str);
	listbox->AddItem(loc_str.CStr(), -1, &netscape_index, EngineTypes::NETSCAPE);
	g_languageManager->GetString(Str::S_IMPORT_FROM_THUNDERBIRD, loc_str);
	listbox->AddItem(loc_str.CStr(), -1, &thunderbird_index, EngineTypes::THUNDERBIRD);
#ifdef _MACINTOSH_
	g_languageManager->GetString(Str::S_IMPORT_FROM_APPLE_MAIL_NEW, loc_str);
	listbox->AddItem(loc_str.CStr(), -1, &apple_index, EngineTypes::APPLE);
#endif
#ifdef MSWIN
	g_languageManager->GetString(Str::S_IMPORT_FROM_OE, loc_str);
	listbox->AddItem(loc_str.CStr(), -1, &oe_index, EngineTypes::OE);
#endif

	g_m2_engine->SetImportInProgress(TRUE);

	m_import_type = g_m2_engine->GetDefaultImporterId();

	switch (m_import_type)
	{
	case EngineTypes::OPERA:
		listbox->SelectItem(opera_index, TRUE);
		break;
	case EngineTypes::EUDORA:
		listbox->SelectItem(eudora_index, TRUE);
		break;
#ifdef MSWIN
	case EngineTypes::OE:
		listbox->SelectItem(oe_index, TRUE);
		break;
#endif//MSWIN
	case EngineTypes::NETSCAPE:
		listbox->SelectItem(netscape_index, TRUE);
		break;
	case EngineTypes::MBOX:
		listbox->SelectItem(mbox_index, TRUE);
		break;
	case EngineTypes::THUNDERBIRD:
		listbox->SelectItem(thunderbird_index, TRUE);
		break;
	case EngineTypes::OPERA7:
		listbox->SelectItem(opera7_index, TRUE);
		break;
#ifdef _MACINTOSH_
	case EngineTypes::APPLE:
		listbox->SelectItem(apple_index, TRUE);
		break;
#endif // _MACINTOSH_
	}

	//Populate the 'import into' dropdown
	OpDropDown* import_into = (OpDropDown*)GetWidgetByName("Import_intoaccount_dropdown");
	if (import_into)
	{
		OpString intoaccount;
		g_languageManager->GetString(Str::D_IMPORT_MAIL_INTO, intoaccount);
		import_into->AddItem(intoaccount.CStr(), -1, NULL);

		AccountManager* account_manager = g_m2_engine->GetAccountManager();
		Account* account;
		int i;
		for (i=0; account_manager && i<account_manager->GetAccountCount(); i++)
		{
			account = account_manager->GetAccountByIndex(i);
			if (account)
			{
				AccountTypes::AccountType type = account->GetIncomingProtocol();
				if ((type>AccountTypes::_FIRST_MAIL && type<AccountTypes::_LAST_MAIL &&
					 type!=AccountTypes::IMAP) ||
					(type>AccountTypes::_FIRST_NEWS && type<AccountTypes::_LAST_NEWS) ||
					type==AccountTypes::IMPORT)
				{
					if (account->GetAccountName().HasContent())
					{
						import_into->AddItem(account->GetAccountName().CStr(), -1, NULL, account->GetAccountId());
					}
				}
			}
		}
	}

	SetWidgetValue("Leave_on_server_checkbox", TRUE);
	SetWidgetValue("Delete_permanently_checkbox",TRUE);

	OpDropDown* incoming_dropdown = (OpDropDown*) GetWidgetByName("Incoming_dropdown");

	if (incoming_dropdown)
	{
		incoming_dropdown->SetEditableText(TRUE);

		if (m_incoming_dropdown_value.HasContent())
		{
			// Only insert the user provided value.
			incoming_dropdown->AddItem(m_incoming_dropdown_value.CStr());
		}
#ifdef IRC_SUPPORT
		else
		{
			ChatNetworkManager* chat_networks = MessageEngine::GetInstance()->GetChatNetworks();
			if (chat_networks != 0)
			{
				// Add all default servers.
				OpAutoVector<OpString> network_names;
				chat_networks->IRCNetworkNames(network_names);

				for (UINT32 index = 0, network_count = network_names.GetCount();
					index < network_count; ++index)
				{
					OpString* network_name = network_names.Get(index);
					OP_ASSERT(network_name != 0);

					incoming_dropdown->AddItem(network_name->CStr());
				}
			}
		}
#endif // IRC_SUPPORT
		incoming_dropdown->SelectItemAndInvoke(0, TRUE, FALSE);
	}
}

/***********************************************************************************
**
**	GuessFields
**
***********************************************************************************/

void NewAccountWizard::GuessFields(BOOL guess_userconfig)
{
	BOOL mail = m_type>AccountTypes::_FIRST_MAIL && m_type<AccountTypes::_LAST_MAIL;
	BOOL news = m_type>AccountTypes::_FIRST_NEWS && m_type<AccountTypes::_LAST_NEWS;

	if (mail || news)
	{
		// Make sure the account creator exists and is ready to go
		if (!m_account_creator)
		{
			m_account_creator = OP_NEW(AccountCreator, ());
			if (!m_account_creator || OpStatus::IsError(m_account_creator->Init()))
				return;
		}

		// See if we can get server information with the provided email address
		OpString				  mail_address, username;
		AccountCreator::Server	  incoming_server;
		AccountTypes::AccountType protocol = news ? AccountTypes::NEWS : AccountTypes::UNDEFINED;

		if (guess_userconfig)
			protocol = news ? AccountTypes::NEWS : AccountTypes::UNDEFINED;
		else
			protocol = m_type;

		GetWidgetText("Mail_edit", mail_address);

		m_found_provider = OpStatus::IsSuccess(m_account_creator->GetServer(mail_address,
																			protocol,
																			TRUE,
																			incoming_server));

		if (!m_found_provider)
		{
			// No provider - get some usable default values
			RETURN_VOID_IF_ERROR(m_account_creator->GetServerDefaults(mail_address,
																	  protocol,
																	  incoming_server));
		}
		else
		{
			// Fill in preferred protocol
			m_type = incoming_server.GetProtocol();
		}

		// We have a server - fill in values
		if (guess_userconfig)
			SetWidgetText("Username_edit", incoming_server.GetUsername().CStr());
		SetWidgetText("Incoming_edit", incoming_server.GetHostname().CStr());
		SetWidgetText("Outgoing_edit", incoming_server.GetHostname().CStr());
		SetWidgetValue("Leave_on_server_checkbox", incoming_server.GetPopLeaveMessages());
		SetWidgetValue("Delete_permanently_checkbox",incoming_server.GetPopDeletePermanently());
		SetWidgetValue("Incoming_secure_checkbox", incoming_server.GetSecure());
		SetWidgetValue("Outgoing_secure_checkbox", incoming_server.GetSecure());

		m_has_guessed_fields = TRUE;
	}
}

/***********************************************************************************
**
**	InitImportFields
**
***********************************************************************************/

void NewAccountWizard::InitImportFields(BOOL all)
{
	if (EngineTypes::NONE == m_import_type)
	{
		return;
	}

	OpTreeView* accountTreeView = (OpTreeView*) GetWidgetByName("Import_setimportaccount");
	accountTreeView->SetTreeModel(NULL);

	EnableForwardButton(FALSE);

	if (all)
	{
		OP_DELETE(m_importer_object);
		m_importer_object = NULL;

		RETURN_VOID_IF_ERROR(ImportFactory::Instance()->Create(m_import_type, &m_importer_object));

		if (m_import_path.IsEmpty())
		{
			m_importer_object->GetLastUsedImportFolder(m_import_type, m_import_path);

			if (m_import_path.IsEmpty())
				m_importer_object->GetDefaultImportFolder(m_import_path);
		}

		accountTreeView->SetMultiselectable(FALSE);

		SetWidgetEnabled("Import_contacts_check", TRUE);
		SetWidgetEnabled("Import_settings_check", TRUE);
		SetWidgetEnabled("Import_messages_check", TRUE);

		SetWidgetValue("Import_settings_check", TRUE);
		SetWidgetValue("Import_contacts_check", FALSE);
		SetWidgetValue("Import_messages_check", TRUE);

		ShowWidget("Import_contacts_check", FALSE);
		ShowWidget("label_for_Account_path_browse", TRUE);
		ShowWidget("Account_path_browse", TRUE);
		ShowWidget("Account_path_dir_browse", FALSE);

		SetWidgetEnabled("Move_to_sent_dropdown", (EngineTypes::OPERA != m_import_type &&
												  EngineTypes::EUDORA != m_import_type));

		OpString loc_str;

		switch (m_import_type)
		{
		case EngineTypes::NETSCAPE:
			g_languageManager->GetString(Str::S_IMPORT_NETSCAPE_PREFS_FILE, loc_str);
			SetWidgetText("label_for_Account_path_browse", loc_str.CStr());
			g_languageManager->GetString(Str::D_BROWSE, loc_str);
			SetWidgetText("Account_path_browse", loc_str.CStr());
			break;
		case EngineTypes::THUNDERBIRD:
			g_languageManager->GetString(Str::S_IMPORT_THUNDERBIRD_PREFS_FILE, loc_str);
			SetWidgetText("label_for_Account_path_browse", loc_str.CStr());
			g_languageManager->GetString(Str::D_BROWSE, loc_str);
			SetWidgetText("Account_path_browse", loc_str.CStr());
			break;
		case EngineTypes::MBOX:
#if defined (DESKTOP_FILECHOOSER_DIRFILE)
			g_languageManager->GetString(Str::D_MBOX_IMPORT_FOLDER_AND_FILE_ADD, loc_str);
			SetWidgetText("Account_path_browse", loc_str.CStr());
#else // !DESKTOP_FILECHOOSER_DIRFILE
			ShowWidget("Account_path_dir_browse", TRUE);
			g_languageManager->GetString(Str::D_MBOX_IMPORT_FILE_ADD, loc_str);
			SetWidgetText("Account_path_browse", loc_str.CStr());
			g_languageManager->GetString(Str::D_MBOX_IMPORT_FOLDER_ADD, loc_str);
			SetWidgetText("Account_path_dir_browse", loc_str.CStr());
#endif // !DESKTOP_FILECHOOSER_DIRFILE
			SetWidgetValue("Import_contacts_check", FALSE);
			SetWidgetValue("Import_settings_check", FALSE);
			SetWidgetEnabled("Import_contacts_check", FALSE);
			SetWidgetEnabled("Import_settings_check", FALSE);
			SetWidgetEnabled("Import_messages_check", FALSE);
			accountTreeView->SetMultiselectable(TRUE);
			g_languageManager->GetString(Str::S_IMPORT_GENERIC_MBOX_FILE, loc_str);
			SetWidgetText("label_for_Account_path_browse", loc_str.CStr());
			break;
#ifdef _MACINTOSH_
		case EngineTypes::APPLE:
			g_languageManager->GetString(Str::D_BROWSE, loc_str);
			SetWidgetText("Account_path_browse", loc_str.CStr());
			accountTreeView->SetMultiselectable(TRUE);
			g_languageManager->GetString(Str::S_IMPORT_GENERIC_MBOX_FILE, loc_str);
			SetWidgetText("label_for_Account_path_browse", loc_str.CStr());
			break;
#endif
#ifdef MSWIN
		case EngineTypes::OE:
			g_languageManager->GetString(Str::S_IMPORT_FROM_OE, loc_str);
			SetWidgetText("label_for_Account_path_browse",loc_str.CStr());
			ShowWidget("Account_path_browse", FALSE);
			break;
#endif
		case EngineTypes::OPERA7:
			accountTreeView->SetMultiselectable(TRUE);
			g_languageManager->GetString(Str::S_IMPORT_OPERA7_ACCOUNT_INI, loc_str);
			SetWidgetText("label_for_Account_path_browse", loc_str.CStr());
			g_languageManager->GetString(Str::D_BROWSE, loc_str);
			SetWidgetText("Account_path_browse", loc_str.CStr());
			break;
		default:
			g_languageManager->GetString(Str::S_IMPORT_MAILBOX_FOLDER, loc_str);
			SetWidgetText("label_for_Account_path_browse", loc_str.CStr());
			g_languageManager->GetString(Str::D_BROWSE, loc_str);
			SetWidgetText("Account_path_browse", loc_str.CStr());
			break;
		}
	}

	OP_STATUS res = m_importer_object->Init();
	accountTreeView->SetTreeModel(m_importer_object->GetModel());
	accountTreeView->SetShowThreadImage(TRUE);

	OpDropDown* move_to_sent = (OpDropDown*)GetWidgetByName("Move_to_sent_dropdown");
	if (move_to_sent)
		move_to_sent->Clear();

	if (OpStatus::IsSuccess(res))
	{
		if (m_importer_object->GetModel()->GetItemCount() > 0)
		{
			if (EngineTypes::MBOX == m_import_type ||
				EngineTypes::APPLE == m_import_type ||
				EngineTypes::OPERA7 == m_import_type)	// select all
			{
				accountTreeView->SelectAllItems();
			}
			else
			{
				accountTreeView->SetSelectedItem(0);
			}

			PopulateMoveToSent(accountTreeView);

			EnableForwardButton(TRUE);
		}
	}

	OpEdit* account_path = static_cast<OpEdit*>(GetWidgetByName("Account_path"));
	if (account_path)
		account_path->SetText(m_import_path.CStr());
}


/***********************************************************************************
**
**	PopulateMoveToSent
**
***********************************************************************************/

void NewAccountWizard::PopulateMoveToSent(OpTreeView* accountTreeView)
{
	OpDropDown* move_to_sent = (OpDropDown*)GetWidgetByName("Move_to_sent_dropdown");
	if (move_to_sent)
	{
		OpString str_none;
		g_languageManager->GetString(Str::S_IMPORT_WIZARD_NONE, str_none);

		move_to_sent->Clear();
		move_to_sent->AddItem(str_none.CStr(), -1, NULL);

		OpINT32Vector items;
		INT32 count_items = accountTreeView->GetSelectedItems(items, FALSE);
		for (int i = 0; i < count_items; i++)
		{
			ImporterModelItem* parent_item = (ImporterModelItem*)accountTreeView->GetItemByPosition(items.Get(i));
			if (!parent_item)
				break;

			OpQueue<ImporterModelItem>* sequence = OP_NEW(OpQueue<ImporterModelItem>, ());
			if (!sequence)
				return;

			if (OpStatus::IsError(m_importer_object->GetModel()->MakeSequence(sequence, parent_item)))
			{
				OP_DELETE(sequence);
				return;
			}

			ImporterModelItem* item = NULL;

			do
			{
				item = m_importer_object->GetModel()->PullItem(sequence);
				if (item && item->GetType() == IMPORT_MAILBOX_TYPE)
				{
					move_to_sent->AddItem(item->GetName().CStr(), -1, NULL, reinterpret_cast<INTPTR>(item));
				}
			}
			while (item);

			OP_DELETE(sequence);
		}
	}
}

/***********************************************************************************
**
**	OnFileChoosingDone
**
***********************************************************************************/
void NewAccountWizard::OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
{
	if (result.files.GetCount())
	{
		m_import_path.Set(result.active_directory);
		m_importer_object->SetLastUsedImportFolder(m_import_type, m_import_path);
		if (m_import_type == EngineTypes::NETSCAPE ||
			m_import_type == EngineTypes::THUNDERBIRD ||
			m_import_type == EngineTypes::EUDORA ||
			m_import_type == EngineTypes::OPERA ||
			m_import_type == EngineTypes::OPERA7)
			m_importer_object->AddImportFile(*result.files.Get(0));
		else
			m_importer_object->AddImportFileList(&result.files);

		InitImportFields(FALSE);
	}
}

/***********************************************************************************
**
**	OnInitPage
**
***********************************************************************************/

void NewAccountWizard::OnInitPage(INT32 page_number, BOOL first_time)
{
	const BOOL chat = m_type > AccountTypes::_FIRST_CHAT && m_type < AccountTypes::_LAST_CHAT;
	const BOOL mail = m_type > AccountTypes::_FIRST_MAIL && m_type < AccountTypes::_LAST_MAIL;
	const BOOL news = m_type > AccountTypes::_FIRST_NEWS && m_type < AccountTypes::_LAST_NEWS;
	const BOOL imap = m_type == AccountTypes::IMAP;
	const BOOL msn = m_type == AccountTypes::MSN;
#ifdef JABBER_SUPPORT
	const BOOL jabber = m_type==AccountTypes::JABBER;
#endif // JABBER_SUPPORT

	EnableForwardButton(TRUE);

	switch (page_number)
	{
	case 1:
		{
#ifdef JABBER_SUPPORT
			if (jabber)
			{
				ShowWidget("label_for_Name_edit", FALSE);
				ShowWidget("Name_edit", FALSE);
				SetWidgetText("label_for_Mail_edit", UNI_L("Jabber ID"));
				ShowWidget("label_for_Mail_edit", TRUE);
				ShowWidget("Mail_edit", TRUE);
				ShowWidget("label_for_Organization_edit", FALSE);
				ShowWidget("Organization_edit", FALSE);
				break;
			}
#endif // JABBER_SUPPORT

			ShowWidget("label_for_Organization_edit", !chat);
			ShowWidget("Organization_edit", !chat);
			ShowWidget("Outgoing_secure_checkbox",!chat);
			ShowWidget("label_for_Name_edit", !msn);
			ShowWidget("Name_edit", !msn);

			if (msn)
				break;

			AccountManager* manager = g_m2_engine->GetAccountManager();

			Account* account = NULL;
			Account* chat_account = NULL;
			OpString string;

			manager->GetAccountById(manager->GetDefaultAccountId(AccountTypes::TYPE_CATEGORY_MAIL), account);

			if (chat)
			{
				manager->GetAccountById(manager->GetDefaultAccountId(AccountTypes::TYPE_CATEGORY_CHAT), chat_account);
			}

			if (chat_account)
				account = chat_account;

			if (account)
			{
				account->GetRealname(string);

				SetWidgetText("Name_edit",	string.CStr());

				if (!mail)
				{
					account->GetEmail(string);
					SetWidgetText("Mail_edit",	string.CStr());
				}

				account->GetOrganization(string);
				SetWidgetText("Organization_edit",	string.CStr());
			}

			if (chat_account)
			{
				OpString8 string8;
				chat_account->GetIncomingUsername(string8);
				SetWidgetText("Username_edit",	string8.CStr());
			}

			if (mail)
			{
				OpString text;
				GetWidgetText("Mail_edit",	text);
				EnableForwardButton(text.HasContent());
			}
		}
		break;

	case 2:
		{
			// Hide the warning initially, and turn the text red.
			OpLabel * illegal_nick_label = (OpLabel*) GetWidgetByName("label_for_nick_warning");
			if (illegal_nick_label)
			{
				illegal_nick_label->SetForegroundColor(OP_RGB(255, 0, 0));
				illegal_nick_label->SetVisibility(FALSE);
			}
			if (chat)
			{
				OpString nick_name;
				GetWidgetText("Username_edit",nick_name);
				OpString8 nick8;
				nick8.Set(nick_name.CStr());
				ShowWidget("label_for_nick_warning",!!nick_name.Compare(nick8));
				EnableForwardButton(!nick_name.Compare(nick8));
			}

			if (!m_has_guessed_fields)
				GuessFields();

			if (msn)
			{
				ShowWidget("label_for_Username_edit", FALSE);
				ShowWidget("Username_edit", FALSE);
				break;
			}

#ifdef JABBER_SUPPORT
			if (jabber)
			{
				SetWidgetText("label_for_Username_edit", UNI_L("Username"));
				break;
			}
#endif // JABBER_SUPPORT

			if(!mail)
			{
				ShowWidget("Email_account_type_label", FALSE);
				ShowWidget("POP_account_radio", FALSE);
				ShowWidget("IMAP_account_radio", FALSE);
			}
			else
			{
				if (first_time && !m_found_provider)
					m_type = m_init_type;
				SetWidgetValue("POP_account_radio", m_type == AccountTypes::POP);
				SetWidgetValue("IMAP_account_radio", m_type == AccountTypes::IMAP);
			}

			OpString label;

			g_languageManager->GetString(!chat ? Str::LocaleString(Str::DI_IDSTR_M2_NEWACC_WIZ_LOGIN) : Str::LocaleString(Str::DI_IDSTR_M2_NEWACC_WIZ_NICK), label);

			SetWidgetText("label_for_Username_edit", label.CStr());
			ShowWidget("label_for_Password_edit", !chat);
			ShowWidget("Password_edit", !chat);

			break;
		}
	case 3:
		{
			OpString label;

			g_languageManager->GetString(!chat ? Str::LocaleString(Str::DI_IDSTR_M2_NEWACC_WIZ_INCOMING_SERVER) : Str::LocaleString(Str::DI_IDSTR_M2_NEWACC_WIZ_IRC_SERVER), label);

			SetWidgetText("label_for_Incoming_edit", label.CStr());
            ShowWidget("Incoming_secure_checkbox", !news && !chat); //M2 doesn't support snews yet

            ShowWidget("Incoming_edit", !chat);
            ShowWidget("Incoming_dropdown", chat);

			ShowWidget("label_for_Outgoing_edit", !chat);
			ShowWidget("Outgoing_edit", !chat);
			ShowWidget("Leave_on_server_checkbox", !(imap || news || chat));
			ShowWidget("Delete_permanently_checkbox",!(imap || news || chat));

#ifdef JABBER_SUPPORT
			if (jabber)
			{
				SetWidgetText("label_for_Incoming_edit", UNI_L("Jabber server"));
				ShowWidget("label_for_Incoming_edit");
				ShowWidget("Incoming_edit");
				ShowWidget("Incoming_dropdown", FALSE); // undo what is done above
				break;
			}
#endif // JABBER_SUPPORT
		}
		break;

	case 4:	// Import
		break;

	case 5:	// Import prefs
		{
			m_import_path.Empty();
			InitImportFields(TRUE);

			OpString start_text;
			g_languageManager->GetString(Str::D_START_IMPORT_BUTTON, start_text);

			SetForwardButtonText(start_text);
		}
		break;

	case 6:	// Import progress
		DoMailImport();
		break;
	}
}

BOOL NewAccountWizard::IsLastPage(INT32 page_number)
{
	// Normal account wizard has 3 pages, or 2 if it's a known provider
	int last_page = m_found_provider ? 2 : 3;

	// Some special cases
	switch (m_type)
	{
	case AccountTypes::IMPORT:
		last_page = 6;
		break;
#ifdef OPERAMAIL_SUPPORT
	case AccountTypes::WEB:
		last_page = 0;
		break;
#endif // OPERAMAIL_SUPPORT
	case AccountTypes::MSN:
		last_page = 2;
		break;
	}

	return page_number >= last_page;
}

UINT32 NewAccountWizard::OnForward(UINT32 page_number)
{
	if (m_type == AccountTypes::IMPORT)
	{
		if (page_number == 0)
			return 4;
	}

	if (m_type == AccountTypes::NEWS && page_number == 1)
		return 3;

#ifdef JABBER_SUPPORT
	if (m_type == AccountTypes::JABBER)
		return page_number + 1;
#endif // JABBER_SUPPORT

	if (m_type > AccountTypes::MSN && m_type < AccountTypes::IMPORT)
		return 7;

	return page_number + 1;
}

UINT32 NewAccountWizard::OnOk()
{
	// Some special cases that can be handled quickly
	switch (m_type)
	{
#ifdef OPERAMAIL_SUPPORT
	case AccountTypes::WEB:
		if (g_pcm2->GetStringPref(PrefsCollectionM2::WebmailAddress).HasContent())
			g_application->GoToPage(g_pcm2->GetStringPref(PrefsCollectionM2::WebmailAddress).CStr());
		return 0;
#endif // OPERAMAIL_SUPPORT

	case AccountTypes::IMPORT:
		UpdateUIAfterAccountCreation();
		return 0;
	}

	AccountCreator::Server	  incoming_server, outgoing_server;
	OpString				  name, email_address, organization, username, password, account_name;
	AccountTypes::AccountType outgoing_type = Account::GetOutgoingForIncoming(m_type);

	// Get necessary information
	GetWidgetText("Name_edit",			name);
	GetWidgetText("Mail_edit",			email_address);
	GetWidgetText("Organization_edit",	organization);
	GetWidgetText("Username_edit",		username);
	GetWidgetText("Password_edit",		password);

	// Account name set to email address by default; might be changed by GetServerInfo() later
	account_name.Set(email_address);

	// If this is a known provider, get server data directly; else, fill in
	if (m_found_provider)
	{
		m_account_creator->GetServer(email_address, m_type, TRUE, incoming_server);
		m_account_creator->GetServer(email_address, outgoing_type, FALSE, outgoing_server);
	}
	else
	{
		// Get necessary information
		OpString incoming_host_total, incoming_host, outgoing_host_total, outgoing_host;
		unsigned incoming_port, outgoing_port;
		BOOL	 leave_messages, incoming_secure, outgoing_secure;

		GetServerInfo(TRUE,  incoming_host, incoming_port, account_name);
		GetServerInfo(FALSE, outgoing_host, outgoing_port, account_name);

		leave_messages  = GetWidgetValue("Leave_on_server_checkbox");
		incoming_secure = GetWidgetValue("Incoming_secure_checkbox");
		outgoing_secure = GetWidgetValue("Outgoing_secure_checkbox");

		// Set incoming server information
		incoming_server.Init(m_type);
		incoming_server.SetHost(incoming_host);
		incoming_server.SetPort(incoming_port);
		incoming_server.SetSecure(incoming_secure);
		incoming_server.SetPopLeaveMessages(leave_messages);
		incoming_server.SetPopDeletePermanently(GetWidgetValue("Delete_permanently_checkbox"));
		
		// Set outgoing server information
		outgoing_server.Init(outgoing_type);
		outgoing_server.SetHost(outgoing_host);
		outgoing_server.SetPort(outgoing_port);
		outgoing_server.SetSecure(outgoing_secure);
		
	}

	// Set the name of the account to the server name for NNTP - fix for bug #296144
	if(m_type == AccountTypes::NEWS)
		account_name.Set(incoming_server.GetHostname());

	// Username should always be what the user typed in the username field, we guessed, but user knows best 
	// fix for bug DSK-209183
	incoming_server.SetUsername(username);
	incoming_server.SetPassword(password);

	outgoing_server.SetUsername(username);
	outgoing_server.SetPassword(password);

	// Now create the account
	Account* account;
	if (OpStatus::IsSuccess(m_account_creator->CreateAccount(name,
															 email_address,
															 organization,
															 account_name,
															 incoming_server,
															 outgoing_server,
															 g_m2_engine,
															 account)))
	{
		// Show a group subscription dialog if necessary
		if ((m_type == AccountTypes::NEWS || m_type == AccountTypes::IRC) &&
			(!GetParentDesktopWindow() || GetParentDesktopWindow()->GetType() != DIALOG_TYPE_ACCOUNT_SUBSCRIPTION))
			g_application->SubscribeAccount(m_type, NULL, account->GetAccountId(), GetParentDesktopWindow());

		// Update UI with new account
		UpdateUIAfterAccountCreation();
	}
	if (m_has_mailto && (m_type == AccountTypes::IMAP || m_type == AccountTypes::POP))
		MailCompose::InternalComposeMail(m_mailto, MAILHANDLER_OPERA, 0, m_mailto_force_background, m_mailto_new_window, m_mailto_attachment.HasContent() ? &m_mailto_attachment : NULL);
	return 0;
}


void NewAccountWizard::UpdateUIAfterAccountCreation()
{
	g_application->SettingsChanged(SETTINGS_ACCOUNT_SELECTOR);
	g_application->SettingsChanged(SETTINGS_MENU_SETUP);
	g_application->SettingsChanged(SETTINGS_TOOLBAR_SETUP);
	g_application->SyncSettings();

	g_input_manager->InvokeAction(OpInputAction::ACTION_SHOW_PANEL, 0, UNI_L("Contacts"), g_application->GetActiveDesktopWindow());

	OpInputAction::Action action = OpInputAction::ACTION_FOCUS_PANEL;
	OpWindow* parent = GetParentWindow();
	if(parent && parent->GetStyle() == OpWindow::STYLE_MODAL_DIALOG)
	{
		action = OpInputAction::ACTION_SHOW_PANEL;
	}

	switch (m_type)
	{
	case AccountTypes::NEWS:
	case AccountTypes::IMAP:
	case AccountTypes::POP:
	case AccountTypes::IMPORT:
		g_input_manager->InvokeAction(action, 0, UNI_L("Mail"), g_application->GetActiveDesktopWindow());
		
		// if it's the first mail/news account, then we need to add the default categories
		if ( m_reset_mail_panel )
			g_input_manager->InvokeAction(OpInputAction::ACTION_RESET_MAIL_PANEL);

		break;
	case AccountTypes::IRC:
		g_input_manager->InvokeAction(action, 0, UNI_L("Chat"), g_application->GetActiveDesktopWindow());
		break;
	}
}


void NewAccountWizard::OnCancel()
{
	if (AccountTypes::IMPORT == m_type && m_importer_object)
	{
		m_importer_object->CancelImport();
	}
	if ( m_reset_mail_panel  && g_m2_engine->GetAccountManager()->GetMailNewsAccountCount() != 0)
	{
		UpdateUIAfterAccountCreation();
	}

	Dialog::OnCancel();
}

void NewAccountWizard::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget->IsNamed("Account_type_listbox"))
	{
		m_type = (AccountTypes::AccountType) (INTPTR) ((OpListBox*)widget)->GetItemUserData(((OpListBox*)widget)->GetSelectedItem());
		return;
	}
	else if (widget->IsNamed("POP_account_radio") && widget->GetValue())
	{
		m_type           = AccountTypes::POP;
		GuessFields(FALSE);
	}
	else if (widget->IsNamed("IMAP_account_radio") && widget->GetValue())
	{
		m_type = AccountTypes::IMAP;
		GuessFields(FALSE);
	}
	else if (widget->IsNamed("Import_type_listbox"))
	{
		OpListBox* listbox = (OpListBox*)GetWidgetByName("Import_type_listbox");

		INT32 selection = listbox->GetSelectedItem();
		if (selection > -1 && listbox->IsEnabled(selection))
		{
			m_import_type = (EngineTypes::ImporterId)(INTPTR)listbox->GetItemUserData(selection);
			EnableForwardButton(TRUE);
		}
		else
		{
			m_import_type = EngineTypes::NONE;
			EnableForwardButton(FALSE);
		}
		return;
	}
	else if (widget->IsNamed("Import_intoaccount_dropdown"))
	{
		OpDropDown* import_into = (OpDropDown*)widget;
		UINT16 account_id = (UINT16)((UINTPTR)import_into->GetItemUserData(import_into->GetSelectedItem()));
		SetWidgetEnabled("Import_settings_check", m_import_type!=EngineTypes::MBOX && account_id==0);
		SetWidgetValue("Import_settings_check", m_import_type!=EngineTypes::MBOX && account_id==0);
	}
	else if (widget->IsNamed("Import_setimportaccount"))
	{
		PopulateMoveToSent((OpTreeView*)widget);
	}
	else if (widget->IsNamed("Mail_edit"))
	{
		OpString text;
		widget->GetText(text);
		EnableForwardButton(text.HasContent());
	}
	else if(widget->IsNamed("Username_edit") && m_type == AccountTypes::IRC)
	{
		// If there is wide characters in the nick, then show error and disable forward button
		OpString nick_name;
		widget->GetText(nick_name);
		OpString8 nick8;
		nick8.Set(nick_name.CStr());
		ShowWidget("label_for_nick_warning",!!nick_name.Compare(nick8));
		EnableForwardButton(!nick_name.Compare(nick8));		
	}
	else if (widget->IsNamed("Leave_on_server_checkbox"))
	{
		SetWidgetEnabled("Delete_permanently_checkbox",widget->GetValue());
	}

	// If there are no more pages then show the Finish button
	if (IsLastPage(GetCurrentPage()))
	{
		OpString finish_text;
		g_languageManager->GetString(Str::S_WIZARD_FINISH, finish_text);
		SetForwardButtonText(finish_text);
	}
	else
	{
		OpString next_text;
		g_languageManager->GetString(Str::S_WIZARD_NEXT_PAGE, next_text);
		SetForwardButtonText(next_text);
	}

	Dialog::OnChange(widget, changed_by_mouse);
}

void NewAccountWizard::OnClick(OpWidget *widget, UINT32 id)
{
	if (	(widget->IsNamed("Account_path_browse") || widget->IsNamed("Account_path_dir_browse"))
		&&	m_importer_object)
	{
		if (!m_chooser)
			RETURN_VOID_IF_ERROR(DesktopFileChooser::Create(&m_chooser));

		if (m_import_path.HasContent())
			m_request.initial_path.Set(m_import_path);
		else
		{
			OpString tmp_storage;
			m_request.initial_path.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_OPEN_FOLDER, tmp_storage));
		};

		Str::LocaleString caption_str(Str::NOT_A_STRING);
		Str::LocaleString filter_str(Str::NOT_A_STRING);
		m_request.action = DesktopFileChooserRequest::ACTION_FILE_OPEN;

		switch (m_import_type)
		{
		case EngineTypes::NETSCAPE:
			filter_str = Str::S_IMPORT_NETSCAPE_FILTER;
			caption_str = Str::D_SELECT_NETSCAPE_JS_TO_IMPORT;
			break;
		case EngineTypes::THUNDERBIRD:
			filter_str = Str::S_IMPORT_THUNDERBIRD_FILTER;
			caption_str = Str::D_SELECT_THUNDERBIRD_JS_TO_IMPORT;
			break;
		case EngineTypes::MBOX:
#if defined (DESKTOP_FILECHOOSER_DIRFILE)
			m_request.action = DesktopFileChooserRequest::ACTION_DIR_FILE_OPEN_MULTI;
			filter_str = Str::S_IMPORT_MBOX_FILTER;
			caption_str = Str::D_SELECT_FILES_AND_DIRS_TO_IMPORT;
#else
			if (widget->IsNamed("Account_path_browse"))
			{
				m_request.action = DesktopFileChooserRequest::ACTION_FILE_OPEN_MULTI;
				filter_str = Str::S_IMPORT_MBOX_FILTER;
				caption_str = Str::D_SELECT_FILES_TO_IMPORT;
			}
			else // Account_path_dir_browse
			{
				m_request.action = DesktopFileChooserRequest::ACTION_DIRECTORY;
				caption_str = Str::D_SELECT_MBOX_DIRS_TO_IMPORT;
			}
#endif
			break;
		case EngineTypes::APPLE:
			m_request.action = DesktopFileChooserRequest::ACTION_FILE_OPEN_MULTI;
			filter_str = Str::S_IMPORT_MBOX_FILTER;
			caption_str = Str::D_SELECT_FILES_TO_IMPORT;
			break;
		case EngineTypes::EUDORA:
			filter_str = Str::S_IMPORT_EUDORA_FILTER;
			caption_str = Str::D_SELECT_EUDORA_INI_TO_IMPORT;
			break;
		case EngineTypes::OPERA:
			filter_str = Str::S_IMPORT_OPERA6_MAIL_FILTER;
			caption_str = Str::D_SELECT_OPERA_ACCOUNTS_INI_TO_IMPORT;
			break;
		case EngineTypes::OPERA7:
			filter_str = Str::S_IMPORT_OPERA7_MAIL_FILTER;
			caption_str = Str::D_SELECT_OPERA7_ACCOUNTS_INI_TO_IMPORT;
			break;
		default:
			caption_str = Str::D_SELECT_FILES_TO_IMPORT;
			break;
		}

		OpString filter;
		g_languageManager->GetString(caption_str, m_request.caption);
		if (filter_str != Str::NOT_A_STRING)
			g_languageManager->GetString(filter_str, filter);

		m_request.extension_filters.DeleteAll();

		if (filter.HasContent())
			StringFilterToExtensionFilter(filter.CStr(), &m_request.extension_filters);

		OpStatus::Ignore(m_chooser->Execute(GetOpWindow(), this, m_request));

		return;

	}
	else if (widget->IsNamed("Import_messages_check"))
	{
		BOOL enable_sent_dropdown = (BOOL)GetWidgetValue("Import_messages_check");
		SetWidgetEnabled("Move_to_sent_dropdown", enable_sent_dropdown &&
												  (EngineTypes::OPERA != m_import_type &&
												  EngineTypes::EUDORA != m_import_type));
	}

	Dialog::OnClick(widget, id);
}

void NewAccountWizard::DoMailImport()
{
	OpDropDown* import_into = (OpDropDown*)GetWidgetByName("Import_intoaccount_dropdown");
	UINT16 account_id = (UINT16)((UINTPTR)import_into->GetItemUserData(import_into->GetSelectedItem()));

	BOOL importsettings = GetWidgetValue("Import_settings_check");
	BOOL importcontacts = GetWidgetValue("Import_contacts_check");
	BOOL importmessages = GetWidgetValue("Import_messages_check");

	if (m_importer_object->GetImportFileCount() == 0 && !m_import_path.IsEmpty())
	{
		m_importer_object->AddImportFile(m_import_path);
	}

	OpDropDown* move_to_sent = (OpDropDown*)GetWidgetByName("Move_to_sent_dropdown");
	if (move_to_sent)
	{
		ImporterModelItem* item = (ImporterModelItem*)move_to_sent->GetItemUserData(move_to_sent->GetSelectedItem());
		m_importer_object->SetMoveToSentItem(item);
	}

	OpTreeView* accountTreeView = (OpTreeView*)GetWidgetByName("Import_setimportaccount");

	if (EngineTypes::MBOX == m_import_type)
	{
		OpINT32Vector items;

		int item_count = accountTreeView->GetSelectedItems(items, FALSE);

		if (item_count == 0)
		{
			return;
		}

		OpVector<ImporterModelItem> modelItems;

		for (INT32 i = 0; i < item_count; i++)
		{
			ImporterModelItem* item = (ImporterModelItem*) accountTreeView->GetItemByPosition(items.Get(i));

			if (!item)
				continue;

			modelItems.Add(item);
		}

		m_importer_object->SetImportItems(modelItems);
		modelItems.Clear();

		if ((account_id==0 && m_importer_object->CreateImportAccount()==OpStatus::OK) ||
			(account_id!=0 && m_importer_object->SetM2Account(account_id)==OpStatus::OK))
		{
			SetWidgetText("Import_progress_label", UNI_L(""));
			((OpProgressBar*)GetWidgetByName("Import_progress_bar"))->SetProgress(0, 0);
			EnableBackButton(FALSE);
			EnableForwardButton(FALSE);
			m_importer_object->ImportMessages();
		}
	}
	else if(EngineTypes::OPERA7 == m_import_type)
	{
		OpINT32Vector items;

		int item_count = accountTreeView->GetSelectedItems(items, FALSE);

		if (item_count == 0)
		{
			return;
		}

		OpVector<ImporterModelItem> modelAccountItems;

		for (INT32 i = 0; i < item_count; i++)
		{
			ImporterModelItem* item = (ImporterModelItem*) accountTreeView->GetItemByPosition(items.Get(i));

			if (!item)
				continue;

			modelAccountItems.Add(item);
		}

		m_importer_object->SetImportItems(modelAccountItems);
		modelAccountItems.Clear();

		SetWidgetText("Import_progress_label", UNI_L(""));
		((OpProgressBar*)GetWidgetByName("Import_progress_bar"))->SetProgress(0, 0);
		EnableBackButton(FALSE);
		EnableForwardButton(FALSE);

		// necessary to import messages, see Opera7Importer.h
		if (importmessages)
			m_importer_object->ImportMessages();
		m_importer_object->ImportAccounts();
	}
#ifdef _MACINTOSH_
	else if(EngineTypes::APPLE == m_import_type)
	{
		INT32 accounts_version = ((AppleMailImporter*)m_importer_object)->GetAccountsVersion();

		OpINT32Vector items;

		int item_count = accountTreeView->GetSelectedItems(items, FALSE);

		if (item_count == 0)
		{
			return;
		}

		if(accounts_version < 2)
		{
			OpVector<ImporterModelItem> modelMailboxItems;
			OpVector<ImporterModelItem> modelSettingsItems;

			for (INT32 i = 0; i < item_count; i++)
			{
				ImporterModelItem* item = (ImporterModelItem*) accountTreeView->GetItemByPosition(items.Get(i));

				if (!item)
					continue;

				if(item->GetType() == IMPORT_MAILBOX_TYPE || item->GetType() == IMPORT_FOLDER_TYPE)
				{
					modelMailboxItems.Add(item);
				}
				else
				{
					modelSettingsItems.Add(item);
				}
			}

			m_importer_object->SetImportItems(modelSettingsItems);
			modelSettingsItems.Clear();

			BOOL has_account = FALSE;

			if (importsettings)
			{
				has_account = OpStatus::IsSuccess(m_importer_object->ImportSettings());

				OpString importFinished;
				g_languageManager->GetString(Str::D_MAIL_IMPORT_FINISHED, importFinished);

				SetWidgetText("Import_progress_label", importFinished.CStr());
				((OpProgressBar*)GetWidgetByName("Import_progress_bar"))->SetProgress(100, 100);
			}

			if (!has_account)
			{
				if ((account_id==0 && m_importer_object->CreateImportAccount()!=OpStatus::OK) ||
					(account_id!=0 && m_importer_object->SetM2Account(account_id)!=OpStatus::OK))
				{
					OpString acc;
					if(m_importer_object->GetImportAccount(acc))
						m_importer_object->SetM2Account(acc);
				}
			}

			m_importer_object->SetImportItems(modelMailboxItems);
			m_importer_object->SetImportItems(modelSettingsItems);
			modelMailboxItems.Clear();

			if (importmessages)
			{
				SetWidgetText("Import_progress_label", UNI_L(""));
				((OpProgressBar*)GetWidgetByName("Import_progress_bar"))->SetProgress(0, 0);
				EnableBackButton(FALSE);
				EnableForwardButton(FALSE);
				m_importer_object->ImportMessages();
			}
		}
		else
		{
			for (INT32 i = 0; i < item_count; i++)
			{
				ImporterModelItem* item = (ImporterModelItem*) accountTreeView->GetItemByPosition(items.Get(i));

				if (!item)
					continue;

				if(item->GetType() == IMPORT_ACCOUNT_TYPE)
				{
					m_importer_object->SetImportItem(item);

					BOOL has_account = FALSE;

					if (importsettings)
					{
						has_account = OpStatus::IsSuccess(m_importer_object->ImportSettings());

						OpString importFinished;
						g_languageManager->GetString(Str::D_MAIL_IMPORT_FINISHED, importFinished);

						SetWidgetText("Import_progress_label", importFinished.CStr());
						((OpProgressBar*)GetWidgetByName("Import_progress_bar"))->SetProgress(100, 100);
					}

					if (has_account ||
						(account_id==0 && m_importer_object->CreateImportAccount() == OpStatus::OK) ||
						(account_id!=0 && m_importer_object->SetM2Account(account_id) == OpStatus::OK))
					{
						OpVector<ImporterModelItem> items;
						items.Add(item);
						m_importer_object->SetImportItems(items);

						if (importmessages)
						{
							SetWidgetText("Import_progress_label", UNI_L(""));
							((OpProgressBar*)GetWidgetByName("Import_progress_bar"))->SetProgress(0, 0);
							EnableBackButton(FALSE);
							EnableForwardButton(FALSE);
							m_importer_object->ImportMessages();
						}
					}
				}
			}
		}
	}
#endif // _MACINTOSH_
	else
	{
		ImporterModelItem* item = (ImporterModelItem*)accountTreeView->GetSelectedItem();

		if (item != NULL)
		{
			m_importer_object->SetImportItem(item);

			BOOL has_account = FALSE;

			if (importsettings)
			{
				has_account = OpStatus::IsSuccess(m_importer_object->ImportSettings());

				OpString importFinished;
				g_languageManager->GetString(Str::D_MAIL_IMPORT_FINISHED, importFinished);

				SetWidgetText("Import_progress_label", importFinished.CStr());
				((OpProgressBar*)GetWidgetByName("Import_progress_bar"))->SetProgress(100, 100);
			}

			if (!has_account)
			{
				if ((account_id==0 && m_importer_object->CreateImportAccount()!=OpStatus::OK) ||
					(account_id!=0 && m_importer_object->SetM2Account(account_id)!=OpStatus::OK))
				{
					OpString acc;
					m_importer_object->GetImportAccount(acc);
					m_importer_object->SetM2Account(acc);
				}
			}

			if (importcontacts)
			{
				m_importer_object->ImportContacts();
				OpString importFinished;
				g_languageManager->GetString(Str::D_MAIL_IMPORT_FINISHED, importFinished);

				SetWidgetText("Import_progress_label", importFinished.CStr());
				((OpProgressBar*)GetWidgetByName("Import_progress_bar"))->SetProgress(100, 100);
			}

			if (importmessages)
			{
				SetWidgetText("Import_progress_label", UNI_L(""));
				((OpProgressBar*)GetWidgetByName("Import_progress_bar"))->SetProgress(0, 0);
				EnableBackButton(FALSE);
				EnableForwardButton(FALSE);
				m_importer_object->ImportMessages();
			}
		}
	}
}

void NewAccountWizard::OnImporterProgressChanged(const Importer* importer, const OpStringC& infoMsg, OpFileLength current, OpFileLength total, BOOL simple)
{
	OpString buf;
	if (!buf.Reserve(80))
		return;

	buf.CStr()[0] = 0;

	if (total > 0)
	{
		int percent = current * 100 / total;

		if (simple)
		{
			OpString format;
			g_languageManager->GetString(Str::D_IMPORT_PROCESSING_SIMPLE, format);

			if (format.CStr())
			{
				uni_sprintf(buf.CStr(), format.CStr(),
							percent, importer->GetGrandTotal());
			}
		}
		else
		{
			OpString format;
			g_languageManager->GetString(Str::D_IMPORT_PROCESSING_FULL, format);

			if (format.CStr())
			{
				uni_sprintf(buf.CStr(), format.CStr(),
							percent, current, total, importer->GetGrandTotal());
			}
		}

		((OpProgressBar*)GetWidgetByName("Import_progress_bar"))->SetProgress(current, total);
	}

	SetWidgetText("Import_info_label", infoMsg.CStr());
	SetWidgetText("Import_progress_label", buf.CStr());
}


void NewAccountWizard::OnImporterFinished(const Importer* importer, const OpStringC& infoMsg)
{

	OpString label, format;
	g_languageManager->GetString(Str::D_IMPORT_FINISHED, format);

	if (format.HasContent())
	{
		label.AppendFormat(format.CStr(), importer->GetGrandTotal());
	}

	SetWidgetText("Import_info_label", infoMsg.CStr());
	SetWidgetText("Import_progress_label", label.CStr());

	EnableForwardButton(TRUE);
	EnableBackButton(TRUE);
}

void NewAccountWizard::GetServerInfo(BOOL incoming, OpString& host, unsigned& port, OpString& account_name)
{
	OpString host_total;
#ifdef IRC_SUPPORT
	// Special handling for incoming IRC servers
	if (m_type == AccountTypes::IRC && incoming)
	{
		OpString network_or_server;
		GetWidgetText("Incoming_dropdown", network_or_server);

		ChatNetworkManager* chat_networks = MessageEngine::GetInstance()->GetChatNetworks();

		if (chat_networks)
		{
			if (chat_networks->HasNetwork(ChatNetworkManager::IRCNetworkType, network_or_server))
			{
				account_name.Set(network_or_server);
				chat_networks->ServerList(ChatNetworkManager::IRCNetworkType, network_or_server, host_total, 6667);
				if (host_total.IsEmpty())
					chat_networks->FirstServer(ChatNetworkManager::IRCNetworkType, network_or_server, host_total);
			}
			else
			{
				OpString network_name;
				OpStatus::Ignore(chat_networks->AddNetworkAndServers(
								 ChatNetworkManager::IRCNetworkType, network_or_server, network_name));

				if (network_name.HasContent())
				{
					account_name.Set(network_name);
					chat_networks->ServerList(ChatNetworkManager::IRCNetworkType, network_name, host_total, 6667);
					if (host_total.IsEmpty())
						chat_networks->FirstServer(ChatNetworkManager::IRCNetworkType, network_name, host_total);
				}
			}
		}
	}
	else
#endif
	if (incoming)
		GetWidgetText("Incoming_edit", host_total);
	else
		GetWidgetText("Outgoing_edit", host_total);

	// Split server and port information
	port = 0;
	if (host_total.IsEmpty() || !host.Reserve(host_total.Length()))
		return;

	uni_sscanf(host_total.CStr(), UNI_L("%[^:]:%u"), host.CStr(), &port);
}

void NewAccountWizard::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (!down && pos >= 0 && nclicks >= 2)
	{
		OpInputAction* action = NULL;
		RETURN_VALUE_IF_ERROR(OpInputAction::CreateInputActionsFromString("Forward", action), );
		OpAutoPtr<OpInputAction> ptr(action);
		g_input_manager->InvokeAction(action, this);
	}
}

#endif //M2_SUPPORT
