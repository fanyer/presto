/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef BLOCKSTORAGE_H
#define BLOCKSTORAGE_H

#include "modules/pi/system/OpLowLevelFile.h"
#include "modules/search_engine/BinCompressor.h"

class BlockStorage;


/**
 * Parent class for BlockStorage based search indexes that can be grouped
 * together with others for coordinated Commit.
 *
 * <p>Note! When several search indexes are grouped, it is the responsibility
 * of the code performing the grouping to make sure that Commit is called
 * on all search indexes in the group in one operation. Committing only some
 * of the indexes will not result in a full commit. Only a successful result
 * from all commits including the last ensures that all indexes are committed.
 * This can be checked with IsFullyCommitted().
 */
class SearchGroupable
{
public:
	/**
	 * Merge this group with (the group of) another SearchGroupable. Grouping
	 * should be done before any of the members of the group are opened or used
	 */
	void GroupWith(SearchGroupable &group_member);

	/**
	 * Get the BlockStorage (of a member of the group) that this
	 * SearchGroupable represents. If this SearchGroupable represents several
	 * BlockStorages or SearchGroupables, they must themselves already be
	 * grouped.
	 */
	virtual BlockStorage &GetGroupMember() = 0;

	/**
	 * Check if all members of a group have been committed. Note that this
	 * function can only be used for verifying a full commit at the highest
	 * level of grouping. It is not necessary to check the other members.
	 * @return TRUE if all members of the group have been fully committed
	 */
	virtual BOOL IsFullyCommitted();
};


/**
 * @brief File storage designed to avoid safe file replace.
 * @author Pavel Studeny <pavels@opera.com>
 *
 * When you save your data into the file, you receive a file position (can be viewed as ID)
 * for further manipulation with the data.
 *
 * File structure:
 * @li 8 B position of the first free block / 0 if no free block is available
 * @li 4 B header
 * @li 4 B block size
 *
 * n * m_blocksize linked blocks
 *
 * Block sturcture:
 * @li 8 B position of the next block (msb == 0 for start blocks, 1 for consecutive blocks)
 * @li (first block in a chain only) 4 B size of the data
 *
 *
 *
 * @par BlockStorage offers 4 levels of safety for write operations:
 *
 * @par 1. Plain write, no transactions
 * Fast and reasonably safe for applications not requring consistency among several data records.
 * @par
 * Methods to use:
 * @li Write/Update
 *
 * @par 2. Write in a transaction
 * Safe except loss of electric power or operating system crash, allows commit or rollback from any point of the transaction.
 * @par
 * Methods to use:
 * @li BeginTransaction
 * @li Write/Update
 * @li Commit/Rollback
 *
 * @par 3. Transaction with prejournalling
 * All the free blocks and blocks going to be modified are journallled before the modification, journal is SafeClosed
 * before any modification of the file. Safety for loss of electric power or operating system crash depends on the
 * filesystem used.
 * @par
 * Methods to use:
 * @li BeginTransaction
 * @li PreJournal
 * @li FlushJournal
 * @li Write/Update
 * @li Commit/Rollback
 *
 * @par 4. Prejournalling to invalid journal
 * Journal is marked as invalid until FlushJournal is called. Safe on any filesystem.
 * @par
 * Methods to use:
 * @li BeginTransaction(TRUE)
 * @li PreJournal
 * @li FlushJournal
 * @li Write/Update
 * @li Commit/Rollback
 *
 * @par Preformance tweaks
 * Safety level 2 calls SafeClose on Commit, safety level 3 and 4 call SafeClose on Commit and FlushJournal.
 * SafeClose makes sure that all the data written to the file are physically on the disk, but it's extremely
 * slow. However, if there was a delay between writing the data and SafeClose, there is a high probability,
 * that the data were already physically written to the disk and SafeClose is then much faster. To avoid
 * freezing in SafeClose in a single threaded program, try something like this (example for safety level 4):
 * @code
 * BeginTransaction(TRUE);
 * PreJournal(blockN); PreJournal(blockM); PreJournal(blockU);
 * // do something else
 * FlushJournal();
 * Write(blockN); Write(blockM); Update(blockU);
 * // do something else
 * Commit();
 * @endcode
 *
 */
