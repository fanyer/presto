/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef SYNC_WIDGET_STATE_MODIFIER_H
#define SYNC_WIDGET_STATE_MODIFIER_H


#ifdef SUPPORT_DATA_SYNC

#include "adjunct/quick_toolkit/widgets/state/WidgetState.h"
#include "adjunct/quick_toolkit/widgets/state/WidgetStateModifier.h"
#include "modules/inputmanager/inputaction.h"


/***********************************************************************************
**  @class SyncWidgetState
**	@brief Default state for sync button. Text and action stay the same
************************************************************************************/
class SyncWidgetState : public WidgetState
{
public:
#if defined(SELFTEST) || defined (_DEBUG)
	enum Type
	{
		SyncEnabled,
		SyncDisabled,
		SyncBusy,
		SyncError
	};

	virtual Type      GetStateType() const = 0;
#endif // defined(SELFTEST) || defined (_DEBUG)

	virtual const uni_char* GetTooltipText() {return GetText(); }
};


/***********************************************************************************
**  @class SyncEnabledWidgetState
**	@brief Enabled state. Sets enabled icon
************************************************************************************/
class SyncEnabledWidgetState : public SyncWidgetState
{
public:
	SyncEnabledWidgetState();

#if defined(SELFTEST) || defined (_DEBUG)
	virtual SyncWidgetState::Type GetStateType() const       { return SyncWidgetState::SyncEnabled; }
#endif // defined(SELFTEST) || defined (_DEBUG)

	virtual const uni_char*       GetText() const            { return m_text.CStr(); }
	virtual const OpInputAction*  GetAction() const          { return &m_action; }
	virtual const char*           GetForegroundImage() const { return "Link Enabled"; }
	virtual const char*           GetStatusImage() const     { return "Link Status Enabled"; }

private:
	OpInputAction m_action;
	OpString      m_text;
};


/***********************************************************************************
**  @class SyncDisabledWidgetState
**	@brief Disabled state. Sets disabled icon
************************************************************************************/
class SyncDisabledWidgetState : public SyncWidgetState
{
public:
	SyncDisabledWidgetState();

#if defined(SELFTEST) || defined (_DEBUG)
	virtual SyncWidgetState::Type GetStateType() const       { return SyncWidgetState::SyncDisabled; }
#endif // defined(SELFTEST) || defined (_DEBUG)

	virtual const uni_char*       GetText() const            { return m_text.CStr(); }
	virtual const OpInputAction*  GetAction() const          { return &m_action; }
	virtual const char*           GetForegroundImage() const { return "Link Disabled"; }
	virtual const char*           GetStatusImage() const     { return "Link Status Disabled"; }

private:
	OpInputAction m_action;
	OpString      m_text;
};


/***********************************************************************************
**  @class SyncBusyWidgetState
**	@brief Busy state. Sets busy icon
************************************************************************************/
class SyncBusyWidgetState : public SyncWidgetState
{
public:
	SyncBusyWidgetState();

#if defined(SELFTEST) || defined (_DEBUG)
	virtual SyncWidgetState::Type GetStateType() const       { return SyncWidgetState::SyncBusy; }
#endif // defined(SELFTEST) || defined (_DEBUG)

	virtual const uni_char*       GetText() const            { return m_text.CStr(); }
	virtual const OpInputAction*  GetAction() const          { return &m_action; }
	virtual const char*           GetForegroundImage() const { return "Link Busy"; }
	virtual const char*           GetStatusImage() const     { return "Link Status Enabled"; }

private:
	OpInputAction m_action;
	OpString      m_text;
};


/***********************************************************************************
**  @class SyncErrorWidgetState
**	@brief Error state. Sets error icon
************************************************************************************/
class SyncErrorWidgetState : public SyncWidgetState
{
public:
	SyncErrorWidgetState();

#if defined(SELFTEST) || defined (_DEBUG)
	virtual SyncWidgetState::Type GetStateType() const       { return SyncWidgetState::SyncError; }
#endif // defined(SELFTEST) || defined (_DEBUG)

	virtual const uni_char*       GetText() const            { return m_text.CStr(); }
	virtual const OpInputAction*  GetAction() const          { return &m_action; }
	virtual const char*           GetForegroundImage() const { return "Link Failed"; }
	virtual const char*           GetStatusImage() const     { return "Link Status Failed"; }

private:
	OpInputAction m_action;
	OpString      m_text;
};


/***********************************************************************************
**  @class SyncWidgetStateModifier
**	@brief Modifier to control sync state button appearance.
**
************************************************************************************/
class SyncWidgetStateModifier : public WidgetStateModifier
{
public:
	SyncWidgetStateModifier() : m_widget_state(NULL) {}
	virtual ~SyncWidgetStateModifier() { OP_DELETE(m_widget_state); }

	virtual OP_STATUS	Init(); // do everything that needs Core there, not in the constructor

	virtual const char* GetDescriptionString() const { return "Sync"; }
	
	WidgetState*	GetCurrentWidgetState() const;	//< returns current widget state

	void			SetWidgetStateToEnabled();	//< sets state to 'enabled' and informs listeners
	void			SetWidgetStateToDisabled();	//< sets state to 'disabled' and informs listeners
	void			SetWidgetStateToBusy();		//< sets state to 'busy' and informs listeners
	void			SetWidgetStateToError();	//< sets state to 'error' and informs listeners

#if defined(SELFTEST) || defined(_DEBUG)
	virtual WidgetStateModifier::Type	GetModifierType() const { return WidgetStateModifier::SyncModifier; }
#endif // defined(SELFTEST) || defined(_DEBUG)

private:
	WidgetState*	m_widget_state;
};

#endif // SUPPORT_DATA_SYNC

#endif // SYNC_WIDGET_STATE_MODIFIER_H
