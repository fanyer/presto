/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

// FIXME: clean up these includes!

#include "modules/dochand/winman.h"

#include "modules/display/cursor.h"
#include "modules/display/prn_dev.h"
#include "modules/display/vis_dev.h"
#include "modules/doc/frm_doc.h"
#include "modules/dom/domutils.h"
#include "modules/dochand/fdelm.h"
#include "modules/dochand/global_history.h"
#include "modules/dochand/win.h"
#include "modules/dochand/windowlistener.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/encodings/detector/charsetdetector.h"
#ifdef GADGET_SUPPORT
#include "modules/gadgets/OpGadget.h"
#endif // GADGET_SUPPORT
#include "modules/hardcore/mem/mem_man.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/unicode/unicode.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/olddebug/timing.h"
#include "modules/pi/OpWindow.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/inputmanager/inputaction.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/display/prn_info.h"
#include "modules/url/url_man.h"
#include "modules/url/protocols/comm.h"
#include "modules/util/gen_str.h"
#include "modules/util/filefun.h"

#ifdef USE_OP_THREAD_TOOLS
# include "modules/pi/OpThreadTools.h"
#endif

#include "modules/util/timecache.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/dom/domenvironment.h"

#ifdef QUICK
# include "adjunct/quick/Application.h"
# include "adjunct/quick/managers/KioskManager.h"
# include "adjunct/quick/dialogs/DownloadDialog.h"
# include "adjunct/quick/hotlist/HotlistManager.h"
#endif // QUICK

#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/windowcommander/src/WindowCommanderManager.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"

#ifdef _PLUGIN_SUPPORT_
# include "modules/ns4plugins/opns4pluginhandler.h"
#endif // _PLUGIN_SUPPORT_

#ifdef LINK_SUPPORT
#include "modules/logdoc/link.h"
#endif // LINK_SUPPORT

#ifdef WEBSERVER_SUPPORT
#include "modules/webserver/webserver-api.h"
#include "modules/webserver/webserver_resources.h"
#endif //WEBSERVER_SUPPORT

#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
# include "modules/scope/scope_window_listener.h"
#endif // SCOPE_WINDOW_MANAGER_SUPPORT

#ifdef ABOUT_HTML_DIALOGS
# include "modules/about/ophtmldialogs.h"
#endif // ABOUT_HTML_DIALOGS

#ifdef USE_OP_CLIPBOARD
# include "modules/dragdrop/clipboard_manager.h"
#endif // USE_OP_CLIPBOARD

void WindowManager::ConstructL()
{
	g_pcdoc->RegisterListenerL(this);
	g_pcdisplay->RegisterListenerL(this);

	LEAVE_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_OPEN_URL_IN_NEW_WINDOW, 0));

	LEAVE_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_ES_CLOSE_WINDOW, 0));
}

WindowManager::WindowManager()
	: idcounter(0),
	  m_documents_having_blink(0),
	  m_privacy_mode_context_id(0),
	  m_number_of_windows_in_privacy_mode(0)
#ifdef WEB_TURBO_MODE
	, m_turbo_mode_context_id(0)
#endif // WEB_TURBO_MODE
#ifdef AB_ERROR_SEARCH_FORM
	, m_last_typed_user_text(NULL)
#endif //AB_ERROR_SEARCH_FORM
{
	curr_clicked_window = NULL;
	current_popup_message[0] = 0;
	current_popup_message_statusline_only = FALSE;
}

WindowManager::~WindowManager()
{
	g_pcdoc->UnregisterListener(this);
	g_pcdisplay->UnregisterListener(this);

	OP_ASSERT(m_number_of_windows_in_privacy_mode == 0 && m_privacy_mode_context_id == 0);

	Clear();

	g_main_message_handler->UnsetCallBacks(this);
}

void WindowManager::Clear()
{
	Window* w = FirstWindow();

	while (w)
	{
		Window* ws = w->Suc();
#ifdef QUICK
		if(g_application)	//g_application is the only uiwindowlistener
#endif // QUICK
		{
			g_windowCommanderManager->GetUiWindowListener()->CloseUiWindow(w->GetWindowCommander());
		}

		w = ws;
	}

#ifdef WEB_TURBO_MODE
	if( m_turbo_mode_context_id )
	{
		urlManager->RemoveContext(m_turbo_mode_context_id, FALSE);
		m_turbo_mode_context_id = 0;
	}
#endif // WEB_TURBO_MODE
}

void WindowManager::SetMaxWindowHistory(int len)
{
	for (Window* w = FirstWindow(); w; w = w->Suc())
		w->SetMaxHistory(len);
}

