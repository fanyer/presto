/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef MAIL_DESKTOP_WINDOW_H
# define MAIL_DESKTOP_WINDOW_H

# if defined M2_SUPPORT

#  include "adjunct/m2/src/engine/headerdisplay.h"
#  include "adjunct/m2/src/engine/listeners.h"
#  include "adjunct/m2/src/engine/message.h"

#  include "adjunct/m2_ui/widgets/MailSearchField.h"
#  include "adjunct/m2_ui/widgets/MessageHeaderGroup.h"

#  include "adjunct/quick_toolkit/widgets/OpGroup.h"
#  include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#  include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#  include "adjunct/quick/Application.h"
#  include "adjunct/quick/widgets/DesktopFileChooserEdit.h"
#  include "adjunct/quick/windows/DesktopTab.h"
#  include "adjunct/quick/widgets/OpInfobar.h"

#  include "modules/hardcore/timer/optimer.h"
#  include "modules/widgets/OpWidgetFactory.h"
#  include "modules/widgets/OpMultiEdit.h"


class ChatDesktopWindow;
class Hotlist;
class OpWebImageWidget;
class OpInfoBar;

/***********************************************************************************
**
**	MailDesktopWindow
**
***********************************************************************************/

class MailDesktopWindow :		public DesktopTab,
								public OpTimerListener,
								public OpTreeModel::Listener,
								public MessageListener,
								public EngineListener,
								public Message::RFC822ToUrlListener,
								public OpPrefsListener,
								public OpTreeViewListener,
								public OpMailClientListener
{
public:
		static OP_STATUS		Construct(MailDesktopWindow** obj, OpWorkspace* parent_workspace);
		virtual					~MailDesktopWindow();

		void					Init(index_gid_t index_id, const uni_char* address, const uni_char* window_title = NULL, BOOL message_only_view = FALSE);

		BOOL					IsCorrectMailView(index_gid_t index_id, const uni_char* address);

		void					SetMessageOnlyView();
		void					SetListOnlyView();
		void					SetListAndMessageView(bool horizontal_splitter);
		BOOL					IsMessageOnlyView() const { return !GetIndexAndLoadingImageGroup()->IsVisible(); }
		BOOL					IsListOnlyView()	const { return !m_mail_and_headers_group->IsVisible(); }

		void					SetOldSelectedPanel(INT32 old_panel) { m_old_selected_panel = old_panel; }

		void					SetMailViewToIndex(index_gid_t index_id, const uni_char* address, const uni_char* window_title, BOOL changed_by_mouse, BOOL force = FALSE, BOOL focus = FALSE, BOOL message_only_view = FALSE);

		/** Try to select a message if it's in this view
		  * @param message_id Message to select
		  */
		void					GoToMessage(message_gid_t message_id);

		/**
		  * Go to the mail view containing the thread messages
		  */
		void					GotoThread(message_gid_t message_id);

		void					SaveSortOrder(INT32 sort_by_column, BOOL sort_ascending, message_gid_t selected_item, index_gid_t index_id);
		void					ShowSelectedMail(BOOL delay, BOOL force = FALSE, BOOL timeout_request=FALSE);
		void					ShowRawMail(const Message* message);
		OP_STATUS				GetDecodedRawMessage(const Message* message, OpString& decoded_message);
		void					SaveMessageAs(message_gid_t message_id);
		message_gid_t			GetCurrentMessageID() { return m_mailview_message->GetId(); }
		UINT16					GetCurrentMessageAccountID() { return m_mailview_message->GetAccountId(); }
		Message*				GetCurrentMessage() const { return m_mailview_message; }
		OpBrowserView*			GetMailView() {return m_mail_view;}
		OpTreeView*				GetIndexView() { return m_index_view; }
		BOOL 					ShowContextMenu(const OpPoint &point, BOOL center, BOOL use_keyboard_context);
		void					RemoveMimeListeners();

		// Subclassing DesktopWindow

		virtual const char*			GetWindowName() {return "Mail Window";}
		virtual OpWindowCommander*	GetWindowCommander() {return m_mail_view ? m_mail_view->GetWindowCommander() : NULL;}
		virtual OpBrowserView*		GetBrowserView() {return m_mail_view;}
		virtual void				OnActivate(BOOL activate, BOOL first_time);

		// hooks subclassed from DesktopWindow

		virtual void			OnSessionReadL(const OpSessionWindow* session_window);
		virtual void			OnSessionWriteL(OpSession* session, OpSessionWindow* session_window, BOOL shutdown);
		virtual void			OnShow(BOOL show);
		virtual void			OnClose(BOOL user_initiated);

		// OpTypedObject

		virtual Type			GetType() {return WINDOW_TYPE_MAIL_VIEW;}

		// OpTimerListener

		virtual void			OnTimeOut(OpTimer* timer);

		// OpWidgetListener

		virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);
		virtual void			OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
		virtual void			OnLayout(OpWidget *widget);
		virtual void			OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
		virtual void			OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		virtual void			OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		virtual void			OnSortingChanged(OpWidget *widget);

		// OpMailClientListener
		virtual	void			GetExternalEmbedPermission(OpWindowCommander* commander, BOOL& has_permission);
		virtual void			OnExternalEmbendBlocked(OpWindowCommander* commander);

		// OpTreeViewListener
		virtual void OnTreeViewDeleted(OpTreeView* treeview) {}
		virtual void OnTreeViewMatchChanged(OpTreeView* treeview);
		virtual void OnTreeViewModelChanged(OpTreeView* treeview) {}
		virtual void OnTreeViewOpenStateChanged(OpTreeView* treeview, INT32 id, BOOL open);

		// MessageObject
		virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);


		// == OpInputContext ======================

		virtual BOOL			OnInputAction(OpInputAction* action);
		virtual const char*		GetInputContextName() {return "Mail Window";}

		// == OpTreeModel::Listener ======================

		void					OnItemAdded(OpTreeModel* tree_model, INT32 pos) {m_index_view->Redraw();}
		void					OnItemChanged(OpTreeModel* tree_model, INT32 pos, BOOL sort) {m_index_view->Redraw();}
		void					OnItemRemoving(OpTreeModel* tree_model, INT32 pos) {}
		void					OnSubtreeRemoving(OpTreeModel* tree_model, INT32 parent, INT32 pos, INT32 subtree_size) {}
		void					OnSubtreeRemoved(OpTreeModel* tree_model, INT32 parent, INT32 pos, INT32 subtree_size) {m_index_view->Redraw();}
		void					OnTreeChanging(OpTreeModel* tree_model) {}
		void					OnTreeChanged(OpTreeModel* tree_model) {m_index_view->Redraw();}
		void					OnTreeDeleted(OpTreeModel* tree_model) {}

		// == MessageEngine::MessageListener ======================

        void					OnMessageBodyChanged(message_gid_t message_id);
		void					OnMessageChanged(message_gid_t message_id);

		// == MessageEngine::EngineListener ======================

		void					OnImporterProgressChanged(const Importer* importer, const OpStringC& infoMsg, OpFileLength current, OpFileLength total, BOOL simple = FALSE) {}
		void					OnImporterFinished(const Importer* importer, const OpStringC& infoMsg) {}
		void					OnIndexChanged(UINT32 index_id);
		void					OnActiveAccountChanged() {}
		void					OnReindexingProgressChanged(INT32 progress, INT32 total) {}

		// == Message::RFC822ToUrlListener ======================

		void					OnUrlCreated(URL* url);
		void					OnUrlDeleted(URL* url);
		void					OnRFC822ToUrlRestarted();
		void					OnRFC822ToUrlDone(OP_STATUS status);
		void					OnMessageBeingDeleted();
		
		// showing / hiding the mail search toolbar
		void					OnSearchingStarted(const OpStringC& search_text);
		void					OnSearchingFinished();

		void					ShowLabelDropdownMenu(INT32 pos);

