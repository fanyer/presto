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

#ifndef __HOTLIST_MODEL_ITEM_H__
#define __HOTLIST_MODEL_ITEM_H__

#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick/managers/WebServerManager.h"
#include "adjunct/quick/models/CollectionModel.h"
#include "adjunct/quick/windows/DesktopGadget.h"

#include "modules/hardcore/timer/optimer.h"
#include "modules/history/OpHistoryModel.h"
#include "modules/history/OpHistoryPage.h"
#include "modules/pi/OpDragManager.h"
#include "modules/util/OpHashTable.h"
#include "modules/windowcommander/OpWindowCommander.h"

#ifdef SUPPORT_DATA_SYNC
# include "modules/sync/sync_dataitem.h"
#endif // SUPPORT_DATA_SYNC

#ifdef M2_SUPPORT
#include "adjunct/m2/src/include/defs.h"
#endif // M2_SUPPORT

class HotlistGenericModel;
class FolderModelItem;
class SimpleFileImage;
class HotlistModel;
class Image;
class URL;

#define NOTES_USE_URLICON

void BuildTimeString( OpString &dest, time_t at, BOOL use24Hour );

/*************************************************************************
 *
 * HotlistModelItem
 *
 **************************************************************************/

class HotlistModelItem : public TreeModelItem<HotlistModelItem, HotlistModel>
					   , public CollectionModelItem
{
public:
	enum SortOptions
	{
		CASE_SENSITIVE  = 0x01,
		ENTIRE_STRING   = 0x02,
		START_OF_STRING = 0x04,
		IN_URLS         = 0x10
	};

public:
	/**
	 * Initializes an element in the off-screen hotlist treelist
	 */
	HotlistModelItem();

	/**
	 * Releases any allocated resouces belonging to this object
	 */
	virtual ~HotlistModelItem();


	/**
	 * Creates a duplicate of the node and returns it.
	 *
	 * @param The duplicated item will have the same identifier 
	 *        when TRUE. Useful in move operations
	 *
	 * @returns The duplicated item
	 */
	virtual HotlistModelItem *GetDuplicate();

	// CollectionModelItem
	virtual const uni_char* GetItemTitle() const { return GetName(); }
	virtual OP_STATUS GetItemUrl(OpString& str, bool display_url = true) const;
	virtual int GetItemID() const { return const_cast<HotlistModelItem*>(this)->GetID(); }
	virtual bool IsExcluded() const { return GetIsTrashFolder() || const_cast<HotlistModelItem*>(this)->IsSeparator(); }
	virtual bool IsItemFolder() const { return !!const_cast<HotlistModelItem*>(this)->IsFolder(); }
	virtual CollectionModelItem* GetParent() const { return GetParentFolder(); }
	virtual bool IsInDeletedFolder() const { return !!GetIsInsideTrashFolder(); }
	virtual CollectionModel* GetItemModel() const;
	virtual bool IsDeletedFolder() { return !!this->GetIsTrashFolder(); }

	virtual void SetName( const uni_char* str ) {}
	virtual void SetUrl( const uni_char* str ) {}
	virtual void SetDisplayUrl( const uni_char* str ) {}
	virtual void SetDescription( const uni_char* str ) {}
	virtual void SetShortName( const uni_char* str ) {}
	virtual void SetPartnerID( const uni_char* str ) {}
	virtual void ClearShortName() { }
	virtual BOOL HasNickname(const OpStringC& nick) { return FALSE; }

	virtual void SetUniqueGUID( const uni_char* str, INT32 model_type );
	virtual void SetCreated( time_t value ) {}
	virtual void SetVisited( time_t value ) {}
	virtual void SetPersonalbarPos( INT32 pos ) {}
	virtual void SetPanelPos( INT32 pos ) { }
	virtual void SetOnPersonalbar( BOOL state ) {}
	virtual void SetSmallScreen( BOOL state ) {}
	virtual void SetUpgradeIcon( BOOL state ) {}
	virtual void SetIsActive( BOOL state ) {}

	// Use to lock status if sending when sending add/delete/change event but item
	// shouldn't be synced.

	// Fields that are received from the Link server
	// This should never be set to non-default values by the client itself
	// and neither should they be sent to the Link server 
	virtual void SetTarget( const uni_char* str ) {}
	virtual void ClearTarget() {}
	virtual void SetHasMoveIsCopy(BOOL copy) {  }
	virtual void SetIsDeletable(BOOL deletable) {  }
	virtual void SetSubfoldersAllowed(BOOL subfolders_allowed) { }
	virtual void SetSeparatorsAllowed(BOOL separators_allowed) { }
	virtual void SetMaxItems(INT32 max_num_items) { }

	// Special folder attributes
	virtual void SetIsTrashFolder( BOOL state ) {}
	virtual void SetIsExpandedFolder( BOOL state ) {}

	// Compatibility with pre 7 bookmark files
	virtual void SetIsLinkBarFolder( BOOL state ) {}

	// Contacts
	virtual void SetMail( const uni_char* str ) {}
	virtual void SetPhone( const uni_char* str ) {}
	virtual void SetFax( const uni_char* str ) {}
	virtual void SetPostalAddress( const uni_char* str ) {}
	virtual void SetPictureUrl( const uni_char* str ) {}
	virtual void SetIconName( const char* str ) { }
	virtual void SetConaxNumber( const uni_char* str ) {}
	virtual void SetImAddress( const uni_char* str ) {}
	virtual void SetImProtocol( const uni_char* str ) {}
	virtual void SetImStatus( const uni_char* str ) {}
	virtual void SetImVisibility( const uni_char* str ) {}
	virtual void SetImXPosition( const uni_char* str ) {}
	virtual void SetImYPosition( const uni_char* str ) {}
	virtual void SetImWidth( const uni_char* str ) {}
	virtual void SetImHeight( const uni_char* str ) {}
	virtual void SetImMaximized( const uni_char* str ) {}
	virtual void SetM2IndexId( const index_gid_t index_id ) {}
	virtual void ChangeImageIfNeeded() {}

#ifdef GADGET_SUPPORT
	//gadget stuff
	virtual void SetGadgetIdentifier( const uni_char* str ) {}	//unique gadget identifier used in gadget class
	virtual void SetGadgetCleanUninstall( const BOOL clean ) {}	//type of installation (downloaded from web, local copy etc)
#endif // GADGET_SUPPORT

	virtual void SetGroupList( const uni_char* str ) {}

	// Store tags we no longer use but want to save on exit
	// to maintain backward compatibility
	void SetUnknownTag( const uni_char *tag );

	// Set marked state
	void  SetIsMarked(BOOL marked) { m_marked = marked; }

	virtual UINT32 GetSeparatorType() const { return 0; }

	/**
	 * Functions for reading data read from file
	 */
	virtual const OpStringC &GetName() const { return m_dummy; }
	virtual const OpStringC &GetTarget() const { return m_dummy; }
	virtual const OpStringC &GetUniqueGUID() const { return m_unique_guid; }
	virtual const OpStringC &GetMenuName();
	virtual const OpStringC &GetUrl() const { return m_dummy; }
	virtual const OpStringC &GetDisplayUrl() const { return m_dummy; }
	virtual const OpStringC &GetResolvedUrl() { return m_dummy; }
	virtual const OpStringC &GetDescription() const { return m_dummy; }
	virtual const OpStringC &GetShortName() const { return m_dummy; }
	virtual const OpStringC &GetPartnerID() const { return m_dummy; }
	virtual const OpStringC &GetCreatedString() { return m_dummy; }
	virtual const OpStringC &GetVisitedString() { return m_dummy; }
	
	virtual time_t GetCreated() const { return 0; }
	virtual time_t GetVisited() const { return 0; }
	virtual INT32 GetPersonalbarPos() const { return -1;}
	virtual BOOL  GetOnPersonalbar() const { return GetPersonalbarPos() >= 0;}
	virtual INT32 GetPanelPos() const { return -1; }
	virtual BOOL  GetInPanel() const { return GetPanelPos() >= 0; }
	virtual BOOL  GetSmallScreen() const { return FALSE; }
	virtual BOOL  GetUpgradeIcon() const { return FALSE; }

	virtual BOOL  GetIsTrashFolder() const { return FALSE; }
	virtual BOOL  GetIsSpecialFolder() const { return FALSE; }

	virtual BOOL ClearVisited(){return FALSE;}
	virtual BOOL ClearCreated(){return FALSE;}

	// Whether the item has a different display url, if not GetDisplayUrl()
	// should returns the same as GetUrl()
	virtual BOOL GetHasDisplayUrl() const {return FALSE;}

	// Returns true for the folder that actually has this property (target)
	virtual BOOL GetHasMoveIsCopy() const { return FALSE; }	

#ifdef WEBSERVER_SUPPORT
	virtual BOOL IsRootService() const { return FALSE; }
#endif

	// Returns parent folder with the property set if it is in a folder that has 
	// this property or it is the folder with this property
	virtual HotlistModelItem *GetIsInMoveIsCopy();	
	virtual FolderModelItem *GetIsInMaxItemsFolder();
	virtual BOOL GetIsDeletable()   const { return TRUE; }
	virtual BOOL GetSubfoldersAllowed() const { return TRUE; }
	virtual BOOL GetSeparatorsAllowed() 
	{
		return TRUE; 
	}

	virtual INT32 GetMaxItems() const { return -1; }

	virtual BOOL  GetIsActive() const { return FALSE; }
	virtual BOOL  GetIsExpandedFolder() const { return FALSE; }

	// Compatibility with pre 7 bookmark files
	virtual BOOL  GetIsLinkBarFolder() const { return FALSE; }

	virtual const OpStringC &GetMail() const { return m_dummy; }
	virtual const OpStringC &GetPhone() const { return m_dummy; }
	virtual const OpStringC &GetFax() const { return m_dummy; }
	virtual const OpStringC &GetPostalAddress() const {return m_dummy;}
	virtual const OpStringC &GetPictureUrl() const { return m_dummy; }
	virtual const OpStringC &GetConaxNumber() const { return m_dummy; }
	virtual const OpStringC &GetImAddress() const { return m_dummy; }
	virtual const OpStringC &GetImProtocol() const { return m_dummy; }
	virtual const OpStringC &GetImStatus() const { return m_dummy; }
	virtual const OpStringC &GetImVisibility() const { return m_dummy; }
	virtual const OpStringC &GetImXPosition() const { return m_dummy; }
	virtual const OpStringC &GetImYPosition() const { return m_dummy; }
	virtual const OpStringC &GetImWidth() const { return m_dummy; }
	virtual const OpStringC &GetImHeight() const { return m_dummy; }
	virtual const OpStringC &GetImMaximized() const { return m_dummy; }
	virtual index_gid_t GetM2IndexId( BOOL create_if_null = FALSE ) { return 0; };

#ifdef GADGET_SUPPORT
	//gadget properties, see description above
	virtual const OpStringC &GetGadgetIdentifier() const { return m_dummy; }
	virtual BOOL GetGadgetCleanUninstall() const { return FALSE; }
#endif // GADGET_SUPPORT

	// A couple of extra functions for smoother mail handling.
	virtual OP_STATUS GetPrimaryMailAddress(OpString& address) { return OpStatus::ERR; }
	virtual OP_STATUS GetMailAddresses(OpAutoVector<OpString>& addresses) { return OpStatus::ERR; }

	virtual const OpStringC &GetGroupList() const { return m_dummy; }

	const OpStringC &GetUnknownTags() const { 
		return m_unknown_tags; 
	}
	virtual void  GetInfoText( OpInfoText &text ) {}

	/**
	 * Returns the image name of the icon that shall be used
	 * to represent this node. The returned value can be 0
	 */
	virtual const char* GetImage() const { return 0; }

	/**
	 * Returns the image that can be used when COLUMN_QUERY
	 */
	virtual Image GetIcon() {  return Image(); }

	/**
	 * Reimplemented from OpTreeModelItem
	 * Returns element data for this object
	 *
	 * @param itemData Data for this object is placed here on return
	 *
	 * @return OP_STATUS value for success of failure
	 */
	OP_STATUS GetItemData( ItemData* item_data );

	/**
	 * Returns the element type. Must be reimplemented to be of any value.
	 */
	Type GetType() { return UNKNOWN_TYPE; }

	/**
	 * Returns the unique element id
	 */
	INT32 GetID() { return m_global_id; }

	/**
	 * Returns the element id of the item this is a copy of, or its own id
	 */
	INT32 GetHistoricalID() { return m_historical_id; }

	/**
	 * Returns the marked state
	 */
	BOOL GetIsMarked() const { return m_marked; }

	/**
	 * Returns TRUE if this node is inside a trash folder
	 */
	BOOL GetIsInsideTrashFolder() const;

	/**
	 * Returns a pointer to the parent folder in the model that contains
	 * this node or 0 of there is no such folder
	 */
	HotlistModelItem *GetParentFolder() const;


	/**
	 * Returns the id of the parent folder in the model that contains
	 * this node or -1 if there is no such folder
	 */
	INT32 GetParentID();

	/**
	 * Rrturns TRUE if the item is a child or grandchild etc of the
	 * 'candidate_parent' element
	 */
	BOOL IsChildOf( HotlistModelItem* candidate_parent );

	/**
	 * Easy to use functions for testing node type
	 */
	BOOL IsFolder() { return GetType() == FOLDER_TYPE; }
	BOOL IsBookmark() { return GetType() == BOOKMARK_TYPE; }
	BOOL IsContact() { return GetType() == CONTACT_TYPE; }
	BOOL IsNote() { return GetType() == NOTE_TYPE; }
	BOOL IsGroup() { return GetType() == GROUP_TYPE; }
#ifdef GADGET_SUPPORT
	BOOL IsGadget() { return GetType() == GADGET_TYPE; }
	BOOL IsUniteService() { return GetType() == UNITE_SERVICE_TYPE; }
#endif // GADGET_SUPPORT
	BOOL IsSeparator() { return GetType() == SEPARATOR_TYPE; }

	/**
	 * Compares strings and return TRUE or FALSE depending on
	 * the strings match. The compare method is modified by
	 * mode
	 *
	 * @param haystack String one
	 * @param needle String two
	 * @param mode Search mode. Use values from enum SortOptions
	 *
	 * @return TRUE on match, otherwise FALSE
	 */
	static BOOL CompareStrings( const uni_char* haystack, const uni_char* needle, int mode );

	void SetTemporaryModel(HotlistModel* temporary_model) {m_temporary_model = temporary_model;}
	HotlistModel* GetTemporaryModel() {return m_temporary_model;}

	/**
	 * Returns TRUE if this node contains the mail address, otherwise FALSE.
	 *
	 * @param address The address to look for.
	 *
	 * @return TRUE on match, otherwise FALSE
	 */
	BOOL ContainsMailAddress( const OpStringC& address );

	// GenericTreeModelItem
	// virtual void OnRemoving();
	// virtual void OnAdded();

#ifdef SUPPORT_DATA_SYNC
	UINT32 GetChangedFields() { return m_changed_fields; }
#ifdef SUPPORT_DATA_SYNC
	void SetChangedFields(UINT32 changed_fields) { m_changed_fields = changed_fields; }
	OpSyncDataItem::DataItemStatus GetLastSyncStatus() { return m_last_sync_status; }
	void SetLastSyncStatus(OpSyncDataItem::DataItemStatus status) { m_last_sync_status = status; }
#endif

protected:

	/**
	 * Copy constructor
	 */
	HotlistModelItem( const HotlistModelItem& item );
	OpString	m_unique_guid;	// Unique ID of the item

private:

	HotlistModelItem& operator=(const HotlistModelItem& item);

	static OpString m_dummy;
	static INT32 m_id_counter;

	HotlistModel* m_temporary_model;  // set when item is about to be added to this model
	INT32 m_global_id;             // Global unique id
	INT32 m_historical_id;         // Id of element this was copied from
	OpString m_unknown_tags;       // List of unkown tags read from file.
	BOOL m_marked;                 // Flag which is used when scanning for specific items.
	BOOL        m_moved_as_child;
#ifdef SUPPORT_DATA_SYNC
	UINT32		m_changed_fields;	// used on delayed synchronization to know what fields have changed
#endif

	OpSyncDataItem::DataItemStatus m_last_sync_status;	// last status on the item
#endif
};



