/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Alexander Remen (alexr@opera.com)
 *
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/desktop_util/prefs/PrefsCollectionM2.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2_ui/dialogs/SignatureEditor.h"
#include "adjunct/m2_ui/controllers/ComposeWindowOptionsController.h"
#include "adjunct/m2_ui/windows/ComposeDesktopWindow.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder.h"
#include "adjunct/quick_toolkit/widgets/QuickCheckBox.h"
#include "adjunct/quick_toolkit/widgets/CalloutDialogPlacer.h"
#include "adjunct/quick_toolkit/widgets/QuickOverlayDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickDropDown.h"

#include "modules/widgets/WidgetContainer.h"

void ComposeWindowOptionsController::InitL()
{
	QuickOverlayDialog* overlay_dialog = OP_NEW_L(QuickOverlayDialog, ());
	LEAVE_IF_ERROR(SetDialog("Mail Compose Window Options Popup", overlay_dialog));

	CalloutDialogPlacer* placer = OP_NEW_L(CalloutDialogPlacer, (*GetAnchorWidget()));
	overlay_dialog->SetDialogPlacer(placer);
	overlay_dialog->SetBoundingWidget(*GetAnchorWidget()->GetParentDesktopWindow()->GetWidgetContainer()->GetRoot());
	overlay_dialog->SetAnimationType(QuickOverlayDialog::DIALOG_ANIMATION_CALLOUT);

	LEAVE_IF_ERROR(GetBinder()->Connect("Message_expanded_checkbox", m_expanded_draft));
	m_expanded_draft.Set(g_pcm2->GetIntegerPref(PrefsCollectionM2::PaddingInComposeWindow) == 0);
	LEAVE_IF_ERROR(m_expanded_draft.Subscribe(MAKE_DELEGATE(*this, &ComposeWindowOptionsController::UpdateExpandedDraft)));

	int flags = g_pcm2->GetIntegerPref(PrefsCollectionM2::MailComposeHeaderDisplay);

	for (int i = 0; i < AccountTypes::HEADER_DISPLAY_LAST; i++)
	{
		m_show_flags[i].Set((flags & (1<<i)) != FALSE);
		OpString8 checkbox_name;
		LEAVE_IF_ERROR(checkbox_name.AppendFormat("cb_show_%d", i));
		if (!overlay_dialog->GetWidgetCollection()->Contains<QuickWidget>(checkbox_name.CStr()))
			continue;
		LEAVE_IF_ERROR(GetBinder()->Connect(checkbox_name.CStr(), m_show_flags[i]));
		LEAVE_IF_ERROR(m_show_flags[i].Subscribe(MAKE_DELEGATE(*this, &ComposeWindowOptionsController::UpdateShowFlags)));
	}

	if (g_m2_engine->GetAccountManager()->GetMailNewsAccountCount() <= 1)
		overlay_dialog->GetWidgetCollection()->Get<QuickCheckBox>("cb_show_5")->Hide();

	if (g_m2_engine->GetAccountManager()->GetFirstAccountWithType(AccountTypes::NEWS) == NULL)
	{
		if (overlay_dialog->GetWidgetCollection()->Contains<QuickCheckBox>("cb_show_10"))
			overlay_dialog->GetWidgetCollection()->Get<QuickCheckBox>("cb_show_10")->Hide();
		overlay_dialog->GetWidgetCollection()->Get<QuickCheckBox>("cb_show_11")->Hide();
	}

	QuickDropDown* from_dropdown = overlay_dialog->GetWidgetCollection()->Get<QuickDropDown>("Default_Account_Dropdown");

	for (int i = 0; i < g_m2_engine->GetAccountManager()->GetAccountCount(); i++)
	{
		Account* account = g_m2_engine->GetAccountManager()->GetAccountByIndex(i);

		if (account && account->GetOutgoingProtocol() == AccountTypes::SMTP)
		{
			OpString16 mail;

			account->GetEmail(mail);

            if (mail.IsEmpty())
				continue;

			if (account->GetAccountName().HasContent() && account->GetAccountName() != mail)
			{
				mail.Set(account->GetAccountName());
			}

			LEAVE_IF_ERROR(from_dropdown->AddItem(mail.CStr(), -1, NULL, account->GetAccountId()));
		}
	}

	LEAVE_IF_ERROR(GetBinder()->Connect(*from_dropdown, g_m2_engine->GetAccountManager()->m_default_mail_account));
}

OP_STATUS ComposeWindowOptionsController::HandleAction(OpInputAction* action)
{
	if (action->GetAction() == OpInputAction::ACTION_OPEN_SIGNATURE_DIALOG)
	{
		SignatureEditor * signature_editor = OP_NEW(SignatureEditor,());
		RETURN_OOM_IF_NULL(signature_editor);
		signature_editor->Init(GetAnchorWidget()->GetParentDesktopWindow(), static_cast<ComposeDesktopWindow*>(GetAnchorWidget()->GetParentDesktopWindow())->GetAccountId());
	}
	return OpStatus::OK;
}

void ComposeWindowOptionsController::UpdateShowFlags(bool)
{
	int flags = g_pcm2->GetIntegerPref(PrefsCollectionM2::MailComposeHeaderDisplay);

	for (int i = 0; i < AccountTypes::HEADER_DISPLAY_LAST; i++)
	{
		flags = m_show_flags[i].Get() ? flags | (1<<i) : flags & ~(1 << i);
	}
	
	TRAPD(err, g_pcm2->WriteIntegerL(PrefsCollectionM2::MailComposeHeaderDisplay, flags));
}

void ComposeWindowOptionsController::UpdateExpandedDraft(bool expanded)
{
	if (expanded)
	{
		TRAPD(err, g_pcm2->WriteIntegerL(PrefsCollectionM2::PaddingInComposeWindow, 0));
	}
	else
	{
		TRAPD(err, g_pcm2->ResetIntegerL(PrefsCollectionM2::PaddingInComposeWindow));
	}
}

#endif // M2_SUPPORT
