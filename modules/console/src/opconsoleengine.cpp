/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#ifdef OPERA_CONSOLE

#include "modules/console/opconsoleengine.h"
#include "modules/dochand/winman.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/util/timecache.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/dochand/win.h"

#ifdef HAS_COMPLEX_GLOBALS
/** Initialize list of source serialization keywords.
  * @param length Length of the array. */
# define INIT_KEYWORDS(length) \
	const char * const \
		OpConsoleEngine::m_keywords[length] =
# define INITSTART				///< Mark start of initializer list.
# define INITEND				///< Mark end of initializer list.
/** Define a serialization keywords.
  * @param value The keyword. */
# define K(value) \
	value
#else
# define INIT_KEYWORDS(length) \
	void OpConsoleEngine::InitSourceKeywordsL()
# define INITSTART \
	int i = 0;
# define INITEND \
	;
# define K(value) \
	m_keywords[i ++] = value
#endif

#ifdef CON_GET_SOURCE_KEYWORD
// This list should be kept in synch with the OpConsoleEngine::Source
// enum in opconsoleengine.h
// Note that keywords may only contain alphanumeric characters and
// underscores ('_'). They may not contain whitespace.
INIT_KEYWORDS(static_cast<int>(OpConsoleEngine::SourceTypeLast))
{
	INITSTART
	K("javascript"),
#ifdef _APPLET_2_EMBED_
	K("java"),
#endif
#ifdef M2_SUPPORT
	K("mail"),
#endif
	K("network"),
	K("xml"),
	K("html"),
	K("css"),
#ifdef XSLT_SUPPORT
	K("xslt"),
#endif
#ifdef SVG_SUPPORT
	K("svg"),
#endif
#ifdef _BITTORRENT_SUPPORT_
	K("bittorrent"),
#endif
#ifdef GADGET_SUPPORT
	K("widgets"),
#endif
#if defined(SUPPORT_DATA_SYNC) || defined(WEBSERVER_SUPPORT)
	K("operaaccount"),
#endif
#ifdef SUPPORT_DATA_SYNC
	K("operalink"),
#endif
#ifdef WEBSERVER_SUPPORT
	K("operaunite"),
#endif
#ifdef AUTO_UPDATE_SUPPORT
	K("autoupdate"),
#endif
	K("internal")
#ifdef DATABASE_MODULE_MANAGER_SUPPORT
	,K("persistent_storage")
#endif //DATABASE_MODULE_MANAGER_SUPPORT
#ifdef WEBSOCKETS_SUPPORT
	,K("websockets")
#endif //WEBSOCKETS_SUPPORT
#ifdef GEOLOCATION_SUPPORT
	,K("geolocation")
#endif // GEOLOCATION_SUPPORT
#ifdef MEDIA_SUPPORT
	,K("media")
#endif // MEDIA_SUPPORT
#ifdef _PLUGIN_SUPPORT_
	,K("plugins")
#endif // _PLUGIN_SUPPORT_
#ifdef SELFTEST
	,K("selftest")
#endif
	INITEND
};
#endif

OpConsoleEngine::OpConsoleEngine()
	: m_messages(NULL),
	  m_lowest_id(1),
	  m_highest_id(0),
	  m_size(0),
	  m_pending_broadcast(FALSE)
{
}

void OpConsoleEngine::ConstructL(size_t size)
{
#if defined(CON_GET_SOURCE_KEYWORD) && !defined(HAS_COMPLEX_GLOBALS)
	// Initialize global array
	InitSourceKeywordsL();
#endif

	// Initialize object data
	m_messages = OP_NEWA_L(Message, size);
	m_size = size;

	// Register with the message handler so that we can call ourselves
	// later
	g_main_message_handler->SetCallBack(this, MSG_CONSOLE_POSTED, 0);
}

OpConsoleEngine::~OpConsoleEngine()
{
	g_main_message_handler->UnsetCallBacks(this);
	OP_DELETEA(m_messages);
}

OpConsoleEngine::Message::Message(Source src, Severity sev, OpWindowCommander *wic, time_t posted_time)
	: source(src)
	, severity(sev)
	, window(wic->GetWindowId())
	, time(posted_time)
{
}