/*************************************************************************
 *
 * GenericModelItem
 *
 *
 *
 **************************************************************************/

class GenericModelItem : public HotlistModelItem
{
public:
	/**
	 * Initializes an element in the off-screen hotlist treelist
	 */
	GenericModelItem();

	/**
	 * Releases any allocated resouces belonging to this object
	 */
	~GenericModelItem();


	//protected:
	/**
	 * Functions for setting data read from file
	 */
	void SetName( const uni_char* str );
	void SetUrl( const uni_char* str )
	{
		m_url.Set(str);
		m_resolved_url.Empty();
		// Notify
	}

	void SetDisplayUrl( const uni_char* str ) 
	{
		m_display_url.Set(str);
	}

	void SetDescription( const uni_char* str )
	{
		m_description.Set(str);
	}

	void SetShortName( const uni_char* str );

	void ClearShortName( )
	{
		m_shortname.Empty();
	}

	void SetCreated( time_t value );
	void SetVisited( time_t value );

	void SetPersonalbarPos( INT32 pos )
	{
		m_personalbar_position = pos;
	}

	void SetPanelPos( INT32 pos )
	{
		m_panel_position = pos;
		//if (pos != -1)
		//m_inpanel = TRUE;
	}

	void SetSmallScreen( BOOL state )
	{
		m_smallscreen = state;
	}

