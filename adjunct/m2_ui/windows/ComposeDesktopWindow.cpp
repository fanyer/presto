/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/desktop_pi/desktop_file_chooser.h"
#include "adjunct/desktop_util/actions/action_utils.h"
#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_util/file_chooser/file_chooser_fun.h"
#include "adjunct/desktop_util/file_utils/filenameutils.h"
#include "adjunct/desktop_util/prefs/PrefsCollectionM2.h"
#include "adjunct/desktop_util/sessions/opsession.h"
#include "adjunct/desktop_util/sessions/SessionAutoSaveManager.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/controller/SimpleDialogController.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick_toolkit/widgets/OpGroup.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick_toolkit/widgets/QuickButton.h"
#include "adjunct/quick_toolkit/widgets/QuickDropDown.h"
#include "adjunct/quick_toolkit/widgets/QuickComposeEdit.h"
#include "adjunct/quick_toolkit/widgets/QuickEdit.h"
#include "adjunct/quick_toolkit/widgets/QuickLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickSkinElement.h"
#include "adjunct/quick_toolkit/widgets/QuickTreeView.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetDecls.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/widgets/OpFindTextBar.h"
#include "adjunct/quick/widgets/OpComposeEdit.h"
#include "adjunct/m2/src/composer/messageencoder.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/index.h"
#include "adjunct/m2/src/engine/store.h"
#include "adjunct/m2/src/include/mailfiles.h"
#include "adjunct/m2/src/util/quote.h"
#include "adjunct/m2_ui/controllers/ComposeWindowOptionsController.h"
#include "adjunct/m2_ui/util/AttachmentOpener.h"
#include "adjunct/m2_ui/windows/ComposeDesktopWindow.h"
// Core includes:
#include "modules/cache/url_stream.h"
#include "modules/encodings/utility/charsetnames.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/inputmanager/inputaction.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/regexp/include/regexp_api.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/windowcommander/src/TransferManagerDownload.h"
#include "adjunct/m2/src/generated/g_message_m2_mapimessage.h"

#define SAVE_DRAFT_TIMER 2*1000

class NoOutgoingServerDialog : public SimpleDialog
{
	public:
		OP_STATUS Init(DesktopWindow* parent_window)
		{
			OpString title;
			OpString message;

			g_languageManager->GetString(Str::S_MISSING_MAIL_SERVER_SETTING, title);
			g_languageManager->GetString(Str::S_MISSING_OUTGOING_MAIL_SERVER, message);

			return SimpleDialog::Init(WINDOW_NAME_NO_OUTGOING_SERVER, title, message, parent_window, TYPE_OK);
		}
		UINT32 OnOk()
		{
			return 0;
		}
};

class InvalidSenderDialog : public SimpleDialog
{
	public:
		OP_STATUS Init(DesktopWindow* parent_window)
		{
			OpString title;
			OpString message;

			g_languageManager->GetString(Str::S_INVALID_FROM_ADDRESS, title);
			g_languageManager->GetString(Str::S_MISSING_MAIL_ADDRESS, message);

			return SimpleDialog::Init(WINDOW_NAME_INVALID_SENDER, title, message, parent_window, TYPE_OK);
		}
		UINT32 OnOk()
		{
			return 0;
		}
};

/***********************************************************************************
**
**	File Chooser listener (for attachments)
**
***********************************************************************************/
class ComposeDesktopWindowChooserListener : public DesktopFileChooserListener
{
	ComposeDesktopWindow*	compose_desktop_window;
public:
	ComposeDesktopWindowChooserListener(ComposeDesktopWindow* compose_desktop_window)
		: compose_desktop_window(compose_desktop_window) {}
	void OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
	{
		for (UINT32 i = 0; i < result.files.GetCount(); i++)
			compose_desktop_window->AddAttachment(result.files.Get(i)->CStr());
		OP_DELETE(this);
	}
};

/***********************************************************************************
**
**	File Chooser listener (for images in HTML messages)
**
***********************************************************************************/
class InsertImageWindowChooserListener : public DesktopFileChooserListener
{
	ComposeDesktopWindow* m_compose_window;
public:
	InsertImageWindowChooserListener(ComposeDesktopWindow* compose_window)
	: m_compose_window(compose_window)
	{}
	void OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
	{
		for (UINT32 i = 0; i < result.files.GetCount(); i++)
		{
			OpString file_url;
			ConvertFullPathtoURL(file_url, result.files.Get(i)->CStr());
			m_compose_window->ImageSelected(file_url);
		}
		OP_DELETE(this);
	}
};

/***********************************************************************************
**
**	ComposeDesktopWindow
**
***********************************************************************************/

OP_STATUS ComposeDesktopWindow::Construct(ComposeDesktopWindow** obj, OpWorkspace* parent_workspace)
{
	*obj = OP_NEW(ComposeDesktopWindow, (parent_workspace));
	if (!*obj)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError((*obj)->init_status))
	{
		(*obj)->Close(TRUE);
		return OpStatus::ERR_NO_MEMORY;
	}
	
	OP_STATUS status = (*obj)->Init();
	if (OpStatus::IsError(status))
	{
		OP_DELETE(*obj);
	}
	return status;
}

ComposeDesktopWindow::ComposeDesktopWindow(OpWorkspace* parent_workspace)
    : m_message_id(0)
	, m_initial_account_id(0)
	, m_attachments_model(2)
	, m_attachments(m_attachments_model)
	, m_save_draft_timer(NULL)
	, m_will_save(FALSE)
	, m_hovering_attachments(FALSE)
	, m_content(NULL)
	, m_widgets(NULL)
	, m_rich_text_editor(NULL)
	, m_toolbar(NULL)
	, m_attachment_list(NULL)
{
	OP_STATUS err;
	err = DesktopWindow::Init(OpWindow::STYLE_DESKTOP, parent_workspace);
	CHECK_STATUS(err);
}

/***********************************************************************************
**
**	Init
**
***********************************************************************************/

