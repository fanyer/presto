#include "core/pch.h"

#include "adjunct/quick/controller/FavoritesOverlayController.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick/managers/DesktopBookmarkManager.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/speeddial/SpeedDialData.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick_toolkit/widgets/QuickOverlayDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickButton.h"
#include "adjunct/quick_toolkit/widgets/CalloutDialogPlacer.h"
#include "adjunct/quick_toolkit/widgets/QuickComposite.h"
#include "adjunct/quick_toolkit/widgets/QuickPagingLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickStackLayout.h"
#include "adjunct/quick_toolkit/widgets/OpTreeViewDropdown.h"
#include "adjunct/quick/quick-widget-names.h"
#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/desktop_util/treemodel/optreemodelitem.h"
#include "modules/widgets/WidgetContainer.h"

FavoritesOverlayController::FavoritesOverlayController(OpWidget* anchor_widget)
	: Base(anchor_widget)
	, m_auto_hide_timer(NULL)
	, m_browser_window(NULL)
	, m_document_window(NULL)
	, m_dropdown(NULL)
	, m_bm_folder_id(-1)
	, m_dropdown_needs_closing(false)
{
}

FavoritesOverlayController::~FavoritesOverlayController()
{
	if (m_browser_window)
		m_browser_window->RemoveListener(this);

	if (m_document_window)
		m_document_window->RemoveListener(this);

	if (m_auto_hide_timer)
	{
		m_auto_hide_timer->Stop();
		OP_DELETE(m_auto_hide_timer);
	}

	if (m_dropdown)
	{
		m_dropdown->OnDeleted();
		OP_DELETE(m_dropdown); 
	}
}

void FavoritesOverlayController::OnKeyboardInputGained(OpInputContext* new_context, OpInputContext* old_context, FOCUS_REASON reason)
{
	if (new_context->GetParentInputContext() == this)
	{
		QuickButton *btn = m_dialog->GetWidgetCollection()->Get<QuickButton>("add_remove_to_bookmarks");
		if (btn && btn->IsVisible())
		{
			btn->GetOpWidget()->SetFocus(reason);
		}
	}
}

void FavoritesOverlayController::InitL()
{
	m_document_window = g_application->GetActiveDocumentDesktopWindow();
	LEAVE_IF_NULL(m_document_window);
	m_document_window->AddListener(this);

	if ((m_browser_window = g_application->GetActiveBrowserDesktopWindow()))
		m_browser_window->AddListener(this);

	QuickOverlayDialog* overlay_dlg = OP_NEW_L(QuickOverlayDialog, ());
	LEAVE_IF_ERROR(SetDialog("URL Fav Menu Dialog", overlay_dlg));

	CalloutDialogPlacer *placer = OP_NEW_L(CalloutDialogPlacer, (*GetAnchorWidget()));
	overlay_dlg->SetDialogPlacer(placer);
	overlay_dlg->SetAnimationType(QuickOverlayDialog::DIALOG_ANIMATION_CALLOUT);

	// Construct dropdown
	LEAVE_IF_ERROR(OpTreeViewDropdown::Construct(&m_dropdown, FALSE));
	m_dropdown->SetType(OpTypedObject::WIDGET_TYPE_DROPDOWN_WITHOUT_EDITBOX);
	m_dropdown->SetListener(this);
	m_dropdown->SetParentInputContext(this);
	m_dropdown->SetVisibility(FALSE);

	// Tweak dialog's buttons
	QuickButton *btn = overlay_dlg->GetWidgetCollection()->GetL<QuickButton>("add_remove_to_bookmarks");
	btn->GetOpWidget()->SetEllipsis(ELLIPSIS_END);

	// The widget 'add_remove_to_bookmarks' funtions as 'remove from bookmark' 
	// if item was bookmarked earlier. This requires skin adjustments and 
	// making 'bookmark_folder_list' widget invisible which is visually an
	// integral component of 'add_remove_to_bookmarks' button.
	if (GetBookmarkItemOfDoc())
	{
		// Apply new skin to 'add_remove_to_bookmarks' widget if item is bookmarked.
		btn->GetOpWidget()->GetBorderSkin()->SetImage("Rich Menu Button Skin");

		// Hide 'bookmark_folder_list' widget.
		btn = overlay_dlg->GetWidgetCollection()->GetL<QuickButton>("bookmark_folder_list");
		btn->Hide();
	}
}

