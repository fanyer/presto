/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef HTMLOGDOCREP_H
#define HTMLOGDOCREP_H

#include "modules/logdoc/logdocenum.h"
#include "modules/logdoc/link.h"
#include "modules/doc/css_mode.h"
#include "modules/doc/doctypes.h"
#include "modules/ecmascript_utils/esloadman.h"
#include "modules/img/image.h"
#include "modules/logdoc/namespace.h"
#include "modules/logdoc/src/helist.h"
#include "modules/logdoc/elementref.h"
#include "modules/style/css_collection.h"

#include "modules/ecmascript/ecmascript.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/logdoc/html.h"
#include "modules/logdoc/src/html5/html5base.h"
#include "modules/url/url2.h"

#ifdef PAGED_MEDIA_SUPPORT
# include "modules/style/css_media.h"
#endif // PAGED_MEDIA_SUPPORT

#ifdef FONTSWITCHING
# include "modules/display/fontdb.h"
#endif // FONTSWITCHING

#ifdef DISPLAY_WRITINGSYSTEM_HEURISTIC
# include "modules/display/writingsystemheuristic.h"
#endif // DISPLAY_WRITINGSYSTEM_HEURISTIC

#if defined ACCESS_KEYS_SUPPORT || defined LOGDOC_GETSCRIPTLIST
# include "modules/util/adt/opvector.h"
#endif // ACCESS_KEYS_SUPPORT || LOGDOC_GETSCRIPTLIST

#include "modules/xmlutils/xmlnames.h"

#ifdef CSS_SCROLLBARS_SUPPORT
# include "modules/widgets/OpScrollbar.h"
#endif // CSS_SCROLLBARS_SUPPORT

#ifdef DELAYED_SCRIPT_EXECUTION
# include "modules/logdoc/htm_lex.h"
#endif // DELAYED_SCRIPT_EXECUTION

class HtmlAttrEntry;
class LogicalDocument;
class FramesDocument;
class HTML_Element;

#ifdef _WML_SUPPORT_
class WML_Context;
#endif // _WML_SUPPORT_

class CSS_Properties;
class CSS;

class VisualDevice;
class XMLDoctype;

class XMLDocumentInformation;
class LinkElement;

class LayoutWorkplace;

typedef OP_STATUS OP_HEP_STATUS;

/**
 * Constants used as return values from HLDocProfile::InsertElement and friends.
 */
class HEParseStatus : public OpStatus
{
public:
	enum
	{
		STOPPED	= USER_SUCCESS // no more data in buffer
	};
};

#ifdef ACCESS_KEYS_SUPPORT
typedef	AutoDeleteHead	AccessKeyTransaction;
#endif // ACCESS_KEYS_SUPPORT

class HLDocProfile
{
private:


	BOOL			is_out_of_memory;

#ifdef _PRINT_SUPPORT_
	HEList			print_image_list;		// A list of image inline elements (HEListElm) used when printing
#endif

	URL*			base_url;
	URL*			real_url;

	OpString		base_url_string;

	URL*			refresh_url;
	short			refresh_seconds;

	AutoNullElementRef	body_he;

	LogicalDocument* hldoc;
	int				form_elm_count;
	int				embed_elm_count;

	KeepAliveElementRef	form; ///< This is the "form pointer" from the HTML5 parser specification. It informs the parser of what form element is currently active or NULL if none is active.
	int				form_count;

	int			text_count; ///< Count characters in textnodes, used in a heuristic for checking if enough content has been loaded from a page to do a first paint.

	BOOL			is_frames_doc;

    BOOL            embed_not_supported;

	BOOL			handle_script; // used to block script handling in innerHTML, outerHTML or insertAdjacentHTML

	char*			character_set;

#ifdef CSS_SCROLLBARS_SUPPORT
	ScrollbarColors scrollbar_colors;
#endif

	CSSCollection	css_collection;
	CSSMODE			css_handling;

	BOOL			unloaded_css;

    BOOL            is_xhtml;

	short			has_media_style;

	LinkElementDatabase link_list;

	VisualDevice*	vis_dev;
	BOOL			new_style_info;

	FramesDocument*	frames_doc;

