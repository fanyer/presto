/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef LOGDOC_H
#define LOGDOC_H

#include "modules/logdoc/htm_ldoc.h"
#include "modules/logdoc/complex_attr.h"
#include "modules/logdoc/logdocenum.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/dom/domeventtypes.h"
#ifdef OPERA_CONSOLE
# include "modules/console/opconsoleengine.h"
#endif // OPERA_CONSOLE
#include "modules/forms/formradiogroups.h"
#ifdef LOGDOCXMLTREEACCESSOR_SUPPORT
# include "modules/xmlutils/xmltreeaccessor.h"
#endif // LOGDOCXMLTREEACCESSOR_SUPPORT

#ifdef SVG_SUPPORT
class SVGWorkplace;
#endif // SVG_SUPPORT

class AHotListFolder;
class UnicodeOutputStream;
class UnicodeFileInputStream;

#ifdef _WML_SUPPORT_
class DocumentManager;
#endif // _WML_SUPPORT_

class XMLParser;
class XMLTokenHandler;
class XMLDocumentInformation;
class LogdocXMLTokenHandler;
class LogdocXMLErrorDataProvider;

#ifdef XSLT_SUPPORT
class LogdocXSLTHandler;
class LogdocXSLTStringDataCollector;
#endif // XSLT_SUPPORT

class NamedElements;
class NamedElementsMap;
class NamedElementsIterator;
class HTMLClassNames;

#ifndef HAS_NO_SEARCHTEXT
class SearchData;
#endif // HAS_NO_SEARCHTEXT

class HTML5Parser;
class HTML5SpeculativeParser;

class LogicalDocument : public ComplexAttr
{
public:

	enum RssStatus
	{
		IS_NOT_RSS,
		MAYBE_IS_RSS,
		IS_RSS
	};

private:

	HTML_Element*		print_root;
	int					count;

	URL_InUse			used_url;

	HTML_Element*		root; ///< The real root (HE_DOC_ROOT)
	AutoNullElementRef	doc_root; ///< Markup::HTE_HTML or Markup::SVGE_SVG or something similiar

	BOOL				loading_finished;

	HLDocProfile		hld_profile;

	HTML_Element*		next_link;
	int					next_link_number;

	XMLParser*				xml_parser;
	LogdocXMLTokenHandler*	xml_tokenhandler;
#ifdef XSLT_SUPPORT
	XMLTokenHandler*		xslt_tokenhandler;
	LogdocXSLTHandler*		xslt_handler;
	LogdocXSLTStringDataCollector* xslt_string_data_collector;
#endif // XSLT_SUPPORT
# ifdef XML_ERRORS
	LogdocXMLErrorDataProvider* xml_errordataprovider;
# endif // XML_ERRORS
	BOOL				is_xml;
# ifndef USE_HTML_PARSER_FOR_XML
    BOOL				xml_finished;
	BOOL				xml_parsing_failed;
#  ifdef XML_DEGRADATION_IGNORE_LEADING_WHITESPACE
	// This is set to TRUE once there are no more leading whitespace.
	BOOL				xml_parser_seen_content;
#  endif // XML_DEGRADATION_IGNORE_LEADING_WHITESPACE
# endif // USE_HTML_PARSER_FOR_XML
	BOOL				parse_xml_as_html;
# ifdef FORMAT_UNSTYLED_XML
	BOOL				is_xml_unstyled;
	BOOL				is_xml_syntax_highlighted;
#endif // FORMAT_UNSTYLED_XML
#ifdef RSS_SUPPORT
	RssStatus			rss_status;
#endif // RSS_SUPPORT
#ifdef _WML_SUPPORT_
	BOOL				is_wml;
#endif // _WML_SUPPORT_

#ifdef DOM_DSE_DEBUGGING
	BOOL				dse_enabled;
	BOOL				dse_recovered;
#endif // DOM_DSE_DEBUGGING

	/**
	 * Mapping from name and id to HTML_Element.
	 */
	NamedElementsMap*	namedelementsmap;

	/** A hashtable of ReferencedHTMLClass objects. */
	HTMLClassNames*		classnames;

#ifndef MOUSELESS
	// Counters for the number of onmousemove, onmouseover and onmouseout
	// attributes that have been seen.  They are not necessarily 100 %
	// accurate, but the only important thing is that they are non-zero if
	// there might be such attributes in the document.
	unsigned onmousemove_count, onmouseover_count, onmouseenter_count, onmouseout_count, onmouseleave_count;

#ifdef TOUCH_EVENTS_SUPPORT
	unsigned touchstart_count, touchmove_count, touchend_count, touchcancel_count;
#endif // TOUCH_EVENTS_SUPPORT
#endif // !MOUSELESS

	BOOL				is_aborted;
	BOOL				is_text;

#ifdef _WML_SUPPORT_
    void                WMLEvaluateBranch(HTML_Element *h_elm, DocumentManager *dm);
#endif // _WML_SUPPORT_

