/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * George Refseth, rfz@opera.com
*/

#include "core/pch.h"

#ifdef DU_REMOTE_URL_HANDLER
# include "adjunct/desktop_util/handlers/RemoteURLHandler.h"
# include "adjunct/desktop_util/adt/opsortedvector.h"
# include "modules/hardcore/timer/optimer.h"

class RemoteURLList
{
public:
	class AutoURLString : public OpTimerListener
	{
		void OnTimeOut(OpTimer* timer)
		{
			URLList->Delete(this);
		}

	public:
		OpString m_url_string;
		OpTimer * m_timer;

		AutoURLString() : m_timer(NULL) {}
		virtual ~AutoURLString() { if (m_timer) OP_DELETE(m_timer); }

		bool operator>  (const AutoURLString& item) const
		{
			return m_url_string.Compare(item.m_url_string) > 0;
		}
		bool operator<  (const AutoURLString& item) const
		{
			return m_url_string.Compare(item.m_url_string) < 0;
		}
		bool operator== (const AutoURLString& item) const
		{
			return !m_url_string.Compare(item.m_url_string);
		}
	};
protected:
	static OpAutoSortedVector<AutoURLString> * URLList;
public:
	static void InsertURL(const OpStringC url, time_t timeout);
	
	static BOOL CheckURL(const OpStringC url);

	static void FlushList();
};

/*static*/
OpAutoSortedVector<RemoteURLList::AutoURLString> * RemoteURLList::URLList = NULL;

/*static*/
void RemoteURLList::InsertURL(const OpStringC url, time_t timeout)
{
	if(!timeout)
		timeout = 60000; /* default 6 min */
	if (!URLList)
		URLList = OP_NEW( OpAutoSortedVector<AutoURLString>, ());
	if (URLList)
	{
		AutoURLString * aus = OP_NEW(AutoURLString, ());
		if (!aus)
			return;
		// If url is quoted, remove them TODO: remove this code when quotes are no longer added.
		if (url[0] == '\"' && url[url.Length()-1] == '\"')
			aus->m_url_string.Set(url.CStr() + 1, url.Length() - 2);
		else
			aus->m_url_string.Set(url);
		
		AutoURLString * sim = URLList->FindSimilar(aus);
		if (sim)
		{
			sim->m_timer->Start(timeout);
			OP_DELETE(aus);
		}
		else
		{
			OpTimer * timer = OP_NEW(OpTimer, ());
			timer->SetTimerListener(aus);
			timer->Start(timeout);
			aus->m_timer = timer;
			URLList->Insert(aus);
		}
	}
}

/*static*/
BOOL RemoteURLList::CheckURL(const OpStringC url_string)
{
	if (!URLList)
		return FALSE;
	else
	{
		AutoURLString * aus = OP_NEW(AutoURLString, ());
		aus->m_url_string.Set(url_string);
		BOOL found =  (URLList->FindSimilar(aus) != NULL);
		OP_DELETE(aus);
		return found;
	}	
}

/*static*/
void RemoteURLList::FlushList()
{
	if (URLList)
	{
		OP_DELETE(URLList);
		URLList = NULL;
	}
}

// Namespace implementations
void RemoteURLHandler::AddURLtoList(const OpStringC url, time_t timeout/* = 0*/)
{
	RemoteURLList::InsertURL(url, timeout);
}


BOOL RemoteURLHandler::CheckLoop(OpenURLSetting &setting)
{
	return RemoteURLList::CheckURL(setting.m_address);
}


void RemoteURLHandler::FlushURLList()
{
	RemoteURLList::FlushList();
}
# endif // DU_REMOTE_URL_HANDLER
