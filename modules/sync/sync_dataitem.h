/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef _SYNC_DATAITEM_H_INCLUDED_
#define _SYNC_DATAITEM_H_INCLUDED_

#include "modules/util/simset.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmlfragment.h"

#define OPERA_LINK_NS UNI_L("http://xmlns.opera.com/2006/link")
#define LINKNAME(localname) XMLExpandedName(OPERA_LINK_NS, UNI_L(localname))
#define LINKNAME_L(localname) XMLExpandedName(OPERA_LINK_NS, localname)

class OpSyncDataCollection;
class OpSyncCollection;
class OpSyncDataItem;
class OpSyncDataQueue;
class OpSyncItem;
class OpSyncFactory;
class OpSyncParser;

// Current class size on Windows 32-bit: 64 bytes
class OpSyncDataItem : private Link
{
	friend class OpSyncItem;
	friend class OpSyncDataCollection;

public:
	/** Only the top level item has a specific type, children should have
	 * DATAITEM_CHILD. */
	enum DataItemType
	{
		DATAITEM_GENERIC = 0,			///< not set yet
		// START DATAITEMS
		DATAITEM_BLACKLIST,				///< sub element under DATAITEM_SPEEDDIAL_2_SETTINGS
		DATAITEM_BOOKMARK,				///< bookmark item
		DATAITEM_BOOKMARK_FOLDER,		///< bookmark folder
		DATAITEM_BOOKMARK_SEPARATOR,	///< bookmark separator (mostly a UI thing, but syncable)
		DATAITEM_CONTACT,				///< contact
		DATAITEM_ENCRYPTION_KEY,		///< encryption_key
		DATAITEM_ENCRYPTION_TYPE,		///< encryption_type
		DATAITEM_EXTENSION,				///< extension
		DATAITEM_FEED,					///< feed
		DATAITEM_NOTE,					///< note item
		DATAITEM_NOTE_FOLDER,			///< note folder
		DATAITEM_NOTE_SEPARATOR,		///< note separator (mostly a UI thing, but syncable)
		DATAITEM_PM_FORM_AUTH,			///< pm_form_auth
		DATAITEM_PM_HTTP_AUTH,			///< pm_http_auth
		DATAITEM_SEARCH,				///< search engines
		DATAITEM_SPEEDDIAL,				///< speed dial item
		DATAITEM_SPEEDDIAL_2,			///< speed dial 2 item (new speed dial structure using regular unique id and previous)
		DATAITEM_SPEEDDIAL_2_SETTINGS,	///< speed dial 2 settings element, a meta data type for speed dial 2 settings
		DATAITEM_TYPED_HISTORY,			///< typed history item
		DATAITEM_URLFILTER,				///< url filter
		DATAITEM_MAX_DATAITEM,
		// END DATAITEMS
		DATAITEM_ATTRIBUTE,				///< attribute item
		DATAITEM_CHILD,					///< child item
		DATAITEM_MAX,
	};

	/**
	 * Returns the base type of this OpSyncDataItem.
	 *
	 * Usually this is the same as GetType(), except for bookmarks and notes.
	 * For the folder and separator of these types the base type is
	 * DATAITEM_BOOKMARK resp. DATAITEM_NOTE. This is needed because an
	 * OpSyncDataItem of the bookmark/notes types may reference any other
	 * OpSyncDataItem of the same base type as its "previous" item (see
	 * SYNC_KEY_PREV).
	 */
	inline static DataItemType BaseTypeOf(DataItemType t) {
		switch (t) {
		case DATAITEM_BOOKMARK:
		case DATAITEM_BOOKMARK_FOLDER:
		case DATAITEM_BOOKMARK_SEPARATOR:
			return DATAITEM_BOOKMARK;

		case DATAITEM_ENCRYPTION_KEY:
		case DATAITEM_ENCRYPTION_TYPE:
			return DATAITEM_ENCRYPTION_KEY;

		case DATAITEM_NOTE:
		case DATAITEM_NOTE_FOLDER:
		case DATAITEM_NOTE_SEPARATOR:
			return DATAITEM_NOTE;

		case DATAITEM_ATTRIBUTE:
		case DATAITEM_BLACKLIST:
		case DATAITEM_CHILD:
		case DATAITEM_CONTACT:
		case DATAITEM_EXTENSION:
		case DATAITEM_FEED:
		case DATAITEM_GENERIC:
		case DATAITEM_MAX:
		case DATAITEM_MAX_DATAITEM:
		case DATAITEM_PM_FORM_AUTH:
		case DATAITEM_PM_HTTP_AUTH:
		case DATAITEM_SEARCH:
		case DATAITEM_SPEEDDIAL:
		case DATAITEM_SPEEDDIAL_2:
		case DATAITEM_SPEEDDIAL_2_SETTINGS:
		case DATAITEM_TYPED_HISTORY:
		case DATAITEM_URLFILTER:
		default:
			return t;
		}
	}

