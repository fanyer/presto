/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_OPFILE_OPFILE_H
#define MODULES_UTIL_OPFILE_OPFILE_H

#include "modules/pi/system/OpFolderLister.h"
#include "modules/pi/system/OpLowLevelFile.h"
#include "modules/util/opfile/opfolder.h"

enum OpFileType
{
	OPFILE,
#ifndef HAVE_NO_OPMEMFILE
	OPMEMFILE,
#endif // !HAVE_NO_OPMEMFILE
	OPSAFEFILE
};

class OpFileDescriptor
{
public:
	OpFileDescriptor() {}
	virtual ~OpFileDescriptor() {}

	/**
	 * Retrieves the type.
	 *
	 * @return OPFILE, OPMEMFILE or OPSAFEFILE.
	 */
	virtual OpFileType Type() const = 0;

	// The following methods are equivalent to those in OpLowLevelFile.
	// See modules/pi/system/OpLowLevelFile.h for further documentation.

	virtual BOOL IsWritable() const = 0;
	virtual OP_STATUS Open(int mode) = 0;
	virtual BOOL IsOpen() const = 0;
	virtual OP_STATUS Close() = 0;
	virtual BOOL Eof() const = 0;
	virtual OP_STATUS Exists(BOOL& exists) const = 0;
	virtual OP_STATUS GetFilePos(OpFileLength& pos) const = 0;
	virtual OP_STATUS SetFilePos(OpFileLength pos, OpSeekMode seek_mode=SEEK_FROM_START) = 0;
	virtual OP_STATUS GetFileLength(OpFileLength& len) const = 0;
	virtual OP_STATUS Write(const void* data, OpFileLength len) = 0;
	virtual OP_STATUS Read(void* data, OpFileLength len, OpFileLength* bytes_read) = 0;
	virtual OP_STATUS ReadLine(OpString8& str) = 0;
	virtual OpFileDescriptor* CreateCopy() const = 0;

	// The following functionality is not in OpLowLevelFile

	/**
	 * Takes a string, converts it to UTF8, appends a new line char
	 * and writes it to the file.
	 *
	 * @param data The string to convert and write.
	 * @return OK, ERR_NO_MEMORY or ERR.
	 */
	OP_STATUS WriteUTF8Line(const OpStringC& data);

	/**
	 * Reads a line stored as UTF8, converted back to unicode.
	 * The OpString needs to be checked after the call to see
	 * how much was read. If nothing was read the string will
	 * be empty.  The line terminating character(s) new line
	 * and carrige return are not included in the returned string.
	 *
	 * @param str Where to store the data being read.
	 * @return OK, ERR_NO_MEMORY or ERR.
	 */
	OP_STATUS ReadUTF8Line(OpString& str);

	/**
	 * Writes a short in its string representation
	 * with an new line char added (ex. 17 => "17\n")
	 *
	 * @param data Number to store.
	 * @return OK, ERR_NO_MEMORY or ERR.
	 */
	OP_STATUS WriteShort(short data);

	/**
	 * Reads a short on its own line.
	 *
	 * @param data Where to store the read number.
	 * @return OK, ERR_NO_MEMORY or ERR.
	 */
	OP_STATUS ReadShort(short& data);

	/**
	 * Writes a byte binary (not string representation).
	 *
	 * @param data Data to store.
	 * @return OK, ERR_NO_MEMORY or ERR.
	 */
	OP_STATUS WriteBinByte(char data);

	/**
	 * Writes a short binary (not string representation).
	 *
	 * @param data Data to store.
	 * @return OK, ERR_NO_MEMORY or ERR.
	 */
	OP_STATUS WriteBinShort(short data);

	/**
	 * Writes a short binary in L-H format (not string representation).
	 *
	 * @param data Data to store.
	 * @return OK, ERR_NO_MEMORY or ERR.
	 */
	OP_STATUS WriteBinShortLH(short data);

	/**
	 * Writes a long binary (not string representation).
	 *
	 * @param data Data to store.
	 * @return OK, ERR_NO_MEMORY or ERR.
	 */
	OP_STATUS WriteBinLong(long data);

	/**
	 * Writes a long binary in L-H format (not string representation).
	 *
	 * @param data Data to store.
	 * @return OK, ERR_NO_MEMORY or ERR.
	 */
	OP_STATUS WriteBinLongLH(long data);

	// The following methods are to be compatible with previous content in filefun.h

	/**
	 * Reads a long binary.
	 *
	 * @param value The read number.
	 * @return OK, ERR_NO_MEMORY or ERR.
	 */
	OP_STATUS ReadBinLong(long& value);

