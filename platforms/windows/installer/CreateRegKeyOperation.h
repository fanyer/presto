/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef CREATE_REG_KEY_OPERATION_H
#define CREATE_REG_KEY_OPERATION_H

#include "adjunct/desktop_util/transactions/OpUndoableOperation.h"
#include "platforms/windows/utils/authorization.h"

/**
 * Undoable Registry key creation.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 * @see OpTransaction
 */
class CreateRegKeyOperation : public OpUndoableOperation
{
public:
	/**
	 * Constructs the operation object.
	 *
	 * @param ancestor_handle a handle to the ancestor key of the key to be
	 * 		created
	 * @param key_name the name of the key to be created, relative to
	 * 		@a ancestor_handle
	 */
	CreateRegKeyOperation(HKEY ancestor_handle, const OpStringC& key_name);

	~CreateRegKeyOperation();

	virtual OP_STATUS Do();
	virtual void Undo();

	/**
	 * @return ancestor_handle a handle to the ancestor key of the key to be
	 * 		created
	 */
	HKEY GetAncestorHandle() const;

	/**
	 * @return key_name the name of the key to be created, relative to the
	 * 		ancestor handle
	 */
	const OpStringC& GetKeyName() const;

private:
	const HKEY m_ancestor_handle;
	OpString m_key_name;

	WindowsUtils::RestoreAccessInfo* m_restore_access_info;
};

#endif // CREATE_REG_KEY_OPERATION_H
