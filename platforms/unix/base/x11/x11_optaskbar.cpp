/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#include "core/pch.h"

#include "x11_optaskbar.h"

#include "adjunct/m2/src/engine/progressinfo.h"
#include "adjunct/m2/src/engine/accountmgr.h"

#ifdef WEBSERVER_SUPPORT
#include "adjunct/quick/hotlist/HotlistModel.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#endif

#ifdef WIDGET_RUNTIME_SUPPORT
#include "adjunct/widgetruntime/GadgetStartup.h"
#endif // WIDGET_RUNTIME_SUPPORT

//static 
OP_STATUS X11OpTaskbar::Create(X11OpTaskbar** taskbar)
{
	*taskbar = OP_NEW(X11OpTaskbar, ());
	if( !*taskbar )
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	else
	{
		OP_STATUS rc = (*taskbar)->Init();
		if( OpStatus::IsError(rc) )
		{
			OP_DELETE(*taskbar);
			return rc;
		}
	}

	return OpStatus::OK;
}


X11OpTaskbar::~X11OpTaskbar()
{
	if (MessageEngine::GetInstance())
	{
		MessageEngine::GetInstance()->RemoveChatListener(this);
		MessageEngine::GetInstance()->GetMasterProgress().RemoveNotificationListener(this);
	}

#ifdef WIDGET_RUNTIME_SUPPORT
	// Do this code with hotlist manager only in case when browser
	// is started (not a gadget, gadget manager or gadget installer)

	if (GadgetStartup::IsBrowserStartupRequested())
	{
#endif // WIDGET_RUNTIME_SUPPORT

#ifdef WEBSERVER_SUPPORT
		HotlistModel* model = g_hotlist_manager->GetUniteServicesModel();
		if(model)
		{
			model->RemoveListener(this);
		}
#endif // WEBSERVER_SUPPORT

#ifdef WIDGET_RUNTIME_SUPPORT
	}
#endif // WIDGET_RUNTIME_SUPPORT
}

OP_STATUS X11OpTaskbar::Init()
{
	m_unite_notification_count = 0;
	m_unread_mail = FALSE;

	if (MessageEngine::GetInstance())
	{
		MessageEngine::GetInstance()->AddChatListener(this);
		MessageEngine::GetInstance()->GetMasterProgress().AddNotificationListener(this);		
	}

#ifdef WIDGET_RUNTIME_SUPPORT
	// Do this code with hotlist manager only in case when browser
	// is started (not a gadget, gadget manager or gadget installer)

	if (GadgetStartup::IsBrowserStartupRequested())
	{
#endif // WIDGET_RUNTIME_SUPPORT

#ifdef WEBSERVER_SUPPORT
		HotlistModel* model = g_hotlist_manager->GetUniteServicesModel();
		if(model)
		{
			model->AddListener(this);
		}
#endif // WEBSERVER_SUPPORT

#ifdef WIDGET_RUNTIME_SUPPORT
	}
#endif // WIDGET_RUNTIME_SUPPORT
	
	return OpStatus::OK;
}


void X11OpTaskbar::OnIndexChanged(UINT32 index_number)
{
	UINT32 count = g_m2_engine ? g_m2_engine->GetUnreadCount() : 0;
	for (OpListenersIterator iterator(m_listener); m_listener.HasNext(iterator);)
	{
		m_listener.GetNext(iterator)->OnUnattendedMailCount(count);
	}
}


void X11OpTaskbar::NeedNewMessagesNotification(Account* account, unsigned count)
{
	if (m_active_mail_windows.GetCount() == 0 && account->GetIncomingProtocol() != AccountTypes::RSS)
	{
		m_unread_mail = TRUE;

		UINT32 count = m_unread_mail ? 1 : 0;
		for (OpListenersIterator iterator(m_listener); m_listener.HasNext(iterator);)
		{
			m_listener.GetNext(iterator)->OnUnreadMailCount(count);
		}
	}
}


void X11OpTaskbar::OnMailViewActivated(MailDesktopWindow* mail_window, BOOL active)
{
	if (active)
	{
		m_unread_mail = FALSE;
		if (m_active_mail_windows.Find(mail_window) == -1)
		{
			m_active_mail_windows.Add(mail_window);
		}
	}
	else
	{
		m_active_mail_windows.RemoveByItem(mail_window);
	}

	UINT32 count = m_unread_mail ? 1 : 0;
	for (OpListenersIterator iterator(m_listener); m_listener.HasNext(iterator);)
	{
		m_listener.GetNext(iterator)->OnUnreadMailCount(count);
	}
}



void X11OpTaskbar::OnUnattendedChatCountChanged(OpWindow* op_window, UINT32 unread)
{
	for (OpListenersIterator iterator(m_listener); m_listener.HasNext(iterator);)
	{
		m_listener.GetNext(iterator)->OnUnattendedChatCount(op_window, unread);
	}
}


#ifdef WEBSERVER_SUPPORT
void X11OpTaskbar::OnItemChanged(OpTreeModel* tree_model, INT32 pos, BOOL sort)
{
	OpTreeModelItem* it = tree_model->GetItemByPosition(pos);
	if(it && it->GetType() == OpTypedObject::UNITE_SERVICE_TYPE)
	{
		UniteServiceModelItem* item = static_cast<UniteServiceModelItem*>(it);
		if(item)
		{
			if(item->GetAttentionStateChanged())
			{
				m_unite_notification_count += item->GetNeedsAttention() ? 1 : -1;

				UINT32 count = m_unread_mail ? 1 : 0;
				for (OpListenersIterator iterator(m_listener); m_listener.HasNext(iterator);)
				{
					m_listener.GetNext(iterator)->OnUniteAttentionCount(count);
				}
			}
		}
	}	
}
#endif