	ES_LoadManager	es_load_manager;
	BOOL			es_script_not_supported;
	BOOL			running_script_is_external;

#ifdef DELAYED_SCRIPT_EXECUTION
	BOOL			es_delay_scripts;
	BOOL			es_is_parsing_ahead;
	BOOL			es_is_executing_delayed_script;
	BOOL			es_is_starting_delayed_script;
	BOOL			es_need_recover;

	class DelayedScriptElm : public Link
	{
	public:
		class OutOfOrderElement : public ListElement<OutOfOrderElement>
		{
		public:
			OutOfOrderElement(HTML_Element* elm) : element(elm) {}
			AutoNullElementRef element;
		};

		HTML_Element* GetFirstParseAheadElement();

		~DelayedScriptElm();

		AutoNullOnDeleteElementRef script_element, current_element;
		unsigned stream_position;
		int form_nr;
		BOOL form_was_closed;
		BOOL is_ready, no_current_form, has_content, has_added_form, has_restored_state;
		BOOL has_completed;  // the script element has actually finished (but scripts it inserted may still cause recovery)
		HTML5ParserState parser_state;
		/** In HTML 5 elements are not necessarily inserted in the same order
		 *  as they exist in the tree (foster parenting may insert the element
		 *  further up the tree).  To make sure these elements are inserted correctly
		 *  after a delayed script (ESInsertElement) we keep a list of the elements
		 *  which are not in their "normal" position in the tree, and insert
		 *  them individually when needed.
		 */
		AutoDeleteList<OutOfOrderElement> out_of_order_parse_ahead_inserted_elements;
	};

	Head es_delayed_scripts;
#endif // DELAYED_SCRIPT_EXECUTION

#ifdef _WML_SUPPORT_
	AutoNullElementRef	wml_card_elm;
# ifdef ACCESS_KEYS_SUPPORT
	OpKey::Code		m_next_do_key;
# endif // ACCESS_KEYS_SUPPORT
#endif //_WML_SUPPORT_

#ifdef _CSS_LINK_SUPPORT_
	AutoDeleteHead	css_links;
	URLLink*		current_css_link;
	URLLink*		next_css_link;
#endif

    uni_char*       meta_description;

	BOOL			is_parsing_inner_html;

#ifdef CSS_ERROR_SUPPORT
	int				m_css_error_count;
#endif // CSS_ERROR_SUPPORT

#ifdef LOGDOC_GETSCRIPTLIST
	OpAutoVector<URL>	script_urls;
#endif // LOGDOC_GETSCRIPTLIST

#ifdef FONTSWITCHING
	OpFontInfo::CodePage preferred_codepage;
	WritingSystem::Script m_preferred_script;
# ifdef DISPLAY_WRITINGSYSTEM_HEURISTIC
	WritingSystem::Script m_guessed_script;
	WritingSystemHeuristic m_writing_system_heuristic;
	BOOL use_text_heuristic;
# endif // DISPLAY_WRITINGSYSTEM_HEURISTIC
#endif

	BOOL				handheld_callback_called;
	BOOL				doctype_considered_handheld;

#ifdef USE_HTML_PARSER_FOR_XML
	XMLNamespaceDeclaration::Reference
						m_current_ns_decl;
#endif // USE_HTML_PARSER_FOR_XML

	/**
	 * @param[out]	modified_text_elm The element that was modified by the inserted text. May be a new element or an old one. Only relevant for text.
	 */
	OP_HEP_STATUS
		InsertElementInternal(HTML_Element *parent, HTML_Element *new_elm, const uni_char* text, unsigned text_len, BOOL resolve_entites, BOOL as_cdata, BOOL expand_wml_vars, HTML_Element** modified_text_elm, HTML_Element* insert_before = NULL);

#ifdef DELAYED_SCRIPT_EXECUTION
	/**
	 * When a delayed script finished this function will "insert" (really change
	 * their status from PARSE_AHEAD to NOT_INSERTED) the elements following the
	 * script until the next script.
	 * @param delayed_script  The delayed script which just finished.
	 * @param from  The first element which hasn't been inserted yet.
	 * @param to  The last element to insert.
	 * @return ERR_NO_MEMORY on OOM, otherwise OK.
	 */
	OP_STATUS	ESInsertElements(DelayedScriptElm* delayed_script, HTML_Element *from, HTML_Element *to);
#endif // DELAYED_SCRIPT_EXECUTION

