/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef COMPOSE_DESKTOP_WINDOW_H
#define COMPOSE_DESKTOP_WINDOW_H

#if defined M2_SUPPORT

#include "adjunct/desktop_util/treemodel/simpletreemodel.h"
#include "adjunct/quick_toolkit/contexts/WidgetContext.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetContainer.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick/windows/DesktopTab.h"
#include "adjunct/m2/src/engine/message.h"
#include "adjunct/m2/src/composer/attachmentholder.h"
#include "adjunct/m2_ui/widgets/RichTextEditor.h"
#include "adjunct/m2_ui/widgets/RichTextEditorListener.h"
#include "modules/hardcore/timer/optimer.h"
#include "modules/widgets/OpWidgetFactory.h"


/***********************************************************************************
**
**	ComposeDesktopWindow
**
***********************************************************************************/

class ComposeDesktopWindow : public DesktopTab
						   , public OpTimerListener
						   , public RichTextEditorListener
						   , public QuickWidgetContainer
						   , public WidgetContext
						   , public OpPrefsListener
{
	protected:
								ComposeDesktopWindow(OpWorkspace* parent_workspace);
	public:

		static OP_STATUS		Construct(ComposeDesktopWindow** obj, OpWorkspace* parent_workspace);

		virtual					~ComposeDesktopWindow();

		OP_STATUS				Init();

		OP_STATUS				InitMessage(MessageTypes::CreateType create_type, INT32 message_id = 0, UINT16 account_id = 0);

		virtual OP_STATUS		SetHeaderValue(Header::HeaderType type, const OpStringC* string);
		// the message argument is supposed to contain no HTML
		virtual OP_STATUS		SetMessageBody(const OpStringC& message);


		virtual void			SetFocusToBody() { m_rich_text_editor->SetFocusToBody(); }
		virtual void			SetFocusToHeaderOrBody();
		virtual void			SelectAccount(UINT16 account_id=0, BOOL propagate_event = TRUE, AccountTypes::AccountTypeCategory account_type_category=AccountTypes::TYPE_CATEGORY_MAIL);
		/**
		  * GetCorrectedCharSet will find out which charset we should use based on the characters in the body
		  * @param body - OpString to check for illegal chars
		  * @param charset_to_use - return value, this is the charset that should be used for that body 
		  */
		void					GetCorrectedCharSet(OpString& body, OpString8& charset_to_use);

		virtual OP_STATUS		SelectEncoding(const OpStringC8& charset);
		virtual void			SelectEncoding(const OpStringC16& charset);

		void					AskForAttachment();

        OP_STATUS               AddAttachment(const uni_char* filepath, const uni_char* suggested_filename, URL& file_url, BOOL is_inlined);

		/**
		 * Add attachments to the mail
		 * @param filepath paths to files which should be attached to the mail. The paths have to be separated by '|'
		 * @param filename (optional)suggested names of attached files, the names should be separated by '|'
		 *				   Number of '|' should be not less then in filepath.
		 * @return OpStatus::OK if all files have been attached
		 */
		OP_STATUS				AddAttachment(const uni_char* filepath, const uni_char* filename = NULL);

		INT32					GetMessageId() {return m_message_id;}

		INT16					GetAccountId() {return m_message.GetAccountId(); }

		MessageTypes::CreateType	GetCreateType() const {return m_create_type;}

		OP_STATUS				ReplaceSignature(const OpStringC& new_signature, UINT16 old_signature_account_id);

		// Subclassing DesktopWindow

		virtual const char*		GetWindowName() {return "Compose Window";}
		virtual void			OnClose(BOOL user_initiated);
		void					ImageSelected(OpString& image_path);
		OpString				GenerateContentId(const uni_char* string_seed);
		void					OnSettingsChanged(DesktopSettings* settings);

		// OpWidgetListener

		virtual void			OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		virtual void			OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		void					OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
		virtual BOOL			OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked);

		// Hooks subclassed from DesktopWindow

		virtual void			OnLayout();
		virtual void			OnDropFiles(const OpVector<OpString>& file_names);
	    virtual void            OnChange(OpWidget *widget, BOOL changed_by_mouse = FALSE);
		virtual void			OnSessionReadL(const OpSessionWindow* session_window);
		virtual void			OnSessionWriteL(OpSession* session, OpSessionWindow* session_window, BOOL shutdown);
		virtual OpBrowserView*	GetBrowserView() {return m_rich_text_editor ? m_rich_text_editor->GetBrowserView() : NULL;}
		OpWindowCommander*		GetWindowCommander() { return m_rich_text_editor && GetBrowserView() ? m_rich_text_editor->GetBrowserView()->GetWindowCommander(): NULL; }

		// RichTextEditorListener
		virtual void			OnTextChanged();
		virtual void			OnInsertImage();
		virtual void			OnTextDirectionChanged (BOOL to_rtl);

			// == QuickWidgetContainer =====================
		virtual void			OnContentsChanged() { Relayout(); }
		
		// OpTypedObject
		virtual Type			GetType() {return WINDOW_TYPE_MAIL_COMPOSE;}

		// OpTimerListener

		virtual void			OnTimeOut(OpTimer *timer);

		// == OpInputContext ======================

		virtual BOOL			OnInputAction(OpInputAction* action);
		virtual const char*		GetInputContextName() {return "Compose Window";}

		OP_STATUS				SaveAttachments();

		// == OpPrefsListener ===========================

		virtual void		PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue);

		
	private:
		void					UpdateMessageWidth();
		void					UpdateVisibleHeaders();
		void					StartSaveTimer(OpWidget&);
		void					SetWillSave(BOOL save) { m_will_save = save && !IsClosing(); }
		void					OnSubjectChanged(OpWidget& widget);
		OP_STATUS				SaveMessage(BOOL for_sending);
		OP_STATUS				SetMessageAccountFromInput(BOOL warn_on_invalid_sender);
		OP_STATUS				SetMessageHTMLBodyFromInput();
		OP_STATUS				SetMessageHeadersFromInput(BOOL warn_on_invalid_headers);
		OP_STATUS				SetMessagePriorityFromInput();
		OP_STATUS				SaveAsDraft();
		OP_STATUS				CheckForOutgoingServer();
		OP_STATUS				SendMessage();
		void					AppendContact(OpEdit* edit, INT32 contact_id, const OpStringC& contact_string);
		void					SetDirection(int direction, BOOL update_body = TRUE,  BOOL force_autodetect = FALSE);
		OP_STATUS				ValidateMailAdresses(enum Header::HeaderType header, OpString& invalid_mails);

		INT32						m_message_id;
		MessageTypes::CreateType	m_create_type;
		int							m_initial_account_id;

		Message						m_message;

		SimpleTreeModel				m_attachments_model;
		AttachmentHolder			m_attachments;

		OpTimer*					m_save_draft_timer;
		BOOL						m_will_save;
		BOOL						m_hovering_attachments;

		QuickWidget*				m_content;
		TypedObjectCollection*		m_widgets;
		RichTextEditor*				m_rich_text_editor;
		OpToolbar*					m_toolbar;
		OpTreeView*					m_attachment_list;
};

#endif //M2_SUPPORT

#endif // COMPOSE_DESKTOP_WINDOW_H
