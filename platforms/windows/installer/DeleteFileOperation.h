// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Julien Picalausa
//

//The delete operation will attempt to rename the file when do is called, since
//that effectively moves it out of the way and ensures that we have delete rights.
//It will destroy the moved file once the transaction is complete and the operation
//is deleted. It will also try to remove the folder containing the file if empty,
//then the parent of that folder and so on, until finding a folder that has content
//or can't be deleted.

#ifndef DELETE_FILE_OPERATION_H
#define DELETE_FILE_OPERATION_H

#include "adjunct/desktop_util/transactions/OpUndoableOperation.h"
#include "modules/util/opfile/opfile.h"
#include "platforms/windows/utils/authorization.h"

class DeleteFileOperation : public OpUndoableOperation
{
public:
	DeleteFileOperation(const OpFile &file, BOOL remove_empty_folders = TRUE);
	~DeleteFileOperation();

	virtual OP_STATUS Do();
	virtual void Undo();

private:
	OpFile *m_file;
	OpFile *m_temp_file;
	BOOL m_init_success;

	BOOL m_remove_empty_folders;

	OpString m_file_name;

	WindowsUtils::RestoreAccessInfo* m_restore_access_info;
};

#endif //DELETE_FILE_OPERATION_H
