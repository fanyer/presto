#include "core/pch.h"
#include "OpHotlistTreeView.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick/widgets/OpBookmarkView.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "modules/widgets/WidgetContainer.h"

OpHotlistTreeView::OpHotlistTreeView(void)
{
	m_last_focused_view = NULL;
	m_model = NULL; //have to be initialized in subclasses
	m_sort_column = 0;
	m_view_style = STYLE_TREE;

	OP_STATUS status;

	// m_folders_view
	status = OpTreeView::Construct(&m_folders_view);
	CHECK_STATUS(status);
	
	m_folders_view->SetRestrictImageSize(TRUE);
	m_folders_view->SetShowThreadImage(TRUE);
	m_folders_view->GetBorderSkin()->SetImage("Panel Treeview Skin", "Treeview Skin");

	AddChild(m_folders_view, TRUE);

	// m_hotlist_view
	status = OpTreeView::Construct(&m_hotlist_view);
	CHECK_STATUS(status);
	
	m_hotlist_view->SetRestrictImageSize(TRUE); // Req. for bookmarks. Maybe not for others? [espen 2008-08-05]
	m_hotlist_view->SetShowThreadImage(TRUE);
	m_hotlist_view->GetBorderSkin()->SetImage("Panel Treeview Skin", "Treeview Skin");

	AddChild(m_hotlist_view, TRUE);

	m_hotlist_view->SetListener(this);
	m_folders_view->SetListener(this);

	// tree view properties
	if(g_pcui->GetIntegerPref(PrefsCollectionUI::EnableDrag)&DRAG_BOOKMARK)
	{
		m_folders_view->SetDragEnabled(TRUE);
		m_hotlist_view->SetDragEnabled(TRUE);
	}
	m_folders_view->SetMatchType(MATCH_FOLDERS);

	m_hotlist_view->SetAllowOpenEmptyFolder(TRUE);
	m_hotlist_view->SetMultiselectable(TRUE);
	m_hotlist_view->SetAllowMultiLineIcons(FALSE);
	m_hotlist_view->SetHaveWeakAssociatedText(FALSE);

	// So that a panel specific font will be used
	m_folders_view->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL);
	m_hotlist_view->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL);

	status = m_hotlist_view->AddTreeViewListener(this);

	CHECK_STATUS(status);
}

OP_STATUS OpHotlistTreeView::Init()
{
	SetViewStyle(STYLE_TREE, TRUE);
	SetDetailed(FALSE, TRUE);

	InitModel();

	return OpStatus::OK;
}

void OpHotlistTreeView::InitModel()
{
	if(m_model)
	{
		// set previous selected item/folder
		HotlistModelItem* item = m_model->GetItemByID(m_model->GetActiveItemId());

		if (item)
		{
			if (item->IsFolder())
			{
				SetCurrentFolderByID(item->GetID());
			}
			else
			{
				SetCurrentFolderByID(item->GetParentFolder() ? item->GetParentFolder()->GetID() : -1);

				m_hotlist_view->SetSelectedItemByID(item->GetID());
			}
		}
	}
}

OpHotlistTreeView::~OpHotlistTreeView(void)
{
}

/***********************************************************************************
 *
 *	SetDetailed
 *
 ***********************************************************************************/

void OpHotlistTreeView::SetDetailed(BOOL detailed, BOOL force)
{
	if (!force && detailed == m_detailed)
		return;

	if (!force)
		SaveSettings();

	m_detailed = detailed;

	if(m_detailed)
	{
		m_folders_view->GetBorderSkin()->SetImage("Detailed Panel Treeview Skin", "Treeview Skin");
		m_hotlist_view->GetBorderSkin()->SetImage("Detailed Panel Treeview Skin", "Treeview Skin");
	}
	else
	{
		m_folders_view->GetBorderSkin()->SetImage("Panel Treeview Skin", "Treeview Skin");
		m_hotlist_view->GetBorderSkin()->SetImage("Panel Treeview Skin", "Treeview Skin");
	}

	m_folders_view->SetShowColumnHeaders(m_detailed);
	m_hotlist_view->SetShowColumnHeaders(m_detailed);

	m_folders_view->SetColumnVisibility(1, FALSE);
	m_folders_view->SetColumnVisibility(2, FALSE);
	m_folders_view->SetColumnVisibility(3, FALSE);
	m_folders_view->SetColumnVisibility(4, FALSE);
	m_folders_view->SetColumnVisibility(5, FALSE);

	OnSetDetailed(detailed);

	LoadSettings();
}

/***********************************************************************************
 *
 *	SetViewStyle
 *
 ***********************************************************************************/

