/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#ifdef SEARCH_ENGINE  // to remove compilation errors with ADVANCED_OPVECTOR

#include "modules/pi/system/OpFolderLister.h"

#include "modules/search_engine/BlockStorage.h"
#include "modules/search_engine/BufferedLowLevelFile.h"
#include "modules/search_engine/ExponentialGrowthFile.h"

// To evaluate memory usage when we don't buffer BlockStorage,
// enable this define:
#undef DONT_BUFFER_BLOCK_STORAGE

#define OLD_JOURNAL_FILE_HEADER   0xBBFF0200
#define OLD_JOURNAL_FILE_HEADER_I 0x0002FFBB

#define BLOCK_FILE_HEADER_0201    0xBBFF0201
#define BLOCK_FILE_HEADER_0201_I  0x0102FFBB
#define BLOCK_FILE_HEADER_0202    0xBBFF0202
#define BLOCK_FILE_HEADER_0202_I  0x0202FFBB

#define BLOCK_FILE_HEADER         0xBBFF0203
#define BLOCK_FILE_HEADER_I       0x0302FFBB

// size of a file header
#define FILE_HDR_SIZE  16
// size of a block header
#define BLOCK_HDR_SIZE 8
// size of journal header
#define JOURNAL_HDR_SIZE 8

#define TRANSACTION_NAME_SUFFIX UNI_L("-j")
#define INVALID_JOURNAL_SUFFIX UNI_L("-g")
#define ATOMIC_COMMIT_IN_PROGRESS_SUFFIX UNI_L("-d")

#define RETURN_IF_ERROR_CLEANUP( expr ) do { if (OpStatus::IsError(rv = expr)) goto cleanup; } while (0);

#define DISTANCE_BETWEEN_BITFIELDS		((OpFileLength)m_blocksize * 8 * m_blocksize + m_blocksize)
#define DISTANCE_BEHIND_BITFIELD(pos)	(((pos) - m_blocksize) % DISTANCE_BETWEEN_BITFIELDS)
#define BITFIELD_POS(pos)				((pos) - DISTANCE_BEHIND_BITFIELD(pos))
#define IS_BITFIELD_BLOCK(pos)			(DISTANCE_BEHIND_BITFIELD(pos) == 0)
#define AT_BLOCK_BOUNDARY(pos)			((pos) % m_blocksize == 0)
#define IS_NORMAL_BLOCK(pos)			(pos != 0 && AT_BLOCK_BOUNDARY(pos) && !IS_BITFIELD_BLOCK(pos))
#ifdef _DEBUG
#define IS_START_BLOCK(pos)				(IS_NORMAL_BLOCK(pos) && (!m_start_blocks_supported || IsStartBlock(pos)))
#else
#define IS_START_BLOCK(pos)				IS_NORMAL_BLOCK(pos)
#endif
#define IS_NOT_START_BLOCK(pos)			(pos == 0 || (IS_NORMAL_BLOCK(pos) && (!m_start_blocks_supported || !IsStartBlock(pos))))

#define SWAP_UINT32(a) ((((a) & 0x000000ffU)<<24) + (((a) & 0x0000ff00U)<< 8) + \
						(((a) & 0x00ff0000U)>> 8) + (((a) & 0xff000000U)>>24))

#ifdef HAVE_UINT64
  #define ANY_INT64 UINT64
#elif defined HAVE_INT64
  #define ANY_INT64 INT64
#else
  #undef ANY_INT64
#endif

static OpFileLength SWAP_OFL(OpFileLength i)
{
	OpFileLength tmp_i;
	unsigned char *src = (unsigned char *)&i;
	unsigned char *dst = (unsigned char *)&tmp_i + sizeof(OpFileLength);
	int j;
	for (j = 0; j < (int)sizeof(OpFileLength); ++j)
		*--dst = *src++;
	return tmp_i;
}

OP_STATUS BlockStorage::ReadFully(OpLowLevelFile *f, void* data, OpFileLength len)
{
	OpFileLength bytes_read;
	while (len > 0) {
		RETURN_IF_ERROR(f->Read(data, len, &bytes_read));
		if (bytes_read == 0)
			return OpStatus::ERR_OUT_OF_RANGE; // File too short
		len -= bytes_read;
		data = (void*)((char*)data + bytes_read);
	}
	return OpStatus::OK;
}

static OP_STATUS ReadInt32Straight(OpLowLevelFile *f, INT32 *i)
{
	return BlockStorage::ReadFully(f, i, 4);
}

static OP_STATUS WriteInt32Straight(OpLowLevelFile *f, INT32 i)
{
	return f->Write(&i, 4);
}

static OP_STATUS ReadInt32Swap(OpLowLevelFile *f, INT32 *i)
{
	UINT32 tmp_i;

	RETURN_IF_ERROR(BlockStorage::ReadFully(f, &tmp_i, 4));
	*i = (INT32)SWAP_UINT32(tmp_i);

	return OpStatus::OK;
}

static OP_STATUS WriteInt32Swap(OpLowLevelFile *f, INT32 i)
{
	i = (UINT32) SWAP_UINT32((UINT32) i);

	return f->Write(&i, 4);
}

static OP_STATUS ReadInt64Straight(OpLowLevelFile *f, OpFileLength *i, UINT32 *msb32)
{
	UINT32 tmp_i;

#ifdef ANY_INT64
	if (sizeof(OpFileLength) == 8) {
		RETURN_IF_ERROR(BlockStorage::ReadFully(f, i, 8));
		if (msb32 != NULL)
			*msb32 = (UINT32)((ANY_INT64)*i >> 32);

		return OpStatus::OK;
	}
#else
	OP_ASSERT(sizeof(OpFileLength) == 4);
#endif

	if (msb32 == NULL)
		msb32 = &tmp_i;

#ifndef OPERA_BIG_ENDIAN
	RETURN_IF_ERROR(BlockStorage::ReadFully(f, i, 4));
	return          BlockStorage::ReadFully(f, msb32, 4);
#else
	RETURN_IF_ERROR(BlockStorage::ReadFully(f, msb32, 4));
	return          BlockStorage::ReadFully(f, i, 4);
#endif
}

static OP_STATUS WriteInt64Straight(OpLowLevelFile *f, OpFileLength i, UINT32 msb32)
{
#ifdef ANY_INT64
	if (sizeof(OpFileLength) == 8) {
		if (msb32 != 0)
			i |= (OpFileLength)((ANY_INT64)msb32 << 32);
		return f->Write(&i, 8);
	}
#else
	OP_ASSERT(sizeof(OpFileLength) == 4);
#endif

#ifndef OPERA_BIG_ENDIAN
	RETURN_IF_ERROR(f->Write(&i, 4));
	return          f->Write(&msb32, 4);
#else
	RETURN_IF_ERROR(f->Write(&msb32, 4));
	return          f->Write(&i, 4);
#endif
}

static OP_STATUS ReadInt64Swap(OpLowLevelFile *f, OpFileLength *i, UINT32 *msb32)
{
	UINT32 tmp_i;
	OpFileLength tmp;

#ifdef ANY_INT64
	if (sizeof(OpFileLength) == 8) {
		RETURN_IF_ERROR(BlockStorage::ReadFully(f, &tmp, 8));
		tmp = SWAP_OFL(tmp);
		*i = tmp;
		if (msb32 != NULL)
			*msb32 = (UINT32)((ANY_INT64)tmp >> 32);

		return OpStatus::OK;
	}
#else
	OP_ASSERT(sizeof(OpFileLength) == 4);
#endif

	if (msb32 == NULL)
		msb32 = &tmp_i;

#ifndef OPERA_BIG_ENDIAN
	RETURN_IF_ERROR(ReadInt32Swap(f, (INT32*)msb32));
	return          ReadInt32Swap(f, (INT32*)i);
#else
	RETURN_IF_ERROR(ReadInt32Swap(f, (INT32*)i));
	return          ReadInt32Swap(f, (INT32*)msb32);
#endif
}

static OP_STATUS WriteInt64Swap(OpLowLevelFile *f, OpFileLength i, UINT32 msb32)
{
#ifdef ANY_INT64
	if (sizeof(OpFileLength) == 8) {
		if (msb32 != 0)
			i |= (OpFileLength)((ANY_INT64)msb32 << 32);
		i = SWAP_OFL(i);
		return f->Write(&i, 8);
	}
#else
	OP_ASSERT(sizeof(OpFileLength) == 4);
#endif

#ifndef OPERA_BIG_ENDIAN
	RETURN_IF_ERROR(WriteInt32Swap(f, (INT32)msb32));
	return          WriteInt32Swap(f, (INT32)i);
#else
	RETURN_IF_ERROR(WriteInt32Swap(f, (INT32)i));
	return          WriteInt32Swap(f, (INT32)msb32);
#endif
}

OP_STATUS BlockStorage::ReadOFL(OpLowLevelFile *f, OpFileLength *ofl)
{
	RETURN_IF_ERROR(ReadInt64(f, ofl, NULL));
	return CheckFilePosition(*ofl);
}

OP_STATUS BlockStorage::WriteOFL(OpLowLevelFile *f, OpFileLength ofl)
{
	RETURN_IF_ERROR(CheckFilePosition(ofl));
	return WriteInt64(f, ofl, 0);
}

OP_STATUS BlockStorage::ReadBlockHeader(OpLowLevelFile *f, OpFileLength *next_pos, BOOL *start_block)
{
	UINT32 msb32;
	RETURN_IF_ERROR(ReadInt64(f, next_pos, &msb32));

#ifdef ANY_INT64
	if (sizeof(OpFileLength) == 8)
#ifdef OPERA_BIG_ENDIAN
		*((unsigned char *)next_pos) &= 0x7F;
#else
		*(((unsigned char *)next_pos) + 7) &= 0x7F;
#endif
#else
	OP_ASSERT(sizeof(OpFileLength) == 4);
#endif
	RETURN_IF_ERROR(CheckFilePosition(*next_pos));

	if (start_block != NULL)
		*start_block = (msb32 & (1u<<31)) == 0 && m_start_blocks_supported;
	return OpStatus::OK;
}

OP_STATUS BlockStorage::WriteBlockHeader(OpLowLevelFile *f, OpFileLength next_pos, BOOL start_block)
{
	INT32 msb32 = !start_block && m_start_blocks_supported ? 1u << 31 : 0;
	RETURN_IF_ERROR(CheckFilePosition(next_pos));
	return WriteInt64(f, next_pos, msb32);
}

OP_STATUS BlockStorage::CheckFilePosition(OpFileLength pos)
{
#ifdef ANY_INT64
	if ((pos >> 31) >> 4 != 0 /* greater than 2^35 = 32GB, or negative */)
	{
		// Clearly something is amiss, ofl should not be this big
		// The search engine will be uselessly slow at this size anyway.

		OP_ASSERT(!"File position was incredibly large, probably a corrupt file. Contact search_engine module owner if reproducible");
		return OpStatus::ERR_OUT_OF_RANGE;
	}
#endif
	if (!AT_BLOCK_BOUNDARY(pos))
	{
		OP_ASSERT(!"File position was not at a block boundary, probably a corrupt file. Contact search_engine module owner if reproducible");
		return OpStatus::ERR_OUT_OF_RANGE;
	}
	return OpStatus::OK;
}

BlockStorage::~BlockStorage(void)
{
	UnGroup();
	if (m_file != NULL)
		Close();
}

static OP_STATUS OpenOpLowLevelFile(OpLowLevelFile **f, const uni_char* path, BlockStorage::OpenMode mode, OpFileFolder folder)
{
	OpString full_path;
	OP_STATUS rv;
	int open_mode;
	BOOL e;


	if (*f != NULL)
	{
		OP_DELETE(*f);
		*f = NULL;
	}

	if (folder != OPFILE_ABSOLUTE_FOLDER && folder != OPFILE_SERIALIZED_FOLDER)
	{
		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(folder, full_path));
	}
	RETURN_IF_ERROR(full_path.Append(path));

	RETURN_IF_ERROR(OpLowLevelFile::Create(f, full_path.CStr(), folder == OPFILE_SERIALIZED_FOLDER));

	if (OpStatus::IsError((*f)->Exists(&e)))
		e = TRUE;

	if (mode == BlockStorage::OpenRead)
	{
		if (e)
			open_mode = OPFILE_READ | OPFILE_SHAREDENYWRITE;
		else
			open_mode = OPFILE_READ | OPFILE_WRITE | OPFILE_SHAREDENYWRITE;
	}
	else {
		if (e)
			open_mode = OPFILE_UPDATE | OPFILE_SHAREDENYWRITE;
		else
			open_mode = OPFILE_READ | OPFILE_WRITE | OPFILE_SHAREDENYWRITE;
	}

	if (OpStatus::IsError(rv = (*f)->Open(open_mode)))
	{
		OP_DELETE(*f);
		*f = NULL;
	}

	return rv;
}

