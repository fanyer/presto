/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#ifdef SEARCH_ENGINE_EXPONENTIAL_GROWTH_FILE

#include "modules/search_engine/ExponentialGrowthFile.h"

//   ExponentialGrowthFile layout before conversion:
//
//                m_virtual_file_length
//                           |
//                           V
//   +--------/   /----------+
//   |ddddddddd...ddddddddddd|
//   +--------/   /----------+
//   0                       ^
//                           |
//                  physical file length
//
//
//   Normal ExponentialGrowthFile layout:
//
//                m_virtual_file_length
//                           |
//                           V
//   +--------/   /----------+--------/   /---+---+
//   |ddddddddd...ddddddddddd|         ...    |mmm|
//   +--------/   /----------+--------/   /---+---+
//   0                                            ^
//                                                |
//                                       physical file length
//
//   ddd = normal file data
//   mmm = metadata:
//     - 8 bytes METADATA_MAGIC_COOKIE
//     - 8 bytes m_virtual_file_length

#define BAD_FILE_POS FILE_LENGTH_NONE
#define OPF_IS_NEGATIVE(a) ((((a>>31)>>31)>>2) != 0)
#define METADATA_SIZE 16
#define METADATA_MAGIC_COOKIE "w,Lp3YpB"

#define ASSERT_AND_RETURN_IF_ERROR(expr) \
	do { \
		OP_STATUS ASSERT_RETURN_IF_ERROR_TMP = expr; \
		OP_ASSERT(OpStatus::IsSuccess(ASSERT_RETURN_IF_ERROR_TMP));\
		if (OpStatus::IsError(ASSERT_RETURN_IF_ERROR_TMP)) \
			return ASSERT_RETURN_IF_ERROR_TMP; \
	} while (0)

ExponentialGrowthFile*
ExponentialGrowthFile::Create(
		OpLowLevelFile* wrapped_file,
		OP_STATUS& status,
		OpFileLength smallest_size,
		BOOL transfer_ownership)
{
	if (!wrapped_file || OPF_IS_NEGATIVE(smallest_size))
	{
		status = OpStatus::ERR;
		return NULL;
	}

	ExponentialGrowthFile* wrapper = OP_NEW(ExponentialGrowthFile, (wrapped_file, nextExpSize(smallest_size), transfer_ownership));
	status = wrapper ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;

	return wrapper;
}

OpLowLevelFile* ExponentialGrowthFile::MakeNormalFile(ExponentialGrowthFile*& file)
{
	OpFileLength physical_file_length;
	OpLowLevelFile* op_file = file->m_f;
	if (OpStatus::IsSuccess(op_file->GetFileLength(&physical_file_length) &&
		OpStatus::IsSuccess(file->EnsureValidVirtualFileLength())))
	{
		OP_ASSERT(physical_file_length == file->m_virtual_file_length);
		if (physical_file_length != file->m_virtual_file_length)
			OpStatus::Ignore(file->ConvertBackToNormalFile());
	}
	file->m_f = NULL;
	OP_DELETE(file);
	file = NULL;
	return op_file;
}

ExponentialGrowthFile::ExponentialGrowthFile(OpLowLevelFile* wrapped_file, OpFileLength smallest_size, BOOL transfer_ownership)
	: FileWrapper(wrapped_file, transfer_ownership),
	  m_smallest_size(smallest_size),
	  m_virtual_file_length(BAD_FILE_POS),
	  m_metadata_needs_write(FALSE)
{
}

ExponentialGrowthFile::~ExponentialGrowthFile()
{
	if (m_f)
		OpStatus::Ignore(WriteMetadata());
}

OP_STATUS ExponentialGrowthFile::SafeClose()
{
	RETURN_IF_ERROR(WriteMetadata());
	return m_f->SafeClose();
}

OP_STATUS ExponentialGrowthFile::Close()
{
	RETURN_IF_ERROR(WriteMetadata());
	return m_f->Close();
}

OP_STATUS ExponentialGrowthFile::Flush()
{
	if ((m_mode & OPFILE_COMMIT) != 0)
		RETURN_IF_ERROR(WriteMetadata());
	return m_f->Flush();
}

BOOL ExponentialGrowthFile::Eof() const
{
	OpFileLength file_pos;
	RETURN_VALUE_IF_ERROR(EnsureValidVirtualFileLength(), TRUE);
	RETURN_VALUE_IF_ERROR(m_f->GetFilePos(&file_pos), TRUE);
	OP_ASSERT(file_pos <= m_virtual_file_length);
	return file_pos >= m_virtual_file_length;
}

