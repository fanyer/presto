/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/desktop_util/adt/finalizer.h"
#include "adjunct/desktop_util/transactions/OpUndoableOperation.h"
#include "adjunct/desktop_util/transactions/OpTransaction.h"


OpTransaction::OpTransaction()
	: m_state(IN_PROGRESS)
{
}

OpTransaction::~OpTransaction()
{
	if (COMMITTED != m_state)
	{
		RollBack();
	}
}


OP_STATUS OpTransaction::Continue(OpAutoPtr<OpUndoableOperation> operation)
{
	BOOL operation_executed = FALSE;

	if (IN_PROGRESS == m_state)
	{
		Finalizer on_return(BindableFunction(
					&AbortUnless, &operation_executed, this));

		RETURN_OOM_IF_NULL(operation.get());
		RETURN_IF_ERROR(operation->Do());

		operation.release()->Into(&m_operations);
		operation_executed = TRUE;
	}

	return operation_executed ? OpStatus::OK : OpStatus::ERR;
}


OP_STATUS OpTransaction::Commit()
{
	BOOL committed = COMMITTED == m_state;

	if (IN_PROGRESS == m_state)
	{
		m_state = COMMITTED;
		committed = TRUE;
	}

	return committed ? OpStatus::OK : OpStatus::ERR;
}


void OpTransaction::RollBack()
{
	OP_ASSERT(COMMITTED != m_state
			|| !"Trying to roll back a committed transaction");
	
	for (OpUndoableOperation* operation = m_operations.Last();
			NULL != operation; operation = operation->Pred())
	{
		operation->Undo();
	}
}


void OpTransaction::AbortUnless(BOOL* condition, OpTransaction* tx)
{
	OP_ASSERT(NULL != condition && NULL != tx);
	if (!*condition)
	{
		tx->m_state = ABORTED;
	}
}
