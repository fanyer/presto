/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/logdoc/logdoc.h"

#include "modules/display/prn_dev.h"
#include "modules/layout/box/box.h"
#include "modules/layout/traverse/traverse.h" // For BoxEdgesObject.
#include "modules/logdoc/xml_ev.h"
#include "modules/logdoc/datasrcelm.h"
#include "modules/logdoc/html5parser.h"
#include "modules/logdoc/src/html5/html5speculativeparser.h"
#include "modules/probetools/probepoints.h"
#include "modules/logdoc/logdoc_util.h"
#ifdef LOGDOCXMLTREEACCESSOR_SUPPORT
# include "modules/logdoc/src/logdocxmltreeaccessor.h"
#endif // LOGDOCXMLTREEACCESSOR_SUPPORT
#include "modules/util/opfile/opfile.h"
#include "modules/util/opfile/opmemfile.h"
#include "modules/util/handy.h"
#include "modules/util/tempbuf.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/url/url_api.h"
#include "modules/formats/argsplit.h"
#include "modules/url/url_link.h"
#include "modules/forms/webforms2support.h"
#ifdef USE_ABOUT_FRAMEWORK
# include "modules/about/opxmlerrorpage.h"
#endif

#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/logdoc_constants.h"
#include "modules/logdoc/xmlenum.h"
#include "modules/display/vis_dev.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/fdelm.h"
#include "modules/dochand/win.h"
#ifdef WEB_TURBO_MODE
# include "modules/dochand/winman.h"
#endif // WEB_TURBO_MODE
#include "modules/display/prn_info.h"
#include "modules/style/css_media.h"
#include "modules/util/htmlify.h"

#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"

#include "modules/dom/domenvironment.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/essched.h"

#ifdef XSLT_SUPPORT
# include "modules/logdoc/src/xsltsupport.h"
#endif // XSLT_SUPPORT

#include "modules/logdoc/src/xmlsupport.h"
#include "modules/xmlutils/xmlutils.h"
#include "modules/xmlutils/xmldocumentinfo.h"
#include "modules/xmlutils/xmlerrorreport.h"

#ifdef _WML_SUPPORT_
# include "modules/logdoc/wml.h"
#endif // _WML_SUPPORT_

#include "modules/layout/layout_workplace.h"

#ifdef SVG_SUPPORT
# include "modules/svg/svg_workplace.h"
#endif // SVG_SUPPORT

#ifdef VISITED_PAGES_SEARCH
# include "modules/search_engine/VisitedSearch.h"
#endif // VISITED_PAGES_SEARCH

#include "modules/logdoc/src/search.h"
#include "modules/doc/searchinfo.h"

#ifdef PHONE2LINK_SUPPORT
# include "modules/unicode/unicode_stringiterator.h"
#endif

#include "modules/windowcommander/src/WindowCommander.h"

#ifdef SCOPE_SUPPORT
# include "modules/probetools/probetimeline.h"
#endif // SCOPE_SUPPORT

#ifdef DNS_PREFETCHING
# include "modules/logdoc/dnsprefetcher.h"
#endif // DNS_PREFETCHING

#include "modules/logdoc/optreecallback.h"

class NamedElements
{
public:
	uni_char *name;
	union
	{
		HTML_Element *single;
		HTML_Element **elements;
	} data;
	unsigned count;
};

class NamedElementsMap
	: public OpStringHashTable<NamedElements>
{
private:
	static void DeleteNamedElements(const void *key, void *data)
	{
		NamedElements *namedelements = (NamedElements *) data;
		OP_DELETEA(namedelements->name);
		if (namedelements->count > 1)
			OP_DELETEA(namedelements->data.elements);
		OP_DELETE(namedelements);
	}

public:
	NamedElementsMap()
		: OpStringHashTable<NamedElements>(TRUE)
	{
	}

	~NamedElementsMap()
	{
		Clear();
	}

	void Clear()
	{
		ForEach(DeleteNamedElements);
		RemoveAll();
	}
};

#ifdef SCOPE_PROFILER

/**
 * Custom probe for PROBE_EVENT_DOCUMENT_PARSING. We reduce noise in the
 * target function by splitting the activation into a separate class.
 */
class OpDocumentParsingProbe
	: public OpPropertyProbe
{
public:

	/**
	 * Activate the OpDocumentParsingProbe.
	 *
	 * @param timeline The timeline to report to. Can not be NULL.
	 * @param url The URL for the document that's being parsed.
	 *
	 * @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS Activate(OpProbeTimeline *timeline, URL *url)
	{
		OpPropertyProbe::Activator<1> act(*this);
		RETURN_IF_ERROR(act.Activate(timeline, PROBE_EVENT_DOCUMENT_PARSING, url));
		RETURN_IF_ERROR(act.AddURL("url", url));
		return OpStatus::OK;
	}
};

# define DOCUMENT_PARSING_PROBE(TMP_PROBE, TMP_FRAMESDOC, TMP_URL) \
	OpDocumentParsingProbe TMP_PROBE; \
	if (TMP_FRAMESDOC->GetTimeline()) \
		RETURN_IF_ERROR(TMP_PROBE.Activate(TMP_FRAMESDOC->GetTimeline(), TMP_URL));

#else // SCOPE_PROFILER
# define DOCUMENT_PARSING_PROBE(TMP_PROBE, TMP_FRAMESDOC, TMP_URL) ((void)0)
#endif // SCOPE_PROFILER

/** Pool of class name strings shared by class attributes and class selectors.
	The idea is that if a class within a document is always represented as the
	same class reference, we can compare references instead of string or substring
	comparisons. */
class HTMLClassNames
	: public OpStringHashTable<ReferencedHTMLClass>
{
public:
	HTMLClassNames(BOOL case_sensitive) : OpStringHashTable<ReferencedHTMLClass>(case_sensitive) {}
	virtual ~HTMLClassNames() { ForEach(ResetPoolPointer); }

private:
	static void ResetPoolPointer(const void *key, void *data) { static_cast<ReferencedHTMLClass*>(data)->PoolDeleted(); }
};

LogicalDocument::LogicalDocument(FramesDocument* frames_doc) :
	print_root(NULL),
	count(1),
	root(NULL),
	loading_finished(FALSE),
	next_link(0),
	next_link_number(0),
	xml_parser(NULL),
	xml_tokenhandler(NULL),
#ifdef XSLT_SUPPORT
	xslt_tokenhandler(NULL),
	xslt_handler(NULL),
	xslt_string_data_collector(NULL),
#endif // XSLT_SUPPORT
#ifdef XML_ERRORS
	xml_errordataprovider(NULL),
#endif // XML_ERRORS
	is_xml(FALSE),
#ifndef USE_HTML_PARSER_FOR_XML
	xml_finished(TRUE),
	xml_parsing_failed(FALSE),
#ifdef XML_DEGRADATION_IGNORE_LEADING_WHITESPACE
	xml_parser_seen_content(FALSE),
#endif // XML_DEGRADATION_IGNORE_LEADING_WHITESPACE
#endif // USE_HTML_PARSER_FOR_XML
	parse_xml_as_html(FALSE),
#ifdef FORMAT_UNSTYLED_XML
	is_xml_unstyled(FALSE),
	is_xml_syntax_highlighted(FALSE),
#endif // FORMAT_UNSTYLED_XML
#ifdef RSS_SUPPORT
	rss_status(IS_NOT_RSS),
#endif // RSS_SUPPORT
#ifdef _WML_SUPPORT_
	is_wml(FALSE),
#endif
#ifdef DOM_DSE_DEBUGGING
	dse_enabled(FALSE),
	dse_recovered(FALSE),
#endif // DOM_DSE_DEBUGGING
	namedelementsmap(NULL),
	classnames(NULL),
#ifndef MOUSELESS
	onmousemove_count(0),
	onmouseover_count(0),
	onmouseenter_count(0),
	onmouseout_count(0),
	onmouseleave_count(0),
#ifdef TOUCH_EVENTS_SUPPORT
	touchstart_count(0),
	touchmove_count(0),
	touchend_count(0),
	touchcancel_count(0),
#endif // TOUCH_EVENTS_SUPPORT
#endif // !MOUSELESS
	is_aborted(FALSE),
	is_text(FALSE),
	layout_workplace(NULL)
#ifdef SVG_SUPPORT
	, svg_workplace(NULL)
#endif // SVG_SUPPORT
#if LOGDOC_LOADHTML_YIELDMS > 0
	, loadhtml_yieldms(0)
	, loadhtml_yieldcount(0)
#endif // LOGDOC_HTMLLOAD_YIELDMS > 0
#ifdef DNS_PREFETCHING
	, dns_prefetch_control(MAYBE)
#endif // DNS_PREFETCHING
#ifdef WEB_TURBO_MODE
	, m_current_context_id(0)
#endif // WEB_TURBO_MODE
#ifdef SPECULATIVE_PARSER
	, m_speculative_parser(NULL)
	, m_speculative_state(NULL)
	, m_is_parsing_speculatively(FALSE)
#endif // SPECULATIVE_PARSER
	, m_parser(NULL)
	, m_fragment_parser(NULL)
	, m_xml_document_info(NULL)
	, doctype_name(NULL)
	, doctype_public_id(NULL)
	, doctype_system_id(NULL)
	, m_pending_write_data(NULL)
	, m_pending_write_data_length(0)
	, m_consumed_pending_write_data(0)
	, m_parsing_done(FALSE)
	, quirks_line_height_mode(FALSE)
	, m_has_content(FALSE)
	, m_continue_parsing(FALSE)
{
	hld_profile.SetLogicalDocument(this);
	hld_profile.SetFramesDocument(frames_doc);
	if (frames_doc)
	{
		hld_profile.SetVisualDevice(frames_doc->GetVisualDevice());
		hld_profile.SetCSSMode(frames_doc->GetWindow()->GetCSSMode());
	}
	SetIsInStrictMode(FALSE);
}

void LogicalDocument::FreeXMLParsingObjects()
{
	OP_DELETE(xml_parser);
	xml_parser = NULL;

	// Order below is important. The xslt_tokenhandler might
	// have a reference to the xml_tokenhandler below
	// (through its internal xmlparser)
#ifdef XSLT_SUPPORT
	OP_DELETE(xslt_tokenhandler);
	xslt_tokenhandler = NULL;
	OP_DELETE(xslt_handler);
	xslt_handler = NULL;
#endif // XSLT_SUPPORT

	OP_DELETE(xml_tokenhandler);
	xml_tokenhandler = NULL;

#ifdef XML_ERRORS
	OP_DELETE(xml_errordataprovider);
	xml_errordataprovider = NULL;
#endif // XML_ERRORS
}

LogicalDocument::~LogicalDocument()
{
	FramesDocument *frames_doc = hld_profile.GetFramesDocument();

	if (print_root)
	{
		if (print_root->Clean(frames_doc))
			print_root->Free(frames_doc);
		print_root = NULL;
	}

	OP_DELETE(m_parser);
	m_parser = NULL;
	OP_DELETE(m_fragment_parser);
	m_fragment_parser = NULL;
#ifdef SPECULATIVE_PARSER
	OP_DELETE(m_speculative_state);
	m_speculative_state = NULL;
	OP_DELETE(m_speculative_parser);
	m_speculative_parser = NULL;
#endif // SPECULATIVE_PARSER
	OP_DELETE(m_xml_document_info);

	if (root)
	{
		if (root->Clean(GetFramesDocument()))
			root->Free(GetFramesDocument());
		root = NULL;
	}

	FreeXMLParsingObjects();

	OP_DELETE(namedelementsmap);
	OP_DELETE(classnames);
	OP_DELETE(layout_workplace);
#ifdef SVG_SUPPORT
	OP_DELETE(svg_workplace);
#endif // SVG_SUPPORT

	OP_DELETEA(doctype_public_id);
	OP_DELETEA(doctype_name);
	OP_DELETEA(doctype_system_id);
	OP_DELETEA(m_pending_write_data);
}

void LogicalDocument::FreeLayout()
{
	if (root)
	{
		FramesDocument* doc = GetFramesDocument();

		root->FreeLayout(doc);
		root->MarkExtraDirty(doc);
		root->MarkPropsDirty(doc);
	}
}

OP_STATUS LogicalDocument::Init()
{
	layout_workplace = OP_NEW(LayoutWorkplace, (GetFramesDocument()));

	if (!layout_workplace)
		return OpStatus::ERR_NO_MEMORY;

#ifdef SVG_SUPPORT
	RETURN_IF_ERROR(SVGWorkplace::Create(&svg_workplace, GetFramesDocument()));
#endif // SVG_SUPPORT

    /* Initial charset and prefered_codepage values:

    1) Use operating system localization info
    2) Use Forced encoding
    3) Use encoding extracted from url

     */

	OP_STATUS rc = OpStatus::OK;
#if defined(FONTSWITCHING)

	// Previously we set an initial preferred script here using
	// GetSystemEncodingL(), but as it was always overriden by the
	// text heuristic the code has been removed.

	// We override the previous codepage, so the rc of the previous call to
	// SetPreferredCodepage is no longer relevant.
	OpStatus::Ignore(rc);

	// A script inferred from a forced encoding takes precedence over one
	// inferred via the text-based writing system detection heuristic.
	if (hld_profile.SetPreferredScript(GetFramesDocument()->GetWindow()->GetForceEncoding()))
		hld_profile.SetUseTextHeuristic(FALSE);

	rc = OpStatus::OK;

#endif // FONTSWITCHING
	FramesDocument *frames_doc = hld_profile.GetFramesDocument();

	if (frames_doc->Get_URL_DataDescriptor())
		rc = hld_profile.SetCharacterSet(frames_doc->Get_URL_DataDescriptor()->GetCharacterSet());

	return rc;
}

#define MAX_TEXT_SIZE	8000

enum LoadStartedStatus
{
	LOADSTARTED_OK,
	LOADSTARTED_OOM,
	LOADSTARTED_RETURN_ZERO,
	LOADSTARTED_RETURN_LEN
};


OP_STATUS LogicalDocument::CreateLogdocRoot()
{
	OP_ASSERT(!root);
	root = NEW_HTML_Element();
	if (!root)
		return OpStatus::ERR_NO_MEMORY;

	HtmlAttrEntry html_attrs[2];
	html_attrs[0].attr = ATTR_LOGDOC;
	html_attrs[0].is_special = TRUE;
	html_attrs[0].is_specified = FALSE;
	html_attrs[0].value = reinterpret_cast<const uni_char*>(this);
	html_attrs[0].ns_idx = SpecialNs::NS_LOGDOC;
	html_attrs[1].attr = ATTR_NULL;
	OP_STATUS status = root->Construct(&hld_profile, NS_IDX_DEFAULT, HE_DOC_ROOT, html_attrs);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(root);
		root = NULL;
		return status;
	}

	// Now check if we already have a dom environment with a document missing its root element
	FramesDocument *frames_doc = GetFramesDocument();
	DOM_Environment* env = frames_doc->GetDOMEnvironment();
	if (env)
		env->NewDocRootElement(root);

	return OpStatus::OK;
}

void LogicalDocument::ClearLogdocRoot()
{
	if (root)
	{
		HTML_Element *elm = root->FirstChild();
		while (elm)
		{
			HTML_Element *next = elm->Suc();
			elm->Remove(this);
			if (elm->Clean(GetFramesDocument()))
				elm->Free(GetFramesDocument());
			elm = next;
		}

		if (root->Clean(GetFramesDocument()))
			root->Free(GetFramesDocument());
		root = NULL;
	}
}

BOOL LogicalDocument::ContinueParsing(ES_LoadManager *load_manager, uni_char *buf_start, uni_char *buf_end, BOOL more)
{
	if (!load_manager->IsBlocked())
	{
		if (is_xml)
			return !xml_finished && !xml_parsing_failed;
		else
		{
			if (buf_start != buf_end) // more unparsed data is available
				return TRUE;
			else if (IsContinuingParsing()) // the parser has more unparsed data
				return TRUE;
			else if (!more)
			{
				// no data to parse, but the HTML parser will handle inserting
				// default elements in case of an empty document, or finish off
				// the already parsed data if this is the end of the stream.
				if (!m_parser || !m_parser->IsFinished())
					return TRUE;
			}
		}
	}

	return FALSE;
}

#ifdef SPECULATIVE_PARSER
OP_STATUS LogicalDocument::MaybeEnableSpeculativeParser()
{
	OP_ASSERT(!m_speculative_state);
	if (!g_pcjs->GetIntegerPref(PrefsCollectionJS::SpeculativeParser, *GetURL()))
		return OpStatus::OK;
#ifdef DELAYED_SCRIPT_EXECUTION
	if (hld_profile.ESIsExecutingDelayedScript())
		return OpStatus::OK;
#endif // DELAYED_SCRIPT_EXECUTION
	if (m_speculative_parser && m_speculative_parser->IsFinished())
		return OpStatus::OK;

	if (!m_speculative_parser || m_speculative_parser->GetStreamPosition() < m_parser->GetStreamPosition())
	{
		FramesDocument* frm_doc = GetFramesDocument();

		/* Make the speculative parser start from where the regular one
		   currently is. Only do this if the this is the first time the
		   speculative parser runs or if the regular parser has parsed ahead of
		   the speculative one. */
		RETURN_IF_ERROR(frm_doc->CreateSpeculativeDataDescriptor(m_parser->GetLastBufferStartOffset()));
		RETURN_OOM_IF_NULL(m_speculative_state = OP_NEW(HTML5ParserState, ()));
		RETURN_IF_ERROR(m_parser->StoreParserState(m_speculative_state));
		m_speculative_buffer_offset = m_parser->GetTokenizer()->GetLastBufferStartOffset();

		frm_doc->GetMessageHandler()->PostMessage(MSG_URL_DATA_LOADED, frm_doc->GetURL().Id(TRUE), 0);
	}

	m_is_parsing_speculatively = TRUE;
	return OpStatus::OK;
}

