/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

// FIXME: clean up these == 96 includes!

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/dochand/docman.h"
#include "modules/dochand/viewportcontroller.h"

#include "modules/doc/html_doc.h"
#include "modules/doc/documentorigin.h"
#include "modules/url/url2.h"
#include "modules/url/url_man.h"
#include "modules/encodings/detector/charsetdetector.h"
#include "modules/inputmanager/inputaction.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/logdoc/savewithinline.h"
#include "modules/forms/formmanager.h"

#ifdef USE_OP_THREAD_TOOLS
#include "modules/pi/OpThreadTools.h"
#endif

#include "modules/hardcore/mh/messages.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/util/timecache.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/collections/pc_print.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/display/prn_info.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/layout/layout_workplace.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/doc/frm_doc.h"
#include "modules/util/gen_str.h"
#include "modules/util/winutils.h"
#include "modules/util/filefun.h"

#include "modules/display/vis_dev.h"
#include "modules/display/coreview/coreview.h"

#include "modules/util/handy.h"

#ifdef QUICK
# include "adjunct/quick/Application.h"
# include "adjunct/quick/managers/KioskManager.h"
# include "adjunct/quick/dialogs/DownloadDialog.h"
# include "adjunct/quick/hotlist/HotlistManager.h"
#endif

#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/windowcommander/src/WindowCommander.h"

#ifdef DRAG_SUPPORT
#include "modules/dragdrop/dragdrop_manager.h"
#endif

#include "modules/history/OpHistoryModel.h"
#include "modules/display/prn_dev.h"

#include "modules/pi/OpWindow.h"
#include "modules/dochand/windowlistener.h"

#include "modules/windowcommander/OpWindowCommanderManager.h"

#if defined(_MACINTOSH_)
# include "platforms/mac/File/FileUtils_Mac.h"
#endif // macintosh latest

#ifdef _PLUGIN_SUPPORT_
#include "modules/ns4plugins/opns4pluginhandler.h"
#endif // _PLUGIN_SUPPORT_

#include "modules/display/VisDevListeners.h"

#ifdef GTK_DOWNLOAD_EXTENSION
# include <gtk/gtk.h>
# include "base/gdk/GdkOpWindow.h"
# include "product/sdk/src/documentwindow.h"
# include "product/sdk/src/bookmarkitemmanager.h"
#endif // GTK_DOWNLOAD_EXTENSION

#include "modules/url/protocols/comm.h"

#ifdef LINK_SUPPORT
#include "modules/logdoc/link.h"
#endif // LINK_SUPPORT

#ifdef NEARBY_ELEMENT_DETECTION
# include "modules/widgets/finger_touch/element_of_interest.h"
#endif // NEARBY_ELEMENT_DETECTION

#ifndef _NO_GLOBALS_
extern HTML_Element *curr_img_element;
#endif

#include "modules/olddebug/timing.h"

#ifdef OBML_COMM_FORCE_CONFIG_FILE
#include "modules/obml_comm/obml_config.h"
#endif // OBML_COMM_FORCE_CONFIG_FILE

extern BOOL ScrollDocument(FramesDocument* doc, OpInputAction::Action action, int times = 1, OpInputAction::ActionMethod method = OpInputAction::METHOD_OTHER);

#ifdef _SPAT_NAV_SUPPORT_
#include "modules/spatial_navigation/sn_handler.h"
#endif // _SPAT_NAV_SUPPORT_

#ifdef SVG_SUPPORT
#include "modules/svg/svg_workplace.h"
#include "modules/svg/svg_image.h"
#endif // SVG_SUPPORT

#ifdef SEARCH_MATCHES_ALL
# include "modules/doc/searchinfo.h"
#endif

#ifdef SEARCH_ENGINES
#include "modules/searchmanager/searchmanager.h"
#endif // SEARCH_ENGINES

#ifdef WAND_SUPPORT
#include "modules/wand/wandmanager.h"
#endif // WAND_SUPPORT

#ifndef HAS_NOTEXTSELECTION
# include "modules/dochand/fdelm.h"
# include "modules/documentedit/OpDocumentEdit.h"
#endif //!HAS_NOTEXTSELECTION

#ifdef CORE_BOOKMARKS_SUPPORT
#include "modules/bookmarks/bookmark_item.h"
#include "modules/bookmarks/bookmark_manager.h"
#endif // CORE_BOOKMARKS_SUPPORT

#define DOC_WINDOW_MAX_TITLE_LENGTH	128

#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
# include "modules/scope/scope_window_listener.h"
#endif // SCOPE_WINDOW_MANAGER_SUPPORT

#ifdef GADGET_SUPPORT
# include "modules/gadgets/OpGadget.h"
#endif // GADGET_SUPPORT

#ifdef APPLICATION_CACHE_SUPPORT
# include "modules/applicationcache/application_cache_manager.h"
#endif // APPLICATION_CACHE_SUPPORT

#ifdef CSS_VIEWPORT_SUPPORT
# include "modules/style/css_viewport.h"
#endif // CSS_VIEWPORT_SUPPORT

#ifdef DEBUG_LOAD_STATUS
# include "modules/olddebug/tstdump.h"
#endif

#ifdef USE_OP_CLIPBOARD
# include "modules/pi/OpClipboard.h"
# ifdef SVG_SUPPORT
#  include "modules/svg/SVGManager.h"
# endif // SVG_SUPPORT
# include "modules/forms/piforms.h"
# include "modules/widgets/OpWidget.h"
#endif // USE_OP_CLIPBOARD

OpenUrlInNewWindowInfo::OpenUrlInNewWindowInfo(URL& url, DocumentReferrer refurl, const uni_char* win_name, BOOL open_in_bg_win, BOOL user_init, unsigned long open_win_id,
											   int open_sub_win_id, BOOL open_in_new_page)
  : new_url(url),
	ref_url(refurl),
	open_in_background_window(open_in_bg_win),
	user_initiated(user_init),
	opener_id(open_win_id),
	opener_sub_win_id(open_sub_win_id),
	open_in_page(open_in_new_page)
{
	window_name = UniSetNewStr(win_name);//FIXME:OOM
}

const OpMessage g_Window_messages[] =
{
	MSG_PROGRESS_START,
	MSG_PROGRESS_END,
#ifdef _PRINT_SUPPORT_
	DOC_START_PRINTING,
	DOC_PRINT_FORMATTED,
	DOC_PAGE_PRINTED,
	DOC_PRINTING_FINISHED,
	DOC_PRINTING_ABORTED,
#endif // _PRINT_SUPPORT_
	MSG_SELECTION_SCROLL,
	MSG_ES_CLOSE_WINDOW,
	MSG_UPDATE_PROGRESS_TEXT,
	MSG_UPDATE_WINDOW_TITLE,
	MSG_HISTORY_CLEANUP,
	WM_OPERA_SCALEDOC,
	MSG_HISTORY_BACK
};

void Window::ConstructL()
{
	forceEncoding.SetL(g_pcdisplay->GetForceEncoding());

	homePage.Empty();

	doc_manager = OP_NEW_L(DocumentManager, (this, NULL, NULL)); // must be set before VisualDevice constructor

	LEAVE_IF_ERROR(doc_manager->Construct());

	msg_handler = OP_NEW_L(MessageHandler, (this));

	LEAVE_IF_ERROR(msg_handler->SetCallBackList(this, 0, g_Window_messages, sizeof g_Window_messages / sizeof g_Window_messages[0]));

#ifdef EPOC
    // Fix for 'hidden' transfer window.
    if (type==WIN_TYPE_DOWNLOAD)
        return;
#endif

	vis_device = VisualDevice::Create(m_opWindow, doc_manager,
									   bShowScrollbars ? VisualDevice::VD_SCROLLING_AUTO : VisualDevice::VD_SCROLLING_NO);
	if (!vis_device)
		LEAVE(OpStatus::ERR_NO_MEMORY);

	doc_manager->SetVisualDevice(vis_device);

	UINT32 rendering_width, rendering_height;
	m_opWindow->GetRenderingBufferSize(&rendering_width, &rendering_height);
	vis_device->SetRenderingViewGeometryScreenCoords(OpRect(0, 0, rendering_width, rendering_height));

	viewportcontroller = OP_NEW_L(ViewportController, (this));

	windowlistener = OP_NEW_L(WindowListener, (this));
	m_opWindow->SetWindowListener(windowlistener);
}

Window::Window(unsigned long nid, OpWindow* opWindow, OpWindowCommander* opWindowCommander)
	: m_features(0)
	, m_online_mode(Window::ONLINE)
	, m_frames_policy(FRAMES_POLICY_DEFAULT)
#ifdef SUPPORT_VISUAL_ADBLOCK
	, m_content_block_edit_mode(FALSE)
	, m_content_block_enabled(TRUE)
	, m_content_block_server_name(NULL)
#endif // SUPPORT_VISUAL_ADBLOCK
	, m_windowCommander(static_cast<WindowCommander*>(opWindowCommander))
	, vis_device(NULL)
	, doc_manager(NULL)
	, msg_handler(NULL)
	, current_message(NULL)
	, current_default_message(NULL)
	, progress_state(WAITING)
	, state(NOT_BUSY)
#ifndef MOUSELESS
	, current_cursor(CURSOR_DEFAULT_ARROW)
	, m_pending_cursor(CURSOR_AUTO)
	, cursor_set_by_doc(CURSOR_DEFAULT_ARROW)
	, has_cursor_set_by_doc(FALSE)
#endif // !MOUSELESS
	// active_link_url unset
	, m_fullscreen_state(OpWindowCommander::FULLSCREEN_NONE)
	// m_previous_fullscreen_state
	, m_is_background_transparent(FALSE)
	// m_old_* unset
	// m_* others: see function body
	, m_is_explicit_suppress_window(FALSE)
	, m_is_implicit_suppress_window(FALSE)
	, m_is_scriptable_window(TRUE)
	, m_is_visible_on_screen(TRUE)
#ifdef _SPAT_NAV_SUPPORT_
	, sn_handler(0)
#endif /* _SPAT_NAV_SUPPORT */
# if defined SN_LEAVE_SUPPORT
	, sn_listener(0)
# endif // SN_LEAVE_SUPPORT
	, m_draw_highlight_rects(TRUE)
	, bCanSelfClose(TRUE) // JS is known to close windows that are in use by others
#ifdef SELFTEST
	, m_is_about_to_close(FALSE)
#endif // SELFTEST
	, m_url_came_from_address_field(FALSE)
	, show_img(FALSE) // may be adjusted in body
#ifdef FIT_LARGE_IMAGES_TO_WINDOW_WIDTH
	, fit_img(FALSE) // may be adjusted in body
#endif // FIT_LARGE_IMAGES_TO_WINDOW_WIDTH
	, load_img(FALSE) // may be adjusted in body
	// is_ssr_mode, era_mode, layout_mode: see body
	, limit_paragraph_width(g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LimitParagraphWidth))
	, flex_root_max_width(0)
	, scale(100)
	, m_text_scale(100)
	, progress_count(0)
	, doc_progress_count(0)
    , upload_total_bytes(0) // Uploading watch
    , phase_uploading(FALSE)
	, start_time(0)

	// progress_mess* arrays - see function body

	, OutputAssociatedWindow(0)
	, bShowScrollbars(g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowScrollbars))

	// bShow* - see function body

	, id(nid)

	, loading(FALSE)
	, end_progress_message_sent(TRUE)
	, history_cleanup_message_sent(FALSE)
	, user_load_from_cache(FALSE)
	, pending_unlock_all_painting(FALSE)
	//, name(NULL)
	, CSSMode((CSSMODE) g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::DocumentMode))
	, forceColors(FALSE)
	, truezoom(FALSE)
	, truezoom_base_scale(100)
	, is_something_hovered(FALSE)
	, ecmascript_disabled(FALSE)
	, ecmascript_paused(NOT_PAUSED)
	, viewportcontroller(NULL)

	, SecurityState(SECURITY_STATE_UNKNOWN)
	, SecurityState_signalled(SECURITY_STATE_UNKNOWN)
	, SecurityState_changed_by_inline(FALSE)
	, SecurityState_signal_delayed(FALSE)
	, m_privacy_mode(FALSE)
	// SecurityTextName unset
#ifdef TRUST_RATING
	, m_trust_rating(Not_Set)
#endif
#ifdef WEB_TURBO_MODE
	, m_turbo_mode(FALSE)
#endif // WEB_TURBO_MODE

	, currWinHistoryLength(0)
	, current_history_number(0)
	, min_history_number(1)
	, max_history_number(0)
	, check_history(FALSE)
	, is_canceling_loading(FALSE)
	, title_update_title_posted(FALSE)

//	, title(NULL)
	, generated_title(FALSE)
#ifdef SHORTCUT_ICON_SUPPORT
	, shortcut_icon_provider(NULL)
#endif // SHORTCUT_ICON_SUPPORT
	, lastUserName(NULL)
	, lastProxyUserName(NULL)
	, type(WIN_TYPE_NORMAL)
	, default_background_color(USE_DEFAULT_COLOR)
#ifdef _PRINT_SUPPORT_
	, printer_info(NULL)
	, preview_printer_info(NULL)
	, preview_mode(FALSE)
	, print_mode(FALSE)
	, frames_print_type(PRINT_ALL_FRAMES)
	, is_formatting_print_doc(FALSE)
	, is_printing(FALSE)
#endif // _PRINT_SUPPORT_
	// m_userAutoReload unset
#ifdef _AUTO_WIN_RELOAD_SUPPORT_
	, m_userAutoReload(this)
#endif // _AUTO_WIN_RELOAD_SUPPORT_

	, opener(NULL)
	, opener_sub_win_id(-1)
	, is_available_to_script(FALSE)
	, opener_origin(NULL)
	, can_be_closed_by_script(FALSE)

#if defined SEARCH_MATCHES_ALL
	, m_search_data(NULL)
#endif // SEARCH_MATCHES_ALL

	, always_load_from_cache(FALSE)

#ifdef LIBOPERA
	, m_lastReqUserInitiated(FALSE)
#endif

	, m_opWindow(opWindow)
	, windowlistener(0)
	// m_bWasInFullscreen unset

#ifdef ACCESS_KEYS_SUPPORT
	// FIXME: Change this when/if your platform has the ability to change this value
	, in_accesskey_mode(FALSE)
#endif // ACCESS_KEYS_SUPPORT
	, window_locked(0)
	, window_closed(FALSE)
	, use_already_requested_urls(TRUE)
	, forced_java_disabled(FALSE)
	, forced_plugins_disabled(FALSE)
	, document_word_wrap_forced(FALSE)
#ifdef WAND_SUPPORT
	, m_wand_in_progress_count(0)
#endif // WAND_SUPPORT
#ifdef HISTORY_SUPPORT
	, m_global_history_enabled(TRUE)
#endif // HISTORY_SUPPORT
#ifdef NEARBY_ELEMENT_DETECTION
	, element_expander(NULL)
#endif // NEARBY_ELEMENT_DETECTION
#ifdef GRAB_AND_SCROLL
	, scroll_is_pan_overridden(FALSE)
	, scroll_is_pan(FALSE)
#endif // GRAB_AND_SCROLL
	, recently_cancelled_keydown(OP_KEY_INVALID)
	, has_shown_unsolicited_download_dialog(FALSE)
#ifdef DOM_JIL_API_SUPPORT
	, m_screen_props_listener(NULL)
#endif // DOM_JIL_API_SUPPORT
	, m_oom_occurred(FALSE)
	, m_next_doclistelm_id(1)
#ifdef SCOPE_PROFILER
	, m_profiling_session(NULL)
#endif // SCOPE_PROFILER
#ifdef KEYBOARD_SELECTION_SUPPORT
	, m_current_keyboard_selection_mode(FALSE)
#endif // KEYBOARD_SELECTION_SUPPORT
{
	OP_ASSERT(opWindowCommander);
	switch ((SHOWIMAGESTATE) g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowImageState))
	{
	case FIGS_OFF: break;
	case FIGS_ON: load_img = TRUE; /* fall through */
	case FIGS_SHOW: show_img = TRUE; break;
	}

	int rm = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::RenderingMode);
	if (rm < 0)
	{
		era_mode = TRUE;
		rm = 0;
	}
	else
		era_mode = FALSE;

	layout_mode = (LayoutMode) rm;
	is_ssr_mode = layout_mode == LAYOUT_SSR;

	SetFlexRootMaxWidth(g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::FlexRootMaxWidth), FALSE);

	progress_mess[0] = 0;
	progress_mess_low[0] = 0;

	SetFeatures(type);
	default_background_color = colorManager->GetBackgroundColor();
}

BOOL Window::Close()
{
	if (window_locked)
	{
		window_closed = TRUE;
#ifdef SELFTEST
		// Signal to the selftest system that this window is about to close and
		// shouldn't be reused.
		SetIsAboutToClose(TRUE);
#endif // SELFTEST
	}
	else
	{
		m_windowCommander->GetDocumentListener()->OnClose(m_windowCommander);
		g_windowCommanderManager->GetUiWindowListener()->CloseUiWindow(m_windowCommander);
	}

    return TRUE;
}

void Window::SetOpener(Window* opened_by, int sub_win_id, BOOL make_available_to_script, BOOL from_window_open)
{
	opener = opened_by;
	opener_sub_win_id = sub_win_id;
	is_available_to_script = make_available_to_script;
	if (!can_be_closed_by_script && from_window_open)
		can_be_closed_by_script = TRUE;

	if (FramesDocument *document = GetOpener())
	{
		OP_ASSERT(!opener_origin);
		opener_origin = document->GetMutableOrigin();
		opener_origin->IncRef();
	}

#ifdef CLIENTSIDE_STORAGE_SUPPORT
	if (opened_by)
		DocManager()->SetStorageManager(opened_by->DocManager()->GetStorageManager(FALSE));
#endif // CLIENTSIDE_STORAGE_SUPPORT
}

FramesDocument* Window::GetOpener(BOOL is_script/*=TRUE*/)
{
	FramesDocument* doc = NULL;

	if (opener && (is_available_to_script || !is_script))
	{
		DocumentManager* doc_man = opener->GetDocManagerById(opener_sub_win_id);

		if (!doc_man)
			doc_man = opener->DocManager();

		doc = doc_man->GetCurrentDoc();
	}

	return doc;
}

URL Window::GetOpenerSecurityContext()
{
	if (opener_origin)
		return opener_origin->security_context;
	return URL();
}

#ifndef MOUSELESS
CursorType Window::GetCurrentCursor()
{
	if (has_cursor_set_by_doc)
		return cursor_set_by_doc;

	switch (state)
	{
	case BUSY:
		return IsNormalWindow() ? CURSOR_WAIT : CURSOR_DEFAULT_ARROW;

	case CLICKABLE:
		return IsNormalWindow() ? CURSOR_ARROW_WAIT : CURSOR_DEFAULT_ARROW;

	default: // There should be no other states.
		OP_ASSERT(0);
		/* fall through */
	case NOT_BUSY:
	case RESERVED: // The RESERVED state is currently unused.
		return CURSOR_DEFAULT_ARROW;
	}
}

CursorType Window::GetCurrentArrowCursor()
{
	// Disable the doc cursor and ask what the cursor would be in that case
	BOOL doc_cursor = has_cursor_set_by_doc;

	has_cursor_set_by_doc = FALSE;
	CursorType arrow_cursor = GetCurrentCursor();
	has_cursor_set_by_doc = doc_cursor;

	return arrow_cursor;
}

#endif // !MOUSELESS

void Window::SetState(WinState s)
{
	state = s;

#ifndef MOUSELESS
	SetCursor(GetCurrentCursor());
#endif // !MOUSELESS
}

Window::~Window()
{
#if defined _PRINT_SUPPORT_
	if (is_printing)
		StopPrinting();
#endif // _PRINT_SUPPORT_

#ifdef NEARBY_ELEMENT_DETECTION
	SetElementExpander(NULL);
#endif // NEARBY_ELEMENT_DETECTION

#ifdef WAND_SUPPORT
	if (m_wand_in_progress_count)
	{
		g_wand_manager->UnreferenceWindow(this);
		OP_ASSERT(m_wand_in_progress_count == 0);
	}
#endif // WAND_SUPPORT

	if (opener_origin)
	{
		opener_origin->DecRef();
		opener_origin = NULL;
	}

	if (GetPrivacyMode())
		g_windowManager->RemoveWindowFromPrivacyModeContext();

	// Remove all references to msg_handler from urlManager
	// Must be done before deleting the doc_manager because it may call back to our listener where we use it.
	if (urlManager && msg_handler)
		urlManager->RemoveMessageHandler(msg_handler);

	loading = FALSE;
	if (doc_manager) // Can be NULL if Construct failed and we're deleting a half-created object
	{
		doc_manager->Clear();
		OP_DELETE(doc_manager);
	}
    if (vis_device)
    {
	    vis_device->SetDocumentManager(NULL);
		OP_DELETE(vis_device);
	}
	if (msg_handler)
	{
		msg_handler->UnsetCallBacks(this);
		OP_DELETE(msg_handler);
	}
	OP_DELETEA(current_default_message);
	OP_DELETEA(current_message);

	if (urlManager)
		urlManager->FreeUnusedResources(FALSE);

    if (m_opWindow)
	    m_opWindow->SetWindowListener(NULL);
	OP_DELETE(windowlistener); windowlistener=NULL;

#ifdef _SPAT_NAV_SUPPORT_
	OP_DELETE(sn_handler); sn_handler=NULL;
#endif // _SPAT_NAV_SUPPORT_

#ifdef SHORTCUT_ICON_SUPPORT
	if (shortcut_icon_provider)
		shortcut_icon_provider->DecRef(NULL);
#endif

#if defined SEARCH_MATCHES_ALL
	OP_DELETE(m_search_data);
#endif // SEARCH_MATCHES_ALL

#ifdef FONTCACHE_PER_WINDOW
	g_font_cache->ClearForWindow(this);
#endif // FONTCACHE_PER_WINDOW

	OP_DELETE(viewportcontroller);

#ifdef USE_OP_CLIPBOARD
	g_clipboard_manager->UnregisterListener(this);
#endif // USE_OP_CLIPBOARD
}

