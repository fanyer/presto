/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
*/

#include "core/pch.h"

#include "modules/doc/frm_doc.h"
#include "modules/style/css.h"
#include "modules/style/css_media.h"
#include "modules/style/css_dom.h"
#include "modules/probetools/probepoints.h"
#include "modules/style/cssmanager.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/dochand/win.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/htm_lex.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/logdoc/link.h"
#include "modules/display/styl_man.h"
#include "modules/util/excepts.h"
#include "modules/util/str.h"
#include "modules/util/tempbuf.h"
#include "modules/util/gen_math.h"
#include "modules/display/vis_dev.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/logdoc/datasrcelm.h"
#include "modules/img/image.h"
#include "modules/layout/box/box.h"
#include "modules/style/src/css_lexer.h"
#include "modules/style/src/css_parser.h"
#include "modules/style/src/css_buffer.h"
#include "modules/style/src/css_ruleset.h"
#include "modules/style/src/css_selector.h"
#include "modules/style/src/css_import_rule.h"
#include "modules/style/src/css_fontface_rule.h"
#include "modules/style/src/css_namespace_rule.h"
#include "modules/style/src/css_viewport_rule.h"

#ifdef STYLE_GETMATCHINGRULES_API
#include "modules/style/css_dom.h"
#endif // STYLE_GETMATCHINGRULES_API

#ifdef SCOPE_PROFILER
#include "modules/probetools/probetimeline.h"
#endif // SCOPE_PROFILER

#include "modules/stdlib/include/double_format.h"

// ***************************

#include "modules/style/src/css_property_strings.h"

// ***************************


int GetCSS_Property(const uni_char* str, int len)
{
	if (len <= CSS_PROPERTY_NAME_MAX_SIZE)
	{
		short end_idx = CSS_property_idx[len+1];
		for (short i=CSS_property_idx[len]; i<end_idx; i++)
			if (uni_strni_eq(str, g_css_property_name[i], len))
				return i;
	}
	return -1;
}

const char* GetCSS_PropertyName(int property)
{
	return g_css_property_name[property];
}

// ***************************

#include "modules/style/src/css_value_strings.h"

// ***************************

CSSValue CSS_GetKeyword(const uni_char* str, int len)
{
	if (len <= CSS_VALUE_NAME_MAX_SIZE)
	{
		short end_idx = CSS_value_idx[len+1];
		for (short i=CSS_value_idx[len]; i<end_idx; i++)
			if (uni_strnicmp(g_css_value_name[i], str, len) == 0)
				return CSSValue(CSS_value_tok[i]);
	}
	return CSS_VALUE_UNKNOWN;
}

const uni_char* CSS_GetKeywordString(short val)
{
	short i = 0;
	for (; i<CSS_VALUE_NAME_SIZE; i++)
	{
		if (CSS_value_tok[i] == val)
			return g_css_value_name[i];
	}
	return NULL;
}

const uni_char* CSS_GetInternalKeywordString(short val)
{
	const uni_char* ret = NULL;
	switch (val)
	{
	case CSS_VALUE_up:
		ret = UNI_L("up");
		break;
	case CSS_VALUE_down:
		ret = UNI_L("down");
		break;
	case CSS_VALUE_use_lang_def:
		ret = UNI_L("use-lang-def");
		break;
	}
	return ret;
}

const int CSS_PSEUDO_MAX_SIZE = 19;

static const short CSS_pseudo_idx[] =
{
	0,	// start idx size 0
	0,	// start idx size 1
	0,	// start idx size 2
	0,	// start idx size 3
	1,	// start idx size 4
	5,	// start idx size 5
	10,	// start idx size 6
	14,	// start idx size 7
	19,	// start idx size 8
	23,	// start idx size 9
	25,	// start idx size 10
	30,	// start idx size 11
	32,	// start idx size 12
	36,	// start idx size 13
	38,	// start idx size 14
	38,	// start idx size 15
	38,	// start idx size 16
	38,	// start idx size 17
	38,	// start idx size 18
	38,	// start idx size 19
	39 // start idx size 20
};

// Update the PSEUDO_CLASS_COUNT values in style_module.h when adding entries.
CONST_ARRAY(g_css_pseudo_name, char*)
	CONST_ENTRY("CUE"),
	CONST_ENTRY("LANG"),
	CONST_ENTRY("LINK"),
	CONST_ENTRY("PAST"),
	CONST_ENTRY("ROOT"),
	CONST_ENTRY("AFTER"),
	CONST_ENTRY("EMPTY"),
	CONST_ENTRY("FOCUS"),
	CONST_ENTRY("HOVER"),
	CONST_ENTRY("VALID"),
	CONST_ENTRY("ACTIVE"),
	CONST_ENTRY("BEFORE"),
	CONST_ENTRY("FUTURE"),
	CONST_ENTRY("TARGET"),
	CONST_ENTRY("CHECKED"),
	CONST_ENTRY("DEFAULT"),
	CONST_ENTRY("ENABLED"),
	CONST_ENTRY("INVALID"),
	CONST_ENTRY("VISITED"),
	CONST_ENTRY("DISABLED"),
	CONST_ENTRY("IN-RANGE"),
	CONST_ENTRY("OPTIONAL"),
	CONST_ENTRY("REQUIRED"),
	CONST_ENTRY("READ-ONLY"),
	CONST_ENTRY("SELECTION"),
	CONST_ENTRY("FIRST-LINE"),
	CONST_ENTRY("LAST-CHILD"),
	CONST_ENTRY("ONLY-CHILD"),
	CONST_ENTRY("READ-WRITE"),
	CONST_ENTRY("FULLSCREEN"),
	CONST_ENTRY("-O-PREFOCUS"),
	CONST_ENTRY("FIRST-CHILD"),
	CONST_ENTRY("FIRST-LETTER"),
	CONST_ENTRY("LAST-OF-TYPE"),
	CONST_ENTRY("ONLY-OF-TYPE"),
	CONST_ENTRY("OUT-OF-RANGE"),
	CONST_ENTRY("FIRST-OF-TYPE"),
	CONST_ENTRY("INDETERMINATE"),
	CONST_ENTRY("FULLSCREEN-ANCESTOR")
CONST_END(g_css_pseudo_name)

static const unsigned short CSS_pseudo_tok[] =
{
	PSEUDO_CLASS_CUE,
	PSEUDO_CLASS_LANG,
	PSEUDO_CLASS_LINK,
	PSEUDO_CLASS_PAST,
	PSEUDO_CLASS_ROOT,
	PSEUDO_CLASS_AFTER,
	PSEUDO_CLASS_EMPTY,
	PSEUDO_CLASS_FOCUS,
	PSEUDO_CLASS_HOVER,
	PSEUDO_CLASS_VALID,
	PSEUDO_CLASS_ACTIVE,
	PSEUDO_CLASS_BEFORE,
	PSEUDO_CLASS_FUTURE,
	PSEUDO_CLASS_TARGET,
	PSEUDO_CLASS_CHECKED,
	PSEUDO_CLASS_DEFAULT,
	PSEUDO_CLASS_ENABLED,
	PSEUDO_CLASS_INVALID,
	PSEUDO_CLASS_VISITED,
	PSEUDO_CLASS_DISABLED,
	PSEUDO_CLASS_IN_RANGE,
	PSEUDO_CLASS_OPTIONAL,
	PSEUDO_CLASS_REQUIRED,
	PSEUDO_CLASS_READ_ONLY,
	PSEUDO_CLASS_SELECTION,
	PSEUDO_CLASS_FIRST_LINE,
	PSEUDO_CLASS_LAST_CHILD,
	PSEUDO_CLASS_ONLY_CHILD,
	PSEUDO_CLASS_READ_WRITE,
	PSEUDO_CLASS_FULLSCREEN,
	PSEUDO_CLASS__O_PREFOCUS,
	PSEUDO_CLASS_FIRST_CHILD,
	PSEUDO_CLASS_FIRST_LETTER,
	PSEUDO_CLASS_LAST_OF_TYPE,
	PSEUDO_CLASS_ONLY_OF_TYPE,
	PSEUDO_CLASS_OUT_OF_RANGE,
	PSEUDO_CLASS_FIRST_OF_TYPE,
	PSEUDO_CLASS_INDETERMINATE,
	PSEUDO_CLASS_FULLSCREEN_ANCESTOR
};


unsigned short CSS_GetPseudoClass(const uni_char* str, int len)
{
	if (len <= CSS_PSEUDO_MAX_SIZE)
	{
		short end_idx = CSS_pseudo_idx[len+1];
		for (short i=CSS_pseudo_idx[len]; i<end_idx; i++)
			if (uni_strni_eq(str, g_css_pseudo_name[i], len) != 0)
				return CSS_pseudo_tok[i];
	}
	return PSEUDO_CLASS_UNKNOWN;
}

const char* CSS_GetPseudoClassString(unsigned short type)
{
	for (int i=0; i<PSEUDO_CLASS_COUNT; i++)
		if (CSS_pseudo_tok[i] == type)
			return g_css_pseudo_name[i];
	return NULL;
}

short CSS_GetPseudoPage(const uni_char* str, int len)
{
	if (len == 4 && uni_strni_eq(str, "LEFT", 4))
		return PSEUDO_PAGE_LEFT;
	else if (len == 5 && uni_strni_eq(str, "RIGHT", 5))
		return PSEUDO_PAGE_RIGHT;
	else if (len == 5 && uni_strni_eq(str, "FIRST", 5))
		return PSEUDO_PAGE_FIRST;
	else
		return PSEUDO_PAGE_UNKNOWN;
}

#ifdef CSS_ANIMATIONS
double CSS_GetKeyframePosition(const uni_char* str, int len)
{
	double position = -1;

	if (len == 4 && uni_strni_eq(str, "FROM", 4))
		position = 0.0;
	else if (len == 2 && uni_strni_eq(str, "TO", 2))
		position = 1.0;

	return position;
}
#endif // CSS_ANIMATIONS

const char* CSS_GetPseudoPageString(short pseudo)
{
	if (pseudo == PSEUDO_PAGE_LEFT)
		return "LEFT";
	else if (pseudo == PSEUDO_PAGE_RIGHT)
		return "RIGHT";
	else if (pseudo == PSEUDO_PAGE_FIRST)
		return "FIRST";
	else
		return NULL;
}

void CSS_Properties::SelectProperty(CSS_decl* new_css_decl, unsigned short new_specificity, UINT32 rule_number, BOOL apply_partially)
{
	OP_PROBE4(OP_PROBE_CSS_SELECTPROPERTY);

	short prop = new_css_decl->GetProperty();

	if (apply_partially)
		switch (prop)
		{
		// Allowed properties for mds=1 (Limited style / apply partially)
		case CSS_PROPERTY_background_attachment:
		case CSS_PROPERTY_background_color:
		case CSS_PROPERTY_background_image:
		case CSS_PROPERTY_background_position:
		case CSS_PROPERTY_background_repeat:
		case CSS_PROPERTY_bottom:
		case CSS_PROPERTY_color:
		case CSS_PROPERTY_content:
		case CSS_PROPERTY_counter_increment:
		case CSS_PROPERTY_counter_reset:
		case CSS_PROPERTY_direction:
		case CSS_PROPERTY_display:
		case CSS_PROPERTY_visibility:
		case CSS_PROPERTY_float:
		case CSS_PROPERTY_font_style:
		case CSS_PROPERTY_font_weight:
		case CSS_PROPERTY_left:
		case CSS_PROPERTY_list_style_type:
		case CSS_PROPERTY_quotes:
		case CSS_PROPERTY_right:
		case CSS_PROPERTY_text_align:
		case CSS_PROPERTY_text_decoration:
		case CSS_PROPERTY_text_transform:
		case CSS_PROPERTY_top:
		case CSS_PROPERTY_unicode_bidi:
		case CSS_PROPERTY_vertical_align:
		case CSS_PROPERTY__wap_marquee_style:
		case CSS_PROPERTY__wap_marquee_loop:
		case CSS_PROPERTY__wap_marquee_dir:
		case CSS_PROPERTY__wap_marquee_speed:
			break;
		default:
			return;
		}

	BOOL use_new_decl = TRUE;

	if (css_decl[prop] != NULL)
	{
		CSS_decl* decl = css_decl[prop];

		CSS_decl::CascadingPoint old_point = decl->GetCascadingPoint();
		CSS_decl::CascadingPoint new_point = new_css_decl->GetCascadingPoint();

		if (old_point == new_point)
			use_new_decl = (specificity[prop] < new_specificity ||
							specificity[prop] == new_specificity && rule_num[prop] < rule_number);
		else
			use_new_decl = new_point > old_point;
	}

	if (use_new_decl)
	{
		css_decl[prop] = new_css_decl;
		specificity[prop] = new_specificity;
		rule_num[prop] = rule_number;
	}
}

void CSS_Properties::SelectSelectionProperty(CSS_decl* new_css_decl, unsigned short new_specificity, UINT32 rule_number)
{
	OP_PROBE4(OP_PROBE_CSS_SELECTSELECTIONPROPERTY);

	short prop = new_css_decl->GetProperty();

	PropertyToSelectionProperty(prop);

	BOOL use_new_decl = TRUE;
	if (CSS_decl* decl = css_decl[prop])
	{
		CSS_decl::CascadingPoint old_point = decl->GetCascadingPoint();
		CSS_decl::CascadingPoint new_point = new_css_decl->GetCascadingPoint();

		if (old_point == new_point)
			use_new_decl = (specificity[prop] < new_specificity ||
							specificity[prop] == new_specificity && rule_num[prop] < rule_number);
		else
			use_new_decl = new_point > old_point;
	}

	if (use_new_decl)
	{
		css_decl[prop] = new_css_decl;
		specificity[prop] = new_specificity;
		rule_num[prop] = rule_number;
	}
}