	OpSyncDataItem(OpSyncDataItem::DataItemType type=OpSyncDataItem::DATAITEM_GENERIC);

private:
	/**
	 * The destructor is declared private. Instead of deleting an instance of
	 * this class, the user shall use IncRefCount() and DecRefCount().
	 */
	virtual ~OpSyncDataItem();

public:
	/**
	 * Makes an exact copy of the item including all children and attributes.
	 *
	 * @note When adding new fields, make sure to update the Copy() method!
	 *
	 * @return The newly created OpSyncDataItem instance or 0 in case of OOM.
	 */
	OpSyncDataItem* Copy() const;

	/**
	 * This enum is returned by Merge() to indicate the status if this item
	 * after the call.
	 */
	enum MergeStatus {
		/** This item was not changed in the Merge() call. */
		MERGE_STATUS_UNCHANGED,
		/** Some part of this item was changed in the Merge() call. */
		MERGE_STATUS_MERGED,
		/** This item was deleted in the Merge() call. This may happen if this
		 * item had the #DataItemStatus #DATAITEM_ACTION_ADDED and the other
		 * item had the #DataItemStatus #DATAITEM_ACTION_DELETED. And instead
		 * of sending first an "added" item and then a "deleted" item, we
		 * remove both items. */
		MERGE_STATUS_DELETED
	};

	/**
	 * Merges the content of the specified other item into this item. This means
	 * that attributes and children from the other item will overwrite
	 * attributes and children of this item. This is an aggressive merge, i.e.
	 * this instance will take over the attribute and children of the other item
	 * instead of creating a copy.
	 *
	 * This method can be used to update an existing item, that is waiting to be
	 * sent to the Link server.
	 *
	 * @note If this item has the #DataItemStatus #DATAITEM_ACTION_ADDED and
	 *  the other item has the #DataItemStatus #DATAITEM_ACTION_DELETED, then
	 *  both this and the other item is deleted and the merge_status is set to
	 *  #MERGE_STATUS_DELETED.
	 *
	 * @param other is the other OpSyncDataItem that shall be merged into this
	 *  item. As a result of this method call the other OpSyncDataItem may loose
	 *  some or all of its attributes and children.
	 * @param merge_status receives on success the status of the merge
	 *  operation. If this item was not changed, the status is set to
	 *  MERGE_STATUS_UNCHANGED. If this item was changed, it is set to
	 *  MERGE_STATUS_MERGED. If this item was deleted, it is set to
	 *  MERGE_STATUS_DELETED.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_OUT_OF_RANGE if the specified other item has a
	 *  different primary key (i.e. m_key or m_data are different) or if some
	 *  other data has an unexpected value.
	 */
	OP_STATUS Merge(OpSyncDataItem* other, enum MergeStatus& merge_status);

	/**
	 * Create a new child or attribute item with the supplied data and
	 * properties.
	 *
	 * @param key The key for this item.
	 * @param data The data associated with the key.
	 * @param type The DataItemType of the item, should be DATAITEM_ATTRIBUTE or
	 *  DATAITEM_CHILD.
	 * @param sync_child_new If not NULL, it receives the pointer to the newly
	 *  created subitem.
	 * @return OK or ERR_NO_MEMORY
	 */
	OP_STATUS Add(const uni_char* key, const uni_char* data, DataItemType type, OpSyncDataItem** sync_child_new);

	/**
	 * Adds an item as a child to this item.
	 *
	 * @param child OpSyncDataItem to be added as a child of this item.
	 * @return OK or ERR_NO_MEMORY
	 */
	OP_STATUS AddChild(OpSyncDataItem* child);

	/**
	 * Adds an item as a child to this item.
	 *
	 * @param key Key to identify the data.
	 * @param data The data to set.
	 * @param sync_child_new Pointer to the created DataItem.
	 * @return OK or ERR_NO_MEMORY
	 */
	OP_STATUS AddChild(const uni_char* key, const uni_char* data, OpSyncDataItem **sync_child_new = NULL);

	/**
	 * Removes an item as a child to this item.
	 *
	 * @param key Primary key of a OpSyncDataItem to be removed as a child of
	 *  this item.
	 */
	void RemoveChild(const uni_char* key);

	/**
	 * Removes all child elements from this item.
	 */
	void RemoveAllChildren();

	/**
	 * Checks if there are children of this node.
	 *
	 * @return TRUE if this node has children.
	 */
	BOOL HasChildren() const;

	/**
	 * Returns the first child in the collection of children or 0 if the item
	 * has no children.
	 */
	OpSyncDataItem* GetFirstChild();

