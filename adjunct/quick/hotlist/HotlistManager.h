/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 * @author Co-owner: Espen Sand (espen)
 *
 */

#ifndef __HOTLIST_MANAGER_H__
#define __HOTLIST_MANAGER_H__

#include "modules/util/OpTypedObject.h"

#include "adjunct/quick/hotlist/HotlistGenericModel.h"
#include "adjunct/quick/models/BookmarkModel.h"
#include "adjunct/quick/managers/DesktopManager.h"

#include "modules/pi/OpDragManager.h"
#include "adjunct/desktop_pi/DesktopDragObject.h"
#include "adjunct/desktop_pi/desktop_file_chooser.h"

class DesktopWindow;
class Image;
class OpTreeView;
class URL;
class Hotlist;
class DesktopGadgetManager;
class OperaAccountManager;

#define g_hotlist_manager (HotlistManager::GetInstance())

/*************************************************************************
 *
 * HotlistManager
 *
 *
 **************************************************************************/

class HotlistManager : public DesktopManager<HotlistManager>
					 , public HotlistModelListener
					 , public DesktopFileChooserListener
#ifdef WIC_USE_ANCHOR_SPECIAL
					 , public AnchorSpecialHandler
#endif // WIC_USE_ANCHOR_SPECIAL
{

public:

	class ItemData
	{
	public:
		ItemData()
		{
			direct_image_pointer = 0;
			small_screen = FALSE;
			personalbar_position = -1;
			panel_position = -1;
		};

	public:
		OpString name;
		OpString url;
		OpString8 image;
		OpString folder;
		const char *direct_image_pointer;
		OpString description;
		OpString shortname;
		OpString created;
		OpString visited;
 		OpString unique_guid;
		BOOL small_screen;
		INT32 personalbar_position; // -1 means not on the bar
		INT32 panel_position;		// -1 means not on the panel

		// Contacts
		OpString mail;
		OpString phone;
		OpString fax;
		OpString postaladdress;
		OpString pictureurl;
		OpString conaxnumber;

	};

	// FIXME: Until merge after 7.30 (htm_elm.cpp)
	class ContactData
	{
	public:
		ContactData()
		{
			in_panel = FALSE;
		}

	public:
		OpString name;
		OpString url;
		BOOL in_panel;
	};

	enum SaveAsMode
	{
		SaveAs_Export,
		SaveAs_Html
	};

	enum SensitiveTag
	{
		VisitedTime=0x01, CreatedTime=0x02
	};

public:

	// Construction.
	HotlistManager();
	virtual ~HotlistManager();

	OP_STATUS Init();

#ifdef WIC_USE_ANCHOR_SPECIAL
	// == AnchorSpecialHandler ===
	virtual BOOL HandlesAnchorSpecial(OpWindowCommander * commander, const OpDocumentListener::AnchorSpecialInfo & info);
	virtual BOOL AnchorSpecial(OpWindowCommander * commander, const OpDocumentListener::AnchorSpecialInfo & info);
#endif // WIC_USE_ANCHOR_SPECIAL

	/**
	 * Saves model if it is dirty
	 *
	 * @return Returns TRUE if data was saved, otherwise FALSE
	 */
	BOOL SaveModel(HotlistModel& model, OpString& filename, BOOL enable_backup);

	/**
	 * Saves model if it is dirty
	 *
	 * @return Returns TRUE if data was saved, otherwise FALSE
	 */
	BOOL SaveModel(int type) {return SaveModel(GetHotlistModel(type), GetFilename(type), GetBackupEnabled(type));}

	/**
	 * Saves all items to a new file.
	 *
	 * @param id The item to match in the model.
	 * @param mode The save mode. Use enum SaveAsMode modes
	 */
	void SaveAs( INT32 id, SaveAsMode mode );

	/**
	 * Saves the elments i the 'index_list' to a new file.
	 *
	 * @param id The id of the model to use
	 * @param id_list The items to match in the model.
	 * @param mode The save mode. Use enum SaveAsMode modes
	 */
	void SaveSelectedAs( INT32 id, OpINT32Vector& id_list, SaveAsMode mode );

	/**
	 * Imports bookmarks from a file into the list position specified by 'id'.
	 * The 'format' argument specifies what kind of format the parser will
	 * use on the file. Use HotlistModel
	 *
	 * @param id The id of the node where import will take place. If the
	 *        id is not a valid node then the root node of the model
	 *        that is accosiated with 'format' will be used.
	 * @param format The fileformat. Use one of HotlistModel::DataFormat
	 *        values.
	 * @param location The file path of the bookmark file you want to import.
	 *        If no path is given, Opera will prompt the user for one.
	 * @param import_into_root_folder Whether imported bookmarks should go
	 *        into the root of the bookmarks, or under a specific folder (such
	 *        a folder will typicall say something like 'Netscape bookmarks'
	 *        or similar).
	 * @param show_import_info Show the user a message when the importing is done?
	 */
	void Import( INT32 id, int format, const OpStringC& location = UNI_L(""), BOOL import_into_root_folder = FALSE, BOOL show_import_info = TRUE );

	/**
	 * Imports bookmarks from a file into the root of the exsisting bookmark file
	 * Called in DesktopBookmarkManager
	 * @return Returns TRUE if data was imported, otherwise FALSE
	 */
	BOOL ImportCustomFileOnce();

	/**
	 * Opens a file selector and asks the user to select a bookmark
	 * or contact file.
	 *
	 * @param The hotlist type. Use one of HotlistModel::ListRoot
	 */
	void Open( int type );

	/**
	 * Opens a file selector and asks the user to select a new bookmark
	 * or contact file.
	 *
	 * @param The hotlist type. Use one of HotlistModel::ListRoot
	 */
	void New( int type );

	// TODO: Remove when patch on dochand taken
	BOOL OnDocumentLoaded( const URL *url, BOOL delayed ) { return TRUE; }

	/**
	 * Returns the identifier of the first item in the list defined by type.
	 *
	 * @return The identifier or -1 if there are no first item
	 */
	INT32 GetFirstId(int type);

	/**
	 * Locates the item with an identifier equal to 'id'
	 *
	 * @param id The item id to match
	 *
	 * @return Pointer to the node on success, otherwise NULL
	 */
	virtual HotlistModelItem *GetItemByID( INT32 id );

	/**
	 * Returns the id of the folder that is the nearest parent
	 * of the node identified by 'id'. If the 'id' specifies
	 * a folder then that id is retuned.
	 *
	 * @param id The item id to match
	 *
	 * @return The identifier or -1 on error.
	 */
	INT32 GetFolderIdById( INT32 id );

	/**
	 * Returns the personal bar position of the item
	 *
	 * @param id Identifier of the item in the personal bar
	 *
	 * @return The position or -1 if not in panel
	 */
	INT32 GetPersonalBarPosition( INT32 id );

	/**
	 * Returns the panel position of the item
	 *
	 * @param id Identifier of the item in panel
	 *
	 * @return The position or -1 if not in panel
	 */
	INT32 GetPanelPosition( INT32 id );

	/**
	 * Sets the personal bar position of the item
	 *
	 * @param id Identifier of the item in personal bar
	 * @param position The new position.
	 * @param broadcast Notify rest of system when TRUE
	 */
	void SetPersonalBarPosition( INT32 id, INT32 position, BOOL broadcast);

	/**
	 * Sets the panel position of the item
	 *
	 * @param id Identifier of the item in panel
	 * @param position The new position.
	 * @param broadcast Notify rest of system when TRUE
	 */
	void SetPanelPosition( INT32 id, INT32 position, BOOL broadcast);

	BOOL IsOnPersonalBar( OpTreeModelItem *item );
	BOOL IsInPanel( OpTreeModelItem *item );
	BOOL IsFolder( OpTreeModelItem *item );
	BOOL IsGroup( OpTreeModelItem *item );
	BOOL IsContact( const OpStringC &mail_address);
	BOOL IsBookmarkFolder( OpTreeModelItem *item );

	BOOL GetMailAndName(INT32 id, OpString &mail, OpString &name,BOOL include_folder);
	BOOL GetMailAndName(OpINT32Vector& id_list, OpString& address, OpString& name, BOOL include_folder_content);

	BOOL GetRFC822FormattedNameAndAddress(OpINT32Vector& id_list, OpString& rfc822_address, BOOL include_folder_content);
	OP_STATUS RecursiveAppendRFC822FormattedFolderItems(OpINT32Vector& id_list, HotlistModel* folder_model, INT32 child_index, OpString& rfc822_address, BOOL& needs_comma);
	OP_STATUS AppendRFC822FormattedItem(const OpStringC& name, const OpStringC& address, OpString& rfc822_address, BOOL& needs_comma) const;

	BOOL GetItemValue_ByEmail( const OpStringC &email, ItemData& item_data );
	BOOL GetItemValue( INT32 id, ItemData& data );
	BOOL GetItemValue( OpTreeModelItem* item, ItemData& data );

	/**
	 * Sets node data. The list that contains the item will be saved and
	 * the apperance updated.
	 *
	 * @param item The item to update
	 * @param data The new item data
	 * @param validate Test (and modify) some of the strings to ensure proper format
	 * @param flag_changed Flag that indicates which fields to set on the item.
	 *                     If value is FLAG_UNKNOWN everything is set
	 *
	 * @return TRUE on success, otherwise FALSE
	 */
	BOOL SetItemValue( INT32 id, const ItemData& data, BOOL validate, UINT32 flag_changed = HotlistModel::FLAG_UNKNOWN );

	/**
	 * Sets node data. The list that contains the item will be saved and
	 * the apperance updated.
	 *
	 * @param id The identitifier of the item to update
	 * @param data The new item data
	 * @param validate Test (and modify) some of the strings to ensure proper format
	 *
	 * @return TRUE on success, otherwise FALSE
	 */
	BOOL SetItemValue( OpTreeModelItem *item, const ItemData& data, BOOL validate, UINT32 flag_changed = HotlistModel::FLAG_UNKNOWN );

	/**
	 * Removes bookmark item from History
	 *
	 * Needed when an url changes on a bookmark item.
	 */
	void RemoveBookmarkFromHistory(HotlistModelItem* item);

	BOOL ShowOnPersonalBar( INT32 id, BOOL state );
	BOOL ShowInPanel( INT32 id, BOOL state );
	BOOL ShowSmallScreen( INT32 id, BOOL state );
	BOOL EditItem_ByEmail( const OpStringC& email, DesktopWindow* parent );
	BOOL EditItem_ByEmailOrAddIfNonExistent( const OpStringC& email, DesktopWindow* parent );
	BOOL EditItem( INT32 id, DesktopWindow* parent );

	/**
	 * Creates an item in the model described by 'onto'.
	 *
	 * @param item_data The properties of the new item
	 * @param type The type of the new item
	 * @param onto Identifier of target node that was dropped onto
	 * @param insert_type INSERT_INTO, INSERT_BEFORE or INSERT_AFTER
	 * @param execute Do the drop operation if TRUE, otherwise just check if it
	 *        is possible
	 * @param first_id On return the indentifier of the dopped item is copied
	 *        here when execute is TRUE. A result of -1 indicates that no new
	 *        item was added.
	 *
	 * @return TRUE if drop opration was allowed otherwise FALSE
	 */
	BOOL DropItem(const ItemData& item_data, OpTypedObject::Type type, INT32 onto, DesktopDragObject::InsertType insert_type, BOOL execute, OpTypedObject::Type drag_object_type, INT32 *first_id=0);

	/**
	 * Moves an item and any children from one location to another in the model.
	 * The operation is only allowed inside the same model and an item can not
	 * be moved into a child of its own.
	 *
	 * @param id Identifier of node that was dropped
	 * @param onto Identifier of target node that was dropped onto
	 * @param drop_type DROP_COPY or DROP_MOVE
	 * @param insert_type INSERT_INTO, INSERT_BEFORE or INSERT_AFTER
	 * @param execute Do the drop operation if TRUE, otherwise just check if it
	 *        is possible
	 * @param first_id On return the indentifier of the dopped item is copied
	 *        here when execute is TRUE. A result of -1 indicates that no new
	 *        item was added.
	 * @param force_delete, is used to make the item be deleted without using the
				trash
	 *
	 * @return TRUE if drop operation was allowed otherwise FALSE
	 */
	BOOL DropItem( INT32 id, INT32 onto, DropType drop_type, DesktopDragObject::InsertType insert_type, BOOL execute,INT32 *first_id=0, BOOL force_delete = FALSE);
	void Copy( const ItemData &cd, BOOL append, INT32 flag_changed );
	void Copy( OpINT32Vector &id_list );
	BOOL Delete( OpINT32Vector &id_list, BOOL save_to_clipboard );
	BOOL Paste( OpTreeModel* model, OpTreeModelItem* item );
	BOOL EmptyTrash( OpTreeModel* model );
	BOOL HasClipboardData();
	BOOL HasClipboardData(INT32 item_type);
	BOOL HasClipboardItems(); // return true if non-folder and non-separator items in clipboard
	BOOL HasClipboardItems(INT32 item_type);

	INT32 GetItemTypeFromModelType(INT32 model_type);

	void SetupPictureUrl(ItemData& item_data);

	BOOL NewContact( const ItemData& item_data, INT32 parent_id, DesktopWindow* parent, BOOL interactive );
	BOOL NewContact(INT32 parent_id, DesktopWindow* parent );
	BOOL NewGroup(INT32 parent_id, DesktopWindow* parent );
	BOOL NewNote(const ItemData& item_data, INT32 parent_id,DesktopWindow* parent, OpTreeView* treeView );
	BOOL NewFolder(INT32 parent_id,DesktopWindow* parent, OpTreeView* treeView );
	BOOL NewFolder(const OpStringC& name, INT32 parent_id, INT32* got_id);
	BOOL NewSeparator(INT32 target_id, INT32* got_id = NULL);

	BOOL NewRootService(OpGadget& root_gadget);

#ifdef WEBSERVER_SUPPORT
	virtual BOOL NewUniteService(const OpStringC& address, 
						 const OpStringC& orig_url, 
						 INT32* got_id, 
						 BOOL clean_uninstall, 
						 BOOL launch_after_install,
						 const OpStringC& force_widget_name, 
						 const OpStringC& force_service_name,
						 const OpStringC& shared_path,
						 OpTreeView* treeView);
#endif //WEBSERVER_SUPPORT
	BOOL AddNickToContact(INT32 contact_id, OpStringC& nick);

	/**
	 * Sets the visible name of the node
	 *
	 * @param item The node to change
	 * @param text The new name
	 *
	 * @return TRUE on success, otherwise FALSE
	 */
	BOOL Rename( OpTreeModelItem *item, OpString& text );

	/**
	 * Returns a list of names of contacts that match the pattern.
	 * The list and each string in it is allocated an must be
	 * released by the caller when no longer needed.
	 *
	 * @param pattern The string to match
	 * @param list The list with matched names
	 * @param num_item The number of ekements in the returned list
	 */
	void GetContactNameList( const OpStringC &pattern, uni_char*** list, INT32* num_item );

	/**
	 * Reparents the item to a new folder in the list that contains
	 * item.
	 *
	 * @param item The item to update
	 * @param new_parent_id Index of new parent
	 */
	BOOL Reparent( OpTreeModelItem *item, INT32 new_parent_id );

	BOOL RegisterTemporaryItemFirstInFolder( OpTreeModelItem *item, HotlistModelItem* parent );
	BOOL RegisterTemporaryItemOrdered( OpTreeModelItem *item, HotlistModelItem* previous );
	BOOL RegisterTemporaryItem( OpTreeModelItem *item, INT32 parent_id, INT32* got_id = NULL);
	BOOL RemoveTemporaryItem( OpTreeModelItem *item );

	// @param only_one if TRUE the function only return TRUE if the count is 1
	BOOL HasNickname(const OpStringC &candidate, HotlistModelItem* exclude_hmi, BOOL exact = TRUE, BOOL only_one = FALSE); 

	void GetNicknameList(const OpStringC &candidate, BOOL exact, OpINT32Vector& id_list, HotlistModelItem* exclude_hmi, BOOL use_id = TRUE);

	BOOL ResolveNickname(const OpStringC& address, OpVector<OpString>& urls);

	BOOL OpenByNickname(const OpStringC &candidate, BOOL3 new_window, BOOL3 new_page, BOOL3 in_background, BOOL exact );
	
	/**
	 * Show or hide multiple gadgets by id.
	 * @param id_list	List with identifiers of the gadgets in the hotlist model
	 *					Note: The id changes on restart of opera!
	 * @param show		Whether the widget should be shown or closed. If a widget is already shown, and show=TRUE
	 *					it will be activated (window gets focus).
	 * @param only_test_for_visibility_change	Test if the call actually changes the visibility of widgets,
	 *					no visibility changes are actually made.
	 * @return			When only_test_for_visibility_change is set to TRUE, and one of the is not yet in 
	 *					the desired state (parameter show) it will return TRUE.
	 *					When only_test_for_visibility_change is set to FALSE, the function will always return TRUE
	 */
	BOOL ShowGadgets( const OpINT32Vector& id_list, BOOL show = TRUE, BOOL only_test_for_visibility_change = FALSE, INT32 model_type = HotlistModel::UniteServicesRoot);

	/**
	 * Determines what should be done if multiple gadgets are selected. i.e. open, close, show or hide actions
	 * @param id_list		List with identifiers of the gadgets in the hotlist model
	 *						Note: The id changes on restart of opera!
	 * @param model_type	Type of model being tested i.e. Gadgets or Unite
	 * @return				Returns the combined state of the gadgets
	 */
	GadgetModelItem::GMI_State GetSelectedGadgetsState(OpINT32Vector& id_list, INT32 model_type);

	/**
	 * Opens/Closes listed gadgets
	 * @param id_list		List with identifiers of the gadgets in the hotlist model
	 *						Note: The id changes on restart of opera!
	 * @param style			Style to set
	 */
	void OpenGadgets(const OpINT32Vector& id_list, BOOL open, INT32 model_type);

	// Open a gadget by id (the id changes on restart of opera)
	// If the gadget is already running it will be activated
	BOOL OpenGadget( INT32 id );
	// Open a gadget by widget id. This id will remain the same during opera sessions
	// If window is not NULL, this will return a pointer to the DesktopWindow of the gadget (NULL if opening failed)
	BOOL OpenGadget( const OpStringC& gadget_id, DesktopWindow** window = NULL, INT32 model_type = HotlistModel::UniteServicesRoot);
	
	BOOL OpenContact( INT32 id, BOOL3 new_window, BOOL3 new_page, BOOL3 in_background, DesktopWindow* target_window = 0, INT32 position = -1 );
	BOOL OpenContacts( const OpINT32Vector& id_list, BOOL3 new_window, BOOL3 new_page, BOOL3 in_background, DesktopWindow* target_window = 0, INT32 position = -1 );
	
	/**
	 * Open a url by id that is used in a hotlistmodel.
	 * @param id	Identifier in the hotlist model where the url can be found
	 * @param new_window, new_page,in_background, target_window, position, parent and ignore_modifier_keys:
	 *				correspond with parameters in the class Application::OpenURLSetting
	 * @return		TRUE if the url was successfully opened
	 */
	BOOL OpenUrl( INT32 id, BOOL3 new_window, BOOL3 new_page, BOOL3 in_background, DesktopWindow* target_window = 0, INT32 position = -1, DesktopWindowCollectionItem* parent = NULL, BOOL ignore_modifier_keys = FALSE);

	BOOL OpenOrderedUrls( const OpINT32Vector& index_list, BOOL3 new_window, BOOL3 new_page, BOOL3 in_background, DesktopWindow* target_window = 0, INT32 position = -1, DesktopWindowCollectionItem* parent = NULL, BOOL ignore_modifier_keys = FALSE);
	BOOL GetOpenUrls( OpINT32Vector& index_list, const OpINT32Vector& id_list);

	/**
	 * Open a list of urls by their id as used in the hotlistmodels.
	 * @param id_list	List with identifier in the hotlist model where the url can be found. All items should com
	 *					from the same model.
	 * @param new_window, new_page,in_background, target_window, position, parent and ignore_modifier_keys:
	 *					correspond with parameters in the class Application::OpenURLSetting
	 * @return			TRUE if the url was successfully opened
	 */
	BOOL OpenUrls( const OpINT32Vector& id_list, BOOL3 new_window, BOOL3 new_page, BOOL3 in_background, DesktopWindow* target_window = 0, INT32 position = -1, DesktopWindowCollectionItem* parent = NULL, BOOL ignore_modifier_keys = FALSE);
	
	BookmarkModel*	GetBookmarksModel() {return (BookmarkModel*)&GetHotlistModel(HotlistModel::BookmarkRoot);}
	HotlistGenericModel*	GetContactsModel() {return (HotlistGenericModel*)&GetHotlistModel(HotlistModel::ContactRoot);}
	HotlistGenericModel*	GetNotesModel() {return (HotlistGenericModel*)&GetHotlistModel(HotlistModel::NoteRoot);}
	virtual HotlistGenericModel*   GetUniteServicesModel() { return (HotlistGenericModel*)&GetHotlistModel(HotlistModel::UniteServicesRoot);}

	void GetModelItemList(INT32 type, INT32 id, BOOL only_folder_contents, OpVector<HotlistModelItem>& items);
	void GetContactAddresses(INT32 id, OpString &string, const OpStringC& email_addresses = UNI_L(""));

	// To be removed
	Image GetBookmarkIcon(const uni_char* document_url) { return Image(); }

	/* ------------------ Implementing HotlistModel::Listener interface ---------------*/

	/**
	 * Will be called when a model contains changes that need to be saved
	 *
	 * @param The model the must be saved.
	 *
	 * @return Returns TRUE on success (data saved), otherwise FALSE
	 */
	BOOL OnHotlistSaveRequest( HotlistModel *model );

	/**
	 * Called by the model when interesting things happen to the hotlist items.
	 *
	 * @param item The item that has been modified
	 */
	virtual void OnHotlistItemAdded(HotlistModelItem* item) {}
	virtual void OnHotlistItemChanged(HotlistModelItem* item, BOOL moved_as_child, UINT32 changed_flag = HotlistModel::FLAG_UNKNOWN) {}
	virtual void OnHotlistItemRemoved(HotlistModelItem* item, BOOL removed_as_child);
	virtual void OnHotlistItemTrashed(HotlistModelItem* item) {}
	virtual void OnHotlistItemUnTrashed(HotlistModelItem* item) {}
	virtual void OnHotlistItemMovedFrom(HotlistModelItem* item) {}

	
	/* ------------------ Implementing DesktopFileChooser interface -------------------*/
	void OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result);


	/* ------------------ Opera Link related methods -----------------------------------*/
	BOOL NewItemOrdered( HotlistGenericModel* model,
						 OpTreeModelItem::Type type, 
						 const ItemData* item_data, 
						 HotlistModelItem* previous );
	BOOL NewItemFirstInFolder( HotlistGenericModel* model, 
							   OpTreeModelItem::Type type,  
							   const ItemData* item_data,
							   HotlistModelItem* parent );

	BOOL IsClipboard(HotlistModel* model) { return model == &m_clipboard; }

	// dialogs

	// Shows the message before deleting a target folder
	BOOL ShowTargetDeleteDialog(HotlistModelItem* hmi_src);
	
	// Shows maximum sub items reached
	void ShowMaximumReachedDialog(HotlistModelItem* hmi);

	// Shows saving fialure dialog, return TRUE if user click Yes(try again)
	BOOL ShowSavingFailureDialog(const OpString& filename);

	// @param focus Only make sense when show is TRUE. Open the panel if TRUE, otherwise just add the panel icon. 
	//				If the panel is already open it will remain open.
	BOOL ShowPanelByName(const uni_char* name, BOOL show, BOOL focus);

