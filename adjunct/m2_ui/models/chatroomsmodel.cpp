
#include "core/pch.h"

#ifdef IRC_SUPPORT

#include "chatroomsmodel.h"
#include "chattersmodel.h"
#include "adjunct/m2/src/engine/chatinfo.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/message.h"
#include "adjunct/m2/src/engine/progressinfo.h"
#include "adjunct/m2/src/engine/store.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/hotlist/HotlistModelItem.h"
#include "modules/locale/oplanguagemanager.h"

UINT32 ChatRoomsModelItem::s_room_id_counter = 1;

/***********************************************************************************
**
**	ChatRoomsModelItem
**
***********************************************************************************/

ChatRoomsModelItem::ChatRoomsModelItem(Type type, INT32 account_id, const OpStringC& name)
:	m_type(type),
	m_account_id(account_id),
	m_room_chatter_id(0),
	m_chatters_model(NULL),
	m_chat_status(AccountTypes::ONLINE),
	m_contact(0)
{
	SetName(name);

	if (!IsServer())
	{
		m_room_chatter_id = s_room_id_counter++;
	}

	if (g_hotlist_manager)
		g_hotlist_manager->GetContactsModel()->AddModelListener(this);
}

ChatRoomsModelItem::~ChatRoomsModelItem()
{
	LeftRoom();

	if (g_hotlist_manager)
		g_hotlist_manager->GetContactsModel()->RemoveModelListener(this);
}

OP_STATUS ChatRoomsModelItem::SetName(const OpStringC& name)
{
	m_contact = g_hotlist_manager->GetContactsModel()->GetByNickname(name);
	return m_name.Set(name);
}


OP_STATUS ChatRoomsModelItem::GetItemData(ItemData* item_data)
{
	if (item_data->query_type == INIT_QUERY)
	{
		if (m_type == CHATROOM_SERVER_TYPE)
		{
			item_data->flags |= FLAG_INITIALLY_OPEN;
		}
		return OpStatus::OK;
	}

	if (item_data->query_type != COLUMN_QUERY)
		return OpStatus::OK;

	item_data->column_query_data.column_text->Set(GetName());

	if (m_type == CHATROOM_SERVER_TYPE)
	{
		Account* account = 0;
		RETURN_IF_ERROR(((AccountManager*)MessageEngine::GetInstance()->GetAccountManager())->GetAccountById(m_account_id, account));

		//ProgressInfo progress;
		OpString status;

		//if (account->GetBackendProgress(TRUE, progress) == OpStatus::OK && progress.GetStatus() == AccountTypes::CONNECTING)
			//account->GetBackendProgressText(TRUE, progress, status);
		//else
		{
			// TODO: Fix translations! Use same string IDs as those used in chat status menus!
			Str::LocaleString status_str = Str::NOT_A_STRING;

			BOOL is_connecting = FALSE;
			switch (account->GetChatStatus(is_connecting))
			{
				case AccountTypes::ONLINE:
					status_str = Str::SI_IDSTR_IM_STATUS_1;
					break;
				case AccountTypes::BUSY:
					status_str = Str::SI_IDSTR_IM_STATUS_4;
					break;
				case AccountTypes::BE_RIGHT_BACK:
					status_str = Str::SI_IDSTR_IM_STATUS_3;
					break;
				case AccountTypes::AWAY:
					status_str = Str::SI_IDSTR_IM_STATUS_2;
					break;
				case AccountTypes::ON_THE_PHONE:
					status_str = Str::SI_IDSTR_IM_STATUS_7;
					break;
				case AccountTypes::OUT_TO_LUNCH:
					status_str = Str::SI_IDSTR_IM_STATUS_10;
					break;
				case AccountTypes::APPEAR_OFFLINE:
					status_str = Str::SI_IDSTR_IM_STATUS_6;
					break;
			}
			g_languageManager->GetString(status_str, status);
		}

		if (status.HasContent())
		{
			item_data->column_query_data.column_text->Append(UNI_L(" ("));
			item_data->column_query_data.column_text->Append(status);
			item_data->column_query_data.column_text->Append(UNI_L(")"));
		}

//		BOOL open = item_data->flags & FLAG_OPEN;

		item_data->column_query_data.column_image = "Group";
		item_data->column_query_data.column_text_color = OP_RGB(0x78, 0x50, 0x28);

		item_data->column_query_data.column_sort_order = account->GetAccountId();
	}
	else if (m_type == CHATROOM_TYPE)
	{
		ChattersModel* model = GetModel()->GetChattersModel(m_account_id, GetName());

		if (model)
		{
			item_data->flags |= FLAG_BOLD;
			uni_char buf[16];
			uni_sprintf(buf,UNI_L(" (%i)"), model->GetItemCount());
			item_data->column_query_data.column_text->Append(buf);
		}

		item_data->column_query_data.column_image = model ? "Chat Joined room" : "Chat Room";
		item_data->column_query_data.column_sort_order = 1;
	}
	else if (m_type == CHATTER_TYPE)
	{
		item_data->column_query_data.column_image = m_contact ? m_contact->GetImage() : "Contact Unknown";

		switch (m_chat_status)
		{
			case AccountTypes::BUSY:
				item_data->column_query_data.column_image_2 = "Status Busy";
				break;

			case AccountTypes::BE_RIGHT_BACK:
				item_data->column_query_data.column_image_2 = "Status Be Right Back";
				break;

			case AccountTypes::AWAY:
				item_data->column_query_data.column_image_2 = "Status Away";
				break;

			case AccountTypes::ON_THE_PHONE:
				item_data->column_query_data.column_image_2 = "Status On The Phone";
				break;

			case AccountTypes::OUT_TO_LUNCH:
				item_data->column_query_data.column_image_2 = "Status Out To Lunch";
				break;
		}

		item_data->column_query_data.column_sort_order = 2;
	}

	return OpStatus::OK;
}

