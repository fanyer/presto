/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/dialogs/ServerManagerDialog.h"
#include "adjunct/quick/dialogs/CookieEditDialog.h"
#include "adjunct/quick/dialogs/ServerPropertiesDialog.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick_toolkit/widgets/OpQuickFind.h"
#include "adjunct/quick/managers/PrivacyManager.h"

#include "modules/widgets/WidgetContainer.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/util/opstrlst.h"
#include "modules/cookies/url_cm.h"
#include "modules/hardcore/mem/mem_man.h"

ServerManager* g_server_manager = NULL;
int ServerManager::s_refcounter = 0;

ServerManagerDialog::~ServerManagerDialog()
{
	ServerManager::ReleaseServerManager();
}

/***********************************************************************************
**
**	OnInit
**
***********************************************************************************/

void ServerManagerDialog::OnInit()
{
	m_server_treeview = (OpTreeView*) GetWidgetByName("Server_treeview");
	OpQuickFind* quickfind = (OpQuickFind*) GetWidgetByName("Quickfind_edit");

	m_server_treeview->SetAutoMatch(FALSE/*TRUE*/);
	m_server_treeview->SetShowThreadImage(TRUE);
	m_server_treeview->SetShowColumnHeaders(FALSE);
	m_server_treeview->SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_EDIT_PROPERTIES)));

	quickfind->SetTarget(m_server_treeview);

	// ?
	TRAPD(op_err, g_url_api->CheckCookiesReadL());
	OpStatus::Ignore(op_err);

	ServerManager::RequireServerManager();

	m_server_treeview->SetTreeModel(g_server_manager, 0);
	m_server_treeview->SetMatchType(m_show_cookies && m_show_wand ? MATCH_ALL : (m_show_cookies ? MATCH_IMPORTANT : (m_show_wand ? MATCH_STANDARD : MATCH_FOLDERS)), TRUE);
}

const char*	ServerManagerDialog::GetWindowName()
{
	if (m_show_cookies)
		return "Cookie Manager Dialog";

	if (m_show_wand)
		return "Wand Manager Dialog";

	return "Server Manager Dialog";
}

/***********************************************************************************
**
**	OnMouseEvent
**
***********************************************************************************/

void ServerManagerDialog::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{

}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL ServerManagerDialog::OnInputAction(OpInputAction* action)
{
	if (!m_server_treeview)
		return FALSE;

	if (m_server_treeview->OnInputAction(action))
		return TRUE;

	ServerItem* item = (ServerItem*) m_server_treeview->GetSelectedItem();
	INT32 model_pos = m_server_treeview->GetSelectedItemModelPos();


	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_NEW_SERVER:
					child_action->SetEnabled(!m_show_cookies && !m_show_wand);
					return TRUE;
				case OpInputAction::ACTION_EDIT_PROPERTIES:
					child_action->SetEnabled( item && ((item->m_type == OpTypedObject::SERVER_TYPE && !m_show_cookies && !m_show_wand) || item->m_type == OpTypedObject::COOKIE_TYPE) );
					return TRUE;
				case OpInputAction::ACTION_DELETE:
					child_action->SetEnabled( item != NULL  && item->m_real_server_name.Compare("*.*"));
					return TRUE;
			}
			break;
		}
		case OpInputAction::ACTION_DELETE:
		{
			if (item && item->m_type == OpTypedObject::COOKIE_TYPE)
			{
				g_server_manager->RemoveCookie(model_pos);
			}
			else if (item && item->m_type == OpTypedObject::WAND_TYPE)
			{
				g_server_manager->RemoveWand(model_pos);
			}
			else if (item && item->m_type == OpTypedObject::SERVER_TYPE)
			{
				BOOL delete_cookies = m_show_cookies;
				BOOL delete_wand = m_show_wand;

				return g_server_manager->HandleServerDeleteAction(m_server_treeview, delete_cookies, delete_wand);
			}

			return TRUE;
		}
		case OpInputAction::ACTION_NEW_SERVER:
		{
			OpString empty;
			ServerPropertiesDialog* dialog = OP_NEW(ServerPropertiesDialog, (empty,TRUE));
			if (dialog)
				dialog->Init(this);

			return TRUE;
		}
		case OpInputAction::ACTION_EDIT_PROPERTIES:
		{
			if (item && item->m_real_server_name.HasContent() && item->m_type == OpTypedObject::SERVER_TYPE && !m_show_cookies && !m_show_wand )
			{
				ServerPropertiesDialog* dialog = OP_NEW(ServerPropertiesDialog, (item->m_real_server_name, FALSE));
				if (dialog)
					dialog->Init(this);
			}
			else if( item && item->m_type == OpTypedObject::COOKIE_TYPE )
			{
				Cookie* cookie = g_server_manager->GetCookie(model_pos);
				if(cookie)
				{
					CookieEditDialog* dialog = OP_NEW(CookieEditDialog, (item->m_cookie_code_tweaked_server_name.CStr(), cookie));
					if (dialog)
						dialog->Init(this);
				}
			}
			return TRUE;
		}
	}

	return Dialog::OnInputAction(action);
}


void ServerManagerDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	Dialog::OnChange(widget, changed_by_mouse);
}


/***********************************************************************************
**
**	ServerManager
**
***********************************************************************************/

ServerManager::ServerManager()
{
	m_root_domain = NULL;
	m_listener = NULL;
}

ServerManager::~ServerManager()
{
	Clear();
}



void ServerManager::Init()
{
	// add default settings site

// Removed for now, trying separate button in prefs instead (johan)
//	OnServerAdded(UNI_L("*.*"));

	// add sites with cookie prefs overrides

	ServerName *current_server_name;

	current_server_name = g_url_api->GetFirstServerName();

	while(current_server_name)
	{
		OnCookieFilterAdded(current_server_name);
		current_server_name = g_url_api->GetNextServerName();
	}

	// add sites with cookies

#if defined(QUICK_COOKIE_EDITOR_SUPPORT)
	g_url_api->BuildCookieEditorListL();
#endif

	// add sites with wand data

	WandLogin* login = g_wand_manager->GetWandLogin(0);
	int i;

	for (i = 0; login; i++)
	{
		OnWandLoginAdded(i);
		login = g_wand_manager->GetWandLogin(i+1);
	}

	WandPage* page = g_wand_manager->GetWandPage(0);

	for (i = 0; page; i++)
	{
		OnWandPageAdded(i);
		page = g_wand_manager->GetWandPage(i+1);
	}

	g_wand_manager->AddListener(this);

	// add sites with regular prefs overrides

	OpString_list *hosts = g_prefsManager->GetOverriddenHostsL();

	if (hosts)
	{
		int count = hosts->Count();
		for (i = 0; i < count; i++)
		{
			OpString host_name = hosts->Item(i);
			if (g_prefsManager->IsHostOverridden(host_name.CStr(), FALSE) == HostOverrideActive)
			{
				OnServerAdded(host_name.CStr());
			}
		}
		OP_DELETE(hosts);
	}
}

void ServerManager::Clear()
{
	ServerName *current_server_name;

	current_server_name = g_url_api->GetFirstServerName();

	while(current_server_name)
	{
		OnCookieFilterRemoved(current_server_name);
		current_server_name = g_url_api->GetNextServerName();
	}

	DeleteAll();

	g_wand_manager->RemoveListener(this);
}


void ServerManager::RequireServerManager()
{
	s_refcounter++;

	if (g_server_manager == NULL)
	{
		OP_ASSERT(s_refcounter == 1);

		if (0 != (g_server_manager = OP_NEW(ServerManager, ())))
			g_server_manager->Init();
	}
}


void ServerManager::ReleaseServerManager()
{
	s_refcounter--;
	if (s_refcounter == 0 && g_server_manager)
	{
		OP_DELETE(g_server_manager);
		g_server_manager = NULL;
	}
}