	void SetUpgradeIcon( BOOL state ) 
	{
		m_upgrade_icon = state;
	}

	/**
	 * Functions for reading data read from file
	 */
	const OpStringC &GetName() const { return m_name; }
	const OpStringC &GetUrl() const { return m_url; }
	const OpStringC &GetDisplayUrl() const { return m_display_url.HasContent() ? m_display_url : m_url; }
	const OpStringC &GetResolvedUrl();
	const OpStringC &GetDescription() const { return m_description; }
	const OpStringC &GetShortName() const { return m_shortname; }
	const OpStringC &GetCreatedString();
	const OpStringC &GetVisitedString();

	time_t GetCreated() const { return m_created; }
	time_t GetVisited() const { return m_visited; }
	INT32 GetPersonalbarPos() const { return m_personalbar_position; }
	INT32 GetPanelPos() const { return m_panel_position; }
	BOOL  GetSmallScreen() const { return m_smallscreen; }
	BOOL  GetUpgradeIcon() const { return m_upgrade_icon; }
	void  GetInfoText( OpInfoText &text );
	virtual BOOL GetSeparatorsAllowed();

	virtual BOOL ClearVisited();
	virtual BOOL ClearCreated();

	virtual BOOL GetHasDisplayUrl() const { return m_display_url.HasContent(); }
protected:

