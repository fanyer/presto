/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/logdoc/htm_ldoc.h"

#include "modules/util/opfile/opfile.h"
#include "modules/util/opfile/unistream.h"
#include "modules/hardcore/unicode/unicode.h"
#include "modules/img/image.h"
#include "modules/logdoc/html5parser.h"
#include "modules/logdoc/xml_ev.h"
#include "modules/logdoc/link.h"
#include "modules/logdoc/helistelm.h"
#include "modules/logdoc/datasrcelm.h"
#include "modules/probetools/probepoints.h"
#include "modules/layout/box/box.h"
#include "modules/layout/cascade.h"
#include "modules/style/css_save.h"
#include "modules/url/url_api.h"
#include "modules/url/url_sn.h"
#include "modules/olddebug/timing.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/encodings/detector/charsetdetector.h"
#include "modules/encodings/utility/charsetnames.h"
#include "modules/layout/layout_workplace.h"

#include "modules/display/color.h"
#include "modules/display/styl_man.h"
#include "modules/forms/formvalue.h"
#include "modules/forms/formvaluelist.h"
#include "modules/forms/formvaluetextarea.h"
#include "modules/forms/formvalueradiocheck.h"
#include "modules/forms/webforms2support.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/util/path.h"
#include "modules/util/handy.h"

#include "modules/dochand/fdelm.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/logdoc/htm_lex.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/src/html5/html5dse_restore_attr.h"
#include "modules/logdoc/src/textdata.h"
#include "modules/style/css.h"
#include "modules/style/css_media.h"
#include "modules/style/cssmanager.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/windowcommander/src/WindowCommander.h"

#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"

#include "modules/wand/wandmanager.h"

#include "modules/xmlutils/xmldocumentinfo.h"

#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/dom/domenvironment.h"
#ifdef XML_EVENTS_SUPPORT
# include "modules/logdoc/xmlevents.h"
#endif // XML_EVENTS_SUPPORT

#ifdef WBXML_SUPPORT
# include "modules/logdoc/wbxml_decoder.h"
#endif // WBXML_SUPPORT

#ifdef _WML_SUPPORT_
# include "modules/logdoc/wml.h"
#endif // _WML_SUPPORT_

#ifdef M2_SUPPORT
# include "modules/logdoc/omfenum.h"
#endif // M2_SUPPORT

#ifdef SVG_SUPPORT
# include "modules/svg/SVGManager.h"
#endif // SVG_SUPPORT

#ifdef DOCUMENT_EDIT_SUPPORT
# include "modules/documentedit/OpDocumentEdit.h"
#endif

#ifdef JS_PLUGIN_SUPPORT
# include "modules/jsplugins/src/js_plugin_object.h"
#endif // JS_PLUGIN_SUPPORT

#ifdef HAS_ATVEF_SUPPORT
# include "modules/display/tvmanager.h"
#endif // HAS_ATVEF_SUPPORT

#ifdef MEDIA_HTML_SUPPORT
# include "modules/media/mediaelement.h"
#endif // MEDIA_HTML_SUPPORT

#ifdef _DEBUG
# include "modules/olddebug/tstdump.h"
#endif // _DEBUG

#ifdef DISPLAY_WRITINGSYSTEM_HEURISTIC
# include "modules/display/writingsystemheuristic.h"
#endif // DISPLAY_WRITINGSYSTEM_HEURISTIC

#define in_headcontent(elm_type) \
	(elm_type == HE_TITLE || elm_type == HE_ISINDEX || \
	 elm_type == HE_BASE || elm_type == HE_META || \
	 elm_type == HE_STYLE)

HLDocProfile::HLDocProfile()
	:
	is_out_of_memory(FALSE)
#ifdef _PRINT_SUPPORT_
	, print_image_list(NULL)
#endif
	, base_url(NULL)
	, real_url(NULL)
	, refresh_url(NULL)
	, refresh_seconds(-1)
	, hldoc(NULL)
	, form_elm_count(0)
	, embed_elm_count(0)
	, form_count(0)
	, text_count(0)
	, is_frames_doc(FALSE)
	, embed_not_supported(FALSE)
	, handle_script(TRUE)
	, character_set(NULL)
	, css_handling(CSS_FULL)
	, unloaded_css(FALSE)
	, is_xhtml(FALSE)
	, has_media_style(0)
	, vis_dev(NULL)
	, new_style_info(FALSE)
	, frames_doc(NULL)
	, es_load_manager(this)
	, es_script_not_supported(FALSE)
	, running_script_is_external(FALSE)
# ifdef DELAYED_SCRIPT_EXECUTION
	, es_delay_scripts(FALSE) // is set to correct value in HLDocProfile::SetFramesDocument.
	, es_is_parsing_ahead(FALSE)
	, es_is_executing_delayed_script(FALSE)
	, es_is_starting_delayed_script(FALSE)
	, es_need_recover(FALSE)
# endif // DELAYED_SCRIPT_EXECUTION
#ifdef _WML_SUPPORT_
# ifdef ACCESS_KEYS_SUPPORT
	, m_next_do_key(OP_KEY_FIRST_DO_UNSPECIFIED)
# endif // ACCESS_KEYS_SUPPORT
#endif //_WML_SUPPORT_
#ifdef _CSS_LINK_SUPPORT_
	, current_css_link(NULL)
	, next_css_link(NULL)
#endif
	, meta_description(NULL)
	, is_parsing_inner_html(FALSE)
#ifdef CSS_ERROR_SUPPORT
	, m_css_error_count(0)
#endif // CSS_ERROR_SUPPORT
#ifdef FONTSWITCHING
	, preferred_codepage(OpFontInfo::OP_DEFAULT_CODEPAGE)
	, m_preferred_script(WritingSystem::Unknown)
	, m_guessed_script(WritingSystem::Unknown)
# ifdef DISPLAY_WRITINGSYSTEM_HEURISTIC
	, use_text_heuristic(TRUE)
# endif // DISPLAY_WRITINGSYSTEM_HEURISTIC
#endif // defined(FONTSWITCHING)
	, handheld_callback_called(FALSE)
	, doctype_considered_handheld(FALSE)
{
}


HLDocProfile::~HLDocProfile()
{
	OP_DELETEA(meta_description);

#ifdef ACCESS_KEYS_SUPPORT
	m_access_keys.Clear();
#endif // ACCESS_KEYS_SUPPORT

	OP_DELETE(base_url);
	OP_DELETE(real_url);

	OP_DELETEA(character_set);

	if (refresh_url)
		OP_DELETE(refresh_url);

	link_list.Clear();

	OpStatus::Ignore(es_load_manager.CancelInlineThreads());

#ifdef DELAYED_SCRIPT_EXECUTION
	es_delayed_scripts.Clear();
#endif // DELAYED_SCRIPT_EXECUTION

#ifdef _CSS_LINK_SUPPORT_
	current_css_link = NULL;
	next_css_link = NULL;
#endif
}

#ifdef _CSS_LINK_SUPPORT_
OP_STATUS HLDocProfile::GetNextCSSLink(URL **url)
{
    // stighal: TODO how to notify user
	if (!next_css_link)
	{
		next_css_link = OP_NEW(URLLink, (URL()));
		if (next_css_link)
			next_css_link->Into(&css_links);
		else
			return OpStatus::ERR_NO_MEMORY;
	}

	*url = &next_css_link->url;
	return OpStatus::OK;
}

OP_STATUS HLDocProfile::SetCSSLink(URL& new_url)
{
	current_css_link = next_css_link;
	next_css_link = NULL;

	if (current_css_link)
	{
		current_css_link->url = new_url;
	}
	else
	{
		current_css_link = OP_NEW(URLLink, (new_url));
		if(current_css_link)
			current_css_link->Into(&css_links);
		else
			return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

void HLDocProfile::CleanCSSLink()
{
	current_css_link = NULL;
	next_css_link = NULL;
}
#endif

void HLDocProfile::SetBaseURL(const URL* url, const uni_char* url_string)
{
	// Setting base url to NULL happens when a BASE
	// element or its href is being removed. If that
	// was implemented correctly, other BASE elements
	// would be searched. Anyway, if there are no more
	// BASE elements present, the base url should be
	// reset to the document's url. Try to do that here.
	if (!url)
		url = real_url;

	if (!url)
	{
		OP_DELETE(base_url);
		base_url = NULL;
	}
	else if (base_url)
		*base_url = *url;
	else
	{
		base_url = OP_NEW(URL, (*url));
		if (!base_url)
			SetIsOutOfMemory(TRUE);
	}
#ifdef DELAYED_SCRIPT_EXECUTION
	if (es_is_executing_delayed_script)
	{
		es_need_recover = TRUE;
	}
#endif
	if (url_string)
	{
		OP_STATUS status = base_url_string.Set(url_string);
		if (OpStatus::IsMemoryError(status))
		{
			base_url_string.Empty();
			SetIsOutOfMemory(TRUE);
		}
	}
	else
		base_url_string.Empty();

	if (GetRoot())
		GetRoot()->ClearResolvedUrls();
}

void HLDocProfile::SetURL(const URL* url)
{
	if (real_url)
		*real_url = *url;
	else
	{
		real_url = OP_NEW(URL, (*url));
		if (!real_url)
			SetIsOutOfMemory(TRUE);
	}
}

void HLDocProfile::OnPutHistoryState(const URL& new_url)
{
	// BaseURLString may be relative, use GetURL to get the full URL.
	// If there is no base URL then use the new document URL.
	URL new_base_url = BaseURLString() ? g_url_api->GetURL(new_url, BaseURLString()) : new_url;
	URL* current_document_url = GetURL();
	URL* current_base_url = BaseURL() ? BaseURL() : GetURL();

	OP_ASSERT(current_document_url && current_base_url);

	if (!(*current_document_url == new_url || *current_base_url == new_base_url))
	{
		// Set new URLs in HLDocProfile so all relative inlines' URLs are resolved in relation to
		// the new URL
		SetBaseURL(&new_base_url, BaseURLString());
		SetURL(&new_url);
#ifdef DELAYED_SCRIPT_EXECUTION
		// Since the base URL has changed there might be some inlines which had been parsed ahead and now they have wrong relative URLs
		ESRecoverIfNeeded();
#endif // DELAYED_SCRIPT_EXECUTION
	}
}

void HLDocProfile::SetRefreshURL(const URL* url, short sec)
{
	if (refresh_url)
		*refresh_url = *url;
	else
	{
		refresh_url = OP_NEW(URL, (*url));
		if (!refresh_url)
			SetIsOutOfMemory(TRUE);
	}
	refresh_seconds = sec;
}

void HLDocProfile::SetBodyElm(HTML_Element* bhe)
{
	OP_ASSERT(bhe);

	body_he.SetElm(bhe);

#ifdef DOCUMENT_EDIT_SUPPORT
	if (bhe->IsContentEditable(TRUE))
	{
		OpStatus::Ignore(frames_doc->SetEditable(FramesDocument::CONTENTEDITABLE));
		if (frames_doc->GetDocumentEdit())
			frames_doc->GetDocumentEdit()->InitEditableRoot(bhe);
	}
#endif // DOCUMENT_EDIT_SUPPORT
}

BOOL HLDocProfile::IsLoaded()
{
	return hldoc->IsLoaded();
}

BOOL HLDocProfile::IsParsed()
{
	return hldoc->IsParsed();
}

void HLDocProfile::RemoveLink(HTML_Element* he)
{
	LinkElement* link = link_list.First();
	while (link != NULL)
	{
		LinkElement* next = link->Suc();
		if (link->IsElm(he))
		{
			link_list.RemoveFromList(link);
			OP_DELETE(link);
		}
		link = next;
	}
}

OP_STATUS HLDocProfile::HandleLink(HTML_Element* he)
{
	LinkElement* link = NULL;

	OP_STATUS result = LinkElement::CollectLink(he, &link);

	if (OpStatus::IsError(result))
		return result;

	if (link == NULL)
		return OpStatus::OK;

	link_list.AddToList(link);

#ifdef SHORTCUT_ICON_SUPPORT
	if (link->GetKinds() & LINK_TYPE_ICON)
		RETURN_IF_ERROR(frames_doc->LoadIcon(link->GetHRef(hldoc), he));
#endif // SHORTCUT_ICON_SUPPORT

	if (link->IsStylesheet())
	{
		LayoutMode layout_mode = frames_doc->GetWindow()->GetLayoutMode();
		BOOL is_smallscreen = layout_mode == LAYOUT_SSR || layout_mode == LAYOUT_CSSR;

		const uni_char *type = he->GetStringAttr(ATTR_TYPE, NS_IDX_HTML);
		// Only read stylesheets that match text/css or has no explicit type. The
		// type might have extra parameters after a ; char so only match the part
		// before it.
		if (!type || !*type || (uni_strni_eq(type, "text/css", 8) &&
		                        (type[8] == '\0' || type[8] == ';')))
		{
#ifdef FORMAT_UNSTYLED_XML
			if (IsXml())
				hldoc->SetXmlStyled();
#endif // FORMAT_UNSTYLED_XML

			if ((is_smallscreen && ((GetCSSMode() == CSS_FULL && HasMediaStyle(CSS_MEDIA_TYPE_HANDHELD))))
				|| (layout_mode != LAYOUT_SSR
					&& g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(GetCSSMode(),
						PrefsCollectionDisplay::EnableAuthorCSS), *GetURL()))
				|| frames_doc->GetWindow()->IsCustomWindow())
			{
				URL* css_url = link->GetHRef(hldoc);

				if (css_url && !css_url->IsEmpty())
					RETURN_IF_MEMORY_ERROR(frames_doc->LoadInline(css_url, he, CSS_INLINE));
			}
			else if (is_smallscreen && !HasMediaStyle(CSS_MEDIA_TYPE_HANDHELD) && GetCSSMode() == CSS_FULL && link->GetMedia() & CSS_MEDIA_TYPE_HANDHELD)
			{
				SetHasMediaStyle(CSS_MEDIA_TYPE_HANDHELD);
				return LoadAllCSS();
			}
			else if (layout_mode != LAYOUT_SSR
					&& !g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(GetCSSMode(),
						PrefsCollectionDisplay::EnableAuthorCSS), *GetURL()))
				SetUnloadedCSS(TRUE);

			SetHasMediaStyle(link->GetMedia());
		}

		if ((link->GetKinds() & LINK_GROUP_ALT_STYLESHEET) == LINK_GROUP_ALT_STYLESHEET)
		{
			BOOL disabled = TRUE;

			if (he->Parent())
				disabled = he->Parent()->GetSpecialBoolAttr(ATTR_STYLESHEET_DISABLED, SpecialNs::NS_LOGDOC, disabled);

			he->SetSpecialBoolAttr(ATTR_STYLESHEET_DISABLED, disabled, SpecialNs::NS_LOGDOC);
		}
	}

	return OpStatus::OK;
}

