/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Author: Johannes Hoff
** Documentation: ../documentation/window-manager.html
*/

#include "core/pch.h"

#ifdef SCOPE_WINDOW_MANAGER_SUPPORT

#include "modules/scope/src/scope_window_manager.h"
#include "modules/scope/src/scope_manager.h"
#include "modules/scope/src/scope_ecmascript_debugger.h"

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/doc/frm_doc.h"

#ifdef EXTENSION_SUPPORT
# include "modules/gadgets/OpGadget.h"
#endif // EXTENSION_SUPPORT

//#define SCOPE_WINDOW_MANAGER_DEBUG

OpScopeWindowManager::OpScopeWindowManager()
	: OpScopeWindowManager_SI()
	, include_all(FALSE)
{
}

/* virtual */
OpScopeWindowManager::~OpScopeWindowManager()
{
}

OP_STATUS
OpScopeWindowManager::Construct()
{
	RETURN_IF_ERROR(OpScopeService::Construct());
	return SetDefaultTypeFilter();
}

const uni_char*
GetWindowTypeName(Window_Type type)
{
	switch (type)
	{
		case WIN_TYPE_NORMAL: return UNI_L("normal");
		case WIN_TYPE_DOWNLOAD: return UNI_L("download");
		case WIN_TYPE_CACHE: return UNI_L("cache");
		case WIN_TYPE_PLUGINS: return UNI_L("plugins");
		case WIN_TYPE_HISTORY: return UNI_L("history");
		case WIN_TYPE_HELP: return UNI_L("help");
		case WIN_TYPE_MAIL_VIEW: return UNI_L("mail_view");
		case WIN_TYPE_MAIL_COMPOSE: return UNI_L("mail_compose");
		case WIN_TYPE_NEWSFEED_VIEW: return UNI_L("newsfeed_view");
		case WIN_TYPE_IM_VIEW: return UNI_L("im_view");
		case WIN_TYPE_P2P_VIEW: return UNI_L("p2p_view");
		case WIN_TYPE_BRAND_VIEW: return UNI_L("brand_view");
		case WIN_TYPE_PRINT_SELECTION: return UNI_L("print_selection");
		case WIN_TYPE_JS_CONSOLE: return UNI_L("js_console");
		case WIN_TYPE_GADGET: return UNI_L("gadget");
		case WIN_TYPE_CONTROLLER: return UNI_L("controller");
		case WIN_TYPE_INFO: return UNI_L("info");
		case WIN_TYPE_DIALOG: return UNI_L("dialog");
		case WIN_TYPE_THUMBNAIL: return UNI_L("thumbnail");
		case WIN_TYPE_DEVTOOLS: return UNI_L("devtools");
		case WIN_TYPE_NORMAL_HIDDEN: return UNI_L("normal_hidden");
	}
	OP_ASSERT(!"Add new Window_Type to the above list!");
	return UNI_L("UNKNOWN");
}

#ifdef SCOPE_WINDOW_MANAGER_DEBUG
void
DisplayWindowInfo(Window* win)
{
	dbg_printf("'%S' (%d)\n", win->GetWindowTitle() ? win->GetWindowTitle() : UNI_L(""), win->Id());
	dbg_printf("     Opened windows:\n");
	for (Window* win = g_windowManager->FirstWindow(); win; win = (Window*)win->Suc())
	{
		const uni_char* title = win->GetWindowTitle();
		dbg_printf("     - Window '%S' (id = %d, type = %S, opener = %d)\n", title ? title : UNI_L(""), win->Id(), GetWindowTypeName(win->GetType()), win->GetOpener() ? win->GetOpener()->GetWindow()->Id() : 0);
	}
}
#endif

/* callback */
void
OpScopeWindowManager::NewWindow(Window *win)
{
#ifdef SCOPE_WINDOW_MANAGER_DEBUG
	dbg_printf("SCP: New window: ");
	DisplayWindowInfo(win);
#endif

	if (!IsEnabled())
		return;
	if (!IsAcceptedType(win->GetType()))
		return;
	WindowInfo info;
	RETURN_VOID_IF_ERROR(SetWindowInfo(info, win));
	RETURN_VOID_IF_ERROR(SendOnWindowUpdated(info));
	// TODO: How to handle errors
}

