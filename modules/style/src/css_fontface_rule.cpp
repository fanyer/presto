/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/style/src/css_fontface_rule.h"
#include "modules/style/src/css_parser.h"

#include "modules/doc/frm_doc.h"
#ifdef SVG_SUPPORT
# include "modules/svg/SVGManager.h"
#endif // SVG_SUPPORT

OP_STATUS CSS_FontfaceRule::CreateCSSFontDescriptor(OpFontInfo** out_fontinfo)
{
	OP_NEW_DBG("CreateCSSFontDescriptor", "webfonts");

	OP_ASSERT(GetFamilyName() != NULL);

	OP_DBG ((
		UNI_L("%s%s%s%s"),
		GetFamilyName(),
		GetStyle() == CSS_VALUE_italic ? UNI_L(" italic ") : UNI_L(""),
		GetStyle() == CSS_VALUE_normal ? UNI_L(" normal ") : UNI_L(""),
		GetWeight() > 4 ? UNI_L(" bold ") : UNI_L("")
		));

	OpFontInfo::FontType format = GetFormat() == CSS_WebFont::FORMAT_SVG ? OpFontInfo::SVG_WEBFONT : OpFontInfo::PLATFORM_WEBFONT;
	if (IsLocalFont())
	{
		format = OpFontInfo::PLATFORM_LOCAL_WEBFONT;
	}

	int fn = -1;

	BOOL create_fontnumber = TRUE;

	fn = styleManager->GetFontNumber(GetFamilyName());
	if (fn >= 0)
		create_fontnumber = FALSE;

	if (create_fontnumber)
	{
		RETURN_IF_ERROR(styleManager->CreateFontNumber(GetFamilyName(), fn));
		OP_ASSERT(fn >= 0 && fn <= SHRT_MAX);
	}

	OpFontInfo* fi = OP_NEW(OpFontInfo, ());
	if (!fi)
		return OpStatus::ERR_NO_MEMORY;

	fi->SetFontType(format);
	if (OpStatus::IsError(fi->SetFace(GetFamilyName())))
	{
		styleManager->ReleaseFontNumber(fn);
		OP_DELETE(fi);
		return OpStatus::ERR_NO_MEMORY;
	}

	switch (GetStyle())
	{
	case CSS_VALUE_normal:
		fi->SetNormal(TRUE);
		break;
	case CSS_VALUE_italic:
		fi->SetItalic(TRUE);
		break;
	case CSS_VALUE_oblique:
		fi->SetOblique(TRUE);
		break;
	default:
		OP_ASSERT(!"Unexpected value");
		break;
	}

	short weight = GetWeight();
	if (weight >= 0 && weight <= 9)
		fi->SetWeight((UINT8)weight, 1);

	fi->SetFontNumber(fn);

	OP_DBG ((
			UNI_L("Assigned fontnumber (%d): %s%s%s%s"),
			fn,
			fi->GetFace(),
			fi->HasItalic() ? UNI_L(" italic ") : UNI_L(""),
			fi->HasNormal() ? UNI_L(" normal ") : UNI_L(""),
			fi->HasWeight(7) ? UNI_L(" bold ") : UNI_L("")
			));

	*out_fontinfo = fi;
	return OpStatus::OK;
}

/* virtual */ void
CSS_FontfaceRule::RemovedWebFont(FramesDocument* doc)
{
	if (ExternalInlineListener::InList())
	{
		m_loading_url.UnsetURL();
		doc->StopLoadingInline(GetSrcURL(), this);
	}

#ifdef SVG_SUPPORT
	if (GetFormat() == CSS_WebFont::FORMAT_SVG)
		doc->GetLogicalDocument()->GetSVGWorkplace()->StopLoadingCSSResource(GetSrcURL(), this);
#endif

	if (GetLoadStatus() == WEBFONT_LOADED)
		if (g_webfont_manager)
			g_webfont_manager->RemoveCSSWebFont(doc, m_used_element, m_used_url);
}

/* virtual */ OP_STATUS
CSS_FontfaceRule::GetCssText(CSS* stylesheet, TempBuffer* buf, unsigned int indent_level)
{
	RETURN_IF_ERROR(buf->Append("@font-face"));
	return CSS_DeclarationBlockRule::GetCssText(stylesheet, buf, indent_level);
}

