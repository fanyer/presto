/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/**
 * @file zipload.h
 * Encapsulates i/o towards ZIP archives.
 * Documentation of the file format can be found at
 * http://www.pkware.com/products/enterprise/white_papers/appnote.html
 */
#ifndef MODULES_UTIL_ZIPLOAD_H
#define MODULES_UTIL_ZIPLOAD_H

#ifdef USE_ZLIB

#include "modules/zlib/zlib.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/opfile/opmemfile.h"
#include "modules/util/adt/opvector.h"

#ifdef UTIL_ZIP_CACHE
#include "modules/hardcore/timer/optimer.h"
#include "modules/util/OpHashTable.h"
#define g_zipcache g_opera->util_module.m_zipcache
#endif // UTIL_ZIP_CACHE

class OpZip;

class OpZipFile
{
	friend class OpZip;
protected:
	unsigned char*	m_pData;
	unsigned long	m_dwPos;
	unsigned long	m_dwSize;

	OP_STATUS Init(unsigned char* pData, unsigned long dwSize);

public:
	OpZipFile();

	~OpZipFile()
	{
		Close();
	}

	OP_STATUS Read(void* pVoid, unsigned long dwSize, unsigned long *pdwRead);

	BOOL IsOpen() const
	{
		return m_pData!=NULL;
	}
	BOOL Close();
	unsigned long GetSize() const
	{
		return m_dwSize;
	}
	unsigned long GetPosition() const
	{
		return m_dwPos;
	}
};

/**
 * Class containing functions for opening a zip-file. And also open
 * the files contained in it.
 *
 * Due to current implementation limitations, we only have limited support for
 * zip64 archives. The data structures defined for zip64 extensions are
 * supported, but we will not be able to read more than 2^32-1 file entries,
 * and we will not be able to read compressed files larger than 2^32-1 bytes
 * (compare CORE-43103, CORE-43104). On 32-bit systems, memory constraints
 * may limit what we can access even more.
 */
class OpZip
{
public:

	enum
	{
		DIR_SIGNATURE			= 0x06054b50,	/**< End of Central Directory */
		ZIP64_LOCATOR_SIGNATURE	= 0x07064b50,	/**< Zip64 End of Central Directory Locator */
		ZIP64_DIR_SIGNATURE		= 0x06064b50,	/**< Zip64 End of Central Directory */
		FILE_SIGNATURE			= 0x02014b50,	/**< Central Directory File Header */
		LOCAL_SIGNATURE			= 0x04034b50,	/**< Local File Header */
		EXT_HEADER_SIGNATURE	= 0x08074b50,	/**< Spanning Marker */
	};

	enum
	{
		COMP_STORE				= 0,	/**< Compression: No compression (stored) */
		COMP_DEFLATE			= 8		/**< Compression: Deflate */
	};

	enum
	{
		COMP_DEFLATE_OPT_NORMAL	= 0		/**< Extra bit flags to use for Deflate compression */
	};

	enum
	{
		FILE_HEADER_LENGTH			= 30,		/**< Length of a Local File Header */
		CENTRAL_DIR_LENGTH			= 46,		/**< Length of a Central Directory Entry */
		END_CENTRAL_DIR_LENGTH		= 22,		/**< Length of End of Central Directory record */
		ZIP64_LOCATOR_LENGTH		= 20,		/**< Length of Zip64 Locator record */
		ZIP64_END_CENTRAL_DIR_LENGTH= 56		/**< Length of Zip64 End of Central Directory record */

#ifdef UTIL_ZIP_WRITE
		, LOCAL_GENERAL_FLAG_OFFSET		=  6	/**< Local File Header: Offset of Gernal Purpose Bit Flag */
		, LOCAL_COMP_METHOD_FLAG_OFFSET	=  8	/**< Local File Header: Offset of Compression Method */
		, LOCAL_CRC32_OFFSET				= 14	/**< Local File Header: Offset of CRC-32 value */
		, LOCAL_COMP_LENGTH_OFFSET		= 18	/**< Local File Header: Offset of Compressed Length */

		, DATA_DESC_COMP_LEN_OFFSET		=  4	/**< Data Descriptor: Offset of Compressed Length */

		, CENTRAL_DIR_GENERAL_FLAG_OFFSET	=  8	/**< Central Diretcory: Offset of Gernal Purpose Bit Flag */
		, CENTRAL_DIR_COMP_METHOD_OFFSET	= 10	/**< Central Directory: Offset of Compression Method Flag */
		, CENTRAL_DIR_CRC32_OFFSET		= 16	/**< Central Directory: Offset of CRC-32 value */
		, CENTRAL_DIR_COMP_LENGTH_OFFSET	= 20	/**< Central Directory: Offset of Compressed Length */
		, CENTRAL_DIR_LOCAL_START_OFFSET  = 42	/**< Central Directory: Offset of Local File Header */

