/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef SERVER_PROPERTIES_DIALOG_H
#define SERVER_PROPERTIES_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick/dialogs/ServerManagerDialog.h"

#include "adjunct/desktop_util/treemodel/simpletreemodel.h"

#include "modules/inputmanager/inputmanager.h"
#include "modules/url/url_man.h"

/***********************************************************************************
**
**	ServerPropertiesDialog
**
***********************************************************************************/

class ServerPropertiesDialog : public Dialog
{
	public:
		ServerPropertiesDialog(OpStringC& real_server_name, BOOL new_server, BOOL readonly = FALSE, const uni_char *start_tab = NULL);
		virtual					~ServerPropertiesDialog();

		DialogType				GetDialogType()			{return TYPE_PROPERTIES;}
		Type					GetType()				{return DIALOG_TYPE_SERVER_PROPERTIES;}
		const char*				GetWindowName()			{return "Server Properties Dialog";}
		const char*				GetHelpAnchor()			{return "server.html";}

		void					OnInit();
		UINT32					OnOk();
		void			   		OnChange(OpWidget *widget, BOOL changed_by_mouse);
		BOOL					OnInputAction(OpInputAction* action);
		void					OnClick(OpWidget *widget, UINT32 id);

	private:
		enum PageId
		{
			GeneralPage = 0,
			CookiesPage,
			ContentPage,
			DisplayPage,
			ScriptingPage,
			NetworkPage,
			WebHandlerPage
		};

		struct HandlerItem
		{
		public:
			HandlerItem():m_deleted(FALSE) {}
			BOOL m_deleted;
			OpString m_name;
		};

	private:

		Cookie*					GetSelectedCookie(BOOL remove_from_treemodel);
		void					UpdateJavaScriptWidgets();

		OP_STATUS				InitGeneral();
		OP_STATUS				InitCookies();
		OP_STATUS				InitContent();
		OP_STATUS				InitStyle();
		OP_STATUS				InitJavaScript();
		OP_STATUS				InitNetwork();
		OP_STATUS				InitWebHandler();

		OP_STATUS				SaveGeneral();
		OP_STATUS				SaveCookies();
		OP_STATUS				SaveContent();
		OP_STATUS				SaveStyle();
		OP_STATUS				SaveJavaScript();
		OP_STATUS				SaveNetwork();
		OP_STATUS				SaveWebHandler();

		int						FillEncodingDropDown(OpDropDown *encoding_dropdown, const char *menu, BOOL lang_drop_down, const char *match);
		void					ResetEncodingDropDown(OpDropDown *encoding_dropdown);
		OP_STATUS				GetCookieDomain(const OpStringC & server_name, OpString & cookie_domain);

		int 					m_loading_index;

		OpString				m_real_server_name;

		BOOL					m_new_server; ///< this is a new, not edited existing server
		BOOL					m_default_settings; ///< we are editing the default settings

		SimpleTreeModel			m_cookies_model;
		BOOL					m_readonly;	///< not editable when in privacy mode
		OpString				m_start_tab;		// name of tab to focus on start, if any

		BOOL					m_proxy_setting_changed;

		OpAutoVector<HandlerItem> m_handler_list;
};

#endif //SERVER_PROPERTIES_DIALOG_H
