/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006 - 2008
 *
 * Web server implementation 
 */

#ifndef __WEBSERVER_RENDEZVOUS_UTIL_H__
#define __WEBSERVER_RENDEZVOUS_UTIL_H__

#include "modules/pi/network/OpSocket.h"
	
class RendezvousLog
{
public:
	static OP_STATUS Make(RendezvousLog *&log);

	~RendezvousLog();
  
	static void log(const char *fmt, ...);

	void logOnce(int id, const char *fmt);

	void clear(int id);

private:
	RendezvousLog();

	BOOL *m_ids;

};

#endif // __WEBSERVER_RENDEZVOUS_UTIL_H__
