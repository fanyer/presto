/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef SESSION_LOAD_CONTEXT_H
#define	SESSION_LOAD_CONTEXT_H

#include "adjunct/desktop_util/sessions/opsession.h"
#include "modules/inputmanager/inputcontext.h"

class ClassicApplication;
class DesktopFileChooser;
class DesktopFileChooserListener;

/**
 * Context used for actions related to session load and management.
 *
 * Currently hooked up into Opera's input action system in a somewhat clumsy
 * and definitely not scalable way.  This should improve once DSK-271123 is
 * implemented.
 */
class SessionLoadContext : public OpInputContext
{
public:
	explicit SessionLoadContext(ClassicApplication& application);
	~SessionLoadContext();

	UINT32 GetClosedSessionCount() const
		{ return m_closed_sessions.GetCount(); }
	OpSession* GetClosedSession(UINT32 i) const
		{ return m_closed_sessions.Get(i); }
	void AddClosedSession(OpSession* session)
		{ OpStatus::Ignore(m_closed_sessions.Add(session)); }
	void EmptyClosedSessions()
		{ m_closed_sessions.DeleteAll(); }

	//
	// OpInputContext
	//

	virtual const char*	GetInputContextName() { return "Session load"; }

	virtual BOOL OnInputAction(OpInputAction* action);

private:
	ClassicApplication* m_application;
	DesktopFileChooser* m_chooser;
	DesktopFileChooserListener* m_chooser_listener;
	OpAutoVector<OpSession> m_closed_sessions;
};

#endif // SESSION_LOAD_CONTEXT_H