void Window::EnsureHistoryLimits()
{
	if (GetHistoryLen() > g_pccore->GetIntegerPref(PrefsCollectionCore::MaxWindowHistory))
		SetMaxHistory(g_pccore->GetIntegerPref(PrefsCollectionCore::MaxWindowHistory));
}

OP_STATUS Window::UpdateTitle(BOOL delay)
{
	if (delay)
	{
		if (!title_update_title_posted)
		{
			msg_handler->PostMessage(MSG_UPDATE_WINDOW_TITLE, 0 , 0);
			title_update_title_posted = TRUE;
		}
		return OpStatus::OK;
	}

    URL url = GetCurrentURL();
	FramesDocument* doc = doc_manager->GetCurrentDoc();

	if (!doc)
		return OpStatus::ERR;

	OpString title;

	BOOL generated = FALSE;

	TempBuffer buffer;
	const uni_char* str = doc->Title(&buffer);

	URL& origurl = url;

	if (str && *str)
		RETURN_IF_ERROR(title.Set(str));
	else
	{
		// Might be nice to send an unescaped string to the UI but then we have to know the charset
		// and decode it correctly. Bug 250545.
		RETURN_IF_ERROR(origurl.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped,title));
		generated = TRUE;
	}

	RETURN_IF_ERROR(SetWindowTitle(title, FALSE, generated));
	RETURN_IF_ERROR(SetProgressText(url));

#ifdef HISTORY_SUPPORT
	// Update the title in the entry in global history
	if (doc_manager->GetCurrentDoc())
		RETURN_IF_ERROR(AddToGlobalHistory(doc_manager->GetCurrentDoc()->GetURL(), title.CStr()));
#endif // HISTORY_SUPPORT

#ifdef CPUUSAGETRACKING
	TempBuffer cpuusage_display_name;
	switch (type)
	{
	case WIN_TYPE_NORMAL:
	case WIN_TYPE_GADGET:
		{
			OpString prefix;
			OpStatus::Ignore(g_languageManager->GetString(
				type == WIN_TYPE_NORMAL ? Str::S_OPERACPU_WINDOW_SOURCE_PREFIX : Str::S_OPERACPU_WIDGET_SOURCE_PREFIX,
				prefix));
			OpStatus::Ignore(cpuusage_display_name.Append(prefix.CStr(), prefix.Length()));
			OpStatus::Ignore(cpuusage_display_name.Append(' '));
			break;
		}
	default:
		; // No prefix
	}
	OpStatus::Ignore(cpuusage_display_name.Append(title.CStr()));
	OpStatus::Ignore(cpu_usage_tracker.SetDisplayName(cpuusage_display_name.GetStorage()));
	if (type == WIN_TYPE_NORMAL)
		cpu_usage_tracker.SetActivatable();
#endif // CPUUSAGETRACKING

	return OpStatus::OK;
}

void Window::SetLoading(BOOL set)
{
	loading = set;
}

OP_STATUS Window::StartProgressDisplay(BOOL bReset, BOOL bResetSecurityState, BOOL bSubResourcesOnly)
{
	if (!end_progress_message_sent)
	{
		// If we start loading a document, make sure UI is told about it.
		if (!m_loading_information.hasRequestedDocument && !bSubResourcesOnly)
		{
			m_loading_information.hasRequestedDocument = TRUE;
			m_windowCommander->GetLoadingListener()->OnLoadingProgress(m_windowCommander, &m_loading_information);
		}
		return OpStatus::OK;
	}

	start_time = g_timecache->CurrentTime();

	if (bResetSecurityState)
		ResetSecurityStateToUnknown();

	if (bReset)
		ResetProgressDisplay();

	m_loading_information.hasRequestedDocument |= !bSubResourcesOnly;

#ifdef DOCHAND_CLEAR_RAMCACHE_ON_STARTPROGRESS
	if( bReset )
		urlManager->CheckRamCacheSize( 0 );
#endif

	msg_handler->PostMessage(MSG_PROGRESS_START, 0, 0);

	end_progress_message_sent = FALSE; // If there was one earlier, that will be for an earlier progress and we should not care more about that one

	return OpStatus::OK;
}



void Window::EndProgressDisplay()
{
	SetState(NOT_BUSY);

	CSSCollection* coll = GetCSSCollection();
	CSS* css = NULL;
	if (coll)
	{
		CSSCollection::Iterator iter(coll, CSSCollection::Iterator::STYLESHEETS);
		do {
			css = static_cast<CSS *>(iter.Next());
		} while (css && css->GetKind() == CSS::STYLE_PERSISTENT);
	}

	if (css != NULL)
	{
		m_windowCommander->GetLinkListener()->OnAlternateCssAvailable(m_windowCommander);
	}

#ifdef LINK_SUPPORT
	const LinkElement* linkElt = GetLinkList();
	if (linkElt != NULL)
	{
		m_windowCommander->GetLinkListener()->OnLinkElementAvailable(m_windowCommander);
	}
#endif // LINK_SUPPORT

	if (!end_progress_message_sent)
	{
		URL url = doc_manager->GetCurrentURL();
		OpLoadingListener::LoadingFinishStatus status;
		// FIXME: backstrom, are there more status to report?
		if (doc_manager->ErrorPageShown())  // If we've shown an error page instead of the page itself loading failed
			status = OpLoadingListener::LOADING_COULDNT_CONNECT;
		else if(url.Status(TRUE) == URL_LOADED)
			status = OpLoadingListener::LOADING_SUCCESS;
		else if(url.Status(TRUE) == URL_LOADING_FAILURE)
			status = OpLoadingListener::LOADING_COULDNT_CONNECT;
		else
			status = OpLoadingListener::LOADING_UNKNOWN;

		msg_handler->PostMessage(MSG_PROGRESS_END, 0, status);
		end_progress_message_sent = TRUE;
		pending_unlock_all_painting = TRUE;
	}
	else if (!pending_unlock_all_painting)
	{
		// Normally this is done on the MSG_PROGRESS_END message (and changing that
		// causes performance regressions which are not fully understood yet) but in case
		// we locked painting and then never started a progress bar (think javascript
		// urls and bookmarklets) we need to unlock it somewhere else, i.e. here.
		UnlockPaintLock();
	}

#ifdef HTTP_BENCHMARK
	EndBenchmark();
#endif
}

void Window::ResetProgressDisplay()
{
	SetProgressCount(0);
	SetDocProgressCount(0);
	m_loading_information.Reset();
	SetProgressState(WAITING, NULL, NULL, 0, NULL);
    SetUploadTotal(0);
}

void Window::SetTrueZoom(BOOL status)
{
	truezoom = status;
	if (scale != 100)
		SetScale(scale);
}

void Window::SetTrueZoomBaseScale(UINT32 scale)
{
	if (scale == truezoom_base_scale)
		return;

	truezoom_base_scale = scale;
	DocumentTreeIterator it(doc_manager);

	it.SetIncludeThis();
	it.SetIncludeEmpty();

	while (it.Next())
	{
		VisualDevice* vd = it.GetDocumentManager()->GetVisualDevice();
		FramesDocument* doc =  it.GetFramesDocument();

#ifdef CSS_VIEWPORT_SUPPORT
		if (doc && doc->GetHLDocProfile())
		{
			doc->RecalculateLayoutViewSize(TRUE);
			if (doc->IsTopDocument())
				doc->RequestSetViewportToInitialScale(VIEWPORT_CHANGE_REASON_BASE_SCALE);
		}
#endif // CSS_VIEWPORT_SUPPORT

		// Can it be null?
		if (vd)
		{
			vd->SetLayoutScale(scale);
			if (doc && doc->GetDocRoot())
				doc->GetDocRoot()->RemoveCachedTextInfo(doc);
		}
	}
	OpViewportInfoListener* info_listener = viewportcontroller->GetViewportInfoListener();
	info_listener->OnTrueZoomBaseScaleChanged(viewportcontroller, scale);
}

void Window::SetScale(INT32 sc)
{
	if (sc < DOCHAND_MIN_DOC_ZOOM)
		sc = DOCHAND_MIN_DOC_ZOOM;
	else if (sc > DOCHAND_MAX_DOC_ZOOM)
		sc = DOCHAND_MAX_DOC_ZOOM;

	BOOL changed = (scale != sc);

	scale = sc;

#ifdef PAGED_MEDIA_SUPPORT
	if (changed && IsFullScreen())
		if (FramesDocument *current_doc = GetCurrentDoc())
			if (LogicalDocument* logdoc = current_doc->GetLogicalDocument())
				current_doc->SetRestoreCurrentPageNumber(logdoc->GetLayoutWorkplace()->GetCurrentPageNumber());
#endif // PAGED_MEDIA_SUPPORT

	doc_manager->SetScale( scale );

	// A subdocument may now have individual scalefactor. That's why we call doc_manager->SetScale always. But we don't tell windowcommander
	// if the window scale didn't change.
	if (!changed)
		return;

	m_windowCommander->GetDocumentListener()->OnScaleChanged(m_windowCommander, scale);
}

void Window::SetTextScale(int sc)
{
	if (sc < DOCHAND_MIN_DOC_ZOOM)
		sc = DOCHAND_MIN_DOC_ZOOM;
	else if (sc > DOCHAND_MAX_DOC_ZOOM)
		sc = DOCHAND_MAX_DOC_ZOOM;

	m_text_scale = sc;

	if (doc_manager->GetCurrentDoc())
		doc_manager->GetCurrentDoc()->SetTextScale(m_text_scale);

	if (doc_manager->GetVisualDevice())
		doc_manager->GetVisualDevice()->SetTextScale(m_text_scale);
}

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
int Window::GetPixelScale()
{
	return m_opWindow->GetPixelScale();
}
#endif // PIXEL_SCALE_RENDERING_SUPPORT

const uni_char*	Window::GetProgressMessage()
{
	if(progress_mess[0] == 0)
	{
		uni_strcpy(progress_mess, progress_mess_low);
		progress_mess_low[0] = 0;
	}
	return progress_mess;
}

void Window::SetUploadTotal(OpFileLength progress_info1)
{
    upload_total_bytes = progress_info1;
}

void Window::SetUploadFileCount(int total, int loaded)
{
	upload_file_count = total;
	upload_file_loaded_count = loaded;
}

void Window::SetProgressState(ProgressState state, const uni_char* extrainfo, MessageHandler *mh, long bytes, URL *loading_url)
{
    OP_MEMORY_VAR BOOL upload_started = FALSE;
    OP_MEMORY_VAR BOOL upload_ended = FALSE;

	if (state == REQUEST_FINISHED)
	{
		msg_handler->PostMessage(MSG_REFRESH_PROGRESS, 0 , 0);

		if (*progress_mess_low)
			return;

		progress_mess[0] = 0;
	}

	if (state != progress_state || extrainfo)
	{
		progress_state = state;

		Str::LocaleString string_number = Str::NOT_A_STRING;
		OP_MEMORY_VAR BOOL use_numeric_syntax = FALSE; /* Use %1 %2 instead of %s etc. */

		switch (state)
		{
		case WAITING_FOR_COOKIES:
			string_number = Str::SI_MPAR_URL_WAITING_FOR_COOKIES;
			break;

		case WAITING_FOR_COOKIES_DNS:
			string_number = Str::SI_MPAR_URL_WAITING_FOR_COOKIES_DNS;
			break;

		case REQUEST_QUEUED:
			string_number = Str::SI_MPAR_URL_REQUEST_QUEUED_STRING;
			break;

		case START_NAME_LOOKUP:
			string_number = Str::SI_MPAR_URL_START_NAME_LOOKUP_STRING;
			break;

		case START_NAME_COMPLETION:
			string_number = Str::SI_MPAR_URL_START_NAME_COMPLETION_STRING;
			break;

		case START_SECURE_CONNECTION:
			string_number = Str::SI_MPAR_URL_START_SECURE_CONNECTION_STRING;
			break;

		case START_CONNECT_PROXY:
			string_number = Str::SI_MPAR_URL_START_CONNECT_PROXY_STRING;
			break;

		case WAITING_FOR_CONNECTION:
			string_number = Str::SI_MPAR_URL_WAITING_FOR_CONNECTION_STRING;
			use_numeric_syntax = TRUE;
			break;

		case START_CONNECT:
			string_number = Str::SI_MPAR_URL_START_CONNECT_STRING;
			break;

		case START_REQUEST:
			string_number = Str::SI_MPAR_URL_START_REQUEST_STRING;
			break;

		case STARTED_40SECOND_TIMEOUT:
			string_number = Str::SI_MPAR_URL_STARTED_40SECOND_TIMEOUT;
			break;

		case UPLOADING_FILES:
            upload_started = TRUE;
			string_number = Str::SI_UPLOADING_FILES_STRING;
#ifdef PROGRESS_EVENTS_SUPPORT
			if (mh && loading_url && bytes > 0)
				UpdateUploadProgress(mh, *loading_url, static_cast<unsigned long>(bytes));
#endif // PROGRESS_EVENTS_SUPPORT
			break;

		case FETCHING_DOCUMENT:
			string_number = Str::SI_FETCHING_DOCUMENT_STRING;
			break;

		case EXECUTING_ECMASCRIPT:
			string_number = Str::SI_EXECUTING_ECMASCRIPT_STRING;
			break;
		case REQUEST_FINISHED:
			string_number = Str::SI_MPAR_URL_REQUEST_FINISHED_STRING;
			break;

        case UPLOADING_FINISHED:
            upload_ended = TRUE;
            break;
		}

		if (state == WAITING || string_number == Str::NOT_A_STRING)
		{
			ClearMessage();
			progress_mess_low[0] = '\0';
			progress_mess[0] = '\0';
		}
		else
		{
            OpString extra;
			OpString string;
			OpString message;

			TRAPD(err, message.ReserveL(MAX_PROGRESS_MESS_LENGTH));
			if (OpStatus::IsMemoryError(err))
			{
				RaiseCondition(err);
				return;
			}
			TRAP(err, g_languageManager->GetStringL(string_number, string));
			if (OpStatus::IsMemoryError(err))
			{
				RaiseCondition(err);
				return;
			}
            if (extrainfo)
				OpStatus::Ignore(extra.Set(extrainfo));

			if (string.CStr())
			{
				if (!use_numeric_syntax)
				{
					uni_snprintf(message.CStr(), message.Capacity(), string.CStr(), (extra.Length() ? extra.CStr() : UNI_L("")));
				}
				else //Uses %1
				{
					uni_snprintf_si(message.CStr(), message.Capacity(), string.CStr(), (extra.Length() ? extra.CStr() : UNI_L("")),
									(state == WAITING_FOR_CONNECTION ? Comm::CountWaitingComm(COMM_WAIT_STATUS_LOAD) : 0)
						);
				}
			}

			if (uni_strncmp(progress_mess, message.CStr(), MAX_PROGRESS_MESS_LENGTH) == 0)
				return;

			if(state == REQUEST_FINISHED)
			{
				uni_strlcpy(progress_mess_low, message.CStr(), MAX_PROGRESS_MESS_LENGTH);
			}
			else
			{
				uni_strlcpy(progress_mess, message.CStr(), MAX_PROGRESS_MESS_LENGTH);
			}
		}

#ifdef EPOC
		// Certain windows are not prepared for interaction with WindowCommander.
		if (type==WIN_TYPE_DOWNLOAD)
			return;
#endif
		m_loading_information.loadingMessage = GetProgressMessage();
		FramesDocument* doc = GetCurrentDoc();
		if (doc &&
			(doc_manager->GetLoadStatus() == NOT_LOADING || doc_manager->GetLoadStatus() == DOC_CREATED) &&
			!upload_started &&
			!phase_uploading)
		{
			DocLoadingInfo info;
			doc->GetDocLoadingInfo(info);
			m_loading_information.loadedImageCount = info.images.loaded_count;
			m_loading_information.totalImageCount = info.images.total_count;
			m_loading_information.loadedBytes = info.loaded_size;
			m_loading_information.totalBytes = info.total_size;
			m_loading_information.loadedBytesRootDocument = GetDocProgressCount();
			m_loading_information.totalBytesRootDocument = DocManager()->GetCurrentURL().GetContentSize();
#ifdef WEB_TURBO_MODE
			m_loading_information.turboTransferredBytes = info.turbo_transferred_bytes + info.images.turbo_transferred_bytes;
			m_loading_information.turboOriginalTransferredBytes = info.turbo_original_transferred_bytes + info.images.turbo_original_transferred_bytes;
#endif // WEB_TURBO_MODE
		}
		m_loading_information.isUploading = ((state == UPLOADING_FILES) || (state == UPLOADING_FINISHED));
		if(upload_started && !phase_uploading)
		{
			OpFileLength upload_total;
			DocManager()->GetCurrentURL().GetAttribute(URL::KHTTP_Upload_TotalBytes, &upload_total, URL::KFollowRedirect);
			SetUploadTotal(upload_total);
			phase_uploading = TRUE;
			m_loading_information.totalBytes = upload_total_bytes;
			m_windowCommander->GetLoadingListener()->OnStartUploading(m_windowCommander);
		}
		if (m_loading_information.isUploading)
		{
			m_loading_information.totalImageCount = upload_file_count;
			m_loading_information.loadedImageCount = upload_file_loaded_count;
		}
		m_windowCommander->GetLoadingListener()->OnLoadingProgress(m_windowCommander, &m_loading_information);
		if(upload_ended && phase_uploading)
		{
			phase_uploading = FALSE;
			m_windowCommander->GetLoadingListener()->OnUploadingFinished(m_windowCommander, OpLoadingListener::LOADING_SUCCESS);
#ifdef PROGRESS_EVENTS_SUPPORT
			if (mh && loading_url)
				UpdateUploadProgress(mh, *loading_url, 0);
#endif // PROGRESS_EVENTS_SUPPORT
		}
	}
}

#ifdef PROGRESS_EVENTS_SUPPORT
void Window::UpdateUploadProgress(MessageHandler *mh, const URL &loading_url, unsigned long bytes)
{
	mh->PostMessage(MSG_DOCHAND_UPLOAD_PROGRESS, loading_url.Id(), bytes);
}
#endif // PROGRESS_EVENTS_SUPPORT

Window* Window::GetOutputAssociatedWindow()
{
	return g_windowManager->GetWindow(OutputAssociatedWindow);
}

void Window::SetFeature(Window_Feature feature)
{
	m_features |= feature;
}

void Window::SetFeatures(Window_Type type)
{
	m_features = 0;

	switch (type)
	{
		case WIN_TYPE_JS_CONSOLE:
		case WIN_TYPE_HISTORY:
		case WIN_TYPE_NORMAL:
		case WIN_TYPE_CACHE:
		case WIN_TYPE_PLUGINS:
		case WIN_TYPE_NORMAL_HIDDEN:
			SetFeature(WIN_FEATURE_ALERTS);
			SetFeature(WIN_FEATURE_PROGRESS);
			SetFeature(WIN_FEATURE_NAVIGABLE);
			SetFeature(WIN_FEATURE_PRINTABLE);
			SetFeature(WIN_FEATURE_RELOADABLE);
			SetFeature(WIN_FEATURE_FOCUSABLE);
			SetFeature(WIN_FEATURE_HOMEABLE);
			SetFeature(WIN_FEATURE_BOOKMARKABLE);
			break;

#ifndef _NO_HELP_
		case WIN_TYPE_HELP:
			SetFeature(WIN_FEATURE_NAVIGABLE);
			SetFeature(WIN_FEATURE_PRINTABLE);
			SetFeature(WIN_FEATURE_RELOADABLE);
			SetFeature(WIN_FEATURE_FOCUSABLE);
			SetFeature(WIN_FEATURE_OUTSIDE);
			SetFeature(WIN_FEATURE_BOOKMARKABLE);
			break;
#endif // !_NO_HELP_

		case WIN_TYPE_DOWNLOAD:
			SetFeature(WIN_FEATURE_FOCUSABLE);	//	NOT for mswin
			break;

		case WIN_TYPE_MAIL_COMPOSE:
			SetFeature(WIN_FEATURE_FOCUSABLE);
			break;

		case WIN_TYPE_MAIL_VIEW:
		case WIN_TYPE_NEWSFEED_VIEW:
			SetFeature(WIN_FEATURE_PRINTABLE);
			SetFeature(WIN_FEATURE_FOCUSABLE);
			break;

		case WIN_TYPE_IM_VIEW:
			SetFeature(WIN_FEATURE_PRINTABLE);
			SetFeature(WIN_FEATURE_FOCUSABLE);
			break;

		case WIN_TYPE_BRAND_VIEW:
			SetFeature(WIN_FEATURE_FOCUSABLE);
			SetFeature(WIN_FEATURE_BOOKMARKABLE);
			SetFeature(WIN_FEATURE_RELOADABLE);
			break;

		case WIN_TYPE_GADGET:
		case WIN_TYPE_CONTROLLER:
			SetFeature(WIN_FEATURE_FOCUSABLE);
			SetFeature(WIN_FEATURE_RELOADABLE);
			break;

		case WIN_TYPE_INFO:
			// no features
			break;

#ifdef _PRINT_SUPPORT_
		case WIN_TYPE_PRINT_SELECTION:
			SetFeature(WIN_FEATURE_PRINTABLE);
			break;
#endif // _PRINT_SUPPORT_

		case WIN_TYPE_DIALOG:
			SetFeature(WIN_FEATURE_NAVIGABLE);
			SetFeature(WIN_FEATURE_FOCUSABLE);
			break;

		case WIN_TYPE_THUMBNAIL:
			SetFeature(WIN_FEATURE_RELOADABLE);
			break;

		case WIN_TYPE_DEVTOOLS:
			SetFeature(WIN_FEATURE_ALERTS);
			SetFeature(WIN_FEATURE_PRINTABLE);
			SetFeature(WIN_FEATURE_RELOADABLE);
			SetFeature(WIN_FEATURE_FOCUSABLE);
			break;

		default:
			// if this happens, add more window types or correct your code!
			OP_ASSERT(true == false);
	}
}

