/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick_toolkit/widgets/OpGroup.h"
#include "adjunct/quick/dialogs/PasswordDialog.h"
#include "adjunct/quick/widgets/OpAddressDropDown.h"
#include "modules/skin/OpSkinManager.h"
#include "adjunct/quick/widgets/OpTrustAndSecurityButton.h"
#include "adjunct/quick/dialogs/SecurityInformationDialog.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "modules/display/vis_dev.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/url/url2.h"
#include "modules/wand/wandmanager.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpWidgetFactory.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/wand/wand_internal.h"

/***********************************************************************************
**
**	PasswordDialog
**
***********************************************************************************/

PasswordDialog::PasswordDialog(OpAuthenticationCallback* callback, OpWindowCommander *commander, BOOL titlebar)
  : m_callback(callback)
  , m_authid(callback->UrlId())
  , m_url_info(NULL)
  , m_commander(commander)
  , m_clean_windowcommander(!commander)
  , m_attached_to_address_field(FALSE)
  , m_addressbar_group_visible(TRUE)
  , m_address_proxy_widget(NULL)
  , m_title_bar(titlebar)
{
	if (m_clean_windowcommander)
		g_windowCommanderManager->GetWindowCommander(&m_commander);
}

PasswordDialog::~PasswordDialog()
{
	OpAddressDropDown* addressDropdown = GetAddressDropdown();

	if (addressDropdown)
		addressDropdown->SetEnabled(TRUE);
	
	OP_DELETE(m_url_info);
}

void PasswordDialog::OnInitVisibility()
{
	ShowWidget("Server_message_label", FALSE);
	ShowWidget("Server_message_info", FALSE);
}

Str::LocaleString PasswordDialog::GetDoNotShowAgainTextID()
{
	return Str::D_AUTHENTICATION_REMEMB_PASSW;
}

OpAddressDropDown* PasswordDialog::GetAddressDropdown()
{
	DesktopWindow* desktop_window = GetParentDesktopWindow();
	if (desktop_window && desktop_window->GetType() == OpTypedObject::WINDOW_TYPE_DOCUMENT)
	{
		return (OpAddressDropDown*)desktop_window->GetWidgetContainer()->GetRoot()->GetWidgetByTypeAndId(WIDGET_TYPE_ADDRESS_DROPDOWN, -1, FALSE);
	}
	return NULL;
}

void PasswordDialog::GetOverlayPlacement(OpRect& initial_placement, OpWidget* overlay_layout_widget)
{
	if (!UseOverlayedSkin())
		return;

	OpAddressDropDown* addressDropdown = GetAddressDropdown();
	OpRect address_rect = addressDropdown ? addressDropdown->GetRect() : OpRect(0, 0, 0, 0);
	
	if (addressDropdown && address_rect.y < 0) 
	{
		//fix for DSK-319703
		DesktopWindow* desktop_window = GetParentDesktopWindow();
		if (desktop_window)
			desktop_window->SyncLayout();
		address_rect = addressDropdown->GetRect();
	}

	OpRect parent_rect = overlay_layout_widget->GetParent()->GetRect();
	OpRect initial_rect = initial_placement;

	// Check address bar alignment
	OpBar::Alignment addressbar_alignment = OpBar::ALIGNMENT_OFF;
	if (addressDropdown && addressDropdown->GetParent() && addressDropdown->GetParent()->GetType() == WIDGET_TYPE_TOOLBAR)
	{
		OpBar* addressbar = (OpBar*)addressDropdown->GetParent();
		addressbar_alignment = addressbar->GetAlignment();
	}
	
	// Get left/right padding of dialog
	INT32 left_padding = 0, right_padding = 0, dummy = 0;
	OpSkinElement* skinElement = g_skin_manager->GetSkinElement(GetSkin());
	if (skinElement)
		skinElement->GetPadding(&left_padding, &dummy, &right_padding, &dummy, SKINSTATE_FOCUSED);

	m_attached_to_address_field = EnableAttachingToAddressField && addressbar_alignment == OpBar::ALIGNMENT_TOP && (address_rect.width + left_padding + right_padding) >= initial_placement.width;
	
	if (m_attached_to_address_field != m_addressbar_group_visible)
	{
		m_addressbar_group_visible = m_attached_to_address_field;
		if (m_address_proxy_widget)
			m_address_proxy_widget->SetVisibility(m_addressbar_group_visible);
		ShowWidget("Addressbar_group", m_addressbar_group_visible);
		CompressGroups();
	}

	// Place overlay according to address dropdown - horizontally
	if (m_attached_to_address_field)
	{
		initial_placement = OpRect(address_rect.x - left_padding, 0, address_rect.width + left_padding + right_padding, initial_placement.height);
		overlay_layout_widget->SetXResizeEffect(RESIZE_FIXED);
	}
	else
	{
		int y_placement = parent_rect.y;
		if (addressbar_alignment == OpBar::ALIGNMENT_TOP)
			y_placement = address_rect.y + address_rect.height;
		initial_placement = OpRect((parent_rect.width - initial_placement.width) / 2, y_placement, initial_placement.width, initial_placement.height);
		overlay_layout_widget->SetXResizeEffect(RESIZE_CENTER);
	}
	overlay_layout_widget->SetRect(initial_placement);

	// Custom dialog padding: Adjust placement of page root
	OpRect page_root_rect = GetPagesGroup()->GetRect();
	int min_width = page_root_rect.x + page_root_rect.width + left_padding + right_padding;
	if (min_width > initial_rect.width)
		min_width = initial_rect.width;
	int additional_padding = (initial_placement.width - min_width) >> 2;
	if (additional_padding > 0)
	{
		page_root_rect.x += additional_padding;
		page_root_rect.width -= additional_padding * 2;
		GetPagesGroup()->SetRect(page_root_rect, FALSE, TRUE);
	}

	// Set proxy rect and security button
	if (m_address_proxy_widget)
	{
		OpRect proxy_rect = address_rect;
		proxy_rect.x = left_padding;
		if (!m_attached_to_address_field)
			proxy_rect.width = initial_placement.width - left_padding - right_padding;
		GetWidgetContainer()->GetRoot()->SetChildRect(m_address_proxy_widget, proxy_rect);
		m_address_proxy_widget->SetEnabled(FALSE);
	}

	if (addressDropdown)
		addressDropdown->SetEnabled(!m_attached_to_address_field);
}

