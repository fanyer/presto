/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef CSSPROPS_H
#define CSSPROPS_H

#include "modules/hardcore/mem/mem_man.h"

#include "modules/display/cursor.h"
#include "modules/layout/layout.h"
#include "modules/logdoc/htm_lex.h"
#include "modules/windowcommander/WritingSystem.h"

#include "modules/style/css_types.h"

class CSS_decl;
class HTML_Element;
class HLDocProfile;
class VisualDevice;

#define CP_LEFT		0
#define CP_RIGHT	1
#define CP_TOP		2
#define CP_BOTTOM	3

#define CP_BORDER_LEFT_WIDTH	0
#define CP_BORDER_RIGHT_WIDTH	1
#define CP_BORDER_TOP_WIDTH		2
#define CP_BORDER_BOTTOM_WIDTH	3

#define CP_MARGIN_LEFT		0
#define CP_MARGIN_RIGHT		1
#define CP_MARGIN_TOP		2
#define CP_MARGIN_BOTTOM	3
#define CP_PADDING_LEFT		4
#define CP_PADDING_RIGHT	5
#define CP_PADDING_TOP		6
#define CP_PADDING_BOTTOM	7

#define CP_WIDTH		0
#define CP_HEIGHT		1

#define CP_MIN_WIDTH		0
#define CP_MIN_HEIGHT		1
#define CP_MAX_WIDTH		2
#define CP_MAX_HEIGHT		3

#define CP_LETTER_SPACING	0
#define CP_WORD_SPACING		1

#define CP_LINEHEIGHT		0
#define CP_VERTICAL_ALIGN	1

/* Max 256 entries to fit in CssPropertyItem. */
enum CssPropertiesItemType
{
	CSSPROPS_UNUSED = 0,
	CSSPROPS_COMMON, // text-dec, text-align, text-transf, float, display, clear,
					 // visibility, white-space, position
	CSSPROPS_FONT,
	CSSPROPS_FONT_NUMBER,
	CSSPROPS_COLOR,
	CSSPROPS_LEFT_RIGHT,
	CSSPROPS_TOP_BOTTOM,
	CSSPROPS_LEFT,
	CSSPROPS_RIGHT,
	CSSPROPS_TOP,
	CSSPROPS_BOTTOM,
	CSSPROPS_ZINDEX,
	CSSPROPS_LINE_HEIGHT_VALIGN,
	CSSPROPS_MARGIN,
	CSSPROPS_MARGIN_TOP_BOTTOM,
	CSSPROPS_MARGIN_LEFT_RIGHT,
	CSSPROPS_MARGIN_TOP,
	CSSPROPS_MARGIN_BOTTOM,
	CSSPROPS_MARGIN_LEFT,
	CSSPROPS_MARGIN_RIGHT,
	CSSPROPS_PADDING,
	CSSPROPS_PADDING_TOP_BOTTOM,
	CSSPROPS_PADDING_LEFT_RIGHT,
	CSSPROPS_PADDING_TOP,
	CSSPROPS_PADDING_BOTTOM,
	CSSPROPS_PADDING_LEFT,
	CSSPROPS_PADDING_RIGHT,
	CSSPROPS_BORDER_WIDTH,
	CSSPROPS_BORDER_LEFT_RIGHT_WIDTH,
	CSSPROPS_BORDER_TOP_BOTTOM_WIDTH,
	CSSPROPS_BORDER_TOP_WIDTH,
	CSSPROPS_BORDER_BOTTOM_WIDTH,
	CSSPROPS_BORDER_LEFT_WIDTH,
	CSSPROPS_BORDER_RIGHT_WIDTH,
	CSSPROPS_BORDER_COLOR,
	CSSPROPS_BORDER_LEFT_COLOR,
	CSSPROPS_BORDER_RIGHT_COLOR,
	CSSPROPS_BORDER_TOP_COLOR,
	CSSPROPS_BORDER_BOTTOM_COLOR,
	CSSPROPS_BORDER_STYLE,
	CSSPROPS_BORDER_LEFT_STYLE,
	CSSPROPS_BORDER_RIGHT_STYLE,
	CSSPROPS_BORDER_TOP_STYLE,
	CSSPROPS_BORDER_BOTTOM_STYLE,

	CSSPROPS_BORDER_TOP_RIGHT_RADIUS,
	CSSPROPS_BORDER_BOTTOM_RIGHT_RADIUS,
	CSSPROPS_BORDER_BOTTOM_LEFT_RADIUS,
	CSSPROPS_BORDER_TOP_LEFT_RADIUS, /* These four must be kept in order and contained so the range can be tested. */

