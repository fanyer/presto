// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Julien Picalausa
//

#ifndef REPLACE_FILE_OPERATION_H
#define REPLACE_FILE_OPERATION_H

#include "adjunct/desktop_util/transactions/OpUndoableOperation.h"
#include "modules/util/opfile/opfile.h"

class CreateFileOperation;
class DeleteFileOperation;

class ReplaceFileOperation : public OpUndoableOperation
{
public:
	ReplaceFileOperation(const OpFile &file, OpFileInfo::Mode mode);
	~ReplaceFileOperation();

	virtual OP_STATUS Do();
	virtual void Undo();

private:
	CreateFileOperation* m_create_op;
	DeleteFileOperation* m_delete_op;

	BOOL m_init_successful;
};

#endif REPLACE_FILE_OPERATION_H