void OpHotlistTreeView::SetViewStyle(ViewStyle view_style, BOOL force)
{
	if (m_view_style == view_style && !force)
		return;

	m_view_style = view_style;

	switch (m_view_style)
	{
		case STYLE_TREE:
			m_folders_view->SetVisibility(FALSE);
			m_hotlist_view->SetVisibility(TRUE);
			break;

		case STYLE_SPLIT:
			m_folders_view->SetVisibility(TRUE);
			m_hotlist_view->SetVisibility(TRUE);
			break;

		case STYLE_FLAT:
			m_folders_view->SetVisibility(FALSE);
			m_hotlist_view->SetVisibility(TRUE);
			break;

		case STYLE_FOLDERS:
			m_folders_view->SetVisibility(TRUE);
			m_hotlist_view->SetVisibility(FALSE);
			break;
	}

	Relayout();
	UpdateCurrentFolder();
}

/***********************************************************************************
 *
 *	SaveSettings
 *
 ***********************************************************************************/

void OpHotlistTreeView::SaveSettings()
{
	if( GetSplitterPrefsSetting() != PrefsCollectionUI::DummyLastIntegerPref )
	{
		g_pcui->WriteIntegerL(GetSplitterPrefsSetting(), GetDivision());
	}
	if( GetViewStylePrefsSetting() != PrefsCollectionUI::DummyLastIntegerPref )
	{
		g_pcui->WriteIntegerL(GetViewStylePrefsSetting(),GetViewStyle());
	}
}

/***********************************************************************************
 *
 *	LoadSettings
 *
 *
 ***********************************************************************************/

void OpHotlistTreeView::LoadSettings()
{
	if( GetSplitterPrefsSetting() != PrefsCollectionUI::DummyLastIntegerPref )
	{
		SetDivision( g_pcui->GetIntegerPref(GetSplitterPrefsSetting()));
	}

	if( GetViewStylePrefsSetting() != PrefsCollectionUI::DummyLastIntegerPref )
	{
		ViewStyle view_style = (ViewStyle)g_pcui->GetIntegerPref(GetViewStylePrefsSetting());
		SetViewStyle( view_style < STYLE_TREE || view_style >= STYLE_FOLDERS ? STYLE_TREE : view_style );
	}
}

/***********************************************************************************
 *
 *	UpdateCurrentFolder
 *
 ***********************************************************************************/

void OpHotlistTreeView::UpdateCurrentFolder()
{
	INT32 parent = 0;

	if (!m_hotlist_view->HasMatchText())
	{
		switch (m_view_style)
		{
			case STYLE_SPLIT:
			case STYLE_FLAT:
			{
				parent = GetCurrentFolderID();

				if (parent <= HotlistModel::MaxRoot)
					parent = -1;

				break;
			}
		}
	}

	m_hotlist_view->SetMatchParentID(parent);
}

/***********************************************************************************
 *
 *	SetCurrentFolderByID
 *
 ***********************************************************************************/

void OpHotlistTreeView::SetCurrentFolderByID(INT32 id)
{
	if (id <= HotlistModel::MaxRoot)
	{
		m_folders_view->SetSelectedItem(-1);
	}
	else
	{
		m_folders_view->SetSelectedItemByID(id);
	}

	if (m_view_style == STYLE_TREE)
	{
		m_hotlist_view->SetSelectedItemByID(id);
	}
}


/***********************************************************************************
 *
 *	GetCurrentFolderID
 *
 ***********************************************************************************/

INT32 OpHotlistTreeView::GetCurrentFolderID()
{
	if (m_view_style == STYLE_TREE)
	{
		HotlistModelItem* item = (HotlistModelItem*) m_hotlist_view->GetSelectedItem();

		if (!item || (!item->IsFolder() && !item->GetParentFolder()))
			return GetRootID();
		return item->IsFolder() ? item->GetID() : item->GetParentFolder()->GetID();
	}
	else
	{
		OpTreeModelItem* item =	m_folders_view->GetSelectedItem();
		return item ? item->GetID() : GetRootID();
	}
}

/***********************************************************************************
 *
 *	GetSelectedFolderID
 *
 ***********************************************************************************/
INT32 OpHotlistTreeView::GetSelectedFolderID()
{
	if (m_view_style == STYLE_TREE)
	{
		HotlistModelItem* item = (HotlistModelItem*) (m_hotlist_view ? m_hotlist_view->GetSelectedItem() : NULL);

		if (!item || (!item->IsFolder() && !item->GetParentFolder()))
			return GetRootID();

		return item->IsFolder() ? item->GetID() : item->GetParentFolder()->GetID();
	}
	else
	{
		OpTreeModelItem* item =	m_folders_view->GetSelectedItem();

		if( !item && m_hotlist_view )
		{
			HotlistModelItem* hmi = (HotlistModelItem*) m_hotlist_view->GetSelectedItem();
			if( hmi )
			{
				if( hmi->IsFolder() )
				{
					item = hmi;
				}
				else if( hmi->GetParentFolder() )
				{
					item = hmi->GetParentFolder();
				}
			}
		}
		return item ? item->GetID() : GetRootID();
	}
}