OP_STATUS HLDocProfile::LoadAllCSS()
{
	OP_STATUS status = OpStatus::OK;

	if (hldoc->GetRoot())
	{
		for (LinkElement* link = link_list.First(); link; link = link->Suc())
			if (link->IsStylesheet())
			{
				URL* css_url = link->GetHRef(hldoc);
				if (css_url && !css_url->IsEmpty())
				{
					status = frames_doc->LoadInline(css_url, link->GetElement(), CSS_INLINE);

					if (OpStatus::IsError(status))
						return status;
				}
			}

		status = hldoc->GetRoot()->LoadAllCSS(HTML_Element::DocumentContext(frames_doc));
	}

	return status;
}

BOOL HLDocProfile::GetESEnabled()
{
	if (IsFramesDoc())
		return FALSE;

	return DOM_Environment::IsEnabled(frames_doc);
}

#if defined(FONTSWITCHING)
BOOL HLDocProfile::SetPreferredScript(const char* cset)
{
	if (cset && *cset)
	{
		WritingSystem::Script script = WritingSystem::FromEncoding(cset);

		if (script != WritingSystem::Unknown)
		{
			SetPreferredScript(script);
			return TRUE;
		}
	}

	return FALSE;
}

# ifdef DISPLAY_WRITINGSYSTEM_HEURISTIC
void HLDocProfile::AnalyzeText(HTML_Element* text_element)
{
	if (use_text_heuristic && IsTextElementVisible(text_element, NULL))
	{
		if (TextData* td = text_element->GetTextData())
			AnalyzeText(td->GetText(), td->GetTextLen());
	}
}

void HLDocProfile::AnalyzeText(const uni_char* text, unsigned text_len)
{
	if (!use_text_heuristic)
		return;

	m_writing_system_heuristic.Analyze(text, text_len);

	m_guessed_script
		= m_writing_system_heuristic.GuessScript();

	if (m_guessed_script != m_preferred_script)
	{
		SetPreferredScript(m_guessed_script);

		// The updated guess of the writing system heuristic is different
		// from the old guess (either that or we're analyzing the first
		// text node). As this could change font preferences, we need to
		// invalidate all text nodes.
		GetRoot()->RemoveCachedTextInfo(frames_doc);
	}
}

# endif // DISPLAY_WRITINGSYSTEM_HEURISTIC
#endif // FONTSWITCHING

OP_STATUS HLDocProfile::SetCharacterSet(const char* cset)
{
	OP_STATUS status;
	if( (status = SetStr(character_set, cset)) != OpStatus::OK)
	{
		if(status == OpStatus::ERR_NO_MEMORY )
			is_out_of_memory = TRUE;

		return status;
	}

#if defined(FONTSWITCHING)
	if (SetPreferredScript(cset))
		use_text_heuristic = FALSE;
#endif // FONTSWITCHING

#ifdef SUPPORT_TEXT_DIRECTION
	const char* charset = GetCharacterSet();

	if (charset && op_strcmp(charset, "iso-8859-8") == 0 && hldoc)
	{
		if (LayoutWorkplace* workplace = hldoc->GetLayoutWorkplace())
			workplace->SetBidiStatus(VISUAL_ENCODING);
	}
#endif // SUPPORT_TEXT_DIRECTION

    return OpStatus::OK;
}

void HLDocProfile::SetNewForm(HTML_Element* current_form)
{
	if (current_form)
		form.SetElm(current_form);
	else
		form.Reset();
}

COLORREF HLDocProfile::GetBgColor() const
{
	return body_he.GetElm() ? body_he->GetBgColor() : USE_DEFAULT_COLOR;
}

COLORREF HLDocProfile::GetTextColor() const
{
	return body_he.GetElm() ? body_he->GetTextColor() : USE_DEFAULT_COLOR;
}

COLORREF HLDocProfile::GetLinkColor() const
{
	return body_he.GetElm() ? body_he->GetLinkColor() : USE_DEFAULT_COLOR;
}

COLORREF HLDocProfile::GetVLinkColor() const
{
	return body_he.GetElm() ? body_he->GetVLinkColor() : USE_DEFAULT_COLOR;
}

COLORREF HLDocProfile::GetALinkColor() const
{
	return body_he.GetElm() ? body_he->GetALinkColor() : USE_DEFAULT_COLOR;
}

void HLDocProfile::SetBgColor(COLORREF color)
{
	if (body_he.GetElm())
		body_he->SetNumAttr(ATTR_BGCOLOR, color);
}

void HLDocProfile::SetTextColor(COLORREF color)
{
	if (body_he.GetElm())
		body_he->SetNumAttr(ATTR_TEXT, color);
}

void HLDocProfile::SetLinkColor(COLORREF color)
{
	if (body_he.GetElm())
		body_he->SetNumAttr(ATTR_LINK, color);
}

void HLDocProfile::SetVLinkColor(COLORREF color)
{
	if (body_he.GetElm())
		body_he->SetNumAttr(ATTR_VLINK, color);
}

void HLDocProfile::SetALinkColor(COLORREF color)
{
	if (body_he.GetElm())
		body_he->SetNumAttr(ATTR_ALINK, color);
}

void HLDocProfile::SetIsOutOfMemory(BOOL value)
{
    is_out_of_memory = value;
	if (!value) g_memory_manager->RaiseCondition( OpStatus::ERR_NO_MEMORY );
}

HtmlDocType CheckHtmlDocType(HTML_Element* root)
{
	HTML_Element* he = root->FirstChild();

	while (he)
		if (he->Type() != HE_FRAMESET &&
            (he->Type() == HE_HEAD ||
             he->Type() == HE_SCRIPT ||
             in_headcontent(he->Type()) ||
             !he->HasContent()))
			he = he->Suc();
		else
            if (he->Type() == HE_FRAMESET)
                return HTML_DOC_FRAMES;
            else
                return HTML_DOC_PLAIN;

	return HTML_DOC_UNKNOWN;
}

BOOL HLDocProfile::IsElmLoaded(HTML_Element* he) const
{
	OP_ASSERT(he);

	return !hldoc->IsParsingUnderElm(he);
}

BOOL HLDocProfile::AddCSS(CSS* new_css)
{
	css_collection.AddCollectionElement(new_css, TRUE);

	HTML_Element* this_css_he = new_css->GetHtmlElement();

	if (this_css_he && new_css->GetKind() == CSS::STYLE_PREFERRED)
	{
		LinkElement* preferred_style = NULL;
		for (LinkElement* link = link_list.First(); link; link = link->Suc())
		{
			if (link->IsElm(this_css_he))
			{
				if (preferred_style && (!link->GetTitle() || uni_strcmp(link->GetTitle(), preferred_style->GetTitle()) != 0))
					new_css->SetEnabled(FALSE);

				break;
			}
			else
				if (!preferred_style && (link->GetKinds() & LINK_TYPE_STYLESHEET) && !(link->GetKinds() & LINK_TYPE_ALTERNATE) && link->GetTitle())
					preferred_style = link;
		}
	}

	if (new_css->IsEnabled() && frames_doc->IsWaitingForStyles())
		css_collection.CheckFontPrefetchCandidates();

	return TRUE;
}

void HLDocProfile::SetFramesDocument(FramesDocument* doc)
{
	frames_doc = doc;
	css_collection.SetFramesDocument(doc);

#ifdef DELAYED_SCRIPT_EXECUTION
	if (doc && doc->GetWindow()->GetType() != WIN_TYPE_DIALOG)
	{
		es_delay_scripts = g_pcjs->GetIntegerPref(PrefsCollectionJS::DelayedScriptExecution, doc->GetURL());

#ifdef DOM_DSE_DEBUGGING
		if (es_delay_scripts)
			hldoc->DSESetEnabled();
#endif // DOM_DSE_DEBUGGING
	}
	else
		es_delay_scripts = FALSE;
#endif // DELAYED_SCRIPT_EXECUTION
}

BOOL HLDocProfile::LoadCSS_Url(HTML_Element* css_he)
{
	BOOL css_loaded = FALSE;
	URL* css_url = css_he->GetLinkURL(hldoc);

	if (css_url)
	{
		OP_ASSERT(css_url->Status(TRUE) == URL_LOADED);

		URL target_url = css_url->GetAttribute(URL::KMovedToURL, URL::KFollowRedirect);
		URL origin_url = css_he->GetLinkOriginURL();

		if (target_url.IsEmpty())
			target_url = *css_url;

		// Check if this css url has been loaded before to avoid recursion

		if (css_he->IsCssImport())
		{
			HTML_Element* parent = css_he->Parent();
			while (parent)
			{
				const URL* parent_css_url = parent->GetUrlAttr(ATTR_HREF, NS_IDX_HTML, hldoc);
				if (parent_css_url && parent_css_url->Id(TRUE) == target_url.Id(TRUE))
					return FALSE;

				if (!parent->IsCssImport())
					break;

				parent = parent->Parent();
			}
		}

		// read data

		OP_STATUS status = OpStatus::OK;

		HEListElm* helm = css_he->GetHEListElmForInline(CSS_INLINE);
		unsigned short int parent_charset_id = helm ? GetSuggestedCharsetId(css_he, this, helm->GetLoadInlineElm()) : 0;
		g_charsetManager->IncrementCharsetIDReference(parent_charset_id);

		URL_DataDescriptor* url_data_desc;
		if (helm && OpStatus::IsSuccess(helm->CreateURLDataDescriptor(url_data_desc, NULL, URL::KFollowRedirect, FALSE, TRUE, frames_doc->GetWindow(), URL_CSS_CONTENT, 0, FALSE, parent_charset_id)))
		{
			BOOL more;
								__DO_START_TIMING(TIMING_CACHE_RETRIEVAL);
								__TIMING_CODE(unsigned long plen = url_data_desc->GetBufSize());
#ifdef OOM_SAFE_API
 			TRAP(status, url_data_desc->RetrieveDataL(more));
			if (status == OpStatus::ERR_NO_MEMORY)
				SetIsOutOfMemory(TRUE);
#else
			url_data_desc->RetrieveData(more);
#endif //OOM_SAFE_API

			uni_char* data_buf = (uni_char*)url_data_desc->GetBuffer();
			unsigned long data_len = url_data_desc->GetBufSize();

								__DO_ADD_TIMING_PROCESSED(TIMING_CACHE_RETRIEVAL,data_len -plen);
								__DO_STOP_TIMING(TIMING_CACHE_RETRIEVAL);


			OP_ASSERT(css_he->GetDataSrc()->IsEmpty());
			// If the above assert ever trigger, enable next line
			//css_he->EmptySrcListAttr();

			if (data_len == 0)
				css_he->GetDataSrc()->SetOriginURL(origin_url);
			else while (data_buf && UNICODE_DOWNSIZE(data_len) > 0 && status == OpStatus::OK)
			{
				if (data_len >= USHRT_MAX-64)
					data_len = USHRT_MAX-65;
								__DO_START_TIMING(TIMING_DOC_LOAD);
				status = css_he->AddToSrcListAttr(data_buf, UNICODE_DOWNSIZE(data_len), origin_url);
								__DO_ADD_TIMING_PROCESSED(TIMING_DOC_LOAD,data_len);
								__DO_STOP_TIMING(TIMING_DOC_LOAD);
				url_data_desc->ConsumeData(data_len);

								__DO_START_TIMING(TIMING_CACHE_RETRIEVAL);
								__TIMING_CODE(unsigned long plen = url_data_desc->GetBufSize());
#ifdef OOM_SAFE_API
				TRAP(status, url_data_desc->RetrieveDataL(more));
#else
				url_data_desc->RetrieveData(more);
#endif //OOM_SAFE_API
								__DO_ADD_TIMING_PROCESSED(TIMING_CACHE_RETRIEVAL,data_len -plen);
								__DO_STOP_TIMING(TIMING_CACHE_RETRIEVAL);
				data_buf = (uni_char*)url_data_desc->GetBuffer();
				data_len = url_data_desc->GetBufSize();
			}

			OP_DELETE(url_data_desc);
		}
		else
		{
			DataSrc* src_head = css_he->GetDataSrc();
			if (src_head)
				src_head->SetOriginURL(origin_url);
			else
				status = OpStatus::ERR_NO_MEMORY;
		}
		g_charsetManager->DecrementCharsetIDReference(parent_charset_id);

		if (status == OpStatus::OK)
		{
			status = css_he->LoadStyle(HTML_Element::DocumentContext(frames_doc), FALSE);

			HTML_Element *tmp = NULL;
			if (css_he->Parent() && css_he->Parent()->HasSpecialAttr(ATTR_STYLESHEET_DISABLED, SpecialNs::NS_LOGDOC))
				tmp = css_he->Parent();
			else if (css_he->HasSpecialAttr(ATTR_STYLESHEET_DISABLED, SpecialNs::NS_LOGDOC))
				tmp = css_he;

			if (tmp)
			{
				BOOL disabled = tmp->GetSpecialBoolAttr(ATTR_STYLESHEET_DISABLED, SpecialNs::NS_LOGDOC);
				FramesDocument *frames_doc = GetFramesDocument();
				css_he->SetStylesheetDisabled(frames_doc, disabled);
			}

			css_loaded = status == OpStatus::OK;
		}

		if (status == OpStatus::ERR_NO_MEMORY)
			SetIsOutOfMemory(TRUE);
	}

	return css_loaded;
}

void HLDocProfile::SetCSSMode(CSSMODE new_css_handling)
{
	if (css_handling != new_css_handling)
	{
		css_handling = new_css_handling;

		if (HTML_Element *root = GetRoot())
			root->MarkPropsDirty(frames_doc);
	}
}

#ifdef _WML_SUPPORT_
BOOL HLDocProfile::IsWml()
{
    return hldoc->IsWml();
}

BOOL HLDocProfile::HasWmlContent()
{
	return GetFramesDocument() && GetFramesDocument()->GetDocManager()
		&& GetFramesDocument()->GetDocManager()->WMLHasWML();
}

void HLDocProfile::WMLInit()
{
    if (OpStatus::IsMemoryError(GetFramesDocument()->GetDocManager()->WMLInit()))
        SetIsOutOfMemory(TRUE);
}

WML_Context* HLDocProfile::WMLGetContext()
{
    DocumentManager *dm = GetFramesDocument()->GetDocManager();
    WML_Context *ret_context = dm->WMLGetContext();
    if (!ret_context)
    {
        if (OpStatus::IsMemoryError(dm->WMLInit()))
            SetIsOutOfMemory(TRUE);
        ret_context = dm->WMLGetContext();
    }

    return ret_context;
}

