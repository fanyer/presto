/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/dialogs/CookieWarningDialog.h"
#include "modules/util/str.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "adjunct/quick/Application.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"

/***********************************************************************************
**
**	WarnCookieDomainError
**
***********************************************************************************/

void WarnCookieDomainError(const char *url, const char *cookie, BOOL no_ip_address)
{
	CookieWarningDialog* dialog = OP_NEW(CookieWarningDialog, (url, cookie, no_ip_address));
	if (dialog)
	{
		if (OpStatus::IsSuccess(dialog->init_status))
			dialog->Init(g_application->GetActiveDesktopWindow());
		else
			OP_DELETE(dialog);
	}
}

/***********************************************************************************
**
**	CookieWarningDialog
**
***********************************************************************************/

CookieWarningDialog::CookieWarningDialog(const char *url, const char *cookie, BOOL no_ip_address)
{
	if (0 != (m_cookie_error = OP_NEW(cookiedomain_error_st, ())))
	{
		m_cookie_error->url = SetNewStr(url);
		m_cookie_error->cookie = SetNewStr(cookie);
		m_cookie_error->no_ip_address = no_ip_address;
		m_cookie_error->timer = 0;
	}
	else
		init_status = OpStatus::ERR_NO_MEMORY;
}

/***********************************************************************************
**
**	~CookieWarningDialog
**
***********************************************************************************/

CookieWarningDialog::~CookieWarningDialog()
{
	OP_DELETE(m_cookie_error);
}

/***********************************************************************************
**
**	OnInit
**
***********************************************************************************/

void CookieWarningDialog::OnInit()
{
	OpString cookiedomerrexplain;
	g_languageManager->GetString((m_cookie_error->no_ip_address ? Str::SI_MSG_NO_DOMAIN_IP_ADDRESS_COOKIE: Str::SI_MSG_ILLEGAL_DOMAIN_COOKIE), cookiedomerrexplain);

	OpMultilineEdit* label = (OpMultilineEdit*)GetWidgetByName("Cookie_info_edit");
	if(label)
	{
		label->SetLabelMode();
		label->SetText(cookiedomerrexplain.CStr());
	}

	SetWidgetText("Url_content", m_cookie_error->url);

	label = (OpMultilineEdit*)GetWidgetByName("Cookie_content");
	if(label)
	{
		label->SetReadOnly(TRUE);
		OpString cookiecontent;
		cookiecontent.Set(m_cookie_error->cookie);
		label->SetText(cookiecontent.CStr());
	}

	OP_DELETEA(m_cookie_error->url);
	OP_DELETEA(m_cookie_error->cookie);
	m_cookie_error->url = NULL;
	m_cookie_error->cookie = NULL;
	m_cookie_error->timer = 0;
}

/***********************************************************************************
**
**	OnOk
**
***********************************************************************************/

UINT32 CookieWarningDialog::OnOk()
{
	if (IsDoNotShowDialogAgainChecked())
	{
		TRAPD(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::ShowCookieDomainErr, FALSE));
	}
	return 0;
}
