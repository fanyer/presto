/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/style/css_collection.h"
#include "modules/style/cssmanager.h"
#include "modules/probetools/probepoints.h"
#include "modules/style/css_style_attribute.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/attr_val.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/dochand/fdelm.h"
#include "modules/dochand/win.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/url/url_sn.h"
#include "modules/layout/layoutprops.h"
#include "modules/layout/frm.h"
#include "modules/media/mediatrack.h"
#include "modules/style/src/css_ruleset.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/style/css_viewport_meta.h"
#include "modules/style/css_svgfont.h"
#include "modules/svg/svg_workplace.h"

#include "modules/style/src/css_pseudo_stack.h"

#define DEFAULT_CELL_PADDING	1

/* virtual */
CSSCollection::~CSSCollection()
{
	OP_ASSERT(m_element_list.Empty());
	OP_ASSERT(m_pending_elements.Empty());

	m_font_prefetch_candidates.DeleteAll();
}

void CSSCollection::SetFramesDocument(FramesDocument* doc)
{
	m_doc = doc;
#ifdef STYLE_CSS_TRANSITIONS_API
	m_transition_manager.SetDocument(doc);
#endif // STYLE_CSS_TRANSITIONS_API

	if (doc && m_font_prefetch_limit == -1 && g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::EnableWebfonts, doc->GetHostName()))
	{
		m_font_prefetch_limit = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::PrefetchWebFonts, doc->GetHostName());
	}
}

void CSSCollection::AddCollectionElement(CSSCollectionElement* new_elm, BOOL commit)
{
	if (!commit)
	{
		new_elm->Into(&m_pending_elements);
		return;
	}

	HTML_Element* new_he = new_elm->GetHtmlElement();
	OP_ASSERT(new_he);
	new_he->SetReferenced(TRUE);

	BOOL is_stylesheet = new_elm->IsStyleSheet();
	BOOL is_import = is_stylesheet && static_cast<CSS*>(new_elm)->IsImport();
	BOOL check_skip = is_stylesheet && !is_import;

	CSSCollectionElement* elm = m_element_list.First();

	CSS* prev_top = NULL;

	while (elm)
	{
		HTML_Element* he = elm->GetHtmlElement();
		if (he && ((is_import && he->IsAncestorOf(new_he)) || new_he->Precedes(he)))
		{
			new_elm->Precede(elm);
			break;
		}

		if (check_skip && elm->IsStyleSheet() && !static_cast<CSS*>(elm)->IsImport())
			prev_top = static_cast<CSS*>(elm);

		elm = elm->Suc();
	}

	if (!elm)
		new_elm->Into(&m_element_list);

	if (check_skip)
	{
		CSS* new_css = static_cast<CSS*>(new_elm);
		if (prev_top && prev_top->IsSame(m_doc->GetLogicalDocument(), new_css))
		{
			if (prev_top->Skip())
				new_css->SetSkip(TRUE);
			else
				prev_top->SetSkip(TRUE);
		}
		else
		{
			while (elm && (!elm->IsStyleSheet() || static_cast<CSS*>(elm)->IsImport()))
				elm = static_cast<CSSCollectionElement*>(elm->Suc());

			if (elm)
			{
				CSS* next_top = static_cast<CSS*>(elm);
				if (next_top->IsSame(m_doc->GetLogicalDocument(), new_css))
					new_css->SetSkip(TRUE);
				else if (prev_top)
					prev_top->SetSkip(FALSE);
			}
		}
	}

	new_elm->Added(this, m_doc);
}

CSSCollectionElement*
CSSCollection::RemoveCollectionElement(HTML_Element* he)
{
	BOOL in_pending = FALSE;

	CSSCollectionElement* elm = m_element_list.First();

	if (!elm)
	{
		elm = m_pending_elements.First();
		in_pending = TRUE;
	}

	while (elm)
	{
		HTML_Element* elm_he = elm->GetHtmlElement();

		if (elm_he == he)
		{
			if (elm->IsStyleSheet())
			{
				CSS* css = static_cast<CSS*>(elm);
				if (!css->Skip() && !css->IsImport() && m_element_list.HasLink(css))
				{
					CSSCollectionElement* elm = static_cast<CSSCollectionElement*>(css->Pred());
					while (elm && (!elm->IsStyleSheet() || static_cast<CSS*>(elm)->IsImport()))
						elm = static_cast<CSSCollectionElement*>(elm->Pred());

					if (elm)
					{
						CSS* prev_top = static_cast<CSS*>(elm);
						if (prev_top->IsSame(m_doc->GetLogicalDocument(), css))
						{
							OP_ASSERT(prev_top->Skip());
							prev_top->SetSkip(FALSE);
						}
					}
				}
			}
			elm->Out();
			elm->Removed(this, m_doc);
			return elm;
		}
		elm = elm->Suc();
		if (!elm && !in_pending)
		{
			elm = m_pending_elements.First();
			in_pending = TRUE;
		}
	}

	return NULL;
}

void CSSCollection::CommitAdded()
{
	CSSCollectionElement* elm = m_pending_elements.First();
	while (elm)
	{
		CSSCollectionElement* next = elm->Suc();
		elm->Out();
		AddCollectionElement(elm);
		elm = next;
	}

	OP_ASSERT(m_pending_elements.Empty());
}

void CSSCollection::StyleChanged(unsigned int changes)
{
	if (m_doc->IsBeingFreed())
		// Don't signal the document if it's being freed.
		return;

	HTML_Element* root = m_doc->GetDocRoot();
	if (root)
	{
		if (changes & (CHANGED_PROPS|CHANGED_WEBFONT|CHANGED_KEYFRAMES))
		{
			if (root->IsPropsDirty())
				m_doc->PostReflowMsg();
			else
				root->MarkPropsDirty(m_doc);

#ifdef SVG_SUPPORT
			if (changes & CHANGED_WEBFONT)
				/* SVG doesn't yet set attribute styles during the default
				   style loading stage, as HTML does.

				   This means that marking properties dirty alone isn't
				   enough, we need to send an extra notification to the SVG
				   code that WebFonts have changed.

				   This work-around can be removed when the SVG attribute
				   styles are set in GetDefaultStyleProperties(). */

				m_doc->GetLogicalDocument()->GetSVGWorkplace()->InvalidateFonts();
#endif // SVG_SUPPORT
		}

#ifdef CSS_VIEWPORT_SUPPORT
		if (changes & CHANGED_VIEWPORT)
		{
			m_viewport.MarkDirty();
			root->MarkDirty(m_doc);
		}
#endif // CSS_VIEWPORT_SUPPORT

		if (changes & CHANGED_MEDIA_QUERIES)
			EvaluateMediaQueryLists();
	}
}

void CSSCollection::MarkAffectedElementsPropsDirty(CSS* css)
{
	HLDocProfile* hld_prof = m_doc->GetHLDocProfile();
	HTML_Element* element = m_doc->GetDocRoot();

	if (element && element->IsPropsDirty())
	{
		m_doc->PostReflowMsg();
		return;
	}

	CSS_MatchContext::Operation context_op(m_match_context, m_doc, CSS_MatchContext::TOP_DOWN, m_doc->GetMediaType(), TRUE, TRUE);

	while (element)
	{
		if (Markup::IsRealElement(element->Type()) && element->IsIncludedActualStyle())
		{
			if (!m_match_context->GetLeafElm(element))
			{
				hld_prof->SetIsOutOfMemory(TRUE);
				break;
			}

			if (!element->IsPropsDirty())
			{
				g_css_pseudo_stack->ClearHasPseudo();
				if (element->HasBeforeOrAfter() || css->GetProperties(m_match_context, NULL, 0) || g_css_pseudo_stack->HasPseudo())
					element->MarkPropsDirty(m_doc);
			}
		}

		element = context_op.Next(element);
	}
}

OP_STATUS CSSCollection::PrefetchURLs()
{
	CSS_MediaType media_type = m_doc->GetMediaType();

	RETURN_IF_ERROR(GenerateStyleSheetArray(media_type));

	CSS_MatchContext::Operation context_op(m_match_context, m_doc, CSS_MatchContext::TOP_DOWN, media_type, TRUE, FALSE);

	if (!m_doc->GetLoadImages() || !m_match_context->ApplyAuthorStyle())
		return OpStatus::OK;

	m_match_context->SetPrefetchMode();

	CSS_Properties css_properties;

	HTML_Element* element = m_doc->GetDocRoot();
	while (element)
	{
		if (Markup::IsRealElement(element->Type()) && element->IsIncludedActualStyle())
		{
			if (!m_match_context->GetLeafElm(element))
				return OpStatus::ERR_NO_MEMORY;

			css_properties.ResetProperty(CSS_PROPERTY_content);
			css_properties.ResetProperty(CSS_PROPERTY__o_border_image);
			css_properties.ResetProperty(CSS_PROPERTY_background_image);
			css_properties.ResetProperty(CSS_PROPERTY_list_style_image);

			/* Get declarations from style attribute. */
			CSS_property_list* pl = element->GetCSS_Style();
			if (pl)
			{
				CSS_decl* css_decl = pl->GetLastDecl();
				while (css_decl)
				{
					css_properties.SelectProperty(css_decl, STYLE_SPECIFICITY, 0, m_match_context->ApplyPartially());
					css_decl = css_decl->Pred();
				}
			}

			for (unsigned int stylesheet_idx = FIRST_AUTHOR_STYLESHEET_IDX; stylesheet_idx < m_stylesheet_array.GetCount(); stylesheet_idx++)
				m_stylesheet_array.Get(stylesheet_idx)->GetProperties(m_match_context, &css_properties, stylesheet_idx);

			CSS_decl* decl;

			if ((decl = css_properties.GetDecl(CSS_PROPERTY_content)))
				RETURN_IF_ERROR(decl->PrefetchURLs(m_doc));
			if ((decl = css_properties.GetDecl(CSS_PROPERTY__o_border_image)))
				RETURN_IF_ERROR(decl->PrefetchURLs(m_doc));
			if ((decl = css_properties.GetDecl(CSS_PROPERTY_background_image)))
				RETURN_IF_ERROR(decl->PrefetchURLs(m_doc));
			if ((decl = css_properties.GetDecl(CSS_PROPERTY_list_style_image)))
				RETURN_IF_ERROR(decl->PrefetchURLs(m_doc));
		}

		element = context_op.Next(element);
	}

	return OpStatus::OK;
}

#ifdef STYLE_GETMATCHINGRULES_API

/** Resolve the CSS_MatchElement to the true HTML_Element it's referring to.

    The returned element is not guaranteed to live outside the cascade, as
    it may have been inserted by LayoutProperties::AutoFirstLine.

    If @ elm is valid, this function *should not* return NULL, but it still
    can if the HTML_Element is missing from the tree. It's bug if this
    happens, but calling code should still handle such a situation gracefully.

    @see LayoutProperties::AutoFirstLine

    @param elm The CSS_MatchElement to resolve.
    @return The ultimate HTML_Element we're referring to, or NULL. */
static HTML_Element*
ResolveMatchElement(const CSS_MatchElement& elm)
{
	HTML_Element* html_elm = elm.GetHtmlElement();
	PseudoElementType pseudo = elm.GetPseudoElementType();

	switch (pseudo)
	{
	case ELM_PSEUDO_BEFORE:
	case ELM_PSEUDO_AFTER:
	case ELM_PSEUDO_FIRST_LETTER:
		{
			HTML_Element* ret = html_elm->FirstChild();

			while (ret)
			{
				if (ret->GetPseudoElement() == pseudo)
					return ret;
				else
					ret = ret->NextSibling();
			}

			return NULL;
		}
	case ELM_PSEUDO_FIRST_LINE:
		return html_elm->Pred();
	case ELM_NOT_PSEUDO:
		return html_elm;
	case ELM_PSEUDO_SELECTION:
	case ELM_PSEUDO_LIST_MARKER:
		// Note: ELM_PSEUDO_SELECTION and ELM_PSEUDO_LIST_MARKER is
		// not supported.
	default:
		return NULL;
	}
}

/** Automatically inserts CSS_MatchRuleListener on construction, and
    automatically removes it on destruction. */
class CSS_AutoListener
{
public:
	CSS_AutoListener(CSS_MatchContext* context, CSS_MatchRuleListener* listener)
		: context(context) { context->SetListener(listener); }
	~CSS_AutoListener() { context->SetListener(NULL); }
private:
	CSS_MatchContext* context;
};

OP_STATUS CSSCollection::GetMatchingStyleRules(const CSS_MatchElement& element, CSS_Properties* css_properties, CSS_MediaType media_type, BOOL include_inherited, CSS_MatchRuleListener* listener)
{
	HLDocProfile* hld_prof = m_doc->GetHLDocProfile();
	RETURN_IF_ERROR(m_doc->ConstructDOMEnvironment());

	CSS_AutoListener auto_listener(m_match_context, listener);

	LayoutProperties::AutoFirstLine first_line(hld_prof);

	if (element.GetPseudoElementType() == ELM_PSEUDO_FIRST_LINE)
		first_line.Insert(element.GetHtmlElement());

	if (!element.IsValid())
		return OpStatus::ERR;

	if (DOM_Environment* environment = m_doc->GetDOMEnvironment())
	{
		if (environment->IsEnabled())
		{
			CSS_MatchElement cur_elm = element;
			do
			{
				HTML_Element* elm = ResolveMatchElement(cur_elm);

				if (!elm)
				{
					// It's a bug if this happens, but handle it gracefully if
					// it does.
					OP_ASSERT(!"Valid element, but NULL returned");
					break;
				}

				CSS_Properties props;
				RETURN_IF_MEMORY_ERROR(GetProperties(elm, g_op_time_info->GetRuntimeMS(), &props, media_type));

				if (hld_prof->GetIsOutOfMemory())
					return OpStatus::ERR_NO_MEMORY;
				css_properties->AddDeclsFrom(props, cur_elm.IsEqual(element));
				cur_elm = cur_elm.GetParent();
			} while (include_inherited && cur_elm.IsValid());
		}
	}

	return OpStatus::OK;
}

#endif // STYLE_GETMATCHINGRULES_API

OP_STATUS
CSSCollection::GenerateStyleSheetArray(CSS_MediaType media_type)
{
	m_stylesheet_array.Empty();

	const uni_char* hostname = m_doc->GetURL().GetAttribute(URL::KUniHostName).CStr();

#ifdef PREFS_HOSTOVERRIDE
	if (hostname && !m_overrides_loaded)
	{
		m_overrides_loaded = TRUE;
		TRAP_AND_RETURN(err, CSSCollection::LoadHostOverridesL(hostname));
	}
#endif // PREFS_HOSTOVERRIDE

	HLDocProfile* hld_prof = m_doc->GetHLDocProfile();
	LayoutMode layout_mode = m_doc->GetWindow()->GetLayoutMode();
	BOOL is_ssr = layout_mode == LAYOUT_SSR;
	BOOL use_style = (is_ssr && hld_prof->HasMediaStyle(CSS_MEDIA_TYPE_HANDHELD) && hld_prof->GetCSSMode() == CSS_FULL ||
					  !is_ssr && g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(hld_prof->GetCSSMode(), PrefsCollectionDisplay::EnableAuthorCSS), hostname)) || m_doc->GetWindow()->IsCustomWindow();

#ifdef _WML_SUPPORT_
# ifdef PREFS_HOSTOVERRIDE
	CSS* wml_css = m_host_overrides[CSSManager::WML_CSS].css;
	if (!wml_css)
		wml_css = g_cssManager->GetCSS(CSSManager::WML_CSS);
# else
	CSS* wml_css = g_cssManager->GetCSS(CSSManager::WML_CSS);
# endif
	RETURN_IF_ERROR(m_stylesheet_array.Add(wml_css));
#endif // _WML_SUPPORT_

#if defined(CSS_MATHML_STYLESHEET) && defined(MATHML_ELEMENT_CODES)
	CSS* mathml_css = g_cssManager->GetCSS(CSSManager::MathML_CSS);
	RETURN_IF_ERROR(m_stylesheet_array.Add(mathml_css));
#endif // CSS_MATHML_STYLESHEET && MATHML_ELEMENT_CODES

	if (use_style)
	{
		CSSCollectionElement* elm = m_element_list.Last();
		CSS* top_css = NULL;
		BOOL may_skip_next = TRUE;
		while (elm)
		{
			if (elm->IsStyleSheet())
			{
				CSS* css = static_cast<CSS*>(elm);
				BOOL apply_sheet = css->IsEnabled() && css->CheckMedia(m_doc, media_type);
				if (!css->IsImport())
				{
					top_css = css;
					if (apply_sheet && !css->IsModified())
						may_skip_next = TRUE;
					else
						if (!css->Skip())
							may_skip_next = FALSE;
				}

				// All imported stylesheets must have an ascendent stylesheet which is stored as top_css.
				OP_ASSERT(top_css != NULL);

				BOOL skip_children = TRUE;
				if (apply_sheet)
				{
					RETURN_IF_ERROR(m_stylesheet_array.Add(css));
					skip_children = FALSE;
				}

				elm = css->GetNextImport(skip_children);
				if (!elm)
				{
					elm = top_css;
					while ((elm = static_cast<CSSCollectionElement*>(elm->Pred())) &&
							elm->IsStyleSheet() &&
							(static_cast<CSS*>(elm)->IsImport() || may_skip_next && static_cast<CSS*>(elm)->Skip()))
						;
				}
			}
			else
				elm = elm->Pred();
		}
	}

	if (((is_ssr && hld_prof->GetCSSMode() == CSS_NONE) ||
		 (!is_ssr && g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(hld_prof->GetCSSMode(), PrefsCollectionDisplay::EnableUserCSS), hostname))) &&
		(m_doc->GetWindow()->IsNormalWindow() || m_doc->GetWindow()->IsThumbnailWindow()))
	{
#ifdef LOCAL_CSS_FILES_SUPPORT
		// Load all user style sheets defined in preferences
		for (unsigned int j=0; j < g_pcfiles->GetLocalCSSCount(); j++)
		{
			CSS* css = g_cssManager->GetCSS(CSSManager::FirstUserStyle+j);
			while (css)
			{
				BOOL skip_children = TRUE;
				if (css->IsEnabled() && css->CheckMedia(m_doc, media_type))
				{
					RETURN_IF_ERROR(m_stylesheet_array.Add(css));
					skip_children = FALSE;
				}
				css = css->GetNextImport(skip_children);
			}
		}

# ifdef EXTENSION_SUPPORT
		// Load all user style sheets defined by extensions
		for (CSSManager::ExtensionStylesheet* extcss = g_cssManager->GetExtensionUserCSS(); extcss;
			 extcss = reinterpret_cast<CSSManager::ExtensionStylesheet*>(extcss->Suc()))
		{
			CSS* css = extcss->css;
			while (css)
			{
				BOOL skip_children = TRUE;
				if (css->CheckMedia(m_doc, media_type))
				{
					RETURN_IF_ERROR(m_stylesheet_array.Add(css));
					skip_children = FALSE;
				}
				css = css->GetNextImport(skip_children);
			}
		}
# endif
#endif // LOCAL_CSS_FILES_SUPPORT

#ifdef PREFS_HOSTOVERRIDE
		CSS* css = m_host_overrides[CSSManager::LocalCSS].css;
		if (!css)
			css = g_cssManager->GetCSS(CSSManager::LocalCSS);
#else
		CSS* css = g_cssManager->GetCSS(CSSManager::LocalCSS);
#endif
		while (css)
		{
			BOOL skip_children = TRUE;
			if (css->IsEnabled() && css->CheckMedia(m_doc, media_type))
			{
				RETURN_IF_ERROR(m_stylesheet_array.Add(css));
				skip_children = FALSE;
			}
			css = css->GetNextImport(skip_children);
		}
	}

