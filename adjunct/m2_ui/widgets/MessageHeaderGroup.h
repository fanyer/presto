/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Alexander Remen (alexr@opera.com)
 */

#ifndef MESSAGE_HEADER_GROUP_H
#define MESSAGE_HEADER_GROUP_H

#ifdef M2_SUPPORT

#include "adjunct/m2/src/include/defs.h"

#include "adjunct/quick/widgets/DesktopFileChooserEdit.h"
#include "adjunct/quick_toolkit/contexts/WidgetContext.h"
#include "adjunct/quick_toolkit/widgets/OpPageListener.h"
#include "adjunct/quick_toolkit/widgets/QuickButton.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetContainer.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetDecls.h"
#include "adjunct/quick_toolkit/windows/DesktopWindowListener.h"
#include "modules/mime/attach_listener.h"

class OpInfobar;
class TypedObjectCollection;
class HeaderDisplay;
class OpBrowserView;
class Message;
class Header;
class URL;
class OpFindTextBar;

class MailAddressActionData
{
public:
	INT32 m_id;
	OpString m_email;
	OpString m_name;

	MailAddressActionData(INT32 id)
	: m_id(id)
	{
	}
};

class AddressButton : public QuickButton
{
public:
								AddressButton() : m_data(0) {};
								~AddressButton() { OP_DELETE(m_data); }

	void						SetMailAddressData(MailAddressActionData *data) { m_data = data; }

private:
	MailAddressActionData*		m_data;
};

class MessageHeaderGroup : public OpWidget
						 , public QuickWidgetContainer
						 , public WidgetContext
						 , public MIMEDecodeListener
						 , public OpLoadingListener
						 , public OpPageListener
						 , public DesktopFileChooserListener
						 , public OpPrefsListener
						 , private DesktopWindowListener
{
public:
						MessageHeaderGroup(MailDesktopWindow* window);
	virtual				~MessageHeaderGroup();

	OP_STATUS			Create(Message* message);
	OP_STATUS			ShowNoMessageSelected();

	OP_STATUS			AddIncompleteMessageToolbar(BOOL continuous_connection, BOOL scheduled_for_fetch = FALSE);
	OP_STATUS			AddSuppressExternalEmbedsToolbar(OpString from, BOOL is_spam);
	OP_STATUS			AddAttachment(URL &url, const OpString& suggested_filename);
	void				AddFindTextToolbar(OpFindTextBar* find_text_toolbar);
	OpBrowserView*		GetMailView() { return m_mail_view; }

	virtual void		OnLayout();
	BOOL				OnMouseWheel(INT32 delta,BOOL vertical);
	virtual void		OnDirectionChanged() { OnContentsChanged(); }

	virtual BOOL		OnInputAction(OpInputAction* action);

	virtual BOOL		OnContextMenu(const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked);

	virtual BOOL		OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked);

	virtual OP_STATUS	OnMessageAttachmentReady(URL &url);

	virtual void		OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result);

	OpToolbar*			GetMessageToolbar() { return m_top_toolbar; }

	// == QuickWidgetContainer =====================
	virtual void		OnContentsChanged() { Relayout(); }

	// == OpLoadingListener ========================

	virtual void		OnLoadingFinished(OpWindowCommander* commander, LoadingFinishStatus status);
	virtual void		OnUrlChanged(OpWindowCommander* commander, URL& url) {} // NOTE: This will be removed from OpLoadingListener soon (CORE-35579), don't add anything here!
	virtual void		OnUrlChanged(OpWindowCommander* commander, const uni_char* url) {}
	virtual void		OnStartLoading(OpWindowCommander* commander) {}
	virtual void		OnLoadingProgress(OpWindowCommander* commander, const LoadingInformation* info) {}
	virtual void		OnAuthenticationRequired(OpWindowCommander* commander, OpAuthenticationCallback* callback) {}
	virtual void		OnAuthenticationCancelled(OpWindowCommander* commander, URL_ID authid) {}
	virtual void		OnStartUploading(OpWindowCommander* commander) {}
	virtual void		OnUploadingFinished(OpWindowCommander* commander, LoadingFinishStatus status) {}
	virtual void		OnLoadingCreated(OpWindowCommander* commander) {}
	virtual void		OnUndisplay(OpWindowCommander* commander) {}
	virtual BOOL		OnLoadingFailed(OpWindowCommander* commander, int msg_id, const uni_char* url) { return FALSE; }
	virtual void        OnXmlParseFailure(OpWindowCommander*) {}
#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	virtual void 		OnSearchSuggestionsRequested(OpWindowCommander* commander, const uni_char* url, OpSearchSuggestionsCallback* callback) {}
#endif

	// == OpPageListener ============================

	virtual void		OnPageScaleChanged(OpWindowCommander* commander, UINT32 newScale) { m_zoom_scale = newScale; UpdateMessageHeight(); }

	// == OpPrefsListener ===========================

	virtual void		PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue);

	// == OpWidgetListener ==========================

	virtual void OnChange(OpWidget *widget, BOOL changed_by_mouse = FALSE);

	// == DesktopWindowListener =====================

	virtual void OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);

	// == OpTimerListener ===========================
	
	virtual void		OnTimeOut(OpTimer* timer);

	DesktopFileChooserRequest	m_request;

private:
	void				UpdateMessages(Index* index, BOOL collapse_thread = FALSE);
	void				SetCorrectToolbar();
	void				RemoveAll();
	OP_STATUS			AddHeader(Header* header);
	OP_STATUS			AddMailHeaders(Message* message);
	OP_STATUS			AddContactToStackLayout(Header* header, const OpStringC8 stacklayout_name);
	
	OP_STATUS			StartExpandTimer();
	void				UpdateMessageHeight();
	void				UpdateMessageWidth();
	OP_STATUS			ShowQuickReply();
	Index*				GetSelectedContactIndex(BOOL create_if_null);
	BOOL				OpenAttachment();
	BOOL				SendQuickReply();
	void				UpdateContactIcon();
	OP_STATUS			SaveQuickReply();

	MailDesktopWindow*		m_mail_window;
	message_gid_t			m_m2_id;
	OpAutoVector<URL>		m_attachments;
	OpAutoVector<Header>	m_headers;
	URL						m_selected_attachment_url;
	HeaderDisplay*			m_header_display;
	OpBrowserView*			m_mail_view;
	DesktopFileChooser*		m_chooser;
	QuickGroup*				m_optoolbar_group;
	OpToolbar*				m_top_toolbar;
	OpInfobar*				m_incomplete_message_bar;
	OpInfobar*				m_suppress_external_embeds_bar;
	OpFindTextBar*			m_find_text_toolbar;
	QuickWidget*			m_header_and_message;
	OpVector<TypedObjectCollection> m_custom_headers;
	TypedObjectCollection*	m_widgets;
	OpMultilineEdit*		m_quick_reply_edit;
	OpTimer*				m_resize_message_timer;
	
	// members to keep information when switching between messages
	UINT32							m_zoom_scale;
	OpDocumentListener::ImageDisplayMode m_image_mode;
	OpAutoINT32HashTable<OpString>	m_quick_replies;

	unsigned				m_minimum_mail_height;
	unsigned				m_minimum_mail_width;

	// actions coming from attachment and contact menus are handled with the following members (before they were handled in the buttons)
	URL*					m_url;
	INT32					m_selected_id;
	OpString				m_selected_email;
	OpString				m_selected_name;
};

#endif // M2_SUPPORT
#endif // MESSAGE_HEADER_GROUP_H
