/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/
#include "core/pch.h"
#include "SignatureEditor.h"
#include "adjunct/desktop_util/string/string_convert.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2_ui/widgets/RichTextEditor.h"
#include "adjunct/quick/dialogs/URLInputDialog.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/locale/oplanguagemanager.h"


/***********************************************************************************
**
**	Init
**
***********************************************************************************/

void SignatureEditor::Init(DesktopWindow *parent_window, UINT32 account_id)
{
	m_account_id = account_id;
	Dialog::Init(parent_window);
}


/***********************************************************************************
**
**	OnInit
**
***********************************************************************************/

void SignatureEditor::OnInit()
{
	m_rich_text_editor = static_cast<RichTextEditor*>(GetWidgetByName("rich_text_editor"));
	if (!m_rich_text_editor)
	{
		// no point in continuing
		return Close();
	}

	Account* account = g_m2_engine->GetAccountById(m_account_id);
	if (!account)
	{
		SetWidgetValue("apply_for_all_accounts", TRUE);
		SetWidgetEnabled("apply_for_all_accounts", FALSE);
		account = g_m2_engine->GetAccountById(g_m2_engine->GetAccountManager()->GetDefaultAccountId(AccountTypes::TYPE_CATEGORY_MAIL));
	}

	if (account)
	{
		// get the stored signature
		BOOL isHTML;
		OpString signature_content;
		account->GetSignature(signature_content,isHTML);
		// initialize the rich text editor
		m_rich_text_editor->Init(this, isHTML, this, 0);
		m_rich_text_editor->SetDirection(account->GetDefaultDirection(),FALSE);
		m_rich_text_editor->SetMailContentAndUpdateView(signature_content, TRUE);
		m_rich_text_editor->SetEmbeddedInDialog();

		// change the explanation to contain the account name
		OpMultilineEdit *explanation = static_cast<OpMultilineEdit*>(GetWidgetByName("explanation_label"));
		if (explanation)
		{
			if (m_account_id != 0)
			{
				OpString explanation_text, account_name, final_text;
				g_languageManager->GetString(Str::D_MAIL_SIGNATURE_EXPLANATION, explanation_text);
				final_text.AppendFormat(explanation_text.CStr(), account->GetAccountName().CStr());
				explanation->SetText(final_text.CStr());
				explanation->SetWrapping(TRUE);
			}
			else
			{
				explanation->SetVisibility(FALSE);
			}
		}
	}
	else
	{
		Close();
	}
}

/***********************************************************************************
**
**	GetOkTextID
**
***********************************************************************************/

Str::LocaleString SignatureEditor::GetOkTextID()
{
	return Str::SI_SAVE_BUTTON_TEXT;
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL SignatureEditor::OnInputAction(OpInputAction* action)
{ 
	if (m_rich_text_editor && m_rich_text_editor->OnInputAction(action)) 
		return TRUE; 
	return Dialog::OnInputAction(action); 
}

/***********************************************************************************
**
**	OnInsertImage
**
***********************************************************************************/

void SignatureEditor::OnInsertImage()
{
	OpString caption, title;
	g_languageManager->GetString(Str::D_MAIL_SIGNATURE_URL_FOR_IMAGE_CAPTION,caption);
	g_languageManager->GetString(Str::D_MAIL_SIGNATURE_URL_FOR_IMAGE_TITLE,title);
	URLInputDialog *url_dialog = OP_NEW(URLInputDialog,());
	url_dialog->Init(this);
	url_dialog->SetURLInputListener(this);
	url_dialog->SetTitleAndMessage(title,caption);
}

/***********************************************************************************
**
**	OnURLInputOkCallback
**
***********************************************************************************/

void SignatureEditor::OnURLInputOkCallback(OpString& url_result)
{
	m_rich_text_editor->InsertImage(url_result);
}


/***********************************************************************************
**
**	OnOk
**
***********************************************************************************/

UINT32 SignatureEditor::OnOk()
{
	// store the HTML source
	BOOL isHTML = m_rich_text_editor->IsHTML();
	OpString signature_content;
	if (isHTML)
		m_rich_text_editor->GetHTMLSource(signature_content,TRUE);
	else
	{
		// the text equivalent returns LF instead of CRLF, we need to convert it
		OpString temp_sig;		
		m_rich_text_editor->GetTextEquivalent(temp_sig);
		UINT32 needed_length = StringConvert::ConvertLineFeedsToLineBreaks(temp_sig.CStr(),temp_sig.Length(),NULL,0);
		uni_char* buf = OP_NEWA(uni_char,needed_length);
		StringConvert::ConvertLineFeedsToLineBreaks(temp_sig.CStr(),temp_sig.Length(),buf,needed_length);
		signature_content.Set(buf,needed_length);
		OP_DELETEA(buf);
	}
	

	// check if we should apply the signature for all accounts or not
	if (GetWidgetValue("apply_for_all_accounts"))
	{
		Account* account = g_m2_engine->GetAccountManager()->GetFirstAccount();

		while (account)
		{
			account->SetSignature(signature_content,isHTML);
			account = (Account*)(account->Suc());
		}

	}
	else
	{
		Account* account = g_m2_engine->GetAccountById(m_account_id);
		if (account)
			account->SetSignature(signature_content, isHTML);
	}
	return 0;
}
