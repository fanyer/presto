/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2002-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef _FRAMES_DOCUMENT_
#define _FRAMES_DOCUMENT_

#include "modules/display/vis_dev.h"
#include "modules/doc/doc.h"
#include "modules/doc/externalinlinelistener.h"
#include "modules/doc/caret_manager.h"
#include "modules/doc/lie_hashtable.h"
#include "modules/doc/doctypes.h"
#include "modules/doc/documentorigin.h"
#include "modules/dochand/docman.h"
#include "modules/forms/formmanager.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/logdoc.h"
#include "modules/logdoc/opelminfo.h"
#include "modules/layout/layout_fixed_point.h"
#include "modules/url/url_lop_api.h"
#include "modules/util/OpHashTable.h"
#include "modules/util/OpRegion.h"
#include "modules/util/adt/opvector.h"
#include "modules/windowcommander/OpViewportController.h"
#include "modules/security_manager/include/security_manager.h"
#ifdef URL_FILTER
#include "modules/content_filter/content_filter.h"
#endif // URL_FILTER

#ifdef VISITED_PAGES_SEARCH
# include "modules/search_engine/VisitedSearch.h"
#endif // VISITED_PAGES_SEARCH

#ifdef WEBFEEDS_DISPLAY_SUPPORT
# include "modules/webfeeds/webfeeds_api.h"
#endif // WEBFEEDS_DISPLAY_SUPPORT

#ifdef PLUGIN_AUTO_INSTALL
# include "modules/util/adt/oplisteners.h"
#endif // PLUGIN_AUTO_INSTALL

#ifdef PAGED_MEDIA_SUPPORT
# if 1 // backwards compatible
#  include "pagedescription.h" // Remove when all code is fixed
# else
class PageDescription;
# endif
#endif // PAGED_MEDIA_SUPPORT
#define DOC_THREAD_MIGRATION

#include "modules/pi/OpKeys.h"

#include "modules/wand/wand_module.h"
#include "modules/wand/wandmanager.h"

class HTML_Document;
class URL_DataDescriptor;
class ReplacedContent;
class OpDocumentEdit;
class DocumentState;
class DocumentFormSubmitCallback;
class DocumentInteractionContext;
class FramesDocElm;
class LoadInlineElm;
class FramesDocument;
class HEListElm;
class LayoutWorkplace;
class TextSelection;
class SelectionBoundaryPoint;

class LoadingImageElementIterator;
class DOM_Environment;
class ES_Runtime;
class ES_ThreadScheduler;
class ES_TerminatingThread;
class ES_AsyncInterface;
class OnLoadSender;
class AsyncOnloadChecker;
#ifdef MEDIA_SUPPORT
class Media;
#endif // MEDIA_SUPPORT

#include "modules/prefs/prefsmanager/collections/pc_display.h"

#ifdef SCOPE_PROFILER
class OpProbeTimeline;
#endif // SCOPE_PROFILER

#if defined(DOCUMENT_EDIT_SUPPORT) || defined(KEYBOARD_SELECTION_SUPPORT)
class CaretPainter;
#endif // DOCUMENT_EDIT_SUPPORT || KEYBOARD_SELECTION_SUPPORT

#ifdef USE_OP_CLIPBOARD
class ClipboardListener;
#endif // USE_OP_CLIPBOARD


enum ESDocException
{
	ES_DOC_EXCEPTION_NONE = 0,
	ES_DOC_EXCEPTION_UNSUPPORTED_OPEN, // Document.open() failed because it was used in a way we do not support (e.g. script in iframe opens parent document)
	ES_DOC_EXCEPTION_XML_OPEN, // Tried to document.open() an XML document.
};

enum HistoryNavigationMode
{
	HISTORY_NAV_MODE_AUTOMATIC = 1,
	HISTORY_NAV_MODE_COMPATIBLE,
	HISTORY_NAV_MODE_FAST
};

enum RespondToMediaType
{
	RESPOND_TO_MEDIA_TYPE_NONE,
	RESPOND_TO_MEDIA_TYPE_HANDHELD,
	RESPOND_TO_MEDIA_TYPE_TV,
	RESPOND_TO_MEDIA_TYPE_SCREEN,
	RESPOND_TO_MEDIA_TYPE_PROJECTION
};

enum FramesPolicy
{
	FRAMES_POLICY_DEFAULT = -1,
	FRAMES_POLICY_NORMAL = 0,
	FRAMES_POLICY_FRAME_STACKING = 1,
	FRAMES_POLICY_SMART_FRAMES = 2
};

#define ABS_POSITION_NORMAL		0
#define ABS_POSITION_STATIC		1
#define ABS_POSITION_SCALE		2

#define WORD_BREAK_NORMAL              0
#define WORD_BREAK_AGGRESSIVE          1
#define WORD_BREAK_EXTREMELY_AGGRESSIVE     2


#define SHOW_IFRAMES_NONE 0
#define SHOW_IFRAMES_SAME_SITE 1
#define SHOW_IFRAMES_SAME_SITE_AND_EXCEPTIONS 2
#define SHOW_IFRAMES_ALL 3

#ifdef CONTENT_MAGIC
#define CONTENT_MAGIC_SIZE	80
#endif // CONTENT_MAGIC

typedef OP_STATUS OP_LOAD_INLINE_STATUS;

// The button_or_delta parameter have several parts which depend on the event.
// For a mouseup/down it's a sequence_count (think doubleclick) and the button.
// These macros are made to handle that.

// click_indicator is used for MouseUp and TouchEnd events to indicate that this might be a click. It shouldn't be
// non-zero if the mouse has moved around a lot. Can only be 0 and 1
#define MAKE_SEQUENCE_COUNT_CLICK_INDICATOR_AND_BUTTON(sequence_count, click_indicator, button) (((sequence_count) << 16) | ((click_indicator) << 15) | ((button & 0x000f)))
#define MAKE_SEQUENCE_COUNT_AND_BUTTON(sequence_count, button) (((sequence_count) << 16) | 1 << 15 | ((button & 0x000f)))
#define EXTRACT_SEQUENCE_COUNT(sequence_count_and_button) (((sequence_count_and_button) >> 16) & 0xffff)
#define EXTRACT_MOUSE_BUTTON(sequence_count_and_button) ((MouseButton)((sequence_count_and_button) & 0x000f))
#define EXTRACT_CLICK_INDICATOR(sequence_count_and_button) ((((sequence_count_and_button) & 0x8000) >> 15) != 0)

#ifdef _PLUGIN_SUPPORT_
/**
 * A listener interface that allows EmbedContents to receive notifications from FramesDocument
 * about a plugin being available for installation. This changes the EMBED placeholder state
 * so that the user instantly sees the plugin availability. Needs platform plugin install manager.
 *
 * As of asynchronous plug-in detection, this interface also broadcasts plug-in
 * detected events. The purpose is to refresh embed contents that become
 * available as we continue detection in the background.
 */
class PluginInstallationListener
{
public:
	/**
	 * Called when a plugin is available for installation.
	 *
	 @param[in] mime_type The mimetype of the plugin that became available
	 */
	virtual void OnPluginAvailable(const uni_char* mime_type) { }

	/**
	 * Called when a plugin is detected.
	 *
	 @param[in] mime_type The mimetype of the plugin that was detected
	 */
	virtual void OnPluginDetected(const uni_char* mime_type) { }
};
#endif // _PLUGIN_SUPPORT_


/**
 * Contains constants used as return values from FramesDocument::LoadInline
 */
class LoadInlineStatus : public OpStatus
{
private:
	LoadInlineStatus();
public:
    enum
    {
        LOADING_STARTED   = USER_SUCCESS + 0, // loading or reloading
        USE_LOADED        = USER_SUCCESS + 1, // use cached url

		LOADING_CANCELLED	= USER_ERROR + 0, // loading cancelled
		LOADING_REFUSED		= USER_ERROR + 1  // loading explicitly suppressed
    };
};


class DelayedLayoutListener
  : public Link
{
public:
	virtual void LayoutContinued() = 0;
};


/**
 * Stores information about the current status of inlines in an
 * objects, including images, but also including scripts, stylesheets,
 * plugin data and other inlines. Used to determine and show progress
 * and loading status.
 */
struct ImageLoadingInfo
{
	/**
	 * Total number of inlines ever processed on the page. This
	 * typically increases as the document is parsed.
	 */
	int total_count;

	/**
	 * The size of the inlines as far as we know. Often we don't know
	 * the size until an inline is already loaded so it's often not
	 * that much larger than loaded_size.
	 */
	OpFileLength total_size;

	/**
	 * The number of inlines that has been successfully loaded or
	 * permanently failed.
	 */
	int loaded_count;

	/**
	 * The size of the inlines that has been loaded.
	 */
	OpFileLength loaded_size;

#ifdef WEB_TURBO_MODE
	/**
	* The total number of bytes transferred from the Turbo proxy
	* when loading the inlines. Including HTTP protocol overhead.
	*/
	OpFileLength turbo_transferred_bytes;

	/**
	* The total number of bytes received by the Turbo proxy when
	* loading the inlines. Including HTTP protocol overhead.
	*/
	OpFileLength turbo_original_transferred_bytes;
#endif // WEB_TURBO_MODE


	ImageLoadingInfo()
		: total_count(0),
		  total_size(0),
		  loaded_count(0),
		  loaded_size(0)
#ifdef WEB_TURBO_MODE
		  ,turbo_transferred_bytes(0)
		  ,turbo_original_transferred_bytes(0)
#endif // WEB_TURBO_MODE
	{
	}

	/**
	 * Clears all values.
	 */
	void Reset()
	{
		total_count  = 0;
		total_size   = 0;
		loaded_count = 0;
		loaded_size  = 0;
#ifdef WEB_TURBO_MODE
		turbo_transferred_bytes = 0;
		turbo_original_transferred_bytes = 0;
#endif // WEB_TURBO_MODE
	}

	/**
	 * Adds the values of the other ImageLoadingInfo to this
	 * ImageLoadingInfo.
	 *
	 * @param[in] other The other ImageLoadingInfo to add to this.
	 */
	void AddTo(const ImageLoadingInfo& other)
	{
		total_count  += other.total_count;
		total_size   += other.total_size;
		loaded_count += other.loaded_count;
		loaded_size  += other.loaded_size;
#ifdef WEB_TURBO_MODE
		turbo_transferred_bytes += other.turbo_transferred_bytes;
		turbo_original_transferred_bytes += other.turbo_original_transferred_bytes;
#endif // WEB_TURBO_MODE
	}

	/**
	 * Checks if the count of loaded inlines is the same as the total
	 * count.
	 *
	 * @returns TRUE if all inlines are loaded.
	 */
	BOOL AllLoaded() const
	{
		return loaded_count >= total_count;
	}
};

/**
 * The same as ImageLoadingInfo but includes documents as well.
 *
 * @see ImageLoadingInfo
 */
struct DocLoadingInfo
{
	OpFileLength total_size;
	OpFileLength loaded_size;

#ifdef WEB_TURBO_MODE
	OpFileLength turbo_transferred_bytes;
	OpFileLength turbo_original_transferred_bytes;
#endif // WEB_TURBO_MODE

	ImageLoadingInfo images;

	DocLoadingInfo()
		: total_size(0),
		  loaded_size(0)
#ifdef WEB_TURBO_MODE
		  ,turbo_transferred_bytes(0)
		  ,turbo_original_transferred_bytes(0)
#endif // WEB_TURBO_MODE
	{
	}

	/**
	 * @see ImageLoadingInfo::Reset()
	 */
	void Reset()
	{
		total_size   = 0;
		loaded_size  = 0;
#ifdef WEB_TURBO_MODE
		turbo_transferred_bytes = 0;
		turbo_original_transferred_bytes = 0;
#endif // WEB_TURBO_MODE
		images.Reset();
	}

	/**
	 * @param[in] other The DocLoadingInfo to add to this.
	 *
	 * @see ImageLoadingInfo::AddTo(ImageLoadingInfo& other)
	 */
	void AddTo(const DocLoadingInfo& other)
	{
		total_size   += other.total_size;
		loaded_size  += other.loaded_size;
#ifdef WEB_TURBO_MODE
		turbo_transferred_bytes += other.turbo_transferred_bytes;
		turbo_original_transferred_bytes += other.turbo_original_transferred_bytes;
#endif // WEB_TURBO_MODE
		images.AddTo(other.images);
	}

	/**
	 * Determines if the stored size and count values indicate that we
	 * have loaded everything we should.
	 */
	BOOL AllLoaded() const
	{
		return loaded_size >= total_size;
	}
};

/**
 * This class creates and registers URL generators for style sheets
 * used by wrapper document.
 */
class DocOperaStyleURLManager
{
public:
	enum WrapperDocType
	{
		IMAGE,
#ifdef MEDIA_HTML_SUPPORT
		MEDIA,
#endif // MEDIA_HTML_SUPPORT
		WRAPPER_DOC_TYPE_COUNT
	};

	static OP_STATUS Construct(WrapperDocType type);
	/**< Construct and register a handler for the stylesheet URL of the type
		 of . Returns silently if a handler for this type
		 has already been constructed. */

	~DocOperaStyleURLManager();

private:
	DocOperaStyleURLManager();
	OP_STATUS CreateGenerator(WrapperDocType type);

	class Generator
	: public OperaURL_Generator
	{
	public:
		OP_STATUS Construct(PrefsCollectionFiles::filepref pref, const char* name);

	private:
		virtual OperaURL_Generator::GeneratorMode GetMode() const { return OperaURL_Generator::KFileContent; }
		virtual OpStringC GetFileLocation(URL &url, OpFileFolder &fldr, OP_STATUS &status);

		OpString m_path;
	};

	Generator* m_url_generators[WRAPPER_DOC_TYPE_COUNT];
};

/**
 * Chainable options for FramesDocument::LoadInline().
 */
class LoadInlineOptions
{
public:
	/**
	 * An already loaded resource will be loaded again conditionally.
	 * @see URL_Reload_Policy::URL_Reload_Conditional.
	 */
	LoadInlineOptions &ReloadConditionally(BOOL value = TRUE)
	{
		reload = value;
		return *this;
	}

	/**
	 * No progressbar will be activated for this inline, but it will still be
	 * mentioned in progressbars started later or earlier.
	 */
	LoadInlineOptions &LoadSilent(BOOL value = TRUE)
	{
		load_silent = value;
		return *this;
	}

	/**
	 * If TRUE, loading this resource will delay the
	 * document's load event. This is the default
	 * behavior. If FALSE, loading the inline will
	 * not delay the document's load event.
	 */
	LoadInlineOptions &DelayLoad(BOOL value = TRUE)
	{
		delay_load = value;
		return *this;
	}

	/**
	 * The url mentioned is from a user supplied css file which means that we
	 * will bypass some security checks.
	 */
	LoadInlineOptions &FromUserCSS(BOOL value = TRUE)
	{
		from_user_css = value;
		return *this;
	}

	/**
	 * Images will be loaded even if the user setting is to not load them, or
	 * even if the loading was aborted. This is used for instance when the user
	 * explicitly wants to load a certain image.
	 */
	LoadInlineOptions &ForceImageLoad(BOOL value = TRUE)
	{
		force_image_load = value;
		return *this;
	}

#ifdef SPECULATIVE_PARSER
	/**
	 * This is a speculative load initiated from the speculative parser. Used to
	 * keep track of speculative loaded URLs returned by
	 * opera.getSpeculativeParserUrls().
	 */
	LoadInlineOptions &SpeculativeLoad(BOOL value = TRUE)
	{
		speculative_load = value;
		return *this;
	}
#endif // SPECULATIVE_PARSER

	/**
	 * The loading priority will be overridden to the specified value. This is
	 * used for instance in speculative parsing when we don't want to specify
	 * the actual inline type because we have no real element for it yet.
	 */
	LoadInlineOptions &ForcePriority(int value)
	{
		force_priority = value;
		return *this;
	}

	LoadInlineOptions()
		: reload(FALSE),
		  load_silent(FALSE),
		  from_user_css(FALSE),
		  force_image_load(FALSE),
		  delay_load(TRUE),
#ifdef SPECULATIVE_PARSER
		  speculative_load(FALSE),
#endif // SPECULATIVE_PARSER
		  force_priority(URL_LOWEST_PRIORITY)
	{
	}

private:
	friend class FramesDocument;
	unsigned reload:1, load_silent:1, from_user_css:1, force_image_load:1, delay_load:1;
#ifdef SPECULATIVE_PARSER
	unsigned speculative_load:1;
#endif // SPECULATIVE_PARSER
	int force_priority;
};

class AsyncLoadInlineElm;
/**
 * A listener class being notified with the loading status when loading of an asynchronous inline has just started.
 */
class AsyncLoadInlineListener : public Link
{
public:
	/**
	 * A callback function called when an asynchronous loading has started.
	 * @param async_inline_elm inline element being loaded asynchronously (@see AsyncLoadInlineElm)
	 * @param load_status - inline loading operation status
	 */
	virtual void OnInlineLoad(AsyncLoadInlineElm* async_inline_elm, OP_LOAD_INLINE_STATUS load_status) = 0;
};

/**
 * Class representing an inline element which has to be loaded asynchronously
 */
class AsyncLoadInlineElm :
	public MessageObject,
	public ListElement<AsyncLoadInlineElm>
{
public:
	/**
	 * Ctor
	 *
	 * @param doc - a document the inline belongs to
	 * @param url - inline's url
	 * @param inline_type - a type of the inline
	 * @param html_elm - an instance of HTML_Element representing the inline
	 * @param options - options used to load this inline. @see LoadInlineOptions.
	 * @param listen - asynchronous loading listener (@see AsyncLoadInlineListener)
	 *
	 */
	AsyncLoadInlineElm(FramesDocument* doc, URL url, InlineResourceType inline_type, HTML_Element *html_elm, const LoadInlineOptions &options, AsyncLoadInlineListener *listen)
	: frm_doc(doc)
	, inline_url(url)
	, inline_type(inline_type)
	, element(html_elm)
	, options(options)
	, listener(listen)
	{}

	/**
	 * Registers the passed in asynchronous loading listener.
	 */
	OP_STATUS AddListener(AsyncLoadInlineListener *listener)
	{
		OP_ASSERT(listener);
		this->listener = listener;

		return OpStatus::OK;
	}

	/**
	 * Unregisters the passed in asynchronous loading listener.
	 */
	OP_STATUS RemoveListener(AsyncLoadInlineListener *listener)
	{
		OP_ASSERT(this->listener == listener);
		this->listener = NULL;
		return OpStatus::OK;
	}

	virtual ~AsyncLoadInlineElm();

	/**
	 * Schedules an inline loading.
	 */
	void LoadInline();

	// MessageObject's interface.
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	URL& GetInlineURL() { return inline_url; }
	HTML_Element* GetInlineElement() const { return element; }
	InlineResourceType GetInlineType() const { return inline_type; }
	OpSecurityState& GetSecurityState() { return security_state; }
	void SetSecurityState(OpSecurityState& state)
	{
		security_state.host_resolving_comm = state.host_resolving_comm;
		security_state.suspended = state.suspended;
		state.host_resolving_comm = NULL;
	}

protected:
	FramesDocument* frm_doc;
	URL inline_url;
	InlineResourceType inline_type;
	HTML_Element* element;
	LoadInlineOptions options;
	AsyncLoadInlineListener* listener;
	OpSecurityState security_state;
	void LoadInlineNow();
};

class AsyncLoadInlineElmHashTable : public OpAutoPointerHashTable<HTML_Element, List<AsyncLoadInlineElm> >
{
protected:
	virtual void Delete(void * data)
	{
		List<AsyncLoadInlineElm>* list = static_cast<List<AsyncLoadInlineElm> *>(data);
		list->Clear();
		OP_DELETE(list);
	}
};

#ifdef WAND_SUPPORT
class WandInserter : public ES_ThreadListener
{
private:
	FramesDocument* frames_doc;
public:
	WandInserter(FramesDocument* doc) : frames_doc(doc) { }

	// See base class documentation
	virtual OP_STATUS Signal(ES_Thread *, ES_ThreadSignal signal);
};
#endif // WAND_SUPPORT

/**
 * This class stores loads and loads of information regarding the
 * current document. It has links to sub frames, to iframes, to
 * LogicalDocument and much more. If something is happening with a
 * document, you can be sure FramesDocument is there.
 *
 * @author Everyone
 */
class FramesDocument
  :
    public MessageObject
#ifdef WEBFEEDS_DISPLAY_SUPPORT
  , public OpFeedListener
