/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef EXPONENTIALGROWTHFILE_H
#define EXPONENTIALGROWTHFILE_H

#include "modules/search_engine/FileWrapper.h"

#ifdef SEARCH_ENGINE_EXPONENTIAL_GROWTH_FILE

/** Wrapper class for OpLowLevelFile's that ensure that the file grows in
 * exponentially increasing increments.
 * 
 * This class can be wrapped around any OpLowLevelFile to ensure exponential
 * growth, limiting file fragmentation to O(log(n)).
 */
class ExponentialGrowthFile
	: public FileWrapper
{
public:
	/** Create an ExponentialGrowthFile object.
	 *
	 * This does not create or open the file (but the supplied wrapped_file
	 * may be already created or open).
	 *
	 * @param wrapped_file The normal OpLowLevelFile that will handle actual I/O.
	 * @param status set to OK, or error code if the return value is NULL.
	 * @param smallest_size The smallest size of the file. It should be a power of 2
	 * @param transfer_ownership If TRUE, on success of this call the ownership of wrapped_file
	 *     is transferred to the new ExponentialGrowthFile, which becomes responsible for
	 *     deallocating it in the destructor. In this case the wrapped_file object must have
	 *     been allocated using OP_NEW.
	 * @return The new ExponentialGrowthFile, or NULL on error
	 */
	static ExponentialGrowthFile* Create(OpLowLevelFile* wrapped_file,
										 OP_STATUS& status,
										 OpFileLength smallest_size = EXPONENTIAL_GROWTH_FILE_SMALLEST_SIZE,
										 BOOL transfer_ownership = TRUE);

	/** Extract the wrapped file and delete the ExponentialGrowthFile object.
	 * If the file was converted to an ExponentialGrowthFile, it will be converted back, but
	 * this should not be the case, so the code asserts that it is not converted.
	 * @param file The ExponentialGrowthFile to be "unwrapped". The pointer will be set to NULL.
	 * @return the wrapped file
	 */
	static OpLowLevelFile* MakeNormalFile(ExponentialGrowthFile*& file);

	OP_STATUS	SafeClose();
	OP_STATUS	Close();

	/** OPPS! If mode includes OPFILE_COMMIT, this operation *will* modify the file position,
	 *  contrary to what is common in OpLowLevelFile implementations
	 */
	OP_STATUS	Flush();

	/** OPPS! Currently not posix compatible. Returns Eof()==TRUE when positioned at end of file,
	 *  not after trying to read past the end. */
	BOOL		Eof() const;

	OP_STATUS	Write(const void* data, OpFileLength len);
	OP_STATUS	Read(void* data, OpFileLength len, OpFileLength* bytes_read);
	OP_STATUS	GetFileLength(OpFileLength* len) const;

	/** OPPS! Currently not posix compatible. Does not allow setting a position beyond the end of the file */
	OP_STATUS	SetFilePos(OpFileLength pos, OpSeekMode mode = SEEK_FROM_START);
	OP_STATUS	SetFileLength(OpFileLength len) { return SetFileLength(len, FALSE); }

	static OpFileLength nextExpSize(OpFileLength size);

private:
	ExponentialGrowthFile();
	ExponentialGrowthFile(OpLowLevelFile*, OpFileLength, BOOL);
	~ExponentialGrowthFile();

	const OpFileLength		m_smallest_size;
	mutable OpFileLength	m_virtual_file_length;
	mutable BOOL			m_metadata_needs_write;

	OpFileLength GetNewPhysicalFileLength(OpFileLength virtual_file_length) const;
	CHECK_RESULT(OP_STATUS SetFileLength(OpFileLength len, BOOL return_to_current_file_pos));
	CHECK_RESULT(OP_STATUS EnsureValidVirtualFileLength() const);
	CHECK_RESULT(OP_STATUS WriteMetadata());
	CHECK_RESULT(OP_STATUS ConvertBackToNormalFile());
};
#endif // SEARCH_ENGINE_EXPONENTIAL_GROWTH_FILE

#endif // !EXPONENTIALGROWTH_H
