/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef WEBSERVER_WIDGET_STATE_MODIFIER_H
#define WEBSERVER_WIDGET_STATE_MODIFIER_H

#ifdef WEBSERVER_SUPPORT

#include "modules/inputmanager/inputaction.h"
#include "adjunct/quick_toolkit/widgets/state/WidgetState.h"
#include "adjunct/quick_toolkit/widgets/state/WidgetStateModifier.h"


/***********************************************************************************
**  @class WebServerWidgetState
**	@brief Default state for web server button. Text and action stay the same
************************************************************************************/
class WebServerWidgetState : public WidgetState
{
public:
#if defined(SELFTEST) || defined (_DEBUG)
	enum Type
	{
		WebServerEnabled,
		WebServerDisabled,
		WebServerEnabling,
		WebServerError
	};

	virtual Type					GetStateType() const = 0;
#endif // defined(SELFTEST) || defined (_DEBUG)

	WebServerWidgetState()  {}
	virtual ~WebServerWidgetState() {}

	virtual const uni_char*   GetText() const;
	virtual const uni_char*   GetTooltipText() { return GetText(); }
	virtual const char*       GetForegroundImage() const;

	virtual void              SetAttention(bool attention);
	virtual OP_STATUS         SetAttentionText(const OpStringC & attention_text);
	virtual const OpStringC & GetAttentionText() const { return m_attention_text; }

protected:
	virtual const char*       GetFgImage() const = 0;
	virtual const char*       GetAttentionFgImage() const = 0;
	virtual const OpStringC & GetStatusText() const = 0;
	virtual OP_STATUS         SetActionText(const OpStringC & action_text) = 0;

private:
	OpString	m_attention_text;
};


/***********************************************************************************
**  @class WebServerEnabledWidgetState
**	@brief Enabled state. Sets enabled icon
************************************************************************************/
class WebServerEnabledWidgetState : public WebServerWidgetState
{
public:
	WebServerEnabledWidgetState();
	virtual ~WebServerEnabledWidgetState() {}

#if defined(SELFTEST) || defined (_DEBUG)
	virtual WebServerWidgetState::Type GetStateType() const { return WebServerWidgetState::WebServerEnabled; }
#endif // defined(SELFTEST) || defined (_DEBUG)

	virtual const OpInputAction*    GetAction() const       { return &m_action; }
	virtual const char*             GetStatusImage() const  { return "Unite Status Enabled"; }

protected:
	virtual const char*       GetFgImage() const            { return "Unite Enabled"; }
	virtual const char*       GetAttentionFgImage() const   { return "Unite Enabled Notification"; }
	virtual const OpStringC & GetStatusText() const         { return m_text; }
	virtual OP_STATUS         SetActionText(const OpStringC & action_text);

private:
	OpInputAction           m_action;
	static const OpStringC8 m_status_image;
	OpString                m_text;
};


/***********************************************************************************
**  @class WebServerDisabledWidgetState
**	@brief Disabled state. Sets disabled icon
************************************************************************************/
class WebServerDisabledWidgetState : public WebServerWidgetState
{
public:
	WebServerDisabledWidgetState();
	virtual ~WebServerDisabledWidgetState() {}

#if defined(SELFTEST) || defined (_DEBUG)
	virtual WebServerWidgetState::Type GetStateType() const { return WebServerWidgetState::WebServerDisabled; }
#endif // defined(SELFTEST) || defined (_DEBUG)

	virtual const OpInputAction*    GetAction() const       { return &m_action; }
	virtual const char*             GetStatusImage() const  { return "Unite Status Disabled"; }

protected:
	virtual const char*       GetFgImage() const            { return "Unite Disabled"; }
	virtual const char*       GetAttentionFgImage() const   { return "Unite Notification"; }
	virtual const OpStringC & GetStatusText() const         { return m_text; }
	virtual OP_STATUS         SetActionText(const OpStringC & action_text);

private:
	OpInputAction           m_action;
	static const OpStringC8 m_status_image;
	OpString                m_text;
};


