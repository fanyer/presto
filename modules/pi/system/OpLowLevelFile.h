/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OPLOWLEVELFILE_H
#define OPLOWLEVELFILE_H

class OpLowLevelFile;

/** @short File open mode flags. To be used with OpLowLevelFile::Open().
 *
 * There are two categories of flags: operation flags and modifier flags. At
 * least one operation flag must be set. Some flags are compound operation
 * flags.
 *
 * Disallowed combinations of operation flags:
 *                          OPFILE_UPDATE with any other operation flag,
 *                          OPFILE_APPEND with OPFILE_WRITE
 *                          OPFILE_APPEND with OPFILE_OVERWRITE
 *
 * Disallowed combinations of operation flags and modifier flags:
 *                          OPFILE_SHAREDENYREAD with OPFILE_READ
 *
 * Many mode flags correspond to character sequences used in the 'mode'
 * argument to C89 fopen(), in which cases the C89 equivalent is shown in
 * parentheses in this documentation.
 */
enum OpFileOpenMode
{
	/** Open file for reading. (C89: "r")
	 *
	 * Operation flag (read anywhere).
	 *
	 * The file must exist. Initial file position is at the beginning of the
	 * file.
	 */
	OPFILE_READ = 0x1,

	/** Open file for writing. (C89: "w")
	 *
	 * Operation flag (write anywhere).
	 *
	 * The file is created if it does not exist; otherwise it is
	 * truncated. Initial file position is at the beginning of the file.
	 */
	OPFILE_WRITE = 0x2,

	/** Open file for reading and writing. (C89: "w+")
	 *
	 * Compound operation flag (read anywhere, write anywhere).
	 *
	 * The file is truncated or created. Initial file position is at the
	 * beginning of the file.
	 *
	 * This mode is equivalent to OPFILE_READ | OPFILE_WRITE.
	 */
	OPFILE_OVERWRITE = OPFILE_READ | OPFILE_WRITE,

	/** Open file for writing at the end of the file. (C89: "a")
	 *
	 * Operation flag (write at end).
	 *
	 * The file is created if it does not exist. Initial file position is
	 * undefined. All write operations will first move the file position to
	 * EOF. This means that existing data cannot be overwritten.
	 */
	OPFILE_APPEND = 0x4,

	/** Open file for writing at the end of the file. (C89: "a+"), and reading.
	 *
	 * Compound operation flag (read anywhere, write at end).
	 *
	 * The file is created if it does not exist. Initial file position is at
	 * the beginning of the file. All write operations will first move the file
	 * position to EOF. This means that existing data cannot be overwritten.
	 *
	 * This mode is equivalent to OPFILE_READ | OPFILE_APPEND.
	 */
	OPFILE_APPENDREAD = OPFILE_READ | OPFILE_APPEND,

	/** Open file for updating. (C89: "r+")
	 *
	 * Operation flag (read anywhere, write anywhere).
	 *
	 * The file must exist. Initial file position is at the beginning of the
	 * file. The file contents are NOT truncated upon opening.
	 */
	OPFILE_UPDATE = 0x8,

	/** Open text file. (fopen() extension not in C89: "t")
	 *
	 * Modifier flag.
	 *
	 * Default is binary, which is the raw (untranslated) mode. Contents in the
	 * data buffer are identical to what's in the file. For text files, on the
	 * other hand, there may be some sort of conversion involved, to conform
	 * with the text file format on the system (e.g. for Windows, there will be
	 * an end-of-file marker at the end of the file, and a linebreak is
	 * represented as CR+LF in the file, while it's represented as LF in the
	 * data buffer). Some platforms (POSIX systems, for example) do not
	 * distinguish between text and binary files.
	 */
	OPFILE_TEXT = 0x10,

	/** Make sure that contents of the file buffer are written directly to
	 * disk if Flush() is called. (fopen() extension not in C89: "c")
	 *
	 * Modifier flag. If combined with operation flags that only support
	 * reading, this flag has no effect.
	 *
	 * Supporting this is recommended, but optional.
	 */
	OPFILE_COMMIT = 0x20,

	/** Deny other processes read access to the file.
	 *
	 * Modifier flag. It may not be used with read-only operations.
	 *
	 * Supporting this is recommended, but optional. It is also OK to deny
	 * others within the same process read access when specifying this flag.
	 */
	OPFILE_SHAREDENYREAD = 0x40,

	/** Deny other processes write access to the file.
	 *
	 * Modifier flag.
	 *
	 * Supporting this is recommended, but optional. It is also OK to deny
	 * others within the same process write access when specifying this flag.
	 */
	OPFILE_SHAREDENYWRITE = 0x80
};


/** @short File offset reference point definitions.
 *
 * Used with OpLowLevelFile::SetFilePos().
 *
 * The values correspond to values used in the 'whence' argument to C89
 * fseek(), and they are shown in parentheses in this documentation.
 */
enum OpSeekMode
{
	/** Offset is relative to the beginning of the file (C89: SEEK_SET). */
	SEEK_FROM_START,

	/** Offset is relative to the end of the file (C89: SEEK_END). */
	SEEK_FROM_END,

	/** Offset is relative to the current file position (C89: SEEK_CUR). */
	SEEK_FROM_CURRENT
};