Window* WindowManager::GetNamedWindow(Window* from_window, const uni_char* window_name, int &sub_win_id, BOOL open_new_window)
{
	if (!window_name)
	{
		sub_win_id = -1;

		if (from_window)
			return from_window;
		else if (open_new_window)
        {
            BOOL3 open_in_new_window = YES;
            BOOL3 open_in_background = MAYBE;
			return GetAWindow(TRUE, open_in_new_window, open_in_background);
        }
		return NULL;
	}

	for (Window* w = FirstWindow(); w; w = w->Suc())
	{
		if (w->Name() && !uni_strcmp(w->Name(), window_name))
			return w;

		FramesDocument* doc = w->GetCurrentDoc();

		if (doc)
		{
			const uni_char* name = window_name;

			doc->FindTarget(name, sub_win_id);

			if (!name)
				return w;
		}
	}

	if (window_name)
	{
		const uni_char* host = from_window->GetCurrentURL().GetAttribute(URL::KUniHostName, TRUE).CStr();

		if (!g_pcdoc->GetIntegerPref(PrefsCollectionDoc::IgnoreTarget, host) &&
		    !g_pcdoc->GetIntegerPref(PrefsCollectionDoc::SingleWindowBrowsing, host) ||
		    from_window->GetType() == WIN_TYPE_BRAND_VIEW)
		{
			if (open_new_window)
			{
				sub_win_id = -1;
				BOOL3 open_in_new_window = YES;
				BOOL3 open_in_background = MAYBE;
				Window* window = GetAWindow(TRUE, open_in_new_window, open_in_background);

				if (window)
				{
					window->SetName(window_name);
					window->SetOpener(from_window, -1, FALSE);
				}

				return window;
			}
			else
				return NULL;
		}
	}

	return from_window;
}

void WindowManager::UpdateVisitedLinks(const URL& url, Window* exclude_window)
{
#ifndef DONT_UPDATE_VISITED_LINKS
	for (Window* w = FirstWindow(); w; w = w->Suc())
		if (w != exclude_window)
			w->UpdateVisitedLinks(url);
#endif // DONT_UPDATE_VISITED_LINKS
}

Window* WindowManager::OpenURLNewWindow(const char* url_name, BOOL check_if_expired, BOOL3 open_in_new_window, EnteredByUser entered_by_user)
{
    BOOL3 open_in_background = MAYBE;
	Window* window;

    window = GetAWindow(TRUE, open_in_new_window, open_in_background);

	if (window)
		if (window->OpenURL(url_name, check_if_expired, TRUE, FALSE, FALSE, entered_by_user == WasEnteredByUser) == OpStatus::ERR_NO_MEMORY)
			return NULL;

	return window;
}

Window* WindowManager::OpenURLNewWindow(const uni_char* url_name, BOOL check_if_expired, BOOL3 open_in_new_window, EnteredByUser entered_by_user)
{
    BOOL3 open_in_background = MAYBE;
	Window* window;

    window = GetAWindow(TRUE, open_in_new_window, open_in_background);

	if (window)
    {
		if (window->OpenURL(url_name, check_if_expired, TRUE, FALSE, FALSE, (open_in_background==YES),entered_by_user) == OpStatus::ERR_NO_MEMORY)
			return NULL;
    }

	return window;
}

BOOL WindowManager::CheckTargetSecurity(FramesDocument *source_doc, FramesDocument *target_doc)
{
	BOOL permitted;

	DOM_Runtime *source_runtime = DOM_Utils::GetDOM_Runtime(source_doc->GetESRuntime());
	DOM_Runtime *target_runtime = DOM_Utils::GetDOM_Runtime(target_doc->GetESRuntime());

	if (source_runtime)
	{
		if (target_runtime)
			OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_STANDARD, source_runtime, target_runtime, permitted);
		else
			OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_STANDARD, OpSecurityContext(source_runtime), OpSecurityContext(target_doc->GetSecurityContext()), permitted);
	}
	else
	{
		OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_STANDARD, source_doc->GetSecurityContext(), target_doc->GetSecurityContext(), permitted);
	}

	return permitted;
}