void HLDocProfile::WMLSetCurrentCard(HTML_Element* he_card)
{
	if (wml_card_elm.GetElm())
	{
		wml_card_elm->MarkExtraDirty(frames_doc);
		wml_card_elm->MarkPropsDirty(frames_doc);
	}
	wml_card_elm.SetElm(he_card);
	if (wml_card_elm.GetElm())
	{
		wml_card_elm->MarkExtraDirty(frames_doc);
		wml_card_elm->MarkPropsDirty(frames_doc);
	}
}
#endif // _WML_SUPPORT_

BOOL HLDocProfile::IsXml()
{
	return hldoc->IsXml();
}

BOOL HLDocProfile::IsXmlSyntaxHighlight()
{
#ifdef FORMAT_UNSTYLED_XML
	return hldoc->IsXmlSyntaxHighlight();
#else // FORMAT_UNSTYLED_XML
	return FALSE;
#endif // FORMAT_UNSTYLED_XML
}

BOOL HLDocProfile::IsXhtml()
{
	if (hldoc->IsXml())
	{
		HTML_Element *root = hldoc->GetRoot();
		if (root && root->GetNsType() == NS_HTML && root->Type() == HE_HTML)
			return TRUE;
	}

	return FALSE;
}

void HLDocProfile::SetXhtml(BOOL the_truth)
{
	is_xhtml = the_truth;

#ifdef USE_HTML_PARSER_FOR_XML
	OpStatus::Ignore(XMLNamespaceDeclaration::Push(m_current_ns_decl,
		UNI_L("http://www.w3.org/1999/xhtml"),
		uni_strlen(UNI_L("http://www.w3.org/1999/xhtml")),
		NULL, NULL, 0));
#endif // USE_HTML_PARSER_FOR_XML
}

#ifdef MEDIA_HTML_SUPPORT
static OP_STATUS
InsertMediaElement(HTML_Element* element)
{
	HTML_Element* media_elm = element->Type() == HE_SOURCE || element->Type() == Markup::HTE_TRACK ? element->ParentActual() : element;
	if (media_elm)
	{
		MediaElement* media = media_elm->GetMediaElement();
		if (media)
			return media->HandleElementChange(element, TRUE, NULL);
	}
	return OpStatus::OK;
}
#endif // MEDIA_HTML_SUPPORT

#ifdef DELAYED_SCRIPT_EXECUTION
HLDocProfile::DelayedScriptElm::~DelayedScriptElm()
{
}

HTML_Element*
HLDocProfile::DelayedScriptElm::GetFirstParseAheadElement()
{
	HTML_Element* last_leaf = script_element->LastLeaf();
	return last_leaf ? last_leaf->Next() : script_element->Next();
}

static OP_STATUS
ESInsertElement(HLDocProfile *hld_profile, HTML_Element* element)
{
	OP_STATUS status = OpStatus::OK;

	if (element->GetInserted() == HE_INSERTED_BY_PARSE_AHEAD)
	{
		element->SetInserted(HE_NOT_INSERTED);

		if (element->IsMatchingType(Markup::HTE_IMG, NS_HTML))
		{
			if (element->GetSpecialBoolAttr(Markup::LOGA_JS_DELAYED_ONLOAD, SpecialNs::NS_LOGDOC))
			{
				element->SetSpecialBoolAttr(Markup::LOGA_JS_DELAYED_ONLOAD, FALSE, SpecialNs::NS_LOGDOC);
				if (HEListElm* helm = element->GetHEListElmForInline(IMAGE_INLINE))
					status = helm->SendImageFinishedLoadingEvent(hld_profile->GetFramesDocument());
			}
		}
		else
		{
			if (!OpStatus::IsMemoryError(status))
				if (element->GetSpecialBoolAttr(Markup::LOGA_JS_DELAYED_ONLOAD, SpecialNs::NS_LOGDOC))
				{
					status = hld_profile->GetFramesDocument()->HandleEvent(ONLOAD, NULL, element, SHIFTKEY_NONE);
					element->SetSpecialBoolAttr(Markup::LOGA_JS_DELAYED_ONLOAD, FALSE, SpecialNs::NS_LOGDOC);
				}

			if (!OpStatus::IsMemoryError(status))
				if (element->GetSpecialBoolAttr(Markup::LOGA_JS_DELAYED_ONERROR, SpecialNs::NS_LOGDOC))
				{
					status = hld_profile->GetFramesDocument()->HandleEvent(ONERROR, NULL, element, SHIFTKEY_NONE);
					element->SetSpecialBoolAttr(Markup::LOGA_JS_DELAYED_ONERROR, FALSE, SpecialNs::NS_LOGDOC);
				}
		}

		if (element->IsMatchingType(Markup::HTE_IFRAME, NS_HTML))
		{
			// We couldn't load iframes until now because of possible dependencies between
			// documents, but now is the time.
			if (OpStatus::IsMemoryError(hld_profile->HandleNewIFrameElementInTree(element)))
				status = OpStatus::ERR_NO_MEMORY;
			element->MarkExtraDirty(hld_profile->GetFramesDocument()); // There might be an empty IFrameContent to replace/repair
		}
#ifdef MEDIA_HTML_SUPPORT
		else if (element->IsMatchingType(HE_AUDIO, NS_HTML) ||
				 element->IsMatchingType(HE_VIDEO, NS_HTML) ||
				 element->IsMatchingType(HE_SOURCE, NS_HTML) ||
				 element->IsMatchingType(Markup::HTE_TRACK, NS_HTML))
		{
			status = InsertMediaElement(element);
		}
#endif // MEDIA_HTML_SUPPORT
#ifdef _PLUGIN_SUPPORT_
		else if (element->IsMatchingType(Markup::HTE_OBJECT, NS_HTML))
		{
			// A preparsed plugin doesn't work so it needs a kick to become initialized
			element->MarkExtraDirty(hld_profile->GetFramesDocument());
		}
#endif // _PLUGIN_SUPPORT_
		else if (element->IsMatchingType(Markup::HTE_META, NS_HTML))
		{
			const uni_char* http_equiv = element->GetStringAttr(Markup::HA_HTTP_EQUIV);
			if (http_equiv && uni_stri_eq(http_equiv, "REFRESH")
				&& (!hld_profile->GetFramesDocument()->GetWindow()->IsMailOrNewsfeedWindow()
					&& hld_profile->GetURL()->Type() != URL_ATTACHMENT))
			{
				const uni_char* content = element->GetStringAttr(Markup::HA_CONTENT);
				element->HandleMetaRefresh(hld_profile, content);
			}
		}
	}

	if (OpStatus::IsSuccess(status))
	{
		LogicalDocument *logdoc = hld_profile->GetFramesDocument()->GetLogicalDocument();
		status = logdoc->AddNamedElement(element, FALSE);
	}

	return status;
}

OP_STATUS
HLDocProfile::ESInsertElements(DelayedScriptElm* delayed_script, HTML_Element *from, HTML_Element *to)
{
	OP_STATUS status = OpStatus::OK;

	if (from)
		do
			if (from)
			{
				OP_STATUS event_status = ESInsertElement(this, from);

				if (OpStatus::IsError(event_status))
					status = event_status;
			}
			else
			{
				/* Not supposed to happen.  Let me know it does.  (jl@opera.com) */
				OP_ASSERT(FALSE);
				break;
			}
		while ((from = (HTML_Element *) from->Next()) != to);

	if (OpStatus::IsSuccess(status))
		status = ESInsertOutOfOrderElements(delayed_script);

	return OpStatus::IsMemoryError(status) ? status: OpStatus::OK;
}

void
HLDocProfile::ESStopLoading()
{
	es_delay_scripts = FALSE;
	es_is_parsing_ahead = FALSE;
	es_is_executing_delayed_script = FALSE;

	DelayedScriptElm* delayed_script = (DelayedScriptElm *)es_delayed_scripts.First();
	if (delayed_script)
	{
		if (frames_doc->IsUndisplaying() || frames_doc->GetDocManager()->IsClearing())
			// If we walk in history we need to reload from start because we
			// don't want to start inserting elements and potentially starting
			// loads right now.
			frames_doc->SetCompatibleHistoryNavigationNeeded();
		else
			OpStatus::Ignore(ESInsertElements(delayed_script, delayed_script->GetFirstParseAheadElement(), NULL));
	}

	es_delayed_scripts.Clear();
}

BOOL
HLDocProfile::ESIsFirstDelayedScript(HTML_Element *script) const
{
	DelayedScriptElm* delayed_script = static_cast<DelayedScriptElm*>(es_delayed_scripts.First());
	if (delayed_script && delayed_script->script_element.GetElm() == script)
		return TRUE;
	return FALSE;
}

OP_STATUS
HLDocProfile::ESAddScriptElement(HTML_Element *element, unsigned stream_position, BOOL is_ready)
{
	DelayedScriptElm *delayed_script;

	for (delayed_script = (DelayedScriptElm *) es_delayed_scripts.First(); delayed_script; delayed_script = (DelayedScriptElm *) delayed_script->Suc())
		if (delayed_script->script_element.GetElm() == element)
			return OpStatus::OK;

	delayed_script = OP_NEW(DelayedScriptElm, ());
	if (!delayed_script)
		return OpStatus::ERR_NO_MEMORY;

	delayed_script->script_element.SetElm(element);
	delayed_script->stream_position = stream_position;
	delayed_script->form_nr = GetCurrentFormNumber();
	delayed_script->form_was_closed = GetCurrentForm() == NULL;
	delayed_script->is_ready = is_ready;
	delayed_script->no_current_form = form.GetElm() == NULL;
	delayed_script->has_added_form = FALSE;
	delayed_script->has_completed = FALSE;
	delayed_script->has_content = hldoc->HasContent();
	delayed_script->has_restored_state = FALSE;

	delayed_script->stream_position = hldoc->GetLastBufferStartOffset();
	OP_STATUS stat = hldoc->StoreParserState(&delayed_script->parser_state);
	if (OpStatus::IsError(stat))
	{
		OP_DELETE(delayed_script);
		return stat;
	}

	delayed_script->Into(&es_delayed_scripts);
	es_is_parsing_ahead = TRUE;

#ifdef DEBUG_ENABLE_OPASSERT
	OP_ASSERT(!delayed_script->Pred() || ((DelayedScriptElm *) delayed_script->Pred())->script_element->Precedes(element));
#endif  // DEBUG_ENABLE_OPASSERT

	return OpStatus::OK;
}

OP_STATUS
HLDocProfile::ESAddFosterParentedElement(HTML_Element *element)
{
	DelayedScriptElm *delayed_script = static_cast<DelayedScriptElm*>(es_delayed_scripts.Last());
	DelayedScriptElm::OutOfOrderElement* ooo_elm = OP_NEW(DelayedScriptElm::OutOfOrderElement, (element));
	if (!ooo_elm)
		return OpStatus::ERR_NO_MEMORY;

	ooo_elm->Into(&delayed_script->out_of_order_parse_ahead_inserted_elements);

	return OpStatus::OK;
}

OP_STATUS
HLDocProfile::ESInsertOutOfOrderElements(DelayedScriptElm* delayed_script)
{
	while (DelayedScriptElm::OutOfOrderElement* ooo_elm = delayed_script->out_of_order_parse_ahead_inserted_elements.First())
	{
		if (ooo_elm->element.GetElm())
			RETURN_IF_ERROR(ESInsertElement(this, ooo_elm->element.GetElm()));

		ooo_elm->Out();
		OP_DELETE(ooo_elm);
	}

	return OpStatus::OK;
}

OP_STATUS
HLDocProfile::ESSetScriptElementReady(HTML_Element *element)
{
	for (DelayedScriptElm *delayed_script = (DelayedScriptElm *) es_delayed_scripts.First(); delayed_script; delayed_script = (DelayedScriptElm *) delayed_script->Suc())
		if (delayed_script->script_element.GetElm() == element)
		{
			if (!delayed_script->is_ready)
			{
				delayed_script->is_ready = TRUE;

				if (!es_delay_scripts && delayed_script == es_delayed_scripts.First())
					return ESStartDelayedScript();
			}

			return OpStatus::OK;
		}

	RETURN_IF_ERROR(element->LoadExternalScript(this));
	return OpStatus::OK;
}

OP_STATUS
HLDocProfile::ESCancelScriptElement(HTML_Element *element)
{
	for (DelayedScriptElm *delayed_script = (DelayedScriptElm *) es_delayed_scripts.First(); delayed_script; delayed_script = (DelayedScriptElm *) delayed_script->Suc())
		if (delayed_script->script_element.GetElm() == element)
		{
			DelayedScriptElm *prev_delayed_script = (DelayedScriptElm *) delayed_script->Pred(), *next_delayed_script = (DelayedScriptElm *) delayed_script->Suc();

			OP_STATUS status;

			delayed_script->Out();

			if (!es_delayed_scripts.First())
				es_is_parsing_ahead = FALSE;

			if (!prev_delayed_script)
			{
				status = ESInsertElements(delayed_script, delayed_script->GetFirstParseAheadElement(), next_delayed_script ? next_delayed_script->GetFirstParseAheadElement() : NULL);

				if (OpStatus::IsSuccess(status) && !es_delay_scripts && !es_is_starting_delayed_script)
				{
					es_is_executing_delayed_script = FALSE;
					status = ESStartDelayedScript();
				}
				else if (es_is_parsing_ahead && !es_delayed_scripts.First())
				{
					// We cancelled the only delayed script without parsing ahead
					// so no point pretending we're parsing ahead
					es_is_parsing_ahead = FALSE;
				}
			}
			else
				status = OpStatus::OK;

			OP_DELETE(delayed_script);

			return status;
		}

	return OpStatus::ERR;
}

static HTML_Element *
ESFindFirstElementToRemove(HTML_Element *from)
{
	HTML_Element *to_remove = from;
	while (to_remove)
		if (to_remove->GetInserted() == HE_INSERTED_BY_PARSE_AHEAD)
			break;
		else
			to_remove = (HTML_Element *) to_remove->Next();
	return to_remove;
}

OP_STATUS
HLDocProfile::ESElementRemoved(HTML_Element *element)
{
	for (DelayedScriptElm *delayed_script = (DelayedScriptElm *) es_delayed_scripts.First(); delayed_script; delayed_script = (DelayedScriptElm *) delayed_script->Suc())
		if (element->IsAncestorOf(delayed_script->script_element.GetElm()) || delayed_script->current_element.GetElm() && element->IsAncestorOf(delayed_script->current_element.GetElm()))
		{
			delayed_script = (DelayedScriptElm *) es_delayed_scripts.First();
			return ESRecover(delayed_script->current_element.GetElm() ? delayed_script->current_element.GetElm() : delayed_script->script_element.GetElm(), delayed_script->stream_position, delayed_script->form_nr, delayed_script->form_was_closed, delayed_script->no_current_form, delayed_script->has_content,
					delayed_script->script_element.GetElm(), &delayed_script->parser_state, delayed_script->has_completed);
		}
	return OpStatus::OK;
}