	/**
	 * Copy constructor
	 */
	GenericModelItem( const GenericModelItem& item );

protected:

	OpString	m_name;
	OpString	m_url;
	OpString	m_display_url;
	OpString	m_resolved_url;
	OpString	m_description;
	time_t		m_created;
	time_t		m_visited;
	INT32		m_personalbar_position;
	INT32		m_panel_position;
	BOOL		m_smallscreen;
	BOOL        m_upgrade_icon;
	OpString    m_shortname;
	OpString	m_created_string;
	OpString	m_visited_string;
};


/*************************************************************************
 *
 * FolderModelItem
 *
 *
 **************************************************************************/

class FolderModelItem : public GenericModelItem
{
public:


	/**
	 * Initializes an element in the off-screen hotlist treelist
	 */
	FolderModelItem();
	FolderModelItem(INT32 dtype);

	/**
	 * Releases any allocated resouces belonging to this object
	 */
	~FolderModelItem();

	/**
	 * Creates a duplicate of the node and returns it
	 */
	HotlistModelItem *GetDuplicate( );

	/**
	 * Functions for setting data read from file
	 */
	void SetIsTrashFolder( BOOL state )
	{
		m_is_trashfolder = state;
	}

	void SetIsActive( BOOL state )
	{
		m_is_activefolder = state;
	}

	void SetIsExpandedFolder( BOOL state )
	{
		m_is_expandedfolder = state;
	}

	// Compatibility with pre 7 bookmark files
	void SetIsLinkBarFolder( BOOL state )
	{
		m_is_linkbarfolder = state;
	}

	/**
	 * Functions for reading data read from file
	 */
	BOOL GetIsTrashFolder() const { return m_is_trashfolder; }

	BOOL GetIsSpecialFolder() const 
	{
		return GetTarget().HasContent();
	}

