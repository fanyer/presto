/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DOCHAND_DOCMAN_H
#define DOCHAND_DOCMAN_H

#include "modules/doc/doctypes.h"
#include "modules/doc/css_mode.h"
#include "modules/doc/documentorigin.h"
#include "modules/dochand/documentreferrer.h"
#include "modules/hardcore/unicode/unicode.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "modules/viewers/viewers.h"
#include "modules/url/url2.h"
#include "modules/url/url_loading.h"
#include "modules/util/simset.h"
#include "modules/util/opfile/unistream.h"
#include "modules/dom/domenvironment.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/idle/idle_detector.h"
#include "modules/windowcommander/OpViewportController.h"

class OpHistoryUserData;

#ifdef TRUST_RATING
#include "modules/dochand/fraud_check.h"
#endif // TRUST_RATING

class FramesDocElm;
class FramesDocument;
class VisualDevice;
class PrintDevice;
class DocListElm;
class HTML_Element;
class URL;
class Viewer;
#ifdef CLIENTSIDE_STORAGE_SUPPORT
class OpStorageManager;
#endif // CLIENTSIDE_STORAGE_SUPPORT

class ES_Thread;
class ES_OpenURLAction;
class ES_Runtime;
class DOM_ProxyEnvironment;
class DOM_Object;
class ES_PersistentValue;

#ifdef _WML_SUPPORT_
class WML_Context;
#endif // _WML_SUPPORT_

#ifdef WIC_USE_DOWNLOAD_CALLBACK
class TransferManagerDownloadCallback;
#endif //WIC_USE_DOWNLOAD_CALLBACK

#if defined _SSL_SUPPORT_ && !defined _EXTERNAL_SSL_SUPPORT_ && !defined _CERTICOM_SSL_SUPPORT_
struct SSL_dialog_config;
class SSL_Certificate_Installer_Base;
#endif // _SSL_SUPPORT_ && !_EXTERNAL_SSL_SUPPORT_ && !_CERTICOM_SSL_SUPPORT_

#ifdef SCOPE_PROFILER
class OpProbeTimeline;
class OpProfilingSession;
#endif // SCOPE_PROFILER

#define UNITE_WARNING_PAGE_URL  "uniteadminwarning"
#define OPERA_CLICKJACK_BLOCK_URL "opera:clickjackingblock"
#define OPERA_CLICKJACK_BLOCK_URL_PATH  "clickjackingblock"

/** Loading states for a DocumentManager. */
enum DM_LoadStat
{
	NOT_LOADING = 0,
	/**< State when nothing is being loaded. */
	WAIT_FOR_HEADER,
	/**< First state when loading a URL: set when the URL is requested and
	     remains set until a MSG_HEADER_LOADED message is received.  */
    WAIT_FOR_ACTION,
	/**< Optional state set after WAIT_FOR_HEADER if the information in the
	     response headers was inconclusive (not enough to determine how to
	     process the resource.)  When (more) data is receieved, we use it to
	     try to figure out what to do. */
	WAIT_FOR_LOADING_FINISHED,
	/**< Optional state set when the loaded resource will not be loaded as a
	     document, but will be handled by us in some way once it has finished
	     loading. */
	WAIT_FOR_USER,
	/**< Set if we display some sort of dialog asking the user what to do, for
	     the duration of that dialog. */
	WAIT_FOR_ECMASCRIPT,
	/**< Set if the resource being loaded will replace the current document,
	     while we process unload events in that document. */
	DOC_CREATED,
	/**< Set once we've created a document for the resource being loaded.
	     From now on, that document is in charge of handling the loading, and
	     we just wait for it to finish. */
	WAIT_MULTIPART_RELOAD
	/**< A MSG_MULTIPART_RELOAD has been received, and we're now waiting for
	     messages relating to the next body part.  Much like WAIT_FOR_HEADER,
	     but with some special handling for the case that there isn't another
	     body part after all. */
};

/**
 * Checks if the url is about:blank (or in case of selftests opera:blanker).
 */
BOOL IsAboutBlankURL(const URL& url);

/**
 * Checks if an URL has enough information in itself to build a security context from it or not. If this returns TRUE then it has
 * enough information to let it be the security context. If it returns FALSE then it's a data: url or javascript: url or about:blank
 * and need to inherit security from someone else unless there is nobody else.
 */
BOOL IsURLSuitableSecurityContext(URL url);

/** Local history element.  DocListElm objects represent the different states a
    window, frame or iframe can shift between as part of history navigation.
    Each history element begins at a certain window global history number
    (DocListElm::number) and continues to the next element's number minus one,
    or, if it is the last element its history list, to the last number that its
    parent document exists at, or, if it has no parent document, to the end of
    the history as determined by Window::GetHistoryMax().  This means that any
    DocListElm can be active at any number of history positions, and that
    navigation between two global history positions might not affect which is
    the active history element in a local history list. */