		, END_CENTRAL_DIR_START_OFFSET	= 16	/**< Offset of Start of Central Directory in End of Central Directory record */
#endif // UTIL_ZIP_WRITE
	};

	/** Largest data type for zip64 support. */
#ifdef HAVE_UINT64
	typedef UINT64 UINTBIG;
	typedef  INT64  INTBIG;
#else
	typedef UINT32 UINTBIG;
	typedef  INT32  INTBIG;
#endif

	/*
	compression method: (2 bytes)
	0 -The file is stored (no compression)
	1 -The file is Shrunk
	2 -The file is Reduced with compression factor 1
	3 -The file is Reduced with compression factor 2
	4 -The file is Reduced with compression factor 3
	5 -The file is Reduced with compression factor 4
	6 -The file is Imploded
	7 -Reserved for Tokenizing compression algorithm
	8 -The file is Deflated
	9 -Enhanced Deflating using Deflate64(tm)
	10-PKWARE Date Compression Library Imploding
	*/

	struct end_of_central_directory_record
	{
		UINT32 signature;				///< End of central dir signature (0x06054b50)
		UINT16 this_disk;				///< Number of this disk
		UINT16 central_start_disk;		///< Number of the disk with the start of the central directory
		UINTBIG entries_here;			///< Total number of entries in the central dir on this disk
		UINTBIG file_count;				///< Total number of entries in the central dir
		UINTBIG central_size;			///< Size of the central directory
		UINTBIG central_start_offset;	///< Offset of start of central directory with respect to the starting disk number
		UINT16 comment_len;				///< zipfile comment length (c)
		end_of_central_directory_record() : signature(0), this_disk(0), central_start_disk(0), entries_here(0), file_count(0), central_size(0), central_start_offset(0), comment_len(0) {}
	};

	struct central_directory_record
	{
		UINT32 signature;			///< central file header signature 4 bytes (0x02014b50)
		UINT16 version_made_by;		///< version made by 2 bytes
		UINT16 ver_2_extract;		///< version needed to extract 2 bytes
		UINT16 general_flags;		///< general purpose bit flag
		UINT16 comp_method;			///< compression method
		UINT16 date;				///< last mod file time
		UINT16 time;				///< last mod file date
		UINT32 crc32;				///< crc-32
		UINTBIG comp_size;			///< compressed size
		UINTBIG uncomp_size;		///< uncompressed size
		UINT16 filename_len;		///< file name length
		UINT16 extra_len;			///< extra field length
		UINT16 comment_len;			///< file comment length
		UINT32 start_disk;			///< disk number start
		UINT16 int_file_attr;		///< internal file attributes
		UINT32 ext_file_attr;		///< external file attributes
		UINTBIG local_offset;		///< relative offset of local header
		char * filename;
		UINT8 * extra;
		char * comment;
		char * GetName() const { return filename; };
		UINT8 * GetExtra() const { return extra; };
		char * GetComment() const { return comment; };
		central_directory_record() : signature(0), version_made_by(0), ver_2_extract(0), general_flags(0), comp_method(0), date(0), time(0), crc32(0), comp_size(0), uncomp_size(0), filename_len(0), extra_len(0), comment_len(0), start_disk(0), int_file_attr(0), ext_file_attr(0), local_offset(0), filename(NULL), extra(NULL), comment(NULL) {}
	};

	struct name_descriptor
	{
		char*	filename;
		char*	extra;
		name_descriptor() : filename(NULL), extra(NULL) {}
	};

	struct local_file_header_record
	{
		UINT32 signature;		///< local file header signature 4 bytes (0x04034b50)
		UINT16 ver_2_extract;	///< version needed to extract
		UINT16 general_flags;	///< general purpose bit flag
		UINT16 comp_method;		///< compression method
		UINT16 date;			///< last mod file timetores file last modification date and time
		UINT16 time;			///< last mod file date
		UINT32 crc32;			///< CRC-32
		UINTBIG comp_size;		///< compressed size
		UINTBIG uncomp_size;	///< uncompressed size
		UINT16 filename_len;	///< file name length
		UINT16 extra_len;		///< extra field length
		name_descriptor* name;	///< filename and extra info fields

		local_file_header_record() : signature(0), ver_2_extract(0), general_flags(0), comp_method(0), date(0), time(0), crc32(0), comp_size(0), uncomp_size(0), filename_len(0), extra_len(0), name(NULL) {}
	};

	struct data_descriptor
	{
		UINT32 crc32;			///< CRC-32
		UINT32 comp_size;		///< compressed size
		UINT32 uncomp_size;		///< uncompressed size
		data_descriptor() : crc32(0), comp_size(0), uncomp_size(0) {}
	};