class BlockStorage : public NonCopyable, public SearchGroupable
{
public:
	enum OpenMode
	{
		OpenRead = 1,
		OpenReadWrite = 2
	};

	/**
	 * @param data pointer to the data to switch
	 * @param size total size of the current data block
	 * @param user_arg value supplied by user on setup
	 * @return length of processed data; if less than size, the callback will be called again with the remaining data; abort operation if <= 0
	 */
	typedef int (* SwitchEndianCallback)(void *data, int size, void *user_arg);

	BlockStorage(void)
		: ReadInt32(NULL)
		, WriteInt32(NULL)
		, ReadInt64(NULL)
		, WriteInt64(NULL)
		, m_file(NULL)
		, m_blocksize(0)
		, m_buffered_blocks(0)
		, m_EndianCallback(NULL)
		, m_callback_arg(NULL)
		, m_start_blocks_supported(FALSE)
		, m_freeblock(INVALID_FILE_LENGTH)
		, m_transaction(NULL)
		, m_transaction_blocks(0)
		, m_reserved_area(0)
		, m_reserved_size(0)
		, m_reserved_deleted(INVALID_FILE_LENGTH)
		, m_next_in_group(this)
		, m_first_in_group(FALSE)
		, m_group_commit_incomplete(FALSE)
	{}

	~BlockStorage(void);

	/**
	 * open the file for read or read/write access
	 * @param path file location
	 * @param mode open mode, OPFILE_XXX constants
	 * @param blocksize size of one data block including block header
	 * @param buffered_blocks buffer multiple blocks when reading
	 * @param folder might be one of predefind folders
	 */

	CHECK_RESULT(OP_STATUS Open(const uni_char* path, OpenMode mode, int blocksize,
		int buffered_blocks = 0, OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER));

	/**
	 * close the file and rollback any pending transaction
	 */
	void Close(void);

#ifdef SELFTEST
	/**
	 * crash simulation for selftests
	 */
	void Crash(void);
#endif

	/**
	 * @return TRUE if the file is opened
	 */
	BOOL IsOpen(void) const {return m_file != NULL;}

	/**
	 * erase all data
	 */
	CHECK_RESULT(OP_STATUS Clear(int blocksize = 0));

	/**
	 * @return size of one data block
	 */
	int GetBlockSize(void) const
	{
		return m_blocksize;
	}

	OpFileLength GetFileSize(void)
	{
		OpFileLength size;

		if (m_file == NULL)
			return 0;
		RETURN_VALUE_IF_ERROR(m_file->GetFileLength(&size), 0);
		return size;
	}

	/**
	 * @return TRUE if endians on disk are the same like in memory
	 */
	BOOL IsNativeEndian(void) const;

	/**
	 * set conversion callback called on every read/write/update
	 * @param cb callback converting either the full data block or, in case of arrays, it's items one by one; can be NULL
	 * @param user_arg argument to be passed to callback
	 */
	void SetOnTheFlyCnvFunc(SwitchEndianCallback cb, void *user_arg = NULL);

	/**
	 * just a helper function to change the endain of an integer
	 */
	static void SwitchEndian(void *buf, int size);

	/**
	 * Write, Update and Delete called within a transaction are crash-safe
	 * @param invalidate_journal set journal as invalid until FlushJournal is called,
	 *        no changes are allowed until FlushJournal is called, use just PreJournal and Reserve
	 * @return OpStatus::ERR if there is already another transaction running
	 */
	CHECK_RESULT(OP_STATUS BeginTransaction(BOOL invalidate_journal = FALSE));

	/**
	 * confirm all changes since BeginTransaction
	 */
	CHECK_RESULT(OP_STATUS Commit(void));

	/**
	 * reject all changes since BeginTransaction
	 */
	CHECK_RESULT(OP_STATUS Rollback(void));

	/**
	 * @return TRUE on ongoing transaction
	 */
	BOOL InTransaction(void) const {return m_transaction && m_transaction->IsOpen();}

	/**
	 * journal blocks in advance, see FlushJournal
	 */
	CHECK_RESULT(OP_STATUS PreJournal(OpFileLength pos));

	/**
	 * make sure that the prejournaled blocks are on the metal - slow, but 100% safe
	 */
	CHECK_RESULT(OP_STATUS FlushJournal(void));