OP_STATUS WindowManager::OpenURLNamedWindow(URL url, Window* load_window, FramesDocument* doc, int sub_win_id, const uni_char* window_name, BOOL user_initiated, BOOL open_in_other_window, BOOL open_in_background, BOOL delay_open, BOOL open_in_page, ES_Thread *thread, BOOL plugin_unrequested_popup)
{
	OP_ASSERT(doc);
	Window* window = load_window;

    FramesDocument* top_doc = doc ? doc->GetTopDocument() : NULL;
    FramesDocument* frame_doc = (sub_win_id == -1) ? doc : top_doc->GetSubDocWithId(sub_win_id);

	DocumentReferrer origin_url;
	if (frame_doc)
		origin_url = DocumentReferrer(frame_doc);

	BOOL check_origin = FALSE;

	if (thread)
	{
		ES_ThreadInfo info = thread->GetOriginInfo();

		if (info.is_user_requested)
		{
			open_in_other_window = open_in_other_window || info.open_in_new_window;
			open_in_background = open_in_background || info.open_in_background;
			if (open_in_other_window)
				thread->SetOpenInNewWindow();
			if (open_in_background)
				thread->SetOpenInBackground();
		}
		else
			open_in_background = FALSE;
	}

	if (url.Type() == URL_JAVASCRIPT && (window && window->IsMailOrNewsfeedWindow() || origin_url.url.GetAttribute(URL::KSuppressScriptAndEmbeds, TRUE) == MIME_EMail_ScriptEmbed_Restrictions))
		/* Don't load javascript URLs clicked in mail or news feed views. */
		return OpStatus::OK;

#ifdef GADGET_SUPPORT
	if ((sub_win_id == -1 || window_name) && window && window->GetGadget() && // For gadgets ...
	    url.Type() != URL_JAVASCRIPT && // ... handle javascript normally ...
	    url.Type() != URL_MAILTO && url.Type() != URL_TEL) // ... as well as mailto/tel.
	{   // Everything else is handled specially.
		// Store these in so that we can reset them if not handled here
		int old_sub_win_id = sub_win_id;
		const uni_char* old_window_name = window_name;

		if (frame_doc)
			frame_doc->FindTarget(window_name, sub_win_id);
		BOOL is_internal_link = origin_url.url.SameServer(url);
		// this combination is true only if target is top, or target is a parent which is top frame:
		BOOL target_is_top = (!window_name && sub_win_id == -1);
		BOOL target_is_sub_window = sub_win_id >= 0;

		if ((is_internal_link && !window->GetGadget()->IsExtension()) && ((!target_is_top && !target_is_sub_window) || open_in_other_window || open_in_background))
			return OpStatus::OK; // Ignore internal links which try to open new window (but allow it in extensions).

		if ((!is_internal_link && target_is_top) || sub_win_id == -2 || (target_is_top && url.Type() == URL_DATA))
		{
			load_window->GetGadget()->OpenURLInNewWindow(url);
			return OpStatus::OK;
		}

		sub_win_id = old_sub_win_id;
		window_name = old_window_name;
	}
#endif // GADGET_SUPPORT

	if (load_window->GetOutputAssociatedWindow())
	{
		window = load_window->GetOutputAssociatedWindow();
		sub_win_id = -1;
	}
	else
	{
		if (open_in_other_window && !DocumentManager::IsSpecialURL(url)) // types that will open a window anyway and will not use a window opened here for anything
		{
			if (thread || plugin_unrequested_popup)
			{
				OP_STATUS status = DOM_Environment::OpenURLWithTarget(url, origin_url, NULL, doc, thread, user_initiated, plugin_unrequested_popup);
				if (status == OpStatus::ERR_NO_MEMORY || status == OpStatus::OK)
					return status;
			}

			if (delay_open)
				window = NULL;
			else
			{
				BOOL3 open_in_new_window = YES;
				BOOL3 open_in_background_window = open_in_background ? YES : MAYBE;
				Window* new_window = g_windowManager->GetAWindow(TRUE, open_in_new_window, open_in_background_window, 0, 0, FALSE, MAYBE, MAYBE, NULL, NULL, open_in_page, load_window);

				if (new_window)
				{
					window = new_window;
					sub_win_id = -1;
				}
			}
		}
		else if (origin_url.url.GetAttribute(URL::KType) == URL_ATTACHMENT && origin_url.url.GetAttribute(URL::KSuppressScriptAndEmbeds, TRUE) == MIME_EMail_ScriptEmbed_Restrictions && url.GetAttribute(URL::KType) != URL_ATTACHMENT)
		// Links opened from attachment URLs (files which have been MIME decoded
		// and hence has a URL on the form attachment:<something>) should
		// always open in top frame, unless it's another attachment you try opening.
		// This only affect mail/newsfeeds and similar, not MHTML that hasn't got the
		// suppress flag set.
		{
			sub_win_id = -1;
		}
		else if (window_name && !DocumentManager::IsSpecialURL(url)) // some types are not meaningful to open in named windows and will only leave us with an empty window
		{
			if (frame_doc)
				frame_doc->FindTarget(window_name, sub_win_id);

			// this combination is true only if target is top, or target is a parent which is top frame:
			BOOL target_is_top = (!window_name && sub_win_id == -1);

			if (window_name)
				window = GetNamedWindow(load_window, window_name, sub_win_id, !delay_open);
			else if (sub_win_id == -2 && delay_open)
			{
				if (!g_pcdoc->GetIntegerPref(PrefsCollectionDoc::IgnoreTarget)
					&& !g_pcdoc->GetIntegerPref(PrefsCollectionDoc::SingleWindowBrowsing))
					window = NULL;
				else
					sub_win_id = -1;
			}

			if (window)
				check_origin = !target_is_top;  // if target is _top we don't need to check origin
			else if (load_window->GetType() == WIN_TYPE_NORMAL)
			{
				OP_STATUS status = DOM_Environment::OpenURLWithTarget(url, origin_url, window_name, doc, thread, user_initiated, plugin_unrequested_popup);
				if (status == OpStatus::ERR_NO_MEMORY || status == OpStatus::OK)
					return status;
			}
		}
	}

	// If we're loading inside MHTML and target the dummy <object>, retarget it to the real document.
	if (sub_win_id != -1)
	{
		if (FramesDocument* target_doc = top_doc->GetSubDocWithId(sub_win_id))
		{
			if (target_doc->GetURL().Type() == URL_ATTACHMENT)
			{
				if (FramesDocument* target_parent = target_doc->GetParentDoc())
				{
					URL target_parent_url = target_parent->GetURL();
					if (target_parent_url.Type() != URL_ATTACHMENT &&
						target_parent->IsGeneratedByOpera())
					{
						sub_win_id = target_parent->GetSubWinId();
					}
				}
			}
		}
	}


	// Check to see if the target is legal, we don't want to allow just any page
	// to alter just any other page.
	if (check_origin)
	{
		FramesDocument *current_doc = window->GetCurrentDoc();
		FramesDocElm *fdelm = current_doc ? current_doc->GetFrmDocElm(sub_win_id) : NULL;
		if (fdelm)
			current_doc = fdelm->GetCurrentDoc();

		/* Implementation of "allowed to navigate" check as described in HTML5:
		     http://dev.w3.org/html5/spec/Overview.html#allowed-to-navigate */

		BOOL allow = !current_doc || WindowManager::CheckTargetSecurity(doc, current_doc);
		if (!allow)
			if (FramesDocument *parent_doc = current_doc->GetParentDoc())
				do
					if (WindowManager::CheckTargetSecurity(doc, parent_doc))
					{
						allow = TRUE;
						break;
					}
				while ((parent_doc = parent_doc->GetParentDoc()) != NULL);
			else if (FramesDocument *opener_doc = window->GetOpener(FALSE))
				if (WindowManager::CheckTargetSecurity(doc, opener_doc))
					allow = TRUE;

		if (!allow)
		{
			// the origin checks failed so we don't allow opening in the named window

			if (url.Type() == URL_JAVASCRIPT)
			{
				window = load_window;
				sub_win_id = frame_doc ? frame_doc->GetSubWinId() : -1;
				open_in_other_window = TRUE;
			}
			else
			{
				OP_STATUS status = DOM_Environment::OpenURLWithTarget(url, origin_url, window_name, doc, thread);
				if (status == OpStatus::ERR_NO_MEMORY || status == OpStatus::OK)
					return status;

				BOOL3 open_in_new_window = YES;
				BOOL3 open_in_background_window = open_in_background ? YES : MAYBE;

# if defined MSWIN
				PopupDestination popup_destination = (enum PopupDestination) g_pcjs->GetIntegerPref(PrefsCollectionJS::TargetDestination, current_doc ? current_doc->GetURL().GetServerName() : NULL);

				/* Qt will activate a new popup window at once so unfortunately we
				   can not let a background image open its popup windows in the
				   background as well because some pages will open more than one
				   popup window. (espen 2001-10-03) */
				switch (popup_destination)
				{
				case POPUP_WIN_IGNORE:
				case POPUP_WIN_BACKGROUND:
					open_in_background_window = YES;
					break;
				}
# endif // MSWIN

				Window* new_window = g_windowManager->GetAWindow(TRUE, open_in_new_window, open_in_background_window, 0, 0, FALSE, MAYBE, MAYBE, NULL, NULL, FALSE, load_window);

				if (new_window)
				{
					window = new_window;
					sub_win_id = -1;
				}
			}
		}
	}

	// clicking on links in mail window should always open new window
	if (window && !window->IsMailOrNewsfeedWindow())
	{
		DocumentManager* doc_man = window->GetDocManagerById(sub_win_id);
		FramesDocument* frm_doc = doc_man ? doc_man->GetCurrentDoc() : NULL;

		if (doc_man)
			doc_man->StoreRequestThreadInfo(thread);

		if (url.Type() == URL_JAVASCRIPT && frm_doc)
		{
			DocumentManager::OpenURLOptions options;
			options.user_initiated = FALSE;
			options.entered_by_user = NotEnteredByUser;
			options.is_walking_in_history = FALSE;
			options.origin_thread = thread;
			RETURN_IF_MEMORY_ERROR(frm_doc->ESOpenURL(url, origin_url, FALSE, FALSE, FALSE, options, !thread || thread->IsUserRequested(), open_in_other_window));
			return OpStatus::OK;
		}
		else if (delay_open)
		{
			if (doc_man)
			{
				doc_man->SetUrlLoadOnCommand(url, origin_url, FALSE, user_initiated, thread);
				doc_man->GetMessageHandler()->PostMessage(MSG_URL_LOAD_NOW, url.Id(TRUE), user_initiated);
            }
		}
		else
			window->GetClickedURL(url, origin_url, sub_win_id, user_initiated, open_in_background);
	}
	else
		if (delay_open)
		{
			OpenUrlInNewWindowInfo* info;

			// We should not use the same context ids as mail context ids when opening links from a mail
			// it can cause all sorts of weird behaviour, e.g. DSK-248618.
			// When shift-clicking, window == NULL, so we have to check the load_window instead
			open_in_background = open_in_other_window && open_in_background;
			if (((window && window->IsMailOrNewsfeedWindow()) || (load_window && load_window->IsMailOrNewsfeedWindow()))
				&& url.Type() != URL_ATTACHMENT)
			{
				URL context_id_free_url = g_url_api->GetURL(url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_Escaped_NOT_FOR_UI));
				info = OP_NEW(OpenUrlInNewWindowInfo, (context_id_free_url, origin_url, window_name, open_in_background, user_initiated, load_window ? load_window->Id() : 0, frame_doc ? frame_doc->GetSubWinId() : 0, open_in_page));
			}
			else
				info = OP_NEW(OpenUrlInNewWindowInfo, (url, origin_url, window_name, open_in_background, user_initiated, load_window ? load_window->Id() : 0, frame_doc ? frame_doc->GetSubWinId() : 0, open_in_page));

			if (info == NULL)
				return OpStatus::ERR_NO_MEMORY;

			g_main_message_handler->PostMessage(MSG_OPEN_URL_IN_NEW_WINDOW, 0, reinterpret_cast<MH_PARAM_2>(info));
		}

	return OpStatus::OK;
}