#ifdef PREFS_HOSTOVERRIDE
	CSS* browser_css = m_host_overrides[CSSManager::BrowserCSS].css;
	if (!browser_css)
		browser_css = g_cssManager->GetCSS(CSSManager::BrowserCSS);
#else
	CSS* browser_css = g_cssManager->GetCSS(CSSManager::BrowserCSS);
#endif
	while (browser_css)
	{
		BOOL skip_children = TRUE;
		if (browser_css->IsEnabled() && browser_css->CheckMedia(m_doc, media_type))
		{
			RETURN_IF_ERROR(m_stylesheet_array.Add(browser_css));
			skip_children = FALSE;
		}
		browser_css = browser_css->GetNextImport(skip_children);
	}

#ifdef SUPPORT_VISUAL_ADBLOCK
	BOOL content_block_mode = m_doc->GetWindow()->GetContentBlockEditMode();
	if (content_block_mode)
	{
# ifdef PREFS_HOSTOVERRIDE
		CSS* block_css = m_host_overrides[CSSManager::ContentBlockCSS].css;
		if (!block_css)
			block_css = g_cssManager->GetCSS(CSSManager::ContentBlockCSS);
# else
		CSS* block_css = g_cssManager->GetCSS(CSSManager::ContentBlockCSS);
# endif
		if (block_css)
			RETURN_IF_ERROR(m_stylesheet_array.Add(block_css));
	}
#endif // SUPPORT_VISUAL_ADBLOCK

	return OpStatus::OK;
}

OP_STATUS
CSSCollection::GetProperties(HTML_Element* he, double runtime_ms, CSS_Properties* css_properties, CSS_MediaType media_type)
{
	OP_PROBE5(OP_PROBE_CSSCOLLECTION_GETPROPERTIES);

#ifdef STYLE_GETMATCHINGRULES_API
	CSS_MatchRuleListener* listener = m_match_context->Listener();
#endif // STYLE_GETMATCHINGRULES_API

	if (m_match_context->GetMode() == CSS_MatchContext::INACTIVE)
		RETURN_IF_ERROR(GenerateStyleSheetArray(media_type));

	CSS_MatchContext::Operation context_op(m_match_context, m_doc, CSS_MatchContext::BOTTOM_UP, media_type, TRUE, TRUE);

	g_css_pseudo_stack->ClearHasPseudo();

	PseudoElementType pseudo_elm = he->GetPseudoElement();

	m_match_context->SetPseudoElm(pseudo_elm);

	CSS_MatchContextElm* context_elm = NULL;
	HTML_Element* match_he = he;

	if (pseudo_elm == ELM_NOT_PSEUDO)
	{
		context_elm = m_match_context->GetLeafElm(match_he);

		/* Get declarations from the style attribute. */

		if (m_match_context->ApplyAuthorStyle() && css_properties)
		{
			CSS_property_list* pl = he->GetCSS_Style();

			/* Skip property list if empty, but not for Scope
			   which expects emission of empty style attributes. */

			if (pl && (!pl->IsEmpty()
#ifdef STYLE_GETMATCHINGRULES_API
				|| listener
#endif // STYLE_GETMATCHINGRULES_API
				))
			{
#ifdef STYLE_GETMATCHINGRULES_API
# ifdef STYLE_MATCH_INLINE_DEFAULT_API
				if (listener)
				{
					CSS_MatchElement elm(context_elm->HtmlElement(), m_match_context->PseudoElm());
					RETURN_IF_MEMORY_ERROR(listener->InlineStyle(elm, pl));
				}
# endif // STYLE_MATCH_INLINE_DEFAULT_API
#endif //STYLE_GETMATCHINGRULES_API

				CSS_decl* css_decl = pl->GetLastDecl();
				while (css_decl)
				{
					css_properties->SelectProperty(css_decl, STYLE_SPECIFICITY, 0, m_match_context->ApplyPartially());
					css_decl = css_decl->Pred();
				}
			}
		}
	}
	else
	{
		/* Find the real element this is a pseudo element for.
		   It's the real element that is actually being matched against
		   the selector when you ignore the pseudo element at the end. */

		match_he = pseudo_elm == ELM_PSEUDO_FIRST_LINE ? match_he->Suc() : match_he->ParentActualStyle();

		if (pseudo_elm == ELM_PSEUDO_FIRST_LETTER || pseudo_elm == ELM_PSEUDO_FIRST_LINE)
		{
			if (match_he->GetIsPseudoElement())
				match_he = match_he->ParentActualStyle();

			while (match_he && match_he->Type() != he->Type())
				match_he = match_he->ParentActualStyle();
		}

		// This should never fail
		OP_ASSERT(match_he);

		context_elm = m_match_context->FindOrGetLeafElm(match_he);
	}

	if (!context_elm)
		return OpStatus::ERR_NO_MEMORY;

	unsigned int stylesheet_idx = FIRST_AUTHOR_STYLESHEET_IDX;

	for (; stylesheet_idx < m_stylesheet_array.GetCount(); stylesheet_idx++)
		m_stylesheet_array.Get(stylesheet_idx)->GetProperties(m_match_context, css_properties, stylesheet_idx);

#if defined(_WML_SUPPORT_) || defined(CSS_MATHML_STYLESHEET)
	NS_Type ns_type = he->GetNsType();
	CSS* css;
# ifdef _WML_SUPPORT_
	if (ns_type == NS_WML && (css = m_stylesheet_array.Get(WML_STYLESHEET_IDX)))
		css->GetProperties(m_match_context, css_properties, stylesheet_idx++);
# endif // _WML_SUPPORT_

# if defined(CSS_MATHML_STYLESHEET) && defined(MATHML_ELEMENT_CODES)
	if (ns_type == NS_MATHML && (css = m_stylesheet_array.Get(MATHML_STYLESHEET_IDX)))
		css->GetProperties(m_match_context, css_properties, stylesheet_idx++);
# endif // CSS_MATHML_STYLESHEET && MATHML_ELEMENT_CODES
#endif // _WML_SUPPORT_ || CSS_MATHML_STYLESHEET

	if (css_properties && m_match_context->ApplyPartially())
	{
		switch (g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(m_doc->GetPrefsRenderingMode(), PrefsCollectionDisplay::HonorHidden)))
		{
		case 1:
			{
				const uni_char* hostname = m_doc->GetURL().GetAttribute(URL::KUniHostName).CStr();
				if (g_pcjs->GetIntegerPref(PrefsCollectionJS::EcmaScriptEnabled, hostname))
					break;
			}
		case 2:
			{
				if (CSS_decl* cp = css_properties->GetDecl(CSS_PROPERTY_display))
					if (cp->TypeValue() == CSS_VALUE_none)
						css_properties->SetDecl(CSS_PROPERTY_display, NULL);
				if (css_properties->GetDecl(CSS_PROPERTY_visibility))
					css_properties->SetDecl(CSS_PROPERTY_visibility, NULL);
			}
			break;
		}
	}

	if (css_properties && !he->GetIsPseudoElement())
	{
		RETURN_IF_ERROR(GetDefaultStyleProperties(context_elm, css_properties));

# ifdef STYLE_GETMATCHINGRULES_API
#  ifdef STYLE_MATCH_INLINE_DEFAULT_API
		if (listener)
		{
			StyleModule::DefaultStyle* default_style = &g_opera->style_module.default_style;
			CSS_Properties default_properties;
			RETURN_IF_ERROR(GetDefaultStyleProperties(context_elm, &default_properties));

			// Create temporary CSS_property_list and populate with declarations from default_style.
			CSS_property_list props;
			CSS_decl* decl;
			for (int i = 0; i < CSS_PROPERTIES_LENGTH; i++)
			{
				decl = default_properties.GetDecl(i);
				if (decl)
				{
					if (i < CSS_PROPERTY_NAME_SIZE || i == CSS_PROPERTY_font_color)
					{
						CSS_decl* default_decl = decl;

						if (!default_decl->GetDefaultStyle())
							// Declarations not being 'default style' declarations can't be put in
							// the temporary property list.  Get a proxy object and put that in the
							// list instead.
							default_decl = default_style->GetProxyDeclaration(i, decl);

						if (default_decl)
							props.AddDecl(default_decl, FALSE, CSS_decl::ORIGIN_USER_AGENT);
					}
				}
			}

			if (!props.IsEmpty())
			{
				CSS_MatchElement elm(context_elm->HtmlElement(), m_match_context->PseudoElm());
				OP_STATUS stat = listener->DefaultStyle(elm, &props);

				if (OpStatus::IsMemoryError(stat))
					return stat;
			}
		}
#  endif // STYLE_MATCH_INLINE_DEFAULT_API
# endif // STYLE_GETMATCHINGRULES_API

#ifdef CSS_ANIMATIONS
		m_animation_manager.ApplyAnimations(he, runtime_ms, css_properties, m_match_context->ApplyPartially());
#endif
	}

	if (context_elm->Type() == Markup::HTE_IMG && (m_match_context->DocLayoutMode() == LAYOUT_SSR || m_match_context->DocLayoutMode() == LAYOUT_CSSR))
	{
		PrefsCollectionDisplay::RenderingModes rendering_mode = m_match_context->DocLayoutMode() == LAYOUT_SSR ? PrefsCollectionDisplay::SSR : PrefsCollectionDisplay::CSSR;
		if (g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::RemoveOrnamentalImages)) ||
			g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::RemoveLargeImages)) ||
			g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::UseAltForCertainImages)))
			he->SetHasRealSizeDependentCss(TRUE);
	}

	int has_pseudo = g_css_pseudo_stack->HasPseudo();

	if (has_pseudo)
		match_he->SetCheckForPseudo(has_pseudo);

#ifdef DISABLE_HIGHLIGHT_ON_PSEUDO_CLASS
	if (HTML_Document* html_doc = m_doc->GetHtmlDocument())
		if (html_doc->GetNavigationElement() == match_he)
		{
			BOOL prevent_highlight = context_elm->IsHoveredWithMatch() || context_elm->IsFocusedWithMatch()
# ifdef SELECT_TO_FOCUS_INPUT
				|| context_elm->IsPreFocusedWithMatch()
# endif // SELECT_TO_FOCUS_INPUT
				;

			if (!prevent_highlight)
			{
				/* Check whether some other matched context elm in chain has a :hover
				   pseudo class in the simple selector. We check only :hover, because
				   it is the only pseudo class that when being active on an element,
				   is also active on ancestors. */

				CSS_MatchContextElm* iter = context_elm->Parent();

				while (iter)
				{
					if (iter->IsHoveredWithMatch())
					{
						prevent_highlight = TRUE;
						break;
					}

					iter = iter->Parent();
				}
			}

			if (prevent_highlight)
			{
				if (html_doc->IsNavigationElementHighlighted() && m_doc->GetTextSelection())
					/* This is the case when the navigation element is highlighted by selecting
					   the text inside it. We need to clear the text selection now, because
					   during the paint routine the text selection will always be painted
					   (regardless of navigation element highlighted flag in HTML_Document). */

					m_doc->ClearDocumentSelection(FALSE);

				html_doc->SetNavigationElementHighlighted(FALSE);
			}
		}
#endif // DISABLE_HIGHLIGHT_ON_PSEUDO_CLASS

	return OpStatus::OK;
}

#ifdef PAGED_MEDIA_SUPPORT

void CSSCollection::GetPageProperties(CSS_Properties* css_properties, BOOL first_page, BOOL left_page, CSS_MediaType media_type)
{
	HLDocProfile* hld_prof = m_doc->GetHLDocProfile();

	const uni_char* hostname = m_doc->GetURL().GetAttribute(URL::KUniHostName).CStr();

	unsigned int stylesheet_num = 0;

	if (g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(hld_prof->GetCSSMode(), PrefsCollectionDisplay::EnableAuthorCSS), hostname) || m_doc->GetWindow()->IsCustomWindow())
	{
		CSSCollectionElement* elm = m_element_list.Last();
		CSS* top_css = NULL;
		while (elm)
		{
			if (elm->IsStyleSheet())
			{
				CSS* css = static_cast<CSS*>(elm);
				if (!css->IsImport())
					top_css = css;

				// All imported stylesheets must have an ascendent stylesheet which is stored as top_css.
				OP_ASSERT(top_css != NULL);

				BOOL skip_children = TRUE;
				if (css->IsEnabled() && css->CheckMedia(m_doc, media_type))
				{
					css->GetPageProperties(m_doc, css_properties, stylesheet_num++, first_page, left_page, media_type);
					skip_children = FALSE;
				}

				elm = css->GetNextImport(skip_children);
				if (!elm)
				{
					elm = top_css;
					while ((elm = static_cast<CSSCollectionElement*>(elm->Pred())) && elm->IsStyleSheet() && static_cast<CSS*>(elm)->IsImport())
						;
				}
			}
			else
				elm = elm->Pred();
		}
	}

	if (g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(hld_prof->GetCSSMode(), PrefsCollectionDisplay::EnableUserCSS), hostname)
		&& (m_doc->GetWindow()->IsNormalWindow() || m_doc->GetWindow()->IsThumbnailWindow()))
	{
#ifdef LOCAL_CSS_FILES_SUPPORT
		// Load all user style sheets defined in preferences
		for (unsigned int j=0; j < g_pcfiles->GetLocalCSSCount(); j++)
		{
			CSS* css = g_cssManager->GetCSS(CSSManager::FirstUserStyle+j);
			while (css)
			{
				BOOL skip_children = TRUE;
				if (css->IsEnabled() && css->CheckMedia(m_doc, media_type))
				{
					css->GetPageProperties(m_doc, css_properties, stylesheet_num++, first_page, left_page, media_type);
					skip_children = FALSE;
				}
				css = css->GetNextImport(skip_children);
			}
		}

# ifdef EXTENSION_SUPPORT
		// Load all user style sheets defined by extensions
		for (CSSManager::ExtensionStylesheet* extcss = g_cssManager->GetExtensionUserCSS(); extcss;
			 extcss = reinterpret_cast<CSSManager::ExtensionStylesheet*>(extcss->Suc()))
		{
			CSS* css = extcss->css;
			while (css)
			{
				BOOL skip_children = TRUE;
				if (css->IsEnabled() && css->CheckMedia(m_doc, media_type))
				{
					css->GetPageProperties(m_doc, css_properties, stylesheet_num++, first_page, left_page, media_type);
					skip_children = FALSE;
				}
				css = css->GetNextImport(skip_children);
			}
		}
# endif
#endif // LOCAL_CSS_FILES_SUPPORT
#ifdef PREFS_HOSTOVERRIDE
		CSS* css = m_host_overrides[CSSManager::LocalCSS].css;
		if (!css)
			css = g_cssManager->GetCSS(CSSManager::LocalCSS);
#else
		CSS* css = g_cssManager->GetCSS(CSSManager::LocalCSS);
#endif
		while (css)
		{
			BOOL skip_children = TRUE;
			if (css->IsEnabled() && css->CheckMedia(m_doc, media_type))
			{
				css->GetPageProperties(m_doc, css_properties, stylesheet_num++, first_page, left_page, media_type);
				skip_children = FALSE;
			}
			css = css->GetNextImport(skip_children);
		}
	}

#ifdef _WML_SUPPORT_
	if (hld_prof->IsWml())
	{
# ifdef PREFS_HOSTOVERRIDE
		CSS* css = m_host_overrides[CSSManager::WML_CSS].css;
		if (!css)
			css = g_cssManager->GetCSS(CSSManager::WML_CSS);
# else
		CSS* css = g_cssManager->GetCSS(CSSManager::WML_CSS);
# endif
		if (css)
			css->GetPageProperties(m_doc, css_properties, stylesheet_num++, first_page, left_page, media_type);
	}
#endif

#if defined(CSS_MATHML_STYLESHEET) && defined(MATHML_ELEMENT_CODES)
	CSS* css = g_cssManager->GetCSS(CSSManager::MathML_CSS);
	if (css)
		css->GetPageProperties(m_doc, css_properties, stylesheet_num++, first_page, left_page, media_type);
#endif // CSS_MATHML_STYLESHEET && MATHML_ELEMENT_CODES

#ifdef PREFS_HOSTOVERRIDE
	CSS* browser_css = m_host_overrides[CSSManager::BrowserCSS].css;
	if (!browser_css)
		browser_css = g_cssManager->GetCSS(CSSManager::BrowserCSS);
