/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file PEMLoader.h
 *
 * PEM loader interface.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#ifndef PEM_LOADER_H
#define PEM_LOADER_H


#if defined(CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT) && defined(DIRECTORY_SEARCH_SUPPORT)

#include "modules/util/opstring.h"


/**
 * @class PEMLoader
 *
 * This is a generic abstract class for parsing PEM files and loading data
 * from it. Any PEM data can be loaded: certificates, keys, etc.
 *
 * By overriding GetBeginMarker() and GetEndMarker() methods it is possible
 * to specify which type of data is to be loaded.
 *
 * By overriding ProcessPEMChunk() it is possible to provide a handler for
 * a PEM chunk that was found in the file.
 *
 * When ProcessL() is called, the directory set by SetInputDirectoryL()
 * is searched for *.pem files. The directory search is not recursive.
 * The file mask and search method can be changed in future if needed.
 *
 * Every found PEM file is then scanned for PEM chunks. PEM chunks are
 * identified by begin and end markers, provided by GetBeginMarker() and
 * GetEndMarker(). The found chunks are processed by ProcessPEMChunk().
 *
 */
class PEMLoader
{
public:
	virtual ~PEMLoader();

public:
	/** Set directory, containing *.pem files.
	 *
	 *  The function can leave with the following codes:
	 *  - OpStatus::ERR_NO_MEMORY     on OOM.
	 */
	void SetInputDirectoryL(const OpStringC& dir);

	/** Process PEM loading.
	 *
	 *  The function can leave with the following codes:
	 *  - OpStatus::ERR_OUT_OF_RANGE  on bad input parameters,
	 *                                i.e. empty input directory
	 *                                set via SetInputDirectoryL().
	 *  - OpStatus::ERR_NO_MEMORY     on OOM.
	 *  - OpStatus::ERR               on other conditions.
	 */
	void ProcessL();

protected:
	/** Provide begin marker. */
	virtual const char* GetBeginMarker() const = 0;

	/** Provide end marker. */
	virtual const char* GetEndMarker() const = 0;

	/** Process found PEM chunk, for example decode it and add to some storage. */
	virtual void ProcessPEMChunk(const char* pem_chunk, size_t pem_len) = 0;

private:
	void ProcessPEMFile(OpFile& file);
	size_t ParsePEMBuffer(const char* buf, size_t buf_len);

private:
	OpString m_input_dir;
};


// Inlines implementation.

inline PEMLoader::~PEMLoader() {}

inline void PEMLoader::SetInputDirectoryL(const OpStringC& dir)
{ m_input_dir.SetL(dir); }


#endif // CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT && DIRECTORY_SEARCH_SUPPORT
#endif // PEM_LOADER_H
