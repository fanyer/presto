/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#ifdef LOC_BINFILELANGMAN

#include "modules/util/opfile/opfile.h"
#include "modules/locale/src/opbinaryfilelanguagemanager.h"

/** Currently supported file format version. */
#define BINARY_FORMAT_VERSION 1
#ifdef LOCALE_BINARY_ENDIAN
/** Flip an opposite-endian 16-bit value. */
# define FLIP16(x) (((static_cast<unsigned short>(x) & 0xFF) << 8) | static_cast<unsigned short>(x) >> 8)
/** Flip an opposite-endian 32-bit value. */
# define FLIP32(x) (((static_cast<unsigned long>(x) & 0xFF) << 24) | ((static_cast<unsigned long>(x) & 0xFF00) << 8) | ((static_cast<unsigned long>(x) & 0xFF0000) >> 8) | static_cast<unsigned long>(x) >> 24)
#endif

OpBinaryFileLanguageManager::OpBinaryFileLanguageManager()
	: m_stringdb(NULL)
	, m_number_of_strings(0)
{
}

OpBinaryFileLanguageManager::~OpBinaryFileLanguageManager()
{
	delete[] m_stringdb;
}

OP_STATUS OpBinaryFileLanguageManager::GetString(Str::LocaleString num, UniString &s)
{
	// Find the proper entry
	stringdb_s target;
	if (0 != (target.id = static_cast<int>(num)))
	{
		stringdb_s *entry =
			reinterpret_cast<stringdb_s *>
			(op_bsearch(&target, m_stringdb, m_number_of_strings,
			            sizeof (stringdb_s), entrycmp));

		if (entry)
		{
			s = entry->string;
			return OpStatus::OK;
		}
	}

	s.Clear();
	return OpStatus::OK;
}

LOCALE_INLINE
UINT32 OpBinaryFileLanguageManager::ReadNative32L(OpFileDescriptor *file)
{
	UINT32 data;
	OpFileLength bytes_read;
	LEAVE_IF_ERROR(file->Read(&data, sizeof (UINT32), &bytes_read));
	if (bytes_read != sizeof (UINT32))
		LEAVE(OpStatus::ERR);
	return data;
}

LOCALE_INLINE
void OpBinaryFileLanguageManager::ReadNativeStringL(OpFileDescriptor *file, uni_char *s, size_t len)
{
	OpFileLength bytes_read;
	LEAVE_IF_ERROR(file->Read(s, len * sizeof (uni_char), &bytes_read));
	if (bytes_read != static_cast<OpFileLength>(len * sizeof (uni_char)))
		LEAVE(OpStatus::ERR);
}

#ifdef LOCALE_BINARY_ENDIAN
UINT32 OpBinaryFileLanguageManager::ReadOpposite32L(OpFileDescriptor *file)
{
	UINT32 oppositedata;
	OpFileLength bytes_read;
	LEAVE_IF_ERROR(file->Read(&oppositedata, sizeof (UINT32), &bytes_read));
	if (bytes_read != sizeof (UINT32))
		LEAVE(OpStatus::ERR);
	return FLIP32(oppositedata);
}

void OpBinaryFileLanguageManager::ReadOppositeStringL(OpFileDescriptor *file, uni_char *s, size_t len)
{
	OpFileLength bytes_read;
	LEAVE_IF_ERROR(file->Read(s, len * sizeof (uni_char), &bytes_read));
	if (bytes_read != static_cast<OpFileLength>(len * sizeof (uni_char)))
		LEAVE(OpStatus::ERR);
	for (uni_char *p = s + len; p >= s; -- p)
	{
		*p = FLIP16(*p);
	}
}
#endif

void OpBinaryFileLanguageManager::LoadTranslationL(OpFileDescriptor *lngfile)
{
	// Format for the binary language files:
	//  FormatVersion: INT32 (also serves as byte order marker)
	//  DBVersion: INT32
	//  LanguageCodeLength: INT32
	//  LanguageCode: UTF16CHAR * LanguageCodeLength
	//  NumberOfEntries: INT32
	//  {
	//     StringId: INT32
	//     StringLength: INT32
	//     String: UTF16CHAR * StringLength
	//  } * NumberOfEntries

#ifdef _DEBUG
	BOOL language_file_exists;
	OP_ASSERT(OpStatus::IsSuccess(lngfile->Exists(language_file_exists)));
	OP_ASSERT(language_file_exists);
#endif

	LEAVE_IF_ERROR(lngfile->Open(OPFILE_READ));

	UINT32 format_version;
	OpFileLength bytes_read;
	LEAVE_IF_ERROR(lngfile->Read(&format_version, sizeof format_version, &bytes_read));
	if (bytes_read != sizeof format_version)
		LEAVE(OpStatus::ERR);
#ifdef LOCALE_BINARY_ENDIAN
	UINT32 (*read32L)(OpFileDescriptor *) = NULL;
	void (*readstringL)(OpFileDescriptor *, uni_char *, size_t) = NULL;
#else
# define read32L(f) ReadNative32L(f)
# define readstringL(f,s,l) ReadNativeStringL(f,s,l)
#endif
	if (format_version == BINARY_FORMAT_VERSION)
	{
#ifdef LOCALE_BINARY_ENDIAN
		// Machine endian
		read32L = ReadNative32L;
		readstringL = ReadNativeStringL;
#endif
	}
#ifdef LOCALE_BINARY_ENDIAN
	else if (format_version == FLIP32(BINARY_FORMAT_VERSION))
	{
		// Opposite endian
		read32L = ReadOpposite32L;
		readstringL = ReadOppositeStringL;
	}
#endif
	else
	{
		LEAVE(OpStatus::ERR_PARSING_FAILED);
	}

	// Get static data
	m_db = read32L(lngfile);
	UINT32 lngcodelen = read32L(lngfile);
	m_language.ReserveL(lngcodelen + 1);
	m_language.DataPtr()[lngcodelen] = 0;
	readstringL(lngfile, m_language.CStr(), lngcodelen);

	// Read everyting; the file is already sorted
	m_number_of_strings = read32L(lngfile);
	m_stringdb = new(ELeave) stringdb_s[m_number_of_strings];
	stringdb_s *current = m_stringdb;

	// Read
#ifdef DEBUG
	int last_id = -1;
#endif
	for (int i = m_number_of_strings; i; -- i)
	{
		// Read the id number
		current->id = static_cast<int>(read32L(lngfile));

#ifdef DEBUG
		OP_ASSERT(current->id > last_id); // Input file must be sorted
		last_id = current->id;
#endif

		// Read the string
		UINT32 len = read32L(lngfile);
		uni_char *val = current->string.GetAppendPtr(len);
		if (!val)
			LEAVE(OpStatus::ERR_NO_MEMORY);
		readstringL(lngfile, val, len);

		// Step to the next entry
		++ current;
	}
	lngfile->Close();
}


int OpBinaryFileLanguageManager::entrycmp(const void *p1, const void *p2)
{
	return reinterpret_cast<const stringdb_s *>(p1)->id -
	       reinterpret_cast<const stringdb_s *>(p2)->id;
}

#endif // LOC_BINFILELANGMAN