	struct file_attributes
	{
		UINT16 version_made_by;	///< version made by 2 bytes
		UINT16 int_attr;		///< internal file attributes
		UINT32 ext_attr;		///< external file attributes
		UINT16 date;			///< last mod file timetores file last modification date and time
		UINT16 time;			///< last mod file date
		UINTBIG length;			///< uncompressed size
	};

	struct index_entry
	{
		OpString name;
		INTBIG idx;
	};

	private:
		OpFile*									m_thisfile;
		end_of_central_directory_record			m_end_of_central_directory;
		central_directory_record**				m_Files;
		unsigned char*							m_DirData;
		OpFileLength							m_sizeofFile;
		OpFileLength							m_pos;				//<<< current position in the zipfile

		index_entry**							m_index_list;

		OP_STATUS CreateIndex();
		void CleanIndex();
#ifdef UTIL_ZIP_CACHE
		UINT32									m_usage_count;
#ifndef CAN_DELETE_OPEN_FILE
		time_t									m_mod_time;
#endif // CAN_DELETE_OPEN_FILE
#endif // UTIL_ZIP_CACHE
		int										m_flags;

	// Support functions for reading and writing little-endian values in zip archives
	/** Read a little-endian 64-bit value.
	 * @return ERR_NOT_SUPPORTED if we do not have a 64-bit data type and are
	 *         trying read a number beyond what is representable by 32 bits.
	 *         OK elsewhere.
	 */
	static OP_STATUS BytesToUInt64(UINT8 * bytes, UINTBIG &val)
	{
#ifdef HAVE_UINT64
# if defined(OPERA_BIG_ENDIAN) || defined(NEEDS_RISC_ALIGNMENT)
		val = bytes[7]; val <<= 8; val |= bytes[6]; val <<= 8; val|=bytes[5]; val <<= 8; val|=bytes[4]; val <<=8;
		val|= bytes[3]; val <<= 8; val |= bytes[2]; val <<= 8; val|=bytes[1]; val <<= 8; val|=bytes[0];
# else
		val = *reinterpret_cast<UINT64 *>(bytes);
# endif
#else // ! HAVE_UINT64
		if (bytes[7] || bytes[6] || bytes[5] || bytes[4])
			return OpStatus::ERR_NOT_SUPPORTED;
# if defined(OPERA_BIG_ENDIAN) || defined(NEEDS_RISC_ALIGNMENT)
		val|= bytes[3]; val <<= 8; val |= bytes[2]; val <<= 8; val|=bytes[1]; val <<= 8; val|=bytes[0];
# else
		val = *reinterpret_cast<UINT32 *>(bytes);
# endif
#endif
		return OpStatus::OK;
	}

	/** Read a little-endian 32-bit value. */
	static UINT32 BytesToUInt32(UINT8 * bytes)
	{
#if defined(OPERA_BIG_ENDIAN) || defined(NEEDS_RISC_ALIGNMENT)
		UINT32 val = bytes[3]; val <<= 8; val |= bytes[2]; val <<= 8; val|=bytes[1]; val <<= 8; val|=bytes[0]; return val;
#else
		return *reinterpret_cast<UINT32 *>(bytes);
#endif
	};

	/** Read a little-endian 16-bit value. */
	UINT16 BytesToUInt16(UINT8 * bytes)
	{
#if defined(OPERA_BIG_ENDIAN) || defined(NEEDS_RISC_ALIGNMENT)
		UINT16 val = bytes[1]; val <<= 8; val |= bytes[0]; return val;
#else
		return *reinterpret_cast<UINT16 *>(bytes);
#endif
	};

#ifdef UTIL_ZIP_WRITE
	/** Write a little-endian 32-bit value. */
	void UInt32ToBytes(UINT32 val, UINT8 *bytes)
	{
#if defined(NEEDS_RISC_ALIGNMENT) || defined(OPERA_BIG_ENDIAN)
		bytes[0] = val; val >>= 8; bytes[1] = val; val >>= 8; bytes[2] = val; val >>= 8; bytes[3] = val;
#else
		*reinterpret_cast<UINT32 *>(bytes) = val;
#endif
	};

	/** Write a little-endian 16-bit value. */
	void UInt16ToBytes(UINT16 val, UINT8 *bytes)
	{
#if defined(NEEDS_RISC_ALIGNMENT) || defined(OPERA_BIG_ENDIAN)
		bytes[0] = val; val >>= 8; bytes[1] = val;
#else
		*reinterpret_cast<UINT16 *>(bytes) = val;
#endif
	};
#endif // UTIL_ZIP_WRITE

	static int compareindexnames(const void *arg1, const void *arg2);
	static int compareindexnames_casesensitive(const void *arg1, const void *arg2);

