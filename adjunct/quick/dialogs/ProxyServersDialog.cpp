/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "ProxyServersDialog.h"

#include "modules/widgets/OpDropDown.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/locale/oplanguagemanager.h"

#include "adjunct/quick/dialogs/ProxyExceptionDialog.h"
#include "adjunct/desktop_util/prefs/WidgetPrefs.h"

void ProxyServersDialog::Init(DesktopWindow* parent_window)
{
	Dialog::Init(parent_window);
}

void ProxyServersDialog::OnInit()
{
	BOOL http = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseHTTPProxy);
	BOOL https= g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseHTTPSProxy);
	BOOL ftp  = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseFTPProxy);
	BOOL socks = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseSOCKSProxy);

	// Make automatic proxy and manual proxy mutual exclusive, disable manual proxy
	// if automatic proxy is enabled
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AutomaticProxyConfig))
	{
		if (!g_pcnet->GetStringPref(PrefsCollectionNetwork::AutomaticProxyConfigURL).HasContent())
		{
			TRAPD(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::AutomaticProxyConfig, FALSE));
		}
		else
		{
			TRAPD(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseHTTPProxy, FALSE);
						g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseHTTPSProxy, FALSE);
						g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseFTPProxy, FALSE);
						g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseSOCKSProxy, FALSE));
		}
	}

	OpString proxy;

	TRAPD(rc, g_pcnet->GetStringPrefL(PrefsCollectionNetwork::HTTPProxy, proxy));
	if (OpStatus::IsSuccess(rc))
	{
		SetProxyHostPortEntries("Http_server_edit", "Http_port_edit", proxy);
		SetWidgetValue("Http_checkbox", proxy.Length() && http);

		OpWidget *enabled_widget = GetWidgetByName("Http_checkbox");
		SetWidgetEnabled("Http_server_edit", enabled_widget->GetValue() ? TRUE : FALSE);
		SetWidgetEnabled("Http_port_edit", enabled_widget->GetValue() ? TRUE : FALSE);
		SetWidgetEnabled("Use_http_for_all", enabled_widget->GetValue() ? TRUE : FALSE);
	}

	TRAP(rc, g_pcnet->GetStringPrefL(PrefsCollectionNetwork::HTTPSProxy, proxy));
	if (OpStatus::IsSuccess(rc))
	{
		SetProxyHostPortEntries("Https_server_edit", "Https_port_edit", proxy);
		SetWidgetValue("Https_checkbox", proxy.Length() && https);

		OpWidget *enabled_widget = GetWidgetByName("Https_checkbox");
		SetWidgetEnabled("Https_server_edit", enabled_widget->GetValue() ? TRUE : FALSE);
		SetWidgetEnabled("Https_port_edit", enabled_widget->GetValue() ? TRUE : FALSE);
	}

	TRAP(rc, g_pcnet->GetStringPrefL(PrefsCollectionNetwork::FTPProxy, proxy));
	if (OpStatus::IsSuccess(rc))
	{
		SetProxyHostPortEntries("Ftp_server_edit", "Ftp_port_edit", proxy);
		SetWidgetValue("Ftp_checkbox", proxy.Length() && ftp);

		OpWidget *enabled_widget = GetWidgetByName("Ftp_checkbox");
		SetWidgetEnabled("Ftp_server_edit", enabled_widget->GetValue() ? TRUE : FALSE);
		SetWidgetEnabled("Ftp_port_edit", enabled_widget->GetValue() ? TRUE : FALSE);
	}

	TRAP(rc, g_pcnet->GetStringPrefL(PrefsCollectionNetwork::SOCKSProxy, proxy));
	if (OpStatus::IsSuccess(rc))
	{
		SetProxyHostPortEntries("Socks_server_edit", "Socks_port_edit", proxy);
		SetWidgetValue("Socks_checkbox", proxy.Length() && socks);

		OpWidget *enabled_widget = GetWidgetByName("Socks_checkbox");
		SetWidgetEnabled("Socks_server_edit", enabled_widget->GetValue() ? TRUE : FALSE);
		SetWidgetEnabled("Socks_port_edit", enabled_widget->GetValue() ? TRUE : FALSE);
	}

	WidgetPrefs::GetIntegerPref(GetWidgetByName("Use_proxy_for_local_servers_checkbox"), PrefsCollectionNetwork::HasUseProxyLocalNames);

	OpStringC autoproxy(g_pcnet->GetStringPref(PrefsCollectionNetwork::AutomaticProxyConfigURL));
	SetWidgetText("Automatic_proxy_edit", autoproxy.CStr());

	SetWidgetValue("auto_proxy_radio", g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AutomaticProxyConfig));
	SetWidgetValue("manual_proxy_radio", !g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AutomaticProxyConfig));

	BOOL use_http_for_all = g_pcui->GetIntegerPref(PrefsCollectionUI::UseHTTPProxyForAllProtocols);
	// only update "Use_http_for_all" checkbox if it is different, otherwise we can reset other settings (see ApplyHTTPProxySettingsToALLProtocols)
	if (GetWidgetValue("Use_http_for_all") != use_http_for_all)
		SetWidgetValue("Use_http_for_all", use_http_for_all);
}


