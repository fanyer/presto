/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Alexander Remen (alexr@opera.com)
 *
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/desktop_util/actions/action_utils.h"
#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/desktop_util/mail/mailcompose.h"
#include "adjunct/desktop_util/mail/mailto.h"
#include "adjunct/desktop_util/prefs/PrefsCollectionM2.h"
#include "adjunct/desktop_util/string/htmlify_more.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_pi/desktop_menu_context.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/index.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/engine/store/store3.h"
#include "adjunct/m2_ui/dialogs/CustomizeHeaderDialog.h"
#include "adjunct/m2_ui/dialogs/SaveAttachmentsDialog.h"
#include "adjunct/m2_ui/models/indexmodel.h"
#include "adjunct/m2_ui/widgets/MessageHeaderGroup.h"
#include "adjunct/m2_ui/windows/MailDesktopWindow.h"
#include "adjunct/m2_ui/util/AttachmentOpener.h"
#include "adjunct/quick_toolkit/widgets/QuickBrowserView.h"
#include "adjunct/quick_toolkit/widgets/QuickDropDown.h"
#include "adjunct/quick_toolkit/widgets/QuickLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickIcon.h"
#include "adjunct/quick_toolkit/widgets/QuickMultilineLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickDynamicGrid.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickStackLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickWrapLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickSkinElement.h"
#include "adjunct/quick/hotlist/Hotlist.h"
#include "adjunct/quick/dialogs/DownloadDialog.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick/managers/DesktopClipboardManager.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/widgets/OpInfobar.h"
#include "adjunct/quick/widgets/OpFindTextBar.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"

#include "modules/windowcommander/OpViewportController.h"

MessageHeaderGroup::MessageHeaderGroup(MailDesktopWindow* window)
	: m_mail_window(window)
	, m_m2_id(0)
	, m_header_display(NULL)
	, m_mail_view(NULL)
	, m_chooser(NULL)
	, m_optoolbar_group(NULL)
	, m_top_toolbar(NULL)
	, m_incomplete_message_bar (NULL)
	, m_suppress_external_embeds_bar(NULL)
	, m_find_text_toolbar(NULL)
	, m_header_and_message(NULL)
	, m_widgets(NULL)
	, m_quick_reply_edit(NULL)
	, m_resize_message_timer(NULL)
	, m_zoom_scale(100)
	, m_image_mode(OpDocumentListener::ALL_IMAGES)
	, m_minimum_mail_height(200)
	, m_minimum_mail_width(10000)
	, m_url(NULL)
	, m_selected_id(-1)
{
	window->AddListener(this);
	
	if(g_m2_engine && g_m2_engine->GetAccountManager())
	{
		m_header_display = g_m2_engine->GetAccountManager()->GetHeaderDisplay();
	}

	init_status = OpToolbar::Construct(&m_top_toolbar);
	CHECK_STATUS(init_status);
	m_top_toolbar->GetBorderSkin()->SetImage("Mailbar skin");
	AddChild(m_top_toolbar);
	SetCorrectToolbar();
}

void MessageHeaderGroup::SetCorrectToolbar()
{
	OpString8 toolbar_name;

	if ((m_mail_window->GetCurrentMessage() && m_mail_window->GetCurrentMessage()->IsFlagSet(StoreMessage::IS_NEWSFEED_MESSAGE)) || 
		g_m2_engine->GetAccountManager()->GetMailNewsAccountCount() == 0)
		RETURN_VOID_IF_ERROR(toolbar_name.Set("Feed "));
	else
		RETURN_VOID_IF_ERROR(toolbar_name.Set("Mail "));

	if (g_pcm2->GetIntegerPref(PrefsCollectionM2::MailViewMode) == MailLayout::LIST_ON_LEFT_MESSAGE_ON_RIGHT && !m_mail_window->IsMessageOnlyView())
		RETURN_VOID_IF_ERROR(toolbar_name.Append("Right Toolbar"));
	else
		RETURN_VOID_IF_ERROR(toolbar_name.Append("Bottom Toolbar"));
	
	if (m_top_toolbar->GetName() != toolbar_name)
		m_top_toolbar->SetName(toolbar_name);
}

MessageHeaderGroup::~MessageHeaderGroup()
{
	RemoveAll();
	OP_DELETE(m_chooser);
	OP_DELETE(m_resize_message_timer);
	if (m_mail_window)
		m_mail_window->RemoveListener(this);
}

OP_STATUS MessageHeaderGroup::ShowNoMessageSelected()
{
	SaveQuickReply();
	RemoveAll();

	m_widgets = OP_NEW(TypedObjectCollection, ());
	RETURN_OOM_IF_NULL(m_widgets);
	
	RETURN_IF_ERROR(CreateQuickWidget("Mail Window No Message Selected", m_header_and_message, *m_widgets));

	m_header_and_message->SetContainer(this);
	m_header_and_message->SetParentOpWidget(this);

	FontAtt mailfont;
	g_pcfontscolors->GetFont(OP_SYSTEM_FONT_UI_TOOLBAR, mailfont);
	WIDGET_FONT_INFO info = m_widgets->Get<QuickLabel>("No Message Selected Label")->GetOpWidget()->font_info;
	info.size = mailfont.GetSize()*1.5;
	info.weight = 6;
	info.font_info = styleManager->GetFontInfo(mailfont.GetFontNumber());
	m_widgets->Get<QuickLabel>("No Message Selected Label")->GetOpWidget()->SetFontInfo(info);

	if (!g_m2_engine->GetStore()->HasFinishedLoading())
	{
		m_widgets->Get<QuickIcon>("No Message Selected Image")->SetImage("Mail Loading Message");
		RETURN_IF_ERROR(m_widgets->Get<QuickLabel>("No Message Selected Label")->SetText(Str::S_LOADING_MESSAGE));
	}
	return OpStatus::OK;
}

OP_STATUS MessageHeaderGroup::SaveQuickReply()
{
	if (g_pcm2->GetIntegerPref(PrefsCollectionM2::ShowQuickReply) && m_m2_id != 0 && m_quick_reply_edit && m_quick_reply_edit->GetTextLength(FALSE) > 0)
	{
		OpString* reply = NULL;
		OpStatus::Ignore(m_quick_replies.GetData(m_m2_id, &reply));

		if (!reply)
		{
			reply = OP_NEW(OpString, ());
			RETURN_OOM_IF_NULL(reply);
			RETURN_IF_ERROR(m_quick_replies.Add(m_m2_id, reply));	
		}
		
		RETURN_IF_ERROR(m_quick_reply_edit->GetText(*reply));
	}
	return OpStatus::OK;
}

OP_STATUS MessageHeaderGroup::Create(Message* message)
{
	if(!message)
	{
		return OpStatus::ERR;
	}

	SaveQuickReply();
	RemoveAll();

	m_m2_id = message->GetId();

	m_widgets = OP_NEW(TypedObjectCollection, ());
	RETURN_OOM_IF_NULL(m_widgets);
	
	RETURN_IF_ERROR(CreateQuickWidget(message->IsFlagSet(StoreMessage::IS_NEWSFEED_MESSAGE) ? 
		"Mail Window Feed Message Pane" : "Mail Window Mail Message Pane", m_header_and_message, *m_widgets));

	OpWidget* focused_widget = GetFocused();
	// Setting the parent OpWidget, adds the browser view as a child and does a GenerateOnShow() which sets the focus on the BrowserView.
	// not good!
	m_header_and_message->SetContainer(this);
	m_header_and_message->SetParentOpWidget(this);
	if (focused_widget)
		focused_widget->RestoreFocus(FOCUS_REASON_ACTIVATE);

	UpdateMessageWidth();

	m_mail_view = m_widgets->Get<QuickBrowserView>("mw_mail_view")->GetOpBrowserView();		
	
	OpWindowCommander *wc = m_mail_view->GetWindowCommander();

	m_mail_view->AddListener(this);
	SetCorrectToolbar();
	if (message->IsFlagSet(StoreMessage::IS_NEWSFEED_MESSAGE))
	{	
		m_mail_view->GetWindow()->SetType(WIN_TYPE_NEWSFEED_VIEW);
		RETURN_IF_ERROR(AddHeader(message->GetHeader(Header::DATE)));
		RETURN_IF_ERROR(m_widgets->Get<QuickLabel>("hl_Author")->SetText(message->GetNiceFrom()));

		FontAtt mailfont;
		g_pcfontscolors->GetFont(OP_SYSTEM_FONT_UI_HEADER, mailfont);
		WIDGET_FONT_INFO info = m_widgets->Get<QuickMultilineLabel>("hl_Subject")->GetOpWidget()->font_info;
		info.size = mailfont.GetSize();
		info.font_info = styleManager->GetFontInfo(mailfont.GetFontNumber());
		m_widgets->Get<QuickMultilineLabel>("hl_Subject")->GetOpWidget()->SetFontInfo(info);
		RETURN_IF_ERROR(AddHeader(message->GetHeader(Header::SUBJECT)));
	}
	else
	{
		m_mail_view->GetWindow()->SetType(WIN_TYPE_MAIL_VIEW);
		m_quick_reply_edit = m_widgets->Get<QuickMultilineEdit>("mw_QuickReplyEdit")->GetOpWidget();
		
		if (g_pcm2->GetIntegerPref(PrefsCollectionM2::ShowQuickReply))
		{
			RETURN_IF_ERROR(ShowQuickReply());
		}
		RETURN_IF_ERROR(AddMailHeaders(message));
	}

	if (wc)
	{
		wc->SetMailClientListener(m_mail_window);
		wc->SetLoadingListener(this);
		wc->SetLayoutMode(g_pcm2->GetIntegerPref(PrefsCollectionM2::FitToWidth) ? OpWindowCommander::AMSR : OpWindowCommander::NORMAL);
		wc->SetPrivacyMode(m_mail_window->GetParentDesktopWindow()->PrivacyMode());
		wc->SetScale(m_zoom_scale);
		WindowCommanderProxy::EnableImages(wc, m_image_mode);
	}

	m_optoolbar_group = m_widgets->Get<QuickGroup>("mw_ToolbarGroup");

	if (message->IsFlagSet(Message::PARTIALLY_FETCHED))
	{
		RETURN_IF_ERROR(AddIncompleteMessageToolbar(message->GetAccountPtr() ? message->GetAccountPtr()->HasContinuousConnection() : FALSE, (message->GetAccountPtr() && message->GetAccountPtr()->IsScheduledForFetch(message->GetId()))));
	}

	if (m_widgets->Get<QuickBrowserView>("mw_mail_view")->GetPreferredWidth() != 10000)
	{		
		RETURN_IF_ERROR(StartExpandTimer());
	}

	return g_pcm2->RegisterListener(this);
}