protected: // functions needed to be overridden in selftests
	virtual DesktopGadgetManager *	GetDesktopGadgetManager();
	virtual OperaAccountManager *	GetOperaAccountManager();
	virtual Hotlist *				GetHotlist();
	virtual BOOL 					AskEnterOnlineMode(BOOL test_offline_mode_first);

private:

	enum FileChoosingContext {
		NO_FILE_CHOOSING,
		OPEN_FILE_CHOOSING,
		NEW_FILE_CHOOSING,
		SAVEAS_FILE_CHOOSING,
		SAVESELECTEDAS_FILE_CHOOSING,
		IMPORT_FILE_CHOOSING
	};
	
	FileChoosingContext	m_file_choosing_context;
	
	void				ResetFileChoosingContext() { m_file_choosing_context = NO_FILE_CHOOSING; }
	void				SetFileChoosingContext(FileChoosingContext context) { m_file_choosing_context = context; }
	FileChoosingContext GetFileChoosingContext () { return m_file_choosing_context; }
	
	class FileChoosingData
	{
	public:
		int				type;
		int				format;
		SaveAsMode		mode;
		OpINT32Vector*	id_list;
		BOOL			into_root;
		BOOL			show_import;
		HotlistModel*	model;
	};
	
	FileChoosingData		choosing_data;
	DesktopFileChooserRequest*	m_request;
	DesktopFileChooser*			m_chooser;
	
	void ChosenNewOrOpen(int type, BOOL new_file, OpString* filename);
	void ChosenSaveAs(int type, SaveAsMode mode, OpString* filename);
	void ChosenSaveSelectedAs(int type, SaveAsMode mode, OpINT32Vector* id_list, OpString* filename);
	void ChosenImport(int type, int format, BOOL into_root, BOOL show_import, HotlistModel* model, OpString* filename);
	
	void ChooseFile( );


	void GetFilenames( int type, OpString &writeName, OpString &readName );
	


	BOOL SafeWrite( const OpString& filename, HotlistModel &model );
	BOOL NewItem( INT32 parent_index, DesktopWindow* parent,
				  OpTreeModelItem::Type type, 
				  const ItemData* item_data,
				  BOOL interactive, 
				  OpTreeView* treeView, 
				  INT32* got_id = NULL, 
				  BOOL allow_duplicates = TRUE,
				  BOOL root_service = FALSE);


	// Get stuff based on the type

	HotlistModel&	GetHotlistModel(int type)	{return GetModelData(type).GetHotlistModel();}
	OpString&		GetFilename(int type)		{return GetModelData(type).GetFilename();}
	BOOL&			GetBackupEnabled(int type)	{return GetModelData(type).GetBackupEnabled();}
	int				GetDataFormat(int type)		{return GetModelData(type).GetDataFormat();}

	// Sometimes we need to get the type from model or id

	int				GetTypeFromModel(HotlistModel& model);
	int				GetTypeFromId(int id);

