/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

//  Description				: Encapsulates i/o towards zip-archives
//  Documentation found at	: http://www.pkware.com/products/enterprise/white_papers/appnote.html

#include "core/pch.h"

#ifdef USE_ZLIB

#include "modules/util/zipload.h"

#if defined DIRECTORY_SEARCH_SUPPORT && defined ZIPFILE_DIRECT_ACCESS_SUPPORT
#include "modules/util/glob.h"
#endif // DIRECTORY_SEARCH_SUPPORT && ZIPFILE_DIRECT_ACCESS_SUPPORT

#include "modules/stdlib/util/opdate.h"
#include "modules/util/filefun.h"

#ifdef UTIL_ZIP_WRITE
# define COMPRESS_BUFFER_GRAIN	1024
# define RESIZE_BUFFER_LENGTH	1024
#endif // UTIL_ZIP_WRITE

#define ZIP_CRC_IN_DATA_DESCRIPTOR	(1 << 3)	// General purpose bit 3: crc-32, compressed size and uncompressed size can be found in central header
#define ZIP_FILENAME_UTF8			(1 << 11)	// General purpose bit 11: UTF-8 encoded name

/** Macro to ensure the number can be representable in addressable memory */
#if defined THIRTY_TWO_BIT && defined HAVE_UINT64
# define VALIDATE_SIZE(variable, value) \
	UINT32 variable; \
	if ((value) >= 0xFFFFFFFFU) return OpStatus::ERR_NOT_SUPPORTED; \
	variable = static_cast<UINT32>(value);
#else
# define VALIDATE_SIZE(variable, value) \
	UINT64 variable = (value);
#endif

/***********************************************************************************
**
**	OpZipFile
**
***********************************************************************************/

OpZipFile::OpZipFile() :
		m_pData(NULL),
		m_dwPos(0),
		m_dwSize(0)
{
}

/***********************************************************************************
**
**	Init
**
***********************************************************************************/

OP_STATUS OpZipFile::Init(unsigned char* pData, unsigned long dwSize)
{
	if (!pData)
		return OpStatus::ERR_NULL_POINTER;

	m_pData = pData;
	m_dwSize = dwSize;

	return OpStatus::OK;
}

/***********************************************************************************
**
**	Close
**
***********************************************************************************/

BOOL OpZipFile::Close()
{
    if (!m_pData)
    {
        return TRUE;
    }
    OP_DELETEA(m_pData);
    m_pData = NULL;
    m_dwSize = 0;
    m_dwPos = 0;
    return TRUE;
}

/***********************************************************************************
**
**	Read
**
***********************************************************************************/

OP_STATUS OpZipFile::Read(void* pVoid, unsigned long dwSize, unsigned long* pdwRead)
{
    OP_ASSERT(m_pData);
    if (!pdwRead || !m_pData)
    {
        return OpStatus::ERR_NULL_POINTER;
    }
    if (m_dwPos>m_dwSize)
    {
        return OpStatus::ERR_OUT_OF_RANGE;
    }
    if (m_dwPos+dwSize > m_dwSize)
    {
        dwSize = m_dwPos-m_dwSize;
    }

    op_memcpy(pVoid, m_pData, dwSize);
    m_dwPos += dwSize;
    *pdwRead = dwSize;
    return OpStatus::OK;
}

/***********************************************************************************
**
**	OpZip
**
***********************************************************************************/

OpZip::OpZip(int flags):
	m_thisfile(NULL),
	m_Files(NULL),
	m_DirData(NULL),
	m_sizeofFile(0),
	m_pos(0)
	,m_index_list(NULL)
#ifdef UTIL_ZIP_CACHE
	,m_usage_count(0)
#ifndef CAN_DELETE_OPEN_FILE
	,m_mod_time(0)
#endif // CAN_DELETE_OPEN_FILE
#endif // UTIL_ZIP_CACHE
	,m_flags(flags)
{
}

/***********************************************************************************
**
**	~OpZip
**
***********************************************************************************/

OpZip::~OpZip()
{
	Close();
}

/***********************************************************************************
**
**	Init
**
***********************************************************************************/

OP_STATUS OpZip::ReadUINT32(UINT32& ret)
{
	UINT8 data[4]; /* ARRAY OK 2009-04-24 johanh */
	OpFileLength bytes_read;

	RETURN_IF_ERROR(m_thisfile->Read(&data, 4, &bytes_read));
	if (bytes_read != 4)
		return OpStatus::ERR;

	ret = BytesToUInt32(data);

	return OpStatus::OK;
}

#ifdef UTIL_ZIP_WRITE

OP_STATUS OpZip::UpdateArchive(OpMemFile *mem_file, uint32 data_size, int comp_level)
{
	OpFileLength	original_zip_fileposition;			// To restore before exiting from this func.
	OpFileLength	original_memfile_fileposition; 		// To restore before exiting from this func.
	OpFileLength	bytes_read;
	INT32			entry_index = -1;		// Index of entry to be modified
	unsigned long	crc_val = 0L;
	UINT32			comp_size = 0;
	UINT32			uncomp_size = 0;
	int i;		// for-loop index

	if(!mem_file)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(m_thisfile->GetFilePos(original_zip_fileposition));
	RETURN_IF_ERROR(mem_file->GetFilePos(original_memfile_fileposition));

	// Browse Central Directory to find proper file

	for(i = 0; i<m_end_of_central_directory.entries_here; i++)
	{
		unsigned int len = uni_strlen(mem_file->GetName());
		if (!uni_strncmp(mem_file->GetName(), m_Files[i]->filename, len) && len == m_Files[i]->filename_len)
		{
			entry_index = i;
			break;
		}
	}

	if(entry_index == -1)
	{
		// No matching entry was found, exit without doing anything
		return OpStatus::ERR_FILE_NOT_FOUND;
	}

	// Go to the beginning of the file to read
	if(OpStatus::IsError(mem_file->SetFilePos(0, SEEK_FROM_START)))
	{
		m_thisfile->SetFilePos(original_zip_fileposition);
		mem_file->SetFilePos(original_memfile_fileposition, SEEK_FROM_START);
		return OpStatus::ERR;
	}

	uncomp_size = data_size ? MIN(mem_file->GetFileLength(), data_size) : mem_file->GetFileLength();
	char *source_buffer = (char*)op_malloc(uncomp_size * sizeof(char));
	if(!source_buffer)
	{
		m_thisfile->SetFilePos(original_zip_fileposition);
		mem_file->SetFilePos(original_memfile_fileposition, SEEK_FROM_START);
		return OpStatus::ERR_NO_MEMORY;
	}

	// Read from the memory file
	if(OpStatus::IsError(mem_file->Read(source_buffer, uncomp_size, &bytes_read)) || bytes_read != uncomp_size)
	{
		m_thisfile->SetFilePos(original_zip_fileposition);
		mem_file->SetFilePos(original_memfile_fileposition, SEEK_FROM_START);
		op_free(source_buffer);
		return OpStatus::ERR;
	}

	// Calculate CRC32
	crc_val = crc32(crc_val, (const Bytef*)source_buffer, uncomp_size);

	char* dest_buffer = NULL;

	if (comp_level != Z_NO_COMPRESSION)
	{
		//Compress given buffer
		z_stream  strm;
		size_t	  bufsize = (size_t)(uncomp_size * 1.001 + 12);

		dest_buffer = (char*)op_malloc(bufsize * sizeof(char));	//we use op_malloc to be able to use op_realloc
		op_memset(&strm, 0, sizeof(z_stream));

		if (!dest_buffer)
		{
			m_thisfile->SetFilePos(original_zip_fileposition);
			mem_file->SetFilePos(original_memfile_fileposition, SEEK_FROM_START);
			op_free(source_buffer);
			return OpStatus::ERR_NO_MEMORY;
		}

		// allocate deflate state
		if (deflateInit2(&strm, comp_level, Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY) != Z_OK)
		{
			m_thisfile->SetFilePos(original_zip_fileposition);
			mem_file->SetFilePos(original_memfile_fileposition, SEEK_FROM_START);
			op_free(source_buffer);
			op_free(dest_buffer);
			return OpStatus::ERR;
		}

		int res;
		strm.avail_in  = uncomp_size;
		strm.next_in   = reinterpret_cast<Bytef*>(const_cast<char*>(source_buffer));
		strm.avail_out = bufsize;
		strm.next_out  = reinterpret_cast<Bytef*>(dest_buffer);

		do {
			// compress until we're done

			res = deflate(&strm, Z_FINISH);
			if (strm.avail_out <= 0)
			{
				dest_buffer = (char*)op_realloc(dest_buffer, bufsize + COMPRESS_BUFFER_GRAIN);
				if (!dest_buffer)
				{
					m_thisfile->SetFilePos(original_zip_fileposition);
					mem_file->SetFilePos(original_memfile_fileposition, SEEK_FROM_START);
					op_free(source_buffer);
					return OpStatus::ERR_NO_MEMORY;
				}
				strm.next_out  = reinterpret_cast<Bytef*>(dest_buffer + bufsize);
				strm.avail_out = COMPRESS_BUFFER_GRAIN;
				bufsize += COMPRESS_BUFFER_GRAIN;
			}

		} while (Z_STREAM_END != res);

		// Cleanup after compression
		comp_size = strm.total_out;
		dest_buffer = (char*)op_realloc(dest_buffer, comp_size);
		if (!dest_buffer)
		{
			m_thisfile->SetFilePos(original_zip_fileposition);
			mem_file->SetFilePos(original_memfile_fileposition, SEEK_FROM_START);
			op_free(source_buffer);
			return OpStatus::ERR_NO_MEMORY;
		}

		deflateEnd(&strm);
		op_free(source_buffer);
		source_buffer = NULL;
	}
	else
	{
		// source_buffer will be deallocated later
		comp_size = uncomp_size;
		dest_buffer = source_buffer;
	}

	// This is the difference between new buffer length and old one
	int switch_offset = comp_size - m_Files[entry_index]->comp_size;
	OpFileLength switch_start;
	OpFileLength file_len;
	m_thisfile->GetFileLength(file_len);

	if (comp_size < m_Files[entry_index]->comp_size)
	{
		switch_start = m_Files[entry_index]->local_offset + FILE_HEADER_LENGTH + m_Files[entry_index]->filename_len + m_Files[entry_index]->extra_len + m_Files[entry_index]->comp_size;

		// File doesn't need to be extended, optimistic case
		char switch_buf[RESIZE_BUFFER_LENGTH];	// ARRAY OK 2011-11-15 markuso

		//Rewrite following part of data to proper offset
		while (switch_start < file_len)
		{
			int len = MIN(RESIZE_BUFFER_LENGTH, file_len - switch_start);

			if(	OpStatus::IsError(m_thisfile->SetFilePos(switch_start, SEEK_FROM_START)) ||
				OpStatus::IsError(m_thisfile->Read(switch_buf, len, &bytes_read)) || bytes_read != len ||
				OpStatus::IsError(m_thisfile->SetFilePos(switch_start + switch_offset, SEEK_FROM_START)) ||
				OpStatus::IsError(m_thisfile->Write(switch_buf, len))
				)
			{
				m_thisfile->SetFilePos(original_zip_fileposition);
				mem_file->SetFilePos(original_memfile_fileposition, SEEK_FROM_START);
				op_free(dest_buffer);
				return OpStatus::ERR;
			}
			switch_start += len;
		}

		if (OpStatus::IsError(m_thisfile->SetFileLength(file_len + switch_offset)))
		{
			m_thisfile->SetFilePos(original_zip_fileposition);
			mem_file->SetFilePos(original_memfile_fileposition, SEEK_FROM_START);
			op_free(dest_buffer);
			return OpStatus::ERR;
		}
		m_sizeofFile = file_len + switch_offset;
	}
	else if (comp_size > m_Files[entry_index]->comp_size)
	{
		// File needs to be extended
		OpFileLength new_len = file_len - m_Files[entry_index]->comp_size + comp_size;
		if (OpStatus::IsError(m_thisfile->SetFileLength(new_len)))
		{
			m_thisfile->SetFilePos(original_zip_fileposition);
			mem_file->SetFilePos(original_memfile_fileposition, SEEK_FROM_START);
			op_free(dest_buffer);
			return OpStatus::ERR;
		}
		m_sizeofFile = new_len;

		OpFileLength switch_end = m_Files[entry_index]->local_offset + FILE_HEADER_LENGTH + m_Files[entry_index]->filename_len + m_Files[entry_index]->extra_len + m_Files[entry_index]->comp_size;
		OpFileLength previous_end = file_len;

		char switch_buf[RESIZE_BUFFER_LENGTH]; // ARRAY OK 2011-11-15 markuso
		int len = MIN(RESIZE_BUFFER_LENGTH, previous_end - switch_end);
		while (len > 0)
		{
			if(	OpStatus::IsError(m_thisfile->SetFilePos(previous_end - len, SEEK_FROM_START)) ||
				OpStatus::IsError(m_thisfile->Read(switch_buf, len, &bytes_read)) || bytes_read != len ||
				OpStatus::IsError(m_thisfile->SetFilePos(previous_end - len + switch_offset, SEEK_FROM_START)) ||
				OpStatus::IsError(m_thisfile->Write(switch_buf, len))
				)
			{
				m_thisfile->SetFilePos(original_zip_fileposition);
				mem_file->SetFilePos(original_memfile_fileposition, SEEK_FROM_START);
				op_free(dest_buffer);
				return OpStatus::ERR;
			}
			previous_end -= len;
			len = MIN(RESIZE_BUFFER_LENGTH, previous_end - switch_end);
		}
	}
	else
	{
		// No switching needed since previous and current buffer are the same size
	}

	//Write newly compressed buffer
	switch_start = m_Files[entry_index]->local_offset + FILE_HEADER_LENGTH + m_Files[entry_index]->filename_len + m_Files[entry_index]->extra_len;
	if(	OpStatus::IsError(m_thisfile->SetFilePos(switch_start, SEEK_FROM_START)) ||
		OpStatus::IsError(m_thisfile->Write(dest_buffer, comp_size))
		)
	{
		m_thisfile->SetFilePos(original_zip_fileposition);
		mem_file->SetFilePos(original_memfile_fileposition, SEEK_FROM_START);
		op_free(dest_buffer);
		return OpStatus::ERR;
	}

	op_free(dest_buffer);
	dest_buffer = NULL;

	//Update values
	if (OpStatus::IsError(UpdateOffsets(entry_index + 1, switch_offset)) ||		// offsets should be updated starting from entry following modified one
		OpStatus::IsError(UpdateLength(entry_index, comp_size, uncomp_size)) ||
		OpStatus::IsError(UpdateCRC32(entry_index, crc_val)) ||
		OpStatus::IsError(UpdateCompressionInfo(entry_index, comp_level == Z_NO_COMPRESSION ? COMP_STORE : COMP_DEFLATE, COMP_DEFLATE_OPT_NORMAL))
		)
	{
		m_thisfile->SetFilePos(original_zip_fileposition);
		mem_file->SetFilePos(original_memfile_fileposition, SEEK_FROM_START);
		return OpStatus::ERR;
	}

	m_thisfile->SetFilePos(0);
	mem_file->SetFilePos(0);
	return OpStatus::OK;
}

