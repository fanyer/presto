/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006 - 2008
 *
 * Web server implementation 
 */

#include "core/pch.h"

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT

/* FIXME: rewrite this code */

#include "modules/webserver/src/rendezvous/webserver_rendezvous_util.h"

#define MAX_IDS 10

RendezvousLog::RendezvousLog()
{
	m_ids=NULL;
}

OP_STATUS RendezvousLog::Make(RendezvousLog *&log)
{
	log = NULL;
	
	OpAutoPtr<RendezvousLog> newlog(OP_NEW(RendezvousLog,()));
	
	if (!newlog.get())
		return OpStatus::ERR_NO_MEMORY;
	
	newlog->m_ids = OP_NEWA(BOOL, MAX_IDS);
	
	if (newlog->m_ids == NULL)
		return OpStatus::ERR_NO_MEMORY;

	BOOL *ids = newlog->m_ids;
  
	for (int i = 0; i < MAX_IDS; i++) 
	{
		ids[i] = FALSE;
	}
	
	log = newlog.release();
  
	return OpStatus::OK;
}

RendezvousLog::~RendezvousLog()
{
	if (m_ids) 
	{
		OP_DELETEA(m_ids);
	}
}

#ifdef _DEBUG

/* static */ void
RendezvousLog::log(const char *fmt, ...)
{
	/* FIXME: rewrite this code */
	va_list args;
	va_start(args, fmt);
	OpString8 s;
	OP_STATUS result = s.AppendVFormat(fmt, args);
	// FIXME: prepend date string
	OP_ASSERT(result == OpStatus::OK);
	OpStatus::Ignore(result);
	va_end(args);
	s.Append("\n");
	  
	OpString s2;
	s2.Set(s.CStr());
	dbg_systemoutput(s2.CStr()); 
}

#else

void
RendezvousLog::log(const char *fmt, ...)
{	
}

#endif //_DEBUG 

/* staic */ void
RendezvousLog::logOnce(int id, const char *fmt)
{
	OP_ASSERT(m_ids);
	if (!m_ids[id]) 
	{
		m_ids[id] = TRUE;
		log(fmt);
	}
}

void
RendezvousLog::clear(int id)
{
	OP_ASSERT(m_ids);
	m_ids[id] = FALSE;
}

#endif // WEBSERVER_RENDEZVOUS_SUPPORT