OP_STATUS BlockStorage::Open(const uni_char* path, OpenMode mode, int blocksize, int buffered_blocks, OpFileFolder folder)
{
	m_blocksize = blocksize;
	m_buffered_blocks = buffered_blocks;
	OP_STATUS rv = OpStatus::OK;
	OP_BOOLEAN e;
	UINT32 header = BLOCK_FILE_HEADER;
	OpFileLength file_length;

	if (mode == OpenReadWrite && (blocksize < BLOCK_HDR_SIZE + 4 || blocksize < FILE_HDR_SIZE))
		return OpStatus::ERR_OUT_OF_RANGE;

	m_freeblock = (OpFileLength)(-1);

	do {
		RETURN_IF_ERROR(OpenOpLowLevelFile(&m_file, path, mode, folder));
#ifndef DONT_BUFFER_BLOCK_STORAGE
		if (buffered_blocks > 0)
		{
			m_file = BufferedLowLevelFile::Create(m_file, rv, blocksize * buffered_blocks);
			RETURN_IF_ERROR_CLEANUP(rv);
		}
#endif
#ifdef SEARCH_ENGINE_EXPONENTIAL_GROWTH_FILE
		ExponentialGrowthFile* exp_growth_file = ExponentialGrowthFile::Create(m_file, rv);
		RETURN_IF_ERROR_CLEANUP(rv);
		m_file = exp_growth_file;
#endif
		RETURN_IF_ERROR_CLEANUP(m_file->GetFileLength(&file_length));

		if (file_length < FILE_HDR_SIZE)  // newly created or invalid file
		{  // the file was rewritten
			ReadInt32 = ReadInt32Straight;
			WriteInt32 = WriteInt32Straight;
			ReadInt64 = ReadInt64Straight;
			WriteInt64 = WriteInt64Straight;
			m_start_blocks_supported = TRUE;

			// Since BlockStorage initialization is not protected by transactions, we must take
			// care to set the file-length of the initial block first, for this to work nicely
			// with ExponentialGrowthFile. (CORE-39213)
			RETURN_IF_ERROR_CLEANUP(m_file->SetFileLength(m_blocksize));
			RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(0));

			RETURN_IF_ERROR_CLEANUP(WriteOFL(m_file, 0));  // free block

			header = BLOCK_FILE_HEADER;
			RETURN_IF_ERROR_CLEANUP(m_file->Write(&header, 4));

			RETURN_IF_ERROR_CLEANUP(WriteInt32(m_file, m_blocksize));

			if (m_blocksize > FILE_HDR_SIZE)
			{
				char zero = 0;
				for (file_length = FILE_HDR_SIZE; file_length < (unsigned int)m_blocksize; file_length++)
					RETURN_IF_ERROR_CLEANUP(m_file->Write(&zero, 1));
			}

			m_file->Flush();
		}
		else
		{
			RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(BLOCK_HDR_SIZE));
			RETURN_IF_ERROR_CLEANUP(ReadFully(m_file, &header, 4));

			switch (header) {
			case OLD_JOURNAL_FILE_HEADER_I:
				ReadInt32 = ReadInt32Swap;
				WriteInt32 = WriteInt32Swap;
				ReadInt64 = ReadInt64Swap;
				WriteInt64 = WriteInt64Swap;
				m_start_blocks_supported = FALSE;
				// no break
			case OLD_JOURNAL_FILE_HEADER:
				RETURN_IF_ERROR_CLEANUP(e = DeleteAssociatedFile(INVALID_JOURNAL_SUFFIX));
				if (e == OpBoolean::IS_TRUE)
				{
					RETURN_IF_ERROR_CLEANUP(DeleteAssociatedFile(TRANSACTION_NAME_SUFFIX));
				}
				else {
					RETURN_IF_ERROR_CLEANUP(e = DeleteAssociatedFile(TRANSACTION_NAME_SUFFIX));
					if (e == OpBoolean::IS_TRUE)
					{
						m_file->Close();
						m_file->Delete();
						OP_DELETE(m_file);
						m_file = NULL;
						continue;
					}
				}
				header = ((header == OLD_JOURNAL_FILE_HEADER) ? BLOCK_FILE_HEADER_0201 : BLOCK_FILE_HEADER_0201_I);
				if (mode != OpenRead)
				{
					RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(BLOCK_HDR_SIZE));
					RETURN_IF_ERROR_CLEANUP(m_file->Write(&header, 4));
					RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(BLOCK_HDR_SIZE + 4));
				}

				if (header == BLOCK_FILE_HEADER_0201_I)
					break;
				// no break
			case BLOCK_FILE_HEADER:
			case BLOCK_FILE_HEADER_0201:
			case BLOCK_FILE_HEADER_0202:
				ReadInt32 = ReadInt32Straight;
				WriteInt32 = WriteInt32Straight;
				ReadInt64 = ReadInt64Straight;
				WriteInt64 = WriteInt64Straight;
				m_start_blocks_supported = header == BLOCK_FILE_HEADER || header == BLOCK_FILE_HEADER_0202;
				break;
			case BLOCK_FILE_HEADER_I:
			case BLOCK_FILE_HEADER_0201_I:
			case BLOCK_FILE_HEADER_0202_I:
				ReadInt32 = ReadInt32Swap;
				WriteInt32 = WriteInt32Swap;
				ReadInt64 = ReadInt64Swap;
				WriteInt64 = WriteInt64Swap;
				m_start_blocks_supported = header == BLOCK_FILE_HEADER_I || header == BLOCK_FILE_HEADER_0202;
				break;
			default:
				RETURN_IF_ERROR_CLEANUP(DeleteAssociatedFile(TRANSACTION_NAME_SUFFIX));
				RETURN_IF_ERROR_CLEANUP(DeleteAssociatedFile(INVALID_JOURNAL_SUFFIX));
				RETURN_IF_ERROR_CLEANUP(DeleteAssociatedFile(ATOMIC_COMMIT_IN_PROGRESS_SUFFIX));

				m_file->Close();
				m_file->Delete();
				OP_DELETE(m_file);
				m_file = NULL;

				continue;
			}

			INT32 bs;
			if (OpStatus::IsError(ReadInt32(m_file, &bs)))
			{
				m_file->Close();
				OP_DELETE(m_file);
				m_file = NULL;
				return OpStatus::ERR_PARSING_FAILED;
			}
			m_blocksize = bs;
		}

#ifdef SEARCH_ENGINE_EXPONENTIAL_GROWTH_FILE
		if (header == BLOCK_FILE_HEADER_0202 || header == BLOCK_FILE_HEADER_0202_I)
		{
			if (mode == OpenReadWrite)
			{
				// Upgrade to new file format version
				RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(BLOCK_HDR_SIZE));
				RETURN_IF_ERROR_CLEANUP(WriteInt32(m_file, BLOCK_FILE_HEADER)); // Becomes correct according to endianness
			}
		}
		else if (header != BLOCK_FILE_HEADER && header != BLOCK_FILE_HEADER_I && exp_growth_file)
		{
			// If an old format is still around, it cannot be upgraded to use exponential growth
			m_file = ExponentialGrowthFile::MakeNormalFile(exp_growth_file);
		}
#endif

		// Enforce atomic group commit
		if (this == GetFirstInGroup())
		{
			// Check the atomic group commit in progress marker
			if (AssociatedFileExists(ATOMIC_COMMIT_IN_PROGRESS_SUFFIX) == OpBoolean::IS_TRUE)
				m_group_commit_incomplete = TRUE;
		}

		// If we didn't manage to complete last commit atomically, assume committed
		// anyway, so none in the group should roll back
		if (GetFirstInGroup()->m_group_commit_incomplete)
			RETURN_IF_ERROR_CLEANUP(DeleteAssociatedFile(TRANSACTION_NAME_SUFFIX));

		// Last in group deletes the atomic commit marker (for all)
		BOOL last_in_group = TRUE;
		BlockStorage *member;
		for (member = m_next_in_group; member != this; member = member->m_next_in_group)
			if (!member->m_file)
				last_in_group = FALSE; // If another member is not opened, this is not the last
		if (last_in_group)
		{
			OpStatus::Ignore(DeleteAssociatedFile(ATOMIC_COMMIT_IN_PROGRESS_SUFFIX));
			for (member = m_next_in_group; member != this; member = member->m_next_in_group)
				OpStatus::Ignore(member->DeleteAssociatedFile(ATOMIC_COMMIT_IN_PROGRESS_SUFFIX));
		}


		RETURN_IF_ERROR_CLEANUP(e = AssociatedFileExists(TRANSACTION_NAME_SUFFIX));
		if (e == OpBoolean::IS_TRUE)
		{
			if (mode == OpenRead)
			{
				m_file->Close();
				if (OpStatus::IsError(rv = m_file->Open(OPFILE_UPDATE | OPFILE_SHAREDENYWRITE)))
				{
					OP_DELETE(m_file);
					m_file = NULL;
					return rv;
				}
			}

			if (OpStatus::IsError(rv = Rollback()))
			{
				OpStatus::Ignore(DeleteAssociatedFile(TRANSACTION_NAME_SUFFIX));
				m_file->Close();
				m_file->Delete();
				OP_DELETE(m_file);
				m_file = NULL;
				return rv;
			}

			if (mode == OpenRead)
			{
				m_file->Close();
				if (OpStatus::IsError(rv = m_file->Open(OPFILE_READ | OPFILE_SHAREDENYWRITE)))
				{
					OP_DELETE(m_file);
					m_file = NULL;
					return rv;
				}
			}
		}

		break;
	} while (1);

	return OpStatus::OK;

cleanup:
	m_file->Close();
	OP_DELETE(m_file);
	m_file = NULL;
	return rv;
}

void BlockStorage::Close(void)
{
	if (InTransaction())
		OpStatus::Ignore(Commit());

	if (WaitingForGroupCommit()) // Not all in the group were committed. (Disk full or something)
		OpStatus::Ignore(Rollback());

	if (m_file != NULL) {
		m_file->Close();
		OP_DELETE(m_file);
		m_file = NULL;
	}
}

#ifdef SELFTEST
void BlockStorage::Crash(void)
{
	if (m_transaction != NULL)
	{
		m_transaction->Close();
		OP_DELETE(m_transaction);
		m_transaction = NULL;
	}

	if (m_file != NULL) {
		m_file->Close();
		OP_DELETE(m_file);
		m_file = NULL;
	}
}
#endif

OP_STATUS BlockStorage::Clear(int blocksize)
{
	uni_char *fname;
	OP_STATUS rv;

	if (m_file == NULL)
		return OpStatus::ERR;

	if (m_transaction != NULL)
	{
		m_transaction->Close();
		m_transaction->Delete();
		OP_DELETE(m_transaction);
		m_transaction = NULL;
	}

	RETURN_OOM_IF_NULL(fname = uni_strdup(m_file->GetFullPath()));

	Close();

	RETURN_IF_ERROR(DeleteFile(fname));

	if (blocksize <= 0)
		blocksize = m_blocksize;

	rv = Open(fname, OpenReadWrite, blocksize, m_buffered_blocks);

	op_free(fname);

	return rv;
}

BOOL BlockStorage::IsNativeEndian(void) const
{
	return ReadInt32 != ReadInt32Swap;
}

void BlockStorage::SetOnTheFlyCnvFunc(SwitchEndianCallback cb, void *user_arg)
{
	m_EndianCallback = cb;
	m_callback_arg = user_arg;
}

void BlockStorage::SwitchEndian(void *buf, int size)
{
	register unsigned char tmp;
	register int pos;

	--size;
	for (pos = (size - 1) / 2; pos >= 0; --pos)
	{
		tmp = ((unsigned char *)buf)[pos];
		((unsigned char *)buf)[pos] = ((unsigned char *)buf)[size - pos];
		((unsigned char *)buf)[size - pos] = tmp;
	}
}