	OP_STATUS ReadUINT32(UINT32& ret);

#ifdef UTIL_ZIP_WRITE
	/**
	* Compress buf and return the content.
	*
	* @param buf pointer of uncompressed content
	* @size count of bytes buf contained
	* @level level of compression, Z_NO_COMPRESSION, Z_BEST_COMPRESSION etc.
	* @out return the compressed buffer, NULL if failure or level is Z_NO_COMPRESSION, caller is responsible to release it
	* @out_size size of content in out, in byte.
	**/
	static OP_STATUS Compress(const char* buf,UINT32 uncomp_size,INT32 comp_level,char*& out,OpFileLength& out_size);
#endif // UTIL_ZIP_WRITE

	public:
		OpZip(int flags=OPFILE_FLAGS_NONE);				///< Flags of type OpFileAdditionalFlags

		~OpZip();

	/**
	 * This method is deprecated. Instead use OpZip::Open() and
	 * specify whether or not to allow writing the specified file.
	 */
	DEPRECATED(OP_STATUS Init(OpString* zfilename));

	DEPRECATED(OP_STATUS Open(const OpString* filename, BOOL write_perm = TRUE));

	/**
	 * \brief Opens or creates a zipfile.
	 *
	 * Opens a zipfile by opening it as an ordinary file and then
	 * checking if it is a zipfile by looking at headers. Then it
	 * reads in all information about the files inside.
	 *
	 * If the file does not exist and write_perm is TRUE then an empty
	 * zipfile is created with the specified filename.
	 *
	 * @param filename the name of the zipfile to open.
	 * @param write_perm permission to write to file system.
	 *
	 * @retval OpStatus::OK if everything went ok
	 * @retval OpStatus::ERR_NO_MEMORY when initialization failed because
	 *         of OOM situation
	 * @retval OpStatus::ERR when file was not a correct zipfile
	 *         or file lock prevented opening it
	 */
	OP_STATUS Open(const OpStringC& filename, BOOL write_perm = FALSE);

		/**
		 * Parses a ZIP file to get the offset of the End of Central
		 * Directory record.
		 *
		 * @param[out] location Set to the offset of the End of Central
		 * Directory record, if found, counted from the beginning of
		 * the file.
		 *
		 * @retval OpStatus::OK If the record was found.
		 * @retval OpStatus::ERR If the record was not found.
		 * @retval other Other errors as reported by OpFile.
	     */
		OP_STATUS LocateEndOfCentralDirectory(OpFileLength* location);

		/**
		 * Parse a file to see if it is a ZIP archive.
		 *
		 * If we can parse the whole file and find its central directory,
		 * it's unequivocally a ZIP archive. If parsing finds a valid ZIP
		 * file entry with general purpose flag bit 3 set (possibly after
		 * several entries with this flag clear), we cannot parse subsequent
		 * entries (due to necessary information being recorded only after
		 * the compressed data), but it's plausibly a ZIP archive.
		 * Otherwise, the file is rejected.
		 *
		 * @param[out] is_zip
		 * - Set to TRUE if the file is unequivocally a ZIP archive (see
		 *   above); in this case, the file's read position is at the end of
		 *   the central directory on return.
		 * - Otherwise, if plausibly a ZIP archive, set false and the file's
		 *   read position is undefined.
		 * - Undefined (along with file read position) on failure.
		 *
		 * @retval OpStatus::OK if the file is plausibly or unequivocally a
		 *   ZIP archive (see above).
		 * @retval OpStatus::ERR_NOT_SUPPORTED if built with HAVE_UINT64
		 *   unset and parsing needed to read a data field that overflows
		 *   UINT32.
		 * @retval OpStatus::ERR if the file is not a ZIP file.
		 * @retval other Other errors (possibly overlapping those above) as
		 *   reported by OpFile.
	     */
    	OP_STATUS IsZipFile(BOOL* is_zip);

		void Close();

		BOOL IsOpen() const
		{
			return m_thisfile && m_thisfile->IsOpen();
		}

		/**
		 * Extract a list of the files and directories in the archive.
		 *
		 * @param list An vector of OpString objects, one per file,
		 * allocated on the heap. It is the callers responsability to
		 * free them.
		 *
		 * @return status
		 */
		OP_STATUS GetFileNameList(OpVector<OpString>& list);

		/**
		 * Check if 'name' is an "implicit" directory in the archive,
		 * that is, if it appears as a parent directory of some other
		 * entry in the archive, but not as a directory entry of its
		 * own.
		 *
		 * @param result Set to TRUE or FALSE on success.
		 * @param name Directory name to look for.
		 */
		OP_STATUS IsImplicitDirectory(BOOL& result, const OpStringC& name);

#ifdef UTIL_ZIP_WRITE