	/**
	 * Check the parent of the element and determine if the text element is to
	 * be displayed. This method should not be called for orphan text elements.
	 * @param element The text element to check
	 * @param[out] is_title Is set to TRUE if element is a descendent
	 * of a title element.
	 */
	BOOL IsTextElementVisible(HTML_Element *element, BOOL *is_title);

public:

	                HLDocProfile();
					~HLDocProfile();

	/** Set base URL.
	 *
	 * @param url The URL object to be set as the base URL.
	 * @param url_string The original url string argument has been created from.
	 * May be NULL if not available.
	 */
	void			SetBaseURL(const URL* url, const uni_char* url_string = NULL);
	void			SetURL(const URL* url);
	void			SetLogicalDocument(LogicalDocument* logdoc) { hldoc = logdoc; }
	LogicalDocument*	GetLogicalDocument() { return hldoc; }
	void			SetBodyElm(HTML_Element* bhe);
	void			ResetBodyElm() { body_he.Reset(); }

	/** Call when new history state has been set.
	 *
	 * Updates the URL and base URL.
	 *
	 * @param new_url The new url of the document.
	 */
	void			OnPutHistoryState(const URL& new_url);

	void			SetRefreshURL(const URL* url, short sec);
	URL*			GetRefreshURL() { return refresh_url; }
	short			GetRefreshSeconds() { return refresh_seconds; }

	/** @returns A pointer to the base URL set for this document or NULL if none. */
	URL*			BaseURL() { return base_url; }
	/** @returns The original base URL string. May be NULL if not available. */
	const uni_char*	BaseURLString() { return base_url_string.CStr(); }

	/** @returns A pointer to the URL for this document. */
	URL*			GetURL();

	/** @returns The root element of the logical tree for this document */
	HTML_Element*	GetRoot();
	HTML_Element*	GetDocRoot();
	HTML_Element*	GetBodyElm() { return body_he.GetElm(); }
	HTML_Element*	GetHeadElm();

	BOOL			IsLoaded();
	BOOL			IsParsed();
	BOOL			IsElmLoaded(HTML_Element* he) const;

#if defined FONTSWITCHING && defined DISPLAY_WRITINGSYSTEM_HEURISTIC
	/**
	 * Pass TRUE if textual analysis (using the WritingSystemHeuristic class)
	 * should be used to guess the script (writing system) used; otherwise,
	 * pass FALSE. If enabled, the preferred script will be updated after each
	 * scanned text node with the current best guess for the script used. While
	 * disabled no textual analysis will take place, which should be kept in
	 * mind if the text heuristic is later reenabled.
	 */
	void			SetUseTextHeuristic(BOOL use_text_heuristic)
	{
		this->use_text_heuristic = use_text_heuristic;
	}

	/**
	 * See SetUseTextHeuristic().
	 *
	 * @returns TRUE if the heuristic is enabled, otherwise FALSE.
	 */
	BOOL			GetUseTextHeuristic() const
	{
		return use_text_heuristic;
	}
#endif // defined FONTSWITCHING && defined DISPLAY_WRITINGSYSTEM_HEURISTIC

#ifdef CSS_SCROLLBARS_SUPPORT
	ScrollbarColors* GetScrollbarColors() { return &scrollbar_colors; }
#endif

	int				GetNewFormNumber() { form_elm_count = 0; return ++form_count; }
	int				GetCurrentFormNumber() { return form_count; }
	void			SetCurrentFormNumber(int new_num) { form_count = new_num; }

	void			SetNewForm(HTML_Element* current_form);
	/**
	 * This is the "form pointer" from the HTML5 parser specification.
	 * It informs the parser of what form element is
	 * currently active or NULL if none is active.
	 */
	HTML_Element*	GetCurrentForm() { return form.GetElm(); }

	int				GetNewFormElmNumber() { return ++form_elm_count; }
	int				GetCurrentFormElmNumber() { return form_elm_count; }
	void			SetCurrentFormElmNumber(int new_num) { form_elm_count = new_num; }

	int				GetNewEmbedElmNumber() { return ++embed_elm_count; }

	COLORREF		GetBgColor() const;
  	COLORREF		GetTextColor() const;
  	COLORREF		GetLinkColor() const;
  	COLORREF		GetVLinkColor() const;
  	COLORREF		GetALinkColor() const;