class DocListElm
  : public Link
{
private:

	URL				url;
	DocumentReferrer referrer_url;
    const int       m_id;
    /**< The ID for this DocListElm. */
	BOOL			send_to_server;
	FramesDocument*	doc;
	int				number;
	int				last_scale;
	LayoutMode		last_layout_mode;
	OpRect			visual_viewport;
	BOOL			has_visual_viewport;
#ifdef PAGED_MEDIA_SUPPORT
	int				current_page;
#endif
	BOOL			owns_doc;
	BOOL			replaceable;
	BOOL			precreated;
	OpStringS		title;

	BOOL script_generated_doc;
	BOOL replaced_empty_position;

#ifdef _WML_SUPPORT_
	WML_Context*	wml_context;
#endif // _WML_SUPPORT_

	friend class DocumentManager;

	BOOL			MaybeReassignDocOwnership();
	/**< If this history position owns its document but shares it with a later
	     history position, reset 'doc' to NULL and assign ownership to the other
	     history position.  This is only valid if this history position is about
	     to be removed (called from Out()) or is about to have its document
	     replaced by another (called from ReplaceDoc().)

	     @return TRUE if this history owned its document and reassigned that
	             ownership to another history position, FALSE otherwise. */

	ES_PersistentValue *m_data;

	OpHistoryUserData* m_user_data;
	/**< User data, owned by this DocListElm. */

public:

					DocListElm(const URL& doc_url, FramesDocument* d, BOOL own, int n, UINT32 id);
					~DocListElm();

	int				GetID() const { return m_id; }
	/**< Returns the ID for this DocListElm. */

	DocListElm*     Suc() const { return (DocListElm*) Link::Suc(); }
	/**< Returns the previous local history element or NULL if there is none.
	     This is the element that would be activated next in this document
	     manager during backwards history navigation (but many history steps
	     might be required for that to happen.) */
	DocListElm*     Pred() const { return (DocListElm*) Link::Pred(); }
	/**< Returns the next local history element or NULL if there is none.  This
	     is the element that would be activated next in this document manager
	     during forwards history navigation (but many history steps might be
	     required for that to happen.) */
    FramesDocument* Doc() const { return doc; }
	/**< Returns the active document at this local history element.  Never
	     returns NULL.  Note that several consecutive history elements can share
	     the same document, representing different scroll positions or states of
	     that document. */
    int				Number() const { return number; }
	/**< Returns the first window history number at which this local history
	     element is used.  If this is the same as the following local history
	     element's number, this local history element is dead and will be purged
	     by the next call to DocumentManager::HistoryCleanup(). */
	const uni_char*	Title() const { return title.CStr(); }
	/**< Returns the title of the document at this history position. */
	URL&			GetUrl() { return url; }
	/**< Returns the URL of the document at this history position.*/
	DocumentReferrer GetReferrerUrl() { return referrer_url; }
	/**< Returns the referrer URL used to load this document (which will also be
	     used if the URL is loaded again in order to activate this history
	     position.) */
	BOOL			GetOwnsDoc() { return owns_doc; }
	/**< Returns TRUE if this history element owns its document.  The first
	     element in the list that has a given document always owns it. */
	BOOL			IsReplaceable() { return replaceable; }
	/**< Returns TRUE if this history element should be replaced if succeeded by
	     another one (that is, if this element should only exist as the last
	     element.) */
	void			SetIsReplaceable() { replaceable = TRUE; }
	/**< See IsReplaceable(). */
	BOOL			IsPreCreated() { return precreated; }
	/**< Returns TRUE if this history position and its document was precreated,
	     that is, it was created before we knew that we actually wanted to
	     create it.  When we "discover" that we want to create a document, we
	     should probably reuse this. */
	void			SetIsPreCreated(BOOL value) { precreated = value; }
	/**< Called when we determine that a precreated document either won't be
	     used or when it has been used. */
	int				GetLastScale() const { return last_scale; }
	/**< Returns the scale level used last time this document was displayed. */
	void			SetLastScale(int scale) { last_scale = scale; }
	/**< Sets the scale level currently used to display this document. */
	LayoutMode		GetLastLayoutMode() const { return last_layout_mode; }
	/**< Returns the layout mode used last time this document was displayed. */
	void			SetLastLayoutMode(LayoutMode mode) { last_layout_mode = mode; }
	/**< Set the layout mode that was used when this document was displayed. */
	BOOL			HasVisualViewport() const { return has_visual_viewport; }
	/**< Returns TRUE if a visual viewport rectangle has been set for this history element. */
	void			ClearVisualViewport() { has_visual_viewport = FALSE; }
	/**< Make the visual viewport rectangle invalid. */
    const OpRect&   GetVisualViewport() const { return visual_viewport; }
	/**< Returns the visual viewport stored in this history element.
	     Not relevant (that is, not kept up-to-date) for the current element. */
    void            SetVisualViewport(const OpRect& viewport) { visual_viewport = viewport; has_visual_viewport = TRUE; }
	/**< Sets the visual viewport rectangle for this history element. */
	void			SetUserData(OpHistoryUserData* user_data);
	/**< Associate user data with this DocListElm. This object assumes ownership of the user_data. */
	OpHistoryUserData*	GetUserData() const;
	/**< Return user data associated with this DocListElm. */
#ifdef PAGED_MEDIA_SUPPORT
    int				GetCurrentPage() { return current_page; }
	/**< Returns the current page displayed at this history position, if in
	     paged media mode.  Much like an extra scroll position dimension. */
    void			SetCurrentPage(int page) { current_page = page; }
	/**< Set the current page displayed at this history position. */
#endif // PAGED_MEDIA_SUPPORT
	OP_STATUS       SetTitle(const uni_char* val);
	/**< Sets the title of the document at this history position. */
	void			SetUrl(const URL &new_url) { url = new_url; }
	/**< Sets the URL displayed at this history position. */

	BOOL			ShouldSendReferrer() { return send_to_server; }
	/**< Returns FALSE if the referrer URL should not be sent to the server
	 *   when loading the URL. */
    void            SetReferrerUrl(const DocumentReferrer& ref_url, BOOL isend_to_server = TRUE) { referrer_url = ref_url; send_to_server = isend_to_server; }
	/**< Sets the referrer URL used to load the URL displayed at this history
	 *   position.
	 *	 @param isend_to_server Set to FALSE if it should not be sent to the
	 *    server when loading the URL. */
	void			SetOwnsDoc(BOOL val) { owns_doc = val; }
	/**< Sets the flag that determines whether this history position owns its
	     document (meaning it deletes the document when its destroyed) or if
	     it shares it with some other history position that owns it.

	     The history position that owns a document is always the first one in
	     the list that has that particular document.  The flag is only to be
	     modified when the list is modified (for instance when the previous
	     owner position is removed while other positions sharing the document
	     remain.)  Use with care. */
	void			SetNumber(int num) { number = num; }
	/**< Updates the history number of this history position.  The history
	     number determines where in the global history this position begins.
	     Use with extreme care!  Setting an invalid history number on a history
	     position causes history bugs and quite possibly crashes. */
	void			Out();
	/**< Removes this history position from the history list it is in.  If this
	     history position owns its document but shares it with another history
	     position in the list, it reassigns ownership of the document to that
	     history position and resets the document pointer to NULL. */
    void			ReplaceDoc(FramesDocument* d);
	/**< Replace the document at this history position with 'd' (or set it )*/

	BOOL			IsScriptGeneratedDocument() { return script_generated_doc; }
	/**< Returns TRUE if this document was generated a script (either via
	     document.open or via returning a string from a javascript: URL.)
	     Script generated documents have URL:s that define their security
	     context but aren't necessarily the origin of the actual contents, so
	     such history positions are never ever reloaded during history
	     navigation and not automatically replaced even though their URL is
	     a javascript: URL, about:blank or an empty URL. */
	void			SetScriptGeneratedDocument(BOOL value = TRUE) { script_generated_doc = value; }
	/**< Sets the flag that determines that this is a script generated
	     document. */

	BOOL			ReplacedEmptyPosition() { return replaced_empty_position; }
	/**< Returns TRUE if this history position replaced an empty history
	     position when it was added to the history list.  An empty history
	     position is defined as one having the URL about:blank, or a javascript:
	     URL, or an empty URL, and that is the first history position in the
	     history list.  The information is used to replace this history position
	     as well, when an inline script or 'load' event handler loads another
	     document, even if this history position wouldn't be considered empty
	     otherwise. */
	void			SetReplacedEmptyPosition() { replaced_empty_position = TRUE; }
	/**< Sets the flag that determines that this document position replaced an
	     empty history position. */

	BOOL			HasPreceding();
	/**< Returns TRUE if there is a history position before this one in the
	     list.  Simply checking 'Pred() != NULL' is not necessarily accurate,
	     since if the immediately preceding element(s) has the same number as
	     this one, they are dead and will be removed in the near future by a
	     call to DocumentManager::HistoryCleanup(). */

#ifdef _WML_SUPPORT_
	WML_Context*	GetWmlContext() { return wml_context; }
	/**< Returns this history position's WML context or NULL if none has been
	     set. */
	void			SetWmlContext(WML_Context *new_ctx);
	/**< Sets this history position's WML context.  If the new context is not
	     the same as the old one, its reference counter is incremented and the
	     old context's is decremented.  The reference is released when this
	     history position is destroyed. */
#endif // _WML_SUPPORT_

	void SetData(ES_PersistentValue *data);
	/**< Sets the history element state's data.
	     The history element becomes the owner of the data. */

	ES_PersistentValue *GetData() { return m_data; }
	/**< Returns the history element state's data */
};


/** A big class that mostly manages documents.

    There is one DocumentManager instance for each Window object, and one for
    each frame and iframe displayed in documents.  (There are also
    DocumentManager objects owned by FramesDocElm objects that represent
    FRAMESET elements; these DocumentManager objects are unused and supposedly
    always empty.)  The DocumentManager is responsible for initially requesting
    URL:s loaded in windows (or frames or iframes,) handling the loading up
    until (in the common case) a document is created (after which the document
    loads on its own with the DocumentManager mostly waiting around for it to
    finish) and for history management.

    @section loading Loading

    Loading is started by a call to the function OpenURL(), usually called from
	Window::OpenURL() (when the request comes directly from the UI) or
	DocumentManager::LoadPendingUrl via DocumentManager::SetUrlLoadOnCommand
	called from WindowManager::OpenURLNamedWindow (when the request comes from
	inside the document complex, such as when following a link in a web page) or
	from ES_OpenURLAction::PerformBeforeUnload() (when the request comes from a
	script, or when a previous call to OpenURL() was delayed pending some script
	thread's execution.)

    For URL schemes that produce data directly (http:, file:, ftp: and data:,
    but not mailto: or telnet:, for instance) OpenURL() usually ends up calling
    URL::LoadDocument() to initiate the request, and setting the 'load_stat'
    member variable to WAIT_FOR_HEADER, indicating we're now waiting for the
    request to return something.

    For the javascript: URL scheme, the request to open it is simply redirected
    to the current document's scripting environment, after first creating an
    empty one if there was no current document.

    For other supported URL schemes, such as mailto: or telnet:, OpenURL()
    contains special code for handling each scheme.  For unsupported URL schemes
    OpDocumentListener::OnUnknownProtocol() is called, and if it doesn't claim
    to have handled the request, an error page is generated, or a console error
    message is posted, if the URL was not entered by the user and the request
    was not user initiated.

    @subsection headers When headers are loaded

    When the URL module reports that all response headers from the request have
    been processed (which, for non-HTTP(S) URL:s mostly means the URL request
    was successful and has started returning data) HandleHeadersLoaded() is
    called to inspect the URL:s content type to decide what to do.  Most of the
    actual inspection and decision making is left to the Viewers class in the
    viewers module, called from UpdateAction().

    If no decision can be made without inspecting the actual data returned by
    the request, the 'load_stat' member variable is set to WAIT_FOR_ACTION,
    which has the effect that HandleHeadersLoaded() is called again when more
    data is received, until a decision has been made.

    If a decision is made, and the action chosen was either VIEWER_OPERA (the
    default for content types handled internally by Opera) or VIEWER_PLUGIN,
    HandleByOpera() is called to continue with the handling.  HandleByOpera()
    determines what to do depending on the type of the content (which is set by
    the Viewers system as well) and generally either calls other parts of Opera
    to handle the content, or creates a new FramesDocument for loading the
    content as a document, or, if the request was some sort of reload, tells the
    current document to reload itself from the new content, or if the URL loaded
    ended up being redirected to the same URL as the current document with a
    fragment identifier, scrolls the current document to that fragment
    identifier.  If a document (the current document) is now in charge of
    handling the loading, HandleByOpera() sets the 'load_stat' member variable
    to DOC_CREATED.  In other cases, loading is either stopped now, or the
    'load_stat' member variable is set to WAIT_FOR_LOADING_FINISHED which means
    that we simply wait for the loading to finish to do something at that time
    instead.

    @subsection unload The unload event

    The previous section's description of what happens when HandleByOpera()
    decides to create a new document is somewhat over-simplified.  If there is a
    previous document and it has a scripting environment with an unload event
    handler registered, an unload event is fired at this pointer.  Since
    processing of that event is asynchronous, and must be completed before a new
    document is created, instead of setting the 'load_stat' member to
    DOC_CREATED, HandleByOpera() may set it to WAIT_FOR_ECMASCRIPT.

    In that case, ES_OpenURLAction::PerformAfterUnload posts a message to the
    DocumentManager (MSG_HANDLE_BY_OPERA) that, when received by the
    DocumentManager, repeats the call to HandleByOpera(), which then creates a
    FramesDocument object and sets the 'load_stat' member variable to
    DOC_CREATED.

    @subsection finish When loading finished

    If the URL request resulted in a document being loaded or reloaded, not much
    is done by the DocumentManager once the loading finishes.  The 'load_stat'
    member variable is reset to NOT_LOADING to indicate that the DocumentManager
    is now idle.  If all DocumentManagers in the window are now idle, the
    appropriate window-level loading finished actions are initiated via a call
    to Window::EndProgressDisplay().

    If the URL request did not result in a document being loaded and the loading
    was not abandoned as soon as that was decided, the function
    HandleAllLoaded() performs the necessary post-processing actions once the
    URL has finished loading.  This includes for instance performing certificate
    installation if a X.509 certificate is loaded, or executing an external
    application with an argument containing path of the URL:s cache file.

    @subsection notmodified Conditional reload not reloading

    Another special case loading result is if a conditional reload ("check if
    modified") is performed and the URL hadn't changed (the server returns a
    "304 Not modified" answer.)  In such a case, the function
    HandleDocumentNotModifed() is called.  It will essentially either call
    HandleAllLoaded() to finish this up, or redirect to HandleHeaderLoaded() if
    a new document should be created anyway (thus ignoring the fact that a
    request to the server resulted in no data, instead loading the document from
    the already cached resource.)

    @section history History

    On the window level, the history is simply a sequential list of numbers, one
    per history position, starting with Window::GetHistoryMin() and ending with
    Window::GetHistoryMax() (both included.)  The current history position is
    Window::GetCurrentHistoryPos() (same as Window::GetCurrentHistoryNumber().)

    In each DocumentManager, the history is instead a list of DocListElm
    objects.  Each DocListElm has a pointer to the document to make active when
    DocListElm is the current element, and some extra state information (scroll
    position, current spatial navigation state and such.)  A DocListElm can
    cover any number of (sequential) history numbers.  That is, when navigating
    between two top-level history positions (two different history numbers) it
    is possible that the same DocListElm object is selected as the current
    history position in a DocumentManager, and thus that the navigation has no
    effect in the location the DocumentManager represents (the top-level
    document, or a frame or iframe.)  Typically, history navigation will affect
    at least one DocumentManager, and possibly several, or even all.

    DocListElm objects in DocumentManagers are selected top-down when a new
    history number becomes the current.  That is, first the Window's
    DocumentManager is told to activate its DocListElm for that history number.
    Then the document pointed to by that DocListElm is asked to propagate the
    history number down into any frames or iframes that it has, and the
    DocumentManagers in them activate their DocListElms for that history number,
    and so on.  This propagation continues through the whole active document
    tree even if some DocumentManagers along the way do not change which
    DocListElm is the current, since descendant DocumentManagers may be
    affacted even if their ancestors are not.

    @subsection setcurrenthistorypos SetCurrentHistoryPos()

    The function in DocumentManager that is called to activate the correct
    DocListElm for a given history number is SetCurrentHistoryPos().  It finds
    the right DocListElm by searching backwards in the list of DocListElm
    objects for the last one with a history number greater than or equal to the
    history number being activated.  If there is no current DocListElm (happens
    when restoring a session, for instance) or if the found DocListElm has a
    different document, or if the document isn't loaded, SetCurrentNew() is
    called to activate and/or load the new document. */
