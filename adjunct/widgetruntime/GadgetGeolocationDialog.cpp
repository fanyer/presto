/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Blazej Kazmierczak bkazmierczak@opera.com
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT
#ifdef DOM_GEOLOCATION_SUPPORT

#include "adjunct/quick/Application.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/widgetruntime/GadgetGeolocationDialog.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"

#include "modules/widgets/OpButton.h"
#include "modules/locale/oplanguagemanager.h"

GadgetGeolocationDialog::GadgetGeolocationDialog(OpInputContext* ctx):m_callback_ctx(ctx)
{

}

UINT32 GadgetGeolocationDialog::OnOk()
{
	BOOL enable = FALSE;
	OpButton *checkbox = (OpButton *)GetWidgetByName("remember_choice");
	if(checkbox)
	{
		enable = checkbox->GetValue() != 0;
	}

	g_input_manager->InvokeAction(OpInputAction::ACTION_PERMISSION_ACCEPT,
			enable,
			NULL,
			m_callback_ctx);

	return 1;
}

void GadgetGeolocationDialog::OnCancel()
{
	BOOL enable = FALSE;
	OpButton *checkbox = (OpButton *)GetWidgetByName("remember_choice");
	if(checkbox)
	{
		enable = checkbox->GetValue() != 0;
	}
	g_input_manager->InvokeAction(OpInputAction::ACTION_PERMISSION_DENY,
			enable,
			NULL,
			m_callback_ctx);
}

OP_STATUS GadgetGeolocationDialog::Init(DesktopWindow* parent, OpString host_name)
{
	m_host_name.Set(host_name);
	RETURN_IF_ERROR(Dialog::Init(parent));
	return OpStatus::OK;
}

void GadgetGeolocationDialog::OnInit()
{
	OpString question_label;
	OpString host_question_text;
	g_languageManager->GetString(Str::D_GEOLOCATION_TOOLBAR_TEXT, question_label);
	
	host_question_text.Reserve(question_label.Length()+ m_host_name.Length()+1);
	uni_snprintf_ss(host_question_text,host_question_text.Capacity(),question_label.CStr(),m_host_name.CStr(),NULL);
	SetWidgetText("host_name_question",host_question_text.CStr());
}
#endif //DOM_GEOLOCATION_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT
