/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#include "modules/search_engine/BufferedLowLevelFile.h"

//   Buffer during sequential read:
//
//                       virtual_file_pos  physical_file_pos       file_length
//                               |                |                    |
//   0                           V                V                    V
//   +--------/   /----------+---+----------------+--------/   /-------+
//   |         ...           |///////buffer///////|         ...        |
//   +--------/   /----------+--------------------+--------/   /-------+
//                           |                    |
//                      buffer_start         buffer_end
//
//   |<--- n*buffer_size --->|<--- buffer_size -->|
//
//
//
//   Buffer at end of file:
//                                        file_length,
//                    virtual_file_pos  physical_file_pos
//                              |             |
//   0                          V             V
//   +--------/   /----------+--+-------------+
//   |         ...           |/////buffer/////|
//   +--------/   /----------+----------------+
//                           |                |
//                      buffer_start     buffer_end
//
//   |<--- m*buffer_size --->|<--- buffer_size -->|
//
//
//
//   Buffer after partially overlapping write (current implementation):
//
//                                                virtual_file_pos,
//                                                physical_file_pos
//                                                        |
//   0                                                    V
//   +--------/   /----------+--------------------+-------+--------------
//   |         ...           |///////buffer///////|                       ...
//   |                       |//////////////|###written###|
//   +--------/   /----------+--------------+-----+-------+--------------
//                           |                    |
//                      buffer_start         buffer_end
//
//   |<--- n*buffer_size --->|<--- buffer_size -->|
//

#define BAD_FILE_POS FILE_LENGTH_NONE
#define OPF_IS_NEGATIVE(a) ((((a>>31)>>31)>>2) != 0)

BufferedLowLevelFile*
BufferedLowLevelFile::Create(
		OpLowLevelFile* wrapped_file,
		OP_STATUS& status,
		OpFileLength buffer_size,
		BOOL transfer_ownership)
{
	if (!wrapped_file || buffer_size == 0)
	{
		status = OpStatus::ERR;
		return NULL;
	}

	unsigned char* buffer = OP_NEWA(unsigned char, (unsigned int)buffer_size);
	if (!buffer)
	{
		status = OpStatus::ERR_NO_MEMORY;
		return NULL;
	}

	// So now, we are guaranteed that m_f and m_buffer are always valid!

	BufferedLowLevelFile* wrapper = OP_NEW(BufferedLowLevelFile, (wrapped_file, buffer_size, buffer, transfer_ownership));
	if (!wrapper)
	{
		OP_DELETEA(buffer);
		status = OpStatus::ERR_NO_MEMORY;
		return NULL;
	}

	status = OpStatus::OK;
	return wrapper;
}

BufferedLowLevelFile::BufferedLowLevelFile(OpLowLevelFile* wrapped_file, OpFileLength buffer_size, unsigned char* buffer, BOOL transfer_ownership)
	: FileWrapper(wrapped_file, transfer_ownership),
	  m_buffer(buffer),
	  m_buffer_size(buffer_size),
	  m_buffer_start(BAD_FILE_POS),
	  m_buffer_end(BAD_FILE_POS),
	  m_physical_file_pos(BAD_FILE_POS),
	  m_virtual_file_pos(BAD_FILE_POS),
	  m_file_length(BAD_FILE_POS),
	  m_last_IO_operation(IO_unknown)
{
}

BufferedLowLevelFile::~BufferedLowLevelFile()
{
	OP_DELETEA(m_buffer);
}

OP_STATUS BufferedLowLevelFile::Open(int mode)
{
	OP_STATUS status = FileWrapper::Open(mode);
	if (OpStatus::IsSuccess(status))
		m_physical_file_pos = m_virtual_file_pos = 0;
	else
		m_physical_file_pos = m_virtual_file_pos = BAD_FILE_POS;

	m_buffer_start = m_buffer_end = BAD_FILE_POS;
	m_file_length = BAD_FILE_POS;

	return status;
}