	/**
	 * Find a child item based on the key given.
	 *
	 * @return OpSyncDataItem to the item found, or NULL if no matching child
	 *  was found.
	 */
	OpSyncDataItem* FindChildById(const uni_char* id);

	/**
	 * Adds an item as an attribute to this item.
	 *
	 * @param attr OpSyncDataItem to be added as an attribute of this item.
	 * @return OK or ERR_NO_MEMORY
	 */
	OP_STATUS AddAttribute(OpSyncDataItem* attr);

	/**
	 * Adds an item as an attribute to this item.
	 *
	 * @param key Key to identify the data.
	 * @param data The data to set.
	 * @param sync_child_new Receives on output the pointer to the created
	 *  OpSyncDataItem.
	 * @return OK or ERR_NO_MEMORY
	 */
	OP_STATUS AddAttribute(const uni_char* key, const uni_char* data, OpSyncDataItem** sync_child_new = NULL);

	/**
	 * Removes an attribute on this item.
	 *
	 * @param key Primary key of a OpSyncDataItem to be removed as a attribute
	 *  of this item.
	 */
	void RemoveAttribute(const uni_char* key);

	/**
	 * Removes all attributes from this item.
	 */
	void RemoveAllAttributes();

	/**
	 * Checks if there are attributes for this node.
	 *
	 * @return TRUE if this node has attributes.
	 */
	BOOL HasAttributes() const;

	/**
	 * Returns the first attribute in the collection of attributes or 0 if the
	 * item has no attributes.
	 */
	OpSyncDataItem* GetFirstAttribute();

	/**
	 * Find an attribute item based on the key given.
	 *
	 * @return OpSyncDataItem to the item found, or NULL if no matching
	 *  attribute was found.
	 */
	OpSyncDataItem* FindAttributeById(const uni_char* id);

	enum DataItemStatus
	{
		DATAITEM_ACTION_NONE,			///< unknown
		DATAITEM_ACTION_ADDED,			///< item has been added
		DATAITEM_ACTION_DELETED,		///< item has been deleted
		DATAITEM_ACTION_MODIFIED,		///< item has been changed
	};

	/**
	 * Return the action on the item.
	 *
	 * @return DataItemAction definition
	 */
	DataItemStatus GetStatus() const { return (DataItemStatus)packed1.status; }

	/**
	 * Sets the action on the item.
	 *
	 * @param status The DataItemStatus to set on this item. This is used to
	 *  decide what to do about an item.
	 */
	void SetStatus(DataItemStatus status) { packed1.status = (unsigned int)status; }

	/**
	 * Sets the type of the item.
	 *
	 * @param type The DataItemType to set on this item.
	 */
	void SetType(DataItemType type) { packed1.type = (unsigned int)type; }

	/**
	 * Return the type of the item.
	 *
	 * @return DataItemType definition.
	 */
	DataItemType GetType() const { return (DataItemType)packed1.type; }

	/**
	 * Returns the base type of this OpSyncDataItem.
	 *
	 * Usually this is the same as GetType(), except for bookmarks and notes.
	 * For the folder and separator of these types the base type is
	 * DATAITEM_BOOKMARK resp. DATAITEM_NOTE. This is needed because an
	 * OpSyncDataItem of the bookmark/notes types may reference any other
	 * OpSyncDataItem of the same base type as its "previous" item (see
	 * SYNC_KEY_PREV).
	 */
	DataItemType GetBaseType() const { return BaseTypeOf(GetType()); }

	/**
	 * Gets the top-level parent of this item, if any.
	 *
	 * @return OpSyncDataItem of the parent, or NULL if this is the parent.
	 */
	inline OpSyncDataItem* GetParent() { return m_parent; }

	/**
	 * Returns the child or attribute matching the given key.
	 *
	 * @param key The key of the item to be return.
	 * @return OpSyncDataItem of the item found, or NULL if no item matches the
	 *  key.
	 */
	OpSyncDataItem* FindItemById(const uni_char* key);

	/**
	 * Returns a pointer to the data of the child or attribute matching the
	 * specified key.
	 * @param key is the key of the child/attribute for which to return the
	 *  associated data.
	 * @return the data of the child/attribute with the specified key or 0 if no
	 *  such child/attribute exists.
	 */
	const uni_char* FindData(const OpStringC& key);

	/**
	 * Returns the OpSyncDataCollection this item is currently in.
	 *
	 * To remove this instance from its associated OpSyncDataCollection you can
	 * use the following code
	 * @code
	 * OpSyncDataItem* item = ...;
	 * if (item->GetList())
	 *     item->GetList()->RemoveItem(item);
	 * @endcode
	 *
	 * @return OpSyncDataCollection of the list the item is in, or NULL if not a
	 *  node in any list.
	 */
	OpSyncDataCollection* GetList() const { return (OpSyncDataCollection*)Link::GetList(); }

