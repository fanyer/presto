/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** wonko
*/

#ifndef OPERA_SFNT_H
#define OPERA_SFNT_H

// endian-related stuff

#define MAKE_TAG( _x1, _x2, _x3, _x4 ) \
    ( ( (UINT32)_x1 << 24 ) |           \
      ( (UINT32)_x2 << 16 ) |           \
      ( (UINT32)_x3 <<  8 ) |           \
      (UINT32)_x4         )

template <typename T, const int N>
T encode_value(const UINT8* data)
{
    T result = 0;
    for (int i = 0; i < N; ++i)
        result = (result << 8) | data[i];
    return result;
}
inline UINT32 GetU32(const UINT8* p) { return encode_value<UINT32, 4>(p); }
inline UINT16 GetU16(const UINT8* p) { return encode_value<UINT16, 2>(p); }

inline void SetU32(UINT8* p, UINT32 v) { *(UINT32*)p = encode_value<UINT32, 4>((UINT8*)&v); }
inline void SetU16(UINT8* p, UINT32 v) { *(UINT16*)p = encode_value<UINT16, 2>((UINT8*)&v); }



/**
   handle for holding a font-file in memory, using mmapping if
   available, otherwise storing entire font in memory. does its own
   clean-up.
 */
class FontHandle
{
public:
	FontHandle()
		: m_data(NULL), m_size(0)
#ifdef MDE_MMAP_MANAGER
		, m_mmap(NULL)
#endif // MDE_MMAP_MANAGER
	{}
	~FontHandle();
	/**
	   makes file accessible as memory
	   @param file the path to the file
	 */
	OP_STATUS LoadFile(const uni_char* filename);
	const UINT8* Data() { return m_data; }
	size_t Size() { return m_size; }

private:
#ifdef MDE_MMAP_MANAGER
	OP_STATUS LoadMMAPFile(const uni_char* filename);
#endif // MDE_MMAP_MANAGER

	const UINT8* m_data;
	size_t m_size;
#ifdef MDE_MMAP_MANAGER
	struct MDE_MMAP_File* m_mmap;
#endif // MDE_MMAP_MANAGER
};



#define FOUR_BYTE_ALIGN(v) ((((v) + 3) >> 2) << 2)


enum FontType
{
    t_ttf = 0,
    t_ttc,
    t_otf,
    t_unknown
};
FontType GetFontType(const UINT32 tag);

#define SFNT_HEADER_SIZE 12u
#define SFNT_TABLE_DIR_SIZE 16u
class TrueTypeTableRecord : public Link
{
public:
	TrueTypeTableRecord()
		: tag(0), checksum(0), offset(0), length(0), padding(0), copied_data(0) {}
	TrueTypeTableRecord(UINT32 tag, UINT32 checksum, UINT32 offset, UINT32 length)
		: tag(tag), checksum(checksum), offset(offset), length(length), padding(0), copied_data(0) {}
    TrueTypeTableRecord(const UINT8* t)
		: padding(0), copied_data(0) { Read(t); }
	~TrueTypeTableRecord() { if (copied_data) OP_DELETEA(copied_data); }

	void Read(const UINT8* t)
	{
		OP_ASSERT(!copied_data);

        tag = GetU32(t);
        checksum = GetU32(t+4);
        offset = GetU32(t+8);
        length = GetU32(t+12);
	}

	void Write(UINT8* t) const
	{
		SetU32(t+0,  tag);
		SetU32(t+4,  checksum);
		SetU32(t+8,  offset);
		SetU32(t+12, length);
	}

	UINT32 tag;
	UINT32 checksum;
	UINT32 offset;
	UINT32 length;

	UINT8 padding;
	UINT8* copied_data;

private:
	TrueTypeTableRecord(const TrueTypeTableRecord &g);
	TrueTypeTableRecord& operator=(const TrueTypeTableRecord&);

};

/**
   changes endian of 16 bit data
   @param data the data
   @param len the length of the data, in bytes (should be even)
*/
inline void SwitchEndian(UINT8* data, size_t len)
{
    OP_ASSERT(!(len & 0x1));
    for (size_t i = 0; i < len; i += 2)
    {
        const UINT8 tmp = data[i];
        data[i+0] = data[i+1];
        data[i+1] = tmp;
    }
}