/** @short File information.
 *
 * OpLowLevelFile::GetFileInfo() is used to request an object of this type to
 * be set up. The only member of this structure to be set by the API user, is
 * 'flags'. The remaining are set by GetFileInfo().
 */
class OpFileInfo
{
public:
	/** Information request flags.
	 *
	 * The API user sets the 'flags' member to a bit-wise OR
	 * combination of these flags.
	 *
	 * Getting the various pieces of file information may be
	 * expensive. The API user should only specify which particular
	 * properties are interesting, and refrain from reading the
	 * OpFileInfo members corresponding to flags excluded because
	 * their values may be undefined.
	 */
	enum Flag
	{
		/** Get the full absolute path for the file. Member 'full_path' will be set. */
		FULL_PATH = 0x1,

		/** Get the serialized form of the file name. Member 'serialized_name' will be set. */
		SERIALIZED_NAME = 0x2,

		/** Check if the file is writable. Member 'writable' will be set. */
		WRITABLE = 0x4,

		/** Check if the file is open. Member 'open' will be set. */
		OPEN = 0x8,

		/** Get the length of the file. Member 'length' will be set. */
		LENGTH = 0x10,

		/** Get the current file position. Member 'pos' will be set.
		 *
		 * @deprecated Use OpLowLevelFile::GetFilePos() instead.
		 */
		POS = 0x40,

		/** Get the timestamp of the last modification. Member 'last_modified' will be set. */
		LAST_MODIFIED = 0x80,

		/** Get the timestamp of creation time. Member 'creation_time' will be set. */
		CREATION_TIME = 0x100,

		/** Get the file type. Member 'mode' will be set. */
		MODE = 0x200,

		/** Get the hidden attribute of this file system entry. Member 'hidden' will be set. */
		HIDDEN = 0x400
	};

	/** File type. */
	enum Mode
	{
		FILE, ///< Regular file
		DIRECTORY, ///< Directory
		SYMBOLIC_LINK, ///< Symbolic link
		NOT_REGULAR ///< None of the above
	};

	/** Values of the Flag enum ORed together.
	 *
	 * To be set before calling OpLowLevelFile::GetFileInfo().
	 */
	int flags;

	/** The full absolute path for the file.
	 *
	 * The data allocated is owned by the API implementation.
	 *
	 * Equivalent to OpLowLevelFile::GetFullPath().
	 */
	const uni_char* full_path;

	/** The file name in a serialized form.
	 *
	 * The data allocated is owned by the API implementation.
	 *
	 * Equivalent to OpLowLevelFile::GetSerializedName().
	 */
	const uni_char* serialized_name;

	/** TRUE if the file is writable.
	 *
	 * Equivalent to OpLowLevelFile::IsWritable().
	 */
	BOOL writable;

	/** TRUE if the file is open.
	 *
	 * Equivalent to OpLowLevelFile::IsOpen().
	 */
	BOOL open;

	/** TRUE if the file is hidden according to how common file listings appear on the platform, ie. default dir, ls or file explorer behavior.
		The attribute is used by webapps to filter out files and folders that should not be displayed in the UI. Platforms can then make all 
		files available in a folder listing and let display be decided by this attribute.
	*/
	BOOL hidden;

	/** The length of the file.
	 *
	 * Equivalent to OpLowLevelFile::GetFileLength(). If the file doesn't
	 * exist, this value is undefined.
	 */
	OpFileLength length;

	/** @deprecated Use OpLowLevelFile::GetFilePos() instead. */
	OpFileLength pos;

	/** Last modified time. Seconds since 1970-01-01 00:00 GMT */
	time_t last_modified;

	/** Creation time. Seconds since 1970-01-01 00:00 GMT.
	 *
	 * If the system cannot get the creation time, it may use something similar
	 * as a substitute, such as e.g. "last meta-data modification time", which
	 * in many cases coincides with creation time. It shall however NOT get the
	 * "last modified time". If the system cannot get anything resembling
	 * creation time, operations with the CREATION_TIME flag set shall fail
	 * (OpStatus::ERR_NOT_SUPPORTED).
	 *
	 * In any case, if this operation is supported, it will read out the same
	 * value from the file system, that is changeable with
	 * OpFileInfoChange::creation_time.
	 */
	time_t creation_time;

	/** Type of file. File, directory, etc. */
	Mode mode;
};

#ifdef SUPPORT_OPFILEINFO_CHANGE
/** @short File information change.
 *
 * To be used with OpLowLevelFile::ChangeFileInfo().
 */
class OpFileInfoChange
{
public:
	/** Change flags.
	 *
	 * The API user sets the 'flags' member to a combination of these flags.
	 *
	 * The implementation of OpLowLevelFile::ChangeFileInfo() must ignore the
	 * OpFileInfoChange members corresponding to flags not set.
	 */
	enum Flag
	{
		/** Specify if the file is to be writable. Member 'writable' will be used. */
		WRITABLE = 0x4,

		/** Set the time of last modification. Member 'last_modified' will be used. */
		LAST_MODIFIED = 0x80,

		/** Set creation time. Member 'creation_time' will be used. */
		CREATION_TIME = 0x100
	};