OP_STATUS
HLDocProfile::ESParseAheadFinished()
{
	if (es_is_parsing_ahead)
	{
		es_delay_scripts = FALSE;
		es_is_parsing_ahead = FALSE;
		return ESStartDelayedScript();
	}
	else
		return OpStatus::OK;
}

OP_STATUS
HLDocProfile::ESStartDelayedScript()
{
	OP_STATUS status = OpStatus::OK;
	OpStatus::Ignore(status);

	if (!es_is_executing_delayed_script)
	{
		while (DelayedScriptElm *delayed_script = (DelayedScriptElm *) es_delayed_scripts.First())
		{
			if (!delayed_script->is_ready)
			{
				status = OpStatus::OK;
				goto exit_function;
			}

			OP_ASSERT(delayed_script->script_element->GetInserted() != HE_INSERTED_BY_PARSE_AHEAD); // The script element we're trying to run should have been inserted by now

			es_is_executing_delayed_script = TRUE;
			es_is_starting_delayed_script = TRUE;

			URL* url = delayed_script->script_element->GetScriptURL(*hldoc->GetURL(), hldoc->GetFramesDocument()->GetLogicalDocument());
			BOOL is_external = url && url->Type() != URL_NULL_TYPE;

			if (OpStatus::IsMemoryError(es_load_manager.RegisterScript(delayed_script->script_element.GetElm(), is_external, GetLogicalDocument()->GetRoot()->IsAncestorOf(delayed_script->script_element.GetElm()), NULL)))
			{
				status = OpStatus::ERR_NO_MEMORY;
				goto exit_function;
			}

			OP_BOOLEAN result;

			if (is_external)
				result = delayed_script->script_element->FetchExternalScript(this, NULL, NULL, FALSE);
			else
				result = delayed_script->script_element->LoadScript(this);

			OP_ASSERT(result != OpStatus::OK);
			if (result == OpBoolean::IS_TRUE)
			{
				delayed_script->current_element.SetElm(delayed_script->script_element->ParentActual());

				// Set form state in the parser
				if (delayed_script->no_current_form)
				{
					// So that we don't think we've added a form when the script is finished
					form.Reset();
				}
				else
				{
					OP_ASSERT(delayed_script->form_nr >= 0);
					form_count = delayed_script->form_nr;
					HTML_Element *iter = delayed_script->script_element.GetElm();
					while (iter && !(iter->IsMatchingType(HE_FORM, NS_HTML) && iter->GetFormNr() == form_count))
						iter = iter->PrevActual();
					if (iter)
						SetNewForm(iter);
					else
						form.Reset(); // Bah, the form isn't there
				}

				es_is_starting_delayed_script = FALSE;
				return OpStatus::OK;
			}
			else if (result != OpBoolean::IS_FALSE && result != OpStatus::ERR)
			{
				status = result;
				goto exit_function;
			}
			else
			{
				OP_ASSERT(es_delayed_scripts.First() != delayed_script); // Or we will have an eternal loop
			}
		}

		es_is_executing_delayed_script = FALSE;
		es_is_starting_delayed_script = FALSE;

		if (!frames_doc->GetHtmlDocument() && !frames_doc->GetFrmDocRoot())
			status = frames_doc->CheckInternal();

		frames_doc->GetMessageHandler()->PostMessage(MSG_URL_DATA_LOADED, frames_doc->GetURL().Id(TRUE), 0);
	}

	return status;

exit_function:
	es_is_executing_delayed_script = FALSE;
	es_is_starting_delayed_script = FALSE;

	if (OpStatus::IsError(status))
	{
		if (DelayedScriptElm *delayed_script = (DelayedScriptElm *) es_delayed_scripts.First())
		{
			if (OpStatus::IsMemoryError(ESInsertElements(delayed_script, delayed_script->GetFirstParseAheadElement(), NULL)))
				status = OpStatus::ERR_NO_MEMORY;
		}

		es_delay_scripts = FALSE;
		es_is_executing_delayed_script = FALSE;
		es_delayed_scripts.Clear();

		if (frames_doc->GetHtmlDocument() && frames_doc->GetFrmDocRoot())
			OpStatus::Ignore(frames_doc->CheckInternal());

		frames_doc->GetMessageHandler()->PostMessage(MSG_URL_DATA_LOADED, frames_doc->GetURL().Id(TRUE), 0);
	}

	return status;
}

OP_STATUS
HLDocProfile::ESFinishedDelayedScript()
{
	if (es_is_executing_delayed_script)
	{
		es_is_executing_delayed_script = FALSE;

		DelayedScriptElm *delayed_script = (DelayedScriptElm *) es_delayed_scripts.First();

		DelayedScriptElm *next_delayed_script = (DelayedScriptElm *) delayed_script->Suc();

		HTML_Element *script = delayed_script->script_element.GetElm();
		HTML_Element *current = delayed_script->current_element.GetElm();
		HTML_Element *to_remove = ESFindFirstElementToRemove(script);
		HTML_Element *from = delayed_script->GetFirstParseAheadElement();
		HTML_Element *stop = next_delayed_script ? next_delayed_script->GetFirstParseAheadElement() : NULL;
		unsigned stream_position = delayed_script->stream_position;

		int form_nr = form_count;
		BOOL form_was_closed = form.GetElm() == NULL;
		BOOL no_current_form = form.GetElm() == NULL;
		BOOL has_content = delayed_script->has_content;
		BOOL need_recover = es_need_recover;

		if (!need_recover && delayed_script->has_restored_state && hldoc->HasParserStateChanged(&delayed_script->parser_state))
			need_recover = TRUE;

		if (delayed_script->has_added_form)
		{
			HTML_Element *iter = to_remove;
			while (!need_recover && iter)
			{
				if (iter->GetNsType() == NS_HTML)
				switch (iter->Type())
				{
					case HE_FORM:
					case HE_INPUT:
					case HE_SELECT:
					case HE_TEXTAREA:
					case HE_KEYGEN:
					need_recover = TRUE;
					break;
				}

				iter = (HTML_Element *) iter->Next();
			}

			if (form.GetElm())
				form_nr = form->GetFormNr();
			no_current_form = form.GetElm() == NULL;

			if (!need_recover && next_delayed_script)
			{
				DelayedScriptElm *iter = next_delayed_script;
				while (iter)
				{
					// FIXME: Is this really right? Shouldn't we rather offset the form_nr with the number
					// of added forms, and I'm pretty sure all pending DelayedScriptElms shouldn't
					// have the same form_nr. There can be forms in between them.
					iter->form_nr = form_nr;
					iter->no_current_form = no_current_form;
					iter = (DelayedScriptElm *) iter->Suc();
				}
			}
		}

		if (need_recover || current && current != script->ParentActual())
		{
			/* Some things do not require a full recovery, and on a small device we
			   might want to just go on anyway, even if the end result is a little
			   wrong. */
			return ESRecover(current, stream_position, form_nr, form_was_closed, no_current_form, has_content,
					delayed_script->script_element.GetElm(), &delayed_script->parser_state, delayed_script->has_completed);
		}

		OP_STATUS status = OpStatus::OK;

		if (OpStatus::IsMemoryError(ESInsertElements(delayed_script, from, stop)))
			status = OpStatus::ERR_NO_MEMORY;

		delayed_script->Out();
		OP_DELETE(delayed_script);

		if (OpStatus::IsMemoryError(ESStartDelayedScript()))
			status = OpStatus::ERR_NO_MEMORY;

		return status;
	}

	return OpStatus::OK;
}

static BOOL
IsInsertedByParseAhead(HTML_Element *element)
{
	switch (element->GetInserted())
	{
	case HE_INSERTED_BY_PARSE_AHEAD:
		return TRUE;

	default:
		return FALSE;

	case HE_INSERTED_BY_LAYOUT:
		// Inserted by the parser/layout. Because of Parse Ahead or not?
		// Check all children, if they're inserted by parse ahead,
		// then we know.
		for (HTML_Element *iter = element->FirstChild(); iter; iter = iter->Suc())
			if (!IsInsertedByParseAhead(iter))
				return FALSE;
		return TRUE;
	}
}

/// goes through the subtree and removes those marked with PARSE_AHEAD, but keep the rest
static void RemoveParseAheadElements(HTML_Element* element, FramesDocument* frames_doc)
{
	// Go through children first, so that we correct the subtrees before destroying the parent
	HTML_Element* child = element->FirstChild();
	while (child)
	{
		HTML_Element* next_child = child->Suc();
		RemoveParseAheadElements(child, frames_doc);
		child = next_child;
	}

	// Remove the element itself
	if (IsInsertedByParseAhead(element))
	{
		if (element->Type() != Markup::HTE_TEXT)  // text nodes don't have children
			element->Parent()->InsertChildrenBefore(element, element->Suc());

		if (element->OutSafe(frames_doc))
			element->Free(frames_doc);
	}
}

OP_STATUS
HLDocProfile::ESRecover(HTML_Element *last_element, unsigned stream_position, int form_nr, BOOL form_was_closed, BOOL no_current_form, BOOL new_has_content,
		HTML_Element *script_element, HTML5ParserState* parser_state, BOOL has_completed)
{
#ifdef DOM_DSE_DEBUGGING
	hldoc->DSESetRecovered();
#endif // DOM_DSE_DEBUGGING
	op_yield();

	DelayedScriptElm *delayed_script = static_cast<DelayedScriptElm *>(es_delayed_scripts.First());
	if (!delayed_script->has_restored_state)
	{
		hldoc->RestoreParserState(&delayed_script->parser_state, delayed_script->script_element.GetElm(), delayed_script->stream_position, delayed_script->has_completed);
		delayed_script->has_restored_state = TRUE;
	}

	OP_STATUS status = frames_doc->SetStreamPosition(stream_position);
	GetLogicalDocument()->GetParser()->Recover();

	if (OpStatus::IsError(status))
	{
		ESStopLoading();

		ES_ThreadScheduler* scheduler = frames_doc->GetESScheduler();
		if (scheduler && scheduler->IsActive())
		{
			ES_Thread *thread = scheduler->GetCurrentThread();
			if (OpStatus::IsMemoryError(scheduler->CancelThread(thread)))
				status = OpStatus::ERR_NO_MEMORY;
		}

		return status;
	}

	// Figure out what the form pointer and form count should be.
	// This part shouldn't really be necessary since the form and
	// form_count will already be right when this function is called, but
	// removing it is somewhat risky and will need much testing.
	form_count = form_nr;
	form.Reset();

	HTML_Element *iter;

	if (!no_current_form && !form_was_closed)
	{
		iter = last_element;
		while (iter)
		{
			if (iter->IsMatchingType(HE_FORM, NS_HTML) && iter->GetFormNr() != -1)
			{
				OP_ASSERT(form_count == iter->GetFormNr());
				form.SetElm(iter);
				break;
			}

			iter = (HTML_Element *) iter->Prev();
		}
	}
	// end form_count/form setting. See comment above about this block


	// TODO: Should reset the EndTagFound flag at elements that we're parsing inside.

	BOOL had_body_before = body_he.GetElm() != NULL && body_he->IsMatchingType(HE_BODY, NS_HTML);
	BOOL removed_frameset = FALSE;
	// Because of foster parenting with HTML 5, parse ahead elements may have
	// been inserted earlier in the tree too, so we need to go through from the root
	// TODO: may be optimized some, there are limits to how much earlier elements
	// may have been inserted.
	iter = GetLogicalDocument()->GetDocRoot();

	while (iter)
	{
		HTML_Element *next;

		if (IsInsertedByParseAhead(iter))
		{
			if (iter->IsMatchingType(HE_FRAMESET, NS_HTML))
				removed_frameset = TRUE;

			next = iter->NextSibling();

			iter->Parent()->MarkExtraDirty(frames_doc);

			if (iter->OutSafe(frames_doc))
				iter->Free(frames_doc);
		}
		else
		{
			HTML5DSE_RestoreAttr* restore_point = static_cast<HTML5DSE_RestoreAttr*>(iter->GetSpecialAttr(ATTR_DSE_RESTORE_PARENT, ITEM_TYPE_COMPLEX, (void*)NULL, SpecialNs::NS_LOGDOC));
			if (restore_point)  // Need to move this element to where it was before we parsed ahead.
			{
				next = iter->NextSibling();
				BOOL can_free = iter->OutSafe(frames_doc, FALSE);
				HTML_Element* parent = restore_point->GetParent();
				// The attribute has served its purpose, we can remove it.
				iter->RemoveSpecialAttribute(ATTR_DSE_RESTORE_PARENT, SpecialNs::NS_LOGDOC);

				if (parent)
				{
					iter->UnderSafe(frames_doc, parent);

					// Remove any elements in the sub tree inserted by parse ahead.
					RemoveParseAheadElements(iter, frames_doc);
				}
				else  // If parent doesn't exist anymore, then this element has no reason to exist anymore either.
					if (can_free && iter->Clean(frames_doc, TRUE))
						iter->Free(frames_doc);
			}
			else
				next = iter->Next();
		}

		iter = next;
	}

	es_need_recover = FALSE;
	es_delay_scripts = FALSE;
	es_is_executing_delayed_script = FALSE;
	es_delayed_scripts.Clear();

	hldoc->SetHasContent(new_has_content);

	BOOL has_body_now = body_he.GetElm() != NULL && body_he->IsMatchingType(HE_BODY, NS_HTML);
	if (had_body_before && !has_body_now || removed_frameset && frames_doc->IsFrameDoc())
		// We removed the body element.  Could be a script wrote a frameset before some
		// normally ignored content that made us insert a BODY element (or before an actual
		// BODY starttag, for that matter).  Must make sure FramesDocument::CheckInternal
		// picks up that frameset if so, by resetting FramesDocument::doc.
	{
		frames_doc->CheckInternalReset();
	}

	if (removed_frameset)
		is_frames_doc = FALSE;

	frames_doc->GetMessageHandler()->PostMessage(MSG_URL_DATA_LOADED, frames_doc->GetURL().Id(TRUE), 0);

	return OpStatus::OK;
}

void
HLDocProfile::ESSetNeedRecover()
{
	es_need_recover = TRUE;
}