void Window::RemoveFeature(Window_Feature feature)
{
	m_features &= ~feature;
}


BOOL Window::HasFeature(Window_Feature feature)
{
	if (m_features & feature)
		return TRUE;
	else
		return FALSE;
}

BOOL Window::HasAllFeatures(Window_Feature feature)
{
	if ((m_features & feature) == (unsigned long) feature)
		return TRUE;
	else
		return FALSE;
}

OP_STATUS Window::SetFullScreenState(OpWindowCommander::FullScreenState state)
{
	if (m_fullscreen_state == state)
		return OpStatus::OK;

	FramesDocument *doc = doc_manager->GetCurrentDoc();

	/* Store the new state before calling DOMExitFullscreen() so it does not notify the platform
	 * about turning fullscreen off (the platform is the one which turned it off).
	 */
	m_fullscreen_state = state;

	if (doc)
	{
#ifdef DOM_FULLSCREEN_MODE
		if (state == OpWindowCommander::FULLSCREEN_NONE)
		{
			// Fullscreen was disabled.
			RETURN_IF_MEMORY_ERROR(doc->DOMExitFullscreen(NULL));
		}
#endif // DOM_FULLSCREEN_MODE

		// Try to keep the scroll positions
		doc_manager->StoreViewport(doc_manager->CurrentDocListElm());
	}

	vis_device->SetScrollType(m_fullscreen_state != OpWindowCommander::FULLSCREEN_PROJECTION && bShowScrollbars ? VisualDevice::VD_SCROLLING_AUTO : VisualDevice::VD_SCROLLING_NO);

	if (doc)
	{
		RETURN_IF_MEMORY_ERROR(UpdateWindow()); // cause reformat because stylesheets may be different

		BOOL make_fit = doc->IsLocalDocFinished();
		doc_manager->RestoreViewport(TRUE, TRUE, make_fit);
	}
	return OpStatus::OK;
}

void Window::SetOutputAssociatedWindow(Window* ass)
{
	OutputAssociatedWindow = ass ? ass->Id() : 0;
}

void Window::SetShowScrollbars(BOOL show)
{
	bShowScrollbars = show;

	vis_device->SetScrollType(bShowScrollbars ? VisualDevice::VD_SCROLLING_AUTO : VisualDevice::VD_SCROLLING_NO);

	if (FramesDocument *doc = doc_manager->GetCurrentDoc())
	{
		doc->RecalculateScrollbars();
		doc->RecalculateLayoutViewSize(TRUE);
	}
}

void Window::SetName(const uni_char *n)
{
	OP_STATUS ret = name.Set(n);
	if (OpStatus::IsError(ret))
	{
		RaiseCondition(ret);
	}
}

void Window::SetNextCSSMode()
{
	switch (CSSMode)
	{
	case CSS_FULL:
		SetCSSMode(CSS_NONE);
		break;
	default:
		SetCSSMode(CSS_FULL);
		break;
	}
}

void Window::SetCSSMode(CSSMODE f)
{
	CSSMode = f;

	OpDocumentListener::CssDisplayMode mode;

	if (CSSMode == CSS_FULL)
	{
		mode = OpDocumentListener::CSS_AUTHOR_MODE;
	}
	else
	{
		mode = OpDocumentListener::CSS_USER_MODE;
	}

	m_windowCommander->GetDocumentListener()->OnCssModeChanged(m_windowCommander, mode);

	OP_STATUS ret = UpdateWindow();
	if (OpStatus::IsMemoryError(ret))
	{
		RaiseCondition(ret);//FIXME:OOM - Can't return OOM err.
	}
}


void Window::SetForceEncoding(const char *encoding, BOOL isnewwin)
{
	OP_STATUS ret = forceEncoding.Set(encoding);
	if (OpStatus::IsMemoryError(ret))
	{
		RaiseCondition(ret);//FIXME:OOM - Can't return OOM err.
	}

	// Update document in window
	if (!isnewwin)
	{
		ret = doc_manager->UpdateCurrentDoc(FALSE, FALSE, FALSE);
		if (OpStatus::IsMemoryError(ret))
		{
			RaiseCondition(ret);//FIXME:OOM - Can't return OOM err.
		}
	}
}

void Window::UpdateVisitedLinks(const URL& url)
{
	doc_manager->UpdateVisitedLinks(url);
}

#ifdef SHORTCUT_ICON_SUPPORT

# ifdef ASYNC_IMAGE_DECODERS
void Window::OnPortionDecoded()
{
	if (shortcut_icon_provider && shortcut_icon_provider->GetImage().ImageDecoded())
	{
		m_windowCommander->GetDocumentListener()->OnDocumentIconAdded(m_windowCommander);
	}
}
# endif // ASYNC_IMAGE_DECODERS

#ifdef SVG_SUPPORT
OP_STATUS Window::SetSVGAsWindowIcon(SVGImageRef* ref)
{
	if(!shortcut_icon_provider)
	{
		URL* url = ref->GetUrl();
		shortcut_icon_provider = UrlImageContentProvider::FindImageContentProvider(*url);
		if (shortcut_icon_provider == NULL)
		{
			shortcut_icon_provider = OP_NEW(UrlImageContentProvider, (*url));
			if (shortcut_icon_provider == NULL)
				return OpStatus::ERR_NO_MEMORY;
		}
	}

	shortcut_icon_provider->SetSVGImageRef(ref);
	shortcut_icon_provider->IncRef(NULL);

	return OpStatus::OK;
}

void Window::RemoveSVGWindowIcon(SVGImageRef* ref)
{
	if (shortcut_icon_provider && shortcut_icon_provider->GetSVGImageRef() == ref)
	{
		shortcut_icon_provider->DecRef(NULL);
		shortcut_icon_provider = NULL;
	}
}
#endif // SVG_SUPPORT

OP_STATUS Window::SetWindowIcon(URL* url)
{
	// proposal:
	// make core passive and let ui query document (via Window) each
	// time a page is loaded?

	if (url)
	{
		if (url->Status(TRUE) != URL_LOADED)
		{
			url = NULL;
			if (FramesDocument *frm_doc = GetCurrentDoc())
			{
				URL *icon = frm_doc->Icon();
				HTML_Element *he = frm_doc->GetDocRoot();
				if (icon && he)
				{
					OP_BOOLEAN loading = frm_doc->LoadIcon(icon, he);
					if (OpStatus::IsMemoryError(loading))
						return loading;
				}

				return OpStatus::OK;
			}
		}
		else if (url->Type() == URL_HTTP || url->Type() == URL_HTTPS)
		{
			int response = url->GetAttribute(URL::KHTTP_Response_Code);
			if (response != HTTP_OK && response != HTTP_NOT_MODIFIED)
				url = NULL;
		}
	}

#ifdef SVG_SUPPORT
	SVGImageRef* ref = NULL;
	if (url && url->ContentType() == URL_SVG_CONTENT)
	{
		ref = shortcut_icon_provider ? shortcut_icon_provider->GetSVGImageRef() : NULL;
		if(ref)
		{
			shortcut_icon_provider->SetSVGImageRef(NULL); // detach for a moment
		}
		else
		{
			OP_STATUS status = OpStatus::ERR;

			if (FramesDocument* frm_doc = GetCurrentDoc())
				if (LogicalDocument* logdoc = frm_doc->GetLogicalDocument())
				{
					status = logdoc->GetSVGWorkplace()->GetSVGImageRef(*url, &ref);

					if (OpStatus::IsSuccess(status))
						SetSVGAsWindowIcon(ref);
				}

			return status;
		}
	}

	// Need to render the SVG icon if we have a ref parameter...
	if(!ref)
#endif // SVG_SUPPORT
	{
		if (url ? *url == m_shortcut_icon_url : m_shortcut_icon_url.IsEmpty())
			return OpStatus::OK;
	}

	if (url)
		m_shortcut_icon_url = URL(*url);
	else
		m_shortcut_icon_url = URL();

#ifndef ASYNC_IMAGE_DECODERS
	if (shortcut_icon_provider)
	{
//		shortcut_icon_provider->GetImage().DecVisible(null_image_listener);
		shortcut_icon_provider->DecRef(NULL);
		shortcut_icon_provider = NULL;
	}
#else // !ASYNC_IMAGE_DECODERS
	if (shortcut_icon_provider && (shortcut_icon_provider->GetImage().ImageDecoded() || !url))
	{
		shortcut_icon_provider->GetImage().DecVisible(this);
		shortcut_icon_provider->DecRef(NULL);
		shortcut_icon_provider = NULL;
	}
#endif // ASYNC_IMAGE_DECODERS
	if (url)
	{
		shortcut_icon_provider = UrlImageContentProvider::FindImageContentProvider(*url);
		if (shortcut_icon_provider == NULL)
		{
			shortcut_icon_provider = OP_NEW(UrlImageContentProvider, (*url));
			if (shortcut_icon_provider == NULL)
			{
#ifdef SVG_SUPPORT
				if (ref != NULL)
					OpStatus::Ignore(ref->Free());
#endif // SVG_SUPPORT
				return OpStatus::ERR_NO_MEMORY;
			}

			// Changing contenttype to an image - if it seems like it is a favicon
			if(!url->GetAttribute(URL::KIsImage))
			{
				OpString ext;

				OP_STATUS ret_val = url->GetAttribute(URL::KSuggestedFileNameExtension_L, ext);

				if (OpStatus::IsSuccess(ret_val))
				{
					const char* data;
					INT32 data_len;
					BOOL more;
					ret_val = shortcut_icon_provider->GetData(data, data_len, more);

					// If it contains the magic sequence (copied from modules/img/src/decoderfactoryico.cpp
					// method DecoderFactoryIco::CheckType) and if urls suggested extension is ico :
					// Force content type to be image/x-icon
					// This will result in it being accepted as an image by the image module. [psmaas 27-01-2006]
					if (OpStatus::IsSuccess(ret_val) && data_len >= 4 && op_memcmp(data, "\0\0\1\0", 4) == 0 && ext.Compare("ico") == 0)
					{
						ret_val = url->SetAttribute(URL::KMIME_ForceContentType, "image/x-icon");
					}
				}

				if(OpStatus::IsMemoryError(ret_val))
				{
#ifdef SVG_SUPPORT
					if (ref != NULL)
						OpStatus::Ignore(ref->Free());
#endif // SVG_SUPPORT
					OP_DELETE(shortcut_icon_provider);
					shortcut_icon_provider = NULL;
					return OpStatus::ERR_NO_MEMORY;
				}
			}
		}

#ifdef SVG_SUPPORT
		if (ref && url->ContentType() == URL_SVG_CONTENT)
		{
			int desired_width = 16; // FIXME: Is 16x16 a good default?
			int desired_height = 16;

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
			const PixelScaler& scaler = vis_device->GetVPScale();
			desired_width = TO_DEVICE_PIXEL(scaler, desired_width);
			desired_height = TO_DEVICE_PIXEL(scaler, desired_height);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

			SVGImage* svgimage = ref->GetSVGImage();
			if (!svgimage)
			{
#ifdef SVG_SUPPORT
				OpStatus::Ignore(ref->Free());
#endif // SVG_SUPPORT
				OP_DELETE(shortcut_icon_provider);
				shortcut_icon_provider = NULL;
				return OpStatus::OK;
			}

			Image img = shortcut_icon_provider->GetImage();

			if (img.IsFailed() ||
				img.Width() != (unsigned int) desired_width ||
				img.Height() != (unsigned int) desired_height)
			{
				OpBitmap* res;

				if (OpStatus::IsSuccess(svgimage->PaintToBuffer(res, 0, desired_width, desired_height)))
				{
					if(OpStatus::IsError(img.ReplaceBitmap(res)))
					{
						OP_DELETE(res);
					}
					else
					{
						shortcut_icon_provider->SetSVGImageRef(ref);
						ref = NULL;
					}
				}
			}
			if (ref)
			{
				OpStatus::Ignore(ref->Free());
				OP_DELETE(shortcut_icon_provider);
				shortcut_icon_provider = NULL;
				return OpStatus::OK;
			}
		}
#endif // SVG_SUPPORT

		shortcut_icon_provider->IncRef(NULL);

#ifdef ASYNC_IMAGE_DECODERS
		shortcut_icon_provider->GetImage().IncVisible(this);
#endif
	}
	Image icon;

	if (shortcut_icon_provider)
		icon = shortcut_icon_provider->GetImage();

	// to force loading
#ifndef ASYNC_IMAGE_DECODERS
	RETURN_IF_ERROR(icon.IncVisible(null_image_listener));

	if (shortcut_icon_provider)
		icon.OnMoreData(shortcut_icon_provider);
#endif // ASYNC_IMAGE_DECODERS

	// We need this call when running ASYNC_IMAGE_DECODERS as well in order
	// to get notified about it when doing a reload..
	m_windowCommander->GetDocumentListener()->OnDocumentIconAdded(m_windowCommander);

#ifndef ASYNC_IMAGE_DECODERS
	icon.DecVisible(null_image_listener);
#endif

	return OpStatus::OK;
}

Image Window::GetWindowIcon() const
{
	if (shortcut_icon_provider)
		return shortcut_icon_provider->GetImage(); // CAN BE DANGEROUS IF KEEPT WHILE shortcut_icon_provider is deleted.
	return Image();
}
BOOL Window::HasWindowIcon() const
{
	if (shortcut_icon_provider)
		return !shortcut_icon_provider->GetImage().IsEmpty();
	return FALSE;
}

OP_STATUS Window::DecodeWindowIcon()
{
	if (!HasWindowIcon() || shortcut_icon_provider->GetImage().ImageDecoded())
		return OpStatus::OK;

	Image icon = shortcut_icon_provider->GetImage();

	RETURN_IF_ERROR(icon.IncVisible(null_image_listener));
	icon.OnMoreData(shortcut_icon_provider);
	icon.DecVisible(null_image_listener);

	return OpStatus::OK;
}

#endif // SHORTCUT_ICON_SUPPORT

#ifdef HISTORY_SUPPORT
OP_STATUS Window::AddToGlobalHistory(URL& url, const uni_char *title)
{
	if (g_globalHistory && !m_privacy_mode && m_global_history_enabled && g_url_api->GlobalHistoryPermitted(url) )
		if (HasFeature(WIN_FEATURE_NAVIGABLE))
		{
			OpString url_string;
			RETURN_IF_ERROR(url.GetAttribute(URL::KUniName_With_Fragment_Username, url_string));
			return g_globalHistory->Add(url_string, title, g_timecache->CurrentTime());
		}

	return OpStatus::OK;
}
#endif // HISTORY_SUPPORT

/* Limited length of window titles. Apparently there's a Internet
   Explorer extension that can't handle long window titles.  This
   patch works around the problem, and at the same time looks nice. */

OP_STATUS Window::SetWindowTitle(const OpString& nstr, BOOL force, BOOL generated)
{
	generated_title = generated;
	const uni_char *str = nstr.CStr();
	if (!str)
		str = UNI_L("...");

	BOOL titleChanged = title.IsEmpty() || !uni_str_eq(str, title.CStr());

	// Reset defaultStatus only when title changes and force is not set
	if(titleChanged && !force)
	{
		RETURN_IF_ERROR(SetDefaultMessage(UNI_L("")));
	}

	// Only set window title if it changed or we are forced to do an update
	if (titleChanged || force)
	{
		if (titleChanged)
			RETURN_IF_ERROR(title.Set(str));

		int strLen = uni_strlen(str) + 1;
		uni_char *newTitle = OP_NEWA(uni_char, strLen);
		if (newTitle == NULL)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		uni_strcpy(newTitle, str);

		if (strLen > DOC_WINDOW_MAX_TITLE_LENGTH)
			uni_strcpy(newTitle + DOC_WINDOW_MAX_TITLE_LENGTH - 4, UNI_L("..."));

		m_windowCommander->GetDocumentListener()->OnTitleChanged(m_windowCommander, newTitle);


#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
		OpScopeWindowListener::OnWindowTitleChanged(this);
#endif // SCOPE_WINDOW_MANAGER_SUPPORT

		OP_DELETEA(newTitle);
	}
	return OpStatus::OK;
}

void Window::SignalSecurityState(BOOL changed)
{
	switch (doc_manager->GetLoadStatus())
	{
	case NOT_LOADING:
	case DOC_CREATED:
	case WAIT_MULTIPART_RELOAD:
		break;

	default:
		/* Don't signal new security state to UI while loading URL until we've
		   created a new document.  If we do, we'll be reporting a security
		   state that doesn't match the currently displayed document in the
		   window, which could possibly be the cause of spoofing bugs.

		   One exception though: if we for some reason have already reported a
		   specific security state and are now about to lower it we don't want
		   to delay the lowering. */
		if (!(SecurityState < SecurityState_signalled && SecurityState_signalled != SECURITY_STATE_UNKNOWN || SecurityState == SECURITY_STATE_UNKNOWN))
		{
			if (changed)
				SecurityState_signal_delayed = TRUE;
			return;
		}
	}

	if (changed || SecurityState_signal_delayed)
	{
		OpDocumentListener::SecurityMode mode = m_windowCommander->GetSecurityModeFromState(SecurityState);

		m_windowCommander->GetDocumentListener()->OnSecurityModeChanged(m_windowCommander, mode, SecurityState_changed_by_inline);

		SecurityState_signalled = SecurityState;
		SecurityState_signal_delayed = FALSE;
	}
}

void Window::ResetSecurityStateToUnknown()
{
	LowLevelSetSecurityState(SECURITY_STATE_UNKNOWN, FALSE, NULL);
}

void Window::SetSecurityState(BYTE sec, BOOL is_inline, const uni_char *name, URL *url)
{
	if (sec != SECURITY_STATE_UNKNOWN)
		LowLevelSetSecurityState(sec, is_inline, name, url);
}

void Window::LowLevelSetSecurityState(BYTE sec, BOOL is_inline, const uni_char *name, URL *url)
{
	if (sec < SecurityState || sec == SECURITY_STATE_UNKNOWN)
	{
		BOOL state_changed = FALSE;

		if (SecurityState != sec)
		{
			// Before core-2.3 we didn't let the UI know about changes between UNKNOWN and NONE
			// but as always it turned out to be better to tell the whole truth.
			state_changed = TRUE;

#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
			if(SecurityState >= SECURITY_STATE_SOME_EXTENDED &&
				SecurityState <= SECURITY_STATE_FULL_EXTENDED &&
				sec >= SECURITY_STATE_FULL && sec <SECURITY_STATE_FULL_EXTENDED &&
				!(BOOL) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::StrictEVMode))
			{
				if (SecurityState == SECURITY_STATE_SOME_EXTENDED && SecurityTextName.Compare(name) == 0)
					return;
				else
					sec = SECURITY_STATE_SOME_EXTENDED;
			}
#endif // SSL_CHECK_EXT_VALIDATION_POLICY
		}

		SecurityState = sec;
		if (SecurityTextName.Set(sec && sec != SECURITY_STATE_UNKNOWN ? name : NULL) == OpStatus::ERR_NO_MEMORY)
			RaiseCondition(OpStatus::ERR_NO_MEMORY);

		if (state_changed)
		{
			SecurityState_changed_by_inline = is_inline;
			SignalSecurityState(TRUE);
		}
	}
}

#ifdef TRUST_RATING
void Window::SetTrustRating(TrustRating rating, BOOL force)
{
	OP_ASSERT(rating != Untrusted_Ask_Advisory);
	if (force || m_trust_rating < rating)
	{
		m_trust_rating = rating;
		m_windowCommander->GetDocumentListener()->OnTrustRatingChanged(m_windowCommander, rating);
	}
}

OP_STATUS Window::CheckTrustRating(BOOL force, BOOL offline)
{
	DocumentTreeIterator iter(doc_manager);
	iter.SetIncludeThis();

	while (iter.Next())
	{
		if (FramesDocument* doc = iter.GetFramesDocument())
			RETURN_IF_MEMORY_ERROR(iter.GetDocumentManager()->CheckTrustRating(doc->GetURL(), offline, force));
	}

	return OpStatus::OK;
}

#endif // TRUST_RATING

void Window::SetType(DocType doc_type)
{
	/* Not entirely sure this function serves much of a purpose anymore. */

    if (type != WIN_TYPE_DOWNLOAD && IsCustomWindow())
        return;
	if (type == WIN_TYPE_JS_CONSOLE ) // console is a custom window (maybe it should not be that)
		return;

    SetType(WIN_TYPE_NORMAL);
}

BOOL Window::CloneWindow(Window**return_window,BOOL open_in_background)
{
	if (IsCustomWindow())
		return FALSE;

    BOOL3 open_in_new_window = YES;
    BOOL3 in_background = open_in_background?YES:NO;
	Window* new_window = g_windowManager->GetAWindow(TRUE, open_in_new_window, in_background);
	if (!new_window)
		return FALSE;

	new_window->SetCSSMode(CSSMode);
    new_window->SetScale(GetScale());
	new_window->SetTextScale(GetTextScale());
    new_window->SetForceEncoding(GetForceEncoding(), TRUE);

	if (load_img && show_img)
	{
		new_window->SetImagesSetting(FIGS_ON);
	}
	else if (show_img)
	{
		new_window->SetImagesSetting(FIGS_SHOW);
	}
	else
	{
		new_window->SetImagesSetting(FIGS_OFF);
	}

#ifdef FIT_LARGE_IMAGES_TO_WINDOW_WIDTH
	new_window->fit_img = fit_img;
#endif // FIT_LARGE_IMAGES_TO_WINDOW_WIDTH

	DocumentManager* doc_man1 = DocManager();
	DocumentManager* doc_man2 = new_window->DocManager();

	short pos = GetCurrentHistoryNumber();
	DocListElm*	doc_elm = doc_man1->FirstDocListElm();
	while (doc_elm)
	{
		FramesDocument* doc = doc_elm->Doc();
		if (doc)
		{
			URL docurl = doc->GetURL();
			DocumentReferrer referrer(doc->GetRefURL());
			RAISE_IF_MEMORY_ERROR(doc_man2->AddNewHistoryPosition(docurl, referrer, doc_elm->Number(), doc_elm->Title(), FALSE, doc->IsPluginDoc()));
		}

		doc_elm = doc_elm->Suc();
	}

	new_window->min_history_number = min_history_number;
	new_window->max_history_number = max_history_number;
	new_window->current_history_number = pos;
	new_window->SetCurrentHistoryPos(pos, TRUE, TRUE);
	if(return_window)
		*return_window=new_window;

	return FALSE;
}

