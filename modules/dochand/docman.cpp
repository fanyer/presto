/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/dochand/docman.h"

#ifdef CLIENTSIDE_STORAGE_SUPPORT
# include "modules/database/opstorage.h"
#endif //CLIENTSIDE_STORAGE_SUPPORT
#include "modules/display/vis_dev.h"
#include "modules/display/prn_dev.h"
#include "modules/display/coreview/coreview.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/dochand/fdelm.h"
#include "modules/dochand/fraud_check.h"
#include "modules/dochand/viewportcontroller.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/probetools/probepoints.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/hardcore/base/opstatus.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/img/image.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/OpWindow.h"
#include "modules/display/prn_info.h"
#include "modules/viewers/viewers.h"
#include "modules/url/url2.h"
#include "modules/url/url_sn.h"
#include "modules/url/url_man.h"
#include "modules/url/url_tools.h"
#include "modules/url/url_lop_api.h" // OperaURL_Generator
#include "modules/util/handy.h"
#include "modules/util/filefun.h"
#include "modules/util/htmlify.h"
#include "modules/util/winutils.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/logdoc/selection.h"
#include "modules/layout/layout_workplace.h"
#include "modules/layout/traverse/traverse.h"
#include "modules/formats/uri_escape.h"

#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_print.h"

#include "modules/about/operafraudwarning.h"
#ifdef ERROR_PAGE_SUPPORT
#include "modules/about/opcrossnetworkerror.h"
#endif // ERROR_PAGE_SUPPORT
#include "modules/about/opsuppressedurl.h"
#include "modules/about/opclickjackingblock.h"

#ifdef WEBFEEDS_DISPLAY_SUPPORT
# include "modules/webfeeds/webfeeds_api.h"
#endif // WEBFEEDS_DISPLAY_SUPPORT

#ifdef PREFS_DOWNLOAD
# include "modules/prefsloader/prefsloadmanager.h"
#endif // PREFS_DOWNLOAD

#if defined OPERA_CONSOLE && defined ERROR_PAGE_SUPPORT
# include "modules/console/opconsoleengine.h"
#endif // OPERA_CONSOLE && ERROR_PAGE_SUPPORT

#ifdef QUICK
# include "adjunct/quick/hotlist/HotlistManager.h"
# include "adjunct/quick/Application.h"
#endif

#ifdef OPERA_SETUP_DOWNLOAD_APPLY_SUPPORT
# ifdef _KIOSK_MANAGER_
#  include "adjunct/quick/managers/KioskManager.h"
# endif // _KIOSK_MANAGER_
#endif //OPERA_SETUP_DOWNLOAD_APPLY_SUPPORT

#ifdef EXTERNAL_PROTOCOL_HANDLER_SUPPORT
# include "modules/url/loadhandler/url_eph.h"
#endif // EXTERNAL_PROTOCOL_HANDLER_SUPPORT

#ifdef _PLUGIN_SUPPORT_
# include "modules/ns4plugins/opns4pluginhandler.h"
#endif // _PLUGIN_SUPPORT_

#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/esterm.h"
#include "modules/dom/domenvironment.h"

#ifdef _SPAT_NAV_SUPPORT_
# include "modules/spatial_navigation/handler_api.h"
#endif // _SPAT_NAV_SUPPORT_

#ifdef _WML_SUPPORT_
# include "modules/logdoc/wml.h"
#endif // _WML_SUPPORT_



#include "modules/windowcommander/src/WindowCommander.h"

#ifdef WIC_USE_DOWNLOAD_CALLBACK
# include "modules/windowcommander/src/TransferManagerDownload.h"
#endif // WIC_USE_DOWNLOAD_CALLBACK

#ifdef XML_AIT_SUPPORT
# include "modules/windowcommander/AITData.h"
# include "modules/dochand/aitparser.h"
#endif // XML_AIT_SUPPORT

#ifdef _BITTORRENT_SUPPORT_
# include "adjunct/bittorrent/bt-info.h"
#endif	// _BITTORRENT_SUPPORT_

#ifdef M2_SUPPORT
# include "adjunct/m2/src/engine/engine.h"
#endif // M2_SUPPORT

#ifdef WEBSERVER_SUPPORT
#include "modules/about/operauniteadminwarning.h"
#endif // WEBSERVER_SUPPORT

#if defined _SSL_SUPPORT_ && !defined _EXTERNAL_SSL_SUPPORT_ && !defined _CERTICOM_SSL_SUPPORT_
# include "modules/libssl/sslv3.h"
# include "modules/libssl/sslopt.h"
# include "modules/libssl/ssl_api.h"
# include "modules/libssl/certinst_base.h"
# include "modules/windowcommander/src/SSLSecurtityPasswordCallbackImpl.h"
#endif // _SSL_SUPPORT_ && !_EXTERNAL_SSL_SUPPORT_ && !_CERTICOM_SSL_SUPPORT_

#define DOCHAND_THREAD_MIGRATION

#ifdef GADGET_SUPPORT
# include "modules/gadgets/OpGadget.h"
#endif // GADGET_SUPPORT

#ifdef SVG_SUPPORT
# include "modules/svg/SVGManager.h"
# include "modules/svg/svg_image.h"
#endif // SVG_SUPPORT

#ifdef APPLICATION_CACHE_SUPPORT
# include "modules/applicationcache/application_cache_manager.h"
#endif // APPLICATION_CACHE_SUPPORT

#ifdef SCOPE_DOCUMENT_MANAGER
# include "modules/scope/scope_document_listener.h"
#endif // SCOPE_DOCUMENT_MANAGER

#ifdef SCOPE_PROFILER
# include "modules/probetools/probetimeline.h"
#endif // SCOPE_PROFILER

#ifdef WEB_HANDLERS_SUPPORT
# include "modules/forms/tempbuffer8.h"
#endif // WEB_HANDLERS_SUPPORT

#ifdef HC_CAP_ERRENUM_AS_STRINGENUM
#define DH_ERRSTR(p,x) Str::##p##_##x
#else
#define DH_ERRSTR(p,x) x
#endif

class QueuedMessageElm
	: public Link
{
public:
	OpMessage msg;
	URL_ID url_id;
	MH_PARAM_2 user_data;
	IAmLoadingThisURL target_url;  // To prevent the target URL from being reloaded while queued.
};

BOOL DocListElm::MaybeReassignDocOwnership()
{
	if (owns_doc)
	{
		/* The element that owns the document is always first in list, so we
		   only need to search the following part of the list for elements that
		   share document with us. */
		DocListElm* dle = Suc();
		while (dle)
		{
			if (dle->doc == doc)
			{
				doc = NULL;
				owns_doc = FALSE;

				dle->owns_doc = TRUE;
				return TRUE;
			}

			dle = dle->Suc();
		}
	}

	return FALSE;
}

int
DocumentManager::GetNextDocListElmId()
{
	if (!GetParentDoc())
	{
		return GetWindow()->GetNextDocListElmId();
	}
	else
	{
		return HistoryIdNotSet;
	}
}

DocListElm::DocListElm(const URL& doc_url, FramesDocument* d, BOOL own, int n, UINT32 id)
	: m_id(id)
{
	url = doc_url;
	referrer_url = DocumentReferrer();
	send_to_server = TRUE;
	doc = d;
	owns_doc = own;
	replaceable = FALSE;
	precreated = FALSE;
	has_visual_viewport = FALSE;
#ifdef PAGED_MEDIA_SUPPORT
	current_page = -1;
#endif
	number = n;
	last_layout_mode = doc ? doc->GetLayoutMode() : LAYOUT_NORMAL;
	last_scale = doc ? doc->GetWindow()->GetScale() : 100;

	script_generated_doc = FALSE;
	replaced_empty_position = FALSE;

#ifdef _WML_SUPPORT_
	wml_context = NULL;
#endif // _WML_SUPPORT_

	m_data = NULL;

	m_user_data = NULL;
}

DocListElm::~DocListElm()
{
	if (owns_doc)
		OP_DELETE(doc);

#ifdef _WML_SUPPORT_
	if (wml_context)
	{
		wml_context->DecRef();
		wml_context = NULL;
	}
#endif // _WML_SUPPORT_

	OP_DELETE(m_data);

	OP_DELETE(m_user_data);
}

OP_STATUS DocListElm::SetTitle(const uni_char* val)
{
	if (title.Compare(val) != 0)
		return title.Set(val);

	return OpStatus::OK;
}

void DocListElm::SetUserData(OpHistoryUserData* user_data)
{
	OP_DELETE(m_user_data);
	m_user_data = user_data;
}

OpHistoryUserData* DocListElm::GetUserData() const
{
	return m_user_data;
}

void DocListElm::Out()
{
	MaybeReassignDocOwnership();
	Link::Out();
}

void DocListElm::ReplaceDoc(FramesDocument* d)
{
	if (owns_doc)
	{
		MaybeReassignDocOwnership();
		OP_DELETE(doc);
	}

	doc = d;
}

BOOL DocListElm::HasPreceding()
{
	DocListElm *preceding = Pred();

	while (preceding)
		if (preceding->number != number)
			return TRUE;
		else
			preceding = preceding->Pred();

	return FALSE;
}

#ifdef _WML_SUPPORT_
void
DocListElm::SetWmlContext(WML_Context *new_ctx)
{
	if (wml_context != new_ctx)
	{
		if (wml_context)
			wml_context->DecRef();
		wml_context = new_ctx;
		if (wml_context)
			wml_context->IncRef();
	}
}
#endif // _WML_SUPPORT_

void
DocListElm::SetData(ES_PersistentValue *data)
{
	OP_DELETE(m_data);
	m_data = data;
}

BOOL IsAboutBlankURL(const URL& url)
{
	if (url.Type() == URL_OPERA)
	{
		OpStringC8 name(url.GetAttribute(URL::KName));
		if (name.CompareI("about:blank") == 0)
			return TRUE;
#ifdef SELFTEST
		if (name.CompareI("opera:blanker") == 0)
			return TRUE;
#endif // SELFTEST
	}
	return FALSE;
}

BOOL IsURLSuitableSecurityContext(URL url)
{
	if (url.GetAttribute(URL::KUniHostName).HasContent())
		return TRUE;
	if (url.Type() == URL_OPERA && !IsAboutBlankURL(url))
		return TRUE;
	return FALSE;
}

void
DocumentManager::NotifyUrlChangedIfAppropriate(URL& url)
{
	// Did this to get the URL in the URL-field early for empty windows
	if (!current_doc_elm && doc_list.Empty() && !parent_doc)
	{
		uni_char *tempname = Window::ComposeLinkInformation(url.GetAttribute(URL::KUniName_Username_Password_Hidden).CStr(), url.GetAttribute(URL::KUniFragment_Name).CStr());
		WindowCommander *win_com = window->GetWindowCommander();

		win_com->GetLoadingListener()->OnUrlChanged(win_com, tempname);
		OP_DELETEA(tempname);
	}
}

void
DocumentManager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	BOOL oom = FALSE;

	FramesDocument* frames_doc = NULL;

#ifdef _PRINT_SUPPORT_
	if (window->GetPreviewMode() || window->GetPrintMode())
		frames_doc = GetPrintDoc();
	else
#endif
		frames_doc = GetCurrentDoc();

#ifdef LOCALE_CONTEXTS
	if (frames_doc)
		g_languageManager->SetContext(frames_doc->GetURL().GetContextId());
#endif

	switch (msg)
	{
#ifdef COOKIE_WARNING_SUPPORT
	case MSG_COOKIE_SET:
	{
		WindowCommander *wc = window->GetWindowCommander();
		wc->GetDocumentListener()->OnCookieWriteDenied(wc);
		break;
	}
#endif // COOKIE_WARNING_SUPPORT
	case MSG_HANDLE_BY_OPERA:
		// par1: ignored
		// par2: arguments to HandleByOpera (check_if_inline_expired | use_plugin << 1 | user_requested << 2)
		g_windowManager->UpdateVisitedLinks(GetCurrentURL(), HandleByOpera((par2 & 1) != 0, (par2 & 2) != 0, (par2 & 4) != 0, &oom) ? window : NULL);
		break;

	case MSG_REFLOW_DOCUMENT:
		// par1: ignored
		// par2: arguments to ClearReflowMsgFlag and Reflow (frameset)
		if (frames_doc)
		{
			frames_doc->ClearReflowMsgFlag(!!par2);

			LayoutWorkplace* wp = frames_doc->GetLogicalDocument() ? frames_doc->GetLogicalDocument()->GetLayoutWorkplace() : NULL;

			if (wp)
				wp->SetCanYield(TRUE);

			OP_STATUS status = frames_doc->Reflow(FALSE, FALSE, !!par2);
			if (status == OpStatus::ERR_YIELD)
				mh->PostMessage(MSG_REFLOW_DOCUMENT, 0, 0);
			else
				RaiseCondition(status);

			if (wp)
				wp->SetCanYield(FALSE);
		}
		break;

	case MSG_WAIT_FOR_STYLES_TIMEOUT:
		if (frames_doc)
			RaiseCondition(frames_doc->WaitForStylesTimedOut());
		break;

	case MSG_INLINE_REPLACE:
		UrlImageContentProvider::ResetAndRestartImageFromID(par1);
		break;

	case MSG_URL_LOADING_FAILED:
	case MSG_HEADER_LOADED:
	case MSG_URL_DATA_LOADED:
	case MSG_NOT_MODIFIED:
	case MSG_ALL_LOADED:
	case MSG_MULTIPART_RELOAD:
		RaiseCondition(HandleLoading(msg, (URL_ID) par1, par2));

		if (msg == MSG_URL_DATA_LOADED)
			window->HandleDocumentProgress(0);
		break;

	case MSG_URL_MOVED:
	{
		URL moved_to = current_url_reserved->GetAttribute(URL::KMovedToURL, FALSE);
		if (current_url_reserved->Id() ==  static_cast<URL_ID>(par1) && !moved_to.IsEmpty())
		{
			OP_ASSERT(moved_to.Id() == static_cast<URL_ID>(par2));
			// Unlock previous url and lock the new url
			current_url_reserved.SetURL(moved_to);

			if (security_check_callback && !queue_messages)
			{
				OP_ASSERT(!security_check_callback->m_suspended || security_check_callback->m_done);

				SecCheckRes res = InitiateSecurityCheck(moved_to, referrer_url);
				if (res == SEC_CHECK_STARTED)
				{
					// Enables queuing. This message is added first on the queue and reprocessed or dropped depending on the outcome.
					security_check_callback->PrepareForSuspendingRedirect();
				}
				else if (res != SEC_CHECK_ALLOWED)
					break;
			}

			RaiseCondition(HandleLoading(msg, (URL_ID) par1, par2));
		}

		break;
	}
	case MSG_URL_LOAD_NOW:
		// par1: url id
		// par2: user_initiated
		LoadPendingUrl((URL_ID) par1, !!par2);
		break;

#ifdef SHORTCUT_ICON_SUPPORT
	case MSG_FAVICON_TIMEOUT:
		// par1: url id
		if (frames_doc)
			frames_doc->FaviconTimedOut(par1);
		break;
#endif // SHORTCUT_ICON_SUPPORT
	case MSG_START_REFRESH_LOADING:
		Refresh(static_cast<URL_ID>(par1));
		break;

	case MSG_DOC_DELAYED_ACTION:
		OnDelayedActionMessageReceived();
		break;

	case MSG_HISTORY_CLEANUP:
		HistoryCleanup();
		break;

	case MSG_DOCHAND_PROCESS_QUEUED_MESSAGE:
	{
		QueuedMessageElm *elm = (QueuedMessageElm *) queued_messages.First();

		if (security_check_callback && elm->msg == MSG_URL_MOVED)
		{
			if (!security_check_callback->m_suspended)
			{
				// A queued redirect that hasn't yet been security checked.

				SecCheckRes res = InitiateSecurityCheck(elm->target_url, referrer_url);
				if (res == SEC_CHECK_STARTED)
				{
					// Async check. The message is left at the head of the queue.
					// It will be reprocessed or dropped depending on the outcome.
					security_check_callback->PrepareForSuspendingRedirect();
				}

				if (res != SEC_CHECK_ALLOWED)
					break;
			}
			OP_ASSERT(!security_check_callback->m_suspended || security_check_callback->m_allowed);
			security_check_callback->Reset();
		}

		// We don't want our call to HandleLoading here to queue the message, do we?
		queue_messages = FALSE;

		if (elm)
		{
			RaiseCondition(HandleLoading(elm->msg, elm->url_id, elm->user_data));

			elm->Out();
			OP_DELETE(elm);
		}

		if (queued_messages.Empty())
		{
			if (GetLoadStatus() == NOT_LOADING)
				StopLoading(FALSE);
		}
		else
		{
			queue_messages = TRUE;
			mh->PostMessage(MSG_DOCHAND_PROCESS_QUEUED_MESSAGE, 0, 0);
		}
		break;
	}
#ifdef _WML_SUPPORT_
	case MSG_WML_REFRESH:
		oom = OpStatus::IsMemoryError(UpdateCurrentDoc(FALSE, FALSE, !!par2));
		break;
#endif // _WML_SUPPORT_

#ifdef TRUST_RATING
	case MSG_TRUST_CHECK_DONE:
		{
			ServerTrustChecker* checker = (ServerTrustChecker*)m_active_trust_checks.First();
			while (checker)
			{
				ServerTrustChecker* next_checker = (ServerTrustChecker*)checker->Suc();
				if (checker->GetId() == (UINT)par1)
				{
					checker->Out();
					OP_DELETE(checker);
				}
				checker = next_checker;
			}
		}
		break;

#endif // TRUST_RATING

#ifdef WEBFEEDS_DISPLAY_SUPPORT
	case MSG_DISPLAY_WEBFEED:
		{
			URL feed_output;
			URL ref_url;
			OpFeed* feed = g_webfeeds_api->GetFeedById(par1);

			if (feed)
			{
				OP_STATUS status = feed->WriteFeed(feed_output, TRUE);
				if (OpStatus::IsSuccess(status))
				{
					OpenURLOptions options;
					options.entered_by_user = WasEnteredByUser;
					options.es_terminated = TRUE; // Why? To make it faster?
					OpenURL(feed_output, DocumentReferrer(ref_url), FALSE/*check_if_expired*/, FALSE/*reload*/, options);
				}
				else if (OpStatus::IsMemoryError(status))
					oom = TRUE;
			}

			break;
		}
#endif // WEBFEEDS_DISPLAY_SUPPORT

#if defined _SSL_SUPPORT_ && !defined _EXTERNAL_SSL_SUPPORT_ && !defined _CERTICOM_SSL_SUPPORT_
	case MSG_DOCHAND_CERTIFICATE_INSTALL:
		if (ssl_certificate_installer)
		{
			/* This is the "finished message," so it'd better be finished. */
			OP_ASSERT(ssl_certificate_installer->Finished());

			if (!ssl_certificate_installer->InstallSuccess())
			{
				queue_messages = FALSE;
				StopLoading(FALSE);
				queued_messages.Clear();
			}
			else if (!queued_messages.Empty())
				mh->PostMessage(MSG_DOCHAND_PROCESS_QUEUED_MESSAGE, 0, 0);
			else
			{
				queue_messages = FALSE;
				StopLoading(FALSE);
			}
			OP_DELETE(ssl_certificate_installer);
			ssl_certificate_installer = NULL;
		}
		break;
#endif // _SSL_SUPPORT_ && !_EXTERNAL_SSL_SUPPORT_ && !_CERTICOM_SSL_SUPPORT_

	default:
		OP_ASSERT(FALSE); // Huh?
	}

	if (oom)
		RaiseCondition(OpStatus::ERR_NO_MEMORY);
}

void
DocumentManager::SetLoadStat(DM_LoadStat value)
{
	if (load_stat == NOT_LOADING && value != NOT_LOADING)
	{
		current_url_used.SetURL(current_url);

#ifdef SHORTCUT_ICON_SUPPORT
		if (!GetFrame())
			/* This call moved from Window::StartProgressDisplay() to here.  We
			   call StartProgressDisplay() when we start loading any document in
			   the window as well as when we start loading some unrelated inline
			   in any document in the window.  This only happens when we start
			   loading a document in the top frame. */
			OpStatus::Ignore(window->SetWindowIcon(NULL));
#endif // SHORTCUT_ICON_SUPPORT

		if (vis_dev)
			vis_dev->StartLoading();

		activity_loading.Begin();
	}
	else if (load_stat != NOT_LOADING && value == NOT_LOADING)
	{
		current_url_used.UnsetURL();
		current_url_reserved.UnsetURL();

		user_initiated_loading = FALSE;

		activity_loading.Cancel();

		if (history_state_doc_elm)
		{
			if (history_state_doc_elm == current_doc_elm)
			{
				SetCurrentURL(history_state_doc_elm->GetUrl(), FALSE);
				history_state_doc_elm->Doc()->ClearAuxUrlToHandleLoadingMessagesFor();
				current_url_is_inline_feed = FALSE;
			}
			history_state_doc_elm = NULL;
		}
#ifdef WEB_HANDLERS_SUPPORT
		if (action_locked)
		{
			action_locked = FALSE;
			SetAction(VIEWER_NOT_DEFINED);
		}
#endif // WEB_HANDLERS_SUPPORT
	}

	if (load_stat != DOC_CREATED && value == DOC_CREATED)
	{
		FramesDocElm* fdelm = frame;
		while (fdelm)
		{
			fdelm->SetOnLoadCalled(FALSE);
			fdelm->SetOnLoadReady(FALSE);
			fdelm = fdelm->Parent();
		}

		if (vis_dev)
			vis_dev->DocCreated();
	}

	load_stat = value;
}

int
DocumentManager::GetNextHistoryNumber(BOOL is_replace, BOOL &replaced_empty)
{
	BOOL do_replace = is_replace;

	replaced_empty = FALSE;

	if (!do_replace && current_doc_elm)
	{
		if (!current_doc_elm->HasPreceding() && !current_doc_elm->IsScriptGeneratedDocument())
		{
			URL url = current_doc_elm->GetUrl();
			if (url.Type() == URL_NULL_TYPE || url.Type() == URL_JAVASCRIPT || url.GetAttribute(URL::KName).Compare("about:blank") == 0)
				do_replace = TRUE;
		}

		if (!do_replace && current_doc_elm->ReplacedEmptyPosition())
		{
			if (request_thread_info.type == ES_THREAD_INLINE_SCRIPT)
				do_replace = TRUE;
			else if (request_thread_info.type == ES_THREAD_EVENT && request_thread_info.data.event.is_window_event &&
			         (request_thread_info.data.event.type == ONLOAD || request_thread_info.data.event.type == DOMCONTENTLOADED))
				do_replace = TRUE;
		}

		replaced_empty = do_replace;
	}

	int next_history_number;

	if (use_history_number != -1)
		next_history_number = use_history_number;
	else if (current_doc_elm && do_replace)
		next_history_number = current_doc_elm->Number();
	else
		next_history_number = window->SetNewHistoryNumber();

	use_history_number = -1;
	return next_history_number;
}

void
DocumentManager::InsertHistoryElement(DocListElm *element)
{
	/* This is not supported! */
	OP_ASSERT(element->Doc());

	DocListElm *existing;

	for (existing = FirstDocListElm(); existing; existing = existing->Suc())
		if (existing->Number() > element->Number())
		{
			element->Precede(existing);
			break;
		}

	if (!existing)
		element->Into(&doc_list);

	if (frame && element == doc_list.First())
	{
		/* When we're inserting the first element into the history in a frame or
		   iframe, it should always get the history number of the parent
		   document, since otherwise this frame would be empty at history
		   positions from the parent document's to this element's. */
		DocListElm *parent = parent_doc->GetDocManager()->current_doc_elm;
		while (parent->Pred() && parent->Pred()->Doc() == parent->Doc())
			parent = parent->Pred();
		element->SetNumber(parent->Number());
	}

	if (!has_posted_history_cleanup)
		if (DocListElm *previous = element->Pred())
			if (previous->Number() == element->Number())
			{
				has_posted_history_cleanup = TRUE;
				mh->PostMessage(MSG_HISTORY_CLEANUP, 0, 0);
			}
}

static const OpMessage docman_messages[] =
{
	MSG_HANDLE_BY_OPERA,
	MSG_REFLOW_DOCUMENT,
	MSG_WAIT_FOR_STYLES_TIMEOUT,
	MSG_INLINE_REPLACE,
	MSG_URL_LOADING_FAILED,
	MSG_HEADER_LOADED,
	MSG_URL_DATA_LOADED,
	MSG_ALL_LOADED,
	MSG_NOT_MODIFIED,
	MSG_MULTIPART_RELOAD,
	MSG_URL_LOAD_NOW,
	MSG_URL_MOVED,
#ifdef TRUST_RATING
	MSG_TRUST_CHECK_DONE,
#endif // TRUST_RATING
#ifdef WEBFEEDS_DISPLAY_SUPPORT
	MSG_DISPLAY_WEBFEED,
#endif // WEBFEEDS_DISPLAY_SUPPORT
#ifdef SHORTCUT_ICON_SUPPORT
	MSG_FAVICON_TIMEOUT,
#endif // SHORTCUT_ICON_SUPPORT
	MSG_START_REFRESH_LOADING,
	MSG_DOC_DELAYED_ACTION,
	MSG_HISTORY_CLEANUP,
#ifdef _WML_SUPPORT_
	MSG_WML_REFRESH,
#endif // _WML_SUPPORT_
#if defined _SSL_SUPPORT_ && !defined _EXTERNAL_SSL_SUPPORT_ && !defined _CERTICOM_SSL_SUPPORT_
	MSG_DOCHAND_CERTIFICATE_INSTALL,
#endif // _SSL_SUPPORT_ && !_EXTERNAL_SSL_SUPPORT_ && !_CERTICOM_SSL_SUPPORT_
#ifdef COOKIE_WARNING_SUPPORT
	MSG_COOKIE_SET,
#endif // COOKIE_WARNING_SUPPORT
	MSG_DOCHAND_PROCESS_QUEUED_MESSAGE
};

OP_STATUS
DocumentManager::UpdateCallbacks(BOOL is_active)
{
	if (!is_active)
	{
		if (has_set_callbacks)
		{
			has_set_callbacks = FALSE;
			mh->UnsetCallBacks(this);
			return mh->SetCallBack(this, MSG_HISTORY_CLEANUP, 0);
		}
	}
	else if (!has_set_callbacks)
	{
		RETURN_IF_ERROR(mh->SetCallBackList(this, 0, docman_messages, sizeof docman_messages / sizeof docman_messages[0]));
		has_set_callbacks = TRUE;
	}
	return OpStatus::OK;
}

void
DocumentManager::HistoryCleanup()
{
	if (has_posted_history_cleanup)
	{
		BOOL try_again_later = FALSE;

		for (DocListElm *element = FirstDocListElm(); element; element = element->Suc())
			if (DocListElm *previous = element->Pred())
				if (previous->Number() == element->Number())
				{
					// Same number -> unreachable in history since it was
					// replaced by another document at the same history position.
					if (previous->Doc()->IsESGeneratingDocument())
					{
						// Quite likely the generated document wants to access the
						// dom environment of this document so we keep it around even
						// though it's not reachable in history
						continue;
					}

					if (previous->Doc()->IsESActive(FALSE))
					{
						// Running scripts right now so can't really
						// delete it. A thing of the past?
						try_again_later = TRUE;
						continue;
					}

					if (previous == history_state_doc_elm)
					{
						history_state_doc_elm = NULL;
					}

					previous->Out();
					OP_DELETE(previous);
				}

		if (try_again_later)
			mh->PostMessage(MSG_HISTORY_CLEANUP, 0, 0);
		else
			has_posted_history_cleanup = FALSE;
	}
}

void
DocumentManager::DropCurrentLoadingOrigin()
{
	if (current_loading_origin)
	{
		current_loading_origin->DecRef();
		current_loading_origin = NULL;
	}
}

DocumentOrigin*
DocumentManager::CreateOriginFromLoadingState()
{
	DocumentOrigin* origin;
	if (current_loading_origin)
	{
		origin = current_loading_origin;
		current_loading_origin = NULL;
		// Not calling IncRef and DecRef since the number of references stay the same.
	}
	else if (!IsURLSuitableSecurityContext(current_url) && (referrer_url.origin || IsURLSuitableSecurityContext(referrer_url.url)))
	{
		if (referrer_url.origin)
		{
			origin = referrer_url.origin;
			origin->IncRef();
		}
		else
			origin = DocumentOrigin::Make(referrer_url.url);
	}
	else
		origin = DocumentOrigin::Make(current_url);

	return origin;
}

void
DocumentManager::SetCurrentURL(const URL &url, BOOL moved)
{
	// Keep the origin if this is a redirect to something with no
	// intrinsic domain.
	if (!moved || IsURLSuitableSecurityContext(url))
		DropCurrentLoadingOrigin();

	current_url = url;

	if (load_stat != NOT_LOADING)
	{
		URL_InUse temporary_use(current_url);
		current_url_used.SetURL(current_url);
	}
}

URL
DocumentManager::GetCurrentDocURL()
{
	return current_doc_elm ? current_doc_elm->GetUrl() : URL();
}

DocumentReferrer DocumentManager::GenerateReferrerURL()
{
	if (!IsURLSuitableSecurityContext(current_url))
	{
		DocumentReferrer referrer = current_doc_elm ? current_doc_elm->GetReferrerUrl() : referrer_url;
		if (!referrer.IsEmpty())
			return referrer;
	}
	return DocumentReferrer(current_url);
}

DocumentManager::DocumentManager(Window* win, FramesDocElm* frm, FramesDocument* pdoc)
  : window(win),
	mh(NULL),
	frame(frm),
	parent_doc(pdoc),
	vis_dev(NULL),
	current_doc_elm(NULL),
	history_len(0),
	queue_messages(FALSE),
	load_stat(NOT_LOADING),
	current_url_is_inline_feed(FALSE),
	current_loading_origin(NULL),
	send_to_server(TRUE),
	current_action(VIEWER_NOT_DEFINED),
	current_application(NULL),
	url_replace_on_command(FALSE),
	user_auto_reload(FALSE),
	reload(FALSE),
	was_reloaded(FALSE),
	were_inlines_reloaded(FALSE),
	were_inlines_requested_conditionally(TRUE),
	redirect(FALSE),
	replace(FALSE),
	scroll_to_fragment_in_document_finished(FALSE),
	load_image_only(FALSE),
	save_image_only(FALSE),
	is_handling_message(FALSE),
	is_clearing(FALSE),
	has_posted_history_cleanup(FALSE),
	has_set_callbacks(FALSE),
	error_page_shown(FALSE),
	use_history_number(-1),
	use_current_doc(FALSE),
	reload_document(FALSE),
	conditionally_request_document(TRUE),
	reload_inlines(FALSE),
	conditionally_request_inlines(TRUE),
	check_expiry(CHECK_EXPIRY_NEVER),
	history_walk(DM_NOT_HIST),
	pending_refresh_id(0),
	waiting_for_refresh_id(0),
	start_navigation_time_ms(0.0),
	restart_parsing(FALSE),
	pending_viewport_restore(FALSE),
#ifdef _PRINT_SUPPORT_
	print_doc(NULL),
	print_preview_doc(NULL),
	print_vd(NULL),
	print_preview_vd(NULL),
#endif // _PRINT_SUPPORT_
	pending_url_user_initiated(FALSE),
	user_started_loading(FALSE),
	user_initiated_loading(FALSE),
	check_url_access(TRUE),
#ifdef _WML_SUPPORT_
	wml_context(NULL),
#endif // _WML_SUPPORT_
	dom_environment(NULL),
	es_pending_unload(FALSE),
	es_terminating_action(NULL),
	request_thread_info(ES_THREAD_EMPTY),
	history_state_doc_elm(NULL),
	history_traverse_pending_delta(0),
	m_waiting_for_online_url(NULL),
#if defined _SSL_SUPPORT_ && !defined _EXTERNAL_SSL_SUPPORT_ && !defined _CERTICOM_SSL_SUPPORT_
	ssl_certificate_installer(NULL),
#endif // _SSL_SUPPORT_ && !_EXTERNAL_SSL_SUPPORT_ && !_CERTICOM_SSL_SUPPORT_
	is_delayed_action_msg_posted(FALSE),
#ifdef TRUST_RATING
	m_next_checker_id(1),
#endif // TRUST_RATING
#ifdef CLIENTSIDE_STORAGE_SUPPORT
	m_storage_manager(NULL),
#endif // CLIENTSIDE_STORAGE_SUPPORT
	activity_loading(ACTIVITY_DOCMAN),
	activity_refresh(ACTIVITY_METAREFRESH),
#ifdef SCOPE_PROFILER
	m_timeline(NULL),
#endif // SCOPE_PROFILER
#ifdef WEB_HANDLERS_SUPPORT
	action_locked(FALSE),
	skip_protocol_handler_check(FALSE),
#endif // WEB_HANDLERS_SUPPORT
	waiting_for_document_ready(FALSE),
	security_check_callback(NULL)
#ifdef DOM_LOAD_TV_APP
	, whitelist(NULL)
#endif
{
	OP_ASSERT(win);
}

DocumentManager::~DocumentManager()
{
	Clear();

#ifdef _WML_SUPPORT_
	if (wml_context)
	{
		wml_context->DecRef();
		wml_context = NULL;
	}
#endif // _WML_SUPPORT_

	DOM_ProxyEnvironment::Destroy(dom_environment);

#ifdef TRUST_RATING
	m_active_trust_checks.Clear();
#endif // TRUST_RATING

#ifdef CLIENTSIDE_STORAGE_SUPPORT
	if (m_storage_manager)
	{
		m_storage_manager->Release();
		m_storage_manager = NULL;
	}
#endif //CLIENTSIDE_STORAGE_SUPPORT

	DropCurrentLoadingOrigin();

#ifdef WIC_USE_DOWNLOAD_CALLBACK
	while (current_download_vector.GetCount())
	{
		current_download_vector.Get(0)->ReleaseDocument();
	}
#endif

	if (mh)
	{
		mh->UnsetCallBacks(this);
		OP_DELETE(mh);
	}

	if (security_check_callback)
	{
		if (!security_check_callback->m_suspended || security_check_callback->m_done)
			OP_DELETE(security_check_callback);
		else
			security_check_callback->Invalidate();
	}

	queued_messages.Clear();
}

OP_STATUS DocumentManager::Construct()
{
	OP_ASSERT(!mh);
	mh = OP_NEW(MessageHandler, (window, this));
	if (!mh)
		return OpStatus::ERR_NO_MEMORY;

	return UpdateCallbacks(TRUE);
}

int DocumentManager::GetSubWinId()
{
	return frame ? frame->GetSubWinId() : -1;
}

void DocumentManager::RemoveFromHistory(int from, BOOL unlink)
{
	/* Our current history position's number and the window's idea of what the
	   current history number ought to be seem to be in conflict if this test
	   fails, and the window is trying to remove the current history position.
	   This is really bad and should be investigated immediately. */
	OP_ASSERT(!current_doc_elm || current_doc_elm->Number() < from);

	while (DocListElm *dle = (DocListElm *) doc_list.Last())
		if (dle->HasPreceding() && dle->Number() >= from)
			/* This history position begins after the point we're removing,
			   so we should remove it. */
			if (!dle->GetOwnsDoc() || dle->Doc()->Free(FALSE, FramesDocument::FREE_VERY_IMPORTANT))
				RemoveElementFromHistory(dle, unlink);
			else
				/* The document couldn't be freed, usually because there is some
				   script currently running in it, affecting the next history
				   position, which typically will be the current one. */
				break;
		else
		{
			/* DocumentManager::InsertHistoryElement() (and anyone else
			   manipulating the history list) should make sure this is true,
			   that is, that child history lists start at the same history
			   number as their parent document. */
			OP_ASSERT(dle->Number() < from);
			dle->Doc()->RemoveFromHistory(from);
			return;
		}
}

void DocumentManager::RemoveUptoHistory(int to, BOOL unlink)
{
	/* Our current history position's number and the window's idea of what the
	   current history number ought to be seem to be in conflict if this test
	   fails.  This is really bad and should be investigated immediately. */
	OP_ASSERT(!current_doc_elm || current_doc_elm->Number() >= to || !current_doc_elm->Suc());

	while (DocListElm *dle = (DocListElm *) doc_list.First())
		if (dle->Suc() && dle->Suc()->Number() <= to)
			/* If there is a later history position that doesn't begin beyond
			   the point we're removing, we can safely remove this one. */
			if (dle->MaybeReassignDocOwnership() || dle->Doc()->Free(FALSE, FramesDocument::FREE_VERY_IMPORTANT))
				RemoveElementFromHistory(dle, unlink);
			else
				/* The document couldn't be freed, usually because there is some
				   script currently running in it, affecting the next history
				   position, which typically will be the current one. */
				break;
		else
		{
			/* If there's a group of elements that have the same number, all but
			   the last are dead and about to be removed by a call to
			   DocumentManager::HistoryCleanup(), but only if they continue to
			   have a number equal to the others.  Since we will reset the
			   "real" element's number to 'to', we also want to reset all the
			   dead ones' numbers to 'to'.  And we also want to skip to the last
			   one, since calling Doc()->RemoveUptoHistory() is only relevant on
			   that one (but is really important, on the other hand.) */
			while (dle->Suc() && dle->Suc()->Number() == dle->Number())
			{
				dle->SetNumber(to);
				dle = dle->Suc();
			}

			/* Otherwise, update this history position's number to the point of
			   removal (it'll be the first history number after the operation,
			   and if this document manager was at all affected, which it
			   obviously was since we got called, its first history position
			   should now start at the globally first history number.) */
			dle->SetNumber(to);
			dle->Doc()->RemoveUptoHistory(to);
			return;
		}
}

BOOL HistoryNumberExists(FramesDocument *top_doc, int number, BOOL forward)
{
	DocListElm *tmp_elm = top_doc->GetDocManager()->CurrentDocListElm();
	while (tmp_elm && tmp_elm->Number() != number && NULL == tmp_elm->Doc()->GetHistoryElmAt(number, forward))
		tmp_elm = forward ? tmp_elm->Suc() : tmp_elm->Pred();

	return tmp_elm != NULL;
}

void DocumentManager::RemoveElementFromHistory(int pos, BOOL unlink)
{
	DocListElm* dle = LastDocListElm();

	while (dle && dle->Number() > pos)
		dle = dle->Pred();

	if (!dle || dle->Number() != pos)
		return;

	RemoveElementFromHistory(dle, unlink);
}

void DocumentManager::RemoveElementFromHistory(DocListElm *dle, BOOL unlink, BOOL update_current_history_pos)
{
	DocListElm* prev = dle->Pred();

	history_len--;

	// set the new current_history_number
	if (update_current_history_pos && dle->Number() == window->GetCurrentHistoryPos())
		window->SetCurrentHistoryPosOnly(prev ? prev->Number() : 0);

	FramesDocument *top_doc = window->GetCurrentDoc();
	if (top_doc)
	{
		// set the new history_max when removing last history element
		if (!dle->Suc() && dle->Number() == window->GetHistoryMax())
		{
			int candidate_max = dle->Number() - 1;

			while (candidate_max > 0 && !HistoryNumberExists(top_doc, candidate_max, FALSE))
				candidate_max--;

			window->SetMaxHistoryPos(candidate_max);
		}

		// set the new history_min when removing first history element, unless it was also the last
		if (!dle->Pred() && dle->Number() == window->GetHistoryMin() && dle->Number() < window->GetHistoryMax())
		{
			int candidate_min = dle->Number() + 1;

			while (candidate_min < window->GetHistoryMax() && !HistoryNumberExists(top_doc, candidate_min, TRUE))
				candidate_min++;

			window->SetMinHistoryPos(candidate_min);
		}
	}

	if (current_doc_elm == dle)
	{
		// embrowser specific: clear current document if removed from history
		if (dle->Doc())
		{
			dle->Doc()->Undisplay();
		}

		current_doc_elm = NULL;
	}

	if (history_state_doc_elm == dle)
	{
		history_state_doc_elm = NULL;
	}

	dle->Out();

	if (unlink)
		/* DocListElm::ReplaceDoc() will have set 'dle->doc' to NULL if other
		   history positions shared document with 'dle'.  In that case trying to
		   put 'dle' back into the history later will fail silently. */
		dle->Into(&unlinked_doc_list);
	else
		OP_DELETE(dle);
}

#ifdef DOCHAND_HISTORY_MANIPULATION_SUPPORT

void DocumentManager::InsertElementInHistory(int pos, DocListElm * doc_elm)
{
	BOOL raise_OOM_condition = FALSE;
	if( doc_elm )
	{
		FramesDocument * doc = doc_elm->Doc();
		if (doc == doc->GetTopDocument())
			window->SetType(doc->Type());

		if (!doc)
			/* Documentless DocListElm:s are not supported.  Leave 'doc_elm' in
			   unlinked_doc_list (where it ought to have been) from where it
			   will be deleted in our destructor.  Since there was no document
			   the history position is light-weight. */
			return;

		if (unlinked_doc_list.HasLink(doc_elm))
			doc_elm->Out();
		else
		{
			/* Either this history position is already in our history, or it
			   comes from a different history list.  Either way, we don't
			   support it. */
			OP_ASSERT(FALSE);
			return;
		}

		InsertHistoryElement(doc_elm);

		int win_history_num = window->GetCurrentHistoryNumber();
		if (pos == win_history_num)
			if( window->UpdateTitle() == OpStatus::ERR_NO_MEMORY )
				raise_OOM_condition = TRUE;

		while(doc_elm)
		{
			doc_elm->SetNumber(pos++);
			doc_elm = doc_elm->Suc();
		}

		DocListElm *elm = FirstDocListElm();
		while(elm)
		{
			if(doc == elm->Doc())
			{
				elm->SetOwnsDoc(TRUE);	// The first element that refers to the document owns it
				elm = elm->Suc();
				break;
			}
			elm = elm->Suc();
		}
		while(elm)
		{
			if(doc == elm->Doc())
			{
				elm->SetOwnsDoc(FALSE);  // No one else does
			}
			elm = elm->Suc();
		}


		history_len++;

	}

	if (raise_OOM_condition)
		RaiseCondition(OpStatus::ERR_NO_MEMORY);
}

#endif // DOCHAND_HISTORY_MANIPULATION_SUPPORT

void DocumentManager::HandleErrorUrl()
{
	SetLoadStat(NOT_LOADING);
	SetUserAutoReload(FALSE);
	SetReload(FALSE);
	SetRedirect(FALSE);
	SetReplace(FALSE);
	SetUseHistoryNumber(-1);
	SetAction(VIEWER_NOT_DEFINED);
	FramesDocument* current_doc = GetCurrentDoc();
	if (current_doc)
		SetCurrentURL(current_doc->GetURL(), FALSE);
	else
		SetCurrentURL(URL(), FALSE);

	EndProgressDisplay();

	if (es_pending_unload)
		OpStatus::Ignore(ESCancelPendingUnload());
}

DocumentManager* DocumentManager::GetDocManagerById(int sub_win_id) const
{
	if (FramesDocument* doc = GetCurrentDoc())
		return doc->GetDocManagerById(sub_win_id);
	else
		return NULL;
}

void DocumentManager::ESGeneratingDocument(ES_Thread *generating_thread, BOOL is_reload, DocumentOrigin* caller_origin)
{
	activity_loading.Begin();

	StoreRequestThreadInfo(generating_thread);

	BOOL replaced_empty;
	ES_Thread *migrate = NULL;

	int current_history_number = window->GetCurrentHistoryNumber();
	int next_history_number = GetNextHistoryNumber(is_reload, replaced_empty);

	FramesDocument *prev_frames_doc;
	FramesDocument *frames_doc;
	DocListElm *prev_doc_elm;
	URL referrer_url;

	prev_frames_doc = current_doc_elm->Doc();

#ifdef DOCUMENT_EDIT_SUPPORT
	BOOL inherit_design_mode = prev_frames_doc->GetDesignMode();
#endif // DOCUMENT_EDIT_SUPPORT

	frames_doc = OP_NEW(FramesDocument, (this, current_url, caller_origin, GetSubWinId()));
	if (!frames_doc ||
		frames_doc->CreateESEnvironment(TRUE) == OpStatus::ERR_NO_MEMORY ||
		frames_doc->ESStartGeneratingDocument(generating_thread) == OpStatus::ERR_NO_MEMORY)
	{
		OP_DELETE(frames_doc);
		goto cleanup_oom;
	}

	prev_doc_elm = current_doc_elm;

	// store the scroll position in DocListElm
	StoreViewport(prev_doc_elm);

	current_doc_elm = OP_NEW(DocListElm, (current_url, frames_doc, TRUE, next_history_number, GetNextDocListElmId()));
	if (!current_doc_elm)
	{
		current_doc_elm = prev_doc_elm;
		OP_DELETE(frames_doc);
		goto cleanup_oom;
	}

	current_doc_elm->SetReferrerUrl(prev_frames_doc, ShouldSendReferrer());

	InsertHistoryElement(current_doc_elm);

	current_doc_elm->SetScriptGeneratedDocument();
	if (replaced_empty)
		current_doc_elm->SetReplacedEmptyPosition();

	if (prev_doc_elm->Doc()->Undisplay() == OpStatus::ERR_NO_MEMORY)
		goto cleanup_oom;

	if (SignalOnNewPage(VIEWPORT_CHANGE_REASON_NEW_PAGE))
		waiting_for_document_ready = TRUE;

	// Both for javascript urls and ordinary document.open, make sure to signal the loading of a new document to the UI.
	OpStatus::Ignore(window->StartProgressDisplay(TRUE, FALSE /* bResetSecurityState */, !!GetParentDoc() /* bSubResourcesOnly */));

	prev_frames_doc->ESSetGeneratedDocument(frames_doc);
	frames_doc->ESSetReplacedDocument(prev_frames_doc);

	if (generating_thread->GetScheduler() == prev_frames_doc->GetESScheduler())
	{
		migrate = generating_thread;
	}
	else
	{
		ES_Thread *interrupted_thread = generating_thread->GetInterruptedThread();
		while (interrupted_thread)
		{
			ES_ThreadScheduler *scheduler = interrupted_thread->GetScheduler();
			if (interrupted_thread->GetBlockType() == ES_BLOCK_FOREIGN_THREAD && scheduler == prev_frames_doc->GetESScheduler())
			{
				migrate = interrupted_thread;
				break;
			}
			interrupted_thread = interrupted_thread->GetInterruptedThread();
		}
	}

	if (migrate)
	{
		/* The script is replacing its own document. */
		OP_STATUS status = frames_doc->ConstructDOMEnvironment();

		if (status == OpStatus::ERR)
		{
			/* It's quite odd if we decide scripting is disabled in the new
			   document, since it has the same URL as the previous one, in the
			   same window, and we obviously think scripting is enabled in the
			   previous one. */
			OP_ASSERT(FALSE);
			StopLoading(TRUE);
			return;
		}

		if (OpStatus::IsMemoryError(status) || OpStatus::IsMemoryError(frames_doc->GetESScheduler()->MigrateThread(migrate)))
			goto cleanup_oom;
	}

	if (prev_frames_doc->SetAsCurrentDoc(FALSE) == OpStatus::ERR_NO_MEMORY ||
		frames_doc->SetAsCurrentDoc(TRUE) == OpStatus::ERR_NO_MEMORY)
		goto cleanup_oom;

#ifdef DOCUMENT_EDIT_SUPPORT
	if (inherit_design_mode && OpStatus::IsMemoryError(frames_doc->SetEditable(FramesDocument::DESIGNMODE)))
		goto cleanup_oom;
#endif // DOCUMENT_EDIT_SUPPORT

	SetLoadStatus(DOC_CREATED);

	if (current_history_number != next_history_number)
		history_len++;

	frames_doc->ClearScreenOnLoad();
	CheckOnNewPageReady();

	return;

cleanup_oom:
	RaiseCondition(OpStatus::ERR_NO_MEMORY);
	StopLoading(TRUE);
}

void
DocumentManager::ESGeneratingDocumentFinished()
{
	if (!has_posted_history_cleanup)
	{
		has_posted_history_cleanup = TRUE;
		mh->PostMessage(MSG_HISTORY_CLEANUP, 0, 0);
	}

	activity_loading.Cancel();
}

/* virtual */ OP_STATUS
DocumentManager::GetRealWindow(DOM_Object *&window)
{
	FramesDocument* frames_doc = GetCurrentDoc();

	if (!frames_doc)
	{
		RETURN_IF_ERROR(CreateInitialEmptyDocument(FALSE, FALSE));
		frames_doc = GetCurrentDoc();
		OP_ASSERT(frames_doc);
	}
	else if (!frames_doc->IsCurrentDoc())
		return OpStatus::ERR;

	if (!frames_doc->GetLogicalDocument())
	{
		DM_LoadStat old_load_stat = load_stat;
		load_stat = DOC_CREATED;
		RETURN_IF_ERROR(frames_doc->CheckSource()); // CheckSource is dangerous... It skips necessary tests and makes us for instance parse and show binary data that should have been downloaded or put into a plugin. This should be removed I think.
		load_stat = old_load_stat;
	}

	if (!frames_doc->GetDOMEnvironment())
	{
		URL host = frames_doc->GetOrigin()->security_context;
		BOOL pref_js_enabled = g_pcjs->GetIntegerPref(PrefsCollectionJS::EcmaScriptEnabled, host);
		BOOL ecmascript_enabled = DOM_Environment::IsEnabled(frames_doc, pref_js_enabled);

		if (!ecmascript_enabled)
			return OpStatus::ERR;
	}

	RETURN_IF_ERROR(frames_doc->ConstructDOMEnvironment());
	window = frames_doc->GetJSWindow();

	return OpStatus::OK;
}

OP_STATUS DocumentManager::ConstructDOMProxyEnvironment()
{
	if (!dom_environment)
	{
		RETURN_IF_ERROR(DOM_ProxyEnvironment::Create(dom_environment));
		dom_environment->SetRealWindowProvider(this);
	}
	return OpStatus::OK;
}

OP_STATUS DocumentManager::GetJSWindow(DOM_Object *&window, ES_Runtime *origining_runtime)
{
	RETURN_IF_ERROR(ConstructDOMProxyEnvironment());
	return dom_environment->GetProxyWindow(window, origining_runtime);
}

BOOL DocumentManager::HasJSWindow()
{
	return dom_environment != NULL;
}

void DocumentManager::UpdateCurrentJSWindow()
{
	if (dom_environment)
		/* This triggers a call to the GetRealWindow callback the next
		   time the proxy window is used. */
		dom_environment->Update();
}

void DocumentManager::ESSetPendingUnload(ES_OpenURLAction *action)
{
	es_pending_unload = action ? TRUE : FALSE;
	es_terminating_action = action;
}

OP_STATUS DocumentManager::ESSendPendingUnload(BOOL check_if_inline_expired, BOOL use_plugin)
{
	OP_STATUS status = es_terminating_action->SendPendingUnload(check_if_inline_expired, use_plugin);

	es_pending_unload = FALSE;
	es_terminating_action = NULL;

	return status;
}

OP_STATUS DocumentManager::ESCancelPendingUnload()
{
	OP_STATUS status = es_terminating_action->CancelPendingUnload();

	es_pending_unload = FALSE;
	es_terminating_action = NULL;

	return status;
}

OP_STATUS DocumentManager::PutHistoryState(ES_PersistentValue *data, const uni_char *title, URL &url, BOOL create_new /*= TRUE*/)
{
	OP_ASSERT(current_doc_elm);
	URL previous_url = current_doc_elm->GetUrl();

	// Copy the security state from the original URL. The state shouldn't change because the
	// new URL must be on the same protocol and host.
	RETURN_IF_ERROR(url.SetAttribute(URL::KSecurityStatus, previous_url.GetAttribute(URL::KSecurityStatus)));
	RETURN_IF_ERROR(url.SetAttribute(URL::KSecurityText, previous_url.GetAttribute(URL::KSecurityText)));

	if (create_new)
	{
		DocumentReferrer referrer(current_doc_elm->Doc());
		RETURN_IF_ERROR(AddNewHistoryPosition(url, referrer, -1, title, TRUE, FALSE, current_doc_elm->Doc()));
	}
	else
	{
		RETURN_IF_ERROR(current_doc_elm->SetTitle(title));
		current_doc_elm->SetUrl(url);
	}

	current_doc_elm->SetData(data);

	if (GetLoadStatus() == NOT_LOADING) // Otherwise the current document loading will be broken
		SetCurrentURL(url, FALSE);
	else
	{
		if (current_doc_elm->Doc()->GetAuxUrlToHandleLoadingMessagesFor().IsEmpty())
			current_doc_elm->Doc()->SetAuxUrlToHandleLoadingMessagesFor(current_doc_elm->Doc()->GetURL());
		history_state_doc_elm = current_doc_elm;
	}

	// Set URLs in HLDocProfile
	current_doc_elm->Doc()->GetHLDocProfile()->OnPutHistoryState(url);
	// Set URL in FramesDocument as well and notify UI about the change.
	current_doc_elm->Doc()->SetUrl(current_doc_elm->GetUrl());
	current_doc_elm->Doc()->NotifyUrlChanged(current_doc_elm->GetUrl());
	return OpStatus::OK;
}

OP_STATUS
DocumentManager::CreateInitialEmptyDocument(BOOL for_javascript_url, BOOL prevent_early_onload,
                                            BOOL use_opera_blanker/* = FALSE */)
{
	OP_ASSERT(!GetCurrentDoc());

	// There is no document here yet (probably loading) so we'll make a
	// about:blank (or opera:blanker if 'use_opera_blanker' is TRUE)
	// placeholder so that scripts have something to play with. The current
	// state (what we're loading etc) shouldn't change.
	// The correct context id should be assigned to the blank url anyway,
	// since it may be used as a parent url somewhere else.
	const char* blank_url_name = use_opera_blanker ? "opera:blanker" : "about:blank";
	URL blank_url;
	if (GetFrame())
	{
		// Effectively "inherit" the context id from what we consider the parent url of the blank url should be.
		blank_url = g_url_api->GetURL(GetFrame()->GetParentFramesDoc()->GetURL(), blank_url_name);
	}
	else
	{
		URL opener_url = GetWindow()->GetOpenerSecurityContext();
		if (opener_url.IsEmpty())
			blank_url = g_url_api->GetURL(blank_url_name, GetWindow()->GetUrlContextId());
		else
			blank_url = g_url_api->GetURL(opener_url, blank_url_name);
	}

	if (load_stat == WAIT_FOR_HEADER && current_url == blank_url)
	{
		// Ah, we have an empty document loading. Speed it up.
		HandleHeaderLoaded(blank_url.Id(TRUE), FALSE);

		OP_ASSERT(GetCurrentDoc());
		OP_ASSERT(current_url == blank_url);
		OP_ASSERT(load_stat == NOT_LOADING || load_stat == DOC_CREATED);

		HandleDataLoaded(blank_url.Id(TRUE));

		OP_ASSERT(GetCurrentDoc());
		OP_ASSERT(current_url == blank_url);
		OP_ASSERT(load_stat == NOT_LOADING || load_stat == DOC_CREATED); // Most likely DOC_CREATED since we haven't done a Reflow
	}

	if (!GetCurrentDoc())
	{
		DocumentOrigin* origin = NULL;
		if (GetFrame())
			origin = GetFrame()->GetParentFramesDoc()->GetMutableOrigin();
		else if (current_loading_origin)
			origin = current_loading_origin;
		else if (referrer_url.origin)
			origin =referrer_url.origin;
		else if (GetWindow()->GetOpenerOrigin())
			origin = GetWindow()->GetOpenerOrigin();

		if (origin)
			origin->IncRef();
		else
		{
			origin = DocumentOrigin::Make(blank_url);

			if (!origin)
				return OpStatus::ERR_NO_MEMORY;
		}

		FramesDocument* doc = OP_NEW(FramesDocument, (this, blank_url, origin, GetSubWinId(), FALSE));
		origin->DecRef();
		origin = NULL;
		if (!doc)
			return OpStatus::ERR_NO_MEMORY;
		int history_number;
		if (FramesDocElm* fdelm = GetFrame())
			history_number = fdelm->GetParentFramesDoc()->GetDocManager()->CurrentDocListElm()->Number();
		else
		{
			BOOL replaced_empty;
			history_number = GetNextHistoryNumber(FALSE, replaced_empty);
		}
		DocListElm* new_dle = OP_NEW(DocListElm, (blank_url, doc, TRUE, history_number, GetNextDocListElmId()));
		if (!new_dle)
		{
			OP_DELETE(doc);
			return OpStatus::ERR_NO_MEMORY;
		}
		new_dle->SetIsReplaceable();
		SetUseHistoryNumber(history_number);
		InsertHistoryElement(new_dle);
		current_doc_elm = new_dle;
		doc->SetWaitForJavascriptURL(for_javascript_url);

		// SetAsCurrentDoc doesn't like to be called unless the url in
		// DocumentManager matches it so we temporarily change the url
		// but have to restore it afterwards so that the current
		// operation in DocumentManager can continue.
		URL temp = current_url;
		current_url = blank_url;
		RETURN_IF_ERROR(doc->SetAsCurrentDoc(TRUE));
		if (load_stat != NOT_LOADING)
			current_url = temp;

		OP_ASSERT(GetCurrentDoc() == doc);

		// We might actually be loading something already and this document shouldn't
		// interfere with that loading, just insert itself first in the list
		DM_LoadStat old_load_stat = load_stat;
		load_stat = DOC_CREATED;
		RETURN_IF_ERROR(doc->CheckSource()); // CheckSource is in general wrong and dangerous but in this case we knows that the document is a simple about:blank and we will do the right thing with it.
		load_stat = old_load_stat;
	}

	FramesDocument* doc = GetCurrentDoc();
	OP_ASSERT(doc);
	if (prevent_early_onload)
		doc->SetInhibitOnLoadAboutBlank(TRUE);
	OP_ASSERT(doc->GetLogicalDocument());
	OP_ASSERT(doc->GetLogicalDocument()->IsParsed());
	RETURN_IF_MEMORY_ERROR(doc->Reflow(FALSE, TRUE, FALSE, FALSE)); // Not happy about this at all, but without it IsLoaded() will not be TRUE
	OP_ASSERT(doc->GetLogicalDocument()->IsLoaded());
	if (prevent_early_onload)
		doc->SetInhibitOnLoadAboutBlank(FALSE);

	return OpStatus::OK;
}

static OP_STATUS GetURLFragment(URL url, OpString& decoded_fragment, OpString& original_fragment)
{
	RETURN_IF_ERROR(url.GetAttribute(URL::KUniFragment_Name, original_fragment));
	RETURN_IF_ERROR(decoded_fragment.Set(original_fragment));
	uni_char* frag = decoded_fragment.CStr();

	// Bug 292214 - url fragments are not url decoded when read with this API so we have to do it ourself
	if (frag && *frag)
	{
		// Need to url decode it
		// new_fragment_id is new_fragment.CStr()
		UriUnescape::ReplaceChars(frag, UriUnescape::NonCtrlAndEsc);
	}
	return OpStatus::OK;
}

OP_STATUS
DocumentManager::UpdateAction(const uni_char* &app)
{
	ViewAction action = GetAction();

	if (action == VIEWER_NOT_DEFINED)
	{
		RETURN_IF_MEMORY_ERROR(g_viewers->GetAppAndAction(current_url, action, app));

#ifdef CONTENT_DISPOSITION_ATTACHMENT_FLAG
		if (current_url.GetAttribute(URL::KUsedContentDispositionAttachment))
		{
			DocumentOrigin* unique_origin = DocumentOrigin::MakeUniqueOrigin(current_url.GetContextId(), TRUE);
			if (!unique_origin)
				return OpStatus::ERR_NO_MEMORY;
			if (current_loading_origin)
				current_loading_origin->DecRef();
			current_loading_origin = unique_origin;
# ifdef _BITTORRENT_SUPPORT_
			if (!(current_url.ContentType() == URL_P2P_BITTORRENT && action != VIEWER_OPERA))
# endif // _BITTORRENT_SUPPORT_
			{
				action = VIEWER_ASK_USER;
			}
		}
# ifdef _BITTORRENT_SUPPORT_
		if(!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableBitTorrent)
			&& current_url.ContentType() == URL_P2P_BITTORRENT && action == VIEWER_OPERA)
		{
			// if BitTorrent is turned off but we still have Opera set as viewer, we need a kludge to workaround
			// being asked to download it with Opera's BT anyway
			action = VIEWER_ASK_USER;
		}
# endif // _BITTORRENT_SUPPORT_
#endif

		// Some download dialogs for embedded content we want to supress
		if (action == VIEWER_ASK_USER && GetFrame())
		{
			if (HTML_Element* embedding_element = GetFrame()->GetHtmlElement())
			{
#ifdef SVG_SUPPORT
				// never-stop-loading on unknown svg document inlines (e.g <animation>, <foreignObject>)
				if (embedding_element->GetNsType() == NS_SVG)
				{
					action = VIEWER_IGNORE_URL;
				}
#endif // SVG_SUPPORT
				if (embedding_element->IsMatchingType(HE_OBJECT, NS_HTML) ||
					embedding_element->IsMatchingType(HE_EMBED, NS_HTML))
				{
					// Was probably meant as a plugin and nobody will be happy to see
					// a download dialog. See DSK-160957
					// This can be made smarter for sure, but we'll need more experience
					// with the real world use of objects for downloads first.
					action = VIEWER_IGNORE_URL;
				}
			}
		}

		SetAction(action);
	}

	return OpStatus::OK;
}

OP_STATUS DocumentManager::SetCurrentDoc(BOOL check_if_inline_expired, BOOL use_plugin, BOOL create_document, BOOL is_user_initiated)
{
	OP_PROBE3(OP_PROBE_DOCUMENTMANAGER_SETCURRENTDOC);
	OP_STATUS status = OpStatus::OK;
	BOOL raise_OOM_condition = FALSE;
	URLStatus url_stat = current_url.Status(TRUE);

	if (!create_document && !use_current_doc && (url_stat == URL_EMPTY || current_url.IsEmpty()))
	{
		HandleErrorUrl();
		return status;
	}
	else if (!create_document && !use_current_doc && url_stat == URL_UNLOADED)
	{
		StopLoading(FALSE);
		return status;
	}
	else
	{
#ifndef NO_URL_OPERA
		BOOL download_doc = (current_url.Type() == URL_OPERA && current_url.GetAttribute(URL::KName).CompareI("opera:download") == 0);
#endif // !NO_URL_OPERA
		URLContentType type = current_url.ContentType();

		if (!create_document &&
			!use_current_doc &&
#ifndef NO_URL_OPERA
			!download_doc &&
#endif // !NO_URL_OPERA
			!use_plugin &&
			type != URL_HTML_CONTENT &&
			type != URL_XML_CONTENT &&
#ifdef _WML_SUPPORT_
			type != URL_WML_CONTENT &&
			type != URL_WBMP_CONTENT &&
#endif
#ifdef SVG_SUPPORT
			type != URL_SVG_CONTENT &&
#endif
			type != URL_TEXT_CONTENT &&
#ifdef HAS_ATVEF_SUPPORT // Reckognize TV:
			type != URL_TV_CONTENT &&
#endif // HAS_ATVEF_SUPPORT
			type != URL_CSS_CONTENT &&
			type != URL_X_JAVASCRIPT &&
			!imgManager->IsImage(type) &&
#ifdef _SSL_SUPPORT_
			type != URL_PFX_CONTENT &&
			type != URL_X_X509_USER_CERT &&
			type != URL_X_X509_CA_CERT &&
			type != URL_X_PEM_FILE &&
#endif
#ifdef _ISHORTCUT_SUPPORT
			type != URL_ISHORTCUT_CONTENT &&
#endif//_ISHORTCUT_SUPPORT
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
			type != URL_PAC_CONTENT &&
#endif // SUPPORT_AUTO_PROXY_CONFIGURATION
#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
			type != URL_MHTML_ARCHIVE &&
#endif
#ifdef APPLICATION_CACHE_SUPPORT
			type != URL_MANIFEST_CONTENT &&
#endif // APPLICATION_CACHE_SUPPORT
#ifdef MEDIA_HTML_SUPPORT
			!g_media_module.IsMediaPlayableUrl(current_url) &&
#endif // MEDIA_HTML_SUPPORT
			type != URL_MIME_CONTENT)
		{
			HandleErrorUrl();
			return status;
		}

		BOOL use_existing_doc = FALSE, replaced_empty;

		window->SetIsImplicitSuppressWindow(!!current_url.GetAttribute(URL::KSuppressScriptAndEmbeds, TRUE));
		window->SetIsScriptableWindow(!current_url.GetAttribute(URL::KSuppressScriptAndEmbeds, TRUE));

		FramesDocument* doc = NULL;

		DocListElm* prev_doc_elm = current_doc_elm;

		if (prev_doc_elm && use_history_number == -1)
		{
			if (prev_doc_elm->IsReplaceable())
				/* Previous (that is, current) history position is "replaceable"
				   (that is, should be replaced,) so reuse its number and let
				   the grim reaper (that is, HistoryCleanup) take it later. */
				SetUseHistoryNumber(prev_doc_elm->Number());
		}

		int current_history_number = window->GetCurrentHistoryNumber();
		int next_history_number = GetNextHistoryNumber(replace, replaced_empty);

		// DSK-120311: When moving to a new document in the history, the
		// character encoding should reset to the default.
#ifdef RESET_ENCODING_ON_NEW_DOCUMENT
		if (current_history_number != next_history_number && !GetWindow()->IsMailOrNewsfeedWindow())
		{
#ifdef PREFS_HOSTOVERRIDE
			// Check if the forced character encoding is overridden for
			// this host.
			const OpStringC force_encoding(g_pcdisplay->GetStringPref(PrefsCollectionDisplay::ForceEncoding, current_url));
			if (force_encoding.HasContent())
			{
				// Convert to ASCII
				char sbencoding[128]; /* ARRAY OK 2011-01-04 peter */
				uni_cstrlcpy(sbencoding, force_encoding.CStr(), ARRAY_SIZE(sbencoding));
				window->SetForceEncoding(sbencoding, TRUE);
			}
			else
#endif
			{
				window->SetForceEncoding(g_pcdisplay->GetForceEncoding(), TRUE);
			}
		}
#endif // RESET_ENCODING_ON_NEW_DOCUMENT

#if defined ERROR_PAGE_SUPPORT && defined TRUST_RATING
		DocListElm* dle = (DocListElm*)doc_list.First();
		if (!dle || next_history_number == dle->Number() && !dle->Pred())
		{
			if (current_url.Type() == URL_OPERA && current_url.GetAttribute(URL::KIsClickThroughPage))
			{
				OpString8 url_str;
				OP_STATUS status;
				if (OpStatus::IsSuccess(status = current_url.GetAttribute(URL::KName, url_str)))
				{
					if (url_str.Compare(PHISHING_WARNING_URL) == 0)
					{
						URL about_blank_url = g_url_api->GetURL("about:blank");
						if (OpStatus::IsSuccess(status = AddNewHistoryPosition(about_blank_url, referrer_url, current_history_number, UNI_L("about:blank"))))
						{
							current_history_number = window->GetCurrentHistoryNumber();
							next_history_number = window->SetNewHistoryNumber();
						}
					}
				}
				if (OpStatus::IsMemoryError(status))
					raise_OOM_condition = TRUE;
			}
		}
#endif // defined ERROR_PAGE_SUPPORT && defined TRUST_RATING

		CancelPendingViewportRestoration();

		if (prev_doc_elm)
			StoreViewport(prev_doc_elm);

#ifdef _PLUGIN_SUPPORT_
		if (use_plugin)
			type = URL_HTML_CONTENT;
#endif // _PLUGIN_SUPPORT_

		if (current_doc_elm &&
		    current_doc_elm->Doc() &&
		    current_url.Type() != URL_JAVASCRIPT &&
		    current_history_number != next_history_number)
		{
			FramesDocument* curr_doc = current_doc_elm->Doc();
			// Normally, when we get here we have stopped most of the
			// document loading already.  However, when dealing with
			// response 204, No Content, we must not stop plugin
			// streams, so they have been left running until now.  At
			// this point we know that we didn't receive a 204, so we
			// can safely stop the plugin streams now.
			OpStatus::Ignore(curr_doc->StopLoading(FALSE, FALSE, TRUE));

			URL doc_url = curr_doc->GetURL();
			if (doc_url == current_url && initial_request_url == current_url && current_url.RelName()
#ifdef _WML_CARD_SUPPORT_
				&& ((curr_doc->GetLogicalDocument() && !curr_doc->GetHLDocProfile()->IsWml()))	// why this? /pw
#endif // _WML_CARD_SUPPORT_
				)
			{
				// Same url. Jump inside existing document
				use_existing_doc = TRUE;
				doc = curr_doc;
			}
		}

		if (!use_existing_doc)
		{
			if (GetWindow()->IsMailOrNewsfeedWindow())
			{
				if (prev_doc_elm && prev_doc_elm->Doc())
				{
					if (prev_doc_elm->Doc()->Undisplay() == OpStatus::ERR_NO_MEMORY) // always call Undisplay before SetAsCurrentDoc !!!
						raise_OOM_condition = TRUE;
					if (prev_doc_elm->Doc()->SetAsCurrentDoc(FALSE) == OpStatus::ERR_NO_MEMORY)
						raise_OOM_condition = TRUE;
				}

				// always delete prev document(s) -- not using history anyway
				RemoveUptoHistory(next_history_number);
				current_doc_elm = prev_doc_elm = NULL;
			}
			DocumentOrigin* new_doc_origin;
			if (current_loading_origin)
			{
				new_doc_origin = current_loading_origin;
				current_loading_origin = NULL;
				// Not calling IncRef and DecRef since the number of references stay the same.
			}
			else
				new_doc_origin = DocumentOrigin::Make(current_url);
			doc = NULL;
			if (new_doc_origin)
			{
				doc = OP_NEW(FramesDocument, (this, current_url, new_doc_origin, GetSubWinId(), use_plugin, current_url_is_inline_feed));

				new_doc_origin->DecRef();
				new_doc_origin = NULL;
			}
			if (!doc)
				raise_OOM_condition = TRUE;
		}

		if (doc)
		{
			current_doc_elm = OP_NEW(DocListElm, (current_url, doc, !use_existing_doc, next_history_number, GetNextDocListElmId()));

			if (!current_doc_elm)
			{
				OP_DELETE((doc));
				doc = NULL;
				raise_OOM_condition = TRUE;
				goto cleanup0;
			}

			InsertHistoryElement(current_doc_elm);

#ifdef DOCHAND_HISTORY_SAVE_ZOOM_LEVEL
			if (!GetParentDoc())
				window->SetScale(current_doc_elm->GetLastScale());
#endif // DOCHAND_HISTORY_SAVE_ZOOM_LEVEL

			current_doc_elm->SetReferrerUrl(referrer_url, send_to_server);
			if (replaced_empty)
				current_doc_elm->SetReplacedEmptyPosition();

			if (doc == doc->GetTopDocument() && !doc->GetURL().GetAttribute(URL::KIsGenerated))
				window->SetType(doc->Type());

			doc->SetStartNavigationTimeMS(start_navigation_time_ms);

			if (use_existing_doc)
			{
				OpString original_fragment;
				OpString decoded_fragment;
				if (GetURLFragment(current_url, decoded_fragment, original_fragment) == OpStatus::ERR_NO_MEMORY ||
					doc->SetRelativePos(original_fragment.CStr(), decoded_fragment.CStr()) == OpStatus::ERR_NO_MEMORY)
					raise_OOM_condition = TRUE;

				if (url_stat != URL_LOADING)
				{
					SetLoadStat(NOT_LOADING);

					EndProgressDisplay();
				}

				if (doc->SetAsCurrentDoc(TRUE) == OpStatus::ERR_NO_MEMORY)
					raise_OOM_condition = TRUE;

				if (!raise_OOM_condition)
					return status;
				else
					goto cleanup0;
			}

			if (prev_doc_elm && prev_doc_elm->Doc())
			{
				if (prev_doc_elm->Doc()->Undisplay() == OpStatus::ERR_NO_MEMORY) // always call Undisplay before SetAsCurrentDoc !!!
					raise_OOM_condition = TRUE;
				if (prev_doc_elm->Doc()->SetAsCurrentDoc(FALSE) == OpStatus::ERR_NO_MEMORY)
					raise_OOM_condition = TRUE;

				g_memory_manager->CheckDocMemorySize();

				FlushHTTPSFromHistory(prev_doc_elm, current_doc_elm);
			}

			if (SignalOnNewPage(VIEWPORT_CHANGE_REASON_NEW_PAGE))
				waiting_for_document_ready = TRUE;

			if( doc->SetAsCurrentDoc(TRUE) == OpStatus::ERR_NO_MEMORY )
				raise_OOM_condition = TRUE;

			if (GetFrame() && GetFrame()->GetParentFramesDoc()->GetFramesStacked())
			{
				FramesDocument* top_doc = GetFrame()->GetTopFramesDoc();
				if (top_doc)
				{
					if (next_history_number > current_history_number)
						top_doc->SetLoadingFrame(GetFrame());
				}
			}

			OP_BOOLEAN res = OpStatus::OK;

			doc->ClearScreenOnLoad();

#ifdef _WML_SUPPORT_
			if (type == URL_WML_CONTENT || wml_context)
			{
				if (!wml_context)
				{
					if (WMLInit() == OpStatus::ERR_NO_MEMORY)
					raise_OOM_condition = TRUE;
				}
				else
				{
					/* 10.4 Context Restrictions of WML 1.3:
					 * [...] Whenever a user agent navigates to a resource that was not the result of an interaction with
					 * the content in the current context, the user agent must establish another context for that navigation.
					 * The user agent may terminate the current context before establishing another one for the new
					 * navigation attempt.
					 */
					if (wml_context->IsSet(WS_USERINIT))
					{
						if (wml_context->SetNewContext() == OpStatus::ERR_NO_MEMORY)
							raise_OOM_condition = TRUE;
						wml_context->SetStatusOff(WS_USERINIT);
					}

					if (wml_context->PreParse() == OpStatus::ERR_NO_MEMORY)
						raise_OOM_condition = TRUE;

					current_doc_elm->SetWmlContext(wml_context);
				}

				SetLoadStat(DOC_CREATED);

				if ((res = doc->CheckSource()) == OpStatus::ERR_NO_MEMORY)
					raise_OOM_condition = TRUE;

				if (wml_context)
				{
					if (wml_context->IsSet(WS_NOACCESS))
					{
						StopLoading(FALSE, TRUE);
						wml_context->DenyAccess();
						if (prev_doc_elm)
						{
							int old_pos = current_doc_elm->Number();
							SetCurrentHistoryPos(prev_doc_elm->Number(), TRUE, is_user_initiated);
							RemoveFromHistory(old_pos, FALSE);
							window->SetCurrentHistoryNumber(prev_doc_elm->Number());
						}
						else
							Clear();

						return raise_OOM_condition ? OpStatus::ERR_NO_MEMORY : status;
					}

					if (wml_context->IsSet(WS_CLEANHISTORY))
					{ // transition to another WML session
						WMLDeWmlifyHistory();
						wml_context->SetFirstInSession(next_history_number);
						wml_context->SetStatusOff(WS_CLEANHISTORY);
					}
					else if (wml_context->IsSet(WS_GO | WS_NEWCONTEXT)) // new context -> new session
						wml_context->SetFirstInSession(next_history_number);
				}
			}
			else
#endif // _WML_SUPPORT_

				if (GetLoadStatus() == DOC_CREATED) // See if we can build the document immediately
				{
					if (OpStatus::IsMemoryError((res = doc->CheckSource())))
						raise_OOM_condition = TRUE;
				}

			if (history_len == 0 || current_history_number != next_history_number)
				history_len++;
			else if (prev_doc_elm && prev_doc_elm->Number() >= current_history_number)
			{
				if (prev_doc_elm == history_state_doc_elm)
				{
					history_state_doc_elm = NULL;
				}
				prev_doc_elm->Out();
				OP_DELETE(prev_doc_elm);
			}

			if (FramesDocument::CheckOnLoad(doc) == OpStatus::ERR_NO_MEMORY)
				raise_OOM_condition = TRUE;

			OpStatus::Ignore(window->UpdateTitle());//Not critical


		}
	}

cleanup0:
	if (raise_OOM_condition)
	{
		RaiseCondition(OpStatus::ERR_NO_MEMORY);
		StopLoading(TRUE, TRUE, TRUE);

		status = OpStatus::ERR_NO_MEMORY;
	}

	CheckOnNewPageReady();

#ifdef QUICK
	g_input_manager->UpdateAllInputStates();
#endif // QUICK

	return status;
}

OP_STATUS DocumentManager::UpdateCurrentDoc(BOOL use_plugin, BOOL parsing_restarted, BOOL is_user_initiated)
{
	OP_STATUS status = OpStatus::OK;
	OpStatus::Ignore(status);

	BOOL raise_OOM_condition = FALSE;
	URLStatus url_stat = current_url.Status(TRUE);

	if (url_stat == URL_UNLOADED)
		return status;

	if (url_stat == URL_EMPTY || current_url.IsEmpty())
	{
		HandleErrorUrl();
		return status;
	}
	else
	{
		URLContentType type = current_url.ContentType();

		if (!use_plugin &&
		    !imgManager->IsImage(type) &&
#ifdef HAS_ATVEF_SUPPORT
		    type != URL_TV_CONTENT &&
#endif // HAS_ATVEF_SUPPORT
		    type != URL_HTML_CONTENT &&
		    type != URL_XML_CONTENT &&
#ifdef _WML_SUPPORT_
		    type != URL_WML_CONTENT &&
#endif
#ifdef SVG_SUPPORT
		    type != URL_SVG_CONTENT &&
#endif
		    type != URL_TEXT_CONTENT &&
		    type != URL_CSS_CONTENT &&
		    type != URL_X_JAVASCRIPT &&
		    GetLoadStatus() != WAIT_FOR_HEADER)
		{
			HandleErrorUrl();
			return status;
		}

		if (current_doc_elm && current_doc_elm->Doc())
		{
			if (imgManager->IsImage(type))
				UrlImageContentProvider::ResetImageFromUrl(current_url);

			FramesDocument* doc = current_doc_elm->Doc();
			/* Store the view port but not in a case history navigation took place as in such case
			 * the storing is done in SetCurrentHistoryPos(). Also do not store the view port when
			 * the parsing was restarted (due to a wrong charset used previously) as it may be invalid
			 * for the reparsed document.
			 */
			if (!IsWalkingInHistory() && !parsing_restarted)
				StoreViewport(current_doc_elm);

			if (use_plugin || doc->GetURL().IsEmpty())
			{
#ifdef _PLUGIN_SUPPORT_
				if (use_plugin)
					type = URL_HTML_CONTENT;
#endif // _PLUGIN_SUPPORT_

				DocumentOrigin* origin = CreateOriginFromLoadingState();
				doc = NULL;
				if (origin)
				{
					doc = OP_NEW(FramesDocument, (this, current_url, origin, GetSubWinId(), use_plugin));
					origin->DecRef();
					origin = NULL;
				}

				if (!doc)
				{
					raise_OOM_condition = TRUE;
					goto cleanup2;
				}

				current_doc_elm->ReplaceDoc(doc);
				if (doc->SetAsCurrentDoc(TRUE) == OpStatus::ERR_NO_MEMORY)
					raise_OOM_condition = TRUE;

				if (doc == doc->GetTopDocument())
					window->SetType(doc->Type());
			}
			else if (use_current_doc)
				doc->SetUrl(current_url);
			else
			{
				if (doc->IsFrameDoc() && current_doc_elm->Number() > GetWindow()->GetCurrentHistoryPos())
				{
					GetWindow()->SetCurrentHistoryPos(current_doc_elm->Number(), FALSE, is_user_initiated);
					GetWindow()->SetCheckHistory(TRUE);
				}

				DocumentOrigin* origin = CreateOriginFromLoadingState();

				if (origin)
				{
					if (doc->ReloadedUrl(current_url, origin, parsing_restarted) == OpStatus::ERR_NO_MEMORY)
						raise_OOM_condition = TRUE;
					origin->DecRef();

					URL_InUse temporary_url_use(current_url);

					if (doc->SetAsCurrentDoc(TRUE) == OpStatus::ERR_NO_MEMORY)
						raise_OOM_condition = TRUE;

					/* Synchronize referrer of the current element also. */
					CurrentDocListElm()->SetReferrerUrl(referrer_url, CurrentDocListElm()->ShouldSendReferrer());
				}
				else
					raise_OOM_condition = TRUE;
			}

#ifdef _WML_SUPPORT_
			if (type == URL_WML_CONTENT || wml_context)
			{
				if (!wml_context)
				{
					if (WMLInit() == OpStatus::ERR_NO_MEMORY)
						raise_OOM_condition = TRUE;
				}
				else if (wml_context->PreParse() == OpStatus::ERR_NO_MEMORY)
					raise_OOM_condition = TRUE;

				SetLoadStat(DOC_CREATED);

				if (doc->CheckSource() == OpStatus::ERR_NO_MEMORY)
					raise_OOM_condition = TRUE;

				if (!doc->IsLoaded())
					SetLoadStat(DOC_CREATED);

				if (wml_context)
				{
					if( wml_context->IsSet(WS_NOACCESS) )
					{
						StopLoading(FALSE);
						wml_context->DenyAccess();
						return raise_OOM_condition ? OpStatus::ERR_NO_MEMORY : status;
					}

					if( wml_context->IsSet(WS_CLEANHISTORY) )
					{ // transition to another WML session
						WMLDeWmlifyHistory();
						wml_context->SetFirstInSession(current_doc_elm->Number());
						wml_context->SetStatusOff(WS_CLEANHISTORY);
					}
					else if( wml_context->IsSet(WS_GO | WS_NEWCONTEXT) ) // new context -> new session
						wml_context->SetFirstInSession(current_doc_elm->Number());
				}
			}
			else
#endif // _WML_SUPPORT_
			{
				// This code is both used for "reload" and "reload from
				// cache". In case of "reload from cache", the load_stat
				// isn't updated yet.
				SetLoadStat(DOC_CREATED);
				were_inlines_reloaded = reload_inlines;
				if (doc->CheckSource() == OpStatus::ERR_NO_MEMORY)
					raise_OOM_condition = TRUE;
			}

			if (OpStatus::IsMemoryError(UpdateWindowHistoryAndTitle()))
				raise_OOM_condition = TRUE;

		  	return raise_OOM_condition ? OpStatus::ERR_NO_MEMORY : status;
		}
	}

cleanup2:
	if (raise_OOM_condition)
	{
		RaiseCondition(OpStatus::ERR_NO_MEMORY);
		StopLoading(TRUE, TRUE, TRUE);

		status = OpStatus::ERR_NO_MEMORY;
	}

	return status;
}

void DocumentManager::SetCurrentNew(DocListElm* prev_doc_elm, BOOL is_user_initiated)
{
	URL start_url = current_doc_elm->GetUrl();
	BOOL pref_frames_enabled = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::FramesEnabled, start_url);
	BOOL pref_always_reload_https_in_history = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AlwaysReloadHTTPSInHistory);
	check_expiry = (CheckExpiryType) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CheckExpiryHistory);

	BOOL raise_OOM_condition = FALSE;

	error_page_shown = FALSE;

	current_url.StopLoading(mh);

	FramesDocument* prev_doc = prev_doc_elm ? prev_doc_elm->Doc() : NULL;

	if (prev_doc)
	{
		URL prev_doc_url = prev_doc->GetURL();

		if (current_url == prev_doc_url)
			if (prev_doc->StopLoading(FALSE) == OpStatus::ERR_NO_MEMORY)
				raise_OOM_condition = TRUE;
	}

	FramesDocument* doc = GetCurrentDoc();

	if (!frame)
		window->SetType(doc->Type());

#ifdef NEARBY_ELEMENT_DETECTION
	window->SetElementExpander(NULL);
#endif // NEARBY_ELEMENT_DETECTION

	BOOL show_img = window->ShowImages();

#ifdef SUPPORT_VISUAL_ADBLOCK
	// cache the server name so it's faster to do a lookup to check if the content blocker is enabled or not
	if(doc && !parent_doc)
		if (ServerName *server_name = doc->GetURL().GetServerName())
			window->SetContentBlockServerName(server_name);
#endif // SUPPORT_VISUAL_ADBLOCK

	if (!GetWindow()->IsMailOrNewsfeedWindow())
	{
		// Bug#128887: When moving to a new document in the history, the
		// character encoding should reset to the default.
		// Check correctness
#ifdef PREFS_HOSTOVERRIDE
		const OpStringC force_encoding(g_pcdisplay->GetStringPref(PrefsCollectionDisplay::ForceEncoding, start_url));
		if (!force_encoding.IsEmpty())
		{
			char sbencoding[128]; // ARRAY OK 2008-02-28 jl
			uni_cstrlcpy(sbencoding, force_encoding.CStr(), ARRAY_SIZE(sbencoding));
			window->SetForceEncoding(sbencoding, TRUE);
		}
		else
#endif // PREFS_HOSTOVERRIDE
			window->SetForceEncoding(g_pcdisplay->GetForceEncoding(), TRUE);
	}

	if (prev_doc_elm)
	{
		FramesDocument* pdoc = prev_doc_elm->Doc();

		if (pdoc)
		{
			if (pdoc->Undisplay() == OpStatus::ERR_NO_MEMORY)  // always call Undisplay before SetAsCurrentDoc !!!
				raise_OOM_condition = TRUE;

			StoreViewport(prev_doc_elm);

			if (pdoc->SetAsCurrentDoc(FALSE) == OpStatus::ERR_NO_MEMORY)
				raise_OOM_condition = TRUE;

			URL_InUse url_anchor(current_doc_elm->GetUrl()); // Don't flush the url.
			g_memory_manager->DisplayedDoc(doc); // Don't flush the document we're going to.
			g_memory_manager->CheckDocMemorySize();
		}
	}

	URL url = current_doc_elm->GetUrl();
	SetCurrentURL(url, FALSE);
	current_url_is_inline_feed = FALSE;

#ifdef TRUST_RATING
	OP_STATUS status = CheckTrustRating(url);
	if (status == OpStatus::ERR_NO_ACCESS)
		return;
	if (OpStatus::IsMemoryError(status))
		raise_OOM_condition = TRUE;
#endif // TRUST_RATING

	if (window->StartProgressDisplay(TRUE) == OpStatus::ERR_NO_MEMORY)
		raise_OOM_condition = TRUE;

	window->SetState(CLICKABLE);

	BOOL clean_document = FALSE;

	if (pref_frames_enabled)
	{
		// check if document is frames enabled but not formatted as
		if (!doc->IsFrameDoc() && doc->IsFramesEnabledDoc())
			clean_document = TRUE;
	}
	else if (doc->IsFrameDoc())
		clean_document = TRUE;

	if (clean_document)
	{
		doc->Clean();

		doc->CleanESEnvironment(TRUE);

		if (doc->CreateESEnvironment(TRUE) == OpStatus::ERR_NO_MEMORY)
			raise_OOM_condition = TRUE;
	}

#ifdef _WML_SUPPORT_
	if (current_doc_elm && current_doc_elm->GetWmlContext())
	{
		if (wml_context != current_doc_elm->GetWmlContext())
		{
			if (wml_context)
				wml_context->DecRef();
			wml_context = current_doc_elm->GetWmlContext();
			if (wml_context)
				wml_context->IncRef();
		}
	}
#endif // _WML_SUPPRT_

	BOOL reload_https = pref_always_reload_https_in_history && url.Type() == URL_HTTPS;

	if (frame)
	{
		if (reload_https)
			frame->Free(FALSE, FramesDocument::FREE_VERY_IMPORTANT);

		BOOL visible_if_current = TRUE;

		if (frame->SetAsCurrentDoc(TRUE, visible_if_current) == OpStatus::ERR_NO_MEMORY)
			raise_OOM_condition = TRUE;
	}
	else
	{
		if (reload_https)
			doc->Free(FALSE, FramesDocument::FREE_VERY_IMPORTANT);

		if (doc->SetAsCurrentDoc(TRUE) == OpStatus::ERR_NO_MEMORY)
			raise_OOM_condition = TRUE;
	}

	window->DocManager()->UpdateSecurityState(FALSE);

	if (!frame)
	{
		if (OpStatus::IsMemoryError(UpdateWindowHistoryAndTitle()))
			raise_OOM_condition = TRUE;

#ifdef SHORTCUT_ICON_SUPPORT
		if (OpStatus::IsMemoryError(window->SetWindowIcon(doc->Icon())))
			raise_OOM_condition = TRUE;
#endif // SHORTCUT_ICON_SUPPORT
	}

	if (url.Type() == URL_JAVASCRIPT)
	{
		if (!doc->GetLogicalDocument())
		{
			// Not sure this can happen anylonger, but... we do need a document for javascript urls to run in. Normally just an empty about:blank document.
			OpenURLOptions options;
			options.is_walking_in_history = TRUE;
			options.user_initiated = is_user_initiated;
			OpenURL(url, doc, FALSE/*check_if_expired*/, FALSE/*reload*/, options);
		}
	}
	else if (!current_doc_elm->IsScriptGeneratedDocument())
	{
		URL_HistoryNormal loadpolicy;
		loadpolicy.SetUserInitiated(is_user_initiated);
		URL ref_url;
		if (!current_doc_elm->GetReferrerUrl().origin ||
		    !current_doc_elm->GetReferrerUrl().origin->IsUniqueOrigin())
			ref_url = current_doc_elm->GetReferrerUrl().url;
		CommState cstate = url.LoadDocument(mh, ref_url, loadpolicy);

		// this will be turned on here, and off when finishing loading
		history_walk = (current_doc_elm && prev_doc_elm && current_doc_elm->Number() < prev_doc_elm->Number()) ? DM_HIST_BACK : DM_HIST_FORW;

		if (cstate == COMM_LOADING)
		{
			// URL is cleared from cache and is now reloading. Clean document.

			if (frame)
				frame->Free(FALSE, FramesDocument::FREE_VERY_IMPORTANT);
			else
				doc->Free(FALSE, FramesDocument::FREE_VERY_IMPORTANT);

			doc->ClearScreenOnLoad(TRUE);

			if (raise_OOM_condition)
				RaiseCondition(OpStatus::ERR_NO_MEMORY);

			SetUserAutoReload(FALSE);
			SetReload(TRUE);
			SetReloadFlags(TRUE, TRUE, FALSE, TRUE);

			SetLoadStat(WAIT_FOR_HEADER);

			/* On failure, rare race conditions may occur.  Usually everything
			   works fine, though, so ignoring this error is quite safe. */
			OpStatus::Ignore(current_url_reserved.SetURL(url));

			return;
		}

		SetLoadStat(DOC_CREATED);

		// Bug#87069: The document was in cache, make sure we set up the
		// viewers and action accordingly.
		const uni_char* app = NULL;
		if (OpStatus::IsMemoryError(UpdateAction(app)))
			raise_OOM_condition = TRUE;

		URLContentType type = current_url.ContentType();
		if (type == URL_UNKNOWN_CONTENT
			|| type == URL_UNDETERMINED_CONTENT)
		{
			current_url.SetAttribute(URL::KForceContentType, URL_TEXT_CONTENT);
		}
	}

	if (doc->SetMode(show_img, window->LoadImages(), window->GetCSSMode(), check_expiry) == OpStatus::ERR_NO_MEMORY)
		raise_OOM_condition = TRUE;

	doc->RecalculateLayoutViewSize(TRUE);

	if (doc->IsTopDocument())
	{
		double min_zoom = ZoomLevelNotSet;
		double max_zoom = ZoomLevelNotSet;
		double zoom = ZoomLevelNotSet;
		BOOL user_zoomable = TRUE;
# ifdef CSS_VIEWPORT_SUPPORT

		HLDocProfile* hld_prof = doc->GetHLDocProfile();
		if (hld_prof)
		{
			CSSCollection* css_coll = hld_prof->GetCSSCollection();
			CSS_Viewport* css_viewport = css_coll->GetViewport();

			if (css_viewport->GetMinZoom() != CSS_VIEWPORT_ZOOM_AUTO)
				min_zoom = css_viewport->GetMinZoom();

			if (css_viewport->GetMaxZoom() != CSS_VIEWPORT_ZOOM_AUTO)
				max_zoom = css_viewport->GetMaxZoom();

			user_zoomable = css_viewport->GetUserZoom();

			if (css_viewport->GetZoom() != CSS_VIEWPORT_ZOOM_AUTO)
				zoom = css_viewport->GetZoom();
		}
#endif // CSS_VIEWPORT_SUPPORT

		ViewportController* controller = doc->GetWindow()->GetViewportController();
		OpViewportInfoListener* info_listener = controller->GetViewportInfoListener();
		OpViewportRequestListener* request_listener = controller->GetViewportRequestListener();

		vis_dev->SetLayoutScale(window->GetTrueZoomBaseScale());

		info_listener->OnZoomLevelLimitsChanged(controller, min_zoom, max_zoom, user_zoomable);
		request_listener->OnZoomLevelChangeRequest(controller, zoom, 0, VIEWPORT_CHANGE_REASON_NEW_PAGE);
	}

	if (current_doc_elm->GetLastScale() != window->GetScale())
		if (HTML_Element* root = doc->GetDocRoot())
			root->RemoveCachedTextInfo(doc);

#ifdef DOCHAND_HISTORY_SAVE_ZOOM_LEVEL
	if (!GetParentDoc())
		window->SetScale(current_doc_elm->GetLastScale());
#endif // DOCHAND_HISTORY_SAVE_ZOOM_LEVEL

#ifdef _WML_SUPPORT_
	if (url.ContentType() == URL_WML_CONTENT || wml_context)
	{
		if (!wml_context)
		{
			if (WMLInit() == OpStatus::ERR_NO_MEMORY)
				raise_OOM_condition = TRUE;
		}
		else if (wml_context->PreParse() == OpStatus::ERR_NO_MEMORY)
			raise_OOM_condition = TRUE;

		if (prev_doc_elm && prev_doc_elm->Number() > current_doc_elm->Number())
			wml_context->SetStatusOn(WS_ENTERBACK);

		if (doc->CheckSource() == OpStatus::ERR_NO_MEMORY)
			raise_OOM_condition = TRUE;

		if (wml_context)
		{
			if (wml_context->IsSet(WS_NOACCESS))
			{
				history_walk = DM_NOT_HIST;
				wml_context->DenyAccess();
				StopLoading(FALSE);
				return;
			}

			if (wml_context->IsSet(WS_CLEANHISTORY))
			{ // transition to another WML session
				WMLDeWmlifyHistory();
				wml_context->SetFirstInSession(window->SetNewHistoryNumber());
				wml_context->SetStatusOff(WS_CLEANHISTORY);
			}
			else
				if (wml_context->IsSet(WS_GO | WS_NEWCONTEXT)) // new context -> new session
					wml_context->SetFirstInSession(window->SetNewHistoryNumber());
		}
	}
	else
#endif // _WML_SUPPORT_
		if (OpStatus::IsMemoryError(doc->CheckSource()))
			raise_OOM_condition = TRUE;

	if (doc->ReactivateDocument() == OpStatus::ERR_NO_MEMORY)
		raise_OOM_condition = TRUE;

	if (doc->IsLoaded())
	{
		SetLoadStat(NOT_LOADING);
		SetAction(VIEWER_NOT_DEFINED);
		EndProgressDisplay();
	}

	if (doc->IsParsed())
	{
		// update screen
		GetVisualDevice()->UpdateAll();
	}

	if (!window->IsLoading())
		window->GetMessageHandler()->PostMessage(MSG_UPDATE_PROGRESS_TEXT, 0, 0);
	else
	{
		window->SetState(CLICKABLE);
		if( window->SetProgressText(url, TRUE) == OpStatus::ERR_NO_MEMORY )
			raise_OOM_condition = TRUE;
	}

	history_walk = DM_NOT_HIST;

	if (raise_OOM_condition)
		RaiseCondition(OpStatus::ERR_NO_MEMORY);
}

BOOL DocumentManager::IsCurrentDocTheSameAt(int position)
{
	DocListElm* doc_elm  = current_doc_elm;
	FramesDocument* frames_doc = GetCurrentDoc();

	if (doc_elm == NULL || frames_doc == NULL)
		return FALSE;

	if (doc_elm->Number() == position)
	{
		return TRUE;
	}
	else if (doc_elm->Number() < position)
	{
		while (doc_elm != NULL && doc_elm->Number() < position && doc_elm->Doc() == frames_doc)
			doc_elm = doc_elm->Suc();

		if (doc_elm != NULL && doc_elm->Number() > position)
			//jumped too far away due to numbering gaps
			doc_elm = doc_elm->Pred();
	}
	else //(doc_elm->Number() > position)
	{
		while (doc_elm != NULL && doc_elm->Number() > position && doc_elm->Doc() == frames_doc)
			doc_elm = doc_elm->Pred();
	}

	return doc_elm != NULL && doc_elm->Doc() == frames_doc;
}

void DocumentManager::SetCurrentHistoryPos(int num, BOOL parent_doc_changed, BOOL is_user_initiated)
{
	if (is_clearing || doc_list.Empty())
		return;

	activity_loading.Begin();
	activity_refresh.Cancel();

#ifdef _PRINT_SUPPORT_
	BOOL was_in_print_preview = FALSE;

	if (print_preview_vd)
	{
		window->TogglePrintMode(TRUE);
		was_in_print_preview = TRUE;
	}
#endif // _PRINT_SUPPORT_

	// Store document position
	if (current_doc_elm)
	{
		StoreViewport(current_doc_elm);
	}

	DocListElm* prev_doc_elm = current_doc_elm;

	history_walk = (current_doc_elm && current_doc_elm->Number() > num) ? DM_HIST_BACK : DM_HIST_FORW;

	current_doc_elm = LastDocListElm();

	while (current_doc_elm->Number() > num && current_doc_elm->Pred())
		current_doc_elm = current_doc_elm->Pred();

	// Check that current_doc_elm changed, to avoid sending OnNewPage
	// for top document when walking in (i)frames history.
	if (current_doc_elm != prev_doc_elm)
	{
		/** If navigate within the same document, use
		 * VIEWPORT_CHANGE_REASON_JUMP_TO_RELATIVE_POS */
		OpViewportChangeReason reason =
			(prev_doc_elm && current_doc_elm->doc == prev_doc_elm->doc ?
			 VIEWPORT_CHANGE_REASON_JUMP_TO_RELATIVE_POS :
			 VIEWPORT_CHANGE_REASON_NEW_PAGE);
		if (SignalOnNewPage(reason))
			waiting_for_document_ready = TRUE;
	}

	FlushHTTPSFromHistory(prev_doc_elm, current_doc_elm);

	FramesDocument *doc = current_doc_elm->Doc();
	URL url = current_doc_elm->GetUrl();

	// Need to keep a pointer to these fragments before changing the URL
	const uni_char *doc_fragment = doc->GetURL().UniRelName();
	const uni_char *hist_fragment = url.UniRelName();

	// We always have to activate the document if current_doc_elm points to another document,
	// or a document that is not yet activated
	BOOL activate_document = !prev_doc_elm || current_doc_elm->Doc() != prev_doc_elm->Doc() || !doc->IsCurrentDoc() || !doc->GetLogicalDocument() && load_stat == NOT_LOADING;

#ifdef _WML_SUPPORT_
	if (!activate_document && current_doc_elm != prev_doc_elm &&
	    (current_doc_elm->GetWmlContext() || prev_doc_elm->GetWmlContext()))
		activate_document = TRUE;
#endif // _WML_SUPPORT_

	if (!activate_document && parent_doc_changed)
	{
		BOOL reload_https = current_doc_elm->GetUrl().Type() == URL_HTTPS && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AlwaysReloadHTTPSInHistory);
		activate_document = reload_https;
		if (!activate_document && doc->UrlNeedReload(check_expiry))
			activate_document = !doc->IsLoaded() || doc->GetURL().GetAttribute(URL::KCachePolicy_MustRevalidate);
	}

	if (activate_document)
	{
		if (!parent_doc_changed)
			window->ResetSecurityStateToUnknown();

		ES_Runtime *es_runtime = doc->GetESRuntime();
		if (es_runtime && es_runtime->Enabled())
		{
			ES_ThreadScheduler *es_scheduler = doc->GetESScheduler();
			es_scheduler->RemoveThreads(TRUE);
		}

		SetCurrentNew(prev_doc_elm, is_user_initiated);

		es_runtime = doc->GetESRuntime();

		if (es_runtime && es_runtime->Enabled())
		{
			ES_ThreadScheduler *es_scheduler = doc->GetESScheduler();
			RAISE_AND_RETURN_VOID_IF_ERROR(es_scheduler->Activate());
		}

		/** This assert just checks that the urls in the DoclistElm
		 *  and FramesDocument are the same, although they might have
		 *  different fragments.
		 */
		OP_ASSERT(doc->GetURL() == url);
		doc->SetUrl(url);
	}
	else
	{
		doc->SetUrl(url);

		if (!GetParentDoc())
		{
			WindowCommander *wc = GetWindow()->GetWindowCommander();
			uni_char *tempname = Window::ComposeLinkInformation(url.GetAttribute(URL::KUniName_Username_Password_Hidden).CStr(), url.GetAttribute(URL::KUniFragment_Name).CStr());

			if (tempname)
			{
				wc->GetLoadingListener()->OnUrlChanged(wc, tempname);
				OP_DELETEA(tempname);
			}
		}

		history_walk = DM_NOT_HIST;
	}

#ifdef SVG_SUPPORT
	if (current_url == url && (url.RelName() || current_url.RelName()))
	{
		if (LogicalDocument* logdoc = doc->GetLogicalDocument())
			if (HTML_Element* root = logdoc->GetDocRoot())
				if (SVGImage* svg = g_svg_manager->GetSVGImage(logdoc, root))
					svg->SetURLRelativePart(url.UniRelName());
	}
#endif // SVG_SUPPORT

	current_url = url;
	referrer_url = current_doc_elm->GetReferrerUrl();

	doc->SetCurrentHistoryPos(num, !prev_doc_elm || current_doc_elm->Doc() != prev_doc_elm->Doc(), is_user_initiated);

#ifdef _PRINT_SUPPORT_
	if (was_in_print_preview)
		window->TogglePrintMode(TRUE);
#endif // _PRINT_SUPPORT_

	doc->RecalculateDeviceMediaQueries();

	// Restore document position
	pending_viewport_restore = FALSE;
	RestoreViewport(FALSE, FALSE, doc->IsLocalDocFinished());

	// Restore the :target pseudo selector, but without scrolling the document.
	HTML_Document *html_doc = doc->GetHtmlDocument();
	if (html_doc)
	{
		if (url.GetAttribute(URL::KUniFragment_Name).CStr())
		{
			OpString decoded_fragment;
			OpString original_fragment;
			if (OpStatus::IsSuccess(GetURLFragment(url, decoded_fragment, original_fragment)) &&
				decoded_fragment.CStr())
			{
				doc->SetRelativePos(decoded_fragment.CStr(), original_fragment.CStr(), FALSE);
			}
		}
		else if (html_doc->GetURLTargetElement())
			html_doc->ApplyTargetPseudoClass(NULL);
	}

	if (prev_doc_elm != current_doc_elm) // Send the event only when the position really changed
	{
		if (doc->IsParsed()
#ifdef DELAYED_SCRIPT_EXECUTION
			&& !doc->GetHLDocProfile()->ESIsExecutingDelayedScript()
#endif // DELAYED_SCRIPT_EXECUTION
		   ) // Otherwise HandleLogdocParsingComplete() will take care of the event
			RAISE_IF_MEMORY_ERROR(doc->HandleWindowEvent(ONPOPSTATE, NULL, NULL, 0, NULL));
		else
			doc->SendOnPopStateWhenReady();
	}

	CheckOnNewPageReady();

	RAISE_AND_RETURN_VOID_IF_ERROR(doc->HandleHashChangeEvent(doc_fragment, hist_fragment));

	OP_ASSERT(GetCurrentDoc()->IsCurrentDoc());
}

OP_STATUS DocumentManager::SetHistoryUserData(int history_ID, OpHistoryUserData* user_data)
{
	OP_ASSERT(!GetParentDoc());

	if (history_ID != HistoryIdNotSet)
	{
		DocListElm* dle = FirstDocListElm();
		while (dle)
		{
			if (dle->GetID() == history_ID)
			{
				dle->SetUserData(user_data);
				return OpStatus::OK;
			}
			dle = dle->Suc();
		}
	}
	return OpStatus::ERR;
}

OP_STATUS DocumentManager::GetHistoryUserData(int history_ID, OpHistoryUserData** user_data) const
{
	OP_ASSERT(!GetParentDoc());

	if (user_data == NULL)
	{
		return OpStatus::ERR;
	}

	if (history_ID != HistoryIdNotSet)
	{
		DocListElm* dle = FirstDocListElm();
		while (dle)
		{
			if (dle->GetID() == history_ID)
			{
				*user_data = dle->GetUserData();
				return OpStatus::OK;
			}
			dle = dle->Suc();
		}
	}
	return OpStatus::ERR;
}

void DocumentManager::UnloadCurrentDoc()
{
	if (is_clearing || doc_list.Empty())
		return;

#ifdef _PRINT_SUPPORT_
	if (print_preview_vd)
		window->TogglePrintMode(TRUE);
#endif // _PRINT_SUPPORT_

	// Store document position
	if (current_doc_elm)
	{
		StoreViewport(current_doc_elm);
	}

	DocListElm* prev_doc_elm = current_doc_elm;

	current_doc_elm = NULL;

	BOOL raise_OOM_condition = FALSE;

	current_url.StopLoading(mh);

	if (prev_doc_elm)
	{
		if (FramesDocument* prev_doc = prev_doc_elm->Doc())
		{
			URL prev_doc_url = prev_doc->GetURL();

			prev_doc->SetUnloading(TRUE);

			if (current_url == prev_doc_url)
				if (prev_doc->StopLoading(FALSE) == OpStatus::ERR_NO_MEMORY)
					raise_OOM_condition = TRUE;

			if (prev_doc->Undisplay() == OpStatus::ERR_NO_MEMORY)  // always call Undisplay before SetAsCurrentDoc !!!
				raise_OOM_condition = TRUE;

			if (prev_doc->SetAsCurrentDoc(FALSE) == OpStatus::ERR_NO_MEMORY)
				raise_OOM_condition = TRUE;

			prev_doc->SetUnloading(FALSE);

			if (window->GetOOMOccurred())
				g_memory_manager->FreeDocMemory(ULONG_MAX, TRUE);
			else
				g_memory_manager->CheckDocMemorySize();
		}
	}

	SetCurrentURL(URL(), FALSE);

	if (!GetFrame())
	{
		WindowCommander* wc = window->GetWindowCommander();
		wc->GetLoadingListener()->OnUrlChanged(wc, UNI_L(""));

		window->SetState(CLICKABLE);

		window->DocManager()->UpdateSecurityState(FALSE);

		OpString empty;

		if (OpStatus::IsMemoryError(window->SetWindowTitle(empty, FALSE, FALSE)))
			raise_OOM_condition = TRUE;

#ifdef SHORTCUT_ICON_SUPPORT
		if (OpStatus::IsMemoryError(window->SetWindowIcon(NULL)))
			raise_OOM_condition = TRUE;
#endif // SHORTCUT_ICON_SUPPORT
	}

	GetVisualDevice()->UpdateAll();

	history_walk = DM_NOT_HIST;

	if (raise_OOM_condition && !window->GetOOMOccurred())
		RaiseCondition(OpStatus::ERR_NO_MEMORY);
}

void DocumentManager::FlushHTTPSFromHistory(DocListElm* &prev_doc_elm, DocListElm* current_doc_elm)
{
	BOOL pref_cache_https_after_sessions = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheHTTPSAfterSessions);

	if (!pref_cache_https_after_sessions && prev_doc_elm && current_doc_elm && current_doc_elm != prev_doc_elm)
	{
		const URL prev_url = prev_doc_elm->GetUrl();
		const URL curr_url = current_doc_elm->GetUrl();

		if ((curr_url.Type() != URL_HTTPS && prev_url.Type() == URL_HTTPS) ||
			(curr_url.Type() == URL_HTTPS && prev_url.Type() == URL_HTTPS &&
			// NOTE: No servername is considered false
			!curr_url.SameServer(prev_url)))
		{
			DocListElm *tmp_doc_elm = LastDocListElm();
			while (tmp_doc_elm)
			{
				const URL tmp_url = tmp_doc_elm->GetUrl();

				// Free all entries in history from that server that are HTTPS
				if (tmp_url.Type() == URL_HTTPS &&
#ifdef APPLICATION_CACHE_SUPPORT
					// Application cache urls are never removed from cache, thus the CacheHTTPSAfterSessions pref
					// and application cache is in conflict. We let application cache win.
					!tmp_url.GetAttribute(URL::KIsApplicationCacheURL) &&
#endif // APPLICATION_CACHE_SUPPORT
					!curr_url.SameServer(tmp_url))
				{
					DocListElm *tmp_tmp_doc_elm = tmp_doc_elm;
					tmp_doc_elm = tmp_doc_elm->Pred();

					if (tmp_tmp_doc_elm != current_doc_elm)
					{
						RemoveElementFromHistory(tmp_tmp_doc_elm, FALSE, FALSE);
						prev_doc_elm = NULL;
					}
				}
				else
					tmp_doc_elm = tmp_doc_elm->Pred();
			}
		}
	}
}

FramesDocument* DocumentManager::GetHistoryNext() const
{
	if (current_doc_elm->Suc())
		return current_doc_elm->Suc()->Doc();
	else
		return NULL;
}

FramesDocument* DocumentManager::GetHistoryPrev() const
{
	if (current_doc_elm->Pred())
		return current_doc_elm->Pred()->Doc();
	else
		return NULL;
}

FramesDocument* DocumentManager::GetCurrentDoc() const
{
	if (current_doc_elm)
		return current_doc_elm->Doc();
	else
		return NULL;
}

FramesDocument* DocumentManager::GetCurrentVisibleDoc() const
{
#ifdef _PRINT_SUPPORT_
	if (window->GetPreviewMode())
		return GetPrintDoc();
	else
#endif
		return GetCurrentDoc();
}

#ifdef CLIENTSIDE_STORAGE_SUPPORT
OpStorageManager* DocumentManager::GetStorageManager(BOOL create)
{
	if (parent_doc)
	{
		OP_ASSERT(!"Should use top most storage manager");
		return NULL;
	}
	else if (!m_storage_manager && create)
		m_storage_manager = OpStorageManager::Create();

	return m_storage_manager;
}
void DocumentManager::SetStorageManager(OpStorageManager* new_mgr)
{
	if (parent_doc)
		OP_ASSERT(!"Should use top most DocumentManager");

	if (new_mgr == NULL)
		return;

	//this method must be called when creating the window and doc manager
	//so the data is shared between both windows, and not after the
	//window got its own storage manger
	OP_ASSERT(m_storage_manager == NULL);

	m_storage_manager = new_mgr;
	new_mgr->IncRefCount();
}
#endif // CLIENTSIDE_STORAGE_SUPPORT

void DocumentManager::StoreViewport(DocListElm* doc_elm)
{
	OP_ASSERT(doc_elm);

	FramesDocument* frm_doc = doc_elm->Doc();

	doc_elm->SetLastScale(window->GetScale());
	doc_elm->SetLastLayoutMode(frm_doc->GetLayoutMode());

#ifdef PAGED_MEDIA_SUPPORT
	int current_page_number = -1;

	if (LayoutWorkplace* workplace = frm_doc->GetLayoutWorkplace())
	{
		current_page_number = workplace->GetCurrentPageNumber();
		doc_elm->SetCurrentPage(current_page_number);
		doc_elm->ClearVisualViewport();
	}

	if (current_page_number == -1)
#endif // PAGED_MEDIA_SUPPORT
	{
		doc_elm->SetVisualViewport(frm_doc->GetVisualViewport());

		for (DocumentTreeIterator it(this); it.Next();)
			if (DocListElm* child_doc_elm = it.GetDocumentManager()->CurrentDocListElm())
				if (FramesDocument* child_doc = child_doc_elm->Doc())
					child_doc_elm->SetVisualViewport(child_doc->GetVisualViewport());
	}
}

void DocumentManager::RestoreViewport(BOOL only_pending_restoration, BOOL recurse, BOOL make_fit)
{
	if (!current_doc_elm)
		return;

	FramesDocument* frm_doc = current_doc_elm->Doc();

	if (frm_doc->GetLayoutMode() != current_doc_elm->GetLastLayoutMode())
		current_doc_elm->ClearVisualViewport();

	int negative_overflow = frm_doc->NegativeOverflow();
	OpRect doc_rect(-negative_overflow, 0, negative_overflow + frm_doc->Width(), frm_doc->Height());
	BOOL restoration_failed = FALSE;

#ifdef PAGED_MEDIA_SUPPORT
	int page_number = current_doc_elm->GetCurrentPage();

	if (page_number != -1)
	{
		OP_ASSERT(!current_doc_elm->HasVisualViewport());
		restoration_failed = TRUE;

		if (!only_pending_restoration || pending_viewport_restore)
			if (LayoutWorkplace* workplace = frm_doc->GetLayoutWorkplace())
				if (workplace->SetCurrentPageNumber(page_number, VIEWPORT_CHANGE_REASON_HISTORY_NAVIGATION) || make_fit)
					if (make_fit || workplace->GetCurrentPageNumber() == page_number)
					{
						/* Page was successfully restored. Now clear the stored page number (and
						   stored visual viewport, if set), so that future reflows won't bump us to
						   a stored page. That would be kind of annoying for the user if she has
						   moved to a different page in the meantime. */

						current_doc_elm->SetCurrentPage(-1);
						restoration_failed = FALSE;
					}
	}
	else
#endif // PAGED_MEDIA_SUPPORT
	{
		// Attempt to restore visual viewport

		if (current_doc_elm->HasVisualViewport())
		{
			OpRect viewport = current_doc_elm->GetVisualViewport();
			BOOL has_size = viewport.width > 0 && viewport.height > 0;
			BOOL fits = has_size ? doc_rect.Contains(viewport) : doc_rect.Contains(OpPoint(viewport.x, viewport.y));

			if (make_fit && !fits)
			{
				// Doesn't fit. Try to make it fit. Will fail if the viewport is larger than the document.

				if (viewport.x > doc_rect.x + doc_rect.width - viewport.width)
					viewport.x = doc_rect.x + doc_rect.width - viewport.width;

				if (viewport.y > doc_rect.y + doc_rect.height - viewport.height)
					viewport.y = doc_rect.y + doc_rect.height - viewport.height;

				if (viewport.x < doc_rect.x)
					viewport.x = doc_rect.x;

				if (viewport.y < doc_rect.y)
					viewport.y = doc_rect.y;
			}

			if (make_fit || fits)
			{
				if (!only_pending_restoration || pending_viewport_restore)
				{
					if (has_size)
						frm_doc->RequestSetVisualViewport(viewport, VIEWPORT_CHANGE_REASON_HISTORY_NAVIGATION);
					else
						frm_doc->RequestSetVisualViewPos(viewport.x, viewport.y, VIEWPORT_CHANGE_REASON_HISTORY_NAVIGATION);
					current_doc_elm->ClearVisualViewport();
				}
			}
			else
				restoration_failed = TRUE;
		}

		if (recurse)
			// Restore child frames and iframes

			for (DocumentTreeIterator it(this); it.Next();)
				it.GetDocumentManager()->RestoreViewport(only_pending_restoration, FALSE, make_fit);
	}

	pending_viewport_restore = restoration_failed;
}

OP_STATUS DocumentManager::AddNewHistoryPosition(URL& url, const DocumentReferrer& ref_url, int history_num, const uni_char* doc_title, BOOL make_current /* = FALSE */, BOOL is_plugin /* = FALSE */, FramesDocument* use_existing_doc /* = NULL */, int scale /* = 100 */)
{
	URLStatus url_stat = url.Status(TRUE);

	if (url_stat == URL_EMPTY || url.IsEmpty()) // We are not going to add a doc with an empty URL. Return quietly
		return OpStatus::OK;
	FramesDocument *doc;
	if (use_existing_doc)
		doc = use_existing_doc;
	else
	{
		DocumentOrigin* origin = DocumentOrigin::Make(url);
		if (!origin)
			return OpStatus::ERR_NO_MEMORY;

		doc = OP_NEW(FramesDocument, (this, url, origin, GetSubWinId(), is_plugin));
		origin->DecRef();
		origin = NULL;
	}

	if (doc)
	{
		BOOL replaced_empty = FALSE;
		history_num = history_num != -1 ? history_num : GetNextHistoryNumber(replace, replaced_empty);
		DocListElm* new_dle = OP_NEW(DocListElm, (url, doc, use_existing_doc != doc, history_num, GetNextDocListElmId()));

		if (new_dle)
		{
			if (make_current)
				current_doc_elm = new_dle;

			if (replaced_empty)
				current_doc_elm->SetReplacedEmptyPosition();

			if (doc == doc->GetTopDocument())
				window->SetType(doc->Type());

			InsertHistoryElement(new_dle);

			new_dle->SetReferrerUrl(ref_url, ShouldSendReferrer());

			if (doc_title && *doc_title)
				RETURN_IF_MEMORY_ERROR(new_dle->SetTitle(doc_title));

#ifdef DOCHAND_HISTORY_SAVE_ZOOM_LEVEL
			new_dle->SetLastScale(scale);
#endif // DOCHAND_HISTORY_SAVE_ZOOM_LEVEL

			int win_history_num = window->GetCurrentHistoryNumber();
			if (history_num == win_history_num && !GetWindow()->IsMailOrNewsfeedWindow())
			{
#ifdef PREFS_HOSTOVERRIDE
				// Bug#128887: Set the forced character encoding if a host override exists.
				// Check correctness
				if (g_pcdisplay->IsPreferenceOverridden(PrefsCollectionDisplay::ForceEncoding, url))
				{
					const OpStringC force_encoding(g_pcdisplay->GetStringPref(PrefsCollectionDisplay::ForceEncoding, url));
					if (!force_encoding.IsEmpty())
					{
						char sbencoding[128]; /* ARRAY OK 2008-02-28 jl */
						uni_cstrlcpy(sbencoding, force_encoding.CStr(), ARRAY_SIZE(sbencoding));
						window->SetForceEncoding(sbencoding, TRUE);
					}
				}
#endif // PREFS_HOSTOVERRIDE

				RETURN_IF_MEMORY_ERROR(window->UpdateTitle());
			}

			history_len++;
		}
		else
			return OpStatus::ERR_NO_MEMORY;
	}
	else
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

void DocumentManager::SetShowImg(BOOL val)
{
#ifdef _PRINT_SUPPORT_
	BOOL was_in_print_preview = FALSE;
	if (print_preview_vd)
	{
		window->TogglePrintMode(TRUE);
		was_in_print_preview = TRUE;
	}
#endif // _PRINT_SUPPORT_

	if (!current_doc_elm)
		return;

	FramesDocument* doc = current_doc_elm->Doc();

	if (doc)
		doc->SetMode(val, window->LoadImages(), window->GetCSSMode(), check_expiry);

#ifdef _PRINT_SUPPORT_
	if (was_in_print_preview)
		window->TogglePrintMode(TRUE);
#endif // _PRINT_SUPPORT_
}

OP_STATUS DocumentManager::ReformatCurrentDoc()
{
#ifdef _PRINT_SUPPORT_
	BOOL was_in_print_preview = FALSE;

	if (print_preview_vd)
	{
		window->TogglePrintMode(TRUE);
		was_in_print_preview = TRUE;
	}
#endif // _PRINT_SUPPORT_

	if (!current_doc_elm)
		return OpStatus::OK;

	FramesDocument* doc = current_doc_elm->Doc();
	LayoutWorkplace* wp = doc->GetLogicalDocument() ? doc->GetLogicalDocument()->GetLayoutWorkplace() : NULL;

	if (doc->IsUndisplaying())
		return OpStatus::OK;

	if (wp)
		wp->SetCanYield(TRUE);

	if (OpStatus::IsMemoryError(doc->Reflow(TRUE, FALSE)))
	{
		if (wp)
			wp->SetCanYield(FALSE);

		return OpStatus::ERR_NO_MEMORY;
	}

	if (wp)
	{
		wp->SetCanYield(FALSE);
		doc->GetHLDocProfile()->GetCSSCollection()->StyleChanged(CSSCollection::CHANGED_MEDIA_QUERIES);
	}

#ifdef _PRINT_SUPPORT_
	if (was_in_print_preview)
		window->TogglePrintMode(TRUE); // rg this one doesn't work very well (if at all)
#endif // _PRINT_SUPPORT_

	return OpStatus::OK;
}

OP_STATUS DocumentManager::ReformatDoc(FramesDocument* doc)
{
#ifdef _PRINT_SUPPORT_
    BOOL was_in_print_preview = FALSE;

    if (print_preview_vd)
    {
		window->TogglePrintMode(TRUE);
        was_in_print_preview = TRUE;
    }
#endif // _PRINT_SUPPORT_

    LayoutWorkplace* wp = doc->GetLogicalDocument() ? doc->GetLogicalDocument()->GetLayoutWorkplace() : NULL;
	
    if (wp)
        wp->SetCanYield(TRUE);
	
	HTML_Element* root = doc->GetDocRoot();
	if (root)
		root->MarkPropsDirty(doc);
	
    if (wp)
        wp->SetCanYield(FALSE);
	
#ifdef _PRINT_SUPPORT_
    if (was_in_print_preview)
        window->TogglePrintMode(TRUE); // rg this one doesn't work very well (if at all)
#endif // _PRINT_SUPPORT_
	
    return OpStatus::OK;
}

DocListElm* DocumentManager::FindDocListElm(FramesDocument *doc) const
{
	DocListElm *dle;
	for (dle = current_doc_elm; dle; dle = (DocListElm *) dle->Suc())
		if (dle->Doc() == doc)
			return dle;
	for (dle = current_doc_elm; dle; dle = (DocListElm *) dle->Pred())
		if (dle->Doc() == doc)
			return dle;
	return NULL;
}

void DocumentManager::UpdateVisitedLinks(const URL& url)
{
#ifdef _PRINT_SUPPORT_
	if (print_preview_vd)
		return;
#endif // _PRINT_SUPPORT_

	if (current_doc_elm && current_doc_elm->Doc())
	{
		URL current_doc_url = current_doc_elm->GetUrl();
		URL doc_url = current_doc_elm->GetUrl().GetAttribute(URL::KMovedToURL, TRUE);
		if (doc_url.IsEmpty())
			doc_url = current_doc_url;

		if (!(doc_url == url) || GetLoadStatus() == NOT_LOADING)
		   	current_doc_elm->Doc()->UpdateLinkVisited();
	}
}

void DocumentManager::CancelRefresh()
{
	if (waiting_for_refresh_id)
	{
		mh->RemoveDelayedMessage(MSG_START_REFRESH_LOADING, static_cast<MH_PARAM_1>(waiting_for_refresh_id), GetSubWinId());
		activity_refresh.Cancel();
	}

	waiting_for_refresh_id = 0;
}

void DocumentManager::SetRefreshDocument(URL_ID url_id, unsigned long delay)
{
	if (waiting_for_refresh_id != url_id)
	{
		if (waiting_for_refresh_id)
			mh->RemoveDelayedMessage(MSG_START_REFRESH_LOADING, static_cast<MH_PARAM_1>(waiting_for_refresh_id), GetSubWinId());

#ifdef _AUTO_WIN_RELOAD_SUPPORT_
		// Fix for bug #153586 - User defined reload setting overrides page refresh
		if (window->GetUserAutoReload()->GetOptIsEnabled())
		{
			waiting_for_refresh_id = 0;
			return;
		}
#endif // _AUTO_WIN_RELOAD_SUPPORT_

		waiting_for_refresh_id = url_id;
		waiting_for_refresh_delay = delay;

		if (waiting_for_refresh_id)
		{
			mh->PostDelayedMessage(MSG_START_REFRESH_LOADING, static_cast<MH_PARAM_1>(url_id), GetSubWinId(), delay);
			activity_refresh.Begin();
		}
	}
}

void DocumentManager::Refresh(URL_ID id)
{
	OpAutoActivity local_activity_refresh(ACTIVITY_METAREFRESH);

	if (id != waiting_for_refresh_id)
		return;

	waiting_for_refresh_id = 0;

#ifdef _PRINT_SUPPORT_
	if (print_preview_vd)
	{
		pending_refresh_id = id;
		return;
	}
#endif // _PRINT_SUPPORT_

	FramesDocument* doc = GetCurrentDoc();

	if (doc)
	{
		URL url = doc->GetRefreshURL(id);
		BOOL is_redirecting = doc->GetURL().Id(TRUE) != url.Id(TRUE);
		BOOL reload = !is_redirecting && url.GetAttribute(URL::KUniFragment_Name).IsEmpty();
		BOOL allow_refresh = window->GetWindowCommander()->GetDocumentListener()->OnRefreshUrl(window->GetWindowCommander(), url.GetAttribute(URL::KUniName_Username_Password_Hidden, TRUE).CStr());
		if (!allow_refresh && !is_redirecting)
			return;

		SetUserAutoReload(FALSE);
		if (is_redirecting)
			SetLoadStatus(WAIT_FOR_HEADER);

		if (!url.IsEmpty())
		{
			if(url.Type() == URL_HTTP || url.Type() == URL_HTTPS)
				url.SetHTTP_Method(HTTP_METHOD_GET);

			DocumentReferrer ref_url = GenerateReferrerURL();
			StopLoading(FALSE);

			if (reload)
			{
				SetReload(TRUE);
				SetReloadFlags(TRUE, FALSE, TRUE, TRUE);
			}
#ifdef SKIP_REFRESH_ON_BACK
			else if (waiting_for_refresh_delay < SKIP_REFRESH_SECONDS * 1000)
				/* The current document (the source of the redirect) should be
				   skipped in the history.  We accomplish this by reusing its
				   history number for the new document.  This has the side-
				   effect that the old document is automatically removed when
				   replaced, and we also avoid holes in the history (which we
				   might get if we just remove the old history position.) */
				SetUseHistoryNumber(current_doc_elm->Number());
#endif // SKIP_REFRESH_ON_BACK

            SetRedirect(is_redirecting);
			OpenURL(url, ref_url, TRUE, reload);
		}
	}
}

OP_STATUS DocumentManager::HandleDataLoaded(URL_ID url_id)
{
	OP_PROBE5(OP_PROBE_DOCUMENTMANAGER_HANDLEDATALOADED);

	OP_STATUS status = OpStatus::OK;
	OpStatus::Ignore(status);

	if (current_url.Id(TRUE) == url_id)
		use_current_doc = FALSE;

	DM_LoadStat ls = GetLoadStatus();

	if (ls == WAIT_FOR_HEADER || ls == WAIT_FOR_ACTION)
	{
		status = HandleHeaderLoaded(url_id, FALSE);
		ls = GetLoadStatus();
	}

	FramesDocument* doc = GetCurrentDoc();

	if (ls == DOC_CREATED || ls == NOT_LOADING || ls == WAIT_MULTIPART_RELOAD && url_id != current_url.Id(TRUE))
	{
		// dispatch to document

		if (doc)
			status = doc->HandleLoading(MSG_URL_DATA_LOADED, url_id, 0);
	}
	else if (ls != WAIT_FOR_ECMASCRIPT && current_url.Id(TRUE) == url_id && current_url.Status(TRUE) != URL_LOADING)
		HandleAllLoaded(url_id);

	if (current_url.Status(TRUE) != URL_LOADING && ls == NOT_LOADING && (!doc || doc->IsLoaded()))
		EndProgressDisplay();

	return status;
}

#ifdef WEB_HANDLERS_SUPPORT
class WebHandlerCallback : public OpDocumentListener::WebHandlerCallback
{
	DocumentManager* doc_man;
	URL target_url;
	URL current_url;
	DocumentReferrer referrer;
	BOOL user_initiated;
	BOOL entered_by_user;
	const uni_char* description;
	OpString8 protocol_or_mime_type;
	BOOL show_never_ask_me_again;
	BOOL content_handler;
	OpString target_host;

public:
	WebHandlerCallback(DocumentManager* doc_man, URL& url, DocumentReferrer& ref, URL& target_url, BOOL user_init, BOOL user_entered, const uni_char *description, BOOL show_dont_ask_me_again = TRUE, BOOL content_handler = TRUE)
	: doc_man(doc_man)
	, target_url(target_url)
	, current_url(url)
	, referrer(ref)
	, user_initiated(user_init)
	, entered_by_user(user_entered)
	, description(description)
	, show_never_ask_me_again(show_dont_ask_me_again)
	, content_handler(content_handler)
	{
	}

	virtual ~WebHandlerCallback() {}

	OP_STATUS Construct(const char* protocol_or_mime_type)
	{
		RETURN_IF_ERROR(this->protocol_or_mime_type.Set(protocol_or_mime_type));
		return target_url.GetAttribute(URL::KUniHostName, target_host);
	}

	virtual void OnOK(BOOL dont_ask_again, const uni_char* filename)
	{
		if (content_handler)
		{
			Viewer* viewer = g_viewers->FindViewerByMimeType(protocol_or_mime_type);
			if (viewer && dont_ask_again)
			{
				viewer->SetFlag(ViewActionFlag::USERDEFINED_VALUE);
			}

			if (viewer && filename)
			{
				if (!*filename)
				{
					viewer->ResetAction(dont_ask_again);
				}
				else
				{
					viewer->SetAction(VIEWER_APPLICATION);

					if (OpStatus::IsMemoryError(viewer->SetApplicationToOpenWith(filename)))
						goto raise_oom_condition;
				}

				doc_man->OpenURL(current_url, referrer, TRUE, FALSE, user_initiated, FALSE, entered_by_user ? WasEnteredByUser : NotEnteredByUser);
			}
			else
			{
				doc_man->OpenURL(target_url, DocumentReferrer(current_url), TRUE, FALSE, user_initiated, FALSE, entered_by_user ? WasEnteredByUser : NotEnteredByUser);
			}

#if defined PREFS_HAS_PREFSFILE && defined PREFS_WRITE
			if (dont_ask_again)
			{
				TRAPD(exception, g_viewers->WriteViewersL());
				if (OpStatus::IsMemoryError(exception))
					goto raise_oom_condition;
			}
#endif // defined PREFS_HAS_PREFSFILE && defined PREFS_WRITE
		}
		else
		{
			OpString protocol;
			if (OpStatus::IsMemoryError(protocol.SetFromUTF8(current_url.GetAttribute(URL::KProtocolName).CStr())))
				goto raise_oom_condition;

			TrustedProtocolData data;
			int index = g_pcdoc->GetTrustedProtocolInfo(protocol, data);
			if (index != -1)
			{
				if (dont_ask_again)
				{
					data.user_defined = TRUE;
					data.flags |= TrustedProtocolData::TP_UserDefined;
				}

				if (filename)
				{
					if (*filename)
					{
						data.viewer_mode = UseCustomApplication;
						data.filename    = filename;
						data.flags |= (TrustedProtocolData::TP_Filename|TrustedProtocolData::TP_ViewerMode);
					}
					else
					{
						data.viewer_mode = UseDefaultApplication;
						data.flags |= TrustedProtocolData::TP_ViewerMode;
					}
				}

				TRAPD(exception, g_pcdoc->SetTrustedProtocolInfoL(index, data));
				if (OpStatus::IsMemoryError(exception))
					goto raise_oom_condition;

#if defined PREFS_HAS_PREFSFILE && defined PREFS_WRITE
				if (dont_ask_again)
				{
					TRAP(exception, g_pcdoc->WriteTrustedProtocolsL(g_pcdoc->GetNumberOfTrustedProtocols()));
					if (OpStatus::IsMemoryError(exception))
						goto raise_oom_condition;
				}
#endif // defined PREFS_HAS_PREFSFILE && defined PREFS_WRITE
			}

			if (filename)
			{
				doc_man->OpenURL(current_url, referrer, TRUE, FALSE, user_initiated, FALSE, entered_by_user ? WasEnteredByUser : NotEnteredByUser);
			}
			else
			{
				doc_man->OpenURL(target_url, referrer, TRUE, FALSE, user_initiated, FALSE, entered_by_user ? WasEnteredByUser : NotEnteredByUser);
			}
		}

		OP_DELETE(this);
		return;

raise_oom_condition:
		doc_man->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		OP_DELETE(this);
		return;
	}

	virtual void OnCancel()
	{
		OP_DELETE(this);
	}

	virtual const uni_char* GetDescription() const { return description; }
	virtual const char* GetMimeTypeOrProtocol() const { return protocol_or_mime_type; }
	const uni_char* GetTargetHostName() const { return target_host.CStr(); }
	virtual BOOL ShowNeverAskMeAgain() const { return show_never_ask_me_again; }
	virtual BOOL IsProtocolHandler() const { return !content_handler; }
};
#endif // WEB_HANDLERS_SUPPORT

OP_STATUS DocumentManager::HandleHeaderLoaded(URL_ID url_id, BOOL check_if_inline_expired)
{
	BOOL pref_suppress_external_embeds = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::SuppressExternalEmbeds);

	if (current_url.IsEmpty() || current_url.Id(TRUE) != url_id || GetLoadStatus() == NOT_LOADING)
	{
		// check frames
		if (FramesDocument* doc = GetCurrentDoc())
			return doc->HandleLoading(MSG_HEADER_LOADED, url_id, check_if_inline_expired);

		return OpStatus::OK;
	}

	if (GetLoadStatus() == WAIT_FOR_HEADER || GetLoadStatus() == WAIT_FOR_ACTION)
	{
		BOOL update_this_window = TRUE;

		URL moved_url = current_url.GetAttribute(URL::KMovedToURL, TRUE);

		if (!moved_url.IsEmpty() && !(moved_url == current_url))
		{
			if ((current_url.Type() == URL_HTTPS || moved_url.Type() == URL_HTTPS) // only redirects to/from HTTPS
				&& user_started_loading // only if the user started the loading
				&& GetParentDoc() == NULL) // only for the top document
			{
				window->ResetSecurityStateToUnknown();
			}

#ifdef HISTORY_SUPPORT
			OP_STATUS ostat = GetWindow()->AddToGlobalHistory(current_url,
							current_url.GetAttribute(URL::KUniName).CStr());
			if(OpStatus::IsFatal(ostat))
				return ostat;
#endif // HISTORY_SUPPORT

			GetWindow()->ClearMovedUrls();

			if (current_url.UniRelName())
				// If a URL we were redirected to has a relative part - use it.
				SetCurrentURL(URL(moved_url, ((moved_url.UniRelName()) ? moved_url.UniRelName() : current_url.UniRelName())), TRUE);
			else
				SetCurrentURL(moved_url, TRUE);
		}

		window->SetState(CLICKABLE);

		if (!window->GetPrivacyMode())
			current_url.Access(TRUE);

		const uni_char* app = NULL;
		RETURN_IF_MEMORY_ERROR(UpdateAction(app));

		ViewAction action = GetAction();
		if (action == VIEWER_WAIT_FOR_DATA)
		{
			SetAction(VIEWER_NOT_DEFINED);
			SetLoadStat(WAIT_FOR_ACTION);
			return OpStatus::OK;
		}

#ifdef WEB_HANDLERS_SUPPORT
		BOOL force_ask_user = FALSE;
		if (action == VIEWER_WEB_APPLICATION)
		{
			BOOL allowed = TRUE;
			if (current_url.Type() == URL_HTTPS)
			{
				URL target_url;
				if (app && *app)
					target_url = g_url_api->GetURL(current_url, app);

				OpStatus::Ignore(OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_STANDARD,
				                                      OpSecurityContext(current_url),
				                                      OpSecurityContext(target_url),
				                                      allowed));
			}

			// Check against cascades/loops.
			Viewer* viewer;
			RETURN_IF_MEMORY_ERROR(g_viewers->FindViewerByWebApplication(current_url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_Escaped_NOT_FOR_UI).CStr(), viewer));
			if (!allowed || viewer || current_url.GetAttribute(URL::KHTTP_Method) != HTTP_METHOD_GET)
			{
				RETURN_IF_MEMORY_ERROR(g_viewers->FindViewerByURL(current_url, viewer, TRUE));
				OP_ASSERT(viewer);
				SetAction(Viewers::GetDefaultAction(viewer->GetContentTypeString8()));
				SetActionLocked(TRUE);
				action = GetAction();
			}
			else
			{
				OpSocketAddressNetType net_type = NETTYPE_UNDETERMINED;
				if (current_url.Status(TRUE) == URL_LOADED)
					net_type = static_cast<OpSocketAddressNetType>(current_url.GetAttribute(URL::KLoadedFromNetType));
				else
				{
					ServerName* server_name = current_url.GetServerName();
					if (server_name)
						net_type = server_name->GetNetType();
				}

				force_ask_user = (net_type == NETTYPE_LOCALHOST || net_type == NETTYPE_PRIVATE || net_type == NETTYPE_UNDETERMINED);
			}
		}
#endif // WEB_HANDLERS_SUPPORT

		BOOL out_of_memory;
		OP_STATUS stat = OpStatus::OK;
		OpStatus::Ignore(stat);

		// suppress the showing of mail and news content that does not use Opera as viewer (stighal, 2002-04-26)
		if (pref_suppress_external_embeds
			&& (action != VIEWER_OPERA && action != VIEWER_SAVE)
			&& window->IsSuppressWindow())
		{
			use_current_doc = FALSE;
			return OpStatus::OK;
		}

		if (load_image_only || save_image_only)
			if (!current_url.IsImage())
			{
				StopLoading(FALSE);
				return OpStatus::OK;
			}

		if (save_image_only)
			action = VIEWER_SAVE;

#ifdef MEDIA_HTML_SUPPORT
		// suppress displaying audio/video if the media backend does not support it;
		// user will have to select action.
		if (current_url.ContentType() == URL_MEDIA_CONTENT && action == VIEWER_OPERA)
		{
			if (!g_media_module.IsMediaPlayableUrl(current_url))
				action = VIEWER_ASK_USER;
		}
#endif

		switch (action)
		{
#ifndef NO_SAVE_SUPPORT
		case VIEWER_SAVE:
			{
				const OpStringC8 mime_type = current_url.GetAttribute(URL::KMIME_Type, TRUE);
				Viewer* viewer = g_viewers->FindViewerByMimeType(mime_type);

				SetLoadStat(WAIT_FOR_USER);

#ifndef WIC_USE_DOWNLOAD_CALLBACK
				OpString url_name, suggested_filename;
				current_url.GetAttribute(URL::KUniName_Username_Password_Hidden, url_name, TRUE);
				if (OpStatus::IsError(current_url.GetAttribute(URL::KSuggestedFileName_L, suggested_filename, TRUE)) ||
				    url_name.IsEmpty() ||
				    suggested_filename.IsEmpty())
				{
					return OpStatus::ERR_NO_MEMORY;
				}
#endif // WIC_USE_DOWNLOAD_CALLBACK

				if(viewer)
				{
# if defined (SDK)
#  ifdef GTK_DOWNLOAD_EXTENSION
					ServerName* sname = current_url.GetServerName();
					unsigned short port = current_url.GetServerPort();

					AuthElm* auth = sname->Get_Auth(NULL, (port == 0) ? 80 : port, current_url.GetPath(), current_url.Type(), 0);
					char *auth_credentials = NULL;
					if (auth)
						OP_STATUS status = auth->GetAuthString(&auth_credentials, current_url.GetRep());

					const uni_char* suggested_filename = (viewer->GetFlags() & VIEWER_FLAG_SAVE_DIRECTLY) ? viewer->GetSaveToFolder() : NULL;

					GtkWidget* ow = GTK_WIDGET(((GdkOpWindow*)GetWindow()->GetOpWindow())->GetGtkWidget());
					OP_ASSERT(ow);
					if (ow)
					{
						gchar* suggested_filename_utf8;

						if (suggested_filename)
							suggested_filename_utf8 = g_utf16_to_utf8(suggested_filename, -1, NULL, NULL, NULL);
						else
							suggested_filename_utf8 = g_strdup("");

						GtkOperaDownloadExtension* download_extension = GTK_OPERA_DOWNLOAD_EXTENSION(gtk_operawidget_get_extension( GTK_OPERA_WIDGET(ow), GTK_OPERA_DOWNLOAD_EXTENSION_TYPE ));

						if (suggested_filename_utf8)
						{
							if (download_extension)
								g_signal_emit(G_OBJECT(download_extension), gtk_opera_download_extension_signals[GTK_OPERA_DOWNLOAD_EXTENSION_REQUEST], 0,
								              current_url.Name(),
								              suggested_filename_utf8,
								              current_url.GetAttribute(URL::KMIME_Type),
								              auth_credentials,
								              0,
								              (viewer->GetFlags() & VIEWER_FLAG_SAVE_DIRECTLY) ? OpDocumentListener::SAVE : OpDocumentListener::ASK_USER);

							g_free(suggested_filename_utf8);
						}
						else
							RaiseCondition(OpStatus::ERR_NO_MEMORY);
					}

					OP_DELETEA(auth_credentials);
#  else // GTK_DOWNLOAD_EXTENSION
					window->GetWindowCommander()->GetDocumentListener()->OnDownloadRequest(
						window->GetWindowCommander(),
						current_url.GetUniName(TRUE, PASSWORD_HIDDEN),
						(viewer->GetFlags() & VIEWER_FLAG_SAVE_DIRECTLY) ? viewer->GetSaveToFolder() : NULL,
						current_url.GetAttribute(URL::KMIME_Type),
						0,
						(viewer->GetFlags() & VIEWER_FLAG_SAVE_DIRECTLY) ? OpDocumentListener::SAVE : OpDocumentListener::ASK_USER);
#  endif // GTK_DOWNLOAD_EXTENSION
# else // SDK
#  if defined (WIC_USE_DOWNLOAD_CALLBACK)
					TransferManagerDownloadCallback * t_tmdc = OP_NEW(TransferManagerDownloadCallback, (this, current_url, viewer, current_action_flag));
                    if (!t_tmdc)
                        return OpStatus::ERR_NO_MEMORY;

                    WindowCommander * wc = window->GetWindowCommander();
                    wc->GetDocumentListener()->OnDownloadRequest(wc, t_tmdc);
                    OP_BOOLEAN finished = t_tmdc->Execute();

                    current_action_flag.Unset(ViewActionFlag::SAVE_AS);

                    if(finished == OpBoolean::IS_FALSE)
                        SetLoadStat(WAIT_FOR_USER);
                    else
                        return finished;
                    break;
#  else // !WIC_USE_DOWNLOAD_CALLBACK
					BOOL can_do = FALSE;
					if (!current_action_flag.IsSet(ViewActionFlag::SAVE_AS)
						&& viewer->GetFlags().IsSet(ViewActionFlag::SAVE_DIRECTLY))
						OpStatus::Ignore(SetSaveDirect(viewer, can_do));
					if (!can_do)
					{
						current_action_flag.Unset(ViewActionFlag::SAVE_AS);
						window->GetWindowCommander()->GetDocumentListener()->OnDownloadRequest(
							window->GetWindowCommander(),
							url_name.CStr(),
							suggested_filename.CStr(),
							current_url.GetAttribute(URL::KMIME_Type, TRUE).CStr(),
							current_url.GetContentSize(),
							(viewer->GetFlags().IsSet(ViewActionFlag::SAVE_DIRECTLY)) ? OpDocumentListener::SAVE : OpDocumentListener::ASK_USER,
							(viewer->GetFlags().IsSet(ViewActionFlag::SAVE_DIRECTLY)) ? viewer->GetSaveToFolder() : NULL);
					}
					else
					{
						current_action_flag.Set(ViewActionFlag::SAVE_DIRECTLY);
						SetLoadStatus(WAIT_FOR_LOADING_FINISHED);
						if (current_url.Status(TRUE) != URL_LOADING)
							HandleAllLoaded(url_id);
						break;
					}
#  endif // !WIC_USE_DOWNLOAD_CALLBACK
# endif // SDK
				}
				else
				{
# if defined (SDK)
#  ifdef GTK_DOWNLOAD_EXTENSION
					ServerName* sname = current_url.GetServerName();
					unsigned short port = current_url.GetServerPort();

					AuthElm* auth = sname->Get_Auth(NULL, (port == 0) ? 80 : port, current_url.GetPath(), current_url.Type(), 0);
					char *auth_credentials = NULL;
					if (auth)
						OP_STATUS status = auth->GetAuthString(&auth_credentials, current_url.GetRep());

					GtkWidget* ow = GTK_WIDGET(((GdkOpWindow*)GetWindow()->GetOpWindow())->GetGtkWidget());
					OP_ASSERT(ow);
					if (ow)
					{
						GtkOperaDownloadExtension* download_extension = GTK_OPERA_DOWNLOAD_EXTENSION(gtk_operawidget_get_extension( GTK_OPERA_WIDGET(ow), GTK_OPERA_DOWNLOAD_EXTENSION_TYPE ));

						if (download_extension)
							g_signal_emit(G_OBJECT(download_extension), gtk_opera_download_extension_signals[GTK_OPERA_DOWNLOAD_EXTENSION_REQUEST], 0,
										  current_url.Name(),
										  NULL,
										  current_url.GetAttribute(URL::KMIME_Type),
										  auth_credentials,
										  0,
										  OpDocumentListener::UNKNOWN);
					}
					OP_DELETEA(auth_credentials);
#  else // GTK_DOWNLOAD_EXTENSION
					window->GetWindowCommander()->GetDocumentListener()->OnDownloadRequest(
						window->GetWindowCommander(),
						current_url.GetUniName(TRUE,PASSWORD_HIDDEN),
						NULL,
						current_url.GetAttribute(URL::KMIME_Type),
						0,
						OpDocumentListener::UNKNOWN);
#  endif // GTK_DOWNLOAD_EXTENSION
# else // SDK
#  if defined (WIC_USE_DOWNLOAD_CALLBACK)
					TransferManagerDownloadCallback * t_tmdc = OP_NEW(TransferManagerDownloadCallback, (this, current_url, viewer, current_action_flag));
                    if (!t_tmdc)
                        return OpStatus::ERR_NO_MEMORY;

                    WindowCommander * wc = window->GetWindowCommander();
                    wc->GetDocumentListener()->OnDownloadRequest(wc, t_tmdc);
                    OP_BOOLEAN finished = t_tmdc->Execute();
                    if(finished == OpBoolean::IS_FALSE)
                        SetLoadStat(WAIT_FOR_USER);
                    else
                    	return finished;
                    break;  // Note: cleaning up window/docman must wait until another
                            // entity takes over the download, or not.
#  else // !WIC_USE_DOWNLOAD_CALLBACK
					window->GetWindowCommander()->GetDocumentListener()->OnDownloadRequest(
						window->GetWindowCommander(),
						url_name.CStr(),
						suggested_filename.CStr(),
						current_url.GetAttribute(URL::KMIME_Type).CStr(),
						current_url.GetContentSize(),
						OpDocumentListener::UNKNOWN,
						NULL);
#  endif // !WIC_USE_DOWNLOAD_CALLBACK
# endif // SDK
				}

				StopLoading(FALSE);	//the window is not handling the loading anymore

				if (current_url.Status(TRUE) != URL_LOADING)
					HandleAllLoaded(url_id);
			}
			break;
#endif // !NO_SAVE_SUPPORT

		case VIEWER_PASS_URL:
			{
#ifndef NO_EXTERNAL_APPLICATIONS
				if (app && *app)
				{
					current_url.StopLoading(mh);

					stat = g_op_system_info->ExecuteApplication(app,
						current_url.GetAttribute(URL::KUniName_Username_Password_Escaped_NOT_FOR_UI).CStr()); // PASSWORD_SHOW_VERBATIM accepted provisionally
				}
#endif // !NO_EXTERNAL_APPLICATIONS

				StopLoading(FALSE);
				EndProgressDisplay();
			}
			break;

		case VIEWER_APPLICATION:
#ifndef NO_EXTERNAL_APPLICATIONS
			if (app && *app)
			{
				OpStatus::Ignore(SetTempDownloadFolder());
				SetLoadStat(WAIT_FOR_LOADING_FINISHED);
				SetApplication(app);
				if (current_url.Status(TRUE) != URL_LOADING)
					HandleAllLoaded(url_id);
			}
			else
			{
# ifdef WIC_USE_DOWNLOAD_CALLBACK
				TransferManagerDownloadCallback * t_tmdc = OP_NEW(TransferManagerDownloadCallback, (this, current_url, NULL, current_action_flag));
                if (!t_tmdc)
					return OpStatus::ERR_NO_MEMORY;

                window->GetWindowCommander()->GetDocumentListener()->OnDownloadRequest(
                        window->GetWindowCommander(), t_tmdc);

                OP_BOOLEAN finished = t_tmdc->Execute();

				if(finished == OpBoolean::IS_FALSE)
                    SetLoadStat(WAIT_FOR_USER);

# else  // !WIC_USE_DOWNLOAD_CALLBACK
				SetLoadStat(WAIT_FOR_USER);
				window->AskAboutUnknownDoc(current_url, GetSubWinId());
# endif // !WIC_USE_DOWNLOAD_CALLBACK
			}
#endif // !NO_EXTERNAL_APPLICATIONS
			break;

		case VIEWER_REG_APPLICATION:
		case VIEWER_RUN:
#ifndef NO_EXTERNAL_APPLICATIONS
			OpStatus::Ignore(SetTempDownloadFolder());
			SetLoadStat(WAIT_FOR_LOADING_FINISHED);
			if (current_url.Status(TRUE) != URL_LOADING)
				HandleAllLoaded(url_id);
#endif // !NO_EXTERNAL_APPLICATIONS
			break;

#ifdef WEB_HANDLERS_SUPPORT
		case VIEWER_WEB_APPLICATION:
			if (app && *app)
			{
				Viewer* viewer;
				RETURN_IF_ERROR(g_viewers->FindViewerByURL(current_url, viewer, TRUE));
				OP_ASSERT(viewer);

				TempBuffer8 parameter_url8;
				RETURN_IF_ERROR(parameter_url8.AppendURLEncoded(current_url.GetAttribute(URL::KName_With_Fragment_Escaped)));
				OpString parameter_url16;
				RETURN_IF_ERROR(parameter_url16.SetFromUTF8(parameter_url8.GetStorage()));

				OpString target_url_str;
				RETURN_IF_ERROR(target_url_str.Set(app));
				RETURN_IF_ERROR(target_url_str.ReplaceAll(UNI_L("%s"), parameter_url16.CStr(), 1));
				URL target_url = g_url_api->GetURL(current_url, target_url_str.CStr());

				if (!viewer->IsUserDefined() || force_ask_user)
				{
					OpString8 mime;
					RETURN_IF_ERROR(current_url.GetAttribute(URL::KMIME_Type, mime, TRUE));
					WebHandlerCallback* cb = OP_NEW(WebHandlerCallback, (this, current_url, referrer_url, target_url, user_initiated_loading, user_started_loading, viewer->GetDescription(), !force_ask_user));
					RETURN_OOM_IF_NULL(cb);
					RETURN_IF_ERROR(cb->Construct(mime));
					window->GetWindowCommander()->GetDocumentListener()->OnWebHandler(window->GetWindowCommander(), cb);

					StopLoading(FALSE, TRUE);
				}
				else // Open without asking
					OpenURL(target_url, DocumentReferrer(current_url), TRUE, FALSE, user_initiated_loading, FALSE, user_started_loading ? WasEnteredByUser : NotEnteredByUser);
			}
			break;
#endif // WEB_HANDLERS_SUPPORT

		case VIEWER_PLUGIN:
		case VIEWER_OPERA:
			if (es_pending_unload)
			{
				SetLoadStat(WAIT_FOR_ECMASCRIPT);
				return ESSendPendingUnload(check_if_inline_expired, action == VIEWER_PLUGIN);
			}

#if defined(SUPPORT_VERIZON_EXTENSIONS) && defined(WIC_ORIENTATION_CHANGE)
			{
				OpString8 verizonHeader;
				verizonHeader.Set( "x-verizon-content");
				current_url.GetAttribute( URL::KHTTPSpecificResponseHeaderL, verizonHeader, TRUE);
				if ( verizonHeader.CompareI( "portrait") == 0)
					window->GetWindowCommander()->GetDocumentListener()->OnOrientationChangeRequest( window->GetWindowCommander(), OpDocumentListener::PORTRAIT);
				else if ( verizonHeader.CompareI( "landscape") == 0)
					window->GetWindowCommander()->GetDocumentListener()->OnOrientationChangeRequest( window->GetWindowCommander(), OpDocumentListener::LANDSCAPE);
			}
#endif //SUPPORT_VERIZON_EXTENSIONS && WIC_ORIENTATION_CHANGE

			update_this_window = !HandleByOpera(check_if_inline_expired, action == VIEWER_PLUGIN, FALSE, &out_of_memory);
			if (out_of_memory)
				return OpStatus::ERR_NO_MEMORY;
			break;

		case VIEWER_IGNORE_URL:
			//just stop Opera's handling of this URL, no more work required from us
			HandleAllLoaded(current_url.Id(TRUE)); // Clean up and inform everyone else.
			StopLoading(FALSE, TRUE); // force end-progress

#ifdef SVG_SUPPORT
			// notify svg manager that the element will be ignored
			{
				HTML_Element* embedding_element = GetFrame() ? GetFrame()->GetHtmlElement() : NULL;
				if (embedding_element && embedding_element->GetNsType() == NS_SVG)
					g_svg_manager->HandleInlineIgnored(GetParentDoc(), embedding_element);
			}
#endif // SVG_SUPPORT

			break;

		case VIEWER_NOT_DEFINED:
			{
                OpString url_name;
                current_url.GetAttribute(URL::KUniName, url_name);

                WindowCommander *wc = GetWindow()->GetWindowCommander();
                wc->GetDocumentListener()->OnUnhandledFiletype(wc, url_name.CStr());

				StopLoading(FALSE, TRUE); // force end-progress
			}
			break;

		case VIEWER_ASK_USER:
			// Allow one (1) "ask_user" that is not caused by a user. Replace any others
			// with a page with a link to the url (ref bug DSK-160957, DSK-172632 and CORE-9298)
			if (!user_initiated_loading)
			{
				if (GetParentDoc() &&
					GetWindow()->HasShownUnsolicitedDownloadDialog()) // Already consumed its chance
				{
					URL parent_frame_url = GetParentDoc()->GetURL();

					URL link_to_content = g_url_api->GetNewOperaURL();
					link_to_content.Unload();

					// Create the placeholder document
					OpSuppressedURL placeholder(link_to_content, current_url, TRUE);
					RETURN_IF_ERROR(placeholder.GenerateData());

					OpenURLOptions options;
					BOOL check_if_expired = FALSE;
					BOOL reload = FALSE;
					OpenURL(link_to_content, GetParentDoc(), check_if_expired, reload, options);

					break; // aborting the download
				}
				else
				{
					GetWindow()->SetHasShownUnsolicitedDownloadDialog(TRUE);
				}
			}
			// fallthrough
		default:
			OP_ASSERT(action != VIEWER_REDIRECT || !"VIEWER_REDIRECT was obsoleted by VIEWER_WEB_APPLICATION");
# ifdef WIC_USE_DOWNLOAD_CALLBACK
			TransferManagerDownloadCallback * t_tmdc = OP_NEW(TransferManagerDownloadCallback, (this, current_url, NULL, current_action_flag));
            if (!t_tmdc)
				return OpStatus::ERR_NO_MEMORY;

            window->GetWindowCommander()->GetDocumentListener()->OnDownloadRequest(
                    window->GetWindowCommander(), t_tmdc);

            OP_BOOLEAN finished = t_tmdc->Execute();

            if(finished == OpBoolean::IS_FALSE)
                SetLoadStat(WAIT_FOR_USER);

# else  // !WIC_USE_DOWNLOAD_CALLBACK
			SetLoadStat(WAIT_FOR_USER);
			window->AskAboutUnknownDoc(current_url, GetSubWinId());
# endif // !WIC_USE_DOWNLOAD_CALLBACK
		}

		use_current_doc = FALSE;

		g_windowManager->UpdateVisitedLinks(current_url, update_this_window ? NULL : GetWindow());

		if (es_pending_unload)
			OpStatus::Ignore(ESCancelPendingUnload());

		return stat;
	}

	return OpStatus::OK;
}

void DocumentManager::HandleUrlMoved(URL_ID url_id, URL_ID moved_to_id)
{
	URL to_url = current_url.GetAttribute(URL::KMovedToURL, TRUE);
#ifdef URL_MOVED_EVENT
	OpStringC url_string = to_url.GetAttribute(URL::KUniName);
	WindowCommander *win_com = window->GetWindowCommander();
	win_com->GetLoadingListener()->OnUrlMoved(win_com, url_string.CStr());
#endif // URL_MOVED_EVENT

	if (!IsURLSuitableSecurityContext(to_url))
	{
		// Redirect to something like a data: url. We need to let the last "real" document in the redirect chain be
		// the origin.
		DropCurrentLoadingOrigin();
		URL url = current_url;
		URL prev_url;
		while (!url.IsEmpty() && !(url == to_url))
		{
			prev_url = url;
			url = url.GetAttribute(URL::KMovedToURL, FALSE);
		}

		if (IsURLSuitableSecurityContext(prev_url))
			current_loading_origin = DocumentOrigin::Make(prev_url);
	}


	NotifyUrlChangedIfAppropriate(to_url);

#ifdef TRUST_RATING
	if (OpStatus::IsMemoryError(CheckTrustRating(to_url)))
		RaiseCondition(OpStatus::ERR_NO_MEMORY);
#endif // TRUST_RATING
}

#ifndef NO_EXTERNAL_APPLICATIONS

OP_STATUS DocumentManager::SetTempDownloadFolder()
{
	OpString filename;
	TRAPD(err, current_url.GetAttributeL(URL::KSuggestedFileName_L, filename, TRUE));
	RETURN_IF_ERROR(err);

	OpString saveto_filename;

	/* This code is fundamentally broken if OPFILE_TEMPDOWNLOAD_FOLDER doesn't
	   exist.  It should exist anyway, I think, or at least with a different
	   condition.  This function is called from non-NO_EXTERNAL_APPLICATIONS
	   code.  Or maybe that code is broken; I don't know. */

#ifndef NO_EXTERNAL_APPLICATIONS
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_TEMPDOWNLOAD_FOLDER, saveto_filename));
#endif // NO_EXTERNAL_APPLICATIONS

	int saveto_len = saveto_filename.Length();
	if (saveto_len > 0 && saveto_filename[saveto_len-1] != PATHSEPCHAR)
		RETURN_IF_ERROR(saveto_filename.Append(PATHSEP));

	RETURN_IF_ERROR(saveto_filename.Append(filename));

	err = CreateUniqueFilename(saveto_filename);
	RETURN_IF_ERROR(err);
	if (saveto_filename.HasContent())
		RETURN_IF_ERROR(current_url.LoadToFile(saveto_filename.CStr()));
	return OpStatus::OK;
}

#endif // NO_EXTERNAL_APPLICATIONS

OP_STATUS DocumentManager::SetSaveDirect(Viewer * viewer, BOOL &can_do)
{
	can_do = FALSE;
#ifndef NO_SAVE_SUPPORT
	OpString filename;
	if (viewer->GetSaveToFolder())
	{
		TRAPD(err, current_url.GetAttributeL(URL::KSuggestedFileName_L, filename, TRUE));
		RETURN_IF_ERROR(err);

		OpString saveto_filename;

		RETURN_IF_ERROR(saveto_filename.Set(viewer->GetSaveToFolder()));
		if( saveto_filename.Length() > 0 && saveto_filename[saveto_filename.Length()-1] != PATHSEPCHAR )
			RETURN_IF_ERROR(saveto_filename.Append(PATHSEP));

		RETURN_IF_ERROR(saveto_filename.Append(filename));
		err = CreateUniqueFilename(saveto_filename);
		RETURN_IF_ERROR(err);
		if (saveto_filename.HasContent())
		{
			RETURN_IF_ERROR(current_url.LoadToFile(saveto_filename.CStr()));
			can_do = TRUE;
		}
	}
#endif // !NO_SAVE_SUPPORT
	return OpStatus::OK;
}

void DocumentManager::HandleMultipartReload(URL_ID url_id, BOOL internal_reload)
{
	FramesDocument* doc = GetCurrentDoc();

	/* If we've calculated that "compatible" history navigation is needed, then
	   reloading this document will have observable side-effects, and possibly
	   break things.  (And it does break test result reporting in the sunspider
	   benchmark.) */

	if (doc && doc->GetCompatibleHistoryNavigationNeeded() && internal_reload)
		return;

	if (current_url.Id(TRUE) != url_id || doc && doc->GetURL().IsImage())
	{
		// check frames
		if (doc && doc->HandleLoading(MSG_MULTIPART_RELOAD, url_id, internal_reload) == OpStatus::ERR_NO_MEMORY)
			RaiseCondition(OpStatus::ERR_NO_MEMORY);

		return;
	}

	DM_LoadStat old_load_stat = GetLoadStatus();
	BOOL was_waiting_for_header = (old_load_stat == WAIT_FOR_HEADER);

	// Need to keep the URL used, since HandleAllLoaded sets load_stat to NOT_LOADING
	// which resets our URL_InUse object.  Once we set load_stat to WAIT_FOR_HEADER
	// below, the URL will be used again.  The temporary URL_IsUse makes sure the use
	// counter doesn't temporarily goes down to zero.
	URL_InUse temporary_use(current_url);

	HandleAllLoaded(url_id, TRUE);

	SetLoadStat(WAIT_MULTIPART_RELOAD);
	SetUserAutoReload(FALSE);

	if (!GetReload() && !internal_reload && !was_waiting_for_header)
		SetReload(TRUE);

	if (old_load_stat == DOC_CREATED)
		if (OpStatus::IsMemoryError(GetCurrentDoc()->HandleLoading(MSG_MULTIPART_RELOAD, url_id, 0)))
			RaiseCondition(OpStatus::ERR_NO_MEMORY);

#ifdef _WML_SUPPORT_
	if (wml_context)
		wml_context->RestartParsing();
#endif // _WML_SUPPORT_
}

#ifdef _ISHORTCUT_SUPPORT
static OP_STATUS ReadParseFollowInternetShortcutHelper(uni_char *filename, Window *window)
{
	TRAPD(status, ReadParseFollowInternetShortcutL(filename, window));
	return status;
}
#endif // _ISHORTCUT_SUPPORT

BOOL DocumentManager::HandleByOpera(BOOL check_if_inline_expired, BOOL use_plugin, BOOL is_user_initiated, BOOL *out_of_memory)
{
	BOOL oom;
	if (out_of_memory == NULL)
		out_of_memory = &oom;
	*out_of_memory = FALSE;

	URLContentType type = current_url.ContentType();

	FramesDocument* doc = GetCurrentDoc();

	if (GetParentDoc()
#ifdef SVG_SUPPORT
		&& (!frame->GetHtmlElement() || !frame->GetHtmlElement()->IsMatchingType(Markup::HTE_IMG, NS_HTML))
#endif // SVG_SUPPORT
		) // this is a frame, check for clickjacking
	{
		// Note that quite similar code exists in
		// HTML_Element::CheckMeta where the <meta http-equiv> is taken care of.
		OpString8 frame_options;
		current_url.GetAttribute(URL::KHTTPFrameOptions, frame_options);
		if (!frame_options.IsEmpty()) // No way to differ between an empty header and no header at all?
		{
			BOOL allow = TRUE;
			if (frame_options.CompareI("deny") == 0)
				allow = FALSE;
			else if (frame_options.CompareI("sameorigin") == 0)
			{
				URL top_doc_url = GetWindow()->DocManager()->GetCurrentDoc()->GetURL();
				if (!current_url.SameServer(top_doc_url) ||
					(top_doc_url.Type() == URL_HTTPS && current_url.Type() == URL_HTTP) ||
					(top_doc_url.Type() == URL_HTTP && current_url.Type() == URL_HTTPS))
					allow = FALSE;
			}

			if (!allow)
			{
				// Nesting a document that prohibits being nested
				URL blocked_page = current_url;
				StopLoading(FALSE /* FALSE = abort immediately*/);
#ifdef ERROR_PAGE_SUPPORT
				GenerateAndShowClickJackingBlock(blocked_page);
#endif // ERROR_PAGE_SUPPORT
				return FALSE;
			}
		}
	}

	switch (type)
	{
#ifdef _MIME_SUPPORT_
	case URL_MIME_CONTENT:
#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
	case URL_MHTML_ARCHIVE:
#endif
		return FALSE;
#endif // _MIME_SUPPORT_

#ifdef _WML_SUPPORT_
	case URL_WML_CONTENT:
		if (window)
		{
			if (wml_context && window->GetURLCameFromAddressField())
				wml_context->SetStatusOn(WS_CLEANHISTORY); // if URL is from address-field, we shall 'clean' the wml history
			else if (!wml_context)
			{
				// find out if the opener window has a wml context
				Window* opener_window = window->GetOpenerWindow();
				if (opener_window)
				{
					WML_Context* old_wc = opener_window->DocManager()->WMLGetContext();

					if (old_wc)
					{
						WMLSetContext(old_wc);			// copies the old context to this DocumentManager
						old_wc->SetActiveTask(NULL);	// remove the active_task of the origin window
					}
				}
			}
		}
	break;
#endif //_WML_SUPPORT_

	case URL_UNKNOWN_CONTENT:
	case URL_UNDETERMINED_CONTENT:
		if (!use_plugin)
			current_url.SetAttribute(URL::KForceContentType, URL_TEXT_CONTENT);
		break;

#ifdef OPERA_SETUP_DOWNLOAD_APPLY_SUPPORT
	case URL_MENU_CONFIGURATION_CONTENT:
	case URL_TOOLBAR_CONFIGURATION_CONTENT:
	case URL_MOUSE_CONFIGURATION_CONTENT:
	case URL_KEYBOARD_CONFIGURATION_CONTENT:
	case URL_SKIN_CONFIGURATION_CONTENT:
	case URL_MULTI_CONFIGURATION_CONTENT:
#endif // OPERA_SETUP_DOWNLOAD_APPLY_SUPPORT
#ifdef GADGET_SUPPORT
	case URL_GADGET_INSTALL_CONTENT:
	case URL_UNITESERVICE_INSTALL_CONTENT:
# ifdef EXTENSION_SUPPORT
	case URL_EXTENSION_INSTALL_CONTENT:
# endif // EXTENSION_SUPPORT
#endif // GADGET_SUPPORT
#ifdef OPERA_SETUP_DOWNLOAD_APPLY_SUPPORT
		{
# if defined(_KIOSK_MANAGER_)
			if( KioskManager::GetInstance()->GetNoDownload() )
			{
				window->GetDocManagerById(-1)->StopLoading(FALSE);
				window->ResetProgressDisplay();
				window->SetState(NOT_BUSY);
				return TRUE;
			}
# endif

			SetLoadStat(WAIT_FOR_USER);

# ifdef GADGET_SUPPORT
			if (type == URL_GADGET_INSTALL_CONTENT
#  ifdef WEBSERVER_SUPPORT
				|| type == URL_UNITESERVICE_INSTALL_CONTENT
#  endif // WEBSERVER_SUPPORT
#  ifdef EXTENSION_SUPPORT
				|| type == URL_EXTENSION_INSTALL_CONTENT
#  endif // EXTENSION_SUPPORT
				)
			{
#  ifdef WIC_USE_DOWNLOAD_CALLBACK
				TransferManagerDownloadCallback * t_tmdc = OP_NEW(TransferManagerDownloadCallback, (this, current_url, NULL, current_action_flag));
                if (!t_tmdc)
					return OpStatus::ERR_NO_MEMORY;

				OpDocumentListener *listener = window->GetWindowCommander()->GetDocumentListener();
				if(listener)
				{
					listener->OnGadgetInstall(window->GetWindowCommander(), t_tmdc);

					// TransferManagerDownload::Execute() might have been called in OnGadgetInstall, which calls
					// ResetCurrentDownloadRequest(), then current_download_request is NULL
					if (current_download_vector.Find(t_tmdc) == -1)
					{
						window->GetDocManagerById(-1)->StopLoading(FALSE);
						window->ResetProgressDisplay();
						window->SetState(NOT_BUSY);
					}
					else
					{
						OP_BOOLEAN finished = t_tmdc->Execute();

						if(finished == OpBoolean::IS_FALSE)
							SetLoadStat(WAIT_FOR_USER);
					}
				}
				else
				{
					OP_DELETE(t_tmdc); // will implicitly remove itself from the list.
				}
#  else // WIC_USE_DOWNLOAD_CALLBACK
				window->AskAboutUnknownDoc(current_url, GetSubWinId());
				return FALSE;
#  endif // else not WIC_USE_DOWNLOAD_CALLBACK
			}
			else
# endif // GADGET_SUPPORT
			{
# ifdef WIC_USE_DOWNLOAD_CALLBACK
				OpDocumentListener *listener = window->GetWindowCommander()->GetDocumentListener();
				if(listener)
				{
					listener->OnSetupInstall(window->GetWindowCommander(), current_url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped).CStr(), type);
				}
# else // WIC_USE_DOWNLOAD_CALLBACK
				window->AskAboutUnknownDoc(current_url, GetSubWinId());
				return FALSE;
# endif // else not WIC_USE_DOWNLOAD_CALLBACK
			}

# ifdef WIC_USE_DOWNLOAD_CALLBACK
			window->GetDocManagerById(-1)->StopLoading(FALSE);
			window->ResetProgressDisplay();
			window->SetState(NOT_BUSY);

			return FALSE;
# endif // WIC_USE_DOWNLOAD_CALLBACK
		}
		break;
#elif defined GADGET_SUPPORT
		window->AskAboutUnknownDoc(current_url, GetSubWinId());
		return FALSE;
#endif // GADGET_SUPPORT

#ifdef _SSL_SUPPORT_
	case URL_PFX_CONTENT:
	case URL_X_X509_USER_CERT:
	case URL_X_X509_CA_CERT:
	case URL_X_PEM_FILE:
#endif // _SSL_SUPPORT_
#ifdef _ISHORTCUT_SUPPORT
	case URL_ISHORTCUT_CONTENT:		//handle InternetShortcuts
#endif //_ISHORTCUT_SUPPORT

#ifdef _BITTORRENT_SUPPORT_
	case URL_P2P_BITTORRENT:
		if(type == URL_P2P_BITTORRENT
			&& !g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableBitTorrent))
		{
			break;
		}
#endif
		{
			// If all features above are disabled this will be dead code and cause a
			// warning. Accept it.
			SetLoadStat(WAIT_FOR_LOADING_FINISHED);
			if(current_url.Status(TRUE) != URL_LOADING)
				HandleAllLoaded(current_url.Id());
			return FALSE;
		}

#ifdef XML_AIT_SUPPORT
	case URL_XML_AIT:
		return FALSE;
#endif // XML_AIT_SUPPORT
	}

	SetLoadStat(DOC_CREATED);

	if (use_current_doc || (doc && (GetReload() || current_doc_elm->IsPreCreated()) && doc->GetURL() == current_url))
	{
		current_doc_elm->SetIsPreCreated(FALSE);

		if (OpStatus::IsMemoryError(UpdateCurrentDoc(use_plugin, FALSE, is_user_initiated)))
			*out_of_memory = TRUE;
	}
	else
	{
		if (doc && doc->GetURL() == current_url)
			SetUseHistoryNumber(current_doc_elm->Number());
		if (OpStatus::IsMemoryError(SetCurrentDoc(check_if_inline_expired, use_plugin, FALSE, is_user_initiated)))
			*out_of_memory = TRUE;
	}

	/* If we delayed signalling current security state to the UI until we
	   actually replaced the current document, then now is the time to actually
	   tell the UI. */
	GetWindow()->SignalSecurityState();

	if (*out_of_memory)
		return FALSE;

	if (GetRestartParsing())
	{
		if (OpStatus::IsMemoryError(RestartParsing()))
		{
			*out_of_memory = TRUE;
			return FALSE;
		}
	}

	OP_ASSERT(!GetCurrentDoc() || GetCurrentDoc()->IsCurrentDoc());

	return TRUE;
}

BOOL DocumentManager::NeedsProgressBar()
{
	if (window->IsLoading())
	{
		DocumentTreeIterator iterator(window);

		iterator.SetIncludeEmpty();
		iterator.SetIncludeThis();

		while (iterator.Next())
		{
			DocumentManager* docman = iterator.GetDocumentManager();
			FramesDocument* frames_doc = iterator.GetFramesDocument();

			if (frames_doc)
			{
				if (docman->GetCurrentURL().Type() != URL_JAVASCRIPT &&
				    frames_doc->GetURL().Id() != docman->GetCurrentURL().Id())
					return TRUE;

				if (frames_doc->NeedsProgressBar())
					return TRUE;
			}
			else if (docman->GetLoadStatus() != NOT_LOADING)
				return TRUE;
		}

		return FALSE;
	}
	else
		return FALSE;
}

void DocumentManager::EndProgressDisplay(BOOL force)
{
	if (force || !NeedsProgressBar())
		window->EndProgressDisplay();
}

OP_STATUS DocumentManager::HandleDocFinished()
{
	FramesDocument *doc = GetCurrentDoc();

	if (GetLoadStatus() != NOT_LOADING && GetLoadStatus() != DOC_CREATED && doc)
	{
		// We didn't expect this at this time. Just ignore it.
		return OpStatus::OK;
	}

	BOOL was_loading = GetLoadStatus() != NOT_LOADING;

	if (GetLoadStatus() != WAIT_MULTIPART_RELOAD)
		SetLoadStat(NOT_LOADING);

	history_walk = DM_NOT_HIST;

	if (user_started_loading)
	{
		user_started_loading = FALSE;
		pending_url_user_initiated = FALSE;
	}

	EndProgressDisplay();

	OP_STATUS stat = OpStatus::OK;

	if (doc)
		stat = FramesDocument::CheckOnLoad(doc);
	else if (frame)
		stat = FramesDocument::CheckOnLoad(NULL, frame);
	if (OpStatus::IsMemoryError(stat))
		return stat;

	ResetStateFlags();

	if (!was_loading)
		return stat;

	if (doc)
	{
		OpString decoded_fragment;
		OpString original_fragment;
		RETURN_IF_ERROR(GetURLFragment(current_url, decoded_fragment, original_fragment));
		if (decoded_fragment.CStr())
			RETURN_IF_MEMORY_ERROR(doc->SetRelativePos(decoded_fragment.CStr(), original_fragment.CStr(), scroll_to_fragment_in_document_finished));
		scroll_to_fragment_in_document_finished = FALSE;

		// update bookmarks info with parent frame
		FramesDocument *parent = doc->GetParentDoc();

		if (!parent && doc->GetURL().Type() == URL_DATA)
			UpdateSecurityState(FALSE);
		if (!parent)
			parent = doc;

		if (doc->GetVisualDevice())
			doc->GetVisualDevice()->LoadingFinished();
	}

	return stat;
}

OP_STATUS DocumentManager::UpdateWindowHistoryAndTitle()
{
	if (!GetParentDoc())
		if (FramesDocument *doc = GetCurrentDoc())
		{
			OpString doc_title;
			TempBuffer buffer;
			RETURN_IF_ERROR(doc_title.Set(doc->Title(&buffer)));

			OP_STATUS stat = OpStatus::OK;

			if (current_doc_elm)
				stat = current_doc_elm->SetTitle(doc_title.CStr());

			if (OpStatus::IsMemoryError(window->UpdateTitle(FALSE)))
				stat = OpStatus::ERR_NO_MEMORY;

			window->EnsureHistoryLimits();

#ifdef SHORTCUT_ICON_SUPPORT
			if (doc->Icon() && doc->Icon()->Status(TRUE) == URL_LOADED)
				if (OpStatus::IsMemoryError(window->SetWindowIcon(doc->Icon())))
					stat = OpStatus::ERR_NO_MEMORY;
#endif // SHORTCUT_ICON_SUPPORT

			return stat;
		}

	return OpStatus::OK;
}

#if defined _SSL_SUPPORT_ && !defined _EXTERNAL_SSL_SUPPORT_ && !defined _CERTICOM_SSL_SUPPORT_

static OP_STATUS
CreateCertificateInstallerHelper(SSL_Certificate_Installer_Base *&ssl_certificate_installer, URL &url, const SSL_Certificate_Installer_flags &installer_flags, SSL_dialog_config *ssl_dialog_config)
{
	TRAPD(status, ssl_certificate_installer = g_ssl_api->CreateCertificateInstallerL(url, installer_flags, ssl_dialog_config));
	return status;
}

#endif // _SSL_SUPPORT_ && _EXTERNAL_SSL_SUPPORT_ && _CERTICOM_SSL_SUPPORT_

void DocumentManager::HandleAllLoaded(URL_ID url_id, BOOL multipart_reload)
{
	if (current_url.Id(TRUE) != url_id)
	{
		// check frames
		if (FramesDocument* doc = GetCurrentDoc())
			if (doc->HandleLoading(MSG_ALL_LOADED, url_id, 0) == OpStatus::ERR_NO_MEMORY)
				RaiseCondition(OpStatus::ERR_NO_MEMORY);

		return;
	}

	if (GetLoadStatus() == WAIT_FOR_USER)
		return;

	URL_InUse temporary_use(current_url);

	SetLoadStat(NOT_LOADING);
	history_walk = DM_NOT_HIST;
	use_current_doc = FALSE;

	if (!multipart_reload)
		EndProgressDisplay();

#ifdef QUICK
	g_hotlist_manager->OnDocumentLoaded( &current_url, TRUE );
#endif // QUICK

	BOOL raise_OOM_condition = FALSE;

	OpString filename;

#if defined _LOCALHOST_SUPPORT_ || !defined RAMCACHE_ONLY
	if (current_url.GetAttribute(URL::KFilePathName_L, filename, TRUE) == OpStatus::ERR_NO_MEMORY )
		raise_OOM_condition = TRUE;
#endif

	ViewAction act = GetAction();

	// The following cases need url to be located on disk

	switch (act)
	{
	case VIEWER_NOT_DEFINED:
		if (!multipart_reload)
			StopLoading(TRUE, !frame);
		break;
	case VIEWER_OPERA:
		{
			URLContentType url_cnt_type = current_url.ContentType();

			switch (url_cnt_type)
			{
#ifdef _SSL_SUPPORT_
			case URL_PFX_CONTENT:
			case URL_X_X509_USER_CERT:
			case URL_X_X509_CA_CERT:
			case URL_X_PEM_FILE:
#if !defined(_EXTERNAL_SSL_SUPPORT_) && !defined(_CERTICOM_SSL_SUPPORT_)
				{
					URL empty_url;
					SSL_dialog_config ssl_dialog_config(window->GetOpWindow(), mh, MSG_DOCHAND_CERTIFICATE_INSTALL, 0, empty_url);

					URL url(current_url);

					SSL_CertificateStore store;

					switch (url_cnt_type)
					{
						case URL_PFX_CONTENT:
						case URL_X_X509_USER_CERT:
							store = SSL_ClientStore;
							break;
						case URL_X_X509_CA_CERT:
							store = SSL_CA_Store;
							break;
						case URL_X_PEM_FILE:
						default:
							store = SSL_ClientOrCA_Store;
							break;
					}

					SSL_Certificate_Installer_flags installer_flags(store, TRUE, FALSE);

					OP_STATUS oom_stat = CreateCertificateInstallerHelper(ssl_certificate_installer, url, installer_flags, &ssl_dialog_config);

					if (!ssl_certificate_installer)
					{
						if (OpStatus::IsMemoryError(oom_stat))
							raise_OOM_condition = TRUE;
					}
					else
					{
						OP_STATUS status = ssl_certificate_installer->StartInstallation();

						if (OpStatus::IsError(status) || ssl_certificate_installer->Finished())
						{
							BOOL install_success = OpStatus::IsSuccess(status) &&
								ssl_certificate_installer->InstallSuccess();

							OP_DELETE(ssl_certificate_installer);
							ssl_certificate_installer = NULL;

							raise_OOM_condition = OpStatus::IsMemoryError(status);

							if (!install_success)
								StopLoading(FALSE);

							EndProgressDisplay(current_url.Status(TRUE) != URL_LOADING);
						}
						else
							queue_messages = TRUE;
					}
				}
#else
				// Handle EPOC certificate manager here
#endif // _EXTERNAL_SSL_SUPPORT_
				break;
#endif // _SSL_SUPPORT_
#ifdef _BITTORRENT_SUPPORT_
			case URL_P2P_BITTORRENT:
				{
					if(act != VIEWER_NOT_DEFINED && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableBitTorrent))
					{
						BTInfo *info = OP_NEW(BTInfo, ());

						if (info)
							info->OnTorrentAvailable(current_url);
						else
							raise_OOM_condition = TRUE;

						current_url_used.UnsetURL();
					}
				}
				break;
#endif
#ifdef _ISHORTCUT_SUPPORT
			case URL_ISHORTCUT_CONTENT:		//handle InternetShortcuts
				{
					if (current_url.PrepareForViewing(TRUE) == MSG_OOM_CLEANUP)
					{
						raise_OOM_condition = TRUE;
						break;
					}

					if (OpStatus::IsMemoryError(current_url.GetAttribute(URL::KFilePathName_L, filename, TRUE)))
					{
						raise_OOM_condition = TRUE;
						break;
					}

					window->ResetSecurityStateToUnknown();

					SetAction(VIEWER_NOT_DEFINED);

					/* Internet shortcuts are not allowed inside of frames. CORE-48788. */
					if (GetFrame())
						StopLoading(FALSE, TRUE);
					else if (filename.CStr())
					{
						if (OpStatus::IsMemoryError(ReadParseFollowInternetShortcutHelper(filename.CStr(), window)))
						{
							raise_OOM_condition = TRUE;
							break;
						}
					}
				}
				break;
#endif // _ISHORTCUT_SUPPORT

#ifdef OPERA_SETUP_DOWNLOAD_APPLY_SUPPORT
			case URL_SKIN_CONFIGURATION_CONTENT:
			case URL_MULTI_CONFIGURATION_CONTENT:
			case URL_MENU_CONFIGURATION_CONTENT:
			case URL_TOOLBAR_CONFIGURATION_CONTENT:
			case URL_MOUSE_CONFIGURATION_CONTENT:
			case URL_KEYBOARD_CONFIGURATION_CONTENT:
# ifdef GADGET_SUPPORT
			case URL_GADGET_INSTALL_CONTENT:
			case URL_UNITESERVICE_INSTALL_CONTENT:
# endif // GADGET_SUPPORT
# ifdef EXTENSION_SUPPORT
			case URL_EXTENSION_INSTALL_CONTENT:
# endif // EXTENSION_SUPPORT
				//handled in quick/dialogs/SetupApplyDialog.cpp
				break;
#endif //OPERA_SETUP_DOWNLOAD_APPLY_SUPPORT

#ifdef XML_AIT_SUPPORT
			case URL_XML_AIT:
				{
					WindowCommander *wc = GetWindow()->GetWindowCommander();
					AITData ait_data;
					if (OpStatus::IsMemoryError(AITParser::Parse(current_url, ait_data)) ||
						OpStatus::IsMemoryError(wc->GetLoadingListener()->OnAITDocument(wc, &ait_data)))
						raise_OOM_condition = TRUE;
					StopLoading(TRUE);
				}
				break;
#endif // XML_AIT_SUPPORT

			default:
			/* Just here because all other cases are optional, and a switch statement with no cases but a default generates a stupid warning. */
			case URL_HTML_CONTENT:
				FramesDocument *doc = GetCurrentDoc();

				if (doc)
				{
					if (OpStatus::IsMemoryError(UpdateWindowHistoryAndTitle()))
						raise_OOM_condition = TRUE;
				}
			}
		}

		window->SetState(NOT_BUSY);
		break;

	case VIEWER_ASK_USER:
		// already handled in HandleHeaderLoaded or from the unknown file dialog
		break;

#ifndef NO_SAVE_SUPPORT
	case VIEWER_SAVE:
		if (current_action_flag.IsSet(ViewActionFlag::SAVE_DIRECTLY))
			EndProgressDisplay(TRUE);
		window->SetState(NOT_BUSY);
		break;
#endif // NO_SAVE_SUPPORT

	case VIEWER_APPLICATION:
	case VIEWER_REG_APPLICATION:
	case VIEWER_RUN:
#ifndef NO_EXTERNAL_APPLICATIONS
		if (current_url.PrepareForViewing(TRUE) == MSG_OOM_CLEANUP)
			raise_OOM_condition = TRUE;
		else
		{
			OP_STATUS status = current_url.GetAttribute(URL::KFilePathName_L, filename, TRUE);
			if (OpStatus::IsMemoryError(status))
				raise_OOM_condition = TRUE;

			TempBuffer file_to_execute;

			if (act == VIEWER_APPLICATION)
			{
				// FIXME FIXME FIXME. The syntax of ExecuteApplication is completely undocumented
				// which makes it quite a security risk. See CORE-17810.

				// Here we'll add quotes around the argument assuming arguments are space seperated
				// and trying to prevent a filename from being split into several parts.
				// See for instance DSK-238483.
				if (OpStatus::IsMemoryError(file_to_execute.Append('\"')) ||
					OpStatus::IsMemoryError(file_to_execute.Append(filename.CStr(), filename.Length())) ||
					OpStatus::IsMemoryError(file_to_execute.Append('\"')))
				{
					raise_OOM_condition = TRUE;
				}
			}
			else
			{
				if (OpStatus::IsMemoryError(file_to_execute.Append(filename.CStr(), filename.Length())))
				{
					raise_OOM_condition = TRUE;
				}
			}
			if (!raise_OOM_condition)
			{
				switch (act)
				{
				case VIEWER_APPLICATION:
				{
					OP_STATUS status =
						g_op_system_info->ExecuteApplication(
										      current_application,
											  file_to_execute.GetStorage(),
#ifdef WIC_USE_DOWNLOAD_CALLBACK
											  TRUE); // This suppress error dialog.
#else
											  FALSE);
#endif // WIC_USE_DOWNLOAD_CALLBACK

					if (OpStatus::IsMemoryError(status))
					{
						raise_OOM_condition = TRUE;
					}
					// Ask user what to do with downloaded file. See DSK-332652.
#ifdef WIC_USE_DOWNLOAD_CALLBACK
					else if (OpStatus::IsError(status))
					{
						TransferManagerDownloadCallback * t_tmdc =
							OP_NEW(TransferManagerDownloadCallback,
								   (this, current_url, NULL,
								    current_action_flag,
									TRUE)); // File is already on the disk.

						status = t_tmdc->Init(filename.CStr());
						if (OpStatus::IsSuccess(status))
						{
							window->GetWindowCommander()->GetDocumentListener()->
								OnDownloadRequest(window->GetWindowCommander(),
												  t_tmdc);
							if(t_tmdc->Execute() == OpBoolean::IS_FALSE)
								SetLoadStat(WAIT_FOR_USER);
						}
						else if (OpStatus::IsMemoryError(status))
							raise_OOM_condition = TRUE;
					}
#endif // WIC_USE_DOWNLOAD_CALLBACK
					break;
				}
				case VIEWER_REG_APPLICATION:
					window->ShellExecute(file_to_execute.GetStorage());
					break;

				case VIEWER_RUN:
					if (OpStatus::IsMemoryError(g_op_system_info->ExecuteApplication(file_to_execute.GetStorage(), UNI_L(""))))
						raise_OOM_condition = TRUE;
				}
			}

			EndProgressDisplay(TRUE);
		}
#endif // NO_EXTERNAL_APPLICATIONS
		break;

	default:
		window->SetState(NOT_BUSY);
	}

	//if the window is Not intiated by the user and this is the first url the window loads, close it
	//since the data is handled elsewhere.
	if ((act != VIEWER_OPERA && act != VIEWER_NOT_DEFINED) &&
	    window->GetOpenerWindow() && window->GetHistoryLen() == 0 && window->CanClose())
		window->Close();

	if (GetLoadStatus() == NOT_LOADING)
	{
		SetUserAutoReload(FALSE);
		SetReload(FALSE);
		SetReplace(FALSE);
		SetUseHistoryNumber(-1);
		SetAction(VIEWER_NOT_DEFINED);
		load_image_only = FALSE;
		save_image_only = FALSE;

		if (!GetCurrentDoc() && GetParentDoc())
		{
			FramesDocument::CheckOnLoad(NULL, frame);

			// Some has to tell the parent that we are finished and there is no document to do it
			if (GetParentDoc()->IsLoaded())
			{
				if (!GetParentDoc()->IsDocTreeFinished())
				{
					if (OpStatus::IsMemoryError(GetParentDoc()->CheckFinishDocument()))
						raise_OOM_condition = TRUE;
				}
			}
		}
	}

	if (raise_OOM_condition)
		RaiseCondition(OpStatus::ERR_NO_MEMORY);
}

void DocumentManager::HandleDocumentNotModified(URL_ID url_id)
{
	BOOL raise_OOM_condition = FALSE;

	if (current_url.Id(TRUE) != url_id)
	{
		// check frames
		FramesDocument* doc = GetCurrentDoc();
		if (doc && doc->HandleLoading(MSG_NOT_MODIFIED, url_id, 0) == OpStatus::ERR_NO_MEMORY )
			raise_OOM_condition = TRUE;
	}
	else
	{
		FramesDocument* doc = GetCurrentDoc();

		BOOL is_loading = window->IsLoading();
		BOOL is_reloading = GetReload();
		BOOL is_redirecting = GetRedirect();

		DM_LoadStat old_load_stat = GetLoadStatus();

		if (is_loading || is_redirecting || !doc || !(doc->GetURL() == current_url))
		{
			if (!is_reloading || is_redirecting)
			{
				if (HandleHeaderLoaded(url_id, TRUE) == OpStatus::ERR_NO_MEMORY)
					raise_OOM_condition = TRUE;

				// The call to HandleHeaderLoaded can result in a new current doc
				FramesDocument *new_current_doc = GetCurrentDoc();

				if (!new_current_doc || doc == new_current_doc)
					HandleAllLoaded(url_id);
			}
			else
			{
				if (doc && doc->IsFrameDoc() && doc->GetURL() == current_url)
				{
					raise_OOM_condition = OpStatus::IsMemoryError(doc->HandleLoading(MSG_NOT_MODIFIED, url_id, 0));

					if (doc->IsLoaded() && GetLoadStatus() != DOC_CREATED)
						HandleAllLoaded(url_id);

					SetLoadStatus(DOC_CREATED);
				}
				else
				{
					if (HandleHeaderLoaded(url_id, TRUE) == OpStatus::ERR_NO_MEMORY)
						raise_OOM_condition = TRUE;
				}
			}
		}
		else
		{
			SetUserAutoReload(FALSE);
			SetReload(FALSE);
			SetRedirect(FALSE);
			SetReplace(FALSE);
			SetUseHistoryNumber(-1);
			SetAction(VIEWER_NOT_DEFINED);
		}

		if (old_load_stat == WAIT_FOR_HEADER || doc && doc->GetURL().Type() == URL_DATA)
			window->DocManager()->UpdateSecurityState(FALSE);
	}

	if (raise_OOM_condition)
		RaiseCondition(OpStatus::ERR_NO_MEMORY);
}

static OP_STATUS UpdateOfflineModeHelper(BOOL toggle, BOOL &is_offline)
{
	TRAPD(status, is_offline = UpdateOfflineModeL(toggle));
	return status;
}

#ifdef OPERA_CONSOLE
static void
PostNetworkError(URL error_url, long error_code, unsigned windowid)
{
	if (!g_console->IsLogging())
		return;

	OpConsoleEngine::Message msg(OpConsoleEngine::Network, OpConsoleEngine::Error);
	msg.window = windowid;

	TRAPD(status,
		  // Ideally, the URL would be both readable (not escaped) and
		  // guaranteed to work when clicked or copy-pasted. In the
		  // absence a solution for that, the escaped URL is used.
		  error_url.GetAttributeL(URL::KUniName_With_Fragment_Escaped, msg.url);
		  g_languageManager->GetStringL(ConvertUrlStatusToLocaleString(error_code), msg.message);
		  g_console->PostMessageL(&msg));
	RAISE_IF_MEMORY_ERROR(status);
}
#endif // OPERA_CONSOLE

void
DocumentManager::SendNetworkErrorCode(Window* window, MH_PARAM_2 user_data)
{
	/*This code will send the error codes the to the error listener*/

	WindowCommander* window_commander = window->GetWindowCommander();
	OpErrorListener* error_listener = window_commander->GetErrorListener();

	union
	{
		OpErrorListener::HttpError http_error;
		OpErrorListener::FileError file_error;
		OpErrorListener::NetworkError network_error;
	};

	enum
	{
		NoErrorType,
		HttpErrorType,
		FileErrorType,
		NetworkErrorType
	}
	error_type;

	/*We need to translate the network error codes to OpErrorListener error codes */
	switch (user_data)
	{
		/*First we translate http error codes */

	case DH_ERRSTR(SI,ERR_HTTP_PAYMENT_REQUIRED):
		error_type = HttpErrorType;
		http_error = OpErrorListener::EC_HTTP_PAYMENT_REQUIRED;
		break;
	case DH_ERRSTR(SI,ERR_URL_TO_LONG):
	case DH_ERRSTR(SI,ERR_HTTP_REQUEST_URI_TOO_LARGE):
		error_type = HttpErrorType;
		http_error = OpErrorListener::EC_HTTP_REQUEST_URI_TOO_LONG;
		break;

	case DH_ERRSTR(SI,ERR_ILLEGAL_URL):
		error_type = HttpErrorType;
		http_error = OpErrorListener::EC_HTTP_BAD_REQUEST;
		break;

	case DH_ERRSTR(SI,ERR_COMM_HOST_NOT_FOUND):
	case DH_ERRSTR(SI,ERR_COMM_HOST_UNAVAILABLE):
	case DH_ERRSTR(SI,ERR_HTTP_NOT_FOUND):
		error_type = HttpErrorType;
		http_error = OpErrorListener::EC_HTTP_NOT_FOUND;
		break;

	case DH_ERRSTR(SI,ERR_HTTP_SERVER_ERROR):
		error_type = HttpErrorType;
		http_error = OpErrorListener::EC_HTTP_INTERNAL_SERVER_ERROR;
		break;

	case DH_ERRSTR(SI,ERR_NO_CONTENT):
		error_type = HttpErrorType;
		http_error = OpErrorListener::EC_HTTP_NO_CONTENT;
		break;

	case DH_ERRSTR(SI,ERR_HTTP_RESET_CONTENT):
		error_type = HttpErrorType;
		http_error = OpErrorListener::EC_HTTP_RESET_CONTENT;
		break;

	case DH_ERRSTR(SI,ERR_COMM_BLOCKED_URL):
	case DH_ERRSTR(SI,ERR_HTTP_NO_CONTENT):
		error_type = HttpErrorType;
		http_error = OpErrorListener::EC_HTTP_NO_CONTENT;
		break;

	case DH_ERRSTR(SI,ERR_HTTP_FORBIDDEN):
		error_type = HttpErrorType;
		http_error = OpErrorListener::EC_HTTP_FORBIDDEN;
		break;

	case DH_ERRSTR(SI,ERR_HTTP_METHOD_NOT_ALLOWED):
		error_type = HttpErrorType;
		http_error = OpErrorListener::EC_HTTP_METHOD_NOT_ALLOWED;
		break;

	case DH_ERRSTR(SI,ERR_HTTP_NOT_ACCEPTABLE):
		error_type = HttpErrorType;
		http_error = OpErrorListener::EC_HTTP_NOT_ACCEPTABLE;
		break;

	case DH_ERRSTR(SI,ERR_HTTP_TIMEOUT):
		error_type = HttpErrorType;
		http_error = OpErrorListener::EC_HTTP_REQUEST_TIMEOUT;
		break;

	case DH_ERRSTR(SI,ERR_HTTP_CONFLICT):
		error_type = HttpErrorType;
		http_error = OpErrorListener::EC_HTTP_CONFLICT;
		break;

	case DH_ERRSTR(SI,ERR_HTTP_GONE):
		error_type = HttpErrorType;
		http_error = OpErrorListener::EC_HTTP_GONE;
		break;

	case DH_ERRSTR(SI,ERR_HTTP_LENGTH_REQUIRED):
		error_type = HttpErrorType;
		http_error = OpErrorListener::EC_HTTP_LENGTH_REQUIRED;
		break;

	case DH_ERRSTR(SI,ERR_HTTP_PRECOND_FAILED):
		error_type = HttpErrorType;
		http_error = OpErrorListener::EC_HTTP_PRECONDITION_FAILED;
		break;

	case DH_ERRSTR(SI,ERR_HTTP_REQUEST_ENT_TOO_LARGE):
		error_type = HttpErrorType;
		http_error = OpErrorListener::EC_HTTP_REQUEST_ENTITY_TOO_LARGE;
		break;

	case DH_ERRSTR(SI,ERR_HTTP_UNSUPPORTED_MEDIA_TYPE):
		error_type = HttpErrorType;
		http_error = OpErrorListener::EC_HTTP_UNSUPPORTED_MEDIA_TYPE;
		break;

	case DH_ERRSTR(SI,ERR_HTTP_NOT_IMPLEMENTED):
		error_type = HttpErrorType;
		http_error = OpErrorListener::EC_HTTP_NOT_IMPLEMENTED;
		break;

	case DH_ERRSTR(SI,ERR_HTTP_BAD_GATEWAY):
		error_type = HttpErrorType;
		http_error = OpErrorListener::EC_HTTP_BAD_GATEWAY;
		break;

	case DH_ERRSTR(SI,ERR_HTTP_GATEWAY_TIMEOUT):
		error_type = HttpErrorType;
		http_error = OpErrorListener::EC_HTTP_GATEWAY_TIMEOUT;
		break;

	case DH_ERRSTR(SI,ERR_HTTP_VERSION_NOT_SUPPORTED):
		error_type = HttpErrorType;
		http_error = OpErrorListener::EC_HTTP_HTTP_VERSION_NOT_SUPPORTED;
		break;

	case DH_ERRSTR(SI,ERR_HTTP_SERVICE_UNAVAILABLE):
		error_type = HttpErrorType;
		http_error = OpErrorListener::EC_HTTP_SERVICE_UNAVAILABLE;
		break;


		/* file errors */

	case DH_ERRSTR(SI,ERR_FILE_CANNOT_OPEN):
		error_type = FileErrorType;
		file_error = OpErrorListener::EC_FILE_REFUSED;
		break;

	case DH_ERRSTR(SI,ERR_FILE_DOES_NOT_EXIST):
		error_type = FileErrorType;
		file_error = OpErrorListener::EC_FILE_NOT_FOUND;
		break;


		/* generic network errors */

	case DH_ERRSTR(SI,ERR_COMM_NETWORK_UNREACHABLE):
	case DH_ERRSTR(SI,ERR_NETWORK_PROBLEM):
	case DH_ERRSTR(SI,ERR_COMM_CONNECT_FAILED):
		error_type = NetworkErrorType;
		network_error = OpErrorListener::EC_NET_CONNECTION;
		break;

	default:
		error_type = NoErrorType;
		break;
	}

	switch (error_type)
	{
	case HttpErrorType:
		error_listener->OnHttpError(window_commander, http_error);
		break;
	case FileErrorType:
		error_listener->OnFileError(window_commander, file_error);
		break;
	case NetworkErrorType:
		error_listener->OnNetworkError(window_commander, network_error);
		break;
	}
}

#ifdef ERROR_PAGE_SUPPORT
static BOOL IsCrossNetworkError(MH_PARAM_2 user_data)
{
	return user_data == Str::S_MSG_CROSS_NETWORK_INTRANET_LOCALHOST ||
		user_data == Str::S_MSG_CROSS_NETWORK_INTERNET_LOCALHOST ||
		user_data == Str::S_MSG_CROSS_NETWORK_INTERNET_INTRANET;
}
#endif // ERROR_PAGE_SUPPORT

void DocumentManager::HandleLoadingFailed(URL_ID url_id, MH_PARAM_2 user_data)
{
	BOOL raise_OOM_condition = FALSE;

	if (url_id == current_url.Id() || url_id == current_url.Id(TRUE))
	{
		ViewAction saved_action = GetAction(); // Continue download correctly after Offline mode alert. johan@opera.com
		SetAction(VIEWER_NOT_DEFINED);

		URL final_url = current_url.GetAttribute(URL::KMovedToURL, TRUE);
		URL new_url;
		if (final_url.IsEmpty())
			final_url = current_url;

		BOOL stop_loading_document = TRUE;
		BOOL execute_redirect = FALSE;

		if (OpStatus::IsError(URL_Manager::ManageTCPReset(final_url, new_url, user_data, execute_redirect)))
			return;

		/** Redirect attempted as a counter measure to a TCP Reset (CORE-45283).

		    Long story short, if we receive a TCP RST on a small packet, for example a redirect, the
			Windows TCP stack usually will lose the content. Linux does not have the problem.

			In an ideal world, we would fix it on the Network layer at a platform level.
			The problem is that all the working methods (blocking sockets, not blocking synchronous sockets
			with and without overlapped I/O, completion routine and possibly completion ports) are not suitable
			for Opera, as they require additional threads or a call to SleepEx().

			For some reasons, it looks like asynchronous sockets (both with and without overlapped I/O) are not able to cope with this problem.
			So we decided to isolate the problem and attempt a redirect.
			For example: if http://skandiabanken.no fails with a reset, http://www.skandiabanken.no is attempted.
			This fixes CORE-45283 and possibly other similar cases.
		*/
		if (execute_redirect)
		{
			DocumentReferrer referrer;

			OpenURL(new_url, referrer, FALSE, FALSE);

			return;
		}

		if (user_data == DH_ERRSTR(SI,ERR_COMM_BLOCKED_URL))
			if (FramesDocElm *frame = GetFrame())
				if (HTML_Element *element = frame->GetHtmlElement())
					if (!element->IsMatchingType(HE_FRAME, NS_HTML))
						/**
						 * In here, the URL was blocked by the content blocker
						 * and is inside an iframe or object (frames don't count),
						 * so we don't load the error page and let the iframe
						 * be hidden.
						 */
						user_data = DH_ERRSTR(SI,ERR_HTTP_NO_CONTENT);

		if (user_data == DH_ERRSTR(SI,ERR_HTTP_RESET_CONTENT))
		{
			FramesDocument* doc = GetCurrentDoc();
			if (doc)
				doc->ResetForm(1); // FIXME: Reset all forms, not only the first. Double check specs.
		}
		else if (user_data == DH_ERRSTR(SI,ERR_HTTP_NO_CONTENT))
		{
			if (final_url.Type() == URL_JAVASCRIPT && GetCurrentDoc())
			{
				// Javascript urls creating no content ends up here possibly without
				// finishing or sending onload events.
				GetCurrentDoc()->CheckFinishDocument();
				// The javascript URL has finished, but don't stop the document (CORE-33876.)
				stop_loading_document = FALSE;
			}
		}
		else if (user_data == DH_ERRSTR(SI, ERR_SSL_ERROR_HANDLED))
		{
			if (DisplayContentIfAvailable(final_url, url_id, user_data))
				return;
		}
		else if (!GetUserAutoReload())
		{

			URL_InUse temporary_url(current_url); // to avoid that final_url gets deleted when current_url is

			//The following code was moved from Window::MessErrorURL
			//When user is prompted to enter Online mode, the page should continue loading if
			//answer is yes.
			Window::OnlineMode online_mode = window->GetOnlineMode();
			if (user_data == DH_ERRSTR(SI,ERR_COMM_HOST_NOT_FOUND) &&
				online_mode != Window::ONLINE && online_mode != Window::DENIED)
			{
				SetAction(saved_action);
				SetWaitingForOnlineUrl(final_url);

				if (online_mode == Window::OFFLINE)
				{
					if (OpStatus::IsMemoryError(window->QueryGoOnline(&final_url)))
						raise_OOM_condition = TRUE;
				}
			}
			else
			{
				URL ref_url;

				SendNetworkErrorCode( window, user_data );

#ifdef ERROR_PAGE_SUPPORT
# ifdef REDIRECT_ON_ERROR
				// show Opera finder page if;
				//1. the preference is ON
				//2. this is the top-frame
				//3. there is no server errormessage
				//4. there is no protocol specified in the original typed string
				//5. the failed server is not the lookup server

				if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableHostNameWebLookup)
					&& !parent_doc
					&& !final_url.GetAttribute(URL::KInternalErrorString))
				{
					OpString *typed_in = GetWindow()->GetTypedInUrl();
					if (!typed_in->IsEmpty() && KNotFound == typed_in->FindFirstOf(':')
						&& KNotFound == typed_in->FindFirstOf('/'))
					{
						ServerName* sname = final_url.GetServerName();

						OpStringC lookup_adr = g_pcnet->GetStringPref(
							PrefsCollectionNetwork::HostNameWebLookupAddress,
							sname);

						const uni_char* failedDomain = sname ? sname->UniName() : NULL;

						if (failedDomain && !lookup_adr.IsEmpty()
							&& KNotFound == lookup_adr.Find(failedDomain))
						{
							OpString lookup_str;

							OP_STATUS oom;
							if (KNotFound != lookup_adr.Find("%s"))
							{
								oom = lookup_str.AppendFormat(lookup_adr.CStr(), typed_in->CStr());
							}
							else
							{
								oom = lookup_str.AppendFormat(UNI_L("%s%s"), lookup_adr.CStr(), typed_in->CStr());
							}

							if (OpStatus::IsMemoryError(oom))
							{
								RaiseCondition(oom);
								return;
							}

							URL lookup_url = g_url_api->GetURL(lookup_str.CStr());
							OpenURL(lookup_url, DocumentReferrer(ref_url), FALSE, FALSE);
							return;
						}
					}
				}
# endif // REDIRECT_ON_ERROR

				BOOL platform_did_the_error_report = window->HandleLoadingFailed(
#ifdef HC_CAP_ERRENUM_AS_STRINGENUM
							static_cast<Str::LocaleString>(user_data),
#else
							ConvertUrlStatusToLocaleString(user_data),
#endif
							final_url);

				if (!platform_did_the_error_report)
				{
					if (IsCrossNetworkError(user_data))
					{
						GenerateAndShowCrossNetworkErrorPage(final_url, static_cast<Str::LocaleString>(user_data));
						error_page_shown = TRUE;
					}
					else
					if (DisplayContentIfAvailable(final_url, final_url.Id(), user_data))
					{
# ifdef OPERA_CONSOLE
						PostNetworkError(final_url, user_data, GetWindow()->Id());
# endif // OPERA_CONSOLE
					}
					else
					{
						URL error_page(final_url);
#ifdef AB_ERROR_SEARCH_FORM
						const uni_char *user_text = GetWindow()->GetTypedInUrl()->CStr();
#else
						const uni_char *user_text = NULL;
#endif //AB_ERROR_SEARCH_FORM
						SetAction(VIEWER_OPERA);
						error_page.SetAttribute(URL::KForceContentType, URL_HTML_CONTENT);
						BOOL show_search_form = !GetFrame() && user_text;
						if (OpStatus::IsMemoryError(g_url_api->GenerateErrorPage(error_page, user_data, &current_url, show_search_form, user_text)))
							raise_OOM_condition = TRUE;
						SetCurrentURL(error_page, FALSE);
						// Now make sure we load the new data. Setting the state to
						// WAIT_FOR_HEADER seems most right
						SetLoadStatus(WAIT_FOR_HEADER); // Load the new document, bypassing the WAIT_FOR_HEADER state
						error_page_shown = TRUE;
						if (OpStatus::IsMemoryError(HandleDataLoaded(error_page.Id(TRUE))))
							raise_OOM_condition = TRUE;
					}

					return;
				}
#endif // ERROR_PAGE_SUPPORT
			}
		}

		if (!raise_OOM_condition && GetLoadStatus() == DOC_CREATED)
		{
			StopLoading(TRUE);
			return;
		}

		if (es_pending_unload)
			OpStatus::Ignore(ESCancelPendingUnload());

		if (stop_loading_document)
			StopLoading(TRUE, !GetFrame());
		else
			SetLoadStat(NOT_LOADING);

		EndProgressDisplay();

		if (FramesDocElm *frame = GetFrame())
			FramesDocument::CheckOnLoad(NULL, frame);
		else if (FramesDocument* doc = GetCurrentDoc())
			SetCurrentURL(doc->GetURL(), FALSE);
	}
	else
	{
		FramesDocument* doc = GetCurrentDoc();

		if (doc && doc->HandleLoading(MSG_URL_LOADING_FAILED, url_id, user_data) == OpStatus::ERR_NO_MEMORY)
			raise_OOM_condition = TRUE;
	}

	if (raise_OOM_condition)
		RaiseCondition(OpStatus::ERR_NO_MEMORY);
}

OP_STATUS DocumentManager::HandleLoading(OpMessage msg, URL_ID url_id, MH_PARAM_2 user_data)
{
	OP_PROBE6(OP_PROBE_DOCUMENTMANAGER_HANDLELOADING);

	if (queue_messages)
	{
		if (QueuedMessageElm *last = (QueuedMessageElm *) queued_messages.Last())
			if (last->msg == msg && last->url_id == url_id && last->user_data == user_data)
				return OpStatus::OK;

		if (QueuedMessageElm *elm = OP_NEW(QueuedMessageElm, ()))
		{
			elm->msg = (OpMessage) msg;
			elm->url_id = url_id;
			elm->user_data = user_data;
			elm->Into(&queued_messages);
			elm->target_url.SetURL(current_url_reserved);
		}
		else
			return OpStatus::ERR_NO_MEMORY;

		return OpStatus::OK;
	}

	if (is_handling_message || is_clearing || GetLoadStatus() == WAIT_FOR_USER)
		return OpStatus::OK;

#ifdef _PRINT_SUPPORT_
	if (print_preview_doc && window->GetPreviewMode())
		return OpStatus::OK;
#endif // _PRINT_SUPPORT_

	OP_STATUS stat = OpStatus::OK;

	is_handling_message = TRUE;

	if (url_id == current_url.Id(TRUE) && GetLoadStatus() == WAIT_MULTIPART_RELOAD)
	{
		/* After MSG_MULTIPART_RELOAD message: If message is MSG_URL_DATA_LOADED
		   and the URL has stopped loading, there won't be another body part
		   after all, and we're actually done loading.  If message is
		   MSG_URL_DATA_LOADED and the URL is still loading, ignore it as
		   suggested in documentation in url/url2.h.  Otherwise revert to
		   WAIT_FOR_HEADER state in preperation to load a new body part. */

		if (msg == MSG_URL_DATA_LOADED)
		{
			if (current_url.Status(TRUE) != URL_LOADING)
			{
				SetLoadStatus(DOC_CREATED);
				FramesDocument *doc = GetCurrentDoc();
				if (!doc || doc->IsLoaded())
					EndProgressDisplay();
			}

			is_handling_message = FALSE;
			return OpStatus::OK;
		}
		else
		{
			if (FramesDocument *doc = GetCurrentDoc())
				if (doc->StopLoading(FALSE) == OpStatus::ERR_NO_MEMORY)
					RaiseCondition(OpStatus::ERR_NO_MEMORY);

			SetLoadStatus(WAIT_FOR_HEADER);
		}
	}

#ifdef AB_ERROR_SEARCH_FORM
	//After this function quits, the text is deleted because it's not going to be used anymore
	AutoEmptyOpString user_text_anchor(GetWindow()->GetTypedInUrl());
#endif //AB_ERROR_SEARCH_FORM

	switch (msg)
	{
	case MSG_URL_DATA_LOADED:
		if (GetRestartParsing())
		{
			OP_STATUS parse_status = RestartParsing();
			if (OpStatus::IsMemoryError(parse_status))
			{
				is_handling_message = FALSE;
				return parse_status;
			}
		}

		if (!OpStatus::IsError(stat))
			stat = HandleDataLoaded(url_id);
		break;

	case MSG_HEADER_LOADED:
		stat = HandleHeaderLoaded(url_id, TRUE);
		break;

	case MSG_MULTIPART_RELOAD:
		HandleMultipartReload(url_id, (user_data != 0 ? TRUE : FALSE));
		break;

	case MSG_ALL_LOADED:
		HandleAllLoaded(url_id);
		break;

	case MSG_NOT_MODIFIED:
		HandleDocumentNotModified(url_id);
		break;

	case MSG_URL_LOADING_FAILED:
		HandleLoadingFailed(url_id, user_data);
		break;

	case MSG_URL_MOVED:
		HandleUrlMoved(url_id, (URL_ID)user_data);
		break;
	}

	is_handling_message = FALSE;

	return stat;
}

OP_STATUS DocumentManager::RestartParsing()
{
	SetRestartParsing(FALSE);

#ifdef _WML_SUPPORT_
	if (wml_context)
	{
		RETURN_IF_MEMORY_ERROR(wml_context->RestartParsing());
	}
#endif // _WML_SUPPORT_

	GetCurrentDoc()->StopLoading(FALSE, TRUE, TRUE);

	OP_STATUS stat = UpdateCurrentDoc(FALSE, TRUE, FALSE);
	OP_ASSERT(!GetCurrentDoc() || GetCurrentDoc()->IsCurrentDoc());

	return stat;
}

void DocumentManager::UpdateSecurityState(BOOL include_loading_docs)
{
	// NOT_LOADING and DOC_CREATED are the states that might have exisiting
	// content. DOC_CREATED could (when walking in history for instance) be
	// a mix of old content and content we're loading the normal way
	// and we have to check the old content.
	if (include_loading_docs ||
		GetLoadStatus() == NOT_LOADING || GetLoadStatus() == DOC_CREATED)
	{
		if (FramesDocument* doc = GetCurrentDoc())
			doc->UpdateSecurityState(include_loading_docs);
	}
}

void DocumentManager::SetReload(BOOL value)
{
	if (!reload != !value)
	{
		reload = reload_document = value;
		conditionally_request_document = !value;
		reload_inlines = FALSE;
		conditionally_request_inlines = TRUE;
	}
	was_reloaded = reload;
}

void DocumentManager::SetReloadFlags(BOOL reload_document0, BOOL conditionally_request_document0, BOOL reload_inlines0, BOOL conditionally_request_inlines0)
{
	reload_document = reload_document0;
	conditionally_request_document = conditionally_request_document0;
	reload_inlines = reload_inlines0;
	were_inlines_reloaded = reload_inlines0;
	conditionally_request_inlines = conditionally_request_inlines0;
	were_inlines_requested_conditionally = conditionally_request_inlines0;
}

#ifdef OPERA_CONSOLE
void DocumentManager::PostURLErrorToConsole(URL& url, Str::LocaleString error_message)
{
	if (!g_console->IsLogging())
		return;

	BOOL raise_oom = FALSE;
	OpConsoleEngine::Message msg(OpConsoleEngine::Network, OpConsoleEngine::Error);
	msg.window = GetWindow()->Id();

	// Eventually it would be nice to unescape the url to make it more readable, but then we need to
	// get the decoding to do it correctly with respect to charset encodings. See bug 250545
	OP_STATUS check = url.GetAttribute(URL::KUniName_With_Fragment_Escaped, msg.url);
	if (OpStatus::IsSuccess(check))
	{
		check = g_languageManager->GetString(error_message, msg.message);
		if (OpStatus::IsSuccess(check))
		{
			TRAP(check, g_console->PostMessageL(&msg));
			raise_oom = check == OpStatus::ERR_NO_MEMORY;
		}
		else
			raise_oom = check == OpStatus::ERR_NO_MEMORY;
	}
	else
		raise_oom = check == OpStatus::ERR_NO_MEMORY;

	if (raise_oom)
		RaiseCondition(OpStatus::ERR_NO_MEMORY);
}
#endif // OPERA_CONSOLE

#ifdef ON_DEMAND_PLUGIN
static OP_STATUS ComposePlacholderStr(OpString &placeholder_str, const uni_char *folder)
{
	OpString file;
	RETURN_IF_MEMORY_ERROR(file.Set(folder));
	RETURN_IF_MEMORY_ERROR(file.Append("missingplugin.svg"));

	TRAPD(status, g_url_api->ResolveUrlNameL(file.CStr(), placeholder_str));
	return status;
}

static void CreatePluginPlaceholderUrl(const URL& parent_url, URL &placeholder_url)
{
	// For plugin placeholders, pretend that this isn't a plugin and replace the
	// current url with the url to the placeholder.  This makes us load a
	// document with that url.

	BOOL use_image_placeholder = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::EnableOnDemandPluginPlaceholder);
	if (use_image_placeholder)
	{
		OpString placholder_url_str;
		OpString img_folder_name;
		OP_STATUS err = g_folder_manager->GetFolderPath(OPFILE_IMAGE_FOLDER, img_folder_name);
		if (OpStatus::IsSuccess(err) &&
			OpStatus::IsSuccess(ComposePlacholderStr(placholder_url_str, img_folder_name)))
		{
			// Change current_url
			placeholder_url = g_url_api->GetURL(placholder_url_str.CStr());

			// Make current url unique so that our message
			// handler is notified about incoming data,
			// regardless if the url is loaded for other
			// plugins simultaneously.

			g_url_api->MakeUnique(placeholder_url);
			// Set the URL of the plugin placeholder the same security status as its parent URL has.
			placeholder_url.SetAttribute(URL::KSecurityStatus, parent_url.GetAttribute(URL::KSecurityStatus, TRUE));

			return;
		}
	}

	// Document will not be painted. Load as little as possible.
	placeholder_url = g_url_api->GetURL("about:blank");
}
#endif // ON_DEMAND_PLUGIN

BOOL DocumentManager::IsRelativeJump(URL url)
{
	if (FramesDocument *current_doc = GetCurrentDoc())
	{
		URL current_doc_url = current_doc->GetURL();

		if (url.RelName())
		{
			if (url == current_doc_url)
				return TRUE;
			else if (current_doc_url.GetAttribute(URL::KUnique, TRUE) &&
				!url.GetAttribute(URL::KUnique, TRUE))
			{
				OpString8 url_string;

				if (OpStatus::IsSuccess(url.GetAttribute(URL::KName_Username_Password_NOT_FOR_UI, url_string, TRUE)))
					if (current_doc_url.GetAttribute(URL::KName_Username_Password_NOT_FOR_UI, TRUE).Compare(url_string) == 0)
						return TRUE;
			}
		}
	}

	return FALSE;
}

BOOL
DocumentManager::SignalOnNewPage(OpViewportChangeReason reason)
{
	if (!GetParentDoc())
	{
		ViewportController* controller = GetWindow()->GetViewportController();
		controller->GetViewportInfoListener()->OnNewPage(controller,
														 reason,
														 current_doc_elm ? current_doc_elm->GetID() : HistoryIdNotSet);
		return TRUE;
	}
	return FALSE;
}

void
DocumentManager::CheckOnNewPageReady()
{
	if (waiting_for_document_ready && !GetVisualDevice()->IsLocked())
	{
		waiting_for_document_ready = FALSE;
		ViewportController* controller = GetWindow()->GetViewportController();
		controller->GetViewportInfoListener()->OnNewPageReady(controller);
	}
}

DocumentManager::OpenURLOptions::OpenURLOptions()
	: user_initiated(FALSE),
	  create_doc_now(FALSE),
	  entered_by_user(NotEnteredByUser),
	  is_walking_in_history(FALSE),
	  es_terminated(FALSE),
	  called_externally(FALSE),
	  bypass_url_access_check(FALSE),
	  ignore_fragment_id(FALSE),
	  is_inline_feed(FALSE),
	  origin_thread(NULL),
	  from_html_attribute(FALSE)
{
}

#ifdef DOM_LOAD_TV_APP
void DocumentManager::SetWhitelist(OpAutoVector<OpString>* new_whitelist)
{
	OP_DELETE(whitelist);
	whitelist = new_whitelist;
}
#endif

void DocumentManager::OpenURL(URL url, DocumentReferrer referrer, BOOL check_if_expired, BOOL reload, const OpenURLOptions &options)
{
	activity_loading.Begin();

#ifdef DOM_LOAD_TV_APP
	if (whitelist)
	{
		BOOL is_whitelisted = FALSE;
		OpString url_name;
		if (!OpStatus::IsSuccess(url.GetAttribute(URL::KUniName, url_name, false)))
			return;
		for (unsigned int i = 0; i < whitelist->GetCount() && !is_whitelisted; i++)
		{
			if (url_name.Compare(whitelist->Get(i)->CStr(), whitelist->Get(i)->Length()) == 0)
				is_whitelisted = TRUE;
		}
		if (!is_whitelisted)
		{
			WindowCommander* wc = window->GetWindowCommander();
			wc->GetTVAppMessageListener()->OnOpenForbiddenUrl(wc, url_name.CStr());
			return;
		}
	}
#endif

	BOOL user_initiated = options.user_initiated;
	BOOL create_doc_now = options.create_doc_now;
	EnteredByUser entered_by_user = options.entered_by_user;
	BOOL is_walking_in_history = options.is_walking_in_history;
	BOOL es_terminated = options.es_terminated;
	BOOL bypass_url_access_check = options.bypass_url_access_check;

	// OpenURL was called without updating current_url to the one set in the history state.
	// (it is normally updated in SetLoadStat(NOT_LOADING) but it may happen SetLoadStat() was not called like e.g. when window.location is changed)
	if (history_state_doc_elm && history_state_doc_elm == current_doc_elm
		&& referrer.url == current_url && history_state_doc_elm->GetReferrerUrl().url == current_url)
	{
		history_state_doc_elm->Doc()->ClearAuxUrlToHandleLoadingMessagesFor();
		SetCurrentURL(history_state_doc_elm->GetUrl(), FALSE);
		current_url_is_inline_feed = FALSE;
		referrer = DocumentReferrer(current_url);
		history_state_doc_elm = NULL;
	}

	FramesDocument* doc = GetCurrentDoc();
	if (doc && !doc->IsContentChangeAllowed())
		return;

#ifdef EXTENSION_SUPPORT
	if (!parent_doc &&
		window->GetType() == WIN_TYPE_GADGET &&
		window->GetGadget()->IsExtension() &&
		window->GetGadget()->GadgetUrlLoaded() &&
		!reload &&
		(!security_check_callback || !security_check_callback->m_suspended))
		return; // No loading urls in the extension background process once it was loaded for the first time.
#endif // EXTENSION_SUPPORT

	// Sandboxed documents are not allowed to navigate parent documents or the top level document,
	// unless the top level is the sandbox document.
	if (referrer.origin && referrer.origin->in_sandbox)
	{
		FramesDocument* sandbox_doc = doc;
		if (!sandbox_doc)
			sandbox_doc = GetParentDoc();

		if (!sandbox_doc)
			return;

		while (!(sandbox_doc->GetOrigin()->security_context == referrer.origin->security_context))
		{
			sandbox_doc = sandbox_doc->GetParentDoc();
			if (!sandbox_doc)
			{
				// Disallow attempt to navigate outside the sandbox.
				return;
			}
		}
	}


#ifdef ERROR_PAGE_SUPPORT
	/* Before doing anything, we need to check if we're coming
	 * from a click-through page because the url might change
	 * to point to the real one. This method might mutate both
	 * url and referrer. This happens for:
	 *  opera:crossnetworkwarning
	 *  opera:site-warning
	 *  opera:clickjackblocking
	 **/
	do {
		OP_STATUS status = HandleClickThroughUrl(url, referrer);
		RaiseCondition(status);
		RETURN_VOID_IF_ERROR(status);
	} while(0);
#endif // ERROR_PAGE_SUPPORT

#ifdef APPLICATION_CACHE_SUPPORT
	ApplicationCache *cache = g_application_cache_manager ? g_application_cache_manager->GetCacheFromContextId(url.GetContextId()) : NULL;
	HTTP_Method method = static_cast<HTTP_Method>(url.GetAttribute(URL::KHTTP_Method));

	if ((method != HTTP_METHOD_POST && method != HTTP_METHOD_PUT && cache && cache->IsCached(url) == FALSE) || (cache && url.GetAttribute(URL::KIsApplicationCacheForeign)))
	{
		/* The url is loaded from an application cache context. However, the
		 * url does not exit in the cache. Since this url is loaded on top level,
		 * the document may not actual be offline application. Thus we initiate
		 * the load from default cache, otherwise we'll get a load error. If this
		 * happens to be a offline application after all, this will be taken care
		 * of by GetURL() or by the parser if the document contains a manifest
		 * attribute.
		 *
		 * Also if the application is a foreign document, load from default cache.
		 *
		 */
		OpString url_str;
		OpStatus::Ignore(url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI, url_str));

		OP_ASSERT(url.GetContextId() != window->GetUrlContextId());
		url = g_url_api->GetURL(url_str, window->GetUrlContextId());
	}
#endif // APPLICATION_CACHE

	const uni_char *hostname;

	if (!window->GetWindowCommander()->GetLoadingListener()->OnAboutToLoadUrl(
			window->GetWindowCommander(),
			url.Name(PASSWORD_NOTHING),
			GetFrame() != NULL //If we got a frame, we are an inline
	))
		return; //Exit if rejected!

	start_navigation_time_ms = g_op_time_info->GetRuntimeMS();

	int priority;
	if (GetParentDoc())
		if (user_initiated)
			priority = FramesDocument::LOAD_PRIORITY_FRAME_DOCUMENT_USER_INITIATED;
		else
			priority = FramesDocument::LOAD_PRIORITY_FRAME_DOCUMENT;
	else
		if (user_initiated)
			priority = FramesDocument::LOAD_PRIORITY_MAIN_DOCUMENT_USER_INITIATED;
		else
			priority = FramesDocument::LOAD_PRIORITY_MAIN_DOCUMENT;

	url.SetAttribute(URL::KHTTP_Priority, priority);

#ifdef NEARBY_ELEMENT_DETECTION
	window->SetElementExpander(NULL);
#endif // NEARBY_ELEMENT_DETECTION

	// URL must be constructed with a context id in privacy mode
	OP_ASSERT(!window->GetPrivacyMode() || url.GetContextId());
#ifdef HTTP_CONTENT_USAGE_INDICATION
	if( GetFrame() != NULL ) // This is not the main document
		url.SetAttribute(URL::KHTTP_ContentUsageIndication, HTTP_UsageIndication_OtherInline);
	else
		url.SetAttribute(URL::KHTTP_ContentUsageIndication, HTTP_UsageIndication_MainDocument);
#endif // HTTP_CONTENT_USAGE_INDICATION

	if (doc)
		hostname = doc->GetHostName();
	else
		hostname = NULL;

#ifdef SUPPORT_VISUAL_ADBLOCK
	// cache the server name so it's faster to do a lookup to check if the content blocker is enabled or not
	if (doc && !parent_doc)
		if (ServerName *server_name = doc->GetURL().GetServerName())
			window->SetContentBlockServerName(server_name);
#endif // SUPPORT_VISUAL_ADBLOCK

	BOOL pref_frames_enabled = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::FramesEnabled, hostname);
	BOOL ignore_url_request = IgnoreURLRequest(url, options.origin_thread);

	CheckExpiryType pref_check_expiry = (CheckExpiryType)
		g_pcnet->GetIntegerPref(is_walking_in_history
		                        ? PrefsCollectionNetwork::CheckExpiryHistory
		                        : PrefsCollectionNetwork::CheckExpiryLoad);

	// this thing is to clear URLCameFromAddressField flag when going to a new url
	// It is needed for the UI to be able to know if it should set focus back to
	// address field or not when stopping transfer. (Trond)
	if (!es_terminated)
		GetWindow()->SetURLCameFromAddressField(FALSE);

	// We set this to TRUE if we show an error page instead of the page itself.
	// This is so that we can return an error status to the document listener
	// even if the error page itself loads.
	error_page_shown = FALSE;

	// If we are opening another URL asynchronously, let it override this URL.
	if (es_terminated && !url_load_on_command.IsEmpty() && !(url_load_on_command == url)
		&& !(url_load_on_command == current_url && url_load_on_command.RelName()))
		return;

#ifdef SUPPORT_VERIZON_EXTENSIONS
    url.SetAttribute( URL::KStoreAllHeaders, TRUE);
#endif //SUPPORT_VERIZON_EXTENSIONS

	if (security_check_callback && security_check_callback->m_suspended && !security_check_callback->m_done)
	{
		/* The previous security check didn't finish and we're loading
		 * a new one now, so cut the previous one loose, have it ignore
		 * the result and start a new one.
		 */
		security_check_callback->Invalidate();
		security_check_callback = NULL;
	}

	if (!load_image_only && !save_image_only && !bypass_url_access_check)
	{
		if (!security_check_callback)
		{
			security_check_callback = OP_NEW(SecurityCheckCallback, (this));
			if (!security_check_callback)
				return;
		}

		OP_ASSERT(!security_check_callback->m_suspended || security_check_callback->m_done);

		if (!security_check_callback->m_suspended)
		{
			SecCheckRes res = InitiateSecurityCheck(url, referrer);
			if (res == SEC_CHECK_STARTED)
				security_check_callback->PrepareForSuspending(check_if_expired, reload, options);

			if (res != SEC_CHECK_ALLOWED)
				return;
		}

		OP_ASSERT(!security_check_callback->m_suspended || security_check_callback->m_allowed);
		security_check_callback->Reset();
	}
	else
	{
		OP_DELETE(security_check_callback);
		security_check_callback = NULL;
	}

#ifdef ON_DEMAND_PLUGIN
	if (FramesDocElm *frame = GetFrame())
		if (HTML_Element *frame_element = frame->GetHtmlElement())
			if (frame_element->IsPluginPlaceholder())
				CreatePluginPlaceholderUrl(frame->GetParentFramesDoc()->GetURL(), url);
#endif // ON_DEMAND_PLUGIN

	check_expiry = check_if_expired ? pref_check_expiry : CHECK_EXPIRY_NEVER;
	history_walk = is_walking_in_history && frame ? frame->GetDocManager()->GetHistoryDirection() : DM_NOT_HIST;

	FramesDocument* current_doc = GetCurrentDoc();
	OP_MEMORY_VAR BOOL relative_jump = !reload && !options.ignore_fragment_id && IsRelativeJump(url);

	// Relative jump within the document.
	// But it's not a jump if the document being opened is an inline
	// webfeed in which case a generated viewer page needs to be opened
	// even though the URL looks like a relative jump.
	if (relative_jump && !options.is_inline_feed)
	{
		OP_BOOLEAN result = JumpToRelativePosition(url);

		if (result == OpBoolean::IS_TRUE)
			return;
	}

	if (window->GetOOMOccurred() && !(url == current_url))
	{
		UnloadCurrentDoc();

		es_terminated = TRUE;
	}
	window->SetOOMOccurred(FALSE);

	if (!es_terminated)
	{
		if (current_doc)
		{
			BOOL need_termination = current_doc->ESNeedTermination(url, reload);

			if (url.Type() == URL_JAVASCRIPT)
			{
				OP_STATUS ret = current_doc->ConstructDOMEnvironment();

				if (OpStatus::IsSuccess(ret))
					if (DOM_Environment *environment = current_doc->GetDOMEnvironment())
						if (environment->IsEnabled())
						{
							ret = current_doc->ESOpenURL(url, referrer, check_if_expired, reload, FALSE, options, entered_by_user == WasEnteredByUser, FALSE, FALSE);
							if (OpStatus::IsSuccess(ret))
							{
								SetCurrentURL(url, FALSE);
								current_url_is_inline_feed = options.is_inline_feed;
								SetLoadStat(WAIT_FOR_HEADER);
								// Load will be finished when the ES_Thread created by ESOpenURL
								// finishes.
								OP_ASSERT(current_doc->GetESScheduler() && current_doc->GetESScheduler()->HasRunnableTasks());
							}
							else if (ret == OpStatus::ERR)
								ret = OpStatus::OK;
						}

				if (OpStatus::IsError(ret))
					RaiseCondition(ret);

				return;
			}
			else if (need_termination)
			{
				/* Let the ES scheduler finish current tasks and fire an unload event.
				   After that it will call this function again with es_terminated==TRUE. */

				OP_STATUS ret = current_doc->ESOpenURL(url, referrer, check_if_expired, reload, GetRedirect(),
				                                       options, FALSE, FALSE, FALSE, GetUserAutoReload());

				if (OpStatus::IsMemoryError(ret))
					RaiseCondition(ret);

				return;
			}
		}
	}

#ifdef LIBOPERA
	window->SetUserInitiated(user_initiated);
#endif

//	This call is moved to after HandleHeaderLoaded is done, the information was needed by WML in a later stage.
//	If this leads to any problem, contact pw@opera.com.
//	window->SetURLCameFromAddressField(FALSE);

	if (is_clearing)
		return;

#ifdef _PRINT_SUPPORT_
	if (print_preview_vd)
		window->TogglePrintMode(TRUE);
#endif // _PRINT_SUPPORT_

	OP_MEMORY_VAR BOOL raise_OOM_condition = FALSE;

	if (user_initiated)
		GetWindow()->SetHasShownUnsolicitedDownloadDialog(FALSE); // Allow a new download dialog after the user has requested a new document

	URLType url_type = url.Type();

	BOOL unwanted_recursive_frame_load = !user_initiated && IsRecursiveDocumentOpening(url, options.from_html_attribute);
	if ((url.IsEmpty() || unwanted_recursive_frame_load) && !current_doc_elm)
	{
		OP_STATUS ret = CreateInitialEmptyDocument(FALSE, FALSE);
		if (OpStatus::IsError(ret))
			RaiseCondition(ret);
		else
			OP_ASSERT(current_doc_elm);
	}

	if (unwanted_recursive_frame_load)
		return;

	if (url.IsEmpty() && !create_doc_now)
	{
		OP_STATUS ret = HandleDocFinished();

		if (OpStatus::IsError(ret))
			RaiseCondition(ret);

		return;
	}

#ifdef _KIOSK_MANAGER_
	if(url_type == URL_MAILTO && KioskManager::GetInstance()->GetNoMailLinks())
		return;
#endif // _KIOSK_MANAGER_

	if (url.Type() != URL_JAVASCRIPT && !current_url.IsEmpty() && !(current_url == url))
	{
		// N.B. Some aspects of OpenURL() rely on the state of the document manager so we can't use DocumentManager::StopLoading() here.
		// It would reset the state and make OpenURL() do the wrong thing (e.g. use_history_number)
		current_url.StopLoading(mh);

		if (doc && request_thread_info.type == ES_THREAD_EMPTY)
			doc->StopLoading(FALSE, FALSE, FALSE);
	}

#if defined WEB_HANDLERS_SUPPORT || defined QUICK
	const OP_BOOLEAN status = ignore_url_request
#if defined WEB_HANDLERS_SUPPORT
	                          || skip_protocol_handler_check
#endif // WEB_HANDLERS_SUPPORT
							  ? OpBoolean::IS_FALSE : AttemptExecuteURLsRecognizedByOpera(url, referrer, user_initiated, entered_by_user == WasEnteredByUser);
#if defined WEB_HANDLERS_SUPPORT
	skip_protocol_handler_check = FALSE;
#endif // WEB_HANDLERS_SUPPORT
	if (OpStatus::IsMemoryError(status))
		raise_OOM_condition = TRUE;
	else if (status == OpBoolean::IS_TRUE)
		return;
#endif // WEB_HANDLERS_SUPPORT || QUICK
	if (g_url_api->LoadAndDisplayPermitted(url))
	{
		// Do not ask if offline. We will be asked again later after offline->online toggling.
		BOOL is_offline = FALSE;

		RaiseCondition(UpdateOfflineModeHelper(FALSE, is_offline));

		window->SetState(CLICKABLE);

		SetLoadStat(WAIT_FOR_HEADER);

		if ((entered_by_user == WasEnteredByUser || pending_url_user_initiated)
			&& user_started_loading == FALSE)
		{
			user_started_loading = TRUE;
		}

		user_initiated_loading = user_initiated;

#ifdef ABSTRACT_CERTIFICATES
		if (url.GetAttribute(URL::KType) == URL_HTTPS)
			if (OpStatus::IsMemoryError(url.SetAttribute(URL::KCertificateRequested, TRUE)))
				raise_OOM_condition = TRUE;
#endif // ABSTRACT_CERTIFICATES

		SetReload(reload);

		BOOL es_force_reload = url_replace_on_command;
		URL tmpurl = URL();  // this is needed because of ARM compiler issue
		SetUrlLoadOnCommand(tmpurl, referrer, FALSE, entered_by_user == WasEnteredByUser);

        if (!save_image_only)
        {
			if (window->SetProgressText(url) == OpStatus::ERR_NO_MEMORY)
				raise_OOM_condition = TRUE;
        }

		// do this check before window->StartProgressDisplay() is called !!!
		BOOL update_security_state = (GetSubWinId() != -1 && (!window->IsLoading() || !current_url.IsEmpty()));

		if (!(current_url == url))
			SetAction(VIEWER_NOT_DEFINED);

#ifdef WEB_HANDLERS_SUPPORT
		SetActionLocked(FALSE);
#endif // WEB_HANDLERS_SUPPORT

		// We must call SetCurrentURL() before calling StartProgressDisplay().
		SetCurrentURL(url, FALSE);
		current_url_is_inline_feed = options.is_inline_feed;
		DropCurrentLoadingOrigin();
		if (!IsURLSuitableSecurityContext(url))
		{
			if (referrer.IsEmpty())
			{
				if (IsAboutBlankURL(url))
					current_loading_origin = DocumentOrigin::MakeUniqueOrigin(url.GetContextId(), FALSE);
			}
			else if (referrer.origin)
			{
				// Inherit security from the referrer.
				current_loading_origin = referrer.origin;
				current_loading_origin->IncRef();
			}
			else
				current_loading_origin = DocumentOrigin::Make(referrer.url);

			if ((!referrer.IsEmpty() || IsAboutBlankURL(url)) && !current_loading_origin)
				raise_OOM_condition = TRUE;
		}

		if (url.Type() != URL_JAVASCRIPT)
			CancelRefresh();

		initial_request_url = url;

#ifdef TRUST_RATING
		if (!parent_doc)
		{
			if (entered_by_user || user_initiated)
				m_active_trust_checks.Clear();  // Abandon active checks when going to a new top level user initiated URL
		}

		OP_STATUS status = CheckTrustRating(url);
		if (status == OpStatus::ERR_NO_ACCESS)
			return;
		if (OpStatus::IsMemoryError(status))
			raise_OOM_condition = TRUE;
#endif // TRUST_RATING

		// remember referrer url
#ifdef _WML_SUPPORT_
		BOOL send_referrer = reload ? ShouldSendReferrer()
			: (!WMLGetContext() || WMLGetContext()->IsSet(WS_SENDREF));
		SetReferrerURL(referrer, send_referrer);

		if (entered_by_user == WasEnteredByUser && wml_context)
			wml_context->SetStatusOn(WS_USERINIT);

#else // _WML_SUPPORT_
		SetReferrerURL(referrer, TRUE);
#endif // _WML_SUPPORT_

		NotifyUrlChangedIfAppropriate(url);

#ifdef OPERA_PERFORMANCE
		if (user_initiated || entered_by_user == WasEnteredByUser)
			urlManager->OnUserInitiatedLoad();
#endif // OPERA_PERFORMANCE

		// only force the reset of security state if we are the top document
        if (!save_image_only)
        {
            // only force the reset of security state if we are the top document
			if (window->StartProgressDisplay(TRUE, !parent_doc /* bResetSecurityState */, !!parent_doc /* bSubResourcesOnly */) == OpStatus::ERR_NO_MEMORY)
				raise_OOM_condition = TRUE;

			if (!parent_doc)
				// StartProgressDisplay won't do it if the progress bar is already visible, but it should still be done.
				window->ResetSecurityStateToUnknown();

			// window->StartProgressDisplay() will set SecurityState to SECURITY_STATE_UNKNOWN
			if (update_security_state)
				window->DocManager()->UpdateSecurityState(FALSE);
		}

		BOOL from_cache_ref;
		if (GetSubWinId() < 0)
		{
			from_cache_ref = window->AlwaysLoadFromCache();
			window->SetUserLoadFromCache(from_cache_ref);
		}
		else
			from_cache_ref = window->GetUserLoadFromCache();

		if (frame && window->IsURLAlreadyRequested(url) && url.Status(TRUE) == URL_LOADED)
			from_cache_ref = TRUE;

		BOOL if_modified_only = FALSE;
		BOOL force_user_reload = FALSE;

		if (!es_force_reload)
		{
			if_modified_only = !!url.GetAttribute(URL::KStillSameUserAgent);

#if defined(WEB_TURBO_MODE) && defined(HTTP_CONTENT_USAGE_INDICATION)
			// Always perform full reload of Turbo mode images opened or saved separately
			if (if_modified_only && reload_document && (load_image_only || save_image_only) &&
				url.GetAttribute(URL::KUsesTurbo) &&
				url.GetAttribute(URL::KHTTP_ContentUsageIndication) == HTTP_UsageIndication_MainDocument)
			{
				if_modified_only = FALSE;
			}
#endif // WEB_TURBO_MODE && HTTP_CONTENT_USAGE_INDICATION

			if (GetUserAutoReload())
			{
				if_modified_only = TRUE;
				force_user_reload = TRUE;
			}
			else if (reload && current_doc && current_doc->GetURL() == url && (!current_doc->IsFrameDoc() || !pref_frames_enabled))
				if_modified_only = FALSE;
		}


		URL_Reload_Policy reloadpolicy;

		if (from_cache_ref || relative_jump || !check_if_expired)
		{
			if (url.GetAttribute(URL::KCachePolicy_MustRevalidate, TRUE) || force_user_reload)
				reloadpolicy = URL_Reload_Conditional;
			else if (url.Status(FALSE) != URL_LOADED)
				reloadpolicy = URL_Reload_If_Expired;
			else if (!check_if_expired && reload)
				reloadpolicy = URL_Reload_Full;
			else
				reloadpolicy = URL_Reload_None;
		}
		else if (reload_document)
			if (if_modified_only || conditionally_request_document)
				reloadpolicy = URL_Reload_Conditional;
			else
				reloadpolicy = URL_Reload_Full;
		else
			reloadpolicy = URL_Reload_If_Expired;

		if (window->GetOnlineMode() == Window::DENIED)
			reloadpolicy = URL_Reload_None;

		URL_LoadPolicy loadpolicy(FALSE, reloadpolicy, user_initiated || entered_by_user == WasEnteredByUser);

#ifdef SCOPE_DOCUMENT_MANAGER
		if (OpScopeDocumentListener::IsEnabled())
		{
			OpScopeDocumentListener::AboutToLoadDocumentArgs args;
			args.url = url.GetRep();
			args.referrer_url = URL(referrer.url).GetRep();
			args.check_if_expired = check_if_expired;
			args.reload = reload;
			args.options = options;

			OpStatus::Ignore(OpScopeDocumentListener::OnAboutToLoadDocument(this, args));
		}
#endif // SCOPE_DOCUMENT_MANAGER

		if (url == current_url_reserved.GetURL())
			/* Need to unset it before calling LoadDocument(), since it would
			   otherwise block reload. */
			current_url_reserved.UnsetURL();

		HTMLLoadContext* ctx_ptr = NULL;

#ifdef URL_FILTER
		FramesDocument *initiating_doc = NULL;

		if (options.origin_thread && options.origin_thread->GetScheduler())
			initiating_doc = options.origin_thread->GetScheduler()->GetFramesDocument();
		else if (parent_doc)
			initiating_doc = parent_doc;
		else
			initiating_doc = current_doc;

		DOM_Environment *dom_env = NULL;
		ServerName *doc_sn = NULL;
		BOOL is_widget = FALSE;

		if (initiating_doc)
		{
			dom_env = initiating_doc->GetDOMEnvironment();
			doc_sn = initiating_doc->GetURL().GetServerName();
			is_widget = initiating_doc->GetWindow()->GetType() == WIN_TYPE_GADGET;
		}

		URLFilterDOMListenerOverride lsn_over(dom_env, NULL);
		HTMLLoadContext ctx(parent_doc ? RESOURCE_SUBDOCUMENT : RESOURCE_DOCUMENT, doc_sn, &lsn_over, is_widget);

		ctx_ptr = &ctx;
#endif // URL_FILTER

#ifdef _WML_SUPPORT_
		CommState state = url.LoadDocument(mh, send_referrer ? referrer.url : URL(), loadpolicy, ctx_ptr);
#else // _WML_SUPPORT_
		CommState state = url.LoadDocument(mh, referrer.url, loadpolicy, ctx_ptr);
#endif //  _WML_SUPPORT_

		if (state != COMM_REQUEST_FAILED)
		{
			window->SetURLAlreadyRequested(url);

			if (OpStatus::IsMemoryError(current_url_reserved.SetURL(url)))
				raise_OOM_condition = TRUE;

			scroll_to_fragment_in_document_finished = !reload;
		}
	}
	else if (url_type == URL_JAVASCRIPT && g_pcjs->GetIntegerPref(PrefsCollectionJS::EcmaScriptEnabled, referrer.url))
	{
		/* All urls loaded in frames documents are handled above, so if we get
		   here we either have no document (in which case we create a new one)
		   or we have a document that isn't a frames document (in which case
		   we just report an error since javascript urls can only be executed
		   in frames documents). */
		OP_ASSERT(!current_doc);

		// We have no document to execute in.  Create one.

		if (!current_url.IsEmpty())
			/* Security check. */
			if (current_url.Type() != referrer.url.Type() || !current_url.SameServer(referrer.url, URL::KCheckPort))
				return;

		// We must call SetCurrentURL() so that the scripts get the right referrer
		SetCurrentURL(url, FALSE);
		current_url_is_inline_feed = options.is_inline_feed;
		SetReferrerURL(referrer); // remember referrer url

		DropCurrentLoadingOrigin();
		if (!referrer.IsEmpty())
		{
			if (referrer.origin)
			{
				// Inherit security from the referrer.
				current_loading_origin = referrer.origin;
				current_loading_origin->IncRef();
			}
			else
				current_loading_origin = DocumentOrigin::Make(referrer.url);

			if (!current_loading_origin)
				raise_OOM_condition = TRUE;
		}

		create_doc_now = FALSE;
		use_current_doc = TRUE;
		if (OpStatus::IsMemoryError(SetCurrentDoc(check_if_expired, FALSE, FALSE, user_initiated)))
			raise_OOM_condition = TRUE;
		use_current_doc = FALSE;

		current_doc = GetCurrentDoc();

		/* No security checking is necessary here since this is a new, empty
		   document that we just created.  In fact, it would almost certainly fail
		   since the url of the document is the javascript url, not an url that is
		   compatible with the origin url. */
		OP_STATUS ret = current_doc->ESOpenJavascriptURL(url, TRUE, FALSE, reload, FALSE, NULL, entered_by_user == WasEnteredByUser, FALSE, FALSE);

		if (OpStatus::IsMemoryError(ret))
			raise_OOM_condition = TRUE;
		else if (OpStatus::IsError(ret))
			return;
	}

	else if (url_type == URL_MAILTO)
	{
		if (!ignore_url_request)
		{
			OpString8 servername;
			BOOL mailto_from_form = !!url.GetAttribute(URL::KHTTPIsFormsRequest);
			if (mailto_from_form)
			{
				if (current_url.GetServerName())
					servername.Set(current_url.GetServerName()->Name());
			}
			window->GenericMailToAction(url, options.called_externally, mailto_from_form, servername);
		}
	}
#if defined(M2_SUPPORT) && defined(IRC_SUPPORT)
	else if (url_type == URL_IRC || url_type == URL_CHAT_TRANSFER)
	{
		if (!ignore_url_request && g_m2_engine && OpStatus::IsMemoryError(g_m2_engine->MailCommand(url)))
			raise_OOM_condition = TRUE;
		else
		{
			if (url_type == URL_CHAT_TRANSFER && window->GetType() != WIN_TYPE_IM_VIEW)
				window->Close();
			return;
		}
	}
#endif // M2_SUPPORT && IRC_SUPPORT
#if !defined(NO_FTP_SUPPORT) || defined(GOPHER_SUPPORT) || defined(WAIS_SUPPORT)
	else if (url_type == URL_FTP || url_type == URL_Gopher || url_type == URL_WAIS)
	{
		if (!ignore_url_request)
		{
			if (entered_by_user != WasEnteredByUser && !user_initiated)
			{
#ifdef OPERA_CONSOLE
				PostURLErrorToConsole(url, Str::SI_ERR_REQUIRES_PROXY_SERVER);
#endif // OPERA_CONSOLE
			}
			else
				window->HandleLoadingFailed(Str::SI_ERR_REQUIRES_PROXY_SERVER, url);
		}
	}
#endif // !NO_FTP_SUPPORT || GOPHER_SUPPORT || WAIS_SUPPORT
#ifdef IMODE_EXTENSIONS
	else if (url_type == URL_TEL)
	{
		OpString tel_str;
		BOOL bHandled = FALSE;

		if (OpStatus::IsMemoryError(url.GetAttribute(URL::KUniName_For_Tel, tel_str, FALSE)))
			raise_OOM_condition = TRUE;
		else
			bHandled = window->GetWindowCommander()->GetDocumentListener()->OnUnknownProtocol(window->GetWindowCommander(), tel_str.CStr());

		if (!bHandled)
		{
			SetCurrentURL(url, FALSE);
			current_url_is_inline_feed = options.is_inline_feed;
			HandleLoadingFailed(url.Id(), DH_ERRSTR(SI,ERR_UNKNOWN_ADDRESS_TYPE));
		}
	}
#endif // IMODE_EXTENSIONS
	else
	{
		BOOL bHandled = FALSE;

		WindowCommander* wic = window->GetWindowCommander();
		// Need to copy string, since UniName (and cousins) use tempbuffers.
		OpString url_str;
		if (OpStatus::IsMemoryError(url.GetAttribute(
						URL::KUniName_With_Fragment_Username_Password_Hidden_Escaped,
						url_str)))
			raise_OOM_condition = TRUE;
		else
			bHandled = wic->GetDocumentListener()->OnUnknownProtocol(wic, url_str.CStr());

#if defined MSWIN || defined _UNIX_DESKTOP_ || defined _MACINTOSH_
#ifdef QUICK
		bHandled = Application::HandleExternalURLProtocol(url);
#endif // QUICK
#endif // MSWIN || UNIX_DESKTOP || _MACINTOSH_

		if (!bHandled && !create_doc_now)
		{
			if (!ignore_url_request)
			{
				if (entered_by_user != WasEnteredByUser && !user_initiated)
				{
#ifdef OPERA_CONSOLE
					PostURLErrorToConsole(url, Str::SI_ERR_UNKNOWN_ADDRESS_TYPE);
#endif // OPERA_CONSOLE
				}
				else
				{
					SetCurrentURL(url, FALSE);
					current_url_is_inline_feed = options.is_inline_feed;
					HandleLoadingFailed(url.Id(), DH_ERRSTR(SI,ERR_UNKNOWN_ADDRESS_TYPE));
				}
			}
		}
	}

	if (create_doc_now)
	{
		use_current_doc = TRUE;
		SetCurrentURL(url, FALSE);
		current_url_is_inline_feed = options.is_inline_feed;
		SetReferrerURL(referrer, ShouldSendReferrer());

		if (OpStatus::IsMemoryError(SetCurrentDoc(check_if_expired, FALSE, FALSE, user_initiated)))
			raise_OOM_condition = TRUE;
		else
			current_doc_elm->SetIsPreCreated(TRUE);
		use_current_doc = FALSE;
	}

	if (raise_OOM_condition)
		RaiseCondition(OpStatus::ERR_NO_MEMORY);
}

DocumentReferrer::DocumentReferrer(FramesDocument* doc)
 : url(doc->GetSecurityContext()),
   origin(doc->GetMutableOrigin())
{
	origin->IncRef();
}

DocumentReferrer::DocumentReferrer(DocumentOrigin* ref_origin)
	: url(ref_origin->security_context),
	  origin(ref_origin)
{
	origin->IncRef();
}

DocumentReferrer::DocumentReferrer(const DocumentReferrer& other)
	: url(other.url),
	  origin(other.origin)
{
	if (origin)
		origin->IncRef();
}
DocumentReferrer& DocumentReferrer::operator=(const DocumentReferrer& other)
{
	url = other.url;
	if (other.origin)
		other.origin->IncRef();
	if (origin)
		origin->DecRef();
	origin = other.origin;
	return *this;
}


DocumentReferrer::~DocumentReferrer()
{
	if (origin)
		origin->DecRefInternal();
}

void DocumentManager::OpenURL(URL& url, const DocumentReferrer& referrer, BOOL check_if_expired, BOOL reload, BOOL user_initiated, BOOL create_doc_now, EnteredByUser entered_by_user, BOOL is_walking_in_history, BOOL es_terminated, BOOL called_externally, BOOL bypass_url_access_check)
{
	OpenURLOptions options;
	options.user_initiated = user_initiated;
	options.create_doc_now = create_doc_now;
	options.entered_by_user = entered_by_user;
	options.is_walking_in_history = is_walking_in_history;
	options.es_terminated = es_terminated;
	options.called_externally = called_externally;
	options.bypass_url_access_check = bypass_url_access_check;
	OpenURL(url, referrer, check_if_expired, reload, options);
}

void
DocumentManager::OpenImageURL(URL url, const DocumentReferrer& referrer, BOOL save, BOOL new_page, BOOL in_background)
{
	if (url.Type() == URL_JAVASCRIPT || (url.GetAttribute(URL::KHeaderLoaded) && !url.GetAttribute(URL::KIsImage,TRUE)))
		return;

	DocumentManager *docman;

	if (new_page)
	{
		BOOL3 new_window = YES;
		BOOL3 open_in_background = in_background ? YES : MAYBE;

		Window *window = g_windowManager->GetAWindow(TRUE, new_window, open_in_background);

		if (!window)
			return;

		docman = window->DocManager();
	}
	else
	{
		docman = this;

		if (FramesDocument *doc = GetCurrentDoc())
			if (ES_ThreadScheduler *scheduler = doc->GetESScheduler())
			{
				scheduler->RemoveThreads(TRUE);
				OpStatus::Ignore(scheduler->Activate());
			}
	}

	if (save)
    {
		docman->save_image_only = TRUE;
        docman->SetActionFlag(ViewActionFlag::SAVE_AS);
    }
	else
    {
		docman->load_image_only = TRUE;
    }

	OpenURLOptions options;
	options.user_initiated = TRUE;
	options.es_terminated = TRUE;
	options.ignore_fragment_id = save;
	BOOL img_reload = FALSE;
#ifdef WEB_TURBO_MODE
	if (url.GetAttribute(URL::KUsesTurbo) && url.GetAttribute(URL::KTurboCompressed))
		img_reload = TRUE;
#endif // WEB_TURBO_MODE
	docman->OpenURL(url, referrer, TRUE, img_reload, options);
}

void
DocumentManager::SetUrlLoadOnCommand(const URL& url, const DocumentReferrer& ref_url, BOOL replace_document/* = FALSE*/, BOOL was_user_initiated/* = FALSE*/, ES_Thread *origin_thread/* = NULL*/)
{
	pending_url_user_initiated = was_user_initiated;

	// Workaround for links of the type <a href="#rel" onclick="document.form[0].submit()">
	// where the "#rel" URL will otherwise override the form submit because it is
	// triggered later.  This code simply makes us ignore the "#rel" URL in that case.
	if (!url_load_on_command.IsEmpty() && IsRelativeJump(url))
		return;

	if (IgnoreURLRequest(url))
		return;

	url_load_on_command = url;
	url_replace_on_command = replace_document;
	referrer_url = ref_url;
}

void DocumentManager::SetApplication(const uni_char *str)
{
	if (!current_application)
		current_application = OP_NEWA(uni_char, 256);

	if (current_application)
	{
		if (str && *str)
		{
			uni_strncpy(current_application, str, 255);
			current_application[255] = 0;
		}
		else
			current_application[0] = 0;
	}
}

BOOL DocumentManager::IsCurrentDocLoaded(BOOL inlines_loaded)
{
	switch (GetLoadStatus())
	{
	default:
		return FALSE;

	case NOT_LOADING:
	case DOC_CREATED:
		if (FramesDocument* doc = GetCurrentDoc())
			return doc->IsLoaded(inlines_loaded);
		else
			return TRUE;
	}
}

void DocumentManager::ResetStateFlags()
{
	BOOL was_reloaded_previously = GetReload();
	BOOL were_inlines_reloaded_previously = GetReloadInlines();
	BOOL were_inlines_requested_conditionally_previously = GetConditionallyRequestInlines();
	SetUserAutoReload(FALSE);
	SetReload(FALSE);
	SetRedirect(FALSE);
	was_reloaded = was_reloaded_previously;
	were_inlines_reloaded = were_inlines_reloaded_previously;
	were_inlines_requested_conditionally = were_inlines_requested_conditionally_previously;
	SetReplace(FALSE);
	SetUseHistoryNumber(-1);
	SetAction(VIEWER_NOT_DEFINED);
	load_image_only = FALSE;
	save_image_only = FALSE;
	use_current_doc = FALSE;
	initial_request_url = URL();
	conditionally_request_document = FALSE;
	conditionally_request_inlines = TRUE;
}

void DocumentManager::StopLoading(BOOL format, BOOL force_end_progress/*=FALSE*/, BOOL abort/*=FALSE*/)
{
#ifdef _PRINT_SUPPORT_
	if (print_preview_vd)
		return;
#endif // _PRINT_SUPPORT_

#ifdef NEARBY_ELEMENT_DETECTION
	window->SetElementExpander(NULL);
#endif // NEARBY_ELEMENT_DETECTION

	g_url_api->SetPauseStartLoading(TRUE);

	ResetStateFlags();

	URLStatus current_url_stat;

	if (!current_url.IsEmpty())
	{
		current_url_stat = current_url.Status(TRUE);
		current_url.StopLoading(mh);
	}
	else
		current_url_stat = URL_UNLOADED;

	if (FramesDocument* doc = GetCurrentDoc())
	{
		current_doc_elm->SetIsPreCreated(FALSE);

		URL old_url = doc->GetURL();

		// Stop as much as possible of the document loading. Keep
		// plugins streams around in case we get a 204 response
		// (CORE-8121, CORE-22650).

		// OOM7: Ignore failure - writing to cache isn't critical.
		// Raising condition can lead to repeated calls of this function.
		OpStatus::Ignore(doc->StopLoading(format, abort, abort));

		// if the loading is aborted or failed (and this is the top document) we want the failed URL in the address field
		// so we set the current_url to URL() so that OnUrlChanged() wont be called.
		if (!GetFrame() && current_url_stat == URL_LOADING_FAILURE || current_url_stat == URL_LOADING_ABORTED)
			SetCurrentURL(URL(), FALSE);
		else if (!(current_url == old_url))
			SetCurrentURL(old_url, FALSE);

		if (GetParentDoc() == NULL) // only for the top document
		{
			window->ResetSecurityStateToUnknown();
		}

		/* A child doc manager should not update the security status of the window if a parent doc
		 * manager is waiting for some url to load or about to switch documents. This happens, for
		 * instance, when StopLoading  is called on a doc manager in the middle of the tree instead
		 * of the top-level doc manager.
		 * A parent doc manager will update the security status when it finished loading or the
		 * load is aborted, so doing it on the children also is wasteful and can lead to the state
		 * to be updated wrongfully (e.g. CORE-24298). */
		BOOL update_security_status = TRUE;
		for (FramesDocument* frames_doc = GetParentDoc(); frames_doc; frames_doc = frames_doc->GetDocManager()->GetParentDoc())
		{
			if (frames_doc->GetDocManager()->GetLoadStatus() != NOT_LOADING &&
				frames_doc->GetDocManager()->GetLoadStatus() != WAIT_MULTIPART_RELOAD)
			{
				update_security_status = FALSE;
				break;
			}
		}

		if (update_security_status)
			window->DocManager()->UpdateSecurityState(FALSE);
	}
	else
		SetCurrentURL(URL(), FALSE);

	scroll_to_fragment_in_document_finished = FALSE;
	SetLoadStat(NOT_LOADING);
	/* Robustness call in case VisualDevice doesn't notify that it has
	 * been unlocked. */
	CheckOnNewPageReady();

	g_url_api->SetPauseStartLoading(FALSE);

	// If this is the last document in the currently loading tree then
	// call EndProgressDisplay to unlock the painter and trigger a paint. If
	// this on the other hand is while we're undisplaying, then the tree
	// is in a state of flux and calling EndProgressDisplay could result
	// in unwanted unlocking and paints.
	BOOL is_undisplaying = FALSE;
	for (FramesDocument* doc = GetParentDoc(); doc; doc = doc->GetParentDoc())
	{
		if (doc->IsUndisplaying())
		{
			is_undisplaying = TRUE;
			break;
		}
	}
	if (!is_undisplaying)
		EndProgressDisplay(force_end_progress);

	if (es_pending_unload)
		OpStatus::Ignore(ESCancelPendingUnload());
}

void DocumentManager::LoadPendingUrl(URL_ID url_id, BOOL user_initiated)
{
	if (is_clearing)
		return;

	if (url_load_on_command.Id(TRUE) == url_id)
	{
		URL url = url_load_on_command;

		// Clear url; we don't want to trigger this twice

		url_load_on_command = URL();

		if (url_replace_on_command)
			SetReplace(TRUE);

		// ("On SHARP branch we always use ref_url = current_url")
		DocumentReferrer ref_url = referrer_url;
		if (ref_url.IsEmpty())
			ref_url = DocumentReferrer(current_url);

		// To avoid reloading internal links
		BOOL check_if_expired = TRUE;

		if ((ref_url.url == url) && url.RelName())
			check_if_expired = FALSE;

		OpenURL(url, ref_url, check_if_expired, url_replace_on_command, user_initiated);
	}
}

OP_BOOLEAN DocumentManager::LoadAllImages()
{
#ifdef _PRINT_SUPPORT_
	if (print_preview_vd)
		return OpBoolean::IS_FALSE;
#endif // _PRINT_SUPPORT_

	if (FramesDocument* doc = GetCurrentDoc())
	{
		BOOL is_reloading = GetReload();

		if (!is_reloading)
			SetReload(TRUE);

		if (!window->ShowImages())
			window->SetImagesSetting(FIGS_SHOW);

		doc->LoadAllImages(FALSE);

		if (!is_reloading)
			SetReload(FALSE);
	}

	return OpBoolean::IS_FALSE;
}

void DocumentManager::Reload(EnteredByUser entered_by_user, BOOL conditionally_request_document, BOOL conditionally_request_inlines, BOOL is_user_auto_reload)
{
	if (!GetWindow()->HasFeature(WIN_FEATURE_RELOADABLE)
#ifdef ERROR_PAGE_SUPPORT
	    || (current_url.Type() == URL_OPERA && current_url.GetAttribute(URL::KIsClickThroughPage)) // opera:site-warning and opera:crossnetworkwarning are not refreshable else they just turn into an error page
#endif // ERROR_PAGE_SUPPORT
	)
		return;

#ifdef GADGET_SUPPORT
	if (GetWindow()->GetGadget())
		GetWindow()->GetGadget()->Reload();
#endif // GADGET_SUPPORT

	if (is_clearing)
		return;

#ifdef _PRINT_SUPPORT_
	if (print_preview_vd)
		window->TogglePrintMode(TRUE);
#endif // _PRINT_SUPPORT_

	// Observable from JavaScript
	//OK to ignore the OP_STATUS return value, empty strings
	OpStatus::Ignore(window->SetMessage(UNI_L("")));
	OpStatus::Ignore(window->SetDefaultMessage(UNI_L("")));

	// 1st bool -> parse ahead outstanding data.
	// 2nd bool -> signal to remove progress bar.
	// 3rd bool -> put doc in aborted state, also kills plugin streams.
	StopLoading(FALSE, FALSE, TRUE);

	if (FramesDocument* doc = GetCurrentDoc())
	{
		/* Clear the flag since it stops being true.  Alternatively we could
		   just not reload since we don't have the content the current document
		   was actually parsed from (and won't get it by reloading the URL,) but
		   that is probably less useful. */
		current_doc_elm->SetScriptGeneratedDocument(FALSE);

		BOOL remove_frames = doc->IsFrameDoc() && !g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::FramesEnabled);
		SetUserAutoReload(is_user_auto_reload);
		SetReload(TRUE);
		SetReloadFlags(TRUE, conditionally_request_document, TRUE, conditionally_request_inlines);
		DocumentReferrer ref_url = doc->GetRefURL();
		URL doc_url = current_doc_elm->GetUrl();
		OpenURLOptions options;
		options.user_initiated = TRUE;
		options.create_doc_now = remove_frames; // Why?
		options.entered_by_user = entered_by_user;
#ifdef WEB_TURBO_MODE
		if (window->GetTurboMode() != !!doc_url.GetAttribute(URL::KUsesTurbo))
		{
			URL_CONTEXT_ID ctx = window->GetTurboMode() ? g_windowManager->GetTurboModeContextId() : 0;
			const OpStringC8 url_str = doc_url.GetAttribute(URL::KName_With_Fragment_Username_Password_NOT_FOR_UI, URL::KNoRedirect);
			doc_url = g_url_api->GetURL(url_str.CStr(),ctx);
			SetUseHistoryNumber(CurrentDocListElm()->Number());
		}
#endif // WEB_TURBO_MODE
		OpenURL(doc_url, DocumentReferrer(ref_url), TRUE /*check_if_expired */, TRUE /*reload*/, options);
	}
}

BOOL DocumentManager::ShouldSendReferrer()
{
	return current_doc_elm ? current_doc_elm->ShouldSendReferrer() : send_to_server;
}

void DocumentManager::CheckHistory(int decrement, int& minhist, int& maxhist)
{
	DocListElm* delm = FirstDocListElm();
	while (delm)
	{
		int new_dec = decrement;
		if (delm->Number() > maxhist + 1)
			new_dec += delm->Number() - maxhist - 1;

		if (new_dec)
			delm->SetNumber(delm->Number() - new_dec);

		if (delm->Number() < minhist)
			minhist = delm->Number();

		if (delm->Number() > maxhist)
			maxhist = delm->Number();

		delm->Doc()->CheckHistory(new_dec, minhist, maxhist);
		delm = delm->Suc();
	}
}

#ifdef _PRINT_SUPPORT_

OP_STATUS DocumentManager::CreatePrintDoc(BOOL preview)
{
	OP_NEW_DBG("DocumentManager::CreatePrintDoc", "async_print");
	FramesDocument* frames_doc = GetCurrentDoc();
	FramesDocument* new_doc = NULL;

	if (frames_doc)
	{
		PrinterInfo* printer_info = window->GetPrinterInfo(preview);
		PrintDevice* pd = printer_info->GetPrintDevice();
		DM_PrintType frames_print_type = window->GetFramesPrintType();

		if (frames_doc->IsFrameDoc())
		{
			if (frames_print_type == PRINT_ACTIVE_FRAME)
			{
				DocumentManager* active_doc_man = frames_doc->GetActiveDocManager();

				if (active_doc_man && active_doc_man->GetCurrentDoc())
					frames_doc = active_doc_man->GetCurrentDoc();
				else
				{
					frames_print_type = PRINT_ALL_FRAMES;

					if (window->SetFramesPrintType(PRINT_ALL_FRAMES, FALSE) == OpStatus::ERR_NO_MEMORY)
						return OpStatus::ERR_NO_MEMORY;
				}
			}
		}

		if (!preview && print_preview_doc)
		{
			print_doc = print_preview_doc;
			GetWindow()->SetPrinting(FALSE);
			GetWindow()->GetMessageHandler()->PostMessage(DOC_PRINT_FORMATTED, 0, 0);
		}
		else
		{
			new_doc = OP_NEW(FramesDocument, (this, frames_doc->GetURL(), frames_doc->GetMutableOrigin(), -1));

			if (preview)
				print_preview_doc = new_doc;
			else
				print_doc = new_doc;

			if (new_doc)
			{
				OP_DBG(("SetFormattingPrintDoc(TRUE)"));
				GetWindow()->SetFormattingPrintDoc(TRUE);
				if (new_doc->CreatePrintLayout(pd, frames_doc,
					pd->GetRenderingViewWidth(), pd->GetRenderingViewHeight(),
					frames_print_type != PRINT_AS_SCREEN,
					preview) == OpStatus::ERR_NO_MEMORY)
				{
					GetWindow()->SetFormattingPrintDoc(FALSE);
					OP_DELETE(new_doc);

					if (preview)
						print_preview_doc = NULL;
					else
						print_doc = NULL;

					return OpStatus::ERR_NO_MEMORY;
				}

				if (frames_doc->IsCurrentDoc())
					new_doc->SetAsCurrentDoc(TRUE);
			}
			else
				return OpStatus::ERR_NO_MEMORY;
		}
	}

	return OpStatus::OK;
}

OP_STATUS DocumentManager::UpdatePrintPreview()
{
	if (print_preview_vd)
	{
		FramesDocument* frames_doc = GetCurrentDoc();

		if (frames_doc)
			frames_doc->DeletePrintCopy();

		OP_DELETE(print_preview_doc);
		print_preview_doc = NULL;

		if (CreatePrintDoc(TRUE) == OpStatus::ERR_NO_MEMORY)
			return OpStatus::ERR_NO_MEMORY;
		else
			print_preview_vd->UpdateAll();
	}

	return OpStatus::OK;
}

OP_BOOLEAN DocumentManager::SetPrintMode(BOOL on, FramesDocElm* copy_fde, BOOL preview)
{
	if (on)
	{
		FramesDocument* doc = (copy_fde) ? copy_fde->GetCurrentDoc() : GetCurrentDoc();

		if (current_doc_elm)
			StoreViewport(current_doc_elm);

		if (doc && (doc->IsLoaded() /*|| GetWindow()->GetType() == WIN_TYPE_IM_VIEW*/))
		{
			PrinterInfo* printer_info = window->GetPrinterInfo(preview);
			PrintDevice* pd = printer_info->GetPrintDevice();
			DM_PrintType frames_print_type = window->GetFramesPrintType();
			VisualDevice *vd = NULL;
			FramesDocument *new_doc = NULL;

			if (copy_fde)
			{
				DocumentManager* top_doc_man = GetWindow()->DocManager();

				if (preview)
				{
					vd = copy_fde->GetVisualDevice();
					if (vd->CreatePrintPreviewVD(print_preview_vd, pd) == OpStatus::ERR_NO_MEMORY)
					{
						//print_preview_vd = NULL;
						return OpStatus::ERR_NO_MEMORY;
					}
					else
					{
						print_preview_vd->SetDocumentManager(this);

						// hide original frame
						vd->Hide();
						vd = print_preview_vd;
						print_preview_vd->GetView()->SetVisibility(TRUE);
						print_preview_vd->SetFocus(FOCUS_REASON_OTHER);
					}
				}
				else
					vd = top_doc_man->GetPrintPreviewVD();

				if (vd)
				{
					if (preview)
						OP_ASSERT(print_preview_vd == vd);
					else
					{
						print_vd = vd;
						OP_ASSERT(print_vd == top_doc_man->GetPrintPreviewVD());
					}

					new_doc = OP_NEW(FramesDocument, (this, doc->GetURL(), doc->GetMutableOrigin(), copy_fde->GetSubWinId()));

					if (!new_doc)
					{
						if (preview)
						{
							OP_DELETE(print_preview_vd);
							print_preview_vd = NULL;
						}
						return OpStatus::ERR_NO_MEMORY;
					}

					if (preview)
						print_preview_doc = new_doc;
					else
						print_doc = new_doc;

					if (new_doc->CreatePrintLayout(pd, doc, frame->GetWidth(), frame->GetHeight(), frames_print_type != PRINT_AS_SCREEN && !frame->IsInlineFrame(), preview) == OpStatus::ERR_NO_MEMORY)
					{
						OP_DELETE(new_doc);
						if (preview)
						{
							OP_ASSERT(print_preview_doc == new_doc);
							print_preview_doc = NULL;
							OP_DELETE(print_preview_vd);
							print_preview_vd = NULL;
						}
						else
						{
							OP_ASSERT(print_doc == new_doc);
							print_doc = NULL;
							// The VisualDevice is the top-most
							// VisualDevice. I assume someone else
							// takes care of it
							print_vd = NULL;
						}
						return OpStatus::ERR_NO_MEMORY;
					}

					if (doc->IsCurrentDoc())
						new_doc->SetAsCurrentDoc(TRUE);
				}
			}
			else
			{
				BOOL set_focus_on_preview_doc = FALSE;
				if (preview) //window->GetPreviewMode())
				{
					if (vis_dev->CreatePrintPreviewVD(print_preview_vd, pd) == OpStatus::ERR_NO_MEMORY)
					{
						//print_preview_vd = NULL;
						return OpStatus::ERR_NO_MEMORY;
					}
					else
					{
						print_preview_vd->SetDocumentManager(this);

						vd = print_preview_vd;
						vis_dev->Hide();
						print_preview_vd->GetView()->SetVisibility(TRUE);
						// Can't set focus until we have a document below
						set_focus_on_preview_doc = TRUE;
					}
				}
				else
				{
					if (!pd)
						return OpBoolean::IS_FALSE; // No PrintDevice -> No printing
					vd = pd; // print_preview_vd = pd;
					print_vd = vd;
				}

				OP_ASSERT(vd);
				OP_ASSERT(!preview || print_preview_vd);
				OP_ASSERT(preview || print_vd);

				// This will also hide all children documents (setting visible to FALSE for their VisualDevices)
				if (CreatePrintDoc(preview) == OpStatus::ERR_NO_MEMORY)
				{
					OP_DELETE(print_preview_vd);
					print_preview_vd = NULL;
					print_vd = NULL;
					return OpStatus::ERR_NO_MEMORY; //rg memfix
				}
				if (set_focus_on_preview_doc)
				{
					print_preview_vd->SetFocus(FOCUS_REASON_OTHER);
				}
			}
			return OpBoolean::IS_TRUE;
		}
		else
			return OpBoolean::IS_FALSE;
	}
	else
	{
		FramesDocument* frames_doc = GetCurrentDoc();

		if (frames_doc && (!print_preview_doc || !print_doc))
			frames_doc->DeletePrintCopy();

		if (preview)
		{
			if (print_doc != print_preview_doc)
				OP_DELETE(print_preview_doc);

			print_preview_doc = NULL;
		}
		else
		{
			if (print_doc != print_preview_doc)
				OP_DELETE(print_doc);

			print_doc = NULL;
		}

		//if (!GetFrame())
		{
			BOOL restore_real_vd = FALSE;
			if (preview && print_preview_vd)
			{
				OP_DELETE(print_preview_vd);
				print_preview_vd = NULL;
				restore_real_vd = !is_clearing;
			}
			else if (!preview && print_vd)
			{
				print_vd = NULL;
				restore_real_vd = !is_clearing;
			}

			if (restore_real_vd)
			{
				vis_dev->Show(NULL);

				// Need to make all the hidden children frames visible as well.
				// Maybe this could be done in a better way.
				DocumentTreeIterator iter(this);
				iter.SetIncludeEmpty();
				while (iter.Next())
				{
					DocumentManager* child_docman = iter.GetDocumentManager();
					CoreView* parent_view = NULL;
					if (child_docman->parent_doc)
					{
						DocumentManager* its_parent = child_docman->parent_doc->GetDocManager();
						parent_view = its_parent->GetVisualDevice()->GetView();
					}
					child_docman->vis_dev->Show(parent_view);
				}
				vis_dev->SetFocus(FOCUS_REASON_OTHER);
			}
		}

		if (!is_clearing)
			if (frames_doc)
			{
				if (current_doc_elm->GetLastScale() != GetWindow()->GetScale())
					SetScale(GetWindow()->GetScale());
				else
				{
					frames_doc->RecalculateScrollbars();
					frames_doc->RecalculateLayoutViewSize(TRUE);
				}
			}

		if (pending_refresh_id)
		{
			Refresh(pending_refresh_id);
			pending_refresh_id = 0;
		}

		return OpBoolean::IS_TRUE;
	}
}
#endif // _PRINT_SUPPORT_

void DocumentManager::SetScale(int scale)
{
	if (!vis_dev)
		return;

#ifdef _PRINT_SUPPORT_
	if (print_vd || print_preview_vd)
	{
		PrinterInfo* printer_info = GetWindow()->GetPrinterInfo(!print_vd);

		if (printer_info)
		{
			scale = (scale * g_pcprint->GetIntegerPref(PrefsCollectionPrint::PrinterScale)) / 100;
			print_preview_vd->SetScale(scale);
		}
	}
#endif // _PRINT_SUPPORT_

	int old_scale = vis_dev->GetScale();
	vis_dev->SetScale(scale);

#ifdef DOCHAND_HISTORY_SAVE_ZOOM_LEVEL
	if (CurrentDocListElm())
		CurrentDocListElm()->SetLastScale(scale);
#endif // DOCHAND_HISTORY_SAVE_ZOOM_LEVEL

	if (FramesDocument* frames_doc = GetCurrentDoc())
	{
		// scale all frames and iframes
		if (frames_doc->GetSubWinId() == -1)
		{
			DocumentTreeIterator it(this);
			it.SetIncludeEmpty();
			while (it.Next())
				it.GetDocumentManager()->SetScale(scale);
		}

		if (scale != old_scale)
		{
			if (!GetWindow()->GetTrueZoom())
			{
				// Mark current selected element dirty
				TextSelection* text_selection = frames_doc->GetTextSelection();

				if (text_selection)
				{
					HTML_Element* element = text_selection->GetStartElement();
					if (element)
					{
						element->MarkDirty(frames_doc);
						text_selection->MarkDirty(element);

						element = text_selection->GetEndElement();
						if (element)
							element->MarkDirty(frames_doc);
						text_selection->MarkDirty(element);
					}
				}

				if (HTML_Element* root = frames_doc->GetDocRoot())
					root->RemoveCachedTextInfo(frames_doc);
			}

			if (frames_doc->GetSubWinId() == -1)
			{
				frames_doc->RecalculateLayoutViewSize(TRUE);
				frames_doc->RecalculateScrollbars();

				/* Update frames' geometry. Typically necessary for truezoom (and
				   in other cases), since that won't affect layout viewport size
				   (and thus there will be no reformatting) */

				for (DocumentTreeIterator it(this); it.Next();)
					if (FramesDocElm* fde = it.GetFramesDocElm())
						fde->UpdateGeometry();
			}
		}
	}
}

#if defined SAVE_DOCUMENT_AS_TEXT_SUPPORT
OP_STATUS DocumentManager::SaveCurrentDocAsText(UnicodeOutputStream* stream, const uni_char* fname, const char *force_encoding)	// <##> can this return an error-code (or boolean) that the save was successful or not ? <JB>
{
# ifdef _PRINT_SUPPORT_
	if (print_preview_vd)
		return OpStatus::ERR;
# endif // _PRINT_SUPPORT_

	if (FramesDocument* doc = GetCurrentDoc())
		return doc->SaveCurrentDocAsText(stream, fname, force_encoding);
	else
		return OpStatus::ERR;
}
#endif // SAVE_DOCUMENT_AS_TEXT_SUPPORT

void DocumentManager::Clear()
{
	is_clearing = TRUE;

#ifdef _PRINT_SUPPORT_
	if (print_preview_doc)
		OpStatus::Ignore(SetPrintMode(FALSE, NULL, TRUE));
	if (print_doc)
		OpStatus::Ignore(SetPrintMode(FALSE, NULL, FALSE));
#endif // _PRINT_SUPPORT_

	StopLoading(FALSE, !GetFrame(), TRUE);

	if (current_doc_elm && current_doc_elm->Doc())
	{
		OP_STATUS status = current_doc_elm->Doc()->Undisplay(TRUE);
		if (OpStatus::IsMemoryError(status))
			RaiseCondition(OpStatus::ERR_NO_MEMORY);
	}

	current_doc_elm = NULL; // Important: GetCurrentDoc() may be called from Document destructor !!!

	for (DocListElm* delm = LastDocListElm(); delm; delm = LastDocListElm())
	{
		if (delm->Number() >= GetWindow()->GetHistoryMax() || delm->Number() <= GetWindow()->GetHistoryMin())
			GetWindow()->SetCheckHistory(TRUE);

		delm->Out();
		OP_DELETE(delm);
	}

	OP_DELETEA(current_application);
	current_application = NULL;

	is_clearing = FALSE;

	OP_DELETE(m_waiting_for_online_url);
	m_waiting_for_online_url = NULL;

#ifdef DOM_LOAD_TV_APP
	SetWhitelist(NULL);
#endif
}

void DocumentManager::ClearHistory()
{
	Clear();
	history_len = 0;
	if (vis_dev)
	{
		vis_dev->SetBgColor(g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_DOCUMENT_BACKGROUND));
		vis_dev->UpdateAll();
	}
}

#ifdef _PRINT_SUPPORT_

OP_DOC_STATUS DocumentManager::PrintPage(PrintDevice* pd, int page_num, BOOL print_selected_only, BOOL only_probe)
{
	if (FramesDocument *prnt_doc = GetPrintDoc())
	{
		if (only_probe)
			// Don't print anything - just check if it is really possible to print the specified page
			return page_num <= prnt_doc->CountPages() ? (OP_DOC_STATUS) OpStatus::OK : DocStatus::DOC_PAGE_OUT_OF_RANGE;
		else
			return prnt_doc->PrintPage(pd, page_num, print_selected_only);
	}
	else
		return DocStatus::DOC_CANNOT_PRINT;
}

#endif // _PRINT_SUPPORT_

void DocumentManager::RaiseCondition(OP_STATUS status)
{
	if (OpStatus::IsMemoryError(status))
	{
		if (window)
			window->RaiseCondition(status);
		else
			g_memory_manager->RaiseCondition(status);
	}
}

#ifdef _WML_SUPPORT_
//
// must be called at least once before parsing any
// WML documents
//**********************************************************************
OP_STATUS DocumentManager::WMLInit()
{
	if (!wml_context)
	{
		wml_context = OP_NEW(WML_Context, (this));
		if (!wml_context)
			return OpStatus::ERR_NO_MEMORY;

		if (OpStatus::IsMemoryError(wml_context->Init())
			|| OpStatus::IsMemoryError(wml_context->PreParse()))
		{
			OP_DELETE(wml_context);
			wml_context = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}

		wml_context->IncRef();
		current_doc_elm->SetWmlContext(wml_context); // store it in the document history
	}

	return OpStatus::OK;
}

// Sets a new context deleteing the old one.
OP_STATUS DocumentManager::WMLSetContext(WML_Context *new_context)
{
	if (wml_context)
		wml_context->DecRef();

	wml_context = OP_NEW(WML_Context, (this));
	if (!wml_context
		|| OpStatus::IsMemoryError(wml_context->Init())
		|| OpStatus::IsMemoryError(wml_context->Copy(new_context, this)))
	{
		OP_DELETE(wml_context);
		wml_context = NULL;

		return OpStatus::ERR_NO_MEMORY;
	}

	wml_context->IncRef();
	if (current_doc_elm)
		current_doc_elm->SetWmlContext(wml_context);

	return OpStatus::OK;
}

// Delete the WML context
void DocumentManager::WMLDeleteContext()
{
	if (wml_context)
		wml_context->DecRef();

	wml_context = NULL;
}

// Clean up the document history. Removes all cards beyond the first in the
// last WML session.
void DocumentManager::WMLDeWmlifyHistory(BOOL delete_all/*=FALSE*/)
{
	int first = delete_all ? -2 : wml_context->GetFirstInSession();

	// -1 indicates not set, -2 indicates delete all
	if (first == -1)
		return;

	DocListElm* tmp_elm = (DocListElm *) doc_list.Last();
	while (tmp_elm && tmp_elm->GetWmlContext() && tmp_elm->Number() > first)
	{
		DocListElm* pred_tmp_elm = tmp_elm->Pred();
		if (tmp_elm != current_doc_elm)
			RemoveElementFromHistory(tmp_elm, FALSE);

		tmp_elm = pred_tmp_elm;
	}
}
#endif // _WML_SUPPORT_

void DocumentManager::SetWaitingForOnlineUrl(URL &url)
{
	OP_DELETE(m_waiting_for_online_url);
	m_waiting_for_online_url = OP_NEW(URL, (url));
}

OP_STATUS DocumentManager::OnlineModeChanged()
{
	if (m_waiting_for_online_url)
	{
		OpenURL(*m_waiting_for_online_url, referrer_url, TRUE/*check_if_expired*/, FALSE /*reload*/);
		OP_DELETE(m_waiting_for_online_url);
		m_waiting_for_online_url = NULL;
	}

	if (FramesDocument *frm_doc = GetCurrentDoc())
		return frm_doc->OnlineModeChanged();
	else
		return OpStatus::OK;
}

BOOL DocumentManager::IsTrustedExternalURLProtocol(const OpStringC &URLName)
{
	BOOL isTrusted = FALSE;

	if (URLName.IsEmpty())
		return isTrusted;

	int colonindex = URLName.FindFirstOf(':');

	if (colonindex != KNotFound)
	{
		int number_trusted_protocols = g_pcdoc->GetNumberOfTrustedProtocols();

		if (number_trusted_protocols)
		{
			OpString protocol;

			if (OpStatus::IsError(protocol.Set(URLName.CStr(), colonindex)))
				return FALSE;

			TrustedProtocolData data;
			int index = g_pcdoc->GetTrustedProtocolInfo(protocol, data);
			if (index != -1)
				// Do not treat UseInternalApplication and UseWebService viewer modes as external. Check if the application's name is not empty too.
				isTrusted = (data.viewer_mode == UseCustomApplication || data.viewer_mode == UseDefaultApplication) && !data.filename.IsEmpty();
		}
	}

	//	Done
	return isTrusted;
}

#if defined WEB_HANDLERS_SUPPORT || defined QUICK
OP_BOOLEAN DocumentManager::AttemptExecuteURLsRecognizedByOpera(URL& url, DocumentReferrer referrer, BOOL user_initiated, BOOL user_started)
{
	enum ExecuteAction
	{
		DoNotExecute,
		ExecuteInOpera,
		ExecuteSystemOrExternalApp,
#ifdef WEB_HANDLERS_SUPPORT
		LoadWebApp
#endif // WEB_HANDLERS_SUPPORT
	} execute_action = DoNotExecute;

	OpString external_application;

	// Check the list of trusted protocols.
	OpString protocol;
	if (execute_action == DoNotExecute)
	{
		if (url.IsEmpty())
			return OpBoolean::IS_FALSE;

		RETURN_IF_MEMORY_ERROR(protocol.Set(url.GetAttribute(URL::KProtocolName)));

		TrustedProtocolData td;
		OP_MEMORY_VAR int index = g_pcdoc->GetTrustedProtocolInfo(protocol, td);
		if (index >= 0)
		{
			URLType url_type = url.Type();
			switch (td.viewer_mode)
			{
				case UseCustomApplication:
				{
					if (url_type == URL_MAILTO)
					{
						execute_action = DoNotExecute;
					}
					else
					{
						execute_action = ExecuteSystemOrExternalApp;
					}
					RETURN_IF_MEMORY_ERROR(external_application.Set(td.filename));
					break;
				}

				case UseInternalApplication:
				{
					execute_action = DoNotExecute;
					break;
				}

				case UseDefaultApplication:
				{
					if (url_type == URL_MAILTO)
					{
						execute_action = DoNotExecute;
					}
					else
					{
						execute_action = ExecuteSystemOrExternalApp;
					}
#ifdef OPSYSTEMINFO_GETPROTHANDLER
					OpString uri_string;
					RETURN_IF_MEMORY_ERROR(g_op_system_info->GetProtocolHandler(uri_string, protocol, external_application));
#endif // OPSYSTEMINFO_GETPROTHANDLER
					break;
				}

#ifdef WEB_HANDLERS_SUPPORT
				case UseWebService:
				{
					if (td.web_handler.HasContent() && url.GetAttribute(URL::KHTTP_Method) == HTTP_METHOD_GET)
					{
						TempBuffer8 parameter_url8;
						RETURN_IF_ERROR(parameter_url8.AppendURLEncoded(url.GetAttribute(URL::KName_With_Fragment_Escaped)));
						OpString parameter_url16;
						RETURN_IF_ERROR(parameter_url16.SetFromUTF8(parameter_url8.GetStorage()));
						OpString target_url_str;
						RETURN_IF_ERROR(target_url_str.Set(td.web_handler.CStr()));
						RETURN_IF_ERROR(target_url_str.ReplaceAll(UNI_L("%s"), parameter_url16.CStr(), 1));
						URL target_url = g_url_api->GetURL(url, target_url_str.CStr());

						BOOL allowed;
						OpSecurityContext source(url);
						OpSecurityContext target(target_url);
						target.AddText(protocol.CStr());
						OP_STATUS rc = g_secman_instance->CheckSecurity(OpSecurityManager::WEB_HANDLER_REGISTRATION, source, target, allowed);
						if (OpStatus::IsError(rc) || !allowed)
						{
							execute_action = DoNotExecute;
							break;
						}

						OpString protocol2;
						RETURN_IF_ERROR(protocol2.Set(target_url.GetAttribute(URL::KProtocolName, TRUE)));

						TrustedProtocolData data2;
						int index = g_pcdoc->GetTrustedProtocolInfo(protocol2, data2);

						// Do not allow to use a handler for protocol the handler itself or any other handler uses.
						if (protocol2.CompareI(url.GetAttribute(URL::KProtocolName, TRUE)) == 0)
						{
							execute_action = DoNotExecute;
							break;
						}
						else if (index >= 0) // We have a handler for protocol this handler would like to use -> possible loop.
						{
							if (data2.viewer_mode == UseWebService)
							{
								execute_action = DoNotExecute;
								break;
							}
						}

						if (!td.user_defined)
						{
							OpString8 protocol8;
							RETURN_IF_ERROR(protocol8.SetUTF8FromUTF16(td.protocol.CStr()));
							WebHandlerCallback* cb = OP_NEW(WebHandlerCallback, (this, url, referrer, target_url, user_initiated, user_started, td.description.CStr(), TRUE, FALSE));
							RETURN_OOM_IF_NULL(cb);
							RETURN_IF_ERROR(cb->Construct(protocol8));
							GetWindow()->GetWindowCommander()->GetDocumentListener()->OnWebHandler(GetWindow()->GetWindowCommander(), cb);
						}
						else // Open without asking
						{
							SetUrlLoadOnCommand(target_url, DocumentReferrer(current_url), TRUE, user_initiated);
							GetMessageHandler()->PostMessage(MSG_URL_LOAD_NOW, target_url.Id(), user_initiated);
						}

						execute_action = LoadWebApp;
					}
					else
					{
						execute_action = DoNotExecute;
					}
				}
#endif // WEB_HANDLERS_SUPPORT
			}
		}
	}

#ifdef M2_SUPPORT
	// Check if we should execute this in M2.
	if (execute_action == DoNotExecute)
	{
		switch (url.Type())
		{
			case URL_NEWS :
			case URL_SNEWS :
#ifdef IRC_SUPPORT
			case URL_IRC :
#endif
			{
				if( g_m2_engine )
				{
					execute_action = ExecuteInOpera;
				}
				break;
			}
		}
	}
#endif

	if (execute_action == ExecuteSystemOrExternalApp)
	{
		if (external_application.HasContent())
		{
#ifdef QUICK
			if (g_application)
				g_application->ExecuteProgram(external_application.CStr(), url.UniName(PASSWORD_NOTHING), FALSE, protocol.CStr());
#else // QUICK
#ifdef EXTERNAL_APPLICATIONS_SUPPORT
			g_op_system_info->ExecuteApplication(external_application,
												 url.GetAttribute(URL::KUniName_Username_Password_Escaped_NOT_FOR_UI).CStr());
#endif // EXTERNAL_APPLICATIONS_SUPPORT
#endif // QUICK
			return OpBoolean::IS_TRUE;
		}
#ifdef QUICK
		else if (g_application)
		{
			g_application->ExecuteProgram(url.UniName(PASSWORD_NOTHING), 0);
			return OpBoolean::IS_TRUE;
		}
#endif // QUICK
	}
#ifdef M2_SUPPORT
	else if (execute_action == ExecuteInOpera)
	{
		// g_mailer_glue will be 0 if Opera runs with M2 disabled.
		if( g_m2_engine )
		{
			RETURN_IF_MEMORY_ERROR(g_m2_engine->MailCommand(url));
			return OpBoolean::IS_TRUE;
		}
	}
#endif // M2_SUPPORT
#ifdef WEB_HANDLERS_SUPPORT
	else if (execute_action == LoadWebApp)
		return OpBoolean::IS_TRUE;
#endif // WEB_HANDLERS_SUPPORT

	return OpBoolean::IS_FALSE;
}
#endif // QUICK
OP_BOOLEAN
DocumentManager::JumpToRelativePosition(const URL &url, BOOL reuse_history_entry)
{
	FramesDocument *doc = GetCurrentDoc();
	OpString new_decoded_fragment;
	OpString new_original_fragment;
	RETURN_IF_ERROR(GetURLFragment(url, new_decoded_fragment, new_original_fragment));
	uni_char* new_fragment_id = new_decoded_fragment.CStr();

	// a relative anchor with just # should never reload page
	if ((!new_fragment_id || *new_fragment_id)
		&& (load_stat != NOT_LOADING
			&& (load_stat != DOC_CREATED || doc->IsLoaded())
			|| !url_load_on_command.IsEmpty() || !doc
			|| !IsRelativeJump(url)))
		// Not possible, need to load the new URL the normal way.
		return OpBoolean::IS_FALSE;

	OpString currently_loading_fragment;
	RETURN_IF_ERROR(currently_loading_fragment.Set(current_doc_elm->GetUrl().UniRelName()));

	if (!currently_loading_fragment.CStr() || !new_original_fragment.CStr() || currently_loading_fragment != new_original_fragment)
	{
		if (load_stat == NOT_LOADING && !reuse_history_entry)
		{
			StoreViewport(current_doc_elm);

			DocListElm *dle = OP_NEW(DocListElm, (url, doc, FALSE, window->SetNewHistoryNumber(), GetNextDocListElmId()));

			if (!dle)
				return OpStatus::ERR_NO_MEMORY;

#ifdef _WML_SUPPORT_
			// relative jumps is inside a deck and should use the same WML_Context
			if (current_doc_elm->GetWmlContext())
				dle->SetWmlContext(current_doc_elm->GetWmlContext());
#endif // _WML_SUPPORT_

			InsertHistoryElement(dle);

			dle->SetReferrerUrl(current_doc_elm->GetReferrerUrl(), ShouldSendReferrer());

			current_doc_elm = dle;
		}
		else
			current_doc_elm->SetUrl(url);

		if (!window->IsLoading())
		{
			if (SignalOnNewPage(VIEWPORT_CHANGE_REASON_JUMP_TO_RELATIVE_POS))
				waiting_for_document_ready = TRUE;
		}

		current_url = url;
		doc->SetUrl(url);

		WindowCommander *wc = GetWindow()->GetWindowCommander();

		if (!GetParentDoc())
		{
			uni_char *tempname = Window::ComposeLinkInformation(url.GetAttribute(URL::KUniName_Username_Password_Hidden).CStr(), url.GetAttribute(URL::KUniFragment_Name).CStr());

			if (tempname)
			{
				wc->GetLoadingListener()->OnUrlChanged(wc, tempname);
				OP_DELETEA(tempname);
			}
		}

		if (!window->IsLoading())
		{
			// These calls are only here to trick the (quick) UI to update the
			// state of the back and forward buttons.  If you know a better way
			// to do that, please change this code.

			wc->GetLoadingListener()->OnStartLoading(wc);
			wc->GetLoadingListener()->OnLoadingFinished(wc, OpLoadingListener::LOADING_SUCCESS);

			if (!window->GetPrivacyMode())
				current_url.Access(TRUE);
		}
	}

	if (new_fragment_id)
		// new_fragment_id can be NULL if the URL code got an oom.
		// SetRelativePos can return ERR if FramesDocument::doc is not set
		// and that would abort this function. Just ignore that.
		RETURN_IF_MEMORY_ERROR(doc->SetRelativePos(new_fragment_id, new_original_fragment.CStr()));

	// must be done after SetRelativePos() because of WML stuff
	if (!window->IsLoading())
		g_windowManager->UpdateVisitedLinks(current_url, NULL);

	CheckOnNewPageReady();

	// Will do nothing if hashes are the same.
	RETURN_IF_ERROR(doc->HandleHashChangeEvent(currently_loading_fragment.CStr(), new_original_fragment.CStr()));

	return OpBoolean::IS_TRUE;
}

BOOL DocumentManager::IsRecursiveDocumentOpening(const URL& url, BOOL from_html_attribute)
{
	// check if furl equals to any ancestor of this frame
	FramesDocument* ancestor_doc = parent_doc;

	// We don't allow deeper nesting than 20 frames. Anything that big is probably a DoS attack
	// We also do not allow this url to appear more than twice as parent. We need to
	// allow one level because pages sometimes behave differently depending on
	// referer and then we allow one loop extra to be sure not to block any real sites.
	int level_count = 0;
	int loop_count = 0;
	while (ancestor_doc)
	{
		level_count++;
		if (ancestor_doc->GetURL() == url)
			// If the url was set from an html attribute do not allow
			// recursion.
			if (from_html_attribute)
				return TRUE;
			else
				loop_count++;
		ancestor_doc = ancestor_doc->GetParentDoc();
	}

	return loop_count > 2 || level_count > 20;
}

void
DocumentManager::StoreRequestThreadInfo(ES_Thread *thread)
{
	if (thread)
		request_thread_info = thread->GetOriginInfo();
	else
		ResetRequestThreadInfo();
}

void
DocumentManager::ResetRequestThreadInfo()
{
	request_thread_info.type = ES_THREAD_EMPTY;
}

BOOL
DocumentManager::IgnoreURLRequest(URL url, ES_Thread *thread)
{
	if (DOM_Environment::IsCalledFromUnrequestedScript(thread))
		return IsSpecialURL(url);
	else
		return FALSE;
}

DocumentManager::SecCheckRes DocumentManager::InitiateSecurityCheck(URL url, const DocumentReferrer& source)
{
	SecCheckRes res = SEC_CHECK_DENIED;
	security_check_callback->Reset();
	security_check_callback->SetURLs(url, source);
	URL source_url = source.url;
	OP_STATUS status = g_secman_instance->CheckSecurity(OpSecurityManager::DOCMAN_URL_LOAD, OpSecurityContext(source_url, this), OpSecurityContext(url), security_check_callback);
	if (OpStatus::IsError(status))
		res = SEC_CHECK_DENIED;
	else if (!security_check_callback->m_done)
		return SEC_CHECK_STARTED;
	else if (security_check_callback->m_allowed)
		res = SEC_CHECK_ALLOWED;

	if (res == SEC_CHECK_DENIED)
		HandleURLAccessDenied(url, source.url);
	else
		security_check_callback->Reset();

	return res;
}

void DocumentManager::HandleURLAccessDenied(URL url, URL source)
{
	StopLoading(FALSE, TRUE, FALSE);
	queue_messages = FALSE;
	queued_messages.Clear();
	security_check_callback->Reset();

#ifdef WEBSERVER_SUPPORT
	if (url.GetAttribute(URL::KIsUniteServiceAdminURL))
		RAISE_IF_MEMORY_ERROR(GenerateAndShowUniteWarning(url, source));
#endif // WEBSERVER_SUPPORT
#ifdef OPERA_CONSOLE
	if (g_console->IsLogging())
	{
		OpConsoleEngine::Message msg(OpConsoleEngine::Network, OpConsoleEngine::Information);
		msg.window = GetWindow()->Id();

		// Eventually it would be nice to unescape the url to make it more readable, but then we need to
		// get the decoding to do it correctly with respect to charset encodings. See bug 250545
		OP_STATUS check = url.GetAttribute(URL::KUniName_With_Fragment_Escaped, msg.url);
		if (OpStatus::IsSuccess(check))
		{
			check = g_languageManager->GetString(Str::S_SECURITY_LOADING_BLOCKED, msg.message);
			if (OpStatus::IsSuccess(check))
				TRAP(check, g_console->PostMessageL(&msg));
		}
		if (OpStatus::IsMemoryError(check))
			RaiseCondition(OpStatus::ERR_NO_MEMORY);
	}
#endif // OPERA_CONSOLE

	if (!current_doc_elm && frame)
		FramesDocument::CheckOnLoad(NULL, frame);
}


/* static */ BOOL
DocumentManager::IsSpecialURL(URL target)
{
	switch (target.Type())
	{
	case URL_MAILTO:
#ifdef IRC_SUPPORT
	case URL_IRC:
#endif
		return TRUE;

	case URL_JAVASCRIPT:
		return FALSE;

	default:
		return !g_url_api->LoadAndDisplayPermitted(target);
	}
}

#ifdef SCOPE_PROFILER
OP_STATUS
DocumentManager::StartProfiling(OpProfilingSession *session)
{
	if (IsProfiling() || !session)
		return OpStatus::ERR;

	m_timeline = session->AddTimeline(this);

	return m_timeline ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

void
DocumentManager::StopProfiling()
{
	m_timeline = NULL;
}

void
DocumentManager::OnAddChild(DocumentManager *child)
{
	OpProfilingSession *session = window->GetProfilingSession();

	// If this Window is currently profiling, also start profiling on the
	// DocumentManager that was just added to the tree.
	if (session)
		RAISE_IF_MEMORY_ERROR(child->StartProfiling(session));
}

void
DocumentManager::OnRemoveChild(DocumentManager *child)
{
	child->StopProfiling();
}

#endif // SCOPE_PROFILER

DocumentTreeIterator::DocumentTreeIterator()
	: start(NULL),
	  current(NULL),
	  include_this(FALSE),
	  include_iframes(TRUE),
	  include_empty(FALSE)
{
}

DocumentTreeIterator::DocumentTreeIterator(Window *window)
	: start(window->DocManager()),
	  current(window->DocManager()),
	  include_this(FALSE),
	  include_iframes(TRUE),
	  include_empty(FALSE)
{
}

DocumentTreeIterator::DocumentTreeIterator(DocumentManager *docman)
	: start(docman),
	  current(docman),
	  include_this(FALSE),
	  include_iframes(TRUE),
	  include_empty(FALSE)
{
}

DocumentTreeIterator::DocumentTreeIterator(FramesDocElm *frame)
	: start(frame->GetDocManager()),
	  current(frame->GetDocManager()),
	  include_this(FALSE),
	  include_iframes(TRUE),
	  include_empty(FALSE)
{
}

DocumentTreeIterator::DocumentTreeIterator(FramesDocument *document)
	: start(document->GetDocManager()),
	  current(document->GetDocManager()),
	  include_this(FALSE),
	  include_iframes(TRUE),
	  include_empty(FALSE)
{
}

void DocumentTreeIterator::SetIncludeThis()
{
	include_this = TRUE;
}

void DocumentTreeIterator::SetExcludeIFrames()
{
	include_iframes = FALSE;
}

void DocumentTreeIterator::SetIncludeEmpty()
{
	include_empty = TRUE;
}

BOOL DocumentTreeIterator::Next(BOOL skip_children)
{
	if (include_this)
	{
		include_this = FALSE;
		if (include_empty || current->GetCurrentDoc())
			return TRUE;
	}

	FramesDocument *document = current->GetCurrentDoc();
	FramesDocElm *frame = current->GetFrame();

	if (document && !skip_children)
		if (FramesDocElm *frm_root = document->GetFrmDocRoot())
			frame = frm_root;
		else if (include_iframes)
		{
			if (FramesDocElm *ifrm_root = document->GetIFrmRoot())
				if (!ifrm_root->Empty())
					frame = ifrm_root;
		}

	BOOL was_frame_root = TRUE;

	while (frame)
	{
		FramesDocElm *leaf = (FramesDocElm *) frame->FirstLeaf();

		if (leaf == frame)
			leaf = NextLeaf(frame);

		frame = leaf;

		if (frame)
			do
				if (include_empty || frame->GetCurrentDoc())
				{
					current = frame->GetDocManager();
					return TRUE;
				}
			while ((frame = NextLeaf(frame)) != 0);

		if (current == start)
			return FALSE;

		if (was_frame_root)
		{
			frame = current->GetFrame();
			was_frame_root = FALSE;
		}
		else if (FramesDocument *parent_document = current->GetParentDoc())
		{
			current = parent_document->GetDocManager();
			frame = current->GetFrame();
		}
		else
			return FALSE;
	}

	return FALSE;
}

void DocumentTreeIterator::SkipTo(DocumentManager* doc_man)
{
	current = doc_man;
}

DocumentManager *DocumentTreeIterator::GetDocumentManager()
{
	return current;
}

FramesDocElm *DocumentTreeIterator::GetFramesDocElm()
{
	return current->GetFrame();
}

FramesDocument *DocumentTreeIterator::GetFramesDocument()
{
	return current->GetCurrentDoc();
}

FramesDocElm* DocumentTreeIterator::NextLeaf(FramesDocElm* frame) const
{
	FramesDocElm* leaf = frame;
	FramesDocElm* subtree_root = start->GetFrame();

	if (leaf == subtree_root)
		// No more leaf nodes in this subtree.

		return NULL;

	while (!leaf->Suc())
	{
		// If leaf doesn't have a successor, go looking in parent

		if (leaf == subtree_root)
			// No more leaf nodes in this subtree.

			return NULL;

		leaf = leaf->Parent();

		if (!leaf)
			return NULL;
	}

	leaf = leaf->Suc();

	// Then, traverse down leaf to find first child, if it has any

	while (leaf->FirstChild())
		leaf = leaf->FirstChild();

	return leaf;
}

void
DocumentManager::PostDelayedActionMessage(int delay_ms)
{
	double new_target_time = g_op_time_info->GetRuntimeMS() + delay_ms;
	if (is_delayed_action_msg_posted && target_time_for_delayed_action_msg > new_target_time)
	{
		// Someone wants something to happen more urgently so delete the slow message.
		mh->RemoveDelayedMessage(MSG_DOC_DELAYED_ACTION, 0, 0);
		is_delayed_action_msg_posted = FALSE;
	}

	if (!is_delayed_action_msg_posted)
	{
		mh->PostMessage(MSG_DOC_DELAYED_ACTION, 0, 0, delay_ms);
		is_delayed_action_msg_posted = TRUE;
		target_time_for_delayed_action_msg = new_target_time;
	}
}


#ifdef TRUST_RATING
OP_STATUS
DocumentManager::CheckTrustRating(URL url, BOOL offline_check_only, BOOL force_check)
{
	TrustRating rating = Not_Set;
	OP_STATUS status = OpStatus::OK;
	BOOL check_done = TRUE;

	OP_ASSERT(!url.IsEmpty());

	if (!parent_doc)
		GetWindow()->SetTrustRating(Not_Set, TRUE);

	BOOL online_check_allowed = !offline_check_only && (force_check || g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableTrustRating));

	if (url.Type() != URL_JAVASCRIPT
#ifdef SELFTEST
		&& !url.GetAttribute(URL::KIsGeneratedBySelfTests)
#endif // SELFTEST
	   )
	{
		BOOL need_online;
		RETURN_IF_ERROR(ServerTrustChecker::GetTrustRating(url, rating, need_online));
		if (need_online && online_check_allowed)
		{
			BOOL is_local, need_resolve;
			RETURN_IF_ERROR(ServerTrustChecker::IsLocalURL(url, is_local, need_resolve));
			if (is_local)
				rating = Unknown_Trust;
			else
			{
				RETURN_IF_ERROR(AddTrustCheck(url, need_resolve));
				check_done = FALSE;
			}
		}
		else
		{
			ServerName *server_name = url.GetServerName();
			if (server_name && rating == Untrusted_Ask_Advisory /* site is untrusted but advisory must be asked to get known real trusting rate */)
			{
				Advisory *sn_advisory = server_name->GetAdvisory(url.GetAttribute(URL::KUniName));
				TrustInfoParser::Advisory advisory;
				if (sn_advisory)
				{
					advisory.homepage_url = sn_advisory->homepage.CStr();
					advisory.advisory_url = sn_advisory->advisory.CStr();
					advisory.text = sn_advisory->text.CStr();
					advisory.type = sn_advisory->type;
					advisory.id = sn_advisory->src;
					if (sn_advisory->type == SERVER_SIDE_FRAUD_MALWARE)
					{
						rating = Malware;
					}
					else if (sn_advisory->type == SERVER_SIDE_FRAUD_PHISHING)
					{
						rating = Phishing;
					}
					else
					{
						rating = Unknown_Trust;
					}
				}
				else // rating is Ask_Avisory but advisory is not going to tell anything so set phishing
					rating = Phishing;
				// if site is phishing or offers malware generate warning page if not already bypassed
				if ((rating == Phishing || rating == Malware) && !server_name->GetTrustRatingBypassed())
				{
					if (OpStatus::IsSuccess(status = GenerateAndShowFraudWarningPage(url, &advisory)))
						status = OpStatus::ERR_NO_ACCESS;
				}
				// Prevent the destructor from deallocating these strings.
				advisory.homepage_url = NULL;
				advisory.advisory_url = NULL;
				advisory.text = NULL;
			}
		}
	}

	if (check_done) // Notify the final rating if the check is done.
	{
		/** Don't leave the rating as Not_Set unless it couldn't be checked for real
		 * e.g. due to the fraud checking feature disabled. */
		if (rating == Not_Set && online_check_allowed)
			rating = Unknown_Trust;

		GetWindow()->SetTrustRating(rating);
	}

	return status;
}

OP_STATUS DocumentManager::AddTrustCheck(URL url, BOOL need_ip_resolve)
{
	OP_ASSERT(!url.IsEmpty());

	// Check if we're already checking this server:
	for (ServerTrustChecker* check = (ServerTrustChecker*)m_active_trust_checks.First();
		 check;
		 check = (ServerTrustChecker*)check->Suc())
	{
		if (check->URLBelongsToThisServer(url))
		{
			if (!check->IsCheckingURL(url))
				RETURN_IF_ERROR(check->AddURL(url));

			return OpStatus::OK;
		}
	}

	// Need to start a new check
	ServerTrustChecker* checker = OP_NEW(ServerTrustChecker, (m_next_checker_id++, this));
	if (!checker)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = checker->Init(url);
	if (OpStatus::IsSuccess(status))
		status = checker->StartCheck(need_ip_resolve);

	if (OpStatus::IsSuccess(status))
		checker->Into(&m_active_trust_checks);
	else
		OP_DELETE(checker);

	return status;
}

OP_STATUS DocumentManager::GenerateAndShowFraudWarningPage(const URL& blocked_url, TrustInfoParser::Advisory *advisory)
{
	OP_ASSERT(blocked_url.GetServerName() != NULL);

	GetCurrentURL().StopLoading(GetMessageHandler());

	URL fraud_warning_url = g_url_api->GetURL(PHISHING_WARNING_URL);
	g_url_api->MakeUnique(fraud_warning_url);
	fraud_warning_url.Unload();

	OperaFraudWarning redirect_page(fraud_warning_url, blocked_url, advisory);
	RETURN_IF_ERROR(redirect_page.GenerateData());
	RETURN_IF_ERROR(fraud_warning_url.SetAttribute(URL::KIsClickThroughPage, TRUE));

	RETURN_IF_ERROR(fraud_warning_url.SetAttribute(URL::KReferrerURL, blocked_url));

	/*
		The fraud checker blocked 'url'. However, because the fraud check is async, if it was slow,
		'url' could have been redirected one or more times and the document being rendered being a
		bit ahead in the redirect chain.
		Or the reverse might also happen. Fraud check was too quick and the document has not yet
		received a single header.
		So, check if the last url in both redirect chains are the same.
	*/
	if (CurrentDocListElm() != NULL && blocked_url.Id(TRUE) == CurrentDocListElm()->GetUrl().Id(TRUE))
		SetUseHistoryNumber(CurrentDocListElm()->Number());

	URL empty_url;
	OpenURL(fraud_warning_url, DocumentReferrer(empty_url), FALSE, FALSE);

	return OpStatus::OK;
}
#endif // TRUST_RATING

#ifdef ERROR_PAGE_SUPPORT
OP_STATUS DocumentManager::GenerateAndShowCrossNetworkErrorPage(const URL& blocked_url, Str::LocaleString error)
{
	OP_ASSERT(blocked_url.GetServerName() != NULL);

	GetCurrentURL().StopLoading(GetMessageHandler());

	URL crossnet_warning_url = g_url_api->GetURL(OPERA_CROSS_NETWORK_ERROR_URL);
	g_url_api->MakeUnique(crossnet_warning_url);
	crossnet_warning_url.Unload();

	OpCrossNetworkError redirect_page(crossnet_warning_url, error, blocked_url);
	RETURN_IF_ERROR(redirect_page.GenerateData());
	RETURN_IF_ERROR(crossnet_warning_url.SetAttribute(URL::KIsClickThroughPage, TRUE));

	RETURN_IF_ERROR(crossnet_warning_url.SetAttribute(URL::KReferrerURL, blocked_url));

	URL empty_url;
	OpenURL(crossnet_warning_url, DocumentReferrer(empty_url), FALSE, FALSE);

	return OpStatus::OK;
}

OP_STATUS DocumentManager::GenerateAndShowClickJackingBlock(const URL& blocked_url)
{
	OP_ASSERT(blocked_url.GetServerName() != NULL || blocked_url.Type() == URL_OPERA);

	GetCurrentURL().StopLoading(GetMessageHandler());

	URL clickjack_url = g_url_api->GetURL(OPERA_CLICKJACK_BLOCK_URL);
	g_url_api->MakeUnique(clickjack_url);
	clickjack_url.Unload();

	OpClickJackingBlock error_page(clickjack_url, blocked_url);
	RETURN_IF_ERROR(error_page.GenerateData());
	RETURN_IF_ERROR(clickjack_url.SetAttribute(URL::KIsClickThroughPage, TRUE));

	RETURN_IF_ERROR(clickjack_url.SetAttribute(URL::KReferrerURL, blocked_url));

	if (CurrentDocListElm())
		SetUseHistoryNumber(CurrentDocListElm()->Number());
	else if (GetParentDoc() && GetParentDoc()-> GetDocManager()->CurrentDocListElm())
		SetUseHistoryNumber(GetParentDoc()-> GetDocManager()->CurrentDocListElm()->Number());

	URL empty_url;
	OpenURL(clickjack_url, DocumentReferrer(empty_url), FALSE, FALSE);

	return OpStatus::OK;
}


OP_STATUS
DocumentManager::HandleClickThroughUrl(URL& url, DocumentReferrer& ref_url)
{
	if (url.Type() == URL_OPERA && ref_url.url.Type() == URL_OPERA &&
		ref_url.url.GetAttribute(URL::KIsClickThroughPage))
	{
		// This if block detects when a opera: link is clicked inside another opera: page.

		OP_ASSERT(ref_url.url.GetAttribute(URL::KIsGeneratedByOpera));

		OpString8 referrer_str, url_str;
		RETURN_IF_ERROR(ref_url.url.GetAttribute(URL::KName, referrer_str));

		if (referrer_str.Compare(OPERA_CROSS_NETWORK_ERROR_URL) == 0)
		{
			// Our friendly cross-network error page !

			RETURN_IF_ERROR(url.GetAttribute(URL::KName, url_str));

#if defined PREFS_WRITE && defined PREFS_HOSTOVERRIDE
			if (url_str.Compare("opera:proceed/override") == 0)
			{
				url = ref_url.url.GetAttribute(URL::KReferrerURL);
				ref_url = DocumentReferrer();

				OP_ASSERT(url.GetServerName() != NULL);

				TRAP_AND_RETURN(status, g_pcnet->OverridePrefL(url.GetServerName()->UniName(), PrefsCollectionNetwork::AllowCrossNetworkNavigation, 1, TRUE));

				// Replace the current history position, so we don't
				// get a history entry with the click-through page.
				if (CurrentDocListElm())
					SetUseHistoryNumber(CurrentDocListElm()->Number());
			}
			else
#endif //PREFS_HOSTOVERRIDE
			if (url_str.Compare("opera:proceed") == 0)
			{
				url = ref_url.url.GetAttribute(URL::KReferrerURL);
				ref_url = DocumentReferrer();

				OP_ASSERT(url.GetServerName() != NULL);

				// Let the host be resolved again.
				url.GetServerName()->SetNetType(NETTYPE_UNDETERMINED);

				// Replace the current history position, so we don't
				// get a history entry with the click-through page.
				if (CurrentDocListElm())
					SetUseHistoryNumber(CurrentDocListElm()->Number());
			}
			else
			{
				// Perhaps harmess, but still we should make sure all opera:links are properly handled.
				OP_ASSERT(!"What did you click ?");
			}

			return OpStatus::OK;
		}
#ifdef TRUST_RATING
		else if (referrer_str.Compare(PHISHING_WARNING_URL) == 0)
		{
			// Our friendly phishing protection page

			RETURN_IF_ERROR(url.GetAttribute(URL::KName, url_str));

			if (url_str.Compare("opera:proceed") == 0)
			{
				url = ref_url.url.GetAttribute(URL::KReferrerURL);
				ref_url = DocumentReferrer();

				OP_ASSERT(url.GetServerName() != NULL);

				// If the refering URL is a opera:site-warning URL, then it's
				// the 'I want to get scammed link' in a phishing site warning
				// so allow user to access the site even if it's unsafe.
				url.GetServerName()->SetTrustRatingBypassed(TRUE);

				// Replace the current history position, so we don't
				// get a history entry with the click-through page.
				if (CurrentDocListElm())
					SetUseHistoryNumber(CurrentDocListElm()->Number());
			}
			else if (url_str.Compare("opera:back") == 0)
			{
				GetWindow()->SetHistoryPrev();
				return OpStatus::ERR; // Prevent continue loading the url, given that we just navigated backwards in history.
			}
			else
			{
				// Perhaps harmess, but still we should make sure all opera:links are properly handled.
				OP_ASSERT(!"What did you click ?");
			}

			return OpStatus::OK;
		}
#endif // TRUST_RATING
		else if (referrer_str.Compare(OPERA_CLICKJACK_BLOCK_URL) == 0)
		{
			RETURN_IF_ERROR(url.GetAttribute(URL::KName, url_str));

			if (url_str.Compare("opera:proceed") == 0)
			{
				URL blocked_url = ref_url.url.GetAttribute(URL::KReferrerURL);

				OP_ASSERT(!blocked_url.IsEmpty());

				DocumentReferrer ref;
				if (GetParentDoc())
				{
					// Regular click in the frame.
					GetWindow()->GetClickedURL(blocked_url, ref, -2, TRUE, FALSE);
					return OpStatus::ERR; // Prevent continue loading the url, given that this just opened a new window.
				}
				else
				{
					// Middle click or ctrl+shift click: this call to
					// DocMgr::OpenURL already happens under the new Window
					// in the top level DocumentManager.
					url = blocked_url;
					ref_url = DocumentReferrer();
				}
			}
			else
			{
				// Perhaps harmess, but still we should make sure all opera:links are properly handled.
				OP_ASSERT(!"What did you click ?");
			}
		}
	}

	return OpStatus::OK;
}
#endif // ERROR_PAGE_SUPPORT

#ifdef WEBSERVER_SUPPORT
	class UniteURL_Generator : public OperaURL_Generator
	{
	public:
		virtual GeneratorMode GetMode() const { return KGenerateLoadHandler; }
	};


OP_STATUS DocumentManager::GenerateAndShowUniteWarning(URL &url, URL& source)
{
	if (!g_opera->dochand_module.unitewarningpage_generator)
	{
		UniteURL_Generator* unite_error_page_address = OP_NEW(UniteURL_Generator, ());
		if (!unite_error_page_address)
			return OpStatus::ERR_NO_MEMORY;

		OP_STATUS status = unite_error_page_address->Construct(UNITE_WARNING_PAGE_URL, FALSE);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(unite_error_page_address);
			return status;
		}

		g_opera->dochand_module.unitewarningpage_generator = unite_error_page_address;
		g_url_api->RegisterOperaURL(unite_error_page_address);
	}

	URL warning_url = g_url_api->GetURL("opera:" UNITE_WARNING_PAGE_URL, source.GetContextId());
	g_url_api->MakeUnique(warning_url);

	BOOL block_access = GetParentDoc() != NULL; // block iframes/frames completely
	OperaUniteAdminWarningURL warning_page(warning_url, url, block_access);
	RETURN_IF_ERROR(warning_page.GenerateData());

	OpenURL(warning_url, DocumentReferrer(source), FALSE, FALSE);

	return OpStatus::OK;
}
#endif // WEBSERVER_SUPPORT

#ifdef WEB_TURBO_MODE
OP_STATUS DocumentManager::UpdateTurboMode()
{
	DocumentTreeIterator iter(this);
	iter.SetIncludeThis();

	while (iter.Next(FALSE))
	{
		FramesDocument *doc = iter.GetFramesDocument();
		if (doc && doc->GetLogicalDocument())
			RETURN_IF_MEMORY_ERROR(doc->GetLogicalDocument()->UpdateTurboMode());
	}

	return OpStatus::OK;
}
#endif // WEB_TURBO_MODE

BOOL DocumentManager::DisplayContentIfAvailable(URL &url, URL_ID url_id, MH_PARAM_2 load_status)
{
	BOOL network_error = load_status == ERR_NETWORK_PROBLEM || load_status == ERR_COMM_NETWORK_UNREACHABLE ||
						 load_status == ERR_COMM_CONNECT_FAILED || load_status == ERR_CONNECT_TIMED_OUT ||
	                     load_status == ERR_COMM_PROXY_CONNECT_FAILED || load_status == ERR_COMM_PROXY_HOST_NOT_FOUND ||
	                     load_status == ERR_COMM_PROXY_HOST_UNAVAILABLE || load_status == ERR_COMM_PROXY_CONNECTION_REFUSED;

	if (!network_error && //network error implies that no new content is available
		url.ContentLoaded() > 0 &&
		!url.GetAttribute(URL::KIsGeneratedByOpera, TRUE))
	{
		if (load_stat == WAIT_FOR_HEADER || load_stat == WAIT_FOR_ACTION)
			HandleHeaderLoaded(url_id, TRUE);
		else
			HandleDataLoaded(url_id);

		return TRUE;
	}

	return FALSE;
}

/* virtual */
void
DocumentManager::SecurityCheckCallback::OnSecurityCheckSuccess(BOOL allowed, ChoicePersistenceType /*type*/)
{
	SecurityCheckFinished();
	m_allowed = allowed;

	if (m_invalidated)
	{
		OP_DELETE(this);
		return;
	}

	if (!m_suspended)
		return;

	if (allowed)
	{
		if (m_redirect)
		{
			if (!m_docman->queued_messages.Empty())
				m_docman->mh->PostMessage(MSG_DOCHAND_PROCESS_QUEUED_MESSAGE, 0, 0);
			else
				m_docman->queue_messages = FALSE;
		}
		else
			m_docman->OpenURL(m_url, m_referrer, m_check_if_expired, m_reload, m_options);
	}
	else
		m_docman->HandleURLAccessDenied(m_url, m_referrer.url);
}

/* virtual */
void
DocumentManager::SecurityCheckCallback::OnSecurityCheckError(OP_STATUS error)
{
	SecurityCheckFinished();
	m_allowed = FALSE;

	if (m_invalidated)
	{
		OP_DELETE(this);
		return;
	}

	if (m_suspended)
		m_docman->HandleURLAccessDenied(m_url, m_referrer.url);
}

void
DocumentManager::SecurityCheckCallback::SecurityCheckFinished()
{
	m_done = TRUE;

	Remove();
	if (m_options.origin_thread)
		m_options.origin_thread->Unblock();
}

void
DocumentManager::SecurityCheckCallback::SetURLs(URL& url, const DocumentReferrer& referrer)
{
	m_url = url;
	m_referrer = referrer;
}

void
DocumentManager::SecurityCheckCallback::PrepareForSuspending(BOOL check_if_expired, BOOL reload, const OpenURLOptions& options)
{
	m_suspended = TRUE;

	m_check_if_expired = check_if_expired;
	m_reload = reload;
	m_options = options;

	if (options.origin_thread)
	{
		options.origin_thread->Block();
		options.origin_thread->AddListener(this);
	}
}

void
DocumentManager::SecurityCheckCallback::PrepareForSuspendingRedirect()
{
	m_suspended = TRUE;
	m_redirect = TRUE;

	// Turn on message queuing to ensure that all redirects are
	// security checked before MSG_URL_LOADED for the final redirect
	// target is delivered.
	m_docman->queue_messages = TRUE;
}

void
DocumentManager::SecurityCheckCallback::Reset()
{
	m_done = FALSE;
	m_suspended = FALSE;
	m_redirect = FALSE;
	m_allowed = FALSE;
	m_invalidated = FALSE;
}

/* virtual */ OP_STATUS
DocumentManager::SecurityCheckCallback::Signal(ES_Thread *thread, ES_ThreadSignal signal)
{
	OP_ASSERT(m_options.origin_thread == thread);
	switch (signal)
	{
	case ES_SIGNAL_FAILED:
	case ES_SIGNAL_FINISHED:
	case ES_SIGNAL_CANCELLED:
		m_options.origin_thread = NULL;
		Remove();
		break;
	}
	return OpStatus::OK;
}

void
DocumentManager::SetAction(ViewAction act)
{
#ifdef WEB_HANDLERS_SUPPORT
	if (!action_locked)
#endif // WEB_HANDLERS_SUPPORT
		current_action = act;
}
