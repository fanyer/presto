/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OP_URL_PLAYER_H
#define OP_URL_PLAYER_H

#include "modules/hardcore/mh/messobj.h" // MessageObject

class OpUrlPlayer :
	public MessageObject
{
public:

	OpUrlPlayer();
	~OpUrlPlayer();

	void Play();

	void LoadUrlList();

private:

	void PlayUrl();

	void StopDelay();

	// Implementing the MessageObject interface
	
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:

	/*
	**
	** UrlList code for handling automated test runs based on a list of URLs from a file
	**
	*/
	class UrlListPlayerEntry : public Link
	{
	public:
		UrlListPlayerEntry(OpString8& url, UINT32 timeout)
			{
				m_url.Set(url.CStr());
				m_timeout = timeout;
			}
		OP_STATUS GetUrl(OpString8& url) { return url.Set(m_url.CStr()); }
		UINT32 GetTimeout() { return m_timeout; }
		
		UrlListPlayerEntry* Suc() const { return static_cast<UrlListPlayerEntry*>(Link::Suc()); }		///< Get next element.
		UrlListPlayerEntry* Pred() const { return static_cast<UrlListPlayerEntry*>(Link::Pred()); }	///< Get previous element.
		
	private:
		OpString8	m_url;			// url to go to
		UINT32		m_timeout;		// timeout defined for this url. If 0, then no timeout
	};
	class UrlListPlayer
	{
	public:
		virtual ~UrlListPlayer()
		{
			UrlListPlayerEntry *entry = GetFirstUrl();
			UrlListPlayerEntry *next;
			
			while(entry)
			{
				next = entry->Suc();
				
				entry->Out();
				OP_DELETE(entry);
				
				entry = next;
			}
		}

		UrlListPlayer()
			: m_is_active(FALSE),
			  m_is_started(FALSE)
		{
			
		}

		OP_STATUS LoadFromFile(OpString& filename, UINT32 default_timeout = 0);
		BOOL IsActive() { return m_is_active; }
		BOOL IsStarted() { return m_is_started; }
		void SetIsStarted(BOOL started) { m_is_started = started; }
		
		UrlListPlayerEntry* GetFirstUrl()
		{
			return static_cast<UrlListPlayerEntry*>(m_entries.First());
		}

		void RemoveEntry(UrlListPlayerEntry* entry)
		{
			entry->Out();
			OP_DELETE(entry);
		}
		
	private:
		Head	m_entries;
		BOOL	m_is_active;		// TRUE when we have entries loaded
		BOOL	m_is_started;		// TRUE after the first URL has been executed
	};

	UrlListPlayer m_url_list_player;
};

#endif // OP_URL_PLAYER_H