void LogicalDocument::DisableSpeculativeParser()
{
	m_is_parsing_speculatively = FALSE;

	/* Sometimes the script finishes and the speculative parser is disabled
	   before the it had a chance to run. E.g., when the regular parser was
	   blocked on a script that the speculative parser already had loaded. Hence
	   the state needs to be deleted. */
	OP_DELETE(m_speculative_state);
	m_speculative_state = NULL;
}
#endif // SPECULATIVE_PARSER

unsigned LogicalDocument::LoadStarted(URL& url, uni_char *&src, int &len, BOOL more)
{
	FramesDocument *frames_doc = GetFramesDocument();
	// Disable adaptive zoom for all Opera generated pages.
	if (frames_doc->GetTopDocument() == frames_doc && frames_doc->IsGeneratedByOpera())
		layout_workplace->SetKeepOriginalLayout();
	const OpStringC8 mime = url.GetAttribute(URL::KMIME_Type);

#ifdef SUPPORT_VERIZON_EXTENSIONS
	OpString8 verizonHeader;
	verizonHeader.Set("x-verizon-content");
	url.GetAttribute(URL::KHTTPSpecificResponseHeaderL, verizonHeader, TRUE);
	if (verizonHeader.CompareI( "standard") == 0)
		layout_workplace->SetKeepOriginalLayout();
#endif //SUPPORT_VERIZON_EXTENSIONS

	if (mime.CStr() && op_stricmp(mime.CStr(), "APPLICATION/VND.WAP.XHTML+XML") == 0)
	{
		hld_profile.SetHasMediaStyle(CSS_MEDIA_TYPE_HANDHELD);

		layout_workplace->SetKeepOriginalLayout();
	}

	URLContentType content_type = url.ContentType();

	BOOL is_generated_document = frames_doc->IsGeneratedDocument();

	BOOL is_xml_content = FALSE;
	BOOL is_text_content = FALSE;

	if (!is_generated_document)
		switch (content_type)
		{
		case URL_XML_CONTENT:
#ifdef _WML_SUPPORT_
		case URL_WML_CONTENT:
#endif // _WML_SUPPORT_
#ifdef SVG_SUPPORT
		case URL_SVG_CONTENT:
#endif // SVG_SUPPORT
			is_xml_content = !parse_xml_as_html;
			break;

		case URL_TEXT_CONTENT:
		case URL_CSS_CONTENT:
		case URL_X_JAVASCRIPT:
		case URL_PAC_CONTENT:
#ifdef APPLICATION_CACHE_SUPPORT
		case URL_MANIFEST_CONTENT:
#endif // APPLICATION_CACHE_SUPPORT
			is_text_content = TRUE;
		}

	FramesDocElm *fde = frames_doc->GetDocManager()->GetFrame();
#ifdef SVG_SUPPORT
	HTML_Element *elm = fde ? fde->GetHtmlElement() : NULL;
	if (!is_text_content && elm && elm->GetNsType() == NS_SVG)
		is_xml_content = TRUE;
#endif // SVG_SUPPORT

	if (is_xml_content)
	{
#ifdef XMLUTILS_PARSE_NOT_INCREMENTAL
		if (more)
			return LOADSTARTED_RETURN_LEN;
#endif // XMLUTILS_PARSE_NOT_INCREMENTAL

		is_xml = TRUE;
#ifndef USE_HTML_PARSER_FOR_XML
		xml_finished = FALSE;
#endif // USE_HTML_PARSER_FOR_XML
#ifdef FORMAT_UNSTYLED_XML
		is_xml_unstyled = TRUE;
#endif // FORMAT_UNSTYLED_XML

		hld_profile.SetIsInStrictMode(TRUE);

#ifdef _WML_SUPPORT_
		is_wml = content_type == URL_WML_CONTENT;

		if (is_wml)
		{
			hld_profile.SetHasMediaStyle(CSS_MEDIA_TYPE_HANDHELD);

# ifdef USE_HTML_PARSER_FOR_XML
			XMLNamespaceDeclaration::Reference xml_ns_decl;
			OP_STATUS oom_stat = XMLNamespaceDeclaration::Push(xml_ns_decl,
				UNI_L("http://www.wapforum.org/2001/wml"),
				uni_strlen(UNI_L("http://www.wapforum.org/2001/wml")),
				NULL, NULL, 0);
			if (OpStatus::IsMemoryError(oom_stat))
				return LOADSTARTED_OOM;
			hld_profile.SetCurrentNsDecl(xml_ns_decl);
# endif // USE_HTML_PARSER_FOR_XML

			layout_workplace->SetKeepOriginalLayout();

			frames_doc->GetDocManager()->WMLInit();
			WML_Context *wml_context = frames_doc->GetDocManager()->WMLGetContext();
			wml_context->PreParse();
		}
#endif // _WML_SUPPORT_

#ifndef USE_HTML_PARSER_FOR_XML
# ifdef XML_ERRORS
		xml_errordataprovider = OP_NEW(LogdocXMLErrorDataProvider, (this));
# endif // XML_ERRORS

		XMLTokenHandler *th;

# ifdef XSLT_SUPPORT
		xslt_handler = OP_NEW(LogdocXSLTHandler, (this));
		if (!xslt_handler || OpStatus::IsMemoryError(XSLT_Handler::MakeTokenHandler(xslt_tokenhandler, xslt_handler)))
			xslt_tokenhandler = NULL;
		th = xslt_tokenhandler;
# else // XSLT_SUPPORT
		th = xml_tokenhandler = OP_NEW(LogdocXMLTokenHandler, (this));
# endif // XSLT_SUPPORT

		LogdocXMLParserListener *xml_parserlistener = OP_NEW(LogdocXMLParserListener, (this));

		if (!th || !xml_parserlistener || OpStatus::IsMemoryError(XMLParser::Make(xml_parser, xml_parserlistener, frames_doc, th, frames_doc->GetURL())))
		{
			OP_DELETE(xml_tokenhandler);
			xml_tokenhandler = NULL;

# ifdef XSLT_SUPPORT
			OP_DELETE(xslt_tokenhandler);
			xslt_tokenhandler = NULL;
			OP_DELETE(xslt_handler);
			xslt_handler = NULL;
# endif // XSLT_SUPPORT

			OP_DELETE(xml_parserlistener);
			return LOADSTARTED_OOM;
		}

		xml_parser->SetOwnsListener();

		XMLParser::Configuration configuration;
		configuration.preferred_literal_part_length = SplitTextLen;
		configuration.store_internal_subset = TRUE;
# ifdef XML_ERRORS
		configuration.generate_error_report = TRUE;
# endif // XML_ERRORS
		xml_parser->SetConfiguration(configuration);

# ifdef XML_ERRORS
		xml_parser->SetErrorDataProvider(xml_errordataprovider);
# endif // XML_ERRORS

		if (xml_tokenhandler)
			xml_tokenhandler->SetParser(xml_parser);
#endif // USE_HTML_PARSER_FOR_XML
	}

	if (!hld_profile.BaseURL())
		hld_profile.SetBaseURL(&url);

	hld_profile.SetURL(&url);

	if (hld_profile.GetIsOutOfMemory())
		return LOADSTARTED_OOM;

	short refresh_int = url.GetAttribute(URL::KHTTP_Refresh_Interval);
	const char *refresh_url_name = url.GetAttribute(URL::KHTTPRefreshUrlName).CStr();

	if (refresh_int >= 0)
	{
		URL refresh_url;

		if (refresh_url_name)
		{
			refresh_url = g_url_api->GetURL(url, refresh_url_name);
			if (refresh_url.Type() == URL_JAVASCRIPT) // Unexplored security implications of allowing javascript urls in any document. See for instance DSK-236358.
				url = URL();
#ifdef _ISHORTCUT_SUPPORT
			// Similarly for known .url shortcut content in inlines, stopped short here. CORE-48788.
			else if (fde && refresh_url.ContentType() == URL_ISHORTCUT_CONTENT)
				url = URL();
#endif // _ISHORTCUT_SUPPORT
		}
		else
			refresh_url = url;

		if (!url.IsEmpty())
			hld_profile.SetRefreshURL(&refresh_url, refresh_int);

	}

	hld_profile.SetUnloadedCSS(FALSE);

	HTTP_Link_Relations relations;
	HTTP_Link_Relations* link_relations;

	if (OpStatus::IsError(CreateLogdocRoot()))
		goto handle_oom;

	link_relations = (HTTP_Link_Relations*) url.GetAttribute(URL::KHTTPLinkRelations, (void*)&relations, TRUE);

	if (link_relations)
		if (OpStatus::IsMemoryError(ProcessHTTPLinkRelations(link_relations)))
			goto handle_oom;

	if (is_text_content)
		is_text = TRUE;

#ifdef DNS_PREFETCHING
	{
		OpString8 dns_prefetch_control;
		dns_prefetch_control.Set("x-dns-prefetch-control");
		url.GetAttribute(URL::KHTTPSpecificResponseHeaderL, dns_prefetch_control, TRUE);
		if (dns_prefetch_control.CompareI("on") == 0)
			SetDNSPrefetchControl(TRUE);
		else if (dns_prefetch_control.CompareI("off") == 0)
			SetDNSPrefetchControl(FALSE);
	}
#endif // DNS_PREFETCHING

	return LOADSTARTED_OK;

handle_oom:
	DELETE_HTML_Element(root);
	root = NULL;

	return LOADSTARTED_OOM;
}

static OP_STATUS UDD_RetreiveData(URL_DataDescriptor *dd, BOOL &more, unsigned long &data_length)
{
	OP_STATUS status;
	TRAP(status, data_length = dd->RetrieveDataL(more));
	return status;
}

OP_STATUS LogicalDocument::GenerateXMLParsingFailedErrorPage()
{
	// Create error document
	URL tmp_url = g_url_api->GetNewOperaURL();
	OpXmlErrorPage errorpage(tmp_url, xml_parser);
	RETURN_IF_ERROR(errorpage.GenerateData());

	// Extract the text
	URL_DataDescriptor *dd = tmp_url.GetDescriptor(NULL, FALSE, TRUE);
	RETURN_OOM_IF_NULL(dd);
	ANCHOR_PTR(URL_DataDescriptor, dd);

	BOOL more = FALSE;
	unsigned long data_length = 0;
	RETURN_IF_ERROR(UDD_RetreiveData(dd, more, data_length));

	// Generated page is ensured to have all data available.
	OP_ASSERT(2 < data_length && !more);

	// Being a bit paranoid about memory alignment, because of the reinterpret_cast to uni_char,
	// although it is safe because the buffer is allocated and the pointer preserved.
	OP_ASSERT(!(PTR_TO_ULONG(dd->GetBuffer()) & 0x1));

	FramesDocument *frames_doc = GetFramesDocument();

#ifdef _WML_SUPPORT_
	hld_profile.WMLSetCurrentCard(NULL);
#endif //_WML_SUPPORT_

	frames_doc->StopLoadingAllInlines(TRUE);
	frames_doc->CleanESEnvironment(TRUE);
	ClearLogdocRoot();
#ifdef SVG_SUPPORT
	svg_workplace->Clean();
#endif // SVG_SUPPORT

	RETURN_IF_ERROR(CreateLogdocRoot());

	doc_root.Reset();
	hld_profile.ResetBodyElm();

	OP_DELETE(m_parser);

	m_parser = OP_NEW(HTML5Parser, (this));
	RETURN_OOM_IF_NULL(m_parser);

	RETURN_IF_ERROR(m_parser->AppendData(reinterpret_cast<const uni_char *>(dd->GetBuffer()), UNICODE_DOWNSIZE(data_length), TRUE, FALSE));
	OP_PARSER_STATUS pstatus = m_parser->Parse(root, NULL);

	RETURN_IF_ERROR(frames_doc->CheckInternal());

	return OpStatus::IsError(pstatus) ? pstatus : OpStatus::OK;
}