	CSSPROPS_OUTLINE_WIDTH_OFFSET,
	CSSPROPS_OUTLINE_COLOR,
	CSSPROPS_OUTLINE_STYLE,
	CSSPROPS_CURSOR,
	CSSPROPS_COLUMN_WIDTH,
	CSSPROPS_COLUMN_COUNT,
	CSSPROPS_COLUMN_GAP,
	CSSPROPS_COLUMN_RULE_WIDTH,
	CSSPROPS_COLUMN_RULE_COLOR,
	CSSPROPS_COLUMN_RULE_STYLE,
	CSSPROPS_MULTI_PANE_PROPS,
	CSSPROPS_BORDER_SPACING,
	CSSPROPS_BORDER_IMAGE,
	CSSPROPS_TEXT_INDENT,
	CSSPROPS_WIDTH,
	CSSPROPS_HEIGHT,
	CSSPROPS_WIDTH_HEIGHT,
	CSSPROPS_MIN_WIDTH,
	CSSPROPS_MIN_HEIGHT,
	CSSPROPS_MIN_WIDTH_HEIGHT,
	CSSPROPS_MAX_WIDTH,
	CSSPROPS_MAX_HEIGHT,
	CSSPROPS_MAX_WIDTH_HEIGHT,
	CSSPROPS_LIST_STYLE_IMAGE,
	CSSPROPS_LETTER_WORD_SPACING,
	CSSPROPS_OTHER, /* table_layout, caption_side, border_collapse,
					   empty_cells, direction, unicode-bidi, list-style-pos,
					   list-style-type, text-overflow, -o-background-size (round) */
	CSSPROPS_BG_IMAGE,
	CSSPROPS_BG_POSITION,
	CSSPROPS_BG_COLOR,
	CSSPROPS_BG_ATTACH,
	CSSPROPS_BG_REPEAT,
	CSSPROPS_BG_ORIGIN,
	CSSPROPS_BG_CLIP,
	CSSPROPS_BG_SIZE,
	CSSPROPS_OBJECT_POSITION,
	CSSPROPS_PAGE,
	CSSPROPS_PAGE_IDENTIFIER,
	CSSPROPS_CLIP,
	CSSPROPS_CONTENT,
	CSSPROPS_COUNTER_RESET,
	CSSPROPS_COUNTER_INCREMENT,
	CSSPROPS_QUOTES,
	CSSPROPS_SET_LINK_SOURCE,
	CSSPROPS_USE_LINK_SOURCE,
	CSSPROPS_WAP_MARQUEE,
	CSSPROPS_IMAGE_RENDERING,
#ifdef SVG_SUPPORT
	CSSPROPS_FILL,
	CSSPROPS_STROKE,
	CSSPROPS_FILL_OPACITY,
	CSSPROPS_STROKE_OPACITY,
	CSSPROPS_FILL_RULE,
	CSSPROPS_STROKE_DASHOFFSET,
	CSSPROPS_STROKE_DASHARRAY,
	CSSPROPS_STROKE_MITERLIMIT,
	CSSPROPS_STROKE_WIDTH,
	CSSPROPS_STROKE_LINECAP,
	CSSPROPS_STROKE_LINEJOIN,
	CSSPROPS_TEXT_ANCHOR,
	CSSPROPS_STOP_COLOR,
	CSSPROPS_STOP_OPACITY,
	CSSPROPS_FLOOD_COLOR,
	CSSPROPS_FLOOD_OPACITY,
	CSSPROPS_CLIP_RULE,
	CSSPROPS_CLIP_PATH,
	CSSPROPS_LIGHTING_COLOR,
	CSSPROPS_MASK,
	CSSPROPS_ENABLE_BACKGROUND,
	CSSPROPS_FILTER,
	CSSPROPS_POINTER_EVENTS,
	CSSPROPS_ALIGNMENT_BASELINE,
	CSSPROPS_BASELINE_SHIFT,
	CSSPROPS_DOMINANT_BASELINE,
	CSSPROPS_WRITING_MODE,
	CSSPROPS_COLOR_INTERPOLATION,
	CSSPROPS_COLOR_INTERPOLATION_FILTERS,
	CSSPROPS_COLOR_PROFILE,
	CSSPROPS_COLOR_RENDERING,
	CSSPROPS_MARKER,
	CSSPROPS_MARKER_END,
	CSSPROPS_MARKER_MID,
	CSSPROPS_MARKER_START,
	CSSPROPS_SHAPE_RENDERING,
	CSSPROPS_TEXT_RENDERING,
	CSSPROPS_GLYPH_ORIENTATION_VERTICAL,
	CSSPROPS_GLYPH_ORIENTATION_HORIZONTAL,
	CSSPROPS_KERNING,
	CSSPROPS_VECTOR_EFFECT,
	CSSPROPS_AUDIO_LEVEL,
	CSSPROPS_SOLID_COLOR,
	CSSPROPS_DISPLAY_ALIGN,
	CSSPROPS_SOLID_OPACITY,
	CSSPROPS_VIEWPORT_FILL,
	CSSPROPS_LINE_INCREMENT,
	CSSPROPS_VIEWPORT_FILL_OPACITY,
	CSSPROPS_BUFFERED_RENDERING,
#endif // SVG_SUPPORT
#ifdef GADGET_SUPPORT
    CSSPROPS_CONTROL_REGION,
#endif // GADGET_SUPPORT
	CSSPROPS_TEXT_SHADOW,
	CSSPROPS_BOX_SHADOW,
	CSSPROPS_NAVUP,
	CSSPROPS_NAVDOWN,
	CSSPROPS_NAVLEFT,
	CSSPROPS_NAVRIGHT,
	CSSPROPS_TABLE,
	CSSPROPS_WRITING_SYSTEM,
	CSSPROPS_ACCESSKEY,
	CSSPROPS_SEL_COLOR,
	CSSPROPS_SEL_BGCOLOR,
	CSSPROPS_OTHER2,
	CSSPROPS_TAB_SIZE,
	CSSPROPS_FLEX_GROW,
	CSSPROPS_FLEX_SHRINK,
	CSSPROPS_FLEX_BASIS,
	CSSPROPS_FLEX_MODES,
	CSSPROPS_ORDER,
#ifdef CSS_TRANSFORMS
	CSSPROPS_TRANSFORM,
	CSSPROPS_TRANSFORM_ORIGIN,
#endif
#ifdef CSS_TRANSITIONS
	CSSPROPS_TRANS_DELAY,
	CSSPROPS_TRANS_DURATION,
	CSSPROPS_TRANS_TIMING,
	CSSPROPS_TRANS_PROPERTY,
#endif // CSS_TRANSITIONS
#ifdef CSS_ANIMATIONS
	CSSPROPS_ANIMATION_NAME,
	CSSPROPS_ANIMATION_DURATION,
	CSSPROPS_ANIMATION_TIMING_FUNCTION,
	CSSPROPS_ANIMATION_ITERATION_COUNT,
	CSSPROPS_ANIMATION_DIRECTION,
	CSSPROPS_ANIMATION_PLAY_STATE,
	CSSPROPS_ANIMATION_DELAY,
	CSSPROPS_ANIMATION_FILL_MODE,
#endif // CSS_ANIMATIONS
#ifdef CSS_MINI_EXTENSIONS
	CSSPROPS_MINI,
#endif // CSS_MINI_EXTENSIONS
	CSSPROPS_ITEM_COUNT
};

// Ugly hack: logdoc uses CSSPROPS_BG_ATTACH_REPEAT_COLOR to check if we have a background color set.
#define CSSPROPS_BG_ATTACH_REPEAT_COLOR CSSPROPS_BG_COLOR

