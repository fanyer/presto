// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Julien Picalausa
//

#ifndef DELETE_REG_KEY_OPERATION_H
#define DELETE_REG_KEY_OPERATION_H

#include "adjunct/desktop_util/transactions/OpUndoableOperation.h"
#include "platforms/windows/utils/authorization.h"

class DeleteRegKeyOperation : public OpUndoableOperation
{
public:
	DeleteRegKeyOperation(const HKEY root_key, const OpStringC &key_path);
	~DeleteRegKeyOperation() {}

	virtual OP_STATUS Do();
	virtual void Undo();

private:

	struct ValueInfo
	{
		OpString value_name;
		DWORD value_type;
		BYTE* value_data;
		DWORD value_data_len;

		ValueInfo() : value_data(NULL) {}

		~ValueInfo() {OP_DELETEA(value_data);}
	};

	struct KeyInfo
	{
		OpString key_path;
		PSECURITY_DESCRIPTOR sec_desc;
		OpAutoVector<ValueInfo> values;

		KeyInfo() : sec_desc(NULL) {}

		~KeyInfo() {OP_DELETEA((char*)sec_desc);}
	};

	OP_STATUS DeleteAndSaveRecursive(const HKEY parent_key, const OpStringC &key_path, OpString &full_key_path);

	OpAutoVector<KeyInfo> m_keys;

	const HKEY m_root_key;
	OpString m_key_path;
};

#endif DELETE_REG_KEY_OPERATION_H