/* virtual */ CSS_PARSE_STATUS
CSS_FontfaceRule::SetCssText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, const int len)
{
	return stylesheet->ParseAndInsertRule(hld_prof, this, NULL, NULL, TRUE, CSS_TOK_DOM_FONT_FACE_RULE, text, len);
}

const uni_char*
CSS_FontfaceRule::GetFamilyName()
{
	CSS_property_list* props = GetPropertyList();

	CSS_decl* decl = props->GetLastDecl();
	while (decl)
	{
		if (decl->GetProperty() == CSS_PROPERTY_font_family)
		{
			if (decl->GetDeclType() == CSS_DECL_STRING)
				return decl->StringValue();
		}
		decl = decl->Pred();
	}
	return NULL;
}

short
CSS_FontfaceRule::GetStyle()
{
	CSS_property_list* props = GetPropertyList();
	CSS_decl* decl = props->GetLastDecl();
	while (decl)
	{
		if (decl->GetProperty() == CSS_PROPERTY_font_style)
		{
			OP_ASSERT(decl->GetDeclType() == CSS_DECL_TYPE);
			if (decl->GetValueType(0) == CSS_NUMBER)
			{
				return decl->TypeValue();
			}
		}
		decl = decl->Pred();
	}
	return CSS_VALUE_normal;
}

short
CSS_FontfaceRule::GetWeight()
{
	CSS_property_list* props = GetPropertyList();
	CSS_decl* decl = props->GetLastDecl();
	while (decl)
	{
		if (decl->GetProperty() == CSS_PROPERTY_font_weight)
		{
			if (decl->GetDeclType() == CSS_DECL_TYPE)
			{
				OP_ASSERT(!"Handle bolder and lighter!");
			}
			else if (decl->GetDeclType() == CSS_DECL_NUMBER)
			{
				if (decl->GetValueType(0) == CSS_NUMBER)
				{
					return (short)decl->GetNumberValue(0);
				}
			}
		}
		decl = decl->Pred();
	}
	return -1; // no defined font-weight
}

CSS_WebFont::LoadStatus CSS_FontfaceRule::GetLoadStatus()
{
	return static_cast<LoadStatus>(m_packed.load_status);
}

// called when loading a font via the svg path failed
void CSS_FontfaceRule::OnLoadingFailed(FramesDocument* doc, URL url)
{
	OP_ASSERT(m_loading_url->IsEmpty());

	OP_LOAD_INLINE_STATUS stat;
	// avoid endless bouncing between codepaths
	if (m_packed.format_mismatch)
	{
		stat = OpStatus::ERR; // trigger LoadNextInSrc below
	}
	else
	{
		// a webfont with format(svg) was requested to be loaded, but
		// loading failed. test loading it as an sfnt webfont.
		OP_ASSERT(GetFormat() == CSS_WebFont::FORMAT_SVG);
		m_packed.used_format = FORMAT_TRUETYPE | FORMAT_OPENTYPE | FORMAT_WOFF;
		m_packed.format_mismatch = TRUE;
		stat = doc->LoadInline(m_used_url, this, FALSE, TRUE, WEBFONT_INLINE);
	}
	if (OpStatus::IsError(stat))
	{
		if (OpStatus::IsMemoryError(stat) || OpStatus::IsError(LoadNextInSrc(doc)))
		{
			m_packed.load_status = WEBFONT_NOTFOUND;
			if (!doc->IsReflowing())
				doc->GetHLDocProfile()->GetCSSCollection()->StyleChanged(CSSCollection::CHANGED_WEBFONT);
		}
	}
	else
		m_loading_url.SetURL(m_used_url);
}

void CSS_FontfaceRule::OnLoad(FramesDocument* doc, URL url, HTML_Element* resolved_element)
{
	OP_ASSERT(m_loading_url->IsEmpty());

	const uni_char* family_name = GetFamilyName();
	if (resolved_element && family_name)
	{
		OP_ASSERT(GetFormat() == CSS_WebFont::FORMAT_SVG);

		OpFontInfo* fi = NULL;
		OP_STATUS err = CreateCSSFontDescriptor(&fi);
		if (OpStatus::IsSuccess(err))
		{
			err = g_webfont_manager->AddCSSWebFont(url, doc, resolved_element, fi);
			if (OpStatus::IsError(err))
			{
				// release fontnumber if we failed, but only if we allocated a new fontnumber
				if (!styleManager->GetFontInfo(fi->GetFontNumber()))
					styleManager->ReleaseFontNumber(fi->GetFontNumber());

				OP_DELETE(fi);
			}
		}

		if (OpStatus::IsSuccess(err))
		{
			SetLoadStatus(doc, WEBFONT_LOADED);
			m_used_element = resolved_element;
		}
		else
		{
			SetLoadStatus(doc, WEBFONT_NOTFOUND); // Doesn't really describe OOM for example...
		}
	}
	else
	{
		SetLoadStatus(doc, WEBFONT_NOTFOUND);
	}

	if (m_packed.load_status == WEBFONT_LOADED || m_packed.load_status == WEBFONT_NOTFOUND)
		if (!doc->IsReflowing())
			doc->GetHLDocProfile()->GetCSSCollection()->StyleChanged(CSSCollection::CHANGED_WEBFONT);
}