OP_STATUS OpZip::UpdateOffsets(int from_entry, int offset)
{
	if (offset == 0)
	{
		//no need to modify
		return OpStatus::OK;
	}

	OpFileLength original_pos;
	if (OpStatus::IsError(m_thisfile->GetFilePos(original_pos)))
		return OpStatus::ERR;

	if (from_entry < 0)
	{
		//invalid from_entry index
		return OpStatus::ERR;
	}

	OpFileLength end_of_central_directory_offset;

	// At first update end of central directory
	if (OpStatus::IsError(LocateEndOfCentralDirectory(&end_of_central_directory_offset)))
		return OpStatus::ERR;

	m_end_of_central_directory.central_start_offset += offset;
	if (OpStatus::IsError(m_thisfile->SetFilePos(end_of_central_directory_offset + END_CENTRAL_DIR_START_OFFSET)) ||
		OpStatus::IsError(m_thisfile->WriteBinLongLH(m_end_of_central_directory.central_start_offset))
		)
	{
		m_thisfile->SetFilePos(original_pos);
		return OpStatus::ERR;
	}

	// Update all central directory entries starting from from_entry
	OpFileLength central_directory_offset = m_end_of_central_directory.central_start_offset;

	for (int i = 0; i < m_end_of_central_directory.entries_here; i++)
	{
		if (i >= from_entry)
		{
			m_Files[i]->local_offset += offset;
			if (OpStatus::IsError(m_thisfile->SetFilePos(central_directory_offset + CENTRAL_DIR_LOCAL_START_OFFSET)) ||
				OpStatus::IsError(m_thisfile->WriteBinLongLH(m_Files[i]->local_offset))
				)
			{
				m_thisfile->SetFilePos(original_pos);
				return OpStatus::ERR;
			}
		}
		central_directory_offset += CENTRAL_DIR_LENGTH + m_Files[i]->filename_len + m_Files[i]->extra_len + m_Files[i]->comment_len;
	}

	m_thisfile->SetFilePos(original_pos);
	return OpStatus::OK;
}

OP_STATUS OpZip::UpdateCRC32(int entry_index, UINT32 crc_val)
{
	OpFileLength original_pos;
	if (OpStatus::IsError(m_thisfile->GetFilePos(original_pos)))
		return OpStatus::ERR;

	if (entry_index < 0 || entry_index >= m_end_of_central_directory.entries_here)
	{
		//invalid entry index
		return OpStatus::ERR;
	}

	if (m_Files[entry_index]->crc32 == crc_val)
	{
		//no need to modify
		return OpStatus::OK;
	}

	//At first update crc32 value in central directory
	OpFileLength central_directory_offset = m_end_of_central_directory.central_start_offset;

	m_Files[entry_index]->crc32 = crc_val;

	for (int i = 0; i < entry_index; i++)
		central_directory_offset += CENTRAL_DIR_LENGTH + m_Files[i]->filename_len + m_Files[i]->extra_len + m_Files[i]->comment_len;
	central_directory_offset += CENTRAL_DIR_CRC32_OFFSET;

	if (OpStatus::IsError(m_thisfile->SetFilePos(central_directory_offset)) ||
		OpStatus::IsError(m_thisfile->WriteBinLongLH(crc_val))
		)
	{
		m_thisfile->SetFilePos(original_pos);
		return OpStatus::ERR;
	}

	//Now update crc32 value in local header
	OpFileLength local_crc32_offset = m_Files[entry_index]->local_offset;
	if (m_Files[entry_index]->general_flags & 0x0008)
	{
		// The third bit is set, CRC32 needs to be modified in data descriptor
		local_crc32_offset = m_Files[entry_index]->local_offset + FILE_HEADER_LENGTH + m_Files[entry_index]->filename_len + m_Files[entry_index]->extra_len + m_Files[entry_index]->comp_size;
	}
	else
	{
		// CRC32 is stored in local file header, this is the regular case
		local_crc32_offset = m_Files[entry_index]->local_offset + LOCAL_CRC32_OFFSET;
	}

	if (OpStatus::IsError(m_thisfile->SetFilePos(local_crc32_offset, SEEK_FROM_START)) ||
		OpStatus::IsError(m_thisfile->WriteBinLongLH(crc_val)))
	{
		m_thisfile->SetFilePos(original_pos);
		return OpStatus::ERR;
	}

	m_thisfile->SetFilePos(original_pos);
	return OpStatus::OK;
}

OP_STATUS OpZip::UpdateCompressionInfo(int entry_index, UINT16 compression_method, UINT16 extra_flag)
{
	OpFileLength original_pos;
	if (OpStatus::IsError(m_thisfile->GetFilePos(original_pos)))
		return OpStatus::ERR;

	if (entry_index < 0 || entry_index >= m_end_of_central_directory.entries_here)
	{
		//invalid entry index
		return OpStatus::ERR;
	}

	if (m_Files[entry_index]->comp_method == compression_method && m_Files[entry_index]->general_flags == extra_flag)
	{
		//no need to modify
		return OpStatus::OK;
	}

	//At first update compression info value in central directory
	m_Files[entry_index]->comp_method = compression_method;
	m_Files[entry_index]->general_flags |= extra_flag;

	OpFileLength central_directory_offset = m_end_of_central_directory.central_start_offset;
	for (int i = 0; i < entry_index; i++)
		central_directory_offset += CENTRAL_DIR_LENGTH + m_Files[i]->filename_len + m_Files[i]->extra_len + m_Files[i]->comment_len;

	if (OpStatus::IsError(m_thisfile->SetFilePos(central_directory_offset + CENTRAL_DIR_GENERAL_FLAG_OFFSET)) ||
		OpStatus::IsError(m_thisfile->WriteBinShortLH(m_Files[entry_index]->general_flags)) ||
		OpStatus::IsError(m_thisfile->SetFilePos(central_directory_offset + CENTRAL_DIR_COMP_METHOD_OFFSET)) ||
		OpStatus::IsError(m_thisfile->WriteBinShortLH(m_Files[entry_index]->comp_method))
		)
	{
		m_thisfile->SetFilePos(original_pos);
		return OpStatus::ERR;
	}

	//Now update compression info value in local header
	OpFileLength local_crc32_offset = m_Files[entry_index]->local_offset;

	if (OpStatus::IsError(m_thisfile->SetFilePos(local_crc32_offset + LOCAL_GENERAL_FLAG_OFFSET, SEEK_FROM_START)) ||
		OpStatus::IsError(m_thisfile->WriteBinShortLH(m_Files[entry_index]->general_flags)) ||
		OpStatus::IsError(m_thisfile->SetFilePos(local_crc32_offset + LOCAL_COMP_METHOD_FLAG_OFFSET, SEEK_FROM_START)) ||
		OpStatus::IsError(m_thisfile->WriteBinShortLH(m_Files[entry_index]->comp_method))
		)
	{
		m_thisfile->SetFilePos(original_pos);
		return OpStatus::ERR;
	}

	m_thisfile->SetFilePos(original_pos);
	return OpStatus::OK;
}

OP_STATUS OpZip::UpdateLength(int entry_index, UINT32 comp_len, UINT32 uncomp_len)
{
	OpFileLength original_pos;
	if (OpStatus::IsError(m_thisfile->GetFilePos(original_pos)))
		return OpStatus::ERR;

	if (entry_index < 0 || entry_index >= m_end_of_central_directory.entries_here)
	{
		//invalid entry index
		return OpStatus::ERR;
	}

	if (m_Files[entry_index]->comp_size == comp_len && m_Files[entry_index]->uncomp_size == uncomp_len)
	{
		//no need to modify
		return OpStatus::OK;
	}

	//At first update length values in central directory
	OpFileLength central_directory_offset = m_end_of_central_directory.central_start_offset;

	m_Files[entry_index]->comp_size = comp_len;
	m_Files[entry_index]->uncomp_size = uncomp_len;

	for (int i = 0; i < entry_index; i++)
		central_directory_offset += CENTRAL_DIR_LENGTH + m_Files[i]->filename_len + m_Files[i]->extra_len + m_Files[i]->comment_len;
	central_directory_offset += CENTRAL_DIR_COMP_LENGTH_OFFSET;

	if (OpStatus::IsError(m_thisfile->SetFilePos(central_directory_offset)) ||
		OpStatus::IsError(m_thisfile->WriteBinLongLH(comp_len)) ||
		OpStatus::IsError(m_thisfile->WriteBinLongLH(uncomp_len))
		)
	{
		m_thisfile->SetFilePos(original_pos);
		return OpStatus::ERR;
	}

	//Now update length values in local header
	OpFileLength local_length_offset = m_Files[entry_index]->local_offset;
	if (m_Files[entry_index]->general_flags & 0x0008)
	{
		// The third bit is set, length needs to be modified in data descriptor
		local_length_offset = m_Files[entry_index]->local_offset + FILE_HEADER_LENGTH + m_Files[entry_index]->filename_len + m_Files[entry_index]->extra_len + m_Files[entry_index]->comp_size + DATA_DESC_COMP_LEN_OFFSET;
	}
	else
	{
		// Length is stored in local file header, this is the regular case
		local_length_offset = m_Files[entry_index]->local_offset + LOCAL_COMP_LENGTH_OFFSET;
	}

	if (OpStatus::IsError(m_thisfile->SetFilePos(local_length_offset, SEEK_FROM_START)) ||
		OpStatus::IsError(m_thisfile->WriteBinLongLH(comp_len)) ||
		OpStatus::IsError(m_thisfile->WriteBinLongLH(uncomp_len))
		)
	{
		m_thisfile->SetFilePos(original_pos);
		return OpStatus::ERR;
	}

	m_thisfile->SetFilePos(original_pos);
	return OpStatus::OK;
}

OP_STATUS OpZip::Compress(const char* source_buffer,UINT32 uncomp_size,INT32 comp_level,char*& dest_buffer,OpFileLength& comp_size)
{
	dest_buffer = NULL;
	comp_size = 0;

	if (comp_level != Z_NO_COMPRESSION)
	{
		//Compress given buffer
		z_stream  strm;
		op_memset(&strm,0,sizeof(z_stream));
		size_t	  bufsize = (size_t)(uncomp_size * 1.001 + 12);

		dest_buffer = (char*)op_malloc(bufsize * sizeof(char));	//we use op_malloc to be able to use op_realloc
		op_memset(&strm, 0, sizeof(z_stream));

		if (!dest_buffer)
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		// allocate deflate state
		if (deflateInit2(&strm, comp_level, Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY) != Z_OK)
		{
			op_free(dest_buffer);
			return OpStatus::ERR;
		}

		int res;
		strm.avail_in  = uncomp_size;
		strm.next_in   = reinterpret_cast<Bytef*>(const_cast<char*>(source_buffer));
		strm.avail_out = bufsize;
		strm.next_out  = reinterpret_cast<Bytef*>(dest_buffer);

		do {
			// compress until we're done

			res = deflate(&strm, Z_FINISH);
			if (strm.avail_out <= 0)
			{
				dest_buffer = (char*)op_realloc(dest_buffer, bufsize + COMPRESS_BUFFER_GRAIN);
				if (!dest_buffer)
				{
					return OpStatus::ERR_NO_MEMORY;
				}
				strm.next_out  = reinterpret_cast<Bytef*>(dest_buffer + bufsize);
				strm.avail_out = COMPRESS_BUFFER_GRAIN;
				bufsize += COMPRESS_BUFFER_GRAIN;
			}

		} while (Z_STREAM_END != res);

		// Cleanup after compression
		comp_size = strm.total_out;
		dest_buffer = (char*)op_realloc(dest_buffer, comp_size);
		if (!dest_buffer)
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		deflateEnd(&strm);
	}
	return OpStatus::OK;
}

#endif // UTIL_ZIP_WRITE

