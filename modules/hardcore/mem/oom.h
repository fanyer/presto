/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_HARDCORE_MEM_OOM_H
#define MODULES_HARDCORE_MEM_OOM_H

#include "modules/hardcore/mh/messobj.h"

struct rainyday_t 
{
	size_t		size;	// in bytes
	rainyday_t  *next;
};

#ifdef OUT_OF_MEMORY_POLLING

#include "modules/hardcore/base/periodic_task.h"

class OOMPeriodicTask : public PeriodicTask
{
private:
	int         ticks;

public:
	OOMPeriodicTask() : ticks(0) {}
	void Run();
};
#endif //OUT_OF_MEMORY_POLLING

#endif // !MODULES_HARDCORE_MEM_OOM_H
