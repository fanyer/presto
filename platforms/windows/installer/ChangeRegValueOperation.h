// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Julien Picalausa
//

#ifndef CHANGE_REG_VALUE_OPERATION_H
#define CHANGE_REG_VALUE_OPERATION_H

#include "adjunct/desktop_util/transactions/OpUndoableOperation.h"
#include "platforms/windows/utils/authorization.h"

#define REG_DELETE (DWORD)-1

class CreateRegKeyOperation;

class ChangeRegValueOperation : public OpUndoableOperation
{
public:
	ChangeRegValueOperation(HKEY root_key, const OpStringC &key_path, const OpStringC &value_name, const BYTE* value_data, const DWORD value_data_type, const DWORD value_data_size);
	~ChangeRegValueOperation();

	virtual OP_STATUS Do();
	virtual void Undo();

private:
	const HKEY m_root_key;
	OpString m_key_path;
	OpString m_value_name;
	DWORD m_old_type;
	const DWORD m_new_type;
	BYTE* m_old_data;
	const BYTE* m_new_data;
	DWORD m_old_data_size;
	const DWORD m_new_data_size;

	WindowsUtils::RestoreAccessInfo* m_restore_access_info;

	CreateRegKeyOperation* m_create_key_op;
};

#endif CHANGE_REG_VALUE_OPERATION_H