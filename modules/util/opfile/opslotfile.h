/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright 2005-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_UTIL_OPFILE_OPSLOTFILE_H
#define MODULES_UTIL_OPFILE_OPSLOTFILE_H

#ifdef UTIL_HAVE_OP_SLOTFILE

#include "modules/util/opfile/opfile.h"
#include "modules/util/adt/opvector.h"

#define SLOTFILE_USES_CRC

/** Partitioned file access
 *
 * An instance of this class writes data into slots in a file, accessing only
 * those slots and thus reducing disk access.
 *
 * Manages two files: one info file containing header information
 * about slots, and one data file containing actual slots.
 *
 * Before slot file is used first time, you need to Init() it, which
 * clears the data and creates the info and data files.
 *
 * Slots are identified by an id string. They are kept in order,
 * specified by an index (0 <= idx < max_nr_slots).
 *
 * You add data into new slots by specifying a new id-string, and you
 * update data in existing slots by using the same id-string. Data
 * removal is also performed using id.
 *
 * You can extract data in order by iterating over indexes from 0 to
 * (GetSlotsCount() - GetNumFreeSlots() - 1), retrieving the id for
 * the given index and then using ReadSlotData for that id.
 */
class OpSlotFile
{
public:
	OpSlotFile();
	virtual ~OpSlotFile();

	/**
	 * Always call Construct first, like on OpFile
	 *
	 * @param data_path path to data file
	 * @param info_path path to index file
	 * @param max_data_size maximum size of data file, can never be larger than
	 *  this. If -1, then no checking
	 * @param folder a named folder or a special folder type
	 *  (OPFILE_ABSOLUTE_FOLDER or OPFILE_SERIALIZED_FOLDER).
	 * @return See OpStatus.
	 */
	OP_STATUS Construct(const uni_char* data_path, const uni_char* info_path, INT32 max_data_size, OpFileFolder folder=OPFILE_ABSOLUTE_FOLDER);

	/**
	 * Initialize the slot file and creates the file on disk, filling it with
	 * zeros to ensure that there always is enough space for the file.
	 *
	 * Will overwrite existing files.
	 *
	 * @param slotsize The size of each slot
	 * @param nrslots Number of slots in slotfile
	 * @param idsize Size of each id-string.  If -1, then any size is allowed.
	 * @return See OpStatus.
	 */
	OP_STATUS Init(UINT32 slotsize, UINT32 nrslots, int idsize = -1);

	/** Open the file.
	 *
	 * Need to do this before any other methods that read/write data (except for init).
	 *
	 * @param read_only if set, file will be opened in read-only mode.
	 * @return See OpStatus.
	 */
	OP_STATUS Open(BOOL read_only = FALSE);

	/** Close file and write changes to index.
	 *
	 * @note Do not set write_header to FALSE unless you are absolutely sure
	 *  that no changes were made to slotfile!
	 *
	 * @param write_header Defaults to TRUE, to write the header, unless the
	 *  file was only open read-only; if set to FALSE, then the header is not
	 *  written.
	 * @return See OpStatus; OK if header was written successfully and file was
	 *  closed without errors
	 */
	OP_STATUS Close(BOOL write_header = TRUE);

	/**
	 * Write changes to header file.
	 */
	OP_STATUS Flush();

	/**
	 * Delete index and data files.
	 *
	 * If you want to use the slotfile again after calling this, you need to call Init().
	 */
	void Delete();

	/**
	 * Checks if a file exists.
	 *
	 * @return TRUE if any of the files exists
	 */
	BOOL Exists();

	/**
	 * Check that file and data in file is valid.
	 *
	 * Will use CRC if SLOTFILE_USES_CRC is defined
	 *
	 * @param nrslots the number of slots that the file is supposed to have
	 * @param slotsize the size that each slot is supposed to have
	 * @param idsize the size that each id-string is expected to have.  If -1,
	 *  then no checking is done.
	 * @return OpStatus::OK if file was found valid
 	 */
	OP_STATUS CheckValidity(UINT32 nrslots, UINT32 slotsize, int idsize = -1);

	/**
	 * Reads data for a slot
	 *
	 * @param id indentifier string of slot
	 * @param buffer pointer to a buffer where to store data
	 * @param buffer_size size of buffer
	 *
	 * @return OpStatus::OK if data was read successfully
	 */
	OP_STATUS ReadSlotData(const char* id, char* buffer, UINT32 buffer_size);

	/**
	 * Writes data into a slot.
	 *
	 * If ID doesn't exist, a new slot will be allocated. If ID
	 * exists, data will be written into existing slot, and the end of
	 * the list.
	 *
	 * @param id identifier string of slot.
	 * @param buffer pointer to buffer where data is stored
	 * @param buffer_size size of data
	 *
	 * @return OpStatus::OK if data was written successfully
	 */
	OP_STATUS WriteSlotData(const char* id, char* buffer, UINT32 buffer_size);