// name table-related stuff

#define SFNT_NAME_RECORD_SIZE 12u
struct SFNTNameRecord
{
	// no bounds-checking is made
	void Read(const UINT8* data);
	/**
	   allocates storeage for and converts a raw name entry into a
	   unicode string, if encoding is supported.
	   @param rawname the raw name data for the name record
	   @param outname (out) the resulting unicode string
	   @return
	   OpStatus::OK on successful conversion
	   OpStatus::ERR on unuspported format
	   OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS GetUnicodeName(const UINT8* rawname, uni_char*& outname) const;

	UINT16 platformID;
	UINT16 encodingID;
	UINT16 languageID;
	UINT16 nameID;
	UINT16 length;
	UINT16 offset;
};

/**
   helper struct to iterate over entries in a name table. each time
   Next is called, iterator moves to the next entry matching the
   passed IDs.

   example use, iterate over all names:

   SFNTNameIterator it;
   if (it.Init(name_table, name_table_size))
       while (it.Next(-1, -1, -1, -1))
	       ; // do stuff here
 */
struct SFNTNameIterator
{
#ifdef DEBUG_ENABLE_OPASSERT
	SFNTNameIterator() : m_count(0xffff), m_current(0) {}
#endif // DEBUG_ENABLE_OPASSERT

	/**
	   initializes the iterator for iteration over name entries
	   @param table should point to an sfnt name table
	   @param size size of the name table
	   @return TRUE if initialization went well, FALSE on data error
	 */
	BOOL Init(const UINT8* table, size_t size);
	/**
	   moves to the next name in the table matching passed IDs -
	   negative values are wild
	   @param platformID platform to match, negative matches any
	   @param encodingID encoding to match, negative matches any
	   @param languageID language to match, negative matches any
	   @param nameID name to match, negative matches any
	   @param english_only if TRUE, match only english names

	   @return TRUE if a name matching IDs was found, FALSE means no
	   more names are available
	 */
	BOOL Next(INT32 platformID,
			  INT32 encodingID,
			  INT32 languageID,
			  INT32 nameID,
			  BOOL english_only = FALSE);

	/**
	   returns pointer to the raw name data corresponding to the
	   current name entry
	 */
	const UINT8* GetRawNameData() const;
	/**
	   returns the length of the name data corresponding to the
	   current name entry, in bytes
	 */
	UINT16 GetRawNameLength() const;