#endif // WEBFEEDS_DISPLAY_SUPPORT
  , public AsyncLoadInlineListener
{
private:
	DocumentManager*	doc_manager;
	URL					url;
	URL					aux_url_to_handle_loading_messages_for;
	DocumentOrigin*		origin;
	BOOL				is_current_doc;
	URL_InUse			reserved_url;
	URL_InUse			url_in_use;

	const int					sub_win_id;

	DocumentState*	document_state;

	BOOL			own_logdoc;
	LogicalDocument*	old_logdoc;
	LogicalDocument*	logdoc;
	URL_DataDescriptor*	url_data_desc;
#ifdef SPECULATIVE_PARSER
	URL_DataDescriptor*	speculative_data_desc;
#endif // SPECULATIVE_PARSER

	URL				loaded_from_url;
	/**< Empty initially and after ReloadedUrl().  Set to the current
	     final target of the document's URL once we've started loaded
	     from it (successfully created a data descriptor from the URL)
	     and is thereafter not changed (except by ReloadedUrl()) and
	     represents the actual URL of the document.  Mostly used to
	     prevent MSG_URL_MOVED messages from having any effect on the
	     displayed URL and the document's security context. */

	int				sub_win_id_counter;
	FramesDocElm*	frm_root;
	FramesDocElm*	active_frm_doc;

	FramesDocElm*	ifrm_root; // list of IFRAME elements


	HTML_Document*	doc;
	BOOL			check_inline_expire_time;
	BOOL			keep_cleared;
	BOOL			remove_from_cache;
	BOOL			compatible_history_navigation_needed;
	BOOL			history_reload;
	/**< Set to FALSE by constructor.  Set to TRUE when reparsing the
	     document during history navigation.  Set to FALSE when
	     reloading the document otherwise (ordinary reload, reload
	     from cache, (non-redirecting) META refresh and so on.)  Not
	     changed otherwise.  Used when loading inlines to determine
	     caching policies. */
	BOOL			navigated_in_history;
	/**< Set to FALSE by constructor.  Set to TRUE when the document
	     is navigated *away*from* in history.  Thus, is TRUE when the
	     document is navigated back to in history, and is then used to
	     disable redirecting META refresh.  Set to FALSE again by
	     reload. */
	BOOL			document_is_reloaded;
	/**< Set to FALSE by constructor.  Set to TRUE when this document
	     is explicitly reloaded. Parent documents that are reloaded
		 do not propagate this flag to their children, i.e. if a page
		 containing an iframe is reloaded, its iframe will not have
		 this flag set to TRUE. */
	HistoryNavigationMode use_history_navigation_mode;
	HistoryNavigationMode override_history_navigation_mode;

#ifdef REFRESH_DELAY_OVERRIDE
	/**
	 * Contains the list of ReviewRefreshDelayCallbackImpl objects associated
	 * with this document.
	 *
	 * @see TWEAK_DOC_REFRESH_DELAY_OVERRIDE
	 * @see CheckRefresh()
	 */
	Head active_review_refresh_delay_callbacks;
#endif // REFRESH_DELAY_OVERRIDE

	BOOL			parse_xml_as_html;

	int				doc_start_elm_no;

#ifndef HAS_NOTEXTSELECTION
	BOOL				selecting_text_is_ok;
    BOOL                is_selecting_text;
	BOOL                start_selection;    // We might want to delay start of a new selection until mouse is moved if
	int                 start_selection_x;  // we click something that may run a script that should read the selection.
	int                 start_selection_y;
	BOOL                was_selecting_text; // Used in HTML_Element::HandleEvent, to find out if we created a new selection so we don't click links.
#endif // !HAS_NOTEXTSELECTION
	TextSelection*		text_selection;
#ifndef MOUSELESS
    BOOL                is_moving_separator;
#endif // !MOUSELESS

#ifdef KEYBOARD_SELECTION_SUPPORT
	BOOL				keyboard_selection_mode_enabled;
#endif // KEYBOARD_SELECTION_SUPPORT


#ifndef MOUSELESS
	FramesDocElm*	drag_frm;

	/**
	 * Used to see if a cursor has moved too far for a mouse up to be considered a click. Important to
	 * do this in screen coordinates rather than document coordinates in case the document is scaled.
	 */
	OpPoint m_last_mouse_down_position_non_scaled;

	/**
	 * If the mouse has at any time moved too far from the mouse down position this will be FALSE
	 */
	BOOL m_mouse_up_may_still_be_click;
#endif // !MOUSELESS

#ifdef TOUCH_EVENTS_SUPPORT
	/**
	 * Used to see if a touch has moved too far for a touch up to be considered a "click". Important to
	 * do this in screen coordinates rather than document coordinates in case the document is scaled.
	 */
	OpPoint m_last_touch_down_position_non_scaled;

	/**
	 * If the touch point has at any time moved too far from the touch down position this will be FALSE
	 */
	BOOL m_touch_up_may_still_be_click;

#endif // TOUCH_EVENTS_SUPPORT

	BOOL			frames_stacked;
	FramesDocElm*	loading_frame;
	BOOL			smart_frames;

#ifdef XML_EVENTS_SUPPORT
	/**
	 * If there exists any xml events in the document. If there are, we have to
	 * complicate some operations so it's nice to know when there are none.
	 */
	BOOL			has_xml_events;
#endif // XML_EVENTS_SUPPORT

	/**
	 * @see SetLocked()
	 * @see GetLocked()
	 */
	int				locked;

#ifdef JS_LAYOUT_CONTROL
	BOOL            pause_layout;
#endif

#ifdef DOM_FULLSCREEN_MODE
	/** The current fullscreen element if any. */
	AutoNullElementRef fullscreen_element;
	/**
	 * @see SetFullscreenElementObscured()
	 * @see IsFullscreenElementObscured()
	 */
	BOOL fullscreen_element_obscured;
#endif // DOM_FULLSCREEN_MODE

	OpString		wrapper_doc_source;
	unsigned		wrapper_doc_source_read_offset;

	BOOL			media_handheld_responded;
	RespondToMediaType
					respond_to_media_type;
	short			frames_policy;

	OP_STATUS       LoadData(BOOL format, BOOL aborted);

#ifdef SPECULATIVE_PARSER
	OP_STATUS       LoadDataSpeculatively();
#endif // SPECULATIVE_PARSER

	void 			LocalFindTarget(const uni_char* &win_name, int &sub_win_id);

	void			ClearInlineList();

	BOOL				GetLocked() { return locked > 0; }
	void				SetLocked(BOOL val) { if (val) locked++; else { OP_ASSERT(locked > 0); locked--; }}

	BOOL				has_delayed_updates;

	ES_Runtime*			es_runtime;
	ES_ThreadScheduler*	es_scheduler;
	ES_AsyncInterface*	es_async_interface;

	BOOL				is_constructing_dom_environment;
	BOOL				override_es_active;

	DOM_Environment*	dom_environment;
	DOM_Object*			js_window;


	BOOL has_sent_domcontent_loaded;
	BOOL es_recent_layout_changes;

	struct
	{
		// onload has been called for this document
		unsigned int onload_called:1;
		unsigned int onload_ready:1;
		unsigned int onload_pending:1;
		unsigned int inhibit_parent_elm_onload:1; ///< Sometimes a parent iframe shouldn't get an onload event. In that case this is set to TRUE.
		unsigned int onload_thread_running_in_subtree:1; ///< Only used on TopDocument. Set to 1 while an onload event is processed. During that time no onload events will be queued in other schedulers (to guarantee the right order).
		unsigned int inhibit_onload_about_blank:1; ///< Set if about:blank onload events should not go ahead.
		unsigned int onpopstate_pending:1;
	} es_info;

	AsyncOnloadChecker* onload_checker;

	// Document being generated by a script in this document, replacing this document.
	// Invariant: either (es_generated_document == NULL)
	//            or (es_generated_document->es_replaced_document == this)
	//            must be TRUE.
	FramesDocument*	es_generated_document;

	// Document being replaced by this document (which is being generated).
	// Invariant: either (es_replaced_document == NULL)
	//            or (es_replaced_document->es_generated_document == this)
	//            must be TRUE.
	FramesDocument* es_replaced_document;

	unsigned        serial_nr, form_value_serial_nr;

	ES_Thread*		restore_form_state_thread;

#ifdef XML_EVENTS_SUPPORT
	Head			xml_events_registrations;
#endif // XML_EVENTS_SUPPORT

	Head			delayed_layout_listeners;
	Tree			delete_iframe_root;

	unsigned		stream_position_base;
	const uni_char *current_buffer;

	// Set of inlines this document references.
	LoadInlineElmHashTable inline_hash;
	// Friend access required to change image_info.
	friend class LoadInlineElm;
	friend class HEListElm;

	// Set of inlines which loading is halted to a security check.
	AsyncLoadInlineElmHashTable async_inline_elms;

	// List of external inline urls waiting for opera to go out of offline mode.
	Head			waiting_for_online_mode;

	HEListElm*		last_helm;

	ImageLoadingInfo
		            image_info;

#ifdef SHORTCUT_ICON_SUPPORT
	URL				favicon;
#endif // SHORTCUT_ICON_SUPPORT

	Head			pages;

#ifdef _PRINT_SUPPORT_
	time_t*			print_time;
#endif

#ifdef PAGED_MEDIA_SUPPORT
	int				iframe_first_page_offset;

	PageDescription*	current_page;

	int				restore_current_page_number;
#endif // PAGED_MEDIA_SUPPORT
	union
	{
		struct
		{
			unsigned int	load_images:1;
			unsigned int	show_images:1;
#ifdef FIT_LARGE_IMAGES_TO_WINDOW_WIDTH
			unsigned int	fit_images:1;
#endif // FIT_LARGE_IMAGES_TO_WINDOW_WIDTH
			unsigned int	data_desc_done:1; ///< TRUE when we have read all data from url_data_desc
		} packed;
		unsigned long
					packed_init;
	};

	BOOL			loading_aborted;
	BOOL			local_doc_finished;
	BOOL			doc_tree_finished;
	BOOL			document_finished_at_least_once;
	/**< The document finished loading at least once in the past, i.e. it's like document_finished
	     but doesn't get reset if new resources are loaded in this document */

	BOOL			delay_doc_finish;
	/**< Set to TRUE if document should delay performing the finished document
	     check (i.e., CheckFinishDocument().) Finishing up eagerly
		 sits uncomfortably with the scripting engine while a garbage collection
		 is active, for instance. @see SetDelayDocumentFinish(). */

	BOOL			posted_delay_doc_finish;
	/**< TRUE if a message has been posted to finish up the document. */

	BOOL			is_multipart_reload;
	/**< A MSG_MULTIPART_RELOAD message has been received signalling that the
	     current body part has ended.  LoadData() should behave as if the URL
	     has stopped loading (status!=URL_LOADING) even though it hasn't. */

#ifdef MEDIA_SUPPORT
	OpVector<Media> media_elements;
#endif // MEDIA_SUPPORT

#ifdef CONTENT_MAGIC
	AutoNullElementRef	content_magic_start;
	BOOL				content_magic_found;
	int					content_magic_character_count;
	long				content_magic_position;
#endif // CONTENT_MAGIC

	BOOL			era_mode;
	LayoutMode		layout_mode;
	short			flex_font_scale; // if flexible font mode:
									 // font-size = "this mode font size" +
									 //             ("previous mode font size" - "this mode font size") * flex_font_scale / 100

	short			abs_positioned_strategy;

	short           aggressive_word_breaking;

	unsigned int	old_win_width;	// CheckERA_LayoutMode only makes sense if the window width actually changes. Cache it here.
									// If nothing else this is a performance fix, especially for iframes. But also a fix for a
									// scrollbar "keep vertical" bug, that appeared because font sizes were recalculated
									// synchronously with the iframe view width change.

	BOOL			override_media_style;

	BOOL			is_generated_doc;

	BOOL			contains_only_opera_internal_scripts;
	const BOOL		is_plugin;

#ifdef WEBFEEDS_DISPLAY_SUPPORT
	BOOL			is_inline_feed;
	BOOL			feed_listener_added;
#endif // WEBFEEDS_DISPLAY_SUPPORT

	double          start_navigation_time_ms; /**< The time the navigation to this document started. Used by the Performance.now() script API. */

	BOOL			is_reflow_msg_posted;
	double			reflow_msg_delayed_to_time; /**< 0.0 if not delayed. Otherwise the GetRuntimeMS() for the time it will fire. */
	BOOL			is_reflow_frameset_msg_posted;

	int				wait_for_styles;
	int				wait_for_paint_blockers;
	BOOL			set_properties_reflow_requested; // a set_properties reflow was requested but typically
													 // not executed because if IsWaitingForStyles()

	BOOL			has_blink;

#ifdef WAND_SUPPORT
	BOOL			has_wand_matches;
	BOOL			is_wand_submitting;
	OpVector<HTML_Element> pending_wand_elements;
	WandInserter*	wand_inserter;
#endif // WAND_SUPPORT

	BOOL			obey_next_autofocus_attr; // flag used by the ATTR_AUTOFOCUS handler
	AutoNullElementRef	element_to_autofocus; // autofocus this if set, NULL otherwise.

	BOOL			display_err_for_next_oninvalid;
	AutoNullElementRef	element_to_refocus; // focus this when suitable (after reflowing) if set.

#ifdef DOCUMENT_EDIT_SUPPORT
	OpDocumentEdit*	document_edit;
#endif
#if defined(DOCUMENT_EDIT_SUPPORT) || defined(KEYBOARD_SELECTION_SUPPORT)
	CaretPainter *caret_painter;
	CaretManager caret_manager;
#endif // DOCUMENT_EDIT_SUPPORT || KEYBOARD_SELECTION_SUPPORT

	BOOL            reflowing;
	BOOL			undisplaying;
	BOOL			need_update_hover_element;

	BOOL			is_being_deleted;
	BOOL			is_being_freed;
	BOOL			is_being_unloaded;
	BOOL			print_copy_being_deleted;
	BOOL			suppress_focus_events;

	/**
	 * Flag to indicate if this is a document purely to be able to
	 * run a javascript url. If so, then we need to supress some
	 * things in it, for instance the onload event, until after
	 * the javascript url has been executed.
	 */
	BOOL			about_blank_waiting_for_javascript_url;

#if defined DOC_HAS_PAGE_INFO && defined _SECURE_INFO_SUPPORT
	URL_InUse		m_security_url_used;
#endif // _SECURE_INFO_SUPPORT

#ifdef _PLUGIN_SUPPORT_
	BOOL			m_asked_about_flash_download;
	/// List of elements where scripts wait for a plugin to be loaded
	OpVector<HTML_Element> m_blocked_plugins;
	/// Stores mime-types of missing plug-ins.
	OpAutoVector<OpString> m_missing_mime_types;
#endif // _PLUGIN_SUPPORT_

#ifdef ON_DEMAND_PLUGIN
	OpPointerSet<HTML_Element>	m_ondemand_placeholders; ///< list of "on-demand" placeholders registered for this document
#endif // ON_DEMAND_PLUGIN

#ifdef VISITED_PAGES_SEARCH
	VisitedSearch::RecordHandle	 m_visited_search;
	BOOL						 m_index_all_text;
#endif // VISITED_PAGES_SEARCH

	/**
	 * This flag indicates whether document was completely parsed before StopLoading was called.
	 * StopLoading may be called either when a user pressed stop or the user navigated away from a page.
	 * It is needed to know whether it is ok to load document from document cache in case user navigated
	 * back to it via history
	 */
	BOOL was_document_completely_parsed_before_stopping;

	OpVector<DocumentInteractionContext> m_document_contexts_in_use;

	/** Keeps a local 'is reflowing' state for all frame_docs. Used to detect if opera is idle, important for testing */
	OpActivity activity_reflow;

	friend class DocumentFormSubmitCallback;

	/**
	 * Before actually loading a form submit we sometimes must
	 * doublecheck with the UI. In those cases we store the current
	 * submit information here until the UI has decided how to
	 * proceed.
	 */
	DocumentFormSubmitCallback *current_form_request;

	HEListElm* inline_data_loaded_helm_next;

	int m_supported_view_modes;
#ifdef GEOLOCATION_SUPPORT
	unsigned int geolocation_usage_count;
#endif // GEOLOCATION_SUPPORT
#ifdef MEDIA_CAMERA_SUPPORT
	unsigned int camera_usage_count;
#endif // MEDIA_CAMERA_SUPPORT

	/**
	 * The contents of a type=text input element upon gaining focus.
	 * When the element loses focus, compare current value against this
	 * snapshot, and trigger onchange actions.
	 */
	OpString focus_input_text;

	/**
	 * Forget about the current form request (clears the pointer,
	 * doesn't delete anything).
	 *
	 * @see current_form_request
	 */
	 void ResetCurrentFormRequest() { current_form_request = NULL; }

	/**
	 * Checks if the form should be submitted and performs the submit if
	 * it should. This might include a
	 * call to the ui so that it can ask the user if the submit is
	 * appropriate (for instance submitting sensitive data on an
	 * unencrypted channel) so the submit might not yet have been
	 * started when this method returns.
	 *
	 * @param[in] continue_after_security_check Don't kill the existing form submit, instead continue with it after the security check. When this is TRUE it must be called from the current_form_request's HandleCallback method.
	 *
	 * @return OpStatus::OK or an error code.
	 *
	 * @see current_form_request
	 */
#ifdef GADGET_SUPPORT
	OP_STATUS		HandleFormRequest(URL form_url, BOOL user_initiated, const uni_char *win_name, BOOL open_in_other_window, BOOL open_in_background, ES_Thread *thread, BOOL continue_after_security_check = FALSE);
#else
	OP_STATUS		HandleFormRequest(URL form_url, BOOL user_initiated, const uni_char *win_name, BOOL open_in_other_window, BOOL open_in_background, ES_Thread *thread);
#endif // GADGET_SUPPORT

	AutoNullElementRef	m_spotlight_elm;

	/**
	 * Helper method for LocalLoadInline. Used to create script nodes for
	 * images that have onload handlers. The node is needed in case the
	 * element is/ends up outside the tree so that we can mark it in a
	 * garbage collection and keep it alive until the onload/onerror handler
	 * has fired.
	 */
	void			ForceImageNodeIfNeeded(InlineResourceType inline_type, HTML_Element* html_elm);

	/**
	 * Call this if an inline is going to be refetched, and the previous
	 * fetch was redirected. This will put the old LoadInlineElm in the right
	 * slot in the hash table, so that the loading messages will be handled
	 * correctly.
	 * @param lie Existing LoadInlineElm that will be refetched.
	 * @return Normal OOM values.
	 */
	OP_STATUS		ResetInlineRedirection(LoadInlineElm *lie, URL *inline_url);

	/**
	 * The actual implementation of the LoadInline variations. Will load an
	 * inline URL and do all the proper steps to make sure it is done right.
	 *
	 * @param img_url The URL that the inline will be loaded from.
	 * @param inline_type Specifying what the inline will be used for.
	 * @param helm A HEListElm already created for this loading, otherwise NULL.
	 * @param html_elm The element associated with the inline. The root of the
	 *                 tree can be used if no element is directly associated.
	 * @param options Options for how to load this inline. @see LoadInlineOptions
	 * @return @see LoadInlineStatus
	 */
	OP_LOAD_INLINE_STATUS
					LocalLoadInline(URL *img_url, InlineResourceType inline_type, HEListElm *helm, HTML_Element *html_elm, const LoadInlineOptions &options = LoadInlineOptions());

	void			StopLoadingInline(LoadInlineElm* lie);

	OP_STATUS		ReflowAndFinishIfLastInlineLoaded();

#ifdef SHORTCUT_ICON_SUPPORT
	void			AbortFaviconIfLastInline();
#endif // SHORTCUT_ICON_SUPPORT

	void			LimitUnusedInlines();

	URL				GetImageURL(int img_id);

	void			SetFrmRootSize();

	/**
	 * Called when information about an inline resource arrives.
	 *
	 * @param[in] lie The inline resource.
	 */
	BOOL			HandleInlineDataLoaded(LoadInlineElm* lie);

	void			SetInlineLoading(LoadInlineElm *lie, BOOL value);
	void			SetInlineStopped(LoadInlineElm *lie);
	/**
	 * Updates the total size/loaded size information in image_info
	 * for the given inline.
	 */
	void			UpdateInlineInfo(LoadInlineElm *lie);
	void			RemoveLoadInlineElm(LoadInlineElm *lie);
	void			RemoveAllLoadInlineElms();

	OP_STATUS		MakeWrapperDoc();
	OP_STATUS		MakeImageWrapperDoc(OpString &buf);
#ifdef MEDIA_HTML_SUPPORT
	OP_STATUS		MakeMediaWrapperDoc(OpString &buf, BOOL is_audio);
#endif // MEDIA_HTML_SUPPORT

	OP_STATUS		InitParsing();

	BOOL			SetLoadingCallbacks(URL_ID url_id);
	void			UnsetLoadingCallbacks(URL_ID url_id);

	// Placeholder

	/**
	 * Parses something we previously treated as XML as HTML.
	 */
	OP_STATUS		ReparseAsHtml(BOOL from_message);

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
	void			PostDelayedActionMessage(int delay_ms = 0);

	/**
	 * Restores form data and scroll positions and such after a
	 * history navigation.
	 *
	 * @returns OpStatus::OK if the operation completed successfully
	 * or an error code otherwise.
	 */
	OP_STATUS		RestoreState();

	/**
	 * This function contains all we need to do when finishing a document,
	 * but which must happen after the onload handler has been run.
	 *
	 * If there is no onload handler (including if there is no javascript)
	 * then this will be run at the end of FinishDocument.
	 */
	OP_STATUS		FinishDocumentAfterOnLoad();

	/**
	 * Helper class for onload sending.
	 */
	friend class OnLoadSender;

	/**
	 * Helper class for onload sending.
	 */
	friend class AsyncOnloadChecker;

	/**
	 * Sends the ONLOAD (and SVGLOAD if an SVG) event to the
	 * document. Should be called when the document is ready for the
	 * onload event.
	 *
	 * This should not be called if
	 * GetTopDocument()->es_info.onload_thread_running_in_subtree is
	 * set because then two onload events might be processed in
	 * parallel. If there is (or might be) an on onload handler so
	 * that a script thread is created, then this will set that flag.
	 */
	OP_STATUS SendOnLoadInternal();

	/**
	 * Helper method for SendOnLoadToFrameElement.
	 */
	void PropagateOnLoadStatus();

	/**
	 * Helper method for onload sending. Will send an onload event to
	 * the HE_FRAME or HE_IFRAME containing this document.
	 *
	 * @param parent_doc The document that has the HE_FRAME/HE_IFRAME
	 * element. Must not be NULL.
	 */
	OP_STATUS SendOnLoadToFrameElement(FramesDocument* parent_doc);

	/**
	 * Helper method to OnLoadReady();
	 */
	static BOOL OnLoadReadyFrame(FramesDocElm *fdelm);

	/**
	 * Checks if all documents in the subtree has set the flag that
	 * they are ready for the onload event.
	 */
	BOOL			OnLoadReady();

	/**
	 * The "restore form state" code runs as a thread in the
	 * ecmascript scheduler to get thread dependencies correct.
	 */
	OP_STATUS		CreateRestoreFormStateThread();

	/**
	 * Scripts sometimes depend on a succesful layout/reflow of the
	 * document and therefore DOM registers a listener to be called
	 * when layout is ready to be queried about layout state. Until it
	 * is called, scripts will be hung.
	 *
	 * <p>This method signals all listeners and removes them for the
	 * listener list.
	 */
	void			SignalAllDelayedLayoutListeners();

	/**
	 * Iframes are deleted asynchronously by registering them in a
	 * list and deleting them when a message arrives. This is the
	 * method that does the deletion.
	 *
	 * @returns FALSE if the deletion couldn't be done right now. In
	 * that case schedule a new deletion attempt. Otherwise it will
	 * return TRUE.
	 */
	BOOL			DeleteAllIFrames();

	HistoryNavigationMode	CalculateHistoryNavigationMode();

	void					RemoveFromCache();

#ifdef VISITED_PAGES_SEARCH
	/**
	 * Informs the search engine that the document is finished parsing
	 * the contents. Sets the handle to NULL.
	 */
	void			CloseVisitedSearchHandle();
#endif // VISITED_PAGES_SEARCH

	/**
	 * Related to GetMediaType. Helper method.
	 */
	CSS_MediaType	RespondTypeToMediaType();

	void			ResetWaitForStyles();
	BOOL			CheckWaitForStylesTimeout();

	void			ExternalInlineStopped(ExternalInlineListener* listener);

	OP_STATUS		ReflowFrames(BOOL set_css_properties);

	// Get inline loads
	LoadInlineElm*	GetInline(URL_ID url_id, BOOL follow_ref = TRUE);

	/**
	 * Starts loading all images even if the setting is to not load any images.
	 * Not to be confused with Window::LoadImages that just checks if images
	 * are set to be loaded and returns TRUE or FALSE.
	 */
	OP_BOOLEAN      LoadImages(BOOL first_image_only);

#ifdef PAGED_MEDIA_SUPPORT
    /**
     * Sets the information in new_page to the documents default
     * values. That includes margins and page area.
     *
     * @todo Document. Find a layout guy who understands this and
     * force him to explain it.
	 *
	 * @todo Move to LayoutWorkplace?
	 */
	void			ClearPage(PageDescription* new_page);

# ifdef _PRINT_SUPPORT_
#  ifdef GENERIC_PRINTING
	int				MakeHeaderFooterStr(const uni_char * str, uni_char * buf, int buf_len, const uni_char * url_name, const uni_char * title, int page_no, int total_pages);
#  endif // GENERIC_PRINTING

	/**
	 * Part of the printing support. Possibly outputs a page header
	 * and a page footer.
	 *
	 * @param[in] vd The print VisualDevice.
	 *
	 * @param[in] page The page description.
	 *
	 * @param[in] horizontal_offset No idea.
	 *
	 * @param[in] vertical_offset No idea.
	 *
	 * @todo Find someone familiar with printing and force them to
	 * document how it works.
	 */
	void			PrintHeaderFooter(VisualDevice *vd, int page_num, int left_offset, int top_offset, int right_offset, int bottom_offset);

	/**
	 * Part of the printing support. Possibly outputs a page header
	 * and a page footer.
	 *
	 * @param[in] vd The print VisualDevice.
	 *
	 * @param[in] page The page description.
	 *
	 * @param[in] horizontal_offset No idea.
	 *
	 * @param[in] vertical_offset No idea.
	 *
	 * @todo Find someone familiar with printing and force them to
	 * document how it works.
	 */
	void			PrintPageHeaderFooter(VisualDevice *vd, PageDescription *page, short horizontal_offset, long vertical_offset);
# endif // _PRINT_SUPPORT_
#endif // PAGED_MEDIA_SUPPORT

	/**
	 * Tells the Window about the current ImageLoadingInfo.
	 */
	void 			SendImageLoadingInfo();

	/**
	 * Total number of frames added to a page.
	 * It's meaningful for top documents only.
	 */
	int num_frames_added;
	/**
	 * Total number of frames removed from a page.
	 * It's meaningful for top documents only.
	 */
	int num_frames_removed;
	/* Current number of frames on a page = num_frames_added - num_frames_removed */

	/** Set to TRUE if all known styles were downloaded. Indicates that styled paint may be performed */
	BOOL do_styled_paint;

#ifndef MOUSELESS
	enum FrameSeperatorType
	{
		NO_SEPARATOR,
		VERTICAL_SEPARATOR,
		HORIZONTAL_SEPARATOR
	};

	/**
	 * Checks if there is a frame separator, the thin border that
	 * sometimes is shown between frames, under the mouse. This is
	 * really a wrapper to the real code in dochand.
	 *
	 * @param x The x coordinate in scaled document coordinates but
	 * relative the view, not the document.
	 *
	 * @param y The x coordinate in scaled document coordinates but
	 * relative the view, not the document.
	 */
	FrameSeperatorType	IsFrameSeparator(int x, int y);

	/**
	 * Starts moving the frame separator, the thin border that
	 * sometimes is shown between frames. This is really a wrapper to
	 * the real code in dochand.
	 *
	 * @param[in] x The x coordinate in scaled document coordinates
	 * but relative the view, not the document.
	 *
	 * @param[in] y The x coordinate in scaled document coordinates
	 * but relative the view, not the document.
	 */
	void	StartMoveFrameSeparator(int x, int y);

	/**
	 * Moves the frame separator, the thin border that sometimes is
	 * shown between frames, to the specified position, as a result of
	 * a mouse move. This is really a wrapper to the real code in
	 * dochand.
	 *
	 * @param[in] x The x coordinate in scaled document coordinates
	 * but relative the view, not the document.
	 *
	 * @param[in] y The x coordinate in scaled document coordinates
	 * but relative the view, not the document.
	 */
	void	MoveFrameSeparator(int x, int y);

	/**
	 * Ends the moving of a frame separator, the thin border that
	 * sometimes is shown between frames. This is really a wrapper to
	 * the real code in dochand.
	 *
	 * @param[in] x The x coordinate in scaled document coordinates
	 * but relative the view, not the document.
	 *
	 * @param[in] y The x coordinate in scaled document coordinates
	 * but relative the view, not the document.
	 */
	void	EndMoveFrameSeparator(int x, int y);
#endif // !MOUSELESS

	/**
	 * Returns the event that should be target of an activation
	 * action.
	 */
	HTML_Element*           FindTargetElement();

	/**
	 * Used to be in Document.
	 *
	 * @param[in] state If the document should be made current (TRUE)
	 * or moved to history/oblivion (FALSE).
	 *
	 * @see FramesDocument::SetAsCurrentDoc
	 */
	void   BaseSetAsCurrentDoc(BOOL state);

	/**
	 * To be called when the logical document has finished parsing.
	 * The first time this is called appropriate DOM events will
	 * be sent.
	 */
	OP_STATUS HandleLogdocParsingComplete();

	/**
	 * Helper method that tries to access the Image URL an action is
	 * likely to refer too. We have a gap in our API/communication here.
	 *
	 * This is not guaranteed to return what the user expected.
	 *
	 * @returns an empty url or the url to the image.
	 */
	URL GetURLOfLastAccessedImage();

	void SetCurrentDocState(BOOL state) { is_current_doc = state; };

	/**
	 * Helper function that configures OpDocumentContext for a specific
	 * element along with as much context as possible.
	 */
	OP_STATUS PopulateInteractionContext(DocumentInteractionContext& ctx,
	                                     HTML_Element* target,
	                                     int offset_x, int offset_y,
	                                     int document_x, int document_y);

	/**
	 * Creates a document context for a specific element.
	 *
	 * @param[in] target The element we should create an OpDocumentContext for.
	 *
	 * @param[in] document_pos The point we should cretae an OpDocumentContext.
	 * For this to make sense that point should be somewhere within the box
	 * of |target|.
	 *
	 * @param[in] screen_pos The screen position of the element. Must be consistent
	 * with document_pos and target or anything could happen.
	 *
	 * @see CreateDocumentContext()
	 */
	OpDocumentContext* CreateDocumentContextForElement(HTML_Element* target,
	                                                   const OpPoint& document_pos,
	                                                   const OpPoint& screen_pos);

	/** Fills a vector of menu items which the document (via HTML5 contextmenu) or
	 *  extensions(via ContextMenu API) requested to add to the context menu.
	 *
	 *  @param menu_items list of menu items to which the menu items will be added.
	 *  @param document_context context in which the menu has been requested.
	 *  @return
	 *      OpStatus::OK
	 *      OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS GetDocumentMenu(OpWindowcommanderMessages_MessageSet::PopupMenuRequest::MenuItemList& menu_items, DocumentInteractionContext* document_context);

	/** Sends OnPopupMenu event to OpMenuListener for specified document context. */
	void     RequestContextMenu(DocumentInteractionContext& context);

#ifdef _PLUGIN_SUPPORT_
	/**
	 * Resumes all plugins that are either fully loaded, or haven't even
	 * become a plugin. A good thing to run after a Reflow().
	 */
	void ResumeAllFinishedPlugins();
#endif // _PLUGIN_SUPPORT_

	/**
	 * Helper function for DeleteIFrame to unhook the CoreView tree before
	 * deleting frames when not doing it the "normal" synchronous route.
	 */
	static void UnhookCoreViewTree(FramesDocElm* frame);

	/**
	 * Helper method to IsLoaded and NeedsProgressBar. Checks the local
	 * resource status. Not checking the parser and not checking any
	 * sub documents so that that part can be done externally.
	 *
	 * @param[in] include_inlines Should be TRUE. If FALSE it will
	 * disregard inlines and give the wrong response in almost all cases.
	 *
	 * @return TRUE if there are no local resources (images, scripts
	 *         css, media, ...) still loading.
	 *
	 * @see IsLoaded()
	 * @see NeedsProgressBar()
	 */
	BOOL HasLoadedLocalResources(BOOL include_inlines);

	/**
	 * When a document has finished loading, lots of things should be
	 * done. Those things that should be done when the local document
	 * is parsed and images loaded and document reflowed is in this
	 * function. Note that this, contrary to FinishDocTree(), does
	 * not wait for sub-documents ([i]frames) to complete.
	 *
	 * <p>This should not be called directly. Instead call
	 * CheckFinishDocument if you can suspect that a state
	 * that controls if this should be called has changed.
	 *
	 * @returns OpStatus::OK if flags were set successfully, an error
	 * code otherwise.
	 */
	OP_STATUS		FinishLocalDoc();

	/**
	 * When a document has finished loading, lots of things should be
	 * done. This method is to run when the local document along with
	 * all child documents (iframes/frames) is loaded. This is run
	 * some time after FinishLocalDoc() (sibling function that doesn't
	 * wait for child documents).
	 *
	 * <p>This should not be called directly. Instead call
	 * CheckFinishDocument if you can suspect that a state
	 * that controls if this should be called has changed.
	 *
	 * @returns OpStatus::OK if flags were set successfully, an error
	 * code otherwise.
	 */
	OP_STATUS		FinishDocTree();

#ifdef APPLICATION_CACHE_SUPPORT
	/**
	 * Triggers the application cache installation/update algorithm if a manifest attribute
	 * exists in the html tag.
	 *
	 * @return 	OpStatus::OK,
	 * 			OpStatus::ERR_NO_MEMORY or
	 * 			OpStatus::ERR for generic error
	 */
	OP_STATUS UpdateAppCacheManifest();
#endif // APPLICATION_CACHE_SUPPORT


	/**
	 * Check if the stored viewport that should be restored fits in
	 * the current document.
	 */
	BOOL StoredViewportCanBeRestored();

	// **************************************************************
	// This is the end of the internal part of the class. Everything
	// below is public and part of the interface that people can
	// use. Some methods below should be made private

public:

	/**
	 * Inline priorities enumeration. The higher number means the higher priority.
	 * Inlines with higher priorities are requested to be loaded sooner than those with lower priority.
	 */
	enum
	{
		LOAD_PRIORITY_FRAME_DOCUMENT_INLINE_PLUGIN_EXTERNAL = URL_LOWEST_PRIORITY + 1, // priority 1 - flash, audio, video in a frame from different hosts than a main doc
		LOAD_PRIORITY_FRAME_DOCUMENT_INLINE_PLUGIN_INTERNAL = URL_LOWEST_PRIORITY + 2, // priority 2 - flash, audio, video in a frame from the same host as a main doc
		LOAD_PRIORITY_FRAME_DOCUMENT_INLINE_IMAGE_EXTERNAL  = URL_LOWEST_PRIORITY + 3, // priority 3 - Image, map, icon in a frame from different hosts than a main doc
		LOAD_PRIORITY_FRAME_DOCUMENT_INLINE_IMAGE_INTERNAL  = URL_LOWEST_PRIORITY + 4, // priority 4 - Image, map, icon in a frame from the same host as a main doc
		LOAD_PRIORITY_FRAME_DOCUMENT_INLINE_REQUIRED_BY_CSS = URL_LOWEST_PRIORITY + 6, // priority 6 - Border image, background image, cursor
		LOAD_PRIORITY_FRAME_DOCUMENT                        = URL_LOWEST_PRIORITY + 8, // priority 8
		LOAD_PRIORITY_FRAME_DOCUMENT_USER_INITIATED         = URL_LOWEST_PRIORITY + 9, // priority 9
		LOAD_PRIORITY_FRAME_DOCUMENT_SYNC_XHR               = URL_LOWEST_PRIORITY + 10, // priority 10

		LOAD_PRIORITY_MAIN_DOCUMENT_INLINE_PLUGIN_EXTERNAL  = URL_NORMAL_PRIORITY + 1, // priority 16 - flash, audio, video in a main doc from different hosts than the main doc
		LOAD_PRIORITY_MAIN_DOCUMENT_INLINE_PLUGIN_INTERNAL  = URL_NORMAL_PRIORITY + 2, // priority 17 - flash, audio, video in a main doc from the same host as the main doc
		LOAD_PRIORITY_MAIN_DOCUMENT_INLINE_IMAGE_EXTERNAL   = URL_NORMAL_PRIORITY + 3, // priority 18 - Image, map, icon in a main doc from different hosts than the main doc
		LOAD_PRIORITY_MAIN_DOCUMENT_INLINE_IMAGE_INTERNAL   = URL_NORMAL_PRIORITY + 4, // priority 19 - Image, map, icon in a main doc from the same host as the main doc
		LOAD_PRIORITY_MAIN_DOCUMENT_INLINE_REQUIRED_BY_CSS  = URL_NORMAL_PRIORITY + 5, // priority 20 - Border image, background image, cursor
		LOAD_PRIORITY_FRAME_DOCUMENT_CSS_INLINE             = URL_NORMAL_PRIORITY + 6, // priority 21
		LOAD_PRIORITY_MAIN_DOCUMENT_CSS_INLINE              = URL_NORMAL_PRIORITY + 7, // priority 22
		LOAD_PRIORITY_FRAME_DOCUMENT_SCRIPT_INLINE_NO_DSE   = URL_NORMAL_PRIORITY + 8, // priority 23
		LOAD_PRIORITY_FRAME_DOCUMENT_SCRIPT_INLINE_DSE      = URL_NORMAL_PRIORITY + 9, // priority 24
		LOAD_PRIORITY_MAIN_DOCUMENT_SCRIPT_INLINE_NO_DSE    = URL_NORMAL_PRIORITY + 10, // priority 25
		LOAD_PRIORITY_MAIN_DOCUMENT_SCRIPT_INLINE_DSE       = URL_NORMAL_PRIORITY + 11, // priority 26
		LOAD_PRIORITY_MAIN_DOCUMENT_SYNC_XHR                = URL_NORMAL_PRIORITY + 12, // priority 27
		LOAD_PRIORITY_MAIN_DOCUMENT                         = URL_NORMAL_PRIORITY + 13, // priority 28
		LOAD_PRIORITY_MAIN_DOCUMENT_USER_INITIATED          = URL_NORMAL_PRIORITY + 14, // priority 29
	};

	/**
	 * Constructor. Stores the DocumentManager, the url, the start
	 * width and height and the sub_win_id for the document.
	 *
	 * @param doc_manager The owning doc_manager, must not be NULL.
	 *
	 * @param c_url The start URL for this document. Can probably be empty.
	 *
	 * @param origin The security context for this document. Must not be NULL. If the constructor
	 * returns non-null, FramesDocument will do IncRef() on it and take ownership of that reference.
	 * Must not be the same DocumentOrigin object as used by existing documents since it might mutate.
	 *
	 * @param[in] sub_w_id The sub window id of the document, -1 for
	 * the root document, 1 or larger for sub documents.
	 *
	 * @param use_plugin Set to TRUE if this should be a plugin
	 * document.
	 *
	 * @param inline_feed The document is a webfeed that should be opened
	 * with a special "viewer" page.
	 */
					FramesDocument(DocumentManager* doc_manager,
						const URL& c_url, DocumentOrigin* origin, int sub_w_id,
						BOOL use_plugin = FALSE, BOOL inline_feed = FALSE);
	/**
	 * Destructor.
	 */
					~FramesDocument();

	/**
	 * The currently focused form element in the document or NULL.
	 *
	 * @see FramesDocument::unfocused_formelement
	 */
	FormObject* current_focused_formobject;

	/**
	 * If the view loses focus, we must unfocus the current
	 * formobject, but focus it again on setfocus. For that reason a
	 * pointer to it is stored here.
	 *
	 * @see Document::current_focused_formobject
	 */
	HTML_Element* unfocused_formelement;
	/**
	 * The form element that is the "default" element right now. To be
	 * activated if the user presses ENTER or otherwise does something
	 * indicating that he wants something to happen.
	 */
	HTML_Element* current_default_formelement;

	/**
	 * Checks that the document is "current", i.e. not in history.
	 *
	 * @returns TRUE if this is a document that is shown right now,
	 * and not in history.
	 */
	BOOL				IsCurrentDoc() { return is_current_doc; };

	/**
	 * In a frame tree, every sub frame has a different id that is
	 * used to identify frames. That id is also stored in
	 * FramesDocuments and HTML_Documents and can be retrieved with
	 * this method.
	 *
	 * @returns The id. The id is -1 for the root document, 0 or
	 * larger for sub frames.
	 */
	int					GetSubWinId() { return sub_win_id; }

	/**
	 * Many FramesDocuments (those for HTML/XML/SVG) has a
	 * LogicalDocument. This method can be used retrieve that object.
	 *
	 * @returns A pointer to the FramesDocument's LogicalDocument or
	 * NULL.
	 */
	LogicalDocument*	GetLogicalDocument() { return logdoc; };

	/**
	 * Get the LayoutWorkplace established by this document, if any.
	 */
	LayoutWorkplace*	GetLayoutWorkplace() const { return logdoc ? logdoc->GetLayoutWorkplace() : NULL; }

	/**
	 * Deletes all content in the document, including child documents
	 * and the HTML_Document.
	 *
	 * @param[in] remove_callbacks If TRUE also deregisters loading
	 * callbacks. Not sure if this ever is FALSE. Couldn't find any
	 * such caller in a quick search.
	 */
	void			Clean(BOOL remove_callbacks = TRUE);

#ifdef MEDIA_SUPPORT
	/**
	 * Adds a media element to the collection of media elements in
	 * the document.
	 *
	 * @param[in] media The media element to add to the document's collection
	 * of media elements.
	 *
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS AddMedia(Media* media);

	/**
	 * Removes a media element from the collection of media elements in
	 * the document.
	 *
	 * @param[in] media The media element to remove from the document's collection
	 * of media elements.
	 */
	void RemoveMedia(Media* media);

	/**
	 * Calls MediaElement::Suspend() on all media elements that have
	 * previously been registered with AddMedia.
	 *
	 * @param[in] remove If TRUE, all media elements are removed from
	 * the document's collection of media elements.
	 */
	void SuspendAllMedia(BOOL remove);

	/**
	 * Calls MediaElement::Resume() on all media elements that have
	 * previously been registered with AddMedia.
	 *
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS ResumeAllMedia();
#endif // MEDIA_SUPPORT

	/**
	 * doc internal method. Used from DocumentInteractionContext and nowhere else.
	 */
	void UnregisterDocumentInteractionCtx(DocumentInteractionContext* ctx);

	/**
	 * doc internal method. Used from DocumentInteractionContext and nowhere else.
	 */
	OP_STATUS RegisterDocumentInteractionCtx(DocumentInteractionContext* ctx);

	/**
	 * Trigger a reflow (and later a repaint).
	 *
	 * @param[in] no_delay If FALSE (default), we will reflow in 200
	 * milliseconds. Otherwise we will reflow as soon as we
	 * can. Reflowing is expensive and might delay page load and
	 * scripts so it shouldn't be done too often.
	 */
	void			PostReflowMsg(BOOL no_delay = FALSE);

	/**
	 * Return the delay that should be used when posting a delayed reflow msg.
	 * The delay depend on wether the document is still parsing and if the script
	 * is currently resting. It may return 0.
	 */
	int				GetReflowMsgDelay();

#ifdef JS_LAYOUT_CONTROL
	/**
	 * Seems to be part of a feature to let web pages control if we
	 * reflow or not. Seems very dangerous to me, and we have to make
	 * sure that no random webpages can control this.
	 *
	 * <p> Prevents reflows and repaints of the document.
	 */
	void            PauseLayout();

	/**
	 * Seems to be part of a feature to let web pages control if we
	 * reflow or not. Seems very dangerous to me, and we have to make
	 * sure that no random webpages can control this.
	 *
	 * <p> Resumes reflowing and painting of the screen.
	 */
	void            ContinueLayout();

	/**
	 * Seems to be part of a feature to let web pages control if we
	 * reflow or not. Seems very dangerous to me, and we have to make
	 * sure that no random webpages can control this.
	 *
	 * @returns TRUE if layout is paused, FALSE normally.
	 */
	BOOL            IsPausedLayout() { return pause_layout; }
#endif // JS_LAYOUT_CONTROL

	/**
	 * Trigger a rebuild of the frameset as a consequence of some
	 * change to the frameset elements. Will trigger a Reflow with
	 * reflow_frameset set to TRUE as soon as the message is handled.
	 */
	void			PostReflowFramesetMsg();

	/**
	 * We keep two flags for whether we have a delayed reflow
	 * outstanding, one for the document itself and one for its
	 * frameset. Those flags are cleared with this function.
	 *
	 * <p>Call this function when the posted message arrives.
	 *
	 * @param[in] frameset if FALSE, the "document reflow message
	 * outstanding" flag is set to FALSE, if TRUE the "frameset reflow
	 * message outstanding" is set to FALSE.
	 */
	void			ClearReflowMsgFlag(BOOL frameset)
	{
		if (frameset)
			is_reflow_frameset_msg_posted = FALSE;
		else
			is_reflow_msg_posted = FALSE;
	}

#ifdef WEBFEEDS_DISPLAY_SUPPORT
	// From OpFeedListener, see base class documentation
	virtual void OnUpdateFinished() {}
	virtual void OnFeedLoaded(OpFeed* feed, OpFeed::FeedStatus status);
	virtual void OnEntryLoaded(OpFeedEntry* entry, OpFeedEntry::EntryStatus) {}
	virtual void OnNewEntryLoaded(OpFeedEntry* entry, OpFeedEntry::EntryStatus) {}
#endif // WEBFEEDS_DISPLAY_SUPPORT

#ifdef WAND_SUPPORT
	/**
	 * We cache if we have matched wand here to avoid having to check
	 * it over and over again.
	 *
	 * @param[in] has_wand_matches TRUE if wand found a match in the
	 * current document.
	 */
	void			SetHasWandMatches(BOOL has_wand_matches) { this->has_wand_matches = has_wand_matches; }

	/**
	 * We cache if we have matched wand here to avoid having to check
	 * it over and over again.
	 *
	 * @returns TRUE if wand found a match in the current document.
	 */
	BOOL			GetHasWandMatches() { return has_wand_matches; }

	/**
	 * We have some trouble with the event handling when using wand
	 * since the submit becomes asynchronous so we keep a flag that
	 * wand is handling a submit here to avoid double submits among
	 * other problems.
	 *
	 * @param[in] status TRUE if wand seems to be doing a submit.
	 */
	void			SetWandSubmitting(BOOL status) { is_wand_submitting = status; }

	/**
	 * We have some trouble with the event handling when using wand
	 * since the submit becomes asynchronous so we keep a flag that
	 * wand is handling a submit here to avoid double submits among
	 * other problems.
	 *
	 * @returns TRUE if wand seems to be doing a submit.
	 */
	BOOL			GetWandSubmitting() { return is_wand_submitting; }

	/**
	 * Add the HTML_Element to the list of all wand fields that
	 * should be filled later and add a listener to the provided
	 * thread that will insert the wand data when signalled, provided
	 * such a listener doesn't already exist.
	 */
	void			AddPendingWandElement(HTML_Element* elm, ES_Thread* thread);

	/**
	 * Insert the Wand data into the waiting objects.
	 */
	void	 		InsertPendingWandData();
#endif // WAND_SUPPORT
# ifdef OPERA_CONSOLE
	/**
	 * Sends an error to the console if there is one. The message will
	 * be attributed the current document. Only used by WebForms2 code
	 * right now but could move out of the ifdef if it turns out to be
	 * useful.
	 *
	 * @param message_part_1 The message.
	 *
	 * @param message_part_2 Optional extra message. Can be null or
	 * will otherwise be appended to message_part_1
	 *
	 * @returns OpStatus::OK if the error was successfully logged (or
	 * ignored on purpose), OpStatus::ERR_NO_MEMORY otherwise.
	 */
	OP_STATUS EmitError(const uni_char* message_part_1, const uni_char* message_part_2);
# endif // OPERA_CONSOLE

	/**
	 * This is part of the support for the autofocus attribute. We
	 * will only obey the autofocus if the user hasn't moved the focus
	 * manually.
	 *
	 * @param[in] new_val TRUE if the next autofocus attribute should
	 * be honored.
	 */
	void			SetObeyNextAutofocus(BOOL new_val) { obey_next_autofocus_attr = new_val; }

	/**
	 * This is part of the support for the autofocus attribute. We
	 * will only obey the autofocus if the user hasn't moved the focus
	 * manually.
	 *
	 * @returns TRUE if the next autofocus attribute should be
	 * honored.
	 */
	BOOL			GetObeyNextAutofocus() { return obey_next_autofocus_attr; }

	/**
	 * This is part of the support for the autofocus attribute.
	 *
	 * @param[in] elm The element that should be autofocused as soon
	 * as we find it suitable to move focus and scroll.
	 */
	void			SetElementToAutofocus(HTML_Element* elm) { element_to_autofocus.SetElm(elm); }

	/**
	 * This is part of the support for the autofocus attribute.
	 *
	 * @returns The element that should be autofocused as soon as we
	 * find it suitable to move focus and scroll.
	 */
	HTML_Element*	GetElementToAutofocus() { return element_to_autofocus.GetElm(); }

	/**
	 * Moves the focus as the autofocus flags says. Calling this
	 * method again after a autofocus has been processed is ok.
	 */
	void			ProcessAutofocus();

	/**
	 * In WebForms2 "invalid" events that are not caught should
	 * sometimes trigger a message to the user (an alert), but there
	 * should only be one such error even if there are several
	 * "invalid" events so we keep a flag here to indicate that the
	 * next event that is not caught should trigger a message.
	 *
	 * <p>This is subject to race conditions and we might need a flag
	 * in the event itself to say if it should cause an alert or not.
	 *
	 * @param[in] new_val TRUE if the next event should trigger a
	 * message. FALSE otherwise.
	 */
	void			SetNextOnInvalidCauseAlert(BOOL new_val) { display_err_for_next_oninvalid = new_val; }

	/**
	 * In WebForms2 "invalid" events that are not caught should
	 * sometimes trigger a message to the user (an alert), but there
	 * should only be one such error even if there are several
	 * "invalid" events so we keep a flag here to indicate that the
	 * next event that is not caught should trigger a message.
	 *
	 * <p>This is subject to race conditions and we might need a flag
	 * in the event itself to say if it should cause an alert or not.
	 *
	 * @returns TRUE if the next event should trigger a message. FALSE
	 * otherwise.
	 */
	BOOL			GetNextOnInvalidCauseAlert() {return display_err_for_next_oninvalid; }

	/**
	 * Sets an element which is supposed to be refocused when it's suitable (after reflowing).
	 * This is used when changing an input's type.
	 *
	 * @param[in] elm The element that should be refocused as soon
	 * as we find it suitable to move focus to.
	 */
	void			SetElementToRefocus(HTML_Element* elm)
	{
		OP_ASSERT(elm->Type() == HE_INPUT);
		element_to_refocus.SetElm(elm);
	}

	/**
	 * Refocuses an element if the element to be refocused is set.
	 */
	void			RefocusElement();

	/**
	 * Return the contents that the currently focused input element (type=text)
	 * had upon gaining focus. Used when determining if onchange handling is
	 * required upon blurring.
	 *
	 * Returns a reference that can be used to update and clear the string
	 * value kept on the document.
	 */
	OpString &GetFocusInputText() { return focus_input_text; }

#ifdef DOCUMENT_EDIT_SUPPORT
	enum EditableMode
	{
		DESIGNMODE_OFF, // Turn off editable mode on the document
		CONTENTEDITABLE_OFF, // Turn off ditable mode on the element
		DESIGNMODE, // Design mode means the entire document is editable
		CONTENTEDITABLE // ContentEditable means only a part of the document is editable
	};
	/**
	 * Turn off or on the editable state of the document.
	 *
	 * @param[in] mode. Sets the editable mode and creates or deletes the documnent edit
	 *					structure depending on mode. You can not change from DESIGNMODE to
	 *					CONTENTEDITABLE and vice versa using this call (without first
	 *					disabling current editable mode).
	 *
	 * @returns OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS		SetEditable(EditableMode mode);

	/**
	 * A document that is partially editable or fully editable has an
	 * OpDocumentEdit object that handles all the editing. This object
	 * is owned by the FramesDocument and will be returned here.
	 *
	 * @returns A pointer to the OpDocumentEdit or NULL if the
	 * document isn't (yet) editable.
	 */
	OpDocumentEdit*	GetDocumentEdit() { return document_edit; }

	/**
	 * Checks if the document is in design mode, i.e. fully editable
	 * by OpDocumentEdit. This will return FALSE if it's only
	 * partially editable.
	 *
	 * @returns TRUE if we are currently in design mode, FALSE if we
	 *			are in content editable mode or if document edit is
	 *			disabled
	 *
	 * @todo Ask emil to add more useful information.
	 */
	BOOL			GetDesignMode();
#endif // DOCUMENT_EDIT_SUPPORT

#if defined DOCUMENT_EDIT_SUPPORT || defined KEYBOARD_SELECTION_SUPPORT
	/**
	 * Returns a pointer to the caret painter, which is an object that
	 * tracks the caret position and makes sure it blinks as it should.
	 *
	 * This is never NULL if keyboard selection mode is enabled or if
	 * there is an OpDocumentEdit object.
	 *
	 * @returns a pointer to the document's CaretPainter or NULL.
	 */
	CaretPainter* GetCaretPainter() { return caret_painter; }

	/**
	 * Returns a pointer to the caret manager, which is an object that
	 * has a lot of operation on manipulating caret positions.
	 *
	 * This is never NULL, though general caret movement may still be
	 * disabled. NULL checking GetCaretPainter is a way to easily
	 * check that.
	 *
	 * @returns a pointer to the document's CaretManager.
	 */
	CaretManager* GetCaret() { return &caret_manager; }
#endif // DOCUMENT_EDIT_WIDGET_SUPPORT || KEYBOARD_SELECTION_SUPPORT

	/**
	 * @deprecated Remove when content.cpp has stopped calling it.
	 */
	void			DEPRECATED(NotifyIntersectionViews(HTML_Element* parent, int dx, long dy)); // inlined below

	/**
	 * Sets up an iframe for asynchronous deletion by DeleteAllIFrames().
	 *
	 * Used when there might still be references to the iframe around.
	 *
	 * After calling this function iframe->IsDeleted() will return true,
	 * indicating that the iframe is queued to be deleted.
	 *
	 * @param iframe  The iframe to delete.
	 */
	void			DeleteIFrame(FramesDocElm *iframe);

	/**
	 * Remove objects related to loading inline data on the |helm|
	 * element. Often inline data results in some data being cached on
	 * the element and if that element is for instance removed from
	 * the tree and cleaned, we don't need and want that data, and we
	 * for sure don't want to have pointers to that data from the
	 * document lists. So this method should be called when we "Clean"
	 * the HTML_Element.
	 *
	 * Don't add code to this method if the inline load has to survive
	 * its element being moved around in the document tree.
	 *
	 * @param[in] helm The element that is being cleaned.
	 */
	void			FlushAsyncInlineElements(HTML_Element* helm);

	/**
	 * Every window and DocumentManager has an id to be used to locate
	 * and address them in a frameset tree. This method is used to
	 * find the DocumentManager with a specific id.
	 *
	 * @param id The id to look for.
	 *
	 * @return The DocumentManager of NULL if no such id was found.
	 */
	DocumentManager*
			        GetDocManagerById(int id);

	/**
	 * Returns the DocumentManager in the currently active frame.
	 *
	 * @returns DocumentManager* A pointer to a DocumentManager or NULL.
	 */
	DocumentManager*
					GetActiveDocManager();

	/**
	 * Extracts the FramesDocElm with a specified id, searching among
	 * all the subdocuments of the FramesDocument.
	 *
	 * @param[in] id The id of the sub document. See the dochand
	 * module for more information of sub document ids.
	 *
	 * @returns The FramesDocElm or NULL if the id didn't match any
	 * sub documents.
	 */
	FramesDocElm*	GetFrmDocElm(int id);

	/**
	 * Document with frames have the sub document in a tree of
	 * FramesDocElms owned by the parent FramesDocument. This returns
	 * the Head of that tree or NULL if there are no frames.
	 *
	 * @returns A pointer to the Head in the FramesDocElm tree or
	 * NULL.
	 */
	FramesDocElm*	GetFrmDocRoot() { return frm_root; }

#ifdef DOC_HAS_PAGE_INFO
	/**
	 * Generates an informative document that can be loaded from the
	 * url, out_url.
	 *
	 * @param[out] out_url The generated document will be in this url.
	 *
	 * @returns OpBoolean::IS_FALSE if we failed to generate a
	 * document, OpBoolean::IS_TRUE if we generated a document
	 * successfully.
	 *
	 * @see OpPageInfo
	 */
	OP_BOOLEAN		GetPageInfo(URL& out_url);
# ifndef USE_ABOUT_FRAMEWORK
	/**
	 * @deprecated Use GetPageInfo or the code in the about module
	 * directly.
	 */
	OP_BOOLEAN      DEPRECATED(GetPageInfoL(URL& out_url));
# endif
#endif // DOC_HAS_PAGE_INFO

	/**
	 * Clears the document to prepare for a reloaded copy (though it
	 * may be the same data we parse once again) to be loaded.
	 *
	 * @param[in] url The url that we will reload, which often is the
	 * same as the current url but may be another url in case of
	 * redirects and generated documents.
	 *
	 * @param[in] origin The security context for the "updated" document.
	 * Must not be null.
	 *
	 * @param parsing_restarted Set to TRUE if parsing of the new
	 * document has already started, FALSE otherwise. If parsing has
	 * already restarted we will not disconnect all parsing listeners
	 * for instance.
	 *
	 * @returns OpStatus::OK if we succeed, OpStatus::ERR_NO_MEMORY if
	 * we encounter an OOM.
	 */
	OP_STATUS       ReloadedUrl(const URL& url,
	                            DocumentOrigin* origin,
	                            BOOL parsing_restarted);

	/**
	 * Certain operations can't be performed while reflowing, for
	 * instance MarkDirty, so we need to know when we are
	 * reflowing. That is done with this flag in FramesDocument.
	 *
	 * @returns TRUE when the reflow is running, FALSE when it is not.
	 *
	 * @todo Maybe move to LayoutWorkplace?
	 */
	BOOL			IsReflowing() { return reflowing; }

	/**
	 * Certain operations can't be performed while reflowing, for
	 * instance MarkDirty, so we need to know when we are
	 * reflowing. That is done with this flag in FramesDocument.
	 *
	 * @param[in] value TRUE when the reflow starts, FALSE when it
	 * ends.
	 *
	 * @todo Maybe move to LayoutWorkplace?
	 */
	void			SetReflowing(BOOL value) { reflowing = value; }

	/**
	 * This sets a flag to inform anyone interested that the
	 * document is currently being unloaded. That means that
	 * certain state can be temporarilly unreliable and that
	 * it is possible to take shortcuts in cleanup operations.
	 *
	 * It's the callers responsibility to make sure calls to this
	 * are balanced.
	 *
	 * @param[in] value TRUE when the Unload starts, FALSE when it
	 * ends.
	 */
	void			SetUnloading(BOOL value) { OP_ASSERT(is_being_unloaded != value); is_being_unloaded = value; }

	/**
	 * Checks if the FramesDocument is currently being deactivated or
	 * freed or deleted. Operations done during those operations may
	 * trigger hooks that should not mess with the FramesDocument
	 * while it's being in this unstable state. By checking with this
	 * method, dangerous or unneeded actions can be avoided.
	 *
	 * @returns TRUE if the document is currently being deactivated,
	 * or FALSE if it's stable. If it returns FALSE it may still have
	 * been Undisplayed earlier but that can be checked with
	 * IsCurrentDoc().
	 */
	BOOL			IsUndisplaying() { return is_being_deleted || is_being_freed || undisplaying || is_being_unloaded; }

	/**
	 * There are two types of documents, HTML_Document and
	 * FramesDocument. This can be used to check which this Document
	 * is. Normally this shouldn't be needed since the code shouldn't
	 * keep HTML_Documents and FramesDocument pointers at the same
	 * place.
	 *
	 * @returns DOC_FRAMES since this is FramesDocument.
	 *
	 * @deprecated No use for this method anymore.
	 */
	DocType	Type() { return DOC_FRAMES; };

	/**
	 * Returns if this is a FramesDocument that only contains a plugin
	 * and nothing more.
	 *
	 * @returns TRUE if this is a document generated to only contain a
	 * plugin, FALSE otherwise.
	 */
	BOOL		 	IsPluginDoc() { return is_plugin; }

	/**
	 * Return if this is a FramesDocument acting as a wrapper document
	 * for a plugin, window or media element.
	 *
	 * @returns TRUE if this is a wrapper document for a plugin, image
	 * or media element, FALSE otherwise.
	 */
	BOOL			IsWrapperDoc();

	/**
	 * Returns a new, free, number to be used by
	 * sub-windows. Typically this is used before creating a new
	 * FramesDocElm and then the number is giveb in that constructor.
	 *
	 * @param A number, starting with 0 and then increasing.
	 */
	int				GetNewSubWinId();

	/**
	 * Paints the document on the VisualDevice. For code outside the
	 * doc module, this method should only be called on FramesDocument
	 * objects.
	 *
	 * If we are currently painting a print preview document, this
	 * function will paint the print preview background, and paint
	 * the visible pages individually through
	 * PageDescription::PrintPage.
	 *
	 * @param[in] rect The part to repaint. This is in document
	 * coordinates relative the upper left corner of the view.
	 *
	 * @param[in] vd The VisualDevice to paint on. This might be
	 * another VisualDevice than the document's normal VisualDevice.
	 */
    OP_DOC_STATUS		Display(const RECT& rect, VisualDevice* vd);

	/**
	 * Inactivates a document when navigating in history or replacing
	 * this document with a new document.
	 *
	 * @param[in] will_be_destroyed Should be TRUE if the document is
	 * about to be destroyed, to keep Undisplay() from doing anything
	 * expensive and pointless, such as storing form values.
	 */
	OP_STATUS		Undisplay(BOOL will_be_destroyed = FALSE);

	/**
	 * Called from Undisplay in case there is no HTML_Document. Does
	 * DisableContent on the root element if there is one. See also
	 * HTML_Document::UndisplayDocument.
	 */
	void UndisplayDocument();

	/**
	 * Called when document gets focus. Will trigger
	 * the document's onfocus event listener.
	 *
	 * @param[in] reason The event that caused the document to get focus.
	 * If this is FOCUS_REASON_ACTIVATE then it
	 * forces us to check where the mouse is and recalculate what it
	 * is hovering.  That is an heavy operation with some
	 * side effects.
	 */
	void			GotKeyFocus(FOCUS_REASON reason);

	/**
	 * To be called when this document loses keyboard focus so that
	 * flags related to selection and similar can be reset/aborted.
	 */
	void			LostKeyFocus();

	/**
	 * Workaround for focus mess. If you have to move focus
	 * temporarily to the document (VisualDevice) but don't want to
	 * trigger any focus events, wrap the code in a
	 * SetSuppressFocusEvents(TRUE);
	 * SetSuppressFocusEvents(FALSE). This can not be nested.
	 *
	 * @param[in] value TRUE if focus events (in DOM) should be
	 * temporarily disabled. FASLE if normal mode should be resumed.
	 */
	void			SetSuppressFocusEvents(BOOL value) { suppress_focus_events = value; }

	/**
	 * This method restarts various things we stopped when going away
	 * from the document.  I.e. it is run when we go back/forward in history
	 * and will restart things like background sounds, refresh timers etc. in
	 * the document we're going to.
	 *
	 * This function in FramesDocument only iterates
	 * through all HTML_Document s in the document tree and calls
	 * Reactivate on them.
	 *
	 * This function used to be known as RestoreForms, but since it didn't
	 * do anything related to forms it was eventually (after a decade or so)
	 * renamed to something which is hopefully more descriptive.
	 *
	 * @returns OpStatus::OK if operation completed successfully and
	 * an error code otherwise. OpStatus::ERR_NO_MEMORY if OOM.
	 *
	 * @see HTML_Document::ReactivateDocument()
	 */
	OP_STATUS       ReactivateDocument();

#ifdef PAGED_MEDIA_SUPPORT
	/**
	 * Returns the number of pages needed to display/print this
	 * document in paged mode.
	 *
	 * @returns The number of pages or 0.
	 */
	int				CountPages();
# ifdef _PRINT_SUPPORT_
	/**
	 * Prints a document to the supplied PrintDevice.
	 *
	 * @param[in] pd The output device.
	 *
	 * @param[in] page_num The page to print.
	 *
	 * @param[in] selected_only Unused. To be implemented or removed
	 * from the interface.
	 *
	 * @todo Remove the selected_only parameter. I've made it have a
	 * default value to ease the transition.
	 */
	OP_DOC_STATUS   PrintPage(PrintDevice* pd, int page_num, BOOL selected_only = FALSE);
# endif // _PRINT_SUPPORT_

	/**
	 * Set a page number to restore after the next reflow, if at that
	 * time we're still in some sort of paged mode.
	 *
	 * @param[in] number Page number. Page 0 is the first page.
	 */
	void			SetRestoreCurrentPageNumber(int number) { restore_current_page_number = number; }
#endif // PAGED_MEDIA_SUPPORT

	/**
	 * For printing, we make a copy of the original document and print
	 * the static (hopefully) copy. This method checks if this is such
	 * a document.
	 *
	 * @returns TRUE if it's the print copy, FALSE if it's an ordinary
	 * document.
	 */
	BOOL			IsPrintDocument()
	{
#ifdef _PRINT_SUPPORT_
		return GetDocManager()->GetPrintDoc() == this;
#else
		return FALSE;
#endif // _PRINT_SUPPORT_
	}

	/* Display type */
	enum Displaying
	{
		REGULAR_DOCUMENT,
		PRINT_PREVIEW_DOCUMENT,
		PRINT_DOCUMENT
	};

	/**
	 * What type of document are we displaying
	 *
	 * @param[in] vd The visual device we are using
	 *
	 * @return The type of document we are displaying
	 */
	Displaying			IsDisplaying(VisualDevice* vd)
	{
#ifdef _PRINT_SUPPORT_
		if (vd->IsPrinter())
			return PRINT_DOCUMENT;

		if (GetDocManager()->GetPrintPreviewVD())
			return PRINT_PREVIEW_DOCUMENT;
#endif
		return REGULAR_DOCUMENT;
	}

	/**
	 * Rendering viewport position and/or size has changed.
	 *
	 * <p>If we are using smart_frames or stacked_frames, the whole
	 * frame is blewn up to cover the document which results in that
	 * DecVisible is not called on images that are gone out of view,
	 * this code propagates the visible rect to all sub frames, which
	 * makes it possible to throw out invisible images.
	 *
	 * @param[in] rendering_rect The visible area in document
	 * coordinates. It's propagated to iframes and frames.
	 */
	void			OnRenderingViewportChanged(const OpRect &rendering_rect);

	/**
	 * Marks all visible (according to HEListElm) image inlines in the given rectangle
	 * for paint. If they are still marked for paint after the Paint, we will mark
	 * them as invisible in MarkToBePaintedImagesAsNotVisible()
	 *
	 * This affects both background images and normal images.
	 *
	 * @param[in] rendering_rect The rendering rectangle, in document coordinates.
	 */
	void			MarkVisibleImagesInRectForPaint(OpRect rendering_rect);

	/**
	 * Undisplays all images with the IsToBePainted() flag set and sets all such
	 * flags to FALSE.
	 */
	void			MarkToBePaintedImagesAsNotVisible();

	/**
	 * Helper method used for scrolling to a rectangle.
	 * @param rect_to_scroll_to  Rectangle to scroll into view.
	 * @param current_scroll_xpos  Current horizontal scroll position.
	 * @param current_scroll_ypos  Current vertical scroll position.
	 * @param total_scrollable_width  Width of containing scrollable element.
	 * @param total_scrollable_height  Height of containing scrollable element.
	 * @param visible_width  Width of visible part of view (of containing element, not of visual viewport).
	 * @param visible_height  Height of visible part of view
	 * @param new_scroll_xpos   The new X position to scroll to
	 * @param new_scroll_ypos   The new Y position to scroll to
	 * @param align   How to align the rect inside the view if scrolling is needed
	 * @param is_rtl_doc   Whether the document has right to left attribute.
	 * @return TRUE if any scrolling is needed, FALSE if the rect is already in view
	 */
	static BOOL		GetNewScrollPos(OpRect rect_to_scroll_to, UINT32 current_scroll_xpos, UINT32 current_scroll_ypos,
									INT32 total_scrollable_width, INT32 total_scrollable_height,
									INT32 visible_width, INT32 visible_height,
									INT32& new_scroll_xpos, INT32& new_scroll_ypos,
									SCROLL_ALIGN align, BOOL is_rtl_doc);


	/**
	 * Scrolls to ensure rect is visible. Normally ScrollToElement is
	 * a better choice.
	 * Just calls the similar method in HTML_Document, if it's a document
	 * of that type, and otherwise doesn't do anything.
	 *
	 * @param[in] rect the document coordinates to make visible
	 * according to the align parameter.
	 *
	 * @param[in] align Where on screen to position the rect.
	 *
	 * @param[in] strict_align If TRUE, request visual viewport to
	 * change as long as the rectangle isn't positioned exactly where
	 * it should be in the visual viewport, as specified by
	 * 'align'. If FALSE, only request visual viewport to be changed
	 * if the specified rectangle is outside the current visual
	 * viewport (fully or partially).
	 *
	 * @param[in] reason Reason for scrolling.
	 *
	 * @param[in] scroll_to Used as base to figure out how to scroll
	 * scrollable containers. Not really sure what to put here, so
	 * just use ScrollToElement. May be NULL.
	 *
	 * @return TRUE if a new visual viewport position was requested
	 * (we needed to scroll), FALSE otherwise.
	 */
	BOOL			ScrollToRect(const OpRect& rect, SCROLL_ALIGN align, BOOL strict_align, OpViewportChangeReason reason, HTML_Element *scroll_to);

# ifdef SCROLL_TO_ACTIVE_ELM
	/**
	 * Scrolls to make the active element (focused element, active naviagation
	 * element) (if any) visible.
	 */
	void			ScrollToActiveElement();
# endif // SCROLL_TO_ACTIVE_ELM

	/**
	 * Called when the view size changes so that documents
	 * has to reflow and/or frames be resized.
	 */
	void			HandleNewLayoutViewSize();

	/**
	 * Handle potential document size change, which may affect scrollbars, etc.
	 */
	void			HandleDocumentSizeChange();

	/**
	 * Returns the width of the document. It might be the same as
	 * HTML_Document but it can also be taking paged media and frames
	 * into account.
	 *
	 * @returns The width of the document in document coordinates.
	 *
	 * @see Height();
	 */
	int				Width();

	/**
	 * For RTL (right-to-left) documents, it's possible that content
	 * overflows to the left of x=0, so that you can scroll to the
	 * left from the initial scroll position. If that's the case, this
	 * returns the amount.
	 *
	 * @see Document::Width()
	 */
	int				NegativeOverflow();

	/**
	 * Returns the height of the document. It might be the same as
	 * HTML_Document::Height but it can also be taking paged media and
	 * frames into account.
	 *
	 * @returns The height of the document in document coordinates.
	 *
	 * @see Width();
	 */
	long			Height();

	/**
	 * Returns the font size of the root element of this document in
	 * layout fixed point format. This is a shortcut to the appropriate
	 * method in DocRootProperties. May be zero, if not yet calculated.
	 */
	LayoutFixed		RootFontSize() const;

	/**
	 * Checks if this document was generated by Opera itself, for
	 * instance (or more specifically) as a wrapper for an image since
	 * we only display images inside documents or for a plain text
	 * file.
	 *
	 * @returns TRUE if this was a document generated by Opera, FALSE
	 * if we are not treating it as a document generated by Opera.
	 */
	BOOL			IsGeneratedDocument() { return is_generated_doc; }

	/**
	 * Mark this document as being generated by Opera. This affects
	 * things like security and layout mode policies. Less magic, less
	 * security...
	 */
	void			SetIsGeneratedDocument(BOOL generated) { is_generated_doc = generated; }

	/**
	 * Checks if this document uses a wrapper document, i.e. an HTML document
	 * around the real resource. This is used by images, video, plugins and other
	 * non-document resources. If a wrapper document is used, then we will
	 * ignore certain user settings (such as disabling of scripts). In that
	 * respect this is similar to ContainsOnlyOperaInternalScripts() expect
	 * that this is not guaranteed any privileges and the document might
	 * be accessed by any other document (needed by ACID3 test 14).
	 *
	 * @returns TRUE if this uses a wrapper document around the real resource (image, video, plugin, ...)
	 */
	BOOL			UsesWrapperDocument() { return wrapper_doc_source.HasContent(); }

	/**
	 * Checks if this document contains only scripts that are generated by
	 * Opera itself (internal Opera scripts). This will give the scripts run
	 * on this page extra security privileges and also prevent other pages
	 * from interacting with this page. In addition the scripts on this
	 * page will run even if the user has disabled scripts in general.
	 *
	 * @returns TRUE if this document contains only internal Opera scripts
	 */
	BOOL			ContainsOnlyOperaInternalScripts() { return contains_only_opera_internal_scripts; }

	/**
	 * Mark this document as only containing Opera internal scripts (scripts
	 * generated by Opera itself). Use with care, this will allow extended
	 * access to internal Opera APIs.
	 */
	void			SetContainsOnlyOperaInternalScripts(BOOL only_internal) { contains_only_opera_internal_scripts = only_internal; }

	/**
	 * Checks if the document considers itself, including all child
	 * documents and potentially all inlines, fully loaded.
	 *
	 * @param[in] inlines_loaded TRUE if the status of inlines
	 * (images, scripts, stylesheets, ...) should be considered, FALSE
	 * if they should be ignored.
	 *
	 * @returns TRUE if the document should be considered fully
	 * loaded, FALSE otherwise.
	 */
	BOOL	IsLoaded(BOOL inlines_loaded = TRUE);

	/**
	 * Checks if we have a fully parsed document.
	 *
	 * @returns TRUE if we have a document and it's fully
	 * parsed. FALSE otherwise.
	 *
	 * @see LogicalDocument::IsParsed()
	 *
	 * @see IsLoaded()
	 */
	BOOL			IsParsed();

	/**
	 * Checks if this document is in a state such that the window
	 * ought to display a progress bar.
	 *
	 * Returns TRUE if there are any currently loading inlines.  Also
	 * returns TRUE if the logical document hasn't finished parsing
	 * (and has been reflowed at least once since) unless this is a
	 * script generated document and the scripts that was generating
	 * it have finished (that is, we're waiting for document.close()
	 * and suspect that no-one's going to call it, ever.)
	 *
	 * Note that the state of this document's frames or iframes is not
	 * considered; the caller is responsible for checking the whole
	 * document tree.
	 *
	 * @returns TRUE if the document is loading and need a
	 * progress bar. FALSE otherwise.
	 */
	BOOL	NeedsProgressBar();

	/**
	 * If used=TRUE, mark all the inline elements in the document as
	 * "Used" so that they're not thrown out of any caches. If FALSE,
	 * mark them as not "Used" so that caches know that they are free
	 * to remove.
	 *
	 * @param used TRUE if inlines should be locked in caches, FALSE
	 * if they should not be locked in caches.
	 */
	void			SetInlinesUsed(BOOL used);

	/**
	 * When a document has finished loading (as in parsed completely and
	 * run a layout pass that has detected all the inlines), lots of things
	 * should be done. This checks if everything that matters has
	 * finished and then calls FinishDocument that does all those things.
	 *
	 * This might revert the loading state somewhat so what was looking
	 * complete before the call is no longer complete afterwards. That
	 * might be caused by this code starting a load of a favicon
	 * or running certain WML scripts.
	 *
	 * Everything that might be the last thing needed to complete
	 * a document load should call this method.
	 *
	 * @returns OpStatus::OK if flags were set successfully or
	 * OpStatus::ERR_NO_MEMORY if an oom situation appeared. In that case
	 * parts of what should have been done might still have been done.
	 */
	OP_STATUS		CheckFinishDocument();

	/**
	 * Generic callback method from the MessageObject interface. It
	 * will be called with the messages we have registered that we
	 * want to listen to.
	 *
	 * @param msg The message code.
	 *
	 * @param par1 The first generic parameter to the message.
	 *
	 * @param par2 The second generic parameter to the message.
	 *
	 * @see MessageObject.
	 */
	virtual void	HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

#ifndef MOUSELESS

	/**
	 * Used to check if a future mouse up can become a click if the mouse
	 * hasn't moved too far from a previous mouse down.  It is
	 * also used for touch to ensure the movement after a touch down
	 * is within the threshold to accept it as a simulated click.
	 *
	 * @param current_pos The current position.  It can be in any coordinate system
	 * provided it's the same as last_down_position.
	 *
	 * @param last_down_position The initial position when the mouse down or
	 * touch down was received. It can be in any coordinate system
	 * provided it's the same as current_pos.
	 *
	 * @returns BOOL TRUE if the movement is less than the threshold to be
	 * considered a "click", otherwise FALSE.
	 */
	static BOOL CheckMovedTooMuchForClick(const OpPoint& current_pos, const OpPoint& last_down_position);

	/**
	 * The generic "handle mouse events" code.
	 *
	 * @param[in] event The type of mouse event, MOUSEMOVE, MOUSEUP,
	 * MOUSEDOWN, MOUSEWHEEL and others I don't recall now.
	 *
	 * @param[in] x The x coordinate of the mouse action in document
	 * coordinates.
	 *
	 * @param[in] y The y coordinate of the mouse action in document
	 * coordinates.
	 *
	 * @param[in] visual_viewport_x The x offset of the visual viewport, in document coordinates.
	 * Note that this must be the visual vieport's x offset and not the rendering view port's one.
	 * @see GetVisualViewport()
	 * @see GetVisualViewportX()
	 *
	 * @param[in] visual_viewport_y The y offset of the visual viewport, in document coordinates.
	 * Note that this must be the visual vieport's y offset and not the rendering view port's one.
	 * @see GetVisualViewport()
	 * @see GetVisualViewportY()
	 *
	 * @param[in] sequence_count_and_button If it's a button press
	 * this is a mix of a sequence count and a button identifier. Use
	 * the macros EXTRACT_MOUSE_BUTTON and EXTRACT_SEQUENCE_COUNT to
	 * get the different parts. If it's a mouse wheel action, it says
	 * how far and in what direction the mouse wheel was
	 * spun. Negative numbers are scrolling up and positive numbers
	 * are scrolling down.
	 *
	 * @param[in] shift_pressed TRUE if the shift key was pressed when
	 * doing the mouse action.
	 *
	 * @param[in] control_pressed TRUE if control key was pressed when
	 * doing the mouse action.
	 *
	 * @param[in] alt_pressed TRUE if alt key was pressed when doing
	 * the mouse action.
	 *
	 * @param[in] outside_document Set this to TRUE if the action
	 * really appeared outside the document and the document is only
	 * told to reset such things as hover.
	 */
	DocAction		MouseAction(DOM_EventType event, int x, int y, int visual_viewport_x, int visual_viewport_y, int button_or_delta, BOOL shift_pressed, BOOL control_pressed, BOOL alt_pressed, BOOL outside_document = FALSE);

	/**
	 * Should be called when the mouse pointer leaves the document so
	 * that we know that we should stop expecting mouse messages.
	 */
	void			MouseOut();


	/**
	 * To be called when the document should handle a mouse down
	 * event. Typically called from the display module after it has
	 * decided which document to send the event to. This can be sent
	 * to either a FramesDocument or an HTML_Document, but for code
	 * outside the doc module it's preferably to send it to the
	 * FramesDocument first since it will forward it to the
	 * HTML_Document unless it's to be handled somewhere else.
	 *
	 * @param[in] mouse_x The x coordinate in document scale, relative
	 * to the upper left corner of the view. To get the full document
	 * coordinate, view_x has to be added.
	 *
	 * @param[in] mouse_y The y coordinate in document scale, relative
	 * to the upper left corner of the view. To get the full document
	 * coordinate, view_y has to be added.
	 *
	 * @param[in] shift_pressed TRUE if the shift key was pressed when
	 * doing the mouse down action.
	 *
	 * @param[in] control_pressed TRUE if control key was pressed when
	 * doing the mouse down action.
	 *
	 * @param[in] alt_pressed TRUE if alt key was pressed when doing
	 * the mouse down action.
	 *
	 * @param[in] button The button that was released. See the
	 * MouseButton enum.
	 *
	 * @param[in] sequence_count The sequence_count of the mouse
	 * up. Used to decide if we're in a doubleclick, trippleclick or
	 * even longer sequence. Will be visible in DOM events.
	 */
    void                MouseDown(int mouse_x, int mouse_y, BOOL shift_pressed, BOOL control_pressed, BOOL alt_pressed, MouseButton button, int sequence_count = 1);

	/**
	 * To be called when the document should handle a mouse up
	 * event. Typically called from the display module after it has
	 * decided which document to send the event to. This can be sent
	 * to either a FramesDocument or an HTML_Document, but for code
	 * outside the doc module it's preferably to send it to the
	 * FramesDocument first since it will forward it to the
	 * HTML_Document unless it's to be handled somewhere else.
	 *
	 * @param[in] mouse_x The x coordinate in document scale, relative
	 * to the upper left corner of the view. To get the full document
	 * coordinate, view_x has to be added.
	 *
	 * @param[in] mouse_y The y coordinate in document scale, relative
	 * to the upper left corner of the view. To get the full document
	 * coordinate, view_y has to be added.
	 *
	 * @param[in] shift_pressed TRUE if the shift key was pressed when
	 * doing the mouse up action.
	 *
	 * @param[in] control_pressed TRUE if control key was pressed when
	 * doing the mouse up action.
	 *
	 * @param[in] alt_pressed TRUE if alt key was pressed when doing
	 * the mouse up action.
	 *
	 * @param[in] button The button that was released. See the
	 * MouseButton enum.
	 *
	 * @param[in] sequence_count The sequence_count of the mouse
	 * up. Used to decide if we're in a doubleclick, trippleclick or
	 * even longer sequence. Will be visible in DOM events.
	 */
    void                MouseUp(int mouse_x, int mouse_y, BOOL shift_pressed, BOOL control_pressed, BOOL alt_pressed, MouseButton button, int sequence_count = 1);
	/**
	 * To be called when the document should handle a mouse move
	 * event. Typically called from the display module after it has
	 * decided which document to send the event to. This can be sent
	 * to either a FramesDocument or an HTML_Document, but for code
	 * outside the doc module it's preferably to send it to the
	 * FramesDocument first since it will forward it to the
	 * HTML_Document unless it's to be handled somewhere else.
	 *
	 * @param[in] mouse_x The x coordinate in document scale, relative
	 * to the upper left corner of the view. To get the full document
	 * coordinate, view_x has to be added.
	 *
	 * @param[in] mouse_y The y coordinate in document scale, relative
	 * to the upper left corner of the view. To get the full document
	 * coordinate, view_y has to be added.
	 *
	 * @param[in] shift_pressed TRUE if the shift key was pressed when
	 * doing the mouse move action.
	 *
	 * @param[in] control_pressed TRUE if control key was pressed when
	 * doing the mouse move action.
	 *
	 * @param[in] alt_pressed TRUE if alt key was pressed when doing
	 * the mouse move action.
	 *
	 * @param[in] middle_button If that button is pressed during the
	 * mouse move.
	 *
	 * @param[in] left_button If that button is pressed during the
	 * mouse move.
	 *
	 * @param[in] right_button If that button is pressed during the
	 * mouse move.
	 */
    void                MouseMove(int mouse_x, int mouse_y, BOOL shift_pressed, BOOL control_pressed, BOOL alt_pressed, BOOL left_button, BOOL middle_button, BOOL right_button);

	/**
	 * To be called when the document should handle a mouse wheel
	 * event. Typically called from the display module after it has
	 * decided which document to send the event to. This can be sent
	 * to either a FramesDocument or an HTML_Document, but for code
	 * outside the doc module it's preferably to send it to the
	 * FramesDocument first since it will forward it to the
	 * HTML_Document unless it's to be handled somewhere else.
	 *
	 * @param[in] mouse_x The x coordinate in document scale, relative
	 * to the upper left corner of the view. To get the full document
	 * coordinate, view_x has to be added.
	 *
	 * @param[in] mouse_y The y coordinate in document scale, relative
	 * to the upper left corner of the view. To get the full document
	 * coordinate, view_y has to be added.
	 *
	 * @param[in] shift_pressed TRUE if the shift key was pressed when
	 * doing the mouse wheel action.
	 *
	 * @param[in] control_pressed TRUE if control key was pressed when
	 * doing the mouse wheel action.
	 *
	 * @param[in] alt_pressed TRUE if alt key was pressed when doing
	 * the mouse wheel action.
	 *
	 * @param[in] delta The amount and direction of the mouse wheel
	 * scroll. Negative numbers are scrolling up and positive numbers
	 * are scrolling down.
	 *
	 * @param[in] vertical TRUE if the mouse wheel scrolled in the
	 * vertical direction (up-down), FALSE if it scrolled horizontally
	 * (left-right).
	 *
	 * @todo lookup details about the delta parameter.
	 */
	void				MouseWheel(int mouse_x, int mouse_y, BOOL shift_pressed, BOOL control_pressed, BOOL alt_pressed, int delta, BOOL vertical);

	/**
	 * Prevent the next mouseup event from generating a click event.
	 */
	void PreventNextClick() { m_mouse_up_may_still_be_click = FALSE; }
#endif // !MOUSELESS

#ifdef TOUCH_EVENTS_SUPPORT
    /**
	 * To be called when the document should handle a touch
	 * event. Typically called from the display module after it has
	 * decided which document to send the event to. This can be sent
	 * to either a FramesDocument or an HTML_Document, but for code
	 * outside the doc module it's preferably to send it to the
	 * FramesDocument first since it will forward it to the
	 * HTML_Document unless it's to be handled somewhere else.
	 *
	 * @param[in] type The DOM event type.
	 *
	 * @param[in] id The numeric identifier of the touch.
	 *
	 * @param[in] x The x coordinate in document scale, relative
	 * to the upper left corner of the view. To get the full document
	 * coordinate, view_x has to be added.
	 *
	 * @param[in] y The y coordinate in document scale, relative
	 * to the upper left corner of the view. To get the full document
	 * coordinate, view_y has to be added.
	 *
	 * @param[in] visual_viewport_x The x offset of the visual viewport, in document coordinates.
	 * Note that this must be the visual vieport's x offset and not the rendering view port's one.
	 * @see GetVisualViewport()
	 * @see GetVisualViewportX()
	 *
	 * @param[in] visual_viewport_y The y offset of the visual viewport, in document coordinates.
	 * Note that this must be the visual vieport's y offset and not the rendering view port's one.
	 * @see GetVisualViewport()
	 * @see GetVisualViewportY()
	 *
	 * @param[in] radius The radius of the touch in GOGI units.
	 *
	 * @param[in] shift_pressed TRUE if the shift key was pressed when
	 * doing the mouse down action.
	 *
	 * @param[in] control_pressed TRUE if control key was pressed when
	 * doing the mouse down action.
	 *
	 * @param[in] alt_pressed TRUE if alt key was pressed when doing
	 * the mouse down action.
	 *
	 * @param[in] user_data Platform supplied pointer used for feedback.
	 */
    void                TouchAction(DOM_EventType type, int id, int x, int y, int visual_viewport_x, int visual_viewport_y, int radius, BOOL shift_pressed, BOOL control_pressed, BOOL alt_pressed, void* user_data);
#endif // TOUCH_EVENTS_SUPPORT

#ifdef DOC_RETURN_TOUCH_EVENT_TO_SENDER
	/**
	 * Signal to the platform how the document responded to the given touch event.
	 * The purpose of this mechanism is to allow documents to control platform UI
	 * actions such as panning and zooming by making platforms aware of the documents
	 * decisions, and also allow platforms to control mouse simulation.
	 *
	 * @param[in] user_data The platform supplied data identifying the touch event.
	 * @param[in] prevented The document wishes the touch event suppressed.
	 *
	 * @return TRUE if mouse simulation has been taken care of, FALSE if Core should generate suitable
	 * mouse events to ensure compatibility with other browsers and non-touch aware documents.
	 */
	BOOL				SignalTouchEventProcessed(void* user_data, BOOL prevented);
#endif // DOC_RETURN_TOUCH_EVENT_TO_SENDER

	/**
	 * Propagate a key event into the given document. This is the external method
	 * that should be used to forward or inject key events to a document.
	 *
	 * @param[in] doc The target document for the key event. If no target document
	 * is provided, the event may still be handled by document-global event consumers
	 * (like Voice or autocomplete.)
	 *
	 * @param[in] helm The element that should receive the event. Can be
	 * NULL.
	 *
	 * @param[in] event The event type.
	 *
	 * @param[in] key The virtual key being pressed or released.
	 *
	 * @param[in] value The character value that the virtual key
	 * is producing; in general this might be any string of Unicode
	 * characters. If a function or modifier key, provided as NULL.
	 *
	 * @param[in] key_event_data Platform specific event data to
	 * pass along with this key event; for the benefit of plugins.
	 * Can be NULL, but any party wanting to keep a reference to it
	 * must use its reference counting methods.
	 *
	 * @param[in] keystate Flags that says which shift, control and
	 * alt keys were pressed when the event was triggered. Used to
	 * modify certain operations, such as link clicks.
	 *
	 * @param[in] repeat TRUE if the key is in repeat mode, reporting
	 * successive key event sequences.
	 *
	 * @param[in] location Indication of where the key is on the keyboard.
	 * A given keycode may be reported by more than one physical key,
	 * the location disambiguating between them, if the device and
	 * platform is able to discern.
	 *
	 * @param[in] data Extra context-dependent data to pass in with the
	 * key event. This is currently used to discriminate between a key
	 * event being issued in the context of a documentedit or not.
	 *
	 * @returns OpBoolean::IS_FALSE if the event wasn't handled at all
	 * (possible only in empty documents), OpBoolean::IS_TRUE if it
	 * was consumed and an OpStatus error code if something went
	 * terribly wrong.
	 *
	 * @see HandleKeyboardEvent
	 *
	 */
	static OP_BOOLEAN		SendDocumentKeyEvent(FramesDocument *doc, HTML_Element* helm, DOM_EventType event, OpKey::Code key, const uni_char *value, OpPlatformKeyEventData *key_event_data, ShiftKeyState keystate, BOOL repeat, OpKey::Location location, unsigned data = 0);

	/**
	 * This is called from the action system, though not the normal
	 * way since FramesDocument isn't an OpInputContext. Instead this
	 * is used as a help function (a very large help function) by the
	 * OpInputContext part of VisualDevice.
	 *
	 * @param[in] The action.
	 *
	 * @returns The "handled" state of the action. OpBoolean::IS_TRUE
	 * if the action was consumed. OpBoolean::IS_FALSE if it was
	 * unknown and should bubble.
	 *
	 * @see OpInputContext::OnInputAction()
	 *
	 * @see VisualDevice::OnInputAction()
	 */
	OP_BOOLEAN				OnInputAction(OpInputAction* action);

	/**
	 * Performs the Reset action on the form with the given number.
	 *
	 * @param[in] form_number The number of the form.
	 *
	 * @returns OpStatus::OK if successful, otherwise an error code.
	 *
	 * @todo Remove/rework. This is only used in
	 * DocumentManager::HandleLoadingFailed and it uses it the wrong
	 * way so it should be able to remove/replace with a working
	 * function.
	 */
	OP_STATUS				ResetForm(int form_number);

	/**
	 * The active HTML_Element is the element has armed with a mouse
	 * down. If the user does mouse up over the armed (active)
	 * HTML_Element it will trigger a click. A mouse up elsewhere will
	 * not trigger any click and the active HTML_Element will be
	 * reset. This is only a proxy for HTML_Document.
	 *
	 * @param[in] active The element or NULL.
	 *
	 * @see HTML_Document::SetActiveHTMLElement()
	 *
	 * @todo Remove and make callers access HTML_Document directly?
	 */
	void					SetActiveHTMLElement(HTML_Element* active_element);

	/**
	 * The active HTML_Element is the element has armed with a mouse
	 * down. If the user does mouse up over the armed (active)
	 * HTML_Element it will trigger a click. A mouse up elsewhere will
	 * not trigger any click and the active HTML_Element will be
	 * reset. This is only a proxy for HTML_Document.
	 *
	 * @returns The element or NULL.
	 *
	 * @see HTML_Document::GetActiveHTMLElement()
	 *
	 * @todo Remove and make callers access HTML_Document directly?
	 */
	HTML_Element*			GetActiveHTMLElement();

	/**
	 * If this element or a child of it is hovered, then the
	 * hover is moved to its parent, otherwise it's left alone.
	 * This is used when elements are removed from the tree to not
	 * lose all hover information so that we still can send
	 * hover events to the remaining elements.
	 *
	 * @param[in] elm The element that should have the hover removed.
	 */
	void					BubbleHoverToParent(HTML_Element* elm);

	/**
	 * This is the proper method to send a window event (an event
	 * targetted at the window rather than an element). It
	 * will be sent through the script system (if enabled) and to the
	 * relevant document part.
	 *
	 * <p>This method is for window events. Mouse events should be
	 * sent through HandleMouseEvent, key events through
	 * HandleKeyboardEvent and general events through HandleEvent. This is
	 * for instance used by ONFOCUS events.
	 *
	 * @param[in] event The event type.
	 *
	 * @param[in] interrupt_thread The thread that triggered this
	 * event or NULL. If not NULL, the interrupt_thread will be
	 * blocked until this event has been handled.
	 *
	 * @param[in] type_custom Name of the event. This is ignored when
	 * event != DOM_EVENT_CUSTOM.
	 *
	 * @param[in] event_param Event specific data that will be passed to
	 * event's constructor. Ignored when event != DOM_EVENT_CUSTOM.
	 *
	 * @param[out] created_thread If a pointer to an ES_Thread pointer
	 * is used here it will be set to the thread created for the event
	 * or be left as is if the event didn't spawn a new thread.
	 * @returns OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 *
	 * @see HTML_Element::HandleEvent()
	 *
	 * @see HandleEvent()
	 *
	 * @see HandleKeyboardEvent()
	 *
	 * @see HandleMouseEvent()
	 *
	 * @see HandleDocumentEvent()
	 *
	 */
	OP_STATUS		HandleWindowEvent(DOM_EventType event, ES_Thread* interrupt_thread = NULL,
					                  const uni_char* type_custom = NULL, int event_param = 0,
					                  ES_Thread** created_thread = NULL);

	/**
	 * This is the proper method to send an event targetted at the document.
	 * This is, for instance, used for ONSCROLL and ONRESIZE events.
	 *
	 * @param event The event type.
	 *
	 * @returns OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS		HandleDocumentEvent(DOM_EventType event);

	/**
	 * This is the proper method to send a key event (compared to
	 * window, mouse and general events, see below) inside Opera. It
	 * will send the event through the script system (if enabled) and
	 * to the relevant document part.
	 *
	 * <p>This method is for key events. Mouse events should be sent
	 * through HandleMouseEvent, general events through HandleEvent
	 * and window events through HandleWindowEvent. This is for
	 * instance used by ONKEYDOWN and ONKEYPRESS events.
	 *
	 * @param[in] event The event type.
	 *
	 * @param[in] target The element that should receive the
	 * event. Not NULL.
	 *
	 * @param[in] keycode The virtual keycode, as defined by OpKey::Code.
	 *
	 * @param[in] key_value The character value produced by the virtual key,
	 * NUL-terminated. NULL if the key does not have an associated character value.
	 *
	 * @param[in] key_event_data Platform specific event data to
	 * pass along with this key event; for the benefit of plugins.
	 * Can be NULL, but any party wanting to keep a reference to it
	 * must use its reference counting methods.
	 *
	 * @param[in] modifiers Flags that says which shift, control and
	 * alt keys were pressed when the event was triggered. Used to
	 * modify certain operations, such as link clicks.
	 *
	 * @param[in] repeat TRUE if the key is in repeat mode, reporting
	 * successive key event sequences.
	 *
	 * @param[in] location For the given keycode, where did it occur?
	 * Some keys map to the same keycode, but instead use their location
	 * to discriminate (e.g., left and right shift keys.)
	 *
	 * @param[in] data Extra context-dependent data to pass in with the
	 * key event. This is currently used to discriminate between a key
	 * event being issued in the context of a documentedit or not.
	 *
	 * @returns OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 *
	 * @see HTML_Element::HandleEvent()
	 *
	 * @see HandleWindowEvent()
	 *
	 * @see HandleEvent()
	 *
	 * @see HandleMouseEvent()
	 *
	 */
	OP_STATUS		HandleKeyboardEvent(DOM_EventType event, HTML_Element* target, OpKey::Code keycode, const uni_char* key_value, OpPlatformKeyEventData *key_event_data, ShiftKeyState modifiers, BOOL repeat, OpKey::Location location, unsigned data = 0);

	/**
	 * This is the proper method to send a hashchange event.
	 * http://www.whatwg.org/specs/web-apps/current-work/multipage/history.html#event-hashchange
	 *
	 * @param old_fragment    old fragment identifier
	 * @param new_fragment    new fragment identifier
	 *
	 * @see HandleEvent()
	 */
	OP_STATUS		HandleHashChangeEvent(const uni_char* old_fragment, const uni_char* new_fragment);

	/**
	 * This is the proper method to send a mouse event (ONLOAD for
	 * instance) inside Opera. It will send the event through the
	 * script system (if enabled) and to the relevant document part.
	 *
	 * <p>This method is for mouse events. General events should be
	 * sent through HandleEvent, key events through HandleKeyboardEvent and
	 * window events through HandleWindowEvent.
	 *
	 * @param[in] event The event type.
	 *
	 * @param[in] related_target Some events have a "related"
	 * element. This can be NULL (and often is).
	 *
	 * @param[in] target The element that should receive the
	 * event. Not NULL.
	 *
	 * @param[in] referencing_element An associated element (only used
	 * for imagemaps I think).
	 *
	 * @param[in] offset_x The offset inside the target element. 0 for
	 * the left edge. See the calculate_offset_lazily param for more info.
	 *
	 * @param[in] offset_y The offset inside the target element. 0 for
	 * the top edge. See the calculate_offset_lazily param for more info.
	 *
	 * @param[in] document_x The x position of the mouse event in
	 * document coordinates.
	 *
	 * @param[in] document_y The y position of the mouse event in
	 * document coordinates.
	 *
	 * @param[in] modifiers Flags that says which shift, control and
	 * alt keys were pressed when the event was triggered. Used to
	 * modify certain operations, such as link clicks.
	 *
	 * @param[in] sequence_count_and_button_or_delta If it's a button
	 * press this is a mix of a sequence count and a button
	 * identifier. Use the macros EXTRACT_MOUSE_BUTTON and
	 * EXTRACT_SEQUENCE_COUNT to get the different parts. If it's a
	 * mouse wheel action, it says how far and in what direction the
	 * mouse wheel was spun. Negative numbers are scrolling up and
	 * positive numbers are scrolling down.
	 *
	 * @param[in] interrupt_thread The thread that triggered this
	 * event or NULL. If not NULL, the interrupt_thread will be
	 * blocked until this event has been handled.
	 *
	 * @param[in] first_part_of_paired_event Set to TRUE for the first of two
	 * events that are sent as a pair, example ONMOUSEOVER -> ONMOUSEMOVE. That
	 * way even if there is no mouse handle when the first event is sent, the
	 * second event will still be send just in case the first event handle
	 * registered a listener.
	 *
	 * @param[in] calculate_offset_lazily If offset_x and offset_y isn't available
	 * this can be set to TRUE and the calculation will be done if a
	 * script asks for the value. This is the fallback for events
	 * such as "mouseleave" where the hit traversal didn't include
	 * the necessary information. When setting this TRUE some extra
	 * information need to be included: doc_x and doc_y of the mouse
	 * event. That is to be stored in offset_x and offset_y.
	 *
	 * Note, if calculate_offset_lazily is TRUE, then the offset
	 * values will not be available to HTML_Element::HandleEvent().
	 *
	 * @param[in] visual_viewport_x The x offset of the visual viewport, in document coordinates.
	 * If NULL, the visual viewport x position must be got from the document.
	 * Note that this must be the visual vieport's x offset and not the rendering view port's one.
	 * @see GetVisualViewport()
	 * @see GetVisualViewportX()
	 *
	 * @param[in] visual_viewport_y The y offset of the visual viewport, in document coordinates.
	 * If NULL, the visual viewport y position must be got from the document.
	 * Note that this must be the visual vieport's y offset and not the rendering view port's one.
	 * @see GetVisualViewport()
	 * @see GetVisualViewportY()
	 *
	 * @param[in] has_keyboard_origin If TRUE, the event originates from a key
	 * press (for example a context menu triggered from the keyboard).
	 *
	 * @returns OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 *
	 * @see HTML_Element::HandleEvent()
	 *
	 * @see HandleWindowEvent()
	 *
	 * @see HandleKeyboardEvent()
	 *
	 * @see HandleEvent()
	 */
	OP_STATUS       HandleMouseEvent(DOM_EventType event,
	                                 HTML_Element* related_target,
	                                 HTML_Element* target,
	                                 HTML_Element* referencing_element,
	                                 int offset_x, int offset_y,
	                                 int document_x, int document_y,
	                                 ShiftKeyState modifiers,
	                                 int sequence_count_and_button_or_delta,
	                                 ES_Thread* interrupt_thread = NULL,
	                                 BOOL first_part_of_paired_event = FALSE,
	                                 BOOL calculate_offset_lazily = FALSE,
	                                 int* visual_viewport_x = NULL, int* visual_viewport_y = NULL,
	                                 BOOL has_keyboard_origin = FALSE);

#ifdef TOUCH_EVENTS_SUPPORT
	/**
	 * This is the proper method to send a touch event inside Opera.
	 * It will send the event through the script system (if enabled) and
	 * to the relevant document part.
	 *
	 * <p>This method is for touch events. General events should be
	 * sent through HandleEvent, key events through HandleKeyboardEvent,
	 * window events through HandleWindowEvent and mouse events
	 * through HandleMouseEvent.
	 *
	 * @param[in] event The event type.
	 *
	 * @param[in] target The element that should receive the
	 * event. Not NULL.
	 *
	 * @param[in] offset_x The offset inside the target element. 0 for
	 * the left edge.
	 *
	 * @param[in] offset_y The offset inside the target element. 0 for
	 * the top edge.
	 *
	 * @param[in] document_x The x position of the touch event in
	 * document coordinates.
	 *
	 * @param[in] document_y The y position of the touch event in
	 * document coordinates.
	 *
	 * @param[in] visual_viewport_x The x offset of the visual viewport, in document coordinates.
	 * Note that this must be the visual vieport's x offset and not the rendering view port's one.
	 * @see GetVisualViewport()
	 * @see GetVisualViewportX()
	 *
	 * @param[in] visual_viewport_y The y offset of the visual viewport, in document coordinates.
	 * Note that this must be the visual vieport's y offset and not the rendering view port's one.
	 * @see GetVisualViewport()
	 * @see GetVisualViewportY()
	 *
	 * @param[in] radius The radius of the touch in document units.
	 *
	 * @param[in] sequence_count_and_button_or_delta Use EXTRACT_CLICK_INDICATOR
	 * to extract if this is a real click or if it has moved too much to be
	 * considered a click. EXTRACT_MOUSE_BUTTON will return the numeric id of the
	 * touch. EXTRACT_SEQUENCE_COUNT will not return any relevant information.
	 *
	 * @param[in] modifiers Flags that says which shift, control and
	 * alt keys were pressed when the event was triggered.
	 *
	 * @param[in] user_data Platform supplied event identifier. See
	 * TWEAK_DOC_RETURN_TOUCH_EVENT_TO_SENDER.
	 *
	 * @param[in] interrupt_thread The thread that triggered this
	 * event or NULL. If not NULL, the interrupt_thread will be
	 * blocked until this event has been handled.
	 *
	 * @returns OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 *
	 * @see HTML_Element::HandleEvent()
	 *
	 * @see HandleMouseEvent()
	 *
	 * @see HandleWindowEvent()
	 *
	 * @see HandleKeyboardEvent()
	 *
	 * @see HandleEvent()
	 */
	OP_STATUS       HandleTouchEvent(DOM_EventType event,
	                                 HTML_Element* target,
	                                 int id, int offset_x, int offset_y,
	                                 int document_x, int document_y,
	                                 int visual_viewport_x, int visual_viewport_y, int radius,
									 int sequence_count_and_button_or_delta,
	                                 ShiftKeyState modifiers, void* user_data,
	                                 ES_Thread* interrupt_thread = NULL);
#endif // TOUCH_EVENTS_SUPPORT


	/**
	 * This is the proper method to send an event (except window,
	 * mouse and key events, see below) inside Opera. It will send the
	 * event through the script system (if enabled) and to the
	 * relevant document part.
	 *
	 * <p>This method is for general events. Mouse events should be
	 * sent through HandleMouseEvent, key events through
	 * HandleKeyboardEvent and window events through
	 * HandleWindowEvent. This is for instance used by ONFOCUS and
	 * ONSCROLL events.
	 *
	 * @param[in] event The event type.
	 *
	 * @param[in] related_target Some events have a "related"
	 * element. This can be NULL (and often is).
	 *
	 * @param[in] target The element that should receive the
	 * event. Not NULL.
	 *
	 * @param[in] modifiers Flags that says which shift, control and
	 * alt keys were pressed when the event was triggered. Used to
	 * modify certain operations, such as link clicks.
	 *
	 * @param[in] detail Detail (repeat count for repeat events)
	 *
	 * @param[in] interrupt_thread The thread that triggered this
	 * event or NULL. If not NULL, the interrupt_thread will be
	 * blocked until this event has been handled.
	 *
	 * @param[out] handled If a pointer to a BOOL is given here, it
	 * will contain TRUE if the event was dispatched through the
	 * script system and FALSE if no script will look at it. It's ok
	 * to use NULL here.
	 *
	 * @param[in] id Event's id. Used to identify an event in case more
	 * than one event of a given type may be handled at the same time.
	 * May also be used to associate an event with some external data.
	 *
	 * @returns OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 *
	 * @see HTML_Element::HandleEvent()
	 *
	 * @see HandleWindowEvent()
	 *
	 * @see HandleKeyboardEvent()
	 *
	 * @see HandleMouseEvent()
	 */
	OP_STATUS       HandleEvent(DOM_EventType event,
	                            HTML_Element* related_target, HTML_Element* target,
	                            ShiftKeyState modifiers,
	                            int detail = 0,
	                            ES_Thread* interrupt_thread = NULL,
	                            BOOL* handled = NULL,
	                            unsigned int id = 0);

	/**
	 * To be called when an event has been completely handled by
	 * scripts, regardless if the script cancelled the event or
	 * not. It will relay the information to different sub systems
	 * that needs to know about the event.
	 *
	 * @param[in] event The event type.
	 *
	 * @param[in] target The element that was targetted by the event.
	 */
	void					HandleEventFinished(DOM_EventType event, HTML_Element* target);

	/**
	 * Enum representing importance level of Free()
	 */
	enum FreeImportance
	{
		/* Freeing of a document's memory is very important to the caller
         * When this level of importance is passed to the Free() also documents which are
         * supposed to be kept in the memory as long as possible will also be freed unconditionally(if any)
         */
		FREE_VERY_IMPORTANT,
		/* Freeing of a document's memory is important to the caller
         * When this level of importance is passed to the Free() also documents which are
         * supposed to be kept in the memory as long as possible will also be freed but only if they are not too close to the current document in history
         */
		FREE_IMPORTANT,
		/* Caller wishes the document's memory would be free but because of some factors it can not be freed
		 * it's not a big deal at the moment
		 */
		FREE_NORMAL
	};

	/**
	 * Frees as much as possible of the internal structure to minimize
	 * size.
	 *
	 * @param[in] layout_only If TRUE, keeps most things except the
	 * layout tree.
	 *
	 * @returns TRUE if the object was minimized as planned, FALSE if
	 * it wasn't possible since someone else requires things to
	 * remain.
	 */
	BOOL            Free(BOOL layout_only = FALSE, FreeImportance free_importance = FREE_NORMAL);


	/**
	 * Stops loading everything that is currently loading, including
	 * child frames, iframes, inlines (images, scripts, stylesheets).
	 *
	 * @param[in] format If TRUE we will try to reorganize the
	 * document to adapt as much as possible to the data we currently
	 * have (reflow and such), if FALSE we will simply stop loading
	 * everything. FALSE is suitable for when a window is being closed
	 * and the actual contents won't matter.
	 *
	 * @param[in] aborting If TRUE, we will note that this document
	 * was explicitly aborted and avoid calling triggering new load
	 * actions (redirects, load images on demand). If FALSE (default)
	 * we will not prevent such things from happening.
	 *
	 * @param[in] stop_plugin_streams TRUE stops the plugin streams,
	 * FALSE spares the plugin streams.
	 *
	 * @returns OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS       StopLoading(BOOL format = TRUE, BOOL aborting = FALSE, BOOL stop_plugin_streams = TRUE);

	/**
	 * Welcome to one of the most poorly named methods in Opera. This
	 * method has three modes depending on the event parameter. It can
	 * either do a form submit, it can follow a link or it can display
	 * (possibly) a tooltip.
	 *
	 * @param[in] url The url that we either hovered or clicked or
	 * triggered otherwise.
	 *
	 * @param[in] win_name If this opens a new document, let it open
	 * in a window named like this. Can be NULL if it should open in
	 * the current document's place.
	 *
	 * @param[in] event The triggering event. If it's ONMOUSEOVER we
	 * will let the UI know what the url is so that it can display a
	 * tooltip or something in the statusbar. If it's ONSUBMIT, a
	 * submit process will start. If it's ONCLICK it will play a click
	 * sound or do some other special actions for url clicks.
	 *
	 * @param[in] shift_pressed TRUE if shift was pressed while doing
	 * something with the link/url.
	 *
	 * @param[in] shift_pressed TRUE if control was pressed while doing
	 * something with the link/url.
	 *
	 * @param[in] thread The ecmascript thread that triggered the
	 * action or NULL if no such thread.
	 *
	 * @param[in] event_elm The element that received the event that
	 * triggered this call. May be NULL.
	 *
	 * @returns OpStatus::OK unless there was a serious error (OOM) in
	 * which case an error code will be returned. Since most of what
	 * happens here is asynchronous, it's not impossible that things
	 * might fail later even if this returns OpStatus::OK.
	 *
	 * @todo Split into three different methods? The callers often
	 * knows which the event is so we don't need this "handle any
	 * event" method.
	 */
	OP_STATUS               MouseOverURL(const URL& url,
										const uni_char* win_name,
										DOM_EventType event,
										BOOL shift_pressed,
										BOOL control_pressed,
										ES_Thread *thread = NULL,
										HTML_Element *event_elm = NULL);

	/**
	 * Makes sure the document has content. Used for instance when
	 * walking in history to reload documents that have been removed
	 * from memory.
	 *
	 * @returns Never returns OpStatus::OK.  Returns
	 * OpBoolean::IS_TRUE if ES environment was created,
	 * OpBoolean::IS_FALSE if not.
	 */
	OP_BOOLEAN		CheckSource();

	/**
	 * Starts loading all images in the document in case we had
	 * previously not loaded them, for instance because the user had
	 * said that he didn't wanted any images loaded. Will disregard
	 * from any user settings that normally prevent image loads.
	 *
	 * @param[in] first_image_only If TRUE will only load the first
	 * yet-not-loaded image and then return.
	 *
	 * @returns OpBoolean::IS_FALSE if no images were loaded, for
	 * instance because the setting was still to not load any images
	 * or because there were no documents to load images
	 * in. OpBoolean::IS_TRUE if there were documents to start loading
	 * images in and an error code if the operation couldn't complete.
	 */
	OP_BOOLEAN		LoadAllImages(BOOL first_image_only);

	/**
	 * Stop loading all image inlines.
	 *
	 * @returns OpBoolean::IS_FALSE. Always. The return value is
	 * likely to go away soon.
	 */
	void			StopLoadingAllImages();

	/**
	 * Stop loading all inlines.
	 *
	 * @param stop_plugin_streams  If TRUE, plugins streams are terminated too.
	 */
	void			StopLoadingAllInlines(BOOL stop_plugin_streams);

	/**
	 * If we initially blocked iframes from loading, this makes us
	 * load them at a later timer, for instance when the user removes
	 * the "block iframes" preference or changes layout mode.
	 *
	 * <p>Currently defunct. Will not do anything until fixed.
	 */
	OP_STATUS		LoadAllIFrames();

	/**
	 * Flag to indicate if this is a document purely to be able to
	 * run a javascript url. If so, then we need to supress some
	 * things in it, for instance the onload event, until after
	 * the javascript url has been executed.
	 *
	 * Set to FALSE once the javascript url has executed and make sure
	 * someone takes a new look at the document to see if it is now
	 * finished.
	 *
	 * @param val The new value of the flag. FALSE means that we
	 * should treat this is a normal document and if there was
	 * a javascript url, it has not completed so no special casing
	 * is needed.  TRUE means that we should delay for instance the
	 * onload event until later.
	 */
	void			SetWaitForJavascriptURL(BOOL val) { about_blank_waiting_for_javascript_url = val; }

	/**
	 * Resets all callbacks so that we don't get any more
	 * messages. If really_all is not set we only disable
	 * the loading related messages.
	 *
	 * @param really_all TRUE if we should stop looking at all
	 * messages, FALSE if we should only stop looking at
	 * loading related messages.
	 */
	OP_STATUS 		UnsetAllCallbacks(BOOL really_all);

	/**
	 * Changes how the document is displayed.
	 *
	 * @param show_images TRUE if images should be shown, FALSE if
	 * they shouldn't be shown.
	 *
	 * @param load_images TRUE if images should be loaded. If this is
	 * FALSE but show_images is TRUE then only previously loaded or
	 * cached images are shown.
	 *
	 * @param css_mode If CSS should be loaded and used or not.
	 *
	 * @param check_expiry If the document should be locked or is
	 * allow to expire and be reloaded.
	 *
	 * @returns OpStatus::OK if flags were set successfully, an error
	 * code otherwise.
	 */
	OP_STATUS		SetMode(BOOL win_show_images, BOOL win_load_images, CSSMODE win_css_mode, CheckExpiryType check_expiry);

	/**
	 * Tells the document to draw an empty document instead of the
	 * real document. This is to be used while loading until we have
	 * decided that it's time to display the document, when that flag
	 * will be reset by methods internal to FramesDocument.
	 *
	 * NOTE: The above text is only a small part of the truth.
	 *
	 * @param keep_cleared TRUE if the document should be painted
	 * empty on future paints, FALSE if we don't want to force
	 * it. Calling the function with FALSE won't replace an earlier
	 * blocking of paints.
	 *
	 * @todo Make mstensho or markuso document the new behaviour of
	 * this method.
	 */
	void			ClearScreenOnLoad(BOOL keep_cleared = FALSE);

	/**
	 * When activating an old document, this checks if we can use the
	 * document as is or if we have to initiate a new load from the
	 * network. This includes checks for partially loaded documents
	 * (loads aborted) and expiry policies.
	 *
	 * @param check_expired How hard we should treat expired
	 * documents.
	 *
	 * @returns TRUE if the document has to be reloaded, FALSE if it
	 * can be used as is.
	 */
	BOOL			UrlNeedReload(CheckExpiryType check_expired);

	/**
	 * Returns the title specified in the document. Will
	 * return NULL if there is none or if it couldn't be
	 * composed due to OOM.
	 *
	 * @param buffer Must not be NULL (default value to be backwards
	 * compatible). Used to store the value in case it must be
	 * composed from several sources.
	 *
	 * @return The title or NULL. Might not live longer that the
	 * buffer but do not have to be explicitly freed.
	 */
	const uni_char*	Title(TempBuffer* buffer);

	/**
	 * Returns the description found in the document or an empty
	 * string if nothing.
	 *
	 * <p>This method is only used by OpBrowserView in Quick and it
	 * should be exposed through some more suitable external
	 * interface.
	 *
	 * @param[out] desc Out parameter that will get the resulting
	 * description string.
	 *
	 * @returns OpStatus::OK unless OOM. OOM will result in
	 * OpStatus::ERR_NO_MEMORY.
	 *
	 * @todo Expose through an official api or at least make it an
	 * official module API through module.export so that other
	 * platforms don't need to have it.
	 */
    OP_STATUS 		GetDescription(OpString &desc);

#ifdef SHORTCUT_ICON_SUPPORT

	/**
	 * Returns a pointer to the url that contains the current favicon
	 * for the document.
	 *
	 * @returns A pointer to an URL. Never a NULL pointer, but it may
	 * be an empty URL.
	 */
	URL*			Icon();
#endif // SHORTCUT_ICON_SUPPORT

#ifndef HAS_NOTEXTSELECTION
	/**
	 * Call this with the argument FALSE to stop any textselection
	 * going on. Should only be called with TRUE as argument from
	 * within FramesDocument.
	 */
    void				SetSelectingText(BOOL selecting_text) { is_selecting_text = selecting_text; };

	/**
	 * Gets a value indicating whether a text selection is currently going on.
	 */
	BOOL				GetSelectingText() { return is_selecting_text; }

	/**
	 * Returns if this FramesDocument was selecting text before the
	 * last mouse up. If it was, then other actions shouldn't be done
	 * on that MouseUp event.
	 *
	 * @returns TRUE if the last MouseUp ended a text selection, FALSE
	 * otherwise.
	 */
	BOOL                GetWasSelectingText() { return was_selecting_text; }

	/**
	 * Selects everything (or deselects everything) in the current
	 * document or this document if it's not a frameset.
	 *
	 * @param[in] select If TRUE, selects everything, if FALSE
	 * deselects everything.
	 *
	 * @returns OpBoolean::IS_TRUE if selection anywhere was changed,
	 * OpBoolean::IS_FALSE otherwise. This return value is adapted to
	 * be used in FramesDocument::OnInputAction()
	 */
	OP_BOOLEAN		SelectAll(BOOL select = TRUE);

	/**
	 * Removes all selections in all documents.
	 *
	 * @returns OpBoolean::IS_TRUE if selection anywhere was changed,
	 * OpBoolean::IS_FALSE otherwise. This return value is adapted to
	 * be used in FramesDocument::OnInputAction()
	 */
	OP_BOOLEAN		DeSelectAll();

	/**
	 * Removes the current selection and starts a new selection
	 * based in the coordinate sent in. The other point of the
	 * selection is set/changed through later calls to MoveSelectionFocusPoint().
	 *
	 * This function will recursively activate all frames with their
	 * host documents, starting with the frame hosting the selection
	 * back to the document root frame.
	 *
	 * x and y are in the document coordinate system.
	 *
	 * @param[in] x The document x position of the starting point.
	 *
	 * @param[in] y The document y position of the starting point.
	 *
	 * @returns OpStatus::OK if operation completed successfully and
	 * an error code otherwise. OpStatus::ERR_NO_MEMORY if OOM.
	 */
	OP_STATUS		StartSelection(int x, int y);

	/**
	 * Moves the anchor selection point to the new position. If there is no
	 * active selection a new selection will be initiated at the specified
	 * coordinates. In addition to modifying the text selection this function
	 * will also move the document caret if in documment edit mode.
	 *
	 * This function will recursively activate all frames with their
	 * host documents, starting with the frame hosting the selection
	 * back to the document root frame.
	 *
	 * x and y are in the document coordinate system.
	 *
	 * @param[in] x The new document x position for the anchor point.
	 *
	 * @param[in] y The new document y position for the anchor point.
	 *
	 * @returns OpStatus::OK if operation completed successfully and
	 * an error code otherwise. OpStatus::ERR_NO_MEMORY if OOM.
	 */
	OP_STATUS		MoveSelectionAnchorPoint(int x, int y);

	/**
	 * Moves the current ending position of the selection. It can be
	 * changed through later calls to MoveSelectionFocusPoint.
	 *
	 * x and y are in the document coordinate system.
	 *
	 * @param[in] x The document x position of the new ending point.
	 *
	 * @param[in] y The document y position of the new ending point.
	 *
	 * @param[in] end_selection TRUE if this seems to be the last
	 * MoveSelectionFocusPoint in a sequence of MoveSelectionFocusPoint calls.
	 * If it's TRUE and we have a delayed decision, we'll force a
	 * collapsed selection somewhere anyway.
	 */
	void	MoveSelectionFocusPoint(int x, int y, BOOL end_selection);

	/**
	 * Selects the word at the specified coordinates. If no word exists
	 * at the point the nearest word will be picked for selection.
	 *
	 * This function will activate this frame.
	 *
	 * @param[in] x The document x position.
	 *
	 * @param[in] y The document y position.
	 *
	 * @returns OpStatus::OK if operation completed successfully and
	 * an error code otherwise. OpStatus::ERR_NO_MEMORY if OOM.
	 */
	OP_STATUS		SelectWord(int x, int y);

#ifdef NEARBY_INTERACTIVE_ITEM_DETECTION
	/**
	 * Selects the link at or nearby the specified position. If multiple links
	 * are found the topmost will be selected.
	 *
	 * When selecting the link, all selectable elements inside the link element
	 * will be selected.
	 *
	 * This function will activate this frame.
	 *
	 * @param [in] x Selection X-coordinate in document coordinate system.
	 * @param [in] y Selection Y-coordinate in document coordinate system.
	 *
	 * @return OpStatus::OK if the operation was successfull and a link was
	 *         selected. If no link was found at the specified coordinates or
	 *         if the operation failed OpStatus::ERR is returned. If OOM
	 *         OpStatus::ERR_NO_MEMORY is returned.
	 */
	OP_STATUS		SelectNearbyLink(int x, int y);
#endif // NEARBY_INTERACTIVE_ITEM_DETECTION

	/**
	 * Moves the current ending position of the selection if selecting text is in progress.
	 *
	 * x and y are in the document coordinate system.
	 *
	 * @param[in] x The document x position of the new ending point.
	 *
	 * @param[in] y The document y position of the new ending point.
	 */
	void 	EndSelectionIfSelecting(int x, int y);

	/**
	 * Returns the selected text in the given buffer. Check with
	 * GetSelectedTextLen how big the buffer has to be, but make sure
	 * to add room for a terminating NULL.
	 *
	 * <p>Gets the selected text, from the active frame/document if
	 * there's currently one, otherwise from the attached HTML_Document.
	 *
	 * @param buf The buffer where text will be put in.
	 *
	 * @param buf_len The size of the buffer in the buf parameter.
	 *
	 * @param[in] include_element_selection If a selection in an element
	 * should be returned, or if the method should only consider
	 * selections in the document text. Elements can either
	 * be form elements or SVG.
	 *
	 * @returns TRUE if a selection could be retrieved, FALSE
	 * otherwise, for example if there was nothing selected or there
	 * was no active frame nor document.
	 */
	BOOL	GetSelectedText(uni_char *buf, long buf_len, BOOL include_element_selection = FALSE);

	/**
	 * See GetSelectedText.
	 *
	 * @param[in] include_element_selection If a selection in an element
	 * should be returned or if the method should only consider
	 * selections in the document text. Elements can either
	 * be form elements or SVG.
	 *
	 * @returns The length of the selected text, not including any
	 * terminating NULL.
	 */
	long	GetSelectedTextLen(BOOL include_element_selection = FALSE);

	/**
	 * Returns if text is currently selected. For the active
	 * frame/document if there's currently one, otherwise from
	 * the attached HTML_Document.
	 *
	 * @param[in] include_element_selection If a selection in an element
	 * should be returned or if the method should only consider
	 * selections in the document text. Elements can either
	 * be form elements or SVG.
	 *
	 * @returns TRUE if there is a selection, FALSE otherwise.
	 */
	BOOL	HasSelectedText(BOOL include_element_selection = FALSE);

	/**
	 * Retrieves the text selection start coordinates. The coordinates are
	 * specified in the document coordinate system.
	 *
	 * This function should be used carefully. It may suffer slow performance
	 * as it will trigger a reflow that might take a long time.
	 *
	 * @param[out] x The document X-position.
	 * @param[out] y The document Y-position.
	 * @param[out] line_height Pixel height of line where selection starts.
	 * @return OpStatus::OK if operation completed successfully and there is an
	 *         active selection. If there is no active text selection
	 *         OpStatus::ERR is returned. If there is no known visible area for
	 *         the document then OpStatus::ERR is returned.
	 *         OpStatus::ERR_NO_MEMORY is returned if the operation failed
	 *         because of out of memory.
	 */
	OP_STATUS	GetSelectionAnchorPosition(int &x, int &y, int &line_height);

	/**
	 * Retrieves the text selection end coordinates. The coordinates are
	 * specified in the document coordinate system.
	 *
	 * This function should be used carefully. It may suffer slow performance
	 * as it will trigger a reflow that might take a long time.
	 *
	 * @param[out] x The document X-position.
	 * @param[out] y The document Y-position.
	 * @param[out] line_height Pixel height of line where selection ends.
	 * @return OpStatus::OK if operation completed successfully and there is an
	 *         active selection. If there is no active text selection
	 *         OpStatus::ERR is returned. If there is no known visible area for
	 *         the document then OpStatus::ERR is returned.
	 *         OpStatus::ERR_NO_MEMORY is returned if the operation failed
	 *         because of out of memory.
	 */
	OP_STATUS	GetSelectionFocusPosition(int &x, int &y, int &line_height);

#if !defined HAS_NOTEXTSELECTION && defined _X11_SELECTION_POLICY_
	/**
	 * Copies currently selected text to the mouse selection buffer. Function is
	 * only to be used when a new selection has been completed by a mouse
	 * (on mouse up).
	 *
	 * @return OpStatus::OK when text was copied or when no text was selected
	 *         OpStatus::ERR_NO_MEMORY on memory errors or OpStatus::ERR in
	 *         other cases.
	 */
	OP_STATUS CopySelectedTextToMouseSelection();
#endif

	/**
	 * Helper method that does GetSelectedTextLen and then
	 * GetSelectedText and handles all allocations.
	 *
	 * @param[out] text The OpString that will get the selected text.
	 *
	 * @param[in] include_element_selection If a selection in an element
	 * should be returned, or if the method should only bother
	 * with selections in the document text. Elements can either
	 * be form elements or SVG.
	 *
	 * @returns OpStatus::ERR_NO_MEMORY if we encountered an OOM
	 * problem, OpStatus::OK otherwise.
	 *
	 * @see GetSelectedText
	 * @see GetSelectedTextLen
	 */
	OP_STATUS			GetSelectedText(OpString& text, BOOL include_element_selection = FALSE)
	{
		UINT32 len = GetSelectedTextLen();

		if (len > 0)
		{
			if (!text.Reserve(len + 1) || !GetSelectedText(text.CStr(), len + 1, include_element_selection))
				return OpStatus::ERR_NO_MEMORY;
		}

		return OpStatus::OK;
	}

	/**
	 * Expands the selection so that if a part of a word is selected,
	 * the whole word will get selected, or the whole sentence or the
	 * whole paragraph. All depending on the selection_type.
	 *
	 * If there is no current text selection, nothing will happen.
	 *
	 * @param[in] selection_type How much to expand, for instance
	 * TEXT_SELECTION_SENTENCE.
	 */
	void			ExpandSelection(TextSelectionType selection_type);

	/**
	 * Call this if you think we should start a text selection on the
	 * given coordinates. This function knows about all the times we
	 * shouldn't and will in those cases not start a text selection.
	 *
	 * @param[in] x The x document coordinate the user indicated that
	 * text selection maybe should start at.
	 *
	 * @param[in] y The y document coordinate the user indicated that
	 * text selection maybe should start at.
	 */
	void			MaybeStartTextSelection(int x, int y);

#endif // !HAS_NOTEXTSELECTION

	/**
	 * This method makes sure that the current selections (normal text
	 * selection and search hit text selection) don't have any
	 * reference to the element. Used when elements are removed from
	 * the tree.
	 *
	 * @param[in] The element to remove references too.
	 */
	void			RemoveFromSelection(HTML_Element* element);

	/**
	 * This method makes sure that the current selections (normal text
	 * selection and search hit text selection) don't have any cached
	 * data that depnds on the current layout tree. Used when elements
	 * are reflowed/have their layout box recreated.
	 *
	 * @param[in] The element to remove layout references too.
	 */
	void			RemoveLayoutCacheFromSelection(HTML_Element* element);

	/**
	 * Removes any current selections.
	 *
	 * @param[in] clear_focus_and_highlight If TRUE removes
	 * the focus and highlight from any elements having those. Will
	 * revert focus back to the default, typically the body element.
	 *
	 * @param clear_search If TRUE (FALSE is the default) also the
	 * search selection will be cleared. Note: Search hits are marked
	 * with selections internally.
	 *
	 * @todo Clean up the mess that is text selection, search highlight,
	 * spatnav highlight, ordinary highlight and focus. The main problem
	 * right now is that since the text selection and the spatnav highlight
	 * is the same (if skinned highlight is disabled), code that want to
	 * change only one of them may accidently change the other as well.
	 */
	void		ClearSelection(BOOL clear_focus_and_highlight, BOOL clear_search = FALSE);

	TextSelection* GetTextSelection() { return text_selection; }
	void SetTextSelectionPointer(TextSelection* ptr) { OP_ASSERT(!text_selection || !"Leaking TextSelection"); text_selection = ptr; }

	/**
	 * Removes any current selections.
	 *
	 * @param[in] clear_focus_and_highlight If TRUE (default) removes
	 * the focus and highlight from any elements having those.
	 *
	 * @param clear_search If TRUE (FALSE is the default) also the
	 * search selection will be cleared. Note: Search hits are marked
	 * with selections internally.
	 *
	 * @param completely_remove_selection If TRUE (FALSE is the default), remove
	 * the selection even if it's empty (collapsed). If this is TRUE text_selection
	 * will always be NULL after the call.
	 */
	void			ClearDocumentSelection(BOOL clear_focus_and_highlight = TRUE, BOOL clear_search = FALSE, BOOL completely_remove_selection = FALSE);

#ifndef HAS_NOTEXTSELECTION
	/**
	 * Sets a new selection replacing the current selection.
	 *
	 * @param[in] start The start point of the selection. Not NULL.
	 *
	 * @param[in] end The end point of the selection. Not NULL. May be
	 * earlier in the document than start.
	 *
	 * @param[in] end_is_focus If TRUE, end will be made focus point and
	 * start will be anchor point. If FALSE, then it will be the opposite.
	 *
	 * @returns OpStatus::OK if operation completed successfully and
	 * an error code otherwise. OpStatus::ERR_NO_MEMORY if OOM.
	 */
	OP_STATUS			DOMSetSelection(const SelectionBoundaryPoint* start, const SelectionBoundaryPoint* end, BOOL end_is_focus = FALSE);

	/**
	 * Sets a new selection replacing the current selection.
	 *
	 * @param[in] start The start point of the selection. Not NULL.
	 *
	 * @param[in] end The end point of the selection. Not NULL. May be
	 * earlier in the document than start.
	 *
	 * @param[in] end_is_focus If TRUE, end will be made focus point and
	 * start will be anchor point. If FALSE, then it will be the opposite.
	 *
	 * @param[in] remember_old_wanted_x_position When moving a selection/caret up and down we want to remember
	 * the column the user is moving in so that we can stay in it even if we temporarily lands on a
	 * short (shorter than the wanted X position) line. On the other hand, for any other selection move,
	 * we should forget that information. Use FALSE here to reset the "wanted X" to the current position, and
	 * TRUE to remember the old value. This argument is only relevant if this is the document's official TextSelection
	 * object. Otherwise the argument is ignored.
	 *
	 * @returns OpStatus::OK if operation completed successfully and
	 * an error code otherwise. OpStatus::ERR_NO_MEMORY if OOM.
	 */
	OP_STATUS			SetSelection(const SelectionBoundaryPoint* start, const SelectionBoundaryPoint* end, BOOL end_is_focus = FALSE, BOOL remember_old_wanted_x_position = FALSE);
#endif // !HAS_NOTEXTSELECTION

#ifndef HAS_NO_SEARCHTEXT

	/**
	 * Remove (only) search hits in current frame and child frames.
	 */
	void		RemoveSearchHits();

# ifdef SEARCH_MATCHES_ALL
	/**
	 * Either starts a new search or continues a current search
	 * depending on if the data object has the "is new search" flag
	 * set or not. The caller is responsible for keeping that flag
	 * updated as wanted.
	 *
	 * The search will be done in all frames in parallell.
	 *
	 * The core external API to this
	 * functionality is Window::HighlightNextMatch.
	 *
	 * @param[in] data The search information. Should be reused
	 * between calls to this method or search will restart.
	 * @param[out] rect Returns the rectangle of the found element in
	 * document coordinates
	 *
	 * @returns OpBoolean::IS_TRUE if a next match was
	 * found. OpBoolean::IS_FALSE if there was no next match.
	 *
	 * @todo Make multi frame searches much better.
	 */
	OP_BOOLEAN		HighlightNextMatch(SearchData *data, OpRect& rect);

# endif // SEARCH_MATCHES_ALL
	/**
	 * Used to search a document. This changes the current search
	 * hit. The active sub document will be searched, or if no such
	 * document exists, this will be searched.
	 *
	 * @param[in] txt The text to search for. Not NULL.
	 *
	 * @param[in] len The length of the text. Larger than 0.
	 *
	 * @param[in] forward If TRUE searches forward, if FALSE searches
	 * backwards from the current search hit.
	 *
	 * @param[in] match_case If TRUE requires matching case, if FALSE
	 * the search is case insensitive.
	 *
	 * @param[in] words Match whole words if TRUE, match partial words
	 * as well.
	 *
	 * @param[in] next If set to TRUE will continue searching after
	 * first letter of the current search hit. If FALSE the current
	 * search hit will be considered once again.
	 *
	 * @param[in] wrap If TRUE, the search will restart from the
	 * beginning/end of the document in case there was no hit from the
	 * first search and the result of a search from that position will
	 * be returned.
	 *
	 * @param[in] only_links If TRUE only text inside links will be
	 * considered. FALSE (the default) searches all text.
	 *
	 * @param[out] left_x If a hit was found, this will contain the
	 * left edge of the visible area of the hit in document
	 * coordinates.
	 *
	 * @param[out] top_y If a hit was found, this will contain the
	 * top edge of the visible area of the hit in document
	 * coordinates.
	 *
	 * @param[out] right_x If a hit was found, this will contain the
	 * document coordinate for the right edge of the visible area of the hit.
	 *
	 * @param[out] right_x If a hit was found, this will contain the
	 * document coordinate for the bottom edge of the visible area of
	 * the hit.
	 *
	 * @returns OpBoolean::IS_TRUE if something was found,
	 * OpBoolean::IS_FALSE if search was completed but nothing was
	 * found and OpStatus::ERR_NO_MEMORY if the seach encountered an
	 * OOM situation.
	 *
	 * @see SearchTextObject in the layout module which does the
	 * actual search.
	 *
	 * @see HTML_Document::SearchText()
	 */
	OP_BOOLEAN		SearchText(const uni_char* txt, int len, BOOL forward, BOOL match_case, BOOL words, BOOL next, BOOL wrap, int &left_x, long &top_y, int &right_x, long &bottom_y, BOOL only_links = FALSE);
#endif // !HAS_NO_SEARCHTEXT

	/**
	 * Called before any text change to give the FramesDocument a chance to protect
	 * its contents against HE_TEXTGROUP/HE_TEXT changes.
	 *
	 * @param text_node The text or text group element that is about to change.
	 */
	void			BeforeTextDataChange(HTML_Element* text_node);

	/**
	 * Called if a text change signalled with BeforeTextDataChange() couldn't be completed.
	 *
	 * @param text_node The text or text group element that was about to change.
	 */
	void			AbortedTextDataChange(HTML_Element* text_node);

	/**
	 * Tell the HTML_Document that the text element changed in the specified way.
	 * This is preceded by a call to BeforeTextDataChange.
	 *
	 * @param text_element The HTML_Element that just changed.
	 *
	 * @see HTML_Element::SetText() for the documentation of the other arguments.
	 */
	void			TextDataChange(HTML_Element* text_element, HTML_Element::ValueModificationType modification_type, unsigned offset, unsigned length1, unsigned length2);

	/**
	 * Internal docxs function that must be called when the parser or something else causes
	 * a HE_TEXT element to be converted to a HE_TEXTGROUP element.
	 *
	 * @param elm The element that used to be an HE_TEXT but is now an HE_TEXTGROUP.
	 *
	 * @see HTML_Element::AppendText().
	 */
	void			OnTextConvertedToTextGroup(HTML_Element* elm);

#ifdef NEARBY_INTERACTIVE_ITEM_DETECTION
	/**
	 * Finds the interactive items that are near to the given rectangle in this doc.
	 * Recurses into sub documents. See FEATURE_NEARBY_INTERACTIVE_ITEM_DETECTION.
	 * @param rect The rectangle to find items within, in this document
	 *			   coordinates. Must be fully inside the part of the visual viewport
	 *			   of this doc that is visible on the physical screen.
	 * @param[out] list A list to store the information about the interactive items in.
	 *					The list is passed into sub docs during recursive calls.
	 *					After gathering InteractiveItemInfo objects for this doc,
	 *					their rects will be adjusted by doc_pos.
	 *					The caller is responsible for the list cleanup regardless of the call result.
	 * @param doc_pos affine pos that is a translation/transform to the top document's root coordinates
	 *				  (or to the top left of the top-leftmost frame).
	 *				  It is computed before each call. Depends on a document's frame AffinePos,
	 *				  document's visual viewport position and parent doc AffinePos.
	 * @returns OpStatus::OK
	 *			OpStatus::ERR_NO_MEMORY in case of OOM.
	 */
	OP_STATUS			FindNearbyInteractiveItems(const OpRect& rect, List<InteractiveItemInfo>& list, const AffinePos& doc_pos);

	/**
	 * Finds links that are near to the given rectangle in this doc.
	 * Recurses into sub documents. See FEATURE_NEARBY_INTERACTIVE_ITEM_DETECTION.
	 * @param rect The rectangle to find link within, in this document
	 *			   coordinates. Must be fully inside the part of the visual viewport
	 *			   of this doc that is visible on the physical screen.
	 * @param[out] list A list to store pointers of the HTML_Elements that
	 *					represent the links. The list is passed into sub docs
	 *					during recursive calls. The caller does not gain
	 *					ownership of the pointers in the list.
	 * @returns OpStatus::OK
	 *			OpStatus::ERR_NO_MEMORY in case of OOM.
	 */
	OP_STATUS			FindNearbyLinks(const OpRect& rect, OpVector<HTML_Element>& list);
#endif // NEARBY_INTERACTIVE_ITEM_DETECTION

	/**
	 * Highlights the next element of a specified type, looking either
	 * forward or backwards. It highlights ranges of HTML_ElementTypes
	 * which is seldom very useful except for one special case, HE_H1
	 * to HE_H6. This method looks in the active document and will
	 * wrap if nothing is found from the current element.
	 *
	 * @param lower_tag The lowest HTML_ElementType value to include.
	 *
	 * @param upper_tag The highest HTML_ElementType value to include.
	 *
	 * @param forward TRUE if we should look forward, FALSE if we
	 * should look backwards.
	 *
	 * @param all_anchors If TRUE anything that can link or act as an
	 * HE_A shoulid be included, that is also HE_AREA.
	 *
	 * @returns TRUE if an element was selected, FALSE if nothing was
	 * found and the selection cleared.
	 *
	 * @see HTML_Document::HighlightNextElm
	 *
	 * @todo Namespace checking, or is this HTML-only and the code
	 * fixed to handle that?
	 */
	BOOL			HighlightNextElm(HTML_ElementType lower_tag, HTML_ElementType upper_tag, BOOL forward, BOOL all_anchors = FALSE);


#if defined KEYBOARD_SELECTION_SUPPORT || defined DOCUMENT_EDIT_SUPPORT
	/**
	 * Paints a caret if applicable. Used during document editing and
	 * in keyboard selection mode.
	 *
	 * @param vd The VisualDevice to draw upon.
	 */
	void			PaintCaret(VisualDevice* vd);
#endif // KEYBOARD_SELECTION_SUPPORT || DOCUMENT_EDIT_SUPPORT

#ifdef KEYBOARD_SELECTION_SUPPORT
	/**
	 * Turn on/off keyboard selection mode for the document.
	 *
	 * @param on If TRUE, turn editable mode on, otherwise turn it off.
	 * @return OpStatus::OK, OpStatus::ERR or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS			SetKeyboardSelectable(BOOL on);

	/**
	 * Command the keyboard caret to move in a specified direction, optionally
	 * also selecting while doing so.
	 *
	 * @param direction The direction to move. See the CaretMovementDirection
	 * values for more information.
	 *
	 * @param in_selection_mode If TRUE we will select in the direction (which may
	 * grow or shring an existing selection.
	 *
	 * If the caret can not move in the specified direction, nothing will happen.
	 */
	void			MoveCaret(OpWindowCommander::CaretMovementDirection direction, BOOL in_selection_mode);

	/**
	 * Get the current caret rect.
	 */
	OP_STATUS			GetCaretRect(OpRect &rect);

	/**
	 * Sets the current Keyboard Selection Mode flag. Doesn't
	 * (currently) do anything except setting the flag.
	 *
	 * @param value The new state.
	 */
	void           SetKeyboardSelectionMode(BOOL value);
#endif // KEYBOARD_SELECTION_SUPPORT

#if defined DOCUMENT_EDIT_SUPPORT || defined KEYBOARD_SELECTION_SUPPORT
	/**
	 * Gets the current Keyboard Selection Mode flag.
	 *
	 * @return TRUE if keyboard selection mode is enabled.
	 */
	BOOL           GetKeyboardSelectionMode()
	{
#ifdef KEYBOARD_SELECTION_SUPPORT
		return keyboard_selection_mode_enabled;
#else
		return FALSE;
#endif // KEYBOARD_SELECTION_SUPPORT
	}
#endif // DOCUMENT_EDIT_SUPPORT || KEYBOARD_SELECTION_SUPPORT

	/**
	 * Returns the url of the currently selected element in the
	 * currently active frame document.
	 *
	 * @param[out] win_name Out parameter that will contain the target
	 * for the url if an url was found.
	 *
	 * @param[in] unused_parameter This used to return a sub_wid maybe
	 * and was an int&, but is now unused and should not be included.
	 *
	 * @returns An url that is empty if there was no url on the
	 * current element.
	 */
	URL			GetCurrentURL(const uni_char* &win_name, int unused_parameter = 0);

	/**
	 * Makes this document the current document (or not the current
	 * document). This does lots of things to neutralize and archive
	 * the current document, or reactivate and restore it.
	 *
	 * @param state If the document should be made current (TRUE) or
	 * moved to history/oblivion (FALSE).
	 * @param visible_if_current Used by the frame activation code
	 * to propate wether the frames should be set to visible or not.
	 */
	OP_STATUS       SetAsCurrentDoc(BOOL state, BOOL visible_if_current = TRUE);

	/**
	 * Returns the element that is the one that can be considered
	 * having "focus" the most. This might be any element currently
	 * highlighted.
	 *
	 * @returns An HTML_Element or NULL if there was no element that
	 * could be considered having focus.
	 *
	 * @see HTML_Element::GetFocusedElement()
	 *
	 * @todo Unused?
	 */
    HTML_Element*	GetFocusElement();

	/**
	 * Returns if the element can be considered having focus from
	 * CSS's (and some others) point of view. Returning TRUE here will
	 * apply the :focus pseudo class and trigger thick borders on
	 * HE_BUTTONs for instance.
	 *
	 * @param[in] element The element to check.
	 */
	BOOL            ElementHasFocus( HTML_Element *element );

	/**
	 * Returns the next URL to visit in case the current url has the
	 * specified id, or an empty URL otherwise. This has to do with
	 * META refresh.
	 *
	 * @param[in] id the id that's supposed to be the current url id. If
	 * it's not this id the method will only return an empty URL.
	 *
	 * @returns The URL that can be empty if there was no such URL.
	 */
	URL				GetRefreshURL(URL_ID id);

	/**
	 * Returns the URL that is most relevant for Viewing Source.
	 *
	 * <p>This method is only used by OpBrowserView::OnInputAction in
	 * Quick and it should rally be moved to FramesDocument and then
	 * exposed through some more suitable external interface.
	 *
	 * @returns An URL that might be empty.
	 *
	 * @todo Move to FramesDocument and expose through an official api
	 * or at least make it an official module API through
	 * module.export so that other platforms don't need to have it.
	 */
	URL			GetURLForViewSource();

	//OP_STATUS	GetSearchUrl(HTML_Element* form_elm, OpString& result);

#ifdef IMAGE_TURBO_MODE

	/**
	 * Internal method. Only to be called from the doc module.
	 *
	 *  Will decode as many more images as possible until the
	 *  memorycache is full.
	 */
	void			DecodeRemainingImages();
#endif

	/**
	 * Invalidates the screen area for repainting the image in
	 * |hle|. This is typically used when animating images. We use
	 * this rather than MarkDirty on the image element to minimize the
	 * amount of work done.
	 *
	 * This call will do nothing if we detect that the previous animation
	 * step did not result in painting the element.
	 *
	 * @param[in] hle The image that needs repainting.
	 */
	void			UpdateAnimatedRect(HEListElm* hle);

	/**
	 * Returns TRUE if all visible images in this document and any of
	 * its child documents have been decoded.
	 *
	 * @returns TRUE or FALSE.
	 */
	BOOL			ImagesDecoded();

	/**
	 * This handles a rel part of an url (the part after the hash sign
	 * (#) and can be changed several times for a document. The
	 * rel_name is used for different things in different documents,
	 * for HTML we scroll to a named anchor with the same name, in SVG
	 * it describes, or modifies, what and how the SVG is shown.
	 *
	 * For HTML, see HTML5 6.11.8 Navigating to a fragment identifier
	 *
	 * @param[in] rel_name A string, not NULL but may be empty.
	 *
	 * @param alternative_rel_name If rel_name is not found, try this
	 * string instead. By HTML5 it will only find elements of
	 * type HE_A with the correct name attribute. May be NULL.
	 *
	 * @param[in] scroll A boolean, TRUE to scroll and FALSE to not
	 * scroll. Default value is TRUE.
	 *
	 * @returns OpStatus::OK if operation completed successfully and
	 * an error code otherwise. OpStatus::ERR_NO_MEMORY if OOM.
	 *
	 * @see DocumentManager::JumpToRelativePosition
	 */
	OP_STATUS 		SetRelativePos(const uni_char* rel_name, const uni_char* alternative_rel_name, BOOL scroll = TRUE);

	/**
	 * Iterates through the children and calls
	 * FramesDocElm::GetNamedSubDocWithId on the frames and iframes.
	 *
	 * @param[in] id The name of the window.
	 *
	 * @see FramesDocElm::GetNamedSubDocWithId
	 *
	 * @returns NULL if nothing found.
	 */
	FramesDocument*	GetSubDocWithId(int id);

	/**
	 * Iterates through the children and calls
	 * FramesDocElm::GetNamedSubWinId on the frames and iframes.
	 *
	 * @param[in] win_name The name of the window.
	 *
	 * @see FramesDocElm::GetNamedSubWinId()
	 *
	 * @todo Move to dochand.
	 */
    int				GetNamedSubWinId(const uni_char* win_name);

	/**
	 * Most FramesDocuments owns an HTML_Document that takes care of
	 * certain things in a document parsed from a HTML document or
	 * something similar (XML/WML/...). This returns a pointer to that
	 * object.
	 *
	 * <p>There is no good definition on what ends up in HTML_Document
	 * and what ends up in FramesDocument and in a future design these
	 * two objects will probably merge.
	 *
	 * @returns A pointer to an HTML_Document or NULL if there was no
	 * such object right now.
	 */
	HTML_Document*	GetHtmlDocument() { return doc; }

	/**
	 * @deprecated Use the non virtual, inlined accessor method
	 * GetHtmlDocument.
	 *
	 * @see GetHtmlDocument()
	 *
	 * @todo Remove as soon as all callers are fixed.
	 */
	DEPRECATED(virtual HTML_Document* GetHTML_Document());

	/**
	 * Check if this document has frames. If this returns TRUE then
	 * it's meaningful to iteratre through the children. If it returns
	 * FALSE there may still be iframes.
	 *
	 * @returns TRUE if this document has frames (and possibly
	 * subdocuments) from the HTML frameset element.
	 *
	 * @see IsFramesEnabledDoc()
	 */
	BOOL			IsFrameDoc() { return frm_root != 0; };

	/**
	 * Check if this document should be a document with frames, even
	 * if it currently isn't.
	 *
	 * @returns TRUE if this document should be a document with
	 * frames, FALSE if there are no such indications.
	 *
	 * @see IsFrameDoc()
	 */
	BOOL			IsFramesEnabledDoc() { return logdoc && logdoc->GetHtmlDocType() == HTML_DOC_FRAMES; };

	/**
	 * In a frameset typically one of the frames is active. This
	 * returns that document or NULL if there is none (in which case
	 * the top document can be considered active in a sense. This
	 * method only works if this is the top document (as in
	 * GetTopFramesDoc).
	 *
	 * @return NULL of the FramesDocument in the document tree that is
	 * currently active.
	 *
	 * @todo Maybe let it fallback to itself if that simplifies code
	 * that uses this method.
	 */
	FramesDocument*	GetActiveSubDoc();

	/**
	 * Every frameset has the concept of an "active" frame, the frame
	 * that will receive events and actions. This sets frm to be the
	 * currently active frame.
	 *
	 * @param[in] frm The frame to make active. NULL means the main document was activated.
	 * @param[in] notify_change - if TRUE (default) other documents are notified
	 * @param[in] reason The reason for setting the frame as active. Only relevant when
	 * notify_focus_change is TRUE (the default).
	 * @see FOCUS_REASON
	 *
	 * @todo Make non-virtual.
	 */
	virtual void	SetActiveFrame(FramesDocElm* frm, BOOL notify_focus_change = TRUE, FOCUS_REASON reason = FOCUS_REASON_ACTIVATE);

	/**
	 * Make the next frame in the frame order the active frame, or if
	 * back is TRUE, the previous frame. Used by users to move "focus"
	 * between different frames in a frameset.
	 *
	 * @param[in] back If TRUE we use "previous" frame instead of
	 * "next".
	 */
	void			SetNextActiveFrame(BOOL back);

	/**
	 * Make no frame be the active frame.
	 *
	 * @param[in] reason The reason of disactivating all the frames.
	 * @see FOCUS_REASON
	 */
	void			SetNoActiveFrame(FOCUS_REASON reason = FOCUS_REASON_ACTIVATE);

	/**
	 * Checks if a certain document is part of the document tree.
	 *
	 * @param[in] sub_doc The FramesDocument you're looking for.
	 *
	 * @returns TRUE if sub_doc is a FramesDocument in the document
	 * tree where this FramesDocument is root.
	 */
	BOOL			HasSubDoc(FramesDocument* sub_doc);

	/**
	 * Returns the current image loading info for this document and all sub documents. It will not clear the info struct so it has to be empty when the method is called or the original values will be included in the result.
	 *
	 * @param[in,out] info The struct that will be filled with info.
	 */
	void			GetImageLoadingInfo(ImageLoadingInfo& info);

	/**
	 * Fetches current loading progress information for this
	 * document. This method is only kept for backwards compatibility
	 * and you should really call the method in FramesDocument.
	 *
	 * @param info A DocLoadingInfo that will be added to. It should
	 * initally be empty or the result will be wrong (combined with
	 * the previous numbers)
	 */
	void			GetDocLoadingInfo(DocLoadingInfo& info);

	/**
	 * Starts the refresh timer (or may even load directly) if there
	 * is a meta refresh in the document.
	 *
	 * @returns OpStatus::OK (quite meaningless, yes).
	 */
	OP_STATUS       CheckRefresh();

	/**
	 * Returns the background on the body in the currently active sub
	 * document.
	 *
	 * @see HTML_Document::GetBGImageURL()
	 */
	URL	 	GetBGImageURL();

	/**
	 * Use this method to tell the top document that a sub document
	 * (FramesDocElm) has been deleted so that it can clear pointers
	 * to it.
	 *
	 * @param[in] The FramesDocElm that was just deleted.
	 */
	void			FramesDocElmDeleted(FramesDocElm* fde);

	/**
	 * Traverses the document tree and updates all links with the
	 * :visited pseudo class. This is potentially quite slow.
	 *
	 * @todo Rebuild this system. Consider 100 tabs with heavy
	 * document in them and think about the lag this will cause.
	 */
	void			UpdateLinkVisited();

	/**
	 * Calculcates what window to use for loading a document when a
	 * document has specified a certain target. Input is win_name, and
	 * output is win_name and sub_wid. If win_name is NULL after the
	 * call, then the window with sub window id sub_wid should be
	 * used, unless sub_wid is -1 in which case we should load in the
	 * top window or -2 in case we should open a new window.
	 *
	 * @param[in] win_name Input and output. Can be NULL and if it's
	 * NULL after the call, the sub_wid signals which window to
	 * use. If it's non-NULL after the call, no window by that name
	 * was found.
	 *
	 * @param[in] sub_wid Output if win_name is NULL after the
	 * call. See function description.
	 */
	void 			FindTarget(const uni_char* &win_name, int &sub_win_id);

#ifdef SHORTCUT_ICON_SUPPORT
	/**
	 * Starts loading the favicon.
	 *
	 * @param[in] icon_url The URL of NULL. If NULL, nothing will be
	 * done.
	 *
	 * @param[in] link_he The element that explicitly requested this
	 * favicon or the docroot if we are loading this without any
	 * explicit requests.
	 *
	 * @returns OpBoolean::IS_TRUE if we have started a load that
	 * isn't yet finished. OpBoolean::IS_FALSE if the load was
	 * completed successfully or we didn't want to load the favicon at
	 * all. An error code if there was OOM or similar problems.
	 */
	OP_BOOLEAN		LoadIcon(URL* icon_url, HTML_Element *link_he);

	/**
	 * Starts loading the favicon after the HEAD element has been parsed if not specified already.
	 */
	OP_BOOLEAN		DoASpeculativeFavIconLoad();
#endif

	/**
	 * Returns the LogicalDocument's HLDocProfile or NULL if there's
	 * no LogicalDocument.
	 *
	 * @returns An HLDocProfile or NULL.
	 */
	HLDocProfile*	GetHLDocProfile() {	return logdoc ? logdoc->GetHLDocProfile() : NULL; }

	/**
	 * Used to clean the script environment after reloading or walking
	 * in history. Will delete the DOM_Environment if
	 * delete_es_runtime is TRUE.
	 *
	 * @todo Document.
	 */
	void			CleanESEnvironment(BOOL delete_es_runtime);

	/**
	 * Set a global flag indicating that we should (or shouldn't)
	 * always create DOM environments in new documents, whether there
	 * seem to be any scripts in them or not.
	 *
	 * Used by the scope debugger to see and handle documents that
	 * have no scripts in them.
	 */
	static void SetAlwaysCreateDOMEnvironment(BOOL value);

	/**
	 * Not really sure what it does, except that it shouldn't be
	 * called by someone not knowing what it does.
	 *
	 * <p>It seems to be related to resetting some event information
	 * (the onload event) and to be used when a script environment is
	 * restarted.
	 *
	 * @param[in] delete_es_runtime If TRUE resets onload information.
	 *
	 * @param[in] force If TRUE creates a DOM_Environment even if there is
	 * no document contents.
	 *
	 * @todo document
	 */
	OP_STATUS       CreateESEnvironment(BOOL delete_es_runtime, BOOL force = FALSE);

	/**
	 * By default a document has no scripting environment which is a
	 * good thing since scripting consumes loads of memory. Often we
	 * do need one anyway (scripts in HTML documents, user javascript,
	 * event handling, ...) and then it's created with this method.
	 *
	 * @returns OpStatus::OK if the environment was created
	 * successfully. OpStatus::ERR_NO_MEMORY of we encountered an OOM
	 * situation and OpStatus::ERR if it couldn't be created for any
	 * other reason, for instance scripts forbidden is this document.
	 */
	OP_STATUS			ConstructDOMEnvironment();

	/**
	 * Everything related to the DOM environment is handled through
	 * the DOM_Environment API owned by the FramesDocument. This
	 * returns that DOM_Environment or NULL if it's not created. Use
	 * ConstructDOMEnvironment if you need one.
	 *
	 * @returns The DOM environment or NULL.
	 *
	 * @see DOM_Environment
	 *
	 * @see ConstructDOMEnvironment();
	 */
	DOM_Environment*	GetDOMEnvironment() { return dom_environment; }

	/**
	 * In the DOM there is an object called window that also acts as
	 * the global object (always in scope).
	 *
	 * @returns NULL or the window object.
	 */
	DOM_Object*			GetJSWindow() { return js_window; }

	/**
	 * In the DOM there is an object called document and this method
	 * returns that if it has been created.
	 *
	 * @returns NULL or the same as
	 * GetDOMEnvironment()->GetDOMDocument()
	 */
	DOM_Object*			GetDOMDocument();

	/**
	 * Scripts run in a runtime. This is that runtime.
	 *
	 * @returns The ES_Runtime or NULL if no DOM environment has been
	 * created yet.
	 */
	ES_Runtime*			GetESRuntime() { return es_runtime; }

#ifdef ECMASCRIPT_DEBUGGER
	/**
	 * A FramesDocument may have multiple ES_Runtimes associated with it
	 * (because of extensions). This function returns *all* ES_Runtimes
	 * associated with the FramesDocument (if any).
	 *
	 * @param [out] es_runtimes A vector to store the ES_Runtimes in.
	 * @return OpStatus::OK on success, otherwise OOM.
	 */
	OP_STATUS			GetESRuntimes(OpVector<ES_Runtime> &es_runtimes);
#endif // ECMASCRIPT_DEBUGGER

	/**
	 * The ES_ThreadScheduler is responsible for keeping track of
	 * scripts to run in the document and keeping track of
	 * dependencies between different script "threads" (every event
	 * creates a new thread and threads can spawn blocking new
	 * threads).
	 *
	 * @returns The ES_ThreadScheduler or NULL if no DOM environment
	 * has yet been created.
	 */
	ES_ThreadScheduler*	GetESScheduler() { return es_scheduler; }

	/**
	 * The ES_AsyncInterface is an API for running scripts in a
	 * document. It exists if there is a dom environment in the
	 * document (call ConstructDOMEnvironment to ensure there is one
	 * if it's important).
	 *
	 * @returns The ES_AsyncInterface or NULL.
	 *
	 * @see ES_AsyncInterface
	 */
	ES_AsyncInterface*	GetESAsyncInterface() { return es_async_interface; }

#if defined DOC_EXECUTEES_SUPPORT || defined HAS_ATVEF_SUPPORT
	/**
	 * Compiles and executes a script in the context of this document.
	 * If more control over the operation is needed, please use
	 * ConstructDOMEnvironment() to ensure the scripting environment
	 * exists and then use the ES_AsyncInterface object returned by
	 * GetESAsyncInterface() to execute the script.
	 *
	 * @param script Source of script to execute.
	 */
	OP_STATUS       ExecuteES(const uni_char* script);
#endif // DOC_EXECUTEES_SUPPORT || HAS_ATVEF_SUPPORT

	/**
	 * Registers terminating actions in all child documents.
	 *
	 * @param[in] thread Not really sure.
	 *
	 * @returns OpStatus::OK if everything is ok, an error code
	 * otherwise.
	 */
	OP_STATUS		ESTerminate(ES_TerminatingThread *thread);

	/**
	 * There are various caches in opera that depends on the document
	 * structure. To know when those are out of date we keep a serial
	 * number on the document and if the serial number of the document
	 * doesn't coincide with the serial number of the cache it has to
	 * be rebuilt. This method returns the current serial number of
	 * the tree.
	 *
	 * @returns The current serial number.
	 */
	unsigned        GetSerialNr() { return serial_nr; }

	/**
	 * There are various caches in opera that depends on the document
	 * structure. To know when those are out of date we keep a serial
	 * number on the document and if the serial number of the document
	 * doesn't coincide with the serial number of the cache it has to
	 * be rebuilt. This method increases the serial number in response
	 * to a change to the tree.
	 */
	void            IncreaseSerialNr() { ++serial_nr; }

	/**
	 * When restoring form data we try to restore them in the order
	 * written by the user in case they have dependencies on eachother
	 * (typical case is select boxes that are prefilled by scripts
	 * depending on other select boxas). This is the keeper of the
	 * ever increasing serial number that identifies the order.
	 *
	 * @return The serial number, always higher than the last call.
	 */
	unsigned		GetNewFormValueSerialNr() { return ++form_value_serial_nr; }

	/**
	 * This method does what have to be done in response to a call to
	 * the DOM method document.open(). Blocks the current thread in
	 * case it can't finish immediately.
	 *
	 * @param origining_runtime The runtime that had the
	 * document.open() call.
	 *
	 * @param[in] url The url that is being opened. NULL to use the url of the origining_runtime.
	 *
	 * @param[in] is_reload TRUE if this is a reload of the current
	 * document.
	 *
	 * @param[in] content_type The content type that was specified in
	 * the document.open call. May be NULL in which case text/html is used.
	 *
	 * @param[out] opened_doc Set if a new document was generated. NULL to ignore.
	 *
	 * @param[out] doc_exception Set if an error that ought to trigger an
	 * ES exception occured.
	 *
	 * @returns OpStatus::OK if the operation completed successfully
	 * or an error code otherwise.
	 */
	OP_STATUS       ESOpen(ES_Runtime *origining_runtime, const URL *url, BOOL is_reload, const uni_char *content_type, FramesDocument **opened_doc, ESDocException *doc_exception);

	/**
	 * This method does what have to be done in response to a call to
	 * the DOM method document.write().
	 *
	 * @param origining_runtime The runtime that ran the script that
	 * had the document.write() call.
	 *
	 * @param[in] string the data written by the script.
	 *
	 * @param[in] string_length Length of the data in uni_chars
	 *
	 * @param[in] end_with_newline TRUE if this is really writeln and
	 * a newline should be added as well as the string.
	 *
	 * @param[out] doc_exception Set if an error that ought to trigger an
	 * ES exception occured.
	 *
	 * @return OpBoolean::IS_FALSE if the data couldn't be handled at
	 * this time. Suspend the thread and try again. OpBoolean::IS_TRUE
	 * if the operation proceeded successfully and an error code
	 * otherwise.
	 */
	OP_BOOLEAN      ESWrite(ES_Runtime *origining_runtime, const uni_char* string, unsigned string_length, BOOL end_with_newline, ESDocException *doc_exception);

	/**
	 * This method does what have to be done in response to a call to
	 * the DOM method document.close().
	 *
	 * @param origining_runtime The runtime that had the
	 * document.close() call.
	 *
	 * @returns OpStatus::OK if the operation completed successfully
	 * or an error code otherwise.
	 */
	OP_STATUS       ESClose(ES_Runtime *origining_runtime);

	/**
	 * Connects this (the document to be generated) with the document
	 * that is generating it. Should be called together with
	 * ESSetReplacedDocument.
	 *
	 * @param[in] doc The other document.
	 */
	void			ESSetGeneratedDocument(FramesDocument *doc);

	/**
	 * Connects this (the document that is generating another
	 * document) with the document that it is generating. Should be
	 * called together with ESSetGeneratedDocument.
	 *
	 * @param[in] doc The other document.
	 */
	void			ESSetReplacedDocument(FramesDocument *doc);

	/**
	 * Returns TRUE if this document is currently being generated by
	 * scripts via document.open/document.write.  Once document.close
	 * is called (if ever,) FALSE will be returned.
	 *
	 * @return TRUE or FALSE.
	 */
	BOOL			ESIsBeingGenerated();

	/**
	 * This is called when a thread is about to start to produce data
	 * for this document. It initialized some parts of it so that it's
	 * ready to receive data.
	 *
	 * @param[in] generating_thread The thread that will produce the data.
	 *
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS		ESStartGeneratingDocument(ES_Thread *generating_thread);

	/**
	 * This should be called in the generated document when the
	 * generating document is finished. It will break the connection
	 * between the two documents which will ensure that we for
	 * instance don't run any more scripts in the other document.
	 */
	void			ESStoppedGeneratingDocument();


	/**
	 * Called when there are no longer any threads generating this
	 * document so that it
	 * can do the necessary cleanup.
	 */
	void			OnESGenerationIdle() { ESStoppedGeneratingDocument(); }

	/**
	 * This will finish all scripts and post relevant events and then
	 * open the url "normally". This is not typically used
	 * externally. If you're not familiar with this code you want to
	 * call DocumentManager::OpenURL() instead of this method.
	 *
	 * @param[in] url The url to load.
	 *
	 * @param[in] ref_url The source document of the url request. So
	 * that we can block file access from non-file documents for
	 * instance, and send the correct headers in an HTTP request.
	 *
	 * @param[in] check_if_expired If FALSE, there will not be a
	 * Check-If-Modified HTTP request. Otherwise there might be
	 * depending on other circiumstances.
	 *
	 * @param[in] reload If TRUE, then this is a reload of the current
	 * document.
	 *
	 * @param[in] replace If TRUE the new document will replace the
	 * current document rather than following it in history. Used for
	 * certain types of redirects.
	 *
	 * @param[in] options See DocumentManager::OpenURLOptions
	 *
	 * user_initiated If TRUE this was triggered directly
	 * by the user (pressing |enter| or clicking on something
	 * typically). If FALSE, then this was triggered in a way that was
	 * not the direct response from a user action (for instance script
	 * timout, mousemove...)
	 *
	 * entered_by_user If the user himself entered the url (either
	 * by typing it or activating a previously saved bookmark). In that
	 * case we will let the url do a little more, assuming the user knows
	 * what he is doing.
	 *
	 * is_walking_in_history If TRUE we load this document
	 * because we have moved back or forward to it in history and we
	 * had not the document in the document cache.
	 *
	 * from_html_attribute If TRUE this is called due to an
	 * attribute changed for example the src attribute of an iframe
	 * element.
	 *
	 * @param[in] origin_thread The thread that caused this to happen.
	 *
	 * @param[in] is_user_requested If the user himself entered the
	 * url this should be TRUE, otherwise FALSE.
	 *
	 * @param[in] open_in_new_window If this results in a new
	 * document, it will open in a new window if this is TRUE.
	 *
	 * @param[in] open_in_background If this results in a new window,
	 * TRUE here will open that window in the background, FALSE will
	 * open it in front of the current window.
	 *
	 * @param[in] user_auto_reload If this is TRUE, this is a reload
	 * triggered by the auto-reload feature. It might be blocked if
	 * it's likely that it will destroy information for the user, such
	 * as data written into a form.
	 *
	 * @return OpStatus::OK if the operation was initiated
	 * successfully. Since the full execution is asynchronous it may
	 * still fail later. OpStatus::ERR_NO_MEMORY if we run out of
	 * memory. For javascript: urls OpStatus::ERR will be returned
	 * if the script wasn't started for some reason. This does not
	 * have to be because of an error condition but it's necessary
	 * to see the difference between "waiting for script" and not.
	 * OpStatus::ERR and other error codes are also possible for other
	 * url types.
	 *
	 * @see DocumentManager::OpenURL().
	 */
	OP_STATUS		ESOpenURL(const URL &url, DocumentReferrer referrer, BOOL check_if_expired, BOOL reload, BOOL replace, const DocumentManager::OpenURLOptions &options, BOOL is_user_requested = FALSE, BOOL open_in_new_window = FALSE, BOOL open_in_background = FALSE, BOOL user_auto_reload = FALSE);

	/**
	 * Executes the javascript code in a javascript url.
	 *
	 * @param[in] The javascript url.
	 *
	 * @param[in] write_result_to_document If TRUE any return value
	 * will be written to the document, replacing the current
	 * document.
	 *
	 * @param[in] write_result_to_url Not really sure what this is,
	 * but it's only set to TRUE by the plugin code. If you don't know
	 * either it's better to leave it as FALSE.
	 *
	 * @param[in] is_reload Set to TRUE if this is run because a
	 * javascript url is reloaded.
	 *
	 * @param[in] want_utf8 If TRUE output is utf-8 encoded before
	 * being sent to the url. This is not used unless
	 * write_result_to_url is TRUE, so it too is only used by pluings.
	 *
	 * @param[in] origin_thread Used to track the origins of the load
	 * requests so that we don't forget if it should be opened in some
	 * specific way or if it was unrequested or not.
	 *
	 * @param[in] is_user_requested TRUE if this opens as a direct
	 * response to a user action (click, enter key or similar), FALSE
	 * otherwise.
	 *
	 * @param[in] open_in_new_window If TRUE, any output causing a new
	 * document will be shown in another window.
	 *
	 * @param[in] open_in_background If TRUE any window opening will
	 * open behind the current document rather than in the front of
	 * it.
	 *
	 * @param[in] entered_by_user If the user himself entered the url (either
	 * by typing it or activating a previously saved bookmark). In that
	 * case we will let the url do a little more, assuming the user knows
	 * what he is doing. It will also affect which charset is used to
	 * decode the utf-8.
	 *
	 * @return OpStatus::OK if the operation was initiated
	 * successfully. Since the full execution is asynchronous it may
	 * still fail later. OpStatus::ERR if no scripts are to be
	 * executed. OpStatus::ERR_NO_MEMORY if we run out of memory.
	 */
	OP_STATUS		ESOpenJavascriptURL(const URL &url, BOOL write_result_to_document, BOOL write_result_to_url, BOOL is_reload, BOOL want_utf8 = FALSE, ES_Thread* origin_thread = NULL, BOOL is_user_requested = FALSE, BOOL open_in_new_window = FALSE, BOOL open_in_background = FALSE, EnteredByUser entered_by_user = NotEnteredByUser);

	/**
	 * This returns TRUE if we can't load url into the current
	 * document's space without doing the es termination phase which
	 * includes sending the unload event and waiting for some scripts
	 * to finish.
	 *
	 * @param[in] url The url we might want to replace the current
	 * document with.
	 *
	 * @param[in] reload TRUE if we're replacing the current document
	 * with a reloaded version of it self.
	 *
	 * @returns TRUE if we need to run the es-termination code, FALSE
	 * if we can't just open the url directly.
	 */
	BOOL			ESNeedTermination(const URL &url, BOOL reload);

	/**
	 * Checks if all children are ready to get the onload event and in
	 * that case sets the flag that says that we are ready for the
	 * onload event. When that flag gets set on the top document (thus
	 * all documents in the document tree has been loaded), we send
	 * the onload event to all documents.
	 *
	 * @param[in] start_point_doc The point to start check or NULL if start_point_frame is set.
	 *
	 * @param[in] start_point_frame The point to start check or NULL if start_point_doc is set.
	 *
	 * @returns OpStatus::ERR_NO_MEMORY if out of memory, OpStatus::OK
	 * otherwise regardless if the onload event was sent or not.
	 */
	static OP_STATUS		CheckOnLoad(FramesDocument* start_point_doc, FramesDocElm* start_point_frame = NULL);

	/**
	 * Schedules a check of the onload status once thread's root
	 * thread has completed.
	 *
	 * @param[in] thread The thread that did something that might have caused
	 * the load status to change.
	 *
	 * @param[in] about_blank_only When the check ends up being scheduled, should
	 * it only go ahead if about:blank hasn't been navigated away from.
	 *
	 * @returns OOM or OK.
	 */
	OP_STATUS		ScheduleAsyncOnloadCheck(ES_Thread* thread, BOOL about_blank_only = FALSE);

	/**
	 * Returns TRUE iff the document or any of its subdocuments is
	 * stopped in the ECMAScript scheduler, waiting for a forced user
	 * action (like responding to an alert).
	 *
	 * @returns TRUE if there is script running, but waiting for
	 * external actions.
	 */
	BOOL			IsESStopped();

	/**
	 * This checks if there are script currently running in the
	 * document (though possibly blocked waiting for user input or
	 * reflows or incoming data and such).
	 *
	 * @param treat_ready_to_run_as_active TRUE if we should consider
	 * a thread that isn't actually running but wants to run as
	 * "active".
	 *
	 * @returns TRUE if there are currently scripts running in this
	 * document (or any descendant document if the param is TRUE).
	 */
	BOOL			IsESActive(BOOL treat_ready_to_run_as_active);

	/**
	 * A document can replace itself by calling document.open(),
	 * document.write() and then document.close() and we have to let
	 * that document complete its script or the generated document
	 * will never be completed. So for that document it will have to
	 * remain in history and execute script until finished. This is
	 * the method to identify such documents.
	 *
	 * @returns TRUE if this document is currently generating a
	 * replacement document (which will be the current document?).
	 */
	BOOL			IsESGeneratingDocument() { return es_generated_document != NULL; }

	/**
	 * @returns TRUE if the onload event has been dispatched for this document
	 *          and finished processing, FALSE otherwise.
	 */
	BOOL			HasCalledOnLoad() { return es_info.onload_called && !es_info.onload_pending; }

	/**
	 * Sets the flag controlling if onload for about:blank should be fired.
	 */
	void			SetInhibitOnLoadAboutBlank(BOOL f) { es_info.inhibit_onload_about_blank = f; }

	/**
	 * Scripts can index frames and ask how many frames there
	 * are. This method returns the index'th frame.
	 *
	 * @param[in] index The index of the frame, 0.... Should be less
	 * than the number returned from GetJSFrame().
	 *
	 * @return The FramesDocument for the child frame or NULL if there
	 * was no such (script visible) frame.
	 *
	 * @todo Rename to DOM...something according to the naming
	 * standard.
	 *
	 * @see CountJSFrames()
	 */
	FramesDocument*	GetJSFrame(int index);

	/**
	 * Scripts can index frames and ask how many frames there
	 * are. This method answers that question.
	 *
	 * @return The number of frames visible to script (>=0).
	 *
	 * @todo Rename to DOM...something according to the naming
	 * standard.
	 */
	int				CountJSFrames();

#ifdef XML_EVENTS_SUPPORT
	/**
	 * Register a XML_Events_Registration for the document. This
	 * should/is corresponging to an element in the NS_EVENT
	 * namespace.
	 *
	 * @param{in] registration The object to register. Will be
	 * informed of changes to the element tree in the document.
	 */
	void			AddXMLEventsRegistration(XML_Events_Registration *registration);

	/**
	 * To iterate over the XML_Events_Registration objects in this
	 * document, use this method to get the first (or NULL) and then
	 * use Suc() on the XML_Events_Registration objects until NULL is
	 * returned.
	 *
	 * @returns The first in the list of XML_Events_Registration
	 * objects in this document or NULL.
	 */
	XML_Events_Registration *GetFirstXMLEventsRegistration();
#endif // XML_EVENTS_SUPPORT

	/**
	 * Scripts sometimes depend on a succesful layout/reflow of the
	 * document and therefore DOM registers a listener to be called
	 * when layout is ready to be queried about layout state. Until it
	 * is called, scripts will be hung.
	 *
	 * <p>This method adds listener as a listener on the layout in
	 * this document.
	 *
	 * @param[in] listener The listener to register. A listener can
	 * only be registered in one document at a time and must live
	 * until it has been removed from the document either by getting a
	 * LayoutContinue callback or by explicitly calling
	 * RemoveDelayedLayoutListener().
	 *
	 * @see RemoveDelayedLayoutListener()
	 */
	void			AddDelayedLayoutListener(DelayedLayoutListener *listener);

	/**
	 * Scripts sometimes depend on a succesful layout/reflow of the
	 * document and therefore DOM registers a listener to be called
	 * when layout is ready to be queried about layout state. Until it
	 * is called, scripts will be hung.
	 *
	 * <p>This method removes a previously registered listener as a
	 * listener on the layout in this document.
	 *
	 * @param[in] listener The listener to unregister.
	 *
	 * @see AddDelayedLayoutListener()
	 */
	void			RemoveDelayedLayoutListener(DelayedLayoutListener *listener);

#ifdef DELAYED_SCRIPT_EXECUTION

	/**
	 * Read the current position in the stream. The number returned
	 * here can be used in SetStreamPosition to restore stream reading
	 * from the current position.
	 *
	 * @param[in] buffer_position A pointer into the current buffer so
	 * that we can know how much of the current buffer the calling
	 * method has already used.
	 *
	 * @returns An offset to be used in SetStreamPosition.
	 */
	unsigned		GetStreamPosition(const uni_char *buffer_position);

	/**
	 * Moves the read pointer in the in stream back to
	 * |new_stream_position|. This is similar to seek functionality in
	 * file API:s but can only be used to move the read pointer to an
	 * earlier position since we can't handle moving to positions not
	 * yet loaded. This is typically used when restarting parsing at
	 * an earlier position since we've determined that the previous
	 * parsing wasn't correct because delayed scripts changed base
	 * conditions.
	 *
	 * @param new_stream_position The absolute position to move the
	 * read pointer to. 0 is the start of the stream, but the value
	 * should really be a value fetched with GetStreamPosition().
	 *
	 * @returns OpStatus::OK if the operation was successful, or
	 * OpStatus::ERR if we failed in restarting the read. In that case
	 * the current read position is unknown and it's safest to just
	 * abort parsing.
	 */
	OP_STATUS		SetStreamPosition(unsigned new_stream_position);
#endif // DELAYED_SCRIPT_EXECUTION

#ifdef SPECULATIVE_PARSER
	/**
	 * Creates and setups the stream position of the data desctriptor used by
	 * the speculative parser.
	 *
	 * @param new_stream_position The absolute position to move the
	 * read pointer to. 0 is the start of the stream.
	 *
	 * @returns normal error codes.
	 */
	OP_STATUS		CreateSpeculativeDataDescriptor(unsigned new_stream_position);

	/**
	 * Get URLs that have been loaded by the speculative parser. Used to
	 * populate the array returned by opera.getSpeculativeParserUrls().
	 *
	 * @param[out] urls A vector that will be populated with the URLs.
	 *
	 * @returns Normal error codes.
	 */
	OP_STATUS GetSpeculativeParserURLs(OpVector<URL> &urls);
#endif // SPECULATIVE_PARSER

	/**
	 * Must be called when the script has done it's "onload"
	 * execution. It will trigger things like form state restoration
	 * and other things we have waited to do until after the onload
	 * handlers have run.
	 *
	 * @returns OpStatus::OK if the operation completed successfully
	 * or an error code otherwise.
	 */
	OP_STATUS		DOMSignalDocumentFinished();

	/**
	 * Related to the document.activeElement property in DOM. This
	 * returns the element that was most recently activated by the
	 * user or the body or NULL.
	 *
	 * @returns A pointer to an element in the document or NULL.
	 */
	HTML_Element*	DOMGetActiveElement();

	/**
	 * Get the element that must be used as target when sending
	 * window events (onload et al). Normally the body element
	 * but slightly more complicated in framesets.
	 * Can return NULL in which case window events can
	 * be skipped anyway.
	 */
	HTML_Element*	GetWindowEventTarget(DOM_EventType event_type);

	/**
	 * Return TRUE if window has event handlers for the given
	 * event_type, originally declared on body as attributes
	 * (e.g. <body onscroll="">)
	 */
	BOOL			HasWindowHandlerAsBodyAttr(DOM_EventType event_type);

#ifdef DOM_FULLSCREEN_MODE
	/**
	 * From the W3C fullscreen API. Maps to the requestFullscreen method.
	 * @param element            Element that is to be made the fullscreen element.
	 * @param origining_runtime  Runtime from which originated this request to exit fullscreen.
	 *                           Used for permission validation.
	 * @param no_error           If TRUE, no fullscreenerror event will be dispatched.
	 * @returns OpStatus::OK if the request succedded.
	 *          OpStatus::ERR if the request failed.
	 *          OpStatus::ERR_NO_MEMORY on oom.
	 */
	OP_STATUS DOMRequestFullscreen(HTML_Element* element, ES_Runtime* origining_runtime, BOOL no_error = FALSE);

	/**
	 * From the W3C fullscreen   API. Maps to the exitFullscreen method.
	 * @param origining_runtime  Runtime from which originated this request to exit fullscreen.
	 *                           Used for permission validation.
	 * @returns OpStatus::OK on success.
	 *          OpStatus::ERR_NO_MEMORY on oom.
	 */
	OP_STATUS DOMExitFullscreen(ES_Runtime* origining_runtime);

	/**
	 * From the W3C fullscreen API. Maps to the fullscreenEnabled property. Is recursive.
	 */
	BOOL DOMGetFullscreenEnabled();

	/**
	 * Validates if the runtime has permissions to do requests to the fullscreen API.
	 * The runtime can do requests if the current thread is user initiated, or
	 * if the current thread is from a user javascript or extension.
	 */
	static BOOL ValidateFullscreenAccess(ES_Runtime* origining_runtime);

	/**
	 * From the W3C fullscreen API. Maps to the fullscreenElement property.
	 */
	HTML_Element* GetFullscreenElement() { return fullscreen_element.GetElm(); }

	/**
	 * Set the current fullscreen element for this document.
	 *
	 * This method will also make sure the necessary style changes are
	 * applied.
	 *
	 * @param elm The element that should be the new fullscreen element.
	 *			  NULL means there will be no fullscreen element.
	 */
	void SetFullscreenElement(HTML_Element* elm);

	/**
	 * Notify that the current fullscreen element may not cover the whole
	 * rendering area or that it may have content from other elements
	 * rendered on top. This method is called from CSSCollection to notify
	 * the document when relevant fullscreen styles are overridden.
	 */
	void SetFullscreenElementObscured() { fullscreen_element_obscured = TRUE; }

	/**
	 * When the fullscreen element is set, this method tells if there might be
	 * other content than the fullscreen element itself which needs to be
	 * displayed. May be used to optimize painting of <canvas> and <video>
	 * elements in fullscreen mode.
	 *
	 * @returns FALSE if the fullscreen element guaranteed to be the only
	 *			content which needs to be displayed, TRUE otherwise.
	 */
	BOOL IsFullscreenElementObscured() const { return fullscreen_element_obscured; }

	/**
	 * Shortcut painting of certain non-obscured replaced elements.
	 * Done to get better performance for CANVAS and VIDEO.
	 *
	 * @param[in] rect The part to repaint. This is in document
	 * coordinates relative the upper left corner of the view.
	 *
	 * @param[in] vd The VisualDevice to paint on. This might be
	 * another VisualDevice than the document's normal VisualDevice.
	 *
	 * @returns DOC_DISPLAYED if the fullscreen element could do the
	 *			optimized painting. DOC_CANNOT_DISPLAY if we need to
	 *			fall back to normal paint traversal in FramesDocument::Display.
	 *			ERR_NO_MEMORY on OOM.
	 */
	OP_DOC_STATUS DisplayFullscreenElement(const RECT& rect, VisualDevice* vd);
#endif // DOM_FULLSCREEN_MODE

/**
	 * Get a marker reflecting if the tree has been invalidated by
	 * the layout engine. DOM needs this information for keeping
	 * the queryselectorcache (which depends on style information)
	 * correct.
	 *
	 * This could have been a flag on DOM_Environment but
	 * performance and ease of access put it here.
	 * The DOM code may reset
	 * the marked-since-last flag via DOMResetRecentLayoutChanges().
	 *
	 * @returns TRUE if the flag is set, FALSE otherwise.
	 */
	BOOL DOMHadRecentLayoutChanges() { return es_recent_layout_changes; }

	/**
	 * Clears the "recent_layout_changes" flag.
	 *
	 * @see DOMHadRecentLayoutChanges for documentation.
	 */
	void DOMResetRecentLayoutChanges() { es_recent_layout_changes = FALSE; }

#ifndef HAS_NOTEXTSELECTION
	/**
	 * Called when the user has changed the selection in any way to
	 * update the range in the DOM_WindowSelection object.
	 */
	void DOMOnSelectionUpdated();
#endif // HAS_NOTEXTSELECTION

	/**
	 * Tells the document that onpopstate event should be sent as soon as the document
	 * is ready (is parsed).
	 */
	void SendOnPopStateWhenReady() { es_info.onpopstate_pending = TRUE; }

	/**
	 * This changes the url of an existing FramesDocument. It mustn't
	 * be used lightly since that changes security domain and other
	 * implicit things based on the url. It should typically only be
	 * used in script context, when the url was original unknown (or
	 * about:blank).
	 *
	 * @param[in] url The new url to set. This will change the
	 * security of the document as it's based on the url.
	 *
	 * @returns OpStatus::OK if the operation completed successfully
	 * or an error code otherwise.
	 */
	OP_STATUS		SetNewUrl(const URL &url);

	/**
	 * Get layout viewport X position, in document coordinates.
	 *
	 * This is the X scroll position, as far as the layout engine is
	 * concerned.
	 *
	 * @see GetLayoutViewport()
	 */
	int			GetLayoutViewX();

	/**
	 * Get layout viewport Y position, in document coordinates.
	 *
	 * This is the Y scroll position, as far as the layout engine is
	 * concerned.
	 *
	 * @see GetLayoutViewport()
	 */
	int			GetLayoutViewY();

	/**
	 * Get layout viewport width, in document coordinates.
	 *
	 * This is the initial containing block width, as far as the layout engine
	 * is concerned.
	 *
	 * @see GetLayoutViewport()
	 */
	int			GetLayoutViewWidth();

	/**
	 * Get layout viewport height, in document coordinates.
	 *
	 * This is the initial containing block height, as far as the layout engine
	 * is concerned.
	 *
	 * @see GetLayoutViewport()
	 */
	int			GetLayoutViewHeight();

	/**
	 * Get layout viewport, in document coordinates.
	 *
	 * This is the scroll position and initial containing block size, as far as
	 * the layout engine is concerned.
	 *
	 * @see LayoutWorkplace::GetLayoutViewport()
	 */
	OpRect		GetLayoutViewport();

	/**
	 * Set new layout viewport position, in document coordinates.
	 *
	 * @see LayoutWorkplace::SetLayoutViewPos()
	 *
	 * @return The area that needed to be repainted because of the viewport
	 * position change (typically due to fixed-positioned content). Note that
	 * this area has already been scheduled for repaint by this method, but the
	 * caller may need this information to prevent the area from being
	 * scrolled.
	 */
	OpRect		SetLayoutViewPos(int x, int y);

	/**
	 * Recalculate size of layout viewport.
	 *
	 * This is necessary to do when something that may affect the layout
	 * viewport has changed, such as window or frame size, zoom factor, history
	 * navigation, etc.
	 *
	 * The document will be scheduled for reflow if the layout viewport
	 * changed.
	 *
	 * @param user_action TRUE if this was triggered by a user action (such as
	 * window resize or zoom factor change) (which may trigger onresize
	 * javascript events), FALSE otherwise.
	 *
	 * @return TRUE if the layout view size changed, otherwise FALSE.
	 */
	BOOL		RecalculateLayoutViewSize(BOOL user_action);

	/**
	 * Recalculate need for scrollbars and display / hide them.
	 *
	 * This will enable and disable scrollbars, based on the size of the
	 * document's contents and the scrollbar settings (on/off/auto). This
	 * method will NOT trigger a reflow if the scrollbar situation changes. It
	 * is up to the caller to do that, based on the return value.
	 *
	 * @param keep_existing_scrollbars If TRUE, do not remove scrollbars, even
	 * if the size of the document's contents would suggest that it would be
	 * appropriate. This is necessary during reflow loops, to make sure that we
	 * don't loop forever.
	 *
	 * @return TRUE if the scrollbar situation changed (i.e. scrollbars were
	 * added or removed), FALSE otherwise.
	 */
	BOOL		RecalculateScrollbars(BOOL keep_existing_scrollbars = FALSE);

	/**
	 * Recalculate device media queries.
	 *
	 * This is necessary to do if the screen properties have
	 * changed. The queries will only be re-evaluted if the conditions
	 * have changed.
	 */
	void		RecalculateDeviceMediaQueries();

	/**
	 * Get visual viewport, in document coordinates.
	 *
	 * This is the area that the user can currently see (with all platform-side
	 * zooming and panning taken into consideration). The user interface will
	 * change the visual viewport by calling
	 * OpViewportController::SetVisualViewport().
	 *
	 * @return Visual viewport, with coordinates relative to this
	 * document. Note that there is really only one visual viewport per window,
	 * not one per frame.
	 */
	OpRect		GetVisualViewport();

	/**
	 * Get visual viewport X position, in document coordinates.
	 *
	 * @see GetVisualViewport()
	 */
	int			GetVisualViewX() { return GetVisualViewport().x; }

	/**
	 * Get visual viewport Y position, in document coordinates.
	 *
	 * @see GetVisualViewport()
	 */
	int			GetVisualViewY() { return GetVisualViewport().y; }

	/**
	 * Get visual viewport width, in document coordinates.
	 *
	 * @see GetVisualViewport()
	 */
	int			GetVisualViewWidth() { return GetVisualViewport().width; }

	/**
	 * Get visual viewport height, in document coordinates.
	 *
	 * @see GetVisualViewport()
	 */
	int			GetVisualViewHeight() { return GetVisualViewport().height; }

	/**
	 * Request that the visual viewport be changed.
	 *
	 * This method will not change the visual viewport on its own; instead it
	 * will call OpViewportListener::OnVisualViewportChangeRequest(), and then
	 * it is up to the user interface to decide whether or not to carry out the
	 * request.
	 *
	 * @param viewport New visual viewport, in document coordinates.
	 *
	 * @param reason Reason for change request.
	 *
	 * @return TRUE if a visual viewport change was request, or FALSE if the
	 * visual viewport is already where it should be
	 */
	BOOL		RequestSetVisualViewport(const OpRect& viewport, OpViewportChangeReason reason);

	/**
	 * Request that the visual viewport position be changed.
	 *
	 * This method will not change the visual viewport on its own; instead it
	 * will call OpViewportListener::OnVisualViewportEdgeChangeRequest(), and
	 * then it is up to the user interface to decide whether or not to carry
	 * out the request.
	 *
	 * @param x New visual viewport X position, in document coordinates.
	 *
	 * @param y New visual viewport Y position, in document coordinates.
	 *
	 * @param reason Reason for change request.
	 *
	 * @return TRUE if a visual viewport change was request, or FALSE if the
	 * visual viewport is already where it should be
	 */
	BOOL		RequestSetVisualViewPos(INT32 x, INT32 y, OpViewportChangeReason reason);

	/**
	 * Request that the specified area be scrolled into the view.
	 *
	 * Calculate a suitable visual viewport to contain the specified area.
	 *
	 * This method will not change the visual viewport on its own; instead it
	 * will call OpViewportListener::OnVisualViewportChangeRequest() or
	 * OpViewportListener::OnVisualViewportEdgeChangeRequest(), and then it is
	 * up to the user interface to decide whether or not to carry out the
	 * request.
	 *
	 * @param rect Rectangle that should fit within the visual viewport.
	 *
	 * @param align Alignment of the rectangle in the visual viewport.
	 *
	 * @param strict_align If TRUE, request visual viewport to change as long
	 * as the rectangle isn't positioned exactly where it should be in the
	 * visual viewport, as specified by 'align'. If FALSE, only request visual
	 * viewport to be changed if the specified rectangle is outside the current
	 * visual viewport (fully or partially).
	 *
	 * @param reason Reason for change request.
	 *
	 * @return TRUE if a visual viewport change was request, or FALSE if the
	 * visual viewport is already where it should be
	 */
	BOOL		RequestScrollIntoView(const OpRect& rect, SCROLL_ALIGN align, BOOL strict_align, OpViewportChangeReason reason);

	/**
	 * Attempt to constrain viewports to the bounds of the document.
	 */
	void			RequestConstrainVisualViewPosToDoc();

	/**
	 * Request that the visual viewport should use the initial scale.
	 *
	 * This method should be called whenever the visual viewport
	 * should be adopted to the initial scale (as specified by the
	 * document). It can be called from either inside FramesDocument,
	 * or from another class controlling the FramesDocument.
	 *
	 * This method will not change the visual viewport on its own;
	 * instead it will call
	 * OpViewportRequestListener::OnZoomLevelChangeRequest(), and then
	 * it is up to the user interface to decide whether or not to
	 * carry out the request.
	 *
	 * @param reason Reason for the change request.
	 */
	void		RequestSetViewportToInitialScale(OpViewportChangeReason reason);

	/**
	 * Use this method to tell the document about incoming data from
	 * the network. The document will find out where to put it and how
	 * to parse it.
	 *
	 * @param msg What kind of data input it is, typically
	 * MSG_URL_DATA_LOADED but may also be for instance
	 * MSG_NOT_MODIFIED.
	 *
	 * @param url_id The url that are sending data to the
	 * document. Used to make sure different loaded documents don't
	 * interfere with eachother.
	 *
	 * @param user_data Extra information connected to the msg.
	 *
	 * @returns OpStatus::OK if the data was processed successfully.
	 *
	 * @todo Write a more thorough documentation.
	 */
	OP_STATUS       HandleLoading(OpMessage msg, URL_ID url_id, MH_PARAM_2 user_data);

	/**
	 * Check if this document has any elements depending on calls to
	 * Blink().
	 *
	 * @returns TRUE if there are blinking elements, FALSE otherwise.
	 */
	BOOL			HasBlink() { return has_blink; }

	/**
	 * Sets if this document has blink elements or not. Only set this
	 * to FALSE if you're absolutely certain there are no elmeents
	 * with blink in the document. If uncertain it's relatively
	 * harmless to leave on though it cost some performance.
	 *
	 * @param[in] val TRUE if there is atleast one element in the
	 * document with blink, FALSE otherwise.
	 */
	void			SetBlink(BOOL val);

	/**
	 * Blinks any blinking elements in the document. Only meaningful
	 * if HasBlink returns TRUE.
	 *
	 * @see HasBlink()
	 */
	void			Blink();

	/**
	 * In a frameset there is one document that is the root
	 * document. This returns that document. This will not return
	 * parent documents on the other side of an iframe. Instead every
	 * iframe creates a local tree.
	 *
	 * @returns The FramesDocument that is the root in the local tree
	 * formed by the window or the iframe this document is in. May be
	 * this element but is never NULL. This is different to
	 * FramesDocElm::GetTopFramesDoc()
	 *
	 * @see FramesDocElm::GetTopFramesDoc()
	 *
	 * @todo There are callers assuming this might return NULL. Fix
	 * those.
	 */
	FramesDocument*	GetTopFramesDoc();

	/**
	 * Geir 2000-05-26: "compact document history when frame documents
	 * are freed.". This whole system is badly tested and likely to
	 * have serious bugs. Should be tested and/or removed.
	 *
	 * @see FramesDocElm::CheckHistory()
	 *
	 * @see DocumentManager::CheckHistory()
	 *
	 * @todo Integrate with the code in dochand and remove from
	 * FramesDocument.
	 */
	void			CheckHistory(int decrement, int& minhist, int& maxhist);

	/**
	 * Removes everything after a certain history number. Used when
	 * you navigate back in history and then follows a new link.
	 *
	 * @param from The history number.
	 *
	 * @see FramesDocElm::RemoveFromHistory()
	 *
	 * @see DocumentManager::RemoveFromHistory()
	 *
	 * @todo Integrate with the code in dochand and remove from
	 * FramesDocument.
	 */
	void			RemoveFromHistory(int from);

	/**
	 * Removes everything up to a certain history number. Used to
	 * clear old history in a window.
	 *
	 * @param to The history number.
	 *
	 * @see FramesDocElm::RemoveUptoHistory()
	 *
	 * @see DocumentManager::RemoveUptoHistory()
	 *
	 * @todo Integrate with the code in dochand and remove from
	 * FramesDocument.
	 */
	void			RemoveUptoHistory(int to);

	/**
	 * Propagates the history pos down to all child documents.
	 *
	 * @param[in] num The number of the history pos.
	 *
	 * @param[in] parent_doc_changed If TRUE, can cause reloads and
	 * such, but see DocumentManager for details.
	 *
	 * @param[in] is_user_initiated If the history traversal was caused
	 * due to user interaction. Else would be for cases initiated by scripts.
	 *
	 * @see FramesDocElm::SetCurrentHistoryPos()
	 *
	 * @see DocumentManager::SetCurrentHistoryPos()
	 *
	 * @todo Move to dochand and keep history positions out of doc?
	 */
	void			SetCurrentHistoryPos(int num, BOOL parent_doc_changed, BOOL is_user_initiated);

	/**
	 * Looks for a frame (or iframe) among the child documents that
	 * has the exact history position |pos|.
	 *
	 * @param[in] pos The position to look for.
	 *
	 * @param[in] forward TRUE if we should search forward, FALSE if
	 * we should search backwards.
	 *
	 * @returns DocListElm* The matching DocListElm or NULL if none
	 * was found.
	 *
	 * @todo Move to dochand and let it play with history numbers and
	 * document trees without involving FramesDocument?
	 *
	 * @see DocListElm
	 *
	 * @see FramesDocElm::GetHistoryElmAt()
	 *
	 */
    DocListElm*     GetHistoryElmAt(int pos, BOOL forward = FALSE);

#ifdef XML_EVENTS_SUPPORT

	/**
	 * Inform the document that there has been xml events found in the
	 * document. From now on some operations will be heavier.
	 */
	void SetHasXMLEvents() { has_xml_events = TRUE; }

	/**
	 * If this returns FALSE we don't have to check for XML Event
	 * attributes for instance.
	 *
	 * @returns TRUE if there are xml events in the document, FALSE otherwise.
	 */
	BOOL HasXMLEvents() { return has_xml_events; }
#endif // XML_EVENTS_SUPPORT

#ifdef _PRINT_SUPPORT_
	/**
	 * Clones the fdoc document into a clone adapted to
	 * printing. Called from DocumentManager.
	 *
	 * @param[in] pd The VisualDevice that will eventually be used to
	 * paint/print the cloned document.
	 *
	 * @param[in] fdoc The original FramesDocument.
	 *
	 * @param[in] vd_width Width of the page to print on.
	 *
	 * @param[in] vd_height Height of the page to print on.
	 *
	 * @param[in] preview Set to TRUE if this is only a print preview.
	 *
	 * @returns OpBoolean::IS_TRUE if the cloning succeeded,
	 * OpBoolean::IS_FALSE if it failed, unless it failed because of
	 * OOM in which case it returns OpStatus::ERR_NO_MEMORY.
	 */
	OP_BOOLEAN		CreatePrintLayout(PrintDevice* pd, FramesDocument* fdoc, int vd_width, int vd_height, BOOL page_format, BOOL preview = FALSE);

	/**
	 * Deletes the document copy we made for printing.
	 */
	void			DeletePrintCopy();
#endif // _PRINT_SUPPORT_

#ifdef SAVE_DOCUMENT_AS_TEXT_SUPPORT
	/**
	 * Serializes the current document into a stream.
	 *
	 * @param[in] stream The document will be saved to this stream if
	 * it's not NULL. The only exception is text files that will be
	 * saved directly to a file named by the parameter fname if that
	 * is non-NULL.
	 *
	 * @param[in] fname The name of a file to save to. This is only
	 * used if the document is a text file or if stream is NULL. If
	 * this is NULL, the stream is always used.
	 *
	 * @param[in] force_encoding A charset to use instead of the
	 * document's charset.
	 *
	 * @returns OpStatus::OK if the operation finished successfully.
	 */
	OP_STATUS		SaveCurrentDocAsText(UnicodeOutputStream* stream, const uni_char* fname=NULL, const char *force_encoding=NULL);
#endif // SAVE_DOCUMENT_AS_TEXT_SUPPORT

#ifdef PAGED_MEDIA_SUPPORT
	/**
	 * Returns a pointer to a page description object for the y
	 * coordinate, creating new ones if needed. This method is only
	 * meaningful on FramesDocument and will move to that class.
	 *
	 * @param y The y coordinate in the document.
	 *
	 * @param create_page If new PageDescription objects should be
	 * created if they don't exist already. If this is TRUE and the
	 * method returns NULL, then we ran out of memory.
	 *
	 * @returns A pointer to a PageDescription object or NULL. If
	 * create_page was TRUE, then a returned NULL means
	 * Out-of-memory. Otherwise we just didn't have one cached since
	 * earlier. The object is owned by the FramesDocument.
	 *
	 * @see PageDescription
	 */
	PageDescription*
					GetPageDescription(long y, BOOL create_page = FALSE);

    /**
     * Has something to do with layouting paged media.
     *
     * @todo Document. Find a layout guy who understands this and
     * force him to explain it.
	 *
	 * @todo Move to LayoutWorkplace?
	 */
	PageDescription*
					GetNextPageDescription(long y);

    /**
     * Has something to do with layouting paged media.
     *
     * @todo Document. Find a layout guy who understands this and
     * force him to explain it.
	 *
	 * @todo Move to LayoutWorkplace?
	 */
	PageDescription*
					CreatePageDescription(PageDescription* prev_page);

    /**
     * Has something to do with layouting paged media.
     *
     * @todo Document. Find a layout guy who understands this and
     * force him to explain it.
	 *
	 * @todo Move to LayoutWorkplace?
	 */
	PageDescription*
					GetPage(int number);

    /**
     * Has something to do with layouting paged media.
     *
     * @todo Document. Find a layout guy who understands this and
     * force him to explain it.
	 *
	 * @todo Move to LayoutWorkplace?
	 */
	PageDescription*
					GetCurrentPage();

	/**
	 * Finish the current page.
	 *
	 * Called when switching to a new page, or when finishing layout.
	 *
	 * @param bottom Position where the next page break occurs, or where the
	 * document ends, relative to the current translation.
	 */
	void			FinishCurrentPage(long bottom);

	/**
	 * Advance to the next page.
	 *
	 * @param next_page_top Position where the page break occurs, relative to
	 * the current translation.
	 *
	 * @todo Move to LayoutWorkplace?
	 */
	PageDescription*
					AdvancePage(long next_page_top);

    /**
     * Has something to do with layouting paged media.
     *
     * @todo Document. Find a layout guy who understands this and
     * force him to explain it.
	 *
	 * @todo Move to LayoutWorkplace?
	 */
	void			SetCurrentPage(PageDescription* page);

    /**
     * Has something to do with layouting paged media.
     *
     * @todo Document. Find a layout guy who understands this and
     * force him to explain it.
	 *
	 * @todo Move to LayoutWorkplace?
	 */
	void			RewindPage(long position);

    /**
     * Has something to do with layouting paged media.
     *
     * @todo Document. Find a layout guy who understands this and
     * force him to explain it.
	 *
	 * @todo Move to LayoutWorkplace?
	 */
	void			ClearPages();

	/**
	 * Get current page top, relative to the top of the first page.
	 *
	 * @todo Move to LayoutWorkplace?
	 */
	long			GetPageTop();

    /**
     * Has something to do with layouting paged media, maybe nested
     * page media because it seems to be used when page breaking
     * iframes.
     *
	 * @see GetRelativePageBottom()
	 *
     * @todo Document. Find a layout guy who understands this and
     * force him to explain it.
	 *
	 * @todo Move to LayoutWorkplace?
	 */
	BOOL			OverflowedPage(long top, long height, BOOL is_iframe = FALSE);

	/**
	 * Get current page top, relative to current VisualDevice translation.
	 *
	 * @see GetRelativePageBottom()
	 *
	 * @todo Move to LayoutWorkplace?
	 */
	long			GetRelativePageTop();

	/**
	 * Get current page bottom, relative to current VisualDevice translation.
	 *
	 * @see GetRelativePageTop()
	 *
	 * @todo Move to LayoutWorkplace?
	 */
	long			GetRelativePageBottom();

	/**
	 * Triggers a reflow and returns TRUE, while saving the
	 * first_page_offset value for the future.
	 *
	 * @returns TRUE
	 *
	 * @todo Document
	 *
	 * @todo Move to LayoutWorkplace?
	 *
	 * @todo Remove the return value since it's a constant (TRUE).
	 *
	 * @param It asserts at print_doc, move to printing instead of
	 * paged_media?
	 */
	BOOL			PageBreakIFrameDocument(int first_page_offset);
#endif // PAGED_MEDIA_SUPPORT

	// Methods for loading inline URLs

	/**
	 * Starts the loading of an inline resource, for instance image,
	 * script, stylesheet, external form data, ...
	 *
	 * A started load can be stopped with StopLoadingInline() if
	 * it's called with the exact same parameters.
	 *
	 * @param[in] inline_url The url to load.
	 *
	 * @param[in] element The element that is responsible for the
	 * load. It will act as the key to the load together with the
	 * inline_type so two inlines can not have the same element and
	 * inline_type at the same type. Trying to load a new resource
	 * that conflicts with an earlier resource will deregister the
	 * earlier resource.
	 *
	 * @param[in] inline_type The type of the inline.
	 *
	 * @param[in] options @see LoadInlineOptions.
	 *
	 * @returns A status saying how the loading
	 * started. OpStatus::ERR_NO_MEMORY has its usual meaning, but
	 * then there is LoadInlineStatus::LOADING_REFUSED which means
	 * that the loading was blocked by security
	 * rules. LoadInlineStatus::LOADING_CANCELLED which is supposed to
	 * mean that the resource couldn't be loaded, for instance because
	 * we're leaving the page or because we can't handle the resource
	 * anyway. LoadInlineStatus::LOADING_CANCELLED has historically
	 * also been returned in cases where we didn't load because we
	 * already had the resource but for that case
	 * LoadInlineStatus::USE_LOADED should be returned nowadays.
	 * Finally it can return LoadInlineStatus::LOADING_STARTED which
	 * means that a load has been initiated and HandleInlineDataLoaded
	 * will be called later. This is the most common return value for the
	 * first call for a load. Subsequent loads will typically return
	 * USE_LOADED on the same element, at least once the
	 * resource has been loaded.
	 *
	 * NOTE: For some edge cases (e.g. when asynchronous security check is required)
	 * LoadInlineStatus::LOADING_STARTED will be returned for urls which mustn't be loaded.
	 * Loading of such urls will later be aborted by posting the URL_LOADING_FAILED message.
	 *
	 * @see StopLoadingInline
	 */
	OP_LOAD_INLINE_STATUS
					LoadInline(URL *inline_url, HTML_Element *html_elm, InlineResourceType inline_type, const LoadInlineOptions &options = LoadInlineOptions())
	{
		return LocalLoadInline(inline_url, inline_type, NULL, html_elm, options);
	}

	/**
	 * Starts the loading of an inline resource, for instance image,
	 * script, stylesheet, external form data, external svg
	 * document... Must not be passed a listener that is already
	 * loading an inline resource.
	 *
	 * A started load can be stopped with StopLoadingInline() if
	 * it's called with the exact same parameters.
	 *
	 * @param[in] url The resource to load.
	 *
	 * @param[in] listener The listener that will receive loading
	 * events. The listener is owned by the caller, and will keep
	 * getting events until the listener is removed by
	 * StopLoadingInline or because the document is destroyed.
	 *
	 * @param[in] reload_conditonally If TRUE, an already loaded
	 * inline URL will be reloaded conditionally from the server.
	 *
	 * @param[in] block_paint_until_loaded If TRUE painting of the document will be avoided
	 * if possible until after the resource has loaded or until too much time has passed.
	 * Should be used for resources that are needed to display any content correctly. Typically
	 * that has been styles and webfonts.
	 *
	 * @returns A status saying how the loading
	 * started. OpStatus::ERR_NO_MEMORY has its usual meaning, but
	 * then there is LoadInlineStatus::LOADING_REFUSED which means
	 * that the loading was blocked by security
	 * rules. LoadInlineStatus::LOADING_CANCELLED which is supposed to
	 * mean that the resource couldn't be loaded, for instance because
	 * we're leaving the page or because we can't handle the resource
	 * anyway. LoadInlineStatus::LOADING_CANCELLED has historically
	 * also been returned in cases where we didn't load because we
	 * already had the resource but for that case
	 * LoadInlineStatus::USE_LOADED should be returned nowadays.
	 * Finally it can return LoadInlineStatus::LOADING_STARTED which
	 * means that a load has been initiated and the listener will be
	 * called later.  For LoadInlineStatus::USE_LOADED the listener
	 * function LoadingStopped is called from LoadInline.
	 *
	 * NOTE: For some edge cases (e.g. when asynchronous security check is required)
	 * LoadInlineStatus::LOADING_STARTED will be returned for urls which mustn't be loaded.
	 * Loading of such urls will later be aborted by posting the URL_LOADING_FAILED message.
	 *
	 * @see StopLoadingInline
	 */
	OP_LOAD_INLINE_STATUS
	LoadInline(const URL &url, ExternalInlineListener *listener, BOOL reload_conditionally = FALSE, BOOL block_paint_until_loaded = FALSE, InlineResourceType inline_type = GENERIC_INLINE);

	/**
	 * Checks the value of the preference PrefsCollectionDoc::ExternalImage
	 * and tells if the given url can be loaded. The value of the preference
	 * only applies for handheld mode.
	 *
	 * @param inline_type   Type of the image inline being used. If the inline_type is
	 *                      not an image TRUE is returned.
	 * @param image_url     The image url to be checked.
	 * @return FALSE if the image is from a different host than the document, the preference
	 *         has value 0 and the inline_type is an image inline type, TRUE otherwise.
	 */
	BOOL			CheckExternalImagePreference(InlineResourceType inline_type, const URL& image_url);

	/**
	  * Assigns an inline proper load priority.
	  *
	  * @param inline_typ Type of the inline.
	  * @param inline_url The URL that the inline will be loaded from.
	  *
	  * @returns Priority for given inline type
	  */
	int				GetInlinePriority(int inline_type, URL *inline_url);

	/**
	 * Stops the inline load started with LoadInline. This must be
	 * called to unregister the inline and allow it to be garbage
	 * collected.
	 *
	 * @param[in] The URL used in LoadInline.
	 *
	 * @param[in] listener The listener used in LoadInline.
	 *
	 * @see LoadInline(const URL &url, ExternalInlineListener *listener)
	 */
	void			StopLoadingInline(const URL &url, ExternalInlineListener *listener);

    /**
	 * Handle that a CSS loading element no longer want to load the
	 * stylesheet, maybe because the href changes or the element is
	 * being removed from the tree.
	 *
	 * @param[in] html_elm The stylesheet loading element.
	 *
	 * @returns OpStatus::OK
	 *
	 * @todo Remove the return value.
	 */
    OP_STATUS       CleanInline(HTML_Element* html_elm);

	/**
	 * Record the painted positon of an image. This is needed for
	 * efficient animation and to notice when images are scrolled out
	 * of view. With the position it's possible to force an image
	 * repaint without doing a reflow.
	 *
	 * Image decoding may be conducted in this call, now that it is
	 * known that the image is visible. Small images and images in
	 * print document are always decoded, otherwise images are only
	 * decoded when the TurboMode pref is enabled.
	 *
	 * @param[in] element The element referring to the image.
	 *
	 * @param[in] absolute_pos The document position of the image.
	 *
	 * @param[in] width The width of the painted image. Document coordinates.
	 *
	 * @param[in] height The height of the painted image. Document coordinates.
	 *
	 * @param[in] background Whether it was a background image or an
	 * ordinary image.
	 *
	 * @param[in] helm The HEListElm associated with the image.
	 *
	 * @param[in] expand_area If TRUE the previous area isn't replaced
	 * by this area, but instead merged.  This is used for inline
	 * elements in layout, when the element is broken onto several
	 * lines and SetImagePosition is called once for each.
	 */
	void			SetImagePosition(HTML_Element* element, const AffinePos& pos, long width, long height, BOOL background, HEListElm* helm = NULL, BOOL expand_area = FALSE);

	/**
	 * Returns if the document is configured to load images or
	 * not. This flag is copied from the Window when the document is
	 * created or navigated to. It can be changed on the fly with the
	 * SetMode method. This is a different flag than the flag to show
	 * images. If this is true we might still display images that are
	 * already decoded.
	 *
	 * @returns TRUE if images should be loaded, FALSE don't load
	 * images.
	 *
	 * @see GetShowImages();
	 */
	BOOL			GetLoadImages() { return packed.load_images; }

	/**
	 * Returns if the document is configured to show images or
	 * not. This flag is copied from the Window when the document is
	 * created or navigated to. It can be changed on the fly with the
	 * SetMode method. This is a different flag than the flag to load
	 * images.
	 *
	 * @returns TRUE if images should be displayed, FALSE don't show
	 * images.
	 *
	 * @see GetLoadImages()
	 */
	BOOL			GetShowImages() { return packed.show_images; }

	/**
	 * Finds out the (re)load policy of the inline. The policy is determined
	 * from if the inline already has been requested, the DocumentManager's
	 * opinion about inline reloading and if we explict requested a reload
	 * through the reload parameter.
	 *
	 * @param inline_url The inline in question.
	 *
	 * @param reload Reload (or at least validate) the inline from the server.
	 *
	 * @returns a loadpolicy suitable for the inline.
	 */
	URL_LoadPolicy	GetInlineLoadPolicy(URL inline_url, BOOL reload);

#ifdef FIT_LARGE_IMAGES_TO_WINDOW_WIDTH
	/**
	 * Returns if the document is configured to resize images to fit
	 * the width of the window. This flag is copied from the Window
	 * when the document is created or navigated to. When the
	 * document's URL is the image, the image will not be resized.
	 *
	 * @returns TRUE if images should be resized to fit the window's
	 *          width.
	 */
	BOOL			GetFitImagesToWindow() { return packed.fit_images; }
#endif // FIT_LARGE_IMAGES_TO_WINDOW_WIDTH

	/**
	 * Checks if the document is using a layout mode that indicates
	 * that we should adapt the page to a handheld device.
	 *
	 * @returns TRUE if we're in "handheld" mode (TRUE when using SSR
	 * or CSSR right now).
	 */
	BOOL			GetHandheldEnabled() { return layout_mode == LAYOUT_SSR || layout_mode == LAYOUT_CSSR; }

    /**
     * Has something to do with layout.
     *
	 * @todo Move to LayoutWorkplace?
	 */
	short			GetFlexFontScale() { return flex_font_scale; }

    /**
     * Instructs the layout engine on how to handle absolutely
     * positioned elements when we have scaled the page.
     *
     * @returns ABS_POSITION_NORMAL, ABS_POSITION_STATIC or
     * ABS_POSITION_SCALE.
	 *
	 * @see ABS_POSITION_NORMAL
	 * @see ABS_POSITION_STATIC
	 * @see ABS_POSITION_SCALE
	 *
	 * @todo Move to LayoutWorkplace?
	 *
	 * @todo Make it an enum? We save some space here but we could
	 * still store in 3 or 8 or 16 bits even if the values are
	 * constants in an enum.
	 */
	short			GetAbsPositionedStrategy() { return abs_positioned_strategy; }

    /**
     * Instructs the layout engine how to handle word
     * breaking. Default is WORD_BREAK_NORMAL.
	 *
	 * @returns WORD_BREAK_NORMAL, WORD_BREAK_AGGRESSIVE, WORD_BREAK_EXTREMELY_AGGRESSIVE
	 *
	 * @see WORD_BREAK_NORMAL
	 * @see WORD_BREAK_EXTREMELY_AGGRESSIVE
	 * @see WORD_BREAK_AGGRESSIVE
	 *
	 * @todo Move to LayoutWorkplace?
	 *
	 * @todo Make it an enum? We save some space here but we could
	 * still store in 3 or 8 or 16 bits even if the values are
	 * constants in an enum.
	 */
	short           GetAggressiveWordBreaking() { return aggressive_word_breaking; }

	/**
	 * Checks if we should trust the media style or not.
	 *
	 * @returns FALSE if we should trust the media style and TRUE if
	 * we should override it (ignore it).
	 *
	 * @see GetMediaType()
	 *
	 * @see GetMediaHandheldResponded()
	 *
	 * @see CheckOverrideMediaStyle()
	 */
	BOOL            GetOverrideMediaStyle() { return override_media_style; }

	/**
	 * Change the text scale in the document.
	 *
	 * @param[in] scale The text scale to set. 100 is the normal size
	 * (100%), 50 (50%) is text in half the original size.
	 *
	 * <p>Removes cached info from boxes in the document and subdocuments
	 * and mark them dirty
	 */
	void			SetTextScale(int scale);

	/**
	 * Call this to set the flag that the document has special
	 * stylesheets adapted to handheld devices.
	 */
	void			SetHandheldStyleFound();

	/**
	 * Returns the current layout mode, SSR, MSR, NORMAL, ...
	 *
	 * @returns The layout mode currently used for this document.
	 */
	LayoutMode		GetLayoutMode() { return layout_mode; }

	/**
	 * @see GetLayoutMode()
	 *
	 * @deprecated Use GetLayoutMode.
	 */
	LayoutMode		DEPRECATED(GetCurrentLayoutMode()); // inlined below

	/**
	 * Returns our current layout mode in the type that prefs uses for
	 * use in for instance PrefsCollectionDisplay::GetPrefFor.
	 *
	 * @returns The current layout mode.
	 *
	 * @see PrefsCollectionDisplay::GetPrefFor()
	 */
	PrefsCollectionDisplay::RenderingModes    GetPrefsRenderingMode();

	/**
	 * Returns if ERA ("fit to width") is enabled. ERA is that we
	 * adapt the layout mode to for instance the window width so
	 * smaller windows give simpler layout.
	 *
	 * @returns TRUE if ERA is enabled, FALSE otherwise.
	 */
	BOOL			GetERA_Mode() { return era_mode; }

	/**
	 * Era_mode changed. We need to reenable CheckERA_LayoutMode for this
	 * window width
	 *
	 */
	void			ERA_ModeChanged() { old_win_width = UINT_MAX; }

	/**
	 * This is the ERA layout mode switching method. It will change
	 * the layout and frame policy (frame stacking, smart frames) to
	 * suit the current window size and style availability. This
	 * should be called when ERA is enabled and something has changed
	 * that may make us reconsider the current layout mode. That may
	 * include window width or media responses.
	 *
	 * @param force, FALSE will enable check versus win_width change.
	 *
	 * <p>Must not be called if ERA isn't wanted.
	 */
	void			CheckERA_LayoutMode(BOOL force = TRUE);

	/**
	 * This has to do with if the page had a stylesheet adapted to
	 * handheld devices. In those cases we may want to skip our own
	 * special rendering modes and trust the web page's ability to
	 * layout in a suitable way. This is for instance used to switch
	 * to projection mode if we detect that the page delivers a
	 * stylesheet adapted to projection devices, while we normally use
	 * screen.
	 *
	 * @returns TRUE if there is something specially adapted to
	 * handhelds here.
	 *
	 * @todo Find someone who can describe this better. Maybe rune.
	 */
	BOOL			GetMediaHandheldResponded() { return media_handheld_responded; }

	/**
	 * Check if this document is using frame stacking or smart frames
	 * or normal frames handling.
	 *
	 * @returns FRAMES_POLICY_DEFAULT, FRAMES_POLICY_NORMAL,
	 * FRAMES_POLICY_FRAME_STACKING or
	 * FRAMES_POLICY_SMART_FRAMES. FRAMES_POLICY_DEFAULT is the normal
	 * return value and means that the same question should be asked
	 * the Window object.
	 */
	int				GetFramesPolicy() { return frames_policy; }

	/**
	 * Convinience method to return the root node in the document
	 * tree. This node has type HE_DOC_ROOT and shouldn't be mixed up
	 * with the root in the document.
	 *
	 * @returns The HE_DOC_ROOT or NULL if there was no document at
	 * all.
	 */
	HTML_Element*	GetDocRoot();

	/**
	 * Checks if all inlines are loaded. This must be checked before
	 * calling FinishDocument. Note that this might change its return
	 * value from FALSE to TRUE back and forth when inline loads are
	 * triggered by for instance scripts or css.
	 *
	 * @returns TRUE If all currently registered inlines are loaded as
	 * they should be.
	 */
	BOOL            InlineImagesLoaded();

	/**
	 * Traverses through the whole
	 * document structure, including inline elements and sets the
	 * security state in the Window to the seucrity state of the
	 * different parts.
	 *
	 * @param[in] include_loading_docs FALSE if only documents with
	 * state NOT_LOADING should be considered.
	 *
	 * @see DocumentManager::UpdateSecurityState(BOOL include_loading_docs)
	 */
	void			UpdateSecurityState(BOOL include_loading_docs);

	/**
	 * While a document is being loaded from an url we keep an
	 * URL_DataDescriptor in the FramesDocument and this method will
	 * return it. It's deleted and set to NULL after the load has
	 * finished and is NULL before the load has actually started.
	 *
	 * @returns NULL or an URL_DataDescriptor.
	 */
	URL_DataDescriptor*
					Get_URL_DataDescriptor() { return url_data_desc; }

	/**
	 * In a document, there can be a current element, selected by for
	 * instance spatial navigation or inline search. That is the
	 * element that should be activated when pressing RETURN. This
	 * returns the same as HTML_Document::GetCurrentHTMLElement()
	 * unless it's a frameset in which case it returns the current
	 * element in the current frame.
	 *
	 * @returns The current HTML_Element or NULL if no element can be
	 * considered the current or if there was no active frame.
	 */
    HTML_Element*	GetCurrentHTMLElement();

	/**
	 * Returns the area in the view that is affected by the selection
	 * to minimize the amount we repaint.
	 *
	 * @param[out] x The upper left x coordinate in document
	 * coordinate scale, relative the upper left corner of the view.
	 *
	 * @param[out] y The upper left y coordinate in document
	 * coordinate scale, relative the upper left corner of the view.
	 *
	 * @param[out] width The width in document coordinate scale.
	 *
	 * @param[out] height The height of the selection in document
	 * coordinate scale.
	 *
	 * @returns TRUE if there was a selection, FALSE otherwise. If it
	 * returns FALSE, no out parameters are set.
	 *
	 * @see HTML_Document::GetSelectionBoundingRect()
	 */
	BOOL	GetSelectionBoundingRect(int &x, int &y, int &width, int &height);

#ifdef ACCESS_KEYS_SUPPORT


	/**
	 * Checks if the document has specified any custom accesskeys.
	 *
	 * @returns TRUE if there are accesskeys, FALSE otherwise.
	 */
	BOOL			HasAccessKeys();

#endif // ACCESS_KEYS_SUPPORT

#ifdef _MIME_SUPPORT_
	/**
	 * This creates an url that contains a placeholder document with a
	 * link to the real url and a message telling the user that the
	 * original url was blocked (for some reason, security or settings
	 * probably).
	 *
	 * <p>This code has moved to the about module and this is only a
	 * wrapper function nowadays.
	 *
	 * @param[in,out] To contain the original url when called and will
	 * contain the generated url when exiting if the method returns
	 * OpStatus::OK.
	 *
	 * @see OpSuppressedURL in the about module.
	 */
    static OP_STATUS	MakeFrameSuppressUrl(URL &frame_url);
#endif // _MIME_SUPPORT_

#ifdef SCOPE_ECMASCRIPT_DEBUGGER
	/**
	 * Inspect an element in the debugger. It is up to the scope module and the
	 * debugger to choose how to handle it, but opening a DOM inspector would be
	 * a good idea.
	 *
	 * @param[in] context Used to find a suitable element to inspect
	 */
	static OP_STATUS DragonflyInspect(OpDocumentContext& context);
#endif // SCOPE_ECMASCRIPT_DEBUGGER

#ifdef SVG_SUPPORT
	OP_STATUS SVGZoomBy(OpDocumentContext& context, int zoomdelta);
	OP_STATUS SVGZoomTo(OpDocumentContext& context, int zoomlevel);
	OP_STATUS SVGResetPan(OpDocumentContext& context);
	OP_STATUS SVGPlayAnimation(OpDocumentContext& context);
	OP_STATUS SVGPauseAnimation(OpDocumentContext& context);
	OP_STATUS SVGStopAnimation(OpDocumentContext& context);
#endif // SVG_SUPPORT

#ifdef MEDIA_HTML_SUPPORT
	/**
	 * Play/Pause a media element.
	 *
	 * @param[in] context Used to find a suitable media element.
	 * @param[in] play If TRUE the media element is played/unpaused, otherwise
	 *     it is paused.
	 */
	OP_STATUS MediaPlay(OpDocumentContext& context, BOOL play);

	/**
	 * Show/Hide a media element's controls.
	 *
	 * @param[in] context Used to find a suitable media element.
	 * @param[in] show If TRUE the controls are shown, otherwise they are hidden.
	 */
	OP_STATUS MediaShowControls(OpDocumentContext& context, BOOL show);

	/**
	 * Mute/Unmute a media element.
	 *
	 * @param[in] context Used to find a suitable media element.
	 * @param[in] mute If TRUE the element is muted, otherwise it is unmuted.
	 */
	OP_STATUS MediaMute(OpDocumentContext& context, BOOL mute);
#endif // MEDIA_HTML_SUPPORT

	/**
	 * The constructor/fetcher for FramesDocElms for iframes. This can fail if
	 * there is OOM or if the iframe can't be created because this is
	 * a printing document for instance (printing documents can't load iframes)
	 * and there was no cloned iframe. That can happen if the cloning has failed
	 * or if the iframe is such that it only appears in printing, not int
	 * normal view. The first time this is called for an iframe, the FramesDocElm
	 * is created (or an error code is returned). Subsequent calls will return
	 * the same FramesDocElm.
	 *
	 * @param[out] frame The FramesDocElm that was created (if return value is OpStatus::OK)
	 * This parameter is set to NULL if the return value is an error code.
	 *
	 * @param[in] width The inital width of the iframe view.
	 *
	 * @param[in] height The initial height of the iframe view.
	 *
	 * @param[in] element The "iframe" element.
	 *
	 * @param[in] parentview The parent docment's view.
	 *
	 * @param[in] load_frame if TRUE any external document specified by element will be loaded inside the iframe.
	 *
	 * @param[in] visible If the document is visible or loaded in the
	 * background. A little unsure what difference that makes.
	 *
	 * @param[in] origin_thread The thread that inserted the iframe.
	 *
	 * @returns OpStatus::ERR_NO_MEMORY, an error code if blocked (for instance because of printing) or OpStatus::OK if
	 * a non-NULL frame was returned.
	 */
	OP_STATUS	GetNewIFrame(FramesDocElm*& frame, int width, int height, HTML_Element* element, OpView* parentview, BOOL load_frame, BOOL visible, ES_Thread* origin_thread = NULL);

	/**
	 * Reflow is the process of creating a new or updated layout and
	 * frame tree to reflect the current logical document, stylesheet
	 * set, layout settings and view size. It's an extremely complex
	 * process and can (unfortunately) be very time consuming.
	 *
	 * <p>Side effects includes triggering loads of inlines and
	 * invalidations of the view so that we repaint whatever had
	 * changed.
	 *
	 * @param[in] set_properties When this is TRUE we do much more. A
	 * little fluffy exactly how much more.
	 *
	 * @param[in] iterate Sometimes a reflow isn't enough to solve the
	 * complex layout equation and then we more until the layout is
	 * finished. If this is FALSE we only do one even if that leaves
	 * us with a partially incorrect layout.
	 *
	 * @param[in] reflow_frameset If this is TRUE we will rebuild the
	 * frame tree, trying to reuse as much as possible of the current
	 * frame tree structure.
	 *
	 * @param[in] find_iframe_parent If TRUE we will try to resolve
	 * inter-document layout dependencies in iframes/parent documents.
	 *
	 * @returns The status of the reflow operation. OpStatus::OK if
	 * the operation finishes successfully. OpStatus::ERR_NO_MEMORY if
	 * it runs out of memory. This is one place very likely to run out
	 * of memory if the memory is low so oom has to be handled well.
	 */
	OP_STATUS		Reflow(BOOL set_properties, BOOL iterate = TRUE, BOOL reflow_frameset = FALSE, BOOL find_iframe_parent = TRUE);

	/**
	 * Get the list of iframes in the document. NULL if this is a
	 * document without any iframes. The can not exist at the same
	 * time as the frm_root (IsFrameDoc()) since a document can't
	 * contain both normal frames and iframes.
	 *
	 * @returns Pointer to the iframe list. All iframes are children
	 * to the root. Will be NULL if there are no iframes.
	 */
	FramesDocElm*	GetIFrmRoot() { return ifrm_root; }

	/**
	 * Checks if this is an iframe document.
	 *
	 * @returns TRUE if this is a document that is loaded directly
	 * into an HTML &lt;iframe&gt; and FALSE otherwise.
	 */
	BOOL			IsInlineFrame();

	/**
	 * Originally part of the feature where we showed every frame as a
	 * different (tabbed) page but now used by some code to iterate
	 * over frames.
	 *
	 * <p>Used by accessibility code in desktop so don't remove until
	 * that is fixed.
	 *
	 * @returns The number of frames (which can be less than the
	 * number of FramesDocuments. A document with no frames will
	 * return 0.
	 *
	 * @todo Remove all this code and make sure users use the
	 * sub_win_id based ones instead and possibly frame iterators.
	 */
	int				CountFrames();

	/**
	 * Originally part of the feature where we showed every frame as a
	 * different (tabbed) page but now used by some code to iterate
	 * over frames.
	 *
	 * <p>Used by accessibility code in desktop so don't remove until
	 * that is fixed.
	 *
	 * @param[in, out] num The number of the frame. 1 or larger. It
	 * will be decreased during the call as part of the search procedure
	 * and will probably be 0 when the method returns.
	 *
	 * @returns Pointer to the frame or NULL if not found.
	 *
	 * @todo Remove all this code and make sure users use the
	 * sub_win_id based ones instead and possibly frame iterators.
	 */
	FramesDocElm*	GetFrameByNumber(int& num);

	/**
	 * Part of the "Wait for styles before showing the page"
	 * feature. This is to be called when our timeout message arrives
	 * to the DocumentManager. This will stop our wait and display the
	 * page anyway.
	 *
	 * @returns OpStatus::OK if the operation completed successfully
	 * or an error code otherwise.
	 */
	OP_STATUS		WaitForStylesTimedOut();

#ifdef SHORTCUT_ICON_SUPPORT
	/**
	 * Called by DocumentManager as a response to the
	 * MSG_FAVICON_TIMEOUT message. This is used to timeout an
	 * inline load in case the server doesn't answer or gives a too
	 * large response. This is only used for favicons where we request
	 * something not explicitly wanted and where servers have been
	 * known to be unhappy about it.
	 *
	 * <p> NOTE: this will kill the loading of the url even if someone
	 * explicitly had requested that url.
	 *
	 * @param[in] url_id The url (id of the url) that we want to
	 * timeout.
	 */
	void			FaviconTimedOut(URL_ID url_id);
#endif // SHORTCUT_ICON_SUPPORT

	/**
	 * The opposite of LoadInline. If an element has started loading
	 * an inline but figures out that it won't need it after all it
	 * should call StopLoadingInline which will deregister the
	 * loading. It may not actually stop the load if there are other
	 * elements that still needs the inline. Only when nothing is
	 * loading it will we stop the network load.
	 *
	 * @param[in] url The url that we previously called LoadInline on
	 * and now wants to stop loading. Not NULL.
	 *
	 * @param[in] element The element responsible for the load. The
	 * same as when LoadInline was called.
	 *
	 * @param[in] inline_type The inline_type used in the LoadInline
	 * call. It's possible for instance to call LoadInline 2 times for
	 * the same element with different inline_type:s and then only
	 * stop one of them.
	 *
	 * @param[in] keep_dangling_helm If TRUE keep the removed
	 * HEListElms this means that the caller must make sure that they
	 * are deleted or added to a LoadInlineElm else it is likely that
	 * a crash occur. If FALSE, which is the default, delete them.
	 *
	 * @param[in] from_delete If TRUE then do not try to find the
	 * corresponding HEListElm and delete it.
	 *
	 * @see LoadInline()
	 */
	void			StopLoadingInline(URL* url, HTML_Element* element, InlineResourceType inline_type, BOOL keep_dangling_helm = FALSE);

	/**
	 * This method is the one that either builds a frame tree or
	 * creates a HTML_Document or does nothing depending on the state
	 * of the LogicalDocument. This should be called when we have
	 * indications that we can make that decision.
	 *
	 * @param[in,out] existing_frames If this is a reload and there
	 * was already frames before send them in here in the format
	 * required (see source code). Frames may be reused.
	 *
	 * @returns OpStatus::OK if the operation completed successfully
	 * or an error code otherwise.
	 */
	OP_STATUS		CheckInternal(Head* existing_frames = NULL);

#ifdef DELAYED_SCRIPT_EXECUTION
	/**
	 * In case so much has happened in a delayed script that our
	 * current structure (for instance that we have a HTML_Document)
	 * is wrong and we have to rerun CheckInternal, this method should
	 * be called.
	 */
	void			CheckInternalReset();
#endif // DELAYED_SCRIPT_EXECUTION

#ifdef _DEBUG
	/**
	 * Debug method that dumps all current inlines to the debug
	 * system. Not used as far as I can tell.
	 */
	void			DebugCheckInline();
#endif

	/**
	 * Check if we're inside a FramesDocument::~FramesDocument call.
	 *
	 * @returns TRUE if the document is right now being deletet. That
	 * is a sign that many things in it might be missing or
	 * inconsistant and that it shouldn't be touched.
	 */
	BOOL			IsBeingDeleted() { return is_being_deleted; }

	/**
	 * Check if we're inside a FramesDocument::Free call.
	 *
	 * @returns TRUE if the document is right now being freed. That is
	 * a sign that many things in it might be missing or inconsistant
	 * and that the FramesDocument shouldn't be touched.
	 */
	BOOL			IsBeingFreed() { return is_being_deleted || is_being_freed; }

	/**
	 * @returns TRUE if the print copy is currently being delted
	 */
	BOOL			IsPrintCopyBeingDeleted() { return print_copy_being_deleted; }

#ifdef LINK_TRAVERSAL_SUPPORT
	/**
	 * Returns all explicit links in a document. Can be used for doing
	 * auto surfers or listing links in the UI.
	 *
	 * @param[in, out] vector The address of an empty vector that will
	 * be filled with OpElementInfo elements owned by the vector (thus
	 * letting it be an OpAutoVector instead of an OpVector). Links
	 * might be missing in case of lack of memory.
	 *
	 * @see HTML_Document::GetLinkElements()
	 */
	void			GetLinkElements(OpAutoVector<OpElementInfo>* vector);
#endif // LINK_TRAVERSAL_SUPPORT

	/**
	 * Returns the media type to be used in calls to LoadCssProperties()
	 */
	CSS_MediaType	GetMediaType();

	/**
	 * Returns TRUE if the loading of the document has been aborted
	 * by the user.
	 *
	 * @returns TRUE if loading is aborted.
	 */
	BOOL			IsAborted() { return loading_aborted; }

	/**
	 * Returns if the FinishLocalDoc() method has run yet. This exists
	 * externally because we won't call FinishDocument until we've run
	 * through a Reflow (that might add work to do before we're
	 * finished such as start loading inlines. So with this method the
	 * reflow can check if they should call FinishDocument.
	 *
	 * @returns TRUE if FinishDocument has already run.
	 */
	BOOL            IsLocalDocFinished() { return local_doc_finished; }

	/**
	 * Set the flag controlling whether or not a future CheckFinishDocument()
	 * should be delayed. That is, if FinishLocalDoc() hasn't been performed yet,
	 * schedule it as an asynchronous operation.
	 *
	 * For instance, a delayed document finish is needed if CheckFinishDocument()
	 * ends up being called during a garbage collection in the scripting engine.
	 * Its garbage collector isn't capable of handling additional script object
	 * allocations while it is active.
	 *
	 * If FinishLocalDoc() has already completed, SetDelayDocumentFinish()
	 * has no effect.
	 */
	void            SetDelayDocumentFinish(BOOL f) { if (!local_doc_finished) delay_doc_finish = f; }

	/**
	 * Returns if the FinishDocTree() method has run yet. This exists
	 * externally because we won't call FinishDocument until we've run
	 * through a Reflow (that might add work to do before we're
	 * finished such as start loading inlines. So with this method the
	 * reflow can check if they should call FinishDocument.
	 *
	 * @returns TRUE if FinishDocument has already run.
	 */
	BOOL            IsDocTreeFinished() { return doc_tree_finished; }

	/**
	 * Returns if the FinishLocalDoc() method has run at least once since a full load/reload started.
	 * Useful for places where it's necessary to know if the document was once completely
	 * loaded, but whether is loading right now is not very interesting (e.g. document.readyState).
	 *
	 * @returns TRUE if FinishDocument() has already run at least once since a full load started.
	 */
	BOOL			GetDocumentFinishedAtLeastOnce() { return document_finished_at_least_once; }

#ifdef CONTENT_MAGIC
	/**
	 * Part of FEATURE_CONTENT_MAGIC.
	 */
	HTML_Element*	GetContentMagicStart() { return content_magic_start.GetElm(); }
	/**
	 * Part of FEATURE_CONTENT_MAGIC.
	 */
	BOOL			GetContentMagicFound() { return content_magic_found; }   // only used by reflow

	/**
	 * Part of FEATURE_CONTENT_MAGIC.
	 */
	void			AddContentMagic(HTML_Element* element, int character_count)
	{
		if (!content_magic_start.GetElm())
			content_magic_start.SetElm(element);
		content_magic_character_count += character_count;
	}

	/**
	 * Part of FEATURE_CONTENT_MAGIC.
	 */
	void			CheckContentMagic();

	/**
	 * Part of FEATURE_CONTENT_MAGIC.
	 */
	void			SetContentMagicPosition();

	/**
	 * Part of FEATURE_CONTENT_MAGIC.
	 */
	long			GetContentMagicPosition() { return content_magic_position; }
	/**
	 * Part of FEATURE_CONTENT_MAGIC.
	 */
	void            SetContentMagicPosition(long pos) { content_magic_position = pos; }    // only used by reflow

#else
	BOOL GetContentMagicFound() { return FALSE; } // Dummy function to keep the code compiling
	void SetContentMagicPosition(long pos=0) {} // Dummy function to keep the code compiling
	HTML_Element* GetContentMagicStart() { return NULL; } // Dummy function to keep the code compiling
	void CheckContentMagic() {} // Dummy function to keep the code compiling
    long GetContentMagicPosition() { return 0; } // Dummy function to keep the code compiling
	void AddContentMagic(HTML_Element* element, int character_count) {} // Dummy function to keep the code compiling
#endif // CONTENT_MAGIC


	/**
	 * Apply frame magic (smart frames or frame stacking), by expanding frame
	 * sizes so that content fits (without scrollbar) (and repositions frames
	 * accordingly).
	 *
	 * At the point of calling this method, the frameset has been laid out more
	 * or less according to the markup. The frame size modifications done in
	 * this method do not affect the frames' layout viewport, which means that
	 * this method will not trigger reflows or onresize javascript events.
	 */
	void			CalculateFrameSizes();

	/**
	 * Schedule top-level frameset for resizing.
	 *
	 * This is used in frame stacking and smart frames mode, to make sure that
	 * the size of the frameset is recalculated when the size of one of the
	 * document's contents changes.
	 */
	void			RecalculateMagicFrameSizes();

	/**
	 * This is part of the SmartFrames feature.
	 *
	 * @returns TRUE if SmartFrames(tm) are enabled for this document.
	 */
	BOOL			GetSmartFrames() { return smart_frames; }

	/**
	 * This is part of the SmartFrames feature. This turns on or off
	 * SmartFrames. It will propagate down to sub frames.
	 *
	 * @param[in] on TRUE if SmartFrames should be turned on, FALSE if
	 * it should be turned off.
	 */
	void			CheckSmartFrames(BOOL on);

	/**
	 * Part of the SmartFrames feature. This expands child frames with
	 * the given amount.
	 *
	 * @param[in] inc_width How much width to add to the child frame.
	 *
	 * @param[in] inc_height How much height to add to the child
	 * frame.
	 */
	void			ExpandFrameSize(int inc_width, long inc_height);

	/**
	 * This is part of the frame stacking feature. This checks if
	 * frame stacking is turned on.
	 *
	 * @returns TRUE if this is frame stacked, FALSE otherwise.
	 */
	BOOL			GetFramesStacked() { return frames_stacked; }

	/**
	 * This is part of the frame stacking feature. This turns on or
	 * off frame stacking. It will propagate down to sub frames.
	 *
	 * @param[in] stack_frames TRUE if frame stacking should be turned
	 * on, FALSE if it should be turned off.
	 */
	void			CheckFrameStacking(BOOL stack_frames);

	/**
	 * This is part of the frame stacking feature. Returns the frame
	 * set in SetLoadingFrame().
	 *
	 * @see SetLoadingFrame()
	 *
	 * @returns The FramesDocElm set in SetLoadingFrame unless it's
	 * been used already.
	 */
	FramesDocElm*	GetLoadingFrame() { return loading_frame; }

	/**
	 * This is part of the frame stacking feature and points to one of
	 * the frames. I'm not sure how it's used but it will make us
	 * scroll to that frame after the first reflow after tha frame's
	 * document has loaded.
	 *
	 * @param[in] frame The frame or NULL to clear the field.
	 */
	void			SetLoadingFrame(FramesDocElm* frame) { loading_frame = frame; }

	/**
	 * Returns TRUE if there somewhere in this document tree is a
	 * dirty document, i.e. a document where the layout tree isn't
	 * uptodate.
	 *
	 * @returns FALSE if all layout trees are up-to-date, TRUE
	 * otherwise.
	 */
	BOOL			HasDirtyChildren();

#if defined(SUPPORT_DEBUGGING_SHELL)
	/**
	 * Part of the SUPPORT_DEBUGGING_SHELL feature. Retrieves some
	 * internal state.
	 */
	Head&			GetInlineList() { return inline_list; }
#endif

#ifdef _PLUGIN_SUPPORT_
	/**
	 * To avoid spamming the user with flash download dialogs we keep
	 * a flag saying that we have already asked. Calling this
	 * retrieves that flag.
	 *
	 * @returns TRUE if we have already asked about downloading flash
	 * for this document.
	 */
	BOOL			AskedAboutFlashDownload() { return m_asked_about_flash_download; }

	/**
	 * To avoid spamming the user with flash download dialogs we keep
	 * a flag saying that we have already asked. Calling this sets
	 * that flag.
	 */
	void			SetAskedAboutFlashDownload() { m_asked_about_flash_download = TRUE; }

	/**
	 * Adds an element to the list of elements where scripts wait for a plugin to
	 * load before the script can resume. This list is used to
	 * resume scripts. Adding an element more than
	 * once is harmless but meaningless.
	 *
	 * @param[in] possible_plugin The element (HE_EMBED, HE_OBJECT, HE_APPLET, ...)
	 * that a script is waiting for.
	 *
	 * Returns OP_STATUS::OK or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS		AddBlockedPlugin(HTML_Element* possible_plugin);

	/**
	 * Removes an element from the list of elements where scripts
	 * wait for a plugin to load. This is done when all scripts
	 * waiting for the plugin has been resumed.
	 *
	 * @param[in] possible_plugin The element (HE_EMBED, HE_OBJECT, HE_APPLET, ...)
	 * that a script is waiting for. Calling this with an element that
	 * isn't in the list is harmless.
	 */
	void			RemoveBlockedPlugin(HTML_Element* possible_plugin);

	/**
	 * Checks if the element is right now loading something that might
	 * become a plugin.
	 *
	 * @param[in] possible_plugin The element.
	 */
	BOOL			IsLoadingPlugin(HTML_Element* possible_plugin);
#endif // _PLUGIN_SUPPORT_

	/**
	 * More specific method than GetShowIFrames. This checks if an
	 * iframe loading a particular url is allowed.
	 *
	 * @param[in] url The url that the iframe would load.
	 *
	 * @returns TRUE if it's allowed, FALSE if it isn't allowed.
	 *
	 * @see GetShowIFrames();
	 */
	BOOL			IsAllowedIFrame(const URL* url);

	/**
	 * Returns if iframes are enabled in this document. Return values
	 * are SHOW_IFRAMES_NONE == 0, SHOW_IFRAMES_SAME_SITE,
	 * SHOW_IFRAMES_SAME_SITE_AND_EXCEPTIONS, SHOW_IFRAMES_ALL. Most
	 * code only have to check if it's 0 ( SHOW_IFRAMES_NONE) or not
	 * 0.
	 *
	 * @returns One of the values described in the text.
	 *
	 * @see IsAllowedIFrame()
	 */
	int				GetShowIFrames();

	/**
	 * Call this when the online/offline mode changes. If going
	 * online, loading of images waiting for an online/offline
	 * decision will start.
	 *
	 * @returns OpStatus::OK if the operation completed successfully
	 * or an error code otherwise.
	 */
	OP_STATUS		OnlineModeChanged();

	/**
	 * Call this to start restoring form values after a history
	 * navigation. This will be asynchronous so the values might not
	 * all be restored after the method returns and in that case it
	 * should be called again later.
	 *
	 * @param initial The first call it should be TRUE, then it should
	 * be FALSE until all forms are filled in.
	 *
	 * @returns OpStatus::OK if the operation completed successfully
	 * or an error code otherwise.
	 */
	OP_STATUS		RestoreFormState(BOOL initial = FALSE);

	/**
	 * Called when a form element has received a new value during
	 * history navigation. This will trigger the onchange event which
	 * may let the page update other related form elements which might
	 * be necessary for the next form value to restore. For instance
	 * in linked select boxes, country, county, village, street....
	 *
	 * @returns OpBoolean::IS_TRUE if an event was sent and we must
	 * block, OpBoolean::IS_FALSE otherwise unless a serious error was
	 * encountered in which case an error code is returned.
	 */
	OP_BOOLEAN		SignalFormValueRestored(HTML_Element *element);

	/**
	 * The "restore form state" code runs as a thread in the
	 * ecmascript scheduler to get thread dependencies correct. This
	 * is called when that thread is finished and form state has been
	 * restored.
	 */
	void			ResetRestoreFormStateThread() { restore_form_state_thread = NULL; }

	/**
	 * Check if we're currently in the progress of restoring historic
	 * form data. Any side effect like submit of location changes
	 * should be ignored while that happens since it's not the user
	 * that is writing the data.
	 *
	 * @param[in] thread The script wanting to check it we're
	 * currently restoring form data.
	 *
	 * @returns TRUE if we're currently restoring form data after
	 * walking in history.
	 */
	BOOL			IsRestoringFormState(ES_Thread *thread);

	/**
	 * This is used to let the document know that changing form value
	 * will/might try to change the current page by submitting or
	 * changing location. If thread is NULL this method is a nop.
	 *
	 * @param[in] thread The script trying to change location. If NULL
	 * then this method will not set any flags.
	 */
	void			SignalFormChangeSideEffect(ES_Thread *thread);

	/**
	 * This should be called when there are no running scripts on a
	 * page to let things that don't want to be done during script
	 * execution be done.
	 */
	void			SignalESResting();

	/**
	 * Returns the relative time the navigation to this document
	 * started.  This is used by the Performance.now() API.
	 *
	 * @returns The time in ms, compatible with GetRuntimeMS(), or
	 * 0.0 if the time for some reason isn't available.
	 */
	double			GetStartNavigationTimeMS() { return start_navigation_time_ms; }

	/**
	 * Sets the time the navigation to this document started.
	 *
	 * @param start_time_ms The time, compatible with GetRuntimeMS().
	 */
	void			SetStartNavigationTimeMS(double start_time_ms) { start_navigation_time_ms = start_time_ms; }

	/**
	 * Sets the current history navigation mode a script has
	 * preferred. Not as strong as OverrideHistoryNavigationMode, but
	 * stronger than the autodetected mode. Will not override
	 * preferences.
	 *
	 * @param[in] The mode a script has requested.
	 */
	void					SetUseHistoryNavigationMode(HistoryNavigationMode mode) { use_history_navigation_mode = mode; }

	/**
	 * Returns the current history navigation mode a script has
	 * preferred. Not as strong as OverrideHistoryNavigationMode, but
	 * stronger than the autodetected mode. Will not override
	 * preferences.
	 *
	 * @returns The mode.
	 */
	HistoryNavigationMode	GetUseHistoryNavigationMode() { return use_history_navigation_mode; }

	/**
	 * A script can force a certain navigation mode and that is stored here.
	 *
	 * @param[in] The mode a script has demanded.
	 */
	void					SetOverrideHistoryNavigationMode(HistoryNavigationMode mode) { override_history_navigation_mode = mode; }

	/**
	 * A script can force a certain navigation mode and that is stored here.
	 *
	 * @returns The mode a script has forced (or none).
	 */
	HistoryNavigationMode	GetOverrideHistoryNavigationMode() { return override_history_navigation_mode; }

	/** Returns TRUE if the page was reached via history browsing. */
	BOOL GetNavigatedInHistory() { return navigated_in_history; }

	/**
	 * Certain pages does things in their onload/onunload... handlers
	 * that convince us that fast navigation will leave the page in an
	 * unusable state. When such an action is discovered this method
	 * should be called. Note that calling this will disable fast
	 * navigation and give the user the a worse experience when
	 * navigating in history so don't call it lightly.
	 */
	void					SetCompatibleHistoryNavigationNeeded() { compatible_history_navigation_needed = TRUE; }

	BOOL					GetCompatibleHistoryNavigationNeeded() { return compatible_history_navigation_needed; }

	/** Returns TRUE if this document has been reloaded for any reason. */
	BOOL					DocumentIsReloaded() { return document_is_reloaded; }

	/**
	 * To be called when the visibility changes in way that would change the value of
	 * the document.hidden property or document.visibilityState property so that
	 * the visibilitychange event should fire.
	 */
	void					OnVisibilityChanged();

#ifdef SUPPORT_VISUAL_ADBLOCK
	/**
	 * Part of the SUPPORT_VISUAL_ADBLOCK feature.
	 *
	 * Checks if a URL is blocked.
	 *
	 * @param[in] url The URL that should be checked.
	 *
	 * @returns TRUE if the URL had tried to load but been blocked.
	 */
	BOOL GetIsURLBlocked(const URL& url);

	/**
	 * Part of the SUPPORT_VISUAL_ADBLOCK feature.
	 *
	 * This function blocks a url. Will not affect already loaded
	 * urls.
	 *
	 * @param[in] url The URL that should be blocked.
	 */
	void AddToURLBlockedList(const URL& url);

	/**
	 * Part of the SUPPORT_VISUAL_ADBLOCK feature.
	 *
	 * This function unblocks a url. Will not force a reload of it if
	 * it was previously blocked.
	 *
	 * @param[in] url The URL that should no longer be blocked.
	 */
	void RemoveFromURLBlockedList(const URL& url);

	/**
	 * Part of the SUPPORT_VISUAL_ADBLOCK feature.
	 *
	 * This function unblocks all blocked urls, and if invalidate is
	 * TRUE everything blocked will be loaded as well.
	 *
	 * @param[in] invalidate If TRUE will not only unblock but also
	 * start loading of any content previously blocked.
	 */
	void ClearURLBlockedList(BOOL invalidate = FALSE);
#endif // SUPPORT_VISUAL_ADBLOCK

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	/**
	 * Tells the accessibility code that an element is going away.
	 *
	 * @param[in] element The element that the accessibility code
	 * should forget all about.
	 */
	void RemoveAccesibilityHookToElement(HTML_Element* element);
#endif // ACCESSIBILITY_EXTENSION_SUPPORT

	/**
	 * Layout related function. Setting for the layout engine.
	 *
	 * @returns The max width a paragraph should have to be easy to
	 * read. Used in some rendering modes that modifies the layout for
	 * increased readability on narrow screens.
	 */
	short GetMaxParagraphWidth();

	/**
	 * Layout related function. Mark Containers dirty, including
	 * containers in all frame and iframe children.
	 */
	void  MarkContainersDirty();

	/**
	 * Checks if the current document is a site we have deemed
	 * extremely important to not use handheld styles on since it's an
	 * important site with very broken handheld styled.
	 *
	 * @returns TRUE if the site's handheld styles should be ignored.
	 *
	 * @todo Remove in favour of site specific fixes somewhere
	 * else. browser.js?
	 *
	 * @todo Only used internally - make it private? Left as public
	 * since this is part of code that might move to layout some day.
	 */
	BOOL  CheckOverrideMediaStyle();

#ifdef VISITED_PAGES_SEARCH
	/**
	 * Part of the search engine feature. FramesDocument has a handle
	 * to the database that can be retrieved with this function. Only
	 * the root document in a document tree should have one.
	 *
	 * @returns A handle. Can be NULL.
	 */
	VisitedSearch::RecordHandle GetVisitedSearchHandle() { return m_visited_search; }

	/**
	 * Part of the search engine feature. FramesDocument might
	 * or might not index its text content in the search_module.
	 *
	 * @returns TRUE if frame is allowed to index its text content
	 */
	BOOL	AllowFullTextIndexing() { return m_index_all_text; }
#endif // VISITED_PAGES_SEARCH

	HTML_Element*	GetSpotlightedElm() { return m_spotlight_elm.GetElm(); }
	void			SetSpotlightedElm(HTML_Element *elm);

	/**
	 * Returns TRUE if the URL identified by 'url_id' (or, if 'follow_ref' is
	 * TRUE, one that was redirected to the same final redirect target as that
	 * URL) has been used as an inline in this document.
	 *
	 * @param url_id Id of URL.
	 * @param folow_ref If TRUE, really ask about the final redirect target of
	 *                  the URL identified by 'url_id'.
	 * @return TRUE or FALSE.
	 */
	BOOL  IsInline(URL_ID url_id, BOOL follow_ref = TRUE) { return GetInline(url_id, follow_ref) != NULL; }

	/**
	 * Returns the character offset of the first invalid character (according to
	 * the charset decoder) in the document being loaded.  Returns -1 if not
	 * known, if called at an inappropriate time, or if no such characters have
	 * been found yet.  The offset is relative the currently parsed "buffer,"
	 * not an absolute number.
	 */
	int GetFirstInvalidCharacterOffset();

	/**
	 * Returns TRUE if the document doesn't allow loading of a
	 * specific resource.
	 *
	 * If the document is generated by opera we normally assume
	 * that the content does not need to be further checked.
	 * However, if the content in part are external content,
	 * further checking would still be needed (mail compose is an
	 * example of this).
	 *
	 * @param[in] url_type The type of the url that is going to be
	 * loaded, for instance URL_FILE, URL_HTTP or URL_ATTACHMENT.
	 */
	BOOL GetSuppress(URLType url_type);


	/**
	 * Returns TRUE if the document was loaded from an Opera generated URL.
	 */
	BOOL IsGeneratedByOpera() {return origin->is_generated_by_opera;}

	const DocumentOrigin* GetOrigin() { return origin; }

	DocumentOrigin* GetMutableOrigin() { return origin; }

	/**
	 * Utility method to extract the host name from the current url.
	 * Can be used when retrieving site-specific prefs, but
	 * it's even better to send in the full URL (available since core-2.1)
	 *
	 * @returns the hostname or NULL of there is no hostname of the
	 * current url.
	 */
	const uni_char* GetHostName();

	/**
	 * Returns TRUE if this document is the top-level document in its
	 * window.
	 *
	 * @return TRUE or FALSE.
	 */
	BOOL IsTopDocument();

	/**
	 * In a document tree, every document except the root document has
	 * another FramesDocument as parent. This method retrieves that
	 * document, but is really a utility function equivalent to
	 * GetDocManager()->GetParentDoc().
	 *
	 * @returns the parent document or NULL if this is the root document.
	 *
	 * @see DocumentManager::GetParentDoc();
	 *
	 * @todo Inlining it would save a couple of bytes and cpu cycles.
	 */
	FramesDocument*		GetParentDoc();

	/**
	 * Every DocumentManager has a MessageHandler which processes
	 * messages for the document loading. This method is a utility function
	 * for FramesDocument equivalent to
	 * GetDocManager()->GetMessageHandler().
	 *
	 * @returns the MessageHandler connected to the document (DocumentManager).
	 *
	 * @see DocumentManager::GetMessageHandler().
	 *
	 * @todo Inlining it would save a couple of bytes and cpu cycles.
	 */
    MessageHandler*		GetMessageHandler();

	/**
	 * To be called when the document (i.e. the VisualDevice
	 * representing the document) should get focus.
	 *
	 * @param[in] reason What caused the document to get focus. See
	 * the focus system for details, but common values are
	 * FOCUS_REASON_MOUSE and FOCUS_REASON_KEYBOARD.
	 *
	 * @see VisualDevice::SetFocus()
	 */
	void				SetFocus(FOCUS_REASON reason = FOCUS_REASON_OTHER);

	/**
	 * Creates a document context which is used in the external API:s to manipulate
	 * or access information about the current element/caret position/word in
	 * in the document.
	 */
	OpDocumentContext* CreateDocumentContext();


	/**
	 * Creates a document context for a specific screen point. It will use
	 * CreateDocumentContextForElement to create the object after figuring out
	 * what is at the supplied screen_pos.
	 *
	 * @param[in] screen_pos The pixel that the OpDocumentContext should be created for.
	 *
	 * @see CreateDocumentContext()
	 */
	OpDocumentContext* CreateDocumentContextForScreenPos(const OpPoint& screen_pos);

	/**
	 * Creates a document context for a specific document point. Always uses this doc as a context's doc.
	 * If the target element is not provided, finds out what element occupies the given point. If it couldn't
	 * find any, creates a document context with no element associated.
	 *
	 * @param[in] doc_pos The document point that the OpDocumentContext should be created for.
	 * @param[in] target Optional. The element that the OpDocumentContext should be created for.
	 *					 If provided, then one of its layout boxes must contain the doc_pos.
	 * @returns New document context or NULL in case of OOM.
	 *
	 * @see CreateDocumentContext()
	 */
	OpDocumentContext* CreateDocumentContextForDocPos(const OpPoint& doc_pos, HTML_Element* target = NULL);

	/**
	 * Let the platform know that now would be a good time to show a context menu
	 * and give it enough information so that it can do that.
	 *
	 * Used for core initiated popup menus(for example results of handling mouse input).
	 *
	 * @param[in] target The element that was the target of the context menu operation.
	 *
	 * @param[in] offset_x The x coordinate relative the target's upper left corner. In document scale.
	 * @param[in] offset_y The y coordinate relative the target's upper left corner. In document scale.
	 * @param[in] document_x The x coordinate relative the document's upper left corner. In document scale.
	 * @param[in] document_y The y coordinate relative the document's upper left corner. In document scale.
	 * @param[in] has_keyboard_origin The flag set to true when request originates from the keyboard event.
	 */
	void	InitiateContextMenu(HTML_Element* target,
						  int offset_x, int offset_y,
						  int document_x, int document_y,
						  BOOL has_keyboard_origin);

	/**
	 * Let the platform know that now would be a good time to show a context menu
	 * and give it enough information so that it can do that. Context menu triggered at
	 * currently focused element.
	 *
	 * Used by WindowCommander::RequestPopupMenu to explicitly request a popup menu
	 * without specified screen position. For core initiated (for example as a result of
	 * handling mouse input) use the version with HTML_Element* parameter.
	 *
	 */
	void	InitiateContextMenu();

	/**
	 * Let the platform know that now would be a good time to show a context menu
	 * and give it enough information so that it can do that. Context menu triggered at
	 * specified screen position.
	 *
	 * Used by WindowCommander::RequestPopupMenu to explicitly request a popup menu
	 * at specified screen position. For core initiated (for example as a result of
	 * handling mouse input) use the version with HTML_Element* parameter.
	 *
	 * @param[in] screen pos at which the context menu is triggered.
	 */
	void	InitiateContextMenu(const OpPoint& screen_pos);

	/**
	 * Basically OpWindowCommander::LoadImage. See its documentation.
	 */
	void LoadImage(OpDocumentContext& ctx, BOOL disable_turbo=FALSE);

	/**
	 * Basically OpWindowCommander::SaveImage. See its documentation.
	 */
	OP_STATUS SaveImage(OpDocumentContext& ctx, const uni_char* filename);

	/**
	* Basically OpWindowCommander::OpenImage. See its documentation.
	*/
	void OpenImage(OpDocumentContext& ctx, BOOL new_tab=FALSE, BOOL in_background=FALSE);

#ifdef RESERVED_REGIONS
	/**
	 * Check if a DOM event type is associated with reserved regions.
	 *
	 * Reserved regions are document areas where selected input events will
	 * trigger scripts. The event selection is made by platforms at compile
	 * time and will typically only include touch events.
	 *
	 * This function checks if the given type is selected.
	 *
	 * @See FEATURE_RESERVED_REGIONS, which exists to allow platform such
	 * as bream limit their interaction with Core in order to improve user
	 * perceived input latency.
	 *
	 * @param type Event type.
	 *
	 * @return TRUE if this event type defines reserved regions.
	 */
	static BOOL IsReservedRegionEvent(DOM_EventType type);

	/**
	 * Do we have any event handlers that cause a reserved region to be established?
	 *
	 * Some event handlers cause the element(s) they are set on to become a
	 * "reserved region", which is a mechanism to tell the UI to pass mouse
	 * events in that area directly to core, and not do zooming, panning or
	 * other things on its own. This method will return TRUE if there's at
	 * least one such event handler.
	 */
	BOOL HasReservedRegionHandler();

	/**
	 * Get the reserved region for this document.
	 *
	 * The reserved region will consist of areas occupied by boxes that have
	 * certain event handlers, or areas occupied by boxes that are of such a
	 * type that they should constitute a reserved region on their own (paged
	 * containers, for instance). If the document has a neighborhood, the
	 * entire document becomes a reserved region.
	 *
	 * The UI should pass mouse events that occur inside the reserved region
	 * directly to core, rather than performing actions (pan, zoom, etc.) on
	 * its own.
	 */
	OP_STATUS GetReservedRegion(const OpRect& search_rect, OpRegion& region);

	/**
	 * Notify platforms of dom changes that affect reserved regions.
	 *
	 * Reserved regions are document areas where selected input events will
	 * trigger scripts. The event selection is made by platforms at compile
	 * time and will typically only include touch events.
	 *
	 * @See FEATURE_RESERVED_REGIONS, which exists to allow platform such
	 * as Bream limit their interaction with Core in order to improve user
	 * perceived input latency.
	 *
	 * The intention is for this signal to be sent when an element has had
	 * a handler of a selected (reserved) type added or removed, such that
	 * the platform can schedule a later call to
	 * OpViewportController::GetReservedRegion and obtain a refreshed list
	 * of these document areas.
	 */
	void SignalReservedRegionChange();
#endif // RESERVED_REGIONS

#ifdef PAGED_MEDIA_SUPPORT
	/**
	 * Notify the DOM and UI about page changes on the specified element.
	 *
	 * @param element The element that establishes the paged container
	 * @param current_page Current page number now. 0 is the first page.
	 * @param page_count Page count now.
	 * @return ERR_NO_MEMORY on OOM, ERR on other errors (e.g. if scripting is
	 * disabled), OK on success.
	 */
	OP_STATUS SignalPageChange(HTML_Element* element, unsigned int current_page, unsigned int page_count);
#endif // PAGED_MEDIA_SUPPORT

#ifdef USE_OP_CLIPBOARD
	/**
	 * Basically OpWindowCommander::CopyImageToClipboard. See its documentation.
	 */
	BOOL CopyImageToClipboard(OpDocumentContext& ctx);

	/**
	 * Basically OpWindowCommander::CopyBGImageToClipboard. See its documentation.
	 */
	BOOL CopyBGImageToClipboard(OpDocumentContext& ctx);

	/**
	 * Gets the clipboard listener of this document.
	 *
	 * @see ClipboardListener
	 */
	ClipboardListener* GetClipboardListener() { return GetWindow(); }
#endif // USE_OP_CLIPBOARD

#ifdef WIC_TEXTFORM_CLIPBOARD_CONTEXT
	/**
	 * Basically OpWindowCommander::CopyTextToClipboard. See its documentation.
	 */
	void CopyTextToClipboard(OpDocumentContext& ctx);

	/**
	 * Basically OpWindowCommander::CutTextToClipboard. See its documentation.
	 */
	void CutTextToClipboard(OpDocumentContext& ctx);

	/**
	 * Basically OpWindowCommander::PasteTextFromClipboard. See its documentation.
	 */
	void PasteTextFromClipboard(OpDocumentContext& ctx);
#endif // WIC_TEXTFORM_CLIPBOARD_CONTEXT
	/**
	 * Basically OpWindowCommander::ClearText. See its documentation.
	 */
	void ClearText(OpDocumentContext& ctx);

#ifdef WIC_MIDCLICK_SUPPORT
	/**
	 * Let the platform know that now would be a good time to show a midclick menu
	 * and give it enough information so that it can do that.
	 *
	 * @param[in] target The element that was the target of the midclick menu operation.
	 *
	 * @param[in] offset_x The x coordinate relative the target's upper left corner. In document scale.
	 * @param[in] offset_y The y coordinate relative the target's upper left corner. In document scale.
	 * @param[in] document_x The x coordinate relative the document's upper left corner. In document scale.
	 * @param[in] document_y The y coordinate relative the document's upper left corner. In document scale.
	 */
	void HandleMidClick(HTML_Element* target,
			  int offset_x, int offset_y,
			  int document_x, int document_y,
			  ShiftKeyState modifiers, BOOL mousedown);
#endif // WIC_MIDCLICK_SUPPORT

	/**
	 * Removes all references to |element| so that it can be safely
	 * deleted without any risk for crashes.
	 *
	 * @param[in] element The element doc must forget all
	 * about since it will go away.
	 */
	void CleanReferencesToElement(HTML_Element* element);

#ifdef WEBFEEDS_DISPLAY_SUPPORT
	/**
	 * Checks if current document is an inline RSS feed.
	 * @return TRUE if document is an inline feed.
	 */
	BOOL IsInlineFeed() { return is_inline_feed; }
#endif // WEBFEEDS_DISPLAY_SUPPORT

#ifdef PLUGIN_AUTO_INSTALL
	/**
	 * Called from WindowCommander when it is notified about a plugin
	 * becoming available for installation by the platform missing plugin
	 * manager.
	 *
	 @param[in] mime_type The mimetype of the plugin that became available
	 */
	void OnPluginAvailable(const uni_char* mime_type);
#endif // PLUGIN_AUTO_INSTALL

#ifdef _PLUGIN_SUPPORT_
	/**
	 * Called from WindowCommander when it is notified about a newly detected
	 * plugin. This function is necessary for async plug-in detection to work
	 * as intended.
	 */
	void OnPluginDetected(const uni_char* mime_type);

	/**
	 * Store mime-type for later reference. Used by OnPluginDetected() when
	 * there are no missing plug-in listeners (usually caused by parsing an
	 * object-tag). This function may non-fatally fail on OOM. If we can't
	 * store the mime-type, we'll just have to live without being able to
	 * auto-refresh the document.
	 */
	void StoreMissingMimeType(const OpStringC& mime_type);
#endif // _PLUGIN_SUPPORT_

	/**
	 * Every FramesDocument is owned by a DocumentManager (and every
	 * HTML_Document is owned by a FramesDocument that is owned by a
	 * DocumentManager). This returns a pointer to that
	 * DocumentManager.
	 *
	 * @returns A pointer to the DocumentManager owning this
	 * FramesDocument (or the DocumentManager owning the
	 * FramesDocument owning this HTML_Document).
	 *
	 * @see DocumentManager
	 */
	DocumentManager*	GetDocManager() { return doc_manager; }

	/**
	 * Every document tree hangs on a DocumentManager owned by a Window.
	 * This is a utility function to retrieve that Window and
	 * is equivalent to GetDocManager()->GetWindow().
	 *
	 * @returns the Window that ultimately owns this document. Never NULL.
	 *
	 * @see DocumentManager::GetWindow()
	 */
	Window*				GetWindow() { return doc_manager->GetWindow(); }

	/**
	 * Every DocumentManager has a VisualDevice that it uses to
	 * draw and interact with the documents. This is a utility function
	 * to extract that VisualDevice pointer. It's equivalent to
	 * GetDocManager()->GetVisualDevice().
	 *
	 * @returns the VisualDevice object
	 *
	 * @see DocumentManager::GetVisualDevice();
	 */
	VisualDevice*		GetVisualDevice() { return doc_manager->GetVisualDevice(); }

	/**
	 * Returns the referrer used when requesting this document
	 *
	 * @returns The referrer URL used in for the request that resulted in
	 *          this document, or an empty DocumentReferrer if no referrer
	 *           was used or no such information is available.
	 */
	DocumentReferrer	GetRefURL();

	/**
	 * Returns the URL for this document.
	 *
	 * @returns The URL which is empty if there was none.
	 */
	URL&				GetURL() { return url; }

	/**
	 * Returns the security URL for this document.
	 *
	 * @returns The URL which is empty if there was none.
	 */
	URL&				GetSecurityContext() { return origin->security_context.IsEmpty() ? url : origin->security_context; }

	/*
	 * Get the top document in a document tree. FramesDocuments exist
	 * in a treelike fashion in documents with iframes and
	 * framesets. This method looks which FramesDocument is the root
	 * in that tree and returns that FramesDocument.
	 *
	 * @returns The root document. Never NULL.
	 */
	FramesDocument* GetTopDocument();

	/**
	 * This function returns TRUE or FALSE whether the url pointed by
	 * link_url should be considered visited or not, when deciding
	 * whether to apply the :visited pseudo class or not
	 *
	 * @param link_url              the url from the link
	 * @param frames_doc            document which URL shall be used for context if visited_links_state has value 1. Can be NULL
	 * @param visited_links_state   value of the preference PrefsCollectionDisplay::VisitedLinksState, optional.
	 *                              If -1 is passed the preference will be read directly.
	 */
	static BOOL			IsLinkVisited(FramesDocument *frames_doc, const URL& link_url, int visited_links_state = -1);

	/**
	 * Changes the url of the document.
	 *
	 * @param[in] new_url The new url to replace the current url.
	 */
	void				SetUrl(const URL& new_url);

	friend class LoadingImageElementIterator;

#ifdef PLUGIN_AUTO_INSTALL
	/**
	* Will propagate the information about missing plugin for the given mimetype up to DocumentDesktopWindow
	* in order to display the proper infobars informing user that a plugin is available for download.
	* See DSK-312517.
	*
	* @param[in] a_mime_type The mimetype of content that is missing a plugin viewer.
	*/
	void NotifyPluginMissingForMimetype(const OpString& a_mime_type);
#endif // PLUGIN_AUTO_INSTALL

	/**
	 * Gets total number of frames added to a page.
	 * This is only meaningful for the top level document
	 */
	int GetNumFramesAdded()
	{
		OP_ASSERT(IsTopDocument());
		return num_frames_added;
	}

	/**
	 * Gets total number of frames removed from a page.
	 * This is only meaningful for the top level document
	 */
	int GetNumFramesRemoved()
	{
		OP_ASSERT(IsTopDocument());
		return num_frames_removed;
	}

	/**
	 * Increases a numer of frames added to a page.
	 * This is only meaningful for the top level document.
	 */
	void OnFrameAdded()
	{
		OP_ASSERT(IsTopDocument());
		++num_frames_added;
	}

	/**
	 * Decreases a number of frames removed from a page.
	 * This is only meaningful for the top level document.
	 */
	void OnFrameRemoved()
	{
		OP_ASSERT(IsTopDocument());
		++num_frames_removed;
		OP_ASSERT(num_frames_added - num_frames_removed >= 0);
	}

	/**
	 * Checks whether a content of a document may be changed e.g.
	 * by loading a new URL or by scripts via innerHTML or document.write().
	 *
	 * This is used for a frame number limit implementation. In frames above the limit
	 * about:blank is loaded and it's not allowed to change the content of such a frame.
	 */
	BOOL IsContentChangeAllowed();

#ifdef ON_DEMAND_PLUGIN
	/**
	 * Checks whether we should use a plugin placeholder for the html element.
	 *
	 * @param[in] html_element The element that is being checked.
	 */
	BOOL IsPluginPlaceholderCandidate(HTML_Element* html_element);

private:
	/**
	 * Checks if this, parent or child documents contain plugin placeholders.
	 *
	 * @return TRUE if any of the documents in document tree contains plugin
	 * placeholders. FALSE if none of the documents does.
	 */
	BOOL DocTreeHasPluginPlaceholders();

public:
	/**
	 * Adds a placeholder element to the list of placeholders known to this
	 * document. Also notifies listeners about the change.
	 *
	 * In case of OOM, element will not be added to the list of placeholders
	 * known to this document in which case it won't be possible to activate
	 * it using ACTION_ACTIVATE_ON_DEMAND_PLUGINS action but it will still
	 * be possible to activate it by clicking.
	 *
	 * @param[in] placeholder The element that became a plug-in placeholder.
	 */
	void RegisterPluginPlaceholder(HTML_Element* placeholder);

	/**
	 * Removes a placeholder element from the list of placeholders known to this
	 * document. Also notifies listeners about the change.
	 *
	 * @param[in] placeholder The element that is no longer a placeholder.
	 */
	void UnregisterPluginPlaceholder(HTML_Element* placeholder);
#endif // ON_DEMAND_PLUGIN

#ifdef _PLUGIN_SUPPORT_
	/**
	 * The listeners list, for holding EmbedContent objects that registered via AddPluginInstallationListener to be
	 * notified about plugin availability.
	 */
	OpListeners<PluginInstallationListener>	m_plugin_install_listeners;

	/**
	 * Will register another listener that will be notified about plugin availability via the
	 * PluginInstallationListener interface. To be used by EmbedContent.
	 *
	 * @param[in] listener The object that wants to be notified.
	 */
	OP_STATUS AddPluginInstallationListener(PluginInstallationListener* listener)
	{
		return m_plugin_install_listeners.Add(listener);
	}

	/**
	 * Will unregister the given object from the listeners list.
	 *
	 * @param[in] listener The object that wants to unregister
	 */
	OP_STATUS RemovePluginInstallationListener(PluginInstallationListener* listener)
	{
		return m_plugin_install_listeners.Remove(listener);
	}
#endif // _PLUGIN_SUPPORT_

#ifdef PLUGIN_AUTO_INSTALL
	/**
	* Notify the platform missing plug-in manager about a newly detected plug-in.
	*
	* @param[in] mime_type The mimetype of content that just became available.
	*/
	void NotifyPluginDetected(const OpString& mime_type);

	/**
	* Will propagate the information about missing plugin for the given mimetype to WindowCommander,
	* in order for the platform missing plugin manager to receive it.
	*
	* @param[in] a_mime_type The mimetype of content that is missing a plugin viewer.
	*/
	void NotifyPluginMissing(const OpStringC& a_mime_type);

	/**
	* Will get information about the plugin for mime_type from the platform missing plugin manager,
	* returning the plugin name and its availability for installation.
	*
	* @param[in] a_mime_type The mimetype that we are missing a plugin for.
	* @param[out] plugin_name The plugin name from platform manager
	* @param[out] available Status of the plugin's availability for installation, from the platform manager.
	*/
	void RequestPluginInfo(const OpString& mime_type, OpString& plugin_name, BOOL &available);

	/**
	* Will request the platform plugin installation to start.
	*
	* @param[in] a_mime_type The mimetype that we are missing a plugin for.
	*/
	void RequestPluginInstallation(const PluginInstallInformation& information);
#endif // PLUGIN_AUTO_INSTALL

	// AsyncLoadInlineListener's interface
	void OnInlineLoad(AsyncLoadInlineElm* async_inline_elm, OP_LOAD_INLINE_STATUS status);

	/**
	 * Informs all documents in this window (beginning from the top one) that
	 * some element in another document (within the same window) got focused.
	 *
	 * @param<in> got_focus - the document containing the focused element.
	 */
	void OnFocusChange(FramesDocument* got_focus);

	/**
	 * Create search URL. See OpWindowCommander::CreateSearchURL() for details.
	 */
	static OP_STATUS CreateSearchURL(OpDocumentContext &context, OpString8 &url, OpString8 &query, OpString8 &charset, BOOL &is_post);


	/**
	 * Marks that document supports given view_mode. The document
	 * is considered to support given mode if it explicitly adjusts it's content
	 * to this mode using view-mode media queries.
	 */
	void SetViewModeSupported(WindowViewMode view_mode) { m_supported_view_modes = m_supported_view_modes | view_mode; }

	/**
	 * Returns whether document supports given view_mode. The document
	 * is considered to support given mode if it explicitly adjusts it's content
	 * to this mode using view-mode media queries.
	 */
	BOOL IsViewModeSupported(WindowViewMode view_mode) { return (m_supported_view_modes & view_mode) != 0; }

	/**
	 * Tell the UI about changes to the url of the top level window and update
	 * some other cached urls.
	 *
	 * @param[in] new_url The url of this FramesDocument.
	 */
	void			NotifyUrlChanged(URL new_url);

	/**
	 * Get the values to be returned for Window.innerWidth and Window.innerHeight
	 * in javascript. This method is used from JS_Window::GetName.
	 *
	 * @param[out] inner_width The returned value for innerWidth.
	 * @param[out] inner_height The returned value for innerHeight.
	 */
	void DOMGetInnerWidthAndHeight(unsigned int& inner_width, unsigned int& inner_height);

	/**
	 * Get scroll position. A script is asking.
	 */
	OpPoint DOMGetScrollOffset();

	/**
	 * Set scroll position as requested by a script.
	 *
	 * @param x If non-NULL, the new X position. If NULL, leave current X position as is.
	 * @param y If non-NULL, the new Y position. If NULL, leave current Y position as is.
	 */
	void DOMSetScrollOffset(int* x, int* y);

#ifdef SCOPE_PROFILER
	/**
	 * Get the OpProbeTimeline for this document.
	 *
	 * @return The timeline if this document is currently being profiled, or NULL
	 *         if it is not.
	 */
	OpProbeTimeline *GetTimeline() { return doc_manager->GetTimeline(); }
#endif // SCOPE_PROFILER

	/** Sets the auxiliary URL for which the loading messages will be handled by this document.
	 * This is needed by history.pushState()/replaceState() because those functions change the current URL
	 * but loading of it must be finished anyway.
	 * Make sure you know what you're doing before using this!
	 */
	void SetAuxUrlToHandleLoadingMessagesFor(URL& url) { aux_url_to_handle_loading_messages_for = url; }

	/** Clears the auxiliary URL. Should be called when loading of aux_url_to_handle_loading_messages_for URL
	 * was finished.
	 * @see SetUrlToHandleLoadingMessagesFor()
	 */
	void ClearAuxUrlToHandleLoadingMessagesFor() { aux_url_to_handle_loading_messages_for = URL(); }

	/** Get the auxiliary URL
	 *
	 * @see SetUrlToHandleLoadingMessagesFor()
	 */
	URL& GetAuxUrlToHandleLoadingMessagesFor() { return aux_url_to_handle_loading_messages_for; }


	/** @short Return the rect describing the element captured by the context.
	 *
	 * Note that #rect might be modified even in the case where the function fails.
	 *
	 * @param context The context describing the element
	 * @param[out] rect The rectangle containing the element in screen coordinates
	 * @return
	 *      OpStatus::OK if the rect was successfully fetched and is available in #rect
	 *      OpStatus::ERR if the rect was not accessible. The values in #rect does not
	 *      represent the elements rectangle.
	 */
	static OP_STATUS GetElementRect(OpDocumentContext &context, OpRect &rect);

	/** Returns FramesDocElm containing the passed document or NULL if the passed document is not a subdocument of this document.*/
	FramesDocElm* GetFrmDocElmByDoc(FramesDocument* doc);

#ifndef MOUSELESS
	/**
	 * If the mouse has at any time moved too far from the mouse down position this will return FALSE
	 */
	BOOL GetMouseUpMayBeClick() { return m_mouse_up_may_still_be_click; }

	/**
	 * Reset back the status of the ongoing mouse interaction to possibly being part of a click.
	 */
	void ResetMouseUpMayBeClick() { m_mouse_up_may_still_be_click = TRUE; }
#endif // !MOUSELESS

	/**
	 * Checks if for the given target and the related target in the given document there's a need to fire the given event
	 * e.g. if anyone listens to it.
	 */
	static BOOL NeedToFireEvent(FramesDocument *doc, HTML_Element *target, HTML_Element *related_target, DOM_EventType type);

#ifdef GEOLOCATION_SUPPORT
	/** MUST be called whenever a script in this document starts accessing geolocation data.
	 *  Triggers Window::NotifyGeolocationAccess when increasing the use count from 0 to 1.
	 */
	void IncGeolocationUseCount();
	/** MUST be called whenever a script in this document ends accessing geolocation data.
	 *  Triggers Window::NotifyGeolocationAccess when decreasing the use count from 1 to 0.
	 */
	void DecGeolocationUseCount();
	/** Obtains number of active geolocation users in this document. */
	unsigned int GetGeolocationUseCount() { return geolocation_usage_count; }
#endif // GEOLOCATION_SUPPORT

#ifdef MEDIA_CAMERA_SUPPORT
	/** MUST be called whenever a script in this document starts accessing camera.
	 *  Triggers Window::NotifyCameraAccess when increasing the use count from 0 to 1.
	 */
	void IncCameraUseCount();
	/** MUST be called whenever a script in this document ends accessing camera.
	 *  Triggers Window::NotifyCameraAccess when decreasing the use count from 1 to 0.
	 */
	void DecCameraUseCount();
	/** Obtains number of active camera users in this document. */
	unsigned int GetCameraUseCount() { return camera_usage_count; }
#endif // MEDIA_CAMERA_SUPPORT

	/**
	 * Part of the "Wait for styles before showing the page"
	 * feature.
	 *
	 * @returns TRUE if the document should not be painted/reflowed
	 * since we're waiting for important data (CSS/XSLT stylesheets).
	 */
	BOOL			IsWaitingForStyles() const { return wait_for_styles > 0; }

	/** Increases the stylesheet load counter. */
	void			IncWaitForStyles();

	/** Decreases the stylesheet load counter. */
	void			DecWaitForStyles();
};