	/**
	 * Removes a slot and marks it as free.
	 *
	 * The slot order list will be "compacted" and otherwise keep its order
	 *
	 * \param id identifier string of slot.
	 * \return OpStatus::OK if slot existed and was removed successfully
	 */
	OP_STATUS RemoveSlot(const char* id);

	/**
	 * Remove data of slots that are corrupt.
	 *
	 * The slot order list will be "compacted" and otherwise keep its order
	 *
	 * @return See OpStatus; OK if there were no corrupt slots, or if all
	 *  corrupt slots were successfully removed.
	 */
	OP_STATUS RemoveCorruptSlots();

	/** Move an entry to a place after another entry in the list
	 *
	 * @param id1 Identifier string for the entry to move.
	 * @param id2 Identifier string for the entry to move id1 after; or NULL to
	 *  move id1 to first place in list.
	 *
	 * @return See OpStatus; OK if entry was successfully moved.
	 */
	OP_STATUS MoveEntryAfter(const char* id1, const char* id2);

	/**
	 * Removes all slots that are not specified in the argument list
	 *
	 * @param id Vector of ids of slots still in use; these shall NOT be removed.
	 * @return OpStatus::OK if slot existed and was removed successfully.
	 */
	OP_STATUS RemoveUnusedSlots(OpVector<OpString8>& id);

	/** The number of free slots in the file. */
	int GetNumFreeSlots();

	/** Size of slots in use by this file. */
	UINT32 GetSlotSize() { return m_slotsize; }

	/** Total number of slots for this file (used + unused). */
	int GetSlotsCount() { return m_max_nr_slots; }

	/**
	 * Get id string for a given index
	 *
	 * @param index
	 * @param buf buffer to copy id into
	 * @param buf_size size of buf
	 *
	 * @return number of bytes copied into buf, 0 if no ID was found/copied
	 */
	int GetSlotId(int index, char * buf, int buf_size);

private:
	/* Specifies the maximum number of slots that can be handled (used as a
	 * sanity check when reading nr of slots from file and allocating buffers
	 * from this) */
	static const unsigned int MAX_NR_SLOTS = 10000;

	struct SlotOrder
	{
		OpString id;
		UINT32 slot_nr;
		UINT32 crc;
		UINT32 crcsize;
	};

	enum SlotUsage
	{
		SLOT_USED = 0,
		SLOT_FREE
	};

	const char* GetHeaderString() { return "OpSlotFile"; }
	static int GetVersion() { return 1; }

	BOOL IsSupportedVersion(int version) { return version == 1; }

	/**
	 * Write header to index file
	 */
	OP_STATUS WriteHeader(UINT32 slotsize, int idsize);

	/**
	 * Write dummy data to data file
	 */
	OP_STATUS WriteDummySlots(OpFile& data_file, UINT32 nr_slots, UINT32 slotsize);

	/**
	 * Read header from index file
	 */
	OP_STATUS ReadHeader();

	/**
	 * Helper function for reading slot data.
	 */
	OP_STATUS ReadSlotData(UINT32 slot_no, char* buffer, UINT32 buffer_size);

	/**
	 * Helper function for writing slot data.
	 */
	OP_STATUS WriteSlotData(UINT32 slot_no, char* buffer, UINT32 buffer_size);

	/**
	 * Removes slot data for a slot by index
	 *
	 * @param index of slot to remove
	 *
	 * @return OpStatus::OK if slot existed and was removed successfully
	 */
	OP_STATUS RemoveSlot(int idx);

	/**
	 * Helper function that move an entry according to indexes.
	 *
	 * @param idx_2 if -1, then move first
	 */
	OP_STATUS MoveEntryAfter(int idx_1, int idx_2);

	/** Returns index for a free slot, -1 if no free slot */
	int GetFreeSlot();

	/**
	 * Find the index of a given id string
	 *
	 * @return index or -1 if not found
	 */
	int FindIndex(const char* id);

	/**
	 * Clears the index table
	 */
	void EraseIndex();

	/**
	 * Deletes old index tables and creates new ones and sets max_nr_slots
	 */
	OP_STATUS RecreateIndex(int max_nr_slots);

	/** Check each slot against its CRC.
	 *
	 * Assumes that slotfile is open and header has been read.
	 *
	 * @param remove_corrupt Remove data of any corrupt slots found
	 */
	OP_STATUS CheckSlotsValidity(BOOL remove_corrupt);

	/**
	 * Calculate CRC for data in buffer
	 */
	UINT32 CalculateCRC(char* buffer, int size);

	OpFile m_data_file;
	OpFile m_info_file;
	BOOL m_readonly;

	UINT32 m_version;
	UINT32 m_slotsize;
	int m_max_nr_slots;
	int m_idsize;

	INT32 m_max_data_size;

	OpFileLength m_slots_start;
	SlotUsage* m_slots_usage;
	SlotOrder** m_slot_order_table;
	BOOL m_header_loaded;
};

#endif // UTIL_HAVE_OP_SLOTFILE
#endif // MODULES_UTIL_OPFILE_OPSLOTFILE_H