BOOL ServerManager::HandleServerDeleteAction(OpTreeView* treeview, BOOL delete_cookies, BOOL delete_wand)
{
	if (treeview == NULL)
		return FALSE;

	ServerItem* item = (ServerItem*)treeview->GetSelectedItem();

	if (item == NULL || item->m_real_server_name.Compare("*.*") == 0)
		return FALSE;

	INT32 model_pos = treeview->GetSelectedItemModelPos();

	ServerName* server_name = g_url_api->GetServerName(item->m_real_server_name.CStr(), FALSE);

	if (server_name && (delete_cookies || !delete_wand))
	{
		server_name->SetAcceptCookies(COOKIE_DEFAULT);
	}

	item->Change();

	int child_count = GetChildCount(model_pos);

	for (int i = 1; i <= child_count; i++)
	{
		ServerItem* child = (ServerItem*)GetItemByPosition(model_pos+1);

		if (child && child->m_type == OpTypedObject::COOKIE_TYPE && delete_cookies)
		{
			RemoveCookie(model_pos+1);
		}
		else if (child && child->m_type == OpTypedObject::WAND_TYPE && delete_wand)
		{
			RemoveWand(model_pos+1);
		}
	}

	if (item == treeview->GetSelectedItem())
	{
		// not yet deleted by removing children

		if (!delete_cookies && !delete_wand)
		{
			if (!g_prefsManager->RemoveOverridesL(item->m_real_server_name.CStr(), TRUE))
			{
				// OP_ASSERT(!"Possibly a site with cookie overrides but no prefs overrides and no cookies?");
			}
		}
		RemoveServerIfEmpty(model_pos);
	}

	return TRUE;
}


Cookie* ServerManager::GetCookie(INT32 position)
{
	if (m_root_domain == NULL)
		return 0;

	ServerItem* item = (ServerItem*)GetItemByPosition(position);
	if (item == NULL || item->m_type != OpTypedObject::COOKIE_TYPE)
		return NULL;

	CookieDomain* lowest_domain = 0;
	BOOL is_full_path = FALSE;
	CookiePath* cp = NULL;

	OpString8 domain, path, name;
	domain.Set(item->m_cookie_code_tweaked_server_name.CStr()); // item is of COOKIE_TYPE
	path.Set(item->m_path.CStr());
	name.Set(item->m_name.CStr());

	TRAPD(err, cp = m_root_domain->GetCookiePathL(domain.CStr(), path.CStr(),
		NULL,
		FALSE, lowest_domain, is_full_path));

	if (err != OpStatus::OK)
		return 0;

	if (cp)
	{
		Cookie_Item_Handler handler;

		handler.name.Set(name.CStr());

		Cookie* cookie = cp->GetCookieL(&handler, FALSE);

		handler.name.Empty();

		return cookie;
	}

	return 0;
}


BOOL ServerManager::RemoveCookie(INT32 position)
{
	if (m_root_domain == NULL)
		return FALSE;

	ServerItem* item = (ServerItem*)GetItemByPosition(position);
	if (item == NULL || item->m_type != OpTypedObject::COOKIE_TYPE)
		return FALSE;

	CookieDomain* lowest_domain = 0;
	BOOL is_full_path = FALSE;
	CookiePath* cp = NULL;

	OpString8 domain, path, name;
	domain.Set(item->m_cookie_code_tweaked_server_name.CStr()); // item is of COOKIE_TYPE
	path.Set(item->m_path.CStr());
	name.Set(item->m_name.CStr());

	TRAPD(err, cp = m_root_domain->GetCookiePathL(domain.CStr(), path.CStr(),
		NULL,
		FALSE, lowest_domain, is_full_path));

	if (err != OpStatus::OK)
		return FALSE;

	if (cp)
	{
		Cookie_Item_Handler handler;

		handler.name.Set(name.CStr());

		Cookie* cookie = cp->GetCookieL(&handler, FALSE);
		if (cookie)
		{
			cookie->Out();
			OP_DELETE(cookie);
		}

		handler.name.Empty();
	}

	return TRUE;
}