#define CSS_VERTICAL_ALIGN_baseline		0
#define CSS_VERTICAL_ALIGN_top			1
#define CSS_VERTICAL_ALIGN_bottom		2
#define CSS_VERTICAL_ALIGN_sub			3
#define CSS_VERTICAL_ALIGN_super		4
#define CSS_VERTICAL_ALIGN_text_top		5
#define CSS_VERTICAL_ALIGN_middle		6
#define CSS_VERTICAL_ALIGN_text_bottom	7
#define CSS_VERTICAL_ALIGN_inherit		8

// values for CommonProps;info.text_align:3
#define CSS_TEXT_ALIGN_left		0
#define CSS_TEXT_ALIGN_right	1
#define CSS_TEXT_ALIGN_justify	2
#define CSS_TEXT_ALIGN_center	3
#define CSS_TEXT_ALIGN_inherit	4
#define CSS_TEXT_ALIGN_default	5
#define CSS_TEXT_ALIGN_value_not_set	6

// values for CommonProps:info.float_type:4
#define CSS_FLOAT_none						0
#define CSS_FLOAT_left						1
#define CSS_FLOAT_right						2
#define CSS_FLOAT_top						3
#define CSS_FLOAT_bottom					4
#define CSS_FLOAT_top_corner				5
#define CSS_FLOAT_bottom_corner				6
#define CSS_FLOAT_top_next_page				7
#define CSS_FLOAT_bottom_next_page			8
#define CSS_FLOAT_top_corner_next_page		9
#define CSS_FLOAT_bottom_corner_next_page	10
#define CSS_FLOAT_inherit					11
#define CSS_FLOAT_value_not_set				12

// values for CommonProps:info.display_type:5
#define CSS_DISPLAY_none				0
#define CSS_DISPLAY_inline				1
#define CSS_DISPLAY_block				2
#define CSS_DISPLAY_inline_block		3
#define CSS_DISPLAY_list_item			4
#define CSS_DISPLAY_run_in				5
#define CSS_DISPLAY_compact				6
#define CSS_DISPLAY_marker				7
#define CSS_DISPLAY_table				8
#define CSS_DISPLAY_inline_table		9
#define CSS_DISPLAY_table_row_group		10
#define CSS_DISPLAY_table_header_group	11
#define CSS_DISPLAY_table_footer_group	12
#define CSS_DISPLAY_table_row			13
#define CSS_DISPLAY_table_column_group	14
#define CSS_DISPLAY_table_column		15
#define CSS_DISPLAY_table_cell			16
#define CSS_DISPLAY_table_caption		17
#define CSS_DISPLAY_wap_marquee			18
#define CSS_DISPLAY_flex				19
#define CSS_DISPLAY_inline_flex			20
#define CSS_DISPLAY__webkit_box			21
#define CSS_DISPLAY__webkit_inline_box	22
#define CSS_DISPLAY_inherit				23
#define CSS_DISPLAY_value_not_set		24

// values for CommonProps:info.visibility:2
#define CSS_VISIBILITY_visible	0
#define CSS_VISIBILITY_hidden	1
#define CSS_VISIBILITY_collapse	2
#define CSS_VISIBILITY_inherit	3
#define CSS_VISIBILITY_value_not_set	4

// values for CommonProps:info.white_space:3
#define CSS_WHITE_SPACE_normal		0
#define CSS_WHITE_SPACE_pre			1
#define CSS_WHITE_SPACE_nowrap		2
#define CSS_WHITE_SPACE_pre_wrap	3
#define CSS_WHITE_SPACE_inherit		4
#define CSS_WHITE_SPACE_value_not_set	5
#define CSS_WHITE_SPACE_pre_line	6

// values for CommonProps:info.clear:3
#define CSS_CLEAR_none		0
#define CSS_CLEAR_left		1
#define CSS_CLEAR_right		2
#define CSS_CLEAR_both		3
#define CSS_CLEAR_inherit	4
#define CSS_CLEAR_value_not_set	5

// values for OtherProps2:info.{overflowx,overflowy}:4
#define CSS_OVERFLOW_visible 0
#define CSS_OVERFLOW_hidden 1
#define CSS_OVERFLOW_scroll 2
#define CSS_OVERFLOW_auto 3
#define CSS_OVERFLOW_paged_x_controls 4
#define CSS_OVERFLOW_paged_x 5
#define CSS_OVERFLOW_paged_y_controls 6
#define CSS_OVERFLOW_paged_y 7
#define CSS_OVERFLOW_inherit 8
#define CSS_OVERFLOW_value_not_set 9

// values for CommonProps:info.position:3
#define CSS_POSITION_static		0
#define CSS_POSITION_relative	1
#define CSS_POSITION_absolute	2
#define CSS_POSITION_fixed		3
#define CSS_POSITION_inherit	4
#define CSS_POSITION_value_not_set  5

// values for OtherProps:info.border_collapse:2
#define CSS_BORDER_COLLAPSE_collapse	0
#define CSS_BORDER_COLLAPSE_separate	1
#define CSS_BORDER_COLLAPSE_inherit		2
#define CSS_BORDER_COLLAPSE_value_not_set 3

// values for OtherProps:info.text_transform:3
#define CSS_TEXT_TRANSFORM_none			0
#define CSS_TEXT_TRANSFORM_capitalize	1
#define CSS_TEXT_TRANSFORM_uppercase	2
#define CSS_TEXT_TRANSFORM_lowercase	3
#define CSS_TEXT_TRANSFORM_inherit		4
#define CSS_TEXT_TRANSFORM_value_not_set	5

// values for OtherProps:info.table_layout:2
#define CSS_TABLE_LAYOUT_auto		0
#define CSS_TABLE_LAYOUT_fixed		1
#define CSS_TABLE_LAYOUT_inherit	2

// values for OtherProps:info.caption_side:3
#define CSS_CAPTION_SIDE_top		0
#define CSS_CAPTION_SIDE_bottom		1
#define CSS_CAPTION_SIDE_left		2
#define CSS_CAPTION_SIDE_right		3
#define CSS_CAPTION_SIDE_inherit	4
#define CSS_CAPTION_SIDE_value_not_set	5

// values for OtherProps:info.empty_cell:2
#define CSS_EMPTY_CELLS_show		0
#define CSS_EMPTY_CELLS_hide		1
#define CSS_EMPTY_CELLS_inherit		2

