/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file PEMLoader.cpp
 *
 * PEM loader implementation.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#include "core/pch.h"

#if defined(CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT) && defined(DIRECTORY_SEARCH_SUPPORT)

#include "modules/libcrypto/include/PEMLoader.h"


void PEMLoader::ProcessL()
{
	if (m_input_dir.IsEmpty())
		LEAVE(OpStatus::ERR_OUT_OF_RANGE);

	// Create folder lister.
	OpFolderLister* lister = 0;
	OP_STATUS status = OpFolderLister::Create(&lister);
	LEAVE_IF_ERROR(status);
	OP_ASSERT(lister);
	OpAutoPtr <OpFolderLister> lister_autoremover(lister);

	// Search for "*.pem" files in m_input_dir.
	status = lister->Construct(m_input_dir.CStr(), UNI_L("*.pem"));
	LEAVE_IF_ERROR(status);

	while(lister->Next())
	{
		const uni_char* filename = lister->GetFullPath();
		OP_ASSERT(filename);

		OpFile file;
		status = file.Construct(filename);
		if (status != OpStatus::OK)
			continue;

		// Not using OPFILE_TEXT here. It's non-portable: on POSIX systems
		// it doesn't remove CR in the end of line, thus it needs to be done
		// manually anyway.
		status = file.Open(OPFILE_READ);
		if (status != OpStatus::OK)
			continue;

		ProcessPEMFile(file);
	}

}


void PEMLoader::ProcessPEMFile(OpFile& file)
{
	OP_ASSERT(file.IsOpen());

	// BUFSIZE is also the maximum size of PEM chunk with certificate.
	const size_t BUFSIZE = 64 * 1024;
	char* buf = OP_NEWA_L(char, BUFSIZE);
	ANCHOR_ARRAY(char, buf);

	// Length of buffer's useful data.
	size_t buf_len = 0;

	while (!file.Eof())
	{
		// Read.
		{
			char*  buf_free_pos = buf + buf_len;
			size_t buf_free_len = BUFSIZE - buf_len;
			OP_ASSERT(buf_free_pos >= buf);
			OP_ASSERT(buf_free_pos + buf_free_len == buf + BUFSIZE);

			OpFileLength bytes_read = 0;
			OP_STATUS status = file.Read(buf_free_pos, buf_free_len, &bytes_read);
			if (status != OpStatus::OK || bytes_read == 0)
				break;

			OP_ASSERT(bytes_read > 0);
			OP_ASSERT(bytes_read < (OpFileLength)buf_free_len);

			buf_len += (size_t)bytes_read;
			OP_ASSERT(buf_len == BUFSIZE || file.Eof());
		}

		// Parse.
		{
			size_t bytes_parsed = ParsePEMBuffer(buf, buf_len);
			OP_ASSERT(bytes_parsed <= buf_len);

			if (bytes_parsed == buf_len)
			{
				// The whole buffer is parsed.
				// Consider it free.
				buf_len = 0;
			}
			else if (bytes_parsed != 0)
			{
				// The buffer is partially parsed.
				// Let's move the unparsed data to the beginning.
				buf_len -= bytes_parsed;
				op_memmove(buf, buf + bytes_parsed, buf_len);
			}
			else
			{
				// Nothing is parsed. Not enough data for parsing
				// or invalid data. Stop parsing.
				break;
			}
		}
	}
}


size_t PEMLoader::ParsePEMBuffer(const char* buf, size_t buf_len)
{
	OP_ASSERT(buf);

	const char* const begin_marker = GetBeginMarker();
	const char* const   end_marker = GetEndMarker();
	const size_t begin_marker_len = op_strlen(begin_marker);
	const size_t   end_marker_len = op_strlen(  end_marker);

	const char* cur_pos = buf;
	const char* const buf_end = buf + buf_len;

	// Process PEM chunks.
	while (cur_pos < buf_end)
	{
		// Length of current "parsing space".
		size_t cur_len = buf_end - cur_pos;
		if (cur_len < begin_marker_len)
			break;

		// Look for begin marker.
		const char* const begin_pos = (const char*) op_memmem(
			cur_pos, cur_len, begin_marker, begin_marker_len);
		if (!begin_pos)
		{
			// End marker not found. Maybe a PEM chunk crosses
			// the buffer border. Thus only eat the buffer up to
			// its possible location.
			cur_pos = buf_end - begin_marker_len + 1;
			break;
		}

		// Check result and update cur_len/cur_pos.
		OP_ASSERT(begin_pos >= cur_pos);
		cur_pos = begin_pos + begin_marker_len;
		OP_ASSERT(cur_pos <=  buf_end);
		cur_len = buf_end - cur_pos;

		// Look for end marker.
		const char* const end_pos = (const char*) op_memmem(
			cur_pos, cur_len, end_marker, end_marker_len);
		if (!end_pos)
		{
			// End marker not found. Maybe a PEM chunk crosses
			// the buffer border. Thus only eat the buffer up to
			// the beginning of the PEM chunk.
			cur_pos = begin_pos;
			break;
		}

		// Check result and update cur_len/cur_pos.
		OP_ASSERT(end_pos >= cur_pos);
		cur_pos = end_pos + end_marker_len;
		OP_ASSERT(cur_pos <=  buf_end);
		// Not updating cur_len, it's not used till the end of the cycle.

		// Found a full PEM chunk inside the buffer: [begin_pos, cur_pos).
		OP_ASSERT(cur_pos > begin_pos);
		const size_t pem_len = cur_pos - begin_pos;

		// Load a certificate from the PEM chunk.
		ProcessPEMChunk(begin_pos, pem_len);
	}
	// No more full PEM chunks in this buffer.

	// Eat whitespace.
	while (cur_pos < buf_end && op_isspace(*cur_pos))
		cur_pos++;

	OP_ASSERT(cur_pos >= buf);
	OP_ASSERT(cur_pos <= buf_end);
	size_t parsed_bytes = cur_pos - buf;

	return parsed_bytes;
}

#endif // CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT && DIRECTORY_SEARCH_SUPPORT