void ServerManager::OnCookieAdded(Cookie* cookie, CookieDomain* domain, CookiePath* path)
{
	ServerItem* item = OP_NEW(ServerItem, ());
	if (!item)
		return;

	item->m_type = OpTypedObject::COOKIE_TYPE;

	if (domain)
	{
		OpString8 buf;
		if (buf.Reserve(512))
		{
			domain->GetFullDomain(buf.CStr(), 512, FALSE);
			item->m_cookie_code_tweaked_server_name.Set(buf); // item is of COOKIE_TYPE

			buf.Empty();
			if (buf.Reserve(512))
			{
				domain->GetFullDomain(buf.CStr(), 512, TRUE);
				item->m_real_server_name.Set(buf);
			}
		}
	}

	while (domain)
	{
		domain = domain->Parent();
		if (domain)
		{
			m_root_domain = domain;
		}
	}

	while (path)
	{
		if (path->PathPart().HasContent())
		{
			item->m_path.Insert(0, path->PathPart());

			if (path->Parent() && path->Parent()->PathPart().HasContent())
				item->m_path.Insert(0, UNI_L("/"));
		}
		path = path->Parent();
	}

	item->m_name.Set(cookie->Name());
	item->m_value.Set(cookie->Value());

	// find or create a parent server:

	int parent = -1;
	int got_index;
	int i;

	for (i = 0; i < GetItemCount(); i++)
	{
		ServerItem* server = GetItemByIndex(i);

		if (server && server->GetType() == OpTypedObject::SERVER_TYPE &&
			server->m_real_server_name.HasContent() && server->m_real_server_name.Compare(item->m_real_server_name) == 0)
		{
			parent = i;
		}
	}
	if (parent == -1)
	{
		ServerItem* server = OP_NEW(ServerItem, ());
		if (server)
		{
			server->m_real_server_name.Set(item->m_real_server_name);
			server->m_cookie_code_tweaked_server_name.Set(item->m_real_server_name); // the same for a server

			got_index = AddLast(server);

			parent = got_index;
		}
	}
	else
	{
		// avoid duplicates
		int size = GetChildCount(parent);
		for (i = 1; i <= size; i++)
		{
			ServerItem* child = GetItemByIndex(parent+i);
			if (child && child->m_name.Compare(item->m_name) == 0 &&
				child->m_path.Compare(item->m_path) == 0)
			{
				OP_DELETE(item);
				return;
			}
		}
	}

	AddLast(item, parent);
}

void ServerManager::OnCookieFilterAdded(ServerName* server_name)
{
	if (server_name->GetAcceptCookies() == COOKIE_DEFAULT)
		return;

	OnServerAdded(server_name->UniName());
}

BOOL ServerManager::RemoveServerIfEmpty(INT32 position)
{
	ServerItem* item = (ServerItem*)GetItemByPosition(position);
	if (item == NULL || item->m_type != OpTypedObject::SERVER_TYPE)
		return FALSE;

	if (GetChildCount(position) != 0)
	{
		BroadcastItemChanged(position);
		return FALSE;
	}

	BOOL use_default = TRUE;

	ServerName* server_name = g_url_api->GetServerName(item->m_real_server_name.CStr(), FALSE);

	if (server_name)
	{
		use_default = (server_name->GetAcceptCookies() == COOKIE_DEFAULT);
	}

	// check for host overrides too

	if (g_prefsManager->IsHostOverridden(item->m_real_server_name.CStr(), TRUE) == HostOverrideActive)
	{
		BroadcastItemChanged(position);
		return FALSE;
	}

	if (use_default)
	{
		Delete(position);
		return TRUE;
	}

	BroadcastItemChanged(position);
	return FALSE;
}


void ServerManager::OnServerAdded(const uni_char* server_name)
{
	if(!server_name || !(*server_name))
		return;

	for (int i = 0; i < GetItemCount(); i++)
	{
		ServerItem* item = GetItemByIndex(i);
		if (item &&
			item->m_type == OpTypedObject::SERVER_TYPE &&
			item->m_real_server_name.Compare(server_name) == 0)
		{
			return;
		}
	}

	ServerItem* item = OP_NEW(ServerItem, ());
	if (item)
	{
		item->m_real_server_name.Set(server_name);
		item->m_cookie_code_tweaked_server_name.Set(server_name); // the same for a server

		AddLast(item);
	}
}


