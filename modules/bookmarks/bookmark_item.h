/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef BOOKMARK_ITEM_H
#define BOOKMARK_ITEM_H

#ifdef CORE_BOOKMARKS_SUPPORT

#include "modules/util/tree.h"

/**
 * Enum containing the different attributes that are associated with a
 * bookmark.
 */
enum BookmarkAttributeType
{
	BOOKMARK_URL = 0,            // string
	BOOKMARK_TITLE,              // string
	BOOKMARK_DESCRIPTION,        // string
	BOOKMARK_SHORTNAME,          // string
	BOOKMARK_FAVICON_FILE,       // string?
	BOOKMARK_THUMBNAIL_FILE,     // string?
	BOOKMARK_CREATED,            // string?
	BOOKMARK_VISITED,             // string?
#ifdef BOOKMARKS_PERSONAL_BAR
	BOOKMARK_PANEL_POS,			 // int
	BOOKMARK_SHOW_IN_PANEL,		 // int
	BOOKMARK_PERSONALBAR_POS,    // int
	BOOKMARK_SHOW_IN_PERSONAL_BAR, // int
#endif
	BOOKMARK_ACTIVE,			 // BOOL
	BOOKMARK_TARGET,
	BOOKMARK_EXPANDED,
	BOOKMARK_SMALLSCREEN,
	BOOKMARK_PARTNER_ID,		// string
	BOOKMARK_DISPLAY_URL,		// used by Desktop to display an user friendly url on UI
#ifdef CORE_SPEED_DIAL_SUPPORT
	BOOKMARK_SD_RELOAD_ENABLED,
	BOOKMARK_SD_RELOAD_INTERVAL,
	BOOKMARK_SD_RELOAD_EXPIRED,
#endif // CORE_SPEED_DIAL_SUPPORT
	BOOKMARK_NONE
};

/**
 * The bookmarks are stored in a tree of folders and this enum
 * contain the different types of folders that may be used. These
 * folders have no connection to directories in the filesystem.
 *
 * FOLDER_NO_FOLDER means that the bookmark is an actual bookmark.
 * FOLDER_NORMAL_FOLDER means that the bookmark is a folder.
 * FOLDER_TRASH_FOLDER means that the bookmark is a special type of
 * folder which contain bookmarks that are to be removed.
 * FOLDER_SPEED_DIAL_FOLDER is a special folder that contains
 * bookmarks that represents speeddials.
 * FOLDER_SEPARATOR_FOLDER is a special folder that represents a
 * separator and does not contain any bookmarks.
 */
enum BookmarkFolderType
{
	FOLDER_NORMAL_FOLDER = 0,
	FOLDER_TRASH_FOLDER,
#ifdef CORE_SPEED_DIAL_SUPPORT
	FOLDER_SPEED_DIAL_FOLDER,
#endif // CORE_SPEED_DIAL_SUPPORT
	FOLDER_SEPARATOR_FOLDER,
	FOLDER_NO_FOLDER
};

/**
 * Different actions to take when an attribute is to long.
 *
 * ACTION_FAIL just return OpStatus::ERR,
 * ACTION_CUT cut the value at the given maximum length.
 */
enum BookmarkActionType
{
	ACTION_FAIL = 0,
	ACTION_CUT
};

class BookmarkItem;
class BookmarkItemListener;

/**
 * Class containing functions for manipulation of bookmark attributes.
 */
class BookmarkAttribute
{
public:
	BookmarkAttribute();
	~BookmarkAttribute();
	void SetAttributeType(BookmarkAttributeType attr_type);
	BookmarkAttributeType GetAttributeType() const;
	OP_STATUS SetValue(BookmarkAttribute *attr);
	OP_STATUS GetTextValue(OpString &value) const;

	/**
	 * Returns the text value of this attribute if any. The returned
	 * pointer is a pointer to internal data that is owned by this
	 * object and is only valid as long as this object is valid.
	 */
	uni_char* GetTextValue() const;
	OP_STATUS SetTextValue(const uni_char *value);
	OP_STATUS SetTextValue(const uni_char *value, unsigned len);
	BOOL OwnsTextValue() const;
	void SetOwnsTextValue(BOOL owns);
	int GetIntValue() const;
	void SetIntValue(int value);
	UINT32 GetMaxLength() const;
	OP_STATUS SetMaxLength(UINT32 value);
	BookmarkActionType GetMaxLengthAction() const;
	void SetMaxLengthAction(BookmarkActionType action);
	BOOL ViolatingMaxLength(BookmarkItem *bookmark);
	OP_STATUS Cut(UINT32 len);
	BOOL Equal(BookmarkAttribute *attribute) const;
	BOOL Equal(OpString *text_value) const;
#ifdef SUPPORT_DATA_SYNC
	BOOL ShouldSync() const;
	void SetSync(BOOL value);
#endif // SUPPORT_DATA_SYNC
private:
	uni_char *m_text_value;
	int m_int_value;
	UINT32 m_max_len;
	union
	{
		struct
		{
			unsigned int m_attr_type : 5;
			unsigned int m_owns_text_val : 1;
			unsigned int m_max_len_action : 1;
#ifdef SUPPORT_DATA_SYNC
			unsigned int  m_sync : 1;
#endif // SUPPORT_DATA_SYNC
		} packed;
		unsigned char packed_init;
	};
};

/**
 * The actual bookmark.
 */
class BookmarkItem : public Tree
{
public:
	BookmarkItem();
	virtual ~BookmarkItem();
	
	/**
	 * Return the id associated with this current bookmark.
	 */
	uni_char* GetUniqueId() const;
	
	/**
	 * Set the id associated with this bookmark. The parameter 'uid'
	 * will be deleted by this object.
	 */
	void SetUniqueId(uni_char *uid);
	