OP_STATUS ExponentialGrowthFile::SetFilePos(OpFileLength pos, OpSeekMode mode /* = SEEK_FROM_START */)
{
	OP_ASSERT(pos != BAD_FILE_POS || mode == SEEK_FROM_CURRENT); // Seek-from-current with offset -1 is allowed
	RETURN_IF_ERROR(EnsureValidVirtualFileLength());

	switch (mode)
	{
	case SEEK_FROM_START:
		break;
	case SEEK_FROM_END:
		if (pos <= m_virtual_file_length)
			pos = m_virtual_file_length - pos;
		else
			pos = BAD_FILE_POS;
		break;
	case SEEK_FROM_CURRENT:
		OpFileLength file_pos;
		ASSERT_AND_RETURN_IF_ERROR(m_f->GetFilePos(&file_pos));
		pos += file_pos;
		break;
	}

	OP_ASSERT(pos <= m_virtual_file_length && !OPF_IS_NEGATIVE(pos));
	if (pos == BAD_FILE_POS || pos > m_virtual_file_length || OPF_IS_NEGATIVE(pos))
		return OpStatus::ERR;

	return m_f->SetFilePos(pos);
}

OP_STATUS ExponentialGrowthFile::GetFileLength(OpFileLength* len) const
{
	if (!len)
		return OpStatus::ERR;
	RETURN_IF_ERROR(EnsureValidVirtualFileLength());
	*len = m_virtual_file_length;
	return OpStatus::OK;
}

OP_STATUS ExponentialGrowthFile::Write(const void* data, OpFileLength len)
{
	RETURN_IF_ERROR(EnsureValidVirtualFileLength());
	OP_STATUS status = m_f->Write(data, len);
	OpFileLength file_pos;
	ASSERT_AND_RETURN_IF_ERROR(m_f->GetFilePos(&file_pos));
	if (file_pos > m_virtual_file_length)
		RETURN_IF_ERROR(SetFileLength(file_pos, TRUE));
	return status;
}

OP_STATUS ExponentialGrowthFile::Read(void* data, OpFileLength len, OpFileLength* bytes_read)
{
	OpFileLength file_pos;
	RETURN_IF_ERROR(EnsureValidVirtualFileLength());
	ASSERT_AND_RETURN_IF_ERROR(m_f->GetFilePos(&file_pos));
	OP_ASSERT(file_pos <= m_virtual_file_length);
	if (file_pos > m_virtual_file_length)
		return OpStatus::ERR;
	if (file_pos+len > m_virtual_file_length)
		len = m_virtual_file_length - file_pos;
	return m_f->Read(data, len, bytes_read);
}

/**
 * Round up to the nearest half-step of 2^n
 * The sequence will be 1,2,3,4,6,8,12,16,24,32,48,64,96,128...
 * @param size Current size
 * @return new size
 */
OpFileLength ExponentialGrowthFile::nextExpSize(OpFileLength size)
{
	if (size <= 4)
		return size;
	size --;

	int shift;
	for (shift = 0; size>>shift != 0; shift++)
		;

	if (((size >> (shift-2)) & 1) == 0)
		size = (OpFileLength)3 << (shift-2);
	else
		size = (OpFileLength)1 << shift;

	return size;
}

OpFileLength ExponentialGrowthFile::GetNewPhysicalFileLength(OpFileLength new_file_length) const
{
	OpFileLength required_length = new_file_length + METADATA_SIZE;
	if (required_length <= m_smallest_size)
		return m_smallest_size;

	OpFileLength biggest_increment = EXPONENTIAL_GROWTH_FILE_BIGGEST_INCREMENT;
	if (required_length >= 2*biggest_increment)
		return ((required_length + biggest_increment-1) / biggest_increment) * biggest_increment;

	return nextExpSize(required_length);
}

OP_STATUS ExponentialGrowthFile::SetFileLength(OpFileLength new_file_length, BOOL return_to_current_file_pos)
{
	OpFileLength old_physical_file_length, new_physical_file_length, current_file_pos = 0;

	OP_ASSERT(!OPF_IS_NEGATIVE(new_file_length) || new_file_length == BAD_FILE_POS);
	if (OPF_IS_NEGATIVE(new_file_length) || new_file_length == BAD_FILE_POS)
		return OpStatus::ERR;
	if (new_file_length == m_virtual_file_length)
		return OpStatus::OK;

	m_virtual_file_length = new_file_length;
	m_metadata_needs_write = TRUE;

	ASSERT_AND_RETURN_IF_ERROR(m_f->GetFileLength(&old_physical_file_length));
	new_physical_file_length = GetNewPhysicalFileLength(new_file_length);
	if (new_physical_file_length == old_physical_file_length)
		return OpStatus::OK;

	if (return_to_current_file_pos)
	{
		ASSERT_AND_RETURN_IF_ERROR(m_f->GetFilePos(&current_file_pos));
		OP_ASSERT(current_file_pos <= new_file_length); // return_to_current_file_pos shouldn't be true if we are shortening the file
		if (current_file_pos > new_file_length)
			current_file_pos = new_file_length;
	}

	if (OpStatus::IsError(m_f->SetFileLength(new_physical_file_length)))
	{
		// Unable to expand file (probably disk full). Attempt to set "unconverted" length
		OP_STATUS status = ConvertBackToNormalFile();
		OP_STATUS status2 = OpStatus::OK;
		if (return_to_current_file_pos)
			status2 = m_f->SetFilePos(current_file_pos);
		ASSERT_AND_RETURN_IF_ERROR(status);
		ASSERT_AND_RETURN_IF_ERROR(status2);
		return OpStatus::OK;
	}

	if (new_physical_file_length > old_physical_file_length)
		RETURN_IF_ERROR(WriteMetadata()); // Write something to ensure that the file is physically resized

	if (return_to_current_file_pos)
		ASSERT_AND_RETURN_IF_ERROR(m_f->SetFilePos(current_file_pos));

	return OpStatus::OK;
}