class DocumentManager
	: private MessageObject
	, public DOM_ProxyEnvironment::RealWindowProvider
{
public:
	/** Indication of what sort of history navigation the current document
	    loading process is part of. */
	enum HistoryDirection
	{
		DM_NOT_HIST		= 0,
		/**< No history navigation (normal document load.) */
		DM_HIST_BACK	= 1,
		/**< Backward history navigation. */
		DM_HIST_FORW	= 2
		/**< Forward history navigation. */
	};

private:

	Window*			window;
	/**< The window in which this document manager lives.  Same (obviously) for
	     all document managers in the window, including those in frames and
	     iframes.  Set by the constructor, and never changed.  Never NULL. */
	MessageHandler*	mh;
	/**< Message handler specific to this document manager.  Used for all URL
	     loads performed by this document manager and documents that live in it.
	     Created by second stage construction and destroyed together with the
	     document manager.  Never NULL (except during construction.) */
	FramesDocElm*	frame;
	/**< The frame or iframe that this document manager belongs to.  That is,
	     the frame or iframe whose documents this document manager manages.
	     Only NULL for the top-level document manager, so can be used to
	     determine whether this is the top-level document manager. */
	FramesDocument*	parent_doc;
	/**< The document that contains 'frame'.  Only NULL for the top-level
	     document manager, so can be used to determine whether this is the
	     top-level document manager. */
	VisualDevice*	vis_dev;
	/**< Visual device owned either by 'window' (if this is the top-level
	     document manager) or 'frame'. */

	Head			doc_list;
	/**< List of history positions (DocListElm objects.)  Can be empty. */
	Head			unlinked_doc_list;
	/**< List of history positions removed from 'doc_list' and not immediately
	     destroyed.  May be reinsterted into 'doc_list' at a later time. */
	DocListElm*		current_doc_elm;
	/**< The current history position.  Can be NULL.  Always an element from
	     'doc_list' when not NULL. */
	int				GetNextDocListElmId();
	/**< Get the ID to use when creating a new DocListElm. */

	int				history_len;
	/**< Pointless waste of space^A^K Maintenance nightmare^A^K Number of
	     history positions in 'doc_list'. */

	Head			queued_messages;
	/**< List of messages queued (received but not processed) while in a call to
	     a function that reenters the message loop.  A workaround for use of
	     blocking dialogs that will be removed when that problems is solved. */
	BOOL			queue_messages;
	/**< If TRUE, any messages received will be queued for later processing.
	     Set to TRUE before a call to a function that reenters the message loop,
	     and back to FALSE again when the queue has been flushed.  That is, it
	     remains TRUE after the troublesome function call has ended, until all
	     the message that were queued during it (and after it) has been
	     processed. */

	DM_LoadStat		load_stat;
	/**< Current load status. */
	URL				current_url;
	/**< While not loading, the URL of the current document.  While loading, the
	     URL we're loading. */
	BOOL			current_url_is_inline_feed;
	/**< Whether the current URL is an webfeed to be displayed with the generated
	     feed viewer page. */
	DocumentOrigin* current_loading_origin;
	/**< The origin for the document we are loading or NULL. */
	URL				initial_request_url;
	/**< Set by OpenURL() to the same value it sets to current_url.  When the
	     loading URL is redirected, current_url is updated, but
	     initial_request_url remains referencing the URL we initially
	     requested. */
	URL_InUse		current_url_used;
	/**< Use reference for 'current_url'.  Set when 'load_stat' is/becomes not
	     NOT_LOADING and unset when 'load_stat' is/becomes NOT_LOADING. */
	IAmLoadingThisURL current_url_reserved;
	/**< Loading reference for 'current_url'.  Set once the URL has successfully
	     been requested and unset when 'load_stat' becomes NOT_LOADING. */

	DocumentReferrer	referrer_url;
	/**< Referrer URL to use/used when requesting 'current_url'. */
	BOOL			send_to_server;
	/**< FALSE if the referrer URL should not be sent to the server when
	 * loading the URL. */

	ViewAction		current_action;
	/**< What to do with the URL currently being loaded.  Is VIEWER_NOT_DEFINED
	     while not loading.  Primarily set by UpdateAction(), which asks the
	     viewers database. */
	ViewActionFlag	current_action_flag; // To overide flags on the viewer (which would be default)
	/**< Modification flags for 'current_action'.  Typically modifies what
	     VIEWER_SAVE means. */
	uni_char*		current_application;
	/**< Application to call to handle the current URL.  Only relevant when
	     'current_action' is VIEWER_APPLICATION. */

	URL				url_load_on_command;
	/**< URL to request with a delay.  Set by SetUrlLoadOnCommand(), and
	     requested by LoadPendingUrl() which is typically called when a
	     MSG_URL_LOAD_NOW message is received. */
	BOOL			url_replace_on_command;
	/**< If TRUE, then LoadPendingUrl() will replace the current history
	     position when it loads 'url_load_on_command'. */

	BOOL			user_auto_reload;
	/**< User auto reload flag (the "Reload every" context menu choice, I
	     think.) */
	BOOL			reload;
	/**< Reload flag.  TRUE while loading during any kind of reload (F5, META
	     refresh, user auto reload, history navigation to non-cached document
	     and probably others.) */
	BOOL			was_reloaded;
	/**< Last-load-reloaded-document flag.  TRUE after finishing any kind of
	     reload (F5, META refresh, user auto reload, history navigation to
		 non-cached document and probably others.) Set after the initial
		 loading is finished and the normal state flags reset. */
	BOOL			were_inlines_reloaded;
	/**< Last-load-reloaded-inlines flag.  TRUE after finishing any kind of
	     reload where the reload_inlines flag was set. Set after the initial
	     loading is finished and the normal state flags reset. */
	BOOL			were_inlines_requested_conditionally;
	/**< Last-load-requested-inlines-conditionally flag.  TRUE after finishing
	     any kind of reload where the request_inlines_conditionally flag was
	     set. Set after the initial loading is finished and the normal state
	     flags reset. */
    BOOL            redirect;
	/**< Redirect flag.  TRUE while loading caused by a META refresh that
	     redirected to another URL.  Not related to HTTP redirects. */
	BOOL			replace;
	/**< Replace flag.  If TRUE when a new history position is created, the
	     current history position is replaced. */
	BOOL			scroll_to_fragment_in_document_finished;
	/**< After loading a document we register the url fragment, but we should
	     only scroll to it if it's an initial load, not if it's history navigation
	     or reloads. */
	BOOL			load_image_only;
	/**< If TRUE, the current load was started in response to a
	     OpInputAction::ACTION_OPEN_IMAGE and will refuse to do anything if the
	     loaded URL turns out not to be an image. */
	BOOL			save_image_only;
	/**< If TRUE, the current load was started in response to a
	     OpInputAction::ACTION_SAVE_IMAGE and will refuse to do anything if the
	     loaded URL turns out not to be an image. */
	BOOL			is_handling_message;
	/**< TRUE while in a call to HandleLoading() handling a loading related
	     message.  HandleLoading() does nothing if set when it's called, to
	     protect against recursive calls. */
	BOOL			is_clearing;
	/**< TRUE while in a call to Clear(), to avoid doing unnecessary things in
	     other functions while clearing the document manager. */
	BOOL			has_posted_history_cleanup;
	/**< TRUE if a MSG_HISTORY_CLEANUP message has been posted and not yet
	     received, to avoid having more than posted at a time. */
	BOOL			has_set_callbacks;
	/**< TRUE if the various message callbacks have been set. */
	BOOL			error_page_shown;
	/**< TRUE if the document currently displayed is a generated error page. */

	int				use_history_number;
	/**< Normally -1.  If >= 0, this number is used for the next history
	     position created.  Typically used to replace a specific history
	     position when loading a document. */

	BOOL			use_current_doc;
	/**< Flag that makes HandleByOpera() call UpdateCurrentDoc() instead of
	     SetCurrentDoc(), makes SetCurrentDoc() go ahead instead of reporting
	     errors in error situations, and makes UpdateCurrentDoc() just update
	     the URL of the current document instead of reloading it.  Only set to
	     TRUE immediately before calls to SetCurrentDoc(), so the rest is
	     probably irrelevant. */

	BOOL			reload_document;
	BOOL			conditionally_request_document;
	BOOL			reload_inlines;
	BOOL			conditionally_request_inlines;

	CheckExpiryType	check_expiry; // Controls whether to check expiry of inline urls, including frames.
								  // Will be updated each time history is walked or new url is loaded.

	HistoryDirection	history_walk; // This flag is set to DM_HIST_BACK or
									// DM_HIST_FORW when activating a new
									// document in history.

	URL_ID			pending_refresh_id; // refresh id received while in preview mode
	URL_ID			waiting_for_refresh_id; // refresh id to load when receiving next
											// refresh message (MSG_START_REFRESH_LOADING),
											// if match.
	unsigned long	waiting_for_refresh_delay;

	double			start_navigation_time_ms; //< The time in GetRuntimeMS scale that the navigation started.

	BOOL			restart_parsing; // if decoding with wrong character set

	BOOL			pending_viewport_restore;

#ifdef _PRINT_SUPPORT_
	FramesDocument*	print_doc;
	FramesDocument*	print_preview_doc;
	VisualDevice*	print_vd;
	VisualDevice*	print_preview_vd;
#endif // _PRINT_SUPPORT_

    BOOL            pending_url_user_initiated;

	/** TRUE if the user manually entered the url that is loading. */
    BOOL            user_started_loading;

	/** TRUE if the user performed some action that initiated this load. Clicked on a link for instance. */
	BOOL			user_initiated_loading;

	/** TRUE if the url access should be checked */
	BOOL            check_url_access;

#ifdef _WML_SUPPORT_
    WML_Context*     wml_context;
#endif // _WML_SUPPORT_

	DOM_ProxyEnvironment *dom_environment;

	BOOL              es_pending_unload;
	ES_OpenURLAction* es_terminating_action;

	ES_ThreadInfo	request_thread_info;
	/** The history element storing the latest state provided by history.pushState/history.replaceState.
	 * It's only saved when pushState/replaceState is called while document is still loading. It's needed to set the document manager's
	 * url after the loading is finished (changing the url while the document is still being loaded would break the loading).
	 * Note that the url is updated only if history_state_doc_elm == current_doc_elm.
	 */
	DocListElm* history_state_doc_elm;
	/** Pending history delta (a sum of all deltas) to be applied when doing the history navigation which is *always* async */
	int history_traverse_pending_delta;

	URL*			m_waiting_for_online_url;

#ifdef WIC_USE_DOWNLOAD_CALLBACK

	friend class TransferManagerDownloadCallback;

	OpVector <TransferManagerDownloadCallback> current_download_vector;
	void AddCurrentDownloadRequest(TransferManagerDownloadCallback *request) { current_download_vector.Add(request); }
	void RemoveCurrentDownloadRequest(TransferManagerDownloadCallback *request) { current_download_vector.RemoveByItem(request); }
#endif // WIC_USE_DOWNLOAD_CALLBACK

#if defined _SSL_SUPPORT_ && !defined _EXTERNAL_SSL_SUPPORT_ && !defined _CERTICOM_SSL_SUPPORT_
	SSL_Certificate_Installer_Base *ssl_certificate_installer;
#endif // _SSL_SUPPORT_ && !_EXTERNAL_SSL_SUPPORT_ && !_CERTICOM_SSL_SUPPORT_

	BOOL			is_delayed_action_msg_posted;
	double			target_time_for_delayed_action_msg;

#ifdef TRUST_RATING
	Head			m_active_trust_checks;
	UINT			m_next_checker_id;
#endif // TRUST_RATING

#ifdef CLIENTSIDE_STORAGE_SUPPORT
	OpStorageManager *m_storage_manager;
#endif // CLIENTSIDE_STORAGE_SUPPORT

	/** Keeps a local 'is-active' state for doc_manager. Used to detect if opera is idle, important for testing */
	OpActivity activity_loading;
	OpActivity activity_refresh;

#ifdef SCOPE_PROFILER
	OpProbeTimeline *m_timeline;
#endif // SCOPE_PROFILER

#ifdef WEB_HANDLERS_SUPPORT
	/** If set an action is not allowed to be changed */
	BOOL action_locked;
	/** If set the check if some web protocol handler should handle a url is skipped when opening the url */
	BOOL skip_protocol_handler_check;
#endif // WEB_HANDLERS_SUPPORT

	/** TRUE if OnNewPage has been sent, and OnNewPageReady should be
	 * sent when the document is ready to be displayed. */
	BOOL			waiting_for_document_ready;

#ifdef _PRINT_SUPPORT_
	OP_STATUS       CreatePrintDoc(BOOL preview);
#endif // _PRINT_SUPPORT_

	void			FlushHTTPSFromHistory(DocListElm* &prev_doc_elm, DocListElm* current_doc_elm);

	/// Notifies the WindowCommander of a changed URL early for empty windows
	void			NotifyUrlChangedIfAppropriate(URL& url);

	void			HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	void			SetLoadStat(DM_LoadStat value);

	int				GetNextHistoryNumber(BOOL is_replace, BOOL &replaced_empty);
	void			InsertHistoryElement(DocListElm *element);

	/**
	 * Removes duplicate (same number) entries in the
	 * history list caused by
	 * a document replacing an existing document in the history.
	 */
	void			HistoryCleanup();

	/** DecRefs and sets the current_loading_origin pointer to NULL if it's currently set. */
	void			DropCurrentLoadingOrigin();

	/**
	 * Uses current_loading_origin, referrer and current_url to determine a suitable DocumentOrigin
	 * for a document loaded from current_url.
	 *
	 * current_loading_origin will be unset when this returns regardless of return value.
	 *
	 * @returns A DocumentOrigin pointer that is already referenced (the caller should not call IncRef).
	 */
	DocumentOrigin*	CreateOriginFromLoadingState();

#ifndef NO_EXTERNAL_APPLICATIONS
	OP_STATUS		SetTempDownloadFolder();
#endif // NO_EXTERNAL_APPLICATIONS
	OP_STATUS		SetSaveDirect(Viewer * viewer, BOOL &can_do);

#ifdef OPERA_CONSOLE
	void			PostURLErrorToConsole(URL& url, Str::LocaleString error_message);
#endif // OPERA_CONSOLE

	BOOL			IsRelativeJump(URL url);

	BOOL			DisplayContentIfAvailable(URL &url, URL_ID id, MH_PARAM_2 load_status);

	/**
	 * Request to signal OpViewportController::OnNewPage() to the UI.
	 *
	 * OnNewPage might not be sent for some reasons, e.g. if this is
	 * not the DocumentManager for the top document.
	 *
	 * @param reason reason for the new page displayed
	 * @return TRUE if OnNewPage was sent
	 */
	BOOL			SignalOnNewPage(OpViewportChangeReason reason);

public:
					DocumentManager(Window* win, FramesDocElm* frm, FramesDocument* pdoc);
	virtual			~DocumentManager();

	OP_STATUS		Construct();
	void			Clear();

	VisualDevice*	GetVisualDevice() const
		{
			return
#ifdef _PRINT_SUPPORT_
				(print_vd) ? print_vd : (print_preview_vd) ? print_preview_vd :
#endif
				vis_dev;
		}

	void			SetVisualDevice(VisualDevice* vd) { vis_dev = vd; }

	/**
	 * @returns Window that contains this DocumentManager. Never NULL.
	 */
	Window*			GetWindow() const { return window; }
	MessageHandler*	GetMessageHandler() { return mh; }
	FramesDocElm*	GetFrame() const { return frame; }
	FramesDocument*	GetParentDoc() const { return parent_doc; }
	FramesDocument*	GetCurrentDoc() const;
#ifdef _PRINT_SUPPORT_
	VisualDevice*	GetPrintPreviewVD() { return print_vd ? print_vd : print_preview_vd; }
	FramesDocument*	GetPrintDoc() const { return print_doc ? print_doc : print_preview_doc; }
#endif

	/**
	 * Get the document that's currently visible.
	 *
	 * This is either the "regular" document returned by GetCurrentDoc(), or
	 * the print preview document.
	 */
	FramesDocument*	GetCurrentVisibleDoc() const;

#ifdef CLIENTSIDE_STORAGE_SUPPORT
	/**
	 * Returns the OpStorageManager used for web storage.
	 *
	 * @param create      if the OpStorageManager has not been set,
	 *                    a new one is allocated if 'create' is TRUE,
	 *                    else NULL is returned.
	 */
	OpStorageManager* GetStorageManager(BOOL create);

	/**
	 * Sets the OpStorageManager that the documents
	 * contained shall use. OpStorageManagers are shared
	 * because some volatile data, like sessionStorage, is
	 * only kept in memory, or when privacy mode is enabled.
	 *
	 * @param new_mgr   The new storage manager
	 */
	void SetStorageManager(OpStorageManager* new_mgr);

#endif // CLIENTSIDE_STORAGE_SUPPORT

	DocumentManager*
					GetDocManagerById(int sub_win_id) const;

	OP_STATUS		UpdateAction(const uni_char* &app);
	/**
	 * @param[in] create_document TRUE if the FramesDocument should be
	 * force created now rather than created during loading later. Be
	 * careful with this. FALSE is the suggested value.
	 * @param[in] is_user_initiated If the history traversal was caused
	 * due to user interaction. Else would be for cases initiated by scripts.
	 */
	OP_STATUS		SetCurrentDoc(BOOL check_if_inline_expired, BOOL use_plugin, BOOL create_document, BOOL is_user_initiated);
	OP_STATUS		UpdateCurrentDoc(BOOL use_plugin, BOOL parsing_restarted, BOOL is_user_initiated);

	void			SetCurrentNew(DocListElm* prev_doc_elm, BOOL is_user_initiated);

	int 			GetUseHistoryNumber() const { return use_history_number; }
	void			SetUseHistoryNumber(int number) { use_history_number = number; }

	int				GetPendingHistoryDelta() { return history_traverse_pending_delta; }
	void			AddPendingHistoryDelta(int delta) { history_traverse_pending_delta += delta; }
	void			ClearPendingHistoryDelta() { history_traverse_pending_delta = 0; }

	void			RemoveFromHistory(int from,  BOOL unlink = FALSE);
	void			RemoveUptoHistory(int to, BOOL unlink = FALSE);
	void			CheckHistory(int decrement, int& minhist, int& maxhist);
    void            RemoveElementFromHistory(int pos, BOOL unlink = TRUE);
	void			RemoveElementFromHistory(DocListElm *dle, BOOL unlink = TRUE, BOOL update_current_history_pos = TRUE);
#ifdef DOCHAND_HISTORY_MANIPULATION_SUPPORT
    void            InsertElementInHistory(int pos, DocListElm * doc_elm);
#endif // DOCHAND_HISTORY_MANIPULATION_SUPPORT

	OP_STATUS		UpdateCallbacks(BOOL is_active);

	/**
	 * Store current visual viewports into the specified DocListElm. In paged
	 * media, store page number instead.
	 *
	 * This is done recursively for all descendant framesets, frames and
	 * iframes.
	 */
	void			StoreViewport(DocListElm* doc_elm);

	/**
	 * Set current visual viewports from the current DocListElm. In paged
	 * media, restore page number instead.
	 *
	 * @param only_pending_restoration If TRUE, the viewport will only be
	 * restored if doing that earlier failed. Restoration fails if
	 * RestoreViewport() is called while the document isn't fully loaded, and
	 * the viewport was outside the bounds of the document.
	 *
	 * @param recurse If TRUE, this is done recursively for all descendant
	 * framesets, frames and iframes.
	 *
	 * @param make_fit If the viewport is outside the bounds of the document,
	 * change it so that it fits. For paged media, if the stored page number is
	 * larger than the number of pages in the document, go to the last page.
	 */
	void			RestoreViewport(BOOL only_pending_restoration, BOOL recurse, BOOL make_fit);

	/**
	 * Cancels pending viewport restoration. See RestoreViewport().
	 */
	void			CancelPendingViewportRestoration() { pending_viewport_restore = FALSE; }

	/**
	 * Add new history element.
	 *
	 * @param url history element's url
	 * @param ref_url history element's referrer
	 * @param history_num position the new element is supposed to be added at. -1 means use th next position.
	 * @param doc_title history element's title.
	 * @param make_current If TRUE the inserted history element become current one.
	 * @param is_plugin If TRUE the url leads to a content handler by plugin.
	 * @param existing_doc a document to be put in the history. If NULL a new one will be created.
	 * @param scale the zoom level.
	 *
	 * @return OpStatus::ERR_NO_MEMORY in case of OOM. OpStatus::OK otherwise.
	 */
	OP_STATUS		AddNewHistoryPosition(URL& url, const DocumentReferrer& ref_url, int history_num, const uni_char* doc_title, BOOL make_current = FALSE, BOOL is_plugin = FALSE, FramesDocument * existing_doc = NULL, int scale = 100);

	/**
	 * This is used to foresee if navigating to a new position will still reuse
	 * the same document, for instance when just changing the rel part of a url
	 *
	 * @param position      history position
	 */
	BOOL			IsCurrentDocTheSameAt(int position);
	/**
	* Traverses this documentmanager's history to the position.
	* @param num                history position
	* @param is_user_initiated  TRUE if this history traversal was
	*                           initiated by the user, like when
	*                           clicking the back or forward buttons
	*                           in the browser UI
	*/
	void			SetCurrentHistoryPos(int num, BOOL parent_doc_changed, BOOL is_user_initiated);
	void			UnloadCurrentDoc();

	int				GetHistoryLen() { return history_len; }
	FramesDocument*	GetHistoryNext() const;
	FramesDocument*	GetHistoryPrev() const;
	/**
	 * Associate user data with a specific DocListElm.
	 *
	 * Set user data for the DocListElm with the specified ID, that
	 * can be reached from a history position within the Window that
	 * this DocListElm belongs to.
	 *
	 * Should only be called on the top level DocumentManager.
	 *
	 * @param history_ID the ID of the DocListElm to set user data for
	 * @param user_data user data to associate. The DocumentManager
	 * assumes ownership of this object on success.
	 * @retval OpStatus::OK if operation was successful.
	 * @retval OpStatus::ERR if operation failed, for example if the history_ID could not be found
	 */
	OP_STATUS		SetHistoryUserData(int history_ID, OpHistoryUserData* user_data);
	/**
	 * Return user data previously associated with a specific DocListElm.
	 *
	 * Get user data for the DocListElm with the specified ID, that
	 * can be reached from a history position within the Window that
	 * this DocListElm belongs to.
	 *
	 * Should only be called on the top level DocumentManager.
	 *
	 * @param history_ID the ID of the DocListElm to get user data for
	 * @param[out] user_data where to fill in the pointer to the user
	 * data. If no user data had previously been set, it is filled
	 * with NULL. The returned instance is still owned by the
	 * callee.
	 * @retval OpStatus::OK if operation was successful
	 * @retval OpStatus::ERR if an error occured, for example if the history_ID could not be found
	 */
	OP_STATUS		GetHistoryUserData(int history_ID, OpHistoryUserData** user_data) const;

	void			SetShowImg(BOOL val);

	OP_STATUS       ReformatCurrentDoc();
	OP_STATUS       ReformatDoc(FramesDocument* doc);

	DocListElm*		CurrentDocListElm() const { return current_doc_elm; }
	DocListElm*		FirstDocListElm() const { return (DocListElm*)doc_list.First(); }
	DocListElm*		LastDocListElm() const { return (DocListElm*)doc_list.Last(); }
	DocListElm*		FindDocListElm(FramesDocument *doc) const;

	void			UpdateVisitedLinks(const URL& url);

    void			Refresh(URL_ID id);

	void            Reload(EnteredByUser entered_by_user, BOOL conditionally_request_document = FALSE, BOOL conditionally_request_inlines = TRUE, BOOL is_user_auto_reload = FALSE);

	URL&			GetCurrentURL() { return current_url; }
	/**
	 * Sets the current URL.
	 * @param moved Set to TRUE if the URL is moved or redirected.
	 */
	void			SetCurrentURL(const URL& url, BOOL moved);
	URL				GetCurrentDocURL();

	/**
	 *  Returns URL to use as referer when loading elements from this document
	 *  The referrer is used for security checks so it's important that it represents
	 *  the true loader of resources
	 */
	DocumentReferrer	GenerateReferrerURL();
	BOOL			ShouldSendReferrer();
	/**< Returns FALSE if the referrer URL should not be sent to the server
	 *   when loading the URL. */
	void        	SetReferrerURL(const DocumentReferrer& new_referrer, BOOL isend_to_server = TRUE) { referrer_url = new_referrer; send_to_server = isend_to_server; }
	/**< Sets the referrer URL used to load the URL displayed at this history
	 *   position.
	 *	 @param isend_to_server Set to FALSE if it should not be sent to the
	 *    server when loading the URL of the current document. */

	void			SetUrlLoadOnCommand(const URL& url, const DocumentReferrer& ref_url, BOOL replace_document = FALSE, BOOL was_user_initiated = FALSE, ES_Thread *origin_thread = NULL);

   	BOOL			GetUserAutoReload() const { return user_auto_reload; }
   	void			SetUserAutoReload(BOOL val) { user_auto_reload = val; }
   	BOOL			GetReload() const { return reload; }
	void			SetReload(BOOL value);
	/**< Sets the reload flags for the loading of the current url.
	 *   @param value If TRUE, the main url will be reloaded.
	 */
	BOOL			MustReloadScripts() { return reload || was_reloaded; }
	BOOL			MustReloadInlines() { return reload_inlines || were_inlines_reloaded; }
	BOOL			MustConditionallyRequestInlines() { return conditionally_request_inlines || were_inlines_requested_conditionally; }
    BOOL            GetRedirect() const { return redirect; }
    void            SetRedirect(BOOL val) { redirect = val; }
   	BOOL			GetReplace() const { return replace; }
   	void			SetReplace(BOOL val) { replace = val; }
	BOOL            GetUserStartedLoading() { return user_started_loading; }
	BOOL			IsClearing() { return is_clearing; }
	BOOL			ErrorPageShown() { return error_page_shown; }
	void			SetErrorPageShown(BOOL shown) { error_page_shown = shown; }

#ifdef WEB_HANDLERS_SUPPORT
	void			SetActionLocked(BOOL val) { action_locked = val;}
	void			SetSkipProtocolHandlerCheck() { skip_protocol_handler_check = TRUE; }
#endif // WEB_HANDLERS_SUPPORT
	void			SetAction(ViewAction act);
	ViewAction		GetAction() { return current_action; }
	void			SetActionFlag(ViewActionFlag::ViewActionFlagType flag) { current_action_flag.Set(flag); }
	void			UnsetActionFlag(ViewActionFlag::ViewActionFlagType flag) { current_action_flag.Unset(flag); }
	void			ResetActionFlag(ViewActionFlag::ViewActionFlagType flag) { current_action_flag.Reset(); }
   	const uni_char*	GetApplication() { return current_application; }
	void        	SetApplication(const uni_char* str);

	void			SetReloadFlags(BOOL reload_document, BOOL conditionally_request_document, BOOL reload_inlines, BOOL conditionally_request_inlines);
	BOOL			GetReloadDocument() { return reload_document; }
	BOOL			GetConditionallyRequestDocument() { return conditionally_request_document; }
	BOOL			GetReloadInlines() { return reload_inlines; }
	BOOL			GetConditionallyRequestInlines() { return conditionally_request_inlines; }

	/** Optional arguments to OpenURL(). */
	class OpenURLOptions
	{
	public:
		OpenURLOptions();
		/**< Constructor.  Sets all members to their documented default
		     values. */

		BOOL user_initiated;
		/**< TRUE if the request was directly initiated by the user.

		     Default: FALSE.

		     FIXME: It is unclear exactly what this means or what effects it
		            has. */

		BOOL create_doc_now;
		/**< If TRUE, a new current document will be created immediately (on
		     success.)  Normally, a new current document isn't created until a
		     response has been received, and the loaded resource has been
		     identified as something that actually should be loaded as a new
		     document.  Only use this if you must have a reference to the new
		     document immediately after the call.

		     Default: FALSE. */

		EnteredByUser entered_by_user;
		/**< WasEnteredByUser if the URL came from the user via the UI (entered
		     into the address field, loaded via a bookmark or similar.)  The
		     effect is that the URL is "trusted" since the user was completely
		     in control of how it was specified.  This allows javascript: URLs
		     to be loaded in any document.

		     NotEnteredByUser means the URL didn't come directly from the user,
		     and can't be completely trusted.

		     Default: NotEnteredByUser. */

		BOOL is_walking_in_history;
		/**< TRUE if this call loads a URL as part of navigating in history.
		     Used for instance when a document containing frames or iframes is
		     navigated to and reloaded from cache because it was not in the
		     document cache, when loading the documents in the frames or
		     iframes.  Affects which set of cache policies used.

		     Default: FALSE. */

		BOOL es_terminated;
		/**< TRUE if this call continues a previous call to OpenURL that was
		     suspended while waiting for scripts in the document to finish.

		     Default: FALSE. */

		BOOL called_externally;
		/**< TRUE if the mailto: URL was sent to Opera from an external
		     application. Used by Quick/M2 to avoid call-out loops.

		     Default: FALSE. */

		BOOL bypass_url_access_check;
		/**< Bypasses the security checks that limit access to file: URLs to
		     other file: URLs and certain opera: URLs, and that prevents
		     loading opera: URLs other than opera:blank in frames or iframes.
		     Use with care, obviously.

		     Default: FALSE. */

		BOOL ignore_fragment_id;
		/**< Disables the direct handling of the URLs fragment identifier when
		     the URL ignoring the fragment identifier is identical to the URL of
		     the current document.  Normally, opening such a URL results in
		     nothing more than the document being scrolled to the identified
		     anchor.

		     Default: FALSE. */

		BOOL is_inline_feed;
		/**< Open the URL as a webfeed with in the generated feed viewer page.

		     Default: FALSE */

		ES_Thread *origin_thread;
		/**< The thread that opened the url.

		     Default: NULL. */

		BOOL from_html_attribute;
		/**< This will change an internal policy that it is allowed to
		     load a url recursively. By setting this to TRUE it will
		     not be allowed.

		     Default: FALSE. */
	};

	void			OpenURL(URL url, DocumentReferrer referrer, BOOL check_if_expired, BOOL reload, const OpenURLOptions &options);
	/**< Open URL.  If the URL is identical to the current document's URL, has a
	     fragment identifier and 'reload' is FALSE, the document will just be
	     scrolled (but see OpenURLOptions::ignore_fragment_id.)  If the URL's
	     scheme is a special one (such as mailto: or javascript:) the URL will
	     be specially handled according to the scheme.  Otherwise, a request to
	     load the URL will be issued using the appropriate reload policy.

	     The appropriate reload policy is determined by quite a few interacting
	     factors, but most importantly the arguments 'check_if_expired' and
	     'reload'.  If 'reload' is TRUE, the URL will be reloaded conditionally
	     (if 'check_if_expired' is TRUE) or unconditionally.  If 'reload' is
	     FALSE, the URL will not be reloaded, unless cache policies dictates
	     otherwise, or 'check_if_expired' is TRUE, in which case a conditional
	     request will typically be issued.

	     Note that the exact behaviour when 'reload' is TRUE can also be
	     overridden by a prior call to SetReloadFlags(), that all conditional
	     requests will typically be affected by expiry checking preferences and
	     the HTTP cache controlling headers, and that the "must-revalidate"
	     value to a Cache-Control header overrides almost everything else.

	     @param url URL to load.
	     @param referrer_url Referrer URL to use if/when the URL is reloaded.
	                         Also used to determine whether to allow the access.
	     @param check_if_expired See function's main documentation.
	     @param reload See function's main documentation.
	     @param options Optional arguments. */

	void			OpenURL(URL& url, const DocumentReferrer& referrer, BOOL check_if_expired, BOOL reload, BOOL user_initiated=FALSE, BOOL create_doc_now=FALSE, EnteredByUser entered_by_user=NotEnteredByUser, BOOL is_walking_in_history=FALSE, BOOL es_terminated=FALSE, BOOL called_externally=FALSE, BOOL bypass_url_access_check=FALSE);
	/**< Open URL.  See OpenURL(URL, URL, BOOL, BOOL, const OpenURLOptions &)
	     for details.  This function is equivalent except in how arguments are
	     passed.  Please use the other variant.  This function will be
	     deprecated and removed in time. */

	void			LoadPendingUrl(URL_ID url_id, BOOL user_initiated);

	void			OpenImageURL(URL url, const DocumentReferrer& referrer, BOOL save, BOOL new_page = FALSE, BOOL in_background = FALSE);
	/**< Like OpenURL, but does nothing if the URL is not an image. */

	BOOL			HandleByOpera(BOOL check_if_inline_expired, BOOL use_plugin, BOOL is_user_initiated, BOOL *out_of_memory = NULL);

	BOOL			NeedsProgressBar();
	/**< Returns TRUE if a progress bar should be displayed in the window
	     because we are currently loading, and FALSE otherwise.  Traverses the
	     document tree and calls FramesDocument::NeedsProgressBar() for each
	     document. */

	void			EndProgressDisplay(BOOL force = FALSE);
	void			HandleErrorUrl();
	OP_STATUS       HandleDocFinished();
	OP_STATUS       UpdateWindowHistoryAndTitle();

	OP_STATUS       HandleDataLoaded(URL_ID url_id);
	OP_STATUS       HandleHeaderLoaded(URL_ID url_id, BOOL check_if_inline_expired);
	void			HandleMultipartReload(URL_ID url_id, BOOL internal_reload);
	void			HandleAllLoaded(URL_ID url_id, BOOL multipart_reload = FALSE);
	void			HandleDocumentNotModified(URL_ID url_id);

	/**
	 * Checks if OpViewportController::OnNewPageReady should be called
	 * and calls it if so.
	 *
	 * OnNewPageReady should be called iff OnNewPage has previously
	 * been called, and the document is now ready to be
	 * displayed. Normally, the VisualDevice notifies when the
	 * document is ready via its lock mechanism, but extra checks
	 * should be made explicitly whenever there is a risk that the
	 * VisualDevice was not locked, after OnNewPage was called.
	 *
	 * @see OpViewportController::OnNewPage
	 * @see OpViewportController::OnNewPageReady
	 * @see VisualDevice::LockUpdate
	 */
	void			CheckOnNewPageReady();

	/**
	 * Sends an error from the network layer to the appropriate windowcommander listener.
	 */
	void 			SendNetworkErrorCode(Window* window, MH_PARAM_2 user_data);
	void			HandleLoadingFailed(URL_ID url_id, MH_PARAM_2 user_data);
	void			HandleUrlMoved(URL_ID url_id, URL_ID moved_to_id);

	OP_STATUS       HandleLoading(OpMessage msg, URL_ID url_id, MH_PARAM_2 user_data);

	BOOL			IsCurrentDocLoaded(BOOL inlines_loaded = TRUE);

	/**
	 * Stops the document loading
	 *
	 * @param format                 Load and parse already buffered outstanding data.
	 * @param force_end_progress     Signal that the progress bar should be removed immediately.
	 * @param abort                  Puts the document in the aborted state. It prevents aborted/unloaded inlines from being (re)loaded when going back to the page in history.
	 *                               Normally TRUE when the stop is user invoked (e.g. by pressing the stop button).
	 */
	void			StopLoading(BOOL format, BOOL force_end_progress = FALSE, BOOL abort = FALSE);
	void			ResetStateFlags();

	int				GetSubWinId();

	OP_BOOLEAN      LoadAllImages();

	DM_LoadStat		GetLoadStatus() { return load_stat; }
	void			SetLoadStatus(DM_LoadStat lstat) { SetLoadStat(lstat); }

#ifdef _PRINT_SUPPORT_
	OP_STATUS       UpdatePrintPreview();
	OP_BOOLEAN      SetPrintMode(BOOL on, FramesDocElm* copy_fde, BOOL preview);
	void			SetPrintPreviewVD(VisualDevice* vd) { print_preview_vd = vd; }
	OP_DOC_STATUS   PrintPage(PrintDevice* pd, int page_num, BOOL print_selected_only, BOOL only_probe=FALSE);
#endif // _PRINT_SUPPORT_

	void			SetScale(int scale);

#if defined SAVE_DOCUMENT_AS_TEXT_SUPPORT
	OP_STATUS		SaveCurrentDocAsText(UnicodeOutputStream* stream, const uni_char* fname=NULL, const char *force_encoding=NULL);

	//OP_STATUS		SaveCurrentDocAsTextToMemory(uni_char *&buf, long& size);
#endif // SAVE_DOCUMENT_AS_TEXT_SUPPORT

	void			ClearHistory();

	BOOL			GetRestartParsing() { return restart_parsing; }
	void			SetRestartParsing(BOOL val) { restart_parsing = val; }
	OP_STATUS		RestartParsing();

	void			UpdateSecurityState(BOOL include_loading_docs);

#ifdef _WML_SUPPORT_
    OP_STATUS       WMLInit();

    BOOL			WMLHasWML() { return(wml_context != NULL); }

    WML_Context*    WMLGetContext() { return wml_context; }
    OP_STATUS       WMLSetContext(WML_Context *new_context);
	void			WMLDeleteContext();

	void			WMLDeWmlifyHistory(BOOL delete_all = FALSE);
#endif // _WML_SUPPORT_

	/**
	 *  Schedules a refresh to happen after a specified delay (i.e. load a URL again)
	 *  @param url_id  Id of URL to refresh
	 *  @param delay   How long to wait before refreshing (in milliseconds)
	 */
	void			SetRefreshDocument(URL_ID url_id, unsigned long delay);
	/** Cancels next scheduled refresh (if any) */
	void			CancelRefresh();

	CheckExpiryType	GetCheckExpiryType() { return check_expiry; }
	void			SetCheckExpiryType(CheckExpiryType ce_type) { check_expiry = ce_type; }
	BOOL			IsWalkingInHistory() { return history_walk != DM_NOT_HIST; }
	HistoryDirection GetHistoryDirection() { return history_walk; }

	void			ESGeneratingDocument(ES_Thread *generating_thread, BOOL is_reload, DocumentOrigin* origin);
	void			ESGeneratingDocumentFinished();

	/* From DOM_ProxyEnvironment::RealWindowProvider. */
	virtual OP_STATUS	GetRealWindow(DOM_Object *&window);

	OP_STATUS		GetJSWindow(DOM_Object *&window, ES_Runtime *origining_runtime);

	BOOL			HasJSWindow();
	void			UpdateCurrentJSWindow();

	void			ESSetPendingUnload(ES_OpenURLAction *action);
	OP_STATUS		ESSendPendingUnload(BOOL check_if_inline_expired, BOOL use_plugin);
	OP_STATUS		ESCancelPendingUnload();

	DOM_ProxyEnvironment *GetDOMEnvironment() { return dom_environment; }
	/**
	 * Creates the proxy environment if it doesn't exist, else does nothing.
	 * If this returns OpStatus::OK, then GetDOMEnvironment will return
	 * something.
	 */
	OP_STATUS ConstructDOMProxyEnvironment();

	/**
	 * Pushes (or replaces) the history element's state as described in
	 * http://www.whatwg.org/specs/web-apps/current-work/#dom-history-pushstate
	 *
	 * @param data - data of the history element's state
	 * @param title - history element's title
	 * @param url - history element's url
	 * @param create_new - the flog indicating it this is push or replace operation
	 */
	OP_STATUS PutHistoryState(ES_PersistentValue *data, const uni_char *title, URL &url, BOOL create_new = TRUE);

	/**
	 * dochand internal method. Don't touch if you're not coming from
	 * inside dochand.
	 *
	 * @param for_javascript_url TRUE to indicate that the new document is
	 *                           purely for running a javascript url.
	 * @param prevent_early_onload If TRUE then do not initiate onload delivery for
	 *                             about:blank while the document is constructed.
	 * @param use_opera_blanker If TRUE, use opera:blanker instead of
	 *                          about:blank.
	 */
	OP_STATUS CreateInitialEmptyDocument(BOOL for_javascript_url, BOOL prevent_early_onload, BOOL use_opera_blanker = FALSE);

	void			RaiseCondition(OP_STATUS status);

	void			SetWaitingForOnlineUrl(URL &url);
	OP_STATUS		OnlineModeChanged();

	static BOOL		IsTrustedExternalURLProtocol(const OpStringC &URLname);

#if defined WEB_HANDLERS_SUPPORT || defined QUICK
	/**
	 * Checks if the protocol the url uses should be handled by some application e.g. a system application or a web service
	 *
	 * @return OpBoolean::IS_FALSE in case the url should be continued loading by Opera
	 * OpBoolean::IS_TRUE if loading should not be continued as the protocol is handled by some other application.
	 * In case of error the error code is returned.
	 */
	OP_BOOLEAN		AttemptExecuteURLsRecognizedByOpera(URL& url, DocumentReferrer referrer, BOOL user_initiated, BOOL user_started);
#endif // WEB_HANDLERS_SUPPORT || QUICK
	OP_BOOLEAN		JumpToRelativePosition(const URL &url, BOOL reuse_history_entry = FALSE);

	/**
	 * Checks if loading |url| would be a recursive load, i.e. would load an url
	 * already in a parent document. That would start an infinite loop using up all
	 * the computer's resources and might hang opera.
	 */
	BOOL IsRecursiveDocumentOpening(const URL& url, BOOL from_html_attribute);

	void			StoreRequestThreadInfo(ES_Thread *thread);
	void			ResetRequestThreadInfo();

	BOOL			IgnoreURLRequest(URL url, ES_Thread *thread = NULL);


	enum SecCheckRes
	{
		SEC_CHECK_DENIED,      // Access denied
		SEC_CHECK_STARTED,     // An asynchronous security check was started.
		SEC_CHECK_ALLOWED,     // Access allowed
	};

	/**
	 * Initiates a security check for a redirected URL.
	 *
	 * @param url The url to check.
	 * @param source The url from which the access was attempted.
	 *
	 * @return Result indicating denied/allowed or if an asynchronous check was started.
	 */
	SecCheckRes InitiateSecurityCheck(URL url, const DocumentReferrer& source);

	/**
	 * Performs the error handling needed when URL access has been denied.
	 *
	 * @param url The url that was denied.
	 * @param source The url from which the access was attempted.
	 */
	void            HandleURLAccessDenied(URL url, URL source);

	static BOOL		IsSpecialURL(URL target);

	BOOL			IsLoadOrSaveImageOnly() { return load_image_only || save_image_only; }

	/**
	 * Some internal actions we don't want to do directly and instead
	 * register information about the action and posts a delayed
	 * message to do them later. Use this to post such a message. If a
	 * message is already queued nothing will be done.
	 *
	 * @param[in] delay_ms How long to delay the message. Normally
	 * 0 would be a good number, but if it is something that is
	 * unlikely to work in a while (for instance because we wait
	 * for message loops to unnest), then a bigger delay might be
	 * appropriate.
	 */
	void			PostDelayedActionMessage(int delay_ms);

	/**
	 * Call when a "delayed action message" is received so that DocumentManager
	 * knows that it has to send a new such message the next time
	 * PostDelayedActionMessage() is called. This should be called by
	 * anyone receiving a MSG_DOC_DELAYED_ACTION message originating
	 * from the MessageHandler in this DocumentManager.
	 *
	 * @see PostDelayedActionMessage
	 */
	void			OnDelayedActionMessageReceived() { is_delayed_action_msg_posted = FALSE; }

#ifdef TRUST_RATING
	/**
	 * Checks current trust rating of the url.
	 * @return OpStatus::OK if url is trusted.
	 *         OpStatus::ERR_NO_ACCESS if url is fraudulent, and the error page was generated.
	 *         Other errors like OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS		CheckTrustRating(URL url, BOOL offline_check_only=FALSE, BOOL force_check=FALSE);
	OP_STATUS		AddTrustCheck(URL url, BOOL need_ip_resolve);
	OP_STATUS		GenerateAndShowFraudWarningPage(const URL& url, TrustInfoParser::Advisory *advisory = NULL);

#endif // TRUST_RATING

#ifdef ERROR_PAGE_SUPPORT
	/**
	 * Generate a cross domain error page.
	 *
	 * @param url The url that the error refers to.
	 * @param error An enum value for a string containing the cross
	 * domain error message.
	 */
	OP_STATUS		GenerateAndShowCrossNetworkErrorPage(const URL& url, Str::LocaleString error);

	/**
	 * Displays a warning that this frame/iframe can't be loaded
	 * here.
	 *
	 * @param[in] url The url that was attemped to load.
	 */
	OP_STATUS		GenerateAndShowClickJackingBlock(const URL& url);

	/**
	 * Processes a url load coming from a click-through page, like
	 * opera:crossnetworkwarning or opera:site-warning.
	 * The document url and referrer passed as arguments are changed
	 * to point to the new final urls.
	 *
	 * @param url      the url to navigate to
	 * @param ref_url  referrer, the url of the click through page
	 * @return OpStatus::OK if everything is ok and the url should keep loading.
	 *         Opstatus::ERR if the url should not continue loading.
	 *         OpStatus::ERR_NO_MEMORY if OOM.
	 */
	OP_STATUS HandleClickThroughUrl(URL& url, DocumentReferrer& ref_url);

#endif // ERROR_PAGE_SUPPORT

#ifdef WEBSERVER_SUPPORT
	/**
	 * Displays a warning that this unite page can't be opened this way.
	 *
	 * @param[in] url The url that was attemped to load.
	 *
	 * @param[in] source The url of the page that tried to load the unite url.
	 */
	OP_STATUS		GenerateAndShowUniteWarning(URL &url, URL& source);
#endif // WEBSERVER_SUPPORT

#ifdef WEB_TURBO_MODE
	/**
	 * Must be called when the Web Turbo Mode changes for the
	 * current window.
	 */
	OP_STATUS		UpdateTurboMode();
#endif // WEB_TURBO_MODE

#ifdef SCOPE_PROFILER
	/**
	 * Start profiling using the specified timeline.
	 *
	 * @param timeline Object to store profiling data in. Owned by caller.
	 *                 The pointer must point to a valid object at least until
	 *                 the next call to StopProfiling().
	 *
	 * @return OpStatus::OK if profiling begun, or OpStatus::ERR if profiling
	 *         already was in progress.
	 */
	OP_STATUS StartProfiling(OpProfilingSession *session);

	/**
	 * Stop profiling. If this document manager is not currently
	 * profiled, nothing happens.
	 */
	void StopProfiling();

	/**
	 * Called when an immediate child is added to this DocumentManager.
	 *
	 * @param child The child DocumentManager about to be added.
	 */
	void OnAddChild(DocumentManager *child);

	/**
	 * Called when an immediate child is removed from this DocumentManager.
	 *
	 * @param child The child DocumentManager about to be added.
	 */
	void OnRemoveChild(DocumentManager *child);

	/**
	 * Get the OpProbeTimeline for this document manager.
	 *
	 * @return The timeline if this document manager is currently
	 *         profiled, otherwise NULL.
	 */
	OpProbeTimeline *GetTimeline() const { return m_timeline; }

	/**
	 * Check whether this document manager is currently profiled.
	 *
	 * @return TRUE if profiling, FALSE if not.
	 */
	BOOL IsProfiling() const { return (m_timeline != NULL); }
#endif // SCOPE_PROFILER

#ifdef DOM_LOAD_TV_APP
	/** Set a list of urls that are allowed to be opened by this DocumentManager.
	 *
	 * The whitelist is the vector of strings. The vector and all strings are
	 * owned by DocumentManager and will be freed when the DocumentManager
	 * is destroyed or if new whitelist it set.
	 * The vector and each string must be allocated by OP_NEWA().
	 *
	 * If a white-list is set, each URL that is opened via OpenURL() will be
	 * compared against the white-list.  If any entry in the white-list is a prefix
	 * of the URL to open, the operation is allowed, otherwise the request to open
	 * the URL is ignored, and OpTVAppMessageListener::OnOpenForbiddenUrl()
	 * is called to signal to the UI that it happened.
	 *
	 * If the whitelist it set to NULL, or SetWhitelist is not called,
	 * DocumentManager will not block anything.
	 *
	 * @param new_whitelist list of strings
	 *  */
	void			SetWhitelist(OpAutoVector<OpString>* new_whitelist);
#endif // DOM_LOAD_TV_APP

private:

	class SecurityCheckCallback : public OpSecurityCheckCallback, public ES_ThreadListener
	{
	public:
		SecurityCheckCallback(DocumentManager *docman)
			: m_docman(docman),
			  m_done(FALSE),
			  m_suspended(FALSE),
			  m_redirect(FALSE),
			  m_allowed(FALSE),
			  m_referrer(),
			  m_check_if_expired(FALSE),
			  m_reload(FALSE),
			  m_invalidated(FALSE) {}

		virtual void OnSecurityCheckSuccess(BOOL allowed, ChoicePersistenceType type = PERSISTENCE_NONE);
		virtual void OnSecurityCheckError(OP_STATUS error);
		void SetURLs(URL& url, const DocumentReferrer& referrer);
		void PrepareForSuspending(BOOL check_if_expired, BOOL reload, const OpenURLOptions& options);
		void PrepareForSuspendingRedirect();
		void Invalidate() { m_invalidated = TRUE; };
		void Reset();

		/** Call when a suspended security check finishes regardless of its result. */
		void SecurityCheckFinished();

		virtual OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal);

		DocumentManager *m_docman;
		BOOL m_done;
		BOOL m_suspended;
		BOOL m_redirect;
		BOOL m_allowed;

		URL m_url;
		DocumentReferrer m_referrer;
		BOOL m_check_if_expired;
		BOOL m_reload;
		BOOL m_invalidated;
		OpenURLOptions m_options;
	};

	friend class SecurityCheckCallback;

	SecurityCheckCallback *security_check_callback;