void FavoritesOverlayController::OnUiClosing()
{
	// The OpWidget tree is about to disappear, but we manage the dropdown's
	// lifetime on our own, so we remove it from that tree first.
	if (m_dropdown->GetParent())
		m_dropdown->Remove();

	Base::OnUiClosing();
}

BOOL FavoritesOverlayController::CanHandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_REMOVE_FROM_STARTPAGE:
		case OpInputAction::ACTION_ADD_TO_STARTPAGE:
		case OpInputAction::ACTION_REMOVE_FROM_FAVORITES:
		case OpInputAction::ACTION_ADD_TO_FAVORITES:
		case OpInputAction::ACTION_MENU_FOLDER:
		{
			return TRUE;
		}
	}

	return Base::CanHandleAction(action);
}


BOOL FavoritesOverlayController::DisablesAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_REMOVE_FROM_STARTPAGE:
		{
			return GetDocPosInSDList() == -1;
		}
		case OpInputAction::ACTION_ADD_TO_STARTPAGE:
		{
			return GetDocPosInSDList() != -1 || !g_speeddial_manager->HasLoadedConfig();
		}
		case OpInputAction::ACTION_REMOVE_FROM_FAVORITES:
		{
			return GetBookmarkItemOfDoc() == NULL;
		}
		case OpInputAction::ACTION_ADD_TO_FAVORITES:
		{
			return GetBookmarkItemOfDoc() != NULL || !g_desktop_bookmark_manager->GetBookmarkModel()->Loaded();
		}
		case OpInputAction::ACTION_MENU_FOLDER:
		{
			return !IsThereAnyFolderInBookmarkList();
		}
	}

	return Base::DisablesAction(action);
}

OP_STATUS FavoritesOverlayController::HandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_REMOVE_FROM_STARTPAGE:
		{
			int pos =  GetDocPosInSDList();
			if (pos == -1)
				return OpStatus::OK;

			RETURN_VALUE_IF_ERROR(g_speeddial_manager->RemoveSpeedDial(pos, FALSE), OpStatus::OK);
			
			ShowActionResultAndTriggerCloseDlgTimer(true);
			
			return OpStatus::OK;
		}

		case OpInputAction::ACTION_ADD_TO_STARTPAGE:
		{
			if (!m_document_window)
				return OpStatus::OK;

			const uni_char *doc_url = m_document_window->GetWindowCommander()->GetCurrentURL(TRUE);
			if (!doc_url)
				return OpStatus::OK;

			SpeedDialData sd_data;
			RETURN_VALUE_IF_ERROR(sd_data.SetURL(doc_url), OpStatus::OK);
			RETURN_VALUE_IF_ERROR(sd_data.SetTitle(m_document_window->GetWindowCommander()->GetCurrentTitle(), FALSE), OpStatus::OK);
			RETURN_VALUE_IF_ERROR(g_speeddial_manager->InsertSpeedDial(g_speeddial_manager->GetTotalCount() - 1, &sd_data), OpStatus::OK);

			ShowActionResultAndTriggerCloseDlgTimer(false);
		
			return OpStatus::OK;
		}

		case OpInputAction::ACTION_REMOVE_FROM_FAVORITES:
		{
			HotlistModelItem* item = GetBookmarkItemOfDoc();
			if (!item)
				return OpStatus::OK;

			BookmarkModel* model = g_hotlist_manager->GetBookmarksModel();
			if (!model->DeleteItem(item, TRUE))
				return OpStatus::OK;

			ShowActionResultAndTriggerCloseDlgTimer(true);

			return OpStatus::OK;
		}

		case OpInputAction::ACTION_ADD_TO_FAVORITES:
		{
			if (!m_document_window)
				return OpStatus::OK;

			INT32 id = -1;
			BookmarkItemData item_data;
			WindowCommanderProxy::AddToBookmarks(m_document_window->GetWindowCommander(), item_data, action->GetActionData(), id);
			if (!g_desktop_bookmark_manager->NewBookmark(item_data, m_bm_folder_id))
				return OpStatus::OK;

			ShowActionResultAndTriggerCloseDlgTimer(false);

			return OpStatus::OK;
		}

		case OpInputAction::ACTION_MENU_FOLDER:
		{
			if (!m_dropdown_needs_closing)
			{
				m_dropdown_needs_closing = true;

				ShowBookmarkFolderList();
			}
			else
			{
				if (m_dropdown->GetTreeViewWindow())
					m_dropdown->GetTreeViewWindow()->ClosePopup();

				m_dropdown_needs_closing = false;
				
				UpdateDropDownCompositeAction(true);
			}
	
			return OpStatus::OK;
		}
	}

	return Base::HandleAction(action);
}

