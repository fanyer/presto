/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/style/cssmanager.h"
#include "modules/style/src/css_pseudo_stack.h"
#include "modules/style/src/css_properties.h"
#include "modules/style/src/css_values.h"
#include "modules/style/css_matchcontext.h"

#ifndef HAS_COMPLEX_GLOBALS
extern void init_g_css_property_name();
extern void init_g_css_value_name();
extern void init_g_media_feature_name();
extern void init_g_css_pseudo_name();
#endif // HAS_COMPLEX_GLOBALS

#define MATCH_CONTEXT_ELM_POOL_SIZE 100

/* virtual */ void
StyleModule::InitL(const OperaInitInfo& info)
{
	CONST_ARRAY_INIT(g_css_property_name);
	CONST_ARRAY_INIT(g_css_value_name);
	CONST_ARRAY_INIT(g_media_feature_name);
	CONST_ARRAY_INIT(g_css_pseudo_name);
	temp_buffer = OP_NEW_L(TempBuffer, ());
	css_pseudo_stack = OP_NEW_L(CSS_PseudoStack, ());
	css_pseudo_stack->ConstructL();
	match_context = OP_NEW_L(CSS_MatchContext, ());
	match_context->ConstructL(MATCH_CONTEXT_ELM_POOL_SIZE);
	css_manager = OP_NEW_L(CSSManager, ());
	css_manager->ConstructL();
	css_manager->LoadLocalCSSL();

#define NEW_DEFAULT_STYLE(X, T, P) do { X = OP_NEW_L(T, P); X->Ref(); X->SetDefaultStyle(); } while(0)

	NEW_DEFAULT_STYLE(default_style.display_type,CSS_type_decl,(CSS_PROPERTY_display, CSS_VALUE_UNSPECIFIED));
	NEW_DEFAULT_STYLE(default_style.list_style_type,CSS_type_decl,(CSS_PROPERTY_list_style_type, CSS_VALUE_UNSPECIFIED));
	NEW_DEFAULT_STYLE(default_style.list_style_pos,CSS_type_decl,(CSS_PROPERTY_list_style_position, CSS_VALUE_UNSPECIFIED));
	NEW_DEFAULT_STYLE(default_style.marquee_dir,CSS_type_decl,(CSS_PROPERTY__wap_marquee_dir, CSS_VALUE_UNSPECIFIED));
	NEW_DEFAULT_STYLE(default_style.marquee_style,CSS_type_decl,(CSS_PROPERTY__wap_marquee_style, CSS_VALUE_UNSPECIFIED));
	NEW_DEFAULT_STYLE(default_style.marquee_loop,CSS_number_decl,(CSS_PROPERTY__wap_marquee_loop, 0, CSS_NUMBER));
	NEW_DEFAULT_STYLE(default_style.overflow_x,CSS_type_decl,(CSS_PROPERTY_overflow_x, CSS_VALUE_hidden));
	NEW_DEFAULT_STYLE(default_style.overflow_y,CSS_type_decl,(CSS_PROPERTY_overflow_y, CSS_VALUE_hidden));
	NEW_DEFAULT_STYLE(default_style.white_space,CSS_type_decl,(CSS_PROPERTY_white_space, CSS_VALUE_normal)); // nowrap/normal/pre
	NEW_DEFAULT_STYLE(default_style.line_height,CSS_type_decl,(CSS_PROPERTY_line_height, CSS_VALUE_normal));
	NEW_DEFAULT_STYLE(default_style.overflow_wrap,CSS_type_decl,(CSS_PROPERTY_overflow_wrap, CSS_VALUE_break_word));
	NEW_DEFAULT_STYLE(default_style.resize,CSS_type_decl,(CSS_PROPERTY_resize, CSS_VALUE_both));
	NEW_DEFAULT_STYLE(default_style.word_spacing,CSS_type_decl,(CSS_PROPERTY_word_spacing, CSS_VALUE_normal));
	NEW_DEFAULT_STYLE(default_style.letter_spacing,CSS_type_decl,(CSS_PROPERTY_letter_spacing, CSS_VALUE_normal));
	NEW_DEFAULT_STYLE(default_style.margin_left,CSS_type_decl,(CSS_PROPERTY_margin_left, CSS_VALUE_auto));
	NEW_DEFAULT_STYLE(default_style.margin_right,CSS_type_decl,(CSS_PROPERTY_margin_right, CSS_VALUE_auto));
	NEW_DEFAULT_STYLE(default_style.infinite_decl,CSS_type_decl,(CSS_PROPERTY__wap_marquee_loop, CSS_VALUE_infinite));
	NEW_DEFAULT_STYLE(default_style.border_collapse,CSS_type_decl,(CSS_PROPERTY_border_collapse, CSS_VALUE_separate)); // separate/collapse
	NEW_DEFAULT_STYLE(default_style.vertical_align,CSS_type_decl,(CSS_PROPERTY_vertical_align, CSS_VALUE_UNSPECIFIED)); // all possible values
	NEW_DEFAULT_STYLE(default_style.text_indent,CSS_number_decl,(CSS_PROPERTY_text_indent, 0, CSS_PX));
	NEW_DEFAULT_STYLE(default_style.text_decoration,CSS_type_decl,(CSS_PROPERTY_text_decoration, CSS_VALUE_UNSPECIFIED)); // blink/underline/linethrough
	NEW_DEFAULT_STYLE(default_style.text_align,CSS_type_decl,(CSS_PROPERTY_text_align, CSS_VALUE_UNSPECIFIED)); // left/right/center/default
	NEW_DEFAULT_STYLE(default_style.caption_side,CSS_type_decl,(CSS_PROPERTY_caption_side, CSS_VALUE_bottom)); // always bottom
	NEW_DEFAULT_STYLE(default_style.clear,CSS_type_decl,(CSS_PROPERTY_clear, CSS_VALUE_left)); // left/right/both
	NEW_DEFAULT_STYLE(default_style.unicode_bidi,CSS_type_decl,(CSS_PROPERTY_unicode_bidi, CSS_VALUE_embed)); // embed/bidi-override
	NEW_DEFAULT_STYLE(default_style.direction,CSS_type_decl,(CSS_PROPERTY_direction, CSS_VALUE_ltr)); // rtl/ltr
	NEW_DEFAULT_STYLE(default_style.content_height,CSS_number_decl,(CSS_PROPERTY_height, 0, CSS_PX)); // number, percentage is negative
	NEW_DEFAULT_STYLE(default_style.content_width,CSS_number_decl,(CSS_PROPERTY_width, 0, CSS_PX)); // number, percentage is negative, Use CSS_NUMBER as unit for PRE width. Don't use GetLengthInPx, but set bit.
	NEW_DEFAULT_STYLE(default_style.border_spacing,CSS_number_decl,(CSS_PROPERTY_border_spacing, 0, CSS_PX)); // number, used for both horizontal and vertical.
	NEW_DEFAULT_STYLE(default_style.bg_image_proxy,CSS_proxy_decl,(CSS_PROPERTY_background_image));
	NEW_DEFAULT_STYLE(default_style.bg_color,CSS_long_decl,(CSS_PROPERTY_background_color, USE_DEFAULT_COLOR)); // various
	NEW_DEFAULT_STYLE(default_style.fg_color,CSS_long_decl,(CSS_PROPERTY_color, USE_DEFAULT_COLOR)); // various
	NEW_DEFAULT_STYLE(default_style.writing_system,CSS_long_decl,(CSS_PROPERTY_writing_system, WritingSystem::Unknown)); // All Script enums.
	NEW_DEFAULT_STYLE(default_style.border_color,CSS_long_decl,(CSS_PROPERTY_border_color, USE_DEFAULT_COLOR));
	NEW_DEFAULT_STYLE(default_style.border_left_style,CSS_type_decl,(CSS_PROPERTY_border_left_style, CSS_VALUE_none));
	NEW_DEFAULT_STYLE(default_style.border_right_style,CSS_type_decl,(CSS_PROPERTY_border_right_style, CSS_VALUE_none));
	NEW_DEFAULT_STYLE(default_style.border_top_style,CSS_type_decl,(CSS_PROPERTY_border_top_style, CSS_VALUE_none));
	NEW_DEFAULT_STYLE(default_style.border_bottom_style,CSS_type_decl,(CSS_PROPERTY_border_bottom_style, CSS_VALUE_none));
	NEW_DEFAULT_STYLE(default_style.border_left_width,CSS_number_decl,(CSS_PROPERTY_border_left_width, 0, CSS_PX));
	NEW_DEFAULT_STYLE(default_style.border_right_width,CSS_number_decl,(CSS_PROPERTY_border_right_width, 0, CSS_PX));
	NEW_DEFAULT_STYLE(default_style.border_top_width,CSS_number_decl,(CSS_PROPERTY_border_top_width, 0, CSS_PX));
	NEW_DEFAULT_STYLE(default_style.border_bottom_width,CSS_number_decl,(CSS_PROPERTY_border_bottom_width, 0, CSS_PX));
	NEW_DEFAULT_STYLE(default_style.float_decl,CSS_type_decl,(CSS_PROPERTY_float, CSS_VALUE_left));
	NEW_DEFAULT_STYLE(default_style.font_color,CSS_long_decl,(CSS_PROPERTY_font_color, USE_DEFAULT_COLOR));
	NEW_DEFAULT_STYLE(default_style.font_style,CSS_type_decl,(CSS_PROPERTY_font_style, CSS_VALUE_normal));
	NEW_DEFAULT_STYLE(default_style.font_size,CSS_number_decl,(CSS_PROPERTY_font_size, 0, CSS_PX));
	NEW_DEFAULT_STYLE(default_style.font_size_type,CSS_type_decl,(CSS_PROPERTY_font_size, CSS_VALUE_none));
	NEW_DEFAULT_STYLE(default_style.font_weight,CSS_number_decl,(CSS_PROPERTY_font_weight, 4, CSS_NUMBER));
	NEW_DEFAULT_STYLE(default_style.table_rules,CSS_long_decl,(CSS_PROPERTY_table_rules, 0));
	NEW_DEFAULT_STYLE(default_style.text_transform,CSS_type_decl,(CSS_PROPERTY_text_transform, CSS_VALUE_none));
	NEW_DEFAULT_STYLE(default_style.object_fit,CSS_type_decl,(CSS_PROPERTY__o_object_fit, CSS_VALUE_contain));

	CSS_generic_value gen_val[4];

	uni_char quotes[4] = { '"', 0, '\'', 0 };

	gen_val[0].SetValueType(CSS_STRING_LITERAL);
	gen_val[0].SetString(&quotes[0]);
	gen_val[1].SetValueType(CSS_STRING_LITERAL);
	gen_val[1].SetString(&quotes[0]);
	gen_val[2].SetValueType(CSS_STRING_LITERAL);
	gen_val[2].SetString(&quotes[2]);
	gen_val[3].SetValueType(CSS_STRING_LITERAL);
	gen_val[3].SetString(&quotes[2]);
	LEAVE_IF_NULL(default_style.quotes = CSS_copy_gen_array_decl::Create(CSS_PROPERTY_quotes, gen_val, 4));
	default_style.quotes->Ref();
	default_style.quotes->SetDefaultStyle();

	gen_val[0].SetValueType(CSS_IDENT);
	gen_val[0].SetType(CSS_VALUE_fixed);
	LEAVE_IF_NULL(default_style.bg_attach = CSS_copy_gen_array_decl::Create(CSS_PROPERTY_background_attachment, gen_val, 1));
	default_style.bg_attach->SetLayerCount(1);
	default_style.bg_attach->Ref();
	default_style.bg_attach->SetDefaultStyle();

	short margin_padding_props[8] = {
		CSS_PROPERTY_margin_left,
		CSS_PROPERTY_margin_right,
		CSS_PROPERTY_margin_top,
		CSS_PROPERTY_margin_bottom,
		CSS_PROPERTY_padding_left,
		CSS_PROPERTY_padding_right,
		CSS_PROPERTY_padding_top,
		CSS_PROPERTY_padding_bottom
	};
	for (int i=0; i<8; i++)
		NEW_DEFAULT_STYLE(default_style.margin_padding[i],CSS_number_decl,(margin_padding_props[i], 0, CSS_PX));

	gen_val[0].SetValueType(CSS_IDENT);
	gen_val[0].SetType(CSS_VALUE_UNSPECIFIED);
	LEAVE_IF_NULL(default_style.font_family = CSS_copy_gen_array_decl::Create(CSS_PROPERTY_font_family, gen_val, 1));
	default_style.font_family->Ref();
	default_style.font_family->SetDefaultStyle();

	NEW_DEFAULT_STYLE(default_style.font_family_type,CSS_type_decl,(CSS_PROPERTY_font_family, CSS_VALUE_none));

#undef NEW_DEFAULT_STYLE

#ifdef DOM_FULLSCREEN_MODE
# define NEW_FULLSCREEN_STYLE(X, T, P) do { X = OP_NEW_L(T, P); X->Ref(); X->SetOrigin(CSS_decl::ORIGIN_USER_AGENT_FULLSCREEN); } while(0)
	NEW_FULLSCREEN_STYLE(default_style.fullscreen_fixed,CSS_type_decl,(CSS_PROPERTY_position, CSS_VALUE_fixed));
	NEW_FULLSCREEN_STYLE(default_style.fullscreen_top,CSS_number_decl,(CSS_PROPERTY_top, 0, CSS_PX));
	NEW_FULLSCREEN_STYLE(default_style.fullscreen_left,CSS_number_decl,(CSS_PROPERTY_left, 0, CSS_PX));
	NEW_FULLSCREEN_STYLE(default_style.fullscreen_right,CSS_number_decl,(CSS_PROPERTY_right, 0, CSS_PX));
	NEW_FULLSCREEN_STYLE(default_style.fullscreen_bottom,CSS_number_decl,(CSS_PROPERTY_bottom, 0, CSS_PX));
	NEW_FULLSCREEN_STYLE(default_style.fullscreen_zindex,CSS_long_decl,(CSS_PROPERTY_z_index, INT_MAX));
	NEW_FULLSCREEN_STYLE(default_style.fullscreen_box_sizing,CSS_type_decl,(CSS_PROPERTY_box_sizing, CSS_VALUE_border_box));
	NEW_FULLSCREEN_STYLE(default_style.fullscreen_margin_top,CSS_number_decl,(CSS_PROPERTY_margin_top, 0, CSS_PX));
	NEW_FULLSCREEN_STYLE(default_style.fullscreen_margin_left,CSS_number_decl,(CSS_PROPERTY_margin_left, 0, CSS_PX));
	NEW_FULLSCREEN_STYLE(default_style.fullscreen_margin_right,CSS_number_decl,(CSS_PROPERTY_margin_right, 0, CSS_PX));
	NEW_FULLSCREEN_STYLE(default_style.fullscreen_margin_bottom,CSS_number_decl,(CSS_PROPERTY_margin_bottom, 0, CSS_PX));
	NEW_FULLSCREEN_STYLE(default_style.fullscreen_width,CSS_number_decl,(CSS_PROPERTY_width, 100, CSS_PERCENTAGE));
	NEW_FULLSCREEN_STYLE(default_style.fullscreen_height,CSS_number_decl,(CSS_PROPERTY_height, 100, CSS_PERCENTAGE));
	NEW_FULLSCREEN_STYLE(default_style.fullscreen_overflow_x,CSS_type_decl,(CSS_PROPERTY_overflow_x, CSS_VALUE_hidden));
	NEW_FULLSCREEN_STYLE(default_style.fullscreen_overflow_y,CSS_type_decl,(CSS_PROPERTY_overflow_y, CSS_VALUE_hidden));
	NEW_FULLSCREEN_STYLE(default_style.fullscreen_zindex_auto,CSS_type_decl,(CSS_PROPERTY_z_index, CSS_VALUE_auto));
	NEW_FULLSCREEN_STYLE(default_style.fullscreen_opacity,CSS_number_decl,(CSS_PROPERTY_opacity, 1.0f, CSS_NUMBER));
	NEW_FULLSCREEN_STYLE(default_style.fullscreen_mask,CSS_type_decl,(CSS_PROPERTY_mask, CSS_VALUE_none));
	NEW_FULLSCREEN_STYLE(default_style.fullscreen_clip,CSS_type_decl,(CSS_PROPERTY_clip, CSS_VALUE_none));
	NEW_FULLSCREEN_STYLE(default_style.fullscreen_filter,CSS_type_decl,(CSS_PROPERTY_filter, CSS_VALUE_none));
	NEW_FULLSCREEN_STYLE(default_style.fullscreen_transform,CSS_type_decl,(CSS_PROPERTY_transform, CSS_VALUE_none));
	NEW_FULLSCREEN_STYLE(default_style.fullscreen_trans_prop,CSS_type_decl,(CSS_PROPERTY_transition_property, CSS_VALUE_none));

	gen_val[0].SetValueType(CSS_SECOND);
	gen_val[0].SetReal(0.0f);
	LEAVE_IF_NULL(default_style.fullscreen_trans_delay = CSS_copy_gen_array_decl::Create(CSS_PROPERTY_transition_delay, gen_val, 1));
	default_style.fullscreen_trans_delay->Ref();
	default_style.fullscreen_trans_delay->SetOrigin(CSS_decl::ORIGIN_USER_AGENT_FULLSCREEN);
	LEAVE_IF_NULL(default_style.fullscreen_trans_duration = CSS_copy_gen_array_decl::Create(CSS_PROPERTY_transition_duration, gen_val, 1));
	default_style.fullscreen_trans_duration->Ref();
	default_style.fullscreen_trans_duration->SetOrigin(CSS_decl::ORIGIN_USER_AGENT_FULLSCREEN);

	gen_val[0].SetValueType(CSS_NUMBER);
	gen_val[0].SetReal(0.25f);
	gen_val[1].SetValueType(CSS_NUMBER);
	gen_val[1].SetReal(0.1f);
	gen_val[2].SetValueType(CSS_NUMBER);
	gen_val[2].SetReal(0.25f);
	gen_val[3].SetValueType(CSS_NUMBER);
	gen_val[3].SetReal(1.0f);
	LEAVE_IF_NULL(default_style.fullscreen_trans_timing = CSS_copy_gen_array_decl::Create(CSS_PROPERTY_transition_timing_function, gen_val, 4));
	default_style.fullscreen_trans_timing->Ref();
	default_style.fullscreen_trans_timing->SetOrigin(CSS_decl::ORIGIN_USER_AGENT_FULLSCREEN);
# undef NEW_FULLSCREEN_STYLE
#endif // DOM_FULLSCREEN_MODE
}