// Returns NULL on OOM
uni_char* Window::ComposeLinkInformation(const uni_char* hlink, const uni_char* rel_name)
{
	const uni_char *empty = UNI_L("");
	uni_char* link = NULL;

	if (!hlink)
		hlink = empty;

	// FIXME(?): The non-configurable DisplayRelativeLink() has been removed
	if (rel_name)
	{
		unsigned int len = uni_strlen(hlink) + uni_strlen(rel_name) + 2;
		link = OP_NEWA(uni_char, len);
		if (link)
			uni_snprintf(link, len, UNI_L("%s#%s"), hlink, rel_name);
	}
	else
		OpStatus::Ignore(UniSetStr_NotEmpty(link, hlink)); // link remains NULL if it fails

	return link;
}

OP_STATUS Window::SetProgressText(URL& url, BOOL unused /* = FALSE */)
{
	// In case we have a URL that is redirecting to another page, we have to call Access to mark it visited.
	// Otherwise the original link won't be marked as visited (since the redirecting url isn't a document)
	if (!GetPrivacyMode())
		url.Access(TRUE);
	return OpStatus::OK;
}

BOOL Window::HandleMessage(UINT message, int id, long user_data)
{
	return FALSE;
}

OP_STATUS Window::OpenURL(const char* in_url_str, BOOL check_if_expired, BOOL user_initiated, BOOL reload, BOOL is_hotlist_url, BOOL open_in_background, EnteredByUser entered_by_user, BOOL called_externally)
{
	OpString16 tmp_str;
	RETURN_IF_ERROR(tmp_str.SetFromUTF8(in_url_str));
	return OpenURL(tmp_str.CStr(), check_if_expired, user_initiated, reload, is_hotlist_url,open_in_background,entered_by_user, called_externally);
}

OP_STATUS Window::OpenURL(const uni_char* in_url_str, BOOL check_if_expired, BOOL user_initiated, BOOL reload, BOOL is_hotlist_url, BOOL open_in_background, EnteredByUser entered_by_user, BOOL called_externally, URL_CONTEXT_ID context_id)
{
	if (!in_url_str)
		return OpStatus::OK;

	OpString search_url;

#ifdef AB_ERROR_SEARCH_FORM
	//The following text will be used to present back to the user
	//in the search field in error pages. More than a few hundred
	//chars is already information overload
	if(entered_by_user == WasEnteredByUser)
		OpStatus::Ignore(m_typed_in_url.Set(in_url_str, 200));
#endif // AB_ERROR_SEARCH_FORM

#ifdef SEARCH_ENGINES
	if (uni_isalpha(in_url_str[0]) && uni_isspace(in_url_str[1]) && in_url_str[2])
	{
		OpString key;
		key.Set(in_url_str, 1);

		SearchElement* search_element = g_search_manager->GetSearchByKey(key);
		if (search_element)
		{
			OpString search_query;
			OpString post_query;
			search_query.Set(in_url_str+2);
			if (search_element->MakeSearchString(search_url, post_query, search_query) != OpStatus::OK)
				search_url.Empty();
		}
	}
#endif // SEARCH_ENGINES

	const uni_char* new_in_url_str=in_url_str;
	while (uni_isspace(*new_in_url_str))
		new_in_url_str++;

	OpString acpUrlStr;

#ifdef SLASHDOT_EASTEREGG
	if (uni_strcmp(new_in_url_str, "/.") == 0)
	{
		RETURN_IF_ERROR(acpUrlStr.Set(UNI_L("http://slashdot.org/")));
	}
	else
#endif // SLASHDOT_EASTEREGG
	{
		if (search_url.HasContent())
		{
			RETURN_IF_ERROR(acpUrlStr.Set(search_url));
		}
		else
			RETURN_IF_ERROR(acpUrlStr.Set(new_in_url_str));
	}

#ifdef CORE_BOOKMARKS_SUPPORT
	//	Nickname ?

	if (!uni_strpbrk((const OpChar *) acpUrlStr.CStr(), UNI_L(".:/?\\")))
	{
		BookmarkAttribute attr;
		RETURN_IF_ERROR(attr.SetTextValue(acpUrlStr.CStr()));
		BookmarkItem *bookmark = g_bookmark_manager->GetFirstByAttribute(BOOKMARK_SHORTNAME, &attr);

		if (bookmark && bookmark->GetFolderType() == FOLDER_NO_FOLDER)
		{
			RETURN_IF_ERROR(bookmark->GetAttribute(BOOKMARK_URL, &attr));
			RETURN_IF_ERROR(attr.GetTextValue(acpUrlStr));
		}
	}
#endif // CORE_BOOKMARKS_SUPPORT

	OpString acpUrlNameResolved;

	OP_STATUS r;
	BOOL res = FALSE;
	TRAP(r,res=g_url_api->ResolveUrlNameL(acpUrlStr, acpUrlNameResolved, entered_by_user == WasEnteredByUser));
	RETURN_IF_ERROR(r);
	if (!res)
		return OpStatus::OK;

	if (acpUrlNameResolved.CStr() == NULL)
		return OpStatus::OK;

	URL url;

#ifdef AB_ERROR_SEARCH_FORM
	//set for OpIllegalHost page to have access to the raw text
	g_windowManager->SetLastTypedUserText(&m_typed_in_url);
#endif // AB_ERROR_SEARCH_FORM

	URL_CONTEXT_ID id = 0;
# ifdef GADGET_SUPPORT
	/* Checking for the gadget context id first was seemingly done on purpose here, so i left it
	 * like that when i moved the search for a suitable context id to GetUrlContextId(), if the
	 * order is not really important the GADGET_SUPPORT section here can be safely removed
	 * since this case is covered in GetUrlContextId() */
	if (GetGadget())
		id = GetGadget()->UrlContextId();
	else
# endif // GADGET_SUPPORT
		id = (context_id != static_cast<URL_CONTEXT_ID>(-1)) ? context_id : GetUrlContextId(acpUrlNameResolved.CStr());

	url = g_url_api->GetURL(acpUrlNameResolved.CStr(), id);

#ifdef SUPPORT_VISUAL_ADBLOCK
	// cache the server name so it's faster to do a lookup to check if the content blocker is enabled or not
	m_content_block_server_name = url.GetServerName();
	if(m_content_block_server_name)
		m_content_block_enabled = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableContentBlocker, m_content_block_server_name) != 0;
#endif // SUPPORT_VISUAL_ADBLOCK

	if (!url.IsEmpty())
	{
		URL dummy;

		if(is_hotlist_url && (url.Type() == URL_HTTP || url.Type() == URL_HTTPS))
		{
			OpStringC8 name = url.GetAttribute(URL::KUserName).CStr();
			if(name.CStr() == NULL && url.GetAttribute(URL::KPassWord).CStr() != NULL)
				name = "";

			ServerName *sn = (ServerName *) url.GetAttribute(URL::KServerName, NULL);

			if(name.CStr() != NULL && !sn->GetPassUserNameURLsAutomatically(name))
			{
				RETURN_IF_ERROR(sn->SetPassUserNameURLsAutomatically(name));
			}
		}

#ifdef REDIRECT_ON_ERROR
		//This partially resolved url will be used to redirect the user directly to a search engine
		//See PrefsCollectionNetwork::HostNameWebLookupAddress
		//The typed url value being set above in this same function, if any,
		//was used in the call to g_url_api->GetURL(acpUrlNameResolved.CStr());
		//which could have generated an OpIllegalHostPage in case the url was malformed.
		//This would present the url in a clearly visible text field to the user
		//Now the url is sabed again, this time to be used for the automatic redirect.
		//The new url needs to be stored because any username and password info have been removed
		//and we cannot automatically upload that information to a 3rd party server
		if(entered_by_user == WasEnteredByUser &&
			g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableHostNameWebLookup))
			m_typed_in_url.Set(in_url_str);
#endif // REDIRECT_ON_ERROR

		SetFlexRootMaxWidth(g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::FlexRootMaxWidth, url));

		r = OpenURL(url, DocumentReferrer(dummy), check_if_expired, reload, -1, user_initiated, open_in_background, entered_by_user, called_externally);
	}
#ifdef AB_ERROR_SEARCH_FORM
	g_windowManager->SetLastTypedUserText(NULL);
#endif // AB_ERROR_SEARCH_FORM

	return r;
}

OP_STATUS Window::OpenURL
(
    OpenUrlInNewWindowInfo *info,
	BOOL check_if_expired,
	BOOL reload,
	EnteredByUser entered_by_user,
	BOOL called_externally
 )
{
    return OpenURL(info->new_url,
                   info->ref_url,
                   check_if_expired,
                   reload,
                   info->opener_sub_win_id,
                   info->user_initiated,
#if defined(QUICK)
				   KioskManager::GetInstance()->GetEnabled() && !KioskManager::GetInstance()->GetKioskWindows() ? FALSE :
#endif
	               info->open_in_background_window,

				   entered_by_user,
				   called_externally);
}

OP_STATUS Window::OpenURL(URL& url, DocumentReferrer ref_url, BOOL check_if_expired, BOOL reload, short sub_win_id,
                          BOOL user_initiated, BOOL open_in_background, EnteredByUser entered_by_user, BOOL called_externally)
{
	DocumentManager* doc_man = GetDocManagerById(sub_win_id);

	if (doc_man)
	{
		doc_man->ResetRequestThreadInfo();
#if defined REDIRECT_ON_ERROR || defined AB_ERROR_SEARCH_FORM
		if(entered_by_user != WasEnteredByUser)
		{
			//it's better to wipe from memory because one never knows if
			//some bug could criple in and store an url with username+password here
			m_typed_in_url.Wipe();
			m_typed_in_url.Empty();
		}
#endif //  defined REDIRECT_ON_ERROR || defined AB_ERROR_SEARCH_FORM

#ifdef _PRINT_SUPPORT_
		if (print_mode)
			StopPrinting();
#endif // _PRINT_SUPPORT_

		DocumentManager::OpenURLOptions options;
		options.user_initiated = user_initiated;
		options.entered_by_user = entered_by_user;
		options.called_externally = called_externally;
		doc_man->OpenURL(url, DocumentReferrer(ref_url), check_if_expired, reload, options);
	}

	return OpStatus::OK;
}

void Window::HandleDocumentProgress(long bytes_read)
{
	URL url = doc_manager->GetCurrentURL();

	SetDocProgressCount(url.ContentLoaded());

	m_loading_information.totalBytes = url.GetContentSize();
	m_loading_information.loadedBytes = GetProgressCount();
	m_loading_information.totalBytesRootDocument = m_loading_information.totalBytes;
	m_loading_information.loadedBytesRootDocument = GetDocProgressCount();

	FramesDocument* doc = GetCurrentDoc();
	if (doc)
	{
		if (doc_manager->GetLoadStatus() == NOT_LOADING ||
			doc_manager->GetLoadStatus() == DOC_CREATED)
		{
			DocLoadingInfo info;
			doc->GetDocLoadingInfo(info);
			m_loading_information.loadedImageCount = info.images.loaded_count;
			m_loading_information.totalImageCount = info.images.total_count;
			m_loading_information.loadedBytes = info.loaded_size;
			m_loading_information.totalBytes = info.total_size;
#ifdef WEB_TURBO_MODE
			m_loading_information.turboTransferredBytes = info.turbo_transferred_bytes + info.images.turbo_transferred_bytes;
			m_loading_information.turboOriginalTransferredBytes = info.turbo_original_transferred_bytes + info.images.turbo_original_transferred_bytes;
#endif // WEB_TURBO_MODE
		}
	}

	m_loading_information.loadingMessage = GetProgressMessage();
	m_loading_information.isUploading = FALSE;
	m_windowCommander->GetLoadingListener()->OnLoadingProgress(m_windowCommander, &m_loading_information);
}

void Window::HandleDataProgress(long bytes, BOOL upload_progress, const uni_char* extrainfo, MessageHandler *mh, URL *url)
{
	// Since the same counter is used for both uploading and downloading we need to reset it
	// when the direction changes
	if (GetProgressState() == UPLOADING_FINISHED && !upload_progress)
		SetProgressCount(0);

	SetProgressCount(progress_count+bytes);

	m_loading_information.loadedBytes = progress_count;
	m_loading_information.isUploading = upload_progress;
	// We need this to avoid calling OnLoadingProgress() before OnStartUploading().
	// Both OnStartUploading() and OnLoadingProgress() will be called in SetProgressState().
	if ( !upload_progress || phase_uploading )
		m_windowCommander->GetLoadingListener()->OnLoadingProgress(m_windowCommander, &m_loading_information);

	SetProgressState(upload_progress ? UPLOADING_FILES : FETCHING_DOCUMENT, extrainfo, mh, bytes, url);
}

void Window::UpdateLoadingInformation(int loaded_inlines, int total_inlines)
{
	m_loading_information.loadedImageCount = loaded_inlines;
	m_loading_information.totalImageCount = total_inlines;
	m_loading_information.loadedBytesRootDocument = GetProgressCount();
	m_loading_information.totalBytesRootDocument = DocManager()->GetCurrentURL().GetContentSize();

	FramesDocument* doc = GetCurrentDoc();
	if (doc && (doc_manager->GetLoadStatus() == NOT_LOADING || doc_manager->GetLoadStatus() == DOC_CREATED))
	{
		DocLoadingInfo info;
		doc->GetDocLoadingInfo(info);
		m_loading_information.loadedImageCount = info.images.loaded_count;
		m_loading_information.totalImageCount = info.images.total_count;
		m_loading_information.loadedBytes = info.loaded_size;
		m_loading_information.totalBytes = info.total_size;
#ifdef WEB_TURBO_MODE
		m_loading_information.turboTransferredBytes = info.turbo_transferred_bytes + info.images.turbo_transferred_bytes;
		m_loading_information.turboOriginalTransferredBytes = info.turbo_original_transferred_bytes + info.images.turbo_original_transferred_bytes;
#endif // WEB_TURBO_MODE
	}
	m_loading_information.isUploading = FALSE;

	m_windowCommander->GetLoadingListener()->OnLoadingProgress(m_windowCommander, &m_loading_information);
}

void Window::LoadPendingUrl(URL_ID url_id, int sub_win_id, BOOL user_initiated)
{
	if (sub_win_id == -2)
	{
	// "LoadPendingURL with sub_win_id == -2 *cannot* work"
        BOOL3 open_in_new_window = MAYBE;
        BOOL3 open_in_background = MAYBE;
		g_windowManager->GetAWindow(TRUE, open_in_new_window, open_in_background)->LoadPendingUrl(url_id, -1, user_initiated);
		return;
	}

	DocumentManager* doc_man = GetDocManagerById(sub_win_id);

	if (doc_man)
		doc_man->LoadPendingUrl(url_id, user_initiated);
}

BOOL Window::HandleLoadingFailed(Str::LocaleString msg_id, URL url)
{
	OpString url_name;

	if (OpStatus::IsMemoryError(url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, url_name, FALSE)))
		RaiseCondition(OpStatus::ERR_NO_MEMORY);
	else if (m_windowCommander->GetLoadingListener()->OnLoadingFailed(m_windowCommander, msg_id, url_name.CStr()))
		/* Event was handled by the listener. */
		return TRUE;

	return FALSE;
}

#ifndef NO_EXTERNAL_APPLICATIONS
BOOL Window::RunApplication(const OpStringC& application,const OpStringC& filename)
{
	RETURN_VALUE_IF_ERROR(Execute(application.CStr(), filename.CStr()), FALSE);
	return TRUE;
}
#endif // !NO_EXTERNAL_APPLICATIONS

#ifndef NO_EXTERNAL_APPLICATIONS
BOOL Window::ShellExecute(const uni_char* filename)
{
	if (filename && *filename)
		g_op_system_info->ExecuteApplication(filename, NULL);

	return FALSE;
}
#endif // !NO_EXTERNAL_APPLICATIONS

////////////////////////////////////////////////////
void Window::AskAboutUnknownDoc(URL& url, int sub_win_id)
{
#ifndef WIC_USE_DOWNLOAD_CALLBACK
#ifdef USE_DOWNLOAD_API

	TRAPD(err, g_DownloadManager->DelegateL(url, m_windowCommander));
	if( OpStatus::IsMemoryError(err) )
		RaiseCondition(OpStatus::ERR_NO_MEMORY);

#elif defined(DOCHAND_USE_ONDOWNLOADREQUEST)
	OpString suggested_filename;
	OpString8 mimetype_str;

	if (OpStatus::IsMemoryError(url.GetAttribute(URL::KSuggestedFileName_L, suggested_filename)) ||
	    OpStatus::IsMemoryError(url.GetAttribute(URL::KMIME_Type, mimetype_str, TRUE)) )
	{
		RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return;
	}

	char *mimetype = mimetype_str.CStr();
	OpFileLength size = url.GetContentSize();
	m_windowCommander->GetDocumentListener()->OnDownloadRequest(m_windowCommander, url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped).CStr(), suggested_filename.CStr(),
	                                                            mimetype, size, OpDocumentListener::UNKNOWN);
#elif defined(QUICK)
	BOOL no_download_dialog = KioskManager::GetInstance()->GetNoDownload();

	if(no_download_dialog || IsThumbnailWindow())
	{
		url.StopLoading(NULL);
		url.Unload();
		return;
	}

	DownloadDialog* dialog = OP_NEW(DownloadDialog, (&url, this));
#ifdef QUICK
	dialog->Init(g_application->GetDesktopWindowFromWindow(this), sub_win_id);
#else // QUICK
	dialog->Init(g_application->GetDesktopWindowFromWindow(this));
#endif // QUICK

#elif defined(LIBOPERA)
	OpString suggested_filename_str;
	OpString8 mimetype_str;
	if (OpStatus::IsMemoryError(url.GetAttribute(URL::KSuggestedFileName_L, suggested_filename_str, TRUE)) || OpStatus::IsMemoryError(mimetype_str.Set(url.GetAttribute(URL::KMIME_Type))))
	{
		RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return;
	}
	uni_char *suggested_filename = suggested_filename_str.CStr();
	char *mimetype = mimetype_str.CStr();
	OpFileLength size = url.GetContentSize();

#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
	url.PauseLoading();
#endif // TCP_PAUSE_DOWNLOAD_EXTENSION

#if !defined GTK_DOWNLOAD_EXTENSION && !defined TCP_PAUSE_DOWNLOAD_EXTENSION
	url.StopLoading(NULL);
	url.Unload();
#endif // !GTK_DOWNLOAD_EXTENSION && !TCP_PAUSE_DOWNLOAD_EXTENSION

# ifdef _OPL_
// OPL: IMPLEMENTME
# elif defined(SDK) || defined(POWERPARTS)
#  ifdef GTK_DOWNLOAD_EXTENSION
	url.PauseLoading();
	OP_STATUS status = GetWindowCommander()->InitiateTransfer(doc_manager, url, OpTransferItem::ACTION_SAVE, NULL);
	if (OpStatus::IsError(status))
	{
		if (status == OpStatus::ERR_NO_MEMORY)
		{
			RaiseCondition(status);
		}
		return;
	}
#  endif // GTK_DOWNLOAD_EXTENSION
# else  // _OPL_
	OperaEmbeddedInternal::instance->signalDownloadFile(url.GetUniName(FALSE, PASSWORD_HIDDEN), suggested_filename,
														mimetype, size);
# endif // _OPL_
#endif // misc platforms
#endif // !WIC_USE_DOWNLOAD_CALLBACK
}

int	Window::SetNewHistoryNumber()
{
	doc_manager->RemoveFromHistory(current_history_number + 1);
	current_history_number++;
	max_history_number = current_history_number;

	if (!history_cleanup_message_sent)
	{
		msg_handler->PostMessage(MSG_HISTORY_CLEANUP, 0, 0);
		history_cleanup_message_sent = TRUE;
	}

	return current_history_number;
}

void Window::SetLastHistoryPos(int pos)
{
	if (pos < min_history_number || pos > max_history_number)
		return;

	current_history_number = pos;
	max_history_number = current_history_number;
	doc_manager->RemoveFromHistory(current_history_number + 1);
	FramesDocument* doc = GetCurrentDoc();
	if (doc)
		doc->RemoveFromHistory(current_history_number + 1);
}

void Window::SetCurrentHistoryPos(int pos, BOOL update_doc_man, BOOL is_user_initiated)
{
	if (pos < min_history_number)
		pos = min_history_number;
	else if (pos > max_history_number)
		pos = max_history_number;

	current_history_number = pos;

	if (update_doc_man)
	{
		doc_manager->SetCurrentHistoryPos(pos, FALSE, is_user_initiated);
		UpdateWindowIfLayoutChanged();
	}
}

int Window::GetNextDocListElmId()
{
	return m_next_doclistelm_id++;
}

OP_STATUS Window::SetHistoryUserData(int history_ID, OpHistoryUserData* user_data)
{
	return doc_manager->SetHistoryUserData(history_ID, user_data);
}

OP_STATUS Window::GetHistoryUserData(int history_ID, OpHistoryUserData** user_data) const
{
	return doc_manager->GetHistoryUserData(history_ID, user_data);
}