unsigned LogicalDocument::Load(URL& url, uni_char* src, int len, BOOL &need_to_grow, BOOL more, BOOL aborted)
{
	OP_PROBE5(OP_PROBE_LOGDOC_LOAD);

#ifdef XSLT_SUPPORT
	if (xslt_string_data_collector && !xslt_string_data_collector->IsCalling())
	{
		/* We shouldn't be given any more data when in this state. */
		OP_ASSERT(len == 0 && !more);

		/* Note: this typically leads to a recursive call to this function! */
		OP_BOOLEAN is_finished = xslt_string_data_collector->ProcessCollectedData();

		if (OpStatus::IsMemoryError(is_finished))
		{
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			hld_profile.SetIsOutOfMemory(TRUE);
		}

		if (is_finished != OpBoolean::IS_FALSE)
		{
			OP_DELETE(xslt_string_data_collector);
			xslt_string_data_collector = NULL;
		}

		return 0;
	}
#endif // XSLT_SUPPORT

	FramesDocument *frames_doc = GetFramesDocument();

	OP_ASSERT(frames_doc);

	if (used_url.GetURL().IsEmpty())
		used_url.SetURL(url);

	if (!more && aborted)
		is_aborted = aborted;

#ifndef USE_HTML_PARSER_FOR_XML
	if (xml_parsing_failed)
		len = 0;
#endif // !USE_HTML_PARSER_FOR_XML

	if (IsParsed())
	{
		if (!used_url.GetURL().IsEmpty() && !more)
		{
#ifdef _CSS_LINK_SUPPORT_
			hld_profile.CleanCSSLink();
#endif // _CSS_LINK_SUPPORT_
		}

		return 0;
	}

	if (!root)
	{
		switch (LoadStarted(url, src, len, more))
		{
		case LOADSTARTED_RETURN_ZERO:
			return 0;

		case LOADSTARTED_RETURN_LEN:
			return len;

		case LOADSTARTED_OOM:
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			hld_profile.SetIsOutOfMemory(TRUE);
			more = FALSE;
		}
	}

	BOOL is_finished = (!is_xml && m_parser && m_parser->IsFinished()) || (is_xml && xml_finished);

	if (hld_profile.GetESLoadManager()->HasData() ||
		!hld_profile.GetESLoadManager()->IsFinished())
		is_finished = FALSE;

#ifdef DELAYED_SCRIPT_EXECUTION
	if (hld_profile.ESHasDelayedScripts())
		is_finished = FALSE;
#endif // DELAYED_SCRIPT_EXECUTION

#ifndef USE_HTML_PARSER_FOR_XML
	if (!xml_finished)
		is_finished = FALSE;
#endif // !USE_HTML_PARSER_FOR_XML

	if (hld_profile.GetIsOutOfMemory() || is_finished)
	{
		if (!xml_parser || more)
		{
			if (!more)
			{
				StopLoading(FALSE);

				if (frames_doc->CheckInternal() == OpStatus::ERR_NO_MEMORY)
				{
					g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
					hld_profile.SetIsOutOfMemory(TRUE);
				}

#ifdef FORMAT_UNSTYLED_XML
				if (is_xml && !xml_parsing_failed && is_xml_unstyled && root)
					if (OpStatus::IsMemoryError(FormatUnstyledXml()))
					{
						g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
						hld_profile.SetIsOutOfMemory(TRUE);
					}
#endif // FORMAT_UNSTYLED_XML
			}

			return 0;
		}
	}

	uni_char *buf_start = src;
	uni_char *buf_end = src + len;

	ES_LoadManager *load_manager = hld_profile.GetESLoadManager();

	/* ES_LoadManager::IsBlocked returns TRUE if

	- there is an inline script thread that isn't blocked by a call
	to document.write{,ln}.
	- there is no inline script threads, but a generating thread
	which is not blocked by a call to document.write{,ln}.

	If the load manager is blocked, there is a chance that a script
	will call document.write{,ln} with data that should be processed
	before any data currently present, so we can't do anything here
	if the load manager is blocked. */

	while (ContinueParsing(load_manager, buf_start, buf_end, more))
	{
		const uni_char *base, *start, *end;
		unsigned length;
		BOOL from_load_manager;

		if (load_manager->HasData())
		{
			/* There are script generated data present, use that and not
			data from the URL. */

			from_load_manager = TRUE;
			load_manager->UpdateCurrentScriptElm(TRUE);
			base = start = GetPendingScriptData(length);
			if (base)
				end = start + length;
			else
				base = start = end = buf_start;
		}
		else
		{
			/* Use data from the URL.  This is the right thing to do only
			if there are no inline script threads or and no generating
			thread.  If there is, the load manager should either have
			been blocked or there should have been script generated
			data available. */

			from_load_manager = FALSE;
			base = start = buf_start;
			end = buf_end;
		}

		// A qualified guess; corrected in other places.
		load_manager->SetLastScriptType(SCRIPT_TYPE_INLINE);

#ifdef MANUAL_PLUGIN_ACTIVATION
		hld_profile.ESSetScriptExternal(load_manager->IsHandlingExternalScript(NULL));
#endif // MANUAL_PLUGIN_ACTIVATION

#ifndef USE_HTML_PARSER_FOR_XML
		if (is_xml)
		{
			unsigned consumed = 0;
			BOOL is_paused = FALSE;

			if (is_aborted)
				xml_finished = TRUE;
			else if (!xml_parsing_failed && !xml_parser->IsFailed())
			{
				if (xml_tokenhandler)
					xml_tokenhandler->ContinueIfBlocked();

# ifdef XML_DEGRADATION_IGNORE_LEADING_WHITESPACE
				if (!xml_parser_seen_content)
				{
					while (start < end && XMLUtils::IsSpace(*start))
						start++;

					if (start != end)
						xml_parser_seen_content = TRUE;
				}
# endif // XML_DEGRADATION_IGNORE_LEADING_WHITESPACE

				unsigned data_length = end - start;
				BOOL local_more_buffers = more;

				int first_invalid_character_offset = frames_doc->GetFirstInvalidCharacterOffset();
				if (first_invalid_character_offset >= 0)
				{
					OP_ASSERT((unsigned)first_invalid_character_offset < data_length);
					data_length = (unsigned)first_invalid_character_offset;
					local_more_buffers = TRUE;
				}

				if (OpStatus::IsMemoryError(xml_parser->Parse(start, data_length, local_more_buffers, &consumed)))
				{
					hld_profile.SetIsOutOfMemory(TRUE);
					break;
				}

				if (first_invalid_character_offset != -1 && !xml_parser->IsOutOfMemory() && !xml_parser->IsFailed() && consumed == data_length)
				{
					if (OpStatus::IsMemoryError(xml_parser->SignalInvalidEncodingError()))
					{
						hld_profile.SetIsOutOfMemory(TRUE);
						break;
					}
				}

				is_paused = xml_parser->IsPaused();
			}

			if (xml_parser->IsOutOfMemory())
			{
				hld_profile.SetIsOutOfMemory(TRUE);
				break;
			}
			else if (xml_parser->IsFailed())
				xml_parsing_failed = TRUE;
			else if (xml_parser->IsFinished())
				xml_finished = TRUE;
			else if (is_paused)
				frames_doc->GetMessageHandler()->PostMessage(MSG_URL_DATA_LOADED, frames_doc->GetURL().Id(TRUE), 0);

			start += consumed;

			if (frames_doc->CheckInternal() == OpStatus::ERR_NO_MEMORY)
				hld_profile.SetIsOutOfMemory(TRUE);
		}
		else
#endif // USE_HTML_PARSER_FOR_XML
		{
			OP_PARSER_STATUS status = FeedHtmlParser(start, end, more, FALSE, FALSE);
			if (OpStatus::IsSuccess(status))
				status = ParseHtml(more);
			if (OpStatus::IsMemoryError(status))
			{
				hld_profile.SetIsOutOfMemory(TRUE);
				break;
			}
		}

		if (frames_doc->GetDocManager()->GetRestartParsing())
		{
			/* If parsing will restart from the beginning, the only reasonable
			thing to do is to cancel all inline scripts. */

			if (OpStatus::IsMemoryError(load_manager->CancelInlineThreads()))
				hld_profile.SetIsOutOfMemory(TRUE);

			break;
		}

		if (from_load_manager)
		{
			if (hld_profile.GetIsOutOfMemory())
			{
				load_manager->HandleOOM();
				break;
			}

			if (is_xml)
				ConsumePendingScriptData(start - base);
			else
				OpStatus::Ignore(SetPendingScriptData(NULL, 0));

			load_manager->UpdateCurrentScriptElm(FALSE);
		}
		else
		{
			buf_start = const_cast<uni_char*>(start);
			break;
		}
	}

#ifdef MANUAL_PLUGIN_ACTIVATION
	hld_profile.ESSetScriptExternal(FALSE);
#endif // MANUAL_PLUGIN_ACTIVATION

	BOOL oom = hld_profile.GetIsOutOfMemory();

	if (oom)
	{
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		hld_profile.SetIsOutOfMemory(FALSE);
	}

	is_finished = !more && buf_start == buf_end;
	is_finished = is_finished && !IsContinuingParsing();

	if (!hld_profile.GetESLoadManager()->IsFinished())
		is_finished = FALSE;

#ifdef DELAYED_SCRIPT_EXECUTION
	if (hld_profile.ESHasDelayedScripts())
		is_finished = FALSE;
#endif // DELAYED_SCRIPT_EXECUTION

#ifndef USE_HTML_PARSER_FOR_XML
	if (!xml_finished && !xml_parsing_failed)
		is_finished = FALSE;
#endif // !USE_HTML_PARSER_FOR_XML

	if (oom || aborted || is_finished)
		m_parsing_done = TRUE;

#ifdef SVG_SUPPORT
	BOOL is_svg_resource = GetFramesDocument()->GetDocManager()->GetFrame() &&
		GetFramesDocument()->GetDocManager()->GetFrame()->IsSVGResourceDocument();
#endif // SVG_SUPPORT

#ifndef USE_HTML_PARSER_FOR_XML
	if (xml_parsing_failed && xml_parser->IsFailed() && is_finished)
	{
#ifdef SVG_SUPPORT
		if (is_svg_resource)
		{
			// Iframed SVG resource should not be reparsed automatically
			// nor an error page displayed, although it would be ignored.
			OpStatus::Ignore(xml_parser->ReportConsoleError());

			GetFramesDocument()->StopLoading(FALSE, TRUE, TRUE);
			GetFramesDocument()->CleanESEnvironment(TRUE);
			ClearLogdocRoot();
			svg_workplace->Clean();
			if (OpStatus::IsMemoryError(CreateLogdocRoot()))
				hld_profile.SetIsOutOfMemory(TRUE);
		}
		else
#endif // SVG_SUPPORT
		{
			BOOL do_reparse = FALSE;

			if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::ReparseFailedXHTML, url) &&
				(url.Type() == URL_HTTP || url.Type() == URL_HTTPS) &&
				url.GetAttribute(URL::KOriginalMIME_Type).Compare("application/xhtml+xml") == 0)
				do_reparse = TRUE;

# ifdef LOGDOC_REPARSE_XML_AS_HTML_FOR_WAP_MIME_TYPE
			if (url.GetAttribute(URL::KOriginalMIME_Type).Compare("text/vnd.wap.wml") == 0)
				do_reparse = TRUE;
# endif // LOGDOC_REPARSE_XML_AS_HTML_FOR_WAP_MIME_TYPE

# ifdef ECMASCRIPT_DEBUGGER
			/* Do not reparse if ecmascript debugging. */
			if (g_ecmaManager->IsDebugging(frames_doc->GetESRuntime()))
				do_reparse = FALSE;
# endif // ECMASCRIPT_DEBUGGER

			WindowCommander *wc = frames_doc->GetWindow()->GetWindowCommander();
			wc->GetLoadingListener()->OnXmlParseFailure(wc);

			if (do_reparse)
			{
#ifdef OPERA_CONSOLE
				OpStatus::Ignore(xml_parser->ReportConsoleError());
#endif // OPERA_CONSOLE
				MessageHandler *mh = frames_doc->GetMessageHandler();
				mh->SetCallBack(frames_doc, MSG_REPARSE_AS_HTML, 0);
				mh->PostMessage(MSG_REPARSE_AS_HTML, 0, 0);
			}
			else
			{
				if (OpStatus::IsMemoryError(GenerateXMLParsingFailedErrorPage()))
					hld_profile.SetIsOutOfMemory(TRUE);
				url.StopLoading(frames_doc->GetMessageHandler());
			}
		}
	}
#endif // !USE_HTML_PARSER_FOR_XML

	if (is_finished && !used_url.GetURL().IsEmpty())
	{
		StopLoading(FALSE);

		if (frames_doc->CheckInternal() == OpStatus::ERR_NO_MEMORY)
		{
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			hld_profile.SetIsOutOfMemory(TRUE);
		}

#ifdef FORMAT_UNSTYLED_XML
#ifdef SVG_SUPPORT
		if (!is_svg_resource)
		// In case a SVG resource is actually not proper SVG, this part can be skipped.
#endif // SVG_SUPPORT
		if (is_xml && !xml_parsing_failed && is_xml_unstyled)
			if (OpStatus::IsMemoryError(FormatUnstyledXml()))
			{
				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				hld_profile.SetIsOutOfMemory(TRUE);
			}
#endif // FORMAT_UNSTYLED_XML
	}
	else
	{
		ES_LoadManager *load_manager = hld_profile.GetESLoadManager();
		BOOL need_flush = load_manager->GetScriptGeneratingDoc() && !load_manager->HasScripts();

# ifdef DELAYED_SCRIPT_EXECUTION
		need_flush = need_flush || hld_profile.ESHasDelayedScripts() && !more && buf_start == buf_end;
# endif // DELAYED_SCRIPT_EXECUTION

		if (need_flush)
			if (frames_doc->CheckInternal() == OpStatus::ERR_NO_MEMORY)
			{
				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				hld_profile.SetIsOutOfMemory(TRUE);
			}
	}

	return buf_end - buf_start;
}

#ifdef SPECULATIVE_PARSER
OP_STATUS LogicalDocument::LoadSpeculatively(URL& url, const uni_char* src, int len, unsigned &rest, BOOL more)
{
	if (!m_speculative_parser)
		RETURN_IF_ERROR(HTML5SpeculativeParser::Make(m_speculative_parser, url, *hld_profile.BaseURL(), this));

	if (m_speculative_state)
	{
		m_speculative_parser->RestoreState(m_speculative_state, m_speculative_buffer_offset, *hld_profile.BaseURL());
		OP_DELETE(m_speculative_state);
		m_speculative_state = NULL;
	}

	if (len != 0)
	{
		RETURN_IF_ERROR(m_speculative_parser->AppendData(src, len, !more));

		rest = 0;
		RETURN_IF_ERROR(m_speculative_parser->Parse());
	}
	return OpStatus::OK;
}

BOOL LogicalDocument::IsSpeculativeParserFinished()
{
	return m_speculative_parser->IsFinished();
}
#endif // SPECULATIVE_PARSER

#ifdef APPLICATION_CACHE_SUPPORT
URL* LogicalDocument::GetManifestUrl()
{
	HTML_Element* html_elm = GetDocRoot(); return html_elm ? html_elm->GetUrlAttr(ATTR_MANIFEST, NS_IDX_HTML, this) : NULL;
}
#endif // APPLICATION_CACHE_SUPPORT



const uni_char* LogicalDocument::GetTitle(HTML_Element* root, TempBuffer* buffer)
{
	if (root)
	{
		const uni_char* raw_title = NULL;
#ifdef _WML_SUPPORT_
		if (root == this->root && IsWml())
		{
			HTML_Element* title_elm = hld_profile.WMLGetCurrentCard();
			if (title_elm)
				raw_title = title_elm->GetAttrValue(WA_TITLE, NS_IDX_WML);
		}
		else
#endif // _WML_SUPPORT_
		{
			HTML_Element* title_elm = root->GetTitle();
			if (!title_elm)
				return NULL;

			size_t buf_size = title_elm->GetTextContentLength() +1;
			if (OpStatus::IsMemoryError(buffer->Expand(buf_size)))
				return NULL;
			title_elm->GetTextContent(buffer->GetStorage(), buf_size);
			raw_title = buffer->GetStorage();
		}

		if (!raw_title)
			return raw_title;

		// Remove line breaks and trailing and prefixed whitespace from the title so that it can easily be used in UI:s.
		if (raw_title != buffer->GetStorage())
		{
			buffer->Clear();
			if (OpStatus::IsError(buffer->Append(raw_title)))
				return raw_title;
		}
		uni_char* cleaned_title = buffer->GetStorage();
		OP_ASSERT(cleaned_title);

		CleanStringForDisplay(cleaned_title);
		return cleaned_title;
	}

	return NULL;
}

HTML_Element* LogicalDocument::GetNamedHE(const uni_char* name)
{
	return root ? root->GetNamedElm(name) : NULL;
}

HTML_Element* LogicalDocument::GetFirstHE(HTML_ElementType tpe)
{
	if (root)
		return root->GetFirstElm(tpe);
	else
		return 0;
}

#ifdef LOGDOCXMLTREEACCESSOR_SUPPORT

OP_STATUS LogicalDocument::CreateXMLTreeAccessor(XMLTreeAccessor *&treeaccessor, XMLTreeAccessor::Node *&node, HTML_Element *element, URL *url, const XMLDocumentInformation *docinfo)
{
	HTML_Element *use_root;

	if (!element || root->IsAncestorOf(element))
	{
		use_root = root;
		if (!docinfo)
			docinfo = GetXMLDocumentInfo();
	}
	else
	{
		use_root = element;
		while (HTML_Element *parent = use_root->ParentActual())
			use_root = parent;
	}

	if (!docinfo)
		if (use_root->Type() == HE_DOC_ROOT)
		{
			HTML_Element *doctype = use_root->FirstChild();
			while (doctype && doctype->Type() != HE_DOCTYPE)
				doctype = doctype->Suc();
			if (doctype)
				docinfo = doctype->GetXMLDocumentInfo();
		}

	LogdocXMLTreeAccessor *accessor = OP_NEW(LogdocXMLTreeAccessor, (this, use_root));

	if (!accessor || docinfo && OpStatus::IsMemoryError(accessor->SetDocumentInformation(docinfo)))
		return OpStatus::ERR_NO_MEMORY;

	accessor->SetDocumentURL(url ? *url : GetFramesDocument()->GetURL());

	treeaccessor = accessor;
	node = accessor->GetElementAsNode(element ? element : use_root);

	return OpStatus::OK;
}

/* static */
void LogicalDocument::FreeXMLTreeAccessor(XMLTreeAccessor *treeaccessor)
{
	OP_DELETE(static_cast<LogdocXMLTreeAccessor*>(treeaccessor));
}

/* static */
XMLTreeAccessor::Node* LogicalDocument::GetElementAsNode(XMLTreeAccessor *tree, HTML_Element *element)
{
	return LogdocXMLTreeAccessor::GetElementAsNode(element);
}

/* static */
HTML_Element* LogicalDocument::GetNodeAsElement(XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode)
{
	return LogdocXMLTreeAccessor::GetNodeAsElement(treenode);
}

#endif // LOGDOCXMLTREEACCESSOR_SUPPORT

HTML_Element* LogicalDocument::GetDOMObjectById(const uni_char* name)
{
	return root ? root->GetElmById(name) : NULL;
}

void LogicalDocument::AddEventHandler(DOM_EventType type)
{
	if (FramesDocument *doc = GetFramesDocument())
		if (DOM_Environment *environment = doc->GetDOMEnvironment())
			environment->AddEventHandler(type);
		else
		{
#ifndef MOUSELESS
			if (type == ONMOUSEMOVE)
				++onmousemove_count;
			else if (type == ONMOUSEOVER)
				++onmouseover_count;
			else if (type == ONMOUSEENTER)
				++onmouseenter_count;
			else if (type == ONMOUSEOUT)
				++onmouseout_count;
			else if (type == ONMOUSELEAVE)
				++onmouseleave_count;
#ifdef TOUCH_EVENTS_SUPPORT
			else if (type == TOUCHSTART)
				++touchstart_count;
			else if (type == TOUCHMOVE)
				++touchmove_count;
			else if (type == TOUCHEND)
				++touchend_count;
			else if (type == TOUCHCANCEL)
				++touchcancel_count;
#endif // TOUCH_EVENTS_SUPPORT
#endif // MOUSELESS
#ifdef RESERVED_REGIONS
			if (FramesDocument::IsReservedRegionEvent(type))
				doc->SignalReservedRegionChange();
#endif // RESERVED_REGIONS
		}
}

void LogicalDocument::RemoveEventHandler(DOM_EventType type)
{
	if (FramesDocument *doc = GetFramesDocument())
		if (DOM_Environment *environment = doc->GetDOMEnvironment())
			environment->RemoveEventHandler(type);
		else
		{
#ifndef MOUSELESS
			if (type == ONMOUSEMOVE)
				--onmousemove_count;
			else if (type == ONMOUSEOVER)
				--onmouseover_count;
			else if (type == ONMOUSEENTER)
				--onmouseenter_count;
			else if (type == ONMOUSEOUT)
				--onmouseout_count;
			else if (type == ONMOUSELEAVE)
				--onmouseleave_count;
#ifdef TOUCH_EVENTS_SUPPORT
			else if (type == TOUCHSTART)
				--touchstart_count;
			else if (type == TOUCHMOVE)
				--touchmove_count;
			else if (type == TOUCHEND)
				--touchend_count;
			else if (type == TOUCHCANCEL)
				--touchcancel_count;
#endif // TOUCH_EVENTS_SUPPORT
#endif // MOUSELESS
#ifdef RESERVED_REGIONS
			if (FramesDocument::IsReservedRegionEvent(type))
				doc->SignalReservedRegionChange();
#endif // RESERVED_REGIONS
		}
}

BOOL LogicalDocument::GetEventHandlerCount(DOM_EventType type, unsigned &count)
{
#ifndef MOUSELESS
	if (type == ONMOUSEMOVE)
		count = onmousemove_count;
	else if (type == ONMOUSEOVER)
		count = onmouseover_count;
	else if (type == ONMOUSEENTER)
		count = onmouseenter_count;
	else if (type == ONMOUSEOUT)
		count = onmouseout_count;
	else if (type == ONMOUSELEAVE)
		count = onmouseleave_count;
#ifdef TOUCH_EVENTS_SUPPORT
	else if (type == TOUCHSTART)
		count = touchstart_count;
	else if (type == TOUCHMOVE)
		count = touchmove_count;
	else if (type == TOUCHEND)
		count = touchend_count;
	else if (type == TOUCHCANCEL)
		count = touchcancel_count;
#endif // TOUCH_EVENTS_SUPPORT
	else
		return FALSE;
	return TRUE;
#else // MOUSELESS
	return FALSE;
#endif // MOUSELESS
}

