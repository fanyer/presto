/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef URL_STORE_H_
#define URL_STORE_H_

#ifdef SEARCH_ENGINE_CACHE
#include "modules/cache/CacheIndex.h"
#endif

#include "modules/url/url_rep.h"

/// "Bookmark" that can be used for saving the position in a LinkObjectStore object.
/// This class is not meant for being very "precise", but to allow to skip A LOT of objects in a relatively very short time
class LinkObjectBookmark
{
private:
	/// Index of the bucket
	unsigned int index;
	/// Position inside the bucket
	unsigned int position;
	/// Absolute number of objects traversed
	UINT32 absolute_position;

public:
	LinkObjectBookmark(): index(0), position(0), absolute_position(0) {}

	/// Reset the position to the first object in the first bucket
	void Reset() { index = position = absolute_position = 0; }
	/// Return TRUE if the bookmark is at reset position
	BOOL IsReset() { return !index && !position; }
	/// Increments the position in the current bucket
	void IncPosition() { position++; absolute_position++; }
	/// Increments the bucket position
	void IncBucket(BOOL valid_object) { index++; position=0; if(valid_object) absolute_position++; }
	/// Returns the index position
	unsigned int GetIndex() { return index; }
	/// Returns the position in the bucket
	unsigned int GetPosition() { return position; }
	/// Set the index position, optionally resetting the position and increasing the absolute position
	void SetIndex(unsigned int new_index, BOOL reset_position) { index=new_index; if(reset_position) { position=0; absolute_position++; } }
	/// Return the absolute position
	UINT32 GetAbsolutePosition() { return absolute_position; }
};


class URL_Store : public LinkObjectStore
{
public:
	URL_Store(unsigned int size);

	/**
	 * Second phase constructor. This method must be called prior to using the object,
	 * unless it was created using the Create() method.
	 */
	OP_STATUS Construct(
#ifdef DISK_CACHE_SUPPORT
		OpFileFolder folder
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
		// OPFILE_ABSOLUTE_FOLDER means do not use operator cache
		, OpFileFolder opfolder
#endif
#endif
		);
	
	/**
	 * Creates and initializes an URL_Store object. 
	 * @param url_store  Set to the created object if successful and to NULL otherwise.
	 *                   DON'T use this as a way to check for errors, check the
	 *                   return value instead!
	 * @param size
	 * @return OP_STATUS - Always check this.
	 */
	static OP_STATUS Create(URL_Store **url_store, unsigned int size
#ifdef DISK_CACHE_SUPPORT
		, OpFileFolder folder = OPFILE_CACHE_FOLDER
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
		// OPFILE_ABSOLUTE_FOLDER means do not use operator cache
		, OpFileFolder opfolder = OPFILE_ABSOLUTE_FOLDER
#endif
#endif
		);

	virtual ~URL_Store();

	URL_Rep*		GetFirstURL_Rep() {
		return (URL_Rep*) GetFirstLinkObject();
	}

	URL_Rep*		GetNextURL_Rep()
	{
		return (URL_Rep*) GetNextLinkObject();
	}

	void			RemoveURL_Rep(URL_Rep* url_rep)
	{
		RemoveLinkObject(url_rep);
	}
	void			AddURL_Rep(URL_Rep* url_rep)
	{
		AddLinkObject(url_rep);
	}

	URL_Rep*		GetURL_Rep(OP_STATUS &op_err, URL_Name_Components_st &url, BOOL create = TRUE
					, URL_CONTEXT_ID context_id = 0
					);

	unsigned long	URL_RepCount() { return LinkObjectCount(); }

#ifdef SEARCH_ENGINE_CACHE
	OP_STATUS AppendFileName(OpString &name, OpFileLength max_cache_size, BOOL session_only
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
		, BOOL operator_cache
#endif
		);

	CacheIndex m_disk_index;
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	CacheIndex m_operator_index;
#endif
#endif

	/////////////////// Bookmark support /////////////////////////////
	/// Get the first objects, and correctly reset the bookmark position
	URL_Rep*		GetFirstURL_Rep(LinkObjectBookmark &bm);
	/// Get the next link object, while increasing the bookmark position
	URL_Rep*		GetNextURL_Rep(LinkObjectBookmark &bookmark);
	/// Jump as fast as possible to the bookmark. While being a lot faster than just calling GetNextLinkObject()
	/// thousand of times, it can be relatively slow.
	URL_Rep*		JumpToURL_RepBookmark(LinkObjectBookmark &bookmark);
};

#endif // URL_STORE_H_