// values for OtherProps:info.resize:3
#define CSS_RESIZE_none				0
#define CSS_RESIZE_both				1
#define CSS_RESIZE_horizontal		2
#define CSS_RESIZE_vertical			3
#define CSS_RESIZE_inherit			4
#define CSS_RESIZE_value_not_set	5

#ifdef SUPPORT_TEXT_DIRECTION

// values for OtherProps:info.direction:2
#define CSS_DIRECTION_ltr			0
#define CSS_DIRECTION_rtl			1
#define CSS_DIRECTION_inherit		2
#define CSS_DIRECTION_value_not_set	3

// values for OtherProps:info.unicode_bidi:2
#define CSS_UNICODE_BIDI_normal			0
#define CSS_UNICODE_BIDI_embed			1
#define CSS_UNICODE_BIDI_bidi_override	2
#define CSS_UNICODE_BIDI_inherit		3
#define CSS_UNICODE_BIDI_value_not_set	4

#endif

// values for OtherProps:info.list_style_pos:2
#define CSS_LIST_STYLE_POS_inside			0
#define CSS_LIST_STYLE_POS_outside			1
#define CSS_LIST_STYLE_POS_inherit			2
#define CSS_LIST_STYLE_POS_value_not_set	3

// values for OtherProps:info.list_style_type:5
#define CSS_LIST_STYLE_TYPE_inherit					0
#define CSS_LIST_STYLE_TYPE_disc					1
#define CSS_LIST_STYLE_TYPE_circle					2
#define CSS_LIST_STYLE_TYPE_square					3
#define CSS_LIST_STYLE_TYPE_box						4
#define CSS_LIST_STYLE_TYPE_decimal					5
#define CSS_LIST_STYLE_TYPE_decimal_leading_zero	6
#define CSS_LIST_STYLE_TYPE_lower_roman				7
#define CSS_LIST_STYLE_TYPE_upper_roman				8
#define CSS_LIST_STYLE_TYPE_lower_greek				9
#define CSS_LIST_STYLE_TYPE_lower_alpha				10
#define CSS_LIST_STYLE_TYPE_lower_latin				11
#define CSS_LIST_STYLE_TYPE_upper_alpha				12
#define CSS_LIST_STYLE_TYPE_upper_latin				13
#define CSS_LIST_STYLE_TYPE_hebrew					14
#define CSS_LIST_STYLE_TYPE_armenian				15
#define CSS_LIST_STYLE_TYPE_georgian				16
#define CSS_LIST_STYLE_TYPE_cjk_ideographic			17
#define CSS_LIST_STYLE_TYPE_hiragana				18
#define CSS_LIST_STYLE_TYPE_katakana				19
#define CSS_LIST_STYLE_TYPE_hiragana_iroha			20
#define CSS_LIST_STYLE_TYPE_katakana_iroha			21
#define CSS_LIST_STYLE_TYPE_none					22
#define CSS_LIST_STYLE_TYPE_value_not_set			23

// values for OtherProps:info.box_sizing:2
#define CSS_BOX_SIZING_content_box			0
#define CSS_BOX_SIZING_border_box			1
#define CSS_BOX_SIZING_inherit				2
#define CSS_BOX_SIZING_value_not_set		3

// values for OtherProps:info.text_overflow:2
#define CSS_TEXT_OVERFLOW_clip				0
#define CSS_TEXT_OVERFLOW_ellipsis			1
#define CSS_TEXT_OVERFLOW_ellipsis_lastline	2
#define CSS_TEXT_OVERFLOW_inherit			3

// values for OtherProps:info.overflow_wrap:2
#define CSS_OVERFLOW_WRAP_normal				0
#define CSS_OVERFLOW_WRAP_break_word			1
#define CSS_OVERFLOW_WRAP_inherit				2
#define CSS_OVERFLOW_WRAP_value_not_set			3

// values for OtherProps:info.object_fit:3
#define CSS_OBJECT_FIT_fill			0
#define CSS_OBJECT_FIT_contain		1
#define CSS_OBJECT_FIT_cover			2
#define CSS_OBJECT_FIT_none			3
#define CSS_OBJECT_FIT_auto			4
#define CSS_OBJECT_FIT_inherit		5
#define CSS_OBJECT_FIT_value_not_set	6

// values for FontProps:info.weight:4
#define CSS_FONT_WEIGHT_inherit	0
#define CSS_FONT_WEIGHT_100		1
#define CSS_FONT_WEIGHT_200		2
#define CSS_FONT_WEIGHT_300		3
#define CSS_FONT_WEIGHT_400		4
#define CSS_FONT_WEIGHT_500		5
#define CSS_FONT_WEIGHT_600		6
#define CSS_FONT_WEIGHT_700		7
#define CSS_FONT_WEIGHT_800		8
#define CSS_FONT_WEIGHT_900		9
#define CSS_FONT_WEIGHT_bolder	10
#define CSS_FONT_WEIGHT_lighter	11
#define CSS_FONT_WEIGHT_bold	12
#define CSS_FONT_WEIGHT_normal	13
#define CSS_FONT_WEIGHT_value_not_set	14

// values for FontProps:info.style:3
#define CSS_FONT_STYLE_inherit	0
#define CSS_FONT_STYLE_normal	1
#define CSS_FONT_STYLE_italic	2
#define CSS_FONT_STYLE_oblique	3
#define CSS_FONT_STYLE_value_not_set	4

// values for FontProps:info.variant:2
#define CSS_FONT_VARIANT_inherit	0
#define CSS_FONT_VARIANT_normal		1
#define CSS_FONT_VARIANT_small_caps	2
#define CSS_FONT_VARIANT_value_not_set 3

// values for FontProps:info.size:16
#define CSS_FONT_SIZE_inherit		0xffff
#define CSS_FONT_SIZE_larger		0xfffe
#define CSS_FONT_SIZE_smaller		0xfffd
#define CSS_FONT_SIZE_value_not_set	0xfffc
#define CSS_FONT_SIZE_use_lang_def	0xfffb
#define CSS_FONT_SIZE_MAX			0xfffa