OP_STATUS ComposeDesktopWindow::Init()
{
	if (!g_application->HasMail())
	{
		OP_ASSERT(!"should not happen");
		return OpStatus::ERR;
	}

	OpString windowtitle;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_COMPOSEMAIL_DLG_TITLE, windowtitle));
	SetTitle(windowtitle.CStr());

	SetIcon("Window Mail Compose Icon");

	OpWidget* root_widget = GetWidgetContainer()->GetRoot();
	RETURN_IF_ERROR(OpToolbar::Construct(&m_toolbar));
	root_widget->AddChild(m_toolbar);
	m_toolbar->SetName("Mail Compose Toolbar");
	m_toolbar->GetBorderSkin()->SetImage("Mail Compose Toolbar Skin", "Window Skin");

	root_widget->AddChild(GetFindTextBar());
		
	m_widgets = OP_NEW(TypedObjectCollection, ());
	RETURN_OOM_IF_NULL(m_widgets);
	
	RETURN_IF_ERROR(CreateQuickWidget("Mail Compose Window", m_content, *m_widgets));

	m_widgets->Get<QuickDropDown>("cw_FromDropdown")->AddOpWidgetListener(*this);
	m_widgets->Get<QuickDropDown>("cw_EncodingDropdown")->AddOpWidgetListener(*this);
	m_rich_text_editor = m_widgets->Get<QuickRichTextEditor>("cw_RichTextEditor")->GetOpWidget();

	QuickComposeEdit* edit = m_widgets->Get<QuickComposeEdit>("cw_ToEdit");
	edit->GetOpWidget()->autocomp.SetType(AUTOCOMPLETION_CONTACTS);
	edit->GetOpWidget()->SetDropdown(UNI_L("Compose To Contact Menu"));
	RETURN_IF_ERROR(edit->AddChangeDelegate(MAKE_DELEGATE(*this, &ComposeDesktopWindow::StartSaveTimer)));

	edit = m_widgets->Get<QuickComposeEdit>("cw_CcEdit");
	edit->GetOpWidget()->autocomp.SetType(AUTOCOMPLETION_CONTACTS);
	edit->GetOpWidget()->SetDropdown(UNI_L("Compose CC Contact Menu"));
	RETURN_IF_ERROR(edit->AddChangeDelegate(MAKE_DELEGATE(*this, &ComposeDesktopWindow::StartSaveTimer)));

	edit = m_widgets->Get<QuickComposeEdit>("cw_BccEdit");
	edit->GetOpWidget()->autocomp.SetType(AUTOCOMPLETION_CONTACTS);
	edit->GetOpWidget()->SetDropdown(UNI_L("Compose BCC Contact Menu"));
	RETURN_IF_ERROR(edit->AddChangeDelegate(MAKE_DELEGATE(*this, &ComposeDesktopWindow::StartSaveTimer)));

	edit = m_widgets->Get<QuickComposeEdit>("cw_NewsgroupsEdit");
	RETURN_IF_ERROR(edit->AddChangeDelegate(MAKE_DELEGATE(*this, &ComposeDesktopWindow::StartSaveTimer)));

	edit = m_widgets->Get<QuickComposeEdit>("cw_FollowuptoEdit");
	RETURN_IF_ERROR(edit->AddChangeDelegate(MAKE_DELEGATE(*this, &ComposeDesktopWindow::StartSaveTimer)));

	edit = m_widgets->Get<QuickComposeEdit>("cw_ReplytoEdit");
	edit->GetOpWidget()->autocomp.SetType(AUTOCOMPLETION_CONTACTS);
	RETURN_IF_ERROR(edit->AddChangeDelegate(MAKE_DELEGATE(*this, &ComposeDesktopWindow::StartSaveTimer)));
	
	QuickEdit* subject = m_widgets->Get<QuickEdit>("cw_SubjectEdit");
	RETURN_IF_ERROR(subject->AddChangeDelegate(MAKE_DELEGATE(*this, &ComposeDesktopWindow::StartSaveTimer)));
	RETURN_IF_ERROR(subject->AddChangeDelegate(MAKE_DELEGATE(*this, &ComposeDesktopWindow::OnSubjectChanged)));
	subject->GetOpWidget()->EnableSpellcheck();

	m_content->SetContainer(this);
	m_content->SetParentOpWidget(root_widget);

	m_widgets->Get<QuickButton>("cw_To")->GetOpWidget()->SetBold(TRUE);
	m_widgets->Get<QuickButton>("cw_Cc")->GetOpWidget()->SetBold(TRUE);
	m_widgets->Get<QuickButton>("cw_Bcc")->GetOpWidget()->SetBold(TRUE);
	if (g_languageManager->GetWritingDirection() == OpLanguageManager::LTR)
	{
		m_widgets->Get<QuickButton>("cw_To")->GetOpWidget()->SetJustify(JUSTIFY_LEFT, TRUE);
		m_widgets->Get<QuickButton>("cw_Cc")->GetOpWidget()->SetJustify(JUSTIFY_LEFT, TRUE);
		m_widgets->Get<QuickButton>("cw_Bcc")->GetOpWidget()->SetJustify(JUSTIFY_LEFT, TRUE);
	}
	else
	{
		m_widgets->Get<QuickButton>("cw_To")->GetOpWidget()->SetJustify(JUSTIFY_RIGHT, TRUE);
		m_widgets->Get<QuickButton>("cw_Cc")->GetOpWidget()->SetJustify(JUSTIFY_RIGHT, TRUE);
		m_widgets->Get<QuickButton>("cw_Bcc")->GetOpWidget()->SetJustify(JUSTIFY_RIGHT, TRUE);
	}

	WIDGET_FONT_INFO wfi = m_widgets->Get<QuickButton>("cw_To")->GetOpWidget()->font_info;
	m_widgets->Get<QuickLabel>("cw_From")->GetOpWidget()->SetFontInfo(wfi.font_info, wfi.size, FALSE, 7, JUSTIFY_LEFT);
	m_widgets->Get<QuickLabel>("cw_Newsgroups")->GetOpWidget()->SetFontInfo(wfi.font_info, wfi.size, FALSE, 7, JUSTIFY_LEFT);
	m_widgets->Get<QuickLabel>("cw_Followupto")->GetOpWidget()->SetFontInfo(wfi.font_info, wfi.size, FALSE, 7, JUSTIFY_LEFT);
	m_widgets->Get<QuickLabel>("cw_Replyto")->GetOpWidget()->SetFontInfo(wfi.font_info, wfi.size, FALSE, 7, JUSTIFY_LEFT);
	m_widgets->Get<QuickLabel>("cw_Subject")->GetOpWidget()->SetFontInfo(wfi.font_info, wfi.size, FALSE, 7, JUSTIFY_LEFT);

	OpString size, attachment;
	g_languageManager->GetString(Str::S_COMPOSEMAIL_ATTACHMENT, attachment);
	g_languageManager->GetString(Str::S_COMPOSEMAIL_SIZE, size);

	m_attachments_model.SetColumnData(0, attachment.CStr());
	m_attachments_model.SetColumnData(1, size.CStr());

	m_attachment_list = m_widgets->Get<QuickTreeView>("cw_AttachmentList")->GetOpWidget();
	m_attachment_list->SetTreeModel(&m_attachments_model);
	m_attachment_list->SetColumnWeight(0, 200);
	m_attachment_list->SetMultiselectable(TRUE);
	m_attachment_list->SetShowColumnHeaders(FALSE);
	m_attachment_list->SetTabStop(g_op_ui_info->IsFullKeyboardAccessActive());
	// Attachment area should be in the tabbing order before all the other fields
	m_widgets->Get<QuickSkinElement>("cw_AttachmentArea")->SetZ(OpWidget::Z_TOP);

	m_save_draft_timer = OP_NEW(OpTimer, ());
	RETURN_OOM_IF_NULL(m_save_draft_timer);

	m_save_draft_timer->SetTimerListener(this);

	return g_pcm2->RegisterListener(this);
}

/***********************************************************************************
**
**	InitMessage
**
***********************************************************************************/

OP_STATUS ComposeDesktopWindow::InitMessage(MessageTypes::CreateType create_type, INT32 message_id, UINT16 account_id)
{
	Message message;

	// remember so we can update parent when sending (has been replied to etc)
	m_create_type = create_type;
	m_message_id = 0;

	AccountManager* account_manager = g_m2_engine->GetAccountManager();
	Account* account = NULL;

	if (message_id)
	{
		OpStatus::Ignore(g_m2_engine->GetMessage(message, message_id, TRUE));
		if (message.IsFlagSet(Message::IS_NEWS_MESSAGE))
		{
			switch(create_type)
			{
				case MessageTypes::REPLY:
				case MessageTypes::QUICK_REPLY:
				case MessageTypes::REPLY_TO_LIST:
				case MessageTypes::REPLY_TO_SENDER:
					create_type = MessageTypes::FOLLOWUP;
					break;
				case MessageTypes::REPLYALL:
					create_type = MessageTypes::REPLY_AND_FOLLOWUP;
					break;
			}
		}
	}

	QuickDropDown* from_dropdown = m_widgets->Get<QuickDropDown>("cw_FromDropdown");
	// create_type may be clobbered by longjmp (TRAP on Unix) before we use it:
	OP_MEMORY_VAR MessageTypes::CreateType new_create_type(create_type);

	for (OP_MEMORY_VAR int i = 0; i < account_manager->GetAccountCount(); i++)
	{
		account = account_manager->GetAccountByIndex(i);

		if (account)
		{
			OpString16 mail, mailaddress;

			account->GetEmail(mailaddress);
			account->GetFormattedEmail(mail, TRUE);

			if ( account->GetIncomingProtocol() == AccountTypes::NEWS ||
                 (account->GetOutgoingProtocol() == AccountTypes::SMTP) )
			{
                if (mail.IsEmpty())
                {
					g_languageManager->GetString(Str::S_COMPOSEMAIL_MISSING_EMAIL, mail);
                }

				if (account->GetAccountName().HasContent() && account->GetAccountName() != mail)
				{
					mail.AppendFormat(UNI_L("   (%s)"), account->GetAccountName().CStr());
				}

				RETURN_IF_ERROR(from_dropdown->GetOpWidget()->AddItem(mail.CStr(), -1, NULL, account->GetAccountId()));
			}
		}
	}

	//Create the new message
    RETURN_IF_ERROR(g_m2_engine->CreateMessage(m_message, account_id, message_id, new_create_type));

	//Set body-text here, to have it available in case OnChange is called (most likely by SelectAccount below)
	OpString body_text;
	BOOL gotHTML = TRUE;
	m_message_id = m_message.GetId();
	if (OpStatus::IsError(m_message.GetRawBody(body_text) ))		
		body_text.Empty();

	gotHTML = m_message.IsHTML();

	OpString folder;
	if (OpStatus::IsSuccess(MailFiles::GetDraftsFolder(folder)))
	{
		m_attachments.Init(m_message_id, folder);
		if (m_attachments.GetCount() > 0)
		{
			m_attachment_list->SetShowColumnHeaders(TRUE);
			m_attachment_list->GetBorderSkin()->SetImage("Treeview Skin");
		}
	}

	for (int attachment_index = m_message.GetAttachmentCount() - 1; attachment_index >= 0; attachment_index--)
	{
		OpString attachment_filename, suggested_filename;

		if (OpStatus::IsError(m_message.GetAttachmentFilename(attachment_index, attachment_filename)) ||
			OpStatus::IsError(m_message.GetAttachmentSuggestedFilename(attachment_index, suggested_filename)))
			continue;

		if (m_message.GetAttachmentURL(attachment_index))	
			RETURN_IF_ERROR(AddAttachment(attachment_filename.CStr(), suggested_filename.CStr(), *m_message.GetAttachmentURL(attachment_index), m_message.IsInlinedAttachment(attachment_index)));
		else
			RETURN_IF_ERROR(AddAttachment(attachment_filename.CStr()));
	}

	//Fill item in the encoding dropdown (will be selected in the call to SelectAccount())
	const char *const *charsets_array = g_charsetManager->GetCharsetNames();
	QuickDropDown* encoding_dropdown = m_widgets->Get<QuickDropDown>("cw_EncodingDropdown");

	//Update charset
	OpString8 message_charset8;
	OpString message_charset, default_charset, def_enc;
	
	RETURN_IF_ERROR(m_message.GetCharset(message_charset8));

	if (message_charset8.IsEmpty() && account)
		RETURN_IF_ERROR(account->GetCharset(message_charset8));

	if (message_charset8.IsEmpty())
		RETURN_IF_ERROR(message_charset.Set(UNI_L("utf-8")));
	else
		RETURN_IF_ERROR(message_charset.Set(message_charset8.CStr()));

	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_AUTO_DETECTED_ENCODING,def_enc));
	RETURN_IF_ERROR(default_charset.AppendFormat(def_enc.CStr(), message_charset.CStr()));
	if (encoding_dropdown && charsets_array)
	{
		int index = 0;
		INT32 tmp_index;
		OpString tmp_item;
		encoding_dropdown->AddItem(default_charset.CStr(), 0,&tmp_index);
		while (charsets_array[index] && OpStatus::IsSuccess(tmp_item.Set(charsets_array[index])))
		{
			if (strcmp(charsets_array[index], "utf-16")!=0 && //We do not want our users to send messages in utf-16 or utf-32!
				strcmp(charsets_array[index], "utf-32")!=0)
			{
				RETURN_IF_ERROR(encoding_dropdown->AddItem(tmp_item.CStr(), -1, &tmp_index));
			}
			index++;
		}
	}

	RETURN_IF_ERROR(m_rich_text_editor->Init(this, gotHTML && !body_text.IsEmpty(), this, m_message_id));
	//Select item in the account dropdown
	SelectAccount(m_message.GetAccountId(),FALSE);
	RETURN_IF_ERROR(account_manager->GetAccountById(m_message.GetAccountId(), account));
	if (account)
	{
		OpString signature;
		BOOL isHTMLsignature;
		account->GetSignature(signature,isHTMLsignature);
		m_rich_text_editor->SetSignature(signature,isHTMLsignature,FALSE);
		
		// has to be set after selecting the account
		m_message.SetFlag(Message::IS_NEWS_MESSAGE, account->GetIncomingProtocol() == AccountTypes::NEWS);
	}

	// Set the direction of the message
	if (account)
	{
		// if the account default is automatic and it's not a new message take a first guess based on the previous message
		if (account->GetDefaultDirection() == AccountTypes::DIR_AUTOMATIC &&
			message_id && m_create_type != MessageTypes::NEW)
		{
			SetDirection(message.GetTextDirection(), FALSE, TRUE);
		}
		else
		{
			// else just use account default
			SetDirection(account->GetDefaultDirection(), FALSE);
		}
	}

	// don't set signature if we are redirecting or reopening a draft
	if (m_create_type != MessageTypes::REDIRECT && m_create_type != MessageTypes::REOPEN)
		m_rich_text_editor->UpdateSignature();

	if (gotHTML && !body_text.IsEmpty())
	{
		RETURN_IF_ERROR(m_rich_text_editor->SetMailContentAndUpdateView(body_text, FALSE, m_message.GetContextId()));
		Relayout();
	}
	else
	{
		RETURN_IF_ERROR(m_rich_text_editor->SetMailContentAndUpdateView(body_text, TRUE, m_message.GetContextId()));
	}
	
	if (m_message_id)
		RETURN_IF_ERROR(g_m2_engine->OnStartEditingMessage(m_message_id));

	m_message.SetFlag(Message::IS_OUTGOING, TRUE);

	Header::HeaderValue string;
	RETURN_IF_ERROR(m_message.GetTo(string, TRUE));
	RETURN_IF_ERROR(m_widgets->Get<QuickComposeEdit>("cw_ToEdit")->SetText(string));

	RETURN_IF_ERROR(m_message.GetCc(string, TRUE));
	RETURN_IF_ERROR(m_widgets->Get<QuickComposeEdit>("cw_CcEdit")->SetText(string));

	RETURN_IF_ERROR(m_message.GetBcc(string, TRUE));
	RETURN_IF_ERROR(m_widgets->Get<QuickComposeEdit>("cw_BccEdit")->SetText(string));

	if (m_message.IsFlagSet(Message::IS_NEWS_MESSAGE))
	{
		RETURN_IF_ERROR(m_message.GetHeaderValue(Header::NEWSGROUPS, string));
		RETURN_IF_ERROR(m_widgets->Get<QuickComposeEdit>("cw_NewsgroupsEdit")->SetText(string));

		RETURN_IF_ERROR(m_message.GetHeaderValue(Header::FOLLOWUPTO, string));
		RETURN_IF_ERROR(m_widgets->Get<QuickComposeEdit>("cw_FollowuptoEdit")->SetText(string));
	}

	RETURN_IF_ERROR(m_message.GetReplyTo(string, TRUE));
	RETURN_IF_ERROR(m_widgets->Get<QuickComposeEdit>("cw_ReplytoEdit")->SetText(string));

	RETURN_IF_ERROR(m_message.GetHeaderValue(Header::SUBJECT, string));
	RETURN_IF_ERROR(m_widgets->Get<QuickEdit>("cw_SubjectEdit")->SetText(string));

	
	UpdateMessageWidth();
	UpdateVisibleHeaders();
	SetFocusToHeaderOrBody();
	return OpStatus::OK;

}

