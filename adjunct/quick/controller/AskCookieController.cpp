/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author: Blazej Kazmierczak (bkazmierczak)
 *
 */
#include "core/pch.h"

#include "adjunct/quick/controller/AskCookieController.h"

#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickExpand.h"
#include "adjunct/quick_toolkit/widgets/QuickMultilineLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickLabel.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/dialogs/CookieWarningDialog.h"
#include "adjunct/quick/managers/DesktopCookieManager.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/OpLocale.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

OpProperty<bool> AskCookieController::s_is_details_shown;
OpProperty<bool> AskCookieController::s_is_saveall_server;
OpProperty<bool> AskCookieController::s_is_saveonly_session;

AskCookieController::AskCookieController(OpCookieListener::AskCookieContext *context) :
					m_widgets(NULL),
					m_cookie_context(context),
					m_details_expand(NULL)
{
}

void AskCookieController::InitL()
{
	if (!m_cookie_context)
		LEAVE(OpStatus::ERR_NULL_POINTER);

	LEAVE_IF_ERROR(SetDialog("Ask Cookie Dialog"));
	m_widgets = m_dialog->GetWidgetCollection();

	LEAVE_IF_ERROR(InitControls());
}
 
OP_STATUS AskCookieController::InitControls()
{
	OpString uni_text; // used to convert from char* to uni_char* (enable unicode char handling)

	RETURN_IF_ERROR(SetWidgetText<QuickLabel>("name_value",m_cookie_context->GetName())); // set name
	RETURN_IF_ERROR(SetWidgetText<QuickLabel>("value_value",m_cookie_context->GetValue())); // set value (allow UTF-8 here)
	RETURN_IF_ERROR(SetWidgetText<QuickLabel>("domain_value",m_cookie_context->GetDomain()));
	RETURN_IF_ERROR(SetWidgetText<QuickLabel>("domain_heading",m_cookie_context->GetDomain()));
	RETURN_IF_ERROR(SetWidgetText<QuickLabel>("server_heading",m_cookie_context->GetServerName()));
	
	// set path (set "/" as default)
	if (m_cookie_context->GetPath())
	{
		RETURN_IF_ERROR(uni_text.SetFromUTF8(m_cookie_context->GetPath()));
		if (uni_text.IsEmpty())
			RETURN_IF_ERROR(uni_text.Set("/"));
		RETURN_IF_ERROR(SetWidgetText<QuickLabel>("path_value",uni_text));
	}

	if (IsSessionCookie()) // set "at the end of session" text instead of a date
	{
		OpString expirestext;
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_COOKIE_EXPIRES_SESSIONEND, expirestext));
		RETURN_IF_ERROR(SetWidgetText<QuickLabel>("expires_value",expirestext));
	}
	else // set expiration time
	{
		uni_char buf[200];
		time_t time = m_cookie_context->GetExpire();
		g_oplocale->op_strftime(buf, 199, UNI_L("%Y-%m-%d %H:%M:%S"), localtime(&time));
		RETURN_IF_ERROR(SetWidgetText<QuickLabel>("expires_value",buf));
	}

	// don't enable "save only for session" checkbox for session cookies
	QuickWidget* name = m_widgets->Get<QuickWidget>("session_only_checkbox");
	if(!name)
		return OpStatus::ERR;
	name->SetEnabled(!IsSessionCookie());

	// set port info
	OpString portlabel_text;
	if (m_cookie_context->GetPortCount() == 1)
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_COOKIE_PORT, portlabel_text));
	else
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_COOKIE_PORTS, portlabel_text));

	RETURN_IF_ERROR(SetWidgetText<QuickLabel>("port_label",portlabel_text.CStr()));
	RETURN_IF_ERROR(SetWidgetText<QuickLabel>("port_value",m_cookie_context->GetPort()));

	//comment has to be UTF-8, according to RFC 2965
	RETURN_IF_ERROR(SetWidgetText<QuickLabel>("comment_value",m_cookie_context->GetComment()));
	RETURN_IF_ERROR(SetWidgetText<QuickLabel>("commentURL_value",m_cookie_context->GetCommentURL()));
	
	
	OpString text, desc;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_ASKCOOKIE_DESCRIPTION, desc));
	// write either "session cookie" or just "cookie"
	OpString sessioncookie, sessioncookie_explanation;
	if (IsSessionCookie())
	{
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_SESSIONCOOKIE, sessioncookie));
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_SESSIONCOOKIE_EXPLANATION, sessioncookie_explanation)); // add explanation for session cookies
	}
	else
	{
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_COOKIE, sessioncookie));
		sessioncookie_explanation.Empty();
	}
	RETURN_IF_ERROR(text.AppendFormat(desc.CStr(), sessioncookie.CStr(), sessioncookie_explanation.CStr())); // insert params into description
	RETURN_IF_ERROR(SetWidgetText<QuickMultilineLabel>("description_multilabel",text));

	//Binding Checkbox in the button-strip
	RETURN_IF_ERROR(GetBinder()->Connect("checkbox_Default", *g_pcnet, PrefsCollectionNetwork::DisplayReceivedCookies, 0, 1));

	RETURN_IF_ERROR(GetBinder()->Connect("saveall_server_checkbox", s_is_saveall_server));
	RETURN_IF_ERROR(GetBinder()->Connect("session_only_checkbox", s_is_saveonly_session));
			
	m_details_expand = m_widgets->Get<QuickExpand>("details_expand");
	if (!m_details_expand)
		return OpStatus::ERR;

	if (s_is_details_shown.Get())
	{
		m_details_expand->Expand();
	}
	else
	{
		m_details_expand->Collapse();
	}

	return OpStatus::OK;
}

void AskCookieController::OnOk()
{	
	bool is_domain = s_is_saveall_server.Get();

	bool is_session = s_is_saveonly_session.Get() && !IsSessionCookie();

	if (is_domain && is_session) // save all cookies for this domain, but only for session
	{
		m_cookie_context->OnAskCookieDone(OpCookieListener::COOKIE_ACTION_ACCEPT_ALL_FROM_DOMAIN_BUT_DISCARD);
	}
	else if (is_domain) // save all cookies for this domain, permanently
	{		
		m_cookie_context->OnAskCookieDone(OpCookieListener::COOKIE_ACTION_ACCEPT_ALL_FROM_DOMAIN);
	}
	else if (is_session) // save this cookie, but only for this session (and show a dialog for every update)
	{
		m_cookie_context->OnAskCookieDone(OpCookieListener::COOKIE_ACTION_ACCEPT_BUT_DISCARD);
	}
	else // save this cookie permanently (and show a dialog for every update)
	{		
		m_cookie_context->OnAskCookieDone(OpCookieListener::COOKIE_ACTION_ACCEPT_SHOW_UPDATES);
	}

	s_is_details_shown.Set(m_details_expand->IsExpanded());

}

void AskCookieController::OnCancel()
{
	if (m_cookie_context)
	{
		if (s_is_saveall_server.Get())
			m_cookie_context->OnAskCookieDone(OpCookieListener::COOKIE_ACTION_REFUSE_ALL_FROM_DOMAIN);
		else
			m_cookie_context->OnAskCookieDone(OpCookieListener::COOKIE_ACTION_REFUSE);
	}

	if (!m_widgets)
		return;
	
	if (m_details_expand)
		s_is_details_shown.Set(m_details_expand->IsExpanded());
}
	
