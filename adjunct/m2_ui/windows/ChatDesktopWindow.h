/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef CHAT_DESKTOP_WINDOW
#define CHAT_DESKTOP_WINDOW

#if defined(M2_SUPPORT) && defined(IRC_SUPPORT)

#include "adjunct/m2/src/engine/listeners.h"
#include "adjunct/m2/src/engine/chatinfo.h"
#include "adjunct/quick/hotlist/HotlistModel.h"
#include "adjunct/quick/managers/SleepManager.h"
#include "adjunct/quick/windows/DesktopTab.h"
#include "modules/widgets/OpWidgetFactory.h"
#include "modules/url/url_man.h"

class ChatHeaderPane;
class HotlistModelItem;

/***********************************************************************************
**
**	InputHistory
**
***********************************************************************************/

class InputHistory
{
public:
	// Constructor.
	InputHistory();

	// Methods.
	OP_STATUS AddInput(const OpStringC &input);
	OP_STATUS PreviousInput(const OpStringC& currently_editing, OpString &out);
	OP_STATUS NextInput(const OpStringC& currently_editing, OpString &out);

private:
	// No copy or assignment.
	InputHistory(InputHistory const &other);
	InputHistory &operator=(InputHistory const &rhs);

	// Methods.
	BOOL ShouldStoreEditLine(const OpStringC& input) const;
	void DecrementIndex();
	void IncrementIndex();

	// Enum values.
	enum { IndexBottom = -2, IndexEditLine = -1, IndexTop = INT_MAX };

	// Members.
	OpAutoVector<OpString> m_history;
	OpString m_edit_line;
	INT32 m_cur_index;
};


/***********************************************************************************
**
**	NickCompletion
**
***********************************************************************************/

class NickCompletion
{
public:
	// Constructor.
	NickCompletion();

	// Methods.
	OP_STATUS CompleteNick(const OpStringC& input_line, INT32& input_position, OpString& output_line);
	void SetTreeView(OpTreeView *tree_view) { m_tree_view = tree_view; }

private:
	// Methods.
	void GetChatterNick(INT32 index, OpString &nick, OpTreeView* tree_view) const;
	INT32 NormalizeIndex(INT32 index, INT32 chatter_count) const;

	// Members.
	OpTreeView *m_tree_view;

	OpString m_current_match_word;
	OpString m_previous_nick_completed;
	INT32 m_current_match_index;

	OpString m_completion_postfix;
};


/***********************************************************************************
**
**	ChatDesktopWindow
**
***********************************************************************************/