void ComposeDesktopWindow::UpdateMessageWidth()
{
	if (g_pcm2->GetIntegerPref(PrefsCollectionM2::PaddingInComposeWindow))
	{
		FontAtt mailfont;
		g_pcfontscolors->GetFont(OP_SYSTEM_FONT_EMAIL_COMPOSE, mailfont);
		WIDGET_FONT_INFO info;
		info.size = mailfont.GetSize();
		info.weight = mailfont.GetWeight();
		info.font_info = styleManager->GetFontInfo(mailfont.GetFontNumber());
		unsigned message_width = WidgetUtils::GetFontWidth(info) * 105;
		if (GetWindowCommander()) 
			message_width*= GetWindowCommander()->GetScale() / 100;

		message_width = max(message_width, m_rich_text_editor->GetPreferredWidth());
		m_widgets->Get<QuickRichTextEditor>("cw_RichTextEditor")->SetPreferredWidth(message_width);
	}
	else
		m_widgets->Get<QuickRichTextEditor>("cw_RichTextEditor")->SetPreferredWidth(WidgetSizes::Infinity);
}

void ComposeDesktopWindow::StartSaveTimer(OpWidget&) 
{ 
	SetWillSave(TRUE);
	m_save_draft_timer->Stop(); 
	m_save_draft_timer->Start(SAVE_DRAFT_TIMER); 
}

void ComposeDesktopWindow::OnSubjectChanged(OpWidget& widget)
{
	OpString title;
	widget.GetText(title);
	if (title.HasContent())
		SetTitle(title.CStr());
	else
	{
		g_languageManager->GetString(Str::DI_IDSTR_M2_COMPOSEMAIL_DLG_TITLE, title);
		SetTitle(title.CStr());
	}
}

void ComposeDesktopWindow::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
	if (pref == PrefsCollectionM2::PaddingInComposeWindow)
	{
		UpdateMessageWidth();
	}
	else if (pref == PrefsCollectionM2::MailComposeHeaderDisplay)
	{
		UpdateVisibleHeaders();
	}
}

void ComposeDesktopWindow::SetFocusToHeaderOrBody()
{
	OpComposeEdit* edit = NULL;

	if (m_message.IsFlagSet(Message::IS_NEWS_MESSAGE) && m_widgets->Get<QuickComposeEdit>("cw_NewsgroupsEdit")->IsVisible())
	{
		edit = m_widgets->Get<QuickComposeEdit>("cw_NewsgroupsEdit")->GetOpWidget();
	}
	else
	{
		edit = m_widgets->Get<QuickComposeEdit>("cw_ToEdit")->GetOpWidget();
	}

	if (edit->GetTextLength() == 0)
	{
		edit->SetFocus(FOCUS_REASON_OTHER);
	}
	else
	{
		OpEdit* edit = m_widgets->Get<QuickEdit>("cw_SubjectEdit")->GetOpWidget();

		if (edit->IsVisible() && edit->GetTextLength() == 0)
		{
			edit->SetFocus(FOCUS_REASON_OTHER);
		}
		else
		{
			m_rich_text_editor->SetFocusToBody();
		}
	}

}

void ComposeDesktopWindow::OnSettingsChanged(DesktopSettings* settings)
{
	DesktopWindow::OnSettingsChanged(settings);
	if (settings->IsChanged(SETTINGS_TOOLBAR_CONTENTS) || settings->IsChanged(SETTINGS_TOOLBAR_SETUP))
	{	
		m_rich_text_editor->InitFontDropdown();
	}
}

// ----------------------------------------------------

OP_STATUS ComposeDesktopWindow::SetHeaderValue(Header::HeaderType type, const OpStringC* string)
{
	switch(type)
	{
	case Header::TO:
		{
			return m_widgets->Get<QuickComposeEdit>("cw_ToEdit")->SetText(*string);
		}
		break;
	case Header::CC:
		{
			return m_widgets->Get<QuickComposeEdit>("cw_CcEdit")->SetText(*string);
		}
		break;
	case Header::BCC:
		{
			return m_widgets->Get<QuickComposeEdit>("cw_BccEdit")->SetText(*string);
		}
		break;
	case Header::SUBJECT:
		{
			return m_widgets->Get<QuickEdit>("cw_SubjectEdit")->SetText(*string);
		}
		break;
	case Header::NEWSGROUPS:
		{
			return m_widgets->Get<QuickComposeEdit>("cw_NewsgroupsEdit")->SetText(*string);
		}
		break;
	case Header::FOLLOWUPTO:
		{
			return m_widgets->Get<QuickComposeEdit>("cw_FollowuptoEdit")->SetText(*string);
		}
		break;
	case Header::REPLYTO:
		{
			return m_widgets->Get<QuickComposeEdit>("cw_ReplytoEdit")->SetText(*string);
		}
		break;
	default:
		{
			return OpStatus::ERR;
		}
	}
}

// ----------------------------------------------------

OP_STATUS ComposeDesktopWindow::SetMessageBody(const OpStringC& message)
{
	if (message.IsEmpty())
		return OpStatus::OK;

	m_rich_text_editor->SetPlainTextBody(message);
	return OpStatus::OK;
}

void ComposeDesktopWindow::SelectAccount(UINT16 account_id, BOOL propagate_event, AccountTypes::AccountTypeCategory account_type_category)
{
	if (account_id==0)
	{
		account_id = g_m2_engine->GetAccountManager()->GetDefaultAccountId(account_type_category);
	}

	OpDropDown *from_dropdown = m_widgets->Get<QuickDropDown>("cw_FromDropdown")->GetOpWidget();
	INT32 pos, dropdown_count = from_dropdown->CountItems();
	for (pos = 0; pos < dropdown_count; pos++)
	{
		if ((pos==0 && account_id==0) || //Select first account if not even default account is found
			((INTPTR)(from_dropdown->GetItemUserData(pos)) == account_id))
		{
			from_dropdown->SelectItem(pos, TRUE);
			if (propagate_event)
				OnChange(from_dropdown, FALSE);
			break;
		}
	}

	m_initial_account_id = account_id;
	SetFocusToHeaderOrBody();
}