#else
	CSS* browser_css = g_cssManager->GetCSS(CSSManager::BrowserCSS);
#endif
	while (browser_css)
	{
		BOOL skip_children = TRUE;
		if (browser_css->IsEnabled() && browser_css->CheckMedia(m_doc, media_type))
		{
			browser_css->GetPageProperties(m_doc, css_properties, stylesheet_num++, first_page, left_page, media_type);
			skip_children = FALSE;
		}
		browser_css = browser_css->GetNextImport(skip_children);
	}

#ifdef SUPPORT_VISUAL_ADBLOCK
	BOOL content_block_mode = m_doc->GetWindow()->GetContentBlockEditMode();
	if (content_block_mode)
	{
# ifdef PREFS_HOSTOVERRIDE
		CSS* block_css = m_host_overrides[CSSManager::ContentBlockCSS].css;
		if (!block_css)
			block_css = g_cssManager->GetCSS(CSSManager::ContentBlockCSS);
# else
		CSS* block_css = g_cssManager->GetCSS(CSSManager::ContentBlockCSS);
# endif
		if (block_css)
			block_css->GetPageProperties(m_doc, css_properties, stylesheet_num++, first_page, left_page, media_type);
	}
#endif // SUPPORT_VISUAL_ADBLOCK
}

#endif // PAGED_MEDIA_SUPPORT

CSS_WebFont*
CSSCollection::GetWebFont(const uni_char* family_name, CSS_MediaType media_type)
{
	CSSCollectionElement* elm = m_element_list.Last();
	CSS* top_css = NULL;
	while (elm)
	{
		if (elm->IsStyleSheet())
		{
			CSS* css = static_cast<CSS*>(elm);
			if (!css->IsImport())
				top_css = css;

			// All imported stylesheets must have an ascendent stylesheet which is stored as top_css.
			OP_ASSERT(top_css != NULL);

			BOOL skip_children = TRUE;
			if (css->IsEnabled() && css->CheckMedia(m_doc, media_type))
			{
				CSS_WebFont* font = css->GetWebFont(family_name);
				if (font)
					return font;
				skip_children = FALSE;
			}

			elm = css->GetNextImport(skip_children);
			if (!elm)
			{
				elm = top_css;
				while ((elm = static_cast<CSSCollectionElement*>(elm->Pred())) && elm->IsStyleSheet() && static_cast<CSS*>(elm)->IsImport())
					;
			}
		}
#ifdef SVG_SUPPORT
		else if (elm->IsSvgFont())
		{
			CSS_SvgFont* font = static_cast<CSS_SvgFont*>(elm);
			if (uni_str_eq(font->GetFamilyName(), family_name))
				return font;
			else
				elm = elm->Pred();
		}
#endif // SVG_SUPPORT
		else
			elm = elm->Pred();
	}
	return NULL;
}

void CSSCollection::AddFontPrefetchCandidate(const uni_char* family_name)
{
	if (m_font_prefetch_limit > 0 && !m_font_prefetch_candidates.Contains(family_name))
	{
		OpString* name = OP_NEW(OpString, ());

		if (!name)
			return;

		OP_STATUS status = name->Set(family_name);

		if (OpStatus::IsSuccess(status))
			status = m_font_prefetch_candidates.Add(name->CStr(), name);

		if (OpStatus::IsError(status))
			OP_DELETE(name);
	}
}

void CSSCollection::DisableFontPrefetching()
{
	if (m_font_prefetch_limit > 0)
	{
		m_font_prefetch_candidates.DeleteAll();
		m_font_prefetch_limit = 0;
	}
}

void CSSCollection::CheckFontPrefetchCandidates()
{
	if (m_font_prefetch_limit > 0)
	{
		OpHashIterator* iter = m_font_prefetch_candidates.GetIterator();
		OP_STATUS status = iter ? iter->First() : OpStatus::ERR;
		int limit = m_font_prefetch_limit;

		while (OpStatus::IsSuccess(status) && limit > 0)
		{
			const uni_char* family_name = static_cast<const uni_char*>(iter->GetKey());
			if (GetWebFont(family_name, m_doc->GetMediaType()))
			{
				styleManager->LookupFontNumber(m_doc->GetHLDocProfile(), family_name, m_doc->GetMediaType());         // Loads the font if needed.
				limit--;
			}
			status = iter->Next();
		}

		OP_DELETE(iter);

		if (limit == 0)
			DisableFontPrefetching();
	}
}

void CSSCollection::OnTimeOut(OpTimer* timer)
{
	StyleChanged(CSSCollection::CHANGED_WEBFONT);
}

void CSSCollection::RegisterWebFontTimeout(int timeout)
{
	int time_remaining = -m_webfont_timer.TimeSinceFiring();

	if (time_remaining <= 0 || timeout < time_remaining)
	{
		m_webfont_timer.Stop();
		m_webfont_timer.Start(timeout);
		m_webfont_timer.SetTimerListener(this);
	}
}

int CSSCollection::GetSuccessiveAdjacent() const
{
	int max_successive_adj = 0;

	for (CSSCollectionElement* elm = m_element_list.First(); elm; elm = elm->Suc())
	{
		if (elm->IsStyleSheet())
		{
			CSS* css = static_cast<CSS*>(elm);
			if (css->IsEnabled() && css->GetSuccessiveAdjacent() > max_successive_adj)
				max_successive_adj = css->GetSuccessiveAdjacent();
		}
	}
	return max_successive_adj;
}

CSSCollection::Iterator::Iterator(CSSCollection* coll, Type type)
{
	OP_ASSERT(coll);

	m_type = type;
	CSSCollectionElement* elm = coll->m_element_list.First();
	m_next = elm ? SkipElements(elm) : NULL;
}

CSSCollectionElement* CSSCollection::Iterator::SkipElements(CSSCollectionElement* cand)
{
	if (m_type != ALL)
		while (cand && (cand->IsSvgFont() && m_type != WEBFONTS ||
#ifdef CSS_VIEWPORT_SUPPORT
						cand->IsViewportMeta() && m_type != VIEWPORT ||
#endif // CSS_VIEWPORT_SUPPORT
						cand->IsStyleSheet() && static_cast<CSS*>(cand)->IsImport() && m_type == STYLESHEETS_NOIMPORTS))
			cand = static_cast<CSSCollectionElement*>(cand->Suc());

	return cand;
}

void
CSSCollection::DimensionsChanged(LayoutCoord old_width,
								 LayoutCoord old_height,
								 LayoutCoord new_width,
								 LayoutCoord new_height)
{
	EvaluateMediaQueryLists();

	if (HasMediaQueryChanged(old_width, old_height, new_width, new_height))
		if (HTML_Element* root = m_doc->GetDocRoot())
			root->MarkPropsDirty(m_doc);
}

void
CSSCollection::DeviceDimensionsChanged(LayoutCoord old_device_width,
									   LayoutCoord old_device_height,
									   LayoutCoord new_device_width,
									   LayoutCoord new_device_height)
{
	EvaluateMediaQueryLists();

	if (HasDeviceMediaQueryChanged(old_device_width, old_device_height, new_device_width, new_device_height))
		if (HTML_Element* root = m_doc->GetDocRoot())
			root->MarkPropsDirty(m_doc);
}

BOOL CSSCollection::HasMediaQueryChanged(LayoutCoord old_width,
										 LayoutCoord old_height,
										 LayoutCoord new_width,
										 LayoutCoord new_height)
{
	for (CSSCollectionElement* elm = m_element_list.First(); elm; elm = elm->Suc())
	{
		if (elm->IsStyleSheet())
		{
			CSS* css = static_cast<CSS*>(elm);
			if (css->IsEnabled() && css->HasMediaQueryChanged(old_width, old_height, new_width, new_height))
				return TRUE;
		}
	}
	return FALSE;
}

BOOL CSSCollection::HasDeviceMediaQueryChanged(LayoutCoord old_device_width,
											   LayoutCoord old_device_height,
											   LayoutCoord new_device_width,
											   LayoutCoord new_device_height)
{
	for (CSSCollectionElement* elm = m_element_list.First(); elm; elm = elm->Suc())
	{
		if (elm->IsStyleSheet())
		{
			CSS* css = static_cast<CSS*>(elm);
			if (css->IsEnabled() && css->HasDeviceMediaQueryChanged(old_device_width, old_device_height, new_device_width, new_device_height))
				return TRUE;
		}
	}
	return FALSE;
}

void CSSCollection::EvaluateMediaQueryLists()
{
	for (CSS_MediaQueryList* mql = m_query_lists.First(); mql; mql = mql->Suc())
		mql->Evaluate(m_doc);
}

#ifdef PREFS_HOSTOVERRIDE

void CSSCollection::LoadHostOverridesL(const uni_char* hostname)
{
	// Read and parse the overridden CSS
	HostOverrideStatus is_overridden = g_pcfiles->IsHostOverridden(hostname, FALSE);

	if (is_overridden == HostOverrideActive
# ifdef PREFSFILE_WRITE_GLOBAL
		|| is_overridden == HostOverrideDownloadedActive
# endif
		)
	{
		HTML_Element::DocumentContext doc_ctx((FramesDocument*)NULL);

		OpFile cssfile;
		ANCHOR(OpFile, cssfile);

		for (unsigned int i = 0; i < CSSManager::FirstUserStyle; i++)
		{
			if (m_host_overrides[i].elm)
			{

				m_host_overrides[i].elm->Free(doc_ctx);
				m_host_overrides[i].elm = NULL;
			}
			m_host_overrides[i].css = NULL;

			switch (i)
			{
			case CSSManager::BrowserCSS:
				if (g_pcfiles->IsPreferenceOverridden(PrefsCollectionFiles::BrowserCSSFile, hostname))
				{
					g_pcfiles->GetFileL(PrefsCollectionFiles::BrowserCSSFile, cssfile, hostname);
				}
				else
					continue;
				break;
			case CSSManager::LocalCSS:
				if (g_pcfiles->IsPreferenceOverridden(PrefsCollectionFiles::LocalCSSFile, hostname))
				{
					g_pcfiles->GetFileL(PrefsCollectionFiles::LocalCSSFile, cssfile, hostname);
				}
				else
					continue;
				break;

# ifdef SUPPORT_VISUAL_ADBLOCK
			case CSSManager::ContentBlockCSS:
				if (g_pcfiles->IsPreferenceOverridden(PrefsCollectionFiles::ContentBlockCSSFile, hostname))
				{
					g_pcfiles->GetFileL(PrefsCollectionFiles::ContentBlockCSSFile, cssfile, hostname);
				}
				else
					continue;
				break;
# endif // SUPPORT_VISUAL_ADBLOCK

			// TODO: Get overrides for specialized stylesheets
			default:
				continue;
			}

			m_host_overrides[i].elm = CSSManager::LoadCSSFileL(&cssfile, i == CSSManager::LocalCSS);
			if (m_host_overrides[i].elm)
				m_host_overrides[i].css = m_host_overrides[i].elm->GetCSS();
		}
	}
}

/* virtual */ void
CSSCollection::FileHostOverrideChanged(const uni_char* hostname)
{
	OpString doc_hostname;

	RETURN_VOID_IF_ERROR(m_doc->GetURL().GetAttribute(URL::KUniHostName, doc_hostname));

	if (doc_hostname.Compare(hostname) != 0)
		return;

	// hostname matched, update the local stylesheets.

	TRAPD(err, CSSCollection::LoadHostOverridesL(hostname));
	if (OpStatus::IsSuccess(err))
	{
		Window* win = m_doc->GetWindow();
		if (win)
			err = win->UpdateWindow(TRUE);
	}
	RAISE_IF_MEMORY_ERROR(err);
}

CSSCollection::LocalStylesheet::~LocalStylesheet()
{
	HTML_Element::DocumentContext doc_ctx((FramesDocument*)NULL);
	if (elm)
		elm->Free(doc_ctx);
}

#endif // PREFS_HOSTOVERRIDE

void
CSSCollection::GetAnchorStyle(CSS_MatchContextElm* context_elm, BOOL set_color, COLORREF& color, int& decoration_flags, CSSValue& border_style, short& border_width)
{
	context_elm->CacheLinkState(m_doc);

	if (context_elm->IsLink())
	{
		HLDocProfile* hld_profile = m_doc->GetHLDocProfile();
		CSSMODE css_handling = hld_profile->GetCSSMode();

		if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LinkHasFrame))
		{
			border_style = CSS_VALUE_outset;
			border_width = 2;
		}

		if (context_elm->IsVisited())
		{
			if (set_color)
			{
				if (hld_profile->GetVLinkColor() == USE_DEFAULT_COLOR ||
					!g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableAuthorFontAndColors)))
				{
					if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::VisitedLinkHasColor) && g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableUserLinkSettings)))
						color = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_VISITED_LINK);
				}
				else
					color = hld_profile->GetVLinkColor();
			}

			if (g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableUserLinkSettings)))
			{
				if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::VisitedLinkHasUnderline))
					decoration_flags |= CSS_TEXT_DECORATION_underline;
				if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::VisitedLinkHasStrikeThrough))
					decoration_flags |= CSS_TEXT_DECORATION_line_through;
			}
		}
		else // Link (not visited)
		{
			if (set_color)
			{
				if (hld_profile->GetLinkColor() == USE_DEFAULT_COLOR ||
					!g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableAuthorFontAndColors)))
				{
					if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LinkHasColor) && g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableUserLinkSettings)))
						color = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_LINK);
				}
				else
					color = hld_profile->GetLinkColor();
			}

			if (g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableUserLinkSettings)))
			{
				if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LinkHasUnderline))
					decoration_flags |= CSS_TEXT_DECORATION_underline;
				if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LinkHasStrikeThrough))
					decoration_flags |= CSS_TEXT_DECORATION_line_through;
			}
		}

		if (set_color && hld_profile->GetALinkColor() != USE_DEFAULT_COLOR)
		{
			context_elm->HtmlElement()->SetHasDynamicPseudo(TRUE);
			HTML_Document* doc = m_doc->GetHtmlDocument();

			if (doc && doc->GetActivePseudoHTMLElement() == context_elm->HtmlElement() &&
				g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableAuthorFontAndColors)))
			{
				color = hld_profile->GetALinkColor();
			}
		}
	}
}

// also used from layoutprops (GetCssProperties)
int GetFormStyle(const Markup::Type elm_type, const InputType itype)
{
	int form_style = STYLE_EX_FORM_TEXTINPUT;
	if (elm_type == Markup::HTE_INPUT && itype == INPUT_IMAGE)
		return form_style;
	if (elm_type == Markup::HTE_INPUT || elm_type == Markup::HTE_BUTTON)
	{
		switch (itype)
		{
		case INPUT_BUTTON:
		case INPUT_SUBMIT:
		case INPUT_RESET:
			return STYLE_EX_FORM_BUTTON;
		}
	}
	else if (elm_type == Markup::HTE_SELECT
#ifdef _SSL_SUPPORT_
		|| elm_type == Markup::HTE_KEYGEN
#endif
		|| itype == INPUT_DATE || itype == INPUT_WEEK)
		return STYLE_EX_FORM_SELECT;
	else if (elm_type == Markup::HTE_TEXTAREA)
		return STYLE_EX_FORM_TEXTAREA;
	return form_style;
}

