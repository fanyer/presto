/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_STYLE_STYLE_MODULE_H
#define MODULES_STYLE_STYLE_MODULE_H

#include "modules/hardcore/opera/module.h"
#include "modules/util/tempbuf.h"

#ifndef HAS_COMPLEX_GLOBALS
#include "modules/style/src/css_properties.h"
#include "modules/style/src/css_values.h"
#include "modules/style/css_media.h"
#endif // HAS_COMPLEX_GLOBALS

// These values must match the number of PSEUDO_CLASS_* values in css.cpp.
#define PSEUDO_CLASS_COUNT 39

// Support the old but popular 20090723 version of the flexbox spec (with -webkit- prefixes only)
#define WEBKIT_OLD_FLEXBOX

class CSSManager;
class CSS_PseudoStack;
class CSS_MatchContext;

class StyleModule : public OperaModule
{
public:
	StyleModule() : css_manager(NULL), temp_buffer(NULL), css_pseudo_stack(NULL), match_context(NULL)
		{
			default_style.display_type = NULL;
			default_style.list_style_type = NULL;
			default_style.list_style_pos = NULL;
			default_style.marquee_dir = NULL;
			default_style.marquee_style = NULL;
			default_style.marquee_loop = NULL;
			default_style.overflow_x = NULL;
			default_style.overflow_y = NULL;
			default_style.white_space = NULL;
			default_style.line_height = NULL;
			default_style.overflow_wrap = NULL;
			default_style.resize = NULL;
			default_style.word_spacing = NULL;
			default_style.letter_spacing = NULL;
			default_style.margin_left = NULL;
			default_style.margin_right = NULL;
			default_style.infinite_decl = NULL;
			default_style.border_collapse = NULL;
			default_style.vertical_align = NULL;
			default_style.text_indent = NULL;
			default_style.text_decoration = NULL;
			default_style.text_align = NULL;
			default_style.caption_side = NULL;
			default_style.clear = NULL;
			default_style.unicode_bidi = NULL;
			default_style.direction = NULL;
			default_style.content_height = NULL;
			default_style.content_width = NULL;
			default_style.border_spacing = NULL;
			default_style.bg_image_proxy = NULL;
			default_style.bg_attach = NULL;
			default_style.bg_color = NULL;
			default_style.fg_color = NULL;
			default_style.writing_system = NULL;
			default_style.border_color = NULL;
			default_style.border_left_style = NULL;
			default_style.border_right_style = NULL;
			default_style.border_top_style = NULL;
			default_style.border_bottom_style = NULL;
			default_style.border_left_width = NULL;
			default_style.border_right_width = NULL;
			default_style.border_top_width = NULL;
			default_style.border_bottom_width = NULL;
			default_style.float_decl = NULL;
			default_style.font_color = NULL;
			default_style.font_style = NULL;
			default_style.font_size = NULL;
			default_style.font_size_type = NULL;
			default_style.font_weight = NULL;
			default_style.font_family = NULL;
			default_style.font_family_type = NULL;
			default_style.table_rules = NULL;
			default_style.text_transform = NULL;
			default_style.object_fit = NULL;
			default_style.quotes = NULL;
#ifdef DOM_FULLSCREEN_MODE
			default_style.fullscreen_fixed = NULL;
			default_style.fullscreen_top = NULL;
			default_style.fullscreen_left = NULL;
			default_style.fullscreen_right = NULL;
			default_style.fullscreen_bottom = NULL;
			default_style.fullscreen_zindex = NULL;
			default_style.fullscreen_box_sizing = NULL;
			default_style.fullscreen_margin_top = NULL;
			default_style.fullscreen_margin_left = NULL;
			default_style.fullscreen_margin_right = NULL;
			default_style.fullscreen_margin_bottom = NULL;
			default_style.fullscreen_width = NULL;
			default_style.fullscreen_height = NULL;
			default_style.fullscreen_overflow_x = NULL;
			default_style.fullscreen_overflow_y = NULL;
			default_style.fullscreen_zindex_auto = NULL;
			default_style.fullscreen_opacity = NULL;
			default_style.fullscreen_mask = NULL;
			default_style.fullscreen_clip = NULL;
			default_style.fullscreen_filter = NULL;
			default_style.fullscreen_transform = NULL;
			default_style.fullscreen_trans_prop = NULL;
			default_style.fullscreen_trans_delay = NULL;
			default_style.fullscreen_trans_duration = NULL;
			default_style.fullscreen_trans_timing = NULL;
#endif // DOM_FULLSCREEN_MODE

			for (int i=0; i<8; i++)
			{
				default_style.margin_padding[i] = NULL;
			}
	}

	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();

	CSSManager* css_manager;
	TempBuffer* temp_buffer;
	CSS_PseudoStack* css_pseudo_stack;
	CSS_MatchContext* match_context;