void Window::SetMaxHistory(int len)
{
	int remove_len = GetHistoryLen() - len - 1;
	if (remove_len > 0)
	{
		if (current_history_number - min_history_number > remove_len)
			min_history_number += remove_len;
		else
			min_history_number = current_history_number;

		doc_manager->RemoveUptoHistory(min_history_number);
	}

	remove_len = GetHistoryLen() - len - 1;
	if (remove_len > 0)
	{
		if (max_history_number - current_history_number > remove_len)
			max_history_number += remove_len;
		else
			max_history_number = current_history_number;

		doc_manager->RemoveFromHistory(max_history_number+1);
	}
}

#ifdef DOCHAND_HISTORY_MANIPULATION_SUPPORT

void Window::RemoveElementFromHistory(int pos)
{
    if((pos < min_history_number) || (pos > max_history_number))
        return;
    if(pos == current_history_number)
    {
        if(current_history_number > min_history_number)
            SetCurrentHistoryPos(current_history_number - 1, TRUE, FALSE);
        else if(current_history_number < max_history_number)
            SetCurrentHistoryPos(current_history_number + 1, TRUE, FALSE);
    }
    if(pos == max_history_number)
    {
		doc_manager->RemoveElementFromHistory(max_history_number--, TRUE); // Unlink from history list
        //doc_manager->RemoveFromHistory(max_history_number--, TRUE); // Unlink from history list
        if(min_history_number > max_history_number)
        {
            min_history_number = 1;
            max_history_number = current_history_number = 0;
        }
    }
    else if(pos == min_history_number)
    {
        doc_manager->RemoveUptoHistory(++min_history_number, TRUE);  // Unlink from history list
    }
    else
        doc_manager->RemoveElementFromHistory(pos);
    CheckHistory();
}

void Window::InsertElementInHistory(int pos, DocListElm * elm)
{
	if (pos < min_history_number)
		pos = min_history_number;
	else if (pos > (max_history_number+1))
		pos = max_history_number+1;

	doc_manager->InsertElementInHistory(pos, elm);

	max_history_number += 1;
	if(pos <= current_history_number)
		SetCurrentHistoryPos(current_history_number + 1, TRUE, FALSE);
	CheckHistory();
}

void Window::RemoveAllElementsFromHistory(BOOL unlink)
{
	int pos = min_history_number;
	while (pos <= max_history_number)
		doc_manager->RemoveElementFromHistory(pos++, unlink);

	CheckHistory();
}

#endif // DOCHAND_HISTORY_MANIPULATION_SUPPORT

void Window::RemoveAllElementsExceptCurrent()
{
	doc_manager->RemoveFromHistory(current_history_number+1);
	doc_manager->RemoveUptoHistory(current_history_number);

	CheckHistory();
}

// Handles OOM locally by setting the flag on the window.
void Window::GetClickedURL(URL url, DocumentReferrer& ref_url, short sub_win_id, BOOL user_initiated, BOOL open_in_background_window)
{
	DocumentManager* doc_man = GetDocManagerById(sub_win_id);
	if (!doc_man)
		return;

	URL current_url = doc_man->GetCurrentURL();

	if (sub_win_id == -2 &&
		(!g_pcdoc->GetIntegerPref(PrefsCollectionDoc::IgnoreTarget) &&
		 !g_pcdoc->GetIntegerPref(PrefsCollectionDoc::SingleWindowBrowsing)))
	{
        BOOL3 open_in_new_window = MAYBE;
        BOOL3 open_in_background = open_in_background_window?YES:MAYBE;
		Window* window = g_windowManager->GetAWindow(TRUE, open_in_new_window, open_in_background);

		if (window)
        {
			if (window->OpenURL(url, ref_url, TRUE, FALSE, -1, user_initiated, open_in_background==YES) == OpStatus::ERR_NO_MEMORY)
				window->RaiseCondition( OpStatus::ERR_NO_MEMORY );
        }
	}
	else
	{
		doc_man->StopLoading(FALSE, FALSE, FALSE);
		if (OpenURL(url, ref_url, !(url==current_url), FALSE, sub_win_id, user_initiated, open_in_background_window) == OpStatus::ERR_NO_MEMORY)
			g_memory_manager->RaiseCondition( OpStatus::ERR_NO_MEMORY );
	}
}

#ifdef LINK_SUPPORT
const LinkElement*
Window::GetLinkList() const
{
	FramesDocument* frames_doc = doc_manager->GetCurrentDoc();
    if (frames_doc)
	{
		HLDocProfile* profile = frames_doc->GetHLDocProfile();
		if (profile)
			return profile->GetLinkList();
	}
	return NULL;
}
const LinkElementDatabase*
Window::GetLinkDatabase() const
{
	if (FramesDocument* frames_doc = doc_manager->GetCurrentDoc())
	{
		if (HLDocProfile* profile = frames_doc->GetHLDocProfile())
			return profile->GetLinkDatabase();
	}
	return NULL;
}
#endif // LINK_SUPPORT

/** Return the CSSCollection object for the current document. */
CSSCollection* Window::GetCSSCollection() const
{
	FramesDocument* frames_doc = doc_manager->GetCurrentVisibleDoc();
    if (frames_doc)
	{
		HLDocProfile* profile = frames_doc->GetHLDocProfile();
		if (profile)
			return profile->GetCSSCollection();
	}
	return NULL;
}

URL Window::GetCurrentShownURL()
{
	return doc_manager->GetCurrentDocURL();
}

URL Window::GetCurrentLoadingURL()
{
	switch (doc_manager->GetLoadStatus())
	{
	case NOT_LOADING:
	case DOC_CREATED:
	case WAIT_MULTIPART_RELOAD:
		return doc_manager->GetCurrentDocURL();

	default:
		URL url = doc_manager->GetCurrentURL();
		URL target = url.GetAttribute(URL::KMovedToURL, TRUE);
		if (!target.IsEmpty())
			return target;
		else
			return url;
	}
}

URL Window::GetCurrentURL()
{
	return doc_manager->GetCurrentDocURL();
}

URL_CONTEXT_ID Window::GetMainUrlContextId(URL_CONTEXT_ID base_id)
{
	// If you edit this, please see Window::GetUrlContextId()

#if defined WEB_TURBO_MODE || defined APPLICATION_CACHE_SUPPORT
	if (base_id != 0 && (
# ifdef WEB_TURBO_MODE
		base_id == g_windowManager->GetTurboModeContextId() ||
# endif // WEB_TURBO_MODE
# ifdef APPLICATION_CACHE_SUPPORT
		g_application_cache_manager->GetCacheFromContextId(base_id) != NULL ||
# endif // APPLICATION_CACHE_SUPPORT
		FALSE))
	{
#ifdef GADGET_SUPPORT
		if (GetGadget())
			return GetGadget()->UrlContextId();
		else
#endif // GADGET_SUPPORT
		if (GetPrivacyMode())
			return g_windowManager->GetPrivacyModeContextId();
		else
			return 0;
	}
	else
#endif // defined WEB_TURBO_MODE || defined APPLICATION_CACHE_SUPPORT
		return base_id;
}

URL_CONTEXT_ID Window::GetUrlContextId(const uni_char *url)
{
	// If you edit this, please see Window::GetMainUrlContextId()

	URL_CONTEXT_ID id = 0;

#ifdef GADGET_SUPPORT
	if (GetGadget())
		id = GetGadget()->UrlContextId();
	else
#endif // GADGET_SUPPORT
	if (GetPrivacyMode())
	{
		id = g_windowManager->GetPrivacyModeContextId();
		OP_ASSERT(id);
	}
#ifdef WEB_TURBO_MODE
	else if (GetTurboMode())
	{
		id = g_windowManager->GetTurboModeContextId();
		OP_ASSERT(id);
	}
#endif // WEB_TURBO_MODE
#ifdef APPLICATION_CACHE_SUPPORT
	else if (url && g_application_cache_manager)
	{
		URL_CONTEXT_ID appcache_context_id = g_application_cache_manager->GetMostAppropriateCache(url);
		if (appcache_context_id)
			id = appcache_context_id;
	}
#endif // APPLICATION_CACHE_SUPPORT

	return id;
}

DocumentManager* Window::GetDocManagerById(int sub_win_id)
{
	if (sub_win_id < 0)
		return doc_manager;
	else
		return doc_manager->GetDocManagerById(sub_win_id);
}

void Window::CheckHistory()
{
	check_history = FALSE;
	int minhist = current_history_number;
	int maxhist = current_history_number;
	doc_manager->CheckHistory(0, minhist, maxhist);
	if (minhist > min_history_number)
		min_history_number = minhist;
	if (maxhist < max_history_number)
		max_history_number = maxhist;
}

void Window::UpdateWindowIfLayoutChanged()
{
	// The session handling in desktop can change
	// the history position on an empty doc_manager. Don't ask.
	if (!doc_manager->GetCurrentDoc())
		return;

	LayoutMode doc_layout_mode = doc_manager->GetCurrentDoc()->GetLayoutMode();
	LayoutMode& win_layout_mode = layout_mode;
	BOOL doc_era_mode = doc_manager->GetCurrentDoc()->GetERA_Mode();
	BOOL& win_era_mode = era_mode;
	if (!win_era_mode && (doc_layout_mode != win_layout_mode ||
	                      doc_manager->GetCurrentDoc()->GetMediaHandheldResponded()) ||
		win_era_mode && !doc_era_mode)
	{
		// the ssr status of window and doc were not in sync
		UpdateWindow();
	}
}

void Window::UnlockPaintLock()
{
	DocumentTreeIterator iterator(this);
	iterator.SetIncludeThis();
	while (iterator.Next())
		iterator.GetDocumentManager()->GetVisualDevice()->StopLoading();
}

void Window::SetHistoryNext(BOOL /* unused = FALSE */)
{
	if (HasFeature(WIN_FEATURE_NAVIGABLE))
	{
		// Assuming user initiated (or we'll have to tweak this code). Allow a new download dialog after the user has moved in history
		SetHasShownUnsolicitedDownloadDialog(FALSE);
		if (current_history_number < max_history_number)
			doc_manager->SetCurrentHistoryPos(++current_history_number, FALSE, TRUE);

		UpdateWindowIfLayoutChanged();
	}
}

void Window::SetHistoryPrev(BOOL /* unused = FALSE */)
{
	if (HasFeature(WIN_FEATURE_NAVIGABLE))
	{
		if (current_history_number > min_history_number)
		{
			// Assuming user initiated (or we'll have to tweak this code). Allow a new download dialog after the user has moved in history
			SetHasShownUnsolicitedDownloadDialog(FALSE);

			FramesDocument* doc = GetCurrentDoc();
			URL url;
			if (doc)
			{
				url = GetCurrentDoc()->GetURL();
			}
			doc_manager->SetCurrentHistoryPos(--current_history_number, FALSE, TRUE);

			if (doc)
			{
				UpdateVisitedLinks(url);
				UpdateWindowIfLayoutChanged();
			}
		}
	}
}

BOOL Window::IsLoading() const
{
	return loading && !doc_manager->IsCurrentDocLoaded();
}

void Window::DisplayLinkInformation(const URL &link_url, ST_MESSAGETYPE msType/* = ST_ASTRING*/, const uni_char *title/* = NULL*/)
{
	OpString link_str;
	OpStatus::Ignore(link_url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_Hidden_Escaped, link_str));

	DisplayLinkInformation(link_str.CStr(), msType, title);
}

void Window::DisplayLinkInformation(const uni_char* link, ST_MESSAGETYPE mt/*=ST_ASTRING*/, const uni_char *title/*=NULL*/)
{
	if (!link)
		link = UNI_L("");

	if (*link || (title && *title))
	{
		m_windowCommander->GetDocumentListener()->OnHover(m_windowCommander, link, title, FALSE);
		is_something_hovered = TRUE;
	}
	else
	{
		if (is_something_hovered)
		{
			m_windowCommander->GetDocumentListener()->OnNoHover(m_windowCommander);
			is_something_hovered = FALSE;
		}

		m_windowCommander->GetDocumentListener()->OnStatusText(m_windowCommander, current_message ? current_message : current_default_message);
	}

	g_windowManager->SetPopupMessage(link, FALSE, mt);
}

void Window::DisplayLinkInformation(const uni_char* hlink, const uni_char* rel_name, ST_MESSAGETYPE msType/* = ST_ASTRING*/, const uni_char *title/* = NULL*/)
{
	uni_char* full_tag = Window::ComposeLinkInformation(hlink, rel_name);

	DisplayLinkInformation(full_tag, msType, title);

	OP_DELETEA(full_tag);
}

#ifdef _PRINT_SUPPORT_
OP_STATUS Window::SetFramesPrintType(DM_PrintType fptype, BOOL update)
{
	FramesDocument* doc = GetCurrentDoc();

	if (doc && (!doc->IsFrameDoc() || (fptype == PRINT_ACTIVE_FRAME && !doc->GetActiveDocManager())))
		fptype = PRINT_ALL_FRAMES;

	if (frames_print_type != fptype)
	{
		frames_print_type = fptype;

		if (update)
			return doc_manager->UpdatePrintPreview();
	}

	return OpStatus::OK;
}

BOOL Window::TogglePrintMode(BOOL preview)
{
	PrintDevice* tmpPrintDev = NULL;

	if (preview)
		preview_mode = !preview_mode;
	else
		print_mode = !print_mode;

	if (preview_mode || print_mode)
		ecmascript_paused |= PAUSED_PRINTING;
	else
	{
		ecmascript_paused &= ~PAUSED_PRINTING;
		ResumeEcmaScriptIfNotPaused();
	}

	OP_STATUS result = OpStatus::OK;

	if (!preview_printer_info && !print_mode && preview_mode)
	{
		preview_printer_info = OP_NEW(PrinterInfo, ());

		if (!preview_printer_info)
			result = OpStatus::ERR_NO_MEMORY;
		else
#ifdef PRINT_SELECTION_USING_DUMMY_WINDOW
			result = preview_printer_info->GetPrintDevice(tmpPrintDev, preview, FALSE, this, NULL);	//if this fails, we don't have any printer device to work towards
#else // !PRINT_SELECTION_USING_DUMMY_WINDOW
			result = preview_printer_info->GetPrintDevice(tmpPrintDev, preview, FALSE, this);	//if this fails, we don't have any printer device to work towards
#endif // !PRINT_SELECTION_USING_DUMMY_WINDOW
	}
	else
		if (!preview)
#ifdef PRINT_SELECTION_USING_DUMMY_WINDOW
			result = printer_info->GetPrintDevice(tmpPrintDev, preview, FALSE, this, NULL);
#else // !PRINT_SELECTION_USING_DUMMY_WINDOW
			result = printer_info->GetPrintDevice(tmpPrintDev, preview, FALSE, this);
#endif

	if (result == OpStatus::ERR)
	{
		/* We could for example not get a connection to the printer.
		   We're not necessarily out of memory, though. */

		if (preview)
			preview_mode = !preview_mode;
		else
			print_mode = !print_mode;
	}
	else
		if (result == OpStatus::ERR_NO_MEMORY ||
			(result = doc_manager->SetPrintMode(print_mode || preview_mode, NULL, preview)) != OpBoolean::IS_TRUE)
		{
			// undo
			if (preview)
				preview_mode = !preview_mode;
			else
				print_mode = !print_mode;

			if (result == OpStatus::ERR_NO_MEMORY)
			{
				RaiseCondition(result);

				if (preview)
				{
					OP_DELETE(preview_printer_info);
					preview_printer_info = NULL;
				}
				else
				{
					OP_DELETE(printer_info);
					printer_info = NULL;
				}

				vis_device->Show(NULL);
				vis_device->SetFocus(FOCUS_REASON_OTHER);

				return FALSE;
			}
		}

#ifdef KEYBOARD_SELECTION_SUPPORT
	if (preview_mode || print_mode)
	{
		DocumentTreeIterator it(this);
		while (it.Next())
			it.GetFramesDocument()->SetKeyboardSelectable(FALSE);
	}
#endif // KEYBOARD_SELECTION_SUPPORT

	if (printer_info && !print_mode)
	{
		OP_DELETE(printer_info);
		printer_info = NULL;
	}

	if (preview_printer_info && !preview_mode)
	{
		OP_DELETE(preview_printer_info);
		preview_printer_info = NULL;
	}

	if (preview)
	{
		OpDocumentListener::PrintPreviewMode mode;

		if (preview_mode)
			mode = OpDocumentListener::PRINT_PREVIEW_ON;
		else
			mode = OpDocumentListener::PRINT_PREVIEW_OFF;

		m_windowCommander->GetDocumentListener()->OnPrintPreviewModeChanged(m_windowCommander, mode);
	}

	return tmpPrintDev ? TRUE : FALSE;
}

BOOL Window::StartPrinting(PrinterInfo* pinfo, int from_page, BOOL selected_only)
{
	if (!print_mode)
		LockWindow();

	//rg this variable is uses to save the state of the global flag that shows if we are in full screen.
	// we have to temporarily reset the flag (not the fullscreen mode state, though) to make documents print even
	// though we are in fullscreen mode. I'm not saying it's beautiful -- m_fullscreen_state is mis-used!
	m_previous_fullscreen_state = m_fullscreen_state;
	m_fullscreen_state = OpWindowCommander::FULLSCREEN_NONE;

/*	if (GetPreviewMode())
	{
		// check printer_info
	}
	else */
	{
		printer_info = pinfo;
		OP_NEW_DBG("Window::StartPrinting", "async_print");
		OP_DBG(("SetPrinting(TRUE)"));
		SetPrinting(TRUE);

		TogglePrintMode(FALSE); // Will delete printer_info/pinfo if failing

		if (!printer_info)
		{
			// TogglePrintMode failed & printer_info invalid, better quit now...
			StopPrinting();

			return FALSE;
		}
	}

	OP_ASSERT(printer_info == pinfo);
	print_mode = TRUE;
#ifndef _MACINTOSH_
	printer_info->next_page = from_page;
#endif
	printer_info->print_selected_only = selected_only;

	FramesDocument* doc = doc_manager->GetPrintDoc();

	if (!doc)
	{
		StopPrinting();
		OP_ASSERT(!printer_info);
		return FALSE;
	}

	PrintDevice* pd = printer_info->GetPrintDevice();
	pd->StartOutput();

	return TRUE;
}

void Window::PrintNextPage(PrinterInfo* pinfo)
{
	PrintDevice* pd = pinfo->GetPrintDevice();

	OP_STATUS ret_val = doc_manager->PrintPage(pd, pinfo->next_page, pinfo->print_selected_only);

	pinfo->next_page++;

	if (ret_val != DocStatus::DOC_CANNOT_PRINT &&
		ret_val != DocStatus::DOC_PAGE_OUT_OF_RANGE &&
		(!pinfo->print_selected_only ||
		 ret_val != DocStatus::DOC_PRINTED) &&
		pinfo->next_page <= pinfo->last_page &&
		// Check if there are more pages left to print before advancing to the next page:
		doc_manager->PrintPage(pd, pinfo->next_page, pinfo->print_selected_only, TRUE) == OpStatus::OK)
	{
		BOOL cont = pd->NewPage();

		if (cont)
		{
#if defined (GENERIC_PRINTING)
			msg_handler->PostDelayedMessage(DOC_PAGE_PRINTED, 0, pinfo->next_page - 1, 100);
#else // !GENERIC_PRINTING
			msg_handler->PostMessage(DOC_PAGE_PRINTED, 0, pinfo->next_page - 1);
#endif // !GENERIC_PRINTING
		}
   		else
   		{
#if defined (GENERIC_PRINTING)
			msg_handler->PostDelayedMessage(DOC_PRINTING_ABORTED, 0, 0, 100);
#else // !GENERIC_PRINTING
			msg_handler->PostMessage(DOC_PRINTING_ABORTED, 0, 0);
			StopPrinting();
#endif // !GENERIC_PRINTING
   		}
   	}
   	else
   	{
#if defined (GENERIC_PRINTING)
		msg_handler->PostDelayedMessage(DOC_PRINTING_FINISHED, 0, 0, 100);
#else // !GENERIC_PRINTING
		msg_handler->PostMessage(DOC_PRINTING_FINISHED, 0, 0);
		StopPrinting();
#endif // !GENERIC_PRINTING
   	}
}

void Window::StopPrinting()
{
	m_fullscreen_state = m_previous_fullscreen_state;

	if (print_mode)
	{
		OpStatus::Ignore(doc_manager->SetPrintMode(FALSE, NULL, FALSE));
		if (!preview_mode)
			TogglePrintMode(FALSE);
		ecmascript_paused &= ~PAUSED_PRINTING;
		ResumeEcmaScriptIfNotPaused();
	}

	if (!preview_mode)
	{
#if defined (GENERIC_PRINTING)
		OP_DELETE(printer_info);
#endif // GENERIC_PRINTING
		printer_info = NULL;
	}

	if (print_mode)
		UnlockWindow();
	print_mode = FALSE;
}

#endif // _PRINT_SUPPORT_

void Window::GenericMailToAction(const URL& url, BOOL called_externally, BOOL mailto_from_form, const OpStringC8& servername)
{
	m_windowCommander->GetDocumentListener()->OnMailTo(m_windowCommander, url.GetAttribute(URL::KName_With_Fragment_Escaped, URL::KNoRedirect), called_externally, mailto_from_form, servername);
	if (doc_manager->GetCurrentURL().IsEmpty())
		Close();
}

void Window::ToggleImageState()
{
	BOOL load_images = LoadImages();
	BOOL show_images = ShowImages();

	if (show_images)
		if (load_images)
			SetImagesSetting(FIGS_OFF);
		else
			SetImagesSetting(FIGS_ON);
	else
		SetImagesSetting(FIGS_SHOW);
}

#ifdef _AUTO_WIN_RELOAD_SUPPORT_

//	___________________________________________________________________________
//
//	AutoWinReloadParam
//	___________________________________________________________________________

AutoWinReloadParam::AutoWinReloadParam(Window* win)
	:	m_fEnabled(FALSE),
		m_secUserSetting(0),
		m_secRemaining(0),
		m_fOnlyIfExpired(FALSE),
		m_timer(NULL),
		m_win(win)
{
	if (g_pcdoc)
		m_secUserSetting = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::LastUsedAutoWindowReloadTimeout);
	if (m_secUserSetting<=0)
		m_secUserSetting = 5;
}

