
#ifndef CHATTERSMODEL_H
#define CHATTERSMODEL_H

#ifdef IRC_SUPPORT

#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/index.h"
#include "adjunct/m2/src/engine/listeners.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/quick/hotlist/HotlistModel.h"

class HotlistModelItem;

// ---------------------------------------------------------------------------------

class Chatter
{
	public:
    virtual ~Chatter() {}

	virtual OP_STATUS GetName(OpString& name) = 0;
	virtual UINT32 GetAccountID() = 0;
};

class ChattersModelItem : public TreeModelItem<ChattersModelItem, ChattersModel>, public Chatter, public HotlistModelListener
{
	public:

							ChattersModelItem(UINT16 account_id, const OpStringC& name, BOOL is_operator, BOOL is_voiced, const OpStringC& prefix);
		virtual				~ChattersModelItem();

		virtual OP_STATUS	GetItemData(ItemData* item_data);
		virtual Type		GetType() {return CHATTER_TYPE;}
		virtual int			GetID()	{return m_chatter_id;}

		BOOL				IsChatter(OpString& chatter) {return m_name.CompareI(chatter) == 0;}

		BOOL				IsOperator() const { return m_is_operator; }
		void				SetOperator(BOOL is_operator) { m_is_operator = is_operator; }

		BOOL				IsVoiced() const { return m_is_voiced; }
		void				SetVoiced(BOOL is_voiced) { m_is_voiced = is_voiced; }

		const OpStringC&	Prefix() const { return m_prefix; }
		void				SetPrefix(const OpStringC& prefix) { m_prefix.Set(prefix); }

		const uni_char*		GetName() {return m_name.CStr();}
		void				SetName(const OpStringC& name);

		AccountTypes::ChatStatus	GetStatus() {return m_chat_status;}
		void				SetStatus(AccountTypes::ChatStatus chat_status) {m_chat_status = chat_status;}

		HotlistModelItem*   GetContact() { return m_contact; }

		// Chatter

		virtual OP_STATUS GetName(OpString& name) {return name.Set(m_name.CStr());}
		virtual UINT32 GetAccountID() {return m_account_id;}

		// HotlistModelListener
		virtual void OnHotlistItemAdded(HotlistModelItem* item) { if (item->HasNickname(m_name)) m_contact = item; }
		virtual void OnHotlistItemChanged(HotlistModelItem* item, BOOL moved_as_child, UINT32 changed_flag = HotlistModel::FLAG_UNKNOWN) { if (item->HasNickname(m_name)) m_contact = item; }
		virtual void OnHotlistItemRemoved(HotlistModelItem* item, BOOL removed_as_child) { if (item == m_contact) m_contact = 0; }
		virtual void OnHotlistItemTrashed(HotlistModelItem* item) {}
		virtual void OnHotlistItemUnTrashed(HotlistModelItem* item) {}
		virtual void OnHotlistItemMovedFrom(HotlistModelItem* item) {}

	private:

		AccountTypes::ChatStatus	m_chat_status;
		OpString				m_name;
		BOOL					m_is_operator;
		BOOL					m_is_voiced;
		OpString				m_prefix;
		UINT16					m_account_id;
		INT32					m_chatter_id;
		HotlistModelItem*		m_contact;

		static UINT32			s_chatter_id_counter;
};

// ---------------------------------------------------------------------------------

class ChattersModel : public TreeModel<ChattersModelItem>, public ChatListener
{
	public:
		ChattersModel();
		~ChattersModel();

		virtual OP_STATUS			Init(UINT16 account_id, OpString& room);

		virtual INT32				GetColumnCount();
		virtual OP_STATUS			GetColumnData(ColumnData* column_data);
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		virtual OP_STATUS			GetTypeString(OpString& type_string);
#endif

		// return TRUE if this model is for a given room

		ChattersModelItem*			GetChatter(const OpStringC& chatter);
		UINT16					GetAccountID() {return m_account_id;}
		BOOL						IsModelForRoom(UINT16 account_id, const OpStringC& room);
		BOOL						IsModerated() {return m_is_moderated;}

		// implmenting EngineTypes::ChatListener
		virtual void				OnChatStatusChanged(UINT16 account_id) {}
		virtual void				OnChatMessageReceived(UINT16 account_id, EngineTypes::ChatMessageType type, const OpStringC& message, const ChatInfo& chat_info, const OpStringC& chatter, BOOL is_private_chat) {}
		virtual void				OnChatServerInformation(UINT16 account_id, const OpStringC& server, const OpStringC& information) {}
		virtual void				OnChatRoomJoining(UINT16 account_id, const ChatInfo& room) {}
		virtual void				OnChatRoomJoined(UINT16 account_id, const ChatInfo& room) {}
		virtual void				OnChatRoomLeaving(UINT16 account_id, const ChatInfo& room, const OpStringC& kicker, const OpStringC& kick_reason) {}
		virtual void				OnChatRoomLeft(UINT16 account_id, const ChatInfo& room, const OpStringC& kicker, const OpStringC& kick_reason) {}
		virtual BOOL				OnChatterJoining(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, BOOL is_operator, BOOL is_voiced, const OpStringC& prefix, BOOL initial);
		virtual void				OnChatterJoined(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, BOOL is_operator, BOOL is_voiced, const OpStringC& prefix, BOOL initial) {}
		virtual void				OnChatterLeaving(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, const OpStringC& message, const OpStringC& kicker) { }
		virtual void				OnChatterLeft(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, const OpStringC& message, const OpStringC& kicker);
		virtual void				OnChatPropertyChanged(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, const OpStringC& changed_by, EngineTypes::ChatProperty property, const OpStringC& property_string, INT32 property_value);
		virtual void				OnWhoisReply(UINT16 account_id, const OpStringC& nick, const OpStringC& user_id, const OpStringC& host, const OpStringC& real_name, const OpStringC& server, const OpStringC& server_info, const OpStringC& away_message, const OpStringC& logged_in_as, BOOL is_irc_operator, int seconds_idle, int signed_on, const OpStringC& channels) {}
		virtual void				OnInvite(UINT16 account_id, const OpStringC& nick, const ChatInfo& room) {}
		virtual void				OnFileReceiveRequest(UINT16 account_id, const OpStringC& sender, const OpStringC& filename, UINT32 file_size, UINT port_number, UINT id) {}
		virtual void				OnUnattendedChatCountChanged(OpWindow* op_window, UINT32 unread) {}
		virtual void				OnChatReconnecting(UINT16 account_id, const ChatInfo& room) {};

	private:

		OpStringHashTable<ChattersModelItem>	m_chatters_hash_table;
		OpString								m_room;
		UINT16								m_account_id;
		BOOL									m_is_moderated;
};

#endif // IRC_SUPPORT

#endif
