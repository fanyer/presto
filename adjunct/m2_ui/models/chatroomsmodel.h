
#ifndef CHATROOMSMODEL_H
#define CHATROOMSMODEL_H

#ifdef IRC_SUPPORT

#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/listeners.h"
#include "adjunct/m2/src/engine/index.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/quick/hotlist/HotlistModel.h"

// ---------------------------------------------------------------------------------

class Chatter;
class HotlistModelItem;

class ChatRoom
{
	public:
    virtual ~ChatRoom() {}

	virtual OP_STATUS GetName(OpString& name) = 0;
	virtual UINT32 GetAccountID() = 0;
	virtual Chatter* GetChatter(const OpStringC& chatter) = 0;
};

class ChatRoomsModelItem : public TreeModelItem<ChatRoomsModelItem, ChatRoomsModel>, public ChatRoom, public HotlistModelListener
{
	public:

									ChatRoomsModelItem(Type type, INT32 account_id, const OpStringC& name);
		virtual						~ChatRoomsModelItem();

		virtual OP_STATUS			GetItemData(ItemData* item_data);
		virtual Type				GetType() { return m_type; }
		virtual int					GetID();

		virtual OP_STATUS			GetName(OpString& name);
		const OpStringC&			GetName() const { return m_name; }

		virtual UINT32				GetAccountID() {return m_account_id;}
		Account*					GetAccount();

		AccountTypes::ChatStatus	GetStatus() { return m_chat_status; }
		void						SetStatus(AccountTypes::ChatStatus chat_status) { m_chat_status = chat_status; }

		BOOL						IsServer() {return m_type == CHATROOM_SERVER_TYPE;}
		BOOL						IsRoom(const OpStringC& name) { return m_type == CHATROOM_TYPE && (name.IsEmpty() || m_name.CompareI(name) == 0);}

		ChattersModel*				GetChattersModel() {return m_chatters_model;}

		void						JoinedRoom();
		void						LeftRoom();

		// if this item is an account, use this to find room item
		ChatRoomsModelItem*			GetRoomItem(const OpStringC& room);
		ChatRoomsModelItem*			GetChatterItem(const OpStringC& nickname);

		virtual Chatter*		GetChatter(const OpStringC& chatter);

		// HotlistModelListener
		virtual void OnHotlistItemAdded(HotlistModelItem* item) { if (item->HasNickname(m_name)) m_contact = item; }
		virtual void OnHotlistItemChanged(HotlistModelItem* item, BOOL moved_as_child, UINT32 changed_flag = HotlistModel::FLAG_UNKNOWN) { if (item->HasNickname(m_name)) m_contact = item; }
		virtual void OnHotlistItemRemoved(HotlistModelItem* item, BOOL removed_as_child) { if (item == m_contact) m_contact = 0; }
		virtual void OnHotlistItemTrashed(HotlistModelItem* item) {}
		virtual void OnHotlistItemUnTrashed(HotlistModelItem* item) {}
		virtual void OnHotlistItemMovedFrom(HotlistModelItem* item) {}

	private:
		OP_STATUS SetName(const OpStringC& name);

		OpString					m_name;
		Type						m_type;
		UINT32						m_account_id;
		UINT32						m_room_chatter_id;
		ChattersModel*				m_chatters_model;
		AccountTypes::ChatStatus	m_chat_status;
		HotlistModelItem*			m_contact;

		static UINT32				s_room_id_counter;
};

// ---------------------------------------------------------------------------------

class ChatRoomsModel : public TreeModel<ChatRoomsModelItem>, public ChatListener, public AccountListener
{
	public:
		ChatRoomsModel();
		~ChatRoomsModel();

		virtual OP_STATUS			Init();

		virtual INT32				GetColumnCount();
		virtual OP_STATUS			GetColumnData(ColumnData* column_data);
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		virtual OP_STATUS			GetTypeString(OpString& type_string);
#endif

		ChatRoomsModelItem*			AddChatRoom(UINT16 account_id, const OpStringC& chat_room);
		void						DeleteChatRoom(UINT32 room_id);

		ChatRoomsModelItem*			AddChatter(UINT16 account_id, const OpStringC& nickname, AccountTypes::ChatStatus chat_status);
		void						DeleteChatter(UINT16 account_id, const OpStringC& nickname);