	BOOL GetIsActive() const { return m_is_activefolder; }
	BOOL GetIsExpandedFolder() const { return m_is_expandedfolder; }
	// Compatibility with pre 7 bookmark files
	BOOL GetIsLinkBarFolder() const { return m_is_linkbarfolder; }
	void  GetInfoText( OpInfoText &text );
	
	virtual const OpStringC &GetTarget() const { return m_target; }
	virtual BOOL  GetHasMoveIsCopy() const { return m_move_is_copy; }
	virtual BOOL  GetIsDeletable()   const { return m_deletable; }
	virtual BOOL  GetSubfoldersAllowed() const { return m_subfolders_allowed; }
	virtual BOOL  GetSeparatorsAllowed() { 
		return m_separators_allowed; 
	}
	virtual INT32 GetMaxItems() const { return m_max_items; }

	virtual void SetTarget( const uni_char* str )
	{
		m_target.Set(str);
	}
	
	virtual void ClearTarget()
	{
		SetTarget(NULL);
		SetIsDeletable(TRUE);
		SetSubfoldersAllowed(TRUE);
		SetSeparatorsAllowed(TRUE);
		SetHasMoveIsCopy(FALSE);
		SetMaxItems(-1);
	}

	virtual void SetHasMoveIsCopy(BOOL copy) { 
		m_move_is_copy = copy; 
	}
	virtual void SetIsDeletable(BOOL deletable) { 
		m_deletable = deletable; 
	}
	virtual void SetSubfoldersAllowed(BOOL subfolders_allowed) { 
		m_subfolders_allowed = subfolders_allowed; 
	}
	virtual void SetSeparatorsAllowed(BOOL separators_allowed) { 
		m_separators_allowed = separators_allowed; 
	}
	virtual void SetMaxItems(INT32 max_num_items) { 
		m_max_items = max_num_items; 
	}

	/**
	 * Returns the element type
	 */
	Type GetType() { return FOLDER_TYPE; }

	/**
	 * Returns the image name of the icon that shall be used
	 * to represent this node. The returned value can be 0
	 */
	const char* GetImage() const;

	// if this folder can allow more items
	virtual BOOL GetMaximumItemReached(INT32 count = 1);

protected:

	/**
	 * Copy constructor
	 */
	FolderModelItem( const FolderModelItem& item );

	OpString m_target;

private:

	BOOL m_is_trashfolder;
	BOOL m_is_activefolder;
	BOOL m_is_expandedfolder;
	BOOL m_is_linkbarfolder;
	BOOL m_move_is_copy;
	BOOL m_deletable;
	BOOL m_subfolders_allowed;
	BOOL m_separators_allowed;
	INT32 m_max_items;
};

/*************************************************************************
*
* UniteFolderModelItem
*
*
**************************************************************************/
#ifdef WEBSERVER_SUPPORT

class UniteFolderModelItem : public FolderModelItem
{
public:

	/**
	* Initializes an element in the off-screen hotlist treelist
	*/
	UniteFolderModelItem();
	UniteFolderModelItem(INT32 dtype);

	/**
	* Releases any allocated resouces belonging to this object
	*/
	~UniteFolderModelItem();

	HotlistModelItem *GetDuplicate( );
	/**
	* Returns the image name of the icon that shall be used
	* to represent this node. The returned value can be 0
	*/
	const char* GetImage() const;

	OP_STATUS GetItemData( ItemData* item_data );
	virtual int GetNumLines() { return 2; }

protected:
	UniteFolderModelItem( const UniteFolderModelItem& item );

};

#endif // WEBSERVER_SUPPORT

/*************************************************************************
 *
 * ContactModelItem
 *
 *
 **************************************************************************/

class ContactModelItem : public GenericModelItem
{
public:
	/**
	 * Initializes an element in the off-screen hotlist treelist
	 */
	ContactModelItem();

	/**
	 * Releases any allocated resouces belonging to this object
	 */
	~ContactModelItem();

	/**
	 * Creates a duplicate of the node and returns it
	 */
	HotlistModelItem *GetDuplicate( );

	/**
	 * Functions for setting data read from file
	 */
	void SetMail( const uni_char* str )
	{
		m_mail.Set(str);
	}

	void SetPhone( const uni_char* str )
	{
		m_phone.Set(str);
	}

	void SetFax( const uni_char* str )
	{
		m_fax.Set(str);
	}

	void SetPostalAddress( const uni_char* str )
	{
		m_postal_address.Set(str);
	}

	void SetPictureUrl( const uni_char* str )
	{
		m_picture_url.Set(str);
	}
	void SetIconName( const char* str );

	void SetConaxNumber( const uni_char* str )
	{
		m_conaxnumber.Set(str);
	}

	void SetImAddress( const uni_char* str )
	{
		m_imaddress.Set(str);
	}

	void SetImProtocol( const uni_char* str )
	{
		m_improtocol.Set(str);
	}

	void SetImStatus( const uni_char* str )
	{
		m_imstatus.Set(str);
	}

	void SetImVisibility( const uni_char* str )
	{
		m_imvisibility.Set(str);
	}

	void SetImXPosition( const uni_char* str )
	{
		m_imxposition.Set(str);
	}

	void SetImYPosition( const uni_char* str )
	{
		m_imyposition.Set(str);
	}