int	ChatRoomsModelItem::GetID()
{
	int id = -1;

	switch (m_type)
	{
		case CHATROOM_TYPE :
		case CHATTER_TYPE :
			id = m_room_chatter_id;
			break;
		case CHATROOM_SERVER_TYPE :
			id = m_account_id;
			break;
		default :
			OP_ASSERT(0);
			break;
	}

	return id;
}


OP_STATUS ChatRoomsModelItem::GetName(OpString& name)
{
	RETURN_IF_ERROR(name.Set(m_name));
	return OpStatus::OK;
}


ChatRoomsModelItem* ChatRoomsModelItem::GetRoomItem(const OpStringC& room)
{
	ChatRoomsModelItem* item = GetChildItem();

	while (item)
	{
		if ((item->GetType() == CHATROOM_TYPE) &&
			(item->m_name.CompareI(room) == 0))
		{
			return item;
		}

		item = item->GetSiblingItem();
	}

	return NULL;
}


ChatRoomsModelItem* ChatRoomsModelItem::GetChatterItem(const OpStringC& nickname)
{
	ChatRoomsModelItem* item = GetChildItem();

	while (item)
	{
		if ((item->GetType() == CHATTER_TYPE) &&
			(item->m_name.CompareI(nickname) == 0))
		{
			return item;
		}

		item = item->GetSiblingItem();
	}

	return NULL;
}


Account* ChatRoomsModelItem::GetAccount()
{
	AccountManager* account_manager = MessageEngine::GetInstance()->GetAccountManager();
	Account* account = NULL;

	account_manager->GetAccountById(m_account_id, account);

	return account;
}

void ChatRoomsModelItem::JoinedRoom()
{
	if (m_chatters_model)
		return;

    if (!(m_chatters_model = OP_NEW(ChattersModel, ())))
		return;

	m_chatters_model->Init(m_account_id, m_name);

	Change();
}

void ChatRoomsModelItem::LeftRoom()
{
	if (!m_chatters_model)
		return;

	OP_DELETE(m_chatters_model);
	m_chatters_model = NULL;
	Change();
}

Chatter* ChatRoomsModelItem::GetChatter(const OpStringC& chatter)
{
	return m_chatters_model ? m_chatters_model->GetChatter(chatter) : NULL;
}

/***********************************************************************************
**
**	ChatRoomsModel
**
***********************************************************************************/

ChatRoomsModel::ChatRoomsModel()
{
}


ChatRoomsModel::~ChatRoomsModel()
{
	MessageEngine::GetInstance()->RemoveAccountListener(this);
	MessageEngine::GetInstance()->RemoveChatListener(this);
}


OP_STATUS ChatRoomsModel::Init()
{
	RETURN_IF_ERROR(MessageEngine::GetInstance()->AddAccountListener(this));
	return MessageEngine::GetInstance()->AddChatListener(this);
}

INT32 ChatRoomsModel::GetColumnCount()
{
	return 1;
}

OP_STATUS ChatRoomsModel::GetColumnData(ColumnData* column_data)
{
	RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_COL_IRCROOM, column_data->text));

	return OpStatus::OK;
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
OP_STATUS ChatRoomsModel::GetTypeString(OpString& type_string)
{
	return g_languageManager->GetString(Str::D_SUBSCRIBE_CHAT, type_string);
}
#endif

/***********************************************************************************
**
**	AddChatRoom
**
***********************************************************************************/