	/**
	 * reserve and journal a block for a future write without modifying the block itself
	 * @return position of the block to be passed to Write or 0 on failure
	 */
	CHECK_RESULT(OpFileLength Reserve());

	/**
	 * write data to previously reserved block, supposed to be called after FlushJournal
	 * @param data block of data to write
	 * @param len should fit into one block, otherwise a new block will be allocated and journalled
	 * @param reserved_pos value returned by Reserve()
	 * @return reserved_pos or 0 on failure, failure is only possible on an allocation of a new block
	 */
	CHECK_RESULT(OpFileLength Write(const void *data, int len, OpFileLength reserved_pos));

	/**
	 * add new data
	 * @return position of the first block of the data in the file / 0 on failure
	 */
	CHECK_RESULT(OpFileLength Write(const void *data, int len));

	/**
	 * @param pos value returned from Write
	 * @return size of the stored data
	 */
	int DataLength(OpFileLength pos);

	/**
	 * read previously written data into memory
	 * @param data buffer to to the data into
	 * @param len size of the buffer
	 * @param pos value returned from Write
	 * @return TRUE on success
	 */
	CHECK_RESULT(BOOL Read(void *data, int len, OpFileLength pos));

	/**
	 * replace existing data
	 * @param data new data
	 * @param len new data size
	 * @param pos value returned from Write
	 * @return TRUE on success
	 */
	CHECK_RESULT(BOOL Update(const void *data, int len, OpFileLength pos));

	/**
	 * replace a piece of existing data, doesn't decrease the size of the data, but it can increase it if offset+size > data length
	 * @param data new data
	 * @param offset position of the modified data in the data block, shouldn't be more than a size of the current data
	 * @param len size of the piece of modified data
	 * @param pos value returned from Write
	 * @return TRUE on success
	 */
	CHECK_RESULT(BOOL Update(const void *data, int offset, int len, OpFileLength pos));

	/**
	 * strip existing data
	 * @param len new data size, the block remains valid even if len was 0
	 * @param pos value returned from Write
	 * @return TRUE on success
	 */
	CHECK_RESULT(BOOL Update(int len, OpFileLength pos));

	/**
	 * remove data from the file
	 * @param pos value returned from Write
	 * @return TRUE on success
	 */
	CHECK_RESULT(BOOL Delete(OpFileLength pos));

	/**
	 * write an additional header data
	 * @param offset offset in bytes from the first user value
	 * @param data user data
	 * @param len size of the data, must fit in the header, the space left is blocksize - 16
	 * @param disk_len if not 0, endian will be adjusted and saved on disk_len bytes
	 * @param count if > 1, data is an array of len * count bytes
	 * @return TRUE on success
	 */
	CHECK_RESULT(BOOL WriteUserHeader(unsigned offset, const void *data, int len, int disk_len = 0, int count = 1));
	CHECK_RESULT(BOOL WriteUserHeader(const void *data, int len, int disk_len = 0, int count = 1))
	{
		return WriteUserHeader(0, data, len, disk_len, count);
	}

	/**
	 * read an additional header data
	 * @param offset offset in bytes from the first user value
	 * @param data user data, data are filled by 0 if WriteUserHeader hasn't been called yet
	 * @param len size of the data
	 * @param disk_len if not 0, endian will be adjusted and read from disk_len bytes
	 * @param count if > 1, data is an array of len * count bytes
	 * @return TRUE on success
	 */
	CHECK_RESULT(BOOL ReadUserHeader(unsigned offset, void *data, int len, int disk_len = 0, int count = 1));
	CHECK_RESULT(BOOL ReadUserHeader(void *data, int len, int disk_len = 0, int count = 1))
	{
		return ReadUserHeader(0, data, len, disk_len, count);
	}

	/**
	 * Special mode optimized for fast appending of data, cannot be mixed with Write/Update/Read.
	 * When using this method IsStartBlock(OpFileLength) will not work correctly.
	 * @param data new or additional data to append
	 * @param len length of data in bytes
	 * @param pos last value returned from Append or 0 if no data had been written yet
	 * @return position of the first block, note that this position can change, 0 on error
	 */
	CHECK_RESULT(OpFileLength Append(const void *data, int len, OpFileLength pos));

