/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/doc/frm_doc.h"

#include "modules/img/image.h"
#include "modules/viewers/viewers.h"
#include "modules/doc/documentstate.h"
#include "modules/doc/documentinteractioncontext.h"
#include "modules/doc/caret_painter.h"
#include "modules/doc/caret_manager.h"
#include "modules/forms/formvalue.h"
#include "modules/forms/formvaluelistener.h"
#include "modules/forms/formmanager.h"
#include "modules/forms/form.h"
#include "modules/formats/uri_escape.h"
#include "modules/pi/OpPrinterController.h"
#include "modules/pi/network/OpSocketAddress.h"

#ifdef WAND_SUPPORT
#include "modules/wand/wandmanager.h"
#endif

#ifdef DOCUMENT_EDIT_SUPPORT
#include "modules/documentedit/OpDocumentEdit.h"
#endif

#ifdef MEDIA_HTML_SUPPORT
#include "modules/media/mediaelement.h"
#endif // MEDIA_HTML_SUPPORT

#ifdef SCOPE_SUPPORT
# include "modules/scope/scope_readystate_listener.h"
# include "modules/scope/scope_element_inspector.h"
# include "modules/probetools/probetimeline.h"
#endif // SCOPE_SUPPORT

#include "modules/pi/OpView.h"
#include "modules/pi/ui/OpUiInfo.h"

#include "modules/dochand/viewportcontroller.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/display/color.h"
#include "modules/encodings/utility/charsetnames.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/display/prn_info.h"

#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esterm.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/encodings/decoders/utf8-decoder.h"
#include "modules/dom/domutils.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_print.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"

#include "modules/display/VisDevListeners.h"
#include "modules/display/vis_dev.h"
#include "modules/doc/html_doc.h"
#include "modules/dochand/docman.h"
#include "modules/dochand/fdelm.h"
#include "modules/layout/cascade.h"
#include "modules/layout/frm.h"
#include "modules/layout/numbers.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/src/textdata.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/media/media.h"
#include "modules/media/mediatrack.h"
#include "modules/probetools/probepoints.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/style/css_media.h"
#include "modules/util/handy.h"
#include "modules/util/htmlify.h"
#include "modules/util/opfile/opfile.h"
#include "modules/encodings/utility/opstring-encodings.h"
#include "modules/widgets/AutoCompletePopup.h"
#include "modules/windowcommander/src/TransferManagerDownload.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/xmlutils/xmlparser.h"

#include "modules/dom/domenvironment.h"
#include "modules/dom/domevents.h"
#include "modules/dom/domeventtypes.h"
#include "modules/dom/domutils.h"

#ifdef JS_PLUGIN_SUPPORT
# include "modules/jsplugins/src/js_plugin_manager.h"
#endif // JS_PLUGIN_SUPPORT

#ifdef HAS_ATVEF_SUPPORT
# include "modules/ecmascript_utils/esasyncif.h"
#endif // HAS_ATVEF_SUPPORT

#ifdef _PLUGIN_SUPPORT_
# include "modules/ns4plugins/opns4pluginhandler.h"
#endif // _PLUGIN_SUPPORT_

#ifdef _WML_SUPPORT_
# include "modules/logdoc/wml.h"
#endif // _WML_SUPPORT_

#ifdef _PRINT_SUPPORT_
#include "modules/pi/OpLocale.h"
#endif // _PRINT_SUPPORT_

#ifdef USE_OP_CLIPBOARD
# include "modules/pi/OpClipboard.h"
#endif // USE_OP_CLIPBOARD

#ifdef WIC_USE_DOWNLOAD_CALLBACK
# include "modules/windowcommander/src/TransferManagerDownload.h"
#endif

#ifdef GADGET_SUPPORT
# include "modules/gadgets/OpGadget.h"
#endif // GADGET_SUPPORT

#include "modules/logdoc/xmlevents.h"

#include "modules/url/url_man.h"
#include "modules/url/url_sn.h"
#include "modules/url/url_rep.h"
#include "modules/display/prn_dev.h"
#include "modules/display/style.h"
#include "modules/display/styl_man.h"

#include "modules/olddebug/timing.h"

#include "modules/forms/webforms2support.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
#include "modules/accessibility/AccessibleDocument.h"
#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/layout/layout_workplace.h"
#include "modules/layout/box/box.h"
#include "modules/layout/content/content.h"
#include "modules/layout/traverse/traverse.h"
#include "modules/layout/layoutprops.h"

#ifdef SVG_SUPPORT
# include "modules/svg/SVGManager.h"
# include "modules/svg/src/SVGUtils.h"
# include "modules/svg/svg_workplace.h"
# include "modules/svg/svg_image.h"
#endif // SVG_SUPPORT

#ifdef HISTORY_SUPPORT
# include "modules/history/OpHistoryModel.h"
#endif // HISTORY_SUPPORT

#ifdef PLATFORM_FONTSWITCHING
# include "modules/pi/OpView.h"
#endif // PLATFORM_FONTSWITCHING

#include "modules/util/gen_str.h"

#ifdef QUICK
# include "adjunct/desktop_util/string/htmlify_more.h"
#endif

#ifdef SKIN_SUPPORT
# include "modules/skin/OpSkinManager.h"
#endif // SKIN_SUPPORT

#ifdef USE_ABOUT_FRAMEWORK
# include "modules/about/oppageinfo.h"
# include "modules/about/opsuppressedurl.h"
# include "modules/about/operafraudwarning.h"
#endif

#ifdef ACTION_MAKE_READABLE_ENABLED
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#endif

#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/comm.h"

#ifdef _SPAT_NAV_SUPPORT_
# include "modules/spatial_navigation/handler_api.h"
# include "modules/spatial_navigation/sn_handler.h"
#endif // _SPAT_NAV_SUPPORT_

#ifdef APPLICATION_CACHE_SUPPORT
# include "modules/applicationcache/application_cache_manager.h"
#endif // APPLICATION_CACHE_SUPPORT

#ifdef DRAG_SUPPORT
# include "modules/dragdrop/dragdrop_manager.h"
#endif // DRAG_SUPPORT

#ifdef USE_OP_CLIPBOARD
# include "modules/dragdrop/clipboard_manager.h"
#endif // USE_OP_CLIPBOARD

#define PREVIEW_PAPER_LEFT_OFFSET	10
#define PREVIEW_PAPER_TOP_OFFSET	10
#define MAX_NESTING_OVERFLOW_CONTAINER 40

#ifdef SCOPE_PROFILER

/**
 * Custom probe for PROBE_EVENT_PAINT. We reduce noise in the
 * target function by splitting the activation into a separate class.
 */
class OpPaintProbe
	: public OpPropertyProbe
{
public:

	/**
	 * Activate the OpPaintProbe.
	 *
	 * @param timeline The timeline to report to. Can not be NULL.
	 * @param rect The area that is about to be painted.
	 *
	 * @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS Activate(OpProbeTimeline *timeline, const RECT &rect, const OpPoint &view_offset)
	{
		const void *key = timeline->GetUniqueKey();

		OpPropertyProbe::Activator<6> act(*this);
		RETURN_IF_ERROR(act.Activate(timeline, PROBE_EVENT_PAINT, key));

		act.AddInteger("x", rect.left);
		act.AddInteger("y", rect.top);
		act.AddInteger("w", rect.right - rect.left);
		act.AddInteger("h", rect.bottom - rect.top);

		act.AddInteger("ox", view_offset.x);
		act.AddInteger("oy", view_offset.y);

		return OpStatus::OK;
	}
};

# define PAINT_PROBE(TMP_PROBE, TMP_TIMELINE, TMP_RECT, TMP_VIEW_OFFSET) \
	OpPaintProbe TMP_PROBE; \
	if (TMP_TIMELINE) \
		RETURN_IF_ERROR(TMP_PROBE.Activate(TMP_TIMELINE, TMP_RECT, TMP_VIEW_OFFSET));

#else // SCOPE_PROFILER
# define PAINT_PROBE(TMP_PROBE, TMP_TIMELINE, TMP_RECT, TMP_VIEW_OFFSET) ((void)0)
#endif // SCOPE_PROFILER

/**
 * This class is designed to check if it's suitable to send an onload
 * event once a script has completed. It's intended to run if the
 * script has done anything that might have changed the load state
 * such as removed or added frames. There can only be one per document
 * and it should listen to the end of the "root" thread. If there
 * is one already then a new one doesn't need to be added.
 *
 * Also, if there is one and it looks like the document might be
 * loaded, it's prudent to delay the onload until this has run.
 */
class AsyncOnloadChecker : public ES_ThreadListener
{
private:
	FramesDocument* doc;

public:
	AsyncOnloadChecker(FramesDocument* frames_doc, BOOL about_blank_only)
		: doc(frames_doc),
		  about_blank_only(about_blank_only)
	{
		OP_ASSERT(doc);
	}

	BOOL about_blank_only;

	/**
	 * Removes this listener and deletes it. There must no remain any
	 * pointers to it in the document.
	 */
	void DocumentDestroyed()
	{
		Remove();
		OP_DELETE(this);
	}

	// See base class documentation
	virtual OP_STATUS Signal(ES_Thread *, ES_ThreadSignal signal)
	{
		OP_NEW_DBG("AsyncOnloadChecker::Signal", "onload");
		if (signal == ES_SIGNAL_CANCELLED ||
		    signal == ES_SIGNAL_FINISHED ||
		    signal == ES_SIGNAL_FAILED)
		{
			// This is where we end up after a script that has removed iframes has finished.
			doc->onload_checker = NULL;

			if (about_blank_only)
				// If we're no longer at about:blank, do not even attempt to deliver onload.
				if (!IsAboutBlankURL(doc->GetURL()))
					return OpStatus::OK;

			if (doc->ifrm_root)
				FramesDocument::CheckOnLoad(NULL, doc->ifrm_root);
			else
				FramesDocument::CheckOnLoad(doc, NULL);
		}
		return OpStatus::OK;
	}
};

static const ServerName *GetServerNameFromURL(URL url)
{
	return static_cast<const ServerName *>(url.GetAttribute(URL::KServerName, (void *) NULL));
}

HTML_Element* LoadingImageElementIterator::GetNext()
{
	LoadInlineElm* lie = NULL;
	do
	{
		if (m_next_helistelm)
		{
			while (m_next_helistelm && m_next_helistelm->GetInlineType() != IMAGE_INLINE)
				m_next_helistelm = m_next_helistelm->Suc();

			if (m_next_helistelm)
			{
				HEListElm* current = m_next_helistelm;
				m_next_helistelm = m_next_helistelm->Suc();
				return current->HElm();
			}
		}

		OP_ASSERT(!m_next_helistelm);

		lie = m_started ? m_lie_iterator.Next() : m_lie_iterator.First();
		m_started = TRUE;
		// Look for an image that is loading
		BOOL listelm_loading = FALSE;
		while (lie && !listelm_loading)
		{
			listelm_loading = lie->GetLoading();
			for (HEListElm* elm = lie->GetFirstHEListElm(); elm && !listelm_loading; elm = elm->Suc())
			{
				if (elm->GetInlineType() == IMAGE_INLINE && !elm->GetEventSent())
					listelm_loading = TRUE;
			}

			if (!listelm_loading)
				lie = m_lie_iterator.Next();
		}

		if (lie)
			m_next_helistelm = lie->GetFirstHEListElm();
	}
	while (lie);

	return NULL;
}

#ifdef WAND_SUPPORT
OP_STATUS WandInserter::Signal(ES_Thread *, ES_ThreadSignal signal)
{
	if (signal == ES_SIGNAL_CANCELLED ||
		signal == ES_SIGNAL_FINISHED  ||
		signal == ES_SIGNAL_FAILED)
	{
		if (frames_doc)
			frames_doc->InsertPendingWandData();

		frames_doc = NULL;
	}

	return OpStatus::OK;
}
#endif // WAND_SUPPORT

class DocumentFormSubmitCallback
	: public OpDocumentListener::FormSubmitCallback
	, public ES_ThreadListener
#ifdef GADGET_SUPPORT
	, public MessageObject
#endif // GADGET_SUPPORT
{
protected:
	FramesDocument *document;
	URL form_url;
	uni_char *windowname;
	BOOL user_initiated;
	BOOL open_in_other_window, open_in_background;
	ES_Thread *thread;
	OpSecurityState secstate;
#ifdef GADGET_SUPPORT
	BOOL waiting_for_callback; ///< TRUE if we're waiting for an async security check
#endif // GADGET_SUPPORT

	BOOL called; ///< The UI has called back to this callback
	BOOL delayed; ///< TRUE if Core has Executed but the UI hadn't yet decided
	BOOL submit; ///< TRUE if the UI wanted the submit to go on
	BOOL cancelled; ///< TRUE if something/someone has made a submit impossible

public:
	DocumentFormSubmitCallback(FramesDocument *document, BOOL user_initiated, URL form_url, BOOL open_in_other_window, BOOL open_in_background)
		: document(document),
		  form_url(form_url),
		  windowname(NULL),
		  user_initiated(user_initiated),
		  open_in_other_window(open_in_other_window),
		  open_in_background(open_in_background),
		  thread(NULL),
#ifdef GADGET_SUPPORT
		  waiting_for_callback(FALSE),
#endif // GADGET_SUPPORT
		  called(FALSE),
		  delayed(FALSE),
		  submit(FALSE),
		  cancelled(FALSE)
	{
	}

	~DocumentFormSubmitCallback()
	{
		if (thread)
		{
			ES_ThreadListener::Remove();
			OpStatus::Ignore(thread->Unblock(ES_BLOCK_USER_INTERACTION));
		}

		OP_DELETEA(windowname);

		if (document)
			document->ResetCurrentFormRequest();

#ifdef GADGET_SUPPORT
		if (waiting_for_callback)
			g_main_message_handler->UnsetCallBacks(this);
#endif // GADGET_SUPPORT
	}

#ifdef GADGET_SUPPORT
	BOOL IsWaitingForInitialSecurityCheck() { return waiting_for_callback; }
	void SetWaitingForSecurityCheck(BOOL new_value) { waiting_for_callback = new_value; }
#endif // GADGET_SUPPORT

	virtual const uni_char *FormServerName()
	{
		const ServerName *sn = GetServerNameFromURL(form_url);
		if (sn)
			return sn->UniName();
		else
			return UNI_L("");
	}

	virtual const uni_char *ReferrerServerName()
	{
		const ServerName *sn = GetServerNameFromURL(document->GetURL());
		if (sn)
			return sn->UniName();
		else
			return UNI_L("");
	}

	virtual BOOL FormServerIsHTTP()
	{
		return form_url.Type() == URL_HTTP;
	}

	virtual OpDocumentListener::SecurityMode ReferrerSecurityMode()
	{
		return document->GetWindow()->GetWindowCommander()->GetSecurityModeFromState(document->GetURL().GetAttribute(URL::KSecurityStatus));
	}

	virtual OpWindowCommander* GetTargetWindowCommander()
	{
		if (windowname)
		{
			int sub_win_id;
			if (Window *window = g_windowManager->GetNamedWindow(document->GetWindow(), windowname, sub_win_id, FALSE))
				/* FIXME: Should perform window/frame spoofing check between
				   'document' and 'window' to avoid displaying dialog over
				   window that won't be affected. */
				return window->GetWindowCommander();
		}

		return document->GetWindow()->GetWindowCommander();
	}

	virtual OP_STATUS GenerateFormDetails(OpWindowCommander *details_commander)
	{
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	virtual void Continue(BOOL do_submit)
	{
		called = TRUE;
		submit = do_submit;

		if (!delayed) // Someone will call Execute and then we will go on
			return;

		if (!cancelled)
			OpStatus::Ignore(Execute());

		DocumentFormSubmitCallback *cb = this;
		OP_DELETE(cb);
	}

	OP_STATUS SetWindowName(const uni_char *new_windowname)
	{
		return UniSetStr(windowname, new_windowname);
	}

	virtual OP_STATUS Signal(ES_Thread *, ES_ThreadSignal signal)
	{
		if (signal == ES_SIGNAL_CANCELLED || signal == ES_SIGNAL_FAILED || signal == ES_SIGNAL_FINISHED)
		{
			// Now we have to prevent the thread from deleting us (it owns its listeners), since we're really owned by the UI (it has to call the callback to make us free ourself)
			ES_ThreadListener::Remove();
			thread = NULL;
		}
		return OpStatus::OK;
	}

	void SetThread(ES_Thread *new_thread)
	{
		OP_ASSERT(!thread);
		thread = new_thread;
		if (thread)
		{
			thread->Block(ES_BLOCK_USER_INTERACTION);
			thread->AddListener(this);
		}
	}

	OP_BOOLEAN Execute()
	{
		if (called)
		{
			ES_Thread* source_thread = thread;
			if (thread)
			{
				ES_ThreadListener::Remove();
				OpStatus::Ignore(thread->Unblock(ES_BLOCK_USER_INTERACTION));
				thread = NULL;
			}

			if (submit)
				RETURN_IF_ERROR(g_windowManager->OpenURLNamedWindow(form_url, document->GetWindow(), document, document->GetSubWinId(), windowname, user_initiated, open_in_other_window, open_in_background, TRUE, FALSE, source_thread));

			return OpBoolean::IS_TRUE; // tells the caller that we are finished and should be deleted
		}
		else
		{
			delayed = TRUE;
			return OpBoolean::IS_FALSE;
		}
	}

	void Cancel()
	{
		cancelled = TRUE;
		document = NULL;

		if (thread)
		{
			ES_ThreadListener::Remove();
			OpStatus::Ignore(thread->Unblock(ES_BLOCK_USER_INTERACTION));
			thread = NULL;
		}
	}
	OpSecurityState& SecurityState() { return secstate; }

#ifdef GADGET_SUPPORT
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 p1, MH_PARAM_2 p2)
	{
		OP_ASSERT(waiting_for_callback);
		if (thread)
			thread->Unblock();

		g_main_message_handler->UnsetCallBacks(this);

		if (document)
			document->HandleFormRequest(form_url, user_initiated, windowname, open_in_other_window, open_in_background ,thread, TRUE);
		else
			OP_DELETE(this);
	}
#endif // GADGET_SUPPORT
};


void FramesDocument::CheckHistory(int decrement, int& minhist, int& maxhist)
{
	if (frm_root)
		frm_root->CheckHistory(decrement, minhist, maxhist);
	else if (ifrm_root)
		ifrm_root->CheckHistory(decrement, minhist, maxhist);
}

void FramesDocument::RemoveFromHistory(int from)
{
	if (frm_root)
		frm_root->RemoveFromHistory(from);
	else if (ifrm_root)
		ifrm_root->RemoveFromHistory(from);
}

void FramesDocument::RemoveUptoHistory(int to)
{
	if (frm_root)
		frm_root->RemoveUptoHistory(to);
	else if (ifrm_root)
		ifrm_root->RemoveUptoHistory(to);
}

DocListElm* FramesDocument::GetHistoryElmAt(int pos, BOOL forward /* =FALSE */)
{
	FramesDocElm *use_root = frm_root ? frm_root : ifrm_root;
	if (use_root)
	{
		FramesDocElm *fde = use_root->FirstChild();

		while (fde)
		{
			DocListElm *tmp_dle = fde->GetHistoryElmAt(pos, forward);

			if (tmp_dle)
				return tmp_dle;

			fde = fde->Suc();
		}
	}

	return NULL;
}

OP_STATUS FramesDocument::UnsetAllCallbacks(BOOL really_all)
{
	MessageHandler* mh = GetMessageHandler();
	BOOL expects_delayed_action_msg = mh->HasCallBack(this, MSG_DOC_DELAYED_ACTION, 0);
	mh->UnsetCallBacks(this);

	if (!really_all && expects_delayed_action_msg)
		RETURN_IF_ERROR(mh->SetCallBack(this, MSG_DOC_DELAYED_ACTION, 0));

	return OpStatus::OK;
}

BOOL FramesDocument::HasSubDoc(FramesDocument* sub_doc)
{
	OP_ASSERT(sub_doc);
	DocumentTreeIterator iter(this);
	iter.SetIncludeThis();
	while (iter.Next())
	{
		if (sub_doc == iter.GetFramesDocument())
			return TRUE;
	}
	return FALSE;
}

extern FramesDocElm* FindFramesDocElm(Head* existing_frames, HTML_Element* he);

FramesDocument::FramesDocument(DocumentManager* doc_manager, const URL& c_url, DocumentOrigin* origin, int sub_w_id, BOOL use_plugin, BOOL inline_feed)
  : doc_manager(doc_manager),
	url(c_url),
	origin(origin),
	is_current_doc(FALSE),
	sub_win_id(sub_w_id),
	document_state(NULL),
	own_logdoc(TRUE),
	old_logdoc(NULL),
	logdoc(NULL),
	url_data_desc(NULL),
#ifdef SPECULATIVE_PARSER
	speculative_data_desc(NULL),
#endif // SPECULATIVE_PARSER
	sub_win_id_counter(1),
	frm_root(0),
	active_frm_doc(0),
	ifrm_root(0),
	doc(0),
	check_inline_expire_time(FALSE),
	keep_cleared(FALSE),
	remove_from_cache(FALSE),
	compatible_history_navigation_needed(FALSE),
	history_reload(FALSE),
	navigated_in_history(FALSE),
	document_is_reloaded(FALSE),
	use_history_navigation_mode(HISTORY_NAV_MODE_AUTOMATIC),
	override_history_navigation_mode(HISTORY_NAV_MODE_AUTOMATIC),
	parse_xml_as_html(FALSE),
	doc_start_elm_no(0),
#ifndef HAS_NOTEXTSELECTION
	selecting_text_is_ok(FALSE),
	is_selecting_text(FALSE),
	start_selection(FALSE),
	was_selecting_text(FALSE),
#endif // !HAS_NOTEXTSELECTION
	text_selection(NULL),
#ifndef MOUSELESS
	is_moving_separator(FALSE),
#endif // !MOUSELESS
#ifdef KEYBOARD_SELECTION_SUPPORT
	keyboard_selection_mode_enabled(FALSE),
#endif // KEYBOARD_SELECTION_SUPPORT
#ifndef MOUSELESS
	drag_frm(0),
	m_mouse_up_may_still_be_click(FALSE),
#endif // !MOUSELESS
	frames_stacked(FALSE),
	loading_frame(NULL),
	smart_frames(FALSE),
#ifdef XML_EVENTS_SUPPORT
	has_xml_events(FALSE),
#endif // XML_EVENTS_SUPPORT
	locked(0),
#ifdef JS_LAYOUT_CONTROL
	pause_layout(FALSE),
#endif
#ifdef DOM_FULLSCREEN_MODE
	fullscreen_element_obscured(FALSE),
#endif // DOM_FULLSCREEN_MODE
	wrapper_doc_source_read_offset(0),
	media_handheld_responded(FALSE),
	respond_to_media_type(RESPOND_TO_MEDIA_TYPE_NONE),
	frames_policy(FRAMES_POLICY_NORMAL),
	has_delayed_updates(FALSE),
	es_runtime(NULL),
	es_scheduler(NULL),
	es_async_interface(NULL),
	is_constructing_dom_environment(FALSE),
	override_es_active(FALSE),
	dom_environment(NULL),
	js_window(NULL),
	has_sent_domcontent_loaded(FALSE),
	es_recent_layout_changes(FALSE),
	// es_info handled in body
	onload_checker(NULL),
	es_generated_document(NULL),
	es_replaced_document(NULL),
	serial_nr(0),
	form_value_serial_nr(0),
	restore_form_state_thread(NULL),
	stream_position_base(0),
	last_helm(NULL),
	image_info(),
#ifdef _PRINT_SUPPORT_
	print_time(NULL),
#endif
#ifdef PAGED_MEDIA_SUPPORT
	iframe_first_page_offset(0),
	current_page(NULL),
	restore_current_page_number(-1),
#endif // PAGED_MEDIA_SUPPORT
	loading_aborted(FALSE),
	local_doc_finished(FALSE),
	doc_tree_finished(FALSE),
	document_finished_at_least_once(FALSE),
	delay_doc_finish(FALSE),
	posted_delay_doc_finish(FALSE),
	is_multipart_reload(FALSE),
#ifdef CONTENT_MAGIC
	content_magic_found(FALSE),
	content_magic_character_count(0),
	content_magic_position(0),
#endif // CONTENT_MAGIC
	era_mode(FALSE),
	layout_mode(LAYOUT_NORMAL),
	flex_font_scale(0),
	abs_positioned_strategy(ABS_POSITION_NORMAL),
	aggressive_word_breaking(WORD_BREAK_NORMAL),
	old_win_width(UINT_MAX),
	override_media_style(FALSE),
	is_generated_doc(FALSE),
	contains_only_opera_internal_scripts(FALSE),
	is_plugin(use_plugin),
#ifdef WEBFEEDS_DISPLAY_SUPPORT
	is_inline_feed(inline_feed),
	feed_listener_added(FALSE),
#endif // WEBFEEDS_DISPLAY_SUPPORT
	start_navigation_time_ms(0.0),
	is_reflow_msg_posted(FALSE),
	reflow_msg_delayed_to_time(0.0),
	is_reflow_frameset_msg_posted(FALSE),
	wait_for_styles(0),
	wait_for_paint_blockers(0),
	set_properties_reflow_requested(FALSE),
	has_blink(FALSE),
#ifdef WAND_SUPPORT
	has_wand_matches(FALSE),
	is_wand_submitting(FALSE),
	wand_inserter(NULL),
#endif // WAND_SUPPORT
	obey_next_autofocus_attr(TRUE),
	display_err_for_next_oninvalid(FALSE),
#ifdef DOCUMENT_EDIT_SUPPORT
	document_edit(NULL),
#endif
#if defined(DOCUMENT_EDIT_SUPPORT) || defined(KEYBOARD_SELECTION_SUPPORT)
	caret_painter(NULL),
	caret_manager(this),
#endif // DOCUMENT_EDIT_SUPPORT || KEYBOARD_SELECTION_SUPPORT
#ifdef SUPPORT_TEXT_DIRECTION
#endif
	reflowing(FALSE),
	undisplaying(FALSE),
	need_update_hover_element(FALSE),
	is_being_deleted(FALSE),
	is_being_freed(FALSE),
	is_being_unloaded(FALSE),
	print_copy_being_deleted(FALSE),
	suppress_focus_events(FALSE)
	, about_blank_waiting_for_javascript_url(FALSE)
#ifdef _PLUGIN_SUPPORT_
	, m_asked_about_flash_download(FALSE)
#endif // _PLUGIN_SUPPORT_
#ifdef VISITED_PAGES_SEARCH
	, m_visited_search(NULL)
	, m_index_all_text(TRUE)
#endif // VISITED_PAGES_SEARCH
	, was_document_completely_parsed_before_stopping(TRUE)
	, activity_reflow(ACTIVITY_REFLOW)
	, current_form_request(NULL)
	, inline_data_loaded_helm_next(NULL)
	, m_supported_view_modes(0)
#ifdef GEOLOCATION_SUPPORT
	, geolocation_usage_count(0)
#endif // GEOLOCATION_SUPPORT
#ifdef MEDIA_CAMERA_SUPPORT
	, camera_usage_count(0)
#endif // MEDIA_CAMERA_SUPPORT
	, num_frames_added(0)
	, num_frames_removed(0)
	, do_styled_paint(FALSE)
	, current_focused_formobject(NULL)
	, unfocused_formelement(NULL)
	, current_default_formelement(NULL)
{
	OP_ASSERT(origin);

	packed_init = 0;

	origin->IncRef();

	if (url.GetAttribute(URL::KUnique))
		reserved_url.SetURL(url);

	layout_mode = GetWindow()->GetLayoutMode();
	era_mode = GetWindow()->GetERA_Mode();

	CheckERA_LayoutMode(); // gmail/SSR fix

	es_info.onload_called = FALSE;
	es_info.onload_ready = FALSE;
	es_info.onload_pending = FALSE;
	es_info.inhibit_parent_elm_onload = FALSE;
	es_info.onload_thread_running_in_subtree = FALSE;
	es_info.inhibit_onload_about_blank = FALSE;
	es_info.onpopstate_pending = FALSE;

#ifdef _OPERA_DEBUG_DOC_
	g_opera->doc_module.m_total_document_count++;
#endif // _OPERA_DEBUG_DOC_
}

#ifdef REFRESH_DELAY_OVERRIDE
class ReviewRefreshDelayCallbackImpl :
	public OpDocumentListener::ReviewRefreshDelayCallback,
	public Link
{
private:
	FramesDocument *frm_doc;

private:
	// Internal helper methods
	LogicalDocument *GetLogicalDocument()     const { return frm_doc->GetLogicalDocument(); }
	DocumentManager *GetDocumentMananger()     const { return frm_doc->GetDocManager(); }

public:
	// constructor
	ReviewRefreshDelayCallbackImpl(FramesDocument *doc) :
		frm_doc(doc)
	{}
	~ReviewRefreshDelayCallbackImpl() { Out(); }
	void OnFramesDocumentDestroyed() { frm_doc = NULL; }

	// OpDocumentListener::ReviewRefreshDelayCallback API
	int  GetDelaySeconds()           const { return frm_doc ? GetLogicalDocument()->GetRefreshSeconds() : 0; }
	BOOL IsRedirect()                const { return frm_doc && GetLogicalDocument()->GetRefreshURL()->Id(TRUE) != frm_doc->GetURLForViewSource().Id(FALSE); }
	BOOL IsTopDocumentRefresh()      const { return frm_doc && frm_doc->IsTopDocument(); }
	const uni_char *GetDocumentURL() const { return frm_doc ? frm_doc->GetURLForViewSource().UniName(PASSWORD_HIDDEN) : NULL; }
	const uni_char *GetRefreshURL()  const { return frm_doc ? GetLogicalDocument()->GetRefreshURL()->UniName(PASSWORD_HIDDEN) : NULL; }

	void CancelRefresh()                   { OP_DELETE(this); }
	void SetRefreshSeconds(int seconds)    { if (NULL != frm_doc) GetDocumentMananger()->SetRefreshDocument(frm_doc->GetURL().Id(TRUE), (unsigned long)seconds*1000); OP_DELETE(this); }
	void Default()                         { SetRefreshSeconds(GetDelaySeconds()); } // bypass
};
#endif // REFRESH_DELAY_OVERRIDE

FramesDocument::~FramesDocument()
{
	OP_NEW_DBG("FramesDocument::~FramesDocument", "doc");
	OP_DBG((UNI_L("this=%p"), this));

#ifdef _OPERA_DEBUG_DOC_
	g_opera->doc_module.m_total_document_count--;
#endif // _OPERA_DEBUG_DOC_

	FormManager::AbortSubmitsForDocument(this);

#ifdef REFRESH_DELAY_OVERRIDE
	for (ReviewRefreshDelayCallbackImpl *cb = static_cast<ReviewRefreshDelayCallbackImpl*>(active_review_refresh_delay_callbacks.First());
		 cb; cb = static_cast<ReviewRefreshDelayCallbackImpl*>(cb->Suc()))
	{
		cb->OnFramesDocumentDestroyed();
	}
#endif // REFRESH_DELAY_OVERRIDE

#ifdef NEARBY_ELEMENT_DETECTION
	// Make sure that fingertouch is destroyed when the document is destroyed
	// FIXME: This destroys the ElementExpander even if it's a completely irrelevant
	// FIXME: FramesDocument that is destroyed, for instance a
	// FIXME: document in history or in a hidden iframe used by a site
	// FIXME: to load data in the background.
	GetWindow()->SetElementExpander(NULL);
#endif // NEARBY_ELEMENT_DETECTION

#ifdef MEDIA_SUPPORT
	SuspendAllMedia(TRUE);
#endif // MEDIA_SUPPORT

	if (es_generated_document)
		es_generated_document->es_replaced_document = NULL;
	es_generated_document = NULL;
	if (es_replaced_document)
		es_replaced_document->es_generated_document = NULL;
	es_replaced_document = NULL;

	OP_DELETE(document_state);
	document_state = NULL;

	is_being_deleted = TRUE;

#ifdef DOCUMENT_EDIT_SUPPORT
	OP_DELETE(document_edit);
	document_edit = NULL;
#endif //DOCUMENT_EDIT_SUPPORT

	if (onload_checker)
	{
		onload_checker->DocumentDestroyed();
		onload_checker = NULL;
	}

#ifdef _PRINT_SUPPORT_
	OP_DELETE(print_time);
#endif

	OpStatus::Ignore(SetAsCurrentDoc(FALSE));

#ifdef SECMAN_USERCONSENT
	g_secman_instance->OnDocumentDestroy(this);	// Must be after SetAsCurrentDoc which calls g_secman_instance->OnDocumentSuspend(this)
#endif // SECMAN_USERCONSENT

	SignalAllDelayedLayoutListeners();

#ifdef XML_EVENTS_SUPPORT
	while (XML_Events_Registration* xml_event = static_cast<XML_Events_Registration*>(xml_events_registrations.First()))
	{
		xml_event->MoveToOtherDocument(this, NULL);
		OP_ASSERT(xml_event != xml_events_registrations.First() || !"Should have been removed by MoveToOtherDocument");
	}
#endif // XML_EVENTS_SUPPORT

#ifdef _PLUGIN_SUPPORT_
	unsigned plugin_script_no = m_blocked_plugins.GetCount();
	while (plugin_script_no-- > 0)
		OpNS4PluginHandler::GetHandler()->Resume(this, m_blocked_plugins.Get(plugin_script_no), TRUE, TRUE);
	OP_ASSERT(m_blocked_plugins.GetCount() == 0);
#endif // _PLUGIN_SUPPORT_

	RemoveAllLoadInlineElms();

	pages.Clear();

	if (dom_environment)
		dom_environment->BeforeDestroy();

	OP_DELETE(text_selection);
	text_selection = NULL;

	if (own_logdoc)
	{
		OP_DELETE(old_logdoc);
		old_logdoc = NULL;
		OP_DELETE(logdoc);
		logdoc = NULL;
	}
#ifdef _PRINT_SUPPORT_
	else
		if (logdoc)
			logdoc->DeletePrintCopy();
#endif // _PRINT_SUPPORT_

#ifdef ON_DEMAND_PLUGIN
	OP_ASSERT(m_ondemand_placeholders.GetCount() == 0);
#endif // ON_DEMAND_PLUGIN

	CleanESEnvironment(TRUE);

	// temporary fix, will be fixed when HTML_Document and FramesDocument are merged. (geir, 2002-06-03)
	if (doc)
	{
		doc->Free();
		OP_DELETE(doc);
		doc = NULL;
	}

	Clean();

	UnsetAllCallbacks(TRUE);

#ifndef RAMCACHE_ONLY
	url.FreeUnusedResources();
#endif

	if (has_blink)
		g_windowManager->RemoveDocumentWithBlink();

#if defined DOC_HAS_PAGE_INFO && defined _SECURE_INFO_SUPPORT
	m_security_url_used.UnsetURL();
#endif

#ifdef VISITED_PAGES_SEARCH
	CloseVisitedSearchHandle();
#endif

	for (UINT32 i = 0; i < m_document_contexts_in_use.GetCount(); i++)
	{
		m_document_contexts_in_use.Get(i)->HandleFramesDocumentDeleted();
	}

#ifdef WEBFEEDS_DISPLAY_SUPPORT
	if ((is_inline_feed || feed_listener_added) && g_webfeeds_api)
		OpStatus::Ignore(g_webfeeds_api->RemoveListener(this));
#endif // WEBFEEDS_DISPLAY_SUPPORT

	if (current_form_request)
	{
#ifdef GADGET_SUPPORT
		if (current_form_request->IsWaitingForInitialSecurityCheck())
			OP_DELETE(current_form_request);
		else
#endif // GADGET_SUPPORT
		{
			current_form_request->Cancel();
		}
		current_form_request = NULL;
	}

#ifdef WAND_SUPPORT
	if (wand_inserter)
	{
		wand_inserter->Remove();
		OP_DELETE(wand_inserter);
	}
#endif // WAND_SUPPORT

	if (origin)
	{
		origin->DecRef();
		origin = NULL;
	}

#ifdef _SPAT_NAV_SUPPORT_
	Window* win = GetWindow();
	if (win && win->GetSnHandler())
	{
		win->GetSnHandler()->DocDestructed ( this );
	}
#endif // _SPAT_NAV_SUPPORT_

	OP_ASSERT(!onload_checker); // Deleted and set to NULL above.

	// This doesn't mean that we're displaying the document, but that it should be removed
	// from the list of "history documents" that the mem manager keeps.
	g_memory_manager->DisplayedDoc(this);

#if defined KEYBOARD_SELECTION_SUPPORT || defined DOCUMENT_EDIT_SUPPORT
	OP_DELETE(caret_painter);
#endif // KEYBOARD_SELECTION_SUPPORT || DOCUMENT_EDIT_SUPPORT
}

HTML_Element *FramesDocument::GetFocusElement(  )
{
	HTML_Element *res = NULL;
	if( doc )
	{
		// Why so many different focus elements, one wonders?
		res = doc->GetFocusedElement();
		if (!res)
			res = doc->GetNavigationElement();
	}
	return res;
}

BOOL FramesDocument::ElementHasFocus( HTML_Element *he )
{
	if( !doc )
		return FALSE;
	return ((he == doc->GetFocusedElement()) ||
			(he == doc->GetNavigationElement() &&
			 !he->IsPreFocused()));
}

#ifdef SAVE_DOCUMENT_AS_TEXT_SUPPORT

OP_STATUS FramesDocument::SaveCurrentDocAsText(UnicodeOutputStream* stream, const uni_char* fname, const char *force_encoding)
{
	if (doc)
	{
		return doc->SaveDocAsText(stream, fname, force_encoding);
	}

	if (active_frm_doc)
	{
		return active_frm_doc->GetCurrentDoc()->SaveCurrentDocAsText(stream, fname, force_encoding);
	}

	return OpStatus::ERR;
}

#endif // SAVE_DOCUMENT_AS_TEXT_SUPPORT

#ifdef _PRINT_SUPPORT_
OP_BOOLEAN FramesDocument::CreatePrintLayout(PrintDevice* pd, FramesDocument* fdoc, int vd_width, int vd_height, BOOL page_format, BOOL preview)
{
	OP_BOOLEAN ok = OpBoolean::IS_FALSE;
	logdoc = fdoc->GetLogicalDocument();
	own_logdoc = FALSE;
	packed.show_images = GetWindow()->ShowImages();
#ifdef FLEXIBLE_IMAGE_POLICY
	// Allow top level images regardless
	packed.show_images = packed.show_images || (url.IsImage() && IsTopDocument());
#endif // FLEXIBLE_IMAGE_POLICY

	if (g_pcprint->GetIntegerPref(PrefsCollectionPrint::FitToWidthPrint))
	{
		era_mode = TRUE;
		CheckERA_LayoutMode();
	}

	if (logdoc)
	{
		int layout_view_width = GetLayoutViewWidth();

		if (GetSubWinId() == -1)
			pd->Reset();

		switch (logdoc->GetHtmlDocType())
		{
		case HTML_DOC_FRAMES:
			{
				OP_ASSERT(logdoc->GetPrintRoot() == NULL);
				OP_ASSERT(!doc);
				OP_ASSERT(IsPrintDocument());
				OP_ASSERT(!ifrm_root);

				logdoc->CreatePrintCopy();

				HTML_Element* he_frmset = NULL;
				if (HTML_Element* root = logdoc->GetPrintRoot())
				{
					he_frmset = root->GetFirstElm(HE_FRAMESET);
				}

				if (!he_frmset || !logdoc->GetHLDocProfile()->IsElmLoaded(he_frmset))
					return OpBoolean::IS_FALSE;

				FramesDocElm* cfdoc_root = fdoc->GetFrmDocRoot();

				if (page_format)
				{
					frm_root = OP_NEW(FramesDocElm, (cfdoc_root->GetSubWinId(),
												0,
												0,
												vd_width,
												vd_height,
												this,
												he_frmset,
												pd,
												FRAMESET_ABSOLUTE_SIZED,
												layout_view_width,
												FALSE,
												NULL));

					if (!frm_root)
						return OpStatus::ERR_NO_MEMORY;

					if (OpStatus::IsError(frm_root->Init(he_frmset, pd, NULL)))
					{
						OP_DELETE(frm_root);
						frm_root = NULL;

						return OpStatus::ERR_NO_MEMORY;
					}

					FramesDocElm* fdoc_root = fdoc->GetFrmDocRoot();

					if (!fdoc_root)
						return OpBoolean::IS_FALSE;

					RETURN_IF_ERROR(fdoc_root->CreatePrintLayoutAllPages(pd, frm_root));
				}
				else
				{
					int left;
					int right;
					int top;
					int bottom;

					pd->GetUserMargins(left, right, top, bottom);

					frm_root = OP_NEW(FramesDocElm, (cfdoc_root->GetSubWinId(),
												0,
												0,
												vd_width - left - right,
												vd_height - top - bottom,
												this,
												he_frmset,
												pd,
												FRAMESET_ABSOLUTE_SIZED,
												layout_view_width,
												FALSE,
												NULL));

					if (!frm_root)
						return OpStatus::ERR_NO_MEMORY;

					if (OpStatus::IsError(frm_root->Init(he_frmset, pd, NULL)))
					{
						OP_DELETE(frm_root);
						frm_root = NULL;

						return OpStatus::ERR_NO_MEMORY;
					}

					if (GetSubWinId() < 0)
						sub_win_id_counter = 1;

					RETURN_IF_ERROR(cfdoc_root->CopyFrames(frm_root));

					RETURN_IF_ERROR(frm_root->FormatFrames());
					for (DocumentTreeIterator it(frm_root); it.Next();)
						if (FramesDocElm* fde = it.GetFramesDocElm())
							fde->UpdateGeometry();
				}

				ok = OpBoolean::IS_TRUE;

				break;
			}

		case HTML_DOC_PLAIN:
			{
				OP_ASSERT(logdoc->GetPrintRoot() == NULL);
				OP_ASSERT(!doc);
				OP_ASSERT(IsPrintDocument());
				OP_ASSERT(!ifrm_root);

				doc = OP_NEW(HTML_Document, (this, GetDocManager()));

				if (doc)
					ok = OpBoolean::IS_TRUE;
				else
				{
					ok = OpStatus::ERR_NO_MEMORY;
					break;
				}

				logdoc->CreatePrintCopy();

				logdoc->GetLayoutWorkplace()->SetFramesDocument(this);

				FramesDocElm* cifrm_root = fdoc->ifrm_root;
				if (cifrm_root)
				{
					OP_ASSERT(!ifrm_root);
					if (page_format)
					{
						ifrm_root = OP_NEW(FramesDocElm, (cifrm_root->GetSubWinId(),
													0,
													0,
													vd_width,
													vd_height,
													this,
													NULL,
													pd,
													FRAMESET_ABSOLUTE_SIZED,
													layout_view_width,
													TRUE,
													NULL));

						if (!ifrm_root)
						{
							ok = OpStatus::ERR_NO_MEMORY;
							break;
						}

						if (OpStatus::IsError(ifrm_root->Init(NULL, pd, NULL)))
						{
							OP_DELETE(ifrm_root);
							ifrm_root = NULL;

							ok = OpStatus::ERR_NO_MEMORY;
							break;
						}

						FramesDocElm* ifdoc_root = fdoc->GetIFrmRoot();

						if (!ifdoc_root)
						{
							ok = OpBoolean::IS_FALSE;
							break;
						}

						if (OpStatus::IsError(ifdoc_root->CreatePrintLayoutAllPages(pd, ifrm_root)))
						{
							ok = OpStatus::ERR_NO_MEMORY;
						}
					}
					else
					{
						int left;
						int right;
						int top;
						int bottom;

						pd->GetUserMargins(left, right, top, bottom);

						ifrm_root = OP_NEW(FramesDocElm, (cifrm_root->GetSubWinId(),
													0,
													0,
													vd_width - left - right,
													vd_height - top - bottom,
													this,
													NULL,
													pd,
													FRAMESET_ABSOLUTE_SIZED,
													layout_view_width,
													TRUE,
													NULL));

						if (!ifrm_root)
						{
							ok = OpStatus::ERR_NO_MEMORY;
							break;
						}

						if (OpStatus::IsError(ifrm_root->Init(NULL, pd, NULL)))
						{
							OP_DELETE(ifrm_root);
							ifrm_root = NULL;
							ok = OpStatus::ERR_NO_MEMORY;
							break;
						}

						if (GetSubWinId() < 0)
							sub_win_id_counter = 1;

						OP_STATUS status = cifrm_root->CopyFrames(ifrm_root);
						if (OpStatus::IsError(status))
						{
							ok = status;
							break;
						}
					}
				}
				if (Reflow(TRUE, TRUE) == OpStatus::ERR_NO_MEMORY)
					ok = OpStatus::ERR_NO_MEMORY;
				break;
			}
		}

		if (OpStatus::IsError(ok) &&
			logdoc->GetHtmlDocType() == HTML_DOC_PLAIN)
		{
			// Restore the old state
			logdoc->GetLayoutWorkplace()->SetFramesDocument(fdoc);
		}
	}

	return ok;
}
#endif // _PRINT_SUPPORT_


#ifdef SHORTCUT_ICON_SUPPORT

URL* FramesDocument::Icon()
{
	return &favicon;
}

OP_BOOLEAN FramesDocument::LoadIcon(URL* icon_url, HTML_Element *link_he)
{
	if (GetWindow()->GetPrivacyMode())
		return OpBoolean::IS_FALSE;

	// only set the favicon for the top-most document
	if (IsTopDocument() && g_pcdoc->GetIntegerPref(PrefsCollectionDoc::AlwaysLoadFavIcon, GetServerNameFromURL(url)) != 0)
	{
		if (GetWindow()->GetOnlineMode() != Window::OFFLINE
		    && icon_url && icon_url->IsValid() && (icon_url->Type()== URL_DATA || icon_url->Type() == url.Type())
		    && (!(*icon_url == favicon) || favicon.Status(TRUE) == URL_UNLOADED))
		{
			HEListElm * default_favicon = GetDocRoot()->GetHEListElmForInline(ICON_INLINE);
			if (default_favicon != NULL)
			{
				// If there is a favicon associated with the doc root, then a speculative
				// favicon load is being done, hence this cannot be called again for the root.
				OP_ASSERT(link_he != GetDocRoot());

				// We got a new icon while we had asked already for the default /favicon.ico
				// so now we abort that first default icon request.
				if (favicon == *default_favicon->GetLoadInlineElm()->GetUrl())
					favicon = URL();
				OP_DELETE(default_favicon);
			}

			if (favicon.IsEmpty())
				// If only one favicon exists, it's set here and we're done.
				// If multiple icons exist, the last that succeeds is set when
				// the inline finishes loading. This way 'favicon' is not set to
				// an url that might fail when there was another that succeeded,
				// and this heuristic works well if the favicon changes over time.
				favicon = *icon_url;

			icon_url->SetAttribute(URL::KSpecialRedirectRestriction, TRUE);

			LoadInlineOptions options;
			options.LoadSilent(TRUE);
			options.DelayLoad(FALSE);

			OP_LOAD_INLINE_STATUS lis = LoadInline(icon_url, link_he, ICON_INLINE, options);
			if (OpStatus::IsMemoryError(lis))
				return lis;
			else if (lis == LoadInlineStatus::LOADING_STARTED)
				return OpBoolean::IS_TRUE;
		}
	}

	return OpBoolean::IS_FALSE;
}

void FramesDocument::FaviconTimedOut(URL_ID url_id)
{
	Head* list;

	if (OpStatus::IsSuccess(inline_hash.GetData(url_id, &list)))
	{
		LoadInlineElm* next_lie;

		for (LoadInlineElm *lie = (LoadInlineElm*)list->First(); lie; lie = next_lie)
		{
			next_lie = (LoadInlineElm*)lie->Suc();
			HEListElm* next_helm;

			for (HEListElm* helm = lie->GetFirstHEListElm(); helm; helm = next_helm)
			{
				next_helm = helm->Suc();

				if (helm->GetInlineType() == ICON_INLINE)
					StopLoadingInline(lie->GetUrl(), helm->HElm(), ICON_INLINE);
			}
		}
	}
}

#endif // SHORTCUT_ICON_SUPPORT

OP_STATUS FramesDocument::CheckFinishDocument()
{
	OP_STATUS status = OpStatus::OK;

	// For a local document to be "finished" it has to have loaded
	// all inlines, it has to have parsed the whole main
	// document. We also need to have run a full Reflow over
	// the full document since that might pick up inlines
	// that need to be loaded (for
	// instance background images specified by CSS)
	if (!IsLocalDocFinished() && // Not already done
	    logdoc && logdoc->IsLoaded() && // Main document parsed and reflowed
	    InlineImagesLoaded() && // All inlines loaded
	    !about_blank_waiting_for_javascript_url)
	{
		if (delay_doc_finish)
		{
			if (!posted_delay_doc_finish)
			{
				MessageHandler *mh = GetMessageHandler();
				if (!mh->HasCallBack(this, MSG_CHECK_FINISH_DOCUMENT, 0))
					RETURN_IF_ERROR(mh->SetCallBack(this, MSG_CHECK_FINISH_DOCUMENT, 0));

				posted_delay_doc_finish = TRUE;
				mh->PostMessage(MSG_CHECK_FINISH_DOCUMENT, 0, 0);
			}
			/* Will be back later. */
			return OpStatus::OK;
		} else if (posted_delay_doc_finish)
			posted_delay_doc_finish = FALSE;

		// First time this is called it might start loading of
		// the favicon and thus make it not finished. In that case
		// the IsLocalDocFinished() flag will not be set and we will
		// do a new attempt later.
		status = FinishLocalDoc();
	}

	if (!IsDocTreeFinished() && // Not already done
	    IsLoaded() &&
	    !about_blank_waiting_for_javascript_url)
	{
		OP_STATUS status2 = FinishDocTree();
		if (OpStatus::IsMemoryError(status2))
			status = status2;
	}

	return status;
}

#ifdef SHORTCUT_ICON_SUPPORT
OP_BOOLEAN FramesDocument::DoASpeculativeFavIconLoad()
{
	if (!IsPrintDocument() &&
	    !GetDocManager()->ErrorPageShown() && // don't do it if the main document failed to load
	    !loading_aborted && // don't do it if we cancelled the loading
	    favicon.IsEmpty() && // only do this for the top frame
	    IsTopDocument() && // only do this for the top frame
	    (url.Type() == URL_HTTP || url.Type() == URL_HTTPS) && // && hldoc-> doesn't have link_icon --> or check inlinelist.
	    g_pcdoc->GetIntegerPref(PrefsCollectionDoc::AlwaysLoadFavIcon, GetServerNameFromURL(url)) == 1 &&
	    !GetDocManager()->ErrorPageShown()
#ifdef SELFTEST
	    && !url.GetAttribute(URL::KIsGeneratedBySelfTests)
#endif // SELFTEST
	   )
	{
		// force loading of url base + "favicon.ico" as fav icon
		URL icon_url = urlManager->GetURL(url, "/favicon.ico");
		if (icon_url.IsEmpty())
			return OpStatus::ERR_NO_MEMORY;

		if (icon_url.Status(TRUE) == URL_LOADED)
		{
			// CORE-620 Don't reissue the request if it
			// failed previously for this session.
			unsigned status = icon_url.GetAttribute(URL::KHTTP_Response_Code, TRUE);
			if (400 <= status && status < 500)
				return OpStatus::OK;
		}

		RETURN_IF_MEMORY_ERROR(LoadIcon(&icon_url, GetDocRoot()));
	}
	return OpStatus::OK;
}
#endif // SHORTCUT_ICON_SUPPORT


OP_STATUS FramesDocument::FinishLocalDoc()
{
	if (!local_doc_finished)
	{
#ifdef SHORTCUT_ICON_SUPPORT
		DoASpeculativeFavIconLoad();
#endif // SHORTCUT_ICON_SUPPORT

#ifdef _WML_SUPPORT_
		if (WML_Context *wc = GetDocManager()->WMLGetContext())
		{
			if (wc->IsParsing())
			{
				RETURN_IF_MEMORY_ERROR(wc->PostParse());
				// check if the postparse triggered something that
				// makes us not finished after all
				if (!logdoc->GetLayoutWorkplace()->IsTraversable())
					return OpStatus::OK;
			}
		}
#endif // _WML_SUPPORT_

		local_doc_finished = document_finished_at_least_once = TRUE;

		if (!dom_environment)
		{
			if (document_state)
			{
				MessageHandler* mh = GetMessageHandler();

				if (!mh->HasCallBack(this, MSG_RESTORE_DOCUMENT_STATE, 0))
					mh->SetCallBack(this, MSG_RESTORE_DOCUMENT_STATE, 0);
				mh->PostMessage(MSG_RESTORE_DOCUMENT_STATE, 0, 0);
			}
		}

		if (doc)
			RETURN_IF_MEMORY_ERROR(doc->HandleLoadingFinished());
		else
			RETURN_IF_MEMORY_ERROR(GetDocManager()->HandleDocFinished());

		if (StoredViewportCanBeRestored())
			GetDocManager()->RestoreViewport(TRUE, FALSE, TRUE);
	}

	return OpStatus::OK;
}

OP_STATUS FramesDocument::FinishDocTree()
{
	OP_NEW_DBG("FramesDocument::FinishDocTree", "doc");
	OP_DBG((UNI_L("this=%p"), this));
	if (!doc_tree_finished)
	{
		doc_tree_finished = TRUE;

		FramesDocElm* frame = GetDocManager()->GetFrame();
		if (frame)
		{
#ifdef SVG_SUPPORT
			if (frame->IsSVGResourceDocument() ||
			    (url.ContentType() == URL_SVG_CONTENT && frame->GetHtmlElement() && frame->GetHtmlElement()->IsMatchingType(HE_IFRAME, NS_HTML) &&
			    frame->GetHtmlElement()->GetInserted() == HE_INSERTED_BY_SVG))
			{
				UrlImageContentProvider* provider = UrlImageContentProvider::FindImageContentProvider(url);
				if (provider)
				{
					if (logdoc && logdoc->IsXml() && logdoc->IsXmlParsingFailed())
						provider->OnSVGImageLoadError();
					else
						if (provider->OnSVGImageLoad())
							es_info.onload_called = TRUE;
				}
			}

			HTML_Element *frm_elm;
			if (frame->IsSVGResourceDocument() &&
				(frm_elm = frame->GetHtmlElement()) &&
				frm_elm->GetNsType() == NS_SVG)
			{
				g_svg_manager->HandleInlineChanged(GetParentDoc(), frm_elm);
			}
			else
#endif // SVG_SUPPORT
			if (logdoc)
				logdoc->GetLayoutWorkplace()->HandleContentSizedIFrame();
#ifdef SVG_SUPPORT
			HTML_Element *frame_element = frame->GetHtmlElement();
			if (frame_element &&
				frame_element->IsMatchingType(HE_IFRAME, NS_HTML) &&
				frame_element->GetInserted() == HE_INSERTED_BY_SVG)
			{
				URL iframe_url = frame->GetCurrentDoc()->GetURL();
				UrlImageContentProvider* provider = UrlImageContentProvider::FindImageContentProvider(iframe_url);
				if(provider && provider->GetSVGImageRef())
				{
					provider->SVGFinishedLoading();

					// Update svg favicons
					if (iframe_url == GetWindow()->GetWindowIconURL())
					{
						GetWindow()->SetWindowIcon(&iframe_url);
					}
				}
			}
#endif // SVG_SUPPORT
		}

		RETURN_IF_MEMORY_ERROR(CheckOnLoad(this));

		// Call FinishDocumentAfterOnLoad immediately if we're not waiting for
		// any onload to finish
		if (!es_info.onload_pending)
			RETURN_IF_ERROR(FinishDocumentAfterOnLoad());

		if (GetParentDoc())
			return GetParentDoc()->CheckFinishDocument();
	}

	return OpStatus::OK;
}

#ifdef APPLICATION_CACHE_SUPPORT
OP_STATUS FramesDocument::UpdateAppCacheManifest()
{
	if (logdoc)
	{
		URL* manifest_url = logdoc->GetManifestUrl();

		// need to associate with a cache if we have a manifest attribute or it was loaded from an appcache context
		if (manifest_url || g_application_cache_manager->GetCacheFromContextId(GetURL().GetContextId()))
		{
			// Make sure we have a DOM Environment.  AppCache doesn't strictly need
			// the environment itself, but it's the only common cache host we have
			// with web workers.  And the number of sites using appcache, but
			// not javascript is expected to be small.
			if (!dom_environment)
				RETURN_IF_ERROR(ConstructDOMEnvironment());

			if (dom_environment)
				RETURN_IF_ERROR(g_application_cache_manager->HandleCacheManifest(dom_environment, manifest_url ? *manifest_url : URL(), GetURL()));
		}
	}

	return OpStatus::OK;
}
#endif // APPLICATION_CACHE_SUPPORT

OP_STATUS FramesDocument::FinishDocumentAfterOnLoad()
{
	RETURN_IF_ERROR(RestoreState());
	CheckRefresh();
#ifdef SCOPE_SUPPORT
	OpScopeReadyStateListener::OnReadyStateChanged(this, OpScopeReadyStateListener::READY_STATE_AFTER_ONLOAD);
#endif // SCOPE_SUPPORT
#ifdef APPLICATION_CACHE_SUPPORT
	RETURN_IF_ERROR(UpdateAppCacheManifest());
#endif // APPLICATION_CACHE_SUPPORT

	return OpStatus::OK;
}

void
FramesDocument::NotifyUrlChanged(URL new_url)
{
	// Only tell the UI about changes to the top documen URL.
	if (GetTopDocument() == this)
	{
		OP_ASSERT(url == new_url || !"Someone saying that we're using an url that we are not using. From here on things will break because of that inconsistancy");
		uni_char *tempname = Window::ComposeLinkInformation(new_url.GetAttribute(URL::KUniName_Username_Password_Hidden, URL::KNoRedirect).CStr(), new_url.UniRelName());
		WindowCommander *win_com = GetWindow()->GetWindowCommander();

		if (new_url.GetAttribute(URL::KHeaderLoaded, TRUE))
			GetWindow()->SetSecurityState(new_url.GetAttribute(URL::KSecurityStatus), FALSE, NULL);

		win_com->GetLoadingListener()->OnUrlChanged(win_com, tempname);
		OP_DELETEA(tempname);
	}
}

static const OpMessage g_doc_msg_types[] = { MSG_URL_DATA_LOADED, MSG_URL_LOADING_FAILED, MSG_HEADER_LOADED, MSG_NOT_MODIFIED, MSG_MULTIPART_RELOAD, MSG_HANDLE_INLINE_DATA_ASYNC };

void
FramesDocument::UnsetLoadingCallbacks(URL_ID url_id)
{
	MessageHandler* mh = GetMessageHandler();
	int num_msg = sizeof(g_doc_msg_types) / sizeof(OpMessage);
	for (int i = 0; i < num_msg; i++)
		mh->UnsetCallBack(this, g_doc_msg_types[i], (MH_PARAM_1) url_id);
}

BOOL
FramesDocument::SetLoadingCallbacks(URL_ID url_id)
{
	OP_ASSERT(IsCurrentDoc());

	MessageHandler* mh = GetMessageHandler();
	int num_msg = sizeof(g_doc_msg_types) / sizeof(OpMessage);
	for (int i = 0; i < num_msg; i++)
	{
		if (!mh->HasCallBack(this, g_doc_msg_types[i], (MH_PARAM_1) url_id))
			if (mh->SetCallBack(this, g_doc_msg_types[i], (MH_PARAM_1) url_id) == OpStatus::ERR_NO_MEMORY)
			{
				UnsetLoadingCallbacks(url_id);
				return FALSE;
			}
	}

	return TRUE;
}

void FramesDocument::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 lpar2)
{
	if (msg != MSG_DOC_DELAYED_ACTION)
	{
		if (FramesDocElm *frame = GetDocManager()->GetFrame())
			if (frame->IsDeleted())
				return;

#ifdef _PRINT_SUPPORT_
		// Maintain same behavior as before print documents were set
		// as Current Document. This is needed because some events
		// reach print documents and interfere with printing.
		if (IsPrintDocument())
			return;
#endif // _PRINT_SUPPORT_

		OP_ASSERT(IsCurrentDoc()); // Wasn't UnsetAllCallbacks called? This is triggered when doing print preview on a page with iframes so there is a bug there somewhere.
		if (!IsCurrentDoc())
			return;
	}

	BOOL raise_OOM = FALSE;

	switch (msg)
	{
	case MSG_URL_MOVED:
		{
			// This code block modifying the security of the main document is not really
			// needed, except for when OpenURL is called with
			// create_doc_now=TRUE (a setting we are trying to remove). In that case we may
			// create a document that we later have to patch up.
			if (loaded_from_url.IsEmpty() && url.Id(TRUE) == (URL_ID) lpar2)
			{
				URL moved_to = url.GetAttribute(URL::KMovedToURL, TRUE);

#ifdef CORS_SUPPORT
				Head *cors_list;
				OpCrossOrigin_Request *cors_request = NULL;
				if (OpStatus::IsSuccess(inline_hash.GetData((URL_ID)par1, &cors_list)))
				{
					LoadInlineElm *lie = static_cast<LoadInlineElm *>(cors_list->First());
					while (lie)
					{
						if ((cors_request = lie->GetCrossOriginRequest()) != NULL)
							break;
						lie = static_cast<LoadInlineElm *>(lie->Suc());
					}
				}
				OpSecurityContext source(url, cors_request);
#else
				OpSecurityContext source(url);
#endif // CORS_SUPPORT
				OpSecurityContext target(moved_to);
				BOOL allowed = FALSE;

				/* If we're making an "incompatible" change in the document's
				   security context, destroy the current scripting environment
				   to avoid leakage/code injection. */
				if (OpStatus::IsError(OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_STANDARD, source, target, allowed)) || !allowed)
					CleanESEnvironment(TRUE);

				raise_OOM = OpStatus::IsMemoryError(SetNewUrl(moved_to));
			}

			Head* list;
			BOOL has_inlines = FALSE;
			if (OpStatus::IsSuccess(inline_hash.GetData((URL_ID)par1, &list)))
			{
				LoadInlineElm* lie, *next_lie = static_cast<LoadInlineElm*>(list->First());
				while (next_lie)
				{
					lie = next_lie;
					next_lie = static_cast<LoadInlineElm*>(lie->Suc());

					lie->UrlMoved(this, (URL_ID)lpar2);

					URL *img_url = lie->GetUrl();

					if (!img_url->IsEmpty())
						UrlImageContentProvider::UrlMoved(*img_url);

					if (!lie->GetAccessForbidden())
						lie->CheckAccessForbidden();
					if (lie->GetAccessForbidden())
						StopLoadingInline(lie);
				}
				has_inlines = !list->Empty();
			}

			// Make sure we can still find the element in the inline_hash
			OP_STATUS status = inline_hash.UrlMoved((URL_ID)par1, (URL_ID)lpar2);
			if (OpStatus::IsMemoryError(status))
				raise_OOM = TRUE;
			else
				OP_ASSERT(status == OpStatus::OK);

			if (has_inlines && !SetLoadingCallbacks(lpar2))
				raise_OOM = TRUE;

			break;
		}

	case MSG_MULTIPART_RELOAD:
		UrlImageContentProvider::ResetAndRestartImageFromID(par1);
		msg = MSG_URL_DATA_LOADED;
		/* fall through */

	case MSG_URL_DATA_LOADED:
	case MSG_URL_LOADING_FAILED:
	case MSG_HEADER_LOADED:
	case MSG_NOT_MODIFIED:
	case MSG_HANDLE_INLINE_DATA_ASYNC:
		{
			URLStatus inline_url_stat = URL_LOADING;

			// when calling LocalLoadInline from
			// HandleInlineDataLoaded on an already loaded resource
			// MSG_HANDLE_INLINE_DATA_ASYNC will be posted. this
			// message is lost if UnsetLoadingCallbacks is called.
			BOOL unset_loading_callbacks = TRUE;

			// Need to check whole list because of possible redirects to the same target url

			Head* list;
			if (OpStatus::IsSuccess(inline_hash.GetData((URL_ID) par1, &list)))
			{
				for (LoadInlineElm *lie = (LoadInlineElm*)list->First(), *next; lie; lie = next)
				{
					OP_ASSERT(inline_hash.Contains(lie)); // If this happens, lie->GetUrl()->Id(TRUE) has changed without a MSG_URL_MOVED!

					if (msg == MSG_HANDLE_INLINE_DATA_ASYNC)
						lie->SetSentHandleInlineDataAsyncMsg(FALSE);

					BOOL seems_to_be_stray_message =
						msg != MSG_HANDLE_INLINE_DATA_ASYNC &&
						msg != MSG_URL_LOADING_DELAYED &&
						msg != MSG_URL_LOADING_FAILED &&
						!lie->GetUrl()->GetAttribute(URL::KHeaderLoaded);
					if (seems_to_be_stray_message)
					{
						if (lie->GetUrl()->Type() == URL_DATA)
							seems_to_be_stray_message = FALSE; // These forget to set the header loaded flag
						else if (lie->GetUrl()->Status(TRUE) == URL_LOADED
							|| lie->GetUrl()->Status(TRUE) == URL_LOADING_FAILURE)
							seems_to_be_stray_message = FALSE; // Some quickly loaded urls forget to set the flag
					}

					if (seems_to_be_stray_message)
					{
						// This wasn't directed at this inline. Can happen for instance
						// if the same url is loaded both as main document and later as
						// inline and we have non-processed document loading messages in
						// the message queue when we start loading the inline.
						// During normal document load, it will be true that *lie->GetUrl() == GetURL()
						// but note that during reload this invariant no longer holds.
						next = static_cast<LoadInlineElm*>(lie->Suc());
						continue;
					}

					if (lie->GetImageStorageId() == lie->GetUrl()->GetAttribute(URL::KStorageId) || msg == MSG_URL_LOADING_FAILED)
						// image url has not changed so it shouldn't be reloaded
						lie->UnsetNeedResetImage();

					if (msg == MSG_URL_LOADING_FAILED)
						// If the URL failed to load due to cross network checks
						// this call to CheckAccessForbidden() verifies that and sets the
						// proper flag in the LoadInlineElm.
						lie->CheckAccessForbidden();

#ifdef CORS_SUPPORT
					if (msg == MSG_HEADER_LOADED && lie && lie->GetCrossOriginRequest())
					{
						BOOL allowed = FALSE;
						OP_STATUS status = lie->CheckCrossOriginAccess(allowed);
						if (OpStatus::IsMemoryError(status))
							raise_OOM = TRUE;
						else if (OpStatus::IsError(status))
							allowed = FALSE;
						if (!allowed)
						{
							/* Not allowed, communicate non-loading via an error event. */
							for (HEListElm* hle = lie->GetFirstHEListElm(); hle; hle = hle->Suc())
								hle->OnError(OpStatus::ERR);

							next = static_cast<LoadInlineElm*>(lie->Suc());
							SetInlineLoading(lie, FALSE);
							continue;
						}
					}
#endif // CORS_SUPPORT

					if (!HandleInlineDataLoaded(lie))
						raise_OOM = TRUE;

					next = (LoadInlineElm*)lie->Suc();

					inline_url_stat = lie->GetUrl()->Status(TRUE);

					// If inline_url_stat != URL_LOADING, we will stop listening for messages and
					// any queued MSG_HANDLE_INLINE_DATA_ASYNC will be silently ignored.
					if (inline_url_stat != URL_LOADING)
						lie->SetSentHandleInlineDataAsyncMsg(FALSE);
					else if (unset_loading_callbacks && lie->HasSentHandleInlineDataAsyncMsg())
						unset_loading_callbacks = FALSE;

					if (lie->IsUnused())
					{
						RemoveLoadInlineElm(lie);
						OP_DELETE(lie);
					}
					else if (inline_url_stat != URL_LOADING)
					{
						UpdateInlineInfo(lie);
					}
				}
			}

			// if last inline element just loaded, and doc is parsed, run reflow:
			if (OpStatus::IsMemoryError(ReflowAndFinishIfLastInlineLoaded()))
				raise_OOM = TRUE;

			if (inline_url_stat != URL_LOADING)
			{
				if (unset_loading_callbacks)
					UnsetLoadingCallbacks(par1);
				SendImageLoadingInfo();
#ifdef SHORTCUT_ICON_SUPPORT
				AbortFaviconIfLastInline();
#endif // SHORTCUT_ICON_SUPPORT

				// If the document is generated by a script after the initial
				// loading has finished, the new document loading will not finish
				// normally, so we need to turn the progress bar off "manually".
				// EndProgressDisplay() will make sure the entire document tree
				// is finished before turning it off.
				if (!NeedsProgressBar())
					GetTopDocument()->GetDocManager()->EndProgressDisplay(FALSE);
			}
		}
		break;


	case MSG_DOC_DELAYED_ACTION:
		GetMessageHandler()->UnsetCallBack(this, MSG_DOC_DELAYED_ACTION, 0);
		GetDocManager()->OnDelayedActionMessageReceived();
		if (!DeleteAllIFrames())
		{
			// We need to wait for nested message loops to unwind.
			// Wait one second.
			PostDelayedActionMessage(1000);
		}

#ifndef MOUSELESS
		if (need_update_hover_element)
		{
			need_update_hover_element = FALSE;
			if (doc)
				doc->RecalculateHoverElement();
		}
#endif // MOUSELESS

		if (remove_from_cache)
		{
			if (!IsESActive(FALSE) && !IsESStopped() && !IsESGeneratingDocument())
			{
				remove_from_cache = FALSE;

				if (!IsCurrentDoc())
					Free(FALSE, FREE_VERY_IMPORTANT);
			}
		}
		break;

#ifdef XSLT_SUPPORT
	case MSG_PARSE_XSLT:
		if (logdoc)
			if (OpStatus::IsMemoryError(logdoc->ParseXSLTStylesheet()))
				raise_OOM = TRUE;
		break;
#endif // XSLT_SUPPORT

	case MSG_REPARSE_AS_HTML:
		if (OpStatus::IsMemoryError(ReparseAsHtml(TRUE)))
			raise_OOM = TRUE;
		break;

	case MSG_REINIT_FRAME:
		{
			FramesDocElm *frame = reinterpret_cast<FramesDocElm*>(par1);
			FrameReinitData *frd = frame->GetReinitData();

			if (frd->m_history_num > -1)
				frame->SetCurrentHistoryPos(frd->m_history_num, TRUE, FALSE);

			if (OpStatus::IsMemoryError(frame->SetAsCurrentDoc(TRUE, frd->m_visible)))
				raise_OOM = TRUE;

			if (frd->m_old_layout_mode != frame->GetLayoutMode() && frame->GetCurrentDoc())
			{
				FramesDocument* frames_doc = frame->GetCurrentDoc();
				frames_doc->CheckERA_LayoutMode();
			}

			GetMessageHandler()->UnsetCallBack(this, MSG_REINIT_FRAME, par1);

			frame->RemoveReinitData();
		}
		break;

#ifndef MOUSELESS
	case MSG_REPLAY_RECORDED_MOUSE_ACTIONS:
		if (doc)
			doc->ReplayRecordedMouseActions();
		break;
#endif // MOUSELESS

	case MSG_RESTORE_DOCUMENT_STATE:
		RestoreState();
		break;

	case MSG_CHECK_FINISH_DOCUMENT:
		if (posted_delay_doc_finish)
		{
			SetDelayDocumentFinish(FALSE);
			posted_delay_doc_finish = FALSE;
			CheckFinishDocument();
		}
		break;
	}

	if (raise_OOM)
	{
		GetHLDocProfile()->SetIsOutOfMemory(FALSE);

		if (GetWindow())
			GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		else
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	}
}

FramesDocElm* FramesDocument::GetFrmDocElm(int id)
{
	DocumentTreeIterator iter(this);
	iter.SetIncludeEmpty(); // Probably not needed but we used to check those
	while (iter.Next())
	{
		FramesDocElm* fdelm = iter.GetFramesDocElm();
		if (fdelm->GetSubWinId() == id)
			return fdelm;
	}
	return NULL;
}

void FramesDocument::Clean(BOOL remove_callbacks)
{
	if (GetLocked())
		return;

	FramesDocElm* tmp_ifrm_root = ifrm_root;
	FramesDocElm* tmp_root = frm_root;
	HTML_Document* tmp_doc = doc;

	// Need to reset pointers before deleting them; onunload event handlers
	// might want to access the document.

	if (remove_callbacks)
		UnsetAllCallbacks(FALSE);

	ifrm_root = NULL;
	frm_root = NULL;
	active_frm_doc = NULL;
	doc = NULL;

	OP_DELETE(tmp_root);
	OP_DELETE(tmp_doc);
	OP_DELETE(tmp_ifrm_root);

	OP_DELETE(url_data_desc);
	url_data_desc = NULL;

#ifdef SPECULATIVE_PARSER
	OP_DELETE(speculative_data_desc);
	speculative_data_desc = NULL;
#endif // SPECULATIVE_PARSER

#ifdef SHORTCUT_ICON_SUPPORT
	favicon = URL();
#endif // SHORTCUT_ICON_SUPPORT

	async_inline_elms.DeleteAll();
}

void FramesDocument::CleanESEnvironment(BOOL delete_es_runtime)
{
	if (delete_es_runtime)
	{
		DOM_Environment::Destroy(dom_environment);

		dom_environment = NULL;
		es_runtime = NULL;
		js_window = NULL;
		es_scheduler = NULL;
		es_async_interface = NULL;
	}
	else
	{
		if (GetESScheduler())
			GetESScheduler()->RemoveThreads();
	}
}

/* static */ void FramesDocument::SetAlwaysCreateDOMEnvironment(BOOL value)
{
	g_opera->doc_module.always_create_dom_environment = value;
}

OP_STATUS FramesDocument::CreateESEnvironment(BOOL delete_es_runtime, BOOL force)
{
	OP_PROBE3(OP_PROBE_FRAMESDOCUMENT_CREATEESENVIRONMENT);
	if (delete_es_runtime)
	{
		es_info.onload_called = 0;
		es_info.onload_ready = 0;
		es_info.onload_pending = 0;
		es_info.onpopstate_pending = 0;
	}

	/* Environment is created on demand in ConstructDOMEnvironment. */

	if (!dom_environment)
	{
		es_runtime = NULL;
		js_window = NULL;
		es_scheduler = NULL;
		es_async_interface = NULL;

		BOOL create_now = g_opera->doc_module.always_create_dom_environment;

#ifdef USER_JAVASCRIPT
		if (!create_now &&
		    g_pcjs->GetIntegerPref(PrefsCollectionJS::UserJSEnabled, GetHostName()) &&
		    g_pcjs->GetIntegerPref(PrefsCollectionJS::UserJSAlwaysLoad, GetHostName()))
		{
			create_now = TRUE;
		}
#endif // USER_JAVASCRIPT

#ifdef EXTENSION_SUPPORT
		if (!create_now && DOM_Utils::WillUseExtensions(this))
		{
			force = create_now = TRUE;
		}
#endif // EXTENSION_SUPPORT

		if (create_now)
			if (force || logdoc && logdoc->GetDocRoot())
				return ConstructDOMEnvironment();
	}

	return OpStatus::OK;
}

OP_STATUS FramesDocument::ConstructDOMEnvironment()
{
	BOOL enabled = GetWindow()->IsScriptableWindow();

#ifdef _PRINT_SUPPORT_
	if (IsPrintDocument())
		enabled = FALSE;
#endif // _PRINT_SUPPORT_

	if (!enabled || is_constructing_dom_environment)
		return OpStatus::ERR;
	else if (!dom_environment)
	{
		is_constructing_dom_environment = TRUE;

		// We might need close to half a megabyte of memory for this so
		// see if we should free some memory first to make it less likely
		// to fail because of oom.
		g_memory_manager->CheckDocMemorySize();

		OP_STATUS status = DOM_Environment::Create(dom_environment, this);

		is_constructing_dom_environment = FALSE;

		RETURN_IF_ERROR(status);

		es_runtime = dom_environment->GetRuntime();
		js_window = dom_environment->GetWindow();
		es_scheduler = dom_environment->GetScheduler();
		es_async_interface = dom_environment->GetAsyncInterface();

#ifdef SCOPE_SUPPORT
		OpScopeReadyStateListener::OnReadyStateChanged(this, OpScopeReadyStateListener::READY_STATE_DOM_ENVIRONMENT_CREATED);
#endif // SCOPE_SUPPORT

		GetDocManager()->UpdateCurrentJSWindow();
	}

	return OpStatus::OK;
}

DOM_Object *FramesDocument::GetDOMDocument()
{
	return dom_environment ? dom_environment->GetDocument() : NULL;
}

#ifdef ECMASCRIPT_DEBUGGER

OP_STATUS FramesDocument::GetESRuntimes(OpVector<ES_Runtime> &es_runtimes)
{
	if (GetDOMEnvironment())
		return GetDOMEnvironment()->GetESRuntimes(es_runtimes);
	return OpStatus::OK;
}

#endif // ECMASCRIPT_DEBUGGER

#ifdef DOC_HAS_PAGE_INFO
OP_BOOLEAN FramesDocument::GetPageInfo(URL& out_url)
{
# ifdef USE_ABOUT_FRAMEWORK
#  ifndef _SECURE_INFO_SUPPORT
	URL_InUse m_security_url_used;
#  endif
	OpPageInfo page_info(out_url, this, logdoc, &m_security_url_used);
	OP_STATUS rc = page_info.GenerateData();
# else
	TRAPD(rc, rc = GetPageInfoL(out_url));
# endif

	return OpStatus::IsError(rc) ? OpBoolean::IS_FALSE : OpBoolean::IS_TRUE;
}

#ifndef USE_ABOUT_FRAMEWORK
OP_BOOLEAN FramesDocument::GetPageInfoL(URL& out_url)
{
	if (!logdoc)
		return OpBoolean::IS_FALSE;

#ifdef _LOCALHOST_SUPPORT_
	OpString infostyleurl;
	ANCHOR(OpString, infostyleurl);
	g_pcfiles->GetFileURLL(PrefsCollectionFiles::StyleInfoPanelFile, &infostyleurl);
#endif

	RETURN_IF_ERROR(out_url.SetAttribute(URL::KMIME_ForceContentType, "text/html"));
	RETURN_IF_ERROR(out_url.SetAttribute(URL::KMIME_CharSet, "utf-16"));
	out_url.SetIsGeneratedByOpera();

	out_url.WriteDocumentData(UNI_L("<!DOCTYPE html>\n<html>\n<head>\n<title>Info</title>\n"));
#ifdef _LOCALHOST_SUPPORT_
	out_url.WriteDocumentDataUniSprintf(
		UNI_L("<link rel=\"stylesheet\" href=\"%s\" type=\"text/css\" media=\"screen,projection,tv,handheld,print\">\n"),
		infostyleurl.CStr());
#endif
	out_url.WriteDocumentData(UNI_L("</head>\n\n<body>\n"));

	OpString tmp_str;
	ANCHOR(OpString, tmp_str);
	tmp_str.ReserveL(128); // reserve a big buffer so that we don't have to grow it much later
	TempBuffer title_tempbuf;
	const uni_char *tmp_ptr = Title(&title_tempbuf);
	HTMLify_string(tmp_str, tmp_ptr, tmp_ptr ? uni_strlen(tmp_ptr) : 0, TRUE);
	if (tmp_str.IsEmpty())
		g_languageManager->GetStringL(Str::S_INFOPANEL_NOTITLE, tmp_str);

	out_url.WriteDocumentDataUniSprintf(UNI_L("<h1>%s</h1>\n"), tmp_str.CStr());

	// start of table of page stats
	out_url.WriteDocumentData(UNI_L("<table>\n"));

	// Page URL
	OpString locale_str;
	ANCHOR(OpString, locale_str);
	g_languageManager->GetStringL(Str::S_INFOPANEL_URL, locale_str);

	tmp_ptr = url.UniName(PASSWORD_HIDDEN);
	HTMLify_string(tmp_str, tmp_ptr, tmp_ptr ? uni_strlen(tmp_ptr) : 0, TRUE);

	out_url.WriteDocumentDataUniSprintf(UNI_L("	<tr><th>%s</th> <td>%s</td></tr>\n"),
		locale_str.CStr(), tmp_str.CStr());

	// Page encoding
	OpString tmp_str2;
	ANCHOR(OpString, tmp_str2);
	OpString tmp_str3;
	ANCHOR(OpString, tmp_str3);
	tmp_str3.SetFromUTF8(url.GetAttribute(URL::KMIME_CharSet).CStr());
	if (tmp_str3.IsEmpty())
	{
		g_languageManager->GetStringL(Str::S_INFOPANEL_NO_SUPPLIED_ENCODING, tmp_str3);
	}
	HTMLify_string(tmp_str2, tmp_str3.CStr(), tmp_str3.Length(), TRUE);

	tmp_str3.SetFromUTF8(logdoc ? logdoc->GetHLDocProfile()->GetCharacterSet() : "");
	HTMLify_string(tmp_str, tmp_str3.CStr(), tmp_str3.Length(), TRUE);

	g_languageManager->GetStringL(Str::S_INFOPANEL_ENCODING, locale_str);

	out_url.WriteDocumentDataUniSprintf(UNI_L("	<tr><th>%s</th> <td>%s (%s)</td></tr>\n"),
		locale_str.CStr(), tmp_str2.CStr(), tmp_str.CStr());

	// Page MIME type
	g_languageManager->GetStringL(Str::S_INFOPANEL_MIME, locale_str);

	tmp_str3.SetFromUTF8(url.GetAttribute(URL::KMIME_Type).CStr());
	HTMLify_string(tmp_str, tmp_str3.CStr(), tmp_str3.Length(), TRUE);

	out_url.WriteDocumentDataUniSprintf(UNI_L("	<tr><th>%s</th> <td>%s</td></tr>\n"),
		locale_str.CStr(), tmp_str.CStr());

	// Various bits of information
	g_languageManager->GetStringL(Str::SI_IDSTR_BYTES, tmp_str);
	g_languageManager->GetStringL(Str::S_INFOPANEL_SIZE, locale_str);
	out_url.WriteDocumentDataUniSprintf(UNI_L("	<tr><th>%s</th> <td>%lu %s</td></tr>\n"),
		locale_str.CStr(), (unsigned long int)url.GetContentLoaded(), tmp_str.CStr());

	g_languageManager->GetStringL(Str::S_INFOPANEL_INLINES, locale_str);
	out_url.WriteDocumentDataUniSprintf(UNI_L("	<tr><th>%s</th> <td>% 4u (% 4u)</td></tr>\n"),
		locale_str.CStr(), image_info.loaded_count, image_info.total_count);

	g_languageManager->GetStringL(Str::S_INFOPANEL_SIZEINLINES, locale_str);
	out_url.WriteDocumentDataUniSprintf(UNI_L("	<tr><th>%s</th> <td>% 8u %s</td></tr>\n"),
		locale_str.CStr(), image_info.loaded_size, tmp_str.CStr());

	// Cache info
	g_languageManager->GetStringL(Str::S_INFOPANEL_CACHE, locale_str);
	url.GetAttributeL(URL::KFilePathName_L, tmp_str, TRUE);
	if (tmp_str.IsEmpty())
		g_languageManager->GetStringL(Str::S_INFOPANEL_NOCACHE, locale_str);

	out_url.WriteDocumentDataUniSprintf(UNI_L("	<tr><th>%s</th> <td>%s</td></tr>\n"),
		locale_str.CStr(), tmp_str.CStr());

	// end of table of page stats
	out_url.WriteDocumentData(UNI_L("</table>\n"));


	// Security section
	g_languageManager->GetStringL(Str::S_INFOPANEL_SECURITY, locale_str);
	out_url.WriteDocumentDataUniSprintf(UNI_L("<h2>%s</h2>\n"), locale_str.CStr());

	// start of table of security stats
	out_url.WriteDocumentData(UNI_L("<table>\n"));

	g_languageManager->GetStringL(Str::S_INFOPANEL_SUMMARY, locale_str);
	tmp_str.Set(url.GetAttribute(URL::KSecurityText).CStr());
	if (tmp_str.IsEmpty())
		g_languageManager->GetStringL(Str::S_INFOPANEL_NO_SECURITY, tmp_str);

	out_url.WriteDocumentDataUniSprintf(UNI_L("	<tr><th>%s</th> <td>%s</td></tr>\n"),
		locale_str.CStr(), tmp_str.CStr());

#ifdef _SECURE_INFO_SUPPORT
	m_security_url_used.UnsetURL();
	URL sec_url = url.GetSecurityInformationURL();
	if (!sec_url.IsEmpty())
	{
		g_languageManager->GetStringL(Str::S_INFOPANEL_SECURITY_MOREINFO, locale_str);

		m_security_url_used.SetURL(sec_url);

		ANCHOR(URL, sec_url);
		out_url.WriteDocumentDataUniSprintf(
			UNI_L("	<tr><td colspan=2><a href=\"%s\" target=\"_blank\">%s</a></td></tr>\n"),
			sec_url.UniName(PASSWORD_HIDDEN), locale_str.CStr());
	}
#endif // _SECURE_INFO_SUPPORT

	// end of table of security stats
	out_url.WriteDocumentData(UNI_L("</table>\n"));


	// Frames section
	if (frm_root)
	{
		g_languageManager->GetStringL(Str::S_INFOPANEL_FRAMES, locale_str);
		out_url.WriteDocumentDataUniSprintf(UNI_L("<h2>%s</h2>\n<ul>\n"), locale_str.CStr());

		g_languageManager->GetStringL(Str::S_INFOPANEL_FRAME, locale_str);
		FramesDocElm *tmp_elm = (FramesDocElm*)frm_root->FirstChild();
		while (tmp_elm)
		{
			if (!tmp_elm->IsFrameset())
			{
				tmp_ptr = tmp_elm->GetCurrentURL().UniName(PASSWORD_HIDDEN);
				HTMLify_string(tmp_str, tmp_ptr, uni_strlen(tmp_ptr), TRUE);
				out_url.WriteDocumentDataUniSprintf(
					UNI_L(" <li>%s <a target=\"_blank\" href=\"%s\">%s</a></li>\n"),
					locale_str.CStr(), tmp_str.CStr(), tmp_str.CStr());
			}

			tmp_elm = (FramesDocElm*)tmp_elm->Next();
		}
		out_url.WriteDocumentData(UNI_L("</ul>\n"));
	}
	else if (ifrm_root)
	{
		g_languageManager->GetStringL(Str::S_INFOPANEL_IFRAMES, locale_str);
		out_url.WriteDocumentDataUniSprintf(UNI_L("<h2>%s</h2>\n<ul>\n"), locale_str.CStr());

		g_languageManager->GetStringL(Str::S_INFOPANEL_IFRAME, locale_str);
		FramesDocElm *tmp_elm = (FramesDocElm*)ifrm_root->FirstChild();
		while (tmp_elm)
		{
			tmp_ptr = tmp_elm->GetCurrentURL().UniName(PASSWORD_HIDDEN);
			HTMLify_string(tmp_str, tmp_ptr, uni_strlen(tmp_ptr), TRUE);
			out_url.WriteDocumentDataUniSprintf(
				UNI_L(" <li>%s <a target=\"_blank\" href=\"%s\">%s</a></li>\n"),
				locale_str.CStr(), tmp_str.CStr(), tmp_str.CStr());

			tmp_elm = (FramesDocElm*)tmp_elm->Next();
		}
		out_url.WriteDocumentData(UNI_L("</ul>\n"));
	}

/*
	out_url.WriteDocumentData(UNI_L("<h2>Time consumption</h2>"));

	out_url.WriteDocumentData(UNI_L("<table>\n"));
	out_url.WriteDocumentDataUniSprintf(UNI_L("<tr>\n<th>Loading:</th> <td>% 8s (% 8s)</td></tr>\n"), UNI_L("1:22"), UNI_L("0:45"));
	out_url.WriteDocumentDataUniSprintf(UNI_L("<tr>\n<th>Rendering:</th> <td>% 8s</td></tr>\n"), UNI_L("0:12"));
	out_url.WriteDocumentDataUniSprintf(UNI_L("<tr>\n<th>Scripts:</th> <td>% 8s</td></tr>\n"), UNI_L("0:22"));
	out_url.WriteDocumentData(UNI_L("</table>\n\n"));
*/

	out_url.WriteDocumentData(UNI_L("</body>\n</html>\n"));

	return OpBoolean::IS_TRUE;
}
#endif
#endif

OP_STATUS FramesDocument::ReloadedUrl(const URL& curl, DocumentOrigin* curl_origin, BOOL parsing_restarted)
{
	OP_ASSERT(curl_origin);
	if (Undisplay() == OpStatus::ERR_NO_MEMORY)
		return OpStatus::ERR_NO_MEMORY;

	if (dom_environment)
		dom_environment->BeforeDestroy();

	if (es_replaced_document)
		ESStoppedGeneratingDocument();

	loaded_from_url = URL();

	history_reload = FALSE;
	document_is_reloaded = TRUE;
	navigated_in_history = FALSE;

	parse_xml_as_html = FALSE;

	loading_aborted = FALSE;
	local_doc_finished = FALSE;
	doc_tree_finished = FALSE;
	document_finished_at_least_once = FALSE;
	delay_doc_finish = posted_delay_doc_finish = FALSE;
	is_multipart_reload = FALSE;
	keep_cleared = FALSE;
	is_generated_doc = FALSE;
	contains_only_opera_internal_scripts = FALSE;
	about_blank_waiting_for_javascript_url = FALSE;

	doc_start_elm_no = 0;

#ifdef KEYBOARD_SELECTION_SUPPORT
	SetKeyboardSelectable(FALSE);
#endif // KEYBOARD_SELECTION_SUPPORT

#ifdef JS_LAYOUT_CONTROL
	pause_layout = FALSE;
#endif

	// temporary fix, will be fixed when HTML_Document and FramesDocument are merged. (geir, 2002-06-03)
	if (doc)
		doc->Free();

#ifdef _PLUGIN_SUPPORT_
	m_asked_about_flash_download = FALSE;
#endif // _PLUGIN_SUPPORT_

	if (logdoc)
		logdoc->GetLayoutWorkplace()->ClearCounters();

	if (own_logdoc)
	{
		undisplaying = TRUE;
		OP_DELETE(logdoc);
		undisplaying = FALSE;
	}

	logdoc = NULL;

	has_sent_domcontent_loaded = FALSE;
	es_recent_layout_changes = FALSE;

	wrapper_doc_source.Empty();
	wrapper_doc_source_read_offset = 0;

	Clean(!parsing_restarted);

	DocListElm* list_elm = GetDocManager()->CurrentDocListElm();
	if (list_elm && (list_elm->Doc() == this))
		list_elm->SetUrl(curl);

	url = curl;

	curl_origin->IncRef();
	origin->DecRef();
	origin = curl_origin;

	SignalAllDelayedLayoutListeners();

	stream_position_base = 0;

	wait_for_styles = 0;
	GetMessageHandler()->RemoveDelayedMessage(MSG_WAIT_FOR_STYLES_TIMEOUT, GetSubWinId(), 0);

	wait_for_paint_blockers = 0;

	last_helm = NULL;

	OP_DELETE(text_selection);
	text_selection = NULL;

	do_styled_paint = FALSE;

	RemoveAllLoadInlineElms();

#ifdef XML_EVENTS_SUPPORT
	OP_ASSERT(!GetFirstXMLEventsRegistration() || !"All xml registration objects should be gone by now or they will affect the reloaded document");
#endif // XML_EVENTS_SUPPORT

#ifdef MEDIA_SUPPORT
	SuspendAllMedia(TRUE);
#endif // MEDIA_SUPPORT

	num_frames_added = 0;
	num_frames_removed = 0;

	CleanESEnvironment(TRUE);

	return CreateESEnvironment(TRUE);
}

int FramesDocument::CountJSFrames()
{
	int frames = 0;
	FramesDocElm* fde = NULL;

	if (frm_root)
		fde = (FramesDocElm*) frm_root->FirstLeaf();
	else
		if (doc && ifrm_root)
			fde = (FramesDocElm*) ifrm_root->FirstLeaf();

	while (fde)
	{
		BOOL include = TRUE;

#ifdef DELAYED_SCRIPT_EXECUTION
		if (HTML_Element* frame_elm = fde->GetHtmlElement())
		{
			if (frame_elm->GetInserted() >= HE_INSERTED_FIRST_HIDDEN_BY_ACTUAL)
				include = FALSE;
		}
#endif // DELAYED_SCRIPT_EXECUTION

		if (include)
			++frames;
		fde = (FramesDocElm*) fde->NextLeaf();
	}

	return frames;
}

/* Returns TRUE if the document or any of its subdocuments is stopped in the
   ECMAScript scheduler, waiting for a forced user action (like responding to
   an alert).
   */
BOOL FramesDocument::IsESStopped()
{
	return GetESScheduler() && GetESScheduler()->IsBlocked();
}

BOOL FramesDocument::IsESActive(BOOL treat_ready_to_run_as_active)
{
	if (!override_es_active && GetESScheduler() && (GetESScheduler()->IsPresentOnTheStack() ||
		                                            treat_ready_to_run_as_active && GetESScheduler()->HasRunnableTasks()))
		return TRUE;

	FramesDocElm* fde = frm_root ? frm_root : ifrm_root;

	if (fde)
	{
		fde = fde->FirstLeaf();

		while (fde)
		{
			FramesDocument *doc = fde->GetCurrentDoc();
			if (doc && doc->IsESActive(treat_ready_to_run_as_active))
				return TRUE;
			fde = fde->NextLeaf();
		}
	}

	return FALSE;
}

FramesDocument* FramesDocument::GetJSFrame(int index)
{
	FramesDocElm* fde = NULL;

	if (frm_root)
		fde = (FramesDocElm*) frm_root->FirstLeaf();
	else
		if (doc && ifrm_root)
			fde = (FramesDocElm*) ifrm_root->FirstLeaf();

	while (fde)
	{
		if (index-- == 0)
			return fde->GetCurrentDoc();

		fde = (FramesDocElm*) fde->NextLeaf();
	}

	return NULL;
}

#ifdef DELAYED_SCRIPT_EXECUTION

unsigned
FramesDocument::GetStreamPosition(const uni_char *buffer_position)
{
	return stream_position_base + (buffer_position - current_buffer);
}

OP_STATUS
FramesDocument::SetStreamPosition(unsigned new_stream_position)
{
	stream_position_base = 0;

	OP_DELETE(url_data_desc);
	url_data_desc = url.GetDescriptor(GetMessageHandler(), FALSE);

	if (!url_data_desc)
		return OpStatus::ERR;

	while (new_stream_position != 0)
	{
		BOOL more = FALSE;

		url_data_desc->RetrieveData(more);
		url_data_desc->GetBuffer();

		unsigned length = UNICODE_DOWNSIZE(url_data_desc->GetBufSize());

		if (length == 0)
		{
			// Cache data has suddenly disappeared.
			// Some kind of oom or internal failure in the network layer.
			// We will end up with a half finished page.
			return OpStatus::ERR;
		}

		if (length > new_stream_position)
			length = new_stream_position;

		url_data_desc->ConsumeData(UNICODE_SIZE(length));

		stream_position_base += length;
		new_stream_position -= length;

		if (new_stream_position != 0 && !more)
		{
			OP_ASSERT(!"Argh, we were not able to seek to the expected position in the stream. The data should have been in the url cache so this should not be possible.");
			break;
		}
	}

	return OpStatus::OK;
}

#endif // DELAYED_SCRIPT_EXECUTION

OP_STATUS
FramesDocument::DOMSignalDocumentFinished()
{
	if (es_info.onload_pending)
	{
		es_info.onload_pending = 0;
		return FinishDocumentAfterOnLoad();
	}

	return OpStatus::OK;
}

HTML_Element*
FramesDocument::DOMGetActiveElement()
{
	if (!doc)
		return NULL;

	HTML_Element *element = doc->GetFocusedElement();

	if (!element)
	{
		FramesDocument *sub_doc = GetTopDocument()->GetActiveSubDoc();

		FramesDocument *parent_doc = sub_doc;
		while (parent_doc && parent_doc != this)
		{
			sub_doc = parent_doc;
			parent_doc = parent_doc->GetParentDoc();
		}

		if (parent_doc && sub_doc != this)
		{
			FramesDocElm *doc_elm = sub_doc->GetDocManager()->GetFrame();
			element = doc_elm->GetHtmlElement();
		}
		else
			element = doc->GetNavigationElement();

		if (!element && logdoc)
			element = logdoc->GetBodyElm();
	}

	return element;
}

HTML_Element* FramesDocument::GetWindowEventTarget(DOM_EventType event_type)
{
	HTML_Element* target = NULL;
	if (IsFrameDoc())
	{
		HTML_Element* frameset = GetFrmDocRoot()->GetHtmlElement();
		if (frameset)
		{
			HTML_Element* elm = frameset->LastLeaf();
			if (elm)
			{
				while (elm && elm != frameset &&
					!(elm->IsMatchingType(HE_FRAMESET, NS_HTML) && elm->HasEventHandlerAttribute(this, event_type)))
					elm = elm->PrevActual();
				frameset = elm;
			}
			target = frameset;
		}
	}
	else if (logdoc)
	{
		target = logdoc->GetBodyElm();

		if (!target)
			target = logdoc->GetDocRoot();
	}

	return target;
}

BOOL
FramesDocument::HasWindowHandlerAsBodyAttr(DOM_EventType event_type)
{
	if (DOM_EventsAPI::IsWindowEventAsBodyAttr(event_type))
		if (LogicalDocument *logdoc = GetLogicalDocument())
			if (HTML_Element *body = logdoc->GetBodyElm())
				return body->HasEventHandlerAttribute(this, event_type);

	return FALSE;
}

#ifndef HAS_NOTEXTSELECTION
void
FramesDocument::DOMOnSelectionUpdated()
{
	if (dom_environment)
		dom_environment->OnSelectionUpdated();
}
#endif // !HAS_NOTEXTSELECTION

/* static */ BOOL
FramesDocument::NeedToFireEvent(FramesDocument *doc, HTML_Element *target, HTML_Element *related_target, DOM_EventType type)
{
	if (DOM_Environment *environment = doc->GetDOMEnvironment())
		return environment->IsEnabled();
	else if (DOM_Environment::IsEnabled(doc))
	{
		unsigned count;
		if (!doc->GetLogicalDocument()->GetEventHandlerCount(type, count) || count != 0)
		{
			HTML_Element *currentTarget = target;
			while (currentTarget)
				if (DOM_Utils::GetEventTargetElement(currentTarget)->HasEventHandlerAttribute(doc, type))
					return TRUE;
				else
					currentTarget = DOM_Utils::GetEventPathParent(currentTarget, target);
		}

		if (doc->HasWindowHandlerAsBodyAttr(type))
			return TRUE;

		if (type == ONMOUSEOVER && FramesDocument::NeedToFireEvent(doc, related_target, target, ONMOUSEOUT))
			return TRUE;
		else if (type == ONMOUSEUP && FramesDocument::NeedToFireEvent(doc, target, related_target, ONMOUSEDOWN))
			return TRUE;
	}
	return FALSE;
}

/* virtual */ OP_STATUS
FramesDocument::HandleEvent(DOM_EventType event, HTML_Element* related_target, HTML_Element* target,
                            ShiftKeyState modifiers, int detail, ES_Thread* interrupt_thread, BOOL *handled,
                            unsigned int id)
{
	OP_BOOLEAN was_handled = OpBoolean::IS_FALSE;

	if (event == ONCHANGE)
	{
		if (target->IsFormElement())
			target->GetFormValue()->UpdateSerialNr(this);
	}

	if (FramesDocument::NeedToFireEvent(this, target, related_target, event))
	{
		RETURN_IF_ERROR(ConstructDOMEnvironment());

		DOM_Environment::EventData data;

		data.type = event;
		data.target = target;
		data.modifiers = modifiers;
		data.relatedTarget = related_target;
		data.detail = detail;
		data.id = id;

		was_handled = dom_environment->HandleEvent(data, interrupt_thread);
	}

#ifdef SVG_SUPPORT
	if (target->GetNsType() == NS_SVG)
	{
		SVGManager::EventData data;
		data.type = event;
		data.detail = detail;
		data.target = target;
		data.frm_doc = this;
		OP_BOOLEAN svg_handled = g_svg_manager->HandleEvent(data);
		if (OpStatus::IsError(svg_handled))
			was_handled = svg_handled;
	}
#endif // SVG_SUPPORT

	if (handled)
		*handled = was_handled == OpBoolean::IS_TRUE;

	if (was_handled == OpBoolean::IS_FALSE)
	{
		HTML_Element::HandleEventOptions opts;
		opts.related_target = related_target;
		opts.modifiers = modifiers;
		opts.thread = interrupt_thread;
		opts.synthetic = interrupt_thread != NULL;
		opts.id = id;

		if (target->HandleEvent(event, this, target, opts) && handled)
			*handled = TRUE;
	}

	return OpStatus::IsMemoryError(was_handled) ? was_handled : OpStatus::OK;
}

void
FramesDocument::HandleEventFinished(DOM_EventType event, HTML_Element* target)
{
#ifdef SVG_SUPPORT
	if (target->GetNsType() == NS_SVG)
	{
		g_svg_manager->HandleEventFinished(event, target);
	}
#endif // SVG_SUPPORT

#ifdef WIDGETS_IME_SUPPORT
	// Special case IME handling for onfocus event. When creating IME
	// for an edit field with an onfocus handler IME creation is
	// postponed until the handler in finished in case the edit
	// contents is changed, and called here instead.
	if (event == ONFOCUS && target->GetFormObject() && (current_focused_formobject == target->GetFormObject()))
	{
		target->GetFormObject()->HandleFocusEventFinished();
	}
#endif // WIDGETS_IME_SUPPORT
#ifdef SCOPE_SUPPORT
	if (event == DOMCONTENTLOADED)
		OpScopeReadyStateListener::OnReadyStateChanged(this, OpScopeReadyStateListener::READY_STATE_AFTER_DOM_CONTENT_LOADED);
#endif // SCOPE_SUPPORT
}

/* virtual */ OP_STATUS
FramesDocument::HandleMouseEvent(DOM_EventType event, HTML_Element* related_target, HTML_Element* target,
                                 HTML_Element* referencing_element,
                                 int offset_x, int offset_y, int document_x, int document_y,
                                 ShiftKeyState modifiers, int sequence_count_and_button_or_delta,
                                 ES_Thread* interrupt_thread /* = NULL */,
                                 BOOL first_part_of_paired_event /* = FALSE */,
                                 BOOL calculate_offset_lazily /* = FALSE */,
                                 int* visual_viewport_x /* = NULL */, int* visual_viewport_y /* = NULL */,
                                 BOOL has_keyboard_origin /* = FALSE */)
{
#ifdef DRAG_SUPPORT
	/* The input events (mouse, keyboard, touch) must be suppressed while dragging and their queue must be emptied (it's done by ReplayRecordedMouseActions()).
	The exceptions here are to make mouse scroll working durring the drag. ONMOUSEUP is here to balance OMOUSEDOWN sent before d'n'd was started. */
	if (g_drag_manager->IsDragging() && !g_drag_manager->IsBlocked() && event != ONMOUSEWHEEL && event != ONMOUSEWHEELH && event != ONMOUSEWHEELV && event != ONMOUSEUP)
	{
		doc->ReplayRecordedMouseActions();
		return OpStatus::OK;
	}
#endif // DRAG_SUPPORT

	OP_BOOLEAN handled = OpBoolean::IS_FALSE;

	if (!IsPrintDocument())
	{
		/* Target, related_target and referencing_target must be either NULL
		   or elements in this document. */
		OP_ASSERT(GetDocRoot() && GetDocRoot()->IsAncestorOf(target));
		OP_ASSERT(!related_target || GetDocRoot() && GetDocRoot()->IsAncestorOf(related_target));
		OP_ASSERT(!referencing_element || GetDocRoot() && GetDocRoot()->IsAncestorOf(referencing_element));

		BOOL send_mouse_event_to_script = TRUE;

#ifdef SUPPORT_VISUAL_ADBLOCK
		if (GetWindow()->GetContentBlockEditMode())
			send_mouse_event_to_script = FALSE;
#endif // SUPPORT_VISUAL_ADBLOCK

#ifdef DRAG_SUPPORT
		/* Some input events are exceptionally passed here (e.g. in order to make mouse scroll working while dragging)
		but they must not be sent to scripts since input events must be suppressed during d'n'd. */
		if (send_mouse_event_to_script)
			send_mouse_event_to_script = !g_drag_manager->IsDragging() || g_drag_manager->IsBlocked();
#endif // DRAG_SUPPORT

		if (GetWindow()->GetType() != WIN_TYPE_DEVTOOLS &&
			!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::AllowScriptToReceiveRightClicks, GetURL()))
		{
			if (event == ONMOUSEUP || event == ONMOUSEDOWN || event == ONCLICK || event == ONCONTEXTMENU)
			{
				if (EXTRACT_MOUSE_BUTTON(sequence_count_and_button_or_delta) == MOUSE_BUTTON_2)
					send_mouse_event_to_script = FALSE;
			}
		}

		if (send_mouse_event_to_script)
		{
			CoreView* view = GetVisualDevice()->GetView();
			OpPoint screen_offset = view ? view->ConvertToScreen(OpPoint(0, 0)) : OpPoint(0, 0);
			int visual_vp_x = visual_viewport_x ? *visual_viewport_x : GetVisualViewX();
			int visual_vp_y = visual_viewport_y ? *visual_viewport_y : GetVisualViewY();

			if (FramesDocument::NeedToFireEvent(this, target, related_target, event))
			{
				OP_STATUS status = ConstructDOMEnvironment();

				if (OpStatus::IsSuccess(status))
				{
					DOM_Environment::EventData data;

					data.type = event;
					if (event == ONDBLCLICK)
						data.detail = 2;
					else if (event == ONCLICK || event == ONMOUSEDOWN || event == ONMOUSEUP)
					{
						data.detail = EXTRACT_SEQUENCE_COUNT(sequence_count_and_button_or_delta);
						data.might_be_click = EXTRACT_CLICK_INDICATOR(sequence_count_and_button_or_delta);
					}
					else if (event == ONMOUSEWHEELH || event == ONMOUSEWHEELV)
						data.detail = sequence_count_and_button_or_delta;
					else
						OP_ASSERT(data.detail == 0);

					data.target = target;
					data.modifiers = modifiers;
					data.screenX = document_x - visual_vp_x + screen_offset.x;
					data.screenY = document_y - visual_vp_y + screen_offset.y;
					data.clientX = document_x - visual_vp_x;
					data.clientY = document_y - visual_vp_y;
					data.offsetX = offset_x;
					data.offsetY = offset_y;
					data.calculate_offset_lazily = calculate_offset_lazily;
					data.button = (event == ONMOUSEWHEELH || event == ONMOUSEWHEELV) ? 0 : EXTRACT_MOUSE_BUTTON(sequence_count_and_button_or_delta);
					data.relatedTarget = related_target;
					data.has_keyboard_origin = has_keyboard_origin;

					handled = dom_environment->HandleEvent(data, interrupt_thread);
				}
				else
					handled = status;
			}

#ifdef SVG_SUPPORT
			if (target->GetNsType() == NS_SVG)
			{
				SVGManager::EventData data;
				data.type = event;
				data.target = target;
				data.modifiers = modifiers;
				if (event == ONDBLCLICK)
					data.detail = 2;
				else if (event == ONCLICK)
				{
					data.detail = EXTRACT_SEQUENCE_COUNT(sequence_count_and_button_or_delta);
					if (data.detail == 0) // old display module
						data.detail = 1;
				}
				else
					data.detail = 0;
				data.screenX = document_x - visual_vp_x + screen_offset.x;
				data.screenY = document_y - visual_vp_y + screen_offset.y;
				data.clientX = document_x - visual_vp_x;
				data.clientY = document_y - visual_vp_y;
				data.offsetX = offset_x;
				data.offsetY = offset_y;
				data.button = EXTRACT_MOUSE_BUTTON(sequence_count_and_button_or_delta);
				data.frm_doc = this;
				OP_BOOLEAN svg_handled = g_svg_manager->HandleEvent(data);
				if (OpStatus::IsError(svg_handled))
					handled = svg_handled;
			}
#endif // SVG_SUPPORT
		}
	}

	if (handled == OpBoolean::IS_TRUE)
	{
		if (first_part_of_paired_event)
		{
			// This event might register an handler for the next event so
			// we must force the next event to be handled even if there
			// is no event handler right now.
			dom_environment->ForceNextEvent();
		}
	}
	else
	{
#ifndef MOUSELESS
		if (event == ONMOUSEMOVE && doc)
			doc->UpdateMouseMovePosition(document_x, document_y);
#endif // !MOUSELESS

		HTML_Element::HandleEventOptions opts;
		opts.related_target = related_target;
		opts.offset_x = offset_x;
		opts.offset_y = offset_y;
		opts.document_x = document_x;
		opts.document_y = document_y;
		opts.sequence_count_and_button_or_key_or_delta = sequence_count_and_button_or_delta;
		opts.modifiers = modifiers;
		opts.thread = interrupt_thread;
		opts.synthetic = interrupt_thread != NULL;
		opts.has_keyboard_origin = has_keyboard_origin;
		target->HandleEvent(event, this, target, opts, referencing_element);
	}

	return OpStatus::IsMemoryError(handled) ? handled : OpStatus::OK;
}

#ifdef TOUCH_EVENTS_SUPPORT
OP_STATUS
FramesDocument::HandleTouchEvent(DOM_EventType event, HTML_Element* target, int id, int offset_x, int offset_y,
		int document_x, int document_y, int visual_viewport_x, int visual_viewport_y, int radius, int sequence_count_and_button_or_delta,
		ShiftKeyState modifiers, void* user_data,
	    ES_Thread* interrupt_thread /* = NULL */)
{
#ifdef DRAG_SUPPORT
	/* The input events (mouse, keyboard, touch) must be suppressed while dragging and their queue must be emptied
	(it's done by ReplayRecordedMouseActions()). */
	if (g_drag_manager->IsDragging() && !g_drag_manager->IsBlocked())
	{
		doc->ReplayRecordedMouseActions();
		return OpStatus::OK;
	}
#endif // DRAG_SUPPORT
	OP_BOOLEAN handled = OpBoolean::IS_FALSE;

	if (!IsPrintDocument())
	{
#ifdef SUPPORT_VISUAL_ADBLOCK
		if (!GetWindow()->GetContentBlockEditMode())
#endif // SUPPORT_VISUAL_ADBLOCK
		{
			handled = ConstructDOMEnvironment();
			if (OpStatus::IsSuccess(handled))
			{
				CoreView* view = GetVisualDevice()->GetView();
				OpPoint screen_offset = view ? view->ConvertToScreen(OpPoint(0, 0)) : OpPoint(0, 0);

				if (event != TOUCHSTART || FramesDocument::NeedToFireEvent(this, target, NULL, event))
				{
					DOM_Environment::EventData data;

					data.type = event;
					data.target = target;
					data.detail = 0;
					data.button = id;
					data.modifiers = modifiers;
					data.offsetX = offset_x;
					data.offsetY = offset_y;
					data.screenX = document_x - visual_viewport_x + screen_offset.x;
					data.screenY = document_y - visual_viewport_y + screen_offset.y;
					data.clientX = document_x - visual_viewport_x;
					data.clientY = document_y - visual_viewport_x;
					data.radius = radius;
					data.user_data = user_data;
					data.might_be_click = EXTRACT_CLICK_INDICATOR(sequence_count_and_button_or_delta);

					handled = dom_environment->HandleEvent(data, interrupt_thread);
				}
			}
		}
	}

	if (handled != OpBoolean::IS_TRUE)
	{
		/*
		 * As we only have a target element in the case of TOUCHSTART, perform
		 * default handling on the document body element. Simulated mouse events
		 * would not be targeted at the same elements as the touch events any
		 * way, and perform their own coordinate -> target mapping.
		 */
		HTML_Element* body = logdoc ? logdoc->GetBodyElm() : NULL;
		if (body)
		{
			BOOL might_be_click = EXTRACT_CLICK_INDICATOR(sequence_count_and_button_or_delta);
			HTML_Element::HandleEventOptions opts;
			opts.offset_x = offset_x;
			opts.offset_y = offset_y;
			opts.document_x = document_x;
			opts.document_y = document_y;
			opts.sequence_count_and_button_or_key_or_delta = MAKE_SEQUENCE_COUNT_CLICK_INDICATOR_AND_BUTTON(0, might_be_click, id);
			opts.modifiers = modifiers;
			opts.radius = radius;
			opts.user_data = user_data;
			opts.thread = interrupt_thread;
			opts.synthetic = interrupt_thread != NULL;

			body->HandleEvent(event, this, body, opts);
		}
		else
			doc->ReplayRecordedMouseActions();
	}

	return OpStatus::IsMemoryError(handled) ? handled : OpStatus::OK;
}
#endif // TOUCH_EVENTS_SUPPORT

/* virtual */ OP_STATUS
FramesDocument::HandleWindowEvent(DOM_EventType event, ES_Thread *interrupt_thread, const uni_char* type_custom, int event_param, ES_Thread** created_thread /* = NULL */)
{
	OP_BOOLEAN handled = OpBoolean::IS_FALSE;
	OP_STATUS status = OpStatus::OK;

	if (HTML_Element *target = GetWindowEventTarget(event))
	{
		if (FramesDocument::NeedToFireEvent(this, target, NULL, event))
		{
			RETURN_IF_ERROR(ConstructDOMEnvironment());

			DOM_Environment::EventData data;

			data.type = event;
			data.type_custom = type_custom;
			data.detail = event_param;
			data.target = target;
			data.windowEvent = TRUE;

			handled = dom_environment->HandleEvent(data, interrupt_thread, created_thread);

			if (event == ONLOAD)
				if (handled == OpBoolean::IS_TRUE)
					es_info.onload_pending = 1;
		}

#ifdef SCOPE_SUPPORT
		if (event == ONLOAD && handled == OpBoolean::IS_FALSE)
			/* In case there was no thread created that would handle this
			   notification, fire it up immediately. Matching "after"
			   notification is sent from the FinishDocumentAfterOnLoad(). */
			OpScopeReadyStateListener::OnReadyStateChanged(this, OpScopeReadyStateListener::READY_STATE_BEFORE_ONLOAD);
#endif // SCOPE_SUPPORT

#ifdef SVG_SUPPORT
		if (target->GetNsType() == NS_SVG)
		{
			SVGManager::EventData data;
			data.type = event;
			data.target = target;
			data.frm_doc = this;
			OP_BOOLEAN svg_handled = g_svg_manager->HandleEvent(data);
			if (OpStatus::IsError(svg_handled))
				handled = svg_handled;
		}
#endif // SVG_SUPPORT

		if (handled != OpBoolean::IS_TRUE)
		{
			HTML_Element::HandleEventOptions opts;
			opts.thread = interrupt_thread;
			opts.synthetic = interrupt_thread != NULL;
			target->HandleEvent(event, this, target, opts);
		}
	}

	return OpStatus::IsMemoryError(handled) ? handled : status;
}

/* virtual */ OP_STATUS
FramesDocument::HandleDocumentEvent(DOM_EventType event)
{
	OP_STATUS status = OpStatus::OK;

	if (HTML_Element *target = GetDocRoot())
	{
		status = HandleEvent(event, NULL, target, SHIFTKEY_NONE);
		if (OpStatus::IsMemoryError(status))
			GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	}

	return status;
}

/* virtual */ OP_STATUS
FramesDocument::HandleKeyboardEvent(DOM_EventType event, HTML_Element* target, OpKey::Code key, const uni_char *key_value, OpPlatformKeyEventData *key_event_data, ShiftKeyState modifiers, BOOL repeat, OpKey::Location location, unsigned extra_data)
{

	if (doc)
	{
		HTML_Element* referencing_element = NULL;

		if (!target)
			target = doc->GetFocusedElement();

		if (!target)
			target = doc->GetCurrentAreaElement();

		if (!target)
		{
			/* Target is the element currently selected by keyboard navigation. */
			target = doc->GetNavigationElement();
			/* ..except if it is prefocused, the document gets it. */
			if (target && target->IsPreFocused())
				target = NULL;
		}

		if (!target && logdoc)
		{
			// The <body> element
			target = logdoc->GetBodyElm();
		}

		if (target)
		{
			OP_BOOLEAN handled = OpBoolean::IS_FALSE;

			/* If the current highlighted element is prefocused, we should not
			   send Enter key event to document keypress handler. */
			BOOL hide_from_scripts = FALSE;
			if (key == OP_KEY_ENTER && doc->GetNavigationElement() && doc->GetNavigationElement()->IsPreFocused())
			{
				hide_from_scripts = TRUE;
			}
			else if (target->IsMatchingType(HE_INPUT, NS_HTML) && target->GetInputType() == INPUT_FILE)
			{
				// Security bug 311968 - if scripts can see and manipulate key events targetted at file inputs,
				// they can more easily trick the user into writing a file path.
				hide_from_scripts = TRUE;
			}

#ifdef DRAG_SUPPORT
			// The input events (mouse, keyboard, touch) must be suppressed while dragging.
			if (g_drag_manager->IsDragging() && !g_drag_manager->IsBlocked())
				hide_from_scripts = TRUE;
#endif // DRAG_SUPPORT

			if (!hide_from_scripts)
			{
				if (FramesDocument::NeedToFireEvent(this, target, NULL, event))
				{
					RETURN_IF_ERROR(ConstructDOMEnvironment());

					DOM_Environment::EventData data;

					data.type = event;
					data.target = target;
					data.modifiers = modifiers;
					data.key_code = key;
					data.key_value = key_value;
					data.key_repeat = repeat;
					data.key_location = location;
					data.key_data = extra_data;
					data.key_event_data = key_event_data;

					handled = dom_environment->HandleEvent(data);
				}

#ifdef SVG_SUPPORT
				if (target->GetNsType() == NS_SVG)
				{
					SVGManager::EventData data;
					data.type = event;
					data.target = target;
					data.modifiers = modifiers;
					data.frm_doc = this;
					OP_BOOLEAN svg_handled = g_svg_manager->HandleEvent(data);
					if (OpStatus::IsError(svg_handled))
						handled = svg_handled;
				}
#endif // SVG_SUPPORT
			}

			if (handled == OpBoolean::IS_FALSE)
			{
				HTML_Element::HandleEventOptions opts;
				opts.sequence_count_and_button_or_key_or_delta = key;
				opts.modifiers = modifiers;
				opts.user_data = const_cast<uni_char *>(key_value);
				opts.key_event_data = key_event_data;
				opts.has_keyboard_origin = TRUE;
				if (target->HandleEvent(event, this, target, opts, referencing_element))
					handled = OpBoolean::IS_TRUE;
			}

			return OpStatus::IsMemoryError(handled) ? handled : OpStatus::OK;
		}
	}

	return OpStatus::ERR_NO_SUCH_RESOURCE;
}

/* virtual */ OP_STATUS
FramesDocument::HandleHashChangeEvent(const uni_char* old_fragment, const uni_char* new_fragment)
{
	if (old_fragment == new_fragment || (old_fragment && new_fragment && uni_str_eq(old_fragment, new_fragment)))
		return OpStatus::OK;

	HTML_Element *target = GetWindowEventTarget(ONHASHCHANGE);

	if (target && FramesDocument::NeedToFireEvent(this, target, NULL, ONHASHCHANGE))
	{
		RETURN_IF_ERROR(ConstructDOMEnvironment());

		DOM_Environment::EventData data;

		data.type = ONHASHCHANGE;
		data.target = target;
		data.windowEvent = TRUE;
		data.old_fragment = old_fragment;
		data.new_fragment = new_fragment;

		RETURN_IF_MEMORY_ERROR(dom_environment->HandleEvent(data, NULL, NULL));
	}

	return OpStatus::OK;
}

/* static */
OP_STATUS FramesDocument::CheckOnLoad(FramesDocument* start_point_doc, FramesDocElm* start_point_frame /* = NULL */)
{
	OP_NEW_DBG("FramesDocument::CheckOnLoad", "onload");
	OP_DBG(("FramesDocument %p: %s", start_point_doc, start_point_doc ? start_point_doc->url.Name(PASSWORD_SHOW) : "<no doc>"));
	OP_DBG(("FramesDocElm %p: %s", start_point_frame, (start_point_frame && start_point_frame->GetCurrentDoc()) ? start_point_frame->GetCurrentDoc()->url.Name(PASSWORD_SHOW) : "<no current doc>"));

	OP_ASSERT(start_point_doc || start_point_frame);
	OP_ASSERT(!(start_point_doc && start_point_frame));

	// Bubble upwards and send onload events until we reach a subtree
	// root where not everything is ready for onload.
	FramesDocument* doc = start_point_doc;
	FramesDocElm* frame = start_point_frame;
	FramesDocument* top_doc = (doc ? doc : frame->GetParentFramesDoc())->GetTopDocument();

	BOOL assume_leaf_is_loaded = FALSE;
	do
	{
		if (doc)
		{
			if (doc->frm_root)
			{
				if (!doc->frm_root->OnLoadCalled())
					return OpStatus::OK;
			}
			else if (!doc->IsLoaded() || doc->es_info.inhibit_onload_about_blank && IsAboutBlankURL(doc->GetURL()))
				return OpStatus::OK;
			else if (doc->ifrm_root && doc->ifrm_root->FirstChild())
			{
				if (!doc->ifrm_root->OnLoadCalled())
					return OpStatus::OK;
			}

			assume_leaf_is_loaded = TRUE;
			if (!doc->es_info.onload_called)
			{
				doc->es_info.onload_ready = TRUE;

				if (top_doc->es_info.onload_thread_running_in_subtree)
					;// We will send it from OnLoadSender::Signal()
				else
					RETURN_IF_ERROR(doc->SendOnLoadInternal());

#ifdef VISITED_PAGES_SEARCH
				if (doc == top_doc)
				{
					doc->CloseVisitedSearchHandle();
					OP_ASSERT(!doc->GetVisitedSearchHandle());
				}
#endif // VISITED_PAGES_SEARCH
			}

			frame = doc->GetDocManager()->GetFrame();
			doc = NULL;
		}

		if (frame)
		{
			// Check if this tree of documents is ready to have the onload events sent
			if (!assume_leaf_is_loaded)
			{
				if (FramesDocument* current_doc = frame->GetCurrentDoc())
				{
					if (!current_doc->IsLoaded() || current_doc->es_info.inhibit_onload_about_blank && IsAboutBlankURL(current_doc->GetURL()))
						return OpStatus::OK;
				}

				assume_leaf_is_loaded = TRUE;
			}

			// Check if all children are finished
			if (!frame->OnLoadCalled())
			{
				for (FramesDocElm* frame_child = frame->FirstChild(); frame_child; frame_child = frame_child->Suc())
				{
					// First checking this flag so that we can cut off
					// the branch we just came from. We trust this
					// flag completely now which may be a mistake
					// (historically we've had document tree branches
					// without a leaf document and then there has been
					// nobody to set the flag) but we create more
					// documents nowadays.
					if (!frame_child->OnLoadCalled())
						// We'll send the events later when this branch is loaded
						return OpStatus::OK;
				}
			}

			if (!frame->OnLoadCalled())
			{
				// The events might have been sent already but if not,
				// say that it's ready to send them.
				frame->SetOnLoadReady(TRUE);

				if (top_doc->es_info.onload_thread_running_in_subtree)
					; // It will be sent from OnLoadSender::Signal
				else
				{
					if (frame->GetCurrentDoc())
					{
						/* Send load event to the frame or iframe element. */
						if (FramesDocument *parent_doc = frame->GetDocManager()->GetParentDoc())
							RETURN_IF_ERROR(frame->GetCurrentDoc()->SendOnLoadToFrameElement(parent_doc));
					}
					else
						frame->SetOnLoadCalled(TRUE);
				}
			}

			if (frame->Parent())
				frame = frame->Parent();
			else
			{
				doc = frame->GetParentFramesDoc();
				frame = NULL;
			}
		}

		OP_ASSERT(!doc || !frame);
	}
	while (doc || frame);

	return OpStatus::OK;
}

/**
 * This class is an integral part of the chain that sends onload
 * events to everyone that needs it.
 * This will catch when one onload event is finished and
 * then move on to the next event.
 */
class OnLoadSender : public ES_ThreadListener
{
	// See base class documentation
	virtual OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal)
	{
		OP_NEW_DBG("OnLoadSender::Signal", "onload");
		OP_ASSERT(thread);
		if (signal == ES_SIGNAL_CANCELLED ||
			signal == ES_SIGNAL_FINISHED ||
			signal == ES_SIGNAL_FAILED)
		{
			ES_ThreadScheduler* scheduler = thread->GetScheduler();
			OP_ASSERT(scheduler);
			FramesDocument* doc = scheduler->GetFramesDocument();
			if (doc)
			{
				OP_DBG(("FramesDocument %p:%s", doc, doc->GetURL().Name(PASSWORD_SHOW)));
				/* Send load event to the frame or iframe element. */
				if (FramesDocument *parent_doc = doc->GetParentDoc())
					RETURN_IF_ERROR(doc->SendOnLoadToFrameElement(parent_doc));

				FramesDocument* top_document = doc->GetTopDocument();
				top_document->es_info.onload_thread_running_in_subtree = FALSE;

				// Try to find somewhere else to send onload
				if (doc->GetDocManager()->GetFrame())
					FramesDocument::CheckOnLoad(NULL, doc->GetDocManager()->GetFrame());

				// First search from this element forward (mode==0), assuming that we're more likely
				// to find a job there than by scanning the start of the document tree, then search
				// from the start (mode==1).
				for (int mode = 0; mode < 2 && !top_document->es_info.onload_thread_running_in_subtree; mode++)
				{
					DocumentTreeIterator it(top_document);
					if (mode == 0)
						it.SkipTo(doc->GetDocManager());
					while (it.Next())
					{
						FramesDocument* other_doc = it.GetFramesDocument();
						OP_ASSERT(mode == 1 || other_doc != doc || !"Should never see doc in the first iteration");
						if (mode == 1 && other_doc == doc) // wrapped around
							break;

						if (other_doc->es_info.onload_ready && !other_doc->es_info.onload_called)
						{
							OP_DBG(("Found a new place to send onload %p:%s", other_doc, other_doc->GetURL().Name(PASSWORD_SHOW)));
							FramesDocument::CheckOnLoad(other_doc);
							if (top_document->es_info.onload_thread_running_in_subtree)
								break;
						}
					}
				}
			}
			else
				OP_ASSERT(FALSE); // What to do? Will miss onload events?

			Remove();
			OP_DELETE(this);
		}
		return OpStatus::OK;
	}
};

void FramesDocument::PropagateOnLoadStatus()
{
	FramesDocElm *this_frame = GetDocManager()->GetFrame();
	for (FramesDocElm* fde = this_frame; fde && fde->OnLoadReady(); fde = fde->Parent())
	{
		for (FramesDocElm* fde_child = fde->FirstChild(); fde_child; fde_child = fde_child->Suc())
			if (!fde_child->OnLoadCalled())
				return;

		fde->SetOnLoadCalled(TRUE);
	}
}

OP_STATUS FramesDocument::SendOnLoadToFrameElement(FramesDocument* parent_doc)
{
	OP_NEW_DBG("FramesDocument::SendOnLoadToFrameElement", "onload");
	OP_DBG(("child doc %p:%s", this, url.Name(PASSWORD_SHOW)));
	OP_DBG(("parent doc %p:%s", parent_doc, parent_doc->url.Name(PASSWORD_SHOW)));

	OP_ASSERT(parent_doc);
	FramesDocElm *this_frame = GetDocManager()->GetFrame();
	HTML_Element *this_frame_elm = this_frame->GetHtmlElement();

	// Move FramesDocElms from onload_ready to onload_called
	PropagateOnLoadStatus();

	if (this_frame_elm && !es_info.inhibit_parent_elm_onload)
	{
		BOOL handled;
		RETURN_IF_ERROR(parent_doc->HandleEvent(ONLOAD, NULL, this_frame_elm, SHIFTKEY_NONE, 0, NULL, &handled));
		if (handled)
			compatible_history_navigation_needed = TRUE;
	}
	es_info.inhibit_parent_elm_onload = FALSE;

	return OpStatus::OK;
}

OP_STATUS FramesDocument::SendOnLoadInternal()
{
	OP_NEW_DBG("FramesDocument::SendOnLoadInternal", "onload");
	OP_DBG(("FramesDocument %p:%s", this, url.Name(PASSWORD_SHOW)));

	OP_ASSERT(es_info.onload_ready);

	if (!es_info.onload_called)
	{
		es_info.onload_called = TRUE;

		ES_Thread* created_thread = NULL;

		if (IsESActive(TRUE) && dom_environment)
			// A queued event (for instance for an frame onload) might register
			// the onload handler so we need to force queueing it even if there
			// is not yet any listener.
			dom_environment->ForceNextEvent();

		if (HandleWindowEvent(ONLOAD, NULL, NULL, 0, &created_thread) == OpStatus::ERR_NO_MEMORY)
			return(OpStatus::ERR_NO_MEMORY);

		if (created_thread)
		{
			OnLoadSender* listener = OP_NEW(OnLoadSender, ());
			if (listener)
			{
				OP_ASSERT(!GetTopDocument()->es_info.onload_thread_running_in_subtree);
				GetTopDocument()->es_info.onload_thread_running_in_subtree = TRUE;
				created_thread->AddListener(listener);
			}
			else
			{
				// OOM, not much to do, just keep going
			}
		}

#ifdef SVG_SUPPORT
		SVGManager::EventData svg_event;
		svg_event.type = SVGLOAD;
		svg_event.frm_doc = this;
		RETURN_IF_ERROR(g_svg_manager->HandleEvent(svg_event));
#endif // SVG_SUPPORT
	}

	return OpStatus::OK;
}

/* static */ BOOL FramesDocument::OnLoadReadyFrame(FramesDocElm *fdelm)
{
	OP_NEW_DBG("FramesDocument::OnLoadReadyFrame", "onload");
	OP_DBG(("FramesDocElm %p:%s", fdelm));
	if (FramesDocument* current_doc = fdelm->GetCurrentDoc())
		return current_doc->OnLoadReady();
	else if (fdelm->FirstChild())
	{
		for (FramesDocElm* fde = fdelm->FirstChild(); fde; fde = fde->Suc())
			if (!OnLoadReadyFrame(fde))
				return FALSE;
	}
	else if (fdelm->GetDocManager()->GetLoadStatus() == WAIT_FOR_HEADER || fdelm->GetDocManager()->GetLoadStatus() == WAIT_FOR_ACTION)
		return FALSE;

	return TRUE;
}

BOOL FramesDocument::OnLoadReady()
{
	OP_NEW_DBG("FramesDocument::OnLoadReady", "onload");
	OP_DBG(("FramesDocument %p:%s", this, url.Name(PASSWORD_SHOW)));
	BOOL res;
	if (es_info.onload_ready)
		if (frm_root)
			res = OnLoadReadyFrame(frm_root);
		else if (ifrm_root)
			res = OnLoadReadyFrame(ifrm_root);
		else
			res = TRUE;
	else
		res = FALSE;
	OP_DBG(("OnLoadReady returns %s", res ? "TRUE" : "FALSE"));
	return res;
}

OP_STATUS
FramesDocument::ScheduleAsyncOnloadCheck(ES_Thread* thread, BOOL about_blank_only)
{
	OP_ASSERT(thread);

	// If there is an onload_checker already we don't need a new one.
	if (!onload_checker)
	{
		ES_Thread* root_thread = thread->GetRunningRootThread();
		onload_checker = OP_NEW(AsyncOnloadChecker, (this, about_blank_only));
		if (!onload_checker)
			return OpStatus::ERR_NO_MEMORY;

		root_thread->AddListener(onload_checker);
	}
	else if (onload_checker->about_blank_only && !about_blank_only)
	{
		/* The about:blank async checker has lowest priority, override. */
		onload_checker->about_blank_only = FALSE;

		/* Replace listener also. */
		onload_checker->Remove();
		ES_Thread* root_thread = thread->GetRunningRootThread();
		root_thread->AddListener(onload_checker);
	}

	return OpStatus::OK;
}


#if defined DOC_EXECUTEES_SUPPORT || defined HAS_ATVEF_SUPPORT
OP_STATUS FramesDocument::ExecuteES(const uni_char* script)
{
	if (!GetESAsyncInterface())
		if (OpStatus::IsError(ConstructDOMEnvironment()))
			return OpStatus::ERR;

	ES_AsyncInterface* ai = GetESAsyncInterface();

	if (!ai)
		return OpStatus::ERR_NULL_POINTER;

	return ai->Eval(script);
}
#endif // DOC_EXECUTEES_SUPPORT || HAS_ATVEF_SUPPORT

OP_STATUS FramesDocument::ESTerminate(ES_TerminatingThread *thread)
{
	DocumentTreeIterator iter(this);
	iter.SetIncludeThis();
	while (iter.Next())
	{
		FramesDocument* frm_doc = iter.GetFramesDocument();
		if (frm_doc->GetESScheduler() == thread->GetScheduler())
		{
			// Obviously already have a terminating thread here so
			// nothing more to do
			continue;
		}

		if (frm_doc->GetDOMEnvironment() &&
			frm_doc->GetDOMEnvironment()->IsEnabled())
		{
			/* Called from parent document. */
			ES_TerminatingAction *action;

			action = OP_NEW(ES_TerminatedByParentAction, (thread));
			if (!action)
				return OpStatus::ERR_NO_MEMORY;

			RETURN_IF_ERROR(frm_doc->GetESScheduler()->AddTerminatingAction(action));
		}
	}

	return OpStatus::OK;
}

static OP_BOOLEAN IsInlineThreadInDocument(FramesDocument *doc, ES_Runtime *origining_runtime, ES_Thread *origining_thread, ESDocException *doc_exception)
{
	OP_ASSERT(doc_exception);

	*doc_exception = ES_DOC_EXCEPTION_NONE;

	if (!doc->GetLogicalDocument())
		return OpBoolean::IS_FALSE; // Probably a javascript url generating a document.

	if (!doc->GetLogicalDocument()->IsParsed()
#ifdef DELAYED_SCRIPT_EXECUTION
		|| doc->GetHLDocProfile()->ESIsExecutingDelayedScript()
#endif // DELAYED_SCRIPT_EXECUTION
		)
	{
		if (origining_runtime == doc->GetESRuntime())
		{
			ES_Thread *thread = doc->GetESScheduler()->GetCurrentThread();

			while (thread)
			{
				if (thread->Type() == ES_THREAD_INLINE_SCRIPT && thread->GetScheduler() == doc->GetESScheduler())
				{
					if (!doc->GetHLDocProfile()->GetESLoadManager()->SupportsWrite(thread))
						return OpStatus::ERR_NOT_SUPPORTED;
					/* Disabled if this is an inline script in an XML document. */
					if (doc->GetHLDocProfile()->IsXml())
					{
						*doc_exception = ES_DOC_EXCEPTION_XML_OPEN;
						return OpStatus::ERR_NOT_SUPPORTED;
					}

					return OpBoolean::IS_TRUE;
				}

				thread = thread->GetInterruptedThread();
			}
		}
	}

	/* Check if it is a script in an iframe that tries to write into an ancestor document. (see CORE-431) */
	FramesDocument *origining_parent_doc = origining_runtime->GetFramesDocument()->GetParentDoc();
	while (origining_parent_doc)
		if (origining_parent_doc == doc)
		{
			*doc_exception = ES_DOC_EXCEPTION_UNSUPPORTED_OPEN;
			return OpStatus::ERR_NOT_SUPPORTED;
		}
		else
			origining_parent_doc = origining_parent_doc->GetParentDoc();

	return OpBoolean::IS_FALSE;
}

/**
 * Other browsers don't fire onload on iframes if the load event was initiated
 * from inside that same onload event handler. Probably some kind of guard against
 * infinite recursion. In our case it will cause scripts that just keep running
 * which will use lots of CPU and make the page respond slower so we also
 * try to detect that situation and skip the onload event. See bug CORE-20563.
 */
static BOOL IsInParentOnLoadEvent(DocumentManager* child_doc, ES_Runtime *origining_runtime)
{
	if (origining_runtime->GetFramesDocument() &&
		child_doc->GetParentDoc() == origining_runtime->GetFramesDocument())
	{
		// FramesDocument* parent_doc = origining_runtime->GetFramesDocument();
		HTML_Element* frame_element = child_doc->GetFrame()->GetHtmlElement();
		ES_Thread *thread = origining_runtime->GetESScheduler()->GetCurrentThread();
		while (thread)
		{
			if (thread->Type() == ES_THREAD_EVENT)
			{
				DOM_Object* event_target_node = DOM_Utils::GetEventTarget(thread);
				if (event_target_node)
				{
					HTML_Element* event_target = DOM_Utils::GetHTML_Element(event_target_node);
					if (event_target == frame_element)
					{
						ES_ThreadInfo info = thread->GetInfo();
						if (info.data.event.type == ONLOAD)
							return TRUE;
					}
				}
			}

			thread = thread->GetInterruptedThread();
		}
	}
	return FALSE;
}



OP_STATUS FramesDocument::ESOpen(ES_Runtime *origining_runtime, const URL *url, BOOL is_reload, const uni_char *content_type, FramesDocument **opened_doc_ptr, ESDocException *doc_exception)
{
	OP_ASSERT(doc_exception);

	*doc_exception = ES_DOC_EXCEPTION_NONE;

	if (!IsContentChangeAllowed()) // Fail silently without throwing any exception.
		return OpStatus::ERR;

	if (IsCurrentDoc() && origining_runtime->GetFramesDocument() != NULL)
	{
		BOOL foreign_script = origining_runtime != GetESRuntime();
		ES_Thread *origining_thread = DOM_Utils::GetDOM_Environment(origining_runtime)->GetScheduler()->GetCurrentThread();

		if (!ESIsBeingGenerated() && es_replaced_document)
		{
			// Document is half closed (open flag=FALSE but traces of the open still
			// remaining -> We're in the middle of a close().
			// Wait for it to finish.
			OP_ASSERT(logdoc || !"Don't know what to do in case of a missing logdoc");
			if (logdoc)
			{
				logdoc->GetHLDocProfile()->GetESLoadManager()->CloseDocument(origining_thread);
				if (origining_thread->IsBlocked())
					return OpStatus::OK;
			}
		}

		OP_BOOLEAN result = IsInlineThreadInDocument(this, origining_runtime, origining_thread, doc_exception);
		RETURN_IF_ERROR(result);

		BOOL inline_script = result == OpBoolean::IS_TRUE;
		URL origining_url(origining_runtime->GetFramesDocument()->GetURL());

		if (foreign_script || !inline_script)
		{
			/* Script generating a new document. */
			DocumentManager *doc_man = GetDocManager();
			doc_man->StopLoading(FALSE);

			if (url)
				doc_man->SetCurrentURL(*url, FALSE);
			else if (!origining_url.IsEmpty())
			{
				URL unique_url = g_url_api->MakeUniqueCopy(origining_url);

				const char *content_type8 = "text/html";
				if (content_type)
					content_type8 = make_singlebyte_in_tempbuffer(content_type, uni_strlen(content_type));
				RETURN_IF_ERROR(unique_url.SetAttribute(URL::KMIME_ForceContentType, content_type8));

				// HTML 5 states that anything but text/html, should be handled as plaintext
				if (unique_url.GetAttribute(URL::KContentType) != URL_HTML_CONTENT)
					unique_url.SetAttribute(URL::KForceContentType, URL_TEXT_CONTENT);

				doc_man->SetCurrentURL(unique_url, FALSE);
			}

			doc_man->ESGeneratingDocument(origining_thread, is_reload, origining_runtime->GetFramesDocument()->GetMutableOrigin());

			FramesDocument *opened_doc = doc_man->GetCurrentDoc();

			if (opened_doc == this)
				return OpStatus::ERR_NO_MEMORY;

			if (opened_doc_ptr)
				*opened_doc_ptr = opened_doc;

			// See comment for the IsInParentOnLoadEvent() function
			BOOL generated_from_parent_onload_handler = IsInParentOnLoadEvent(GetDocManager(), origining_runtime);
			if (generated_from_parent_onload_handler)
				opened_doc->es_info.inhibit_parent_elm_onload = TRUE;

			HLDocProfile *hld_profile = opened_doc->GetHLDocProfile();
			if (hld_profile && origining_runtime->GetFramesDocument()->GetHLDocProfile())
			{
				URL *base_url = origining_runtime->GetFramesDocument()->GetHLDocProfile()->BaseURL();
				if (base_url)
					hld_profile->SetBaseURL(base_url, origining_runtime->GetFramesDocument()->GetHLDocProfile()->BaseURLString());
			}
		}
		else if (!logdoc)
		{
			DocumentManager *doc_man = GetDocManager();

			if (url)
			{
				doc_man->SetCurrentURL(*url, FALSE);
				SetUrl(*url);
			}
			else if (!origining_url.IsEmpty())
			{
				URL unique_url = g_url_api->MakeUniqueCopy(origining_url);

				const char *content_type8 = "text/html";
				if (content_type)
					content_type8 = make_singlebyte_in_tempbuffer(content_type, uni_strlen(content_type));
				RETURN_IF_ERROR(unique_url.SetAttribute(URL::KMIME_ForceContentType, content_type8));

				if (unique_url.GetAttribute(URL::KContentType) != URL_HTML_CONTENT)
					unique_url.SetAttribute(URL::KForceContentType, URL_TEXT_CONTENT);

				doc_man->SetCurrentURL(unique_url, FALSE);
				SetUrl(unique_url);
			}

			return ESStartGeneratingDocument(origining_thread);
		}
	}

	return OpStatus::OK;
}

OP_BOOLEAN FramesDocument::ESWrite(ES_Runtime *origining_runtime, const uni_char* string, unsigned string_length, BOOL end_with_newline, ESDocException* doc_exception)
{
	OP_ASSERT(string);
	OP_ASSERT(doc_exception);

	*doc_exception = ES_DOC_EXCEPTION_NONE;

	if (IsCurrentDoc() || es_generated_document)
	{
		ES_Thread *thread = DOM_Utils::GetDOM_Environment(origining_runtime)->GetScheduler()->GetCurrentThread();
		FramesDocument *target_doc = this;

		if (es_generated_document)
		{
			OP_ASSERT(es_generated_document->ESIsBeingGenerated());
			target_doc = es_generated_document;

		}
		else if (!ESIsBeingGenerated())
		{
			/* In case the insertion point is unknown (i.e., document is parsed)
			   and the writing script is or has interrupted an external script
			   (in the document to be written) document.write should be no-op.

			   This is to prevent external scripts from being able to use
			   document.write() to blow away the document by implicitly calling
			   document.open. */
			BOOL is_or_interrupts_external = FALSE;
			ES_Thread *thread_iter = thread;
			while (thread_iter)
			{
				if (thread_iter->IsExternalScript() && thread_iter->GetScheduler()->GetFramesDocument() == target_doc)
				{
					is_or_interrupts_external = TRUE;
					break;
				}

				thread_iter = thread_iter->GetInterruptedThread();
			}

			if (is_or_interrupts_external && target_doc->GetLogicalDocument()->IsParsed()
#ifdef DELAYED_SCRIPT_EXECUTION
			    && !target_doc->GetHLDocProfile()->ESIsExecutingDelayedScript()
#endif // DELAYED_SCRIPT_EXECUTION
			   )
				// Pretend all went OK
				return OpBoolean::IS_TRUE;

			/* Implicitly open document if it isn't being generated already.
			   Note that if 'thread' is an inline script in this document,
			   ESOpen() won't do anything. */
			RETURN_IF_ERROR(ESOpen(origining_runtime, NULL, FALSE, NULL, &target_doc, doc_exception));
			if (thread->IsBlocked())
				return OpBoolean::IS_FALSE;
		}

		ES_LoadManager *load_manager = target_doc->GetHLDocProfile()->GetESLoadManager();

		if (string_length > 0 || end_with_newline)
		{
			if (thread->GetBlockType() == ES_BLOCK_DOCUMENT_REPLACED)
				return OpBoolean::IS_FALSE;
			else
			{
				FramesDocElm *last_iframe = ifrm_root ? ifrm_root->LastChild() : NULL;

				OP_STATUS status = load_manager->Write(thread, string, string_length, end_with_newline);
				if (status == OpStatus::OK)
				{
					if (last_iframe != (ifrm_root ? ifrm_root->LastChild() : NULL))
						thread->Pause();

					return OpBoolean::IS_TRUE;
				}
				else
					return (OpStatus::IsMemoryError(status)) ? status : OpBoolean::IS_FALSE;
			}
		}
	}

	return OpBoolean::IS_TRUE;
}

OP_STATUS FramesDocument::ESClose(ES_Runtime *origining_runtime)
{
#ifdef DOC_THREAD_MIGRATION
	FramesDocument *target_doc = es_generated_document ? es_generated_document : this;

	if (target_doc->IsCurrentDoc() && target_doc->ESIsBeingGenerated())
		return target_doc->GetHLDocProfile()->GetESLoadManager()->CloseDocument(DOM_Utils::GetDOM_Environment(origining_runtime)->GetScheduler()->GetCurrentThread());
	else
		return OpStatus::OK;
#else // DOC_THREAD_MIGRATION
	if (IsCurrentDoc() || es_generated_document)
	{
		FramesDocument *target_doc = es_generated_document ? es_generated_document : this;

		if (target_doc->logdoc)
		{
			ES_Thread *origining_thread = origining_runtime->GetFramesDocument()->GetESScheduler()->GetCurrentThread();
			return target_doc->GetHLDocProfile()->GetESLoadManager()->CloseDocument(origining_thread);
		}
	}

	return OpStatus::OK;
#endif // DOC_THREAD_MIGRATION
}

void FramesDocument::ESSetGeneratedDocument(FramesDocument *doc)
{
	OP_ASSERT(!es_generated_document || !doc);
	OP_ASSERT(es_generated_document != doc);
	OP_ASSERT(!es_replaced_document);
	es_generated_document = doc;

	if (remove_from_cache)
		PostDelayedActionMessage();
}

void FramesDocument::ESSetReplacedDocument(FramesDocument *doc)
{
	OP_ASSERT(!es_replaced_document || !doc);
	OP_ASSERT(es_replaced_document != doc);
	OP_ASSERT(!es_generated_document);
	es_replaced_document = doc;
}

BOOL FramesDocument::ESIsBeingGenerated()
{
	return logdoc && GetHLDocProfile()->GetESLoadManager()->GetScriptGeneratingDoc();
}

OP_STATUS FramesDocument::ESStartGeneratingDocument(ES_Thread *generating_thread)
{
	OP_ASSERT(!logdoc);

	logdoc = OP_NEW(LogicalDocument, (this));
	if (!logdoc || OpStatus::IsError(logdoc->Init()))
	{
		OP_DELETE(logdoc);
		logdoc = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	// So that the document won't pick up URL-redirects (CORE-32736).
	loaded_from_url = url;
	origin->is_generated_by_opera = !IsAboutBlankURL(url) && url.GetAttribute(URL::KIsGeneratedByOpera);

	return GetHLDocProfile()->GetESLoadManager()->OpenDocument(generating_thread);
}

void FramesDocument::ESStoppedGeneratingDocument()
{
	if (es_replaced_document)
	{
		OP_ASSERT(es_replaced_document->es_generated_document == this);

		if (es_replaced_document->remove_from_cache) // couldn't be completed until now
			es_replaced_document->PostDelayedActionMessage();

		es_replaced_document->ESSetGeneratedDocument(NULL);
		es_replaced_document = NULL;

		GetDocManager()->ESGeneratingDocumentFinished();
	}
}

OP_STATUS FramesDocument::ESOpenURL(const URL &url, DocumentReferrer referrer, BOOL check_if_expired, BOOL reload, BOOL replace, const DocumentManager::OpenURLOptions &options, BOOL is_user_requested, BOOL open_in_new_window, BOOL open_in_background, BOOL user_auto_reload)
{
	/* Note about entered_by_user and URL_JAVASCRIPT: when called from
	   DOM, DOM will already have made a more qualified security check
	   than we can do here and decided to allow the operation, and
	   will, in order to disable our security check, say that the URL
	   was entered by the user (entered_by_user==WasEnteredByUser). */

	DocumentManager *docman = GetDocManager();

	if (url.IsEmpty())
		return OpStatus::OK;

	/* Ignore this request.  A script tried to navigate somewhere as a
	   result of us restoring form state during history navigation.
	   Most certainly that was not what the user intended. */
	if (IsRestoringFormState(options.origin_thread))
		return url.Type() == URL_JAVASCRIPT ? OpStatus::ERR : OpStatus::OK;

	if (url == GetURL() && url.RelName() && !reload)
	{
		// Reuse the current history position if this is done during the load phase.
		BOOL from_doc_load = DOM_Utils::IsInlineScriptOrWindowOnLoad(options.origin_thread);
		OP_BOOLEAN result = docman->JumpToRelativePosition(url, from_doc_load || replace);

		if (OpStatus::IsError(result))
			return result;
		else if (result == OpBoolean::IS_TRUE)
			return OpStatus::OK;
	}

	if (options.origin_thread)
	{
		ES_ThreadInfo info = options.origin_thread->GetOriginInfo();

		is_user_requested = is_user_requested || info.is_user_requested;
		open_in_new_window = open_in_new_window || info.open_in_new_window;
		open_in_background = open_in_background || info.open_in_background;
	}

#ifdef GADGET_SUPPORT
	if (GetWindow()->GetGadget() && url.Type() != URL_JAVASCRIPT)
	{
		if (!reload && open_in_new_window)
		{
			GetWindow()->GetGadget()->OpenURLInNewWindow(url);
			return OpStatus::OK;
		}
	}
#endif

	// Select how to load the url by checking different conditions.

	if (url.Type() == URL_JAVASCRIPT)
	{
		/* See note in beginning of function about entered_by_user. */

		if (!dom_environment && OpStatus::IsMemoryError(ConstructDOMEnvironment()))
			return OpStatus::ERR_NO_MEMORY;

		if (!dom_environment || !dom_environment->IsEnabled())
			return OpStatus::ERR;

		BOOL allow_access = options.origin_thread ? dom_environment->AllowAccessFrom(options.origin_thread) : dom_environment->AllowAccessFrom(referrer.url);

		if (options.entered_by_user != WasEnteredByUser && (reload && IsTopDocument() || !allow_access))
			return OpStatus::ERR;

		return ESOpenJavascriptURL(url, TRUE, FALSE, reload || options.is_walking_in_history, FALSE, options.origin_thread, is_user_requested, open_in_new_window, open_in_background, options.entered_by_user);
	}

	if (open_in_new_window)
		return g_windowManager->OpenURLNamedWindow(url, GetWindow(), this, -2, NULL, options.user_initiated, TRUE, open_in_background, TRUE, FALSE, options.origin_thread);

	BOOL suppress_unload_event = options.entered_by_user == WasEnteredByUser && (!es_scheduler || !es_scheduler->IsActive());

	if (!dom_environment && logdoc && !suppress_unload_event)
		if (HTML_Element *body = logdoc->GetHLDocProfile()->GetBodyElm())
			if (body->HasEventHandlerAttribute(this, ONUNLOAD))
				if (OpStatus::IsMemoryError(ConstructDOMEnvironment()))
					return OpStatus::ERR_NO_MEMORY;

	docman->StoreRequestThreadInfo(options.origin_thread);

	if (!dom_environment || !dom_environment->IsEnabled() || suppress_unload_event || !ESNeedTermination(url, reload))
	{
		URL tmp(url);
		DocumentManager::OpenURLOptions tmp_options = options;
		tmp_options.create_doc_now = FALSE;
		tmp_options.es_terminated = TRUE;
		docman->OpenURL(tmp, referrer, check_if_expired, reload, tmp_options);
		return OpStatus::OK;
	}

	// So we're going to add a delayed load to be triggered after the current
	// document has stabilized and all unload events have been processed

	ES_OpenURLAction *action;
	action = OP_NEW(ES_OpenURLAction, (docman, url, referrer.url, check_if_expired, reload, replace, options.user_initiated, options.entered_by_user, options.is_walking_in_history, user_auto_reload, options.from_html_attribute, docman->GetUseHistoryNumber()));

	if (!action)
		return OpStatus::ERR_NO_MEMORY;

	if (reload && !options.origin_thread)
		action->SetReloadFlags(docman->GetReloadDocument(), docman->GetConditionallyRequestDocument(), docman->GetReloadInlines(), docman->GetConditionallyRequestInlines());

	ES_Thread *interrupt_thread = NULL;
	ES_Thread *blocking_thread = NULL;

	if (options.origin_thread && options.origin_thread->GetScheduler()->GetFramesDocument() != this)
	{
		if (url.Type() == URL_OPERA)
			interrupt_thread = options.origin_thread;
		else
			blocking_thread = options.origin_thread;
	}

	if (GetESScheduler()->AddTerminatingAction(action, interrupt_thread, blocking_thread) == OpStatus::ERR_NO_MEMORY)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

OP_STATUS FramesDocument::ESOpenJavascriptURL(const URL &url, BOOL write_result_to_document, BOOL write_result_to_url, BOOL is_reload, BOOL want_utf8, ES_Thread* origin_thread, BOOL is_user_requested, BOOL open_in_new_window, BOOL open_in_background, EnteredByUser entered_by_user /* = NotEnteredByUser */)
{
	if (!dom_environment)
		RETURN_IF_ERROR(ConstructDOMEnvironment());
	if (!dom_environment->IsEnabled())
		return OpStatus::ERR;

	OP_ASSERT(url.Type() == URL_JAVASCRIPT);

	/* The incoming string has had its non-ASCII characters UTF-8 encoded as
	   part of the conversion to an URL object. The only bit that remains
	   is to convert the percent-encoded portions into valid and usable
	   javascript: source.

	   Those escaped octets are assumed to be UTF-8 characters..but if they're
	   not, they are interpreted as literal codepoints (see CORE-35005 for
	   details on why.)

	   Relevant testcases to consider if you alter this code can be
	   found on BTS bugs: CORE-3125, CORE-6627, CORE-8822, DSK-194990. */

	OpString decoded_source;
	const char *url_and_utf8_encoded_source = url.GetAttribute(URL::KPath).CStr();
	RETURN_IF_ERROR(UriUnescape::UnescapeJavascriptURL(decoded_source, url_and_utf8_encoded_source));
	if (decoded_source.IsEmpty())
		return OpStatus::ERR;

	return ES_JavascriptURLThread::Load(es_scheduler, decoded_source.CStr(), url, write_result_to_document, write_result_to_url, is_reload, want_utf8, origin_thread, is_user_requested, open_in_new_window, open_in_background);
}

BOOL FramesDocument::ESNeedTermination(const URL &url, BOOL reload)
{
	/* Termination is not necessary if this is just a jump inside the document or if
	   the URL is invalid or of a type that is not handled internally. */

	BOOL relative_jump = url == GetURL() && url.RelName() && !reload;
	BOOL need_termination = !relative_jump && !url.IsEmpty();

	if (need_termination)
		switch (url.Type())
		{
		case URL_MAILTO:
#ifdef M2_SUPPORT
		case URL_IRC:
#endif // M2_SUPPORT
#if defined(BREW) && defined(IMODE_EXTENSIONS)
		case URL_TEL:
#endif // BREW && IMODE_EXTENSIONS
			need_termination = FALSE;
		}

	return need_termination;
}

#ifdef XML_EVENTS_SUPPORT
void
FramesDocument::AddXMLEventsRegistration(XML_Events_Registration *registration)
{
	registration->Into(&xml_events_registrations);
}

XML_Events_Registration *
FramesDocument::GetFirstXMLEventsRegistration()
{
	return (XML_Events_Registration *) xml_events_registrations.First();
}
#endif // XML_EVENTS_SUPPORT

void FramesDocument::AddDelayedLayoutListener(DelayedLayoutListener *listener)
{
	listener->Into(&delayed_layout_listeners);
}

void FramesDocument::RemoveDelayedLayoutListener(DelayedLayoutListener *listener)
{
	listener->Out();
}

void FramesDocument::SignalAllDelayedLayoutListeners()
{
	while (DelayedLayoutListener *listener = (DelayedLayoutListener *) delayed_layout_listeners.First())
	{
		listener->LayoutContinued();

		if (listener->InList())
			listener->Out();
	}
}

/* static */void
FramesDocument::UnhookCoreViewTree(FramesDocElm* frame)
{
	/* Unhook all views from the bottom up because that is what the view code expects. */
	for (FramesDocElm* child = frame->FirstChild(); child; child = child->Suc())
		UnhookCoreViewTree(child);
	if (FramesDocument* doc = frame->GetCurrentDoc())
	{
		if (doc->ifrm_root)
			UnhookCoreViewTree(doc->ifrm_root);
		else if (doc->frm_root)
			UnhookCoreViewTree(doc->frm_root);
		doc->Undisplay(TRUE); // Will delete CoreViews created by plugins and scrollable containers.
	}

	frame->Hide(TRUE);
}

void
FramesDocument::DeleteIFrame(FramesDocElm *iframe)
{
	FramesDocument *doc = iframe->GetCurrentDoc();

	if (!doc || !doc->IsESActive(TRUE))
	{
		iframe->StopLoading(FALSE);
	}

	// This unhooking is normally done in SetAsCurrentDoc(FALSE) but we don't want to run that just yet.
	UnhookCoreViewTree(iframe);

	iframe->Under(&delete_iframe_root);
	iframe->SetIsDeleted();
	PostDelayedActionMessage();
}

BOOL
FramesDocument::DeleteAllIFrames()
{
	// First check if some iframe is executing scripts in a parent nested message loop.
	for (FramesDocElm *iframe = FramesDocElm::GetFirstFramesDocElm(&delete_iframe_root);
		iframe;
		iframe = iframe->Suc())
	{
		if (iframe->GetCurrentDoc() && iframe->GetCurrentDoc()->IsESActive(FALSE))
			return FALSE;
	}

	while (FramesDocElm *iframe = FramesDocElm::GetFirstFramesDocElm(&delete_iframe_root))
	{
		iframe->Out();
		OP_DELETE(iframe);
	}

	return TRUE;
}

OP_STATUS
FramesDocument::SetNewUrl(const URL &url)
{
	// This method is only called in extraordinary circumstances. If it would end up being run in the
	// main code path then we need to fix the quality and robustness of it.

#ifdef APPLICATION_CACHE_SUPPORT
	OP_ASSERT((!dom_environment	|| url.GetAttribute(URL::KIsApplicationCacheURL)) || !"Can't change url after scripts have already run. That would be bad");
#else
	OP_ASSERT(!dom_environment || !"Can't change url after scripts have already run. That would be bad");
#endif

	DocumentOrigin* new_origin;
	if (IsURLSuitableSecurityContext(url))
	{
		new_origin = DocumentOrigin::Make(url);
		if (!new_origin)
			return OpStatus::ERR_NO_MEMORY;
		origin->DecRef();
		origin = new_origin;
	}
	// else keep the previous security context which might be more useful than this one.

	SetUrl(url);

	// FIXME: Several DocListElms can point to this FramesDocument and all those should have their url modified/updated. Though it's most likely to happen on redirect pages and about:blank so in 99.99% of the cases there is only one DocListElm.
	GetDocManager()->FindDocListElm(this)->SetUrl(url);

	if (IsCurrentDoc() && (GetDocManager()->GetLoadStatus() == NOT_LOADING || GetDocManager()->GetLoadStatus() == DOC_CREATED))
		GetDocManager()->SetCurrentURL(url, TRUE);

	NotifyUrlChanged(this->url);

	return OpStatus::OK;
}

HTML_Element* FramesDocument::FindTargetElement()
{
	HTML_Element* target = doc->GetCurrentAreaElement();

	if (!target)
		target = doc->GetNavigationElement();

	if (!target)
		target = doc->GetFocusedElement();

	return target;
}

OP_STATUS FramesDocument::Undisplay(BOOL will_be_destroyed)
{
	OP_NEW_DBG("FramesDocument::Undisplay", "doc");
	OP_DBG((UNI_L("this=%p; document_state=%p"), this, document_state));
	OP_STATUS status = OpStatus::OK;

#ifdef WAND_SUPPORT
	if (is_wand_submitting)
		g_wand_manager->UnreferenceDocument(this);
#endif

	undisplaying = TRUE;

	if (!document_state && !will_be_destroyed && !doc_manager->GetReload() && OpStatus::IsMemoryError(DocumentState::Make(document_state, this)))
	{
		document_state = NULL;
		status = OpStatus::ERR_NO_MEMORY;
	}

#ifdef DOM_FULLSCREEN_MODE
	if (OpStatus::IsMemoryError(DOMExitFullscreen(NULL)))
		status = OpStatus::ERR_NO_MEMORY;
#endif // DOM_FULLSCREEN_MODE

	if (frm_root)
	{
		if (OpStatus::IsMemoryError(frm_root->Undisplay(will_be_destroyed)))
			status = OpStatus::ERR_NO_MEMORY;
	}
	else if (doc)
	{
		if (OpStatus::IsMemoryError(doc->UndisplayDocument()))
			status = OpStatus::ERR_NO_MEMORY;
	}
	else
		UndisplayDocument();

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	VisualDevice* vd = GetVisualDevice();
	if (vd && vd->GetAccessibleDocument())
	{
		vd->GetAccessibleDocument()->DocumentUndisplayed(this);
	}
#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#ifdef APPLICATION_CACHE_SUPPORT
	if (g_application_cache_manager && dom_environment)
		g_application_cache_manager->CacheHostDestructed(dom_environment);
#endif // APPLICATION_CACHE_SUPPORT

	undisplaying = FALSE;

	return(status);
}

void FramesDocument::OnFocusChange(FramesDocument* got_focus)
{
	DocumentTreeIterator iter(GetTopDocument());
	iter.SetIncludeThis();
	while (iter.Next())
	{
		FramesDocument* curr_doc = iter.GetFramesDocument();
		if (got_focus != curr_doc && curr_doc->GetHtmlDocument())
			curr_doc->GetHtmlDocument()->ClearFocusAndHighlight(TRUE, FALSE);
	}

#ifdef KEYBOARD_SELECTION_SUPPORT
	got_focus->GetWindow()->UpdateKeyboardSelectionMode(got_focus->GetKeyboardSelectionMode());
#endif // KEYBOARD_SELECTION_SUPPORT
}

void FramesDocument::GotKeyFocus(FOCUS_REASON reason)
{
	if (!suppress_focus_events)
		OpStatus::Ignore(HandleWindowEvent(ONFOCUS)); //Avoiding loops on OOM

#ifndef MOUSELESS
	BOOL recalculate_hover = reason == FOCUS_REASON_ACTIVATE;
	if (recalculate_hover && doc && !reflowing && !undisplaying && !need_update_hover_element)
	{
		need_update_hover_element = TRUE;
		PostDelayedActionMessage();
	}
#endif // MOUSELESS

#ifdef _MIME_SUPPORT_
	// We never want focus in the "wrapper" document used by MHTML. Forward to the
	// first and only child document if it exists yet.
	URLCacheType cache_type = static_cast<URLCacheType>(GetURL().GetAttribute(URL::KCacheType));
	if (cache_type == URL_CACHE_MHTML && g_input_manager->GetKeyboardInputContext() == GetVisualDevice())
	{
		DocumentTreeIterator it(this);
		if (it.Next())
			it.GetFramesDocument()->SetFocus(reason);
	}
#endif // _MIME_SUPPORT_
}

/* virtual */ void FramesDocument::LostKeyFocus()
{
	OpStatus::Ignore(HandleWindowEvent(ONBLUR)); //Avoiding loops on OOM

	// Forget all about pending key events to cancel if focus moves between documents.
	GetWindow()->SetRecentlyCancelledKeyDown(OP_KEY_INVALID);

#ifndef MOUSELESS
	is_moving_separator = FALSE;
#endif // !MOUSELESS
#ifndef HAS_NOTEXTSELECTION

	selecting_text_is_ok = FALSE;
	start_selection = FALSE;
#endif // !HAS_NOTEXTSELECTION
}

BOOL
FramesDocument::IsWrapperDoc()
{
	return  wrapper_doc_source.HasContent() ||
			is_plugin ||
#ifdef MEDIA_HTML_SUPPORT
			g_media_module.IsMediaPlayableUrl(url) ||
#endif // MEDIA_HTML_SUPPORT
			url.IsImage();
}

int
FramesDocument::GetNewSubWinId()
{
	if (FramesDocument *pdoc = GetParentDoc())
		return pdoc->GetNewSubWinId();
	else
		return sub_win_id_counter++;
}

DocumentReferrer
FramesDocument::GetRefURL()
{
	if (!IsURLSuitableSecurityContext(GetURL()))
	{
		// In reloads GetRefURL() will become the new origin of documents with
		// no intrinsic origin so in that case we need to use the origin to end
		// up with a good final state. In the long run http referrers and
		// security origins should be kept apart.
		return DocumentReferrer(GetMutableOrigin());
	}

	DocListElm* current_doc_elm = GetDocManager()->CurrentDocListElm();
	if (current_doc_elm)
	{
		DocListElm* iterator = current_doc_elm;
		while (iterator)
		{
			if (iterator->Doc() == this)
				return iterator->GetReferrerUrl();
			iterator = iterator->Pred();
		}
		iterator = current_doc_elm->Suc();
		while (iterator)
		{
			if (iterator->Doc() == this)
				return iterator->GetReferrerUrl();
			iterator = iterator->Suc();
		}
	}

	return DocumentReferrer();
}

static void FillBackground(VisualDevice* vis_dev, const RECT& rect, const COLORREF* color = NULL)
{
	OpRect doc_rect(&rect);

	doc_rect.x += vis_dev->GetRenderingViewX();
	doc_rect.y += vis_dev->GetRenderingViewY();

	if (color)
		vis_dev->SetBgColor(*color);
	else
		vis_dev->SetDefaultBgColor();

	vis_dev->DrawBgColor(doc_rect);
}

OP_DOC_STATUS
FramesDocument::Display(const RECT& rect, VisualDevice* vd)
{
	OP_PROBE6(OP_PROBE_FRAMESDOCUMENT_DISPLAY);

	PAINT_PROBE(probe, GetTimeline(), rect, DOMGetScrollOffset());

	OP_DOC_STATUS paint_status = DocStatus::DOC_CANNOT_DISPLAY;

#ifdef DOM_FULLSCREEN_MODE
	if (GetFullscreenElement() && !IsPrintDocument())
	{
		paint_status = DisplayFullscreenElement(rect, vd);
		if (paint_status != DocStatus::DOC_CANNOT_DISPLAY)
			return paint_status;
	}
#endif // DOM_FULLSCREEN_MODE

#ifdef _PRINT_SUPPORT_
	if (IsDisplaying(vd) == PRINT_PREVIEW_DOCUMENT) // print preview mode
		if (GetSubWinId() == -1)
		{
			COLORREF color = PRINT_PREVIEW_BG_COLOR;

			vd->Reset();
			FillBackground(vd, rect, &color);
		}
#endif

	BOOL local_keep_cleared = keep_cleared || wait_for_paint_blockers > 0;
	FramesDocument *parent_doc = GetParentDoc();

	while (!local_keep_cleared && parent_doc)
	{
		local_keep_cleared = parent_doc->keep_cleared || parent_doc->wait_for_paint_blockers > 0;
		parent_doc = parent_doc->GetParentDoc();
	}

	if (local_keep_cleared)
	{
		FillBackground(vd, rect);
		goto after_paint;
	}

	if (frm_root)
	{
		if (GetSubWinId() == -1 && !GetWindow()->IsBackgroundTransparent())
		{
			/* Paint background color on root frameset. In some cases (when the
				layout viewport is smaller than the rendering viewport) the root
				frameset also becomes smaller than the rendering viewport. */
			FillBackground(vd, rect);
		}

#ifdef _PRINT_SUPPORT_
		if (!vd->GetWindow()->GetPreviewMode())
#endif // _PRINT_SUPPORT_
		{
			frm_root->DisplayBorder(vd);
		}

#ifdef PAGED_MEDIA_SUPPORT
#ifdef _PRINT_SUPPORT_
		if (IsDisplaying(vd) == PRINT_PREVIEW_DOCUMENT) // print preview mode
		{
			DM_PrintType frames_print_type = vd->GetWindow()->GetFramesPrintType();
			int left = 0;
			int top = 0;

			if (frames_print_type == PRINT_AS_SCREEN)
			{
				// draw paper

				PrintDevice* pd = vd->GetWindow()->GetPrinterInfo(TRUE)->GetPrintDevice();
				int horizontal_dim;
				int vertical_dim;
				int bottom;
				int right;
				RECT p_rect;
				OpRect page_clip_rect;

				pd->GetUserMargins(left, right, top, bottom);

				if (FALSE /* landscape */)
				{
					horizontal_dim = pd->GetPaperHeight();
					vertical_dim = pd->GetPaperWidth();

					page_clip_rect.width = pd->GetRenderingViewHeight() - left - right;
					page_clip_rect.height = vertical_dim - pd->GetPaperLeftOffset() - bottom;
				}
				else
				{
					horizontal_dim = pd->GetPaperWidth();
					vertical_dim = pd->GetPaperHeight();

					page_clip_rect.width = pd->GetRenderingViewWidth() - left - right;
					page_clip_rect.height = vertical_dim - pd->GetPaperTopOffset() - bottom;
				}

				p_rect.left = PREVIEW_PAPER_LEFT_OFFSET;
				p_rect.top = PREVIEW_PAPER_TOP_OFFSET;
				p_rect.right = PREVIEW_PAPER_LEFT_OFFSET + horizontal_dim;
				p_rect.bottom = PREVIEW_PAPER_TOP_OFFSET + vertical_dim;

				vd->SetBgColor(OP_RGB(255,255,255));
				vd->DrawBgColor(p_rect);

				if (GetSubWinId() == -1 && g_pcprint->GetIntegerPref(PrefsCollectionPrint::ShowPrintHeader, GetHostName()))
					PrintHeaderFooter(vd,
										1,
										pd->GetPaperLeftOffset() + left / 2,
										PREVIEW_PAPER_TOP_OFFSET + pd->GetPaperTopOffset(),
										pd->GetPaperWidth() - pd->GetPaperLeftOffset() - left / 2,
										PREVIEW_PAPER_TOP_OFFSET + pd->GetPaperHeight() - pd->GetPaperTopOffset());

				left += pd->GetPaperLeftOffset() + PREVIEW_PAPER_LEFT_OFFSET;
				top += pd->GetPaperTopOffset() + PREVIEW_PAPER_TOP_OFFSET;

				vd->TranslateView(-left, -top);
				vd->BeginClipping(page_clip_rect);
			}

			frm_root->Display(vd, rect);

			if (frames_print_type == PRINT_AS_SCREEN)
			{
				vd->EndClipping();
				vd->TranslateView(left, top);
			}

		}
#endif // _PRINT_SUPPORT_
#endif // PAGED_MEDIA_SUPPORT
		paint_status = DocStatus::DOC_DISPLAYED;
		goto after_paint;
	}
	else if (doc)
	{
#ifdef PAGED_MEDIA_SUPPORT
#ifdef _PRINT_SUPPORT_
		if (IsDisplaying(vd) == PRINT_PREVIEW_DOCUMENT) // print preview mode
		{
			int view_x = vd->GetRenderingViewX();
			int view_y = vd->GetRenderingViewY();

			if (GetSubWinId() == -1)
			{
				RECT bg_rect = rect;

				bg_rect.left += view_x;
				bg_rect.right += view_x;
				bg_rect.top += view_y;
				bg_rect.bottom += view_y;

				vd->Reset();

				vd->SetBgColor(PRINT_PREVIEW_BG_COLOR);
				vd->DrawBgColor(bg_rect);
			}

			paint_status = DocStatus::DOC_DISPLAYED;

			if (vd->GetWindow()->GetFramesPrintType() == PRINT_AS_SCREEN || IsInlineFrame())
				paint_status = doc->Display(rect, vd);
			else
			{
				long y = PREVIEW_PAPER_TOP_OFFSET;
				long offset = PREVIEW_PAPER_TOP_OFFSET;
				OpRect clip_rect(&rect);
				clip_rect.OffsetBy(view_x, view_y);

				for (PageDescription* page = (PageDescription*) pages.First(); page && y < rect.bottom + view_y; page = (PageDescription*) page->Suc())
				{
					OpRect page_clip_rect(PREVIEW_PAPER_LEFT_OFFSET + page->GetPageBoxLeft() + page->GetMarginLeft(),
											y + page->GetPageBoxTop() + page->GetMarginTop(),
											page->GetPageWidth(),
											page->GetPageHeight());
					RECT p_rect;

					// Draw paper

					p_rect.left = PREVIEW_PAPER_LEFT_OFFSET;
					p_rect.top = y;
					p_rect.right = PREVIEW_PAPER_LEFT_OFFSET + page->GetSheetWidth();
					p_rect.bottom = y + page->GetSheetHeight();

					vd->SetBgColor(OP_RGB(255,255,255));
					vd->DrawBgColor(p_rect);

					if (g_pcprint->GetIntegerPref(PrefsCollectionPrint::ShowPrintHeader, GetHostName()))
						PrintPageHeaderFooter(vd, page, PREVIEW_PAPER_LEFT_OFFSET + page->GetPageBoxLeft(), page->GetPageBoxTop() + y);

					if (page_clip_rect.Intersecting(clip_rect))
					{
						vd->TranslateView(-(PREVIEW_PAPER_LEFT_OFFSET + page->GetPageBoxLeft()),
											-(offset + page->GetPageBoxTop()));

						page->PrintPage(vd, doc, rect);

						vd->TranslateView(PREVIEW_PAPER_LEFT_OFFSET + page->GetPageBoxLeft(),
											offset + page->GetPageBoxTop());
					}

					y += page->GetSheetHeight() + PREVIEW_PAPER_TOP_OFFSET;
					offset += PREVIEW_PAPER_TOP_OFFSET;
				}
			}
			goto after_paint;
		}
		else
#endif // _PRINT_SUPPORT_
#endif // PAGED_MEDIA_SUPPORT
		{
			// The background of a HTML documents will always cover the whole view
			// and this is handled by the paint method of the layout boxes,
			// but other documents needs an explicit refresh of background.

			if (GetHLDocProfile() && (GetHLDocProfile()->IsXml()
#ifdef _WML_SUPPORT_
				|| GetHLDocProfile()->IsWml()
#endif // _WML_SUPPORT_
				))
			{
				// only do this for the top document, frames, iframes and objects
				// can be transparent and should not be painted here but in the
				// painting of the HE_DOC_ROOT box
				if (GetParentDoc() == NULL && !GetWindow()->IsBackgroundTransparent())
					vd->DrawBgColor(rect);
			}

			paint_status = doc->Display(rect, vd);
#ifdef DOCUMENT_EDIT_SUPPORT
			if (document_edit)
				// Draw carets and resizing borders.
				document_edit->Paint(vd);
#endif // DOCUMENT_EDIT_SUPPORT
#if defined KEYBOARD_SELECTION_SUPPORT || defined DOCUMENT_EDIT_SUPPORT
			PaintCaret(vd);
#endif // KEYBOARD_SELECTION_SUPPORT || DOCUMENT_EDIT_SUPPORT

			if (GetSubWinId() >= 0)
			{
				DocumentManager* top_doc_man = GetDocManager()->GetWindow()->DocManager();

				if (top_doc_man)
				{
					FramesDocument* frames_doc = top_doc_man->GetCurrentDoc();

					if (frames_doc)
					{
						FramesDocument* doc = frames_doc->GetActiveSubDoc();

						if (doc == this && g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::ShowActiveFrame, GetHostName()))
							doc->GetVisualDevice()->DrawWindowBorder(TRUE);
					}
				}
			}

			goto after_paint;
		}
	}
	else
	{
		if (!GetWindow()->IsBackgroundTransparent())
			FillBackground(vd, rect);
		paint_status = DocStatus::DOC_CANNOT_DISPLAY;
		goto after_paint;
	}

after_paint:

	return paint_status;
}

void FramesDocument::HandleDocumentSizeChange()
{
	VisualDevice* vis_dev = GetVisualDevice();

	int new_width = Width();
	long new_height = Height();
	int negative_overflow = NegativeOverflow();

	vis_dev->SetDocumentSize(new_width, new_height, negative_overflow);

	if (ifrm_root)
		ifrm_root->SetSize(new_width, new_height);

	ViewportController* controller = GetWindow()->GetViewportController();
	OpViewportInfoListener* listener = controller->GetViewportInfoListener();
	listener->OnDocumentContentChanged(controller, OpViewportInfoListener::REASON_DOCUMENT_SIZE_CHANGED);

	if (IsTopDocument())
	{
		OpRect layout_viewport = GetLayoutViewport();

		listener->OnLayoutViewportSizeChanged(controller, layout_viewport.width, layout_viewport.height);
		listener->OnDocumentSizeChanged(controller, new_width + negative_overflow, new_height);
#ifdef RESERVED_REGIONS
		listener->OnDocumentContentChanged(controller, OpViewportInfoListener::REASON_RESERVED_REGIONS_CHANGED);
#endif // RESERVED_REGIONS

#ifdef PAGED_MEDIA_SUPPORT
		if (logdoc)
			if (LayoutWorkplace* workplace = logdoc->GetLayoutWorkplace())
			{
				int current_page = workplace->GetCurrentPageNumber();
				int page_count = workplace->GetTotalPageCount();

				controller->SignalPageChange(current_page, page_count);
			}
#endif // PAGED_MEDIA_SUPPORT
	}

	if (!IsLoaded())
		/* We have told VisualDevice about the new document size.
		   Perhaps it is big enough to restore the scroll position now? */

		GetDocManager()->RestoreViewport(TRUE, FALSE, FALSE);
}

int FramesDocument::Width()
{
	if (doc)
	{
		PrefsCollectionDisplay::RenderingModes rendering_mode = GetPrefsRenderingMode();

#ifdef PAGED_MEDIA_SUPPORT
#ifdef _PRINT_SUPPORT_
		if (GetWindow()->GetPreviewMode() && pages.First())
		{
			short w = 0;

			for (PageDescription* page = (PageDescription*) pages.First(); page; page = (PageDescription*) page->Suc())
				if (w < page->GetSheetWidth())
					w = page->GetSheetWidth();

			return w + PREVIEW_PAPER_LEFT_OFFSET*2;
		}
		else
#endif // _PRINT_SUPPORT_
#endif // PAGED_MEDIA_SUPPORT
			if (era_mode && layout_mode != LAYOUT_NORMAL && g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::AllowHorizontalScrollbar), GetHostName()) == 0)
				return GetLayoutViewWidth();
			else
				return doc->Width();
	}
	else
		if (frm_root)
		{
#ifdef _PRINT_SUPPORT_
			if (GetSubWinId() < 0 && GetWindow()->GetPreviewMode() && GetWindow()->GetFramesPrintType() == PRINT_AS_SCREEN)
				return GetWindow()->GetPrinterInfo(TRUE)->GetPrintDevice()->GetPaperWidth() + 2*PREVIEW_PAPER_LEFT_OFFSET;
#endif // _PRINT_SUPPORT_

			return frm_root->GetWidth();
		}
		else
			return 0;
}



int FramesDocument::NegativeOverflow()
{
	if (doc)
		return doc->NegativeOverflow();
	return 0;
}

long FramesDocument::Height()
{
	if (doc)
	{
#ifdef PAGED_MEDIA_SUPPORT
#ifdef _PRINT_SUPPORT_
		if (GetWindow()->GetPreviewMode() && pages.First())
		{
			long h = GetSubWinId() == -1 ? PREVIEW_PAPER_TOP_OFFSET : 0;

			for (PageDescription* page = (PageDescription*) pages.First(); page; page = (PageDescription*) page->Suc())
				h += page->GetSheetHeight() + PREVIEW_PAPER_TOP_OFFSET;

			return h;
		}
#endif // _PRINT_SUPPORT_

		if (CSS_IsPagedMedium(GetMediaType()) && pages.Last())
		{
			PageDescription* page = (PageDescription*) pages.Last();
			long doc_bottom = doc->Height();
			long page_bottom = page->GetPageTop() + GetVisualDevice()->GetRenderingViewHeight();

			return MAX(doc_bottom, page_bottom);
		}
#endif // PAGED_MEDIA_SUPPORT

		return GetVisualDevice()->ApplyScaleRoundingNearestUp(doc->Height());
	}
	else
		if (frm_root)
		{
			long h = 0;
#ifdef _PRINT_SUPPORT_
			if (GetWindow()->GetPreviewMode() && GetSubWinId() == -1)
			{
				if (GetWindow()->GetFramesPrintType() == PRINT_AS_SCREEN)
					return GetWindow()->GetPrinterInfo(TRUE)->GetPrintDevice()->GetPaperHeight() + 2*PREVIEW_PAPER_TOP_OFFSET;

				h = PREVIEW_PAPER_TOP_OFFSET;
			}
#endif
			VisualDevice* vis_dev = GetVisualDevice();

			return vis_dev->ScaleToDoc(vis_dev->ApplyScaleRoundingNearestUp(h)) + frm_root->GetHeight();
		}
		else
			return 0;
}

LayoutFixed FramesDocument::RootFontSize() const
{
	return logdoc ? logdoc->GetLayoutWorkplace()->GetDocRootProperties().GetRootFontSize() : LayoutFixed(0);
}

OP_STATUS FramesDocument::ReactivateDocument()
{
	RETURN_IF_ERROR(CheckRefresh());

	if (frm_root)
	{
		RETURN_IF_ERROR(frm_root->ReactivateDocument());
	}
	else if (doc)
	{
		RETURN_IF_ERROR(doc->ReactivateDocument());

#if defined (WAND_SUPPORT) && defined (PREFILLED_FORM_WAND_SUPPORT)
		if (!document_state && !g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseParanoidMailpassword))
		{
			g_wand_manager->Use(this, NO, TRUE);
		}
#endif // WAND_SUPPORT && PREFILLED_FORM_WAND_SUPPORT
	}

	if (ifrm_root)
		RETURN_IF_ERROR(ifrm_root->ReactivateDocument());

#ifdef APPLICATION_CACHE_SUPPORT
	if (g_application_cache_manager && dom_environment)
	{
		// Have to re-assosiate app cache.  May trigger cache update if manifest has changed
		RETURN_IF_ERROR(UpdateAppCacheManifest());
	}
#endif // APPLICATION_CACHE_SUPPORT

	return OpStatus::OK;
}

#ifdef _PRINT_SUPPORT_
#ifdef PAGED_MEDIA_SUPPORT
void
FramesDocument::PrintPageHeaderFooter(VisualDevice *vd, PageDescription *page, short horizontal_offset, long vertical_offset)
{
	PrintHeaderFooter(vd,
						page->GetNumber(),
						horizontal_offset + (page->GetMarginLeft() / 2) ,
						vertical_offset,
						horizontal_offset + page->GetSheetWidth() - page->GetPageBoxLeft() - page->GetPageBoxRight() - 1 - (page->GetMarginRight() / 2) ,
						vertical_offset + page->GetSheetHeight() - page->GetPageBoxTop() - page->GetPageBoxBottom() - 1);
}
#endif // PAGED_MEDIA_SUPPORT

#ifdef GENERIC_PRINTING

int FramesDocument::MakeHeaderFooterStr(const uni_char* str, uni_char* buf, int buf_len, const uni_char* url_name, const uni_char* title, int page_no, int total_pages)
{
	// sanity checks
	if (!buf)
		return 0;

	int i = 0;
	buf[0] = '\0';

	if (!str)
		return 0;

	const uni_char* tmp = str;
	while (*tmp && i < buf_len - 20)
	{
		if (*tmp == '&')
		{
			tmp++;
			if (*tmp == 'p' || *tmp == 'P')
			{
				uni_snprintf(buf + i, 10, UNI_L("%d"), (*tmp == 'p') ? page_no : total_pages);
				buf[i + 10] = 0; // Buggy snprintf
			}
			else if (*tmp == 't' || *tmp == 'T')
			{
				// write current time
				struct tm* dt = op_localtime(print_time);
				g_oplocale->op_strftime(buf+i, buf_len-i, UNI_L("%X"), dt);
			}
			else if (*tmp == 'd' || *tmp == 'D')
			{
				// write current date
				struct tm* dt = op_localtime(print_time);
				g_oplocale->op_strftime(buf+i, buf_len-i, UNI_L("%x"), dt);
			}
			else if (*tmp == 'w' || *tmp == 'u')
			{
				// write title or url
				const uni_char* val = (*tmp == 'w') ? title : url_name;
				if (val && (int) uni_strlen(val) < buf_len-i)
					uni_strlcpy(buf+i, val, buf_len-i);
			}
			else
			{
				buf[i] = *tmp;
				buf[i+1] = '\0';
			}
			tmp++;
			i = uni_strlen(buf);
		}
		else
		{
			buf[i++] = *tmp++;
		}
	}
	buf[i] = '\0';
	return i;
}

#else // GENERIC_PRINTING

static OP_STATUS GetLanguageStringHelper(Str::LocaleString num, OpString &string)
{
	TRAPD(status, g_languageManager->GetStringL(num, string));
	return status;
}

#endif // GENERIC_PRINTING

void
FramesDocument::PrintHeaderFooter(VisualDevice *vd, int page_num, int left_offset, int top_offset, int right_offset, int bottom_offset)
{
	vd->Reset();

	Style* style = styleManager->GetStyle(HE_DOC_ROOT);
	PresentationAttr p_attr = style->GetPresentationAttr();
	WritingSystem::Script script =
#ifdef FONTSWITCHING
		GetHLDocProfile() ?	GetHLDocProfile()->GetPreferredScript() :
#endif // FONTSWITCHING
		WritingSystem::Unknown;
	PresentationAttr::PresentationFont p_font = p_attr.GetPresentationFont(script);
	int font_number = styleManager->GetFontNumber(p_font.Font->GetFaceName());

	const FontAtt& old_font_att = vd->GetFontAtt();
	vd->SetFont(font_number);

	COLORREF old_color = vd->GetColor();
	vd->SetColor(0, 0, 0);

	bottom_offset -= vd->GetFontHeight();

	TempBuffer title_tempbuf;
	const uni_char* title = Title(&title_tempbuf);
	// Eventually it would be nice to unescape the url to make it more readable, but then we need to
	// get the decoding to do it correctly with respect to charset encodings. See bug 250545
	const uni_char* url_name = GetURL().GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped).CStr();

	if (!print_time)
	{
		print_time = OP_NEW(time_t, ());
		op_time(print_time);
	}

	const uni_char*	topleftstring = NULL;
	uni_char*		toprightstring = NULL;
	const uni_char*	bottomleftstring = NULL;
	uni_char*		bottomrightstring = NULL;
	UINT32			toprightlength = 0;
	UINT32			bottomrightlength = 0;

#ifdef GENERIC_PRINTING
	uni_char tlstr[512]; /* ARRAY OK 2008-02-28 jl */
	uni_char trstr[512]; /* ARRAY OK 2008-02-28 jl */
	uni_char blstr[512]; /* ARRAY OK 2008-02-28 jl */
	uni_char brstr[512]; /* ARRAY OK 2008-02-28 jl */
	OpString lhs;
	OpString rhs;
	OpString lfs;
	OpString rfs;

	if (OpStatus::IsSuccess(lhs.Set(g_pcprint->GetStringPref(PrefsCollectionPrint::PrintLeftHeaderString))) &&
		OpStatus::IsSuccess(rhs.Set(g_pcprint->GetStringPref(PrefsCollectionPrint::PrintRightHeaderString))) &&
		OpStatus::IsSuccess(lfs.Set(g_pcprint->GetStringPref(PrefsCollectionPrint::PrintLeftFooterString))) &&
		OpStatus::IsSuccess(rfs.Set(g_pcprint->GetStringPref(PrefsCollectionPrint::PrintRightFooterString))))
	{
		int current_page_number = page_num;
		int total_pages = CountPages();
		MakeHeaderFooterStr(lhs.CStr(), tlstr, 512, url_name, title, current_page_number, total_pages);
		MakeHeaderFooterStr(rhs.CStr(), trstr, 512, url_name, title, current_page_number, total_pages);
		MakeHeaderFooterStr(lfs.CStr(), blstr, 512, url_name, title, current_page_number, total_pages);
		MakeHeaderFooterStr(rfs.CStr(), brstr, 512, url_name, title, current_page_number, total_pages);
		if (*tlstr) topleftstring = tlstr;
		if (*trstr) toprightstring = trstr;
		if (*blstr) bottomleftstring = blstr;
		if (*brstr) bottomrightstring = brstr;
	}
#else // GENERIC_PRINTING
	OpString translated_string;
	uni_char page_string[128]; /* ARRAY OK 2008-02-28 jl */

	OP_STATUS err = GetLanguageStringHelper(Str::SI_PAGE_TEXT, translated_string);
	OpStatus::Ignore(err); // FIXME: OOM

	uni_snprintf(page_string, 128, translated_string.CStr(), page_num);
	page_string[127] = 0;

	uni_char time_string[128]; /* ARRAY OK 2008-02-28 jl */
	struct tm *local_time = op_localtime(print_time);
	g_oplocale->op_strftime(time_string, 128, UNI_L("%c"), local_time);

	topleftstring = title;
	toprightstring = page_string;
	bottomleftstring = url_name;
	bottomrightstring = time_string;
#endif // GENERIC_PRINTING

	if (toprightstring)
	{
		toprightlength = vd->GetFontStringWidth((const uni_char *) toprightstring, uni_strlen(toprightstring));
		vd->TxtOut(right_offset - toprightlength, top_offset, toprightstring, uni_strlen(toprightstring));
	}

	if (topleftstring)
	{
		INT32 available = right_offset - left_offset;
		if (toprightstring)
			available -= toprightlength + 10;
		size_t top_length = uni_strlen(topleftstring);

		BOOL cut = FALSE;
		OpString16 cut_string;

		while (top_length && vd->GetFontStringWidth((const uni_char*) topleftstring, top_length) > (UINT32) available)
		{
			--top_length;
			cut = TRUE;
		}

		if (cut && top_length > 3)
		{
			if (OpStatus::IsError(cut_string.Set(topleftstring, top_length)))
				goto end;

			if (OpStatus::IsError(cut_string.Insert(top_length - 3, UNI_L("..."))))
				goto end;

			topleftstring = cut_string.CStr();
		}

		vd->TxtOut(left_offset, top_offset, topleftstring, top_length);

	}

	if (bottomrightstring)
	{
		bottomrightlength = vd->GetFontStringWidth((const uni_char *) bottomrightstring, uni_strlen(bottomrightstring));
		vd->TxtOut(right_offset - bottomrightlength, bottom_offset, bottomrightstring, uni_strlen(bottomrightstring));
	}

	if (bottomleftstring)
	{
		// Clipping might cut a letter in half. We want full characters, but we
		// don't want to overwrite the right hand part with the left hand part
		INT32 available = right_offset - left_offset;
		if (bottomrightstring)
			available -= bottomrightlength+10; // arbitrary min margin between the strings

		size_t bottom_length = uni_strlen(bottomleftstring);

		BOOL cut = FALSE;
		OpString16 cut_string;

		while (bottom_length && (INT32) vd->GetFontStringWidth((const uni_char *) bottomleftstring, bottom_length) > available)
		{
			// One less character at a time. Binary search would be quicker, but more complex.
			--bottom_length;
			cut = TRUE;
		}

		if (cut && bottom_length > 3)
		{
			if (OpStatus::IsError(cut_string.Set(bottomleftstring, bottom_length)))
				goto end;

			if (OpStatus::IsError(cut_string.Insert(bottom_length - 3, UNI_L("..."))))
				goto end;

			bottomleftstring = cut_string.CStr();
		}

		vd->TxtOut(left_offset, bottom_offset, bottomleftstring, bottom_length);
	}

end:

	vd->SetFont(old_font_att);
	vd->SetColor(old_color);
}

#ifdef PAGED_MEDIA_SUPPORT
OP_DOC_STATUS
PageDescription::PrintPage(VisualDevice* vd, HTML_Document* doc, const RECT& rect)
{
	OP_ASSERT(vd);
	OP_ASSERT(doc);

	OP_DOC_STATUS status = OpStatus::OK;
	Style* style = styleManager->GetStyle(HE_DOC_ROOT);
	PresentationAttr p_attr = style->GetPresentationAttr();
	WritingSystem::Script script = WritingSystem::Unknown;
#ifdef FONTSWITCHING
	if (doc->GetFramesDocument()->GetHLDocProfile())
		script = doc->GetFramesDocument()->GetHLDocProfile()->GetPreferredScript();
#endif // FONTSWITCHING
	PresentationAttr::PresentationFont p_font = p_attr.GetPresentationFont(script);
	int font_number = styleManager->GetFontNumber(p_font.Font->GetFaceName());

	DM_PrintType frames_print_type = doc->GetWindow()->GetFramesPrintType();

	vd->Reset();
	vd->SetFont(font_number);

	if (frames_print_type != PRINT_AS_SCREEN)
	{
		OpRect clip_rect(GetMarginLeft(),
						 GetPageTop() + GetMarginTop(),
						 GetPageWidth(),
						 GetPageHeight());

		vd->BeginClipping(clip_rect);

		vd->TranslateView(-GetMarginLeft(), -GetMarginTop());
	}

	doc->GetFramesDocument()->SetCurrentPage(this);

	/* Actual printing is done from a printing message callback, and is not
	   initiated the traditional way. This means that we need to go through OnPaint
	   here. For print preview, we already come from OnPaint when we get here. */

	if (vd->IsPrinter())
	{
		OpRect r(&rect);
		OpPainter* p = const_cast<OpPainter*>((static_cast<PrintDevice*>(vd))->GetPrinterController()->GetPainter());
		vd->OnPaint(r, p);
	}
	else
	{
		status = doc->Display(rect, vd);
	}

	if (frames_print_type != PRINT_AS_SCREEN)
	{
		vd->TranslateView(GetMarginLeft(), GetMarginTop());
		vd->EndClipping();
	}


	return status;
}
#endif // PAGED_MEDIA_SUPPORT

OP_DOC_STATUS
FramesDocument::PrintPage(PrintDevice* pd, int page_num, BOOL selected_only)
{
#ifdef PAGED_MEDIA_SUPPORT
	if (doc)
	{
		DM_PrintType frames_print_type = GetDocManager()->GetWindow()->GetFramesPrintType();

		if (IsInlineFrame())
			frames_print_type = PRINT_AS_SCREEN;

		PageDescription* page = GetPage((frames_print_type != PRINT_AS_SCREEN || IsInlineFrame()) ? page_num : 1);

		if (page)
		{
			if (frames_print_type != PRINT_AS_SCREEN)
			{
				if (g_pcprint->GetIntegerPref(PrefsCollectionPrint::ShowPrintHeader, GetHostName()))
				{
					OpRect header_clip_rect(0, 0,
											page->GetSheetWidth(),
											page->GetSheetHeight());
					OP_NEW_DBG("prepare", "header");
					OP_DBG(("clip_rect: ") << header_clip_rect);

					pd->BeginClipping(header_clip_rect);
					PrintPageHeaderFooter(pd, page, 0, 0);
					pd->EndClipping();
				}
				pd->TranslateView(0, page->GetPageTop());
			}

			if (frames_print_type != PRINT_AS_SCREEN && pd->RequireBanding())
			{
				RECT rect;

				while (pd->NextBand(rect))
					page->PrintPage(pd, doc, rect);
			}
			else
			{
				RECT rect;

				if (frames_print_type == PRINT_AS_SCREEN)
				{
					rect.top = 0;
					rect.left = 0;
					rect.right = page->GetPageWidth();
					rect.bottom = page->GetPageHeight();
				}
				else
				{
					rect.top = page->GetMarginTop();
					rect.left = page->GetMarginLeft();
					rect.right = page->GetMarginLeft() + page->GetPageWidth();
					rect.bottom = page->GetMarginTop() + page->GetPageHeight();
				}

				page->PrintPage(pd, doc, rect);
			}

			if (frames_print_type != PRINT_AS_SCREEN)
				pd->TranslateView(0, -page->GetPageTop());

			return DocStatus::DOC_DISPLAYED;
		}

		return DocStatus::DOC_PAGE_OUT_OF_RANGE;
	}
	else
		if (frm_root)
		{
			if (GetDocManager()->GetWindow()->GetFramesPrintType() == PRINT_AS_SCREEN)
			{
				RECT rect;

				int top, left, right, bottom;
				pd->GetUserMargins(left, right, top, bottom);

				if (GetSubWinId() == -1 && g_pcprint->GetIntegerPref(PrefsCollectionPrint::ShowPrintHeader, GetHostName()))
				{
					OpRect header_clip_rect(0, 0,
											pd->GetPaperWidth(),
											pd->GetPaperHeight());
					pd->BeginClipping(header_clip_rect);
					pd->TranslateView(0, pd->GetPaperTopOffset());
					pd->Translate(0, pd->GetPaperTopOffset());

					PrintHeaderFooter(pd,
									  1,
									  pd->GetPaperLeftOffset() + left / 2,
									  pd->GetPaperTopOffset(),
									  pd->GetPaperWidth() - pd->GetPaperLeftOffset() - left / 2,
									  pd->GetPaperHeight() - pd->GetPaperTopOffset());

					pd->TranslateView(0, -pd->GetPaperTopOffset());
					pd->Translate(0, -pd->GetPaperTopOffset());
					pd->EndClipping();
				}

				pd->TranslateView(-left, -top);

				if (pd->RequireBanding())
					while (pd->NextBand(rect))
						frm_root->Display(pd, rect);
				else
				{
					rect.top = 0;
					rect.left = 0;
					rect.right = frm_root->GetWidth();
					rect.bottom = frm_root->GetHeight();

					frm_root->Display(pd, rect);
				}

				pd->TranslateView(left, top);

				return DocStatus::DOC_PRINTED;
			}
			else
				return frm_root->PrintPage(pd, page_num, selected_only);
		}
		else
			return DocStatus::DOC_CANNOT_PRINT;
#else // PAGED_MEDIA_SUPPORT
	return DocStatus::DOC_CANNOT_PRINT;
#endif // PAGED_MEDIA_SUPPORT
}
#endif // _PRINT_SUPPORT_

#ifdef PAGED_MEDIA_SUPPORT
int FramesDocument::CountPages()
{
	if (doc)
	{
		int p = 0;

		for (PageDescription* page = (PageDescription*) pages.First(); page; page = (PageDescription*) page->Suc())
			p++;

		return p;
	}
	else
		if (frm_root)
		{
#ifdef _PRINT_SUPPORT_
			if (GetDocManager()->GetWindow()->GetFramesPrintType() == PRINT_AS_SCREEN)
				return 1;
			else
#endif // _PRINT_SUPPORT_
				return frm_root->CountPages();
		}
		else
			return 0;
}
#endif // PAGED_MEDIA_SUPPORT

void FramesDocument::OnRenderingViewportChanged(const OpRect &rendering_rect)
{
	// if we are using smart_frames or stacked_frames, the whole frame is blewn up to cover the document
	// which results in that DecVisible is not called on images that are gone out of view, this code
	// propagates the visible rect to all sub frames, which makes it possible to throw out invisible images

	if (frm_root)
		frm_root->OnRenderingViewportChanged(rendering_rect);
	else
	{
		if (ifrm_root)
			ifrm_root->OnRenderingViewportChanged(rendering_rect);

#ifdef SVG_SUPPORT
		if (logdoc)
			logdoc->GetSVGWorkplace()->RegisterVisibleArea(rendering_rect);
#endif // SVG_SUPPORT

#ifdef _SPAT_NAV_SUPPORT_
		if (OpSnHandler* sn_handler = GetWindow()->GetSnHandler())
			sn_handler->OnScroll();
#endif // _SPAT_NAV_SUPPORT_
		LoadInlineElmHashIterator iterator(inline_hash);
		for (LoadInlineElm* lie = iterator.First(); lie; lie = iterator.Next())
		{
			for (HEListElm* hle = lie->GetFirstHEListElm(); hle; hle = hle->Suc())
			{
				if ((hle->IsImageInline()
#ifdef HAS_ATVEF_SUPPORT
					 || hle->IsImageAtvef()
#endif
						) && hle->GetImageVisible() && hle->HElm()->GetLayoutBox())
				{
					Box* layout_box = hle->HElm()->GetLayoutBox();
					OpRect inline_bbox(0, 0, layout_box->GetWidth(), layout_box->GetHeight());
					hle->GetImagePos().Apply(inline_bbox);

					// Check if this element is within the rendering viewport.

					if (inline_bbox.Intersecting(rendering_rect))
					{
						hle->Display(this,
									 hle->GetImagePos(),
									 hle->GetImageWidth(),
									 hle->GetImageHeight(),
									 hle->IsBgImageInline(),
									 FALSE);
					}
					else
					{
						hle->Undisplay();
					}
				}
			}
		}
	}
}

/* static */
BOOL FramesDocument::GetNewScrollPos(OpRect rect_to_scroll_to, UINT32 current_scroll_xpos, UINT32 current_scroll_ypos,
									INT32 total_scrollable_width, INT32 total_scrollable_height,
									INT32 visible_width, INT32 visible_height,
									INT32& new_scroll_xpos, INT32& new_scroll_ypos,
									SCROLL_ALIGN align, BOOL is_rtl_doc)
{
	BOOL changed = FALSE;

	// for rect_to_scroll_tos larger than view, scroll so rect_to_scroll_to is (almost) in upper left corner
	INT w, h;
	if (rect_to_scroll_to.width > visible_width)
		w = (int)(visible_width * 0.8);
	else
		w = rect_to_scroll_to.width;

	if (rect_to_scroll_to.height > visible_height)
		h = (int)(visible_height * 0.8);
	else
		h = rect_to_scroll_to.height;

	new_scroll_xpos = INT32(current_scroll_xpos);
	new_scroll_ypos = INT32(current_scroll_ypos);

	switch (align)
	{
	case ::SCROLL_ALIGN_CENTER:
		{
			if (rect_to_scroll_to.x < INT32(current_scroll_xpos) || rect_to_scroll_to.x + rect_to_scroll_to.width > INT32(current_scroll_xpos + visible_width))
			{
				new_scroll_xpos = rect_to_scroll_to.x + w/2 - INT32(visible_width / 2);
				new_scroll_xpos = (INT32)MAX(0, MIN((int)new_scroll_xpos, (int)(total_scrollable_width - visible_width)));
				changed = TRUE;
			}

			if (rect_to_scroll_to.y < INT32(current_scroll_ypos) || rect_to_scroll_to.y + rect_to_scroll_to.height > INT32(current_scroll_ypos + visible_height))
			{
				new_scroll_ypos = rect_to_scroll_to.y + h/2 - INT32(visible_height / 2);
				new_scroll_ypos = (INT32)MAX(0, MIN((int)new_scroll_ypos, (int)(total_scrollable_height - visible_height)));
				changed = TRUE;
			}
		}
		break;

#if defined(DOCUMENT_EDIT_SUPPORT) || defined(KEYBOARD_SELECTION_SUPPORT)
	case ::SCROLL_ALIGN_NEAREST:
		{
			if (rect_to_scroll_to.x < INT(current_scroll_xpos))
			{
				new_scroll_xpos = rect_to_scroll_to.x;
				changed = TRUE;
			}
			if (rect_to_scroll_to.y < INT(current_scroll_ypos))
			{
				new_scroll_ypos = rect_to_scroll_to.y;
				changed = TRUE;
			}
			if (rect_to_scroll_to.x + rect_to_scroll_to.width > INT(current_scroll_xpos + visible_width))
			{
				new_scroll_xpos = is_rtl_doc ? rect_to_scroll_to.x + rect_to_scroll_to.width - visible_width : rect_to_scroll_to.x;
				changed = TRUE;
			}
			if (rect_to_scroll_to.y + rect_to_scroll_to.height > INT(current_scroll_ypos + visible_height))
			{
				new_scroll_ypos = rect_to_scroll_to.y + rect_to_scroll_to.height - visible_height;
				changed = TRUE;
			}
		}
		break;
#endif // DOCUMENT_EDIT_SUPPORT || KEYBOARD_SELECTION_SUPPORT

	case ::SCROLL_ALIGN_SPOTLIGHT:
		{
			new_scroll_ypos = rect_to_scroll_to.y - SPOTLIGHT_MARGIN;

#if defined SKIN_SUPPORT && defined SKIN_OUTLINE_SUPPORT
			OpSkinElement* skin_elm = g_skin_manager->GetSkinElement("Active Element Inside");
			if (skin_elm)
			{
				INT32 padding;
				skin_elm->GetPadding(&padding, &padding, &padding, &padding, 0);
				new_scroll_ypos -= padding;

				INT32 border_width;
				skin_elm->GetBorderWidth(&border_width, &border_width, &border_width, &border_width, 0);

				new_scroll_ypos -= border_width;

				if (new_scroll_ypos < 0)
					new_scroll_ypos = 0;
			}
#endif // SKIN_SUPPORT && SKIN_OUTLINE_SUPPORT

			if (rect_to_scroll_to.x + rect_to_scroll_to.width > INT(current_scroll_xpos + visible_width))
				new_scroll_xpos = is_rtl_doc ? rect_to_scroll_to.x + rect_to_scroll_to.width - visible_width : rect_to_scroll_to.x;

			changed = TRUE;
		}
		break;
	case ::SCROLL_ALIGN_TOP:
		{
			new_scroll_ypos = rect_to_scroll_to.y;

			if (rect_to_scroll_to.x + rect_to_scroll_to.width > INT(current_scroll_xpos + visible_width))
				new_scroll_xpos = is_rtl_doc ? rect_to_scroll_to.x + rect_to_scroll_to.width - visible_width : rect_to_scroll_to.x;

			changed = TRUE;
		}
		break;

	case ::SCROLL_ALIGN_LAZY_TOP:
		{
			// Do not scroll if the rectangle to scroll to is visible at least partially
			if (rect_to_scroll_to.x + rect_to_scroll_to.width <= INT32(current_scroll_xpos) || rect_to_scroll_to.x >= INT32(current_scroll_xpos + visible_width))
			{
				new_scroll_xpos = is_rtl_doc ? rect_to_scroll_to.x + rect_to_scroll_to.width - visible_width : rect_to_scroll_to.x;
				changed = TRUE;
			}
			if (rect_to_scroll_to.y + rect_to_scroll_to.height <= INT32(current_scroll_ypos) || rect_to_scroll_to.y >= INT32(current_scroll_ypos + visible_height))
			{
				new_scroll_ypos = rect_to_scroll_to.y;
				changed = TRUE;
			}
		}
		break;

	case ::SCROLL_ALIGN_BOTTOM:
		{
			new_scroll_ypos = rect_to_scroll_to.y + rect_to_scroll_to.height - int(visible_height);

			if (rect_to_scroll_to.x + rect_to_scroll_to.width > INT(current_scroll_xpos + visible_width))
				new_scroll_xpos = is_rtl_doc ? rect_to_scroll_to.x + rect_to_scroll_to.width - visible_width : rect_to_scroll_to.x;

			changed = TRUE;
		}
		break;
	}

	if (new_scroll_ypos < 0 && rect_to_scroll_to.y+rect_to_scroll_to.height-1 >= 0)
		new_scroll_ypos = 0;

	return changed;
}

BOOL FramesDocument::ScrollToRect(const OpRect& rect, SCROLL_ALIGN align, BOOL strict_align, OpViewportChangeReason reason, HTML_Element *scroll_to)
{
#ifdef DISABLE_SCROLL_TO_RECT
	return FALSE;
#else

	OpRect target_rect(rect);

	if (scroll_to)
	{
		BOOL is_rtl_doc = FALSE;
#ifdef SUPPORT_TEXT_DIRECTION
		LogicalDocument* logdoc = GetLogicalDocument();
		if (logdoc)
			is_rtl_doc = logdoc->GetLayoutWorkplace()->IsRtlDocument();
#endif //SUPPORT_TEXT_DIRECTION

		// If a scrollable container cannot be scrolled to the element according
		// to alignment, try to adjust by scrolling the parent scrollable container instead.
		// Don't allow deep nesting of scrollable containers - GetRect() is heavy operation.
		Content* content = Content::FindParentOverflowContent(scroll_to);
		for (int cont_i = 0; content && cont_i < MAX_NESTING_OVERFLOW_CONTAINER; cont_i++)
		{
			if (ScrollableArea* scrollable = content->GetScrollable())
			{
				// Element is a child of a scrollable container.
				// Scroll the container(s), and then the document view to the outermost container.
				RECT scroll_cont_rect;
				AffinePos scroll_cont_ctm;
				if (!scrollable->GetOwningBox()->GetRect(this, PADDING_BOX, scroll_cont_ctm, scroll_cont_rect))
					break;

				// scroll_cont_target_rect is target_rect but in the coordinate
				// system of the scrollable container.
				OpRect scroll_cont_target_rect = target_rect;
				scroll_cont_ctm.ApplyInverse(scroll_cont_target_rect);

				scroll_cont_target_rect.OffsetBy(-scroll_cont_rect.left, -scroll_cont_rect.top);

				scroll_cont_target_rect.x += scrollable->GetViewX();
				scroll_cont_target_rect.y += scrollable->GetViewY();

				int visible_width = scroll_cont_rect.right - scroll_cont_rect.left - scrollable->GetVerticalScrollbarWidth();
				int visible_height = scroll_cont_rect.bottom - scroll_cont_rect.top - scrollable->GetHorizontalScrollbarWidth();
				INT32 new_xpos, new_ypos;
				BOOL changed = GetNewScrollPos(scroll_cont_target_rect, scrollable->GetViewX(),
											   scrollable->GetViewY(),
											   scrollable->GetScrollPaddingBoxWidth(),
											   scrollable->GetScrollPaddingBoxHeight(),
											   visible_width,
											   visible_height,
											   new_xpos, new_ypos,
											   align, is_rtl_doc);
				if (changed)
				{
					// Scroll the scrollable div and adjust the coordinates of the
					// rect we want to make visible since it has moved.
					LayoutCoord prev_view_x = scrollable->GetViewX();
					LayoutCoord prev_view_y = scrollable->GetViewY();
					scrollable->SetViewX(LayoutCoord(new_xpos), TRUE);
					scrollable->SetViewY(LayoutCoord(new_ypos), TRUE);
					target_rect.x += prev_view_x - scrollable->GetViewX();
					target_rect.y += prev_view_y - scrollable->GetViewY();
				}
			}
#ifdef PAGED_MEDIA_SUPPORT
			else if (PagedContainer* paged_cont = content->GetPagedContainer())
				if (scroll_to->GetLayoutBox())
				{
					RECT border_rect;
					AffinePos ctm;

					if (!paged_cont->GetPlaceholder()->GetRect(this, BORDER_BOX, ctm, border_rect))
						break;

					unsigned int first_page;
					unsigned int last_page;
					unsigned int old_page = paged_cont->GetCurrentPageNumber();
					OpRect rect_in_paged(target_rect);

					/* Note that this isn't going to work perfectly for transforms. target_rect
					   may have been transformed from some local coordinate system to the top
					   level coordinate system, which may have enlarged it, making it contain
					   uninteresting parts. This happens e.g. if rotation is 45 degrees, and
					   that may take us to the wrong page. The problem is similar for
					   scrollable containers, but it's more noticable when it happens with
					   paged containers. */

					rect_in_paged.OffsetBy(-border_rect.left, -border_rect.top);
					ctm.ApplyInverse(rect_in_paged);
					paged_cont->GetPagesContaining(rect_in_paged, first_page, last_page);

# if defined(DOCUMENT_EDIT_SUPPORT) || defined(KEYBOARD_SELECTION_SUPPORT)
					if (align == SCROLL_ALIGN_NEAREST)
					{
						if (old_page < first_page)
							paged_cont->SetCurrentPageNumber(first_page);
						else if (old_page > last_page)
							paged_cont->SetCurrentPageNumber(last_page);
					}
					else
# endif // DOCUMENT_EDIT_SUPPORT || KEYBOARD_SELECTION_SUPPORT
						if (align == SCROLL_ALIGN_CENTER)
							paged_cont->SetCurrentPageNumber((first_page + last_page) / 2);
						else if (align == SCROLL_ALIGN_BOTTOM)
							paged_cont->SetCurrentPageNumber(last_page);
						else
							paged_cont->SetCurrentPageNumber(first_page);

					unsigned int new_page = paged_cont->GetCurrentPageNumber();

					if (new_page != old_page)
					{
						OpPoint old_page_point(paged_cont->GetViewX(old_page), paged_cont->GetViewY(old_page));
						OpPoint new_page_point(paged_cont->GetViewX(new_page), paged_cont->GetViewY(new_page));

						ctm.Apply(old_page_point);
						ctm.Apply(new_page_point);

						target_rect.x -= new_page_point.x - old_page_point.x;
						target_rect.y -= new_page_point.y - old_page_point.y;
					}
				}
#endif // PAGED_MEDIA_SUPPORT

			HTML_Element *parent = content->GetHtmlElement();
			scroll_to = parent;
			content = Content::FindParentOverflowContent(parent);
		}
	}

	BOOL changed = RequestScrollIntoView(target_rect, align, strict_align, reason);

#ifdef SVG_SUPPORT
	if (scroll_to && logdoc)
		if (HTML_Element* elm = logdoc->GetDocRoot())
			if (elm->GetNsType() == NS_SVG)
				g_svg_manager->ScrollToRect(target_rect, align, scroll_to);
#endif // SVG_SUPPORT

	return changed;
#endif // DISABLE_SCROLL_TO_RECT
}

#ifdef SCROLL_TO_ACTIVE_ELM
void FramesDocument::ScrollToActiveElement()
{
	if (!doc || !logdoc)
		return;

	/* If the window is resized while we have a focused element or a highlighed
	   element, attempt to make sure that the this element is inside the visual
	   viewport after the resize. */

	HTML_Element *element = doc->GetFocusedElement();

	if (!element)
		element = doc->GetNavigationElement();

	if (element)
	{
# if !defined (HAS_NO_SEARCHTEXT) && defined (SEARCH_MATCHES_ALL)
		int offset = 0;
		SelectionElm *s_elm = doc->GetSelectionElmFromHe(element);
		TextSelection *ts = (s_elm != NULL) ? s_elm->GetSelection() : NULL;
		if (ts && element->Content())
			offset = ts->GetStartSelectionPoint().GetOffset();

		if (offset != 0)
			doc->SaveScrollToElement(element, offset);
		else
			doc->SaveScrollToElement(element);
# else
		doc->SaveScrollToElement(element);
# endif // !HAS_NO_SEARCHTEXT && SEARCH_MATCHES_ALL

		if (logdoc->GetLayoutWorkplace()->IsTraversable())
		{
			// Document is traversable; there's no pending reflow. Scroll right away.

			doc->ScrollToSavedElement();
		}
	}
}
#endif // SCROLL_TO_ACTIVE_ELM

#ifdef PAGED_MEDIA_SUPPORT
/* virtual */ PageDescription*
FramesDocument::GetPageDescription(long y, BOOL create_page /* = FALSE */)
{
	if (IsInlineFrame())
		return GetParentDoc()->GetPageDescription(y, create_page);

	// If we don't have paged media, we should bail out immediately

	PageDescription* page = (PageDescription*) pages.First();

	while (page)
	{
		if (y < page->GetPageTop() + page->GetSheetHeight())
			return page;

		page = (PageDescription*) page->Suc();
	}

	if (!create_page)
		return NULL;

	for (;;)
	{
		PageDescription* new_page = CreatePageDescription(page);

		if (!new_page)
			return NULL;
		else
			if (new_page->GetPageTop() > y)
				return page;
			else
				if (new_page->GetPageBottom() > y)
					return new_page;
				else
					page = new_page;

	}
}

PageDescription* FramesDocument::CreatePageDescription(PageDescription* prev_page)
{
	if (IsInlineFrame())
		return GetParentDoc()->CreatePageDescription(prev_page);

	PageDescription* new_page;

	if (!prev_page)
		prev_page = (PageDescription*) pages.Last();

	new_page = OP_NEW(PageDescription, (prev_page ? prev_page->GetPageTop() + prev_page->GetSheetHeight() : 0));

	if (new_page)
	{
		if (prev_page)
			new_page->Follow(prev_page);
		else
			new_page->Into(&pages);

		ClearPage(new_page);
	}

	return new_page;
}

void FramesDocument::ClearPage(PageDescription* new_page)
{
	if (IsInlineFrame())
	{
		GetParentDoc()->ClearPage(new_page);
		return;
	}

	short page_box_width = GetVisualDevice()->GetRenderingViewWidth();
	long page_box_height = LONG_MAX >> 1;
	short sheet_width = page_box_width;
	long sheet_height = page_box_height;
	LayoutCoord margin_left(0);
	LayoutCoord margin_right(0);
	LayoutCoord margin_top(0);
	LayoutCoord margin_bottom(0);

#ifdef _PRINT_SUPPORT_
	if (GetVisualDevice()->IsPrinter())
	{
		int current_page_number = new_page->GetNumber();
		PrintDevice* pd = (PrintDevice*)GetVisualDevice();

		sheet_width = pd->GetPaperWidth();
		sheet_height = pd->GetPaperHeight();
		page_box_width = pd->GetRenderingViewWidth();
		page_box_height = pd->GetRenderingViewHeight();

		new_page->SetPageBoxOffset(pd->GetPaperLeftOffset(), pd->GetPaperTopOffset(), pd->GetPaperRightOffset(), pd->GetPaperBottomOffset());

		BOOL first_page = current_page_number == 1;
		BOOL left_page = current_page_number % 2 == 0;
		CSS_Properties css_page_properties;

		GetHLDocProfile()->GetCSSCollection()->GetPageProperties(&css_page_properties, first_page, left_page, GetMediaType());

		int user_margin_left, user_margin_right, user_margin_top, user_margin_bottom;

		pd->GetUserMargins(user_margin_left, user_margin_right, user_margin_top, user_margin_bottom);
		margin_top = LayoutCoord(user_margin_top);
		margin_right = LayoutCoord(user_margin_right);
		margin_bottom = LayoutCoord(user_margin_bottom);
		margin_left = LayoutCoord(user_margin_left);

		CSS_decl* cp = css_page_properties.GetDecl(CSS_PROPERTY_size);
		CSSLengthResolver length_resolver(pd);

		if (cp)
		{
			if (cp->GetDeclType() == CSS_DECL_TYPE)
			{

				if ((cp->TypeValue() == CSS_VALUE_landscape && !pd->IsLandscape()) || (cp->TypeValue() == CSS_VALUE_portrait && pd->IsLandscape()))
				{
					short sw = sheet_width;
					short pw = page_box_width;

					sheet_width = (short) sheet_height;
					sheet_height = sw;
					page_box_width = (short) page_box_height;
					page_box_height = pw;
				}

			}
			else
				if (cp->GetDeclType() == CSS_DECL_NUMBER2)
				{
					CSS_number_decl* cpn = (CSS_number_decl*)cp;
					short vtype = cpn->GetValueType(0);

					if (vtype != CSS_PERCENTAGE)
					{
						int val = length_resolver.GetLengthInPixels(cpn->GetNumberValue(0), vtype);

						if (val != INT_MAX && val > 0)
						{
							if (val > SHRT_MAX)
								val = SHRT_MAX;

							page_box_width = val;
						}
					}

					vtype = cpn->GetValueType(1);

					if (vtype != CSS_PERCENTAGE)
					{
						int val = length_resolver.GetLengthInPixels(cpn->GetNumberValue(1), vtype);

						if (val != INT_MAX && val > 0)
							page_box_height = val;
					}
				}
		}

		HTMLayoutProperties::GetPageMargins(logdoc->GetLayoutWorkplace(),
											&css_page_properties,
											LayoutCoord(page_box_width),
											LayoutCoord(page_box_height),
											FALSE,
											margin_top,
											margin_right,
											margin_bottom,
											margin_left);

	}
#endif // _PRINT_SUPPORT_

	new_page->SetMargins(margin_left, margin_right, margin_top, margin_bottom);
	new_page->SetPageArea(page_box_width - margin_left - margin_right, page_box_height - margin_top - margin_bottom);

	new_page->SetSheet(sheet_width, sheet_height);
}


/* It is ok for iframes to be larger than the page */

BOOL
FramesDocument::OverflowedPage(long top, long height, BOOL is_iframe /* = FALSE */)
{
	if (IsInlineFrame())
		return GetParentDoc()->OverflowedPage(top, height, is_iframe);

	if (!current_page)
		return TRUE;

	/* No point in attempting to break after element, if
	   it's taller than the page, unless it is an iframe */

	return GetRelativePageBottom() < top + height && (is_iframe || current_page->GetPageHeight() >= height);
}

long FramesDocument::GetRelativePageTop()
{
	if (IsInlineFrame())
		return GetParentDoc()->GetRelativePageTop() - iframe_first_page_offset;

	return (current_page ? current_page->GetPageTop() : 0) - GetVisualDevice()->GetTranslationY();
}


long FramesDocument::GetRelativePageBottom()
{
	if (IsInlineFrame())
		return GetParentDoc()->GetRelativePageBottom() - iframe_first_page_offset;

	return current_page ? current_page->GetPageBottom()- GetVisualDevice()->GetTranslationY() : LONG_MAX;
}


PageDescription*
FramesDocument::GetCurrentPage()
{
	if (IsInlineFrame())
		return GetParentDoc()->GetCurrentPage();

	return current_page;
}

void FramesDocument::SetCurrentPage(PageDescription* page)
{
	if (IsInlineFrame())
	{
		GetParentDoc()->SetCurrentPage(page);
		return;
	}

	current_page = page;
}

long FramesDocument::GetPageTop()
{
	if (IsInlineFrame())
		return GetParentDoc()->GetPageTop();

	return current_page ? current_page->GetPageTop() : 0;
}


PageDescription* FramesDocument::GetNextPageDescription(long y)
{
	if (IsInlineFrame())
		return GetParentDoc()->GetNextPageDescription(y);

	// If we don't have paged media, we should bail out immediately

	PageDescription* page = (PageDescription*) pages.First();

	BOOL found_preceding_page = FALSE;

	for (; page; page = (PageDescription*) page->Suc())
	{
		long top = page->GetPageTop();

		if (y < top)
			break;
		else
			if (y < top + page->GetSheetHeight())
				if (found_preceding_page)
					return page;
				else
				{
					found_preceding_page = TRUE;
					y = page->GetPageTop() + page->GetSheetHeight();
				}
	}

	return CreatePageDescription(page);
}

int PageDescription::GetNumber()
{
	// .. or should this be stored in PageDescription ?

	int number = 1;

	for (PageDescription* page = (PageDescription*) this->Pred(); page; page = (PageDescription*) page->Pred())
		number++;

	return number;
}

PageDescription* FramesDocument::GetPage(int number)
{
	if (IsInlineFrame())
		return GetParentDoc()->GetPage(number);

	for (PageDescription* page = (PageDescription*) pages.First(); page; page = (PageDescription*) page->Suc())
		if (--number == 0)
			return page;

	return NULL;
}

void FramesDocument::FinishCurrentPage(long bottom)
{
	if (IsInlineFrame())
		GetParentDoc()->FinishCurrentPage(bottom + iframe_first_page_offset);
	else
		if (current_page && current_page->GetSheetHeight() == LONG_MAX >> 1)
		{
			/* In OperaShow, the last page is always infinitely long, and
			   is adjusted to the real size when a new page is appended. */

			long height = (bottom - 1) - current_page->GetPageTop();

			if (height < GetVisualDevice()->GetRenderingViewHeight())
				height = GetVisualDevice()->GetRenderingViewHeight();

			int sheet_height = height + current_page->GetMarginTop() + current_page->GetMarginBottom();
#ifdef _PRINT_SUPPORT_
			if (!GetVisualDevice()->IsPrinter())
#endif
				// Keep the sheet height aligned with the scale so there won't be glitches in scaled mode.
				// (Since all pages is stacked on top of each other, so the viewport is moved to an exact top)
				sheet_height = GetVisualDevice()->ApplyScaleRoundingNearestUp(sheet_height);

			current_page->SetSheet(current_page->GetSheetWidth(), sheet_height);

			current_page->SetPageArea(current_page->GetPageWidth(), height);
		}
}

PageDescription* FramesDocument::AdvancePage(long next_page_top)
{
	if (IsInlineFrame())
		return GetParentDoc()->AdvancePage(next_page_top + iframe_first_page_offset);

	FinishCurrentPage(next_page_top);

	PageDescription* next_page_description = GetNextPageDescription(GetPageTop());

	if (next_page_description)
		current_page = next_page_description;

	return next_page_description;
}

void FramesDocument::RewindPage(long position)
{
	if (IsInlineFrame())
	{
		GetParentDoc()->RewindPage(position + iframe_first_page_offset);
		return;
	}

	position += GetVisualDevice()->GetTranslationY();

	if (current_page == NULL ||
		position < current_page->GetPageTop() ||
		position >= current_page->GetPageBottom())
	{
		PageDescription* page_description = GetPageDescription(position, TRUE);

		if (page_description)
			current_page = page_description;
	}
}

void FramesDocument::ClearPages()
{
	if (IsInlineFrame())
	{
		GetParentDoc()->ClearPages();
		return;
	}

	if (!current_page)
		current_page = CreatePageDescription(NULL); // FIXME: propagate oom
	else
	{
		/* ClearPage doesn't reset the page's top position, so we need to keep the first one or
		* all the rest of the pages created afterwards will have wrong top positions */
		current_page = static_cast<PageDescription*>(pages.First());

		current_page->Out();
		pages.Clear();
		current_page->Into(&pages);

		ClearPage(current_page);
	}
}
BOOL FramesDocument::PageBreakIFrameDocument(int first_page_offset)
{
	OP_ASSERT(IsInlineFrame());
	// set some page offset
	OP_ASSERT(GetTopFramesDoc()->IsPrintDocument());
	iframe_first_page_offset = first_page_offset;
	Reflow(TRUE,TRUE, FALSE, FALSE);

	return TRUE;
}


#endif // PAGED_MEDIA_SUPPORT

HTML_Element* FramesDocument::GetDocRoot()
{
	if (logdoc)
	{
#ifdef PAGED_MEDIA_SUPPORT
#ifdef _PRINT_SUPPORT_
		if (IsPrintDocument())
			return logdoc->GetPrintRoot();
		else
#endif // _PRINT_SUPPORT_
#endif // PAGED_MEDIA_SUPPORT
			return logdoc->GetRoot();
	}
	else
		return NULL;
}


void FramesDocument::SetFrmRootSize()
{
	if (logdoc)
		logdoc->GetLayoutWorkplace()->CalculateFramesetSize();
}

void FramesDocument::SetTextScale(int scale)
{
	DocumentTreeIterator iter(this);
	iter.SetIncludeThis();
	while (iter.Next())
	{
		FramesDocument* frm_doc = iter.GetFramesDocument();
		if (frm_doc->GetDocRoot())
			frm_doc->GetDocRoot()->RemoveCachedTextInfo(this);

		frm_doc->GetVisualDevice()->SetTextScale(scale);
	}
}

void FramesDocument::SetHandheldStyleFound()
{
	CheckERA_LayoutMode();
}

PrefsCollectionDisplay::RenderingModes
FramesDocument::GetPrefsRenderingMode()
{
	switch (layout_mode)
	{
	case LAYOUT_NORMAL:
		return PrefsCollectionDisplay::MSR; // Just hope that noone looks at this value.
	case LAYOUT_SSR:
		return PrefsCollectionDisplay::SSR;
	case LAYOUT_CSSR:
		return PrefsCollectionDisplay::CSSR;
	case LAYOUT_AMSR:
		return PrefsCollectionDisplay::AMSR;
#ifdef TV_RENDERING
	case LAYOUT_TVR:
		return PrefsCollectionDisplay::TVR;
#endif // TV_RENDERING
	}
	OP_ASSERT(layout_mode == LAYOUT_MSR);
	return PrefsCollectionDisplay::MSR;
}

void FramesDocument::CheckERA_LayoutMode(BOOL force)
{
	LayoutMode old_layout_mode = layout_mode;
	PrefsCollectionDisplay::RenderingModes old_rendering_mode = GetPrefsRenderingMode();
	int old_iframe_policy = GetShowIFrames();

	unsigned int win_width, dummyheight;
	GetWindow()->GetCSSViewportSize(win_width, dummyheight);

#if defined(_PRINT_SUPPORT_)
	if (IsPrintDocument() && g_pcprint->GetIntegerPref(PrefsCollectionPrint::FitToWidthPrint))
	{
		PrintDevice* pd = GetWindow()->GetPrinterInfo(TRUE)->GetPrintDevice();
		int left, right, top, bottom;
		pd->GetUserMargins(left, right, top, bottom);
		win_width = pd->GetPaperWidth() - left - right;
	}
	else
#endif
	{
		int scale = GetVisualDevice()->GetScale();

		if (scale != 100)
			win_width = win_width * 100 / scale;
	}

	if (!force && win_width == old_win_width)
		return;

	old_win_width = win_width;

	const uni_char *hostname = GetHostName();
	unsigned int ssr_crossover_size = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::RM1_CrossoverSize, hostname);
	unsigned int cssr_crossover_size = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::RM2_CrossoverSize, hostname);
	unsigned int amsr_crossover_size = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::RM3_CrossoverSize, hostname);
	unsigned int msr_crossover_size = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::RM4_CrossoverSize, hostname);

	respond_to_media_type = RESPOND_TO_MEDIA_TYPE_NONE;

	if (GetSubWinId() != -1)
		layout_mode = GetDocManager()->GetFrame()->GetLayoutMode();
	else
		if (era_mode)
		{
			if (win_width < ssr_crossover_size)
				layout_mode = LAYOUT_SSR;
			else
				if (win_width < cssr_crossover_size)
					layout_mode = LAYOUT_CSSR;
				else
					if (win_width < amsr_crossover_size)
						layout_mode = LAYOUT_AMSR;
					else
						if (win_width < msr_crossover_size)
							layout_mode = LAYOUT_MSR;
						else
							layout_mode = LAYOUT_NORMAL;
		}

	PrefsCollectionDisplay::RenderingModes rendering_mode = GetPrefsRenderingMode();

	if (GetSubWinId() != -1 && layout_mode == GetDocManager()->GetParentDoc()->GetLayoutMode())
	{
		short parent_frame_policy = GetDocManager()->GetParentDoc()->GetFramesPolicy();

		FramesDocElm *frame = GetDocManager()->GetFrame();
		if (frame && frame->IsInlineFrame() && frame->GetNotifyParentOnContentChange() &&
			parent_frame_policy != FRAMES_POLICY_FRAME_STACKING)

			/* Unless we are in frame stacking mode, use smart frames
			   for content-sized iframes. Otherwise frames with
			   percentage heights will collapse to zero height. */

			frames_policy = FRAMES_POLICY_SMART_FRAMES;
		else
			frames_policy = parent_frame_policy;

		abs_positioned_strategy = GetDocManager()->GetParentDoc()->GetAbsPositionedStrategy();
		aggressive_word_breaking = GetDocManager()->GetParentDoc()->GetAggressiveWordBreaking();
	}
	else
	{
		frames_policy = FRAMES_POLICY_NORMAL;
		abs_positioned_strategy = ABS_POSITION_NORMAL;
		aggressive_word_breaking = WORD_BREAK_NORMAL;

		if (GetWindow()->ForceWordWrap())
			aggressive_word_breaking = WORD_BREAK_AGGRESSIVE;

		HLDocProfile* hld_prof = GetHLDocProfile();

#ifdef CSS_VIEWPORT_SUPPORT
		OP_ASSERT(IsTopDocument());
		CSS_Viewport* viewport = hld_prof ? hld_prof->GetCSSCollection()->GetViewport() : NULL;
#endif // CSS_VIEWPORT_SUPPORT

		if (layout_mode != LAYOUT_NORMAL)
		{
			short media_style_handing = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::MediaStyleHandling), hostname);

			switch (media_style_handing)
			{
			default:
			case 0:
				override_media_style = FALSE;
				break;
			case 1:
				override_media_style = CheckOverrideMediaStyle();
				break;
			case 2:
				override_media_style = TRUE;
				break;
			}

			abs_positioned_strategy = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::AbsolutePositioning), hostname);
			aggressive_word_breaking = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::AllowAggressiveWordBreaking), hostname);
			respond_to_media_type = static_cast<RespondToMediaType>(g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::RespondToMediaType), hostname));
			frames_policy = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::FramesPolicy), hostname);
			if (respond_to_media_type != RESPOND_TO_MEDIA_TYPE_NONE && hld_prof && (hld_prof->HasMediaStyle(RespondTypeToMediaType()) && !GetOverrideMediaStyle()
#ifdef CSS_VIEWPORT_SUPPORT
				|| viewport && viewport->HasProperties()
#endif
				))
			{
				layout_mode = LAYOUT_NORMAL;
			}
		}

		if (layout_mode == LAYOUT_NORMAL && GetWindow()->GetFlexRootMaxWidth() != 0)
			frames_policy = FRAMES_POLICY_SMART_FRAMES;

#ifdef CSS_VIEWPORT_SUPPORT
		if (layout_mode == LAYOUT_NORMAL && viewport && viewport->HasProperties())
		{
			respond_to_media_type = RESPOND_TO_MEDIA_TYPE_NONE;
			media_handheld_responded = FALSE;
		}
#endif // CSS_VIEWPORT_SUPPORT

		if (GetWindow()->GetFramesPolicy() != FRAMES_POLICY_DEFAULT)
			frames_policy = GetWindow()->GetFramesPolicy();
	}

	CheckFrameStacking(frames_policy == FRAMES_POLICY_FRAME_STACKING);
	CheckSmartFrames(frames_policy == FRAMES_POLICY_SMART_FRAMES);

	if (IsInlineFrame())
	{
		FramesDocElm* this_frame = GetDocManager()->GetFrame();
		VisualDevice* this_vd = this_frame->GetVisualDevice();

		if (this_vd)
		{
			if (frames_policy == FRAMES_POLICY_FRAME_STACKING)
				this_vd->SetScrollType(VisualDevice::VD_SCROLLING_NO);
			else
				this_vd->SetScrollType((VisualDevice::ScrollType)this_frame->GetFrameScrolling());
		}
	}

	if (frm_root)
	{
		frm_root->CheckERA_LayoutMode();
	}
	else
	{
		int flexible_fonts = layout_mode == LAYOUT_NORMAL ? FLEXIBLE_FONTS_DOCUMENT_ONLY : g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::FlexibleFonts), hostname);

		if (flexible_fonts == FLEXIBLE_FONTS_PREVIOUS_TO_PREDEFINED || flexible_fonts == FLEXIBLE_FONTS_DOCUMENT_TO_PREDEFINED)
		{
			unsigned int tmp_win_width;

			switch (layout_mode)
			{
			case LAYOUT_SSR:
				tmp_win_width = MIN(ssr_crossover_size, win_width);
				flex_font_scale = tmp_win_width * 100 / ssr_crossover_size; break;
			case LAYOUT_CSSR:
				tmp_win_width = MAX(ssr_crossover_size, win_width);
				tmp_win_width = MIN(cssr_crossover_size, tmp_win_width);
				flex_font_scale = (tmp_win_width - ssr_crossover_size) * 100 / (cssr_crossover_size - ssr_crossover_size); break;
			case LAYOUT_AMSR:
				tmp_win_width = MAX(cssr_crossover_size, win_width);
				tmp_win_width = MIN(amsr_crossover_size, tmp_win_width);
				flex_font_scale = (tmp_win_width - cssr_crossover_size) * 100 / (amsr_crossover_size - cssr_crossover_size); break;
			case LAYOUT_MSR:
				tmp_win_width = MAX(amsr_crossover_size, win_width);
				tmp_win_width = MIN(msr_crossover_size, tmp_win_width);
				flex_font_scale = (tmp_win_width - amsr_crossover_size) * 100 / (msr_crossover_size - amsr_crossover_size); break;
			}
		}

		if (GetDocRoot() && GetDocRoot()->GetLayoutBox())
			if (old_layout_mode != layout_mode)
			{
				BOOL support_float;
				short old_column_strategy;
				short column_strategy;
				int old_apply_doc_styles;
				int apply_doc_styles;

				support_float = layout_mode == LAYOUT_NORMAL || g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::Float), hostname) == 1;

				old_column_strategy = old_layout_mode == LAYOUT_NORMAL ? TABLE_STRATEGY_NORMAL : g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(old_rendering_mode, PrefsCollectionDisplay::TableStrategy), hostname);
				column_strategy = layout_mode == LAYOUT_NORMAL ? TABLE_STRATEGY_NORMAL : g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::TableStrategy), hostname);

				old_apply_doc_styles = old_layout_mode == LAYOUT_NORMAL ? APPLY_DOC_STYLES_YES : g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(old_rendering_mode, PrefsCollectionDisplay::DownloadAndApplyDocumentStyleSheets), hostname);
				apply_doc_styles = layout_mode == LAYOUT_NORMAL ? APPLY_DOC_STYLES_YES : g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::DownloadAndApplyDocumentStyleSheets), hostname);

				int iframe_policy = GetShowIFrames();

				if (iframe_policy != SHOW_IFRAMES_NONE && iframe_policy != old_iframe_policy)
					LoadAllIFrames();

				GetDocRoot()->ERA_LayoutModeChanged(this, old_apply_doc_styles != apply_doc_styles, support_float, old_column_strategy != column_strategy);

			}
			else
				if (flexible_fonts == FLEXIBLE_FONTS_PREVIOUS_TO_PREDEFINED || flexible_fonts == FLEXIBLE_FONTS_DOCUMENT_TO_PREDEFINED)
					GetDocRoot()->RemoveCachedTextInfo(this);
	}
}

void FramesDocument::HandleNewLayoutViewSize()
{
	if (frm_root)
	{
		if (ReflowFrames(FALSE) == OpStatus::ERR_NO_MEMORY)
			GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	}
	else if (doc)
	{
		if (HTML_Element* root = GetDocRoot())
			root->MarkDirty(this, FALSE);
	}
}

BOOL FramesDocument::IsLoaded(BOOL inlines_loaded)
{
	if (!HasLoadedLocalResources(inlines_loaded))
		return FALSE;

	if (!logdoc || !logdoc->IsLoaded())
		return FALSE;

	if (frm_root)
		return frm_root->IsLoaded(inlines_loaded);
	else if (ifrm_root && !ifrm_root->IsLoaded(inlines_loaded))
		return FALSE;

	return TRUE;
}

/* private */
BOOL FramesDocument::HasLoadedLocalResources(BOOL inlines_loaded)
{
	if (GetDocManager()->GetCurrentURL().Type() != URL_JAVASCRIPT &&
	    url.Id() != GetDocManager()->GetCurrentURL().Id())
		return FALSE;

	if (inlines_loaded && !InlineImagesLoaded())
		return FALSE;

	if (about_blank_waiting_for_javascript_url)
		return FALSE;

	return TRUE;
}

BOOL FramesDocument::IsParsed()
{
	return logdoc && logdoc->IsParsed();
}

BOOL FramesDocument::NeedsProgressBar()
{
	// It is common for web apps to start loading images and scripts after the initial load
	// and we don't want to flash progress bars or "is loading" mouse cursors for those.
	if (GetDocumentFinishedAtLeastOnce())
		return FALSE;

	if (HasLoadedLocalResources(TRUE))
		if (logdoc)
			if (logdoc->IsLoaded())
				return FALSE;
			else if (ESIsBeingGenerated())
				if (GetHLDocProfile()->GetESLoadManager()->IsGenerationIdle())
					return FALSE;

	return TRUE;
}

void FramesDocument::LocalFindTarget(const uni_char* &win_name, int &subwin_id)
{
	subwin_id = -1;

	if (win_name)
	{
		if (uni_stri_eq(win_name, "_SELF"))
		{
			subwin_id = GetSubWinId(); // use this
			win_name = NULL;
		}
		else if (uni_stri_eq(win_name, "_PARENT"))
		{
			if (GetParentDoc())
				subwin_id = GetParentDoc()->GetSubWinId();

			win_name = NULL;
		}
		else if (uni_stri_eq(win_name, "_TOP"))
			win_name = NULL; // together with subwin_id = -1, this makes the document load in whole window
		else if (uni_stri_eq(win_name, "_BLANK"))
		{
			win_name = NULL;
			subwin_id = -2; // signal new window
		}

		if (win_name)
		{
			// try this document first
			subwin_id = GetNamedSubWinId(win_name);

			if (subwin_id < 0 && GetSubWinId() >= 0)
			{
				// try top document
				DocumentManager* top_doc_man = GetDocManager()->GetWindow()->DocManager();
				if (top_doc_man)
				{
					FramesDocument* frames_doc = top_doc_man->GetCurrentDoc();

					if (frames_doc)
						subwin_id = frames_doc->GetNamedSubWinId(win_name);
				}
			}

			if (subwin_id >= 0) // ok, found sub window
				win_name = NULL;
		}
	}
	else
		subwin_id = GetSubWinId(); // use this
}

#ifndef MOUSELESS
void FramesDocument::MouseOut()
{
	is_moving_separator = FALSE;
#ifndef HAS_NOTEXTSELECTION
	is_selecting_text = FALSE;
	selecting_text_is_ok = FALSE;
	start_selection = FALSE;
#endif // !HAS_NOTEXTSELECTION

	if (doc)
		doc->MouseOut();
}

/* virtual */ DocAction FramesDocument::MouseAction(DOM_EventType event, int x, int y, int visual_viewport_x, int visual_viewport_y, int sequence_count_and_button_or_delta, BOOL shift_pressed, BOOL control_pressed, BOOL alt_pressed, BOOL outside_document)
{
	if (frm_root)
	{
		FrameSeperatorType sep = IsFrameSeparator(x, y);

		if (sep != NO_SEPARATOR)
		{
			GetWindow()->SetCursor(sep == HORIZONTAL_SEPARATOR ? CURSOR_HOR_SPLITTER : CURSOR_VERT_SPLITTER);

			return DOC_ACTION_DRAG_SEP;
		}
	}
	else if (doc)
		return doc->MouseAction(event, x, y, visual_viewport_x, visual_viewport_y, sequence_count_and_button_or_delta, shift_pressed, control_pressed, alt_pressed, outside_document);

	return DOC_ACTION_NONE;
}
#endif // ! MOUSELESS

/* static */ OP_BOOLEAN
FramesDocument::SendDocumentKeyEvent(FramesDocument* frames_doc, HTML_Element* helm, DOM_EventType event, OpKey::Code key, const uni_char *value, OpPlatformKeyEventData *key_event_data, ShiftKeyState keystate, BOOL repeat, OpKey::Location location, unsigned data)
{
	if (key == OP_KEY_ENTER && AutoCompletePopup::IsAutoCompletionVisible() && AutoCompletePopup::IsAutoCompletionHighlighted())
	{
		AutoCompletePopup::CloseAnyVisiblePopup();
		return OpBoolean::IS_TRUE; // This key will only close the autocompletionbox and should not be handled.
	}

	if (frames_doc && frames_doc->GetHtmlDocument())
	{
		OP_STATUS status = frames_doc->HandleKeyboardEvent(event, helm, key, value, key_event_data, keystate, repeat, location, data);
		if (OpStatus::IsMemoryError(status))
			return status;
		else
			return OpBoolean::IS_TRUE;
	}
	return OpBoolean::IS_FALSE;
}

URL FramesDocument::GetURLOfLastAccessedImage()
{
	if (m_document_contexts_in_use.GetCount() > 0)
	{
		DocumentInteractionContext* ctx = m_document_contexts_in_use.Get(m_document_contexts_in_use.GetCount()-1);
		if (ctx->HasImage())
			return ctx->GetImageURL();
	}
	return URL();
}

#ifdef KEYBOARD_SELECTION_SUPPORT
static OpWindowCommander::CaretMovementDirection ActionToMoveDirection(OpInputAction::Action action, BOOL& range)
{
	range = FALSE;
	switch (action)
	{
	default:
		OP_ASSERT(!"Missing case in ActionToMoveDirection()");
		// Fall through.
	case OpInputAction::ACTION_RANGE_GO_TO_START:
		range = TRUE;
		// Fall through.
	case OpInputAction::ACTION_GO_TO_START:
		return OpWindowCommander::CARET_DOCUMENT_HOME;

	case OpInputAction::ACTION_RANGE_GO_TO_LINE_START:
		range = TRUE;
		// Fall through.
	case OpInputAction::ACTION_GO_TO_LINE_START:
		return OpWindowCommander::CARET_LINE_HOME;

	case OpInputAction::ACTION_RANGE_GO_TO_END:
		range = TRUE;
		// Fall through.
	case OpInputAction::ACTION_GO_TO_END:
		return OpWindowCommander::CARET_DOCUMENT_END;

	case OpInputAction::ACTION_RANGE_GO_TO_LINE_END:
		range = TRUE;
		// Fall through.
	case OpInputAction::ACTION_GO_TO_LINE_END:
		return OpWindowCommander::CARET_LINE_END;

	case OpInputAction::ACTION_RANGE_PAGE_UP:
		range = TRUE;
		// Fall through.
	case OpInputAction::ACTION_PAGE_UP:
		return OpWindowCommander::CARET_PAGEUP;

	case OpInputAction::ACTION_RANGE_PAGE_DOWN:
		range = TRUE;
		// Fall through.
	case OpInputAction::ACTION_PAGE_DOWN:
		return OpWindowCommander::CARET_PAGEDOWN;

	case OpInputAction::ACTION_RANGE_NEXT_CHARACTER:
		range = TRUE;
		// Fall through.
	case OpInputAction::ACTION_NEXT_CHARACTER:
		return OpWindowCommander::CARET_RIGHT;

	case OpInputAction::ACTION_RANGE_PREVIOUS_CHARACTER:
		range = TRUE;
		// Fall through.
	case OpInputAction::ACTION_PREVIOUS_CHARACTER:
		return OpWindowCommander::CARET_LEFT;

	case OpInputAction::ACTION_RANGE_NEXT_WORD:
		range = TRUE;
		// Fall through.
	case OpInputAction::ACTION_NEXT_WORD:
		return OpWindowCommander::CARET_WORD_RIGHT;

	case OpInputAction::ACTION_RANGE_PREVIOUS_WORD:
		range = TRUE;
		// Fall through.
	case OpInputAction::ACTION_PREVIOUS_WORD:
		return OpWindowCommander::CARET_WORD_LEFT;

	case OpInputAction::ACTION_RANGE_PREVIOUS_PARAGRAPH:
		range = TRUE;
		// Fall through.
	case OpInputAction::ACTION_PREVIOUS_PARAGRAPH:
		return OpWindowCommander::CARET_PARAGRAPH_UP;

	case OpInputAction::ACTION_RANGE_NEXT_PARAGRAPH:
		range = TRUE;
		// Fall through.
	case OpInputAction::ACTION_NEXT_PARAGRAPH:
		return OpWindowCommander::CARET_PARAGRAPH_DOWN;

	case OpInputAction::ACTION_RANGE_NEXT_LINE:
		range = TRUE;
		// Fall through.
	case OpInputAction::ACTION_NEXT_LINE:
		return OpWindowCommander::CARET_DOWN;

	case OpInputAction::ACTION_RANGE_PREVIOUS_LINE:
		range = TRUE;
		// Fall through.
	case OpInputAction::ACTION_PREVIOUS_LINE:
		return OpWindowCommander::CARET_UP;
	}
}
#endif // !KEYBOARD_SELECTION_SUPPORT

OP_BOOLEAN
FramesDocument::OnInputAction(OpInputAction* action)
{
	Window *window = GetWindow();
	URL url;

#if defined ACTION_OPEN_IMAGE_ENABLED || defined ACTION_OPEN_BACKGROUND_IMAGE_ENABLED
	INT32 shift_state = g_op_system_info->GetShiftKeyState();
	BOOL shift   = shift_state & SHIFTKEY_SHIFT;
	BOOL control = shift_state & SHIFTKEY_CTRL;
#endif // ACTION_OPEN_IMAGE_ENABLED || ACTION_OPEN_BACKGROUND_IMAGE_ENABLED

	if (logdoc)
	{
		OP_BOOLEAN handled = logdoc->GetLayoutWorkplace()->HandleInputAction(action);

		if (handled != OpBoolean::IS_FALSE)
			return handled;
	}

	switch (action->GetAction())
	{
	case OpInputAction::ACTION_ACTIVATE_ELEMENT:
		if (doc)
			if (HTML_Element* target = FindTargetElement())
				return doc->ActivateElement(target, action->GetShiftKeys());
		return OpBoolean::IS_FALSE;

	case OpInputAction::ACTION_FOCUS_NEXT_WIDGET:
	case OpInputAction::ACTION_FOCUS_PREVIOUS_WIDGET:
		{
			GetVisualDevice()->SetDrawFocusRect(TRUE);

			BOOL handled = FALSE;
			if (doc)
				handled = doc->NextTabElm(action->GetAction() == OpInputAction::ACTION_FOCUS_NEXT_WIDGET);

			return handled ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
		}

	case OpInputAction::ACTION_LOWLEVEL_PREFILTER_ACTION:
		{
			// This is a temporary hack to make this code compile and work and avoid
			// cross module release problems. Reassigning action is not a good idea.
			action = action->GetChildAction();
			switch (action->GetAction())
			{
			case OpInputAction::ACTION_LOWLEVEL_KEY_DOWN:
			case OpInputAction::ACTION_LOWLEVEL_KEY_UP:
			case OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED:
				if (action->IsKeyboardInvoked())
				{
					OpKey::Code key = action->GetActionKeyCode();

					DOM_EventType event_type = ONKEYPRESS;
					if (action->GetAction() == OpInputAction::ACTION_LOWLEVEL_KEY_DOWN)
						event_type = ONKEYDOWN;
					else if (action->GetAction() == OpInputAction::ACTION_LOWLEVEL_KEY_UP)
						event_type = ONKEYUP;
#ifdef ACCESS_KEYS_SUPPORT
					else if (window && window->GetAccesskeyMode() && HasAccessKeys())
					{
						if (HLDocProfile* profile = GetHLDocProfile())
						{
							if (HLDocProfile::AccessKey* access_key = profile->GetAccessKey(action->GetActionKeyCode()))
							{
								BOOL process_access_key = TRUE;
								if (access_key->GetElement()->IsMatchingType(HE_AREA, NS_HTML))
								{
									// For areas, don't process access keys if the image
									// this area belongs to is not displayed (display: none)
									// or is hidden (visibility: hidden)
									process_access_key = FALSE;

									HTML_Element* map = access_key->GetElement()->ParentActual();
									while (map && !map->IsMatchingType(HE_MAP, NS_HTML))
										map = map->ParentActual();

									if (map)
									{
										HTML_Element* start = profile->GetDocRoot();
										while (start)
										{
											HTML_Element* img = doc->FindAREAObjectElement(map, start);
											if (!img)
												break;
											else if (img->GetLayoutBox())
											{
												Head prop_list;
												LayoutProperties* lprops = LayoutProperties::CreateCascade(img, prop_list, profile);
												process_access_key = lprops && lprops->GetProps()->visibility != CSS_VALUE_hidden;
												prop_list.Clear();
											}

											start = img->NextActual();
										}
									}
								}

								if (process_access_key)
								{
									GetVisualDevice()->SetDrawFocusRect(TRUE);
									if (!(access_key->GetElement()->IsFormElement() && access_key->GetElement()->IsDisabled(this)))
									{
#  ifdef SVG_SUPPORT
										if (access_key->GetElement()->GetNsType() == NS_SVG)
											g_svg_manager->HandleAccessKey(this, access_key->GetElement(), access_key->key);
										else
#  endif // SVG_SUPPORT
											doc->ActivateElement(access_key->GetElement());
									}
									window->GetWindowCommander()->GetDocumentListener()->OnAccessKeyUsed(window->GetWindowCommander());
#  ifndef ACCESS_KEYS_MODE_PERSISTS
									window->SetAccesskeyMode(FALSE);
#  endif // ACCESS_KEYS_MODE_PERSISTS
									return OpBoolean::IS_TRUE;
								}
							}
						}
					}
#endif // ACCESS_KEYS_SUPPORT

					return SendDocumentKeyEvent(this, NULL, event_type, key, action->GetKeyValue(), action->GetPlatformKeyEventData(), action->GetShiftKeys(), action->GetKeyRepeat(), action->GetKeyLocation(), 0);
				}
				break;
			}
		}
		break;

#ifdef DOM_FULLSCREEN_MODE
#if defined OP_KEY_SPACE_ENABLED || OP_KEY_ESCAPE_ENABLED
	case OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED:
		if (action->GetShiftKeys() == 0 &&
			GetFullscreenElement() != NULL &&
			GetFullscreenElement()->IsMatchingType(HE_VIDEO, NS_HTML))
		{
#if defined OP_KEY_SPACE_ENABLED && defined MEDIA_HTML_SUPPORT
			if (action->GetActionData() == OP_KEY_SPACE)
			{
				// Fancy: make space bar pause and play just like in media players.
				MediaElement *media_element = GetFullscreenElement()->GetMediaElement();
				if (media_element)
				{
					if (media_element->GetPaused() || media_element->GetPlaybackEnded())
						media_element->Play(NULL);
					else
						media_element->Pause(NULL);
					return OpBoolean::IS_TRUE;
				}
			}
#endif // OP_KEY_SPACE_ENABLED && MEDIA_HTML_SUPPORT
#ifdef OP_KEY_ESCAPE_ENABLED
			if (action->GetActionData() == OP_KEY_ESCAPE)
			{
				// Fancy: ESC closes fullscreen video, just like in media players.
				RETURN_IF_MEMORY_ERROR(DOMExitFullscreen(NULL));
				return OpBoolean::IS_TRUE;
			}
#endif // OP_KEY_ESCAPE_ENABLED
		}
		break;
#endif // defined OP_KEY_SPACE_ENABLED || OP_KEY_ESC_ENABLED
#endif //DOM_FULLSCREEN_MODE

	case OpInputAction::ACTION_HIGHLIGHT_NEXT_URL:
	case OpInputAction::ACTION_HIGHLIGHT_PREVIOUS_URL:
#ifdef _PRINT_SUPPORT_
		if (!window->GetPreviewMode())
#endif // _PRINT_SUPPORT_
		{
			window->HighlightURL(action->GetAction() == OpInputAction::ACTION_HIGHLIGHT_NEXT_URL);
			return OpBoolean::IS_TRUE;
		}
		break;

	case OpInputAction::ACTION_HIGHLIGHT_NEXT_HEADING:
	case OpInputAction::ACTION_HIGHLIGHT_PREVIOUS_HEADING:
#ifdef _PRINT_SUPPORT_
		if (!window->GetPreviewMode())
#endif // _PRINT_SUPPORT_
		{
			window->HighlightHeading(action->GetAction() == OpInputAction::ACTION_HIGHLIGHT_NEXT_HEADING);
			return OpBoolean::IS_TRUE;
		}
		break;

	case OpInputAction::ACTION_HIGHLIGHT_NEXT_ELEMENT:
	case OpInputAction::ACTION_HIGHLIGHT_PREVIOUS_ELEMENT:
#ifdef _PRINT_SUPPORT_
		if (!window->GetPreviewMode())
#endif // _PRINT_SUPPORT_
		{
			window->HighlightItem(action->GetAction() == OpInputAction::ACTION_HIGHLIGHT_NEXT_ELEMENT);
			return OpBoolean::IS_TRUE;
		}
		break;

	case OpInputAction::ACTION_FOCUS_NEXT_FRAME:
	case OpInputAction::ACTION_FOCUS_PREVIOUS_FRAME:
		GetTopDocument()->SetNextActiveFrame(action->GetAction() == OpInputAction::ACTION_FOCUS_PREVIOUS_FRAME);
		return OpBoolean::IS_TRUE;

#ifndef HAS_NOTEXTSELECTION
	case OpInputAction::ACTION_SELECT_ALL:
		{
			OP_BOOLEAN res = SelectAll(TRUE);
			if (OpStatus::IsError(res))
				return res;

# ifdef _X11_SELECTION_POLICY_
			g_clipboard_manager->SetMouseSelectionMode(TRUE);
			g_clipboard_manager->Copy(GetClipboardListener(), GetWindow()->GetUrlContextId());
			g_clipboard_manager->SetMouseSelectionMode(FALSE);
# endif // _X11_SELECTION_POLICY_
			return res;
		}

	case OpInputAction::ACTION_DESELECT_ALL:
		return GetTopDocument()->DeSelectAll();
#endif // !HAS_NOTEXTSELECTION

	case OpInputAction::ACTION_SEARCH: // should be moved to browser window
		{
			break;
		}

#ifdef USE_OP_CLIPBOARD
# ifndef HAS_NOTEXTSELECTION
	case OpInputAction::ACTION_CUT:
		g_clipboard_manager->Cut(GetClipboardListener(), GetWindow()->GetUrlContextId(), this);
		return OpBoolean::IS_TRUE;
	case OpInputAction::ACTION_COPY:
		g_clipboard_manager->Copy(GetClipboardListener(), GetWindow()->GetUrlContextId(), this);
		return OpBoolean::IS_TRUE;
	case OpInputAction::ACTION_PASTE:
		g_clipboard_manager->Paste(GetClipboardListener(), this);
		return OpBoolean::IS_TRUE;
# endif // HAS_NOTEXTSELECTION
#endif // USE_OP_CLIPBOARD

#ifdef WAND_SUPPORT
	case OpInputAction::ACTION_WAND:
		if (!g_wand_manager->Usable(this, FALSE))
			return OpBoolean::IS_FALSE;
		g_wand_manager->Use(this);
		return OpBoolean::IS_TRUE;
#endif // WAND_SUPPORT

#ifdef KEYBOARD_SELECTION_SUPPORT
	case OpInputAction::ACTION_TOGGLE_KEYBOARD_SELECTION:
		SetKeyboardSelectable(!GetKeyboardSelectionMode());
		return OpBoolean::IS_TRUE;

	case OpInputAction::ACTION_RANGE_GO_TO_START:
	case OpInputAction::ACTION_GO_TO_START:
	case OpInputAction::ACTION_RANGE_GO_TO_LINE_START:
	case OpInputAction::ACTION_GO_TO_LINE_START:
	case OpInputAction::ACTION_RANGE_GO_TO_END:
	case OpInputAction::ACTION_GO_TO_END:
	case OpInputAction::ACTION_RANGE_GO_TO_LINE_END:
	case OpInputAction::ACTION_GO_TO_LINE_END:
	case OpInputAction::ACTION_RANGE_PAGE_UP:
	case OpInputAction::ACTION_PAGE_UP:
	case OpInputAction::ACTION_RANGE_PAGE_DOWN:
	case OpInputAction::ACTION_PAGE_DOWN:
	case OpInputAction::ACTION_RANGE_NEXT_CHARACTER:
	case OpInputAction::ACTION_NEXT_CHARACTER:
	case OpInputAction::ACTION_RANGE_PREVIOUS_CHARACTER:
	case OpInputAction::ACTION_PREVIOUS_CHARACTER:
	case OpInputAction::ACTION_RANGE_NEXT_WORD:
	case OpInputAction::ACTION_NEXT_WORD:
	case OpInputAction::ACTION_RANGE_PREVIOUS_WORD:
	case OpInputAction::ACTION_PREVIOUS_WORD:
	case OpInputAction::ACTION_RANGE_NEXT_LINE:
	case OpInputAction::ACTION_NEXT_LINE:
	case OpInputAction::ACTION_RANGE_PREVIOUS_LINE:
	case OpInputAction::ACTION_PREVIOUS_LINE:
	case OpInputAction::ACTION_RANGE_NEXT_PARAGRAPH:
	case OpInputAction::ACTION_NEXT_PARAGRAPH:
	case OpInputAction::ACTION_RANGE_PREVIOUS_PARAGRAPH:
	case OpInputAction::ACTION_PREVIOUS_PARAGRAPH:
		if (GetKeyboardSelectionMode())
		{
			BOOL range;
			OpWindowCommander::CaretMovementDirection direction = ActionToMoveDirection(action->GetAction(), range);
			MoveCaret(direction, range);
			return OpBoolean::IS_TRUE;
		}
		break;
#endif // KEYBOARD_SELECTION_SUPPORT

#if defined(ACTION_OPEN_IMAGE_ENABLED)
	case OpInputAction::ACTION_OPEN_IMAGE:
		// FIXME: We don't _really_ know which image this action refers to.
		url = GetURLOfLastAccessedImage();
		// FIXME: Use the modifiers in the OpDocumentContext object so that we get what the user had
		// pressed when it was created rather than what is pressed now, some time later.
		if (!url.IsEmpty())
		{
			Window* window = GetWindow();
			BOOL new_window = shift ||
				window->GetType() == WIN_TYPE_NEWSFEED_VIEW ||
				window->GetType() == WIN_TYPE_MAIL_VIEW;
			GetTopDocument()->GetDocManager()->OpenImageURL(url, DocumentReferrer(GetURL()), FALSE, new_window, shift && control);
		}
		return OpBoolean::IS_TRUE;
#endif // ACTION_OPEN_IMAGE_ENABLED

#ifdef ACTION_OPEN_BACKGROUND_IMAGE_ENABLED
	case OpInputAction::ACTION_OPEN_BACKGROUND_IMAGE:
		url = GetBGImageURL();
		if (!url.IsEmpty())
			GetTopDocument()->GetDocManager()->OpenImageURL(url, DocumentReferrer(GetURL()), FALSE, shift, shift && control);
		return OpBoolean::IS_TRUE;
#endif // ACTION_OPEN_BACKGROUND_IMAGE_ENABLED

#if defined(ACTION_SAVE_IMAGE_ENABLED)
	case OpInputAction::ACTION_SAVE_IMAGE:
		// FIXME: We don't _really_ know which image this action refers to.
		url = GetURLOfLastAccessedImage();
		if (!url.IsEmpty())
		{
#ifdef WIC_USE_DOWNLOAD_CALLBACK
			ViewActionFlag view_action_flag;
			view_action_flag.Set(ViewActionFlag::SAVE_AS);
			TransferManagerDownloadCallback * download_callback = OP_NEW(TransferManagerDownloadCallback, (GetTopDocument()->GetDocManager(), url, NULL, view_action_flag));
			if (!download_callback)
				return OpStatus::ERR_NO_MEMORY;

			if (GetTopDocument()->GetDocManager()->GetWindow() &&
				GetTopDocument()->GetDocManager()->GetWindow()->GetWindowCommander())
			{
				WindowCommander * wic;
				wic = GetTopDocument()->GetDocManager()->GetWindow()->GetWindowCommander();
				wic->GetDocumentListener()->OnDownloadRequest(wic, download_callback);
				download_callback->Execute();
			}
			else
				OP_DELETE(download_callback);
#else
			GetTopDocument()->GetDocManager()->OpenImageURL(url, GetURL(), TRUE);
#endif // WIC_USE_DOWNLOAD_CALLBACK
		}
		return OpBoolean::IS_TRUE;
#endif // ACTION_SAVE_IMAGE_ENABLED && DISPLAY_CLICKINFO_SUPPORT

#ifdef ACTION_SAVE_BACKGROUND_IMAGE_ENABLED
	case OpInputAction::ACTION_SAVE_BACKGROUND_IMAGE:
		url = GetBGImageURL();
		if (!url.IsEmpty())
			GetTopDocument()->GetDocManager()->OpenImageURL(url, DocumentReferrer(GetURL()), TRUE);
		return OpBoolean::IS_TRUE;
#endif // ACTION_SAVE_BACKGROUND_IMAGE_ENABLED

#ifdef ACTION_ACTIVATE_ON_DEMAND_PLUGINS_ENABLED
	case OpInputAction::ACTION_ACTIVATE_ON_DEMAND_PLUGINS:
		{
			DocumentTreeIterator iter(GetWindow());

			iter.SetIncludeThis();

			while (iter.Next())
			{
				FramesDocument* doc = iter.GetFramesDocument();
				OpAutoPtr<OpHashIterator> iter_elm(doc->m_ondemand_placeholders.GetIterator());
				RETURN_OOM_IF_NULL(iter_elm.get());

				if (LogicalDocument* logdoc = doc->GetLogicalDocument())
					for (OP_STATUS status = iter_elm->First(); status == OpStatus::OK; status = iter_elm->Next())
						logdoc->GetLayoutWorkplace()->ActivatePluginPlaceholder(static_cast<HTML_Element*>(iter_elm->GetData()));
			}

			return OpBoolean::IS_TRUE;
		}

#endif // ACTION_ACTIVATE_ON_DEMAND_PLUGINS_ENABLED

	case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
#ifdef USE_OP_CLIPBOARD
# ifndef HAS_NOTEXTSELECTION
			case OpInputAction::ACTION_PASTE:
			case OpInputAction::ACTION_CUT:
				child_action->SetEnabled(g_clipboard_manager->ForceEnablingClipboardAction(child_action->GetAction(), this));
				return OpBoolean::IS_TRUE;
			case OpInputAction::ACTION_COPY:
				if (doc)
					child_action->SetEnabled(doc->GetSelectedTextLen() > 0 ||
					                         g_clipboard_manager->ForceEnablingClipboardAction(OpInputAction::ACTION_COPY, this));
				else
					child_action->SetEnabled(FALSE);

				return OpBoolean::IS_TRUE;
# endif // HAS_NOTEXTSELECTION
#endif // USE_OP_CLIPBOARD

#ifdef ACTION_ACTIVATE_ON_DEMAND_PLUGINS_ENABLED
			case OpInputAction::ACTION_ACTIVATE_ON_DEMAND_PLUGINS:
				child_action->SetEnabled(DocTreeHasPluginPlaceholders());
				return OpBoolean::IS_TRUE;
#endif // ACTION_ACTIVATE_ON_DEMAND_PLUGINS_ENABLED

#if defined(ACTION_OPEN_IMAGE_ENABLED)
			case OpInputAction::ACTION_OPEN_IMAGE:
				// FIXME: We don't really know which image the action refers to
				url = GetURLOfLastAccessedImage();
				break;
#endif // ACTION_OPEN_IMAGE_ENABLED

#if defined(ACTION_SAVE_IMAGE_ENABLED)
			case OpInputAction::ACTION_SAVE_IMAGE:
				// FIXME: We don't really know which image the action refers to
				url = GetURLOfLastAccessedImage();
				break;
#endif // ACTION_OPEN_IMAGE_ENABLED

#ifdef ACTION_OPEN_BACKGROUND_IMAGE_ENABLED
			case OpInputAction::ACTION_OPEN_BACKGROUND_IMAGE:
				url = GetBGImageURL();
				break;
#endif // ACTION_OPEN_BACKGROUND_IMAGE_ENABLED

#ifdef ACTION_SAVE_BACKGROUND_IMAGE_ENABLED
			case OpInputAction::ACTION_SAVE_BACKGROUND_IMAGE:
				url = GetBGImageURL();
				break;
#endif // ACTION_OPEN_BACKGROUND_IMAGE_ENABLED
			default:
				return OpBoolean::IS_FALSE;
			}

#if defined(ACTION_OPEN_IMAGE_ENABLED) || defined(ACTION_SAVE_IMAGE_ENABLED) || defined(ACTION_OPEN_BACKGROUND_IMAGE_ENABLED) || defined(ACTION_SAVE_BACKGROUND_IMAGE_ENABLED)
			child_action->SetEnabled(!url.IsEmpty() && (g_secman_instance->IsSafeToExport(url) && !url.GetAttribute(URL::KHeaderLoaded) || url.IsImage()));
			return OpBoolean::IS_TRUE;
#endif // ACTION_OPEN_IMAGE_ENABLED || ACTION_SAVE_IMAGE_ENABLED || ACTION_OPEN_BACKGROUND_IMAGE_ENABLED || ACTION_SAVE_BACKGROUND_IMAGE_ENABLED
		}
		break;

#ifdef ACTION_MAKE_READABLE_ENABLED
	case OpInputAction::ACTION_MAKE_READABLE:
		{
			TextSelection *ts = GetTextSelection ();

			if (ts && !ts->IsEmpty ())
			{
				HTML_Element *text;
				CSS_decl *decl;
				FontAtt font;

				text = ts->GetStartElement ();
				// Font preferences stores size in pixels.
				g_pcfontscolors->GetFont (OP_SYSTEM_FONT_DOCUMENT_NORMAL, font);
				decl = LayoutProperties::GetComputedDecl (text, CSS_PROPERTY_font_size, 0, GetHLDocProfile());

				if (!decl)
					return OpBoolean::IS_TRUE;

				// GetComputedDecl will always use CSS_PX units.
				OP_ASSERT (decl->GetValueType(0) == CSS_PX);

				GetWindow()->GetMessageHandler()->PostMessage
					(WM_OPERA_SCALEDOC, 0,
					 MAKELONG(static_cast<int>(font.GetHeight() * 100 / decl->GetNumberValue(0)),
							  TRUE /* scale is absolute */));
			}
		}
		return OpBoolean::IS_TRUE;
#endif // ACTION_MAKE_READABLE_ENABLED
	}

	return OpBoolean::IS_FALSE;
}

/* virtual */ OP_STATUS FramesDocument::ResetForm(int form_number)
{
	// Need the form element for the new API:
	HTML_Element* he = GetDocRoot();
	while (he && (!(he->IsMatchingType(HE_ISINDEX, NS_HTML) ||
					he->IsMatchingType(HE_FORM, NS_HTML)) ||
					he->GetFormNr() != form_number))
	{
		he = he->NextActual();
	}
	if (he)
	{
		return FormManager::ResetForm(this, he);
	}
	return OpStatus::OK;
}

void FramesDocument::SetActiveHTMLElement(HTML_Element* active_element)
{
	if (doc)
		doc->SetActiveHTMLElement(active_element);
}

HTML_Element* FramesDocument::GetActiveHTMLElement()
{
	if (doc)
		return doc->GetActiveHTMLElement();
	else
		return NULL;
}

void FramesDocument::BubbleHoverToParent(HTML_Element* elm)
{
	// No events are necessary (or wanted) and no pseudo classes
	// change for elements remaining in the tree.
	if (doc && elm->IsAncestorOf(doc->GetHoverHTMLElement()))
		doc->SetHoverHTMLElementSilently(elm->Parent());
}

void FramesDocument::SetInlinesUsed(BOOL used)
{
	if (frm_root)
		frm_root->SetInlinesUsed(used);

	LoadInlineElmHashIterator iterator(inline_hash);
	for (LoadInlineElm *iter = iterator.First(); iter; iter = iterator.Next())
		iter->SetUsed(used);

	if (ifrm_root)
		ifrm_root->SetInlinesUsed(used);
}

#ifdef GADGET_SUPPORT
OP_STATUS FramesDocument::HandleFormRequest(URL form_url, BOOL user_initiated, const uni_char *win_name, BOOL open_in_other_window, BOOL open_in_background, ES_Thread *thread, BOOL continue_after_security_check)
#else
OP_STATUS FramesDocument::HandleFormRequest(URL form_url, BOOL user_initiated, const uni_char *win_name, BOOL open_in_other_window, BOOL open_in_background, ES_Thread *thread)
#endif // GADGET_SUPPORT
{
	DocumentFormSubmitCallback *callback;

#ifdef GADGET_SUPPORT
	if (continue_after_security_check)
	{
		OP_ASSERT(current_form_request && current_form_request->IsWaitingForInitialSecurityCheck());
		callback = current_form_request;
		callback->SetWaitingForSecurityCheck(FALSE);
		current_form_request = NULL;
	}
	else
#endif // GADGET_SUPPORT
	{
		if (current_form_request)
		{
#ifdef GADGET_SUPPORT
			if (current_form_request->IsWaitingForInitialSecurityCheck())
				OP_DELETE(current_form_request);
			else
#endif // GADGET_SUPPORT
			{
				current_form_request->Cancel();
			}
			current_form_request = NULL;
		}

		callback = OP_NEW(DocumentFormSubmitCallback, (this, user_initiated, form_url, open_in_other_window, open_in_background));

		if (!callback || win_name && *win_name && OpStatus::IsMemoryError(callback->SetWindowName(win_name)))
		{
			OP_DELETE(callback);
			return OpStatus::ERR_NO_MEMORY;
		}

		callback->SetThread(thread);
	}

#ifdef GADGET_SUPPORT
	if (OpGadget *gadget = GetWindow()->GetGadget())
	{
		if (!continue_after_security_check && GetWindow()->GetType() == WIN_TYPE_GADGET)
		{
			// Fail silently for ALL form requests that leads to the toplevel document in widgets being replaced
			int swid = sub_win_id;
			FindTarget(win_name, swid);
			if (swid == -1 || form_url.Type() == URL_WIDGET)
			{
				OP_DELETE(callback);
				return OpStatus::OK;
			}
		}

		BOOL allowed = FALSE;

		OpStatus::Ignore(OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_LOADSAVE,
		                                                  OpSecurityContext(gadget, GetWindow()),
		                                                  OpSecurityContext(form_url),
		                                                  allowed,
		                                                  callback->SecurityState()));

		if (!allowed)
		{
			if (callback->SecurityState().suspended && !continue_after_security_check)
			{
				// suspend
				Comm* comm = callback->SecurityState().host_resolving_comm;
				g_main_message_handler->SetCallBack(callback, MSG_COMM_NAMERESOLVED, comm->Id());
				g_main_message_handler->SetCallBack(callback, MSG_COMM_LOADING_FAILED, comm->Id());
				callback->SetWaitingForSecurityCheck(TRUE);
				current_form_request = callback;
				return OpStatus::OK;
			}

			OP_DELETE(callback);
			return OpStatus::OK;
		}
	}
#endif // GADGET_SUPPORT

	WindowCommander *wc = GetWindow()->GetWindowCommander();

	wc->GetDocumentListener()->OnFormSubmit(wc, callback);

	OP_BOOLEAN finished = callback->Execute();

	if (finished == OpBoolean::IS_TRUE)
	{
		OP_DELETE(callback);
	}
	else if (finished == OpBoolean::IS_FALSE)
	{
		current_form_request = callback;
	}
	else
	{
		OP_ASSERT(OpStatus::IsError(finished));
		OP_DELETE(callback);
		return finished;
	}

	return OpStatus::OK;
}

OP_STATUS FramesDocument::MouseOverURL(const URL& url,
									   const uni_char* win_name,
									   DOM_EventType event,
									   BOOL shift_pressed,
									   BOOL control_pressed,
									   ES_Thread *thread/*=NULL*/,
									   HTML_Element *event_elm/*=NULL*/)
{
	switch (event)
	{
	case ONCLICK:
		{
			if (url.Type() == URL_OPERA && url.GetAttribute(URL::KName).CompareI("opera:forcehtml") == 0)
				return ReparseAsHtml(FALSE);

#ifdef HIGHLIGHT_CLICKED_LINK
			HTML_Document* html_doc = GetHtmlDocument();

			if (event_elm)
				html_doc->HighlightElement(event_elm);
#endif //HIGHLIGHT_CLICKED_LINK
		}
		// fallthrough from ONCLICK
	case ONSUBMIT:
		{
			int subwin_id = this->GetSubWinId();

			GetWindow()->GetWindowCommander()->GetDocumentListener()->OnLinkClicked(GetWindow()->GetWindowCommander());

			if (subwin_id != -1 && GetDocManager()->GetFrame()->IsSpecialObject())
				win_name = NULL;

			// Convert back to modifiers so the tweaks work
			ShiftKeyState local_modifiers = 0;
			if (shift_pressed)
				local_modifiers |= SHIFTKEY_SHIFT;
			if (control_pressed)
				local_modifiers |= SHIFTKEY_CTRL;

			/** Prevent opening new window for certain URL types, f.e. javascript: */
			BOOL allow_new_window = url.Type() != URL_JAVASCRIPT;
			BOOL open_in_other_window = allow_new_window && (SHIFTKEY_OPEN_IN_NEW_WINDOW & local_modifiers) != 0;
			BOOL toggle_open_in_background = ((local_modifiers & SHIFTKEY_TOGGLE_OPEN_IN_BACKGROUND)
											  && !(local_modifiers & SHIFTKEY_PREVENT_OPEN_IN_BACKGROUND));
			BOOL open_in_background = allow_new_window && toggle_open_in_background;

			BOOL user_initiated = !thread || thread->IsUserRequested();

			if (url.GetAttribute(URL::KHTTPIsFormsRequest))
				return HandleFormRequest(url, user_initiated, win_name, open_in_other_window, open_in_background, thread);

			return g_windowManager->OpenURLNamedWindow(url, GetWindow(), this, subwin_id, win_name, user_initiated, open_in_other_window, open_in_background, TRUE, FALSE, thread);
		}

	case ONMOUSEOVER:
		{
			const uni_char*	title = NULL;
			HTML_Document* html_doc = GetHtmlDocument();
			ST_MESSAGETYPE szType = ST_ALINK;
			if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::DisplayLinkTitle, GetHostName()) && html_doc)
			{
				// Take the title specified closes to the actual hovered element
				HTML_Element* hovered = html_doc->GetHoverHTMLElement();
				while (!title && hovered)
				{
					title = hovered->GetElementTitle();
					hovered = hovered->Parent();
				}

				if(title && *title)
					szType = ST_ATITLE;	//this is a multiline tooltip attribute
			}
			OpString link_str;

			const char* url_charset = NULL;
			if (logdoc)
				url_charset = GetHLDocProfile()->GetCharacterSet();
			if (!url_charset)
				url_charset = "utf-8";
			unsigned short charset_id;
			if (OpStatus::IsSuccess(g_charsetManager->GetCharsetID(url_charset, &charset_id)))
				url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_Hidden, charset_id, link_str);

			if (!link_str.IsEmpty() || title && *title)
				GetWindow()->DisplayLinkInformation(link_str.CStr(), szType, title);

#ifdef DNS_PREFETCHING
			if (logdoc)
				logdoc->DNSPrefetch(url, DNS_PREFETCH_MOUSEOVER);
#endif // DNS_PREFETCHING
		}
		break;
	}

	return OpStatus::OK;
}

BOOL FramesDocument::Free(BOOL layout_only/*=FALSE*/, FreeImportance free_importance/*= FREE_NORMAL */)
{
	OP_NEW_DBG("FramesDocument::Free", "doc");
	OP_DBG((UNI_L("this=%p; document_state=%p"), this, document_state));
	if (GetLocked())
		return FALSE;

	if (IsESActive(FALSE) || IsESStopped())
		return FALSE;

#ifdef DOCUMENT_EDIT_SUPPORT
	DocListElm *doc_list_elem = doc_manager->FindDocListElm(this);
	unsigned int distance_in_history = DOC_DOCEDIT_KEEP_HISTORY_DISTANCE + 1;
	if (doc_list_elem)
		distance_in_history = op_abs(GetWindow()->GetCurrentHistoryPos() - doc_list_elem->Number());

	if (GetDocumentEdit())
	{
		switch (free_importance)
		{
			case FREE_VERY_IMPORTANT:
				break;

			case FREE_IMPORTANT:
			{
				/* Even if free is important to be performed keep the docedit if it's 'very close' to the current doc in history... */
				if (distance_in_history <= DOC_DOCEDIT_KEEP_HISTORY_DISTANCE &&
					/* ...but only when this 'very close' is  not grater than max window history */
					distance_in_history <= (unsigned int)g_pccore->GetIntegerPref(PrefsCollectionCore::MaxWindowHistory))
					return FALSE;
			}
			break;

			case FREE_NORMAL:
				return FALSE;

			default:
				OP_ASSERT(FALSE); // Bad parameter passed (maybe FreeImportance enum was extended byt new values are not handled here)
		}
	}
#endif // DOCUMENT_EDIT_SUPPORT

	is_being_freed = TRUE;

	if (layout_only && CalculateHistoryNavigationMode() == HISTORY_NAV_MODE_COMPATIBLE)
		layout_only = FALSE;

	if (document_state)
		OpStatus::Ignore(document_state->StoreIFrames());

	reflowing = FALSE;

	if (logdoc)
		logdoc->GetLayoutWorkplace()->ClearCounters();

	do_styled_paint = FALSE;

	RemoveAllLoadInlineElms();

	if (text_selection)
	{
		OP_DELETE(text_selection);
		text_selection = NULL;
	}

	if (frm_root)
		frm_root->Free(layout_only, free_importance);
	else
	{
		if (doc)
			doc->Free();

		if (ifrm_root)
			ifrm_root->Free(layout_only, free_importance);
	}

	if (layout_only)
	{
		if (logdoc && own_logdoc)
			logdoc->FreeLayout();

		is_being_freed = FALSE;
		return TRUE;
	}

	if (dom_environment)
		dom_environment->BeforeDestroy();

	if (own_logdoc)
		OP_DELETE(logdoc);

	logdoc = NULL;

	has_sent_domcontent_loaded = FALSE;
	local_doc_finished = FALSE;
	doc_tree_finished = FALSE;

	OP_DELETE(ifrm_root);
	ifrm_root = NULL;

	SignalAllDelayedLayoutListeners();

	wait_for_styles = 0;
	GetMessageHandler()->RemoveDelayedMessage(MSG_WAIT_FOR_STYLES_TIMEOUT, GetSubWinId(), 0);

	wait_for_paint_blockers = 0;

	last_helm = NULL;

	async_inline_elms.DeleteAll();

#ifdef SHORTCUT_ICON_SUPPORT
	favicon = URL();
#endif // SHORTCUT_ICON_SUPPORT
	image_info.Reset();

#ifdef DOC_THREAD_MIGRATION
	if (es_generated_document)
	{
		/* Need to keep the document and its DOM environment around while
		   the other document is being generated. */

		reflowing = FALSE;
		is_being_freed = FALSE;
		remove_from_cache = TRUE; // Can't reuse this FramesDocument since it has an old script environment.

		return FALSE;
	}
#endif // DOC_THREAD_MIGRATION

	CleanESEnvironment(TRUE);
	reflowing = FALSE;
	is_being_freed = FALSE;

	return TRUE;
}

OP_STATUS FramesDocument::StopLoading(BOOL format, BOOL aborting/*=FALSE*/, BOOL stop_plugin_streams/*=TRUE*/)
{
	// check whether a document which is about to be stopped changed was completely parsed
	// to know whether it's safe to bring it again from the docs cache or whether
	// it should be taken from the network in case it's requested again (if not stopped by the used)
	// this has to be called before logdoc->StopLoading() as it finishes the parsing
	if (IsParsed())
	{
		/* In case user pressed stop this function might be called twice: after user pressed stop and
		 * when user navigates from a page. In such case the parsing status when user pressed stop
		 * should be stored and not modified at second call of this function
		 * (because then it will be always TRUE as this functions finishes parsing).
		 */
		if (!IsAborted())
		{
			was_document_completely_parsed_before_stopping = TRUE;
		}
	}
	else
	{
		was_document_completely_parsed_before_stopping = FALSE;
	}

	if (aborting)
		loading_aborted = aborting;

#ifdef WAND_SUPPORT
	if (format && GetWindow()->IsCancelingLoading())
	{
		// Abort hack for not showing several wanddialogs.
		SetWandSubmitting(FALSE);
	}
#endif // WAND_SUPPORT

	OP_STATUS status = OpStatus::OK;

	if (url_data_desc)
	{
		if (format)
		{
			if (LoadData(FALSE, TRUE) == OpStatus::ERR_NO_MEMORY )
				status = OpStatus::ERR_NO_MEMORY;
		}

		OP_DELETE(url_data_desc);
		url_data_desc = NULL;
	}

	StopLoadingAllInlines(stop_plugin_streams);

	if (frm_root)
		frm_root->StopLoading(format, aborting);
	else if (doc)
	{
		if (doc->StopLoading(format, aborting) == OpStatus::ERR_NO_MEMORY)
			status = OpStatus::ERR_NO_MEMORY;

		if (ifrm_root)
			ifrm_root->StopLoading(format, aborting);
	}
	else if (logdoc)
		logdoc->StopLoading(aborting);

	ResetWaitForStyles();
	UnsetAllCallbacks(FALSE);

	if (GetHLDocProfile())
	{
#ifdef DELAYED_SCRIPT_EXECUTION
		GetHLDocProfile()->ESStopLoading();
#endif // DELAYED_SCRIPT_EXECUTION

		if (GetHLDocProfile()->GetESLoadManager()->CancelInlineThreads() == OpStatus::ERR_NO_MEMORY)
			status = OpStatus::ERR_NO_MEMORY;
	}

	if (format)
		if( CheckOnLoad(this) == OpStatus::ERR_NO_MEMORY )
			status = OpStatus::ERR_NO_MEMORY;

	return(status);
}

OP_BOOLEAN FramesDocument::LoadAllImages(BOOL first_image_only)
{
	if (!GetWindow()->ShowImages())
		return OpBoolean::IS_FALSE;

	BOOL loaded_images = FALSE;
	DocumentTreeIterator iter(this);
	iter.SetIncludeThis();
	while (iter.Next())
	{
		FramesDocument* frm_doc = iter.GetFramesDocument();
		frm_doc->image_info.Reset();

		OP_BOOLEAN loaded_image = frm_doc->LoadImages(first_image_only);
		RETURN_IF_ERROR(loaded_image);
		if (loaded_image == OpBoolean::IS_TRUE)
			loaded_images = TRUE;
	}
	return loaded_images ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}


/* virtual */
OP_STATUS FramesDocument::SetMode(BOOL win_show_images, BOOL win_load_images, CSSMODE win_css_mode, CheckExpiryType check_expiry)
{
	OP_STATUS status = OpStatus::OK;
	OpStatus::Ignore(status);
#ifdef FIT_LARGE_IMAGES_TO_WINDOW_WIDTH
	packed.fit_images  = GetWindow()->FitImages() && !GetURL().IsImage();
#endif // FIT_LARGE_IMAGES_TO_WINDOW_WIDTH
	if (frm_root)
	{
		return frm_root->SetMode(win_show_images, win_load_images, win_css_mode, check_expiry);
	}
	else
		if (doc)
		{
			BOOL load_frame_images = FALSE;

			if (win_load_images != GetLoadImages())
			{
				packed.load_images = win_load_images;
				if(win_load_images)
					load_frame_images = TRUE;  // also load images for frames

#ifdef FLEXIBLE_IMAGE_POLICY
				BOOL stop_and_start_loading = !(url.IsImage() && IsTopDocument());
#else
				BOOL stop_and_start_loading = TRUE;
#endif // FLEXIBLE_IMAGE_POLICY
				if (stop_and_start_loading)
				{
					if (packed.load_images)
					{
						LoadImages(FALSE);
					}
					else
					{
						StopLoadingAllImages();

						// FIXME: need a check for finished loading (either here or in StopLoadingAllImages)
					}
				}
			}

			BOOL image_changed = FALSE;

#ifdef FLEXIBLE_IMAGE_POLICY
			// Always show images if the user loads nothing but the image.
			if (url.IsImage() && IsTopDocument())
				win_show_images = TRUE;
#endif // !FLEXIBLE_IMAGE_POLICY

			if (!win_show_images != !packed.show_images)
			{
				packed.show_images = win_show_images;
				image_changed = TRUE;
			}

			BOOL set_css_properties = FALSE;

			if (GetHandheldEnabled() != GetWindow()->GetHandheld())
				set_css_properties = TRUE;

			if (GetHLDocProfile() && (win_css_mode != GetHLDocProfile()->GetCSSMode()))
			{
				GetHLDocProfile()->SetCSSMode(win_css_mode);
				set_css_properties = TRUE;
			}
			else
				if (image_changed && logdoc)
				{
					HTML_Element* root_element = logdoc->GetRoot();

					if (root_element)
						root_element->MarkImagesDirty(this);

					if (GetVisualDevice())
						GetVisualDevice()->UpdateAll();
				}

			BOOL scale_changed = FALSE;

			if (scale_changed || load_frame_images || image_changed || set_css_properties)
			{
				// Don't forget the iframes:

				if (ifrm_root)
					for (FramesDocElm *fde = ifrm_root->FirstChild(); fde; fde=fde->Suc())
						if (fde->SetMode(win_show_images, win_load_images, win_css_mode, check_expiry) == OpStatus::ERR_NO_MEMORY)
							return OpStatus::ERR_NO_MEMORY;
			}
		}

	return status;
}

void FramesDocument::ClearScreenOnLoad(BOOL new_keep_cleared)
{
	if (IsTopDocument())
	{
		ViewportController* controller = GetWindow()->GetViewportController();
		OpViewportRequestListener* request_listener = controller->GetViewportRequestListener();
		OpViewportInfoListener* info_listener = controller->GetViewportInfoListener();

		OpViewportRequestListener::ViewportPosition pos(
			OpPoint(0, 0),
			OpViewportRequestListener::EDGE_LEFT,
			OpViewportRequestListener::EDGE_TOP);

		LayoutWorkplace* workplace = NULL;
		if (logdoc)
			workplace = logdoc->GetLayoutWorkplace();
# ifdef SUPPORT_TEXT_DIRECTION
		// FIXME: This is proably too early to safely detect RTLedness in many cases.

		if (workplace && workplace->IsRtlDocument())
		{
			pos.point.x = workplace->GetLayoutViewWidth() - 1;
			pos.hor_edge = OpViewportRequestListener::EDGE_RIGHT;
		}
# endif // SUPPORT_TEXT_DIRECTION

		double min_zoom_level = ZoomLevelNotSet;
		double max_zoom_level = ZoomLevelNotSet;
		double initial_zoom_level = ZoomLevelNotSet;
		BOOL user_zoomable = TRUE;
		short layout_view_w = 0;
		long layout_view_h = 0;
# ifdef CSS_VIEWPORT_SUPPORT

		HLDocProfile* hld_profile = GetHLDocProfile();
		if (hld_profile)
		{
			CSSCollection* css_coll = hld_profile->GetCSSCollection();
			CSS_Viewport* css_viewport = css_coll->GetViewport();

			if (css_viewport->GetMinZoom() != CSS_VIEWPORT_ZOOM_AUTO)
				min_zoom_level = css_viewport->GetMinZoom();

			if (css_viewport->GetMaxZoom() != CSS_VIEWPORT_ZOOM_AUTO)
				max_zoom_level = css_viewport->GetMaxZoom();

			user_zoomable = css_viewport->GetUserZoom();

			if (css_viewport->GetZoom() != CSS_VIEWPORT_ZOOM_AUTO)
				initial_zoom_level = css_viewport->GetZoom();
		}
#endif // CSS_VIEWPORT_SUPPORT

		if (workplace)
		{
			layout_view_w = workplace->GetLayoutViewWidth();
			layout_view_h = workplace->GetLayoutViewHeight();
		}

		GetVisualDevice()->SetLayoutScale(GetWindow()->GetTrueZoomBaseScale());

		info_listener->OnZoomLevelLimitsChanged(controller, min_zoom_level, max_zoom_level, user_zoomable);
		request_listener->OnVisualViewportEdgeChangeRequest(controller, pos, VIEWPORT_CHANGE_REASON_NEW_PAGE);
		if (0 < layout_view_w || 0 < layout_view_h)
			info_listener->OnLayoutViewportSizeChanged(controller, layout_view_w, layout_view_h);
		if (ZoomLevelNotSet != initial_zoom_level)
		{
			request_listener->OnZoomLevelChangeRequest(controller, initial_zoom_level,
			                                           0, VIEWPORT_CHANGE_REASON_NEW_PAGE);
		}
	}
	else
		RequestSetVisualViewPos(0, 0, VIEWPORT_CHANGE_REASON_NEW_PAGE);

	if (new_keep_cleared)
		keep_cleared = TRUE;

	VisualDevice* vis_dev = GetVisualDevice();

	vis_dev->SetDefaultBgColor();
	vis_dev->UpdateAll();
}

BOOL FramesDocument::UrlNeedReload(CheckExpiryType check_expired)
{
	if (url.GetAttribute(URL::KCachePolicy_MustRevalidate))
		return TRUE;

	// If there is still a data descriptor, the URL was only partly loaded.
	// But a local file will never resume loading, so we should reload it.
	if (url.Type() == URL_FILE && url_data_desc)
		return TRUE;

	BYTE stat = url.Status(FALSE);
	if (stat == URL_UNLOADED || stat == URL_LOADING || stat == URL_LOADING_ABORTED || stat == URL_LOADING_FAILURE)
		return TRUE;

	if (check_expired != CHECK_EXPIRY_NEVER &&
		url.Expired(FALSE, check_expired == CHECK_EXPIRY_USER))
		return TRUE;

	URL nxt_url = url.GetAttribute(URL::KMovedToURL);
	while (!nxt_url.IsEmpty())
	{
		stat = nxt_url.Status(FALSE);
		if (stat == URL_UNLOADED || stat == URL_LOADING || stat == URL_LOADING_ABORTED || stat == URL_LOADING_FAILURE)
			return TRUE;
		else
			if (check_expired != CHECK_EXPIRY_NEVER &&
				url.Expired(FALSE, check_expired == CHECK_EXPIRY_USER))
				return TRUE;
			else
				nxt_url = nxt_url.GetAttribute(URL::KMovedToURL);
	}

	if (frm_root)
	{
		// check if any of frame documents need reload ???
		// what if any of them is already loading ???
	}

	return FALSE;
}

const uni_char* FramesDocument::Title(TempBuffer* buffer)
{
	OP_ASSERT(buffer);
	if (logdoc)
		return logdoc->Title(buffer);
	else
		return 0;
}

OP_STATUS FramesDocument::GetDescription(OpString &desc)
{
	HLDocProfile *hld_profile = GetHLDocProfile();
	if (hld_profile)
		return desc.Set(hld_profile->GetDescription());
	else
	{
		desc.Empty();
		return OpStatus::OK;
	}
}

void FramesDocument::SetNoActiveFrame(FOCUS_REASON reason /* = FOCUS_REASON_ACTIVATE */)
{
	// we don't want to move focus to a frameset document, so keep active frame
	// if top document is a frameset
	if (active_frm_doc && !GetTopDocument()->IsFrameDoc())
	{
		SetActiveFrame(NULL, TRUE, reason);
		active_frm_doc = NULL;
	}
}

#ifndef HAS_NOTEXTSELECTION

OP_BOOLEAN FramesDocument::SelectAll(BOOL select/*=TRUE*/)
{
	if (doc)
		return doc->SelectAll(select);

	if (active_frm_doc)
	{
		FramesDocument* adoc = active_frm_doc->GetCurrentDoc();

		if (adoc)
			return adoc->SelectAll(select);
	}

	return OpStatus::OK;
}

OP_BOOLEAN
FramesDocument::DeSelectAll()
{
	OP_BOOLEAN ret = OpBoolean::IS_FALSE;
	DocumentTreeIterator iter(this);
	iter.SetIncludeThis();

	while (iter.Next())
	{
		if (OpBoolean::IS_TRUE == iter.GetFramesDocument()->SelectAll(FALSE))
			ret = OpBoolean::IS_TRUE;
	}

	return ret;
}

OP_STATUS FramesDocument::DOMSetSelection(const SelectionBoundaryPoint* start, const SelectionBoundaryPoint* end, BOOL end_is_focus)
{
	RETURN_IF_ERROR(SetSelection(start, end, end_is_focus));
#ifdef DOCUMENT_EDIT_SUPPORT
	if (GetDocumentEdit())
		GetDocumentEdit()->OnDOMChangedSelection();
#endif // DOCUMENT_EDIT_SUPPORT
#if defined DOCUMENT_EDIT_SUPPORT || defined KEYBOARD_SELECTION_SUPPORT
	if (caret_painter)
	{
		caret_painter->UpdatePos();
		caret_painter->UpdateWantedX();
	}
#endif // DOCUMENT_EDIT_SUPPORT || KEYBOARD_SELECTION_SUPPORT
	return OpStatus::OK;
}

OP_STATUS FramesDocument::SetSelection(const SelectionBoundaryPoint* start, const SelectionBoundaryPoint* end, BOOL end_is_focus, BOOL remember_old_wanted_x_position)
{
	OP_ASSERT(start && start->GetElement());
	OP_ASSERT(end && end->GetElement());
#ifdef _DEBUG
	HTML_Element* start_root = start->GetElement();
	while (start_root->Parent())
		start_root = start_root->Parent();
	HTML_Element* end_root = end->GetElement();
	while (end_root->Parent())
		end_root = end_root->Parent();
#ifdef DOCUMENT_EDIT_SUPPORT
	// DocumentEdit has a historic right to do stupid things but is also responsible for cleaning up.
	if (!GetDocumentEdit())
#endif // DOCUMENT_EDIT_SUPPORT
	{
		OP_ASSERT(end_root->Type() == HE_DOC_ROOT || !"Putting selections in disconnected subtrees is not supported and might cause crashes#2. DocumentEdit has a historic right to do stupid things but is also responsible for cleaning up. Eventually with a newer and better DocumentEdit that part of the assert can be removed.");
		OP_ASSERT(start_root->Type() == HE_DOC_ROOT || !"Putting selections in disconnected subtrees is not supported and might cause crashes#1. DocumentEdit has a historic right to do stupid things but is also responsible for cleaning up. Eventually with a newer and better DocumentEdit that part of the assert can be removed.");
	}
#endif // _DEBUG
	ClearDocumentSelection(FALSE, FALSE, TRUE);

	OP_ASSERT(!text_selection);
	text_selection = OP_NEW(TextSelection, ());

	if (text_selection)
	{
		if (end_is_focus)
			text_selection->SetNewSelectionPoints(this, *start, *end, TRUE, remember_old_wanted_x_position);
		else
			text_selection->SetNewSelectionPoints(this, *end, *start, TRUE, remember_old_wanted_x_position);

		if (doc && doc->GetElementWithSelection() && !text_selection->IsEmpty())
			doc->SetElementWithSelection(NULL);
	}
	return text_selection ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

OP_STATUS FramesDocument::StartSelection(int x, int y)
{
	// Activate this frame.
	FramesDocElm* frame = GetDocManager()->GetFrame();
	if (frame)
		GetTopDocument()->SetActiveFrame(frame);
	else
		GetTopDocument()->SetNoActiveFrame();

	if (doc)
		return doc->StartSelection(x, y);

	return OpStatus::OK;
}

OP_STATUS FramesDocument::MoveSelectionAnchorPoint(int x, int y)
{
	// Activate this frame.
	FramesDocElm* frame = GetDocManager()->GetFrame();
	if (frame)
		GetTopDocument()->SetActiveFrame(frame);
	else
		GetTopDocument()->SetNoActiveFrame();

	if (doc)
		return doc->MoveSelectionAnchorPoint(x, y);

	return OpStatus::OK;
}

void FramesDocument::MoveSelectionFocusPoint(int x, int y, BOOL end_selection)
{
	if (doc)
		doc->MoveSelectionFocusPoint(x, y, end_selection);
}

OP_STATUS FramesDocument::SelectWord(int x, int y)
{
	// Activate this frame.
	FramesDocElm* frame = GetDocManager()->GetFrame();
	if (frame)
		GetTopDocument()->SetActiveFrame(frame);
	else
		GetTopDocument()->SetNoActiveFrame();

	if (doc)
		return doc->SelectWord(x, y);

	return OpStatus::ERR;
}

#ifdef NEARBY_INTERACTIVE_ITEM_DETECTION
OP_STATUS FramesDocument::SelectNearbyLink(int x, int y)
{
	OpVector<HTML_Element> item_list;
	OP_STATUS res = FindNearbyLinks(OpRect(x, y, 1, 1), item_list);
	if (OpStatus::IsError(res))
		return res;

	if (item_list.GetCount() == 0)
		return OpStatus::ERR;

	// Activate this frame.
	FramesDocElm* frame = GetDocManager()->GetFrame();
	if (frame)
		GetTopDocument()->SetActiveFrame(frame);
	else
		GetTopDocument()->SetNoActiveFrame();

	// If we have multiple overlapping links we use the topmost one which is
	// the last one in the link items list.
	HTML_Element* html_elm = item_list.Get(item_list.GetCount() - 1);
	return doc ? doc->SelectElement(html_elm) : OpStatus::ERR;
}
#endif // NEARBY_INTERACTIVE_ITEM_DETECTION

void FramesDocument::EndSelectionIfSelecting(int x, int y)
{
	start_selection = FALSE;
	was_selecting_text = is_selecting_text && selecting_text_is_ok;

	if (is_selecting_text)
	{
#if defined DOCUMENT_EDIT_SUPPORT || defined KEYBOARD_SELECTION_SUPPORT
		if (GetCaretPainter() && GetTextSelection() && !GetTextSelection()->GetStartElement())
			// Always allow people to position the selection in
			// documents with a visible collapsed selection (i.e. caret).
			selecting_text_is_ok = TRUE;
#endif // DOCUMENT_EDIT_SUPPORT
		if (selecting_text_is_ok)
			MoveSelectionFocusPoint(x, y, TRUE);

		is_selecting_text = FALSE;
		selecting_text_is_ok = FALSE;
		selection_scroll_active = FALSE;
		g_opera->doc_module.m_selection_scroll_document = NULL;
	}
}

OP_STATUS FramesDocument::GetSelectionAnchorPosition(int &x, int &y, int &line_height)
{
	if (!GetTextSelection() || GetTextSelection()->IsEmpty())
		return OpStatus::ERR;

	OpPoint start_pos;
	int start_line_height = 0;

	OP_STATUS res = GetTextSelection()->GetAnchorPointPosition(this, start_pos, start_line_height);
	if (OpStatus::IsSuccess(res))
	{
		x = start_pos.x;
		y = start_pos.y;
		line_height = start_line_height;
	}

	return res;
}

OP_STATUS FramesDocument::GetSelectionFocusPosition(int &x, int &y, int &line_height)
{
	if (!GetTextSelection() || GetTextSelection()->IsEmpty())
		return OpStatus::ERR;

	OpPoint end_pos;
	int end_line_height = 0;

	OP_STATUS res = GetTextSelection()->GetFocusPointPosition(this, end_pos, end_line_height);
	if (OpStatus::IsSuccess(res))
	{
		x = end_pos.x;
		y = end_pos.y;
		line_height = end_line_height;
	}

	return res;
}

long FramesDocument::GetSelectedTextLen(BOOL include_element_selection /* = FALSE */)
{
	if (active_frm_doc)
	{
		if (FramesDocument* active_doc = active_frm_doc->GetCurrentDoc())
			return active_doc->GetSelectedTextLen(include_element_selection);
	}
	else if (doc)
		return doc->GetSelectedTextLen(include_element_selection);

	return 0;
}

BOOL FramesDocument::HasSelectedText(BOOL include_element_selection /* = FALSE */)
{
	if (active_frm_doc)
	{
		if (FramesDocument* active_doc = active_frm_doc->GetCurrentDoc())
			return active_doc->HasSelectedText(include_element_selection);
	}
	else if (doc)
		return doc->HasSelectedText(include_element_selection);

	return FALSE;
}

BOOL FramesDocument::GetSelectedText(uni_char *buf, long buf_len, BOOL include_element_selection /* = FALSE */)
{
	if (active_frm_doc)
	{
		if (FramesDocument* active_doc = active_frm_doc->GetCurrentDoc())
			return active_doc->GetSelectedText(buf, buf_len, include_element_selection);
	}
	else if (doc)
		return doc->GetSelectedText(buf, buf_len, include_element_selection);

	return FALSE;
}


#if !defined HAS_NOTEXTSELECTION && defined _X11_SELECTION_POLICY_
OP_STATUS FramesDocument::CopySelectedTextToMouseSelection()
{
	g_clipboard_manager->SetMouseSelectionMode(TRUE);
	OP_STATUS status = g_clipboard_manager->Copy(GetClipboardListener(), GetWindow()->GetUrlContextId());
	g_clipboard_manager->SetMouseSelectionMode(FALSE);

	return status;
}
#endif // !defined HAS_NOTEXTSELECTION && defined _X11_SELECTION_POLICY_


void FramesDocument::ExpandSelection(TextSelectionType selection_type)
{
	if (doc)
		doc->ExpandSelection(selection_type);
}


void FramesDocument::MaybeStartTextSelection(int mouse_x, int mouse_y)
{
	BOOL can_start_textselection = !g_pcdoc->GetIntegerPref(PrefsCollectionDoc::DisableTextSelect, GetHostName());

#ifdef GRAB_AND_SCROLL
	can_start_textselection = can_start_textselection && !GetWindow()->GetScrollIsPan();
#endif // GRAB_AND_SCROLL

#ifdef SVG_SUPPORT
	can_start_textselection = can_start_textselection && !g_svg_manager->IsInTextSelectionMode();
#endif // SVG_SUPPORT

	if (can_start_textselection)
	{
		HTML_Document* htmldoc = GetHtmlDocument();
		BOOL can_restart_selection = TRUE;

		if (can_restart_selection && HasSelectedText() && htmldoc && htmldoc->GetHoverHTMLElement())
		{
			// Don't start selection if we're mousing down on something
			// that are likely to start a script (either via a javascript
			// url or via a event listener) that might ask for current
			// selection. This assumes that the hover element
			// corresponds to the coordinates sent in here. Probably true.
			HTML_Element* hovered = htmldoc->GetHoverHTMLElement();

			while (hovered && hovered->IsText())
			{
				hovered = hovered->ParentActual(); // Use last_descendent here?
			}

			can_restart_selection = !hovered ||
				!(hovered->Type() == HE_A || hovered->Type() == HE_IMG ||
				  hovered->Type() == HE_BUTTON || FormManager::IsButton(hovered) ||
				  hovered->Type() == HE_INPUT && hovered->GetInputType() == INPUT_IMAGE);
		}

		// Don't start selection if the hovered element is unselectable.
		BOOL unselectable = FALSE;
		BOOL is_svg = FALSE;
		if (htmldoc && htmldoc->GetHoverHTMLElement())
		{
			HTML_Element* hovelm = htmldoc->GetHoverHTMLElement();
			if (hovelm->Type() == HE_TEXT && hovelm->ParentActual()->GetUnselectable())
				unselectable = TRUE;
			else if (hovelm->GetUnselectable())
				unselectable = TRUE;
#ifdef SVG_SUPPORT
			if (hovelm->GetNsType() == NS_SVG)
				is_svg = TRUE; // SVGManager manages selection in SVG
#endif // SVG_SUPPORT
		}

		OP_BOOLEAN within_selection = OpBoolean::IS_FALSE;
#ifdef DRAG_SUPPORT
		within_selection = htmldoc ? htmldoc->IsWithinSelection(mouse_x, mouse_y) : OpBoolean::IS_FALSE;
		if (OpStatus::IsMemoryError(within_selection))
		{
			GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			return; // No point in continuing due to OOM.
		}
#endif // DRAG_SUPPORT

		// Start a selection only when the content under the mouse is selectable, is not SVG element and it's not the selection too.
		// The last is needed in order to allow selection dragging.
		if (!unselectable && !is_svg && within_selection == OpBoolean::IS_FALSE)
			if (can_restart_selection)
			{
				if (StartSelection(mouse_x, mouse_y) == OpStatus::ERR_NO_MEMORY)
					GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				else
				{
					SetSelectingText(TRUE);
					selecting_text_is_ok = FALSE;
				}
			}
			else
			{
				// We hit a link or a button. We don't want to start
				// selection immediately because the link may be a
				// script that want to read the selection we have, so
				// clearing it here will make that impossible.
				// Will be started if mouse is moved in Document::MouseMove.
				start_selection = TRUE;
				start_selection_x = mouse_x;
				start_selection_y = mouse_y;
			}
	}
	else if (HasSelectedText())
	{
#ifdef DRAG_SUPPORT
		if (GetHtmlDocument() && GetHtmlDocument()->IsWithinSelection(mouse_x, mouse_y) == OpBoolean::IS_FALSE)
#endif // DRAG_SUPPORT
			// This line of code isn't obvious. Not even sure it's right.
			GetTopDocument()->ClearSelection(TRUE);
	}
}

#endif // !HAS_NOTEXTSELECTION

void FramesDocument::RemoveFromSelection(HTML_Element* element)
{
	if (!is_being_deleted)
	{
		if (text_selection)
		{
			// We only call the TextSelection::RemoveElement method for elements in
			// the main tree, and we do that limitation for two reasons:
			// 1. Optimization. The selection will not point to any elements in disconnected
			//    subtrees anyway since that would cause crashes. But see #2.
			// 2. Documentedit. It will put pointers to elements in disconnected subtrees
			//    here and we must not in any way modify them until those subtrees are
			//    put back into the main tree again. Long term documentedit should stop
			//    doing that but that is not a small project.
			if (GetDocRoot() && GetDocRoot()->IsAncestorOf(element))
				text_selection->RemoveElement(element);
		}

		if (doc)
			doc->RemoveFromSearchSelection(element);
	}
}

void FramesDocument::RemoveLayoutCacheFromSelection(HTML_Element* element)
{
	if (!is_being_deleted && doc)
		doc->RemoveLayoutCacheFromSearchHits(element);
}

void FramesDocument::ClearDocumentSelection(BOOL clear_focus_and_highlight, BOOL clear_search, BOOL completely_remove_selection)
{
	if (text_selection && (completely_remove_selection || !text_selection->IsEmpty()))
	{
		text_selection->Clear(this);
		OP_DELETE(text_selection);
		text_selection = NULL;
#if defined KEYBOARD_SELECTION_SUPPORT || defined DOCUMENT_EDIT_SUPPORT
		if (caret_painter)
			caret_painter->StopBlink();
#endif // KEYBOARD_SELECTION_SUPPORT || DOCUMENT_EDIT_SUPPORT
	}

	if (doc)
		doc->ClearDocumentSelection(clear_focus_and_highlight, clear_search);
}

void FramesDocument::ClearSelection(BOOL clear_focus_and_highlight/*=TRUE*/, BOOL clear_search/*=FALSE*/)
{
	DocumentTreeIterator iter(this);
	iter.SetIncludeThis();
	while (iter.Next())
		iter.GetFramesDocument()->ClearDocumentSelection(clear_focus_and_highlight, clear_search);
}

#ifndef HAS_NO_SEARCHTEXT

void FramesDocument::RemoveSearchHits()
{
	DocumentTreeIterator iter(this);
	iter.SetIncludeThis();
	while (iter.Next())
	{
		HTML_Document* html_doc = iter.GetFramesDocument()->doc;
		if (html_doc)
			html_doc->RemoveSearchHits();
	}
}

# ifdef SEARCH_MATCHES_ALL
OP_BOOLEAN
FramesDocument::HighlightNextMatch(SearchData *data, OpRect& rect)
{
	OP_BOOLEAN result = OpBoolean::IS_FALSE;
	FramesDocElm *frame = frm_root;

	if (doc)
	{
		result = doc->HighlightNextMatch(data, rect);
		frame = ifrm_root;
	}

	if (frame && (frame = (FramesDocElm *) frame->FirstLeaf()) != NULL)
		do
			if (FramesDocument *fdoc = frame->GetCurrentDoc())
			{
				result = fdoc->HighlightNextMatch(data, rect);
				if (result == OpBoolean::IS_TRUE && fdoc->GetVisualDevice())
				{
					rect.x += frame->GetAbsX() - fdoc->GetVisualDevice()->GetRenderingViewX();
					rect.y += frame->GetAbsY() - fdoc->GetVisualDevice()->GetRenderingViewY();
				}
			}
		while ((frame = (FramesDocElm *) frame->NextLeaf()) != NULL);

	return result;
}
# endif // SEARCH_MATCHES_ALL

OP_BOOLEAN FramesDocument::SearchText(const uni_char* txt, int len, BOOL forward, BOOL match_case, BOOL words, BOOL next, BOOL wrap, int &left_x, long &top_y, int &right_x, long &bottom_y, BOOL only_links /* = FALSE */)
{
	if (active_frm_doc)
	{
		FramesDocument* active_doc = active_frm_doc->GetCurrentDoc();
		if (active_doc)
			return active_doc->SearchText(txt, len, forward, match_case, words, next, wrap, left_x, top_y, right_x, bottom_y, only_links);
	}
	else if (doc)
		return doc->SearchText(txt, len, forward, match_case, words, next, wrap, only_links, left_x, top_y, right_x, bottom_y);

	return OpBoolean::IS_FALSE;
}
#endif // !HAS_NO_SEARCHTEXT

void FramesDocument::BeforeTextDataChange(HTML_Element* text_node)
{
	if (text_selection)
		text_selection->BeforeTextDataChange(text_node);

	if (doc)
		doc->BeforeTextDataChange(text_node);
}

void FramesDocument::AbortedTextDataChange(HTML_Element* text_node)
{
	if (text_selection)
		text_selection->AbortedTextDataChange(text_node);

	if (doc)
		doc->AbortedTextDataChange(text_node);
}

void FramesDocument::TextDataChange(HTML_Element* text_element, HTML_Element::ValueModificationType modification_type, unsigned offset, unsigned length1, unsigned length2)
{
	if (text_selection && GetDocRoot() && GetDocRoot()->IsAncestorOf(text_element))
		text_selection->TextDataChange(text_element, modification_type, offset, length1, length2);

	if (doc)
		doc->TextDataChange(text_element, modification_type, offset, length1, length2);
}

void FramesDocument::OnTextConvertedToTextGroup(HTML_Element* elm)
{
	// Selections point to text elements and elements, not to textgroups. If a text element
	// changes we need to modify the selection as well.
	if (text_selection)
		text_selection->OnTextConvertedToTextGroup(elm);
#ifdef DOCUMENT_EDIT_SUPPORT
	if (GetDocumentEdit())
		GetDocumentEdit()->OnTextConvertedToTextGroup(elm);
#endif // DOCUMENT_EDIT_SUPPORT
	if (doc)
		doc->OnTextConvertedToTextGroup(elm);
}

#ifdef NEARBY_INTERACTIVE_ITEM_DETECTION
FrameDocHitTestIterator::FrameDocHitTestIterator(FramesDocument* doc, const OpRect& rect, const AffinePos& pos) : next(NULL)
{
	OP_ASSERT(doc);

	current.doc = doc;
	current.rect = rect;
	current.pos = pos;

	FramesDocElm* root = doc->GetFrmDocRoot() ? doc->GetFrmDocRoot() : doc->GetIFrmRoot();
	if (root)
		next = root->FirstLeaf();
}

BOOL FrameDocHitTestIterator::Advance()
{
	FramesDocElm* leaf = next;

	while (leaf)
	{
		next = next->NextLeaf();

		FramesDocument* parent_doc = leaf->GetParentFramesDoc();
		FramesDocument* leaf_doc = leaf->GetCurrentDoc();
		if (leaf_doc)
		{
			AffinePos pos = leaf->GetAbsPos();
			OpRect leaf_rect = current.rect;
			BOOL frame_hit = TRUE;

			pos.ApplyInverse(leaf_rect);
			leaf_rect.IntersectWith(OpRect(0, 0, leaf->GetWidth(), leaf->GetHeight()));

			if (leaf_rect.IsEmpty())
			{
				leaf = leaf->NextLeaf();
				continue;
			}

			if (leaf->IsInlineFrame())
			{
				/* Check whether the iframe is not overlapped in the leaf_rect part in the parent doc.
				   To make things simpler, we take just the central point of the leaf_rect. */

				OpPoint parent_doc_point(leaf_rect.x + leaf_rect.width / 2, leaf_rect.y + leaf_rect.height / 2);
				pos.Apply(parent_doc_point);
				HTML_Element* root_elm = parent_doc->GetLogicalDocument() ? parent_doc->GetLogicalDocument()->GetRoot() : NULL;
				if (!root_elm)
					return FALSE; // If there is no root, then don't descend into any frames.

				Box* box = root_elm->GetInnerBox(parent_doc_point.x, parent_doc_point.y, parent_doc);

				if (!(box && box->GetHtmlElement() == leaf->GetHtmlElement()))
					frame_hit = FALSE;
			}

			if (frame_hit)
			{
				VisualDevice* leaf_vis_dev = leaf->GetVisualDevice();
				OpPoint offset_for_leaf(leaf_vis_dev->GetRenderingViewX(), leaf_vis_dev->GetRenderingViewY());
				// Adjust by the exact document's position in a frame/iframe
				offset_for_leaf -= leaf_vis_dev->ScaleToDoc(leaf_vis_dev->GetInnerPosition());

				current.rect = leaf_rect;
				current.rect.OffsetBy(offset_for_leaf);

				current.pos.Append(pos);
				current.pos.Append(AffinePos(-offset_for_leaf.x, -offset_for_leaf.y));

				current.doc = leaf_doc;

				return TRUE;
			}
		}

		leaf = next;
	}

	return FALSE;
}

OP_STATUS FramesDocument::FindNearbyInteractiveItems(const OpRect& rect, List<InteractiveItemInfo>& list, const AffinePos& doc_pos)
{
	// Top document has translation equal to zero.
	OP_ASSERT(!IsTopDocument() || doc_pos == AffinePos(0, 0));

	InteractiveItemInfo* iter = list.Last();
	OpRect rect_adjusted = rect;

	if (GetDocManager()->GetFrame())
	{
		// Adding frame's scrollbars and limiting the rect_adjusted.
		RETURN_IF_ERROR(GetDocManager()->GetFrame()->AddFrameScrollbars(rect_adjusted, list));
	}
	else
		rect_adjusted.IntersectWith(GetVisualViewport()); // Just in case.

	if (doc && !rect_adjusted.IsEmpty())
	{
		// At this point rect_adjusted cannot be outside the rect of the root of this doc.
		OP_ASSERT(-rect_adjusted.x <= NegativeOverflow());
		RETURN_IF_ERROR(doc->FindNearbyInteractiveItems(rect_adjusted, list));
	}

	// Adjust rects of found links with this document's AffinePos to top doc root.
	if (!IsTopDocument())
	{
		iter = iter ? iter->Suc() : list.First();

		while (iter)
		{
			RETURN_IF_MEMORY_ERROR(iter->PrependPos(doc_pos));
			iter = iter->Suc();
		}
	}

	if (rect_adjusted.IsEmpty())
		return OpStatus::OK;

	FrameDocHitTestIterator frame_iterator(this, rect_adjusted, doc_pos);
	while (frame_iterator.Advance())
	{
		const FrameDocHitTestItem& item = frame_iterator.Current();
		RETURN_IF_ERROR(item.doc->FindNearbyInteractiveItems(item.rect, list,
															 item.pos));
	}

	return OpStatus::OK;
}

OP_STATUS FramesDocument::FindNearbyLinks(const OpRect& rect, OpVector<HTML_Element>& list)
{
	OpRect rect_adjusted = rect;

	if (!GetDocManager()->GetFrame())
		rect_adjusted.IntersectWith(GetVisualViewport()); // Just in case.

	if (doc && !rect_adjusted.IsEmpty())
	{
		// At this point rect_adjusted cannot be outside the rect of the root of this doc.
		OP_ASSERT(-rect_adjusted.x <= NegativeOverflow());
		RETURN_IF_ERROR(doc->FindNearbyLinks(rect_adjusted, list));
	}

	if (rect_adjusted.IsEmpty())
		return OpStatus::OK;

	FrameDocHitTestIterator frame_iterator(this, rect_adjusted, AffinePos(0, 0));
	while (frame_iterator.Advance())
	{
		const FrameDocHitTestItem& item = frame_iterator.Current();
		RETURN_IF_ERROR(item.doc->FindNearbyLinks(item.rect, list));
	}

	return OpStatus::OK;
}
#endif // NEARBY_INTERACTIVE_ITEM_DETECTION

BOOL FramesDocument::HighlightNextElm(HTML_ElementType lower_tag, HTML_ElementType upper_tag, BOOL forward, BOOL all_anchors/*=FALSE*/)
{
	if (active_frm_doc)
	{
		FramesDocument* adoc = active_frm_doc->GetCurrentDoc();

		if (adoc)
			return adoc->HighlightNextElm(lower_tag, upper_tag, forward, all_anchors);
	}
	else
		if (doc)
			return doc->HighlightNextElm(lower_tag, upper_tag, forward, all_anchors);

	return FALSE;
}

#if defined KEYBOARD_SELECTION_SUPPORT || defined DOCUMENT_EDIT_SUPPORT
void FramesDocument::PaintCaret(VisualDevice* vd)
{
	if (caret_painter)
		caret_painter->Paint(vd);
}
#endif // KEYBOARD_SELECTION_SUPPORT || DOCUMENT_EDIT_SUPPORT

#ifdef KEYBOARD_SELECTION_SUPPORT

OP_STATUS FramesDocument::SetKeyboardSelectable(BOOL on)
{
	if (on)
	{
		if (!GetKeyboardSelectionMode())
		{
#ifdef _PRINT_SUPPORT_
			if (GetWindow()->GetPreviewMode() || GetWindow()->GetPrintMode())
				return OpStatus::ERR; // Block this if we're currently printing/displaying print preview.
#endif // _PRINT_SUPPORT_
			// Enable the keyboard selection mode.
			if (!caret_painter)
			{
				caret_painter = OP_NEW(CaretPainter, (this));
				if (!caret_painter)
					return OpStatus::ERR_NO_MEMORY;
			}
			SetKeyboardSelectionMode(TRUE);

			// Position the initial caret if there is no previous caret/selection.
			if (!GetTextSelection() || !GetTextSelection()->GetStartElement())
			{
				SelectionBoundaryPoint caret_point;
				if (HTML_Element* focus_element = GetFocusElement())
				{
					caret_point.SetLogicalPosition(focus_element, 0);

					HTML_Element* old_style_elm;
					int old_style_offset;
					TextSelection::ConvertPointToOldStyle(caret_point, old_style_elm, old_style_offset);

					HTML_Element* better_elm;
					int better_offset;
					if (CaretManager::GetValidCaretPosFrom(this, old_style_elm, old_style_offset, better_elm, better_offset))
						caret_point = TextSelection::ConvertFromOldStyle(better_elm, better_offset);
				}
				else if (logdoc && logdoc->GetBodyElm())
					caret_point.SetLogicalPosition(logdoc->GetBodyElm(), 0);

				if (doc && caret_point.GetElement())
					doc->SetSelection(&caret_point, &caret_point);
			}
			caret_painter->UpdatePos();
			caret_painter->UpdateWantedX();
			caret_painter->RestartBlink();
		}
	}
	else
	{
		// Disable the keyboard selection mode.
		if (GetKeyboardSelectionMode())
		{
#ifdef DOCUMENT_EDIT_SUPPORT
			if (!document_edit)
#endif // DOCUMENT_EDIT_SUPPORT
			{
				if (caret_painter)
				{
					caret_painter->StopBlink();
					OP_DELETE(caret_painter);
					caret_painter = NULL;
				}
			}
			SetKeyboardSelectionMode(FALSE);
		}
		GetWindow()->UpdateKeyboardSelectionMode(FALSE);
	}

	return OpStatus::OK;
}

void FramesDocument::MoveCaret(OpWindowCommander::CaretMovementDirection direction, BOOL in_selection_mode)
{
	if (GetKeyboardSelectionMode())
	{
		OP_ASSERT(GetCaretPainter());
		if (logdoc && logdoc->GetBodyElm())
		{
			SelectionBoundaryPoint old_anchor;
			if (GetTextSelection() && in_selection_mode)
				old_anchor = *GetTextSelection()->GetAnchorPoint();

			HTML_Element *containing_elm;
			if (direction == OpWindowCommander::CARET_DOCUMENT_HOME || direction == OpWindowCommander::CARET_DOCUMENT_END)
				containing_elm = logdoc->GetBodyElm();
			else
				containing_elm = caret_manager.GetElement() ? caret_manager.GetContainingElementActual(caret_manager.GetElement()) : logdoc->GetBodyElm();

			// Order the caret to be put at the indicated position.
			GetCaret()->MoveCaret(direction, containing_elm, logdoc->GetBodyElm());

			if (old_anchor.GetElement() && GetTextSelection())
			{
				BOOL moving_up_or_down = direction == OpWindowCommander::CARET_UP || OpWindowCommander::CARET_DOWN;
				SelectionBoundaryPoint new_focus = *GetTextSelection()->GetFocusPoint();
				GetTextSelection()->SetNewSelectionPoints(this, old_anchor, new_focus, TRUE, moving_up_or_down);
			}

			caret_painter->ScrollIfNeeded();

			if (doc && GetTextSelection() && GetTextSelection()->GetFocusPoint()->GetElement())
				doc->SetNavigationElement(GetTextSelection()->GetFocusPoint()->GetElement(), FALSE);
		}
	}
}

#ifdef KEYBOARD_SELECTION_SUPPORT
void FramesDocument::SetKeyboardSelectionMode(BOOL value)
{
	keyboard_selection_mode_enabled = value;
	GetWindow()->UpdateKeyboardSelectionMode(value);
}
#endif // KEYBOARD_SELECTION_SUPPORT

OP_STATUS FramesDocument::GetCaretRect(OpRect &rect)
{
	CaretPainter *caret_painter = GetCaretPainter();

	if (caret_painter && (GetKeyboardSelectionMode()
#ifdef DOCUMENT_EDIT_SUPPORT
		|| document_edit
#endif // DOCUMENT_EDIT_SUPPORT
			))
	{
		rect = caret_painter->GetCaretRectInDocument();
		return OpStatus::OK;
	}

	return OpStatus::ERR;
}
#endif // KEYBOARD_SELECTION_SUPPORT

URL FramesDocument::GetCurrentURL(const uni_char* &win_name, int unused_parameter /* = 0 */)
{
	URL curl;
	if (active_frm_doc)
	{
		FramesDocument* adoc = active_frm_doc->GetCurrentDoc();
		if (adoc)
			curl = adoc->GetCurrentURL(win_name);
	}
	else
		if (doc)
			curl = doc->GetCurrentURL(win_name);

	return curl;
}

void FramesDocument::SetCurrentHistoryPos(int n, BOOL parent_doc_changed, BOOL is_user_initiated)
{
	if (ifrm_root)
		ifrm_root->SetCurrentHistoryPos(n, parent_doc_changed, is_user_initiated);
	else if (frm_root)
		frm_root->SetCurrentHistoryPos(n, parent_doc_changed, is_user_initiated);
}

/* virtual */
HTML_Document* FramesDocument::GetHTML_Document()
{
	// Deprecated. Use GetHtmlDocument()
	return doc;
}

#ifdef MEDIA_SUPPORT
OP_STATUS FramesDocument::AddMedia(Media *media)
{
	if (media_elements.Find(media) < 0)
		return media_elements.Add(media);
	return OpStatus::OK;
}

void FramesDocument::RemoveMedia(Media *media)
{
	INT32 idx = media_elements.Find(media);
	if (idx >= 0)
		media_elements.Remove(idx);
}

void FramesDocument::SuspendAllMedia(BOOL remove)
{
	for (UINT32 idx = 0; idx < media_elements.GetCount(); idx++)
	{
		Media *media = media_elements.Get(idx);
		media->Suspend(remove);
	}

	if (remove)
		media_elements.Remove(0, media_elements.GetCount());
}

OP_STATUS FramesDocument::ResumeAllMedia()
{
	for (UINT32 idx = 0; idx < media_elements.GetCount(); idx++)
	{
		Media *media = media_elements.Get(idx);
		RETURN_IF_MEMORY_ERROR(media->Resume());
	}
	return OpStatus::OK;
}
#endif // MEDIA_SUPPORT

OP_STATUS FramesDocument::SetAsCurrentDoc(BOOL state, BOOL visible_if_current/*=TRUE*/)
{
	OP_NEW_DBG("FramesDocument::SetAsCurrentDoc", "doc");
	OP_DBG((UNI_L("this=%p; document_state=%p"), this, document_state));
	SetLocked(TRUE); // make sure that frame structure (frm_root) is not freed while in use

#ifdef WAND_SUPPORT
	// Abort hack for not showing several wanddialogs.
	SetWandSubmitting(FALSE);
#endif // WAND_SUPPORT

	/* Note: this function must not return without calling Document::SetAsCurrentDoc(state)
	   and calling SetLocked(FALSE) again or bad things will happen. */

	MessageHandler* mh = GetMessageHandler();

	// We want to be able to reflow if current doc is changed.
	is_reflow_msg_posted = FALSE;
	is_reflow_frameset_msg_posted = FALSE;

	OP_STATUS stat = OpStatus::OK;
	OpStatus::Ignore(stat);

	if (state)
		keep_cleared = FALSE;

	if (!state && IsCurrentDoc())
	{
		if (dom_environment)
			dom_environment->BeforeUnload();

#ifdef DOC_THREAD_MIGRATION
		if (GetESScheduler())
			GetESScheduler()->RemoveThreads(TRUE);
#else // DOC_THREAD_MIGRATION
		/* Slightly ugly, if a script in this document is generating another document
		   which is replacing this one, we'll get here while that script is executing,
		   but then it is too early to call RemoveThreads(TRUE). */
		if (GetESScheduler())
		{
			FramesDocument *document = this;
			while (document)
				if (document->es_generated_document)
					break;
				else
					document = document->GetParentDoc();
			if (!document)
				GetESScheduler()->RemoveThreads(TRUE);
		}
#endif // DOC_THREAD_MIGRATION

		if (es_replaced_document)
			/* Break the document generation so that we can clean up the documents. */
			ESStoppedGeneratingDocument();
	}
	else if (state)
	{
		if (DocumentManager *docman = GetDocManager())
			docman->UpdateCurrentJSWindow();

		if (GetESScheduler())
		{
			/* If the scheduler has been shut down, it needs to be activated again to
			   accept new threads (which it doesn't while it is being shut down).  Also,
			   timeout and interval threads that were deactivated when the scheduler
			   was shut down are reactivated. */
			if (OpStatus::IsMemoryError(GetESScheduler()->Activate()))
				stat = OpStatus::ERR_NO_MEMORY;

			if (es_generated_document)
			{
				es_generated_document->es_replaced_document = NULL;
				es_generated_document = NULL;
			}
		}

		if (document_state && logdoc && logdoc->IsLoaded())
		{
			OP_DELETE(document_state);
			document_state = NULL;
		}
	}

	if (!state && IsCurrentDoc())
	{
		GetDocManager()->SetRefreshDocument(0, 0);
		UnsetAllCallbacks(TRUE);

		LoadInlineElmHashIterator iterator(inline_hash);
		for (LoadInlineElm* lie = iterator.First(); lie; lie = iterator.Next())
		{
			if (lie->GetLoading())
				SetInlineStopped(lie);
		}

		navigated_in_history = TRUE;

		if (doc && !is_being_deleted)
		{
			doc->SetHoverHTMLElement(NULL);
			doc->SetHoverReferencingHTMLElement(NULL);
		}
	}

	if (state && !mh->HasCallBack(this, MSG_URL_MOVED, 0))
		if (OpStatus::IsMemoryError(stat = mh->SetCallBack(this, MSG_URL_MOVED, 0)))
		{
			UnsetAllCallbacks(TRUE);
			stat = OpStatus::ERR_NO_MEMORY;
		}

	if (state)
	{
		// Tell the UI about our url so that it can update the address bar
		// This should really be done in DocumentManager
		URL tmp_url = GetDocManager()->GetCurrentURL();
		NotifyUrlChanged(tmp_url);
	}

	// Todo: Inline here
	BaseSetAsCurrentDoc(state);

	if (current_form_request)
	{
		current_form_request->Cancel();
		current_form_request = NULL;
	}

#ifdef SVG_SUPPORT
	if (logdoc)
		logdoc->GetSVGWorkplace()->SetAsCurrentDoc(state);

	FramesDocElm* frame = doc_manager->GetFrame();
	/* If this frame wraps a SVG inline ONLOAD might have been sent already and should not be sent again. */
	if (frame && state && GetURL().ContentType() == URL_SVG_CONTENT && frame->GetHtmlElement() &&
	    frame->GetHtmlElement()->GetSpecialBoolAttr(ATTR_INLINE_ONLOAD_SENT, SpecialNs::NS_LOGDOC))
		es_info.inhibit_parent_elm_onload = TRUE;
#endif // SVG_SUPPORT

	packed.load_images = GetWindow()->LoadImages();
	packed.show_images = GetWindow()->ShowImages();

#ifdef FLEXIBLE_IMAGE_POLICY
	packed.show_images = packed.show_images || (url.IsImage() && IsTopDocument());
#endif // FLEXIBLE_IMAGE_POLICY

#ifdef FIT_LARGE_IMAGES_TO_WINDOW_WIDTH
	packed.fit_images  = GetWindow()->FitImages() && !url.IsImage();
#endif // FIT_LARGE_IMAGES_TO_WINDOW_WIDTH

	if (frm_root)
	{
		UnsetAllCallbacks(state);
		stat = frm_root->SetAsCurrentDoc(state, TRUE);
	}
	else if (doc)
	{
		doc->SetAsCurrentDoc(state);

		if (state && packed.load_images)
		{
			BOOL reload_interrupted_images = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::AlwaysReloadInterruptedImages, GetHostName());

			LoadInlineElmHashIterator iterator(inline_hash);
			for (LoadInlineElm* lie = iterator.First(); lie; lie = iterator.Next())
			{
				if ((reload_interrupted_images && lie->GetUrl()->Status(TRUE) == URL_LOADING_ABORTED) ||
					(lie->GetLoaded() && lie->GetUrl()->Status(TRUE) == URL_UNLOADED))
				{
					BOOL was_image = FALSE;

					for (HEListElm *helm = lie->GetFirstHEListElm(); helm; helm = helm->Suc())
						if (helm->IsImageInline() || helm->IsEmbedInline())
						{
							// We need to ensure that images and plugins that have been thrown out of the network cache are reloaded on history navigation.
							// Normally, the inlines are loaded when the layout box is created. In this case we already have a layout box.
							helm->HElm()->MarkExtraDirty(this);
							was_image = TRUE;
						}

					if (was_image)
					{
						RemoveLoadInlineElm(lie);
						OP_DELETE(lie);
					}
				}
			}
		}

		LoadInlineElmHashIterator iterator(inline_hash);
		for (LoadInlineElm* lie = iterator.First(); lie; lie = iterator.Next())
			lie->UpdateDocumentIsCurrent();
	}

	if (!state)
		if (!DeleteAllIFrames())
			PostDelayedActionMessage(1000);

	if (ifrm_root)
		if (OpStatus::IsMemoryError(ifrm_root->SetAsCurrentDoc(state, visible_if_current)))
			stat = OpStatus::ERR_NO_MEMORY;

	if (!state && logdoc)
		logdoc->SetCompleted(TRUE, IsPrintDocument());

	if (state)
	{
		HandleDocumentSizeChange();
		RecalculateScrollbars();
		RecalculateMagicFrameSizes();

#ifdef MEDIA_SUPPORT
		if (OpStatus::IsMemoryError(ResumeAllMedia()))
			stat = OpStatus::ERR_NO_MEMORY;
#endif // MEDIA_SUPPORT

#ifdef _MIME_SUPPORT_
		// We never want focus in the "wrapper" document used by MHTML. If we have it anyway then forward
		// the focus to the first and only child document as soon as it exists, i.e. here.
		if (FramesDocument* parent_doc = GetParentDoc())
			if (g_input_manager->GetKeyboardInputContext() == parent_doc->GetVisualDevice())
			{
				URLCacheType cache_type = static_cast<URLCacheType>(parent_doc->GetURL().GetAttribute(URL::KCacheType));
				if (cache_type == URL_CACHE_MHTML)
					GetVisualDevice()->SetFocus(FOCUS_REASON_ACTIVATE);
			}
#endif // _MIME_SUPPORT_

#ifdef SECMAN_USERCONSENT
		g_secman_instance->OnDocumentResume(this);
#endif // SECMAN_USERCONSENT
	}
	else
	{
		BOOL need_remove_from_cache = CalculateHistoryNavigationMode() == HISTORY_NAV_MODE_COMPATIBLE;

		/* If the document was not loaded completely before stopping it and it was not aborted by the user. We do not want to load it from cache */
		need_remove_from_cache = (!was_document_completely_parsed_before_stopping && !IsAborted()) || need_remove_from_cache;

#ifdef DOCUMENT_EDIT_SUPPORT
		if (GetDocumentEdit())
			need_remove_from_cache = FALSE;
#endif // DOCUMENT_EDIT_SUPPORT

		if (need_remove_from_cache)
			if (DocListElm *dle = GetDocManager()->FindDocListElm(this))
				if (dle->IsScriptGeneratedDocument())
					need_remove_from_cache = FALSE;

		if (need_remove_from_cache)
			RemoveFromCache();

#ifdef MEDIA_SUPPORT
		SuspendAllMedia(FALSE);
#endif // MEDIA_SUPPORT

#ifdef WEBSOCKETS_SUPPORT
		if (dom_environment && dom_environment->CloseAllWebSockets())
			RemoveFromCache();
#endif //WEBSOCKETS_SUPPORT

#ifdef SECMAN_USERCONSENT
		g_secman_instance->OnDocumentSuspend(this);
#endif // SECMAN_USERCONSENT
	}

	if (!state)
		activity_reflow.End();

	SetLocked(FALSE);

#ifdef KEYBOARD_SELECTION_SUPPORT
	if (is_current_doc)
		GetWindow()->UpdateKeyboardSelectionMode(GetKeyboardSelectionMode());
#endif // KEYBOARD_SELECTION_SUPPORT

	return stat;
}

URL FramesDocument::GetRefreshURL(URL_ID id)
{
	if (url.Id(TRUE) == id && logdoc)
	{
		short refresh_sec = logdoc->GetRefreshSeconds();
		if (refresh_sec >= 0)
		{
			URL* refresh_url = logdoc->GetRefreshURL();
			if (refresh_url)
			{
				// a local file refresh_url and with zero refresh_sec may cause a loop
//				if (refresh_url->Type() == URL_FILE && refresh_sec < 2)
//					refresh_sec = 2;

				return (refresh_url->IsEmpty()) ? GetURL() : *refresh_url;
			}
		}
	}
	return URL();
}

URL FramesDocument::GetURLForViewSource()
{
	if (loaded_from_url.IsEmpty())
	{
		URL moved_to = url.GetAttribute(URL::KMovedToURL,TRUE);
		if (!moved_to.IsEmpty())
			return moved_to;
		else
			return url;
	}
	else
		return loaded_from_url;
}

#ifdef IMAGE_TURBO_MODE

void FramesDocument::DecodeRemainingImages()
{
	if (!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DecodeAll, GetHostName()))
		return;

	if (!GetWindow()->IsVisibleOnScreen())
	{
		// This doesn't make sense to do for hidden tabs.
		// We throw away images in hidden tabs after a while anyway!
		return;
	}

	// Run through all images and run PreDecode on them. The imagecode itself will judge if the image
	// should be predecoded or not. (If the cache is full and it can't free any invisible images, it won't
	// predecode anymore images)

	HTML_Element* helm = GetDocRoot();
	while(helm)
	{
		if (helm->IsMatchingType(HE_IMG, NS_HTML) || helm->IsMatchingType(HE_OBJECT, NS_HTML))
		{
			HEListElm* hle = helm->GetHEListElm(FALSE);

			if (hle && !hle->GetImageVisible())
			{
				Image image = hle->GetImage();
				if (image.CountListeners() == 0) // FIX: Make a IsVisible function.
				{
					image.PreDecode(hle);
				}
			}
		}
		helm = (HTML_Element*) helm->Next();
	}
}

#endif // IMAGE_TURBO_MODE

void FramesDocument::UpdateAnimatedRect(HEListElm* hle)
{
	if (!hle->IsImageInline() || !hle->GetImageVisible() || hle->HasPendingAnimationInvalidation()) // No reason to update if we did not paint the last time we tried.
		return;

	Image image = hle->GetImage();
	OpRect frame_rect = image.GetCurrentFrameRect(hle);
	OpRect box_rect(0, 0, hle->GetImageWidth(), hle->GetImageHeight());

	if (image.Width() == 0 || image.Height() == 0)
		return;

	OpRect update_rect;
	if (hle->IsBgImageInline())
	{
		update_rect = box_rect;
	}
	else
	{
		// Calculate the stretched destination rectangle.
		update_rect.x = frame_rect.x * box_rect.width / image.Width();
		update_rect.y = frame_rect.y * box_rect.height / image.Height();
		update_rect.width = ((frame_rect.x + frame_rect.width) * box_rect.width + image.Width()) / image.Width() - update_rect.x;
		update_rect.height = ((frame_rect.y + frame_rect.height) * box_rect.height + image.Height()) / image.Height() - update_rect.y;
	}

	hle->GetImagePos().Apply(update_rect);

#ifdef SVG_SUPPORT
	HTML_Element* helm = hle->HElm();
	if (helm->GetNsType() == NS_SVG)
	{
		g_svg_manager->RepaintElement(this, helm);
	}
	else
#endif // SVG_SUPPORT
	{
		GetVisualDevice()->Update(update_rect.x, update_rect.y, update_rect.width, update_rect.height);
		hle->SetHasPendingAnimationInvalidation(TRUE);
	}
}

BOOL FramesDocument::ImagesDecoded()
{
	if (!frm_root)
	{
		LoadInlineElmHashIterator iterator(inline_hash);
		for (LoadInlineElm* lie = iterator.First(); lie; lie = iterator.Next())
		{
			for (HEListElm *hle = lie->GetFirstHEListElm(); hle; hle = hle->Suc())
			{
				// Only care about visible images.
				if (hle->IsImageInline() && hle->GetImageVisible())
				{
					Image image = hle->GetImage();
					// Note that animated images will only be decoded when the
					// last frame is decoded and here we're happy if one frame is
					// decoded so that we have something to paint. image.IsAnimated()
					// will not return TRUE until the first frame is decoded so that
					// is a good condition.
					if (!image.ImageDecoded() && !image.IsFailed() && !image.IsAnimated())
						return FALSE;
				}
			}
		}
	}

	FramesDocElm *fde = frm_root ? frm_root : ifrm_root;

	while (fde)
	{
		if (FramesDocument *doc = fde->GetCurrentDoc())
			if (!doc->ImagesDecoded())
				return FALSE;
		fde = (FramesDocElm *) fde->Next();
	}

	return TRUE;
}

#ifndef MOUSELESS
FramesDocument::FrameSeperatorType FramesDocument::IsFrameSeparator(int x, int y)
{
	if (frm_root)
	{
		FramesDocElm* fde = frm_root->IsSeparator(x, y);

		if (!fde)
			return NO_SEPARATOR;
		else
			if (fde->Parent()->IsRow())
				return HORIZONTAL_SEPARATOR;
			else
				return VERTICAL_SEPARATOR;
	}
	else
		return NO_SEPARATOR;
}

void FramesDocument::StartMoveFrameSeparator(int x, int y)
{
	if (frm_root)
	{
		drag_frm = frm_root->IsSeparator(x, y);

		if (drag_frm)
			drag_frm->StartMoveSeparator(x, y);
	}
}

void FramesDocument::MoveFrameSeparator(int x, int y)
{
	if (frm_root && drag_frm)
	{
		drag_frm->MoveSeparator(x, y);

		if (GetVisualDevice())
			GetVisualDevice()->UpdateAll(); // Repaint the borders
	}
}

void FramesDocument::EndMoveFrameSeparator(int x, int y)
{
	if (frm_root && drag_frm)
	{
		drag_frm->EndMoveSeparator(x, y);
	}

	drag_frm = 0;
}
#endif // !MOUSELESS

OP_STATUS FramesDocument::SetRelativePos(const uni_char* rel_name, const uni_char* alternative_rel_name, BOOL scroll /* = TRUE */)
{
	if (doc)
	{
#ifdef _WML_SUPPORT_
		if (DocumentManager *doc_man = GetDocManager())
		{
			if (WML_Context *wc = doc_man->WMLGetContext())
			{
				if (!wc->IsParsing() && wc->IsSet(WS_GO))
				{
					RETURN_IF_MEMORY_ERROR(wc->PreParse());
					logdoc->WMLReEvaluate();
					RETURN_IF_MEMORY_ERROR(wc->PostParse());
				}
			}
		}
#endif // _WML_SUPPORT_

		return doc->SetRelativePos(rel_name, alternative_rel_name, scroll);
	}

	return(OpStatus::ERR);
}

FramesDocument* FramesDocument::GetSubDocWithId(int id)
{
	DocumentTreeIterator iter(this);
	iter.SetIncludeThis();
	while (iter.Next())
	{
		if (iter.GetFramesDocument()->GetSubWinId() == id)
			return iter.GetFramesDocument();
	}
	return NULL;
}

int FramesDocument::GetNamedSubWinId(const uni_char* win_name)
{
	if (!win_name || *win_name == 0)
		return -1;

	DocumentTreeIterator iter(this);
	iter.SetIncludeEmpty();
	while (iter.Next())
	{
		FramesDocElm* fdelm = iter.GetFramesDocElm();
		OP_ASSERT(fdelm);

		const uni_char* this_frame_name = fdelm->GetName();
		if (!this_frame_name || !*this_frame_name)
			this_frame_name = fdelm->GetFrameId();
		if (this_frame_name && uni_str_eq(this_frame_name, win_name) && !fdelm->IsFrameset())
			return fdelm->GetSubWinId();
	}
	return -1;
}

void FramesDocument::FramesDocElmDeleted(FramesDocElm* fde)
{
	if (active_frm_doc == fde)
		active_frm_doc = NULL;

	if (loading_frame == fde)
		loading_frame = NULL;
}

FramesDocument* FramesDocument::GetActiveSubDoc()
{
	if (GetFramesStacked())
		return NULL;
	else
		return active_frm_doc ? active_frm_doc->GetDocManager()->GetCurrentDoc() : 0;
}

void FramesDocument::SetActiveFrame(FramesDocElm* frm, BOOL notify_focus_change /* = TRUE */, FOCUS_REASON reason /* = FOCUS_REASON_ACTIVATE */)
{
	if (frm && frm->IsBeingDeleted())
		return;

	FramesDocument* doc = NULL;

	if (GetFramesStacked())
	{
		active_frm_doc = frm;
		doc = active_frm_doc ? active_frm_doc->GetCurrentDoc() : GetTopDocument();
	}
	else if (active_frm_doc != frm && !GetHandheldEnabled())
	{
		BOOL show_active_border = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::ShowActiveFrame, GetHostName());

		if (active_frm_doc && show_active_border)
			if (FramesDocument* active_doc = active_frm_doc->GetCurrentDoc())
				active_doc->GetVisualDevice()->UpdateAll();

		active_frm_doc = frm;

		if (active_frm_doc)
		{
			doc = active_frm_doc->GetCurrentDoc();
			if (doc && show_active_border)
				doc->GetVisualDevice()->UpdateAll();
		}
		else
			doc = GetTopDocument();
	}

	if (doc && notify_focus_change)
	{
		OnFocusChange(doc);
		doc->SetFocus(reason);
	}
}

void FramesDocument::SetNextActiveFrame(BOOL back)
{
	FramesDocElm *candidate = NULL;

	if (active_frm_doc)
	{
		candidate = back ? active_frm_doc->PrevActive() : active_frm_doc->NextActive();

		if (candidate && candidate->GetCurrentDoc() && candidate->GetCurrentDoc()->GetFrmDocRoot())
		{
			FramesDocElm *child_frm = back
					? candidate->PrevActive()
					: candidate->GetCurrentDoc()->GetFrmDocRoot()->NextActive();
			if (child_frm)
				candidate = child_frm;
		}
	}

	if (!candidate)
	{
		if (ifrm_root)
			candidate = back ? ifrm_root->PrevActive() : ifrm_root->NextActive();

		if (!candidate && frm_root)
			candidate = back ? (FramesDocElm*) frm_root->LastLeafActive(): (FramesDocElm*) frm_root->NextActive();
	}

	SetActiveFrame(candidate);
}

void FramesDocument::GetImageLoadingInfo(ImageLoadingInfo& info)
{
	DocumentTreeIterator iter(this);
	iter.SetIncludeThis();
	while (iter.Next())
		info.AddTo(iter.GetFramesDocument()->image_info);
}

void FramesDocument::GetDocLoadingInfo(DocLoadingInfo& info)
{
	DocumentTreeIterator iter(this);
	iter.SetIncludeThis();
	while (iter.Next())
	{
		FramesDocument* frm_doc = iter.GetFramesDocument();
		if (!frm_doc->url.IsImage())
		{
			OpFileLength total_size = frm_doc->url.GetContentSize();
			OpFileLength loaded_size = frm_doc->url.GetContentLoaded();

#ifdef WEB_TURBO_MODE
			if (doc_manager->GetWindow()->GetTurboMode())
			{
				UINT32 turbo_transferred_bytes, turbo_original_transferred_bytes;
				frm_doc->url.GetAttribute(URL::KTurboTransferredBytes, &turbo_transferred_bytes);
				frm_doc->url.GetAttribute(URL::KTurboOriginalTransferredBytes, &turbo_original_transferred_bytes);
				if (turbo_original_transferred_bytes > 0)
				{
					info.turbo_transferred_bytes += turbo_transferred_bytes;
					info.turbo_original_transferred_bytes += turbo_original_transferred_bytes;
				}
			}
#endif // WEB_TURBO_MODE

			if (loaded_size > total_size)
				total_size = loaded_size;

			info.total_size += total_size;
			info.loaded_size += loaded_size;
		}
		info.images.AddTo(frm_doc->image_info);

		OpFileLength total_size = frm_doc->image_info.total_size;
		OpFileLength loaded_size = frm_doc->image_info.loaded_size;

		if (loaded_size > total_size)
			total_size = loaded_size;

		info.total_size += total_size;
		info.loaded_size += loaded_size;
	}
}

OP_STATUS FramesDocument::CheckRefresh()
{
	if (logdoc)
	{
		BOOL is_redirect = logdoc->GetRefreshURL() && logdoc->GetRefreshURL()->Id(TRUE) != loaded_from_url.Id(FALSE);

		if (is_redirect)
		{
			/* If the refresh is a redirect, and we've navigated to
			   this document in history, don't perform the redirect. */
			if (navigated_in_history)
				return OpStatus::OK;
		}

		if (!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableClientRefresh, GetServerNameFromURL(GetURL())) ||
			(!is_redirect && document_is_reloaded && !g_pcnet->GetIntegerPref(PrefsCollectionNetwork::ClientRefreshToSame)))
				return OpStatus::OK;

		int refresh_sec = logdoc->GetRefreshSeconds();
		if (refresh_sec >= 0)
		{
			URL* refresh_url = logdoc->GetRefreshURL();
			if (refresh_url)
			{
#ifdef URL_FILTER
				ServerName *doc_sn = refresh_url->GetServerName();
				URLFilterDOMListenerOverride lsn_over(dom_environment, NULL);
				HTMLLoadContext load_ctx(RESOURCE_REFRESH, doc_sn, &lsn_over, GetWindow()->GetType() == WIN_TYPE_GADGET);
				BOOL allowed = FALSE;

				// Checks if content_filter rules allow this URL
				RETURN_IF_ERROR(g_urlfilter->CheckURL(refresh_url, allowed, NULL,  &load_ctx));

				if (!allowed)
					return OpStatus::OK;
#endif // URL_FILTER

				// a local file refresh_url and with zero refresh_sec may cause a loop
				if (refresh_url->Type() == URL_FILE && refresh_sec < 2)
					refresh_sec = 2;

#ifdef REFRESH_DELAY_OVERRIDE
				ReviewRefreshDelayCallbackImpl *cb = OP_NEW(ReviewRefreshDelayCallbackImpl, (this));

				if (!cb)
					return OpStatus::ERR_NO_MEMORY;

				cb->Into(&active_review_refresh_delay_callbacks);
				GetWindow()->GetWindowCommander()->GetDocumentListener()->OnReviewRefreshDelay(GetWindow()->GetWindowCommander(), cb);

				// The refresh will be set by the callback
				return OpStatus::OK;
#else
				GetDocManager()->SetRefreshDocument(url.Id(TRUE), (unsigned long)refresh_sec*1000);
#endif // REFRESH_DELAY_OVERRIDE
			}
		}
	}

	return OpStatus::OK;
}

URL FramesDocument::GetBGImageURL()
{
	if (active_frm_doc)
	{
		FramesDocument* active_doc = active_frm_doc->GetCurrentDoc();
		if (active_doc)
			return active_frm_doc->GetCurrentDoc()->GetBGImageURL();
	}
	else if (doc)
	{
		return doc->HTML_Document::GetBGImageURL();
	}

	return URL();
}

void FramesDocument::UpdateLinkVisited()
{
	DocumentTreeIterator iter(this);
	iter.SetIncludeThis();
	while (iter.Next())
	{
		FramesDocument* frm_doc = iter.GetFramesDocument();

		if (frm_doc->logdoc && frm_doc->logdoc->GetRoot() &&
			!frm_doc->IsWaitingForStyles())
		{
			frm_doc->logdoc->GetRoot()->UpdateLinkVisited(frm_doc);
		}
	}
}

void FramesDocument::FindTarget(const uni_char* &win_name, int &sub_win_id)
{
	LocalFindTarget(win_name, sub_win_id);
}

void FramesDocument::SetBlink(BOOL val)
{
	if (!has_blink && val)
	{
		// turn blinking on in win_man if this is the first document with blink
		g_windowManager->AddDocumentWithBlink();
	}
	else if (has_blink && !val)
	{
		// turns it off in win_man if this is the last document with blink
		g_windowManager->RemoveDocumentWithBlink();
	}
	has_blink = val;
}

void FramesDocument::Blink()
{
	HTML_Element* doc_root = GetDocRoot();
	if (doc_root && !reflowing) // Must never traverse when reflowing
	{
		VisualDevice* vis_dev = GetVisualDevice();
		RECT area = {0, 0, vis_dev->GetRenderingViewWidth(), vis_dev->GetRenderingViewHeight()};
		BlinkObject blink_object(this, area);

		blink_object.Traverse(doc_root);
	}

	FramesDocElm *fde = frm_root ? frm_root : ifrm_root;

	while (fde)
	{
		if (FramesDocument* doc = fde->GetCurrentDoc())
			doc->Blink();
		fde = (FramesDocElm *) fde->Next();
	}
}

FramesDocument*	FramesDocument::GetTopFramesDoc()
{
	FramesDocument* pdoc = this;

	while (pdoc)
	{
		DocumentManager* doc_man = pdoc->GetDocManager();

		if ((!doc_man->GetFrame() || !doc_man->GetFrame()->IsInlineFrame()) && doc_man->GetParentDoc())
			pdoc = doc_man->GetParentDoc();
		else
			break;
	}

	return pdoc;
}

DocumentManager* FramesDocument::GetDocManagerById(int id)
{
	DocumentTreeIterator iter(this);
	iter.SetIncludeEmpty(); // Probably not needed but we used to check those
	while (iter.Next())
	{
		// More complicated than needed I guess, but directly converted
		// from the recursive implementation
		FramesDocElm* fdelm = iter.GetFramesDocElm();
		if (fdelm->GetSubWinId() == id)
			return fdelm->GetDocManager();
		if (FramesDocument* frm_doc = iter.GetFramesDocument())
		{
			if (frm_doc->GetSubWinId() == id)
				return frm_doc->GetDocManager();
		}
	}
	return NULL;
}

DocumentManager* FramesDocument::GetActiveDocManager()
{
	if (active_frm_doc)
		return active_frm_doc->GetDocManager();
	else
		return NULL;
}

OP_STATUS FramesDocument::CheckInternal(Head* existing_frames)
{
	if (!frm_root && !doc)
	{
		int html_doc_type = HTML_DOC_PLAIN;
		HTML_Element* frameset = NULL;

		if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::FramesEnabled, GetHostName()) && !url.IsImage())
		{
			html_doc_type = logdoc->GetHtmlDocType();

			if (html_doc_type == HTML_DOC_FRAMES)
			{
				frameset = logdoc->GetFirstHE(HE_FRAMESET);

				if (!frameset || !logdoc->GetHLDocProfile()->IsElmLoaded(frameset))
					frameset = NULL;
			}
		}

		if (frameset && !ifrm_root)
		{
			CheckFrameStacking(frames_policy == FRAMES_POLICY_FRAME_STACKING);
			CheckSmartFrames(frames_policy == FRAMES_POLICY_SMART_FRAMES);

			if (GetSubWinId() == -1)
				// refresh document window

				GetVisualDevice()->UpdateAll();

			BOOL frame_in_frame = (GetDocManager()->GetFrame() && !GetDocManager()->GetFrame()->IsInlineFrame());
			VisualDevice* main_vd = GetVisualDevice();

			if (frame_in_frame)
			{
				// frame in frame
				main_vd = GetDocManager()->GetWindow()->VisualDev();
			}

			if (existing_frames)
				frm_root = FindFramesDocElm(existing_frames, frameset);

			int view_width = GetLayoutViewWidth();

			if (frm_root)
				frm_root->Reset(FRAMESET_ABSOLUTE_SIZED, view_width, NULL);
			else
			{
				frm_root = OP_NEW(FramesDocElm, (0,
											0, 0, 0, 0,
											this,
											frameset,
											main_vd,
											FRAMESET_ABSOLUTE_SIZED,
											view_width,
											FALSE,
											NULL));

				if (!frm_root)
					return OpStatus::ERR_NO_MEMORY;

				if (OpStatus::IsError(frm_root->Init(frameset, main_vd, NULL)))
				{
					OP_DELETE(frm_root);
					frm_root = NULL;
					return OpStatus::ERR_NO_MEMORY;
				}
			}

			SetFrmRootSize();

			FramesDocument* top_frm_doc = this;

			if (GetSubWinId() >= 0)
			{
				// New frames document inside a frame

				DocumentManager* top_doc_man = GetDocManager()->GetWindow()->DocManager();

				if (top_doc_man)
				{
					FramesDocument* top_doc = top_doc_man->GetCurrentDoc();

					if (top_doc)
						top_frm_doc = top_doc;
				}
			}

			RETURN_IF_MEMORY_ERROR(frm_root->BuildTree(top_frm_doc, existing_frames));

			RETURN_IF_ERROR(frm_root->FormatFrames());

			for (FramesDocElm* fde = frm_root->FirstLeaf(); fde; fde = fde->NextLeaf())
				fde->UpdateGeometry();

			if (existing_frames == NULL)
				RETURN_IF_ERROR(frm_root->LoadFrames());

			RETURN_IF_ERROR(frm_root->ShowFrames());

			RETURN_IF_MEMORY_ERROR(CheckRefresh());
		}
		else
			if (html_doc_type == HTML_DOC_PLAIN)
			{
				DocumentManager* doc_man = GetDocManager();
				FramesDocElm* frame = doc_man->GetFrame();

				if (frame && !frame->IsInlineFrame())
				{
					frame->Show(); // make sure that frame window is visible
				}

				if( doc )
				{
					doc->Free();
					OP_DELETE(doc);
				}
				doc = OP_NEW(HTML_Document, (this, doc_man));

				if (!doc) // stighal: TODO what to do with return values
					return OpStatus::ERR_NO_MEMORY;

				doc->SetAsCurrentDoc(TRUE);
			}
	}

	return OpStatus::OK;
}

#ifdef DELAYED_SCRIPT_EXECUTION
void
FramesDocument::CheckInternalReset()
{
	if (doc)
	{
		doc->Free();
		OP_DELETE(doc);
		doc = NULL;
	}
}
#endif // DELAYED_SCRIPT_EXECUTION

OP_STATUS
FramesDocument::InitParsing()
{
	Window *win = GetWindow();

	OP_DELETE(url_data_desc);

	BOOL set_loaded_from_url = TRUE;

#ifdef WEBFEEDS_DISPLAY_SUPPORT
	if (is_inline_feed)
	{
		OpFeed* feed = g_webfeeds_api->GetFeedByUrl(url);
		if (!feed)
			return OpStatus::ERR_NULL_POINTER;

		// Use a generated Opera page instead
		contains_only_opera_internal_scripts = TRUE;
		is_generated_doc = TRUE;

		URL generated_url = urlManager->GetNewOperaURL();
		RETURN_IF_ERROR(feed->WriteFeed(generated_url));
		url_data_desc = generated_url.GetDescriptor(GetMessageHandler(), FALSE, FALSE, TRUE, win);
	}
	else
#endif // WEBFEEDS_DISPLAY_SUPPORT
#ifdef HAS_ATVEF_SUPPORT
	if (!IsWrapperDoc() && url.Type() != URL_TV)
#else // HAS_ATVEF_SUPPORT
	if (!IsWrapperDoc())
#endif // HAS_ATVEF_SUPPORT
	{
#ifdef XMLUTILS_PARSE_RAW_DATA
		if (url.ContentType() == URL_XML_CONTENT || url.ContentType() == URL_WML_CONTENT)
			url_data_desc = url.GetDescriptor(GetMessageHandler(), FALSE, TRUE, TRUE, win);
		else
#endif // XMLUTILS_PARSE_RAW_DATA
		{
			unsigned short int parent_charset = 0;
			HLDocProfile *parent_hld_profile = GetParentDoc() ? GetParentDoc()->GetHLDocProfile() : NULL;
			if (parent_hld_profile)
			{
				/* DSK-62051: If we are a frame and we do not declare an
				 * encoding, we should inherit the encoding of our parent
				 * document. Try to find the encoding used by the parent
				 * document (us) as the default. This will also allow
				 * encodings to be inherited for nested frames (DSK-157855).
				 *
				 * The IFRAME and FRAMESET tags does not have CHARSET
				 * attributes, so we do not need to look for that here.
				 */
				const char *parent_charset_string = parent_hld_profile->GetCharacterSet();
				if (OpStatus::IsSuccess(g_charsetManager->GetCharsetID(parent_charset_string, &parent_charset)))
				{
					// Perform security check
					OpSecurityContext
						source(GetParentDoc()->GetURL(), parent_charset_string),
						target(GetURL(), parent_charset_string);
					BOOL allowed = FALSE;
					if (OpStatus::IsSuccess(OpSecurityManager::CheckSecurity(
					    OpSecurityManager::DOC_SET_PREFERRED_CHARSET, source, target, allowed))
						&& !allowed)
					{
						parent_charset = 0;
					}
				}
			}

			g_charsetManager->IncrementCharsetIDReference(parent_charset);
			url_data_desc = url.GetDescriptor(GetMessageHandler(), FALSE, FALSE, TRUE, win, URL_UNDETERMINED_CONTENT, 0, FALSE, parent_charset);
			g_charsetManager->DecrementCharsetIDReference(parent_charset);
		}

		if (!url_data_desc)
		{
			// url.GetDescriptor() may return NULL if the URL has been sucessfully loaded, but has no data.
			// Failing to set the loaded_from_url properly can lead to security issues (CORE-32707).
			set_loaded_from_url = url.Status(FALSE) != URL_LOADING;
		}
	}

	OP_ASSERT(url.GetAttribute(URL::KMovedToURL, TRUE).IsEmpty());
	if (set_loaded_from_url)
	{
		loaded_from_url = url;
		origin->is_generated_by_opera = !IsAboutBlankURL(url) && url.GetAttribute(URL::KIsGeneratedByOpera);
	}
	win->SetIsImplicitSuppressWindow(!!url.GetAttribute(URL::KSuppressScriptAndEmbeds, TRUE));

	win->SetIsScriptableWindow(win->IsScriptableWindow() && url.GetAttribute(URL::KSuppressScriptAndEmbeds, TRUE) != MIME_EMail_ScriptEmbed_Restrictions);

#if defined(HISTORY_SUPPORT) || defined(VISITED_PAGES_SEARCH)
	if (GetTopDocument() == this
	    && (url.Type() == URL_HTTP || url.Type() == URL_HTTPS)
	    && !url.GetAttribute(URL::KIsGeneratedByOpera)
#ifdef HISTORY_SUPPORT
	    && win->IsGlobalHistoryEnabled()
	    && g_url_api->GlobalHistoryPermitted(url)
#endif
	    && !GetWindow()->GetPrivacyMode()
		)
	{
		// Visited Pages Search and History should always add the same urls in the same format.
		OpString8 clean_url;
		OpString clean_url16;
		RETURN_IF_ERROR(url.GetAttribute(URL::KUniName_With_Fragment_Username, 0, clean_url16, URL::KFollowRedirect));
		RETURN_IF_ERROR(clean_url.SetUTF8FromUTF16(clean_url16)); // Must be set indirectly from unicode url for IDNA decoding

#ifdef HISTORY_SUPPORT
		if (g_globalHistory)
		{
			RETURN_IF_ERROR(g_globalHistory->Add(clean_url, clean_url, g_timecache->CurrentTime()));
		}
#endif // HISTORY_SUPPORT

#ifdef VISITED_PAGES_SEARCH
		if (!m_visited_search && g_visited_search->IsOpen())
		{
			m_visited_search = g_visited_search->CreateRecord(clean_url.CStr());
			if (!m_visited_search)
					return OpStatus::ERR_NO_MEMORY;

			m_index_all_text = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::PageContentSearch);
			FramesDocument *iter_doc = this;
			while (m_index_all_text && iter_doc)
			{
				if (iter_doc->GetURL().GetAttribute(URL::KCachePolicy_NoStore) || iter_doc->GetURL().Type() == URL_HTTPS)
					m_index_all_text = FALSE;
				else
					iter_doc = iter_doc->GetParentDoc();
			}
		}
#endif // VISITED_PAGES_SEARCH
	}

#endif // HISTORY_SUPPORT || VISITED_PAGES_SEARCH

	return OpStatus::OK;
}

OP_STATUS
FramesDocument::LoadData(BOOL format, BOOL aborted)
{
	OP_PROBE5(OP_PROBE_FRAMESDOCUMENT_LOADDATA);

#ifdef _DEBUG
	DebugTimer timer(DebugTimer::PARSING);
#endif

	OP_STATUS stat = OpStatus::OK;

	if (!logdoc)
		return(OpStatus::OK);
	else
	{
#ifdef SPECULATIVE_PARSER
		if (logdoc->IsParsingSpeculatively())
			return LoadDataSpeculatively();
#endif // SPECULATIVE_PARSER
		if (!url_data_desc && !logdoc->IsParsed())
		{
			if (!packed.data_desc_done)
				RETURN_IF_MEMORY_ERROR(InitParsing());
		}
		else
			if (!url_data_desc)
				// i.e. logdoc->IsParsed() == TRUE
#ifdef DELAYED_SCRIPT_EXECUTION
				if (!GetHLDocProfile()->ESIsExecutingDelayedScript())
#endif // DELAYED_SCRIPT_EXECUTION
					return HandleLogdocParsingComplete();
	}

	OpStatus::Ignore(stat); // To silence DEBUGGING_OP_STATUS when stat is not used...
	const uni_char* data_buf = NULL;
	unsigned buf_len = 0;
	BOOL more = FALSE;

	ES_LoadManager* load_manager = logdoc->GetHLDocProfile()->GetESLoadManager();
	BOOL executing_script = load_manager->HasScripts();

#ifdef SCRIPTS_IN_XML_HACK
	if (logdoc->GetHLDocProfile()->IsXml())
		// Scripts aren't blocking in XML anyway, so we don't care.
		executing_script = FALSE;
#endif // SCRIPTS_IN_XML_HACK

	BOOL data_from_data_desc = FALSE;

	if (IsWrapperDoc()
#ifdef HAS_ATVEF_SUPPORT
	    || url.Type() == URL_TV
#endif // HAS_ATVEF_SUPPORT
		)
	{
		if (wrapper_doc_source.IsEmpty())
		{
			// No meaning trying to replace document.written data with a wrapper document. It will
			// not work because LogicalDocument will prefer the document.written data anyway, and we
			// will not be able to load it as an inline resource. Just add a dummy character to make
			// wrapper_doc_source non-empty.
			if (load_manager->GetScriptGeneratingDoc())
			{
				RETURN_IF_ERROR(wrapper_doc_source.Set(" "));
				wrapper_doc_source_read_offset = 1;
			}
			else
			{
				RETURN_IF_ERROR(MakeWrapperDoc());
				wrapper_doc_source_read_offset = 0;
				is_generated_doc = TRUE;
			}
		}

		data_buf = wrapper_doc_source.CStr() + wrapper_doc_source_read_offset;
		buf_len = wrapper_doc_source.Length() - wrapper_doc_source_read_offset;
	}
	else if (url_data_desc)
	{
									__DO_START_TIMING(TIMING_CACHE_RETRIEVAL);
									__TIMING_CODE(unsigned long plen = url_data_desc->GetBufSize());
		if (executing_script)
			// No point in trying to fetch more data when javascript is executing
			more = TRUE;
		else
			TRAP_AND_RETURN_VALUE_IF_ERROR(stat,url_data_desc->RetrieveDataL(more),stat);

		data_buf = (uni_char*)url_data_desc->GetBuffer();
		buf_len = UNICODE_DOWNSIZE(url_data_desc->GetBufSize());
									__DO_ADD_TIMING_PROCESSED(TIMING_CACHE_RETRIEVAL,buf_len -plen);
									__DO_STOP_TIMING(TIMING_CACHE_RETRIEVAL);

		data_from_data_desc = TRUE;

		if (!logdoc->GetHLDocProfile()->GetCharacterSet())
			RETURN_IF_ERROR(logdoc->GetHLDocProfile()->SetCharacterSet(url_data_desc->GetCharacterSet()));
	}
	else if (url.Status(TRUE) == URL_LOADING)
		more = TRUE;
#ifdef DELAYED_SCRIPT_EXECUTION
	// If we're done parsing ahead and still have delayed scripts, then we're just waiting for
	// those scripts to load/run.  Nothing to do here.
	else if (logdoc->GetHLDocProfile()->ESHasDelayedScripts() && !logdoc->GetHLDocProfile()->ESIsParsingAhead())
		return OpStatus::OK;
#endif // DELAYED_SCRIPT_EXECUTION

	if (!data_buf && (!more || executing_script))  // data_buf doesn't need to contain anything if we're parsing the script written buffer in any case
	{
		data_buf = UNI_L("");
	}

	if (data_buf && (buf_len > 0 ||
					 !more ||
					 executing_script ||
					 logdoc->IsContinuingParsing() ||
					 aborted))
	{
		URL base_url = url;

		if (base_url.IsEmpty() ||
			base_url.Type() == URL_JAVASCRIPT)
			base_url = GetRefURL().url;

		BOOL need_to_grow = FALSE;
		unsigned int rest;
		BOOL expect_more = wrapper_doc_source.IsEmpty() && (more || url.Status(TRUE) == URL_LOADING && !is_multipart_reload);

		__DO_START_TIMING(TIMING_DOC_LOAD);

		current_buffer = data_buf;

		if (!IsAboutBlankURL(url) && url.GetAttribute(URL::KIsGeneratedByOpera))
			origin->is_generated_by_opera = TRUE; // Some URL:s (e.g. multipart MIME) get generated by Opera status while loading.

#ifdef XMLUTILS_PARSE_RAW_DATA
		BOOL is_xml_content = url.ContentType() == URL_XML_CONTENT || url.ContentType() == URL_WML_CONTENT;
		if (is_xml_content)
			rest = logdoc->Load(base_url, (uni_char*)data_buf, UNICODE_SIZE(buf_len), need_to_grow, expect_more, aborted);
		else
#endif // XMLUTILS_PARSE_RAW_DATA
			rest = logdoc->Load(base_url, (uni_char*)data_buf, buf_len, need_to_grow, expect_more, aborted);

		current_buffer = NULL;

		__DO_ADD_TIMING_PROCESSED(TIMING_DOC_LOAD,buf_len -rest);
		__DO_STOP_TIMING(TIMING_DOC_LOAD);

		stream_position_base += buf_len - rest;

		if (logdoc->GetHLDocProfile()->GetIsOutOfMemory())
		{
			logdoc->GetHLDocProfile()->SetIsOutOfMemory(FALSE);
			return OpStatus::ERR_NO_MEMORY;
		}

		if (data_from_data_desc && url_data_desc)
		{
#ifdef XMLUTILS_PARSE_RAW_DATA
			// using the raw datastream so we don't convert it like for UNICODE
			if (is_xml_content)
				url_data_desc->ConsumeData(UNICODE_SIZE(buf_len) - rest);
			else
#endif // XMLUTILS_PARSE_RAW_DATA
			{
				rest = UNICODE_SIZE(rest); // this is in chars, so must be adjusted...
				url_data_desc->ConsumeData(UNICODE_SIZE(buf_len) - rest);
			}
		}

		if (need_to_grow && url_data_desc && url_data_desc->Grow() == 0)
			return OpStatus::ERR_NO_MEMORY;

		if (!wrapper_doc_source.IsEmpty())
			wrapper_doc_source_read_offset += buf_len - rest;

		if (GetDocManager()->GetRestartParsing())
			GetMessageHandler()->PostDelayedMessage(MSG_URL_DATA_LOADED, (MH_PARAM_1) url.Id(TRUE), 0, 50);

		if (!aborted && !more)
		{
			if (!load_manager->IsFinished())
				more = TRUE;

			if (rest != 0)
				more = TRUE;
		}
	}

	if (!more)
	{
		if (OpStatus::IsMemoryError(CreateESEnvironment(FALSE, TRUE)))
			stat = OpStatus::ERR_NO_MEMORY;

#ifdef DELAYED_SCRIPT_EXECUTION
		if (OpStatus::IsMemoryError(GetHLDocProfile()->ESParseAheadFinished()))
			stat = OpStatus::ERR_NO_MEMORY;
#endif // DELAYED_SCRIPT_EXECUTION

#ifdef RSS_SUPPORT
		if (logdoc->IsRss())
		{
#if defined WEBFEEDS_DISPLAY_SUPPORT && defined WEBFEEDS_AUTO_DISPLAY_SUPPORT
			feed_listener_added = TRUE;
			g_webfeeds_api->LoadFeed(url, this);
#else
			OpString url_name;
			if (OpStatus::IsMemoryError(url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, url_name)))
				stat = OpStatus::ERR_NO_MEMORY;
			else
				GetWindow()->GetWindowCommander()->GetDocumentListener()->OnRss(GetWindow()->GetWindowCommander(), url_name.CStr());
#endif // WEBFEEDS_DISPLAY_SUPPORT && WEBFEEDS_AUTO_DISPLAY_SUPPORT
		}
#endif // RSS_SUPPORT

		if (logdoc->IsParsed()
#ifdef DELAYED_SCRIPT_EXECUTION
			&& !GetHLDocProfile()->ESIsExecutingDelayedScript()
#endif // DELAYED_SCRIPT_EXECUTION
			)
		{
			HandleLogdocParsingComplete();
		}

		if (url.Status(TRUE) != URL_LOADING || is_multipart_reload)
		{
			OP_DELETE(url_data_desc);
			url_data_desc = NULL;
			packed.data_desc_done = TRUE;

#ifdef SPECULATIVE_PARSER
			OP_DELETE(speculative_data_desc);
			speculative_data_desc = NULL;
#endif // SPECULATIVE_PARSER
		}
	}

	if (OpStatus::IsMemoryError(CheckFinishDocument()))
		stat = OpStatus::ERR_NO_MEMORY;

	return stat;
}

#ifdef SPECULATIVE_PARSER
OP_STATUS
FramesDocument::LoadDataSpeculatively()
{
	if (!speculative_data_desc)
		return OpStatus::OK; /* Triggered by stray message. */

	BOOL more;
	RETURN_IF_LEAVE(speculative_data_desc->RetrieveDataL(more));

	const uni_char *data_buf = reinterpret_cast<const uni_char *>(speculative_data_desc->GetBuffer());
	unsigned buf_len = UNICODE_DOWNSIZE(speculative_data_desc->GetBufSize());
	unsigned rest = buf_len;

	RETURN_IF_MEMORY_ERROR(logdoc->LoadSpeculatively(url, data_buf, buf_len, rest, more));

	speculative_data_desc->ConsumeData(UNICODE_SIZE(buf_len) - UNICODE_SIZE(rest));

	return OpStatus::OK;
}

OP_STATUS
FramesDocument::CreateSpeculativeDataDescriptor(unsigned new_stream_position)
{
	if (speculative_data_desc)
		return OpStatus::OK;

	/* Get the encoding of the main document to make sure the speculative
	 * parser continues to parse with the same encoding. */
	unsigned short int override_charset_id = 0;
	if (url_data_desc)
		override_charset_id = url_data_desc->GetCharsetID();
	else if (GetHLDocProfile())
		OpStatus::Ignore(g_charsetManager->GetCharsetID(GetHLDocProfile()->GetCharacterSet(), &override_charset_id));

	g_charsetManager->IncrementCharsetIDReference(override_charset_id);
	speculative_data_desc = url.GetDescriptor(GetMessageHandler(), FALSE, FALSE, TRUE, NULL, URL_UNDETERMINED_CONTENT, override_charset_id);
	g_charsetManager->DecrementCharsetIDReference(override_charset_id);

	if (!speculative_data_desc)
		return OpStatus::ERR;

	while (new_stream_position)
	{
		BOOL more = FALSE;

		speculative_data_desc->RetrieveData(more);
		speculative_data_desc->GetBuffer();

		unsigned length = UNICODE_DOWNSIZE(speculative_data_desc->GetBufSize());
		if (length == 0)
			return OpStatus::ERR_NO_MEMORY;

		if (length > new_stream_position)
			length = new_stream_position;

		speculative_data_desc->ConsumeData(UNICODE_SIZE(length));
		new_stream_position -= length;

		if (new_stream_position != 0 && !more)
		{
			OP_ASSERT(!"Not able to seek to the expected position in the stream. The data should have been in the url cache so this should not be possible.");
			break;
		}
	}

	return OpStatus::OK;
}

OP_STATUS
FramesDocument::GetSpeculativeParserURLs(OpVector<URL> &urls)
{
	LoadInlineElmHashIterator iterator(inline_hash);

	for (LoadInlineElm *lie = iterator.First(); lie; lie = iterator.Next())
		if (lie->IsSpeculativeLoad())
			RETURN_IF_MEMORY_ERROR(urls.Add(lie->GetUrl()));

	return OpStatus::OK;
}

#endif // SPECULATIVE_PARSER

OP_STATUS FramesDocument::HandleLogdocParsingComplete()
{
	OP_STATUS status = OpStatus::OK;
	// Send the DOMCONTENTLOADED event that is sent as soon as the HTML tree
	// is complete, even if there might be external resources not yet loaded.
	if (!has_sent_domcontent_loaded)
	{
		OP_BOOLEAN handled = OpBoolean::IS_FALSE;

		if (dom_environment && dom_environment->IsEnabled())
		{
			DOM_Environment::EventData data;

			data.type = DOMCONTENTLOADED;
			data.target = logdoc->GetRoot();

			handled = dom_environment->HandleEvent(data);

			if (OpStatus::IsMemoryError(handled))
				status = OpStatus::ERR_NO_MEMORY;
		}

#ifdef SCOPE_SUPPORT
		if (handled == OpBoolean::IS_FALSE)
		{
			/* Send notifications immediately in case there was
			   no thread created that would handle these. */
			OpScopeReadyStateListener::OnReadyStateChanged(this, OpScopeReadyStateListener::READY_STATE_BEFORE_DOM_CONTENT_LOADED);
			OpScopeReadyStateListener::OnReadyStateChanged(this, OpScopeReadyStateListener::READY_STATE_AFTER_DOM_CONTENT_LOADED);
		}
#endif // SCOPE_SUPPORT

		if (OpStatus::IsSuccess(status)) // if last event failed, no meaning to try this
			if (FramesDocument *parentdoc = GetParentDoc())
				if (HTML_Element *target = GetDocManager()->GetFrame()->GetHtmlElement())
					if (DOM_Environment *environment = parentdoc->GetDOMEnvironment())
						if (environment->IsEnabled())
						{
							DOM_Environment::EventData data;

							data.type = DOMFRAMECONTENTLOADED;
							data.target = target;

							OP_BOOLEAN handled = environment->HandleEvent(data);

							if (OpStatus::IsMemoryError(handled))
								status = OpStatus::ERR_NO_MEMORY;
						}

		has_sent_domcontent_loaded = TRUE;
	}

	if (es_info.onpopstate_pending)
	{
		es_info.onpopstate_pending = FALSE;
		status = HandleWindowEvent(ONPOPSTATE, NULL, NULL, 0, NULL);
	}

	// Do an immediate (kind of) reflow instead of waiting for timeout
	if (InlineImagesLoaded())
		PostReflowMsg(TRUE);

	if (!IsPrintDocument()) // logdoc is shared between the print document and real document (sigh)
		logdoc->SetCompleted(FALSE);

	RETURN_IF_MEMORY_ERROR(CheckFinishDocument());

	return status;
}

void FramesDocument::CheckSmartFrames(BOOL on)
{
	if (frm_root && smart_frames != on)
		frm_root->CheckSmartFrames(on);

	smart_frames = on;
}

void FramesDocument::ExpandFrameSize(int inc_width, long inc_height)
{
	if (frm_root)
		frm_root->ExpandFrameSize(inc_width, inc_height);
}

void FramesDocument::CalculateFrameSizes()
{
	if (frm_root && frames_policy != FRAMES_POLICY_NORMAL)
		frm_root->CalculateFrameSizes(frames_policy);
}

void FramesDocument::RecalculateMagicFrameSizes()
{
	if (frames_policy != FRAMES_POLICY_NORMAL && !IsTopDocument() && !IsFrameDoc() && !IsInlineFrame())
	{
		// FIXME: GetTopFramesDoc() doesn't necessarily get the top document (!)

		FramesDocument* top_doc = GetTopFramesDoc();

		if (HTML_Element* root = top_doc->GetDocRoot())
			root->MarkDirty(top_doc, FALSE);
	}
}

void FramesDocument::CheckFrameStacking(BOOL stack_frames)
{
	if (frm_root && stack_frames != frames_stacked)
		frm_root->CheckFrameStacking(stack_frames);

	frames_stacked = stack_frames;
}

OP_BOOLEAN FramesDocument::CheckSource()
{
	OP_BOOLEAN stat = OpBoolean::IS_FALSE;
	OpStatus::Ignore(stat);

	if (!logdoc)
	{
		MessageHandler* mh = GetMessageHandler();

		history_reload = GetDocManager()->IsWalkingInHistory();

		if (!IsWrapperDoc() &&
#ifdef HAS_ATVEF_SUPPORT
			url.ContentType() != URL_TV_CONTENT &&
#endif // HAS_ATVEF_SUPPORT
			url.Type() != URL_JAVASCRIPT)
		{
			RETURN_IF_MEMORY_ERROR(InitParsing());
		}

		packed.data_desc_done = 0;
		stream_position_base = 0;
		local_doc_finished = FALSE;
		doc_tree_finished = FALSE;
		delay_doc_finish = posted_delay_doc_finish = FALSE;
		wrapper_doc_source_read_offset = 0;

		logdoc = OP_NEW(LogicalDocument, (this));
		if (! logdoc || OpStatus::IsError(logdoc->Init()))
		{
			OP_DELETE(logdoc);
			logdoc = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}

		if (parse_xml_as_html)
			logdoc->SetParseXmlAsHtml();

		if (!mh->HasCallBack(this, MSG_URL_MOVED, 0))
			if (OpStatus::IsError(mh->SetCallBack(this, MSG_URL_MOVED, 0)))
			{
				UnsetAllCallbacks(TRUE);

				return(OpStatus::ERR_NO_MEMORY);
			}

		stat = CreateESEnvironment(TRUE);
		if (stat == OpStatus::OK)
			stat = OpBoolean::IS_TRUE;

		if( stat != OpStatus::ERR_NO_MEMORY )
		{
			layout_mode = GetWindow()->GetLayoutMode();
			era_mode = GetWindow()->GetERA_Mode();

			CheckERA_LayoutMode();

#ifdef CONTENT_MAGIC
			content_magic_start.Reset();
			content_magic_found = FALSE;
			content_magic_character_count = 0;
			content_magic_position = 0;
#endif // CONTENT_MAGIC

#ifdef SVG_SUPPORT
			if (url.ContentType() == URL_SVG_CONTENT)
				if (FramesDocElm *frame = GetDocManager()->GetFrame())
					frame->SetNotifyParentOnContentChange();
#endif

			OP_STATUS s = LoadData(FALSE, FALSE);

			if (OpStatus::IsError(s))
				stat = s;
		}
	}
	else // we have a logical document
	{
		// 2008-04-05 Since the doctype is not checked to be handheld
		// when document is taken from cache, OnHandheldChanged() was
		// not called. Part of fix for bug #269300 -magnez
		HLDocProfile* hld = logdoc->GetHLDocProfile();
		WindowCommander* win_com = GetWindow()->GetWindowCommander();
		win_com->GetDocumentListener()->OnHandheldChanged(win_com, hld->GetHasHandheldDocType());
		hld->SetHandheldCallbackCalled(TRUE);
#ifdef _WML_SUPPORT_
		if (logdoc->GetHLDocProfile()->HasWmlContent())
		{
			logdoc->WMLReEvaluate();
			GetDocManager()->WMLGetContext()->PostParse();
		}
#endif // _WML_SUPPORT_
	}

#ifdef PLATFORM_FONTSWITCHING
	if (logdoc)
	{
		VisualDevice *vis_dev = GetVisualDevice();
		if (vis_dev && vis_dev->GetView())
			vis_dev->GetOpView()->SetPreferredCodePage(logdoc->GetHLDocProfile()->GetPreferredCodePage());
	}
#endif // PLATFORM_FONTSWITCHING

	return stat;
}

OP_STATUS FramesDocument::HandleLoading(OpMessage msg, URL_ID url_id, MH_PARAM_2 user_data)
{
	OP_PROBE5(OP_PROBE_FRAMESDOCUMENT_HANDLELOADING);

	if (FramesDocElm *frame = GetDocManager()->GetFrame())
		if (frame->IsDeleted())
			return OpStatus::OK;

	if (!IsCurrentDoc())
	{
		/* If this document is replacing another one (by means of document.write),
		   the data belongs to the document it is replacing. */
		if (es_replaced_document)
		{
			OP_ASSERT(es_replaced_document->IsCurrentDoc());
			return es_replaced_document->HandleLoading(msg, url_id, user_data);
		}
		else
			OP_ASSERT(FALSE);
	}

	OP_STATUS stat = OpStatus::OK;

	OpStatus::Ignore(stat);		// For OOM debugging
	if (url.Id(TRUE) == url_id || (!aux_url_to_handle_loading_messages_for.IsEmpty() && aux_url_to_handle_loading_messages_for.Id(TRUE) == url_id))
	{
		switch (msg)
		{
		case MSG_URL_DATA_LOADED:
			/* If the DocumentManager's load status is NOT_LOADING, it seems to
			   have done StopLoading(), and we shouldn't start or continue
			   parsing here.  The message was probably just sitting around in
			   the message loop already when we stopped loading, and we should
			   ignore it. */

			if (GetDocManager()->GetLoadStatus() != NOT_LOADING)
			{
				// parse new data
				if (!logdoc)
				{
					logdoc = OP_NEW(LogicalDocument, (this));

					if (! logdoc || OpStatus::IsError(logdoc->Init()))
					{
						OP_DELETE(logdoc);
						logdoc = NULL;
						return OpStatus::ERR_NO_MEMORY;
					}
				}

				return LoadData(TRUE, FALSE);
			}
			else
				OP_ASSERT(!logdoc || logdoc->IsParsed());

			break;

		case MSG_NOT_MODIFIED:
			stat = CheckRefresh();
			if (frm_root)
				frm_root->ReloadIfModified();

			return stat;

		case MSG_MULTIPART_RELOAD:
			is_multipart_reload = TRUE;
			return LoadData(TRUE, FALSE);
		}
	}

	if (frm_root)
		stat = frm_root->HandleLoading(msg, url_id, user_data);
	else
	{
		if (ifrm_root)
			return(ifrm_root->HandleLoading(msg, url_id, user_data));
	}

	return stat;
}

static OpRect ConstrainRectToBoundary(OpRect rect, const OpRect& boundary)
{
	if (rect.Right() > boundary.Right())
		rect.x = boundary.Right() - rect.width;

	if (rect.x < boundary.x)
		rect.x = boundary.x;

	if (rect.Bottom() > boundary.Bottom())
		rect.y = boundary.Bottom() - rect.height;

	if (rect.y < boundary.y)
		rect.y = boundary.y;

	return rect;
}

int FramesDocument::GetLayoutViewX()
{
	return GetLayoutViewport().x;
}

int FramesDocument::GetLayoutViewY()
{
	return GetLayoutViewport().y;
}

int FramesDocument::GetLayoutViewWidth()
{
	return GetLayoutViewport().width;
}

int FramesDocument::GetLayoutViewHeight()
{
	return GetLayoutViewport().height;
}

OpRect FramesDocument::GetLayoutViewport()
{
	if (logdoc)
		return logdoc->GetLayoutWorkplace()->GetLayoutViewport();

	return OpRect();
}

OpRect FramesDocument::SetLayoutViewPos(int x, int y)
{
	if (logdoc)
		return logdoc->GetLayoutWorkplace()->SetLayoutViewPos(LayoutCoord(x), LayoutCoord(y));

	return OpRect();
}

BOOL FramesDocument::RecalculateLayoutViewSize(BOOL user_action)
{
	if (logdoc)
	{
		short old_layout_width = GetLayoutViewWidth();
		long old_layout_height = GetLayoutViewHeight();

		logdoc->GetLayoutWorkplace()->RecalculateLayoutViewSize();

		BOOL size_changed = old_layout_width != GetLayoutViewWidth() || old_layout_height != GetLayoutViewHeight();

		if (size_changed && user_action)
		{
			if (frames_policy == FRAMES_POLICY_NORMAL || IsTopDocument())
				if (HandleDocumentEvent(ONRESIZE) == OpStatus::ERR_NO_MEMORY)
					GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		}
		return size_changed;
	}
	return FALSE;
}

BOOL FramesDocument::RecalculateScrollbars(BOOL keep_existing_scrollbars)
{
	if (logdoc)
		return logdoc->GetLayoutWorkplace()->RecalculateScrollbars(keep_existing_scrollbars);

	return FALSE;
}

OpRect FramesDocument::GetVisualViewport()
{
	if (IsTopDocument())
	{
		OpRect visual_viewport = GetWindow()->GetViewportController()->GetVisualViewport();

		visual_viewport.x -= NegativeOverflow();

		return visual_viewport;
	}
	else
		return GetVisualDevice()->GetRenderingViewport();
}

BOOL FramesDocument::RequestSetVisualViewport(const OpRect& viewport, OpViewportChangeReason reason)
{
	if (IsTopDocument())
	{
		ViewportController* controller = GetWindow()->GetViewportController();

		OpRect visual_viewport = controller->GetVisualViewport();

		if (!visual_viewport.Equals(viewport))
		{
			OpViewportRequestListener* listener = controller->GetViewportRequestListener();
			OpRect adjusted_viewport(viewport);

			adjusted_viewport.x += NegativeOverflow();
			listener->OnVisualViewportChangeRequest(controller, adjusted_viewport, adjusted_viewport, reason);

			return TRUE;
		}
	}
	else
		return RequestSetVisualViewPos(viewport.x, viewport.y, reason);

	return FALSE;
}

BOOL FramesDocument::RequestSetVisualViewPos(INT32 x, INT32 y, OpViewportChangeReason reason)
{
	int doc_width = Width();
	long doc_height = Height();
	int negative_overflow = NegativeOverflow();
	OpRect doc_rect(-negative_overflow, 0, doc_width + negative_overflow, doc_height);
	const OpRect visual_viewport = GetVisualViewport();
	OpRect new_visual_viewport = visual_viewport;

	new_visual_viewport.x = x;
	new_visual_viewport.y = y;
	new_visual_viewport = ConstrainRectToBoundary(new_visual_viewport, doc_rect);

	if (new_visual_viewport.x != visual_viewport.x || new_visual_viewport.y != visual_viewport.y)
	{
		if (IsTopDocument())
		{
			ViewportController* controller = GetWindow()->GetViewportController();
			OpViewportRequestListener* listener = controller->GetViewportRequestListener();
			OpViewportRequestListener::ViewportPosition pos(
				OpPoint(new_visual_viewport.x, new_visual_viewport.y),
				OpViewportRequestListener::EDGE_LEFT,
				OpViewportRequestListener::EDGE_TOP);

			pos.point.x += negative_overflow;
			listener->OnVisualViewportEdgeChangeRequest(controller, pos, reason);

			return TRUE;
		}
		else
		{
			FramesDocElm* this_frame = doc_manager->GetFrame();
			BYTE frame_scrolling = this_frame ? (VisualDevice::ScrollType)this_frame->GetFrameScrolling() : VisualDevice::VD_SCROLLING_AUTO;

			if (frame_scrolling != SCROLLING_NO || (reason != VIEWPORT_CHANGE_REASON_INPUT_ACTION && reason != VIEWPORT_CHANGE_REASON_SPATIAL_NAVIGATION))
			{
				VisualDevice* vis_dev = GetVisualDevice();

				vis_dev->SetRenderingViewPos(new_visual_viewport.x, new_visual_viewport.y, FALSE);
				SetLayoutViewPos(new_visual_viewport.x, new_visual_viewport.y);
				return TRUE;
			}
		}
	}

	return FALSE;
}

BOOL FramesDocument::RequestScrollIntoView(const OpRect& rect, SCROLL_ALIGN align, BOOL strict_align, OpViewportChangeReason reason)
{
	VisualDevice* vis_dev = GetVisualDevice();
	const OpRect visual_viewport = GetVisualViewport();
	OpRect requested_visual_viewport(visual_viewport);
	int doc_width = Width();
	long doc_height = Height();
	int negative_overflow = NegativeOverflow();
	BOOL changed = FALSE;

	if (strict_align || !visual_viewport.Contains(rect))
	{
		const OpRect layout_viewport = GetLayoutViewport();
		const OpRect doc_rect(-negative_overflow, 0, doc_width + negative_overflow, doc_height);
		OpRect new_layout_viewport = layout_viewport;
		INT32 new_xpos = visual_viewport.x;
		INT32 new_ypos = visual_viewport.y;
		BOOL fix_x_pos = FALSE;

		switch (align)
		{
		case SCROLL_ALIGN_CENTER:
		{
			INT32 modified_width = rect.width;
			INT32 modified_height = rect.height;

			// For rectangles larger than viewport, scroll so rect is (almost) in upper left corner

			if (modified_width > visual_viewport.width)
				modified_width = visual_viewport.width * 8 / 10;

			if (modified_height > visual_viewport.height)
				modified_height = visual_viewport.height * 8 / 10;

			if (rect.x < visual_viewport.x || rect.x + rect.width > visual_viewport.x + visual_viewport.width)
			{
				new_xpos = rect.x + modified_width / 2 - (visual_viewport.width / 2);
				new_xpos = MAX(0, MIN(new_xpos, doc_width - visual_viewport.width));
			}

			if (rect.y < visual_viewport.y || rect.y + rect.height > visual_viewport.y + visual_viewport.height)
			{
				new_ypos = rect.y + modified_height / 2 - (visual_viewport.height / 2);
				new_ypos = MAX(0, MIN(new_ypos, doc_height - visual_viewport.height));
				new_layout_viewport.y = rect.y + (rect.height - layout_viewport.height) / 2;
			}
			break;
		}

#if defined(DOCUMENT_EDIT_SUPPORT) || defined(KEYBOARD_SELECTION_SUPPORT)
		case SCROLL_ALIGN_NEAREST:
			if (rect.x < visual_viewport.x)
				new_xpos = rect.x;

			if (rect.y < visual_viewport.y)
				new_ypos = rect.y;

			if (rect.x + rect.width > visual_viewport.x + visual_viewport.width)
				new_xpos = rect.x + rect.width - visual_viewport.width;

			if (rect.y + rect.height > visual_viewport.y + visual_viewport.height)
				new_ypos = rect.y + rect.height - visual_viewport.height;

			break;
#endif // DOCUMENT_EDIT_SUPPORT || KEYBOARD_SELECTION_SUPPORT

		case SCROLL_ALIGN_LAZY_TOP:
		{
			// If the rect cannot be visible after any scroll, give up and don't move.
			if (!doc_rect.Intersecting(rect))
				break;

			BOOL vv_x_moved = FALSE;
			BOOL vv_y_moved = FALSE;
			BOOL is_rtl_doc = FALSE;
#ifdef SUPPORT_TEXT_DIRECTION
			LogicalDocument* logdoc = GetLogicalDocument();
			if (logdoc)
				is_rtl_doc = logdoc->GetLayoutWorkplace()->IsRtlDocument();
#endif //SUPPORT_TEXT_DIRECTION
			if (rect.Top() > visual_viewport.Bottom() - SPOTLIGHT_MARGIN
				|| rect.Bottom() < visual_viewport.Top() + SPOTLIGHT_MARGIN)
			{
				new_ypos = rect.Top() - SPOTLIGHT_MARGIN;
				vv_y_moved = TRUE;
			}
			if (rect.Left() > visual_viewport.Right() - SPOTLIGHT_MARGIN
				|| rect.Right() < visual_viewport.Left() + SPOTLIGHT_MARGIN)
			{
				new_xpos = is_rtl_doc
					? rect.Right() - visual_viewport.width + SPOTLIGHT_MARGIN
					: rect.Left() - SPOTLIGHT_MARGIN;
				vv_x_moved = TRUE;
			}

			// If we have to move, the move should show beginning of the rect.
			if (vv_x_moved && !vv_y_moved
				&& (rect.Bottom() > visual_viewport.Bottom() - SPOTLIGHT_MARGIN
					|| rect.Top() < visual_viewport.Top() + SPOTLIGHT_MARGIN))
				new_ypos = rect.y - SPOTLIGHT_MARGIN;
			if (vv_y_moved && !vv_x_moved
				&& (rect.Right() > visual_viewport.Right() - SPOTLIGHT_MARGIN
					|| rect.Left() < visual_viewport.Left() + SPOTLIGHT_MARGIN))
				new_xpos = is_rtl_doc
					? rect.Right() - visual_viewport.width + SPOTLIGHT_MARGIN
					: rect.Left() - SPOTLIGHT_MARGIN;
			break;
		}


		case SCROLL_ALIGN_SPOTLIGHT:
		{
			new_ypos = rect.y - SPOTLIGHT_MARGIN;

#if defined SKIN_SUPPORT && defined SKIN_OUTLINE_SUPPORT
			OpSkinElement* skin_elm = g_skin_manager->GetSkinElement("Active Element Inside");
			if (skin_elm)
			{
				INT32 padding;
				skin_elm->GetPadding(&padding, &padding, &padding, &padding, 0);
				new_ypos -= padding;

				INT32 border_width;
				skin_elm->GetBorderWidth(&border_width, &border_width, &border_width, &border_width, 0);

				new_ypos -= border_width;

				if (new_ypos < 0)
					new_ypos = 0;
			}
#endif // SKIN_SUPPORT && SKIN_OUTLINE_SUPPORT

			new_layout_viewport.y = new_ypos;
			fix_x_pos = TRUE;
			break;
		}

		case SCROLL_ALIGN_TOP:
			new_ypos = rect.y;
			new_layout_viewport.y = rect.y;
			fix_x_pos = TRUE;
			break;

		case SCROLL_ALIGN_BOTTOM:
			new_ypos = rect.y + rect.height - visual_viewport.height;
			new_layout_viewport.y = rect.y + rect.height - layout_viewport.height;
			fix_x_pos = TRUE;
			break;
		}

		if (fix_x_pos)
		{
			if (rect.width > visual_viewport.width)
			{
				// Can't fit the whole width
#ifdef SUPPORT_TEXT_DIRECTION
				BOOL is_rtl_doc = FALSE;
				LogicalDocument* logdoc = GetLogicalDocument();
				if (logdoc)
					is_rtl_doc = logdoc->GetLayoutWorkplace()->IsRtlDocument();

				if (is_rtl_doc)
				{
					// Can't fit the whole width, make sure the right edge is visible
					new_xpos = rect.x + rect.width - visual_viewport.width;
				}
				else
#endif // SUPPORT_TEXT_DIRECTION
				{
					// Can't fit the whole width, make sure the left edge is visible
					new_xpos = rect.x;
				}
			}
			else if (rect.x < visual_viewport.x)
			{
				// Scroll the left edge into view
				new_xpos = rect.x;
			}
			else if (rect.x + rect.width > visual_viewport.x + visual_viewport.width)
			{
				// Scroll the right edge into view
				new_xpos = rect.x + rect.width - visual_viewport.width;
			}
		}

		if (new_ypos < 0 && rect.y + rect.height - 1 >= 0)
			new_ypos = 0;

		new_layout_viewport = ConstrainRectToBoundary(new_layout_viewport, doc_rect);

		requested_visual_viewport.x = new_xpos;
		requested_visual_viewport.y = new_ypos;
		requested_visual_viewport = ConstrainRectToBoundary(requested_visual_viewport, doc_rect);

		changed = visual_viewport.x != requested_visual_viewport.x || visual_viewport.y != requested_visual_viewport.y;

		if (changed)
		{
			OpRect updated_area = SetLayoutViewPos(new_layout_viewport.x, new_layout_viewport.y);

			if (!IsTopDocument())
				vis_dev->SetRenderingViewPos(new_xpos, new_ypos, FALSE, &updated_area);
		}
	}

	ViewportController* controller = GetWindow()->GetViewportController();

	if (IsTopDocument())
	{
		if (changed)
		{
			OpViewportRequestListener* listener = controller->GetViewportRequestListener();
			OpRect adjusted_viewport(requested_visual_viewport);
			OpRect adjusted_rect(rect);

			adjusted_viewport.x += negative_overflow;
			adjusted_rect.x += negative_overflow;

			listener->OnVisualViewportChangeRequest(controller, adjusted_viewport, adjusted_rect, reason);
		}
	}
	else
	{
		/* Make sure that ancestor documents are scrolled to the right position
		   as well. This needs to be done regardless of this document's visual
		   viewport changing. */

		FramesDocElm* fde = GetDocManager()->GetFrame();

		/* Update cached iframe position. */
		RECT frame_rect;
		HTML_Element *frame_element = fde->GetHtmlElement();
		if (frame_element && fde->GetParentFramesDoc()->GetLogicalDocument()->GetBoxRect(frame_element, BOUNDING_BOX, frame_rect))
		{
			AffinePos frame_pos;
			frame_pos.SetTranslation(frame_rect.left, frame_rect.top);
			fde->SetPosition(frame_pos);
		}

		OpRect parent_rect(rect);

		parent_rect.x += fde->GetAbsX() - vis_dev->GetRenderingViewX();
		parent_rect.y += fde->GetAbsY() - vis_dev->GetRenderingViewY();

		return GetDocManager()->GetParentDoc()->RequestScrollIntoView(parent_rect, align, FALSE, reason) || changed;
	}

	return changed;
}

void FramesDocument::RequestSetViewportToInitialScale(OpViewportChangeReason reason)
{
#ifdef CSS_VIEWPORT_SUPPORT
	HLDocProfile* hld_profile = GetHLDocProfile();
	if (hld_profile)
	{
		CSSCollection* css_coll = hld_profile->GetCSSCollection();
		CSS_Viewport* css_viewport = css_coll->GetViewport();
		double initial_zoom_level = ZoomLevelNotSet;

		if (css_viewport->GetZoom() != CSS_VIEWPORT_ZOOM_AUTO)
			initial_zoom_level = css_viewport->GetZoom();

		ViewportController* controller = GetWindow()->GetViewportController();
		OpViewportRequestListener* request_listener = controller->GetViewportRequestListener();

		request_listener->OnZoomLevelChangeRequest(controller, initial_zoom_level, 0, reason);
	}
#endif // CSS_VIEWPORT_SUPPORT
}

void FramesDocument::RequestConstrainVisualViewPosToDoc()
{
#ifdef _PRINT_SUPPORT_
	// this bailout is needed in print preview mode. without it, a
	// reflow in the non-print document will constrain the viewport of
	// the previewed document to the bounds of the non-print document.
	// this may for example may make it impossible to scroll to the
	// end of the previewed document.
	if (GetWindow()->GetPreviewMode() && !IsPrintDocument())
		return;
#endif // _PRINT_SUPPORT_

	int doc_width = Width();
	long doc_height = Height();
	int negative_overflow = NegativeOverflow();
	const OpRect visual_viewport = GetVisualViewport();
	const OpRect layout_viewport = GetLayoutViewport();
	const OpRect doc_rect(-negative_overflow, 0, doc_width + negative_overflow, doc_height);

	OpRect new_visual_viewport = ConstrainRectToBoundary(visual_viewport, doc_rect);
	OpRect new_layout_viewport = ConstrainRectToBoundary(layout_viewport, doc_rect);

	if (!new_layout_viewport.Equals(layout_viewport))
		SetLayoutViewPos(new_layout_viewport.x, new_layout_viewport.y);

	if (!new_visual_viewport.Equals(visual_viewport))
		RequestSetVisualViewPos(new_visual_viewport.x, new_visual_viewport.y, VIEWPORT_CHANGE_REASON_DOCUMENT_SIZE);
}

URL FramesDocument::GetImageURL(int img_id)
{
	LoadInlineElm* lie = GetInline(img_id);

	if (lie && lie->GetUrl())
		return *lie->GetUrl();

	return URL();
}


#ifdef DOCUMENT_EDIT_SUPPORT

OP_STATUS FramesDocument::SetEditable(EditableMode mode)
{
	if (document_edit && (mode == DESIGNMODE_OFF && document_edit->IsUsingDesignMode() ||
		                  mode == CONTENTEDITABLE_OFF && !document_edit->IsUsingDesignMode()))
	{
		OP_DELETE(document_edit);
		document_edit = NULL;

#ifdef KEYBOARD_SELECTION_SUPPORT
		if (!GetKeyboardSelectionMode())
#endif // KEYBOARD_SELECTION_SUPPORT
		{
			if (caret_painter)
			{
				caret_painter->StopBlink();
				OP_DELETE(caret_painter);
				caret_painter = NULL;
			}
		}
	}
	else if ((mode == DESIGNMODE || mode == CONTENTEDITABLE) && !document_edit)
	{
		if (!IsCurrentDoc())
			return OpStatus::ERR;
		if (GetWindow()->GetType() == WIN_TYPE_MAIL_VIEW)
			return OpStatus::ERR;

		if (!caret_painter)
		{
			caret_painter = OP_NEW(CaretPainter, (this));
			if (!caret_painter)
				return OpStatus::ERR_NO_MEMORY;
		}

		return OpDocumentEdit::Construct(&document_edit, this, (mode == DESIGNMODE) ? TRUE : FALSE);
	}

	return OpStatus::OK;
}

BOOL FramesDocument::GetDesignMode()
{
	return GetDocumentEdit() && GetDocumentEdit()->m_body_is_root;
}

#endif // DOCUMENT_EDIT_SUPPORT


void FramesDocument::FlushAsyncInlineElements(HTML_Element *helm)
{
	OP_PROBE7(OP_PROBE_FRAMESDOC_FLUSHINLINEELM);

	if (async_inline_elms.GetCount() > 0)
	{
		OP_PROBE8(OP_PROBE_FRAMESDOC_FLUSHINLINEELM3);
		List<AsyncLoadInlineElm> *async_inlines;
		if (OpStatus::IsSuccess(async_inline_elms.Remove(helm, &async_inlines)))
		{
			async_inlines->Clear();
			OP_DELETE(async_inlines);
		}
	}
}

void FramesDocument::SendImageLoadingInfo()
{
	if (FramesDocument* top_doc = GetTopDocument())
	{
		ImageLoadingInfo info;
		top_doc->GetImageLoadingInfo(info);

		GetWindow()->UpdateLoadingInformation(info.loaded_count, info.total_count);
	}
}

OP_BOOLEAN FramesDocument::LoadImages(BOOL first_image_only)
{
	LoadInlineElmHashIterator iterator(inline_hash);
	for (LoadInlineElm* lie = iterator.First(); lie; lie = iterator.Next())
		for (HEListElm* helm = lie->GetFirstHEListElm(); helm; helm = (HEListElm*) helm->Suc())
			if (helm->IsImageInline())
			{
				if (first_image_only)
					if (lie->GetLoaded() || lie->GetLoading() || !helm->HElm()->GetLayoutBox())
						break;

				/* FIXME: LocalLoadInline should be changed to take a
				   LoadInlineElm as parameter... [GI] */

				switch (LocalLoadInline(lie->GetUrl(), helm->GetInlineType(), helm, helm->HElm(), LoadInlineOptions().ForceImageLoad()))
				{
				case OpStatus::ERR_NO_MEMORY:
					return OpStatus::ERR_NO_MEMORY;

				case LoadInlineStatus::LOADING_STARTED:
					if (first_image_only)
						return OpBoolean::IS_TRUE;

				default:
					break;
				}
			}

	return OpBoolean::IS_TRUE;
}

void FramesDocument::StopLoadingAllImages()
{
	/* Note: no-one cares about the return value. */
	DocumentTreeIterator iter(this);
	iter.SetIncludeThis();
	while (iter.Next())
	{
		FramesDocument* frm_doc = iter.GetFramesDocument();
		if (frm_doc->url.IsImage())
			continue;

		MessageHandler* mh = frm_doc->GetMessageHandler();

		LoadInlineElmHashIterator iterator(frm_doc->inline_hash);
		for (LoadInlineElm* lie = iterator.First(); lie; lie = iterator.Next())
		{
			if (lie->GetLoading() && lie->HasOnlyImageInlines())
			{
				frm_doc->SetInlineStopped(lie);

				URL* img_url = lie->GetUrl();

				if (img_url->Status(FALSE) == URL_LOADING && lie->HasInlineType(IMAGE_INLINE))
				{
					img_url->StopLoading(mh);
				}
				else
				{
					URL nxt_url = img_url->GetAttribute(URL::KMovedToURL);

					while (!nxt_url.IsEmpty())
					{
						if (nxt_url.Status(FALSE) == URL_LOADING)
						{
							img_url->StopLoading(mh);
							break;
						}
						else
							nxt_url = nxt_url.GetAttribute(URL::KMovedToURL);
					}
				}
			}
		} // end for all LoadInlineElm
	} // end for all documents
}

void FramesDocument::StopLoadingAllInlines(BOOL stop_plugin_streams)
{
	MessageHandler* mh = GetMessageHandler();

	async_inline_elms.DeleteAll();

	LoadInlineElmHashIterator iterator(inline_hash);
	for (LoadInlineElm* lie = iterator.First(); lie; lie = iterator.Next())
	{
		URL* inline_url = lie->GetUrl();

		// Stop all inlines !!!

		if (lie->GetLoading())
		{
			if (inline_url && inline_url->Status(FALSE) == URL_LOADING)
				inline_url->StopLoading(mh);

			SetInlineStopped(lie);
		}

#ifdef _PLUGIN_SUPPORT_
		if (stop_plugin_streams)
		{
			HEListElm* iter = lie->GetFirstHEListElm();
			while (iter)
			{
				if (iter->GetInlineType() == EMBED_INLINE)
				{
					OpNS4Plugin* plugin_instance = iter->HElm()->GetNS4Plugin();
					if (plugin_instance)
						plugin_instance->StopLoadingAllStreams();
				}

				iter = iter->Suc();
			}
		}
#endif // _PLUGIN_SUPPORT_
	}
}

void FramesDocument::SetImagePosition(HTML_Element* element, const AffinePos& pos, long width, long height, BOOL background, HEListElm* helm /* = NULL */, BOOL expand_area /* = FALSE */)
{
	if (!helm)
		helm = element->GetHEListElm(background);

	OP_ASSERT(helm);
	OP_ASSERT(helm->GetFramesDocument() == this);

	if (helm)
	{
		helm->Display(this, pos, width, height, background, expand_area);

		BOOL sync_decoded = FALSE;
#ifdef _PRINT_SUPPORT_
		if ( IsPrintDocument())
		{
			helm->LoadAll(FALSE);
			sync_decoded = TRUE;
		}
#endif // _PRINT_SUPPORT_

#ifdef IMAGE_TURBO_MODE
		if (!sync_decoded)
			if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::TurboMode, GetHostName()))
			{
				helm->LoadAll(FALSE);
				sync_decoded = TRUE;
			}
#endif
		if (!sync_decoded)
			helm->LoadSmall();
	}
}

// return FALSE if oom
BOOL FramesDocument::HandleInlineDataLoaded(LoadInlineElm* lie)
{
	BOOL out_of_memory = FALSE;
	HLDocProfile* hld_profile = logdoc->GetHLDocProfile();
	URLStatus inline_url_stat = lie->GetUrl()->Status(TRUE);

	BOOL refuse_processing = lie->GetAccessForbidden();

	if (refuse_processing)
		inline_url_stat = URL_LOADING_FAILURE;

	switch (inline_url_stat)
	{
	case URL_UNLOADED:
		SetInlineStopped(lie);
		break;

	case URL_LOADED:
	case URL_LOADING_ABORTED:
		SetInlineLoading(lie, FALSE);
		break;

	case URL_LOADING:
		SetInlineLoading(lie, TRUE);
		break;

	default:
		SetInlineStopped(lie);
		break;
	}

	lie->SetIsProcessing(TRUE);

	BOOL image_data_handled = FALSE;
	Image img;

	HEListElm* helm = lie->GetFirstHEListElm();

	while (helm && !out_of_memory)
	{
		inline_data_loaded_helm_next = static_cast<HEListElm*>(helm->Suc());

		switch (helm->GetInlineType())
		{
		case IMAGE_INLINE:
		case BGIMAGE_INLINE:
		case EXTRA_BGIMAGE_INLINE:
		case BORDER_IMAGE_INLINE:
		case VIDEO_POSTER_INLINE:
			{
				HTML_Element* he = helm->HElm();

				if (he)
				{
					BOOL size_changed = FALSE;
					img = helm->GetImage();

					UINT32 old_image_width = img.Width();
					UINT32 old_image_height = img.Height();

					lie->ResetImageIfNeeded();

					URL old_url;
					URL url = *lie->GetUrl();
					old_url = helm->GetOldUrl();
					if (!old_url.IsEmpty())
					{
						if (url.Status(TRUE) == URL_LOADING)
						{
							helm = inline_data_loaded_helm_next;
							continue;
						}

						//the content provider for this image will change, so
						//cleanup to prevent assert due to dangling reference
						img = Image();

						helm->UpdateImageUrl(&url);

						size_changed = TRUE;
						image_data_handled = TRUE;
						img = helm->GetImage();
					}

#ifdef SVG_SUPPORT
					if (!refuse_processing &&
						url.Status(TRUE) == URL_LOADED && url.ContentType() == URL_SVG_CONTENT &&
						he->GetNsType() != NS_SVG &&
						(!(logdoc && logdoc->GetRoot() && logdoc->GetRoot()->IsAncestorOf(he)) ||
						 (!he->IsMatchingType(HE_IMG, NS_HTML) && !he->IsMatchingType(HE_INPUT, NS_HTML)) ||
						 helm->GetInlineType() == BGIMAGE_INLINE ||
						 helm->GetInlineType() == EXTRA_BGIMAGE_INLINE ||
						 helm->GetInlineType() == BORDER_IMAGE_INLINE))
					{
						if (UrlImageContentProvider *content_provider = helm->GetUrlContentProvider())
							content_provider->UpdateSVGImageRef(logdoc);
					}
					else if (!refuse_processing && url.ContentType() == URL_SVG_CONTENT && url.Status(TRUE) == URL_LOADED)
					{
						Box* box = he->GetLayoutBox();
						if (box && !he->GetIsGeneratedContent() &&
							helm->GetInlineType() == IMAGE_INLINE &&
							(he->IsMatchingType(HE_IMG, NS_HTML) || he->IsMatchingType(HE_INPUT, NS_HTML)))
						{
							if (!reflowing)
							{
								// Create a new layout box for the SVG
								he->MarkExtraDirty(this);
								image_data_handled = TRUE;
							}
						}
					}
#endif // SVG_SUPPORT

					if (!image_data_handled)
					{
						img = helm->GetImage();

						helm->MoreDataLoaded();

						size_changed = (old_image_width != 0 && old_image_width != img.Width()) ||
						               (old_image_height != 0 && old_image_height != img.Height());

						image_data_handled = TRUE;
					}

					if (!img.IsEmpty() && img.Width() > 0 && img.Height() > 0)
					{
						if (helm->IsImageInline())
						{
							if (size_changed)
							{
								// The size (or contents) of the image has changed, make sure to repaint all of it
								he->MarkDirty(this, TRUE, TRUE);
							}

							if (helm->GetInlineType() == IMAGE_INLINE || helm->GetInlineType() == VIDEO_POSTER_INLINE)
							{
								if (Box* box = he->GetLayoutBox())
									box->SignalChange(this, BOX_SIGNAL_REASON_IMAGE_DATA_LOADED);

#ifdef SVG_SUPPORT
								else if (he->GetNsType() == NS_SVG && inline_url_stat == URL_LOADED)
								{
									// The actual image element has no box if it's an SVG
									// image. It wants to know when the image is fully decoded.
									g_svg_manager->HandleInlineChanged(this, he);
								}
#endif // SVG_SUPPORT
							}
						}
					}
					else if (inline_url_stat == URL_LOADED && img.IsFailed())
					{
#ifdef SVG_SUPPORT
						if (he->GetNsType() == NS_SVG)
							g_svg_manager->HandleInlineChanged(this, he);
#endif // SVG_SUPPORT
#ifdef MEDIA_HTML_SUPPORT
						if (he->IsMatchingType(HE_VIDEO, NS_HTML) &&
							helm->GetInlineType() == VIDEO_POSTER_INLINE)
						{
							// The <video poster> image is broken, fall back
							// to just displaying the video itself.
							if (MediaElement* media = he->GetMediaElement())
								media->PosterFailed();
						}
#endif // MEDIA_HTML_SUPPORT
					}

					if (!helm->GetEventSent() && !lie->GetLoading()) // Might have been sent when decoding it above
						if (OpStatus::IsMemoryError(helm->SendImageFinishedLoadingEvent(this)))
							out_of_memory = TRUE;
				}
			}
			break;

		case SCRIPT_INLINE:
			if (inline_url_stat == URL_LOADING_FAILURE)
				helm->SendOnError();

			if (inline_url_stat != URL_LOADING && !helm->GetHandled())
			{
#ifdef DELAYED_SCRIPT_EXECUTION
				if (hld_profile->ESHasDelayedScripts())
				{
					if (OpStatus::IsMemoryError(hld_profile->ESSetScriptElementReady(helm->HElm())))
						out_of_memory = TRUE;
				}
				else
#endif // DELAYED_SCRIPT_EXECUTION
					if (OpStatus::IsMemoryError(helm->HElm()->LoadExternalScript(hld_profile)))
						out_of_memory = TRUE;

				helm->SetHandled();
			}
			break;

		case CSS_INLINE:
			if (inline_url_stat != URL_LOADING)
			{
				BOOL was_waiting = IsWaitingForStyles();
				UINT response_code;

				URL url(lie->GetUrl()->GetAttribute(URL::KMovedToURL, URL::KFollowRedirect));
				if (url.IsEmpty())
					url = *lie->GetUrl();

				switch (url.Type())
				{
				case URL_HTTP:
				case URL_HTTPS:
					response_code = url.GetAttribute(URL::KHTTP_Response_Code, URL::KNoRedirect);
					break;

				default:
					response_code = HTTP_OK;
				}

				// Fix for bug #135100, in strict mode only load style files which have correct content type
				BOOL content_type_ok = TRUE;
				if (hld_profile->IsInStrictMode())
				{
					URLContentType content_type = url.ContentType();
					if (content_type != URL_CSS_CONTENT)
					{
						content_type_ok = FALSE;
					}
				}

				// Don't decrement the wait_for_styles counter until style sheet is loaded
				// because it may contain imported style sheets and LocalLoadInline uses
				// wait_for_styles counter to determine if we still want to wait.

				if (inline_url_stat == URL_LOADED && !helm->GetHandled()
					&& (response_code == HTTP_OK || response_code == HTTP_NOT_MODIFIED)
					&& content_type_ok)
				{
					BOOL css_loaded = hld_profile->LoadCSS_Url(helm->HElm());
					helm->SetHandled();

					if (!css_loaded && hld_profile->GetIsOutOfMemory())
						out_of_memory = TRUE;

					if (OpStatus::IsMemoryError(HandleEvent(ONLOAD, NULL, helm->HElm(), SHIFTKEY_NONE)))
						out_of_memory = TRUE;
				}

				if (was_waiting)
				{
					DecWaitForStyles();

					// This might seem like a perfect place to check if all known CSS has been loaded.  It is  not!
					// Check in DecWaitForStyles() instead. There are additional async steps to the loading process
					// (e.g. due to Before/AfterCSS events) so wait_for_styles might never reach zero here.
				}
			}
			break;
#ifdef SHORTCUT_ICON_SUPPORT
		case ICON_INLINE:
			{
				if (inline_url_stat != URL_LOADING)
				{
					// CORE-620 we have to reset an url flag after
					// loading or url won't know that it can cache the url
					lie->GetUrl()->SetAttribute(URL::KSpecialRedirectRestriction, FALSE);
				}
				if (inline_url_stat == URL_LOADED)
				{
					URL *inline_url = lie->GetUrl();
					BOOL can_set_icon = TRUE;

					if (inline_url->Type() == URL_HTTP || inline_url->Type() == URL_HTTPS)
						if (inline_url->GetAttribute(URL::KHTTP_Response_Code, TRUE) >= 400)
							can_set_icon = FALSE;

#if defined SVG_SUPPORT
					if (can_set_icon && inline_url->ContentType() == URL_SVG_CONTENT)
						// SVG favicons have to be loaded as documents.
						can_set_icon = FALSE;
#endif // SVG_SUPPORT

					if (can_set_icon)
					{
						GetWindow()->SetWindowIcon(inline_url);
						favicon = *inline_url;
					}
				}
			}
			break;
#endif // SHORTCUT_ICON_SUPPORT

#ifdef MEDIA_HTML_SUPPORT
		case TRACK_INLINE:
			{
				TrackElement* track_elm = helm->HElm()->GetTrackElement();
				if (inline_url_stat == URL_LOADING && !refuse_processing)
					track_elm->LoadingProgress(helm);
				else
					track_elm->LoadingStopped(helm);
			}
			break;
#endif // MEDIA_HTML_SUPPORT

		case GENERIC_INLINE:
			{
				if (!IsWaitingForStyles() && helm->HElm())
				{
					HTML_Element* element = helm->HElm();

					if (element->IsMatchingType(HE_OBJECT, NS_HTML))
					{
						Box* box = element->GetLayoutBox();

						if (!box)
						{
							/* If this happens during reflow (reflowing==TRUE in the condition below,) we're
							   called from LocalLoadInline, called from LayoutProperties::CreateLayoutBox, and
							   at that point we can't call HTML_Element::MarkExtraDirty().  Nor do we need to,
							   since the layout engine is already about to recreate the layout box, which
							   is why we would call HTML_Element::MarkExtraDirty() in the first place. */
							if (!reflowing && (lie->GetUrl()->Status(TRUE) != URL_LOADING || lie->GetUrl()->ContentType() != URL_UNDETERMINED_CONTENT))
							{
								element->MarkExtraDirty(this);
								logdoc->SetNotCompleted();
							}
#ifdef _PLUGIN_SUPPORT_
							if (inline_url_stat == URL_LOADING_FAILURE)
								OpNS4PluginHandler::GetHandler()->Resume(this, element, TRUE, TRUE);
#endif // _PLUGIN_SUPPORT_
						}
						else
							if (box->IsContentEmbed())
								box->SignalChange(this);
#ifdef _PLUGIN_SUPPORT_
							else if (inline_url_stat != URL_LOADING)
								// Nobody cared about this data so if we're waiting for a plugin
								// to load, we're waiting in vain.
								OpNS4PluginHandler::GetHandler()->Resume(this, element, TRUE, TRUE);
#endif // _PLUGIN_SUPPORT_
					}
					else if (element->IsMatchingType(HE_EMBED, NS_HTML))
					{
						Box* box = element->GetLayoutBox();
						if (!box)
						{
							/* If this happens during reflow (reflowing==TRUE in the condition below,) we're
							   called from LocalLoadInline, called from LayoutProperties::CreateLayoutBox, and
							   at that point we can't call HTML_Element::MarkExtraDirty().  Nor do we need to,
							   since the layout engine is already about to recreate the layout box, which
							   is why we would call HTML_Element::MarkExtraDirty() in the first place. */
							if (!reflowing && (lie->GetUrl()->Status(TRUE) != URL_LOADING || lie->GetUrl()->ContentType() != URL_UNDETERMINED_CONTENT))
							{
								element->MarkExtraDirty(this);
								logdoc->SetNotCompleted();
							}
						}
						else
							if (box->IsContentEmbed())
								box->SignalChange(this);
					}
				}
			}
			break;

		case EMBED_INLINE:
			{
				HTML_Element* he = helm->HElm();

				if (he)
				{
					Box* box = he->GetLayoutBox();

					if (box)
						box->SignalChange(this);

#ifdef _PLUGIN_SUPPORT_
					if (he->GetNS4Plugin())
					{
						if (OpStatus::IsMemoryError(he->GetNS4Plugin()->OnMoreDataAvailable()))
							out_of_memory = TRUE;
						if (lie->GetUrl()->Status(TRUE) != URL_LOADING)
							he->MarkDirty(this);
					}
#endif // _PLUGIN_SUPPORT_
				}
			}

			break;
		}

		helm = inline_data_loaded_helm_next;
	}

	inline_data_loaded_helm_next = NULL;
	if (inline_url_stat != lie->GetUrl()->Status(TRUE))
	{
		inline_url_stat = lie->GetUrl()->Status(TRUE);

		switch (inline_url_stat)
		{
		case URL_UNLOADED: break;
		case URL_LOADED: SetInlineLoading(lie, FALSE); break;
		case URL_LOADING: SetInlineLoading(lie, TRUE); break;
		default: SetInlineStopped(lie); break;
		}
	}

	if (ExternalInlineListener *listener = lie->GetFirstExternalListener())
	{
		const URL &url = *lie->GetUrl();

		do
		{
			ExternalInlineListener *next = listener->Suc();

			if (inline_url_stat == URL_LOADING && !refuse_processing)
				listener->LoadingProgress(this, url);
			else
			{
				ExternalInlineStopped(listener);
				listener->LoadingStopped(this, url);
			}

			listener = next;
		} while (listener);
	}

	lie->SetIsProcessing(FALSE);

	return !out_of_memory;
}

#ifdef _DEBUG
static void
CheckInlineCounting(LoadInlineElmHashTable *inline_hash, const ImageLoadingInfo &image_info, const URL &doc_url)
{
	int actually_loading = 0;
	int actually_loaded = 0;

	LoadInlineElmHashIterator iterator(*inline_hash);
	for (LoadInlineElm* lie = iterator.First(); lie; lie = iterator.Next())
	{
		// Can't both be loading and be loaded.
		OP_ASSERT(!(lie->GetLoading() && lie->GetLoaded()));

		if (lie->GetLoading() && lie->GetDelayLoad())
			actually_loading += 1;
		else if (lie->GetLoaded())
			actually_loaded += 1;
	}

	OP_ASSERT(actually_loaded == image_info.loaded_count);
	OP_ASSERT(actually_loading + actually_loaded == image_info.total_count);
}
#endif // _DEBUG

void
FramesDocument::SetInlineLoading(LoadInlineElm *lie, BOOL value)
{
	// Reset everything.
	SetInlineStopped(lie);

	if (value)
		lie->SetLoading(TRUE);
	else
		lie->SetLoaded(TRUE);

	if (!lie->GetLoading() || lie->GetDelayLoad())
		image_info.total_count += 1;
	image_info.total_size += lie->GetTotalSize();

	if (!value)
	{
		image_info.loaded_count += 1;
		image_info.loaded_size += lie->GetLoadedSize();
#ifdef WEB_TURBO_MODE
		if (doc_manager->GetWindow()->GetTurboMode())
		{
			UINT32 turbo_received, turbo_total;
			lie->GetUrl()->GetAttribute(URL::KTurboTransferredBytes, &turbo_received);
			lie->GetUrl()->GetAttribute(URL::KTurboOriginalTransferredBytes, &turbo_total);
			if (turbo_total > 0)
			{
				image_info.turbo_transferred_bytes += turbo_received;
				image_info.turbo_original_transferred_bytes += turbo_total;
			}
		}
#endif // WEB_TURBO_MODE
	}

#ifdef _DEBUG
	CheckInlineCounting(&inline_hash, image_info, url);
#endif // _DEBUG
}

void
FramesDocument::SetInlineStopped(LoadInlineElm *lie)
{
#ifdef _DEBUG
	CheckInlineCounting(&inline_hash, image_info, url);
#endif // _DEBUG

	if (lie->GetLoading() || lie->GetLoaded())
	{
		if (!lie->GetLoading() || lie->GetDelayLoad())
			image_info.total_count -= 1;
		image_info.total_size -= lie->GetLastTotalSize();
	}

	if (lie->GetLoaded())
	{
		image_info.loaded_count -= 1;
		image_info.loaded_size -= lie->GetLastLoadedSize();
	}

	lie->SetLoading(FALSE);
	lie->SetLoaded(FALSE);

#ifdef _DEBUG
	CheckInlineCounting(&inline_hash, image_info, url);
#endif // _DEBUG
}

void
FramesDocument::UpdateInlineInfo(LoadInlineElm *lie)
{
	if (lie->GetLoading())
	{
		image_info.total_size -= lie->GetLastTotalSize();
		image_info.total_size += lie->GetTotalSize();
		image_info.loaded_size -= lie->GetLastLoadedSize();
		image_info.loaded_size += lie->GetLoadedSize();
#ifdef WEB_TURBO_MODE
		if (doc_manager->GetWindow()->GetTurboMode())
		{
			UINT32 turbo_received, turbo_total;
			lie->GetUrl()->GetAttribute(URL::KTurboTransferredBytes, &turbo_received);
			lie->GetUrl()->GetAttribute(URL::KTurboOriginalTransferredBytes, &turbo_total);
			if (turbo_total > 0)
			{
				image_info.turbo_transferred_bytes += turbo_received;
				image_info.turbo_original_transferred_bytes += turbo_total;
			}
		}
#endif // WEB_TURBO_MODE
	}
}

void
FramesDocument::RemoveLoadInlineElm(LoadInlineElm *lie)
{
	SetInlineStopped(lie);
	if (waiting_for_online_mode.HasLink(lie))
	{
		lie->Out();
	}
	else
	{
#ifdef _DEBUG
		OP_STATUS status = inline_hash.Remove(lie);
		OP_ASSERT(status == OpStatus::OK);
#else // _DEBUG
		OpStatus::Ignore(inline_hash.Remove(lie));
#endif // _DEBUG
	}
}

void
FramesDocument::RemoveAllLoadInlineElms()
{
#ifdef _DEBUG
	CheckInlineCounting(&inline_hash, image_info, url);
#endif // _DEBUG

	inline_hash.DeleteAll();
	waiting_for_online_mode.Clear();

	image_info.Reset();
}

void FramesDocument::StopLoadingInline(LoadInlineElm* lie)
{
#ifdef _DEBUG
	CheckInlineCounting(&inline_hash, image_info, GetURL());
#endif // _DEBUG

	if (!lie->IsUnused())
		return;

	URL* url = lie->GetUrl();
	URL_ID url_id = url->Id(TRUE);

	url->StopLoading(GetMessageHandler());
	UnsetLoadingCallbacks(url_id);

	if (!lie->GetIsProcessing())
	{
		if (!lie->GetLoaded())
		{
			RemoveLoadInlineElm(lie);
			OP_DELETE(lie);
		}
		else
			LimitUnusedInlines();
	}
	else
		SetInlineStopped(lie);

	ReflowAndFinishIfLastInlineLoaded();
	SendImageLoadingInfo();
#ifdef SHORTCUT_ICON_SUPPORT
	AbortFaviconIfLastInline();
#endif

#ifdef _DEBUG
	CheckInlineCounting(&inline_hash, image_info, GetURL());
#endif // _DEBUG
}

BOOL FramesDocument::CheckExternalImagePreference(InlineResourceType inline_type, const URL& image_url)
{
	if ((HEListElm::IsImageInline(inline_type) || inline_type == ICON_INLINE) &&
		GetHandheldEnabled() &&
		!g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ExternalImage, GetHostName()))
	{
		ServerName *doc_sn = GetURL().GetServerName();
		ServerName *img_sn = image_url.GetServerName();
		if (img_sn && doc_sn != img_sn)
			return FALSE;
	}
	return TRUE;
}

OP_STATUS FramesDocument::ReflowAndFinishIfLastInlineLoaded()
{
	OP_STATUS status = OpStatus::OK;

	if (logdoc && !undisplaying)
	{
		if (logdoc->IsParsed() && InlineImagesLoaded())
			PostReflowMsg(TRUE);

		status = CheckFinishDocument();
	}

	return status;
}

#ifdef SHORTCUT_ICON_SUPPORT
void FramesDocument::AbortFaviconIfLastInline()
{
	// If only one inline element left and it is a favicon - abort it.
	if (logdoc->IsLoaded() && image_info.total_count - image_info.loaded_count == 1)
	{
		LoadInlineElmHashIterator iterator(inline_hash);
		for (LoadInlineElm* lie = iterator.First(); lie; lie = iterator.Next())
		{
			if (lie->GetLoading() && lie->HasOnlyInlineType(ICON_INLINE))
			{
				GetMessageHandler()->PostDelayedMessage(MSG_FAVICON_TIMEOUT, (MH_PARAM_1) lie->GetUrl()->Id(), ICON_INLINE, 3000);
				break;
			}
		}
	}
}
#endif // SHORTCUT_ICON_SUPPORT

void FramesDocument::StopLoadingInline(URL* url, HTML_Element* element, InlineResourceType inline_type, BOOL keep_dangling_helm /* = FALSE */)
{
	LoadInlineElm* lie = GetInline(url->Id(TRUE));

	if (lie)
	{
		for (HEListElm *helm = lie->GetFirstHEListElm(); helm; helm = helm->Suc())
			if (helm->HElm() == element && helm->GetInlineType() == inline_type)
			{
				if (inline_data_loaded_helm_next == helm)
					inline_data_loaded_helm_next = inline_data_loaded_helm_next->Suc();

				lie->Remove(helm);
				// Keep it alive just to keep old_url available or
				// we'll get flicker if the image is being replaced by
				// something else. Just need to check that the helm is
				// owned by the element first
				if (!helm->GetElm() || !keep_dangling_helm)
					OP_DELETE(helm);

				break;
			}

		StopLoadingInline(lie);
	}
}

OP_LOAD_INLINE_STATUS FramesDocument::LoadInline(const URL &url, ExternalInlineListener *listener, BOOL reload_conditionally /*=FALSE*/, BOOL block_paint_until_loaded /*=FALSE*/, InlineResourceType inline_type /*=GENERIC_INLINE*/)
{
	OP_ASSERT(!listener->InList());

	if (!logdoc || !GetDocRoot())
		return LoadInlineStatus::LOADING_CANCELLED;

	URL local_url = url, moved_to = local_url.GetAttribute(URL::KMovedToURL, TRUE);

	if (!moved_to.IsEmpty())
	{
		List<ExternalInlineListener> temporary;

		listener->Into(&temporary);
		listener->LoadingRedirected(this, local_url, moved_to);

		if (temporary.Empty())
			return LoadInlineStatus::LOADING_CANCELLED;
		else
			listener->Out();

		local_url = moved_to;
	}

	// Hack to get a conditional reload of the inline off the ground.
	DocumentManager *docman = GetDocManager();
	BOOL reload_doc = docman->GetReloadDocument();
	BOOL reload_doc_cond = docman->GetConditionallyRequestDocument();
	BOOL reload_inlines = docman->GetReloadInlines();
	BOOL reload_inlines_cond = docman->GetConditionallyRequestInlines();

	docman->SetReloadFlags(reload_doc, reload_doc_cond, reload_conditionally, reload_inlines_cond);

	OP_LOAD_INLINE_STATUS status = LoadInline(&local_url, GetDocRoot(), inline_type, LoadInlineOptions().ReloadConditionally(reload_conditionally));

	// Resetting the reload flags
	docman->SetReloadFlags(reload_doc, reload_doc_cond, reload_inlines, reload_inlines_cond);

	if (status == LoadInlineStatus::USE_LOADED)
		listener->LoadingStopped(this, local_url);
	else if (status == LoadInlineStatus::LOADING_STARTED)
	{
		RETURN_IF_ERROR(GetInline(local_url.Id(FALSE), moved_to.IsEmpty())->AddExternalListener(listener));
		if (block_paint_until_loaded && !listener->has_inc_wait_for_paint_blockers)
		{
			if (!reflowing && CheckWaitForStylesTimeout())
			{
				listener->has_inc_wait_for_paint_blockers = TRUE;
				wait_for_paint_blockers++;
			}
		}
	}

	return status;
}

int FramesDocument::GetInlinePriority(int inline_type, URL *inline_url)
{
	ServerName *inline_sn = inline_url->GetServerName(URL::KFollowRedirect);

	BOOL same_host_as_document = inline_sn == NULL || GetURL().GetServerName() == inline_sn;

	int priority = URL_NORMAL_PRIORITY;

	switch (inline_type)
	{
	case ICON_INLINE:
		priority = URL_LOWEST_PRIORITY;
		break;

	case IMAGE_INLINE:
	case VIDEO_POSTER_INLINE:
		// When images and plugins are from the same host as main page give them a little higher priority
		// Images and plugins from other hosts most likely are ads
		if (same_host_as_document)
		{
			priority = ((!GetParentDoc())) ? LOAD_PRIORITY_MAIN_DOCUMENT_INLINE_IMAGE_INTERNAL :
				LOAD_PRIORITY_FRAME_DOCUMENT_INLINE_IMAGE_INTERNAL;
		}
		else
		{
			priority = ((!GetParentDoc())) ? LOAD_PRIORITY_MAIN_DOCUMENT_INLINE_IMAGE_EXTERNAL :
				LOAD_PRIORITY_FRAME_DOCUMENT_INLINE_IMAGE_EXTERNAL;
		}
		break;

		// More important to display CSSed page
	case BORDER_IMAGE_INLINE:
	case BGIMAGE_INLINE:
	case EXTRA_BGIMAGE_INLINE:
	case WEBFONT_INLINE:
		priority = ((!GetParentDoc())) ? LOAD_PRIORITY_MAIN_DOCUMENT_INLINE_REQUIRED_BY_CSS :
			LOAD_PRIORITY_FRAME_DOCUMENT_INLINE_REQUIRED_BY_CSS;
		break;

		// When DSE is disabled scripts should be the most important as they block further parsing of a document
		// But if DSE is enabled scripts should be less important than CSS
	case SCRIPT_INLINE:
#ifdef DELAYED_SCRIPT_EXECUTION
		if (g_pcjs->GetIntegerPref(PrefsCollectionJS::DelayedScriptExecution, GetURL()))
			priority = ((!GetParentDoc())) ? LOAD_PRIORITY_MAIN_DOCUMENT_SCRIPT_INLINE_DSE :
			LOAD_PRIORITY_FRAME_DOCUMENT_SCRIPT_INLINE_DSE;
		else
#endif // DELAYED_SCRIPT_EXECUTION
			priority = ((!GetParentDoc())) ? LOAD_PRIORITY_MAIN_DOCUMENT_SCRIPT_INLINE_NO_DSE :
			LOAD_PRIORITY_FRAME_DOCUMENT_SCRIPT_INLINE_NO_DSE;
		break;

	case CSS_INLINE:
		priority = ((!GetParentDoc())) ? LOAD_PRIORITY_MAIN_DOCUMENT_CSS_INLINE :
			LOAD_PRIORITY_FRAME_DOCUMENT_CSS_INLINE;
		break;

		// plugins/track - less important than others
	case GENERIC_INLINE:
	case TRACK_INLINE:
	case EMBED_INLINE:
		if (same_host_as_document)
			priority = ((!GetParentDoc())) ? LOAD_PRIORITY_MAIN_DOCUMENT_INLINE_PLUGIN_INTERNAL :
			LOAD_PRIORITY_FRAME_DOCUMENT_INLINE_PLUGIN_INTERNAL;
		else
			priority = ((!GetParentDoc())) ? LOAD_PRIORITY_MAIN_DOCUMENT_INLINE_PLUGIN_EXTERNAL :
			LOAD_PRIORITY_FRAME_DOCUMENT_INLINE_PLUGIN_EXTERNAL;
		break;

	default:
		OP_ASSERT(!"All inline types should be given the priority here explicitly");
	}

	return priority;
}

void FramesDocument::StopLoadingInline(const URL &url, ExternalInlineListener *listener)
{
	URL local_url = url;

	Head* list;
	if (OpStatus::IsSuccess(inline_hash.GetData(local_url.Id(TRUE), &list)))
	{
		for (LoadInlineElm *lie = (LoadInlineElm*)list->First(); lie; lie = (LoadInlineElm*)lie->Suc())
			if (lie->RemoveExternalListener(listener))
			{
				for (HEListElm *helm = lie->GetFirstHEListElm(); helm; helm = helm->Suc())
					if (helm->HElm() == GetDocRoot() && helm->GetInlineType() == GENERIC_INLINE)
					{
						lie->Remove(helm);
						OP_DELETE(helm);
						break;
					}

				StopLoadingInline(lie);
				ExternalInlineStopped(listener);
				return;
			}
	}

	if (listener->InList())
	{
		ExternalInlineStopped(listener);
		listener->Out();
	}
}

OP_STATUS FramesDocument::CleanInline(HTML_Element* html_elm)
{
	html_elm->RemoveCSS(HTML_Element::DocumentContext(this));
	html_elm->EmptySrcListAttr();

	URL * css_url = html_elm->GetLinkURL(logdoc);
	if (css_url)
	{
		int css_url_id = css_url->Id(TRUE);
		LoadInlineElm* lie = GetInline(css_url_id);
		if(lie)
			if (lie->RemoveHEListElm(html_elm, CSS_INLINE))
			{
				/* RemoveHEListElm() may delete HEListElm object associated with html_elm and if the deleted HEListElm is the only one belonging to
				   LoadInlineElm LoadInlineElm itself may be deleted as well leaving us with the stale pointer.
				   Refetch the pointer to avoid such scenario. */
				lie = GetInline(css_url_id);
				if (lie && lie->IsUnused())
				{
					lie->SetUsed(FALSE);

					/* Move to the end of the list.  Unused inlines are flushed from
					   start to end and, since we move inlines to the end when they
					   first become unused, in the order they became unused. */
					if (inline_hash.Contains(lie))
						inline_hash.MoveLast(lie);
				}
			}
	}

	LimitUnusedInlines();

	return OpStatus::OK;
}

OP_STATUS FramesDocument::ResetInlineRedirection(LoadInlineElm *lie, URL *inline_url)
{
	URL_ID redirected_url_id = lie->GetRedirectedUrl()->Id();

	lie->ResetRedirection();

	return inline_hash.UrlMoved(redirected_url_id, inline_url->Id(FALSE));
}

OP_LOAD_INLINE_STATUS FramesDocument::LocalLoadInline(URL *img_url, InlineResourceType inline_type, HEListElm *helm, HTML_Element *html_elm, const LoadInlineOptions &options)
{
	OP_ASSERT(html_elm);
	OP_ASSERT(!helm || (helm->HElm() == html_elm && helm->GetInlineType() == inline_type));
	OP_ASSERT(!html_elm->IsText());

	// do not attempt to start loading empty urls
	if (!img_url || img_url->IsEmpty())
		return LoadInlineStatus::LOADING_CANCELLED;

	const BOOL reload = options.reload && !loading_aborted;

	URLType img_url_type   = img_url->Type();
	URLStatus img_url_stat = img_url->Status(TRUE);

	if (loading_aborted && !options.force_image_load && img_url_stat != URL_LOADED)
		return LoadInlineStatus::LOADING_CANCELLED;

#if defined(APPLICATION_CACHE_SUPPORT) && defined(DEBUG)
	if (g_application_cache_manager->GetApplicationCacheFromCacheHost(GetDOMEnvironment()))
	{
		// Inlines should have same context.  If it doesn't then figure out
		// where the inline was constructed, and why it has a different context
		// (was it constructed before document changed context and never updated then,
		// or was it not resolved relative to document or base URL?)
		OP_ASSERT(img_url->GetContextId() == GetURL().GetContextId());
	}
#endif // APPLICATION_CACHE_SUPPORT

	BOOL allow_inline_url;
	OpSecurityState sec_state;
	RETURN_IF_ERROR(g_secman_instance->CheckSecurity(OpSecurityManager::DOC_INLINE_LOAD,
													 OpSecurityContext(this),
													 OpSecurityContext(*img_url, inline_type, options.from_user_css),
													 allow_inline_url,
													 sec_state));
	if (allow_inline_url == FALSE)
	{
		if (!sec_state.suspended)
		{
			return LoadInlineStatus::LOADING_REFUSED;
		}
	}

	if (!CheckExternalImagePreference(inline_type, *img_url))
		return LoadInlineStatus::LOADING_REFUSED;

	if (inline_type == EMBED_INLINE)
	{
#ifdef _PLUGIN_SUPPORT_
		BOOL is_applet = html_elm->IsMatchingType(HE_APPLET, NS_HTML);
		const uni_char* type_attribute = html_elm->GetEMBED_Type();
		if (is_applet || type_attribute)
		{
			const uni_char* mime_type = is_applet ? UNI_L("application/x-java-applet") : type_attribute;

			ViewActionReply reply;
			RETURN_IF_MEMORY_ERROR(Viewers::GetViewAction(*img_url, mime_type, reply, TRUE));

			if (reply.action != VIEWER_OPERA)
			{
				if (reply.app.IsEmpty())
					RETURN_IF_MEMORY_ERROR(Viewers::GetViewAction(*img_url, UNI_L("*"), reply, TRUE));

#ifdef PLUGIN_AUTO_INSTALL
				if (reply.mime_type.IsEmpty())
#else // PLUGIN_AUTO_INSTALL
				if (!reply.app.CStr() || reply.mime_type.IsEmpty())
#endif // PLUGIN_AUTO_INSTALL
					return LoadInlineStatus::LOADING_CANCELLED;
			}
		}
#else // _PLUGIN_SUPPORT_
#ifndef SVG_SUPPORT // SVG must load embeds to see if they are SVGs
		return LoadInlineStatus::LOADING_CANCELLED;
#endif // !SVG_SUPPORT
#endif // _PLUGIN_SUPPORT_
	}

	OP_LOAD_INLINE_STATUS loading = LoadInlineStatus::USE_LOADED;
	OpStatus::Ignore(loading); // To silence DEBUGGING_OP_STATUS when loading is not used.

	Window* window = GetWindow();

	BOOL load_this_inline = FALSE;
	URL_ID img_url_id = img_url->Id();
	LoadInlineElm* lie = GetInline(img_url_id, FALSE);

	BOOL is_loading_blocked_by_user = (HEListElm::IsImageInline(inline_type) || inline_type == ICON_INLINE) &&
		!options.force_image_load && !window->LoadImages() && !(img_url_type == URL_FILE && window->ShowImages());
#ifdef FLEXIBLE_IMAGE_POLICY
	is_loading_blocked_by_user = is_loading_blocked_by_user && !(url.IsImage() && IsTopDocument());
#endif // FLEXIBLE_IMAGE_POLICY

	if (!lie)
	{
		lie = OP_NEW(LoadInlineElm, (this, *img_url));

		if (!lie)
			return OpStatus::ERR_NO_MEMORY;

#ifdef SPECULATIVE_PARSER
		lie->SetIsSpeculativeLoad(options.speculative_load);
#endif // SPECULATIVE_PARSER

		if (HEListElm::IsImageInline(inline_type))
		{
			load_this_inline = !is_loading_blocked_by_user;

#ifdef CORS_SUPPORT
			if (html_elm->HasAttr(Markup::HA_CROSSORIGIN))
				RETURN_IF_MEMORY_ERROR(lie->SetCrossOriginRequest(GetURL(), img_url, html_elm));
#endif // CORS_SUPPORT

			if (load_this_inline)
			{
				PrefsCollectionDisplay::RenderingModes rendering_mode = GetPrefsRenderingMode();

				if (g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::ShowImages), GetHostName()) == 1)
				{
					int images = 0;

					/* There exists no accurate image count (image_info.total_count
					   inconveniently includes scripts/stylesheets/etc), so we have
					   to count them here. */

					LoadInlineElmHashIterator iterator(inline_hash);
					for (LoadInlineElm* lie = iterator.First(); lie; lie = iterator.Next())
						if ((lie->GetLoaded() || lie->GetLoading()) && lie->HasInlineType(IMAGE_INLINE))
							if (++images > 4)
							{
								load_this_inline = FALSE;
								break;
							}
				}
			}
		}
		else if (inline_type == ICON_INLINE)
		{
			load_this_inline = !is_loading_blocked_by_user;
		}
#if defined(_HTTP_COMPRESS) && defined(NS4P_DISABLE_HTTP_COMPRESS)
		else if (inline_type == EMBED_INLINE)
		{
			img_url->SetAttribute(URL::KDisableCompress, TRUE);
			load_this_inline = TRUE;
		}
#endif // _HTTP_COMPRESS && NS4P_DISABLE_HTTP_COMPRESS
		else if (inline_type == GENERIC_INLINE)
		{
			URLContentType cnt_type = img_url->ContentType();
			if (cnt_type == URL_UNDETERMINED_CONTENT)
			{
				const uni_char* type_str = html_elm->GetStringAttr(ATTR_TYPE);
				if (type_str)
					if (Viewer* v = g_viewers->FindViewerByMimeType(type_str))
					{
						cnt_type = v->GetContentType();

#if defined(_HTTP_COMPRESS) && defined(NS4P_DISABLE_HTTP_COMPRESS)
						if (v->GetAction() == VIEWER_PLUGIN)
							img_url->SetAttribute(URL::KDisableCompress,TRUE);
#endif // _HTTP_COMPRESS && NS4P_DISABLE_HTTP_COMPRESS
					}
			}

			if (cnt_type == URL_PNG_CONTENT
				|| cnt_type == URL_GIF_CONTENT
				|| cnt_type == URL_JPG_CONTENT
				|| cnt_type == URL_XBM_CONTENT
#ifdef HAS_ATVEF_SUPPORT // TV: works with object
				|| cnt_type == URL_TV_CONTENT
#endif // HAS_ATVEF_SUPPORT
				|| cnt_type == URL_WEBP_CONTENT
#ifdef _WML_SUPPORT_
				|| cnt_type == URL_WBMP_CONTENT
#endif
				|| cnt_type == URL_BMP_CONTENT
				)
			{
				load_this_inline = window->LoadImages() || (img_url_type == URL_FILE && window->ShowImages());
			}
			else
				load_this_inline = TRUE;
		}
#ifdef MEDIA_HTML_SUPPORT
		else if (inline_type == TRACK_INLINE)
		{
			/* For <track>, the mode for CORS is to be fetched from
			   the corresponding media (<video> or <audio>) element.
			   Fail if there is no corresponding element. */
			HTML_Element* media_elm = html_elm->ParentActual();
			if (media_elm &&
				(media_elm->IsMatchingType(Markup::HTE_VIDEO, NS_HTML) ||
				 media_elm->IsMatchingType(Markup::HTE_AUDIO, NS_HTML)))
			{
				if (img_url->Type() == URL_DATA)
					load_this_inline = TRUE;
				else
				{
					BOOL same_origin;
					RETURN_IF_MEMORY_ERROR(OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_STANDARD,
																			OpSecurityContext(this),
																			OpSecurityContext(*img_url),
																			same_origin));
					if (same_origin)
						load_this_inline = TRUE;
#ifdef CORS_SUPPORT
					else if (media_elm->HasAttr(Markup::HA_CROSSORIGIN))
					{
						RETURN_IF_MEMORY_ERROR(lie->SetCrossOriginRequest(GetURL(), img_url, media_elm));
						load_this_inline = TRUE;
					}
#endif // CORS_SUPPORT
				}
			}
		}
#endif // MEDIA_HTML_SUPPORT
		else
		{
			load_this_inline = TRUE;
#ifdef CORS_SUPPORT
			if (inline_type == SCRIPT_INLINE && html_elm->HasAttr(Markup::HA_CROSSORIGIN))
				RETURN_IF_MEMORY_ERROR(lie->SetCrossOriginRequest(GetURL(), img_url, html_elm));
#endif // CORS_SUPPORT
		}

		OP_STATUS status = inline_hash.Add(lie);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(lie);
			return status;
		}
	}
	else
	{
		if (img_url_stat == URL_LOADING_ABORTED || img_url_stat == URL_UNLOADED || lie->GetLoaded() && img_url_stat != URL_LOADED)
		{
			/* It has been thrown out of the cache or was never loaded correctly
			   last time we loaded it, or is currently being loaded by someone
			   else.  It can be thrown out of cache despite having a
			   LoadInlineElm if it's a file url for instance. */
			load_this_inline = !is_loading_blocked_by_user;
		}
		else if (!lie->GetLoaded() && !lie->GetLoading() && img_url_stat == URL_LOADED)
		{
			if (!lie->HasSentHandleInlineDataAsyncMsg())
			{
				SetLoadingCallbacks(img_url->Id(TRUE));
				GetMessageHandler()->PostMessage(MSG_HANDLE_INLINE_DATA_ASYNC, img_url->Id(TRUE), 0);
				lie->SetSentHandleInlineDataAsyncMsg(TRUE);
			}
			SetInlineLoading(lie, TRUE);
		}
	}

	if (reload)
		load_this_inline = TRUE;

	BOOL forced_refetch = FALSE;
	// always check if script should be refetched - see CORE-24662
	if (!load_this_inline && !helm && inline_type == SCRIPT_INLINE)
	{
		/* If we're loading a script, has it already loaded or know that loading failed
		   (very commn with DSE) don't force re-fetching of it as we'll get NSL.
		 */
		if (lie->IsElementAdded(html_elm, inline_type))
		{
			if (lie->GetLoading())
				return LoadInlineStatus::LOADING_STARTED;
			else if (img_url_stat == URL_LOADING_FAILURE)
				return LoadInlineStatus::LOADING_CANCELLED;
			else
				return LoadInlineStatus::USE_LOADED;
		}

		// Otherwise force re-fetching it (GetInlineLoadPolicy() will drive what type of load will be used).
		load_this_inline = TRUE;
		forced_refetch = TRUE;
	}

	/* We have to return *after* creating a LoadInlineElm because LoadInline(const URL &, ExternalInlineListener *, BOOL) assumes
	 * the LoadInlineElm exists when LOADING_STARTED is returned.
	 */
	if (allow_inline_url == FALSE && sec_state.suspended)
	{
		if (load_this_inline)
		{
			List<AsyncLoadInlineElm> *async_inlines;
			BOOL new_list = FALSE;

			if (OpStatus::IsError(async_inline_elms.GetData(html_elm, &async_inlines)))
			{
				RETURN_OOM_IF_NULL(async_inlines = OP_NEW(List<AsyncLoadInlineElm>, ()));
				new_list = TRUE;
			}

			AsyncLoadInlineElm *async_inline;

			// If we already have one waiting for the same element/inline_type combination, get rid of it.
			for (async_inline = async_inlines->First(); async_inline; async_inline = async_inline->Suc())
				if (async_inline->GetInlineType() == inline_type)
				{
					async_inline->Out();
					OP_DELETE(async_inline);
					break;
				}

			async_inline = OP_NEW(AsyncLoadInlineElm, (this, *img_url, inline_type, html_elm, options, this));

			if (!async_inline || (new_list && OpStatus::IsError(async_inline_elms.Add(html_elm, async_inlines))))
			{
				if (new_list)
					OP_DELETE(async_inlines);
				return OpStatus::ERR_NO_MEMORY;
			}

			async_inline->Into(async_inlines);
			async_inline->SetSecurityState(sec_state);
			async_inline->LoadInline();

			return LoadInlineStatus::LOADING_STARTED;
		}
	}

	if (!helm)
	{
		if (HEListElm::IsImageInline(inline_type))
		{
			if (inline_type == EXTRA_BGIMAGE_INLINE)
			{
				/* Find an inactive HEListElm */
				HTML_Element::BgImageIterator iter(html_elm);
				HEListElm *iter_elm = iter.Next();
				while (iter_elm)
				{
					if (!iter_elm->IsActive() && iter_elm->GetInlineType() == EXTRA_BGIMAGE_INLINE)
					{
						helm = iter_elm;
						helm->SetActive(TRUE);
						break;
					}

					iter_elm = iter.Next();
				}
			}
			else
				helm = html_elm->GetHEListElmForInline(inline_type);

			if (helm)
			{
				if (!helm->IsInList(lie))
				{
#ifdef _PRINT_SUPPORT_
					HLDocProfile* hld_profile = GetHLDocProfile();
					// If this is a print document and the HEListElm is on the print images list
					// remove it from there since it's going to be loaded (will be put on LoadInlineElm's list).
					if (IsPrintDocument() && hld_profile)
						hld_profile->RemovePrintImageElement(helm); // It checks if the element belongs to the print images list.
#endif // _PRINT_SUPPORT_

					// This element used another resource for this role and it's
					// in previous_lie, or if previous_lie is NULL, |helm| was
					// just kept alive for the old url.
					LoadInlineElm *previous_lie = helm->GetLoadInlineElm();

					helm->Undisplay();
					if (previous_lie)
						previous_lie->Remove(helm);

					if (load_this_inline)
					{
						// Store the old url so that it can be used
						// while we load the new image
						UrlImageContentProvider *content_provider = helm->GetUrlContentProvider();
						if (content_provider)
							helm->SetOldUrl(*content_provider->GetUrl());
					}
					else
					{
						// Use the new image url immediately. Not
						// perfect since we should wait for it to be
						// decoded as well (see CORE-24164).
						helm->UpdateImageUrl(img_url);
					}

					if (lie->IsUnused())
					{
						if (img_url->Status(TRUE) == URL_UNLOADED && !UrlImageContentProvider::GetImageFromUrl(*img_url).ImageDecoded())
							load_this_inline = TRUE;

						lie->SetUsed(TRUE);
					}

					helm->SetDelayLoad(options.delay_load);
					helm->SetFromUserCSS(options.from_user_css);

					lie->Add(helm);

					if (previous_lie && previous_lie->IsUnused())
					{
						previous_lie->SetUsed(FALSE);

						/* Move to the end of the list.  Unused inlines are flushed from
						   start to end and, since we move inlines to the end when they
						   first become unused, in the order they became unused. */
						if (inline_hash.Contains(previous_lie))
							inline_hash.MoveLast(previous_lie);

						LimitUnusedInlines();
					}
				}

				if (!load_this_inline)
					if (lie->IsElementAdded(html_elm, inline_type))
						if (lie->GetLoading())
						{
							ForceImageNodeIfNeeded(inline_type, html_elm);
							return LoadInlineStatus::LOADING_STARTED;
						}
						else if (img_url_stat == URL_LOADING_FAILURE)
							return LoadInlineStatus::LOADING_CANCELLED;
						else
							return LoadInlineStatus::USE_LOADED;
					else
						return LoadInlineStatus::LOADING_CANCELLED;
			}
		}
		else
			if (!load_this_inline && lie->IsElementAdded(html_elm, inline_type))
			{
				if (lie->GetLoading())
					return LoadInlineStatus::LOADING_STARTED;
				else if (img_url_stat == URL_LOADING_FAILURE)
					return LoadInlineStatus::LOADING_CANCELLED;
				else
					return LoadInlineStatus::USE_LOADED;
			}
	}

	MessageHandler* mh = GetMessageHandler();

	if (!helm)
	{
		helm = lie->AddHEListElm(html_elm, inline_type, this, img_url, lie->GetLoaded() || !load_this_inline);

		if (!helm)
		{
			if (lie->IsUnused())
			{
				RemoveLoadInlineElm(lie);
				OP_DELETE(lie);
			}
			return OpStatus::ERR_NO_MEMORY;
		}

		helm->SetDelayLoad(options.delay_load);
		helm->SetFromUserCSS(options.from_user_css);

		if ((lie->GetLoaded() || !load_this_inline) && img_url_stat != URL_LOADING && !forced_refetch /* if forced_refetch is TRUE we're loading a script which we want to be refetched => proceed with loading. */)
		{
			URL_ID lie_listening_id = lie->GetRedirectedUrl()->Id(); // Could there be a MSG_URL_MOVED queued already that will change what this lie listens to?
			if (!SetLoadingCallbacks(lie_listening_id))
				return OpStatus::ERR_NO_MEMORY;

			if (!lie->HasSentHandleInlineDataAsyncMsg())
			{
				mh->PostMessage(MSG_HANDLE_INLINE_DATA_ASYNC, lie_listening_id, 0);
				lie->SetSentHandleInlineDataAsyncMsg(TRUE);
			}
		}

		if (!load_this_inline)
		{
			if (lie->GetLoaded() || lie->GetLoading())
			{
				// We will have sent a MSG_HANDLE_INLINE_DATA_ASYNC or there will
				// be a future MSG_URL_DATA_LOADED (or equivalent) so we'll tell
				// the caller that the load has started so the caller
				// knows what will come

				if (inline_type == CSS_INLINE)
					IncWaitForStyles();

				if (lie->GetLoading())
					ForceImageNodeIfNeeded(inline_type, html_elm);

				return LoadInlineStatus::LOADING_STARTED;
			}

			BOOL must_reload = img_url->Status(TRUE) == URL_LOADING_FAILURE
				&& img_url_stat != URL_LOADING_FAILURE;
			if (!must_reload)
			{
				lie->SetNeedSecurityUpdate(TRUE);
				return LoadInlineStatus::LOADING_CANCELLED;
			}
		}
	}

#ifdef _DEBUG
	/* Debug code to check that assumptions made in FlushInlineElement are correct. */

	BOOL dbg_is_image = html_elm->GetHEListElm(FALSE) == helm;
	BOOL dbg_is_bgimage = FALSE;
	HTML_Element::BgImageIterator iter(html_elm);
	HEListElm *iter_elm = iter.Next();
	while (iter_elm)
	{
		if (iter_elm == helm)
		{
			dbg_is_bgimage = TRUE;
			break;
		}
		iter_elm = iter.Next();
	}
	BOOL dbg_is_borderimage = html_elm->GetHEListElmForInline(BORDER_IMAGE_INLINE) == helm;
	BOOL dbg_is_videoposter = html_elm->GetHEListElmForInline(VIDEO_POSTER_INLINE) == helm;
	BOOL dbg_is_track = html_elm->GetHEListElmForInline(TRACK_INLINE) == helm;
	BOOL dbg_is_script = html_elm->GetHEListElmForInline(SCRIPT_INLINE) == helm;
	BOOL dbg_is_generic = html_elm->GetHEListElmForInline(GENERIC_INLINE) == helm;
	BOOL dbg_is_other = html_elm->Type() == HE_DOC_ROOT || html_elm->GetNsType() == NS_HTML && (html_elm->Type() == HE_APPLET || html_elm->Type() == HE_EMBED || html_elm->Type() == HE_OBJECT || html_elm->Type() == HE_LINK || html_elm->Type() == HE_PROCINST);
	BOOL dbg_is_correct = dbg_is_image || dbg_is_bgimage || dbg_is_script || dbg_is_other || dbg_is_generic || dbg_is_borderimage || dbg_is_videoposter || dbg_is_track;

#ifdef XML_EVENTS_SUPPORT
	BOOL dbg_is_xmlevents = html_elm->GetXMLEventsRegistration() != NULL;
	dbg_is_correct = dbg_is_correct || dbg_is_xmlevents;
#endif // XML_EVENTS_SUPPORT

	/* Assumptions are not correct.  This inline might not be flushed by FlushInlineElement. */
	OP_ASSERT(dbg_is_correct);
#endif // _DEBUG


	loading = LoadInlineStatus::LOADING_STARTED;

	if (!SetLoadingCallbacks(img_url_id))
		return OpStatus::ERR_NO_MEMORY;

	BOOL was_loading = !InlineImagesLoaded();
	URL ref_url = GetURL();
	CommState load_stat = COMM_REQUEST_FAILED;

	if (helm)
		helm->SetImageLastDecodedHeight(0);

	if (*img_url == GetURL()
		&& (!html_elm || html_elm->GetInserted() != HE_INSERTED_BY_DOM)
		&& (img_url_stat == URL_LOADING || img_url_stat == URL_LOADED)
		&& !(reload && GetDocManager()->GetReloadInlines()))
		load_stat = COMM_LOADING;
	else
	{
		BOOL was_redirected = !(*lie->GetRedirectedUrl() == *img_url);
		URL_LoadPolicy loadpolicy = GetInlineLoadPolicy(*img_url, reload);

		int priority = options.force_priority;
		if (priority == URL_LOWEST_PRIORITY)
			priority = GetInlinePriority(inline_type, img_url);
		img_url->SetAttribute(URL::KHTTP_Priority, priority);

#ifdef HTTP_CONTENT_USAGE_INDICATION
		HTTP_ContentUsageIndication usage;
		switch (inline_type)
		{
		case SCRIPT_INLINE:
			usage = HTTP_UsageIndication_Script;
			break;
		case CSS_INLINE:
			usage = HTTP_UsageIndication_Style;
			break;
		case BGIMAGE_INLINE:
			usage = HTTP_UsageIndication_BGImage;
			break;
		default:
			usage = HTTP_UsageIndication_OtherInline;
		}
		img_url->SetAttribute(URL::KHTTP_ContentUsageIndication,usage);
#endif // HTTP_CONTENT_USAGE_INDICATION

		// This should not happen. We used to bail earlier
		// (allow_inline_url == FALSE && sec_state.suspended) if
		// load_this_inline was FALSE (eg because the inline was
		// already loaded), but this caused CORE-45395. If it does
		// happen it is likely that someone changed load_this_inline
		// to TRUE after the check.
		if (allow_inline_url == FALSE)
		{
			OP_ASSERT(!"Reached LoadDocument for a URL that should not be loaded.");
			return LoadInlineStatus::LOADING_REFUSED;
		}

#ifdef URL_FILTER
		URL doc_url = GetURL();
		ServerName *doc_sn = doc_url.GetServerName();
		URLFilterDOMListenerOverride lsn_over(dom_environment, html_elm);
		HTMLLoadContext ctx(HTMLLoadContext::ConvertElement(html_elm), doc_sn, &lsn_over, GetWindow()->GetType() == WIN_TYPE_GADGET);

		load_stat = img_url->LoadDocument(mh, ref_url, loadpolicy, &ctx);
#else
		load_stat = img_url->LoadDocument(mh, ref_url, loadpolicy, NULL);
#endif

		GetWindow()->SetURLAlreadyRequested(*img_url);

		// If we start loading the inline again here, revert the previous
		// redirection if the old URL is redirected, to avoid loading messages
		// not being handled if the refetch will not be redirected, causing the
		// wrong url id to be used when getting the entries from the
		// inline_hash. See CORE-44843.
		if (load_stat == COMM_LOADING && was_redirected)
			RETURN_IF_MEMORY_ERROR(ResetInlineRedirection(lie, img_url));
	}

	if (helm && (helm->GetOldUrl().IsEmpty() || load_stat == COMM_REQUEST_FAILED))
		helm->UpdateImageUrl(img_url); // Stop using the old url

	if (load_stat == COMM_REQUEST_FAILED)
	{
		if (inline_type == SCRIPT_INLINE || inline_type == IMAGE_INLINE)
			helm->SendOnError();

		loading = LoadInlineStatus::LOADING_CANCELLED;
		SetInlineLoading(lie, FALSE);
	}

	if (loading != LoadInlineStatus::LOADING_CANCELLED)
	{
		SetInlineLoading(lie, TRUE);
		ForceImageNodeIfNeeded(inline_type, html_elm);
	}

	if (load_stat == COMM_LOADING)
		lie->SetNeedResetImage();

	if (img_url->Status(TRUE) == URL_LOADED)
	{
		// Apparently it's already loaded. That means that there might not be
		// any messages from the network code and we need to take care of
		// certain things here that would otherwise be done on MSG_URL_DATA_LOADED
		// or inside the network code.
		if (load_stat == COMM_LOADING && img_url_type != URL_OPERA && img_url_type != URL_DATA &&
			img_url_type != URL_JAVASCRIPT && img_url_type != URL_ATTACHMENT && img_url_type != URL_CONTENT_ID)
		{
			BYTE security_status = img_url->GetAttribute(URL::KSecurityStatus, TRUE);
			OP_ASSERT(security_status != SECURITY_STATE_UNKNOWN);
			window->SetSecurityState(security_status, TRUE, img_url->GetAttribute(URL::KSecurityText).CStr(), img_url);
		}

		if (!lie->HasSentHandleInlineDataAsyncMsg())
		{
			mh->PostMessage(MSG_HANDLE_INLINE_DATA_ASYNC, img_url->Id(TRUE), 0);
			lie->SetSentHandleInlineDataAsyncMsg(TRUE);
		}
		loading = LoadInlineStatus::LOADING_STARTED;
		SetInlineLoading(lie, TRUE);
	}

	if (!was_loading && !InlineImagesLoaded())
	{
		local_doc_finished = FALSE;
		doc_tree_finished = FALSE;
		delay_doc_finish = posted_delay_doc_finish = FALSE;

		// If the resource is added after the document has completed loading, then don't disturb the user with
		// a progress bar or "is loading" mouse cursors.
		BOOL load_silent = options.load_silent || GetDocumentFinishedAtLeastOnce();
		if (!load_silent && options.delay_load)
		{
			RETURN_IF_ERROR(window->StartProgressDisplay(TRUE, FALSE /* bResetSecurityState */, TRUE /* bSubResourcesOnly */));

			window->SetState(CLICKABLE);
		}
	}

	if (!lie->GetLoading())
	{
		/* If the inline isn't loading now, we aren't waiting for any messages, and
		   depending on why it isn't loading, there might not come any messages. */
		UnsetLoadingCallbacks(img_url_id);
		lie->SetSentHandleInlineDataAsyncMsg(FALSE);
	}

	if (loading == LoadInlineStatus::LOADING_STARTED && inline_type == CSS_INLINE)
		IncWaitForStyles();

#ifdef _DEBUG
	CheckInlineCounting(&inline_hash, image_info, url);
#endif // _DEBUG

	return loading;
}

URL_LoadPolicy FramesDocument::GetInlineLoadPolicy(URL inline_url, BOOL reload)
{
	DocumentManager *docman = GetDocManager();
	URL_Reload_Policy reloadpolicy;

	if (GetWindow()->IsURLAlreadyRequested(inline_url) && inline_url.Status(TRUE) == URL_LOADED)
		reloadpolicy = URL_Reload_None;
	else if (docman->MustReloadInlines())
		if (docman->MustConditionallyRequestInlines())
			reloadpolicy = URL_Reload_Conditional;
		else
			reloadpolicy = URL_Reload_Full;
	else if (reload)
		reloadpolicy = URL_Reload_Conditional;
	else
		reloadpolicy = URL_Reload_If_Expired;

	if (GetWindow()->GetOnlineMode() == Window::DENIED)
		reloadpolicy = URL_Reload_None;

	URL_LoadPolicy loadpolicy(history_reload, reloadpolicy, FALSE);

	loadpolicy.SetInlineElement(TRUE);
	return loadpolicy;
}

void FramesDocument::LimitUnusedInlines()
{
	LoadInlineElm *lie;
	unsigned total_inlines = 0, unused_inlines = 0;
	LoadInlineElmHashIterator iterator(inline_hash);

	lie = iterator.First();

	while (lie)
	{
		++total_inlines;
		if (lie->IsUnused())
			++unused_inlines;
		lie = iterator.Next();
	}

	lie = iterator.First();

	while (lie && unused_inlines > DOC_MINIMUM_UNUSED_INLINES && unused_inlines + unused_inlines > total_inlines)
	{
		if (lie->IsUnused())
		{
			RemoveLoadInlineElm(lie);
			OP_DELETE(lie);

			--unused_inlines;
			--total_inlines;
		}

		lie = iterator.Next();
	}
}

LoadInlineElm* FramesDocument::GetInline(URL_ID url_id, BOOL follow_ref)
{
	// First check if LoadInlineElm is already in list of elements
	// waiting for online mode:
	Window::OnlineMode online_mode = GetWindow()->GetOnlineMode();

	if (online_mode == Window::OFFLINE || online_mode == Window::USER_RESPONDING)
		for (LoadInlineElm* lie = (LoadInlineElm*) waiting_for_online_mode.First(); lie; lie = (LoadInlineElm*)lie->Suc())
			if (follow_ref ? lie->GetUrl()->Id(TRUE) == url_id : lie->IsLoadingUrl(url_id))
				return lie;

	// Check list of other inline elements:
	if (follow_ref)
	{
		Head* list;
		if (OpStatus::IsSuccess(inline_hash.GetData(url_id, &list)))
			return (LoadInlineElm*)list->First();
	}
	else
	{
		LoadInlineElmHashIterator iterator(inline_hash);
		for (LoadInlineElm* lie = iterator.First(); lie; lie = iterator.Next())
			if (lie->IsLoadingUrl(url_id))
				return lie;
	}

	return NULL;
}

#ifdef DOM_FULLSCREEN_MODE
OP_STATUS
FramesDocument::DOMRequestFullscreen(HTML_Element* element, ES_Runtime* origining_runtime, BOOL no_error)
{
	BOOL allow_fullscreen = TRUE;
	if (!ValidateFullscreenAccess(origining_runtime))
		allow_fullscreen = FALSE;
	else if (IsUndisplaying())
		allow_fullscreen = FALSE;
	else if (GetWindow()->GetType() != WIN_TYPE_NORMAL && GetWindow()->GetType() != WIN_TYPE_DEVTOOLS && GetWindow()->GetType() != WIN_TYPE_GADGET)
		allow_fullscreen = FALSE;
	else if (!GetLogicalDocument() || !GetLogicalDocument()->GetRoot() || !GetLogicalDocument()->GetRoot()->IsAncestorOf(element))
		allow_fullscreen = FALSE;
	else if (!DOMGetFullscreenEnabled())
		allow_fullscreen = FALSE;

	if (!allow_fullscreen)
	{
		if (!no_error)
			RETURN_IF_ERROR(HandleDocumentEvent(FULLSCREENERROR));
		return OpStatus::ERR;
	}

	// Set all elements in all documents.
	FramesDocument* ancestor_doc = this;
	HTML_Element* fullscreen_element = element;
	do
	{
		ancestor_doc->SetFullscreenElement(fullscreen_element);
		OpStatus::Ignore(ancestor_doc->HandleDocumentEvent(FULLSCREENCHANGE));

		DocumentManager* docman = ancestor_doc->GetDocManager();
		FramesDocElm* fdelm = docman->GetFrame();
		if (fdelm)
		{
			ancestor_doc = docman->GetParentDoc();
			fullscreen_element = fdelm->GetHtmlElement();
		}
		else
			ancestor_doc = NULL;
	}
	while (ancestor_doc);

	if (!GetWindow()->IsFullScreen() && g_pcjs->GetIntegerPref(PrefsCollectionJS::ChromelessDOMFullscreen, GetHostName()))
		return GetWindow()->GetWindowCommander()->GetDocumentListener()->OnJSFullScreenRequest(GetWindow()->GetWindowCommander(), TRUE);

	return OpStatus::OK;
}

OP_STATUS
FramesDocument::DOMExitFullscreen(ES_Runtime* origining_runtime)
{
	if (fullscreen_element.GetElm())
	{
		if (!ValidateFullscreenAccess(origining_runtime))
			return OpStatus::OK;

		DocumentTreeIterator iter(this);
		while (iter.Next())
		{
			FramesDocument* doc = iter.GetFramesDocument();
			if (doc && doc->fullscreen_element.GetElm())
			{
				doc->SetFullscreenElement(NULL);
				RETURN_IF_ERROR(doc->HandleDocumentEvent(FULLSCREENCHANGE));
			}
		}

		FramesDocument* ancestor_doc = this;
		do
		{
			ancestor_doc->SetFullscreenElement(NULL);
			RETURN_IF_ERROR(ancestor_doc->HandleDocumentEvent(FULLSCREENCHANGE));
			ancestor_doc = ancestor_doc->GetParentDoc();
		}
		while (ancestor_doc);

		if (GetWindow()->GetFullScreenState() == OpWindowCommander::FULLSCREEN_NORMAL)
			return GetWindow()->GetWindowCommander()->GetDocumentListener()->OnJSFullScreenRequest(GetWindow()->GetWindowCommander(), FALSE);
	}

	return OpStatus::OK;
}

BOOL
FramesDocument::ValidateFullscreenAccess(ES_Runtime* origining_runtime)
{
	if (origining_runtime == NULL)
		return TRUE;
	ES_Thread *thread = origining_runtime->GetESScheduler()->GetCurrentThread();
	OP_ASSERT(thread && "If there is a runtime, then this call came from DOM, therefore there must be a thread running.");
	if (!thread || !thread->IsUserRequested())
		return FALSE;
	return TRUE;
}

BOOL
FramesDocument::DOMGetFullscreenEnabled()
{
	for (FramesDocument *current_doc = this; current_doc; current_doc = current_doc->GetParentDoc())
	{
		if (current_doc->GetDocManager()->GetFrame())
		{
			HTML_Element *frame_element = current_doc->GetDocManager()->GetFrame()->GetHtmlElement();
			OP_ASSERT(frame_element);
			if (!frame_element->IsIncludedActual() || // Elements not insert by the parser or DOM are not allowed.
				!frame_element->IsMatchingType(Markup::HTE_IFRAME, NS_HTML) || // allowfullscreen only allowed on iframe elements.
				!frame_element->HasAttr(Markup::HA_ALLOWFULLSCREEN)) // allowfullscreen attribute must be set to allow fullscreen.
				return FALSE;
		}
		else
		{
			OP_ASSERT(!current_doc->GetParentDoc());
		}
	}
	return TRUE;
}

void
FramesDocument::SetFullscreenElement(HTML_Element* elm)
{
	if (fullscreen_element.GetElm() != elm)
	{
		if (logdoc && !IsUndisplaying())
		{
			fullscreen_element_obscured = FALSE;
			logdoc->GetHLDocProfile()->GetCSSCollection()->ChangeFullscreenElement(fullscreen_element.GetElm(), elm);
		}

		fullscreen_element.SetElm(elm);
	}
}

OP_DOC_STATUS
FramesDocument::DisplayFullscreenElement(const RECT& rect, VisualDevice* vd)
{
	LayoutWorkplace* layout_workplace = GetLogicalDocument()->GetLayoutWorkplace();
	HTML_Element* root = GetDocRoot();

	if (!IsPrintDocument() && (root->IsDirty() ||
							   root->HasDirtyChildProps() ||
							   root->IsPropsDirty() ||
							   layout_workplace->GetYieldElement()))
	{
		RETURN_IF_ERROR(Reflow(FALSE, TRUE));
	}

	HTML_Element* elm = GetFullscreenElement();
	if (elm && !IsFullscreenElementObscured() && elm->GetNsType() == NS_HTML)
	{
		if (elm->Type() == Markup::HTE_IFRAME)
		{
			layout_workplace->PaintFullscreen(elm, rect, vd);
			FramesDocElm* fde = FramesDocElm::GetFrmDocElmByHTML(elm);
			if (fde)
			{
				FramesDocument* doc = fde->GetCurrentDoc();
				if (doc)
					return doc->DisplayFullscreenElement(rect, vd);
			}
		}
		else
		{
			OP_STATUS err = layout_workplace->PaintFullscreen(elm, rect, vd);
			if (err == OpStatus::OK)
				return DocStatus::DOC_DISPLAYED;
			else if (OpStatus::IsMemoryError(err))
				return DocStatus::ERR_NO_MEMORY;
			else
			{
				OP_ASSERT(err == OpStatus::ERR);
				return DocStatus::DOC_CANNOT_DISPLAY;
			}
		}
	}

	return DocStatus::DOC_CANNOT_DISPLAY;
}
#endif // DOM_FULLSCREEN_MODE

void FramesDocument::ForceImageNodeIfNeeded(InlineResourceType inline_type, HTML_Element* html_elm)
{
	if (inline_type == IMAGE_INLINE)
		if (html_elm->HasAttr(ATTR_ONLOAD) || html_elm->HasAttr(ATTR_ONERROR))
		{
			// Must be kept alive in the ecmascript engine until those elements
			// have fired and for that we need to have nodes for them.
			if (OpStatus::IsSuccess(ConstructDOMEnvironment()))
			{
				DOM_Object* node;
				OpStatus::Ignore(dom_environment->ConstructNode(node, html_elm));
				// If the line above fails (oom), then the worst that can happen is that
				// the image gets garbage collected before any onload handler runs
				// which might make the page appear slightly broken.
			}
		}
}

#ifdef _PRINT_SUPPORT_
void FramesDocument::DeletePrintCopy()
{
	print_copy_being_deleted = TRUE;

	// Must do this so layout knows we are deleting stuff so it uninitialize things.
	// This will only work as long as LogicalDocument::DeletePrintCopy send the wrong document to Clean,Free.
	is_being_deleted = TRUE;

	if (logdoc)
	{
		logdoc->DeletePrintCopy();
		logdoc->GetLayoutWorkplace()->SetFramesDocument(logdoc->GetFramesDocument()); //Yikes
	}

	if (frm_root)
		frm_root->DeletePrintCopy();
	else
		if (ifrm_root)
			ifrm_root->DeletePrintCopy();

	is_being_deleted = FALSE;

	print_copy_being_deleted = FALSE;
}
#endif // _PRINT_SUPPORT_

void FramesDocument::UpdateSecurityState(BOOL include_loading_docs)
{
	// Traverse all used resources on the page and concatenate their
	// security states.  This can be a little tricky since some
	// resources might not have been used, for instance images when
	// image loading is disabled, and thus not loaded and therefore we
	// don't know anything about them. Those will have to be
	// ignored. Also, some items may have been dropped from the url
	// cache but are still used as an Image or other cached resource
	Window* window = GetWindow();
	DocumentTreeIterator iter(this);
	iter.SetIncludeThis();
	while (iter.Next())
	{
		if (!include_loading_docs &&
			iter.GetDocumentManager()->GetLoadStatus() != NOT_LOADING &&
			iter.GetDocumentManager()->GetLoadStatus() != DOC_CREATED)
		{
			// NOT_LOADING and DOC_CREATED are the states that
			// might have existing content. DOC_CREATED could
			// (when walking in history for instance) be
			// a mix of old content and content we're loading
			// the normal way and we have to check the old
			// content. See DocumentManager::UpdateSecurityState
			continue;
		}

		FramesDocument* frm_doc = iter.GetFramesDocument();
		URL& frm_doc_url = frm_doc->GetSecurityContext();
		if (frm_doc_url.GetAttribute(URL::KHeaderLoaded, TRUE) || frm_doc_url.Type() == URL_DATA)
		{
			BYTE security_state = SECURITY_STATE_NONE;
			if (frm_doc_url.Type() != URL_DATA)
				security_state = frm_doc_url.GetAttribute(URL::KSecurityStatus);
			window->SetSecurityState(security_state, FALSE, frm_doc_url.GetAttribute(URL::KSecurityText).CStr());
		}

		if (frm_doc->is_generated_doc)
			continue; // next document

		LoadInlineElmHashIterator iterator(frm_doc->inline_hash);
		for (LoadInlineElm* lie = iterator.First(); lie; lie = iterator.Next())
		{
			if (!lie->GetLoaded() && !lie->GetLoading()) // not used on the page
				continue; // next inline

			URL* inline_url = lie->GetUrl();
			if (!inline_url->IsEmpty() && inline_url->Type() != URL_DATA)
			{
				BOOL data_exists_or_is_used = TRUE;
				BOOL got_final_security = FALSE;
				BYTE security_status = inline_url->GetAttribute(URL::KSecurityStatus); // unreliable, but failing on the "safe" side
				for (HEListElm* helm = lie->GetFirstHEListElm(); helm && !got_final_security; helm = helm->Suc())
				{
					data_exists_or_is_used = TRUE;
					// All helms should have the same security state, only need
					// to check one
					switch (helm->GetInlineType())
					{
					case BGIMAGE_INLINE:
					case EXTRA_BGIMAGE_INLINE:
					case IMAGE_INLINE:
					case ICON_INLINE:
					case BORDER_IMAGE_INLINE:
						// Use the provider's security, recorded at the time the image was decoded
						// since we might have dropped the data from the url cache since
						if (UrlImageContentProvider* provider = helm->GetUrlContentProvider())
						{
							security_status = provider->GetSecurityStateOfSource(*inline_url);
							got_final_security = TRUE;
							break;
						}
						else
						{
							// no provider, means the image is not used. Might have been sent to
							// the ui as a favicon, but in that case it should have been removed
							// from the ui at about the same time as the provider went away.
							data_exists_or_is_used = FALSE;
							continue; // Check next use of the data
						}
					}
				}
				if (data_exists_or_is_used)
				{
					window->SetSecurityState(security_status, TRUE, inline_url->GetAttribute(URL::KSecurityText).CStr());
				}
			}
		}
	} // end for every document
}

BOOL FramesDocument::InlineImagesLoaded()
{
#ifdef MEDIA_SUPPORT
	for (UINT32 i = 0; i < media_elements.GetCount(); i++)
	{
		Media *media = media_elements.Get(i);
		if (media->GetDelayingLoad())
			return FALSE;
	}
#endif // MEDIA_SUPPORT

	return image_info.AllLoaded();
}

HTML_Element* FramesDocument::GetCurrentHTMLElement()
{
	if (doc)
		return doc->GetNavigationElement();
	else if (active_frm_doc)
	{
		FramesDocument* adoc = active_frm_doc->GetCurrentDoc();
		if (adoc)
			return adoc->GetCurrentHTMLElement();
	}

	return NULL;
}

#ifdef ACCESS_KEYS_SUPPORT

// Access key functionality

BOOL FramesDocument::HasAccessKeys()
{
	HLDocProfile *hld_profile = logdoc ? logdoc->GetHLDocProfile() : NULL;
	if( hld_profile )
		return hld_profile->HasAccessKeys();
	else
		return FALSE;
}

#endif // ACCESS_KEYS_SUPPORT

#ifdef _MIME_SUPPORT_
/* static */
OP_STATUS FramesDocument::MakeFrameSuppressUrl(URL &frame_url)
{
	URL tmp_url = g_url_api->GetNewOperaURL();
	if( tmp_url.SetAttribute(URL::KMIME_ForceContentType, "text/html") == OpStatus::ERR_NO_MEMORY )
		return OpStatus::ERR_NO_MEMORY;

# ifdef USE_ABOUT_FRAMEWORK
	OpSuppressedURL suppressed_url(tmp_url, frame_url);
	OP_STATUS rc = suppressed_url.GenerateData();
	frame_url = tmp_url;
	return rc;
# else
	tmp_url.WriteDocumentData(UNI_L("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\">\r\n<html>\r\n<head>\r\n<meta http-equiv=\"Content-Type\" content=\"text/html; charset=iso-8859-1\">\r\n<title>"));

	OpString tmp_str;
	g_languageManager->GetString(Str::SI_IDSTR_SUPPRESSED_FRAME_TITLE, tmp_str);

	tmp_url.WriteDocumentData(tmp_str.CStr());
	tmp_url.WriteDocumentData(UNI_L("</title>\r\n</head>\r\n"));

	g_languageManager->GetString(Str::SI_IDSTR_SUPPRESSED_FRAME_TEXT, tmp_str);

	tmp_url.WriteDocumentData(tmp_str.CStr());
	tmp_url.WriteDocumentData(UNI_L("<br/>\r\n"));

	uni_char *string = HTMLify_string_link(frame_url.UniName(PASSWORD_SHOW));
	if (!string)
		return OpStatus::ERR_NO_MEMORY;
	tmp_url.WriteDocumentData(string);
	OP_DELETEA(string);

	g_languageManager->GetString(Str::S_SUPPRESSED_CLICK_TO_VIEW, tmp_str);
	tmp_url.WriteDocumentData(URL::KNormal, UNI_L("\n"));
	tmp_url.WriteDocumentData(URL::KNormal, tmp_str);
	tmp_url.WriteDocumentDataFinished();

	frame_url = tmp_url;

	return OpStatus::OK;
# endif
}
#endif // _MIME_SUPPORT_

BOOL FramesDocument::GetSelectionBoundingRect(int &x, int &y, int &width, int &height)
{
	if (doc)
		return doc->GetSelectionBoundingRect(x, y, width, height);
	else
		return FALSE;
}

#ifdef SCOPE_ECMASCRIPT_DEBUGGER
/* static */ OP_STATUS
FramesDocument::DragonflyInspect(OpDocumentContext& context)
{
	DocumentInteractionContext& dic = static_cast<DocumentInteractionContext&>(context);

	FramesDocument *frm_doc = dic.GetDocument();
	HTML_Element* elm = dic.GetHTMLElement();

	if (!frm_doc || !elm)
		return OpStatus::ERR;

	return OpScopeElementInspector::InspectElement(frm_doc, elm);
}
#endif // SCOPE_ECMASCRIPT_DEBUGGER

#ifdef SVG_SUPPORT
OP_STATUS
FramesDocument::SVGZoomBy(OpDocumentContext& context, int zoomdelta)
{
	DocumentInteractionContext& dic = static_cast<DocumentInteractionContext&>(context);
	HTML_Element* elm = dic.GetHTMLElement();
	if (elm)
	{
		OP_ASSERT(logdoc && logdoc->GetRoot() && logdoc->GetRoot()->IsAncestorOf(elm));

		OpInputAction action(zoomdelta < 0 ? OpInputAction::ACTION_SVG_ZOOM_OUT : OpInputAction::ACTION_SVG_ZOOM_IN);
		action.SetActionPosition(context.GetScreenPosition());
		action.SetActionData(op_abs(zoomdelta));

		g_svg_manager->OnInputAction(&action, elm, this);
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}

OP_STATUS
FramesDocument::SVGZoomTo(OpDocumentContext& context, int zoomlevel)
{
	DocumentInteractionContext& dic = static_cast<DocumentInteractionContext&>(context);
	HTML_Element* elm = dic.GetHTMLElement();
	if (elm)
	{
		OP_ASSERT(logdoc && logdoc->GetRoot() && logdoc->GetRoot()->IsAncestorOf(elm));

		OpInputAction action(OpInputAction::ACTION_SVG_ZOOM);
		action.SetActionPosition(context.GetScreenPosition());
		action.SetActionData(op_abs(zoomlevel));

		g_svg_manager->OnInputAction(&action, elm, this);
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}

OP_STATUS
FramesDocument::SVGResetPan(OpDocumentContext& context)
{
	DocumentInteractionContext& dic = static_cast<DocumentInteractionContext&>(context);
	HTML_Element* elm = dic.GetHTMLElement();
	if (elm)
	{
		OP_ASSERT(logdoc && logdoc->GetRoot() && logdoc->GetRoot()->IsAncestorOf(elm));
		OpInputAction action(OpInputAction::ACTION_SVG_RESET_PAN);
		g_svg_manager->OnInputAction(&action, elm, this);
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}

OP_STATUS
FramesDocument::SVGPlayAnimation(OpDocumentContext& context)
{
	DocumentInteractionContext& dic = static_cast<DocumentInteractionContext&>(context);
	HTML_Element* elm = dic.GetHTMLElement();
	if (elm)
	{
		OP_ASSERT(logdoc && logdoc->GetRoot() && logdoc->GetRoot()->IsAncestorOf(elm));
		return g_svg_manager->StartAnimation(elm, this);
	}
	else
		return OpStatus::ERR;
}

OP_STATUS
FramesDocument::SVGPauseAnimation(OpDocumentContext& context)
{
	DocumentInteractionContext& dic = static_cast<DocumentInteractionContext&>(context);
	HTML_Element* elm = dic.GetHTMLElement();
	if (elm)
	{
		OP_ASSERT(logdoc && logdoc->GetRoot() && logdoc->GetRoot()->IsAncestorOf(elm));
		return g_svg_manager->PauseAnimation(elm, this);
	}
	else
		return OpStatus::ERR;
}

OP_STATUS
FramesDocument::SVGStopAnimation(OpDocumentContext& context)
{
	DocumentInteractionContext& dic = static_cast<DocumentInteractionContext&>(context);
	HTML_Element* elm = dic.GetHTMLElement();
	if (elm)
	{
		OP_ASSERT(logdoc && logdoc->GetRoot() && logdoc->GetRoot()->IsAncestorOf(elm));
		return g_svg_manager->StopAnimation(elm, this);
	}
	else
		return OpStatus::ERR;
}
#endif // SVG_SUPPORT

#ifdef MEDIA_HTML_SUPPORT
OP_STATUS
FramesDocument::MediaPlay(OpDocumentContext& context, BOOL play)
{
	DocumentInteractionContext& dic = static_cast<DocumentInteractionContext&>(context);
	HTML_Element* elm = dic.GetHTMLElement();

	if (elm)
	{
		MediaElement* media_elm = elm->GetMediaElement();
		if (media_elm)
		{
			if (play)
				media_elm->Play();
			else
				media_elm->Pause();

			return OpStatus::OK;
		}
	}

	return OpStatus::ERR;
}

OP_STATUS
FramesDocument::MediaShowControls(OpDocumentContext& context, BOOL show)
{
	DocumentInteractionContext& dic = static_cast<DocumentInteractionContext&>(context);
	HTML_Element* elm = dic.GetHTMLElement();

	if (elm)
	{
		MediaElement* media_elm = elm->GetMediaElement();

		if (media_elm)
		{
			if (show)
				elm->SetAttr(Markup::HA_CONTROLS, ITEM_TYPE_STRING, const_cast<uni_char*>(UNI_L("")));
			else
				elm->RemoveAttribute(Markup::HA_CONTROLS);

			media_elm->HandleAttributeChange(elm, ATTR_CONTROLS, NULL);

			return OpStatus::OK;
		}
	}

	return OpStatus::ERR;
}

OP_STATUS
FramesDocument::MediaMute(OpDocumentContext& context, BOOL mute)
{
	DocumentInteractionContext& dic = static_cast<DocumentInteractionContext&>(context);
	HTML_Element* elm = dic.GetHTMLElement();

	if (elm)
	{
		MediaElement* media_elm = elm->GetMediaElement();

		if (media_elm)
		{
			media_elm->SetMuted(mute);
			return OpStatus::OK;
		}
	}

	return OpStatus::ERR;
}
#endif // MEDIA_HTML_SUPPORT

OP_STATUS FramesDocument::GetNewIFrame(FramesDocElm*& frame, int width, int height, HTML_Element* element, OpView* clipview, BOOL load_frame, BOOL visible, ES_Thread *origin_thread)
{
	OP_NEW_DBG("FramesDocument::GetNewIFrame", "doc");
	OP_DBG((UNI_L("this=%p"), this));
	OP_PROBE4(OP_PROBE_FRAMESDOCUMENT_GETNEWIFRAME);
	OP_ASSERT(element->GetInserted() != HE_INSERTED_BY_PARSE_AHEAD);
	OP_ASSERT(!IsUndisplaying() || !"Trying to create and load frames while undisplaying is both a waste of time and risky");

	URL *iframe_url = NULL;
#ifdef SVG_SUPPORT
	if(g_svg_manager->AllowFrameForElement(element))
	{
		URL* root_url = &GetURL();
		iframe_url = g_svg_manager->GetXLinkURL(element, root_url);
	}
	else
#endif // SVG_SUPPORT
	if(element->GetNsType() == NS_HTML)
	{
		iframe_url = (URL *) element->GetUrlAttr((element->Type() == HE_IFRAME || element->Type() == HE_IMG || element->Type() == HE_EMBED) ? ATTR_SRC : ATTR_DATA, NS_IDX_HTML, logdoc);
	}

	if (!ifrm_root)
	{
#ifdef _PRINT_SUPPORT_
		if (IsPrintDocument())
		{
			// We don't create iframes in print documents. They should have been cloned before the reflow.
			// This can probably happen if we have media type in CSS so an iframe is display:none on
			// screen but display:block on printer.
			frame = NULL;
			return OpStatus::ERR_NOT_SUPPORTED;
		}
#endif // _PRINT_SUPPORT_

		int view_width = GetLayoutViewWidth();
		int view_height = GetLayoutViewHeight();

		ifrm_root = OP_NEW(FramesDocElm, (GetNewSubWinId(), 0, 0, view_width,
										  view_height, this, NULL, GetVisualDevice(),
										  FRAMESET_ABSOLUTE_SIZED, 0, FALSE, NULL));

		if (!ifrm_root || ifrm_root->Init(NULL, GetVisualDevice(), NULL) == OpStatus::ERR_NO_MEMORY)
		{
			OP_DELETE(ifrm_root);
			ifrm_root = NULL;
			frame = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	// Check if an iframe already exist.
	// This can happen if document must restart formatting because of a new style sheet

	frame = ifrm_root->FirstChild();
	while (frame && frame->GetHtmlElement() != element)
		frame = frame->Suc();

#ifdef _PRINT_SUPPORT_
	if (!frame && IsPrintDocument())
	{
		// We don't create iframes in print documents. They should have been cloned before the reflow.
		// This can probably happen if we have media type in CSS so an iframe is display:none on
		// screen but display:block on printer.
		return OpStatus::ERR_NOT_SUPPORTED;
	}
#endif // _PRINT_SUPPORT_

	BOOL from_restored_state = FALSE;
	if (!frame && document_state && (!iframe_url || iframe_url->Type() != URL_ATTACHMENT || !IsGeneratedByOpera()))
	{
		frame = document_state->FindIFrame(element);
		from_restored_state = TRUE;
	}

	if (frame)
	{
		LayoutMode frame_layout_mode = frame->GetLayoutMode();

		frame->CheckSpecialObject(element);
		frame->SetSize(width, height);

		if (OpStatus::IsMemoryError(frame->SetReinitData(
				from_restored_state ? GetWindow()->GetCurrentHistoryNumber() : -1,
				visible, frame_layout_mode)))
			return OpStatus::ERR_NO_MEMORY;

		MessageHandler *mh = GetMessageHandler();
		if (!mh->HasCallBack(this, MSG_REINIT_FRAME, (MH_PARAM_1) frame))
			if (mh->SetCallBack(this, MSG_REINIT_FRAME, (MH_PARAM_1) frame) == OpStatus::ERR_NO_MEMORY)
				return OpStatus::ERR_NO_MEMORY;

		mh->PostDelayedMessage(MSG_REINIT_FRAME, (MH_PARAM_1) frame, 0, 0);

		return OpStatus::OK;
	}

	frame = OP_NEW(FramesDocElm, (GetNewSubWinId(), -width, -height, width, height, this, element, GetVisualDevice(), FRAMESET_ABSOLUTE_SIZED, 0, TRUE, NULL));

	if (!frame)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	if (frame->Init(element, GetVisualDevice(), NULL) == OpStatus::ERR_NO_MEMORY)
	{
		OP_DELETE(frame);
		frame = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	frame->Under(ifrm_root);

	OP_STATUS stat = frame->FormatFrames();

	if (load_frame && !OpStatus::IsMemoryError(stat))
		stat = frame->LoadFrames(origin_thread);

	if (OpStatus::IsMemoryError(stat))
	{
		frame->Out();
		OP_DELETE(frame);
		frame = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	frame->SetSize(width, height);

	DocumentTreeIterator it(frame);

	it.SetIncludeThis();

	while (it.Next())
		if (FramesDocElm* fde = it.GetFramesDocElm())
			fde->UpdateGeometry();

	OP_ASSERT(frame);

	return OpStatus::OK;
}

OP_STATUS FramesDocument::ReflowFrames(BOOL set_css_properties)
{
	OP_ASSERT(frm_root);

	VisualDevice* vis_dev = GetVisualDevice();
	BOOL keep_existing_scrollbars = FALSE;
	BOOL old_handheld_enabled = GetHandheldEnabled();
	BOOL is_iframe = IsInlineFrame();

	layout_mode = GetWindow()->GetLayoutMode();
	era_mode = GetWindow()->GetERA_Mode();

	CheckERA_LayoutMode();

	if (logdoc)
		logdoc->GetHLDocProfile()->SetCSSMode(GetWindow()->GetCSSMode());

	if (!set_css_properties)
		set_css_properties = GetHandheldEnabled() != old_handheld_enabled;

	BOOL reflow_again;

	// Loop may be repeated on the top-level document only.

	do
	{
		reflow_again = FALSE;

		if (HTML_Element* root = GetDocRoot())
		{
			/* Mark clean. Note that we just mark the root clean, without checking the
			   rest of the tree. Elements in frameset documents are marked dirty by the
			   parser under certain circumstances, but it seems that it serves no purpose
			   for frameset documents. */

			root->MarkClean();
			root->MarkPropsClean();
		}

		/* Calculate available space.
		   Set the size on the frameset based on layout viewport size. */

		SetFrmRootSize();

		/* Distribute available space among sub-frames, according to framesets'
		   COLS and ROWS attributes. If the size of a sub-frame changes here,
		   the document in that frame will also be reflowed right away if that
		   document defines another frameset; or, if it defines a regular HTML
		   document (with BODY), it will be scheduled for reflow (marked dirty,
		   etc.). */

		RETURN_IF_ERROR(frm_root->FormatFrames());

		if (set_css_properties)
		{
			/* Reflow each document brutally and thoroughly.
			   Reload CSS properties, regenerate all layout boxes. */

			RETURN_IF_ERROR(frm_root->FormatDocs());
			set_css_properties = FALSE;
		}

		if (frames_policy != FRAMES_POLICY_NORMAL)
			if (GetSubWinId() == -1 || is_iframe)
			{
				/* Apply frame magic (smart frames / frame stacking) to the top-level
				   frameset and framesets established by iframes. Do the entire frame
				   tree (including framesets defined by sub-documents). */

				CalculateFrameSizes();

				// Check if the scrollbar situation changed, and reflow frameset again if so.

				reflow_again = RecalculateScrollbars(keep_existing_scrollbars);
				keep_existing_scrollbars = TRUE;
			}
	}
	while (reflow_again);

	if (GetSubWinId() == -1)
	{
		if (frames_stacked)
		{
			INT32 tmp = frm_root->GetHeight();
			vis_dev->ApplyScaleRounding(&tmp);
			frm_root->ForceHeight(tmp);

			if (frm_root->GetHeight() < vis_dev->GetRenderingViewHeight())
			{
				// The total of stacked frames is shorter than visible height. Find last frame
				// and make it fill the rest of the space.

				FramesDocElm* last_frame = (FramesDocElm*) frm_root->LastLeaf();
				while (last_frame && (last_frame->HasExplicitZeroSize() || !last_frame->GetVisualDevice()))
					last_frame = (FramesDocElm*) last_frame->PrevLeaf();

				if (last_frame)
					last_frame->ForceHeight(last_frame->GetHeight() + vis_dev->GetRenderingViewHeight() - frm_root->GetHeight());

				frm_root->ForceHeight(vis_dev->GetRenderingViewHeight());
			}

			if (vis_dev->GetView())
			{
				if (loading_frame && (loading_frame->IsLoaded() || loading_frame->GetHeight() > GetVisualViewHeight()))
				{
					// Jump to the frame that was just navigated to in history.

					RequestSetVisualViewPos(0, loading_frame->GetAbsY(), VIEWPORT_CHANGE_REASON_FRAME_FOCUS);
					loading_frame = NULL;
				}
			}
		}
	}
	else
		if (logdoc)
			logdoc->GetLayoutWorkplace()->HandleContentSizedIFrame();

	if (GetSubWinId() == -1 || is_iframe)
		for (DocumentTreeIterator it(frm_root); it.Next();)
			if (FramesDocElm* fde = it.GetFramesDocElm())
				fde->UpdateGeometry();

	HandleDocumentSizeChange();

	if (GetVisualDevice())
		GetVisualDevice()->UpdateAll();

	return OpStatus::OK;
}

OP_STATUS FramesDocument::Reflow(BOOL set_properties, BOOL iterate, BOOL reflow_frameset, BOOL find_iframe_parent)
{
//ee	OP_ASSERT(handheld_enabled == GetWindow()->GetHandheld() || set_properties);
	OP_PROBE0(OP_PROBE_FRAMESDOCUMENT_REFLOW);

#ifdef SCOPE_PROFILER
	OpTypeProbe probe;

	if (GetTimeline())
	{
		OpProbeTimeline *t = GetTimeline();
		RETURN_IF_ERROR(probe.Activate(t, PROBE_EVENT_REFLOW, t->GetUniqueKey()));
	}
#endif // SCOPE_PROFILER

	if (is_reflow_msg_posted)
	{
		GetMessageHandler()->RemoveDelayedMessage(MSG_REFLOW_DOCUMENT, GetSubWinId(), 0);
		is_reflow_msg_posted = FALSE;
	}

	OP_ASSERT(!reflowing);
	reflowing = TRUE;

	/* A previous, unexecuted, reflow asked for a set_properties reflow. */
	set_properties |= set_properties_reflow_requested;

	if (find_iframe_parent)
	{
		/* The layout of an IFRAME may depend on its parent documents, so reflow
		   the parent first. */

		FramesDocument *parent = this;

		while (parent && parent->IsInlineFrame())
		{
			parent = parent->GetDocManager()->GetParentDoc();

			if (parent)
			{
				OP_STATUS e = parent->Reflow(FALSE, iterate, reflow_frameset, FALSE);
				if (OpStatus::IsError(e))
				{
					reflowing = FALSE;
					return e;
				}
			}
		}
	}

	if (FramesDocElm *frame = GetDocManager()->GetFrame())
		if (frame->IsDeleted())
		{
			/* The frame has been deleted. This either happened before calling
			   this method, or while calling the FRAME's parent's Reflow(), in
			   the above block. */

			reflowing = FALSE;
			return OpStatus::OK;
		}

	if (GetVisualDevice())
		GetVisualDevice()->BeginPaintLock();

	BOOL has_reflowed = FALSE;

	OP_STATUS status = OpStatus::OK;
	OpStatus::Ignore(status);

#ifdef XSLT_SUPPORT
	if (set_properties && logdoc)
	{
		status = logdoc->ApplyXSLTStylesheets();
		if (OpStatus::IsError(status))
		{
			reflowing = FALSE;
			return status;
		}
	}
#endif // XSLT_SUPPORT

	if (!IsWaitingForStyles())
	{
		set_properties_reflow_requested = FALSE;

		if (media_handheld_responded || respond_to_media_type != RESPOND_TO_MEDIA_TYPE_NONE)
		{
			BOOL old_media_handheld_responded = media_handheld_responded;

			if (respond_to_media_type != RESPOND_TO_MEDIA_TYPE_NONE && GetHLDocProfile() &&
				GetHLDocProfile()->HasMediaStyle(RespondTypeToMediaType()))
				media_handheld_responded = TRUE;
			else
				media_handheld_responded = FALSE;

			if (!set_properties)
				set_properties = media_handheld_responded != old_media_handheld_responded;
		}

		if (frm_root)
		{
			if (reflow_frameset)
			{
				if (logdoc)
				{
					HTML_Element* frameset = logdoc->GetFirstHE(HE_FRAMESET);

					if (frameset)
					{
						Head existing_frames;

						frm_root->AppendChildrenToList(&existing_frames);

						frm_root->Into(&existing_frames);

						frm_root = NULL;

						status = CheckInternal(&existing_frames);

						for (FramesDocElm* fde = (FramesDocElm*) existing_frames.First(); fde; fde = (FramesDocElm*) existing_frames.First())
						{
							fde->Out();
							OP_DELETE(fde);
						}
					}
				}
			}
			else
			{
				status = ReflowFrames(set_properties);
			}

			if (logdoc && logdoc->IsParsed())
				logdoc->SetCompleted(TRUE);

			status = CheckFinishDocument();
		}
		else
		{
			if (logdoc)
			{
				HTML_Element* root;

#ifdef PAGED_MEDIA_SUPPORT
#ifdef _PRINT_SUPPORT_
				if (!(IsPrintDocument() || !logdoc->GetPrintRoot()))
					root = NULL;
				else if (IsPrintDocument())
					root = logdoc->GetPrintRoot();
				else
#endif // _PRINT_SUPPORT_
#endif // PAGED_MEDIA_SUPPORT
					root = logdoc->GetRoot();

				if (root)
				{
					if (set_properties)
					{
						layout_mode = GetWindow()->GetLayoutMode();
#if defined(_PRINT_SUPPORT_)
						if (IsPrintDocument() && g_pcprint->GetIntegerPref(PrefsCollectionPrint::FitToWidthPrint))
							era_mode = TRUE;
						else
#endif
							era_mode = GetWindow()->GetERA_Mode();

						BOOL set_has_real_size_dependent_css = FALSE;
						if (layout_mode == LAYOUT_SSR || layout_mode == LAYOUT_CSSR)
						{
							PrefsCollectionDisplay::RenderingModes rendering_mode = layout_mode == LAYOUT_SSR ? PrefsCollectionDisplay::SSR : PrefsCollectionDisplay::CSSR;
							if (g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::RemoveOrnamentalImages), GetHostName()) ||
								g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::RemoveLargeImages), GetHostName()) ||
								g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::UseAltForCertainImages), GetHostName()))
								set_has_real_size_dependent_css = TRUE;
						}

						/* First do some cleaning in inline_hash.  Remove all HEListElms created by
						   the html layout engine. */

						LoadInlineElmHashIterator iterator(inline_hash);
						for (LoadInlineElm* lie = iterator.First(); lie; lie = iterator.Next())
						{
							HEListElm* next_helm = NULL;
							HEListElm* helm = lie->GetFirstHEListElm();

							while (helm)
							{
								HTML_Element* elm = helm->HElm();
								if (set_has_real_size_dependent_css && elm->IsMatchingType(HE_IMG, NS_HTML))
									elm->SetHasRealSizeDependentCss(TRUE);

								next_helm = helm->Suc();

								if (helm->GetInlineType() == BGIMAGE_INLINE ||
									helm->GetInlineType() == IMAGE_INLINE ||
									helm->GetInlineType() == EMBED_INLINE ||
									helm->GetInlineType() == GENERIC_INLINE ||
									helm->GetInlineType() == EXTRA_BGIMAGE_INLINE ||
									helm->GetInlineType() == BORDER_IMAGE_INLINE ||
									helm->GetInlineType() == VIDEO_POSTER_INLINE
#ifdef SVG_SUPPORT
									&& elm->GetNsType() != NS_SVG // HEListElms for SVG elements are not recreated during reflow
#endif // SVG_SUPPORT
									)
								{
									if (helm->GetEventSent())
										elm->SetSpecialBoolAttr(ATTR_INLINE_ONLOAD_SENT, TRUE, SpecialNs::NS_LOGDOC);

#ifdef _PLUGIN_SUPPORT_
									if (helm->IsEmbedInline())
									{
										Box* box = elm->GetLayoutBox();
										if (box && box->GetContent() && box->GetContent()->IsEmbed())
											static_cast<EmbedContent*>(box->GetContent())->Hide(this);
									}
#endif // _PLUGIN_SUPPORT_

									helm->Undisplay();
									if (root->IsAncestorOf(elm))
										OP_DELETE(helm);
								}

								helm = next_helm;
							}
						}

						CheckERA_LayoutMode();

						if (ifrm_root)
						{
							FramesDocElm *fde = (FramesDocElm *) ifrm_root->FirstLeaf();
							while (fde)
							{
								if (FramesDocument *doc = fde->GetCurrentDoc())
									if (doc->Reflow(set_properties, iterate, reflow_frameset, FALSE) == OpStatus::ERR_NO_MEMORY)
										status = OpStatus::ERR_NO_MEMORY;
								fde = (FramesDocElm *) fde->NextLeaf();
							}
						}
					}

					imgManager->LockImageCache();
					OP_ASSERT(logdoc && logdoc->GetLayoutWorkplace());
					status = logdoc->GetLayoutWorkplace()->Reflow(has_reflowed, set_properties, iterate);
					if (doc_tree_finished)
					{
						if (StoredViewportCanBeRestored())
							GetDocManager()->RestoreViewport(TRUE, FALSE, TRUE);
					}

					imgManager->UnlockImageCache();

					if (OpStatus::IsError(status))
					{
						reflowing = FALSE;
						return status;
					}
#ifndef MOUSELESS
					if (doc)
					{
						HTML_Element *hover_elm = doc->GetHoverHTMLElement();
						if (hover_elm)
							hover_elm->UpdateCursor(this);
					}
#endif // !MOUSELESS

#ifdef _PLUGIN_SUPPORT_
					// If some script was waiting for a plugin to load before continuing, now is the time to release it and let it go on
					ResumeAllFinishedPlugins();
#endif // _PLUGIN_SUPPORT_
				}
			}
		}
	}
	else
	{
		if (logdoc)
			/* We're waiting for stylesheets. Prefetch URLs from added
			   stylesheets/elements. */
			status = logdoc->GetHLDocProfile()->GetCSSCollection()->PrefetchURLs();

		if (set_properties)
		{
			/* We asked for a set_properties reflow. Make sure we
			   keep that information. */
			set_properties_reflow_requested = TRUE;
		}
	}

	reflowing = FALSE;

	/* Make sure that viewports are within the (potentially new and reduced)
	   bounds of the document. Not sure if it is good / sufficient to let this
	   operation be triggered by reflow, but that's what we do for now. */

	RequestConstrainVisualViewPosToDoc();

	if (!IsPrintDocument())
		ProcessAutofocus();

	RefocusElement();

	VisualDevice* vd = GetVisualDevice();
	if (vd)
	{
		vd->EndPaintLock();
		vd->OnReflow();
		/* We did a reflow and now it's time to paint with all known
		 * CSSes downloaded and applied.
		 *
		 * To avoid painting too early when page empty check that it
		 * has either finished parsing or has loaded some content. Also
		 * avoid painting when running scripts.
		 *
		 * This is needed for platforms where
		 * PrefsCollectionDisplay::StyledFirstUpdateTimeout is set to
		 * a large number so that the time to the first paint can be
		 * too long.
		 */
		if (do_styled_paint &&
			wait_for_paint_blockers <= 0 &&
			!IsWaitingForStyles() &&
			(!es_scheduler || !es_scheduler->HasRunnableTasks()) &&
			(IsParsed() || !logdoc || logdoc->GetBodyElm() && logdoc->GetHLDocProfile()->TextCount() > 1000))
		{
			vd->StylesApplied();
			do_styled_paint = FALSE;
		}
	}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	if (has_reflowed && vd && vd->GetAccessibleDocument())
	{
		vd->GetAccessibleDocument()->DocumentReflowed(this);
	}
#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#if defined DOCUMENT_EDIT_SUPPORT || defined KEYBOARD_SELECTION_SUPPORT
	// The location of the caret might have moved so tell the caret painter to
	// recalculate the new position.
	if (has_reflowed)
	{
		if (GetCaretPainter())
			GetCaretPainter()->UpdatePos();
#ifdef DOCUMENT_EDIT_SUPPORT
		if (document_edit)
			document_edit->OnReflow();
#endif
	}
#endif // DOCUMENT_EDIT_SUPPORT || KEYBOARD_SELECTION_SUPPORT

#ifdef PAGED_MEDIA_SUPPORT
	if (restore_current_page_number >= 0)
	{
		if (logdoc)
			logdoc->GetLayoutWorkplace()->SetCurrentPageNumber(restore_current_page_number, VIEWPORT_CHANGE_REASON_INPUT_ACTION);

		restore_current_page_number = -1;
	}
#endif // PAGED_MEDIA_SUPPORT

	if (status != OpStatus::ERR_YIELD)
		activity_reflow.Cancel();

	if (has_reflowed)
	{
		OP_PROBE0(OP_PROBE_REFLOW_COUNT);
		g_locked_dd_closer.CloseLockedDropDowns();
		ViewportController* controller = GetWindow()->GetViewportController();
		OpViewportInfoListener* listener = controller->GetViewportInfoListener();
		listener->OnDocumentContentChanged(controller, OpViewportInfoListener::REASON_REFLOW);
	}

	return status;
}

void FramesDocument::UndisplayDocument()
{
	if (logdoc && logdoc->GetRoot())
		logdoc->GetRoot()->DisableContent(this);
}

int FramesDocument::GetReflowMsgDelay()
{
	int doc_delay = 0, script_delay = 0;

	if (GetDocManager()->GetLoadStatus() == DOC_CREATED && !(IsParsed() && InlineImagesLoaded()))
		doc_delay = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ReflowDelayLoad, GetHostName());

	if (GetTopDocument()->IsESActive(TRUE))
		script_delay = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ReflowDelayScript, GetHostName());

	return MAX(doc_delay, script_delay);
}

void FramesDocument::PostReflowMsg(BOOL no_delay)
{
	OP_ASSERT(!IsBeingFreed() || !"Find out what is posting the message and abort before getting here.");

# ifdef JS_LAYOUT_CONTROL
	if (pause_layout)
		return;
# endif

	es_recent_layout_changes = TRUE;

	MessageHandler *mh = GetMessageHandler();

	if (!is_reflow_msg_posted)
		activity_reflow.Begin();

	if (is_reflow_msg_posted)
	{
		if (no_delay && reflow_msg_delayed_to_time != 0.0)
		{
			mh->RemoveDelayedMessage(MSG_REFLOW_DOCUMENT, GetSubWinId(), 0);
			is_reflow_msg_posted = FALSE;
		}
	}

	if (!is_reflow_msg_posted)
	{
		int delay = no_delay ? 0 : GetReflowMsgDelay();

		mh->PostMessage(MSG_REFLOW_DOCUMENT, GetSubWinId(), 0, delay);

		is_reflow_msg_posted = TRUE;
		reflow_msg_delayed_to_time = delay > 0 ? g_op_time_info->GetRuntimeMS() + delay : 0.0;
	}
}

#ifdef JS_LAYOUT_CONTROL
void FramesDocument::PauseLayout()
{
	if (!pause_layout)
	{
		pause_layout = TRUE;
		// Only lock once
		GetVisualDevice()->LockUpdate(TRUE);
	}
}

void FramesDocument::ContinueLayout()
{
	// Only update everything if we had a pause
	if (pause_layout)
	{
		pause_layout = FALSE;
		GetVisualDevice()->LockUpdate(FALSE);

		PostReflowMsg();
		// Force an invalidation of the screen
		GetVisualDevice()->UpdateAll();
	}
}
#endif // JS_LAYOUT_CONTROL

void FramesDocument::PostReflowFramesetMsg()
{
	OP_ASSERT(!IsBeingFreed() || !"Find out what is posting the message and abort before getting here.");

	if (!is_reflow_frameset_msg_posted)
	{
		GetMessageHandler()->PostMessage(MSG_REFLOW_DOCUMENT, GetSubWinId(), 1);

		is_reflow_frameset_msg_posted = TRUE;

		activity_reflow.Begin();
	}
}

#ifdef WAND_SUPPORT
void FramesDocument::AddPendingWandElement(HTML_Element* elm, ES_Thread* thread)
{
	OP_ASSERT(elm);
	OP_ASSERT(thread);

	if (!wand_inserter)
	{
		wand_inserter = OP_NEW(WandInserter, (this));
		if (wand_inserter)
		{
			SetHasWandMatches(TRUE);
			OpStatus::Ignore(pending_wand_elements.Add(elm));
			thread->AddListener(wand_inserter);
		}
	}
	else
	{
		SetHasWandMatches(TRUE);
		OpStatus::Ignore(pending_wand_elements.Add(elm));
	}
}

void FramesDocument::InsertPendingWandData()
{
	wand_inserter = NULL;

	for (unsigned int i = 0; i < pending_wand_elements.GetCount(); i++)
	{
		g_wand_manager->InsertWandDataInDocument(this,
			pending_wand_elements.Get(i), NO);
	}

	pending_wand_elements.Clear();
}
#endif

BOOL FramesDocument::IsInlineFrame()
{
	return GetDocManager()->GetFrame() && GetDocManager()->GetFrame()->IsInlineFrame();
}


int FramesDocument::CountFrames()
{
	if (frm_root)
		return frm_root->CountFrames();
	else
		return 0;
}

FramesDocElm* FramesDocument::GetFrameByNumber(int& num)
{
	if (frm_root)
		return frm_root->GetFrameByNumber(num);
	else
		return NULL;
}

OP_STATUS FramesDocument::WaitForStylesTimedOut()
{
	BOOL was_waiting = IsWaitingForStyles() || wait_for_paint_blockers > 0;
	ResetWaitForStyles();

	wait_for_paint_blockers = 0;

	if (was_waiting)
		GetVisualDevice()->UpdateAll();

	return OpStatus::OK;
}

BOOL FramesDocument::CheckWaitForStylesTimeout()
{
	int timeout = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::WaitForStyles, GetHostName());

	BOOL enable = (!GetDocRoot() || !GetDocRoot()->GetLayoutBox()) && timeout > 0;

	if (enable && wait_for_styles == 0 && wait_for_paint_blockers == 0)
		GetMessageHandler()->PostDelayedMessage(MSG_WAIT_FOR_STYLES_TIMEOUT, GetSubWinId(), 0, timeout);

	return enable;
}

void FramesDocument::IncWaitForStyles()
{
	OP_ASSERT(wait_for_styles >= -1);

	if (wait_for_styles == -1)
		return;

	if (CheckWaitForStylesTimeout())
		wait_for_styles++;
}

void FramesDocument::DecWaitForStyles()
{
	if (IsWaitingForStyles())
	{
		wait_for_styles--;

		if (!IsWaitingForStyles())
		{
			SignalAllDelayedLayoutListeners();
			GetVisualDevice()->UpdateAll();

			/* All known CSS has been loaded. Reflow without delay,
			   in order to display the styled content as fast as
			   possible. */
			do_styled_paint = TRUE;
		}
	}
}

void FramesDocument::ResetWaitForStyles()
{
	HLDocProfile *hld_profile = GetHLDocProfile();
	if (hld_profile)
		hld_profile->GetCSSCollection()->DisableFontPrefetching();

	wait_for_styles = -1;

	SignalAllDelayedLayoutListeners();
}

void FramesDocument::ExternalInlineStopped(ExternalInlineListener* listener)
{
	if (listener->has_inc_wait_for_paint_blockers)
	{
		listener->has_inc_wait_for_paint_blockers = FALSE;
		if (wait_for_paint_blockers > 0)
		{
			wait_for_paint_blockers--;
			if (!IsWaitingForStyles() && wait_for_paint_blockers == 0)
				GetVisualDevice()->UpdateAll();
		}
	}

}

DocOperaStyleURLManager::DocOperaStyleURLManager()
{
	for (int i = 0; i < WRAPPER_DOC_TYPE_COUNT; i++)
		m_url_generators[i] = NULL;
}

DocOperaStyleURLManager::~DocOperaStyleURLManager()
{
	for (int i = 0; i < WRAPPER_DOC_TYPE_COUNT; i++)
		OP_DELETE(m_url_generators[i]);
}

/* static */ OP_STATUS
DocOperaStyleURLManager::Construct(WrapperDocType type)
{
	if (!g_opera->doc_module.opera_style_url_generator)
	{
		DocOperaStyleURLManager* style_urlman = OP_NEW(DocOperaStyleURLManager, ());
		if (!style_urlman)
			return OpStatus::ERR_NO_MEMORY;

		g_opera->doc_module.opera_style_url_generator = style_urlman;
	}
	return g_opera->doc_module.opera_style_url_generator->CreateGenerator(type);
}

OP_STATUS
DocOperaStyleURLManager::CreateGenerator(WrapperDocType type)
{
	if (m_url_generators[type])
		return OpStatus::OK;

	PrefsCollectionFiles::filepref pref;
	const char* name;

	if (type == IMAGE)
	{
		pref = PrefsCollectionFiles::StyleImageFile;
		name = "style/image.css";
	}
#ifdef MEDIA_HTML_SUPPORT
	else if (type == MEDIA)
	{
		pref = PrefsCollectionFiles::StyleMediaFile;
		name = "style/media.css";
	}
#endif // MEDIA_HTML_SUPPORT
	else
	{
		return OpStatus::OK;
	}

	Generator* generator = OP_NEW(Generator, ());
	if (!generator)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = generator->Construct(pref, name);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(generator);
		return OpStatus::ERR;
	}

	m_url_generators[type] = generator;

	g_url_api->RegisterOperaURL(generator);

	return OpStatus::OK;
}


/* static */ OP_STATUS
DocOperaStyleURLManager::Generator::Construct(PrefsCollectionFiles::filepref pref, const char* name)
{
	const OpFile *file = g_pcfiles->GetFile(pref);

	RETURN_IF_ERROR(m_path.Set(file->GetFullPath()));
	RETURN_IF_ERROR(OperaURL_Generator::Construct(name, FALSE));

	return OpStatus::OK;
}

/* virtual */ OpStringC
DocOperaStyleURLManager::Generator::GetFileLocation(URL &url, OpFileFolder &fldr, OP_STATUS &status)
{
	if (OpStatus::IsError(status = url.SetAttribute(URL::KContentType, URL_CSS_CONTENT)) ||
	    OpStatus::IsError(status = url.SetAttribute(URL::KMIME_CharSet, "utf-8")))
	{
		OpStringC result;
		return result;
	}

	fldr = OPFILE_ABSOLUTE_FOLDER;

	OpStringC result(m_path.CStr());
	return result;
}

OP_STATUS FramesDocument::MakeWrapperDoc()
{
	if (is_plugin)
	{
		OP_STATUS status;
#ifdef ZOOMABLE_STANDALONE_PLUGINS
		status = wrapper_doc_source.Set("<body style='margin: 0;'><embed src='' width=100% height=100%></body>");
#else
		status = wrapper_doc_source.Set("<html><head><meta name='viewport' content='width=device-width'></head><body style='margin: 0;'><embed src='' width='100%' height='100%'></body></html>");
#endif
		return status;
	}
#ifdef MEDIA_HTML_SUPPORT
	else if (g_media_module.IsMediaPlayableUrl(url))
	{
		const OpStringC8& http_content_type = url.GetAttribute(URL::KMIME_Type);
		RETURN_IF_ERROR(MakeMediaWrapperDoc(wrapper_doc_source, http_content_type.CompareI("audio/", 6) == 0));
	}
#endif // MEDIA_HTML_SUPPORT
#ifdef HAS_ATVEF_SUPPORT
	else if (url.Type() == URL_TV)
	{
		RETURN_IF_ERROR(wrapper_doc_source.Set("<body style='padding: 0; margin: 0;'><img src=''></body>"));
	}
#endif // HAS_ATVEF_SUPPORT
	else
	{
		RETURN_IF_ERROR(MakeImageWrapperDoc(wrapper_doc_source));

		if (!GetWindow()->LoadImages())
		{
			BOOL stop_loading = TRUE;
#ifdef FLEXIBLE_IMAGE_POLICY
			// Allow images to be loaded if it's only the image and nothing else
			stop_loading = !IsTopDocument();
#endif // FLEXIBLE_IMAGE_POLICY
			if (stop_loading)
			{
				// FIXME: Not sure DocumentManager will be at all happy about this.
				url.StopLoading(GetMessageHandler());
			}
		}
	}

	return OpStatus::OK;
}

OP_STATUS
FramesDocument::MakeImageWrapperDoc(OpString &buf)
{
	// Must first check if this document is an object/embed created document because in that case
	// we need to fake that it's a pure simple image rather than a full document. Really a bug
	// in layout (CORE-36238) but that isn't easy to fix.
	if (FramesDocElm* fdelm = GetDocManager()->GetFrame())
	{
		if (HTML_Element* elm = fdelm->GetHtmlElement())
		{
			if (elm->IsMatchingType(HE_EMBED, NS_HTML) ||
				elm->IsMatchingType(HE_OBJECT, NS_HTML))
			{
				const char* embedded_image_doc_html =
					"<!DOCTYPE html><html>"
					"<style>html,body,img{display:block;overflow:hidden;margin:0;padding:0;width:100%;height:100%;}</style>"
					"<body><img src=''></body></html>";
				return buf.Set(embedded_image_doc_html);
			}
		}
	}
	RETURN_IF_ERROR(DocOperaStyleURLManager::Construct(DocOperaStyleURLManager::IMAGE));
	const char* image_doc_html =
		"<!DOCTYPE html>"
		"<html>"
		"<head>"
		"<link rel='stylesheet' media='screen,projection,tv,handheld' href='opera:style/image.css'>"
		// This rule will be rewritten by script and then apply for small screens (screens smaller than the image).
		"<style media='not all'>"
			/* large images: centered and downscaled (no scrollbars). */
			"body.contain {"
				"position: absolute;"
				"margin: 0;"
				"top: 0;"
				"left: 0;"
				"bottom: 0;"
				"right: 0;"
			"}"
			"body.contain > img {"
				"width: 100%;"
				"height: 100%;"
				"-o-object-fit: contain;"
				"cursor: zoom-in;"
			"}"
			/* large images: centered and unscaled (with scrollbars). */
			"body.zoom > img {"
				"cursor: zoom-out;"
			"}"
		"</style>"
		"<link rel='icon' href=''>"
		"</head>"

		"<body class=contain>"
			"<img onerror='this.error=true' src=''>"
			"<script>"
			"(function() {"
				// To make it easier for extensions to disable this code.
				"if (window.donotrun)"
					"return;"
				"var img = document.querySelector('img');"
				// Set title from last path component (typically the filename).
				"document.title = window.location.pathname.match(/([^\\/]*)\\/*$/)[1];"
				// Zoom center coordinates.
				"var zoomX;"
				"var zoomY;"

				"function zoomIn() {"
					"document.body.className = 'zoom';"
					// Center view on (zoomX, zoomY).
					"window.scrollTo(zoomX + img.offsetLeft - window.innerWidth/2,"
						"zoomY + img.offsetTop - window.innerHeight/2);"
				"}"

				"function zoomOut() {"
					// Remember (zoomX, zoomY) for keyboard zoom.
					"zoomX = window.scrollX + window.innerWidth/2 - img.offsetLeft;"
					"zoomY = window.scrollY + window.innerHeight/2 - img.offsetTop;"
					"document.body.className = 'contain';"
				"}"

				// Set the media rules from naturalWidth/naturalHeight.
				"var interval = setInterval(function() {"
					"if (img.naturalWidth && img.naturalHeight) {"
						"clearInterval(interval);"
						// Show (wxh) in title.
						"document.title += ' ('+img.naturalWidth+'\\u00d7'+img.naturalHeight+')';"
						// Set the media rule from naturalWidth/naturalHeight, taking
						// into account padding (margin is assumed to be 0)
						"var padding = 2*parseInt(getComputedStyle(document.body).padding, 10);"
						"var s = document.querySelector('style[media]');"
						"s.media = '(max-width:' + (img.naturalWidth + padding) + 'px),(max-height:' + (img.naturalHeight + padding) + 'px)';"
						// Initial (zoomX, zoomY) for keyboard zoom.
						"zoomX = img.naturalWidth/2;"
						"zoomY = img.naturalHeight/2;"
					"} else if (img.error) {"
						"clearInterval(interval);"
						// could do something to indicate brokenness here, like:
						//img.className = 'broken'
					"}"
				"}, 100);"

				// Was any modifier key pressed?
				"function modKey(e) { return e.altKey || e.ctrlKey || e.metaKey || e.shiftKey; }"

				// Toggle zoom with mouse.
				"img.onclick = function(e) {"
					"if (e.button == 0 && !modKey(e)) {"
						"var s = getComputedStyle(img);"
						"if (s.cursor == 'zoom-in') {"
							// Calculate zoomX/zoomY as the clicked position in natural image coordinates.
							"zoomX = e.offsetX;"
							"zoomY = e.offsetY;"
							"var clientAspect = img.clientWidth/img.clientHeight;"
							"var naturalAspect = img.naturalWidth/img.naturalHeight;"
							// displayWidth/displayHeight = naturalAspect, solve for the unknown.
							"var scale;"
							"if (naturalAspect > clientAspect) {"
								// Letterboxed.
								"var displayHeight = img.clientWidth/naturalAspect;"
								"zoomY -= (img.clientHeight - displayHeight)/2;"
								"scale = img.clientWidth/img.naturalWidth;"
							"} else {"
								// Pillarboxed.
								"var displayWidth = img.clientHeight*naturalAspect;"
								"zoomX -= (img.clientWidth - displayWidth)/2;"
								"scale = img.clientHeight/img.naturalHeight;"
							"}"
							"zoomX /= scale;"
							"zoomY /= scale;"
							"zoomIn();"
						"} else if (s.cursor == 'zoom-out') {"
							"zoomOut();"
						"}"
					"}"
				"};"

				// Toggle zoom with enter button (not space since that is typically bound to fast-forward).
				"window.onkeypress = function(e) {"
					"if (e.keyCode == 13 && !modKey(e)) {"
						"var s = getComputedStyle(img);"
						"if (s.cursor == 'zoom-in')"
							"zoomIn();"
						"else if (s.cursor == 'zoom-out')"
							"zoomOut();"
						// Disable other actions triggered by this key, such as scrolling.
						"e.preventDefault();"
					"}"
				"};"

				"var drag;"

				"window.ondragstart = function(e) {"
					"if (document.body.className == 'zoom')"
						"e.preventDefault();"
				"};"

				"window.onmousedown = function(e) {"
					"if (e.button == 0 && !modKey(e) && document.body.className == 'zoom')"
						"drag = {"
							"screenX: e.screenX,"
							"screenY: e.screenY,"
							"scrollX: window.scrollX,"
							"scrollY: window.scrollY"
						"};"
				"};"

				"window.onmouseup = function(e) {"
					// Reset cursor to the default cursor from the style rules.
					"img.style.cursor = '';"
					"drag = undefined;"
				"};"

				"window.onmousemove = function(e) {"
					"if (!drag) return;"
					"img.style.cursor = 'move';"
					"window.scrollTo(drag.scrollX + drag.screenX - e.screenX,"
									"drag.scrollY + drag.screenY - e.screenY);"
				"};"
			"})();"
			"</script>"
		"</body>";

	return buf.Set(image_doc_html);
}

#ifdef MEDIA_HTML_SUPPORT
OP_STATUS FramesDocument::MakeMediaWrapperDoc(OpString &buf, BOOL is_audio)
{
	const char* element_name = is_audio ? "audio" : "video";
	RETURN_IF_ERROR(DocOperaStyleURLManager::Construct(DocOperaStyleURLManager::MEDIA));
	TRAPD(status,
		  buf.SetL("<html><head><link rel='stylesheet' media='screen,projection,tv,handheld' href='opera:style/media.css'></head><body><div><");
		  buf.AppendL(element_name);
		  buf.AppendL(" src=\"\" autoplay controls></");
		  buf.AppendL(element_name);
		  buf.AppendL("></div></body></html>");
		);
	return status;
}
#endif // MEDIA_HTML_SUPPORT

#ifdef _DEBUG
void FramesDocument::DebugCheckInline()
{
	OP_NEW_DBG("DebugCheckInline", "image_loading");
	OP_DBG((UNI_L("this=%p"), this));
	LoadInlineElmHashIterator iterator(inline_hash);
	for (LoadInlineElm* lie = iterator.First(); lie; lie = iterator.Next())
	{
		const char* name = lie->GetUrl()->GetAttribute(URL::KName_Username_Password_NOT_FOR_UI).CStr();
		URLStatus   stat = lie->GetUrl()->Status(TRUE);
		HEListElm*  helm = lie->GetFirstHEListElm();

		OP_DBG(("url: ") << name << ", stat: " << stat << ", helm:" << helm);
	}
}
#endif

#ifdef LINK_TRAVERSAL_SUPPORT
void FramesDocument::GetLinkElements(OpAutoVector<OpElementInfo>* vector)
{
	OP_ASSERT(vector);
	if (doc)
		doc->GetLinkElements(vector);
}
#endif // LINK_TRAVERSAL_SUPPORT

OP_STATUS FramesDocument::LoadAllIFrames()
{
	if (ifrm_root)
	{
		for (FramesDocElm* fde = ifrm_root->FirstChild(); fde; fde = fde->Suc())
		{
			FramesDocument* doc = fde->GetCurrentDoc();

			if (doc && doc->IsLoaded())
			{
				continue;
			}

			URL url = fde->GetCurrentURL();

			if (!IsAllowedIFrame(&url))
				continue;

			RETURN_IF_MEMORY_ERROR(fde->LoadFrames());
		}
	}
	return OpStatus::OK;
}


CSS_MediaType FramesDocument::RespondTypeToMediaType()
{
	switch (respond_to_media_type)
	{
	case RESPOND_TO_MEDIA_TYPE_HANDHELD:
		return CSS_MEDIA_TYPE_HANDHELD;
#ifdef TV_RENDERING
	case RESPOND_TO_MEDIA_TYPE_TV:
		return CSS_MEDIA_TYPE_TV;
#endif // TV_RENDERING
	case RESPOND_TO_MEDIA_TYPE_PROJECTION:
		return CSS_MEDIA_TYPE_PROJECTION;
	}
	return CSS_MEDIA_TYPE_SCREEN;
}

CSS_MediaType FramesDocument::GetMediaType()
{
#ifdef _PRINT_SUPPORT_
	if (IsPrintDocument())
		return CSS_MEDIA_TYPE_PRINT;
#endif // _PRINT_SUPPORT_

#ifdef DOC_PRIMARY_MEDIUM
	if (GetWindow()->GetLayoutMode() == LAYOUT_NORMAL && DOC_PRIMARY_MEDIUM != CSS_MEDIA_TYPE_NONE && GetHLDocProfile() && GetHLDocProfile()->HasMediaStyle(DOC_PRIMARY_MEDIUM))
		return DOC_PRIMARY_MEDIUM;
#endif // DOC_PRIMARY_MEDIUM

	if (GetWindow()->GetFullScreenState() == OpWindowCommander::FULLSCREEN_PROJECTION && GetHLDocProfile() && GetHLDocProfile()->HasMediaStyle(CSS_MEDIA_TYPE_PROJECTION))
		return CSS_MEDIA_TYPE_PROJECTION;

	if (GetMediaHandheldResponded() && !GetOverrideMediaStyle())
		return RespondTypeToMediaType();

	return CSS_MEDIA_TYPE_SCREEN;
}

#ifdef CONTENT_MAGIC
void FramesDocument::CheckContentMagic()
{
	if (content_magic_character_count > 60)
		content_magic_found = TRUE;
	else
	{
		content_magic_start.Reset();
		content_magic_character_count = 0;
	}
}

void FramesDocument::SetContentMagicPosition()
{
	if (g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(GetPrefsRenderingMode(), PrefsCollectionDisplay::TableMagic), GetHostName()) == 0)
	{
		// content magic was not enabled
		content_magic_position = 0;
		return;
	}

	OP_ASSERT(content_magic_found);
	OP_ASSERT(logdoc);

	if (content_magic_start.GetElm())
	{
		RECT element_rect;

		if (content_magic_start->GetLayoutBox() && logdoc->GetBoxRect(content_magic_start.GetElm(), BOUNDING_BOX, element_rect))
		{
			OpRect content_magic_rect(&element_rect);

			HTML_Element* parent_element = content_magic_start->Parent();

			while (parent_element && !parent_element->GetLayoutBox()->IsTableCell())
				parent_element = parent_element->Parent();

			RECT parent_element_rect;

			if (parent_element && logdoc->GetBoxRect(parent_element, BOUNDING_BOX, parent_element_rect))
			{
				if (parent_element_rect.top < element_rect.top)
				{
					/* Move content magic position towards the content magic element's "parent",
					   but not more than half of the layout viewport height. It could be better to
					   use something else than layout viewport height, for instance "visual
					   viewport minimum height", if such a value were available. */

					int parent_distance = element_rect.top - parent_element_rect.top;
					int max_distance = GetLayoutViewHeight() / 2;
					int distance = MIN(max_distance, parent_distance);

					content_magic_rect.y -= distance;
					content_magic_rect.height += distance;
				}
			}

			content_magic_position = content_magic_rect.y;

			if (IsTopDocument())
			{
				ViewportController* controller = GetWindow()->GetViewportController();

				content_magic_rect.x += NegativeOverflow();
				controller->GetViewportInfoListener()->OnContentMagicFound(controller, content_magic_rect);
			}
		}
	}
}
#endif // CONTENT_MAGIC

void FramesDocument::ProcessAutofocus()
{
	if (element_to_autofocus.GetElm() && doc)
	{
		FormObject* form_object = element_to_autofocus->GetFormObject();
		if (form_object)
		{
			// Must null the pointer since we might get back here from
			// SetFocus or ScrollToElement (since focus changes may
			// trigger position changes that trigger a reflow that trigger
			// this method)
			HTML_Element* elm = element_to_autofocus.GetElm();
			element_to_autofocus.Reset();

			if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::AllowAutofocusFormElement, GetServerNameFromURL(url))
				&& !navigated_in_history)
			{
				form_object->SetFocus(FOCUS_REASON_SIMULATED); // One of the types that trigger the ONFOCUS event
				//Reflow(FALSE, TRUE); // To ensure that the element's box is layouted so that we can scroll to it
				doc->ScrollToElement(elm, SCROLL_ALIGN_CENTER, FALSE, VIEWPORT_CHANGE_REASON_FORM_FOCUS);
			}
		}
	}
}

#ifdef OPERA_CONSOLE

OP_STATUS FramesDocument::EmitError(const uni_char* message_part_1, const uni_char* message_part_2)
{
	OP_ASSERT(message_part_1);

	if (g_console && g_console->IsLogging())
	{
		OpConsoleEngine::Severity severity = OpConsoleEngine::Error;
		OpConsoleEngine::Source source = OpConsoleEngine::HTML;

		OpConsoleEngine::Message cmessage(source, severity);

		// Set the window id.
		if (GetWindow())
			cmessage.window = GetWindow()->Id();

		// Set the error message itself.
		int len_part_1 = uni_strlen(message_part_1);
		int needed_len = len_part_1 + (message_part_2 ? uni_strlen(message_part_2) : 0) + 1;
		uni_char* message_p = cmessage.message.Reserve(needed_len);
		if (!message_p)
			return OpStatus::ERR_NO_MEMORY;

		uni_strcpy(message_p, message_part_1);
		if (message_part_2)
			uni_strcpy(message_p+len_part_1, message_part_2);

		// Do this after the string copy since one of the messages could be a pointer
		// to the static buffer used by URL::UniName and we don't want to destroy it.

		// Eventually it would be nice to unescape the url to make it more readable, but then we need to
		// get the decoding to do it correctly with respect to charset encodings. See bug 250545
		RETURN_IF_ERROR(GetURL().GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped,cmessage.url));

		g_console->PostMessageL(&cmessage);
	}

	return OpStatus::OK;
}
#endif // OPERA_CONSOLE

void FramesDocument::RefocusElement()
{
	if (element_to_refocus.GetElm() && doc)
	{
		FormObject* form_object = element_to_refocus->GetFormObject();
		if (form_object)
		{
			element_to_refocus.Reset();

			form_object->SetFocus(FOCUS_REASON_OTHER); // One of the types that does not trigger the ONFOCUS event

#ifdef WIDGETS_IME_SUPPORT
			// This sets the position via GetBoxRect so that the IME gets displayed at the proper location
			AffinePos dummy;
			form_object->GetPosInDocument(&dummy);

			// This is to make IME visible even if the element had an ONFOCUS event listener since
			// those shouldn't apply to the above call, see CORE-39073.
			form_object->HandleFocusEventFinished();
#endif // WIDGETS_IME_SUPPORT
		}
	}
}

BOOL FramesDocument::HasDirtyChildren()
{
	if (frm_root)
	{
		FramesDocElm* elm = (FramesDocElm *) frm_root->FirstLeaf();
		while (elm)
		{
			FramesDocument* frmDoc = (FramesDocument *) elm->GetCurrentDoc();
			if (frmDoc && frmDoc->HasDirtyChildren())
				return TRUE;
			elm = (FramesDocElm *) elm->NextLeaf();
		}
		return FALSE;
	}
	HTML_Element* docRoot = GetDocRoot();
	if (!docRoot || docRoot->IsDirty() || IsReflowing())
	{
		return TRUE;
	}
	if (ifrm_root)
	{
		FramesDocElm* elm = ifrm_root->FirstChild();
		while (elm)
		{
			FramesDocument* frmDoc = (FramesDocument *) elm->GetCurrentDoc();
			if (frmDoc && frmDoc->HasDirtyChildren())
				return TRUE;
			elm = elm->Suc();
		}
	}
	return FALSE;
}

PrefsCollectionDisplay::RenderingModes LayoutModeToRenderingMode(int layout_mode)
{
	switch (layout_mode)
	{
		case LAYOUT_SSR:
			return PrefsCollectionDisplay::SSR;
		case LAYOUT_CSSR:
			return PrefsCollectionDisplay::CSSR;
		case LAYOUT_AMSR:
			return PrefsCollectionDisplay::AMSR;
#ifdef TV_RENDERING
		case LAYOUT_TVR:
			return PrefsCollectionDisplay::TVR;
#endif // TV_RENDERING
		default:
			return PrefsCollectionDisplay::MSR;
	}
}

static BOOL ShowIFrameInSSR(const char *unfiltered, const char *url)
{
	char filtered[256]; // ARRAY OK 2008-02-28 jl

	op_strlcpy(filtered, unfiltered, 256);
	StrFilterOutChars(filtered, "^'<>@ ?|!${[*#%()=+]}");

	return op_strstr(url, filtered) != NULL;
}

BOOL FramesDocument::IsAllowedIFrame(const URL* url)
{
	const char *urlname = url ? url->GetAttribute(URL::KName_Username_Password_Hidden).CStr() : NULL;

	switch (GetShowIFrames())
	{
	case SHOW_IFRAMES_NONE:
		return FALSE;

	case SHOW_IFRAMES_SAME_SITE_AND_EXCEPTIONS:
		{
			if (!urlname)
				return FALSE; // dont show if there is no content...

			if (ShowIFrameInSSR("g%m!a%i##l.|g'oo@g!l!e$.#%c%o+m", urlname) || // allow iframes in gmail
			    ShowIFrameInSSR("w!!w%w#.goo%%g!%l#e.co'm/a++c#c<o|u<>n|ts/S#ervi#c+e!L@o##g%inB!ox", urlname) || // gmail login
			    ShowIFrameInSSR("m%s#n.c!o%%m", urlname) || // hotmail login
			    ShowIFrameInSSR("m%u!##r!s.<>163@.c%o!m", urlname)) // murs.163.com
				return TRUE;
		}
	// fall through
	case SHOW_IFRAMES_SAME_SITE:
		{
			if (!urlname)
				return FALSE; // dont show if there is no content...

			const ServerName* cur_server = GetServerNameFromURL(GetDocManager()->GetCurrentURL());
			const ServerName* ifrm_server = GetServerNameFromURL(*url);

			if (cur_server && ifrm_server && cur_server == ifrm_server)
				return TRUE;
		}
		break;

	case SHOW_IFRAMES_ALL:
		return TRUE;

	default:
		OP_ASSERT(0);
	}

	return FALSE;
}

int FramesDocument::GetShowIFrames()
{
	if (layout_mode == LAYOUT_NORMAL)
		return SHOW_IFRAMES_ALL;

	int show_iframes_mode = SHOW_IFRAMES_NONE;

	PrefsCollectionDisplay::RenderingModes rendering_mode = LayoutModeToRenderingMode(layout_mode);
	show_iframes_mode = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::ShowIFrames), GetHostName());

	return show_iframes_mode;
}

OP_STATUS FramesDocument::OnlineModeChanged()
{
	BOOL is_online = GetWindow()->GetOnlineMode() == Window::ONLINE;

	if (frm_root)
		frm_root->OnlineModeChanged();
	else
	{
		if (is_online)
		{
			LoadInlineElm *iterator = (LoadInlineElm*) waiting_for_online_mode.First();
			while (iterator)
			{
				iterator->Out();

				OP_STATUS status = inline_hash.Add(iterator);
				if (OpStatus::IsError(status))
				{
					OP_DELETE(iterator);
					return status;
				}

				HEListElm* helm = iterator->GetFirstHEListElm();
				OP_ASSERT(helm);
				while (helm)
				{
					LocalLoadInline(iterator->GetUrl(), helm->GetInlineType(), helm, helm->HElm());
					helm = helm->Suc();
				}

				iterator = (LoadInlineElm*) waiting_for_online_mode.First();
			}
		}
		else
			waiting_for_online_mode.Clear();

		if (ifrm_root)
			RETURN_IF_ERROR(ifrm_root->OnlineModeChanged());
	}

	if (logdoc && doc && !local_doc_finished)
	{
		logdoc->SetCompleted(!logdoc->GetRoot()->IsDirty(), IsPrintDocument());
		RETURN_IF_ERROR(CheckFinishDocument());
	}

	return HandleWindowEvent(is_online ? ONONLINE : ONOFFLINE);
}

OP_STATUS
FramesDocument::ReparseAsHtml(BOOL from_message)
{
	if (logdoc && logdoc->IsParsed() && logdoc->IsXmlParsingFailed())
		if (!from_message)
		{
			MessageHandler *mh = GetMessageHandler();
			mh->SetCallBack(this, MSG_REPARSE_AS_HTML, 0);
			mh->PostMessage(MSG_REPARSE_AS_HTML, 0, 0);
		}
		else
		{
			RETURN_IF_ERROR(ReloadedUrl(url, origin, FALSE));
			parse_xml_as_html = TRUE;
			RETURN_IF_ERROR(CheckSource());
			if (!IsLoaded())
				GetDocManager()->SetLoadStatus(DOC_CREATED);
		}

	return OpStatus::OK;
}

void
FramesDocument::PostDelayedActionMessage(int delay_ms /* = 0 */)
{
	MessageHandler* mh = GetMessageHandler();
	if (mh->HasCallBack(this, MSG_DOC_DELAYED_ACTION, 0) ||
	    OpStatus::IsSuccess(mh->SetCallBack(this, MSG_DOC_DELAYED_ACTION, 0)))
	{
		GetDocManager()->PostDelayedActionMessage(delay_ms);
	}
}

OP_STATUS
FramesDocument::RestoreState()
{
	OP_NEW_DBG("FramesDocument::RestoreState", "doc");
	OP_DBG((UNI_L("this=%p; document_state=%p"), this, document_state));
	if (document_state)
	{
		RETURN_IF_ERROR(document_state->Restore(this));
		RETURN_IF_ERROR(RestoreFormState(TRUE));
	}

	return OpStatus::OK;
}

class DocumentRestoreFormStateThread
	: public ES_Thread
{
protected:
	FramesDocument *doc;

public:
	DocumentRestoreFormStateThread(FramesDocument *doc)
		: ES_Thread(NULL),
		  doc(doc)
	{
	}

	virtual OP_STATUS EvaluateThread();
	virtual OP_STATUS Signal(ES_ThreadSignal signal);
};

/* virtual */ OP_STATUS
DocumentRestoreFormStateThread::EvaluateThread()
{
	RETURN_IF_ERROR(doc->RestoreFormState());

	if (!IsBlocked())
		is_completed = TRUE;

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DocumentRestoreFormStateThread::Signal(ES_ThreadSignal signal)
{
	switch (signal)
	{
	case ES_SIGNAL_FINISHED:
	case ES_SIGNAL_FAILED:
	case ES_SIGNAL_CANCELLED:
		doc->ResetRestoreFormStateThread();
	}

	return ES_Thread::Signal(signal);
}

OP_STATUS
FramesDocument::RestoreFormState(BOOL initial)
{
	OP_NEW_DBG("FramesDocument::RestoreFormState", "doc");
	if (document_state)
	{
		OP_DBG((UNI_L("this=%p; document_state=%p"), this, document_state));
		if (initial && dom_environment && dom_environment->IsEnabled())
			RETURN_IF_ERROR(CreateRestoreFormStateThread());
		else
		{
			OP_BOOLEAN paused;

			RETURN_IF_ERROR(paused = document_state->RestoreForms(this));

			if (paused == OpBoolean::IS_FALSE)
			{
				OP_DELETE(document_state);
				document_state = NULL;
			}
		}
	}

	return OpStatus::OK;
}

OP_BOOLEAN
FramesDocument::SignalFormValueRestored(HTML_Element *element)
{
	if (FramesDocument::NeedToFireEvent(this, element, NULL, ONCHANGE))
	{
		RETURN_IF_ERROR(CreateRestoreFormStateThread());

		if (restore_form_state_thread)
		{
			OP_STATUS status = HandleEvent(ONCHANGE, NULL, element, SHIFTKEY_NONE, 0, restore_form_state_thread);

			if (OpStatus::IsMemoryError(status))
				return status;

			return restore_form_state_thread->IsBlocked() ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
		}
	}

	return OpBoolean::IS_FALSE;
}

OP_STATUS
FramesDocument::CreateRestoreFormStateThread()
{
	if (!restore_form_state_thread)
	{
		if (OpStatus::IsMemoryError(ConstructDOMEnvironment()))
			return OpStatus::ERR_NO_MEMORY;

		if (dom_environment && dom_environment->IsEnabled())
		{
			DocumentRestoreFormStateThread *thread = OP_NEW(DocumentRestoreFormStateThread, (this));

			if (!thread)
				return OpStatus::ERR_NO_MEMORY;

			OP_BOOLEAN added = dom_environment->GetScheduler()->AddRunnable(thread);

			if (added == OpBoolean::IS_TRUE)
				restore_form_state_thread = thread;
			else if (OpStatus::IsMemoryError(added))
				return OpStatus::ERR_NO_MEMORY;
		}
	}

	return OpStatus::OK;
}

BOOL
FramesDocument::IsRestoringFormState(ES_Thread *thread)
{
	if (thread && restore_form_state_thread && thread->IsOrHasInterrupted(restore_form_state_thread))
		return TRUE;

	FramesDocElm *frames;

	if ((frames = frm_root) || (frames = ifrm_root))
	{
		frames = (FramesDocElm *) frames->FirstLeaf();
		while (frames)
		{
			if (FramesDocument *doc = (FramesDocument *) frames->GetCurrentDoc())
				if (doc->IsRestoringFormState(thread))
					return TRUE;

			frames = (FramesDocElm *) frames->NextLeaf();
		}
	}

	return FALSE;
}

void
FramesDocument::SignalFormChangeSideEffect(ES_Thread *thread)
{
	if (thread)
	{
		while (ES_Thread *interrupted_thread = thread->GetInterruptedThread())
			thread = interrupted_thread;

		ES_ThreadInfo info = thread->GetInfo();

		if (info.type == ES_THREAD_EVENT && info.data.event.type == ONCHANGE)
		{
			DOM_Object *target = DOM_Utils::GetEventTarget(thread);
			HTML_Element *element = DOM_Utils::GetHTML_Element(target);

			if (element && element->IsFormElement())
				element->GetFormValue()->DoNotSignalValueRestored();
		}
	}
}

void
FramesDocument::SignalESResting()
{
	if (remove_from_cache)
		PostDelayedActionMessage();
	else if (IsCurrentDoc())
	{
		override_es_active = TRUE;

		BOOL is_es_active = GetTopDocument()->IsESActive(TRUE);

		if (!is_es_active)
		{
			DocumentTreeIterator iter(GetWindow());

			iter.SetIncludeThis();

			double new_delay_target = g_op_time_info->GetRuntimeMS() + GetReflowMsgDelay();

			while (iter.Next())
			{
				// If documents have long reflow timers (for reflows during scripts)
				// we will move them to short reflow timers (for reflows outside
				// of scripts)
				FramesDocument *doc = iter.GetFramesDocument();
				if (doc->is_reflow_msg_posted && doc->reflow_msg_delayed_to_time > new_delay_target)
				{
					doc->is_reflow_msg_posted = FALSE;
					doc->GetMessageHandler()->RemoveDelayedMessage(MSG_REFLOW_DOCUMENT, doc->GetSubWinId(), 0);
					doc->PostReflowMsg();

					// We need to end activity that was registered on the reflow we just removed with
					// RemoveDelayedMessage(MSG_REFLOW_DOCUMENT... Since PostReflowMsg registers a new reflow
					// message, and registers a new activity, the activity counter goes from 1 to 2 to 1, and
					// the it goes to 0 when the posted reflow is finally executed.
					activity_reflow.End();
				}
				if (doc->has_delayed_updates)
				{
					doc->has_delayed_updates = FALSE;
					if (iter.GetDocumentManager()->GetLoadStatus() != DOC_CREATED)
						doc->GetVisualDevice()->SyncDelayedUpdates();
				}
			}
		}

		override_es_active = FALSE;
	}
}

HistoryNavigationMode
FramesDocument::CalculateHistoryNavigationMode()
{
	HistoryNavigationMode mode = override_history_navigation_mode;

	if (mode == HISTORY_NAV_MODE_AUTOMATIC)
		mode = (HistoryNavigationMode) g_pcdoc->GetIntegerPref(PrefsCollectionDoc::HistoryNavigationMode, GetHostName());

	if (mode == HISTORY_NAV_MODE_AUTOMATIC)
		mode = use_history_navigation_mode;

	if (mode == HISTORY_NAV_MODE_AUTOMATIC)
		mode = compatible_history_navigation_needed ? HISTORY_NAV_MODE_COMPATIBLE : HISTORY_NAV_MODE_FAST;

	return mode;
}

void
FramesDocument::RemoveFromCache()
{
	remove_from_cache = TRUE;
	PostDelayedActionMessage();
}

void
FramesDocument::OnVisibilityChanged()
{
	// No need to send the event if there is no environment since in that case there
	// can't be any event listeners since this event isn't registered through HTML.
	if (dom_environment)
		HandleDocumentEvent(ONVISIBILITYCHANGE);
}

#ifdef SUPPORT_VISUAL_ADBLOCK
BOOL
FramesDocument::GetIsURLBlocked(const URL& url)
{
	if(GetWindow()->GetContentBlockEditMode())
	{
		LoadInlineElm* elm = GetInline(url.Id());

		if(elm)
			return elm->GetContentBlockRendered();
	}
	return FALSE;
}

void
FramesDocument::AddToURLBlockedList(const URL& url)
{
	LoadInlineElm* elm = GetInline(url.Id());

	if(elm)
		elm->SetContentBlockRendered(TRUE);
}

void
FramesDocument::RemoveFromURLBlockedList(const URL& url)
{
	LoadInlineElm* elm = GetInline(url.Id());

	if(elm)
		elm->SetContentBlockRendered(FALSE);
}

void FramesDocument::ClearURLBlockedList(BOOL invalidate)
{
	LoadInlineElmHashIterator iterator(inline_hash);
	for (LoadInlineElm* lie = iterator.First(); lie; lie = iterator.Next())
	{
		if(invalidate && lie->GetContentBlockRendered())
		{
			BOOL was_image = FALSE;

			for (HEListElm *helm = lie->GetFirstHEListElm(); helm; helm = helm->Suc())
			{
				switch (helm->GetInlineType())
				{
				case IMAGE_INLINE:
				case BGIMAGE_INLINE:
				case BORDER_IMAGE_INLINE:
				case EXTRA_BGIMAGE_INLINE:
				case EMBED_INLINE:
					// Force recreation of the layout box; that will trigger a call
					// to FramesDocument::LoadInline that will load the URL.
					helm->HElm()->MarkExtraDirty(this);
					was_image = TRUE;
				}
			}
			if (was_image)
			{
				RemoveLoadInlineElm(lie);
				OP_DELETE(lie);
				continue;
			}
		}
		lie->SetContentBlockRendered(FALSE);
	}
}

#endif // SUPPORT_VISUAL_ADBLOCK

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

void FramesDocument::RemoveAccesibilityHookToElement(HTML_Element* element)
{
	VisualDevice* vd = GetVisualDevice();
	if (vd && vd->GetAccessibleDocument())
		vd->GetAccessibleDocument()->ElementRemoved(this, element);
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

short FramesDocument::GetMaxParagraphWidth()
{
	if (!GetWindow()->GetLimitParagraphWidth())
		return -1;

	int width = GetWindow()->GetViewportController()->GetMaxParagraphWidth();

	if (width < 0)
	{
		// Fallback to 95% of the window width
		unsigned int tmp_width, dummyheight;
		GetWindow()->GetCSSViewportSize(tmp_width, dummyheight);
		width = tmp_width * 95 / 100;
	}

	return MAX(80, width);
}

void FramesDocument::MarkContainersDirty()
{
	if (GetDocRoot())
		GetDocRoot()->MarkContainersDirty(this);

	FramesDocElm *fde = frm_root ? frm_root : ifrm_root;

	while (fde)
	{
		if (FramesDocument* doc = fde->GetCurrentDoc())
			doc->MarkContainersDirty();
		fde = (FramesDocElm *) fde->Next();
	}
}

BOOL FramesDocument::CheckOverrideMediaStyle()
{
	const char filterChars[] = "^'`<>@ ?|!${[*#%()=+]}";

	const char *dont_use_handheld_urls[6];
	dont_use_handheld_urls[0] = "m@s$n.c'o(m";
	dont_use_handheld_urls[1] = "d@ag#b%la@de#t.n%o";
	dont_use_handheld_urls[2] = "x#h<t*m#l.e<x#*pr*e#s*s<*en.*<s#e";
	dont_use_handheld_urls[3] = "f(+or'b+r)uk+er.(n<o";
	dont_use_handheld_urls[4] = "<v#g'.%n+o";
	dont_use_handheld_urls[5] = NULL;

	URL& url = GetURL();

	int i = 0;

	while (dont_use_handheld_urls[i])
	{
		char filter_domain[128]; // ARRAY OK 2008-02-28 jl

		op_strlcpy(filter_domain, dont_use_handheld_urls[i], 128);

		StrFilterOutChars(filter_domain, filterChars);

		if (url.GetAttribute(URL::KName_Username_Password_Hidden).Find(filter_domain) != KNotFound)
			return TRUE;

		i++;
	}

	return FALSE;
}

#ifdef VISITED_PAGES_SEARCH
void
FramesDocument::CloseVisitedSearchHandle()
{
	if (m_visited_search != NULL)
	{
		OpStatus::Ignore(g_visited_search->CloseRecord(m_visited_search));
		m_visited_search = NULL;
	}
}
#endif // VISITED_PAGES_SEARCH

void
FramesDocument::SetSpotlightedElm(HTML_Element *elm)
{
	if (m_spotlight_elm.GetElm())
		m_spotlight_elm->MarkDirty(this, FALSE, TRUE);

	HTML_Element *candidate = elm;
	while (candidate && candidate->IsText())
		candidate = candidate->Parent();

	m_spotlight_elm.SetElm(candidate);

	if (m_spotlight_elm.GetElm())
	{
		m_spotlight_elm->MarkDirty(this, FALSE, TRUE);
	}
}

void FramesDocument::BaseSetAsCurrentDoc(BOOL state)
{
	if (state)
	{
		if (!IsCurrentDoc())
		{
			SetCurrentDocState(TRUE);

			if (!(url_in_use.GetURL() == url))
				url_in_use.SetURL(url);

			SetInlinesUsed(TRUE); // redundant call? Check FramesDocument::SetAsCurrentDoc
			g_memory_manager->DisplayedDoc(this);

#ifndef MOUSELESS
			GetWindow()->SetCursor(CURSOR_DEFAULT_ARROW, TRUE);
#endif // !MOUSELESS
		}
	}
	else if (IsCurrentDoc())
	{
#ifndef MOUSELESS
		is_moving_separator = FALSE;
#endif // !MOUSELESS
#ifndef HAS_NOTEXTSELECTION
		is_selecting_text = FALSE;
		selecting_text_is_ok = FALSE;
		start_selection = FALSE;
# if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
		GetWindow()->ResetSearch();
# endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT
#endif // !HAS_NOTEXTSELECTION

		SetCurrentDocState(FALSE);
		if (url_in_use.GetURL().Type() != URL_FILE)
			url_in_use.UnsetURL();
		SetInlinesUsed(FALSE); // redundant call? Check FramesDocument::SetAsCurrentDoc

		OpStatus::Ignore(g_memory_manager->UndisplayedDoc(this,
#ifdef DOCUMENT_EDIT_SUPPORT
						(GetDocumentEdit() != NULL)
#else // DOCUMENT_EDIT_SUPPORT
						FALSE
#endif // DOCUMENT_EDIT_SUPPORT
						));
	}
}

#ifdef WEBFEEDS_DISPLAY_SUPPORT
void FramesDocument::OnFeedLoaded(OpFeed* feed, OpFeed::FeedStatus status)
{
	if (feed && !OpFeed::IsFailureStatus(status))
	{
		// We're going to load a generated page. This works by setting the
		// is_inline_feed in the FramesDocument created by a load which will
		// make the generated page load instead of the one we request.
		// This is far from a a good solution but it's the best one we got

		DocumentManager* docman = GetDocManager();
		URL output_url = feed->GetURL();
		URL dummy_url;

		// We don't actually want anything to load from the network. If things
		// start loading from the network we will get a mess of generated data
		// and page load messages.
		// To prevent reloading the page we hide the potentially dangerous cache policy.
		BOOL cache_policy = !!output_url.GetAttribute(URL::KCachePolicy_AlwaysVerify);
		output_url.SetAttribute(URL::KCachePolicy_AlwaysVerify, FALSE);

		// Make sure this doesn't take a new history position
		DocumentManager::OpenURLOptions open_url_options;
		open_url_options.entered_by_user = WasEnteredByUser; // que?
		open_url_options.es_terminated = TRUE; // que?
		open_url_options.bypass_url_access_check = TRUE;
		open_url_options.is_inline_feed = TRUE;

		docman->SetUseHistoryNumber(docman->CurrentDocListElm()->Number());
		docman->OpenURL(output_url, DocumentReferrer(dummy_url), FALSE /* check_if_expired */, FALSE /* reload */, open_url_options);

		// Restore the url to its original state
		output_url.SetAttribute(URL::KCachePolicy_AlwaysVerify, cache_policy);
	}
}
#endif // WEBFEEDS_DISPLAY_SUPPORT

#if !defined(MOUSELESS)
/* static */
BOOL FramesDocument::CheckMovedTooMuchForClick(const OpPoint& current_pos, const OpPoint& last_down_position)
{
	BOOL is_click = TRUE;
	int delta_x = op_abs(current_pos.x - last_down_position.x);
	int delta_y = op_abs(current_pos.y - last_down_position.y);

	// Smart code borrowed from the mouse gesture system. Since we only need it
	// to be approximately accurate for values close to the treshold we can
	// use this.

	// Simplified pythagoras sqrt(a^2+b^2) ~ max(a,b) + min(a,b)/2
	// Accurate +- 1 pixel up to abaout a dozen pixel's length
	// Should be good enough, if not, then  max(a,b) + (min(a,b) * 3)/8
	// is yet a tad better, but IMO simple is best. steink
	int big_dist;
	int small_dist;
	if (delta_x > delta_y)
	{
		big_dist = delta_x;
		small_dist = delta_y;
	}
	else
	{
		big_dist = delta_y;
		small_dist = delta_x;
	}

	int approx_distance = big_dist + (small_dist >> 1);

	// This pref seems to be as good threshold as any. Smaller movements might be clicks,
	// larger may be mouse gestures
	int threshold = g_pccore->GetIntegerPref(PrefsCollectionCore::GestureThreshold);

	if (approx_distance > threshold)
		is_click = FALSE;

	return is_click;
}

void FramesDocument::MouseUp(int mouse_x, int mouse_y, BOOL shift_pressed, BOOL control_pressed, BOOL alt_pressed, MouseButton button, int sequence_count)
{
	if (m_mouse_up_may_still_be_click)
	{
		OpPoint current_pos = GetVisualDevice()->ScaleToScreen(mouse_x, mouse_y);
		m_mouse_up_may_still_be_click = CheckMovedTooMuchForClick(current_pos,
			OpPoint(m_last_mouse_down_position_non_scaled.x, m_last_mouse_down_position_non_scaled.y));
	}
	int view_x = GetVisualDevice()->GetRenderingViewX();
	int view_y = GetVisualDevice()->GetRenderingViewY();
	int document_x = mouse_x + view_x;
	int document_y = mouse_y + view_y;

#ifndef HAS_NOTEXTSELECTION
	if (!is_selecting_text)
#endif
	{
		if (is_moving_separator)
		{
			EndMoveFrameSeparator(mouse_x, mouse_y);
			is_moving_separator = FALSE;
			return;
		}
#ifdef _PRINT_SUPPORT_
		else
			if (GetWindow() && GetWindow()->GetPreviewMode())
				return;
#endif // _PRINT_SUPPORT_
	}

	int sequence_count_and_button = MAKE_SEQUENCE_COUNT_CLICK_INDICATOR_AND_BUTTON(sequence_count, m_mouse_up_may_still_be_click, button);
	MouseAction(ONMOUSEUP, document_x, document_y, GetVisualViewX(), GetVisualViewY(), sequence_count_and_button, shift_pressed, control_pressed, alt_pressed);
}

void FramesDocument::MouseDown(int mouse_x, int mouse_y, BOOL shift_pressed, BOOL control_pressed, BOOL alt_pressed, MouseButton button, int sequence_count)
{
	m_mouse_up_may_still_be_click = TRUE;
	m_last_mouse_down_position_non_scaled = GetVisualDevice()->ScaleToScreen(mouse_x, mouse_y);
	GetWindow()->SetHasShownUnsolicitedDownloadDialog(FALSE); // Allow a new download dialog after each click

	int sequence_count_and_button_or_delta = MAKE_SEQUENCE_COUNT_AND_BUTTON(sequence_count, button);
	int view_x = GetVisualDevice()->GetRenderingViewX();
	int view_y = GetVisualDevice()->GetRenderingViewY();

	switch (MouseAction(ONMOUSEDOWN, mouse_x + view_x, mouse_y + view_y, GetVisualViewX(), GetVisualViewY(), sequence_count_and_button_or_delta, shift_pressed, control_pressed, alt_pressed))
	{
	case DOC_ACTION_DRAG_SEP:
		StartMoveFrameSeparator(mouse_x, mouse_y);
		is_moving_separator = TRUE;
		break;
	}
}

void FramesDocument::MouseMove(int mouse_x, int mouse_y, BOOL shift_pressed, BOOL control_pressed, BOOL alt_pressed, BOOL left_button, BOOL middle_button, BOOL right_button)
{
#ifdef DRAG_SUPPORT
	// The input events (mouse, keyboard, touch) must be suppressed while dragging.
	if (g_drag_manager->IsDragging() && !g_drag_manager->IsBlocked())
		return;
#endif // DRAG_SUPPORT

	if(m_mouse_up_may_still_be_click)
	{
		OpPoint current_pos = GetVisualDevice()->ScaleToScreen(mouse_x, mouse_y);
		m_mouse_up_may_still_be_click = CheckMovedTooMuchForClick(current_pos,
										OpPoint(m_last_mouse_down_position_non_scaled.x, m_last_mouse_down_position_non_scaled.y));
	}
	int view_x = GetVisualDevice()->GetRenderingViewX();
	int view_y = GetVisualDevice()->GetRenderingViewY();
	int document_x = mouse_x + view_x;
	int document_y = mouse_y + view_y;

	MouseAction(ONMOUSEMOVE, document_x, document_y, GetVisualViewX(), GetVisualViewY(), MOUSE_BUTTON_1, shift_pressed, control_pressed, alt_pressed);

#ifndef HAS_NOTEXTSELECTION
	selecting_text_is_ok = TRUE;
#endif

	if (left_button)
	{
#ifndef HAS_NOTEXTSELECTION
		Window* window = GetWindow();

		if (start_selection && (start_selection_x != document_x || start_selection_y != document_y))
		{
			start_selection = FALSE;
			OP_STATUS retval = StartSelection(start_selection_x, start_selection_y);
			if (retval == OpStatus::ERR_NO_MEMORY)
				window->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			else
			{
				is_selecting_text = TRUE;
				selecting_text_is_ok = FALSE;
			}
		}
		if (is_selecting_text)
		{
			MoveSelectionFocusPoint(document_x, document_y, FALSE);

#ifdef GRAB_AND_SCROLL
			if (!window->GetScrollIsPan())
#endif // GRAB_AND_SCROLL
			{
				// Start selectionscroll
				if (!selection_scroll_active)
				{
					selection_scroll_active = TRUE;
					g_opera->doc_module.m_selection_scroll_document = static_cast<FramesDocument*>(this);
					window->GetMessageHandler()->PostMessage(MSG_SELECTION_SCROLL, 0, 0);
				}
			}
		}
		else
#endif // !HAS_NOTEXTSELECTION
			if (is_moving_separator)
				MoveFrameSeparator(mouse_x, mouse_y);
	}
}

void FramesDocument::MouseWheel(int mouse_x, int mouse_y, BOOL shift_pressed, BOOL control_pressed, BOOL alt_pressed, int delta, BOOL vertical)
{
	int view_x = GetVisualDevice()->GetRenderingViewX();
	int view_y = GetVisualDevice()->GetRenderingViewY();
	MouseAction(vertical ? ONMOUSEWHEELV : ONMOUSEWHEELH, mouse_x + view_x, mouse_y + view_y,  GetVisualViewX(), GetVisualViewY(), delta, shift_pressed, control_pressed, alt_pressed);
}

#endif // !MOUSELESS

#ifdef TOUCH_EVENTS_SUPPORT
void FramesDocument::TouchAction(DOM_EventType type, int id, int x, int y, int visual_viewport_x, int visual_viewport_y, int radius, BOOL shift_pressed, BOOL control_pressed, BOOL alt_pressed, void* user_data)
{
#ifdef DRAG_SUPPORT
	// The input events (mouse, keyboard, touch) must be suppressed while dragging.
	if (g_drag_manager->IsDragging() && !g_drag_manager->IsBlocked())
		return;
#endif // DRAG_SUPPORT

	if (doc)
	{
		if (type == TOUCHSTART)
		{
			m_touch_up_may_still_be_click = TRUE;
			m_last_touch_down_position_non_scaled = GetVisualDevice()->ScaleToScreen(x, y);
		}
		else if (type == TOUCHMOVE || type == TOUCHEND)
		{
			if (m_touch_up_may_still_be_click)
			{
				OpPoint current_pos = GetVisualDevice()->ScaleToScreen(x, y);
				m_touch_up_may_still_be_click = CheckMovedTooMuchForClick(current_pos,
								OpPoint(m_last_touch_down_position_non_scaled.x, m_last_touch_down_position_non_scaled.y));
			}
		}
		int doc_x = x + GetVisualDevice()->GetRenderingViewX();
		int doc_y = y + GetVisualDevice()->GetRenderingViewY();

		int sequence_count_and_button = MAKE_SEQUENCE_COUNT_CLICK_INDICATOR_AND_BUTTON(1, m_touch_up_may_still_be_click, MOUSE_BUTTON_1);
		doc->TouchAction(type, id, doc_x, doc_y, visual_viewport_x, visual_viewport_y, radius, sequence_count_and_button, shift_pressed, control_pressed, alt_pressed, user_data);
	}
}
#endif // TOUCH_EVENTS_SUPPORT

#ifdef DOC_RETURN_TOUCH_EVENT_TO_SENDER
BOOL FramesDocument::SignalTouchEventProcessed(void* user_data, BOOL prevented)
{
	WindowCommander* wc = GetWindow()->GetWindowCommander();
	return wc->GetDocumentListener()->OnTouchEventProcessed(wc, user_data, prevented);
}
#endif // DOC_RETURN_TOUCH_EVENT_TO_SENDER

int FramesDocument::GetFirstInvalidCharacterOffset()
{
	if (url_data_desc && current_buffer)
	{
		int first_invalid_character_offset = url_data_desc->GetFirstInvalidCharacterOffset();
		if (first_invalid_character_offset != -1)
		{
			OP_ASSERT(stream_position_base < (unsigned) first_invalid_character_offset);
			return url_data_desc->GetFirstInvalidCharacterOffset() - stream_position_base;
		}
	}

	return -1;
}

BOOL FramesDocument::GetSuppress(URLType url_type)
{
	BOOL result = FALSE;
#ifdef _MIME_SUPPORT_
	Window *w = GetWindow();

	/* Normally Generated by Opera would indicate trusted content,
	 * not so obvious if inlines are from external content as would
	 * be the case for mail compose. */
	if (!IsGeneratedByOpera() || w->GetType() == WIN_TYPE_MAIL_COMPOSE)
	{
		if (w->GetType() == WIN_TYPE_NEWSFEED_VIEW)
			return result;
		

		// for MIME only allow access to the other parts of the MIME message and data URIs
		BOOL is_allowed_type_in_mime = url_type == URL_ATTACHMENT
			                        || url_type == URL_CONTENT_ID
			                        || url_type == URL_DATA;
		BOOL parent_disallows = !url.IsEmpty()
			&& (url.GetAttribute(URL::KSuppressScriptAndEmbeds, TRUE) == MIME_EMail_ScriptEmbed_Restrictions
				|| (url.Type() == URL_ATTACHMENT && !is_allowed_type_in_mime));


		if ((w && w->IsSuppressWindow()) || parent_disallows)
		{
			WindowCommander *wc;
			BOOL has_permission = FALSE;
	
			if (wc = w->GetWindowCommander())
				wc->GetMailClientListener()->GetExternalEmbedPermission(wc, has_permission);
				
			if (has_permission)
				result = FALSE;
			else
				result = g_url_api->EmailUrlSuppressed(url_type) ;

			if (result && wc)					
				wc->GetMailClientListener()->OnExternalEmbendBlocked(wc);
			
		}
	}
#endif // _MIME_SUPPORT_

	return result;
}

const uni_char* FramesDocument::GetHostName()
{
	return url.GetAttribute(URL::KUniHostName).CStr();
}

BOOL FramesDocument::IsTopDocument()
{
	return GetDocManager()->GetFrame() == NULL;
}

FramesDocument* FramesDocument::GetParentDoc()
{
	return GetDocManager()->GetParentDoc();
}

MessageHandler* FramesDocument::GetMessageHandler()
{
	return GetDocManager()->GetMessageHandler();
}

void FramesDocument::SetFocus(FOCUS_REASON reason /*= FOCUS_REASON_OTHER*/)
{
	GetVisualDevice()->SetFocus(reason);
}

OpDocumentContext* FramesDocument::CreateDocumentContext()
{
	FramesDocument* target_doc = GetActiveSubDoc();
	if (!target_doc)
		target_doc = this;

	HTML_Element* target = target_doc->GetFocusElement();
	VisualDevice* vd = target_doc->GetVisualDevice();

	OpPoint document_pos(0,0);
	OpPoint screen_pos(0,0);

	if (target)
	{
		OpPoint offset(0,0);

		target->GetCaretCoordinate(target_doc, document_pos.x, document_pos.y, offset.x, offset.y);

		OpPoint screen_coords = vd->ScaleToScreen(OpPoint(document_pos.x, document_pos.y));
		OpPoint view_coords = vd->OffsetToContainerAndScroll(screen_coords);
		screen_pos = vd->GetOpView()->ConvertToScreen(view_coords);
	}
	else
	{
		HLDocProfile* hld_profile = target_doc->GetHLDocProfile();
		if (hld_profile)
			target = hld_profile->GetBodyElm();

		if (!target)
			target = target_doc->GetDocRoot();

		screen_pos = vd->OffsetToContainer(OpPoint(vd->VisibleWidth()/2, vd->VisibleHeight()/2) + vd->GetPosOnScreen());
		document_pos = vd->ScaleToDoc(screen_pos);
	}

	return target_doc->CreateDocumentContextForElement(target, document_pos, screen_pos);
}

OpDocumentContext* FramesDocument::CreateDocumentContextForElement(HTML_Element* target,
                                                                   const OpPoint& document_pos,
                                                                   const OpPoint& screen_pos)
{
	OpView* view = GetVisualDevice()->GetOpView();
	OpWindow* window = const_cast<OpWindow*>(GetWindow()->GetOpWindow());
	DocumentInteractionContext* ctx = OP_NEW(DocumentInteractionContext, (this, screen_pos, view, window, target));
	if (ctx)
	{
		OP_STATUS status;
		if (target)
			status = PopulateInteractionContext(*ctx, target, 0, 0, document_pos.x, document_pos.y);
		else
		{
			OP_ASSERT(m_document_contexts_in_use.GetCount() <= 10); // There should be like 0 or 1 of these outstanding. If they get more we will have to rethink stuff.
			status = m_document_contexts_in_use.Add(ctx);
		}

		if (OpStatus::IsError(status))
		{
			OP_DELETE(ctx);
			ctx = NULL;
		}
	}
	return ctx;
}

OpDocumentContext* FramesDocument::CreateDocumentContextForScreenPos(const OpPoint& screen_pos)
{
	FramesDocument* target_doc;
	HTML_Element* target;
	OpPoint document_pos;

	target = GetWindow()->GetViewportController()->FindElementAtScreenPosAndTranslate(screen_pos, target_doc, document_pos);

	if (!target_doc)
	{
		target_doc = this;
		document_pos.x = 0;
		document_pos.y = 0;
	}

	return target_doc->CreateDocumentContextForElement(target, document_pos, screen_pos);
}

OpDocumentContext* FramesDocument::CreateDocumentContextForDocPos(const OpPoint& doc_pos, HTML_Element* target /*= NULL*/)
{
	OpPoint screen_point = GetVisualDevice()->ConvertToScreen(doc_pos);

	if (!target)
	{
		HTML_Element* root = GetLogicalDocument() ? GetLogicalDocument()->GetRoot() : NULL;
		if (root)
		{
			Box* box = root->GetInnerBox(doc_pos.x, doc_pos.y, this);
			if (box)
				target = box->GetHtmlElement();
		}
	}

	return CreateDocumentContextForElement(target, doc_pos, screen_point);
}

void FramesDocument::UnregisterDocumentInteractionCtx(DocumentInteractionContext* ctx)
{
	m_document_contexts_in_use.RemoveByItem(ctx); // O(n) in number of objects so this assumes there are very few contexts
}

OP_STATUS FramesDocument::RegisterDocumentInteractionCtx(DocumentInteractionContext* ctx)
{
	OP_ASSERT(m_document_contexts_in_use.Find(ctx) == -1);
	OP_ASSERT(m_document_contexts_in_use.GetCount() < 10 || !"Now something must be wrong. I expect 1-2 of these to be live at any one time, not more than 10");
	return m_document_contexts_in_use.Add(ctx);
}

void FramesDocument::RequestContextMenu(DocumentInteractionContext& context)
{
#ifdef _POPUP_MENU_SUPPORT_
	if (OpPopupMenuRequestMessage* menu_request = OpPopupMenuRequestMessage::Create())
	{
		OP_STATUS status = GetDocumentMenu(menu_request->GetDocumentMenuItemListRef(), &context);
		OP_ASSERT(OpStatus::IsSuccess(status) || OpStatus::IsMemoryError(status));
		if (OpStatus::IsMemoryError(status))
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);

		// GetDocumentMenu OOM is not a critical condition - at most we wont display additional menu items
		// We can still try to continue here.

		Window* window = GetWindow();
		// TODO in multi process this should be done by sending a message to the channel.
		window->GetWindowCommander()->GetMenuListener()->OnPopupMenu(window->GetWindowCommander(), context, menu_request);
		OP_DELETE(menu_request);
	}
	else
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
#endif // _POPUP_MENU_SUPPORT_
}

OP_STATUS
FramesDocument::GetDocumentMenu(OpWindowcommanderMessages_MessageSet::PopupMenuRequest::MenuItemList& menu_items, DocumentInteractionContext* document_context)
{
#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
	// @note that in multi process phase 2 extension manager will be in different
	// component so this operation will be asynchronous.
	RETURN_IF_ERROR(DOM_Utils::AppendExtensionMenu(menu_items, document_context, document_context->GetDocument(), document_context->GetHTMLElement()));
#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

	return OpStatus::OK;
}

void FramesDocument::InitiateContextMenu()
{
#ifdef _POPUP_MENU_SUPPORT_
	FramesDocument* target_doc = GetActiveSubDoc();
	if (!target_doc)
		target_doc = this;
	HTML_Element* target = target_doc->GetFocusElement();
	VisualDevice* vd = target_doc->GetVisualDevice();

	OpPoint document_pos(0,0);
	OpPoint screen_pos(0,0);
	OpPoint offset(0,0);
	if (target)
	{
		target->GetCaretCoordinate(target_doc, document_pos.x, document_pos.y, offset.x, offset.y);

		OpPoint screen_coords = vd->ScaleToScreen(OpPoint(document_pos.x, document_pos.y));
		OpPoint view_coords = vd->OffsetToContainerAndScroll(screen_coords);
		screen_pos = vd->GetOpView()->ConvertToScreen(view_coords);
	}
	else
	{
		HLDocProfile* hld_profile = target_doc->GetHLDocProfile();
		if (hld_profile)
			target = hld_profile->GetBodyElm();

		if (!target)
			target = target_doc->GetDocRoot();

		screen_pos = vd->OffsetToContainer(OpPoint(vd->VisibleWidth()/2, vd->VisibleHeight()/2) + vd->GetPosOnScreen());
		document_pos = vd->ScaleToDoc(screen_pos);
	}

	DocumentInteractionContext ctx(target_doc, screen_pos, target_doc->GetVisualDevice()->GetOpView(), const_cast<OpWindow*>(GetWindow()->GetOpWindow()), target);
	if (OpStatus::IsSuccess(PopulateInteractionContext(ctx, target, offset.x, offset.y, document_pos.x, document_pos.y)))
		RequestContextMenu(ctx);
#endif // _POPUP_MENU_SUPPORT_
}

void FramesDocument::InitiateContextMenu(const OpPoint& screen_pos)
{
#ifdef _POPUP_MENU_SUPPORT_
	FramesDocument* target_doc;
	HTML_Element* target;
	OpPoint document_pos(0, 0);
	OpPoint offset_pos(0, 0);

	target = GetWindow()->GetViewportController()->FindElementAtScreenPosAndTranslate(screen_pos, target_doc, document_pos);

	if (!target_doc)
	{
		target_doc = this;
		document_pos.x = 0;
		document_pos.y = 0;
	}

	if (!target)
		target = target_doc->GetDocRoot();

	DocumentInteractionContext ctx(target_doc, screen_pos, target_doc->GetVisualDevice()->GetOpView(), const_cast<OpWindow*>(GetWindow()->GetOpWindow()), target);
	if (OpStatus::IsSuccess(PopulateInteractionContext(ctx, target, 0, 0, document_pos.x, document_pos.y)))
		RequestContextMenu(ctx);
#endif // _POPUP_MENU_SUPPORT_
}

void FramesDocument::InitiateContextMenu(HTML_Element* target,
                                         int offset_x, int offset_y,
                                         int document_x, int document_y,
                                         BOOL has_keyboard_origin)
{
	OP_ASSERT(target);
#ifdef _POPUP_MENU_SUPPORT_
	OpPoint screen_coords = GetVisualDevice()->ScaleToScreen(OpPoint(document_x, document_y));
	OpPoint view_coords = GetVisualDevice()->OffsetToContainerAndScroll(screen_coords);
	OpPoint screen_pos = GetVisualDevice()->GetOpView()->ConvertToScreen(view_coords);

	DocumentInteractionContext ctx(this, screen_pos, GetVisualDevice()->GetOpView(), const_cast<OpWindow*>(GetWindow()->GetOpWindow()), target);
	if (has_keyboard_origin)
		ctx.SetHasKeyboardOrigin();

	if (OpStatus::IsSuccess(PopulateInteractionContext(ctx, target, offset_x, offset_y, document_x, document_y)))
		RequestContextMenu(ctx);
#endif // _POPUP_MENU_SUPPORT_
}

OP_STATUS
FramesDocument::PopulateInteractionContext(DocumentInteractionContext& ctx,
                                           HTML_Element* target,
                                           int offset_x, int offset_y,
                                           int document_x, int document_y)
{
	BOOL mailto = FALSE;
	BOOL link = FALSE;
	BOOL image = FALSE;
	BOOL selection = FALSE;
	BOOL is_editable_form_element = FALSE;
	BOOL is_form_element = FALSE;
#ifdef _PLUGIN_SUPPORT_
	BOOL is_plugin = FALSE;
#endif //#ifdef _PLUGIN_SUPPORT_
#ifdef SVG_SUPPORT
	BOOL is_svg = FALSE;
#endif // SVG_SUPPORT
	BOOL is_readonly = TRUE;
#ifdef MEDIA_HTML_SUPPORT
	BOOL is_media = FALSE;
	BOOL is_video = FALSE;
#endif // MEDIA_HTML_SUPPORT

	OpString selected_text;
	URL image_url;
	URL link_url;
	HTML_Element* image_element = NULL;

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	int spell_session_id = 0;
#endif // INTERNAL_SPELLCHECK_SUPPORT

	Window* window = GetWindow();

	HTML_Element* h_elm = target;
	const uni_char* link_title = NULL;

	while (h_elm)
	{
		switch (h_elm->GetNsType())
		{
#ifdef SVG_SUPPORT
			case NS_SVG:
			{
				if(h_elm->Type() == Markup::SVGE_SVG)
				{
					is_svg = TRUE;
					window->GetWindowCommander()->SetCurrentElement(h_elm);
					image_element = h_elm;
				}
			}
			break;
#endif // SVG_SUPPORT

			case NS_HTML:
			{
				switch (h_elm->Type())
				{
					case HE_INPUT:
					case HE_TEXTAREA:
						{
							is_form_element = TRUE;
							if (h_elm->Type() == HE_INPUT && h_elm->GetInputType() == INPUT_FILE)
								is_readonly = TRUE;
							else
								is_readonly = h_elm->GetReadOnly();
							FormValue* form_value = h_elm->GetFormValue();
							if (form_value->IsUserEditableText(h_elm))
							{
								is_editable_form_element = TRUE;
							}
#ifdef INTERNAL_SPELLCHECK_SUPPORT
							if (spell_session_id == 0)
							{
								OpPoint p(offset_x, offset_y);
								spell_session_id = form_value->CreateSpellSessionId(h_elm, &p);
							}
#endif // INTERNAL_SPELLCHECK_SUPPORT
						}
						break;

					case HE_SELECT:
						is_form_element = TRUE;
						is_readonly = h_elm->GetReadOnly();
						break;

#ifdef MEDIA_HTML_SUPPORT
					case HE_VIDEO:
						is_video = TRUE;
						// Intentional fall through.
					case HE_AUDIO:
						is_media = TRUE;
						break;
#endif // MEDIA_HTML_SUPPORT

					case HE_OBJECT:
					case HE_EMBED:
						{
#ifdef _PLUGIN_SUPPORT_
							if(h_elm->GetLayoutBox() && h_elm->GetLayoutBox()->IsContentEmbed())
							{
								is_plugin = TRUE;
							}
#endif //#ifdef _PLUGIN_SUPPORT_
						}
						// Fall through
					case HE_IMG:
						{
							URL img_url;
							if (h_elm->Type() == HE_IMG
								|| (h_elm->GetLayoutBox() && h_elm->GetLayoutBox()->IsContentImage()))
							{
								// We don't want to show image information for something that isn't an image.
								img_url = h_elm->GetImageURL(TRUE, GetLogicalDocument());
								URLContentType content_type = static_cast<URLContentType>(img_url.GetAttribute(URL::KContentType, URL::KFollowRedirect));
								if (content_type == URL_UNDETERMINED_CONTENT ||
								    imgManager->IsImage(content_type))
									image = TRUE;
								else if (content_type == URL_UNKNOWN_CONTENT)
								{
									Image img = UrlImageContentProvider::GetImageFromUrl(img_url);
									if (img.Width() != 0)
										image = TRUE;
								}
							}

							if (image)
							{
#ifdef _PLUGIN_SUPPORT_
								is_plugin = FALSE;
#endif //#ifdef _PLUGIN_SUPPORT_
								window->GetWindowCommander()->SetCurrentElement(h_elm);
								image_url = img_url;
#ifndef WIC_USE_DOWNLOAD_CALLBACK
								link_url = img_url;
#endif // !WIC_USE_DOWNLOAD_CALLBACK
								image_element = h_elm;

								URL* usemap_url = h_elm->GetIMG_UsemapURL();
								if (usemap_url && !usemap_url->IsEmpty())
								{
									LogicalDocument* logdoc = GetLogicalDocument();
									HTML_Element* map_element = NULL;
									if (*usemap_url == GetURL())
										map_element = logdoc->GetNamedHE(usemap_url->UniRelName());

									if (map_element)
									{
										int rx = 0; //GetViewX() - frm->AbsX() + x;
										int ry = 0; //(int)(GetViewY() - frm->AbsY() + y);
										RECT rect;
										if (logdoc->GetBoxRect(h_elm, CONTENT_BOX, rect))
										{
#if 0
											rx = x - rect.left;
											ry = y - rect.top;
#else
											// FIXME Coordinates taken out of thin air
											rx = offset_x;
											ry = offset_y;
#endif // 0
										}

										HTML_Element* default_element = NULL;
										HTML_Element* link_element = map_element->GetLinkElement(GetVisualDevice(), rx, ry, h_elm, default_element);

										if (!link_element)
											link_element = default_element;

										if (link_element)
										{
											URL *area_url = link_element->GetAREA_NoHRef() ? NULL : link_element->GetUrlAttr(ATTR_HREF, NS_IDX_HTML, GetLogicalDocument());

											if (area_url && !area_url->IsEmpty())
											{
												link = TRUE;

												if (area_url->Type() == URL_MAILTO)
													mailto = TRUE;

												link_url = *area_url;
											}
										}
									}
								}
								break;
							}
						}
						// fall through if non-image OBJECT

					case HE_A:
						link_title = h_elm->GetStringAttr(ATTR_TITLE);
							// fall through
					default:
						{
							URL attr_url = h_elm->GetAnchor_URL(this);

							if (!attr_url.IsEmpty())
							{
								link = TRUE;

								if (attr_url.Type() == URL_MAILTO)
									mailto = TRUE;

								link_url = attr_url;
							}
						}
						break;
					} // end Type() switch for NS_HTML
				} // end NS_HTML
				break;

#ifdef _WML_SUPPORT_
			case NS_WML:
				{
					URL tmp_url;
					URL *attr_url = NULL;
					WML_ElementType wml_tag = (WML_ElementType)h_elm->Type();
					if (wml_tag == WE_ANCHOR || wml_tag == WE_DO)
					{
						tmp_url = GetDocManager()->WMLGetContext()->GetWmlUrl(h_elm, NULL, WEVT_UNKNOWN);
						attr_url = &tmp_url;
					}
					else
						attr_url = (URL*)h_elm->GetSpecialAttr(ATTR_CSS_LINK, ITEM_TYPE_URL, (void*)NULL, SpecialNs::NS_LOGDOC);

					if (attr_url && !attr_url->IsEmpty())
					{
						link = TRUE;

						if (attr_url->Type() == URL_MAILTO)
							mailto = TRUE;

						link_url = *attr_url;
					}
				}
				break;
#endif // _WML_SUPPORT_

			default: // "unknown" namespace
				{
					URL *attr_url = NULL;
					attr_url = (URL*)h_elm->GetSpecialAttr(ATTR_CSS_LINK, ITEM_TYPE_URL, (void*)NULL, SpecialNs::NS_LOGDOC);

					if (attr_url && !attr_url->IsEmpty())
					{
						link = TRUE;

						if (attr_url->Type() == URL_MAILTO)
							mailto = TRUE;

						link_url = *attr_url;
					}
				}
		} // end namespace switch
		h_elm = h_elm->Parent();
	}

#ifdef HAS_NOTEXTSELECTION
	selection = FALSE;
#else
	if (target->IsInSelection())
	{
		selection = GetSelectedTextLen() > 0;

		if(selection)
		{
			GetSelectedText(selected_text);
		}
	}
#endif

	// We need the context registered since we might refer to it when we have actions
	// with no explicit object. See GetURLOfLastAccessedImage. It also needs to be
	// registered to be told of the deletion of FramesDocument if it's still alive.
	RETURN_IF_ERROR(m_document_contexts_in_use.Add(&ctx));

	if (link)
	{
		RETURN_IF_ERROR(ctx.SetHasLink(link_url, link_title));
	}

	if (image)
	{
		ctx.SetHasImage(image_url, image_element);
	}

#ifdef MEDIA_HTML_SUPPORT
	if (is_media)
	{
		ctx.SetHasMedia();
	}
	if (is_video)
	{
		ctx.SetIsVideo();
	}
#endif // MEDIA_HTML_SUPPORT

#ifdef _PLUGIN_SUPPORT_
	if(is_plugin)
	{
		ctx.SetHasPlugin();
	}
#endif

	if (selection && !selected_text.IsEmpty())
	{
		ctx.SetHasTextSelection();
	}

	if (mailto)
	{
		ctx.SetHasMailtoLink();
	}

#ifdef SVG_SUPPORT
	if (is_svg)
	{
		ctx.SetHasSVG();
	}
#endif // SVG_SUPPORT

#ifdef DOCUMENT_EDIT_SUPPORT
	if (GetDocumentEdit())
	{
		HTML_Element* h = target->IsMatchingType(HE_HTML, NS_HTML) && logdoc->GetBodyElm() ? logdoc->GetBodyElm() : target; // To catch the case when body is editable but only covers a part of the html element.

		if (GetDocumentEdit()->GetEditableContainer(h))
		{
			is_readonly = GetDocumentEdit()->m_readonly;
			ctx.SetHasEditableText();

#ifdef INTERNAL_SPELLCHECK_SUPPORT
			// This code is ugly because it's a copy of what the code looked like before the context menu was docxsified.
			// Should be possible to do this without involving a layout traversal
			OP_ASSERT(logdoc && logdoc->GetRoot()); // Since GetEditableContainer returned TRUE we know we have a tree.
			IntersectionObject intersection(this, LayoutCoord(document_x), LayoutCoord(document_y), TRUE);
			intersection.Traverse(logdoc->GetRoot());
			Box* inner_box = intersection.GetInnerBox();
			if (inner_box)
			{
				OpStatus::Ignore(GetDocumentEdit()->CreateSpellUISession(&intersection, spell_session_id));
			}
#endif // INTERNAL_SPELLCHECK_SUPPORT
		}
	}
#endif // DOCUMENT_EDIT_SUPPORT

	if (is_form_element)
	{
		ctx.SetIsFormElement();
		if (is_editable_form_element)
		{
			ctx.SetHasEditableText();
		}
	}

	if (is_readonly)
	{
		ctx.SetIsReadonly();
	}

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	if (spell_session_id != 0)
	{
		ctx.SetSpellSessionId(spell_session_id);
	}
#endif // INTERNAL_SPELLCHECK_SUPPORT

	return OpStatus::OK;
}

void FramesDocument::LoadImage(OpDocumentContext& ctx, BOOL disable_turbo /*=FALSE*/)
{
	// Only possible type right now and for the planned future
	DocumentInteractionContext& dic = static_cast<DocumentInteractionContext&>(ctx);

	// Code below originates in quick. It's basically copied here since it
	// didn't belong there.

	URL img_url = dic.GetImageURL();
	if(!img_url.IsEmpty())
	{
		Window* window = GetWindow();
		if (!window->ShowImages())
			window->SetImagesSetting(FIGS_SHOW);

		HTML_Element* img_elm = dic.GetImageElement();
		if (img_elm)
		{
#ifdef WEB_TURBO_MODE
			if (disable_turbo && img_url.GetAttribute(URL::KUsesTurbo) && !img_url.GetAttribute(URL::KTurboBypassed))
			{
				img_elm->DisableTurboForImage();
				// Re-fetch the image URL since it has been replaced by the above call
				img_url = img_elm->GetImageURL();
				// Update the image URL in the context to match the new URL object
				dic.SetHasImage(img_url, NULL);
			}
#endif // WEB_TURBO_MODE

			OpStatus::Ignore(LoadInline(&img_url, img_elm, IMAGE_INLINE, LoadInlineOptions().ReloadConditionally().ForceImageLoad()));
		}
	}
}

OP_STATUS FramesDocument::SaveImage(OpDocumentContext& ctx, const uni_char* filename)
{
	DocumentInteractionContext& dic = static_cast<DocumentInteractionContext&>(ctx);

	if (!dic.HasCachedImageData())
		return OpStatus::ERR;

	URL url = dic.GetImageURL();
	unsigned long url_status;

	url_status = url.PrepareForViewing(TRUE, TRUE, TRUE, TRUE);
	if (url_status == 0)
		url_status = url.SaveAsFile(filename);

	return ConvertUrlStatusToOpStatus(url_status);
}

void FramesDocument::OpenImage(OpDocumentContext& ctx, BOOL new_tab /*=FALSE*/, BOOL in_background /*=FALSE*/)
{
	// Only possible type right now and for the planned future
	DocumentInteractionContext& dic = static_cast<DocumentInteractionContext&>(ctx);

	URL img_url = dic.GetImageURL();
	if(!img_url.IsEmpty())
	{
		Window* window = GetWindow();
		if (!window->ShowImages())
			window->SetImagesSetting(FIGS_SHOW);

		BOOL new_window = new_tab ||
			window->GetType() == WIN_TYPE_NEWSFEED_VIEW ||
			window->GetType() == WIN_TYPE_MAIL_VIEW;

		GetTopDocument()->GetDocManager()->OpenImageURL(img_url, GetMutableOrigin(), FALSE, new_window, new_window && in_background);
	}
}

#ifdef RESERVED_REGIONS
/* static */
BOOL FramesDocument::IsReservedRegionEvent(DOM_EventType type)
{
	for (int i = 0; g_reserved_region_types[i] != DOM_EVENT_NONE; i++)
		if (type == g_reserved_region_types[i])
			return TRUE;

	return FALSE;
}

BOOL FramesDocument::HasReservedRegionHandler()
{
	for (int i = 0; g_reserved_region_types[i] != DOM_EVENT_NONE; i++)
	{
		DOM_EventType type = g_reserved_region_types[i];

		if (DOM_Environment* environment = GetDOMEnvironment())
		{
			if (environment->HasEventHandlers(type))
				return TRUE;
		}
		else if (logdoc)
		{
			unsigned int count;
			if (logdoc->GetEventHandlerCount(type, count) && count > 0)
				return TRUE;
		}
	}

	return FALSE;
}

OP_STATUS FramesDocument::GetReservedRegion(const OpRect& search_rect, OpRegion& region)
{
	if (logdoc)
	{
		LayoutWorkplace* workplace = logdoc->GetLayoutWorkplace();

		if (workplace->HasReservedRegionBoxes() || HasReservedRegionHandler())
			return workplace->GetReservedRegion(search_rect, region);
	}

	return OpStatus::OK;
}

void FramesDocument::SignalReservedRegionChange()
{
	ViewportController* controller = GetWindow()->GetViewportController();
	controller->GetViewportInfoListener()->OnDocumentContentChanged(controller, OpViewportInfoListener::REASON_RESERVED_REGIONS_CHANGED);
}
#endif // RESERVED_REGIONS

#ifdef PAGED_MEDIA_SUPPORT
OP_STATUS FramesDocument::SignalPageChange(HTML_Element* element, unsigned int current_page, unsigned int page_count)
{
	HLDocProfile* hld_profile = GetHLDocProfile();

	if (!hld_profile)
		return OpStatus::OK;

	RETURN_IF_ERROR(ConstructDOMEnvironment());

	HTML_Element* element_list[3] = { element, NULL }; // ARRAY OK 2011-11-28 mstensho

	if (element->Type() == Markup::HTE_DOC_ROOT)
	{
		int count = 0;

		if (HTML_Element* body = hld_profile->GetBodyElm())
			element_list[count++] = body;

		if (HTML_Element* root = hld_profile->GetRoot())
			element_list[count++] = root;

		element_list[count] = NULL;
	}

	for (int i = 0; element_list[i]; i++)
	{
		DOM_Environment::EventData data;

		data.type = ONPAGECHANGE;
		data.target = element_list[i];
		data.current_page = current_page;
		data.page_count = page_count;

		RETURN_IF_ERROR(dom_environment->HandleEvent(data));
	}

	if (element->Type() == Markup::HTE_DOC_ROOT)
	{
		// Paged overflow on viewport must be reported to the UI.

		ViewportController* controller = GetWindow()->GetViewportController();

		controller->SignalPageChange(current_page, page_count);
	}

	return OpStatus::OK;
}
#endif // PAGED_MEDIA_SUPPORT

#ifdef USE_OP_CLIPBOARD
BOOL FramesDocument::CopyImageToClipboard(OpDocumentContext& ctx)
{
	DocumentInteractionContext& dic = static_cast<DocumentInteractionContext&>(ctx);

	if (g_clipboard_manager && dic.HasCachedImageData())
	{
		URL url = dic.GetImageURL();
		OP_STATUS sts = g_clipboard_manager->CopyImageToClipboard(url, GetWindow()->GetUrlContextId());
		if (OpStatus::IsSuccess(sts))
			return TRUE;
	}
	return FALSE;
}

BOOL FramesDocument::CopyBGImageToClipboard(OpDocumentContext& ctx)
{
	DocumentInteractionContext& dic = static_cast<DocumentInteractionContext&>(ctx);

	if (g_clipboard_manager && dic.HasCachedBGImageData())
	{
		URL url = GetBGImageURL();
		OP_STATUS sts = g_clipboard_manager->CopyImageToClipboard(url, GetWindow()->GetUrlContextId());
		if (OpStatus::IsSuccess(sts))
			return TRUE;
	}
	return FALSE;
}
#endif // USE_OP_CLIPBOARD

#ifdef WIC_TEXTFORM_CLIPBOARD_CONTEXT
class DocumentClipboardHelper : public ClipboardListener
{
	DocumentInteractionContext* doc_ctx;
public:
	DocumentClipboardHelper(DocumentInteractionContext* ctx)
		: doc_ctx(ctx)
	{}

	~DocumentClipboardHelper()
	{
		OP_DELETE(doc_ctx);
	}

	void OnPaste(OpClipboard* clipboard);
	void OnCut(OpClipboard* clipboard);
	void OnCopy(OpClipboard* clipboard);
	void OnEnd()
	{
		OP_DELETE(this);
	}
};

void DocumentClipboardHelper::OnPaste(OpClipboard* clipboard)
{
	OP_ASSERT(doc_ctx);
	HTML_Element* form_elm = doc_ctx->GetHTMLElement();
	if (!form_elm)
		return;

	FormValue* value = form_elm->GetFormValue();
	OpString clipboard_text;
	if (value->HasMeaningfulTextRepresentation() &&
		OpStatus::IsSuccess(clipboard->GetText(clipboard_text)) &&
		!clipboard_text.IsEmpty())
	{
		OpStatus::Ignore(value->SetValueFromText(form_elm, clipboard_text.CStr(), TRUE));
		// TRUE since this call is assumed to be triggered from UI
		const BOOL has_user_origin = TRUE;
		FormValueListener::HandleValueChanged(doc_ctx->GetDocument(), form_elm, TRUE, has_user_origin, NULL);
	}

}

void DocumentClipboardHelper::OnCut(OpClipboard* clipboard)
{
	OP_ASSERT(doc_ctx);
	HTML_Element* form_elm = doc_ctx->GetHTMLElement();
	if (!form_elm)
		return;

	FormValue* value = form_elm->GetFormValue();
	OpString selected_text;
 	RETURN_VOID_IF_ERROR(value->GetValueAsText(form_elm, selected_text));
	if (!selected_text.IsEmpty())
	{
		RETURN_VOID_IF_ERROR(clipboard->PlaceText(selected_text.CStr(), doc_ctx->GetDocument()->GetWindow()->GetUrlContextId()));
		RETURN_VOID_IF_ERROR(value->SetValueFromText(form_elm, NULL, TRUE));
		// TRUE since this call is assumed to be triggered from UI
		const BOOL has_user_origin = TRUE;
		FormValueListener::HandleValueChanged(doc_ctx->GetDocument(), form_elm, TRUE, has_user_origin, NULL);
	}
}

void DocumentClipboardHelper::OnCopy(OpClipboard* clipboard)
{
	OP_ASSERT(doc_ctx);

	OpString selected_text;
	if (doc_ctx->IsFormElement() && doc_ctx->HasEditableText())
	{
		HTML_Element* form_elm = doc_ctx->GetHTMLElement();
		if (!form_elm)
			return;
		FormValue* value = form_elm->GetFormValue();
		if (value->HasMeaningfulTextRepresentation())
			OpStatus::Ignore(value->GetValueAsText(form_elm, selected_text));
	}
	else if (FramesDocument* doc = doc_ctx->GetDocument())
	{
		OpStatus::Ignore(doc->GetSelectedText(selected_text));
	}

	if (!selected_text.IsEmpty())
		clipboard->PlaceText(selected_text.CStr(), doc_ctx->GetDocument()->GetWindow()->GetUrlContextId());
}

static DocumentClipboardHelper* PrepareClipboardHelperAndRaiseIfOOM(FramesDocument* doc, DocumentInteractionContext& dic)
{
	DocumentInteractionContext* copy = static_cast<DocumentInteractionContext*>(dic.CreateCopy());
	if (!copy)
	{
		doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return NULL;
	}

	DocumentClipboardHelper* clipboard_helper = OP_NEW(DocumentClipboardHelper, (copy));
	if (!clipboard_helper)
	{
		OP_DELETE(copy);
		doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return NULL;
	}

	return clipboard_helper;
}

void FramesDocument::CopyTextToClipboard(OpDocumentContext& ctx)
{
	DocumentInteractionContext& dic = static_cast<DocumentInteractionContext&>(ctx);

	OpString selected_text;
	if (dic.IsFormElement() && dic.HasEditableText())
	{
		HTML_Element* form_elm = dic.GetHTMLElement();
		FormValue* value = form_elm->GetFormValue();
		if (value->HasMeaningfulTextRepresentation())
			OpStatus::Ignore(value->GetValueAsText(form_elm, selected_text));
	}
	else
	{
		OpStatus::Ignore(GetSelectedText(selected_text));
	}

	if (!selected_text.IsEmpty())
	{
		DocumentClipboardHelper* clipboard_helper = PrepareClipboardHelperAndRaiseIfOOM(this, dic);
		if (!clipboard_helper)
			return;
		g_clipboard_manager->Copy(clipboard_helper, GetWindow()->GetUrlContextId(), this, dic.GetHTMLElement());
	}
	else
	{
		OpInputAction action_copy(OpInputAction::ACTION_COPY);
		OnInputAction(&action_copy);
	}
}

void FramesDocument::CutTextToClipboard(OpDocumentContext& ctx)
{
	DocumentInteractionContext& dic = static_cast<DocumentInteractionContext&>(ctx);

	if (!dic.IsFormElement() || !dic.HasEditableText())
		return;

	HTML_Element* form_elm = dic.GetHTMLElement();
	FormValue* value = form_elm->GetFormValue();
	if (value->HasMeaningfulTextRepresentation())
	{
		DocumentClipboardHelper* clipboard_helper = PrepareClipboardHelperAndRaiseIfOOM(this, dic);
		if (!clipboard_helper)
			return;
		g_clipboard_manager->Cut(clipboard_helper, GetWindow()->GetUrlContextId(), this, form_elm);
	}
}

void FramesDocument::PasteTextFromClipboard(OpDocumentContext& ctx)
{
	DocumentInteractionContext& dic = static_cast<DocumentInteractionContext&>(ctx);

	if (!dic.IsFormElement() || !dic.HasEditableText())
		return;

	DocumentClipboardHelper* clipboard_helper = PrepareClipboardHelperAndRaiseIfOOM(this, dic);
	if (!clipboard_helper)
		return;
	g_clipboard_manager->Paste(clipboard_helper, this,  dic.GetHTMLElement());
}
#endif // WIC_TEXTFORM_CLIPBOARD_CONTEXT

void FramesDocument::ClearText(OpDocumentContext& ctx)
{
	DocumentInteractionContext& dic = static_cast<DocumentInteractionContext&>(ctx);

	if (!dic.IsFormElement() || !dic.HasEditableText())
		return;

	HTML_Element* form_elm = dic.GetHTMLElement();
	if (!form_elm)
		return;

	form_elm->GetFormValue()->SetValueFromText(form_elm, NULL);
	// TRUE since this call is assumed to be triggered from UI
	const BOOL has_user_origin = TRUE;
	FormValueListener::HandleValueChanged(dic.GetDocument(), form_elm, TRUE, has_user_origin, NULL);
}

# ifdef WIC_MIDCLICK_SUPPORT
void FramesDocument::HandleMidClick(HTML_Element* target, int offset_x, int offset_y,
									int document_x, int document_y, ShiftKeyState modifiers, BOOL mousedown)
{
	OpPoint screen_coords = GetVisualDevice()->ScaleToScreen(OpPoint(document_x, document_y));
	OpPoint view_coords = GetVisualDevice()->OffsetToContainerAndScroll(screen_coords);
	OpPoint screen_pos = GetVisualDevice()->GetOpView()->ConvertToScreen(view_coords);

	Window * window = GetWindow();
	DocumentInteractionContext ctx(this, screen_pos, GetVisualDevice()->GetOpView(), const_cast<OpWindow*>(window->GetOpWindow()), target);

	if (OpStatus::IsSuccess(PopulateInteractionContext(ctx, target, offset_x, offset_y, document_x, document_y)))
	{
#ifdef _POPUP_MENU_SUPPORT_
		window->GetWindowCommander()->GetMenuListener()->OnMidClick(window->GetWindowCommander(), ctx, mousedown, modifiers);
#endif // _POPUP_MENU_SUPPORT_
	}
}
# endif // MIDCLICK_OPEN_SUPPORT

void FramesDocument::CleanReferencesToElement(HTML_Element* element)
{
	OP_ASSERT(element);
	OP_ASSERT(element->IsReferenced()); // Expensive function, don't call if not needed

#if defined ACCESSIBILITY_EXTENSION_SUPPORT
	RemoveAccesibilityHookToElement(element);
#endif

#ifdef _PLUGIN_SUPPORT_
	INT32 elm_pos = m_blocked_plugins.Find(element);
	if (elm_pos >= 0)
	{
		OpNS4PluginHandler::GetHandler()->Resume(this, element, TRUE, TRUE);
		if (m_blocked_plugins.Find(element)!=-1)
			RemoveBlockedPlugin(element);
	}
#endif // _PLUGIN_SUPPORT_

#ifdef WAND_SUPPORT
	pending_wand_elements.RemoveByItem(element);
#endif // WAND_SUPPORT

	if (doc)
		doc->CleanReferencesToElement(element);
}

BOOL FramesDocument::IsLinkVisited(FramesDocument *frames_doc, const URL& link_url, int visited_links_state)
{
	if (visited_links_state == -1)
		visited_links_state = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::VisitedLinksState);

	OP_ASSERT(0 <= visited_links_state && visited_links_state <= 2);

	if (visited_links_state == 0)
		return FALSE;
	else if (visited_links_state == 1 && frames_doc)
		if (ServerName *link_sn = link_url.GetServerName())
		{
			if (ServerName *doc_sn = frames_doc->GetURL().GetServerName())
			{
				if (doc_sn != link_sn)
					return FALSE;
			}
			else if (frames_doc->GetURL().Type() != URL_OPERA)
				// Serverless schemes that are not opera: urls are not
				// allowed. For instance, prevents documents with data:
				// urls from poking at a link, or attachment:s from
				// the mail client to poke at history.
				return FALSE;
		}

	return !!link_url.GetAttribute(URL::KIsFollowed, URL::KFollowRedirect);
}

#ifdef _PLUGIN_SUPPORT_

OP_STATUS
FramesDocument::AddBlockedPlugin(HTML_Element* possible_plugin)
{
	if (m_blocked_plugins.Find(possible_plugin) == -1)
	{
		possible_plugin->SetReferenced(TRUE);
		RETURN_IF_ERROR(m_blocked_plugins.Add(possible_plugin));
		if (!IsLoadingPlugin(possible_plugin))
		{
			// Loading will be started by the layout engine in a Reflow
			// so we need to circumvent all reflow timers and do it
			// immediately or we'll just be idling
			PostReflowMsg(TRUE);
		}
	}
	return OpStatus::OK;
}

void
FramesDocument::RemoveBlockedPlugin(HTML_Element* possible_plugin)
{
	m_blocked_plugins.RemoveByItem(possible_plugin);
}

BOOL
FramesDocument::IsLoadingPlugin(HTML_Element* possible_plugin)
{
	for (int inline_types = 0; inline_types < 2; inline_types++)
	{
		// Would have been easier if we had always used the same inline type
		InlineResourceType inline_type = inline_types == 0 ? GENERIC_INLINE : EMBED_INLINE;

		if (HEListElm* helm = possible_plugin->GetHEListElmForInline(inline_type))
		{
			if (LoadInlineElm* lie = helm->GetLoadInlineElm())
			{
				if (lie->GetLoading()) // FIXME, this is not the same check as ShowEmbed so there might be a race
				{
					// Wait with the decision, leave blocked
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}

void FramesDocument::ResumeAllFinishedPlugins()
{
	// If some script was waiting for a plugin to load before continuing, now is the time to release it and let it go on
	for (unsigned i = 0; i < m_blocked_plugins.GetCount(); i++)
	{
		HTML_Element* possible_plugin = m_blocked_plugins.Get(i);

		if (!IsLoadingPlugin(possible_plugin))
		{
			// Not loading, just need to check if we're in the gap between loading and
			// initialising, in which case we should wait for the initialise
			OpNS4Plugin* plugin = possible_plugin->GetNS4Plugin();
			if (!plugin)
			{
				// No plugin, resume marking the plugin as failed
				OpNS4PluginHandler::GetHandler()->Resume(this, possible_plugin, TRUE, TRUE);
			}
			else
			{
				// Plugin is as ready as it will be, go on.
				OpNS4PluginHandler* plugin_manager = OpNS4PluginHandler::GetHandler();
				BOOL is_inited = plugin->IsPluginFullyInited();
				BOOL also_resume_threads_waiting_for_init = is_inited;
				// This might not resume the plugin if is_inited is FALSE
				plugin_manager->Resume(this, possible_plugin, FALSE, also_resume_threads_waiting_for_init);
				// If it was removed from the list we're iterating we need to compensate
				if (m_blocked_plugins.GetCount() == i || m_blocked_plugins.Get(i) != possible_plugin)
					i--;
			}
		}
	}
}

#endif // _PLUGIN_SUPPORT_

FramesDocument* FramesDocument::GetTopDocument()
{
	FramesDocument* current = this;
	FramesDocument* parent_doc;
	while ((parent_doc = current->GetParentDoc()) != NULL)
		current = parent_doc;

	return current;
}

void FramesDocument::SetUrl(const URL& new_url)
{
	// Update the URL. Typically used when the fragment identifier changes
	// but can also happen for instance during a redirect in a reload.
	if (!(new_url == url))
	{
		// This is a slightly risky operation. Ideally we should not
		// set url without also explicitly setting origin. We do not,
		// so instead try to deduce a correct security_context.
		if (IsURLSuitableSecurityContext(new_url))
			origin->security_context = new_url;
		else if (origin->security_context.IsEmpty())
			// Inherit security from the old url assuming it was responsible for "new_url".
			origin->security_context = url;
	}

	url = new_url;
}

/* static */ OP_STATUS
FramesDocument::CreateSearchURL(OpDocumentContext &context, OpString8 &url, OpString8 &query, OpString8 &charset, BOOL &is_post)
{
	DocumentInteractionContext &dic = static_cast<DocumentInteractionContext &>(context);
	FramesDocument *document = dic.GetDocument();
	HTML_Element *text_element = dic.GetHTMLElement();

	if (!document || !text_element)
		return OpStatus::ERR;

	if (text_element->IsMatchingType(HE_INPUT, NS_HTML))
		switch (text_element->GetFormValue()->GetType())
		{
		case FormValue::VALUE_LIST_SELECTION:
		case FormValue::VALUE_RADIOCHECK:
			return OpStatus::ERR;
		default:
			break;
		}
	else if (!text_element->IsMatchingType(HE_TEXTAREA, NS_HTML))
		return OpStatus::ERR;

	HTML_Element *form_element = FormManager::FindFormElm(document, text_element);

	if (!form_element)
		return OpStatus::ERR;

	HTML_Element *submit_element = FormManager::FindDefaultSubmitButton(document, form_element);
	Form form(document->GetURL(), form_element, submit_element, 0, 0);

	form.SetSearchFieldElement(text_element);

	OP_STATUS status;
	URL submit_url = form.GetURL(document, status);

	RETURN_IF_ERROR(status);
	RETURN_IF_ERROR(submit_url.GetAttribute(URL::KName_Username_Password_Hidden, url));
	RETURN_IF_ERROR(form.GetQueryString(query));

	if (url.Find("%s") == KNotFound && query.Find("%s") == KNotFound)
		return OpStatus::ERR;

	is_post = submit_url.GetAttribute(URL::KHTTP_Method) == HTTP_METHOD_POST;

	RETURN_IF_ERROR(charset.Set(form.GetCharset()));

	return OpStatus::OK;
}

/* static */
OP_STATUS FramesDocument::GetElementRect(OpDocumentContext &context, OpRect &rect)
{
	DocumentInteractionContext &dic = static_cast<DocumentInteractionContext&>(context);

	HTML_Element* helm = dic.GetHTMLElement();
	FramesDocument* frm_doc = dic.GetDocument();
	if (helm && frm_doc)
	{
		RECT r;
		LogicalDocument* log_doc = frm_doc->GetLogicalDocument();
		if (log_doc && log_doc->GetBoxRect(helm, CONTENT_BOX, r))
		{
			rect.Set(r.left, r.top, r.right - r.left, r.bottom - r.top);
			ViewportController* vpc = frm_doc->GetWindow()->GetViewportController();
			rect = vpc->ConvertToToplevelRect(frm_doc, rect);
			VisualDevice* vd = frm_doc->GetVisualDevice();
			vd->ScaleToScreen(rect);

			return OpStatus::OK;
		}
	}

	return OpStatus::ERR;
}

FramesDocElm* FramesDocument::GetFrmDocElmByDoc(FramesDocument *doc)
{
	FramesDocElm* frm_root = GetIFrmRoot() ? GetIFrmRoot() : GetFrmDocRoot();
	FramesDocElm* iter = frm_root ? frm_root->FirstChild() : NULL;
	while(iter)
	{
		if (iter->GetCurrentDoc() == doc)
			return iter;

		iter = iter->Next();
	}

	return NULL;
}

BOOL FramesDocument::IsContentChangeAllowed()
{
	if (FramesDocument* parent = GetParentDoc())
	{
		FramesDocElm* frame = parent->GetFrmDocElmByDoc(this);
		if (frame && !frame->IsContentAllowed()) // This is a doc of a frame which is not allowed to change
			return FALSE;
	}

	return TRUE;
}

#ifdef PLUGIN_AUTO_INSTALL
void FramesDocument::NotifyPluginDetected(const OpString& mime_type)
{
	WindowCommander* wincom = GetWindow()->GetWindowCommander();
	OpDocumentListener* doclis;

	if (wincom && (doclis = wincom->GetDocumentListener()))
		doclis->NotifyPluginDetected(wincom, mime_type);
}

void FramesDocument::NotifyPluginMissing(const OpStringC& a_mime_type)
{
	WindowCommander* wincom = GetWindow()->GetWindowCommander();
	OpDocumentListener* doclis;

	if (wincom && (doclis = wincom->GetDocumentListener()))
		doclis->NotifyPluginMissing(wincom, a_mime_type);

	StoreMissingMimeType(a_mime_type);
}

void FramesDocument::RequestPluginInfo(const OpString& mime_type, OpString& plugin_name, BOOL &available)
{
	WindowCommander* wincom = GetWindow()->GetWindowCommander();
	OpDocumentListener* doclis;

	if (wincom && (doclis = wincom->GetDocumentListener()))
		doclis->RequestPluginInfo(wincom, mime_type, plugin_name, available);
}

void FramesDocument::RequestPluginInstallation(const PluginInstallInformation& information)
{
	WindowCommander* wincom = GetWindow()->GetWindowCommander();
	OpDocumentListener* doclis;

	if (wincom && (doclis = wincom->GetDocumentListener()))
		doclis->RequestPluginInstallDialog(wincom, information);
}

void FramesDocument::OnPluginAvailable(const uni_char* mime_type)
{
	OpListenersIterator iterator(m_plugin_install_listeners);

	while (m_plugin_install_listeners.HasNext(iterator))
		m_plugin_install_listeners.GetNext(iterator)->OnPluginAvailable(mime_type);
}
#endif // PLUGIN_AUTO_INSTALL

#ifdef _PLUGIN_SUPPORT_
void FramesDocument::OnPluginDetected(const uni_char* mime_type)
{
	OpListenersIterator iterator(m_plugin_install_listeners);
	if (m_plugin_install_listeners.HasNext(iterator))
	{
		do
		{
			m_plugin_install_listeners.GetNext(iterator)->OnPluginDetected(mime_type);
		}
		while (m_plugin_install_listeners.HasNext(iterator));
	}
	else
	{
		/**
		 * Object-tags are parsed without creating actual content if the
		 * mime-type is unsupported. This means that there will be no plug-in
		 * install listeners and we'll have to rely on stored mime-types.
		 * Object-tags might also have fallback content, so we can't just
		 * refresh a single element either.
		 */
		for (UINT32 i = 0; i < m_missing_mime_types.GetCount(); ++i)
		{
			if (m_missing_mime_types.Get(i)->Compare(mime_type) == 0)
			{
#ifdef PLUGIN_AUTO_INSTALL
				// Disable auto-install for this mime-type.
				NotifyPluginDetected(*m_missing_mime_types.Get(i));
#endif  //PLUGIN_AUTO_INSTALL

				m_missing_mime_types.Remove(i);
				Reflow(TRUE);
				break;
			}
		}
	}
}

void FramesDocument::StoreMissingMimeType(const OpStringC& mime_type)
{
	if (OpString* mime = OP_NEW(OpString, ()))
		if (OpStatus::IsError(mime->Set(mime_type)) || OpStatus::IsError(m_missing_mime_types.Add(mime)))
			OP_DELETE(mime);
}
#endif // _PLUGIN_SUPPORT_

#ifdef ON_DEMAND_PLUGIN
BOOL FramesDocument::IsPluginPlaceholderCandidate(HTML_Element* html_element)
{
    BOOL is_turbo_enabled = FALSE;

#ifdef WEB_TURBO_MODE
    is_turbo_enabled = GetWindow()->GetTurboMode();
#endif

    return !(IsTopDocument() && IsPluginDoc()) && !html_element->IsPluginDemanded() &&
        (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::EnableOnDemandPlugin, GetURL()) ||
         is_turbo_enabled);
}

BOOL FramesDocument::DocTreeHasPluginPlaceholders()
{
	DocumentTreeIterator iter(GetWindow());

	iter.SetIncludeThis();

	while (iter.Next())
		if (iter.GetFramesDocument()->m_ondemand_placeholders.GetCount() > 0)
			return TRUE;

	return FALSE;
}

void FramesDocument::RegisterPluginPlaceholder(HTML_Element* placeholder)
{
	/* Done after marking element as a placeholder as OOM on inserting element
	   should not prevent element from becoming a plugin placeholder. */
	RETURN_VOID_IF_ERROR(m_ondemand_placeholders.Add(placeholder));

	if (WindowCommander* commander = GetWindow()->GetWindowCommander())
		commander->GetDocumentListener()->OnOnDemandStateChangeNotify(commander, DocTreeHasPluginPlaceholders());
}

void FramesDocument::UnregisterPluginPlaceholder(HTML_Element* placeholder)
{
	RETURN_VOID_IF_ERROR(m_ondemand_placeholders.Remove(placeholder));

	if (WindowCommander* commander = GetWindow()->GetWindowCommander())
		commander->GetDocumentListener()->OnOnDemandStateChangeNotify(commander, DocTreeHasPluginPlaceholders());
}
#endif // ON_DEMAND_PLUGIN

void FramesDocument::OnInlineLoad(AsyncLoadInlineElm* async_inline_elm, OP_LOAD_INLINE_STATUS status)
{
	if (OpStatus::IsError(status))
	{
		GetMessageHandler()->PostMessage(MSG_URL_LOADING_FAILED, async_inline_elm->GetInlineURL().Id(), 0);
	}
	else if (status == LoadInlineStatus::USE_LOADED)
	{
		GetMessageHandler()->PostMessage(MSG_HEADER_LOADED, async_inline_elm->GetInlineURL().Id(), 0);
		GetMessageHandler()->PostMessage(MSG_URL_DATA_LOADED, async_inline_elm->GetInlineURL().Id(), 0);
	}
	/* in case of LOADING_STARTED the code will be working as with not asynchronous inlines i.e.
	 * messagess from the network will be received so there no need to handle this case here
	 */

	HTML_Element *helm = async_inline_elm->GetInlineElement();

	async_inline_elm->Out();
	OP_DELETE(async_inline_elm);

	List<AsyncLoadInlineElm> *async_inlines;
	if (OpStatus::IsSuccess(async_inline_elms.GetData(helm, &async_inlines)) &&
		async_inlines->Empty() &&
		OpStatus::IsSuccess(async_inline_elms.Remove(helm, &async_inlines)))
	{
		OP_DELETE(async_inlines);
	}
}

AsyncLoadInlineElm::~AsyncLoadInlineElm()
{
	OP_ASSERT(security_state.host_resolving_comm);
	g_main_message_handler->RemoveCallBacks(this, security_state.host_resolving_comm->Id());
}

void AsyncLoadInlineElm::LoadInline()
{
	OP_ASSERT(security_state.host_resolving_comm);
	const OpMessage msges[] = { MSG_COMM_NAMERESOLVED, MSG_COMM_LOADING_FAILED };
	int msges_cnt = sizeof(msges) / sizeof(msges[0]);
	OP_STATUS status = g_main_message_handler->SetCallBackList(this, security_state.host_resolving_comm->Id(), msges, msges_cnt);
	if (OpStatus::IsError(status))
		if (listener)
			listener->OnInlineLoad(this, status);
}

void AsyncLoadInlineElm::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(security_state.host_resolving_comm &&
			  par1 == static_cast<MH_PARAM_1>(security_state.host_resolving_comm->Id()) &&
			  (msg == MSG_COMM_LOADING_FAILED || msg == MSG_COMM_NAMERESOLVED));

	if (msg == MSG_COMM_LOADING_FAILED)
	{
		if (listener)
			listener->OnInlineLoad(this, OpStatus::ERR);
	}
	else
		LoadInlineNow();
}

void AsyncLoadInlineElm::LoadInlineNow()
{
	OP_LOAD_INLINE_STATUS status;
	status = frm_doc->LoadInline(&inline_url, element, inline_type, options);

	if (listener)
		listener->OnInlineLoad(this, status);
}

void
FramesDocument::DOMGetInnerWidthAndHeight(unsigned int& inner_width, unsigned int& inner_height)
{
	/* Get the actual window inner size for top-level
		document. WinWidth() and WinHeight() in VisualDevice represent
		the rendering buffer size, which may be larger than the
		window. */

	if (IsTopDocument())
	{
		if (GetWindow()->GetTrueZoom())
		{
			/* So this code doesn't handle the case where you use scrollbars in combination
				with true zoom. I don't know if we would ever want to do that. If someone tries,
				he will trigger the assert here. The alternatives would be to either use the
				initial viewport size adjusted for both layout base scale and true zoom factor,
				or use the visual viewport and add the scrollbar size scaled by both layout base
				scale and true zoom factor. There might be a problem with the former if the visual
				viewport and scale factor is not in sync (platform code has to ensure this). The
				latter will introduce rounding issues. */
			OP_ASSERT(GetVisualDevice()->GetScrollType() == VisualDevice::VD_SCROLLING_NO);

			OpRect visual_viewport = GetVisualViewport();
			inner_width = static_cast<unsigned int>(visual_viewport.width);
			inner_height = static_cast<unsigned int>(visual_viewport.height);
		}
		else
			GetWindow()->GetCSSViewportSize(inner_width, inner_height);
	}
	else
	{
		inner_width = MAX(0, GetVisualDevice()->WinWidth());
		inner_height = MAX(0, GetVisualDevice()->WinHeight());
	}
}

OpPoint FramesDocument::DOMGetScrollOffset()
{
#ifdef PAGED_MEDIA_SUPPORT
	/* This is not an ideal solution, but it will have to do until we have
	   teamed up with OpViewportController completely for paged overflow.
	   Ideally the visual viewport position in the viewport controller should
	   change when we change pages, but currently it stays at 0,0. Getting that
	   right involves some work, so currently the ugly divide is right here. */

	if (logdoc)
		if (RootPagedContainer* pc = logdoc->GetLayoutWorkplace()->GetRootPagedContainer())
			return OpPoint(pc->GetViewX(), pc->GetViewY());
#endif // PAGED_MEDIA_SUPPORT

	OpRect visual_viewport = GetVisualViewport();

	return OpPoint(visual_viewport.x, visual_viewport.y);
}

void FramesDocument::DOMSetScrollOffset(int* x, int* y)
{
#ifdef PAGED_MEDIA_SUPPORT
	/* This is not an ideal solution, but it will have to do until we have
	   teamed up with OpViewportController completely for paged overflow.
	   Ideally the visual viewport position in the viewport controller should
	   change when we change pages, but currently it stays at 0,0. Getting that
	   right involves some work, so currently the ugly divide is right here. */

	if (logdoc)
		if (RootPagedContainer* pc = logdoc->GetLayoutWorkplace()->GetRootPagedContainer())
		{
			pc->SetScrollOffset(x, y);
			return;
		}
#endif // PAGED_MEDIA_SUPPORT

	OpRect visual_viewport = GetVisualViewport();

	if (x)
		visual_viewport.x = *x;

	if (y)
		visual_viewport.y = *y;

	RequestSetVisualViewPos(visual_viewport.x, visual_viewport.y, VIEWPORT_CHANGE_REASON_SCRIPT_SCROLL);
}

void FramesDocument::RecalculateDeviceMediaQueries()
{
	if (logdoc)
	{
		logdoc->GetLayoutWorkplace()->RecalculateDeviceMediaQueries();
	}
}

BOOL FramesDocument::StoredViewportCanBeRestored()
{
		int negative_overflow = NegativeOverflow();
		unsigned int width = negative_overflow + Width();
		if (old_win_width == width + GetVisualDevice()->GetVerticalScrollbarSize())
			width += GetVisualDevice()->GetVerticalScrollbarSize();
		OpRect doc_rect(-negative_overflow, 0, width, Height());
		DocListElm *doc_list_elm = doc_manager->CurrentDocListElm();
		OpRect viewport;
		if (!doc_list_elm)
			return FALSE;
		viewport = doc_list_elm->GetVisualViewport();
		BOOL has_size = viewport.width > 0 && viewport.height > 0;
		return has_size ? doc_rect.Contains(viewport) : doc_rect.Contains(OpPoint(viewport.x, viewport.y));
}

#ifdef GEOLOCATION_SUPPORT
void
FramesDocument::IncGeolocationUseCount()
{
	++geolocation_usage_count;
	if (geolocation_usage_count == 1)
		GetWindow()->NotifyGeolocationAccess();
}

void
FramesDocument::DecGeolocationUseCount()
{
	OP_ASSERT(geolocation_usage_count > 0);
	--geolocation_usage_count;
	if (geolocation_usage_count == 0)
		GetWindow()->NotifyGeolocationAccess();
}

#endif // GEOLOCATION_SUPPORT

#ifdef MEDIA_CAMERA_SUPPORT
void
FramesDocument::IncCameraUseCount()
{
	++camera_usage_count;
	if (camera_usage_count == 1)
		GetWindow()->NotifyCameraAccess();
}
void
FramesDocument::DecCameraUseCount()
{
	OP_ASSERT(camera_usage_count > 0);
	--camera_usage_count;
	if (camera_usage_count == 0)
		GetWindow()->NotifyCameraAccess();
}

#endif // MEDIA_CAMERA_SUPPORT
