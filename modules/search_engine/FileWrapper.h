/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef FILEWRAPPER_H
#define FILEWRAPPER_H

#include "modules/pi/system/OpLowLevelFile.h"

/** Pass-through wrapper class for OpLowLevelFile's.
 * 
 * This class can be wrapped around any OpLowLevelFile to perform... nothing.
 * It works as a parent class for implementations that do... something.
 */
class FileWrapper
	: public OpLowLevelFile
{
public:
	/** Create a FileWrapper.
	 *
	 * This does not create or open the file (but the supplied wrapped_file
	 * may be already created or open).
	 *
	 * @param wrapped_file The normal OpLowLevelFile that will handle actual I/O.
	 * @param transfer_ownership If TRUE, the ownership of wrapped_file
	 *     is transferred to the new FileWrapper, which becomes responsible for
	 *     deallocating it in the destructor. In this case the wrapped_file object must have
	 *     been allocated using OP_NEW.
	 */
	FileWrapper(OpLowLevelFile* wrapped_file, BOOL transfer_ownership);
	~FileWrapper();

	const uni_char*	GetFullPath() const;
	const uni_char*	GetSerializedName() const;
	OP_STATUS		GetLocalizedPath(OpString *localized_path) const;
	BOOL			IsWritable() const;
	OP_STATUS		Open(int mode);
	BOOL			IsOpen() const;
	BOOL			Eof() const;
	OP_STATUS		Write(const void* data, OpFileLength len);
	OP_STATUS		Read(void* data,
						 OpFileLength len,
						 OpFileLength* bytes_read);
	OP_STATUS		ReadLine(char** data);
	OpLowLevelFile*	CreateCopy();
	OpLowLevelFile*	CreateTempFile(const uni_char* prefix);
	OP_STATUS		CopyContents(const OpLowLevelFile* copy);
	OP_STATUS		SafeClose();
	OP_STATUS		SafeReplace(OpLowLevelFile* new_file);
	OP_STATUS		Exists(BOOL* exists) const;

	OP_STATUS		GetFilePos(OpFileLength* pos) const;
	OP_STATUS		GetFileLength(OpFileLength* len) const;
	OP_STATUS		GetFileInfo(OpFileInfo* info);
#ifdef SUPPORT_OPFILEINFO_CHANGE
	OP_STATUS		ChangeFileInfo(const OpFileInfoChange* changes);
#endif
	OP_STATUS		Close();
	OP_STATUS		MakeDirectory();
	OP_STATUS		Delete();
	OP_STATUS		Flush();

	OP_STATUS		SetFilePos(OpFileLength pos,
							   OpSeekMode mode = SEEK_FROM_START);

	OP_STATUS		SetFileLength(OpFileLength len);

#ifdef PI_ASYNC_FILE_OP
	void			SetListener(OpLowLevelFileListener* listener);
	OP_STATUS		ReadAsync(void* data,
							  OpFileLength len,
							  OpFileLength abs_pos = FILE_LENGTH_NONE);
	OP_STATUS		WriteAsync(const void* data,
							  OpFileLength len,
							  OpFileLength abs_pos = FILE_LENGTH_NONE);
	OP_STATUS		DeleteAsync();
	OP_STATUS		FlushAsync();
	OP_STATUS		Sync();
	BOOL			IsAsyncInProgress();
#endif // PI_ASYNC_FILE_OP

private:
	FileWrapper();

protected:
	OpLowLevelFile*			m_f;
	const BOOL				m_owns_file;
	int						m_mode;
};

#endif // !FILEWRAPPER_H