void MessageHeaderGroup::RemoveAll()
{
	g_pcm2->UnregisterListener(this);

	if (m_resize_message_timer)
		m_resize_message_timer->Stop();

	m_m2_id = 0;
	m_url = NULL;
	m_selected_id = -1;
	m_selected_email.Empty();
	m_selected_name.Empty();

	if (m_mail_view && m_mail_view->GetWindowCommander())
	{
		m_image_mode = m_mail_view->GetWindowCommander()->GetImageMode();

		m_mail_view->GetWindowCommander()->SetMailClientListener(NULL);
		m_mail_view->GetWindowCommander()->SetLoadingListener(NULL);
		m_mail_view = NULL;
	}

	OP_DELETE(m_widgets);
	m_widgets = NULL;

	OP_DELETE(m_header_and_message);
	m_header_and_message = NULL;

	m_find_text_toolbar->SetAlignment(OpBar::ALIGNMENT_OFF);
	m_find_text_toolbar->SetVisibility(FALSE);

	m_incomplete_message_bar = NULL;
	m_suppress_external_embeds_bar = NULL;
	m_optoolbar_group = NULL;
	m_quick_reply_edit = NULL;
	m_custom_headers.DeleteAll();
	m_attachments.DeleteAll();
	m_headers.DeleteAll();
}


OP_STATUS MessageHeaderGroup::AddMailHeaders(Message* message)
{
	if (!m_header_display)
		return OpStatus::OK;

	for (HeaderDisplayItem* item = m_header_display->GetFirstItem(); item; item = static_cast<HeaderDisplayItem*>(item->Suc()))
	{
		Header* header;
		if (item->m_display && (header = message->GetHeader(item->m_headername)))
			RETURN_IF_ERROR(AddHeader(header));
	}

	return OpStatus::OK;
}

OP_STATUS MessageHeaderGroup::AddHeader(Header* h)
{
	Header::HeaderValue value;

	if(!h || h->GetType() > Header::UNKNOWN || OpStatus::IsError(h->GetValue(value)) || value.IsEmpty())
	{
		return OpStatus::OK;
	}

	Header *header = OP_NEW(Header, (*h));

	if (!header || OpStatus::IsError(m_headers.Add(header)))
	{
		OP_DELETE(header);
		return OpStatus::ERR_NO_MEMORY;
	}

	switch (header->GetType())
	{
		case Header::SUBJECT:
		{
			QuickMultilineLabel* subject = m_widgets->Get<QuickMultilineLabel>("hl_Subject");
			RETURN_IF_ERROR(subject->SetText(value));
			if (!m_mail_window->GetCurrentMessage()->IsFlagSet(StoreMessage::IS_NEWSFEED_MESSAGE))
			{
				subject->SetNominalWidth(WidgetUtils::GetStringWidth(subject->GetOpWidget()->font_info, value.CStr()));
			}
			break;
		}
		case Header::DATE:
		{
			OpString date;
			time_t t;
			header->GetValue(t);

			// FIXME: %#c is nice on windows, but we need %c on other platforms
			OpString time_format;
#ifdef MSWIN
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_MAIL_HEADER_TIME_FORMAT, time_format));
#else
			RETURN_IF_ERROR(time_format.Set(UNI_L("%c")));
#endif
			RETURN_IF_ERROR(FormatTime(date, time_format.CStr(), t));
				
			RETURN_IF_ERROR(m_widgets->Get<QuickLabel>("hl_Date")->SetText(date));
			m_widgets->Get<QuickLabel>("hl_Date")->GetOpWidget()->SetJustify(JUSTIFY_RIGHT, FALSE);
			break;
		}
		case Header::ORGANIZATION:
		{
			RETURN_IF_ERROR(m_widgets->Get<QuickLabel>("hl_Organization")->SetText(value));
			m_widgets->Get<QuickLabel>("hl_Organization")->Show();
			break;
		}
		case Header::FROM:
		{
			RETURN_IF_ERROR(AddContactToStackLayout(header, "hs_FromHeaders"));
			if (m_widgets->Contains<QuickLabel>("hl_From"))
				m_widgets->Get<QuickLabel>("hl_From")->Show();
			break;
		}
		case Header::TO:
		{
			RETURN_IF_ERROR(AddContactToStackLayout(header, "hs_ToHeaders"));
			m_widgets->Get<QuickLabel>("hl_To")->Show();
			break;
		}
		case Header::CC:
		{
			RETURN_IF_ERROR(AddContactToStackLayout(header, "hs_CCHeaders"));
			m_widgets->Get<QuickLabel>("hl_CC")->Show();
			break;
		}
		case Header::BCC:
		{
			RETURN_IF_ERROR(AddContactToStackLayout(header, "hs_BCCHeaders"));
			m_widgets->Get<QuickLabel>("hl_BCC")->Show();
			break;
		}
		default:
		{
			QuickDynamicGrid* headergrid = m_widgets->Get<QuickDynamicGrid>("CustomHeaderGrid");
			OpAutoPtr<TypedObjectCollection> row(OP_NEW(TypedObjectCollection,()));
			RETURN_OOM_IF_NULL(row.get());

			headergrid->Instantiate(*row.get());
			OpString title;
			RETURN_IF_ERROR(header->GetTranslatedName(title));
			RETURN_IF_ERROR(row->Get<QuickLabel>("CustomHeaderTitle")->SetText(title));
			RETURN_IF_ERROR(row->Get<QuickMultilineLabel>("CustomHeaderValue")->SetText(value));
			RETURN_IF_ERROR(m_custom_headers.Add(row.get()));
			row.release();
			break;
		}
	}
	return OpStatus::OK;
}

