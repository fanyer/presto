/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/util/objfactory.h"

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/hardcore/mh/mh.h"

#include "modules/url/protocols/comm.h"
#include "modules/url/protocols/scomm.h"

template <class OBJECT>
OpObjectFactory<OBJECT>::OpObjectFactory()
	: size(0),
	  capacity(0),
	  id(0),
	  refill_failed(FALSE),
	  oom_condition_raised(FALSE),
	  post_critical_section(FALSE)
{
}

template <class OBJECT>
OpObjectFactory<OBJECT>::~OpObjectFactory()
{
	g_main_message_handler->RemoveDelayedMessage(MSG_OBJ_FACTORY_REFILL, id, 0);
	g_main_message_handler->UnsetCallBack(this, MSG_OBJ_FACTORY_REFILL, id);
	while (size > 0) {
		OBJECT *obj = (OBJECT*)cache.First();
		obj->Out();
		delete obj;
		size--;
	}
}

template <class OBJECT>
void OpObjectFactory<OBJECT>::ConstructL( size_t requested_capacity )
{
	OP_ASSERT( !id );

	capacity = requested_capacity;

	while (size < capacity)
	{
		OBJECT *obj = new OBJECT;
		LEAVE_IF_NULL(obj);
		obj->Into(&cache);
		size++;
	}

	int tmpid = ++g_unique_id_counter;
	LEAVE_IF_ERROR(g_main_message_handler->SetCallBack( this, MSG_OBJ_FACTORY_REFILL, tmpid));
	
	// Now we're ready.
	id = tmpid;
}

template <class OBJECT>
OBJECT *
OpObjectFactory<OBJECT>::Allocate()
{
	OBJECT *obj;

	OP_ASSERT( id );

	if (refill_failed)
		PostRefill();

	obj = new OBJECT;
	if (obj != NULL)
		return obj;

	if (size < capacity/2)
	{
		BOOL f = !oom_condition_raised;
		oom_condition_raised = TRUE;
		if (f)
			g_memory_manager->RaiseCondition( OpStatus::ERR_NO_MEMORY );
	}

	if (size == 0)
		return NULL;
	
	size--;								// Take the element now so that PostRefill() does not take it.
	obj = (OBJECT*)cache.First();
	obj->Out();

	if (size == capacity-1)
		PostRefill();

	return obj;
}

template <class OBJECT>
void OpObjectFactory<OBJECT>::HandleCallback(OpMessage /*msg*/, MH_PARAM_1 /*par1*/, MH_PARAM_2 /*par2*/)
{
	OP_ASSERT( id );

	while (size < capacity)
	{
		OBJECT *obj = new OBJECT;
		if (obj == NULL) {
			PostRefill();
			return;
		}
		obj->Into(&cache);
		size++;
	}
	oom_condition_raised = FALSE;		// Don't clear until we've managed to refill completely
}

template <class OBJECT>
void
OpObjectFactory<OBJECT>::PostRefill()
{
	if (post_critical_section)
		return;

	post_critical_section = TRUE;

	refill_failed = FALSE;	   			// Clear the flag first to avoid a storm
	if (!g_main_message_handler->PostDelayedMessage( MSG_OBJ_FACTORY_REFILL, id, 0, OBJFACTORY_REFILL_INTERVAL ))
		refill_failed = TRUE;

	post_critical_section = FALSE;
}

template class OpObjectFactory<CommWaitElm>;
template class OpObjectFactory<SCommWaitElm>;
