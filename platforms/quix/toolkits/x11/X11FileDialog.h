/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Espen Sand
 */

#ifndef _X11_FILE_DIALOG_H_
#define _X11_FILE_DIALOG_H_

#include "platforms/quix/toolkits/x11/X11FileChooser.h" // X11FileChooserSettings
#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick_toolkit/widgets/OpToolTipListener.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"

class OpTreeView;
class SimpleFileImage;

class FileListModel : public SimpleTreeModel
{
public:
	struct UserData
	{
		UserData(UINT32 type, UINT32 time, UINT64 size):type(type),time(time),size(size) {}
		OpString text;
		UINT32 type;
		UINT32 time;
		UINT64 size;
	};

public:
	FileListModel(INT32 column_count = 1):SimpleTreeModel(column_count){}
	virtual OP_STATUS GetColumnData(OpTreeModel::ColumnData* column_data);
	virtual INT32 CompareItems(INT32 column, OpTreeModelItem* item1, OpTreeModelItem* item2);
};


class X11FileDialog : public Dialog
{
private:
	struct PlaceItem
	{
		enum PlaceType { FixedFolder, CustomFolder, HistoryFolder };

		PlaceItem(PlaceType place_type) { type = place_type; }
		OpString title;
		OpString path;
		PlaceType type;
		INT32 id;

		static INT32 next_unused_id;
	};

	enum BlockMode
	{
		BM_Ignore = 0,
		BM_Yes,
		BM_No
	};

public:
	X11FileDialog(X11FileChooserSettings& settings);
	~X11FileDialog();

	Type GetType(){ return IsDirectorySelector() ? DIALOG_TYPE_X11_DIRECTORY_SELECTOR : DIALOG_TYPE_X11_FILE_SELECTOR;}
	const char*	GetWindowName() { return IsDirectorySelector() ? "Directory Chooser Dialog" : "File Chooser Dialog"; }
	const char*	GetInputContextName() { return "File Chooser Dialog"; }

	virtual BOOL GetIsBlocking() {return TRUE;}
	virtual DialogType GetDialogType() {return TYPE_OK_CANCEL;}

	virtual void OnCancel();
	virtual void OnInitVisibility();
	virtual void OnInit();

	virtual BOOL OnInputAction(OpInputAction* action);

	/**
	 * Adds an item to the place list
	 *
	 * @param name The item name. If 0 or empty the name will be extracted from the path
	 * @param path_id Symbolic path to use. Can be OPFILE_FOLDER_COUNT to indicate no path
	 * @param path Hardcoded path to use. The hardcoded path is used if the symbolic path
	          is set at the same time
	 * @param type The place item type. A node with PlaceItem::CustomFolder can be removed
	 *        and/or modified.  It is also written to file and restored. See @ref ReadPlacesL()
	 *        and @ref WritePlacesL()
	 */
	void AddPlace(const OpStringC& name, OpFileFolder path_id, const uni_char* path, PlaceItem::PlaceType type);

	/**
	 * Sets active directory and updates the displayed content. The active directory is 
	 * returned to calling code when dialog closes
	 *
	 * @param path The new directory
	 * @param add_to_undo_list Add old active directory to undo list before setting the new
	 * @param force If TRUE, update content even if 'path' identical to the active directory
	 */
	void SetDirectory(const OpStringC& path, BOOL add_to_undo_list, BOOL force);

	/**
	 * Updates the content of the path list using the current directory
	 */
	void UpdatePathList();

	/**
	 * Updates the typed text by replacing the current extension with the currently selected
	 * extension when dialog is used for saving files. The first filter is used if the selected
	 * extension contains more than one filter.
	 */
	void UpdateTypedExtension();

	/**
	 * Changes the current directory to the parent directory and updates the content
	 *
	 * @return TRUE if it was possible to step one level close to root, otherwise FALSE
	 */
	BOOL Up();

	/**
	 * Changes the current directory to the previous directory in the navigation history
	 *
	 * @return TRUE if it was possible to step one level back, otherwise FALSE
	 */
	BOOL Back();

	/**
	 * Starts editing the currently selected place item if that item can can be edited
	 *
	 * @return TRUE if edit could be started, otherwise FALSE
	 */
	BOOL EditSelectedPlaceItem();

	/**
	 * Starts editing the currently selected path item if that item can can be edited
	 *
	 * @return TRUE if edit could be started, otherwise FALSE
	 */
	BOOL EditSelectedPathItem();

	/**
	 * Creates a new nameless folder (memory only), shows a node in the current directory
	 * and activates an inline edit session. The creation is completed when the edit is
	 * either canceled (@ref OnItemEditAborted) or completed (@ref OnFolderComplete)
	 *
	 * @return TRUE if the new node was created and editing started, otherwise FALSE
	 */
	BOOL NewFolder();