OP_STATUS LogicalDocument::AddNamedElement(HTML_Element *element, BOOL descendants, BOOL force)
{
	BOOL is_full_tree_scan = FALSE;

	if (!namedelementsmap)
		if (!force)
			return OpStatus::OK;
		else
		{
			if (!(namedelementsmap = OP_NEW(NamedElementsMap, ())))
				return OpStatus::ERR_NO_MEMORY;
			is_full_tree_scan = TRUE;
		}

#ifdef DELAYED_SCRIPT_EXECUTION
	if (element->GetInserted() == HE_INSERTED_BY_PARSE_AHEAD)
		return OpStatus::OK;
#endif // DELAYED_SCRIPT_EXECUTION

	for (HTML_Element *stop = descendants ? element->NextSiblingActual() : NULL; element != stop; element = descendants ? element->NextActual() : NULL)
	{
		const uni_char *id, *name = element->GetName();

		HTML_ImmutableAttrIterator idattr_iter(element);
		while ((id = idattr_iter.GetNextId()) || name)
		{
			const uni_char *current = id;
			NamedElements *namedelements;

		again:
			if (current)
				if (namedelementsmap->GetData(current, &namedelements) == OpStatus::OK)
				{
					int count = namedelements->count;
					HTML_Element **elements = count == 1 ? &namedelements->data.single : namedelements->data.elements;
					int i;

					for (i = count - 1; i >= 0; --i)
						if (elements[i] == element)
							break;
					if (i >= 0)
						goto skip;

					for (i = count; i > 0; i >>= 1)
						if (i == 1)
						{
							HTML_Element **new_elements = OP_NEWA(HTML_Element *, count * 2);
							if (!new_elements)
								goto oom_error;
							op_memcpy(new_elements, elements, count * sizeof(HTML_Element *));
							if (count != 1)
								OP_DELETEA(namedelements->data.elements);
							elements = namedelements->data.elements = new_elements;
						}
						else if (i & 1)
							break;

					if (is_full_tree_scan || !element->Next() || elements[count - 1]->Precedes(element))
						i = count;
					else
					{
						if (element->Precedes(elements[0]))
							i = 0;
						else if (count == 2)
							i = 1;
						else
						{
							int hi = count - 2;
							int lo = 1;

							while (hi >= lo)
							{
								i = (hi + lo) / 2;

								if (elements[i]->Precedes(element))
									lo = ++i;
								else
									hi = i - 1;
							}
						}

						for (int j = count; j > i; --j)
							elements[j] = elements[j - 1];
					}

					elements[i] = element;
					++namedelements->count;
				}
				else
				{
					namedelements = OP_NEW(NamedElements, ());
					if (!namedelements || !(namedelements->name = UniSetNewStr(current)))
					{
						OP_DELETE(namedelements);
						goto oom_error;
					}

					namedelements->count = 1;
					namedelements->data.single = element;

					OP_STATUS status;
					if (OpStatus::IsMemoryError(status = namedelementsmap->Add(namedelements->name, namedelements)))
					{
						OP_DELETEA(namedelements->name);
						OP_DELETE(namedelements);
						goto oom_error;
					}
					OP_ASSERT(OpStatus::IsSuccess(status));
				}

		skip:
			if (name && current != name && (!id || !uni_str_eq(id, name)))
			{
				current = name;
				goto again;
			}

			name = NULL;
		}
	}

	return OpStatus::OK;

oom_error:
	OP_DELETE(namedelementsmap);
	namedelementsmap = NULL;

	return OpStatus::ERR_NO_MEMORY;
}

OP_STATUS LogicalDocument::RemoveNamedElementFrom(HTML_Element *element, NamedElements *namedelements)
{
	HTML_Element **elements = namedelements->count == 1 ? &namedelements->data.single : namedelements->data.elements;
	int i;

	for (i = namedelements->count - 1; i >= 0; --i)
		if (elements[i] == element)
		{
			if (namedelements->count == 1)
			{
				namedelementsmap->Remove(namedelements->name, &namedelements);

				OP_DELETEA(namedelements->name);
				OP_DELETE(namedelements);
			}
			else
			{
				int count = --namedelements->count;

				if (count == 1)
				{
					HTML_Element *tmp = elements[i == 0 ? 1 : 0];
					OP_DELETEA(elements);
					namedelements->data.single = tmp;
				}
				else
				{
					for (int j = i + 1; j <= count; ++j)
						elements[j - 1] = elements[j];

					// If the new count is an exact multiple of 2, shrink the
					// array
					OP_ASSERT(count > 0); // Or the check below will fail
					BOOL is_multiple_of_2 = (count & (count - 1)) == 0;

					if (is_multiple_of_2)
					{
						HTML_Element **new_elements = OP_NEWA(HTML_Element *, count);
						if (!new_elements)
							return OpStatus::ERR_NO_MEMORY;
						op_memcpy(new_elements, elements, count * sizeof(HTML_Element *));
						OP_DELETEA(elements);
						namedelements->data.elements = new_elements;
					}
				}
			}

			break;
		}

	return OpStatus::OK;
}

OP_STATUS LogicalDocument::RemoveNamedElement(HTML_Element *element, BOOL descendants)
{
	if (!namedelementsmap)
		return OpStatus::OK;

#ifdef DELAYED_SCRIPT_EXECUTION
	if (element->GetInserted() == HE_INSERTED_BY_PARSE_AHEAD)
		return OpStatus::OK;
#endif // DELAYED_SCRIPT_EXECUTION

	if (descendants)
	{
		for (HTML_Element *iter = element, *stop = (HTML_Element *) iter->NextSiblingActual(); iter != stop; iter = (HTML_Element *) iter->NextActual())
			if (OpStatus::IsMemoryError(RemoveNamedElement(iter, FALSE)))
				goto oom_error;
	}
	else
	{
		const uni_char *name = element->GetName(), *id;

		HTML_ImmutableAttrIterator idattr_iter(element);
		while ((id = idattr_iter.GetNextId()) || name)
		{
			const uni_char *current = id;

			NamedElements *namedelements;

		again:
			if (current && namedelementsmap->GetData(current, &namedelements) == OpStatus::OK)
				if (OpStatus::IsMemoryError(RemoveNamedElementFrom(element, namedelements))) goto oom_error;

			if (name && current != name && (!id || !uni_str_eq(id, name)))
			{
				current = name;
				goto again;
			}

			name = NULL;
		}
	}

	return OpStatus::OK;

oom_error:
	OP_DELETE(namedelementsmap);
	namedelementsmap = NULL;

	return OpStatus::ERR_NO_MEMORY;
}

void LogicalDocument::GetNamedElements(NamedElementsIterator &iterator, const uni_char *name, BOOL check_id, BOOL check_name)
{
	if (!root)
		return;

	if (!namedelementsmap)
		if (OpStatus::IsMemoryError(AddNamedElement(root, TRUE, TRUE)))
			return;

	NamedElements *namedelements;
	if (namedelementsmap->GetData(name, &namedelements) == OpStatus::OK)
		iterator.Set(namedelements, NULL, check_id, check_name);
}

int LogicalDocument::SearchNamedElements(NamedElementsIterator &iterator, HTML_Element *parent, const uni_char *name, BOOL check_id, BOOL check_name)
{
	if (!root)
		return 0;

	if (!namedelementsmap)
		if (OpStatus::IsMemoryError(AddNamedElement(root, TRUE, TRUE)))
			return -1;

	NamedElements *namedelements;

	if (namedelementsmap->GetData(name, &namedelements) == OpStatus::OK)
	{
		iterator.Set(namedelements, parent, check_id, check_name);

		int count;

		if (!parent && check_id && check_name)
			count = namedelements->count;
		else
		{
			count = 0;
			while (iterator.GetNextElement())
				++count;
			iterator.Reset();
		}

		return count;
	}
	else
		return 0;
}

void LogicalDocument::SetCompleted(BOOL forced/*=FALSE*/, BOOL is_printing/*=FALSE*/)
{
	if (is_printing)
		return;

	if (forced || layout_workplace->IsTraversable() && IsParsed())
	{
#ifdef DELAYED_SCRIPT_EXECUTION
		if (hld_profile.ESHasDelayedScripts())
			return;
#endif // DELAYED_SCRIPT_EXECUTION

		loading_finished = TRUE;
	}
}

int LogicalDocument::GetElmNumber(HTML_Element* he)
{
	if (root)
	{
		BOOL found = FALSE;
		int i = root->GetElmNumber(he, found);
		return (found) ? i : 0;
	}
	else
		return 0;
}

HTML_Element* LogicalDocument::GetElmNumber(int elm_nr)
{
	int i = 0;
	if (root)
		return root->GetElmNumber(elm_nr, i);
	else
		return 0;
}

HtmlDocType LogicalDocument::GetHtmlDocType()
{
	if (hld_profile.IsFramesDoc())
		return HTML_DOC_FRAMES;
	else
	{
		BOOL is_plain = HasContent() || IsParsed();
		is_plain |= IsXml();
#ifdef _WML_SUPPORT_
		is_plain |= IsWml();
#endif // _WML_SUPPORT_
		if (is_plain)
			return HTML_DOC_PLAIN;
	}
	return HTML_DOC_UNKNOWN;
}

void LogicalDocument::StopLoading(BOOL aborted)
{
	m_continue_parsing = FALSE;
	if (m_parser)
	{
		TRAPD(sstat, m_parser->StopParsingL(aborted));
		if (OpStatus::IsMemoryError(sstat))
			hld_profile.SetIsOutOfMemory(TRUE);

		OP_DELETE(m_parser);
		m_parser = NULL;
	}
	m_parsing_done = TRUE;

#ifdef SPECULATIVE_PARSER
	DisableSpeculativeParser();
#endif // SPECULATIVE_PARSER

#ifdef DELAYED_SCRIPT_EXECUTION
	hld_profile.ESStopLoading();
#endif // DELAYED_SCRIPT_EXECUTION

	hld_profile.GetESLoadManager()->StopLoading();

#ifdef _CSS_LINK_SUPPORT_
	if (!used_url.GetURL().IsEmpty())
		hld_profile.CleanCSSLink();
#endif // _CSS_LINK_SUPPORT_

	FreeXMLParsingObjects();
}

#ifdef _WML_SUPPORT_
void LogicalDocument::WMLReEvaluate()
{
    if (root)
        WMLEvaluateBranch(root->FirstChild(), hld_profile.GetFramesDocument()->GetDocManager());
}

void LogicalDocument::WMLEvaluateBranch(HTML_Element *h_elm, DocumentManager *dm)
{
    if (h_elm)
    {
        HTML_Element *tmp_elm = h_elm;
        while (tmp_elm)
        {
			if (OpStatus::IsMemoryError(tmp_elm->WMLInit(dm)))
				if (GetHLDocProfile())
					GetHLDocProfile()->SetIsOutOfMemory(TRUE);

			WMLEvaluateBranch(tmp_elm->FirstChildActual(), dm);

            if ((WML_ElementType)tmp_elm->Type() == WE_CARD && tmp_elm->GetNsIdx() == NS_IDX_WML)
				hld_profile.WMLGetContext()->SetStatusOff(WS_FIRSTCARD | WS_INRIGHTCARD);

            tmp_elm = (HTML_Element *) tmp_elm->SucActual();
        }
    }
}
#endif // _WML_SUPPORT_

BOOL LogicalDocument::ESFlush(const uni_char *str, unsigned str_len, BOOL end_with_newline, ES_Thread *thread/*=NULL*/)
{
	OP_PROBE4(OP_PROBE_LOGDOC_ESFLUSH);

	ES_LoadManager *load_manager = hld_profile.GetESLoadManager();

#ifdef DELAYED_SCRIPT_EXECUTION
	if (hld_profile.ESIsExecutingDelayedScript())
		hld_profile.ESFlushDelayed();
#endif // DELAYED_SCRIPT_EXECUTION

	if (hld_profile.IsXml())
		return TRUE;

	BOOL already_parsing = GetRoot() != NULL;
	URL url = GetFramesDocument()->GetURL();
	URLContentType content_type = url.ContentType();
	BOOL is_text = content_type == URL_TEXT_CONTENT;
	if ((already_parsing && is_text) ||
		content_type == URL_CSS_CONTENT ||
		content_type == URL_X_JAVASCRIPT)
		return TRUE;

#ifdef MANUAL_PLUGIN_ACTIVATION
	hld_profile.ESSetScriptExternal(load_manager->IsHandlingExternalScript(thread));
#endif // MANUAL_PLUGIN_ACTIVATION

	OP_STATUS status;
	if (already_parsing)
	{
		load_manager->SetLastScriptType(SCRIPT_TYPE_GENERATED);

		const uni_char *start = str;
		status = FeedHtmlParser(start, start + str_len, TRUE, TRUE, end_with_newline);

		if (OpStatus::IsSuccess(status))
			status = ParseHtml(TRUE);
	}
	else
	{
		load_manager->SetLastScriptType(SCRIPT_TYPE_INLINE);

		status = ParseHtmlFromString(str, str_len, is_text, TRUE, end_with_newline);
		if (OpStatus::IsMemoryError(status))
			hld_profile.SetIsOutOfMemory(TRUE);
	}

#ifdef MANUAL_PLUGIN_ACTIVATION
	hld_profile.ESSetScriptExternal(FALSE);
#endif // MANUAL_PLUGIN_ACTIVATION

	if (OpStatus::IsMemoryError(status))
		hld_profile.SetIsOutOfMemory(TRUE);

	return TRUE;
}

#ifdef _PRINT_SUPPORT_
void LogicalDocument::CreatePrintCopy()
{
	OP_ASSERT(!print_root);

	if (root)
		print_root = root->CopyAll(GetHLDocProfile());//FIXME:OOM
}

void LogicalDocument::DeletePrintCopy()
{
	if (print_root)
	{
		if (print_root->Clean(hld_profile.GetFramesDocument()))
			print_root->Free(hld_profile.GetFramesDocument());
		else
			/* The print_root should not contain dom/script or anything else
			 * that makes it dangerous to Free directly. It will crash later
			 * when they are removed.  /emil
			 */
			OP_ASSERT(!"Emil thinks this should never happen.");

		print_root = NULL;
	}
}
#endif // _PRINT_SUPPORT_

#if defined SAVE_DOCUMENT_AS_TEXT_SUPPORT
OP_STATUS LogicalDocument::WriteAsText(UnicodeOutputStream* out, int max_line_length, HTML_Element* start_element)
{
    if (!start_element)
        start_element = GetRoot();

	if (start_element)
	{
		int line_length = -1;
		BOOL trailing_ws = FALSE;
		BOOL prevent_newline = FALSE;
		BOOL pending_newline = FALSE;

		Head props_list;
		LayoutProperties* cascade = new LayoutProperties();
		if (!cascade)
			return OpStatus::ERR_NO_MEMORY;

		OP_STATUS status = cascade->InitProps();
		if (OpStatus::IsMemoryError(status))
		{
			OP_DELETE(cascade);
			return status;
		}
		hld_profile.GetVisualDevice()->Reset();

		// Set ua default values
		cascade->GetProps()->Reset(NULL, &hld_profile);
		cascade->Into(&props_list);

		status = start_element->WriteAsText(out, &hld_profile, cascade, max_line_length, line_length, trailing_ws, prevent_newline, pending_newline);

		props_list.Clear();
		return status;
	}
	else
		return OpStatus::OK;
}
#endif // SAVE_DOCUMENT_AS_TEXT_SUPPORT

BOOL LogicalDocument::GetBoxRect(HTML_Element* element, BoxRectType type, RECT& rect)
{
	FramesDocument* frm_doc = GetFramesDocument();

	if (root && frm_doc)
	{
		if (frm_doc->Reflow(FALSE) == OpStatus::ERR_NO_MEMORY)
			frm_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);

		return element->GetLayoutBox() && element->GetLayoutBox()->GetRect(frm_doc, type, rect);
	}

	return FALSE;
}

BOOL LogicalDocument::GetBoxRect(HTML_Element* element, RECT& rect)
{
	FramesDocument* frm_doc = GetFramesDocument();

	if (root && frm_doc)
	{
		if (frm_doc->Reflow(FALSE) == OpStatus::ERR_NO_MEMORY)
			frm_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);

		if (element->GetLayoutBox())
		{
			BoxEdgesObject box_edges(frm_doc, element, root);

			box_edges.Traverse(root);

			if (box_edges.GetBoxFound())
			{
				rect = box_edges.GetBoxEdges();

				return TRUE;
			}
		}
	}

	return FALSE;
}

BOOL LogicalDocument::GetBoxClientRect(HTML_Element* element, RECT& rect)
{
	return FALSE;
}

BOOL LogicalDocument::GetCliprect(HTML_Element* element, RECT& rect)
{
	FramesDocument* frm_doc = GetFramesDocument();

	if (root && frm_doc && !frm_doc->IsReflowing())
	{
		// Must reflow first so we don't feed CliprectObject with a layoutbox that will be
		// removed in root->Traverse and then used.

		if (frm_doc->Reflow(FALSE) == OpStatus::ERR_NO_MEMORY) // FIXME: OOM
			return FALSE;

		if (!element->GetLayoutBox())
			return FALSE;

		CliprectObject cliprect(frm_doc, element->GetLayoutBox());

		cliprect.Traverse(root);

		BOOL ret = cliprect.GetCliprect(rect);

		if (ret)
		{
			VisualDevice* visual_device = frm_doc->GetVisualDevice();

			// Intersect with the visible area of the document
			OpRect oprect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
			oprect.IntersectWith(visual_device->GetRenderingViewport());
			rect.left = oprect.x;
			rect.top = oprect.y;
			rect.right = oprect.x + oprect.width;
			rect.bottom = oprect.y + oprect.height;
		}

		return ret;
	}

	return FALSE;
}