void ServerManager::OnCookieRemoved(Cookie* cookie)
{
	for (int i = 0; i < GetItemCount(); i++)
	{
		ServerItem* item = GetItemByIndex(i);
		if (item &&
			item->m_name.Compare(cookie->Name()) == 0 &&
			item->m_value.Compare(cookie->Value()) == 0)
		{
			int parent = GetItemParent(i);

			Delete(i);

			RemoveServerIfEmpty(parent);

			if( m_listener )
			{
				m_listener->OnCookieRemoved(cookie);
			}
		}
	}
}

void ServerManager::OnCookieFilterRemoved(ServerName* server_name)
{
	for (int i = 0; i < GetItemCount(); i++)
	{
		ServerItem* item = GetItemByIndex(i);

		if (item && item->GetType() == OpTypedObject::SERVER_TYPE &&
			item->m_real_server_name.HasContent() && item->m_real_server_name.Compare(server_name->UniName()) == 0)
		{
			RemoveServerIfEmpty(i);
		}
	}
}


void ServerManager::OnCookieChanged(Cookie* cookie, const uni_char* old_name, const uni_char* old_value)
{
	for (int i = 0; i < GetItemCount(); i++)
	{
		ServerItem* item = GetItemByIndex(i);
		if (item && item->m_name.Compare(old_name) == 0 && item->m_value.Compare(old_value) == 0)
		{
			item->m_name.Set(cookie->Name());
			item->m_value.Set(cookie->Value());
			BroadcastItemChanged(i);
			break;
		}
	}
}



void ServerManager::OnWandLoginAdded(int index)
{
	WandLogin* login = g_wand_manager->GetWandLogin(index);

	// Don't add any of the new opera:x stuff for link,mail etc
	// Lokk for opera: at the start of the string
	if (login->id.Find(WAND_OPERA_PREFIX) == 0)
		return;

	ServerItem* item = OP_NEW(ServerItem, ());
	if (!item)
		return;

	item->m_type = OpTypedObject::WAND_TYPE;
	item->m_name.Set(login->username);
	item->m_path.Set(login->id);
	item->m_GetOnThisServer = FALSE;

	item->m_real_server_name.Set(login->id);

	int pos = item->m_real_server_name.Find(UNI_L("://"));
	if (pos != KNotFound)
	{
		item->m_real_server_name.Set((item->m_real_server_name.CStr()+pos+3));
	}
	pos = item->m_real_server_name.Find(UNI_L("/"));
	if (pos != KNotFound)
	{
		item->m_real_server_name[pos] = 0;
	}

	item->m_cookie_code_tweaked_server_name.Set(item->m_real_server_name); // the same for a server

	// find or create a parent server:

	int parent = -1;
	int got_index;

	for (int i = 0; i < GetItemCount(); i++)
	{
		ServerItem* server = GetItemByIndex(i);

		if (server && server->GetType() == OpTypedObject::SERVER_TYPE &&
			server->m_real_server_name.HasContent() && server->m_real_server_name.Compare(item->m_real_server_name) == 0)
		{
			parent = i;
		}
	}
	if (parent == -1)
	{
		ServerItem* server = OP_NEW(ServerItem, ());
		if (server)
		{
			server->m_real_server_name.Set(item->m_real_server_name);
			server->m_cookie_code_tweaked_server_name.Set(item->m_real_server_name); // the same for a server

			got_index = AddLast(server);

			parent = got_index;
		}
	}

	AddLast(item, parent);
}

void ServerManager::OnWandLoginRemoved(int index)
{
	WandLogin* login = g_wand_manager->GetWandLogin(index);

	for (int i = 0; i < GetItemCount(); i++)
	{
		ServerItem* item = GetItemByIndex(i);
		if (item &&
			item->m_name.Compare(login->username) == 0 &&
			item->m_path.Compare(login->id) == 0)
		{
			int parent = GetItemParent(i);

			Delete(i);

			RemoveServerIfEmpty(parent);
		}
	}
}