OP_STATUS
CSSCollection::GetDefaultStyleProperties(CSS_MatchContextElm* context_elm, CSS_Properties* css_properties)
{
	OP_PROBE4(OP_PROBE_CSSCOLLECTION_GETDEFAULTSTYLE);

	HLDocProfile* hld_profile = m_doc->GetHLDocProfile();

	Markup::Type elm_type = context_elm->Type();
	NS_Type elm_ns_type = context_elm->GetNsType();
	HTML_Element* element = context_elm->HtmlElement();

	BOOL set_display = css_properties->GetDecl(CSS_PROPERTY_display) == NULL;
	CSSValue display_type = CSS_VALUE_UNSPECIFIED;

	StyleModule::DefaultStyle* default_style = &g_opera->style_module.default_style;

    if (elm_ns_type != NS_HTML && elm_type != Markup::HTE_DOC_ROOT)
	{
		switch (elm_ns_type)
		{
#ifdef _WML_SUPPORT_
		case NS_WML:
			switch ((WML_ElementType)elm_type)
			{
			case WE_DO:
				if (set_display &&
					element->HasSpecialAttr(Markup::LOGA_WML_SHADOWING_TASK, SpecialNs::NS_LOGDOC))
				{
					display_type = CSS_VALUE_none;
					// We don't need to retrieve anchor style if display is none.
					break;
				}
				// Fall-through
			case WE_ANCHOR:
				{
					BOOL set_color = (css_properties->GetDecl(CSS_PROPERTY_color) == NULL);
					COLORREF color = USE_DEFAULT_COLOR;
					int decoration_flags = 0;
					CSSValue border_style = CSS_VALUE_UNSPECIFIED;
					short border_width = BORDER_WIDTH_NOT_SET;

					if (set_color || !css_properties->GetDecl(CSS_PROPERTY_text_decoration) || g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LinkHasFrame))
						GetAnchorStyle(context_elm, TRUE, color, decoration_flags, border_style, border_width);

					if (color != USE_DEFAULT_COLOR && set_color)
					{
						CSS_long_decl* decl = default_style->fg_color;
						decl->SetLongValue(color);
						css_properties->SetDecl(CSS_PROPERTY_color, decl);
					}
					if (decoration_flags && !css_properties->GetDecl(CSS_PROPERTY_text_decoration))
					{
						CSS_type_decl* decl = default_style->text_decoration;
						/* Text decoration flags are stored as CSSValue, carefully chosen not to
						   collide with other valid values for text-decoration. */
						decl->SetTypeValue(CSSValue(decoration_flags));
						css_properties->SetDecl(CSS_PROPERTY_text_decoration, decl);
					}
					if (border_style != CSS_VALUE_UNSPECIFIED)
					{
						OP_ASSERT(border_width != BORDER_WIDTH_NOT_SET);

						CSS_type_decl* style_decl = default_style->border_bottom_style;
						style_decl->SetTypeValue(border_style);

						CSS_number_decl* width_decl = default_style->border_bottom_width;
						width_decl->SetNumberValue(0, (float)border_width, CSS_PX);

						if (!css_properties->GetDecl(CSS_PROPERTY_border_top_style))
							css_properties->SetDecl(CSS_PROPERTY_border_top_style, style_decl);
						if (!css_properties->GetDecl(CSS_PROPERTY_border_bottom_style))
							css_properties->SetDecl(CSS_PROPERTY_border_bottom_style, style_decl);
						if (!css_properties->GetDecl(CSS_PROPERTY_border_left_style))
							css_properties->SetDecl(CSS_PROPERTY_border_left_style, style_decl);
						if (!css_properties->GetDecl(CSS_PROPERTY_border_right_style))
							css_properties->SetDecl(CSS_PROPERTY_border_right_style, style_decl);

						if (!css_properties->GetDecl(CSS_PROPERTY_border_top_width))
							css_properties->SetDecl(CSS_PROPERTY_border_top_width, width_decl);
						if (!css_properties->GetDecl(CSS_PROPERTY_border_bottom_width))
							css_properties->SetDecl(CSS_PROPERTY_border_bottom_width, width_decl);
						if (!css_properties->GetDecl(CSS_PROPERTY_border_left_width))
							css_properties->SetDecl(CSS_PROPERTY_border_left_width, width_decl);
						if (!css_properties->GetDecl(CSS_PROPERTY_border_right_width))
							css_properties->SetDecl(CSS_PROPERTY_border_right_width, width_decl);
					}
				}
				break;
			}
			break;
#endif // _WML_SUPPORT_

#ifdef SVG_SUPPORT
		case NS_SVG:
		{
			SVGWorkplace* svg_workplace = m_doc->GetLogicalDocument()->GetSVGWorkplace();
			RETURN_IF_ERROR(svg_workplace->GetDefaultStyleProperties(element, css_properties));
			break;
		}
#endif // SVG_SUPPORT

#ifdef MEDIA_HTML_SUPPORT
		case NS_CUE:
			GetDefaultCueStyleProperties(element, css_properties);
			break;
#endif // MEDIA_HTML_SUPPORT

		default:
			break;
		}

		if (display_type != CSS_VALUE_UNSPECIFIED)
		{
			CSS_type_decl* decl = default_style->display_type;
			decl->SetTypeValue(display_type);
			css_properties->SetDecl(CSS_PROPERTY_display, decl);
		}

		return OpStatus::OK;
	}

	LayoutMode layout_mode = m_match_context->DocLayoutMode();

	CSSMODE css_handling = hld_profile->GetCSSMode();
	Style* style = styleManager->GetStyle(elm_type);
	const LayoutAttr& lattr = style->GetLayoutAttr();

	BOOL set_white_space = css_properties->GetDecl(CSS_PROPERTY_white_space) == NULL;
	BOOL set_valign = css_properties->GetDecl(CSS_PROPERTY_vertical_align) == NULL;
	BOOL set_text_indent = css_properties->GetDecl(CSS_PROPERTY_text_indent) == NULL;
	BOOL set_bg_color = css_properties->GetDecl(CSS_PROPERTY_background_color) == NULL;
	BOOL set_fg_color = css_properties->GetDecl(CSS_PROPERTY_color) == NULL;

	CSSValue white_space = CSS_VALUE_UNSPECIFIED;
	CSSValue vertical_align_type = CSS_VALUE_UNSPECIFIED;
	int text_indent = INT_MIN;
	COLORREF bg_color = USE_DEFAULT_COLOR;
	COLORREF fg_color = USE_DEFAULT_COLOR;

	Border border;
	border.Reset();

	CSSValue text_align = CSS_VALUE_UNSPECIFIED;
	int decoration_flags = 0;
	CSSValue font_italic = CSS_VALUE_UNSPECIFIED;

#ifdef SUPPORT_TEXT_DIRECTION
	CSSValue unicode_bidi = CSS_VALUE_UNSPECIFIED;
#endif // SUPPORT_TEXT_DIRECTION

	CSSValue html_align = (CSSValue)(INTPTR)element->GetAttr(Markup::HA_ALIGN, ITEM_TYPE_NUM, (void*)(INTPTR)CSS_VALUE_UNSPECIFIED);

	BOOL use_user_font_setting = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableUserFontAndColors));

	short default_padding_horz = 0;
	short default_padding_vert = 0;
	COLORREF border_default_color = USE_DEFAULT_COLOR;
	CSSValue border_default_style = CSS_VALUE_none;
	short border_default_width = CSS_BORDER_WIDTH_MEDIUM;
	BOOL has_left_margin = FALSE;
	BOOL has_right_margin = FALSE;
	BOOL has_top_margin = FALSE;
	BOOL has_bottom_margin = FALSE;
	BOOL has_vertical_padding = FALSE;
	BOOL has_horizontal_padding = FALSE;
	BOOL replaced_set_html_align = FALSE;
	BOOL replaced_elm = FALSE;
	BOOL absolute_width = FALSE;

	BOOL margin_h_auto = FALSE;

	enum {
		MARGIN_LEFT,
		MARGIN_RIGHT,
		MARGIN_TOP,
		MARGIN_BOTTOM,
		PADDING_LEFT,
		PADDING_RIGHT,
		PADDING_TOP,
		PADDING_BOTTOM,
		MARGIN_PADDING_SIZE
	};

	// padding-left, padding-right, padding-top, padding-bottom
	short margin_padding[MARGIN_PADDING_SIZE] = { 0, 0, 0, 0, 0, 0, 0, -1 }; /* ARRAY OK 2012-03-14 rune */

	// <input type="image"> must be special cased since <input> normally doesn't support height and width
	if (!css_properties->GetDecl(CSS_PROPERTY_width) &&
		(SupportAttribute(elm_type, ATTR_WIDTH_SUPPORT) || elm_type == Markup::HTE_INPUT && element->GetInputType() == INPUT_IMAGE))
	{
		HTML_Element* width_elm = element;
		BOOL has_width = element->HasNumAttr(Markup::HA_WIDTH);
		if (elm_type == Markup::HTE_COL && !has_width)
		{
			// fix bug 83881, width of <COL> elements should inherit width from a parent <COLGROUP> if set to auto
			while (width_elm)
			{
				if (width_elm->Type() == Markup::HTE_COLGROUP)
				{
					has_width = width_elm->HasNumAttr(Markup::HA_WIDTH);
					break;
				}
				width_elm = width_elm->Parent();
			}
		}

		if (has_width)
		{
			int val = (int)width_elm->GetNumAttr(Markup::HA_WIDTH);

			if (val != 0 || (elm_type != Markup::HTE_TD && elm_type != Markup::HTE_TH && elm_type != Markup::HTE_COL && elm_type != Markup::HTE_COLGROUP))
				if (elm_type != Markup::HTE_PRE || val > 0)
				{
					int value_type;
					if (elm_type == Markup::HTE_PRE)
					{
						value_type  = CSS_NUMBER;
					}
					else
					{
						if (val < 0)
						{
							val = -val;
							value_type = CSS_PERCENTAGE;
						}
						else
						{
							value_type = CSS_PX;
							absolute_width = TRUE;
						}
					}
					CSS_number_decl* width_decl = default_style->content_width;
					width_decl->SetNumberValue(0, (float)val, value_type);
					css_properties->SetDecl(CSS_PROPERTY_width, width_decl);
				}
		}
	}

	// <input type="image"> must be special cased since <input> normally doesn't support height and width
	if (!css_properties->GetDecl(CSS_PROPERTY_height) &&
		(SupportAttribute(elm_type, ATTR_HEIGHT_SUPPORT) || elm_type == Markup::HTE_INPUT && element->GetInputType() == INPUT_IMAGE))
	{
		if (element->HasNumAttr(Markup::HA_HEIGHT))
		{
			int value = element->GetNumAttr(Markup::HA_HEIGHT);

			if (value != 0 || (elm_type != Markup::HTE_TD && elm_type != Markup::HTE_TH))
			{
				int value_type = CSS_PX;
				if (value < 0)
				{
					value_type = CSS_PERCENTAGE;
					value = -value;
				}

				CSS_number_decl* height_decl = default_style->content_height;
				height_decl->SetNumberValue(0, (float)value, value_type);
				css_properties->SetDecl(CSS_PROPERTY_height, height_decl);
			}
		}
	}

	if (!css_properties->GetDecl(CSS_PROPERTY_background_image) &&
		SupportAttribute(elm_type, ATTR_BACKGROUND_SUPPORT) &&
		g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableAuthorFontAndColors)))
	{
		StyleAttribute* style_attr = (StyleAttribute*)element->GetAttr(Markup::HA_BACKGROUND, ITEM_TYPE_COMPLEX, (void*)0);
		if (style_attr)
		{
			OP_ASSERT(style_attr->GetProperties()->GetLength() == 1);
			css_properties->SetDecl(CSS_PROPERTY_background_image, style_attr->GetProperties()->GetFirstDecl());
		}
	}

	float rel_font_size = 0;
	short font_size = -1;
	short font_weight = -1;
	short font_number = -1;

	switch (elm_type)
	{
	case Markup::HTE_TITLE:
	case Markup::HTE_META:
	case Markup::HTE_BASE:
	case Markup::HTE_LINK:
	case Markup::HTE_STYLE:
	case Markup::HTE_SCRIPT:
	case Markup::HTE_AREA:
	case Markup::HTE_HEAD:
	case Markup::HTE_XML:
		if (set_display)
			display_type = CSS_VALUE_none;
		break;

	case Markup::HTE_NOFRAMES:
		if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::FramesEnabled, m_doc->GetURL().GetServerName()))
			display_type = CSS_VALUE_none;
		break;

	case Markup::HTE_NOEMBED:
		if (set_display)
			display_type = CSS_VALUE_none;
		break;

	case Markup::HTE_NOSCRIPT:
		/* Only scripting on/off can control display type of NOSCRIPT. See CORE-14030. */
		if (!element->GetSpecialAttr(Markup::LOGA_NOSCRIPT_ENABLED, ITEM_TYPE_BOOL, 0, SpecialNs::NS_LOGDOC))
			display_type = CSS_VALUE_none;
		break;

	case Markup::HTE_LEGEND:
		margin_padding[PADDING_LEFT] = margin_padding[PADDING_RIGHT] = 2;
		has_horizontal_padding = TRUE;
		// fall-through

	case Markup::HTE_ADDRESS:
		if (elm_type == Markup::HTE_ADDRESS)
			font_italic = CSS_VALUE_italic;
		// fall-through

	case Markup::HTE_HTML:
	case Markup::HTE_ISINDEX:
	case Markup::HTE_BLOCKQUOTE:
	case Markup::HTE_DIR:
	case Markup::HTE_MENU:
		if (set_display)
			display_type = CSS_VALUE_block;
		break;

	case Markup::HTE_FORM:
		{
			CSS_MatchContextElm* parent = context_elm->Parent();
			if (parent)
			{
				Markup::Type parent_type = parent->Type();
				if (parent->GetNsIdx() == NS_IDX_HTML && (parent_type == Markup::HTE_TABLE || parent_type == Markup::HTE_TBODY || parent_type == Markup::HTE_TFOOT || parent_type == Markup::HTE_THEAD || parent_type == Markup::HTE_TR))
				{
					display_type = CSS_VALUE_none;
					break;
				}
			}
			if (set_display)
				display_type = CSS_VALUE_block;
		}
		break;

	case Markup::HTE_FIELDSET:
		if (set_display)
			display_type = CSS_VALUE_block;
		border_default_style = CSS_VALUE_groove;
		border_default_width = 2;
		margin_padding[MARGIN_LEFT] = margin_padding[MARGIN_RIGHT] = 2;
		has_left_margin = TRUE;
		break;

	case Markup::HTE_MARQUEE:
		{
			if (set_display)
				display_type = CSS_VALUE__wap_marquee;

			if (!css_properties->GetDecl(CSS_PROPERTY_overflow_x))
				css_properties->SetDecl(CSS_PROPERTY_overflow_x, default_style->overflow_x);
			if (!css_properties->GetDecl(CSS_PROPERTY_overflow_y))
				css_properties->SetDecl(CSS_PROPERTY_overflow_y, default_style->overflow_y);

			int direction = 0;
			if (!css_properties->GetDecl(CSS_PROPERTY__wap_marquee_dir))
			{
				direction = (int)element->GetNumAttr(Markup::HA_DIRECTION);
				if (direction)
				{
					CSS_type_decl* dir_decl = default_style->marquee_dir;
					CSSValue dir = CSS_VALUE_UNSPECIFIED;
					switch (direction)
					{
					case ATTR_VALUE_left:
						dir = CSS_VALUE_rtl;
						break;
					case ATTR_VALUE_right:
						dir = CSS_VALUE_ltr;
						break;
					case ATTR_VALUE_up:
						dir = CSS_VALUE_up;
						break;
					case ATTR_VALUE_down:
						dir = CSS_VALUE_down;
						break;
					}
					dir_decl->SetTypeValue(dir);
					css_properties->SetDecl(CSS_PROPERTY__wap_marquee_dir, dir_decl);
				}
			}

			CSSValue style = CSS_VALUE_UNSPECIFIED;
			if (!css_properties->GetDecl(CSS_PROPERTY__wap_marquee_style))
			{
				int behavior = (int)element->GetNumAttr(Markup::HA_BEHAVIOR);
				switch (behavior)
				{
				case ATTR_VALUE_slide:
					style = CSS_VALUE_slide; break;
				case ATTR_VALUE_alternate:
					style = CSS_VALUE_alternate; break;
				}
				if (style != CSS_VALUE_UNSPECIFIED)
				{
					CSS_type_decl* style_decl = default_style->marquee_style;
					style_decl->SetTypeValue(style);
					css_properties->SetDecl(CSS_PROPERTY__wap_marquee_style, style_decl);
				}
			}

			if (!css_properties->GetDecl(CSS_PROPERTY__wap_marquee_loop))
			{
				long marquee_loop = (long)element->GetAttr(Markup::HA_LOOP, ITEM_TYPE_NUM, (void*)-1);

				if (style == CSS_VALUE_slide && marquee_loop == -1)
					marquee_loop = 1;
				else if (marquee_loop == 0)
					marquee_loop = -1;

				if (marquee_loop != -1)
				{
					default_style->marquee_loop->SetNumberValue(0, (float)marquee_loop, CSS_NUMBER);
					css_properties->SetDecl(CSS_PROPERTY__wap_marquee_loop, default_style->marquee_loop);
				}
				else
					css_properties->SetDecl(CSS_PROPERTY__wap_marquee_loop, default_style->infinite_decl);
			}

			if (set_bg_color && layout_mode != LAYOUT_SSR)
				bg_color = (long)element->GetAttr(Markup::HA_BGCOLOR, ITEM_TYPE_NUM, (void*)USE_DEFAULT_COLOR);

			if (!css_properties->GetDecl(CSS_PROPERTY_height))
			{
				if (direction == ATTR_VALUE_up || direction == ATTR_VALUE_down)
				{
					default_style->content_height->SetNumberValue(0, 200, CSS_PX);
					css_properties->SetDecl(CSS_PROPERTY_height, default_style->content_height);
				}
			}
		}
		break;

	case Markup::HTE_BUTTON:
		if (set_display)
			display_type = CSS_VALUE_inline_block;
		text_align = CSS_VALUE_center;
		if (set_white_space && !css_properties->GetDecl(CSS_PROPERTY_width))
			white_space = CSS_VALUE_nowrap;

		default_padding_horz = 8;
		default_padding_vert = 1;

		/* Fallthrough */
	case Markup::HTE_SELECT:
#ifdef _SSL_SUPPORT_
	case Markup::HTE_KEYGEN:
#endif
	case Markup::HTE_TEXTAREA:
	case Markup::HTE_INPUT:
		{
			InputType itype = element->GetInputType();
			if (elm_type == Markup::HTE_TEXTAREA)
			{
				itype = INPUT_TEXTAREA; // virtual type to simplify code
				if (!css_properties->GetDecl(CSS_PROPERTY_line_height))
					css_properties->SetDecl(CSS_PROPERTY_line_height, default_style->line_height);
				if (!css_properties->GetDecl(CSS_PROPERTY_overflow_wrap))
					css_properties->SetDecl(CSS_PROPERTY_overflow_wrap, default_style->overflow_wrap);
				if (!css_properties->GetDecl(CSS_PROPERTY_resize))
					css_properties->SetDecl(CSS_PROPERTY_resize, default_style->resize);
			}
			if (elm_type == Markup::HTE_INPUT || elm_type == Markup::HTE_TEXTAREA)
			{
				if (!css_properties->GetDecl(CSS_PROPERTY_text_transform))
					css_properties->SetDecl(CSS_PROPERTY_text_transform, default_style->text_transform);
			}

			if (set_text_indent)
				text_indent = 0;

			BOOL element_disabled = element->IsDisabled(m_doc);

			if (set_fg_color)
			{
				fg_color = OP_RGB(0, 0, 0);
				if (element_disabled)
					fg_color = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED);

				if (elm_type == Markup::HTE_TEXTAREA)
					fg_color = element_disabled ? g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED) :
													g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TEXT);
				else if (elm_type == Markup::HTE_BUTTON)
					fg_color = element_disabled ? g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED) :
													g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BUTTON_TEXT);
			}

			border_default_width = 2;

			// Set default alignment
			text_align = CSS_VALUE_default;

			if (FormManager::IsButton(itype))
			{
				text_align = CSS_VALUE_center;
			}
			else if (itype == INPUT_NUMBER || itype == INPUT_RANGE || itype == INPUT_DATE)
			{
				text_align = CSS_VALUE_right;
			}

			switch (itype)
			{
			case INPUT_BUTTON:
			case INPUT_SUBMIT:
			case INPUT_RESET:
				default_padding_horz = 8;
				default_padding_vert = 1;
				if (set_fg_color)
					fg_color = element_disabled ? g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED) :
													g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BUTTON_TEXT);
				break;

			// this is done to mimic the behaviour of ff/webkit. that
			// is:
			// * 3px 3px 3px 4px for checkbox
			// * 3px 3px 0px 5px for radio button
			case INPUT_RADIO:
				margin_padding[MARGIN_BOTTOM] = 0;
				has_bottom_margin = TRUE;
			case INPUT_CHECKBOX:
				margin_padding[MARGIN_TOP] = 3;
				has_top_margin = TRUE;
				margin_padding[MARGIN_RIGHT] = 3;
				has_right_margin = TRUE;
				margin_padding[MARGIN_LEFT] = itype == INPUT_CHECKBOX ? 4 : 5;
				has_left_margin = TRUE;
				// fall through

			case INPUT_TEXT:
			case INPUT_TEXTAREA:
			case INPUT_PASSWORD:
			case INPUT_FILE:
			case INPUT_NUMBER:
			case INPUT_URI:
			case INPUT_EMAIL:
			case INPUT_TIME:
			case INPUT_DATETIME:
			case INPUT_DATETIME_LOCAL:
			case INPUT_WEEK:
			case INPUT_COLOR:
			case INPUT_TEL:
			case INPUT_SEARCH:
				default_padding_horz = 1;
				default_padding_vert = 1;
				if (set_fg_color)
					fg_color = element_disabled ? g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED) :
													g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TEXT_INPUT);
				break;

			case INPUT_IMAGE:
				border_default_width = 0;
				replaced_set_html_align = TRUE;
				break;

			case INPUT_HIDDEN:
				display_type = CSS_VALUE_none;
				break;
			}

			int form_style = GetFormStyle(elm_type, itype);
			Style* style = styleManager->GetStyleEx(form_style);
			const PresentationAttr& p_attr = style->GetPresentationAttr();
			if (elm_type == Markup::HTE_INPUT && itype == INPUT_IMAGE)
			{
				replaced_elm = TRUE;
			}
			else
			{
				// FIXME: move GetFontColorDefined() to HTMLayoutProperties (needs to be inherited !!!)
				if (set_fg_color && use_user_font_setting && (
					!g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableAuthorFontAndColors))))
					fg_color = p_attr.Color;

				WritingSystem::Script script = WritingSystem::Unknown;