void LogicalDocument::RemoveFromParseState(HTML_Element* element)
{
#ifdef DELAYED_SCRIPT_EXECUTION
	// This might change the parse state
	if (OpStatus::IsMemoryError(hld_profile.ESElementRemoved(element)))
		hld_profile.SetIsOutOfMemory(TRUE);
#endif // DELAYED_SCRIPT_EXECUTION
	if (xml_tokenhandler)
		xml_tokenhandler->RemovingElement(element);
}

#ifdef XSLT_SUPPORT

void LogicalDocument::CleanUpAfterXSLT(BOOL aborted, HTML_Element *root_element)
{
	if (!is_xml && xml_parser && !aborted)
	{
		/* Not strictly necessary, I suppose.  The parser is pretty much
		   guaranteed to be in the state that it has given the token handler
		   a XMLToken::TYPE_Finished token, which blocked, and is now
		   waiting to be called again so that it can say that it has
		   finished. */
		OpStatus::Ignore(xml_parser->Parse(UNI_L(""), 0, FALSE));
		OP_ASSERT(xml_parser->IsFinished());
	}

# ifndef USE_HTML_PARSER_FOR_XML
	xml_finished = TRUE;
# endif // USE_HTML_PARSER_FOR_XML

	OP_DELETE(xml_parser);
	xml_parser = NULL;

	if (xslt_string_data_collector && (xslt_string_data_collector->IsFinished() || aborted))
	{
		OP_DELETE(xslt_string_data_collector);
		xslt_string_data_collector = NULL;
	}

	OpString *error_message = xslt_handler->GetErrorMessage();

	if (root_element)
	{
		while (HTML_Element *existing = root->FirstChild())
		{
			existing->Remove(this);
			OpStatus::Ignore(root->HandleDocumentTreeChange(this, root, existing, NULL, FALSE));
			if (existing->Clean(this))
				existing->Free(this);
		}

		root->MarkExtraDirty(GetFramesDocument());
		doc_root.Reset();

		while (HTML_Element *child = root_element->FirstChild())
		{
			child->Remove(this);
			child->UnderSafe(this, root);
		}

#ifdef FORMAT_UNSTYLED_XML
		OpStatus::Ignore(FormatUnstyledXml(error_message ? error_message->CStr() : NULL, TRUE));
#endif // FORMAT_UNSTYLED_XML
	}

	OP_DELETE(error_message);
}

OP_STATUS LogicalDocument::PrepareForHTMLOutput(LogdocXSLTStringDataCollector *string_data_collector)
{
	is_xml = FALSE;
	is_text = FALSE;

	xslt_string_data_collector = string_data_collector;

#ifdef DELAYED_SCRIPT_EXECUTION
	hld_profile.ESDisableDelayScripts();
#endif // DELAYED_SCRIPT_EXECUTION

	return OpStatus::OK;
}

OP_STATUS LogicalDocument::PrepareForTextOutput(LogdocXSLTStringDataCollector *string_data_collector)
{
	is_xml = FALSE;
	is_text = TRUE;

	RETURN_IF_ERROR(ConstructTextDocumentStructure());

	xslt_string_data_collector = string_data_collector;
	return OpStatus::OK;
}

/* Should be fixed for morph_2. */

unsigned LogicalDocument::GetXSLTStylesheetsCount()
{
	return 0;
}

URL LogicalDocument::GetXSLTStylesheetURL(unsigned index)
{
	return URL();
}

#endif // XSLT_SUPPORT

OP_STATUS LogicalDocument::FeedHtmlParser(const uni_char*& buf_start, const uni_char* buf_end, BOOL more, BOOL from_doc_write, BOOL add_newline)
{
	if (!m_parser)
	{
		m_parser = OP_NEW(HTML5Parser, (this));
		if (!m_parser)
			return OpStatus::ERR_NO_MEMORY;

#ifdef SELFTEST
		if (g_selftest.doc_start_line_number != 0 && GetFramesDocument() == g_selftest.GetTestDoc())
		{
			m_parser->SetFirstLineNumber(g_selftest.doc_start_line_number);
			g_selftest.doc_start_line_number = 0;
		}
#endif // SELFTEST
	}

	ES_LoadManager *load_manager = hld_profile.GetESLoadManager();
	BOOL append = !from_doc_write && !load_manager->IsWriting();

	if (buf_start != buf_end || add_newline)
	{
		OP_STATUS data_stat;
		if (append)
			TRAP(data_stat, m_parser->AppendDataL(buf_start, buf_end - buf_start, !more, FALSE));
		else
			TRAP(data_stat, m_parser->InsertDataL(buf_start, buf_end - buf_start, add_newline));

		if (OpStatus::IsMemoryError(data_stat))
			return data_stat;

		buf_start = buf_end;
	}
	else if (!more)
		RETURN_IF_LEAVE(m_parser->SignalNoMoreDataL(FALSE));

	return OpStatus::OK;
}

OP_STATUS LogicalDocument::ParseHtmlFromString(const uni_char *string, unsigned string_length, BOOL treat_as_text, BOOL from_doc_write, BOOL add_newline)
{
	OP_ASSERT(!root || !"This should only be called for empty documents.");

	if (treat_as_text)
		is_text = TRUE;
	RETURN_IF_MEMORY_ERROR(CreateLogdocRoot());
	RETURN_IF_MEMORY_ERROR(FeedHtmlParser(string, string + string_length, FALSE, from_doc_write, add_newline));
	RETURN_IF_MEMORY_ERROR(ParseHtml(FALSE));

	return OpStatus::OK;
}

