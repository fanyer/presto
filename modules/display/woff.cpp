/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2010-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef USE_ZLIB

#include "modules/display/woff.h"
#include "modules/display/sfnt_base.h"
#include "modules/util/opfile/opfile.h"
#include "modules/zlib/zlib.h"

/**
   this struct and compare function is used to determine the order of
   the tables in the sfnt file (as opposed to the order of the table
   directoires). the order of the tables doesn't necessarily
   correspond to the order of the table directories, but it's
   convenient to maintain the original order of tables and directories
   respecitvely since otherwise the checksums of the sfnt font will
   have to be recalculated. also, the selftests in display.woff assume
   that the respecitve orders are maintained.
 */
struct wOFFIndexOffset
{
	UINT16 index;
	UINT32 offset;
};
int offset_compare(const void* ap, const void* bp)
{
    const wOFFIndexOffset* a = static_cast<const wOFFIndexOffset*>(ap);
    const wOFFIndexOffset* b = static_cast<const wOFFIndexOffset*>(bp);
    return a->offset - b->offset;
}

class AutoBuffer : public TempBuffer
{
public:
	UINT8* GetData() const { return reinterpret_cast<UINT8*>(GetStorage()); }
	size_t GetBufferSize() const { return 2*GetCapacity(); }
	OP_STATUS Ensure(size_t size)
	{
		// have to null-terminate or Expand will assert
		if (uni_char* s = GetStorage())
			s[0] = 0;
		return Expand((size+1)>>1);
	}
};

#ifdef MDE_MMAP_MANAGER
# include "modules/libgogi/mde_mmap.h"
/**
   handle to file data - mmap:s file
 */
class FileDataHandle
{
public:
	FileDataHandle() : m_data(0), m_size(0), m_mmap(0) {}
	~FileDataHandle()
	{
		if (m_mmap)
			g_mde_mmap_manager->UnmapFile(m_mmap);
	}
	OP_STATUS Open(const uni_char* path)
	{
		// map the file to memory
		if (!g_mde_mmap_manager)
		{
			g_mde_mmap_manager = OP_NEW(MDE_MMapManager, ());
			if (!g_mde_mmap_manager)
				return OpStatus::ERR_NO_MEMORY;
		}
		MDE_MMAP_File* mmap;
		RETURN_IF_ERROR(g_mde_mmap_manager->MapFile(&mmap, path));
		m_mmap = mmap;
		m_data = (UINT8*)mmap->filemap->GetAddress();
		m_size = mmap->filemap->GetSize();
		return OpStatus::OK;
	}
	OP_STATUS GetData(size_t offset, size_t size, const UINT8*& buf)
	{
		if (offset + size > m_size)
			return OpStatus::ERR;
		buf = m_data + offset;
		return OpStatus::OK;
	}
	virtual size_t GetSize() const { return m_size; }
private:
	const UINT8* m_data;
	size_t m_size;
	struct MDE_MMAP_File* m_mmap;
};
#else // MDE_MMAP_MANAGER
/**
   handle to file data - reads requested chunk to AutoBuffer
 */
class FileDataHandle
{
public:
	FileDataHandle() : m_filelength(0) {}
	OP_STATUS Open(const uni_char* path)
	{
		RETURN_IF_ERROR(m_file.Construct(path, OPFILE_ABSOLUTE_FOLDER));
		RETURN_IF_ERROR(m_file.Open(OPFILE_READ));
		return m_file.GetFileLength(m_filelength);
	}
	OP_STATUS GetData(size_t offset, size_t size, const UINT8*& buf)
	{
		OpFileLength bytes_read;
		if (offset + size > m_filelength)
			return OpStatus::ERR;
		RETURN_IF_ERROR(m_buf.Ensure(size));
		RETURN_IF_ERROR(m_file.SetFilePos(offset, SEEK_FROM_START));
		RETURN_IF_ERROR(m_file.Read(m_buf.GetData(), size, &bytes_read));
		if (size != bytes_read)
			return OpStatus::ERR;
		buf = m_buf.GetData();
		return OpStatus::OK;
	}
	virtual size_t GetSize() const { return static_cast<size_t>(m_filelength); }
private:
	OpFile m_file;
	OpFileLength m_filelength;
	AutoBuffer m_buf;
};
#endif // MDE_MMAP_MANAGER


