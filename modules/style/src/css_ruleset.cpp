/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/logdoc/htm_elm.h"
#include "modules/style/src/css_ruleset.h"
#include "modules/style/src/css_selector.h"
#include "modules/probetools/probepoints.h"
#include "modules/style/src/css_parser.h"
#include "modules/style/src/css_conditional_rule.h"

CSS_DeclarationBlockRule::CSS_DeclarationBlockRule(CSS_property_list* pl) : m_prop_list(pl)
{
	if (pl)
		pl->Ref();
}

CSS_DeclarationBlockRule::~CSS_DeclarationBlockRule()
{
	if (m_prop_list)
		m_prop_list->Unref();
}

void
CSS_DeclarationBlockRule::SetPropertyList(HLDocProfile* hld_profile, CSS_property_list* pl, BOOL delete_duplicates)
{
	SetPropertyListInternal(pl);
	if (delete_duplicates)
		pl->PostProcess(TRUE, FALSE);
}

/* virtual */ OP_STATUS
CSS_DeclarationBlockRule::GetCssText(CSS* stylesheet, TempBuffer* buf, unsigned int indent_level)
{
	RETURN_IF_ERROR(buf->Append(" { "));
	TRAP_AND_RETURN(stat, m_prop_list->AppendPropertiesAsStringL(buf));
	return buf->Append(" }");
}

CSS_StyleRule::CSS_StyleRule()
#ifdef SCOPE_CSS_RULE_ORIGIN
	: m_line_number(0)
#endif // SCOPE_CSS_RULE_ORIGIN
{
}

CSS_StyleRule::~CSS_StyleRule()
{
	m_selectors.Clear();
}

void
CSS_StyleRule::SetPropertyList(HLDocProfile* hld_profile, CSS_property_list* pl, BOOL delete_duplicates)
{
	SetPropertyListInternal(pl);

	if (pl)
	{
		unsigned int flags = pl->PostProcess(delete_duplicates, TRUE);

		BOOL has_content = (flags & CSS_property_list::HAS_CONTENT) != 0;
		BOOL has_url_prop = (flags & CSS_property_list::HAS_URL_PROPERTY) != 0;

#ifdef CSS_TRANSITIONS
		if (hld_profile)
		{
			if (flags & CSS_property_list::HAS_TRANSITIONS)
				hld_profile->GetCSSCollection()->SetHasTransitions();
# ifdef CSS_ANIMATIONS
			if (flags & CSS_property_list::HAS_ANIMATIONS)
				hld_profile->GetCSSCollection()->SetHasAnimations();
# endif // CSS_ANIMATIONS
		}
#endif // CSS_TRANSITIONS

		CSS_Selector* css_sel = static_cast<CSS_Selector*>(m_selectors.First());
		while (css_sel)
		{
			css_sel->SetHasContentProperty(has_content);
			css_sel->SetMatchPrefetch(has_url_prop);
			css_sel = static_cast<CSS_Selector*>(css_sel->Suc());
		}
	}
}

void
CSS_StyleRule::AddSelector(CSS_Selector* sel)
{
	sel->Into(&m_selectors);
}

BOOL
CSS_StyleRule::HasTargetElmType(Markup::Type type)
{
	CSS_Selector* sel = FirstSelector();
	while (sel)
	{
		if (sel->GetTargetElmType() == type)
			return TRUE;
		sel = static_cast<CSS_Selector*>(sel->Suc());
	}
	return FALSE;
}

/* virtual */ OP_STATUS
CSS_StyleRule::GetSelectorText(CSS* stylesheet, TempBuffer* buf)
{
	CSS_Selector* css_sel = static_cast<CSS_Selector*>(m_selectors.First());
	while (css_sel)
	{
		RETURN_IF_ERROR(css_sel->GetSelectorText(stylesheet, buf));
		css_sel = css_sel->SucActual();
		if (css_sel)
			RETURN_IF_ERROR(buf->Append(", "));
	}
	return OpStatus::OK;
}

/* virtual */ CSS_PARSE_STATUS
CSS_StyleRule::SetSelectorText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len)
{
	return stylesheet->ParseAndInsertRule(hld_prof, this, GetConditionalRule(), NULL, TRUE, CSS_TOK_DOM_SELECTOR, text, len);
}

/* virtual */ OP_STATUS
CSS_StyleRule::GetCssText(CSS* stylesheet, TempBuffer* buf, unsigned int indent_level)
{
	RETURN_IF_ERROR(GetSelectorText(stylesheet, buf));
	return CSS_DeclarationBlockRule::GetCssText(stylesheet, buf, indent_level);
}

/* virtual */ CSS_PARSE_STATUS
CSS_StyleRule::SetCssText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len)
{
	return stylesheet->ParseAndInsertRule(hld_prof, this, GetConditionalRule(), NULL, TRUE, CSS_TOK_DOM_STYLE_RULE, text, len);
}

/* virtual */ OP_STATUS
CSS_PageRule::GetSelectorText(CSS* stylesheet, TempBuffer* buf)
{
	RETURN_IF_ERROR(buf->Append("@page"));
	CSS_Selector* page_sel = PageSelector();
	CSS_SelectorAttribute* sel_attr;
	if (page_sel && (sel_attr = page_sel->FirstSelector()->GetFirstAttr()))
	{
		RETURN_IF_ERROR(buf->Append(" "));
		return sel_attr->GetSelectorText(buf);
	}
	else
		return OpStatus::OK;
}

/* virtual */ CSS_PARSE_STATUS
CSS_PageRule::SetSelectorText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len)
{
	return stylesheet->ParseAndInsertRule(hld_prof, this, GetConditionalRule(), NULL, TRUE, CSS_TOK_DOM_PAGE_SELECTOR, text, len);
}

/* virtual */ CSS_PARSE_STATUS
CSS_PageRule::SetCssText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len)
{
	return stylesheet->ParseAndInsertRule(hld_prof, this, GetConditionalRule(), NULL, TRUE, CSS_TOK_DOM_PAGE_RULE, text, len);
}
