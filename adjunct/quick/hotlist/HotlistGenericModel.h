/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 * @author Co-owner: Espen Sand (espen)
 *
 */

#pragma once

#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/desktop_util/treemodel/optreemodelitem.h"
#include "adjunct/quick/managers/FavIconManager.h"

#include "modules/pi/OpDragManager.h"
#include "modules/hardcore/timer/optimer.h"
#include "modules/history/OpHistoryModel.h"
#include "modules/history/OpHistoryPage.h"
#include "modules/util/OpHashTable.h"
#include "modules/windowcommander/OpWindowCommander.h"

#include "adjunct/quick/hotlist/HotlistModelItem.h"
#include "adjunct/quick/hotlist/HotlistModel.h"

class FileImage;
class HotlistModelItem;
class HotlistGenericModelListener;
class BookmarkModelItem;
class SeparatorModelItem;
class Image;
class URL;

#define NOTES_USE_URLICON
//#define HOTLIST_USE_MOVE_ITEM

/**
 * With MOVE_ITEM defined, we don't need all the special handling inside 
 * OnHotlistItemRemoving/Added anymore,
 * and we should be able to get rid of all SetIsMovingItems stuff
 *
 **/

/*************************************************************************
 *
 * HotlistGenericModelEmailLookupBuffer
 *
 *
 **************************************************************************/

/**
 * Helper class that stores e-mail to HotlistModelItem relationship using
 * a hash table. Speeds up IsContact() type access which M2 uses a lot.
 * The 'm_email_text' member stores strings that do not exist in the
 * contact node. That is: Strings from contact nodes with a comma separated
 * list of e-mail addresses are stored here as the hash table itself
 * does not store local copies of strings.
 */
class HotlistGenericModelEmailLookupBuffer
{
public:
	HotlistGenericModelEmailLookupBuffer()
	{
		m_email_table = OP_NEW(OpGenericStringHashTable, ());
		m_email_text = OP_NEW(OpVector<OpString>, ());
	}

	~HotlistGenericModelEmailLookupBuffer()
			{
		if( m_email_table )
		{
			m_email_table->RemoveAll();
			OP_DELETE(m_email_table);
		}
		if( m_email_text )
		{
			m_email_text->DeleteAll();
			OP_DELETE(m_email_text);
		}
	}

	OpGenericStringHashTable* table() const { return m_email_table; }
	OpVector<OpString>* text() const { return m_email_text; }

private:
	OpGenericStringHashTable* m_email_table;
	OpVector<OpString>* m_email_text;
};


/*************************************************************************
 *
 * HotlistGenericModelSearchItem
 *
 *
 **************************************************************************/

struct HotlistGenericModelSearchItem
{
	HotlistGenericModelSearchItem() { item = 0; }
	void Reset() { item = 0; key.Empty(); }

	HotlistModelItem* item;
	OpString key;
};



/*************************************************************************
 *
 * HotlistGenericModel
 *
 *
 **************************************************************************/