void
HLDocProfile::ESFlushDelayed()
{
	// So a script document.wrote content into the middle of the document and we have to
	// start parsing that HTML as if it had been written during a normal execution.

	// That involves removing (some) preparsed content after this script and restoring
	// the parser state as it was when the scipt tag was encountered.
	DelayedScriptElm *delayed_script = (DelayedScriptElm *) es_delayed_scripts.First();

	if (delayed_script->script_element.GetElm())
	{
		HTML_Element *parent = delayed_script->script_element->ParentActual();

		parent->MarkExtraDirty(GetFramesDocument());
	}

	int old_form_count = form_count;
	form_count = delayed_script->form_nr;

	if (!delayed_script->has_restored_state)
	{
		hldoc->RestoreParserState(&delayed_script->parser_state, delayed_script->script_element.GetElm(), delayed_script->stream_position, delayed_script->has_completed);
		delayed_script->has_restored_state = TRUE;
	}

	form_count = old_form_count;
}

void
HLDocProfile::ESRecoverIfNeeded()
{
	if (ESIsExecutingDelayedScript())
	{
		DelayedScriptElm *delayed_script = static_cast<DelayedScriptElm *>(es_delayed_scripts.First());

		if (es_need_recover)
		{
			HTML_Element *current = delayed_script->current_element.GetElm() ? delayed_script->current_element.GetElm() : delayed_script->script_element.GetElm();
			if (OpStatus::IsMemoryError(ESRecover(current, delayed_script->stream_position, delayed_script->form_nr, delayed_script->form_was_closed, delayed_script->no_current_form, delayed_script->has_content,
				delayed_script->script_element.GetElm(), &delayed_script->parser_state, delayed_script->has_completed)))
				SetIsOutOfMemory(TRUE);
		}
	}
}
#endif // DELAYED_SCRIPT_EXECUTION

OP_STATUS
HLDocProfile::HandleNewIFrameElementInTree(HTML_Element* new_elm, ES_Thread* origin_thread)
{
	OP_ASSERT(new_elm);
	OP_ASSERT(new_elm->IsMatchingType(HE_IFRAME, NS_HTML));

	if (!IsFramesDoc() && frames_doc->IsCurrentDoc() && frames_doc->GetLogicalDocument()->GetRoot()->IsAncestorOf(new_elm))
	{
		BOOL load_frame = frames_doc->IsAllowedIFrame(new_elm->GetUrlAttr(ATTR_SRC, NS_IDX_HTML, hldoc));

		if (load_frame)
		{
			int width = new_elm->HasNumAttr(ATTR_WIDTH) ? (int)new_elm->GetNumAttr(ATTR_WIDTH) : 300;
			long height = new_elm->HasNumAttr(ATTR_HEIGHT) ? new_elm->GetNumAttr(ATTR_HEIGHT) : 150;

			// Start loading the iframe
			FramesDocElm* dummy;
			RETURN_IF_ERROR(frames_doc->GetNewIFrame(dummy, width, height, new_elm, NULL, load_frame, FALSE, origin_thread));
			hldoc->SetHasContent(TRUE);
		}
	}
	return OpStatus::OK;
}

static void
SetFormNumberAndCurrentForm(HLDocProfile *hld_profile, HTML_Element *element)
{
	// Set ATTR_JS_ELM_IDX on FORM, AKA "this element is FORM number seven in the document".
	// Also sets HLDocProfile::form, AKA HLDocProfile::GetCurrentForm, if there isn't one already.

	if (element->GetInserted() != HE_INSERTED_BY_LAYOUT)
	{
		// Remember the last used FormElmNumber and save as an
		// attribute in the current form

		HTML_Element* prev_form = hld_profile->GetCurrentForm();
		if (prev_form)
		{
			int current_elm_no = hld_profile->GetCurrentFormElmNumber();
			if (current_elm_no > 0)
				prev_form->SetSpecialNumAttr(ATTR_HIGHEST_USED_FORM_ELM_NO, current_elm_no, SpecialNs::NS_LOGDOC);
		}

		if (!hld_profile->GetCurrentForm())
		{
			int formnr = hld_profile->GetNewFormNumber();

			element->SetSpecialNumAttr(ATTR_JS_ELM_IDX, formnr, SpecialNs::NS_LOGDOC);

			hld_profile->SetNewForm(element);
		}
	}
}

static void
SetElementNumberWithinForm(HLDocProfile *hld_profile, HTML_Element *element)
{
	// Set ATTR_JS_ELM_IDX, AKA "this element is the seventh form element inside its FORM".

	int elm_number = hld_profile->GetNewFormElmNumber();
	element->SetSpecialNumAttr(ATTR_JS_ELM_IDX, elm_number, SpecialNs::NS_LOGDOC);
}

static void
SetElementFormNumber(HLDocProfile *hld_profile, HTML_Element *element)
{
	// Set ATTR_JS_FORM_IDX, AKA "this element belongs to FORM number seven in the document".

	int form_number;
	if (element->GetInserted() == HE_INSERTED_BY_DOM || !hld_profile->GetCurrentForm())
		form_number = -1;
	else
		form_number = hld_profile->GetCurrentFormNumber();

	element->SetSpecialNumAttr(ATTR_JS_FORM_IDX, form_number, SpecialNs::NS_LOGDOC);
}

OP_HEP_STATUS
HLDocProfile::InsertElement(HTML_Element *parent, HTML_Element *new_elm)
{
	OP_ASSERT(!(new_elm && new_elm->IsText()) || !"Add text with InsertText or InsertTextElement");
	return InsertElementInternal(parent, new_elm, NULL, 0, FALSE, FALSE, FALSE, NULL);
}

OP_HEP_STATUS
HLDocProfile::InsertElementBefore(HTML_Element *before, HTML_Element *new_elm)
{
	OP_ASSERT(!(new_elm && new_elm->IsText()) || !"Add text with InsertText or InsertTextElement");
	HTML_Element* parent = before->Parent();
	return InsertElementInternal(parent, new_elm, NULL, 0, FALSE, FALSE, FALSE, NULL, before);
}

OP_HEP_STATUS
HLDocProfile::InsertTextElement(HTML_Element *parent, HTML_Element *new_elm)
{
	OP_ASSERT(new_elm && new_elm->IsText() || !"Add normal elements with InsertElement");
	return InsertElementInternal(parent, new_elm, NULL, 0, FALSE, FALSE, FALSE, NULL);
}

OP_HEP_STATUS
HLDocProfile::InsertTextElementBefore(HTML_Element *before, HTML_Element *new_elm)
{
	OP_ASSERT(new_elm && new_elm->IsText() || !"Add normal elements with InsertElement");
	HTML_Element* parent = before->Parent();
	return InsertElementInternal(parent, new_elm, NULL, 0, FALSE, FALSE, FALSE, NULL, before);
}

OP_HEP_STATUS
HLDocProfile::InsertText(HTML_Element *parent, const uni_char* text, unsigned text_len, BOOL resolve_entites, BOOL as_cdata, BOOL expand_wml_vars, HTML_Element*& modified_text_elm)
{
	return InsertElementInternal(parent, NULL, text, text_len, resolve_entites, as_cdata, expand_wml_vars, &modified_text_elm);
}


