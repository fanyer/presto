/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef ST_BLOCK_STORAGE_H
#define ST_BLOCK_STORAGE_H

#ifdef SELFTEST

typedef class ST_BlockStorage CursorDBBackend;
typedef class BlockStorageWrapper CursorDBBackendCreate;

#include "modules/search_engine/BlockStorage.h"

class ST_BlockStorage
{
public:
	virtual ~ST_BlockStorage() {}
	virtual void Close() = 0;
	virtual OP_STATUS Clear(long blocksize = 0) = 0;
	virtual int GetBlockSize() = 0;
	virtual OpFileLength GetFileSize() = 0;
	virtual BOOL IsStartBlock(OpFileLength pos) = 0;
	virtual BOOL InTransaction() = 0;
	virtual OP_STATUS Commit() = 0;
	virtual OP_STATUS Open(const uni_char* path, BlockStorage::OpenMode mode, long blocksize, int buffered_blocks = 0, OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER) = 0;
	virtual BOOL ReadUserHeader(unsigned offset, void *data, int len, int disk_len = 0, int count = 1) = 0;
	virtual OP_STATUS SetStartBlock(OpFileLength pos) = 0;
	virtual OP_STATUS StartBlocksUpdated() = 0;
	virtual BOOL WriteUserHeader(unsigned offset, const void *data, int len, int disk_len = 0, int count = 1) = 0;
	virtual BlockStorage &GetGroupMember() = 0;

private:
	friend class CursorWrapper;
	virtual BlockStorage* GetBlockStorage() = 0;
};

class BlockStorageWrapper : public ST_BlockStorage
{
public:
	virtual void Close() { m_block_storage.Close(); }
	virtual OP_STATUS Clear(long blocksize) { return m_block_storage.Clear(blocksize); }
	virtual int GetBlockSize() { return m_block_storage.GetBlockSize(); }
	virtual OpFileLength GetFileSize() { return m_block_storage.GetFileSize(); }
	virtual BOOL IsStartBlock(OpFileLength pos) { return m_block_storage.IsStartBlock(pos); }
	virtual BOOL InTransaction() { return m_block_storage.InTransaction(); }
	virtual OP_STATUS Commit() { return m_block_storage.Commit(); }
	virtual OP_STATUS Open(const uni_char* path, BlockStorage::OpenMode mode, long blocksize, int buffered_blocks, OpFileFolder folder) { return m_block_storage.Open(path, mode, blocksize, buffered_blocks, folder); }
	virtual BOOL ReadUserHeader(unsigned offset, void *data, int len, int disk_len, int count) { return m_block_storage.ReadUserHeader(offset, data, len, disk_len, count); }
	virtual OP_STATUS SetStartBlock(OpFileLength pos) { return m_block_storage.SetStartBlock(pos); }
	virtual OP_STATUS StartBlocksUpdated() { return m_block_storage.StartBlocksUpdated(); }
	virtual BOOL WriteUserHeader(unsigned offset, const void *data, int len, int disk_len, int count) { return m_block_storage.WriteUserHeader(offset, data, len, disk_len, count); }
	virtual BlockStorage &GetGroupMember() { return m_block_storage.GetGroupMember(); }

private:
	virtual BlockStorage* GetBlockStorage() { return &m_block_storage; }
	BlockStorage m_block_storage;
};

#else
typedef class BlockStorage CursorDBBackend;
typedef class BlockStorage CursorDBBackendCreate;
#endif // SELFTEST

#endif // ST_BLOCK_STORAGE_H