// Returns NULL on OOM
Window* WindowManager::AddWindow(OpWindow* opWindow, OpWindowCommander* opWindowCommander)
{
	Window *newWin = OP_NEW(Window, (++idcounter, opWindow, opWindowCommander));

	if (newWin == NULL || OpStatus::IsError(newWin->Construct()))
	{
		OP_DELETE(newWin);
		return NULL;
	}

    newWin->Into(&win_list);

#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	OpScopeWindowListener::OnNewWindow(newWin);
#endif // SCOPE_WINDOW_MANAGER_SUPPORT

    return newWin;
}

void WindowManager::DeleteWindow(Window* window)
{
    if (window)
	{
		if (window == curr_clicked_window)
			curr_clicked_window = NULL;

		for (Window* ww = FirstWindow(); ww; ww = ww->Suc())
		{
            if (ww->GetOutputAssociatedWindow() == window)
                ww->SetOutputAssociatedWindow(NULL);

			if (ww->GetOpenerWindow() == window)
				ww->SetOpener(NULL, -1, TRUE);
		}

#ifdef _PRINT_SUPPORT_
		if (window->GetPreviewMode())
		{
			// Leave print preview mode, to avoid memory leak.
			window->TogglePrintMode(TRUE);
		}
#endif // _PRINT_SUPPORT_

#ifdef WEBSERVER_SUPPORT
		if (g_webserver)
			g_webserver->windowClosed(window->Id());
#endif // WEBSERVER_SUPPORT

#ifdef ABOUT_HTML_DIALOGS
		HTML_Dialog *dlg = g_html_dialog_manager->FindDialog(window);
		if (dlg)
			dlg->CloseDialog();
#endif // ABOUT_HTML_DIALOGS

        window->Out();

#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	OpScopeWindowListener::OnWindowRemoved(window);
#endif // SCOPE_WINDOW_MANAGER_SUPPORT

        OP_DELETE(window);
    }

	g_memory_manager->CheckDocMemorySize();
}