ChatRoomsModelItem* ChatRoomsModel::AddChatRoom(UINT16 account_id, const OpStringC& chat_room)
{
	ChatRoomsModelItem* parent = GetAccountItem(account_id);

	if (!parent)
		return NULL;

	ChatRoomsModelItem* item = parent->GetRoomItem(chat_room);

	if (item)
		return item;

	item = OP_NEW(ChatRoomsModelItem, (OpTypedObject::CHATROOM_TYPE, account_id, chat_room));
	if (!item)
		return NULL;

	parent->AddChildLast(item);

	Account* account = item->GetAccount();

	if (account)
	{
		OpString folder_name;
		folder_name.Set(chat_room);

		account->AddSubscribedFolder(folder_name);
	}

	return item;
}

/***********************************************************************************
**
**	DeleteChatRoom
**
***********************************************************************************/

void ChatRoomsModel::DeleteChatRoom(UINT32 room_id)
{
	ChatRoomsModelItem* item = GetItemByIDAndType(room_id, OpTypedObject::CHATROOM_TYPE);

	if (!item)
		return;

	Account* account = item->GetAccount();

	if (account)
	{
		account->RemoveSubscribedFolder(item->GetName());
	}

	item->Delete();
}

/***********************************************************************************
**
**	AddChatter / DeleteChatter
**
***********************************************************************************/

ChatRoomsModelItem* ChatRoomsModel::AddChatter(UINT16 account_id, const OpStringC& nickname, AccountTypes::ChatStatus chat_status)
{
	ChatRoomsModelItem* parent = GetAccountItem(account_id);

	if (!parent)
		return NULL;

	ChatRoomsModelItem* item = parent->GetChatterItem(nickname);

	if (item)
	{
		item->SetStatus(chat_status);
		item->Change();
		return item;
	}

	item = OP_NEW(ChatRoomsModelItem, (OpTypedObject::CHATTER_TYPE, account_id, nickname));
	if (!item)
		return NULL;

	item->SetStatus(chat_status);
	parent->AddChildLast(item);

	return item;
}


void ChatRoomsModel::DeleteChatter(UINT16 account_id, const OpStringC& nickname)
{
	ChatRoomsModelItem* parent = GetAccountItem(account_id);
	if (parent)
	{
		ChatRoomsModelItem* item = parent->GetChatterItem(nickname);
		if (item)
			item->Delete();
	}
}

/***********************************************************************************
**
**	GetChattersModel
**
***********************************************************************************/

ChattersModel* ChatRoomsModel::GetChattersModel(UINT16 account_id, const OpStringC& room)
{
	ChatRoomsModelItem* item = GetRoomItem(account_id, room);

	if (!item)
		return NULL;

	return item->GetChattersModel();
}

/***********************************************************************************
**
**	GetRoomItem
**
***********************************************************************************/

ChatRoomsModelItem* ChatRoomsModel::GetRoomItem(UINT16 account_id, const OpStringC& chat_room)
{
	ChatRoomsModelItem* item = GetAccountItem(account_id);

	if (!item)
		return NULL;

	return item->GetRoomItem(chat_room);
}

/***********************************************************************************
**
**	RoomStatusChanged
**
***********************************************************************************/

void ChatRoomsModel::RoomStatusChanged(UINT16 account_id, const OpStringC& room)
{
	if (room.IsEmpty())
	{
		ChatRoomsModelItem* item = GetAccountItem(account_id);

		if (!item)
			return;

		item = item->GetChildItem();

		while (item)
		{
			item->Change();
			item = item->GetSiblingItem();
		}
	}
	else
	{
		ChatRoomsModelItem* item = GetRoomItem(account_id, room);

		if (!item)
			return;

		item->Change();
	}
}

/***********************************************************************************
**
**	RoomStatusChanged
**
***********************************************************************************/

void ChatRoomsModel::DeleteAllChatters(UINT16 account_id)
{
	ChatRoomsModelItem* parent = GetAccountItem(account_id);
	if (parent != 0)
	{
		ChatRoomsModelItem* item = parent->GetChildItem();

		while (item != 0)
		{
			ChatRoomsModelItem *item_to_delete = 0;

			if (item->GetType() == OpTypedObject::CHATTER_TYPE)
				item_to_delete = item;

			item = item->GetSiblingItem();

			if (item_to_delete)
				item_to_delete->Delete();
		}
	}
}

/***********************************************************************************
**
**	OnAccountAdded
**
***********************************************************************************/