/***********************************************************************************
 *
 *	SetPrefsmanFlags
 *
 ***********************************************************************************/

void OpHotlistTreeView::SetPrefsmanFlags(PrefsCollectionUI::integerpref splitter_prefs_setting,
									 PrefsCollectionUI::integerpref viewstyle_prefs_setting,
									 PrefsCollectionUI::integerpref detailed_splitter_prefs_setting,
									 PrefsCollectionUI::integerpref detailed_viewstyle_prefs_setting)
{
	m_splitter_prefs_setting  = splitter_prefs_setting;
	m_viewstyle_prefs_setting = viewstyle_prefs_setting;
	m_detailed_splitter_prefs_setting  = detailed_splitter_prefs_setting;
	m_detailed_viewstyle_prefs_setting = detailed_viewstyle_prefs_setting;
}

/***********************************************************************************
 *
 *	OnDeleted
 *
 ***********************************************************************************/

void OpHotlistTreeView::OnDeleted()
{
	m_hotlist_view->RemoveTreeViewListener(this);
	SaveSettings();
	OpSplitter::OnDeleted();
}

/***********************************************************************************
 *
 * OnChange
 *
 ***********************************************************************************/

void OpHotlistTreeView::OnChange(OpWidget *widget,
							 BOOL changed_by_mouse)
{
	if (widget == m_folders_view)
	{
		UpdateCurrentFolder();
	}
	else if (GetListener())
	{
		GetListener()->OnChange(this, changed_by_mouse);
	}
}

/***********************************************************************************
 *
 *	OnItemEdited
 *
 ***********************************************************************************/

void OpHotlistTreeView::OnItemEdited(OpWidget *widget, INT32 pos, INT32 column, OpString& text)
{
	g_hotlist_manager->Rename( ((OpTreeView*)widget)->GetItemByPosition(pos), text );
}

/***********************************************************************************
 *
 * DragDropMove - Drag'n'drop helper method, handles both move and drop
 *
 ***********************************************************************************/