#ifdef URL_FILTER
/// Class that provides context information to content_filter extension classes
class URLFilterDOMListenerOverride: public URLFilterExtensionListenerOverride
{
private:
	/// Real context provided by Docxs
	DOMLoadContext real_ctx;

public:
	URLFilterDOMListenerOverride(DOM_Environment *env, HTML_Element *element): real_ctx(env, element)
	{ }

	virtual void URLBlocked(const uni_char* url, OpWindowCommander* wic, DOMLoadContext *dom_ctx)
	{
		OP_ASSERT(!dom_ctx);

		if (GetOriginalListener())
			GetOriginalListener()->URLBlocked(url, wic, &real_ctx);
	}

	virtual void URLUnBlocked(const uni_char* url, OpWindowCommander* wic, DOMLoadContext *dom_ctx)
	{
		OP_ASSERT(!dom_ctx);

		if (GetOriginalListener())
			GetOriginalListener()->URLUnBlocked(url, wic, &real_ctx);
	}

#ifdef SELFTEST
	virtual void URLAllowed(const uni_char* url, OpWindowCommander* wic, DOMLoadContext *dom_ctx)
	{
		OP_ASSERT(!dom_ctx);

		if (GetOriginalListener())
			GetOriginalListener()->URLAllowed(url, wic, &real_ctx);
	}
#endif // SELFTEST
};
#endif // URL_FILTER