void FavoritesOverlayController::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (button != MOUSE_BUTTON_1)
		return;
	
	if (widget == m_dropdown 
		&& nclicks == 1 && down)
	{
		m_dropdown_needs_closing = false;

		UpdateDropDownCompositeAction(true);

		m_bm_folder_id = -1;

		FavoriteFolder *item = static_cast<FavoriteFolder*>(m_dropdown->GetSelectedItem(&m_bm_folder_id));
		if (!item)
		{
			m_dropdown->ShowDropdown(FALSE);
			return;
		}

		m_bm_folder_id = item->GetFolderID();

		QuickButton *btn = m_dialog->GetWidgetCollection()->Get<QuickButton>("add_remove_to_bookmarks");
		if (!btn)
		{
			m_dropdown->ShowDropdown(FALSE);
			return;
		}
		
		// To prevent button from growing lengthwise setting its width fixed
		btn->SetFixedWidth(btn->GetMinimumWidth());
		
		OpStringC folder_name = item->GetName();
		if (folder_name.HasContent())
		{
			OpString txt, str;
			RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::S_URL_ADD_TO_SELECTED_BM_FOLDER, str));
			RETURN_VOID_IF_ERROR(txt.AppendFormat(str.CStr(), folder_name.CStr()));
			RETURN_VOID_IF_ERROR(btn->SetText(txt));
		}

		m_dropdown->ShowDropdown(FALSE);
	}
}

void FavoritesOverlayController::OnDesktopWindowActivated(DesktopWindow* desktop_window, BOOL active)
{
	if (!active && desktop_window == m_browser_window)
	{
		if (m_dropdown_needs_closing && m_dropdown->GetTreeViewWindow())
			m_dropdown->GetTreeViewWindow()->ClosePopup();

		CancelDialog();
	}
}

void FavoritesOverlayController::OnDesktopWindowShown(DesktopWindow* desktop_window, BOOL shown)
{
	UpdateDropDownBtnSkin(shown ? true : false);

	if (shown)
		UpdateDropDownCompositeAction(false);
}

void FavoritesOverlayController::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	if (desktop_window == m_browser_window)
		m_browser_window = NULL;
	else if ( desktop_window == m_document_window)
		m_document_window = NULL;
	else
	{
		UpdateDropDownBtnSkin(false);
		DesktopWindow* dialog_desktop_window = m_dialog->GetDesktopWindow();
		VisualDevice* visual_device = dialog_desktop_window->GetWidgetContainer()->GetRoot()->GetVisualDevice();
		OpPoint point;
		visual_device->GetView()->GetMousePos(&point.x, &point.y);
		point = visual_device->ScaleToDoc(point);
		OpRect rect = dialog_desktop_window->GetWidgetContainer()->GetRoot()->GetRect();
		if (rect.Contains(point))
		{
			OpWidget* captured_widget = g_widget_globals->captured_widget;
			if (!captured_widget || (captured_widget && captured_widget->GetType() == OpTypedObject::WIDGET_TYPE_ROOT))
			{
				m_dropdown_needs_closing = false;
				UpdateDropDownCompositeAction(true);
			}
		}
		else
		{
			CancelDialog();
		}
	}
}

