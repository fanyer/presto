/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#include "core/pch.h"

#ifdef HAS_ATVEF_SUPPORT

#include "modules/display/tvmanager.h"
#include "modules/url/url2.h"

/** Container element used in the list of listener. */
class TvListenerContainer
	: public Link
{
public:
	/** Create a container.
	  * @param listener The object to encapsulate.
	  */
	TvListenerContainer(OpTvWindowListener *listener)
		: Link(), m_listener(listener) {};
	/** Return the encapsulated pointer as the link id. */
	virtual const char* LinkId() { return reinterpret_cast<const char *>(m_listener); }
	/** Retrieve the encapsulated pointer. */
	inline OpTvWindowListener *GetListener() { return m_listener; }

private:
	OpTvWindowListener *m_listener; ///< The encapsulated object
};

TvManager::TvManager()
		: chroma_red(0),
		  chroma_green(0),
		  chroma_blue(0),
		  chroma_alpha(0)
{}

TvManager::~TvManager()
{
	// Are there listeners still registered?
	m_listeners.Clear();
}

void TvManager::SetDisplayRect(const URL *url, const OpRect& tvRect)
{
	const uni_char *urlstring =
		url ? url->GetAttribute(URL::KUniName, FALSE).CStr() : NULL;
	TvListenerContainer *p =
		static_cast<TvListenerContainer *>(m_listeners.First());
	while (p)
	{
		p->GetListener()->SetDisplayRect(urlstring, tvRect);
		p = static_cast<TvListenerContainer *>(p->Suc());
	}
}

void TvManager::OnTvWindowAvailable(const URL *url, BOOL available)
{
	const uni_char *urlstring =
		url ? url->GetAttribute(URL::KUniName, FALSE).CStr() : NULL;
	TvListenerContainer *p =
		static_cast<TvListenerContainer *>(m_listeners.First());
	while (p)
	{
		p->GetListener()->OnTvWindowAvailable(urlstring, available);
		p = static_cast<TvListenerContainer *>(p->Suc());
	}
}

#ifdef MEDIA_SUPPORT
void TvManager::SetDisplayRect(OpMediaHandle handle, const OpRect& tvRect, const OpRect& clipRect)
{
	TvListenerContainer *p =
		static_cast<TvListenerContainer *>(m_listeners.First());
	while (p)
	{
		p->GetListener()->SetDisplayRect(handle, tvRect, clipRect);
		p = static_cast<TvListenerContainer *>(p->Suc());
	}
}

void TvManager::OnTvWindowAvailable(OpMediaHandle handle, BOOL available)
{
	TvListenerContainer *p =
		static_cast<TvListenerContainer *>(m_listeners.First());
	while (p)
	{
		p->GetListener()->OnTvWindowAvailable(handle, available);
		p = static_cast<TvListenerContainer *>(p->Suc());
	}
}
#endif // MEDIA_SUPPORT

void TvManager::GetChromakeyColor(UINT8* red, UINT8* green, UINT8* blue, UINT8* alpha)
{
	*red = chroma_red;
	*green = chroma_green;
	*blue = chroma_blue;
	*alpha = chroma_alpha;
}


void TvManager::SetChromakeyColor(UINT8 red, UINT8 green, UINT8 blue, UINT8 alpha)
{
	chroma_red = red;
	chroma_green = green;
	chroma_blue = blue;
	chroma_alpha = alpha;
}

OP_STATUS TvManager::RegisterTvListener(OpTvWindowListener *listener)
{
	// Don't do anything if we already have it registered
	if (NULL != GetContainerFor(listener)) return OpStatus::OK;

	// Encapsulate the OpTvWindowListener in a TvListenerContainer and
	// put it into our list.
	TvListenerContainer *new_container = OP_NEW(TvListenerContainer, (listener));
	if (!new_container)
		return OpStatus::ERR_NO_MEMORY;

	new_container->IntoStart(&m_listeners);
	return OpStatus::OK;
}

void TvManager::UnregisterTvListener(OpTvWindowListener *listener)
{
	// Find the Listener object corresponding to this OpTvWindowListener
	TvListenerContainer *container = GetContainerFor(listener);

	if (NULL == container)
	{
		// Don't do anything if we weren't registered (warn in debug builds)
		OP_ASSERT(!"Unregistering unregistered listener");
		return;
	}

	container->Out();
	OP_DELETE(container);
}

TvListenerContainer *TvManager::GetContainerFor(const OpTvWindowListener *which)
{
	Link *p = m_listeners.First();
	while (p && p->LinkId() != reinterpret_cast<const char *>(which))
	{
		p = p->Suc();
	}

	// Either the found object, or NULL
	return static_cast<TvListenerContainer *>(p);
}

#endif // HAS_ATVEF_SUPPORT