		/** Save changes in memory back to disk.
		 *
		 * This function updates zip file based on the memory file (syncs zip
		 * based on mem file).  Stored file will be compressed using given
		 * compression level.  Modified file can be stored or compressed before
		 * update.  Data is compressed using deflate method or stored.
		 *
		 * This function updates data region, modifies offset values in central
		 * directory entries and end of central directory, compressed,
		 * uncompressed lengths and CRC32 values in both central directory
		 * entries and local headers.
		 *
		 * Output file within archive will have length of nDataSize.
		 *
		 * @param mem_file A memory file containing data used to update current archive
		 * @param data_size Length of data to be updated. If not specified (or
		 *        0), whole pMemFile will be saved.
		 * @param comp_level Compression level to be used (defined in zlib.h)
		 *        will be saved.
		 * @return See OpStatus.
		 */
		OP_STATUS UpdateArchive(OpMemFile *mem_file, uint32 data_size = 0, int comp_level = Z_DEFAULT_COMPRESSION);

		/**
		 * This function updates offset values in central directory entries and end of central
		 * directory (both stored file and memory structures are udpated).
		 *
		 * @param from_entry Index of entry that is the first one to be modified (all following
		 *        entries will also be modified
		 * @param offset Difference between current offset values and previous ones
		 *
		 * @return status
		 */
		OP_STATUS UpdateOffsets(int from_entry, int offset);

		/** Update CRC32 value for an archive member.
		 *
		 * This function updates CRC32 values in central directory entries and
		 * local headers (both stored file and memory structures are udpated).
		 *
		 * @param entry_index Index of entry that is to be modified
		 * @param crc_val New CRC32 value
		 * @return See OpStatus.
		 */
		OP_STATUS UpdateCRC32(int entry_index, UINT32 crc_val);

		/**
		 * This function updates compression info values in central directory entries and local
		 * headers (both stored file and memory structures are udpated).
		 *
		 * @param entry_index Index of entry that is to be modified
		 * @param compression_method New compression method
		 * @param extra_flag Bits to be set in general flags byte
		 *
		 * @return status
		 */
		OP_STATUS UpdateCompressionInfo(int entry_index, UINT16 compression_method, UINT16 extra_flag);

		/**
		 * This function updates compressed and uncompressed length values in central directory
		 * entries and local headers (both stored file and memory structures are udpated).
		 *
		 * @param entry_index Index of entry that is to be modified
		 * @param comp_len Compressed size
		 * @param uncomp_len Uncompressed size
		 *
		 * @return status
		 */
		OP_STATUS UpdateLength(int entry_index, UINT32 comp_len, UINT32 uncomp_len);

		BOOL m_read_only;
#endif // UTIL_ZIP_WRITE

#ifdef UTIL_ZIP_WRITE
		/** Add a file to the archive, not support directory currently. file will be added to root directory of zip file.
		* OpZip::Close() may have to be invoked before all the contents be flushed to disk.
		*
		* @return status
		*/
		OP_STATUS AddFileToArchive(OpFile* newfile);
#endif // UTIL_ZIP_WRITE

		// ZIP File API

		/**
		 * Get index in zipfile of a file with a certain filename.
		 *
		 * @param pstrName the filename to lookup
		 * @param file not used
		 *
		 * @return The index or -1 if file was not found.
		 */
		int GetFileIndex(OpString* pstrName, OpZipFile* file);

		/**
		 * Get file at index
		 *
		 * @param index of file to retrieve
		 * @param file OpZipFile to work on

		*/
		OP_STATUS GetFile(int index, OpZipFile* file);

		/**
		 * Get filedata from name
		 *
		 * @param pstrName Filename.
		 * @param data Pointer to databuffer.  NOTE: Caller must free this
		 * @return long length of databuffer, -1 on error.

		*/

		long GetFile(OpString* pstrName, unsigned char*& data);

		/**
		 * Get filehandle from name
		 *
		 * @param pstrName filename
		 * @return OpFile handle. NOTE: Temporary file, deleted when handle is closed!
		 */
		OpFile* GetOpZipFileHandleL(OpString* pstrName);

#ifndef HAVE_NO_OPMEMFILE
		/**
		 * Get filehandle from name
		 *
		 * @param pstrName Filename of entry in zip.
		 * @param res Optional pointer in which to store an OpStatus indicating
		 *  what went wrong when return is NULL; if NULL (the default), caller
		 *  can't tell the difference between OOM and ERR.
		 * @param index Optional pointer in which to store the zipfile's index
		 *  for the given name (as returned by GetFileIndex).
		 * @return OpMemFile handle.
		 */
		OpMemFile* CreateOpZipFile(OpString* pstrName, OP_STATUS *res=NULL, int *index=NULL);
#endif

		/**
		 * Extracts a file from an archive to a specified filename
		 *
		 * @param pstrZName Name of file in archive
		 * @param filename File of extracted file
		 * @return status, OpStatus::OK indicates success
		 */
		OP_STATUS Extract(OpString* pstrZName, OpString* filename);

		OP_STATUS ExtractTo(OpString* outlocation);