void ServerManager::OnWandPageAdded(int index)
{
	WandPage* page = g_wand_manager->GetWandPage(index);
	const uni_char* username;
	ServerItem* item = OP_NEW(ServerItem, ());
	if (!item)
		return;

	item->m_type = OpTypedObject::WAND_TYPE;
	if (page->FindUserName(username) == WandPage::FIND_USER_OK)
		item->m_name.Set(username);
	item->m_path.Set(page->GetUrl());
	item->m_GetOnThisServer = page->GetOnThisServer();

	item->m_real_server_name.Set(item->m_path);

	int pos = item->m_real_server_name.Find(UNI_L("://"));
	if (pos != KNotFound)
	{
		item->m_real_server_name.Set((item->m_real_server_name.CStr()+pos+3));
	}
	pos = item->m_real_server_name.Find(UNI_L("/"));
	if (pos != KNotFound)
	{
		item->m_real_server_name[pos] = 0;
	}

	item->m_cookie_code_tweaked_server_name.Set(item->m_real_server_name); // the same for a server

	// find or create a parent server:

	int parent = -1;
	int got_index;

	for (int i = 0; i < GetItemCount(); i++)
	{
		ServerItem* server = GetItemByIndex(i);

		if (server && server->GetType() == OpTypedObject::SERVER_TYPE &&
			server->m_real_server_name.HasContent() && server->m_real_server_name.Compare(item->m_real_server_name) == 0)
		{
			parent = i;
		}
	}
	if (parent == -1)
	{
		ServerItem* server = OP_NEW(ServerItem, ());
		if (server)
		{
			server->m_real_server_name.Set(item->m_real_server_name);
			server->m_cookie_code_tweaked_server_name.Set(item->m_real_server_name); // the same for a server

			got_index = AddLast(server);

			parent = got_index;
		}
	}

	AddLast(item, parent);
}


void ServerManager::OnWandPageRemoved(int index)
{
	WandPage* page = g_wand_manager->GetWandPage(index);

	for (int i = 0; i < GetItemCount(); i++)
	{
		ServerItem* item = GetItemByIndex(i);
		const uni_char* username;

		WandPage::FindUserNameResult result = page->FindUserName(username);

		if (item &&
			((result == WandPage::FIND_USER_OK && item->m_name.Compare(username) == 0) ||
			result == WandPage::FIND_USER_ERR_NOTHING_STORED_ON_SERVER ||
			result == WandPage::FIND_USER_ERR_NOTHING_STORED_ON_PAGE ||
			result == WandPage::FIND_USER_ERR_NO_USERNAME) &&
			item->m_path.Compare(page->GetUrl()) == 0)
		{
			int parent = GetItemParent(i);

			Delete(i);

			RemoveServerIfEmpty(parent);
		}
	}
}


BOOL ServerManager::RemoveWand(INT32 position)
{
	ServerItem* item = (ServerItem*)GetItemByPosition(position);
	if (item == NULL || item->m_type != OpTypedObject::WAND_TYPE)
		return FALSE;

	WandLogin* login = g_wand_manager->GetWandLogin(0);
	int i;

	for (i = 0; login; i++)
	{
		if (item->m_name.Compare(login->username) == 0 &&
			item->m_path.Compare(login->id) == 0)
		{
			g_wand_manager->DeleteLogin(login->id.CStr(), login->username.CStr());
			return TRUE;
		}

		login = g_wand_manager->GetWandLogin(i+1);
	}

	WandPage* page = g_wand_manager->GetWandPage(0);

	for (i = 0; page; i++)
	{
		const uni_char* username;

		WandPage::FindUserNameResult result = page->FindUserName(username);

		if (((result == WandPage::FIND_USER_OK && item->m_name.Compare(username) == 0) || 
				result == WandPage::FIND_USER_ERR_NOTHING_STORED_ON_SERVER ||
				result == WandPage::FIND_USER_ERR_NOTHING_STORED_ON_PAGE ||
				result == WandPage::FIND_USER_ERR_NO_USERNAME) 
			&& item->m_path.Compare(page->GetUrl()) == 0)
		{
			g_wand_manager->DeleteWandPage(i);
			return TRUE;
		}
		page = g_wand_manager->GetWandPage(i+1);
	}

	return TRUE;
}