void PasswordDialog::OnInit()
{
	OpString server_label, message_label, login_label;
	if ( m_callback->GetType() == OpAuthenticationCallback::PROXY_AUTHENTICATION_NEEDED ||
		 m_callback->GetType() == OpAuthenticationCallback::PROXY_AUTHENTICATION_WRONG    )
	{
		g_languageManager->GetString(Str::D_PASSWORD_DLG_PROXY_SERVER, server_label);
		g_languageManager->GetString(Str::D_PASSWORD_DLG_PROXY_AUTH, message_label);
		SetTitle(message_label.CStr());
	}
	else
	{
		g_languageManager->GetString(Str::DI_IDM_DOWNDLG_SERVER, server_label);
	}
	g_languageManager->GetString(Str::DI_IDM_SERVER_MESSAGE_LABEL, message_label);
	g_languageManager->GetString(Str::D_PLEASE_LOG_IN, login_label);

	// Labels
	SetWidgetText("Server_label", server_label.CStr());
	SetWidgetText("Message_label", message_label.CStr());
	SetWidgetText("Login_label", login_label.CStr());

	SetWidgetText("Message_info", m_callback->Message());

	if (0 != (m_address_proxy_widget = OP_NEW(OpWidget, ())))
	{
		GetWidgetContainer()->GetRoot()->AddChild(m_address_proxy_widget, FALSE, TRUE);
		m_address_proxy_widget->SetSkinned(TRUE);
	}

	OpTrustAndSecurityButton *server_info = (OpTrustAndSecurityButton*)GetWidgetByName("Server_info");
	if (server_info)
	{
		OpString text;
		text.Set(m_callback->ServerUniName());
		server_info->SetTrustAndSecurityLevel(text, NULL, m_callback, TRUE, TRUE);
	}
	OpLabel *submit_warning = (OpLabel*)GetWidgetByName("Submit_warning");

	if (submit_warning)
	{
		OpString submit_warning_text;
		if (m_callback->SecurityLevel() == OpAuthenticationCallback::AUTH_SEC_PLAIN_TEXT
			&& (!server_info ||
				!server_info->URLTypeIsHTTPS()
			))
		{
			g_languageManager->GetString(Str::D_PASSWORD_DLG_SUBMIT_WARNING, submit_warning_text);
		}
		else
		{
			ShowWidget("label_group", FALSE);
		}
		submit_warning->SetText(submit_warning_text.CStr());
		submit_warning->SetForegroundColor(OP_RGB(0xff,0,0));
	}

	// Disable header group if this is not really an overlay dialog:
	if (!UseOverlayedSkin())
		ShowWidget("Header_group", FALSE);

	// Increase size of dialog header:
	OpWidget* header_label = GetWidgetByName("Login_label");
	if (header_label)
		header_label->SetFontInfo(header_label->font_info.font_info, header_label->font_info.size+3, header_label->font_info.italic, 7 /*(bold)*/, header_label->font_info.justify);

	// Set submit button as default
	OpButton* submit_button = (OpButton*)GetWidgetByName("Submit_button");
	if (submit_button)
		submit_button->SetDefaultLook(TRUE);

	OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Username_dropdown");
	if (dropdown)
	{
		dropdown->SetEditableText();
		dropdown->SetDelayOnChange(TRUE);
		dropdown->edit->autocomp.SetType(AUTOCOMPLETION_PERSONAL_INFO);

		SetWidgetText("Username_dropdown", m_callback->LastUserName());

		// Fill the usernamedropdown with items from wand

		if (m_callback->GetType() == OpAuthenticationCallback::PROXY_AUTHENTICATION_NEEDED ||
			m_callback->GetType() == OpAuthenticationCallback::PROXY_AUTHENTICATION_WRONG) // Base the idstring on server and port
		{
			OpString tmp;
			tmp.AppendFormat(UNI_L(":%d"), m_callback->Port());
			m_idstring.Set(m_callback->ServerNameC());
			m_idstring.Append(tmp);
		}
		else // Base the idstring on url
		{
			m_idstring.Set(m_callback->Url());
			MakeWandUrl(m_idstring);
			m_idstring.Insert(0, UNI_L("*")); ///< So that it is on the entire server.
		}

		// The entries in the wand database are now in the order oldest -> newest but
		// we want to display them in the opposite order
		int index = 0;
		const WandLogin* wandlogin = g_wand_manager->FindLogin(m_idstring.CStr(), index);
		while(wandlogin)
		{
			index++;
			wandlogin = g_wand_manager->FindLogin(m_idstring.CStr(), index);
		}
		while (index > 0)
		{
			const WandLogin* wandlogin = g_wand_manager->FindLogin(m_idstring.CStr(), --index);
			dropdown->AddItem(wandlogin->username.CStr());
		}

		dropdown->SetAlwaysInvoke(TRUE);

		if (dropdown->CountItems() > 0)
		{
			dropdown->SelectItemAndInvoke(0, TRUE, FALSE);
			dropdown->edit->SelectText();

			SetDoNotShowDialogAgain(TRUE);
		}
	}

	OpWidget* edit = GetWidgetByName("Password_edit");

    if (edit)
    {
      ((OpEdit*)edit)->SetPasswordMode(TRUE);
    }

#ifdef SECURITY_INFORMATION_PARSER
    m_callback->CreateURLInformation(&m_url_info);
#endif // SECURITY_INFORMATION_PARSER

    // FIXME: m_info = NULL; // The AuthenticationInformation is no longer valid

	// Restore orignal address
	if (GetAddressDropdown())
		GetAddressDropdown()->RestoreOriginalAddress();

	CompressGroups();
}