	/**
	 * This handles radio groups in a document.
	 */
	DocumentRadioGroups radio_button_groups;

	/** Process HTTP Link headers and create e.g. link elements for stylesheets.
		To be called when document root is created so that we have something to
		insert the link elements under. */
	OP_STATUS ProcessHTTPLinkRelations(HTTP_Link_Relations* link_relations);

	/** Construct the generated document structure used for plain text
	    documents. */
	OP_STATUS ConstructTextDocumentStructure();

	class LayoutWorkplace* layout_workplace;
#ifdef SVG_SUPPORT
	SVGWorkplace* svg_workplace;
#endif

#if LOGDOC_LOADHTML_YIELDMS > 0
	double loadhtml_yieldms;
	unsigned char loadhtml_yieldcount;
#endif // LOGDOC_HTMLLOAD_YIELDMS > 0

#ifdef DNS_PREFETCHING
	BOOL3			dns_prefetch_control;
#endif // DNS_PREFETCHING

#ifdef WEB_TURBO_MODE
	/** The context to be used during a switch to or from Turbo mode. */
	URL_CONTEXT_ID		m_current_context_id;
#endif // WEB_TURBO_MODE

	/**
	 * It's slightly tricky to delete all the XML/XSLT objects in the
	 * right order and it has to be done from two places so better
	 * have it in a helper method.
	 */
	void		FreeXMLParsingObjects();

	/**
	 * Create the root and let everyone that needs to know
	 * about it know. If this returns OpStatus::OK you have
	 * a root, otherwise you don't. Should not be called
	 * if there already is a root.
	 */
	OP_STATUS	CreateLogdocRoot();

	/**
	 * Removes the document root and cleans it if needed.
	 */
	void		ClearLogdocRoot();

	/**
	 * Used to check if we should continue calling the parser code with
	 * the current available data or if we should return control to
	 * the message loop.
	 * @param load_manager The load manager for the current logical document.
	 * @param buf_start The start of the buffer to parse.
	 * @param buf_end The end of the buffer to parse.
	 * @param more TRUE if more data is to be expected from the stream.
	 * @returns TRUE if we should call the parsing code.
	 */
	BOOL		ContinueParsing(ES_LoadManager *load_manager, uni_char *buf_start, uni_char *buf_end, BOOL more);

#ifdef SPECULATIVE_PARSER
	/**
	 * Starts the speculative parser if certain conditions are fullfilled.
	 */
	OP_STATUS	MaybeEnableSpeculativeParser();

	/**
	 * Stops the speculative parser, normal parsing will continue.
	 */
	void		DisableSpeculativeParser();

	HTML5SpeculativeParser*	m_speculative_parser;
	HTML5ParserState*		m_speculative_state;
	unsigned				m_speculative_buffer_offset;
	BOOL					m_is_parsing_speculatively;
#endif // SPECULATIVE_PARSER

	HTML5Parser*	m_parser;
	HTML5Parser*	m_fragment_parser;

	XMLDocumentInformation *m_xml_document_info;

	uni_char*       doctype_name;
	uni_char*       doctype_public_id;
	uni_char*       doctype_system_id;

	/** If write was done before we had created a root, the
	 *  we need to wait to feed the parser with it until we
	 *  have created a root. This stores it in the meanwhile.
	 */
	uni_char*		m_pending_write_data;
	unsigned		m_pending_write_data_length;
	unsigned		m_consumed_pending_write_data;

	BOOL			m_parsing_done;
	BOOL            strict_mode;
	BOOL			quirks_line_height_mode;
	BOOL			m_has_content;
	BOOL			m_continue_parsing;

public:

    				LogicalDocument(FramesDocument* frames_doc);
    				~LogicalDocument();

	OP_STATUS       Init();

	/**
	 * Generates a XML parsing failed error page using the current
	 * XMLParser's error information.
	 */
	OP_STATUS		GenerateXMLParsingFailedErrorPage();

	unsigned		LoadStarted(URL& url, uni_char *&src, int &len, BOOL more);
	unsigned		Load(URL& url, uni_char* src, int len, BOOL &need_to_grow, BOOL more, BOOL aborted = FALSE);

#ifdef SPECULATIVE_PARSER
	/**
	 * Speculatively parse and load resources from the stream when the regular
	 * parser is blocked waiting for an external script.
	 *
	 * @param more TRUE if more data from the stream is expected.
	 * @returns Normal OOM values.
	 */
	OP_STATUS		LoadSpeculatively(URL& url, const uni_char* src, int len, unsigned &rest, BOOL more);
	BOOL			IsParsingSpeculatively() { return m_is_parsing_speculatively; }
	BOOL			IsSpeculativeParserFinished();
#endif // SPECULATIVE_PARSER

#ifdef APPLICATION_CACHE_SUPPORT

	/**
	 * Returns the manifest url given in the manifest attribute on
	 * the html tag for this document.
	 *
	 * @return The manifest url.
	 */
	URL*  			GetManifestUrl();
#endif // APPLICATION_CACHE_SUPPORT