OP_STATUS OpZip::IsZipFile(BOOL* is_zip)
{
	UINT16 general_purpose_bit_flag, filename_len, extra_len, comment_len;
	UINTBIG compressed_size = 0;
	UINT8 entry[34]; /* ARRAY OK 2011-11-29 peter */
	UINT8 extrabuffer[16]; /* ARRAY OK 2011-11-29 peter */
	OpFileLength bytes_read=0;
	BOOL have_data_desc = FALSE;

	*is_zip = FALSE;

	RETURN_IF_ERROR(m_thisfile->SetFilePos(0));

	// Read the local file entries in the zip-file
	while (!m_thisfile->Eof())
	{
		RETURN_IF_ERROR(m_thisfile->Read(entry, 30, &bytes_read));
		if (bytes_read != 30)
			return OpStatus::ERR;

		// Signature
		switch (BytesToUInt32(entry))
		{
		case LOCAL_SIGNATURE:
			// Jump over version
			general_purpose_bit_flag = BytesToUInt16(&entry[6]);
			if (general_purpose_bit_flag & 0x08)
				return OpStatus::OK;

			// Jump over compression method, modfication date and time, CRC.
			compressed_size = BytesToUInt32(&entry[18]);

			// Jump over uncompressed size
			filename_len = BytesToUInt16(&entry[26]);
			extra_len = BytesToUInt16(&entry[28]);

			// Check for zip64 extensions
			if (compressed_size == 0xFFFFFFFFU && extra_len > 0)
			{
				// Jump over filename
				RETURN_IF_ERROR(m_thisfile->SetFilePos(filename_len, SEEK_FROM_CURRENT));

				UINT16 extra_offset = 0;
				while (extra_offset < extra_len)
				{
					// Read extra type and length
					RETURN_IF_ERROR(m_thisfile->Read(extrabuffer, 4, &bytes_read));
					if (bytes_read != 4)
						break;
					UINT16 field_length = BytesToUInt16(&extrabuffer[2]);
					if (BytesToUInt16(extrabuffer) == 1 && field_length >= 16)
					{
						/* Read zip64 extended information extra field. We are
						 * interested in the second 64-bit value, which is the
						 * compressed size. */
						RETURN_IF_ERROR(m_thisfile->Read(extrabuffer, 16, &bytes_read));
						RETURN_IF_ERROR(BytesToUInt64(&extrabuffer[8], compressed_size));

						// Skip any remainder
						if (field_length > 16)
						{
							RETURN_IF_ERROR(m_thisfile->SetFilePos(field_length - 16, SEEK_FROM_CURRENT));
						}
					}
					else
					{
						// Skip this field
						RETURN_IF_ERROR(m_thisfile->SetFilePos(field_length, SEEK_FROM_CURRENT));
					}
					extra_offset += 4 + field_length;
				}

				// Jump over compressed data
				RETURN_IF_ERROR(m_thisfile->SetFilePos(compressed_size, SEEK_FROM_CURRENT));
			}
			else
			{
				// Jump over filename, extra field and compressed data.
				RETURN_IF_ERROR(m_thisfile->SetFilePos(filename_len+extra_len+compressed_size, SEEK_FROM_CURRENT));
			}

			// Jump over data descriptor field.
			if (have_data_desc)
				RETURN_IF_ERROR(m_thisfile->SetFilePos(12, SEEK_FROM_CURRENT));

			break;

		case FILE_SIGNATURE:
			// Start of central directory found, exit this loop.
			goto found_central_directory_start;

		default:
			{
				// Check if the file maybe have a data descriptor field.
				UINT32 maybe_size = BytesToUInt32(&entry[4]);
				if (!compressed_size || have_data_desc == TRUE || maybe_size != compressed_size)
					return OpStatus::ERR;
				RETURN_IF_ERROR(m_thisfile->SetFilePos(static_cast<OpFileLength>(-18), SEEK_FROM_CURRENT));
				have_data_desc = TRUE;

				// Check for header again, after the data descriptor field.
			}
		}
	}

found_central_directory_start:
	if (m_thisfile->Eof())
		return OpStatus::ERR;

	OpFileLength oldpos;
	RETURN_IF_ERROR(m_thisfile->GetFilePos(oldpos));
	oldpos -= 30;
	RETURN_IF_ERROR(m_thisfile->SetFilePos(oldpos));

	// Central directory
	while (!m_thisfile->Eof())
	{
		RETURN_IF_ERROR(m_thisfile->Read(entry, 34, &bytes_read));

		// Signature
		switch (BytesToUInt32(entry))
		{
		case ZIP64_DIR_SIGNATURE:
			{
				// Zip64 end of central directory
				UINTBIG len;
				RETURN_IF_ERROR(BytesToUInt64(&entry[4], len));

				// Jump over directory
				RETURN_IF_ERROR(m_thisfile->GetFilePos(oldpos));
				oldpos += len - 22; // 34 bytes - 12 bytes header
				RETURN_IF_ERROR(m_thisfile->SetFilePos(len - 22, SEEK_FROM_CURRENT));
			}
			break;

		case ZIP64_LOCATOR_SIGNATURE:
			// Zip64 end of central directory locator (20 bytes)
			RETURN_IF_ERROR(m_thisfile->GetFilePos(oldpos));
			oldpos -= 14; // we read 34 bytes, so rewind by 14
			RETURN_IF_ERROR(m_thisfile->SetFilePos(static_cast<OpFileLength>(-14), SEEK_FROM_CURRENT));
			break;

		case DIR_SIGNATURE:
			/* We have found the end of central dir signature. Set the file
			 * pointer to point to it, and indicate that we did find it. */
			RETURN_IF_ERROR(m_thisfile->SetFilePos(oldpos));
			*is_zip = TRUE;
			return OpStatus::OK;

		case FILE_SIGNATURE:
			{
				// A central directory entry.

				oldpos += bytes_read;

				// Jump over version, bit flag, method, time, date, CRC, size
				filename_len = BytesToUInt16(&entry[28]);
				extra_len = BytesToUInt16(&entry[30]);
				comment_len = BytesToUInt16(&entry[32]);

				// Jump over disk start, file attributes, offset, filename, extra field and comment.
				OpFileLength delta = 12 + filename_len + extra_len + comment_len;
				if (oldpos + delta > m_sizeofFile)
					return OpStatus::ERR;

				RETURN_IF_ERROR(m_thisfile->SetFilePos(delta, SEEK_FROM_CURRENT));
				oldpos += delta;
			}
			break;

		default:
			// Unknown header or not a zip file
			return OpStatus::ERR;
		}
	}

	return OpStatus::ERR;
}

/***********************************************************************************
**
**	Open
**
***********************************************************************************/

OP_STATUS OpZip::Open(const OpStringC& filename, BOOL write_perm)
{
	Close();

	if (filename.IsEmpty())
		return OpStatus::ERR;

	m_thisfile = OP_NEW(OpFile, ());

	if (!m_thisfile)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(m_thisfile->Construct(filename));

	BOOL exists;
	OP_STATUS err = m_thisfile->Exists(exists);
	if (OpStatus::IsSuccess(err) && exists)		// open an existing file, first check if the file is a zipfile
	{
		BOOL is_zip;
#ifndef UTIL_ZIP_WRITE
		RETURN_IF_ERROR(m_thisfile->Open(OPFILE_READ));
#else
		m_read_only = FALSE;
		if (!write_perm || OpStatus::IsError(m_thisfile->Open(OPFILE_UPDATE | OPFILE_SHAREDENYWRITE))) {
			RETURN_IF_ERROR(m_thisfile->Open(OPFILE_READ));	// Maybe file is write protected
			m_read_only = TRUE;
		}
#endif // UTIL_ZIP_WRITE
		OpFileLength len, bytes_read;
		err = m_thisfile->GetFileLength(len);
		if (OpStatus::IsError(err))
		{
			Close();
			return err;
		}
		m_sizeofFile = len;

		// Try traversing the file as a Zip archive
		err = IsZipFile(&is_zip);
		if (OpStatus::IsError(err))
		{
			Close();
			return err;
		}

		OpFileLength eocd_pos;
		if (is_zip)
		{
			// We found the End of Central Directory record. Remember where.
			RETURN_IF_ERROR(m_thisfile->GetFilePos(eocd_pos));
		}
		else
		{
			/* This looks like a ZIP file, but cannot be parsed from the
			 * front. Try locating the End of Central Directory record by
			 * scanning backwards. */
			OP_STATUS res;
			res = LocateEndOfCentralDirectory(&eocd_pos);

			if (OpStatus::IsSuccess(res))	//we found the signature, set the position for reading
			{
				m_pos = eocd_pos;
				m_thisfile->SetFilePos(m_pos);
			}
			else
			{
				OP_ASSERT(1);	//This is not a standard zipfile
				Close();
				return OpStatus::ERR;
			}
		}

		BOOL headerread = FALSE;
		{
			// Read the End of Central Directory record.
			UINT8 readbuf[END_CENTRAL_DIR_LENGTH];
			headerread = OpStatus::IsSuccess(m_thisfile->Read(readbuf, END_CENTRAL_DIR_LENGTH, &bytes_read)) &&
					bytes_read == END_CENTRAL_DIR_LENGTH;

			m_end_of_central_directory.signature = BytesToUInt32(readbuf);
			m_end_of_central_directory.this_disk = BytesToUInt16(readbuf+4);
			m_end_of_central_directory.central_start_disk = BytesToUInt16(readbuf+6);
			m_end_of_central_directory.entries_here = BytesToUInt16(readbuf+8);
			m_end_of_central_directory.file_count = BytesToUInt16(readbuf+10);
			m_end_of_central_directory.central_size = BytesToUInt32(readbuf+12);
			m_end_of_central_directory.central_start_offset = BytesToUInt32(readbuf+16);
			m_end_of_central_directory.comment_len = BytesToUInt16(readbuf+20);
		}

		if (!headerread || m_end_of_central_directory.signature != DIR_SIGNATURE)
		{
			Close();
			return OpStatus::ERR;
		}

		/////////////////////////////////////////////////////
		//check if we have a zip64 end of central directory//
		/////////////////////////////////////////////////////

		// Flag whether we must have a zip64 header.
		bool need_zip64_header =
			m_end_of_central_directory.central_start_offset == 0xFFFFFFFF ||
			m_end_of_central_directory.entries_here == 0xFFFFU ||
			m_end_of_central_directory.file_count == 0xFFFFU ||
			m_end_of_central_directory.central_size == 0xFFFFFFFFU;

		if (eocd_pos > ZIP64_LOCATOR_LENGTH + ZIP64_END_CENTRAL_DIR_LENGTH)
		{
			/* The fixed-length Zip64 End of Central Directory Locator
			 * record should directly precede the End of Central Directory
			 * record. */
			m_pos = eocd_pos - ZIP64_LOCATOR_LENGTH;
			m_thisfile->SetFilePos(m_pos);
			UINT8 readbuf[ZIP64_LOCATOR_LENGTH];

			if (OpStatus::IsSuccess(m_thisfile->Read(readbuf, ZIP64_LOCATOR_LENGTH, &bytes_read)) &&
			    bytes_read == ZIP64_LOCATOR_LENGTH &&
			    BytesToUInt32(readbuf) == ZIP64_LOCATOR_SIGNATURE)
			{
				/* We have found the Locator record, now use that to
				 * locate the Zip64 End of Central Directory record. */
				UINTBIG newpos;
				RETURN_IF_ERROR(BytesToUInt64(readbuf+8, newpos));
				if (newpos + ZIP64_END_CENTRAL_DIR_LENGTH > static_cast<UINTBIG>(m_pos))
				{
					/* The Zip64 End of Central Directory record cannot
					 * possibly fit in that space. */
					Close();
					return OpStatus::ERR;
				}

				m_pos = newpos;
				m_thisfile->SetFilePos(m_pos);

				UINT8 readbuf2[ZIP64_END_CENTRAL_DIR_LENGTH];
				if (OpStatus::IsSuccess(m_thisfile->Read(readbuf2, ZIP64_END_CENTRAL_DIR_LENGTH, &bytes_read)) &&
				    bytes_read == ZIP64_END_CENTRAL_DIR_LENGTH &&
				    BytesToUInt32(readbuf2) == ZIP64_DIR_SIGNATURE)
				{
					/* We have found the Zip64 End of Central Directory
					 * record. Use it to update the data we read from the
					 * standard End of Central Directory record above. */
					RETURN_IF_ERROR(BytesToUInt64(readbuf2+24, m_end_of_central_directory.entries_here));
					RETURN_IF_ERROR(BytesToUInt64(readbuf2+32, m_end_of_central_directory.file_count));
					RETURN_IF_ERROR(BytesToUInt64(readbuf2+40, m_end_of_central_directory.central_size));
					RETURN_IF_ERROR(BytesToUInt64(readbuf2+48, m_end_of_central_directory.central_start_offset));

					need_zip64_header = false;
				}
			}
		}

		if (need_zip64_header)
		{
			/* We found a zip64 indicator in the End of Central Directory
			 * record, but were unable to locate the zip64 record. */
			Close();
			return OpStatus::ERR;
		}

		//////////////////////////////////////////////////////////////////////////////
		//we now have a full description of the whole zipfile, read the central-record
		//////////////////////////////////////////////////////////////////////////////

		VALIDATE_SIZE(central_size, m_end_of_central_directory.central_size);
		m_DirData = OP_NEWA(unsigned char, central_size);
		if (!m_DirData)
		{
			Close();
			return OpStatus::ERR_NO_MEMORY;
		}

		unsigned int i;
		for(i=0; i<m_end_of_central_directory.central_size; i++)
			m_DirData[i] = 0;

		VALIDATE_SIZE(entries_here, m_end_of_central_directory.entries_here);
		m_Files = OP_NEWA(central_directory_record*, entries_here);
		if (!m_Files)
		{
			Close();
			return OpStatus::ERR_NO_MEMORY;
		}

		for(i=0; i<m_end_of_central_directory.entries_here; i++)
			m_Files[i] = NULL;

		OpFileLength pos = m_end_of_central_directory.central_start_offset;

		m_pos = pos;

		m_thisfile->SetFilePos(m_pos);

		OpFileLength bytestoread = m_end_of_central_directory.central_size;
		OpFileLength bytesread;
		OpFileLength bufferoffset = 0;
		OpFileLength totalbytesread = 0;

		do
		{
			//read the file structure in the zip-file
			m_thisfile->Read(m_DirData+bufferoffset, bytestoread, &bytesread);
			bufferoffset += bytesread;
			totalbytesread += bytesread;
			bytestoread = m_end_of_central_directory.central_size - totalbytesread;
		} while ((bytestoread > 0) && !m_thisfile->Eof());
		RETURN_IF_ERROR(m_thisfile->GetFilePos(m_pos));

		OpFileLength dwRead = totalbytesread;

		OP_ASSERT(dwRead==(OpFileLength)m_end_of_central_directory.central_size);
		if (dwRead != (OpFileLength)(m_end_of_central_directory.central_size) )
		{
			Close();
			return OpStatus::ERR;
		}

		unsigned char* pData = m_DirData;
		for(i=0; i<m_end_of_central_directory.entries_here; i++ )
		{
			// Set the header pointer in the m_Files array
			if (pData + 46 > m_DirData + totalbytesread)
			{
				// Error in zip file.  Discard the whole thing.
				Close();
				return OpStatus::ERR;
			}
			central_directory_record *fh = OP_NEW(central_directory_record, ());
			if (!fh)
			{
				Close();
				return OpStatus::ERR;
			}
			fh->signature = BytesToUInt32(pData);
			fh->version_made_by = BytesToUInt16(pData+4);
			fh->ver_2_extract = BytesToUInt16(pData+6);
			fh->general_flags = BytesToUInt16(pData+8);
			fh->comp_method = BytesToUInt16(pData+10);
			fh->time = BytesToUInt16(pData+12);
			fh->date = BytesToUInt16(pData+14);
			fh->crc32 = BytesToUInt32(pData+16);
			fh->comp_size = BytesToUInt32(pData+20);
			fh->uncomp_size = BytesToUInt32(pData+24);
			fh->filename_len = BytesToUInt16(pData+28);
			fh->extra_len = BytesToUInt16(pData+30);
			fh->comment_len = BytesToUInt16(pData+32);
			fh->start_disk = BytesToUInt16(pData+34);
			fh->int_file_attr = BytesToUInt16(pData+36);
			fh->ext_file_attr = BytesToUInt32(pData+38);
			fh->local_offset = BytesToUInt32(pData+42);

			// Iterate over extra fields
			if (pData + 46 + fh->filename_len + fh->extra_len + fh->comment_len > m_DirData + totalbytesread)
			{
				// Error in zip file. Discard the whole thing.
				OP_DELETE(fh);
				Close();
				return OpStatus::ERR;
			}

			if (fh->filename_len > 0) fh->filename = (char*)op_malloc(fh->filename_len);
			if (fh->extra_len > 0)    fh->extra    = (UINT8*)op_malloc(fh->extra_len);
			if (fh->comment_len > 0)  fh->comment  = (char*)op_malloc(fh->comment_len);
			if ((fh->filename_len && fh->filename == 0) ||
			    (fh->extra_len    && fh->extra    == 0) ||
			    (fh->comment_len  && fh->comment  == 0))
			{
				op_free(fh->filename);
				op_free(fh->extra);
				op_free(fh->comment);
				OP_DELETE(fh);
				Close();
				return OpStatus::ERR_NO_MEMORY;
			}

			if (fh->filename_len > 0)
				op_memcpy(fh->filename, pData + 46, fh->filename_len);
			if (fh->extra_len > 0)
				op_memcpy(fh->extra, pData + 46 + fh->filename_len, fh->extra_len);
			if (fh->comment_len > 0)
				op_memcpy(fh->comment, pData + 46 + fh->filename_len + fh->extra_len, fh->comment_len);

			m_Files[i] = fh;
			if (fh->signature != FILE_SIGNATURE)
			{
				Close();
				return OpStatus::ERR;
			}

			// Just be sure that the filename is consistent

			char* pstrName = fh->GetName();
			for (int j=0; j<fh->filename_len; j++, pstrName++)
			{
				if (*pstrName=='/' || *pstrName=='\\')
				{
					*pstrName=PATHSEPCHAR;
				}
			}

			bool need_zip64_record =
				fh->comp_size == 0xFFFFFFFFU ||
				fh->uncomp_size == 0xFFFFFFFFU ||
				fh->local_offset == 0xFFFFFFFFU;

			if (need_zip64_record && fh->extra_len > 0)
			{
				// Iterate over extra headers to find the zip64 extra field
				UINT16 offset = 0;
				while (offset < fh->extra_len)
				{
					UINT16 extra_len= BytesToUInt16(fh->extra + offset + 2);
					if (extra_len >= 8 && BytesToUInt16(fh->extra + offset) == 1)
					{
						/* Read the Zip64 extended information extra field.
						 * The order within the record is fixed, but it
						 * needs only be long enough to cover the values
						 * that did not fit in the standard record. */
						UINT8 *zip64_data = fh->extra + offset + 4;

						// Uncompressed size: 8 bytes (0-7)
						RETURN_IF_ERROR(BytesToUInt64(zip64_data, fh->uncomp_size));

						// Compressed size: 8 bytes (8-15)
						if (extra_len >= 16)
							RETURN_IF_ERROR(BytesToUInt64(zip64_data + 8, fh->comp_size));
						else if (fh->comp_size == 0xFFFFFFFFU)
						{
							Close();
							return OpStatus::ERR;
						}

						// Offset of local header: 8 bytes (16-23)
						if (extra_len >= 24)
							RETURN_IF_ERROR(BytesToUInt64(zip64_data + 16, fh->local_offset));
						else if (fh->local_offset == 0xFFFFFFFFU)
						{
							Close();
							return OpStatus::ERR;
						}

						// Starting disk: 4 bytes (24-27)
						if (extra_len >= 28)
							fh->start_disk = BytesToUInt32(zip64_data + 24);
						else if (fh->start_disk == 0xFFFU)
						{
							/* Not that we really care, since we do not
							 * support spanning, but this nevertheless
							 * means that the archive is broken. */
							Close();
							return OpStatus::ERR;
						}

						need_zip64_record = false;
						break;
					}

					offset += extra_len + 4;
				}
			}

			if (need_zip64_record)
			{
				/* We found a zip64 indicator in the directory record,
				 * but were unable to locate the zip64 extra field. */
				Close();
				return OpStatus::ERR;
			}

			// Get next header
			pData += 46 + fh->filename_len + fh->extra_len + fh->comment_len;
		}
	}
	else	//create a new blank zipfile
	{
		RETURN_IF_ERROR(m_thisfile->Open(write_perm ? OPFILE_READ | OPFILE_WRITE : OPFILE_READ));
		op_memset(&m_end_of_central_directory,0,sizeof(end_of_central_directory_record));
		m_end_of_central_directory.signature = DIR_SIGNATURE;
	}
	return OpStatus::OK;
}