OP_STATUS BlockStorage::BeginTransaction(BOOL invalidate_journal)
{
	char *bitmap;
	unsigned int i;
	OP_STATUS rv = OpStatus::OK;
	OpFileLength file_length;

	if (m_file == NULL || m_transaction != NULL)
		return OpStatus::ERR;

#ifdef _DEBUG
	m_journal_invalid = invalidate_journal;
#endif

	OP_ASSERT(m_reserved_area == 0 && m_reserved_size == 0 && m_reserved_deleted == INVALID_FILE_LENGTH);

	m_file->Flush();

	RETURN_IF_ERROR(m_journal_compressor.InitCompDict());

	RETURN_IF_ERROR(m_file->GetFileLength(&file_length));

	RETURN_IF_ERROR(OpenAssociatedFile(&m_transaction, OpenReadWrite, TRANSACTION_NAME_SUFFIX));

	// prepare a bit map for saved pages
	RETURN_IF_ERROR_CLEANUP(WriteOFL(m_transaction, file_length));

	if ((bitmap = OP_NEWA(char, (unsigned int)((file_length / m_blocksize + 7) >> 3))) == NULL)
	{
		char zero = 0;

		for (i = (unsigned int)((file_length / m_blocksize + 7) >> 3); i > 0; --i)
			RETURN_IF_ERROR_CLEANUP(m_transaction->Write(&zero, 1));
	}
	else {
		op_memset(bitmap, 0, (unsigned int)((file_length / m_blocksize + 7) >> 3));
		rv = m_transaction->Write(bitmap, (unsigned int)((file_length / m_blocksize + 7) >> 3));

		OP_DELETEA(bitmap);

		if (OpStatus::IsError(rv))
			goto cleanup;
	}

	m_transaction_blocks = (unsigned int)(file_length / m_blocksize);

	m_transaction->Flush();

	RETURN_IF_ERROR_CLEANUP(JournalBlock(0));

	if (invalidate_journal)
	{
		OpLowLevelFile *f = NULL;

		RETURN_IF_ERROR_CLEANUP(OpenAssociatedFile(&f, OpenReadWrite, INVALID_JOURNAL_SUFFIX));

		if (OpStatus::IsError(rv = WriteOFL(f, file_length)))
		{
			f->Close();
			f->Delete();
			OP_DELETE(f);
			goto cleanup;
		}

		f->SafeClose();
		OP_DELETE(f);
	}
	else
		RETURN_IF_ERROR_CLEANUP(DeleteAssociatedFile(INVALID_JOURNAL_SUFFIX)); // delete the file if it exists

	return OpStatus::OK;

cleanup:
	m_transaction->Close();
	m_transaction->Delete();
	OP_DELETE(m_transaction);
	m_transaction = NULL;
	return rv;
}

OP_STATUS BlockStorage::Commit(void)
{
	OP_BOOLEAN rv;

	if (m_transaction == NULL)
		return OpStatus::ERR;

	if (WaitingForGroupCommit())
		return OpStatus::OK; // And we're still WaitingForGroupCommit()...

	RETURN_IF_ERROR(rv = AssociatedFileExists(INVALID_JOURNAL_SUFFIX));
	if (rv == OpBoolean::IS_TRUE)
	{
		OP_ASSERT(0); // you have probably called BeginTransaction(TRUE), but haven't called FlushJournal()
		RETURN_IF_ERROR(FlushJournal());
	}

	OP_ASSERT(m_reserved_area == 0 && m_reserved_size == 0 && m_reserved_deleted == INVALID_FILE_LENGTH);

	m_journal_compressor.FreeCompDict();

	if ((OpStatus::IsError(rv = m_file->SafeClose())))
	{
		if (m_file->IsOpen())
			return rv;
	}
	else
		rv = m_file->Open(OPFILE_UPDATE | OPFILE_SHAREDENYWRITE);

	m_transaction->Close(); // From this point, WaitingForGroupCommit() will return TRUE
	if (m_transaction->IsOpen())
		rv = OpStatus::ERR;

	if (OpStatus::IsError(rv))
	{
		OP_DELETE(m_file);
		m_file = NULL;
		OP_DELETE(m_transaction);
		m_transaction = NULL;
		return rv;
	}

	BlockStorage *group_member;
	BOOL ready = TRUE;
	for (group_member = m_next_in_group; group_member != this; group_member = group_member->m_next_in_group)
		if (group_member->InTransaction() && !group_member->WaitingForGroupCommit())
			ready = FALSE;
	if (ready)
	{
		// Create "atomic group commit in progress" marker
		rv = GetFirstInGroup()->CreateAssociatedFile(ATOMIC_COMMIT_IN_PROGRESS_SUFFIX);
		// From this point, the commit should take effect even if we crash

		// Finish commit for all in group
		group_member = this;
		do
		{
			if (group_member->WaitingForGroupCommit())
			{
				OP_STATUS status = group_member->m_transaction->Delete();
				OP_DELETE(group_member->m_transaction);
				group_member->m_transaction = NULL;
				rv = OpStatus::IsError(rv) ? rv : status;
			}
			group_member = group_member->m_next_in_group;
		}
		while (group_member != this);

		// Remove "atomic group commit in progress" marker
		if (OpStatus::IsSuccess(rv))
			rv = GetFirstInGroup()->DeleteAssociatedFile(ATOMIC_COMMIT_IN_PROGRESS_SUFFIX);
	}

	return rv;
}

OP_STATUS BlockStorage::Rollback(void)
{
	unsigned char in_journal;
	OpFileLength pos, origin=0, original_length=0, file_length, current_file_length;
	OP_BOOLEAN e;
	OP_STATUS rv = OpStatus::OK;
	char *blockbuf = NULL;
	unsigned char *compressed_buf = NULL;
	INT32 c_size;
	unsigned chk_size;

	if (m_file == NULL)
		return OpStatus::ERR;

	if (WaitingForGroupCommit()) // Not all in the group were committed. (Disk full or something)
	{
		// m_transaction is closed, make sure it is reopened
		OP_DELETE(m_transaction);
		m_transaction = NULL;
	}

	RETURN_IF_ERROR(e = AssociatedFileExists(INVALID_JOURNAL_SUFFIX));
	if (e == OpBoolean::IS_TRUE)
	{
		OpLowLevelFile *f = NULL;

		RETURN_IF_ERROR(m_file->GetFileLength(&file_length));

		RETURN_IF_ERROR(OpenAssociatedFile(&f, OpenReadWrite, INVALID_JOURNAL_SUFFIX));
		e = ReadOFL(f, &original_length);
		f->Close();
		OP_DELETE(f);
		RETURN_IF_ERROR(e);

		if (file_length > original_length)
		{
			RETURN_IF_ERROR(m_file->SetFileLength(original_length));
		}

		if (m_transaction == NULL)
		{
			RETURN_IF_ERROR(DeleteAssociatedFile(TRANSACTION_NAME_SUFFIX));
		}
		else {
			m_transaction->Close();
			e = m_transaction->Delete();
			OP_DELETE(m_transaction);
			m_transaction = NULL;

			RETURN_IF_ERROR(e);
		}

		OpStatus::Ignore(DeleteAssociatedFile(INVALID_JOURNAL_SUFFIX));  // no error check

		m_freeblock = INVALID_FILE_LENGTH;

		m_reserved_area = 0;
		m_reserved_size = 0;
		m_reserved_deleted = INVALID_FILE_LENGTH;

		m_journal_compressor.FreeCompDict();

		return OpStatus::OK;
	}

	if (m_transaction == NULL)
	{
		RETURN_IF_ERROR(e = AssociatedFileExists(TRANSACTION_NAME_SUFFIX));
		if (e != OpBoolean::IS_TRUE)
			return e == OpBoolean::IS_FALSE ? OpStatus::ERR : e;

		RETURN_IF_ERROR(OpenAssociatedFile(&m_transaction, OpenRead, TRANSACTION_NAME_SUFFIX));
	}
	else
		m_transaction->Flush();

	RETURN_IF_ERROR_CLEANUP(m_transaction->GetFileLength(&file_length));
	if (file_length == 0)
	{
		// It is an invalid/empty journal, so nothing to roll back. Just hope everything is OK (bug DSK-238400)
		goto rollback_ok;
	}

	RETURN_IF_ERROR_CLEANUP(m_transaction->SetFilePos(0));
	RETURN_IF_ERROR_CLEANUP(ReadOFL(m_transaction, &original_length));
	pos = JOURNAL_HDR_SIZE + ((original_length / m_blocksize + 7) >> 3);
	RETURN_IF_ERROR_CLEANUP(m_transaction->SetFilePos(pos));

	RETURN_OOM_IF_NULL(compressed_buf = OP_NEWA(unsigned char, ((3 * m_blocksize) >> 1) + 8));
	if ((blockbuf = OP_NEWA(char, m_blocksize)) == NULL)
	{
		OP_DELETEA(compressed_buf);
		return OpStatus::ERR_NO_MEMORY;
	}

	while (file_length > pos)
	{
		RETURN_IF_ERROR_CLEANUP(ReadOFL(m_transaction, &origin));
		// check if this page had been successfully journalled
		RETURN_IF_ERROR_CLEANUP(m_transaction->SetFilePos(JOURNAL_HDR_SIZE + ((origin / m_blocksize) >> 3)));
		RETURN_IF_ERROR_CLEANUP(ReadFully(m_transaction, &in_journal, 1));
		if ((in_journal & (1 << ((origin / m_blocksize) & 7))) == 0)
			break;

		RETURN_IF_ERROR_CLEANUP(m_transaction->SetFilePos(pos + 8));

		RETURN_IF_ERROR_CLEANUP(ReadInt32(m_transaction, &c_size));
		OP_ASSERT(c_size <= ((3 * m_blocksize) >> 1) + 8);
		RETURN_IF_ERROR_CLEANUP(ReadFully(m_transaction, compressed_buf, c_size));
		chk_size = m_journal_compressor.Decompress(blockbuf, compressed_buf, c_size);
		OP_ASSERT(chk_size == (unsigned)m_blocksize);
		if (chk_size != (unsigned)m_blocksize) RETURN_IF_ERROR_CLEANUP(OpStatus::ERR);

		RETURN_IF_ERROR_CLEANUP(m_file->GetFileLength(&current_file_length));
		if (current_file_length < origin)
			RETURN_IF_ERROR_CLEANUP(m_file->SetFileLength(origin));
		RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(origin));

		RETURN_IF_ERROR_CLEANUP(m_file->Write(blockbuf, m_blocksize));

		pos += 12 + c_size;
	}

	OP_DELETEA(compressed_buf);
	compressed_buf = NULL;
	OP_DELETEA(blockbuf);
	blockbuf = NULL;

	m_file->Flush();

	m_freeblock = INVALID_FILE_LENGTH;

	RETURN_IF_ERROR_CLEANUP(m_file->GetFileLength(&file_length));
	if (file_length != original_length)
		RETURN_IF_ERROR_CLEANUP(m_file->SetFileLength(original_length));

rollback_ok:
	m_reserved_area = 0;
	m_reserved_size = 0;
	m_reserved_deleted = INVALID_FILE_LENGTH;

	m_transaction->Close();
	m_transaction->Delete();
	OP_DELETE(m_transaction);
	m_transaction = NULL;

	m_journal_compressor.FreeCompDict();

	return OpStatus::OK;

cleanup:
	m_transaction->Close();
	OP_DELETE(m_transaction);
	m_transaction = NULL;
	if (blockbuf != NULL)
		OP_DELETEA(blockbuf);
	if (compressed_buf != NULL)
		OP_DELETEA(compressed_buf);
	return rv;
}

OP_STATUS BlockStorage::PreJournal(OpFileLength pos)
{
	OpFileLength bitfield_pos;

	if (!InTransaction())
		return OpStatus::ERR;

	OP_ASSERT(IS_START_BLOCK(pos));

	if (pos / m_blocksize >= (OpFileLength)m_transaction_blocks)
		return OpStatus::OK;

	while (pos != 0)
	{
		RETURN_IF_ERROR(JournalBlock(pos));

		bitfield_pos = BITFIELD_POS(pos);
		RETURN_IF_ERROR(JournalBlock(bitfield_pos));

		OP_ASSERT(IS_NORMAL_BLOCK(pos));
		RETURN_IF_ERROR(m_file->SetFilePos(pos));
		RETURN_IF_ERROR(ReadBlockHeader(m_file, &pos));
	}

	return OpStatus::OK;
}

OP_STATUS BlockStorage::FlushJournal(void)
{
	OP_STATUS rv = OpStatus::OK;
	OpFileLength bitfield_pos, fsize;

	if (!InTransaction())
		return OpStatus::ERR;

	if (m_reserved_area != 0)
	{
		fsize = m_reserved_area;

		// strip the exceeding piece of file
		if (m_reserved_size != 0)
			RETURN_IF_ERROR(m_file->SetFileLength(fsize));
	}
	else {
		RETURN_IF_ERROR(m_file->Flush());
		RETURN_IF_ERROR(m_file->GetFileLength(&fsize));
	}

	RETURN_IF_ERROR_CLEANUP(m_transaction->SafeClose());

	rv = DeleteAssociatedFile(INVALID_JOURNAL_SUFFIX);
	if (OpStatus::IsError(rv))
	{
		RETURN_IF_ERROR_CLEANUP(m_transaction->Open(OPFILE_UPDATE | OPFILE_SHAREDENYWRITE));
		return rv;
	}

	if (m_freeblock != INVALID_FILE_LENGTH)
	{
		int fi;
		OpFileLength fill = 0;
		unsigned char c;

		RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(0));
		RETURN_IF_ERROR_CLEANUP(ReadOFL(m_file, &bitfield_pos));

		while (bitfield_pos < m_freeblock || (m_freeblock == 0 && bitfield_pos < fsize && bitfield_pos > 0))
		{
			RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(bitfield_pos));

			for (fi = m_blocksize / sizeof(fill); fi > 0; --fi)
				RETURN_IF_ERROR_CLEANUP(m_file->Write(&fill, sizeof(fill)));

			if (m_blocksize % sizeof(fill) != 0)
				RETURN_IF_ERROR_CLEANUP(m_file->Write(&fill, m_blocksize % sizeof(fill)));

			bitfield_pos += DISTANCE_BETWEEN_BITFIELDS;
		}

		// if m_freeblock == 0, it was already cleared
		if (m_freeblock != 0 && m_reserved_deleted != INVALID_FILE_LENGTH && m_reserved_deleted > (OpFileLength)m_blocksize && m_reserved_deleted - m_blocksize > m_freeblock)
		{
			RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(bitfield_pos));

			if (((int)((m_reserved_deleted - bitfield_pos) / m_blocksize) - 1) > 0)
			{
				for (fi = 0; fi < ((int)((m_reserved_deleted - bitfield_pos) / m_blocksize) - 1) / 8; ++fi)
					RETURN_IF_ERROR_CLEANUP(m_file->Write(&fill, 1));

				RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(bitfield_pos + fi));
			}

			RETURN_IF_ERROR_CLEANUP(ReadFully(m_file, &c, 1));

			c &= ((unsigned char)0xFF) << ((int)((m_reserved_deleted - bitfield_pos) / m_blocksize - 1) & 7);

			RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos((OpFileLength)-1, SEEK_FROM_CURRENT));
			RETURN_IF_ERROR_CLEANUP(m_file->Write(&c, 1));
		}

		RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(0));
		RETURN_IF_ERROR_CLEANUP(WriteOFL(m_file, m_freeblock));
	}

	m_reserved_area = 0;
	m_reserved_size = 0;

	m_reserved_deleted = INVALID_FILE_LENGTH;