OP_STATUS ExponentialGrowthFile::EnsureValidVirtualFileLength() const
{
	unsigned char metadata[METADATA_SIZE]; /* ARRAY OK 2010-11-24 roarl */
	OpFileLength physical_file_length, current_file_pos, metadata_pos, bytes_read;

	if (m_virtual_file_length != BAD_FILE_POS)
		return OpStatus::OK;

	ASSERT_AND_RETURN_IF_ERROR(m_f->GetFilePos(&current_file_pos));
	ASSERT_AND_RETURN_IF_ERROR(m_f->GetFileLength(&physical_file_length));
	if (physical_file_length < METADATA_SIZE)
		goto fallback;
	metadata_pos = physical_file_length - METADATA_SIZE;

	ASSERT_AND_RETURN_IF_ERROR(m_f->SetFilePos(metadata_pos));
	ASSERT_AND_RETURN_IF_ERROR(m_f->Read(metadata, METADATA_SIZE, &bytes_read));
	OP_ASSERT(bytes_read == METADATA_SIZE);
	if (bytes_read != METADATA_SIZE)
		return OpStatus::ERR_OUT_OF_RANGE; // File too short

	m_virtual_file_length = 0;
	for (int i=8; i<16; i++)
		m_virtual_file_length = (m_virtual_file_length<<8) + metadata[i];

	// If the metadata is not okay, the file is not yet converted
	if (op_strncmp((char*)metadata, METADATA_MAGIC_COOKIE, 8) != 0)
		goto fallback;

	OP_ASSERT(m_virtual_file_length <= metadata_pos && !OPF_IS_NEGATIVE(m_virtual_file_length));
	if (m_virtual_file_length > metadata_pos || OPF_IS_NEGATIVE(m_virtual_file_length))
		goto fallback;

	ASSERT_AND_RETURN_IF_ERROR(m_f->SetFilePos(current_file_pos));
	return OpStatus::OK;

fallback:
	// Assume the file is not yet converted
	m_virtual_file_length = physical_file_length;
	ASSERT_AND_RETURN_IF_ERROR(m_f->SetFilePos(current_file_pos));
	return OpStatus::OK;
}

OP_STATUS ExponentialGrowthFile::WriteMetadata()
{
	unsigned char metadata[METADATA_SIZE]; /* ARRAY OK 2010-11-24 roarl */
	OpFileLength physical_file_length, metadata_pos, tmp;

	if (!m_metadata_needs_write)
		return OpStatus::OK;

	ASSERT_AND_RETURN_IF_ERROR(m_f->GetFileLength(&physical_file_length));
	OP_ASSERT(m_virtual_file_length != BAD_FILE_POS);
	OP_ASSERT(physical_file_length >= m_virtual_file_length);

	m_metadata_needs_write = FALSE; // From this point, it will be written or doesn't need to be written

	if (physical_file_length < m_virtual_file_length + METADATA_SIZE)
	{
		// No room left after m_virtual_file_length to write the metadata. Attempt to set "unconverted" length
		ASSERT_AND_RETURN_IF_ERROR(ConvertBackToNormalFile());
		return OpStatus::OK;
	}

	metadata_pos = physical_file_length - METADATA_SIZE;

	op_memcpy(metadata, METADATA_MAGIC_COOKIE, 8);
	tmp = m_virtual_file_length;
	for (int i=0; i<8; i++) {
		metadata[15-i] = (unsigned char)(tmp & 0xff);
		tmp >>= 8;
	}

	if (OpStatus::IsError(m_f->SetFilePos(metadata_pos)) ||
		OpStatus::IsError(m_f->Write(metadata, METADATA_SIZE)))
	{
		// Unable to expand file (probably disk full). Attempt to set "unconverted" length
		ASSERT_AND_RETURN_IF_ERROR(ConvertBackToNormalFile());
		return OpStatus::OK;
	}

	return OpStatus::OK;
}

OP_STATUS ExponentialGrowthFile::ConvertBackToNormalFile()
{
	OP_STATUS status = m_f->SetFileLength(m_virtual_file_length);
	if (OpStatus::IsError(status))
	{
		// Now we're screwed, there is no way out. Let's hope the error status
		// is propagated and appropriate action is taken.
		m_virtual_file_length = BAD_FILE_POS;
	}
	m_metadata_needs_write = FALSE;
	return status;
}

#undef BAD_FILE_POS
#undef OPF_IS_NEGATIVE
#undef META_DATA_SIZE
#undef METADATA_MAGIC_COOKIE
#undef ASSERT_AND_RETURN_IF_ERROR

#endif // SEARCH_ENGINE_EXPONENTIAL_GROWTH_FILE
