/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef PROXY_SERVERS_DIALOG_H
#define PROXY_SERVERS_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"

class ProxyServersDialog : public Dialog
{
	public:

		Type				GetType()				{return DIALOG_TYPE_PROXY_SERVERS;}
		const char*			GetWindowName()			{return "Proxy Servers Dialog";}
		const char*			GetHelpAnchor()			{return "server.html#proxy";}

		void				Init(DesktopWindow* parent_window);
		void				OnInit();

		UINT32				OnOk();
		BOOL				OnInputAction(OpInputAction* action);	
		void				OnChange(OpWidget *widget, BOOL changed_by_mouse);
		void				OnSettingsChanged(DesktopSettings* settings);

	private:
		void				ApplyHTTPProxySettingsToALLProtocols(BOOL value);
		void				EnableManualConfigurationProxyGroup(BOOL enable);

		// Split "localhost:8080" into 2 strings and set them to a host and a port widget respectively
		void				SetProxyHostPortEntries(const char* host_widget, const char* port_widget, OpString& proxystring);
		// Read host name from host_widget, port name from port_widget and concatenate the 2 strings
		uni_char*			GetProxyHostPortEntries(const char* host_widget, const char* port_widget, OpString& proxystring);

};


#endif //this file