#define WOFF_TABLE_DIR_SIZE 20u ///< size of WOFF table directory, in bytes
#define WOFF_HEADER_SIZE 44u ///< size of WOFF header, in bytes
#define WOFF_SIGNATURE_SIZE 4 ///< size of WOFF signature, in bytes

/**
   wOFF font header, as defined in the wOFF spec
 */
struct WoffHeader
{
    UINT32	signature;	// 0x774F4646 'wOFF'
    UINT32	flavor;		// The "sfnt version" of the original file: 0x00010000 for TrueType flavored fonts or 0x4F54544F 'OTTO' for CFF flavored fonts.
    UINT32	length;		// Total size of the WOFF file.
    UINT16	numTables;	// Number of entries in directory of font tables.
    UINT16	reserved;	// Reserved, must be set to zero.
    UINT32	totalSfntSize;	// Total size needed for the uncompressed font data, including the sfnt header, directory, and tables.
    UINT16	majorVersion;	// Major version of the WOFF font, not necessarily the major version of the original sfnt font.
    UINT16	minorVersion;	// Minor version of the WOFF font, not necessarily the minor version of the original sfnt font.
    UINT32	metaOffset;	// Offset to metadata block, from beginning of WOFF file; zero if no metadata block is present.
    UINT32	metaLength;	// Length of compressed metadata block; zero if no metadata block is present.
    UINT32	metaOrigLength;	// Uncompressed size of metadata block; zero if no metadata block is present.
    UINT32	privOffset;	// Offset to private data block, from beginning of WOFF file; zero if no private data block is present.
    UINT32	privLength;	// Length of private data block; zero if no private data block is present.

	/**
	   initializes the wOFF file header from data
	   @param data should contain the wOFF file header - WOFF_HEADER_SIZE bytes will be read
	 */
    void Read(const UINT8* data)
    {
        const UINT8* d = data;
        signature	= GetU32(d); d += 4;
        flavor		= GetU32(d); d += 4;
        length		= GetU32(d); d += 4;
        numTables	= GetU16(d); d += 2;
        reserved	= GetU16(d); d += 2;
        totalSfntSize	= GetU32(d); d += 4;
        majorVersion	= GetU16(d); d += 2;
        minorVersion	= GetU16(d); d += 2;
        metaOffset	= GetU32(d); d += 4;
        metaLength	= GetU32(d); d += 4;
        metaOrigLength	= GetU32(d); d += 4;
        privOffset	= GetU32(d); d += 4;
        privLength	= GetU32(d); d += 4;
        OP_ASSERT(data + WOFF_HEADER_SIZE == d);
    }
	/**
	   determines the sanity of the wOFF file, based ont the contents in the file header
	   @param totalWoffSize the size of the wOFF  file - used for bounds-checking
	 */
	BOOL Sane(size_t totalWoffSize)
	{
		if (totalWoffSize < WOFF_HEADER_SIZE + numTables*WOFF_TABLE_DIR_SIZE)
			return FALSE;

		if (reserved != 0 // "If this field is non-zero, a conforming WOFF processor MUST reject the file as invalid"
			|| signature != 0x774f4646 // wOFF ('wOFF') font
			|| (flavor != 0x00010000 && flavor != 0x4f54544f) // truetype (1.0 in 16.16) or opentype ('otto') font
			|| length != totalWoffSize)
			return FALSE;

		// "If the offset and length fields pointing to the metadata or
		// private data block are out of range, indicating a byte range
		// beyond the physical size of the file, a conforming WOFF
		// processor MUST reject the file as invalid"
		if (metaOffset || metaLength || metaOrigLength)
		{
			if (metaOffset + metaLength > totalWoffSize)
				return FALSE;
			// "If no extended metadata is present, the metaOffset,
			// metaLength and metaOrigLength fields in the WOFF header
			// MUST be set to zero"
			if (!(metaOffset && metaLength && metaOrigLength))
				return FALSE;
		}
		if (privOffset || privLength)
		{
			if (privOffset + privLength > totalWoffSize)
				return FALSE;
			// "If no private data is present, the privOffset and
			// privLength fields in the WOFF header MUST be set to zero"
			if (!(privOffset && privLength))
				return FALSE;
		}

		return TRUE;
	}
};
/**
   wOFF table directory, as defined in the wOFF spec
 */
