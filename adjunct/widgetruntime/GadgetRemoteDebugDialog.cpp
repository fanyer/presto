/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Bazyli Zygan bazyl@opera.com
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/quick/Application.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"

#include "adjunct/widgetruntime/GadgetRemoteDebugDialog.h"
#include "adjunct/widgetruntime/GadgetRemoteDebugHandler.h"
#include "adjunct/quick_toolkit/widgets/OpIcon.h"

#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpButton.h"


namespace{
	const char GADGET_CONNECTION_STATUS[] = "gadget_connection_status_label";
	const char GADGET_CONNECTION_ICON[] = "gadget_connection_status_icon";
}

GadgetRemoteDebugDialog::GadgetRemoteDebugDialog()
	: m_debug_handler(NULL)
{
}


GadgetRemoteDebugDialog::~GadgetRemoteDebugDialog()
{
	if (NULL != m_debug_handler)
	{
		OpStatus::Ignore(m_debug_handler->RemoveListener(*this));
	}
}


OP_STATUS GadgetRemoteDebugDialog::Init(GadgetRemoteDebugHandler& debug_handler)
{
	m_debug_handler = &debug_handler;
	RETURN_IF_ERROR(m_debug_handler->AddListener(*this));

	RETURN_IF_ERROR(Dialog::Init(
					g_application->GetActiveDesktopWindow(TRUE)));

	// _Must_ return OpStatus::OK from now on.

	// Set bolds where they should be
	static_cast<OpLabel*>(GetWidgetByName("gadget_remote_debug_title_label"))->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);
	//static_cast<OpLabel*>(GetWidgetByName("gadget_remote_debug_setup_label"))->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);

	// Set ip and port where they should be
	const GadgetRemoteDebugHandler::State& debug_state =
			m_debug_handler->GetState();
	
	if (debug_state.m_ip_address.Length() < 8)
	{
		SetDefaults();
	}
	else
	{
		OpStringC ip(debug_state.m_ip_address);
		ParseIpAddress(ip, debug_state.m_port_no);
	}
	
	SetEditEnabled(!debug_state.m_connected);
	if (debug_state.m_connected)
	{
		SetWidgetText(GADGET_CONNECTION_STATUS,Str::D_GADGET_REMOTE_DEBUG_SETUP_CONNECTED);
	}

	return OpStatus::OK;
}

void GadgetRemoteDebugDialog::Connect()
{
	OP_ASSERT(NULL != m_debug_handler);
	SetEditEnabled(FALSE);
	 
	int port;
	OpString ip, port_str;
	
	static_cast<OpEdit*>(GetWidgetByName("gadget_ip_edit"))->GetText(ip);		
	static_cast<OpEdit*>(GetWidgetByName("gadget_port_edit"))->GetText(port_str);	
	
	SetWidgetText(GADGET_CONNECTION_STATUS,Str::D_GADGET_REMOTE_DEBUG_SETUP_CONNECTING);
	OpIcon* progress_icon = static_cast<OpIcon*>(
			GetWidgetByName(GADGET_CONNECTION_ICON));

	OP_ASSERT(NULL != progress_icon);

	progress_icon->SetImage("Thumbnail Busy Image"); 
	ShowWidget(GADGET_CONNECTION_ICON,TRUE);

 
	port = NumberFromString(port_str);
	GadgetRemoteDebugHandler::State debug_state;
	debug_state.m_ip_address.Set(ip);
	debug_state.m_port_no = port;
	debug_state.m_connected = TRUE;
	OpStatus::Ignore(m_debug_handler->SetState(debug_state));
	 
	
}

void GadgetRemoteDebugDialog::Disconnect()
{
	SetEditEnabled(TRUE);
	GadgetRemoteDebugHandler::State debug_state;
	debug_state.m_connected = FALSE;
	OpStatus::Ignore(m_debug_handler->SetState(debug_state));
	SetWidgetText(GADGET_CONNECTION_STATUS,Str::D_GADGET_REMOTE_DEBUG_SETUP_DISCONNECTED);
	ShowWidget(GADGET_CONNECTION_ICON,FALSE);
}


void GadgetRemoteDebugDialog::OnConnectionSuccess()
{
	SetEditEnabled(FALSE);
	SetWidgetText(GADGET_CONNECTION_STATUS,Str::D_GADGET_REMOTE_DEBUG_SETUP_CONNECTED);
	ShowWidget(GADGET_CONNECTION_ICON,FALSE);
}

void GadgetRemoteDebugDialog::OnConnectionFailure()
{
	SetEditEnabled(TRUE);
	SetWidgetText(GADGET_CONNECTION_STATUS,Str::D_GADGET_REMOTE_DEBUG_SETUP_CONNECT_FAILED);
	ShowWidget(GADGET_CONNECTION_ICON,FALSE);
}

void GadgetRemoteDebugDialog::OnClick(OpWidget* widget, UINT32 id)
{
	if (widget->GetName() == "gadget_connect_button")
	{
		Connect();
	}
	if (widget->GetName() == "gadget_disconnect_button")
	{
		Disconnect();
	}	
}

void GadgetRemoteDebugDialog::SetDefaults()
{
	static_cast<OpEdit*>(GetWidgetByName("gadget_ip_edit"))->SetText(UNI_L("127.0.0.1"));	
	static_cast<OpEdit*>(GetWidgetByName("gadget_port_edit"))->SetText(UNI_L("7001"));		
}

void GadgetRemoteDebugDialog::ParseIpAddress(const OpStringC& ip_address, int port)
{
	OpString ip, prt;	 
	char port_string[10];
 
	// And now create a string for port
	snprintf(port_string, 10, "%d", port);
	prt.Append(port_string, strlen(port_string));
			   
	static_cast<OpEdit*>(GetWidgetByName("gadget_ip_edit"))->SetText(ip_address);	
	static_cast<OpEdit*>(GetWidgetByName("gadget_port_edit"))->SetText(prt);			
	
}

void GadgetRemoteDebugDialog::SetEditEnabled(BOOL enable)
{
	static_cast<OpButton*>(GetWidgetByName("gadget_connect_button"))->SetEnabled(enable == 1);		
	static_cast<OpButton*>(GetWidgetByName("gadget_disconnect_button"))->SetEnabled(enable != 1);
	
	static_cast<OpEdit*>(GetWidgetByName("gadget_ip_edit"))->SetEnabled(enable);	
	static_cast<OpEdit*>(GetWidgetByName("gadget_port_edit"))->SetEnabled(enable);	
	
}

int GadgetRemoteDebugDialog::NumberFromString(OpString& str)
{
	int val = 0;
	uni_char *s = str.CStr();
	
	for (int i = 0; i < str.Length(); i++)
	{
		if (s[i] < '0' || s[i] > '9')
		{
			val = -1;
			break;
		}
		val = val*10 + (s[i] - '0');
	}
	
	
	return val;
}

#endif // WIDGET_RUNTIME_SUPPORT