	/** Values of the Flag enum ORed together.
	 *
	 * The bits set specify what OpLowLevelFile::ChangeFileInfo() will change.
	 */
	int flags;

	/** TRUE to make the file writable, FALSE to make it read-only.
	 *
	 * Note that some users (administrator, root or similar) can write any file,
	 * regardless of the usual mechanisms of access control (unless the disk is
	 * mounted read-only, of course).  If an attempt to make a file read-only
	 * leaves it writable, by such a user, the implementation shall fail a pi
	 * selftest in SPARTAN runs (see SPARTAN-727) unless it notices this and
	 * reports that making the file read-only (for this user) isn't supported.
	 * Whether this matters in the wild remains open to debate.
	 */
	BOOL writable;

	/** Last modified time. Seconds since 1970-01-01 00:00 GMT */
	time_t last_modified;

	/** Creation time. Seconds since 1970-01-01 00:00 GMT.
	 *
	 * If the system cannot set the creation time, it may set something similar
	 * as a substitute, such as e.g. "last meta-data modification time", which
	 * in many cases coincides with creation time. It shall however NOT change
	 * the "last modified time". If the system cannot set anything resembling
	 * creation time, operations with the CREATION_TIME flag set shall fail
	 * (OpStatus::ERR_NOT_SUPPORTED).
	 *
	 * In any case, if this operation is supported (and thus modifies a
	 * timestamp in the file system), the same value may be read back with
	 * OpFileInfo::creation_time.
	 */
	time_t creation_time;
};
#endif // SUPPORT_OPFILEINFO_CHANGE

#ifdef PI_ASYNC_FILE_OP
/** @short Listener for asynchronous file operations.
 *
 * Any method on the OpLowLevelFile object may be called directly from a
 * listener method implementation, but some will fail if asynchronous file
 * operations are in progress (obviously not counting the one that triggered
 * this listener method call). Methods that say "May be called while
 * asynchronous file operations are in progress" in their documentation are
 * methods that do not affect, or are affected by, asynchronous file
 * operations, and may always be called successfully from a listener
 * method. When in doubt, listener method implementors should check with
 * OpLowLevelFile::IsAsyncInProgress(), and if it returns TRUE, methods that
 * don't announce that they may be called while asynchronous operations are in
 * progress should be avoided. The destructor may always be safely invoked from
 * a listener method.
 */
class OpLowLevelFileListener
{
public:
	virtual ~OpLowLevelFileListener() {}

	/** Asynchronous file read operation completed.
	 *
	 * Triggered by a call to OpLowLevelFile::ReadAsync().
	 *
	 * The data read is put in the buffer that was passed to
	 * OpLowLevelFile::ReadAsync(). If the operation failed ('result' parameter
	 * is not OK), the contents of the buffer is undefined.
	 *
	 * @param file The object on which the asynchronous file operation was
	 * requested.
	 * @param result Result of the operation. See return value of Read() for
	 * details.
	 * @param len Number of bytes read
	 */
	virtual void OnDataRead(OpLowLevelFile* file, OP_STATUS result, OpFileLength len) = 0;

	/** Asynchronous file write operation completed.
	 *
	 * Triggered by a call to OpLowLevelFile::WriteAsync().
	 *
	 * @param file The object on which the asynchronous file operation was
	 * requested.
	 * @param result Result of the operation. See return value of Write() for
	 * details.
	 * @param len Number of bytes written
	 */
	virtual void OnDataWritten(OpLowLevelFile* file, OP_STATUS result, OpFileLength len) = 0;

	/** Asynchronous file delete operation completed.
	 *
	 * Triggered by a call to OpLowLevelFile::DeleteAsync().
	 *
	 * @param file The object on which the asynchronous file operation was
	 * requested.
	 * @param result Result of the operation. See return value of Delete() for
	 * details.
	 */
	virtual void OnDeleted(OpLowLevelFile* file, OP_STATUS result) = 0;

	/** Asynchronous write buffer flush operation completed.
	 *
	 * Triggered by a call to OpLowLevelFile::FlushAsync().
	 *
	 * @param file The object on which the asynchronous file operation was
	 * requested.
	 * @param result Result of the operation. See return value of Flush() for
	 * details.
	 */
	virtual void OnFlushed(OpLowLevelFile* file, OP_STATUS result) = 0;
};
#endif // PI_ASYNC_FILE_OP

