/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * wonko
 */
#include "core/pch.h"
#include "modules/display/sfnt_base.h"

#ifdef MDE_MMAP_MANAGER
# include "modules/libgogi/mde_mmap.h"
#else // MDE_MMAP_MANAGER
# include "modules/util/opfile/opfile.h"
#endif // MDE_MMAP_MANAGER

FontHandle::~FontHandle()
{
#ifdef MDE_MMAP_MANAGER
	if (m_mmap)
		g_mde_mmap_manager->UnmapFile(m_mmap);
	else
#endif // MDE_MMAP_MANAGER
		OP_DELETEA(m_data);
}
OP_STATUS FontHandle::LoadFile(const uni_char* filename)
{
#ifdef MDE_MMAP_MANAGER
    OP_STATUS mmap_status = LoadMMAPFile(filename);
    if (mmap_status != OpStatus::ERR_FILE_NOT_FOUND)
        return mmap_status;
    else
#endif // MDE_MMAP_MANAGER
    {
		// allocate buffer and read file to memory
		OpFile file;
		RETURN_IF_ERROR(file.Construct(filename));
		RETURN_IF_ERROR(file.Open(OPFILE_READ));
		OpFileLength len, bytes_read;
		RETURN_IF_ERROR(file.GetFileLength(len));
		size_t size = (size_t)len;
		UINT8* data = OP_NEWA(UINT8, size);
		if (!data)
			return OpStatus::ERR_NO_MEMORY;
		OP_STATUS s = file.Read(data, len, &bytes_read);
		if (OpStatus::IsSuccess(s) && bytes_read != len)
			s = OpStatus::ERR;
		if (OpStatus::IsError(s))
		{
			OP_DELETEA(data);
			return s;
		}
		m_data = data;
		m_size = size;
		return OpStatus::OK;
	}
}

#ifdef MDE_MMAP_MANAGER
OP_STATUS FontHandle::LoadMMAPFile(const uni_char* filename)
{
    // map the file to memory
    if (!g_mde_mmap_manager)
    {
        g_mde_mmap_manager = OP_NEW(MDE_MMapManager, ());
        if (!g_mde_mmap_manager)
            return OpStatus::ERR_NO_MEMORY;
    }
    MDE_MMAP_File* mmap;
    RETURN_IF_ERROR(g_mde_mmap_manager->MapFile(&mmap, filename));
    m_mmap = mmap;
    m_data = (UINT8*)mmap->filemap->GetAddress();
    m_size = mmap->filemap->GetSize();
    return OpStatus::OK;
}
#endif // MDE_MMAP_MANAGER

FontType GetFontType(const UINT32 tag)
{
    switch(tag)
    {
    case 0x00010000:
        return t_ttf;
        break;
    case MAKE_TAG('t','t','c','f'):
        return t_ttc;
        break;
    case MAKE_TAG('O','T','T','O'):
        return t_otf;
        break;
    }
    return t_unknown;
}

// name table-related stuff

// Microsoft (3) Unicode BMP (1)
// or
// Unicode (0) BMP (<= 3)
#define IS_UNI_NAME(p, e) (((p) == 3 && (e) == 1) || ((p) == 0 && (e) <= 3))
// Macintosh (1) Roman (0)
#define IS_ASCII_NAME(p, e) ((p) == 1 && (e) == 0)

/**
   allocates storage for and converts a big-endian unicode BMP name
   entry into a unicode string - returns NULL on OOM
   @param name_be the big-endian unicode BMP name
   @param namelen_b the length of name_be, in _bytes_
 */
static inline
uni_char* GetUniNameBE(const uni_char* name_be, size_t namelen_b)
{
    // allocate storage for name
    OP_ASSERT(!(namelen_b & 0x1));
    size_t namelen = namelen_b >> 1;
    uni_char* name = OP_NEWA(uni_char, namelen + 1);
    if (name)
	{
		// copy and transform to little endian
		op_memcpy(name, name_be, namelen_b);
		SwitchEndian((UINT8*)name, namelen_b);
		name[namelen] = 0;
	}
	return name;
}

/**
   allocates storage for and converts an ASCII name entry into a
   unicode string - returns NULL on OOM
 */
static inline
uni_char* GetUniNameASCII(const UINT8* name_ascii, size_t namelen)
{
    // allocate storage for name
    uni_char* name = OP_NEWA(uni_char, namelen + 1);
    if (name)
	{
		for (size_t i = 0; i < namelen; ++i)
			name[i] = (uni_char)name_ascii[i];
		name[namelen] = 0;
	}
	return name;
}


void SFNTNameRecord::Read(const UINT8* data)
{
	platformID = GetU16(data); data += 2;
	encodingID = GetU16(data); data += 2;
	languageID = GetU16(data); data += 2;
	nameID     = GetU16(data); data += 2;
	length     = GetU16(data); data += 2;
	offset     = GetU16(data); data += 2;
}

OP_STATUS SFNTNameRecord::GetUnicodeName(const UINT8* rawname, uni_char*& outname) const
{
	uni_char* name;
	if (IS_UNI_NAME(platformID, encodingID))
		name = GetUniNameBE(reinterpret_cast<const uni_char*>(rawname), length);
	else if (IS_ASCII_NAME(platformID, encodingID))
		name = GetUniNameASCII(rawname, length);
	else
		return OpStatus::ERR;

	if (!name)
		return OpStatus::ERR_NO_MEMORY;

	outname = name;
	return OpStatus::OK;
}