// values for FontFamilyProps:info.type:3
#define CSS_FONT_FAMILY_font_number		0
#define CSS_FONT_FAMILY_inherit			1
#define CSS_FONT_FAMILY_value_not_set	2
#define CSS_FONT_FAMILY_use_lang_def	3
#define CSS_FONT_FAMILY_generic			4

// values for BorderProps:info.style:4
#define CSS_BORDER_STYLE_inherit	0
#define CSS_BORDER_STYLE_none		1
#define CSS_BORDER_STYLE_hidden		2
#define CSS_BORDER_STYLE_dotted		3
#define CSS_BORDER_STYLE_dashed		4
#define CSS_BORDER_STYLE_solid		5
#define CSS_BORDER_STYLE_double		6
#define CSS_BORDER_STYLE_groove		7
#define CSS_BORDER_STYLE_ridge		8
#define CSS_BORDER_STYLE_inset		9
#define CSS_BORDER_STYLE_outset		10
#define CSS_BORDER_STYLE_highlight_border	11
#define CSS_BORDER_STYLE_value_not_set		12

// values for MultiPaneProps:info.column_fill:2
#define CSS_COLUMN_FILL_inherit		0
#define CSS_COLUMN_FILL_auto		1
#define CSS_COLUMN_FILL_balance		2
#define CSS_COLUMN_FILL_value_not_set	3

// values for MultiPaneProps:info.column_span:14
#define CSS_COLUMN_SPAN_max_value		0x3ffb
#define CSS_COLUMN_SPAN_inherit			0x3ffc
#define CSS_COLUMN_SPAN_none			0x3ffd
#define CSS_COLUMN_SPAN_all				0x3ffe
#define CSS_COLUMN_SPAN_value_not_set	0x3fff

// values for PageProps:info.break_after:4
// values for PageProps:info.break_before:4
// values for PageProps:info.break_inside:3
#define CSS_BREAK_inherit		0
#define CSS_BREAK_auto			1
#define CSS_BREAK_avoid			2
#define CSS_BREAK_avoid_page	3
#define CSS_BREAK_avoid_column	4
#define CSS_BREAK_always		5
#define CSS_BREAK_left			6
#define CSS_BREAK_right			7
#define CSS_BREAK_page			8
#define CSS_BREAK_column		9

// values for PageProps:info.page:3
#define CSS_PAGE_inherit	0
#define CSS_PAGE_auto		1
#define CSS_PAGE_identifier	2

// max values for PageProps:info.orphans:9 and PageProps:info.widows:9
// leave room for inherit
#define CSS_ORPHANS_WIDOWS_VAL_MAX	(0x1ff-1)

// values for WapMarqueeProps:info.style:2
#define CSS_WAP_MARQUEE_STYLE_scroll	0
#define CSS_WAP_MARQUEE_STYLE_slide		1
#define CSS_WAP_MARQUEE_STYLE_alternate	2

// values for WapMarqueeProps:info.dir:2
#define CSS_WAP_MARQUEE_DIR_rtl		0
#define CSS_WAP_MARQUEE_DIR_ltr		1
#define CSS_WAP_MARQUEE_DIR_up		2
#define CSS_WAP_MARQUEE_DIR_down	3

// values for WapMarqueeProps:info.speed:2
#define CSS_WAP_MARQUEE_SPEED_slow			0
#define CSS_WAP_MARQUEE_SPEED_normal		1
#define CSS_WAP_MARQUEE_SPEED_fast			2
#define CSS_WAP_MARQUEE_SPEED_value_not_set	3

// values for OtherProps2:info.box_decoration_break
#define CSS_BOX_DECORATION_BREAK_slice			0
#define CSS_BOX_DECORATION_BREAK_clone			1
#define CSS_BOX_DECORATION_BREAK_inherit		2
#define CSS_BOX_DECORATION_BREAK_value_not_set	3

#define CSS_ORPHANS_WIDOWS_inherit	CSS_ORPHANS_WIDOWS_VAL_MAX+1

#define CSS_ZINDEX_inherit			LONG_MIN
#define CSS_ZINDEX_auto				LONG_MIN+1
#define CSS_ZINDEX_MIN_VAL			LONG_MIN+2
#define CSS_ZINDEX_MAX_VAL			LONG_MAX

#define CSS_TABLE_BASELINE_not_set	LONG_MIN
#define CSS_TABLE_BASELINE_inherit	LONG_MIN+1

#define CSS_TAB_SIZE_inherit		LONG_MIN

// values for length units
#define CSS_LENGTH_number		0
#define CSS_LENGTH_em			1
#define CSS_LENGTH_rem			2
#define CSS_LENGTH_px			3
#define CSS_LENGTH_ex			4
#define CSS_LENGTH_cm			5
#define CSS_LENGTH_mm			6
#define CSS_LENGTH_in			7
#define CSS_LENGTH_pt			8
#define CSS_LENGTH_pc			9
#define CSS_LENGTH_percentage	10
#define CSS_LENGTH_keyword_px	11	// keyword stored with pixel values (thin, thick, medium)

struct CommonProps
{
	struct {
		unsigned long text_decoration:4;
		unsigned long inherit_text_decoration:1;
		unsigned long text_decoration_set:1;
		unsigned long text_align:3;
		unsigned long float_type:4;
		unsigned long display_type:5;
		unsigned long visibility:3;
		unsigned long white_space:3;
		unsigned long clear:3;
		unsigned long position:3;
		//unsigned long unused____:2;
	} info;

	short	GetTextDecoration();
};

struct BorderProps
{
	struct {
		unsigned long style:4;
		//unsigned long unused____:28;
	} info;
};

struct OtherProps
{
	struct {
		unsigned long text_transform:3;
		unsigned long table_layout:2;
		unsigned long caption_side:3;
		unsigned long border_collapse:2;
		unsigned long empty_cells:2;
#ifdef SUPPORT_TEXT_DIRECTION
		unsigned long direction:2;
		unsigned long unicode_bidi:3;
#endif
		unsigned long list_style_pos:2;
		unsigned long list_style_type:5;
		unsigned long text_overflow:2;
		unsigned long resize:3;
		unsigned long box_sizing:2;
		// unsigned long unused____:1;
	} info;
};