/* gcc 3.3 can't handle deprecated inline; so separate DEPRECATED declaration
 * from inline definition:
 */

inline LayoutMode FramesDocument::GetCurrentLayoutMode() { return layout_mode; }
inline void FramesDocument::NotifyIntersectionViews(HTML_Element* parent, int dx, long dy) {}

/**
 * Used by the dom code to keep images with onload handlers alive.
 */
class LoadingImageElementIterator
{
private:
	LoadInlineElmHashIterator m_lie_iterator;
	HEListElm* m_next_helistelm;
	BOOL m_started;

public:
	LoadingImageElementIterator(FramesDocument* frames_doc) :
	  m_lie_iterator(frames_doc->inline_hash),
	  m_next_helistelm(NULL),
	  m_started(FALSE)
	{}

	/**
	 * Returns the next HTML_Element that is loading something with
	 * type IMAGE_INLINE (not other image types) or NULL.
	 */
	HTML_Element* GetNext();
};

#ifdef NEARBY_INTERACTIVE_ITEM_DETECTION
/**
 * Item used by the FrameDocHitTestIterator class.
 */
struct FrameDocHitTestItem
{
private:
	/** Not implemented. */
	FrameDocHitTestItem(const FrameDocHitTestItem& other);

	/** Not implemented. */
	FrameDocHitTestItem& operator=(const FrameDocHitTestItem& other);

public:
	/** Frames document currently pointed out by iterator. */
	FramesDocument* doc;