/* callback */
void
OpScopeWindowManager::WindowRemoved(Window *win)
{
#ifdef SCOPE_WINDOW_MANAGER_DEBUG
	dbg_printf("SCP: Window removed: ");
	DisplayWindowInfo(win);
#endif
	g_scope_manager->WindowClosed(win);

	OpStatus::Ignore(window_activations.RemoveByItem(win));

	if (!IsEnabled())
		return;
	WindowID w;
	w.SetWindowID(win->Id());
	RETURN_VOID_IF_ERROR(SendOnWindowClosed(w));
	// TODO: How to handle errors
}

/**
 * Resets filters of included and excluded window ID's.
 *
 * It is up to the users of this function to call g_scope_manager->FilterChanged() after
 * all possible additional changes are done to the filter. (It's expensive to call, so it
 * should not be done more than necessary).
 */
void
OpScopeWindowManager::ClearWindowIdFilters()
{
	included_windows.Clear();
	excluded_windows.Clear();
	include_all = FALSE;
}

/* virtual */
OP_STATUS
OpScopeWindowManager::OnServiceEnabled()
{
	ClearWindowIdFilters();
	RETURN_IF_ERROR(SetDefaultTypeFilter());
	g_scope_manager->FilterChanged();

	return OpStatus::OK;
}

/* callback */
void
OpScopeWindowManager::WindowTitleChanged(Window *win)
{
#ifdef SCOPE_WINDOW_MANAGER_DEBUG
	dbg_printf("SCP: Window changed title: ");
	DisplayWindowInfo(win);
#endif

	if (!IsEnabled())
		return;
	WindowInfo info;
	RETURN_VOID_IF_ERROR(SetWindowInfo(info, win));
	RETURN_VOID_IF_ERROR(SendOnWindowUpdated(info));
	// TODO: How to handle errors
}

/* callback */
void
OpScopeWindowManager::ActiveWindowChanged(Window *win)
{
#ifdef SCOPE_WINDOW_MANAGER_DEBUG
	dbg_printf("SCP: Active window changed: ");
	DisplayWindowInfo(win);
#endif
	
	if (!IsAcceptedType(win->GetType()))
		return;

	// Don't allow duplicate activations in the history; remove
	// the old one if present.
	OpStatus::Ignore(window_activations.RemoveByItem(win));

	if (window_activations.GetCount() >= MAX_ACTIVATION_HISTORY)
		window_activations.Remove(0);

	RAISE_IF_MEMORY_ERROR(window_activations.Add(win));

	if (!IsEnabled())
		return;
	WindowID w;
	w.SetWindowID(win->Id());
	RETURN_VOID_IF_ERROR(SendOnWindowActivated(w));
	// TODO: How to handle errors
}

void 
OpScopeWindowManager::ReadyStateChanged(FramesDocument *doc, OpScopeReadyStateListener::ReadyState state)
{
	if (!IsEnabled())
		return;

	if (state == OpScopeReadyStateListener::READY_STATE_AFTER_ONLOAD && doc->IsTopDocument())
	{
		unsigned window_id = doc->GetWindow()->Id();
		WindowID w;
		w.SetWindowID(window_id);
		RETURN_VOID_IF_ERROR(SendOnWindowLoaded(w));
	}
}

BOOL
OpScopeWindowManager::AcceptWindow(unsigned window_id)
{
	return AcceptWindow(g_windowManager->GetWindow(window_id));
}

BOOL
OpScopeWindowManager::IsAcceptedType(Window_Type type)
{
	return !excluded_types.Contains(static_cast<INT32>(type));
}
/* virtual */ BOOL
OpScopeWindowManager::AcceptWindow(Window* window)
{
	unsigned window_id = window ? window->Id() : 0;
	return !(window && !IsAcceptedType(window->GetType())) && (include_all || included_windows.Find(window_id) >= 0 || AcceptOpener(window)) && excluded_windows.Find(window_id) == -1;
}

BOOL
OpScopeWindowManager::AcceptOpener(Window* window)
{
	if (!window || !window->GetOpener() || !window->GetOpener()->GetWindow()) return FALSE;
	return AcceptWindow(window->GetOpener()->GetWindow());
}

Window *
OpScopeWindowManager::FindWindow(unsigned id) const
{
	for (Window* w = g_windowManager->FirstWindow(); w; w = w->Suc())
		if (w->Id() == id)
			return w;
	return NULL;
}

OP_STATUS
OpScopeWindowManager::FindWindow(unsigned id, Window *&window)
{
	window = FindWindow(id);
	return window ? OpStatus::OK : SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Window not found"));
}