		static BOOL IsFileZipCompatible(OpFile* filetocheck);

#ifdef UTIL_ZIP_CACHE
		OP_STATUS IncUsageCount();
		OP_STATUS DecUsageCount();
		UINT32 GetUsageCount();

		void SetOpFileFlags(int flags);
#endif // UTIL_ZIP_CACHE

	/**
	 * Get file attributes for the file with a certain index.
	 *
	 * @param index for the file for which attributes are requested.
	 * @param attr the requested attributes.
	 */
    void GetFileAttributes(int index, OpZip::file_attributes *attr);
};

/* deprecated, use OpZip::Open() instead */
inline OP_STATUS OpZip::Init(OpString* zfilename) {
	return OpZip::Open(zfilename);
}

/* deprecated, use the other Open() instead */
inline OP_STATUS OpZip::Open(const OpString* filename, BOOL write_perm) {
	return Open(*filename, write_perm);
}

#ifdef ZIPFILE_DIRECT_ACCESS_SUPPORT

#ifdef DIRECTORY_SEARCH_SUPPORT
class OpFolderLister;

class OpZipFolderLister : public OpFolderLister
{
public:

	OpZipFolderLister();
	virtual ~OpZipFolderLister();

	static	OP_STATUS Create(OpFolderLister** new_lister);

	virtual OP_STATUS Construct(const uni_char* path, const uni_char* pattern);

	virtual BOOL Next();

	virtual const uni_char* GetFileName() const;

	virtual const uni_char* GetFullPath() const;

	virtual OpFileLength GetFileLength() const { /*Implement if needed*/ OP_ASSERT(FALSE); return 0; }

	virtual BOOL IsFolder() const;

private:

	OpZip		*m_zip;					// this is the file we list
	OpString			m_current_file;
	OpString m_zip_fullpath;
	OpString m_path_inside;
	OpString			m_current_fullpath;
	UINT32				m_current_index;
	OpAutoVector<OpString>	m_filenames;
};
#endif // DIRECTORY_SEARCH_SUPPORT

/**
 * Access zip-archives directly through the filename.
 *
 * Example:
 * "c:\www\archive.zip\index.html" will give you index.html
 *
 * Observe that a filename must be specified inside the
 * zipfile. Trying "file.zip/" will generate an error.
 */
class OpZipFolder : public OpLowLevelFile
{
#ifdef DIRECTORY_SEARCH_SUPPORT
	friend class OpZipFolderLister;
#endif // DIRECTORY_SEARCH_SUPPORT
public:
	virtual ~OpZipFolder();

	/**
	 * Split the given path into a leading path (denoting the path to a zip
	 * archive that exists on disk), and a trailing path (denoting a
	 * relative path within the zip archive).
	 * Next, open the zip archive (leading path) as a folder and extract
	 * the specified file (trailing path) to memory. This memory area is
	 * then accessed through an OpMemFile.
	 *
	 * @param[out] The created OpZipFolder object is stored here.
	 * @param path Full path to file withing a zip file.
	 * @param flags OpFile::Construct()-type flags (OpFileAdditionalFlags)
	 * @return OpStatus::OK if OK, OpStatus::ERR if zipfile not found
	 * or file not found in zipfile. OpStatus::ERR_NO_MEMORY on OOM.
	 */
	static OP_STATUS Create(OpZipFolder **folder, const uni_char *path, int flags = OPFILE_FLAGS_NONE);

	virtual const uni_char* GetFullPath() const { return m_fullpath.CStr(); }

#ifdef UTIL_ZIP_WRITE
	virtual BOOL IsWritable() const { return !(m_zip_archive->m_read_only); }
#else
	virtual BOOL IsWritable() const { return FALSE; }
#endif//UTIL_ZIP_WRITE

	virtual OP_STATUS Open(int mode);

	virtual BOOL IsOpen() const { return m_data_file && m_data_file->IsOpen(); }

	virtual OP_STATUS MakeDirectory() { return OpStatus::ERR; }

	virtual OP_STATUS Close();

	virtual BOOL Eof() const;

	virtual OP_STATUS Exists(BOOL* exists) const;

	virtual OP_STATUS GetFilePos(OpFileLength* pos) const;

	virtual OP_STATUS SetFilePos(OpFileLength pos, OpSeekMode seek_mode=SEEK_FROM_START);

	virtual OP_STATUS GetFileLength(OpFileLength* len) const;

	virtual OP_STATUS Read(void* data, OpFileLength len, OpFileLength* bytes_read);

	/**
	 * This function reads a line from the file opened. It allocates
	 * a fixed size buffer and sets *data to point to it. If any error
	 * occurs this buffer is freed and *data is set to 0. When
	 * everything is OK it is the callers responsabilty to free the
	 * allocated buffer.
	 *
	 * @param data is used for passing the allocated buffer.
	 * @return OpStatus
	 */
	virtual OP_STATUS ReadLine(char** data);