#ifdef _DEBUG
	m_journal_invalid = FALSE;
#endif

	RETURN_IF_ERROR_CLEANUP(m_transaction->Open(OPFILE_UPDATE | OPFILE_SHAREDENYWRITE));
	return OpStatus::OK;

cleanup:
	m_file->Close();
	OP_DELETE(m_file);
	m_file = NULL;
	m_transaction->Close();
	OP_DELETE(m_transaction);
	m_transaction = NULL;
	return rv;
}

// must be bigger than 1
#define BS_RESERVE_BLOCKS 16

OpFileLength BlockStorage::Reserve()
{
	OpFileLength pos, bitfield_pos;

	OP_ASSERT(!WaitingForGroupCommit());

	if (m_freeblock == INVALID_FILE_LENGTH)
	{
		RETURN_VALUE_IF_ERROR(m_file->SetFilePos(0), 0);
		RETURN_VALUE_IF_ERROR(ReadOFL(m_file, &m_freeblock), 0);
	}

	if (m_freeblock != 0)
	{
		if ((pos = GetFreeBlock(FALSE)) == 0)
			return 0;
	}
	else {
		while (m_reserved_size > 0)
		{
			pos = m_reserved_area;
			m_reserved_area += m_blocksize;
			m_reserved_size -= m_blocksize;

			if (IS_BITFIELD_BLOCK(pos))
				continue;

			return pos;
		}

		m_file->Flush();
		RETURN_VALUE_IF_ERROR(m_file->GetFileLength(&pos), 0);
		RETURN_VALUE_IF_ERROR(m_file->SetFileLength(pos + m_blocksize * BS_RESERVE_BLOCKS), 0);

		m_reserved_area = pos + m_blocksize;  // because pos is already taken as reserved
		m_reserved_size = m_blocksize * (BS_RESERVE_BLOCKS - 1);

		for (bitfield_pos = m_reserved_area - m_blocksize; bitfield_pos < m_reserved_area + m_reserved_size; bitfield_pos += m_blocksize)
		{
			if (IS_BITFIELD_BLOCK(bitfield_pos))
			{
				int fi;
				OpFileLength fill = 0;

				RETURN_VALUE_IF_ERROR(m_file->SetFilePos(bitfield_pos), 0);
				for (fi = m_blocksize / sizeof(fill); fi > 0; --fi)
					RETURN_VALUE_IF_ERROR(m_file->Write(&fill, sizeof(fill)), 0);

				if (m_blocksize % sizeof(fill) != 0)
					RETURN_VALUE_IF_ERROR(m_file->Write(&fill, m_blocksize % sizeof(fill)), 0);
			}
		}

		if (IS_BITFIELD_BLOCK(pos))
		{
			pos += m_blocksize;
			m_reserved_area += m_blocksize;
			m_reserved_size -= m_blocksize;
		}
	}

	return pos;
}

OpFileLength BlockStorage::Write(const void *data, int len, OpFileLength reserved_pos)
{
	void *orig_data;
	int size, orig_len;
	OpFileLength pos = 0;
	OpFileLength head = 0;
	OP_STATUS rv = OpStatus::OK;

	if (m_file == NULL)
		return 0;

#ifdef _DEBUG
	OP_ASSERT(!InTransaction() || !m_journal_invalid);
#endif
	OP_ASSERT(!WaitingForGroupCommit());

	if (reserved_pos == 0)
	{
		if ((head = CreateBlock(0, FALSE)) == 0)
			return 0;
	}
	else {
		RETURN_VALUE_IF_ERROR(m_file->SetFilePos(reserved_pos), 0);
		RETURN_VALUE_IF_ERROR(WriteBlockHeader(m_file, 0, TRUE), 0);
		head = reserved_pos;
	}

	orig_data = (void *)data;
	orig_len = len;

	if (m_EndianCallback != NULL)
		SwitchEndianInData(orig_data, len);

	OP_ASSERT(IS_NORMAL_BLOCK(head));

	size = len > m_blocksize - BLOCK_HDR_SIZE - 4 ? m_blocksize - BLOCK_HDR_SIZE - 4 : len;
	RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(head + BLOCK_HDR_SIZE));

	RETURN_IF_ERROR_CLEANUP(WriteInt32(m_file, len));

	if (size > 0) {
		RETURN_IF_ERROR_CLEANUP(m_file->Write(data, size));

		data = (const char *)data + size;
		len -= size;
	}

	pos = head;

	while (len > 0)
	{
		if ((pos = CreateBlock(pos, pos == head)) == 0) {
			if (!Delete(head))
				goto cleanup; // Avoid warning
			goto cleanup;
		}

		OP_ASSERT(IS_NORMAL_BLOCK(pos));

		size = len > m_blocksize - BLOCK_HDR_SIZE ? m_blocksize - BLOCK_HDR_SIZE : len;
		RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(pos + BLOCK_HDR_SIZE));
		RETURN_IF_ERROR_CLEANUP(m_file->Write(data, size));

		data = (const char *)data + size;
		len -= size;
	}

	m_file->Flush();

	if (m_EndianCallback != NULL)
		SwitchEndianInData(orig_data, orig_len);

	return head;

cleanup:
	if (m_EndianCallback != NULL)
		SwitchEndianInData(orig_data, orig_len);
	return 0;
}

OpFileLength BlockStorage::Write(const void *data, int len)
{
	return Write(data, len, 0);
}

int BlockStorage::DataLength(OpFileLength pos)
{
	INT32 data_length;

	if (m_file == NULL)
		return 0;

	if (!IS_START_BLOCK(pos))
	{
		OP_ASSERT(0);
		return 0;
	}

	RETURN_VALUE_IF_ERROR(m_file->SetFilePos(pos + 8), 0);
	RETURN_VALUE_IF_ERROR(ReadInt32(m_file, &data_length), 0);

	if (data_length < 0 || data_length > 16777216)
	{
		OP_ASSERT(0);   // if you really want to read 16MB of memory, this assert is incorrect
						// proper, but slower check would be m_last_block_header.data_length < GetFileSize()
		return 0;
	}

	return data_length;
}

BOOL BlockStorage::Read(void *data, int len, OpFileLength pos)
{
	void *orig_data;
	int size, orig_len;
	OpFileLength next;
	INT32 maxlen;

	if (m_file == NULL || len <= 0)
		return FALSE;

	orig_data = data;
	orig_len = len;

	OP_ASSERT(IS_START_BLOCK(pos));

	m_file->Flush();

	RETURN_VALUE_IF_ERROR(m_file->SetFilePos(pos), FALSE);
	RETURN_VALUE_IF_ERROR(ReadBlockHeader(m_file, &next), FALSE);
	RETURN_VALUE_IF_ERROR(ReadInt32(m_file, &maxlen), FALSE);

	if (len > maxlen)
	{
		OP_ASSERT(0);
		return FALSE;
	}

	size = len > m_blocksize - BLOCK_HDR_SIZE - 4 ? m_blocksize - BLOCK_HDR_SIZE - 4 : len;
	if (size > 0)
		RETURN_VALUE_IF_ERROR(ReadFully(m_file, data, size), FALSE);

	len -= size;

	while (len > 0)
	{
		if (next == 0)  // not enough data in the file
			return FALSE;

		data = (char *)data + size;

		OP_ASSERT(IS_NOT_START_BLOCK(next));

		RETURN_VALUE_IF_ERROR(m_file->SetFilePos(next), FALSE);
		RETURN_VALUE_IF_ERROR(ReadBlockHeader(m_file, &next), FALSE);

		size = len > m_blocksize - BLOCK_HDR_SIZE ? m_blocksize - BLOCK_HDR_SIZE : len;
		RETURN_VALUE_IF_ERROR(ReadFully(m_file, data, size), FALSE);

		len -= size;
	}

	if (m_EndianCallback != NULL)
		SwitchEndianInData(orig_data, orig_len);

	return TRUE;
}


BOOL BlockStorage::Update(const void *data, int len, OpFileLength pos)
{
	void *orig_data;
	int size, orig_len;
	OpFileLength orig_pos, next;
	OP_STATUS rv = OpStatus::OK;

	if (m_file == NULL)
		return FALSE;

#ifdef _DEBUG
	OP_ASSERT(!InTransaction() || !m_journal_invalid);
#endif
	OP_ASSERT(IS_START_BLOCK(pos));
	OP_ASSERT(!WaitingForGroupCommit());

	RETURN_VALUE_IF_ERROR(m_file->SetFilePos(pos), FALSE);
	RETURN_VALUE_IF_ERROR(ReadBlockHeader(m_file, &next), FALSE);

	if (InTransaction())
		RETURN_VALUE_IF_ERROR(JournalBlock(pos), FALSE);

	orig_data = (void *)data;
	orig_len = len;
	orig_pos = pos;

	if (m_EndianCallback != NULL)
		SwitchEndianInData(orig_data, len);

	RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(pos + BLOCK_HDR_SIZE));

	size = len > m_blocksize - BLOCK_HDR_SIZE - 4 ? m_blocksize - BLOCK_HDR_SIZE - 4 : len;

	RETURN_IF_ERROR_CLEANUP(WriteInt32(m_file, len));

	if (size > 0) {
		RETURN_IF_ERROR_CLEANUP(m_file->Write(data, size));

		data = (const char *)data + size;
		len -= size;
	}

	while (len > 0)
	{
		if (next != 0)
		{
			pos = next;
			if (InTransaction())
				RETURN_IF_ERROR_CLEANUP(JournalBlock(pos));
		}
		else {
			if ((pos = CreateBlock(pos, pos == orig_pos)) == 0)
				goto cleanup;
		}

		OP_ASSERT(IS_NOT_START_BLOCK(pos));

		RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(pos));
		RETURN_IF_ERROR_CLEANUP(ReadBlockHeader(m_file, &next));

		RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(pos + BLOCK_HDR_SIZE));

		size = len > m_blocksize - BLOCK_HDR_SIZE ? m_blocksize - BLOCK_HDR_SIZE : len;
		RETURN_IF_ERROR_CLEANUP(m_file->Write(data, size));

		data = (const char *)data + size;
		len -= size;
	}

	m_file->Flush();

	if (next != 0)
	{
		OP_ASSERT(next != pos);
		RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(pos));
		RETURN_IF_ERROR_CLEANUP(WriteBlockHeader(m_file, 0, pos == orig_pos));
		if (!Delete(next))
			goto cleanup;
	}

	if (m_EndianCallback != NULL)
		SwitchEndianInData(orig_data, orig_len);

	return TRUE;

cleanup:
	if (m_EndianCallback != NULL)
		SwitchEndianInData(orig_data, orig_len);
	return FALSE;
}