OP_HEP_STATUS
HLDocProfile::InsertElementInternal(HTML_Element *parent, HTML_Element *new_elm, const uni_char* text, unsigned text_len, BOOL resolve_entities, BOOL as_cdata, BOOL expand_wml_vars, HTML_Element** modified_text_elm, HTML_Element* insert_before)
{
	OP_PROBE6(OP_PROBE_HLDOCPROFILE_INSERTELEMENT);

	if (!parent || !(new_elm || text))
        return OpStatus::ERR;

	HTML_Element* last_child = parent->LastChild();

	/* An :after pseudo element might already have been inserted.
	   Ensure that it is kept last in list of children. */

	if (!insert_before && last_child && last_child->GetIsAfterPseudoElement())
		insert_before = last_child;

	OP_HEP_STATUS status = OpStatus::OK;
	OpStatus::Ignore(status);

	if (text)
	{
		OP_ASSERT(modified_text_elm);

		new_elm = parent->CreateText(this, text, text_len, resolve_entities, as_cdata, expand_wml_vars);
		if (!new_elm)
			return OpStatus::ERR_NO_MEMORY;

		*modified_text_elm = new_elm;
	}

#ifdef DELAYED_SCRIPT_EXECUTION
	if (es_is_executing_delayed_script)
	{
		HTML_Element *iter = insert_before ? insert_before->Pred() : last_child;

		// If the last elements were inserted by parse ahead, or by the parser
		// in reponse to a parse ahead, we need to insert the new element before them
		while (iter && IsInsertedByParseAhead(iter))
		{
			insert_before = iter;
			iter = iter->Pred();
		}
	}
	else if (es_is_parsing_ahead && !IsXml() && new_elm)
	{
		HTML_Element *iter = new_elm, *stop = static_cast<HTML_Element *>(new_elm->NextSibling());
		do
			if (iter->GetInserted() == HE_NOT_INSERTED)
				iter->SetInserted(HE_INSERTED_BY_PARSE_AHEAD);
		while ((iter = iter->Next()) != stop);
	}
#endif // DELAYED_SCRIPT_EXECUTION

	if (!new_elm->Parent())
		if (insert_before)
			new_elm->Precede(insert_before);
		else
			new_elm->Under(parent);

	BOOL in_tree = frames_doc->GetLogicalDocument()->GetRoot()->IsAncestorOf(new_elm);

	if (!in_tree)
		/* Most of this function expects to be called when inserting an element
		 * to the tree but it's not always the case (see CORE-42477) so if not
		 * inserting to the tree just return here.
		 * ElementSignalInserted() in htm_elm.cpp is supposed to take care of
		 * everything what's needed upon insertion of a subtree with 'new_elm'
		 * to the tree.
		 */
		return is_out_of_memory ? OpStatus::ERR_NO_MEMORY : OpStatus::OK;

#ifdef FORMAT_UNSTYLED_XML
	if (IsXml() && Markup::IsRealElement(new_elm->Type()) && new_elm->GetNsType() != NS_USER)
		hldoc->SetXmlStyled();
#endif // FORMAT_UNSTYLED_XML

	if (is_parsing_inner_html)
		return is_out_of_memory ? OpStatus::ERR_NO_MEMORY : OpStatus::OK;

	if (!new_elm->HasNoLayout())
	{
		new_elm->MarkExtraDirty(frames_doc);
		new_elm->MarkPropsDirty(frames_doc);
	}
	else
		new_elm->MarkPropsClean();

	if (parent == hldoc->GetRoot())
	{
		if (Markup::IsRealElement(new_elm->Type()) && parent == hldoc->GetRoot())
		{
			hldoc->SetDocRoot(new_elm);
#ifdef SVG_SUPPORT
			/**
			* Quirks lineheight mode for case where we have svg as root, to avoid getting vertical scrollbar.
			*/
			if(new_elm->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
			{
				hldoc->SetIsInQuirksLineHeightMode();
			}
#endif // SVG_SUPPORT
			DOM_Environment *environment = frames_doc->GetDOMEnvironment();

			if (!environment)
				if (OpStatus::IsMemoryError(frames_doc->CreateESEnvironment(FALSE, TRUE)))
					status = OpStatus::ERR_NO_MEMORY;
				else
					environment = frames_doc->GetDOMEnvironment();

			if (environment)
				environment->NewRootElement(new_elm);
		}
	}
	else if (!handheld_callback_called && GetFramesDocument()->GetParentDoc() == NULL)
	{
		WindowCommander *win_com = GetFramesDocument()->GetWindow()->GetWindowCommander();
		OpDocumentListener* docListener = win_com->GetDocumentListener();

		docListener->OnHandheldChanged(win_com, FALSE);
		handheld_callback_called = TRUE;
	}

#ifdef RSS_SUPPORT
	if (parent == hldoc->GetRoot()
		|| (parent->IsMatchingType(HE_HTML, NS_HTML))
		|| (parent->IsMatchingType(HE_BODY, NS_HTML)))
	{
		const uni_char *tag = new_elm->GetTagName();
		if (tag)
		{
			if (uni_stricmp(tag, "RSS") == 0
				|| uni_stricmp(tag, "RDF") == 0
				|| (!IsXml()
					&& uni_stricmp(tag, "RDF:RDF") == 0))
				hldoc->SetRssStatus(LogicalDocument::MAYBE_IS_RSS);
			else if (uni_stricmp(tag, "FEED") == 0 &&
					 (new_elm->GetNsType() == NS_ATOM03 || new_elm->GetNsType() == NS_ATOM10))
				hldoc->SetRssStatus(LogicalDocument::IS_RSS);
		}
	}
	else if (hldoc->GetRssStatus() == LogicalDocument::MAYBE_IS_RSS)
	{
		const uni_char *tag = new_elm->GetTagName();
		if (tag && uni_stricmp(tag, "CHANNEL") == 0)
			hldoc->SetRssStatus(LogicalDocument::IS_RSS);
	}
#endif // RSS_SUPPORT

	RETURN_IF_ERROR(hldoc->AddNamedElement(new_elm, FALSE));

	if (DOM_Environment *environment = frames_doc->GetDOMEnvironment())
		RETURN_IF_ERROR(environment->ElementInserted(new_elm));

#ifdef XML_EVENTS_SUPPORT
	if (frames_doc->HasXMLEvents())
	{
		if (Markup::IsRealElement(new_elm->Type()) && new_elm->HasXMLEventAttribute())
			new_elm->HandleInsertedElementWithXMLEvent(frames_doc);

		for (XML_Events_Registration *registration = frames_doc->GetFirstXMLEventsRegistration();
			 registration;
			 registration = (XML_Events_Registration *) registration->Suc())
			RETURN_IF_ERROR(registration->HandleElementInsertedIntoDocument(frames_doc, new_elm));
	}
#endif // XML_EVENTS_SUPPORT

	if (new_elm->IsText())
	{
		BOOL is_title;
		BOOL is_visible = IsTextElementVisible(new_elm, &is_title);
		if (is_visible || is_title)
		{
			HTML_Element* stop = (insert_before ? insert_before : new_elm->NextSibling());

			/* The code below is prepared for handling both HTE_TEXT and HTE_TEXTGROUP.
			 Note however that it may mutate HTE_TEXT into HTE_TEXTGROUP. What's more
			 if the mutated HTE_TEXT was the child of HTE_TEXTGROUP the new HTE_TEXTGROUP will
			 be the child of the same HTE_TEXTGROUP as well.
			 */
#ifdef PHONE2LINK_SUPPORT
			if (is_visible)
			{
# if defined SVG_SUPPORT && defined XLINK_SUPPORT
				HTML_Element *parent_candidate = new_elm->Parent();
				while (parent_candidate->IsText())
					parent_candidate = parent_candidate->Parent();

				hldoc->ScanForTextLinks(new_elm, stop, parent_candidate->GetNsType() == NS_SVG);
# else // SVG_SUPPORT && XLINK_SUPPORT
				hldoc->ScanForTextLinks(new_elm, stop, FALSE);
# endif // SVG_SUPPORT && XLINK_SUPPORT
			}
#endif // PHONE2LINK_SUPPORT

#ifdef VISITED_PAGES_SEARCH
			hldoc->IndexText(new_elm, stop);
#endif // VISITED_PAGES_SEARCH

			HTML_Element* iter = new_elm;
			do
			{
				TextData* td = iter->GetTextData();

				if (td && !WhiteSpaceOnly(td->GetText(), td->GetTextLen()))
				{
					text_count += td->GetTextLen();

					if (is_visible)
					{
						hldoc->SetHasContent(TRUE);

#ifdef DISPLAY_WRITINGSYSTEM_HEURISTIC
						// analyze text to detect writing system
						AnalyzeText(td->GetText(), td->GetTextLen());
#endif // DISPLAY_WRITINGSYSTEM_HEURISTIC
					}
				}
				iter = iter->Next();
			}
			while (iter && iter != stop);
		}
	}
	else switch (new_elm->GetNsType())
	{
	case NS_HTML:
	{
		switch (new_elm->Type())
		{
		case HE_HTML:
		{
			if (!hldoc->GetDocRoot())
			{
				hldoc->SetDocRoot(new_elm);

				if (DOM_Environment *environment = frames_doc->GetDOMEnvironment())
					environment->NewRootElement(new_elm);
			}
#ifdef _WML_SUPPORT_
			if (HasWmlContent()
			    && (new_elm->HasAttr(WA_ONTIMER, NS_IDX_WML)
			        || new_elm->HasAttr(WA_ONENTERFORWARD, NS_IDX_WML)
			        || new_elm->HasAttr(WA_ONENTERBACKWARD, NS_IDX_WML)))
			{
				if (OpStatus::IsMemoryError(new_elm->WMLInit(GetFramesDocument()->GetDocManager())))
				{
					SetIsOutOfMemory(TRUE);
					status = OpStatus::ERR_NO_MEMORY;
				}
			}
#endif // _WML_SUPPORT_
#ifdef FONTSWITCHING
			WritingSystem::Script script =
				WritingSystem::FromLanguageCode(new_elm->GetCurrentLanguage());
			if (script != WritingSystem::Unknown)
			{
				SetPreferredScript(script);
				use_text_heuristic = FALSE;
			}
#endif // FONTSWITCHING
		}
		break;

		case HE_BODY:
		{
			// First body the HTML parser gives us is the body element, or the
			// first body that is the child of docroot in case of XML/XHTML.
			if (!GetBodyElm() && (!IsXml() || parent == GetDocRoot()))
			{
				OP_ASSERT(parent == GetDocRoot() || !"The official body element is always the child of the html element");
				SetBodyElm(new_elm);
#ifdef SHORTCUT_ICON_SUPPORT
				GetFramesDocument()->DoASpeculativeFavIconLoad();
#endif // SHORTCUT_ICON_SUPPORT
			}

#ifdef _WML_SUPPORT_
			if (HasWmlContent()
			    && (new_elm->HasAttr(WA_ONTIMER, NS_IDX_WML)
			        || new_elm->HasAttr(WA_ONENTERFORWARD, NS_IDX_WML)
			        || new_elm->HasAttr(WA_ONENTERBACKWARD, NS_IDX_WML)))
			{
				if (OpStatus::IsMemoryError(new_elm->WMLInit(GetFramesDocument()->GetDocManager())))
				{
					SetIsOutOfMemory(TRUE);
					status = OpStatus::ERR_NO_MEMORY;
				}
			}
#endif // _WML_SUPPORT_
		}
		break;

		case HE_BASE:
		{
			if (URL *baseurl = new_elm->GetUrlAttr(ATTR_HREF, NS_IDX_HTML, hldoc))
				SetBaseURL(baseurl, new_elm->GetStringAttr(ATTR_HREF, NS_IDX_HTML));
		}
		break;

		case HE_META:
		{
			HTML_Element::DocumentContext ctx(frames_doc);
			if (!new_elm->CheckMetaElement(ctx, parent, TRUE))
			{
				if (GetIsOutOfMemory())
				{
					status = OpStatus::ERR_NO_MEMORY;
					break;
				}
				frames_doc->GetDocManager()->SetRestartParsing(TRUE);
				status = HEParseStatus::STOPPED;
			}
			else
			{
				if (GetIsOutOfMemory())
				{
					status = OpStatus::ERR_NO_MEMORY;
					break;
				}
			}
		}
		break;

		case HE_PROCINST:
		case HE_LINK:
			if (new_elm->IsLinkElement() && OpStatus::IsMemoryError(HandleLink(new_elm)))
			{
				SetIsOutOfMemory(TRUE);
				status = OpStatus::ERR_NO_MEMORY;
			}
			break;

		case HE_ISINDEX:
			new_elm->MakeIsindex(this, new_elm->GetISINDEX_Prompt());

			if (GetIsOutOfMemory())
				status = OpStatus::ERR_NO_MEMORY;

			break;

		case HE_FRAMESET:
#ifdef DELAYED_SCRIPT_EXECUTION
			if (es_is_executing_delayed_script)
			{
				// If the frameset is before a body tag, then
				// it's a framesdoc, otherwise
				// it isn't. Bugs 171062 and 220270.
				HTML_Element* body = GetBodyElm();
				if (!body || body->GetInserted() == HE_INSERTED_BY_PARSE_AHEAD)
				{
					SetIsFramesDoc();

					es_need_recover = TRUE;
					break;
				}
			}
#endif // DELAYED_SCRIPT_EXECUTION
			if (!hldoc->HasContent()
				&& g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::FramesEnabled, *GetURL())
				&& parent->Type() != HE_FRAMESET)
			{
				SetIsFramesDoc();
				if (!GetBodyElm())
					SetBodyElm(new_elm);
			}
			/* fall through */

		case HE_FRAME:
			if (FramesDocElm *fde = frames_doc->GetFrmDocRoot())
			{
				while (fde && fde->GetHtmlElement())
					fde = (FramesDocElm *) fde->Next();

				if (fde)
					fde->SetHtmlElement(new_elm);
			}
			break;

		case HE_IFRAME:
#ifdef DELAYED_SCRIPT_EXECUTION
			if (!new_elm->IsIncludedActual())
				break; // done in ESInsertElement instead
#endif // DELAYED_SCRIPT_EXECUTION
			if (OpStatus::IsMemoryError(HandleNewIFrameElementInTree(new_elm)))
			{
				SetIsOutOfMemory(TRUE);
				status = OpStatus::ERR_NO_MEMORY;
			}
			break;

#ifdef _WML_SUPPORT_
		case HE_OPTION:
			if (HasWmlContent() && new_elm->HasAttr(WA_ONPICK, NS_IDX_WML))
			{
				if (OpStatus::IsMemoryError(new_elm->WMLInit(GetFramesDocument()->GetDocManager())))
				{
					SetIsOutOfMemory(TRUE);
					status = OpStatus::ERR_NO_MEMORY;
				}
			}
			break;
#endif //_WML_SUPPORT_

		case HE_FORM:
			SetFormNumberAndCurrentForm(this, new_elm);

			if (new_elm->GetId())
			{
				// Might take ownership of previously orphaned radio buttons with form attributes.
				DocumentRadioGroups& groups = GetLogicalDocument()->GetRadioGroups();
				if (OpStatus::IsError(groups.AdoptOrphanedRadioButtons(GetFramesDocument(), new_elm)))
					SetIsOutOfMemory(TRUE);
			}
			break;

		case HE_INPUT:
			if (new_elm->GetInputType() == INPUT_RADIO)
			{
				DocumentRadioGroups& groups = GetLogicalDocument()->GetRadioGroups();
				HTML_Element* form_element = GetCurrentForm();
				status = groups.RegisterRadioAndUncheckOthers(GetFramesDocument(), form_element, new_elm);
				if (OpStatus::IsError(status))
					SetIsOutOfMemory(TRUE);
			}

#ifdef LOGDOC_LOAD_IMAGES_FROM_PARSER
			else if (new_elm->GetInputType() == INPUT_IMAGE)
			{
				URL inline_url = new_elm->GetImageURL(FALSE, GetLogicalDocument());

				OP_LOAD_INLINE_STATUS st = frames_doc->LoadInline(&inline_url, new_elm, IMAGE_INLINE);
				if (OpStatus::IsMemoryError(st))
				{
					status = OpStatus::ERR_NO_MEMORY;
					SetIsOutOfMemory(TRUE);
				}
			}
#endif // LOGDOC_LOAD_IMAGES_FROM_PARSER

			SetElementNumberWithinForm(this, new_elm);
			SetElementFormNumber(this, new_elm);

#if defined(WAND_SUPPORT) && defined(PREFILLED_FORM_WAND_SUPPORT)
			if (g_wand_manager->HasMatch(frames_doc, new_elm))
			{
				frames_doc->SetHasWandMatches(TRUE);
				g_wand_manager->InsertWandDataInDocument(frames_doc, new_elm, NO);
			}
#endif // defined(WAND_SUPPORT) && defined(PREFILLED_FORM_WAND_SUPPORT)
			break;

		case HE_BUTTON:
		case HE_SELECT:
#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_)
		case HE_KEYGEN:
#endif // _SSL_SUPPORT_ && !_EXTERNAL_SSL_SUPPORT_
		case HE_TEXTAREA:
			SetElementNumberWithinForm(this, new_elm);
			SetElementFormNumber(this, new_elm);
			break;

#ifdef MANUAL_PLUGIN_ACTIVATION
		case HE_APPLET:
			if (ESGetScriptExternal())
				new_elm->SetPluginExternal(TRUE);
			break;
#endif // MANUAL_PLUGIN_ACTIVATION

		case HE_OBJECT:
		case HE_EMBED:
			// Plugins can act as form controls
			SetElementFormNumber(this, new_elm);

#ifdef MANUAL_PLUGIN_ACTIVATION
			if (ESGetScriptExternal() || frames_doc->IsPluginDoc())
				new_elm->SetPluginExternal(TRUE);
#endif // MANUAL_PLUGIN_ACTIVATION
			break;

		case HE_LABEL:
		case HE_FIELDSET:
		case HE_OUTPUT:
		case HE_LEGEND:
		case HE_PROGRESS:
		case HE_METER:
			SetElementFormNumber(this, new_elm);
			break;

		case HE_IMG:
		{
			SetElementFormNumber(this, new_elm);
#if !defined(LOGDOC_LOAD_IMAGES_FROM_PARSER) && !defined(CANVAS_SUPPORT)
			if ((new_elm->HasAttr(ATTR_ONLOAD) || new_elm->HasAttr(ATTR_ONERROR)) && !new_elm->GetHEListElmForInline(IMAGE_INLINE))
#endif // !LOGDOC_LOAD_IMAGES_FROM_PARSER && !CANVAS_SUPPORT
			{
				/* There are three reasons we might choose to load the image now.
				   1. The tweak to load images from the parser is enabled.
				   2. Canvas is enabled, in which case we have to make sure the images
				      are loaded to be able to be spec compliant and compatible with other
					  browsers.
				   3. A script appears to be waiting for us to load this image, so we better
				      load it as soon as possible. */

				URL *src = new_elm->GetUrlAttr(ATTR_SRC, NS_IDX_HTML, frames_doc->GetLogicalDocument());
				if (src && !src->IsEmpty())
				{
					OP_LOAD_INLINE_STATUS inlinestatus = frames_doc->GetLoadImages() ? frames_doc->LoadInline(src, new_elm, IMAGE_INLINE) : LoadInlineStatus::LOADING_REFUSED;

					if (OpStatus::IsError(inlinestatus) && !OpStatus::IsMemoryError(inlinestatus))
						inlinestatus = new_elm->SendEvent(ONLOAD, frames_doc);

					if (OpStatus::IsMemoryError(inlinestatus))
					{
						status = OpStatus::ERR_NO_MEMORY;
						SetIsOutOfMemory(TRUE);
					}
				}
			}
		}
		break;
		case HE_NOSCRIPT:
			/* GetESEnabled() will return FALSE if there's a frameset present since SCRIPTs have
			 * to be ignored after it, but NOSCRIPTs after the frameset should also be ignored.
			 * DSK-138076 and CORE-27636 for some context. */
			if (!GetESEnabled() && !IsFramesDoc())
				new_elm->SetSpecialBoolAttr(ATTR_NOSCRIPT_ENABLED, TRUE, SpecialNs::NS_LOGDOC);
			break;
#ifdef MEDIA_HTML_SUPPORT
		case HE_AUDIO:
		case HE_VIDEO:
		case HE_SOURCE:
		case Markup::HTE_TRACK:
		{
#ifdef DELAYED_SCRIPT_EXECUTION
			if (!new_elm->IsIncludedActual())
				break; // done in ESInsertElement instead
#endif // DELAYED_SCRIPT_EXECUTION
			status = InsertMediaElement(new_elm);
			if (OpStatus::IsMemoryError(status))
				SetIsOutOfMemory(TRUE);
			break;
		}
#endif // MEDIA_HTML_SUPPORT
		}
	}
	break;

#ifdef _WML_SUPPORT_
	case NS_WML:
		if (new_elm->WMLInit(GetFramesDocument()->GetDocManager()) == OpStatus::ERR_NO_MEMORY)
		{
			status = OpStatus::ERR_NO_MEMORY;
			SetIsOutOfMemory(TRUE);
		}
		break;
#endif // _WML_SUPPORT_

#ifdef SVG_SUPPORT
	case NS_SVG:
	{
		OP_STATUS stat = g_svg_manager->InsertElement(new_elm);

		if (OpStatus::IsMemoryError(stat))
		{
			status = OpStatus::ERR_NO_MEMORY;
			SetIsOutOfMemory(TRUE);
		}
	}
	break;
#endif // SVG_SUPPORT
	}

	// We need to set the initial forms pseudo classes sometime before the element is
	// accessed by script or layed out, but after it's been hooked into the tree since
	// tree structure might affect for pseudo classes. Here is a good place, except for textareas
	// since they haven't gotten their value yet.
	if (new_elm->IsFormElement())
	{
		FormValue* form_value = new_elm->GetFormValue();
		form_value->SetMarkedPseudoClasses(form_value->CalculateFormPseudoClasses(frames_doc, new_elm));
	}

#ifdef DELAYED_SCRIPT_EXECUTION
	if (es_is_executing_delayed_script && es_need_recover && !GetIsParsingInnerHTML())
		status = HEParseStatus::STOPPED;
#endif // DELAYED_SCRIPT_EXECUTION

	if (is_out_of_memory)
		return OpStatus::ERR_NO_MEMORY;

	if (!new_elm->HasNoLayout() && !new_elm->IsText())
	{
#ifdef ACCESS_KEYS_SUPPORT
		const uni_char* key = new_elm->GetAccesskey();
		if (key)
		{
			RETURN_IF_MEMORY_ERROR(AddAccessKey(key, new_elm));
		}
#endif // ACCESS_KEYS_SUPPORT

		HTML_ElementType type = new_elm->Type();
		if (new_elm->GetNsType() == NS_HTML &&
			((type == HE_INPUT && new_elm->GetInputType() != INPUT_HIDDEN) || type == HE_SELECT || type == HE_BUTTON
#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_)
			|| type == HE_KEYGEN
#endif
			|| type == HE_TEXTAREA || type == HE_IMG || type == HE_EMBED || type == HE_APPLET || type == HE_TABLE || type == HE_HR))
		{
			hldoc->SetHasContent(TRUE);
		}
	}

#ifdef DNS_PREFETCHING
	URL anchor_url = new_elm->GetAnchor_URL(frames_doc);
	if (!anchor_url.IsEmpty())
		GetLogicalDocument()->DNSPrefetch(anchor_url, DNS_PREFETCH_PARSING);
#endif // DNS_PREFETCHING

    return status;
}

