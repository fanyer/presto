/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/scope/src/scope_transport.h"
#include "adjunct/desktop_scope/src/scope_desktop_window_manager.h"
#include "adjunct/desktop_pi/DesktopPopupMenuHandler.h"

#include "adjunct/desktop_scope/src/scope_widget_info.h"

#include "adjunct/m2_ui/widgets/RichTextEditor.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/managers/DesktopClipboardManager.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick/speeddial/SpeedDialThumbnail.h"
#include "adjunct/quick_toolkit/widgets/OpGroup.h"
#include "adjunct/quick/widgets/OpPagebar.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick_toolkit/widgets/QuickComposite.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick/widgets/OpAddressDropDown.h"
#include "adjunct/quick/widgets/PagebarButton.h"
#include "adjunct/quick/widgets/OpPagebar.h"
#include "adjunct/quick/widgets/OpPersonalbar.h"
#include "adjunct/desktop_pi/system_input_pi.h"

#include "modules/widgets/WidgetContainer.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/util/OpTypedObject.h"
#include "modules/widgets/OpWidget.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpDropDown.h"

// Comment it to have printfs to check timing of events
//#define _DEBUG_SCOPE_TIMING
//#define _DEBUG_SCOPE_TIMING2


/* OpScopeDesktopWindowManager */

OpScopeDesktopWindowManager::OpScopeDesktopWindowManager()
	: m_system_input_pi(0)
{
	m_last_closed.SetWindowID(0);
	m_last_activated.SetWindowID(0);

	// Create the system input object
	OpStatus::Ignore(SystemInputPI::Create(&m_system_input_pi));
}

/* virtual */
OpScopeDesktopWindowManager::~OpScopeDesktopWindowManager()
{
	OP_DELETE(m_system_input_pi);
}

void OpScopeDesktopWindowManager::OnDesktopWindowChanged(DesktopWindow* desktop_window) 
{
	// Only send the message if scope is enabled
	if (!IsEnabled())
		return;

#ifdef _DEBUG_SCOPE_TIMING
	printf("OnDesktopWindowChanged\n");
#endif // _DEBUG_SCOPE_TIMING

	// Build and send the scope event
	DesktopWindowInfo info;
	RETURN_VOID_IF_ERROR(SetDesktopWindowInfo(info, desktop_window));
	RETURN_VOID_IF_ERROR(SendOnWindowUpdated(info));
}

void OpScopeDesktopWindowManager::OnDesktopWindowActivated(DesktopWindow* desktop_window, BOOL active)
{
	// Only send the message if scope is enabled and the window is being set to active
	if (!IsEnabled() || !active)
		return;

#ifdef _DEBUG_SCOPE_TIMING
	printf("OnDesktopWindowActivated %s, %s (%d)\n", desktop_window->GetSpecificWindowName(), (desktop_window->GetTitle() ? uni_down_strdup(desktop_window->GetTitle()) : ""), desktop_window->GetID());
#endif // _DEBUG_SCOPE_TIMING

	// Build the scope event
	DesktopWindowInfo info;
	RETURN_VOID_IF_ERROR(SetDesktopWindowInfo(info, desktop_window));

	// If there is a window that closed that waits for this activate before having it's closed event sent off
	if (m_last_closed.GetWindowID() > 0)
	{
#ifdef _DEBUG_SCOPE_TIMING
		printf("Sending Delayed Close OnDesktopWindowActivated %s\n", m_last_closed.GetName().UTF8AllocL());
#endif // _DEBUG_SCOPE_TIMING

		// Now send the closed event on m_last_closed
		RETURN_VOID_IF_ERROR(SendOnWindowClosed(m_last_closed));

		// Reset m_last_closed
		m_last_closed.SetWindowID(0);
	}

	// Store the last activated in case it's sent before the closed event
	CopyDesktopWindowInfo(info, m_last_activated);

#ifdef _DEBUG_SCOPE_TIMING
	printf("Saving last activated OnDesktopWindowActivated %s\n", m_last_activated.GetName().UTF8AllocL());

	printf("OnDesktopWindowActivated Sending\n");
#endif // _DEBUG_SCOPE_TIMING

	// Send the scope event
	RETURN_VOID_IF_ERROR(SendOnWindowActivated(info));
}

void OpScopeDesktopWindowManager::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	// Kill the listener since the window is closing
	desktop_window->RemoveListener(this);
	
	// Kill the document desktop window listener if this is one
	if (desktop_window->GetType() == OpTypedObject::WINDOW_TYPE_DOCUMENT)
		static_cast<DocumentDesktopWindow *>(desktop_window)->RemoveDocumentWindowListener(this);

	// Only send the message if scope is enabled
	if (!IsEnabled())
		return;

#ifdef _DEBUG_SCOPE_TIMING
	printf("OnDesktopWindowClosing %s, %s (%d)\n", desktop_window->GetSpecificWindowName(), desktop_window->GetTitle() ? uni_down_strdup(desktop_window->GetTitle()) : "", desktop_window->GetID());
#endif // _DEBUG_SCOPE_TIMING

	// Build the scope event
	DesktopWindowInfo info;
	RETURN_VOID_IF_ERROR(SetDesktopWindowInfo(info, desktop_window));

	/**
	 * Note: 
	 * If the parent of the window (dialog) that is being closed is a dialog, 
	 * we need to wait sending the close event
	 * until after the dialog below gets the activate event 
	 * 
	 * This is to ensure that the active window is correct, because the activated
	 * event on the window beneath the closing dialog gets the activate event after
	 * this close
	 *
	 * Note: Due to the fact that Windows sends the events in a different order to 
	 * UNIX and mac we also have to check if the last activated window message was
	 * already sent. If that happens we don't want to delay sending of the 
	 * close message as the activation as already gone
	 *
	 */
	DesktopWindow* parent = desktop_window->GetParentDesktopWindow();
	if (parent && parent->IsDialog() && (!m_last_activated.GetWindowID() || (m_last_activated.GetWindowID() != (UINT32)parent->GetID())))
	{
#ifdef _DEBUG_SCOPE_TIMING
		printf("Last Activated OnDesktopWindowClosing parent: %s, %s, last_activated: %s\n", parent->GetSpecificWindowName(), parent->GetTitle() ? uni_down_strdup(parent->GetTitle()) : "", m_last_activated.GetName().UTF8AllocL());
#endif // _DEBUG_SCOPE_TIMING
		// Reset last activated
		m_last_activated.SetWindowID(0);

		// Delay sending the close until just before the window underneath is activated
		CopyDesktopWindowInfo(info, m_last_closed);
		return;
	}

	// Reset last activated
	m_last_activated.SetWindowID(0);

#ifdef _DEBUG_SCOPE_TIMING
	printf("OnDesktopWindowClosing Sending\n");
#endif // _DEBUG_SCOPE_TIMING

	// Send the scope event
	RETURN_VOID_IF_ERROR(SendOnWindowClosed(info));
}

void OpScopeDesktopWindowManager::OnDesktopWindowShown(DesktopWindow* desktop_window, BOOL shown)
{
	// Only send the message if scope is enabled and being shown
	if (!IsEnabled() || !shown)
		return;
	
#ifdef _DEBUG_SCOPE_TIMING
	printf("OnDesktopWindowShown %s, %s (%d)\n", desktop_window->GetSpecificWindowName(), (desktop_window->GetTitle() ? uni_down_strdup(desktop_window->GetTitle()) : ""), desktop_window->GetID());
#endif // _DEBUG_SCOPE_TIMING
	
	// Build and send the scope event
	DesktopWindowInfo info;
	RETURN_VOID_IF_ERROR(SetDesktopWindowInfo(info, desktop_window));
	RETURN_VOID_IF_ERROR(SendOnWindowShown(info));
}

void OpScopeDesktopWindowManager::OnDesktopWindowPageChanged(DesktopWindow* desktop_window)
{
	// Only send the message if scope is enabled
	if (!IsEnabled())
		return;
	
#ifdef _DEBUG_SCOPE_TIMING
	printf("OnDesktopWindowPageChanged %s, %s (%d)\n", desktop_window->GetSpecificWindowName(), (desktop_window->GetTitle() ? uni_down_strdup(desktop_window->GetTitle()) : ""), desktop_window->GetID());
#endif // _DEBUG_SCOPE_TIMING
	
	// Build and send the scope event
	DesktopWindowInfo info;
	RETURN_VOID_IF_ERROR(SetDesktopWindowInfo(info, desktop_window));
	RETURN_VOID_IF_ERROR(SendOnWindowPageChanged(info));
}

////////////////////////////////////////////////////////////////////////

void OpScopeDesktopWindowManager::OnMenuShown(BOOL shown, QuickMenuInfo& info)
{

	OP_NEW_DBG("OpScopeDesktopWindowManager::OnMenuShown()", "desktop-scope");

	OP_DBG(("Menu '%S' is %s", info.GetMenuId().GetMenuName().CStr(), shown?"SHOWN":"HIDDEN"));

	// Only send the message if scope is enabled
	if (!IsEnabled())
		return;

	if (shown)
		RETURN_VOID_IF_ERROR(SendOnMenuShown(info));
	else
	{
		RETURN_VOID_IF_ERROR(SendOnMenuClosed(info.GetMenuId()));
	}
}



