/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "blockfile.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/desktop_util/opfile/desktop_opfile.h"
#include "modules/util/path.h"

#define BUFSIZE_INTERNAL 2048

/***********************************************************************************
**
**	BlockFile::BlockFile
**
***********************************************************************************/

BlockFile::BlockFile() :
	m_header_size(0),
	m_block_size(0),
	m_block_count(0),
	m_file(NULL),
	m_listener(NULL),
	m_buf(NULL),
	m_buf_filepos((OpFileLength)-1),
	m_bufsize(0),
	m_pos((OpFileLength)-1),
	m_use_buf(TRUE)
{
	if(m_use_buf)
	{
		m_buf = OP_NEWA(char, BUFSIZE_INTERNAL);
	}
}

BlockFile::~BlockFile()
{
	OP_DELETE(m_file);
	OP_DELETEA(m_buf);
}

/***********************************************************************************
**
**	BlockFile::Init
**
***********************************************************************************/

OP_STATUS BlockFile::Init(const uni_char* path, const uni_char* name, OpFileLength header_size, OpFileLength block_size, BOOL append_size_extension, BOOL* existed, BOOL readonly)
{
	RETURN_IF_ERROR(m_path.Set(path));
	RETURN_IF_ERROR(m_name.Set(name));
	m_block_size = block_size;
	m_header_size = (INT32)header_size + 4;	// the 4 very first bytes are used by blockfile to store block count

	BOOL exists;
	// start of code to upgrade _x files to not have extension is so is desired.. remove this after a week
	if (!append_size_extension)
	{
		OpFile* file = CreateBlockFile(path, name, block_size, TRUE);

		if (file)
		{
			if (file->Exists(exists)==OpStatus::OK && exists)
			{
				OpFile* file_without_size_extension = CreateBlockFile(path, name, block_size, FALSE);
				if (file_without_size_extension)
					OpStatus::Ignore(DesktopOpFileUtils::Move(file, file_without_size_extension)); // FIXME: OOM
				OP_DELETE(file_without_size_extension);
			}
		}

		OP_DELETE(file);
	}
	// end of upgrade..

	// Delete existing m_file
	OP_DELETE(m_file);

	m_file = CreateBlockFile(path, name, block_size, append_size_extension);

	if (!m_file)
	{
		return OpStatus::ERR;
	}

	OP_STATUS ret;
	if ((ret=m_file->Exists(exists)) != OpStatus::OK)
	{
        return ret;
	}

	if (existed)
		*existed = exists;

	int mode = OPFILE_SHAREDENYWRITE;
	
	if(!readonly)
	{
		if (exists)
		{
			mode |= OPFILE_UPDATE;
		}
		else
		{
			mode |= OPFILE_OVERWRITE;
		}
	}
	else
	{
		mode |= OPFILE_READ;
	}

	RETURN_IF_ERROR(m_file->Open(mode));

	OP_ASSERT(m_file->IsOpen());

	if (ReadValue(m_block_count) != OpStatus::OK)
	{
		RETURN_IF_ERROR(SetBlockCount(0));
	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**	BlockFile::CreateBlockFile
**
***********************************************************************************/

/*static*/ OpFile* BlockFile::CreateBlockFile(const uni_char* path, const uni_char* name, OpFileLength block_size, BOOL append_size_extension)
{
	OpString full_path;

	OpString filename_without_size_extension;
	filename_without_size_extension.AppendFormat(UNI_L("%s.dat"), name);

	// AppendVFormat does not reliably support 64 bit (will always fail on PPC) so we must
	// use the system interface to get the 64-bit value as a string.
	OpString8 tmp;
	g_op_system_info->OpFileLengthToString(block_size, &tmp);
	OpString block_size_string;
	block_size_string.Set(tmp.CStr());

	OpString filename_with_size_extension;
	filename_with_size_extension.AppendFormat(UNI_L("%s_%s.dat"), name, block_size_string.CStr());

	
	if (OpStatus::IsError(OpPathDirFileCombine(full_path, OpStringC(path), append_size_extension ? filename_with_size_extension : filename_without_size_extension)))
		return NULL;
	
	OpFile* file = OP_NEW(OpFile, ());
	if (!file)
		return NULL;
	
	if (OpStatus::IsError(file->Construct(full_path.CStr())))
	{
		OP_DELETE(file);
		return NULL;
	}
	
	return file;
}

/***********************************************************************************
**
**	BlockFile::CreateBackup
**
***********************************************************************************/

OP_STATUS BlockFile::CreateBackup(BlockFile*& backup_file, OpFileLength header_size, OpFileLength block_size)
{
	backup_file = OP_NEW(BlockFile, ());

	if (!backup_file)
		return OpStatus::ERR_NO_MEMORY;

	OpString backup_name;
	backup_name.AppendFormat(UNI_L("%s_tmp"), m_name.CStr());

	return backup_file->Init(m_path.CStr(), backup_name.CStr(), header_size, block_size);
}

/***********************************************************************************
**
**	BlockFile::ReplaceWithBackup
**
***********************************************************************************/

OP_STATUS BlockFile::ReplaceWithBackup(BlockFile* backup_file)
{
	BOOL was_open = m_file->IsOpen();

	if (m_file->Close()!=OpStatus::OK ||
		backup_file->GetFile()->Close()!=OpStatus::OK)
	{
		OP_ASSERT(0);
		OP_DELETE(backup_file);
		return OpStatus::ERR;
	}

	OP_STATUS ret;
	if ((ret=m_file->SafeReplace(backup_file->GetFile())) != OpStatus::OK)
	{
		OP_ASSERT(0);
	    OP_DELETE(backup_file);
        return ret;
	}

	m_header_size = backup_file->m_header_size;
	m_block_size = backup_file->GetBlockSize();
	m_block_count = backup_file->GetBlockCount();

    OP_DELETE(backup_file);

	if (was_open)
	{
		RETURN_IF_ERROR(m_file->Open(OPFILE_UPDATE|OPFILE_SHAREDENYWRITE));
	}

	return OpStatus::OK;
}


/***********************************************************************************
**
**	BlockFile::RemoveFile
**
***********************************************************************************/
OP_STATUS BlockFile::RemoveFile()
{
	RETURN_IF_ERROR(m_file->Close());
	
	return m_file->Delete();
}

/***********************************************************************************
**
**	BlockFile::Flush
**
***********************************************************************************/

OP_STATUS BlockFile::Flush()
{
	if(m_use_buf)
	{
		MarkBufferInvalid();
	}

	return m_file->Flush();
}

/***********************************************************************************
**
**	BlockFile::SafeFlush
**
***********************************************************************************/

OP_STATUS BlockFile::SafeFlush()
{
	BOOL was_open = m_file->IsOpen();

	if(m_use_buf)
	{
		MarkBufferInvalid();
	}

	RETURN_IF_ERROR(m_file->SafeClose());

	if (was_open)
	{
		RETURN_IF_ERROR(m_file->Open(OPFILE_UPDATE|OPFILE_SHAREDENYWRITE));
	}

	return OpStatus::OK;
}


/***********************************************************************************
**
**	BlockFile::SetBlockCount
**
***********************************************************************************/

OP_STATUS BlockFile::SetBlockCount(INT32 block_count)
{
	OP_ASSERT(m_file->IsOpen());

	OpFileLength file_length;
	m_file->GetFileLength(file_length);

	OpFileLength needed_length = (block_count + 1) * m_block_size + m_header_size;

	if (needed_length < 4096)
	{
		needed_length = 4096;
	}

	if (file_length < needed_length || file_length > needed_length * 3)
	{
		needed_length *= 2;

		for (INT32 i = 1; (needed_length & (~i)) > 0; i <<= 1)
		{
			needed_length &= (~i);
		}

		RETURN_IF_ERROR(m_file->SetFileLength(needed_length));

		RETURN_IF_ERROR(SafeFlush());
	}

	RETURN_IF_ERROR(m_file->SetFilePos(0));

	if(m_use_buf)
	{
		MarkBufferInvalid();
	}

	RETURN_IF_ERROR(WriteValue(block_count));

	m_block_count = block_count;

	return OpStatus::OK;
}

/***********************************************************************************
**
**	BlockFile::CreateBlock
**
***********************************************************************************/

OP_STATUS BlockFile::CreateBlock(INT32& block_index)
{
	block_index = m_block_count;
	return SetBlockCount(m_block_count + 1);
}

/***********************************************************************************
**
**	BlockFile::DeleteBlock
**
***********************************************************************************/

OP_STATUS BlockFile::DeleteBlock(INT32 block_index)
{
	OP_ASSERT(block_index < m_block_count);

	// if not deleting last block, we must move last block to take space of deleted block

	if (block_index != m_block_count - 1)
	{
		OpString8 buffer;

		if (!buffer.Reserve((int)m_block_size))
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(Seek(m_block_count - 1));
		RETURN_IF_ERROR(Read(buffer.CStr(), m_block_size));
		RETURN_IF_ERROR(Seek(block_index));
		RETURN_IF_ERROR(Write(buffer.CStr(), m_block_size));
	}

	RETURN_IF_ERROR(SetBlockCount(m_block_count - 1));

	if (block_index != m_block_count && m_listener)
	{
		m_listener->OnBlockMoved(this, m_block_count, block_index);
	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**	BlockFile::GetCurrentBlock
**
***********************************************************************************/

INT32 BlockFile::GetCurrentBlock()
{
	if(!m_use_buf || m_pos + 1 == 0)
	{
		OpFileLength pos;
		m_file->GetFilePos(pos);
		return (INT32)((INT32)pos - m_header_size) / (INT32)m_block_size;
	}
	else
	{
		if (m_buf_filepos + 1 == 0)
		{
			m_file->GetFilePos(m_buf_filepos);
		}
		return (INT32)((INT32)m_buf_filepos - (INT32)m_bufsize + (INT32)m_pos - m_header_size) / (INT32)m_block_size;
	}
}

/***********************************************************************************
**
**	BlockFile::GetCurrentOffset
**
***********************************************************************************/

OpFileLength BlockFile::GetCurrentOffset()
{
	if(!m_use_buf || m_pos + 1 == 0)
	{
		OpFileLength pos;
		m_file->GetFilePos(pos);
		return (pos - m_header_size) % m_block_size;
	}
	else
	{
		if (m_buf_filepos + 1 == 0)
		{
			m_file->GetFilePos(m_buf_filepos);
		}
		return (m_buf_filepos - m_bufsize + m_pos - m_header_size) % m_block_size;
	}
}

/***********************************************************************************
**
**	BlockFile::ReadHeaderValue
**
***********************************************************************************/

OP_STATUS BlockFile::ReadHeaderValue(OpFileLength header_offset, INT32& value)
{
	if(m_use_buf)
	{
		MarkBufferInvalid();
	}

	RETURN_IF_ERROR(m_file->SetFilePos(header_offset + 4));

	return ReadValue(value);
}

/***********************************************************************************
**
**	BlockFile::WriteHeaderValue
**
***********************************************************************************/

OP_STATUS BlockFile::WriteHeaderValue(OpFileLength header_offset, INT32 value)
{
	if(m_use_buf)
	{
		MarkBufferInvalid();
	}

	RETURN_IF_ERROR(m_file->SetFilePos(header_offset + 4));

	return WriteValue(value);
}

/***********************************************************************************
**
**	BlockFile::Seek
**
***********************************************************************************/

OP_STATUS BlockFile::Seek(INT32 block_index, OpFileLength block_offset)
{
	OP_ASSERT(m_file->IsOpen());

	if(m_use_buf)
	{
		MarkBufferInvalid();
	}

	return m_file->SetFilePos(block_index * m_block_size + block_offset + m_header_size);
}

/***********************************************************************************
**
**	BlockFile::Read
**
***********************************************************************************/

OP_STATUS BlockFile::Read(void* buffer, OpFileLength length)
{
	OP_ASSERT(m_file->IsOpen());
	OP_ASSERT(length < 1000000);

	if(m_use_buf)
	{
		if(m_pos + 1 == 0)
		{
			if(InitBuffer() != OpStatus::OK)
			{
				OP_ASSERT(0);
				return OpStatus::ERR;
			}
		}

		OP_ASSERT(m_pos + 1 != 0);

		OP_STATUS s = ReadData(buffer,length);
		OP_ASSERT(s == OpStatus::OK);
		return s;
	}
	else
	{
		OpFileLength bytes_read = 0;
		return m_file->Read(buffer, length, &bytes_read);
	}
}

/***********************************************************************************
**
**	BlockFile::Write
**
***********************************************************************************/

OP_STATUS BlockFile::Write(void* buffer, OpFileLength length)
{
	OP_ASSERT(m_file->IsOpen());
	OP_ASSERT(length < 1000000);

	if(m_use_buf)
	{
		if(SeekBack() != OpStatus::OK)
		{
			return OpStatus::ERR;
		}
	}

	return m_file->Write(buffer, length);
}

/***********************************************************************************
**
**	BlockFile::InitBuffer
**
***********************************************************************************/

OP_STATUS BlockFile::InitBuffer()
{
	OP_ASSERT(m_pos + 1 == 0);
	OP_ASSERT(m_bufsize == 0);

	OpFileLength nread;
	if(m_file->Read(m_buf, BUFSIZE_INTERNAL, &nread) != OpStatus::OK)
	{
		if(nread > 0)
		{
			m_pos = 0;
			m_bufsize = nread;
			return OpStatus::OK;
		}
		return OpStatus::ERR;
	}

	//OP_ASSERT(nread == BUFSIZE_INTERNAL);

	m_pos = 0;
	m_bufsize = nread;

	return OpStatus::OK;
}

/***********************************************************************************
**
**	BlockFile::SeekBack
**
***********************************************************************************/

OP_STATUS BlockFile::SeekBack()
{
	if(m_pos + 1 != 0)
	{
		OpFileLength pos;
		if(m_file->GetFilePos(pos)!=OpStatus::OK ||
		   (pos + m_pos) < (OpFileLength)(m_bufsize) ||
		   m_file->SetFilePos(pos - m_bufsize + m_pos)!=OpStatus::OK)
		{
			OP_ASSERT(0);
			return OpStatus::ERR;
		}
		MarkBufferInvalid();
	}
	return OpStatus::OK;
}

/***********************************************************************************
**
**	BlockFile::ReadData
**
***********************************************************************************/

OP_STATUS BlockFile::ReadData(void *p, OpFileLength len)
{
	OP_ASSERT(m_use_buf);
	OP_ASSERT(len < 1000000);

	if(len > m_bufsize)
	{
		if(SeekBack() != OpStatus::OK)
		{
			return OpStatus::ERR;
		}

		OpFileLength bytes_read;
		if (m_file->Read(p, len, &bytes_read) != OpStatus::OK)
		{
			return OpStatus::ERR;
		}

		if (bytes_read < len)
			return OpStatus::ERR_OUT_OF_RANGE;

		return OpStatus::OK;
	}

	if(m_pos + len > m_bufsize)
	{
		if(SeekBack() != OpStatus::OK)
		{
			return OpStatus::ERR;
		}

		if(InitBuffer() != OpStatus::OK)
		{
			OP_ASSERT(0);
			return OpStatus::ERR;
		}

	}

	return ReadBuffer(p,len);
}

/***********************************************************************************
**
**	BlockFile::ReadBuffer
**
***********************************************************************************/

OP_STATUS BlockFile::ReadBuffer(void *p, OpFileLength len)
{
	OP_ASSERT(m_use_buf);
	OP_ASSERT(len <= m_bufsize);
	OP_ASSERT(m_pos + 1 != 0);
	OP_ASSERT(m_pos + len <= m_bufsize);
	OP_ASSERT(len < 1000000);

	char *c = (m_buf + m_pos);
	memcpy(p, c, (size_t)len);
	m_pos += len;

	return OpStatus::OK;
}

/***********************************************************************************
**
**	BlockFile::MarkBufferInvalid
**
***********************************************************************************/

void BlockFile::MarkBufferInvalid()
{
	m_pos = (OpFileLength)-1;
	m_bufsize = 0;
	m_buf_filepos = (OpFileLength)-1;
}

/***********************************************************************************
**
**	BlockFile::ReadValue
**
***********************************************************************************/

OP_STATUS BlockFile::ReadValue(INT32& value)
{
	OP_ASSERT(m_file->IsOpen());

	if(!m_use_buf)
	{
		long value_long;
		if (m_file->ReadBinLong(value_long) != OpStatus::OK)
		{
			return OpStatus::ERR;
		}

		value = value_long;

		return OpStatus::OK;
	}
	else
	{
		if(m_pos + 1 == 0)
		{
			if(InitBuffer() != OpStatus::OK)
			{
				//OP_ASSERT(0);
				return OpStatus::ERR;
			}
		}

		unsigned char c[4];
		OP_STATUS s = ReadData(c,4);
		//OP_ASSERT(s == OpStatus::OK);
		if(s != OpStatus::OK)
		{
			return s;
		}

		value = (c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3];

		return OpStatus::OK;
	}
}

OP_STATUS BlockFile::ReadValue(INT32 block_index, OpFileLength block_offset, INT32& value)
{
	RETURN_IF_ERROR(Seek(block_index, block_offset));
	return ReadValue(value);
}

/***********************************************************************************
**
**	BlockFile::WriteValue
**
***********************************************************************************/

OP_STATUS BlockFile::WriteValue(INT32 value)
{
	OP_ASSERT(m_file->IsOpen());

	if(m_use_buf)
	{
		if(SeekBack() != OpStatus::OK)
		{
			return OpStatus::ERR;
		}
	}

	return m_file->WriteBinLong(value);
}

OP_STATUS BlockFile::WriteValue(INT32 block_index, OpFileLength block_offset, INT32 value)
{
	RETURN_IF_ERROR(Seek(block_index, block_offset));
	return WriteValue(value);
}

/***********************************************************************************
**
**	BlockFile::ReadString
**
***********************************************************************************/

OP_STATUS BlockFile::ReadString(OpString& string, BOOL read_as_char)
{
	OP_ASSERT(m_file->IsOpen());

	if(m_use_buf)
	{
		if(m_pos + 1 == 0)
		{
			if(InitBuffer() != OpStatus::OK)
			{
				OP_ASSERT(0);
				return OpStatus::ERR;
			}
		}

		OP_ASSERT(m_pos + 1 != 0);
		OP_ASSERT(m_pos <= m_bufsize);

		if(m_pos == m_bufsize)
		{
			MarkBufferInvalid();

			if(InitBuffer() != OpStatus::OK)
			{
				OP_ASSERT(0);
				return OpStatus::ERR;
			}
		}

		OP_ASSERT(m_pos < m_bufsize);

		char *p = m_buf + m_pos;
		char tmp[1024 + 1];
		int i;

		for(i=0; (p < m_buf + m_bufsize) && (i<1024) && (*p != 0) && (*p != '\n'); i++, p++)
		{
			tmp[i] = *p;
		}

		tmp[i] = 0;

		//OP_ASSERT(i <= 1024);

		if(p >= m_buf + m_bufsize)
		{
			if(SeekBack() != OpStatus::OK)
			{
				return OpStatus::ERR;
			}

			if (read_as_char)
			{
				OP_STATUS ret;
				OpString8 tmp8;
				if ((ret=m_file->ReadLine(tmp8))!=OpStatus::OK)
					return ret;

				return string.Set(tmp8);
			}
			else
			{
				return m_file->ReadUTF8Line(string);
			}
		}

		OP_ASSERT(p < m_buf + m_bufsize);

		OP_ASSERT(m_pos + i <= m_bufsize);

		m_pos += i;

		if((*p == 0) || (*p == '\n'))
		{
			m_pos++;
		}

#ifdef UNICODE
		if (!read_as_char)
		{
			string.SetFromUTF8(tmp);
		}
		else
#endif //UNICODE
		{
			string.Set(tmp);
		}

		return OpStatus::OK;
	}
	else
	{
		if (read_as_char)
		{
			OP_STATUS ret;
			OpString8 tmp8;
			if ((ret=m_file->ReadLine(tmp8))!=OpStatus::OK)
				return ret;

			return string.Set(tmp8);
		}
		else
		{
			return m_file->ReadUTF8Line(string);
		}
	}
}

OP_STATUS BlockFile::ReadString(INT32 block_index, OpFileLength block_offset, OpString& string)
{
	RETURN_IF_ERROR(Seek(block_index, block_offset));
	return ReadString(string);
}

/***********************************************************************************
**
**	BlockFile::WriteString
**
***********************************************************************************/

OP_STATUS BlockFile::WriteString(const uni_char* string)
{
	OP_ASSERT(m_file->IsOpen());

	if(m_use_buf)
	{
		if(SeekBack() != OpStatus::OK)
		{
			return OpStatus::ERR;
		}
	}

	return m_file->WriteUTF8Line(string);
}

OP_STATUS BlockFile::WriteString(INT32 block_index, OpFileLength block_offset, const uni_char* string)
{
	RETURN_IF_ERROR(Seek(block_index, block_offset));
	return WriteString(string);
}


/***********************************************************************************
**
**	BlockFile::WriteString8
**
***********************************************************************************/

OP_STATUS BlockFile::WriteString8(const char* string)
{
	OP_ASSERT(m_file->IsOpen());

	if(m_use_buf)
	{
		if(SeekBack() != OpStatus::OK)
		{
			return OpStatus::ERR;
		}
	}

	OP_STATUS ret;
	if ((ret=m_file->Write(string, strlen(string)))!=OpStatus::OK ||
		(ret=m_file->WriteBinByte('\n'))!=OpStatus::OK)
	{
		return ret;
	}

	return OpStatus::OK;
}

OP_STATUS BlockFile::WriteString8(INT32 block_index, OpFileLength block_offset, const char* string)
{
	RETURN_IF_ERROR(Seek(block_index, block_offset));
	return WriteString8(string);
}

#endif //M2_SUPPORT