const char* PasswordDialog::GetFallbackSkin()
{
	if (UseOverlayedSkin())
		return "Password Dialog Skin";
	return "Dialog Skin";
}

class QuickWandLoginCallback : public WandLoginCallback
{
public:
	QuickWandLoginCallback(OpWindowCommander* commander, OpAuthenticationCallback* callback, BOOL release_windowcommander) :
		m_commander(commander),
		m_callback(callback),
		m_release_windowcommander(release_windowcommander) {}
	~QuickWandLoginCallback();

	OP_STATUS SetUserName(const uni_char* name) { return m_username.Set(name); }
	virtual void OnPasswordRetrieved(const uni_char* password);
	virtual void OnPasswordRetrievalFailed();
private:
	OpWindowCommander* m_commander;
	OpAuthenticationCallback* m_callback;
	BOOL m_release_windowcommander;
	OpString m_username;
};

QuickWandLoginCallback::~QuickWandLoginCallback()
{
	if (m_release_windowcommander)
	{
		g_windowCommanderManager->ReleaseWindowCommander(m_commander);
	}
}
void QuickWandLoginCallback::OnPasswordRetrieved(const uni_char *password)
{
	if (password)
	{
		m_callback->Authenticate(m_username.CStr(), password);
	}

	OP_DELETE(this);
}

void QuickWandLoginCallback::OnPasswordRetrievalFailed()
{
	// Oh, too bad.
	OnPasswordRetrieved(NULL);
}