BOOL BlockStorage::Update(const void *data, int offset, int len, OpFileLength pos)
{
	void *orig_data;
	int old_size, write_size, orig_len;
	OpFileLength next, orig_pos;
	OP_STATUS rv = OpStatus::OK;

	if (m_file == NULL)
		return FALSE;

#ifdef _DEBUG
	OP_ASSERT(!InTransaction() || !m_journal_invalid);
#endif
	OP_ASSERT(IS_START_BLOCK(pos));
	OP_ASSERT(!WaitingForGroupCommit());

	orig_data = (void *)data;
	orig_len = len;
	orig_pos = pos;

	if (m_EndianCallback != NULL)
		SwitchEndianInData(orig_data, len);

	RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(pos + BLOCK_HDR_SIZE));
	RETURN_IF_ERROR_CLEANUP(ReadInt32(m_file, &old_size));

	if (old_size < offset)
		goto cleanup;

	write_size = m_blocksize;
	next = 0;

	if (old_size < offset + len)
	{
		if (InTransaction())
			RETURN_IF_ERROR_CLEANUP(JournalBlock(pos));

		RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(pos + BLOCK_HDR_SIZE));
		RETURN_IF_ERROR_CLEANUP(WriteInt32(m_file, offset + len));
	}

	if (offset >= m_blocksize - BLOCK_HDR_SIZE - 4)
	{
		offset -= m_blocksize - BLOCK_HDR_SIZE - 4;

		RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(pos));

		RETURN_IF_ERROR_CLEANUP(ReadBlockHeader(m_file, &next));

		while (offset >= m_blocksize - BLOCK_HDR_SIZE)
		{
			pos = next;

			OP_ASSERT(IS_NOT_START_BLOCK(pos));

			RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(next));
			RETURN_IF_ERROR_CLEANUP(ReadBlockHeader(m_file, &next));

			offset -= m_blocksize - BLOCK_HDR_SIZE;
		}

		if (offset != 0)
		{
			pos = next;

			OP_ASSERT(IS_NOT_START_BLOCK(pos));

			RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(next));
			RETURN_IF_ERROR_CLEANUP(ReadBlockHeader(m_file, &next));
		}
	}
	else {
		write_size = len + offset > m_blocksize - BLOCK_HDR_SIZE - 4 ? m_blocksize - BLOCK_HDR_SIZE - 4 - offset : len;

		if (InTransaction())
			RETURN_IF_ERROR_CLEANUP(JournalBlock(pos));

		RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(pos + BLOCK_HDR_SIZE + 4 + offset));

		RETURN_IF_ERROR_CLEANUP(m_file->Write(data, write_size));

		data = (const char *)data + write_size;
		len -= write_size;

		write_size += offset + 4;

		offset = 0;

		if (len > 0)
		{
			RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(pos));
			RETURN_IF_ERROR_CLEANUP(ReadBlockHeader(m_file, &next));
		}
	}

	if (offset != 0)  // write to partially affected block
	{
		write_size = len + offset > m_blocksize - BLOCK_HDR_SIZE ? m_blocksize - BLOCK_HDR_SIZE - offset : len;

		if (InTransaction())
			RETURN_IF_ERROR_CLEANUP(JournalBlock(pos));

		RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(pos + BLOCK_HDR_SIZE + offset));

		RETURN_IF_ERROR_CLEANUP(m_file->Write(data, write_size));

		data = (const char *)data + write_size;
		len -= write_size;

		write_size += offset;
	}

	// pos ~ the previous block, next ~ the block to write / 0 to create one, offset == 0
	while (len > 0)
	{
		if (next != 0)
		{
			pos = next;
			if (InTransaction())
				RETURN_IF_ERROR_CLEANUP(JournalBlock(pos));
		}
		else {
			if ((pos = CreateBlock(pos, pos == orig_pos)) == 0)
				goto cleanup;
		}

		RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(pos));
		RETURN_IF_ERROR_CLEANUP(ReadBlockHeader(m_file, &next));

		RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(pos + BLOCK_HDR_SIZE));

		write_size = len > m_blocksize - BLOCK_HDR_SIZE ? m_blocksize - BLOCK_HDR_SIZE : len;
		RETURN_IF_ERROR_CLEANUP(m_file->Write(data, write_size));

		data = (const char *)data + write_size;
		len -= write_size;
	}

	m_file->Flush();

	if (m_EndianCallback != NULL)
		SwitchEndianInData(orig_data, orig_len);

	return TRUE;

cleanup:
	if (m_EndianCallback != NULL)
		SwitchEndianInData(orig_data, orig_len);
	return FALSE;
}

BOOL BlockStorage::Update(int len, OpFileLength pos)
{
	int size, old_size;
	OpFileLength orig_pos, next;

	if (m_file == NULL)
		return FALSE;

#ifdef _DEBUG
	OP_ASSERT(!InTransaction() || !m_journal_invalid);
#endif
	OP_ASSERT(IS_START_BLOCK(pos));
	OP_ASSERT(!WaitingForGroupCommit());

	RETURN_VALUE_IF_ERROR(m_file->SetFilePos(pos + BLOCK_HDR_SIZE), FALSE);
	RETURN_VALUE_IF_ERROR(ReadInt32(m_file, &old_size), FALSE);

	if (old_size < len)
		return FALSE;

	if (old_size == len)
		return TRUE;

	if (InTransaction())
		RETURN_VALUE_IF_ERROR(JournalBlock(pos), FALSE);

	RETURN_VALUE_IF_ERROR(m_file->SetFilePos(pos + BLOCK_HDR_SIZE), FALSE);

	RETURN_VALUE_IF_ERROR(WriteInt32(m_file, len), FALSE);

	size = len > m_blocksize - BLOCK_HDR_SIZE - 4 ? m_blocksize - BLOCK_HDR_SIZE - 4 : len;

	if (size > 0)
		len -= size;

	if (len / (m_blocksize - BLOCK_HDR_SIZE) == (old_size - size) / (m_blocksize - BLOCK_HDR_SIZE))
	{
		m_file->Flush();
		return TRUE;
	}

	orig_pos = pos;
	RETURN_VALUE_IF_ERROR(m_file->SetFilePos(pos), FALSE);
	RETURN_VALUE_IF_ERROR(ReadBlockHeader(m_file, &next), FALSE);

	while (len > 0)
	{
		if (next == 0)
			break;

		pos = next;

		OP_ASSERT(IS_NOT_START_BLOCK(next));

		RETURN_VALUE_IF_ERROR(m_file->SetFilePos(pos), FALSE);
		RETURN_VALUE_IF_ERROR(ReadBlockHeader(m_file, &next), FALSE);

		size = len > m_blocksize - BLOCK_HDR_SIZE ? m_blocksize - BLOCK_HDR_SIZE : len;

		len -= size;
	}

	m_file->Flush();

	if (next != 0)
	{
		// This is very dangerous. If you see this assert, break the program execution here and run away!
		OP_ASSERT(next != pos);
		RETURN_VALUE_IF_ERROR(m_file->SetFilePos(pos), FALSE);
		RETURN_VALUE_IF_ERROR(WriteBlockHeader(m_file, 0, pos == orig_pos), FALSE);
		return Delete(next);
	}

	return TRUE;
}

BOOL BlockStorage::Delete(OpFileLength pos)
{
	OpFileLength last_block_pos, next_block, file_length, bitfield_pos, min_bf;
	OpFileLength orig_pos = pos;
	int block_no;
	unsigned char c;
	BOOL last_block_deleted = FALSE;

	if (m_file == NULL)
		return FALSE;

#ifdef _DEBUG
	OP_ASSERT(!InTransaction() || !m_journal_invalid);
#endif
	//OP_ASSERT(IS_START_BLOCK(pos));   Allow chopping off the end of a chain from Update
	OP_ASSERT(IS_NORMAL_BLOCK(pos));
	OP_ASSERT(m_reserved_area == 0 && m_reserved_size == 0 && m_reserved_deleted == INVALID_FILE_LENGTH);
	OP_ASSERT(!WaitingForGroupCommit());

	m_file->Flush();

	RETURN_VALUE_IF_ERROR(m_file->GetFileLength(&file_length), FALSE);

	last_block_pos = file_length - m_blocksize;

	if (m_freeblock == INVALID_FILE_LENGTH)
	{
		RETURN_VALUE_IF_ERROR(m_file->SetFilePos(0), FALSE);
		RETURN_VALUE_IF_ERROR(ReadOFL(m_file, &m_freeblock), FALSE);
	}

	min_bf = file_length;

	while (pos != 0)
	{
		RETURN_VALUE_IF_ERROR(m_file->SetFilePos(pos), FALSE);
		RETURN_VALUE_IF_ERROR(ReadBlockHeader(m_file, &next_block), FALSE);
		//OP_ASSERT(IS_NOT_START_BLOCK(next_block));   Allow deletion of "Append" chains
		OP_ASSERT(IS_NORMAL_BLOCK(pos));

		bitfield_pos = BITFIELD_POS(pos);
		block_no = (int)((pos - bitfield_pos) / m_blocksize - 1);

		if (bitfield_pos < min_bf)
			min_bf = bitfield_pos;

		if (InTransaction())
		{
			RETURN_VALUE_IF_ERROR(JournalBlock(bitfield_pos), FALSE);

			// journalling the deleted block isn't really necessary here, but let's rather do it now then in CreateBlock
			RETURN_VALUE_IF_ERROR(JournalBlock(pos), FALSE);
		}

		// NB! Optimize this later by using a cached bitfield for checking if
		// a block is deleted in IsStartBlock(). (roarl)
		if (pos == orig_pos) {
			RETURN_VALUE_IF_ERROR(m_file->SetFilePos(pos), FALSE);
			RETURN_VALUE_IF_ERROR(WriteBlockHeader(m_file, next_block, FALSE), FALSE);
		}

		RETURN_VALUE_IF_ERROR(m_file->SetFilePos(bitfield_pos + (block_no >> 3)), FALSE);

		RETURN_VALUE_IF_ERROR(ReadFully(m_file, &c, 1), FALSE);

		OP_ASSERT((c & (1 << (block_no & 7))) == 0);

		c |= 1 << (block_no & 7);

		RETURN_VALUE_IF_ERROR(m_file->SetFilePos(bitfield_pos + (block_no >> 3)), FALSE);

		RETURN_VALUE_IF_ERROR(m_file->Write(&c, 1), FALSE);

		last_block_deleted |= (pos >= last_block_pos);

		pos = next_block;
	}

	if ((m_freeblock == 0 || m_freeblock > min_bf) && min_bf < file_length)
	{
		m_freeblock = min_bf;
		RETURN_VALUE_IF_ERROR(m_file->SetFilePos(0), FALSE);
		RETURN_VALUE_IF_ERROR(WriteOFL(m_file, m_freeblock), FALSE);
	}

	m_file->Flush();

	if (last_block_deleted)
		RETURN_VALUE_IF_ERROR(Truncate(), FALSE);

	return TRUE;
}

BOOL BlockStorage::WriteUserHeader(unsigned offset, const void *data, int len, int disk_len, int count)
{
	int i;
	char value[16];  /* ARRAY OK 2010-09-24 roarl */

#ifdef _DEBUG
	OP_ASSERT(!InTransaction() || !m_journal_invalid);
#endif
	OP_ASSERT(count >= 1);
	OP_ASSERT(disk_len >= 0);
	OP_ASSERT(disk_len <= 16);  // integers longer than 128 bits are not supported
	OP_ASSERT((disk_len == 0 && len * count <= m_blocksize - FILE_HDR_SIZE) ||
		(disk_len != 0 && disk_len * count <= m_blocksize - FILE_HDR_SIZE));  // user header doesn't fit to the block
	OP_ASSERT(!WaitingForGroupCommit());

	if (m_file == NULL ||
		(disk_len == 0 && len * count > m_blocksize - FILE_HDR_SIZE) ||
		(disk_len != 0 && disk_len * count > m_blocksize - FILE_HDR_SIZE))
		return FALSE;

	if (InTransaction())
		RETURN_VALUE_IF_ERROR(JournalBlock(0), FALSE);

	RETURN_VALUE_IF_ERROR(m_file->SetFilePos(FILE_HDR_SIZE + offset), FALSE);

	if (disk_len == 0)
		return OpStatus::IsSuccess(m_file->Write(data, len * count));

	for (i = 0; i < count; ++i)
	{
		op_memset(value, 0, sizeof(value));
#ifdef OPERA_BIG_ENDIAN
		if (disk_len >= len)
			op_memcpy(value + disk_len - len, (char *)data + i * len, len);
		else
			op_memcpy(value, (char *)data + i * len + (len - disk_len), disk_len);
#else
		op_memcpy(value, (char *)data + i * len, disk_len >= len ? len : disk_len);
#endif

		if (!IsNativeEndian())
			SwitchEndian(value, disk_len);

		RETURN_VALUE_IF_ERROR(m_file->Write(value, disk_len), FALSE);
	}

	return TRUE;
}