#ifdef FONTSWITCHING
				script = hld_profile->GetPreferredScript();
#endif // FONTSWITCHING
				FontAtt* font = p_attr.GetPresentationFont(script).Font;
				font_size = op_abs(font->GetHeight());
				font_size = MAX(font_size, g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::MinFontSize));
				font_weight = font->GetWeight();
				if (p_attr.Italic)
					font_italic = CSS_VALUE_italic;

				if (!css_properties->GetDecl(CSS_PROPERTY_font_family))
				{
					CSS_type_decl* font_family_type = default_style->font_family_type;
					font_family_type->SetTypeValue(CSS_VALUE_use_lang_def);
					css_properties->SetDecl(CSS_PROPERTY_font_family, font_family_type);
				}
			}
		}
		break;

	case Markup::HTE_OPTION:
	case Markup::HTE_OPTGROUP:
		if (set_text_indent)
			text_indent = 0;

		break;

	case Markup::HTE_PROGRESS:
	case Markup::HTE_METER:
		if (set_valign)
			vertical_align_type = CSS_VALUE_middle;
		border_default_width = 1;
		break;

	case Markup::HTE_IFRAME:
		{
			URL* src_url = element->GetUrlAttr(Markup::HA_SRC, NS_IDX_HTML, m_doc->GetLogicalDocument());
			BOOL show_iframe = m_doc->IsAllowedIFrame(src_url);
			if (!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::IFramesEnabled, m_doc->GetURL()) || !show_iframe)
			{
				if (set_display)
				{
					if (!show_iframe)
						display_type = CSS_VALUE_none;
					else
						display_type = CSS_VALUE_block;
				}
				break;
			}
			else
			{
				// If frameborder isn't specified it should be on according to standard.
				if (!element->HasAttr(Markup::HA_FRAMEBORDER) || element->GetFrameBorder())
				{
					border_default_width = 2;
					border_default_style = CSS_VALUE_inset;
				}
				replaced_elm = TRUE;
			}
		}

		replaced_set_html_align = TRUE;
		break;

	case Markup::HTE_TABLE:
		{
			// Gecko and Webkit compatibility.
			CSSValue border_collapse = CSS_VALUE_separate;

			if (set_display)
				display_type = CSS_VALUE_table;

			if (set_bg_color &&
				layout_mode != LAYOUT_SSR &&
				g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableAuthorFontAndColors)))
			{
				bg_color = element->GetNumAttr(Markup::HA_BGCOLOR, NS_IDX_HTML, USE_DEFAULT_COLOR);
			}

			if (set_text_indent)
				text_indent = 0;

            // IE 6 inherits all properties into a table in CSS1CompatMode, but some are reset in QuirksMode...
            if (!hld_profile->IsInStrictMode())
			{
				if (!css_properties->GetDecl(CSS_PROPERTY_font_size))
				{
					CSS_type_decl* font_size_decl = default_style->font_size_type;
					font_size_decl->SetTypeValue(CSS_VALUE_use_lang_def);
					css_properties->SetDecl(CSS_PROPERTY_font_size, font_size_decl);
				}
				font_weight = 4;
				font_italic = CSS_VALUE_normal;
				if (set_white_space)
					white_space = CSS_VALUE_normal;
				text_align = CSS_VALUE_default; // only default alignment, should not be set as specified

				if (!css_properties->GetDecl(CSS_PROPERTY_word_spacing))
					css_properties->SetDecl(CSS_PROPERTY_word_spacing, default_style->word_spacing);
				if (!css_properties->GetDecl(CSS_PROPERTY_letter_spacing))
					css_properties->SetDecl(CSS_PROPERTY_letter_spacing, default_style->letter_spacing);
            }

			short table_rules = short(element->GetNumAttr(Markup::HA_RULES, NS_IDX_HTML, 0));
			short table_frame = short(element->GetNumAttr(Markup::HA_FRAME, NS_IDX_HTML, 0));

			if (table_rules)
			{
				CSS_long_decl* decl = default_style->table_rules;
				decl->SetLongValue(table_rules);
				css_properties->SetDecl(CSS_PROPERTY_table_rules, decl);
			}

			BOOL has_border = table_frame != ATTR_VALUE_void && element->HasNumAttr(Markup::HA_BORDER);

			if (has_border)
			{
				border_default_width = (short)(INTPTR)element->GetAttr(Markup::HA_BORDER, ITEM_TYPE_NUM, (void*)0);

				if (border_default_width)
				{
					border_default_style = CSS_VALUE_outset;

					if (!table_frame)
						table_frame = ATTR_VALUE_border;
				}
				else
					if (table_frame || table_rules)
						border_default_style = CSS_VALUE_hidden;
			}
			else
				if (!table_frame && table_rules)
					table_frame = ATTR_VALUE_void;

			if (table_rules)
			{
				border_collapse = CSS_VALUE_collapse;

				if (border_default_style != CSS_VALUE_hidden && table_frame != ATTR_VALUE_box && table_frame != ATTR_VALUE_border)
				{
					if (table_frame != ATTR_VALUE_vsides && table_frame != ATTR_VALUE_lhs)
						border.left.style = CSS_VALUE_hidden;

					if (table_frame != ATTR_VALUE_hsides && table_frame != ATTR_VALUE_above)
						border.top.style = CSS_VALUE_hidden;
				}
			}

			switch (table_frame)
			{
			case ATTR_VALUE_void:
				border_default_style = CSS_VALUE_hidden;
				break;

			case ATTR_VALUE_box:
			case ATTR_VALUE_border:
				if (!has_border)
				{
					border_default_style = CSS_VALUE_solid;
					border_default_width = CSS_BORDER_WIDTH_THIN;
				}
				break;

			case ATTR_VALUE_lhs:
			case ATTR_VALUE_rhs:
			case ATTR_VALUE_vsides:
				if (table_frame != ATTR_VALUE_rhs)
				{
					border.left.width = has_border ? border_default_width : CSS_BORDER_WIDTH_THIN;
					if (border.left.style != CSS_VALUE_hidden)
						border.left.style = has_border ? border_default_style : CSS_VALUE_solid;
				}
				if (table_frame != ATTR_VALUE_lhs)
				{
					border.right.width = has_border ? border_default_width : CSS_BORDER_WIDTH_THIN;
					border.right.style = has_border ? border_default_style : CSS_VALUE_solid;
				}

				border_default_width = CSS_BORDER_WIDTH_MEDIUM;

				if (border_default_style != CSS_VALUE_hidden)
					border_default_style = CSS_VALUE_none;

				break;

			case ATTR_VALUE_above:
			case ATTR_VALUE_below:
			case ATTR_VALUE_hsides:
				if (table_frame != ATTR_VALUE_below)
				{
					border.top.width = has_border ? border_default_width : CSS_BORDER_WIDTH_THIN;
					if (border.top.style != CSS_VALUE_hidden)
						border.top.style = has_border ? border_default_style : CSS_VALUE_solid;
				}
				if (table_frame != ATTR_VALUE_above)
				{
					border.bottom.width = has_border ? border_default_width : CSS_BORDER_WIDTH_THIN;
					border.bottom.style = has_border ? border_default_style : CSS_VALUE_solid;
				}

				border_default_width = CSS_BORDER_WIDTH_MEDIUM;

				if (border_default_style != CSS_VALUE_hidden)
					border_default_style = CSS_VALUE_none;

				break;
			}

			if (!css_properties->GetDecl(CSS_PROPERTY_border_spacing))
			{
				int border_spacing;
				if (element->HasNumAttr(Markup::HA_CELLSPACING))
					border_spacing = (int)(INTPTR)element->GetAttr(Markup::HA_CELLSPACING, ITEM_TYPE_NUM, (void*)0);
				else
					border_spacing = 2;

				CSS_number_decl* decl = default_style->border_spacing;
				decl->SetNumberValue(0, (float)border_spacing, CSS_PX);
				css_properties->SetDecl(CSS_PROPERTY_border_spacing, decl);
			}

			if (html_align == CSS_VALUE_center)
			{
				margin_h_auto = TRUE;
				html_align = CSS_VALUE_UNSPECIFIED;
			}
			if (!css_properties->GetDecl(CSS_PROPERTY_border_collapse))
			{
				CSS_type_decl* decl = default_style->border_collapse;
				decl->SetTypeValue(border_collapse);
				css_properties->SetDecl(CSS_PROPERTY_border_collapse, decl);
			}
			replaced_set_html_align = TRUE;
		}
		break;

	case Markup::HTE_IMG:
	case Markup::HTE_OBJECT:
		if (element->HasNumAttr(Markup::HA_BORDER))
		{
			border_default_width = (short)(INTPTR)element->GetAttr(Markup::HA_BORDER, ITEM_TYPE_NUM, (void*)1);
			border_default_style = CSS_VALUE_solid;
		}
		// fall-through
	case Markup::HTE_EMBED:
	case Markup::HTE_APPLET:
		replaced_set_html_align = TRUE;

#ifdef CANVAS_SUPPORT
		// fall-through
	case Markup::HTE_CANVAS:
#endif // CANVAS_SUPPORT
		replaced_elm = TRUE;
		break;

#ifdef MEDIA_HTML_SUPPORT
	case Markup::HTE_AUDIO:
		if (set_display)
			if (element->GetBoolAttr(Markup::HA_CONTROLS))
				display_type = CSS_VALUE_inline;
			else
				display_type = CSS_VALUE_none;
		break;
#endif // MEDIA_HTML_SUPPORT

	case Markup::HTE_EM:
	case Markup::HTE_I:
	case Markup::HTE_VAR:
	case Markup::HTE_CITE:
	case Markup::HTE_DFN:
		font_italic = CSS_VALUE_italic;
		break;

	case Markup::HTE_B:
	case Markup::HTE_STRONG:
		font_weight = 7;
		break;

	case Markup::HTE_BLINK:
		decoration_flags |= CSS_TEXT_DECORATION_blink;
		break;

	case Markup::HTE_Q:
		// Add quotes before and after
		element->SetCheckForPseudo(CSS_PSEUDO_CLASS_BEFORE|CSS_PSEUDO_CLASS_AFTER);
		if (!css_properties->GetDecl(CSS_PROPERTY_quotes))
			css_properties->SetDecl(CSS_PROPERTY_quotes, default_style->quotes);
		break;

	case Markup::HTE_ABBR:
	case Markup::HTE_ACRONYM:
		if (element->HasAttr(Markup::HA_TITLE))
		{
			border.bottom.style = CSS_VALUE_dotted;
			border.bottom.width = 1;
		}
		break;

	case Markup::HTE_DIV:
	case Markup::HTE_FIGURE:
	case Markup::HTE_FIGCAPTION:
	case Markup::HTE_SECTION:
	case Markup::HTE_NAV:
	case Markup::HTE_ARTICLE:
	case Markup::HTE_ASIDE:
	case Markup::HTE_HGROUP:
	case Markup::HTE_HEADER:
	case Markup::HTE_FOOTER:
		if (set_display)
			display_type = CSS_VALUE_block;
		break;

	case Markup::HTE_CENTER:
		if (set_display)
			display_type = CSS_VALUE_block;
		text_align = CSS_VALUE_center;
		break;

	case Markup::HTE_TBODY:
	case Markup::HTE_THEAD:
	case Markup::HTE_TFOOT:
		if (set_display)
		{
			if (elm_type == Markup::HTE_TBODY)
				display_type = CSS_VALUE_table_row_group;
			else
				if (elm_type == Markup::HTE_THEAD)
					display_type = CSS_VALUE_table_header_group;
				else
					display_type = CSS_VALUE_table_footer_group;
		}
		if (set_valign)
		{
			vertical_align_type = (CSSValue)(INTPTR)element->GetAttr(Markup::HA_VALIGN, ITEM_TYPE_NUM, (void*)CSS_VALUE_UNSPECIFIED);

			if (vertical_align_type == CSS_VALUE_UNSPECIFIED)
				vertical_align_type = CSS_VALUE_middle;
		}
		break;

	case Markup::HTE_COL:
		if (set_display)
			display_type = CSS_VALUE_table_column;
		break;

	case Markup::HTE_COLGROUP:
		if (set_display)
			display_type = CSS_VALUE_table_column_group;
		break;

	case Markup::HTE_TD:
	case Markup::HTE_TH:
		{
			if (set_bg_color &&
				layout_mode != LAYOUT_SSR &&
				g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableAuthorFontAndColors)))
			{
				bg_color = (long)element->GetAttr(Markup::HA_BGCOLOR, ITEM_TYPE_NUM, (void*)USE_DEFAULT_COLOR);
			}

			if (elm_type == Markup::HTE_TH)
				font_weight = 7;

			if (set_white_space &&
				(!absolute_width || hld_profile->IsInStrictMode()) && // IE7 (and earlier) ignores nowrap on TH/TD if width is set to an absolute value (not %)
				element->HasAttr(Markup::HA_NOWRAP))
				white_space = CSS_VALUE_nowrap;

			// Check if table element has cellpadding.
			HTML_Element* h = element->Parent();
			while (h)
			{
				if (h->Type() == Markup::HTE_TABLE && h->GetInserted() != HE_INSERTED_BY_LAYOUT)
				{
					default_padding_vert = default_padding_horz = (short) h->GetNumAttr(Markup::HA_CELLPADDING, NS_IDX_HTML, DEFAULT_CELL_PADDING);
					break;
				}
				h = h->Parent();
			}

			if (set_valign)
			{
				vertical_align_type = (CSSValue)(INTPTR)element->GetAttr(Markup::HA_VALIGN, ITEM_TYPE_NUM, (void*)CSS_VALUE_UNSPECIFIED);

				if (vertical_align_type == CSS_VALUE_UNSPECIFIED)
					vertical_align_type = CSS_VALUE_inherit;
			}
		}

		if (set_display)
			display_type = CSS_VALUE_table_cell;
		break;

	case Markup::HTE_TR:
		if (set_bg_color &&
			layout_mode != LAYOUT_SSR &&
			g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableAuthorFontAndColors)))
		{
			bg_color = (long)element->GetAttr(Markup::HA_BGCOLOR, ITEM_TYPE_NUM, (void*)USE_DEFAULT_COLOR);
		}

		if (set_display)
			display_type = CSS_VALUE_table_row;

		if (set_valign)
		{
			vertical_align_type = (CSSValue)(INTPTR)element->GetAttr(Markup::HA_VALIGN, ITEM_TYPE_NUM, (void*)CSS_VALUE_UNSPECIFIED);

			if (vertical_align_type == CSS_VALUE_UNSPECIFIED)
			{
				HTML_Element* parent = element->ParentActualStyle();

				vertical_align_type = parent && parent->Type() == Markup::HTE_TABLE ? CSS_VALUE_middle : CSS_VALUE_inherit;
			}
		}
		break;

	case Markup::HTE_FONT:
		if (g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableAuthorFontAndColors)))
		{
			COLORREF font_color = element->GetFontColor();
			if (font_color != USE_DEFAULT_COLOR)
			{
				CSS_long_decl* decl = default_style->font_color;
				decl->SetLongValue(font_color);
				css_properties->SetDecl(CSS_PROPERTY_font_color, decl);
			}
			if (element->GetFontSizeDefined())
			{
				if (!css_properties->GetDecl(CSS_PROPERTY_font_size))
				{
					CSS_type_decl* font_size_decl = default_style->font_size_type;
					font_size_decl->SetTypeValue(CSS_VALUE_use_lang_def);
					css_properties->SetDecl(CSS_PROPERTY_font_size, font_size_decl);
				}
			}
			int tmp_font_number = element->GetFontNumber();
			if (tmp_font_number >= 0 && styleManager->HasFont(tmp_font_number))
				font_number = tmp_font_number;
		}

		break;

	case Markup::HTE_A:
		if (set_fg_color || !css_properties->GetDecl(CSS_PROPERTY_text_decoration) || g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LinkHasFrame))
			GetAnchorStyle(context_elm, set_fg_color, fg_color, decoration_flags, border_default_style, border_default_width);
		break;

	case Markup::HTE_SMALL:
	case Markup::HTE_BIG:
		break;

	case Markup::HTE_SUB:
	case Markup::HTE_SUP:
		if (set_valign)
			vertical_align_type = (elm_type == Markup::HTE_SUB) ? CSS_VALUE_sub : CSS_VALUE_super;
		break;