	/**
	 * Returns the HE_DOC_ROOT element for the document.
	 */
	HTML_Element*	GetRoot() { return root; }

	/**
	 * Returns the logical root element for the document, for instance the HE_HTML in a html docuemnt.
	 */
	HTML_Element*	GetDocRoot() { return doc_root.GetElm(); }

	/**
	 * Sets a new element to be the logical root in the document (i.e. the html element in an html document).
	 */
	void			SetDocRoot(HTML_Element *new_root) { doc_root.SetElm(new_root); }

    HTML_Element*	GetBodyElm() { return is_xml ? (hld_profile.GetBodyElm() ? hld_profile.GetBodyElm() : doc_root.GetElm()) : hld_profile.GetBodyElm(); }
    void	        SetBodyElm(HTML_Element* body) { hld_profile.SetBodyElm(body); }
	BOOL			IsElmLoaded(HTML_Element* he) const { return hld_profile.IsElmLoaded(he); }

	URL*			GetRefreshURL() { return hld_profile.GetRefreshURL(); }
	short			GetRefreshSeconds() { return hld_profile.GetRefreshSeconds(); }

	COLORREF		GetBgColor() const { return hld_profile.GetBgColor(); }
  	COLORREF		GetTextColor() const { return hld_profile.GetTextColor(); }
  	COLORREF		GetLinkColor() const { return hld_profile.GetLinkColor(); }
  	COLORREF		GetVLinkColor() const { return hld_profile.GetVLinkColor(); }

	/**
	 * Returns the title specified in the document. Will
	 * return NULL if there is none or if it couldn't be
	 * composed due to OOM. This is cleaned up and suitable for
	 * display.
	 *
	 * @param buffer Must not be NULL.
	 * Used to store the value in case it must be
	 * composed from several sources or if it needs to be cleanup up.
	 *
	 * @return The title or NULL. Might not live longer that the
	 * buffer but do not have to be explicitly freed. Will not
	 * contain line breaks or other control
	 * codes. NULL means "no title" or "out of memory".
	 *
	 * @see GetTitle
	 */
	const uni_char*	Title(TempBuffer* buffer) { return GetTitle(root, buffer); }

	/**
	 * Find the title in the document rooted in root. It can be the document
	 * in this LogicalDocument or another document, but WML title will
	 * only work if it's this document's root.
	 *
	 * @param root The root of the sub tree. May be NULL in which case NULL will be returned.
	 *
	 * @param buffer Must not be NULL.
	 * Used to store the value in case it must be
	 * composed from several sources or if it needs to be cleanup up.
	 *
	 * @return The title or NULL. Might not live longer that the
	 * buffer but do not have to be explicitly freed. Will not
	 * contain line breaks or other control
	 * codes. NULL means "no title" or "out of memory".
	 *
	 * @see Title
	 */
	const uni_char* GetTitle(HTML_Element* root, TempBuffer* buffer);

    HTML_Element*	GetNamedHE(const uni_char* name);
    HTML_Element*	GetFirstHE(HTML_ElementType tpe);

#ifdef LOGDOCXMLTREEACCESSOR_SUPPORT
	OP_STATUS		CreateXMLTreeAccessor(XMLTreeAccessor *&treeaccessor, XMLTreeAccessor::Node *&node, HTML_Element *element = NULL, URL *url = NULL, const XMLDocumentInformation *docinfo = NULL);
	/**< Creates an XMLTreeAccessor instance that accesses elements belonging
	     to this logical document.  If 'element' is NULL , the whole document
	     will be accessed, the created tree accessor's root node will be the
	     element returned by GetRoot(), and 'node' will be set to it.  If
	     'element' is non-NULL, the subtree that it is the root element of
	     will be accessed, and it will be the tree accessor's root node if it
	     is a HE_DOC_ROOT element and the tree accessor's root element if it
	     is a regular element.  In any case, 'node' will be set to 'element'.

	     The created tree accessor is owned by the caller and must be deleted
	     through a call to LocicalDocument::FreeXMLTreeAccessor.

	     @param treeaccessor Set to the created tree accessor on success.
	     @param node Set to the tree accessor node representing 'element', or
	                 to the root node if 'element' is NULL.
	     @param element Optional root element of sub-tree to access.
	     @param url Optional document URL of the tree to access.
	     @param docinfo Optional document information of the tree to access.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	static void FreeXMLTreeAccessor(XMLTreeAccessor *treeaccessor);
	/**< Destroys a tree accessor previously created by CreateXMLTreeAccessor.

	     @param treeaccessor A tree accessor or NULL. */

	static XMLTreeAccessor::Node *GetElementAsNode(XMLTreeAccessor *treeaccessor, HTML_Element *element);
	/**< Returns a tree accessor node representing 'element'.  The tree
	     accessor 'treeaccessor' must have been created by a call to
	     CreateXMLTreeAccessor(), and 'element' must be part of the sub-tree
	     that it accesses.

	     @param treeaccessor A tree accessor.
	     @param element An element.
	     @return A tree accessor node representing 'element'. */