public:
		// == OpFileChooserListener ======================
		void					OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result);

		// == Value providers =================================
		index_gid_t				GetIndexID() { return m_index_id; };


		// == OpToolTipListener ======================

		virtual BOOL			HasToolTipText(OpToolTip* tooltip) {return TRUE;}
		virtual void			GetToolTipText(OpToolTip* tooltip, OpInfoText& text);
		virtual void			GetToolTipThumbnailText(OpToolTip* tooltip, OpString& title, OpString& url_string, URL& url);

		// == OpPrefsListener ========================

		virtual void			PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue);

		OpWidget*				GetGlobalViewButton();

		void					UpdateSorting();

protected:
								MailDesktopWindow(OpWorkspace* parent_workspace);

private:
		void					UpdateTreeview(index_gid_t new_index_id);
		OP_STATUS				UpdateTitle();
		OP_STATUS				UpdateStatusText();
		void					SetCorrectToolbar(index_gid_t new_index_id);

		OP_STATUS				GetTitleForWindow(OpString& title);

		class UndoRedoAction
		{
			public:

										UndoRedoAction(OpInputAction::Action action, index_gid_t index, UndoRedoAction* additional = 0) : m_action(action), m_index(index), m_additional(additional) {}
										~UndoRedoAction() { OP_DELETE(m_additional); }

				OpInputAction::Action	GetAction() const {return m_action;}

				void					SetItems(OpINT32Vector& items) {m_items.DuplicateOf(items);};
				void					AddItem(message_gid_t item) { m_items.Add(item); }
				const OpINT32Vector&	GetItems() const {return m_items;}

				void					SetIndex(INT32 index) {m_index = index;};
				index_gid_t				GetIndex() const {return m_index;}

				UndoRedoAction*			GetAdditionalUndoRedoAction() const { return m_additional; }

			private:

				OpInputAction::Action	m_action;
				OpINT32Vector			m_items;
				index_gid_t				m_index;
				UndoRedoAction*			m_additional;
		};

		OpWorkspace*			GetSDICorrectedParentWorkspace();

		void					OpenComposeWindow(MessageTypes::CreateType type, message_gid_t message_id);

		void					AddUndoRedo(OpInputAction* action, OpINT32Vector& items);
		BOOL					UndoRedo(BOOL undo, BOOL check_if_available_only);
		void					UndoRedo(UndoRedoAction* action, BOOL undo);

		void					DoAllSelectedMails(OpInputAction* action);
		void					DoActionOnItems(OpInputAction* action, OpINT32Vector& items, INT32 index_id, BOOL add_undo = TRUE);
		BOOL					CheckAndRemoveReadOnly(OpInputAction* action, OpINT32Vector& items, INT32 index_id);

		OP_STATUS				GetSelectedMessage(BOOL full_message, BOOL force = FALSE, BOOL timeout_request = FALSE);

		MailSearchField*		GetMailSearchField() { return static_cast<MailSearchField*>(GetWidgetByType(WIDGET_TYPE_MAIL_SEARCH_FIELD)); }

		/**
		 * @return the OpGroup used to switch between @a m_index_view_group
		 *		and @a m_loading_image.
		 */
		OpWidget*				GetIndexAndLoadingImageGroup() const { return m_index_view_group->GetParent(); }

		URL_InUse				m_current_message_url_inuse;
		message_gid_t			m_saveas_message_id;
		message_gid_t			m_old_session_message_id;

		OpTimer*				m_message_read_timer;
		OpTimer*				m_mail_selected_timer;

		index_gid_t				m_index_id;
		OpString				m_contact_address;

		OpSplitter*				m_splitter;
		OpTreeView*				m_index_view;
		unsigned				m_running_searches;
		OpBrowserView*			m_mail_view;
		OpButton*				m_loading_image; ///< for displaying a spinner before the database has finished loading

		Message*				m_mailview_message;

		OpGroup*				m_index_view_group;

		MessageHeaderGroup*		m_mail_and_headers_group;
		OpInfobar*				m_search_in_toolbar;
		OpToolbar*				m_left_top_toolbar;
		index_gid_t				m_search_in_index;

		OpAutoVector<UndoRedoAction>	m_undo_redo;
		INT32							m_current_undo_redo_position;

		OpINT32Vector					m_history;
		OpAutoINT32HashTable<OpString>	m_quick_replies;
		UINT32							m_current_history_position;

		BOOL							m_show_raw_message;

		INT32					m_old_selected_panel;
		enum MailWindowPanelState
		{
			
			PANEL_HIDDEN = -1,			// no panel selected
			PANEL_ALIGNMENT_OFF = -2,	// panel selector is hidden
			PANEL_HIDDEN_BY_USER = -3	// After the window was create and the panel was shown, the user changed / hid the panel
		};

		BOOL m_allow_external_embeds;
};

#endif //M2_SUPPORT
#endif // MAIL_DESKTOP_WINDOW_H
