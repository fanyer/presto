/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/hardcore/base/periodic_task.h"
#include "modules/pi/OpSystemInfo.h"

PeriodicTaskManager::PeriodicTaskManager()
{
	fire_time = DBL_MAX;
}

PeriodicTaskManager::~PeriodicTaskManager()
{
	intervals.Clear ();
}

OP_STATUS PeriodicTaskManager::RegisterTask(PeriodicTask *t, int interval)
{
	PeriodicTaskInterval *i;

	for (i = (PeriodicTaskInterval *) intervals.First(); i; i = (PeriodicTaskInterval *) i->Suc())
		if (i->interval == interval)
			break;

	if (!i)
	{
		if (!(i = OP_NEW(PeriodicTaskInterval,())))
			return OpStatus::ERR_NO_MEMORY;

		i->fire_time = op_ceil(g_op_time_info->GetRuntimeMS() / interval) * interval;
		i->interval = interval;
		i->Into(&intervals);

		if (i->fire_time < fire_time)
			fire_time = i->fire_time;
	}
	t->Into(&(i->tasks));

	return OpStatus::OK;
}

OP_STATUS PeriodicTaskManager::UnregisterTask(PeriodicTask *t)
{
	PeriodicTaskInterval *i;
	PeriodicTask *j;

	for (i = (PeriodicTaskInterval *) intervals.First(); i; i = (PeriodicTaskInterval *) i->Suc())
		for (j = (PeriodicTask *) i->tasks.First(); j; j = (PeriodicTask *) j->Suc())
			if (j == t)
			{
				j->Out();
				if (i->tasks.Empty())
				{
					i->Out();
					OP_DELETE(i);
				}
				return OpStatus::OK;
			}

	return OpStatus::ERR;
}

void PeriodicTaskManager::RunTasks()
{
	PeriodicTaskInterval *i;
	PeriodicTask *j;

	double time = g_op_time_info->GetRuntimeMS();

	if (time < fire_time)
		return;

	fire_time = DBL_MAX;

	for (i = (PeriodicTaskInterval *) intervals.First(); i; i = (PeriodicTaskInterval *) i->Suc())
	{
		if (time >= i->fire_time)
		{
			for (j = (PeriodicTask *) i->tasks.First(); j; j = (PeriodicTask *) j->Suc())
			{
				if (j->IsEnabled())
					j->Run();
				else
				{
					j->IncrementDisabledTriggerCount();
					j->RunWasDisabled();
				}
			}

			i->fire_time = op_floor(time + i->interval);
		}

		if (i->fire_time < fire_time)
			fire_time = i->fire_time;
	}
}