OP_STATUS OpZip::LocateEndOfCentralDirectory(OpFileLength* location)
{
	OpFileLength original_pos;
	OpFileLength cur_pos = m_pos;
	OP_STATUS err;

	if (OpStatus::IsError(m_thisfile->GetFilePos(original_pos)))
		return OpStatus::ERR;

	if (!location)
		return OpStatus::ERR;

	UINT16 count=0;
	// 22 is the size if one count with an comment-field of length zero
	cur_pos = m_sizeofFile - END_CENTRAL_DIR_LENGTH; //sizeof(m_end_of_central_directory);

	if (OpStatus::IsError(m_thisfile->SetFilePos(cur_pos)))	//start searching for the end of central directory record, why did they put the comment here?
		return OpStatus::ERR;

	UINT32 readbuf;
	if (OpStatus::IsError(ReadUINT32(readbuf)))
	{
		m_thisfile->SetFilePos(original_pos);
		return OpStatus::ERR;
	}

	BOOL signaturefound = (readbuf == DIR_SIGNATURE);

	if (!signaturefound)		//if not signature was found, try to find it with an offsetscan starting at the place it should have ended
	{
		cur_pos += 4;
	}
    // Do not search through more than 4Kb.
	while(!signaturefound && !m_thisfile->Eof() && count < 4096)
	{
		count++;
		if (cur_pos == 0)
		{
			m_thisfile->SetFilePos(original_pos);
			return OpStatus::ERR;
		}

		cur_pos --;
		if (OpStatus::IsError(err = m_thisfile->SetFilePos(cur_pos)))
		{
			m_thisfile->SetFilePos(original_pos);
			return err;
		}

		if (OpStatus::IsError(err = ReadUINT32(readbuf)))
		{
			m_thisfile->SetFilePos(original_pos);
			return err;
		}

		signaturefound = (readbuf == DIR_SIGNATURE);
	}

	*location = cur_pos;
	m_thisfile->SetFilePos(original_pos);
	return signaturefound ? OpStatus::OK : OpStatus::ERR;
}

/***********************************************************************************
**
**	AddFileToArchive
**
***********************************************************************************/
#ifdef UTIL_ZIP_WRITE
OP_STATUS OpZip::AddFileToArchive(OpFile* newfile)
{
	// 	truncate central directories and end_of_central_directory_record parts, will be appended to the end of zip file
	if(m_end_of_central_directory.file_count != 0)
	{
		OpFileLength truncate_len = m_end_of_central_directory.comment_len + END_CENTRAL_DIR_LENGTH;
		for(int i=0; i<m_end_of_central_directory.file_count; i++)
		{
			truncate_len += CENTRAL_DIR_LENGTH + m_Files[i]->comment_len + m_Files[i]->filename_len + m_Files[i]->extra_len;
		}
		if(m_sizeofFile <= truncate_len)
			return OpStatus::ERR;
		RETURN_IF_ERROR(m_thisfile->SetFileLength(m_sizeofFile - truncate_len ));
	}

	// move write pos to the end, new files will be wrote there
	RETURN_IF_ERROR(m_thisfile->SetFilePos(0,SEEK_FROM_END));

	if (!newfile->IsOpen())
	{
		RETURN_IF_ERROR(newfile->Open(OPFILE_READ));
	}

	// compress the new file

	OpFileLength uncompressed_filelength;
	RETURN_IF_ERROR(newfile->GetFileLength(uncompressed_filelength));

	char* filecontent = OP_NEWA(char, uncompressed_filelength);
	if (!filecontent)
		return OpStatus::ERR_NO_MEMORY;
	OpFileLength bytes_read = 0;
	RETURN_IF_ERROR(newfile->Read(filecontent, uncompressed_filelength,&bytes_read));
	if(bytes_read != uncompressed_filelength)
		return OpStatus::ERR; // FIXME should try to read all

	char* compressed_filecontent = NULL;
	OpFileLength compressed_length = 0;
	RETURN_IF_ERROR(Compress(filecontent,uncompressed_filelength,Z_DEFAULT_COMPRESSION,compressed_filecontent,compressed_length));

	// fill local_file_header_record

	UINT32 crcvalue = 0; crcvalue = crc32(crcvalue, (const Bytef*)filecontent, uncompressed_filelength);
	OP_DELETEA(filecontent);
	filecontent = NULL;
	local_file_header_record record_for_new;
	op_memset(&record_for_new,0,sizeof(local_file_header_record));
	record_for_new.signature = LOCAL_SIGNATURE;
	record_for_new.ver_2_extract = 10;
	record_for_new.general_flags = 0;
	record_for_new.comp_method = COMP_DEFLATE;			//maybe moved out to preferences
	record_for_new.crc32 = crcvalue;
	record_for_new.comp_size = compressed_length;
	record_for_new.uncomp_size = uncompressed_filelength;
	record_for_new.filename_len = uni_strlen(newfile->GetName());
	record_for_new.extra_len = 0;

	//
	//Bits Contents
	//0-4 Day of the month (1-31)
	//5-8 Month (1 = January, 2 = February, and so on)
	//9-15 Year offset from 1980 (add 1980 to get actual year)
	//

	record_for_new.date = 0;		//date now FIXME

	//
	//Bits Contents
	//0-4 Second divided by 2
	//5-10 Minute (0-59)
	//11-15 Hour (0-23 on a 24-hour clock)
	//

	record_for_new.time = 0;		//time now FIXME

	// fill central_directory_record

	central_directory_record *fh = OP_NEW(central_directory_record, ());
	char* filename = OP_NEWA(char, record_for_new.filename_len+1);
	if(!fh || !filename)
	{
		OP_DELETEA(compressed_filecontent);
		OP_DELETEA(filename);
		return OpStatus::ERR_NO_MEMORY;
	}

	op_memset(fh,0,sizeof(central_directory_record));

	uni_cstrlcpy(filename, newfile->GetName(), record_for_new.filename_len+1);
	fh->filename = filename;

	fh->comp_method = record_for_new.comp_method;
	fh->comp_size = record_for_new.comp_size;
	fh->date = record_for_new.date;
	fh->filename_len = record_for_new.filename_len;
	fh->general_flags = record_for_new.general_flags;
	fh->crc32 = record_for_new.crc32;
	fh->signature = FILE_SIGNATURE;
	fh->time = record_for_new.time;
	fh->uncomp_size = record_for_new.uncomp_size;
	fh->ver_2_extract = record_for_new.ver_2_extract;
	fh->ext_file_attr = 0;
	OpFileLength local_offset = 0;
	RETURN_IF_ERROR(m_thisfile->GetFilePos(local_offset));
	fh->local_offset = local_offset;

	data_descriptor datadesc;
	datadesc.crc32 = record_for_new.crc32;
	datadesc.comp_size = record_for_new.comp_size;
	datadesc.uncomp_size = record_for_new.uncomp_size;

	// update m_end_of_central_directory and central_directory_record

	m_end_of_central_directory.file_count += 1;
	m_end_of_central_directory.entries_here += 1;

	m_Files = (central_directory_record**)op_realloc(m_Files, sizeof(central_directory_record*)*m_end_of_central_directory.entries_here);
	if(!m_Files)
	{
		OP_DELETEA(compressed_filecontent);
		OP_DELETEA(filename);
		return OpStatus::ERR_NO_MEMORY;
	}
	m_Files[m_end_of_central_directory.entries_here-1] = fh;
	m_end_of_central_directory.central_size += CENTRAL_DIR_LENGTH + fh->filename_len + fh->comment_len + fh->extra_len ;

	// write local_file_header,file content, and data_descriptor

	OP_ASSERT (m_thisfile->IsOpen());

	unsigned char writebuf[4]; /* ARRAY OK 2010-04-13 adamm */

	// local_file_header
	UInt32ToBytes(record_for_new.signature, writebuf);
	RETURN_IF_ERROR(m_thisfile->Write(writebuf, 4));
	UInt16ToBytes(record_for_new.ver_2_extract, writebuf);
	RETURN_IF_ERROR(m_thisfile->Write(writebuf, 2));
	UInt16ToBytes(record_for_new.general_flags, writebuf);
	RETURN_IF_ERROR(m_thisfile->Write(writebuf, 2));
	UInt16ToBytes(record_for_new.comp_method, writebuf);
	RETURN_IF_ERROR(m_thisfile->Write(writebuf, 2));
	UInt16ToBytes(record_for_new.date, writebuf);
	RETURN_IF_ERROR(m_thisfile->Write(writebuf, 2));
	UInt16ToBytes(record_for_new.time, writebuf);
	RETURN_IF_ERROR(m_thisfile->Write(writebuf, 2));
	UInt32ToBytes(record_for_new.crc32, writebuf);
	RETURN_IF_ERROR(m_thisfile->Write(writebuf, 4));
	UInt32ToBytes(record_for_new.comp_size, writebuf);
	RETURN_IF_ERROR(m_thisfile->Write(writebuf, 4));
	UInt32ToBytes(record_for_new.uncomp_size, writebuf);
	RETURN_IF_ERROR(m_thisfile->Write(writebuf, 4));
	UInt16ToBytes(record_for_new.filename_len, writebuf);
	RETURN_IF_ERROR(m_thisfile->Write(writebuf, 2));
	UInt16ToBytes(record_for_new.extra_len, writebuf);
	RETURN_IF_ERROR(m_thisfile->Write(writebuf, 2));
	RETURN_IF_ERROR(m_thisfile->Write(filename, record_for_new.filename_len));
	OP_ASSERT(record_for_new.extra_len == 0 && "Write extra record here!");

	// file content
	m_thisfile->Write(compressed_filecontent, record_for_new.comp_size);

	// data_descriptor
	UInt32ToBytes(datadesc.crc32, writebuf);
	RETURN_IF_ERROR(m_thisfile->Write(writebuf, 4));
	UInt32ToBytes(datadesc.comp_size, writebuf);
	RETURN_IF_ERROR(m_thisfile->Write(writebuf, 4));
	UInt32ToBytes(datadesc.uncomp_size, writebuf);
	RETURN_IF_ERROR(m_thisfile->Write(writebuf, 4));

	// update m_end_of_central_directory.central_start_offset
	OpFileLength current_pos = 0;
	RETURN_IF_ERROR(m_thisfile->GetFilePos(current_pos));
	m_end_of_central_directory.central_start_offset = current_pos;

	// write central directories record and end_of_central_directory

	for(int i=0; i < m_end_of_central_directory.entries_here; i++)
	{
		UInt32ToBytes(m_Files[i]->signature, writebuf);
		RETURN_IF_ERROR(m_thisfile->Write(writebuf, 4));
		UInt16ToBytes(m_Files[i]->version_made_by, writebuf);
		RETURN_IF_ERROR(m_thisfile->Write(writebuf, 2));
		UInt16ToBytes(m_Files[i]->ver_2_extract, writebuf);
		RETURN_IF_ERROR(m_thisfile->Write(writebuf, 2));
		UInt16ToBytes(m_Files[i]->general_flags, writebuf);
		RETURN_IF_ERROR(m_thisfile->Write(writebuf, 2));
		UInt16ToBytes(m_Files[i]->comp_method, writebuf);
		RETURN_IF_ERROR(m_thisfile->Write(writebuf, 2));
		UInt16ToBytes(m_Files[i]->date, writebuf);
		RETURN_IF_ERROR(m_thisfile->Write(writebuf, 2));
		UInt16ToBytes(m_Files[i]->time, writebuf);
		RETURN_IF_ERROR(m_thisfile->Write(writebuf, 2));
		UInt32ToBytes(m_Files[i]->crc32, writebuf);
		RETURN_IF_ERROR(m_thisfile->Write(writebuf, 4));
		UInt32ToBytes(m_Files[i]->comp_size, writebuf);
		RETURN_IF_ERROR(m_thisfile->Write(writebuf, 4));
		UInt32ToBytes(m_Files[i]->uncomp_size, writebuf);
		RETURN_IF_ERROR(m_thisfile->Write(writebuf, 4));
		UInt16ToBytes(m_Files[i]->filename_len, writebuf);
		RETURN_IF_ERROR(m_thisfile->Write(writebuf, 2));
		UInt16ToBytes(m_Files[i]->extra_len, writebuf);
		RETURN_IF_ERROR(m_thisfile->Write(writebuf, 2));
		UInt16ToBytes(m_Files[i]->comment_len, writebuf);
		RETURN_IF_ERROR(m_thisfile->Write(writebuf, 2));
		UInt16ToBytes(m_Files[i]->start_disk, writebuf);
		RETURN_IF_ERROR(m_thisfile->Write(writebuf, 2));
		UInt16ToBytes(m_Files[i]->int_file_attr, writebuf);
		RETURN_IF_ERROR(m_thisfile->Write(writebuf, 2));
		UInt32ToBytes(m_Files[i]->ext_file_attr, writebuf);
		RETURN_IF_ERROR(m_thisfile->Write(writebuf, 4));
		UInt32ToBytes(m_Files[i]->local_offset, writebuf);
		RETURN_IF_ERROR(m_thisfile->Write(writebuf, 4));
		RETURN_IF_ERROR(m_thisfile->Write(m_Files[i]->filename,m_Files[i]->filename_len));
	}

	//then write end of central directory
	UInt32ToBytes(m_end_of_central_directory.signature, writebuf);
	RETURN_IF_ERROR(m_thisfile->Write(writebuf, 4));
	UInt16ToBytes(m_end_of_central_directory.this_disk, writebuf);
	RETURN_IF_ERROR(m_thisfile->Write(writebuf, 2));
	UInt16ToBytes(m_end_of_central_directory.central_start_disk, writebuf);
	RETURN_IF_ERROR(m_thisfile->Write(writebuf, 2));
	UInt16ToBytes(m_end_of_central_directory.entries_here, writebuf);
	RETURN_IF_ERROR(m_thisfile->Write(writebuf, 2));
	UInt16ToBytes(m_end_of_central_directory.file_count, writebuf);
	RETURN_IF_ERROR(m_thisfile->Write(writebuf, 2));
	UInt32ToBytes(m_end_of_central_directory.central_size, writebuf);
	RETURN_IF_ERROR(m_thisfile->Write(writebuf, 4));
	UInt32ToBytes(m_end_of_central_directory.central_start_offset, writebuf);
	RETURN_IF_ERROR(m_thisfile->Write(writebuf, 4));
	UInt16ToBytes(m_end_of_central_directory.comment_len, writebuf);
	RETURN_IF_ERROR(m_thisfile->Write(writebuf, 2));

	RETURN_IF_ERROR(m_thisfile->GetFileLength(m_sizeofFile));

	return OpStatus::OK;
}
#endif