	static HTML_Element *GetNodeAsElement(XMLTreeAccessor *treeaccessor, XMLTreeAccessor::Node* treenode);
	/**< Returns the element represented by the tree accessor node 'treenode'.
	     The tree accessor 'treeaccessor' must have been created by a call to
	     CreateXMLTreeAccessor(), and 'treenode' must belong to the tree that
	     it accesses (that is, not come from another tree accessor's tree.)

	     @param treeaccessor A tree accessor.
	     @param treenode A tree accessor node.
	     @return The element represented by 'treenode'. */
#endif // LOGDOCXMLTREEACCESSOR_SUPPORT

	HTML_Element*	GetDOMObjectById(const uni_char* name);

	/* Count event handlers.  This is really just backup functionality for
	   the counting that DOM_Environment does, in documents where no
	   DOM_Environment has been created yet. */
	void			AddEventHandler(DOM_EventType type);
	void			RemoveEventHandler(DOM_EventType type);
	BOOL			GetEventHandlerCount(DOM_EventType type, unsigned &count);

	int				SearchNamedElements(NamedElementsIterator &iterator, HTML_Element *parent, const uni_char *name, BOOL check_id, BOOL check_name);
	/**< Search the table of named elements for elements named 'name'.  If 'parent' is not NULL,
	     only elements that are descendants of 'parent' are considered.  If 'check_id' is FALSE,
	     ID attributes will not be considered (the id attribute in HTML or elements declared as
	     ID attributes in the DTD in XML.)  If 'check_name' is FALSE, HTML name attributes will
	     not be considered.

		 If the return count is irrelevant and the search should be
		 done on the entire tree, GetNamedElements may be a better
		 choice.

	     @param iterator Iterator object that will be initialized to iterate over all found
	                     named elements.
	     @param parent Optional root element for the subtree to search (can be NULL.)
	     @param name The name to search for.
	     @param check_id Check ID attributes.
	     @param check_name Check HTML name attributes.

	     @return The number of found elements or -1 on OOM. */

	void				GetNamedElements(NamedElementsIterator &iterator, const uni_char *name, BOOL check_id, BOOL check_name);
	/**< Get elements named 'name'. This is a simpler and in some cases faster version of
	     SearchNamedElements with less functionality.

	     @see SearchNamedElements()

	     @param iterator Iterator object that will be initialized to iterate over all found
	                     named elements.
	     @param name The name to search for.
	     @param check_id Check ID attributes.
	     @param check_name Check HTML name attributes. */

	OP_STATUS		AddNamedElement(HTML_Element *element, BOOL descendants, BOOL force = FALSE);
	OP_STATUS		RemoveNamedElementFrom(HTML_Element *element, NamedElements *namedelements);
	OP_STATUS		RemoveNamedElement(HTML_Element *element, BOOL descendants);

	void			SetCompleted(BOOL forced = FALSE, BOOL is_printing = FALSE);
	void			SetNotCompleted() { loading_finished = FALSE; }

	/**
	 * Returns whether the LogicalDocument is "loaded". In
	 * this case we defines "loaded" as: Fully parsed, no
	 * pending delayed scripts and a clean tree with no
	 * pending reflows.
	 *
	 * @todo Remove the requirement for a reflow so that we
	 * get a non-layout dependent definition.
	 */
	BOOL			IsLoaded() const { return loading_finished; }

	/**
	* Returns whether the tree is parsed. This will return
	* TRUE whenever a parse operation isn't in progress, which
	* means that it will return TRUE right before delayed scripts
	* run, and delayed scripts might resume parsing so we might
	* not be fully parsed.
	*
	* To check if there are pending delayed scripts, check the
	* DELAYED_SCRIPT_EXECUTION define and what
	* GetHLDocProfile()->ESHasDelayedScripts()
	* returns
	*/
	BOOL			IsParsed() const { return root && m_parsing_done; }

    int 			GetElmNumber(HTML_Element* he);
    HTML_Element* 	GetElmNumber(int elm_nr);

	HtmlDocType		GetHtmlDocType();

	/**
	 * End the load/parse that is in progress. If aborted is FALSE,
	 * clean up and prepare the document for usage, for instance by
	 * inserting mandatory but missing elements.
	 *
	 * @param aborted If FALSE make the document ready for
	 * use, if TRUE, do as little as possible.
	 */
	void			StopLoading(BOOL aborted);

	/**
	 * Insert HTML, HEAD and BODY elements, if missing.
	 *
	 * @return OK, ERR_NO_MEMORY or ERR
	 */
	OP_STATUS		InsertMissingElements();

	URL*			GetURL() { return used_url; }
	URL*			GetBaseURL() { return hld_profile.BaseURL(); }

	HLDocProfile* 	GetHLDocProfile() { return &hld_profile; }