OP_STATUS BufferedLowLevelFile::Close()
{
	m_physical_file_pos = m_virtual_file_pos = BAD_FILE_POS;
	m_buffer_start = m_buffer_end = BAD_FILE_POS;
	m_file_length = BAD_FILE_POS;
	return m_f->Close();
}

/** OPPS! Currently not posix compatible. Returns Eof()==TRUE when positioned at end of file,
 *  not after trying to read past the end. */
BOOL BufferedLowLevelFile::Eof() const
{
	RETURN_VALUE_IF_ERROR(EnsureValidFileLength(), TRUE);
	RETURN_VALUE_IF_ERROR(EnsureValidVirtualFilePos(), TRUE);
	return m_file_length == m_virtual_file_pos;
}

OP_STATUS BufferedLowLevelFile::GetFilePos(OpFileLength* pos) const
{
	OP_STATUS status;

	if (!pos)
		return OpStatus::ERR;
	
	if (OpStatus::IsError(status = EnsureValidVirtualFilePos())) {
		*pos = 0;
		return status;
	}

	*pos = m_virtual_file_pos;
	return OpStatus::OK;
}

OP_STATUS BufferedLowLevelFile::EnsureValidVirtualFilePos() const
{
	OP_STATUS status = OpStatus::OK;

	if (m_virtual_file_pos == BAD_FILE_POS && OpStatus::IsSuccess(status = EnsureValidPhysicalFilePos()))
		m_virtual_file_pos = m_physical_file_pos;

	return status;
}

OP_STATUS BufferedLowLevelFile::EnsureValidPhysicalFilePos() const
{
	OP_STATUS status = OpStatus::OK;
	OpFileLength actual_pos;

	if (m_physical_file_pos == BAD_FILE_POS && OpStatus::IsSuccess(status = m_f->GetFilePos(&actual_pos)))
		m_physical_file_pos = actual_pos;

	return status;
}

OP_STATUS BufferedLowLevelFile::SetFilePos(OpFileLength pos, OpSeekMode mode /* = SEEK_FROM_START */)
{
	// Seeks are lazy, physical file position is not updated until Read/Write

	OP_ASSERT(pos != BAD_FILE_POS || mode == SEEK_FROM_CURRENT); // Seek-from-current with offset -1 is allowed
	RETURN_IF_ERROR(EnsureValidFileLength());
	
	switch (mode)
	{
	case SEEK_FROM_START:
		break;
	case SEEK_FROM_END:
		if (pos <= m_file_length)
			pos = m_file_length - pos;
		else
			pos = BAD_FILE_POS;
		break;
	case SEEK_FROM_CURRENT:
		RETURN_IF_ERROR(EnsureValidVirtualFilePos());
		pos += m_virtual_file_pos;
		break;
	}
	
	if (pos == BAD_FILE_POS || pos > m_file_length || OPF_IS_NEGATIVE(pos))
		return OpStatus::ERR;

	m_virtual_file_pos = pos;
	return OpStatus::OK;
}

OP_STATUS BufferedLowLevelFile::EnsurePhysicalFilePos(OpFileLength pos, IOop operation)
{
	OP_STATUS status;

	OP_ASSERT(pos != BAD_FILE_POS);
	if (pos == m_physical_file_pos && (operation & m_last_IO_operation) != 0)
	{
		m_last_IO_operation = operation;
		return OpStatus::OK;
	}

	m_physical_file_pos = pos;
	status = m_f->SetFilePos(pos);
	if (OpStatus::IsError(status))
		m_physical_file_pos = BAD_FILE_POS;
	else
		m_last_IO_operation = operation;

	return status;
}

OP_STATUS BufferedLowLevelFile::GetFileLength(OpFileLength* len) const
{
	OP_STATUS status;

	if (!len)
		return OpStatus::ERR;
	
	if (OpStatus::IsError(status = EnsureValidFileLength())) {
		*len = 0;
		return status;
	}

	*len = m_file_length;
	return OpStatus::OK;
}