	// OpWidgetListener
	virtual void OnChange(OpWidget* widget, BOOL changed_by_mouse = FALSE);
	virtual BOOL OnItemEditVerification(OpWidget *widget, INT32 pos, INT32 column, const OpString& text);
	virtual void OnItemEditAborted(OpWidget *widget, INT32 pos, INT32 column, OpString& text);
	virtual void OnItemEdited(OpWidget *widget, INT32 pos, INT32 column, OpString& text);
	virtual BOOL OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked);
	virtual void OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
	virtual void OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
	virtual void OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);

	// OpToolTipListener
	virtual BOOL HasToolTipText(OpToolTip* tooltip) { return m_tooltip_message.HasContent(); }
	virtual void GetToolTipText(OpToolTip* tooltip, OpInfoText& text) { text.SetTooltipText(m_tooltip_message); }
	virtual INT32 GetToolTipDelay(OpToolTip* tooltip) { return 0; }
	virtual BOOL GetPreferredPlacement(OpToolTip* tooltip, OpRect& ref_rect, PREFERRED_PLACEMENT& placement);

private:
	/**
	 * Returns TRUE if the selector functions as a directory selector
	 */
	BOOL IsDirectorySelector() const { return m_settings.type == ToolkitFileChooser::DIRECTORY; }

	/**
	 * Writes custom directories (bookmarks) to file named filedialogpaths.ini
	 */
	void ReadPlacesL();

	/**
	 * Reads custom directories (bookmarks) from file named filedialogpaths.ini
	 */
	void WritePlacesL();

	/**
	 * Called each time the edit mode changes
	 *
	 * @param state TRUE is an item is being modified, otherwise FALSE
	 */
	void OnEditModeChanged(BOOL state);

	/**
	 * Called when an inline editing operation has been complete. This can be the result
	 * of either a folder-rename or a folder-create session.
	 *
	 * @param pos Item's visible position in the treeview
	 * @param filename The new or modified name
	 * @param exec Create or rename folder if TRUE. If FALSE the code will only verify it
	 *        is allowed to do these actions
	 *
	 * @return TRUE if the requested operation was allowed and successfully executed (if
	 *         exec is TRUE), otherwise FALSE.
	 */
	BOOL OnFolderComplete (int pos, const OpString& filename, BOOL exec);

	/**
	 * Verifies current selected data
	 *
	 * @param on_ok TRUE if the dialog is about to be closed (accecpted by the user)
	 *
	 * @return TRUE if the dataset is valid, ohtherwise FALSE
	 */
	BOOL VerifyInput(BOOL on_ok);

	/**
	 * Verifies a filename
	 *
	 * @param candidate The filename to be tested. Shall not include any directory component
	 * @param on_ok TRUE if the dialog is about to be closed (accecpted by the user)
	 *
	 * @return TRUE if the filename is acceptable, ohtherwise FALSE
	 */
	BOOL VerifyInputFilename(const OpString& candidate, BOOL on_ok);

	/**
	 * Verifies the combination of entered text and the current directory. If there
	 * is written text then the text is exclusively used if it start with a '/', otherwise
	 * it is appended to the the current directory
	 *
	 * @param change_directory Changes the current directory to the new when TRUE and 
	 *        on successful verification
	 *
	 * @return TRUE on successful verification (directory exists), ottherwise FALSE
	 */
	BOOL VerifyInputDirectory(BOOL change_directory);

	/**
	 * Shows a message under the edited item. If there is no active edit
	 * item the message is shown in the upper part of the dialog
	 *
	 * @param message The text that shall be displayed
	 */
	void ShowMessage(const OpStringC& message);

	/**
	 * Called by @ref OnDragMove or OnDragDrop when needed
	 *
	 * @param widget The widget on which the d-n-d is taking place
	 * @param drag_object The object containing all data about the d-n-d operation
	 * @param pos Widget position
	 * @param x X coordinate of d-n-d operation. Relative to widget
	 * @param x Y coordinate of d-n-d operation. Relative to widget
	 * @param drop TRUE when object is dropped on widget, FALSE when moved over it
	 */
	void HandleDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y, bool drop);

private:
	X11FileChooserSettings& m_settings;
	FileListModel m_paths_model;
	SimpleTreeModel	m_places_model;
	OpTreeView* m_paths_tree_view;
	OpTreeView* m_places_tree_view;
	SimpleFileImage* m_back_image;
	SimpleFileImage* m_up_image;
	SimpleFileImage* m_cross_image;
	OpString m_active_directory;
	OpString8 m_rename_path;
	OpString m_tooltip_message;
	OpVector<FileListModel::UserData> m_userdata_list;
	OpVector<OpString> m_undo_list;
	OpVector<PlaceItem> m_places;
	INT32 m_num_fixed_places;
	BOOL m_is_editing;
	BlockMode m_block_path_change;
};



#endif