UINT32 ProxyServersDialog::OnOk()
{
	// Insert your code here
	OpString httpserver;
	OpString httpsserver;
	OpString ftpserver;
	OpString socksserver;
	OpString autoproxy;

	GetProxyHostPortEntries("Http_server_edit", "Http_port_edit", httpserver);
	GetProxyHostPortEntries("Https_server_edit", "Https_port_edit", httpsserver);
	GetProxyHostPortEntries("Ftp_server_edit", "Ftp_port_edit", ftpserver);
	GetProxyHostPortEntries("Socks_server_edit", "Socks_port_edit", socksserver);

	TRAPD(err, g_pcnet->WriteStringL(PrefsCollectionNetwork::HTTPProxy, httpserver));
	TRAP(err, g_pcnet->WriteStringL(PrefsCollectionNetwork::HTTPSProxy, httpsserver));
	TRAP(err, g_pcnet->WriteStringL(PrefsCollectionNetwork::FTPProxy, ftpserver));
	TRAP(err, g_pcnet->WriteStringL(PrefsCollectionNetwork::SOCKSProxy, socksserver));

	if (GetWidgetValue("manual_proxy_radio"))
	{
		// Disable automatic proxy
		TRAP(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::AutomaticProxyConfig, FALSE));

		BOOL http, https, ftp, socks;

		http = GetWidgetValue("Http_checkbox");
		https = GetWidgetValue("Https_checkbox");
		ftp = GetWidgetValue("Ftp_checkbox");
		socks = GetWidgetValue("Socks_checkbox");

		TRAPD(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseHTTPProxy, http));
		TRAP(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseHTTPSProxy, https));
		TRAP(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseFTPProxy, ftp));
		TRAP(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseSOCKSProxy, socks));
	}
	else
	{
		// Disable manual proxy
		TRAP(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseHTTPProxy, FALSE));
		TRAP(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseHTTPSProxy, FALSE));
		TRAP(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseFTPProxy, FALSE));
		TRAP(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseSOCKSProxy, FALSE));

		GetWidgetText("Automatic_proxy_edit", autoproxy);
		TRAP(err, g_pcnet->WriteStringL(PrefsCollectionNetwork::AutomaticProxyConfigURL, autoproxy));
		TRAP(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::AutomaticProxyConfig, autoproxy.HasContent()));
		g_pcnet->EnableAutomaticProxy(autoproxy.HasContent());
	}

	WidgetPrefs::SetIntegerPref(GetWidgetByName("Use_proxy_for_local_servers_checkbox"), PrefsCollectionNetwork::HasUseProxyLocalNames);
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Use_http_for_all"), PrefsCollectionUI::UseHTTPProxyForAllProtocols);

	return 0;
}

void ProxyServersDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget->IsNamed("manual_proxy_radio") || widget->IsNamed("auto_proxy_radio"))
	{
		EnableManualConfigurationProxyGroup(GetWidgetValue("manual_proxy_radio"));
		SetWidgetEnabled("Group_automatic", GetWidgetValue("auto_proxy_radio"));

		if (GetWidgetValue("auto_proxy_radio"))
		{
			// Focus the edit field
			OpWidget* edit = GetWidgetByName("Automatic_proxy_edit");
			if (edit)
				edit->SetFocus(FOCUS_REASON_OTHER);
		}
	}

	if (widget->IsNamed("Use_http_for_all"))
	{
		ApplyHTTPProxySettingsToALLProtocols(GetWidgetValue("Use_http_for_all"));
	}

	if(widget->IsNamed("Http_checkbox"))
	{
		BOOL http_proxy_enabled = widget->GetValue() ? TRUE : FALSE;

		SetWidgetEnabled("Http_server_edit", http_proxy_enabled);
		SetWidgetEnabled("Http_port_edit", http_proxy_enabled);
		SetWidgetEnabled("Use_http_for_all", http_proxy_enabled);

		// when disabling HTTP proxy and "use HTTP for all" is set,
		// all dependant proxies must be disabled as well,
		// but we want to keep the set state of "use HTTP for all" in order to reenable everything,
		// when HTTP proxy is enabled again
		if (GetWidgetValue("Use_http_for_all"))
			ApplyHTTPProxySettingsToALLProtocols(http_proxy_enabled);
	}

	if(widget->IsNamed("Https_checkbox"))
	{
		SetWidgetEnabled("Https_server_edit", widget->GetValue() ? TRUE : FALSE);
		SetWidgetEnabled("Https_port_edit",   widget->GetValue() ? TRUE : FALSE);
	}

	if(widget->IsNamed("Ftp_checkbox"))
	{
		SetWidgetEnabled("Ftp_server_edit", widget->GetValue() ? TRUE : FALSE);
		SetWidgetEnabled("Ftp_port_edit",   widget->GetValue() ? TRUE : FALSE);
	}

	if(widget->IsNamed("Socks_checkbox"))
	{
		SetWidgetEnabled("Socks_server_edit", widget->GetValue() ? TRUE : FALSE);
		SetWidgetEnabled("Socks_port_edit",   widget->GetValue() ? TRUE : FALSE);
	}
}

void ProxyServersDialog::SetProxyHostPortEntries(const char* host_widget, const char* port_widget, OpString& proxystring)
{
	uni_char* proxy = proxystring.CStr();

	if(proxy == NULL)
		return;

	uni_char *temp = (uni_char*)g_memory_manager->GetTempBuf();
	const uni_char *temp_host = temp;
	uni_char *temp_port = NULL;

	if(uni_strlen(proxy) < g_memory_manager->GetTempBufUniLenUniChar())
	{
		uni_strcpy(temp,proxy);
		temp_port = uni_strchr(temp,']');
		temp_port = uni_strchr(temp_port ? temp_port : temp,':');
		if(temp_port)
			*(temp_port++) = '\0';
	}
	else
		temp_host = proxy;

	SetWidgetText(host_widget, temp_host);
	if(temp_port)
		SetWidgetText(port_widget, temp_port);
}


uni_char* ProxyServersDialog::GetProxyHostPortEntries(const char* host_widget, const char* port_widget, OpString& proxystring)
{
	OpString host, port;
	GetWidgetText(host_widget, host);
	GetWidgetText(port_widget, port);

	if (host.Length())
	{
		proxystring.Reserve(host.Length() + port.Length() + 2);
		proxystring.Set(host);

		uni_char* temp_port = uni_strchr(proxystring.CStr(), ']');
		temp_port = uni_strchr(temp_port ? temp_port : proxystring.CStr(), ':');

		if(port.Length() &&
			(temp_port == NULL ||								// Accept server:port
			(uni_stristr(proxystring.CStr(), UNI_L("://")) && uni_strchr(uni_stristr(proxystring.CStr(), UNI_L("://"))+1, ':') == NULL) )	// Accept protocol://server.port
			)
		{
			proxystring.Append(":");
			proxystring.Append(port);
		}
	}
	return proxystring.CStr();
}