void OpHotlistTreeView::DragDropMove(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y, BOOL drop)
{
	DesktopDragObject* drag_object = static_cast<DesktopDragObject *>(op_drag_object);

	if (widget == m_folders_view || widget == m_hotlist_view)
	{
		OpTreeView* tree_view = (OpTreeView*) widget;

		INT32 new_selection_id = -1;

		OpTreeModelItem* item = tree_view->GetItemByPosition(pos);

		// if flat view, dropping something outside any items, means to current folder
		BOOL always_insert_into = FALSE;

		if (!item && widget == m_hotlist_view && GetViewStyle() != STYLE_TREE)
		{
			item = m_folders_view->GetSelectedItem();
			always_insert_into = TRUE;
		}

		// Find out if we're working within same hotlist model

		HotlistModelItem* model_item = NULL;

		switch (drag_object->GetType())
		{
		case DRAG_TYPE_BOOKMARK:
		case DRAG_TYPE_CONTACT:
		case DRAG_TYPE_NOTE:
		case DRAG_TYPE_SEPARATOR:
			model_item = g_hotlist_manager->GetItemByID(drag_object->GetID(0));
			break;
		}

		// Find out what drop and insert type to use

		DropType drop_type     = drag_object->GetVisualDropType();
		DesktopDragObject::InsertType insert_type = drag_object->GetSuggestedInsertType();

		// if no drop type suggested, use move

		if (drop_type == DROP_NOT_AVAILABLE || drop_type == DROP_UNINITIALIZED)
		{
			drop_type = DROP_MOVE;
		}

		if (item && !g_hotlist_manager->IsFolder(item)
			&& !g_hotlist_manager->IsGroup(item)
			&& insert_type == DesktopDragObject::INSERT_INTO)
		{
			// cannot drop into non folders

			insert_type = DesktopDragObject::INSERT_BEFORE;
		}

		// if sorted in some ways, insert before/after is not available

		if (/*tree_view->GetSortByColumn() != -1 ||*/ always_insert_into )
		{
			insert_type = DesktopDragObject::INSERT_INTO;
		}


		if (model_item && m_model == model_item->GetModel())
		{
			// Set type correctly
			INT32 target_id = item ? item->GetID() : HotlistModel::BookmarkRoot;

			HotlistModelItem* hmi_target = m_model->GetItemByID(target_id);

			if (drop)
			{
				/* Check if drop is allowed */
				if (hmi_target && !AddItemAllowed( hmi_target->GetID(),
					g_hotlist_manager->GetBookmarksModel(),
					drag_object))
				{
					ShowMaxItemsReachedDialog(hmi_target, insert_type);
					return;
				}

				GenericTreeModel::ModelLock lock(m_model);

				for (INT32 i = 0; i < drag_object->GetIDCount(); i++)
				{

					BOOL force_delete = FALSE;
					int dropped_id = -1;
					HotlistModelItem* hmi_src    = m_model->GetItemByID(drag_object->GetID(i));
					hmi_target = m_model->GetItemByID(target_id);

					//No need to move the source, since it already is where it should be
					//Also if moved, the target becomes invalid (bug 278778)
					if (hmi_src == hmi_target)
						break;

					// Do code to handle a move
					if (drop_type == DROP_MOVE && hmi_target && hmi_src)
					{
						// If the source is the trash then MOVE will be MOVE
						if (!hmi_src->GetIsInsideTrashFolder())
						{
							HotlistModelItem *target_parent_folder = hmi_target->GetIsInMoveIsCopy();

							// No moving the target folder itself into the trash, without a message,
							// And then do a direct delete
							if (hmi_src->GetTarget().HasContent() && hmi_target->GetIsTrashFolder())
							{
								// If they don't want to delete just stop.
								if (!g_hotlist_manager->ShowTargetDeleteDialog(hmi_src))
									break;

								// Otherwise actualy delete this item
								force_delete = TRUE;
							}

							// Changes move to copy when the property is set, unless you are moving within the same parent folders
							if (target_parent_folder && (target_parent_folder != hmi_src->GetIsInMoveIsCopy()))
							{
								drop_type = DROP_COPY;
							}
						}
					}

					if(AllowDropItem(hmi_target,hmi_src,insert_type,drop_type))
					{
						INT32 ret = OnDropItem(hmi_target,drag_object,i,drop_type,insert_type,&dropped_id,force_delete);

						if(ret == -1)
							break;
						else if(ret == 1)
							continue;

						// Preserve correct order. Fix for bug #173732
						// The ID can't be trusted for force deletes
						if (!force_delete)
						{
							target_id = dropped_id;
						}
					}

					if( i == 0 )
					{
						new_selection_id = dropped_id;
					}
					insert_type = DesktopDragObject::INSERT_AFTER;
				}
			}
			else 
			{
				if(AllowDropItem(hmi_target,m_model->GetItemByID(drag_object->GetID(0)),insert_type,drop_type))
				{
					// drop is allowed.. set the types
					drag_object->SetDesktopDropType(drop_type);
					drag_object->SetInsertType(insert_type);
				}
			}
		}
		else // handle drop other types
		{
			if(!OnExternalDragDrop(tree_view,item,drag_object,drop_type,insert_type,drop,new_selection_id))
				return;
		}

		if (new_selection_id != -1)
		{
			// Not if parent isn't open
			// but when in STYLE_FLAT or STYLE_SPLIT we should do it.

			if (tree_view->IsItemOpen(tree_view->GetParent(tree_view->GetItemByID(new_selection_id))) || tree_view == m_folders_view || m_view_style == STYLE_FLAT || m_view_style == STYLE_SPLIT )
			{
				tree_view->SetSelectedItemByID(new_selection_id, FALSE, TRUE, TRUE);
			}
		}
	}
}

/***********************************************************************************
 *
 *	OnDragMove
 *
 ***********************************************************************************/
void OpHotlistTreeView::OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y)
{
	DragDropMove(widget, drag_object, pos, x, y, FALSE);
}

/***********************************************************************************
 *
 *	OnDragDrop
 *
 ***********************************************************************************/

void OpHotlistTreeView::OnDragDrop(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	DragDropMove(widget, op_drag_object, pos, x, y, TRUE);
}

/***********************************************************************************
 *
 *	AddItemAllowed
 *
 *  @param drag_object - If drag_object NULL, checks if adding one item is allowed.
 *
 *  @return TRUE if the maximum items limit on folder with id id will
 *               still be respected if the items in drag_object are added to it.
 *
 ***********************************************************************************/