/***********************************************************************************
**
**	Close
**
***********************************************************************************/

void OpZip::Close()
{
	if (m_thisfile)
	{
		m_thisfile->Close();
		OP_DELETE(m_thisfile);
		m_thisfile = NULL;
	}

	if (m_index_list)
	{
		CleanIndex();
	}

	if (m_Files)
	{
		for (UINTBIG i = 0; i < m_end_of_central_directory.entries_here; i++)
		{
			if (m_Files[i])
			{
				op_free(m_Files[i]->filename);
				op_free(m_Files[i]->extra);
				op_free(m_Files[i]->comment);
				OP_DELETE(m_Files[i]);
			}
		}

		OP_DELETEA(m_Files);
		m_Files = NULL;
	}

	if (m_DirData)
	{
		OP_DELETEA(m_DirData);
		m_DirData = NULL;
	}
}

/***********************************************************************************
**
**	GetFile
**
***********************************************************************************/

OP_STATUS OpZip::GetFile(int iIndex, OpZipFile* file)
{
    OP_ASSERT(m_thisfile!=NULL);
    OP_ASSERT(file);
    OP_ASSERT(iIndex>=0 && static_cast<UINTBIG>(iIndex)<m_end_of_central_directory.entries_here);

	if (!m_thisfile)
        return OpStatus::ERR_NULL_POINTER;

    if (iIndex<0 || static_cast<UINTBIG>(iIndex)>=m_end_of_central_directory.entries_here)
        return OpStatus::ERR;

    local_file_header_record local_header;

    m_pos = m_Files[iIndex]->local_offset;
    m_thisfile->SetFilePos(m_pos);

	BOOL localfileheaderread = FALSE;
	UINT32 sizeofheader_ondisk = 30;
	{ // read local_header
		unsigned char readbuf[30]; /* ARRAY OK 2009-04-24 johanh */
		OpFileLength bytes_read;
		localfileheaderread = OpStatus::IsSuccess(m_thisfile->Read(readbuf, sizeofheader_ondisk, &bytes_read))  && bytes_read == sizeofheader_ondisk; // Note: no notification of short reads
		local_header.signature = BytesToUInt32(readbuf);
		local_header.ver_2_extract = BytesToUInt16(readbuf+4);
		local_header.general_flags = BytesToUInt16(readbuf+6);
		local_header.comp_method = BytesToUInt16(readbuf+8);
		local_header.date = BytesToUInt16(readbuf+10);
		local_header.time = BytesToUInt16(readbuf+12);
		local_header.crc32 = BytesToUInt32(readbuf+14);
		local_header.comp_size = BytesToUInt32(readbuf+18);
		local_header.uncomp_size = BytesToUInt32(readbuf+22);
		local_header.filename_len = BytesToUInt16(readbuf+26);
		local_header.extra_len = BytesToUInt16(readbuf+28);
	}

    if (!localfileheaderread || local_header.signature != LOCAL_SIGNATURE)
    {
        return OpStatus::ERR;
    }

    m_pos += sizeofheader_ondisk;
    m_pos += local_header.filename_len+local_header.extra_len;

    m_thisfile->SetFilePos(m_pos);

	/* Mac/Info-Zip created archives set the file sizes in the local header to
	 * zero. bsdtar archives only set the uncompressed size, leaving the
	 * compressed size zero. Zip64 format archives set the file sizes in the
	 * local header to 0xFFFFFFFF and store the file lenghts in an extra header.
	 * In either case, just use the central directory record.
	 */
	central_directory_record *central_fileheader = m_Files[iIndex];
	if (((!local_header.uncomp_size || !local_header.comp_size) && local_header.general_flags & ZIP_CRC_IN_DATA_DESCRIPTOR) ||
	    (local_header.uncomp_size == 0xFFFFFFFFU || local_header.comp_size == 0xFFFFFFFFU))
	{
		local_header.uncomp_size = central_fileheader->uncomp_size;
		local_header.comp_size = central_fileheader->comp_size;
		local_header.crc32 = central_fileheader->crc32;
	}

	VALIDATE_SIZE(uncomp_size, local_header.uncomp_size);
    unsigned char* pData= OP_NEWA(unsigned char, uncomp_size);
    OpFileLength bytes_read;

    if (!pData)
    {
        return OpStatus::ERR_NO_MEMORY;
    }

    switch (local_header.comp_method)
    {
	case COMP_STORE:
        {
            if (local_header.uncomp_size &&		// allow nullsize files
				(OpStatus::IsError(m_thisfile->Read(pData, local_header.uncomp_size, &bytes_read)) || static_cast<UINTBIG>(bytes_read) != local_header.uncomp_size))
            {
                OP_DELETEA(pData);
				return OpStatus::ERR;
            }
        }
        break;

	case COMP_DEFLATE:
        {
			VALIDATE_SIZE(comp_buffer_size, local_header.comp_size+1);
            unsigned char* pSource = OP_NEWA(unsigned char, comp_buffer_size);
            OP_ASSERT(pSource);
            if (!pSource)
            {
				OP_DELETEA(pData);
				return OpStatus::ERR_NO_MEMORY;
            }

			OpFileLength bytes_read;
			if (OpStatus::IsError(m_thisfile->Read(pSource, local_header.comp_size, &bytes_read)) || static_cast<UINTBIG>(bytes_read) != local_header.comp_size)
            {
                OP_DELETEA(pSource);
                OP_DELETEA(pData);
                return OpStatus::ERR;
            }
            int err;
            z_stream d_stream;
            // Avoid reading uninitialized memory. Nothing dangerous, but
            // some fields, such as total_read, are updated by inflate.
            op_memset(&d_stream, 0, sizeof(d_stream));

			// FIXME: zlib uses 32-bit values for file lengths, so compressed
			// files larger than 0xFFFFFFFF bytes will fail to load here.
            d_stream.next_in  = pSource;
            d_stream.avail_in = static_cast<uInt>(local_header.comp_size+1);	//work around for false buf_err when this reaches 0
			d_stream.next_out = pData;
            d_stream.avail_out = static_cast<uInt>(local_header.uncomp_size);
            err = inflateInit2(&d_stream, -MAX_WBITS);
            if (err == Z_OK)
            {
                err = inflate(&d_stream, Z_FINISH);
                if (err == Z_STREAM_END)
                    err = inflateEnd(&d_stream);
            }
            OP_DELETEA(pSource);
			pSource=NULL;
            if (err != Z_OK)
            {
                OP_DELETEA(pData);
                pData = NULL;
                return OpStatus::ERR;
            }
        }
        break;

	default:
		{
			OP_ASSERT(!"unsupported compression scheme");

			OP_DELETEA(pData);
			return OpStatus::ERR;
		}
	}

	unsigned long crc_val = 0; // crc32(0L, NULL, 0) always returns 0
	if (crc32(crc_val, pData, static_cast<uInt>(local_header.uncomp_size)) != local_header.crc32)
	{
		OP_DELETEA(pData);
		pData = NULL;
		return OpStatus::ERR;
	}

    return file->Init(pData, uncomp_size);
}

/***********************************************************************************
**
**	GetFileIndex
**
***********************************************************************************/

int OpZip::GetFileIndex(OpString* pstrName, OpZipFile* /*file*/)
{
    OP_ASSERT(m_thisfile!=NULL);		//has the zipfile been closed? Don't do that.

    const uni_char slash = '/';
    const uni_char backslash = '\\';

	if (!m_index_list)
	{
		RETURN_VALUE_IF_ERROR(CreateIndex(), -1);
	}

	int len = pstrName->Length();
    for (int j=0; j< len; ++j)
    {
        if (*(pstrName->DataPtr()+j) == slash || *(pstrName->DataPtr()+j) == backslash)
        {
            *(pstrName->DataPtr()+j)=PATHSEPCHAR;
        }
    }

	index_entry** compval = NULL;
	index_entry* val = OP_NEW(index_entry, ());
	if (!val)
		return -1;

	BOOL case_sensitive =
#ifdef ZIPFILE_DIRECT_ACCESS_SUPPORT
		(m_flags & OPFILE_FLAGS_CASE_SENSITIVE_ZIP) != 0;
#else
		FALSE;
#endif // ZIPFILE_DIRECT_ACCESS_SUPPORT

	int (*compare)(const void *, const void *);
	compare = case_sensitive ? compareindexnames_casesensitive : compareindexnames;

	if (OpStatus::IsSuccess(val->name.Set(*pstrName)))
	{
		compval = reinterpret_cast<index_entry** >
			(op_bsearch(reinterpret_cast<const void *>(&val),
						reinterpret_cast<const void *>(m_index_list),
						static_cast<size_t>(m_end_of_central_directory.entries_here),
						sizeof(index_entry *), compare));
	}

	OP_DELETE(val);

	// FIXME: OpVector uses INT32 for indexing, so we currently cannot handle
	// ZIP files with more than 0xFFFFFFFF entries.
	if (compval == NULL || static_cast<int>((*compval)->idx) != ((*compval)->idx))
	{
		return -1;
	}
	else
	{
		return static_cast<int>((*compval)->idx);
	}
}

