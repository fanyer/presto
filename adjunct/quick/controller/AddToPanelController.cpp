#include "core/pch.h"

#include "adjunct/quick/controller/AddToPanelController.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/hotlist/Hotlist.h"
#include "adjunct/quick/panels/HotlistPanel.h"
#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick/managers/DesktopBookmarkManager.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickIcon.h"
#include "adjunct/quick_toolkit/widgets/QuickLabel.h"

AddToPanelController::AddToPanelController()
	: m_hotlist_item(NULL)
{

}

void AddToPanelController::InitL()
{
	LEAVE_IF_ERROR(SetDialog("Add To Panel Dialog"));

	LEAVE_IF_ERROR(m_dialog->GetWidgetCollection()->GetL<QuickLabel>("panel_title")->SetText(m_bookmark_item_data.name));
	LEAVE_IF_ERROR(m_dialog->GetWidgetCollection()->GetL<QuickLabel>("address_string")->SetText(m_bookmark_item_data.url));
	
	Image img = g_favicon_manager->Get(m_bookmark_item_data.url.CStr());
	if (!img.IsEmpty())
	{
		QuickIcon *icon = m_dialog->GetWidgetCollection()->GetL<QuickIcon>("panel_icon");
		icon->SetBitmapImage(img);
	}
}

OP_STATUS AddToPanelController::InitPanel(const uni_char *title, const uni_char *url, bool &show_dialog)
{
	show_dialog = false;

	RETURN_IF_ERROR(m_bookmark_item_data.name.Set(title));
	RETURN_IF_ERROR(m_bookmark_item_data.url.Set(url));
	m_bookmark_item_data.panel_position = 0;
	
	// Open panel if item exists
	m_hotlist_item = GetHotlistItem(url);
	if (m_hotlist_item && m_hotlist_item->GetInPanel())
		ShowPanel(*m_hotlist_item, m_bookmark_item_data);
	else
		show_dialog = true;

	return OpStatus::OK;
}

void AddToPanelController::OnOk()
{
	if (m_bookmark_item_data.url.IsEmpty())
		return;

	if (!m_hotlist_item)
		g_desktop_bookmark_manager->NewBookmark(m_bookmark_item_data, HotlistModel::BookmarkRoot);
	else if(!m_hotlist_item->GetInPanel())
		g_desktop_bookmark_manager->SetItemValue(m_hotlist_item, m_bookmark_item_data, TRUE, HotlistModel::FLAG_PANEL);

	if (m_hotlist_item == NULL)
		m_hotlist_item = GetHotlistItem(m_bookmark_item_data.url.CStr());

	if (m_hotlist_item)
		ShowPanel(*m_hotlist_item, m_bookmark_item_data);
}

HotlistModelItem* AddToPanelController::GetHotlistItem(const uni_char *url_str)
{
	BookmarkModel* model = g_hotlist_manager->GetBookmarksModel();
	if (!model)
		return NULL;

	return model->GetByURL(url_str, FALSE);
}

void AddToPanelController::ShowPanel(HotlistModelItem &hotlist_item, BookmarkItemData &bookmark_item)
{
	BrowserDesktopWindow *browser_wnd = g_application->GetActiveBrowserDesktopWindow();
	if (!browser_wnd || !browser_wnd->GetHotlist())
		return;
	
	Hotlist* hotlist = browser_wnd->GetHotlist();
	if (hotlist->GetAlignment() == OpBar::ALIGNMENT_OFF)
		g_input_manager->InvokeAction(OpInputAction::ACTION_SET_ALIGNMENT, OpBar::ALIGNMENT_OLD_VISIBLE, UNI_L("hotlist"));

	for (INT32 count = hotlist->GetPanelCount() - 1; count >= 0; count--)
	{
		HotlistPanel *panel = hotlist->GetPanel(count);
		if (panel->GetType() != PANEL_TYPE_WEB && panel->GetID() != hotlist_item.GetID())
			continue;
			
		hotlist->SetSelectedPanel(hotlist_item.GetPanelPos());
		break;
	}
}