AutoWinReloadParam::~AutoWinReloadParam()
{
	OP_DELETE(m_timer);
}

//	----------------------------------------------------------------------------

void AutoWinReloadParam::Enable( BOOL fEnable)
{
	m_fEnabled = fEnable;
	if (fEnable)
	{
		if (!m_timer)
		{
			OP_STATUS status = OpStatus::OK;
			m_timer = OP_NEW(OpTimer, ());
			if (!m_timer)
				status = OpStatus::ERR_NO_MEMORY;

			if (OpStatus::IsError(status))
			{
				OP_ASSERT(FALSE);
				// Now the page won't reload automatically
				g_memory_manager->RaiseCondition(status);//FIXME:OOM At least we can
				//tell the memory manager
				m_timer = NULL;
			}
			else
				m_timer->SetTimerListener(this);
		}

		if (m_timer)
			m_timer->Start(1000); // Every second. This is not optimal
								  // but is the same as it was when
								  // this code piggy backed on the
								  // comm cleaner.

		m_secRemaining = m_secUserSetting;

		m_win->DocManager()->CancelRefresh();
	}
	else if (m_timer)
	{
		m_timer->Stop();

		// document refresh has been disabeled as long as user defined reload is
		// active, but now we have to activate refresh again (in all sub documents)
		DocumentTreeIterator iter(m_win);
		iter.SetIncludeThis();

		while (iter.Next())
			iter.GetFramesDocument()->CheckRefresh();
	}
}

void AutoWinReloadParam::Toggle( )
{
	Enable(!m_fEnabled);
}

void AutoWinReloadParam::SetSecUserSetting ( int secUserSetting)
{
	SetParams( m_fEnabled, secUserSetting, m_fOnlyIfExpired, TRUE);
}

void AutoWinReloadParam::SetParams( BOOL fEnable, int secUserSetting, BOOL fOnlyIfExpired, BOOL fUpdatePrefsManager)
{
	m_secUserSetting = secUserSetting;
	if (m_secUserSetting<=0)
		m_secUserSetting = 1;
	m_fOnlyIfExpired = fOnlyIfExpired;
	Enable( fEnable);
	if (fUpdatePrefsManager)
	{
		TRAPD(err, g_pcdoc->WriteIntegerL(PrefsCollectionDoc::LastUsedAutoWindowReloadTimeout, m_secUserSetting));
	}
}

BOOL AutoWinReloadParam::OnSecTimer()
{
	BOOL fReload = FALSE;
	if (m_fEnabled)
	{
		if (m_secUserSetting > 0)
		{
			--m_secRemaining;
			OP_ASSERT(m_secRemaining >= 0);

			if (m_secRemaining <= 0)
			{
				m_secRemaining = m_secUserSetting;
				fReload = TRUE;
			}
		}
	}
	return fReload;
}

/* virtual */
void AutoWinReloadParam::OnTimeOut(OpTimer* timer)
{
	OP_ASSERT(timer);
	OP_ASSERT(timer == m_timer);
	if (m_fEnabled)
	{
		//
		//	Auto user reload ?
		//
		DocumentManager *doc_manager = m_win->DocManager();
		if (doc_manager->GetLoadStatus() == NOT_LOADING
			&& OnSecTimer())
		{
			//	DEBUG_Beep();
			doc_manager->Reload(NotEnteredByUser, GetOptOnlyIfExpired(), TRUE, TRUE);
		}
		m_timer->Start(1000); // Every second. Not optimal. See code in Enable()
	}
}

#endif // _AUTO_WIN_RELOAD_SUPPORT_

FramesDocument* Window::GetActiveFrameDoc()
{
	FramesDocument* activeDoc = NULL;
	FramesDocument* currentDoc = GetCurrentDoc();

	if (currentDoc)
	{
		// The active frame document can always be found through the root
		// document (GetCurrentDoc) because all frame activations are done
		// directly on that document.

		DocumentManager* activeDocMan = currentDoc->GetActiveDocManager();
		if (!activeDocMan)
			activeDocMan = DocManager();
		activeDoc = activeDocMan->GetCurrentDoc();
	}

	return activeDoc;
}

OP_STATUS Window::SetLastUserName(const char *_name)
{
	return SetStr(lastUserName, _name);
}

OP_STATUS Window::SetLastProxyUserName(const char *_name)
{
	return SetStr(lastProxyUserName, _name);
}

OP_STATUS Window::SetHomePage(const OpStringC& url)
{
	return homePage.Set(url);
}

void Window::SetAlwaysLoadFromCache(BOOL from_cache)
{
	always_load_from_cache = from_cache;
}

#ifndef HAS_NO_SEARCHTEXT
BOOL Window::SearchText(const uni_char* key, BOOL forward, BOOL matchcase, BOOL word, BOOL next, BOOL wrap, BOOL only_links)
{
	if( !key )
	{
		return FALSE;
	}

	FramesDocument* doc = doc_manager->GetCurrentVisibleDoc();
	if (doc && doc->IsFrameDoc())
		doc = doc->GetActiveSubDoc();
	if (!doc)
		return FALSE;

    int left_x, right_x;
    long top_y, bottom_y;

	OP_BOOLEAN result = doc->SearchText(key, uni_strlen(key), forward, matchcase, word, next, wrap,
			                            left_x, top_y, right_x, bottom_y, only_links);

	if (result == OpStatus::ERR_NO_MEMORY)
		RaiseCondition(result);
    else if (result == OpBoolean::IS_TRUE)
	{
		doc->RequestScrollIntoView(
			OpRect(left_x, top_y, right_x - left_x + 1, bottom_y - top_y + 1),
			SCROLL_ALIGN_CENTER, FALSE, VIEWPORT_CHANGE_REASON_FIND_IN_PAGE);
		return TRUE;
    }

    return FALSE;
}

# ifdef SEARCH_MATCHES_ALL
void
Window::ResetSearch()
{
	if (m_search_data)
	{
		m_search_data->SetIsNewSearch(TRUE);
		m_search_data->SetMatchingDoc(NULL);
	}
}

BOOL
Window::HighlightNextMatch(const uni_char* key,
					BOOL match_case, BOOL match_words,
					BOOL links_only, BOOL forward, BOOL wrap,
					BOOL &wrapped, OpRect& rect)
{
	wrapped = FALSE;
	FramesDocument* doc = doc_manager->GetCurrentVisibleDoc();

	if (!doc)
		return FALSE;

	if (!m_search_data)
	{
		m_search_data = OP_NEW(SearchData, (match_case, match_words, links_only, forward, wrap));
		if (!m_search_data || OpStatus::IsMemoryError(m_search_data->SetSearchText(key)))
			return FALSE;
	}
	else
	{
		m_search_data->SetMatchCase(match_case);
		m_search_data->SetMatchWords(match_words);
		m_search_data->SetLinksOnly(links_only);
		m_search_data->SetForward(forward);
		m_search_data->SetWrap(wrap);
		m_search_data->SetSearchText(key);
	}

	SetState(BUSY);

	OP_BOOLEAN found = doc->HighlightNextMatch(m_search_data, rect);

	if (wrap && m_search_data->GetMatchingDoc() == NULL && !m_search_data->IsNewSearch())
	{
		found = doc->HighlightNextMatch(m_search_data, rect);
		wrapped = TRUE;
	}

	if (found == OpBoolean::IS_TRUE)
	{
		if (!doc->IsTopDocument())
		{
			FramesDocElm* frame = doc->GetDocManager() ? doc->GetDocManager()->GetFrame() : NULL;
			if (frame && doc->GetVisualDevice())
			{
				rect.x += frame->GetAbsX() - doc->GetVisualDevice()->GetRenderingViewX();
				rect.y += frame->GetAbsY() - doc->GetVisualDevice()->GetRenderingViewY();
			}
		}
	}

	SetState(NOT_BUSY);

	m_search_data->SetIsNewSearch(FALSE);

	return m_search_data->GetMatchingDoc() != NULL;
}

void Window::EndSearch(BOOL activate_current_match, int modifiers)
{
	if (activate_current_match && m_search_data)
	{
		if (FramesDocument *match_doc = m_search_data->GetMatchingDoc())
		{
			HTML_Document *html_doc = match_doc->GetHtmlDocument();
			if (html_doc)
				html_doc->ActivateElement(html_doc->GetNavigationElement(), modifiers);
		}
	}
}

# endif // SEARCH_MATCHES_ALL
#endif // !HAS_NO_SEARCHTEXT

#ifdef ABSTRACT_CERTIFICATES

const OpCertificate *Window::GetCertificate()
{
	return static_cast <const OpCertificate *> (doc_manager->GetCurrentURL().GetAttribute(URL::KRequestedCertificate, NULL));
}

#endif // ABSTRACT_CERTIFICATES

void Window::Activate( BOOL fActivate/*=TRUE*/)
{
}

void Window::GotoHistoryPos()
{
	if (current_history_number < max_history_number && current_history_number >= 0)
		SetCurrentHistoryPos(current_history_number, TRUE, FALSE);
	else
		SetCurrentHistoryPos(max_history_number, TRUE, FALSE);
}

/** Flushes pending drawoperations so that things wouldn't lag behind, if we do something agressive. */
void Window::Sync()
{
	VisualDev()->view->Sync();
}

void Window::SetWindowSize(int wWin, int hWin)
{
	m_windowCommander->GetDocumentListener()->OnOuterSizeRequest(m_windowCommander, wWin, hWin);
}

void Window::SetWindowPos(int xWin, int yWin)
{
	m_windowCommander->GetDocumentListener()->OnMoveRequest(m_windowCommander, xWin, yWin);
}

void Window::GetWindowSize(int& wWin, int& hWin)
{
	m_windowCommander->GetDocumentListener()->OnGetOuterSize(m_windowCommander, (UINT32*) &wWin, (UINT32*) &hWin);
}

void Window::GetWindowPos(int& xWin, int& yWin)
{
	m_windowCommander->GetDocumentListener()->OnGetPosition(m_windowCommander, &xWin, &yWin);
}

//	Set the size of the client area (space available for doc content)
void Window::SetClientSize	( int cx, int cy)
{
	m_windowCommander->GetDocumentListener()->OnInnerSizeRequest(m_windowCommander, cx, cy);
}

void Window::GetClientSize(int &cx, int &cy)
{
	UINT32 tmpw=0, tmph=0;
	m_windowCommander->GetDocumentListener()->OnGetInnerSize(m_windowCommander, &tmpw, &tmph);
	cx = tmpw;
	cy = tmph;
}

void Window::GetCSSViewportSize(unsigned int& viewport_width, unsigned int& viewport_height)
{
	UINT32 tmpw = 0;
	UINT32 tmph = 0;

	m_windowCommander->GetDocumentListener()->OnGetInnerSize(m_windowCommander, &tmpw, &tmph);

	if (GetTrueZoom())
	{
		viewport_width = VisualDev()->LayoutScaleToDoc(tmpw);
		viewport_height = VisualDev()->LayoutScaleToDoc(tmph);
	}
	else
	{
		viewport_width = VisualDev()->ScaleToDoc(tmpw);
		viewport_height = VisualDev()->ScaleToDoc(tmph);
	}
}

void Window::HandleNewWindowSize(unsigned int width, unsigned int height)
{
	VisualDevice* normal_vis_dev = VisualDev();
	FramesDocument* doc;

#ifdef _PRINT_SUPPORT_
	VisualDevice* print_preview_vis_dev = NULL;

	if (GetPreviewMode())
	{
		print_preview_vis_dev = doc_manager->GetPrintPreviewVD();
		doc = doc_manager->GetPrintDoc();
	}
	else
#endif // _PRINT_SUPPORT_
		doc = doc_manager->GetCurrentDoc();

	// Update with new rendering buffer size as well.

	UINT32 rendering_width, rendering_height;
	OpRect rendering_rect;
	m_opWindow->GetRenderingBufferSize(&rendering_width, &rendering_height);
	rendering_rect.width = rendering_width;
	rendering_rect.height = rendering_height;
	normal_vis_dev->SetRenderingViewGeometryScreenCoords(rendering_rect);

#ifdef DEBUG
	// We assume that OpWindow tells us the correct (new) window size. Let's verify it's true.

	{
		UINT32 window_width, window_height;
		m_opWindow->GetInnerSize(&window_width, &window_height);
		OP_ASSERT(window_width == width && window_height == height);
	}
#endif // DEBUG

#ifdef _PRINT_SUPPORT_
	if (print_preview_vis_dev)
		print_preview_vis_dev->SetRenderingViewGeometryScreenCoords(rendering_rect);
#endif // _PRINT_SUPPORT_

	if (doc)
	{
		doc->RecalculateScrollbars();
		doc->RecalculateLayoutViewSize(TRUE);
		doc->RequestSetViewportToInitialScale(VIEWPORT_CHANGE_REASON_WINDOW_SIZE);
	}

#if defined SCROLL_TO_ACTIVE_ELM_ON_RESIZE && defined SCROLL_TO_ACTIVE_ELM
	if (FramesDocument* active_frame = GetActiveFrameDoc())
		active_frame->ScrollToActiveElement();
#endif // SCROLL_TO_ACTIVE_ELM_ON_RESIZE && SCROLL_TO_ACTIVE_ELM
}

OP_STATUS Window::UpdateWindow(BOOL /* unused = FALSE */)
{
	// Set expirytype temporary to never, and then change back after the UpdateWindow.
	// Prevents expired images from getting reloaded when we change layoutmode.
	CheckExpiryType old_expiry_type = doc_manager->GetCheckExpiryType();
	doc_manager->SetCheckExpiryType(CHECK_EXPIRY_NEVER);

	if (doc_manager->ReformatCurrentDoc() == OpStatus::ERR_NO_MEMORY)
		return OpStatus::ERR_NO_MEMORY;

	doc_manager->SetCheckExpiryType(old_expiry_type);

	return OpStatus::OK;
}

OP_STATUS Window::UpdateWindowAllDocuments(BOOL /* unused = FALSE */)
{
    if (doc_manager)
    {
        // Set expirytype temporary to never, and then change back after the UpdateWindowAllDocuments.
        // Prevents expired images from getting reloaded when we change layoutmode.
        CheckExpiryType old_expiry_type = doc_manager->GetCheckExpiryType();
		doc_manager->SetCheckExpiryType(CHECK_EXPIRY_NEVER);
        
		DocListElm* elm = doc_manager->FirstDocListElm();
		while (elm)
		{
			FramesDocument* frm_doc = elm->Doc();
			if (frm_doc)
				doc_manager->ReformatDoc(frm_doc);
			
			elm = elm->Suc();
		}
		doc_manager->SetCheckExpiryType(old_expiry_type);
    }       
    return OpStatus::OK;
}

OP_STATUS Window::UpdateWindowDefaults(BOOL scroll, BOOL progress, BOOL news, WORD scale, BOOL size)
{
	SetShowScrollbars(scroll);
    SetScale(scale);
	return SetWindowTitle(title, TRUE, generated_title);
}

#ifndef MOUSELESS
void
Window::UseDefaultCursor()
{
	has_cursor_set_by_doc = FALSE;
	current_cursor = GetCurrentCursor();
	SetCurrentCursor();
}

void
Window::CommitPendingCursor()
{
	if(m_pending_cursor == CURSOR_AUTO)
		UseDefaultCursor();
	else
		SetCursor(m_pending_cursor, TRUE);
	m_pending_cursor = CURSOR_AUTO;
}

void
Window::SetCurrentCursor()
{
	m_opWindow->SetCursor(current_cursor);
}

void Window::SetCursor(CursorType cursor, BOOL set_by_document/*=FALSE*/)
{
	if (set_by_document)
	{
		has_cursor_set_by_doc = TRUE;
		cursor_set_by_doc = cursor;
	}

	// Only allow cursor changes in case the cursor either comes from
	// a trusted source (page load) or the window currently has
	// no page load cursor.
	if (!set_by_document || !IsNormalWindow() || state != BUSY)
	{
		if (cursor != current_cursor)
		{
			current_cursor = cursor;
			SetCurrentCursor();
		}
	}
}
#endif // !MOUSELESS

void Window::ClearMessage()
{
	OP_DELETEA(current_message);
	current_message = NULL;
	m_windowCommander->GetDocumentListener()->OnNoHover(m_windowCommander);
}

void Window::SetMessageInternal()
{
	m_windowCommander->GetDocumentListener()->OnStatusText(m_windowCommander, current_message ? current_message : current_default_message);
}

OP_STATUS Window::SetMessage(const uni_char* msg)
{
	if (!msg)
	{
		OP_DELETEA(current_message);
		current_message = NULL;
	}
	else if (current_message && uni_strcmp(current_message, msg) == 0)
	{
		// Nothing changed, bail out

		if (current_default_message)
		{
			g_windowManager->SetPopupMessage(current_message);
		}
		return OpStatus::OK;
	}
	else
	{
		RETURN_IF_ERROR(UniSetStr(current_message, msg));
	}

	SetMessageInternal();

	return OpStatus::OK;
}

OP_STATUS Window::SetDefaultMessage(const uni_char* msg)
{
	if (!msg)
	{
		OP_DELETEA(current_default_message);
		current_default_message = NULL;
	}
	else
		if (current_default_message && uni_strcmp(current_default_message, msg) == 0)
			// Nothing changed, bail out

			return OpStatus::OK;
		else
			RETURN_IF_ERROR(UniSetStr(current_default_message, msg));

	m_windowCommander->GetDocumentListener()->OnStatusText(m_windowCommander, current_message ? current_message : current_default_message);

	return OpStatus::OK;
}

void Window::SetActiveLinkURL(const URL& url, HTML_Document* doc)
{
	const uni_char* title = NULL;

	if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::DisplayLinkTitle) && doc)
	{
		// Find the title string for this link.
		HTML_Element *el = doc->GetActiveHTMLElement();
		for (; el; el = el->Parent())
		{
			title = el->GetStringAttr(ATTR_TITLE);
			if (title || el->Type() == HE_A) break;
		}
	}

	if (title)
		DisplayLinkInformation(url, ST_ATITLE, title);
	else
		DisplayLinkInformation(url, ST_ALINK);
#ifndef MOUSELESS
	SetCursor(CURSOR_CUR_POINTER);
#endif // !MOUSELESS
	active_link_url = url;
}

void Window::ClearActiveLinkURL()
{
	ClearMessage();
	SetState(GetState()); // Reset the mouse cursor.
	URL empty;
	active_link_url = empty;
}

void Window::SetImagesSetting(SHOWIMAGESTATE set)
{
#ifdef GADGET_SUPPORT
	if (GetGadget())
	{
        load_img = TRUE;
        show_img = TRUE;
	}
	else
#endif
	{
		switch (set)
		{

		case FIGS_OFF:
			load_img = FALSE;
			show_img = FALSE;
			break;

		case FIGS_SHOW:
			load_img = FALSE;
			show_img = TRUE;
			break;

		default:
			load_img = TRUE;
			show_img = TRUE;
			break;
		}
	}

	doc_manager->SetShowImg(show_img);

	/*{
        FramesDocument *doc = doc_manager->GetCurrentDoc();
        if (doc)
        {
			doc->SetShowImg(show_img);
            / *if (doc->SetShowImg(show_img) == DOC_NEED_IMAGE_RELOAD)
            {
                if (load_img)
                {
					StartProgressDisplay(TRUE, TRUE, TRUE);
                    SetState(BUSY);
                    SetState(CLICKABLE);
                }
            }* /
        }
    }*/

	OpDocumentListener::ImageDisplayMode mode;
#ifdef GADGET_SUPPORT
	if (GetGadget())
	{
		mode = OpDocumentListener::ALL_IMAGES;
	}
	else
#endif
	{
		switch (set)
		{
		case FIGS_OFF:
			mode = OpDocumentListener::NO_IMAGES;
			break;
		case FIGS_SHOW:
			mode = OpDocumentListener::LOADED_IMAGES;
			break;
		default:
			mode = OpDocumentListener::ALL_IMAGES;
			break;
		}
    }
	m_windowCommander->GetDocumentListener()->OnImageModeChanged(m_windowCommander, mode);
}

LayoutMode Window::GetLayoutMode() const
{
	if (era_mode)
		if (FramesDocument* doc = GetCurrentDoc())
			return doc->GetLayoutMode();

	return layout_mode;
}

void Window::SetLayoutMode(LayoutMode mode)
{
	if (layout_mode != mode)
	{
		LayoutMode old_layout_mode = layout_mode;

		layout_mode = mode;
		is_ssr_mode = (mode == LAYOUT_SSR || mode == LAYOUT_CSSR); // use handheld until obsolete

		FramesDocument* frames_doc = doc_manager->GetCurrentDoc();

		int old_iframe_policy = SHOW_IFRAMES_ALL;

		if (frames_doc)
			old_iframe_policy = frames_doc->GetShowIFrames();

		HLDocProfile *hldoc = frames_doc ? frames_doc->GetHLDocProfile() : NULL;
		if (hldoc && old_layout_mode == LAYOUT_SSR)
			hldoc->LoadAllCSS();

		OP_STATUS ret = UpdateWindow(); // cause reformat because stylesheets may be different
		if (OpStatus::IsError(ret))
		{
			RaiseCondition(ret);
		}

		if (frames_doc)
		{
			int iframe_policy = frames_doc->GetShowIFrames();
			if (iframe_policy != SHOW_IFRAMES_NONE && old_iframe_policy != iframe_policy)
			{
				//FIXME  SHOW_IFRAMES_SOME
				frames_doc->LoadAllIFrames();
			}
			frames_doc->RecalculateLayoutViewSize(TRUE);
		}
	}
}

void Window::SetFramesPolicy(INT32 value)
{
	if (m_frames_policy != value)
	{
		m_frames_policy = value;
		UpdateWindow();
	}
}