OP_STATUS CSS_FontfaceRule::SetLoadStatus(FramesDocument* doc, LoadStatus status)
{
	OP_ASSERT(m_packed.load_status == WEBFONT_LOADING && status != WEBFONT_LOADING && (GetFormat() & FORMAT_SVG));
	m_packed.load_status = status;
	if (status != WEBFONT_LOADED
	    // see CORE-37662: when loading the parent document via
	    // SVGWorkplaceImpl::LoadCSSResource CSS_FontfaceRule::OnLoad
	    // is called directly. this may cause recursion in combination
	    // with the format-mismatch fallback from the sfnt path. if
	    // this call originated from CSS_FontfaceRule::LoadingStopped
	    // LoadNextInSrc will be called from there.
	    && !m_packed.format_mismatch)
	{
		OP_STATUS stat = LoadNextInSrc(doc);
		if (OpStatus::IsError(stat))
		{
			m_packed.load_status = WEBFONT_NOTFOUND;
			return stat;
		}
	}
	return OpStatus::OK;
}

OP_STATUS CSS_FontfaceRule::Load(FramesDocument* doc)
{
	return m_packed.load_status == WEBFONT_NOTLOADED ? LoadNextInSrc(doc) : OpStatus::OK;
}

void CSS_FontfaceRule::LoadingRedirected(const URL& from, const URL& to)
{
	URL local_to = to;
	m_loading_url.SetURL(local_to);
}

