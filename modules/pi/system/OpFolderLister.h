/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPFOLDERLISTER_H
#define OPFOLDERLISTER_H

#ifdef DIRECTORY_SEARCH_SUPPORT

/** Folder lister.
 *
 * An instance of this class finds and iterates over entries, matching a given
 * pattern, in a given folder (a.k.a. directory on some systems).
 *
 * The folder lister iterates over names of all entries in the directory: it
 * pays no attention to anything else about the entry in determining whether it
 * matches. In particular, "hidden" or "special" files (e.g. "." and "..") are
 * included if they match the given pattern; and several entries may refer to
 * the same physical entity on disk if the system supports any kind of aliasing
 * (e.g. Unix-style symbolic links and hard links).
 *
 * Construct() invalidates any prior search and prepares for a search, but does
 * not select an entry - there may, after all, be no matching entry to select.
 * In particular, an unreadable or non-existent folder yields a valid search
 * with no matches. Only after a successful call to Construct has prepared a
 * search (and no subsequent call has invalidated it) are any of the other
 * methods of this class valid.
 *
 * After a successful call to Construct, successive calls to Next() iterate
 * over the matching entries in the folder. Next() returns FALSE when no
 * further entries remain; for an empty search, this happens on the first call.
 * Each time it returns TRUE, it selects an entry in the folder: the remaining
 * methods of this class query properties of the currently selected entry.
 * After Next() has returned FALSE, no entry is selected, so one cannot validly
 * call these query methods.
 */
class OpFolderLister
{
public:
	/** Create an OpFolderLister object.
	 *
	 * @param new_lister (out) Set to the new OpFolderLister object
	 *
	 * @return OK or ERR_NO_MEMORY.
	 */
	static OP_STATUS Create(OpFolderLister** new_lister);

	virtual ~OpFolderLister() {}

	/** Start a new search.
	 *
	 * Selects a folder in which to search and a pattern to match against its
	 * contents. May (despite its name) be called at any time to abort any
	 * on-going search and prepare for a new search. When this method returns,
	 * there is no current result, until Next() is called.
	 *
	 * If the requested folder cannot be listed (e.g. due to access
	 * restrictions, or because it does not exist), an "empty" folder lister
	 * should be the result (that is, one whose Next() function always returns
	 * FALSE) and OpStatus::OK should be returned from this function.
	 *
	 * In the pattern, a '*' may match arbitrary text (including an empty one);
	 * the rest of a matching folder entry name must be identical to the rest
	 * of the pattern. Thus "*.*" matches any folder entry name with a "." in
	 * it (notably \em including "." and ".."); "*.ini" matches any entry name
	 * with ".ini" as its last four characters and "stem.*" matches any entry
	 * with "stem." as its first five. Note that only the entry's name \em
	 * within the folder is relevant.
	 *
	 * @param path Absolute path to the folder to list.
	 * @param pattern Search pattern to match.
	 *
	 * @return OK or ERR_NO_MEMORY.
	 */
	virtual OP_STATUS Construct(const uni_char* path, const uni_char* pattern) = 0;

	/** Advance to the next search result in the listing.
	 *
	 * Searches for a new entry in the folder matching the pattern. Returns
	 * FALSE if no such entry remains. Note, in particular, that this may
	 * happen on the first call, if there are no matches at all (e.g. because
	 * the folder does not exist). Otherwise, returns TRUE and selects a
	 * matching entry. Information about this entry may be obtained by calling
	 * the methods GetFileName(), GetFullPath(), GetFileLength() and
	 * IsFolder().
	 *
	 * @return FALSE if Construct() has not been called or failed its last
	 * call; or if all matching entries have since been seen; else TRUE.
	 */
	virtual BOOL Next() = 0;

	/** Get the filename for the current result.
	 *
	 * @return The filename for the current result.
	 */
	virtual const uni_char* GetFileName() const = 0;

	/** Get the full path for the current result.
	 *
	 * @return The full path for the current result.
	 */
	virtual const uni_char* GetFullPath() const = 0;

	/** Get the file size of the current result.
	 *
	 * @return The file size of the current result.
	 */
	virtual OpFileLength GetFileLength() const = 0;

	/** Check if the current result represents a folder.
	 *
	 * @return TRUE if the current result represents a folder.
	 */
	virtual BOOL IsFolder() const = 0;
};

#endif // DIRECTORY_SEARCH_SUPPORT

#endif // !OPFOLDERLISTER_H