Window* WindowManager::GetWindow(unsigned long nid)
{
	for (Window* w = FirstWindow(); w; w = w->Suc())
		if (w->Id() == nid)
			return w;

    return NULL;
}

void WindowManager::Insert(Window* window, Window* precede)
{
	window->Out();

	if (precede)
		window->Precede(precede);
	else
		window->Into(&win_list);
}

const uni_char* WindowManager::GetPopupMessage(ST_MESSAGETYPE *strType)
{
	if(strType)
		*strType = messageStringType;
	const uni_char *szResult = (current_popup_message[0] ? current_popup_message : NULL);

	return szResult;
}

void WindowManager::SetPopupMessage(const uni_char *msg, BOOL fStatusLineOnly, ST_MESSAGETYPE strType)
{
    if (msg && !uni_strcmp(msg, current_popup_message))
        return;

	current_popup_message_statusline_only = fStatusLineOnly;
	messageStringType = strType;

    if (msg)
        uni_strncpy(current_popup_message, msg, 4096);
	else
        current_popup_message[0] = 0;

	current_popup_message[4095] = 0; // Ensures that the string is always null terminated.
}

OP_STATUS WindowManager::UpdateWindows(BOOL /* unused = FALSE */)
{
	for (Window* w = FirstWindow(); w; w = w->Suc())
		w->UpdateWindow();

	return OpStatus::OK;
}

OP_STATUS WindowManager::UpdateWindowsAllDocuments(BOOL /* unused = FALSE */)
{
	for (Window* w = FirstWindow(); w; w = w->Suc())
		w->UpdateWindowAllDocuments();
	
	return OpStatus::OK;
}

OP_STATUS WindowManager::UpdateAllWindowDefaults(BOOL scroll, BOOL progress, BOOL news, WORD scale, BOOL size)
{
	BOOL failed = FALSE;

    for (Window *w = FirstWindow(); w; w = w->Suc())
	{
		if (w->HasFeature(WIN_FEATURE_NAVIGABLE)) // document windows
			if (OpStatus::IsError(w->UpdateWindowDefaults(scroll, progress, news, scale, size)))
				failed = TRUE;
	}
	return failed ? OpStatus::ERR_NO_MEMORY : OpStatus::OK;
}