	/**
	 * Reads a short binary.
	 *
	 * @param value The read number.
	 * @return OK, ERR_NO_MEMORY or ERR.
	 */
	OP_STATUS ReadBinShort(short& value);

	/**
	 * Reads a byte binary.
	 *
	 * @param value The read number.
	 * @return OK, ERR_NO_MEMORY or ERR.
	 */
	OP_STATUS ReadBinByte(char& value);

	/* Deprecated APIs without status return.
	 * Please use the versions that return OP_STATUS instead.
	 */
	long  DEPRECATED(ReadBinLong());  // inlined below
	short DEPRECATED(ReadBinShort()); // inlined below
	char  DEPRECATED(ReadBinByte());  // inlined below

private:
	/** Block assignment operator. */
	OpFileDescriptor &operator=(const OpFileDescriptor &);
	/** Block copy constructor. */
	OpFileDescriptor(const OpFileDescriptor &);
};

/* gcc 3 but < 3.4 can't cope with deprecated inline, so separate deprecated
 * declaration from inline definition.
 */
inline long  OpFileDescriptor::ReadBinLong()  { long  value; ReadBinLong(value);  return value; }
inline short OpFileDescriptor::ReadBinShort() { short value; ReadBinShort(value); return value; }
inline char  OpFileDescriptor::ReadBinByte()  { char  value; ReadBinByte(value);  return value; }

#ifdef UTIL_ASYNC_FILE_OP
class OpFile;

class OpFileListener
{
public:
	virtual ~OpFileListener() {}
	virtual void OnDataRead(OpFile* file, OP_STATUS result, OpFileLength len) = 0;
	virtual void OnDataWritten(OpFile* file, OP_STATUS result, OpFileLength len) = 0;
	virtual void OnDeleted(OpFile* file, OP_STATUS result) = 0;
	virtual void OnFlushed(OpFile* file, OP_STATUS result) = 0;
};
#endif // UTIL_ASYNC_FILE_OP

/* @short A bitmask specifying additional flags that can be passed to OpFile::Construct */
enum OpFileAdditionalFlags
{
	OPFILE_FLAGS_NONE				= 0x00,	///< Default, no additional flags
#ifdef ZIPFILE_DIRECT_ACCESS_SUPPORT
	OPFILE_FLAGS_ASSUME_ZIP			= 0x01,	///< Assume that the file could be a zip file and try to do direct zip support magic regardless of extension
	OPFILE_FLAGS_CASE_SENSITIVE_ZIP	= 0x02,	///< If this is a zip file, do case sensitive filename matching inside the archive
#endif // ZIPFILE_DIRECT_ACCESS_SUPPORT
	_opfile_flags_dummy				= 0
};

class OpFile
	: public OpFileDescriptor
#ifdef UTIL_ASYNC_FILE_OP
	, private OpLowLevelFileListener