void ChatRoomsModel::OnAccountAdded(UINT16 account_id)
{
	Account* account = MessageEngine::GetInstance()->GetAccountById(account_id);

	if (!account)
		return;

	if (account->GetIncomingProtocol() <= AccountTypes::_FIRST_CHAT || AccountTypes::_LAST_CHAT <= account->GetIncomingProtocol())
		return;

	ChatRoomsModelItem* item = OP_NEW(ChatRoomsModelItem, (OpTypedObject::CHATROOM_SERVER_TYPE, account_id, account->GetAccountName()));
	if (!item)
		return;

	AddLast(item);

	// include rooms for each account

	for (UINT32 room = 0; room < account->GetSubscribedFolderCount(); room++)
	{
		OpString chat_room;

		account->GetSubscribedFolderName(room, chat_room);
		if (!chat_room.IsEmpty())
		{
			AddChatRoom(account_id, chat_room);
		}
	}
}

/***********************************************************************************
**
**	OnAccountRemoved
**
***********************************************************************************/

void ChatRoomsModel::OnAccountRemoved(UINT16 account_id, AccountTypes::AccountType account_type)
{
	ChatRoomsModelItem* item = GetAccountItem(account_id);

	if (!item)
		return;

	item->Delete();
}

/***********************************************************************************
**
**	OnChatStatusChanged
**
***********************************************************************************/

void ChatRoomsModel::OnChatStatusChanged(UINT16 account_id)
{
	// Update the visual state of the account.
	ChatRoomsModelItem* item = GetAccountItem(account_id);

	if (!item)
		return;

	item->Change();

	// If the changed account is a chat account we must delete all chatter contacts
	// from the model.
	Account* account;
	((AccountManager*)MessageEngine::GetInstance()->GetAccountManager())->GetAccountById(account_id, account);

	if (account != 0)
	{
		if ((account->GetIncomingProtocol() > AccountTypes::_FIRST_CHAT) &&
			(account->GetIncomingProtocol() < AccountTypes::_LAST_CHAT))
		{
			if (!account->GetProgress(TRUE).IsConnected())
			{
				DeleteAllChatters(account_id);
			}
		}
	}
}

/***********************************************************************************
**
**	OnChatRoomJoining
**
***********************************************************************************/

void ChatRoomsModel::OnChatRoomJoining(UINT16 account_id, const ChatInfo& room)
{
	ChatRoomsModelItem* item = AddChatRoom(account_id, room.ChatName());

	if (!item)
		return;

	item->JoinedRoom();
}

/***********************************************************************************
**
**	OnChatRoomLeft
**
***********************************************************************************/

void ChatRoomsModel::OnChatRoomLeft(UINT16 account_id, const ChatInfo& room, const OpStringC& kicker, const OpStringC& kick_reason)
{
	if (room.HasName())
	{
		ChatRoomsModelItem* item = GetRoomItem(account_id, room.ChatName());

		if (item)
		{
			item->LeftRoom();
		}
	}
	else
	{
		// if room is empty, it means all rooms.. probably disconnected
		ChatRoomsModelItem* item = GetAccountItem(account_id);

		if (!item)
			return;

		item = item->GetChildItem();

		while (item)
		{
			item->LeftRoom();
			item = item->GetSiblingItem();
		}
	}
}

/***********************************************************************************
**
**	OnChatterJoined.
**
***********************************************************************************/

void ChatRoomsModel::OnChatterJoined(UINT16 account_id,
									 const ChatInfo& room,
									 const OpStringC& chatter,
									 BOOL is_operator,
									 BOOL is_voiced,
									 const OpStringC& prefix,
									 BOOL initial)
{
	RoomStatusChanged(account_id, room.ChatName());
}

/***********************************************************************************
**
**	OnChatterLeft.
**
***********************************************************************************/

void ChatRoomsModel::OnChatterLeft(UINT16 account_id,
								   const ChatInfo& room,
								   const OpStringC& chatter,
								   const OpStringC& message,
								   const OpStringC& kicker)
{
	RoomStatusChanged(account_id, room.ChatName());
}

/***********************************************************************************
**
**	OnChatPropertyChanged.
**
***********************************************************************************/

void ChatRoomsModel::OnChatPropertyChanged(UINT16 account_id, const ChatInfo& room,
	const OpStringC& chatter, const OpStringC& changed_by, EngineTypes::ChatProperty property,
	const OpStringC& property_string, INT32 property_value)
{
	if (property == EngineTypes::ROOM_STATUS)
	{
		RoomStatusChanged(account_id, room.ChatName());
	}
	else if (property == EngineTypes::CHATTER_PRESENCE)
	{
		if (property_value != AccountTypes::OFFLINE)
		{
			AddChatter(account_id, chatter, (AccountTypes::ChatStatus) property_value);
		}
		else
		{
			DeleteChatter(account_id, chatter);
		}
	}
}

#endif // IRC_SUPPORT