Window* WindowManager::SignalNewWindow(Window* opener, int width, int height, BOOL3 scrollbars, BOOL3 toolbars, BOOL3 focus_document, BOOL3 open_background, BOOL opened_by_script)
{
	OP_STATUS status = OpStatus::OK;

	BOOL wic_focusdoc = focus_document == NO ? FALSE : TRUE;
	BOOL wic_openback = (open_background == NO || open_background == MAYBE) ? FALSE : TRUE;

	WindowCommander* opener_wic = opener ? opener->GetWindowCommander() : NULL;

	WindowCommander* windowCommander;
	windowCommander = OP_NEW(WindowCommander, ());
	if (windowCommander == NULL)
	{
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return NULL;
	}

	status = windowCommander->Init();

	if (OpStatus::IsError(status))
	{
		if (OpStatus::IsMemoryError(status))
		{
			g_memory_manager->RaiseCondition(status);
		}
		OP_DELETE(windowCommander);
		return NULL;
	}

	BOOL show_scrollbars = scrollbars != NO;
	if (scrollbars == MAYBE)
		show_scrollbars = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowScrollbars);

	status = g_windowCommanderManager->GetUiWindowListener()->CreateUiWindow(
		windowCommander, opener_wic,
		width, height,
		( show_scrollbars	? OpUiWindowListener::CREATEFLAG_SCROLLBARS : OpUiWindowListener::CREATEFLAG_NO_FLAGS ) |
		( toolbars			? OpUiWindowListener::CREATEFLAG_TOOLBARS : OpUiWindowListener::CREATEFLAG_NO_FLAGS ) |
		( wic_focusdoc		? OpUiWindowListener::CREATEFLAG_FOCUS_DOCUMENT : OpUiWindowListener::CREATEFLAG_NO_FLAGS ) |
		( wic_openback		? OpUiWindowListener::CREATEFLAG_OPEN_BACKGROUND : OpUiWindowListener::CREATEFLAG_NO_FLAGS ) |
		( opened_by_script	? OpUiWindowListener::CREATEFLAG_OPENED_BY_SCRIPT : OpUiWindowListener::CREATEFLAG_NO_FLAGS ) |
		OpUiWindowListener::CREATEFLAG_USER_INITIATED
	);

	// get the Window* that was created with help from the windowCommander

	if (OpStatus::IsSuccess(status))
	{
		for (Window* w = FirstWindow(); w; w = w->Suc())
		{
			if (w->GetWindowCommander() == windowCommander)
			{
				if (opener)
				{
					// If the opener window is private, set the new as private too.
					if (opener->GetPrivacyMode())
						w->SetPrivacyMode(TRUE);
#ifdef WEB_TURBO_MODE
					// Inherit turbo mode from opener window alt. global setting
					BOOL use_turbo;
					if (opener->GetType() == WIN_TYPE_NORMAL)
						use_turbo = opener->GetTurboMode();
					else
						use_turbo = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseWebTurbo);

					w->SetTurboMode(use_turbo);
#endif // WEB_TURBO_MODE
				}

				return w;
			}
		}
	}

	if (OpStatus::IsMemoryError(status))
	{
		g_memory_manager->RaiseCondition(status);
	}
	// failed, so clean up.
	OP_DELETE(windowCommander);

	return NULL;
}

void WindowManager::CloseWindow(Window *window)
{
	g_windowCommanderManager->GetUiWindowListener()->CloseUiWindow(window->GetWindowCommander());
}

Window* WindowManager::GetAWindow(BOOL unused1, BOOL3 new_window, BOOL3 background,
                                  int width /*= 0 */, int height /*= 0*/,
                                  BOOL unused2,	/*=FALSE*/
                                  BOOL3 scrollbars,		/*=MAYBE*/
                                  BOOL3 toolbars,		/*=MAYBE*/
                                  const uni_char* unused3 /*= NULL*/,
                                  const uni_char* unused4 /* = NULL */,
                                  BOOL unused5,	/*=FALSE*/
                                  Window* opener /* = NULL */
                                 )
{
	OP_ASSERT(new_window == YES || new_window == MAYBE);
#ifdef QUICK
	Window* active_window = opener ? opener : g_application->GetActiveWindow();

#ifdef GADGET_SUPPORT
	if (opener && opener->GetOpWindow() && ((OpWindow *)opener->GetOpWindow())->GetStyle() == OpWindow::STYLE_GADGET)
	{
		// Anything opened from a gadget should use a new window if possible
		new_window = YES;
		if (!g_pcdoc->GetIntegerPref(PrefsCollectionDoc::NewWindow))
		{
			// Can we reuse an existing window?
			DesktopWindow* desktop_window = (DesktopWindow *)g_application->GetActiveBrowserDesktopWindow();
			if (desktop_window && desktop_window->GetWindowCommander())
			{
				active_window = desktop_window->GetWindowCommander()->GetWindow();
				new_window = MAYBE;
			}
		}
	}
#endif // GADGET_SUPPORT
	if (new_window == MAYBE && !g_pcdoc->GetIntegerPref(PrefsCollectionDoc::NewWindow))
		if (active_window && !active_window->IsCustomWindow())
			return active_window;
	return SignalNewWindow(active_window, width, height, scrollbars, toolbars, YES, background);
#else
	return SignalNewWindow(opener, width, height, scrollbars, toolbars, YES, background);
#endif
}

#ifdef MSWIN

Window* WindowManager::GetADDEIdWindow(DWORD id, BOOL &tile_now)   // tile_now (in or out) : autotile
{
    Window* dde_window = NULL;

	for (Window* window = FirstWindow(); window; window = window->Suc())
        if (id == window->Id() && !window->IsCustomWindow())
        {
            dde_window = window;

            tile_now = TRUE;
        }

    return dde_window;
}

#endif

void WindowManager::SetGlobalImagesSetting(SHOWIMAGESTATE set)
{
	for (Window *welm = FirstWindow(); welm; welm = welm->Suc())
		welm->SetImagesSetting(set);
}

void WindowManager::ReloadAll()
{
	for (Window *w = FirstWindow(); w; w = w->Suc())
        w->Reload(WasEnteredByUser);
}

void WindowManager::RefreshAll()
{
	for (Window *w = FirstWindow(); w; w = w->Suc())
	{
		OpStatus::Ignore(w->DocManager()->UpdateCurrentDoc(FALSE, FALSE, TRUE));//FIXME:OOM
	}
}

