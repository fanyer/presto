/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#ifndef __UNIXUTILS_H__
#define __UNIXUTILS_H__

class OpFile;
class DesktopWindow;

namespace UnixUtils
{
	/**
	 * Convert 8-bit string to 16-bit target. It is assumed the string
	 * encoded in current locale. Should the initial conversion using 
	 * current locale fail, an attempt is made to convert from UTF8
	 *
	 * @param src 8-bit source string
	 * @param target 16-bit target string
	 *
	 * @return OpStatus::OK on success, otherwise an error code
	 */
	OP_STATUS FromNativeOrUTF8(const char* src, OpString *target);

	/**
	 * Determines the file permission level we shall use when creating files or
	 * directories. Reads the setting from OPERA_STRICT_FILE_PERMISSIONS
	 * environment variable.
	 *
	 * @return TRUE when we shall use a strict setting, otherwise FALSE
	 */
	BOOL UseStrictFilePermission();

	/**
	 * Creates all path components of a path with filename. Text after
	 * after the last '/' is assumed to be the filename when 'stop_on_last_pathsep'
	 * is TRUE and no directory will be made out of that component.
	 * 
	 * @param path The path to be constucted
	 * @param stop_on_last_pathsep Everything after the last PATHSEPCHAR 
	 *        gets ignored when TRUE. 
	 *
	 * @return TRUE on success, otherwise FALSE
	 */
	OP_STATUS CreatePath(const uni_char* filename, BOOL stop_on_last_pathsep);
	OP_STATUS CreatePath(const char* filename, BOOL stop_on_last_pathsep);

	/**
	 * Split applied path into a directory and a filename. The retuned directory
	 * exists while the file does not have to.
	 *
	 * @param src The path to split
	 * @param directory On successful return, the directory component of the path
	 * @param filename On successful return, the file component of the path
	 *
	 * @return TRUE on success, otherwise FALSE. The returned parameter
	           values can contain random data on FALSE
	 */
	BOOL SplitDirectoryAndFilename(const OpString& src, OpString& directory, OpString& filename);

	/**
	 * Clean and simplify the specified path by removing ".." and
	 * multiple '/' specifiers
	 *
	 * @param candidate Path to purify
	 */
	void CanonicalPath(OpString& candidate);

	/**
	 * Attempts to set a write lock (advisory locking) on the file.
	 * This function will open a file on success. The returned file
	 * descriptor should be closed when no longer needed. The lock is
	 * automatically removed the the process that created it terminates.
	 *
	 * @param filename The file to lock
	 * @param On success the file descriptor is returned if pointer is not NULL
	 *
	 * @return -1: Error, 0: Was already locked, 1: Success
	 */
	int LockFile(const OpStringC& filename, int* file_descriptor);

	BOOL SearchInStandardFoldersL( OpFile &file, const uni_char* filename, BOOL allow_local, BOOL copy_file );

	//* Call shortly after g_opera->InitL to get the random animal going.
	bool BreakfastSSLRandomEntropy();
	//* Feed a noisy word to the random animal.
	void FeedSSLRandomEntropy(UINT32 noise);

	int CalculateScaledSize(int src, double scale);

	/** Check whether we should disregard the requested font size.
	 * "Snap" to nearest (presumably) nice-looking size for core X fonts. This
	 * is done to avoid our somewhat ugly (but necessary) core X anti-aliased
	 * font scaling algorithm in cases where the requested size is pretty close
	 * to the size that was found to be the nearest one. Bitmap fonts scaled by
	 * the X server is also quite ugly, so let's try to avoid it.
	 * @param req_size The font size requested by Opera
	 * @param avail_size The nearest font size available
	 * @return The font size to be used. If avail_size is "close enough" to
	 * req_size, avail_size will be returned. Otherwise, req_size will be
	 * returned.
	 */
	bool SnapFontSize(int req_size, int avail_size);

	inline bool IsSurrogate(uni_char ch)
	{
		return ch >= 0xd800 && ch <= 0xdfff;
	}

	inline uint32_t DecodeSurrogate(uni_char low, uni_char high)
	{
		if (low < 0xd800 || low > 0xdbff || high < 0xdc00 || high > 0xdfff)
			return '?';

		return 0x10000 + (((low & 0x03ff) << 10) | (high & 0x03ff));
	}