	void 			FreeLayout();

#ifdef RSS_SUPPORT
	BOOL			IsRss() { return rss_status == IS_RSS; }
	RssStatus		GetRssStatus() { return rss_status; }
	void			SetRssStatus(RssStatus status) { rss_status = status; }
#endif // RSS_SUPPORT

	XMLParser*		GetXmlParser() { return xml_parser; }
	void			SetXMLTokenHandler(LogdocXMLTokenHandler *tokenhandler) { xml_tokenhandler = tokenhandler; }

#ifdef _WML_SUPPORT_
	BOOL			IsWml() { return is_wml; }
    void            WMLReEvaluate();
#endif // _WML_SUPPORT_
	BOOL			IsXml() { return is_xml; }

#ifndef USE_HTML_PARSER_FOR_XML
	BOOL			IsXmlParsingFailed() { return xml_parsing_failed; }
#else // USE_HTML_PARSER_FOR_XML
	BOOL			IsXmlParsingFailed() { return FALSE; }
#endif // USE_HTML_PARSER_FOR_XML
	void			SetParseXmlAsHtml() { parse_xml_as_html = TRUE; }

#ifdef FORMAT_UNSTYLED_XML
	void			SetXmlStyled() { is_xml_unstyled = FALSE; }
	BOOL			IsXmlSyntaxHighlight() { return is_xml_syntax_highlighted; }
#endif // FORMAT_UNSTYLED_XML

	FramesDocument*	GetDocument() { return hld_profile.GetFramesDocument(); }
	// this one will soon be deprecated:
	FramesDocument*	GetFramesDocument() { return hld_profile.GetFramesDocument(); }

	/**
	 * Used to insert data into the parsing data stream from a script at the
	 * current insertion point.
	 * @param str A pointer to the data to be inserted into the stream.
	 * @param str_len The length, in uni_chars, of the data pointed to by str.
	 * @param end_with_newline Adds a newline at the end of the data if TRUE.
	 * @param thread The script thread that emitted the data to insert.
	 * @returns TRUE if the data is handled or FALSE if the thread needs to be suspended.
	 */
	BOOL			ESFlush(const uni_char *str, unsigned str_len, BOOL end_with_newline, ES_Thread *thread = NULL);

#ifdef _PRINT_SUPPORT_
	HTML_Element*	GetPrintRoot() { return print_root; }
	void			CreatePrintCopy();
	void			DeletePrintCopy();
#endif // _PRINT_SUPPORT_

#if defined SAVE_DOCUMENT_AS_TEXT_SUPPORT
	OP_STATUS		WriteAsText(UnicodeOutputStream* out, int max_line_length, HTML_Element* start_element=NULL);
#endif // SAVE_DOCUMENT_AS_TEXT_SUPPORT

	BOOL			GetBoxRect(HTML_Element* element, BoxRectType type, RECT& rect);
	BOOL			GetBoxRect(HTML_Element* element, RECT& rect);
	BOOL			GetBoxClientRect(HTML_Element* element, RECT& rect);

	BOOL			GetCliprect(HTML_Element* element, RECT& rect);

	/**
	 * If an element goes away, we must make sure the parser doesn't have any
	 * reference to it.
	 */
	void			RemoveFromParseState(HTML_Element* element);

#ifdef XSLT_SUPPORT
	void			CleanUpAfterXSLT(BOOL aborted, HTML_Element *root_element = NULL);

	OP_STATUS		PrepareForHTMLOutput(LogdocXSLTStringDataCollector *string_data_collector);
	OP_STATUS		PrepareForTextOutput(LogdocXSLTStringDataCollector *string_data_collector);

	/* These are used from doc, so stubs are kept for now. */
	BOOL			IsXSLTInProgress() { return FALSE; }
	OP_STATUS		ParseXSLTStylesheet() { return OpStatus::OK; }
	OP_STATUS		ApplyXSLTStylesheets() { return OpStatus::OK; }

	unsigned		GetXSLTStylesheetsCount();
	URL				GetXSLTStylesheetURL(unsigned index);
#endif // XSLT_SUPPORT

	/**
	 * This handles radio groups in a document so that we can easily
	 * let them affect eachothers.
	 */
	DocumentRadioGroups& GetRadioGroups() { return radio_button_groups; }

	/**
	 * Returns the LayoutWorkplace. Never NULL.
	 */
	LayoutWorkplace* GetLayoutWorkplace() { return layout_workplace; }
#ifdef SVG_SUPPORT
	/**
	 * Returns the SVGWorkplace. Never NULL.
	 */
	SVGWorkplace* GetSVGWorkplace() { return svg_workplace; }
#endif

#ifdef OPERA_CONSOLE
	static OP_STATUS		PostConsoleMsg(URL *error_url, Str::LocaleString str_num,
							               OpConsoleEngine::Source src, OpConsoleEngine::Severity sev,
							               LogicalDocument* logdoc);