#endif // UTIL_ASYNC_FILE_OP
{
	friend class OpSafeFile;
public:
	// OpFile

	OpFile()
		: m_file(NULL)
#ifdef UTIL_ASYNC_FILE_OP
		, m_listener(NULL)
#endif // UTIL_ASYNC_FILE_OP
	{
	}
	virtual ~OpFile();

	/**
	 * Initializes the OpFile object with information about where the file can be found.
	 * This function must have been called successfully for any other functions to be called.
	 *
	 * @param path Full path or filename. Depends on the folder param.
	 * @param folder A named folder or a special folder type (OPFILE_ABSOLUTE_FOLDER or OPFILE_SERIALIZED_FOLDER).
	 * @param flags Extra flags, a bitwise-OR combination from the OpFileAdditionalFlags enum.
	 */
	virtual OP_STATUS Construct(const OpStringC& path, OpFileFolder folder=OPFILE_ABSOLUTE_FOLDER, int flags=OPFILE_FLAGS_NONE);


#ifdef CRYPTO_ENCRYPTED_FILE_SUPPORT  // Turn on by importing API_CRYPTO_ENCRYPTED_FILE from the crypto module
	/**
	 * Initializes an encrypted OpFile object with information about the key and where the file can be found.
	 * This function must have been called successfully for any other functions to be called.
	 * 
	 * The underlying encryption algorithm is AES in CFB encryption mode.
	 * 
	 * Note: An encrypted OpFile behaves mainly the same way as an normal OpFile, apart from a few differences: 
	 * 
	 * - When opening an encrypted file with a wrong key, OpFile::Open(mode) will return OpStatus::ERR_NO_ACCESS.
	 * 
	 * - It is not possible to use SetFilePos and write in the middle of an encrypted file. However, SetFilePos
	 * 	 can be used to read anywhere in a file.
	 *
	 * - ReadLine is currently not supported for encrypted files.
	 *
	 * @param path Full path or filename. Depends on the folder param.
	 * @param key  The encryption key
	 * @param key_length the length of the key in bytes. Supported key lengths are 16, 24 and 32 (128, 192 or 256 bits)
	 * @param folder A named folder or a special folder type (OPFILE_ABSOLUTE_FOLDER or OPFILE_SERIALIZED_FOLDER).
	 * 
	 * @return OK, ERR_NO_MEMORY or ERR if folder/path does not exist.  
	 */

	OP_STATUS ConstructEncrypted(const uni_char* path, const byte *key, unsigned int key_length = 16, OpFileFolder folder=OPFILE_ABSOLUTE_FOLDER);
#endif	

	// The following methods are equivalent to those in OpLowLevelFile.
	// See modules/pi/system/OpLowLevelFile.h for further documentation.

	const uni_char* GetFullPath() const;
	const uni_char* GetSerializedName() const;
	OP_STATUS GetLocalizedPath(OpString *localized_path) const;
	OP_STATUS MakeDirectory();

	/**
	 * Returns the file name part of the full path.
	 */
	const uni_char* GetName() const;

	/**
	 * Returns the directory name part of the full path.
	 */
	OP_STATUS GetDirectory(OpString &directory);

	// OpFileDescriptor implementation. For documentation see OpFileDescriptor above.

	virtual OpFileType Type() const;

	virtual BOOL IsWritable() const;

	virtual OP_STATUS Open(int mode);
	virtual BOOL IsOpen() const;
	virtual OP_STATUS Close();

	virtual BOOL Eof() const;
	virtual OP_STATUS Exists(BOOL& exists) const;
	/**
	 * Convenience wrapper for OpFile::Exists(BOOL&)
	 */
	virtual BOOL ExistsL() const;
	/**
	 * Convenience wrapper for OpFile::Exists(BOOL&)
	 * @param full file name
	 */
	static BOOL ExistsL(const OpStringC& path);

	virtual OP_STATUS GetFilePos(OpFileLength& pos) const;
	virtual OP_STATUS SetFilePos(OpFileLength pos, OpSeekMode seek_mode=SEEK_FROM_START);
	virtual OP_STATUS GetFileLength(OpFileLength& len) const;

	virtual OP_STATUS SetFileLength(OpFileLength len);
 
	virtual OP_STATUS Write(const void* data, OpFileLength len);
	virtual OP_STATUS Read(void* data, OpFileLength len, OpFileLength* bytes_read);
	virtual OP_STATUS ReadLine(OpString8& str);

	virtual OpFileDescriptor* CreateCopy() const;

	virtual OP_STATUS SafeClose();
	virtual OP_STATUS SafeReplace(OpFile* source);

	// OpFile additions

	/**
	 * Changes this file to be a copy of another file.
	 *
	 * @param copy The file to copy.
	 * @return OK, ERR_NO_MEMORY or ERR.
	 */
	OP_STATUS Copy(const OpFile* copy);

	/**
	 * Deletes the file or folder.
	 *
	 * @param recursive If TRUE and if the file points to
	 * a folder it will be deleted along with all its content.
	 * @return OK, ERR_NO_MEMORY or ERR.
	 */
	OP_STATUS Delete(BOOL recursive = TRUE);

	/**
	 * Deletes the folder.
	 *
	 * @param recursive If TRUE all content will be deleted.
	 * @return OK, ERR_NO_MEMORY or ERR.
	 */
	OP_STATUS DeleteFolder(BOOL recursive);

	/**
	 * Copies the contents of a file, on disk.
	 *
	 * See documentation of OpLowLevelFile::CopyContents() for more details.
	 *
	 * @param file File to copy contents from.
	 * @param fail_if_exists Will return ERR if this file exists.
	 * @return See OpLowLevelFile::CopyContents() for return values.
	 */
	OP_STATUS CopyContents(OpFile* file, BOOL fail_if_exists);

	/**
	 * Retrieves the date for last modification.
	 * @param value When the file was modified. Seconds since 1970-01-01 00:00 GMT.
	 * @return OK, ERR_NO_MEMORY or ERR.
	 */
	OP_STATUS GetLastModified(time_t& value) const;

	/**
	 * Retrieves the type for this file.
	 * @return Mode. File, directory or symbolic link.
	 */
	OP_STATUS GetMode(OpFileInfo::Mode &mode) const;

	/**
	 * Forces a write of all buffered data for the file The open status
	 * of the file is unaffected.
	 *
	 * See OpLowLevelFile::Flush() documentation for more information.
	 *
	 * @return OK, ERR_NO_MEMORY or ERR.
	 */
	OP_STATUS Flush();

	/**
	 * Prints a formatted string to the file. Works similar to fprintf().
	 *
	 * Note: There is currently a limitation in that at most 2048 characters
	 *       can be printed this way in each call.
	 *
	 * @param format Format string, followed by arguments.
	 * @return OK, ERR_NO_MEMORY or ERR.
	 */
	OP_STATUS Print(const char* format, ...);

	// Folder listing support

#ifdef DIRECTORY_SEARCH_SUPPORT
	/**
	 * Creates a OpFolderLister object which can be used for listing
	 * the files and folders in a folder. The returned OpFolderLister
	 * is to be freed by the caller.
	 *
	 * @param folder Folder to list.
	 * @param pattern Pattern to match for the items in the folder. "*.win" for example.
	 * @param path Should only be used if folder is OPFILE_ABSOLUTE_FOLDER.
	 * @return A new OpFolderLister object that must be freed by the caller or NULL on OOM.
	 */
	static OpFolderLister* GetFolderLister(OpFileFolder folder, const uni_char* pattern, const uni_char* path=NULL);
	static OpFolderLister* GetFolderLister(OpFileFolder folder, const OpStringC &pattern, const OpStringC &path=OpStringC(NULL))
	{
		return GetFolderLister(folder, pattern.CStr(), path.CStr());
	}
#endif // DIRECTORY_SEARCH_SUPPORT

#ifdef UTIL_ASYNC_FILE_OP
	// The following methods implement the async part of the OpLowLevelFile API.
	// See modules/pi/system/OpLowLevelFile.h for further documentation.

	virtual void SetListener (OpFileListener *listener);
	virtual OP_STATUS ReadAsync(void* data, OpFileLength len, OpFileLength abs_pos = FILE_LENGTH_NONE);
	virtual OP_STATUS WriteAsync(const void* data, OpFileLength len, OpFileLength abs_pos = FILE_LENGTH_NONE);
	virtual OP_STATUS DeleteAsync();
	virtual OP_STATUS FlushAsync();
	virtual OP_STATUS Sync();
	virtual BOOL IsAsyncInProgress();
#endif // UTIL_ASYNC_FILE_OP

	// The following methods implement some of the OpLowLevelFile API.
	// See modules/pi/system/OpLowLevelFile.h for further documentation.
	OP_STATUS GetFileInfo(OpFileInfo* info) { return m_file ? m_file->GetFileInfo(info) : OpStatus::ERR; }
#ifdef SUPPORT_OPFILEINFO_CHANGE
	OP_STATUS ChangeFileInfo(const OpFileInfoChange* changes) { return m_file ? m_file->ChangeFileInfo(changes) : OpStatus::ERR; }
#endif // SUPPORT_OPFILEINFO_CHANGE

	/* Deprecated APIs without status return.
	 * Please use the versions that return OP_STATUS instead. */
	virtual OpFileLength DEPRECATED(GetFileLength() const); // inlined below
	virtual OpFileLength DEPRECATED(GetFilePos() const); // inlined below
	time_t DEPRECATED(GetLastModified() const); // inlined below
	OpFileInfo::Mode DEPRECATED(GetMode() const);

private:
	static OP_STATUS ConstructPath(OpString& result, const uni_char* path, OpFileFolder folder);

	OpLowLevelFile* m_file;

	friend class DesktopOpFileUtils;

	/** Block assignment operator. */
	OpFile &operator=(const OpFile &);
	/** Block copy constructor. */
	OpFile(const OpFile &);

#ifdef UTIL_ASYNC_FILE_OP
	// The following methods implement the OpLowLevelFileListener API.
	// See modules/pi/system/OpLowLevelFile.h for further documentation.
	virtual void OnDataRead(OpLowLevelFile* lowlevel_file, OP_STATUS result, OpFileLength len);
	virtual void OnDataWritten(OpLowLevelFile* lowlevel_file, OP_STATUS result, OpFileLength len);
	virtual void OnDeleted(OpLowLevelFile* lowlevel_file, OP_STATUS result);
	virtual void OnFlushed(OpLowLevelFile* lowlevel_file, OP_STATUS result);

	OpFileListener *m_listener;
#endif // UTIL_ASYNC_FILE_OP
};

// cope with gcc 3 but < 3.4, as above
inline OpFileLength OpFile::GetFileLength() const { OpFileLength len; GetFileLength(len); return len; }
inline OpFileLength OpFile::GetFilePos() const { OpFileLength pos; GetFilePos(pos); return pos; }
inline time_t OpFile::GetLastModified() const { time_t value; GetLastModified(value); return value; }

#endif // !MODULES_UTIL_OPFILE_OPFILE_H