/***********************************************************************************
**
**	CreateIndex
**
***********************************************************************************/
OP_STATUS OpZip::CreateIndex()
{
	if (!m_Files)
		return OpStatus::ERR_NULL_POINTER;

	VALIDATE_SIZE(entries_here, m_end_of_central_directory.entries_here);
	m_index_list = OP_NEWA(index_entry*, entries_here);

	if (!m_index_list)
		return OpStatus::ERR_NO_MEMORY;

	for (INTBIG i=0; i < static_cast<INTBIG>(m_end_of_central_directory.entries_here); ++i)
	{
		central_directory_record* fh = m_Files[i];
		m_index_list[i] = OP_NEW(index_entry, ());

		OP_STATUS status = OpStatus::ERR_NO_MEMORY;
		if (m_index_list[i])
		{
			if (fh->general_flags & ZIP_FILENAME_UTF8)
				status = m_index_list[i]->name.SetFromUTF8(fh->GetName(), fh->filename_len);
			else
				status = m_index_list[i]->name.Set(fh->GetName(), fh->filename_len);
		}

		if (OpStatus::IsError(status))
		{
			for (; i>=0; i--)
				OP_DELETE(m_index_list[i]);

			OP_DELETEA(m_index_list);
			m_index_list = NULL;
			return status;
		}
		m_index_list[i]->idx = i;
	}

	BOOL case_sensitive =
#ifdef ZIPFILE_DIRECT_ACCESS_SUPPORT
		(m_flags & OPFILE_FLAGS_CASE_SENSITIVE_ZIP) != 0;
#else
		FALSE;
#endif // ZIPFILE_DIRECT_ACCESS_SUPPORT

	int (*compare)(const void *, const void *);
	compare = case_sensitive ? compareindexnames_casesensitive : compareindexnames;

	op_qsort(reinterpret_cast<void *>(m_index_list),
			 static_cast<size_t>(m_end_of_central_directory.entries_here),
			 sizeof(index_entry *), compare);

	return OpStatus::OK;
}

void OpZip::CleanIndex()
{
	for (UINTBIG i=0; i < m_end_of_central_directory.entries_here; ++i)
	{
		OP_DELETE(m_index_list[i]);
	}
	OP_DELETEA(m_index_list);
	m_index_list = NULL;
}

int OpZip::compareindexnames(const void *arg1, const void *arg2)
{
	register index_entry * const *entry1 = reinterpret_cast<index_entry * const *>(arg1);
	register index_entry * const *entry2 = reinterpret_cast<index_entry * const *>(arg2);

	if ((*entry1)->name.IsEmpty())
		return -1;
	else if ((*entry2)->name.IsEmpty())
		return 1;

	return uni_stricmp((*entry1)->name.CStr(), (*entry2)->name.CStr());
}

int OpZip::compareindexnames_casesensitive(const void *arg1, const void *arg2)
{
	register index_entry * const *entry1 = reinterpret_cast<index_entry * const *>(arg1);
	register index_entry * const *entry2 = reinterpret_cast<index_entry * const *>(arg2);

	if ((*entry1)->name.IsEmpty())
		return -1;
	else if ((*entry2)->name.IsEmpty())
		return 1;

	return uni_strcmp((*entry1)->name.CStr(), (*entry2)->name.CStr());
}

/***********************************************************************************
**
**	GetFile
**
***********************************************************************************/

long OpZip::GetFile(OpString* pstrName, unsigned char*& data)
{
	OpZipFile file;
	int fileindex = GetFileIndex(pstrName, &file);
	if (fileindex != -1 && OpStatus::IsSuccess(GetFile(fileindex, &file)) )
	{
		unsigned long dwSize = file.GetSize(), bytes_read;
		RETURN_VALUE_IF_NULL(data = OP_NEWA(unsigned char, dwSize), -1);
		if (OpStatus::IsError(file.Read(data, dwSize, &bytes_read)) || bytes_read != dwSize)
		{
			OP_DELETEA(data);
			data = NULL;
			dwSize = ~0ul;
		}
		file.Close();
		return dwSize;
	}
	else
	{
		if (data)
		{
			OP_DELETEA(data);
			data = NULL;
		}
		return -1;
	}
}

/***********************************************************************************
**
**	GetOpZipFileHandleL
**
***********************************************************************************/

OpFile* OpZip::GetOpZipFileHandleL(OpString* pstrName)
{
	OpZipFile zfile;
	int fileindex = GetFileIndex(pstrName, &zfile);
	if (fileindex != -1 && OpStatus::IsSuccess(GetFile(fileindex, &zfile)) )
	{
		OP_STATUS ret;

		unsigned long dwSize = zfile.GetSize(), bytes_read;
		unsigned char* data = OP_NEWA(unsigned char, dwSize);
		if (!data)
			return NULL;

		if (OpStatus::IsError(zfile.Read(data, dwSize, &bytes_read)) || bytes_read != dwSize)
		{
			OP_DELETEA(data);
			return NULL;
		}

		OpFile* file = OP_NEW(OpFile, ());
		if (!file)
		{
			OP_DELETEA(data);
			return NULL;
		}

		ret = file->Construct(pstrName->CStr(), OPFILE_TEMP_FOLDER);

		if (OpStatus::IsError(ret))
		{
			OP_DELETEA(data);
			OP_DELETE(file);
			return NULL;
		}

		OpStatus::Ignore(file->Open(OPFILE_WRITE));

		file->Write(data, dwSize);
		file->Close();
		OP_DELETEA(data);
		zfile.Close();
		return file;
	}
	else
		return NULL;
}

/***********************************************************************************
**
**	CreateOpZipFile
**
***********************************************************************************/

#ifndef HAVE_NO_OPMEMFILE
OpMemFile* OpZip::CreateOpZipFile(OpString* pstrName, OP_STATUS *res, int *index)
{
	OpZipFile zfile;
	OP_STATUS result;

	if (res)
		*res = OpStatus::OK;

	int fileindex = GetFileIndex(pstrName, &zfile);
	if (index)
		*index = fileindex;
	if (fileindex == -1)
	{
		if (res)
			*res = OpStatus::ERR_FILE_NOT_FOUND;
		return NULL;
	}

	result = GetFile(fileindex, &zfile);
	if (OpStatus::IsSuccess(result))
	{
		unsigned long dwSize = zfile.GetSize(), bytes_read;
		unsigned char* data = OP_NEWA(unsigned char, dwSize);
		if (!data)
		{
			if (res)
				*res = OpStatus::ERR_NO_MEMORY;
			return NULL;
		}

		OP_STATUS s = zfile.Read(data, dwSize, &bytes_read);
		if (OpStatus::IsError(s) || bytes_read != dwSize)
		{
			OP_DELETEA(data);
			data = NULL;
			if (res)
				*res = OpStatus::IsError(s) ? s : OpStatus::ERR;
		}

#ifdef UTIL_ZIP_WRITE
		OpMemFile* memoryfile = OpMemFile::Create(data, (long)dwSize, TRUE, pstrName->CStr());
#else
		OpMemFile* memoryfile = OpMemFile::Create(data, (long)dwSize, TRUE);
#endif // UTIL_ZIP_WRITE

		if (!memoryfile && res)
			*res = OpStatus::ERR_NO_MEMORY;

		zfile.Close();
		return memoryfile;
	}
	else
	{
		if (res)
			*res = result;
		return NULL;
	}
}
#endif // ndef HAVE_NO_OPMEMFILE

/***********************************************************************************
**
**	Extract
**
***********************************************************************************/

OP_STATUS OpZip::Extract(OpString* pstrZName, OpString* filename)
{
	OpFile file;
	RETURN_IF_ERROR(file.Construct(filename->CStr()));

	unsigned int len = pstrZName->Length();
	if (len > 0 && pstrZName->CStr()[len - 1] == PATHSEPCHAR)
	{
		// Create directory.
		RETURN_IF_ERROR(file.MakeDirectory());
		return OpStatus::OK;
	}
	RETURN_IF_ERROR(file.Open(OPFILE_WRITE));

    unsigned char* filedata = NULL;

	unsigned long filesize = GetFile(pstrZName, filedata);
	OP_STATUS err;
	if (filesize != (unsigned long)-1)
	{
		err = file.Write(filedata, filesize);
	}
	else
	{
		err = OpStatus::ERR;
	}
	OP_DELETEA(filedata);

	OP_STATUS err2 = file.Close();
	return OpStatus::IsError(err) ? err : err2;
}

/***********************************************************************************
**
**	IsFileZipCompatible
**
***********************************************************************************/

/*static*/
BOOL OpZip::IsFileZipCompatible(OpFile* filetocheck)
{
	BOOL close_file = FALSE;

	if (!filetocheck->IsOpen() && filetocheck->GetFullPath())
    {
		OpStatus::Ignore(filetocheck->Open(OPFILE_READ));
		close_file = TRUE;
    }
    if (!filetocheck->IsOpen())
    {
        return FALSE;
    }

	unsigned char readbuf[4]; /* ARRAY OK 2010-04-13 adamm */
	UINT32 sizeofheader_ondisk = 4;
	OpFileLength bytes_read;
	BOOL header_read = OpStatus::IsSuccess(filetocheck->Read(readbuf, sizeofheader_ondisk, &bytes_read))  && bytes_read == sizeofheader_ondisk; // Note: no notification of short reads

	if (close_file)
	{
		filetocheck->Close();
	}

	if (!header_read)
	{
		return FALSE;
	}

	UINT32 sign = BytesToUInt32(readbuf);

    return (sign == LOCAL_SIGNATURE) || (sign == DIR_SIGNATURE);
 }

/***********************************************************************************
**
**	ExtractTo
**
***********************************************************************************/

OP_STATUS
OpZip::ExtractTo(OpString* outpath)
{
    UINTBIG foundidx = 0;

    //loop through the archive and extract the datafiles
    while(foundidx < m_end_of_central_directory.entries_here)
    {
        OpString indexfilename;
		OpString outfilename;
		central_directory_record *fh = m_Files[foundidx];

		if (fh->general_flags & ZIP_FILENAME_UTF8)
		{
			RETURN_IF_ERROR(indexfilename.SetFromUTF8(fh->GetName(), fh->filename_len));
		}
		else
		{
			RETURN_IF_ERROR(indexfilename.Set(fh->GetName(), fh->filename_len));
		}

		RETURN_IF_ERROR(outfilename.Set(outpath->CStr()));
		RETURN_IF_ERROR(outfilename.Append(indexfilename));

		RETURN_IF_ERROR(Extract(&indexfilename, &outfilename));

        ++foundidx;

        fh = m_Files[foundidx];
    }

	return OpStatus::OK;
}

OP_STATUS
OpZip::GetFileNameList(OpVector<OpString>& list)
{
	if (!m_index_list)
	{
		RETURN_IF_ERROR(CreateIndex());
	}

	index_entry** index_list_traverser = m_index_list;
	for (UINTBIG i=0; i < m_end_of_central_directory.entries_here; i++)
	{
#ifdef HAVE_UINT64
		// FIXME: OpVector uses INT32 for indexing, so we currently cannot handle
		// ZIP files with more than 0xFFFFFFFF entries.
		if (static_cast<INT32>((*index_list_traverser)->idx) != ((*index_list_traverser)->idx))
			return OpStatus::ERR_NOT_SUPPORTED;
#endif
		OpString *name = OP_NEW(OpString, ());
		if (!name)
			return OpStatus::ERR_NO_MEMORY;

		OpAutoPtr<OpString> name_ap(name);
		RETURN_IF_ERROR(name->Set((*index_list_traverser)->name));
		RETURN_IF_ERROR(list.Insert(static_cast<INT32>((*index_list_traverser)->idx), name));	// Insert items into list, sorted on original index.
		index_list_traverser++;
		name_ap.release();
	}

	return OpStatus::OK;
}

OP_STATUS
OpZip::IsImplicitDirectory(BOOL& result, const OpStringC& name)
{
	if (!m_index_list)
	{
		RETURN_IF_ERROR(CreateIndex());
	}

	int name_length = name.Length();

	if (name[name_length - 1] == PATHSEPCHAR)
	{
		--name_length;
	}

	index_entry** index_list_traverser = m_index_list;
	for (UINTBIG i=0; i < m_end_of_central_directory.entries_here; i++)
	{
		if ((*index_list_traverser)->name.Compare(name.CStr(), name_length) == 0)
		{
			if ((*index_list_traverser)->name[name_length] == PATHSEPCHAR)
			{
				result = TRUE;
				return OpStatus::OK;
			}
		}

		index_list_traverser++;
	}

	result = FALSE;
	return OpStatus::OK;
}

void OpZip::GetFileAttributes(int index, OpZip::file_attributes *attr)
{
	OP_ASSERT(index >= 0 && static_cast<UINTBIG>(index) < m_end_of_central_directory.entries_here && attr);

	attr->version_made_by = m_Files[index]->version_made_by;
	attr->int_attr = m_Files[index]->int_file_attr;
	attr->ext_attr = m_Files[index]->ext_file_attr;
	attr->date = m_Files[index]->date;
	attr->time = m_Files[index]->time;
	attr->length = m_Files[index]->uncomp_size;
}

////////////////////////////////////////////////////////////////////////////////

#ifdef ZIPFILE_DIRECT_ACCESS_SUPPORT

OpZipFolder::OpZipFolder() :
	m_zip_archive(NULL),
	m_data_file(NULL),
#ifdef UTIL_ZIP_CACHE
	m_own_zip_archive(TRUE),
#endif // UTIL_ZIP_CACHE
	m_current_file(-1)
{
}

OpZipFolder::~OpZipFolder()
{
#ifdef UTIL_ZIP_CACHE
	if (m_own_zip_archive)
		OP_DELETE(m_zip_archive);
	else
	{
		m_zip_archive->DecUsageCount();
		if (!m_zip_archive->GetUsageCount())
			g_zipcache->IsRemovable(m_zip_archive);
	}
#else
	OP_DELETE(m_zip_archive);
#endif // UTIL_ZIP_CACHE
	OP_DELETE(m_data_file);
}

/* static */ OP_STATUS
OpZipFolder::FindLeadingPath(const uni_char *path, size_t path_len, size_t& exist_len)
{
	if (!path)
		return OpStatus::ERR_NULL_POINTER;

	/*
	 * OpLowLevelFile::Create() doesn't take a len param, so we must
	 * NUL-terminate the paths we give to it :(
	 */
	uni_char *p = uni_strdup(path);
	if (!p)
		return OpStatus::ERR_NO_MEMORY;

	size_t i = path_len;
	OP_STATUS s = OpStatus::OK;

	while (i > 0)
	{
		OpLowLevelFile *f;
		BOOL exists = FALSE;

		s = OpLowLevelFile::Create(&f, p);
		if (OpStatus::IsSuccess(s))
			s = f->Exists(&exists);
		OP_DELETE(f);
		if (OpStatus::IsError(s) || exists)
			break;

		while (p[--i] != PATHSEPCHAR && i > 0)
			/* do nothing */;
		p[i] = '\0'; // Truncate p to next path component
	}

	op_free(p);
	exist_len = i;
	return s;
}