	virtual OpLowLevelFile* CreateTempFile(const uni_char* prefix) { OP_ASSERT(FALSE); return NULL; }

	virtual OP_STATUS SafeReplace(OpLowLevelFile* new_file) { OP_ASSERT(FALSE); return OpStatus::OK; }

	static BOOL MaybeZipArchive(const uni_char* path);

	// Living in limbo

	virtual OP_STATUS GetFileInfo(OpFileInfo* info);

#ifdef SUPPORT_OPFILEINFO_CHANGE
	virtual OP_STATUS ChangeFileInfo(const OpFileInfoChange* changes) { return OpStatus::ERR; }
#endif // SUPPORT_OPFILEINFO_CHANGE

	virtual const uni_char* GetSerializedName() const { /*Implement if needed*/ OP_ASSERT(FALSE); return NULL; }

	virtual OP_STATUS GetLocalizedPath(OpString *localized_path) const { return OpStatus::ERR; };

	virtual OP_STATUS SetFileLength(OpFileLength len) { /*Implement if needed*/ OP_ASSERT(FALSE); return OpStatus::OK; }

#ifndef UTIL_ZIP_WRITE
	virtual OP_STATUS Write(const void* data, OpFileLength len) { /*Implement if needed*/ OP_ASSERT(FALSE); return OpStatus::ERR; }
#else
	virtual OP_STATUS Write(const void* data, OpFileLength len);
#endif // UTIL_ZIP_WRITE

	virtual OP_STATUS SafeClose() { /*Implement if needed*/ OP_ASSERT(FALSE); return OpStatus::OK; }

	virtual OP_STATUS Delete() { /*Implement if needed*/ OP_ASSERT(FALSE); return OpStatus::OK; }

	virtual OP_STATUS Flush() { /*Implement if needed*/ OP_ASSERT(FALSE); return OpStatus::OK; }

	virtual OP_STATUS CopyContents(const OpLowLevelFile* copy) { /*Implement if needed*/ OP_ASSERT(FALSE); return OpStatus::ERR; }

	virtual OpLowLevelFile* CreateCopy();

#ifdef PI_ASYNC_FILE_OP
	// TODO: implementation !
	virtual void SetListener(OpLowLevelFileListener* listener) { OP_ASSERT(!"Not implemented"); }
	virtual OP_STATUS ReadAsync(void* data, OpFileLength len, OpFileLength abs_pos = FILE_LENGTH_NONE) { OP_ASSERT(!"Not implemented"); return OpStatus::ERR; }
	virtual OP_STATUS WriteAsync(const void* data, OpFileLength len, OpFileLength abs_pos = FILE_LENGTH_NONE) { OP_ASSERT(!"Not implemented"); return OpStatus::ERR; }
	virtual OP_STATUS DeleteAsync() { OP_ASSERT(!"Not implemented"); return OpStatus::ERR; }
	virtual OP_STATUS FlushAsync() { OP_ASSERT(!"Not implemented"); return OpStatus::ERR; }
	virtual OP_STATUS Sync() { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual BOOL IsAsyncInProgress() { return FALSE; }
#endif // PI_ASYNC_FILE_OP

#ifdef DIRECTORY_SEARCH_SUPPORT
	static OpFolderLister* GetFolderLister(OpFileFolder folder, const uni_char* pattern, const uni_char* path=NULL);
#endif //DIRECTORY_SEARCH_SUPPORT

protected:
	OpZipFolder();

	OpZip*			m_zip_archive;

private:
	/**
	 * Find the leading part of the given path that exists on disk.
	 *
	 * Start with the full path and remove path components until an
	 * existing path is found. Set exist_len to the index following that
	 * path component.
	 * Example: Given the path "/foo/bar/baz.zip/xyzzy/blah", where only
	 * "/foo/bar/baz.zip" actually exists on disk, set exist_len to the
	 * index of the '/' following "baz.zip", and return OpStatus::OK.
	 *
	 * @param path The full path to be examined.
	 * @param path_len Length of above path.
	 * @param[out] exist_len Initial length of path that exists on disk.
	 * @returns OpStatus::OK on success, ERR_NULL_POINTER if path is NULL.
	 *          Otherwise whatever error is reported by OpLowLevelFile's
	 *          ::Create() or ::Exists() methods.
	 */
	static OP_STATUS FindLeadingPath(const uni_char *path, size_t path_len, size_t& exist_len);

	/**
	 * Check if 'dir' is a directory that exist in the zipfile but
	 * not having an own entry.
	 * @param is_dir FALSE if 'dir' do not exist at all. Otherwise TRUE.
	 */
	OP_STATUS CheckIfDirectory(const OpStringC &dir, BOOL *is_dir);

	OpString		m_fullpath;		///< Path to file inside zip archive

	/* This is NULL when we do not refer to a valid file */
	OpMemFile*		m_data_file;

#ifdef UTIL_ZIP_CACHE
	BOOL m_own_zip_archive;
#endif // UTIL_ZIP_CACHE
	void CleanIndex();

	/* Refer to a file index within m_zip_archive. However, the special
	 * value -1 signifies a we refer to no file in particular inside the
	 * zip archive. This is similar to an OpLowLevelFile that refers to a
	 * non-existing file. In this case, m_data_file shall be NULL.
	 *
	 * Another special case is when m_current_file == 0, but m_data_file
	 * is still NULL. In this case, we refer to an "implicit" folder
	 * within the zip file (i.e. a folder which exists as part of some
	 * other entry's path name, but that does not exist in its own right).
	 *
	 */
	int m_current_file;
};

#ifdef UTIL_ZIP_CACHE

/**
 * ZipCache contains functions for caching OpZip objects when using
 * OpZipFolder. Is used only by the function Create in OpZipFolder.
 */
class ZipCache : public OpTimerListener
{
public:
	ZipCache();
	/**
	 * Removes all entries in the cache.
	 */
	virtual ~ZipCache();