	void SetImWidth( const uni_char* str )
	{
		m_imwidth.Set(str);
	}

	void SetImHeight( const uni_char* str )
	{
		m_imheight.Set(str);
	}

	void SetImMaximized( const uni_char* str )
	{
		m_immaximized.Set(str);
	}

	void SetM2IndexId( const index_gid_t index_id )
	{
		m_m2_index_id = index_id;
	}

	/**
	 * Change from contact0 to contact38 or contact39 if the contact is followed or ignored (and the other way around)
	 */
	void ChangeImageIfNeeded();
	/**
	 * Functions for reading data read from file
	 */
	const OpStringC &GetMail() const { return m_mail; }
	const OpStringC &GetPhone() const { return m_phone; }
	const OpStringC &GetFax() const { return m_fax; }
	const OpStringC &GetPostalAddress() const {return m_postal_address;}
	const OpStringC &GetPictureUrl() const { return m_picture_url; }
	const OpStringC &GetConaxNumber() const { return m_conaxnumber; }
	const OpStringC &GetImAddress() const { return m_imaddress; }
	const OpStringC &GetImProtocol() const { return m_improtocol; }
	const OpStringC &GetImStatus() const { return m_imstatus; }
	const OpStringC &GetImVisibility() const { return m_imvisibility;}
	const OpStringC &GetImXPosition() const { return m_imxposition; }
	const OpStringC &GetImYPosition() const { return m_imyposition; }
	const OpStringC &GetImWidth() const { return m_imwidth; }
	const OpStringC &GetImHeight() const { return m_imheight; }
	const OpStringC &GetImMaximized() const { return m_immaximized; }
	void  GetInfoText( OpInfoText &text );
	virtual index_gid_t GetM2IndexId( BOOL create_if_null = FALSE );
	virtual BOOL HasNickname(const OpStringC& nick);

	OP_STATUS GetPrimaryMailAddress(OpString& address);
	OP_STATUS GetMailAddresses(OpAutoVector<OpString>& addresses);
	/**
	 * Returns the element type
	 */
	Type GetType() { return CONTACT_TYPE; }

	/**
	 * Returns the image name of the icon that shall be used
	 * to represent this node. The returned value can be 0
	 */
	const char* GetImage() const;

protected:

	/**
	 * Copy constructor
	 */
	ContactModelItem( const ContactModelItem& item );

private:

	OpString m_mail;
	OpString m_phone;
	OpString m_fax;
	OpString m_postal_address;
	OpString m_picture_url;
	OpString8 m_icon_name;
	OpString m_conaxnumber;
	OpString m_imaddress;
	OpString m_improtocol;
	OpString m_imstatus;
	OpString m_imvisibility;
	OpString m_imxposition;
	OpString m_imyposition;
	OpString m_imwidth;
	OpString m_imheight;
	OpString m_immaximized;
	index_gid_t m_m2_index_id;
};


/*************************************************************************
 *
 * NoteModelItem
 *
 *
 **************************************************************************/

class NoteModelItem : public GenericModelItem
{
public:
	/**
	 * Initializes an element in the off-screen hotlist treelist
	 */
	NoteModelItem();

	/**
	 * Releases any allocated resouces belonging to this object
	 */
	~NoteModelItem();

	/**
	 * Creates a duplicate of the node and returns it
	 */
	HotlistModelItem *GetDuplicate( );

	/**
	 * Returns the element type
	 */
	Type GetType() { return NOTE_TYPE; }

	/**
	 * Returns the image name of the icon that shall be used
	 * to represent this node. The returned value can be 0
	 */
	const char* GetImage() const;
#ifdef NOTES_USE_URLICON
	Image GetIcon();
#endif

	void  GetInfoText( OpInfoText &text );

protected:
	/**
	 * Copy constructor
	 */
	NoteModelItem( const NoteModelItem& item );

private:
};

/*************************************************************************
 *
 * GadgetModelItem
 *
 *
 **************************************************************************/

#ifdef GADGET_SUPPORT
class GadgetModelItem : public GenericModelItem, 
						public DesktopWindowListener

