/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifndef HAVE_NO_OPMEMFILE

#include "modules/util/opfile/opfile.h"
#include "modules/util/opfile/opmemfile.h"

OpMemFile::OpMemFile() :
	m_data(NULL),
	m_size(0),
	m_pos(0),
	m_file_len(0),
	m_open(FALSE),
	m_filename(NULL)
{
}

OpMemFile::~OpMemFile()
{
	OP_DELETEA(m_data);
	OP_DELETEA(m_filename);
}

OpMemFile*
OpMemFile::Create(unsigned char* data, OpFileLength size, BOOL take_over, const uni_char *name)
{
	OpMemFile* file = OP_NEW(OpMemFile, ());
	if (!file)
	{
		return NULL;
	}

	unsigned int len = name ? uni_strlen(name)+1 : 1;
	file->m_filename = OP_NEWA(uni_char, len);
	if (!file->m_filename)
	{
		OP_DELETE(file);
		return NULL;
	}

	if (name)
		uni_strcpy(file->m_filename, name);
	else
		file->m_filename[0] = 0;

	if (take_over)
	{
		file->m_size = size;
		file->m_file_len = size;
		file->m_data = data;
	}
	else if ((size_t)size)
	{
		file->m_data = OP_NEWA(unsigned char, (size_t)size);
		if (file->m_data == NULL)
		{
			OP_DELETE(file);
			return NULL;
		}
	
		file->m_size = size;
		file->m_file_len = size;
		op_memcpy(file->m_data, data, (size_t)size);
	}
	return file;
}

OpMemFile*
OpMemFile::Create(const unsigned char* data, OpFileLength size, const uni_char *name)
{
	OpMemFile* file = OP_NEW(OpMemFile, ());
	if (!file)
	{
		return NULL;
	}

	if ((size_t)size)
	{
		file->m_data = OP_NEWA(unsigned char, (size_t)size);
		if (file->m_data == NULL)
		{
			OP_DELETE(file);
			return NULL;
		}
	
		file->m_size = size;
		file->m_file_len = size;
		op_memcpy(file->m_data, data, (size_t)size);
	}
	return file;
}

OP_STATUS
OpMemFile::Open(int mode)
{
	m_pos = 0;
	m_open = TRUE;
	return OpStatus::OK;
}

BOOL
OpMemFile::IsOpen() const
{
	return m_open;
}

OP_STATUS
OpMemFile::Close()
{
	m_open = FALSE;
	m_pos = 0;
	return OpStatus::OK;
}

BOOL
OpMemFile::Eof() const
{
	return m_pos == m_file_len;
}

OP_STATUS
OpMemFile::Exists(BOOL& exists) const
{
	exists = TRUE;
	return OpStatus::OK;
}

OP_STATUS
OpMemFile::GetFilePos(OpFileLength& pos) const
{
	pos = m_pos;
	return OpStatus::OK;
}

OP_STATUS
OpMemFile::SetFilePos(OpFileLength pos, OpSeekMode seek_mode)
{
	switch(seek_mode)
	{
	case SEEK_FROM_START:
		{
			OP_ASSERT(pos <= m_size);
			m_pos = pos;
			if (m_pos > m_file_len)
				m_file_len = m_pos;
		}
		break;
	case SEEK_FROM_END:
		{
			OP_ASSERT(pos <= m_file_len);
			if (pos <= m_file_len)
				m_pos = m_file_len - pos;
			else
				m_pos = 0;
		}
		break;
	case SEEK_FROM_CURRENT:
		{
			OP_ASSERT((m_pos+pos) <= m_size);
			m_pos += pos;
			if (m_pos > m_file_len)
				m_file_len = m_pos;
		}
		break;
	default: OP_ASSERT(!"Unknown mode");
	}

	return OpStatus::OK;
}

OP_STATUS 
OpMemFile::GrowIfNeeded(OpFileLength desired_size)
{
	if (desired_size > m_size)
	{
		OpFileLength new_size = MAX(m_size * 2, desired_size);
		return Resize(new_size);
	}

	return OpStatus::OK;
}

OP_STATUS 
OpMemFile::Resize(OpFileLength new_size)
{
	// Create a new buffer of the size we need.
	unsigned char* new_buffer = OP_NEWA(unsigned char, (size_t)new_size);
	if (new_buffer == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	// Copy the old content into the new buffer, then delete it.
	if (m_size > 0)
	{
		op_memcpy(new_buffer, m_data, (size_t)m_size);
		OP_DELETEA(m_data);
	}
	m_size = new_size;
	m_data = new_buffer;
	return OpStatus::OK;
}

OP_STATUS
OpMemFile::Write(const void* data, OpFileLength len)
{
	OP_STATUS result = OpStatus::OK;

	RETURN_IF_ERROR(GrowIfNeeded(len + m_pos));
	
	op_memcpy(m_data + m_pos, data, (size_t)len);

	m_pos += (size_t)len;
	if (m_pos > m_file_len)
		m_file_len = (size_t)m_pos;

	return result;
}

OP_STATUS
OpMemFile::Read(void* data, OpFileLength len, OpFileLength* bytes_read)
{
	OP_STATUS result = OpStatus::OK;
	size_t n = 0;

	if (!bytes_read)
		return OpStatus::ERR_NULL_POINTER;
	if ((size_t)m_pos + (size_t)len > (size_t)m_file_len) //check for EOF
	{
		size_t readbytes = (size_t)(m_file_len - m_pos);
		if (readbytes > 0)
		{
			op_memcpy(data, m_data + (size_t)m_pos, readbytes);
			n = readbytes;
		}
	}
	else //go ahead and read as much as you want
	{
		op_memcpy(data, m_data + m_pos, (size_t)len);
		n = (size_t)len;
	}
	m_pos += n;
	*bytes_read = n;
	return result;
}

OP_STATUS
OpMemFile::ReadLine(OpString8& data)
{
	const unsigned char* start = (const unsigned char*)m_data + m_pos;
	const unsigned char* p = start;
	const unsigned char* end = m_data + m_file_len;
	while (p < end && *p != '\n')
	{
		++p;
	}

	size_t bytes_to_read = (size_t)(p - m_data - (size_t)m_pos);
	char* new_data = OP_NEWA(char, bytes_to_read + 1);
	if (new_data == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	op_memcpy(new_data, start, bytes_to_read); // Shouldn't include new line character.
	new_data[bytes_to_read] = '\0';
	data.TakeOver(new_data);

	if (p < end )
	{
		bytes_to_read++; // Skip past new line character.
	}

	m_pos += bytes_to_read;

	return OpStatus::OK;
}

OP_STATUS
OpMemFile::Copy(const OpFileDescriptor* copy)
{
	OpMemFile* cp = (OpMemFile*)copy;

	if ((size_t)m_size && m_data)
		OP_DELETEA(m_data);

	if ((size_t)cp->m_size)
	{
		m_data = OP_NEWA(unsigned char, (size_t)cp->m_file_len);
		if (!m_data)
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		op_memcpy(m_data, cp->m_data, (size_t)cp->m_file_len);
	}
	m_size = cp->m_file_len;
	m_file_len = cp->m_file_len;

	return OpStatus::OK;
}

OpFileDescriptor*
OpMemFile::CreateCopy() const
{
	return OpMemFile::Create(m_data, m_file_len);
}

#endif // HAVE_NO_OPMEMFILE
