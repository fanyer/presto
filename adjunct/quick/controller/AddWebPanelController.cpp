#include "core/pch.h"

#include "adjunct/quick/controller/AddWebPanelController.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick/hotlist/Hotlist.h"
#include "adjunct/quick/panels/HotlistPanel.h"
#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/quick_toolkit/widgets/QuickRichTextLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickAddressDropDown.h"
#include "adjunct/quick_toolkit/widgets/QuickEdit.h"

void AddWebPanelController::InitL()
{
	DocumentDesktopWindow *document_window = g_application->GetActiveDocumentDesktopWindow();
	if (!document_window)
		LEAVE(OpStatus::ERR_NULL_POINTER);
	
	LEAVE_IF_ERROR(SetDialog("Add Web Panel Dialog"));

	InitAddressL();
	SetupGetWebPabelLabelL();
}

BOOL AddWebPanelController::CanHandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_OPEN_URL_IN_NEW_PAGE:
		case OpInputAction::ACTION_GO_TO_TYPED_ADDRESS:
			 return TRUE;
	}

	return OkCancelDialogContext::CanHandleAction(action);
}

OP_STATUS AddWebPanelController::HandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_OPEN_URL_IN_NEW_PAGE:
		{	
			CancelDialog();

			if (!g_application->GetInputContext())
				return OpStatus::ERR;

			return g_application->GetInputContext()->OnInputAction(action);
		}

		case OpInputAction::ACTION_GO_TO_TYPED_ADDRESS:
		case OpInputAction::ACTION_OK:
		{
			QuickAddressDropDown *address_drop_down = m_dialog->GetWidgetCollection()->Get<QuickAddressDropDown>("address_inputbox");
			RETURN_VALUE_IF_NULL(address_drop_down, OpStatus::ERR);

			// Close dropdown list
			if (action->GetAction() == OpInputAction::ACTION_GO_TO_TYPED_ADDRESS 
				&& address_drop_down->GetOpWidget()->DropdownIsOpen())
			{
				address_drop_down->GetOpWidget()->ShowDropdown(FALSE);
				return  OpStatus::OK;
			}

			BookmarkItemData bookmark_item;
			bookmark_item.panel_position = 0;
			RETURN_IF_ERROR(address_drop_down->GetText(bookmark_item.url));
			if (bookmark_item.url.IsEmpty())
				return  OpStatus::OK;

			OpString resolved_address;
			BOOL is_resolved;
			TRAPD(status, is_resolved = g_url_api->ResolveUrlNameL(bookmark_item.url, resolved_address, TRUE));
			if (!OpStatus::IsSuccess(status) && !is_resolved)
				return  OpStatus::OK;

			RETURN_IF_ERROR(bookmark_item.url.Set(resolved_address));

			const uni_char *doc_url = GetCurrentURL();
			if (doc_url && *doc_url && bookmark_item.url.Compare(doc_url) == 0)
				OpStatus::Ignore(bookmark_item.name.Set(g_application->GetActiveDocumentDesktopWindow()->GetWindowCommander()->GetCurrentTitle()));

			HotlistModelItem *hotlist_item = GetHotlistItem(bookmark_item.url.CStr());
			if (hotlist_item)
			{
				if (!hotlist_item->GetInPanel())
					g_desktop_bookmark_manager->SetItemValue(hotlist_item, bookmark_item, TRUE, HotlistModel::FLAG_PANEL);
			}
			else
				g_desktop_bookmark_manager->NewBookmark(bookmark_item, HotlistModel::BookmarkRoot);
			
			if (hotlist_item == NULL)
				hotlist_item = GetHotlistItem(bookmark_item.url.CStr());

			if (hotlist_item)
				ShowPanel(*hotlist_item, bookmark_item);

			if (OpInputAction::ACTION_GO_TO_TYPED_ADDRESS == action->GetAction())
			{
				g_input_manager->InvokeAction(OpInputAction::ACTION_OK, 0, NULL, this);
				return  OpStatus::OK;
			}

			break;
		}
	}

	return OkCancelDialogContext::HandleAction(action);
}

void AddWebPanelController::InitAddressL()
{
	QuickAddressDropDown *address_drop_down = m_dialog->GetWidgetCollection()->GetL<QuickAddressDropDown>("address_inputbox");

	ANCHORD(OpAddressDropDown::InitInfo, info);
	info.m_max_lines = 10;

	const uni_char *doc_url = GetCurrentURL();
	if (doc_url && *doc_url) // Get current page URL
		info.m_url_string.SetL(doc_url);

	address_drop_down->GetOpWidget()->InitAddressDropDown(info);
}

void AddWebPanelController::SetupGetWebPabelLabelL()
{
	ANCHORD(OpString, rich_text_label);
	rich_text_label.SetL("<a href=\"opera:/action/Open url in new page, &quot;http://my.opera.com/community/customize/panel/&quot;\">%s</a>");
	ANCHORD(OpString, get_panel_str);
	QuickRichTextLabel *rich_label = m_dialog->GetWidgetCollection()->GetL<QuickRichTextLabel>("get_web_panel");
	g_languageManager->GetStringL(Str::D_CUSTOMIZE_DIALOG_GET_WEB_PANEL, get_panel_str);

	ANCHORD(OpString, final_text);
	LEAVE_IF_ERROR(final_text.AppendFormat(rich_text_label.CStr(), get_panel_str.CStr()));
	LEAVE_IF_ERROR(rich_label->SetText(final_text));
}

void AddWebPanelController::ShowPanel(HotlistModelItem &hotlist_item, BookmarkItemData &bookmark_item)
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

HotlistModelItem* AddWebPanelController::GetHotlistItem(const uni_char *url_str)
{
	BookmarkModel* model = g_hotlist_manager->GetBookmarksModel();
	if (!model)
		return NULL;

	return model->GetByURL(url_str, FALSE);
}

const uni_char* AddWebPanelController::GetCurrentURL()
{
	DocumentDesktopWindow *doc_window = g_application->GetActiveDocumentDesktopWindow();
	if (!doc_window)
		return NULL;

	return doc_window->GetWindowCommander()->GetCurrentURL(TRUE);
}