#ifdef STYLE_GETMATCHINGRULES_API
BOOL PropertyIsInherited(short property)
{
	switch (property)
	{
	case CSS_PROPERTY_fill:
	case CSS_PROPERTY_font:
	case CSS_PROPERTY_mask:
	case CSS_PROPERTY_page:
	case CSS_PROPERTY_color:
	case CSS_PROPERTY_cursor:
	case CSS_PROPERTY_filter:
	case CSS_PROPERTY_marker:
	case CSS_PROPERTY_quotes:
	case CSS_PROPERTY_stroke:
	case CSS_PROPERTY_widows:
	case CSS_PROPERTY_kerning:
	case CSS_PROPERTY_orphans:
	case CSS_PROPERTY_clip_path:
	case CSS_PROPERTY_clip_rule:
	case CSS_PROPERTY_direction:
	case CSS_PROPERTY_fill_rule:
	case CSS_PROPERTY_font_size:
	case CSS_PROPERTY_font_color:
	case CSS_PROPERTY_font_style:
	case CSS_PROPERTY_list_style:
	case CSS_PROPERTY_marker_end:
	case CSS_PROPERTY_marker_mid:
	case CSS_PROPERTY_stop_color:
	case CSS_PROPERTY_text_align:
	case CSS_PROPERTY_visibility:
	case CSS_PROPERTY_audio_level:
	case CSS_PROPERTY_empty_cells:
	case CSS_PROPERTY_flood_color:
	case CSS_PROPERTY_font_family:
	case CSS_PROPERTY_font_weight:
	case CSS_PROPERTY_line_height:
	case CSS_PROPERTY_solid_color:
	case CSS_PROPERTY_table_rules:
	case CSS_PROPERTY_text_anchor:
	case CSS_PROPERTY_text_indent:
	case CSS_PROPERTY_text_shadow:
	case CSS_PROPERTY_white_space:
	case CSS_PROPERTY__o_tab_size:
	case CSS_PROPERTY_caption_side:
	case CSS_PROPERTY_fill_opacity:
	case CSS_PROPERTY_font_stretch:
	case CSS_PROPERTY_font_variant:
	case CSS_PROPERTY_marker_start:
	case CSS_PROPERTY_stop_opacity:
	case CSS_PROPERTY_stroke_width:
	case CSS_PROPERTY_word_spacing:
	case CSS_PROPERTY_writing_mode:
	case CSS_PROPERTY_color_profile:
	case CSS_PROPERTY_display_align:
	case CSS_PROPERTY_flood_opacity:
	case CSS_PROPERTY_marker_offset:
	case CSS_PROPERTY_solid_opacity:
	case CSS_PROPERTY_vector_effect:
	case CSS_PROPERTY_viewport_fill:
	case CSS_PROPERTY_baseline_shift:
	case CSS_PROPERTY_border_spacing:
	case CSS_PROPERTY_letter_spacing:
	case CSS_PROPERTY_lighting_color:
	case CSS_PROPERTY_line_increment:
	case CSS_PROPERTY_pointer_events:
	case CSS_PROPERTY_stroke_linecap:
	case CSS_PROPERTY_stroke_opacity:
	case CSS_PROPERTY_text_transform:
	case CSS_PROPERTY_text_rendering:
	case CSS_PROPERTY_writing_system:
	case CSS_PROPERTY_border_collapse:
	case CSS_PROPERTY_color_rendering:
	case CSS_PROPERTY_image_rendering:
	case CSS_PROPERTY_list_style_type:
	case CSS_PROPERTY_selection_color:
	case CSS_PROPERTY_shape_rendering:
	case CSS_PROPERTY_stroke_linejoin:
	case CSS_PROPERTY_font_size_adjust:
	case CSS_PROPERTY_list_style_image:
	case CSS_PROPERTY_stroke_dasharray:
	case CSS_PROPERTY_dominant_baseline:
	case CSS_PROPERTY_enable_background:
	case CSS_PROPERTY_page_break_inside:
	case CSS_PROPERTY_stroke_miterlimit:
	case CSS_PROPERTY_stroke_dashoffset:
	case CSS_PROPERTY_alignment_baseline:
	case CSS_PROPERTY_color_interpolation:
	case CSS_PROPERTY_list_style_position:
	case CSS_PROPERTY_viewport_fill_opacity:
	case CSS_PROPERTY_glyph_orientation_vertical:
	case CSS_PROPERTY_selection_background_color:
	case CSS_PROPERTY_color_interpolation_filters:
	case CSS_PROPERTY_glyph_orientation_horizontal:
		return TRUE;

	default:
		return FALSE;
	}
}

void CSS_Properties::AddDeclsFrom(CSS_Properties& props, BOOL include_inherit_no)
{
	for (short i = 0; i < CSS_PROPERTIES_LENGTH; i++)
	{
		if (props.css_decl[i] != NULL && css_decl[i] == NULL && (include_inherit_no || PropertyIsInherited(i)))
			css_decl[i] = props.css_decl[i];
	}
}
#endif // STYLE_GETMATCHINGRULES_API


CSS::CSS(HTML_Element* he, const URL& base_url, BOOL user_defined)
	: m_user_def(user_defined), m_is_local(FALSE), m_has_syntactically_valid_css_header(TRUE),
	  m_default_ns_idx(NS_IDX_NOT_FOUND), m_ns_elm_list(NULL), m_src_html_element(he),
	  m_media_object(NULL), m_base_url(base_url), m_succ_adj(0),
#ifdef DOM_FULLSCREEN_MODE
	  m_complex_fullscreen(FALSE),
#endif // DOM_FULLSCREEN_MODE
	  m_xml(FALSE),
	  m_skip(FALSE), m_modified(FALSE),
	  m_next_rule_number(0)
{
	const uni_char* rel = he->GetLinkRel();
	const uni_char* ttl = he->GetStringAttr(Markup::HA_TITLE);

	BOOL is_alternate = FALSE;

	if (he->Type() == Markup::HTE_PROCINST)
		is_alternate = he->HasSpecialAttr(ATTR_ALTERNATE, SpecialNs::NS_LOGDOC);
	else
		if (rel && he->IsMatchingType(Markup::HTE_LINK, NS_HTML))
		{
			OP_ASSERT(LinkElement::MatchKind(rel) & LINK_TYPE_STYLESHEET);
			is_alternate = (LinkElement::MatchKind(rel) & LINK_TYPE_ALTERNATE) != 0;
		}

	if (is_alternate)
		m_sheet_type = STYLE_ALTERNATE;
	else if (ttl && *ttl)
		m_sheet_type = STYLE_PREFERRED;
	else
		m_sheet_type = STYLE_PERSISTENT;

	m_enabled = m_sheet_type == STYLE_PERSISTENT || m_sheet_type == STYLE_PREFERRED;
}

CSS::~CSS()
{
	if (m_default_ns_idx != NS_IDX_NOT_FOUND)
		g_ns_manager->GetElementAt(m_default_ns_idx)->DecRefCount();

	while (m_ns_elm_list)
	{
		NS_ListElm* tmp_elm = m_ns_elm_list->Suc();
		OP_DELETE(m_ns_elm_list);
		m_ns_elm_list = tmp_elm;
	}

	m_rules.Clear();
	m_query_widths.Clear();
	m_query_heights.Clear();
	m_query_ratios.Clear();
	m_query_device_widths.Clear();
	m_query_device_heights.Clear();
	m_query_device_ratios.Clear();
	m_id_rules.DeleteAll();
	m_class_rules.DeleteAll();
	m_type_rules.DeleteAll();
	m_webfonts.DeleteAll();
}

/* virtual */ void
CSS::Added(CSSCollection* collection, FramesDocument* doc)
{
	OP_ASSERT(collection);

	if (m_type_rules.GetCount() > 0 ||
		m_id_rules.GetCount() > 0 ||
		m_class_rules.GetCount() > 0)
		collection->MarkAffectedElementsPropsDirty(this);

	unsigned int changes = CSSCollection::CHANGED_NONE;

	if (m_page_rules.First())
		changes |= CSSCollection::CHANGED_PAGEPROPS;
	if (m_webfonts.GetCount() > 0)
		changes |= CSSCollection::CHANGED_WEBFONT;
#ifdef CSS_VIEWPORT_SUPPORT
	if (m_viewport_rules.First())
		changes |= CSSCollection::CHANGED_VIEWPORT;
#endif // CSS_VIEWPORT_SUPPORT
#ifdef CSS_ANIMATIONS
	if (m_keyframes_rules.First())
		changes |= CSSCollection::CHANGED_KEYFRAMES;
#endif

	if (changes != CSSCollection::CHANGED_NONE)
		collection->StyleChanged(changes);
}

class WebFontRemovalListener : public OpHashTableForEachListener
{
public:
	WebFontRemovalListener(FramesDocument* doc) : m_doc(doc) {}

	virtual void HandleKeyData(const void* key, void* data)
	{
		Head* list = static_cast<Head*>(data);
		CSS_WebFont* web_font = static_cast<CSS_WebFont*>(list->First());
		while (web_font)
		{
			web_font->RemovedWebFont(m_doc);
			web_font = static_cast<CSS_WebFont*>(web_font->Suc());
		}
	}

private:
	FramesDocument* m_doc;
};

/* virtual */ void
CSS::Removed(CSSCollection* collection, FramesDocument* doc)
{
	OP_ASSERT(collection);

	unsigned int changes = CSSCollection::CHANGED_NONE;

	if (m_type_rules.GetCount() > 0 ||
		m_id_rules.GetCount() > 0 ||
		m_class_rules.GetCount() > 0)
		changes |= CSSCollection::CHANGED_PROPS;
	if (m_page_rules.First())
		changes |= CSSCollection::CHANGED_PAGEPROPS;
	if (m_webfonts.GetCount() > 0)
	{
		WebFontRemovalListener listener(doc);
		m_webfonts.ForEach(&listener);
		changes |= CSSCollection::CHANGED_WEBFONT;
	}
#ifdef CSS_VIEWPORT_SUPPORT
	if (m_viewport_rules.First())
		changes |= CSSCollection::CHANGED_VIEWPORT;
#endif // CSS_VIEWPORT_SUPPORT

	if (changes != CSSCollection::CHANGED_NONE)
		collection->StyleChanged(changes);
}

static inline UINT32 MakeRuleNumber(unsigned int css_num, unsigned int rule_num) { return (((0xfff - (css_num & 0xfff)) << 20) | (rule_num & 0xfffff)); }

#ifdef PAGED_MEDIA_SUPPORT

void CSS::GetPageProperties(FramesDocument* doc, CSS_Properties* css_properties, unsigned int stylesheet_number, BOOL first_page, BOOL left_page, CSS_MediaType media_type)
{
	CSS_RuleElm* first_elm = m_page_rules.First();
	if (first_elm)
	{
		CSS_RuleElm* lists[2] = { first_elm, NULL };

		RuleElmIterator iter(doc, lists, media_type);
		CSS_RuleElm* rule_elm;
		while ((rule_elm = iter.Next()) != NULL)
		{
			CSS_PageRule* current = static_cast<CSS_PageRule*>(rule_elm->GetRule());

			unsigned short specificity = current->GetSpecificity();
			short pseudo_class = current->GetPagePseudo();

			CSS_property_list* css_prop_list = current->GetPropertyList();
			if (css_prop_list)
			{
				CSS_decl* css_decl = css_prop_list->GetLastDecl();
				while (css_decl)
				{
					if ((!pseudo_class ||
						 (pseudo_class & PSEUDO_PAGE_FIRST && first_page) ||
						 (pseudo_class & PSEUDO_PAGE_LEFT && left_page) ||
						 (pseudo_class & PSEUDO_PAGE_RIGHT && !left_page)))
						css_properties->SelectProperty(css_decl, specificity, MakeRuleNumber(stylesheet_number, current->GetRuleNumber()), FALSE);

					css_decl = css_decl->Pred();
				}
			}
		}
	}
}

#endif // PAGED_MEDIA_SUPPORT

#ifdef CSS_VIEWPORT_SUPPORT

void CSS::GetViewportProperties(FramesDocument* doc, CSS_Viewport* viewport, CSS_MediaType media_type)
{
	CSS_RuleElm* first_elm = m_viewport_rules.First();
	if (first_elm)
	{
		CSS_RuleElm* lists[2] = { first_elm, NULL };

		RuleElmIterator iter(doc, lists, media_type);
		CSS_RuleElm* rule_elm;
		while ((rule_elm = iter.Next()) != NULL)
		{
			CSS_ViewportRule* current = static_cast<CSS_ViewportRule*>(rule_elm->GetRule());
			current->AddViewportProperties(viewport);
		}
	}
}

#endif // CSS_VIEWPORT_SUPPORT

#ifdef CSS_ANIMATIONS

CSS_KeyframesRule* CSS::GetKeyframesRule(FramesDocument* doc, const uni_char* name, CSS_MediaType media_type)
{
	if (CSS_RuleElm* first_elm = m_keyframes_rules.First())
	{
		CSS_RuleElm* lists[2] = { first_elm, NULL };
		RuleElmIterator iter(doc, lists, media_type);

		CSS_RuleElm* rule_elm;
		while ((rule_elm = iter.Next()) != NULL)
		{
			CSS_KeyframesRule* keyframes_rule = static_cast<CSS_KeyframesRule*>(rule_elm->GetRule());
			if (uni_strcmp(keyframes_rule->GetName(), name) == 0)
				return keyframes_rule;
		}
	}

	return NULL;
}

#endif

static BOOL LegalProperty(PseudoElementType pseudo_elm, short property)
{
	if (pseudo_elm != ELM_PSEUDO_BEFORE && pseudo_elm != ELM_PSEUDO_AFTER)
	{
		switch (property)
		{
		case CSS_PROPERTY_color:
		case CSS_PROPERTY_background_color:
			return TRUE;
		case CSS_PROPERTY_background_image:
		case CSS_PROPERTY_background_repeat:
		case CSS_PROPERTY_background_position:
		case CSS_PROPERTY_background_attachment:
		case CSS_PROPERTY_font_size:
		case CSS_PROPERTY_font_style:
		case CSS_PROPERTY_font_variant:
		case CSS_PROPERTY_font_weight:
		case CSS_PROPERTY_font_family:
		case CSS_PROPERTY_word_spacing:
		case CSS_PROPERTY_letter_spacing:
		case CSS_PROPERTY_text_decoration:
		case CSS_PROPERTY_vertical_align:
		case CSS_PROPERTY_text_transform:
		case CSS_PROPERTY_line_height:
		case CSS_PROPERTY_clear:
		case CSS_PROPERTY_text_shadow:
			return (pseudo_elm != ELM_PSEUDO_SELECTION);
		case CSS_PROPERTY_float:
		case CSS_PROPERTY_margin_top:
		case CSS_PROPERTY_margin_left:
		case CSS_PROPERTY_margin_right:
		case CSS_PROPERTY_margin_bottom:
		case CSS_PROPERTY_padding_top:
		case CSS_PROPERTY_padding_left:
		case CSS_PROPERTY_padding_right:
		case CSS_PROPERTY_padding_bottom:
		case CSS_PROPERTY_border_top_width:
		case CSS_PROPERTY_border_left_width:
		case CSS_PROPERTY_border_right_width:
		case CSS_PROPERTY_border_bottom_width:
		case CSS_PROPERTY_border_top_style:
		case CSS_PROPERTY_border_left_style:
		case CSS_PROPERTY_border_right_style:
		case CSS_PROPERTY_border_bottom_style:
		case CSS_PROPERTY_border_top_color:
		case CSS_PROPERTY_border_left_color:
		case CSS_PROPERTY_border_right_color:
		case CSS_PROPERTY_border_bottom_color:
			return (pseudo_elm == ELM_PSEUDO_FIRST_LETTER);
		default:
			return FALSE;
		}
	}

	return TRUE;
}

#ifdef MEDIA_HTML_SUPPORT

static BOOL LegalCueProperty(short property, Markup::Type type, BOOL has_past_or_future = FALSE)
{
	switch (property)
	{
	case CSS_PROPERTY_color:
	case CSS_PROPERTY_text_decoration:
	case CSS_PROPERTY_text_shadow:
	case CSS_PROPERTY_outline_color:
	case CSS_PROPERTY_outline_style:
	case CSS_PROPERTY_outline_width:
	case CSS_PROPERTY_background_color:
	case CSS_PROPERTY_background_image:
	case CSS_PROPERTY_background_repeat:
	case CSS_PROPERTY_background_attachment:
	case CSS_PROPERTY_background_position:
	case CSS_PROPERTY_background_size:
	case CSS_PROPERTY_background_origin:
	case CSS_PROPERTY_background_clip:
		return TRUE;

	case CSS_PROPERTY_font_style:
	case CSS_PROPERTY_font_variant:
	case CSS_PROPERTY_font_weight:
	case CSS_PROPERTY_font_size:
	case CSS_PROPERTY_line_height:
	case CSS_PROPERTY_font_family:
	case CSS_PROPERTY_white_space:
		return !has_past_or_future;

# ifdef CSS_TRANSITIONS
	case CSS_PROPERTY_transition_delay:
	case CSS_PROPERTY_transition_duration:
	case CSS_PROPERTY_transition_property:
	case CSS_PROPERTY_transition_timing_function:
#  ifdef CSS_ANIMATIONS
	case CSS_PROPERTY_animation_name:
	case CSS_PROPERTY_animation_duration:
	case CSS_PROPERTY_animation_timing_function:
	case CSS_PROPERTY_animation_iteration_count:
	case CSS_PROPERTY_animation_direction:
	case CSS_PROPERTY_animation_play_state:
	case CSS_PROPERTY_animation_delay:
	case CSS_PROPERTY_animation_fill_mode:
#  endif // CSS_ANIMATIONS
		return type != Markup::CUEE_ROOT;
# endif // CSS_TRANSITIONS
	}

	return FALSE;
}

#endif // MEDIA_HTML_SUPPORT