void ComposeDesktopWindow::GetCorrectedCharSet(OpString& body, OpString8& charset_to_use)
{

	OpDropDown* encoding_dropdown = m_widgets->Get<QuickDropDown>("cw_EncodingDropdown")->GetOpWidget();

	if (encoding_dropdown->GetSelectedItem() == 0)
	{
		// we use the account default (or the previous message's) charset
		m_message.GetCharset(charset_to_use);
		int invalid_count;
		OpString invalid_chars;
		// check if it's valid for the characters in the mail
		if (OpStatus::IsSuccess(m_message.IsBodyCharsetCompatible(body, invalid_count, invalid_chars)) 
			&& invalid_count > 0)
		{
			// it shouldn't be used, there are illegal characters, let's use utf-8
			charset_to_use.Set("utf-8");
			OpString def_enc,default_encoding;
			g_languageManager->GetString(Str::S_AUTO_DETECTED_ENCODING,def_enc);
			default_encoding.AppendFormat(def_enc.CStr(), UNI_L("utf-8"));
			
			// update the UI as well
			encoding_dropdown->ChangeItem(default_encoding.CStr(),0);
			// for some reason I need to invalidate the dropdown to make it paint the changes
			encoding_dropdown->InvalidateAll();
			m_message.SetCharset(charset_to_use,FALSE);
		}
	}
	else 
	{
		// the user has made a choice, we shouldn't detect if it's reasonable
		charset_to_use.Set(encoding_dropdown->GetItemText(encoding_dropdown->GetSelectedItem()));
		m_message.SetCharset(charset_to_use, TRUE);
	}
}

OP_STATUS ComposeDesktopWindow::SelectEncoding(const OpStringC8& charset)
{
	OpString16 charset16;
	RETURN_IF_ERROR(charset16.Set(charset));

	SelectEncoding(charset16);
	return OpStatus::OK;
}

void ComposeDesktopWindow::SelectEncoding(const OpStringC16& charset)
{
	OpDropDown* encoding_dropdown = m_widgets->Get<QuickDropDown>("cw_EncodingDropdown")->GetOpWidget();

	INT32 encoding_index = encoding_dropdown->CountItems();
	while (encoding_index>0)
	{
		if (charset.CompareI(encoding_dropdown->GetItemText(--encoding_index)) == 0)
		{
			OpString charset_auto;
			encoding_dropdown->SelectItem(encoding_index, TRUE);
			break;
			
		}
	}

}

// ----------------------------------------------------

void ComposeDesktopWindow::OnClose(BOOL user_initiated)
{
	if (m_will_save)
	{
		OpStatus::Ignore(SaveMessage(FALSE));

		g_session_auto_save_manager->SaveCurrentSession();
	}

	if (!g_m2_engine->GetIndexById(IndexTypes::DRAFTS)->Contains(m_message_id))
	{
		m_attachments.RemoveAll();
	}

	DesktopWindow::OnClose(user_initiated);
}

ComposeDesktopWindow::~ComposeDesktopWindow()
{
	g_pcm2->UnregisterListener(this);
	OP_DELETE(m_widgets);
	OP_DELETE(m_content);
	
	OP_DELETE(m_save_draft_timer);

	g_m2_engine->OnStopEditingMessage(m_message_id);
}

BOOL ComposeDesktopWindow::OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked)
{
	switch (widget->GetType())
	{
		case OpTypedObject::WIDGET_TYPE_COMPOSE_EDIT:
		case OpTypedObject::WIDGET_TYPE_EDIT:
		{
			g_application->GetMenuHandler()->ShowPopupMenu("Toolbar Edit Item Popup Menu", PopupPlacement::AnchorAtCursor(), 0, keyboard_invoked);
			return TRUE;
		}
	}
	return FALSE;
}

/***********************************************************************************
**
**	OnLayout
**
***********************************************************************************/

void ComposeDesktopWindow::OnLayout()
{
	OpRect window_rect;
	GetBounds(window_rect);
	window_rect = m_toolbar->LayoutToAvailableRect(window_rect);
	window_rect = GetFindTextBar()->LayoutToAvailableRect(window_rect);
	m_content->Layout(window_rect);
}


/***********************************************************************************
**
**	OnSessionReadL
**
***********************************************************************************/

void ComposeDesktopWindow::OnSessionReadL(const OpSessionWindow* session_window)
{
	INT32 message_id = 0;
	TRAPD(err, message_id = session_window->GetValueL("mail id"));
	InitMessage(message_id==0 ? MessageTypes::NEW : MessageTypes::REOPEN, message_id);
}

/***********************************************************************************
**
**	OnSessionWriteL
**
***********************************************************************************/

void ComposeDesktopWindow::OnSessionWriteL(OpSession* session, OpSessionWindow* session_window, BOOL shutdown)
{
	if (shutdown)
	{
		OpStatus::Ignore(SaveMessage(FALSE));
	}

	session_window->SetTypeL(OpSessionWindow::MAIL_COMPOSE_WINDOW);
	session_window->SetValueL("mail id", m_message_id);
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL ComposeDesktopWindow::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
    	case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_OPEN:
				{
					const BOOL enabled = (m_attachment_list->GetSelectedItemCount() > 0);
					child_action->SetEnabled(enabled);

					return TRUE;
				}
				case OpInputAction::ACTION_REMOVE_ATTACHMENT:
				{
					const BOOL enabled = (m_attachment_list->GetSelectedItemCount() > 0);
					child_action->SetEnabled(enabled);

					return TRUE;
				}
				case OpInputAction::ACTION_TOGGLE_HTML_EMAIL: 
				{ 
					Account* account;
					DesktopOpDropDown* from_dropdown = m_widgets->Get<QuickDropDown>("cw_FromDropdown")->GetOpWidget();
					g_m2_engine->GetAccountManager()->GetAccountById( (UINT16)(from_dropdown->GetItemUserData(from_dropdown->GetSelectedItem())), account);
					if (account)
					{
						child_action->SetEnabled(account->GetIncomingProtocol() != AccountTypes::NEWS); 
					}
					break; // SetSelected is handled in the RichTextEditor
				} 
				case OpInputAction::ACTION_SEND_MAIL: 
				{ 
					child_action->SetEnabled(g_m2_engine->GetStore()->HasFinishedLoading());
					return TRUE;
				}
				case OpInputAction::ACTION_SHOW_POPUP_MENU:
				{
					if (child_action->IsActionDataStringEqualTo(UNI_L("Display Headers Menu")))
					{
						child_action->SetEnabled(TRUE);
						return TRUE;
					}
					return FALSE;
				}
            }
			break;
        }
		case OpInputAction::ACTION_SHOW_POPUP_MENU:
		{
			if (action->IsActionDataStringEqualTo(UNI_L("Display Headers Menu")))
			{
				OpWidget* button = NULL;
				if (action->GetFirstInputContext() && action->GetFirstInputContext()->GetType() == OpTypedObject::WIDGET_TYPE_TOOLBAR)
					button = static_cast<OpWidget*>(action->GetFirstInputContext())->GetWidgetByNameInSubtree("tb_DisplayHeadersMenu");
				
				if (!button)
					button = GetWidgetByName("tb_DisplayHeadersMenu");
				
				DialogContext* controller = button
						? OP_NEW(ComposeWindowOptionsController, (button))
						: NULL;
				ShowDialog(controller, g_global_ui_context, this);
				return TRUE;
			}
			return FALSE;
		}
		case OpInputAction::ACTION_COMPOSE_TO_CONTACT:
			AppendContact(m_widgets->Get<QuickComposeEdit>("cw_ToEdit")->GetOpWidget(), action->GetActionData(),
				action->GetActionDataString());
			return TRUE;

		case OpInputAction::ACTION_COMPOSE_CC_CONTACT:
			AppendContact(m_widgets->Get<QuickComposeEdit>("cw_CcEdit")->GetOpWidget(), action->GetActionData(),
				action->GetActionDataString());
			return TRUE;

		case OpInputAction::ACTION_COMPOSE_BCC_CONTACT:
			AppendContact(m_widgets->Get<QuickComposeEdit>("cw_BccEdit")->GetOpWidget(), action->GetActionData(),
				action->GetActionDataString());
			return TRUE;

		case OpInputAction::ACTION_DISCARD_DRAFT:
			m_attachments.RemoveAll();
			m_save_draft_timer->Stop();
			SetWillSave(FALSE);
			if (m_message_id)
			{
				OpINT32Vector id;
				id.Add(m_message_id);

				// Make sure that we can remove the draft
				g_m2_engine->OnStopEditingMessage(m_message_id);
				g_m2_engine->RemoveMessages(id, FALSE);
				id.Clear();
			}

			Close();

			return TRUE;


		case OpInputAction::ACTION_SAVE_DOCUMENT:
		case OpInputAction::ACTION_SAVE_DRAFT:
			if (OpStatus::IsError(SaveMessage(FALSE)))
			{
				m_save_draft_timer->Start(SAVE_DRAFT_TIMER) ;
				SetWillSave(TRUE);
			}			
			return TRUE;

		case OpInputAction::ACTION_SEND_MAIL:
		{
			OpString invalid_mails;
			OpStatus::Ignore(SaveMessage(FALSE));
			if (OpStatus::IsError(ValidateMailAdresses(Header::TO, invalid_mails))
				|| OpStatus::IsError(ValidateMailAdresses(Header::CC, invalid_mails))
				|| OpStatus::IsError(ValidateMailAdresses(Header::BCC, invalid_mails)))
			{
				return TRUE;
			}
			else if (!invalid_mails.IsEmpty())
			{
				OpString tmp, field, title, content;
				RETURN_IF_ERROR(g_languageManager->GetString(Str::D_MAIL_COMPOSE_WINDOW_ADDRESS_ERROR_TITLE, title));
				RETURN_IF_ERROR(g_languageManager->GetString(Str::D_MAIL_COMPOSE_WINDOW_ADDRESS_ERROR_CONTENT, tmp));
				RETURN_IF_ERROR(content.AppendFormat(tmp.CStr(), invalid_mails.CStr()));
				SimpleDialog* error_dialog = OP_NEW(SimpleDialog, ());
				if (error_dialog)
					RETURN_IF_ERROR(error_dialog->Init(WINDOW_NAME_INVALID_MESSAGE, title, content, this, Dialog::TYPE_OK, Dialog::IMAGE_ERROR, TRUE));
			}
			else if (OpStatus::IsSuccess(SaveMessage(TRUE)))
				Close();

			return TRUE;
		}
		case OpInputAction::ACTION_ADD_ATTACHMENT:
		{
			if (action->GetActionData() == 1 && action->GetFirstInputContext() != m_attachment_list)
				return FALSE;

			AskForAttachment();
			Relayout(); //In case attachment-list was hidden, and it now contains a file
			return TRUE;
		}

		case OpInputAction::ACTION_OPEN:
		{
			DesktopFileChooser *chooser;
			RETURN_VALUE_IF_ERROR(GetChooser(chooser), TRUE);
			//Get array of selected items;
			OpINT32Vector items;
			int item_count = m_attachment_list->GetSelectedItems(items);
			for (int i = 0; i < item_count; i++)
			{
				OpString suggested_filename;
				URL url;
				RETURN_VALUE_IF_ERROR(m_attachments.GetAttachmentInfoById(items.Get(i), 0, suggested_filename, url), TRUE);
				if (AttachmentOpener::OpenAttachment(&url, chooser, this) == FALSE)
					return TRUE;
			}

			return TRUE;
		}
		case OpInputAction::ACTION_REMOVE_ATTACHMENT:
		{
			if (m_attachment_list->IsFocused())
			{
				//Get array of selected items;
				OpINT32Vector items;
				int item_count = m_attachment_list->GetSelectedItems(items);

				if (item_count == 0)
				{
					return TRUE; //Nothing to do
				}

				for (; item_count>0; item_count--)
				{
					m_attachments.Remove(items.Get(item_count - 1));
				}
				
				if (m_attachments_model.GetCount() == 0)
				{
					m_attachment_list->GetBorderSkin()->SetImage("Mail Compose Attachment");
					m_attachment_list->SetShowColumnHeaders(FALSE);
					m_message.SetFlag(StoreMessage::HAS_ATTACHMENT, FALSE);
				}

				Relayout(); //In case attachment-list was forced shown, and no longer contains a file
				return TRUE;
			}
			break;
		}
		case OpInputAction::ACTION_CHANGE_DIRECTION_AUTOMATIC:
		{
			SetDirection(AccountTypes::DIR_AUTOMATIC);
			return TRUE;
		}
		case OpInputAction::ACTION_CHANGE_DIRECTION_TO_LTR:
		{
			SetDirection(AccountTypes::DIR_LEFT_TO_RIGHT);
			return TRUE;
		}
		case OpInputAction::ACTION_CHANGE_DIRECTION_TO_RTL:
		{
			SetDirection(AccountTypes::DIR_RIGHT_TO_LEFT);
			return TRUE;
		}
		case OpInputAction::ACTION_SHOW_CONTEXT_MENU:
		{
			g_application->GetMenuHandler()->ShowPopupMenu("Mail Compose Popup Menu", PopupPlacement::AnchorAtCursor(), 0, TRUE);
			return TRUE;
		}
		case OpInputAction::ACTION_SET_PRIORITY:
		{
			RETURN_VALUE_IF_ERROR(m_message.SetPriority(action->GetActionData()), FALSE);

			OpButton* button = m_widgets->Get<QuickButton>("cw_PriorityButton")->GetOpWidget();
			switch(action->GetActionData())
			{
				case 1:
				case 2:
					button->GetForegroundSkin()->SetImage("Priority High");
					break;
				case 4:
				case 5:
					button->GetForegroundSkin()->SetImage("Priority Low");
					break;
				case 3:
				default:
					button->GetForegroundSkin()->SetImage("Priority Normal");
					break;
			}
			StartSaveTimer(*button);
			return TRUE;
		}
	}

	if (m_rich_text_editor && m_rich_text_editor->OnInputAction(action))
		return TRUE;

	if (action->IsLowlevelAction())
		return FALSE;

	return DesktopWindow::OnInputAction(action);
}