BOOL OpHotlistTreeView::AddItemAllowed(INT32 id, HotlistModel* model, DesktopDragObject* drag_object, INT32 num_items)
{
	if (id == -1 || !model)
		return TRUE;

	HotlistModelItem* parent_item = model->GetItemByID(id);

	if ( !parent_item )
		return TRUE;

	if (!parent_item->IsFolder())
	{
		parent_item = parent_item->GetParentFolder();
	}

	HotlistModelItem* max_item_parent = 0;
	if (parent_item)
	{
		max_item_parent = parent_item->GetIsInMaxItemsFolder();
	}

	if (!drag_object)
	{
		OP_ASSERT(num_items > 0);
		if (max_item_parent && max_item_parent->GetMaxItems() > 0
			&& max_item_parent->GetChildCount() + num_items > max_item_parent->GetMaxItems())
		{
			return FALSE;
		}
	}
	else
	{
		INT32 i;
		// If at least one item is not inside this target folder, do the test
		for (i = 0; i < drag_object->GetIDCount(); i++)
		{
			HotlistModelItem* hmi_src = model->GetItemByID(drag_object->GetID(i));

			if (hmi_src && max_item_parent && (max_item_parent != hmi_src->GetIsInMaxItemsFolder()))
			{
				INT32 item_count = drag_object->GetIDCount();

				if (max_item_parent && max_item_parent->GetMaxItems() > 0
					&& max_item_parent->GetChildCount() + item_count > max_item_parent->GetMaxItems())
					{
						return FALSE;
					}
			}
		}
	}

	return TRUE;
}

/***********************************************************************************
 *
 *	ShowMaxItemsReachedDialog
 *
 ***********************************************************************************/

void OpHotlistTreeView::ShowMaxItemsReachedDialog(HotlistModelItem* hmi_target, DesktopDragObject::InsertType insert_type)
{
	if (hmi_target)
	{
		HotlistModelItem* parent_item = hmi_target->GetIsInMaxItemsFolder();

		if (!parent_item)
		{
			if (hmi_target->IsFolder())
				parent_item = hmi_target;
			else
				parent_item = hmi_target->GetParentFolder();
		}

		OpString title, message, format;
		g_languageManager->GetString(Str::D_BOOKMARK_PROP_MAX_ITEMS_TITLE, title);

		g_languageManager->GetString(Str::D_BOOKMARK_PROP_MAX_ITEMS_TEXT, format);
		message.AppendFormat(format.CStr(), parent_item ? parent_item->GetMaxItems() : 0);

		SimpleDialog::ShowDialog(WINDOW_NAME_MAX_BOOKMARKS_REACHED, NULL, message.CStr(), title.CStr(), Dialog::TYPE_OK, Dialog::IMAGE_INFO);
	}
}

/***********************************************************************************
 *
 *	OnInputAction
 *
 ***********************************************************************************/

