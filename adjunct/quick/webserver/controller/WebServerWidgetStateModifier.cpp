/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#ifdef WEBSERVER_SUPPORT

#include "adjunct/quick/webserver/controller/WebServerWidgetStateModifier.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/inputmanager/inputaction.h"


/***********************************************************************************
**
**  WebServerWidgetState
**
************************************************************************************/

const uni_char* WebServerWidgetState::GetText() const
{
	if (HasAttention() && m_attention_text.HasContent())
	{
		return m_attention_text.CStr();
	}
	else
	{
		return GetStatusText().CStr();
	}
}


const char* WebServerWidgetState::GetForegroundImage() const
{
#ifdef SHOW_DISCOVERED_DEVICES_SUPPORT
	if (HasAttention())
	{
		return GetAttentionFgImage();
	}
	else
#endif // SHOW_DISCOVERED_DEVICES_SUPPORT
	{
		return GetFgImage();
	}
}


void WebServerWidgetState::SetAttention(bool attention)
{
	if (WidgetState::HasAttention() != attention)
	{
		WidgetState::SetAttention(attention);
		SetActionText(GetText());
	}
}


OP_STATUS WebServerWidgetState::SetAttentionText(const OpStringC & attention_text)
{
	if (m_attention_text.Compare(attention_text) != 0)
	{
		RETURN_IF_ERROR(m_attention_text.Set(attention_text.CStr()));
		if (HasAttention())
		{
			RETURN_IF_ERROR(SetActionText(GetText()));
		}
	}
	return OpStatus::OK;
}

/***********************************************************************************
**
**  WebServerEnabledWidgetState
**
************************************************************************************/

WebServerEnabledWidgetState::WebServerEnabledWidgetState()
	: m_action(OpInputAction::ACTION_SHOW_HIDDEN_POPUP_MENU)
{
	g_languageManager->GetString(Str::S_WEBSERVER_STATE_ENABLED, m_text);
	OpStatus::Ignore(SetActionText(m_text.CStr()));
	m_action.SetActionDataString(UNI_L("Webserver Status Popup Menu"));
}


OP_STATUS WebServerEnabledWidgetState::SetActionText(const OpStringC & action_text)
{
	return m_action.GetActionInfo().SetStatusText(action_text.CStr());
}

/***********************************************************************************
**
**  WebServerDisabledWidgetState
**
************************************************************************************/

WebServerDisabledWidgetState::WebServerDisabledWidgetState()
	: m_action(OpInputAction::ACTION_SHOW_HIDDEN_POPUP_MENU)
{
	g_languageManager->GetString(Str::S_WEBSERVER_STATE_DISABLED, m_text);
	OpStatus::Ignore(SetActionText(m_text.CStr()));
	m_action.SetActionDataString(UNI_L("Webserver Setup Status Popup Menu"));
}


OP_STATUS WebServerDisabledWidgetState::SetActionText(const OpStringC & action_text)
{
	return m_action.GetActionInfo().SetStatusText(action_text.CStr());
}


/***********************************************************************************
**
**  WebServerEnablingWidgetState
**
************************************************************************************/

WebServerEnablingWidgetState::WebServerEnablingWidgetState()
	: m_action(OpInputAction::ACTION_SHOW_HIDDEN_POPUP_MENU)
{
	g_languageManager->GetString(Str::S_WEBSERVER_STATE_ENABLING, m_text);
	OpStatus::Ignore(SetActionText(m_text.CStr()));
	m_action.SetActionDataString(UNI_L("Webserver Setup Status Popup Menu"));
}


OP_STATUS WebServerEnablingWidgetState::SetActionText(const OpStringC & action_text)
{
	return m_action.GetActionInfo().SetStatusText(action_text.CStr());
}


/***********************************************************************************
**
**  WebServerErrorWidgetState
**
************************************************************************************/

WebServerErrorWidgetState::WebServerErrorWidgetState()
	: m_action(OpInputAction::ACTION_SHOW_HIDDEN_POPUP_MENU)
{
	g_languageManager->GetString(Str::S_WEBSERVER_STATE_FAILED, m_text);
	OpStatus::Ignore(SetActionText(m_text.CStr()));
	m_action.SetActionDataString(UNI_L("Webserver Status Popup Menu"));
}


OP_STATUS WebServerErrorWidgetState::SetActionText(const OpStringC & action_text)
{
	return m_action.GetActionInfo().SetStatusText(action_text.CStr());
}


/***********************************************************************************
**
**  WebServerWidgetStateModifier
**
************************************************************************************/

OP_STATUS WebServerWidgetStateModifier::Init()
{
	m_widget_state = OP_NEW(WebServerDisabledWidgetState, ());
	return (m_widget_state ? WidgetStateModifier::Init() : OpStatus::ERR);
}


WidgetState* WebServerWidgetStateModifier::GetCurrentWidgetState() const
{
	OP_ASSERT(IsInited());
	return m_widget_state;
}


bool WebServerWidgetStateModifier::HasAttention() const
{
	OP_ASSERT(m_widget_state);
	if (m_widget_state)
	{
		return m_widget_state->HasAttention();
	}
	else
	{
		return false;
	}
}


void WebServerWidgetStateModifier::SetAttention(bool attention)
{
	OP_ASSERT(m_widget_state);
	if (m_widget_state)
	{
		m_widget_state->SetAttention(attention);
		GenerateOnStateChanged(m_widget_state);
	}
}


OP_STATUS WebServerWidgetStateModifier::SetWidgetStateToEnabled()
{
	OP_ASSERT(IsInited());

	WebServerWidgetState * state = OP_NEW(WebServerEnabledWidgetState, ());
	if (!state)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(state->SetAttentionText(m_widget_state->GetAttentionText()));
	state->SetAttention(m_widget_state->HasAttention());

	OP_DELETE(m_widget_state);
	m_widget_state = state;
	GenerateOnStateChanged(m_widget_state);

	return OpStatus::OK;
}


OP_STATUS WebServerWidgetStateModifier::SetWidgetStateToDisabled()
{
	OP_ASSERT(IsInited());

	WebServerWidgetState * state = OP_NEW(WebServerDisabledWidgetState, ());
	if (!state)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(state->SetAttentionText(m_widget_state->GetAttentionText()));
	state->SetAttention(m_widget_state->HasAttention());

	OP_DELETE(m_widget_state);
	m_widget_state = state;
	GenerateOnStateChanged(m_widget_state);

	return OpStatus::OK;
}


OP_STATUS WebServerWidgetStateModifier::SetWidgetStateToEnabling()
{
	OP_ASSERT(IsInited());

	WebServerWidgetState * state = OP_NEW(WebServerEnablingWidgetState, ());
	if (!state)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(state->SetAttentionText(m_widget_state->GetAttentionText()));
	state->SetAttention(m_widget_state->HasAttention());

	OP_DELETE(m_widget_state);
	m_widget_state = state;
	GenerateOnStateChanged(m_widget_state);

	return OpStatus::OK;
}


OP_STATUS WebServerWidgetStateModifier::SetWidgetStateToError()
{
	OP_ASSERT(IsInited());

	WebServerWidgetState * state = OP_NEW(WebServerErrorWidgetState, ());
	if (!state)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(state->SetAttentionText(m_widget_state->GetAttentionText()));
	state->SetAttention(m_widget_state->HasAttention());

	OP_DELETE(m_widget_state);
	m_widget_state = state;
	GenerateOnStateChanged(m_widget_state);

	return OpStatus::OK;
}


#endif // WEBSERVER_SUPPORT