Str::LocaleString PasswordDialog::GetOkTextID()
{
	return Str::SI_IDFORM_SUBMIT;
}

UINT32 PasswordDialog::OnOk()
{
    OpString password16;
    OpString username16;
	OpString ghost_text;
	OpEdit* password_field = (OpEdit*)(GetWidgetByName("Password_edit"));
	if(password_field)
		password_field->GetGhostText(ghost_text);

	GetWidgetText("Username_dropdown", username16);
	GetWidgetText("Password_edit", password16);

	BOOL remember_password = IsDoNotShowDialogAgainChecked();

	OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Username_dropdown");

	// The dropdown and the wand database has items in opposite order
	int login_index = dropdown->CountItems() - dropdown->GetSelectedItem() - 1;
	WandLogin* login = g_wand_manager->FindLogin(m_idstring.CStr(), login_index);

	if (g_wand_manager->GetIsSecurityEncrypted() &&
		ghost_text.HasContent() &&
		login &&
		login->username.Compare(username16) == 0)
	{
		OpAutoPtr<QuickWandLoginCallback> callback(OP_NEW(QuickWandLoginCallback, (m_commander, 
				m_callback, m_clean_windowcommander)));
		if (OpStatus::IsSuccess(callback->SetUserName(username16.CStr())))
		{
			OP_STATUS get_login_success = OpStatus::OK;
			if (m_clean_windowcommander)
			{
				get_login_success = g_wand_manager->GetLoginPasswordWithoutWindow(login, callback.get());
			}
			else
			{
				get_login_success = WindowCommanderProxy::GetLoginPassword(m_commander, login, callback.get());
			}
			if (OpStatus::IsSuccess(get_login_success))
			{
				callback.release();
				if (m_clean_windowcommander)
				{
					// Owned by the callback now
					m_clean_windowcommander = FALSE;
				}
				return 0;
			}
		}
	}
	else if (remember_password && username16.HasContent() && password16.HasContent())
	{
		if (m_clean_windowcommander)
		{
			g_wand_manager->StoreLoginWithoutWindow(m_idstring.CStr(), username16.CStr(), password16.CStr() );
		}
		else
		{
			WindowCommanderProxy::StoreLogin(m_commander, m_idstring.CStr(), username16.CStr(), password16.CStr() );
		}
	}
	else // if (!remember_password)
	{
		g_wand_manager->DeleteLogin(m_idstring.CStr(), username16.CStr());
	}

	m_callback->Authenticate(username16.CStr(), password16.CStr());

	if(m_clean_windowcommander)
	{
		g_windowCommanderManager->ReleaseWindowCommander(m_commander);
		m_commander = NULL;
	}

    return 0;
}

void PasswordDialog::OnCancel()
{
	m_callback->CancelAuthentication();

	if(m_clean_windowcommander)
	{
		g_windowCommanderManager->ReleaseWindowCommander(m_commander);
		m_commander = NULL;
	}

}

/**
 * Simple memory model. Owned by the wand api and the creator in parallel. Will delete itself when
 * it has been called by wand and ForgetDialog has been called, regardless of order.
 */
class PasswordFiller : public WandLoginCallback
{
public:
	PasswordFiller(PasswordDialog* dialog) : m_dialog(dialog), m_has_been_called(FALSE) {}
	virtual void OnPasswordRetrieved(const uni_char* password);
	virtual void OnPasswordRetrievalFailed();
	void ForgetDialog()
	{
		m_dialog = NULL;
		if (m_has_been_called)
			OP_DELETE(this);
	}
private:
	PasswordDialog* m_dialog;
	BOOL m_has_been_called;
};

void PasswordFiller::OnPasswordRetrieved(const uni_char* password)
{
	m_has_been_called = TRUE;
	if (m_dialog)
	{
		m_dialog->SetWidgetText("Password_edit", password ? password : UNI_L(""));
		// Will be deleted when ForgetDialog is called
	}
	else
	{
		// We got a callback later than expected. This is possible according to the API
		// but this code isn't written to handle it.
		OP_ASSERT(!"Unsupported behaviour of the wand api");
		OP_DELETE(this);
	}
}

void PasswordFiller::OnPasswordRetrievalFailed()
{
	OnPasswordRetrieved(NULL);
}

void PasswordDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Username_dropdown");
	OpEdit* password_field = (OpEdit*)(GetWidgetByName("Password_edit"));
	if (widget == dropdown)
	{
		if (dropdown->GetSelectedItem() >= 0)
		{
			// The order in the dropdown is the opposite of the order in the
			// wand database
			int login_index = dropdown->CountItems() - dropdown->GetSelectedItem() - 1;
			WandLogin* login = g_wand_manager->FindLogin(m_idstring.CStr(), login_index);
			OP_ASSERT(login);
			dropdown->SetText(login->username.CStr());

			OpString pass;

			if (!g_wand_manager->GetIsSecurityEncrypted())
			{
				// Will only be used synchronous since we have !security_encrypted
				PasswordFiller* filler = OP_NEW(PasswordFiller, (this));
				if (filler)
				{
					OP_STATUS get_login_success = OpStatus::OK;
					if (m_clean_windowcommander)
					{
						get_login_success = g_wand_manager->GetLoginPasswordWithoutWindow(login, filler);
					}
					else
					{
						get_login_success = WindowCommanderProxy::GetLoginPassword(m_commander, login, filler);
					}
					if (OpStatus::IsSuccess(get_login_success))
					{
						filler->ForgetDialog(); // Will/might delete the filler.
					}
					else
					{
						OP_DELETE(filler);
					}
				}
//				if (OpStatus::IsSuccess(login->GetPassword(pass)))
//				{
//					SetWidgetText("Password_edit", pass.CStr());
//				}
			}
			else
			{
				OpString label_password_hidden;

				g_languageManager->GetString(Str::D_PASSWORD_NOT_DISPLAYED,label_password_hidden);
				if(password_field)
					password_field->SetGhostText(label_password_hidden.CStr());
			}
		}
	}
	else if(widget == password_field)
	{
		password_field->SetGhostText(UNI_L(""));
	}
	Dialog::OnChange(widget, changed_by_mouse);
}

void PasswordDialog::OnPaint(OpWidget *widget, const OpRect &paint_rect)
{
	// Paint address field on top of dialog
	if (m_attached_to_address_field && widget == m_address_proxy_widget)
	{
		// Make sure address dropdown exist
		OpAddressDropDown* addressDropdown = GetAddressDropdown();
		if (addressDropdown)
		{
			OpRect address_rect = addressDropdown->GetRect(TRUE);
			OpRect proxy_rect = widget->GetRect(TRUE);

			VisualDevice* old_visdev = addressDropdown->GetVisualDevice();
			VisualDevice* new_visdev = widget->GetVisualDevice();

			// Reset position
			AffinePos pos = new_visdev->GetCTM();
			new_visdev->Translate(-pos.GetTranslation().x - address_rect.x + proxy_rect.x, -pos.GetTranslation().y - address_rect.y + proxy_rect.y);

			// Paint
			addressDropdown->SetVisualDevice(new_visdev);
			addressDropdown->GenerateOnPaint(paint_rect);
			addressDropdown->SetVisualDevice(old_visdev);

			// Restore position
			new_visdev->SetCTM(pos);
		}
		else
			m_attached_to_address_field = FALSE;
	}
}

void PasswordDialog::OnClose(BOOL user_initiated)
{
	Dialog::OnClose(user_initiated);
}

void PasswordDialog::OnAddressDropdownResize()
{
	ResetDialog();
}

BOOL PasswordDialog::OnInputAction(OpInputAction * action)
{
	switch (action->GetAction())
	{
	case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_BASIC_AUTHENTICATION_INFO:
					return TRUE;
				default:
					break;
			}
		}
		break;
	case OpInputAction::ACTION_BASIC_AUTHENTICATION_INFO:
#ifdef _SECURE_INFO_SUPPORT
	case OpInputAction::ACTION_SHOW_SECURITY_INFORMATION:
	case OpInputAction::ACTION_EXTENDED_SECURITY:
	case OpInputAction::ACTION_HIGH_SECURITY:
	case OpInputAction::ACTION_NO_SECURITY:
#ifdef TRUST_RATING
	case OpInputAction::ACTION_TRUST_INFORMATION:
	case OpInputAction::ACTION_TRUST_UNKNOWN:
	case OpInputAction::ACTION_TRUST_FRAUD:
#endif // TRUST_RATING
	{
		SecurityInformationDialog* secInfoDialog = OP_NEW(SecurityInformationDialog, ());
		if (secInfoDialog)
		{
			secInfoDialog->Init(this, NULL, m_url_info);
		}
		return TRUE;
	}
#endif //_SECURE_INFO_SUPPORT
	default:
		break;
	}

	return Dialog::OnInputAction(action);
}
