/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OP_UNDOABLE_OPERATION_H
#define OP_UNDOABLE_OPERATION_H

#include "modules/util/simset.h"


/**
 * Represents one of the undoable, atomic operations comprising an
 * OpTransaction.
 *
 * Please make sure only generic OpUndoableOperation implementations reside
 * in @c desktop_util/transactions.  Anything specific to the task at hand is
 * best kept close the task itself.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 * @see OpTransaction::Continue
 */
class OpUndoableOperation : public ListElement<OpUndoableOperation>
{
public:
	/**
	 * Executes the operation.
	 *
	 * @return status used by OpTransaction to determine subsequent course of
	 * 		action
	 */
	virtual OP_STATUS Do() = 0;

	/**
	 * Reverses the effects of Do().  No return value, because when we're here,
	 * we're already rolling back, so there's nothing more we can do to respond
	 * to errors.
	 */
	virtual void Undo() = 0;
};


#endif // OP_UNDOABLE_OPERATION_H