BOOL CSS::GetProperties(CSS_MatchContext* match_context,
						CSS_Properties* css_properties,
						unsigned int stylesheet_number) const
{
	OP_PROBE4(OP_PROBE_CSS_GETPROPERTIES);

	BOOL found = FALSE;

	UINT32 idx;

	CSS_MatchContextElm* context_elm = match_context->GetCurrentLeafElm();
	OP_ASSERT(context_elm);

	// determine index
	int elm_ns_idx  = context_elm->GetNsIdx();
	NS_Type ns_type = g_ns_manager->GetNsTypeAt(elm_ns_idx);

	Markup::Type type = context_elm->Type();

	if (!Markup::IsRealElement(type))
		return FALSE;
	else if (match_context->IsXml())
	{
		if (ns_type == NS_HTML)
		{
			// Special case since textarea is spelled textArea in SVG
			// for some reason, and that will cause GetTypeFromNameAmbiguous()
			// return HTE_UNKNOWN during parsing of the stylesheet.
			if (type == Markup::HTE_TEXTAREA)
				idx = static_cast<UINT32>(Markup::HTE_UNKNOWN);
			else
				idx = static_cast<UINT32>(type);
		}
		else if (ns_type != NS_USER && *(g_ns_manager->GetPrefixAt(elm_ns_idx)) == '\0')
		{
			if (g_html5_name_mapper->HasAmbiguousName(type))
				idx = static_cast<UINT32>(Markup::HTE_UNKNOWN);
			else
				idx = static_cast<UINT32>(type);
		}
		else
			idx = static_cast<UINT32>(type);
	}
	else
		idx = static_cast<UINT32>(type);

	const uni_char* id_attr = context_elm->GetIdAttribute();
	BOOL has_id = id_attr && *id_attr;

#define MAX_LIST_COUNT 31

	// Id, class, type, and universal lists.
	CSS_RuleElm* rule_list_array[MAX_LIST_COUNT+1];
	CSS_RuleElm** rule_lists = rule_list_array;
	unsigned int list_count = 0;

	CSS_RuleElmList* list;

	if (has_id)
	{
		if (OpStatus::IsSuccess(m_id_rules.GetData(id_attr, &list)) && list->First())
			rule_lists[list_count++] = list->First();
	}

	const ClassAttribute* class_attr = context_elm->GetClassAttribute();

	if (class_attr)
	{
		unsigned int i = 0;

		const ReferencedHTMLClass* class_ref = NULL;

		while (list_count < MAX_LIST_COUNT-2)
		{
			class_ref = class_attr->GetClassRef(i);

			if (class_ref == NULL)
				break;

			if (OpStatus::IsSuccess(m_class_rules.GetData(class_ref->GetString(), &list)) && list->First())
				rule_lists[list_count++] = list->First();

			i++;
		}

		if (class_ref != NULL)
		{
			// This should be a rare case. We have more than 29 classes in the elements class attribute...
			rule_lists = MakeDynamicRuleElmList(rule_lists, i, class_attr, list_count);
			if (!rule_lists)
				rule_lists = rule_list_array;
		}
	}

	if (OpStatus::IsSuccess(m_type_rules.GetData(idx, &list)) && list->First())
		rule_lists[list_count++] = list->First();

	if (OpStatus::IsSuccess(m_type_rules.GetData(Markup::HTE_ANY, &list)) && list->First())
		rule_lists[list_count++] = list->First();

	rule_lists[list_count] = NULL;

	if (list_count > 0)
	{
		CSS_StyleRule* current = NULL;
		BOOL prefetch = match_context->PrefetchMode();

		RuleElmIterator iter(match_context->Document(), rule_lists, match_context->MediaType());
		CSS_RuleElm* rule_elm;

		while ((rule_elm = iter.Next()) != NULL)
		{
			current = static_cast<CSS_StyleRule*>(rule_elm->GetRule());

#ifdef SCOPE_PROFILER
			OpPropertyProbe probe;

			if (match_context->Document()->GetTimeline())
			{
				OpProbeTimeline* t = match_context->Document()->GetTimeline();
				const OpProbeEventType type = PROBE_EVENT_CSS_SELECTOR_MATCHING;
				CSS_Selector* css_sel = rule_elm->GetSelector();

				OpPropertyProbe::Activator<1> act(probe);

				OP_STATUS status = act.Activate(t, type, css_sel);

				if (status == OpActivationStatus::CREATED)
				{
					// Add the selector text as an attribute.
					TempBuffer buf;
					status = css_sel->GetSelectorText(this, &buf);
					if (OpStatus::IsSuccess(status))
						status = act.AddString("selector", buf.GetStorage());
				}

				if (OpStatus::IsError(status))
					return found;
			}
#endif // SCOPE_PROFILER

			// No need to match empty rulesets, but we need to emit the rule if scope
			// is listening (!listener), and for MarkAffectedElementsPropsDirty (css_properties).
			// The latter is because MarkAffectedElementsPropsDirty is called to trigger reload
			// of properties for rules that might have had their last declaration removed.
			if (!current->GetPropertyList()->GetFirstDecl()
				&& css_properties
#ifdef STYLE_GETMATCHINGRULES_API
				&& !match_context->Listener()
#endif // STYLE_GETMATCHINGRULES_API
				)
				continue;

			CSS_Selector* css_sel = rule_elm->GetSelector();
			OP_ASSERT(css_sel);

			// Skip matching selectors for rulesets that have no property
			// declarations that could contain url values.
			if (prefetch && !css_sel->MatchPrefetch())
				continue;

			CSS_Selector::MatchResult match(CSS_Selector::NO_MATCH);

			if ((!css_sel->HasClassInTarget() || context_elm->GetClassAttribute()) &&
				(!css_sel->HasIdInTarget() || context_elm->GetIdAttribute()))
			{
				match = css_sel->Match(match_context, context_elm);
			}

			if (match != CSS_Selector::NO_MATCH)
			{
#ifdef STYLE_GETMATCHINGRULES_API
				if (match_context->Listener())
				{
					CSS_DOMRule* dom_rule = current->GetDOMRule();
					if (!dom_rule)
					{
						DOM_Environment* dom_env = match_context->Document()->GetDOMEnvironment();
						OP_ASSERT(dom_env);
						dom_rule = OP_NEW(CSS_DOMRule, (dom_env, current, m_src_html_element));
						current->SetDOMRule(dom_rule);
					}
					OP_STATUS stat = OpStatus::OK;
					if (dom_rule)
					{
						CSS_MatchElement elm(context_elm->HtmlElement(), match_context->PseudoElm());

# ifdef STYLE_MATCH_LOCAL_API
						if (m_is_local)
							stat = match_context->Listener()->LocalRuleMatched(elm, dom_rule, css_sel->GetSpecificity());
						else
# endif // STYLE_MATCH_LOCAL_API
							stat = match_context->Listener()->RuleMatched(elm, dom_rule, css_sel->GetSpecificity());
					}
					else
						stat = OpStatus::ERR_NO_MEMORY;

					if (OpStatus::IsMemoryError(stat))
					{
						match_context->Document()->GetHLDocProfile()->SetIsOutOfMemory(TRUE);
						break;
					}
				}
#endif // STYLE_GETMATCHINGRULES_API
				if (css_properties)
				{
					CSS_property_list* css_prop_list = current->GetPropertyList();
					if (css_prop_list)
					{
						CSS_decl* css_decl = css_prop_list->GetLastDecl();
						UINT32 rule_number = MakeRuleNumber(stylesheet_number, current->GetRuleNumber());
						unsigned short specificity = css_sel->GetSpecificity();
						BOOL apply_partially = match_context->ApplyPartially();
						if (match == CSS_Selector::MATCH)
						{
#ifdef MEDIA_HTML_SUPPORT
							if (context_elm->IsCueElement())
							{
								Markup::Type elm_type = context_elm->Type();
								BOOL sel_has_past_or_future = css_sel->HasPastOrFuturePseudo();

								while (css_decl)
								{
									if (LegalCueProperty(css_decl->GetProperty(), elm_type, sel_has_past_or_future))
										css_properties->SelectProperty(css_decl, specificity, rule_number, apply_partially);

									css_decl = css_decl->Pred();
								}
							}
							else
#endif // MEDIA_HTML_SUPPORT
							if (match_context->PseudoElm() < ELM_PSEUDO_FIRST_LETTER)
							{
								while (css_decl)
								{
									css_properties->SelectProperty(css_decl, specificity, rule_number, apply_partially);

									css_decl = css_decl->Pred();
								}
							}
							else
							{
								while (css_decl)
								{
									if (LegalProperty(match_context->PseudoElm(), css_decl->GetProperty()))
										css_properties->SelectProperty(css_decl, specificity, rule_number, apply_partially);

									css_decl = css_decl->Pred();
								}
							}
						}
						else if (match == CSS_Selector::MATCH_SELECTION)
						{
							while (css_decl)
							{
								if (LegalProperty(ELM_PSEUDO_SELECTION, css_decl->GetProperty()))
									css_properties->SelectSelectionProperty(css_decl, specificity, rule_number);

								css_decl = css_decl->Pred();
							}
						}
					}

					found = TRUE;
				}
				else
				{
					found = TRUE;
					break;
				}
			}
		}
	}

	if (rule_lists != rule_list_array)
		OP_DELETEA(rule_lists);

	return found;
}

CSS::CSS_RuleElm** CSS::MakeDynamicRuleElmList(CSS_RuleElm** elm_list, unsigned int i, const ClassAttribute* class_attr, unsigned int& list_count) const
{
	CSS_RuleElmList* list;
	OpVector<CSS_RuleElm> vec;
	OP_STATUS stat = OpStatus::OK;

	while (stat == OpStatus::OK)
	{
		const ReferencedHTMLClass* class_ref = class_attr->GetClassRef(i);

		if (class_ref == NULL)
			break;

		if (OpStatus::IsSuccess(m_class_rules.GetData(class_ref->GetString(), &list)) && list->First())
			stat = vec.Add(list->First());

		i++;
	}

	if (stat == OpStatus::OK && vec.GetCount() > 0)
	{
		CSS_RuleElm** new_elm_list = OP_NEWA(CSS_RuleElm*, (list_count + 3 + vec.GetCount()));
		if (new_elm_list)
		{
			for (unsigned int j=0; j<list_count; j++)
				new_elm_list[j] = elm_list[j];
			for (UINT32 k=0; k<vec.GetCount(); k++)
				new_elm_list[list_count++] = vec.Get(k);
			elm_list = new_elm_list;
		}
	}

	return elm_list;
}

CSS_PARSE_STATUS CSS::Load(HLDocProfile* hld_profile, DataSrc* src_head, unsigned start_line_number, unsigned start_line_character)
{
	RETURN_IF_ERROR(MediaAttrChanged());

	CSS_Buffer* css_buf = OP_NEW(CSS_Buffer, ());

	if (css_buf && css_buf->AllocBufferArrays(src_head->GetDataSrcElmCount()))
	{
		DataSrcElm* src_elm = src_head->First();

		while (src_elm)
		{
			css_buf->AddBuffer(src_elm->GetSrc(), src_elm->GetSrcLen());
			src_elm = src_elm->Suc();
		}

		CSS_Parser* parser = OP_NEW(CSS_Parser, (this, css_buf, GetBaseURL(), hld_profile, start_line_number));

		CSS_PARSE_STATUS stat = parser ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
		if (parser)
			TRAP(stat, parser->ParseL());

		OP_DELETE(parser);
		OP_DELETE(css_buf);

		return stat;
	}
	else
	{
		OP_DELETE(css_buf);
		return OpStatus::ERR_NO_MEMORY;
	}
}

const uni_char*
CSS::GetTitle() const
{
	return m_src_html_element->GetStringAttr(Markup::HA_TITLE);
}

const URL*
CSS::GetHRef(LogicalDocument* logdoc/*=NULL*/) const
{
	return m_src_html_element->GetUrlAttr(Markup::HA_HREF, NS_IDX_HTML, logdoc);
}

void
CSS::AddNameSpaceL(HLDocProfile* hld_prof, const uni_char* name, const uni_char* uri)
{
	int new_ns_idx = g_ns_manager->GetNsIdx(uri, uni_strlen(uri), name, name ? uni_strlen(name) : 0 );

	CSS_NamespaceRule* new_rule = OP_NEW_L(CSS_NamespaceRule, (new_ns_idx));
	LEAVE_IF_ERROR(AddRule(hld_prof, new_rule));

	if (name)
	{
		NS_ListElm* new_elm = OP_NEW_L(NS_ListElm, (new_ns_idx, 0));
		new_elm->Precede(m_ns_elm_list);
		m_ns_elm_list = new_elm;
	}
	else
	{
		if (m_default_ns_idx != NS_IDX_NOT_FOUND)
			g_ns_manager->GetElementAt(m_default_ns_idx)->DecRefCount();
		m_default_ns_idx = new_ns_idx;
		if (m_default_ns_idx != NS_IDX_NOT_FOUND)
			g_ns_manager->GetElementAt(m_default_ns_idx)->IncRefCount();
	}
}

int
CSS::GetNameSpaceIdx(const uni_char* prefix)
{
	NS_ListElm* elm = m_ns_elm_list;
	while (elm)
	{
		int ns_idx = elm->GetNsIdx();
		const uni_char* elm_prefix = g_ns_manager->GetElementAt(ns_idx)->GetPrefix();
		if (elm_prefix && uni_strcmp(elm_prefix, prefix) == 0)
			return ns_idx;
		elm = elm->Suc();
	}
	return NS_IDX_NOT_FOUND;
}

CSS_property_list* CSS::LoadHtmlStyleAttr(const uni_char* buf_start, int buf_len, const URL& base_url, HLDocProfile* hld_profile, CSS_PARSE_STATUS& stat)
{
	CSS_Buffer css_buf;
	if (css_buf.AllocBufferArrays(1))
	{
		css_buf.AddBuffer(buf_start, buf_len);

		CSS_Parser parser(NULL, &css_buf, base_url, hld_profile, 1);
		parser.SetNextToken(hld_profile->IsInStrictMode() ? CSS_TOK_STYLE_ATTR_STRICT : CSS_TOK_STYLE_ATTR);
		TRAP(stat, parser.ParseL());
		if (OpStatus::IsMemoryError(stat))
			return NULL;
		CSS_property_list* props = parser.GetCurrentPropList();
		if (props)
		{
			props->Ref();
# ifdef CSS_TRANSITIONS
			unsigned flags = props->PostProcess(TRUE, TRUE);
			if (hld_profile)
			{
				if ((flags & CSS_property_list::HAS_TRANSITIONS))
					hld_profile->GetCSSCollection()->SetHasTransitions();
#  ifdef CSS_ANIMATIONS
				if ((flags & CSS_property_list::HAS_ANIMATIONS))
					hld_profile->GetCSSCollection()->SetHasAnimations();
#  endif // CSS_ANIMATIONS
			}
# else // CSS_TRANSITIONS
			props->PostProcess(TRUE, FALSE);
# endif // CSS_TRANSITIONS
		}
		return props;
	}
	else
	{
		stat = OpStatus::ERR_NO_MEMORY;
		return NULL;
	}
}

