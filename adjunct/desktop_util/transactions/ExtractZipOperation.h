/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef EXTRACT_ZIP_OPERATION_H
#define EXTRACT_ZIP_OPERATION_H

#include "adjunct/desktop_util/transactions/CreateFileOperation.h"
#include "adjunct/desktop_util/transactions/OpUndoableOperation.h"

class OpZip;


/**
 * Undoable ZIP archive extraction.
 *
 * Implemented as a series of CreateFileOperation, one for each archive entry.
 * The archive is really extracted in Do().
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class ExtractZipOperation : public OpUndoableOperation
{
public:
	typedef OpAutoVector<CreateFileOperation> SubOperations;

	/**
	 * Constructs an ExtractZipOperation and defines the archive and the target
	 * path.
	 *
	 * @param archive the ZIP archive
	 * @param extract_to_path where the archive should be extracted
	 */
	ExtractZipOperation(const OpZip& archive, const OpStringC& extract_to_path);

	virtual OP_STATUS Do();
	virtual void Undo();

	/**
	 * @return the CreateFileOperation objects comprising the extraction of the
	 * 		archive.  The correct result is available after Do() has been
	 * 		called.
	 */
	const SubOperations& GetSubOperations() const;

private:
	static void UndoUnless(const BOOL* condition,
			ExtractZipOperation* operation);

	const OpZip* m_archive;
	OpString m_extract_to_path;
	SubOperations m_create_file_operations;
};

#endif // EXTRACT_ZIP_OPERATION_H
