/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef SERVER_MANAGER_DIALOG_H
#define SERVER_MANAGER_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/url/url_man.h"
#include "modules/wand/wandmanager.h"

class OpTreeView;
class PrefsFile;

/***********************************************************************************
**
**	ServerManagerDialog
**
***********************************************************************************/

class ServerManagerDialog : public Dialog
{
	public:

								ServerManagerDialog(BOOL show_cookies = TRUE, BOOL show_wand = TRUE) : m_server_treeview(NULL) { m_show_cookies = show_cookies; m_show_wand = show_wand; };
		virtual					~ServerManagerDialog();

		DialogType				GetDialogType()			{return TYPE_CLOSE;}
		Type					GetType()				{return DIALOG_TYPE_SERVER_MANAGER;}
		const char*				GetWindowName();
		const char*				GetHelpAnchor()			{return "server.html";}

		void					OnInit();

		BOOL					OnInputAction(OpInputAction* action);
		void					OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
		void					OnChange(OpWidget *widget, BOOL changed_by_mouse);

	private:

		OpTreeView*				m_server_treeview;
		BOOL					m_show_cookies;
		BOOL					m_show_wand;
};

/***********************************************************************************
**
**	ServerItem
**
***********************************************************************************/

class ServerManager;

class ServerItem : public TreeModelItem<ServerItem, ServerManager>
{
	public:
		ServerItem() { m_type = SERVER_TYPE; }
		~ServerItem() { }

		OP_STATUS				GetItemData(ItemData* item_data);
		BOOL 					ContainsText(const OpString& text, BOOL match_folder_name, OpTypedObject::Type child_type);

		static OP_STATUS		AppendCookieLocalDomainIfNeeded(OpString& server_name);

		Type					GetType() { return m_type; }
		int						GetID()	{ return 1; }

		OpTypedObject::Type		m_type;
		OpString				m_cookie_code_tweaked_server_name;
		OpString				m_real_server_name;
		OpString				m_path;
		OpString				m_name;
		OpString				m_value;

		BOOL					m_GetOnThisServer;
};


class ServerManagerListener
{
public:
	virtual ~ServerManagerListener() {}
	virtual void OnCookieRemoved(Cookie* cookie) = 0;
};


class CookieEditorListener
{
public:
	virtual ~CookieEditorListener() {}
	virtual void OnCookieChanged(Cookie* cookie, const uni_char* old_name, const uni_char* old_value ) = 0;
};



/***********************************************************************************
**
**	ServerManager
**
***********************************************************************************/

class ServerManager : public TreeModel<ServerItem>, public WandListener, public CookieEditorListener
{
	public:

								ServerManager();
		virtual					~ServerManager();

		INT32					GetChildCount(INT32 position) {return GetSubtreeSize(position);}
		INT32					GetColumnCount() { return 1; }
		OP_STATUS				GetColumnData(ColumnData* column_data) { return OpStatus::OK; }
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		OP_STATUS				GetTypeString(OpString& type_string) { return OpStatus::ERR; }
#endif

		void 					SetListener( ServerManagerListener* listener) { m_listener = listener; }

		/**
		 * RequireServerManager should be used in pair with ReleaseServerManager
		 * to make sure we delete the global server manager when not in use
		 */
		static void				RequireServerManager();
		static void				ReleaseServerManager();

		// add/change/removal

		/**
		 * Removes site specific preferences and cookies, wand logins associated with server
		 * Typically used to handle ACTION_DELETE on a ServerItem of SERVER_TYPE
		 */
		BOOL					HandleServerDeleteAction(OpTreeView* treeview, BOOL delete_cookies = TRUE, BOOL delete_wand = TRUE);

		Cookie* 				GetCookie(INT32 position);

		BOOL					RemoveCookie(INT32 position);
		BOOL					RemoveWand(INT32 position);
		BOOL					RemoveServerIfEmpty(INT32 position);

		void					OnServerAdded(const uni_char* server_name); ///< used by OnCookieFilterAdded

		void					OnCookieAdded(Cookie* cookie, CookieDomain* domain, CookiePath* path);
		void					OnCookieFilterAdded(ServerName* server_name);
		void					OnCookieRemoved(Cookie* cookie);
		void					OnCookieFilterRemoved(ServerName* server_name);
		void 					OnCookieChanged(Cookie* cookie, const uni_char* old_name, const uni_char* old_value );

		void					OnWandLoginAdded(int index);
		void					OnWandLoginRemoved(int index);
		void					OnWandPageAdded(int index);
		void					OnWandPageRemoved(int index);

	private:
		/**
		 * First simple implementation, Init emulates use of listeners
		 */
		void					Init();
		void					Clear();

		CookieDomain*					m_root_domain;
		ServerManagerListener*			m_listener;

		static int						s_refcounter;
};

extern ServerManager* g_server_manager;

#endif //SERVER_MANAGER_DIALOG_H
