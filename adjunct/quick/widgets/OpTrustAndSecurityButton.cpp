/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/**
 * @file OpTrustAndSecurityButton.cpp
 * @author rfz@opera.com
 *
 * Contains function definitions for the classes defined in the corresponding
 * header file.
 */

#include "core/pch.h"
#include "adjunct/quick/dialogs/SecurityInformationDialog.h"
#include "adjunct/quick/managers/DesktopSecurityManager.h"
#include "adjunct/quick/widgets/OpTrustAndSecurityButton.h"
#include "modules/doc/doc.h"
#include "modules/dochand/win.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"

/*static*/
OP_STATUS OpTrustAndSecurityButton::Construct(OpTrustAndSecurityButton ** obj)
{
	*obj = OP_NEW(OpTrustAndSecurityButton, ());
	if (!*obj)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

/***********************************************************************************
**
**	OpTrustAndSecurityButton
**
***********************************************************************************/

OpTrustAndSecurityButton::OpTrustAndSecurityButton()
	:	OpButton(OpButton::TYPE_CUSTOM),
		m_sec_mode(OpDocumentListener::UNKNOWN_SECURITY),
		m_suppress_text(FALSE)
#ifdef TRUST_RATING
	, 	m_trust_mode(OpDocumentListener::TRUST_NOT_SET)
#endif // TRUST_RATING
#ifdef SECURITY_INFORMATION_PARSER
	, 	m_url_type(OpWindowCommander::WIC_URLType_Unknown)
#endif // SECURITY_INFORMATION_PARSER
{
	OpInputAction* action;
	if (OpStatus::IsSuccess(OpInputAction::CreateInputActionsFromString("Trust Unknown", action)))
		SetAction(action);
	SetButtonStyle(OpButton::STYLE_IMAGE_AND_TEXT_ON_RIGHT);
	SetFixedImage(FALSE);
	GetBorderSkin()->SetImage("Low Security Button Skin");
	GetBorderSkin()->SetForeground(TRUE);
	SetText(UNI_L(""));
}

/***********************************************************************************
**
**	SetTrustAndSecurityLevel
**
***********************************************************************************/
BOOL
OpTrustAndSecurityButton::SetTrustAndSecurityLevel(
	const OpString &					button_text,
	OpWindowCommander* 					wic,
	OpAuthenticationCallback*			callback 		/* = NULL*/,
	BOOL								show_server_name/* = FALSE*/,
	BOOL								always_visible	/* = FALSE*/)
{
	if (!wic && !callback)
		return FALSE;

	SetText(UNI_L(""));
	OpDocumentListener::SecurityMode old_sec_mode = m_sec_mode;

	if (wic)
	{
		m_sec_mode = wic->GetSecurityMode();
		m_server_name.Set(wic->GetCurrentURL(FALSE));
		m_trust_mode = wic->GetTrustMode();
#ifdef SECURITY_INFORMATION_PARSER
		m_url_type = wic->GetWicURLType();
#endif // SECURITY_INFORMATION_PARSER
	}
	else // if (auth_info)
	{
#ifdef SECURITY_INFORMATION_PARSER
		if (	callback->GetType() != OpAuthenticationCallback::PROXY_AUTHENTICATION_NEEDED
			&&	callback->GetType() != OpAuthenticationCallback::PROXY_AUTHENTICATION_WRONG)
			m_sec_mode = (OpDocumentListener::SecurityMode) callback->GetSecurityMode();
#endif // SECURITY_INFORMATION_PARSER
		m_server_name.Set(callback->ServerUniName());
#ifdef SECURITY_INFORMATION_PARSER
		m_trust_mode = (OpDocumentListener::TrustMode) callback->GetTrustMode();
		m_url_type = (OpWindowCommander::WIC_URLType) callback->GetWicURLType();
#endif // SECURITY_INFORMATION_PARSER
	}

	OpDocumentListener::TrustMode  old_trust_mode = m_trust_mode;

	// Set the buttons appearance.
	if (always_visible || ButtonNeedsToBeVisible())
	{
		if (m_trust_mode == OpDocumentListener::PHISHING_DOMAIN)
		{
			SetFraudAppearance();
		}
		else if (g_desktop_security_manager->IsEV(m_sec_mode))
		{
			SetHighAssuranceAppearance(button_text);
		}
		else if (m_sec_mode == OpDocumentListener::HIGH_SECURITY)
		{
			SetSecureAppearance(button_text);
		}
		else if (m_sec_mode == OpDocumentListener::NO_SECURITY)
		{
			SetInsecureAppearance();
		}
		else
		{
			SetUnknownAppearance();
		}
		if (show_server_name)
			SetText(button_text.CStr());
	}

	return ((old_sec_mode != m_sec_mode) || (old_trust_mode != m_trust_mode));
}

/***********************************************************************************
**
**	ButtonNeedsToBeVisible
**
***********************************************************************************/

BOOL OpTrustAndSecurityButton::ButtonNeedsToBeVisible()
{
	if(m_trust_mode == OpDocumentListener::PHISHING_DOMAIN)
		return TRUE;

	// Don't display if
	//   - The button has been disabled and the page is insecure.
	//   or
	//   - The page has no server name
	//   or
	//   - The page is not of type http nor https
	if (  (m_server_name.IsEmpty())
#ifdef SECURITY_INFORMATION_PARSER
			 ||	((m_url_type != OpWindowCommander::WIC_URLType_HTTP)  && (m_url_type != OpWindowCommander::WIC_URLType_HTTPS) && (m_url_type != OpWindowCommander::WIC_URLType_FTP))
#endif // SECURITY_INFORMATION_PARSER
		  )
	{
		return FALSE;
	}

	// if there is no security or trust problems hide the button anyway
	if (m_sec_mode == OpDocumentListener::NO_SECURITY && m_trust_mode != OpDocumentListener::PHISHING_DOMAIN)
	{
		return FALSE;
	}

	return TRUE;
}

/***********************************************************************************
**
**	SetFraudAppearance
**
***********************************************************************************/

void OpTrustAndSecurityButton::SetFraudAppearance()
{
	GetBorderSkin()->SetImage("Untrusted Site Security Button Skin");
	GetBorderSkin()->SetForeground(TRUE);
	OpInputAction* fraud_action;
	if (OpStatus::IsSuccess(OpInputAction::CreateInputActionsFromString("Trust Fraud", fraud_action)))
		SetAction(fraud_action);

	OpString text;
	g_languageManager->GetString(Str::D_NEW_SECURITY_INFORMATION_FRAUD_SITE, text);

	SetText(text.CStr());
}

/***********************************************************************************
**
**	SetSecureAppearance
**
***********************************************************************************/

void OpTrustAndSecurityButton::SetSecureAppearance(const OpString &button_text)
{
	// The site has high security (encryption wise), therefore is must be ok.
	// A cert should should indicate even more trust, so we don't really
	// care if it is known or unknown.
	GetBorderSkin()->SetImage("Security Button Skin");
	GetBorderSkin()->SetForeground(TRUE);
	OpInputAction* security_action;
	if (OpStatus::IsSuccess(OpInputAction::CreateInputActionsFromString("High Security,,,,High Security Simple", security_action)))
		SetAction(security_action);

	SetText(button_text.CStr());
}

/***********************************************************************************
 **
 **	SetInsecureAppearance
 **
 ***********************************************************************************/

void OpTrustAndSecurityButton::SetInsecureAppearance()
{
	GetBorderSkin()->SetImage("Low Security Button Skin");
	GetBorderSkin()->SetForeground(TRUE);
	OpInputAction* unknown_action;
	if (OpStatus::IsSuccess(OpInputAction::CreateInputActionsFromString("No Security", unknown_action)))
		SetAction(unknown_action);
	SetText(UNI_L(""));
}

/***********************************************************************************
**
**	SetUnknownAppearance
**
***********************************************************************************/

void OpTrustAndSecurityButton::SetUnknownAppearance()
{
	GetBorderSkin()->SetImage("Low Security Button Skin");
	GetBorderSkin()->SetForeground(TRUE);
	OpInputAction* unknown_action;
#ifdef TRUST_RATING
	if (OpStatus::IsSuccess(OpInputAction::CreateInputActionsFromString("Trust Unknown", unknown_action)))
#else // !TRUST_RATING
	if (OpStatus::IsSuccess(OpInputAction::CreateInputActionsFromString("No Security", unknown_action)))
#endif // !TRUST_RATING
		SetAction(unknown_action);
	SetText(UNI_L(""));
}

/***********************************************************************************
**
**	SetKnownAppearance
**
***********************************************************************************/

void OpTrustAndSecurityButton::SetKnownAppearance()
{
	GetBorderSkin()->SetImage("Low Security Button Skin");
	GetBorderSkin()->SetForeground(TRUE);
	OpInputAction* information_action;
	if (OpStatus::IsSuccess(OpInputAction::CreateInputActionsFromString("Trust Information", information_action)))
		SetAction(information_action);
	SetText(UNI_L(""));
}

/***********************************************************************************
**
** SetHighAssuranceAppearance
**
***********************************************************************************/

void OpTrustAndSecurityButton::SetHighAssuranceAppearance(const OpString &button_text)
{
	GetBorderSkin()->SetImage("High Assurance Security Button Skin");
	GetBorderSkin()->SetForeground(TRUE);
	OpInputAction* security_action;
	if (OpStatus::IsSuccess(OpInputAction::CreateInputActionsFromString("Extended Security", security_action)))
		SetAction(security_action);
	SetText(button_text.CStr());
}

/***********************************************************************************
**
** SetText
**
***********************************************************************************/

OP_STATUS OpTrustAndSecurityButton::SetText(const uni_char* text)
{
	RETURN_IF_ERROR(m_button_text.Set(text));
	if (m_suppress_text)
		return OpButton::SetText(UNI_L(""));
	else
		return OpButton::SetText(text);
}

/***********************************************************************************
**
** SuppressText
**
***********************************************************************************/

void OpTrustAndSecurityButton::SuppressText(BOOL suppress)
{
	if (suppress == m_suppress_text)
		return;
	m_suppress_text = suppress;
	if (suppress)
		OpButton::SetText(UNI_L(""));
	else
		OpButton::SetText(m_button_text.CStr());
}

/***********************************************************************************
**
** GetScaledRequiredSize
**
***********************************************************************************/

void OpTrustAndSecurityButton::GetRequiredSize(INT32& width, INT32& height)
{
	INT32 in_width = width;
	INT32 in_height = height;
	INT32 suppressed_text_width;
	INT32 scaled_suppressed_text_width;

	// GetRequiredSize

	if (!m_suppress_text)
		OpButton::SetText(UNI_L(""));
	OpButton::GetRequiredSize(suppressed_text_width, height); // ignore this value of height
	OpButton::SetText(m_button_text.CStr());
	OpButton::GetRequiredSize(width, height); // use this value of height
	if (m_suppress_text)
		OpButton::SetText(UNI_L(""));

	if (height != in_height)
	{
		// We are going to scale the security button image, so
		// we should scale the width as well as the height
		float image_scale_factor = ((float)in_height) / ((float)height);
		scaled_suppressed_text_width = (INT32) (image_scale_factor * (float)suppressed_text_width);
	}
	else
		scaled_suppressed_text_width = suppressed_text_width;

	width -= suppressed_text_width - scaled_suppressed_text_width;
	if (in_width - width < 200)
	{
		width = scaled_suppressed_text_width;
		SuppressText(TRUE);
	}
	else
		SuppressText(FALSE);
}