    void			SetBgColor(COLORREF color);
    void			SetTextColor(COLORREF color);
    void			SetLinkColor(COLORREF color);
    void			SetVLinkColor(COLORREF color);
    void			SetALinkColor(COLORREF color);

	BOOL			IsFramesDoc() { return is_frames_doc; }
	void			SetIsFramesDoc() { is_frames_doc = TRUE; }

    BOOL            GetHandleScript() { return handle_script; }
    void            SetHandleScript(BOOL the_truth) { handle_script = the_truth; }

	CSSCollection*	GetCSSCollection() { return &css_collection; }

	/**
	 * @returns TRUE if the CSS was committed, FALSE if it was merely added for later usage.
	 */
	BOOL			AddCSS(CSS* css);

	/**
	 * Process the loaded CSS data and make sure it's processed through the CSS parser.
	 *
	 * @param css_he The element that has the link to the CSS (that must already be loaded before calling this).
	 */
	BOOL			LoadCSS_Url(HTML_Element* css_he);
	CSSMODE			GetCSSMode() { return css_handling; }
	void			SetCSSMode(CSSMODE new_css_handling);

	BOOL			GetUnloadedCSS() const { return unloaded_css; }
	void			SetUnloadedCSS(BOOL value) { unloaded_css = value; }

	BOOL			HasHandheldStyle() const { return HasMediaStyle(CSS_MEDIA_TYPE_HANDHELD); }
	BOOL			HasMediaStyle(CSS_MediaType media_type) const { return ((has_media_style & media_type) ? TRUE : FALSE); }
	void			SetHasHandheldStyle(BOOL value) { SetHasMediaStyle(CSS_MEDIA_TYPE_HANDHELD); }
	void			SetHasMediaStyle(short media);

	/**
	 * Returns the first of a list of <link> elements in the
	 * current document or NULL if theer are none. See GetLinkDatabase
	 * for a different view of the list.
	 */
	const LinkElement*  GetLinkList() { return link_list.First(); }
	/**
	 * Returns a database object that allows iterating over sub-
	 * LinkElements. Will never return NULL. See LinkElementDatabase
	 * for more information.
	 */
	const LinkElementDatabase*  GetLinkDatabase() { return &link_list; }
	OP_STATUS			HandleLink (HTML_Element* he);
	void				RemoveLink (HTML_Element* he);

	OP_STATUS		LoadAllCSS();

	int				GetCssSuccessiveAdjacent() const { return css_collection.GetSuccessiveAdjacent(); }

#ifdef _WML_SUPPORT_
    BOOL			IsWml();
	BOOL			HasWmlContent();
    /** Initializes the WML context. MUST be called at least once before doing any WML. */
    void            WMLInit();
    WML_Context*    WMLGetContext();
	/** Sets the current wml card that is displayed and in use */
	void			WMLSetCurrentCard(HTML_Element* he_card);
	HTML_Element*	WMLGetCurrentCard() { return wml_card_elm.GetElm(); }
#endif // _WML_SUPPORT_
	BOOL			IsXml();
	BOOL			IsXmlSyntaxHighlight();

    BOOL            IsXhtml();
    void            SetXhtml(BOOL the_truth);

	BOOL			GetIsOutOfMemory() const { return is_out_of_memory; }
	void			SetIsOutOfMemory(BOOL value);

	VisualDevice*	GetVisualDevice() { return vis_dev; }
	void			SetVisualDevice(VisualDevice* vd) { vis_dev = vd; }

	FramesDocument*	GetFramesDocument() { return frames_doc; }
	void			SetFramesDocument(FramesDocument* doc);

    void            SetEmbedNotSupported(BOOL the_truth = FALSE) { embed_not_supported = the_truth; }
    BOOL            GetEmbedNotSupported() { return embed_not_supported; }

	BOOL			GetESEnabled();

	ES_LoadManager* GetESLoadManager() { return &es_load_manager; }

	BOOL			ESGetScriptGeneratingDoc() const { return es_load_manager.GetScriptGeneratingDoc(); }

	void			ESSetScriptNotSupported(BOOL the_truth = TRUE) { es_script_not_supported = the_truth; }
	BOOL			ESGetScriptNotSupported() const { return es_script_not_supported; }

# ifdef MANUAL_PLUGIN_ACTIVATION
	void			ESSetScriptExternal(BOOL external) { running_script_is_external = external; }
	BOOL			ESGetScriptExternal() { return running_script_is_external; }
# endif // MANUAL_PLUGIN_ACTIVATION

# ifdef DELAYED_SCRIPT_EXECUTION
	void				ESStopLoading();

