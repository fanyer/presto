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
#include "URLInputDialog.h"
#include "adjunct/quick/widgets/OpAddressDropDown.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "modules/locale/oplanguagemanager.h"

/***********************************************************************************
**
**	SetMessageAndTitle
**
***********************************************************************************/

void URLInputDialog::SetTitleAndMessage( const OpStringC& title, const OpStringC& message)
{
	SetTitle(title.CStr());
	
	OpLabel* label = static_cast<OpLabel*> (GetWidgetByName("message_field"));
	if (label)
	{
		label->SetText(message.CStr());
	}
}

/***********************************************************************************
**
**	OnInit
**
***********************************************************************************/

void URLInputDialog::OnInit()
{
	OpAddressDropDown* address_dropdown = (OpAddressDropDown*) GetWidgetByName("address_field");
	if(address_dropdown)
	{
		address_dropdown->SetInvokeActionOnClick(FALSE);
		address_dropdown->SetMaxNumberOfColumns(1);
		address_dropdown->SetOpenInTab(FALSE);
		OpInputAction *okaction = OP_NEW(OpInputAction, (OpInputAction::ACTION_OK));
		address_dropdown->SetAction(okaction);
	}
}

/***********************************************************************************
**
**	OnOk
**
***********************************************************************************/

UINT32 URLInputDialog::OnOk()
{
	OpAddressDropDown* address_dropdown = (OpAddressDropDown*) GetWidgetByName("Address_field");
	if(address_dropdown)
	{
		OpString URL_result;
		address_dropdown->GetText(URL_result);
		if (m_url_input_listener)
			m_url_input_listener->OnURLInputOkCallback(URL_result);
	}
	return 0;
}

/***********************************************************************************
**
**	OnClose
**
***********************************************************************************/

void URLInputDialog::OnCancel()
{
	if (m_url_input_listener)
		m_url_input_listener->OnURLInputCancelCallback();
}