class ChatDesktopWindow
:	public DesktopTab,
	public OpTreeModel::Listener,
	public ChatListener,
	public SleepListener,
	public HotlistModelListener
{
	public:
								ChatDesktopWindow(OpWorkspace* workspace = NULL);
		virtual					~ChatDesktopWindow();

		OP_STATUS				Init(INT32 account_id, const OpStringC& name, BOOL is_room, BOOL is_joining = FALSE);

		void					GoToChat();
		void					GoToChatRoom();
		BOOL					IsCorrectChatRoom(UINT32 account_id, const OpStringC& name, BOOL is_room);
		void					LeaveChat();

		void					EditRoomProperties();
		void					Flush();
		BOOL					IsSendingAllowed();
		BOOL					IsInsideRoom();
		BOOL					IsRoom() {return m_is_room;}
		BOOL					IsOperator() {return m_is_operator;}
		BOOL					IsModerated() {return m_is_room_moderated;}
		BOOL					IsSecret() {return m_is_room_secret;}
		BOOL					IsTopicProtected() {return m_is_topic_protected;}
		BOOL					HasPassword() {return m_password.HasContent();}
		BOOL					HasLimit() {return m_limit.HasContent();}

		const uni_char*			GetName() {return m_name.CStr(); }
		const uni_char*			GetTopic() {return m_topic.CStr();}
		const uni_char*			GetPassword() {return m_password.CStr();}
		const uni_char*			GetLimit() {return m_limit.CStr();}

		const ChatInfo&			GetChatInfo() const { return m_chat_info; }

		INT32					GetAccountID() {return m_account_id;}

		/** Find the window that represents a particular chat room/nick
		  * @param account_id Account that should be connected to room/nick
		  * @param name Name of room/nick
		  * @param is_room Whether it is a room or a nick
		  * @return A window with the specified properties is found or NULL otherwise
		  */
		static ChatDesktopWindow* FindChatRoom(UINT32 account_id, const OpStringC& name, BOOL is_room);

		// Subclassing DesktopWindow
		virtual const char*			GetWindowName() {return m_is_room ? "Chat Room Window" : "Chat Window";}
		virtual OpWindowCommander*	GetWindowCommander() {return m_chat_view ? m_chat_view->GetWindowCommander() : NULL; }
		virtual OpBrowserView*		GetBrowserView() {return m_chat_view;}

		virtual void			OnClose(BOOL user_initiated);
		virtual void			OnActivate(BOOL activate, BOOL first_time);
		virtual void			OnLayout();
		virtual void			OnSessionReadL(const OpSessionWindow* session_window);
		virtual void			OnSessionWriteL(OpSession* session, OpSessionWindow* session_window, BOOL shutdown);

		// WidgetListener
		virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);
		virtual void			OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);

		// == OpTreeModel::Listener ======================
		void					OnItemAdded(OpTreeModel* tree_model, INT32 pos) { OnTreeChanged(tree_model); }
		void					OnItemChanged(OpTreeModel* tree_model, INT32 pos, BOOL sort) { OnTreeChanged(tree_model); }
		void					OnItemRemoving(OpTreeModel* tree_model, INT32 pos) {}
		void					OnSubtreeRemoving(OpTreeModel* tree_model, INT32 parent, INT32 pos, INT32 subtree_size) {}
		void					OnSubtreeRemoved(OpTreeModel* tree_model, INT32 parent, INT32 pos, INT32 subtree_size) { OnTreeChanged(tree_model); }
		void					OnTreeChanging(OpTreeModel* tree_model) {}
		void					OnTreeChanged(OpTreeModel* tree_model);
		void					OnTreeDeleted(OpTreeModel* tree_model) {}

		// ChatListener
		void					OnChatStatusChanged(UINT16 account_id) {}
		void					OnChatMessageReceived(UINT16 account_id, EngineTypes::ChatMessageType type, const OpStringC& message, const ChatInfo& chat_info, const OpStringC& chatter, BOOL is_private_chat);
		void					OnChatServerInformation(UINT16 account_id, const OpStringC& server, const OpStringC& information) {}
		void					OnChatRoomJoining(UINT16 account_id, const ChatInfo& room) {}
		void					OnChatRoomJoined(UINT16 account_id, const ChatInfo& room);
		void					OnChatRoomLeaving(UINT16 account_id, const ChatInfo& room, const OpStringC& kicker, const OpStringC& kick_reason);
		void					OnChatRoomLeft(UINT16 account_id, const ChatInfo& room, const OpStringC& kicker, const OpStringC& kick_reason);
		BOOL					OnChatterJoining(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, BOOL is_operator, BOOL is_voiced, const OpStringC& prefix, BOOL initial) {return TRUE;}
		void					OnChatterJoined(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, BOOL is_operator, BOOL is_voiced, const OpStringC& prefix, BOOL initial);
		void					OnChatterLeaving(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, const OpStringC& message, const OpStringC& kicker);
		void					OnChatterLeft(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, const OpStringC& message, const OpStringC& kicker) { }
		void					OnChatPropertyChanged(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, const OpStringC& changed_by, EngineTypes::ChatProperty property, const OpStringC& property_string, INT32 property_value);
		void					OnWhoisReply(UINT16 account_id, const OpStringC& nick, const OpStringC& user_id, const OpStringC& host, const OpStringC& real_name, const OpStringC& server, const OpStringC& server_info, const OpStringC& away_message, const OpStringC& logged_in_as, BOOL is_irc_operator, int seconds_idle, int signed_on, const OpStringC& channels);
		void					OnInvite(UINT16 account_id, const OpStringC& nick, const ChatInfo& room);
		void					OnFileReceiveRequest(UINT16 account_id, const OpStringC& sender, const OpStringC& filename, UINT32 file_size, UINT port_number, UINT id);
		void					OnUnattendedChatCountChanged(OpWindow* op_window, UINT32 unread) {}
		void					OnChatReconnecting(UINT16 account_id, const ChatInfo& room);

		// == OpInputContext ======================
		virtual BOOL			OnInputAction(OpInputAction* action);
		virtual const char*		GetInputContextName() {return "Chat Window";}

		// OpTypedObject
		virtual Type			GetType() {return m_is_room ? WINDOW_TYPE_CHAT_ROOM : WINDOW_TYPE_CHAT;}

		// SleepListener
		virtual void			OnSleep() { m_reconnect = IsInsideRoom(); }
		virtual void			OnWakeUp() { if (m_reconnect) GoToChatRoom(); }

		// DesktopFileChooserListener
		virtual void			OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result);

		// HotlistModelListener
		virtual void OnHotlistItemAdded(HotlistModelItem* item) { if (item->HasNickname(m_name)) m_contact = item; }
		virtual void OnHotlistItemChanged(HotlistModelItem* item, BOOL moved_as_child, UINT32 changed_flag = HotlistModel::FLAG_UNKNOWN) { if (item->HasNickname(m_name)) m_contact = item; }
		virtual void OnHotlistItemRemoved(HotlistModelItem* item, BOOL removed_as_child) { if (item == m_contact) m_contact = 0; }
		virtual void OnHotlistItemTrashed(HotlistModelItem* item) {}
		virtual void OnHotlistItemUnTrashed(HotlistModelItem* item) {}
		virtual void OnHotlistItemMovedFrom(HotlistModelItem* item) {}

		void					SetAttention(BOOL attention);
		void 					UpdateTaskbar();

	private:
		// Methods.
		void					SendFiles(const OpStringC& nick);
		BOOL					ChatCommand(EngineTypes::ChatMessageType command, BOOL only_check_if_available = FALSE);
		void					ConvertToHTML(OpString& html, const uni_char* text, INT32 len = 0);
		void					GetNick(OpString& nick);
		BOOL					Send();
		BOOL					CompleteNick();
		BOOL					CycleMessage(BOOL cycle_up);
		INT32					GetChatterCount(OpTreeView* view_to_use = NULL);
		void					GetChatterNick(INT32 index, OpString& nick, OpTreeView* view_to_use = NULL);
		BOOL					ContainsChatter(OpString& chatter, OpTreeView* view_to_use = NULL);
		void					GetSelectedChatterNick(OpString& nick);
		HotlistModelItem*		GetSelectedChatterContact();
		void					WriteLine(const uni_char* left_text, const uni_char* right_text, const char* class_name, BOOL highlight = TRUE, BOOL convert_to_html = TRUE);
		void					WriteStartTimeInfo();
		void					UpdateTitleAndInfo();
		void					InitURL(Window* window = 0, BOOL init_with_data = TRUE);
		void					UpdateChatInfoIfNeeded(const ChatInfo& chat_info);

		OP_STATUS				MakeValidHTMLId(const OpStringC& id, OpString& valid_id) const;
		OP_STATUS				InsertHighlightMarkup(OpString& formatted_message, const OpStringC& nick, BOOL highlight_whole_message, BOOL& contains_highlighted_message) const;
		BOOL					ShouldHighlightWholeMessage(const OpStringC& message, const OpStringC& nick) const;
		OP_STATUS				SetName(const OpStringC& name);

		OpString				m_name;
		HotlistModelItem*		m_contact;
		ChatInfo				m_chat_info;

		OpString				m_topic;
		OpString				m_password;
		OpString				m_limit;
		OpString				m_time_created;
		OpString				m_last_time_stamp;

		OpString				m_command_character;

		InputHistory			m_input_history;
		NickCompletion			m_nick_completor;

		UINT32					m_account_id;
		BOOL					m_is_room;
		BOOL					m_is_room_moderated;
		BOOL					m_is_topic_protected;
		BOOL					m_is_room_secret;
		BOOL					m_is_operator;
		BOOL					m_is_voiced;
		ChatHeaderPane*				m_header_pane;
		OpSplitter*				m_splitter;
		OpMultilineEdit*		m_text_edit;
		OpButton*				m_send_button;
		OpTreeView*				m_persons_view;
		OpBrowserView*			m_chat_view;

		URL						m_url;
		URL_InUse				m_url_in_use;

		BOOL                    m_last_was_double_down;
		BOOL					m_reconnect;

		OpString				m_send_files_nick;

		UINT32                  m_personal_attention_count;

		static OpVector<ChatDesktopWindow>	s_attention_windows;
};

#endif //M2_SUPPORT && IRC_SUPPORT

#endif // CHAT_DESKTOP_WINDOW