void CSS_FontfaceRule::LoadingStopped(FramesDocument* doc, const URL& url)
{
	doc->StopLoadingInline(url, this);

	// let's not do anything more if we're deleting the document
	if (doc->IsBeingDeleted())
	{
		m_loading_url.UnsetURL();
		return;
	}

	URLStatus url_stat = url.Status(TRUE);

	UINT32 http_response;
	if (url.Type() == URL_HTTP || url.Type() == URL_HTTPS)
		http_response = url.GetAttribute(URL::KHTTP_Response_Code, URL::KFollowRedirect);
	else
		http_response = HTTP_OK;

	OP_STATUS stat = OpStatus::OK;

	if (url_stat == URL_LOADED && (http_response == HTTP_OK || http_response == HTTP_NOT_MODIFIED))
	{
		const uni_char* family_name = GetFamilyName();
		if (family_name)
		{
			OpFontInfo* fi = NULL;
			stat = CreateCSSFontDescriptor(&fi);
			if (OpStatus::IsSuccess(stat))
			{
				stat = g_webfont_manager->AddCSSWebFont(GetSrcURL(), doc, NULL, fi);
				if (OpStatus::IsError(stat))
				{
					// release fontnumber if we failed, but only if we allocated a new fontnumber
					if (!styleManager->GetFontInfo(fi->GetFontNumber()))
						styleManager->ReleaseFontNumber(fi->GetFontNumber());

					OP_DELETE(fi);

#if defined SVG_SUPPORT
					if (!m_packed.format_mismatch)
					{
						m_loading_url.UnsetURL();

						// loading this font as an sfnt (platform)
						// font failed. try to load it using the svg
						// path instead.
						m_packed.used_format = CSS_WebFont::FORMAT_SVG;
						m_packed.format_mismatch = TRUE;
						// check for not found below assumes this
						OP_ASSERT(m_packed.load_status != WEBFONT_NOTLOADED);
						stat = doc->GetLogicalDocument()->GetSVGWorkplace()->LoadCSSResource(url, this);
						// see CORE-37662: call to LoadCSSResource may
						// trigger direct call to
						// CSS_FontfaceRule::OnLoad if url redirects
						// to the parent document. in this case
						// LoadNextInSrc will not be called from
						// OnLoad, but should instead be called from
						// here.
						if (m_packed.load_status == WEBFONT_NOTFOUND)
							stat = OpStatus::ERR;
						// let OnLoad deal with updating status, triggering LoadNextInSrc etc
						if (OpStatus::IsSuccess(stat))
						{
							return;
						}
					}
#endif // SVG_SUPPORT
				}
			}

			if (stat == OpStatus::OK)
			{
				m_packed.load_status = WEBFONT_LOADED;
			}
		}
	}
	else if (url_stat == URL_LOADED)
	{
#ifdef OPERA_CONSOLE
		if (g_console->IsLogging())
		{
			OpConsoleEngine::Severity severity = OpConsoleEngine::Information;
			OpConsoleEngine::Source source = OpConsoleEngine::Internal;

			OpConsoleEngine::Message cmessage(source, severity);

			// Set the window id.
			if (doc->GetWindow())
				cmessage.window = doc->GetWindow()->Id();

			OP_STATUS err = cmessage.message.Set(UNI_L("Unable to load webfont"));
			err = url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, 0, cmessage.url);
			TRAP(err, g_console->PostMessageL(&cmessage));
			if (OpStatus::IsFatal(err))
				g_memory_manager->RaiseCondition(err);
		}
#endif // OPERA_CONSOLE
	}

	if (OpStatus::IsError(stat) || m_packed.load_status != WEBFONT_LOADED)
		m_loading_url.UnsetURL();

	if (OpStatus::IsMemoryError(stat))
	{
		m_packed.load_status = WEBFONT_NOTFOUND;
	}
	else if (m_packed.load_status != WEBFONT_LOADED)
	{
		stat = LoadNextInSrc(doc);
		if (OpStatus::IsError(stat))
			m_packed.load_status = WEBFONT_NOTFOUND;
	}

	if (m_packed.load_status == WEBFONT_LOADED || m_packed.load_status == WEBFONT_NOTFOUND)
		if (!doc->IsReflowing())
			doc->GetHLDocProfile()->GetCSSCollection()->StyleChanged(CSSCollection::CHANGED_WEBFONT);

	if (OpStatus::IsFatal(stat))
		g_memory_manager->RaiseCondition(stat);
}