void FavoritesOverlayController::OnTimeOut(OpTimer* timer)
{
	if (timer == m_auto_hide_timer)
	{
		m_auto_hide_timer->Stop();
		OP_DELETE(m_auto_hide_timer);
		m_auto_hide_timer = NULL;

		CancelDialog();
	}
}

OP_STATUS FavoritesOverlayController::CreateBookmarkFolderListModel(BookmarkModel **folderlist_model)
{
	OpAutoPtr<BookmarkModel> bm_folderlist_model(OP_NEW(BookmarkModel,()));
	RETURN_OOM_IF_NULL(bm_folderlist_model.get());

	FavoriteFolderSortListener sort_listener;
	bm_folderlist_model->SetSortListener(&sort_listener);

	// Special items
	OpAutoPtr<FavoriteFolder>bmf_item(OP_NEW(FavoriteFolder, ()));
	RETURN_OOM_IF_NULL(bmf_item.get());
	RETURN_IF_ERROR(bmf_item->SetRootFolder());
	bm_folderlist_model->AddLast(bmf_item.release(), -1);
	
	BookmarkModel* bmm = g_hotlist_manager->GetBookmarksModel();
	if (!bmm)
		return OpStatus::ERR_NULL_POINTER;

	INT32 count = bmm->GetCount();
	for (INT32 i = 0; i < count; i++)
	{
		HotlistModelItem*  item = bmm->GetItemByIndex(i);
		if (item && item->IsFolder() && !item->GetIsTrashFolder() && !item->GetIsInsideTrashFolder())
		{
			OpAutoPtr<FavoriteFolder>bmf_item(OP_NEW(FavoriteFolder, ()));
			RETURN_OOM_IF_NULL(bmf_item.get());
			RETURN_IF_ERROR(bmf_item->SetFolderTitle(item->GetName(), item->GetHistoricalID()));
			bm_folderlist_model->AddSorted(bmf_item.release(), -1);
		}
	}
	
	bm_folderlist_model->SetSortListener(NULL);

	*folderlist_model = bm_folderlist_model.release();
	return OpStatus::OK;
}

void FavoritesOverlayController::ShowBookmarkFolderList()
{
	if (!m_dropdown->GetParent())
		// This is so that the dropdown popup has a parent DesktopWindow like it
		// should have.
		m_dialog->GetDesktopWindow()->GetWidgetContainer()->GetRoot()->AddChild(m_dropdown);

	m_bm_folder_id = -1;

	QuickComposite* composite = m_dialog->GetWidgetCollection()->Get<QuickComposite>("drop_down_btns_composite");
	if (!composite)
		return;

	OpRect rect_final = composite->GetOpWidget()->GetScreenRect();
	rect_final.y += rect_final.height;

	if (m_dropdown->GetTreeViewWindow())
		m_dropdown->GetTreeViewWindow()->ClosePopup();

	BookmarkModel *folderlist_model;
	RETURN_VOID_IF_ERROR(CreateBookmarkFolderListModel(&folderlist_model));
	OpAutoPtr<BookmarkModel> bm_folderlist_model(folderlist_model);
	
	m_dropdown->SetMaxNumberOfColumns(1);

	RETURN_VOID_IF_ERROR(m_dropdown->InitTreeViewDropdown("Bookmark Folder Treeview Window Skin"));
	m_dropdown->GetTreeViewWindow()->GetTreeView()->GetBorderSkin()->SetImage("Bookmark Folder Treeview Skin");
	RETURN_VOID_IF_ERROR(m_dropdown->SetModel(bm_folderlist_model.release(), TRUE));
	m_dropdown->SetTreeViewName(WIDGET_NAME_ADDRESSFIELD_FAV_TREEVIEW);
	m_dropdown->GetTreeViewWindow()->AddDesktopWindowListener(this);

	OpTreeView *view =m_dropdown->GetTreeView();
	for (INT32 i = view->GetColumnCount() - 1; i > 0 ; i--)
		view->SetColumnVisibility(i, FALSE);
	
	INT32 item_height =m_dropdown->GetTreeView()->GetLineHeight();
	if (item_height != rect_final.height && rect_final.height > item_height)
	{
		item_height = rect_final.height - item_height;
		m_dropdown->GetTreeView()->SetExtraLineHeight(item_height);
	}

	m_dropdown->SetMaxLines(7);
	m_dropdown->SetInvokeActionOnClick(FALSE);
	m_dropdown->ShowButton(FALSE);
	m_dropdown->SetAllowWrappingOnScrolling(TRUE);
	m_dropdown->SetEllipsis(ELLIPSIS_END);
	m_dropdown->SetMinHeight(rect_final.height);
	m_dropdown->SetMinHeight(rect_final.height);

	m_dropdown->ShowDropdown(TRUE, rect_final);
}