	/** Default stylesheet declarations. */
	struct DefaultStyle {
		CSS_type_decl* display_type;
		CSS_type_decl* list_style_type;
		CSS_type_decl* list_style_pos;
		CSS_type_decl* marquee_dir;
		CSS_type_decl* marquee_style;
		CSS_number_decl* marquee_loop;
		CSS_type_decl* overflow_x;
		CSS_type_decl* overflow_y;
		CSS_type_decl* white_space; // nowrap/normal/pre
		CSS_type_decl* line_height;
		CSS_type_decl* overflow_wrap;
		CSS_type_decl* resize;
		CSS_type_decl* word_spacing;
		CSS_type_decl* letter_spacing;
		CSS_type_decl* margin_left;
		CSS_type_decl* margin_right;
		CSS_type_decl* infinite_decl; // -wap-marquee-loop
		CSS_type_decl* border_collapse; // separate/collapse
		CSS_type_decl* vertical_align; // all possible values
		CSS_number_decl* text_indent;
		CSS_type_decl* text_decoration; // blink/underline/linethrough
		CSS_type_decl* text_align; // left/right/center/default
		CSS_type_decl* caption_side; // always bottom
		CSS_type_decl* clear; // left/right/both
		CSS_type_decl* unicode_bidi; // embed/bidi-override
		CSS_type_decl* direction; // rtl/ltr
		CSS_number_decl* content_height; // number, percentage is negative
		CSS_number_decl* content_width; // number, percentage is negative, Use CSS_NUMBER as unit for PRE width. Don't use GetLengthInPx, but set bit.
		CSS_number_decl* border_spacing; // number, used for both horizontal and vertical.
		CSS_proxy_decl* bg_image_proxy;
		CSS_gen_array_decl* bg_attach; // fixed
		CSS_long_decl* bg_color; // various
		CSS_long_decl* fg_color; // various
		CSS_long_decl* writing_system; // All Script enums.
		CSS_long_decl* border_color;
		CSS_type_decl* border_left_style;
		CSS_type_decl* border_right_style;
		CSS_type_decl* border_top_style;
		CSS_type_decl* border_bottom_style;
		CSS_number_decl* border_left_width;
		CSS_number_decl* border_right_width;
		CSS_number_decl* border_top_width;
		CSS_number_decl* border_bottom_width;
		CSS_type_decl* float_decl;
		CSS_long_decl* font_color;
		CSS_type_decl* font_style;
		CSS_number_decl* font_size;
		CSS_type_decl* font_size_type;
		CSS_number_decl* font_weight;
		CSS_heap_gen_array_decl* font_family;
		CSS_type_decl* font_family_type;
		CSS_long_decl* table_rules;
		CSS_type_decl* text_transform;
		CSS_type_decl* object_fit;
		CSS_gen_array_decl* quotes;
#ifdef DOM_FULLSCREEN_MODE
		CSS_type_decl* fullscreen_fixed;
		CSS_number_decl* fullscreen_top;
		CSS_number_decl* fullscreen_left;
		CSS_number_decl* fullscreen_right;
		CSS_number_decl* fullscreen_bottom;
		CSS_long_decl* fullscreen_zindex;
		CSS_type_decl* fullscreen_box_sizing;
		CSS_number_decl* fullscreen_margin_top;
		CSS_number_decl* fullscreen_margin_left;
		CSS_number_decl* fullscreen_margin_right;
		CSS_number_decl* fullscreen_margin_bottom;
		CSS_number_decl* fullscreen_width;
		CSS_number_decl* fullscreen_height;
		CSS_type_decl* fullscreen_overflow_x;
		CSS_type_decl* fullscreen_overflow_y;
		CSS_type_decl* fullscreen_zindex_auto;
		CSS_number_decl* fullscreen_opacity;
		CSS_type_decl* fullscreen_mask;
		CSS_type_decl* fullscreen_clip;
		CSS_type_decl* fullscreen_filter;
		CSS_type_decl* fullscreen_transform;
		CSS_type_decl* fullscreen_trans_prop;
		CSS_gen_array_decl* fullscreen_trans_delay;
		CSS_gen_array_decl* fullscreen_trans_duration;
		CSS_gen_array_decl* fullscreen_trans_timing;
#endif // DOM_FULLSCREEN_MODE

		CSS_number_decl* margin_padding[8];

		CSS_decl* GetProxyDeclaration(int property, CSS_decl* real_decl);
	};

	DefaultStyle default_style;

#ifndef HAS_COMPLEX_GLOBALS
	const char* css_property_name[CSS_PROPERTY_NAME_SIZE];
	const uni_char* css_value_name[CSS_VALUE_NAME_SIZE];
	const char* media_feature_name[MEDIA_FEATURE_COUNT];
	const char* css_pseudo_name[PSEUDO_CLASS_COUNT];
#endif
};

#define g_cssManager g_opera->style_module.css_manager
#define g_styleTempBuffer g_opera->style_module.temp_buffer
#define g_css_pseudo_stack g_opera->style_module.css_pseudo_stack
#define g_css_match_context g_opera->style_module.match_context
#ifndef HAS_COMPLEX_GLOBALS
# define g_css_property_name g_opera->style_module.css_property_name
# define g_css_value_name g_opera->style_module.css_value_name
# define g_media_feature_name g_opera->style_module.media_feature_name
# define g_css_pseudo_name g_opera->style_module.css_pseudo_name
#endif // HAS_COMPLEX_GLOBALS

#define STYLE_MODULE_REQUIRED

#endif // !MODULES_STYLE_STYLE_MODULE_H
