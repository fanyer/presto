/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef OPBUFFEREDLOWLEVELFILE_H
#define OPBUFFEREDLOWLEVELFILE_H

#include "modules/search_engine/FileWrapper.h"

/** Buffering wrapper class for OpLowLevelFile's.
 * 
 * This class can be wrapped around any OpLowLevelFile to perform buffered
 * reads aligned on block boundaries. It may speed up file usage on Windows.
 */
class BufferedLowLevelFile
	: public FileWrapper
{
public:
	/** Create an BufferedLowLevelFile object.
	 *
	 * This does not create or open the file (but the supplied wrapped_file
	 * may be already created or open).
	 *
	 * @param wrapped_file The normal, unbuffered OpLowLevelFile that will handle actual I/O.
	 * @param status set to OK, or error code if the return value is NULL.
	 * @param buffer_size The size of the buffer. Optimally it should be a power of 2
	 * @param transfer_ownership If TRUE, on success of this call the ownership of wrapped_file
	 *     is transferred to the new BufferedLowLevelFile, which becomes responsible for
	 *     deallocating it in the destructor. In this case the wrapped_file object must have
	 *     been allocated using OP_NEW.
	 * @return The new BufferedLowLevelFile, or NULL on error
	 */
	static BufferedLowLevelFile* Create(OpLowLevelFile* wrapped_file,
										OP_STATUS& status,
										OpFileLength buffer_size,
										BOOL transfer_ownership = TRUE);

	~BufferedLowLevelFile();

	OP_STATUS		Open(int mode);
	/** OPPS! Currently not posix compatible. Returns Eof()==TRUE when positioned at end of file,
	 *  not after trying to read past the end. */
	BOOL			Eof() const;
	OP_STATUS		Write(const void* data, OpFileLength len);
	OP_STATUS		Read(void* data,
						 OpFileLength len,
						 OpFileLength* bytes_read);
	OP_STATUS		ReadLine(char** data);
	OpLowLevelFile*	CreateCopy();
	OpLowLevelFile*	CreateTempFile(const uni_char* prefix);
	OP_STATUS		SafeClose();

	OP_STATUS		GetFilePos(OpFileLength* pos) const;
	OP_STATUS		GetFileLength(OpFileLength* len) const;
	OP_STATUS       Close();
	OP_STATUS		Flush();

	/** OPPS! Currently not posix compatible. Does not allow setting a position beyond the end of the file */
	OP_STATUS		SetFilePos(OpFileLength pos,
							   OpSeekMode mode = SEEK_FROM_START);

	OP_STATUS		SetFileLength(OpFileLength len);

private:
	BufferedLowLevelFile();
	BufferedLowLevelFile(OpLowLevelFile*, OpFileLength, unsigned char*, BOOL);

	enum IOop {
		IO_unknown = 3,
		IO_read    = 1,
		IO_write   = 2
	};

	unsigned char* const	m_buffer;
	const OpFileLength		m_buffer_size;
	mutable OpFileLength	m_buffer_start;
	mutable OpFileLength	m_buffer_end;  // non-inclusive
	mutable OpFileLength	m_physical_file_pos;
	mutable OpFileLength	m_virtual_file_pos;
	mutable OpFileLength	m_file_length;
	IOop                    m_last_IO_operation;
	
	CHECK_RESULT(OP_STATUS EnsureValidFileLength() const);
	CHECK_RESULT(OP_STATUS EnsureValidVirtualFilePos() const);
	CHECK_RESULT(OP_STATUS EnsureValidPhysicalFilePos() const);
	CHECK_RESULT(OP_STATUS EnsurePhysicalFilePos(OpFileLength pos, IOop operation));
	CHECK_RESULT(OP_STATUS BufferVirtualFilePos());
	CHECK_RESULT(OP_STATUS BufferedRead(void* data, OpFileLength len, OpFileLength* bytes_read));
};

#endif // !OPBUFFEREDLOWLEVELFILE_H