#ifdef SUPPORT_TEXT_DIRECTION
	case Markup::HTE_BDO:
		if (element->HasNumAttr(Markup::HA_DIR) || !hld_profile->IsInStrictMode())
			unicode_bidi = CSS_VALUE_bidi_override;
		// direction will be set by default further down
		break;
#endif

	case Markup::HTE_U:
	case Markup::HTE_INS:
		decoration_flags = CSS_TEXT_DECORATION_underline;
		break;

	case Markup::HTE_S:
	case Markup::HTE_STRIKE:
	case Markup::HTE_DEL:
		decoration_flags = CSS_TEXT_DECORATION_line_through;
		break;

	case Markup::HTE_MARK:
		if (set_fg_color)
			fg_color = OP_RGB(0, 0, 0);
		if (set_bg_color)
			bg_color = OP_RGB(255, 255, 0);
		break;

	case Markup::HTE_P:
#ifdef _WML_SUPPORT_
		if (set_white_space && element->HasNumAttr(Markup::WMLA_MODE, NS_IDX_WML) && element->GetNumAttr(Markup::WMLA_MODE, NS_IDX_WML) == WATTR_VALUE_nowrap)
			white_space = CSS_VALUE_nowrap;
#endif //_WML_SUPPORT_
	case Markup::HTE_DL:
	case Markup::HTE_DT:
	case Markup::HTE_DD:
		if (set_display)
			display_type = CSS_VALUE_block;
		break;

	case Markup::HTE_UL:
	case Markup::HTE_OL:
		if (set_display)
			display_type = CSS_VALUE_block;
		if (!css_properties->GetDecl(CSS_PROPERTY_list_style_type))
		{
			CSS_type_decl* decl = default_style->list_style_type;
			decl->SetTypeValue(element->GetListType());
			css_properties->SetDecl(CSS_PROPERTY_list_style_type, decl);
		}
		if (!css_properties->GetDecl(CSS_PROPERTY_list_style_position))
		{
			CSS_type_decl* decl = default_style->list_style_pos;
			decl->SetTypeValue(CSS_VALUE_outside);
			css_properties->SetDecl(CSS_PROPERTY_list_style_position, decl);
		}
		break;

	case Markup::HTE_LI:
		if (!css_properties->GetDecl(CSS_PROPERTY_list_style_position))
		{
			HTML_Element* h = element->ParentActualStyle();
			if (h && h->Type() != Markup::HTE_UL && h->Type() != Markup::HTE_OL && h->Type() != Markup::HTE_DIR && h->Type() != Markup::HTE_MENU)
			{
				CSS_type_decl* decl = default_style->list_style_pos;
				decl->SetTypeValue(CSS_VALUE_inside);
				css_properties->SetDecl(CSS_PROPERTY_list_style_position, decl);
			}
		}
		if (set_display)
			display_type = CSS_VALUE_list_item;
		if (!css_properties->GetDecl(CSS_PROPERTY_list_style_type) && element->HasAttr(Markup::HA_TYPE))
		{
			CSS_type_decl* decl = default_style->list_style_type;
			decl->SetTypeValue(element->GetListType());
			css_properties->SetDecl(CSS_PROPERTY_list_style_type, decl);
		}
		break;

	case Markup::HTE_CAPTION:
		{
			if (set_display)
				display_type = CSS_VALUE_table_caption;

			if (html_align != CSS_VALUE_left && html_align != CSS_VALUE_right)
			{
				if (html_align == CSS_VALUE_bottom && !css_properties->GetDecl(CSS_PROPERTY_caption_side))
					css_properties->SetDecl(CSS_PROPERTY_caption_side, default_style->caption_side);

				html_align = CSS_VALUE_center;
			}
		}
		break;

	case Markup::HTE_NOBR:
		if (set_white_space)
			white_space = CSS_VALUE_nowrap;
		break;

	case Markup::HTE_BR:
		{
			// CHECK: should we support 'clear' attribute for other elements than BR ?
			if (!css_properties->GetDecl(CSS_PROPERTY_clear))
			{
				int html_clear = (int) element->GetNumAttr(Markup::HA_CLEAR, NS_IDX_HTML, CLEAR_NONE);
				CSSValue clear;
				switch (html_clear)
				{
				case CLEAR_LEFT:
					clear = CSS_VALUE_left; break;
				case CLEAR_RIGHT:
					clear = CSS_VALUE_right; break;
				case CLEAR_ALL:
					clear = CSS_VALUE_both; break;
				default:
					clear = CSS_VALUE_UNSPECIFIED; break;
				}
				if (clear != CSS_VALUE_UNSPECIFIED)
				{
					CSS_type_decl* decl = default_style->clear;
					decl->SetTypeValue(clear);
					css_properties->SetDecl(CSS_PROPERTY_clear, decl);
				}
			}
		}
		break;

	case Markup::HTE_HR:
		{
			if (set_display)
				display_type = CSS_VALUE_block;

			if (html_align != CSS_VALUE_right && !css_properties->GetDecl(CSS_PROPERTY_margin_right))
				css_properties->SetDecl(CSS_PROPERTY_margin_right, default_style->margin_right);
			if (html_align != CSS_VALUE_left && !css_properties->GetDecl(CSS_PROPERTY_margin_left))
				css_properties->SetDecl(CSS_PROPERTY_margin_left, default_style->margin_left);

			html_align = CSS_VALUE_UNSPECIFIED;

			int content_height = (int)(INTPTR)element->GetAttr(Markup::HA_SIZE, ITEM_TYPE_NUM, (void*)(INTPTR)2, NS_IDX_DEFAULT);

			COLORREF color = element->GetFontColor();
			if (!element->HasAttr(Markup::HA_NOSHADE))
			{
				border_default_width = 1;
				if (color == USE_DEFAULT_COLOR)
				{
					border_default_style = CSS_VALUE_inset;
				}
				else
				{
					border_default_color = color;
					border_default_style = CSS_VALUE_solid;
					if (set_bg_color)
						bg_color = color;
				}

				if (content_height > 2)
					content_height -= 2;
				else
					content_height = 0;
			}
			else
			{
				if (set_bg_color)
				{
					if (color != USE_DEFAULT_COLOR)
						bg_color = color;
					else
						bg_color = OP_RGB(128, 128, 128);
				}

				if (content_height < 1)
					content_height = 1;
			}

			if (!css_properties->GetDecl(CSS_PROPERTY_height))
			{
				default_style->content_height->SetNumberValue(0, (float)content_height, CSS_PX);
				css_properties->SetDecl(CSS_PROPERTY_height, default_style->content_height);
			}
		}

		break;

	case Markup::HTE_BODY:
		if (set_display)
			display_type = CSS_VALUE_block;

		if (layout_mode != LAYOUT_SSR)
		{
			if (g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableAuthorFontAndColors)))
			{
				if (set_bg_color)
					bg_color = hld_profile->GetBgColor();
				if (!css_properties->GetDecl(CSS_PROPERTY_background_attachment) && element->HasAttr(Markup::HA_BGPROPERTIES))
					css_properties->SetDecl(CSS_PROPERTY_background_attachment, default_style->bg_attach);
				if (set_fg_color && element->HasAttr(Markup::HA_TEXT))
				{
					COLORREF tmp_color = element->GetTextColor();
					if (tmp_color != USE_DEFAULT_COLOR)
						fg_color = tmp_color;
				}
			}

			has_left_margin = element->HasNumAttr(Markup::HA_LEFTMARGIN);
			has_top_margin = element->HasNumAttr(Markup::HA_TOPMARGIN);

			if (has_left_margin)
				margin_padding[MARGIN_LEFT] = (short)(INTPTR)element->GetAttr(Markup::HA_LEFTMARGIN, ITEM_TYPE_NUM, (void*)(long)margin_padding[MARGIN_LEFT]);
			else if (element->HasNumAttr(Markup::HA_MARGINWIDTH))
			{
				has_left_margin = TRUE;
				margin_padding[MARGIN_LEFT] = (short)(INTPTR)element->GetAttr(Markup::HA_MARGINWIDTH, ITEM_TYPE_NUM, (void*)(long)default_padding_horz);
			}

			if (has_top_margin)
				margin_padding[MARGIN_TOP] = (short)(INTPTR)element->GetAttr(Markup::HA_TOPMARGIN, ITEM_TYPE_NUM, (void*)(long)margin_padding[MARGIN_TOP]);
			else if (element->HasNumAttr(Markup::HA_MARGINHEIGHT))
			{
				has_top_margin = TRUE;
				margin_padding[MARGIN_TOP] = (short)(INTPTR)element->GetAttr(Markup::HA_MARGINHEIGHT, ITEM_TYPE_NUM, (void*)(long)default_padding_vert);
			}

			if (!has_left_margin || !has_top_margin)
			{
				FramesDocElm* frm_doc_elm = m_doc->GetDocManager()->GetFrame();
				if (frm_doc_elm)
				{
					if (!has_left_margin)
						margin_padding[MARGIN_LEFT] = frm_doc_elm->GetFrameMarginWidth();
					if (!has_top_margin)
						margin_padding[MARGIN_TOP] = frm_doc_elm->GetFrameMarginHeight();
				}
				else
				{
					if (!has_left_margin)
						margin_padding[MARGIN_LEFT] = 8;
					if (!has_top_margin)
						margin_padding[MARGIN_TOP] = 8;
				}
				has_left_margin = has_top_margin = TRUE;
			}
		}
		break;

	case Markup::HTE_VIDEO:
		if (!css_properties->GetDecl(CSS_PROPERTY__o_object_fit))
		{
			css_properties->SetDecl(CSS_PROPERTY__o_object_fit, default_style->object_fit);
		}
		break;

	case Markup::HTE_PRE:
	case Markup::HTE_XMP:
	case Markup::HTE_LISTING:
	case Markup::HTE_PLAINTEXT:
		if (set_white_space)
			if (elm_type == Markup::HTE_PRE && element->HasAttr(Markup::HA_WRAP))
				white_space = CSS_VALUE_pre_wrap;
			else
				white_space = CSS_VALUE_pre;
		// fall through

	default:
		{
			WritingSystem::Script script = WritingSystem::Unknown;
#ifdef FONTSWITCHING
			script = hld_profile->GetPreferredScript();
#endif // FONTSWITCHING
			const PresentationAttr& p_attr = style->GetPresentationAttr();
			FontAtt* font = p_attr.GetPresentationFont(script).Font;
			if (font)
			{
				if (set_display &&
					elm_type != Markup::HTE_CODE && elm_type != Markup::HTE_SAMP &&
					elm_type != Markup::HTE_TT && elm_type != Markup::HTE_KBD)
					display_type = CSS_VALUE_block;

				BOOL use_user_font_setting = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableUserFontAndColors));

				if (set_fg_color && use_user_font_setting && (
					!g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableAuthorFontAndColors))))
				{
					fg_color = p_attr.Color;
				}

				if (use_user_font_setting && !p_attr.InheritFontSize)
					font_size = op_abs(font->GetHeight());
				else
				{
					switch (elm_type)
					{
					case Markup::HTE_H1:
						rel_font_size = 2.0f;
						break;
					case Markup::HTE_H2:
						rel_font_size = 1.5f;
						break;
					case Markup::HTE_H3:
						rel_font_size = 1.17f;
						break;
					case Markup::HTE_H5:
						rel_font_size = 0.83f;
						break;
					case Markup::HTE_H6:
						rel_font_size = 0.67f;
						break;
					case Markup::HTE_PRE:
					case Markup::HTE_LISTING:
					case Markup::HTE_PLAINTEXT:
					case Markup::HTE_XMP:
					case Markup::HTE_CODE:
					case Markup::HTE_SAMP:
					case Markup::HTE_TT:
					case Markup::HTE_KBD:
						rel_font_size = 0.8125f;
						break;
					default:
						break;
					}
				}

				if (g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableUserFontAndColors)) ||
					elm_type == Markup::HTE_PRE || elm_type == Markup::HTE_LISTING || elm_type == Markup::HTE_PLAINTEXT ||
					elm_type == Markup::HTE_XMP || elm_type == Markup::HTE_CODE || elm_type == Markup::HTE_SAMP ||
					elm_type == Markup::HTE_TT || elm_type == Markup::HTE_KBD)
				{
					if (!css_properties->GetDecl(CSS_PROPERTY_font_family))
					{
						CSS_type_decl* font_family_type = default_style->font_family_type;
						font_family_type->SetTypeValue(CSS_VALUE_use_lang_def);
						css_properties->SetDecl(CSS_PROPERTY_font_family, font_family_type);
					}
				}

				if (font->GetWeight() != 4)
					font_weight = font->GetWeight();
				if (p_attr.Italic)
					font_italic = CSS_VALUE_italic;
			}
			break;
		}
	}

	CSSValue float_type = CSS_VALUE_none;
	if (replaced_set_html_align)
	{
		if (html_align == CSS_VALUE_left ||
			html_align == CSS_VALUE_right)
		{
			if (html_align == CSS_VALUE_left)
				float_type = CSS_VALUE_left;
			else
				float_type = CSS_VALUE_right;
			html_align = CSS_VALUE_UNSPECIFIED;

			if (!css_properties->GetDecl(CSS_PROPERTY_float))
			{
				CSS_type_decl* decl = default_style->float_decl;
				decl->SetTypeValue(float_type);
				css_properties->SetDecl(CSS_PROPERTY_float, decl);
			}
		}
		else
			float_type = CSS_VALUE_none;

		if (set_valign && elm_type != Markup::HTE_TABLE)
			if (html_align == CSS_VALUE_top ||
				html_align == CSS_VALUE_middle ||
				html_align == CSS_VALUE_text_bottom)
				 vertical_align_type = html_align;
	}

	if (html_align != CSS_VALUE_UNSPECIFIED)
	{
		text_align = html_align;
	}

	if (set_display && element->GetBoolAttr(Markup::HA_HIDDEN))
	{
		display_type = CSS_VALUE_none;
	}

#ifdef SUPPORT_TEXT_DIRECTION
	if (!css_properties->GetDecl(CSS_PROPERTY_direction) && element->HasNumAttr(Markup::HA_DIR)) // dont mess with dir if it's not there, it's inherited
	{
		CSSValue direction = CSSValue((INTPTR)element->GetAttr(Markup::HA_DIR, ITEM_TYPE_NUM, (void*) 0));

		if (direction)
		{
			CSS_type_decl* decl = default_style->direction;
			decl->SetTypeValue(direction);
			css_properties->SetDecl(CSS_PROPERTY_direction, decl);

			if (unicode_bidi != CSS_VALUE_bidi_override)
				unicode_bidi = CSS_VALUE_embed;
		}
	}

	if (unicode_bidi != CSS_VALUE_UNSPECIFIED && !css_properties->GetDecl(CSS_PROPERTY_unicode_bidi))
	{
		CSS_type_decl* decl = default_style->unicode_bidi;
		decl->SetTypeValue(unicode_bidi);
		css_properties->SetDecl(CSS_PROPERTY_unicode_bidi, decl);
	}