BOOL OpHotlistTreeView::OnInputAction(OpInputAction* action)
{
	BOOL trash_selected, special_folder, special_folder_selected;

	OpTreeView* tree_view = GetTreeViewFromInputContext(action->GetFirstInputContext());

	HotlistModelItem* item = (HotlistModelItem*) tree_view->GetSelectedItem();

	if (item)
	{
		// Flag to know if the only item that is selected is the trash folder
		trash_selected = tree_view->GetSelectedItemCount() == 1 && item->GetIsTrashFolder();

		// Selected item is special folder...
		special_folder = item->GetIsSpecialFolder();

		if (special_folder)
			special_folder_selected = tree_view->GetSelectedItemCount() == 1;
		else
		{
			// ...or inside special folder
			special_folder = item->GetParentFolder() && item->GetParentFolder()->GetIsSpecialFolder();
			special_folder_selected = FALSE;
		}
	}
	else // !item
	{
		trash_selected = FALSE;
		special_folder = FALSE;
		special_folder_selected = FALSE;
	}

	// these actions don't need a selected item
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			
			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_SHOW_ON_PERSONAL_BAR:
				case OpInputAction::ACTION_HIDE_FROM_PERSONAL_BAR:
				{
					if (item && !item->IsSeparator())
						child_action->SetSelectedByToggleAction(OpInputAction::ACTION_SHOW_ON_PERSONAL_BAR, g_hotlist_manager->IsOnPersonalBar(item));
					else
						child_action->SetEnabled(FALSE);

					return TRUE;
				}

				case OpInputAction::ACTION_NEW_FOLDER:
				{
					BOOL folder_allowed = (!item || item->GetSubfoldersAllowed());
					if (item)
					{
						if (item->GetIsInsideTrashFolder())
						{
							folder_allowed = FALSE;
						}
						else
						{
							HotlistModelItem* max_items_folder = item->GetIsInMaxItemsFolder();
							
							if (max_items_folder
								&& max_items_folder->GetMaxItems() > 0
								&& max_items_folder->GetChildCount() >= max_items_folder->GetMaxItems())
								{
									folder_allowed = FALSE;
								}
						}
					}

					child_action->SetEnabled( folder_allowed );
					return TRUE;
				}
					break;
				case OpInputAction::ACTION_NEW_SEPERATOR:
				{
					BOOL separator_allowed = (!item || item->GetSeparatorsAllowed() && !item->GetIsInsideTrashFolder());
					child_action->SetEnabled( !special_folder_selected && !special_folder && separator_allowed );
					return TRUE;
				}
				break;

				case OpInputAction::ACTION_VIEW_STYLE:
				{
					child_action->SetSelected(child_action->GetActionData() == m_view_style);
					return TRUE;
				}

				case OpInputAction::ACTION_OPEN_LINK:
				case OpInputAction::ACTION_OPEN_LINK_IN_NEW_WINDOW:
				case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW:
				if (item && item->IsFolder() && !item->IsSeparator())
				{
					//Fix for bug 209849 : [REGRESSION] Can't open all bookmarks
					child_action->SetEnabled( TRUE );
					return TRUE;
				}
				case OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE:
				case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE:
				{
					child_action->SetEnabled(!item || !item->IsSeparator()/*TRUE*/);
					return TRUE;
				}
				case OpInputAction::ACTION_DELETE:
				case OpInputAction::ACTION_CUT:
				{

					BOOL selected_deletable = TRUE;
					BOOL target_folder = FALSE;
					if (item)
					{
						selected_deletable = !(item && !item->GetIsDeletable());

						// Only change the cut
						if (child_action->GetAction() == OpInputAction::ACTION_CUT)
							target_folder = item->GetTarget().HasContent();
					}
					child_action->SetEnabled(tree_view->GetSelectedItemCount() > 0
											 && !trash_selected
											 && selected_deletable
											 && !target_folder);
					return TRUE;
				}
				case OpInputAction::ACTION_COPY:
				{
					child_action->SetEnabled(tree_view->GetSelectedItemCount() > 0 && !trash_selected);
					//&& !special_folder_selected);
					return TRUE;
				}
				case OpInputAction::ACTION_EMPTY_TRASH:
				{
					// enable as soon as a trashed item is found
					if (m_model)
					{
						for (INT32 i = 0; i < m_model->GetItemCount(); i++)
						{
							HotlistModelItem * item = m_model->GetItemByIndex(i);
							if (item && (!item->GetIsTrashFolder() && item->GetIsInsideTrashFolder()))
							{
								child_action->SetEnabled(/*trash_selected ?*/ TRUE);
								return TRUE;
							}
						}
					}					
					child_action->SetEnabled(FALSE);
					return TRUE;
				}
			}
			break;

		}// End case ACTION_GET_ACTION_STATE

		case OpInputAction::ACTION_LOWLEVEL_PREFILTER_ACTION:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED:
				{
					BOOL state = g_op_system_info->GetShiftKeyState(); // Only Enter and nothing but Enter shall trigger this.
					if (child_action->GetActionData() == OP_KEY_ENTER && state == 0 && child_action->GetFirstInputContext() == m_hotlist_view && m_hotlist_view->GetEditPos() == -1)
					{
						return OpenSelectedFolder();
					}
					break;
				}
			}
			break;
		}

		case OpInputAction::ACTION_SHOW_ON_PERSONAL_BAR:
		case OpInputAction::ACTION_HIDE_FROM_PERSONAL_BAR:
		{
			BOOL result = FALSE;
			OpINT32Vector id_list;
			UINT32 item_count = tree_view->GetSelectedItems(id_list);
			for( UINT32 i=0; i<item_count; i++ )
			{
				result |= g_hotlist_manager->ShowOnPersonalBar( id_list.Get(i), action->GetAction() == OpInputAction::ACTION_SHOW_ON_PERSONAL_BAR );
			}
			return result;
		}

		case OpInputAction::ACTION_VIEW_STYLE:
		{
			SetViewStyle((ViewStyle) action->GetActionData(),TRUE);
			SaveSettings();
			g_prefsManager->CommitL();
			return TRUE;
		}

		case OpInputAction::ACTION_EMPTY_TRASH:
			g_hotlist_manager->EmptyTrash( m_model );
			return TRUE;
		break;

		case OpInputAction::ACTION_COPY:
		{
			OpINT32Vector id_list;
			tree_view->GetSelectedItems( id_list );
			if( id_list.GetCount() == 0 )
			{
				return FALSE;
			}
			else if( action->GetAction() == OpInputAction::ACTION_COPY )
			{
				g_hotlist_manager->Copy( id_list );
			}
			return TRUE;
		}
		break;

		case OpInputAction::ACTION_PASTE:
		{
			g_hotlist_manager->Paste( m_model, item );
			return TRUE;
		}
		break;

		case OpInputAction::ACTION_OPEN_LINK:
		case OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE:
		case OpInputAction::ACTION_OPEN_LINK_IN_NEW_WINDOW:
		case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE:
		case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW:
		{
			if (action->GetActionData() == 0)
			{

				OpINT32Vector list;
				tree_view->GetSelectedItems(list);
				if( list.GetCount() > 0 )
				{
					if( action->GetAction() == OpInputAction::ACTION_OPEN_LINK )
					{
						OpenUrls(list, MAYBE, MAYBE, MAYBE, 0, -1, action->IsKeyboardInvoked());
					}

					else if( action->GetAction() == OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE )
					{
						OpenUrls(list, NO, YES, NO );
					}
					else if( action->GetAction() == OpInputAction::ACTION_OPEN_LINK_IN_NEW_WINDOW )
					{
						OpenUrls(list, YES, NO, NO );
					}
					else if( action->GetAction() == OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE )
					{
						OpenUrls(list, NO, YES, YES );
					}
					else if( action->GetAction() == OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW )
				    {
						OpenUrls(list, YES, NO, YES );
					}
				}
				return TRUE;
			}
			break;
		}

		case OpInputAction::ACTION_SHOW_CONTEXT_MENU:
		{
			ShowContextMenu(GetBounds().Center(),TRUE, tree_view,TRUE);
			return TRUE;
		}

		case OpInputAction::ACTION_RENAME:
		{
			//Let the user rename the item (name) directly in the treeview
			tree_view->EditItem(tree_view->GetSelectedItemPos());
			return TRUE;
		}

		case OpInputAction::ACTION_GET_TYPED_OBJECT:
		{
			if (action->GetActionData() == WIDGET_TYPE_TREEVIEW)
			{
				action->SetActionObject(m_hotlist_view);
				return TRUE;
			}
			return FALSE;
		}
	}
	
	return tree_view->OnInputAction(action);
}

