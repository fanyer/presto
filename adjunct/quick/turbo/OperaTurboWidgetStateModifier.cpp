/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"


#include "adjunct/quick/turbo/OperaTurboWidgetStateModifier.h"
#include "adjunct/quick/managers/NotificationManager.h"
#include "adjunct/quick/managers/OperaTurboManager.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/inputmanager/inputaction.h"


/***********************************************************************************
**
**  OperaTurboWidgetState
**
************************************************************************************/
OpInputAction OperaTurboWidgetState::m_action(OpInputAction::ACTION_SHOW_HIDDEN_POPUP_MENU);

OperaTurboWidgetState::OperaTurboWidgetState()
{
	if (m_action.GetActionDataString() == NULL)
		m_action.SetActionDataString(UNI_L("Turbo Popup Menu"));
}

const char* OperaTurboWidgetState::GetForegroundImage() const
{
	if (HasAttention())
	{
		return GetAttentionFgImage();
	}
	else
	{
		return GetFgImage();
	}
}


/***********************************************************************************
**
**  OperaTurboEnabledWidgetState
**
************************************************************************************/

OperaTurboEnabledWidgetState::OperaTurboEnabledWidgetState()
{
	g_languageManager->GetString(Str::S_OPERA_TURBO_ON, m_text);
}


const uni_char* OperaTurboEnabledWidgetState::GetTooltipText()
{
	if(HasAttention())
	{
		g_languageManager->GetString(Str::S_OPERA_TURBO_BUSY_SERVERS, m_tooltip_text);
	}
	else
	{
		float compression_rate = g_opera_turbo_manager->GetCompressionRate();
		if(compression_rate > 0)
		{
			OpString str;
			g_languageManager->GetString(Str::S_OPERA_WEB_TURBO_COMPRESSION_INFO, str);
			OpString data_saved;
			int bytes_saved = g_opera_turbo_manager->GetBytesSaved();
			if(bytes_saved < 1024)
			{
				g_languageManager->GetString(Str::SI_IDSTR_BYTES, data_saved);
			}
			else if(bytes_saved < 1024*1024)
			{
				g_languageManager->GetString(Str::SI_IDSTR_KILOBYTE, data_saved);
				bytes_saved /= 1024;
			}
			else
			{
				g_languageManager->GetString(Str::SI_IDSTR_MEGABYTE, data_saved);
				bytes_saved /= 1024*1024;
			}
			OpString s;
			s.AppendFormat(UNI_L("%d %s"), bytes_saved, data_saved.CStr());
			m_tooltip_text.Empty();
			m_tooltip_text.AppendFormat(str.CStr(), compression_rate, s.CStr());
		}
		else
		{
			OpString str;
			g_languageManager->GetString(Str::S_OPERA_WEB_TURBO_ENABLED, str);
			m_tooltip_text.Set(str);
		}
	}
	return m_tooltip_text.CStr();
}


/***********************************************************************************
**
**  OperaTurboDisbledWidgetState
**
************************************************************************************/

OperaTurboDisabledWidgetState::OperaTurboDisabledWidgetState()
{
	g_languageManager->GetString(Str::S_OPERA_TURBO_OFF, m_text);
}

const uni_char* OperaTurboDisabledWidgetState::GetTooltipText()
{
	if(HasAttention())
	{
		g_languageManager->GetString(Str::S_OPERA_TURBO_BUSY_SERVERS, m_tooltip_text);
	}
	else
	{
		g_languageManager->GetString(Str::S_OPERA_WEB_TURBO_DISABLED, m_tooltip_text);
	}
	return m_tooltip_text.CStr();
}

/***********************************************************************************
**
**  OperaTurboWidgetStateModifier
**
************************************************************************************/

OP_STATUS OperaTurboWidgetStateModifier::Init()
{
	if (g_opera_turbo_manager->GetTurboMode() == OperaTurboManager::OperaTurboOff)
		m_widget_state = OP_NEW(OperaTurboDisabledWidgetState, ());
	else
		m_widget_state = OP_NEW(OperaTurboEnabledWidgetState, ());

	return (m_widget_state ? WidgetStateModifier::Init() : OpStatus::ERR);
}

void OperaTurboWidgetStateModifier::SetWidgetStateToEnabled()
{
	OP_DELETE(m_widget_state);
	m_widget_state = OP_NEW(OperaTurboEnabledWidgetState, ());

	GenerateOnStateChanged(m_widget_state);
}

void OperaTurboWidgetStateModifier::SetWidgetStateToDisabled()
{
	OP_DELETE(m_widget_state);
	m_widget_state = OP_NEW(OperaTurboDisabledWidgetState, ());

	GenerateOnStateChanged(m_widget_state);
}

void OperaTurboWidgetStateModifier::OnShowNotification(const OpString& text, const OpString& button_text, OpInputAction* action)
{
	OpString title;
	g_notification_manager->ShowNotification(DesktopNotifier::NETWORK_SPEED_NOTIFICATION, text, "Web Turbo Mode Info Icon", action, TRUE);
}

bool OperaTurboWidgetStateModifier::HasAttention() const
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


void OperaTurboWidgetStateModifier::SetAttention(bool attention)
{
	OP_ASSERT(m_widget_state);
	if (m_widget_state)
	{
		m_widget_state->SetAttention(attention);
		GenerateOnStateChanged(m_widget_state);
	}
}