	static void		PostConsoleMsgL(URL *error_url,
							Str::LocaleString str_num,
							OpConsoleEngine::Source src,
							OpConsoleEngine::Severity sev,
							LogicalDocument* logdoc);
#endif // OPERA_CONSOLE

#ifdef VISITED_PAGES_SEARCH
	OP_STATUS		IndexText(HTML_Element *elm, HTML_Element *stop);
#endif // VISITED_PAGES_SEARCH

#ifdef PHONE2LINK_SUPPORT
	OP_STATUS		ScanForTextLinks(HTML_Element *txt_elm, HTML_Element *stop_elm, BOOL in_svg);
#endif // PHONE2LINK_SUPPORT

#ifndef HAS_NO_SEARCHTEXT
# ifdef SEARCH_MATCHES_ALL
	OP_STATUS		FindAllMatches(SearchData* data, HTML_Element *offset_from = NULL, int offset = 0);
# endif // SEARCH_MATCHES_ALL
#endif // HAS_NO_SEARCHTEXT

#if LOGDOC_LOADHTML_YIELDMS > 0
	BOOL YieldParsing()
	{
		loadhtml_yieldcount = (loadhtml_yieldcount + 1) % 8;
		if (!hld_profile.GetIsParsingInnerHTML() && loadhtml_yieldms != 0 && loadhtml_yieldcount == 0)
			return g_op_time_info->GetRuntimeMS() >= loadhtml_yieldms;
		return FALSE;
	}
#endif // LOGDOC_LOADHTML_YIELDMS > 0

#ifdef DNS_PREFETCHING
	/**
	 * Controls prefetch of DNS. The origin for this value is the
	 * x-dns-prefetch-control header.
	 *
	 * @param BOOL Request if DNS prefetching should be enabled (TRUE) or
	 *             disabled (FALSE). If once disabled once it cannot be enabled
	 *             again.
	 */
	void			    SetDNSPrefetchControl(BOOL);

	/**
	 * Get the the value set by x-dns-prefetch-control header.
	 *
	 * @returns YES or NO if control is set by this or its parent document. Or
	 *          MAYBE if not set.
	 */
	BOOL3				GetDNSPrefetchControl();

	/**
	 * Returns whether DNS prefetching should be performed for links in this document.
	 *
	 * @returns TRUE if DNS prefetching should be performed. Secure documents
	 *          will return FALSE if not overridden by the DNS prefetch control
	 *          above.
	 */
	BOOL				DNSPrefetchingEnabled();

	/**
	 * Prefetch DNS resolution for a URL.
	 *
	 * @param url The URL.
	 * @param type The source of the prefetch.
	 */
	void				DNSPrefetch(URL url, DNSPrefetchType type);
#endif // DNS_PREFETCHING

#ifdef WEB_TURBO_MODE
	/**
	 * Must be called when the Web Turbo Mode changes for the
	 * current document.
	 */
	OP_STATUS			UpdateTurboMode();

	/**
	 * @returns The turbo url context if switching to turbo mode otherwise the standard context
	 */
	URL_CONTEXT_ID		GetCurrentContextId() { return m_current_context_id; }
#endif // WEB_TURBO_MODE

	/**
	 * Return a document unique reference to the HTML class given by the classname. The returned reference
	 * can be used directly to compare classnames. GetClassReference will return the same reference for
	 * two strings that are lexicographically equal. For documents that are not in strict mode, classes that
	 * only differ in case share the same reference.
	 *
	 * @param classname A null-terminated class string to get a reference for. The classname must be
	 *					heap allocated and this method will take over the responsibility for deleting
	 *					the string. Even if NULL is returned, this method has the responsibility for
	 *					making sure it gets deleted.
	 *
	 * @returns A class reference object for the classname. NULL means OOM. The returned object must not
	 *          be deleted, but UnRef() must be called when it's no longer referenced.
	 */
	ReferencedHTMLClass*
						GetClassReference(uni_char* classname);

	/**
	 * Return a document unique reference to the HTML class given by the classname. The returned reference
	 * can be used directly to compare classnames. GetClassReference will return the same reference for
	 * two strings that are lexicographically equal. For documents that are not in strict mode, classes that
	 * only differ in case share the same reference.
	 *
	 * @param classname The class string to get a reference for. Does not need to be null-terminated.
	 * @param length The length of the classname string.
	 *
	 * @returns A class reference object for the classname. NULL means OOM. The returned object must not
	 *          be deleted, but UnRef() must be called when it's no longer referenced.
	 */
	ReferencedHTMLClass*
						GetClassReference(const uni_char* classname, size_t length);

	/**
	 * Create a ClassAttribute object based on a class attribute string.
	 *
	 * @param str The class attribute string
	 * @param str_len The length of the class attribute string
	 *
	 * @returns The created attribute object. NULL on OOM.
	 */
	ClassAttribute*		CreateClassAttribute(const uni_char* str, size_t str_len);