struct FontProps
{
	struct {
		unsigned long weight:4;
		unsigned long style:3;
		unsigned long variant:2;
		unsigned long size:16;
		unsigned long size_type:4;
		// unsigned long unused____:3;
	} info;
};

struct FontFamilyProps
{
	struct {
		unsigned long type:3;
		unsigned long font_number:16;
		unsigned long awaiting_webfont:1;
		// unsigned long unused____:12;
	} info;
};

struct OtherProps2
{
	struct {
		unsigned long opacity:8;
		unsigned long opacity_set:1;
		unsigned long inherit_opacity:1;
		unsigned long overflow_wrap:2;
		unsigned long object_fit:3;
		unsigned long box_decoration_break:2;
		unsigned long bg_size_round:1;
		unsigned long overflowx:4;
		unsigned long overflowy:4;
		// unsigned long unused____:6;
	} info;
};

struct PageProps
{
	struct {
		unsigned long break_after:4;
		unsigned long break_before:4;
		unsigned long break_inside:3;
		unsigned long page:2;
		unsigned long orphans:9;
		unsigned long widows:9;
		unsigned long unused____:1;
	} info;
};

struct MultiPaneProps
{
	struct {
		unsigned long column_fill:2;
		unsigned long column_span:14;
//		unsigned long unused____:16;
	} info;
};

struct WapMarqueeProps
{
	struct {
		unsigned long style:2;
		unsigned long dir:2;
		unsigned long speed:2;
		unsigned long loop:26;
	} info;
};

#define CSS_WAP_MARQUEE_LOOP_VAL_MAX		0x03ffffff-1
#define CSS_WAP_MARQUEE_LOOP_infinite		CSS_WAP_MARQUEE_LOOP_VAL_MAX+1

#ifdef CSS_MINI_EXTENSIONS
struct MiniProps
{
	struct {
		unsigned long fold:2;
		unsigned long focus_opacity:8;
		unsigned long focus_opacity_set:1;
		unsigned long unused____:21;
	} info;
};

// values for MiniProps:info.fold:2
#define CSS_MINI_FOLD_none			0
#define CSS_MINI_FOLD_folded		1
#define CSS_MINI_FOLD_unfolded		2
#define CSS_MINI_FOLD_value_not_set 3
#endif // CSS_MINI_EXTENSIONS

#define LENGTH_4_VALUE_inherit	-30
#define LENGTH_4_VALUE_auto		-29
#define LENGTH_4_VALUE_not_set	-28
#define LENGTH_4_PROPS_VAL_MIN	-27
#define LENGTH_4_PROPS_VAL_MAX	31
//#define LENGTH_4_PROPS_UVAL_MAX	60
#define LENGTH_4_PROPS_TYPE_MAX 3 // just 2 bits for the type

struct Length4Props
{
	struct {
		signed long		val1:6; // 0 - 63 or -32 - +31
		unsigned long	val1_type:2; // 0 - 3
		signed long		val2:6; // 0 - 63 or -32 - +31
		unsigned long	val2_type:2; // 0 - 3
		signed long		val3:6; // 0 - 63 or -32 - +31
		unsigned long	val3_type:2; // 0 - 3
		signed long		val4:6; // 0 - 63 or -32 - +31
		unsigned long	val4_type:2; // 0 - 3
	} info;
};

#define LENGTH_2_VALUE_inherit	-2046 //4095
#define LENGTH_2_VALUE_auto		-2045 //4094
#define LENGTH_2_VALUE_o_content_size	-2044 //4093
#define LENGTH_2_VALUE_o_skin	-2043 //4092
#define LENGTH_2_VALUE_normal	LENGTH_2_VALUE_auto // never in combination with LENGTH_2_VALUE_auto
#define LENGTH_2_VALUE_none		LENGTH_2_VALUE_auto // never in combination with LENGTH_2_VALUE_auto
#define LENGTH_2_VALUE_not_set	-2042 //4092
#define LENGTH_2_PROPS_VAL_MIN	-2041
#define LENGTH_2_PROPS_VAL_MAX	2047
//#define LENGTH_2_PROPS_UVAL_MAX	4092

struct Length2Props
{
	struct {
		signed long		val1:12; // 0 - 4095 or -2048 - +2047
		unsigned long	val1_type:4; // 0 - 15
		signed long		val2:12; // 0 - 4095 or -2048 - +2047
		unsigned long	val2_type:4; // 0 - 15
	} info;
};

#define LENGTH_VALUE_inherit	-134217726
#define LENGTH_VALUE_auto		-134217725 // never in combination with LENGTH_VALUE_none
#define LENGTH_VALUE_o_content_size	-134217724
#define LENGTH_VALUE_o_skin		-134217723
#define LENGTH_VALUE_none		LENGTH_VALUE_auto // never in combination with LENGTH_VALUE_auto
#define LENGTH_PROPS_VAL_MIN	-134217722
#define LENGTH_PROPS_VAL_MAX	134217727
//#define LENGTH_PROPS_UVAL_MAX	268435456

struct LengthProps
{
	struct {
		signed long		val:28; // 0 - 268435456 or -134217728 - +134217727
		unsigned long	val_type:4; // 0 - 15
	} info;
};

typedef enum {
	CSS_VALUE_KEYWORD,
	CSS_VALUE_LENGTH,
	CSS_VALUE_NUMBER,
	CSS_VALUE_STRING
} CSS_ValueType;

class CSS_Value
{
public:

						CSS_Value(CSS_decl* css_decl, short default_keyword);

						CSS_Value(short keyword)
							: type(CSS_VALUE_KEYWORD), length_unit(0) { value.keyword = keyword; }
						CSS_Value(float val, short unit)
							: type(CSS_VALUE_LENGTH), length_unit(unit) { value.length = val; }
						CSS_Value(long number)
							: type(CSS_VALUE_NUMBER), length_unit(0) { value.number = number; }
						CSS_Value(const uni_char* string)
							: type(CSS_VALUE_STRING), length_unit(0) { value.string = string; }

	union {
		int				keyword;
		float			length;
		long			number;
		const uni_char*	string;
	} value;

	CSS_ValueType		type;
	short				length_unit;