OP_STATUS
OpScopeWindowManager::GetWindowType(const uni_char *name, Window_Type &type)
{
	if (uni_str_eq(name, "normal")) type = WIN_TYPE_NORMAL;
	else if (uni_str_eq(name, "download")) type = WIN_TYPE_DOWNLOAD;
	else if (uni_str_eq(name, "cache")) type = WIN_TYPE_CACHE;
	else if (uni_str_eq(name, "plugins")) type = WIN_TYPE_PLUGINS;
	else if (uni_str_eq(name, "history")) type = WIN_TYPE_HISTORY;
	else if (uni_str_eq(name, "help")) type = WIN_TYPE_HELP;
	else if (uni_str_eq(name, "mail_view")) type = WIN_TYPE_MAIL_VIEW;
	else if (uni_str_eq(name, "mail_compose")) type = WIN_TYPE_MAIL_COMPOSE;
	else if (uni_str_eq(name, "newsfeed_view")) type = WIN_TYPE_NEWSFEED_VIEW;
	else if (uni_str_eq(name, "im_view")) type = WIN_TYPE_IM_VIEW;
	else if (uni_str_eq(name, "p2p_view")) type = WIN_TYPE_P2P_VIEW;
	else if (uni_str_eq(name, "brand_view")) type = WIN_TYPE_BRAND_VIEW;
	else if (uni_str_eq(name, "print_selection")) type = WIN_TYPE_PRINT_SELECTION;
	else if (uni_str_eq(name, "js_console")) type = WIN_TYPE_JS_CONSOLE;
	else if (uni_str_eq(name, "gadget")) type = WIN_TYPE_GADGET;
	else if (uni_str_eq(name, "controller")) type = WIN_TYPE_CONTROLLER;
	else if (uni_str_eq(name, "info")) type = WIN_TYPE_INFO;
	else if (uni_str_eq(name, "dialog")) type = WIN_TYPE_DIALOG;
	else if (uni_str_eq(name, "thumbnail")) type = WIN_TYPE_THUMBNAIL;
	else if (uni_str_eq(name, "devtools")) type = WIN_TYPE_DEVTOOLS;
	else if (uni_str_eq(name, "normal_hidden")) type = WIN_TYPE_NORMAL_HIDDEN;
	else
	{
		type = WIN_TYPE_NORMAL;
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Could not find window type"));
	}

	return OpStatus::OK;
}

OP_STATUS
OpScopeWindowManager::SetDefaultTypeFilter()
{
	excluded_types.RemoveAll();
	RETURN_IF_MEMORY_ERROR(excluded_types.Add(static_cast<INT32>(WIN_TYPE_DEVTOOLS)));
	RETURN_IF_MEMORY_ERROR(excluded_types.Add(static_cast<INT32>(WIN_TYPE_NORMAL_HIDDEN)));
	return OpStatus::OK;
}

OP_STATUS
OpScopeWindowManager::DoGetActiveWindow(WindowID &out)
{
	int size = static_cast<int>(window_activations.GetCount());

	// This is a workaround for CORE-27688. 
	// The problem is that the window type for the Dragonfly
	// window is set *after* the activation event.
	// This means that the Dragonfly window becomes the active
	// window whenever it's opened.
	//
	// To fix this, the last 'MAX_ACTIVATION_HISTORY' activated
	// windows are stored. Whenever the active window is requested,
	// we find the most recent activation with an accepted window type.
	for (int i = size - 1; i >= 0; --i)
	{
		Window *win = window_activations.Get(static_cast<unsigned>(i));
		OP_ASSERT(win);

		if (win && IsAcceptedType(win->GetType()))
		{
			out.SetWindowID(win->Id());
			return OpStatus::OK;
		}
	}

	out.SetWindowID(0);
	return OpStatus::OK;
}

OP_STATUS
OpScopeWindowManager::SetWindowInfo(WindowInfo &info, Window *win)
{
	info.SetWindowID(win->Id());
	RETURN_IF_ERROR(info.SetTitle(win->GetWindowTitle() ? win->GetWindowTitle() : UNI_L("")));
	RETURN_IF_ERROR(info.SetWindowType(GetWindowTypeName(win->GetType())));
	info.SetOpenerID(win->GetOpener() ? win->GetOpener()->GetWindow()->Id() : 0);

#ifdef EXTENSION_SUPPORT
	if (win->GetType() == WIN_TYPE_GADGET)
		if (OpGadget *extension = win->GetGadget())
		{
			OpString extension_name;
			if (OpStatus::IsSuccess(extension->GetGadgetName(extension_name)) && extension_name.HasContent())
				RETURN_IF_ERROR(info.SetExtensionName(extension_name.CStr()));
		}
#endif // EXTENSION_SUPPORT

	info.SetIsPrivate(win->GetPrivacyMode());
	return OpStatus::OK;
}