/** @short Low-level representation of a file.
 *
 * An instance of this class can perform various operations on a file, such
 * as: open, create, delete, read, write, and get various bits of
 * information. A file is either open or closed. It also either exists or
 * doesn't. An open file exists.
 *
 * The implementation is allowed to buffer write operations, in such a way that
 * calling methods like Open(), Write(), WriteAsync() or SetFileLength() may
 * cause but not detect out of disk errors (OpStatus::ERR_NO_DISK). Instead,
 * such a condition may be detected and reported in a following call to methods
 * like Flush(), Close(), GetFilePos(), SetFilePos(), Write(), etc.
 *
 * READ AND WRITE OPERATIONS ON THE SAME OBJECT:
 *
 * When a file is opened with a mode that allows both reading and writing
 * (OPFILE_UPDATE or OPFILE_APPENDREAD), the API user shall ensure that a
 * write operation is not directly followed by a read operation without an
 * intervening call to Flush() or SetFilePos(), and reading is not directly
 * followed by writing without an intervening call to SetFilePos(), unless the
 * read operation encounters end-of-file.
 *
 * ASYNCHRONOUS FILE OPERATIONS:
 *
 * Asynchronous file operations are supported if PI_ASYNC_FILE_OP is
 * defined (set by importing API_PI_ASYNC_FILE_OP). Methods for
 * asynchronous operations have the "Async" suffix appended to their
 * name. All other methods are synchronous.
 *
 * Only a few of the synchronous methods may be called while asynchronous file
 * operations are in progress. Such methods specify this explicitly in their
 * documentation. Other methods will return OpStatus::ERR when called at such
 * times.
 *
 * Each asynchronous file operation leads to one and only one call to
 * a OpLowLevelFileListener method (but operations not completed when
 * the destructor is invoked will not have their listener method
 * called). Multiple simultaneous asynchronous file operations are
 * allowed. The resulting listener methods are called in the same
 * order as the operations were requested.
 */
class OpLowLevelFile
{
public:
	/** Create an OpLowLevelFile object.
	 *
	 * This does not create or open the file.
	 *
	 * @param[out] new_file Set to the new OpLowLevelFile object. The value must
	 * be ignored unless OpStatus::OK is returned.
	 * @param path Full path for the file.
	 * @param serialized TRUE if the path is a serialized name. See GetSerializedName()
	 *
	 * @return OK or ERR_NO_MEMORY.
	 */
	static OP_STATUS Create(OpLowLevelFile** new_file, const uni_char* path, BOOL serialized=FALSE);

	/** Destructor.
	 *
	 * May be called while asynchronous file operations are in progress.
	 *
	 * This will close the file if it is open. Before that it will complete any
	 * asynchronous file operations in progress, but without calling the
	 * associated listener methods. (Read-only asynchronous operations may thus
	 * be skipped without anyone noticing.)
	 */
	virtual ~OpLowLevelFile() {}

	/**
	 * Returns name of the class. Allows for safe down-casting.
	 */
	virtual const char* Type() const { return "OpLowLevelFile"; }

	/** Retrieve information about the file.
	 *
	 * The 'flags' member of the given OpFileInfo object chooses which pieces
	 * of information to retrieve.
	 *
	 * @param[in,out] info on input info.flags specifies which fields
	 *  are requested. Multiple flags can be OR-combined. Any other
	 *  attribute of info is undefined on input.
	 *
	 *  If the method returns OpStatus::OK, then on output the
	 *  implementation of this method must set at least the fields
	 *  that are requested by the specified flags. An implementation
	 *  is allowed to set more fields than what info.flags dictates,
	 *  but should refrain from doing so if it is more expensive. The
	 *  caller is required to ignore the fields not requested in
	 *  info.flags.
	 *
	 *  If the method returns an error code, then the caller must not
	 *  use any of the requested values, because they may be undefined.
	 *
	 * @return
	 *  - OpStatus::OK if the requested fields were filled successfully.
	 *  - OpStatus::ERR_NO_MEMORY if there was not enough memory to
	 *    fill the data or execute the operation.
	 *  - OpStatus::ERR_NOT_SUPPORTED if at least one of the specified
	 *    flags is not supported by the platform or file system.
	 *  - OpStatus::ERR or another error code for other errors.
	 */
	virtual OP_STATUS GetFileInfo(OpFileInfo* info) = 0;

#ifdef SUPPORT_OPFILEINFO_CHANGE
	/** Change file information.
	 *
	 * Only applicable to existing files that are closed. OpStatus::ERR is
	 * returned otherwise.
	 *
	 * The 'flags' member of the given OpFileInfoChange object chooses which
	 * pieces of information to change. Before making any changes to the file,
	 * the implementation will check if all requested operations are supported,
	 * and, if that is not the case, return OpStatus::ERR_NOT_SUPPORTED
	 * immediately.
	 *
	 * @param changes What to change ('changes.flags'), and what to change it
	 * to (the other member variables of 'changes').
	 *
	 * @return OK if successful, ERR_NO_MEMORY on OOM, ERR_NO_ACCESS if write
	 * access was denied, ERR_FILE_NOT_FOUND if the file does not exist,
	 * ERR_NOT_SUPPORTED if (the file exists, but) at least one of the flags
	 * set is not supported by the platform or file system, ERR for other
	 * errors.
	 */
	virtual OP_STATUS ChangeFileInfo(const OpFileInfoChange* changes) = 0;
#endif // SUPPORT_OPFILEINFO_CHANGE

	/** Get the full absolute path for the file.
	 *
	 * May be called while asynchronous file operations are in progress.
	 * The path shall not contain trailing path separator. Exceptions are the
	 * root directory '/' on unix-like systems or 'C:\' on windows like systems.
	 */
	virtual const uni_char* GetFullPath() const = 0;

	/** Get the serialized full absolute path for the file.
	 *
	 * May be called while asynchronous file operations are in progress.
	 *
	 * Returns the file name in a serialized form. Used for storing file
	 * names in prefs files. For most platforms this is the same as
	 * GetFullPath(), but some platforms may need to e.g. escape certain
	 * characters.
	 *
	 * If the platform expands environment variables when unserializing
	 * file names, it should return the original name here for the file
	 * to be serialized again with the environment variable references
	 * intact.
	 */
	virtual const uni_char* GetSerializedName() const = 0;