CSS_property_list* CSS::LoadDOMStyleValue(HLDocProfile* hld_profile, CSS_DOMRule* rule, const URL& base_url, int prop, const uni_char* buf_start, int buf_len, CSS_PARSE_STATUS& stat)
{
	CSS_Buffer css_buf;
	if (css_buf.AllocBufferArrays(1))
	{
		css_buf.AddBuffer(buf_start, buf_len);
		CSS_Parser parser(NULL, &css_buf, base_url, hld_profile, 1);
        parser.SetDOMProperty(prop);

		int start_tok = CSS_TOK_DOM_STYLE_PROPERTY;
		if (rule)
		{
			switch (rule->GetType())
			{
			case CSS_DOMRule::PAGE:
				start_tok = CSS_TOK_DOM_PAGE_PROPERTY;
				break;
			case CSS_DOMRule::FONT_FACE:
				start_tok = CSS_TOK_DOM_FONT_DESCRIPTOR;
				break;
			case CSS_DOMRule::VIEWPORT:
				start_tok = CSS_TOK_DOM_VIEWPORT_PROPERTY;
				break;
			case CSS_DOMRule::KEYFRAME:
				start_tok = CSS_TOK_DOM_KEYFRAME_PROPERTY;
				break;
			default:
				OP_ASSERT(!"Missing handling for style object of this rule type.");
			case CSS_DOMRule::STYLE:
				break;
			}
		}
		parser.SetNextToken(start_tok);

		TRAP(stat, parser.ParseL());
		if (OpStatus::IsMemoryError(stat))
			return NULL;
		CSS_property_list* props = parser.GetCurrentPropList();
		if (props)
		{
#ifdef CSS_TRANSITIONS
			unsigned flags = props->PostProcess(TRUE, TRUE);
			if (hld_profile)
			{
				if ((flags & CSS_property_list::HAS_TRANSITIONS))
					hld_profile->GetCSSCollection()->SetHasTransitions();
# ifdef CSS_ANIMATIONS
				if ((flags & CSS_property_list::HAS_ANIMATIONS))
					hld_profile->GetCSSCollection()->SetHasAnimations();
# endif // CSS_ANIMATIONS
			}
#else // CSS_TRANSITIONS
			props->PostProcess(TRUE, FALSE);
#endif // CSS_TRANSITIONS
			props->Ref();
		}
		return props;
	}
	else
	{
		stat = OpStatus::ERR_NO_MEMORY;
		return NULL;
	}
}

BOOL CSS::IsColorProperty(short property)
{
	switch (property)
	{
	case CSS_PROPERTY_fill:
	case CSS_PROPERTY_stroke:
	case CSS_PROPERTY_stop_color:
	case CSS_PROPERTY_flood_color:
	case CSS_PROPERTY_lighting_color:
	case CSS_PROPERTY_viewport_fill:
	case CSS_PROPERTY_solid_color:
	case CSS_PROPERTY_color:
	case CSS_PROPERTY_background_color:
	case CSS_PROPERTY_border_bottom_color:
	case CSS_PROPERTY_border_left_color:
	case CSS_PROPERTY_border_right_color:
	case CSS_PROPERTY_border_top_color:
	case CSS_PROPERTY_outline_color:
	case CSS_PROPERTY_selection_color:
	case CSS_PROPERTY_selection_background_color:
	case CSS_PROPERTY_column_rule_color:
		return TRUE;
	default:
		return FALSE;
	}
}

const uni_char* CSS::GetDimKeyword(float number)
{
    int n = (int)op_floor(number);
    switch (n)
    {
    case 1:
        return CSS_GetKeywordString(CSS_VALUE_thin);
    case 3:
        return CSS_GetKeywordString(CSS_VALUE_medium);
    case 6:
        return CSS_GetKeywordString(CSS_VALUE_thick);
    }

    return NULL;
}

void CSS::FormatDeclarationL(TempBuffer* tmp_buf, const CSS_decl* decl, BOOL space_before, CSS_SerializationFormat format_mode)
{
	switch (decl->GetProperty())
	{
	case CSS_PROPERTY_clip:
		if (decl->GetDeclType() != CSS_DECL_TYPE)
		{
			tmp_buf->AppendL(UNI_L("rect("));
			for (int j = 0; j < 4; j++)
			{
				if (j > 0)
					tmp_buf->AppendL(',');
				if (decl->GetValueType(j) == CSS_VALUE_auto)
					CSS::FormatCssValueL((void*)(int)CSS_VALUE_auto, CSS_VALUE_TYPE_keyword, tmp_buf, space_before || j > 0);
				else
					CSS::FormatCssNumberL(decl->GetNumberValue(j), decl->GetValueType(j), tmp_buf, space_before || j > 0);
			}
			tmp_buf->AppendL(UNI_L(")"));

				return;
		}
		break;

	case CSS_PROPERTY_page:
		if (decl->GetDeclType() == CSS_DECL_STRING)
		{
				CSS::FormatCssValueL((void*)decl->StringValue(), CSS_VALUE_TYPE_unquoted_string, tmp_buf, space_before);
				return;
		}
		break;

	case CSS_PROPERTY_font_weight:
		if (decl->GetDeclType() == CSS_DECL_NUMBER)
		{
			CSS::FormatCssNumberL(decl->GetNumberValue(0) * 100, decl->GetValueType(0), tmp_buf, space_before);
			return;
		}
		else if (decl->GetDeclType() == CSS_DECL_TYPE && CSS_is_font_system_val(decl->TypeValue()))
			return;
		break;

	case CSS_PROPERTY_text_decoration:
		if (decl->TypeValue() != CSS_VALUE_none && decl->TypeValue() != CSS_VALUE_inherit)
		{
			short decoration = decl->TypeValue();
			if (decoration & CSS_TEXT_DECORATION_underline)
			{
				CSS::FormatCssValueL((void*)(int)CSS_VALUE_underline, CSS_VALUE_TYPE_keyword, tmp_buf, space_before);
				space_before = TRUE;
			}
			if (decoration & CSS_TEXT_DECORATION_overline)
			{
				CSS::FormatCssValueL((void*)(int)CSS_VALUE_overline, CSS_VALUE_TYPE_keyword, tmp_buf, space_before);
				space_before = TRUE;
			}
			if (decoration & CSS_TEXT_DECORATION_line_through)
			{
				CSS::FormatCssValueL((void*)(int)CSS_VALUE_line_through, CSS_VALUE_TYPE_keyword, tmp_buf, space_before);
				space_before = TRUE;
			}
			if (decoration & CSS_TEXT_DECORATION_blink)
			{
				CSS::FormatCssValueL((void*)(int)CSS_VALUE_blink, CSS_VALUE_TYPE_keyword, tmp_buf, space_before);
				space_before = TRUE;
			}
			return;
		}
		break;

	case CSS_PROPERTY_font_style:
	case CSS_PROPERTY_font_variant:
	case CSS_PROPERTY_font_size:
	case CSS_PROPERTY_font_family:
	case CSS_PROPERTY_line_height:
		if (decl->GetDeclType() == CSS_DECL_TYPE && CSS_is_font_system_val(decl->TypeValue()))
			return;
		break;
#ifdef CSS_ANIMATIONS
	case CSS_PROPERTY_animation_timing_function:
#endif
#ifdef CSS_TRANSITIONS
	case CSS_PROPERTY_transition_timing_function:
		{
			if (space_before)
				tmp_buf->AppendL(' ');

			if (decl->GetDeclType() == CSS_DECL_TYPE)
			{
				CSS::FormatCssValueL((void*)(INTPTR)decl->TypeValue(), CSS_VALUE_TYPE_keyword, tmp_buf, space_before);
				return;
			}

			OP_ASSERT(decl->GetDeclType() == CSS_DECL_GEN_ARRAY);
			const CSS_generic_value* gen_arr = decl->GenArrayValue();
			short len = decl->ArrayValueLen();
			OP_ASSERT(len > 0 && len%4 == 0);

			for (short i=0;;)
			{
				FormatCssTimingValueL(&(gen_arr[i]), tmp_buf, space_before);
				i+=4;

				if (i >= len)
					break;

				tmp_buf->Append(", ");
			}
			return;
		}
		break;
#endif // CSS_TRANSITIONS
	}

    int magic_number = 0;

	switch (decl->GetDeclType())
	{
	case CSS_DECL_TYPE:
		CSS::FormatCssValueL((void*)(long)decl->TypeValue(), CSS_VALUE_TYPE_keyword, tmp_buf, space_before);
		break;

	case CSS_DECL_STRING:
		if (((CSS_string_decl*)decl)->IsUrlString())
			CSS::FormatCssValueL((void*)decl->StringValue(), CSS_VALUE_TYPE_func_url, tmp_buf, space_before);
#ifdef SKIN_SUPPORT
		else if (((CSS_string_decl*)decl)->IsSkinString())
			CSS::FormatCssValueL((void*)decl->StringValue(), CSS_VALUE_TYPE_func_skin, tmp_buf, space_before);
#endif // SKIN_SUPPORT
		else
			CSS::FormatCssValueL((void*)decl->StringValue(), CSS_VALUE_TYPE_string, tmp_buf, space_before);
		break;

	case CSS_DECL_LONG:
		{
			if (IsColorProperty(decl->GetProperty()))
			{
				uni_char* tmp_str = (uni_char*)g_memory_manager->GetTempBuf2k();
				if (space_before)
					tmp_buf->AppendL(' ');
				HTM_Lex::GetRGBStringFromVal(decl->LongValue(), tmp_str, format_mode == CSS_FORMAT_CURRENT_STYLE);
				tmp_buf->AppendL(tmp_str);
			}
			else
				CSS::FormatCssValueL((void*)decl->LongValue(), CSS_VALUE_TYPE_long, tmp_buf, space_before);
		}
		break;

	case CSS_DECL_COLOR:
		CSS::FormatCssColorL(decl->LongValue(), tmp_buf, space_before, format_mode);
		break;

    case CSS_DECL_NUMBER4:
        magic_number += 2;
		// fall-through is intended
    case CSS_DECL_NUMBER2:
        magic_number++;
		// fall-through is intended
    case CSS_DECL_NUMBER:
        {
            magic_number++;
            for (int j = 0; j < magic_number; j++)
            {
				BOOL allow_collapse_if_equal = (decl->GetProperty() != CSS_PROPERTY_transform_origin);

                if (j == 1 && magic_number == 2 && allow_collapse_if_equal
                    && decl->GetNumberValue(j) == decl->GetNumberValue(j - 1)
                    && decl->GetValueType(j) == decl->GetValueType(j - 1)) break;

                CSS::FormatCssNumberL(decl->GetNumberValue(j), decl->GetValueType(j), tmp_buf, space_before || j > 0);
            }
        }
        break;

    case CSS_DECL_ARRAY:
    case CSS_DECL_GEN_ARRAY:
        {
            BOOL is_gen = decl->GetDeclType() == CSS_DECL_GEN_ARRAY;
			short prop = decl->GetProperty();
			BOOL use_comma = (prop == CSS_PROPERTY_font_family ||
							  prop == CSS_PROPERTY_background_image ||
							  prop == CSS_PROPERTY_background_attachment ||
							  prop == CSS_PROPERTY_background_clip ||
							  prop == CSS_PROPERTY_background_origin ||
							  prop == CSS_PROPERTY_cursor
#ifdef CSS_TRANSITIONS
							  || prop == CSS_PROPERTY_transition_delay
							  || prop == CSS_PROPERTY_transition_duration
							  || prop == CSS_PROPERTY_transition_property
#endif // CSS_TRANSITIONS
#ifdef CSS_ANIMATIONS
							  || prop == CSS_PROPERTY_animation_name
							  || prop == CSS_PROPERTY_animation_iteration_count
							  || prop == CSS_PROPERTY_animation_direction
							  || prop == CSS_PROPERTY_animation_fill_mode
							  || prop == CSS_PROPERTY_animation_play_state
							  || prop == CSS_PROPERTY_animation_delay
							  || prop == CSS_PROPERTY_animation_duration
#endif // CSS_ANIMATIONS
				);

            int array_len = decl->ArrayValueLen();
            for (int ai = 0; ai < array_len; ai++)
            {
				if (is_gen)
				{
					if (use_comma && ai > 0)
						tmp_buf->AppendL(',');

					const CSS_generic_value* gen_array = decl->GenArrayValue();
					FormatCssGenericValueL(CSSProperty(decl->GetProperty()), gen_array[ai], tmp_buf, ai > 0 || space_before, format_mode);
				}
#ifdef CSS_TRANSITIONS
				else
				{
					short* array = decl->ArrayValue();
					if (decl->GetProperty() == CSS_PROPERTY_transition_property)
					{
						if (use_comma && ai > 0)
							tmp_buf->AppendL(',');
						CSS::FormatCssValueL((void*)(INTPTR)array[ai], CSS_VALUE_TYPE_property, tmp_buf, space_before || ai > 0);
					}
				}
#endif // CSS_TRANSITIONS
			}
		}
		break;

	case CSS_DECL_PROXY:
		OP_ASSERT(!"Proxy objects must be resolved before they are formatted");
		break;

#ifdef CSS_TRANSFORMS
	case CSS_DECL_TRANSFORM_LIST:
		{
			const CSS_transform_list::CSS_transform_item* iter = static_cast<const CSS_transform_list*>(decl)->First();

			BOOL first_value = TRUE;
			BOOL tmp_space_before = space_before;

			while (iter)
			{
				CSS::FormatCssValueL((void*)(INTPTR)iter->type, CSS_VALUE_TYPE_keyword, tmp_buf, tmp_space_before || !first_value);

				tmp_buf->AppendL('(');
				tmp_space_before = FALSE;

				for (int i = 0; i < iter->n_values; i++)
				{
					if (i > 0)
					{
						tmp_buf->AppendL(',');
						tmp_space_before = TRUE;
					}

					CSS::FormatCssNumberL(iter->value[i], iter->value_type[i], tmp_buf, tmp_space_before);
				}

				tmp_buf->AppendL(')');

				first_value = FALSE;
				iter = iter->Suc();
			}
		}
		break;
#endif
	}
}

void
CSS::FormatCssTimingValueL(const CSS_generic_value *gen_arr, TempBuffer* tmp_buf, BOOL space_before)
{
	int i = 0;

	if (space_before)
		tmp_buf->AppendL(" ");

	if (gen_arr[i].GetValueType() == CSS_NUMBER)
	{
		tmp_buf->AppendL("cubic-bezier(");

		for (;;)
		{
			CSS::FormatCssNumberL(gen_arr[i++].GetReal(), CSS_NUMBER, tmp_buf, FALSE);

			if (i == 4)
			{
				tmp_buf->AppendL(")");
				break;
			}

			tmp_buf->Append(", ");
		}
	}
	else
	{
		OP_ASSERT(gen_arr[i].GetValueType() == CSS_INT_NUMBER);
		tmp_buf->AppendL("steps(");

		CSS::FormatCssValueL((void*)(INTPTR)gen_arr[i++].GetNumber(),
							 CSS::CSS_VALUE_TYPE_long, tmp_buf, FALSE);

		if (gen_arr[i++].GetType() == CSS_VALUE_start)
			tmp_buf->AppendL(", start)");
		else
			tmp_buf->AppendL(", end)");
	}
}