OP_STATUS LogicalDocument::ParseHtml(BOOL more)
{
	OP_PROBE6(OP_PROBE_LOGDOC_LOADHTML);

	FramesDocument* frames_doc = GetFramesDocument();

	DOCUMENT_PARSING_PROBE(probe, frames_doc, GetURL());

#if LOGDOC_LOADHTML_YIELDMS > 0
	loadhtml_yieldms = g_op_time_info->GetRuntimeMS() + LOGDOC_LOADHTML_YIELDMS;
#endif // LOGDOC_HTMLLOAD_YIELDMS > 0

	OP_ASSERT(m_parser);

	OP_PARSER_STATUS pstat = OpStatus::OK;

	if (m_parser->IsParsingStarted())
		TRAP(pstat, m_parser->ContinueParsingL());
	else
	{
		if (!root)
		{
			m_continue_parsing = TRUE;
			return HTML5ParserStatus::PARSING_POSTPONED;
		}

		m_parser->SetIsPlainText(is_text);
		pstat = m_parser->Parse(root, NULL);
	}

	m_continue_parsing = FALSE;

	ES_LoadManager *load_manager = hld_profile.GetESLoadManager();

	// If the parser LEAVEs with a non-OK, non-ERROR status it is to indicate
	// that we will need to call the parser again at a later stage to finish
	// the parsing. If, in that case, there will be no more messages posted to
	// continue because both the URL and the load manager has exhausted their
	// data streams, we need to post a loading message ourself.
	if (pstat != OpStatus::OK && !OpStatus::IsError(pstat) && !more && load_manager->IsFinished())
	{
		m_continue_parsing = TRUE;
		frames_doc->GetMessageHandler()->PostMessage(MSG_URL_DATA_LOADED, frames_doc->GetURL().Id(TRUE), 0);
	}

	if (pstat == HTML5ParserStatus::FINISHED_DOC_WRITE)
	{
		TRAP(pstat, m_parser->FlushTextL());
		load_manager->SignalWriteFinished();
	}

#ifdef SPECULATIVE_PARSER
	if (pstat == HTML5ParserStatus::EXECUTE_SCRIPT && m_parser->IsBlocking() && (!m_speculative_parser || !m_speculative_parser->IsFinished()))
	{
		// TODO: Check that the parser is actually blocking on an _external_script_ (non data URI?).
		OP_ASSERT(m_parser->IsParsingStarted());
		OP_ASSERT(!m_parser->IsFinished());

		pstat = MaybeEnableSpeculativeParser();
	}
#endif // SPECULATIVE_PARSER

#if LOGDOC_LOADHTML_YIELDMS > 0
	loadhtml_yieldms = 0;
#endif // LOGDOC_HTMLLOAD_YIELDMS > 0

	if (OpStatus::IsMemoryError(pstat))
		return pstat;

	if (frames_doc->CheckInternal() == OpStatus::ERR_NO_MEMORY)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

NamedElementsIterator::NamedElementsIterator()
	: namedelements(NULL)
{
}

void
NamedElementsIterator::Set(NamedElements *new_namedelements, HTML_Element *new_parent, BOOL new_check_id, BOOL new_check_name)
{
	namedelements = new_namedelements;
	parent = new_parent;
	check_id = new_check_id;
	check_name = new_check_name;
	index = 0;
}

HTML_Element *
NamedElementsIterator::GetNextElement()
{
	if (namedelements && index < namedelements->count)
	{
		HTML_Element **elements;
		if (namedelements->count == 1)
			elements = &namedelements->data.single;
		else
			elements = namedelements->data.elements;

		if (!parent && check_id && check_name)
			return elements[index++];
		else
		{
			const uni_char *name = namedelements->name;

			for (; index < namedelements->count; ++index)
				if (!parent || parent->IsAncestorOf(elements[index]) && parent != elements[index])
				{
					const uni_char *s;

					if ((check_id && check_name) ||
						(check_id && (s = elements[index]->GetId()) && uni_str_eq(s, name)) ||
						(check_name && (s = elements[index]->GetName()) && uni_str_eq(s, name)))
						return elements[index++];
				}
		}
	}

	return NULL;
}

void
NamedElementsIterator::Reset()
{
	index = 0;
}

OP_STATUS
LogicalDocument::ProcessHTTPLinkRelations(HTTP_Link_Relations* link_relations)
{
	// Never call this method if root is not created. We need something to put link elements under.
	OP_ASSERT(root);

	while (link_relations)
	{
		ParameterList& params = link_relations->GetParameters();

		HtmlAttrEntry attr_array[8];
		HtmlAttrEntry* attr_p = attr_array;
		Parameters* param;

		OP_STATUS status = OpStatus::OK;

		attr_p->attr = ATTR_HREF;
		status = SetStr(((uni_char*&)attr_p->value), link_relations->LinkURI().CStr());
		if (status == OpStatus::OK)
			attr_p->value_len = attr_p->value ? uni_strlen(attr_p->value) : 0;
		attr_p++;

		if (status == OpStatus::OK && (param = params.GetParameter("rel")) != 0)
		{
			attr_p->attr = ATTR_REL;
			status = SetStr(((uni_char*&)attr_p->value), param->GetValue().CStr());
			if (status == OpStatus::OK)
				attr_p->value_len = attr_p->value ? uni_strlen(attr_p->value) : 0;
			attr_p++;
		}
		if (status == OpStatus::OK && (param = params.GetParameter("rev")) != 0)
		{
			attr_p->attr = ATTR_REV;
			status = SetStr(((uni_char*&)attr_p->value), param->GetValue().CStr());
			if (status == OpStatus::OK)
				attr_p->value_len = attr_p->value ? uni_strlen(attr_p->value) : 0;
			attr_p++;
		}
		if (status == OpStatus::OK && (param = params.GetParameter("charset"))!= 0)
		{
			attr_p->attr = ATTR_CHARSET;
			status = SetStr(((uni_char*&)attr_p->value), param->GetValue().CStr());
			if (status == OpStatus::OK)
				attr_p->value_len = attr_p->value ? uni_strlen(attr_p->value) : 0;
			attr_p++;
		}
		if (status == OpStatus::OK && (param = params.GetParameter("type")) != 0)
		{
			attr_p->attr = ATTR_TYPE;
			status = SetStr(((uni_char*&)attr_p->value), param->GetValue().CStr());
			if (status == OpStatus::OK)
				attr_p->value_len = attr_p->value ? uni_strlen(attr_p->value) : 0;
			attr_p++;
		}
		if (status == OpStatus::OK && (param = params.GetParameter("media")) != 0)
		{
			attr_p->attr = ATTR_MEDIA;
			status = SetStr(((uni_char*&)attr_p->value), param->GetValue().CStr());
			if (status == OpStatus::OK)
				attr_p->value_len = attr_p->value ? uni_strlen(attr_p->value) : 0;
			attr_p++;
		}
		if (status == OpStatus::OK && (param = params.GetParameter("title")) != 0)
		{
			attr_p->attr = ATTR_TITLE;
			status = SetStr(((uni_char*&)attr_p->value), param->GetValue().CStr());
			if (status == OpStatus::OK)
				attr_p->value_len = attr_p->value ? uni_strlen(attr_p->value) : 0;
			attr_p++;
		}

		attr_p->attr = ATTR_NULL; // terminate the attribute array

		if (status == OpStatus::OK)
		{
			HTML_Element* new_elm = NEW_HTML_Element();

            if (new_elm && OpStatus::ERR_NO_MEMORY != new_elm->Construct(&hld_profile, NS_IDX_HTML, HE_LINK, attr_array, HE_INSERTED_BY_CSS_IMPORT))
			{
				new_elm->Under(root);
				status = hld_profile.HandleLink(new_elm);
			}
			else
            {
                DELETE_HTML_Element(new_elm);
				status = OpStatus::ERR_NO_MEMORY;
            }
		}

		attr_p = attr_array;
		while (attr_p->attr != ATTR_NULL)
		{
			OpStatus::Ignore(SetStr(((uni_char*&)attr_p->value), (uni_char*)NULL));
			attr_p++;
		}

		if (status != OpStatus::OK)
			return status;

		link_relations = link_relations->Suc();
	}

	return OpStatus::OK;
}

OP_STATUS LogicalDocument::ConstructTextDocumentStructure()
{
	HtmlAttrEntry html_attrs[1];
	html_attrs[0].attr = ATTR_NULL;

	HTML_Element *html_elm = NULL;
	HTML_Element *body_elm = NULL;
	HTML_Element *pre_elm = NULL;

	FramesDocument *frames_doc = GetFramesDocument();

	if (!(html_elm = NEW_HTML_Element())
	    || OpStatus::IsMemoryError(html_elm->Construct(&hld_profile, NS_IDX_HTML, HE_HTML, NULL, HE_NOT_INSERTED)))
		goto handle_oom;

	html_elm->UnderSafe(frames_doc, root, FALSE);
	OP_ASSERT(doc_root.GetElm() == html_elm);

	if (!(body_elm = NEW_HTML_Element())
	    || OpStatus::IsMemoryError(body_elm->Construct(&hld_profile, NS_IDX_HTML, HE_BODY, NULL, HE_NOT_INSERTED)))
		goto handle_oom;

	body_elm->UnderSafe(frames_doc, html_elm, FALSE);
	OP_ASSERT(hld_profile.GetBodyElm() == body_elm);

#ifndef NO_PRE_FOR_PLAIN_TEXT
	if (!(pre_elm = NEW_HTML_Element())
	    || OpStatus::IsMemoryError(pre_elm->Construct(&hld_profile, NS_IDX_HTML, HE_PRE, NULL, HE_NOT_INSERTED)))
		goto handle_oom;

	pre_elm->UnderSafe(frames_doc, body_elm, FALSE);
#endif // NO_PRE_FOR_PLAIN_TEXT

	return OpStatus::OK;

handle_oom:
	if (pre_elm)
	{
		pre_elm->Out();
		DELETE_HTML_Element(pre_elm);
	}
	if (body_elm)
	{
		body_elm->Out();
		hld_profile.ResetBodyElm();
		DELETE_HTML_Element(body_elm);
	}
	if (html_elm)
	{
		html_elm->Out();
		if (html_elm->Clean(frames_doc))
			html_elm->Free(frames_doc);
		doc_root.Reset();
	}
	return OpStatus::ERR_NO_MEMORY;
}

#ifdef OPERA_CONSOLE

/*static*/ OP_STATUS
LogicalDocument::PostConsoleMsg(URL *error_url, Str::LocaleString str_num,
							OpConsoleEngine::Source src,
							OpConsoleEngine::Severity sev,
							LogicalDocument* logdoc)
{
	TRAPD(status, PostConsoleMsgL(error_url, str_num, src, sev, logdoc));
	return status;
}

/*static*/ void
LogicalDocument::PostConsoleMsgL(URL *error_url, Str::LocaleString str_num,
							OpConsoleEngine::Source src,
							OpConsoleEngine::Severity sev,
							LogicalDocument* logdoc)
{
	if (!g_console->IsLogging())
		return;

	OpConsoleEngine::Message msg(src, sev);
	ANCHOR(OpConsoleEngine::Message, msg);

	msg.window = logdoc ? logdoc->GetFramesDocument()->GetWindow()->Id() : 0;

	error_url->GetAttributeL(URL::KUniName_With_Fragment, msg.url);
	g_languageManager->GetStringL(str_num, msg.message);

	g_console->PostMessageL(&msg);
}
#endif // OPERA_CONSOLE

#ifdef VISITED_PAGES_SEARCH

/** Does this block of text continue without line break from the previous? */
static BOOL IsContinuation(HTML_Element *elm, HLDocProfile *hld_profile)
{
	/* Since the input elm is a text element, and we will at most traverse
	 * back to the previous text element, the sum of traversal done here for
	 * the whole document is limited to one full traversal of the tree. */
	for (elm = elm->Prev(); elm; elm = elm->Prev())
	{
		HTML_ElementType type = elm->Type();

		if (!IsHtmlInlineElm(type, hld_profile) || type == HE_BR) // A block element
			return FALSE;

		if (!elm->FirstChild()) // A non-block leaf element
		{
			if (type == HE_TEXT)
			{
				const uni_char *text = elm->Content();
				if (text && *text && WhiteSpaceOnly(text))
					return FALSE; // Whitespace text has not been sent to the indexer
			}

			return TRUE;
		}
	}
	return FALSE;
}

static inline float HE2Rank(HTML_Element *elm)
{
	if (elm)
	{
		if (elm->GetNsType() == NS_HTML)
		{
			switch (elm->Type())
			{
			case HE_TITLE:
				return RANK_TITLE;
			case HE_H1:
				return RANK_H1;
			case HE_H2:
				return RANK_H2;
			case HE_H3:
				return RANK_H3;
			case HE_H4:
				return RANK_H4;
			case HE_H5:
				return RANK_H5;
			case HE_H6:
			case HE_CAPTION:
				return RANK_H6;
			case HE_I:
			case HE_B:
			case HE_STRONG:
				return RANK_I;
			case HE_EM:
				return RANK_EM;
			default:
				return RANK_P;
			}
		}
	}

	return RANK_P;
}

OP_STATUS
LogicalDocument::IndexText(HTML_Element *elm, HTML_Element *stop_elm)
{
	OP_ASSERT(elm->IsText());
	if (FramesDocument *frm_doc = GetFramesDocument())
	{
		FramesDocument *top_doc = (FramesDocument*) frm_doc->GetTopDocument();
		VisitedSearch::RecordHandle handle = top_doc->GetVisitedSearchHandle();
		if (handle)
		{
			HTML_Element *parent = elm->Parent();
			HTML_Element *stop = stop_elm;
			HTML_Element *iter = elm;
			BOOL is_continuation = IsContinuation(elm, &hld_profile);

			do
			{
				const uni_char *text = iter->Content();
				if (text && *text && (text[0] != '\n' || text[1] != 0))
				{
					if (parent && parent->IsMatchingType(HE_TITLE, NS_HTML))
					{
						RETURN_IF_ERROR(g_visited_search->AddTitle(handle, text));
						if (frm_doc->AllowFullTextIndexing())
							RETURN_IF_ERROR(g_visited_search->AddTextBlock(handle, text, HE2Rank(parent), is_continuation));
					}
					else if (frm_doc->AllowFullTextIndexing())
						RETURN_IF_ERROR(g_visited_search->AddTextBlock(handle, text, HE2Rank(parent), is_continuation));

					is_continuation = TRUE;
				}

				iter = iter->Next();
			}
			while (iter && iter != stop);
		}
	}

	return OpStatus::OK;
}
#endif // VISITED_PAGES_SEARCH

#ifdef PHONE2LINK_SUPPORT
enum P2LProp
{
	P2L_EMAIL_OK = 1,
	P2L_PHONE_OK = 2,
	P2L_PHONE_WS = 4,
	P2L_EMAIL_STOP = 8
};

UINT
GetCharacterProperty(UnicodePoint c)
{
	if (c >= '0' && c <= '9')
		return P2L_EMAIL_OK | P2L_PHONE_OK;

	switch (c)
	{
	case '+':
	case 0xFF10: /* FULLWIDTH DIGIT ZERO */
	case 0xFF11: /* FULLWIDTH DIGIT ONE */
	case 0xFF12: /* FULLWIDTH DIGIT TWO */
	case 0xFF13: /* FULLWIDTH DIGIT THREE */
	case 0xFF14: /* FULLWIDTH DIGIT FOUR */
	case 0xFF15: /* FULLWIDTH DIGIT FIVE */
	case 0xFF16: /* FULLWIDTH DIGIT SIX */
	case 0xFF17: /* FULLWIDTH DIGIT SEVEN */
	case 0xFF18: /* FULLWIDTH DIGIT EIGHT */
	case 0xFF19: /* FULLWIDTH DIGIT NINE */
		return P2L_EMAIL_OK | P2L_PHONE_OK;

	case '<':
	case '>':
	case '[':
	case ']':
	case ':':
	case ';':
	case '@':
	case '\\':
	case ',':
	case '"':
		return P2L_EMAIL_STOP;

	case '.':
	case '-':
		return P2L_EMAIL_OK | P2L_PHONE_WS;

	case '\t':
	case '\n':
	case '\r':
	case ' ':
	case '(':
	case ')':
	case 0x00a0: /* non-breaking space */
	case 0x30CE: /* KATAKANA LETTER NO */
	case 0x306E: /* HIRAGANA LETTER NO */
	case 0xFF0D: /* FULLWIDTH HYPHEN-MINUS */
	case 0xFF0E: /* FULLWIDTH FULL STOP */
	case 0xFF08: /* FULLWIDTH LEFT PARENTHESIS */
	case 0xFF09: /* FULLWIDTH RIGHT PARENTHESIS */
	case 0x200B: /* ZERO WIDTH SPACE */
	case 0x202f: /* NARROW NONBREAKING SPACE */
	case 0x2060: /* WORD JOINER */
	case 0x3000: /* IDEOGRAPHIC SPACE */
	case 0xFEFF: /* ZERO WIDTH NO-BREAK SPACE */
		return P2L_PHONE_WS;
	}

	return P2L_EMAIL_OK;
}

enum P2LLinkType
{
	P2L_NOT_LINK = 1,
	P2L_PHONE_LINK = 2,
	P2L_EMAIL_LINK = 4
};

OP_STATUS
AddPhoneNode(HLDocProfile *hld_profile, HTML_Element *group_elm,
			 const uni_char *txt, size_t txt_len, UINT link_type, BOOL in_svg)
{
	if (txt_len == 0)
		return OpStatus::OK;

	HTML_Element *parent = group_elm;
	HTML_Element *new_elm = NEW_HTML_Element();
	if (!new_elm)
		return OpStatus::ERR_NO_MEMORY;

	if (link_type != P2L_NOT_LINK)
	{
		HtmlAttrEntry attributes[2];
#if defined SVG_SUPPORT && defined XLINK_SUPPORT
		attributes[0].attr = in_svg ? static_cast<int>(Markup::XLINKA_HREF) : static_cast<int>(ATTR_HREF);
		attributes[0].ns_idx = in_svg ? NS_IDX_XLINK : NS_IDX_DEFAULT;
#else // SVG_SUPPORT && XLINK_SUPPORT
		attributes[0].attr = ATTR_HREF;
		attributes[0].ns_idx = NS_IDX_DEFAULT;
#endif // SVG_SUPPORT && XLINK_SUPPORT
		attributes[0].is_specified = FALSE;
		attributes[0].is_special = FALSE;

		uni_char *buf;
		size_t buf_len;
		if (link_type == P2L_PHONE_LINK)
		{
			buf_len = txt_len + 4;
			buf = OP_NEWA(uni_char, buf_len);
			if (!buf)
			{
				DELETE_HTML_Element(new_elm);
				return OpStatus::ERR_NO_MEMORY;
			}

			uni_strcpy(buf, UNI_L("tel:"));
			for (size_t i = 0; i < txt_len; i++)
			{
				if(txt[i] >= 0xFF10)
			  		buf[i + 4] = txt[i] - 0xFF10 - '0';
				else
					buf[i + 4] = txt[i];
			}
		}
		else // email
		{
			buf_len = txt_len + 7;
			buf = OP_NEWA(uni_char, buf_len);
			if (!buf)
			{
				DELETE_HTML_Element(new_elm);
				return OpStatus::ERR_NO_MEMORY;
			}

			uni_strcpy(buf, UNI_L("mailto:"));
			uni_strncpy(buf + 7, txt, txt_len);
		}

		attributes[0].value = buf;
		attributes[0].value_len = buf_len;

		attributes[1].attr = ATTR_NULL;

		if (OpStatus::IsMemoryError(new_elm->Construct(hld_profile,
#if defined SVG_SUPPORT && defined XLINK_SUPPORT
				in_svg ? NS_IDX_SVG : NS_IDX_HTML,
				in_svg ? static_cast<HTML_ElementType>(Markup::SVGE_A) : HE_A,
#else // SVG_SUPPORT && XLINK_SUPPORT
				NS_IDX_HTML, HE_A,
#endif // SVG_SUPPORT && XLINK_SUPPORT
				attributes)))
		{
			DELETE_HTML_Element(new_elm);
			OP_DELETEA(buf);
			return OpStatus::ERR_NO_MEMORY;
		}

		OP_DELETEA(buf);

		new_elm->Under(parent);
		parent = new_elm;

		new_elm = NEW_HTML_Element();
		if (!new_elm)
			return OpStatus::ERR_NO_MEMORY;
	}

	if (OpStatus::IsMemoryError(new_elm->Construct(hld_profile, txt, txt_len)))
	{
		DELETE_HTML_Element(new_elm);
		return OpStatus::ERR_NO_MEMORY;
	}

	if (group_elm->Type() != HE_TEXTGROUP)
		group_elm->SetType(HE_TEXTGROUP);

	new_elm->Under(parent);

	return OpStatus::OK;
}

OP_STATUS
ScanForPhoneNumbers(HTML_Element *txt_elm, HTML_Element *stop_elm, HLDocProfile *hld_profile, BOOL in_svg)
{
	HTML_Element *iter_elm = txt_elm;
	OP_STATUS status = OpStatus::OK;

	while (iter_elm && iter_elm != stop_elm)
	{
		if (iter_elm->Type() == HE_TEXT
			&& iter_elm->GetAnchorTags(hld_profile->GetFramesDocument()) == NULL)
		{
			int start_pos = 0;
			int last_num_pos = 0;
			int num_digits = 0;

			const uni_char *txt = iter_elm->Content();

			if (!txt || WhiteSpaceOnly(txt, iter_elm->ContentLength()))
			{
				iter_elm = iter_elm->Next();
				continue;
			}

			UnicodeStringIterator iter(txt, 0, iter_elm->ContentLength());
			UnicodePoint c;
			BOOL prev_is_whitespace = TRUE;

			while (!iter.IsAtEnd())
			{
				c = iter.At();

				UINT prop = GetCharacterProperty(c);

				if (num_digits == 0 && (prop & P2L_PHONE_OK) && prev_is_whitespace)
				{
					last_num_pos = iter.Index();
					num_digits++;
				}
				else if (num_digits > 0 && (prop & P2L_PHONE_OK))
				{
					num_digits++;
				}
				else if (num_digits > 0 && (prop & P2L_PHONE_WS) != P2L_PHONE_WS)
				{
					if (num_digits >= MIN_PHONE_DIGITS_COUNT && num_digits <= MAX_PHONE_DIGITS_COUNT)
					{
						status = AddPhoneNode(hld_profile, iter_elm, txt + start_pos, last_num_pos - start_pos, P2L_NOT_LINK, in_svg);
						if (OpStatus::IsMemoryError(status))
							goto error_exit;
						status = AddPhoneNode(hld_profile, iter_elm, txt + last_num_pos, iter.Index() - last_num_pos, P2L_PHONE_LINK, in_svg);
						if (OpStatus::IsMemoryError(status))
							goto error_exit;

						start_pos = iter.Index();
					}

					last_num_pos = 0;
					num_digits = 0;
				}

				prev_is_whitespace = uni_isspace(c);
				iter.Next();
			}

			if (num_digits >= MIN_PHONE_DIGITS_COUNT && num_digits <= MAX_PHONE_DIGITS_COUNT)
			{
				status = AddPhoneNode(hld_profile, iter_elm, txt + start_pos, last_num_pos - start_pos, P2L_NOT_LINK, in_svg);
				if (OpStatus::IsMemoryError(status))
					goto error_exit;
				status = AddPhoneNode(hld_profile, iter_elm, txt + last_num_pos, iter.Index() - last_num_pos, P2L_PHONE_LINK, in_svg);
				if (OpStatus::IsMemoryError(status))
					goto error_exit;

			}
			else
			{
				// if the iter_elm is hasn't changed type we haven't added new
				// nodes meaning that we haven't split the text so we can just
				// keep the old text element
				if (iter_elm->Type() == HE_TEXTGROUP)
				{
					status = AddPhoneNode(hld_profile, iter_elm, txt + start_pos, iter.Index() - start_pos, P2L_NOT_LINK, in_svg);
					if (OpStatus::IsMemoryError(status))
						goto error_exit;

				}
			}

			if (iter_elm->Type() == HE_TEXTGROUP)
			{
				// if the element changed from TEXT to TEXTGROUP we need to delete
				// the textdata of the old element
				iter_elm->DeleteTextData();
			}

			iter_elm = static_cast<HTML_Element*>(iter_elm->NextSibling());
		}
		else
			iter_elm = iter_elm->Next();
	}

	return OpStatus::OK;

 error_exit:
	OP_ASSERT(iter_elm->Type() == HE_TEXTGROUP);
	if (iter_elm->Type() == HE_TEXTGROUP)
		iter_elm->DeleteTextData();
	return status;
}

OP_STATUS
ScanForEmailAddrs(HTML_Element *txt_elm, HTML_Element *stop_elm, HLDocProfile *hld_profile, BOOL in_svg)
{
	HTML_Element *iter_elm = txt_elm;

	while (iter_elm && iter_elm != stop_elm)
	{
		if (iter_elm->Type() == HE_TEXT
			&& iter_elm->GetAnchorTags(hld_profile->GetFramesDocument()) == NULL)
		{
			const uni_char *txt = iter_elm->Content();
			unsigned int txt_len = iter_elm->ContentLength();
			if (!txt || WhiteSpaceOnly(txt, txt_len))
			{
				iter_elm = iter_elm->Next();
				continue;
			}
			UnicodeStringIterator iter(txt, 0, (int) txt_len);
			UnicodePoint c;
			int start_pos = 0;
			BOOL prev_was_valid = FALSE;

			while (!iter.IsAtEnd())
			{
				c = iter.At();

				if (c == '@' && iter.Index() > 0 && prev_was_valid)
				{
					size_t at_pos = iter.Index();
					iter.Next();

					while (!iter.IsAtEnd())
					{
						c = iter.At();

						UINT prop = GetCharacterProperty(c);
						if ((prop & P2L_EMAIL_OK) != P2L_EMAIL_OK)
						{
							break;
						}

						iter.Next();
					}

					if (iter.Index() > 1 + at_pos + 1)
					{
						UnicodeStringIterator start_iter(txt, at_pos, (int) txt_len);

						while (!start_iter.IsAtBeginning())
						{
							start_iter.Previous();
							c = start_iter.At();

							UINT prop = GetCharacterProperty(c);
							if ((prop & P2L_EMAIL_OK) != P2L_EMAIL_OK)
							{
								start_iter.Next();
								break;
							}
						}

						if (start_iter.Index() < at_pos)
						{
							RETURN_IF_MEMORY_ERROR(AddPhoneNode(hld_profile, iter_elm, txt + start_pos, start_iter.Index() - start_pos, P2L_NOT_LINK, in_svg));
							RETURN_IF_MEMORY_ERROR(AddPhoneNode(hld_profile, iter_elm, txt + start_iter.Index(), iter.Index() - start_iter.Index(), P2L_EMAIL_LINK, in_svg));
						}
					}

					if (!(iter.IsAtEnd() && c == '@'))
						start_pos = iter.Index();
				}

				prev_was_valid = ((GetCharacterProperty(c) & P2L_EMAIL_OK) == P2L_EMAIL_OK);

				// We do not need to check iter.IsAtEnd() here, since
				// iter.Next() is a no-op in that case.
				iter.Next();
			}

			// if the iter_elm is hasn't changed type we haven't added new
			// nodes meaning that we haven't split the text so we can just
			// keep the old text element
			if (iter_elm->Type() == HE_TEXTGROUP)
			{
				RETURN_IF_MEMORY_ERROR(AddPhoneNode(hld_profile, iter_elm, txt + start_pos, iter.Index() - start_pos, P2L_NOT_LINK, in_svg));

				// if the element changed from TEXT to TEXTGROUP we need to delete
				// the textdata of the old element
				iter_elm->DeleteTextData();
			}

			iter_elm = static_cast<HTML_Element*>(iter_elm->NextSibling());
		}
		else
			iter_elm = iter_elm->Next();
	}

	return OpStatus::OK;
}

OP_STATUS
LogicalDocument::ScanForTextLinks(HTML_Element *txt_elm, HTML_Element* stop_elm, BOOL in_svg)
{
	if (GetURL()->Type() != URL_OPERA)
	{
		RETURN_IF_ERROR(ScanForEmailAddrs(txt_elm, stop_elm, &hld_profile, in_svg));

		RETURN_IF_ERROR(ScanForPhoneNumbers(txt_elm, stop_elm, &hld_profile, in_svg));
	}

	return OpStatus::OK;
}
#endif // PHONE2LINK_SUPPORT

#ifndef HAS_NO_SEARCHTEXT
# ifdef SEARCH_MATCHES_ALL
OP_STATUS
LogicalDocument::FindAllMatches(SearchData* data, HTML_Element *offset_from/*=NULL*/, int offset/*=0*/)
{
#if 0
	// debug code
	uni_char *stxt = OP_NEWA(uni_char, 3);
	stxt[0] = 's';
	stxt[1] = 's';
	stxt[2] = 0;
	data->SetSearchText(stxt);

//	data->SetSearchText(UNI_L(""));
//	data->SetMatchCase(TRUE);
//	data->SetMatchWords(TRUE);
//	data->SetLinksOnly(TRUE);
#endif // 0

	SearchHelper search_helper(data, this);

	RETURN_IF_MEMORY_ERROR(search_helper.Init());

	return search_helper.FindAllMatches(offset_from, offset);
}

# endif // SEARCH_MATCHES_ALL
#endif // HAS_NO_SEARCHTEXT

#ifdef FORMAT_UNSTYLED_XML

static OP_STATUS
CreateAndInsertText(HLDocProfile *hld_profile, HTML_Element *parent, const uni_char *text)
{
	HTML_Element::DocumentContext dc(hld_profile->GetLogicalDocument());
	HTML_Element *text_elm = HTML_Element::CreateText(text, uni_strlen(text), FALSE, FALSE, FALSE);
	if (!text)
	{
		DELETE_HTML_Element(text_elm);
		return OpStatus::ERR_NO_MEMORY;
	}
	text_elm->SetInserted(HE_INSERTED_BY_PARSE_AHEAD);
	return text_elm->UnderSafe(dc, parent);
}

static OP_STATUS
CreateAndInsert(HLDocProfile *hld_profile, HTML_Element *&element, HTML_ElementType type, const uni_char *classname = NULL, const uni_char *text = NULL)
{
	HTML_Element *elm = NEW_HTML_Element();
	HtmlAttrEntry attrs[2];
	if (classname)
	{
		attrs[0].attr = ATTR_CLASS;
		attrs[0].ns_idx = NS_IDX_HTML;
		attrs[0].value = classname;
		attrs[0].value_len = uni_strlen(classname);
		attrs[1].attr = ATTR_NULL;
	}
	else
		attrs[0].attr = ATTR_NULL;
	if (!elm || OpStatus::IsMemoryError(elm->Construct(hld_profile, NS_IDX_HTML, type, attrs, HE_INSERTED_BY_PARSE_AHEAD)))
	{
		DELETE_HTML_Element(elm);
		return OpStatus::ERR_NO_MEMORY;
	}
	HTML_Element::DocumentContext dc(hld_profile->GetLogicalDocument());
	if (element)
		RETURN_IF_ERROR(elm->UnderSafe(dc, element));
	if (text)
		RETURN_IF_ERROR(CreateAndInsertText(hld_profile, elm, text));
	else
		element = elm;
	return OpStatus::OK;
}

static BOOL
ElementHasAttributes(HTML_Element *element)
{
	HTML_AttrIterator iterator(element);

	const uni_char *name, *value;
	int ns_idx;
	BOOL specified, id;

	while (iterator.GetNext(name, value, ns_idx, specified, id))
		if (specified)
			return TRUE;

	return FALSE;
}

static const uni_char *
FormatQName(const uni_char *localpart, int ns_idx, TempBuffer &buffer)
{
	const uni_char *prefix;
	if (ns_idx != NS_IDX_DEFAULT && (prefix = g_ns_manager->GetPrefixAt(ns_idx)) != NULL && *prefix)
	{
		buffer.Clear();
		buffer.Append(prefix);
		buffer.Append(":");
		buffer.Append(localpart);
		return buffer.GetStorage();
	}
	else
		return localpart;
}

static BOOL
ReevaluatePreserveSpace(HTML_Element *parent)
{
	while (parent)
	{
		const uni_char *xml_space = parent->GetStringAttr(XMLA_SPACE, NS_IDX_XML);
		if (xml_space)
			if (uni_str_eq(xml_space, "preserve"))
				return TRUE;
			else if (uni_str_eq(xml_space, "default"))
				return FALSE;
		parent = parent->ParentActual();
	}
	return FALSE;
}

static OP_STATUS
DoFormatUnstyledXml(HLDocProfile *hld_profile, HTML_Element *root, HTML_Element *&current, const uni_char *style, const uni_char *header, const uni_char *message)
{
#define OPEN_NO_CLASS(type) RETURN_IF_ERROR(CreateAndInsert(hld_profile, current, type))
#define OPEN(type, classname) RETURN_IF_ERROR(CreateAndInsert(hld_profile, current, type, UNI_L(classname)))
#define WITH_TEXT(type, classname, text) RETURN_IF_ERROR(CreateAndInsert(hld_profile, current, type, UNI_L(classname), text))
#define ADD_TEXT(text) RETURN_IF_ERROR(CreateAndInsertText(hld_profile, current, UNI_L(text)))
#define CLOSE(type) do { OP_ASSERT(current->Type() == type); current = current->Parent(); } while (0)

	OPEN_NO_CLASS(HE_HTML);
	OPEN_NO_CLASS(HE_HEAD);
	OPEN_NO_CLASS(HE_STYLE);
	RETURN_IF_ERROR(CreateAndInsertText(hld_profile, current, style));
	CLOSE(HE_STYLE);
	CLOSE(HE_HEAD);
	OPEN_NO_CLASS(HE_BODY);
	OPEN(HE_DIV, "header");
	RETURN_IF_ERROR(CreateAndInsertText(hld_profile, current, header));
	if (message)
		WITH_TEXT(HE_DIV, "message", message);
	CLOSE(HE_DIV);

	TempBuffer qname_buffer;
	BOOL preserve_space = FALSE;

	OPEN(HE_DIV, "syntax");
	HTML_Element *iter = root->FirstChildActual();
	while (iter)
	{
		if (Markup::IsRealElement(iter->Type()))
		{
			BOOL has_attributes = ElementHasAttributes(iter);
			BOOL has_children = iter->FirstChildActual() != NULL;

			const uni_char *xml_space = iter->GetStringAttr(XMLA_SPACE, NS_IDX_XML);
			if (xml_space)
				if (uni_str_eq(xml_space, "preserve"))
					preserve_space = TRUE;
				else if (uni_str_eq(xml_space, "default"))
					preserve_space = FALSE;

			RETURN_IF_ERROR(CreateAndInsert(hld_profile, current, HE_DIV, preserve_space ? UNI_L("element preserve") : UNI_L("element normal")));

			OPEN(HE_DIV, "starttag");
			if (has_attributes)
				OPEN(HE_SPAN, "leading");
			ADD_TEXT("<");
			WITH_TEXT(HE_SPAN, "name", FormatQName(iter->GetTagName(), iter->GetNsIdx(), qname_buffer));
			if (has_attributes)
			{
				ADD_TEXT(" ");
				CLOSE(HE_SPAN);
				OPEN(HE_SPAN, "attributes");

				HTML_AttrIterator iterator(iter);

				const uni_char *name, *value;
				int ns_idx;
				BOOL specified, id;

				while (iterator.GetNext(name, value, ns_idx, specified, id))
					if (specified)
					{
						OPEN(HE_SPAN, "attribute");
						ADD_TEXT(" ");
						WITH_TEXT(HE_SPAN, "name", FormatQName(name, ns_idx, qname_buffer));
						ADD_TEXT("=\"");
						WITH_TEXT(HE_SPAN, "value", value);
						ADD_TEXT("\"");
						CLOSE(HE_SPAN);
					}

				CLOSE(HE_SPAN);
			}
			if (has_children)
				ADD_TEXT(">");
			else
				ADD_TEXT("/>");
			CLOSE(HE_DIV);
			if (has_children)
			{
				OPEN(HE_DIV, "content");
				iter = iter->FirstChildActual();
				continue;
			}
			else
				CLOSE(HE_DIV);
		}
		else if (iter->Type() == HE_COMMENT)
		{
			OPEN(HE_DIV, "comment");
			ADD_TEXT("<!--");
			WITH_TEXT(HE_SPAN, "data", iter->GetStringAttr(ATTR_CONTENT));
			ADD_TEXT("-->");
			CLOSE(HE_DIV);
		}
		else if (iter->Type() == HE_TEXT)
		{
			const uni_char *data = iter->Content();
			if (preserve_space || !XMLUtils::IsWhitespace(data))
			{
				OPEN(HE_DIV, "text");
				if (iter->GetIsCDATA())
					ADD_TEXT("<![CDATA[");
				RETURN_IF_ERROR(CreateAndInsert(hld_profile, current, HE_SPAN, preserve_space ? UNI_L("data pre") : UNI_L("data normal"), data));
				if (iter->GetIsCDATA())
					ADD_TEXT("]]>");
				CLOSE(HE_DIV);
			}
		}
		else if (iter->Type() == HE_TEXTGROUP)
		{
			TempBuffer buffer;
			buffer.SetExpansionPolicy(TempBuffer::AGGRESSIVE);
			buffer.SetCachedLengthPolicy(TempBuffer::TRUSTED);
			for (HTML_Element *child = iter->Next(), *stop = static_cast<HTML_Element *>(iter->NextSibling()); child != stop; child = child->Next())
				if (child->Type() == HE_TEXT)
					buffer.Append(child->Content());
			const uni_char *data = buffer.GetStorage();
			if (data && (preserve_space || !XMLUtils::IsWhitespace(data)))
			{
				OPEN(HE_DIV, "text");
				if (iter->GetIsCDATA())
					ADD_TEXT("<![CDATA[");
				RETURN_IF_ERROR(CreateAndInsert(hld_profile, current, HE_SPAN, preserve_space ? UNI_L("data pre") : UNI_L("data normal"), data));
				if (iter->GetIsCDATA())
					ADD_TEXT("]]>");
				CLOSE(HE_DIV);
			}
		}
		else if (iter->Type() == HE_PROCINST)
		{
			OPEN(HE_DIV, "processing-instruction");
			ADD_TEXT("<?");
			WITH_TEXT(HE_SPAN, "target", iter->GetStringAttr(ATTR_TARGET, NS_IDX_DEFAULT));
			const uni_char *data = iter->GetStringAttr(ATTR_CONTENT, NS_IDX_DEFAULT);
			if (data && *data)
			{
				ADD_TEXT(" ");
				WITH_TEXT(HE_SPAN, "data", data);
			}
			ADD_TEXT("?>");
			CLOSE(HE_DIV);
		}
		else if (iter->Type() == HE_DOCTYPE)
		{
			const XMLDocumentInformation *document_info = iter->GetXMLDocumentInfo();
			if (document_info)
			{
				OPEN(HE_DIV, "document-type-declaration");
				ADD_TEXT("<!DOCTYPE ");
				WITH_TEXT(HE_SPAN, "name", document_info->GetDoctypeName());
				const uni_char *public_id = document_info->GetPublicId(), *system_id = document_info->GetSystemId();
				if (public_id || system_id)
				{
					if (public_id)
					{
						ADD_TEXT(" PUBLIC \"");
						WITH_TEXT(HE_SPAN, "public-id", public_id);
						if (system_id)
							ADD_TEXT("\" \"");
						else
							ADD_TEXT("\">");
					}
					else
						ADD_TEXT(" SYSTEM \"");
					if (system_id)
					{
						WITH_TEXT(HE_SPAN, "system-id", system_id);
						ADD_TEXT("\">");
					}
				}
				else
					ADD_TEXT(">");
				CLOSE(HE_DIV);
			}
		}

		while (iter)
			if (HTML_Element *suc = iter->SucActual())
			{
				iter = suc;
				break;
			}
			else if (HTML_Element *parent = iter->ParentActual())
			{
				if (Markup::IsRealElement(parent->Type()))
				{
					CLOSE(HE_DIV);
					OPEN(HE_DIV, "endtag");
					ADD_TEXT("</");
					WITH_TEXT(HE_SPAN, "name", FormatQName(parent->GetTagName(), parent->GetNsIdx(), qname_buffer));
					ADD_TEXT(">");
					CLOSE(HE_DIV);
					CLOSE(HE_DIV);

					if (parent->GetStringAttr(XMLA_SPACE, NS_IDX_XML))
						preserve_space = ReevaluatePreserveSpace(parent->ParentActual());
				}
				iter = parent;
			}
			else
				iter = NULL;
	}
	CLOSE(HE_DIV);

	OPEN(HE_DIV, "real");
	return OpStatus::OK;

#undef OPEN_NO_CLASS
#undef OPEN
#undef WITH_TEXT
#undef ADD_TEXT
#undef CLOSE
}

OP_STATUS
LogicalDocument::FormatUnstyledXml(const uni_char *message, BOOL xslt_failed)
{
	if (!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::FormatUnstyledXML))
		return OpStatus::OK;

	HLDocProfile *hld_profile = GetHLDocProfile();

	OpFile style_file;
	RETURN_IF_ERROR(style_file.Construct(UNI_L("unstyledxml.css"), OPFILE_STYLE_FOLDER));

	UnicodeFileInputStream style_stream;
	RETURN_IF_ERROR(style_stream.Construct(&style_file, URL_CSS_CONTENT, TRUE));

	TempBuffer style_buffer;
	style_buffer.SetExpansionPolicy(TempBuffer::AGGRESSIVE);
	style_buffer.SetCachedLengthPolicy(TempBuffer::TRUSTED);
	while (style_stream.has_more_data() && style_stream.no_errors())
	{
		int length;
		uni_char *data = style_stream.get_block(length);

		if (!data)
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(style_buffer.Append(data, UNICODE_DOWNSIZE(length)));
	}

	OpString header;

#ifdef XSLT_SUPPORT
	if (xslt_failed)
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_INVALID_XSLT_STYLESHEET, header));
	else