#ifdef DOM_LOAD_TV_APP
	OpAutoVector<OpString>* whitelist;
	/**< The list of urls that this documentmanager is allowed to load, or NULL. */
#endif
};

class DocumentTreeIterator
{
protected:
	const DocumentManager *start;
	DocumentManager *current;
	BOOL include_this, include_iframes, include_empty;

	FramesDocElm* NextLeaf(FramesDocElm* frame) const;
	/**< Find the next leaf node that follows 'frame' in the document tree, but
	     confine search to the subtree defined by the 'start' member. */

public:
	DocumentTreeIterator();
	DocumentTreeIterator(Window *window);
	DocumentTreeIterator(DocumentManager *docman);
	DocumentTreeIterator(FramesDocElm *frame);
	DocumentTreeIterator(FramesDocument *document);

	void SetIncludeThis();
	/**< Include the DocumentManager/FramesDocElm/FramesDocument object that the
	     iterator was constructed with in the iteration.  If not called, that
	     object is not included. */

	void SetExcludeIFrames();
	/**< Exclude DocumentManager/FramesDocElm/FramesDocument objects that belong
	     to iframes.  If not called, such objects are included. */

	void SetIncludeEmpty();
	/**< Include DocumentManager/FramesDocElm objects that are empty, that is,
	     that have no current document.  If not called, such objects are not
	     included. */

	BOOL Next(BOOL skip_children = FALSE);
	/**< Moves the iterator to the next position.  If there was a next position,
	     TRUE is returned, otherwise FALSE is returned.  If 'skip_children' is
	     TRUE, children of the current position (frames or iframes in the
	     current document and their children, that is) are skipped. */

	DocumentManager *GetDocumentManager();
	/**< Returns the current DocumentManager.  Can only be called after a call to
	     Next() that returned TRUE. */

	FramesDocElm *GetFramesDocElm();
	/**< Returns the current FramesDocElm.  Note: if the current document is
	     the top document in a window, this function returns NULL.  Can only be
	     called after a call to Next() that returned TRUE. */

	FramesDocument *GetFramesDocument();
	/**< Returns the current FramesDocument.  Note: if SetIncludeEmpty() has been
	     called, this function may return NULL.  Can only be called after a call
	     to Next() that returned TRUE. */

	void SkipTo(DocumentManager* doc_man);
	/**< Allows the user to set the position of the iterator. Useful to skip a
	     portion of the iteration. It's the caller's job to make sure that
	     doc_man and the element the iterator was initialised with
	     are consistant. */
};

#endif // DOCHAND_DOCMAN_H