OP_STATUS CSS_FontfaceRule::LoadNextInSrc(FramesDocument* doc)
{
	OP_ASSERT(!ExternalInlineListener::InList());

	m_used_url = URL();
	m_packed.format_mismatch = FALSE;

	OP_STATUS stat = GetNextInSrc(doc);
	if (OpStatus::IsMemoryError(stat))
	{
		m_packed.load_status = WEBFONT_NOTFOUND;
		return stat;
	}

	if (!m_used_url.IsEmpty())
	{
#ifdef URL_FILTER
		// Checks if content_filter rules allow this font to be loaded
		ServerName* doc_sn = m_used_url.GetServerName();
		URLFilterDOMListenerOverride lsn_over(doc->GetDOMEnvironment(), NULL);
		BOOL widget = doc && doc->GetWindow() && doc->GetWindow()->GetType() == WIN_TYPE_GADGET;
		HTMLLoadContext load_ctx(RESOURCE_FONT, doc_sn, &lsn_over, widget);
		BOOL font_allowed = FALSE;


		RETURN_IF_ERROR(g_urlfilter->CheckURL(&m_used_url, font_allowed, NULL,  &load_ctx));

		if (!font_allowed)
			return LoadNextInSrc(doc);
#endif // URL_FILTER

		CSS_WebFont::Format format = GetFormat();
		if ((format & (FORMAT_TRUETYPE | FORMAT_TRUETYPE_AAT | FORMAT_OPENTYPE | FORMAT_EMBEDDED_OPENTYPE | FORMAT_WOFF)) || format == FORMAT_UNKNOWN)
		{
			m_packed.load_status = WEBFONT_LOADING;
			OP_LOAD_INLINE_STATUS stat = doc->LoadInline(m_used_url, this, FALSE, TRUE, WEBFONT_INLINE);
			if (OpStatus::IsMemoryError(stat))
			{
				m_packed.load_status = WEBFONT_NOTFOUND;
				return stat;
			}
			else if (OpStatus::IsError(stat))
				return LoadNextInSrc(doc);

			m_loading_url.SetURL(m_used_url);
		}
		else if (format & FORMAT_SVG)
		{
#if defined SVG_SUPPORT
			m_packed.load_status = WEBFONT_LOADING;
			if (OpStatus::IsError(doc->GetLogicalDocument()->GetSVGWorkplace()->LoadCSSResource(m_used_url, this)))
			{
				m_packed.load_status = WEBFONT_NOTFOUND;
			}
#else
			m_packed.load_status = WEBFONT_NOTFOUND;
#endif // SVG_SUPPORT
		}
		else
			return LoadNextInSrc(doc);
	}
	else if (IsLocalFont())
	{
		OpFontInfo* fi = NULL;
		RETURN_IF_ERROR(CreateCSSFontDescriptor(&fi));
		OP_STATUS stat = g_webfont_manager->AddLocalFont(doc, fi, m_font_ref);
		if (OpStatus::IsError(stat))
		{
			// release fontnumber if we failed, but only if we allocated a new fontnumber
			if (!styleManager->GetFontInfo(fi->GetFontNumber()))
				styleManager->ReleaseFontNumber(fi->GetFontNumber());

			styleManager->GetFontManager()->RemoveWebFont(m_font_ref);
			m_font_ref = 0;
			OP_DELETE(fi);
			return stat;
		}

		m_packed.load_status = WEBFONT_LOADED;

		// At least two cases here:
		// 1. No StyleChanged needed since we got a valid local font right away (if there was no failed loading roundtrip involved)
		//    http://t/core/standards/css3/webfonts/standard/src-multiple-local-unexisting-local-existing.html
		// 2. We had a failed src: url() before local(), so we need to reload the css properties
		//    http://t/core/standards/css3/webfonts/standard/src-multiple-remote-unexisting-local-existing.html
		if (!doc->IsReflowing())
			doc->GetHLDocProfile()->GetCSSCollection()->StyleChanged(CSSCollection::CHANGED_WEBFONT);
	}
	else
	{
		m_packed.load_status = WEBFONT_NOTFOUND;

		// even if there was an error we should reload css properties, because we might be stuck with a bad font here already
		// - unless we finished checking all the src values and found no matches, in which case we can return during a reflow with
		// a correct result.
		if (!doc->IsReflowing())
			doc->GetHLDocProfile()->GetCSSCollection()->StyleChanged(CSSCollection::CHANGED_WEBFONT);
	}

	return OpStatus::OK;
}

OP_STATUS CSS_FontfaceRule::GetNextInSrc(FramesDocument* doc)
{
	CSS_property_list* props = GetPropertyList();
	CSS_decl* decl = props->GetFirstDecl();
	while (decl && decl->GetProperty() != CSS_PROPERTY_src)
		decl = decl->Suc();

	if (decl)
	{
		OP_ASSERT(decl->GetDeclType() == CSS_DECL_GEN_ARRAY);
		short arr_len = decl->ArrayValueLen();
		const CSS_generic_value* gen_arr = decl->GenArrayValue();

		while (++m_src_idx < arr_len)
		{
			if (gen_arr[m_src_idx].GetValueType() == CSS_FUNCTION_URL)
			{
				m_used_url = g_url_api->GetURL(gen_arr[m_src_idx].GetString(), doc->GetURL().GetContextId());
				if (m_src_idx+1 < arr_len && gen_arr[m_src_idx+1].GetValueType() == CSS_FUNCTION_FORMAT)
					m_packed.used_format = static_cast<CSS_WebFont::Format>(gen_arr[++m_src_idx].GetType());
				else
					m_packed.used_format = CSS_WebFont::FORMAT_UNKNOWN;

				OP_ASSERT(!m_packed.is_local);
				return OpStatus::OK;
			}
			else if (gen_arr[m_src_idx].GetValueType() == CSS_FUNCTION_LOCAL)
			{
				OP_ASSERT(m_font_ref == 0);
				OP_STATUS status = styleManager->GetFontManager()->GetLocalFont(m_font_ref, gen_arr[m_src_idx].GetString());
				if (OpStatus::IsSuccess(status) && m_font_ref)
				{
					m_packed.is_local = 1;
					return OpStatus::OK;
				}
				else if (OpStatus::IsMemoryError(status))
					return status;
			}
		}

		/* We're at the end of the src descriptor. */
		m_used_url = URL();
		m_packed.used_format = CSS_WebFont::FORMAT_UNKNOWN;
	}

	m_used_element = NULL;
	return OpStatus::OK;
}