OP_STATUS ServerItem::GetItemData(ItemData* item_data)
{
	INT32 model_pos = GetIndex();

	if (item_data->query_type == MATCH_QUERY)
	{
		BOOL match = FALSE;

		if (item_data->match_query_data.match_type == MATCH_IMPORTANT)
		{
			match = FALSE;

			if (m_type == COOKIE_TYPE)
			{
				match = TRUE;
			}
			else if (m_type == SERVER_TYPE)
			{
				// true if cookie children
				int children = g_server_manager->GetChildCount(model_pos);

				for (int i = 1; i <= children; i++)
				{
					ServerItem* item = (ServerItem*)g_server_manager->GetItemByPosition(model_pos + i);
					if (item && item->m_type == COOKIE_TYPE)
					{
						match = TRUE;
						break;
					}
				}

				if (!match)
				{
					ServerName* server_name = g_url_api->GetServerName(m_real_server_name.CStr(), FALSE);

					if (server_name)
					{
						match = server_name->GetAcceptCookies() != COOKIE_DEFAULT;
					}
				}
			}

			if( match && item_data->match_query_data.match_text && item_data->match_query_data.match_text->CStr() )
			{
				match = ContainsText(*item_data->match_query_data.match_text, FALSE, COOKIE_TYPE) || (m_type == SERVER_TYPE && ContainsText(*item_data->match_query_data.match_text, TRUE, COOKIE_TYPE));
			}
		}
		else if (item_data->match_query_data.match_type == MATCH_STANDARD)
		{
			match = FALSE;

			if (m_type == WAND_TYPE)
			{
				match = TRUE;
			}
			else if (m_type == SERVER_TYPE)
			{
				// true if wand children
				int children = g_server_manager->GetChildCount(model_pos);

				for (int i = 1; i <= children; i++)
				{
					ServerItem* item = (ServerItem*)g_server_manager->GetItemByPosition(model_pos + i);
					if (item && item->m_type == WAND_TYPE)
					{
						match = TRUE;
						break;
					}
				}
			}

			if( match && item_data->match_query_data.match_text && item_data->match_query_data.match_text->CStr() )
			{
				match = ContainsText(*item_data->match_query_data.match_text, FALSE, WAND_TYPE);
			}
		}
		else if (item_data->match_query_data.match_type == MATCH_FOLDERS)
		{
			if (m_type == SERVER_TYPE)
			{
				match = FALSE;

				ServerName* server_name = g_url_api->GetServerName(m_real_server_name.CStr(), FALSE);

			    if (server_name)
				{
					match = !(server_name->GetAcceptCookies() == COOKIE_DEFAULT);
				}

				match = match || (g_prefsManager->IsHostOverridden(m_real_server_name.CStr(), FALSE) == HostOverrideActive);
				match = match || m_real_server_name.CompareI("*.*") == 0;

				if( match && item_data->match_query_data.match_text && item_data->match_query_data.match_text->CStr() )
				{
					match = ContainsText(*item_data->match_query_data.match_text, TRUE, UNKNOWN_TYPE);
				}
			}
		}
		else if (item_data->match_query_data.match_type == MATCH_ALL)
		{
			match = TRUE;

			if( item_data->match_query_data.match_text && item_data->match_query_data.match_text->CStr() )
			{
				match = ContainsText(*item_data->match_query_data.match_text, FALSE, UNKNOWN_TYPE);
			}
		}

		if (match)
		{
			item_data->flags |= FLAG_MATCHED;
		}
	}

	if (item_data->query_type == INFO_QUERY && m_type == WAND_TYPE)
	{
		item_data->info_query_data.info_text->AddTooltipText(UNI_L(""), m_path.CStr());
	}

	if (item_data->query_type != COLUMN_QUERY)
	{
		return OpStatus::OK;
	}

	// Keep in sync with ServerItem::ContainsText()
	OpString *coltxt = item_data->column_query_data.column_text;
	if (m_type == SERVER_TYPE)
	{
		if (m_real_server_name.CompareI("*.*") == 0)
		{
			coltxt->Set(UNI_L("Default settings") /*Should normally not be visible from UI*/);
			item_data->column_query_data.column_sort_order = 1;
			item_data->flags |= FLAG_BOLD;
		}
		else
		{
			coltxt->Set(m_real_server_name);
			item_data->column_query_data.column_sort_order = 2;
		}

		// check if necessary to get the image
		if (!(item_data->flags & FLAG_NO_PAINT))
		{
			OpString icon_url;
			icon_url.Set("http://");
			icon_url.Append(m_real_server_name);
			
			item_data->column_bitmap = g_favicon_manager->Get(icon_url.CStr());
			if ( item_data->column_bitmap.IsEmpty() && icon_url.FindI("www") == KNotFound)
			{
				icon_url.Insert(7, "www.");
				item_data->column_bitmap = g_favicon_manager->Get(icon_url.CStr());
			}

			if ( item_data->column_bitmap.IsEmpty() )
			{
				item_data->column_query_data.column_image = "Folder";
			}
		}
	}
	else if (m_type == COOKIE_TYPE)
	{
		coltxt->Set(m_path);
		coltxt->Append(" ");
		coltxt->Append(m_name);
		coltxt->Append(": ");
		coltxt->Append(m_value);
		item_data->column_query_data.column_image = "Attachment Documents";
	}
	else	//WAND_TYPE
	{
		if (m_GetOnThisServer)
			coltxt->Set("*");
		coltxt->Append(m_path.CStr(),35);
		if (m_path.Length() > 35)
			coltxt->Append("...");
		coltxt->Append(" ");
		coltxt->Append(m_name);
		item_data->column_query_data.column_image = "Bookmark Visited";
	}
	
	return OpStatus::OK;
}



