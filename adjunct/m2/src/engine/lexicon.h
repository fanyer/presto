/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef ENGINE_LEXICON_H
#define ENGINE_LEXICON_H

#include "adjunct/m2/src/include/defs.h"
#include "adjunct/desktop_util/adt/opsortedvector.h"
#include "adjunct/m2/src/util/blockfile.h"

#include "modules/search_engine/StringTable.h"

#define LEXICON_MEMORY_BLOCK_SIZE	1024 * 100
#define MAX_WORD_LENGTH	256

/***********************************************************************************
**
**	Lexicon
**
**	Class that implements the mapping of a word -> list of ids.
**
***********************************************************************************/

class Lexicon
{
	public:
		/**
		 * @param path directory with lexicon files, traling path separator is optional
		 * @param name prefix for the lexicon file names
		 * @param read_file will read the lexicon file into memory
		 * @param existed will contain FALSE if the file(s) was newly created
		 */
		OP_STATUS			Init(const uni_char* path, const uni_char* name, BOOL read_file = TRUE, BOOL* existed = NULL);

		/**
		 * converts all data to StringTable, closes itself and deletes all files,
		 * closes string_table and deletes its files on error
		 */
		OP_STATUS ConvertTo(StringTable &string_table);

		/**
		 * delete lexicon and all files
		 */
		static OP_STATUS Erase(Lexicon *lexicon);

		/**
		 * check if a lexicon exists
		 */
		static BOOL Exists(const uni_char* path, const uni_char* name);

		/**
		 * get number of words
		 */
		INT32				GetWordCount() {return m_entries_list.GetCount();}

		/**
		 * get indexth word
		 */
		OP_STATUS			GetWord(INT32 index, OpString& word);

		/**
		 * find files containing the given word
		 * @param words space-separated list of words
		 * @param id_list matched message ids
		 * @param partial_match_allowed search for prefix
		 */
		OP_STATUS			FindWord(const uni_char* word, OpINTSortedVector& id_list, BOOL partial_match_allowed = FALSE);

		/**
		 * find a word
		 * @param start_of_word word or prefix to search
		 * @param index index of the first word found
		 * @param count number of matching words
		 * @param partial_match_allowed search for prefix
		 */
		OP_STATUS			GetMatch(const uni_char* start_of_word, INT32& index, INT32& count, BOOL partial_match_allowed = TRUE);

		class LexiconEntry
		{
			public:

				OP_STATUS			Init(INT32 entry_index, const uni_char* word, INT32 max_len = MAX_WORD_LENGTH, BOOL to_lower = TRUE, INT32 bytes_per_character = 2);

				INT32				GetEntryIndex() {return m_entry_index;}
				void				SetEntryIndex(INT32 entry_index) {m_entry_index = entry_index;}

				void				GetWord(OpString& word) const;

				void				SetBlockSizeAndIndex(INT32 block_size, INT32 block_index);
				void				SetBlockSizeAndIndexPacked(INT32 block_size_and_block_index) {m_is_char_and_block_size_and_index = (block_size_and_block_index & 0x7fffffff) | (m_is_char_and_block_size_and_index & 0x80000000);}

				INT32				GetIsCharAndBlockSizeAndIndexPacked() {return m_is_char_and_block_size_and_index;}

				BOOL				IsChar() const {return (m_is_char_and_block_size_and_index & 0x80000000) != 0;}
				const char*			GetChar() const {return &m_char;}
				const uni_char*		GetUniChar() const {return (uni_char*) &m_char;}

				INT32				GetBlockSize() const;
				INT32				GetBlockIndex() const {return m_is_char_and_block_size_and_index & 0x03ffffff;}
				INT32				GetDiskOrder() const {return m_is_char_and_block_size_and_index & 0x7fffffff;}

				INT32				GetMaximumIDCount() {return (GetBlockSize() / 4) - 1;}

				INT32				CompareWord(const LexiconEntry* other_entry) const;
				INT32				CompareWord(const LexiconEntry* other_entry, INT32 number_of_characters_to_compare) const;
				INT32				CompareDiskOrder(const LexiconEntry* other_entry) const {return GetDiskOrder() - other_entry->GetDiskOrder();}

				bool				operator>  (const LexiconEntry& item) const { return CompareWord(&item) > 0; };
				bool				operator<  (const LexiconEntry& item) const { return CompareWord(&item) < 0; };
				bool				operator== (const LexiconEntry& item) const { return CompareWord(&item) == 0; };

			private:

				LexiconEntry()	{}

				INT32				m_entry_index;			// index in the entries file where this entry starts
				UINT32				m_is_char_and_block_size_and_index;	// packed bits (1 bit for is_char, 5 for block_size and 26 for block index)
				char				m_char;					// null terminated word starts here
		};

		class CompareLexiconDiskOrder
		{
			public:
				BOOL operator()(const LexiconEntry* lhs, const LexiconEntry* rhs) const
				{ return lhs->CompareDiskOrder(rhs) < 0; }
		};

		class LexiconBlock
		{
			public:

				LexiconBlock() {m_buffer = OP_NEWA(char, LEXICON_MEMORY_BLOCK_SIZE); m_bytes_used = 0;}
				~LexiconBlock() {OP_DELETEA(m_buffer);}

				OP_STATUS			CreateLexiconEntry(INT32 entry_index, const uni_char* word, LexiconEntry*& entry, BOOL to_lower);
				BOOL				BufferOOM() { return !m_buffer; }

			private:

				char*		m_buffer;
				INT32		m_bytes_used;
		};

		OP_STATUS								ReadLexicon(INT32* dirty_count = NULL);
		OP_STATUS								CreateLexiconEntry(INT32 entry_index, const uni_char* word, LexiconEntry*& entry, BOOL to_lower);
		OP_STATUS								GetIndexFile(INT32 block_size, BlockFile*& block_file);

		OpString								m_path;
		OpString								m_name;
		OpSortedVector<LexiconEntry>			m_entries_list;
		OpSortedVector<LexiconEntry, CompareLexiconDiskOrder> m_entries_list_disk_order;
		BlockFile								m_entries_file;
		OpAutoVector<BlockFile>					m_index_files;
		OpAutoVector<LexiconBlock>				m_lexicon_blocks;
		LexiconEntry*							m_temp_lexicon_entry;
};

#endif // ENGINE_LEXICON_H
