// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Julien Picalausa
//

#ifndef MOVE_FILE_OPERATION_H
#define MOVE_FILE_OPERATION_H

#include "adjunct/desktop_util/transactions/OpUndoableOperation.h"
#include "modules/util/opfile/opfile.h"
#include "platforms/windows/utils/authorization.h"

class MoveFileOperation : public OpUndoableOperation
{
public:
	MoveFileOperation(const OpFile &from_file, const OpFile &to_file);
	~MoveFileOperation();

	virtual OP_STATUS Do();
	virtual void Undo();

private:
	OpFile *m_from_file;
	OpFile *m_to_file;
	BOOL m_init_success;

	WindowsUtils::RestoreAccessInfo* m_restore_access_info;
};

#endif //MOVE_FILE_OPERATION_H