	/**
	 * See if zipfile with path key is cached and get the
	 * corresponding OpZip object.
	 */
	OP_STATUS GetData(const uni_char *key, OpZip **zip);

	/**
	 * Same as GetData(), except allow 'path' to refer to a path _inside_
	 * the zip file. In this case, the actual archive path (aka. the 'key')
	 * must be a prefix of the given path. The archive path is returned by
	 * setting the 'key_len' parameter (which indicates the end index of
	 * the archive path prefix in 'path'). 'zip' and return value is as
	 * GetData() above.
	 */
	OP_STATUS SearchData(const uni_char *path, size_t& key_len, OpZip **zip);

	/**
	 * Add a new OpZip object to the cache with path key.
	 */
	OP_STATUS Add(const uni_char *key, OpZip *zip);

	/**
	 * Called when a certain OpZip object should be removed from the
	 * cache if it is not still referenced.
	 */
	virtual void OnTimeOut(OpTimer *timer);

	/**
	 * Called by OpZipFolder when its OpZip object is no longer
	 * referenced to indicate to the cache that it may be removed.
	 * The cache then restarts the timer for this particular OpZip
	 * object.
	 */
	void IsRemovable(OpZip *zip);

	/**
	 * Remove all entries in the cache which are not used by any
	 * OpZipFolder object.
	 */
	void FlushUnused();

	/**
	 * Removes specific entry if the item is unused
	 */
	void FlushIfUnused(OpZip *zip);

	/**
	 * Checks if the specified file is a vaild Zip file.
	 * If possible checks cached version first and otherwise
	 * creates and caches opened OpZip object.
	 * @param path - Path to a file we want to check.
	 * @param zip - optional. if set to anything else than NULL
	 * @param path path to a file we want to check
	 * @param zip optional. if set to anything else than NULL
	 * and the file is a valid zip then this will be set to OpZip
	 * object bound to it. This object is still owned by ZipCache
	 * and should not be deleted by the caller. If the caller wants to
	 * store the pointer to this object then he should use OpZip
	 * reference counting functionality(ie. use OpZip::IncUsageCount).
	 * @return - TRUE if the file has been successfully recognized as zip file.
	 * reference counting functionality(ie. use OpZip::IncUsageCount)
	 * @return TRUE if the file has been successfully recognized as zip file.
	 */
	BOOL IsZipFile(const uni_char* path, OpZip** zip = NULL);

#ifdef SELFTEST
	enum LookupMethod
	{
		LOOKUP_DEFAULT,
		LOOKUP_LINEAR,
		LOOKUP_HASH
	};

	void SetLookupMethod(LookupMethod lookup_method) { m_lookup_method = lookup_method; };
#endif // SELFTEST
private:
	struct CacheElm : public Link
	{
		CacheElm() : timer(NULL), key(NULL), zip(NULL) {}
		~CacheElm()
		{
			op_free(key);
			timer->Stop();
			OP_DELETE(timer);
			OP_DELETE(zip);
		}
		OpTimer*  timer; ///< Allocated by OP_NEW freed by OP_DELETE
		uni_char* key;   ///< Allocated by op_alloc freed by op_free.
		OpZip*    zip;   ///< Allocated by OP_NEW freed by OP_DELETE
	};

	Head m_cache;
	OpStringHashTable<CacheElm> m_zip_files;
	TempBuffer search_path_buffer;

#ifdef SELFTEST
	LookupMethod m_lookup_method;
#endif // SELFTEST
};
#endif // UTIL_ZIP_CACHE

#endif //ZIPFILE_DIRECT_ACCESS_SUPPORT

#endif // USE_ZLIB
#endif // !MODULES_UTIL_ZIPLOAD_H