#endif

	if (element->IsMatchingType(Markup::HTE_DATALIST, NS_HTML))
		display_type = CSS_VALUE_none;

	BOOL margin_v_specified = FALSE;

	if (replaced_elm)
	{
		margin_padding[MARGIN_LEFT] = margin_padding[MARGIN_RIGHT] = short((INTPTR)element->GetAttr(Markup::HA_HSPACE, ITEM_TYPE_NUM, (void*)(INTPTR)0));
		margin_padding[MARGIN_TOP] = margin_padding[MARGIN_BOTTOM] = short((INTPTR)element->GetAttr(Markup::HA_VSPACE, ITEM_TYPE_NUM, (void*)(INTPTR)0));
	}
	else
		if (float_type == CSS_VALUE_none)
		{
			BOOL drop_vertical_margin = FALSE;
			if (elm_type == Markup::HTE_UL || elm_type == Markup::HTE_OL)
			{
				HTML_Element* h = element->Parent();
				while (h)
				{
					if (h->Type() == Markup::HTE_UL || h->Type() == Markup::HTE_OL)
					{
						drop_vertical_margin = TRUE;
						break;
					}
					h = h->Parent();
				}
			}

			if (elm_type == Markup::HTE_FIELDSET)
			{
				/* Apply default vertical "separation" in the style settings
				   for this element to the padding properties instead of the
				   margin properties. */

				margin_padding[PADDING_TOP] = (short)lattr.LeadingSeparation;
				margin_padding[PADDING_BOTTOM] = (short)lattr.TrailingSeparation;
				has_vertical_padding = TRUE;
			}
			else if (!has_top_margin && !drop_vertical_margin)
			{
				if (elm_type == Markup::HTE_FORM)
				{
					margin_padding[MARGIN_TOP] = (short)lattr.LeadingSeparation;
					/*
					 * CORE-23030 Forms in standards mode should not have any margin-bottom,
					 * unlike quirks mode in which it has margin-bottom:1em
					 *
					 * CORE-4808 form sandwiched between table
					 * elements should not be rendered. Unfortunately,
					 * because the parser currently doesn't move the form's
					 * contents outside the table, we can only remove all
					 * the margin, so it doesn't affect table layout
					 */
					if (hld_profile->IsInStrictMode())
					{
						margin_padding[MARGIN_BOTTOM] = 0;
					}
					else
					{
						HTML_Element* he_parent = element->ParentActualStyle();
						if (he_parent &&
								(he_parent->Type() == Markup::HTE_TABLE ||
								he_parent->Type() == Markup::HTE_THEAD ||
								he_parent->Type() == Markup::HTE_TBODY ||
								he_parent->Type() == Markup::HTE_TFOOT||
								he_parent->Type() == Markup::HTE_TR)
							)
						{
							margin_padding[MARGIN_BOTTOM] = 0;
						}
						else
							margin_padding[MARGIN_BOTTOM] = (short)lattr.TrailingSeparation;
					}
				}
				else
				{
					margin_padding[MARGIN_TOP] = (short)lattr.LeadingSeparation;
					margin_padding[MARGIN_BOTTOM] = (short)lattr.TrailingSeparation;
				}
			}

			if (elm_type == Markup::HTE_UL || elm_type == Markup::HTE_OL || elm_type == Markup::HTE_FIELDSET ||
				elm_type == Markup::HTE_DIR || elm_type == Markup::HTE_MENU)
			{
				/* Apply default horizontal "offset" in the style settings for
				   this element to the padding properties instead of the margin
				   properties. */

				margin_padding[PADDING_LEFT] = (short)lattr.LeftHandOffset;
				margin_padding[PADDING_RIGHT] = (short)lattr.RightHandOffset;
				has_horizontal_padding = TRUE;
			}
			else if (!has_left_margin)
			{
				margin_padding[MARGIN_LEFT] = (short)lattr.LeftHandOffset;
				margin_padding[MARGIN_RIGHT] = (short)lattr.RightHandOffset;
			}

			if (has_top_margin && !has_bottom_margin)
				margin_padding[MARGIN_BOTTOM] = margin_padding[MARGIN_TOP];
			if (has_left_margin && !has_right_margin)
				margin_padding[MARGIN_RIGHT] = margin_padding[MARGIN_LEFT];

			if (hld_profile->IsInStrictMode() || elm_type == Markup::HTE_BODY)
			{
				margin_v_specified = TRUE;
			}
		}

	if (!has_horizontal_padding)
		margin_padding[PADDING_LEFT] = margin_padding[PADDING_RIGHT] = default_padding_horz;

	if (!has_vertical_padding)
	{
		margin_padding[PADDING_TOP] = default_padding_vert;
		if (margin_padding[PADDING_BOTTOM] < 0)
			margin_padding[PADDING_BOTTOM] = default_padding_vert;
	}

	if (margin_h_auto)
	{
		if (!css_properties->GetDecl(CSS_PROPERTY_margin_left))
			css_properties->SetDecl(CSS_PROPERTY_margin_left, default_style->margin_left);
		if (!css_properties->GetDecl(CSS_PROPERTY_margin_right))
			css_properties->SetDecl(CSS_PROPERTY_margin_right, default_style->margin_right);
	}

	const short margin_padding_props[MARGIN_PADDING_SIZE] = {
		CSS_PROPERTY_margin_left,
		CSS_PROPERTY_margin_right,
		CSS_PROPERTY_margin_top,
		CSS_PROPERTY_margin_bottom,
		CSS_PROPERTY_padding_left,
		CSS_PROPERTY_padding_right,
		CSS_PROPERTY_padding_top,
		CSS_PROPERTY_padding_bottom
	};

	for (int i=0; i<MARGIN_PADDING_SIZE; i++)
	{
		if (margin_padding[i] != 0 && !css_properties->GetDecl(margin_padding_props[i]))
		{
			CSS_number_decl* decl = default_style->margin_padding[i];
			if (margin_padding[i] > 0)
				decl->SetNumberValue(0, margin_padding[i], CSS_PX);
			else
				decl->SetNumberValue(0, (-margin_padding[i] / 24.0f), CSS_EM);
			if (i == MARGIN_TOP || i == MARGIN_BOTTOM)
				decl->SetUnspecified(!margin_v_specified);
			css_properties->SetDecl(margin_padding_props[i], decl);
		}
	}

	if (border_default_color != USE_DEFAULT_COLOR)
	{
		CSS_long_decl* decl = default_style->border_color;
		decl->SetLongValue(border_default_color);
		if (!css_properties->GetDecl(CSS_PROPERTY_border_left_color))
			css_properties->SetDecl(CSS_PROPERTY_border_left_color, decl);
		if (!css_properties->GetDecl(CSS_PROPERTY_border_right_color))
			css_properties->SetDecl(CSS_PROPERTY_border_right_color, decl);
		if (!css_properties->GetDecl(CSS_PROPERTY_border_top_color))
			css_properties->SetDecl(CSS_PROPERTY_border_top_color, decl);
		if (!css_properties->GetDecl(CSS_PROPERTY_border_bottom_color))
			css_properties->SetDecl(CSS_PROPERTY_border_bottom_color, decl);
	}
	if (border_default_style != CSS_VALUE_none)
	{
		border.top.style = border_default_style;
		border.left.style = border_default_style;
		border.right.style = border_default_style;
		border.bottom.style = border_default_style;
	}
	if (border_default_width != CSS_BORDER_WIDTH_MEDIUM)
	{
		border.top.width = border_default_width;
		border.left.width = border_default_width;
		border.right.width = border_default_width;
		border.bottom.width = border_default_width;
	}
	if (border.left.style != BORDER_STYLE_NOT_SET && !css_properties->GetDecl(CSS_PROPERTY_border_left_style))
	{
		CSS_type_decl* decl = default_style->border_left_style;
		decl->SetTypeValue(border.left.GetStyle());
		css_properties->SetDecl(CSS_PROPERTY_border_left_style, decl);
	}
	if (border.right.style != BORDER_STYLE_NOT_SET && !css_properties->GetDecl(CSS_PROPERTY_border_right_style))
	{
		CSS_type_decl* decl = default_style->border_right_style;
		decl->SetTypeValue(border.right.GetStyle());
		css_properties->SetDecl(CSS_PROPERTY_border_right_style, decl);
	}
	if (border.top.style != BORDER_STYLE_NOT_SET && !css_properties->GetDecl(CSS_PROPERTY_border_top_style))
	{
		CSS_type_decl* decl = default_style->border_top_style;
		decl->SetTypeValue(border.top.GetStyle());
		css_properties->SetDecl(CSS_PROPERTY_border_top_style, decl);
	}
	if (border.bottom.style != BORDER_STYLE_NOT_SET && !css_properties->GetDecl(CSS_PROPERTY_border_bottom_style))
	{
		CSS_type_decl* decl = default_style->border_bottom_style;
		decl->SetTypeValue(border.bottom.GetStyle());
		css_properties->SetDecl(CSS_PROPERTY_border_bottom_style, decl);
	}
	if (border.left.width != BORDER_WIDTH_NOT_SET && !css_properties->GetDecl(CSS_PROPERTY_border_left_width))
	{
		CSS_number_decl* decl = default_style->border_left_width;
		decl->SetNumberValue(0, (float)border.left.width, CSS_PX);
		css_properties->SetDecl(CSS_PROPERTY_border_left_width, decl);
	}
	if (border.right.width != BORDER_WIDTH_NOT_SET && !css_properties->GetDecl(CSS_PROPERTY_border_right_width))
	{
		CSS_number_decl* decl = default_style->border_right_width;
		decl->SetNumberValue(0, (float)border.right.width, CSS_PX);
		css_properties->SetDecl(CSS_PROPERTY_border_right_width, decl);
	}
	if (border.top.width != BORDER_WIDTH_NOT_SET && !css_properties->GetDecl(CSS_PROPERTY_border_top_width))
	{
		CSS_number_decl* decl = default_style->border_top_width;
		decl->SetNumberValue(0, (float)border.top.width, CSS_PX);
		css_properties->SetDecl(CSS_PROPERTY_border_top_width, decl);
	}
	if (border.bottom.width != BORDER_WIDTH_NOT_SET && !css_properties->GetDecl(CSS_PROPERTY_border_bottom_width))
	{
		CSS_number_decl* decl = default_style->border_bottom_width;
		decl->SetNumberValue(0, (float)border.bottom.width, CSS_PX);
		css_properties->SetDecl(CSS_PROPERTY_border_bottom_width, decl);
	}

	if (display_type != CSS_VALUE_UNSPECIFIED)
	{
		CSS_type_decl* decl = default_style->display_type;
		decl->SetTypeValue(display_type);
		css_properties->SetDecl(CSS_PROPERTY_display, decl);
	}
	if (white_space != CSS_VALUE_UNSPECIFIED)
	{
		CSS_type_decl* decl = default_style->white_space;
		decl->SetTypeValue(white_space);
		css_properties->SetDecl(CSS_PROPERTY_white_space, decl);
	}
	if (bg_color != USE_DEFAULT_COLOR)
	{
		CSS_long_decl* decl = default_style->bg_color;
		decl->SetLongValue(bg_color);
		css_properties->SetDecl(CSS_PROPERTY_background_color, decl);
	}
	if (text_align != CSS_VALUE_UNSPECIFIED && !css_properties->GetDecl(CSS_PROPERTY_text_align))
	{
		CSS_type_decl* decl = default_style->text_align;
		decl->SetTypeValue(text_align);
		css_properties->SetDecl(CSS_PROPERTY_text_align, decl);
	}
	if (vertical_align_type != CSS_VALUE_UNSPECIFIED)
	{
		CSS_type_decl* decl = default_style->vertical_align;
		decl->SetTypeValue(vertical_align_type);
		css_properties->SetDecl(CSS_PROPERTY_vertical_align, decl);
	}
	if (text_indent != INT_MIN)
	{
		CSS_number_decl* decl = default_style->text_indent;
		decl->SetNumberValue(0, (float)text_indent, CSS_PX);
		css_properties->SetDecl(CSS_PROPERTY_text_indent, decl);
	}
	if (decoration_flags && !css_properties->GetDecl(CSS_PROPERTY_text_decoration))
	{
		CSS_type_decl* decl = default_style->text_decoration;
		/* Text decoration flags are stored as CSSValue, carefully chosen not to
		   collide with other valid values for text-decoration. */
		decl->SetTypeValue(CSSValue(decoration_flags));
		css_properties->SetDecl(CSS_PROPERTY_text_decoration, decl);
	}
	if (fg_color != USE_DEFAULT_COLOR)
	{
		CSS_long_decl* decl = default_style->fg_color;
		decl->SetLongValue(fg_color);
		css_properties->SetDecl(CSS_PROPERTY_color, decl);
	}
	if ((font_size > 0 || rel_font_size != 0) && !css_properties->GetDecl(CSS_PROPERTY_font_size))
	{
		if (rel_font_size == 0)
			rel_font_size = font_size;
		CSS_number_decl* decl = default_style->font_size;
		decl->SetNumberValue(0, rel_font_size, font_size > 0 ? CSS_PX : CSS_EM);
		css_properties->SetDecl(CSS_PROPERTY_font_size, decl);
	}
	if (font_weight > 0 && !css_properties->GetDecl(CSS_PROPERTY_font_weight))
	{
		CSS_number_decl* decl = default_style->font_weight;
		decl->SetNumberValue(0, font_weight, CSS_NUMBER);
		css_properties->SetDecl(CSS_PROPERTY_font_weight, decl);
	}
	if (font_number >= 0 && !css_properties->GetDecl(CSS_PROPERTY_font_family))
	{
		CSS_heap_gen_array_decl* decl = default_style->font_family;
		/* Both font numbers and CSSValues are stored in the declaration array. A font number is
		   stored as a plain CSSValue, while a "real" CSSValue is tagged with
		   CSS_GENERIC_FONT_FAMILY. */
		decl->GenArrayValueRep()->SetType(CSSValue(font_number));
		css_properties->SetDecl(CSS_PROPERTY_font_family, decl);
	}

	if (font_italic != CSS_VALUE_UNSPECIFIED && !css_properties->GetDecl(CSS_PROPERTY_font_style))
	{
		CSS_type_decl* decl = default_style->font_style;
		decl->SetTypeValue(font_italic);
		css_properties->SetDecl(CSS_PROPERTY_font_style, decl);
	}

	const uni_char* uni_lang = element->GetStringAttr(Markup::HA_LANG);
	if (uni_lang && *uni_lang)
	{
		WritingSystem::Script current_script = WritingSystem::FromLanguageCode(uni_lang);
		CSS_long_decl* decl = default_style->writing_system;
		decl->SetLongValue((long)current_script);
		css_properties->SetDecl(CSS_PROPERTY_writing_system, decl);
	}

#ifdef DOM_FULLSCREEN_MODE
	if (context_elm->HasFullscreenStyles())
		ApplyFullscreenProperties(context_elm, css_properties);
#endif // DOM_FULLSCREEN_MODE

	return OpStatus::OK;
}

#ifdef MEDIA_HTML_SUPPORT

void CSSCollection::GetDefaultCueStyleProperties(HTML_Element* element, CSS_Properties* css_properties)
{
	Markup::Type element_type = element->Type();
	StyleModule::DefaultStyle* default_style = &g_opera->style_module.default_style;

	switch (element_type)
	{
	case Markup::CUEE_ROOT:
		{
			/* cue box "root"

			   color: 'white'
			   font: 5vh sans-serif
			   white-space: pre-line */
			MediaCueDisplayState* cuestate = MediaCueDisplayState::GetFromHtmlElement(element);
			if (!cuestate)
				break;

			if (!css_properties->GetDecl(CSS_PROPERTY_color))
			{
				CSS_long_decl* decl = default_style->fg_color;
				decl->SetLongValue(OP_RGB(255, 255, 255));
				css_properties->SetDecl(CSS_PROPERTY_color, decl);
			}

			if (!css_properties->GetDecl(CSS_PROPERTY_white_space))
			{
				CSS_type_decl* decl = default_style->white_space;
				decl->SetTypeValue(CSS_VALUE_pre_line);
				css_properties->SetDecl(CSS_PROPERTY_white_space, decl);
			}

			float font_size = cuestate->GetFontSize();

			if (font_size != 0 && !css_properties->GetDecl(CSS_PROPERTY_font_size))
			{
				CSS_number_decl* decl = default_style->font_size;
				decl->SetNumberValue(0, font_size, CSS_PX);
				css_properties->SetDecl(CSS_PROPERTY_font_size, decl);
			}

			short font_number = styleManager->GetGenericFontNumber(StyleManager::SANS_SERIF,
																   cuestate->GetScript());
			if (font_number >= 0 && !css_properties->GetDecl(CSS_PROPERTY_font_family))
			{
				CSS_heap_gen_array_decl* decl = default_style->font_family;
				decl->GenArrayValueRep()->SetType(CSSValue(font_number));
				css_properties->SetDecl(CSS_PROPERTY_font_family, decl);
			}

			/* The following properties are "external input" used to get the
			   correct behavior out of the layout engine.

			   Some of the below ('direction', 'text-align' and 'width') stem
			   from the WebVTT rendering specification. */

			OP_ASSERT(!css_properties->GetDecl(CSS_PROPERTY_display));

			CSS_type_decl* display_decl = default_style->display_type;
			display_decl->SetTypeValue(CSS_VALUE_block);
			css_properties->SetDecl(CSS_PROPERTY_display, display_decl);

			OP_ASSERT(!css_properties->GetDecl(CSS_PROPERTY_width));

			CSS_number_decl* width_decl = default_style->content_width;
			width_decl->SetNumberValue(0, (float)cuestate->GetRect().width, CSS_PX);
			css_properties->SetDecl(CSS_PROPERTY_width, width_decl);

			OP_ASSERT(!css_properties->GetDecl(CSS_PROPERTY_direction));

			CSS_type_decl* dir_decl = default_style->direction;
			dir_decl->SetTypeValue(cuestate->GetDirection());
			css_properties->SetDecl(CSS_PROPERTY_direction, dir_decl);

			OP_ASSERT(!css_properties->GetDecl(CSS_PROPERTY_text_align));

			CSS_type_decl* text_align_decl = default_style->text_align;
			text_align_decl->SetTypeValue(cuestate->GetAlignment());
			css_properties->SetDecl(CSS_PROPERTY_text_align, text_align_decl);

			OP_ASSERT(!css_properties->GetDecl(CSS_PROPERTY_overflow_wrap));

			css_properties->SetDecl(CSS_PROPERTY_overflow_wrap, default_style->overflow_wrap);

			/* The following style is really for the "background box", but we
			   let it apply to the root, and then inherit it to the
			   background-box and skip applying it for the root

			   background: rgba(0,0,0,0.8) */

			if (!css_properties->GetDecl(CSS_PROPERTY_background_color))
			{
				CSS_long_decl* decl = default_style->bg_color;
				decl->SetLongValue(OP_RGBA(0, 0, 0, 204));
				css_properties->SetDecl(CSS_PROPERTY_background_color, decl);
			}
		}
		break;

	case Markup::CUEE_I: /* 'i' - font-style: italic */
		if (!css_properties->GetDecl(CSS_PROPERTY_font_style))
		{
			CSS_type_decl* decl = default_style->font_style;
			decl->SetTypeValue(CSS_VALUE_italic);
			css_properties->SetDecl(CSS_PROPERTY_font_style, decl);
		}
		break;

	case Markup::CUEE_B: /* 'b' - font-weight: bold */
		if (!css_properties->GetDecl(CSS_PROPERTY_font_weight))
		{
			CSS_number_decl* decl = default_style->font_weight;
			decl->SetNumberValue(0, 7, CSS_NUMBER);
			css_properties->SetDecl(CSS_PROPERTY_font_weight, decl);
		}
		break;

	case Markup::CUEE_U: /* 'u' - text-decoration: underline */
		if (!css_properties->GetDecl(CSS_PROPERTY_text_decoration))
		{
			CSS_type_decl* decl = default_style->text_decoration;
			decl->SetTypeValue(CSSValue(CSS_TEXT_DECORATION_underline));
			css_properties->SetDecl(CSS_PROPERTY_text_decoration, decl);
		}
		break;

	default:
		break;
	}
}

#endif // MEDIA_HTML_SUPPORT