BOOL BlockStorage::ReadUserHeader(unsigned offset, void *data, int len, int disk_len, int count)
{
	int i;
	char value[16];  /* ARRAY OK 2010-09-24 roarl */

	OP_ASSERT(count >= 1);
	OP_ASSERT(disk_len >= 0);
	OP_ASSERT(disk_len <= 16);  // integers longer than 128 bits are not supported
	OP_ASSERT((disk_len == 0 && len * count <= m_blocksize - FILE_HDR_SIZE) ||
		(disk_len != 0 && disk_len * count <= m_blocksize - FILE_HDR_SIZE));  // user header doesn't fit to the block

	if (m_file == NULL ||
		(disk_len == 0 && len * count > m_blocksize - FILE_HDR_SIZE) ||
		(disk_len != 0 && disk_len * count > m_blocksize - FILE_HDR_SIZE))
		return FALSE;

	RETURN_VALUE_IF_ERROR(m_file->SetFilePos(FILE_HDR_SIZE + offset), FALSE);

	if (disk_len == 0)
		return OpStatus::IsSuccess(ReadFully(m_file, data, len * count));


	for (i = 0; i < count; ++i)
	{
		op_memset(value, 0, sizeof(value));

		RETURN_VALUE_IF_ERROR(ReadFully(m_file, value, disk_len), FALSE);

		if (!IsNativeEndian())
			SwitchEndian(value, disk_len);

#ifdef OPERA_BIG_ENDIAN
		if (disk_len >= len)
			op_memcpy((char *)data + i * len, value + disk_len - len, len);
		else
			op_memcpy((char *)data + i * len + (len - disk_len), value, disk_len);
#else
		op_memcpy((char *)data + i * len, value, disk_len >= len ? len : disk_len);
#endif
	}

	return TRUE;
}

OpFileLength BlockStorage::Append(const void *data, int len, OpFileLength pos)
{
	void *orig_data;
	int size, orig_len;
	INT32 old_size;
	int old_block_size;
	OP_STATUS rv = OpStatus::OK;

	if (m_file == NULL)
		return 0;

#ifdef _DEBUG
	OP_ASSERT(!InTransaction() || !m_journal_invalid);
#endif
	OP_ASSERT(!WaitingForGroupCommit());

	if (pos == 0)
		old_size = 0;
	else {
		OP_ASSERT(IS_START_BLOCK(pos));
		RETURN_VALUE_IF_ERROR(m_file->SetFilePos(pos + BLOCK_HDR_SIZE), 0);
		RETURN_VALUE_IF_ERROR(ReadInt32(m_file, &old_size), 0);
	}

	orig_data = (void *)data;
	orig_len = len;

	if (m_EndianCallback != NULL)
		SwitchEndianInData(orig_data, len);

	old_block_size = old_size % (m_blocksize - BLOCK_HDR_SIZE - 4);
	if (old_block_size != 0)
	{
		size = len + old_block_size > m_blocksize - BLOCK_HDR_SIZE - 4 ? m_blocksize - BLOCK_HDR_SIZE - 4 - old_block_size : len;

		if (InTransaction())
			RETURN_IF_ERROR_CLEANUP(JournalBlock(pos));

		RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(pos + BLOCK_HDR_SIZE));

		RETURN_IF_ERROR_CLEANUP(WriteInt32(m_file, old_size + size));

		if (size > 0) {
			RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(pos + BLOCK_HDR_SIZE + 4 + old_block_size));

			RETURN_IF_ERROR_CLEANUP(m_file->Write(data, size));

			data = (const char *)data + size;
			len -= size;
			old_size += size;
		}
	}

	while (len > 0)
	{
		if ((pos = CreateBlock(pos, TRUE, TRUE)) == 0)
			goto cleanup;

		size = len > m_blocksize - BLOCK_HDR_SIZE - 4 ? m_blocksize - BLOCK_HDR_SIZE - 4 : len;

		RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(pos + BLOCK_HDR_SIZE));

		RETURN_IF_ERROR_CLEANUP(WriteInt32(m_file, old_size + size));
		RETURN_IF_ERROR_CLEANUP(m_file->Write(data, size));

		data = (const char *)data + size;
		len -= size;
		old_size += size;
	}

	m_file->Flush();

	if (m_EndianCallback != NULL)
		SwitchEndianInData(orig_data, orig_len);

	return pos;

cleanup:
	if (m_EndianCallback != NULL)
		SwitchEndianInData(orig_data, orig_len);
	return 0;
}

BOOL BlockStorage::ReadApnd(void *data, int len, OpFileLength pos)
{
	void *orig_data;
	int size, orig_len;
	register int size_available;
	OpFileLength next;
	INT32 maxlen;

	if (m_file == NULL || len < 0)
		return FALSE;

	OP_ASSERT(IS_START_BLOCK(pos));

	orig_data = data;
	orig_len = len;

	if (len == 0)
		return TRUE;

	m_file->Flush();

	RETURN_VALUE_IF_ERROR(m_file->SetFilePos(pos), FALSE);
	RETURN_VALUE_IF_ERROR(ReadBlockHeader(m_file, &next), FALSE);
	RETURN_VALUE_IF_ERROR(ReadInt32(m_file, &maxlen), FALSE);

	if (len > maxlen)
	{
		OP_ASSERT(0);
		return FALSE;
	}

	size = maxlen % (m_blocksize - BLOCK_HDR_SIZE - 4);
	if (size == 0)
		size = m_blocksize - BLOCK_HDR_SIZE - 4;
	if (size > len)
		size = len;

	data = (char *)data + len - size;

	size_available = maxlen % (m_blocksize - BLOCK_HDR_SIZE - 4);
	if (size < size_available)
		RETURN_VALUE_IF_ERROR(m_file->SetFilePos(pos + BLOCK_HDR_SIZE + 4 + size_available - size), FALSE);

	RETURN_VALUE_IF_ERROR(ReadFully(m_file, data, size), FALSE);

	len -= size;

	size = m_blocksize - BLOCK_HDR_SIZE - 4;

	while (len > 0)
	{
		if (next == 0)  // not enough data in the file
			return FALSE;

		pos = next;
		RETURN_VALUE_IF_ERROR(m_file->SetFilePos(pos), FALSE);
		RETURN_VALUE_IF_ERROR(ReadBlockHeader(m_file, &next), FALSE);

		if (size > len)
			size = len;

		data = (char *)data - size;

		size_available = m_blocksize - BLOCK_HDR_SIZE - 4;
		if (size < size_available)
			RETURN_VALUE_IF_ERROR(m_file->SetFilePos(pos + BLOCK_HDR_SIZE + 4 + size_available - size), FALSE);
		else
			RETURN_VALUE_IF_ERROR(ReadInt32(m_file, &maxlen), FALSE);  // to move the file pointer

		RETURN_VALUE_IF_ERROR(ReadFully(m_file, data, size), FALSE);

		len -= size;
	}

	if (m_EndianCallback != NULL)
		SwitchEndianInData(orig_data, orig_len);

	return TRUE;
}


OpFileLength BlockStorage::CreateBlock(OpFileLength current_block, BOOL current_start_block, BOOL append_mode)
{
	OpFileLength block_pos;
	BOOL start_block = current_block == 0;

	OP_ASSERT(!WaitingForGroupCommit());

	if (m_freeblock == INVALID_FILE_LENGTH)
	{
		RETURN_VALUE_IF_ERROR(m_file->SetFilePos(0), 0);
		RETURN_VALUE_IF_ERROR(ReadOFL(m_file, &m_freeblock), 0);
	}

	if (m_freeblock != 0)
	{
		if ((block_pos = GetFreeBlock(TRUE)) == 0)
			return 0;

		RETURN_VALUE_IF_ERROR(m_file->SetFilePos(block_pos), 0);
	}
	else {
		if (m_reserved_area != 0)
		{
			OP_ASSERT(0);  // mixing Reserve and Write/Update doesn't make sense

			if (m_reserved_size != 0)
			{
				RETURN_VALUE_IF_ERROR(m_file->SetFileLength(m_reserved_area), 0);
			}

			m_reserved_area = 0;
			m_reserved_size = 0;
		}

		m_file->Flush();
		RETURN_VALUE_IF_ERROR(m_file->GetFileLength(&block_pos), 0);

		// This is very dangerous. If you see this assert, break the program execution here and run away!
		OP_ASSERT(current_block < block_pos);

		if (IS_BITFIELD_BLOCK(block_pos))
		{  // create a bitfield for deleted blocks
			int fi;
			OpFileLength fill = 0;

			RETURN_VALUE_IF_ERROR(m_file->SetFileLength(block_pos + m_blocksize * 2), 0);

			RETURN_VALUE_IF_ERROR(m_file->SetFilePos(block_pos), 0);
			for (fi = m_blocksize / sizeof(fill); fi > 0; --fi)
				RETURN_VALUE_IF_ERROR(m_file->Write(&fill, sizeof(fill)), 0);

			if (m_blocksize % sizeof(fill) != 0)
				RETURN_VALUE_IF_ERROR(m_file->Write(&fill, m_blocksize % sizeof(fill)), 0);

			block_pos += m_blocksize;
		}
		else
			RETURN_VALUE_IF_ERROR(m_file->SetFileLength(block_pos + m_blocksize), 0);

		RETURN_VALUE_IF_ERROR(m_file->SetFilePos(block_pos), 0);
	}


	if (append_mode)
	{
		RETURN_VALUE_IF_ERROR(WriteBlockHeader(m_file, current_block, TRUE), 0);
	}
	else
	{
		RETURN_VALUE_IF_ERROR(WriteBlockHeader(m_file, 0, start_block), 0);  // next block is 0
	}

	if (current_block != 0 && !append_mode)
	{
		if (InTransaction())
			RETURN_VALUE_IF_ERROR(JournalBlock(current_block), 0);

		// This is very dangerous. If you see this assert, break the program execution here and run away!
		OP_ASSERT(block_pos != current_block);
		RETURN_VALUE_IF_ERROR(m_file->SetFilePos(current_block), 0);
		RETURN_VALUE_IF_ERROR(WriteBlockHeader(m_file, block_pos, current_start_block), 0);
	}

	m_file->Flush();

	return block_pos;
}