void FavoritesOverlayController::ShowActionResultAndTriggerCloseDlgTimer(bool is_action_type_delete)
{
	UINT32 timeout = 400;
	
	QuickButton *btn = m_dialog->GetWidgetCollection()->Get<QuickButton>("add_remove_status_btn");
	if (btn)
	{
		OpString status_txt;
		if (is_action_type_delete)
			RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::S_URL_REMOVED_NOTIFICATION, status_txt));
		else
			RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::S_URL_ADDED_NOTIFICATION, status_txt));

		if (OpStatus::IsError(btn->SetText(status_txt)))
			timeout = 0;
		
		QuickPagingLayout *pages = m_dialog->GetWidgetCollection()->Get<QuickPagingLayout>("pages");
		if (pages)
			pages->GoToPage(1);
	}
	
	m_auto_hide_timer = OP_NEW(OpTimer, ());
	if (m_auto_hide_timer)
	{
		m_auto_hide_timer->SetTimerListener(this);
		m_auto_hide_timer->Start(timeout);
	}
	else
		CancelDialog();
}

int FavoritesOverlayController::GetDocPosInSDList(bool include_display_url)
{
	if (!m_document_window)
		return -1;

	const uni_char *doc_url = m_document_window->GetWindowCommander()->GetCurrentURL(TRUE);
	if (!doc_url)
		return -1;

	return g_speeddial_manager->FindSpeedDialByUrl(doc_url, true);
}

HotlistModelItem* FavoritesOverlayController::GetBookmarkItemOfDoc()
{
	if (!m_document_window)
		return NULL;

	const uni_char *doc_url = m_document_window->GetWindowCommander()->GetCurrentURL(TRUE);
	if (!doc_url)	
		return NULL;
	
	return g_hotlist_manager->GetBookmarksModel()->GetByURL(doc_url, false, true);
}

void FavoritesOverlayController::UpdateDropDownBtnSkin(bool is_dd_open)
{
	QuickComposite *composite = m_dialog->GetWidgetCollection()->Get<QuickComposite>("drop_down_btns_composite");
	if (!composite)
		return;

	composite->GetOpWidget()->GetBorderSkin()->SetImage(is_dd_open ? 
		"Composite Dropdown Open Button Skin" : "Composite Dropdown Button Skin");

	QuickButton *btn = m_dialog->GetWidgetCollection()->Get<QuickButton>("add_remove_to_bookmarks");
	if (!btn)
		return;
		
	if (is_dd_open)
	{
		OpString txt;
		RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::S_URL_ADD_TO_BM_FOLDER, txt));
		RETURN_VOID_IF_ERROR(btn->SetText(txt));
		btn->GetOpWidget()->GetBorderSkin()->SetImage("Composite Dropdown Left Button Skin");
		btn->GetOpWidget()->GetForegroundSkin()->SetImage("Favorites Fake Skin");
		btn->GetOpWidget()->SetFixedImage(TRUE);
	}
	else
	{
		if (m_bm_folder_id == -1)
		{
			OpString txt;
			RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::S_URL_ADD_TO_FAVORITES, txt));
			RETURN_VOID_IF_ERROR(btn->SetText(txt));
		}
		
		btn->GetOpWidget()->GetBorderSkin()->SetImage("Composite Dropdown Left Button Skin");
		btn->GetOpWidget()->SetFixedImage(FALSE);
		btn->GetOpWidget()->SetUpdateNeeded(TRUE);
		btn->GetOpWidget()->UpdateActionStateIfNeeded();
	}
}

