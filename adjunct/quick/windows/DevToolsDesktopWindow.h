/** -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Alexey Feldgendler (alexeyf)
 */
#if !defined DEVTOOLS_DESKTOP_WINDOW_H && defined INTEGRATED_DEVTOOLS_SUPPORT
#define DEVTOOLS_DESKTOP_WINDOW_H

#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"

class OpBrowserView;

/***********************************************************************************
**
**	DevToolsDesktopWindow
**
***********************************************************************************/

class DevToolsDesktopWindow:
	public DesktopWindow,
	public OpPageListener,
	public DesktopWindowCollection::Listener
{

public:

	explicit				DevToolsDesktopWindow(OpWorkspace* parent_workspace);
	virtual					~DevToolsDesktopWindow();

	static INT32			GetMinimumWindowHeight() { return 200; }

	// Subclassing DesktopWindow

	virtual OpWindowCommander* GetWindowCommander() { return m_document_view ? m_document_view->GetWindowCommander() : NULL; }
	virtual OpBrowserView*	GetBrowserView() { return m_document_view; }
	virtual void			UpdateWindowIcon() { OnPageDocumentIconAdded(NULL); }
	virtual void			ResetParentWorkspace() {}

	// Hooks

	virtual void			OnLayout();

	// OpTypedObject

	virtual Type			GetType() { return WINDOW_TYPE_DEVTOOLS; }

	// == OpInputContext ======================

	virtual BOOL			OnInputAction(OpInputAction* action);
	virtual const char*		GetInputContextName() {return "Developer Tools Window";}

	// == OpPageListener ======================

	void					OnPageTitleChanged(OpWindowCommander* commander, const uni_char* title);
	void					OnPageDocumentIconAdded(OpWindowCommander* commander);
	void					OnPageWindowAttachRequest(OpWindowCommander* commander, BOOL attached);
	void					OnPageGetWindowAttached(OpWindowCommander* commander, BOOL* attached);
	void					OnPageClose(OpWindowCommander* commander);
	void					OnPageRaiseRequest(OpWindowCommander* commander);
	void					OnPageLowerRequest(OpWindowCommander* commander);
	void					OnPageGetInnerSize(OpWindowCommander* commander, UINT32* width, UINT32* height);
	void					OnPageGetOuterSize(OpWindowCommander* commander, UINT32* width, UINT32* height);
	void					OnPageGetPosition(OpWindowCommander* commander, INT32* x, INT32* y);

	
	// == DesktopWindowCollection::Listener ===
	
	void OnDesktopWindowRemoved(DesktopWindow* window);
	void OnDesktopWindowAdded(DesktopWindow* window) {}
	void OnDocumentWindowUrlAltered(DocumentDesktopWindow* document_window, const OpStringC& url) {}
	void OnCollectionItemMoved(DesktopWindowCollectionItem* item,
							   DesktopWindowCollectionItem* old_parent,
							   DesktopWindowCollectionItem* old_previous) {}

private:

	OpBrowserView*			m_document_view;
};

#endif // DEVTOOLS_DESKTOP_WINDOW_H
