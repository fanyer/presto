/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef CLASSIC_GLOBAL_UI_CONTEXT_H
#define	CLASSIC_GLOBAL_UI_CONTEXT_H

#include "adjunct/desktop_pi/DesktopMultimediaPlayer.h"
#include "adjunct/quick_toolkit/contexts/UiContext.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "modules/inputmanager/inputcontext.h"

class Application;
class ChatDesktopWindow;
class DesktopFileChooser;
class DesktopFileChooserListener;
class MailDesktopWindow;
class Message;
class OpMultimediaPlayer;


/**
 * The global input context.
 *
 * It starts disabled.  SetEnabled(TRUE) is required to start handling actions
 * Typically, an Application will call it when it's fully operational.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class ClassicGlobalUiContext : public UiContext
		, public DesktopWindowListener
{
public:
	explicit ClassicGlobalUiContext(Application& application);
	~ClassicGlobalUiContext();

	/**
	 * Tells the input context to start or stop handling actions.
	 */
	void SetEnabled(BOOL enabled);

	//
	// DesktopWindowListener
	//
	virtual void OnDesktopWindowActivated(DesktopWindow* desktop_window,
			BOOL active);
	virtual void OnDesktopWindowClosing(DesktopWindow* desktop_window,
			BOOL user_initiated);
	

	//
	// OpInputContext
	//
	virtual const char*	GetInputContextName()
		{ return "Application"; }

	virtual BOOL IsInputDisabled();

	/**
	 * This method started as a copy of ClassicApplication::OnInputAction().
	 * Now, feature-specific contexts must be extracted, and what's left should
	 * become the true global context.
	 */
	virtual BOOL OnInputAction(OpInputAction* action);

private:
	BOOL m_enabled;
	Application* m_application;
	DesktopFileChooser* m_chooser;
	DesktopFileChooserListener* m_chooser_listener;
	DesktopWindow* m_most_recent_spyable_desktop_window;

	//
	// Everything below this line belongs in some feature context.
	//

private:
	OpWindowCommander* GetWindowCommander();
    ChatDesktopWindow* GetActiveChatDesktopWindow();

	// FIXME: Belongs in M2 context.
private:
	MailDesktopWindow* GoToMailView(Message* message);


	// FIXME: Belongs in multimedia context.
private:
	OpMultimediaPlayer*	GetMultimediaPlayer();

	OpMultimediaPlayer* m_multimedia_player;

};

#endif // CLASSIC_GLOBAL_UI_CONTEXT_H