	/** Get the file name in a localized/"friendly" form.
	 *
	 * May be called while asynchronous file operations are in progress.
	 *
	 * Some systems support a localized or "user friendly" version of
	 * some file paths. For example, in Windows XP a user's documents
	 * are typically stored in "C:\\Documents and settings\\{user}\\My
	 * Documents". A localized form of this path may be simply "My
	 * Documents" (in English) or "Mine Dokumenter" (in Norwegian).
	 *
	 * If the system cannot provide a localized form for this
	 * particular file path, or in general, this method should return
	 * OpStatus::ERR.
	 *
	 * @param[out] localized_path Set to the localized path. The value must be
	 * ignored unless OpStatus::OK is returned.
	 *
	 * @return OK, ERR_NO_MEMORY or ERR.
	 */
	virtual OP_STATUS GetLocalizedPath(OpString *localized_path) const = 0;

	/** @return TRUE if the file is writable.
	 *
	 * May be called while asynchronous file operations are in progress.
	 *
	 * If the file doesn't exist, check if it is creatable. That counts as
	 * writable.
	 */
	virtual BOOL IsWritable() const = 0;

	/** Open the file.
	 *
	 * If the file is to be created when opened (determined by the 'mode'
	 * parameter), the directory structure is completed automatically;
	 * i.e. all missing directories in the filename must be created.
	 *
	 * If the file exists prior to this method call, it must be a regular file
	 * or a symbolic link pointing to a regular file (i.e. not a directory, for
	 * example), for this method to succeed. Otherwise, OpStatus::ERR is
	 * returned.
	 *
	 * @param mode A combination of OpFileOpenMode values ORed together.
	 *
	 * @return OK, ERR_NO_MEMORY, ERR_NO_DISK, ERR_NO_ACCESS,
	 * ERR_FILE_NOT_FOUND or ERR. ERR is returned if the flag combination in
	 * 'mode' is illegal.
	 */
	virtual OP_STATUS Open(int mode) = 0;

	/** Is the file open?
	 *
	 * May be called while asynchronous file operations are in progress.
	 *
	 * @return TRUE if the file is open.
	 */
	virtual BOOL IsOpen() const = 0;

	/** Close the file.
	 *
	 * Has no effect if the file is not open -- this is defined as a
	 * successful operation, even if the file does not exist.
	 *
	 * @return OK, ERR_NO_MEMORY, ERR_NO_DISK or ERR.
	 */
	virtual OP_STATUS Close() = 0;

	/** Create the path as a directory, if it does not already exist.
	 *
	 * The directory structure is completed automatically; i.e. all missing
	 * directories of the destination (this file) must be created.
	 *
	 * @return OK if successful (also if the directory already existed),
	 * ERR_NO_MEMORY on OOM, ERR_NO_DISK if there is insufficient space on the
	 * file system, ERR_NO_ACCESS if access was denied, ERR if the path exists
	 * but is a regular file, and for other errors.
	 */
	virtual OP_STATUS MakeDirectory() = 0;

	/** Check EOF state: has an attempt been made to read past the end of the file?
	 *
	 * Only applicable to open files. Otherwise, return value is undefined.
	 *
	 * Simply having read the last byte (or having current file position at
	 * the last byte) in the file doesn't imply EOF; one must have attempted
	 * to read past the end of it to be EOF. In other words, behavior should
	 * be identical to C89 feof(). EOF state is cleared by calling
	 * SetFilePos() or SetFileLength(), or by closing and reopening the file.
	 *
	 * This method may NOT be called while asynchronous file operations are in
	 * progress. If that happens, behavior is undefined.
	 *
	 * @return TRUE if EOF, otherwise FALSE.
	 */
	virtual BOOL Eof() const = 0;

	/** Check if the file exists.
	 *
	 * @param[out] exists Set to TRUE if the file exists. The value must be
	 * ignored unless OpStatus::OK is returned.
	 *
	 * @return OK, ERR_NO_MEMORY, ERR_NO_ACCESS or ERR. ERR_NO_ACCESS is
	 * returned only when it is impossible to <b>check whether the file
	 * exists</b>, due to access restrictions. Access restrictions on the file
	 * itself does not cause an error. ERR_FILE_NOT_FOUND should never be
	 * returned, since the sole purpose of this method is to check whether the
	 * file exists.
	 */
	virtual OP_STATUS Exists(BOOL* exists) const = 0;

	/** Get the current read/write position of the file.
	 *
	 * Only applicable to open files. OpStatus::ERR is returned otherwise.
	 *
	 * @param[out] pos Set to the current read/write position. The value must
	 * be ignored unless OpStatus::OK is returned.
	 *
	 * @return OK, ERR_NO_MEMORY, ERR_NO_DISK or ERR.
	 */
	virtual OP_STATUS GetFilePos(OpFileLength* pos) const = 0;

