/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

// Implementation of OpSafeFile, a class for overwriting a file in a safe manner.

#include "core/pch.h"

#include "modules/datastream/opdatastreamsafefile.h"

#define MAX_DS_BUFFER_SIZE 8192

OpDataStreamSafeFile::OpDataStreamSafeFile()
:m_buffer_size(0)
,m_max_buffer_size(MAX_DS_BUFFER_SIZE)
{
}

OpDataStreamSafeFile::~OpDataStreamSafeFile()
{
	OP_DELETEA(m_buffer);
}

OP_STATUS
OpDataStreamSafeFile::Construct(const OpStringC& path, OpFileFolder folder, int flags)
{
	m_buffer = OP_NEWA(unsigned char, (size_t) m_max_buffer_size + 1);
	if (!m_buffer)
		return OpStatus::ERR_NO_MEMORY;

	return OpSafeFile::Construct(path, folder, flags);
}

OP_STATUS
OpDataStreamSafeFile::Construct(OpFile* file)
{
	m_buffer = OP_NEWA(unsigned char, (size_t) m_max_buffer_size + 1);
	if (!m_buffer)
		return OpStatus::ERR_NO_MEMORY;

	return OpSafeFile::Construct(file);
}

OP_STATUS OpDataStreamSafeFile::Write(const void* data, OpFileLength len)
{
	OpFileLength copysize = len > (m_max_buffer_size - m_buffer_size) ? m_max_buffer_size - m_buffer_size : len;
	op_memcpy(m_buffer + m_buffer_size, (const char*)data, (size_t)copysize);
	m_buffer_size += copysize;
	if (len != copysize)
	{
		// buffer is full, write to disk.
		RETURN_IF_ERROR(OpFile::Write(m_buffer, m_buffer_size));
		m_buffer_size = 0;

		if ((len - copysize) >= m_max_buffer_size)
		{
			//if more remaining in buffer than we have space for we write this to file also.
			RETURN_IF_ERROR(OpFile::Write((char*)data + copysize, len - copysize));
		}
		else
		{
			op_memcpy(m_buffer, (const char*)data + copysize, (size_t)(len - copysize));
			m_buffer_size += len - copysize;
		}

	}
	return OpStatus::OK;
}

OP_STATUS OpDataStreamSafeFile::SafeClose()
{
	if (m_buffer_size)
		RETURN_IF_ERROR(OpFile::Write(m_buffer, m_buffer_size));

	return OpSafeFile::SafeClose();
}

OP_STATUS OpDataStreamSafeFile::Read(void* data, OpFileLength len, OpFileLength* bytes_read)
{
	return OpFile::Read(data, len, bytes_read);
}

