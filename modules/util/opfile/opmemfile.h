/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_UTIL_OPFILE_OPMEMFILE_H
#define MODULES_UTIL_OPFILE_OPMEMFILE_H

#ifndef HAVE_NO_OPMEMFILE

#include "modules/util/opfile/opfile.h"

/**
 * OpMemFile contains functionality for simulating a file in memory
 * instead of a file on a filesystem.
 */
class OpMemFile : public OpFileDescriptor
{
public:
	OpMemFile();
	virtual ~OpMemFile();

	/**
	 * Creates an OpMemFile object which will consist of the 'data'
	 * buffer and have an initial file size and buffer size equal to
	 * size. The file pointer will be at the beginning of the buffer.
	 *
	 * @param data Buffer which describes the contents of the file.
	 *
	 * @param size The size of the 'data' buffer.
	 *
	 * @param take_over If FALSE, a new buffer will be allocated and data will
	 * be copied; otherwise data is used as file data and the caller MUST NOT
	 * free data or access it after the returned OpMemFile has been deleted.
	 *
	 * @param name The name of this file, defaults to NULL (treated as an empty
	 * string).  A copy shall be saved and returned by GetName.
	 *
	 * @return NULL if either the new object or the new buffer could
	 * not be allocated, otherwise an new OpMemFile object.
	 */
	static OpMemFile* Create(unsigned char* data, OpFileLength size, BOOL take_over,
							 const uni_char *name = NULL);
	/**
	 * @overload
	 */
	static OpMemFile* Create(const unsigned char* data, OpFileLength size,
							 const uni_char *name = NULL);

	virtual OpFileType Type() const { return OPMEMFILE; }
	virtual BOOL IsWritable() const { return TRUE; }

	virtual OP_STATUS Open(int mode);
	virtual BOOL IsOpen() const;
	virtual OP_STATUS Close();

	virtual BOOL Eof() const;
	virtual OP_STATUS Exists(BOOL& exists) const;

	virtual OP_STATUS GetFilePos(OpFileLength& pos) const;
	virtual OP_STATUS SetFilePos(OpFileLength pos, OpSeekMode seek_mode=SEEK_FROM_START);
	
	virtual uni_char* GetName() { return m_filename; }

	/**
	 * Writes a buffer 'data' of size 'len' to the file. The internal
	 * buffer size is increased if needed. The buffer size is at least
	 * doubled when the current buffer is too small.
	 *
	 * @return OpStatus::ERR_NO_MEMORY if the buffer could not be
	 * increased enough to save the new data.
	 */
	virtual OP_STATUS Write(const void* data, OpFileLength len);
	virtual OP_STATUS Read(void* data, OpFileLength len, OpFileLength* bytes_read);
	virtual OP_STATUS ReadLine(OpString8& str);

	/**
	 * Set this object as a copy of another OpMemFile. The contents of
	 * this file will be deleted.
	 *
	 * @param copy An OpMemFile object which will be copied to this object.
	 *
	 * @return OpStatus::ERR_NO_MEMORY if there was not enough memory
	 * to allocate a new buffer for the new copy. Otherwise OpStatus::OK.
	 */
	virtual OP_STATUS Copy(const OpFileDescriptor* copy);

	/**
	 * Return a copy of this file.
	 *
	 * @return Copy of this file or NULL on OOM.
	 */
	virtual OpFileDescriptor* CreateCopy() const;

	OP_STATUS GetFileLength(OpFileLength& len) const { len=m_file_len; return OpStatus::OK; }
	OpFileLength GetFileLength() const { return m_file_len; }

	unsigned char* GetBuffer() const { return m_data; }
private:
	OP_STATUS Resize(OpFileLength new_size);
	OP_STATUS GrowIfNeeded(OpFileLength desired_size);

private:
	unsigned char* m_data;
	OpFileLength   m_size;
	OpFileLength   m_pos;
	OpFileLength   m_file_len;
	BOOL           m_open;
	uni_char*      m_filename;

	/** Block assignment operator. */
	OpMemFile &operator=(const OpMemFile &);
	/** Block copy constructor. */
	OpMemFile(const OpMemFile &);
};

#endif // !HAVE_NO_OPMEMFILE

#endif // !MODULES_UTIL_OPFILE_OPMEMFILE_H