// Keep in sync with ServerItem::GetItemData()
BOOL ServerItem::ContainsText(const OpString& text, BOOL match_folder_name, OpTypedObject::Type child_type)
{
	if( !text.CStr() )
		return FALSE;

	if( m_type == COOKIE_TYPE )
	{
		return (m_path.CStr() && uni_stristr(m_path.CStr(), text.CStr())) ||
			(m_name.CStr() && uni_stristr(m_name.CStr(), text.CStr())) ||
			(m_real_server_name.CStr() && uni_stristr(m_real_server_name.CStr(), text.CStr())) ||
			(m_value.CStr() && uni_stristr(m_value.CStr(), text.CStr()));
	}
	else if( m_type == WAND_TYPE )
	{
		return (m_path.CStr() && uni_stristr(m_path.CStr(), text.CStr())) ||
			(m_real_server_name.CStr() && uni_stristr(m_real_server_name.CStr(), text.CStr())) ||
			(m_name.CStr() && uni_stristr(m_name.CStr(), text.CStr()));
	}
	else if (m_type == SERVER_TYPE)
	{
		if( match_folder_name )
		{
			return m_real_server_name.CStr() && uni_stristr(m_real_server_name.CStr(), text.CStr());
		}
		else
		{
			INT32 model_pos = GetIndex();

			INT32 children = g_server_manager->GetChildCount(model_pos);
			for (INT32 i = 1; i <= children; i++)
			{
				ServerItem* item = (ServerItem*)g_server_manager->GetItemByPosition(model_pos + i);
				if (item && (child_type == UNKNOWN_TYPE || item->m_type == child_type))
				{
					if (item->ContainsText(text, FALSE, child_type) )
					{
						return TRUE;
					}
				}
			}
		}
	}

	return FALSE;
}