{
public:
	// These are in order of importance, used when determining state
	// of multiply selected items
	enum GMI_State
	{
		Closed,		
		Hidden,
		Shown,
		NoneSelected
	};

public:
	/**
	 * Initializes an element in the off-screen hotlist treelist
	 */
	GadgetModelItem();
	GadgetModelItem(INT32 dtype);

	/**
	 * Releases any allocated resouces belonging to this object
	 */
	virtual ~GadgetModelItem();

	/**
	 * Returns the element type
	 */
	Type GetType() { return GADGET_TYPE; }

	/**
	 * Returns the image name of the icon that shall be used
	 * to represent this node. The returned value can be 0
	 */
	const char* GetImage() const;

	/**
	 * Returns the icon defined by the widget author, can return NULL
	 */
	Image			GetIcon();

	void	GetInfoText( OpInfoText &text );

#ifdef WEBSERVER_SUPPORT
	BOOL  GetIsUpgraded() const { return m_upgraded; }
#endif

	//removes the gadget from the system
	OP_STATUS	UninstallGadget();

	//install a gadget from a file
	OP_STATUS	InstallGadget(const OpStringC &address, 
				      const OpStringC& orig_url,
				      const BOOL clean_uninstall = FALSE, 
				      BOOL launch_after_install = TRUE, 
				      const OpStringC &force_widget_name = UNI_L(""),
				      const OpStringC &force_service_name = UNI_L(""),	// Service_name only for Unite Services
					  const OpStringC& shared_path = UNI_L(""),		// Shared path only for Unite Services
					  URLContentType install_type = URL_GADGET_INSTALL_CONTENT
					  );

	// Upgrade an already installed gadget
	OP_STATUS   RestartGadget(BOOL launch);

	virtual OP_STATUS ShowGadget(BOOL show = TRUE,
			BOOL save_running_state = TRUE) = 0; // needed for Unite apps (if TRUE, RUNNING=YES in unite.adr is updated)

	virtual BOOL		IsGadgetRunning()
	{
		return (m_gadget_window != NULL);
	};


	//FIXME: apply properties directly to desktopgadget if appropriate
	void SetGadgetIdentifier( const uni_char* str ) {	m_gadget_id.Set(str); };
	void SetGadgetCleanUninstall( const BOOL clean ) { m_perform_clean_uninstall = clean; };

	const OpStringC &GetGadgetIdentifier() const { return m_gadget_id; }
	BOOL GetGadgetCleanUninstall() const { return m_perform_clean_uninstall; }
	//const OpStringC &GetItemStatus() const { return m_item_status; }

	//get the gadget window beloing to this item, returns NULL if the gadget is not running
	DesktopGadget*	GetDesktopGadget() const { return m_gadget_window; }
	//get the opgadget beloing to this item, can return NULL
	OpGadget*	GetOpGadget();

	BOOL	EqualsID(const OpStringC & gadget_id, const OpStringC & alternate_gadget_id = OpStringC()); // gadget_id is OpGadget::GetGadgetID()

	//DekstopWindow::Listener hooks
	virtual void	OnDesktopWindowClosing(DesktopWindow* desktop_window, 
								   BOOL user_initiated);

	void	OnDesktopWindowChanged(DesktopWindow* desktop_window);

	void	OnDesktopWindowStatusChanged(DesktopWindow* desktop_window, 
										 DesktopWindowStatusType type);

protected:

	virtual void AddActivityListener() {}
	/**
	 * Copy constructor
	 */
	GadgetModelItem( const GadgetModelItem& item );

	virtual void CloseGadgetWindow(BOOL immediately = FALSE, BOOL user_initiated = FALSE, BOOL force = TRUE);

	BOOL		IsInsideGadgetFolder(const OpStringC & gadget_path);

	OpString        m_gadget_id;
#ifdef WEBSERVER_SUPPORT
	OpString        m_item_status;
	OpString8		m_button_image; // Only unite services
#endif
	DesktopGadget*	m_gadget_window;

private:
	BOOL			m_perform_clean_uninstall;
	SimpleFileImage*m_image;
#ifdef WEBSERVER_SUPPORT
	Image			m_scaled_image;
	BOOL			m_upgraded;
#endif // WEBSERVER_SUPPORT
	BOOL            m_ignore_notifications;
};
#endif // GADGET_SUPPORT


#ifdef WEBSERVER_SUPPORT

/*************************************************************************
 *
 * UniteServiceModelItem
 *
 *
 **************************************************************************/

class UniteServiceModelItem : public GadgetModelItem
                            , public DesktopGadget::ActivityListener
                            , public OpTimerListener
{

public:
	void SetGadgetWindow(DesktopGadget& gadget_window);

	//DesktopGadget::ActivityListener hooks
	void		OnDesktopGadgetActivityStateChanged(DesktopGadget::DesktopGadgetActivityState act_state, 
							    time_t seconds_since_last_request);
	void		OnGetAttention();

	OP_STATUS	InstallRootService(OpGadget& root_service);
	void		SetRootServiceName();

	void		GoToPublicPage();

 	/**
 	 * Releases any allocated resouces belonging to this object
 	 */
 	~UniteServiceModelItem();
 	UniteServiceModelItem();

protected:
	UniteServiceModelItem(const UniteServiceModelItem&);

public:
 	/**
 	 * Creates a duplicate of the node and returns it
 	 */
 	UniteServiceModelItem *GetDuplicate( );


 	/**
	 * Returns the element type
	 */
	Type GetType() { return UNITE_SERVICE_TYPE; }

	/**
	 * Returns the image name of the icon that shall be used
	 * to represent this node. The returned value can be 0
	 */
	virtual const char* GetImage() const { return "Unite Panel Default"; }

	OP_STATUS GetItemData(ItemData* item_data);
	virtual int GetNumLines() { return 2; }

	OP_STATUS CreateGadgetWindow();

	void CloseGadgetWindow(BOOL immediately, 
			       BOOL user_initiated, 
			       BOOL force); 
	void AddActivityListener();

	OP_STATUS	ShowGadget(BOOL show = TRUE, BOOL save_running_state = TRUE);

	BOOL  IsRootService() const { return m_is_root_service; }
	void  SetIsRootService();

	BOOL  NeedsConfiguration() const { return m_needs_configuration; }
	void  SetNeedsConfiguration(BOOL needs_configuration);

	BOOL  GetIsDeletable()   const { return !m_is_root_service; }

	void SetGadgetStyle( const INT32 style );
	virtual INT32 GetGadgetStyle() const { return DesktopGadget::GADGET_STYLE_HIDDEN; }

	BOOL		IsGadgetRunning();


	/*
	 * DON'T use that variable for internal housekeeping. It is supposed to only be used for the
	 * user-intended state, represented by 'RUNNING=YES' in unite.adr. See bug [DSK-270351].
	 */ 
	void SetAddToAutoStart(BOOL add);
	BOOL NeedsToAutoStart() { return m_add_to_auto_start; }

	//DekstopWindow::Listener hooks
	virtual void	OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);
	virtual void	OnDesktopWindowActivated(DesktopWindow* desktop_window, BOOL active);

	BOOL OpenOrFocusServicePage();
	void SetOpenServicePage(BOOL open) { m_open_page = open; }
	BOOL GetOpenServicePage() { return m_open_page; }
	
	// set to True in WebServerManager::OnNewDOMEventListener, openning page before that will 404
	void SetCanOpenServicePage(BOOL open) { m_can_open_page = open; }
	BOOL GetCanOpenServicePage() { return m_can_open_page; }

	void SetNeedsAttention(BOOL needs_attention);
	BOOL GetNeedsAttention() const { return m_needs_attention; }
	BOOL GetAttentionStateChanged() const { return m_attention_state_changed; }

	// get the text for not running state, it differ for root gadget("not running") and normal gadget(" ")
	OpString GetNotRunningStatusText();
	
	virtual void GetInfoText(OpInfoText &text);
	
	virtual void OnTimeOut(OpTimer* timer);

	OP_STATUS	SetServiceWindow(DocumentDesktopWindow * service_window);