#endif // XSLT_SUPPORT
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_NO_XML_STYLESHEET, header));

	HTML_Element::DocumentContext dc(this);
	HTML_Element *current = NULL;

	OP_STATUS status = DoFormatUnstyledXml(hld_profile, root, current, style_buffer.GetStorage(), header.CStr(), message);

	root->RemoveCachedTextInfo(hld_profile->GetFramesDocument());

	if (OpStatus::IsSuccess(status))
		while (HTML_Element *elm = root->FirstChild())
		{
			elm->OutSafe(dc);
			if (OpStatus::IsMemoryError(elm->UnderSafe(dc, current)))
			{
				status = OpStatus::ERR_NO_MEMORY;
				break;
			}
		}

	while (HTML_Element *parent = current->Parent())
		current = parent;

	if (OpStatus::IsMemoryError(status))
	{
		if (current->Clean(dc))
			current->Free(dc);
		return status;
	}

	if (OpStatus::IsMemoryError(current->UnderSafe(dc, root)))
		status = OpStatus::ERR_NO_MEMORY;
	else if (OpStatus::IsMemoryError(root->HandleDocumentTreeChange(dc, root, current, NULL, TRUE)))
		status = OpStatus::ERR_NO_MEMORY;

	root->MarkExtraDirty(hld_profile->GetFramesDocument());

	is_xml_unstyled = FALSE;
	return status;
}

#endif // FORMAT_UNSTYLED_XML

#ifdef DNS_PREFETCHING
void LogicalDocument::SetDNSPrefetchControl(BOOL value)
{
	if (value)
	{
		if (GetDNSPrefetchControl() != NO)
			dns_prefetch_control = YES;
	}
	else
		dns_prefetch_control = NO;
}

BOOL3 LogicalDocument::GetDNSPrefetchControl()
{
	if (dns_prefetch_control == MAYBE && GetDocument()->GetParentDoc() && GetDocument()->GetParentDoc()->GetLogicalDocument())
		return GetDocument()->GetParentDoc()->GetLogicalDocument()->GetDNSPrefetchControl();

	return  dns_prefetch_control;
}

BOOL LogicalDocument::DNSPrefetchingEnabled()
{
	BOOL is_secure = GetURL()->Type() == URL_HTTPS;
	BOOL3 control = GetDNSPrefetchControl();

	if (control == MAYBE)
		return !is_secure;

	return control == YES;
}

void LogicalDocument::DNSPrefetch(URL url, DNSPrefetchType type)
{
	if (GetFramesDocument()->GetWindow()->IsMailOrNewsfeedWindow())
		return;

	if (!DNSPrefetchingEnabled())
		return;

	g_dns_prefetcher->Prefetch(url, type);
}
#endif // DNS_PREFETCHING

#ifdef WEB_TURBO_MODE
OP_STATUS LogicalDocument::UpdateTurboMode()
{
	if (root)
	{
		FramesDocument *doc = GetFramesDocument();
		m_current_context_id = 0;
		if (doc->GetWindow()->GetTurboMode())
			m_current_context_id = g_windowManager->GetTurboModeContextId();

		HTML_Element *iter_elm = root;
		while (iter_elm)
		{
			iter_elm->UpdateTurboMode(this, m_current_context_id);
			iter_elm = iter_elm->Next();
		}

		// reset it again so we don't accidentally use it
		m_current_context_id = 0;
	}

	return OpStatus::OK;
}
#endif // WEB_TURBO_MODE