	// stubs to call Link class methods:
	OpSyncDataItem* Next() { return static_cast<OpSyncDataItem*>(Link::Suc()); }
	const OpSyncDataItem* Next() const { return static_cast<const OpSyncDataItem*>(Link::Suc()); }

	/**
	 * Returns TRUE if this instance precedes the specified item in the
	 * associated OpSyncDataCollection. This is e.g. true directly after calling
	 * this->AddBefore(item).
	 */
	BOOL IsBeforeInList(OpSyncDataItem* item) const { return Link::Precedes(item); }

public:
	OpString m_key;
	OpString m_data;

	unsigned int IncRefCount() { return ++m_ref_count; }
	unsigned int DecRefCount() {
		OP_ASSERT(m_ref_count>0);
		if (--m_ref_count == 0)
		{
			OP_DELETE(this);
			return 0;
		}
		else
			return m_ref_count;
	}

private:
	/** Children associated with this entry (usually comes from XML). */
	OpSyncDataCollection* m_children;
	/** Collection of OpSyncDataItems containing attribute data associated with
	 * this entry (usually comes from XML attributes). */
	OpSyncDataCollection* m_attributes;
	OpSyncDataItem* m_parent;

	union
	{
		struct
		{
			unsigned int type:9;
			unsigned int status:9;
		} packed1; // 18 bits
		unsigned int packed1_init;
	};

	/**
	 * A single OpSyncDataItem instance can be used by different parents: it may
	 * be assigned to multiple OpSyncItem instances and it may be in one
	 * OpSyncDataCollection. The reference counter is increased on adding the
	 * OpSyncDataItem to an OpSyncItem (OpSyncItem::SetDataSyncItem()) or on
	 * adding it to an OpSyncDataCollection (OpSyncDataCollection::AddItem()).
	 *
	 * @see IncRefCount(), DecRefCount()
	 */
	unsigned int m_ref_count;
};

/** This is a compound class used to encapsulate OpSyncDataItem. */
class OpSyncItem : private Link
{
	friend class OpSyncCoordinator;
	friend class OpSyncCollection;

public:
	enum Key
	{
		/**
		 * The element IDs should be represented as a client assigned UUID hex
		 * encoded into a 32 bytes upper case string. Note, however, that:
		 * - For compatibility reasons, the hex encoded string MUST be upper
		 *   case.
		 * - The UUID must consist exclusively of hexadecimal digits, without
		 *   dashes, curly brackets or any other characters (only 32
		 *   characters).
		 * @note clients should be liberal in what they accept. They shouldn't
		 *  validate, let alone try to fix, any IDs received from the
		 *  server. IDs from the server should be both stored and sent back as
		 *  they were received. Certain hardcoded limits (like only having 50
		 *  bytes for storing each ID) are ok though. See DSK-278557 for
		 *  discussion and rationale.
		 */
		SYNC_KEY_ID,

		/**
		 * Author of the extension. Used to populate the extension manager before
		 * the extension is fetched from the addons server.
		 **/
		SYNC_KEY_AUTHOR,

		/**
		 * The "content" element communicates the content of a note or
		 * a urlfilter. This element should only contain character data.
		 *
		 * @note It is recommended that clients use xml:space="preserve" on the
		 *  content tag to preserve any formatting.
		 */
		SYNC_KEY_CONTENT,

		/**
		 * The "created" attribute of a bookmark communicates a date and time
		 * that the user created the bookmark. The element should contain a date
		 * in RFC 3339 format.
		 */
		SYNC_KEY_CREATED,

		/**
		 * The "custom_title" element indicates the custom title set by the user, 
		 * if any. If not set, there is no custom title, and the title contained 
		 * in the "title" field should be used.
		 */
		SYNC_KEY_CUSTOM_TITLE,

		/**
		 * Specifies if the folder is deletable on the client. This should
		 * normally be 1 as the server should recreate the folder when new items
		 * appear in it. Valid values are 0 or 1.
		 *
		 * There are some special rules for deleting target folders:
		 *
		 * - Clients should show a warning dialog before deleting one,
		 *   explaining that all data inside it will be deleted from the device
		 *   (e.g. all bookmarks in the Opera Mini folder will be deleted from
		 *   the phones if the Opera Mini folder is deleted from Desktop). The
		 *   wording for Desktop as of 9.62 is "Deleting the Opera Mini folder
		 *   will delete all bookmarks on the mobile phone. Do you want to
		 *   continue?", but there should be another, generic message for target
		 *   folders that were created after that client version was released.
		 * - Target folders CAN NOT be moved to Trash, they have to be deleted
		 *   right away.
		 * - The server will send a normal delete command for the folder to any
		 *   client that sees the whole dataset (i.e. that could see the target
		 *   folder as a folder), but will send a delete for every item inside
		 *   it to the device that has the target folder as its root folder.
		 */
		SYNC_KEY_DELETABLE,