////////////////////////////////////////////////////////////////////////

void OpScopeDesktopWindowManager::OnClose(WidgetWindow* window)
{
	// Only send the message if scope is enabled
	if (!IsEnabled())
		return;

	DesktopWidgetWindow* widget_window = static_cast<DesktopWidgetWindow*>(window);

#ifdef _DEBUG_SCOPE_TIMING2
	printf("OnWidgetWindowClose %s\n", widget_window->GetWindowName());
#endif // _DEBUG_SCOPE_TIMING
	
	DesktopWindowInfo info;
	RETURN_VOID_IF_ERROR(SetDesktopWindowInfo(info, widget_window));
	// Just send the same shown event until we decide to make a special event
	RETURN_VOID_IF_ERROR(SendOnWindowClosed(info));

	// Remove the closed window from the list
	OpStatus::Ignore(m_widget_windows.RemoveByItem(widget_window));
}

void OpScopeDesktopWindowManager::OnShow(WidgetWindow* window, BOOL shown)
{
	// Only send the message if scope is enabled
	if (!IsEnabled())
		return;
	
	DesktopWidgetWindow* widget_window = static_cast<DesktopWidgetWindow*>(window);

#ifdef _DEBUG_SCOPE_TIMING2
	printf("OnWidgetWindowShown %s, Show: %u\n", widget_window->GetWindowName(), shown);
#endif // _DEBUG_SCOPE_TIMING
	
	DesktopWindowInfo info;
	RETURN_VOID_IF_ERROR(SetDesktopWindowInfo(info, widget_window));

	if (shown)
	{
		RETURN_VOID_IF_ERROR(SendOnWindowShown(info));

		if (m_widget_windows.Find(widget_window) == -1)
			OpStatus::Ignore(m_widget_windows.Add(widget_window));
	}
	else
	{
		// We should always remove items from the list as soon as we know they
		// are not longer visible
		OpStatus::Ignore(m_widget_windows.RemoveByItem(widget_window));
	}
}


////////////////////////////////////////////////////////////////////////

void OpScopeDesktopWindowManager::OnLoadingFinished(DocumentDesktopWindow* document_window, OpLoadingListener::LoadingFinishStatus status, BOOL was_stopped_by_user)
{	
	// Only send the message if scope is enabled
	if (!IsEnabled())
		return;

	/*LOADING_SUCCESS, LOADING_REDIRECT, LOADING_COULDNT_CONNECT, LOADING_UNKNOWN*/
	if (status == OpLoadingListener::LOADING_SUCCESS || status == OpLoadingListener::LOADING_COULDNT_CONNECT)
	{
#ifdef _DEBUG_SCOPE_TIMING
		OpString url;
		document_window->GetURL(url);
		printf("OnLoadingFinished %s\n", url.UTF8AllocL());
#endif //_DEBUG_SCOPE_TIMING
		PageLoaded(document_window);
	}
}

void OpScopeDesktopWindowManager::PageLoaded(DesktopWindow* desktop_window)
{	
	// Only send the message if scope is enabled
	if (!IsEnabled())
		return;

	// Build and send the scope event
	DesktopWindowInfo info;
	RETURN_VOID_IF_ERROR(SetDesktopWindowInfo(info, desktop_window));
	RETURN_VOID_IF_ERROR(SendOnWindowPageLoaded(info));
}

////////////////////////////////////////////////////////////////////////

void OpScopeDesktopWindowManager::CopyDesktopWindowInfo(DesktopWindowInfo &from, DesktopWindowInfo &to)
{
	to.ResetEncodedSize();
	to.GetRectRef().ResetEncodedSize();

	// Copy over the data. This is her as the DesktopWindowInfo is generated code
	to.SetWindowID(from.GetWindowID());
	to.SetTitle(from.GetTitle());
	to.SetName(from.GetName());
	to.SetWindowType(from.GetWindowType());
	to.SetOnScreen(from.GetOnScreen());
	to.SetState(from.GetState());
	to.GetRectRef().SetX(from.GetRect().GetX());
	to.GetRectRef().SetY(from.GetRect().GetY());
	to.GetRectRef().SetWidth(from.GetRect().GetWidth());
	to.GetRectRef().SetHeight(from.GetRect().GetHeight());
}

////////////////////////////////////////////////////////////////////////

OpScopeDesktopWindowManager_SI::DesktopWindowInfo::DesktopWindowType OpScopeDesktopWindowManager::GetWindowType(OpTypedObject::Type type)
{
	if (type >= OpTypedObject::DIALOG_TYPE && type < OpTypedObject::DIALOG_TYPE_LAST)
		return DesktopWindowInfo::DESKTOPWINDOWTYPE_DIALOG;
	
	switch (type)
	{
		case OpTypedObject::WINDOW_TYPE_UNKNOWN: 
			return DesktopWindowInfo::DESKTOPWINDOWTYPE_UNKNOWN;
			
		case OpTypedObject::WINDOW_TYPE_DESKTOP: 
		case OpTypedObject::WINDOW_TYPE_DESKTOP_WITH_BROWSER_VIEW:
		case OpTypedObject::WINDOW_TYPE_BROWSER:
		case OpTypedObject::WINDOW_TYPE_MAIL_VIEW:
		case OpTypedObject::WINDOW_TYPE_MAIL_COMPOSE:
		case OpTypedObject::WINDOW_TYPE_HOTLIST:
		case OpTypedObject::WINDOW_TYPE_DOCUMENT:
		case OpTypedObject::WINDOW_TYPE_CHAT:
		case OpTypedObject::WINDOW_TYPE_CHAT_ROOM:
		case OpTypedObject::WINDOW_TYPE_HELP:
		case OpTypedObject::WINDOW_TYPE_UI_BUILDER:
		case OpTypedObject::WINDOW_TYPE_JAVASCRIPT_CONSOLE:
		case OpTypedObject::WINDOW_TYPE_PAGE_CYCLER:
		case OpTypedObject::WINDOW_TYPE_PANEL:
#ifdef CDK_MODE_SUPPORT
		case OpTypedObject::WINDOW_TYPE_EMULATOR:
#endif //CDK_MODE_SUPPORT
		case OpTypedObject::WINDOW_TYPE_SOURCE:
		case OpTypedObject::WINDOW_TYPE_GADGET:
		case OpTypedObject::WINDOW_TYPE_ACCESSKEY_CYCLER:
		case OpTypedObject::WINDOW_TYPE_DEVTOOLS:
			// Desktop specific:
		case OpTypedObject::WINDOW_TYPE_TOOLTIP:
		case OpTypedObject::WINDOW_TYPE_TOPLEVEL:
		case OpTypedObject::WIDGET_TYPE_WIDGETWINDOW:
			return DesktopWindowInfo::DESKTOPWINDOWTYPE_NORMAL;
			
		default:
			OP_ASSERT(!"Add new Window Type to the above list!");
			break;
	}
	
	return DesktopWindowInfo::DESKTOPWINDOWTYPE_UNKNOWN;
}

////////////////////////////////////////////////////////////////////////

OpScopeDesktopWindowManager_SI::DesktopWindowInfo::DesktopWindowState OpScopeDesktopWindowManager::GetWindowState(OpWindow::State state)
{
	switch (state) 
	{
		case OpWindow::RESTORED:
			return DesktopWindowInfo::DESKTOPWINDOWSTATE_RESTORED;

		case OpWindow::MAXIMIZED:
			return DesktopWindowInfo::DESKTOPWINDOWSTATE_MAXIMIZED;

		case OpWindow::MINIMIZED:
			return DesktopWindowInfo::DESKTOPWINDOWSTATE_MINIMIZED;

		case OpWindow::FULLSCREEN:
			return DesktopWindowInfo::DESKTOPWINDOWSTATE_FULLSCREEN;

		default:
			OP_ASSERT(!"Add new Window State to the above list!");
			break;
	}

	return DesktopWindowInfo::DESKTOPWINDOWSTATE_RESTORED;
}

////////////////////////////////////////////////////////////////////////
	
OP_STATUS OpScopeDesktopWindowManager::SetDesktopWindowInfo(DesktopWindowInfo &info, DesktopWidgetWindow *win)
{
	info.SetWindowID(win->GetID());

	RETURN_IF_ERROR(info.SetTitle(win->GetTitle()));

	{
	OpString win_name;
	RETURN_IF_ERROR(win_name.Set(win->GetWindowName()));
	RETURN_IF_ERROR(info.SetName(win_name.CStr()));
	}

	info.SetWindowType(GetWindowType(win->GetType()));

// 	// Set the rect
	INT32  x = 0;
	INT32  y = 0;
	UINT32 w = 0;
	UINT32 h = 0;

	if (win->GetWindow())
	{
		win->GetWindow()->GetOuterSize(&w, &h);
		win->GetWindow()->GetOuterPos(&x, &y);
	}

 	info.GetRectRef().SetX(x);
 	info.GetRectRef().SetY(y);
 	info.GetRectRef().SetWidth(w);
 	info.GetRectRef().SetHeight(h);

// 	// TODO: FIXME: Figure out if the window is on the screen
// 	OpScreenProperties screen_props;
// 	g_op_screen_info->GetProperties(&screen_props, win->GetWindow(), NULL);

 	// Set the onscreen flag
 	info.SetOnScreen(true); 

 	// Set the window state
 	OpRect rect_dummy;
 	OpWindow::State state = OpWindow::RESTORED;
	if (win->GetWindow())
		win->GetWindow()->GetDesktopPlacement(rect_dummy, state);
	info.SetState(GetWindowState(state));

	info.SetActive(FALSE); 
	
	return OpStatus::OK;
}
////////////////////////////////////////////////////////////////////////
	