// return a block from the list of deleted blocks
// update_file - GetFreeBlock is called from Update or Write via CreateBlock, !update_file - GetFreeBlock is called from Reserve
// m_freeblock must be > 0 upon a call of this method, therefore at least one non-empty bitfield exists
// m_reserved_deleted must be > 0 or -1
// if update_file, bit field pointed by m_freeblock is modified directly, m_freeblock may be updated to the next non-empty bitfield
// if !update_file, m_reserved_deleted is updated to point to the next free block or 0 on EOF (then m_freeblock would also 0)
OpFileLength BlockStorage::GetFreeBlock(BOOL update_file)
{
	unsigned char *block_buf = NULL;
	unsigned char c, tc;
	int i, j, k;
	BOOL more_exist;
	OpFileLength fsize, block_pos, bitfield_pos;
	unsigned block_no;
	OP_STATUS rv = OpStatus::OK;

	OP_ASSERT(m_freeblock != 0 && m_freeblock != INVALID_FILE_LENGTH);
	OP_ASSERT(!update_file || m_reserved_deleted == INVALID_FILE_LENGTH);
	OP_ASSERT(!WaitingForGroupCommit());

	if (m_reserved_deleted == INVALID_FILE_LENGTH)
	{
		bitfield_pos = m_freeblock;
		block_no = 0;
	}
	else {
		bitfield_pos = BITFIELD_POS(m_reserved_deleted);
		block_no = (int)((m_reserved_deleted - bitfield_pos) / m_blocksize - 1);
	}

	RETURN_VALUE_IF_ERROR(m_file->SetFilePos(bitfield_pos), 0);

	if ((block_buf = OP_NEWA(unsigned char, m_blocksize)) != NULL)
	{
		RETURN_IF_ERROR_CLEANUP(ReadFully(m_file, block_buf, m_blocksize));

		// find the first available deleted block, because it isn't known yet
		if (m_reserved_deleted == INVALID_FILE_LENGTH)
		{
			i = 0;
			j = 0;

			while (i < m_blocksize && block_buf[i] == 0)
				++i;

			OP_ASSERT(i < m_blocksize);
			if (i >= m_blocksize)
				goto cleanup;

			c = block_buf[i];

			while (((1 << j) & c) == 0)
				++j;

			block_no = i * 8 + j;

			block_pos = m_freeblock + ((OpFileLength)i * 8 + j) * m_blocksize + m_blocksize;
		}
		else {
			i = block_no >> 3;
			j = block_no & 7;
			c = block_buf[i];
			block_pos = m_reserved_deleted;
		}

		c &= ((unsigned char)0xFF) << (j + 1);

		more_exist = c != 0;  // this means more in the same bitfield
		k = i;
		if (!more_exist)
		{
			++k;

			while (k < m_blocksize && block_buf[k] == 0)
				++k;

			if (k < m_blocksize)
			{
				more_exist = TRUE;

				j = 0;
				while (((1 << j) & block_buf[k]) == 0)
					++j;
			}
		}
		else {
			do
				++j;
			while (((1 << j) & c) == 0);
		}

		if (InTransaction())
			RETURN_IF_ERROR_CLEANUP(JournalBlock(bitfield_pos));

		if (update_file)
		{
			RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(bitfield_pos + i));
			RETURN_IF_ERROR_CLEANUP(m_file->Write(&c, 1));
		}

		if (!more_exist)  // need to update m_freeblock to point to the next non-empty bitfield
		{
			m_file->Flush();
			RETURN_IF_ERROR_CLEANUP(m_file->GetFileLength(&fsize));

			if (update_file && InTransaction())  // otherwise journalled allready
				RETURN_IF_ERROR_CLEANUP(JournalBlock(0));

			m_freeblock += DISTANCE_BETWEEN_BITFIELDS;

			while (m_freeblock < fsize)
			{
				RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(m_freeblock));

				RETURN_IF_ERROR_CLEANUP(ReadFully(m_file, block_buf, m_blocksize));

				for (i = 0; i < m_blocksize; ++i)
					if (block_buf[i] != 0)
						break;

				if (i < m_blocksize)
					break;

				m_freeblock += DISTANCE_BETWEEN_BITFIELDS;
			}

			if (m_freeblock >= fsize)
			{
				m_freeblock = 0;
				if (!update_file)
					m_reserved_deleted = 0;
			}
			else if (!update_file)
			{
				j = 0;
				while (((1 << j) & block_buf[i]) == 0)
					++j;


				m_reserved_deleted = m_freeblock + ((OpFileLength)i * 8 + j) * m_blocksize + m_blocksize;
			}

			OP_DELETEA(block_buf);

			if (update_file)
			{
				RETURN_VALUE_IF_ERROR(m_file->SetFilePos(0), 0);
				RETURN_VALUE_IF_ERROR(WriteOFL(m_file, m_freeblock), 0);
			}
		}
		else {
			if (!update_file)
				m_reserved_deleted = m_freeblock + ((OpFileLength)k * 8 + j) * m_blocksize + m_blocksize;
			OP_DELETEA(block_buf);
		}

		return block_pos;
	}


	// fallback if we couldn't allocate the buffer

	// find the first available deleted block, because it isn't known yet
	if (m_reserved_deleted == INVALID_FILE_LENGTH)
	{
		i = 0;
		j = 0;

		do {
			RETURN_VALUE_IF_ERROR(ReadFully(m_file, &c, 1), 0);
			if (c != 0)
				break;
			++i;
		} while (i < m_blocksize);

		OP_ASSERT(i < m_blocksize);

		while (((1 << j) & c) == 0)
			++j;

		block_no = i * 8 + j;

		block_pos = m_freeblock + ((OpFileLength)i * 8 + j) * m_blocksize + m_blocksize;
	}
	else {
		i = block_no >> 3;
		j = block_no & 7;

		RETURN_VALUE_IF_ERROR(m_file->SetFilePos(bitfield_pos + i), 0);

		RETURN_VALUE_IF_ERROR(ReadFully(m_file, &c, 1), 0);

		block_pos = m_reserved_deleted;
	}

	c &= ((unsigned char)0xFF) << (j + 1);

	more_exist = c != 0;  // this means more in the same bitfield
	k = i;
	if (!more_exist)
	{
		++k;

		if (k < m_blocksize)
		{
			do {
				RETURN_VALUE_IF_ERROR(ReadFully(m_file, &tc, 1), 0);
				if (tc != 0)
					break;
				++k;
			} while (k < m_blocksize);


			if (k < m_blocksize)
			{
				more_exist = TRUE;

				j = 0;
				while (((1 << j) & tc) == 0)
					++j;
			}
		}
	}
	else {
		do
			++j;
		while (((1 << j) & c) == 0);
	}

	if (InTransaction())
		RETURN_VALUE_IF_ERROR(JournalBlock(bitfield_pos), 0);

	if (update_file)
	{
		RETURN_VALUE_IF_ERROR(m_file->SetFilePos(bitfield_pos + i), 0);
		RETURN_VALUE_IF_ERROR(m_file->Write(&c, 1), 0);
	}

	if (!more_exist)  // need to update m_freeblock to point to the next non-empty bitfield
	{
		m_file->Flush();
		RETURN_VALUE_IF_ERROR(m_file->GetFileLength(&fsize), 0);

		if (update_file && InTransaction())  // otherwise journalled allready
			RETURN_VALUE_IF_ERROR(JournalBlock(0), 0);

		m_freeblock += DISTANCE_BETWEEN_BITFIELDS;

		while (m_freeblock < fsize)
		{
			RETURN_VALUE_IF_ERROR(m_file->SetFilePos(m_freeblock), 0);

			for (i = 0; i < m_blocksize; ++i)
			{
				RETURN_VALUE_IF_ERROR(ReadFully(m_file, &c, 1), 0);
				if (c != 0)
					break;
			}

			if (i < m_blocksize)
				break;

			m_freeblock += DISTANCE_BETWEEN_BITFIELDS;
		}

		if (m_freeblock >= fsize)
		{
			m_freeblock = 0;
			if (!update_file)
				m_reserved_deleted = 0;
		}
		else if (!update_file)
		{
			j = 0;
			while (((1 << j) & c) == 0)
				++j;


			m_reserved_deleted = m_freeblock + ((OpFileLength)i * 8 + j) * m_blocksize + m_blocksize;
		}

		if (update_file)
		{
			RETURN_VALUE_IF_ERROR(m_file->SetFilePos(0), 0);
			RETURN_VALUE_IF_ERROR(WriteOFL(m_file, m_freeblock), 0);
		}
	}
	else {
		if (!update_file)
			m_reserved_deleted = m_freeblock + ((OpFileLength)k * 8 + j) * m_blocksize + m_blocksize;
	}

	return block_pos;

cleanup:
	OP_DELETEA(block_buf);
	return 0;
}

// when Delete calls Truncate, there is at least one free block and at least one block to truncate
OP_STATUS BlockStorage::Truncate(void)
{
	OpFileLength trunc_block, last_block, bitfield_pos;
	int i, j;
	unsigned char *block_buf = NULL;
	unsigned char c;
	OP_STATUS rv = OpStatus::OK;

	OP_ASSERT(!WaitingForGroupCommit());

	if (m_freeblock == INVALID_FILE_LENGTH)
	{
		RETURN_IF_ERROR(m_file->SetFilePos(0));
		RETURN_IF_ERROR(ReadOFL(m_file, &m_freeblock));
	}

	m_file->Flush();
	RETURN_IF_ERROR(m_file->GetFileLength(&last_block));

	last_block -= m_blocksize;

	bitfield_pos = BITFIELD_POS(last_block);

	trunc_block = last_block;

	if ((block_buf = OP_NEWA(unsigned char, m_blocksize)) != NULL)
	{
		RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(bitfield_pos));

		do {
			RETURN_IF_ERROR_CLEANUP(ReadFully(m_file, block_buf, m_blocksize));

			i = (int)((trunc_block - bitfield_pos - 1) / m_blocksize);

			while (i >= 0 && (block_buf[i >> 3] & (1 << (i & 7))) != 0)
			{
				trunc_block -= m_blocksize;
				--i;
			}

			if (i < 0)
			{
				trunc_block -= m_blocksize;

				if (trunc_block > m_freeblock)
				{
					bitfield_pos -= DISTANCE_BETWEEN_BITFIELDS;
					RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(bitfield_pos));
					i = 8 * m_blocksize - 1;
				}
				else {
					m_freeblock = 0;
					RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(0));
					RETURN_IF_ERROR_CLEANUP(WriteOFL(m_file, m_freeblock));

					break;
				}
			}
			else {
				block_buf[i >> 3] &= ((unsigned char)0xFF) >> (8 - (i & 7));
				op_memset(block_buf + (i >> 3) + 1, 0, m_blocksize - (i >> 3) - 1);
				RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(bitfield_pos + (i >> 3)));
				RETURN_IF_ERROR_CLEANUP(m_file->Write(block_buf + (i >> 3), m_blocksize - (i >> 3)));

				i >>= 3;

				// adjust m_freeblock
				while (i >= 0 && block_buf[i] == 0)
					--i;

				// if m_freeblock < bitfield_pos, there are more free blocks
				if (i < 0 && bitfield_pos <= m_freeblock)
				{
					m_freeblock = 0;
					RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(0));
					RETURN_IF_ERROR_CLEANUP(WriteOFL(m_file, m_freeblock));
				}

				break;
			}
		} while (1);

		OP_DELETEA(block_buf);

		return m_file->SetFileLength(trunc_block + m_blocksize);
	}

	// fallback for out of memory
	do {
		i = (int)((trunc_block - bitfield_pos - 1) / m_blocksize);
		do {
			RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(bitfield_pos + (i >> 3)));
			RETURN_IF_ERROR_CLEANUP(ReadFully(m_file, &c, 1));

			if ((c & (1 << (i & 7))) == 0)
				break;

			trunc_block -= m_blocksize;
			--i;
		} while (i >= 0);

		if (i < 0)
		{
			trunc_block -= m_blocksize;

			if (trunc_block > m_freeblock)
			{
				bitfield_pos -= DISTANCE_BETWEEN_BITFIELDS;
				RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(bitfield_pos));
				i = 8 * m_blocksize - 1;
			}
			else {
				m_freeblock = 0;
				RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(0));
				RETURN_IF_ERROR_CLEANUP(WriteOFL(m_file, m_freeblock));

				break;
			}
		}
		else {
			c &= ((unsigned char)0xFF) >> (8 - (i & 7));
			RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(bitfield_pos + (i >> 3)));
			RETURN_IF_ERROR_CLEANUP(m_file->Write(&c, 1));
			c = 0;
			for (j = 1; j < m_blocksize - (i >> 3); ++j)
				RETURN_IF_ERROR_CLEANUP(m_file->Write(&c, 1));

			i >>= 3;

			// adjust m_freeblock
			do {
				RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(bitfield_pos + i));
				RETURN_IF_ERROR_CLEANUP(ReadFully(m_file, &c, 1));

				if (c != 0)
					break;

				--i;
			} while (i >= 0);

			if (i < 0 && bitfield_pos <= m_freeblock)
			{
				m_freeblock = 0;
				RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(0));
				RETURN_IF_ERROR_CLEANUP(WriteOFL(m_file, m_freeblock));
			}

			break;
		}
	} while (1);

	return m_file->SetFileLength(trunc_block + m_blocksize);

cleanup:
	OP_DELETEA(block_buf);
	OpStatus::Ignore(m_file->SetFileLength(trunc_block + m_blocksize));
	return rv;
}

OP_STATUS BlockStorage::JournalBlock(OpFileLength pos)
{
	int block_no;
	unsigned char in_journal;
	char *blockbuf = NULL;
	unsigned char *compressed_buf = NULL;
	int c_size;
	OP_STATUS rv;

	OP_ASSERT(AT_BLOCK_BOUNDARY(pos));

	block_no = (int)(pos / m_blocksize);

	if (block_no >= m_transaction_blocks)  // newly created blocks aren't journalled
		return OpStatus::OK;

	RETURN_IF_ERROR(m_transaction->SetFilePos(JOURNAL_HDR_SIZE + (block_no >> 3)));
	RETURN_IF_ERROR(ReadFully(m_transaction, &in_journal, 1));

	if (in_journal & (1 << (block_no & 7)))  // block is already journalled
		return OpStatus::OK;

	RETURN_OOM_IF_NULL(compressed_buf = OP_NEWA(unsigned char, ((3 * m_blocksize) >> 1) + 8));
	if ((blockbuf = OP_NEWA(char, m_blocksize)) == NULL)
	{
		OP_DELETEA(compressed_buf);
		return OpStatus::ERR_NO_MEMORY;
	}

	RETURN_IF_ERROR_CLEANUP(m_transaction->SetFilePos(0, SEEK_FROM_END));
	RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(pos));

	RETURN_IF_ERROR_CLEANUP(WriteOFL(m_transaction, pos));

	if (OpStatus::IsSuccess(rv = ReadFully(m_file, blockbuf, m_blocksize)))
	{
		c_size = m_journal_compressor.Compress(compressed_buf, blockbuf, m_blocksize);
		OP_ASSERT(c_size > 0);
		OP_ASSERT(c_size <= ((3 * m_blocksize) >> 1) + 8);
		RETURN_IF_ERROR_CLEANUP(WriteInt32(m_transaction, c_size));
		RETURN_IF_ERROR_CLEANUP(m_transaction->Write(compressed_buf, c_size));
	}

	OP_DELETEA(blockbuf);
	OP_DELETEA(compressed_buf);

	RETURN_IF_ERROR(m_transaction->SetFilePos(JOURNAL_HDR_SIZE + (block_no >> 3)));
	in_journal |= (1 << (block_no & 7));

	RETURN_IF_ERROR(m_transaction->Write(&in_journal, 1));

	rv = m_transaction->Flush();
	RETURN_IF_ERROR(m_transaction->SetFilePos(0));

	return rv;

cleanup:
	OP_DELETEA(blockbuf);
	OP_DELETEA(compressed_buf);
	return rv;
}

void BlockStorage::SwitchEndianInData(void *data, int size)
{
	unsigned char *ptr = (unsigned char *)data;
	register int processed;

	while (size > 0)
	{
		if ((processed = m_EndianCallback(ptr, size, m_callback_arg)) <= 0)
			return;
		ptr += processed;
		size -= processed;
	}
}