void Window::SetERA_Mode(BOOL value)
{
#ifdef GADGET_SUPPORT
	if (GetGadget() && value == TRUE)
		return;
#endif

	if (era_mode != value)
	{
		era_mode = value;

		DocumentTreeIterator iter(this);

		iter.SetIncludeThis();

		while (iter.Next())
			iter.GetFramesDocument()->ERA_ModeChanged();

		UpdateWindow(); // reformat
	}
}

void Window::Reload(EnteredByUser entered_by_user, BOOL conditionally_request_document, BOOL conditionally_request_inlines)
{
#ifdef _PRINT_SUPPORT_
	if (print_mode)
		StopPrinting();
#endif // _PRINT_SUPPORT_
	doc_manager->Reload(entered_by_user, conditionally_request_document, conditionally_request_inlines, FALSE);
}

BOOL Window::CancelLoad(BOOL oom)
{
	is_canceling_loading = TRUE;

	doc_manager->StopLoading(!oom, TRUE, TRUE);
	if (oom)
		SetOOMOccurred(TRUE);

	SetState(NOT_BUSY);
	EndProgressDisplay();

	EnsureHistoryLimits();

	phase_uploading = FALSE;
	is_canceling_loading = FALSE;
	return TRUE;
}

void Window::Raise()
{
	m_windowCommander->GetDocumentListener()->OnRaiseRequest(m_windowCommander);
}

void Window::Lower()
{
	m_windowCommander->GetDocumentListener()->OnLowerRequest(m_windowCommander);
}

FramesDocument *Window::GetCurrentDoc() const
{
	return doc_manager->GetCurrentDoc();
}

void Window::HighlightURL(BOOL forward)
{
	FramesDocument* doc = GetActiveFrameDoc();

	if (doc)
	{
		if (doc->HighlightNextElm(HE_A, HE_A, forward, TRUE /*ignore lower/upper - get next anchor elm */))
		{
#ifdef DISPLAY_CLICKINFO_SUPPORT
			// Save link title for bookmark purposes
			MouseListener::GetClickInfo().SetTitleElement(doc, doc->GetCurrentHTMLElement());
#endif // DISPLAY_CLICKINFO_SUPPORT

			const uni_char* window_name = 0;
			URL url = doc->GetCurrentURL(window_name);

			if (!url.IsEmpty())
			{
				// Eventually it would be nice to unescape the url to make it more readable, but then we need to
				// get the decoding to do it correctly with respect to charset encodings. See bug 250545
				OpStringC hlink = url.GetAttribute(URL::KUniName_With_Fragment_Escaped);
				if (hlink.HasContent())
					g_windowManager->SetPopupMessage(hlink.CStr(), FALSE, ST_ALINK);
			}
		}
	}
}

#ifdef _SPAT_NAV_SUPPORT_

/**
 *  Returns a pointer to the Window's Spatial Navigation handler.
 *  If the handler does not exist it is constructed and initialized.
 *  If memory allocation fails, then the function returns 0.
 *
 *  @return Pointer to the SnHandler or 0 on failure.
 *
 */
OpSnHandler* Window::GetSnHandlerConstructIfNeeded ()
{
	if (sn_handler == NULL)
	{
#ifdef PHONE_SN_HANDLER
		sn_handler = OP_NEW(PhoneSnHandler, ());
#else
		sn_handler = OP_NEW(SnHandler, ());
#endif

		if (sn_handler == NULL) // Out om memory!
		{
			return 0;
		}

		if (OpStatus::IsError(sn_handler->Init(this)))
		{
			OP_DELETE(sn_handler);
			sn_handler = NULL;

			return 0;
		}
	}
	return sn_handler;
}


BOOL Window::MarkNextLinkInDirection(INT32 direction, POINT* startingPoint, BOOL scroll)
{
	OpSnHandler* handler = GetSnHandlerConstructIfNeeded();

	if (handler && handler->MarkNextLinkInDirection(direction, startingPoint, scroll))
		return TRUE;
#ifdef SN_LEAVE_SUPPORT
	else
		LeaveInDirection(direction);
#endif // SN_LEAVE_SUPPORT

	return FALSE;
}

BOOL Window::MarkNextImageInDirection(INT32 direction, POINT* startingPoint, BOOL scroll)
{
	OpSnHandler* handler = GetSnHandlerConstructIfNeeded();
	if (handler)
		return handler->MarkNextImageInDirection(direction, startingPoint, scroll);
	return FALSE;
}

BOOL Window::MarkNextItemInDirection(INT32 direction, POINT* startingPoint, BOOL scroll)
{
	OpSnHandler* handler = GetSnHandlerConstructIfNeeded();

#ifdef SN_LEAVE_SUPPORT
	if (!handler || !handler->MarkNextItemInDirection(direction, startingPoint, scroll))
		LeaveInDirection(direction);

	return TRUE;

#else // SN_LEAVE_SUPPORT
	if (handler)
	{
		BOOL state = handler->MarkNextItemInDirection(direction, startingPoint, scroll);

#ifdef DISPLAY_CLICKINFO_SUPPORT
		FramesDocument* doc = GetActiveFrameDoc();
		if (doc)
		{
			// Save link title for bookmark purposes
			MouseListener::GetClickInfo().SetTitleElement(doc, doc->GetCurrentHTMLElement());
		}
#endif // DISPLAY_CLICKINFO_SUPPORT
		return state;
	}

	return FALSE;
#endif // SN_LEAVE_SUPPORT
}


#ifdef SN_LEAVE_SUPPORT
//OBS! This is a temporary hack!
// It should probably be moved to some better place.
void Window::LeaveInDirection(INT32 direction)
{
	if (sn_listener == NULL) return;

	FramesDocument *doc = GetActiveFrameDoc();

	if (!doc)
	{
		sn_listener->LeaveInDirection(direction);
		return;
	}

	HTML_Document *hdoc = doc->GetHtmlDocument();

	if (!hdoc)
	{
		sn_listener->LeaveInDirection(direction);
		return;
	}

	HTML_Element *helm = hdoc->GetNavigationElement();

	if (!helm)
	{
		sn_listener->LeaveInDirection(direction);
		return;
	}

	LogicalDocument *logdoc = doc->GetLogicalDocument();

	if (!logdoc)
	{
		sn_listener->LeaveInDirection(direction);
		return;
	}

	RECT rect;

	if (!logdoc->GetBoxRect(helm, rect))
	{
		sn_listener->LeaveInDirection(direction);
		return;
	}

	VisualDevice *visdev = doc->GetVisualDevice();

	if (visdev == NULL)
	{
		sn_listener->LeaveInDirection(direction);
		return;
	}

	// Calculate the "leave point"
	int x = 0;
	int y = 0;
	switch ((direction % 360) / 90)
	{
	case 0: // Right
		x = rect.right;
		y = (rect.top + rect.bottom) / 2;
		break;
	case 1: // Up
		x = (rect.right + rect.left) / 2;
		y = rect.top;
		break;
	case 2: // Left
		x = rect.left;
		y = (rect.top + rect.bottom) / 2;
		break;
	case 3: // Down
		x = (rect.right + rect.left) / 2;
		y = rect.bottom;
		break;
	}

	sn_listener->LeaveInDirection(direction, x, y, visdev->GetOpView());
	return;
}

void Window::SetSnLeaveListener(SnLeaveListener *listener)
{
	sn_listener = listener;
}

void Window::SnMarkSelection()
{
	SnHandler* handler = static_cast<SnHandler *>(GetSnHandler());

	if (handler) handler->HighlightCurrentElement();
}

#endif //SN_LEAVE_SUPPORTED

#endif // _SPAT_NAV_SUPPORT_

void Window::HighlightHeading(BOOL forward)
{
    if (FramesDocument* doc = GetActiveFrameDoc())
		doc->HighlightNextElm(HE_H1, HE_H6, forward, FALSE);
}

void Window::HighlightItem(BOOL forward)
{
    if (FramesDocument* doc = GetActiveFrameDoc())
        doc->HighlightNextElm(Markup::HTE_FIRST, Markup::HTE_LAST, forward, FALSE);
}

OP_STATUS Window::GetHighlightedURL(uint16 key, BOOL bNewWindow, BOOL bOpenInBackground)
{
    FramesDocument* doc = GetCurrentDoc();

	if (doc)
	{
		FramesDocument *sub_doc = doc->GetActiveSubDoc();

		{
#ifdef _WML_SUPPORT_
			if (doc->GetDocManager()->WMLHasWML())
			{
				if (!FormManager::ValidateWMLForm(doc))
					return OpStatus::OK;
			}
#endif // _WML_SUPPORT_
			int sub_win_id = -1;
			const uni_char* window_name = NULL;
			URL url = doc->GetCurrentURL(window_name);

			if (!url.IsEmpty())
				return g_windowManager->OpenURLNamedWindow(url, this, doc, sub_doc ? sub_doc->GetSubWinId() : sub_win_id, window_name, TRUE, bNewWindow, bOpenInBackground, TRUE);
			else if (key==OP_KEY_SPACE)
			{
                if (doc->IsFrameDoc())
					doc = doc->GetActiveSubDoc();

                ScrollDocument(doc, bNewWindow ? (bOpenInBackground? OpInputAction::ACTION_GO_TO_START : OpInputAction::ACTION_PAGE_UP)
				                               : (bOpenInBackground? OpInputAction::ACTION_GO_TO_END : OpInputAction::ACTION_PAGE_DOWN));
			}
		}
    }

	return OpStatus::OK;
}

void Window::RaiseCondition( OP_STATUS type )
{
	if (type == OpStatus::ERR_NO_MEMORY)
		GetMessageHandler()->PostOOMCondition(TRUE);
	else if (type == OpStatus::ERR_SOFT_NO_MEMORY)
		GetMessageHandler()->PostOOMCondition(FALSE);
	else if (type == OpStatus::ERR_NO_DISK)
		GetMessageHandler()->PostOODCondition();
	else
		OP_ASSERT(0);
}

Window::OnlineMode Window::GetOnlineMode()
{
	if (m_online_mode == Window::ONLINE || m_online_mode == Window::OFFLINE)
		m_online_mode = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode) ? OFFLINE : ONLINE;

	return m_online_mode;
}

OP_STATUS Window::QueryGoOnline(URL *url)
{
	if (m_online_mode == Window::OFFLINE)
	{
		OpString url_name;
		OpString message;

		RETURN_IF_ERROR(url->GetAttribute(URL::KUniName_Username_Password_Hidden, url_name, TRUE));

		if (url_name.Length() > 100)
			url_name.Delete(90).Append(UNI_L("..."));

		OpString warning;
		TRAPD(err, g_languageManager->GetStringL(Str::SI_MSG_OFFLINE_WARNING, warning));

		if (OpStatus::IsMemoryError(err))
		{
			return err;
		}

		RETURN_IF_ERROR(message.AppendFormat(warning.CStr(), url_name.CStr()));

		m_online_mode = Window::USER_RESPONDING;

		WindowCommander* wc = GetWindowCommander();
		OP_ASSERT(wc);
		OpDocumentListener *listener = wc->GetDocumentListener();

		listener->OnQueryGoOnline(wc, message.CStr(), this);
	}
	return OpStatus::OK;
}

void Window::OnDialogReply(DialogCallback::Reply reply)
{
	if (reply == DialogCallback::REPLY_YES)
	{
		m_online_mode = Window::ONLINE;

		if (UpdateOfflineMode(FALSE) == OpBoolean::IS_TRUE)
			UpdateOfflineMode(TRUE);
	}
	else
	{
		m_online_mode = Window::DENIED;

		if (OpStatus::IsMemoryError(DocManager()->OnlineModeChanged()))
			RaiseCondition(OpStatus::ERR_NO_MEMORY);
	}
}

void
Window::LockWindow()
{
	window_locked++;
}

void
Window::UnlockWindow()
{
	OP_ASSERT(window_locked);
	window_locked--;

	if (window_locked == 0 && window_closed)
	{
		g_main_message_handler->PostMessage(MSG_ES_CLOSE_WINDOW, 0, id);
		window_closed = FALSE;
	}
}

BOOL
Window::IsURLAlreadyRequested(const URL &url)
{
	return loading && use_already_requested_urls && already_requested_urls.Contains(url.Id());
}

void
Window::SetURLAlreadyRequested(const URL &url)
{
	if (loading && use_already_requested_urls)
		if (OpStatus::IsMemoryError(already_requested_urls.Add(url.Id())))
		{
			already_requested_urls.RemoveAll();
			use_already_requested_urls = FALSE;
		}
}

void
Window::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
#ifdef LOCALE_CONTEXTS
	if (doc_manager && doc_manager->GetCurrentDoc())
		g_languageManager->SetContext(doc_manager->GetCurrentDoc()->GetURL().GetContextId());
#endif

	switch (msg)
	{
	case MSG_PROGRESS_START:
		if (!loading)
		{
#ifdef DEBUG_LOAD_STATUS
			PrintfTofile("loading.txt","================================%lu\n", (unsigned long) g_op_time_info->GetWallClockMS());
#endif
			SetLoading(TRUE);
			already_requested_urls.RemoveAll();
			use_already_requested_urls = TRUE;
#if defined(EPOC)
			if (m_windowCommander)	// Check in case this Window is the "hidden transfer window"
			{
				Window* epocWindow = m_windowCommander ? m_windowCommander->GetWindow() : 0;
				if((epocWindow->m_loading_information).isUploading)
					m_windowCommander->GetLoadingListener()->OnStartUploading(m_windowCommander);
				else
					m_windowCommander->GetLoadingListener()->OnStartLoading(m_windowCommander);
			}
#else // EPOC
			m_windowCommander->GetLoadingListener()->OnStartLoading(m_windowCommander);
#endif // EPOC
		}
		break;

	case MSG_PROGRESS_END:
		if (loading)
		{
#ifdef DEBUG_LOAD_STATUS
			PrintfTofile("loading.txt","================================%lu\n", (unsigned long) g_op_time_info->GetWallClockMS());
#endif
			SetLoading(FALSE);

			UnlockPaintLock();
			pending_unlock_all_painting = FALSE;

			already_requested_urls.RemoveAll();
			use_already_requested_urls = TRUE;

			OpLoadingListener::LoadingFinishStatus status = static_cast<OpLoadingListener::LoadingFinishStatus>(par2);
			m_windowCommander->GetLoadingListener()->OnLoadingFinished(m_windowCommander, status);

			if (m_online_mode == Window::DENIED)
				m_online_mode = Window::OFFLINE;

#ifdef _SPAT_NAV_SUPPORT_
			OpSnHandler* handler = GetSnHandlerConstructIfNeeded();
			if (handler)
				handler->OnLoadingFinished();
#endif // _SPAT_NAV_SUPPORT_

			OP_STATUS oom = doc_manager->UpdateWindowHistoryAndTitle();
			if (OpStatus::IsMemoryError(oom))
				RaiseCondition(oom);
		}
		break;

#ifdef _PRINT_SUPPORT_
	case DOC_START_PRINTING:
# ifdef GENERIC_PRINTING
		{
			PrinterInfo * prnInfo = OP_NEW(PrinterInfo, ());
			int start_page = 1;
			if (prnInfo == NULL)
			{
				RaiseCondition(OpStatus::ERR_NO_MEMORY);
				break;
			}

			PrintDevice * printDevice;
#  ifdef PRINT_SELECTION_USING_DUMMY_WINDOW
			Window * selection_window = 0;
			OP_STATUS result = prnInfo->GetPrintDevice(printDevice, FALSE, FALSE, this, &selection_window);
#  else
			OP_STATUS result = prnInfo->GetPrintDevice(printDevice, FALSE, FALSE, this);
#  endif
			if (OpStatus::IsError(result) || !printDevice->StartPage())
			{
				OP_DELETE(prnInfo);
				break;
			}

			SetFramesPrintType((DM_PrintType)g_pcprint->GetIntegerPref(PrefsCollectionPrint::DefaultFramesPrintType), TRUE);

			Window * window = this;
#  ifdef PRINT_SELECTION_USING_DUMMY_WINDOW
			if (selection_window != 0)
				window = selection_window;
#  endif
			window->StartPrinting(prnInfo, start_page, FALSE);
		}
# endif // GENERIC_PRINTING
		break;

	case DOC_PRINT_FORMATTED:
# if defined GENERIC_PRINTING
		if (printer_info)
			printer_info->GetPrintDevice()->GetPrinterController()->PrintDocFormatted(printer_info);
		else if (preview_printer_info)
			preview_printer_info->GetPrintDevice()->GetPrinterController()->PrintDocFormatted(preview_printer_info);
		{
			OpPrintingListener::PrintInfo info; //rg fixme
			info.status = OpPrintingListener::PrintInfo::PRINTING_STARTED;
			m_windowCommander->GetPrintingListener()->OnPrintStatus(m_windowCommander, &info);
		}
# elif defined MSWIN
		{
			OP_NEW_DBG("Window::HandleMessage", "async_print");
			OP_DBG(("Will enter ucPrintFormatted"));
			ucPrintDocFormatted();
		}
# elif defined _X11_ || defined _MACINTOSH_
		PrintNextPage(printer_info ? printer_info : preview_printer_info);
# endif // GENERIC_PRINTING
		break;

	case DOC_PAGE_PRINTED:
# if defined GENERIC_PRINTING
		if (printer_info)
			printer_info->GetPrintDevice()->GetPrinterController()->PrintPagePrinted(printer_info);
		else if (preview_printer_info)
			preview_printer_info->GetPrintDevice()->GetPrinterController()->PrintPagePrinted(preview_printer_info);
		{
			OpPrintingListener::PrintInfo info; //rg fixme
			info.status = OpPrintingListener::PrintInfo::PAGE_PRINTED;
			info.currentPage = par2;
			m_windowCommander->GetPrintingListener()->OnPrintStatus(m_windowCommander, &info);
		}
# else // GENERIC_PRINTING
		{
			OpPrintingListener::PrintInfo info; //rg fixme
			info.status = OpPrintingListener::PrintInfo::PAGE_PRINTED;
			info.currentPage = par2;
			m_windowCommander->GetPrintingListener()->OnPrintStatus(m_windowCommander, &info);
		}
# endif // GENERIC_PRINTING
		break;

	case DOC_PRINTING_FINISHED:
# ifdef GENERIC_PRINTING
		if (printer_info)
			printer_info->GetPrintDevice()->GetPrinterController()->PrintDocFinished();
		else if (preview_printer_info)
			preview_printer_info->GetPrintDevice()->GetPrinterController()->PrintDocFinished();
		StopPrinting();
		{
			OpPrintingListener::PrintInfo info;
			info.status = OpPrintingListener::PrintInfo::PRINTING_DONE;
			m_windowCommander->GetPrintingListener()->OnPrintStatus(m_windowCommander, &info);
		}
# else // GENERIC_PRINTING
		{
			OpPrintingListener::PrintInfo info;
			info.status = OpPrintingListener::PrintInfo::PRINTING_DONE;
			m_windowCommander->GetPrintingListener()->OnPrintStatus(m_windowCommander, &info);
		}
# endif // GENERIC_PRINTING
		break;

	case DOC_PRINTING_ABORTED:
# ifdef GENERIC_PRINTING
		if (printer_info)
			printer_info->GetPrintDevice()->GetPrinterController()->PrintDocAborted();
		else if (preview_printer_info)
			preview_printer_info->GetPrintDevice()->GetPrinterController()->PrintDocAborted();
		StopPrinting();
		{
			OpPrintingListener::PrintInfo info; //rg fixme
			info.status = OpPrintingListener::PrintInfo::PRINTING_ABORTED;
			m_windowCommander->GetPrintingListener()->OnPrintStatus(m_windowCommander, &info);
		}
# else // GENERIC_PRINTING
		{
			OpPrintingListener::PrintInfo info; //rg fixme
			info.status = OpPrintingListener::PrintInfo::PRINTING_ABORTED;
			m_windowCommander->GetPrintingListener()->OnPrintStatus(m_windowCommander, &info);
		}
# endif // GENERIC_PRINTING
		break;
#endif // _PRINTING_SUPPORT_

#if !defined MOUSELESS && !defined HAS_NOTEXTSELECTION
	case MSG_SELECTION_SCROLL:
#ifdef DRAG_SUPPORT
		if (!g_drag_manager->IsDragging())
#endif
		{
			if (!selection_scroll_active)
				break;

			const int fudge = 5;
			BOOL button_pressed = FALSE;

			OpInputContext* ic = NULL;
			FramesDocument* doc = GetActiveFrameDoc();

			while(doc)
			{
				VisualDevice* vis_dev = doc->GetVisualDevice();
				CoreView *view = vis_dev->GetView();

				BOOL retry = FALSE;

				if (view)
				{
					OpRect bounding_rect(doc->GetVisualViewport());
					if (!ic && g_input_manager->GetKeyboardInputContext())
					{
						ic = g_input_manager->GetKeyboardInputContext();
						// First try with the focused inputcontext. It may be a scrollable container in page layout.
						if (ic->GetBoundingRect(bounding_rect))
						{
							bounding_rect.x += vis_dev->GetRenderingViewX();
							bounding_rect.y += vis_dev->GetRenderingViewY();
						}
						// Retry, scrolling the document (if not the document).
						retry = ic != vis_dev;
					}
					else
						ic = vis_dev;

					INT32 xpos, ypos;
					if(!view->GetMouseButton(MOUSE_BUTTON_1) &&
					   !view->GetMouseButton(MOUSE_BUTTON_2) &&
					   !view->GetMouseButton(MOUSE_BUTTON_3))
					   break;

					button_pressed = TRUE;

					view->GetMousePos(&xpos, &ypos);

					xpos = vis_dev->GetRenderingViewX() + vis_dev->ScaleToDoc(xpos);
					ypos = vis_dev->GetRenderingViewY() + vis_dev->ScaleToDoc(ypos);

					BOOL handled = FALSE;

					if(xpos < bounding_rect.x + fudge)
					{
						OpInputAction action(OpInputAction::ACTION_SCROLL_LEFT);
						handled |= ic->OnInputAction(&action);
					}
					else if(xpos > bounding_rect.x + bounding_rect.width - fudge)
					{
						OpInputAction action(OpInputAction::ACTION_SCROLL_RIGHT);
						handled |= ic->OnInputAction(&action);
					}

					if(ypos < bounding_rect.y + fudge)
					{
						OpInputAction action(OpInputAction::ACTION_SCROLL_UP);
						handled |= ic->OnInputAction(&action);
					}
					else if(ypos > bounding_rect.y + bounding_rect.height - fudge)
					{
						OpInputAction action(OpInputAction::ACTION_SCROLL_DOWN);
						handled |= ic->OnInputAction(&action);
					}

					if (handled)
						break;
                }

				if (!retry)
					doc = doc->GetParentDoc();
			}

			if (button_pressed)
				//Keep the scroll running...
				if (!GetMessageHandler()->PostDelayedMessage(MSG_SELECTION_SCROLL, 0, 0, 75))
		    		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
 		}
		break;
#endif // !MOUSELESS && !HAS_NOTEXTSELECTION

	case MSG_ES_CLOSE_WINDOW:
		g_main_message_handler->PostMessage(MSG_ES_CLOSE_WINDOW, 0, Id());
		break;

	case MSG_UPDATE_PROGRESS_TEXT:
		{
			OP_STATUS ret = SetProgressText(doc_manager->GetCurrentURL(), TRUE);
			if (ret == OpStatus::ERR_NO_MEMORY)
			{
				RaiseCondition(ret);
			}
		}
		break;
	case MSG_UPDATE_WINDOW_TITLE:
		{
			title_update_title_posted = FALSE;
			OP_STATUS ret = UpdateTitle(FALSE);
			if (ret == OpStatus::ERR_NO_MEMORY)
			{
				RaiseCondition(ret);
			}
		}
		break;
	case WM_OPERA_SCALEDOC:
		{
			//	wParam			= Window to scale.
			//	HIWORD( lParam)	= (BOOL) bIsAbsolute
			//	LOWORD( lParam) = New scale or adjustment (depending on the highword)

			short scale;
			if (HIWORD(par2)) // absolute (=TRUE) or relative (=FALSE)
				scale = LOWORD(par2);
			else
				scale = GetScale() + static_cast<short>(LOWORD(par2));

			ViewportController* controller = GetViewportController();
			controller->GetViewportRequestListener()->
				OnZoomLevelChangeRequest(controller, (MIN((short)DOCHAND_MAX_DOC_ZOOM, scale)) / 100.0
// don't specify a priority rectangle, though the input action might
// have had one:
// TODO: pass the priority rectangle in the message parameter as well.
										 , 0,
// this message is a result of an input action (e.g. CTRL+mouse-wheel
// or definition in input.ini):
										 VIEWPORT_CHANGE_REASON_INPUT_ACTION
					);
		}
		break;

	case MSG_URL_MOVED:
		{
			URL cu = doc_manager->GetCurrentURL();
			if (cu.Id(TRUE) == static_cast<URL_ID>(par2))
			{
				URL moved_to = cu.GetAttribute(URL::KMovedToURL, TRUE);
				if (!moved_to.IsEmpty())
				{
					URLLink *url_ref = OP_NEW(URLLink, (moved_to));
					url_ref->Into(&moved_urls);
				}
			}
		}
		break;

	case MSG_HISTORY_BACK:
		if (HasHistoryPrev())
			SetHistoryPrev(FALSE);
		break;

	case MSG_HISTORY_CLEANUP:
		history_cleanup_message_sent = FALSE;
		EnsureHistoryLimits();
		break;
	}

	if (check_history)
		CheckHistory();
}

