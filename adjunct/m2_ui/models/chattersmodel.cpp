
#include "core/pch.h"

#ifdef IRC_SUPPORT

#include "chattersmodel.h"
#include "adjunct/m2/src/engine/chatinfo.h"
#include "adjunct/m2/src/engine/message.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/store.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/hotlist/HotlistModelItem.h"
#include "modules/locale/oplanguagemanager.h"
//#include "modules/prefs/prefsmanager/prefstypes.h"

UINT32 ChattersModelItem::s_chatter_id_counter = 1;

/***********************************************************************************
**
**	ChattersModelItem
**
***********************************************************************************/

ChattersModelItem::ChattersModelItem(UINT16 account_id, const OpStringC& name,
	BOOL is_operator, BOOL is_voiced, const OpStringC& prefix)
:	m_chat_status(AccountTypes::ONLINE),
	m_is_operator(is_operator),
	m_is_voiced(is_voiced),
	m_account_id(account_id),
	m_chatter_id(s_chatter_id_counter++)
{
	SetName(name);
	m_prefix.Set(prefix);

	if (g_hotlist_manager)
		g_hotlist_manager->GetContactsModel()->AddModelListener(this);
}

ChattersModelItem::~ChattersModelItem()
{
	if (g_hotlist_manager)
		g_hotlist_manager->GetContactsModel()->RemoveModelListener(this);
}

OP_STATUS ChattersModelItem::GetItemData(ItemData* item_data)
{
	if (item_data->query_type != COLUMN_QUERY)
	{
		return OpStatus::OK;
	}

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

	if (IsOperator())
	{
		item_data->flags |= FLAG_BOLD;
	}
	else if (IsVoiced())
	{
		// item_data->flags |= FLAG_ITALIC;
	}
	else if (GetModel()->IsModerated() && !IsVoiced())
	{
		item_data->column_query_data.column_text_color = OP_RGB(0x80, 0x80, 0x80);
	}

	OpString user_name;
	if (m_prefix.HasContent())
		user_name.AppendFormat(UNI_L("%s"), m_prefix.CStr());

	user_name.Append(m_name);

	item_data->column_query_data.column_text->Set(user_name);
	item_data->column_query_data.column_sort_order = (IsOperator() ? 0 : (IsVoiced() ? 1 : 2));

	return OpStatus::OK;
}

void ChattersModelItem::SetName(const OpStringC& name)
{
	m_contact = g_hotlist_manager->GetContactsModel()->GetByNickname(name);
	m_name.Set(name);
}

/***********************************************************************************
**
**	ChattersModel
**
***********************************************************************************/

ChattersModel::ChattersModel()
{
}


ChattersModel::~ChattersModel()
{
	MessageEngine::GetInstance()->RemoveChatListener(this);
}


OP_STATUS ChattersModel::Init(UINT16 account_id, OpString& room)
{
	m_account_id = account_id;
	m_room.Set(room);
	m_is_moderated = FALSE;
	return MessageEngine::GetInstance()->AddChatListener(this);
}

INT32 ChattersModel::GetColumnCount()
{
	return 1;
}

OP_STATUS ChattersModel::GetColumnData(ColumnData* column_data)
{
	RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_COL_IRCNICK, column_data->text));
	return OpStatus::OK;
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
OP_STATUS ChattersModel::GetTypeString(OpString& type_string)
{
	return g_languageManager->GetString(Str::S_USERS_TYPE, type_string);
}
#endif

/***********************************************************************************
**
**	IsModelForRoom
**
***********************************************************************************/

BOOL ChattersModel::IsModelForRoom(UINT16 account_id, const OpStringC& room)
{
	return account_id == m_account_id && (room.IsEmpty() || m_room.CompareI(room) == 0);
}

/***********************************************************************************
**
**	GetChatter
**
***********************************************************************************/

ChattersModelItem* ChattersModel::GetChatter(const OpStringC& chatter)
{
	if (chatter.IsEmpty())
		return NULL;

	ChattersModelItem* item = NULL;

	m_chatters_hash_table.GetData(chatter.CStr(), &item);

	return item;
}

/***********************************************************************************
**
**	OnChatterJoining
**
***********************************************************************************/

BOOL ChattersModel::OnChatterJoining(UINT16 account_id, const ChatInfo& room,
	const OpStringC& chatter, BOOL is_operator, BOOL is_voiced, const OpStringC& prefix, BOOL initial)
{
	if (!IsModelForRoom(account_id, room.ChatName()))
	{
		return TRUE;
	}

	if (GetChatter(chatter))
		return FALSE;

	ChattersModelItem* item = OP_NEW(ChattersModelItem, (m_account_id, chatter, is_operator, is_voiced, prefix));
	if (!item)
		return FALSE;

	m_chatters_hash_table.Add(item->GetName(), item);
	AddLast(item);

	return TRUE;
}

/***********************************************************************************
**
**	OnChatterLeft
**
***********************************************************************************/

void ChattersModel::OnChatterLeft(UINT16 account_id, const ChatInfo& room,
	const OpStringC& chatter, const OpStringC& message,
	const OpStringC& kicker)
{
	if (!IsModelForRoom(account_id, room.ChatName()))
		return;

	ChattersModelItem* item = 0;
	m_chatters_hash_table.Remove(chatter.CStr(), &item);

	if (item != 0)
		item->Delete();
}

/***********************************************************************************
**
**	OnChatPropertyChanged
**
***********************************************************************************/

void ChattersModel::OnChatPropertyChanged(UINT16 account_id, const ChatInfo& room,
	const OpStringC& chatter, const OpStringC& changed_by,
	EngineTypes::ChatProperty property, const OpStringC& property_string,
	INT32 property_value)
{
	if (!IsModelForRoom(account_id, room.ChatName()))
	{
		return;
	}

	if (property == EngineTypes::ROOM_MODERATED)
	{
		m_is_moderated = property_value;
		ChangeAll();
		return;
	}

	ChattersModelItem* item = GetChatter(chatter);

	if (item)
	{
		switch (property)
		{
			case EngineTypes::CHATTER_NICK:
			{
				m_chatters_hash_table.Remove(item->GetName(), &item);
				item->SetName(property_string);
				m_chatters_hash_table.Add(item->GetName(), item);
				break;
			}

			case EngineTypes::CHATTER_OPERATOR:
			{
				item->SetOperator(property_value);
				break;
			}

			case EngineTypes::CHATTER_VOICED:
			{
				item->SetVoiced(property_value);
				break;
			}

			case EngineTypes::CHATTER_PRESENCE:
			{
				item->SetStatus((AccountTypes::ChatStatus) property_value);
				break;
			}
			case EngineTypes::CHATTER_UNKNOWN_MODE_OPERATOR:
			{
				item->SetOperator(property_value == 1);
			}
			case EngineTypes::CHATTER_UNKNOWN_MODE_VOICED:
			{
				item->SetVoiced(property_value == 1);
			}
		}

		item->Change(TRUE);
	}
}

#endif // IRC_SUPPORT