void
CSS::FormatCssGenericValueL(CSSProperty prop,
							const CSS_generic_value& gen_val,
							TempBuffer *tmp_buf,
							BOOL space_before,
							CSS_SerializationFormat format_mode)
{
	switch (gen_val.value_type)
	{
	case CSS_DECL_COLOR:
		CSS::FormatCssColorL(gen_val.value.color, tmp_buf, space_before, format_mode);
		break;
	case CSS_COMMA:
		tmp_buf->AppendL(',');
		break;
	case CSS_SLASH:
		if (space_before)
			tmp_buf->AppendL(' ');

		tmp_buf->AppendL('/');
		break;
	case CSS_IDENT:
		{
			short keyword = gen_val.value.type;
			if (prop == CSS_PROPERTY_font_family)
			{
				if ((keyword & CSS_GENERIC_FONT_FAMILY) == 0)
				{
					CSS::FormatCssValueL((void*)styleManager->GetFontFace(keyword), CSS_VALUE_TYPE_unquoted_string, tmp_buf, space_before);
					break;
				}
				else
					keyword &= CSS_GENERIC_FONT_FAMILY_mask;
			}
			CSS::FormatCssValueL((void*)(long)keyword, CSS_VALUE_TYPE_keyword, tmp_buf, space_before);
		}
		break;
	case CSS_STRING_LITERAL:
		{
			BOOL quotable_string = (prop != CSS_PROPERTY_counter_increment &&
									prop != CSS_PROPERTY_counter_reset);

#ifdef CSS_ANIMATIONS
			if (prop == CSS_PROPERTY_animation_name)
				quotable_string = FALSE;
#endif // CSS_ANIMATIONS

			CSS::FormatCssValueL((void*)gen_val.value.string, quotable_string ? CSS_VALUE_TYPE_string : CSS_VALUE_TYPE_unquoted_string, tmp_buf, space_before);
		}
		break;
	case CSS_FUNCTION_ATTR:
		CSS::FormatCssValueL((void*)gen_val.value.string, CSS_VALUE_TYPE_func_attr, tmp_buf, space_before);
		break;
	case CSS_FUNCTION_URL:
		CSS::FormatCssValueL((void*)gen_val.value.string, CSS_VALUE_TYPE_func_url, tmp_buf, space_before);
		break;
#ifdef CSS_GRADIENT_SUPPORT
	case CSS_FUNCTION_LINEAR_GRADIENT:
	case CSS_FUNCTION_WEBKIT_LINEAR_GRADIENT:
	case CSS_FUNCTION_O_LINEAR_GRADIENT:
	case CSS_FUNCTION_REPEATING_LINEAR_GRADIENT:
		CSS::FormatCssValueL((void*)gen_val.value.gradient, CSS_VALUE_TYPE_func_linear_gradient, tmp_buf, space_before);
		break;

	case CSS_FUNCTION_RADIAL_GRADIENT:
	case CSS_FUNCTION_REPEATING_RADIAL_GRADIENT:
		CSS::FormatCssValueL((void*)gen_val.value.gradient, CSS_VALUE_TYPE_func_radial_gradient, tmp_buf, space_before);
		break;
#endif
#ifdef SKIN_SUPPORT
	case CSS_FUNCTION_SKIN:
		CSS::FormatCssValueL((void*)gen_val.value.string, CSS_VALUE_TYPE_func_skin, tmp_buf, space_before);
		break;
#endif // SKIN_SUPPORT
	case CSS_FUNCTION_COUNTER:
		CSS::FormatCssValueL((void*)gen_val.value.string, CSS_VALUE_TYPE_func_counter, tmp_buf, space_before);
		break;
	case CSS_FUNCTION_COUNTERS:
		CSS::FormatCssValueL((void*)gen_val.value.string, CSS_VALUE_TYPE_func_counters, tmp_buf, space_before);
		break;
	case CSS_FUNCTION_FORMAT:
		{
			const uni_char* format_str;
			switch (gen_val.GetType())
			{
			case CSS_WebFont::FORMAT_TRUETYPE:
				format_str = UNI_L("truetype");
				break;
			case CSS_WebFont::FORMAT_TRUETYPE_AAT:
				format_str = UNI_L("truetype-aat");
				break;
			case CSS_WebFont::FORMAT_OPENTYPE:
				format_str = UNI_L("opentype");
				break;
			case CSS_WebFont::FORMAT_EMBEDDED_OPENTYPE:
				format_str = UNI_L("embedded-opentype");
				break;
			case CSS_WebFont::FORMAT_WOFF:
				format_str = UNI_L("woff");
				break;
			case CSS_WebFont::FORMAT_SVG:
			case CSS_WebFont::FORMAT_UNKNOWN:
			default:
				format_str = UNI_L("svg");
				break;
			}
			CSS::FormatCssValueL((void*)format_str, CSS_VALUE_TYPE_func_format, tmp_buf, space_before);
		}
		break;
	case CSS_FUNCTION_LOCAL:
		CSS::FormatCssValueL((void*)gen_val.value.string, CSS_VALUE_TYPE_func_local, tmp_buf, space_before);
		break;
	case CSS_INT_NUMBER:
		CSS::FormatCssNumberL((float)gen_val.value.number, CSS_NUMBER, tmp_buf, space_before);
		break;
	case CSS_PERCENTAGE:
	case CSS_SECOND:
	case CSS_EM:
	case CSS_REM:
	case CSS_EX:
	case CSS_PX:
	case CSS_CM:
	case CSS_MM:
	case CSS_IN:
	case CSS_PT:
	case CSS_PC:
	case CSS_MS:
	case CSS_HZ:
	case CSS_DEG:
	case CSS_RAD:
	case CSS_KHZ:
	case CSS_GRAD:
	case CSS_NUMBER:
	case CSS_DIMEN:
		CSS::FormatCssNumberL((float)gen_val.value.real, gen_val.value_type, tmp_buf, space_before);
		break;
	case CSS_HASH:
		CSS::FormatCssValueL((void*)gen_val.value.string, CSS_VALUE_TYPE_hash, tmp_buf, space_before);
		break;
	}
}

OP_STATUS CSS::FormatQuotedString(TempBuffer* tmp_buf, const uni_char* str_val)
{
	TRAPD(ret, FormatQuotedStringL(tmp_buf, str_val));
	return ret;
}

void CSS::FormatQuotedStringL(TempBuffer* tmp_buf, const uni_char* str_val)
{
	tmp_buf->AppendL('"');

	const uni_char* tmp_ptr = str_val;
	while (*tmp_ptr)
	{
		const uni_char* next_escape = tmp_ptr;
		uni_char escape_char = 0;
		while (*next_escape)
		{
			switch (*next_escape)
			{
			case '\n':
				escape_char = 'a';
				break;
			case '\f':
				escape_char = 'c';
				break;
			case '\r':
				escape_char = 'd';
				break;
			case '\\':
				escape_char = '\\';
				break;
			case '"':
				escape_char = '"';
				break;
			default:
				next_escape++;
				continue;
			}
			break;
		}

		if (*next_escape)
		{
			tmp_buf->AppendL(tmp_ptr, next_escape - tmp_ptr);
			tmp_buf->AppendL('\\');
			tmp_buf->AppendL(escape_char);
			next_escape++;
		}
		else
			tmp_buf->AppendL(tmp_ptr);

		tmp_ptr = next_escape;
	}

	tmp_buf->AppendL('"');
}

void CSS::FormatCssColorL(long color_value, TempBuffer* tmp_buf, BOOL space_before, CSS_SerializationFormat format_mode)
{
	if (space_before)
		tmp_buf->AppendL(' ');

	if ((color_value & CSS_COLOR_KEYWORD_TYPE_named) && format_mode == CSS_FORMAT_CSSOM_STYLE)
	{
		long color_index = color_value & CSS_COLOR_KEYWORD_TYPE_index;

		if ((color_value & CSS_COLOR_KEYWORD_TYPE_ui_color) == CSS_COLOR_KEYWORD_TYPE_ui_color)
			CSS::FormatCssValueL((void*)color_index, CSS_VALUE_TYPE_keyword, tmp_buf, FALSE);
		else if (COLOR_is_indexed(color_value))
			tmp_buf->AppendL(HTM_Lex::GetColNameByIndex(color_index));
		else
		{
			/* For CSSOM we want to output the keyword, whether the
			   variable has actual color stored or not. */
			if ((color_value & CSS_COLOR_invert) == CSS_COLOR_invert)
				color_index = CSS_VALUE_invert;
			else if ((color_value & CSS_COLOR_transparent) == CSS_COLOR_transparent)
				color_index = CSS_VALUE_transparent;
			else if ((color_value & CSS_COLOR_inherit) == CSS_COLOR_inherit)
				color_index = CSS_VALUE_inherit;
			else if ((color_value & CSS_COLOR_current_color) == CSS_COLOR_current_color)
				color_index = CSS_VALUE_currentColor;

			OP_ASSERT(color_index);

			CSS::FormatCssValueL((void*)color_index, CSS_VALUE_TYPE_keyword, tmp_buf, FALSE);
		}
	}
	else
	{
		uni_char* tmp_str = (uni_char*)g_memory_manager->GetTempBuf2k();
		HTM_Lex::GetRGBStringFromVal(HTM_Lex::GetColValByIndex(color_value), tmp_str, format_mode == CSS_FORMAT_CURRENT_STYLE);
		tmp_buf->AppendL(tmp_str);
	}
}

void CSS::FormatCssStringL(TempBuffer* tmp_buf, const uni_char* str_val, const uni_char* prefix, const uni_char* suffix, BOOL force_quote)
{
	if (prefix)
		tmp_buf->AppendL(prefix);

	if (str_val)
	{
		BOOL must_quote = force_quote;
		if (!must_quote)
		{
			int i = 0;
			while (str_val[i])
			{
				if (uni_isspace(str_val[i]) || str_val[i] == '"' || str_val[i] == '\'')
					must_quote = TRUE;
				i++;
			}
		}

		if (must_quote)
			CSS::FormatQuotedStringL(tmp_buf, str_val);
		else
			tmp_buf->AppendL(str_val);
	}

	if (suffix)
		tmp_buf->AppendL(suffix);
}

void CSS::FormatCssValueL(void* value, CssValue_t type, TempBuffer* tmp_buf, BOOL space_before)
{
    uni_char* tmp_str = (uni_char*)g_memory_manager->GetTempBuf2k();

    if (space_before)
		tmp_buf->AppendL(' ');

    switch (type)
    {
	case CSS_VALUE_TYPE_hash:
		tmp_buf->AppendL("#");
		CSS::FormatCssStringL(tmp_buf, (uni_char*)value, NULL, NULL, FALSE);
		return;

    case CSS_VALUE_TYPE_long:
        uni_ltoa((long)value, tmp_str, 10);
        break;

    case CSS_VALUE_TYPE_color:
        HTM_Lex::GetRGBStringFromVal((long)value, tmp_str, FALSE);
        break;

    case CSS_VALUE_TYPE_string:
		CSS::FormatCssStringL(tmp_buf, (uni_char*)value, NULL, NULL, TRUE);
        return;

    case CSS_VALUE_TYPE_keyword:
		{
			short keyword = (short)(INTPTR)value;
			const uni_char* keyword_str = CSS_GetKeywordString(keyword);
			// #internal keywords from css_values.txt don't have any string representation
			if (!keyword_str)
				keyword_str = CSS_GetInternalKeywordString(keyword);
			OP_ASSERT(keyword_str);
			tmp_buf->AppendL(keyword_str);
		}
		return;

	case CSS_VALUE_TYPE_property:
		{
			char* prop_tmp_str = (char*)tmp_str;
			short prop = (short)(INTPTR)value;
			const char* property = prop == -1 ? "all" : GetCSS_PropertyName(prop);
			OP_ASSERT(property);
			op_strcpy(prop_tmp_str, property);
			op_strlwr(prop_tmp_str);
			tmp_buf->AppendL(prop_tmp_str);
		}
		return;

	case CSS_VALUE_TYPE_func_url:
		CSS::FormatCssStringL(tmp_buf, (uni_char*)value, UNI_L("url("), UNI_L(")"), TRUE);
		return;

	case CSS_VALUE_TYPE_func_attr:
		CSS::FormatCssStringL(tmp_buf, (uni_char*)value, UNI_L("attr("), UNI_L(")"), FALSE);
		return;

#ifdef SKIN_SUPPORT
	case CSS_VALUE_TYPE_func_skin:
		CSS::FormatCssStringL(tmp_buf, ((uni_char*)value)+2, UNI_L("-o-skin("), UNI_L(")"), TRUE);
		return;
#endif // SKIN_SUPPORT

	case CSS_VALUE_TYPE_func_local:
		CSS::FormatCssStringL(tmp_buf, (uni_char*)value, UNI_L("local("), UNI_L(")"), TRUE);
		return;

	case CSS_VALUE_TYPE_func_format:
		CSS::FormatCssStringL(tmp_buf, (uni_char*)value, UNI_L("format("), UNI_L(")"), TRUE);
		return;

	case CSS_VALUE_TYPE_func_counter:
		CSS::FormatCssStringL(tmp_buf, (uni_char*)value, UNI_L("counter("), UNI_L(")"), FALSE);
		return;

	case CSS_VALUE_TYPE_func_counters:
		CSS::FormatCssStringL(tmp_buf, (uni_char*)value, UNI_L("counters("), UNI_L(")"), FALSE);
		return;

    case CSS_VALUE_TYPE_unquoted_string:
		CSS::FormatCssStringL(tmp_buf, (uni_char*)value, NULL, NULL, FALSE);
		return;

#ifdef CSS_GRADIENT_SUPPORT
    case CSS_VALUE_TYPE_func_linear_gradient:
		{
			CSS_LinearGradient* gradient = static_cast<CSS_LinearGradient*>(value);
			if (gradient->repeat)
				tmp_buf->AppendL(UNI_L("repeating-linear-gradient("));
			else
			{
				if (gradient->webkit_prefix)
					tmp_buf->AppendL(UNI_L("-webkit-linear-gradient("));
				else if (gradient->o_prefix)
					tmp_buf->AppendL(UNI_L("-o-linear-gradient("));
				else
					tmp_buf->AppendL(UNI_L("linear-gradient("));
			}
			gradient->ToStringL(tmp_buf);
			tmp_buf->AppendL(UNI_L(")"));
		}
		return;

    case CSS_VALUE_TYPE_func_radial_gradient:
		{
			CSS_RadialGradient* gradient = static_cast<CSS_RadialGradient*>(value);
			if (gradient->repeat)
				tmp_buf->AppendL(UNI_L("repeating-radial-gradient("));
			else
				tmp_buf->AppendL(UNI_L("radial-gradient("));
			gradient->ToStringL(tmp_buf);
			tmp_buf->AppendL(UNI_L(")"));
		}
		return;
#endif
    }

    tmp_buf->AppendL(tmp_str);
}

void CSS::FormatCssNumberL(float num, short type, TempBuffer* tmp_buf, BOOL space_before)
{
    if (type == CSS_DIM_TYPE)
	{
		if (space_before)
			tmp_buf->AppendL(' ');
		tmp_buf->AppendL(GetDimKeyword(num));
	}
	else
	{
		char* tmp_str = (char*)g_memory_manager->GetTempBuf2k();
		int tmp_str_idx = 0;

		if (space_before)
			tmp_str[tmp_str_idx++] = ' ';

		// Angles are always serialized as 'deg'
		if (type == CSS_RAD)
			num *= float(180.0/M_PI);

		double round_num = Round(num, 2, ROUND_NORMAL);
		if (round_num == -0.0)
			round_num = 0.0;
		OpDoubleFormat::ToString(&tmp_str[tmp_str_idx], round_num);

		tmp_str_idx += op_strlen(&tmp_str[tmp_str_idx]);

		switch (type)
		{
		case CSS_PERCENTAGE: tmp_str[tmp_str_idx++] = '%'; break;
		case CSS_SECOND: tmp_str[tmp_str_idx++] = 's'; break;
		case CSS_EM: tmp_str[tmp_str_idx++] = 'e'; tmp_str[tmp_str_idx++] = 'm'; break;
		case CSS_REM: tmp_str[tmp_str_idx++] = 'r'; tmp_str[tmp_str_idx++] = 'e'; tmp_str[tmp_str_idx++] = 'm'; break;
		case CSS_EX: tmp_str[tmp_str_idx++] = 'e'; tmp_str[tmp_str_idx++] = 'x'; break;
		case CSS_PX: tmp_str[tmp_str_idx++] = 'p'; tmp_str[tmp_str_idx++] = 'x'; break;
		case CSS_CM: tmp_str[tmp_str_idx++] = 'c'; tmp_str[tmp_str_idx++] = 'm'; break;
		case CSS_MM: tmp_str[tmp_str_idx++] = 'm'; tmp_str[tmp_str_idx++] = 'm'; break;
		case CSS_IN: tmp_str[tmp_str_idx++] = 'i'; tmp_str[tmp_str_idx++] = 'n'; break;
		case CSS_PT: tmp_str[tmp_str_idx++] = 'p'; tmp_str[tmp_str_idx++] = 't'; break;
		case CSS_PC: tmp_str[tmp_str_idx++] = 'p'; tmp_str[tmp_str_idx++] = 'c'; break;
		case CSS_MS: tmp_str[tmp_str_idx++] = 'm'; tmp_str[tmp_str_idx++] = 's'; break;
		case CSS_HZ: tmp_str[tmp_str_idx++] = 'H'; tmp_str[tmp_str_idx++] = 'z'; break;
		// Angles are always serialized as 'deg'
		case CSS_RAD: tmp_str[tmp_str_idx++] = 'd'; tmp_str[tmp_str_idx++] = 'e'; tmp_str[tmp_str_idx++] = 'g'; break;
		case CSS_GRAD: tmp_str[tmp_str_idx++] = 'g'; tmp_str[tmp_str_idx++] = 'r'; tmp_str[tmp_str_idx++] = 'a'; tmp_str[tmp_str_idx++] = 'd'; break;
		case CSS_DPI: tmp_str[tmp_str_idx++] = 'd'; tmp_str[tmp_str_idx++] = 'p'; tmp_str[tmp_str_idx++] = 'i'; break;
		case CSS_DPCM: tmp_str[tmp_str_idx++] = 'd'; tmp_str[tmp_str_idx++] = 'p'; tmp_str[tmp_str_idx++] = 'c'; tmp_str[tmp_str_idx++] = 'm'; break;
		case CSS_DPPX: tmp_str[tmp_str_idx++] = 'd'; tmp_str[tmp_str_idx++] = 'p'; tmp_str[tmp_str_idx++] = 'p'; tmp_str[tmp_str_idx++] = 'x'; break;
		case CSS_NUMBER:
		case CSS_DIMEN: break;
		}
		tmp_str[tmp_str_idx++] = '\0';

		tmp_buf->AppendL(tmp_str);
	}
}