	inline void MakeSurrogate(uint32_t ucs, uni_char &high, uni_char &low)
	{
		OP_ASSERT(ucs >= 0x10000 && ucs <= 0x10ffff);
		ucs -= 0x10000;
		high = 0xd800 | uni_char(ucs >> 10);
		low  = 0xdc00 | uni_char(ucs & 0x03ff);
	}

	/* Creates a grand-child process.
	 *
	 * A process, when complete, sends SIGCHLD to its parent and becomes defunct
	 * (a.k.a. a zombie) until its parent wait()s for it.  If the parent has
	 * died before the child, the child just exits peacefully (in fact because
	 * its parent's death orphaned it, at which point it was adopted by the init
	 * process, pid = 1, which ignores child death).
	 *
	 * This function does a double-fork and waits for the primary child's exit;
	 * the grand-child gets a return of 0 and should do something; the
	 * original process doesn't need to handle SIGCHLD.  If the primary fork
	 * fails, or the primary child is unable to perform the secondary fork, the
	 * parent process sees a negative return value; otherwise, a positive one.
	 */
	int GrandFork();


	/**
	 * Tests the filename to verify if it can be written to or not. The user will
	 * be prompted with a dialog if there is a problem. 
	 *
	 * @param parent Parent to dialogs if any. Can be NULL
	 * @param candidate The filename to test
	 * @param ask_replace Allow user to decide if an existing file can be overwritten
	 *
	 * @return TRUE if file can be written, FALSE otherwise
	 */
	BOOL CheckFileToWrite(DesktopWindow* parent, const OpStringC& candidate, BOOL ask_replace);

	/**
	 * Clean up a folder, i.e. if the folder exists, remove the folder and create a new
	 * folder with same name.
	 *
	 * @param path Path to folder
	 *
	 * @return OP_STATUS
	 */
	OP_STATUS CleanUpFolder(const OpString &path);

	/**
	 * Tells user filename is missing
	 *
	 * @param parent Dialog parent. Can be NULL
	 */
	void PromptMissingFilename(DesktopWindow* parent);

	/**
	 * Tells user filename is the same as an existing directory
	 *
	 * @param parent Dialog parent. Can be NULL
	 */
	void PromptFilenameIsDirectory(DesktopWindow* parent);

	/**
	 * Tells user filename can not be written because of lacking access rights.
	 * A test is done on the filename to treat it as a directory if it is. If the
	 * filename is empty a generic message is shown.
	 *
	 * @param parent Dialog parent. Can be NULL
	 * @param filename The filename
	 */
	void PromptNoWriteAccess(DesktopWindow* parent, const OpStringC& filename);

	/**
	 * Tells user filename points to an existing file. 
	 *
	 * @param parent Dialog parent. Can be NULL
	 * @param filename The filename
	 * @param ask_continue. If TRUE, the user can decide to overwrite the existing file
	 *
	 * @return TRUE if file can be overwritten, otherwise FALSE
	 */
	BOOL PromptFilenameAlreadyExist(DesktopWindow* parent, const OpStringC& filename, BOOL ask_continue);

	/**
	 * Tests for the existence of an environment variable and returns TRUE
	 * if it is set to "1", "yes" or "true". The "yes" or "true" can be in 
	 * any combination of cases
	 *
	 * @param name The environment variable to examine
	 *
	 * @return TRUE on a positive match, otherwise FALSE
	 */
	BOOL HasEnvironmentFlag(const char* name);

	/**
     * A static implementation of OpSystemInfo::ExpandSystemVariablesInString().
     *
     * @param in Serialized filename string
     * @param out OpString for storing the result
     * @return success or failure
     */
	OP_STATUS UnserializeFileName(const uni_char* in, OpString* out);

	/** Use system variables to re-express a path-name.
	 *
     * Complementary to UnserializeFileName().
	 *
	 * @param path Raw (but possibly relative) path to file or directory.
	 * @return NULL on OOM, else a pointer to a freshly allocated string, which
	 * the caller is responsible for delete[]ing when no longer needed.
	 */
	uni_char* SerializeFileName(const uni_char *path);
}

#endif // __UNIXUTILS_H__