OP_STATUS ComposeDesktopWindow::SaveMessage(BOOL for_sending)
{
	for_sending = for_sending || g_m2_engine->GetIndexById(IndexTypes::OUTBOX)->Contains(m_message_id);
	m_save_draft_timer->Stop();

	RETURN_IF_ERROR(SetMessageAccountFromInput(for_sending));

	RETURN_IF_ERROR(SetMessageHeadersFromInput(for_sending));

	MessageEncoder encoder;
	OpAutoPtr<OpQuote> wrapper(m_message.CreateQuoteObject());
	encoder.SetTextWrapper(wrapper);
	Account* account;
	OpDropDown *from_dropdown = m_widgets->Get<QuickDropDown>("cw_FromDropdown")->GetOpWidget();
	g_m2_engine->GetAccountManager()->GetAccountById((UINT16)(INTPTR)from_dropdown->GetItemUserData(from_dropdown->GetSelectedItem()), account);
	if (account)
		encoder.SetAllow8BitTransfer(account->GetAllow8BitTransfer());

	// Set plain text
	OpString plain_text;
	RETURN_IF_ERROR(m_rich_text_editor->GetTextEquivalent(plain_text));

	OpString8 charset_to_use;
	GetCorrectedCharSet(plain_text ,charset_to_use);
	if (m_rich_text_editor->IsHTML())
	{
		// double check if we need to change the encoding (there might be a mismatch between body and HTML source)
		// like bug DSK-240878 where the body is in english and the font is in japanese
		OpString html_content;
		RETURN_IF_ERROR(m_rich_text_editor->GetHTMLSource(html_content));
		GetCorrectedCharSet(plain_text ,charset_to_use);
	}

	RETURN_IF_ERROR(encoder.SetPlainTextBody(plain_text, charset_to_use));

	Header* header = m_message.GetFirstHeader();
	while (header)
	{	
		Header::HeaderValue value;
		RETURN_IF_ERROR(header->GetValue(value));
		if (header->GetType() == Header::UNKNOWN)
		{
			OpString8 name;
			RETURN_IF_ERROR(header->GetName(name));
			RETURN_IF_ERROR(encoder.AddHeaderValue(name, value, charset_to_use));
		}
		else
			RETURN_IF_ERROR(encoder.AddHeaderValue(header->GetType(), value, charset_to_use));

		header = static_cast<Header*>(header->Suc());
	}

	// Set html
	if (m_rich_text_editor->IsHTML())
	{
		OpAutoPtr<Upload_Multipart> html_part;
		RETURN_IF_ERROR(m_rich_text_editor->GetEncodedHTMLPart(html_part, charset_to_use));
		RETURN_IF_ERROR(encoder.SetHTMLBody(OpAutoPtr<Upload_Base>(html_part.release())));
	}

	// Attachments
	if (for_sending)
		RETURN_IF_ERROR(m_attachments.EncodeForSending(encoder, charset_to_use));

	OpString8 encoded_message;
	RETURN_IF_ERROR(encoder.Encode(encoded_message, charset_to_use, for_sending));

	RETURN_IF_ERROR(m_message.SetRawMessage(encoded_message.CStr()));

	if (!for_sending)
	{
		RETURN_IF_ERROR(SaveAsDraft());
		OP_STATUS ret = m_attachments.Save(m_message_id);

		if (ret == OpStatus::ERR_NO_ACCESS)
		{
			OpString dialog_title;
			OpString dialog_message;
	
			RETURN_IF_ERROR(g_languageManager->GetString(Str::D_M2_ADD_ATTACHMENT_FAILED_TITLE, dialog_title));
			RETURN_IF_ERROR(g_languageManager->GetString(Str::D_M2_ADD_ATTACHMENT_FAILED_BODY, dialog_message));
	
			SimpleDialog* warning_dialog = OP_NEW(SimpleDialog, ());
			if (warning_dialog)
				RETURN_IF_ERROR(warning_dialog->Init(WINDOW_NAME_INVALID_MESSAGE, dialog_title, dialog_message, this, Dialog::TYPE_OK, Dialog::IMAGE_WARNING, TRUE));

			return OpStatus::ERR;
		}
		RETURN_IF_ERROR(ret);
	}
	else
	{
		RETURN_IF_ERROR(CheckForOutgoingServer());
		RETURN_IF_ERROR(SendMessage());
	}

	return OpStatus::OK;
}

OP_STATUS ComposeDesktopWindow::SetMessageAccountFromInput(BOOL warn_on_invalid_sender)
{
	Account* account = 0;
	OpDropDown *from_dropdown = m_widgets->Get<QuickDropDown>("cw_FromDropdown")->GetOpWidget();
	g_m2_engine->GetAccountManager()->GetAccountById((UINT16)(INTPTR)from_dropdown->GetItemUserData(from_dropdown->GetSelectedItem()), account);

	if (account)
	{
		OpString sender;
		account->GetEmail(sender);
		if (sender.IsEmpty())
		{
			if (warn_on_invalid_sender)
			{
				InvalidSenderDialog* dialog = OP_NEW(InvalidSenderDialog, ());
				if (dialog)
					dialog->Init(this);
			}
			return OpStatus::ERR;
		}

		m_message.SetAccountId(account->GetAccountId(), TRUE);

		if (warn_on_invalid_sender && account->GetAccountId() != m_initial_account_id)
			g_m2_engine->GetAccountManager()->SetLastUsedAccount(account->GetAccountId());
	}

	return OpStatus::OK;
}

