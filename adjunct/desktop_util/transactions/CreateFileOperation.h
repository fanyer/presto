/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef CREATE_FILE_OPERATION_H
#define CREATE_FILE_OPERATION_H

#include "adjunct/desktop_util/transactions/OpUndoableOperation.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/simset.h"


/**
 * Undoable file creation.
 *
 * For this operation to be undoable in a clean way, Do() records all the path
 * components that have to be created for the file to be created.  In Undo(),
 * each of the recorded path components is deleted, provided it is empty.
 *
 * Note that this class does not really create a file, because we don't know how
 * the file should be created.  OpFile has more than one way of doing that
 * (OpFile::Open(), OpFile::CopyContents(), etc.), and there's no way to tell
 * which is the one for a particular case.  That's why clients are expected to
 * register a CreateFileOperation with OpTransaction right before actually doing
 * that something that results in file creation.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class CreateFileOperation : public OpUndoableOperation
{
public:
	/**
	 * Makes an OpString a list element.
	 */
	class PathComponent : public ListElement<PathComponent>
	{
	public:
		explicit PathComponent(OpString& component);
		const OpStringC& Get() const;
	private:
		OpString m_component;
	};

	typedef AutoDeleteList<PathComponent> PathComponentList;

	/**
	 * Constructs a CreateFileOperation associated with an OpFile.
	 *
	 * @param file represents the file to be created
	 * @param file_mode determines the file type
	 */
	CreateFileOperation(const OpFile& file, OpFileInfo::Mode file_mode);

	/**
	 * Records all the path components that have to be created for the file to
	 * be created.  Does not actually create the file -- that is left up to the
	 * client.  The path components are then available via GetPathComponents().
	 * Even if Do() fails half way through, GetPathComponents() will return the
	 * components recorded up to the point of failure, which can be used to
	 * attempt some corrective actions, etc.
	 *
	 * @return status
	 * @see GetPathComponents()
	 */
	virtual OP_STATUS Do();

	virtual void Undo();

	/**
	 * @return the file that this operation concerns
	 */
	const OpFile& GetFile() const;

	/**
	 * @return the type of the file that this operation concerns
	 */
	OpFileInfo::Mode GetFileMode() const;

	/**
	 * @return all the path components that have to be created for the file to
	 * 		be created, in order from longest to shortest.  The correct result
	 * 		is available after Do() has been called.
	 */
	const PathComponentList& GetPathComponents() const;

private:
	static OP_STATUS DeleteDirIfEmpty(const OpStringC& path);

	OpFile m_file;
	const OpFileInfo::Mode m_file_mode;
	PathComponentList m_components;
};

#endif // CREATE_FILE_OPERATION_H