void CSS::FormatCssPropertyNameL(short property, TempBuffer* tmp_buf)
{
	char name[CSS_PROPERTY_NAME_MAX_SIZE+1]; /* ARRAY OK 2009-02-09 rune */

	op_strncpy(name, GetCSS_PropertyName(property), CSS_PROPERTY_NAME_MAX_SIZE);
	name[CSS_PROPERTY_NAME_MAX_SIZE] = '\0';
	op_strlwr(name);

	tmp_buf->AppendL(name);
}

void CSS::FormatCssAspectRatioL(short w, short h, TempBuffer* tmp_buf)
{
	char num_buf[8]; /* ARRAY OK 2009-02-09 rune */
	op_itoa(w, num_buf, 10);
	tmp_buf->AppendL(num_buf);
	tmp_buf->AppendL("/");
	op_itoa(h, num_buf, 10);
	tmp_buf->AppendL(num_buf);
}

BOOL CSS::IsImport()
{
	return m_src_html_element->IsCssImport();
}

CSS* CSS::GetNextImport(BOOL skip_children)
{
	HTML_Element* next_elm = m_src_html_element;
	CSS* css = NULL;
	do
	{
		if (!skip_children && next_elm->LastChild())
			next_elm = next_elm->LastChild();
		else
			next_elm = static_cast<HTML_Element*>(next_elm->PrevSibling());

		if (next_elm && next_elm->IsCssImport())
			css = next_elm->GetCSS();
	}
	while (!css && next_elm && next_elm->IsCssImport());

	return css;
}

/*
	HOW TO RETURN hsl.to.rgb(h, s, l):
		SELECT:
			l<=0.5: PUT l*(s+1) IN m2
			ELSE: PUT l+s-l*s IN m2
		PUT l*2-m2 IN m1
		PUT hue.to.rgb(m1, m2, h+1/3) IN r
		PUT hue.to.rgb(m1, m2, h    ) IN g
		PUT hue.to.rgb(m1, m2, h-1/3) IN b
		RETURN (r, g, b)

	HOW TO RETURN hue.to.rgb(m1, m2, h):
		IF h<0: PUT h+1 IN h
		IF h>1: PUT h-1 IN h
		IF h*6<1: RETURN m1+(m2-m1)*h*6
		IF h*2<1: RETURN m2
		IF h*3<2: RETURN m1+(m2-m1)*(2/3-h)*6
		RETURN m1
*/

static double HUE_TO_RGB(double m1, double m2, double h)
{
	if (h < 0)
		h += 1;
	else if (h > 1)
		h -= 1;

	if (h*6 < 1)
		return m1 + (m2 - m1)*h*6;
	else if (h*2 < 1)
		return m2;
	else if (h*3 < 2)
		return m1 + (m2 - m1)*(2/3.0 - h)*6;
	else
		return m1;
}

COLORREF HSLA_TO_RGBA(double h, double s, double l, double a)
{
	/* Normalize values inside 0..1 */
	h = op_fmod(op_fmod(h, 360.0) + 360.0, 360.0) / 360.0;
	if (s < 0)
		s = 0;
	else if (s > 100)
		s = 100;
	s /= 100.0;
	if (l < 0)
		l = 0;
	else if (l > 100)
		l = 100;
	l /= 100.0;

	/* Convert */
	double m1, m2;

	if (l <= 0.5)
		m2 = l*(s+1);
	else
		m2 = l + s - l*s;
	m1 = l*2 - m2;

	double r = HUE_TO_RGB(m1, m2, h + 1/3.0);
	double g = HUE_TO_RGB(m1, m2, h);
	double b = HUE_TO_RGB(m1, m2, h - 1/3.0);

	if (a >= 1.0)
		return OP_RGB((int)(OpRound(r*255)), (int)(OpRound(g*255)), (int)(OpRound(b*255)));
	else
		return OP_RGBA((int)(OpRound(r*255)), (int)(OpRound(g*255)), (int)(OpRound(b*255)), (int)(OpRound(a*255)));
}

BOOL ParseColor(const uni_char* string, int len, COLORREF& color)
{
	CSS_Buffer buf;
	if (!buf.AllocBufferArrays(1))
		return FALSE;

	buf.AddBuffer(string, len);
	CSS_Lexer lexer(&buf, TRUE);

	int alpha_count = 0;
	COLORREF alpha = 0x000000ff;
	int token;
	YYSTYPE value;
	// Eat whitespaces
	while ((token = lexer.Lex(&value)) == CSS_TOK_SPACE)
		;
	switch (token)
	{
	case CSS_TOK_IDENT:
		color = buf.GetNamedColorIndex(value.str.start_pos, value.str.str_len);
		if (color != USE_DEFAULT_COLOR)
			color = HTM_Lex::GetColValByIndex(color);
		else if (buf.GetValueSymbol(value.str.start_pos, value.str.str_len) == CSS_VALUE_transparent)
		{
			color = OP_RGBA(0, 0, 0, 0);
			alpha = 0;
		}
		break;
	case CSS_TOK_HASH:
		color = buf.GetColor(value.str.start_pos, value.str.str_len);
		break;
	case CSS_TOK_RGBA_FUNC:
		alpha_count = 2;
		// fall-through is intended
	case CSS_TOK_RGB_FUNC:
		{
			int type = 0; // CSS_TOK_PERCENTAGE or CSS_TOK_INTEGER
			UINT8 val[4];
			for (int i=0; i<5+alpha_count; i++)
			{
				while ((token = lexer.Lex(&value)) == CSS_TOK_SPACE)
					;
				if (i%2)
				{
					if (token != ',')
						return FALSE;
				}
				else
				{
					BOOL negative = FALSE;
					if (token == '-' || token == '+')
					{
						negative = (token == '-');
						token = lexer.Lex(&value);
					}

					int idx = i/2;
					if (idx < 3)
					{
						if (token != CSS_TOK_PERCENTAGE && token != CSS_TOK_INTEGER || type && type != token)
							return FALSE;

						if (!type)
							type = token;

						if (negative)
							val[idx] = 0;
						else if (type == CSS_TOK_PERCENTAGE)
						{
							double num = (value.number.number*255)/100;
							if (num > 255)
								val[idx] = 255;
							else
								val[idx] = static_cast<UINT8>(OpRound(num));
						}
						else
							val[idx] = MIN(value.integer.integer, 255);
					}
					else
					{
						if (token != CSS_TOK_NUMBER && token != CSS_TOK_INTEGER)
							return FALSE;

						if (negative)
							val[idx] = 0;
						else if (token == CSS_TOK_NUMBER)
						{
							double num = value.number.number*255;
							if (num > 255)
								val[idx] = 255;
							else
								val[idx] = static_cast<UINT8>(OpRound(num));
						}
						else
							val[idx] = MIN(value.integer.integer, 1)*255;
					}
				}
			}
			while ((token = lexer.Lex(&value)) == CSS_TOK_SPACE)
				;
			if (token == ')')
			{
				color = OP_RGBA(val[0], val[1], val[2], 0);
				if (alpha_count)
					alpha = val[3];
			}
		}
		break;
	case CSS_TOK_HSLA_FUNC:
		alpha_count = 2;
		// fall-through is intended
	case CSS_TOK_HSL_FUNC:
		{
			double val[4];
			for (int i=0; i<5+alpha_count; i++)
			{
				while ((token = lexer.Lex(&value)) == CSS_TOK_SPACE)
					;
				if (i%2)
				{
					if (token != ',')
						return FALSE;
				}
				else
				{
					BOOL negative = FALSE;
					if (token == '-' || token == '+')
					{
						negative = (token == '-');
						token = lexer.Lex(&value);
					}
					int idx = i/2;
					if (idx == 1 || idx == 2)
					{
						if (token == CSS_TOK_PERCENTAGE)
							val[idx] = value.number.number;
						else
							return FALSE;
					}
					else
					{
						if (token == CSS_TOK_NUMBER)
							val[idx] = value.number.number;
						else if (token == CSS_TOK_INTEGER)
							val[idx] = double(value.integer.integer);
						else
							return FALSE;
					}

					if (negative)
						val[idx] *= -1;
				}
			}
			while ((token = lexer.Lex(&value)) == CSS_TOK_SPACE)
				;
			if (token == ')')
			{
				// Use 0 for alpha since we'll add 8-bit alpha below instead of using 7-bit alpha with OP_RGBA
				color = HSLA_TO_RGBA(val[0], val[1], val[2], 0);
				if (alpha_count)
				{
					int tmp_a = (int)OpRound(val[3]*255);
					if (tmp_a > 255)
						tmp_a = 255;
					else if (tmp_a < 0)
						tmp_a = 0;
					alpha = (COLORREF)tmp_a;
				}
			}
		}
		break;
	default:
		return FALSE;
	}
	// Eat whitespaces
	while ((token = lexer.Lex(&value)) == CSS_TOK_SPACE)
		;
	if (token != CSS_TOK_EOF || color == USE_DEFAULT_COLOR)
		return FALSE;
	else
	{
		color |= alpha<<24;
		return TRUE;
	}
}

OP_STATUS ParseFontShorthand(const uni_char* string, unsigned len,
							 HLDocProfile* hld_profile, CSS_property_list*& out_props)
{
	CSS_Buffer buf;
	if (!buf.AllocBufferArrays(1))
		return OpStatus::ERR_NO_MEMORY;

	buf.AddBuffer(string, len);

	// The font short-hand doesn't take any url() values
	// Just pass empty url as base.
	URL base_url;
	CSS_Parser parser(NULL, &buf, base_url, hld_profile, 1);
	parser.SetDOMProperty(CSS_PROPERTY_font);
	parser.SetNextToken(CSS_TOK_DOM_STYLE_PROPERTY);

	TRAPD(err, parser.ParseL());
	if (OpStatus::IsMemoryError(err))
		return err;

	out_props = parser.GetCurrentPropList();
	if (!out_props)
		return OpStatus::ERR;

	out_props->PostProcess(TRUE, FALSE);
	out_props->Ref();

	return OpStatus::OK;
}

OP_STATUS CSS::InsertFontFaceRule(CSS_FontfaceRule* font_rule)
{
// Add font to hashtable.
	const uni_char* family_name = font_rule->GetFamilyName();
	if (family_name)
	{
		Head* existing_rules = NULL;
		m_webfonts.GetData(family_name, &existing_rules);
		if (existing_rules)
		{
			CSS_WebFont* existing_rule = static_cast<CSS_WebFont*>(existing_rules->First());
			while (existing_rule)
			{
				if (font_rule->IsDescriptorEqual(existing_rule))
				{
					existing_rule->Out();
					break; // it should only be possible to have one match
				}

				existing_rule = static_cast<CSS_WebFont*>(existing_rule->Suc());
			}

			static_cast<CSS_WebFont*>(font_rule)->Into(existing_rules);
		}
		else
		{
			existing_rules = OP_NEW(Head, ());
			if (existing_rules)
			{
				OP_STATUS stat = m_webfonts.Add(family_name, existing_rules);
				if (OpStatus::IsSuccess(stat))
				{
					static_cast<CSS_WebFont*>(font_rule)->Into(existing_rules);
				}
				else
				{
					OP_DELETE(existing_rules);
					return stat;
				}
			}
			else
			{
				return OpStatus::ERR_NO_MEMORY;
			}
		}
	}
	return OpStatus::OK;
}

OP_STATUS CSS::AddRule(HLDocProfile* hld_prof, CSS_Rule* rule, CSS_ConditionalRule* current_conditional)
{
	OP_STATUS stat = OpStatus::OK;

	if (rule->GetType() == CSS_Rule::STYLE)
	{
		CSS_StyleRule* style_rule = static_cast<CSS_StyleRule*>(rule);
		CSS_Selector* sel = style_rule->FirstSelector();
		style_rule->SetRuleNumber(m_next_rule_number++);

		while (sel)
		{
			if (sel->HasSingleIdInTarget())
			{
				CSS_SelectorAttribute* sel_attr = sel->FirstSelector()->GetFirstAttr();
				while (sel_attr && sel_attr->GetType() != CSS_SEL_ATTR_TYPE_ID)
					sel_attr = sel_attr->Suc();

				OP_ASSERT(sel_attr);
				const uni_char* id = sel_attr->GetId();

				CSS_RuleElmList* rule_list;
				if (OpStatus::IsError(m_id_rules.GetData(id, &rule_list)))
				{
					rule_list = OP_NEW(CSS_RuleElmList, ());
					if (rule_list)
					{
						stat = m_id_rules.Add(id, rule_list);
						if (stat != OpStatus::OK)
						{
							OP_DELETE(rule_list);
							rule_list = NULL;
						}
					}
					else
						stat = OpStatus::ERR_NO_MEMORY;
				}
				if (rule_list)
					stat = rule_list->AddRule(style_rule, sel);
			}
			else if (sel->HasSingleClassInTarget())
			{
				CSS_SelectorAttribute* sel_attr = sel->FirstSelector()->GetFirstAttr();
				while (sel_attr && sel_attr->GetType() != CSS_SEL_ATTR_TYPE_CLASS)
					sel_attr = sel_attr->Suc();

				OP_ASSERT(sel_attr);
				const uni_char* cls = sel_attr->GetClass();

				CSS_RuleElmList* rule_list;
				if (OpStatus::IsError(m_class_rules.GetData(cls, &rule_list)))
				{
					rule_list = OP_NEW(CSS_RuleElmList, ());
					if (rule_list)
					{
						stat = m_class_rules.Add(cls, rule_list);
						if (stat != OpStatus::OK)
						{
							OP_DELETE(rule_list);
							rule_list = NULL;
						}
					}
					else
						stat = OpStatus::ERR_NO_MEMORY;
				}
				if (rule_list)
					stat = rule_list->AddRule(style_rule, sel);
			}
			else
			{
				UINT32 idx = sel->GetTargetElmType();
				CSS_RuleElmList* rule_list;
				if (OpStatus::IsError(m_type_rules.GetData(idx, &rule_list)))
				{
					rule_list = OP_NEW(CSS_RuleElmList, ());
					if (rule_list)
					{
						stat = m_type_rules.Add(idx, rule_list);
						if (stat != OpStatus::OK)
						{
							OP_DELETE(rule_list);
							rule_list = NULL;
						}
					}
					else
						stat = OpStatus::ERR_NO_MEMORY;
				}
				if (rule_list)
					stat = rule_list->AddRule(style_rule, sel);
			}

			int succ_adj = sel->GetMaxSuccessiveAdjacent();
			if (m_succ_adj < succ_adj)
				m_succ_adj = succ_adj;

#ifdef DOM_FULLSCREEN_MODE
			if (sel->HasComplexFullscreen())
				m_complex_fullscreen = TRUE;
#endif // DOM_FULLSCREEN_MODE

			sel = static_cast<CSS_Selector*>(sel->Suc());
		}
	}
	else if (rule->GetType() == CSS_Rule::PAGE)
	{
		stat = m_page_rules.AddRule(static_cast<CSS_PageRule*>(rule), NULL);
	}
	else if (rule->GetType() == CSS_Rule::MEDIA)
	{
		CSS_MediaObject* media_obj = static_cast<CSS_MediaRule*>(rule)->GetMediaObject();
		if (media_obj)
			stat = media_obj->AddQueryLengths(this);
	}
	else if (rule->GetType() == CSS_Rule::IMPORT)
	{
		CSS_MediaObject* media_obj = static_cast<CSS_ImportRule*>(rule)->GetMediaObject();
		if (media_obj)
			stat = media_obj->AddQueryLengths(this);
	}
	else if (rule->GetType() == CSS_Rule::FONT_FACE)
	{
		stat = InsertFontFaceRule(static_cast<CSS_FontfaceRule*>(rule));
	}
#ifdef CSS_VIEWPORT_SUPPORT
	else if (rule->GetType() == CSS_Rule::VIEWPORT)
	{
		stat = m_viewport_rules.AddRule(static_cast<CSS_ViewportRule*>(rule), NULL);
	}
#endif // CSS_VIEWPORT_SUPPORT
#ifdef CSS_ANIMATIONS
	else if (rule->GetType() == CSS_Rule::KEYFRAMES)
	{
		stat = m_keyframes_rules.AddRule(static_cast<CSS_BlockRule*>(rule), NULL);
	}
#endif // CSS_ANIMATIONS

	if (current_conditional)
	{
		OP_ASSERT(rule->GetType() == CSS_Rule::STYLE ||
				  rule->GetType() == CSS_Rule::PAGE ||
				  rule->GetType() == CSS_Rule::FONT_FACE ||
				  rule->GetType() == CSS_Rule::VIEWPORT ||
				  rule->GetType() == CSS_Rule::KEYFRAMES ||
				  rule->GetType() == CSS_Rule::SUPPORTS ||
				  rule->GetType() == CSS_Rule::MEDIA);

		static_cast<CSS_BlockRule*>(rule)->SetConditionalRule(current_conditional);
		current_conditional->AddRule(rule);
	}
	else
		rule->Into(&m_rules);

	return stat;
}