	/**
	 * Sets the internal data structure for the doctype of the document.
	 * @param name A null terminated string containing the doctype name.
	 * @param pub A null terminated string containing the public identifier of the doctype.
	 * @param sys A null terminated string containing the system identifier of the doctype.
	 * @returns Normal OOM values.
	 */
	OP_STATUS			SetDoctype(const uni_char *name, const uni_char *pub, const uni_char *sys);

	XMLDocumentInformation*
						GetXMLDocumentInfo() { return m_xml_document_info; }
	void				SetXMLDocumentInfo(XMLDocumentInformation *document_info);

	/**
	 * Called when DOCTYPE is inserted to find out if we are in quirks mode or not
	 * @param force_quirks_mode If TRUE, will force us into quirks mode.
	 */
	void				CheckQuirksMode(BOOL force_quirks_mode);

	const uni_char*     GetDoctypeName() { return doctype_name; }
	const uni_char*     GetDoctypePubId() { return doctype_public_id; }
	const uni_char*     GetDoctypeSysId() { return doctype_system_id; }

	BOOL                IsInStrictMode() { return strict_mode; }
	void				SetIsInStrictMode(BOOL smode);

	BOOL				HasContent() { return m_has_content; }
	void				SetHasContent(BOOL has_content) { m_has_content = has_content; }

	/** Returns TRUE if the document is an iframe and the srcdoc attribute is used. */
	BOOL				IsSrcdoc();

	/**
	 * Used to check if the parser is still parsing and the given element is still
	 * not closed so there might be more content inserted as a descendant of the element
	 * during normal parsing.
	 * @param elm The element to check if is closed or still will get more content.
	 * @returns TRUE if the parser can still insert more content under the element.
	 */
	BOOL				IsParsingUnderElm(HTML_Element *elm);

	BOOL				IsInStrictLineHeightMode() { return strict_mode && !quirks_line_height_mode; }
	void				SetIsInQuirksLineHeightMode() { quirks_line_height_mode = TRUE; }

	/** Returns TRUE if the parser should be called again even if no new data has been received. */
	BOOL				IsContinuingParsing() { return m_continue_parsing; }

	/**
	 * Used to parse a buffer of markup at a certain point in the logical tree, for
	 * instance for innerHTML or documentedit operations.
	 * @param root The root of the logical tree to insert created nodes into.
	 * @param context_elm The element directly under which the new nodes will be inserted.
	 * @param buffer Pointer to the markup data to be parsed.
	 * @param buffer_length The length, in uni_chars, of buffer.
	 * @param use_xml_parser If TRUE, XML parser will be used to parse a fragment.
	 * @return OpStatus::OK on success, OpStatus::ERR in case of parsing errors
	 * or OpStatus::ERR_NO_MEMORY on OOM.
	 **/
	OP_STATUS			ParseFragment(HTML_Element *root, HTML_Element *context_elm, const uni_char *buffer, unsigned buffer_length, BOOL use_xml_parser);

# ifdef DELAYED_SCRIPT_EXECUTION
	/**
	 * During parsing, this returns the latest element inserted, except any elements
	 * which have been foster parented.  This is basically the currently last leaf
	 * of the tree still being constructed, but as the parser sees it (so doesn't take
	 * into account things like layout or scripts which may have changed the tree compared
	 * to the parser's view of it).
	 * Only returns meaningful value while parsing.
	 */
	HTML_Element*		GetParserStateLastNonFosterParentedElement() const;

	/**
	 * During parsing with delayed script excecution, this stores the state of the tree
	 * and parser for each delayed script element we encounter.  Used so that we can
	 * restore the state in case of ESRecover.
	 * @param state  The variable where to store the state.  Use this with RestoreParserState() later
	 * @return  ERR_OUT_OF_MEMORY on memory error, otherwise OK
	 */
	OP_STATUS			StoreParserState(HTML5ParserState* state);

	/**
	 * Used by DSE to restore the parser to the state it had when state was stored
	 * by StoreParserState().
	 * @param state  The state which was stored in the parameter to StoreParserState()
	 * @param script_element  The script element which caused us to restore
	 * @param buffer_stream_position  The position in the stream we've restored to
	 * @param script_has_completed  If TRUE script_element which caused the restore has
	 *   actually completed running, so don't restore its script state
	 * @return  ERR_OUT_OF_MEMORY on memory error, otherwise OK
	 */
	OP_STATUS			RestoreParserState(HTML5ParserState* state, HTML_Element* script_element, unsigned buffer_stream_position, BOOL script_has_completed);

	BOOL				HasParserStateChanged(HTML5ParserState* state);

	/**
	 * Get's the uni_char offset (compared to all data added) of the start of the last
	 * buffer we've added to the tokenizer.
	 * Returns 0 if we don't have a parser or any buffers in the parser.
	 */
	unsigned			GetLastBufferStartOffset();
# endif // DELAYED_SCRIPT_EXECUTION

