
#ifndef SYNC_WIDGET_STATE_MODIFIER_H
#define SYNC_WIDGET_STATE_MODIFIER_H

#include "adjunct/desktop_util/state/WidgetState.h"
#include "adjunct/desktop_util/state/WidgetStateModifier.h"

class SyncWidgetState : public WidgetState
{
public:
	SyncWidgetState();
	virtual ~SyncWidgetState();

	virtual const uni_char*	GetText()	{ return m_text; }
	virtual OpInputAction*	GetAction() { return m_action; }
	virtual const char*		GetForegroundImage() = 0;

private:
	const uni_char*		m_text;
	OpInputAction*		m_action;
};

class SyncEnabledWidgetState : public SyncWidgetState
{
public:
	virtual const char*		GetForegroundImage()	{return "Link Enabled";}
};

class SyncDisabledWidgetState : public SyncWidgetState
{
public:
	virtual const char*		GetForegroundImage()	{return "Link Disabled";}
};

class SyncBusyWidgetState : public SyncWidgetState
{
public:
	virtual const char*		GetForegroundImage()	{return "Link Busy";}
};

class SyncErrorWidgetState : public SyncWidgetState
{
public:
	virtual const char*		GetForegroundImage()	{return "Link Failed";}
};

class SyncWidgetStateModifier : public WidgetStateModifier
{
public:
	SyncWidgetStateModifier();
	virtual ~SyncWidgetStateModifier();

	WidgetState*	GetCurrentWidgetState();

	void			SetWidgetStateToEnabled();
	void			SetWidgetStateToDisabled();
	void			SetWidgetStateToBusy();
	void			SetWidgetStateToError();

private:
	WidgetState*	m_widget_state;
};


#endif // SYNC_WIDGET_STATE_MODIFIER_H