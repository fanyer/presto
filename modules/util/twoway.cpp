/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#include "modules/util/twoway.h"

TwoWayPointer_Target::~TwoWayPointer_Target()
{
	TwoWayPointer_Base *point;

	while ((point = (TwoWayPointer_Base *) pointers.First()) != NULL)
	{
		point->SignalActionL(TWOWAYPOINTER_ACTION_DESTROY);
		point->Out(); // Force it out
		point->Internal_Release();

		OP_ASSERT(!point->InList()); // point should not be in a list after this call
	}
}

void TwoWayPointer_Target::SignalActionL(uint32 action_val)
{
	TwoWayPointer_Base * OP_MEMORY_VAR point;
	Head temp_list;
	ANCHOR(Head, temp_list);
	OP_STATUS op_err;

	// Move the list into a temporary list
	temp_list.Append(&pointers);

	while ((point = (TwoWayPointer_Base *) temp_list.First()) != NULL)
	{
		TRAP(op_err, point->SignalActionL(action_val));
		if (OpStatus::IsError(op_err))
		{
			/* If the function left, move the unprocessed items back into the
			 * original list and LEAVE */
			pointers.Append(&temp_list);
			LEAVE(op_err);
		}

		// Action MAY have removed the item from the list
		if (point == (TwoWayPointer_Base *) temp_list.First())
		{
			point->Out();
			point->Into(&pointers);
		}
	}

}