BOOL ProxyServersDialog::OnInputAction(OpInputAction* action)
{
	if (action->GetAction() == OpInputAction::ACTION_GET_ACTION_STATE)
	{
		OpInputAction* child_action = action->GetChildAction();
		if (child_action->GetAction() == OpInputAction::ACTION_MANAGE_PROXY_EXCEPTION_LIST)
		{
			child_action->SetEnabled(GetWidgetValue("manual_proxy_radio"));
			return TRUE;
		}
	}
	else if (action->GetAction() == OpInputAction::ACTION_MANAGE_PROXY_EXCEPTION_LIST)
	{
		ProxyExceptionDialog* dialog = OP_NEW(ProxyExceptionDialog, ());
		if (dialog)
			dialog->Init(this);
		return TRUE;
	}
	return Dialog::OnInputAction(action);
}

void ProxyServersDialog::OnSettingsChanged(DesktopSettings* settings)
{
	Dialog::OnSettingsChanged(settings);

	if (settings->IsChanged(SETTINGS_PROXY))
	{
		OnInit();
	}
}

void ProxyServersDialog::EnableManualConfigurationProxyGroup(BOOL enable)
{
	SetWidgetEnabled("Http_checkbox", enable);
	SetWidgetEnabled("Https_checkbox", enable);
	SetWidgetEnabled("Ftp_checkbox", enable);
	SetWidgetEnabled("Socks_checkbox", enable);

	SetWidgetEnabled("label_for_port_edit", enable);
	SetWidgetEnabled("label_for_ip_edit", enable);
	SetWidgetEnabled("label_for_protocol_edit", enable);

	SetWidgetEnabled("Http_server_edit", GetWidgetValue("Http_checkbox") && enable);
	SetWidgetEnabled("Http_port_edit",   GetWidgetValue("Http_checkbox") && enable);
	SetWidgetEnabled("Use_http_for_all", GetWidgetValue("Http_checkbox") && enable);

	SetWidgetEnabled("Https_server_edit", GetWidgetValue("Https_checkbox") && enable);
	SetWidgetEnabled("Https_port_edit",   GetWidgetValue("Https_checkbox") && enable);

	SetWidgetEnabled("Ftp_server_edit", GetWidgetValue("Ftp_checkbox") && enable);
	SetWidgetEnabled("Ftp_port_edit",   GetWidgetValue("Ftp_checkbox") && enable);

	SetWidgetEnabled("Socks_server_edit", GetWidgetValue("Socks_checkbox") && enable);
	SetWidgetEnabled("Socks_port_edit",   GetWidgetValue("Socks_checkbox") && enable);

	SetWidgetEnabled("Use_proxy_for_local_servers_checkbox", enable);
	SetWidgetEnabled("Exception_list", enable);
}

void ProxyServersDialog::ApplyHTTPProxySettingsToALLProtocols(BOOL value)
{
	if (value)
	{
		OpString address, port;
		GetWidgetText("Http_server_edit", address);
		GetWidgetText("Http_port_edit", port);

		// HTTPS
		SetWidgetValue("Https_checkbox",TRUE);
		SetWidgetText("Https_server_edit",address);
		SetWidgetText("Https_port_edit",port);

		SetWidgetEnabled("Https_checkbox", FALSE);
		SetWidgetEnabled("Https_server_edit", FALSE);
		SetWidgetEnabled("Https_port_edit", FALSE);

		// FTP
		SetWidgetValue("Ftp_checkbox",TRUE);
		SetWidgetText("Ftp_server_edit",address);
		SetWidgetText("Ftp_port_edit",port);

		SetWidgetEnabled("Ftp_checkbox", FALSE);
		SetWidgetEnabled("Ftp_server_edit", FALSE);
		SetWidgetEnabled("Ftp_port_edit", FALSE);

		// Socks
		SetWidgetValue("Socks_checkbox", FALSE);
		SetWidgetEnabled("Socks_checkbox", FALSE);
	}
	else
	{
		SetWidgetValue("Https_checkbox",FALSE);
		SetWidgetValue("Ftp_checkbox",FALSE);
		SetWidgetEnabled("Https_checkbox", TRUE);
		SetWidgetEnabled("Ftp_checkbox", TRUE);
		SetWidgetEnabled("Socks_checkbox", TRUE);
	}
}