	/** Set the current read/write position.
	 *
	 * Only applicable to open files. OpStatus::ERR is returned otherwise.
	 *
	 * @param pos New read/write position.
	 * @param seek_mode pos is relative start, end or current position.
	 * @return OK, ERR_NO_MEMORY, ERR_NO_DISK or ERR.
	 */
	virtual OP_STATUS SetFilePos(OpFileLength pos, OpSeekMode seek_mode=SEEK_FROM_START) = 0;

	/** Get the length of the file.
	 *
	 * Only applicable to existing files. OpStatus::ERR_FILE_NOT_FOUND is
	 * returned otherwise.
	 *
	 * @param[out] len Set to the length of the file. The value must be ignored
	 * unless OpStatus::OK is returned.
	 *
	 * @return OK, ERR_NO_MEMORY, ERR_NO_DISK, ERR_NO_ACCESS,
	 * ERR_FILE_NOT_FOUND or ERR.
	 */
	virtual OP_STATUS GetFileLength(OpFileLength* len) const = 0;

	/** Set the length of the file.
	 *
	 * Only applicable to open files. OpStatus::ERR is returned otherwise.
	 *
	 * File position after this operation is undefined.
	 *
	 * @param len New length of the file. If the file previously was larger
	 * than this, the extra data is lost. If the file previously was shorter,
	 * it is extended, and the extended part reads as NUL bytes.
	 *
	 * @return OK, ERR_NO_MEMORY, ERR_NO_DISK or ERR.
	 */
	virtual OP_STATUS SetFileLength(OpFileLength len) = 0;

	/** Write a chunk of data to the file.
	 *
	 * Only applicable to open files. OpStatus::ERR is returned otherwise.
	 *
	 * When a file is opened with a mode that allows both reading and writing
	 * (OPFILE_UPDATE or OPFILE_APPENDREAD), the API user shall ensure that a
	 * write operation is not directly followed by a read operation without an
	 * intervening call to Flush() or SetFilePos().
	 *
	 * Notes about OPFILE_APPEND mode: Subsequent writes to the file will
	 * always end up at the then current end of file, irrespective of any
	 * intervening SetFilePos() calls. The read/write position is moved to
	 * the new end of file position.
	 *
	 * @param data Data to write.
	 * @param len Length of data parameter, in bytes.
	 *
	 * @return OK if it succeeded, ERR_NO_MEMORY if out of memory, ERR_NO_DISK
	 * if the file system is full, or ERR for generic errors.
	 */
	virtual OP_STATUS Write(const void* data, OpFileLength len) = 0;

	/** Read a chunk of data.
	 *
	 * Only applicable to open files. OpStatus::ERR is returned otherwise.
	 *
	 * When a file is opened with a mode that allows both reading and writing
	 * (OPFILE_UPDATE or OPFILE_APPENDREAD), the API user shall ensure that a
	 * read operation is not directly followed by write operation without an
	 * intervening call to SetFilePos(), unless the read operation encounters
	 * end-of-file.
	 *
	 * @param data Where to store the read data.
	 * @param len Maximum number of bytes to read.
	 * @param[out] bytes_read Stores number of bytes read. The value must be
	 * ignored unless OpStatus::OK is returned.
	 *
	 * @return OK if it succeeded, ERR_NO_MEMORY if out of memory,
	 * ERR_NULL_POINTER is bytes_read is NULL, or ERR for generic errors.
	 * Note that reaching EOF is not considered to be an error.
	 */
	virtual OP_STATUS Read(void* data, OpFileLength len, OpFileLength* bytes_read) = 0;

	/** Read one line from the file.
	 *
	 * Only applicable to open files. OpStatus::ERR is returned otherwise.
	 *
	 * Reading stops when a newline character is found or EOF is reached.
	 * The newline character must not be stored in the result string.
	 *
	 * @param[out] data Where to store pointer to the data read. The data must
	 * be allocated by the platform implementation using new[]. Ownership is
	 * transferred to the caller, meaning that the caller must delete it (using
	 * delete []) when done with it. Data is only allocated if OK is returned;
	 * otherwise it must be ignored.
	 *
	 * @return OK if it succeeded, ERR_NO_MEMORY if out of memory, or ERR for
	 * generic errors.
	 */
	virtual OP_STATUS ReadLine(char** data) = 0;

	/** Create a copy of the OpLowLevelFile object.
	 *
	 * May be called while asynchronous file operations are in progress.
	 *
	 * Note that no real disk copy is performed here. The new object shall be
	 * closed, even if this file is open.
	 *
	 * @return A pointer to a new OpLowLevelFile, or NULL if error.
	 */
	virtual OpLowLevelFile* CreateCopy() = 0;

	/** Create a temporary OpLowLevelFile in the same folder as this file.
	 *
	 * May be called while asynchronous file operations are in progress.
	 *
	 * This method returns an OpLowLevelFile holding a valid filename, and such
	 * that a file with this name did not exist when this method checked. The
	 * filename generated will start with the value of the 'prefix' parameter
	 * in case it is non-NULL.
	 *
	 * The temporary file is not opened automatically, but it must exist in the
	 * file system when this method has returned. Its size shall be 0. The
	 * implementation is not required to generate the file name and create its
	 * file system entry in one atomic operation.
	 *
	 * @param prefix Give the temporary file this prefix. An empty string, or
	 * even a NULL pointer, is allowed. There may be a maximum number of
	 * characters limitation on systems here, but such a limit is currently not
	 * documented. However, it is recommended that API users do not use more
	 * than 5 characters.
	 *
	 * @return A pointer to a new OpLowLevelFile. NULL if error.
	 */
	virtual OpLowLevelFile* CreateTempFile(const uni_char* prefix) = 0;