OP_STATUS MessageHeaderGroup::AddContactToStackLayout(Header* header, const OpStringC8 stacklayout_name)
{
	if (!m_widgets->Get<QuickWrapLayout>(stacklayout_name)->IsVisible())
		m_widgets->Get<QuickWrapLayout>(stacklayout_name)->Show();

	const Header::From* from = header->GetLastAddress();
	bool need_comma = false;

	while (from)
	{	
		HotlistModelItem *item = g_hotlist_manager->GetContactsModel()->GetByEmailAddress(from->GetAddress().CStr());
				
		OpAutoPtr<MailAddressActionData> data (OP_NEW(MailAddressActionData, (item ? item->GetID() : -1)));
		RETURN_OOM_IF_NULL(data.get());
		RETURN_IF_ERROR(data->m_email.Set(from->GetAddress()));
		if (header->GetType() == Header::FROM && from->GetName().HasContent())
			RETURN_IF_ERROR(data->m_name.Set(from->GetName()));
		else if (item && item->GetName().HasContent())
			RETURN_IF_ERROR(data->m_name.Set(item->GetName()));
		else
			RETURN_IF_ERROR(data->m_name.Set(from->GetName()));
		
		OpInputAction* input_action = NULL;
		OpAutoPtr<OpInputAction> ptr(NULL);
		if (item || data->m_email.HasContent())
		{
			OpString8 action_string;
			RETURN_IF_ERROR(action_string.AppendFormat("Show hidden popup menu, \"%s %s Popup Menu\"", header->GetType() == Header::FROM? "From" : "Mail", item ? "Contact" : "Address"));
			RETURN_IF_ERROR(OpInputAction::CreateInputActionsFromString(action_string, input_action));
			ptr.reset(input_action);
		}

		OpAutoPtr<AddressButton> new_button (OP_NEW(AddressButton, ()));
		RETURN_OOM_IF_NULL(new_button.get());
		RETURN_IF_ERROR(new_button->Init());

		if (input_action)
		{
			// the contact actions use this data to know which contact to deal with
			input_action->SetActionData((INTPTR)data.get());
			input_action->GetActionInfo().SetTooltipText(data->m_email.CStr());
			new_button->SetAction(input_action);
		}

		new_button->GetOpWidget()->SetButtonTypeAndStyle(OpButton::TYPE_TOOLBAR, OpButton::STYLE_TEXT);
		new_button->GetOpWidget()->SetSystemFont(OP_SYSTEM_FONT_UI_DIALOG);
		new_button->GetOpWidget()->SetFixedTypeAndStyle(TRUE);
		new_button->SetSkin("Mail Window Header Button Skin");

		OpString button_text;

		if (data->m_name.IsEmpty())
		{
			RETURN_IF_ERROR(button_text.Set(data->m_email));
		}
		else if (data->m_email.IsEmpty())
		{
			RETURN_IF_ERROR(button_text.Set(data->m_name));
		}
		else if (header->GetType() == Header::FROM || !item)
		{			
			RETURN_IF_ERROR(button_text.AppendFormat(UNI_L("%s <%s>"), data->m_name.CStr(), data->m_email.CStr()));
			
		}
		else
		{
			RETURN_IF_ERROR(button_text.Set(data->m_name));
		}

		if (need_comma)
			RETURN_IF_ERROR(button_text.Append(UNI_L(",")));

		RETURN_IF_ERROR(new_button->SetText(button_text));
		// we set the data in the button so that it's deleted when the button is deleted
		new_button->SetMailAddressData(data.release());
		new_button->GetOpWidget()->SetTabStop(g_op_ui_info->IsFullKeyboardAccessActive());

		new_button->SetMinimumWidth(0);
		new_button->SetEllipsis(ELLIPSIS_CENTER);

		RETURN_IF_ERROR(m_widgets->Get<QuickWrapLayout>(stacklayout_name)->InsertWidget(new_button.release(), 0));

		from = (Header::From*)from->Pred();
		need_comma = true;
	}
	return OpStatus::OK;
}


OP_STATUS MessageHeaderGroup::OnMessageAttachmentReady(URL& url)
{
	OpString suggested_filename;
	RETURN_IF_LEAVE(url.GetAttributeL(URL::KSuggestedFileName_L, suggested_filename, FALSE));
	OpAutoPtr<URL> attachment (OP_NEW(URL, (url)));
	RETURN_OOM_IF_NULL(attachment.get());
	RETURN_IF_ERROR(m_attachments.Add(attachment.get()));
	attachment.release();
	return AddAttachment(url, suggested_filename);
}

OP_STATUS MessageHeaderGroup::AddAttachment(URL &url, const OpString& suggested_filename)
{
	if (!m_widgets)
		return OpStatus::ERR;

	OpAutoPtr<QuickButton> new_button (OP_NEW(QuickButton, ()));
	RETURN_OOM_IF_NULL(new_button.get());
	RETURN_IF_ERROR(new_button->Init());
	new_button->GetOpWidget()->SetButtonTypeAndStyle(OpButton::TYPE_TOOLBAR, OpButton::STYLE_IMAGE_AND_TEXT_ON_RIGHT);
	new_button->GetOpWidget()->SetSystemFont(OP_SYSTEM_FONT_UI_DIALOG);
	new_button->GetOpWidget()->SetFixedTypeAndStyle(TRUE);
	RETURN_IF_ERROR(new_button->SetText(suggested_filename));

	URL* urlcopy = OP_NEW(URL, (url));
	OpInputAction* input_action;
	RETURN_IF_ERROR(OpInputAction::CreateInputActionsFromString("Show hidden popup menu, \"Mail Attachment Toolbar Popup\"", input_action));
	OpAutoPtr<OpInputAction> ptr(input_action);
	input_action->SetActionData((INTPTR)urlcopy);

	OpString tooltip, string;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDL_SIZE, string));
	
	OpFileLength size = url.ContentLoaded();
	OpString formatted_size;
	RETURN_IF_ERROR(FormatByteSize(formatted_size, size));
	RETURN_IF_ERROR(tooltip.AppendFormat("%s\r\n%s: %s",suggested_filename.CStr(), string.CStr(), formatted_size.CStr()));
	input_action->GetActionInfo().SetTooltipText(tooltip);
	
	RETURN_IF_ERROR(new_button->SetAction(input_action));

	Image iconimage;
	OpBitmap* iconbitmap = NULL;

	OpString content_type_name;
	OpString content_type;
	const char* mimetype = url.GetAttribute(URL::KMIME_Type).CStr();
	content_type.Set(mimetype);
	g_desktop_op_system_info->GetFileTypeInfo(suggested_filename, content_type, content_type_name, iconbitmap);

	if(iconbitmap)
	{
		iconimage = imgManager->GetImage(iconbitmap);
		if (iconimage.IsEmpty())     // OOM
			OP_DELETE(iconbitmap);
	}
	new_button->GetOpWidget()->GetForegroundSkin()->SetBitmapImage(iconimage);
	new_button->GetOpWidget()->SetFixedImage(TRUE);
	new_button->SetSkin("Mail Window Header Button Skin");
	new_button->SetMinimumWidth(0);
	new_button->SetEllipsis(ELLIPSIS_CENTER);

	RETURN_IF_ERROR(m_widgets->Get<QuickWrapLayout>("hs_Attachments")->InsertWidget(new_button.release()));

	if (!m_widgets->Get<QuickWrapLayout>("hs_Attachments")->IsVisible())
	{
		m_widgets->Get<QuickLabel>("hl_Attachments")->Show();
		m_widgets->Get<QuickWrapLayout>("hs_Attachments")->Show();
	}
	if (m_attachments.GetCount() < 2)
		m_widgets->Get<QuickButton>("hb_SaveAttachments")->Hide();
	else
		m_widgets->Get<QuickButton>("hb_SaveAttachments")->Show();

	return OpStatus::OK;
}

void MessageHeaderGroup::AddFindTextToolbar(OpFindTextBar* find_text_toolbar) 
{
	m_find_text_toolbar = find_text_toolbar; 
	AddChild(m_find_text_toolbar); 
}

void MessageHeaderGroup::OnLayout()
{
	OpRect bounds = GetBounds();
	if (m_top_toolbar)
		bounds = m_top_toolbar->LayoutToAvailableRect(bounds);

	if (!m_header_and_message)
		return;

	if (m_header_and_message)
	{
		m_header_and_message->Layout(bounds);
	}
	
	if (!m_optoolbar_group)
		return;

	if (m_widgets->Contains<QuickWrapLayout>("hs_ToHeaders") && 
		m_widgets->Get<QuickWrapLayout>("hs_ToHeaders")->HasHiddenItems() && !m_widgets->Get<QuickWidget>("hb_Show_More_ToHeaders")->IsVisible())
	{
		m_widgets->Get<QuickWidget>("hb_Show_More_ToHeaders")->Show();
	}
	if (m_widgets->Contains<QuickWrapLayout>("hs_CCHeaders") &&
		m_widgets->Get<QuickWrapLayout>("hs_CCHeaders")->HasHiddenItems() && !m_widgets->Get<QuickWidget>("hb_Show_More_CCHeaders")->IsVisible())
	{
		m_widgets->Get<QuickWidget>("hb_Show_More_CCHeaders")->Show();
	}
	if (m_widgets->Contains<QuickWrapLayout>("hs_BCCHeaders") && 
		m_widgets->Get<QuickWrapLayout>("hs_BCCHeaders")->HasHiddenItems() && !m_widgets->Get<QuickWidget>("hb_Show_More_BCCHeaders")->IsVisible())
	{
		m_widgets->Get<QuickWidget>("hb_Show_More_BCCHeaders")->Show();
	}
	if (m_widgets->Contains<QuickWrapLayout>("hs_Attachments") && 
		m_widgets->Get<QuickWrapLayout>("hs_Attachments")->HasHiddenItems() && !m_widgets->Get<QuickWidget>("hb_Show_More_Attachments")->IsVisible())
	{
		m_widgets->Get<QuickWidget>("hb_Show_More_Attachments")->Show();
	}

	// this is messy because m_incomplete_message_bar and m_suppress_external_embeds_bar are direct children of the m_optoolbar_group
	// the m_find_text_toolbar is created by the DesktopTab in MailDesktopWindow and needs to be kept alive through the life of the MessageHeaderGroup
	// the x and y coordinates are therefore a bit confusing and of course related to which is their parent
	OpRect toolbargrouprect = m_optoolbar_group->GetOpWidget()->GetRect();
	OpRect rect_to_use (0, 0, toolbargrouprect.width, toolbargrouprect.height);

	INT32 incomplete_height = 0, suppress_height = 0, find_text_height = 0;

	if (m_incomplete_message_bar)
	{
		incomplete_height = m_incomplete_message_bar->GetHeightFromWidth(rect_to_use.width);
	}
	if (m_suppress_external_embeds_bar)
	{
		suppress_height = m_suppress_external_embeds_bar->GetHeightFromWidth(rect_to_use.width);
	}
	if (m_find_text_toolbar->IsVisible())
	{
		find_text_height = m_find_text_toolbar->GetHeightFromWidth(rect_to_use.width);
	}

	if (m_optoolbar_group)
		m_optoolbar_group->SetFixedHeight(incomplete_height+suppress_height+find_text_height);

	if (m_incomplete_message_bar)
	{
		rect_to_use = m_incomplete_message_bar->LayoutToAvailableRect(rect_to_use);
	}

	if (m_suppress_external_embeds_bar)
	{
		rect_to_use = m_suppress_external_embeds_bar->LayoutToAvailableRect(rect_to_use);
	}

	if (m_find_text_toolbar)
	{
		
		rect_to_use.x += m_optoolbar_group->GetOpWidget()->GetParent()->GetRect().x + toolbargrouprect.x;
		rect_to_use.y += m_optoolbar_group->GetOpWidget()->GetParent()->GetParent()->GetRect().y + 
						 m_optoolbar_group->GetOpWidget()->GetParent()->GetParent()->GetParent()->GetRect().y +
						 m_optoolbar_group->GetOpWidget()->GetParent()->GetRect().y + 
						 toolbargrouprect.y;
		
		rect_to_use = m_find_text_toolbar->LayoutToAvailableRect(rect_to_use);
	}

}