OP_STATUS OpScopeDesktopWindowManager::SetDesktopWindowInfo(DesktopWindowInfo &info, DesktopWindow *win)
{
	info.SetWindowID(win->GetID());

	RETURN_IF_ERROR(info.SetTitle(win->GetTitle()));

	{
	OpString win_name;
	RETURN_IF_ERROR(win_name.Set(win->GetSpecificWindowName()));
	RETURN_IF_ERROR(info.SetName(win_name.CStr()));
	}
	
	info.SetWindowType(GetWindowType(win->GetType()));

	// Set the rect
	OpRect rect;
	win->GetAbsoluteRect(rect);
	info.GetRectRef().SetX(rect.x);
	info.GetRectRef().SetY(rect.y);
	info.GetRectRef().SetWidth(rect.width);
	info.GetRectRef().SetHeight(rect.height);

	// Figure out if the window is on the screen
	OpScreenProperties screen_props;
	g_op_screen_info->GetProperties(&screen_props, win->GetWindow(), NULL);

	// Set the onscreen flag
	info.SetOnScreen(screen_props.screen_rect.Contains(rect));

	// Set the window state
	OpRect rect_dummy;
	OpWindow::State state;
	win->GetOpWindow()->GetDesktopPlacement(rect_dummy, state);
	info.SetState(GetWindowState(state));

	info.SetActive(win->IsActive());
	
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
OpScopeDesktopWindowManager::OnServiceEnabled()
{
	// Kill any Widget Windows tracked in the previous session
	m_widget_windows.Clear();
	
	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////

OpScopeDesktopWindowManager_SI::QuickWidgetInfo::QuickWidgetType OpScopeDesktopWindowManager::GetWidgetType(OpWidget *widget)
{
	OpTypedObject::Type type = widget->GetType();

	switch (type)
	{
		case OpTypedObject::WIDGET_TYPE_BUTTON:
		{
			OpButton* button = static_cast<OpButton*>(widget);
			if (button->GetPropertyPage())
				return QuickWidgetInfo::QUICKWIDGETTYPE_DIALOGTAB;
			// fall through
		}

		case OpTypedObject::WIDGET_TYPE_EXPAND:
		case OpTypedObject::WIDGET_TYPE_NEW_PAGE_BUTTON:
		case OpTypedObject::WIDGET_TYPE_STATE_BUTTON:
		case OpTypedObject::WIDGET_TYPE_ZOOM_MENU_BUTTON:
		case OpTypedObject::WIDGET_TYPE_PAGEBAR_BUTTON:
		case OpTypedObject::WIDGET_TYPE_TRUST_AND_SECURITY_BUTTON:
		case OpTypedObject::WIDGET_TYPE_TOOLBAR_MENU_BUTTON:
		case OpTypedObject::WIDGET_TYPE_SHRINK_ANIMATION:
		case OpTypedObject::WIDGET_TYPE_HOTLIST_CONFIG_BUTTON:
		case OpTypedObject::WIDGET_TYPE_EXTENSION_BUTTON:
		case OpTypedObject::WIDGET_TYPE_TRASH_BIN_BUTTON:
		{
			if (widget->IsOfType(OpTypedObject::WIDGET_TYPE_PAGEBAR_BUTTON))
				return QuickWidgetInfo::QUICKWIDGETTYPE_TABBUTTON;
			else
				return QuickWidgetInfo::QUICKWIDGETTYPE_BUTTON;
		}
		case OpTypedObject::WIDGET_TYPE_CHECKBOX: 
			return QuickWidgetInfo::QUICKWIDGETTYPE_CHECKBOX;

		case OpTypedObject::WIDGET_TYPE_GROUP: 
		{
			// Dialog tabs have this GetTab set even though they are really a group
			if (static_cast<OpGroup *>(widget)->GetTab())
				return QuickWidgetInfo::QUICKWIDGETTYPE_DIALOGTAB;
			break;
		}	

		case OpTypedObject::WIDGET_TYPE_DROPDOWN_WITHOUT_EDITBOX:
		case OpTypedObject::WIDGET_TYPE_DROPDOWN: 
			return QuickWidgetInfo::QUICKWIDGETTYPE_DROPDOWN;

		case OpTypedObject::WIDGET_TYPE_EDIT: 
		case OpTypedObject::WIDGET_TYPE_MULTILINE_EDIT: 
		case OpTypedObject::WIDGET_TYPE_SEARCH_EDIT:
		case OpTypedObject::WIDGET_TYPE_COMPOSE_EDIT: 
		case OpTypedObject::WIDGET_TYPE_RICH_TEXT_EDITOR:
		case OpTypedObject::WIDGET_TYPE_BROWSERVIEW: //TODO move to a separate type
			return QuickWidgetInfo::QUICKWIDGETTYPE_EDITFIELD;

		case OpTypedObject::WIDGET_TYPE_LABEL: 
        case OpTypedObject::WIDGET_TYPE_RICHTEXT_LABEL:
	    case OpTypedObject::WIDGET_TYPE_STATUS_FIELD:
		case OpTypedObject::WIDGET_TYPE_PASSWORD_STRENGTH:
		case OpTypedObject::WIDGET_TYPE_DESKTOP_LABEL:
			return QuickWidgetInfo::QUICKWIDGETTYPE_LABEL;

		case OpTypedObject::WIDGET_TYPE_RADIOBUTTON: 
			return QuickWidgetInfo::QUICKWIDGETTYPE_RADIOBUTTON;

		case OpTypedObject::WIDGET_TYPE_SEARCH_DROPDOWN:
		case OpTypedObject::WIDGET_TYPE_RESIZE_SEARCH_DROPDOWN:
		case OpTypedObject::WIDGET_TYPE_SPEEDDIAL_SEARCH:
			return QuickWidgetInfo::QUICKWIDGETTYPE_SEARCH;

		case OpTypedObject::WIDGET_TYPE_ADDRESS_DROPDOWN:
			return QuickWidgetInfo::QUICKWIDGETTYPE_ADDRESSFIELD;
			
		case OpTypedObject::WIDGET_TYPE_TOOLBAR:			
		case OpTypedObject::WIDGET_TYPE_PAGEBAR:			
		case OpTypedObject::WIDGET_TYPE_PERSONALBAR:
		case OpTypedObject::WIDGET_TYPE_PERSONALBAR_INLINE:
		case OpTypedObject::WIDGET_TYPE_PANEL_SELECTOR:
		case OpTypedObject::WIDGET_TYPE_INFOBAR:
		case OpTypedObject::WIDGET_TYPE_FINDTEXTBAR:
			return QuickWidgetInfo::QUICKWIDGETTYPE_TOOLBAR;

		case OpTypedObject::WIDGET_TYPE_TREEVIEW:
		//case OpTypedObject::WIDGET_TYPE_LISTBOX:
			return QuickWidgetInfo::QUICKWIDGETTYPE_TREEVIEW;

		case OpTypedObject::WIDGET_TYPE_THUMBNAIL:
			return QuickWidgetInfo::QUICKWIDGETTYPE_THUMBNAIL;

		case OpTypedObject::WIDGET_TYPE_FLOW_LAYOUT:
		case OpTypedObject::WIDGET_TYPE_DYNAMIC_GRID_LAYOUT:
			return QuickWidgetInfo::QUICKWIDGETTYPE_GRIDLAYOUT;

		case OpTypedObject::WIDGET_TYPE_FLOW_LAYOUT_ITEM:
		case OpTypedObject::WIDGET_TYPE_DYNAMIC_GRID_ITEM:
			return QuickWidgetInfo:: QUICKWIDGETTYPE_GRIDITEM;

		case OpTypedObject::WIDGET_TYPE_QUICK_FIND:
			return QuickWidgetInfo::QUICKWIDGETTYPE_QUICKFIND;

		case OpTypedObject::WIDGET_TYPE_ICON:
			return QuickWidgetInfo::QUICKWIDGETTYPE_ICON;

		case OpTypedObject::WIDGET_TYPE_PROGRESSBAR:
			return QuickWidgetInfo::QUICKWIDGETTYPE_PROGRESSBAR;

		case OpTypedObject::WIDGET_TYPE_LISTBOX:
			return QuickWidgetInfo::QUICKWIDGETTYPE_LISTBOX;

		default:
			break;
	}

	return QuickWidgetInfo::QUICKWIDGETTYPE_UNKNOWN;
}


////////////////////////////////////////////////////////////////////////

OP_STATUS OpScopeDesktopWindowManager::SetQuickWidgetInfo(QuickWidgetInfo& info, OpWidget* widget)
{
	OpString name;
	RETURN_IF_ERROR(name.Set(widget->GetName()));
	RETURN_IF_ERROR(info.SetName(name));

	info.SetType(GetWidgetType(widget));

	BOOL visible = widget->IsVisible();

	// If this widget is visible check if all its parents are also visible
	if (visible)
	{
		OpWidget *parent_widget = widget;
	
		while ((parent_widget = parent_widget->GetParent()) != NULL)
		{
			visible = parent_widget->IsVisible();

			if (!visible)
				break;
		}
	}

	// Note: There are more toolbar types than this one, add if neccessary
	if (visible && (widget->GetType() == OpTypedObject::WIDGET_TYPE_TOOLBAR
	             || widget->GetType() == OpTypedObject::WIDGET_TYPE_PAGEBAR
	             || widget->GetType() == OpTypedObject::WIDGET_TYPE_PERSONALBAR
	             || widget->GetType() == OpTypedObject::WIDGET_TYPE_PERSONALBAR_INLINE
	             || widget->GetType() == OpTypedObject::WIDGET_TYPE_PANEL_SELECTOR
	             || widget->GetType() == OpTypedObject::WIDGET_TYPE_INFOBAR
	             || widget->GetType() == OpTypedObject::WIDGET_TYPE_FINDTEXTBAR))
	{
		visible = static_cast<OpToolbar*>(widget)->IsOn();
	}
	else if (visible && widget->IsOfType(OpTypedObject::WIDGET_TYPE_BUTTON))
	{
		OpGroup* page = static_cast<OpButton*>(widget)->GetPropertyPage();
		if (page != NULL)
			// Make the tab button of an inactive dialog tab invisible.
			// In QuickTabs, the contents of an inactive tab are detached from
			// the tree, hence the GetParent() check.
			visible = page->IsVisible() && page->GetParent() != NULL;
	}

	info.SetVisible(visible);


	if (widget->GetType() != OpTypedObject::WIDGET_TYPE_DROPDOWN)
	{
		info.SetValue(widget->GetValue());
	}
	else
	{
		info.SetValue(static_cast<OpDropDown*>(widget)->IsDroppedDown() ? 1 : 0);
	}


	info.SetEnabled(widget->IsEnabled());
	
	if (widget->IsEnabled() && (widget->GetType() == OpTypedObject::WIDGET_TYPE_TOOLBAR
	                         || widget->GetType() == OpTypedObject::WIDGET_TYPE_PAGEBAR
	                         || widget->GetType() == OpTypedObject::WIDGET_TYPE_PERSONALBAR
	                         || widget->GetType() == OpTypedObject::WIDGET_TYPE_PERSONALBAR_INLINE
	                         || widget->GetType() == OpTypedObject::WIDGET_TYPE_PANEL_SELECTOR
	                         || widget->GetType() == OpTypedObject::WIDGET_TYPE_INFOBAR
	                         || widget->GetType() == OpTypedObject::WIDGET_TYPE_FINDTEXTBAR))
		info.SetEnabled(static_cast<OpToolbar*>(widget)->IsOn());


	// These items are set based on the widget type
	// First set them to what is the major defaults
	OpString	text;
	OpString    visible_text;
	BOOL		default_look = FALSE;
	BOOL		focused_look = FALSE;

	OpStatus::Ignore(widget->GetText(text));

	// Set visible text to same as text for all widgets except addressfield
	if (widget->GetType() != OpTypedObject::WIDGET_TYPE_ADDRESS_DROPDOWN)
	{
		info.SetVisible_text(text);
		info.SetAdditional_text(0);
	}

	if (widget->GetType() == OpTypedObject::WIDGET_TYPE_BROWSERVIEW)
	{
		text.Set(static_cast<OpBrowserView*>(widget)->GetWindowCommander()->GetSelectedText());
	}

	if (widget->GetType() == OpTypedObject::WIDGET_TYPE_THUMBNAIL && text.IsEmpty())
	{
		OpStatus::Ignore(text.Set(static_cast<GenericThumbnail*>(widget)->GetTitle()));
	}

	if (widget->GetType() == OpTypedObject::WIDGET_TYPE_RICH_TEXT_EDITOR)
	{
		RichTextEditor* rich_text_editor = static_cast<RichTextEditor*> (widget);
		if (OpStatus::IsSuccess(rich_text_editor->GetTextEquivalent(text)))
			info.SetVisible_text(text);
		OpString html_source;
		if (OpStatus::IsSuccess(rich_text_editor->GetHTMLSource(html_source)))
			info.SetAdditional_text(html_source);

		info.SetValue(rich_text_editor->IsHTML() ? 1 : 0);
	}

	// Set the specifics based on the widget type
	switch (widget->GetType()) 
	{
		// TODO: FIXME Which ones here?
		case OpTypedObject::WIDGET_TYPE_RADIOBUTTON:
		case OpTypedObject::WIDGET_TYPE_CHECKBOX:
		case OpTypedObject::WIDGET_TYPE_BUTTON:
		case OpTypedObject::WIDGET_TYPE_EXPAND: 
		// case OpTypedObject::WIDGET_TYPE_NEW_PAGE_BUTTON:
		case OpTypedObject::WIDGET_TYPE_STATE_BUTTON:
		case OpTypedObject::WIDGET_TYPE_ZOOM_MENU_BUTTON:
		// case OpTypedObject::WIDGET_TYPE_PAGEBAR_BUTTON: 
		// case OpTypedObject::WIDGET_TYPE_TRUST_AND_SECURITY_BUTTON:
		// case OpTypedObject::WIDGET_TYPE_TOOLBAR_MENU_BUTTON:
		case OpTypedObject::WIDGET_TYPE_SHRINK_ANIMATION:
		case OpTypedObject::WIDGET_TYPE_HOTLIST_CONFIG_BUTTON:
		case OpTypedObject::WIDGET_TYPE_EXTENSION_BUTTON:
		{

			// These are only relevant for Buttons
			default_look = static_cast<OpButton *>(widget)->HasDefaultLook();
			focused_look = static_cast<OpButton *>(widget)->HasForcedFocusedLook();
		}
			break;
			
		case OpTypedObject::WIDGET_TYPE_ADDRESS_DROPDOWN:
		{
			// The new address field is sometimes cut off so we'll get all of it!
			OpStatus::Ignore(static_cast<OpAddressDropDown *>(widget)->GetFullAddress(text));

			// The visible text
			static_cast<OpAddressDropDown *>(widget)->GetShowAddress(visible_text);
			info.SetVisible_text(visible_text);

			// The highlighted thext
			OpString additional_text;
			static_cast<OpAddressDropDown *>(widget)->GetHighlightDomain(additional_text);
			info.SetAdditional_text(additional_text);
		}
			break;
			
		case OpTypedObject::WIDGET_TYPE_DROPDOWN:
		case OpTypedObject::WIDGET_TYPE_DROPDOWN_WITHOUT_EDITBOX:
			{
				// Drop downs need to get the text of the selected item
				INT32 idx = static_cast<OpDropDown *>(widget)->GetSelectedItem();
				if (text.IsEmpty())
					RETURN_IF_ERROR(text.Set(static_cast<OpDropDown *>(widget)->GetItemText(idx)));
			}
			break;
		case OpTypedObject::WIDGET_TYPE_THUMBNAIL:
			info.SetCol(MAX(static_cast<GenericThumbnail*>(widget)->GetNumber(), 0));
			break;
		default:
			break;
	}

	// Set the items we set above based on the widget type
	info.SetDefaultLook(default_look);
	info.SetFocusedLook(focused_look);
	info.SetText(text);

	OpRect rect;

	// Add hacks for widgets within widgets like tabs on dialogs
	// which are just buttons on groups
	switch (widget->GetType())
	{
		case OpTypedObject::WIDGET_TYPE_GROUP:
		{
			OpButton *tab = static_cast<OpGroup *>(widget)->GetTab();
			if (tab)
				rect = tab->GetScreenRect();
			else
				rect = widget->GetScreenRect();
			break;
		}
		default:
			rect = widget->GetScreenRect();
			break;
	}

	// Set Column on tab buttons
	if (widget->IsOfType(OpTypedObject::WIDGET_TYPE_PAGEBAR_BUTTON))
	{
		// Set position
		OpWidget* parent = GetParentToolbar(widget);
		if (parent && parent->GetType() == OpTypedObject::WIDGET_TYPE_PAGEBAR)
		{
			PagebarButton *button = static_cast<PagebarButton *>(widget);
			INT32 pos = static_cast<OpPagebar*>(parent)->GetPosition(button);
			info.SetCol(pos);
			info.SetRow(0);
		}

		if (widget->GetName().IsEmpty() || widget->GetName().Find("Tab ") == 0)
		{
			// Set Name based on position
			OpString name;
			name.AppendFormat("Tab %d", info.GetCol());
			info.SetName(name);
			
			// SetName on the parent
			OpString8 name8;
			name8.Set(name);
			widget->SetName(name8);
		}
	}

	// Set parent ---------
	OpWidget* parent = widget->GetParent();
	if (parent)
	{
		// Set position for buttons in Bookmarks bar
		if (parent->GetType() == OpTypedObject::WIDGET_TYPE_PERSONALBAR_INLINE)
		{
			INT32 position = static_cast<OpPersonalbar *>(parent)->GetPosition(widget);
			if (position != -1)
				info.SetCol(position);
			info.SetRow(0);
		}

		if (parent->GetType() == OpTypedObject::WIDGET_TYPE_SEARCH_SPECIAL_DROPDOWN ||  // It doesn't have a name
			parent->GetType() == OpTypedObject::WIDGET_TYPE_EXTENSION_SET || // This type is unknown for watir
			parent->GetType() == OpTypedObject::WIDGET_TYPE_COMPOSITE) // Always skip Composite
		{
			parent = parent->GetParent();
		}
	}
	else
	{
		// Widgets that must have their parent set:
		// Controls in toolbars, pagebar and personalbar
		// Controls within tab buttons
		// FIXME: This needs to be generalized

		parent = GetParentTab(widget);  // Pagebar buttons
		if (!parent) // If parent not already set 
		{
			// if ancestor is a toolbar, pagebar, personalbar, setParentName(toolbarname)
			parent = GetParentToolbar(widget);
		}
	}

	if (parent && parent->GetName().HasContent())
	{
		OpString parent_name;
		RETURN_IF_ERROR(parent_name.Set(parent->GetName()));
		info.SetParent(parent_name);
	}

	// Set the rect
	info.GetRectRef().SetX(rect.x);
	info.GetRectRef().SetY(rect.y);
	info.GetRectRef().SetWidth(rect.width);
	info.GetRectRef().SetHeight(rect.height);
	
	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////


/**
 *
 * For Controls inside a tabbutton, we set the parent of the control to be the tabbutton
 * GetParentTab returns the tabbutton parent of a widget, if any
 *
 */

OpWidget* OpScopeDesktopWindowManager::GetParentTab(OpWidget* widget)
{
	OpWidget* wdg = 0;
	if (widget)
	{
		if (widget->IsOfType(OpTypedObject::WIDGET_TYPE_PAGEBAR_BUTTON))
		{
			return 0;
		}

		wdg = widget->GetParent();
	}
	
	// Currently only checking immediate parent
	/*while (wdg && !wdg->IsOfType(OpTypedObject::WIDGET_TYPE_PAGEBAR_BUTTON))
	{
		wdg = wdg->GetParent();
	}*/
	
	if (wdg && wdg->IsOfType(OpTypedObject::WIDGET_TYPE_PAGEBAR_BUTTON))
		return wdg;
	else
		return 0;
}

////////////////////////////////////////////////////////////////////////

// TODO paremeterize on type (GetParentWidget(widget, Type))

/**
 * returns the toolbar that is a parent (ancestor) of widget, if any
 *
 *
 */
OpWidget* OpScopeDesktopWindowManager::GetParentToolbar(OpWidget* widget)
{
	OpWidget* wdg = 0;
	if (widget)
	{
		if (widget->GetType() == OpTypedObject::WIDGET_TYPE_TOOLBAR
		 || widget->GetType() == OpTypedObject::WIDGET_TYPE_PAGEBAR
		 || widget->GetType() == OpTypedObject::WIDGET_TYPE_PERSONALBAR
		 || widget->GetType() == OpTypedObject::WIDGET_TYPE_PERSONALBAR_INLINE
		 || widget->GetType() == OpTypedObject::WIDGET_TYPE_PANEL_SELECTOR
		 || widget->GetType() == OpTypedObject::WIDGET_TYPE_INFOBAR
		 || widget->GetType() == OpTypedObject::WIDGET_TYPE_FINDTEXTBAR)
		{
			return 0;
		}

		wdg = widget->GetParent();
	}

	while (wdg && wdg->GetType() != OpTypedObject::WIDGET_TYPE_TOOLBAR 
	           && wdg->GetType() != OpTypedObject::WIDGET_TYPE_PAGEBAR
	           && wdg->GetType() != OpTypedObject::WIDGET_TYPE_PERSONALBAR
	           && wdg->GetType() != OpTypedObject::WIDGET_TYPE_PERSONALBAR_INLINE
	           && wdg->GetType() != OpTypedObject::WIDGET_TYPE_PANEL_SELECTOR
	           && wdg->GetType() != OpTypedObject::WIDGET_TYPE_INFOBAR
	           && wdg->GetType() != OpTypedObject::WIDGET_TYPE_FINDTEXTBAR)
	{
		wdg = wdg->GetParent();
	}

	return wdg;
}

////////////////////////////////////////////////////////////////////////

/**
 * Build access string which can be used to specify the widget to desktop-watir
 * e.g. [browser.quick_window(:name, "name").]quick_button(:name, "name")
 *
 * BuildWatirAccessString appends (or if insert_first TRUE, inserts) one widget in the string access_path
 * @param widget_info - the widget info for the widget whose access specifier is to be inserted/appended
 * @param access_path - the (partial) path to append or insert this widget access part into
 *
 */
void OpScopeDesktopWindowManager::BuildWatirAccessString(QuickWidgetInfo& widget_info, OpString& access_path, BOOL insert_first)
{
	OpString access_string;
	GetWatirWidgetType(widget_info.GetType(), access_string);

	if (widget_info.GetType() == QuickWidgetInfo::QUICKWIDGETTYPE_TREEITEM && widget_info.GetText().HasContent())
	{
		access_string.AppendFormat("(:string_id, LookupStringId(%s))", widget_info.GetText().CStr());
	}
	else if (widget_info.GetType() == QuickWidgetInfo::QUICKWIDGETTYPE_TREEITEM)
	{
		access_string.AppendFormat("(:pos, [%u, %u])", widget_info.GetRow(), widget_info.GetCol());
	}
	// If button in personalbar use pos
	else if (widget_info.GetType() == QuickWidgetInfo::QUICKWIDGETTYPE_BUTTON &&
			 widget_info.GetParent().Compare("Personalbar Inline") == 0 
			 || widget_info.GetType() == QuickWidgetInfo::QUICKWIDGETTYPE_THUMBNAIL &&
			 widget_info.GetName().IsEmpty())
	{
		access_string.AppendFormat(UNI_L("(:pos, %u)"), widget_info.GetCol());
	}
	// Use name if possible
	else if (widget_info.GetName().HasContent())
	{
		access_string.AppendFormat("(:name, \"%s\")", widget_info.GetName().CStr());
	}
	else if (widget_info.GetText().HasContent())
	{
		// Tabs don't have a name
		access_string.AppendFormat(widget_info.GetType() == QuickWidgetInfo::QUICKWIDGETTYPE_TABBUTTON ?
									"(:text, \"%s\")" : "(:string_id, LookupStringId(%s))", widget_info.GetText().CStr());
	}
	else
	{
		access_string.Append("([Unnamed])");
	}
		
	// Wrap
	if (access_string.Length() > 25)
		access_string.Append("\n\t");


	if (insert_first)
	{
		access_string.Insert(0, ".");
		access_path.Insert(0, access_string);
	}
	else
	{
		access_path.Append(".");
		access_path.Append(access_string);
	}
	
}

////////////////////////////////////////////////////////////////////////

void OpScopeDesktopWindowManager::GetWatirWidgetType(QuickWidgetInfo::QuickWidgetType type, OpString& widget_type)
{
	const char *type_str = NULL;

	switch (type) 
	{
		case QuickWidgetInfo::QUICKWIDGETTYPE_BUTTON:
			type_str = "quick_button";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_CHECKBOX:
			type_str = "quick_checkbox";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_DIALOGTAB:
			type_str = "quick_dialogtab";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_DROPDOWN:
			type_str = "quick_dropdown";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_EDITFIELD:
			type_str = "quick_editfield";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_LABEL:
			type_str = "quick_label";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_RADIOBUTTON:
			type_str = "quick_radiobutton";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_ADDRESSFIELD:
			type_str = "quick_addressfield";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_SEARCH:
			type_str = "quick_searchfield";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_TOOLBAR:
			type_str = "quick_toolbar";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_TREEVIEW:
			type_str = "quick_treeview";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_TREEITEM:
			type_str = "quick_treeitem";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_TABBUTTON:
			type_str = "quick_tab";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_THUMBNAIL:
			type_str = "quick_thumbnail";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_GRIDLAYOUT:
			type_str = "quick_gridlayout";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_GRIDITEM:
			type_str = "quick_griditem";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_QUICKFIND:
			type_str = "quick_find";
			break;
		case QuickWidgetInfo::QUICKWIDGETTYPE_ICON:
			type_str = "quick_icon";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_PROGRESSBAR:
			type_str = "quick_progressbar";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_LISTBOX:
			type_str = "quick_listbox";
			break;

		//case QUICKWIDGETTYPE_UNKNOWN
		default:
			type_str = "quick_unknown";
			break;
	}

	if (type_str)
		OpStatus::Ignore(widget_type.Set(type_str));
}

////////////////////////////////////////////////////////////////////////

/**
 * 
 * Appends widget information for the widget given by widget_info (and widget) to tooltip_text
 *    and if add_to_path TRUE, adds accessor string for this widget (and parents) to watir_access_path 
 *
 *
 */

void OpScopeDesktopWindowManager::GetQuickWidgetInfoToolTip(QuickWidgetInfo &widget_info, OpString &tooltip_text, OpString &watir_access_path, BOOL add_to_path, BOOL next_parent, OpWidget *widget, BOOL skip_parents)
{
	const char *type_str = NULL;

	switch (widget_info.GetType()) 
	{
		case QuickWidgetInfo::QUICKWIDGETTYPE_BUTTON:
			// Check if this is a dialog tab's button
			if (widget && static_cast<OpButton *>(widget)->GetButtonType() == OpButton::TYPE_TAB && static_cast<OpButton *>(widget)->GetPropertyPage())
			{
				// Get the name of the control from the Group since that's what you really want to access
				if (OpStatus::IsSuccess(SetQuickWidgetInfo(widget_info, static_cast<OpButton *>(widget)->GetPropertyPage())))
				{
					// TODO: FIXME: Not sure if check_parent should be set here or not
					GetQuickWidgetInfoToolTip(widget_info, tooltip_text, watir_access_path, TRUE, FALSE, static_cast<OpButton *>(widget)->GetPropertyPage());
					return;
				}
			}
			type_str = "Button";
			break;
			
		case QuickWidgetInfo::QUICKWIDGETTYPE_CHECKBOX:
			type_str = "Checkbox";
			break;
			
		case QuickWidgetInfo::QUICKWIDGETTYPE_DIALOGTAB:
			type_str = "Dialog Tab";
			break;
			
		case QuickWidgetInfo::QUICKWIDGETTYPE_DROPDOWN:
			type_str = "Drop Down";
			break;
			
		case QuickWidgetInfo::QUICKWIDGETTYPE_EDITFIELD:
			type_str = "Edit Field";
			break;
			
		case QuickWidgetInfo::QUICKWIDGETTYPE_LABEL:
			type_str = "Label";
			break;
			
		case QuickWidgetInfo::QUICKWIDGETTYPE_RADIOBUTTON:
			type_str = "Radio Button";
			break;
			
		case QuickWidgetInfo::QUICKWIDGETTYPE_ADDRESSFIELD:
			type_str = "Address Field";
			break;
			
		case QuickWidgetInfo::QUICKWIDGETTYPE_SEARCH:
			type_str = "Search Field";
			break;
			
		case QuickWidgetInfo::QUICKWIDGETTYPE_TOOLBAR:
			type_str = "Toolbar";
			break;
			
		case QuickWidgetInfo::QUICKWIDGETTYPE_TREEVIEW:
			type_str = "Treeview";
			break;
			
		case QuickWidgetInfo::QUICKWIDGETTYPE_TREEITEM:
			type_str = "Treeview Item";
			break;
			
		case QuickWidgetInfo::QUICKWIDGETTYPE_TABBUTTON:
			type_str = "Tab Button";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_THUMBNAIL:
			type_str = "Thumbnail";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_GRIDLAYOUT:
			type_str = "GridLayout";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_GRIDITEM:
			type_str = "GridItem";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_LISTBOX:
			type_str = "Listbox";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_QUICKFIND:
			type_str = "QuickFind";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_ICON:
			type_str = "QuickIcon";
			break;

		case QuickWidgetInfo::QUICKWIDGETTYPE_PROGRESSBAR:
			type_str = "QuickProgressBar";
			break;
	}

	OpString widget_type;
	if (type_str)
	{
		OpStatus::Ignore(widget_type.Set(type_str));
	}
	else
	{
		OpStatus::Ignore(widget_type.Set("Unknown"));

		// Append the number of the OpWidget Type so we have an idea on what we have missed
		if (widget)
			widget_type.AppendFormat(" %u", widget->GetType());
	}

	// Space out the entries
	if (tooltip_text.HasContent())
		tooltip_text.Append("\n");

	tooltip_text.AppendFormat("Quick Widget Information:\n----------------------\n"
		"Name: %s\n"
		"Text: %s\n"
		"Visible text: %s\n"
		"%s text: %s\n"
		"Type: %s\n"
		"Parent: %s\n"
		"Value: =%u\n"
		"Visible: %s\n"
		"Enabled: %s\n"
		"Pos: x=%u, y=%u\n"
		"Size: width=%u, height=%u\n"
		"Ref: row=%u, col=%u\n",

		widget_info.GetName().HasContent() ? widget_info.GetName().CStr() : UNI_L("[unnamed]"),
		widget_info.GetText().HasContent() ? widget_info.GetText().CStr() : UNI_L("[no text]"),
		widget_info.GetVisible_text().HasContent() ? widget_info.GetVisible_text().CStr() : UNI_L("[no text]"),
		widget_info.GetType() == QuickWidgetInfo::QUICKWIDGETTYPE_ADDRESSFIELD ? UNI_L("Highlighted") : UNI_L("Additional"),
		widget_info.GetAdditional_text().HasContent() ? widget_info.GetAdditional_text().CStr() : UNI_L("[no text]"),
		widget_type.CStr(),
		widget_info.GetParent().HasContent() ? widget_info.GetParent().CStr() : UNI_L("[no parent]"),
		widget_info.GetValue(),
		widget_info.GetVisible() ? UNI_L("Yes") : UNI_L("No"),
		widget_info.GetEnabled() ? UNI_L("Yes") : UNI_L("No"),
		widget_info.GetRect().GetX(), widget_info.GetRect().GetY(),
		widget_info.GetRect().GetWidth(), widget_info.GetRect().GetHeight(),
		widget_info.GetRow(), widget_info.GetCol());

	// Add all the parent widgets
	if (widget && !skip_parents)
	{
		OpWidget *parent_widget = widget;

		OpString parent_name;
		parent_name.Set(widget_info.GetParent());

		BOOL next_parent = FALSE;

		while ((parent_widget = parent_widget->GetParent()) != NULL)
		{
			QuickWidgetInfo parent_info;
			if (OpStatus::IsSuccess(SetQuickWidgetInfo(parent_info, parent_widget)))
			{
				// If we have hit an unknown widget we have gone far enough
				if (parent_info.GetType() != QuickWidgetInfo::QUICKWIDGETTYPE_UNKNOWN)
				{
					// If you get here, meaning you will print the parent, say you want to print it to access path only if it's set
					// as my parent
					BOOL add_parent_to_path = FALSE;
					if (parent_info.GetName().HasContent() && parent_name.Compare(parent_info.GetName()) == 0
						|| parent_info.GetText().HasContent() && parent_name.Compare(parent_info.GetText()) == 0)
						{
							add_parent_to_path = TRUE;
						}

					GetQuickWidgetInfoToolTip(parent_info, tooltip_text, watir_access_path, add_parent_to_path, next_parent, parent_widget, /* skip_parents */TRUE);
					if (add_parent_to_path)
						next_parent = TRUE;

					// If the parent was set as my parent and was added to path, then reset parent_name to be my parent's parent
					if (add_parent_to_path)
						parent_name.Set(parent_info.GetParent());
				}
			}
		}
	}

	// TODO: Add Text for some widgets instead
	if (add_to_path)
	{
		// Here set the watir_access_path for this widget
		// If this is a 'next_parent' then insert_first, else append
		BuildWatirAccessString(widget_info, watir_access_path, next_parent /* insert_first*/);
	}
}

////////////////////////////////////////////////////////////////////////

void OpScopeDesktopWindowManager::GetQuickWidgetInfoToolTip(OpWidget *widget, OpString &tooltip_text)
{
	OpString watir_access_path;

	OpString subitem_access;

	// First check if this widget needs to be broken down into QuickWidgetInfo items
	OpAutoPtr<OpScopeWidgetInfo> scope_widget_info (widget->CreateScopeWidgetInfo());
	QuickWidgetInfoList quick_widget_info_list;
	
	if (scope_widget_info.get() &&
		OpStatus::IsSuccess(scope_widget_info->AddQuickWidgetInfoItems(quick_widget_info_list, TRUE, FALSE)))
	{

		QuickWidgetInfo *widget_info_item;
		
		// Get the current mouse position
		OpPoint mouse_position = g_input_manager->GetMousePosition();

		// Find the item that the mouse is over and report that back
		for (UINT32 i = 0; i < quick_widget_info_list.GetQuickwidgetListRef().GetCount(); i++)
		{
			widget_info_item = quick_widget_info_list.GetQuickwidgetListRef().Get(i);

			OpRect widget_rect;
			widget_rect.Set(widget_info_item->GetRect().GetX(), widget_info_item->GetRect().GetY(), widget_info_item->GetRect().GetWidth(), widget_info_item->GetRect().GetHeight());

			// Found the item so set it and move on to the widget itself
			if (widget_rect.Contains(mouse_position))
			{
				// This is a sub-item of the item below so we set add_to_path to false 
				// and add this to the access path after GetQuickWidgetInfoToolTip for the item below is called
				GetQuickWidgetInfoToolTip(*widget_info_item, tooltip_text, watir_access_path, FALSE, FALSE);

				BuildWatirAccessString(*widget_info_item, subitem_access, FALSE);

				tooltip_text.Append("\n");
				break;
			}
		}
	}

	QuickWidgetInfo widget_info;

	// Skip composite
 	if (widget->GetType() == OpTypedObject::WIDGET_TYPE_COMPOSITE)
 	{
 		widget = widget->GetParent();
 		if (widget->GetType() == OpTypedObject::WIDGET_TYPE)
 			widget = widget->GetParent();
 	}

	// If EditField inside OpDropDown -> Skip the editfield
	if (widget->GetType() == OpTypedObject::WIDGET_TYPE_EDIT && widget->GetParent() &&
		widget->GetParent()->GetType() == OpTypedObject::WIDGET_TYPE_DROPDOWN &&
		widget->GetName().IsEmpty())
		{
			widget = widget->GetParent();

		}
	// If EditField inside QuickFind -> Skip the editfield
	else if (widget->GetType() == OpTypedObject::WIDGET_TYPE_EDIT && widget->GetParent() &&
		widget->GetParent()->GetType() == OpTypedObject::WIDGET_TYPE_QUICK_FIND)
	{
		widget = widget->GetParent();
	}
	// Hack to get the addressfield and searchfields correct.
	else if (widget->GetType() == OpTypedObject::WIDGET_TYPE_EDIT && widget->GetParent() &&
		widget->GetParent()->GetType() == OpTypedObject::WIDGET_TYPE_ADDRESS_DROPDOWN &&
		widget->GetName().IsEmpty())
		{
			// Basically, skip me, take my parent instead:
			widget = widget->GetParent();
		}
	    else if ((widget->GetType() == OpTypedObject::WIDGET_TYPE_SEARCH_EDIT || widget->GetType() == OpTypedObject::WIDGET_TYPE_EDIT)
			 && widget->GetParent() && //widget->GetParent()->GetType() == OpTypedObject::WIDGET_TYPE_SEARCH_DROPDOWN &&
			 widget->GetName().IsEmpty())
		{
			
			// Skip me, my parent and its parent ... duh
			OpWidget* special = widget->GetParent();

			if (special->GetParent() && special->GetParent()->GetType() == OpTypedObject::WIDGET_TYPE_SEARCH_SPECIAL_DROPDOWN)
			{

				if (special->GetParent() && special->GetParent()->GetType() == OpTypedObject::WIDGET_TYPE_SEARCH_SPECIAL_DROPDOWN) 
				{
					special = special->GetParent();
					widget = special->GetParent();
				}
			}
		}
		//Skip the children of OpPasswordStrength()
		else if (widget->GetParent() && widget->GetParent()->GetType() == OpTypedObject::WIDGET_TYPE_PASSWORD_STRENGTH)
		{
			widget = widget->GetParent();
		}

	
	if (widget && widget->GetType() != OpTypedObject::WIDGET_TYPE_ROOT)
	{
		// Convert the widget info to quick widget info
		RETURN_VOID_IF_ERROR(SetQuickWidgetInfo(widget_info, widget));
		
		// TODO: FIXME. check_parent doesn't need to be set here although this might be for example a treeview
		// that is the parent of a treeitem
		GetQuickWidgetInfoToolTip(widget_info, tooltip_text, watir_access_path, TRUE, FALSE, widget);
	}

	// Now add the path to the subitem, if any
	watir_access_path.Append(subitem_access);

	// Add information about which window the widget is in
	DesktopWindow *dw = widget->GetParentDesktopWindow();
	if (dw)
	{
		DesktopWindowInfo window_info;
			
		if (OpStatus::IsSuccess(SetDesktopWindowInfo(window_info, dw)))
		{
			const char *temp_str;
			switch (window_info.GetState()) 
			{
				case DesktopWindowInfo::DESKTOPWINDOWSTATE_RESTORED:
					temp_str = "Restored";
					break;
						
				case DesktopWindowInfo::DESKTOPWINDOWSTATE_MINIMIZED:
					temp_str = "Minimized";
					break;

				case DesktopWindowInfo::DESKTOPWINDOWSTATE_MAXIMIZED:
					temp_str = "Maximized";
					break;

				case DesktopWindowInfo::DESKTOPWINDOWSTATE_FULLSCREEN:
					temp_str = "Fullscreen";
					break;
						
				default:
					temp_str = "Unknown State";
					break;
			}

			OpString window_state;
			OpStatus::Ignore(window_state.Set(temp_str));

			temp_str = "Unknown";
			switch (window_info.GetWindowType()) 
			{
				case DesktopWindowInfo::DESKTOPWINDOWTYPE_DIALOG:
					temp_str = "Dialog";
					break;

				case DesktopWindowInfo::DESKTOPWINDOWTYPE_NORMAL:
					temp_str = "Normal";
					// fall through
				default:
				{
					OpString path;
					path.AppendFormat(".quick_window(:name, \"%s\")", window_info.GetName().HasContent() ? window_info.GetName().CStr() : UNI_L("[unnamed]"));
					watir_access_path.Insert(0, path);
				}
					break;
			}

			OpString window_type;
			OpStatus::Ignore(window_type.Set(temp_str));

			// Add some space and a header
			tooltip_text.AppendFormat("\nQuick Window Information:\n----------------------\n"
				"ID: %u\n"
				"Name: %s\n"
				"Title: %s\n"
				"Type: %s\n"
				"State: %s\n"
				"Pos: x=%u, y=%u\n"
				"Size: width=%u, height=%u\n",
				window_info.GetWindowID(),
				window_info.GetName().HasContent() ? window_info.GetName().CStr() : UNI_L("[unnamed]"),
				window_info.GetTitle().HasContent() ? window_info.GetTitle().CStr() : UNI_L("[no title]"),
				window_type.CStr(),
				window_state.CStr(),
				window_info.GetRect().GetX(), window_info.GetRect().GetY(),
				window_info.GetRect().GetWidth(), window_info.GetRect().GetHeight());
		}
	}
	else // Parent window is a DesktopWidgetWindow, not a DesktopWindow
	{
		// TODO: Do we need the window in the path (in some cases?)?
	}

	watir_access_path.Insert(0, "browser");
	watir_access_path.Append("\n\n");
	tooltip_text.Insert(0, watir_access_path);

	// If this is not in a Browser Window, add additional access path as alternative
	int pos = watir_access_path.Find("quick_window(:name, \"Browser Window\")");
	if (pos == KNotFound) 
	{
		int winpos = watir_access_path.Find(".quick_window(");
		if (pos != KNotFound)
		{
			int endparen = watir_access_path.Find(")", winpos);
			int length = endparen - winpos;
			if (length > 0)
			{
				OpString alternative_access_path;
				alternative_access_path.Set(watir_access_path, 7); // 'browser'
				alternative_access_path.Append(watir_access_path.CStr() + winpos + length + 1);
				tooltip_text.Insert(0, alternative_access_path);
			}
		}
	}

	// Put the text into the clipboard so it can be used without having to type again
	if (((g_op_system_info->GetShiftKeyState() & SHIFTKEY_ALT) == SHIFTKEY_ALT) &&
		((g_op_system_info->GetShiftKeyState() & SHIFTKEY_SHIFT) == SHIFTKEY_SHIFT))
		OpStatus::Ignore(g_desktop_clipboard_manager->PlaceText(tooltip_text.CStr()));
	else if ((g_op_system_info->GetShiftKeyState() & SHIFTKEY_ALT) == SHIFTKEY_ALT)
		OpStatus::Ignore(g_desktop_clipboard_manager->PlaceText(watir_access_path.CStr()));

}

////////////////////////////////////////////////////////////////////////

OP_STATUS OpScopeDesktopWindowManager::MenuItemPressed(const OpStringC& menu_text, BOOL popmenu)
{
	// Build the menu item id
	QuickMenuItemID menu_item_id;
	RETURN_IF_ERROR(menu_item_id.SetMenuText(menu_text.CStr()));
	menu_item_id.SetPopupMenu(popmenu);
	
	// Send the event
	return SendOnMenuItemPressed(menu_item_id);
}

////////////////////////////////////////////////////////////////////////

OP_STATUS OpScopeDesktopWindowManager::DoGetActiveWindow(DesktopWindowID &out)
{
	DesktopWindow* window = g_application->GetDesktopWindowCollection().GetCurrentInputWindow();

	if (!window)
		return SetCommandError(OpScopeTPHeader::InternalError, UNI_L("No active desktop window found"));

	out.SetWindowID(window->GetID());

	return OpStatus::OK;
}

OP_STATUS OpScopeDesktopWindowManager::DoListWindows(DesktopWindowList &out)
{
	OpVector<DesktopWindow> windows;
	RETURN_IF_ERROR(g_application->GetDesktopWindowCollection().GetDesktopWindows(windows));

	for (UINT32 i = 0; i < windows.GetCount(); i++)
	{
		DesktopWindow* window = windows.Get(i);
		if (!window)
			return SetCommandError(OpScopeTPHeader::InternalError, UNI_L("Desktop window not found"));

		OpAutoPtr<DesktopWindowInfo> info(OP_NEW(DesktopWindowInfo, ()));
		if (info.get() == NULL)
			return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(SetDesktopWindowInfo(*info.get(), window));
		RETURN_IF_ERROR(out.GetWindowListRef().Add(info.get()));
		info.release();
	}

	for (UINT32 i = 0; i < m_widget_windows.GetCount(); i++)
	{
		DesktopWidgetWindow* window = m_widget_windows.Get(i);
		if (!window)
			return SetCommandError(OpScopeTPHeader::InternalError, UNI_L("Desktop window not found"));

		OpAutoPtr<DesktopWindowInfo> info(OP_NEW(DesktopWindowInfo, ()));
		if (info.get() == NULL)
			return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(SetDesktopWindowInfo(*info.get(), window));
		RETURN_IF_ERROR(out.GetWindowListRef().Add(info.get()));
		info.release();

	}

	return OpStatus::OK;
}

DesktopWidgetWindow* OpScopeDesktopWindowManager::GetDesktopWidgetWindow(INT32 id)
{
	for (int i = 0; i < (int)m_widget_windows.GetCount(); i++)
	{
		DesktopWidgetWindow* win = (DesktopWidgetWindow*) m_widget_windows.Get(i);
		if (win && win->GetID() == id)
			return win;
	}
	return NULL;
}


OP_STATUS OpScopeDesktopWindowManager::DoPressQuickMenuItem(const QuickMenuItemID &in)
{
	m_system_input_pi->PressMenu(in.GetMenuText(), in.GetPopupMenu() ? true : false);
	return OpStatus::OK;
}

OP_STATUS OpScopeDesktopWindowManager::DoListQuickWidgets(const DesktopWindowID &in, QuickWidgetInfoList &out)
{

	OP_STATUS status = OpStatus::OK;
	OpVector<OpWidget> widgets;

	DesktopWindow* window = g_application->GetDesktopWindowCollection().GetDesktopWindowByID(in.GetWindowID());

	// If it's not in the desktopwindowcollection, it might be a DesktopWidgetWindow
	if (!window)
	{
		DesktopWidgetWindow* win = GetDesktopWidgetWindow(in.GetWindowID());
		if (!win)
			return SetCommandError(OpScopeTPHeader::InternalError, UNI_L("Desktop window not found"));
		status = win->ListWidgets(widgets);
	}
	else
	{
		status =  window->ListWidgets(widgets);
	}

	if (OpStatus::IsSuccess(status))
	{
		for (UINT32 i = 0; i < widgets.GetCount(); i++)
		{
			OpWidget* widget = widgets.Get(i);
			OpAutoPtr<QuickWidgetInfo> info(OP_NEW(QuickWidgetInfo, ()));
			if (info.get() == NULL)
				return OpStatus::ERR_NO_MEMORY;
			RETURN_IF_ERROR(SetQuickWidgetInfo(*info.get(), widget));
			RETURN_IF_ERROR(out.GetQuickwidgetListRef().Add(info.get()));
			info.release();
			
			// Add any further breakup of the widget that may be required to 
			// describe it to the UI automation system
			OpAutoPtr<OpScopeWidgetInfo> widget_info (widget->CreateScopeWidgetInfo());
			if (widget_info.get())
				RETURN_IF_ERROR(widget_info->AddQuickWidgetInfoItems(out, FALSE, TRUE));
		}
	}

	return OpStatus::OK;
	
}

OP_STATUS OpScopeDesktopWindowManager::GetWidgetInWidgetWindow(DesktopWidgetWindow* window, const QuickWidgetSearch& in, QuickWidgetInfo& out)
{
	if (!window) return OpStatus::ERR;

	OpWidget* widget = NULL;
	
	if (in.GetSearchType() == QuickWidgetSearch::QUICKWIDGETSEARCHTYPE_NAME) 
	{		
		OpString8 name;
		name.Set(in.GetData().CStr());
		widget = window->GetWidgetByName(name); // label_for_blah

	}
	else if (in.GetSearchType() == QuickWidgetSearch::QUICKWIDGETSEARCHTYPE_TEXT) 
	{		
		OpString8 name;
		name.Set(in.GetData().CStr());
		widget = window->GetWidgetByText(name); 
	}
	else
	{
		return SetCommandError(OpScopeTPHeader::InternalError, UNI_L("Quick widget search type not supported"));
	}
	
	// Make sure we actually found a widget
	if (!widget)
	{
		return SetCommandError(OpScopeTPHeader::InternalError, UNI_L("Quick widget not found"));
	}
	
	RETURN_IF_ERROR(SetQuickWidgetInfo(out, widget));
	
	return OpStatus::OK;
}	


/**
 * OBS: OBS: This isn't used currently as we rather send the whole widget list to webdriver, and search there.
 * However, if this should be used, note that the name of tab buttons is not stored in widget->m_name (retrieved
 * by widget->GetName() but generated from it's position, so searching for a tabbutton by name
 * without modifying this won't work
 *
 **/
OP_STATUS OpScopeDesktopWindowManager::DoGetQuickWidget(const QuickWidgetSearch& in, QuickWidgetInfo& out)
{
	UINT32 window_id = in.GetWindowID().GetWindowID();
	DesktopWindow* window = g_application->GetDesktopWindowCollection().GetDesktopWindowByID(window_id);
	if (!window)
	{
		DesktopWidgetWindow* win = GetDesktopWidgetWindow(window_id);
		if (win)
			return GetWidgetInWidgetWindow(win, in, out);

		return SetCommandError(OpScopeTPHeader::InternalError, UNI_L("Desktop window not found"));
	}

	OpWidget* widget = NULL;
	
	if (in.GetSearchType() == QuickWidgetSearch::QUICKWIDGETSEARCHTYPE_NAME) 
	{		
		OpString8 name;
		name.Set(in.GetData().CStr());
		widget = window->GetWidgetByName(name); // label_for_blah

	}
	else if (in.GetSearchType() == QuickWidgetSearch::QUICKWIDGETSEARCHTYPE_TEXT) 
	{		
		OpString8 name;
		name.Set(in.GetData().CStr());
		widget = GetWidgetByText(window, name); 
	}
	else
	{
		return SetCommandError(OpScopeTPHeader::InternalError, UNI_L("Quick widget search type not supported"));
	}
	
	// Make sure we actually found a widget
	if (!widget)
	{
		return SetCommandError(OpScopeTPHeader::InternalError, UNI_L("Quick widget not found"));
	}
	
	RETURN_IF_ERROR(SetQuickWidgetInfo(out, widget));
	
	return OpStatus::OK;
}


OP_STATUS OpScopeDesktopWindowManager::DoListQuickMenus(QuickMenuList &out)
{
	/**
	 * First get all menubars. This is done through quick, as the menubars need to be tied to the windowId of the BrowserDesktopWindow
	 */
#ifndef _MACINTOSH_
	if (g_pcui->GetIntegerPref(PrefsCollectionUI::ShowMenu))
	{
		OpVector<DesktopWindow> main_windows;
		if (OpStatus::IsSuccess(g_application->GetDesktopWindowCollection().GetDesktopWindows(OpTypedObject::WINDOW_TYPE_BROWSER, main_windows)))
		{
			for (UINT32 i = 0; i < main_windows.GetCount(); i++)
			{
				BrowserDesktopWindow* win = static_cast<BrowserDesktopWindow*>(main_windows.Get(i));

				if (win)
				{
					OpAutoPtr<QuickMenuInfo> info(OP_NEW(QuickMenuInfo, ()));
					if (info.get() == NULL)
						return OpStatus::ERR_NO_MEMORY;
					
					if (win->GetMenubarQuickMenuInfoItems(*info.get()))
					{
						RETURN_IF_ERROR(out.GetMenuListRef().Add(info.get()));
						info.release();
					}
				}
			}
		}
	}
#endif // _MACINTOSH_

	// Get PopupMenus
	/*bool res = */g_desktop_popup_menu_handler->ListPopupMenuInfo(out);
	return OpStatus::OK;
	
}

// This is done on the Java side instead
OpWidget* OpScopeDesktopWindowManager::GetWidgetByText(DesktopWindow* window, OpString8& text)
{
	if (!window || text.IsEmpty()) 
		return NULL;

	OpVector<OpWidget> widgets;
	if (OpStatus::IsSuccess(window->ListWidgets(widgets)))
	{
		for (UINT32 i = 0; i < widgets.GetCount(); i++)
		{
			OpWidget* widget = widgets.Get(i);
			OpString wdg_text;
			if (widget && OpStatus::IsSuccess(widget->GetText(wdg_text)) && wdg_text.HasContent())
			{
				OpString8 widget_text;
				widget_text.Set(wdg_text);
				if (widget_text.Compare(text) == 0)
				{
					return widget;
				}
			}
		}
	}
	return NULL;
}