/* static */
OP_STATUS OpZipFolder::Create(OpZipFolder **folder, const uni_char *path, int flags)
{
	size_t exist_len, path_len = uni_strlen(path);
	OpAutoPtr<OpZipFolder> zip_folder(NULL);
	OpZip *zip = NULL;
	OpString archive_path, within_path;
	OP_STATUS s;

	*folder = NULL;

#ifdef UTIL_ZIP_CACHE
	if (!g_zipcache)
	{
		g_zipcache = OP_NEW(ZipCache, ());
		if (!g_zipcache)
			return OpStatus::ERR_NO_MEMORY;
	}
#endif // UTIL_ZIP_CACHE

	zip_folder.reset(OP_NEW(OpZipFolder, ()));
	if (!zip_folder.get())
		return OpStatus::ERR_NO_MEMORY;

	// split up the path, and find the zip-file component
#ifdef UTIL_ZIP_CACHE
	g_zipcache->SearchData(path, exist_len, &zip);
	if (zip)
	{
		zip_folder->m_zip_archive = zip;
		zip_folder->m_own_zip_archive = FALSE;
		RETURN_IF_ERROR(zip->IncUsageCount());
		zip->SetOpFileFlags(flags);
	}
	else  // Not found in zip cache
#endif // UTIL_ZIP_CACHE
		RETURN_IF_ERROR(FindLeadingPath(path, path_len, exist_len));

	/*
	 * exist_len splits path into a leading part - denoting a file
	 * (probably a zip archive) that actually exists on disk - and
	 * a trailing path (starting at index exist_len + 1) - denoting
	 * a virtual path within the assumed zip archive.
	 */

	if (exist_len == 0 || exist_len + 1 >= path_len) // no archive, or just archive
		return OpStatus::ERR;
	RETURN_IF_ERROR(archive_path.Set(path, exist_len++));
	RETURN_IF_ERROR(within_path.Set(path + exist_len, path_len - exist_len));
	OP_ASSERT(archive_path.HasContent() && within_path.HasContent());

#ifdef UTIL_ZIP_CACHE
	if (!zip) // Not found in zip cache
#endif // UTIL_ZIP_CACHE
	{
		RETURN_OOM_IF_NULL(zip = OP_NEW(OpZip, (flags)));
		if (OpStatus::IsError(s = zip->Open(archive_path)))
		{
			OP_DELETE(zip);
			return s;
		}
		zip_folder->m_zip_archive = zip;
#ifdef UTIL_ZIP_CACHE
		RETURN_IF_ERROR(g_zipcache->Add(archive_path.CStr(), zip));
		zip_folder->m_own_zip_archive = FALSE;
		RETURN_IF_ERROR(zip->IncUsageCount());
#endif // UTIL_ZIP_CACHE
	}

	OP_ASSERT(zip && zip == zip_folder->m_zip_archive);

	// Initialize the rest of the zip_folder object
	RETURN_IF_ERROR(zip_folder->m_fullpath.Set(path, path_len));
	if (within_path.HasContent())
	{
		zip_folder->m_data_file = zip->CreateOpZipFile(&within_path, &s, &zip_folder->m_current_file);
		if (!zip_folder->m_data_file && s == OpStatus::ERR_FILE_NOT_FOUND)
		{
			/*
			 * ERR_FILE_NOT_FOUND is not a fatal error. The
			 * OpZipFolder object will now refer to a non-existing
			 * file.
			 */
			OP_ASSERT(zip_folder->m_current_file == -1);
			s = OpStatus::OK;
			/*
			 * Special case: If within_path refers to an "implicit"
			 * folder within the zip archive, we signal that by
			 * setting m_current_file to 0 (m_data_file == NULL).
			 */
			BOOL is_dir;
			RETURN_IF_ERROR(zip_folder->CheckIfDirectory(within_path, &is_dir));
			if (is_dir)
			{
				zip_folder->m_current_file = 0;
				s = OpStatus::OK;
			}
		}
		RETURN_IF_ERROR(s);
	}

	// Successfully created OpZipFolder object. Clean up and return:
	*folder = zip_folder.get();
	zip_folder.release();
	return OpStatus::OK;
}

#ifdef UTIL_ZIP_CACHE
OP_STATUS ZipCache::GetData(const uni_char *key, OpZip **zip)
{
	CacheElm* cache_elm;
	RETURN_IF_ERROR(m_zip_files.GetData(key, &cache_elm));
	cache_elm->timer->Start(UTIL_ZIP_CACHE_TIME);
	*zip = cache_elm->zip;
	return OpStatus::OK;
}

OP_STATUS ZipCache::SearchData(const uni_char *path, size_t& key_len, OpZip **zip)
{
	if (
#ifdef SELFTEST
		m_lookup_method == LOOKUP_HASH || (m_lookup_method == LOOKUP_DEFAULT && m_cache.Cardinal() > UTIL_ZIP_CACHE_LINEAR_SEARCH_THRESHOLD)
#else //!SELFTEST
		m_cache.Cardinal() > UTIL_ZIP_CACHE_LINEAR_SEARCH_THRESHOLD
#endif // !SELFTEST
		)
	{
		search_path_buffer.Clear();
		RETURN_IF_ERROR(search_path_buffer.Append(path));
		uni_char* last_pathsep = search_path_buffer.GetStorage() + search_path_buffer.Length();
		while ((last_pathsep = const_cast<uni_char*>(GetLastPathSeparator(search_path_buffer.GetStorage(), last_pathsep))) != NULL)
		{
			*last_pathsep = 0;
			if (OpStatus::IsSuccess(GetData(search_path_buffer.GetStorage(), zip)))
			{
				key_len = last_pathsep - search_path_buffer.GetStorage();
				return OpStatus::OK;
			}
		}
		return OpStatus::ERR;
	}
	else
	{
		CacheElm *cache_elm = (CacheElm*) m_cache.First();
		for (; cache_elm; cache_elm = (CacheElm*) cache_elm->Suc())
		{
			key_len = uni_strlen(cache_elm->key);
			if (uni_strncmp(cache_elm->key, path, key_len) == 0 &&
				path[key_len] == PATHSEPCHAR)
			{
				/* Found match */
				cache_elm->timer->Start(UTIL_ZIP_CACHE_TIME);
				*zip = cache_elm->zip;
				return OpStatus::OK;
			}
		}
	}
	return OpStatus::ERR;
}

OP_STATUS ZipCache::Add(const uni_char *key, OpZip *zip)
{
	OpAutoPtr<CacheElm> cache_elm(OP_NEW(CacheElm, ()));
	OpAutoPtr<OpTimer> timer(OP_NEW(OpTimer, ()));

	if (!cache_elm.get())
		return OpStatus::ERR_NO_MEMORY;
	if (!timer.get())
		return OpStatus::ERR_NO_MEMORY;

	cache_elm->key = uni_strdup(key);
	if (!cache_elm->key)
		return OpStatus::ERR_NO_MEMORY;

	cache_elm->timer = timer.release();

	RETURN_IF_ERROR(m_zip_files.Add(cache_elm->key, cache_elm.get()));
	cache_elm->zip = zip;
	cache_elm->Into(&m_cache);

	cache_elm->timer->Start(UTIL_ZIP_CACHE_TIME);
	cache_elm->timer->SetTimerListener(this);

	cache_elm.release();
	timer.release();

	return OpStatus::OK;
}

void ZipCache::OnTimeOut(OpTimer *timer)
{
	CacheElm *cache_elm = (CacheElm*) m_cache.First();

	for (; cache_elm; cache_elm = (CacheElm*) cache_elm->Suc())
	{
		if (cache_elm->timer == timer)
		{
			OpZip *zip = cache_elm->zip;

			if (zip->GetUsageCount() == 0)
			{
				m_zip_files.Remove(cache_elm->key, &cache_elm);
				cache_elm->Out();
				OP_DELETE(cache_elm);
			}
			return;
		}
	}
}

void ZipCache::IsRemovable(OpZip *zip)
{
	// Find timer associated with this zip object and restart its timer.
	CacheElm *cache_elm = (CacheElm*) m_cache.First();
	for (; cache_elm; cache_elm = (CacheElm*) cache_elm->Suc())
	{
		if (cache_elm->zip == zip)
			cache_elm->timer->Start(UTIL_ZIP_CACHE_TIME);
	}
}

void ZipCache::FlushUnused()
{
	OpZip *zip;

	CacheElm *cache_elm = (CacheElm*) m_cache.First();
	while(cache_elm)
	{
		CacheElm* tmp = cache_elm;
		cache_elm = (CacheElm*) cache_elm->Suc();

		zip = tmp->zip;
		if (zip->GetUsageCount() == 0)
		{
			m_zip_files.Remove(tmp->key, &tmp);
			tmp->Out();
			OP_DELETE(tmp);
		}
	}
}

void ZipCache::FlushIfUnused(OpZip *aZip)
{
	if (aZip->GetUsageCount() == 0)
	{
		OpZip *zip;

		CacheElm *cache_elm = (CacheElm*) m_cache.First();
		while(cache_elm)
		{
			CacheElm* tmp = cache_elm;
			cache_elm = (CacheElm*) cache_elm->Suc();

			zip = tmp->zip;
			if (zip == aZip)
			{
				m_zip_files.Remove(tmp->key, &tmp);
				tmp->Out();
				OP_DELETE(tmp);
			}
		}
	}
}

BOOL ZipCache::IsZipFile(const uni_char* path, OpZip** output_zip)
{
	// First, check if we already have a matching zip file in cache.
	OpZip *zip = NULL;
	if (OpStatus::IsError(GetData(path, &zip)) || !zip)
	{
		// If not then try to open it.
		zip = OP_NEW(OpZip, (0));
		if (!zip)
			return FALSE;
		OpAutoPtr<OpZip> zip_ap(zip);
		RETURN_VALUE_IF_ERROR(zip->Open(path, FALSE), FALSE);
		RETURN_VALUE_IF_ERROR(Add(path, zip), FALSE);
		zip_ap.release();
	}
	if (output_zip)
		*output_zip = zip;
	return TRUE;
}

ZipCache::ZipCache()
#ifdef SELFTEST
	: m_lookup_method(LOOKUP_DEFAULT)
#endif
{
}

ZipCache::~ZipCache()
{
	CacheElm *cache_elm = (CacheElm*) m_cache.First();
	for (; cache_elm; cache_elm = (CacheElm*) cache_elm->Suc())
	{
		m_zip_files.Remove(cache_elm->key, &cache_elm);
		OP_ASSERT(cache_elm->zip->GetUsageCount() == 0);
	}
	m_cache.Clear();
}

OP_STATUS OpZip::IncUsageCount()
{
#ifndef CAN_DELETE_OPEN_FILE
	if (0 == m_usage_count && NULL != m_thisfile && !m_thisfile->IsOpen())
	{
		// Check if the archive has changed since the last time we accessed it,
		// and rebuild the metadata if it has.
		BOOL dirty = TRUE;
		OpFileLength new_file_length = FILE_LENGTH_NONE;
		RETURN_IF_ERROR(m_thisfile->GetFileLength(new_file_length));
		if (new_file_length == m_sizeofFile)
		{
			time_t new_mod_time = 0;
			RETURN_IF_ERROR(m_thisfile->GetLastModified(new_mod_time));
			if (new_mod_time == m_mod_time)
			{
				dirty = FALSE;
			}
		}
		if (dirty)
		{
			if (m_index_list)
			{
				CleanIndex();
			}
			OpString archive_path;
			RETURN_IF_ERROR(archive_path.Set(m_thisfile->GetFullPath()));
#ifdef UTIL_ZIP_WRITE
			RETURN_IF_ERROR(Open(archive_path, !m_read_only));
#else
			RETURN_IF_ERROR(Open(archive_path));
#endif // UTIL_ZIP_WRITE
		}
		else
		{
#ifdef UTIL_ZIP_WRITE
			const int mode = m_read_only
					? OPFILE_READ : OPFILE_UPDATE | OPFILE_SHAREDENYWRITE;
			RETURN_IF_ERROR(m_thisfile->Open(mode));
#else
			RETURN_IF_ERROR(m_thisfile->Open(OPFILE_READ));
#endif // UTIL_ZIP_WRITE
		}
	}
#endif // CAN_DELETE_OPEN_FILE
	m_usage_count++;
	return OpStatus::OK;
}

OP_STATUS OpZip::DecUsageCount()
{
	OP_ASSERT(m_usage_count > 0);
	m_usage_count--;
#ifndef CAN_DELETE_OPEN_FILE
	if (0 == m_usage_count && NULL != m_thisfile)
	{
		RETURN_IF_ERROR(m_thisfile->GetLastModified(m_mod_time));
		RETURN_IF_ERROR(m_thisfile->Close());
	}
#endif // CAN_DELETE_OPEN_FILE
	return OpStatus::OK;
}

UINT32 OpZip::GetUsageCount()
{
	return m_usage_count;
}

void OpZip::SetOpFileFlags(int flags)
{
	int old_flags = m_flags;
	m_flags = flags;
	if ((old_flags & OPFILE_FLAGS_CASE_SENSITIVE_ZIP) != (m_flags & OPFILE_FLAGS_CASE_SENSITIVE_ZIP)
		&& m_index_list)
	{
		CleanIndex();
	}
}
#endif // UTIL_ZIP_CACHE

OP_STATUS
OpZipFolder::CheckIfDirectory(const OpStringC &dir, BOOL *is_dir)
{
	return m_zip_archive->IsImplicitDirectory(*is_dir, dir);
}

OP_STATUS
OpZipFolder::Exists(BOOL* exists) const
{
	if (m_data_file)
		*exists = TRUE;
	else if (m_current_file > -1) // Directory without entry
		*exists = TRUE;
	else
		*exists = FALSE;
	return OpStatus::OK;
}

OP_STATUS
OpZipFolder::Read(void* data, OpFileLength len, OpFileLength* bytes_read)
{
	if (!bytes_read)
		return OpStatus::ERR_NULL_POINTER;
	if (!m_data_file)
		return OpStatus::ERR;
	return m_data_file->Read(data, len, bytes_read);
}

OP_STATUS
OpZipFolder::GetFilePos(OpFileLength* pos) const
{
	if (!m_data_file)
		return OpStatus::ERR;
	return m_data_file->GetFilePos(*pos);
}

OP_STATUS
OpZipFolder::SetFilePos(OpFileLength pos, OpSeekMode seek_mode)
{
	if (!m_data_file)
		return OpStatus::ERR;
	return m_data_file->SetFilePos(pos, seek_mode);
}

BOOL
OpZipFolder::Eof() const
{
	if (!m_data_file)
		return TRUE;
	return m_data_file->Eof();
}