		/**
		 * The "description" element of a bookmark communicates the textual
		 * description that the user has given to this bookmark. The element
		 * should contain only character data.
		 * When part of an extension, contains the description of the extension
		 * as a placeholder for the extension manager before the extension
		 * is fetched from the addons server.
		 * @note It is recommended that clients use xml:space="preserve" on the
		 *  tag to preserve any formatting.
		 */
		SYNC_KEY_DESCRIPTION,

		SYNC_KEY_ENCODING,

		/**
		 * The "extension_id" is element used for the speed dial item.
		 * It is non-empty if the Speed Dial comes from extension. The value
		 * is the extension global unique id (stored in config.xml)
		 */
		SYNC_KEY_EXTENSION_ID,

		/**
		 * URL of the extension in the addons system.
		 */
		SYNC_KEY_EXTENSION_UPDATE_URI,

		/** "form_data" element, child of DATAITEM_PM_FORM_AUTH. */
		SYNC_KEY_FORM_DATA,

		/** "form_url" element, child of DATAITEM_PM_FORM_AUTH. */
		SYNC_KEY_FORM_URL,
		SYNC_KEY_GROUP,
		SYNC_KEY_HIDDEN,

		/**
		 * The "icon" element of a bookmark communicates the PNG version of the
		 * .ico favicon file associated with this bookmark. The format MUST be
		 * PNG and it MUST be Base64 encoded. If multiple sized versions are
		 * available, then the server should be given the biggest icon. It is
		 * recommended that this is a read-only element for most clients, and a
		 * write+read element on desktop.
		 */
		SYNC_KEY_ICON,

		SYNC_KEY_IS_POST,
		SYNC_KEY_KEY,

		/**
		 * The "last_read_item" element is the id for the lastest feed item
		 * considered read. The element should contain an item id. If the item
		 * doesn't have an id, the calculated id for the item will be "uri +
		 * title".
		 */
		SYNC_KEY_LAST_READ,

		SYNC_KEY_LAST_TYPED,

		/**
		 * How many bookmarks is the user allowed to copy to this folder. The
		 * special value "0" means no limit.
		 */
		SYNC_KEY_MAX_ITEMS,

		SYNC_KEY_MODIFIED,

		/**
		 * When the user moves/drags a bookmark TO this folder, it should act
		 *like a copy (not move as usually). Valid values are 0 or 1.
		 *
		 * @note Moving FROM the target folder doesn't differ regardless of this
		 *  attribute.
		 */
		SYNC_KEY_MOVE_IS_COPY,

		/**
		 * The "nickname" element of a bookmark communicates the nickname that
		 * the user has given to this bookmark. The element should only contain
		 * character data.
		 *
		 * If multiple bookmarks share the same nickname, last edit wins. Any
		 * other bookmarks with that nickname get the nickname removed and all
		 * clients marked as dirty to ensure all relevant bookmarks are updated
		 * correctly.
		 */
		SYNC_KEY_NICKNAME,

		/** "page_url" element, child of DATAITEM_PM_FORM_AUTH. */
		SYNC_KEY_PAGE_URL,

		/**
		 * The "panel_pos" element of a bookmark communicates the position the
		 * bookmark should have inside the panel, if show at all. Positions
		 * start with 1.
		 *
		 * If there is no data available, clients should assume a value of -1
		 * (invalid position).
		 */
		SYNC_KEY_PANEL_POS,

		/**
		 * The "parent" attribute of a bookmark, bookmark_folder or
		 * bookmark_separator item communicates the unique id of the parent
		 * bookmark_folder item for this item.
		 * If the attribute is empty, the item is supposed to reside in the root
		 * folder.
		 */
		SYNC_KEY_PARENT,

		/**
		 * The "partner_id" attribute is used for the "speeddial2" element. It
		 * is non-empty if the Speed Dial comes from a partner (i.e. it's a
		 * "default Speed Dial", as opposed to the user creating it). The value
		 * is the id for that partner Speed Dial.
		 */
		SYNC_KEY_PARTNER_ID,

		/** "password" element, child of DATAITEM_PM_FORM_AUTH and
		 * DATAITEM_PM_HTTP_AUTH. */
		SYNC_KEY_PASSWORD,

		/**
		 * The "scope" attribute of a pm_form_auth specifies if the user
		 * selected to use the password only on one page or on all pages on the
		 * same server. Valid values are "page" and "server".
		 */
		SYNC_KEY_PM_FORM_AUTH_SCOPE,

		/**
		 * The "personal_bar_pos" element of a bookmark communicates the
		 * position the bookmark should have inside the personal bar, if shown
		 * at all. Positions start with 1.
		 *
		 * If there is no data available, clients should assume a value of -1
		 * (invalid position).
		 */
		SYNC_KEY_PERSONAL_BAR_POS,