BOOL MessageHeaderGroup::OnContextMenu(const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked)
{
	if (!m_widgets)
		return FALSE;

	QuickSkinElement* root_widget;
	OP_STATUS error;
	TRAP_AND_RETURN_VALUE_IF_ERROR(error, root_widget = m_widgets->GetL<QuickSkinElement>("Mail Window Toolbar And Headers"), FALSE);

	OpRect header_rect = root_widget->GetOpWidget()->GetScreenRect();
	
	OpRect screen_rect = GetScreenRect();
	OpPoint p = menu_point;

	p.x += screen_rect.x;
	p.y += screen_rect.y;
	if (header_rect.Contains(p))
	{
		g_application->GetMenuHandler()->ShowPopupMenu("Mail Header Toolbar Popup", PopupPlacement::AnchorAt(p), 0, keyboard_invoked);
		return TRUE;
	}
	return FALSE;
}

BOOL MessageHeaderGroup::OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked)
{
	if (widget == m_quick_reply_edit)
	{
		return HandleWidgetContextMenu(widget, menu_point);
	}

	return FALSE;
}

OP_STATUS MessageHeaderGroup::AddIncompleteMessageToolbar(BOOL continuous_connection, BOOL scheduled_for_fetch)
{
	RETURN_IF_ERROR(OpInfobar::Construct(&m_incomplete_message_bar));

	m_optoolbar_group->GetOpWidget()->AddChild(m_incomplete_message_bar);

	RETURN_IF_ERROR(m_incomplete_message_bar->Init("Incomplete Message Toolbar"));
	m_incomplete_message_bar->GetBorderSkin()->SetImage("Incomplete Message Toolbar");
	m_incomplete_message_bar->SetWrapping(OpBar::WRAPPING_NEWLINE);

	OpString text;

	if (scheduled_for_fetch && !continuous_connection)
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_MAIL_WILL_BE_FETCHED_ON_NEXT_CHECK, text));
	else if (continuous_connection)
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_MAIL_INCOMPLETE_MESSAGE_FETCH, text));
	else
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_MAIL_INCOMPLETE_MESSAGE_FETCH_ON_NEXT_CHECK, text));

	return m_incomplete_message_bar->SetStatusText(text);
}

OP_STATUS MessageHeaderGroup::AddSuppressExternalEmbedsToolbar(OpString from, BOOL is_spam)
{
	if (m_suppress_external_embeds_bar)
	{
		return OpStatus::OK;
	}
	RETURN_IF_ERROR(OpInfobar::Construct(&m_suppress_external_embeds_bar));
	m_optoolbar_group->GetOpWidget()->AddChild(m_suppress_external_embeds_bar);

	RETURN_IF_ERROR(m_suppress_external_embeds_bar->Init("Suppress External Embeds Toolbar"));
	m_suppress_external_embeds_bar->GetBorderSkin()->SetImage("Suppress External Embeds Toolbar");
	m_suppress_external_embeds_bar->SetWrapping(OpBar::WRAPPING_NEWLINE);

	OpString text;
	g_languageManager->GetString(Str::S_MAIL_SUPPRESS_EXTERNAL_EMBEDS, text);
	RETURN_IF_ERROR(m_suppress_external_embeds_bar->SetStatusText(text));

	OpButton *checkbox =  static_cast<OpButton*>(m_suppress_external_embeds_bar->GetWidgetByNameInSubtree("tb_checkbox_for_all"));
	if (checkbox)
	{
		if (is_spam)
		{
			checkbox->SetEnabled(FALSE);
		}

		OpString tmp;
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_MAIL_APPLY_FOR_ALL_MESSAGES_FROM, tmp));
		text.Empty();
		RETURN_IF_ERROR(text.AppendFormat(tmp.CStr(), from.CStr()));
		RETURN_IF_ERROR(checkbox->SetText(text));
		checkbox->SetEllipsis(ELLIPSIS_END);
	}
	m_suppress_external_embeds_bar->SetVisibility(TRUE);
	return OpStatus::OK;
}

BOOL MessageHeaderGroup::SendQuickReply()
{
	if (!m_quick_reply_edit || m_quick_reply_edit->GetTextLength(FALSE) == 0)
	{
		return TRUE;
	}

	Message new_message, *old_message = m_mail_window->GetCurrentMessage();

	if (!old_message)
		return TRUE;

	RETURN_VALUE_IF_ERROR(g_m2_engine->CreateMessage(new_message, m_mail_window->GetCurrentMessageAccountID(), m_mail_window->GetCurrentMessageID(), MessageTypes::QUICK_REPLY), TRUE);

	OpString body, new_text;

	RETURN_VALUE_IF_ERROR(new_message.GetRawBody(body), TRUE);
	RETURN_VALUE_IF_ERROR(m_quick_reply_edit->GetText(new_text, FALSE), TRUE);

	RETURN_VALUE_IF_ERROR(new_text.Append("\r\n\r\n"), TRUE);
	if (new_message.IsHTML())
	{
		int insert_point = body.FindI("<BODY>");
		if (insert_point == -1)
		{
			// Could handle this better, but ok for now
			return TRUE;
		}
		insert_point += 6;
		OpString html_new_text;
		RETURN_VALUE_IF_ERROR(html_new_text.Set(XHTMLify_string_with_BR(new_text.CStr(), new_text.Length(), FALSE, FALSE, TRUE, TRUE).CStr()), TRUE);
				
		RETURN_VALUE_IF_ERROR(body.Insert(insert_point,html_new_text.CStr()), TRUE);
	}
	else
	{
		RETURN_VALUE_IF_ERROR(body.Insert(0,new_text), TRUE);
	}

	RETURN_VALUE_IF_ERROR(new_message.SetRawBody(body, TRUE, TRUE, TRUE), TRUE);

	INT32 selected_item = m_widgets->Get<QuickDropDown>("mw_QuickReplyType")->GetOpWidget()->GetValue();
	selected_item = static_cast<DesktopOpDropDown*>(m_widgets->Get<QuickDropDown>("mw_QuickReplyType")->GetOpWidget())->GetItemUserData(selected_item);
	RETURN_VALUE_IF_ERROR(new_message.SetPreferredRecipients(static_cast<MessageTypes::CreateType>(selected_item), *old_message), TRUE);

	RETURN_VALUE_IF_ERROR(g_m2_engine->SendMessage(new_message, FALSE), TRUE);
	RETURN_VALUE_IF_ERROR(g_m2_engine->MessageReplied(m_mail_window->GetCurrentMessageID(), TRUE), TRUE);
	
	m_quick_reply_edit->Clear();
	OpString* reply = NULL;
	if (OpStatus::IsSuccess(m_quick_replies.Remove(m_m2_id, &reply)))
		OP_DELETE(reply);

	OpString ghost_text;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_QUICK_REPLY_GHOST_TEXT, ghost_text));
	RETURN_IF_ERROR(m_quick_reply_edit->SetGhostText(ghost_text.CStr()));

	Relayout();
	
	OpInputAction focus_message_body(OpInputAction::ACTION_FOCUS_PAGE);
	return m_mail_window->OnInputAction(&focus_message_body);
}

BOOL MessageHeaderGroup::OnMouseWheel(INT32 delta, BOOL vertical) 
{ 
	if (vertical)
	{
		if (delta > 0)
			g_input_manager->InvokeAction(OpInputAction::ACTION_SCROLL_DOWN, delta);
		else
			g_input_manager->InvokeAction(OpInputAction::ACTION_SCROLL_UP, -delta);
	}
	return TRUE;
}