	/**
	 * Called to pass a buffer of data from a markup stream to the parser before or during parsing.
	 * @param[out] buf_start Points to the start of the data to be parsed before the call
	 *  and is set to the point in the stream where the parser stopped consuming the data
	 *  after the call.
	 * @param buf_end Points to the last character of data in the buffer started at buf_start.
	 * @param more TRUE if this is not the last data to be expected from this stream.
	 * @param from_doc_write TRUE if the data comes from document.write().
	 * @param add_newline TRUE if a newline should be added to the end of the data.
	 * @returns Normal OOM values.
	 */
	OP_STATUS			FeedHtmlParser(const uni_char*& buf_start, const uni_char* buf_end, BOOL more, BOOL from_doc_write, BOOL add_newline);
	/**
	 * Called to parse a finite string of markup data into an empty document.
	 * @param string Pointing to the start of the string to parse.
	 * @param string_length The length, in uni_chars, of string.
	 * @param treat_as_text If TRUE, the data will be inserted as plain text and not parsed.
	 * @param from_doc_write TRUE if the data is from document.write().
	 * @param add_newline TRUE if a newline should be added to the end of the data.
	 * @returns Normal OOM values.
	 */
	OP_STATUS			ParseHtmlFromString(const uni_char *string, unsigned string_length, BOOL treat_as_text, BOOL from_doc_write, BOOL add_newline);
	/**
	 * Called to parse the data already fed to the parser with FeedHtmlParser().
	 * @param more TRUE if more data from the stream is expected.
	 * @returns Normal OOM values.
	 */
	OP_STATUS			ParseHtml(BOOL more);

	/** Returns a pointer to the current HTML parser, if any. */
	HTML5Parser*		GetParser() { return m_parser; }

	/**
	 * Called when a script inserted by the parser has finished running,
	 * either because it ran to completion or because it failed.
	 * @param script_element The element of the finished script.
	 * @param loadman The load manager for the current document.
	 * @returns TRUE if the finished script was started by the parser,
	 *  otherwise FALSE.
	 */
	BOOL				SignalScriptFinished(HTML_Element *script_element, ES_LoadManager *loadman);

	/**
	 * Used to set a buffer of data that should be passed to the parser at a later
	 * stage. This is used when trying to parse data when the root of the tree is not
	 * yet created.
	 * @param data Pointer to the start of the data buffer.
	 * @param data_length The length, in uni_chars, of the data pointed to by data.
	 * @returns Normal OOM values.
	 */
	OP_STATUS			SetPendingScriptData(const uni_char* data, unsigned data_length);
	/**
	 * Returns a pointer to the data waiting to be inserted into the parser, if any.
	 * @param[out] length Will hold the length, in uni_chars, of the data after the call.
	 * @returns A pointer to the pending data.
	 */
	const uni_char*		GetPendingScriptData(unsigned &length);
	/**
	 * Will consume a part of the pending script data once it has been passed to
	 * the appropriate parser.
	 * @param length The number of uni_chars to consume from the start of the buffer.
	 */
	void				ConsumePendingScriptData(unsigned length);

#ifdef DOM_DSE_DEBUGGING
	/**
	 * Set flag that indicates DSE was used during parsing of this document.
	 */
	void				DSESetEnabled() { dse_enabled = TRUE; }
	/**
	 * Set flag that indicates DSE failed (HLDocProfile::ESRecover() called) during
	 * parsing of this document.
	 */
	void				DSESetRecovered() { dse_recovered = TRUE; }

	/**
	 * Returns TRUE if DSE was used during parsing of this document.
	 */
	BOOL				DSEGetEnabled() { return dse_enabled; }
	/**
	 * Returns TRUE if DSE failed (HLDocProfile::ESRecover() called) during parsing
	 * of this document.
	 */
	BOOL				DSEGetRecovered() { return dse_recovered; }
#endif // DOM_DSE_DEBUGGING

private:
	OP_STATUS		InitClassNames();

	// Implementation of the ComplexAttr interface
	BOOL			IsA(int type) { return type == T_LOGDOC || ComplexAttr::IsA(type); }
	OP_STATUS		CreateCopy(ComplexAttr **copy_to) { return OpStatus::ERR_NOT_SUPPORTED; }

#ifdef FORMAT_UNSTYLED_XML
	OP_STATUS		FormatUnstyledXml(const uni_char *message = NULL, BOOL xslt_failed = FALSE);
#endif // FORMAT_UNSTYLED_XML
};

class NamedElementsIterator
{
protected:
	friend void LogicalDocument::GetNamedElements(NamedElementsIterator &, const uni_char *, BOOL, BOOL);
	friend int LogicalDocument::SearchNamedElements(NamedElementsIterator &, HTML_Element *, const uni_char *, BOOL, BOOL);

	NamedElements *namedelements;
	HTML_Element *parent;
	BOOL check_id, check_name;
	unsigned index;

	void Set(NamedElements *namedelements, HTML_Element *parent, BOOL check_id, BOOL check_name);

public:
	NamedElementsIterator();

	HTML_Element *GetNextElement();
	void Reset();
};
#endif // LOGDOC_H