	/**
	   fetches the unicode name corresponding to the current name
	   entry. outname will be allocated, filled with the name and
	   null-terminated.
	   @return
	   OpStatus::OK if allocation and conversion was successful
	   OpStatus::ERR on bounds-error or unsupported format
	   OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS GetUnicodeName(uni_char*& outname) const;

	const UINT8* m_name_table; // pointer to start of name table
	size_t m_name_table_size;  // size of name table

	UINT16 m_count;  // number of names in table
	UINT16 m_offset;  // offset to start of string storage

	INT32 m_current; // index of current name

	SFNTNameRecord m_rec; // current name entry
};
inline
const UINT8* SFNTNameIterator::GetRawNameData() const
{
	// uninitialized or out-of-bounds
	OP_ASSERT(m_current >= 0 && m_current < m_count);

	const UINT8* name = m_name_table + m_offset + m_rec.offset;
	// should never trigger, or Next failed to bounds-check properly
	OP_ASSERT(name + m_rec.length <= m_name_table + m_name_table_size);
	return name;
}
inline
UINT16 SFNTNameIterator::GetRawNameLength() const
{
	// uninitialized or out-of-bounds
	OP_ASSERT(m_current >= 0 && m_current < m_count);

	return m_rec.length;
}
inline
OP_STATUS SFNTNameIterator::GetUnicodeName(uni_char*& outname) const
{
	// uninitialized or out-of-bounds
	OP_ASSERT(m_current >= 0 && m_current < m_count);

	return m_rec.GetUnicodeName(GetRawNameData(), outname);
}

/**
   fetches pointer to and length of raw name entry matching passed IDs -
   see http://www.microsoft.com/typography/otspec/name.htm for further details
   @param nametab the sfnt name table
   @param tablen the size (in bytes) of the sfnt name table
   @param platformID the platform id of the desired name
   @param encodingID the encoding id of the desired name (note that unicode BMP is big endian)
   @param languageID the language id of the desired name
   @param nameID     the name id of the desired name
   @param rawname (out) a pointer to the desired name entry
   @param rawlen (out) the length of the desired name entry
   @return TRUE on match, FALSE if name is not in name table
*/
BOOL GetNameEntry(const UINT8* nametab, UINT32 tablen,
                  UINT16 platformID, UINT16 encodingID, UINT16 languageID, UINT16 nameID,
                  const UINT8*& rawname, UINT16& rawlen);

/**
   fetches english unicode name matching nameID from an sfnt name table.

   @param nametab the name table
   @param tablen the length of the name table
   @param nameID the name ID of the name to fetch
   @param name (out) on success, points to the name, in unicode little
   endian - caller obtains ownership and should OP_DELETEA

   @return
    OpStatus::ERR if no name with supported format matching nameID is in table,
    OpStatus::ERR_NO_MEMORY on OOM,
    OpStatus::OK on success
 */
OP_STATUS GetEnglishUnicodeName(const UINT8* nametab, UINT16 tablen,
								UINT16 nameID,
								uni_char*& name);


/**
   matches a passed big endian string against occurances of a certain
   name in an sfnt name table. only matches against english "full font
   name" and "postscript name" entries.

   @param nametab the sfnt name table
   @param size the size of the name table, in bytes
   @param nameBE the big endian unicode BMP string to look for
   @param namelen the length of nameBE, in characters
   @return TRUE if an entry is found to match, FALSE otherwise
 */
BOOL MatchFace(const UINT8* nametab, const UINT32 size,
               const uni_char* nameBE, const size_t namelen);


// genral sfnt-related stuff

class SFNTChecksumAdder
{
public:
	static UINT32 CalculateChecksum(const UINT8* data, const UINT32 size)
	{
		SFNTChecksumAdder adder;
		adder.addData(data, size);
		return adder.getChecksum();
	}

	SFNTChecksumAdder() : checksum(0), worklong(0), pos(0) {}

	void addData(const UINT8* data, UINT32 size)
	{
		const UINT8* pos = data;
		for(UINT32 i=0; i < size; i++)
			addUINT8(*pos++);
	}

	void addUINT8(UINT8 value)
	{
		worklong <<= 8;
		worklong += value;
		pos += 1;
		if (pos == 4)
		{
			checksum += worklong;
			worklong = pos = 0;
		}
	}

	void addUINT16(UINT16 value)
	{
		addUINT8(value >> 8);
		addUINT8(value & 0xff);
	}

	void addUINT32(UINT32 value)
	{
		addUINT16(value >> 16);
		addUINT16(value & 0xffff);
	}

	UINT32 getChecksum() 
	{
		short temppos = pos;
		UINT32 tempworklong = worklong;

		while (temppos++ < 4)
			tempworklong <<= 8;

		return checksum + tempworklong;
	}

private:
	UINT32 checksum;
	UINT32 worklong;
	short pos;
};

/**
   fetches an sfnt table from an sfnt font file
   @param font the sfnt font
   @param size the size of the font, in bytes
   @param tag the tag of the font to fetch, as an UINT32 (see MakeSFNTTag)
   @param table (out) will be set to point to the start of the table if found
   @param length (out) the size of the fetched table, in bytes
   @return TRUE if a table matching the specified tag is found (and in range), FALSE otherwise
 */
BOOL GetSFNTTable(const UINT8* font, size_t size, UINT32 tag, const UINT8*& table, UINT16& length, UINT32* checksum = 0);

/**
   converts a string into a UINT32 sfnt tag
   @param tagstr the tag as a string - exactly four characters are
   used, use ' ' (blankspace) when tag contains less than four
   characters
   @return the resulting tag
 */
inline UINT32 MakeSFNTTag(const char* tagstr) { return encode_value<UINT32, 4>((UINT8*)tagstr); }

#endif // !OPERA_SFNT_H