BOOL MessageHeaderGroup::OnInputAction(OpInputAction* action)
{
	if (!m_widgets)
		return FALSE;

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_SAVE_ATTACHMENT_LIST_TO_FOLDER:
				{
					child_action->SetEnabled(m_attachments.GetCount() != 0 );
					return TRUE;
				}
				case OpInputAction::ACTION_COPY:
				case OpInputAction::ACTION_VIEW_MESSAGES_FROM_CONTACT:
				case OpInputAction::ACTION_EDIT_PROPERTIES:
				{
					child_action->SetEnabled(TRUE);
					return TRUE;
				}

				case OpInputAction::ACTION_ADD_CONTACT:
				{
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
				case OpInputAction::ACTION_CUSTOMIZE_TOOLBARS:
				{
					OpInputContext* header_field;
					OP_STATUS error;
					TRAP_AND_RETURN_VALUE_IF_ERROR(error,   header_field = m_widgets->GetL<QuickSkinElement>("Mail Window Toolbar And Headers")->GetOpWidget(), FALSE);
					if (m_mail_window->GetCurrentMessage()->IsFlagSet(StoreMessage::IS_NEWSFEED_MESSAGE)
						&& (action->GetFirstInputContext()->IsChildInputContextOf(header_field) || action->GetFirstInputContext() == header_field))
					{
						// only disable this action for feeds and in the header field
						child_action->SetEnabled(FALSE);
						return TRUE;
					}
					return FALSE;
				}
				case OpInputAction::ACTION_SHOW_POPUP_MENU:
				{
					if (child_action->HasActionDataString() && 
					   (uni_strcmp(child_action->GetActionDataString(), UNI_L("Mail Address Popup Menu")) == 0 ||
						uni_strcmp(child_action->GetActionDataString(), UNI_L("Mail Contact Popup Menu")) == 0 ||
						uni_strcmp(child_action->GetActionDataString(), UNI_L("Mail Attachment Toolbar Popup")) == 0))
					{
						child_action->SetEnabled(TRUE);
						return TRUE;
					}
					return FALSE;
				}
				case OpInputAction::ACTION_OPEN_EXPAND:
				{
					if (!child_action->HasActionDataString())
						return FALSE;
					OpString8 layout;
					RETURN_VALUE_IF_ERROR(layout.Set(child_action->GetActionDataString()), TRUE);
					if (!m_widgets->Contains<QuickWrapLayout>(layout))
						return FALSE;
					child_action->SetEnabled(m_widgets->Get<QuickWrapLayout>(layout)->HasHiddenItems());
					return TRUE;
				}
				case OpInputAction::ACTION_CLOSE_EXPAND:
				{
					if (!child_action->HasActionDataString())
						return TRUE;
					OpString8 layout;
					RETURN_VALUE_IF_ERROR(layout.Set(child_action->GetActionDataString()), TRUE);
					if(!m_widgets->Contains<QuickWrapLayout>(layout))
						return FALSE;
					child_action->SetEnabled(m_widgets->Get<QuickWrapLayout>(layout)->HasHiddenItems() == FALSE);
					return TRUE;
				}
				case OpInputAction::ACTION_FLAG_MESSAGE:
				{
					child_action->SetEnabled(child_action->GetActionData() != MessageEngine::GetInstance()->GetStore()->GetMessageFlag(m_m2_id, StoreMessage::IS_FLAGGED));
					return TRUE;
				}
				case OpInputAction::ACTION_SEND_MAIL:
				case OpInputAction::ACTION_QUICK_REPLY:
				{
					child_action->SetEnabled(m_quick_reply_edit && m_quick_reply_edit->GetTextLength(FALSE) != 0);
					return TRUE;
				}
				case OpInputAction::ACTION_EXPAND_MESSAGE:
				{
					child_action->SetSelected(m_minimum_mail_width == 10000);
					return TRUE;
				}

				case OpInputAction::ACTION_WATCH_INDEX:
				case OpInputAction::ACTION_WATCH_CONTACT:
				{
					Index* index = NULL;
					if (child_action->GetAction() == OpInputAction::ACTION_WATCH_INDEX)
					{
						index = g_m2_engine->GetIndexer()->GetThreadIndex(m_m2_id);
					}
					else
					{
						index = GetSelectedContactIndex(FALSE);
					}

					if (index && !index->IsWatched())
					{
						child_action->SetEnabled(!index->IsWatched() && !index->IsIgnored());
						child_action->SetSelected(FALSE);
					}
					else
					{
						child_action->SetEnabled(TRUE);
					}
					return TRUE;
				}

				case OpInputAction::ACTION_STOP_WATCH_INDEX:
				case OpInputAction::ACTION_STOP_WATCH_CONTACT:
				{
					Index* index = NULL;
					if (child_action->GetAction() == OpInputAction::ACTION_STOP_WATCH_INDEX)
					{
						index = g_m2_engine->GetIndexer()->GetThreadIndex(m_m2_id);
					}
					else
					{
						index = GetSelectedContactIndex(FALSE);
					}
					BOOL enabled = FALSE;
					if (index)
					{
						enabled = index->IsWatched() && !index->IsIgnored();
						child_action->SetSelected(index->IsWatched());
					}
					else
					{
						child_action->SetSelected(FALSE);
					}
					child_action->SetEnabled(enabled);
					return TRUE;
				}

				case OpInputAction::ACTION_IGNORE_INDEX:
				case OpInputAction::ACTION_IGNORE_CONTACT:
				{
					Index* index = NULL;
					if (child_action->GetAction() == OpInputAction::ACTION_IGNORE_INDEX)
					{
						index = g_m2_engine->GetIndexer()->GetThreadIndex(m_m2_id);
					}
					else
					{
						index = GetSelectedContactIndex(FALSE);
					}
					if (index)
					{
						child_action->SetEnabled(!index->IsIgnored() && !index->IsWatched());
						child_action->SetSelected(FALSE);
					}
					else
					{
						child_action->SetEnabled(TRUE);
					}
					return TRUE;
				}

				case OpInputAction::ACTION_STOP_IGNORE_INDEX:
				case OpInputAction::ACTION_STOP_IGNORE_CONTACT:
				{
					Index* index = NULL;
					if (child_action->GetAction() == OpInputAction::ACTION_STOP_IGNORE_INDEX)
					{
						index = g_m2_engine->GetIndexer()->GetThreadIndex(m_m2_id);
					}
					else
					{
						index = GetSelectedContactIndex(FALSE);
					}
					BOOL enabled = FALSE;
					if (index)
					{
						enabled = index->IsIgnored() && !index->IsWatched();
						child_action->SetSelected(index->IsIgnored());
					}
					child_action->SetEnabled(enabled);
					return TRUE;
				}
				case OpInputAction::ACTION_DISPLAY_EXTERNAL_EMBEDS:
				{
					if (m_selected_email.IsEmpty())
						return FALSE;
					index_gid_t index_id = g_m2_engine->GetIndexIDByAddress(m_selected_email);
					Index* index = g_m2_engine->GetIndexById(index_id);
					if (child_action->GetActionData() == 0)
					{
						bool enabled = index && index->GetIndexFlag(IndexTypes::INDEX_FLAGS_ALLOW_EXTERNAL_EMBEDS) == true;
						child_action->SetSelected(enabled);
						child_action->SetEnabled(enabled || child_action->GetActionOperator() == OpInputAction::OPERATOR_NONE);
					}
					return TRUE;
				}
			}
			break;
		}
		case OpInputAction::ACTION_EXPAND_MESSAGE:
		{
			PrefsCollectionM2::integerpref pref;
			if (g_pcm2->GetIntegerPref(PrefsCollectionM2::MailViewMode) == 1 && !m_mail_window->IsMessageOnlyView()) 
				pref = PrefsCollectionM2::MessageWidthListOnRight;
			else
				pref = PrefsCollectionM2::MessageWidthListOnTop;
	
			if (g_pcm2->GetIntegerPref(pref) != g_pcm2->GetDefaultIntegerPref(pref))
			{
				TRAPD(err, g_pcm2->ResetIntegerL(pref));
			}
			else
			{
				TRAPD(err, g_pcm2->WriteIntegerL(pref, g_pcm2->GetDefaultIntegerPref(pref) ? 0 : 105));
			}
			return TRUE;
		}
		case OpInputAction::ACTION_SHOW_POPUP_MENU:
		case OpInputAction::ACTION_SHOW_HIDDEN_POPUP_MENU:
		{
			m_url = NULL;
			m_selected_id = -1;
			m_selected_email.Empty();
			m_selected_name.Empty();

			if (!action->HasActionDataString())
				return FALSE;

			if(uni_strcmp(action->GetActionDataString(), UNI_L("Mail Address Popup Menu")) == 0 ||
			   uni_strcmp(action->GetActionDataString(), UNI_L("Mail Contact Popup Menu")) == 0 || 
			   uni_strcmp(action->GetActionDataString(), UNI_L("From Address Popup Menu")) == 0 ||
			   uni_strcmp(action->GetActionDataString(), UNI_L("From Contact Popup Menu")) == 0)
			{
				MailAddressActionData *data = (MailAddressActionData *)action->GetActionData();
				if(data)
				{
					m_selected_id = data->m_id;
					m_selected_email.Set(data->m_email);
					m_selected_name.Set(data->m_name);
				}
			}
			else if (uni_strcmp(action->GetActionDataString(), UNI_L("Mail Attachment Toolbar Popup")) == 0)
			{
				DesktopMenuContext * menu_context  = NULL;
				TransferManagerDownloadCallback * t_file_download_callback = NULL;
				menu_context = OP_NEW(DesktopMenuContext, ());
				if (menu_context)
				{
					URL *url = (URL *)action->GetActionData();
					m_url = url;
					t_file_download_callback = OP_NEW(TransferManagerDownloadCallback, (NULL, *url, NULL, ViewActionFlag()));
					if (t_file_download_callback)
					{
						menu_context->Init(t_file_download_callback);


						OpRect rect;
						OpPoint point;

						InvokeContextMenuAction(point, menu_context, action->GetActionDataString(), rect);
					}
					else
						OP_DELETE(menu_context);
				}
				return TRUE;
			}
			return FALSE;
		}
		case OpInputAction::ACTION_SEND_MAIL:
		case OpInputAction::ACTION_QUICK_REPLY:
		{
			return SendQuickReply();
		}
		case OpInputAction::ACTION_VIEW_MESSAGES_FROM_CONTACT:
		{
			HotlistManager::ItemData item_data;
			g_hotlist_manager->GetItemValue(m_selected_id, item_data );
			if( !item_data.mail.IsEmpty() )
			{
				g_application->GoToMailView(0, item_data.mail.CStr(), NULL, TRUE, FALSE, TRUE, action->IsKeyboardInvoked());
			}
			return TRUE;
		}

		case OpInputAction::ACTION_EDIT_PROPERTIES:
		{
			if(m_selected_id > 0)
			{
				g_hotlist_manager->EditItem(m_selected_id, GetParentDesktopWindow());
			}
			return TRUE;
		}
		case OpInputAction::ACTION_FOCUS_QUICK_REPLY:
		{
			if (m_quick_reply_edit)
				m_quick_reply_edit->SetFocus(FOCUS_REASON_KEYBOARD);
			return TRUE;
		}
		case OpInputAction::ACTION_ADD_CONTACT:
		{
			if(m_selected_id > 0)
			{
				g_hotlist_manager->EditItem(m_selected_id, GetParentDesktopWindow());
				return TRUE;
			}
			
			HotlistManager::ItemData item_data;
			if(m_selected_email.HasContent())
			{
				// Address dropdown in the header pane
				item_data.mail.Set(m_selected_email);
				item_data.name.Set(m_selected_name);
			}
			else if (m_mail_window->GetCurrentMessage())
			{
				// context menu for message
				if (!m_mail_window->GetCurrentMessage())
					return TRUE;
				Header* from_header = m_mail_window->GetCurrentMessage()->GetHeader(m_mail_window->GetCurrentMessage()->IsFlagSet(Message::IS_RESENT) ? Header::RESENTFROM : Header::FROM);
				if (from_header)
				{
					const Header::From* from_address = from_header->GetFirstAddress();
					RETURN_VALUE_IF_ERROR(item_data.mail.Set(from_address->GetAddress()), TRUE);
					RETURN_VALUE_IF_ERROR(item_data.name.Set(from_address->GetName()), TRUE);
				}
			}

			g_hotlist_manager->SetupPictureUrl(item_data);
			g_hotlist_manager->NewContact(item_data, HotlistModel::ContactRoot, GetParentDesktopWindow(), TRUE);
			return TRUE;
		}

		case OpInputAction::ACTION_COMPOSE_MAIL:

		{
			if (m_selected_email.HasContent() && action->GetActionMethod() == OpInputAction::METHOD_MENU)
			{
				OpString formatted_address;
				BOOL needs_comma = FALSE;

				if (m_selected_name.IsEmpty() || OpStatus::IsError(g_hotlist_manager->AppendRFC822FormattedItem(m_selected_name, m_selected_email, formatted_address, needs_comma)))
					RETURN_VALUE_IF_ERROR(formatted_address.Set(m_selected_email), TRUE);

				MailTo mailto;
				mailto.Init(formatted_address, NULL, NULL, NULL, NULL);
				MailCompose::ComposeMail(mailto);

				return TRUE;
			}
			return FALSE;
		}

		case OpInputAction::ACTION_COPY:
		{
			if(m_selected_email.HasContent() && action->GetActionMethod() == OpInputAction::METHOD_MENU)
			{
				OpString name;
				if (m_selected_name.HasContent() && OpStatus::IsSuccess(name.AppendFormat("%s <%s>", m_selected_name.CStr(), m_selected_email.CStr())))
					g_desktop_clipboard_manager->PlaceText(name.CStr(), g_application->GetClipboardToken());	
				else
					g_desktop_clipboard_manager->PlaceText(m_selected_email.CStr(), g_application->GetClipboardToken());
			}
			return TRUE;
		}

		case OpInputAction::ACTION_SAVE:
		{
			if (action->GetActionMethod() != OpInputAction::METHOD_MENU)
				return FALSE;

			if (!m_url)
				return TRUE;

			if (!m_chooser)
				RETURN_VALUE_IF_ERROR(DesktopFileChooser::Create(&m_chooser), TRUE);

			OpString fname;
			TRAPD(err, m_url->GetAttributeL(URL::KSuggestedFileName_L, fname));
			RETURN_VALUE_IF_ERROR(err, TRUE);
			m_selected_attachment_url = *m_url;

			OpString tmp_storage;
			OpFileFolder initial_folder_id = (action->GetReferrerAction() == OpInputAction::ACTION_SAVE_ATTACHMENT_TO_DOWNLOAD_FOLDER)
				? OPFILE_DOWNLOAD_FOLDER : OPFILE_SAVE_FOLDER;
			m_request.initial_path.Set(g_folder_manager->GetFolderPathIgnoreErrors(initial_folder_id, tmp_storage));
			g_languageManager->GetString(Str::S_SAVE_AS_CAPTION, m_request.caption);
			m_request.initial_path.Append(fname);
			m_request.action = DesktopFileChooserRequest::ACTION_FILE_SAVE_PROMPT_OVERWRITE;

			OpStatus::Ignore(m_chooser->Execute(GetParentOpWindow(), this, m_request));

			return TRUE;
		}

		case OpInputAction::ACTION_SAVE_ATTACHMENT_TO_DOWNLOAD_FOLDER:
		{
			if (!m_url)
				return TRUE;

			OpString fname;
			TRAPD(err, m_url->GetAttributeL(URL::KSuggestedFileName_L, fname, TRUE));

			OpFile file;
			file.Construct(fname.CStr(), OPFILE_DOWNLOAD_FOLDER);

			BOOL exists;
			file.Exists(exists);
			if (exists && (SimpleDialog::ShowDialog(WINDOW_NAME_YES_NO, GetParentDesktopWindow(), 
					Str::S_FILE_SAVE_PROMPT_OVERWRITE, Str::S_OPERA,
					Dialog::TYPE_YES_NO, Dialog::IMAGE_WARNING) == Dialog::DIALOG_RESULT_NO))
			{
				// open the normal "save as" dialog
				OpInputAction save_action(OpInputAction::ACTION_SAVE);
				save_action.SetActionDataString(action->GetActionDataString());
				save_action.SetActionMethod(action->GetActionMethod());
				save_action.SetReferrerAction(OpInputAction::ACTION_SAVE_ATTACHMENT_TO_DOWNLOAD_FOLDER);
				save_action.SetFirstInputContext(GetParentDesktopWindow());
				return OnInputAction(&save_action);
			}

			m_url->SaveAsFile(file.GetFullPath());

			return TRUE;
		}

		case OpInputAction::ACTION_OPEN:
		{
			if (action->GetActionMethod() != OpInputAction::METHOD_MENU)
				return FALSE;

			return AttachmentOpener::OpenAttachment(m_url, m_chooser, GetParentDesktopWindow());
		}

		case OpInputAction::ACTION_CUSTOMIZE_TOOLBARS:
		{
			OpInputContext* header_field;
			OP_STATUS err;
			TRAP_AND_RETURN_VALUE_IF_ERROR(err, header_field = m_widgets->GetL<QuickSkinElement>("Mail Window Toolbar And Headers")->GetOpWidget(), FALSE);
			
			if (!action->GetFirstInputContext()->IsChildInputContextOf(header_field) && action->GetFirstInputContext() != header_field)
				return FALSE;

			CustomizeHeaderDialog* dialog = OP_NEW(CustomizeHeaderDialog, ());
			if (dialog)
				dialog->Init(g_application->GetActiveDesktopWindow());
			return TRUE;
		}
		case OpInputAction::ACTION_OPEN_EXPAND:
		{
			if (!action->HasActionDataString())
				return FALSE;
			OpString8 layout;
			RETURN_VALUE_IF_ERROR(layout.Set(action->GetActionDataString()), TRUE);
			m_widgets->Get<QuickWrapLayout>(layout)->SetMaxVisibleLines(0);
			return TRUE;
		}
		case OpInputAction::ACTION_CLOSE_EXPAND:
		{
			if (!action->HasActionDataString())
				return FALSE;
			OpString8 layout;
			RETURN_VALUE_IF_ERROR(layout.Set(action->GetActionDataString()), TRUE);
			m_widgets->Get<QuickWrapLayout>(layout)->SetMaxVisibleLines(1);
			return TRUE;
		}
		case OpInputAction::ACTION_FLAG_MESSAGE:
		{
			g_m2_engine->MessageFlagged(m_m2_id, !MessageEngine::GetInstance()->GetStore()->GetMessageFlag(m_m2_id, StoreMessage::IS_FLAGGED));
			return TRUE;
		}

		case OpInputAction::ACTION_DISPLAY_EXTERNAL_EMBEDS:
		{
			// perhaps handle in the window?
			if (m_selected_email.IsEmpty() || action->GetActionMethod() != OpInputAction::METHOD_MENU)
				return FALSE;

			index_gid_t index_id = g_m2_engine->GetIndexIDByAddress(m_selected_email);
			Index* index = g_m2_engine->GetIndexById(index_id);
			if (index)
			{
				index->SetIndexFlag(IndexTypes::INDEX_FLAGS_ALLOW_EXTERNAL_EMBEDS, action->GetActionData() == 1);
				static_cast<MailDesktopWindow*>(GetParentDesktopWindow())->ShowSelectedMail(FALSE, TRUE);
			}
			return TRUE;
		}

		case OpInputAction::ACTION_WATCH_INDEX:
		{
			Index* index = g_m2_engine->GetIndexer()->GetThreadIndex(m_m2_id);
			if (!index)
			{
				OpINTSet thread_ids;
				g_m2_engine->GetStore()->GetThreadIds(m_m2_id, thread_ids);
				g_m2_engine->GetIndexer()->CreateThreadIndex(thread_ids, index);
			}

			if (index)
			{
				index->ToggleWatched(TRUE);
				UpdateMessages(index);
			}
			return TRUE;
		}
		case OpInputAction::ACTION_WATCH_CONTACT:
		{
			Index* index = GetSelectedContactIndex(TRUE);

			if (index)
			{
				index->ToggleWatched(TRUE);
				UpdateContactIcon();
				UpdateMessages(index);
			}

			return TRUE;
		}

		case OpInputAction::ACTION_STOP_WATCH_INDEX:
		{
			Index* index = g_m2_engine->GetIndexer()->GetThreadIndex(m_m2_id);

			if (index)
			{
				index->ToggleWatched(FALSE);
				UpdateMessages(index);
			}
			return TRUE;
		}
		case OpInputAction::ACTION_STOP_WATCH_CONTACT:
		{
			Index* index = GetSelectedContactIndex(TRUE);

			if (index)
			{
				index->ToggleWatched(FALSE);
				UpdateContactIcon();
				UpdateMessages(index);
			}
			return TRUE;
		}

		case OpInputAction::ACTION_IGNORE_INDEX:
		{	
			Index* index = g_m2_engine->GetIndexer()->GetThreadIndex(m_m2_id);

			if (!index)
			{
				OpINTSet thread_ids;
				g_m2_engine->GetStore()->GetThreadIds(m_m2_id, thread_ids);
				g_m2_engine->GetIndexer()->CreateThreadIndex(thread_ids, index);
			}

			if (index)
			{
				index->ToggleIgnored(TRUE);
				g_m2_engine->IndexRead(index->GetId());
				UpdateMessages(index, TRUE);
			}
			return TRUE;
		}
		case OpInputAction::ACTION_IGNORE_CONTACT:
		{

			Index* index = GetSelectedContactIndex(TRUE);

			if (index)
			{
				index->ToggleIgnored(TRUE);
				g_m2_engine->IndexRead(index->GetId());
				UpdateContactIcon();
				UpdateMessages(index);
			}
			return TRUE;
		}

		case OpInputAction::ACTION_STOP_IGNORE_INDEX:
		{
			Index* index = g_m2_engine->GetIndexer()->GetThreadIndex(m_m2_id);

			if (index)
			{
				index->ToggleIgnored(FALSE);
				UpdateMessages(index);
			}
			return TRUE;
		}
		case OpInputAction::ACTION_STOP_IGNORE_CONTACT:
		{
			Index* index = GetSelectedContactIndex(TRUE);

			if (index)
			{
				index->ToggleIgnored(FALSE);
				UpdateContactIcon();
				UpdateMessages(index);
			}
			return TRUE;
		}
		case OpInputAction::ACTION_SAVE_ATTACHMENT_LIST_TO_FOLDER:
		{
			SaveAttachmentsDialog* dialog = OP_NEW(SaveAttachmentsDialog, (&m_attachments));
			if(dialog)
				dialog->Init(g_application->GetActiveDesktopWindow(FALSE));
			return TRUE;
		}
