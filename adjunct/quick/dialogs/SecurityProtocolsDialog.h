/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SECURITY_PROTOCOLS_DIALOG_H
#define SECURITY_PROTOCOLS_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"

class SSL_Options;

class SecurityProtocolsDialog : public Dialog
{
	public:

		SecurityProtocolsDialog();
		~SecurityProtocolsDialog();

		Type				GetType()				{return DIALOG_TYPE_SECURITY_PROTOCOLS;}
		const char*			GetWindowName()			{return "Security Protocols Dialog";}
		const char*			GetHelpAnchor()			{return "protocols.html";}

		void				Init(DesktopWindow* parent_window);
		void				OnInit();

		UINT32				OnOk();

	private:
		SSL_Options*		m_options_manager;
		SimpleTreeModel		m_ciphers_model;
#ifdef SSL_2_0_SUPPORT		
		int					m_ssl2_count;
#endif // SSL_2_0_SUPPORT
};


#endif //this file