		SYNC_KEY_POSITION,
		SYNC_KEY_POST_QUERY,

		/**
		 * The "previous" attribute of a bookmark, bookmark_folder or
		 * bookmark_separator communicates the unique id of the previous
		 * item in the list. This item may be a "bookmark", a "bookmark_folder"
		 * or a "bookmark_separator". It can also be empty, in which case the
		 * element is supposed to be the first in its containing folder.
		 */
		SYNC_KEY_PREV,

		SYNC_KEY_RELOAD_ENABLED,
		SYNC_KEY_RELOAD_INTERVAL,
		SYNC_KEY_RELOAD_ONLY_IF_EXPIRED,
		SYNC_KEY_RELOAD_POLICY,

		/**
		 * If the user allowed to create separators beneath this folder. Valid
		 * values are 0 or 1.
		 */
		SYNC_KEY_SEPARATORS_ALLOWED,

		/**
		 * The "show_in_panel" element of a bookmark communicates if the
		 * bookmark should be shown in the panel or not. Valid values are 0 or
		 * 1.
		 *
		 * If there is no data available, clients should assume a value of 0.
		 */
		SYNC_KEY_SHOW_IN_PANEL,

		/**
		 * The "show_in_personal_bar" element of a bookmark communicates if the
		 * bookmark should be shown in the personal bar or not. Valid values are
		 * 0 or 1.
		 *
		 * If there is no data available, clients should assume a value of 0.
		 */
		SYNC_KEY_SHOW_IN_PERSONAL_BAR,

		SYNC_KEY_STATUS,

		/**
		 * If the user allowed to create sub-folders beneath this folder. Valid
		 * values are 0 or 1.
		 */
		SYNC_KEY_SUB_FOLDERS_ALLOWED,

		/**
		 * Marks the folder as being the "container" for the given target. For
		 * example, a bookmark_folder marked with the target "mini" will be the
		 * folder containing all the Opera Mini bookmarks.
		 *
		 * Currently known targets are:
		 * - "mini": The "mini" target is defined for Opera Mini usage. It will
		 *   be changed to support folders and separators, and every Bream-based
		 *   browser (including Opera Mobile) will use this target.
		 * - "desktop": The "desktop" target is defined for Opera Desktop
		 *   usage. It's not needed in most cases, but in some it is required to
		 *   avoid receiving information not relevant to Desktop.
		 * - "device": The "device" target is defined for Devices (the current
		 *   "TV" core profile).
		 * - "test_target": The "test_target" target is defined for testing
		 *   purposes.
		 */
		SYNC_KEY_TARGET,

		SYNC_KEY_THUMBNAIL,

		/**
		 * The "title" element of a bookmark communicates the title of the
		 * bookmark as assigned when the bookmark is created. This element
		 * should only contain character data.
		 * When part of an extension, contains the title of the extension
		 * as a placeholder for the extension manager before the extension
		 * is fetched from the addons server.
		 */
		SYNC_KEY_TITLE,

		/** "topdoc_url" element, child of DATAITEM_PM_FORM_AUTH. */
		SYNC_KEY_TOPDOC_URL,

		/**
		 * The "type" attribute of a bookmark_folder indicates the type of the
		 * folder. The only defined type, apart from the non-specified, default
		 * one, is "trash". There should be only one folder marked as
		 * type="trash" per user.
		 */
		SYNC_KEY_TYPE,

		/**
		 * The "update_interval" element is the number of seconds to wait before
		 * updating the feed.
		 */
		SYNC_KEY_UPDATE_INTERVAL,

		/**
		 * The "uri" element of a bookmark communicates the uri of the resource
		 * that this bookmark points to. This element should only contain a
		 * valid URI as specified in RFC 3986.
		 * When part of an extension, contains the uri of the extension
		 * as a placeholder for the extension manager before the extension
		 * is fetched from the addons server.
		 */
		SYNC_KEY_URI,

		/** "username" element, child of DATAITEM_PM_FORM_AUTH and
		 * DATAITEM_PM_HTTP_AUTH. */
		SYNC_KEY_USERNAME,

		/**
		 * A generic string indicating the extension's version number.
		 */
		SYNC_KEY_VERSION,

		/**
		 * The "visited" attribute of a bookmark item communicates a date and
		 * time that the user last visited the bookmark. The element should
		 * contain a date in RFC 3339 format.
		 */
		SYNC_KEY_VISITED,

		/**
		 * This is used to indicate the last enum value. The SyncModule has an
		 * array with an entry for each item. The array is allocated in
		 * SyncModule::InitItemTableL().
		 */
		SYNC_KEY_NONE		///< Special case to set an empty key.
	};

public:
	OpSyncItem(OpSyncDataItem::DataItemType type=OpSyncDataItem::DATAITEM_GENERIC)
		: m_type(type)
		, m_data_queue(0)
		, m_data_item(0)
		, m_primary_key(OpSyncItem::SYNC_KEY_NONE)
		, m_primary_key_name(0) {}

