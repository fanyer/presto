
#include "core/pch.h"

#if defined(M2_SUPPORT) && defined(IRC_SUPPORT)

#include "ChatPanel.h"

#include "adjunct/m2_ui/models/chatroomsmodel.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/m2_ui/dialogs/JoinChatDialog.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/m2_ui/windows/ChatDesktopWindow.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/widgets/WidgetContainer.h"

/***********************************************************************************
**
**	ChatPanel
**
***********************************************************************************/

ChatPanel::ChatPanel()
{
}

OP_STATUS ChatPanel::Init()
{
	RETURN_IF_ERROR(OpTreeView::Construct(&m_rooms_view));
	AddChild(m_rooms_view, TRUE);

	m_rooms_view->SetListener(this);
	m_rooms_view->SetShowColumnHeaders(FALSE);
	m_rooms_view->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL);
	m_rooms_view->GetBorderSkin()->SetImage("Panel Treeview Skin", "Treeview Skin");

	OpTreeModel* rooms_model;
	g_m2_engine->GetChatRoomsModel(&rooms_model);
	m_rooms_view->SetTreeModel(rooms_model, 0);
	// m_rooms_view->SetAction(new OpInputAction(OpInputAction::ACTION_JOIN_CHAT_ROOM));

	SetToolbarName("Chat Panel Toolbar", "Chat Full Toolbar");
	SetName("Chat");

	return OpStatus::OK;
}

/***********************************************************************************
**
**	GetPanelText
**
***********************************************************************************/

void ChatPanel::GetPanelText(OpString& text, Hotlist::PanelTextType text_type)
{
	g_languageManager->GetString(Str::DI_IDSTR_M2_HOTLISTTAB_CHAT, text);
}

/***********************************************************************************
**
**	OnFullModeChanged
**
***********************************************************************************/

void ChatPanel::OnFullModeChanged(BOOL full_mode)
{
	m_rooms_view->SetLinkColumn(IsSingleClick() ? 0 : -1);
}

/***********************************************************************************
**
**	OnLayout
**
***********************************************************************************/

void ChatPanel::OnLayoutPanel(OpRect& rect)
{
	m_rooms_view->SetRect(rect);
}

/***********************************************************************************
**
**	OnFocus
**
***********************************************************************************/

