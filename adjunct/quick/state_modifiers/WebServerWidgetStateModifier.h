
#ifndef WEBSERVER_WIDGET_STATE_MODIFIER_H
#define WEBSERVER_WIDGET_STATE_MODIFIER_H

#include "adjunct/quick/controller/state/WidgetState.h"
#include "adjunct/quick/controller/state/WidgetStateModifier.h"

class WebServerWidgetState : public WidgetState
{
public:
	WebServerWidgetState();

	virtual const uni_char*	GetText()	{ return m_text; }
	virtual OpInputAction*	GetAction() { return m_action; }
	virtual const char*		GetForegroundImage() = 0;

private:
	const uni_char*		m_text;
	OpInputAction*		m_action;
};

class WebServerEnabledWidgetState : public WebServerWidgetState
{
public:
	virtual const char*		GetForegroundImage()	{return "Unite Enabled";}
};

class WebServerDisabledWidgetState : public WebServerWidgetState
{
public:
	virtual const char*		GetForegroundImage()	{return "Unite Disabled";}
};

class WebServerErrorWidgetState : public WebServerWidgetState
{
public:
	virtual const char*		GetForegroundImage()	{return "Unite Failed";}
};

class WebServerWidgetStateModifier : public WidgetStateModifier
{
public:
	WebServerWidgetStateModifier();
	~WebServerWidgetStateModifier();

	WidgetState*	GetCurrentWidgetState();

	void			SetWidgetStateToEnabled();
	void			SetWidgetStateToDisabled();
	void			SetWidgetStateToError();

private:
	WidgetState*	m_widget_state;
};


#endif // WEBSERVER_WIDGET_STATE_MODIFIER_H