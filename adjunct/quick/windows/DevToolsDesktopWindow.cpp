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
#include "core/pch.h"

#ifdef INTEGRATED_DEVTOOLS_SUPPORT

#include "adjunct/quick/windows/DevToolsDesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/OpBrowserView.h"
#include "modules/dochand/win.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_tools.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/dragdrop/dragdrop_manager.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/WindowCommanderProxy.h"

/***********************************************************************************
**
**	DevToolsDesktopWindow
**
***********************************************************************************/

DevToolsDesktopWindow::DevToolsDesktopWindow(OpWorkspace* parent_workspace)
	: m_document_view(NULL) // needed by GetWindowCommander called by
	                        // OnDesktopWindowAdded listener implemented in
							// TabAPIListener and called before Init returns
{
	OP_STATUS status = DesktopWindow::Init(OpWindow::STYLE_DESKTOP, parent_workspace);
	CHECK_STATUS(status);

	OpString windowtitle;
	g_languageManager->GetString(Str::D_DEVTOOLS_TITLE, windowtitle);

	OpWidget* root_widget = GetWidgetContainer()->GetRoot();

	status = OpBrowserView::Construct(&m_document_view);
	CHECK_STATUS(status);
	root_widget->AddChild(m_document_view);
	status = m_document_view->init_status;
	CHECK_STATUS(status);

	m_document_view->AddListener(this);
	OpWindowCommander* wc = GetWindowCommander();
	WindowCommanderProxy::SetType(wc, WIN_TYPE_DEVTOOLS);
	wc->SetScale(g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::Scale));
	wc->SetSSLListener(0); // never show SSL certificate dialogs, just fail if there are any problems
	wc->SetImageMode(OpDocumentListener::ALL_IMAGES); // load all images
	wc->SetLayoutMode(OpWindowCommander::NORMAL); // set normal rendering mode (disables fit-to-width)
	m_document_view->SetFocus(FOCUS_REASON_OTHER);

	OpStringC devtools_url = g_pctools->GetStringPref(PrefsCollectionTools::DevToolsUrl);
	wc->FollowURL(devtools_url.CStr(), NULL, FALSE);
	SetTitle(windowtitle.CStr());

	status = g_application->GetDesktopWindowCollection().AddListener(this);
	CHECK_STATUS(status);
}

/***********************************************************************************
**
**	~DevToolsDesktopWindow
**
***********************************************************************************/

DevToolsDesktopWindow::~DevToolsDesktopWindow()
{
	g_application->GetDesktopWindowCollection().RemoveListener(this);

	if (g_input_manager)
	{
		// State of button is not updated automatically when closing
		// devtools when in attached window
		g_input_manager->UpdateAllInputStates(FALSE);
	}
}

/***********************************************************************************
**
**	OnLayout
**
***********************************************************************************/

void DevToolsDesktopWindow::OnLayout()
{
	OpRect rect;
	GetBounds(rect);

	m_document_view->GetWindowCommander()->SetDocumentFullscreenState(IsFullscreen() ? OpWindowCommander::FULLSCREEN_NORMAL : OpWindowCommander::FULLSCREEN_NONE);
	m_document_view->SetRect(rect);
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL DevToolsDesktopWindow::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_RESTORE_PAGE:
		case OpInputAction::ACTION_ZOOM_IN:
		case OpInputAction::ACTION_RESTORE_WINDOW:
		case OpInputAction::ACTION_ZOOM_TO:
		case OpInputAction::ACTION_ZOOM_OUT:
		case OpInputAction::ACTION_MINIMIZE_PAGE:
		case OpInputAction::ACTION_MINIMIZE_WINDOW:
			return TRUE;
		case OpInputAction::ACTION_GET_TYPED_OBJECT:
		{
			if (action->GetActionData() == GetType())
			{
				action->SetActionObject(this);
				return TRUE;
			}
			break;
		}
		case OpInputAction::ACTION_ENTER_FULLSCREEN:
		{
			if( !GetWorkspace() )
			{
				if (!IsFullscreen())
				{
					LockUpdate(TRUE);
					Fullscreen(TRUE);
					LockUpdate(FALSE);
					return TRUE;
				}
				return FALSE;
			}
			break;
		}
		case OpInputAction::ACTION_LEAVE_FULLSCREEN:
		{
			if( !GetWorkspace() )
			{
				if (IsFullscreen())
				{
					LockUpdate(TRUE);
					Fullscreen(FALSE);
					LockUpdate(FALSE);
					return TRUE;
				}
				return FALSE;
			}
			break;
		}
		case OpInputAction::ACTION_CLOSE_PAGE:
		case OpInputAction::ACTION_CLOSE_WINDOW:
		{
			if (m_document_view)
			{
				OnPageClose(m_document_view->GetWindowCommander());
			}
			return TRUE;
		}
	}

	if (!action->IsLowlevelAction() && m_document_view->OnInputAction(action))
		return TRUE;

	return DesktopWindow::OnInputAction(action);
}