void FavoritesOverlayController::UpdateDropDownCompositeAction(bool is_reset)
{
	QuickComposite *composite = m_dialog->GetWidgetCollection()->Get<QuickComposite>("drop_down_btns_composite");
	if (!composite)
		return;

	QuickButton *btn = m_dialog->GetWidgetCollection()->Get<QuickButton>("bookmark_folder_list");
	if (!btn)
		return;

	if (is_reset)
		composite->GetOpWidget()->SetActionButton(NULL);
	else
		composite->GetOpWidget()->SetActionButton(btn->GetOpWidget());
}

bool FavoritesOverlayController::IsThereAnyFolderInBookmarkList()
{
	BookmarkModel* bmm = g_hotlist_manager->GetBookmarksModel();
	if (!bmm)
		return false;

	INT32 count = bmm->GetCount();
	for (INT32 i = 0; i < count; i++)
	{
		HotlistModelItem*  item = bmm->GetItemByIndex(i);
		if (item && item->IsFolder() && !item->GetIsTrashFolder() && !item->GetIsInsideTrashFolder())
			return true;
	}

	return false;
}

FavoriteFolder::FavoriteFolder()
	: m_show_separator(FALSE)
	, m_bm_folder_id(-1)
{

}

OP_STATUS FavoriteFolder::SetRootFolder()
{
	OpString str;
	RETURN_IF_ERROR(m_item_image.Set("Root Folder"));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_URL_BOOKMARK_ROOT_FOLDER, str));
	RETURN_IF_ERROR(m_folder_title.Set(str));
	m_show_separator = true;
	SetIsExpandedFolder(FALSE);
	SetName(str.CStr());
	return OpStatus::OK;
}

OP_STATUS FavoriteFolder::SetFolderTitle(const OpStringC& folder_title, INT32 bookmark_id)
{
	RETURN_IF_ERROR(m_item_image.Set("Sub Folder"));
	RETURN_IF_ERROR(m_folder_title.Set(folder_title));
	m_bm_folder_id = bookmark_id;
	SetName(folder_title);
	SetIsExpandedFolder(FALSE);
	SetIsActive(FALSE);
	return OpStatus::OK;
}

OP_STATUS FavoriteFolder::GetItemData(ItemData* item_data)
{
	if (item_data->query_type == SKIN_QUERY)
	{
		if (m_show_separator)
			item_data->skin_query_data.skin = "Favorites Dropdown Separator Treeview Item Skin";
		else
			item_data->skin_query_data.skin = "Favorites Dropdown Treeview Item Skin";
		
		return OpStatus::OK;
	}
	else if (item_data->query_type == COLUMN_QUERY && item_data->column_query_data.column == 0)
	{
		RETURN_IF_ERROR(item_data->column_query_data.column_text->Set(m_folder_title));
		item_data->column_query_data.column_image = m_item_image.CStr();
		item_data->column_query_data.column_indentation = 0;
		return OpStatus::OK;
	}

	return FolderModelItem::GetItemData(item_data);
}

INT32 FavoriteFolderSortListener::OnCompareItems(OpTreeModel* tree_model, OpTreeModelItem* item0, OpTreeModelItem* item1)
{
	FavoriteFolder* folder0 = static_cast<FavoriteFolder*>(item0);
	FavoriteFolder* folder1 = static_cast<FavoriteFolder*>(item1);

	if (folder0->GetFolderID() == -1)
		return -1;
	else if (folder1->GetFolderID() == -1)
		return 1;
	else
		return folder0->GetFolderTitle().CompareI(folder1->GetFolderTitle());
}