OP_STATUS ComposeDesktopWindow::SetMessageHeadersFromInput(BOOL warn_on_invalid_headers)
{
	BOOL has_needed_headers = FALSE;
	BOOL headers_has_errors = FALSE;
	int parseerror_start = 0;
	int parseerror_end = 0;
	OpString string;

	//To
	QuickComposeEdit* to_edit = m_widgets->Get<QuickComposeEdit>("cw_ToEdit");
	to_edit->GetText(string);
	if (m_message.SetTo(string, TRUE, &parseerror_start, &parseerror_end)==OpStatus::ERR_PARSING_FAILED) //QuotePair-decode string
	{
		headers_has_errors = TRUE;
		if (parseerror_end>0)
		{
			to_edit->GetOpWidget()->SetSelection(parseerror_start, parseerror_end);
			to_edit->GetOpWidget()->SetFocus(FOCUS_REASON_OTHER);
		}
	}
	else
	{
		has_needed_headers |= string.HasContent();
	}

	//Cc
	QuickComposeEdit* cc_edit = m_widgets->Get<QuickComposeEdit>("cw_CcEdit");
	cc_edit->GetText(string);
	if (m_message.SetCc(string, TRUE, &parseerror_start, &parseerror_end)==OpStatus::ERR_PARSING_FAILED) //QuotePair-decode string
	{
		headers_has_errors = TRUE;
		if (parseerror_end>0)
		{
			cc_edit->GetOpWidget()->SetSelection(parseerror_start, parseerror_end);
			cc_edit->GetOpWidget()->SetFocus(FOCUS_REASON_OTHER);
		}
	}
	else
	{
		has_needed_headers |= string.HasContent();
	}

	//Bcc
	QuickComposeEdit* bcc_edit = m_widgets->Get<QuickComposeEdit>("cw_BccEdit");
	bcc_edit->GetText(string);
	if (m_message.SetBcc(string, TRUE, &parseerror_start, &parseerror_end)==OpStatus::ERR_PARSING_FAILED) //QuotePair-decode string
	{
		headers_has_errors = TRUE;
		if (parseerror_end>0)
		{
			bcc_edit->GetOpWidget()->SetSelection(parseerror_start, parseerror_end);
			bcc_edit->GetOpWidget()->SetFocus(FOCUS_REASON_OTHER);
		}
	}
	else
	{
		has_needed_headers |= string.HasContent();
	}

	//Newsgroups
	QuickComposeEdit* newsgroups_edit = m_widgets->Get<QuickComposeEdit>("cw_NewsgroupsEdit");
	newsgroups_edit->GetText(string);
	if (m_message.SetHeaderValue(Header::NEWSGROUPS, string, FALSE, &parseerror_start, &parseerror_end)==OpStatus::ERR_PARSING_FAILED) //QuotePair-decode string
	{
		headers_has_errors = TRUE;
		if (parseerror_end>0)
		{
			newsgroups_edit->GetOpWidget()->SetSelection(parseerror_start, parseerror_end);
			newsgroups_edit->GetOpWidget()->SetFocus(FOCUS_REASON_OTHER);
		}
	}
	else
	{
		Account* account = m_message.GetAccountPtr();
		BOOL is_message_for_news_account = account && account->GetIncomingProtocol() == AccountTypes::NEWS;
		has_needed_headers |= (string.HasContent() && is_message_for_news_account);
	}

	//FollowupTo
	QuickComposeEdit* followupto_edit = m_widgets->Get<QuickComposeEdit>("cw_FollowuptoEdit");
	followupto_edit->GetText(string);
	if (m_message.SetHeaderValue(Header::FOLLOWUPTO, string, FALSE, &parseerror_start, &parseerror_end)==OpStatus::ERR_PARSING_FAILED) //QuotePair-decode string
	{
		headers_has_errors = TRUE;
		if (parseerror_end>0)
		{
			followupto_edit->GetOpWidget()->SetSelection(parseerror_start, parseerror_end);
			followupto_edit->GetOpWidget()->SetFocus(FOCUS_REASON_OTHER);
		}
	}

	//ReplyTo
	QuickComposeEdit* replyto_edit = m_widgets->Get<QuickComposeEdit>("cw_ReplytoEdit");
	replyto_edit->GetText(string);
	if (m_message.SetReplyTo(string, TRUE, &parseerror_start, &parseerror_end)==OpStatus::ERR_PARSING_FAILED) //QuotePair-decode string
	{
		headers_has_errors = TRUE;
		if (parseerror_end>0)
		{
			replyto_edit->GetOpWidget()->SetSelection(parseerror_start, parseerror_end);
			replyto_edit->GetOpWidget()->SetFocus(FOCUS_REASON_OTHER);
		}
	}

	//Subject
	m_widgets->Get<QuickEdit>("cw_SubjectEdit")->GetText(string);
	m_message.SetHeaderValue(Header::SUBJECT, string);
	Account* account = m_message.GetAccountPtr();
	BOOL empty_subject_warning = (string.IsEmpty() && account && account->GetWarnIfEmptySubject());

	if (warn_on_invalid_headers)
	{
		if (!has_needed_headers || headers_has_errors)
		{
			OpString dialog_title;
			OpString dialog_message;
	
			g_languageManager->GetString(Str::S_INVALID_MESSAGE, dialog_title);
			g_languageManager->GetString(Str::S_MISSING_ADDRESS_OR_SUBJECT, dialog_message);
	
			SimpleDialog* warning_dialog = OP_NEW(SimpleDialog, ());
			if (warning_dialog)
				warning_dialog->Init(WINDOW_NAME_INVALID_MESSAGE, dialog_title, dialog_message, this, Dialog::TYPE_OK, Dialog::IMAGE_WARNING, TRUE);
	
			return OpStatus::ERR;
		}
		else if (empty_subject_warning)
		{
			OpString dialog_title;
			OpString dialog_message;
	
			g_languageManager->GetString(Str::D_EMPTY_SUBJECT_WARNING_TITLE, dialog_title);
			g_languageManager->GetString(Str::D_EMPTY_SUBJECT_WARNING_MESSAGE, dialog_message);
	
			SimpleDialog* question_dialog = OP_NEW(SimpleDialog, ());
			if (!question_dialog)
				return OpStatus::ERR_NO_MEMORY;
	
			INT32 result = 0;
			BOOL do_not_show_again = FALSE;
			question_dialog->Init(WINDOW_NAME_EMPTY_SUBJECT_WARNING, dialog_title, dialog_message, this, Dialog::TYPE_YES_NO, Dialog::IMAGE_QUESTION, TRUE, &result, &do_not_show_again);
	
			if (do_not_show_again)
				account->SetWarnIfEmptySubject(!do_not_show_again);
	
			if (Dialog::DialogResultCode(result) == Dialog::DIALOG_RESULT_NO)
				return OpStatus::ERR;
		}
	}

	return OpStatus::OK;
}

OP_STATUS ComposeDesktopWindow::SaveAsDraft()
{
	OpStatus::Ignore(g_m2_engine->SaveDraft(&m_message));
	m_message_id = m_message.GetId();
	if (m_message_id)
		g_m2_engine->OnStartEditingMessage(m_message_id);
	
	return OpStatus::OK;
}

OP_STATUS ComposeDesktopWindow::CheckForOutgoingServer()
{
	Account* account = m_message.GetAccountPtr();
	BOOL has_outgoing_server = account && account->GetOutgoingProtocol() != AccountTypes::UNDEFINED;

	if (!has_outgoing_server)
	{
		NoOutgoingServerDialog* dialog = OP_NEW(NoOutgoingServerDialog, ());
		if (dialog)
			dialog->Init(this);

		return OpStatus::ERR;
	}
	
	return OpStatus::OK;
}

OP_STATUS ComposeDesktopWindow::SendMessage()
{
	// continue with all actions that only apply to send mail:
	OP_ASSERT_SUCCESS(g_m2_engine->SendMessage(m_message, FALSE));

	if (m_message.GetParentId())
	{
		Message::Flags flag = Message::IS_READ;

		switch (m_message.GetCreateType())
		{
			case MessageTypes::NEW:
				break;
			case MessageTypes::REPLY:
			case MessageTypes::QUICK_REPLY:
			case MessageTypes::REPLY_TO_LIST:
			case MessageTypes::REPLY_TO_SENDER:
			case MessageTypes::REPLYALL:
			case MessageTypes::FOLLOWUP:
			case MessageTypes::REPLY_AND_FOLLOWUP:
				flag = Message::IS_REPLIED;
				break;
			case MessageTypes::FORWARD:
				flag = Message::IS_FORWARDED;
				break;
		}

		Message message;
		if (OpStatus::IsSuccess(g_m2_engine->GetMessage(message, m_message.GetParentId(), TRUE)))
		{
			Header::HeaderValue parent_subject, subject;
			RETURN_IF_ERROR(message.GetHeaderValue(Header::SUBJECT, parent_subject));
			RETURN_IF_ERROR(m_message.GetHeaderValue(Header::SUBJECT, subject));
				
			if (Store3::StripSubject(parent_subject).Compare(Store3::StripSubject(subject)) != 0)
			{
				m_message.SetParentId(0);
				return g_m2_engine->UpdateMessage(m_message);
			}
				
			if (flag == Message::IS_REPLIED)
			{
				g_m2_engine->MessageReplied(m_message.GetParentId(), TRUE);
			}
			else if (flag)
			{
				message.SetFlag(flag, TRUE);
				g_m2_engine->UpdateMessage(message);
			}
		}
	}

	SetWillSave(FALSE);
	
	return OpStatus::OK;
}

/***********************************************************************************
**
**	OnInsertImage
**
***********************************************************************************/

