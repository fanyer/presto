/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OP_TRANSACTION_H
#define OP_TRANSACTION_H

#include "modules/util/simset.h"

class OpUndoableOperation;


/**
 * A transaction involving any number of undoable, atomic operations.
 *
 * Enroll operations with Continue(), commit the transaction with Commit().
 *
 * A transaction is never rolled back explicitely.  Instead, roll-back is
 * automatic and happens if and only if the transaction object is destroyed
 * before Commit() is called successfully.
 *
 * Typically, you will not be registering each and every operation with the
 * transaction object---only those that actually need undoing upon roll-back.
 *
 * All operations must (appear to) be atomic.  OpTransaction can do nothing
 * about any partial results of a failed operation.
 *
 * Example:
 * @code
 *	OP_STATUS Execute()
 *	{
 * 		OpTransaction tx;
 * 		RETURN_IF_ERROR(PrepareUndoableOperation());
 * 		ConcreteOperation* operation = OP_NEW(ConcreteOperation, ());
 * 		RETURN_IF_ERROR(tx.Continue(OpAutoPtr<OpUndoableOperation>(operation)));
 *		// ...
 * 		return tx.Commit();
 * 	}
 * @endcode
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class OpTransaction
{
public:
	OpTransaction();
	~OpTransaction();

	/**
	 * Enrolls an operation that needs undoing upon roll-back.  Any error
	 * occuring within, including errors when calling any method on
	 * @a operation, renders committing impossible.
	 *
	 * @param operation an undoable operation
	 * @return status
	 */
	OP_STATUS Continue(OpAutoPtr<OpUndoableOperation> operation);

	/**
	 * Commits the transaction.  No further Continue() calls are possible.  The
	 * transaction object can now be destroyed.  Unless Commit() itself fails,
	 * there will be no roll-back.
	 *
	 * @return status
	 */
	OP_STATUS Commit();

	/**
	 * Resets the transaction state to try again. If you have tried to fix the problem
	 * with the transaction, you have to reset the state of the transaction, to be able
	 * to try it again.
	 *
	 */
	 void	 ResetState() { m_state = IN_PROGRESS; };

private:
	typedef AutoDeleteList<OpUndoableOperation> OperationList;

	// Non-copyable.
	OpTransaction(const OpTransaction&);
	OpTransaction& operator=(const OpTransaction&);

	void RollBack();

	static void AbortUnless(BOOL* condition, OpTransaction* tx);

	enum
	{
		IN_PROGRESS,
		ABORTED,
		COMMITTED,
	} m_state;

	OperationList m_operations;
};

#endif // OP_TRANSACTION_H