	/** Copy the contents of another file into this file.
	 *
	 * Both files (this file and 'source') must be closed. OpStatus::ERR is
	 * returned otherwise.
	 *
	 * This file may or may not exist before the operation. The directory
	 * structure is completed automatically; i.e. all missing directories of
	 * the destination (this file) must be created.
	 *
	 * @param source File from which to copy contents. It must exist, or
	 * OpStatus::ERR_FILE_NOT_FOUND is returned.
	 *
	 * @return OK, ERR_NO_MEMORY, ERR_NO_DISK, ERR_NO_ACCESS,
	 * ERR_FILE_NOT_FOUND or ERR.
	 */
	virtual OP_STATUS CopyContents(const OpLowLevelFile* source) = 0;

	/** Close the file, flush write buffers.
	 *
	 * Closing a file in this way ensures that data is actually written to
	 * disk (no OS caching etc.) before the call returns.
	 *
	 * Has no effect if the file is not open -- this is defined as a
	 * successful operation, even if the file does not exist.
	 *
	 * @return OK, ERR_NO_MEMORY, ERR_NO_DISK or ERR.
	 */
	virtual OP_STATUS SafeClose() = 0;

	/** Replace this file with another file as an atomic operation.
	 *
	 * Only applicable to closed files. OpStatus::ERR is returned
	 * otherwise. The file 'source' must exist before the operation.
	 * OpStatus::ERR_FILE_NOT_FOUND is returned otherwise.
	 *
	 * This file may or may not exist before the operation. The directory
	 * structure is completed automatically; i.e. all missing directories of
	 * the destination (this file) must be created. If the destination file
	 * already exists, it must be writable; if it is read-only, this operation
	 * shall fail.
	 *
	 * This is an atomic operation; i.e. this file must be either overwritten
	 * with the complete contents of the other file ('source' parameter)
	 * (success), or remain unmodified (failure). After this operation, the
	 * other file ('source') must either not exist (success), or remain
	 * unmodified (failure). The implementation may simply rename (or "move")
	 * the other file to this file if that is a safe operation.
	 *
	 * @param source File to replace this file with. The file must exist before
	 * the operation, and it must be gone after a successful operation, or
	 * remain unmodified after an unsuccessful operation.
	 * @return OK, ERR_NO_MEMORY, ERR_NO_DISK, ERR_NO_ACCESS,
	 * ERR_FILE_NOT_FOUND or ERR.
	 */
	virtual OP_STATUS SafeReplace(OpLowLevelFile* source) = 0;

	/** Delete the file.
	 *
	 * Only applicable to existing files that are closed. OpStatus::ERR is
	 * returned if the file is open. OpStatus::ERR_FILE_NOT_FOUND is returned
	 * if the file does not exist.
	 *
	 * @return OK if it succeeded, ERR_NO_MEMORY if out of memory,
	 * ERR_NO_ACCESS if write access was denied, ERR_FILE_NOT_FOUND if the file
	 * did not exist, or ERR for generic errors.
	 */
	virtual OP_STATUS Delete() = 0;

	/** Flush write buffers.
	 *
	 * Only applicable to open files. OpStatus::ERR is returned otherwise.
	 *
	 * Forces a write to disk of any buffered data for the file. This will
	 * only work reliably if the file is opened with the OPFILE_COMMIT flag
	 * set, and then only if the platform supports it.
	 *
	 * Note that the openness of the file is unaffected.
	 *
	 * File position after this operation is undefined.
	 *
	 * @return OK, ERR_NO_MEMORY, ERR_NO_DISK or ERR.
	 */
	virtual OP_STATUS Flush() = 0;

#ifdef PI_ASYNC_FILE_OP
	/** Set the listener to be called for asynchronous operations on this file
	 *
	 * Must be called BEFORE initiating the first asynchronous operations.
	 *
	 * May NOT be called while asynchronous file operations are in progress.
	 *
	 * @param listener The OpLowLevelFileListener object whose methods will be
	 * invoked when asynchronous operations on this OpLowLevelFile object are
	 * completed.
	 */
	virtual void SetListener(OpLowLevelFileListener *listener) = 0;