BOOL HLDocProfile::IsTextElementVisible(HTML_Element *element, BOOL *is_title)
{
	if (is_title)
		*is_title = FALSE;

	if (!element->IsText())
		return FALSE;

	HTML_Element *parent_candidate = element->Parent();
	while (parent_candidate->IsText())
		parent_candidate = parent_candidate->Parent();

	if (is_title)
		*is_title = parent_candidate->IsMatchingType(HE_TITLE, NS_HTML);

	return (
		   !parent_candidate->IsMatchingType(HE_TITLE, NS_HTML)
		&& !parent_candidate->IsScriptElement()
		&& !parent_candidate->IsStyleElement()
#ifdef SVG_SUPPORT
		&& !parent_candidate->IsMatchingType(Markup::SVGE_HANDLER, NS_SVG)
#endif // SVG_SUPPORT
		&& !(parent_candidate->IsMatchingType(HE_NOSCRIPT, NS_HTML)
		&& !parent_candidate->GetSpecialBoolAttr(ATTR_NOSCRIPT_ENABLED, SpecialNs::NS_LOGDOC))
		&& !(parent_candidate->IsMatchingType(HE_NOFRAMES, NS_HTML)
				&& g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::FramesEnabled, *GetURL()))
		&& !(parent_candidate->IsMatchingType(HE_NOEMBED, NS_HTML))
		&& !(parent_candidate->IsMatchingType(HE_IFRAME, NS_HTML)
				&& g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::IFramesEnabled, *GetURL()))
		);
}

OP_HEP_STATUS HLDocProfile::EndElement(HTML_Element *elm)
{
    if (!elm)
        return OpStatus::OK;

	RETURN_IF_ERROR(GetLayoutWorkplace()->EndElement(elm));

	switch (elm->GetNsType())
    {
    case NS_HTML:
        {
            HTML_ElementType type = elm->Type();

            switch(type)
            {
			case HE_APPLET:
			    if (elm->GetLayoutBox())
				    elm->GetLayoutBox()->SignalChange(frames_doc);
                break;

			case HE_OBJECT:
				if (!elm->GetLayoutBox())
					/* No layout box, because we have been waiting PARAM children
					   of the OBJECT. Schedule for reflow, so that it gets a box. */

					elm->MarkExtraDirty(frames_doc);

#ifdef JS_PLUGIN_SUPPORT
				RETURN_IF_MEMORY_ERROR(OpStatus::IsMemoryError(elm->PassParamsForJSPlugin(GetLogicalDocument())));
#endif // JS_PLUGIN_SUPPORT

				break;

            case HE_TITLE:
                if (GetFramesDocument())
                    RETURN_IF_MEMORY_ERROR(GetFramesDocument()->GetWindow()->UpdateTitle());
                break;
			case HE_SELECT:
#ifdef _WML_SUPPORT_
					if (HasWmlContent())
					{
						if (WML_Context *wc = WMLGetContext())
						{
							wc->SetInitialSelectValue(elm);
							wc->UpdateWmlSelection(elm, FALSE);
						}
					}
#endif // _WML_SUPPORT_

					RETURN_IF_MEMORY_ERROR(FormValueList::GetAs(elm->GetFormValue())->SetInitialSelection(elm, FALSE));
				break;

			case HE_TEXTAREA:
				{
					FormValueTextArea* form_value = FormValueTextArea::GetAs(elm->GetFormValue());
					RETURN_IF_MEMORY_ERROR(form_value->SetInitialValue(elm));
					// The length might have changed and then we need to revalidate the textarea. If
					// this doesn't change type flags it should be pretty cheap anyway.
					frames_doc->GetLogicalDocument()->GetLayoutWorkplace()->ApplyPropertyChanges(elm, CSS_PSEUDO_CLASS_GROUP_FORM, TRUE);
				}
				break;

			case Markup::HTE_SCRIPT:
				{
# ifdef DELAYED_SCRIPT_EXECUTION
					unsigned stream_position = hldoc->GetLastBufferStartOffset();
# else
					unsigned stream_position = 0;
# endif // DELAYED_SCRIPT_EXECUTION
					RETURN_IF_MEMORY_ERROR(elm->HandleScriptElement(this, NULL, TRUE, GetLogicalDocument()->GetRoot()->IsAncestorOf(elm), stream_position));
				}
				break;

			case Markup::HTE_STYLE:
				if (GetLogicalDocument()->GetRoot()->IsAncestorOf(elm))
					RETURN_IF_MEMORY_ERROR(elm->ProcessCSS(this));
				break;

#ifdef MEDIA_HTML_SUPPORT
			case Markup::HTE_AUDIO:
			case Markup::HTE_VIDEO:
				if (MediaElement* media = elm->GetMediaElement())
					media->HandleEndElement();
				break;
#endif // MEDIA_HTML_SUPPORT
            }
        } // end NS_HTML
        break;

#ifdef _WML_SUPPORT_
    case NS_WML:
        if ((int)elm->Type() == WE_CARD)
		{
            WMLGetContext()->SetStatusOff(WS_FIRSTCARD | WS_INRIGHTCARD);
			if (GetFramesDocument())
				RETURN_IF_MEMORY_ERROR(GetFramesDocument()->GetWindow()->UpdateTitle());
		}
		else if ((int)elm->Type() == WE_ANCHOR)
		{
			// We need to reload properties to update with visited state (href in <GO> children).
			RETURN_IF_ERROR(GetLayoutWorkplace()->ApplyPropertyChanges(elm, 0, TRUE));
		}
		break;
#endif // _WML_SUPPORT_

#ifdef SVG_SUPPORT
		case NS_SVG:
			{
				BOOL in_document = GetLogicalDocument()->GetRoot()->IsAncestorOf(elm);

				if (elm->Type() == Markup::SVGE_SCRIPT)
				{
# ifdef DELAYED_SCRIPT_EXECUTION
					unsigned stream_position = hldoc->GetLastBufferStartOffset();
# else
					unsigned stream_position = 0;
# endif // DELAYED_SCRIPT_EXECUTION
					RETURN_IF_MEMORY_ERROR(elm->HandleScriptElement(this, NULL, TRUE, in_document, stream_position));
				}
				else if (elm->Type() == Markup::HTE_STYLE)
				{
					if (in_document)
						RETURN_IF_MEMORY_ERROR(elm->ProcessCSS(this));
				}

				// Only notify @c g_svg_manager if the element is in the
				// main document tree. (See CORE-42705).
				if (in_document)
					RETURN_IF_MEMORY_ERROR(g_svg_manager->EndElement(elm));
			}
			break;
#endif // SVG_SUPPORT
    }
#ifdef DOCUMENT_EDIT_SUPPORT
	if (elm->IsContentEditable() && !elm->IsMatchingType(HE_IFRAME, NS_HTML) && !elm->IsMatchingType(HE_HTML, NS_HTML))
	{
		OpStatus::Ignore(frames_doc->SetEditable(FramesDocument::CONTENTEDITABLE));
		if (frames_doc->GetDocumentEdit())
			frames_doc->GetDocumentEdit()->InitEditableRoot(elm);
	}
#endif // DOCUMENT_EDIT_SUPPORT

    return OpStatus::OK;
}

#ifdef ACCESS_KEYS_SUPPORT

#include "accesskeys.cpp"

OpKey::Code
AccessKeyStringToOpKey(const uni_char *key)
{
	if (!key)
		return OP_KEY_FIRST;

	if (uni_strlen(key) == 1)
	{
		uni_char str[2]; /* ARRAY OK 2011-12-01 sof */
		str[0] = uni_toupper(*key);
		str[1] = 0;
		return OpKey::FromString(str);
	}

	OpKey::Code op_key = OP_KEY_FIRST;
#if defined ACCESSKEYS_RECOGNIZES_OP_KEYS || defined ACCESSKEYS_RECOGNIZES_MULTICHAR

	const uni_char *original_key = key;

# ifdef ACCESSKEYS_RECOGNIZES_OP_KEYS
	if (uni_strni_eq(key, "OP_KEY_", 7))
		key += 7;

	if (uni_stri_eq(key, "F1"))
		op_key = OP_KEY_F1;
	else if (uni_stri_eq(key, "F2"))
		op_key = OP_KEY_F2;
	else if (uni_stri_eq(key, "F3"))
		op_key = OP_KEY_F3;
	else if (uni_stri_eq(key, "F4"))
		op_key = OP_KEY_F4;
	else if (uni_stri_eq(key, "F5"))
		op_key = OP_KEY_F5;
	else if (uni_stri_eq(key, "F6"))
		op_key = OP_KEY_F6;
	else if (uni_stri_eq(key, "F7"))
		op_key = OP_KEY_F7;
	else if (uni_stri_eq(key, "F8"))
		op_key = OP_KEY_F8;
	else if (uni_stri_eq(key, "F9"))
		op_key = OP_KEY_F9;
	else if (uni_stri_eq(key, "F10"))
		op_key = OP_KEY_F10;
	else if (uni_stri_eq(key, "F11"))
		op_key = OP_KEY_F11;
	else if (uni_stri_eq(key, "F12"))
		op_key = OP_KEY_F12;
	else if (uni_stri_eq(key, "HOME"))
		op_key = OP_KEY_HOME;
	else if (uni_stri_eq(key, "END"))
		op_key = OP_KEY_END;
	else if (uni_stri_eq(key, "PAGEUP"))
		op_key = OP_KEY_PAGEUP;
	else if (uni_stri_eq(key, "PAGEDOWN"))
		op_key = OP_KEY_PAGEDOWN;
	else if (uni_stri_eq(key, "UP"))
		op_key = OP_KEY_UP;
	else if (uni_stri_eq(key, "DOWN"))
		op_key = OP_KEY_DOWN;
	else if (uni_stri_eq(key, "LEFT"))
		op_key = OP_KEY_LEFT;
	else if (uni_stri_eq(key, "RIGHT"))
		op_key = OP_KEY_RIGHT;
	else if (uni_stri_eq(key, "ESCAPE"))
		op_key = OP_KEY_ESCAPE;
	else if (uni_stri_eq(key, "ESC"))
		op_key = OP_KEY_ESCAPE;
	else if (uni_stri_eq(key, "DIV"))
		op_key = OP_KEY_DIV;
	else if (uni_stri_eq(key, "MUL"))
		op_key = OP_KEY_MUL;
	else if (uni_stri_eq(key, "ADD"))
		op_key = OP_KEY_ADD;
	else if (uni_stri_eq(key, "DEC"))
		op_key = OP_KEY_DEC;
	else if (uni_stri_eq(key, "INS"))
		op_key = OP_KEY_INS;
	else if (uni_stri_eq(key, "DEL"))
		op_key = OP_KEY_DEL;
	else if (uni_stri_eq(key, "BACKSPACE"))
		op_key = OP_KEY_BACKSPACE;
	else if (uni_stri_eq(key, "TAB"))
		op_key = OP_KEY_TAB;
	else if (uni_stri_eq(key, "SPACE"))
		op_key = OP_KEY_SPACE;
	else if (uni_stri_eq(key, "ENTER"))
		op_key = OP_KEY_ENTER;
# endif // ACCESSKEYS_RECOGNIZES_OP_KEYS

# ifdef ACCESSKEYS_RECOGNIZES_MULTICHAR
	// If we haven't found one yet, translate the WAP CSS accesskey values to OpKeys
	if (op_key == OP_KEY_FIRST)
		op_key = AccesskeyToOpKey(original_key);
# endif // ACCESSKEYS_RECOGNIZES_MULTICHAR
#endif // ACCESSKEYS_RECOGNIZES_OP_KEYS || ACCESSKEYS_RECOGNIZES_MULTICHAR
	return op_key;
}

/*virtual*/ void
HLDocProfile::AccessKey::AccessKeyElmRef::OnDelete(FramesDocument *document)
{
	if (document)
	{
		WindowCommander *wincom = document->GetWindow()->GetWindowCommander();
		wincom->GetAccessKeyListener()->OnAccessKeyRemoved(wincom, m_key->key);

		m_key->Out();
		OP_DELETE(m_key);
	}
}

OP_BOOLEAN
HLDocProfile::AddAccessKey(AccessKeyTransaction &transaction, const uni_char* key)
{
	OpKey::Code k = AccessKeyStringToOpKey(key);
	AccessKey* access_key = GetAccessKey(k);

	if (access_key == NULL)
	{
		access_key = OP_NEW(AccessKey, (k, NULL, TRUE));
		if (!access_key)
			return OpStatus::ERR_NO_MEMORY;

		access_key->Into(&transaction);

		return OpBoolean::IS_TRUE;
	}

    return OpBoolean::IS_FALSE;
}