BOOL SFNTNameIterator::Init(const UINT8* table, size_t size)
{
	if (size < 6u)
		return FALSE;

	const UINT8* data = table;

	// language tags for format 1 name tables are ignored
	const UINT16 format = GetU16(data); data += 2;
	if (format > 1) // unreecognized format
		return FALSE;

	m_count  = GetU16(data); data += 2;
	m_offset = GetU16(data); data += 2;

	if (size < 6u + m_count*SFNT_NAME_RECORD_SIZE)
		return FALSE;

	m_name_table = table;
	m_name_table_size = size;
	m_current = -1;
	return TRUE;
}

BOOL SFNTNameIterator::Next(INT32 platformID,
							INT32 encodingID,
							INT32 languageID,
							INT32 nameID,
							BOOL english_only/* = FALSE*/)
{
	if (m_current >= m_count)
		return FALSE;

	while (1)
	{
		++ m_current;
		OP_ASSERT(m_current >= 0);

		if (m_current >= m_count)
			return FALSE;

		m_rec.Read(m_name_table + 6u + m_current*SFNT_NAME_RECORD_SIZE);

		// match
		if ((platformID < 0 || platformID == m_rec.platformID) &&
			(encodingID < 0 || encodingID == m_rec.encodingID) &&
			(languageID < 0 || languageID == m_rec.languageID) &&
			(nameID     < 0 || nameID     == m_rec.nameID))
		{
			if (english_only)
			{
				switch (m_rec.platformID)
				{
				case 1: // macintosh
					if (m_rec.encodingID != 0 || m_rec.languageID != 0)
						continue;
					break;
				case 3: // microsoft
					if ((m_rec.languageID & 0x3FF) != 0x009)
						continue;
					break;
				}
			}
			// check that name data is within bounds
			if ((UINT32)(m_offset + m_rec.offset + m_rec.length) <= m_name_table_size)
				return TRUE;
		}
	}
	return FALSE;
}


BOOL GetNameEntry(const UINT8* nametab, UINT32 tablen,
				  UINT16 platformID, UINT16 encodingID, UINT16 languageID, UINT16 nameID,
				  const UINT8*& rawname, UINT16& rawlen)
{
	SFNTNameIterator it;
	if (!it.Init(nametab, tablen) ||
		!it.Next(platformID, encodingID, languageID, nameID))
		return FALSE;

	rawname = it.GetRawNameData();
	rawlen  = it.GetRawNameLength();
	return TRUE;
}

OP_STATUS GetEnglishUnicodeName(const UINT8* nametab, UINT16 tablen,
								UINT16 nameID,
								uni_char*& name)
{
	SFNTNameIterator it;
	if (!it.Init(nametab, tablen))
		return OpStatus::ERR;

	const INT32 platformID = -1;
	const INT32 encodingID = -1;
	const INT32 languageID = -1;
	while (it.Next(platformID, encodingID, languageID, nameID, TRUE))
	{
		const OP_STATUS status = it.GetUnicodeName(name);
		if (OpStatus::IsSuccess(status) ||   // name fetched OK
			OpStatus::IsMemoryError(status)) // OOM
			return status;
		// OpStatus::ERR means unsupported format
	}
	return OpStatus::ERR;
}


BOOL MatchFace(const UINT8* nametab, const UINT32 size,
			   const uni_char* nameBE, const size_t namelen)
{
	SFNTNameIterator it;
	if (!it.Init(nametab, size))
		return FALSE;

	while (it.Next(-1, -1, -1, -1, TRUE))
	{
		// "full font name" and "postscript name"
		if (it.m_rec.nameID != 4 && it.m_rec.nameID != 6)
			continue;

		const UINT16 rawlen = it.GetRawNameLength();

		// compare to nameBE
		if (IS_UNI_NAME(it.m_rec.platformID, it.m_rec.encodingID) &&
			2u*namelen == rawlen)
		{
			if (!op_memcmp(nameBE, it.GetRawNameData(), rawlen))
				return TRUE;
		}
		// compare ASCII
		else if (IS_ASCII_NAME(it.m_rec.platformID, it.m_rec.encodingID) &&
				 namelen == rawlen)
		{
			const UINT8* rawname = it.GetRawNameData();
			const UINT8* name8 = (const UINT8*)nameBE;
			size_t i;
			for (i = 0; i < namelen; ++i)
				if (name8[2u*i] || name8[2u*i+1] != rawname[i])
					break;
			if (i == namelen)
				return TRUE;
		}
	}

	return FALSE;
}

// general sfnt-related stuff

BOOL GetSFNTTable(const UINT8* font, size_t size, UINT32 tag, const UINT8*& table, UINT16& length, UINT32* checksum)
{
    // room for table header
    if (size < SFNT_HEADER_SIZE)
        return FALSE;

    // type check (cannot get table for, unknown and not for ttc without knowing font index)
    const FontType type = GetFontType(GetU32(font));
    if (type != t_ttf && type != t_otf)
        return FALSE;

    // room for table records
    const UINT16 numTables = GetU16(font + 4);
    if (SFNT_HEADER_SIZE + numTables*SFNT_TABLE_DIR_SIZE > size)
        return FALSE;

    UINT32 o = SFNT_HEADER_SIZE;
    for (UINT16 i = 0; i < numTables; ++i)
    {
        TrueTypeTableRecord tab(font + o);
        if (tab.tag == tag && tab.offset + tab.length <= size)
        {
            table = font + tab.offset;
            length = tab.length;
			if (checksum)
				*checksum = tab.checksum;
            return  TRUE;
        }
        o += SFNT_TABLE_DIR_SIZE;
    }

    return FALSE;
}