/***********************************************************************************
 *
 *	OpenSelectedFolder
 *
 ***********************************************************************************/

BOOL OpHotlistTreeView::OpenSelectedFolder()
{
	if (m_view_style != STYLE_SPLIT && m_view_style != STYLE_FLAT)
	{
		return FALSE;
	}

	HotlistModelItem* item = (HotlistModelItem*) m_hotlist_view->GetSelectedItem();

	if (!item || !item->IsFolder())
	{
		return FALSE;
	}

	// if the folder is the same we're in, go up

	if (GetCurrentFolderID() == item->GetID())
	{
		HotlistModelItem* parent = (HotlistModelItem*) m_hotlist_view->GetParentByPosition(m_hotlist_view->GetSelectedItemPos());

		if (parent)
		{
			m_folders_view->SetSelectedItemByID(parent->GetID());
		}
		else
		{
			m_folders_view->SetSelectedItem(-1);
		}
	}
	else
	{
		m_folders_view->SetSelectedItemByID(item->GetID());
	}

	return TRUE;
}

/***********************************************************************************
 *
 * OpenUrls
 *
 * @param list - list of ids of items to open
 *
 * Open the bookmarks in list in the order of their sorting in the hotlist view
 *
 ***********************************************************************************/
// Inn: Id_liste -> GetOpenUrls -> Får indeksene til disse -> ordered_list

BOOL OpHotlistTreeView::OpenUrls( const OpINT32Vector& list,
							  BOOL3 new_window,
							  BOOL3 new_page,
							  BOOL3 in_background,
							  DesktopWindow* target_window,
							  INT32 position,
							  BOOL ignore_modifier_keys )
{

	OpINT32Vector index_list;
	GetSortedItemList(list, index_list);
	return g_hotlist_manager->OpenOrderedUrls(index_list, new_window, new_page, in_background, target_window, position, NULL, ignore_modifier_keys );
}

/***********************************************************************************
 *
 * GetSortedItemList
 *
 * @param list - list of ids of items to open
 * @param index_list - list of indices of the items in the order of
 *                      their sorting in the hotlist view
 *
 *
 ***********************************************************************************/
void OpHotlistTreeView::GetSortedItemList( const OpINT32Vector& list, OpINT32Vector& index_list)
{
	OpINT32Vector ordered_list;

	// translate to the indexes in the model
	g_hotlist_manager->GetOpenUrls(index_list, list);

	UINT32 i;
	for (i = 0; i < index_list.GetCount(); i++)
	{
		// Get the index in the view model by the index in the model itself
		INT32 j = m_hotlist_view->GetItemByModelPos(index_list.Get(i));
		ordered_list.Add(j);
	}

	ordered_list.Sort();
	index_list.Clear();

	// then create the index list with the model indices
	for (i = 0; i < ordered_list.GetCount(); i++)
	{
		index_list.Add(m_hotlist_view->GetModelPos(ordered_list.Get(i)));
	}
}