#ifdef GADGET_UPDATE_SUPPORT
	OP_STATUS ContainsStubGadget(BOOL& stub);
#endif //GADGET_UPDATE_SUPPORT

private:
	OP_STATUS	GetServicePageAddress(OpString & address);
	BOOL		ServiceWindowHasServiceOpened();

private:
	OpString		m_item_activity_status; 
	OpTimer*		m_item_activity_timer;
	INT32			m_item_activity_timer_count;
	BOOL            m_is_root_service;
	BOOL			m_needs_configuration;
	BOOL            m_add_to_auto_start;
	BOOL            m_open_page;
	BOOL            m_can_open_page;
	BOOL			m_needs_attention;
	BOOL			m_attention_state_changed;
	DocumentDesktopWindow *	m_service_window;		///< Reference to the tab that has the service's page opened (as opposed to the gadget window, which is hidden)
};

#endif // WEBSERVER_SUPPORT

/*************************************************************************
 *
 * GroupModelItem
 *
 *
 **************************************************************************/

class GroupModelItem : public GenericModelItem
{
public:
	/**
	 * Initializes an element in the off-screen hotlist treelist
	 */
	GroupModelItem();

	/**
	 * Releases any allocated resouces belonging to this object
	 */
	~GroupModelItem();

	/**
	 * Creates a duplicate of the node and returns it
	 */
	HotlistModelItem *GetDuplicate( );

	void SetGroupList( const uni_char* str )
	{
		m_group_list.Set(str);
	}

	const OpStringC &GetGroupList() const { return m_group_list; }

	void AddItem(UINT32 id);
	void RemoveItem(UINT32 id);

	UINT32 GetItemCount();
	UINT32 GetItemByPosition(UINT32 position);

	/**
	 * Returns the element type
	 */
	Type GetType() { return GROUP_TYPE; }

	/**
	 * Returns the image name of the icon that shall be used
	 * to represent this node. The returned value can be 0
	 */
	const char* GetImage() const;

	void  GetInfoText( OpInfoText &text );

protected:
	/**
	 * Copy constructor
	 */
	GroupModelItem( const GroupModelItem& item );

private:

	OpString m_group_list;
};

/*************************************************************************
 *
 * SeparatorModelItem
 *
 *
 **************************************************************************/

class SeparatorModelItem : public HotlistModelItem
{
public:
	enum {
		NORMAL_SEPARATOR,
		TRASH_SEPARATOR,
		ROOT_SERVICE_SEPARATOR
	};
		
	/**
	 * Initializes an element in the off-screen hotlist treelist
	 */
	SeparatorModelItem();
	SeparatorModelItem(UINT32 type);

	/**
	 * Releases any allocated resouces belonging to this object
	 */
	~SeparatorModelItem();

	/**
	 * Creates a duplicate of the node and returns it
	 */
	HotlistModelItem *GetDuplicate( );

	virtual BOOL  GetIsDeletable()   const;

	/**
	 * Returns the element type
	 */
	Type GetType() { return SEPARATOR_TYPE; }


	/**
	 * Functions for setting data read from file
	 */
	void SetName( const uni_char* str )
	{
		m_name.Set(str);
	}



	/**
	 * Functions for reading data read from file
	 */
	const OpStringC &GetName() const { return m_name; }

	virtual UINT32 GetSeparatorType() const { return m_separator_type; }

	OP_STATUS GetItemData( ItemData* item_data );

protected:

	/**
	 * Copy constructor
	 */
	SeparatorModelItem( const SeparatorModelItem& item );

private:
	OpString	  m_name;	
	UINT32        m_separator_type;
};

#endif