/* virtual */ void
StyleModule::Destroy()
{
	OP_DELETE(css_manager);
	css_manager = NULL;
	OP_DELETE(temp_buffer);
	temp_buffer = NULL;
	OP_DELETE(css_pseudo_stack);
	css_pseudo_stack = NULL;
	OP_DELETE(match_context);
	match_context = NULL;

	// Delete default stylesheet declarations
	CSS_decl::Unref(default_style.display_type);
	CSS_decl::Unref(default_style.list_style_type);
	CSS_decl::Unref(default_style.list_style_pos);
	CSS_decl::Unref(default_style.marquee_dir);
	CSS_decl::Unref(default_style.marquee_style);
	CSS_decl::Unref(default_style.marquee_loop);
	CSS_decl::Unref(default_style.overflow_x);
	CSS_decl::Unref(default_style.overflow_y);
	CSS_decl::Unref(default_style.white_space);
	CSS_decl::Unref(default_style.line_height);
	CSS_decl::Unref(default_style.overflow_wrap);
	CSS_decl::Unref(default_style.resize);
	CSS_decl::Unref(default_style.word_spacing);
	CSS_decl::Unref(default_style.letter_spacing);
	CSS_decl::Unref(default_style.margin_left);
	CSS_decl::Unref(default_style.margin_right);
	CSS_decl::Unref(default_style.infinite_decl);
	CSS_decl::Unref(default_style.border_collapse);
	CSS_decl::Unref(default_style.vertical_align);
	CSS_decl::Unref(default_style.text_indent);
	CSS_decl::Unref(default_style.text_decoration);
	CSS_decl::Unref(default_style.text_align);
	CSS_decl::Unref(default_style.caption_side);
	CSS_decl::Unref(default_style.clear);
	CSS_decl::Unref(default_style.unicode_bidi);
	CSS_decl::Unref(default_style.direction);
	CSS_decl::Unref(default_style.content_height);
	CSS_decl::Unref(default_style.content_width);
	CSS_decl::Unref(default_style.border_spacing);
	CSS_decl::Unref(default_style.bg_image_proxy);
	CSS_decl::Unref(default_style.bg_attach);
	CSS_decl::Unref(default_style.bg_color);
	CSS_decl::Unref(default_style.fg_color);
	CSS_decl::Unref(default_style.writing_system);
	CSS_decl::Unref(default_style.border_color);
	CSS_decl::Unref(default_style.border_left_style);
	CSS_decl::Unref(default_style.border_right_style);
	CSS_decl::Unref(default_style.border_top_style);
	CSS_decl::Unref(default_style.border_bottom_style);
	CSS_decl::Unref(default_style.border_left_width);
	CSS_decl::Unref(default_style.border_right_width);
	CSS_decl::Unref(default_style.border_top_width);
	CSS_decl::Unref(default_style.border_bottom_width);
	CSS_decl::Unref(default_style.float_decl);
	CSS_decl::Unref(default_style.font_color);
	CSS_decl::Unref(default_style.font_style);
	CSS_decl::Unref(default_style.font_size);
	CSS_decl::Unref(default_style.font_size_type);
	CSS_decl::Unref(default_style.font_weight);
	CSS_decl::Unref(default_style.font_family);
	CSS_decl::Unref(default_style.font_family_type);
	CSS_decl::Unref(default_style.table_rules);
	CSS_decl::Unref(default_style.text_transform);
	CSS_decl::Unref(default_style.object_fit);
	CSS_decl::Unref(default_style.quotes);
#ifdef DOM_FULLSCREEN_MODE
	CSS_decl::Unref(default_style.fullscreen_fixed);
	CSS_decl::Unref(default_style.fullscreen_top);
	CSS_decl::Unref(default_style.fullscreen_left);
	CSS_decl::Unref(default_style.fullscreen_right);
	CSS_decl::Unref(default_style.fullscreen_bottom);
	CSS_decl::Unref(default_style.fullscreen_zindex);
	CSS_decl::Unref(default_style.fullscreen_box_sizing);
	CSS_decl::Unref(default_style.fullscreen_margin_top);
	CSS_decl::Unref(default_style.fullscreen_margin_left);
	CSS_decl::Unref(default_style.fullscreen_margin_right);
	CSS_decl::Unref(default_style.fullscreen_margin_bottom);
	CSS_decl::Unref(default_style.fullscreen_width);
	CSS_decl::Unref(default_style.fullscreen_height);
	CSS_decl::Unref(default_style.fullscreen_overflow_x);
	CSS_decl::Unref(default_style.fullscreen_overflow_y);
	CSS_decl::Unref(default_style.fullscreen_zindex_auto);
	CSS_decl::Unref(default_style.fullscreen_opacity);
	CSS_decl::Unref(default_style.fullscreen_mask);
	CSS_decl::Unref(default_style.fullscreen_clip);
	CSS_decl::Unref(default_style.fullscreen_filter);
	CSS_decl::Unref(default_style.fullscreen_transform);
	CSS_decl::Unref(default_style.fullscreen_trans_prop);
	CSS_decl::Unref(default_style.fullscreen_trans_delay);
	CSS_decl::Unref(default_style.fullscreen_trans_duration);
	CSS_decl::Unref(default_style.fullscreen_trans_timing);
#endif // DOM_FULLSCREEN_MODE

	for (int i=0; i<8; i++)
		CSS_decl::Unref(default_style.margin_padding[i]);
}

CSS_decl*
StyleModule::DefaultStyle::GetProxyDeclaration(int property, CSS_decl* real_decl)
{
	CSS_proxy_decl* proxy_decl = NULL;
	if (property == CSS_PROPERTY_background_image)
		proxy_decl = bg_image_proxy;

	OP_ASSERT(proxy_decl); // Only to be called on properties we know we need proxy objects for

	if (proxy_decl)
		proxy_decl->UpdateRealDecl(real_decl);

	return proxy_decl;
}
