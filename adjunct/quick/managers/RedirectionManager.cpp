// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// shuais
//
#include "core/pch.h"

#include "adjunct/quick/managers/RedirectionManager.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/Application.h"
#include "adjunct/desktop_util/resources/ResourceDefines.h"

#include "modules/url/url_man.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

RedirectUrl::RedirectUrl(OpStringC unique_id)
{
	OpStatus::Ignore(id.Set(unique_id));
	g_main_message_handler->SetCallBack(this, MSG_URL_LOADING_FAILED, 0);
	g_main_message_handler->SetCallBack(this, MSG_HEADER_LOADED, 0);
}

RedirectUrl::~RedirectUrl()
{
	g_main_message_handler->RemoveCallBack(this, MSG_URL_LOADING_FAILED);
	g_main_message_handler->RemoveCallBack(this, MSG_HEADER_LOADED);
}

void RedirectUrl::ResolveUrl(OpStringC url_str)
{
	if (!url.IsEmpty())
		url.Unload();

	url = urlManager->GetURL(url_str, NULL, TRUE);
	if (url.IsEmpty())
		return;

	URL ref_url;
	URL_LoadPolicy policy;
	url.SetHTTP_Method(HTTP_METHOD_HEAD);
	url.Load(g_main_message_handler, ref_url);
}
void RedirectUrl::OnResolved()
{	
	moved_to_url.Set(url.GetAttribute(URL::KHTTP_Location, TRUE));
	
	// Resolve recursively
	if (moved_to_url.HasContent() && RedirectionManager::GetInstance()->IsRedirectUrl(moved_to_url))
		ResolveUrl(moved_to_url);
	else
		OnComplete();
}

void RedirectUrl::OnComplete()
{
	if (moved_to_url.HasContent())
	{
		if (g_hotlist_manager)
		{
			BookmarkModel* model = g_hotlist_manager->GetBookmarksModel();
			if (model)
			{
				HotlistModelItem* item = model->GetByUniqueGUID(id);
				if (item && item->GetPartnerID().HasContent())
				{
					OP_ASSERT(!item->GetHasDisplayUrl());
					item->SetDisplayUrl(moved_to_url);
				}
			}
		}

		if (g_speeddial_manager)
		{
			DesktopSpeedDial* sd = g_speeddial_manager->GetSpeedDialByID(id);
			if (sd && sd->GetPartnerID().HasContent())
			{
				OP_ASSERT(!sd->HasDisplayURL());
				sd->SetDisplayURL(moved_to_url);
			}
		}
	}

	if (RedirectionManager::GetInstance())
		RedirectionManager::GetInstance()->OnResolved(this);
}

void RedirectUrl::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if ((URL_ID)par1 == url.Id())
	{
		switch(msg)
		{
			case MSG_HEADER_LOADED:
				OnResolved();
				break;
			case MSG_URL_LOADING_FAILED:
				OnComplete();
				break;
			default:
				OP_ASSERT(false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////


RedirectionManager::RedirectionManager()
{
	
}

RedirectionManager::~RedirectionManager()
{
	g_speeddial_manager->RemoveListener(*this);
	g_desktop_bookmark_manager->RemoveBookmarkListener(this);
	g_desktop_bookmark_manager->GetBookmarkModel()->RemoveModelListener(this);
}

OP_STATUS RedirectionManager::Init()
{
	g_speeddial_manager->AddListener(*this);
	g_desktop_bookmark_manager->AddBookmarkListener(this);
	return OpStatus::OK; 
}

void RedirectionManager::ResolveRedirectUrl(OpStringC unique_id, OpStringC url)
{
	if (!IsRedirectUrl(url))
		return;

	for (UINT i=0; i<m_urls.GetCount(); i++)
	{
		if (m_urls.Get(i)->id.Compare(unique_id) == 0)
		{
			m_urls.Get(i)->ResolveUrl(url);
			return;
		}
	}

	RedirectUrl* new_url = OP_NEW(RedirectUrl, (unique_id));
	if (new_url)
	{
		OpStatus::Ignore(m_urls.Add(new_url));
		new_url->ResolveUrl(url);
	}
}

void RedirectionManager::OnResolved(RedirectUrl* url)
{
	m_urls.RemoveByItem(url);
	OP_DELETE(url);
}

BOOL RedirectionManager::IsRedirectUrl(OpStringC url)
{
	return url.Find("://redir.opera.com") != KNotFound || url.Find("://redir.operachina.com") != KNotFound;
}

void RedirectionManager::OnBookmarkModelLoaded()
{
	if (g_application->DetermineFirstRunType() == Application::RUNTYPE_FIRST_NEW_BUILD_NUMBER
		|| g_application->DetermineFirstRunType() == Application::RUNTYPE_FIRST
		|| g_application->DetermineFirstRunType() == Application::RUNTYPE_FIRSTCLEAN
		|| g_run_type->m_added_custom_folder
		|| g_region_info->m_changed)
	{
		OpINT32Vector& default_bookmarks = g_desktop_bookmark_manager->GetDefaultBookmarks();

		for (UINT32 i=0; i<default_bookmarks.GetCount(); i++)
		{
			HotlistModelItem* item = g_hotlist_manager->GetItemByID(default_bookmarks.Get(i));
			if (item && item->GetPartnerID().HasContent())
			{
				// On upgrade display url is reset to what is in the new file,
				// so this is TRUE if there was no display url set there
				if (!item->GetHasDisplayUrl())
				{
					// What the redirect url points to may have changed after an upgrade
					ResolveRedirectUrl(item->GetUniqueGUID(), item->GetUrl());
				}
			}
		}
	}

	g_desktop_bookmark_manager->GetBookmarkModel()->AddModelListener(this);
}

void RedirectionManager::OnHotlistItemAdded(HotlistModelItem* item)
{
	if (item->GetPartnerID().HasContent() && !item->GetHasDisplayUrl())
		ResolveRedirectUrl(item->GetUniqueGUID(), item->GetUrl());
}

void RedirectionManager::OnSpeedDialAdded(const DesktopSpeedDial& entry)
{
	if (entry.GetPartnerID().HasContent() && !entry.HasDisplayURL())
		ResolveRedirectUrl(entry.GetUniqueID(), entry.GetURL());
}

void RedirectionManager::OnSpeedDialsLoaded()
{
	if (g_application->DetermineFirstRunType() == Application::RUNTYPE_FIRST_NEW_BUILD_NUMBER
		|| g_application->DetermineFirstRunType() == Application::RUNTYPE_FIRST
		|| g_application->DetermineFirstRunType() == Application::RUNTYPE_FIRSTCLEAN
		|| g_run_type->m_added_custom_folder
		|| g_region_info->m_changed)
	{
		UINT count = g_speeddial_manager->GetTotalCount();
		for (UINT i=0; i<count; i++)
		{
			DesktopSpeedDial* sd = g_speeddial_manager->GetSpeedDial(i);
			if (sd->GetPartnerID().HasContent())
			{
				// What the redirect url points to may have changed after an upgrade
				sd->SetDisplayURL(NULL);
				ResolveRedirectUrl(sd->GetUniqueID(), sd->GetURL());
			}
		}
	}
}