		ChatRoomsModelItem*			GetRoomItem(UINT16 account_id, const OpStringC& chat_room);
		ChatRoom*				GetChatRoom(UINT32 room_id) { return GetItemByIDAndType(room_id, OpTypedObject::CHATROOM_TYPE); }
		ChatRoom*				GetChatter(UINT32 chatter_id) { return GetItemByIDAndType(chatter_id, OpTypedObject::CHATTER_TYPE); }

		// get ChattersTreeModel for a given room on an account

		ChattersModel*				GetChattersModel(UINT16 account_id, const OpStringC& room);

	protected:
		// implmenting MessageEngine::AccountListener
		virtual void				OnAccountAdded(UINT16 account_id);
		virtual void				OnAccountRemoved(UINT16 account_id, AccountTypes::AccountType account_type);
		virtual void				OnFolderAdded(UINT16 account_id, const OpStringC& name, const OpStringC& path, BOOL subscribedLocally, INT32 subscribedOnServer, BOOL editable) {}
		virtual void				OnFolderRemoved(UINT16 account_id, const OpStringC& path) {}
		virtual void				OnFolderRenamed(UINT16 account_id, const OpStringC& old_path, const OpStringC& new_path) {}
		virtual void				OnFolderLoadingCompleted(UINT16 account_id) { }

		// implmenting MessageEngine::ChatListener
		virtual void				OnChatStatusChanged(UINT16 account_id);
		virtual void				OnChatMessageReceived(UINT16 account_id, EngineTypes::ChatMessageType type, const OpStringC& message, const ChatInfo& chat_info, const OpStringC& chatter, BOOL is_private_chat) {}
		virtual void				OnChatServerInformation(UINT16 account_id, const OpStringC& server, const OpStringC& information) {}
		virtual void				OnChatRoomJoining(UINT16 account_id, const ChatInfo& room);
		virtual void				OnChatRoomJoined(UINT16 account_id, const ChatInfo& room) {}
		virtual void				OnChatRoomLeaving(UINT16 account_id, const ChatInfo& room, const OpStringC& kicker, const OpStringC& kick_reason) {}
		virtual void				OnChatRoomLeft(UINT16 account_id, const ChatInfo& room, const OpStringC& kicker, const OpStringC& kick_reason);
		virtual BOOL				OnChatterJoining(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, BOOL is_operator, BOOL is_voiced, const OpStringC& prefix, BOOL initial) {return TRUE;}
		virtual void				OnChatterJoined(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, BOOL is_operator, BOOL is_voiced, const OpStringC& prefix, BOOL initial);
		virtual void				OnChatterLeaving(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, const OpStringC& message, const OpStringC& kicker) {}
		virtual void				OnChatterLeft(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, const OpStringC& message, const OpStringC& kicker);
		virtual void				OnChatPropertyChanged(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, const OpStringC& changed_by, EngineTypes::ChatProperty property, const OpStringC& property_string, INT32 property_value);
		virtual void				OnWhoisReply(UINT16 account_id, const OpStringC& nick, const OpStringC& user_id, const OpStringC& host, const OpStringC& real_name, const OpStringC& server, const OpStringC& server_info, const OpStringC& away_message, const OpStringC& logged_in_as, BOOL is_irc_operator, int seconds_idle, int signed_on, const OpStringC& channels) {}
		virtual void				OnInvite(UINT16 account_id, const OpStringC& nick, const ChatInfo& room) {}
		virtual void				OnFileReceiveRequest(UINT16 account_id, const OpStringC& sender, const OpStringC& filename, UINT32 file_size, UINT port_number, UINT id) {}
		virtual void				OnUnattendedChatCountChanged(OpWindow* op_window, UINT32 unread) {}
		virtual void				OnChatReconnecting(UINT16 account_id, const ChatInfo& room) {};

	private:

		ChatRoomsModelItem*			GetAccountItem(UINT16 account_id) {return GetItemByIDAndType(account_id, OpTypedObject::CHATROOM_SERVER_TYPE);}
		void						DeleteAllChatters(UINT16 account_id);
		void						RoomStatusChanged(UINT16 account_id, const OpStringC& room);
};

#endif // IRC_SUPPORT

#endif