#ifdef _PRINT_SUPPORT_
		case OpInputAction::ACTION_LEAVE_PRINT_PREVIEW:
		case OpInputAction::ACTION_PRINT_PREVIEW:
		{
			WindowCommanderProxy::TogglePrintMode(m_mail_view->GetWindowCommander());
			if (WindowCommanderProxy::GetPreviewMode(m_mail_view->GetWindowCommander()))
			{
				m_widgets->Get<QuickWidget>("Mail Window Toolbar And Headers")->Hide();
				if (m_widgets->Contains<QuickSkinElement>("mw_QuickReplySection"))
					m_widgets->Get<QuickSkinElement>("mw_QuickReplySection")->Hide();
			}
			else
			{
				m_widgets->Get<QuickWidget>("Mail Window Toolbar And Headers")->Show();
				if (g_pcm2->GetIntegerPref(PrefsCollectionM2::ShowQuickReply) && m_widgets->Contains<QuickSkinElement>("mw_QuickReplySection"))
					m_widgets->Get<QuickSkinElement>("mw_QuickReplySection")->Show();
			}
			UpdateMessageHeight();
			return TRUE;
		}
#endif // _PRINT_SUPPORT_
	}

	if (m_mail_view && m_mail_view->OnInputAction(action))
		return TRUE;

	return FALSE;
}