BOOL CSS::DeleteRule(HLDocProfile* hld_prof, CSS_Rule* rule)
{
	if (m_rules.HasLink(rule))
	{
		RuleRemoved(hld_prof, rule);
		rule->Out();
		OP_DELETE(rule);
		return TRUE;
	}
	else
		return FALSE;
}

void CSS::RuleRemoved(HLDocProfile* hld_prof, CSS_Rule* rule)
{
	CSS_Rule::Type rule_type = rule->GetType();

	switch (rule_type)
	{
	case CSS_Rule::STYLE:
		{
			CSS_Selector* sel = static_cast<CSS_StyleRule*>(rule)->FirstSelector();
			while (sel)
			{
				if (sel->HasSingleIdInTarget())
				{
					CSS_SelectorAttribute* sel_attr = sel->FirstSelector()->GetFirstAttr();
					while (sel_attr && sel_attr->GetType() != CSS_SEL_ATTR_TYPE_ID)
						sel_attr = sel_attr->Suc();

					OP_ASSERT(sel_attr);
					const uni_char* id = sel_attr->GetId();

					CSS_RuleElmList* rule_list;
					if (OpStatus::IsSuccess(m_id_rules.GetData(id, &rule_list)))
					{
						m_id_rules.Remove(id, &rule_list);

						rule_list->DeleteRule(rule);

						if (rule_list->First())
						{
							CSS_StyleRule* new_rule = static_cast<CSS_StyleRule*>(rule_list->First()->GetRule());
							CSS_Selector* new_sel = new_rule->FirstSelector();
							const uni_char* new_id = NULL;
							while (new_sel)
							{
								if (new_sel->HasSingleIdInTarget())
								{
									CSS_SelectorAttribute* new_sel_attr = new_sel->FirstSelector()->GetFirstAttr();
									while (new_sel_attr && new_sel_attr->GetType() != CSS_SEL_ATTR_TYPE_ID)
										new_sel_attr = new_sel_attr->Suc();

									new_id = new_sel_attr->GetId();
									if (uni_stricmp(new_id, id) == 0)
										break;
								}
								new_sel = static_cast<CSS_Selector*>(new_sel->Suc());
							}

							OP_ASSERT(new_id);
							m_id_rules.Add(new_id, rule_list);
						}
						else
							OP_DELETE(rule_list);
					}
				}
				else if (sel->HasSingleClassInTarget())
				{
					CSS_SelectorAttribute* sel_attr = sel->FirstSelector()->GetFirstAttr();
					while (sel_attr && sel_attr->GetType() != CSS_SEL_ATTR_TYPE_CLASS)
						sel_attr = sel_attr->Suc();

					OP_ASSERT(sel_attr);
					const uni_char* cls = sel_attr->GetClass();

					CSS_RuleElmList* rule_list;
					if (OpStatus::IsSuccess(m_class_rules.GetData(cls, &rule_list)))
					{
						m_class_rules.Remove(cls, &rule_list);
						rule_list->DeleteRule(rule);

						if (rule_list->First())
						{
							CSS_StyleRule* new_rule = static_cast<CSS_StyleRule*>(rule_list->First()->GetRule());
							CSS_Selector* new_sel = new_rule->FirstSelector();
							const uni_char* new_cls = NULL;
							while (new_sel)
							{
								if (new_sel->HasSingleClassInTarget())
								{
									CSS_SelectorAttribute* new_sel_attr = new_sel->FirstSelector()->GetFirstAttr();
									while (new_sel_attr && new_sel_attr->GetType() != CSS_SEL_ATTR_TYPE_CLASS)
										new_sel_attr = new_sel_attr->Suc();

									new_cls = new_sel_attr->GetClass();
									if (uni_stricmp(new_cls, cls) == 0)
										break;
								}
								new_sel = static_cast<CSS_Selector*>(new_sel->Suc());
							}

							OP_ASSERT(new_cls);
							m_class_rules.Add(new_cls, rule_list);
						}
						else
							OP_DELETE(rule_list);
					}
				}
				else
				{
					UINT32 idx = sel->GetTargetElmType();
					CSS_RuleElmList* rule_list;
					if (OpStatus::IsSuccess(m_type_rules.GetData(idx, &rule_list)))
					{
						rule_list->DeleteRule(rule);
						if (!rule_list->First())
						{
							m_type_rules.Remove(idx, &rule_list);
							OP_DELETE(rule_list);
						}
					}
				}

				sel = static_cast<CSS_Selector*>(sel->Suc());
			}
		}
		break;

	case CSS_Rule::PAGE:
		m_page_rules.DeleteRule(rule);
		break;

	case CSS_Rule::VIEWPORT:
#ifdef CSS_VIEWPORT_SUPPORT
		m_viewport_rules.DeleteRule(rule);
#endif // CSS_VIEWPORT_SUPPORT
		break;

	case CSS_Rule::SUPPORTS:
	case CSS_Rule::MEDIA:
		static_cast<CSS_ConditionalRule*>(rule)->DeleteRules(hld_prof, this);
		break;

	case CSS_Rule::FONT_FACE:
		{
			// Remove font from hashtable.
			CSS_FontfaceRule* remove_rule = static_cast<CSS_FontfaceRule*>(rule);
			const uni_char* family_name = remove_rule->GetFamilyName();
			Head* existing_rules = NULL;
			if (m_webfonts.GetData(family_name, &existing_rules) == OpStatus::OK && existing_rules)
			{
				// Remove the font variant
				static_cast<CSS_WebFont*>(remove_rule)->Out();

				if (existing_rules->Empty())
				{
					m_webfonts.Remove(family_name, &existing_rules);
					m_webfonts.Delete(existing_rules);
				}
			}
			if (hld_prof)
				remove_rule->RemovedWebFont(hld_prof->GetFramesDocument());
		}
		break;

	case CSS_Rule::IMPORT:
	case CSS_Rule::CHARSET:
	case CSS_Rule::UNKNOWN:
	default:
		break;
	}
}

OP_STATUS
CSS::ReplaceRule(HLDocProfile* hld_prof, CSS_Rule* old_rule, CSS_Rule* new_rule, CSS_ConditionalRule* current_conditional)
{
	OP_ASSERT(old_rule->GetType() == new_rule->GetType());

	// Replace in list.
	OP_STATUS ret = InsertRule(hld_prof, old_rule, new_rule, current_conditional);
	RuleRemoved(hld_prof, old_rule);
	old_rule->Out();
	OP_DELETE(old_rule);
	return ret;
}

OP_STATUS
CSS::InsertRule(HLDocProfile* hld_prof, CSS_Rule* before_rule, CSS_Rule* new_rule, CSS_ConditionalRule* current_conditional)
{
	if (current_conditional)
	{
		OP_ASSERT(new_rule->GetType() == CSS_Rule::STYLE ||
				  new_rule->GetType() == CSS_Rule::PAGE ||
				  new_rule->GetType() == CSS_Rule::FONT_FACE ||
				  new_rule->GetType() == CSS_Rule::VIEWPORT ||
				  new_rule->GetType() == CSS_Rule::SUPPORTS ||
				  new_rule->GetType() == CSS_Rule::MEDIA);
		static_cast<CSS_DeclarationBlockRule*>(new_rule)->SetConditionalRule(current_conditional);
	}

	if (before_rule)
		new_rule->Precede(before_rule);
	else if (current_conditional)
		current_conditional->AddRule(new_rule);
	else
		new_rule->Into(&m_rules);

	return RuleInserted(hld_prof, new_rule);
}

OP_STATUS
CSS::RuleInserted(HLDocProfile* hld_prof, CSS_Rule* rule)
{
	CSS_Rule::Type rule_type = rule->GetType();

	switch (rule_type)
	{
	case CSS_Rule::STYLE:
		{
			CSS_StyleRule* style_rule = static_cast<CSS_StyleRule*>(rule);
			UpdateRuleNumbers(style_rule);
			CSS_Selector* sel = style_rule->FirstSelector();
			while (sel)
			{
				OP_STATUS stat = OpStatus::OK;
				if (sel->HasSingleIdInTarget())
				{
					CSS_SelectorAttribute* sel_attr = sel->FirstSelector()->GetFirstAttr();
					while (sel_attr && sel_attr->GetType() != CSS_SEL_ATTR_TYPE_ID)
						sel_attr = sel_attr->Suc();

					OP_ASSERT(sel_attr);
					const uni_char* id = sel_attr->GetId();

					CSS_RuleElmList* rule_list;
					if (OpStatus::IsError(m_id_rules.GetData(id, &rule_list)))
					{
						rule_list = OP_NEW(CSS_RuleElmList, ());
						if (rule_list)
						{
							stat = m_id_rules.Add(id, rule_list);
							if (stat != OpStatus::OK)
							{
								OP_DELETE(rule_list);
								rule_list = NULL;
							}
						}
						else
							stat = OpStatus::ERR_NO_MEMORY;
					}
					if (rule_list)
						stat = rule_list->InsertRule(style_rule, sel);
				}
				else if (sel->HasSingleClassInTarget())
				{
					CSS_SelectorAttribute* sel_attr = sel->FirstSelector()->GetFirstAttr();
					while (sel_attr && sel_attr->GetType() != CSS_SEL_ATTR_TYPE_CLASS)
						sel_attr = sel_attr->Suc();

					OP_ASSERT(sel_attr);
					const uni_char* cls = sel_attr->GetClass();

					CSS_RuleElmList* rule_list;
					if (OpStatus::IsError(m_class_rules.GetData(cls, &rule_list)))
					{
						rule_list = OP_NEW(CSS_RuleElmList, ());
						if (rule_list)
						{
							stat = m_class_rules.Add(cls, rule_list);
							if (stat != OpStatus::OK)
							{
								OP_DELETE(rule_list);
								rule_list = NULL;
							}
						}
						else
							stat = OpStatus::ERR_NO_MEMORY;
					}
					if (rule_list)
						stat = rule_list->InsertRule(style_rule, sel);
				}
				else
				{
					UINT32 idx = sel->GetTargetElmType();
					CSS_RuleElmList* rule_list;
					if (OpStatus::IsError(m_type_rules.GetData(idx, &rule_list)))
					{
						rule_list = OP_NEW(CSS_RuleElmList, ());
						if (rule_list)
						{
							stat = m_type_rules.Add(idx, rule_list);
							if (stat != OpStatus::OK)
							{
								OP_DELETE(rule_list);
								rule_list = NULL;
							}
						}
						else
							stat = OpStatus::ERR_NO_MEMORY;
					}
					if (rule_list)
						stat = rule_list->InsertRule(style_rule, sel);
				}
				RETURN_IF_MEMORY_ERROR(stat);

				int succ_adj = sel->GetMaxSuccessiveAdjacent();
				if (m_succ_adj < succ_adj)
					m_succ_adj = succ_adj;

#ifdef DOM_FULLSCREEN_MODE
				if (sel->HasComplexFullscreen())
					m_complex_fullscreen = TRUE;
#endif // DOM_FULLSCREEN_MODE

				sel = static_cast<CSS_Selector*>(sel->Suc());
			}
		}
		break;

	case CSS_Rule::PAGE:
		{
			CSS_PageRule* page_rule = static_cast<CSS_PageRule*>(rule);
			UpdateRuleNumbers(page_rule);
			m_page_rules.InsertRule(page_rule, NULL);
		}
		break;

	case CSS_Rule::VIEWPORT:
#ifdef CSS_VIEWPORT_SUPPORT
		{
			CSS_ViewportRule* viewport_rule = static_cast<CSS_ViewportRule*>(rule);
			UpdateRuleNumbers(viewport_rule);
			m_viewport_rules.InsertRule(viewport_rule, NULL);
		}
#endif // CSS_VIEWPORT_SUPPORT
		break;

	case CSS_Rule::MEDIA:
		{
			CSS_MediaObject* media_obj = static_cast<CSS_MediaRule*>(rule)->GetMediaObject();
			if (media_obj)
				RETURN_IF_ERROR(media_obj->AddQueryLengths(this));
		}
		break;

	case CSS_Rule::IMPORT:
		{
			CSS_MediaObject* media_obj = static_cast<CSS_ImportRule*>(rule)->GetMediaObject();
			if (media_obj)
				RETURN_IF_ERROR(media_obj->AddQueryLengths(this));
		}
		break;

	case CSS_Rule::FONT_FACE:
		RETURN_IF_ERROR(InsertFontFaceRule(static_cast<CSS_FontfaceRule*>(rule)));
		break;
	case CSS_Rule::CHARSET:
	case CSS_Rule::UNKNOWN:
	default:
		break;
	}
	return OpStatus::OK;
}

CSS_ImportRule*
CSS::FindImportRule(HTML_Element* sheet_elm)
{
	CSS_Rule* rule = static_cast<CSS_Rule*>(m_rules.First());
	while (rule)
	{
		CSS_Rule::Type type = rule->GetType();
		if (type == CSS_Rule::IMPORT)
		{
			if (static_cast<CSS_ImportRule*>(rule)->GetElement() == sheet_elm)
				break;
		}
		else if (type != CSS_Rule::CHARSET)
		{
			rule = NULL;
			break;
		}
		rule = static_cast<CSS_Rule*>(rule->Suc());
	}

	// this method must not be called unless we know that a stylesheet is imported from this stylesheet.
	OP_ASSERT(rule);

	return static_cast<CSS_ImportRule*>(rule);
}