OP_STATUS BufferedLowLevelFile::EnsureValidFileLength() const
{
	OP_STATUS status = OpStatus::OK;
	OpFileLength actual_length;

	if (m_file_length == BAD_FILE_POS && OpStatus::IsSuccess(status = m_f->GetFileLength(&actual_length)))
		m_file_length = actual_length;

	return status;
}

OP_STATUS BufferedLowLevelFile::Write(const void* data, OpFileLength len)
{
	OP_STATUS status;

	RETURN_IF_ERROR(EnsureValidFileLength());
	if ((m_mode & OPFILE_APPEND) != 0)
		m_virtual_file_pos = m_file_length;
	else
		RETURN_IF_ERROR(EnsureValidVirtualFilePos());
	RETURN_IF_ERROR(EnsurePhysicalFilePos(m_virtual_file_pos, IO_write));

	if (m_buffer_start != BAD_FILE_POS && m_virtual_file_pos+len > m_buffer_start && m_virtual_file_pos < m_buffer_end)
	{
		// Buffered data overlaps -> update buffer with what's written
		OpFileLength cpy_len, data_off, buf_off;
		cpy_len = len;
		buf_off = m_virtual_file_pos - m_buffer_start;
		data_off = 0;
		if (m_virtual_file_pos < m_buffer_start)
		{
			buf_off = 0;
			data_off = m_buffer_start - m_virtual_file_pos;
			cpy_len -= data_off;
		}
		if (m_virtual_file_pos + len > m_buffer_end) {
			if (m_buffer_end - m_buffer_start < m_buffer_size) {
				// Expand the buffer
				m_buffer_end = m_virtual_file_pos + len;
				if (m_buffer_end - m_buffer_start > m_buffer_size)
					m_buffer_end = m_buffer_start + m_buffer_size;
			}
			if (m_virtual_file_pos + len > m_buffer_end)
				cpy_len -= m_virtual_file_pos + len - m_buffer_end;
		}
		op_memcpy(m_buffer + buf_off, (const char*)data + data_off, (size_t)cpy_len);
	}

	status = m_f->Write(data, len);
	if (OpStatus::IsSuccess(status))
	{
		m_physical_file_pos = m_virtual_file_pos += len;
		if (m_physical_file_pos > m_file_length)
			m_file_length = m_physical_file_pos;
	}
	else
	{
		m_physical_file_pos = m_virtual_file_pos = BAD_FILE_POS;
		m_buffer_start = m_buffer_end = BAD_FILE_POS;
		m_file_length = BAD_FILE_POS;
	}

	return status;
}

OP_STATUS BufferedLowLevelFile::Read(void* data, OpFileLength len, OpFileLength* bytes_read)
{
	OP_STATUS status;

	RETURN_IF_ERROR(EnsureValidFileLength());
	RETURN_IF_ERROR(EnsureValidVirtualFilePos());

	if (!bytes_read)
		return OpStatus::ERR_NULL_POINTER;

	status = BufferedRead(data, len, bytes_read);
	if (OpStatus::IsError(status))
	{
		m_physical_file_pos = m_virtual_file_pos = BAD_FILE_POS;
		m_buffer_start = m_buffer_end = BAD_FILE_POS;
	}
	return status;
}

OP_STATUS BufferedLowLevelFile::ReadLine(char** data)
{
	// Not implemented, so just invalidate everything and call underlying file
	m_physical_file_pos = m_virtual_file_pos = BAD_FILE_POS;
	m_buffer_start = m_buffer_end = BAD_FILE_POS;
	return m_f->ReadLine(data);
}

OpLowLevelFile* BufferedLowLevelFile::CreateCopy()
{
	OpLowLevelFile* copy;
	OpLowLevelFile* unbuffered_copy = m_f->CreateCopy();
	if (!unbuffered_copy)
		return NULL;
	OP_STATUS res;
	copy = BufferedLowLevelFile::Create(unbuffered_copy, res, m_buffer_size);
	if (!OpStatus::IsSuccess(res))
	{
		OP_DELETE(unbuffered_copy);
		return NULL;
	}
	return copy;
}

