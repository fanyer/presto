/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef OPERA_TURBO_WIDGET_STATE_MODIFIER_H
#define OPERA_TURBO_WIDGET_STATE_MODIFIER_H


#include "modules/inputmanager/inputaction.h"
#include "adjunct/quick_toolkit/widgets/state/WidgetState.h"
#include "adjunct/quick_toolkit/widgets/state/WidgetStateModifier.h"


/***********************************************************************************
**  @class OperaTurboWidgetState
**	@brief Default state for turbo button.
************************************************************************************/
class OperaTurboWidgetState : public WidgetState
{
public:
#if defined(SELFTEST) || defined (_DEBUG)
	enum Type
	{
		TurboEnabled,
		TurboDisabled,
	};

	virtual Type GetStateType() const = 0;
#endif // defined(SELFTEST) || defined (_DEBUG)

	OperaTurboWidgetState();
	virtual ~OperaTurboWidgetState() {}

	virtual const OpInputAction* GetAction() const          { return &m_action; }
	virtual const char*          GetForegroundImage() const;
	virtual const uni_char*      GetTooltipText()           { return NULL; }
	virtual const char*          GetStatusImage() const     { return GetForegroundImage(); }

protected:
	virtual const char*          GetFgImage() const = 0;
	virtual const char*          GetAttentionFgImage() const = 0;

private:
	static OpInputAction         m_action;
};


/***********************************************************************************
**  @class OperaTurboEnabledWidgetState
************************************************************************************/
class OperaTurboEnabledWidgetState : public OperaTurboWidgetState
{
public:
	OperaTurboEnabledWidgetState();
	virtual ~OperaTurboEnabledWidgetState() {}

#if defined(SELFTEST) || defined (_DEBUG)
	virtual OperaTurboWidgetState::Type GetStateType() const { return OperaTurboWidgetState::TurboEnabled; }
#endif // defined(SELFTEST) || defined (_DEBUG)

	virtual const uni_char*   GetText()             const { return m_text.CStr(); }
	virtual const uni_char*   GetTooltipText();
	virtual const char*       GetFgImage()          const { return "Web Turbo Mode Enabled"; } 
	virtual const char*       GetAttentionFgImage() const { return "Web Turbo Mode Enabled Warning"; }

private:
	OpString m_text;
	OpString m_tooltip_text;
};


/***********************************************************************************
**  @class OperaTurboDisabledWidgetState
************************************************************************************/
class OperaTurboDisabledWidgetState : public OperaTurboWidgetState
{
public:
	OperaTurboDisabledWidgetState();
	virtual ~OperaTurboDisabledWidgetState() {}

#if defined(SELFTEST) || defined (_DEBUG)
	virtual OperaTurboWidgetState::Type GetStateType() const { return OperaTurboWidgetState::TurboDisabled; }
#endif // defined(SELFTEST) || defined (_DEBUG)

	virtual const uni_char*   GetText()             const { return m_text.CStr(); }
	virtual const uni_char*   GetTooltipText();
	virtual const char*       GetFgImage()          const { return "Web Turbo Mode Disabled"; } 
	virtual const char*       GetAttentionFgImage() const { return "Web Turbo Mode Disabled Warning"; }

private:
	OpString m_text;
	OpString m_tooltip_text;
};


/***********************************************************************************
**  @class OperaTurboWidgetStateModifier
**	@brief Modifier to control turbo state button appearance.
**
************************************************************************************/
class OperaTurboWidgetStateModifier : public WidgetStateModifier
{
public:
	OperaTurboWidgetStateModifier(): m_widget_state(NULL) {}
	virtual ~OperaTurboWidgetStateModifier() { OP_DELETE(m_widget_state); }

	virtual OP_STATUS Init(); // do everything that needs Core there, not in the constructor

#if defined(SELFTEST) || defined(_DEBUG)
	virtual WidgetStateModifier::Type GetModifierType() const { return WidgetStateModifier::OperaTurboModifier; }
#endif // defined(SELFTEST) || defined(_DEBUG)

	virtual const char* GetDescriptionString() const { return "Turbo"; }
	
	WidgetState* GetCurrentWidgetState() const { return m_widget_state; }

	void SetWidgetStateToEnabled();
	void SetWidgetStateToDisabled();

	void OnShowNotification(const OpString& text, const OpString& button_text, OpInputAction* action);

	bool HasAttention() const;
	void SetAttention(bool attention);

private:
	WidgetState*	m_widget_state;
};

#endif // OPERA_TURBO_WIDGET_STATE_MODIFIER_H