// Put this into a class to hide creation, destruction and member data

	class ModelData
	{
		public:
							ModelData(HotlistModelListener* listener, int model_type, int data_format) :
								m_model(model_type, FALSE),
								m_enable_backup(TRUE),
								m_data_format(data_format)
							{
								m_model.AddModelListener(listener);
							}

			virtual HotlistModel&	GetHotlistModel()	{return m_model;}
			OpString&		GetFilename()		{return m_filename;}
			BOOL&			GetBackupEnabled()	{return m_enable_backup;}
			int				GetDataFormat()		{return m_data_format;}

		private:

			HotlistGenericModel	m_model;
			OpString		m_filename;
			BOOL			m_enable_backup;
			int				m_data_format;
	};

	class BookmarkModelData : public ModelData
	{
	public:
		BookmarkModelData(HotlistModelListener* listener,int data_format)
			: ModelData(listener,HotlistModel::BookmarkRoot,data_format)
		{
		}

		virtual HotlistModel&	GetHotlistModel()	{return m_bookmark_model;}

	private:
		BookmarkModel m_bookmark_model;
	};

	ModelData&		GetModelData(int type);
	ModelData*		GetModelData( OpTreeModel& target );
	BookmarkModelData	m_bookmarks_modeldata;
	ModelData		m_contacts_modeldata;
	ModelData		m_notes_modeldata;
	ModelData       m_services_modeldata;
	// special clipboard model

	HotlistGenericModel	m_clipboard; 
	OpINT32Vector	m_global_id_clipboard;

};

#endif