	/**
	 * read data written by Append
	 * @param data buffer to to the data into
	 * @param len size of the buffer
	 * @param pos value returned from Write
	 * @return TRUE on success
	 */
	CHECK_RESULT(BOOL ReadApnd(void *data, int len, OpFileLength pos));

	const uni_char *GetFullName(void) const {return m_file->GetFullPath();}

	/** normal read that returns error if data could not be fully read.
	 * The running thread is blocked until all data is read.
	 */
	CHECK_RESULT(static OP_STATUS ReadFully(OpLowLevelFile *f, void* data, OpFileLength len));

	/**  checks whether a file or directory exists */
	CHECK_RESULT(static OP_BOOLEAN FileExists(const uni_char *name, OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER));

	/** deletes a file or directory, return IS_TRUE if file existed; may delete not-empty directories, depending on the actual OpFile implementation */
	CHECK_RESULT(static OP_BOOLEAN DeleteFile(const uni_char *name, OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER));

	/** checks whether a directory is empty */
	CHECK_RESULT(static OP_BOOLEAN DirectoryEmpty(const uni_char *name, OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER));

	/** renames a file */
	CHECK_RESULT(static OP_STATUS RenameFile(const uni_char *old_path, const uni_char *new_path, OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER));

	/** renames storage and journal files */
	CHECK_RESULT(static OP_STATUS RenameStorage(const uni_char *old_path, const uni_char *new_path, OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER));

	/**
	 * Check if this block storage file supports querying of start blocks.
	 * 
	 * @return TRUE if start blocks are supported
	 */
	BOOL IsStartBlocksSupported() const { return m_start_blocks_supported; }

	/**
	 * Check if a block is a valid (non-deleted) start block.
	 * Start blocks must be supported for this method to work, see IsStartBlocksSupported().
	 * An optimized way to read all data in a file more or less sequentially, is something
	 * like this:
	 * 
	 * <pre>
	 * int block_size = bs->GetBlockSize();
	 * OpFileLength file_size = bs->GetFileSize();
	 * for (OpFileLength pos=0; pos < file_size; pos += block_size)
	 * {
	 *     if (bs->IsStartBlock(pos))
	 *     {
	 *         int length = bs->DataLength(pos);
	 *         data = new ...;
	 *         bs->Read(data, length, pos);
	 *     }
	 * }
	 * </pre>
	 * 
	 * @param pos a block file position
	 * @return TRUE if the given block is a valid start block
	 */
	BOOL IsStartBlock(OpFileLength pos);
	
	/**
	 * Mark a specific block as a start block. This is only allowed for files
	 * that do not yet support start blocks, and is intended for upgrading such a file
	 * to support start blocks, see IsStartBlocksSupported(). When all start blocks
	 * have the start block flag set, the file must be upgraded for supporting start blocks
	 * using StartBlocksUpdated().
	 * 
	 * @param pos a block file position
	 * @return TRUE if the given block is a valid, non-deleted start block
	 */
	CHECK_RESULT(OP_STATUS SetStartBlock(OpFileLength pos));
	
	/**
	 * When all start blocks have been marked using SetStartBlock(OpFileLength),
	 * use this method to upgrade the file. This is only allowed for files
	 * that do not yet support start blocks, see IsStartBlocksSupported(). After
	 * calling this method, IsStartBlocksSupported() will return TRUE.
	 * 
	 * <p>SetStartBlock(OpFileLength) and StartBlocksUpdated() can take the place
	 * of Write/Update operations in transaction/journalling operations. When
	 * prejournaling, all blocks <i>except</i> start blocks should be prejournaled. 
	 * 
	 * @return TRUE if the given block is a valid, non-deleted start block
	 */
	CHECK_RESULT(OP_STATUS StartBlocksUpdated());

	/**
	 * @return an estimate of the memory used (in RAM) by this data structure
	 */
#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
	size_t EstimateMemoryUsed() const;
#endif

	/**
	 * Group this BlockStorage with another
	 */
	void GroupWith(BlockStorage &group_member);

	/**
	 * Remove this BlockStorage from the group
	 */
	void UnGroup();

	/**
	 * Get SearchGroupable group member
	 */
	virtual BlockStorage &GetGroupMember() { return *this; }