void ComposeDesktopWindow::OnInsertImage()
{

	InsertImageWindowChooserListener * listener = OP_NEW(InsertImageWindowChooserListener, (this));
	if (listener)
	{
		OpString filter;
		g_languageManager->GetString(Str::S_FILE_CHOOSER_IMAGE_FILTER, filter);
		StringFilterToExtensionFilter(filter.CStr(), &m_request.extension_filters);
		
		g_languageManager->GetString(Str::D_HTML_MAIL_ADD_PICTURE_TITLE, m_request.caption);

		m_request.action = DesktopFileChooserRequest::ACTION_FILE_OPEN;

		DesktopFileChooser *chooser;
		RETURN_VOID_IF_ERROR(GetChooser(chooser));

		if (OpStatus::IsError(chooser->Execute(GetOpWindow(), listener, m_request)))
			OP_DELETE(listener);
	}
}

/***********************************************************************************
**
**	OnTextChanged
**
***********************************************************************************/

void ComposeDesktopWindow::OnTextChanged()
{
	//Restart timer if a control is changed is performed (user is doing something)
	m_save_draft_timer->Stop();
	m_save_draft_timer->Start(SAVE_DRAFT_TIMER) ;

	SetWillSave(TRUE);
}

/***********************************************************************************
**
**	OnTextDirectionChanged
**
***********************************************************************************/

void ComposeDesktopWindow::OnTextDirectionChanged(BOOL to_rtl)
{
	m_message.SetTextDirection(to_rtl ? AccountTypes::DIR_RIGHT_TO_LEFT : AccountTypes::DIR_LEFT_TO_RIGHT);
}


/***********************************************************************************
**
**	OnTimeOut
**
***********************************************************************************/

void ComposeDesktopWindow::OnTimeOut(OpTimer *timer)
{
	if (timer==m_save_draft_timer && m_will_save)
	{
		OpInputAction action(OpInputAction::ACTION_SAVE_DRAFT);
		OnInputAction(&action);

		g_session_auto_save_manager->SaveCurrentSession();
		SetWillSave(FALSE);
	}
}

/***********************************************************************************
**
**	OnDropFiles
**
***********************************************************************************/

void ComposeDesktopWindow::OnDropFiles(const OpVector<OpString>& file_names)
{
	for (UINT32 i = 0; i < file_names.GetCount(); i++)
	{
		OpString filename = *file_names.Get(i);
		if (OpStatus::IsSuccess(ConvertURLtoFullPath(filename)))
			AddAttachment(filename.CStr());
	}
}

void ComposeDesktopWindow::OnDragMove(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	DesktopDragObject* drag_object = static_cast<DesktopDragObject *>(op_drag_object);
	if (drag_object->GetURL() )
	{
		if(*drag_object->GetURL() )
		{
			drag_object->SetDesktopDropType(DROP_COPY);
			return;
		}
	}
	drag_object->SetDesktopDropType(DROP_NOT_AVAILABLE);
}

void ComposeDesktopWindow::OnDragDrop(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	DesktopDragObject* drag_object = static_cast<DesktopDragObject *>(op_drag_object);
	if (drag_object->GetURL() )
	{
		if( drag_object->GetURLs().GetCount() > 0 )
		{
			OnDropFiles(drag_object->GetURLs());
		}
		else
		{
			OpString* s = OP_NEW(OpString, ());
			if (s)
			{
				OpAutoVector<OpString> file_names;
				s->Set(drag_object->GetURL());
				file_names.Add(s);
				OnDropFiles(file_names);
			}
		}
	}
}

/***********************************************************************************
**
**	OnMouseEvent
**
***********************************************************************************/

void ComposeDesktopWindow::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (widget != m_attachment_list)
	{
		return;
	}

	if (!down && button == MOUSE_BUTTON_1 && pos == -1)
	{
		AskForAttachment();
	}
	else if (nclicks == 2 && button == MOUSE_BUTTON_1 && pos != -1)
	{
		OpInputAction action(OpInputAction::ACTION_OPEN);
		OnInputAction(&action);
	}
	else if (!down && button == MOUSE_BUTTON_2)
	{
		OpINT32Vector items;
		int item_count = m_attachment_list->GetSelectedItems(items);

		OpAutoPtr<DesktopMenuContext> menu_context  (OP_NEW(DesktopMenuContext, ()));
		TransferManagerDownloadCallback * download_callback = NULL;
		if (menu_context.get())
		{
			OpString suggested_filename;
			URL url;
			if (item_count > 0)
				RETURN_VOID_IF_ERROR(m_attachments.GetAttachmentInfoById(items.Get(0), 0, suggested_filename, url));
			download_callback = OP_NEW(TransferManagerDownloadCallback, (NULL, url, NULL, ViewActionFlag()));
			if (download_callback)
			{
				menu_context->Init(download_callback);

				OpRect rect;
				OpPoint point(x,y);

				InvokeContextMenuAction(point, menu_context.release(), UNI_L("Mail Attachment Popup"), rect);
				return;
			}
		}
		g_application->GetMenuHandler()->ShowPopupMenu("Mail Attachment Popup", PopupPlacement::AnchorAtCursor());
	}
}

/***********************************************************************************
**
**	OnChange
**
***********************************************************************************/

void ComposeDesktopWindow::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	//Restart timer if a control is changed is performed (user is doing something)
	m_save_draft_timer->Stop();
	m_save_draft_timer->Start(SAVE_DRAFT_TIMER) ;

	OpDropDown *from_dropdown = m_widgets->Get<QuickDropDown>("cw_FromDropdown")->GetOpWidget();

    if (widget==from_dropdown)
    {
        UINT16 account_id = (UINT16)((INTPTR)from_dropdown->GetItemUserData(from_dropdown->GetSelectedItem()));

        Account* account;
        g_m2_engine->GetAccountManager()->GetAccountById(account_id, account);
        if (!account)
            return;

		UINT16 old_account_id = m_message.GetAccountId();

        Account* old_account;
        g_m2_engine->GetAccountManager()->GetAccountById(old_account_id, old_account); //NULL is a legal old_account value!

		m_message.SetAccountId(account_id, TRUE);

		// update the new account default charset

		OpString8 charset8;
		account->GetCharset(charset8);
		OpString def_enc, default_encoding, charset;
		charset.Set(charset8);
		g_languageManager->GetString(Str::S_AUTO_DETECTED_ENCODING,def_enc);
		default_encoding.AppendFormat(def_enc.CStr(), charset.CStr());
		// update the UI as well
		m_widgets->Get<QuickDropDown>("cw_EncodingDropdown")->GetOpWidget()->ChangeItem(default_encoding.CStr(),0);

		BOOL is_redirected = m_create_type == MessageTypes::REDIRECT || m_create_type==MessageTypes::REOPEN;

		// only change formatting preferences if it's a new message or if it's a reopened draft
		if ((m_create_type == MessageTypes::NEW ||
			m_create_type == MessageTypes::REOPEN) &&
			m_rich_text_editor->IsHTML() != (account->PreferHTMLComposing() == Message::PREFER_TEXT_HTML))
		{	
			// normally SetSignature will update the view, except if it's a redirected message, then we don't set the signature
			m_rich_text_editor->SwitchHTML(is_redirected);
		}

		if (!is_redirected)
		{
			//Update signature
			OpString new_signature;
			BOOL is_HTML_sig;
			account->GetSignature(new_signature,is_HTML_sig);
			m_rich_text_editor->SetSignature(new_signature,is_HTML_sig);			
		}

		OpString text;
		OpString old;

		//Update Reply-To
		account->GetReplyTo(text);
		m_widgets->Get<QuickComposeEdit>("cw_ReplytoEdit")->SetText(text.CStr());

		//Update CC only if CC edit matches previous account's auto cc
		if (old_account)
		{
			old_account->GetAutoCC(old);
			m_widgets->Get<QuickComposeEdit>("cw_CcEdit")->GetText(text);
		}

		if (!old_account || old.CompareI(text) == 0)
		{
			account->GetAutoCC(text);
			m_widgets->Get<QuickComposeEdit>("cw_CcEdit")->SetText(text.CStr());
		}

		//Update BCC only if BCC edit matches previous account's auto bcc
		if (old_account)
		{
			old_account->GetAutoBCC(old);
			m_widgets->Get<QuickComposeEdit>("cw_BccEdit")->GetText(text);
		}

		if (!old_account || old.CompareI(text) == 0)
		{
			account->GetAutoBCC(text);
			m_widgets->Get<QuickComposeEdit>("cw_BccEdit")->SetText(text.CStr());
		}

		m_message.SetFlag(StoreMessage::IS_NEWS_MESSAGE, account->GetIncomingProtocol() == AccountTypes::NEWS);
		UpdateVisibleHeaders();

		// Update direction in message, window and direction drop down
		SetDirection(account->GetDefaultDirection());
		//Show/Hide headers
		Relayout();
		OnTextChanged();
    }

	if (m_message_id)
	{
		SetWillSave(TRUE);
	}
}

/***********************************************************************************
**
**	AskForAttachment
**
***********************************************************************************/

void ComposeDesktopWindow::AskForAttachment()
{
	ComposeDesktopWindowChooserListener * listener = OP_NEW(ComposeDesktopWindowChooserListener, (this));
	if (listener)
	{
		m_request.action = DesktopFileChooserRequest::ACTION_FILE_OPEN_MULTI;
		g_languageManager->GetString(Str::S_ATTACH_FILES, m_request.caption);
		m_request.extension_filters.DeleteAll();

		DesktopFileChooser *chooser;
		RETURN_VOID_IF_ERROR(GetChooser(chooser));

		if (OpStatus::IsError(chooser->Execute(GetOpWindow(), listener, m_request)))
			OP_DELETE(listener);
	}
}

/***********************************************************************************
**
**	AddAttachment
**
***********************************************************************************/