/***********************************************************************************
**  @class WebServerEnablingWidgetState
**	@brief 'In progress of enabling' state
************************************************************************************/
class WebServerEnablingWidgetState : public WebServerWidgetState
{
public:
	WebServerEnablingWidgetState();
	virtual ~WebServerEnablingWidgetState() {}

#if defined(SELFTEST) || defined (_DEBUG)
	virtual WebServerWidgetState::Type GetStateType() const { return WebServerWidgetState::WebServerEnabling; }
#endif // defined(SELFTEST) || defined (_DEBUG)

	virtual const OpInputAction*    GetAction() const       { return &m_action; }
	virtual const char*             GetStatusImage() const  { return "Unite Status Disabled"; }

protected:
	virtual const char*       GetFgImage() const            { return "Unite Disabled"; }
	virtual const char*       GetAttentionFgImage() const   { return "Unite Notification"; }
	virtual const OpStringC & GetStatusText() const         { return m_text; }
	virtual OP_STATUS         SetActionText(const OpStringC & action_text);

private:
	OpInputAction           m_action;
	static const OpStringC8 m_status_image;
	OpString                m_text;

};


/***********************************************************************************
**  @class WebServerErrorWidgetState
**	@brief Error state. Sets error icon
************************************************************************************/
class WebServerErrorWidgetState : public WebServerWidgetState
{
public:
	WebServerErrorWidgetState();
	virtual ~WebServerErrorWidgetState() {}

#if defined(SELFTEST) || defined (_DEBUG)
	virtual WebServerWidgetState::Type GetStateType() const { return WebServerWidgetState::WebServerError; }
#endif // defined(SELFTEST) || defined (_DEBUG)

	virtual const OpInputAction*    GetAction() const       { return &m_action; }
	virtual const char*             GetStatusImage() const  { return "Unite Status Enabled"; }

protected:
	virtual const char*       GetFgImage() const            { return "Unite Failed"; }
	virtual const char*       GetAttentionFgImage() const   { return "Unite Failed Notification"; }
	virtual const OpStringC & GetStatusText() const         { return m_text; }
	virtual OP_STATUS         SetActionText(const OpStringC & action_text);

private:
	OpInputAction           m_action;
	static const OpStringC8 m_status_image;
	OpString                m_text;
};


/***********************************************************************************
**  @class WebServerWidgetStateModifier
**	@brief Modifier to control web server state button appearance.
**
************************************************************************************/
class WebServerWidgetStateModifier : public WidgetStateModifier
{
public:
	WebServerWidgetStateModifier() : m_widget_state(NULL) {}
	virtual ~WebServerWidgetStateModifier() { OP_DELETE(m_widget_state); }

	virtual OP_STATUS	Init(); // do everything that needs Core there, not in the constructor

	virtual const char* GetDescriptionString() const { return "Webserver"; }

	WidgetState*	GetCurrentWidgetState() const;

	virtual bool	HasAttention() const;
	virtual void	SetAttention(bool attention);
	virtual OP_STATUS			SetAttentionText(const OpStringC & attention_text) { return m_widget_state->SetAttentionText(attention_text); }
	virtual const OpStringC &	GetAttentionText() const { return m_widget_state->GetAttentionText(); }

	OP_STATUS		SetWidgetStateToEnabled();
	OP_STATUS		SetWidgetStateToDisabled();
	OP_STATUS		SetWidgetStateToEnabling();
	OP_STATUS		SetWidgetStateToError();

#if defined(SELFTEST) || defined(_DEBUG)
	virtual WidgetStateModifier::Type	GetModifierType() const { return WidgetStateModifier::WebServerModifier; }
#endif // defined(SELFTEST) || defined(_DEBUG)

private:
	WebServerWidgetState*	m_widget_state;
};

#endif // WEBSERVER_SUPPORT

#endif // WEBSERVER_WIDGET_STATE_MODIFIER_H
