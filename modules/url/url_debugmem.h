/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jonny Rein Eriksen
**
*/

#ifndef _URL_DEBUGMEM_H_
#define _URL_DEBUGMEM_H_

#ifdef _OPERA_DEBUG_DOC_
struct DebugUrlMemory
{
	unsigned long memory_buffers;
	unsigned long number_visited, memory_visited;
	unsigned long number_loaded, memory_loaded;
		unsigned long number_ramcache, memory_ramcache;
	unsigned long number_servernames, memory_servernames;
	unsigned long number_cookies, number_waiting_cookies, memory_cookies;
	unsigned long registered_diskcache, registered_ramcache;

	DebugUrlMemory(){
		memory_buffers = 
		number_visited = memory_visited = 
		number_loaded =  memory_loaded =
		number_ramcache = memory_ramcache =
		number_cookies = number_waiting_cookies = memory_cookies =
		number_servernames = memory_servernames = 
		registered_diskcache = registered_ramcache = 0;
	};

};
#endif


#endif // _URL_DEBUGMEM_H_