	BOOL				ESDelayScripts() { return !es_load_manager.GetScriptGeneratingDoc() && es_delay_scripts && !IsXml() && !es_load_manager.IsClosing(); }
	BOOL				ESIsExecutingDelayedScript() const { return es_is_executing_delayed_script; }
	BOOL				ESIsParsingAhead() const { return es_is_parsing_ahead; }
	BOOL				ESHasDelayedScripts() const { return !es_delayed_scripts.Empty(); }
	BOOL				ESIsFirstDelayedScript(HTML_Element *script) const;

	/**
	 * Disables DSE.  Should only be called before we start parsing.
	 */
	void				ESDisableDelayScripts() { es_delay_scripts = FALSE; OP_ASSERT(!es_is_parsing_ahead && es_delayed_scripts.Empty()); }

	OP_STATUS			ESAddScriptElement(HTML_Element *element, unsigned stream_position, BOOL is_ready);
	OP_STATUS			ESSetScriptElementReady(HTML_Element *element);
	OP_STATUS			ESCancelScriptElement(HTML_Element *element);
	OP_STATUS			ESElementRemoved(HTML_Element *element);

	OP_STATUS			ESParseAheadFinished();
	OP_STATUS			ESStartDelayedScript();
	OP_STATUS			ESFinishedDelayedScript();
	void				ESMarkLastDelayedScriptCompleted() { OP_ASSERT(!es_is_parsing_ahead); static_cast<DelayedScriptElm*>(es_delayed_scripts.First())->has_completed = TRUE; }
	OP_STATUS			ESRecover(HTML_Element *last_element, unsigned stream_position, int form_nr, BOOL form_was_closed, BOOL no_current_form, BOOL has_content, HTML_Element *script_element, HTML5ParserState* parser_state, BOOL script_completed);

	/**
	 * An element has been foster parented someplace not corresponding to its markup position.
	 * @param element  The element which has been foster parented.
	 * @return  ERR_NO_MEMORY on OOM otherwise OK.
	 */
	OP_STATUS			ESAddFosterParentedElement(HTML_Element *element);
	/**
	 * Insert the foster parented elements during ESRecover.
	 * @param delayed_script  The script which caused the recover.
	 * @return  ERR_NO_MEMORY on OOM otherwise OK.
	 */
	OP_STATUS			ESInsertOutOfOrderElements(DelayedScriptElm* delayed_script);
	BOOL				ESGetNeedRecover() { return es_need_recover; }
	void				ESSetNeedRecover();

	void				ESFlushDelayed();
	void				ESSetCurrentElement(HTML_Element *element);
	void				ESRecoverIfNeeded();
# endif // DELAYED_SCRIPT_EXECUTION

	/**
	 * Parser internal function.
	 * Call this when the parser puts a new iframe element in the tree (or in
	 * the case of DSE, when DSE makes it active.
	 *
	 * @param[in] new_element The html:iframe element.
	 *
	 * @param[in] origin_thread The thread that inserted the iframe.
	 *
	 * @returns OpStatus::OK if a new iframe was created the way it should be,
	 * otherwise an error code.
	 */
	OP_STATUS		HandleNewIFrameElementInTree(HTML_Element* new_element, ES_Thread* origin_thread = NULL);

#if defined(FONTSWITCHING)
	OpFontInfo::CodePage	GetPreferredCodePage() { return preferred_codepage; }

	/**
	 * Gets the preferred script to use when displaying the current document.
	 *
	 * @see SetPreferredScript()
	 */
	WritingSystem::Script   GetPreferredScript() { return m_preferred_script; }

	/**
	 * Infers a likely writing system from the codepage in cset and sets
	 * it as the preferred script.
	 *
	 * @returns TRUE if we were able to guess a writing system, otherwise
	 * FALSE.
	*/
	BOOL			SetPreferredScript(const char* cset);

	void			SetPreferredScript(const WritingSystem::Script script)
	{
		m_preferred_script = script;
	}

# ifdef DISPLAY_WRITINGSYSTEM_HEURISTIC
	void			AnalyzeText(const uni_char* text, unsigned text_len);
	void			AnalyzeText(HTML_Element* text_element);

