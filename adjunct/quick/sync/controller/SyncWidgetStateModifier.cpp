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

#ifdef SUPPORT_DATA_SYNC

#include "adjunct/quick/sync/controller/SyncWidgetStateModifier.h"
#include "modules/inputmanager/inputaction.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"


/***********************************************************************************
**
**  SyncEnabledWidgetState
**
************************************************************************************/

SyncEnabledWidgetState::SyncEnabledWidgetState()
	: m_action(OpInputAction::ACTION_SHOW_HIDDEN_POPUP_MENU)
{
	g_languageManager->GetString(Str::S_SYNC_STATE_ENABLED, m_text);
	m_action.GetActionInfo().SetStatusText(m_text.CStr());
	m_action.SetActionDataString(UNI_L("Sync Popup Menu"));
}


/***********************************************************************************
**
**  SyncDisabledWidgetState
**
************************************************************************************/

SyncDisabledWidgetState::SyncDisabledWidgetState()
	: m_action(OpInputAction::ACTION_SHOW_HIDDEN_POPUP_MENU)
{
	g_languageManager->GetString(Str::S_SYNC_STATE_DISABLED, m_text);
	m_action.GetActionInfo().SetStatusText(m_text.CStr());
	m_action.SetActionDataString(UNI_L("Sync Setup Popup Menu"));
}


/***********************************************************************************
**
**  SyncBusyWidgetState
**
************************************************************************************/

SyncBusyWidgetState::SyncBusyWidgetState()
	: m_action(OpInputAction::ACTION_SHOW_HIDDEN_POPUP_MENU)
{
	g_languageManager->GetString(Str::S_SYNC_STATE_BUSY, m_text);
	m_action.GetActionInfo().SetStatusText(m_text.CStr());
	m_action.SetActionDataString(UNI_L("Sync Popup Menu"));
}


/***********************************************************************************
**
**  SyncErrorWidgetState
**
************************************************************************************/

SyncErrorWidgetState::SyncErrorWidgetState()
	: m_action(OpInputAction::ACTION_SHOW_HIDDEN_POPUP_MENU)
{
	g_languageManager->GetString(Str::S_SYNC_STATE_FAILED, m_text);
	m_action.GetActionInfo().SetStatusText(m_text.CStr());
	m_action.SetActionDataString(UNI_L("Sync Popup Menu"));
}


/***********************************************************************************
**
**  SyncWidgetStateModifier
**
************************************************************************************/

OP_STATUS SyncWidgetStateModifier::Init()
{
	m_widget_state = OP_NEW(SyncDisabledWidgetState, ());
	return (m_widget_state ? WidgetStateModifier::Init() : OpStatus::ERR);
}


WidgetState* SyncWidgetStateModifier::GetCurrentWidgetState() const
{
	OP_ASSERT(IsInited());
	return m_widget_state;
}


void SyncWidgetStateModifier::SetWidgetStateToEnabled()
{
	OP_DELETE(m_widget_state);
	m_widget_state = OP_NEW(SyncEnabledWidgetState, ());

	GenerateOnStateChanged(m_widget_state);
}


void SyncWidgetStateModifier::SetWidgetStateToDisabled()
{
	OP_DELETE(m_widget_state);
	m_widget_state = OP_NEW(SyncDisabledWidgetState, ());

	GenerateOnStateChanged(m_widget_state);
}


void SyncWidgetStateModifier::SetWidgetStateToBusy()
{
	OP_DELETE(m_widget_state);
	m_widget_state = OP_NEW(SyncBusyWidgetState, ());

	GenerateOnStateChanged(m_widget_state);
}


void SyncWidgetStateModifier::SetWidgetStateToError()
{
	OP_DELETE(m_widget_state);
	m_widget_state = OP_NEW(SyncErrorWidgetState, ());

	GenerateOnStateChanged(m_widget_state);
}


#endif //  SUPPORT_DATA_SYNC