OP_STATUS LogicalDocument::InitClassNames()
{
	OP_ASSERT(classnames == NULL);
	classnames = OP_NEW(HTMLClassNames, (hld_profile.IsInStrictMode()));
	return classnames ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

inline BOOL IsClassSeparator(uni_char c)
{
	return (c == 0x20 || c == 0x09 || c == 0x0a || c == 0x0c || c == 0x0d);
}

ClassAttribute*
LogicalDocument::CreateClassAttribute(const uni_char* str, size_t str_len)
{
	if (!str)
	{
		str = UNI_L("");
		str_len = 0;
	}

	OP_STATUS stat(OpStatus::OK);
	OpVector<ReferencedHTMLClass> classes;
	ReferencedHTMLClass* single_class(NULL);

	const uni_char* cur = str;
	const uni_char* end = str + str_len;

	while (cur < end)
	{
		while (cur < end && IsClassSeparator(*cur))
			cur++;

		if (cur < end)
		{
			const uni_char* class_start = cur;

			while (cur < end && !IsClassSeparator(*cur))
				cur++;

			ReferencedHTMLClass* hash_entry = GetClassReference(class_start, cur - class_start);
			if (hash_entry)
			{
				if (single_class)
				{
					stat = classes.Add(single_class);
					if (stat == OpStatus::ERR_NO_MEMORY)
						single_class->UnRef();
				}

				single_class = hash_entry;
			}
			else
				stat = OpStatus::ERR_NO_MEMORY;
		}
	}

	if (classes.GetCount() && single_class && stat == OpStatus::OK)
	{
		if ((stat = classes.Add(single_class)) == OpStatus::OK)
			single_class = NULL;
	}

	uni_char* string(NULL);
	if (stat == OpStatus::OK)
	{
		string = UniSetNewStrN(str, str_len);
		if (!string)
			stat = OpStatus::ERR_NO_MEMORY;
	}

	ClassAttribute* class_attr(NULL);

	if (stat == OpStatus::OK)
	{
		class_attr = OP_NEW(ClassAttribute, (string, single_class));
		if (class_attr)
		{
			if (classes.GetCount() > 0)
				stat = class_attr->Construct(classes);
		}
		else
			stat = OpStatus::ERR_NO_MEMORY;
	}

	// If something went wrong, unref hash entries.
	if (stat == OpStatus::ERR_NO_MEMORY)
	{
		if (class_attr)
			OP_DELETE(class_attr);
		else if (string)
			OP_DELETEA(string);

		if (single_class)
			single_class->UnRef();

		UINT32 i = classes.GetCount();
		while (i--)
			classes.Get(i)->UnRef();

		return NULL;
	}

	return class_attr;
}

ReferencedHTMLClass* LogicalDocument::GetClassReference(uni_char* classname)
{
	OpAutoArray<uni_char> classname_ptr(classname);

	if (!classnames && OpStatus::IsMemoryError(InitClassNames()))
		return NULL;

	OP_ASSERT(classnames);

	ReferencedHTMLClass* reference;

	if (classnames->GetData(classname, &reference) == OpStatus::OK)
	{
		reference->Ref();
		return reference;
	}

	reference = OP_NEW(ReferencedHTMLClass, (classname, classnames));

	if (reference)
	{
		classname_ptr.release();
		OP_STATUS stat = classnames->Add(classname, reference);
		if (OpStatus::IsError(stat))
		{
			OP_ASSERT(stat == OpStatus::ERR_NO_MEMORY);
			OP_DELETE(reference);
			return NULL;
		}
		else
			reference->Ref();
	}

	return reference;
}

ReferencedHTMLClass* LogicalDocument::GetClassReference(const uni_char* classname, size_t length)
{
	if (!classnames && OpStatus::IsMemoryError(InitClassNames()))
		return NULL;

	OP_ASSERT(classnames);

	ReferencedHTMLClass* reference = NULL;

	TempBuffer* temp_buf = g_opera->logdoc_module.GetTempBuffer();

	if (temp_buf->Append(classname, length) == OpStatus::OK)
		classnames->GetData(temp_buf->GetStorage(), &reference);

	g_opera->logdoc_module.ReleaseTempBuffer(temp_buf);

	if (!reference)
	{
		uni_char* new_classname = UniSetNewStrN(classname, length);

		if (!new_classname)
			return NULL;

		reference = OP_NEW(ReferencedHTMLClass, (new_classname, classnames));
		if (!reference)
		{
			OP_DELETEA(new_classname);
			return NULL;
		}

		OP_STATUS stat = classnames->Add(new_classname, reference);
		if (OpStatus::IsError(stat))
		{
			OP_ASSERT(stat == OpStatus::ERR_NO_MEMORY);
			OP_DELETE(reference);
			return NULL;
		}
	}

	OP_ASSERT(reference);

	reference->Ref();
	return reference;
}

void ReferencedHTMLClass::Delete()
{
	if (m_classnames)
	{
		ReferencedHTMLClass* data = NULL;
		OpStatus::Ignore(m_classnames->Remove(this->m_class, &data));
		OP_ASSERT(data == this);
	}
	OP_DELETE(this);
}

#include "modules/logdoc/src/dtdstrings.inc"
BOOL FindInHandheldList(const uni_char *doctype_public_id, const char *const *doctypes_list)
{
	const char * const * dt;
	for (dt = doctypes_list; *dt; dt++)
	{
		if (uni_str_eq(doctype_public_id, *dt))
			return TRUE;
	}
	return FALSE;
}

void CheckHandheldMode(HLDocProfile *hld_profile, const uni_char *doctype_name, const uni_char *doctype_public_id)
{
	if (!doctype_name || !doctype_public_id)
		return;

	BOOL handheld_changed = FALSE;

	// Loop through the lists of HTML or WML handheld types to determine
	// if the given doctype is a handheld doctype
	if (uni_stri_eq(doctype_name, "html") && FindInHandheldList(doctype_public_id, g_handheld_html_doctypes)
		|| uni_stri_eq(doctype_name, "wml") && FindInHandheldList(doctype_public_id, g_handheld_wml_doctypes))
	{
		hld_profile->SetHasMediaStyle(CSS_MEDIA_TYPE_HANDHELD);

		handheld_changed = TRUE;

		hld_profile->GetLayoutWorkplace()->SetKeepOriginalLayout();
	}

	FramesDocument *frm_doc = hld_profile->GetFramesDocument();
	if (frm_doc->GetParentDoc() == NULL)
	{
		WindowCommander *win_com = frm_doc->GetWindow()->GetWindowCommander();
		OpDocumentListener* docListener = win_com->GetDocumentListener();
		docListener->OnHandheldChanged(win_com, handheld_changed);
		hld_profile->SetHandheldCallbackCalled(TRUE);
		hld_profile->SetHasHandheldDocType(handheld_changed);
	}
}

void LogicalDocument::SetXMLDocumentInfo(XMLDocumentInformation *document_info)
{
	OP_DELETE(m_xml_document_info);
	m_xml_document_info = document_info;

	const uni_char *resolved_system_id = m_xml_document_info->GetResolvedSystemId();

	if (resolved_system_id)
		if (uni_str_eq(resolved_system_id, "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd") ||
		    uni_str_eq(resolved_system_id, "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd") ||
		    uni_str_eq(resolved_system_id, "http://www.w3.org/TR/xhtml1/DTD/xhtml1-frameset.dtd"))
			hld_profile.SetXhtml(TRUE);

	CheckHandheldMode(&hld_profile, m_xml_document_info->GetDoctypeName(), m_xml_document_info->GetPublicId());
}

OP_STATUS LogicalDocument::SetDoctype(const uni_char *name, const uni_char *pub, const uni_char *sys)
{
	strict_mode = TRUE; // Set it to true initially if we have a doctype

	OP_DELETEA(doctype_name);
	doctype_name = UniSetNewStr(name);

	OP_DELETEA(doctype_public_id);
	doctype_public_id = UniSetNewStr(pub);

	OP_DELETEA(doctype_system_id);
	doctype_system_id = UniSetNewStr(sys);

	if ((!doctype_name && name)
		|| (!doctype_public_id && pub)
		|| (!doctype_system_id && sys))
	{
		OP_DELETEA(doctype_name);
		OP_DELETEA(doctype_public_id);
		OP_DELETEA(doctype_system_id);

		return OpStatus::ERR_NO_MEMORY;
	}

	if (hld_profile.GetFramesDocument())
		CheckHandheldMode(&hld_profile, doctype_name, doctype_public_id);

	return OpStatus::OK;
}

void LogicalDocument::CheckQuirksMode(BOOL force_quirks_mode)
{
	if (force_quirks_mode || !doctype_name || !uni_stri_eq(doctype_name, "HTML"))
		strict_mode = FALSE;
	else
	{
		size_t pub_len = doctype_public_id ? uni_strlen(doctype_public_id) : 0;
		size_t sys_len = doctype_system_id ? uni_strlen(doctype_system_id) : 0;

		if (pub_len > 0)
		{
			if (uni_stri_eq_lower(doctype_public_id, "html"))
				strict_mode = FALSE;
			else
			{
				if (pub_len > gDTDlenmin)
				{
					int index = (pub_len > gDTDlenmax) ? gDTDlengths[gDTDlenmax - gDTDlenmin + 1]
					: gDTDlengths[pub_len - gDTDlenmin];

					while (index >= 0)
					{
						const char *check_str = gDTDStrings[index];
						size_t check_len = op_strlen(check_str);

						if (check_len > pub_len)
							break;
						else if (uni_strni_eq(check_str, doctype_public_id, check_len))
						{
							int check_tok = gDTDtokens[index];
							if (((check_tok & DTD_EXACT_MATCH) && check_len == pub_len)
								|| !(check_tok & DTD_SYSTEM_IDENT))
							{
								if ((check_tok & DTD_TRIGGERS_LIMITED_QUIRKS)
									|| ((check_tok & DTD_NEED_SYSTEM_ID)
									&& doctype_system_id != NULL))
								{
									quirks_line_height_mode = TRUE;
								}
								else
									strict_mode = FALSE;
							}

							break;
						}

						index--;
					}
				}
			}
		}

		if (sys_len > 0)
		{
			if (sys_len > gDTDlenmin)
			{
				int index = (sys_len > gDTDlenmax) ? gDTDlengths[gDTDlenmax - gDTDlenmin]
				: gDTDlengths[sys_len - gDTDlenmin];

				while (index)
				{
					const char *check_str = gDTDStrings[index];
					size_t check_len = op_strlen(check_str);

					if (check_len > sys_len)
						break;
					else if (uni_strni_eq(check_str, doctype_system_id, check_len))
					{
						int check_tok = gDTDtokens[index];
						if ((check_tok & DTD_SYSTEM_IDENT)
							&& (!(check_tok & DTD_EXACT_MATCH)
							|| check_len == sys_len))
						{
							strict_mode = FALSE;

							if (check_tok & DTD_TRIGGERS_LIMITED_QUIRKS)
								quirks_line_height_mode = TRUE;
						}

						break;
					}

					index--;
				}
			}
		}
	}

	URL sn(GetURL() ? *GetURL() : URL());
	int override = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::CompatModeOverride, sn);

	if (override != CSS_COMPAT_MODE_AUTO)
	{
		strict_mode = (override == CSS_COMPAT_MODE_STRICT);
		return;
	}
}

BOOL LogicalDocument::IsParsingUnderElm(HTML_Element *elm)
{
	return m_parser && m_parser->IsParsingUnderElm(elm);
}

void LogicalDocument::SetIsInStrictMode(BOOL smode)
{
	int override = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::CompatModeOverride);
	if (override == CSS_COMPAT_MODE_AUTO)
		strict_mode = smode;
	else
		strict_mode = (override == CSS_COMPAT_MODE_STRICT);
}

BOOL LogicalDocument::IsSrcdoc()
{
	if (FramesDocument *frm_doc = GetDocument())
	{
		if (frm_doc->IsInlineFrame())
		{
			if (FramesDocElm *fdelm = frm_doc->GetDocManager()->GetFrame())
			{
				if (HTML_Element *frame_elm = fdelm->GetHtmlElement())
				{
					return frame_elm->HasAttr(Markup::HA_SRCDOC);
				}
			}
		}
	}

	return FALSE;
}

#ifndef USE_HTML_PARSER_FOR_XML
class XMLFragmentTreeListener : public OpTreeCallback
{
public:
	HTML_Element *m_found_element;
	LogicalDocument *m_logdoc;
	XMLFragmentTreeListener(LogicalDocument *logdoc) : m_found_element(NULL), m_logdoc(logdoc) {}
	virtual ~XMLFragmentTreeListener()
	{
		if (m_found_element)
		{
			HTML_Element* current_element = m_found_element;
			while (HTML_Element *parent = current_element->Parent())
				current_element = parent;

			if (current_element->Clean(m_logdoc))
				current_element->Free(m_logdoc);
		}
	}
	virtual OP_STATUS ElementFound(HTML_Element *element) { m_found_element = element; return OpStatus::OK; }
	virtual OP_STATUS ElementNotFound() { OP_ASSERT(!"Shouldn't be called"); return OpStatus::OK; }
};
#endif // !USE_HTML_PARSER_FOR_XML

OP_STATUS LogicalDocument::ParseFragment(HTML_Element *root, HTML_Element *context_elm, const uni_char *buffer, unsigned buffer_length, BOOL use_xml_parser)
{
	OP_PROBE7(OP_PROBE_LOGDOC_PARSEFRAGMENT);

#ifndef USE_HTML_PARSER_FOR_XML
	if (use_xml_parser)
	{
		XMLParser *parser;
		XMLFragmentTreeListener callback(this);
		URL dummy;
		XMLTokenHandler *tokenhandler;
		RETURN_IF_ERROR(OpTreeCallback::MakeTokenHandler(tokenhandler, this, &callback));
		OpAutoPtr<XMLTokenHandler> ap_tokenhandler(tokenhandler);

		RETURN_IF_ERROR(XMLParser::Make(parser, NULL, GetFramesDocument(), tokenhandler, dummy));
		OpAutoPtr<XMLParser> ap_parser(parser);

		XMLParser::Configuration configuration;
		configuration.preferred_literal_part_length = SplitTextLen;
		configuration.parse_mode = XMLParser::PARSEMODE_FRAGMENT;
		configuration.store_internal_subset = FALSE;
		configuration.allow_xml_declaration = FALSE;
		configuration.allow_doctype = FALSE;
		XMLNamespaceDeclaration::Reference nsdeclaration;
		if (context_elm)
			RETURN_IF_ERROR(XMLNamespaceDeclaration::PushAllInScope(nsdeclaration, context_elm, TRUE));
		configuration.nsdeclaration = nsdeclaration;
		configuration.max_tokens_per_call = (1 << 20); // 2^20 as suggested in the documentation of this field.
# ifdef XML_ERRORS
		configuration.generate_error_report = FALSE;
# endif // XML_ERRORS
		parser->SetConfiguration(configuration);

		RETURN_IF_ERROR(parser->Parse(buffer, buffer_length, FALSE));

		if (parser->IsOutOfMemory())
			return OpStatus::ERR_NO_MEMORY;

		if (parser->IsFailed() || !parser->IsFinished())
			return OpStatus::ERR;

		if (callback.m_found_element)
		{
			HTML_Element *child = callback.m_found_element->FirstChild();
			while (child)
			{
				HTML_Element *next_child = child->Suc();
				child->OutSafe(GetFramesDocument(), FALSE);
				child->UnderSafe(GetFramesDocument(), root);
				child = next_child;
			}
		}
	}
	else
#endif // !USE_HTML_PARSER_FOR_XML
	{
		if (!m_fragment_parser)
		{
			m_fragment_parser = OP_NEW(HTML5Parser, (this));
			if (!m_fragment_parser)
				return OpStatus::ERR_NO_MEMORY;
		}
		else
			m_fragment_parser->ResetParser();

		RETURN_IF_MEMORY_ERROR(m_fragment_parser->AppendData(buffer, buffer_length, TRUE, TRUE));
		RETURN_IF_MEMORY_ERROR(m_fragment_parser->Parse(root, root == context_elm ? NULL : context_elm));
	}

	return OpStatus::OK;
}

#ifdef DELAYED_SCRIPT_EXECUTION
HTML_Element* LogicalDocument::GetParserStateLastNonFosterParentedElement() const
{
	return m_parser ? m_parser->GetLastNonFosterParentedElement() : NULL;
}

OP_STATUS LogicalDocument::StoreParserState(HTML5ParserState* state)
{
	OP_ASSERT(m_parser);

	RETURN_IF_ERROR(m_parser->StoreParserState(state));

	return OpStatus::OK;
}

OP_STATUS LogicalDocument::RestoreParserState(HTML5ParserState* state, HTML_Element* script_element, unsigned buffer_stream_position, BOOL script_has_completed)
{
	// need to "rewind" the primary tokenizer to the state it was in when
	// this script element was added
	OP_ASSERT(m_parser);

	RETURN_IF_ERROR(m_parser->RestoreParserState(state, script_element, buffer_stream_position, script_has_completed));

	return OpStatus::OK;
}

BOOL LogicalDocument::HasParserStateChanged(HTML5ParserState* state)
{
	OP_ASSERT(m_parser);

	return m_parser->HasParserStateChanged(state);
}

unsigned LogicalDocument::GetLastBufferStartOffset()
{
	return m_parser ? m_parser->GetLastBufferStartOffset() : 0;
}
#endif // DELAYED_SCRIPT_EXECUTION

BOOL LogicalDocument::SignalScriptFinished(HTML_Element *script_element, ES_LoadManager *loadman)
{
	if (m_parser)
	{
		BOOL started_by_parser = m_parser->SignalScriptFinished(script_element, loadman);
		if (!loadman->HasParserBlockingScript())
		{
			m_continue_parsing = TRUE;
#ifdef SPECULATIVE_PARSER
			DisableSpeculativeParser();
#endif // SPECULATIVE_PARSER
		}
		return started_by_parser;
	}

	return FALSE;
}

OP_STATUS LogicalDocument::SetPendingScriptData(const uni_char* data, unsigned data_length)
{
	if (data)
	{
		OP_ASSERT(!m_pending_write_data);
		m_pending_write_data = OP_NEWA(uni_char, data_length);
		m_consumed_pending_write_data = 0;
		if (!m_pending_write_data)
			return OpStatus::ERR_NO_MEMORY;

		op_memcpy(m_pending_write_data, data, sizeof(uni_char)*data_length);
		m_pending_write_data_length = data_length;
	}
	else
	{
		OP_DELETEA(m_pending_write_data);
		m_pending_write_data = NULL;
		m_pending_write_data_length = 0;
		m_consumed_pending_write_data = 0;

		// Only the HTML parser will signal that it has finished a doc.write
		// buffer, so in all other cases (like XML in selftest) we do it here.
		if (!m_parser)
			hld_profile.GetESLoadManager()->SignalWriteFinished();
	}

	return OpStatus::OK;
}

void LogicalDocument::ConsumePendingScriptData(unsigned length)
{
	OP_ASSERT(length <= m_pending_write_data_length - m_consumed_pending_write_data);
	m_consumed_pending_write_data += length;

	if (m_consumed_pending_write_data == m_pending_write_data_length)
		OpStatus::Ignore(SetPendingScriptData(NULL, 0));
}

const uni_char* LogicalDocument::GetPendingScriptData(unsigned &length)
{
	length = m_pending_write_data_length - m_consumed_pending_write_data;
	return m_pending_write_data + m_consumed_pending_write_data;
}
