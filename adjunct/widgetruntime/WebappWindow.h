/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * \file 
 * \author Owner: Patryk Obara (pobara)
 */
#ifndef WEBAPP_WINDOW_H
#define WEBAPP_WINDOW_H

#ifdef WIDGET_RUNTIME_SUPPORT
#ifdef WEBAPP_SUPPORT

#include "adjunct/quick/windows/DesktopWindow.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "modules/widgets/OpWidget.h"
#include "adjunct/quick/widgets/OpBrowserView.h"

/**
 * This is main window designed to display single webapplication 
 *
 * It can also be called SSB (Site Specific Browser).
 * It should make webapplication feel as standalone application
 * and not document presented through browser UI.
 *
 * \todo register certain listeners
 * \todo careful UI design
 */
class WebappWindow :
	public DesktopWindow,
    public OpPageListener
{
	public:
						WebappWindow();
	virtual 			~WebappWindow(); /// Destructor

	OP_STATUS Init(const uni_char *address);

	// == DesktopWindow ==============================
	
	// let's act as Document Window for now
	virtual const char*	GetWindowName() { return "Document Window"; }
	OpBrowserView*		GetBrowserView() { return m_document_view; }
	OpWindowCommander*	GetWindowCommander() { return m_document_view ? m_document_view->GetWindowCommander() : NULL; }

	// virtual Type	GetType() { return WINDOW_TYPE_DOCUMENT; }


	private:
	/// Document presented inside of window.
	OpBrowserView* m_document_view;

	//OpListeners<DocumentWindowListener>	m_listeners;

	// == OpPageListener ======================
	virtual void			OnPageStartLoading(OpWindowCommander* commander);
	// virtual void 		OnPageUrlChanged(OpWindowCommander* commander, const uni_char* url)
	
	// == DesktopWindow hooks ========================
	virtual void			OnLayout();

	// == OpInputContext =============================
	virtual BOOL			OnInputAction(OpInputAction* action);

};

#endif // WEBAPP_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT
#endif // WEBAPP_WINDOW_H