void WindowManager::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
	switch (id)
	{
	case OpPrefsCollection::Doc:
		if (pref == PrefsCollectionDoc::ShowImageState)
			SetGlobalImagesSetting((SHOWIMAGESTATE) newvalue);
		break;

	case OpPrefsCollection::Display:
		switch (pref)
		{
#ifdef SUPPORT_TEXT_DIRECTION
		case PrefsCollectionDisplay::RTLFlipsUI:
#endif // SUPPORT_TEXT_DIRECTION
		case PrefsCollectionDisplay::LeftHandedUI:
			UpdateWindows();
		}
		break;
	}
}

void WindowManager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_OPEN_URL_IN_NEW_WINDOW)
	{
		// info is never null here, we make sure of that when we send
		// this message to ourself.
		OpenUrlInNewWindowInfo* info = reinterpret_cast<OpenUrlInNewWindowInfo*>(par2);
		BOOL3 open_in_new_window = YES;
		BOOL3 open_in_background = info->open_in_background_window?YES:MAYBE;
		Window* new_window = GetAWindow(TRUE, open_in_new_window, open_in_background, 0, 0, FALSE, MAYBE, MAYBE, NULL, NULL, info->open_in_page, GetWindow(info->opener_id));

		if (new_window)
		{
			Window* opener = GetWindow(info->opener_id);

			if (opener)
			{
				// Copy settings from opening window

				new_window->SetCSSMode(opener->GetCSSMode());
				new_window->SetScale(opener->GetScale());

				new_window->SetTextScale(opener->GetTextScale());
				// Bug#128887: Use the force encoding for the target URL.
#ifdef PREFS_HOSTOVERRIDE
				const OpStringC force_encoding(g_pcdisplay->GetStringPref(PrefsCollectionDisplay::ForceEncoding, info->new_url));
				if (!force_encoding.IsEmpty())
				{
					char sbencoding[128]; /* ARRAY OK 2008-02-28 peter */
					uni_cstrlcpy(sbencoding, force_encoding.CStr(), ARRAY_SIZE(sbencoding));
					new_window->SetForceEncoding(sbencoding);
				}
				else
#endif
				{
					new_window->SetForceEncoding(g_pcdisplay->GetForceEncoding());
				}
				new_window->SetERA_Mode(opener->GetERA_Mode());

				BOOL show_img = opener->ShowImages();
				BOOL load_img = opener->LoadImages();

				if (load_img && show_img)
					new_window->SetImagesSetting(FIGS_ON);
				else
					if (show_img)
						new_window->SetImagesSetting(FIGS_SHOW);
					else
						new_window->SetImagesSetting(FIGS_OFF);

				if (!new_window->GetOpener())
				{
					BOOL allow_js_access_to_opener = TRUE;
#ifdef WEBSERVER_SUPPORT
					allow_js_access_to_opener = !info->ref_url.url.GetAttribute(URL::KIsUniteServiceAdminURL);
#endif // WEBSERVER_SUPPORT
					new_window->SetOpener(opener, info->opener_sub_win_id, allow_js_access_to_opener);
				}
			}

			if (info->window_name)
				new_window->SetName(info->window_name);

			OP_STATUS ret = new_window->OpenURL(info->new_url, info->ref_url, TRUE, FALSE, -1, info->user_initiated, info->open_in_background_window);

			if (OpStatus::IsError(ret))
				g_memory_manager->RaiseCondition(ret);
		}

		OP_DELETE(info);
	}
	else if (msg == MSG_ES_CLOSE_WINDOW)
	{
		Window *window = GetWindow(par2);

		if (window)
			window->Close();
	}
}

void WindowManager::AddDocumentWithBlink()
{
	OP_ASSERT(m_documents_having_blink >= 0);
	if (m_documents_having_blink == 0)
		m_blink_timer_listener.SetEnabled(TRUE);

	m_documents_having_blink++;
}

void WindowManager::RemoveDocumentWithBlink()
{
	OP_ASSERT(m_documents_having_blink > 0);
	m_documents_having_blink--;
	if (m_documents_having_blink == 0)
		m_blink_timer_listener.SetEnabled(FALSE);
}


#ifdef _OPERA_DEBUG_DOC_
unsigned WindowManager::GetActiveDocumentCount()
{
	unsigned count = 0;
	for (Window *w = FirstWindow(); w; w = w->Suc())
	{
		DocumentTreeIterator it(w);
		it.SetIncludeThis();
		while (it.Next())
			count++;
	}
	return count;
}
#endif // _OPERA_DEBUG_DOC_

OP_STATUS WindowManager::OnlineModeChanged()
{
	OP_STATUS status = OpStatus::OK;
	for (Window *w = FirstWindow(); w; w = w->Suc())
        if (OpStatus::IsMemoryError(w->DocManager()->OnlineModeChanged()))
		{
			OpStatus::Ignore(status);
			status = OpStatus::ERR_NO_MEMORY;
		}
	return status;
}

void WindowManager::BlinkTimerListener::PostNewTimerMessage()
{
	OP_ASSERT(!m_message_in_queue);
	OP_ASSERT(m_is_active);

	int milliseconds_between_blinks = 1000;
	if (m_timer) // Might be NULL if OOM
		m_timer->Start(milliseconds_between_blinks);

	m_message_in_queue = TRUE;
}

