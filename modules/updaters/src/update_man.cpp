/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined UPDATERS_ENABLE_MANAGER
#include "modules/updaters/update_man.h" 

AutoFetch_Element::~AutoFetch_Element()
{
	if(InList())
		Out();
}

void AutoFetch_Element::PostFinished(MH_PARAM_2 result)
{
	finished = TRUE;
	if(finished_msg != MSG_NO_MESSAGE)
		g_main_message_handler->PostMessage(finished_msg, Id(), result);
}

void AutoFetch_Manager::InitL()
{
}

void AutoFetch_Manager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	AutoFetch_Element *item, *next_item;

	next_item = (AutoFetch_Element *) updaters.First();
	if(next_item)
	{
		while(next_item)
		{
			item = next_item;
			next_item = next_item->Suc();

			if(item->Id() == par1 || item->IsFinished())
			{
				OP_DELETE(item);
				break;
			}
		}
		if(updaters.Empty())
			OnAllUpdatersFinished();
	}
}

void AutoFetch_Manager::OnAllUpdatersFinished()
{
	if(updaters.Empty() && fin_msg != MSG_NO_MESSAGE)
		g_main_message_handler->PostMessage(fin_msg, fin_id,0);
}

OP_STATUS AutoFetch_Manager::AddUpdater(AutoFetch_Element *new_updater, BOOL is_started)
{
	if(new_updater)
	{
		if(!is_started)
		{
			OP_STATUS op_err = new_updater->StartLoading();
			if(OpStatus::IsError(op_err))
			{
				OP_DELETE(new_updater);
				return op_err;
			}
		}
		new_updater->Into(&updaters);
		return OpStatus::OK;
	}
	return OpStatus::ERR_NULL_POINTER;
}

#ifdef UPDATERS_ENABLE_MANAGER_CLEANUP
void AutoFetch_Manager::Cleanup()
{
	AutoFetch_Element *item, *next_item;

	next_item = (AutoFetch_Element *) updaters.First();
	if(next_item)
	{
		while(next_item)
		{
			item = next_item;
			next_item = next_item->Suc();

			if(item->IsFinished())
			{
				OP_DELETE(item);
				break;
			}
		}
		if(updaters.Empty() && fin_msg != MSG_NO_MESSAGE)
			g_main_message_handler->PostMessage(fin_msg, 0,0);
	}
}
#endif
#endif