BOOL OpHotlistTreeView::OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked)
{
	INT32 x = menu_point.x;
	INT32 y = menu_point.y;

	if( widget && widget->GetType() == WIDGET_TYPE_TREEVIEW )
	{
		// Needed for split view layout
		x += widget->GetRect(FALSE).x;
		y += widget->GetRect(FALSE).y;

		ShowContextMenu(OpPoint(x,y),FALSE, (OpTreeView*)widget,FALSE);
	}

	return TRUE;
}

/***********************************************************************************
 *
 *	OnMouseEvent
 *
 ***********************************************************************************/

void OpHotlistTreeView::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	HotlistModelItem* item = (HotlistModelItem*) ((OpTreeView*)widget)->GetItemByPosition(pos);

	// From here on we need to have a valid item

	if (item == NULL)
	{
		return;
	}

	// double clicking a folder in flat hotlist view should go into that folder or maybe up
	if (widget == m_hotlist_view && nclicks == 2 && button == MOUSE_BUTTON_1)
	{
		if (OpenSelectedFolder())
		{
			return;
		}
	}

	if (down && nclicks == 1 && button == MOUSE_BUTTON_3)
	{
		if (!g_hotlist_manager->IsFolder(item))
		{
			g_input_manager->InvokeAction(OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE);
			return;
		}
		else if(g_hotlist_manager->IsBookmarkFolder(item))
		{
			g_input_manager->InvokeAction(OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE);
			return;
		}
	}

	BOOL shift = GetWidgetContainer()->GetView()->GetShiftKeys() & SHIFTKEY_SHIFT;
	BOOL ctrl = GetWidgetContainer()->GetView()->GetShiftKeys() & SHIFTKEY_CTRL;
	if (ctrl && !shift)
	{
		// This combination is now allowed here
		return;
	}

	// only left mouse button click should pass, but folders should not act on just clicking unless shift it down.. use menu
	if (button != MOUSE_BUTTON_1 || (g_hotlist_manager->IsFolder(item) && !shift))
		return;

	HandleMouseEvent(static_cast<OpTreeView*>(widget),item,pos,button,down,nclicks);
}

BOOL OpHotlistTreeView::AllowDropItem(HotlistModelItem* to, HotlistModelItem* from, DesktopDragObject::InsertType& insert_type, DropType& drop_type)
{
	// Do not move on same item or between models.
	if( !from || from == to  )
	{
		return FALSE;
	}

	if( m_model != from->GetModel() )
	{
		return FALSE;
	}

	if (to)
	{
		if ( !from->GetIsInsideTrashFolder() && to->GetIsInMoveIsCopy())
		{
			if (drop_type == DROP_MOVE)
			{
				drop_type = DROP_COPY;
			}
		}

		// Don't move folders into folders that don't allow subfolders
		if (from && from->IsFolder())
		{
			if (to)
			{
				if (!to->GetSubfoldersAllowed()
					|| (to->GetParentFolder()
						&& !to->GetParentFolder()->GetSubfoldersAllowed()))
					{
						return FALSE;
					}
			}
		}
		// Don't move separators into folders that don't allow them
		if (from && from->IsSeparator())
		{
			if (to)
			{
				if (!to->GetSeparatorsAllowed()
					|| (to->GetParentFolder()
						&& !to->GetParentFolder()->GetSeparatorsAllowed()))
					{
						return FALSE;
					}
			}
		}

		// Make sure trash and target folders don't change level
		// Note: this code well the "else if" will need to be modified once target folders are
		// not only on the root as they are now.
		if (from->GetIsTrashFolder() || (from->IsFolder() && from->GetTarget().HasContent()))
		{
			// Don't move the trash folder into another folder
			if (insert_type == DesktopDragObject::INSERT_INTO && to->IsFolder())
				return FALSE;
			else if ((insert_type == DesktopDragObject::INSERT_BEFORE || insert_type == DesktopDragObject::INSERT_AFTER) && to->GetParentFolder())
				return FALSE;
		}

		// Do not move or copy a folder into its own child tree
		if( from->IsFolder() )
		{
			HotlistModelItem* hmi = to->GetParentFolder();
			while( hmi )
			{
				if( hmi == from )
				{
					return FALSE;
				}
				hmi = hmi->GetParentFolder();
			}
		}
	}

	return TRUE;
}

void OpHotlistTreeView::OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	if(new_input_context == m_hotlist_view || new_input_context == m_folders_view)
	{
		m_last_focused_view = static_cast<OpTreeView*>(new_input_context);
		g_input_manager->UpdateAllInputStates();
	}
}

OpTreeView* OpHotlistTreeView::GetTreeViewFromInputContext(OpInputContext* input_context)
{
	if( input_context == m_folders_view )
		return m_folders_view;
	else if(input_context == m_hotlist_view)
		return m_hotlist_view;
	else if(m_last_focused_view)
		return m_last_focused_view;
	else 
		return m_hotlist_view;
}