struct WoffTableDir
{
    UINT32	tag;		// 4-byte sfnt table identifier.
    UINT32	offset;		// Offset to the data, from beginning of WOFF file.
    UINT32	compLength;	// Length of the compressed data, excluding padding.
    UINT32	origLength;	// Length of the uncompressed table, excluding padding.
    UINT32	origChecksum;	// Checksum of the uncompressed table.

	/**
	   initializes the table directory from data
	   @param data should contain the wOFF table directory - WOFF_TABLE_DIR_SIZE bytes will be read
	 */
    void Read(const UINT8* data)
    {
        const UINT8* d = data;
        tag		= GetU32(d); d += 4;
        offset		= GetU32(d); d += 4;
        compLength	= GetU32(d); d += 4;
        origLength	= GetU32(d); d += 4;
        origChecksum	= GetU32(d); d += 4;
        OP_ASSERT(data + WOFF_TABLE_DIR_SIZE == d);
    }
	/**
	   determines the sanity of the wOFF table, based on the contents in the table directory
	   @param totalWoffSize the size of the wOFF file - used for bounds-checking
	 */
	BOOL Sane(size_t totalWoffSize)
	{
		// "WOFF files containing table directory entries for which
		// compLength is greater than origLength are considered
		// invalid and MUST NOT be loaded by user agents"
        if (compLength > origLength)
            return FALSE;
        // bounds-check source data
        if (offset + compLength > totalWoffSize)
            return FALSE;
		return TRUE;
	}
};

/**
   helper struct for wOFF to sfnt conversion. this struct holds the
   wOFF header, along with wOFF and sfnt representations of the table
   directories.
 */
class wOFFData
{
private:
	wOFFData() : m_woffDirs(0), m_sfntDirs(0), m_tableOrder(0) {}
	/**
	   reads the wOFF file header
	   @param woffHeaderData should contain the wOFF header data -
	   WOFF_HEADER_SIZE bytes will be read
	   @param totalWoffSize - total size of wOFF file, used for sanity checking
	   @return OpStatus::OK on success, OpStatus::ERR on format error
	 */
	OP_STATUS ReadHeader(const UINT8* woffHeaderData, size_t totalWoffSize);
	/**
	   reads wOFF table directories and creates the corresponding sfnt
	   directories, along with data to determine table order
	   @param woffTableDirectoryData should contains the wOFF table
	   directory data - m_woffHead.numTables*WOFF_TABLE_DIR_SIZE will
	   be read
	   @param totalWoffSize - total size of wOFF file, used for sanity checking
	   @return OpStatus::OK on success, OpStatus::ERR on format error,
	   OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS ReadTableDirectories(const UINT8* woffTableDirectoryData, size_t totalWoffSize);

	/**
	   writes the sfnt font to file
	   @param woffData handle to the wOFF dont data
	   @param file the file (assumed to be created and opened for
	   writing) that the sfnt font will be written to
	   @return OpStatus::OK on success, OpStatus::ERR on format error,
	   OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS WriteSFNTFontInt(FileDataHandle& woffData, OpFile& file) const;

public:
	~wOFFData() { OP_DELETEA(m_woffDirs); OP_DELETEA(m_sfntDirs); OP_DELETEA(m_tableOrder); }
	/**
	   creates a wOFFData handle from a wOFF font in memory
	   @param woffData pointer to the wOFF font in memory
	   @param woffSize the size of the wOFF font, in bytes
	   @param data (out) handle to the created wOFFData - caller should OP_DELETE on success
	   @return OpStatus::OK on success, OpStatus::ERR on format error, OpStatus::ERR_NO_MEMORY on OOM
	 */
	static OP_STATUS Create(const UINT8* woffData, size_t woffSize, wOFFData*& data);
	/**
	   creates a wOFFData handle from a wOFF font file
	   @param woffData handle to the wOFF font file
	   @param data (out) handle to the created wOFFData - caller should OP_DELETE on success
	   @return OpStatus::OK on success, OpStatus::ERR on format error, OpStatus::ERR_NO_MEMORY on OOM
	 */
	static OP_STATUS Create(FileDataHandle& woffData, wOFFData*& data);