BOOL Window::GetLimitParagraphWidth()
{
	return limit_paragraph_width
#ifndef LIMIT_PARA_WIDTH_IGNORE_MODES
		&& !era_mode && (GetLayoutMode() == LAYOUT_NORMAL)
#endif
		;
}

void Window::SetLimitParagraphWidth(BOOL set)
{
	limit_paragraph_width = set;

	if (FramesDocument *frm_doc = doc_manager->GetCurrentDoc())
		frm_doc->MarkContainersDirty();
}

void Window::SetFlexRootMaxWidth(int max_width, BOOL reformat)
{
	if (flex_root_max_width != max_width)
	{
		// max_width == 0 means disable flex-root

		flex_root_max_width = max_width;

		if (doc_manager)
			if (FramesDocument* doc = doc_manager->GetCurrentDoc())
				doc->RecalculateLayoutViewSize(TRUE);

		if (reformat)
			UpdateWindow();
	}
}

void Window::ResumeEcmaScriptIfNotPaused()
{
	if (ecmascript_paused == NOT_PAUSED)
	{
		DocumentTreeIterator iterator(this);
		iterator.SetIncludeThis();

		while (iterator.Next())
		{
			if (ES_ThreadScheduler *scheduler = iterator.GetFramesDocument()->GetESScheduler())
				scheduler->ResumeIfNeeded();
		}
	}
}

void Window::SetEcmaScriptPaused(BOOL value)
{
	if (value)
		ecmascript_paused |= PAUSED_OTHER;
	else
	{
		ecmascript_paused &= ~PAUSED_OTHER;
		ResumeEcmaScriptIfNotPaused();
	}
}

void Window::CancelAllEcmaScriptTimeouts()
{
	DocumentTreeIterator iterator(this);
	iterator.SetIncludeThis();

	while (iterator.Next())
		if (ES_ThreadScheduler *scheduler = iterator.GetFramesDocument()->GetESScheduler())
			scheduler->CancelAllTimeouts(TRUE, TRUE);
}

#if defined GRAB_AND_SCROLL
BOOL Window::GetScrollIsPan() const
{
	return scroll_is_pan_overridden ? scroll_is_pan : g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::ScrollIsPan);
}

void Window::OverrideScrollIsPan()
{
	if (scroll_is_pan_overridden)
		scroll_is_pan = scroll_is_pan ? FALSE : TRUE;
	else
	{
		scroll_is_pan_overridden = TRUE;
		scroll_is_pan = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::ScrollIsPan) ? FALSE : TRUE;
	}
}
#endif // GRAB_AND_SCROLL

#ifdef SAVE_SUPPORT

void Window::GetSuggestedSaveFileName(URL *url, uni_char *szName, int maxSize)
{
}

OP_BOOLEAN Window::SaveAs(URL& url, BOOL load_to_file, BOOL save_inline, BOOL frames_only)
{
	return OpBoolean::IS_FALSE;
}

OP_BOOLEAN Window::SaveAs(URL& url, BOOL load_to_file, BOOL save_inline, const uni_char* szSaveToFolder, BOOL fCreateDirectory, BOOL fExecute, BOOL hasfilename, const uni_char* handlerapplication, BOOL frames_only)
{
	return OpBoolean::IS_FALSE;
}

#endif // SAVE_SUPPORT

#ifdef NEARBY_ELEMENT_DETECTION

void Window::SetElementExpander(ElementExpander* expander)
{
	ElementExpander* old_element_expander = element_expander;

	// Assign expander member variable before deleting old_element_expander because
	// ElementExpander destructor may call WindowCommander::HideFingertouchElements, and then Window::SetElementExpander
	// that can lead to a double free (crash)
	element_expander = expander;

	OP_DELETE(old_element_expander);
}

#endif // NEARBY_ELEMENT_DETECTION


void Window::SetPrivacyMode(BOOL privacy_mode)
{
	if (privacy_mode != m_privacy_mode)
	{
		m_privacy_mode = privacy_mode;

		if (m_privacy_mode)
			g_windowManager->AddWindowToPrivacyModeContext();
		else
		{
			g_windowManager->RemoveWindowFromPrivacyModeContext();

			doc_manager->UnloadCurrentDoc();
			RemoveAllElementsExceptCurrent();
		}
	}
}

#ifdef WEB_TURBO_MODE
void Window::SetTurboMode(BOOL turbo_mode)
{
	if (turbo_mode)
	{
		if (type != WIN_TYPE_NORMAL) // Turbo Mode only available for normal windows.
			return;

#ifdef GADGET_SUPPORT
		if(GetGadget())
			return;
#endif // GADGET_SUPPORT

#ifdef OBML_COMM_FORCE_CONFIG_FILE
		if (g_obml_config->GetExpired()) // Absence of correct and valid config file means no Turbo support
		{
			urlManager->SetWebTurboAvailable(FALSE);
			return;
		}
#endif // OBML_COMM_FORCE_CONFIG_FILE
	}

	if (turbo_mode != m_turbo_mode)
	{
		if (turbo_mode)
		{
			if (OpStatus::IsSuccess(g_windowManager->CheckTuboModeContext()))
				m_turbo_mode = turbo_mode;
		}
		else
			m_turbo_mode = turbo_mode;

		if (turbo_mode == m_turbo_mode)
		{
			if (doc_manager)
				doc_manager->UpdateTurboMode();
		}
	}
}
#endif // WEB_TURBO_MODE

void Window::ScreenPropertiesHaveChanged(){

	//iter all doc-managers, and inform visual devices that
	//the screen properties have changed
	DocumentTreeIterator it(this);
	it.SetIncludeThis();
	while (it.Next())
	{
			if(it.GetDocumentManager()->GetVisualDevice())
			{
				it.GetDocumentManager()->GetVisualDevice()->ScreenPropertiesHaveChanged();
				if (FramesDocument* doc = it.GetFramesDocument())
					doc->RecalculateDeviceMediaQueries();
			}
	}

#ifdef DOM_JIL_API_SUPPORT
	if(m_screen_props_listener)
		m_screen_props_listener->OnScreenPropsChanged();
#endif // DOM_JIL_API_SUPPORT
}

#ifdef GADGET_SUPPORT
void Window::SetGadget(OpGadget* gadget)
{
	m_windowCommander->SetGadget(gadget);

	era_mode = FALSE;
	show_img = TRUE;
	load_img = TRUE;
}

OpGadget* Window::GetGadget() const
{
	return m_windowCommander->GetGadget();
}
#endif // GADGET_SUPPORT

WindowViewMode Window::GetViewMode()
{
#ifdef GADGET_SUPPORT
	if (GetType() == WIN_TYPE_GADGET)
		return 	GetGadget()->GetMode();
#endif // GADGET_SUPPORT
	if (IsThumbnailWindow())
		return WINDOW_VIEW_MODE_MINIMIZED;
	if (IsFullScreen())
		return WINDOW_VIEW_MODE_FULLSCREEN;
	return WINDOW_VIEW_MODE_WINDOWED;
}

#ifdef SCOPE_PROFILER

OP_STATUS
Window::StartProfiling(OpProfilingSession *session)
{
	if (m_profiling_session != NULL)
		return OpStatus::ERR;

	m_profiling_session = session;

	DocumentTreeIterator i(this);
	i.SetIncludeThis();
	i.SetIncludeEmpty();

	while (i.Next())
	{
		DocumentManager *docman = i.GetDocumentManager();

		OP_STATUS status = docman->StartProfiling(session);

		if (OpStatus::IsMemoryError(status))
		{
			StopProfiling();
			m_profiling_session = NULL;
			return status;
		}
	}

	return OpStatus::OK;
}

void
Window::StopProfiling()
{
	if (m_profiling_session == NULL)
		return;

	m_profiling_session = NULL;

	DocumentTreeIterator i(this);
	i.SetIncludeThis();
	i.SetIncludeEmpty();

	while (i.Next())
		i.GetDocumentManager()->StopProfiling();
}

#endif // SCOPE_PROFILER

#ifdef GEOLOCATION_SUPPORT
void
Window::NotifyGeolocationAccess()
{
	DocumentTreeIterator it(DocManager());
	it.SetIncludeThis();
	OpAutoStringHashSet hosts_set;
	OpVector<uni_char> hosts;
	while (it.Next())
	{
		if (FramesDocument* doc = it.GetFramesDocument())
		{
			if (doc->GetGeolocationUseCount() == 0)
				continue;
			const uni_char* hostname = OpSecurityManager::GetHostName(doc->GetURL());
			uni_char* hostname_copy = UniSetNewStr(hostname); // Copy because this might use temporary URL buffers.
			if (!hostname_copy)
				return;
			OP_STATUS status = hosts_set.Add(hostname_copy);
			if (OpStatus::IsError(status))
			{
				OP_DELETEA(hostname_copy);
				if (status != OpStatus::ERR)
					return;
			}
		}
	}
	RETURN_VOID_IF_ERROR(hosts_set.CopyAllToVector(hosts));
	GetWindowCommander()->GetDocumentListener()->OnGeolocationAccess(GetWindowCommander(), static_cast<OpVector<uni_char>*>(&hosts));
}
#endif // GEOLOCATION_SUPPORT

#ifdef MEDIA_CAMERA_SUPPORT
void
Window::NotifyCameraAccess()
{
	DocumentTreeIterator it(DocManager());
	it.SetIncludeThis();
	OpAutoStringHashSet hosts_set;
	OpVector<uni_char> hosts;
	while (it.Next())
	{
		if (FramesDocument* doc = it.GetFramesDocument())
		{
			if (doc->GetCameraUseCount() == 0)
				continue;
			const uni_char* hostname = OpSecurityManager::GetHostName(doc->GetURL());
			uni_char* hostname_copy = UniSetNewStr(hostname); // Copy because this might use temporary URL buffers.
			if (!hostname_copy)
				return;
			OP_STATUS status = hosts_set.Add(hostname_copy);
			if (OpStatus::IsError(status))
			{
				OP_DELETEA(hostname_copy);
				if (status != OpStatus::ERR)
					return;
			}
		}
	}
	RETURN_VOID_IF_ERROR(hosts_set.CopyAllToVector(hosts));

	GetWindowCommander()->GetDocumentListener()->OnCameraAccess(GetWindowCommander(), static_cast<OpVector<uni_char>*>(&hosts));
}
#endif // MEDIA_CAMERA_SUPPORT

#ifdef USE_OP_CLIPBOARD
void Window::OnCopy(OpClipboard* clipboard)
{
	FramesDocument* doc = GetActiveFrameDoc();
	if (doc)
	{
		HTML_Document* html_doc = doc->GetHtmlDocument();
#ifdef DOCUMENT_EDIT_SUPPORT
		OpDocumentEdit* docedit = doc->GetDocumentEdit();
#endif // DOCUMENT_EDIT_SUPPORT
		if (html_doc)
		{
			if (HTML_Element* elm_with_sel = html_doc->GetElementWithSelection())
			{
				if (elm_with_sel->IsFormElement())
				{
					if (FormObject* form_object = elm_with_sel->GetFormObject())
						form_object->GetWidget()->OnCopy(clipboard);
				}
#ifdef SVG_SUPPORT
				else
				{
					g_svg_manager->Copy(elm_with_sel, clipboard);
				}
#endif // SVG_SUPPORT
			}
#ifdef DOCUMENT_EDIT_SUPPORT
			else if (docedit && docedit->IsFocused())
				docedit->OnCopy(clipboard);
#endif // DOCUMENT_EDIT_SUPPORT
			else
			{
#ifndef HAS_NOTEXTSELECTION
				INT32 sel_text_len = html_doc->GetSelectedTextLen();

				if (sel_text_len > 0)
				{
					uni_char* text = OP_NEWA(uni_char, sel_text_len + 1);

					if (text && doc->GetSelectedText(text, sel_text_len + 1))
					{
						clipboard->PlaceText(text, GetUrlContextId());
					}

					OP_DELETEA(text);
					return;
				}
#endif // HAS_NOTEXTSELECTION
#ifdef SVG_SUPPORT
				if (LogicalDocument* logdoc = doc->GetLogicalDocument())
				{
					SVGWorkplace* svg_wp = logdoc->GetSVGWorkplace();
					if (svg_wp->HasSelectedText())
					{
						SelectionBoundaryPoint start, end;
						if (svg_wp->GetSelection(start, end) && start.GetElement())
						{
							TempBuffer buffer;
							if (g_svg_manager->GetTextSelection(start.GetElement()->ParentActual(), buffer))
							{
								clipboard->PlaceText(buffer.GetStorage(), GetUrlContextId());
							}
						}
					}
				}
#endif // SVG_SUPPORT
			}
		}
	}
}

void Window::OnCut(OpClipboard* clipboard)
{
	FramesDocument* doc = GetActiveFrameDoc();
	if (doc)
	{
		HTML_Document* html_doc = doc->GetHtmlDocument();
		if (html_doc)
		{
			if (HTML_Element* elm_with_sel = html_doc->GetElementWithSelection())
			{
				if (elm_with_sel->IsFormElement())
				{
					if (FormObject* form_object = elm_with_sel->GetFormObject())
						form_object->GetWidget()->OnCut(clipboard);
				}
#ifdef SVG_SUPPORT
				else
				{
					g_svg_manager->Cut(elm_with_sel, clipboard);
				}
#endif // SVG_SUPPORT
			}
#ifdef DOCUMENT_EDIT_SUPPORT
			else if (OpDocumentEdit* docedit = doc->GetDocumentEdit())
				docedit->OnCut(clipboard);
#endif // DOCUMENT_EDIT_SUPPORT
		}
	}
}

void Window::OnPaste(OpClipboard* clipboard)
{
	FramesDocument* doc = GetActiveFrameDoc();
	if (doc)
	{
		HTML_Document* html_doc = doc->GetHtmlDocument();
		if (html_doc)
		{
			if (HTML_Element* elm_with_sel = html_doc->GetElementWithSelection())
			{
				if (elm_with_sel->IsFormElement())
				{
					if (FormObject* form_object = elm_with_sel->GetFormObject())
						form_object->GetWidget()->OnPaste(clipboard);
				}
#ifdef SVG_SUPPORT
				else
				{
					g_svg_manager->Paste(elm_with_sel, clipboard);
				}
#endif // SVG_SUPPORT
			}
#ifdef DOCUMENT_EDIT_SUPPORT
			else if (OpDocumentEdit* docedit = doc->GetDocumentEdit())
				docedit->OnPaste(clipboard);
#endif // DOCUMENT_EDIT_SUPPORT
		}
	}
}

void Window::OnEnd()
{
	FramesDocument* doc = GetActiveFrameDoc();
	if (doc)
	{
		HTML_Document* html_doc = doc->GetHtmlDocument();
		if (html_doc)
		{
			if (HTML_Element* elm_with_sel = html_doc->GetElementWithSelection())
			{
				if (elm_with_sel->IsFormElement())
				{
					if (FormObject* form_object = elm_with_sel->GetFormObject())
						form_object->GetWidget()->OnEnd();
				}
#ifdef SVG_SUPPORT
				else
				{
					g_svg_manager->ClipboardOperationEnd(elm_with_sel);
				}
#endif // SVG_SUPPORT
			}
#ifdef DOCUMENT_EDIT_SUPPORT
			else if (OpDocumentEdit* docedit = doc->GetDocumentEdit())
				docedit->OnEnd();
#endif // DOCUMENT_EDIT_SUPPORT
		}
	}
}
#endif // USE_OP_CLIPBOARD

#ifdef KEYBOARD_SELECTION_SUPPORT
void Window::UpdateKeyboardSelectionMode(BOOL enabled)
{
	if (enabled != m_current_keyboard_selection_mode)
	{
		GetWindowCommander()->GetDocumentListener()->OnKeyboardSelectionModeChanged(GetWindowCommander(), enabled);
		m_current_keyboard_selection_mode = enabled;
	}
}
#endif // KEYBOARD_SELECTION_SUPPORT

#ifdef EXTENSION_SUPPORT
OP_STATUS
Window::GetDisallowedScreenshotVisualDevices(FramesDocument* target_document, FramesDocument* source_document, OpVector<VisualDevice>& visdevs_to_hide)
{
	OpGadget* source_gadget = source_document->GetWindow()->GetGadget();
	OP_ASSERT(source_gadget); // only extensions have access to taking a screenshot atm.
	DocumentTreeIterator it(target_document);

	for (BOOL have_more = it.Next(); have_more; have_more = it.Next())
	{
		FramesDocElm* elem = it.GetFramesDocElm();
		VisualDevice* visdev = elem->GetVisualDevice();

		BOOL allowed;
		OpSecurityState state;
		RETURN_IF_ERROR(g_secman_instance->CheckSecurity(OpSecurityManager::DOM_EXTENSION_ALLOW,
		                                                 OpSecurityContext(elem->GetCurrentDoc()),
		                                                 OpSecurityContext(elem->GetCurrentDoc()->GetSecurityContext(), source_gadget),
		                                                 allowed, state));
		OP_ASSERT(!state.suspended);
		if (!allowed)
			RETURN_IF_ERROR(visdevs_to_hide.Add(visdev));
	}
	return OpStatus::OK;
}

OP_STATUS
Window::PerformScreenshot(FramesDocument* target_document, FramesDocument* source_document, OpBitmap*& out_bitmap)
{
	VisualDevice* vis_dev = target_document->GetVisualDevice();
	if (!vis_dev)
		return OpStatus::ERR;

	OpVector<VisualDevice> visdevs_to_hide;
	RETURN_IF_ERROR(GetDisallowedScreenshotVisualDevices(target_document, source_document, visdevs_to_hide));

	OpRect rendering_rect = OpRect(0, 0, vis_dev->GetRenderingViewWidth(), vis_dev->GetRenderingViewHeight());
	OpBitmap* bitmap = NULL;

	RETURN_IF_ERROR(OpBitmap::Create(&bitmap, rendering_rect.width, rendering_rect.height,
	                                 FALSE, FALSE, 0, 0, TRUE));

	OpPainter* painter = bitmap->GetPainter();
	if (painter)
	{
		painter->SetColor(0, 0, 0, 0);
		painter->ClearRect(rendering_rect);
		vis_dev->PaintWithExclusion(painter, rendering_rect, visdevs_to_hide);

		bitmap->ReleasePainter();
		out_bitmap = bitmap;
		return OpStatus::OK;
	}
	else
	{
		OP_DELETE(bitmap);
		return OpStatus::ERR;
	}
}
#endif // EXTENSION_SUPPORT