OP_STATUS ComposeDesktopWindow::AddAttachment(const uni_char* filepath, const uni_char* suggested_filename, URL& file_url, BOOL is_inlined)
{
	SetWillSave(TRUE);
	if (!is_inlined)
	{
		StartSaveTimer(*m_attachment_list);
		m_attachment_list->SetShowColumnHeaders(TRUE);
		m_attachment_list->GetBorderSkin()->SetImage("Treeview Skin");
		m_message.SetFlag(StoreMessage::HAS_ATTACHMENT, TRUE);
	}
	return m_attachments.AddFromCache(filepath, suggested_filename, file_url, is_inlined);
}

OP_STATUS ComposeDesktopWindow::AddAttachment(const uni_char* filepath, const uni_char* filename)
{
	StartSaveTimer(*m_attachment_list);
	m_attachment_list->SetShowColumnHeaders(TRUE);
	m_attachment_list->GetBorderSkin()->SetImage("Treeview Skin");
	m_message.SetFlag(StoreMessage::HAS_ATTACHMENT, TRUE);

	UniString temporary;
	RETURN_IF_ERROR(temporary.SetConstData(filepath));
	OpAutoPtr< OtlCountedList<UniString> > paths(temporary.Split('|'));
	RETURN_OOM_IF_NULL(paths.get());

	RETURN_IF_ERROR(temporary.SetConstData(filename ? filename : UNI_L("")));
	OpAutoPtr< OtlCountedList<UniString> > names(temporary.Split('|'));
	RETURN_OOM_IF_NULL(names.get());

	for (OtlCountedList<UniString>::Iterator path_it = paths->Begin(), name_it = names->Begin(); path_it != paths->End(); path_it++)
	{
			OpString file;
			OpString name;
			if (name_it != names->End())
				OpStatus::Ignore(name.SetFromUniString(*name_it++));
		
			if (!(*path_it).IsEmpty() && OpStatus::IsSuccess(file.SetFromUniString(*path_it)))
				RETURN_IF_ERROR(m_attachments.AddFromFile(file.CStr(), name.CStr()));
	}
	return OpStatus::OK;
}

/***********************************************************************************
**
**	AppendContact
**
***********************************************************************************/

void ComposeDesktopWindow::AppendContact(OpEdit* edit, INT32 contact_id,
	const OpStringC& contact_string)
{
	OpString string;

	edit->GetText(string);

	g_hotlist_manager->GetContactAddresses(contact_id, string, contact_string);

	edit->SetText(string.CStr());
	edit->SetCaretPos(string.Length());
}

/***********************************************************************************
**
**	GenerateContentId
**
***********************************************************************************/
OpString ComposeDesktopWindow::GenerateContentId(const uni_char* string_seed)
{
	OpString res;

	// Format: "<filename.extension@[time-in-ms].[generated-string]>"
	res.AppendFormat(UNI_L("%s@"),string_seed);

	for (int j = res.Length()-1; j >= 0 ; j--)
	{
		if (res[j] == ' ')
			res[j] = '_';
	}


	time_t current_time = g_timecache->CurrentTime();
	res.AppendFormat(UNI_L("%d"), current_time);

	// Append a string generated from string_seed, but only containing hex-digits
	union {
		unsigned char c[8];
		double d;
		UINT32 i[2];
	} gen_str;
	gen_str.d = current_time;
	UINT i = 0;
	while (string_seed && *string_seed)
	{
		gen_str.c[i++ %8] ^= *string_seed;
		string_seed++;
	}
	res.AppendFormat(UNI_L(".%08x%08x"), gen_str.i[0], gen_str.i[1]);

	return res;
}

/***********************************************************************************
**
**	ImageSelected
**
***********************************************************************************/
void ComposeDesktopWindow::ImageSelected(OpString& image_path)
{

	URL image_url = urlManager->GetURL(image_path.CStr());
	image_url.QuickLoad(TRUE);
	URL_DataStream image_stream(image_url,TRUE);
	
	OpString content_id,file_name;
	image_url.GetAttributeL(URL::KSuggestedFileName_L,file_name);
	content_id.Set(GenerateContentId(file_name.CStr()).CStr());
	content_id.Insert(0,"cid:");
	URL image_cid_url;
	image_cid_url = urlManager->GetURL(content_id,m_message.GetContextId());
	image_cid_url.Unload();
	image_cid_url.SetAttribute(URL::KType,URL_CONTENT_ID);
	image_cid_url.SetAttribute(URL::KIsGeneratedByOpera, TRUE);

	URL_DataStream cached_image_stream(image_cid_url,TRUE);
	unsigned char *buffer = OP_NEWA(unsigned char,1000);
	unsigned long length = 1;
	do
	{
		TRAPD(error,length = image_stream.ReadDataL(buffer,1000));
		if (OpStatus::IsError(error))
			return;
		TRAP(error,cached_image_stream.WriteDataL(buffer,length));
		if (OpStatus::IsError(error))
			return;
	} while (buffer && length > 0);
	OP_DELETEA(buffer);
	cached_image_stream.PerformActionL(DataStream::KFinishedAdding);
	image_cid_url.SetAttribute(URL::KCacheType, URL_CACHE_TEMP);
	
	// set the correct mime type on the cache URL
	image_cid_url.SetAttribute(URL::KContentType,image_url.GetAttribute(URL::KContentType));
	image_cid_url.QuickLoad(FALSE);
	AddAttachment(image_path.CStr(), file_name.CStr(), image_cid_url, TRUE);
	m_rich_text_editor->InsertImage(content_id);
}

/***********************************************************************************
**
**	SetDirection
**
***********************************************************************************/
void ComposeDesktopWindow::SetDirection(int direction, BOOL update_body, BOOL force_autodetect)
{
	m_rich_text_editor->SetDirection(direction, update_body, force_autodetect);
	m_message.SetTextDirection(direction);
	StartSaveTimer(*m_widgets->Get<QuickButton>("cw_DirectionButton")->GetOpWidget());
}

void ComposeDesktopWindow::UpdateVisibleHeaders()
{
	int flags = g_pcm2->GetIntegerPref(PrefsCollectionM2::MailComposeHeaderDisplay);
	
	for (int i = 0; i < AccountTypes::HEADER_DISPLAY_LAST; i++)
	{
		const char* widget_1_str = NULL;
		const char* widget_2_str = NULL;
		bool visible = (flags & (1 << i)) != 0;
		switch (i)
		{
			case AccountTypes::ACCOUNT:
				widget_1_str = "cw_From";
				widget_2_str = "cw_FromDropdown";
				if (flags & (1 << AccountTypes::ENCODING))
					visible = true;
				else
					visible = visible && g_m2_engine->GetAccountManager()->GetMailNewsAccountCount() > 1;
				break;
			case AccountTypes::TO:
				widget_1_str = "cw_ToEdit";
				widget_2_str = "cw_To";
				break;
			case AccountTypes::CC:
				widget_1_str = "cw_CcEdit";
				widget_2_str = "cw_Cc";
				break;
			case AccountTypes::BCC:
				widget_1_str = "cw_BccEdit";
				widget_2_str = "cw_Bcc";
				break;
			case AccountTypes::SUBJECT:
				widget_1_str = "cw_SubjectEdit";
				widget_2_str = "cw_Subject";
				break;
			case AccountTypes::REPLYTO:
				widget_1_str = "cw_ReplyToEdit";
				widget_2_str = "cw_Replyto";
				break;
			case AccountTypes::ATTACHMENTS:
				widget_1_str = "cw_AttachmentArea";
				widget_2_str = "cw_AttachmentList";
				break;
			case AccountTypes::ENCODING:
				widget_1_str = "cw_EncodingDropdown";
				break;
			case AccountTypes::DIRECTION:
				widget_1_str = "cw_DirectionButton";
				break;
			case AccountTypes::PRIORITY:
				widget_1_str = "cw_PriorityButton";
				break;
			case AccountTypes::FOLLOWUPTO:
				widget_1_str = "cw_FollowuptoEdit";
				widget_2_str = "cw_Followupto";
				visible = visible && m_message.IsFlagSet(Message::IS_NEWS_MESSAGE);
				break;
			case AccountTypes::NEWSGROUPS:
				widget_1_str = "cw_NewsgroupsEdit";
				widget_2_str = "cw_Newsgroups";
				visible = visible && m_message.IsFlagSet(Message::IS_NEWS_MESSAGE);
				break;
			default:
				break;
		}

		if (visible)
		{
			m_widgets->Get<QuickWidget>(widget_1_str)->Show();
			if (widget_2_str)
				m_widgets->Get<QuickWidget>(widget_2_str)->Show();
		}
		else if (!m_widgets->Contains<QuickComposeEdit>(widget_1_str) || m_widgets->Get<QuickComposeEdit>(widget_1_str)->GetOpWidget()->IsEmpty())
		{
			// never hide edit fields with content
			m_widgets->Get<QuickWidget>(widget_1_str)->Hide();
			if (widget_2_str)
				m_widgets->Get<QuickWidget>(widget_2_str)->Hide();
		}

	}
}

OP_STATUS ComposeDesktopWindow::SaveAttachments()
{
	RETURN_IF_ERROR(g_m2_engine->SaveDraft(&m_message));
	m_message_id = m_message.GetId();
	RETURN_IF_ERROR(m_attachments.Save(m_message_id));
	return OpStatus::OK;
}

OP_STATUS ComposeDesktopWindow::ValidateMailAdresses(enum Header::HeaderType header, OpString& invalid_mails)
{
	const Header::From *mail_address = m_message.GetHeader(header) ? m_message.GetHeader(header)->GetFirstAddress() : NULL;
	while (mail_address)
	{
		if (!FormManager::IsValidEmailAddress(mail_address->GetAddress()))
		{
			RETURN_IF_ERROR(invalid_mails.AppendFormat("    - %s\n", mail_address->GetAddress().CStr()));
		}
		mail_address = static_cast<Header::From *>(mail_address->Suc());
	}
	return OpStatus::OK;
}
#endif // M2_SUPPORT