class HotlistGenericModel
:	public HotlistModel,
	public FavIconManager::FavIconListener
{

public:
	/**
	 * Initializes an off-screen hotlist treelist
	 */
	HotlistGenericModel( INT32 type, BOOL temp );

	/**
	 * Releases any allocated resouces belonging to this object
	 */
	virtual ~HotlistGenericModel();


	///////////////////////////////////////////////////////////////
	// Subclassing HotlistGenericModel
	virtual BOOL		      EmptyTrash();
	
	/**
	 * Returns the current trash folder
	 */
	virtual HotlistModelItem* GetTrashFolder() {return AddTrashFolder(); }

	// Functions to move items in model
	/**
	 * Deletes the entire tree and its contents
	 */
	virtual void Erase();
	
	/**
	 * Sets or clears the dirty flag of the model. A dirty flag equal to
	 * TRUE indicates that the list should be saved to disk
	 */
	virtual void SetDirty( BOOL state, INT32 timeout_ms=5000 );

	///////////////////////////////////////////////////////////////

	/**
	 * Registers and returns a node in the tree.
	 *
	 * @param item The node to register
	 *
	 * @return The node
	 */
	HotlistModelItem* AddItem(HotlistModelItem* item, BOOL allow_dups/* = TRUE*/, BOOL parsed_item/* = FALSE*/);

	/**
	 * Creates and returns a node in the tree.
	 *
	 * @param type. Can be BOOKMARK_TYPE, CONTACT_TYPE or FOLDER_TYPE
	 *
	 * @return The node, 0 on error or wrong type
	 */
	HotlistModelItem* AddItem( OpTypedObject::Type type , OP_STATUS& status, BOOL parsed_item/* = FALSE */);

	HotlistModelItem* CreateItem(OpTypedObject::Type type, OP_STATUS& status, BOOL parsed_item);

	/**
	 * Deletes the item (and any children) in the mode. The item
	 * is copied to the trash folder of the model if possible
	 *
	 * @param item The item to delete
	 * @param copy_to_trash Copy item to trash folder when TRUE
	 *
	 * @return TRUE on success, othetwise FALSE
	 */
	BOOL DeleteItem( OpTreeModelItem* item, BOOL copy_to_trash, BOOL handle_trash = FALSE, BOOL keep_gadget = FALSE ); 
	
	/**
	 * Copies (duplicates) the item and any children into the model
	 *
	 * @param item The node to copy
	 * @param maintain_flag A set of flags telling what flags not to clear
	 *        before copying the items to the model.
	 * @param copy_to_trash If TRUE, the new item will not have a new unique ID
	 *
	 * @return The identifier of the first new item that was added
	 *         to the model on success, otherwise -1
	 */
	INT32 CopyItem( OpTreeModelItem* item, INT32 maintain_flag );

	/**
	 * Copies (duplicates) the item and any children and possible siblings.
	 * Do not use this function directly. Use @ref CopyItem instead.
	 *
	 * @param model Source model
	 * @param index Index to copy from in source model
	 * @param testsibling Copy siblings if TRUE
	 * @param sibling_index Insert the duplicates in the model at this index
	 *        (before or after). Use -1 to disable this functionality
	 * @param insert_type. Specifies where the new item is to be placed.
	 * @param maintain_flag A set of flags telling what flags not to clear
	 *        before copying the items from the model.
	 * @param move_item If TRUE, the items will be moved and not copied
	 * @param copy_to_trash If TRUE, the new item will not have a new unique ID
	 *
	 * @return The identifier of the first new item that was added
	 *         to the model on success, otherwise -1
	 */
	INT32 CopyItem( HotlistModel& model, INT32 index, BOOL testsibling,int sibling_index, DesktopDragObject::InsertType insert_type, INT32 maintain_flag );

	/**
	 * Pastes the nodes in the model to the position specified
	 * by 'at'. The new nodes are placed at the end of the folder
	 * if 'at' is a folder or after 'at' if it is a regular node.
	 *
	 * @param from The model to copy from
	 * @param at The destination in this model
	 * @param insert_type. Specifies where the new item is to be placed
	 *        with respect to 'at'
	 * @param maintain_flag A set of flags telling what flags not to clear
	 *        before pasting the items from the 'from' model.
	 * @param move_item Move the item instead of copying/deleting it
	 *
	 * @return The identifier of the first new item that was added
	 *         to the model on success, otherwise -1
	 */
	INT32 PasteItem( HotlistModel& from, OpTreeModelItem* at, DesktopDragObject::InsertType insert_type, INT32 maintain_flag );

	// temporary item

	/**
	 * Creates and returns a node. The node gets the model as parent
	 * but the node is not added to the model. This function should
	 * only be used when adding new items in a process where the addition
	 * can be canceled. Use @ref RegisterTemporaryItem to make it a
	 * regular item
	 *
	 * @param type. Can be BOOKMARK_TYPE, CONTACT_TYPE or FOLDER_TYPE
	 *
	 * @return The node, 0 on error or wrong type
	 */
	HotlistModelItem* AddTemporaryItem( OpTypedObject::Type type );

	/**
	 * Adds a temporary node to the model. The internal model of the
	 * node must be the same as the this mode. The node should have
	 * been created with @ref AddTemporaryItem
	 *
	 * @param item The item to add
	 * @param parent_id The identifier of the parent.
	 *
	 * @return TRUE on success, otherwise FALSE.
	 */
	virtual BOOL RegisterTemporaryItem( OpTreeModelItem *item, int parent_id, INT32* got_id);

	/**
	 * Adds a temporary node to the model. The internal model of the
	 * node must be the same as the this mode. The node should have
	 * been created with @ref AddTemporaryItem
	 *
	 * @param item The item to add
	 * @param parent_id The identifier of the parent.
	 *
	 * @return TRUE on success, otherwise FALSE.
	 */
	BOOL RegisterTemporaryItemOrdered(OpTreeModelItem *item, HotlistModelItem* previous );
	BOOL RegisterTemporaryItemFirstInFolder(OpTreeModelItem *item, HotlistModelItem* parent );


	/**
	 * Removes a tempoary item that has been added with @ref AddTemporaryItem
	 *
	 * @param item The item to be removed. The pointer must not be
	 *        accessed after this function returns with the value of TRUE.
	 *
	 * @param TRUE if the item was removed, otherwise FALSE
	 */
	BOOL RemoveTemporaryItem( OpTreeModelItem *item );


	// Functions to move items in model

	/**
	 * Reparent the source node to the target folder
	 *
	 * @param src Source node
	 * @param target Target folder
	 * @param first_child If true src is inserted as first child of target folder
	 *
	 * @return TRUE on success, otherwise FALSE
	 */
	BOOL Reparent( HotlistModelItem *src, HotlistModelItem *target, BOOL first_child = FALSE, BOOL handle_trash = FALSE );

	/**
	 * Reorder the source node as next of new_prev item
	 *
	 * @param src Source node
	 * @param new_prev New previous item of src
	 *
	 * @return TRUE on success, otherwise FALSE
	 */
	BOOL Reorder( HotlistModelItem *src, HotlistModelItem *new_prev, BOOL handle_trash = FALSE );





	HotlistModelItem* AddTrashFolder();

	/**
	 * Sets the trashfolder id. Only to be used when parsing items from file.
	 */
	void SetTrashfolderId( INT32 id ) {	m_trash_folder_id = id;	}

	INT32 GetTrashFolderId() { return m_trash_folder_id; }

	/**
	 * Adds an item at he enof of the list but before the trash if the trash folder
	 * is the last item in the list
	 */
	void AddLastBeforeTrash( INT32 parent_index, HotlistModelItem* item, INT32* got_index );


	void AddGadgetToModel(HotlistModelItem* item, INT32* got_index);

	/**
	 * Returns TRUE if this item is descendant of item
	 **/
	BOOL GetIsDescendantOf(HotlistModelItem* descendant, HotlistModelItem* item);

	UINT32 NumUniteGadgets(BOOL running_only = FALSE);

	
	/**
	 * Returns the first contact item that holds the same e-mail address
	 * as the argument
	 *
	 * @param address The e-mail address to locate
	 *
	 * @param A pointer to the first item in the list that match or 0 on
	 *        no match
	 */
	HotlistModelItem *GetByEmailAddress( const OpStringC &address );

#ifdef HOTLIST_USE_MOVE_ITEM
	INT32 MoveItem( HotlistGenericModel* from_model, INT32 from_index, HotlistModelItem* to, InsertType insert_type);
#endif

	// TreeModel
	INT32 GetColumnCount();
	OP_STATUS GetColumnData(ColumnData* column_data);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	OP_STATUS GetTypeString(OpString& type_string);
#endif


	// GADGETS ///////////////////////////////////////////////////

	void HandleGadgetCallback(OpDocumentListener::GadgetShowNotificationCallback* gadget, 
							  OpDocumentListener::GadgetShowNotificationCallback::Reply);

	BOOL IsDroppingGadgetAllowed(HotlistModelItem* item, HotlistModelItem* to, DesktopDragObject::InsertType insert_type);

#ifdef GADGET_SUPPORT
	/**
	 * Returns the item with specified gadget identifier
	 *
	 * @param gadget_id The unique gadget identifier
	 *
	 * @return A pointer to the first item in the list that match or NULL on
	 *        no match
	 */	
	HotlistModelItem* GetByGadgetIdentifier(const OpStringC &gadget_id);
#endif // GADGET_SUPPORT

#ifdef GADGET_SUPPORT
	/**
	 * Returns the number of HotlistModelItems with specified gadget identifier.
	 *
	 * @param gadget_id The unique gadget identifier
	 * @param include_items_in_trash If true, items in that are deleted (in trash folder) are counted too
	 * 
	 * @return the number of gadget model items in the model that refer to the unique ID
	 *			ie. the number of references to the gadget installation
	 *
	 */	
	UINT32 GetGadgetCountByIdentifier(const OpStringC &gadget_id, BOOL include_items_in_trash = TRUE);
#endif // GADGET_SUPPORT

	// FavIconManager::FavIconListener ---------------------------------------------

	virtual void OnFavIconAdded(const uni_char* document_url, const uni_char* image_path);
	virtual void OnFavIconsRemoved() {}


#ifdef WEBSERVER_SUPPORT
	UniteServiceModelItem* GetRootService() { return m_root_service; }
	void SetRootService(UniteServiceModelItem* root) { if (!m_root_service)  m_root_service = root; }
	void ResetRootService() { if (m_root_service) m_root_service = NULL; }
#endif

private:
	SeparatorModelItem* AddSpecialSeparator(UINT32 type);
	void AddRootServiceSeparator(HotlistModelItem* previous);

	/**
	 * Creates a hash table that is used to speed up e-mail to item lookup
	 */
	void CreateEmailLookupTable();

	/**
	 * Empties hash table that is used tp speed up e-mail to item lookup
	 * Must always be called when list content changes
	 */
	void InvalidateEmailLookupTable();



private:
	HotlistGenericModelSearchItem m_current_search_item; // Speed up testing
	HotlistGenericModelEmailLookupBuffer *m_email_buffer;

	INT32 m_trash_separator_id;
	INT32 m_trash_folder_id;

#ifdef WEBSERVER_SUPPORT
	UniteServiceModelItem* m_root_service;
#endif	
};
