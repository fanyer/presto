/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef BLOCKFILE_H
#define BLOCKFILE_H

#include "adjunct/m2/src/include/defs.h"

class OpFile;

/***********************************************************************************
**
**	BlockFile
**
***********************************************************************************/

class BlockFile
{
	public:

		class BlockFileListener
		{
			public:
            virtual ~BlockFileListener() {}

				// Called when deleting block that is not last one, and the last one will then be moved to fill
				// up the deleted slot

				virtual void	OnBlockMoved(BlockFile* block_file, INT32 old_block_index, INT32 new_block_index) = 0;
		};

					BlockFile();
					~BlockFile();

		OP_STATUS	Init(const uni_char* path, const uni_char* name, OpFileLength header_size, OpFileLength block_size, BOOL append_size_extension = FALSE, BOOL* existed = NULL, BOOL readonly = FALSE);

		void		SetListener(BlockFileListener* listener) {m_listener = listener;}

		OP_STATUS	CreateBackup(BlockFile*& backup_file) { return CreateBackup(backup_file, m_header_size-4, m_block_size); }
		OP_STATUS	CreateBackup(BlockFile*& backup_file, OpFileLength header_size, OpFileLength block_size);
		OP_STATUS	ReplaceWithBackup(BlockFile* backup_file);
		
		OP_STATUS	RemoveFile();

		OP_STATUS	Flush();
		OP_STATUS	SafeFlush();

		OP_STATUS	CreateBlock(INT32& block_index);
		OP_STATUS	DeleteBlock(INT32 block_index);

		OP_STATUS	ReadHeaderValue(OpFileLength header_offset, INT32& value);
		OP_STATUS	WriteHeaderValue(OpFileLength header_offset, INT32 value);

		OP_STATUS	Seek(INT32 block_index, OpFileLength block_offset = 0);

		OP_STATUS	Read(void* buffer, OpFileLength length);
		OP_STATUS	Write(void* buffer, OpFileLength length);

		OP_STATUS	ReadValue(INT32& value);
		OP_STATUS	WriteValue(INT32 value);

		OP_STATUS	ReadValue(INT32 block_index, OpFileLength block_offset, INT32& value);
		OP_STATUS	WriteValue(INT32 block_index, OpFileLength block_offset, INT32 value);

		OP_STATUS	ReadString(OpString& string, BOOL read_as_char = FALSE);
		OP_STATUS	WriteString(const uni_char* string);
		OP_STATUS	WriteString8(const char* string);

		OP_STATUS	ReadString(INT32 block_index, OpFileLength block_offset, OpString& string);
		OP_STATUS	WriteString(INT32 block_index, OpFileLength block_offset, const uni_char* string);
		OP_STATUS	WriteString8(INT32 block_index, OpFileLength block_offset, const char* string);

		INT32		GetBlockCount() {return m_block_count;}
		OP_STATUS	SetBlockCount(INT32 block_count);

		OpFileLength GetBlockSize() {return m_block_size;}
		INT32		GetCurrentBlock();
		OpFileLength GetCurrentOffset();

		static OpFile*	CreateBlockFile(const uni_char* path, const uni_char* name, OpFileLength block_size, BOOL append_size_extension = FALSE);

	private:

		OpFile*		GetFile() {return m_file;}

		INT32		m_header_size;
		OpFileLength m_block_size;
		INT32		m_block_count;
		OpFile*		m_file;
		OpString	m_path;
		OpString	m_name;
		BlockFileListener*	m_listener;

		char		*m_buf;
		OpFileLength m_buf_filepos;
		OpFileLength m_bufsize;
		OpFileLength m_pos;
		BOOL		m_use_buf;

		OP_STATUS	InitBuffer();
		OP_STATUS	ReadBuffer(void *p, OpFileLength len);
		OP_STATUS	ReadData(void *p, OpFileLength len);
		void		MarkBufferInvalid();
		OP_STATUS	SeekBack();
};

#endif // BLOCKFILE_H