	virtual ~OpSyncItem() { SetDataSyncItem(0); }

	OP_STATUS Construct() {
		SetDataSyncItem(OP_NEW(OpSyncDataItem, (GetType())));
		RETURN_OOM_IF_NULL(GetDataSyncItem());
		return OpStatus::OK;
	}

	/**
	 * Returns the item type.
	 *
	 * @return Type of the item
	 */
	OpSyncDataItem::DataItemType GetType() const { return m_type; }

	/**
	 * Sets item data.
	 * @return OK or ERR_NO_MEMORY
	 */
	OP_STATUS SetData(Key key, const OpStringC& data);

	/**
	 * Gets item data.
	 * @param key the type of data to get.
	 * @param data the data will be set to empty (i.e data.CStr() will be NULL)
	 *  if no value was found. Otherwise it will contain the value of the key
	 *  requested.
	 *
	 * @return OK, ERR_NO_MEMORY
	 */
	OP_STATUS GetData(Key key, OpString& data) const;

	OP_STATUS SetStatus(OpSyncDataItem::DataItemStatus status);
	OpSyncDataItem::DataItemStatus GetStatus() const {
		return m_data_item ? m_data_item->GetStatus() : OpSyncDataItem::DATAITEM_ACTION_NONE;
	}

	/**
	 * Commits the current edited item and signals the sync module that you're
	 * done adding data to it. The item will be in an undefined state after this
	 * call, so do not add additional data to the item after CommitItem() has
	 * been called. The only action permitted on the item after this call is to
	 * free it.
	 *
	 * @param dirty Item is added as a "dirty" item. Do not set this to TRUE
	 *  unless you know the implications of it. Setting this to TRUE will
	 *  initiate a dirty sync, which is not really what you want in most cases.
	 * @param ordered Item is to be added into the outgoing queue ordered so it
	 *  will always be sent after any parent or previous item it might
	 *  reference. Set this to FALSE if you are already certain you provide the
	 *  module with the items sorted as there is performance overhead with
	 *  adding items with this flag set. Must be set to FALSE if dirty is set
	 *  to TRUE.
	 * @return OK or ERR_NO_MEMORY
	 */
	OP_STATUS CommitItem(BOOL dirty = FALSE, BOOL ordered = TRUE);

	void SetDataSyncItem(OpSyncDataItem* item) {
		if (m_data_item) m_data_item->DecRefCount();
		m_data_item = item;
		if (m_data_item) m_data_item->IncRefCount();
	}

	OpSyncDataItem* GetDataSyncItem() { return m_data_item; }
	const OpSyncDataItem* GetDataSyncItem() const { return m_data_item; }

	/**
	 * Set the primary key type and content for this item.
	 *
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_OUT_OF_RANGE if primary key is an empty string.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS SetPrimaryKey(Key primary_key, const uni_char* key_data);
	Key GetPrimaryKey() const { return m_primary_key; }

	void SetDataQueue(OpSyncDataQueue* data_queue) { m_data_queue= data_queue; }

	// stubs to call Link class methods:
	OpSyncItem* Next() { return static_cast<OpSyncItem*>(Link::Suc()); }
	const OpSyncItem* Next() const { return static_cast<const OpSyncItem*>(Link::Suc()); }
	BOOL IsInList() const { return Link::InList(); }
	void AddAfter(OpSyncItem* item) { Link::Follow(item); }
	void AddBefore(OpSyncItem* item) { Link::Precede(item); }

private:
	void Into(OpSyncCollection* list) { Link::Into(reinterpret_cast<Head*>(list)); }

protected:
	OpSyncDataItem::DataItemType m_type;	///< The type of this item.
	OpSyncDataQueue* m_data_queue;
	OpSyncDataItem* m_data_item;			///< The top-level data item.
	Key m_primary_key;						///< The primary key.
	const uni_char* m_primary_key_name;
	OpString m_key_data;					///< Data for the primary key.
};

/**
 * This iterator can be used to iterate over a OpSyncDataCollection resp.
 * OpSyncCollection.
 *
 * @note It is safe to remove the current item from the list while iterating.
 * @note It is not safe to insert new items into the list while iterating.
 *
 * @code
 * OpSyncDataCollection my_data_collection;
 * ...
 * for (OpSyncIterator<OpSyncDataItem> iter(my_data_collection.First()); *iter; ++iter)
 * {
 *     OpSyncDataItem* item = *iter;
 *     OpSyncDataItem::DataItemType type = iter->GetType();
 *     ...;
 * }
 * @endcode
 */
template<class T>
class OpSyncIterator
{
protected:
	T* m_current;
	T* m_next;

public:
	OpSyncIterator(T* start) { Reset(start); }