	/*CSS_Value&	operator=(short keyword) { type = CSS_VALUE_KEYWORD; value.keyword = keyword; }
	CSS_Value&	operator=(long number) { type = CSS_VALUE_NUMBER; value.number = number; }
	CSS_Value&	operator=(const uni_char* string) { type = CSS_VALUE_STRING; value.string = string; }*/

	void		SetKeyword(int keyword) { type = CSS_VALUE_KEYWORD; value.keyword = keyword; }
	void		SetLength(float val, short unit) { type = CSS_VALUE_NUMBER; value.length = val; length_unit = unit; }
	void		SetNumber(long number) { type = CSS_VALUE_NUMBER; value.number = number; }
	void		SetString(const uni_char* string) { type = CSS_VALUE_STRING; value.string = string; }
};

struct CSS_ValueEx
{
	CSS_decl*	css_decl;
	int			ivalue;
	int			type;
	BOOL		is_decimal;
	BOOL		is_keyword;
};

enum PropertyChange
{
	/** No change. */
	PROPS_CHANGED_NONE = 0,

	/** Change requires repaint of element. */
	PROPS_CHANGED_UPDATE = 1 << 0,

	/** Change requires a reflow of the element (MarkDirty). */
	PROPS_CHANGED_SIZE = 1 << 1,

	/** Change requires new layout box (MarkExtraDirty). */
	PROPS_CHANGED_STRUCTURE = 1 << 2,

	/** Size that affect layout has not changed, but visual size changed
		(F.ex Outline) */
	PROPS_CHANGED_UPDATE_SIZE = 1 << 3,

	/** Change requires reflow of all text including measuring word widths. */
	PROPS_CHANGED_REMOVE_CACHED_TEXT = 1 << 4,

	/** Element has border after property changes. */
	PROPS_CHANGED_HAS_BORDER = 1 << 5,

	/** Border widths changed. */
	PROPS_CHANGED_BORDER_WIDTH = 1 << 6,

	/** Border colors changed. */
	PROPS_CHANGED_BORDER_COLOR = 1 << 7,

	/** Transition properties changed. */
	PROPS_CHANGED_TRANSITION = 1 << 8,

	/** The bounding box needs to be recalculated. */
	PROPS_CHANGED_BOUNDS = 1 << 9

#ifdef SVG_SUPPORT
	,

	/** SVG specific change bit: The element may have changed size and/or
		position */
	PROPS_CHANGED_SVG_RELAYOUT = 1 << 10,

	/** SVG specific change bit: The element may have changed
		appearance */
	PROPS_CHANGED_SVG_REPAINT = 1 << 11,

	/** SVG specific change bit: The element may have changed audio
		level */
	PROPS_CHANGED_SVG_AUDIO_LEVEL = 1 << 12,

	/** SVG specific change bit: The elements display property changed */
	PROPS_CHANGED_SVG_DISPLAY = 1 << 13
#endif // SVG_SUPPORT
};


#ifdef SVG_SUPPORT

/** All SVG specific change bits. Must be updated when new SVG
	specific bits are added. */

# define PROPS_CHANGED_SVG (PROPS_CHANGED_SVG_RELAYOUT | PROPS_CHANGED_SVG_REPAINT | PROPS_CHANGED_SVG_AUDIO_LEVEL | PROPS_CHANGED_SVG_DISPLAY)

#endif // SVG_SUPPORT

class CssPropertyItem
{
private:
	friend class HTMLayoutProperties;
	friend class LayoutWorkplace;
	friend class SharedCss;
	friend class SharedCssManager;

	struct {
		unsigned short type:8; // CssPropertiesItemType
		union {
			struct {
				unsigned short val1_percentage:1;
				unsigned short val1_decimal:1; // multiplied with 100 and stored as integer
				unsigned short val1_default:1; // comes from default stylesheet
				unsigned short val1_unspecified:1; // set as specified (even when from default stylesheet)
				unsigned short val2_percentage:1;
				unsigned short val2_decimal:1; // multiplied with 100 and stored as integer
				unsigned short val2_default:1; // comes from default stylesheet
				unsigned short val2_unspecified:1; // set as specified (even when from default stylesheet)
				unsigned short val3_percentage:1;
				unsigned short val3_decimal:1; // multiplied with 100 and stored as integer
				unsigned short val3_default:1; // comes from default stylesheet
				unsigned short val3_unspecified:1; // set as specified (even when from default stylesheet)
				unsigned short val4_percentage:1;
				unsigned short val4_decimal:1; // multiplied with 100 and stored as integer
				unsigned short val4_default:1; // comes from default stylesheet
				unsigned short val4_unspecified:1; // set as specified (even when from default stylesheet)
			} info1;

			struct {
				unsigned short val_percentage:1;
				unsigned short image_is_gradient:1;
				// Used for storing CSSValues and small or enum-like integers.
				// 14 bits is the minimum size to safely store CSSValues.
				unsigned short extra:14;
			} info2;

			unsigned short is_inherit:1; // used in combination with CSS_decl* and uni_char* (value NULL)
			unsigned short info_init:16;
		};

		unsigned short decl_is_referenced:1; // This property item has a reference to a CSS declaration.

	} info;

	union {
		CommonProps		common_props;
		OtherProps		other_props;
		BorderProps		border_props;
		FontProps		font_props;
		FontFamilyProps	font_family_props;
		OtherProps2		other_props2;
		PageProps		page_props;
		MultiPaneProps	multi_pane_props;
		LengthProps		length_val;
		Length2Props	length2_val;
		Length4Props	length4_val;
		long			long_val;
		float			float_val;
		COLORREF		color_val;
		const uni_char*	string;
		CSS_decl*		css_decl;
		WapMarqueeProps	wap_marquee_props;
		FlexboxModes	flexbox_modes;
		CssURL			url;
#ifdef CSS_GRADIENT_SUPPORT
		CSS_Gradient*	gradient;
#endif // CSS_GRADIENT_SUPPORT
		WritingSystem::Script
						writing_system;
#ifdef CSS_MINI_EXTENSIONS
		MiniProps		mini_props;
#endif // CSS_MINI_EXTENSIONS
	} value;

	void		Init() { info.type = CSSPROPS_UNUSED; info.info_init = 0; value.long_val = 0; info.decl_is_referenced = 0; }