#ifdef DOM_FULLSCREEN_MODE
void CSSCollection::ApplyFullscreenProperties(CSS_MatchContextElm* context_elm, CSS_Properties* css_properties)
{
	/*	Below is the UA stylesheet for fullscreen elements and ancestors taken
		from a snapshot of the spec draft for Fullscreen API:

		http://dvcs.w3.org/hg/fullscreen/raw-file/tip/Overview.html#rendering


		Applied between author and author important:

		:fullscreen {
		  position:fixed;
		  top:0;
		  left:0;
		  right:0;
		  bottom:0;
		  z-index:2147483647;
		  box-sizing:border-box;
		  margin:0;
		  width:100%;
		  height:100%;
		}
		// If there is a fullscreen element that is not the root then
		// we should hide the viewport scrollbar.
		:fullscreen-ancestor:root {
		  overflow:hidden;
		}
		:fullscreen-ancestor {
		  // Ancestors of a fullscreen element should not induce stacking contexts
		  // that would prevent the fullscreen element from being on top.
		  z-index:auto;
		  // Ancestors of a fullscreen element should not be partially transparent,
		  // since that would apply to the fullscreen element and make the page visible
		  // behind it. It would also create a pseudo-stacking-context that would let content
		  // draw on top of the fullscreen element.
		  opacity:1;
		  // Ancestors of a fullscreen element should not apply SVG masking, clipping, or
		  // filtering, since that would affect the fullscreen element and create a pseudo-
		  // stacking context.
		  mask:none;
		  clip:auto;
		  filter:none;
		  transform:none;
		  transition:none;
		}

		Applied as normal UA style:

		:fullscreen {
		  background: black;
		  color: white;
		}

		// This rule is not in the spec at the time of writing.
		iframe:fullscreen {
		  border-style: none;
		}
	*/

	BOOL true_fullscreen = TRUE;
	StyleModule::DefaultStyle* default_style = &g_opera->style_module.default_style;

	/* Add fullscreen default style between author and author
	   important and update true_fullscreen flag accordingly. */

#define FULLSCREEN_DEFAULT_STYLE(prop, def)								\
	do {																\
		decl = css_properties->GetDecl((prop));							\
		if (!decl || !decl->GetImportant())								\
			css_properties->SetDecl((prop), (def));						\
		else															\
			true_fullscreen = FALSE;									\
	} while (0)

	CSS_decl* decl;
	if (context_elm->IsFullscreenElement())
	{
		FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_position, default_style->fullscreen_fixed);
		FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_top, default_style->fullscreen_top);
		FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_left, default_style->fullscreen_left);
		FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_right, default_style->fullscreen_right);
		FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_bottom, default_style->fullscreen_bottom);
		FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_z_index, default_style->fullscreen_zindex);
		FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_box_sizing, default_style->fullscreen_box_sizing);
		FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_margin_top, default_style->fullscreen_margin_top);
		FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_margin_left, default_style->fullscreen_margin_left);
		FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_margin_right, default_style->fullscreen_margin_right);
		FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_margin_bottom, default_style->fullscreen_margin_bottom);
		FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_width, default_style->fullscreen_width);
		FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_height, default_style->fullscreen_height);

		// UA level style

		decl = css_properties->GetDecl(CSS_PROPERTY_background_color);
		if (!decl || decl == default_style->bg_color)
		{
			CSS_long_decl* bg_decl = default_style->bg_color;
			bg_decl->SetLongValue(OP_RGB(0, 0, 0));
			css_properties->SetDecl(CSS_PROPERTY_background_color, bg_decl);
		}
		else
		{
			CSSDeclType type = decl->GetDeclType();
			if (type == CSS_DECL_COLOR || type == CSS_DECL_LONG)
			{
				COLORREF col = static_cast<COLORREF>(decl->LongValue());
				col = GetRGBA(col);
				if (OP_GET_A_VALUE(col) != 255)
					true_fullscreen = FALSE;
			}
			else
				true_fullscreen = FALSE;
		}

		decl = css_properties->GetDecl(CSS_PROPERTY_color);
		if (!decl || decl == default_style->fg_color)
		{
			CSS_long_decl* fg_decl = default_style->fg_color;
			fg_decl->SetLongValue(OP_RGB(255, 255, 255));
			css_properties->SetDecl(CSS_PROPERTY_color, fg_decl);
		}

		if (context_elm->Type() == Markup::HTE_IFRAME && context_elm->GetNsType() == NS_HTML)
		{
			if (css_properties->GetDecl(CSS_PROPERTY_border_top_style) == default_style->border_top_style)
				css_properties->SetDecl(CSS_PROPERTY_border_top_style, NULL);
			else
				true_fullscreen = FALSE;

			if (css_properties->GetDecl(CSS_PROPERTY_border_left_style) == default_style->border_left_style)
				css_properties->SetDecl(CSS_PROPERTY_border_left_style, NULL);
			else
				true_fullscreen = FALSE;

			if (css_properties->GetDecl(CSS_PROPERTY_border_right_style) == default_style->border_right_style)
				css_properties->SetDecl(CSS_PROPERTY_border_right_style, NULL);
			else
				true_fullscreen = FALSE;

			if (css_properties->GetDecl(CSS_PROPERTY_border_bottom_style) == default_style->border_bottom_style)
				css_properties->SetDecl(CSS_PROPERTY_border_bottom_style, NULL);
			else
				true_fullscreen = FALSE;

			if (css_properties->GetDecl(CSS_PROPERTY_padding_top) ||
				css_properties->GetDecl(CSS_PROPERTY_padding_left) ||
				css_properties->GetDecl(CSS_PROPERTY_padding_right) ||
				css_properties->GetDecl(CSS_PROPERTY_padding_bottom))
				true_fullscreen = FALSE;
		}
	}
	else if (context_elm->IsFullscreenAncestor())
	{
		if (context_elm->IsRoot())
		{
			FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_overflow_x, default_style->fullscreen_overflow_x);
			FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_overflow_y, default_style->fullscreen_overflow_y);
		}

		FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_z_index, default_style->fullscreen_zindex_auto);
		FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_opacity, default_style->fullscreen_opacity);
		FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_mask, default_style->fullscreen_mask);
		FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_clip, default_style->fullscreen_clip);
		FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_filter, default_style->fullscreen_filter);
# ifdef CSS_TRANSFORMS
		FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_transform, default_style->fullscreen_transform);
# endif // CSS_TRANSFORMS
# ifdef CSS_TRANSITIONS
		FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_transition_property, default_style->fullscreen_trans_prop);
		FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_transition_delay, default_style->fullscreen_trans_delay);
		FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_transition_duration, default_style->fullscreen_trans_duration);
		FULLSCREEN_DEFAULT_STYLE(CSS_PROPERTY_transition_timing_function, default_style->fullscreen_trans_timing);
# endif // CSS_TRANSITIONS
	}

#undef FULLSCREEN_DEFAULT_STYLE

	if (!true_fullscreen)
		m_doc->SetFullscreenElementObscured();
}
#endif // DOM_FULLSCREEN_MODE

#ifdef CSS_VIEWPORT_SUPPORT

void CSSCollection::CascadeViewportProperties()
{
	m_viewport.ResetComputedValues();
	HLDocProfile* hld_prof = m_doc->GetHLDocProfile();
	CSS_MediaType media_type = m_doc->GetMediaType();

	const uni_char* hostname = m_doc->GetURL().GetAttribute(URL::KUniHostName).CStr();

	if (g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(hld_prof->GetCSSMode(), PrefsCollectionDisplay::EnableAuthorCSS), hostname) || m_doc->GetWindow()->IsCustomWindow())
	{
		CSSCollectionElement* elm = m_element_list.Last();
		CSS* top_css = NULL;
		while (elm)
		{
			if (elm->IsStyleSheet())
			{
				CSS* css = static_cast<CSS*>(elm);
				if (!css->IsImport())
					top_css = css;

				// All imported stylesheets must have an ascendent stylesheet which is stored as top_css.
				OP_ASSERT(top_css != NULL);

				BOOL skip_children = TRUE;
				if (css->IsEnabled() && css->CheckMedia(m_doc, media_type))
				{
					css->GetViewportProperties(m_doc, &m_viewport, media_type);
					skip_children = FALSE;
				}

				elm = css->GetNextImport(skip_children);
				if (!elm)
				{
					elm = top_css;
					while ((elm = static_cast<CSSCollectionElement*>(elm->Pred())) && elm->IsStyleSheet() && static_cast<CSS*>(elm)->IsImport())
						;
				}
			}
			else
			{
				if (elm->IsViewportMeta())
					static_cast<CSS_ViewportMeta*>(elm)->AddViewportProperties(&m_viewport);

				elm = elm->Pred();
			}
		}
	}

	if (g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(hld_prof->GetCSSMode(), PrefsCollectionDisplay::EnableUserCSS), hostname)
		&& (m_doc->GetWindow()->IsNormalWindow() || m_doc->GetWindow()->IsThumbnailWindow()))
	{
#ifdef LOCAL_CSS_FILES_SUPPORT
		// Load all user style sheets defined in preferences
		for (unsigned int j=0; j < g_pcfiles->GetLocalCSSCount(); j++)
		{
			CSS* css = g_cssManager->GetCSS(CSSManager::FirstUserStyle+j);
			while (css)
			{
				BOOL skip_children = TRUE;
				if (css->IsEnabled() && css->CheckMedia(m_doc, media_type))
				{
					css->GetViewportProperties(m_doc, &m_viewport, media_type);
					skip_children = FALSE;
				}
				css = css->GetNextImport(skip_children);
			}
		}

# ifdef EXTENSION_SUPPORT
		// Load all user style sheets defined by extensions
		for (CSSManager::ExtensionStylesheet* extcss = g_cssManager->GetExtensionUserCSS(); extcss;
			 extcss = reinterpret_cast<CSSManager::ExtensionStylesheet*>(extcss->Suc()))
		{
			CSS* css = extcss->css;
			while (css)
			{
				BOOL skip_children = TRUE;
				if (css->IsEnabled() && css->CheckMedia(m_doc, media_type))
				{
					css->GetViewportProperties(m_doc, &m_viewport, media_type);
					skip_children = FALSE;
				}
				css = css->GetNextImport(skip_children);
			}
		}
# endif
#endif // LOCAL_CSS_FILES_SUPPORT
#ifdef PREFS_HOSTOVERRIDE
		CSS* css = m_host_overrides[CSSManager::LocalCSS].css;
		if (!css)
			css = g_cssManager->GetCSS(CSSManager::LocalCSS);
#else
		CSS* css = g_cssManager->GetCSS(CSSManager::LocalCSS);
#endif
		while (css)
		{
			BOOL skip_children = TRUE;
			if (css->IsEnabled() && css->CheckMedia(m_doc, media_type))
			{
				css->GetViewportProperties(m_doc, &m_viewport, media_type);
				skip_children = FALSE;
			}
			css = css->GetNextImport(skip_children);
		}
	}

#ifdef _WML_SUPPORT_
	if (hld_prof->IsWml())
	{
# ifdef PREFS_HOSTOVERRIDE
		CSS* css = m_host_overrides[CSSManager::WML_CSS].css;
		if (!css)
			css = g_cssManager->GetCSS(CSSManager::WML_CSS);
# else
		CSS* css = g_cssManager->GetCSS(CSSManager::WML_CSS);
# endif
		if (css)
			css->GetViewportProperties(m_doc, &m_viewport, media_type);
	}
#endif

#if defined(CSS_MATHML_STYLESHEET) && defined(MATHML_ELEMENT_CODES)
	CSS* css = g_cssManager->GetCSS(CSSManager::MathML_CSS);
	if (css)
		css->GetViewportProperties(m_doc, &m_viewport, media_type);
#endif // CSS_MATHML_STYLESHEET && MATHML_ELEMENT_CODES

#ifdef PREFS_HOSTOVERRIDE
	CSS* browser_css = m_host_overrides[CSSManager::BrowserCSS].css;
	if (!browser_css)
		browser_css = g_cssManager->GetCSS(CSSManager::BrowserCSS);
#else
	CSS* browser_css = g_cssManager->GetCSS(CSSManager::BrowserCSS);
#endif
	while (browser_css)
	{
		BOOL skip_children = TRUE;
		if (browser_css->IsEnabled() && browser_css->CheckMedia(m_doc, media_type))
		{
			browser_css->GetViewportProperties(m_doc, &m_viewport, media_type);
			skip_children = FALSE;
		}
		browser_css = browser_css->GetNextImport(skip_children);
	}

#ifdef SUPPORT_VISUAL_ADBLOCK
	BOOL content_block_mode = m_doc->GetWindow()->GetContentBlockEditMode();
	if (content_block_mode)
	{
# ifdef PREFS_HOSTOVERRIDE
		CSS* block_css = m_host_overrides[CSSManager::ContentBlockCSS].css;
		if (!block_css)
			block_css = g_cssManager->GetCSS(CSSManager::ContentBlockCSS);
# else
		CSS* block_css = g_cssManager->GetCSS(CSSManager::ContentBlockCSS);
# endif
		if (block_css)
			block_css->GetViewportProperties(m_doc, &m_viewport, media_type);
	}
#endif // SUPPORT_VISUAL_ADBLOCK

	BOOL has_fullscreen_elm = FALSE;
#ifdef DOM_FULLSCREEN_MODE
	has_fullscreen_elm = m_doc->GetFullscreenElement() != NULL;
#endif // DOM_FULLSCREEN_MODE

	m_viewport.SetDefaultProperties(has_fullscreen_elm);

	if (m_viewport.HasProperties() && m_doc->GetLayoutMode() != LAYOUT_NORMAL)
		// Viewport META overrides ERA.
		m_doc->CheckERA_LayoutMode();

	if (m_doc->RecalculateLayoutViewSize(FALSE))
	{
		HTML_Element* root = m_doc->GetDocRoot();
		if (root)
			root->MarkDirty(m_doc);
	}
}

#endif // CSS_VIEWPORT_SUPPORT

#ifdef STYLE_MATCH_INLINE_DEFAULT_API
CSS_MatchElement::CSS_MatchElement()
	: m_html_element(NULL)
	, m_pseudo(ELM_NOT_PSEUDO)
{
}

CSS_MatchElement::CSS_MatchElement(HTML_Element* html_element)
	: m_html_element(html_element)
	, m_pseudo(ELM_NOT_PSEUDO)
{
}

CSS_MatchElement::CSS_MatchElement(HTML_Element* html_element, PseudoElementType pseudo)
	: m_html_element(html_element)
	, m_pseudo(pseudo)
{
}

CSS_MatchElement
CSS_MatchElement::GetParent() const
{
	if (IsPseudo())
		return CSS_MatchElement(m_html_element);
	else
		return CSS_MatchElement(m_html_element->ParentActual());
}

BOOL
CSS_MatchElement::IsValid() const
{
	if (!m_html_element)
		return FALSE;

	if (m_html_element->Type() == Markup::HTE_DOC_ROOT)
		return FALSE;

	switch (m_pseudo)
	{
	case ELM_NOT_PSEUDO:
		return TRUE;
	case ELM_PSEUDO_BEFORE:
		return m_html_element->HasBefore();
	case ELM_PSEUDO_AFTER:
		return m_html_element->HasAfter();
	case ELM_PSEUDO_FIRST_LETTER:
		return m_html_element->HasFirstLetter();
	case ELM_PSEUDO_FIRST_LINE:
		return m_html_element->HasFirstLine();
	case ELM_PSEUDO_SELECTION:
	case ELM_PSEUDO_LIST_MARKER:
	default:
		return FALSE; // Unsupported.
	}
}

BOOL
CSS_MatchElement::IsEqual(const CSS_MatchElement& elm) const
{
	return m_html_element == elm.m_html_element && m_pseudo == elm.m_pseudo;
}

/* static */ CSS_decl*
CSS_MatchRuleListener::ResolveDeclaration(CSS_decl* decl)
{
	if (decl->GetDeclType() == CSS_DECL_PROXY)
		decl = static_cast<CSS_proxy_decl*>(decl)->GetRealDecl();

	return decl;
}

#endif

#ifdef DOM_FULLSCREEN_MODE

BOOL CSSCollection::HasComplexFullscreenStyles() const
{
	for (CSSCollectionElement* elm = m_element_list.First(); elm; elm = elm->Suc())
	{
		if (elm->IsStyleSheet())
		{
			CSS* css = static_cast<CSS*>(elm);
			if (css->IsEnabled() && css->HasComplexFullscreenStyles())
				return TRUE;
		}
	}
	return FALSE;
}

void
CSSCollection::ChangeFullscreenElement(HTML_Element* old_elm, HTML_Element* new_elm)
{
	HTML_Element* root = m_doc->GetDocRoot();

	/* Nothing to do. Should not call this method if the fullscreen element didn't change. */
	OP_ASSERT(old_elm != new_elm);
	if (!root || old_elm == new_elm)
		return;

	if (HasComplexFullscreenStyles())
		root->MarkPropsDirty(m_doc);
	else
	{
		HTML_Element* cur = old_elm;
		do
		{
			while (cur && cur->Type() != Markup::HTE_DOC_ROOT)
			{
				cur->MarkPropsDirty(m_doc);
				cur = cur->Parent();
			}
		} while ((cur = new_elm, new_elm = NULL), cur);
	}

# ifdef CSS_VIEWPORT_SUPPORT
	m_viewport.MarkDirty();
	root->MarkDirty(m_doc);
# endif // CSS_VIEWPORT_SUPPORT
}

#endif // DOM_FULLSCREEN_MODE

CSS_MediaQueryList*
CSSCollection::AddMediaQueryList(const uni_char* media_string, CSS_MediaQueryList::Listener* listener)
{
	OP_ASSERT(media_string);
	OP_ASSERT(listener);

	CSS_MediaObject* media_object = OP_NEW(CSS_MediaObject, ());

	if (!media_object || OpStatus::IsMemoryError(media_object->SetMediaText(media_string, uni_strlen(media_string), FALSE)))
	{
		OP_DELETE(media_object);
		return NULL;
	}

	CSS_MediaQueryList* new_query_list = OP_NEW(CSS_MediaQueryList, (media_object, listener, m_doc));

	if (new_query_list)
		new_query_list->Into(&m_query_lists);
	else
		OP_DELETE(media_object);

	return new_query_list;
}