OpLowLevelFile* BufferedLowLevelFile::CreateTempFile(const uni_char* prefix)
{
	OpLowLevelFile* temp;
	OpLowLevelFile* unbuffered_temp = m_f->CreateTempFile(prefix);
	if (!unbuffered_temp)
		return NULL;
	OP_STATUS res;
	temp = BufferedLowLevelFile::Create(unbuffered_temp, res, m_buffer_size);
	if (!OpStatus::IsSuccess(res))
	{
		OP_DELETE(unbuffered_temp);
		return NULL;
	}
	return temp;
}

OP_STATUS BufferedLowLevelFile::SafeClose()
{
	m_physical_file_pos = m_virtual_file_pos = BAD_FILE_POS;
	m_buffer_start = m_buffer_end = BAD_FILE_POS;
	m_file_length = BAD_FILE_POS;
	return m_f->SafeClose();
}

OP_STATUS BufferedLowLevelFile::Flush()
{
	OP_STATUS s;
	s = m_f->Flush();
	m_physical_file_pos = BAD_FILE_POS;
	return s;
}

OP_STATUS BufferedLowLevelFile::SetFileLength(OpFileLength len)
{
	OP_STATUS status;

	OP_ASSERT(len != BAD_FILE_POS);
	if (len == m_file_length)
		return OpStatus::OK;

	m_file_length = len;
	m_physical_file_pos = BAD_FILE_POS;
	m_virtual_file_pos = BAD_FILE_POS;
	if (m_buffer_start != BAD_FILE_POS && m_buffer_start >= len)
		m_buffer_start = m_buffer_end = BAD_FILE_POS;
	if (m_buffer_end != BAD_FILE_POS && m_buffer_end > len)
		m_buffer_end = len;

	status = m_f->SetFileLength(len);
	if (OpStatus::IsError(status)) {
		m_buffer_start = m_buffer_end = BAD_FILE_POS;
		m_file_length = BAD_FILE_POS;
	}

	return status;
}

OP_STATUS BufferedLowLevelFile::BufferVirtualFilePos()
{
	OP_STATUS status;
	OpFileLength start, bytes_read;

	if (m_buffer_start != BAD_FILE_POS && m_virtual_file_pos >= m_buffer_start && m_virtual_file_pos < m_buffer_end)
		return OpStatus::OK;

	// Buffers are aligned on m_buffer_size boundaries
	start = (m_virtual_file_pos/m_buffer_size)*m_buffer_size;
	
	RETURN_IF_ERROR(EnsurePhysicalFilePos(start, IO_read));
	status = m_f->Read(m_buffer, m_buffer_size, &bytes_read);
	if (OpStatus::IsSuccess(status))
	{
		m_buffer_start = start;
		m_buffer_end = start+bytes_read;
		m_physical_file_pos = m_buffer_end;
		
		if (m_virtual_file_pos >= m_buffer_end) // Unable to buffer the actual m_virtual_file_pos
			return OpStatus::ERR;
	}
	else
	{
		m_physical_file_pos = m_virtual_file_pos = BAD_FILE_POS;
		m_buffer_start = m_buffer_end = BAD_FILE_POS;
	}

	return status;
}

OP_STATUS BufferedLowLevelFile::BufferedRead(void* data, OpFileLength len, OpFileLength* bytes_read)
{
	OP_STATUS status;
	OpFileLength size;
	*bytes_read = 0;
	while (len > 0 && m_virtual_file_pos < m_file_length)
	{
		status = BufferVirtualFilePos();
		if (OpStatus::IsError(status))
			return *bytes_read > 0 ? OpStatus::OK : status;
		size = len;
		if (size > m_buffer_end - m_virtual_file_pos)
			size = m_buffer_end - m_virtual_file_pos;
		op_memcpy(data, m_buffer + (m_virtual_file_pos - m_buffer_start), (size_t)size);
		data = (char*)data + size;
		len -= size;
		m_virtual_file_pos += size;
		*bytes_read += size;
	}
	return OpStatus::OK;
}

#undef BAD_FILE_POS
#undef OPF_IS_NEGATIVE