CSS_PARSE_STATUS
CSS::ParseAndInsertRule(HLDocProfile* hld_prof, CSS_Rule* before_rule, CSS_ConditionalRule* conditional_rule, CSS_KeyframesRule* keyframes_rule, BOOL replace, int start_token, const uni_char* buffer, int length)
{
	CSS_PARSE_STATUS stat = OpStatus::OK;

	CSS_Buffer* css_buf = OP_NEW(CSS_Buffer, ());

	if (css_buf && css_buf->AllocBufferArrays(1))
	{
		css_buf->AddBuffer(buffer, length);

		CSS_Parser* parser = OP_NEW(CSS_Parser, (this, css_buf, GetBaseURL(), hld_prof, 1));
		if (parser)
		{
			if (start_token == CSS_TOK_DOM_RULE)
			{
				/* Since we are trying to insert a CSSRule that can be of any
				   type, we need to set up the min/max allow levels to be able
				   to catch hierarchy errors correctly. */

				/* replace is only TRUE for setting cssText of exisiting rules
				   in which case it is only allowed to replace it with a rule
				   of the same type. That will also be reflected in the
				   start_token as the type of that rule and it will not cause
				   any hierarchy errors since the existing rule is already ok. */
				OP_ASSERT(!replace);

				CSS_Parser::AllowLevel allow_min = CSS_Parser::ALLOW_CHARSET;
				CSS_Parser::AllowLevel allow_max = CSS_Parser::ALLOW_STYLE;

				if (conditional_rule)
					allow_min = CSS_Parser::ALLOW_STYLE;
				else
				{
					CSS_Rule* pred = static_cast<CSS_Rule*>(before_rule ? before_rule->Pred() : m_rules.First());
					if (pred)
					{
						switch (pred->GetType())
						{
						case CSS_Rule::CHARSET:
						case CSS_Rule::IMPORT:
							allow_min = CSS_Parser::ALLOW_IMPORT;
							break;
						case CSS_Rule::NAMESPACE:
							allow_min = CSS_Parser::ALLOW_NAMESPACE;
							break;
						}
					}
					if (before_rule)
					{
						switch (before_rule->GetType())
						{
						case CSS_Rule::CHARSET:
							/* Not permitted to insert rules before @charset rules. */
							stat = CSSParseStatus::HIERARCHY_ERROR;
							break;
						case CSS_Rule::IMPORT:
							allow_max = CSS_Parser::ALLOW_IMPORT;
							break;
						case CSS_Rule::NAMESPACE:
							allow_max = CSS_Parser::ALLOW_NAMESPACE;
							break;
						}
					}
				}

				parser->SetAllowLevel(allow_min, allow_max);
			}

			if (OpStatus::IsSuccess(stat))
			{
				parser->SetNextToken(start_token);
				parser->SetDOMRule(before_rule, replace);
				parser->SetCurrentConditionalRule(conditional_rule);
#ifdef CSS_ANIMATIONS
				parser->SetCurrentKeyframesRule(keyframes_rule);
#endif // CSS_ANIMATIONS
				TRAP(stat, parser->ParseL());
			}
		}
		else
			stat = OpStatus::ERR_NO_MEMORY;

		OP_DELETE(parser);
	}
	else
		stat = OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsSuccess(stat))
	{
		if (replace)
		{
			unsigned int changes = CSSCollection::CHANGED_NONE;
			switch (start_token)
			{
			case CSS_TOK_DOM_SUPPORTS_RULE:
			case CSS_TOK_DOM_MEDIA_RULE:
			case CSS_TOK_DOM_IMPORT_RULE:
				changes = CSSCollection::CHANGED_ALL;
				break;
			case CSS_TOK_DOM_STYLE_RULE:
			case CSS_TOK_DOM_SELECTOR:
				changes = CSSCollection::CHANGED_PROPS;
				break;
			case CSS_TOK_DOM_FONT_FACE_RULE:
				changes = CSSCollection::CHANGED_WEBFONT;
				break;
			case CSS_TOK_DOM_PAGE_RULE:
			case CSS_TOK_DOM_PAGE_SELECTOR:
				changes = CSSCollection::CHANGED_PAGEPROPS;
				break;
			case CSS_TOK_DOM_VIEWPORT_RULE:
				changes = CSSCollection::CHANGED_VIEWPORT;
				break;
			case CSS_TOK_DOM_KEYFRAMES_RULE:
				changes = CSSCollection::CHANGED_KEYFRAMES;
			case CSS_TOK_DOM_KEYFRAME_RULE:
				break;
			default:
				OP_ASSERT(!"Rule type not handled");
			case CSS_TOK_DOM_CHARSET_RULE:
				break;
			}
			hld_prof->GetCSSCollection()->StyleChanged(changes);
		}
		else
		{
			OP_ASSERT(start_token == CSS_TOK_DOM_RULE || start_token == CSS_TOK_DOM_KEYFRAME_RULE); // If this changes, so should probably the handling.

			Added(hld_prof->GetCSSCollection(), hld_prof->GetFramesDocument());
		}
	}

	OP_DELETE(css_buf);
	return stat;
}

BOOL CSS::DeleteRule(HLDocProfile* hld_prof, unsigned int idx)
{
	Link* l = m_rules.First();
	while (l && idx-- > 0) l = l->Suc();
	if (l)
	{
		DeleteRule(hld_prof, static_cast<CSS_Rule*>(l));
		return TRUE;
	}
	else
		return FALSE;
}

void CSS::UpdateRuleNumbers(CSS_BlockRule* inserted_rule)
{
	CSS_Rule* prev = inserted_rule;
	while (prev)
	{
		if (prev->Pred())
			prev = static_cast<CSS_Rule*>(prev->Pred());
		else if (prev == inserted_rule || prev->GetType() == CSS_Rule::FONT_FACE)
			prev = static_cast<CSS_DeclarationBlockRule*>(prev)->GetConditionalRule();
		else
			prev = NULL;

		CSS_Rule::Type prev_type = prev ? prev->GetType() : CSS_Rule::UNKNOWN;
		if (prev_type == CSS_Rule::STYLE || prev_type == CSS_Rule::PAGE || prev_type == CSS_Rule::VIEWPORT)
			break;
	}

	unsigned int rule_num = prev ? static_cast<CSS_DeclarationBlockRule*>(prev)->GetRuleNumber() + 1 : 0;
	inserted_rule->SetRuleNumber(rule_num++);
	CSS_Rule* next = inserted_rule;
	while (next)
	{
		if (next->Suc())
			next = static_cast<CSS_Rule*>(next->Suc());
		else
		{
			switch (next->GetType())
			{
			case CSS_Rule::STYLE:
			case CSS_Rule::PAGE:
			case CSS_Rule::FONT_FACE:
			case CSS_Rule::VIEWPORT:
			case CSS_Rule::SUPPORTS:
			case CSS_Rule::MEDIA:
				next = static_cast<CSS_DeclarationBlockRule*>(next)->GetConditionalRule();
				break;
			default:
				next = NULL;
				break;
			}
		}

		CSS_Rule::Type next_type = next ? next->GetType() : CSS_Rule::UNKNOWN;
		if ((next_type == CSS_Rule::STYLE || next_type == CSS_Rule::PAGE || next_type == CSS_Rule::VIEWPORT) &&
			static_cast<CSS_DeclarationBlockRule*>(next)->GetRuleNumber() < rule_num)
		{
			static_cast<CSS_DeclarationBlockRule*>(next)->SetRuleNumber(rule_num++);
		}
	}

	if (!next)
		m_next_rule_number = rule_num;
}

BOOL CSS::IsSame(LogicalDocument* logdoc, CSS* css)
{
	const URL* url_1 = GetHRef(logdoc);
	const URL* url_2 = css->GetHRef(logdoc);
	return url_1 && url_2 && *url_1 == *url_2 && !IsModified() && !css->IsModified();
}

class MediaQueryLength : public Link
{
public:
	LayoutCoord length;
	char type_bits;
	MediaQueryLength* Suc() { return static_cast<MediaQueryLength*>(Link::Suc()); }
};

class MediaQueryRatio : public Link
{
public:
	LayoutCoord w;
	LayoutCoord h;
	char type_bits;
	MediaQueryRatio* Suc() { return static_cast<MediaQueryRatio*>(Link::Suc()); }
};

BOOL CSS::HasMediaQueryChanged(LayoutCoord old_width, LayoutCoord old_height, LayoutCoord new_width, LayoutCoord new_height)
{
	return HasMediaQueryChanged(m_query_widths, old_width, new_width)
		|| HasMediaQueryChanged(m_query_heights, old_height, new_height)
		|| HasMediaQueryChanged(m_query_ratios, old_width, old_height, new_width, new_height);
}

BOOL CSS::HasDeviceMediaQueryChanged(LayoutCoord old_device_width, LayoutCoord old_device_height, LayoutCoord new_device_width, LayoutCoord new_device_height)
{
	return HasMediaQueryChanged(m_query_device_widths, old_device_width, new_device_width)
		|| HasMediaQueryChanged(m_query_device_heights, old_device_height, new_device_height)
		|| HasMediaQueryChanged(m_query_device_ratios, old_device_width, old_device_height, new_device_width, new_device_height);
}

BOOL CSS::HasMediaQueryChanged(const Head& list, LayoutCoord old_length, LayoutCoord new_length)
{
	if (list.Empty() || old_length == new_length)
		return FALSE;

	MediaQueryLength* cur = static_cast<MediaQueryLength*>(list.First());

	LayoutCoord min_length, max_length;
	if (old_length > new_length)
	{
		min_length = new_length;
		max_length = old_length;
	}
	else
	{
		min_length = old_length;
		max_length = new_length;
	}

	while (cur && cur->length <= min_length)
	{
		if ((cur->type_bits & QUERY_EQUALS) && (cur->length == new_length || cur->length == old_length))
			return TRUE;
		cur = cur->Suc();
	}

	while (cur && cur->length <= max_length)
	{
		if ((cur->type_bits & QUERY_MIN) || (cur->type_bits & QUERY_EQUALS) && (cur->length == new_length || cur->length == old_length))
			return TRUE;
		cur = cur->Suc();
	}
	return FALSE;
}

BOOL CSS::HasMediaQueryChanged(const Head& list, LayoutCoord old_width, LayoutCoord old_height, LayoutCoord new_width, LayoutCoord new_height)
{
	if (list.Empty() || old_width*new_height == new_width*old_height)
		return FALSE;

	MediaQueryRatio* cur = static_cast<MediaQueryRatio*>(list.First());

	LayoutCoord min_w, min_h, max_w, max_h;
	if (old_width*new_height > new_width*old_height)
	{
		min_w = new_width;
		min_h = new_height;
		max_w = old_width;
		max_h = old_height;
	}
	else
	{
		min_w = old_width;
		min_h = old_height;
		max_w = new_width;
		max_h = new_height;
	}

	while (cur && (cur->w*min_h < cur->h*min_w))
		cur = cur->Suc();

	if (cur)
	{
		if (cur->w*min_h == cur->h*min_w)
		{
			if ((cur->type_bits & (QUERY_MAX|QUERY_EQUALS)))
				return TRUE;
			cur = cur->Suc();
		}

		// cur is now > min

		if (cur)
		{
			while (cur && cur->w*max_h < max_w*cur->h)
			{
				if ((cur->type_bits & (QUERY_MAX|QUERY_MIN)))
					return TRUE;
				cur = cur->Suc();
			}

			if (cur && cur->w*max_h == cur->h*max_w && (cur->type_bits & (QUERY_MIN|QUERY_EQUALS)))
				return TRUE;
		}
	}
	return FALSE;
}

OP_STATUS CSS::AddMediaQueryLength(Head& list, LayoutCoord length, MediaQueryType type)
{
	if (type == QUERY_MAX)
	{
		type = QUERY_MIN;
		length++;
	}
	MediaQueryLength* cur = static_cast<MediaQueryLength*>(list.First());
	while (cur && length > cur->length)
		cur = cur->Suc();
	if (!cur || cur->length > length)
	{
		MediaQueryLength* tmp = OP_NEW(MediaQueryLength, ());
		if (!tmp)
			return OpStatus::ERR_NO_MEMORY;
		tmp->type_bits = type;
		tmp->length = length;
		if (cur)
			tmp->Precede(cur);
		else
			tmp->Into(&list);
	}
	else
		cur->type_bits |= type;

	return OpStatus::OK;
}

OP_STATUS CSS::AddMediaQueryRatio(Head& list, LayoutCoord width, LayoutCoord height, MediaQueryType type)
{
	MediaQueryRatio* cur = static_cast<MediaQueryRatio*>(list.First());
	while (cur && cur->w*height < width*cur->h)
		cur = cur->Suc();
	if (!cur || cur->w*height > width*cur->h)
	{
		MediaQueryRatio* tmp = OP_NEW(MediaQueryRatio, ());
		if (!tmp)
			return OpStatus::ERR_NO_MEMORY;
		tmp->type_bits = type;
		tmp->w = width;
		tmp->h = height;
		if (cur)
			tmp->Precede(cur);
		else
			tmp->Into(&list);
	}
	else
		cur->type_bits |= type;

	return OpStatus::OK;
}

OP_STATUS
CSS::MediaAttrChanged()
{
	m_media_object = m_src_html_element->GetLinkStyleMediaObject();
	if (m_media_object)
		return m_media_object->AddQueryLengths(this);
	else
		return OpStatus::OK;
}

CSS::CSS_RuleElmList::~CSS_RuleElmList()
{
	CSS_RuleElm* cur = m_first, *tmp;
	while (cur)
	{
		tmp = cur->Next();
		OP_DELETE(cur);
		cur = tmp;
	}
}

OP_STATUS CSS::CSS_RuleElmList::AddRule(CSS_BlockRule* rule, CSS_Selector* selector)
{
	CSS::CSS_RuleElm* new_elm = OP_NEW(CSS::CSS_RuleElm, (rule, selector));
	if (new_elm)
	{
		Prepend(new_elm);
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

OP_STATUS CSS::CSS_RuleElmList::InsertRule(CSS_BlockRule* rule, CSS_Selector* selector)
{
	CSS::CSS_RuleElm* new_elm = OP_NEW(CSS::CSS_RuleElm, (rule, selector));
	if (new_elm)
	{
		CSS::CSS_RuleElm* before = NULL;
		CSS::CSS_RuleElm* after = First();
		while (after && rule->GetRuleNumber() < after->GetRule()->GetRuleNumber())
		{
			before = after;
			after = after->Next();
		}
		if (after)
			new_elm->Precede(after);
		if (before)
			before->Precede(new_elm);
		else
			m_first = new_elm;
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

void CSS::CSS_RuleElmList::DeleteRule(CSS_Rule* rule)
{
	CSS_RuleElm* prev = NULL;
	CSS_RuleElm* elm = First();
	while (elm && elm->GetRule() != rule)
	{
		prev = elm;
		elm = elm->Next();
	}
	if (elm)
	{
		CSS_RuleElm* next;

		do {
			next = elm->Next();
			OP_DELETE(elm);
			elm = next;
		} while (elm && elm->GetRule() == rule);

		if (prev)
			prev->Precede(next);
		else
			m_first = next;
	}
}

CSS::CSS_RuleElm* CSS::RuleElmIterator::Next()
{
start:
	if (!m_cur_elm)
	{
		m_cur_elm = *m_lists++;
		m_cur_conditional = NULL;
	}

	CSS::CSS_RuleElm* ret = m_cur_elm;
	if (ret)
	{
		m_cur_elm = ret->Next();
		CSS_ConditionalRule* conditional_rule = ret->GetRule()->GetConditionalRule();
		if (conditional_rule != m_cur_conditional)
		{
			m_cur_conditional = conditional_rule;
			if (conditional_rule && !conditional_rule->EnterRule(m_doc, m_media))
			{
				// skip all rules nested in the conditinal rule
				while (m_cur_elm && m_cur_elm->GetRule()->GetConditionalRule() == conditional_rule)
					m_cur_elm = m_cur_elm->Next();
				goto start;
			}
		}
	}
	return ret;
}

#ifdef _DEBUG
Debug&
operator<<(Debug& d, const CSSProperty& property)
{
	TempBuffer buf;
	CSS::FormatCssPropertyNameL(property, &buf);
	d << buf.GetStorage();
	return d;
}
#endif