OP_STATUS
GetAccessKeyInfo(HLDocProfile::AccessKey *access_key, HLDocProfile *hld_profile)
{
	OP_ASSERT(access_key);

	HTML_Element *element = access_key->GetElement();
	HTML_ElementType element_type = element->Type();
	NS_Type ns_type = element->GetNsType();
	access_key->title.Empty();
	access_key->url.Empty();

	if (ns_type == NS_HTML)
	{
		switch (element_type)
		{
		case HE_INPUT:
			if (element->GetInputType() == INPUT_PASSWORD
				|| element->GetInputType() == INPUT_HIDDEN
				|| element->GetInputType() == INPUT_RADIO
				|| element->GetInputType() == INPUT_CHECKBOX)
				break;
			// fall-through for normal values
		case HE_BUTTON:
			{
				FormValue *value = element->GetFormValue();
				if (value)
				{
					RETURN_IF_MEMORY_ERROR(value->GetValueAsText(element, access_key->title));
				}
			}
			break;

		case HE_AREA:
			{
				URL *url = element->GetAREA_URL(hld_profile->GetLogicalDocument());
				if (url)
				{
					RETURN_IF_MEMORY_ERROR(url->GetAttribute(URL::KUniName, access_key->url, TRUE));
				}
				RETURN_IF_MEMORY_ERROR(access_key->title.Set(element->GetAREA_Name()));
			}
			break;

		case HE_A:
			{
				const uni_char* title = element->GetA_Title();
				if (title)
				{
					RETURN_IF_MEMORY_ERROR(access_key->title.Set(title));
				}

				// use child elements to build text.. works in most cases, right?
				HTML_Element* child_elm = element->FirstChild();
				const uni_char* alt_text = NULL;

				while (!access_key->title.Length() && child_elm && child_elm != element)
				{
					if (child_elm->Type() == HE_IMG && child_elm->GetIMG_Alt())
					{
						alt_text = child_elm->GetIMG_Alt();
					}
					else if (child_elm->Type() == HE_TEXT)
					{
						RETURN_IF_MEMORY_ERROR(access_key->title.Set(child_elm->Content()));
					}

					child_elm = child_elm->Next();
				}

				if (!access_key->title.Length() && alt_text)
				{
					RETURN_IF_MEMORY_ERROR(access_key->title.Set(alt_text));
				}


				if (URL* a_url = element->GetA_URL(hld_profile->GetLogicalDocument()))
				{
					RETURN_IF_MEMORY_ERROR(a_url->GetAttribute(URL::KUniName, access_key->url, TRUE));
				}
			}
			break;
		}
	}
#ifdef _WML_SUPPORT_
	else if (ns_type == NS_WML)
	{
		if (WML_ElementType(element_type) == WE_DO)
		{
			const uni_char* label = element->GetStringAttr(WA_LABEL, NS_IDX_WML);
			if (label)
			{
				RETURN_IF_MEMORY_ERROR(access_key->title.Set(label));
			}

			UINT32 dummy_action;
			URL do_url = hld_profile->WMLGetContext()->GetWmlUrl(element, &dummy_action, WEVT_ONCLICK);

			if (!do_url.IsEmpty())
			{
				RETURN_IF_MEMORY_ERROR(do_url.GetAttribute(URL::KUniName, access_key->url, TRUE));
			}
		}
	}
#endif //_WML_SUPPORT_
	if (!access_key->title.Length())
	{
		RETURN_IF_MEMORY_ERROR(access_key->title.Set(element->GetElementTitle()));
	}

	return OpStatus::OK;
}

OP_STATUS
HLDocProfile::CommitAccessKeys(AccessKeyTransaction &transaction, HTML_Element* element)
{
	AccessKey *access_key = (AccessKey*)transaction.First();
	while (access_key)
	{
		AccessKey *next_key = (AccessKey*)access_key->Suc();
		access_key->Out();
		access_key->SetElement(element);

		if (OpStatus::IsMemoryError(GetAccessKeyInfo(access_key, this)))
		{
			OP_DELETE(access_key);
			return OpStatus::ERR_NO_MEMORY;
		}

		access_key->Into(&m_access_keys);

		WindowCommander *wc = frames_doc->GetWindow()->GetWindowCommander();
		wc->GetAccessKeyListener()
			->OnAccessKeyAdded(wc, access_key->key, access_key->title.CStr(), access_key->url.CStr());

		access_key = next_key;
	}

	return OpStatus::OK;
}

OP_STATUS
HLDocProfile::AddAccessKeyFromStyle(const uni_char* key, HTML_Element* element, BOOL &conflict)
{
	OpKey::Code op_key = AccessKeyStringToOpKey(key);

#ifdef _WML_SUPPORT_
	if (op_key == OP_KEY_FIRST && element->IsMatchingType(WE_DO, NS_WML))
	{
		op_key = m_next_do_key;
		m_next_do_key = static_cast<OpKey::Code>(static_cast<unsigned>(m_next_do_key) + 1);
		if (op_key > OP_KEY_LAST_DO_UNSPECIFIED)
		{
			OP_ASSERT(!"The number of unknown DO keys is too high");
			op_key = OP_KEY_LAST_DO_UNSPECIFIED;
		}
	}
#endif // _WML_SUPPORT_

	if (op_key == OP_KEY_FIRST)
	{
		conflict = TRUE;
		return OpStatus::OK;
	}

	AccessKey* access_key = GetAccessKey(op_key);

	if (access_key)
		conflict = TRUE;
	else
	{
		access_key = OP_NEW(AccessKey, (op_key, element, TRUE));
		if (access_key == NULL
			|| OpStatus::IsMemoryError(GetAccessKeyInfo(access_key, this)))
		{
			OP_DELETE(access_key);
			return OpStatus::ERR_NO_MEMORY;
		}

		access_key->Into(&m_access_keys);

		conflict = FALSE;

		WindowCommander *wc = frames_doc->GetWindow()->GetWindowCommander();
		wc->GetAccessKeyListener()
			->OnAccessKeyAdded(wc, op_key, access_key->title.CStr(), access_key->url.CStr());
	}

    return OpStatus::OK;
}

void
HLDocProfile::RemoveAccessKey(HTML_Element *elm, BOOL from_style_only/*=FALSE*/)
{
	AccessKey *key = (AccessKey *)m_access_keys.First();
	while (key)
	{
		AccessKey *tmp_key = (AccessKey *)key->Suc();

		if (key->GetElement() == elm && (!from_style_only || key->from_style))
		{
			WindowCommander *wc = frames_doc->GetWindow()->GetWindowCommander();
			wc->GetAccessKeyListener()->OnAccessKeyRemoved(wc, key->key);

			key->Out();
			OP_DELETE(key);
		}

		key = tmp_key;
	}
}

OP_STATUS HLDocProfile::AddAccessKey(const uni_char* key, HTML_Element* element)
{
	OpKey::Code op_key = AccessKeyStringToOpKey(key);
	AccessKey* access_key = GetAccessKey(op_key);

	BOOL changed_access_key_binding = TRUE;

	if (access_key == NULL)
	{
#ifdef _WML_SUPPORT_
		if (op_key == OP_KEY_FIRST && element->IsMatchingType(WE_DO, NS_WML))
		{
			op_key = m_next_do_key;
			m_next_do_key = static_cast<OpKey::Code>(static_cast<unsigned>(m_next_do_key) + 1);
			if (op_key > OP_KEY_LAST_DO_UNSPECIFIED)
			{
				OP_ASSERT(!"The number of unknown DO keys is too high");
				op_key = OP_KEY_LAST_DO_UNSPECIFIED;
			}
		}
#endif // _WML_SUPPORT_

		access_key = OP_NEW(AccessKey, (op_key, element, FALSE));
		if (!access_key)
			return OpStatus::ERR_NO_MEMORY;

		access_key->Into(&m_access_keys);
		changed_access_key_binding = TRUE;
	}
	else
	{
		if (element != access_key->GetElement())
		{
			// Already something bound to that key. Let the element first in the tree
			// decide and mark that we have conflicts here
			access_key->had_conflicts = TRUE;
			if (element->Precedes(access_key->GetElement()))
			{
				access_key->SetElement(element);
				changed_access_key_binding = TRUE;
			}
		}
	}

	if (changed_access_key_binding)
	{
		if (OpStatus::IsMemoryError(GetAccessKeyInfo(access_key, this)))
		{
			access_key->Out();
			OP_DELETE(access_key);
			return OpStatus::ERR_NO_MEMORY;
		}

		WindowCommander* wc = frames_doc->GetWindow()->GetWindowCommander();
		wc->GetAccessKeyListener()->OnAccessKeyAdded(wc, op_key, access_key->title.CStr(), access_key->url.CStr());
	}
	return OpStatus::OK;
}

OP_STATUS HLDocProfile::RemoveAccessKey(const uni_char* key, HTML_Element* element)
{
	OpKey::Code op_key = AccessKeyStringToOpKey(key);
	AccessKey* access_key = GetAccessKey(op_key);

	HTML_Element* new_element = NULL;
	if (access_key && access_key->GetElement() == element)
	{
		BOOL had_conflicts = access_key->had_conflicts;
		if (had_conflicts)
		{
			// Maybe there is some better element later in the document
			HTML_Element* it = element;
			while (it)
			{
				const uni_char* other_key = it->GetAccesskey();
				if (other_key)
				{
					OpKey::Code other_op_key = AccessKeyStringToOpKey(other_key);
					if (other_op_key == op_key)
					{
						// Found a new binding to the key
						new_element = it;
						break;
					}
				}
				it = it->NextActual();
			}
		}

		if (!new_element)
		{
			access_key->Out();
			OP_DELETE(access_key);
		}

		WindowCommander* wc = frames_doc->GetWindow()->GetWindowCommander();
		wc->GetAccessKeyListener()->OnAccessKeyRemoved(wc, op_key);

		if (new_element)
		{
			access_key->SetElement(new_element);
			if (OpStatus::IsMemoryError(GetAccessKeyInfo(access_key, this)))
			{
				access_key->Out();
				OP_DELETE(access_key);
				return OpStatus::ERR_NO_MEMORY;
			}

			wc->GetAccessKeyListener()->OnAccessKeyAdded(wc, op_key, access_key->title.CStr(), access_key->url.CStr());
		}
	}

	return OpStatus::OK;
}

HLDocProfile::AccessKey* HLDocProfile::GetAccessKey(OpKey::Code key)
{
	for (AccessKey* access_key = (AccessKey*) m_access_keys.First(); access_key; access_key = (AccessKey*)access_key->Suc())
		if (access_key->key == key)
			return access_key;

	return NULL;
}

OP_STATUS HLDocProfile::GetAllAccessKeys(OpVector<HLDocProfile::AccessKey>& accesskeys)
{
	for (AccessKey* access_key = (AccessKey*) m_access_keys.First(); access_key; access_key = (AccessKey*)access_key->Suc())
	{
		RETURN_IF_ERROR(accesskeys.Add(access_key));
	}
	return OpStatus::OK;
}

#endif // ACCESS_KEYS_SUPPORT

OP_STATUS HLDocProfile::SetDescription(const uni_char *desc)
{
	OP_DELETEA(meta_description);
    meta_description = UniSetNewStr(desc);
    if (desc && *desc && ! meta_description)
        return OpStatus::ERR_NO_MEMORY;

    return OpStatus::OK;
}

#define DOC_TYPE_NO_NEED		0
#define DOC_TYPE_TRANSITIONAL	1
#define DOC_TYPE_4_TRANSITIONAL	2
#define DOC_TYPE_FRAMESET		3
#define DOC_TYPE_4_FRAMESET		4
#define DOC_TYPE_FORCED_QUIRKS	5


const uni_char* HLDocProfile::GetDoctypeName() { return hldoc->GetDoctypeName(); }
const uni_char* HLDocProfile::GetDoctypePubId() { return hldoc->GetDoctypePubId(); }
const uni_char* HLDocProfile::GetDoctypeSysId() { return hldoc->GetDoctypeSysId(); }

BOOL HLDocProfile::IsInStrictMode() { return hldoc->IsInStrictMode(); }
void HLDocProfile::SetIsInStrictMode(BOOL smode) { hldoc->SetIsInStrictMode(smode); }

BOOL HLDocProfile::IsInStrictLineHeightMode() { return hldoc->IsInStrictLineHeightMode(); }

NS_Type HLDocProfile::GetNsType(int ns_idx)
{
	if (ns_idx <= 0)
		if (IsXml())
		{
			if (IsXhtml())
				return NS_HTML;
#ifdef _WML_SUPPORT_
			if (IsWml())
				return NS_WML;
#endif // _WML_SUPPORT_

			return NS_USER;
		}
		else
			return NS_HTML;
	else
		return g_ns_manager->GetNsTypeAt(ns_idx);
}

URL* HLDocProfile::GetURL()
{
	return hldoc->GetURL();
}

HTML_Element* HLDocProfile::GetRoot()
{
	return hldoc->GetRoot();
}

HTML_Element* HLDocProfile::GetDocRoot()
{
	return hldoc->GetDocRoot();
}

HTML_Element* HLDocProfile::GetHeadElm()
{
	if (HTML_Element *root = GetDocRoot())
		if (root->IsMatchingType(HE_HTML, NS_HTML))
		{
			HTML_Element *child = root->FirstChild();

			while (child && !child->IsMatchingType(HE_HEAD, NS_HTML))
				child = child->Suc();

			return child;
		}

	return NULL;
}

#ifdef _PRINT_SUPPORT_
OP_STATUS HLDocProfile::AddPrintImageElement(HTML_Element* element, InlineResourceType inline_type, URL* img_url)
{
	OP_ASSERT(element && img_url);
	OP_ASSERT(inline_type == IMAGE_INLINE || inline_type == BGIMAGE_INLINE);

    HEListElm *hele = OP_NEW(HEListElm, (element, inline_type, GetFramesDocument()));
    if (hele)
	{
		if (hele->UpdateImageUrl(img_url))
			hele->Into(&print_image_list);
		else
		{
			OP_DELETE(hele);
			hele = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}
	}

    return OpStatus::OK;
}

BOOL HLDocProfile::IsPrintImageElement(HEListElm* elm)
{
	return print_image_list.HasLink(elm);
}

void  HLDocProfile::RemovePrintImageElement(HEListElm* elm)
{
	if (IsPrintImageElement(elm))
		elm->Out();
}
#endif

void HLDocProfile::SetHasMediaStyle(short media)
{
	if (media && (has_media_style & media) != media)
	{
		has_media_style |= media;
		if (frames_doc && frames_doc->GetLayoutMode() != LAYOUT_NORMAL)
			frames_doc->SetHandheldStyleFound();
	}
}

LayoutWorkplace* HLDocProfile::GetLayoutWorkplace()
{
	OP_ASSERT(hldoc); OP_ASSERT(hldoc->GetLayoutWorkplace()); return hldoc->GetLayoutWorkplace();
}

#ifdef LOGDOC_GETSCRIPTLIST

OP_STATUS HLDocProfile::AddScript(HTML_Element *element)
{
	URL *script_url = element->GetScriptURL(*GetURL(), frames_doc->GetLogicalDocument());

	if (script_url)
	{
		for (unsigned index = 0; index < script_urls.GetCount(); ++index)
			if (*script_url == *script_urls.Get(index))
				return OpStatus::OK;

		URL *copy_url = OP_NEW(URL, (*script_url));
		if (!copy_url)
			return OpStatus::ERR_NO_MEMORY;
		else
			return script_urls.Add(copy_url);
	}

	return OpStatus::OK;
}

#endif // LOGDOC_GETSCRIPTLIST

#ifdef USE_HTML_PARSER_FOR_XML
void HLDocProfile::PopNsDecl(unsigned level)
{
	XMLNamespaceDeclaration::Pop(m_current_ns_decl, level);
}
#endif // USE_HTML_PARSER_FOR_XML