OP_STATUS
OpScopeWindowManager::DoListWindows(WindowList &out)
{
	for (Window* win = g_windowManager->FirstWindow(); win; win = (Window*)win->Suc())
	{
		if (IsAcceptedType(win->GetType()))
		{
			OpAutoPtr<WindowInfo> info(OP_NEW(WindowInfo, ()));
			RETURN_OOM_IF_NULL(info.get());
			RETURN_IF_ERROR(SetWindowInfo(*info.get(), win));
			RETURN_IF_ERROR(out.GetWindowListRef().Add(info.get()));
			info.release();
		}
	}
	return OpStatus::OK;
}

OP_STATUS
OpScopeWindowManager::DoModifyFilter(const WindowFilter &in)
{
	if (in.GetClearFilter())
		ClearWindowIdFilters();

	int i, count;
	const OpValueVector<UINT32> &include_ids = in.GetIncludeIDList();
	count = include_ids.GetCount();
	for (i = 0; i < count; ++i)
		RETURN_IF_ERROR(included_windows.Add(include_ids.Get(i)));

	const OpValueVector<UINT32> &exclude_ids = in.GetExcludeIDList();
	count = exclude_ids.GetCount();
	for (i = 0; i < count; ++i)
		RETURN_IF_ERROR(excluded_windows.Add(exclude_ids.Get(i)));

	const OpVector<OpString> &include_patterns = in.GetIncludePatternList();
	count = include_patterns.GetCount();
	for (i = 0; i < count; ++i)
		if (include_patterns.Get(i)->Compare(UNI_L("*")) == 0)
			include_all = TRUE;

	g_scope_manager->FilterChanged();

	// TODO: What about exclude patterns? Maybe redesign WindowFilter message?

	return OpStatus::OK;
}


OP_STATUS
OpScopeWindowManager::DoCreateWindow(const CreateWindowArg &in, WindowID &out)
{
	Window *w = g_windowManager->SignalNewWindow(NULL);
	RETURN_OOM_IF_NULL(w);

	w->SetOpener(NULL, -1, TRUE, TRUE);

	if (in.HasWindowType())
	{
		Window_Type type;
		RETURN_IF_ERROR(GetWindowType(in.GetWindowType().CStr(), type));
		w->SetType(type);
	}

	if (in.HasIsPrivate())
		w->SetPrivacyMode(in.GetIsPrivate());

	out.SetWindowID(w->Id());
	return OpStatus::OK;
}

OP_STATUS
OpScopeWindowManager::DoCloseWindow(const CloseWindowArg &in)
{
	Window *w;
	RETURN_IF_ERROR(FindWindow(in.GetWindowID(), w));
	g_windowManager->CloseWindow(w);
	return OpStatus::OK;
}

OP_STATUS
OpScopeWindowManager::DoOpenURL(const OpenURLArg &in)
{
	Window *w;
	RETURN_IF_ERROR(FindWindow(in.GetWindowID(), w));
	return w->OpenURL(in.GetUrl().CStr());
}

OP_STATUS
OpScopeWindowManager::DoModifyTypeFilter(const ModifyTypeFilterArg &in)
{
	if (in.GetMode().Compare("replace") == 0)
		excluded_types.RemoveAll();

	if (in.GetMode().Compare("replace") == 0 || in.GetMode().Compare("append") == 0)
	{
		const OpAutoVector<OpString> &typeList = in.GetTypeList();

		for (unsigned i = 0; i < typeList.GetCount(); ++i)
		{
			Window_Type type;
			RETURN_IF_ERROR(GetWindowType(typeList.Get(i)->CStr(), type));
			RETURN_IF_MEMORY_ERROR(excluded_types.Add(static_cast<INT32>(type)));
		}
		return OpStatus::OK;
	}
	else if (in.GetMode().Compare("default") == 0)
		return SetDefaultTypeFilter();

	return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Unknown mode"));
}

#ifdef SCOPE_WINDOW_MANAGER_DEBUG
#undef SCOPE_WINDOW_MANAGER_DEBUG
#endif

#endif // SCOPE_WINDOW_MANAGER_SUPPORT