	/**
	 * Gets the script that was guessed by AnalyzeText().
	 *
	 * @see GetPreferredScript()
	 * @return The guessed script, or WritingSystem::Unknown
	 */
	WritingSystem::Script   GetGuessedScript() { return m_guessed_script; }
# endif // DISPLAY_WRITINGSYSTEM_HEURISTIC
#endif // FONTSWITCHING

	int TextCount() const { return text_count; }

	const char*		GetCharacterSet() { return character_set; }

	OP_STATUS		SetCharacterSet(const char* cset);

	/**
	 * Inserts an element as the last child of another element in the logical tree.
	 * @param parent The element that the new element should be placed as the last child of.
	 * @param new_elm The element to insert into the tree.
	 * @returns OK - If the operations succeeded.
	 *          ERR - If the operation failed.
	 *          ERR_NO_MEMORY - If OOM.
	 *          STOPPED - If the parser should stop and restart parsing.
	 */
    OP_HEP_STATUS	InsertElement(HTML_Element *parent, HTML_Element *new_elm);
	/**
	 * Inserts an element before another element in the logical tree.
	 * @param before The element that the new element should precede.
	 * @param new_elm The element to insert into the tree.
	 * @returns OK - If the operations succeeded.
	 *          ERR - If the operation failed.
	 *          ERR_NO_MEMORY - If OOM.
	 *          STOPPED - If the parser should stop and restart parsing.
	 */
    OP_HEP_STATUS	InsertElementBefore(HTML_Element *before, HTML_Element *new_elm);
	/**
	 * Called to close an element during parsing, either because an end tag has
	 * been found or because the element is implicitly closed by another tag.
	 * @param elm The element to be closed.
	 * @returns Normal OOM values.
	 */
    OP_HEP_STATUS	EndElement(HTML_Element *elm);
	/**
	 * @param[out]	modified_text_elm The element that was modified by the inserted text. May be a new element or an old one.
	 */
	OP_HEP_STATUS	InsertText(HTML_Element *parent, const uni_char* text, unsigned text_len, BOOL resolve_entites, BOOL as_cdata, BOOL expand_wml_vars, HTML_Element*& modified_text_elm);
	/**
	 * Inserts a text element as the last child of another element in the logical tree.
	 * @param parent The element that the new element should be placed as the last child of.
	 * @param new_elm The element to insert into the tree.
	 * @returns OK - If the operations succeeded.
	 *          ERR - If the operation failed.
	 *          ERR_NO_MEMORY - If OOM.
	 *          STOPPED - If the parser should stop and restart parsing.
	 */
	OP_HEP_STATUS	InsertTextElement(HTML_Element *parent, HTML_Element *new_elm);
	/**
	 * Inserts a text element before another element in the logical tree.
	 * @param before The element that the new element should precede.
	 * @param new_elm The element to insert into the tree.
	 * @returns OK - If the operations succeeded.
	 *          ERR - If the operation failed.
	 *          ERR_NO_MEMORY - If OOM.
	 *          STOPPED - If the parser should stop and restart parsing.
	 */
	OP_HEP_STATUS	InsertTextElementBefore(HTML_Element *before, HTML_Element *new_elm);

#ifdef _CSS_LINK_SUPPORT_
	URL*			GetCurrentCSSLink() { return current_css_link ? &current_css_link->url : NULL; }
	OP_STATUS		GetNextCSSLink(URL **url);
	OP_STATUS		SetCSSLink(URL& new_url);
	void			CleanCSSLink();
#endif

#ifdef ACCESS_KEYS_SUPPORT
	// Access key functionality

	class AccessKey : public Link
	{
	public:
		AccessKey(OpKey::Code iKey, HTML_Element *iElm, BOOL iFrom_style)
			: key(iKey), from_style(iFrom_style), had_conflicts(FALSE), element(iElm, this) {}

		HTML_Element*	GetElement() { return element.GetElm(); }
		void			SetElement(HTML_Element *elm) { element.SetElm(elm); }

		OpKey::Code		key;
		BOOL			from_style;
		OpString		title;
		OpString		url;

	private:
		friend class HLDocProfile;

		class AccessKeyElmRef : public ElementRef
		{
		public:
			AccessKeyElmRef(HTML_Element *elm, AccessKey *key) : ElementRef(elm), m_key(key) {}