#include "modules/util/opfile/opfile.h"

OP_BOOLEAN BlockStorage::FileExists(const uni_char *name, OpFileFolder folder)
{
	OpFile file;
	BOOL exists;

	RETURN_IF_ERROR(file.Construct(name, folder));
	RETURN_IF_ERROR(file.Exists(exists));

	return exists ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}

OP_BOOLEAN BlockStorage::DeleteFile(const uni_char *name, OpFileFolder folder)
{
	OpFile file;
	BOOL exists;

	RETURN_IF_ERROR(file.Construct(name, folder));
	RETURN_IF_ERROR(file.Exists(exists));
	if (exists)
	{
		OP_STATUS status = file.Delete();
		if (status == OpStatus::ERR_FILE_NOT_FOUND)
			exists = FALSE;
		else if (OpStatus::IsError(status))
			return status;
	}

	return exists ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}

OP_BOOLEAN BlockStorage::DirectoryEmpty(const uni_char *name, OpFileFolder folder)
{
	OpFolderLister *fl;

	RETURN_OOM_IF_NULL(fl = OpFile::GetFolderLister(folder, UNI_L("*"), name));

	while (fl->Next())
	{
		if (fl->IsFolder() &&
			(uni_strcmp(fl->GetFileName(), UNI_L(".")) == 0  || uni_strcmp(fl->GetFileName(), UNI_L("..")) == 0))
			continue;

		OP_DELETE(fl);
		return OpBoolean::IS_FALSE;
	}

	OP_DELETE(fl);
	return OpBoolean::IS_TRUE;
}

OP_STATUS BlockStorage::RenameFile(const uni_char *old_path, const uni_char *new_path, OpFileFolder folder)
{
	OpFile old_file;
	OpFile new_file;

	RETURN_IF_ERROR(old_file.Construct(old_path, folder));
	RETURN_IF_ERROR(new_file.Construct(new_path, folder));

	return new_file.SafeReplace(&old_file);
}

OP_STATUS BlockStorage::RenameStorage(const uni_char *old_path, const uni_char *new_path, OpFileFolder folder)
{
	OpString old_journal_path;
	OpString new_journal_path;

	RETURN_IF_ERROR(old_journal_path.Set(old_path));
	RETURN_IF_ERROR(old_journal_path.Append(INVALID_JOURNAL_SUFFIX));
	RETURN_IF_ERROR(new_journal_path.Set(new_path));
	RETURN_IF_ERROR(new_journal_path.Append(INVALID_JOURNAL_SUFFIX));
	if (FileExists(old_journal_path.CStr(), folder) == OpBoolean::IS_TRUE)
		RETURN_IF_ERROR(RenameFile(old_journal_path.CStr(), new_journal_path.CStr()));

	RETURN_IF_ERROR(old_journal_path.Set(old_path));
	RETURN_IF_ERROR(old_journal_path.Append(TRANSACTION_NAME_SUFFIX));
	RETURN_IF_ERROR(new_journal_path.Set(new_path));
	RETURN_IF_ERROR(new_journal_path.Append(TRANSACTION_NAME_SUFFIX));
	if (FileExists(old_journal_path.CStr(), folder) == OpBoolean::IS_TRUE)
		RETURN_IF_ERROR(RenameFile(old_journal_path.CStr(), new_journal_path.CStr()));
	
	return RenameFile(old_path, new_path, folder);
}

OP_STATUS BlockStorage::CreateAssociatedFile(const uni_char *suffix)
{
	OpLowLevelFile *f = NULL;
	OP_STATUS status;

	RETURN_IF_ERROR(OpenAssociatedFile(&f, OpenReadWrite, suffix));
	if (OpStatus::IsError(status = f->SafeClose()))
		OpStatus::Ignore(f->Delete());
	OP_DELETE(f);
	return status;
}

OP_BOOLEAN BlockStorage::AssociatedFileExists(const uni_char *suffix)
{
	RETURN_IF_ERROR(BuildAssociatedFileName(m_associated_file_name_buf, suffix));
	return FileExists(m_associated_file_name_buf.CStr());
}

OP_BOOLEAN BlockStorage::DeleteAssociatedFile(const uni_char *suffix)
{
	RETURN_IF_ERROR(BuildAssociatedFileName(m_associated_file_name_buf, suffix));
	return DeleteFile(m_associated_file_name_buf.CStr());
}

OP_STATUS BlockStorage::OpenAssociatedFile(OpLowLevelFile **f, OpenMode mode, const uni_char *suffix)
{
	RETURN_IF_ERROR(BuildAssociatedFileName(m_associated_file_name_buf, suffix));
	return OpenOpLowLevelFile(f, m_associated_file_name_buf.CStr(), mode, OPFILE_ABSOLUTE_FOLDER);
}

OP_STATUS BlockStorage::BuildAssociatedFileName(OpString &name, const uni_char *suffix)
{
	if (!m_file)
		return OpStatus::ERR_NULL_POINTER;
	const uni_char *full_path = m_file->GetFullPath();
	name.Reserve((int)uni_strlen(full_path) + (int)uni_strlen(suffix));
	RETURN_IF_ERROR(name.Set(full_path));
	return name.Append(suffix);
}

BOOL BlockStorage::IsStartBlock(OpFileLength pos)
{
	BOOL start_block;
	OpFileLength dummy_next;

	if (m_file == NULL || !m_start_blocks_supported || !IS_NORMAL_BLOCK(pos))
		return FALSE;

	RETURN_VALUE_IF_ERROR(m_file->SetFilePos(pos), FALSE);
	RETURN_VALUE_IF_ERROR(ReadBlockHeader(m_file, &dummy_next, &start_block), FALSE);

	return start_block;
}

OP_STATUS BlockStorage::SetStartBlock(OpFileLength pos)
{
	OpFileLength next, current;

	if (m_file == NULL)
		return OpStatus::ERR;

	OP_ASSERT(!m_start_blocks_supported);
	OP_ASSERT(IS_NORMAL_BLOCK(pos));

	RETURN_IF_ERROR(m_file->SetFilePos(pos));
	RETURN_IF_ERROR(ReadBlockHeader(m_file, &next, NULL));
	//m_file->SetFilePos(pos);
	//RETURN_IF_ERROR(WriteBlockHeader(m_file, next, TRUE));    (Unnecessary, start blocks are 0)
	while (next != 0)
	{
		RETURN_IF_ERROR(m_file->SetFilePos(next));
		current = next;
		RETURN_IF_ERROR(ReadBlockHeader(m_file, &next, NULL));

		m_start_blocks_supported = TRUE; // Temporary, for WriteBlockHeader

		RETURN_IF_ERROR(m_file->SetFilePos(current));
		RETURN_IF_ERROR(WriteBlockHeader(m_file, next, FALSE));

		m_start_blocks_supported = FALSE;
	}

	return OpStatus::OK;
}

OP_STATUS BlockStorage::StartBlocksUpdated()
{
	OP_STATUS rv = OpStatus::OK;

	if (m_file == NULL)
		return OpStatus::ERR;

	OP_ASSERT(!m_start_blocks_supported);


	// NB! As discussed in Delete(), the following block will be obsolete when using
	// a cached bitfield for checking if a block is deleted in IsStartBlock(). (roarl)
	unsigned char *bitfield;
	RETURN_OOM_IF_NULL(bitfield = OP_NEWA(unsigned char, m_blocksize));
	OpFileLength bitfield_pos = 0, file_length, pos;
	RETURN_IF_ERROR_CLEANUP(m_file->GetFileLength(&file_length));
	for (pos = 0; pos < file_length; pos += m_blocksize) {
		if (IS_BITFIELD_BLOCK(pos))
		{
			bitfield_pos = pos;
			RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(pos));
			RETURN_IF_ERROR_CLEANUP(ReadFully(m_file, bitfield, m_blocksize));
		}
		else if (IS_NORMAL_BLOCK(pos))
		{
			int block_no = (int)((pos - bitfield_pos) / m_blocksize - 1);
			if ((bitfield[block_no >> 3] & (1 << (block_no & 7))) != 0)
			{
				m_start_blocks_supported = TRUE; // Temporary, for WriteBlockHeader
				RETURN_IF_ERROR_CLEANUP(m_file->SetFilePos(pos));
				RETURN_IF_ERROR_CLEANUP(WriteBlockHeader(m_file, 0, FALSE));
				m_start_blocks_supported = FALSE;
			}
		}
	}
	OP_DELETEA(bitfield);


	RETURN_IF_ERROR(m_file->SetFilePos(BLOCK_HDR_SIZE));
	RETURN_IF_ERROR(WriteInt32(m_file, BLOCK_FILE_HEADER)); // Becomes correct according to endianness

	m_start_blocks_supported = true;

	return OpStatus::OK;
	
cleanup:
	OP_DELETEA(bitfield);
	return rv;
}

#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
size_t BlockStorage::EstimateMemoryUsed() const
{
	size_t sum = 0;

	if (m_file)
		sum += sizeof(OpLowLevelFile) + 2*sizeof(size_t);
	if (m_transaction)
		sum += sizeof(OpLowLevelFile) + 2*sizeof(size_t);

	return sum +
		sizeof(m_file) +
		sizeof(m_transaction) +
		sizeof(m_blocksize) +
		sizeof(m_EndianCallback) +
		sizeof(m_callback_arg) +
		sizeof(m_start_blocks_supported) +
		sizeof(m_freeblock) +
		sizeof(m_journal_compressor) +
		sizeof(m_transaction_blocks) +
		sizeof(m_reserved_area) +
		sizeof(m_reserved_size) +
		sizeof(m_reserved_deleted) +
		sizeof(m_next_in_group);
}
#endif

void SearchGroupable::GroupWith(SearchGroupable &group_member)
{
	GetGroupMember().GroupWith(group_member.GetGroupMember());
}

BOOL SearchGroupable::IsFullyCommitted()
{
	return GetGroupMember().IsFullyCommitted();
}

void BlockStorage::GroupWith(BlockStorage &group_member)
{
	// Ensure that we are not already grouped
	BlockStorage *tmp = this;
	do
	{
		if (tmp == &group_member)
			return;
		tmp = tmp->m_next_in_group;
	}
	while (tmp != this);

	// Widen the loop
	tmp = m_next_in_group;
	m_next_in_group = group_member.m_next_in_group;
	group_member.m_next_in_group = tmp;

#ifdef _DEBUG
	// Ensure that none of the group members have been opened yet
	tmp = this;
	do
	{
		OP_ASSERT(!tmp->IsOpen());
		tmp = tmp->m_next_in_group;
	}
	while (tmp != this);
#endif
}

void BlockStorage::UnGroup()
{
	BlockStorage *prev_in_group = this;
	while (prev_in_group->m_next_in_group != this)
		prev_in_group = prev_in_group->m_next_in_group;

	// Remove this from loop
	prev_in_group->m_next_in_group = m_next_in_group;
	m_next_in_group = this;
}

BOOL BlockStorage::IsFullyCommitted()
{
	BlockStorage *tmp = this;
	do
	{
		if (tmp->InTransaction() || tmp->WaitingForGroupCommit())
			return FALSE;
		tmp = tmp->m_next_in_group;
	}
	while (tmp != this);

	return TRUE;
}

BlockStorage *BlockStorage::GetFirstInGroup()
{
	BlockStorage *group_member = this;
	do
	{
		if (group_member->m_first_in_group)
			return group_member;
		group_member = group_member->m_next_in_group;
	}
	while (group_member != this);

	// Nobody called GetFirstInGroup yet, this must be the first
	m_first_in_group = TRUE;
	return this;
}

#undef OLD_JOURNAL_FILE_HEADER
#undef OLD_JOURNAL_FILE_HEADER_I
#undef BLOCK_FILE_HEADER_0201
#undef BLOCK_FILE_HEADER_0201_I
#undef BLOCK_FILE_HEADER_0202
#undef BLOCK_FILE_HEADER_0202_I
#undef BLOCK_FILE_HEADER
#undef BLOCK_FILE_HEADER_I
#undef FILE_HDR_SIZE
#undef BLOCK_HDR_SIZE
#undef JOURNAL_HDR_SIZE
#undef TRANSACTION_NAME_SUFFIX
#undef INVALID_JOURNAL_SUFFIX
#undef RETURN_IF_ERROR_CLEANUP
#undef DISTANCE_BETWEEN_BITFIELDS
#undef DISTANCE_BEHIND_BITFIELD
#undef BITFIELD_POS
#undef IS_BITFIELD_BLOCK
#undef AT_BLOCK_BOUNDARY
#undef IS_NORMAL_BLOCK
#undef IS_START_BLOCK
#undef IS_NOT_START_BLOCK
#undef SWAP_UINT32
#undef ANY_INT64
#undef BS_RESERVE_BLOCKS

#endif  // SEARCH_ENGINE