	/** Resets the iterator to the specified start. */
	T* Reset(T* start) {
		m_current = start;
		m_next = m_current ? m_current->Next() : 0;
		return m_current;
	}

	/**
	 * Moves the iterator to the next position. Children will not be iterated,
	 * you need to create a new iterator to iterate children if HasChildren() on
	 * the OpSyncDataItem returns TRUE.
	 * @return the current T* item after the iterator has moved.
	 */
	T* operator++() { return Reset(m_next); }

	/**
	 * Moves the iterator to the next position. Children will not be iterated,
	 * you need to create a new iterator to iterate children if HasChildren() on
	 * the OpSyncDataItem returns TRUE.
	 * @retval TRUE If there was a next position.
	 * @retval FALSE If there is no next position.
	 */
	BOOL Next() { return (++(*this) ? TRUE : FALSE); }

	/** Returns the current T* item. */
	T* operator*() const { return m_current; }
	T* GetDataItem() const { return m_current; }
	T* operator->() const { return m_current; }
};

typedef OpSyncIterator<OpSyncDataItem> OpSyncDataItemIterator;
typedef OpSyncIterator<OpSyncItem> OpSyncItemIterator;


#ifdef _DEBUG
# define CASE_ITEM_2_STRING(x) case OpSyncDataItem::x: return d << "OpSyncDataItem::" # x

inline Debug& operator<<(Debug& d, enum OpSyncDataItem::DataItemType t)
{
	switch (t) {
		CASE_ITEM_2_STRING(DATAITEM_GENERIC);
		CASE_ITEM_2_STRING(DATAITEM_BLACKLIST);
		CASE_ITEM_2_STRING(DATAITEM_BOOKMARK);
		CASE_ITEM_2_STRING(DATAITEM_BOOKMARK_FOLDER);
		CASE_ITEM_2_STRING(DATAITEM_BOOKMARK_SEPARATOR);
		CASE_ITEM_2_STRING(DATAITEM_CONTACT);
		CASE_ITEM_2_STRING(DATAITEM_ENCRYPTION_KEY);
		CASE_ITEM_2_STRING(DATAITEM_ENCRYPTION_TYPE);
		CASE_ITEM_2_STRING(DATAITEM_FEED);
		CASE_ITEM_2_STRING(DATAITEM_NOTE);
		CASE_ITEM_2_STRING(DATAITEM_NOTE_FOLDER);
		CASE_ITEM_2_STRING(DATAITEM_NOTE_SEPARATOR);
		CASE_ITEM_2_STRING(DATAITEM_PM_FORM_AUTH);
		CASE_ITEM_2_STRING(DATAITEM_PM_HTTP_AUTH);
		CASE_ITEM_2_STRING(DATAITEM_SEARCH);
		CASE_ITEM_2_STRING(DATAITEM_SPEEDDIAL);
		CASE_ITEM_2_STRING(DATAITEM_SPEEDDIAL_2);
		CASE_ITEM_2_STRING(DATAITEM_SPEEDDIAL_2_SETTINGS);
		CASE_ITEM_2_STRING(DATAITEM_TYPED_HISTORY);
		CASE_ITEM_2_STRING(DATAITEM_URLFILTER);
		CASE_ITEM_2_STRING(DATAITEM_MAX_DATAITEM);
		CASE_ITEM_2_STRING(DATAITEM_ATTRIBUTE);
		CASE_ITEM_2_STRING(DATAITEM_CHILD);
	default: return d << "OpSyncDataItem::DataItemType(unknown:" << (int)t << ")";
	}
}

inline Debug& operator<<(Debug& d, enum OpSyncDataItem::DataItemStatus t)
{
	switch (t) {
		CASE_ITEM_2_STRING(DATAITEM_ACTION_NONE);
		CASE_ITEM_2_STRING(DATAITEM_ACTION_ADDED);
		CASE_ITEM_2_STRING(DATAITEM_ACTION_DELETED);
		CASE_ITEM_2_STRING(DATAITEM_ACTION_MODIFIED);
	default: return d << "OpSyncDataItem::DataItemStatus(unknown:" << (int)t << ")";
	}
}

inline Debug& operator<<(Debug& d, enum OpSyncDataItem::MergeStatus t)
{
	switch (t) {
		CASE_ITEM_2_STRING(MERGE_STATUS_UNCHANGED);
		CASE_ITEM_2_STRING(MERGE_STATUS_MERGED);
		CASE_ITEM_2_STRING(MERGE_STATUS_DELETED);
	default: return d << "OpSyncDataItem::MergeStatus(unknown:" << (int)t << ")";
	}
}

# undef CASE_ITEM_2_STRING
#endif // _DEBUG

#endif //_SYNC_DATAITEM_H_INCLUDED_