			virtual	void	OnDelete(FramesDocument *document);
			virtual	void	OnRemove(FramesDocument *document) { OnDelete(document); }
			virtual void	OnInsert(FramesDocument *old_document, FramesDocument *new_document) { OnDelete(old_document); }

		private:
			HLDocProfile::AccessKey*	m_key;
		};

		BOOL			had_conflicts; // Multiple elements have tried to register listeners for this at the same time
		AccessKeyElmRef	element;
	};

	BOOL				HasAccessKeys() const {return m_access_keys.First() != NULL;};

	OP_BOOLEAN			AddAccessKey(AccessKeyTransaction &transaction, const uni_char* key);
	OP_STATUS			CommitAccessKeys(AccessKeyTransaction &transaction, HTML_Element* element);
	OP_STATUS			AddAccessKeyFromStyle(const uni_char* key, HTML_Element* element, BOOL &conflict);
	void				RemoveAccessKey(HTML_Element *elm, BOOL from_style_only = FALSE);

	OP_STATUS			AddAccessKey(const uni_char* key, HTML_Element* element);
	/**
	 * @param[in] key The key that the binding of element is to.
	 * @param[in] element The element that the binding is supposed to be to.
	 *
	 * Removes any binding from key to element. This might rebind key to another element.
	 */
	OP_STATUS			RemoveAccessKey(const uni_char* key, HTML_Element* element);
	AccessKey*			GetAccessKey(OpKey::Code key);
	OP_STATUS			GetAllAccessKeys(OpVector<AccessKey>& accesskeys);

	Head				m_access_keys;
#endif // ACCESS_KEYS_SUPPORT

	// Get description of document

    const uni_char*     GetDescription() { return meta_description; }
    OP_STATUS           SetDescription(const uni_char *desc);

    // Document type functions

	const uni_char*     GetDoctypeName();
	const uni_char*     GetDoctypePubId();
	const uni_char*     GetDoctypeSysId();

	BOOL                IsInStrictMode();
	void				SetIsInStrictMode(BOOL smode);

	BOOL				IsInStrictLineHeightMode();

	NS_Type				GetNsType(int ns_idx);

#ifdef _PRINT_SUPPORT_
	OP_STATUS			AddPrintImageElement(HTML_Element* element, InlineResourceType inline_type, URL* img_url);
	BOOL				IsPrintImageElement(HEListElm* elm);
	void				RemovePrintImageElement(HEListElm* elm);
#endif

	void				SetIsParsingInnerHTML(BOOL value) { is_parsing_inner_html = value; }
	BOOL				GetIsParsingInnerHTML() { return is_parsing_inner_html; }

	LayoutWorkplace*    GetLayoutWorkplace();

#ifdef CSS_ERROR_SUPPORT
	int					GetCssErrorCount() { return m_css_error_count; }
	void				IncCssErrorCount() { m_css_error_count++; }
#endif // CSS_ERROR_SUPPORT

#ifdef LOGDOC_GETSCRIPTLIST
	/** Add a script to the list of scripts loaded by this document */
	OP_STATUS			AddScript(HTML_Element *element);
	/** Retrieve the list of scripts loaded by this document */
	const OpVector<URL>	*GetScriptList() { return &script_urls; }
#endif // LOGDOC_GETSCRIPTLIST

	void				SetHandheldCallbackCalled(BOOL called) { handheld_callback_called = called; }
	/**
	 * Is the doctype of this document considered to be handheld?
	 * See CheckHandheldMode in htm_ldoc.cpp
	 */
	void				SetHasHandheldDocType(BOOL handheld_doctype) { doctype_considered_handheld = handheld_doctype; }
	BOOL				GetHasHandheldDocType() const { return doctype_considered_handheld; }
#ifdef USE_HTML_PARSER_FOR_XML
	XMLNamespaceDeclaration::Reference&
						GetCurrentNsDecl() { return m_current_ns_decl; }
	void				SetCurrentNsDecl(XMLNamespaceDeclaration *decl) { m_current_ns_decl = decl; }
	void				PopNsDecl(unsigned level);
#endif // USE_HTML_PARSER_FOR_XML
};

extern HtmlDocType CheckHtmlDocType(HTML_Element* root);

#endif // HTMLOGDOCREP_H
