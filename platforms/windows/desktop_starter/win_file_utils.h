// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2011 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//


namespace WinFileUtils
{

 	/**
	 * Reads the complete contents of a file into a buffer
	 * This read is designed to read an ascii text file into memory
	 * The buffer MUST released by the caller with delete []
	 *
	 * @param file      Full path of file to read in UTF16
	 * @param buffer    Pointer to the buffer to hold the file contents 
	 * @param buf_size  Pointer to size of returned buffer 
	 *
	 * @return OP_STATUS. 
	 */
		DWORD  Read(uni_char *file, char** buffer, size_t& buf_size);

	/************************************************************************
	* Trick Windows into pre-loading all pages in a dll into the address space
	* of Opera.exe. This will reduce the time it takes to load the dll and map
	* it into the address space by reducing the hard page faults that would
	* normally trigger this behavior.
	*
	* See http://infrequently.org/2011/01/on-the-care-and-feeding-of-spinning-disks/
	* for additional information
	* 
	* @param dll_file	Full path of dll to load
	* @param max_read	Maximum length to preload, or 0 to read it all
	* @return FALSE if the mapping failed, otherwise TRUE. 
	*
	************************************************************************/
	BOOL PreLoadDll(const uni_char* dll_file, DWORD max_read);
}