	/** Request asynchronous file read operation.
	 *
	 * See Read() for more details.
	 *
	 * May be called while asynchronous file operations are in progress.
	 *
	 * OpLowLevelFileListener::OnDataRead() is called when the read operation
	 * has completed. It has to be called once and only once per
	 * operation. Note that listener methods may be called before this method
	 * has returned. This includes calls to listener methods that belong to
	 * previously requested asynchronous operations that just finished.
	 *
	 * Before this method is called, a valid listener object must have been
	 * set for this file. See SetListener() for more details.
	 *
	 * @param data Data buffer to put the read data into. Must be at least as
	 * large as 'len'. Must not be deleted or dereferenced at all by the caller
	 * while the read operation is in progress.
	 * @param len Number of bytes to read.
	 * @param abs_pos File position. A value of FILE_LENGTH_NONE means start
	 * reading at current file position, and update it, just like for a normal
	 * Read(). If it is different from FILE_LENGTH_NONE, on the other hand,
	 * reading will be done at the specified position, relative to the
	 * beginning of the file, and it will not affect the file position of this
	 * object (GetFilePos() will return the same value before and after this
	 * operation).
	 *
	 * @return OK if the request was issued (but not necessarily handled)
	 * successfully, ERR_NO_MEMORY on OOM, ERR if the asynchronous file
	 * operation request could not be issued for some reason. If ERR or
	 * ERR_NO_MEMORY is returned, the listener method will not be called.
	 */
	virtual OP_STATUS ReadAsync(void* data, OpFileLength len, OpFileLength abs_pos = FILE_LENGTH_NONE) = 0;

	/** Request asynchronous file write operation.
	 *
	 * See Write() for more details.
	 *
	 * May be called while asynchronous file operations are in progress.
	 *
	 * OpLowLevelFileListener::OnDataWritten() is called when the write
	 * operation has completed. It has to be called once and only once per
	 * operation. Note that listener methods may be called before this method
	 * has returned. This includes calls to listener methods that belong to
	 * previously requested asynchronous operations that just finished.
	 *
	 * Before this method is called, a valid listener object must have been
	 * set for this file. See SetListener() for more details.
	 *
	 * @param data Data buffer to write. Must not be deleted or dereferenced at
	 * all by the caller while the write operation is in progress.
	 * @param len Number of bytes to write
	 * @param abs_pos File position. A value of FILE_LENGTH_NONE means start
	 * writing at current file position, and update it, just like for a normal
	 * Write(). If it is different from FILE_LENGTH_NONE, on the other hand,
	 * writing will be done at the specified position, relative to the
	 * beginning of the file, and it will not affect the file position of this
	 * object (GetFilePos() will return the same value before and after this
	 * operation).
	 *
	 * @return OK if the request was issued (but not necessarily handled)
	 * successfully, ERR_NO_MEMORY on OOM, ERR if the asynchronous file
	 * operation request could not be issued for some reason. If ERR or
	 * ERR_NO_MEMORY is returned, the listener method will not be called.
	 */
	virtual OP_STATUS WriteAsync(const void* data, OpFileLength len, OpFileLength abs_pos = FILE_LENGTH_NONE) = 0;

	/** Request asynchronous file delete operation.
	 *
	 * See Delete() for more details.
	 *
	 * May be called while asynchronous file operations are in progress.
	 *
	 * OpLowLevelFileListener::OnDeleted() is called when the delete operation
	 * has completed. It has to be called once and only once per
	 * operation. Note that listener methods may be called before this method
	 * has returned. This includes calls to listener methods that belong to
	 * previously requested asynchronous operations that just finished.
	 *
	 * Before this method is called, a valid listener object must have been
	 * set for this file. See SetListener() for more details.
	 *
	 * @return OK if the request was issued (but not necessarily handled)
	 * successfully, ERR_NO_MEMORY on OOM, ERR if the asynchronous file
	 * operation request could not be issued for some reason. If ERR or
	 * ERR_NO_MEMORY is returned, the listener method will not be called.
	 */
	virtual OP_STATUS DeleteAsync() = 0;

	/** Request asynchronous write buffer flush operation.
	 *
	 * See Flush() for more details.
	 *
	 * May be called while asynchronous file operations are in progress.
	 *
	 * OpLowLevelFileListener::OnFlushed() is called when the flush operation
	 * has completed. It has to be called once and only once per
	 * operation. Note that listener methods may be called before this method
	 * has returned. This includes calls to listener methods that belong to
	 * previously requested asynchronous operations that just finished.
	 *
	 * Before this method is called, a valid listener object must have been
	 * set for this file. See SetListener() for more details.
	 *
	 * @return OK if the request was issued (but not necessarily handled)
	 * successfully, ERR_NO_MEMORY on OOM, ERR if the asynchronous file
	 * operation request could not be issued for some reason. If ERR or
	 * ERR_NO_MEMORY is returned, the listener method will not be called.
	 */
	virtual OP_STATUS FlushAsync() = 0;

	/** Complete all pending/unfinished asynchronous file operations.
	 *
	 * May be called while asynchronous file operations are in progress.
	 *
	 * A call to this method will block until all pending/unfinished
	 * file operations have completed, and have the resulting listener
	 * methods being called. If there are no asynchronous operations
	 * in progress, this method will just return immediately.
	 *
	 * @return Normally OK, but if a memory allocation (not releated to any
	 * particular operation) related to syncing failed, ERR_NO_MEMORY is
	 * returned.
	 */
	virtual OP_STATUS Sync() = 0;

	/** Are there any asynchronous file operations in progress?
	 *
	 * May be called while asynchronous file operations are in progress.
	 *
	 * @return TRUE if there are any pending/unfinished asynchronous file
	 * operations, FALSE otherwise.
	 */
	virtual BOOL IsAsyncInProgress() = 0;
#endif // PI_ASYNC_FILE_OP
};

#endif // !OPLOWLEVELFILE_H