	static int	GetCssPropertyChangeBits(HTML_Element* elm, CssPropertyItem* a, CssPropertyItem* b);
#ifdef SVG_SUPPORT
	static int	GetSVGCssPropertyChangeBits(HTML_Element* elm, CssPropertyItem* a, CssPropertyItem* b);

	struct SVGPropertyMapping {
		short p1;
		short p2;
	};
#endif

	static void LoadSVGCssProperties(const CSS_Properties &cssprops, CssPropertyItem* props_array, int &prop_count);

#ifdef WEBKIT_OLD_FLEXBOX

	/** Return TRUE if this element appears to be an old-spec flexbox item.

		When this happens, the 20090723 version of the flexbox spec will be
		used (only supported -webkit- prefixed) on this element. */

	static BOOL IsOldSpecFlexboxItem(HTML_Element* elm);

	/** Return TRUE if this element appears to be an old-spec flexbox.

		When this happens, the 20090723 version of the flexbox spec will be
		used (only supported -webkit- prefixed) on this element. */

	static BOOL IsOldSpecFlexbox(const CSS_Properties& cssprops);

#endif // WEBKIT_OLD_FLEXBOX

	/** Set this item to CSSPROPS_FLEX_MODES and populate it according to the properties. */

	void SetFlexModes(const CSS_Properties& cssprops, HTML_Element* elm);

public:

	CssPropertyItem() { Init(); REPORT_MEMMAN_INC(sizeof(CssPropertyItem)); }

	~CssPropertyItem();

	static void		SetLengthPropertyNew(HTML_Element* elm,
										 unsigned int prop,
										 int count,
										 CSS_ValueEx value[],
										 BOOL value_calculated,
										 CssPropertyItem props_array[],
										 int& prop_count);

	static void		SetPageProperty(HTML_Element* elm, int css_prop, const CSS_Value& value, CssPropertyItem* pi = NULL);

	static void		SetCssZIndex(HTML_Element* elm, const CSS_Value& value, CssPropertyItem* pi = NULL);

	// static methods to manipulate the stored CSS properties
	static CssPropertyItem*
					GetCssPropertyItem(HTML_Element* elm, unsigned short prop);

	/* Static helper for adding references to css declarations */

	static void		AddReferences(CssPropertyItem *new_props, int prop_count);

	/* Static helper for removing references to css declarations */

	static void		RemoveReferences(CssPropertyItem *new_props, int prop_count);

	/* Static helper for assigning declarations to a CssPropertyItems array */

	static void		AssignCSSDeclaration(CssPropertiesItemType prop,
										 CssPropertyItem* props_array,
										 int &prop_count,
										 CSS_decl *decl);

	static OP_STATUS	LoadCssProperties(HTML_Element* elm,
									  double runtime_ms,
									  HLDocProfile* hld_profile,
									  CSS_MediaType media_type,
									  int* changes = NULL);

	/** @deprecated Temporary access methods below. They are inlined because
		they are temporarily being used in one method each in HTML_Element.
		DEPRECATED! SHALL NOT BE USED!  inlined below. */
	static CursorType	DEPRECATED(GetCursorType(HTML_Element* elm));
	static COLORREF		DEPRECATED(GetCssBackgroundColor(HTML_Element* elm));
	static COLORREF		DEPRECATED(GetCssColor(HTML_Element* elm));
};

#ifndef DOXYGEN_DOCUMENTATION

/* gcc 3 but < 3.4 gets tangled up combining DEPRECATED with inline, so
 * separate inline definition from DEPRECATED declaration:
 */
inline CursorType CssPropertyItem::GetCursorType(HTML_Element* elm)
{
	CssPropertyItem* item = CssPropertyItem::GetCssPropertyItem(elm, CSSPROPS_CURSOR);
	if (item)
		return (CursorType)item->info.info2.extra;

	return CURSOR_DEFAULT_ARROW;
}

inline COLORREF CssPropertyItem::GetCssBackgroundColor(HTML_Element* elm)
{
	CssPropertyItem* item = GetCssPropertyItem(elm, CSSPROPS_BG_COLOR);
	if (item)
	{
		COLORREF col = item->value.color_val;
		OP_ASSERT((col & CSS_COLOR_KEYWORD_TYPE_named) == 0 || (col & 0x0f000000) == 0); // This is why you should not use this deprecated method. You need to use the cascade instead to get the computed value.
		if ((col & CSS_COLOR_KEYWORD_TYPE_named) != 0 && (col & 0x0f000000) != 0)
			return USE_DEFAULT_COLOR;
		return HTM_Lex::GetColValByIndex(col);
	}
	else
		return USE_DEFAULT_COLOR;
}

inline COLORREF CssPropertyItem::GetCssColor(HTML_Element* elm)
{
	CssPropertyItem* item = CssPropertyItem::GetCssPropertyItem(elm, CSSPROPS_COLOR);
	if (item)
	{
		COLORREF col = item->value.color_val;
		if (col == CSS_COLOR_transparent)
			return USE_DEFAULT_COLOR;
		if (col == CSS_COLOR_inherit || col == CSS_COLOR_invert || col == CSS_COLOR_current_color)
		{
			// This is the reason this method shouldn't be used. We don't have the context to
			// return a correct color.
			return USE_DEFAULT_COLOR;
		}
		return HTM_Lex::GetColValByIndex(col);
	}
	else
		return USE_DEFAULT_COLOR;
}

#endif // !DOXYGEN_DOCUMENTATION

class SharedCss;

class SharedCssManager
{
public:
	static SharedCssManager* CreateL();
	~SharedCssManager();

	void DeleteSharedCssProperties(CssPropertyItem*, int size);
	CssPropertyItem* GetSharedCssProperties(SharedCss*& shared, CssPropertyItem*, int size);

	static void DecRef(SharedCss* shared);

	unsigned would_have_allocated, would_have_allocated_peak;
	unsigned have_allocated, have_allocated_peak;

private:
	SharedCssManager();

	Head* property_list;
	SharedCss* FindSharedCss(CssPropertyItem* x, int size, int create);
};

/** Some initial values */
#define BACKGROUND_POSITION_INITIAL 0
#define OBJECT_POSITION_INITIAL 50

#endif // CSSPROPS_H