	/**
	 * @return TRUE if all BlockStorages in the group has been committed
	 */
	virtual BOOL IsFullyCommitted();


#ifndef SELFTEST
protected:
#endif
	/**
	 * Get the first member of the group
	 */
	BlockStorage *GetFirstInGroup();


	/** Create (an empty, closed) file with a name starting with the same name as this BlockStorage */
	CHECK_RESULT(OP_STATUS CreateAssociatedFile(const uni_char *suffix));
	/** Check whether an associated file exists */
	CHECK_RESULT(OP_BOOLEAN AssociatedFileExists(const uni_char *suffix));
	/** Delete an associated file, return IS_TRUE if the file existed */
	CHECK_RESULT(OP_BOOLEAN DeleteAssociatedFile(const uni_char *suffix));
	/** Open an associated file */
	CHECK_RESULT(OP_STATUS OpenAssociatedFile(OpLowLevelFile **f, OpenMode mode, const uni_char *suffix));
	/** Build name of associated file */
	CHECK_RESULT(OP_STATUS BuildAssociatedFileName(OpString &name, const uni_char *suffix));

protected:
	// endian-insensitive integers
	CHECK_RESULT(OP_STATUS (* ReadInt32)(OpLowLevelFile *, INT32 *));
	CHECK_RESULT(OP_STATUS (* WriteInt32)(OpLowLevelFile *, INT32));
	// It's safe to change OpFileLength to 64 bits
	CHECK_RESULT(OP_STATUS (* ReadInt64)(OpLowLevelFile *, OpFileLength *, UINT32 *msb32));
	CHECK_RESULT(OP_STATUS (* WriteInt64)(OpLowLevelFile *, OpFileLength, UINT32 msb32));
	CHECK_RESULT(OP_STATUS ReadOFL(OpLowLevelFile *f, OpFileLength *ofl));
	CHECK_RESULT(OP_STATUS WriteOFL(OpLowLevelFile *f, OpFileLength ofl));
	CHECK_RESULT(OP_STATUS ReadBlockHeader(OpLowLevelFile *f, OpFileLength *next_pos, BOOL *start_block = NULL));
	CHECK_RESULT(OP_STATUS WriteBlockHeader(OpLowLevelFile *f, OpFileLength next_pos, BOOL start_block));

	/**
	 * Sanity check for file positions read from file (in case of corrupted files).
	 * @param pos The file position to be checked
	 * @return ERR if pos is negative or above the sanity check limit, otherwise OK
	 */
	CHECK_RESULT(OP_STATUS CheckFilePosition(OpFileLength pos));

	// current_block == 0 -> new chain
	CHECK_RESULT(OpFileLength CreateBlock(OpFileLength current_block, BOOL current_start_block, BOOL append_mode = FALSE));

	// to be called if m_freeblock > 0
	CHECK_RESULT(OpFileLength GetFreeBlock(BOOL update_file));

	CHECK_RESULT(OP_STATUS Truncate(void));

	CHECK_RESULT(OP_STATUS JournalBlock(OpFileLength pos));

	void SwitchEndianInData(void *data, int size);

	BOOL WaitingForGroupCommit() const { return m_transaction && !m_transaction->IsOpen(); }

	OpLowLevelFile *m_file;
	OpString m_associated_file_name_buf;
	int m_blocksize;
	int m_buffered_blocks;

	SwitchEndianCallback m_EndianCallback;
	void *m_callback_arg;
	
	BOOL m_start_blocks_supported;

private:
	// m_freeblock == 0 -> no free block available
	// m_freeblock == -1 -> free block status is unknown and should be read from the file
	OpFileLength m_freeblock;
	OpLowLevelFile *m_transaction;
	BinCompressor m_journal_compressor;
	// blocks present in the file before a transaction
	int m_transaction_blocks;
	OpFileLength m_reserved_area;
	OpFileLength m_reserved_size;
	OpFileLength m_reserved_deleted;  // previously deleted block reserved for writing

	BlockStorage *m_next_in_group;    // BlockStorage groups are linked in a loop
	BOOL m_first_in_group;            // TRUE if this BlockStorage was opened first
	BOOL m_group_commit_incomplete;   // TRUE if this BlockStorage detected an incomplete group commit

#ifdef _DEBUG
	BOOL m_journal_invalid;
#endif
};

#endif  // BLOCKSTORAGE_H

