/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 2010 Opera Software AS.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 */

#ifndef SCOPE_DESKTOP_WINDOW_MANAGER_H
#define SCOPE_DESKTOP_WINDOW_MANAGER_H

#include "modules/scope/src/scope_service.h"
#include "adjunct/desktop_scope/src/generated/g_scope_desktop_window_manager_interface.h"
#include "adjunct/quick/windows/DocumentWindowListener.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/DesktopWidgetWindow.h"


class OpWidget;
class SystemInputPI;

/*
class OpScopeDesktopWidgetLister
{
public:
virtual OP_STATUS ListWidgets(QuickWidgetInfoList &out);
};
*/

class OpScopeDesktopWindowManager :
  public OpScopeDesktopWindowManager_SI
, public DesktopWindowListener
, public DocumentWindowListener
, public WidgetWindow::Listener
{
public:
	
	OpScopeDesktopWindowManager();
	virtual ~OpScopeDesktopWindowManager();

	// DesktopWindow::Listener 
	virtual void	OnDesktopWindowChanged(DesktopWindow* desktop_window);
	virtual void	OnDesktopWindowActivated(DesktopWindow* desktop_window, BOOL active);
	virtual void	OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);
	virtual void	OnDesktopWindowShown(DesktopWindow* desktop_window, BOOL shown);
	virtual void	OnDesktopWindowPageChanged(DesktopWindow* desktop_window);

	// DesktopMenuHandler
	void OnMenuShown(BOOL shown, QuickMenuInfo& info);

	// DocumentWindowListener 
	virtual void	OnLoadingFinished(DocumentDesktopWindow* document_window, OpLoadingListener::LoadingFinishStatus status, BOOL was_stopped_by_user = FALSE);

	// WidgetWindow::Listener
    virtual void	OnClose(WidgetWindow* window);
    virtual void	OnShow(WidgetWindow* window, BOOL shown);

	// OpScopeService
	virtual OP_STATUS OnServiceEnabled();
	
	// Puts all the widget info into a string to display in a tooltip
	void GetQuickWidgetInfoToolTip(OpWidget *widget, OpString &tooltip_text);

	// Called when a menu item is pressed
	OP_STATUS MenuItemPressed(const OpStringC& menu_text, BOOL popmenu = FALSE);
	
	SystemInputPI *GetSystemInputPI() { return m_system_input_pi; }

	void PageLoaded(DesktopWindow* desktop_window);

	// Request/Response functions
	OP_STATUS DoGetActiveWindow(DesktopWindowID &out);
	OP_STATUS DoListWindows(DesktopWindowList &out);
	OP_STATUS DoListQuickWidgets(const DesktopWindowID &in, QuickWidgetInfoList &out);
	OP_STATUS DoGetQuickWidget(const QuickWidgetSearch &in, QuickWidgetInfo &out);
		OP_STATUS DoListQuickMenus(QuickMenuList &out);
		OP_STATUS DoPressQuickMenuItem(const QuickMenuItemID &in);

	OpScopeDesktopWindowManager_SI::QuickWidgetInfo::QuickWidgetType		GetWidgetType(OpWidget *widget);

private:
	void GetQuickWidgetInfoToolTip(QuickWidgetInfo &widget_info, OpString &tooltip_text, OpString &watir_access_path, BOOL add_to_path, BOOL next_parent, OpWidget *widget = NULL, BOOL skip_parents = FALSE);

	void BuildWatirAccessString(QuickWidgetInfo& widget_info, OpString& access_path, BOOL insert_first);
	void GetWatirWidgetType(QuickWidgetInfo::QuickWidgetType type, OpString& widget_type);
	
	// Sets the data in the proto message structure from the Opera opject
	OP_STATUS SetQuickWidgetInfo(QuickWidgetInfo& info, OpWidget* wdg);
	OP_STATUS SetDesktopWindowInfo(DesktopWindowInfo &info, DesktopWindow *win);
	OP_STATUS SetDesktopWindowInfo(DesktopWindowInfo &info, DesktopWidgetWindow *win);
	
	// Convert between the Opera enum and the proto enums
	OpScopeDesktopWindowManager_SI::DesktopWindowInfo::DesktopWindowType	GetWindowType(OpTypedObject::Type type);
	OpScopeDesktopWindowManager_SI::DesktopWindowInfo::DesktopWindowState	GetWindowState(OpWindow::State state);


	OpWidget* GetWidgetByText(DesktopWindow* window, OpString8& text);
	OpWidget* GetParentToolbar(OpWidget* widget);
	OpWidget* GetParentTab(OpWidget* widget);

	DesktopWidgetWindow* GetDesktopWidgetWindow(INT32 id);
	OP_STATUS GetWidgetInWidgetWindow(DesktopWidgetWindow* window, const QuickWidgetSearch& in, QuickWidgetInfo& out);

	void CopyDesktopWindowInfo(DesktopWindowInfo &from, DesktopWindowInfo &to);

	DesktopWindowInfo m_last_closed;
	DesktopWindowInfo m_last_activated;

	// All open DesktopWidgetWindows are referenced in this collection
	// Listing windows lists the windows in the DesktopWindowCollection pluss these 
	OpVector<DesktopWidgetWindow> m_widget_windows;

	SystemInputPI *m_system_input_pi;
	
};

#endif // SCOPE_DESKTOP_WINDOW_MANAGER_H