void DevToolsDesktopWindow::OnPageTitleChanged(OpWindowCommander* commander, const uni_char* title)
{
	SetTitle(title);
}

void DevToolsDesktopWindow::OnPageDocumentIconAdded(OpWindowCommander* commander)
{
	OpBitmap* bitmap;
	if (OpStatus::IsSuccess(GetWindowCommander()->GetDocumentIcon(&bitmap)))
	{
		Image img = imgManager->GetImage(bitmap);
		SetIcon(img);
		
		if (GetToplevelDesktopWindow()->IsDevToolsOnly())
		{
			// and set the favicon as icon of the BrowserDesktopWindow if it's a devtool only Window
			GetToplevelDesktopWindow()->SetIcon(img);
		}
		
	}
}

void DevToolsDesktopWindow::OnPageWindowAttachRequest(OpWindowCommander* commander, BOOL attached)
{
	if (attached)
	{
		// Find the topmost active window that's not devtools
		BrowserDesktopWindow *browser_window = g_application->GetActiveBrowserDesktopWindow();

		if (browser_window)
			browser_window->AttachDevToolsWindow(TRUE);
	}
	else
	{
		if (GetWindowCommander() == commander)
		{
			// Just detach the view
			DesktopWindow *desktop_window = GetToplevelDesktopWindow();

			if (desktop_window)
				desktop_window->AttachDevToolsWindow(FALSE);
		}
	}
}

void DevToolsDesktopWindow::OnPageGetWindowAttached(OpWindowCommander* commander, BOOL* attached)
{
	if (GetWindowCommander() == commander)
	{
		DesktopWindow *desktop_window = GetToplevelDesktopWindow();

		*attached = FALSE;
		if (desktop_window)
			*attached = (desktop_window->HasDevToolsWindowAttached() && !desktop_window->IsDevToolsOnly());
	}
}

void DevToolsDesktopWindow::OnPageClose(OpWindowCommander* commander)
{
	if (GetWindowCommander() == commander)
	{
		// Close the window
		Close(FALSE);
	}
}

void DevToolsDesktopWindow::OnPageRaiseRequest(OpWindowCommander* commander)
{
	if (g_drag_manager->IsDragging())
		return;

	Activate();
	m_document_view->SetFocus(FOCUS_REASON_ACTIVATE);
}

void DevToolsDesktopWindow::OnPageLowerRequest(OpWindowCommander* commander)
{
	if (g_drag_manager->IsDragging())
		return;

	DocumentDesktopWindow *ddw = g_application->GetActiveDocumentDesktopWindow();
	if (ddw) {
		ddw->Activate();
		ddw->GetBrowserView()->SetFocus(FOCUS_REASON_RELEASE);
	}
}

void DevToolsDesktopWindow::OnPageGetInnerSize(OpWindowCommander* commander, UINT32* width, UINT32* height)
{
	m_document_view->GetOpWindow()->GetInnerSize(width, height);
}

void DevToolsDesktopWindow::OnPageGetOuterSize(OpWindowCommander* commander, UINT32* width, UINT32* height)
{
	GetOuterSize(*width, *height);
}

void DevToolsDesktopWindow::OnPageGetPosition(OpWindowCommander* commander, INT32* x, INT32* y)
{
	GetOuterPos(*x, *y);
}

void DevToolsDesktopWindow::OnDesktopWindowRemoved(DesktopWindow* window)
{
	// close devtools window when last browser window is closed
	if (GetParentDesktopWindow() && GetParentDesktopWindow()->IsDevToolsOnly() && window != this &&
		g_application->GetDesktopWindowCollection().GetCount(WINDOW_TYPE_BROWSER) == 1)
		Close();
}

#endif // INTEGRATED_DEVTOOLS_SUPPORT