void ChatPanel::OnFocus(BOOL focus,FOCUS_REASON reason)
{
	if (focus)
	{
		m_rooms_view->SetFocus(reason);
	}
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL ChatPanel::OnInputAction(OpInputAction* action)
{
	INT32 account_id = 0;
	INT32 id = 0;
	OpString name;
	Type type = UNKNOWN_TYPE;
	BOOL is_room = FALSE;
	BOOL is_chatter = FALSE;
	BOOL is_account = FALSE;

	OpTreeModelItem* item = m_rooms_view->GetSelectedItem();

	if (item)
	{
		id = item->GetID();
		type = item->GetType();

		switch (type)
		{
			case CHATROOM_TYPE:
		{
				ChatRoom* chat_room = g_m2_engine->GetChatRoom(id);

			if (chat_room)
			{
					chat_room->GetName(name);
					account_id = chat_room->GetAccountID();
					is_room = TRUE;
			}
				break;
			}

			case CHATTER_TYPE:
			{
				ChatRoom* chatter = g_m2_engine->GetChatter(id);

				if (chatter)
				{
					chatter->GetName(name);
					account_id = chatter->GetAccountID();
					is_chatter = TRUE;
				}
				break;
		}

			case CHATROOM_SERVER_TYPE:
			{
				account_id = item->GetID();
				is_account = TRUE;
				break;
			}
		}
	}

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_JOIN_CHAT_ROOM:
				{
					if (action->GetActionDataString())
						return FALSE;

					child_action->SetEnabled(is_room);
					return TRUE;
				}

				case OpInputAction::ACTION_LEAVE_CHAT_ROOM:
				{
					if (action->GetActionDataString())
						return FALSE;

					child_action->SetEnabled(is_room && ChatDesktopWindow::FindChatRoom(account_id, name, TRUE));
					return TRUE;
				}

				case OpInputAction::ACTION_SET_CHAT_STATUS:
				{
					if (child_action->GetActionData())
						return FALSE;

					AccountTypes::ChatStatus action_chat_status = g_application->GetChatStatusFromString(child_action->GetActionDataString());

					BOOL is_connecting = FALSE;
					AccountTypes::ChatStatus chat_status = g_application->GetChatStatus(account_id, is_connecting);

					child_action->SetSelected(chat_status == action_chat_status);

					if (is_connecting)
						child_action->SetEnabled(action_chat_status == AccountTypes::OFFLINE);
					else
						child_action->SetEnabled(g_application->SetChatStatus(account_id, chat_status, TRUE));

					return TRUE;
				}
				case OpInputAction::ACTION_DELETE:
				{
					child_action->SetEnabled(is_room || is_account);
					return TRUE;
				}
				case OpInputAction::ACTION_EDIT_PROPERTIES:
				{
					child_action->SetEnabled(is_account || is_chatter || ChatDesktopWindow::FindChatRoom(account_id, name, TRUE));
					return TRUE;
				}
			}
			break;
		}

		case OpInputAction::ACTION_SET_CHAT_STATUS:
		{
			if (action->GetActionData())
				return FALSE;

			return g_application->SetChatStatus(account_id, g_application->GetChatStatusFromString(action->GetActionDataString()));
		}

		case OpInputAction::ACTION_DELETE:
		{
			if (is_room)
			{
				ChatDesktopWindow* chat_window = ChatDesktopWindow::FindChatRoom(account_id, name, TRUE);

				if (chat_window)
				{
					chat_window->LeaveChat();
					chat_window->Close();
				}

				g_m2_engine->DeleteChatRoom(id);
			}
			else if (is_account)
			{
				g_application->DeleteAccount(account_id, GetParentDesktopWindow());
			}
			return TRUE;
		}
		case OpInputAction::ACTION_LIST_CHAT_ROOMS:
		{
			if (!action->GetActionData())
			{
				// set account and let action continue
				action->SetActionData(account_id);
			}
			break;
		}
		case OpInputAction::ACTION_NEW_CHAT_ROOM:
		case OpInputAction::ACTION_JOIN_CHAT_ROOM:
		{
			if (!action->GetActionData())
			{
				// set account and let action continue
				action->SetActionData(account_id);
			}
			if (!action->GetActionDataString() && is_room)
			{
				action->SetActionDataString(name.CStr());
			}
			break;
		}

		case OpInputAction::ACTION_SHOW_CONTEXT_MENU:
			return ShowContextMenu(GetBounds().Center(),TRUE,TRUE);
			break;

		case OpInputAction::ACTION_LEAVE_CHAT_ROOM:
		{
			if (action->GetActionDataString() || !is_room)
				break;

			return g_application->LeaveChatRoom(account_id, name);
		}

		case OpInputAction::ACTION_EDIT_PROPERTIES:
		{
			if (is_account)
			{
				g_application->EditAccount(account_id, GetParentDesktopWindow());
			}
			else if (is_chatter)
			{
				HotlistModelItem* item = g_hotlist_manager->GetContactsModel()->GetByNickname(name);

				if (item)
				{
					g_hotlist_manager->EditItem(item->GetID(), GetParentDesktopWindow());
				}
			}
			else
			{
				ChatDesktopWindow* chat_window = ChatDesktopWindow::FindChatRoom(account_id, name, TRUE);
				if(chat_window)
				    chat_window->EditRoomProperties();
			}

			return TRUE;
		}
	}
	return FALSE;
}

/***********************************************************************************
**
**	OnMouseEvent
**
***********************************************************************************/

void ChatPanel::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (widget != m_rooms_view)
	{
		return;
	}

	if (!down && button == MOUSE_BUTTON_2)
	{
		ShowContextMenu(OpPoint(x+widget->GetRect().x,y+widget->GetRect().y),FALSE,FALSE);
		return;
	}

	OpTreeModelItem* item;

	item = m_rooms_view->GetItemByPosition(pos);

	if (item == NULL)
	{
		return;
	}

	BOOL click_state_ok = (IsSingleClick() && !down && nclicks == 1) || nclicks == 2;

	if (click_state_ok && button == MOUSE_BUTTON_1)
	{
		if (item->GetType() == OpTreeModelItem::CHATROOM_TYPE)
		{
			ChatRoom* chat_room = g_m2_engine->GetChatRoom(item->GetID());

			if (chat_room)
			{
				OpString room;
				chat_room->GetName(room);

				ChatInfo chat_info(room, OpStringC());

				g_application->GoToChat(chat_room->GetAccountID(),
					chat_info, TRUE);
			}
		}
		else if (item->GetType() == OpTreeModelItem::CHATTER_TYPE)
		{
			ChatRoom* chatter = g_m2_engine->GetChatter(item->GetID());
			if (chatter)
			{
				OpString chatter_name;
				chatter->GetName(chatter_name);

				ChatInfo chat_info(chatter_name, OpStringC());

				g_application->GoToChat(chatter->GetAccountID(),
					chat_info, FALSE);
			}
		}
	}
}

BOOL ChatPanel::ShowContextMenu(const OpPoint &point, BOOL center, BOOL use_keyboard_context)
{
	OpPoint p = point + GetScreenRect().TopLeft();
	g_application->GetMenuHandler()->ShowPopupMenu("Chat Item Popup Menu", PopupPlacement::AnchorAt(p, center), 0, use_keyboard_context);
	return TRUE;
}


#endif // M2_SUPPORT && IRC_SUPPORT