	/** Rectangle in doc's coordinates that can still be used for hit testing.

	    It may be partially outside the visible area of the document. That
	    outer part can overlap with the scrollbar(s) of the frame that this
	    document is inside. */
	OpRect rect;

	/** Affine position of doc in relation to the top left corner of the
	    top document. */
	AffinePos pos;

	FrameDocHitTestItem() : doc(NULL) {}
};

/**
 * Iterates down the frame/iframe document hierarchy finding documents
 * overlapping the specified hit test area. It starts by testing a high-level
 * document and walks down the tree each iteration until the last matching leaf
 * is found.
 */
class FrameDocHitTestIterator
{
private:
	/** Current iterator item. */
	FrameDocHitTestItem current;

	/** Next frames document element to do hit testing on. */
	FramesDocElm* next;

public:
	/**
	 * Constructs a frame document hit test iterator. Initially the iterator
	 * will point at the specified parent frames document, not the first
	 * matching hit. The first matching hit will be reached (if possible) after
	 * the first call to Advance().
	 *
	 * @param [in] doc Parent document hosting the frames/iframes to iterate.
	 * @param [in] rect Rectangle for hit testing.
	 * @param [in] pos Affine position of doc in relation to the top left
	 *                 corner of the top document.
	 */
	FrameDocHitTestIterator(FramesDocument* doc, const OpRect& rect, const AffinePos& pos);

	/**
	 * @return The current item the iterator points at.
	 */
	const FrameDocHitTestItem& Current() const { return current; }

	/**
	 * If possible, advances the iterator one step.
	 * @return If the iterator was advanced TRUE is returned. If not, the end
	 *         has been reached and the iterator points at the last item; in
	 *         this case FALSE is returned.
	 */
	BOOL Advance();
};
#endif /* NEARBY_INTERACTIVE_ITEM_DETECTION */

#endif /* _FRAMES_DOCUMENT_ */