void MessageHeaderGroup::OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
{
	if (result.files.GetCount())
	{
		OpString* selected_file_name = result.files.Get(0);
		m_selected_attachment_url.SaveAsFile(*selected_file_name);
		if (result.active_directory.HasContent())
		{
			TRAPD(err, g_pcfiles->WriteDirectoryL(OPFILE_SAVE_FOLDER, result.active_directory.CStr()));
		}
	}
	m_selected_attachment_url = URL();
}

void MessageHeaderGroup::OnLoadingFinished(OpWindowCommander* commander, LoadingFinishStatus status) 
{ 
	UpdateMessageHeight();
	if (m_resize_message_timer)
		m_resize_message_timer->Stop();
}

void MessageHeaderGroup::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget == m_quick_reply_edit && m_widgets->Contains<QuickButton>("mw_QuickReplyButton"))
	{
		m_widgets->Get<QuickButton>("mw_QuickReplyButton")->GetOpWidget()->SetUpdateNeeded(TRUE);
		m_widgets->Get<QuickButton>("mw_QuickReplyButton")->GetOpWidget()->UpdateActionStateIfNeeded();
	}
}

void MessageHeaderGroup::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	if (m_header_and_message)
		m_header_and_message->SetParentOpWidget(0);
	RemoveAll();
	m_mail_window = NULL;
}

void MessageHeaderGroup::OnTimeOut(OpTimer* timer)
{
	if (timer == m_resize_message_timer)
		UpdateMessageHeight();
	if (m_mail_view->GetWindowCommander()->IsLoading())
		m_resize_message_timer->Start(500);
}

void MessageHeaderGroup::UpdateMessageHeight()
{
	if (!m_mail_view)
		return;

	unsigned doc_width, doc_height;
	m_mail_view->GetWindowCommander()->GetViewportController()->GetDocumentSize(&doc_width, &doc_height);
	doc_width = doc_width * m_zoom_scale / 100;
	doc_height = 50 + doc_height * m_zoom_scale / 100;
	
	if (doc_height > m_minimum_mail_height && doc_height != m_widgets->Get<QuickBrowserView>("mw_mail_view")->GetPreferredHeight())
	{
		m_widgets->Get<QuickBrowserView>("mw_mail_view")->SetPreferredHeight(doc_height);
	}

	if (doc_width > m_minimum_mail_width)
	{
		m_widgets->Get<QuickBrowserView>("mw_mail_view")->SetPreferredWidth(doc_width);
	}
	else if (m_minimum_mail_width != m_widgets->Get<QuickBrowserView>("mw_mail_view")->GetPreferredWidth())
	{
		m_widgets->Get<QuickBrowserView>("mw_mail_view")->SetPreferredWidth(m_minimum_mail_width);
	}
}