WindowManager::BlinkTimerListener::BlinkTimerListener() :
	m_is_active(FALSE),
	m_message_in_queue(FALSE)
{
	OP_STATUS status = OpStatus::OK;
	m_timer = OP_NEW(OpTimer, ());
	if (!m_timer)
		status = OpStatus::ERR_NO_MEMORY;
	if (OpStatus::IsError(status))
	{
		OP_ASSERT(FALSE);
		// Have no idea how to handle this.  If posting the
		// message fails, we will not get called again, and opera won't
		// have blinking elements. Tragedy!
		g_memory_manager->RaiseCondition(status);//FIXME:OOM At least we can
		//tell the memory manager
		m_timer = NULL;
		return;
	}

	m_timer->SetTimerListener(this);
	OP_ASSERT(m_timer);
}

WindowManager::BlinkTimerListener::~BlinkTimerListener()
{
	OP_DELETE(m_timer);
}

/* virtual */
void WindowManager::BlinkTimerListener::OnTimeOut(OpTimer* timer)
{
	OP_ASSERT(m_message_in_queue);
	OP_ASSERT(timer);
	OP_ASSERT(timer == m_timer);

	m_message_in_queue = FALSE;

	if (m_is_active)
		PostNewTimerMessage();

	// Turn the text on off on off on off on off
	g_opera->layout_module.m_blink_on = !g_opera->layout_module.m_blink_on;

	for (Window* win = g_windowManager->FirstWindow(); win; win = win->Suc())
	{
		FramesDocument *doc = win->DocManager()->GetCurrentVisibleDoc();
		if (doc && doc->HasBlink())
			doc->Blink();
	}
}

void WindowManager::BlinkTimerListener::SetEnabled(BOOL enable)
{
	if (enable)
	{
		m_is_active = TRUE;
		if (!m_message_in_queue)
			PostNewTimerMessage();
	}
	else
	{
		m_is_active = FALSE;
		m_message_in_queue = FALSE;
		if (m_timer)
			m_timer->Stop();
	}
}

URL_CONTEXT_ID WindowManager::AddWindowToPrivacyModeContext()
{
	m_number_of_windows_in_privacy_mode++;

	if (!m_number_of_windows_in_privacy_mode || !m_privacy_mode_context_id)
	{
		m_privacy_mode_context_id = urlManager->GetNewContextID();
		if (!m_privacy_mode_context_id)
			return 0;

		TRAPD(status, (urlManager->AddContextL(m_privacy_mode_context_id
			, OPFILE_ABSOLUTE_FOLDER, OPFILE_ABSOLUTE_FOLDER, OPFILE_ABSOLUTE_FOLDER, OPFILE_ABSOLUTE_FOLDER
			, FALSE, PrefsCollectionNetwork::DiskCacheSize)));

		if (OpStatus::IsError(status))
			return 0;

		urlManager->SetContextIsRAM_Only(m_privacy_mode_context_id, TRUE);
	}

	return m_privacy_mode_context_id;
}

void WindowManager::RemoveWindowFromPrivacyModeContext()
{
	m_number_of_windows_in_privacy_mode--;
	OP_ASSERT(m_number_of_windows_in_privacy_mode >= 0);

	if (m_number_of_windows_in_privacy_mode == 0 && m_privacy_mode_context_id)
		ExitPrivacyMode();
}

void WindowManager::ExitPrivacyMode()
{
	OP_ASSERT(m_number_of_windows_in_privacy_mode == 0);

	if (m_privacy_mode_context_id)
	{
#ifdef USE_OP_CLIPBOARD
		g_clipboard_manager->EmptyClipboard(m_privacy_mode_context_id);
#endif
		urlManager->RemoveContext(m_privacy_mode_context_id, TRUE);
		m_privacy_mode_context_id = 0;
	}
#ifdef SECMAN_USERCONSENT
	g_secman_instance->OnExitPrivateMode();
#endif // SECMAN_USERCONSENT
#ifdef WINDOW_COMMANDER_TRANSFER
	g_transferManager->RemovePrivacyModeTransfers();
#endif
}

#ifdef WEB_TURBO_MODE
OP_STATUS WindowManager::CheckTuboModeContext()
{
	if( m_turbo_mode_context_id )
		return OpStatus::OK;

	OpFileFolder cache_folder;
	RETURN_IF_ERROR(g_folder_manager->AddFolder(OPFILE_CACHE_FOLDER, UNI_L("turbo"),&cache_folder));

	m_turbo_mode_context_id = urlManager->GetNewContextID();
	if( !m_turbo_mode_context_id )
		return OpStatus::ERR;

	TRAPD(op_err, urlManager->AddContextL(m_turbo_mode_context_id, OPFILE_ABSOLUTE_FOLDER, OPFILE_COOKIE_FOLDER, cache_folder, cache_folder, TRUE, (int) PrefsCollectionNetwork::DiskCacheSize));

	if( OpStatus::IsSuccess(op_err) || OpStatus::IsMemoryError(op_err) )
		return op_err;
	else
		return OpStatus::ERR;
}
#endif // WEB_TURBO_MODE