OpConsoleEngine::Message::Message(Source src, Severity sev, Window *win, time_t posted_time)
	: source(src)
	, severity(sev)
	, window(win->Id())
	, time(posted_time)
{
}

void OpConsoleEngine::Message::CopyL(const OpConsoleEngine::Message *src)
{
	if (!src)
		LEAVE(OpStatus::ERR_NULL_POINTER);

	// Deep copy data from the other structure
	source = src->source;
	severity = src->severity;
	window = src->window;
	time = src->time;
	url.SetL(src->url);
	message.SetL(src->message);
	context.SetL(src->context);
	error_code = src->error_code;
}

void OpConsoleEngine::Message::Clear()
{
	url.Empty();
	context.Empty();
	message.Empty();
}

unsigned int OpConsoleEngine::PostMessageL(const OpConsoleEngine::Message *msg)
{
#ifdef CON_NO_BACKLOG
	// No need to proceed if no one is listening
	if (!IsLogging())
	{
		return MESSAGE_BLOCKED;
	}
#endif

	// Do not log console messages from private windows
	if (msg->window > 0)
	{
		Window* win = g_windowManager->GetWindow(msg->window);
		if (win && win->GetPrivacyMode())
			return static_cast<unsigned>(MESSAGE_BLOCKED);
	}

	// Check if we are overwriting an old message
	unsigned int new_id = m_highest_id + 1;
	if (new_id == m_lowest_id + m_size)
	{
		++ m_lowest_id;
	}

	// Copy the data over to the new message
	OpConsoleEngine::Message *p = m_messages + (new_id % m_size);
	p->Clear();
	p->CopyL(msg);
	if (!p->time)
	{
		p->time = g_timecache->CurrentTime();
	}
	m_highest_id = new_id;
	
	// Save a position in the list
	p->id = new_id;

	if (!m_pending_broadcast)
	{
		// Remind ourselves that we need to broadcast this later. Don't do it
		// now to avoid recursing too deep (we might be deep down in the ES
		// parser or something already).
		g_main_message_handler->PostMessage(MSG_CONSOLE_POSTED, 0,
		                                    static_cast<MH_PARAM_2>(new_id));
		m_pending_broadcast = TRUE;
	}

	return new_id;
}

void OpConsoleEngine::HandleCallback(OpMessage msg, MH_PARAM_1, MH_PARAM_2 in_id)
{
	unsigned int start_id = MAX(static_cast<unsigned int>(in_id), m_lowest_id);

	if (MSG_CONSOLE_POSTED == msg)
	{
		OP_STATUS rc;

		// Broadcast the updates to all listeners
		for (Link *l = m_listeners.First(); l; l = l->Suc())
		{
			rc = OpStatus::OK;
			for (unsigned int id = start_id; id <= m_highest_id && OpStatus::IsSuccess(rc); ++ id)
			{
				rc = (static_cast<OpConsoleListener *>(l))->
				         NewConsoleMessage(id, m_messages + (id % m_size));
			}

			if (OpStatus::IsError(rc))
			{
				// Something went wrong while broadcasting to this
				// listener. Flag the error and continue with the next
				// listener.
				if (OpStatus::IsRaisable(rc))
				{
					g_memory_manager->RaiseCondition(rc);
				}
			}
		}
	}
	else
	{
		OP_ASSERT(!"Should not have seen that message.");
	}

	m_pending_broadcast = FALSE;

#ifdef CON_NO_BACKLOG
	Clear();
#endif
}

const struct OpConsoleEngine::Message *OpConsoleEngine::GetMessage(unsigned int id)
{
	if (id >= GetLowestId() && id <= GetHighestId())
	{
		return &m_messages[id % m_size];
	}

	// Index out of bounds.
	return NULL;
}

void OpConsoleEngine::Clear()
{
	for (unsigned int i = 0; i < m_size; ++ i)
	{
		m_messages[i].Clear();
	}

	// Keep the numbering even after having removed the data.
	m_lowest_id = m_highest_id + 1;
}

#endif // OPERA_CONSOLE