OP_STATUS
OpZipFolder::Close()
{
	if (!m_data_file)
		return OpStatus::ERR;

	return m_data_file->Close();
}

OP_STATUS
OpZipFolder::Open(int mode)
{
	if (!m_data_file)
		return OpStatus::ERR_FILE_NOT_FOUND;

#ifdef UTIL_ZIP_WRITE
	if (m_zip_archive->m_read_only && (mode & OPFILE_WRITE))
		return OpStatus::ERR_NO_ACCESS;
#endif//UTIL_ZIP_WRITE

	return m_data_file->Open(mode);
}

OP_STATUS
OpZipFolder::GetFileLength(OpFileLength* len) const
{
	if (!m_data_file)
		return OpStatus::ERR;

	*len = m_data_file->GetFileLength();
	return OpStatus::OK;
}

/* static */
BOOL
OpZipFolder::MaybeZipArchive(const uni_char* path)
{
	if (uni_stristr(path, ".zip" PATHSEP)
#ifdef GADGET_SUPPORT
		|| uni_stristr(path, ".wgt" PATHSEP)		// some widget extension
		|| uni_stristr(path, ".us" PATHSEP)		// old Unite (service) extension
		|| uni_stristr(path, ".ua" PATHSEP)		// new Unite (application) extension
# ifdef EXTENSION_SUPPORT
		|| uni_stristr(path, ".oex" PATHSEP)	// extension extension
# endif // EXTENSION_SUPPORT
#endif // GADGET_SUPPORT
		)
	{
		return TRUE;
	}

	return FALSE;
}

OP_STATUS
OpZipFolder::ReadLine(char** data)
{
	if (!m_data_file || !data)
		return OpStatus::ERR;

	*data = NULL;

	OpString8 line;
	OP_STATUS status = m_data_file->ReadLine(line);
	if ( OpStatus::IsError(status) )
		return status;

	SetStr(*data, line.CStr());
	if ( *data == NULL )
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

OP_STATUS OpZipFolder::GetFileInfo(OpFileInfo *info)
{
	if (info->flags & ~(OpFileInfo::LAST_MODIFIED |
						OpFileInfo::MODE |
						OpFileInfo::FULL_PATH |
						OpFileInfo::SERIALIZED_NAME |
						OpFileInfo::WRITABLE |
						OpFileInfo::OPEN |
						OpFileInfo::HIDDEN |
						OpFileInfo::LENGTH))
		// Only the above set of flags is currently supported. If any
		// other flag is set (e.g. OpFileInfo::CREATION_TIME) we
		// return an error:
		return OpStatus::ERR_NOT_SUPPORTED;

	if (info->flags &
		(OpFileInfo::LAST_MODIFIED | OpFileInfo::MODE | OpFileInfo::LENGTH | OpFileInfo::HIDDEN))
	{	// Only get the file attributes for the flags that require them:
		OpZip::file_attributes attr;
		if (m_data_file)
			m_zip_archive->GetFileAttributes(m_current_file, &attr);
		else
			op_memset(&attr, 0, sizeof(attr));

		if (info->flags & OpFileInfo::LAST_MODIFIED)
		{
			if (m_current_file < 0) // file does not exist
				return OpStatus::ERR; // nonsense question

			uni_char date_str[21]; /* ARRAY OK 2011-11-30 peter */
			unsigned int year, month, day, hour, minute, second;
			double utc_time;

			year = 1980 + ((attr.date & 0xFE00) >> 9);
			month = (attr.date & 0x01E0) >> 5;
			day = attr.date & 0x001F;
			hour = (attr.time & 0xF800) >> 11;
			minute = (attr.time & 0x07E0) >> 5;
			second = (attr.time & 0x001F) << 1; // Second is stored divided by 2.

			uni_snprintf(date_str, 21, UNI_L("%04u-%02u-%02uT%02u:%02u:%02uZ"),
						 year, month, day, hour, minute, second);
			utc_time = OpDate::ParseDate(date_str);

			info->last_modified = (int)(utc_time / 1000.0);
		}

		if (info->flags & OpFileInfo::MODE)
		{
			if (m_current_file < 0) // file does not exist
				return OpStatus::ERR; // nonsense question
			else if (!m_data_file)
			{
				// A directory without entry has been opened.
				OP_ASSERT(m_current_file == 0);
				info->mode = OpFileInfo::DIRECTORY;
			}
			else
			{
				UINT8 format;
				UINT8 version = (attr.version_made_by & 0xFF00) >> 8;
				switch (version)
				{
				case 0: // MS-DOS (FAT)
					format = (attr.ext_attr & 0xFF000000) >> 24;
					if (format & 0x10)
						info->mode = OpFileInfo::DIRECTORY;
					else
						info->mode = OpFileInfo::FILE;
					break;

				case 3: // Unix (Ext2)
					format = (attr.ext_attr & 0xF0000000) >> 28;
					switch (format) {
					case 0x4:
						info->mode = OpFileInfo::DIRECTORY; break;
					case 0x8:
						info->mode = OpFileInfo::FILE; break;
					case 0xA:
						info->mode = OpFileInfo::SYMBOLIC_LINK; break;
					default:
						info->mode = OpFileInfo::NOT_REGULAR;
					}
					break;

				default:
					info->mode = OpFileInfo::FILE;
				}
			}
		}

		if (info->flags & OpFileInfo::HIDDEN)
		{
			if (m_current_file < 0) // file does not exist
				return OpStatus::ERR; // nonsense question

			if (attr.ext_attr & 0x02)
				info->hidden = TRUE;
			else
				info->hidden = FALSE;
		}

		if (info->flags & OpFileInfo::LENGTH)
			info->length = attr.length;
	}

	if (info->flags & OpFileInfo::FULL_PATH)
		info->full_path = GetFullPath();

	if (info->flags & OpFileInfo::SERIALIZED_NAME)
		info->serialized_name = GetSerializedName();

	if (info->flags & OpFileInfo::WRITABLE)
		info->writable = FALSE;

	if (info->flags & OpFileInfo::OPEN)
		info->open = IsOpen();

	return OpStatus::OK;
}

#ifdef UTIL_ZIP_WRITE
OP_STATUS OpZipFolder::Write(const void* data, OpFileLength len)
{
	if (!m_data_file)
		return OpStatus::ERR;

	// Write to the data file (update in the memory)
	if(data && len > 0 && m_data_file->Write(data, len) == OpStatus::OK)
	{
		// Update real zip archive
		return m_zip_archive->UpdateArchive(m_data_file, len, Z_DEFAULT_COMPRESSION);
	}

	return OpStatus::ERR;
}
#endif // UTIL_ZIP_WRITE

OpLowLevelFile* OpZipFolder::CreateCopy()
{
	OpZipFolder *copy = NULL;
	if (OpStatus::IsError(OpZipFolder::Create(&copy, m_fullpath.CStr())))
		return NULL;

	return copy;
}

#ifdef DIRECTORY_SEARCH_SUPPORT

static OP_STATUS CheckDirectories(OpVector<OpString>& list)
{
	OpStringHashTable<OpString> dir;
	unsigned int i;
	for (i=0; i<list.GetCount(); i++)
	{
		OpString *path = list.Get(i);
		if (!path || path->IsEmpty())
			continue;

		int pathsep = path->FindLastOf(PATHSEPCHAR);
		while (pathsep > 0)
		{
			BOOL found = dir.Contains(path->CStr());
			if (pathsep == path->Length()-1 && !found)
			{
				path->CStr()[path->Length()-1] = 0;
				found = dir.Contains(path->CStr());
				pathsep = path->FindLastOf(PATHSEPCHAR);
				if (!found)
				{
					RETURN_IF_ERROR(dir.Add(path->CStr(), path));
					continue;
				}
			}
			if (found)
			{
				OpString sub_dir;
				RETURN_IF_ERROR(sub_dir.Set(path->CStr(), pathsep));
				pathsep = sub_dir.FindLastOf(PATHSEPCHAR);
				if (pathsep > 0)
				{
					found = dir.Contains(sub_dir.CStr());
					if (!found)
					{
						OpString *find = OP_NEW(OpString, ());
						if (!find)
							return OpStatus::ERR_NO_MEMORY;
						OpAutoPtr<OpString> find_ap(find);
						RETURN_IF_ERROR(find->Set(sub_dir.CStr(), pathsep));
						if (!dir.Contains(find->CStr()))
						{
							RETURN_IF_ERROR(dir.Add(find->CStr(), find));
							RETURN_IF_ERROR(list.Add(find));
							find_ap.release();
						}
					}
				}
			}
			else
			{
				OpString *find = OP_NEW(OpString, ());
				if (!find)
					return OpStatus::ERR_NO_MEMORY;
				OpAutoPtr<OpString> find_ap(find);
				RETURN_IF_ERROR(find->Set(path->CStr(), pathsep));
				if (!dir.Contains(find->CStr()))
				{
					RETURN_IF_ERROR(dir.Add(find->CStr(), find));
					RETURN_IF_ERROR(list.Add(find));
					find_ap.release();
				}
				pathsep = find->FindLastOf(PATHSEPCHAR);
			}
		}
	}

	return OpStatus::OK;
}

OpFolderLister*
OpZipFolder::GetFolderLister(OpFileFolder folder, const uni_char* pattern, const uni_char* path)
{
	OpString full_path;
	if (folder != OPFILE_ABSOLUTE_FOLDER && folder != OPFILE_SERIALIZED_FOLDER)
	{
		if (OpStatus::IsError(g_folder_manager->GetFolderPath(folder, full_path))) return NULL;
	}

	if (OpStatus::IsError(full_path.Append(path))) return NULL;

	OpFolderLister* lister;
	if (OpStatus::IsError(OpZipFolderLister::Create(&lister))) return NULL;
	if (OpStatus::IsError(lister->Construct(full_path.CStr(), pattern)))
	{
		OP_DELETE(lister);
		return NULL;
	}

	return lister;
}


OpZipFolderLister::OpZipFolderLister()
: m_zip(NULL)
, m_current_index(0)
{
}

OpZipFolderLister::~OpZipFolderLister()
{
	OP_DELETE(m_zip);
	m_filenames.DeleteAll();
}

/*static*/ OP_STATUS
OpZipFolderLister::Create(OpFolderLister** new_lister)
{
	*new_lister = OP_NEW(OpZipFolderLister, ());
	if (!*new_lister)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

OP_STATUS
OpZipFolderLister::Construct(const uni_char* path, const uni_char* pattern)
{
	size_t path_len = uni_strlen(path), exists_len;
	OpString match_pattern;

	m_zip = OP_NEW(OpZip, ());
	if (!m_zip)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(OpZipFolder::FindLeadingPath(path, path_len, exists_len));
	if (!exists_len)
		return OpStatus::ERR;
	RETURN_IF_ERROR(m_zip_fullpath.Set(path, exists_len++));
	RETURN_IF_ERROR(m_path_inside.Set(path + exists_len, path_len - MIN(exists_len, path_len)));

	RETURN_IF_ERROR(m_zip->Open(&m_zip_fullpath, FALSE));
	RETURN_IF_ERROR(m_zip->GetFileNameList(m_filenames));
	RETURN_IF_ERROR(CheckDirectories(m_filenames));

	if (m_path_inside.HasContent() && m_path_inside.FindLastOf(PATHSEPCHAR) != m_path_inside.Length()-1)
		RETURN_IF_ERROR(m_path_inside.Append(PATHSEP));
	RETURN_IF_ERROR(match_pattern.Set(m_path_inside));
	RETURN_IF_ERROR(match_pattern.Append(pattern));
	OpString match_pattern_folder;
	RETURN_IF_ERROR(match_pattern_folder.Set(match_pattern));
	RETURN_IF_ERROR(match_pattern_folder.Append(PATHSEP));

#ifdef UTIL_CONVERT_TO_BACKSLASH
	StrReplaceChars(match_pattern.CStr(), '\\', '/');
	StrReplaceChars(match_pattern_folder.CStr(), '\\', '/');
	StrReplaceChars(m_path_inside.CStr(), '\\', '/');
#endif // UTIL_CONVERT_TO_BACKSLASH

	unsigned int i;
	for (i=0; i < m_filenames.GetCount();)
	{
		OpString *name = m_filenames.Get(i);
#ifdef UTIL_CONVERT_TO_BACKSLASH
		StrReplaceChars(name->CStr(), '\\', '/');
#endif // UTIL_CONVERT_TO_BACKSLASH

		if (!OpGlob(name->CStr(), match_pattern.CStr()) && !OpGlob(name->CStr(), match_pattern_folder.CStr()) || m_path_inside.HasContent() && (uni_strcmp(name->CStr(), m_path_inside.CStr()) == 0))
			m_filenames.Delete(i);
		else
		{
#ifdef UTIL_CONVERT_TO_BACKSLASH
			StrReplaceChars(name->CStr(), '/', '\\');
#endif // UTIL_CONVERT_TO_BACKSLASH

			OpString tmp;
			RETURN_IF_ERROR(tmp.Set(name->CStr()+m_path_inside.Length()));
			RETURN_IF_ERROR(name->Set(tmp));
			i++;
		}
	}

#ifdef UTIL_CONVERT_TO_BACKSLASH
	StrReplaceChars(m_path_inside.CStr(), '/', '\\');
#endif // UTIL_CONVERT_TO_BACKSLASH

	return OpStatus::OK;
}

BOOL
OpZipFolderLister::Next()
{
	if (m_current_index >= m_filenames.GetCount())
	{
		return FALSE;
	}

	OpString* name = m_filenames.Get(m_current_index);
	m_current_file.Set(*name);
	m_current_fullpath.Set(m_zip_fullpath);
	m_current_fullpath.Append(PATHSEP);
	m_current_fullpath.Append(m_path_inside);
	m_current_fullpath.Append(m_current_file);

	m_current_index++;
	return TRUE;
}

const uni_char*
OpZipFolderLister::GetFileName() const
{
	return m_current_file.CStr();
}

const uni_char*
OpZipFolderLister::GetFullPath() const
{
	return m_current_fullpath.CStr();
}

BOOL
OpZipFolderLister::IsFolder() const
{
	//if the name ends with PATHSEPCHAR, this is a folder
	const uni_char *file = m_current_file.CStr();
	return file[m_current_file.Length()-1] == PATHSEPCHAR;
}
#endif // DIRECTORY_SEARCH_SUPPORT

#endif // ZIPFILE_DIRECT_ACCESS_SUPPORT

#endif // USE_ZLIB