void MessageHeaderGroup::UpdateMessageWidth()
{
	int width;
	if (g_pcm2->GetIntegerPref(PrefsCollectionM2::MailViewMode) == MailLayout::LIST_ON_LEFT_MESSAGE_ON_RIGHT && !m_mail_window->IsMessageOnlyView()) 
		width = g_pcm2->GetIntegerPref(PrefsCollectionM2::MessageWidthListOnRight);
	else
		width = g_pcm2->GetIntegerPref(PrefsCollectionM2::MessageWidthListOnTop);
	
	if (width != 0)
	{
		FontAtt mailfont;
		g_pcfontscolors->GetFont(OP_SYSTEM_FONT_EMAIL_DISPLAY, mailfont);
		WIDGET_FONT_INFO info;
		info.size = mailfont.GetSize();
		info.weight = mailfont.GetWeight();
		info.font_info = styleManager->GetFontInfo(mailfont.GetFontNumber());
		m_minimum_mail_height = 200;
		m_minimum_mail_width = WidgetUtils::GetFontWidth(info) * width * m_zoom_scale / 100;
		m_widgets->Get<QuickBrowserView>("mw_mail_view")->SetPreferredHeight(m_minimum_mail_height);
		m_widgets->Get<QuickBrowserView>("mw_mail_view")->SetPreferredWidth(m_minimum_mail_width);
	}
	else
	{
		m_minimum_mail_height = 10000;
		m_minimum_mail_width = 10000;
		m_widgets->Get<QuickBrowserView>("mw_mail_view")->SetPreferredHeight(m_minimum_mail_height);
		m_widgets->Get<QuickBrowserView>("mw_mail_view")->SetPreferredWidth(m_minimum_mail_width);
	}
}

void MessageHeaderGroup::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
	if(pref == PrefsCollectionM2::ShowQuickReply)
	{
		if (newvalue)
			OpStatus::Ignore(ShowQuickReply());
		else if (m_widgets->Contains<QuickSkinElement>("mw_QuickReplySection"))
			m_widgets->Get<QuickSkinElement>("mw_QuickReplySection")->Hide();
	}
	else if (pref == PrefsCollectionM2::MessageWidthListOnRight || pref == PrefsCollectionM2::MessageWidthListOnTop)
	{
		UpdateMessageWidth();
		StartExpandTimer();
	}
	else if (pref == PrefsCollectionM2::MailViewMode)
	{
		SetCorrectToolbar();
	}
}

OP_STATUS MessageHeaderGroup::ShowQuickReply()
{
	if (!m_quick_reply_edit)
		return OpStatus::ERR;

	OpString translated_string, text_to_use;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_QUICK_REPLY_GHOST_TEXT, translated_string));
	RETURN_IF_ERROR(m_quick_reply_edit->SetGhostText(translated_string.CStr()));
	
	OpString* reply = NULL;
	if (OpStatus::IsSuccess(m_quick_replies.GetData(m_m2_id, &reply)) && reply)
		m_quick_reply_edit->SetText(reply->CStr());

	m_quick_reply_edit->SetScrollbarStatus(SCROLLBAR_STATUS_OFF);
	m_quick_reply_edit->SetListener(this);

	OpDropDown* dropdown = static_cast<OpDropDown*>(m_widgets->Get<QuickDropDown>("mw_QuickReplyType")->GetOpWidget());
	const Message* message = m_mail_window->GetCurrentMessage();
	
	INT32 got_index = -1;
	Header::HeaderValue from, list, replyto;

	dropdown->Clear();
	dropdown->SetEllipsis(ELLIPSIS_END);

	RETURN_IF_ERROR(message->GetHeaderValue(Header::FROM, from, TRUE));
	if (from.HasContent())
	{
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_MAIL_QUICKREPLY_TO_SENDER, translated_string));
		RETURN_IF_ERROR(text_to_use.AppendFormat(translated_string.CStr(), from.CStr()));
		RETURN_IF_ERROR(dropdown->AddItem(text_to_use.CStr(), -1, &got_index, MessageTypes::REPLY_TO_SENDER));
	}

	if ( message->GetHeader(Header::CC) ||
	    (message->GetHeader(Header::TO) && message->GetHeader(Header::TO)->GetFirstAddress() &&
		 message->GetHeader(Header::TO)->GetFirstAddress()->Suc()))
	{
		text_to_use.Empty();
		RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_MAILVIEWTOOLBAR_REPLYALL, text_to_use));
		RETURN_IF_ERROR(dropdown->AddItem(text_to_use.CStr(), -1, &got_index, MessageTypes::REPLYALL));
		dropdown->SelectItem(got_index, TRUE);
	}

	if (OpStatus::IsSuccess(Message::GetMailingListHeader(*message, list)) && list.Find(from) == KNotFound)
	{
		text_to_use.Empty();
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_MAIL_QUICKREPLY_TO_LIST, translated_string));
		RETURN_IF_ERROR(text_to_use.AppendFormat(translated_string.CStr(), list.CStr()));
		RETURN_IF_ERROR(dropdown->AddItem(text_to_use.CStr(), -1, &got_index, MessageTypes::REPLY_TO_LIST));
		dropdown->SelectItem(got_index, TRUE);
	}

	if (OpStatus::IsSuccess(message->GetHeaderValue(Header::REPLYTO, replyto, TRUE)) && replyto.HasContent()
		&& replyto.Find(from) == KNotFound && replyto.Find(list) == KNotFound)
	{
		text_to_use.Empty();
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_MAIL_QUICKREPLY_TO_REPLY_TO_HEADER, translated_string));
		RETURN_IF_ERROR(text_to_use.AppendFormat(translated_string.CStr(), replyto.CStr()));
		RETURN_IF_ERROR(dropdown->AddItem(text_to_use.CStr(), -1, &got_index, MessageTypes::QUICK_REPLY));
		dropdown->SelectItem(got_index, TRUE);
	}

	m_widgets->Get<QuickSkinElement>("mw_QuickReplySection")->Show();
	return OpStatus::OK;
}

OP_STATUS MessageHeaderGroup::StartExpandTimer()
{
	if (!m_resize_message_timer)
	{
		m_resize_message_timer = OP_NEW(OpTimer, ());
		RETURN_OOM_IF_NULL(m_resize_message_timer);
		m_resize_message_timer->SetTimerListener(this);
	}

	m_resize_message_timer->Start(200);
	return OpStatus::OK;
}

Index* MessageHeaderGroup::GetSelectedContactIndex(BOOL create_if_null)
{
	OpString email;
	email.Set(m_selected_email);
	if (m_selected_id < 0 && m_selected_email.IsEmpty())
	{
		// this happens when a contact action is invoked from the context menu in the message
		if (!m_mail_window->GetCurrentMessage())
			return NULL;
		Header* from_header = m_mail_window->GetCurrentMessage()->GetHeader(m_mail_window->GetCurrentMessage()->IsFlagSet(Message::IS_RESENT) ? Header::RESENTFROM : Header::FROM);
		if (!from_header)
			return NULL;
		const Header::From* from_address = from_header->GetFirstAddress();
		if (!from_address)
			return NULL;
		email.Set(from_address->GetAddress());
		if (create_if_null)
			g_m2_engine->GetGlueFactory()->GetBrowserUtils()->AddContact(email, from_address->GetName());

		HotlistModelItem* item = g_hotlist_manager->GetContactsModel()->GetByEmailAddress(email);
		if (item)
			m_selected_id = item->GetID();
	}
	else if (m_selected_id < 0 && m_selected_email.HasContent())
	{
		if (create_if_null)
			g_m2_engine->GetGlueFactory()->GetBrowserUtils()->AddContact(m_selected_email, m_selected_name);

		HotlistModelItem* item = g_hotlist_manager->GetContactsModel()->GetByEmailAddress(m_selected_email);
		if (item)
			m_selected_id = item->GetID();
	}


	if (m_selected_id < 0)
		return g_m2_engine->GetIndexer()->GetCombinedContactIndex(email);

	ContactModelItem* item = static_cast<ContactModelItem*>(g_hotlist_manager->GetContactsModel()->GetItemByID(m_selected_id));
	if (!item)
		return NULL;

	index_gid_t index_id = item->GetM2IndexId(create_if_null);
	return g_m2_engine->GetIndexById(index_id);
}

void MessageHeaderGroup::UpdateContactIcon()
{
	if (m_selected_id < 0)
		return;

	ContactModelItem * hotlist_item = static_cast<ContactModelItem*>(g_hotlist_manager->GetContactsModel()->GetItemByID(m_selected_id));
	if (hotlist_item)
	{
		hotlist_item->ChangeImageIfNeeded();
		g_hotlist_manager->GetContactsModel()->SetDirty(TRUE);
	}
}

void MessageHeaderGroup::UpdateMessages(Index* index, BOOL collapse_thread)
{
	IndexModel* index_model = static_cast<IndexModel*>(m_mail_window->GetIndexView()->GetTreeModel());

	for (INT32SetIterator it(index->GetIterator()); it; it++)
	{
		message_gid_t message_id = it.GetData();
		if (collapse_thread)
		{
			if (index_model->GetPosition(message_id) > 0)
			{
				m_mail_window->GetIndexView()->OpenItem(m_mail_window->GetIndexView()->GetItemByID(message_id), FALSE);
				index_model->GetItemByIndex(index_model->GetPosition(message_id))->Change(FALSE);
				m_mail_window->GetIndexView()->SetSelectedItemByID(message_id);
				return;
			}
		}
		else if (index_model->GetPosition(message_id) != -1)
		{
			index_model->GetItemByIndex(index_model->GetPosition(message_id))->Change(FALSE);
		}
	}
}
#endif // M2_SUPPORT