	/**
	 * Return the parent folder id for this bookmark.
	 */
	uni_char* GetParentUniqueId() const;
	/**
	 * Set the id for parent folder for this bookmark.
	 */
	void SetParentUniqueId(uni_char *uid);
	
	/**
	 * Get the value of one of the bookmark attributes. Attributes from
	 * a predefined list, or platform-specific extensions
	 */
	OP_STATUS GetAttribute(BookmarkAttributeType attribute, BookmarkAttribute *attr_val) const;

	/**
	 * Get value of a bookmark attribute. The returned value is a
	 * pointer to an internal value that is only valid as long as this
	 * bookmark item is valid and it is owned by this bookmark item.
	 */
	BookmarkAttribute* GetAttribute(BookmarkAttributeType attr_type) const;
	
	/**
	 * Set one of the bookmark attributes. Space for value is
	 * allocated and value is copied. If value is NULL then the
	 * attribute will be cleared if it already existed and nothing
	 * will be done if it did not exist.
	 */
	OP_STATUS SetAttribute(BookmarkAttributeType attribute, BookmarkAttribute *value);
	
	/**
	 *
	 */
	BookmarkFolderType GetFolderType() const;
	
	/**
	 *
	 */
	void SetFolderType(BookmarkFolderType foldertype);
	
	/**
	 *
	 */
	BookmarkItem* GetParentFolder() const;
	
	/**
	 *
	 */
	OP_STATUS SetParentFolder(BookmarkItem *bookmark);
	
	/**
	 *
	 */
	BookmarkItem* GetChildren() const;

	/**
	 * Get the previous bookkmark in the same folder as this bookmark.
	 */
	BookmarkItem* GetPreviousItem() const;

	/**
	 * Get the next bookkmark in the same folder as this bookmark.
	 */
	BookmarkItem* GetNextItem() const;
	
	/**
	 *
	 */
	UINT32 GetFolderDepth() const;
	
	/**
	 *
	 */
	UINT32 GetCount() const;

	/**
	 *
	 */
	void SetCount(UINT32 count);

	unsigned int GetMaxCount() const;
	void SetMaxCount(unsigned int max_count);

	BOOL SubFoldersAllowed() const;
	void SetSubFoldersAllowed(BOOL value);

	BOOL Deletable() const;
	void SetDeletable(BOOL value);

	BOOL MoveIsCopy() const;
	void SetMoveIsCopy(BOOL value);

	BOOL SeparatorsAllowed() const;
	void SetSeparatorsAllowed(BOOL value);

    /**
	 *
	 */	
	void Cut(BookmarkAttributeType attr_type, UINT32 len);

	/**
	 *
	 */
	void ClearAttributes();

#ifdef SUPPORT_DATA_SYNC
	/** 
	 * Set whether a bookmark item should be synced when added
	 * modified or deleted. By default a bookmark item will be
	 * synced.
	 */
	void SetAllowedToSync(BOOL allowed_to_sync);

	/** 
	 * Check if this bookmark item should be synced when added,
	 * modified or deleted.
	 */
	BOOL IsAllowedToSync() const;

	BOOL IsModified() const;
	BOOL IsAdded() const;
	BOOL IsDeleted() const;
	void SetModified(BOOL is_modified);
	void SetAdded(BOOL is_added);
	void SetDeleted(BOOL is_deleted);

	BOOL ShouldSyncAttribute(BookmarkAttributeType attr_type) const;
	void SetSyncAttribute(BookmarkAttributeType attr_type, BOOL value);
	BOOL ShouldSyncParent() const;
	void SetSyncParent(BOOL value);

	void ClearSyncFlags();
	BOOL ShouldSync() const;
#endif // SUPPORT_DATA_SYNC

	void SetListener(BookmarkItemListener *listener);
	BookmarkItemListener* GetListener() const { return m_listener; }
	void RemoveListener(BookmarkItemListener *listener)
	{ 
		if (listener == m_listener) m_listener = NULL;
	}
private:
	BookmarkAttribute* FindAttribute(BookmarkAttributeType attr_type) const;
	OP_STATUS CreateAttribute(BookmarkAttributeType attr_type, BookmarkAttribute **attr);

	OpVector<BookmarkAttribute> m_attributes;

	uni_char *m_id;
	uni_char *m_pid;

	UINT32 m_count;
	unsigned int m_max_count;

	union
	{
		struct
		{
#ifdef SUPPORT_DATA_SYNC
			unsigned int m_is_modified : 1;
			unsigned int m_is_added : 1;
			unsigned int m_is_deleted : 1;
			unsigned int m_should_sync_parent : 1;
			unsigned int m_allowed_to_sync : 1;
#endif // SUPPORT_DATA_SYNC
			unsigned int m_folder_type : 3;
			unsigned int m_move_is_copy : 1;
			unsigned int m_deletable : 1;
			unsigned int m_sub_folders_allowed : 1;
			unsigned int m_separators_allowed : 1;
		} packed;
		unsigned short packed_init;
	};

	BookmarkItemListener *m_listener;
};

/**
 * Class for holding a list of bookmarks.
 */
class BookmarkListElm : public Link
{
public:
	void SetBookmark(BookmarkItem *b) { bookmark = b; }
	BookmarkItem* GetBookmark() { return bookmark; }
private:
	BookmarkItem *bookmark;
};

class BookmarkItemListener
{
public:
	virtual ~BookmarkItemListener() { }

	virtual void OnBookmarkChanged(BookmarkAttributeType attr_type, BOOL moved) = 0;
	virtual void OnBookmarkDeleted() = 0;
	virtual void OnBookmarkRemoved() = 0;
};

#endif // CORE_BOOKMARKS_SUPPORT
#endif // BOOKMARK_ITEM_H