	/**
	   writes the sfnt font to memory
	   @param woffData the wOFF font data, in memory
	   @param sfntData (out) pointer to target buffer. on success sfnt
	   font will be written to this location - caller should
	   OP_DELETEA
	   @param sfntSize (out) the size of the sfnt font, in bytes
	   @return OpStatus::OK on success, OpStatus::ERR on format error,
	   OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS WriteSFNTFont(const UINT8* woffData, const UINT8*& sfntData, size_t& sfntSize) const;

	/**
	   writes the sfnt font to sfntFile
	   @param woffData handle to the wOFF dont data
	   @param sfntFile full absolute path to the destination file
	   (will be created) that the sfnt font will be written to. if the
	   function fails the file will be deleted.
	   @return OpStatus::OK on success, OpStatus::ERR on format error,
	   OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS WriteSFNTFont(FileDataHandle& woffData, const uni_char* sfntFile) const;

	WoffHeader m_woffHead;
	WoffTableDir* m_woffDirs;
	TrueTypeTableRecord* m_sfntDirs;
	wOFFIndexOffset* m_tableOrder;
};
OP_STATUS wOFFData::Create(const UINT8* woffData, size_t woffSize, wOFFData*& data)
{
	OpAutoPtr<wOFFData> d(OP_NEW(wOFFData, ()));
	if (!d.get())
		return OpStatus::ERR_NO_MEMORY;
	RETURN_IF_ERROR(d->ReadHeader(woffData, woffSize));
	RETURN_IF_ERROR(d->ReadTableDirectories(woffData + WOFF_HEADER_SIZE, woffSize));
	data = d.release();
	return OpStatus::OK;
}
OP_STATUS wOFFData::Create(FileDataHandle& woffData, wOFFData*& data)
{
	OpAutoPtr<wOFFData> d(OP_NEW(wOFFData, ()));
	if (!d.get())
		return OpStatus::ERR_NO_MEMORY;
	const UINT8* buf;
	RETURN_IF_ERROR(woffData.GetData(0, WOFF_HEADER_SIZE, buf));
	RETURN_IF_ERROR(d->ReadHeader(buf, woffData.GetSize()));
	RETURN_IF_ERROR(woffData.GetData(WOFF_HEADER_SIZE, d->m_woffHead.numTables*WOFF_TABLE_DIR_SIZE, buf));
	RETURN_IF_ERROR(d->ReadTableDirectories(buf, woffData.GetSize()));
	data = d.release();
	return OpStatus::OK;
}
OP_STATUS wOFFData::ReadHeader(const UINT8* woffHeaderData, size_t totalWoffSize)
{
	if (WOFF_HEADER_SIZE > totalWoffSize)
		return OpStatus::ERR;
	m_woffHead.Read(woffHeaderData);
	if (!m_woffHead.Sane(totalWoffSize))
		return OpStatus::ERR;

	return OpStatus::OK;
}
OP_STATUS wOFFData::ReadTableDirectories(const UINT8* woffTableDirectoryData, size_t totalWoffSize)
{
	// should only be called once
	OP_ASSERT(!m_woffDirs && !m_sfntDirs && !m_tableOrder);

    // allocate storage for and read wOFF table directories
    OpAutoArray<WoffTableDir> woffDirs(OP_NEWA(WoffTableDir, m_woffHead.numTables));
    if (!woffDirs.get())
        return OpStatus::ERR_NO_MEMORY;

	const UINT8* d = woffTableDirectoryData;
    for (UINT16 i = 0; i < m_woffHead.numTables; ++i)
    {
        woffDirs[i].Read(d); d += WOFF_TABLE_DIR_SIZE;
		if (!woffDirs[i].Sane(totalWoffSize))
			return OpStatus::ERR;
    }

    // determine order of tables (as opposed to table directories)
    OpAutoArray<wOFFIndexOffset> tableOrder(OP_NEWA(wOFFIndexOffset, m_woffHead.numTables));
    if (!tableOrder.get())
        return OpStatus::ERR_NO_MEMORY;
    for (UINT16 i = 0; i < m_woffHead.numTables; ++i)
	{
        tableOrder[i].index = i;
		tableOrder[i].offset = woffDirs[i].offset;
	}
    op_qsort(tableOrder.get(), m_woffHead.numTables, sizeof(tableOrder[0]), offset_compare);

    // size of sfnt data - accumulates in below loop
    size_t size = SFNT_HEADER_SIZE + m_woffHead.numTables*SFNT_TABLE_DIR_SIZE;

    // allocate storage for and initialize sfnt table directories
    OpAutoArray<TrueTypeTableRecord> sfntDirs(OP_NEWA(TrueTypeTableRecord, m_woffHead.numTables));
    if (!sfntDirs.get())
        return OpStatus::ERR_NO_MEMORY;
    for (UINT16 n = 0; n < m_woffHead.numTables; ++n)
    {
		// directories are initialized in table order (rather than
		// table directory order) so that size can be grown
		// incrementally and used to set offset in sfnt font. order of
		// sfntDirs and woffDirs is still the same though, and
		// corresponds to the order of the table directories.
        UINT16 i = tableOrder[n].index;

        sfntDirs[i].tag = woffDirs[i].tag;
        sfntDirs[i].checksum = woffDirs[i].origChecksum;
        sfntDirs[i].offset = size;
        sfntDirs[i].length = woffDirs[i].origLength;

        size += FOUR_BYTE_ALIGN(sfntDirs[i].length);
    }

    // " If this value is incorrect, a conforming WOFF processor MUST
    // reject the file as invalid"
    if (size != m_woffHead.totalSfntSize)
        return OpStatus::ERR;

	m_woffDirs = woffDirs.release();
	m_sfntDirs = sfntDirs.release();
	m_tableOrder = tableOrder.release();
	return OpStatus::OK;
}



/**
   writes sfnt header corresponding to the wOFF header to out

   @param woffHeader the wOFF header for which to write a
   corresponding sfnt header
   @param out pointer to target data - no bounds-checking is made,
   total data written is SFNT_HEADER_SIZE bytes
 */
static
void WriteSFNTHeader(const WoffHeader& woffHeader, UINT8* out)
{
    UINT16 log2 = 0;
    while ((1 << (log2+1)) <= woffHeader.numTables)
        ++ log2;
    const UINT16 searchRange = 1 << (log2 + 4);

    UINT8* d = out;
    SetU32(d, woffHeader.flavor);		d += 4;
    SetU16(d, woffHeader.numTables);	d += 2;
    SetU16(d, searchRange);				d += 2;
    SetU16(d, log2);					d += 2;
    SetU16(d, (woffHeader.numTables << 4) - searchRange);	d += 2;
    OP_ASSERT(out + SFNT_HEADER_SIZE == d);
}

/**
   writes sfnt table directories to out

   @param sfntDirs sfnt table directories
   @param tabCount number of snft tables
   @param out pointer to target data - no bounds-checking is made,
   total data written is tabCount*SFNT_TABLE_DIR_SIZE bytes
 */
static
void WriteSFNTTableDirectories(const TrueTypeTableRecord* sfntDirs, UINT16 tabCount, UINT8* out)
{
    for (size_t i = 0; i < tabCount; ++i)
    {
        sfntDirs[i].Write(out);
        out += SFNT_TABLE_DIR_SIZE;
    }
}

/**
   inflates (if necessary) and writes an sfnt table to out

   @param woffDir the woff table directory of the table to be written
   @param woffTable pointer to the table in the wOFF file (optionally compressed)
   @param out pointer to target data, inflated table will be written
   to this location - no bounds-checking is made, total data written
   is FOUR_BYTE_ALIGN(woffDir.origLength) bytes
   @return OpStatus::OK on success, OpStatus::ERR on inflate error
 */
static
OP_STATUS WriteSFNTTable(const WoffTableDir& woffDir, const UINT8* woffTable, UINT8* out)
{
    // table is stored uncompressed
    if (woffDir.compLength == woffDir.origLength)
    {
		op_memcpy(out, woffTable, woffDir.origLength);
    }
    // create output buffer and decompress table
    else
    {
        // initialize zlib stream
        z_stream stream;
        op_memset(&stream, 0, sizeof(stream));
        stream.next_in   = const_cast<Bytef*>(woffTable);
        stream.avail_in  = woffDir.compLength;
        stream.next_out  = out;
        stream.avail_out = woffDir.origLength;

        // inflate table
		// "Any error in decompression or discrepancy in table size
		// means the WOFF file is invalid and must not be used"
		OP_STATUS status = OpStatus::OK;
        if (inflateInit(&stream) != Z_OK
            || inflate(&stream, Z_FINISH) != Z_STREAM_END
			|| stream.total_out != woffDir.origLength)
			status = OpStatus::ERR;
		if (inflateEnd(&stream) != Z_OK)
			status = OpStatus::ERR;
		RETURN_IF_ERROR(status);
    }

    if (UINT32 pad = FOUR_BYTE_ALIGN(woffDir.origLength) - woffDir.origLength)
        op_memset(out + woffDir.origLength, 0, pad);
	return OpStatus::OK;
}

OP_STATUS wOFFData::WriteSFNTFont(const UINT8* woffData, const UINT8*& sfntData, size_t& sfntSize) const
{
    // allocate storage for sfnt font
    OpAutoArray<UINT8> sfnt(OP_NEWA(UINT8, m_woffHead.totalSfntSize));
    if (!sfnt.get())
        return OpStatus::ERR_NO_MEMORY;

    WriteSFNTHeader(m_woffHead, sfnt.get());
    WriteSFNTTableDirectories(m_sfntDirs, m_woffHead.numTables, sfnt.get() + SFNT_HEADER_SIZE);
    for (UINT32 i = 0; i < m_woffHead.numTables; ++i)
    {
        OP_ASSERT(m_woffDirs[i].origLength == m_sfntDirs[i].length);
        RETURN_IF_ERROR(WriteSFNTTable(m_woffDirs[i], woffData + m_woffDirs[i].offset, sfnt.get() + m_sfntDirs[i].offset));
    }
    sfntData = const_cast<const UINT8*>(sfnt.release());
	sfntSize = m_woffHead.totalSfntSize;
    return OpStatus::OK;
}

OP_STATUS wOFF2sfnt(const UINT8* woffData, const size_t woffSize,
                    const UINT8*& sfntData, size_t& sfntSize)
{
	OP_ASSERT(IswOFF(woffData, woffSize));
	wOFFData* woff;
	RETURN_IF_ERROR(wOFFData::Create(woffData, woffSize, woff));
	// write sfnt font to buffer
    OP_STATUS status = woff->WriteSFNTFont(woffData, sfntData, sfntSize);
	OP_DELETE(woff);
	return status;
}

BOOL IswOFF(const UINT8* fontData, const size_t fontSize)
{
	return fontSize >= WOFF_SIGNATURE_SIZE && GetU32(fontData) == 0x774f4646; // 'wOFF'
}

/**
   inflates (if necessary) and writes an sfnt table to
   out. internally, inflation is done to buf. current capacity
   determines the amount of data to be deflated in one go.

   @param woffDir the woff table directory of the table to be written
   @param woffTable pointer to the table in the wOFF file (optionally
   compressed)
   @param buf intermediate target for inflation. buf will not be
   grown, current capacity affects how many calls to inflate will be
   made.
   @param out inflated table will be written to this file - total data
   written is FOUR_BYTE_ALIGN(woffDir.origLength) bytes
   @return OpStatus::OK on success, OpStatus::ERR on inflate error or
   failure to write to file
 */
static
OP_STATUS WriteSFNTTable(const WoffTableDir& woffDir, const UINT8* woffTable, AutoBuffer& buf, OpFile& out)
{
    // table is stored uncompressed
    if (woffDir.compLength == woffDir.origLength)
    {
		RETURN_IF_ERROR(out.Write(woffTable, woffDir.origLength));
    }
    // create output buffer and decompress table
    else
    {
		// use current buffer size as block size
		const size_t blockSize = buf.GetBufferSize();

        // initialize zlib stream
        z_stream stream;
        op_memset(&stream, 0, sizeof(stream));
        stream.next_in   = const_cast<Bytef*>(woffTable);
        stream.avail_in  = woffDir.compLength;

		if (inflateInit(&stream) != Z_OK)
			return OpStatus::ERR;

		// inflate data
		while (1)
		{
			// write as much as possible
			stream.next_out  = buf.GetData();
			stream.avail_out = blockSize;
			const int z_status = inflate(&stream, Z_SYNC_FLUSH);

			// something went wrong
			if (z_status != Z_OK && z_status != Z_STREAM_END)
			{
				inflateEnd(&stream);
				return OpStatus::ERR;
			}

			// write inflated data to file
			OP_STATUS op_status = out.Write(buf.GetData(), blockSize - stream.avail_out);
			if (OpStatus::IsError(op_status))
			{
				inflateEnd(&stream);
				return op_status;
			}

			// done
			if (z_status == Z_STREAM_END)
			{
				if (inflateEnd(&stream) != Z_OK || stream.total_out != woffDir.origLength)
					return OpStatus::ERR;
				break;
			}
		}
    }

	if (UINT32 pad = FOUR_BYTE_ALIGN(woffDir.origLength) - woffDir.origLength)
	{
		UINT32 zero = 0;
		RETURN_IF_ERROR(out.Write(reinterpret_cast<UINT8*>(&zero), pad));
	}
	return OpStatus::OK;
}

OP_STATUS wOFFData::WriteSFNTFontInt(FileDataHandle& woffData, OpFile& file) const
{
	AutoBuffer buf;
	size_t size;

	size = SFNT_HEADER_SIZE;
	RETURN_IF_ERROR(buf.Ensure(size));
    WriteSFNTHeader(m_woffHead, buf.GetData());
	RETURN_IF_ERROR(file.Write(buf.GetData(), size));

	size = m_woffHead.numTables*SFNT_TABLE_DIR_SIZE;
	RETURN_IF_ERROR(buf.Ensure(size));
    WriteSFNTTableDirectories(m_sfntDirs, m_woffHead.numTables, buf.GetData());
	RETURN_IF_ERROR(file.Write(buf.GetData(), size));

	// inflate at least this much in one go
	RETURN_IF_ERROR(buf.Ensure(DISPLAY_WOFF_INFLATE_BLOCK_SIZE));

    for (UINT32 n = 0; n < m_woffHead.numTables; ++n)
    {
		// appending data - have to write in table order
		const UINT32 i = m_tableOrder[n].index;

        OP_ASSERT(m_woffDirs[i].origLength == m_sfntDirs[i].length);

		// load table data
		const UINT8* table;
		RETURN_IF_ERROR(woffData.GetData(m_woffDirs[i].offset, m_woffDirs[i].compLength, table));
		// write table data
        RETURN_IF_ERROR(WriteSFNTTable(m_woffDirs[i], table, buf, file));
    }
    return OpStatus::OK;
}
OP_STATUS wOFFData::WriteSFNTFont(FileDataHandle& woffData, const uni_char* sfntFile) const
{
	OpFile file;
	RETURN_IF_ERROR(file.Construct(sfntFile));
	RETURN_IF_ERROR(file.Open(OPFILE_WRITE));
	OP_STATUS status = WriteSFNTFontInt(woffData, file);
	OpStatus::Ignore(file.Close());
	if (OpStatus::IsError(status))
		OpStatus::Ignore(file.Delete(FALSE));
	return status;
}

OP_STATUS wOFF2sfnt(const uni_char* woffFile, const uni_char* sfntFile)
{
	OP_ASSERT(IswOFF(woffFile) == OpBoolean::IS_TRUE);
	FileDataHandle woffData;
	RETURN_IF_ERROR(woffData.Open(woffFile));
	wOFFData* woff;
	RETURN_IF_ERROR(wOFFData::Create(woffData, woff));
	// write sfnt font to file
    OP_STATUS status = woff->WriteSFNTFont(woffData, sfntFile);
	OP_DELETE(woff);
	return status;
}

OP_BOOLEAN IswOFF(const uni_char* fontFile)
{
	OpFile f;
	RETURN_IF_ERROR(f.Construct(fontFile));
	RETURN_IF_ERROR(f.Open(OPFILE_READ));
	OpFileLength bytes_read;
	if (f.GetFileLength() < WOFF_SIGNATURE_SIZE)
		return OpBoolean::IS_FALSE;
	UINT8 data[WOFF_SIGNATURE_SIZE];
	RETURN_IF_ERROR(f.Read(data, WOFF_SIGNATURE_SIZE, &bytes_read));
	if (WOFF_SIGNATURE_SIZE != bytes_read)
		return OpStatus::ERR;
	return IswOFF(data, WOFF_SIGNATURE_SIZE) ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}

#endif // USE_ZLIB
