#include "core/pch.h"

#include "adjunct/quick/widgets/FavoritesCompositeListener.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/widgets/OpAddressDropDown.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/managers/SpeedDialManager.h"

#include "modules/widgets/OpEdit.h"

FavoritesCompositeListener::FavoritesCompositeListener(OpAddressDropDown *addr_dd)
	: m_dropdown(addr_dd)
{ 
	OpStatus::Ignore(g_main_message_handler->SetCallBack(this, MSG_SPEEDDIALS_OR_BOOKMARKS_LIST_IS_CHANGED, reinterpret_cast<MH_PARAM_1>(this)));

	if (g_bookmark_manager->IsBusy())
		OpStatus::Ignore(g_desktop_bookmark_manager->AddBookmarkListener(this));
	else
		g_hotlist_manager->GetBookmarksModel()->AddModelListener(this);

	OpStatus::Ignore(g_speeddial_manager->AddListener(*this));

}

FavoritesCompositeListener::~FavoritesCompositeListener()
{
	g_main_message_handler->UnsetCallBack(this ,MSG_SPEEDDIALS_OR_BOOKMARKS_LIST_IS_CHANGED, reinterpret_cast<MH_PARAM_1>(this));

	if (g_speeddial_manager)
		OpStatus::Ignore(g_speeddial_manager->RemoveListener(*this));

	if (g_desktop_bookmark_manager)
		OpStatus::Ignore(g_desktop_bookmark_manager->RemoveBookmarkListener(this));

	if (g_hotlist_manager && g_hotlist_manager->GetBookmarksModel())
		g_hotlist_manager->GetBookmarksModel()->RemoveModelListener(this);
}

void FavoritesCompositeListener::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg != MSG_SPEEDDIALS_OR_BOOKMARKS_LIST_IS_CHANGED)
		return;

	OpVector<DesktopWindow> windows;
	g_application->GetDesktopWindowCollection().GetDesktopWindows(OpTypedObject::WINDOW_TYPE_BROWSER, windows);
	for (UINT32 i = 0; i < windows.GetCount(); i++)
	{
		DocumentDesktopWindow* doc_win = static_cast<BrowserDesktopWindow*>(windows.Get(i))->GetActiveDocumentDesktopWindow();
		if (doc_win && doc_win->GetAddressDropdown())
			doc_win->GetAddressDropdown()->UpdateFavoritesFG(false);
	}
}

void FavoritesCompositeListener::OnBookmarkModelLoaded()
{
	g_hotlist_manager->GetBookmarksModel()->AddModelListener(this);
	UpdateFavoritesWidget();
}

void FavoritesCompositeListener::OnHotlistItemAdded(HotlistModelItem* item)
{
	UpdateFavoritesWidget();
}

void FavoritesCompositeListener::OnHotlistItemChanged(HotlistModelItem* item, BOOL moved_as_child, UINT32 changed_flag)
{
	if (!item->GetIsInsideTrashFolder())
		UpdateFavoritesWidget();
}

void FavoritesCompositeListener::OnSpeedDialAdded(const DesktopSpeedDial& entry)
{
	UpdateFavoritesWidget();
}

void FavoritesCompositeListener::OnSpeedDialRemoving(const DesktopSpeedDial& entry)
{
	UpdateFavoritesWidget();
}

void FavoritesCompositeListener::OnHotlistItemRemoved(HotlistModelItem* item, BOOL removed_as_child)
{
	UpdateFavoritesWidget();
}

void FavoritesCompositeListener::OnHotlistItemTrashed(HotlistModelItem* item)
{
	UpdateFavoritesWidget();
}

void FavoritesCompositeListener::UpdateFavoritesWidget()
{
	DocumentDesktopWindow* doc_win = g_application->GetActiveDocumentDesktopWindow();
	if (doc_win && doc_win->GetAddressDropdown() != m_dropdown)
		return;

	g_main_message_handler->RemoveDelayedMessage(MSG_SPEEDDIALS_OR_BOOKMARKS_LIST_IS_CHANGED, reinterpret_cast<MH_PARAM_1>(this), 0);
	g_main_message_handler->PostMessage(MSG_SPEEDDIALS_OR_BOOKMARKS_LIST_IS_CHANGED, reinterpret_cast<MH_PARAM_1>(this), 0);
}
