/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/display/styl_man.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/layout/cssprops.h"
#include "modules/layout/layout_fixed_point.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/probetools/probepoints.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/url/url_api.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/ui/OpUiInfo.h"
#include "modules/layout/box/box.h"
#include "modules/layout/content/content.h"
#include "modules/forms/formsenum.h"
#include "modules/style/css_gradient.h"

#include "modules/util/gen_math.h"
#include "modules/layout/frm.h"
#ifdef SKIN_SUPPORT
# include "modules/skin/OpSkinManager.h"
#endif // SKIN_SUPPORT

#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"

#ifdef SCOPE_SUPPORT
# include "modules/probetools/probetimeline.h"
#endif // SCOPE_SUPPORT

CssPropertyItem::~CssPropertyItem()
{
	REPORT_MEMMAN_DEC(sizeof(CssPropertyItem));

	if (info.decl_is_referenced)
		value.css_decl->Unref();
}

/* static */ int
CssPropertyItem::GetCssPropertyChangeBits(HTML_Element* elm, CssPropertyItem* a, CssPropertyItem* b)
{
	int bits = 0;
	BOOL style_removed = FALSE;

	if (!a)
		if (!b)
			return 0;
		else
		{
			style_removed = TRUE;
			a = b;
			b = NULL;
		}

#define IFCHANGE(A, B) if (a->value.CPR.info.A != b->value.CPR.info.A) bits|=B;
#define IFSET(A, B, C) if (a->value.CPR.info.A != B) bits|=C;
#define IFSET2(A, B1, B2, C) if (a->value.CPR.info.A != B1 && a->value.CPR.info.A != B2) bits|=C;

	switch (a->info.type)
	{
#define CPR common_props
	case CSSPROPS_COMMON:
		{
			unsigned int old_display_type(CSS_DISPLAY_value_not_set);
			unsigned int new_display_type(CSS_DISPLAY_value_not_set);

			if (style_removed)
				old_display_type = a->value.common_props.info.display_type;
			else
			{
				new_display_type = a->value.common_props.info.display_type;
				if (b)
					old_display_type = b->value.common_props.info.display_type;
			}

			if (old_display_type == CSS_DISPLAY_value_not_set)
				old_display_type = CSS_DISPLAY_inline;

			if (new_display_type == CSS_DISPLAY_value_not_set)
				new_display_type = CSS_DISPLAY_inline;

			if (!elm->GetLayoutBox())
			{
				old_display_type = CSS_DISPLAY_none;
				HTML_Element* parent = elm->Parent();
				if (!parent->GetLayoutBox() && !parent->IsExtraDirty())
					new_display_type = CSS_DISPLAY_none;
			}

			if (old_display_type != new_display_type)
				bits |= PROPS_CHANGED_STRUCTURE;

			if (b)
			{
				IFCHANGE(text_decoration, PROPS_CHANGED_UPDATE);
				IFCHANGE(text_align, PROPS_CHANGED_UPDATE);
				IFCHANGE(float_type, PROPS_CHANGED_STRUCTURE);
				if (a->value.common_props.info.visibility != b->value.common_props.info.visibility)
				{
					if (a->value.common_props.info.visibility == CSS_VISIBILITY_collapse ||
						b->value.common_props.info.visibility == CSS_VISIBILITY_collapse)
						bits |= PROPS_CHANGED_SIZE;
					else
						bits |= PROPS_CHANGED_UPDATE;
				}
				IFCHANGE(white_space, PROPS_CHANGED_SIZE|PROPS_CHANGED_REMOVE_CACHED_TEXT);
				IFCHANGE(clear, PROPS_CHANGED_SIZE);
				IFCHANGE(position, PROPS_CHANGED_STRUCTURE);
			}
			else
			{
				bits |= PROPS_CHANGED_UPDATE;
				IFSET2(float_type, CSS_FLOAT_value_not_set, CSS_FLOAT_none, PROPS_CHANGED_STRUCTURE);
				if (a->value.common_props.info.visibility == CSS_VISIBILITY_collapse)
					bits |= PROPS_CHANGED_SIZE;
				IFSET(white_space, CSS_WHITE_SPACE_value_not_set, PROPS_CHANGED_SIZE|PROPS_CHANGED_REMOVE_CACHED_TEXT);
				IFSET2(clear, CSS_CLEAR_value_not_set, CSS_CLEAR_none, PROPS_CHANGED_SIZE);
				IFSET2(position, CSS_POSITION_value_not_set, CSS_POSITION_static, PROPS_CHANGED_STRUCTURE);
			}
		}
		break;

#undef CPR
#define CPR other_props
	case CSSPROPS_OTHER:
		if (b)
		{
			IFCHANGE(table_layout, PROPS_CHANGED_STRUCTURE);
			IFCHANGE(caption_side, PROPS_CHANGED_STRUCTURE);
			IFCHANGE(border_collapse, PROPS_CHANGED_STRUCTURE);
#ifdef SUPPORT_TEXT_DIRECTION
			IFCHANGE(direction, PROPS_CHANGED_STRUCTURE);
			IFCHANGE(unicode_bidi, PROPS_CHANGED_STRUCTURE);
#endif
			IFCHANGE(list_style_pos, PROPS_CHANGED_STRUCTURE);
			IFCHANGE(text_transform, PROPS_CHANGED_SIZE|PROPS_CHANGED_REMOVE_CACHED_TEXT);
			IFCHANGE(list_style_type, PROPS_CHANGED_STRUCTURE);
			IFCHANGE(empty_cells, PROPS_CHANGED_UPDATE);
			IFCHANGE(text_overflow, PROPS_CHANGED_UPDATE);
			IFCHANGE(resize, PROPS_CHANGED_UPDATE);
			IFCHANGE(box_sizing, PROPS_CHANGED_SIZE);
		}
		else
		{
			bits |= PROPS_CHANGED_UPDATE;
			IFSET(table_layout, CSS_TABLE_LAYOUT_auto, PROPS_CHANGED_STRUCTURE);
			IFSET(caption_side, CSS_CAPTION_SIDE_value_not_set, PROPS_CHANGED_STRUCTURE);
			IFSET(border_collapse, CSS_BORDER_COLLAPSE_value_not_set, PROPS_CHANGED_STRUCTURE);
#ifdef SUPPORT_TEXT_DIRECTION
			IFSET(direction, CSS_DIRECTION_value_not_set, PROPS_CHANGED_STRUCTURE);
			IFSET(unicode_bidi, CSS_UNICODE_BIDI_value_not_set, PROPS_CHANGED_STRUCTURE);
#endif
			IFSET(list_style_pos, CSS_LIST_STYLE_POS_value_not_set, PROPS_CHANGED_STRUCTURE);
			IFSET(text_transform, CSS_TEXT_TRANSFORM_value_not_set, PROPS_CHANGED_SIZE|PROPS_CHANGED_REMOVE_CACHED_TEXT);
			IFSET(list_style_type, CSS_LIST_STYLE_TYPE_value_not_set, PROPS_CHANGED_STRUCTURE);
			IFSET2(box_sizing, CSS_BOX_SIZING_value_not_set, CSS_BOX_SIZING_content_box, PROPS_CHANGED_SIZE);
		}
		break;
#undef CPR
#define CPR other_props2
	case CSSPROPS_OTHER2:
		if (b)
		{
			unsigned int new_opacity = a->value.other_props2.info.opacity_set ? a->value.other_props2.info.opacity : 255;
			unsigned int old_opacity = b->value.other_props2.info.opacity_set ? b->value.other_props2.info.opacity : 255;
			if (old_opacity != new_opacity)
			{
				if (old_opacity == 255 || new_opacity == 255)
					bits |= PROPS_CHANGED_STRUCTURE;
				else
					bits |= PROPS_CHANGED_UPDATE;
			}
			IFCHANGE(overflow_wrap, PROPS_CHANGED_SIZE|PROPS_CHANGED_REMOVE_CACHED_TEXT);
			IFCHANGE(object_fit, PROPS_CHANGED_UPDATE|PROPS_CHANGED_SIZE);
			IFCHANGE(bg_size_round, PROPS_CHANGED_UPDATE);
			IFCHANGE(overflowx, PROPS_CHANGED_STRUCTURE);
			IFCHANGE(overflowy, PROPS_CHANGED_STRUCTURE);
		}
		else
		{
			bits |= PROPS_CHANGED_UPDATE;
			if (a->value.other_props2.info.opacity_set && a->value.other_props2.info.opacity != 255)
				bits |= PROPS_CHANGED_STRUCTURE;
			IFSET(overflow_wrap, CSS_OVERFLOW_WRAP_value_not_set, PROPS_CHANGED_SIZE|PROPS_CHANGED_REMOVE_CACHED_TEXT);
			IFSET(object_fit, CSS_OBJECT_FIT_value_not_set, PROPS_CHANGED_SIZE);
			IFSET(overflowx, CSS_OVERFLOW_value_not_set, PROPS_CHANGED_STRUCTURE);
			IFSET(overflowy, CSS_OVERFLOW_value_not_set, PROPS_CHANGED_STRUCTURE);
		}
		break;
#undef CPR
#define CPR border_props
	case CSSPROPS_BORDER_IMAGE:

		/* FIXME: Because border widths may have changed we use the
		   bigger hammer here.

		   With closer investigation we may get away with just an
		   update, if the border widths haven't changed. */

		bits |= PROPS_CHANGED_UPDATE_SIZE;
		break;
	case CSSPROPS_BORDER_STYLE:
	case CSSPROPS_BORDER_LEFT_STYLE:
	case CSSPROPS_BORDER_RIGHT_STYLE:
	case CSSPROPS_BORDER_TOP_STYLE:
	case CSSPROPS_BORDER_BOTTOM_STYLE:
		{
			BOOL has_border = (!style_removed && (a->value.border_props.info.style != CSS_BORDER_STYLE_none &&
												  a->value.border_props.info.style != CSS_BORDER_STYLE_value_not_set));

			if (has_border)
				bits |= PROPS_CHANGED_HAS_BORDER;

			if (b)
			{
				IFCHANGE(style, PROPS_CHANGED_UPDATE_SIZE);
			}
			else
			{
				IFSET2(style, CSS_BORDER_STYLE_value_not_set, CSS_BORDER_STYLE_none, PROPS_CHANGED_UPDATE_SIZE);
			}

			if (!(bits & PROPS_CHANGED_UPDATE_SIZE))
			{
				Box* box = elm->GetLayoutBox();
				if (box && box->IsTableBox() && !box->IsTableCaption())
					bits |= PROPS_CHANGED_UPDATE_SIZE;
			}
		}
		break;

#ifdef CSS_MINI_EXTENSIONS
#undef CPR
#define CPR mini_props
	case CSSPROPS_MINI:
		if (b)
		{
			IFCHANGE(fold, PROPS_CHANGED_UPDATE);
			IFCHANGE(focus_opacity, PROPS_CHANGED_UPDATE);
		}
		else
			bits |= PROPS_CHANGED_UPDATE;
		break;
#endif // CSS_MINI_EXTENSIONS

#undef CPR

	case CSSPROPS_ZINDEX:
		if (a->value.long_val == CSS_ZINDEX_auto ||
			a->value.long_val == CSS_ZINDEX_inherit ||
			b == NULL ||
			b->value.long_val == CSS_ZINDEX_auto ||
			b->value.long_val == CSS_ZINDEX_inherit)
			bits |= PROPS_CHANGED_STRUCTURE;
		else
			bits |= PROPS_CHANGED_UPDATE_SIZE;
		break;

	case CSSPROPS_FONT:
	case CSSPROPS_FONT_NUMBER:
	case CSSPROPS_LETTER_WORD_SPACING:
	case CSSPROPS_TAB_SIZE:
		bits |= PROPS_CHANGED_REMOVE_CACHED_TEXT;
		/* fall through */
	case CSSPROPS_LEFT:
	case CSSPROPS_RIGHT:
	case CSSPROPS_WIDTH:
	case CSSPROPS_LEFT_RIGHT:
	case CSSPROPS_WIDTH_HEIGHT:
	case CSSPROPS_TOP_BOTTOM:
	case CSSPROPS_TOP:
	case CSSPROPS_BOTTOM:
	case CSSPROPS_MARGIN_TOP_BOTTOM:
	case CSSPROPS_MARGIN_LEFT_RIGHT:
	case CSSPROPS_MARGIN_TOP:
	case CSSPROPS_MARGIN_BOTTOM:
	case CSSPROPS_MARGIN_LEFT:
	case CSSPROPS_MARGIN_RIGHT:
	case CSSPROPS_PADDING_TOP_BOTTOM:
	case CSSPROPS_PADDING_LEFT_RIGHT:
	case CSSPROPS_PADDING_TOP:
	case CSSPROPS_PADDING_BOTTOM:
	case CSSPROPS_PADDING_LEFT:
	case CSSPROPS_PADDING_RIGHT:
	case CSSPROPS_LINE_HEIGHT_VALIGN:
	case CSSPROPS_BORDER_SPACING:
	case CSSPROPS_TEXT_INDENT:
	case CSSPROPS_HEIGHT:
	case CSSPROPS_MIN_WIDTH:
	case CSSPROPS_MIN_HEIGHT:
	case CSSPROPS_MIN_WIDTH_HEIGHT:
	case CSSPROPS_MAX_WIDTH:
	case CSSPROPS_MAX_HEIGHT:
	case CSSPROPS_MAX_WIDTH_HEIGHT:
	case CSSPROPS_QUOTES:
	case CSSPROPS_TABLE:
	case CSSPROPS_WRITING_SYSTEM:
	case CSSPROPS_COLUMN_GAP:
	case CSSPROPS_MULTI_PANE_PROPS:
	case CSSPROPS_FLEX_GROW:
	case CSSPROPS_FLEX_SHRINK:
	case CSSPROPS_FLEX_BASIS:
	case CSSPROPS_FLEX_MODES:
	case CSSPROPS_ORDER:
		bits |= PROPS_CHANGED_SIZE;
		break;

	case CSSPROPS_MARGIN:
	case CSSPROPS_PADDING:
		// Not changed size if we have margin: 0;
		if (b ||
			a->value.length4_val.info.val1 != 0 ||
			a->value.length4_val.info.val2 != 0 ||
			a->value.length4_val.info.val3 != 0 ||
			a->value.length4_val.info.val4 != 0)
			bits |= PROPS_CHANGED_SIZE;
		break;

	case CSSPROPS_OBJECT_POSITION:
		bits |= PROPS_CHANGED_SIZE | PROPS_CHANGED_UPDATE;
		break;

	case CSSPROPS_COLOR:
	case CSSPROPS_OUTLINE_COLOR:
	case CSSPROPS_OUTLINE_STYLE:
	case CSSPROPS_BG_IMAGE:
	case CSSPROPS_BG_POSITION:
	case CSSPROPS_BG_COLOR:
	case CSSPROPS_BG_REPEAT:
	case CSSPROPS_BG_ATTACH:
	case CSSPROPS_BG_ORIGIN:
	case CSSPROPS_BG_CLIP:
	case CSSPROPS_BG_SIZE:
	case CSSPROPS_SET_LINK_SOURCE:
	case CSSPROPS_USE_LINK_SOURCE:
	case CSSPROPS_SEL_COLOR:
	case CSSPROPS_SEL_BGCOLOR:
	case CSSPROPS_BORDER_TOP_RIGHT_RADIUS:
	case CSSPROPS_BORDER_BOTTOM_RIGHT_RADIUS:
	case CSSPROPS_BORDER_BOTTOM_LEFT_RADIUS:
	case CSSPROPS_BORDER_TOP_LEFT_RADIUS:
	case CSSPROPS_IMAGE_RENDERING:
	case CSSPROPS_COLUMN_RULE_WIDTH:
	case CSSPROPS_COLUMN_RULE_STYLE:
	case CSSPROPS_COLUMN_RULE_COLOR:
		bits |= PROPS_CHANGED_UPDATE;
		break;

#ifdef CSS_TRANSFORMS
	case CSSPROPS_TRANSFORM:
		{
			CSS_decl *decl = b ? b->value.css_decl : NULL;
			if (!decl || decl->GetDeclType() == CSS_DECL_TYPE && decl->TypeValue() == CSS_VALUE_none)
				bits |= PROPS_CHANGED_STRUCTURE;
			else
				bits |= PROPS_CHANGED_BOUNDS;
		}
		break;
#endif

	case CSSPROPS_TEXT_SHADOW:
		/* This is sub-optimal. We need to mark text boxes dirty,
		   which this will do, to let the text shadow contribute
		   to all relevant bounding boxes. But this will also nuke
		   the word info, which is not necessary when the text-
		   shadow changes. */
		bits |= PROPS_CHANGED_REMOVE_CACHED_TEXT;
		/* fall-through */
#ifdef CSS_TRANSFORMS
	case CSSPROPS_TRANSFORM_ORIGIN:
#endif
	case CSSPROPS_OUTLINE_WIDTH_OFFSET:
	case CSSPROPS_BOX_SHADOW:
		bits |= PROPS_CHANGED_BOUNDS;
		break;

#ifdef CSS_TRANSITIONS
	case CSSPROPS_TRANS_PROPERTY:
		bits |= PROPS_CHANGED_TRANSITION;
		break;
#endif // CSS_TRANSITIONS

	case CSSPROPS_LIST_STYLE_IMAGE:
	case CSSPROPS_PAGE:
	case CSSPROPS_PAGE_IDENTIFIER:
	case CSSPROPS_CLIP:
	case CSSPROPS_CONTENT:
	case CSSPROPS_COUNTER_RESET:
	case CSSPROPS_COUNTER_INCREMENT:
#ifdef ACCESS_KEYS_SUPPORT
	case CSSPROPS_ACCESSKEY:
#endif // ACCESS_KEYS_SUPPORT
		bits |= PROPS_CHANGED_STRUCTURE;
		break;

	case CSSPROPS_BORDER_WIDTH:
	case CSSPROPS_BORDER_LEFT_RIGHT_WIDTH:
	case CSSPROPS_BORDER_TOP_BOTTOM_WIDTH:
	case CSSPROPS_BORDER_TOP_WIDTH:
	case CSSPROPS_BORDER_BOTTOM_WIDTH:
	case CSSPROPS_BORDER_LEFT_WIDTH:
	case CSSPROPS_BORDER_RIGHT_WIDTH:
		bits |= PROPS_CHANGED_BORDER_WIDTH;
		break;

	case CSSPROPS_BORDER_COLOR:
	case CSSPROPS_BORDER_LEFT_COLOR:
	case CSSPROPS_BORDER_RIGHT_COLOR:
	case CSSPROPS_BORDER_TOP_COLOR:
	case CSSPROPS_BORDER_BOTTOM_COLOR:
		bits |= PROPS_CHANGED_BORDER_COLOR;
		break;

	case CSSPROPS_COLUMN_WIDTH:
		if (!a->info.is_inherit && b && !b->info.is_inherit)
			/* We did not toggle between single and multi columns, because we
			   went from one width that was neither 'auto' nor 'inherit', to
			   another width that was neither 'auto' nor 'inherit'. */

			bits |= PROPS_CHANGED_SIZE;
		else
			/* We either toggled between auto and non-auto width (which
			   typically implies toggling between single and multi columns), or
			   we cannot tell if we did ('inherit'). */

			bits |= PROPS_CHANGED_STRUCTURE;
		break;

	case CSSPROPS_COLUMN_COUNT:
		if (a->info.is_inherit || b && b->info.is_inherit ||
			(a->value.length_val.info.val <= 1) != (!b || b->value.length_val.info.val <= 1))
			/* We either toggled between single and multi columns, or we cannot
			   tell if we did ('inherit'). */

			bits |= PROPS_CHANGED_STRUCTURE;
		else
			// We did not toggle between single and multi columns.

			bits |= PROPS_CHANGED_SIZE;
		break;

	case CSSPROPS_CURSOR:
	case CSSPROPS_WAP_MARQUEE:
#ifdef SVG_SUPPORT
	case CSSPROPS_FILL:
	case CSSPROPS_STROKE:
	case CSSPROPS_FILL_OPACITY:
	case CSSPROPS_STROKE_OPACITY:
	case CSSPROPS_FILL_RULE:
	case CSSPROPS_STROKE_DASHOFFSET:
	case CSSPROPS_STROKE_DASHARRAY:
	case CSSPROPS_STROKE_MITERLIMIT:
	case CSSPROPS_STROKE_WIDTH:
	case CSSPROPS_STROKE_LINECAP:
	case CSSPROPS_STROKE_LINEJOIN:
	case CSSPROPS_TEXT_ANCHOR:
	case CSSPROPS_STOP_COLOR:
	case CSSPROPS_STOP_OPACITY:
	case CSSPROPS_FLOOD_COLOR:
	case CSSPROPS_FLOOD_OPACITY:
	case CSSPROPS_CLIP_RULE:
	case CSSPROPS_CLIP_PATH:
	case CSSPROPS_LIGHTING_COLOR:
	case CSSPROPS_MASK:
	case CSSPROPS_ENABLE_BACKGROUND:
	case CSSPROPS_FILTER:
	case CSSPROPS_POINTER_EVENTS:
	case CSSPROPS_ALIGNMENT_BASELINE:
	case CSSPROPS_BASELINE_SHIFT:
	case CSSPROPS_DOMINANT_BASELINE:
	case CSSPROPS_WRITING_MODE:
	case CSSPROPS_COLOR_INTERPOLATION:
	case CSSPROPS_COLOR_INTERPOLATION_FILTERS:
	case CSSPROPS_COLOR_PROFILE:
	case CSSPROPS_COLOR_RENDERING:
	case CSSPROPS_MARKER:
	case CSSPROPS_MARKER_END:
	case CSSPROPS_MARKER_MID:
	case CSSPROPS_MARKER_START:
	case CSSPROPS_SHAPE_RENDERING:
	case CSSPROPS_TEXT_RENDERING:
	case CSSPROPS_GLYPH_ORIENTATION_VERTICAL:
	case CSSPROPS_GLYPH_ORIENTATION_HORIZONTAL:
	case CSSPROPS_KERNING:
	case CSSPROPS_VECTOR_EFFECT:
	case CSSPROPS_AUDIO_LEVEL:
	case CSSPROPS_SOLID_COLOR:
	case CSSPROPS_DISPLAY_ALIGN:
	case CSSPROPS_SOLID_OPACITY:
	case CSSPROPS_VIEWPORT_FILL:
	case CSSPROPS_LINE_INCREMENT:
	case CSSPROPS_VIEWPORT_FILL_OPACITY:
	case CSSPROPS_BUFFERED_RENDERING:
#endif // SVG_SUPPORT
#ifdef GADGET_SUPPORT
    case CSSPROPS_CONTROL_REGION:
#endif // GADGET_SUPPORT
	case CSSPROPS_NAVUP:
	case CSSPROPS_NAVDOWN:
	case CSSPROPS_NAVLEFT:
	case CSSPROPS_NAVRIGHT:
#ifdef CSS_TRANSITIONS
	case CSSPROPS_TRANS_DELAY:
	case CSSPROPS_TRANS_DURATION:
	case CSSPROPS_TRANS_TIMING:
#endif // CSS_TRANSITIONS
#ifdef CSS_ANIMATIONS
	case CSSPROPS_ANIMATION_NAME:
	case CSSPROPS_ANIMATION_DURATION:
	case CSSPROPS_ANIMATION_TIMING_FUNCTION:
	case CSSPROPS_ANIMATION_ITERATION_COUNT:
	case CSSPROPS_ANIMATION_DIRECTION:
	case CSSPROPS_ANIMATION_PLAY_STATE:
	case CSSPROPS_ANIMATION_DELAY:
	case CSSPROPS_ANIMATION_FILL_MODE:
#endif

		/* do nothing for these properties. Change handling done somewhere else. */
		break;

	default:
		OP_ASSERT(!"All CSSPROPS structures must be handled.");
		break;
	}
	return bits;
#undef IFCHANGE
}

#ifdef SVG_SUPPORT

/* static */ int
CssPropertyItem::GetSVGCssPropertyChangeBits(HTML_Element* elm, CssPropertyItem* a, CssPropertyItem* b)
{
	/* SVG recognizes some of the common CSS properties and has its
	   slew of properties only used for SVG (which maps to the
	   "presentation attributes" in SVG). */

	int bits = 0;
	if (!a)
		if (!b)
			return 0;
		else
		{
			a = b;
			b = NULL;
		}

#define IFCHANGE(A, B) if (a->value.CPR.info.A != b->value.CPR.info.A) bits|=B;
#define IFSET(A, B, C) if (a->value.CPR.info.A != B) bits|=C;
#define IFSET2(A, B1, B2, C) if (a->value.CPR.info.A != B1 && a->value.CPR.info.A != B2) bits|=C;

	switch (a->info.type)
	{
#define CPR common_props
	case CSSPROPS_COMMON:
		{
			if (b)
			{
				IFCHANGE(display_type, PROPS_CHANGED_SVG_DISPLAY);
				IFCHANGE(text_decoration, PROPS_CHANGED_SVG_RELAYOUT);
				IFCHANGE(visibility, PROPS_CHANGED_SVG_RELAYOUT);
			}
			else
			{
				IFSET(display_type, CSS_DISPLAY_inline, PROPS_CHANGED_SVG_DISPLAY);
				IFSET(text_decoration, CSS_TEXT_DECORATION_none, PROPS_CHANGED_SVG_RELAYOUT);
				IFSET(visibility, CSS_VISIBILITY_visible, PROPS_CHANGED_SVG_RELAYOUT);
			}
		}
		break;

#undef CPR
#define CPR other_props
	case CSSPROPS_OTHER:
		if (b)
		{
#ifdef SUPPORT_TEXT_DIRECTION
			IFCHANGE(direction, PROPS_CHANGED_SVG_RELAYOUT);
			IFCHANGE(unicode_bidi, PROPS_CHANGED_SVG_RELAYOUT);
#endif
			break;
		}
		else
		{
#ifdef SUPPORT_TEXT_DIRECTION
			IFSET(direction, CSS_DIRECTION_value_not_set, PROPS_CHANGED_SVG_RELAYOUT);
			IFSET(unicode_bidi, CSS_UNICODE_BIDI_value_not_set, PROPS_CHANGED_SVG_RELAYOUT);
#endif
		}
		break;
#undef CPR
#define CPR other_props2
	case CSSPROPS_OTHER2:
		if (b)
		{
			IFCHANGE(opacity, PROPS_CHANGED_SVG_REPAINT);
			IFCHANGE(overflowx, PROPS_CHANGED_SVG_RELAYOUT);
			IFCHANGE(overflowy, PROPS_CHANGED_SVG_RELAYOUT);
		}
		else
		{
			if (a->value.other_props2.info.opacity_set && a->value.other_props2.info.opacity != 255)
				bits |= PROPS_CHANGED_SVG_REPAINT;

			IFSET(overflowx, CSS_OVERFLOW_value_not_set, PROPS_CHANGED_SVG_RELAYOUT);
			IFSET(overflowy, CSS_OVERFLOW_value_not_set, PROPS_CHANGED_SVG_RELAYOUT);
		}
		break;

#undef CPR
#undef IFCHANGE

	case CSSPROPS_FONT:
	case CSSPROPS_FONT_NUMBER:
	case CSSPROPS_CURSOR:
	case CSSPROPS_FILL_RULE:
	case CSSPROPS_STROKE_DASHARRAY:
	case CSSPROPS_STROKE_MITERLIMIT:
	case CSSPROPS_STROKE_WIDTH:
	case CSSPROPS_STROKE_LINECAP:
	case CSSPROPS_STROKE_LINEJOIN:
	case CSSPROPS_TEXT_ANCHOR:
	case CSSPROPS_CLIP_RULE:
	case CSSPROPS_CLIP_PATH:
	case CSSPROPS_MASK:
	case CSSPROPS_ENABLE_BACKGROUND:
	case CSSPROPS_FILTER:
	case CSSPROPS_POINTER_EVENTS:
	case CSSPROPS_ALIGNMENT_BASELINE:
	case CSSPROPS_BASELINE_SHIFT:
	case CSSPROPS_DOMINANT_BASELINE:
	case CSSPROPS_WRITING_MODE:
	case CSSPROPS_MARKER:
	case CSSPROPS_MARKER_END:
	case CSSPROPS_MARKER_MID:
	case CSSPROPS_MARKER_START:
	case CSSPROPS_GLYPH_ORIENTATION_VERTICAL:
	case CSSPROPS_GLYPH_ORIENTATION_HORIZONTAL:
	case CSSPROPS_KERNING:
	case CSSPROPS_VECTOR_EFFECT:
	case CSSPROPS_DISPLAY_ALIGN:
	case CSSPROPS_LINE_INCREMENT:
	case CSSPROPS_BUFFERED_RENDERING:
	case CSSPROPS_STROKE_DASHOFFSET:
	case CSSPROPS_TEXT_RENDERING:
		bits |= PROPS_CHANGED_SVG_RELAYOUT;
		break;

	case CSSPROPS_COLOR_PROFILE:
	case CSSPROPS_COLOR_RENDERING:
	case CSSPROPS_IMAGE_RENDERING:
	case CSSPROPS_COLOR_INTERPOLATION_FILTERS:
	case CSSPROPS_COLOR_INTERPOLATION:
	case CSSPROPS_LIGHTING_COLOR:
	case CSSPROPS_VIEWPORT_FILL_OPACITY:
	case CSSPROPS_SOLID_COLOR:
	case CSSPROPS_SOLID_OPACITY:
	case CSSPROPS_VIEWPORT_FILL:
	case CSSPROPS_SHAPE_RENDERING:
	case CSSPROPS_COLOR:
	case CSSPROPS_FILL:
	case CSSPROPS_STROKE:
	case CSSPROPS_FILL_OPACITY:
	case CSSPROPS_STROKE_OPACITY:
	case CSSPROPS_STOP_COLOR:
	case CSSPROPS_STOP_OPACITY:
	case CSSPROPS_FLOOD_COLOR:
	case CSSPROPS_FLOOD_OPACITY:
		bits |= PROPS_CHANGED_SVG_REPAINT;
		break;

	case CSSPROPS_AUDIO_LEVEL:
		bits |= PROPS_CHANGED_SVG_AUDIO_LEVEL;
		break;
	}
	return bits;
}

#endif // SVG_SUPPORT

static unsigned long
GetBreakValue(int keyword_value, unsigned long breakval)
{
	switch (keyword_value)
	{
	case CSS_VALUE_inherit:
		breakval = CSS_BREAK_inherit; break;
	case CSS_VALUE_always:
		breakval = CSS_BREAK_always; break;
	case CSS_VALUE_avoid:
		breakval = CSS_BREAK_avoid; break;
	case CSS_VALUE_left:
		breakval = CSS_BREAK_left; break;
	case CSS_VALUE_right:
		breakval = CSS_BREAK_right; break;
	case CSS_VALUE_page:
		breakval = CSS_BREAK_page; break;
	case CSS_VALUE_column:
		breakval = CSS_BREAK_column; break;
	case CSS_VALUE_avoid_page:
		breakval = CSS_BREAK_avoid_page; break;
	case CSS_VALUE_avoid_column:
		breakval = CSS_BREAK_avoid_column; break;
	}
	return breakval;
}

static unsigned int
GetOverflowValue(short css_val)
{
	switch (css_val)
	{
	case CSS_VALUE_visible:
		return CSS_OVERFLOW_visible;
	case CSS_VALUE_hidden:
		return CSS_OVERFLOW_hidden;
	case CSS_VALUE_scroll:
		return CSS_OVERFLOW_scroll;
	case CSS_VALUE__o_paged_x_controls:
		return CSS_OVERFLOW_paged_x_controls;
	case CSS_VALUE__o_paged_x:
		return CSS_OVERFLOW_paged_x;
	case CSS_VALUE__o_paged_y_controls:
		return CSS_OVERFLOW_paged_y_controls;
	case CSS_VALUE__o_paged_y:
		return CSS_OVERFLOW_paged_y;
	case CSS_VALUE_auto:
		return CSS_OVERFLOW_auto;
	case CSS_VALUE_inherit:
		return CSS_OVERFLOW_inherit;
	}

	return CSS_OVERFLOW_value_not_set;
}

#if defined(_CSS_LINK_SUPPORT_)

static URL
GetUrlFromCssDecl(CSS_decl* css_decl, HTML_Element* he, URL* doc_url)
{
	URL url;

	while (he && he->GetIsPseudoElement())
		he = he->Parent();

	const uni_char* url_name = NULL;
	if (css_decl->GetDeclType() == CSS_DECL_GEN_ARRAY)
	{
		const CSS_generic_value* gen_arr = css_decl->GenArrayValue();
		if (gen_arr[0].value_type == CSS_STRING_LITERAL)
			url_name = gen_arr[0].value.string;
		else if (gen_arr[0].value_type == CSS_FUNCTION_ATTR && he)
		{
			url_name = he->GetAttrValue(gen_arr[0].value.string);

			if (url_name)
			{
				// Make a copy because the value may be read from an url and the temp buffer
				// used may be overwritten by urlManager later on.
				size_t url_name_len = uni_strlen(url_name);
				if (UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2kLen()) > url_name_len)
				{
					uni_strcpy((uni_char*)g_memory_manager->GetTempBuf2k(), url_name);
					url_name = (uni_char*)g_memory_manager->GetTempBuf2k();
				}
			}
		}
	}
	else if (css_decl->GetDeclType() == CSS_DECL_STRING && he)
		url_name = css_decl->StringValue();

	if (url_name && doc_url)
	{
		url = g_url_api->GetURL(*doc_url, url_name);
	}

	return url;
}

#endif // _CSS_LINK_SUPPORT_

COLORREF
GetColorFromCssDecl(CSS_decl* css_decl)
{
	switch (css_decl->GetDeclType())
	{
	case CSS_DECL_LONG:
		return (COLORREF)css_decl->LongValue();

	case CSS_DECL_COLOR:
		return HTM_Lex::GetColValByIndex(css_decl->LongValue());

	case CSS_DECL_TYPE:
		switch (css_decl->TypeValue())
		{
		case CSS_VALUE_inherit:
			return COLORREF(CSS_COLOR_inherit);

		case CSS_VALUE_transparent:
			return COLORREF(CSS_COLOR_transparent);

		case CSS_VALUE_invert:
			return COLORREF(CSS_COLOR_invert);

		case CSS_VALUE_currentColor:
			return COLORREF(CSS_COLOR_current_color);

		default:
			return COLORREF(USE_DEFAULT_COLOR);
		}

#ifdef SKIN_SUPPORT
	case CSS_DECL_STRING:
		{
			UINT32 skin_col(static_cast<UINT32>(USE_DEFAULT_COLOR));
			if (((CSS_string_decl*)css_decl)->IsSkinString())
			{
				char name8[120]; /* ARRAY OK 2008-02-05 mstensho */
				uni_cstrlcpy(name8, css_decl->StringValue()+2, ARRAY_SIZE(name8));
				OpStatus::Ignore(g_skin_manager->GetTextColor(name8, &skin_col));
			}
			return skin_col;
		}
#endif // SKIN_SUPPORT

	default:
		return COLORREF(USE_DEFAULT_COLOR);
	}
}

long
GetBorderStyleFromCssDecl(CSS_decl* css_decl)
{
	if (css_decl->GetDeclType() == CSS_DECL_TYPE)
	{
		switch (css_decl->TypeValue())
		{
		case CSS_VALUE_inherit:
			return CSS_BORDER_STYLE_inherit;
		case CSS_VALUE_none:
			return CSS_BORDER_STYLE_none;
		case CSS_VALUE_hidden:
			return CSS_BORDER_STYLE_hidden;
		case CSS_VALUE_dotted:
			return CSS_BORDER_STYLE_dotted;
		case CSS_VALUE_dashed:
			return CSS_BORDER_STYLE_dashed;
		case CSS_VALUE_solid:
			return CSS_BORDER_STYLE_solid;
		case CSS_VALUE_double:
			return CSS_BORDER_STYLE_double;
		case CSS_VALUE_groove:
			return CSS_BORDER_STYLE_groove;
		case CSS_VALUE_ridge:
			return CSS_BORDER_STYLE_ridge;
		case CSS_VALUE_inset:
			return CSS_BORDER_STYLE_inset;
		case CSS_VALUE_outset:
			return CSS_BORDER_STYLE_outset;
		case CSS_VALUE__o_highlight_border:
			return CSS_BORDER_STYLE_highlight_border;
		}
	}

	return CSS_BORDER_STYLE_value_not_set;
}

CursorType
GetCursorValue(short keyword)
{
	switch (keyword)
	{
	case CSS_VALUE_auto: return CURSOR_AUTO;
	case CSS_VALUE_crosshair: return CURSOR_CROSSHAIR;
	case CSS_VALUE_pointer: return CURSOR_CUR_POINTER;
	case CSS_VALUE_move: return CURSOR_MOVE;
	case CSS_VALUE_e_resize: return CURSOR_E_RESIZE;
	case CSS_VALUE_ne_resize: return CURSOR_NE_RESIZE;
	case CSS_VALUE_nw_resize: return CURSOR_NW_RESIZE;
	case CSS_VALUE_n_resize: return CURSOR_N_RESIZE;
	case CSS_VALUE_se_resize: return CURSOR_SE_RESIZE;
	case CSS_VALUE_s_resize: return CURSOR_S_RESIZE;
	case CSS_VALUE_sw_resize: return CURSOR_SW_RESIZE;
	case CSS_VALUE_w_resize: return CURSOR_W_RESIZE;
	case CSS_VALUE_text: return CURSOR_TEXT;
    case CSS_VALUE_wait: return CURSOR_WAIT;
	case CSS_VALUE_help: return CURSOR_HELP;
	case CSS_VALUE_progress: return CURSOR_PROGRESS;
	case CSS_VALUE_context_menu: return CURSOR_CONTEXT_MENU;
	case CSS_VALUE_cell: return CURSOR_CELL;
	case CSS_VALUE_vertical_text: return CURSOR_VERTICAL_TEXT;
	case CSS_VALUE_alias: return CURSOR_ALIAS;
	case CSS_VALUE_copy: return CURSOR_COPY;
	case CSS_VALUE_no_drop: return CURSOR_NO_DROP;
	case CSS_VALUE_not_allowed: return CURSOR_NOT_ALLOWED;
	case CSS_VALUE_ew_resize: return CURSOR_EW_RESIZE;
	case CSS_VALUE_ns_resize: return CURSOR_NS_RESIZE;
	case CSS_VALUE_nesw_resize: return CURSOR_NESW_RESIZE;
	case CSS_VALUE_nwse_resize: return CURSOR_NWSE_RESIZE;
	case CSS_VALUE_col_resize: return CURSOR_COL_RESIZE;
	case CSS_VALUE_row_resize: return CURSOR_ROW_RESIZE;
	case CSS_VALUE_all_scroll: return CURSOR_ALL_SCROLL;
	case CSS_VALUE_none: return CURSOR_NONE;
	case CSS_VALUE_zoom_in: return CURSOR_ZOOM_IN;
	case CSS_VALUE_zoom_out: return CURSOR_ZOOM_OUT;
	case CSS_VALUE_default:
	default: return CURSOR_DEFAULT_ARROW;
	}
}

int
LengthTypeConvert(int type)
{
	switch (type)
	{
	case CSS_EM:
		return CSS_LENGTH_em;
	case CSS_REM:
		return CSS_LENGTH_rem;
	case CSS_PX:
		return CSS_LENGTH_px;
	case CSS_EX:
		return CSS_LENGTH_ex;
	case CSS_CM:
		return CSS_LENGTH_cm;
	case CSS_MM:
		return CSS_LENGTH_mm;
	case CSS_IN:
		return CSS_LENGTH_in;
	case CSS_PT:
		return CSS_LENGTH_pt;
	case CSS_PC:
		return CSS_LENGTH_pc;
	case CSS_PERCENTAGE:
		return CSS_LENGTH_percentage;
	//case CSS_NUMBER:
	default:
		return CSS_LENGTH_number;
	}
}

/** Do we need to regenerate the box of the container?

	A new box is necessary if we switch between STF (shrink-to-fit) and
	non-STF. If certain property values are 'inherit', we cannot easily tell if
	we're going to make this switch, and therefore we have to always return
	TRUE in such cases.

	The parameters are associated with the (new) specified CSS values. */

static BOOL
NeedsNewContainer(Container* container,
				  BOOL width_auto, BOOL left_auto, BOOL right_auto,
				  BOOL width_inherit, BOOL left_inherit, BOOL right_inherit,
				  unsigned int display_type,
				  unsigned int float_type,
				  unsigned int position,
				  Markup::Type elm_type)
{
	BOOL will_be_stf;

	if (width_auto || width_inherit)
	{
		if (display_type == CSS_DISPLAY_inline_block ||
			float_type == CSS_FLOAT_left || float_type == CSS_FLOAT_right ||
			(position == CSS_POSITION_absolute || position == CSS_POSITION_fixed) && (left_auto || right_auto) ||
			elm_type == Markup::HTE_LEGEND ||
			elm_type == Markup::HTE_BUTTON)
			/* Display / float / position / element type tells us that it's going to
			   become an STF container _if_ width is auto. */

			if (width_auto)
				/* Computed width is definitely auto. This will become an STF container
				   then. */

				will_be_stf = TRUE;
			else
				/* We don't know if the computed width is 'auto' or not. Since we don't
				   know what we're going to get, what we already have doesn't matter. */

				return TRUE;
		else
		{
			if (display_type == CSS_DISPLAY_inherit || float_type == CSS_FLOAT_inherit || position == CSS_POSITION_inherit)
				/* We cannot tell for sure that computed width will be non-auto, and
				   display / float / position is 'inherit', so who knows if it's going to
				   become an STF container or not? */

				return TRUE;

			if (left_inherit || right_inherit)
				if (position == CSS_POSITION_absolute || position == CSS_POSITION_fixed)
					/* Position suggests that STF may happen, left / right is 'inherit',
					   so there's no way to know.*/

					return TRUE;

			/* No matter what the computed width is, this will not become an STF
			   container. */

			will_be_stf = FALSE;
		}
	}
	else
		/* Computed width is definitely non-auto, so this will not become an STF
		   container. */

		will_be_stf = FALSE;

	return will_be_stf != container->IsShrinkToFit();
}

CSS_Value::CSS_Value(CSS_decl* css_decl, short default_keyword)
{
	switch (css_decl->GetDeclType())
	{
	case CSS_DECL_TYPE:
		type = CSS_VALUE_KEYWORD;
		value.keyword = css_decl->TypeValue();
		break;
	case CSS_DECL_NUMBER:
		type = CSS_VALUE_LENGTH;
		value.length = css_decl->GetNumberValue(0);
		length_unit = LengthTypeConvert(css_decl->GetValueType(0));
		break;
	case CSS_DECL_STRING:
		type = CSS_VALUE_STRING;
		value.string = css_decl->StringValue();
		break;
	//case CSS_DECL_LONG:
	default:
		type = CSS_VALUE_NUMBER;
		value.number = css_decl->LongValue();
		break;
	}
}

CssPropertyItem*
CssPropertyItem::GetCssPropertyItem(HTML_Element* elm, unsigned short prop)
{
	int css_prop_len = elm->GetCssPropLen();
	CssPropertyItem* css_properties = elm->GetCssProperties();
	for (int i = 0; i < css_prop_len; i++)
	{
		if (css_properties[i].info.type == prop)
			return &css_properties[i];
	}

	return NULL;
}

void
CssPropertyItem::SetLengthPropertyNew(HTML_Element* elm, unsigned int prop, int count,
								   CSS_ValueEx value[], BOOL value_calculated,
								   CssPropertyItem props_array[], int& prop_count)
{
	OP_ASSERT(count == 4 || count == 2 || count == 1 && !value_calculated);

	int i;

	if (!value_calculated)
	{
		int ivalue;
		BOOL split = FALSE;
		BOOL double_split = FALSE;

		if (count == 1)
		{
			split = TRUE;
			count = 2;
			value[1].css_decl = NULL;
		}

		for (i = 0; i < count; i++)
		{
			CSS_decl* css_decl = value[i].css_decl;
			if (css_decl)
			{
				short decl_type = css_decl->GetDeclType();
				value[i].is_keyword = decl_type == CSS_DECL_TYPE || decl_type == CSS_DECL_GEN_ARRAY && css_decl->GetValueType(i) == CSS_IDENT;
				if (value[i].is_keyword)
				{
					value[i].type = CSS_LENGTH_number;
					value[i].is_decimal = FALSE;

					switch (decl_type == CSS_DECL_TYPE ? css_decl->TypeValue() : css_decl->GenArrayValue()[i].GetType())
					{
					case CSS_VALUE_inherit:
						if (prop == CSSPROPS_LINE_HEIGHT_VALIGN && i == 1)
							value[i].ivalue = CSS_VERTICAL_ALIGN_inherit;
						else
							value[i].ivalue = count == 4 ? LENGTH_4_VALUE_inherit : LENGTH_2_VALUE_inherit;
						break;
					case CSS_VALUE_auto:
						value[i].ivalue = count == 4 ? LENGTH_4_VALUE_auto : LENGTH_2_VALUE_auto;
						break;
					case CSS_VALUE_top:
						value[i].ivalue = CSS_VERTICAL_ALIGN_top;
						break;
					case CSS_VALUE_baseline:
						value[i].ivalue = CSS_VERTICAL_ALIGN_baseline;
						break;
					case CSS_VALUE_bottom:
						value[i].ivalue = CSS_VERTICAL_ALIGN_bottom;
						break;
					case CSS_VALUE_sub:
						value[i].ivalue = CSS_VERTICAL_ALIGN_sub;
						break;
					case CSS_VALUE_super:
						value[i].ivalue = CSS_VERTICAL_ALIGN_super;
						break;
					case CSS_VALUE_text_top:
						value[i].ivalue = CSS_VERTICAL_ALIGN_text_top;
						break;
					case CSS_VALUE_middle:
						value[i].ivalue = CSS_VERTICAL_ALIGN_middle;
						break;
					case CSS_VALUE_text_bottom:
						value[i].ivalue = CSS_VERTICAL_ALIGN_text_bottom;
						break;
					case CSS_VALUE_none:
					case CSS_VALUE_normal:
						OP_ASSERT(count == 2);
						value[i].ivalue = LENGTH_2_VALUE_auto;
						break;
					case CSS_VALUE__o_content_size:
						value[i].ivalue = LENGTH_2_VALUE_o_content_size;
						break;
					case CSS_VALUE__o_skin:
						value[i].ivalue = LENGTH_2_VALUE_o_skin;
						break;
					case CSS_VALUE_medium:
						value[i].type = CSS_LENGTH_keyword_px;
						value[i].ivalue = CSS_BORDER_WIDTH_MEDIUM;
						break;
					case CSS_VALUE_thin:
						value[i].type = CSS_LENGTH_keyword_px;
						value[i].ivalue = CSS_BORDER_WIDTH_THIN;
						break;
					case CSS_VALUE_thick:
						value[i].type = CSS_LENGTH_keyword_px;
						value[i].ivalue = CSS_BORDER_WIDTH_THICK;
						break;
					default:
						OP_ASSERT(decl_type == CSS_DECL_TYPE);
						if (CSS_is_font_system_val(css_decl->TypeValue()))
						{
							// Reset line-height to normal.
							OP_ASSERT(i == 0 && count == 2);
							value[i].ivalue = LENGTH_2_VALUE_auto;
						}
						break;
					}
				}
				else
				{
					float length;

#ifdef CSS_TRANSFORMS
					const BOOL is_transform_origin = (prop == CSSPROPS_TRANSFORM_ORIGIN && css_decl->GetDeclType() == CSS_DECL_NUMBER2);
#else
					const BOOL is_transform_origin = FALSE;
#endif

					if ((prop >= CSSPROPS_BORDER_TOP_RIGHT_RADIUS && prop <= CSSPROPS_BORDER_TOP_LEFT_RADIUS) ||
						prop == CSSPROPS_BG_POSITION || prop == CSSPROPS_BG_SIZE || (prop == CSSPROPS_BORDER_SPACING && css_decl->GetDeclType() == CSS_DECL_NUMBER2) || is_transform_origin)
					{
						length = css_decl->GetNumberValue(i);
						value[i].type = LengthTypeConvert(css_decl->GetValueType(i));
					}
					else
					{
						length = css_decl->GetNumberValue(0);
						value[i].type = LengthTypeConvert(css_decl->GetValueType(0));
					}

					if (count == 4 && value[i].type > LENGTH_4_PROPS_TYPE_MAX && value[i].type != CSS_LENGTH_percentage)
						split = TRUE; // Length4Props has a value_type bitfield of 2 bits

					value[i].is_decimal = op_floor(length) != op_ceil(length);

					if (value[i].is_decimal)
						ivalue = LayoutFixedAsInt(FloatToLayoutFixed(length));
					else
					{
						if (length > INT_MAX)
							ivalue = INT_MAX;
						else if (length < INT_MIN)
							ivalue = INT_MIN;
						else
							ivalue = (int) length;
					}

					if (ivalue < 0 && prop == CSSPROPS_PADDING)
						ivalue = 0;
					else
						if (count == 4)
						{
							if (ivalue < LENGTH_4_PROPS_VAL_MIN)
							{
								split = TRUE;

								if (ivalue < LENGTH_2_PROPS_VAL_MIN)
								{
									if (prop == CSSPROPS_MARGIN || prop == CSSPROPS_PADDING || prop == CSSPROPS_BORDER_WIDTH)
										double_split = TRUE;
									else
									{
										if (value[i].is_decimal)
											value[i].is_decimal = FALSE;
										if (length > LENGTH_2_PROPS_VAL_MIN)
											ivalue = (int) length;
										else
											ivalue = LENGTH_2_PROPS_VAL_MIN;
									}
								}
							}
							else
								if (ivalue > LENGTH_4_PROPS_VAL_MAX)
								{
									split = TRUE;

									if (ivalue > LENGTH_2_PROPS_VAL_MAX)
									{
										if (prop == CSSPROPS_MARGIN || prop == CSSPROPS_PADDING || prop == CSSPROPS_BORDER_WIDTH)
											double_split = TRUE;
										else
										{
											if (value[i].is_decimal)
												value[i].is_decimal = FALSE;
											if (length < LENGTH_2_PROPS_VAL_MAX)
												ivalue = (int) length;
											else
												ivalue = LENGTH_2_PROPS_VAL_MAX;
										}
									}
								}
						}
						else
						{
							if (ivalue < LENGTH_2_PROPS_VAL_MIN)
							{
								if (prop == CSSPROPS_LINE_HEIGHT_VALIGN || prop == CSSPROPS_LETTER_WORD_SPACING ||
									prop == CSSPROPS_BORDER_SPACING || prop == CSSPROPS_OUTLINE_WIDTH_OFFSET ||
									(prop >= CSSPROPS_BORDER_TOP_RIGHT_RADIUS && prop <= CSSPROPS_BORDER_TOP_LEFT_RADIUS)
#ifdef CSS_TRANSFORMS
									|| prop == CSSPROPS_TRANSFORM_ORIGIN
#endif
									)
								{
									if (value[i].is_decimal)
										value[i].is_decimal = FALSE;
									if (length > LENGTH_2_PROPS_VAL_MIN)
										ivalue = (int) length;
									else
										ivalue = LENGTH_2_PROPS_VAL_MIN;
								}
								else
								{
									split = TRUE;

									if (ivalue < LENGTH_PROPS_VAL_MIN)
										ivalue = LENGTH_PROPS_VAL_MIN;
								}
							}
							else
								if (ivalue > LENGTH_2_PROPS_VAL_MAX)
								{
									if (prop == CSSPROPS_LINE_HEIGHT_VALIGN || prop == CSSPROPS_LETTER_WORD_SPACING ||
										prop == CSSPROPS_BORDER_SPACING || prop == CSSPROPS_OUTLINE_WIDTH_OFFSET ||
										(prop >= CSSPROPS_BORDER_TOP_RIGHT_RADIUS && prop <= CSSPROPS_BORDER_TOP_LEFT_RADIUS)
#ifdef CSS_TRANSFORMS
										|| prop == CSSPROPS_TRANSFORM_ORIGIN
#endif
)
									{
										if (value[i].is_decimal)
											value[i].is_decimal = FALSE;
										if (length < LENGTH_2_PROPS_VAL_MAX)
											ivalue = (int) length;
										else
											ivalue = LENGTH_2_PROPS_VAL_MAX;
									}
									else
									{
										split = TRUE;

										if (ivalue > LENGTH_PROPS_VAL_MAX)
											ivalue = LENGTH_PROPS_VAL_MAX;
									}
								}
						}

					value[i].ivalue = ivalue;
				}
			}
			else
				if (prop == CSSPROPS_BG_POSITION || prop == CSSPROPS_BORDER_SPACING
#ifdef CSS_TRANSFORMS
					|| prop == CSSPROPS_TRANSFORM_ORIGIN
#endif
					)
				{
					OP_ASSERT(i == 1);
					value[1] = value[0];
					value[1].css_decl = NULL;
				}
				else
				{
					value[i].ivalue = count == 4 ? LENGTH_4_VALUE_not_set : LENGTH_2_VALUE_not_set;
					value[i].type = CSS_LENGTH_number;
					value[i].is_decimal = FALSE;
					value[i].is_keyword = FALSE;
				}
		}

		if (split)
		{
			for (i = 0; i < count; i++)
			{
				if (count == 4)
				{
					if (!value[i].css_decl)
						value[i].ivalue = LENGTH_2_VALUE_not_set;
					else
						if (value[i].is_keyword)
						{
							if (value[i].ivalue == LENGTH_4_VALUE_inherit)
								value[i].ivalue = LENGTH_2_VALUE_inherit;
							else
								if (value[i].ivalue == LENGTH_4_VALUE_auto)
									value[i].ivalue = LENGTH_2_VALUE_auto;
						}
				}
				else
					if (value[i].is_keyword)
					{
						if (value[i].ivalue == LENGTH_2_VALUE_inherit)
							value[i].ivalue = LENGTH_VALUE_inherit;
						else
							if (value[i].ivalue == LENGTH_2_VALUE_auto)
								value[i].ivalue = LENGTH_VALUE_auto;
							else
								if (value[i].ivalue == LENGTH_2_VALUE_o_skin)
									value[i].ivalue = LENGTH_VALUE_o_skin;
					}
			}

			if (count == 4)
			{
				unsigned int new_prop1;
				unsigned int new_prop2;

				switch (prop)
				{
				case CSSPROPS_MARGIN:
					new_prop1 = CSSPROPS_MARGIN_LEFT_RIGHT;
					new_prop2 = CSSPROPS_MARGIN_TOP_BOTTOM;
					break;
				case CSSPROPS_PADDING:
					new_prop1 = CSSPROPS_PADDING_LEFT_RIGHT;
					new_prop2 = CSSPROPS_PADDING_TOP_BOTTOM;
					break;
				default:
					OP_ASSERT(prop == CSSPROPS_BORDER_WIDTH);
					new_prop1 = CSSPROPS_BORDER_LEFT_RIGHT_WIDTH;
					new_prop2 = CSSPROPS_BORDER_TOP_BOTTOM_WIDTH;
					break;
				}

				CSS_ValueEx tmp_value = value[1];
				value[1] = value[2];
				value[2] = tmp_value;

				if (value[0].css_decl || value[1].css_decl)
					SetLengthPropertyNew(elm, new_prop1, 2, value, !double_split, props_array, prop_count);

				if (value[2].css_decl || value[3].css_decl)
					SetLengthPropertyNew(elm, new_prop2, 2, &(value[2]), !double_split, props_array, prop_count);
			}
			else
			{
				unsigned int new_prop[2];

				switch (prop)
				{
				case CSSPROPS_LEFT_RIGHT:
					new_prop[0] = CSSPROPS_LEFT;
					new_prop[1] = CSSPROPS_RIGHT;
					break;
				case CSSPROPS_TOP_BOTTOM:
					new_prop[0] = CSSPROPS_TOP;
					new_prop[1] = CSSPROPS_BOTTOM;
					break;
				case CSSPROPS_MARGIN_TOP_BOTTOM:
					new_prop[0] = CSSPROPS_MARGIN_TOP;
					new_prop[1] = CSSPROPS_MARGIN_BOTTOM;
					break;
				case CSSPROPS_MARGIN_LEFT_RIGHT:
					new_prop[0] = CSSPROPS_MARGIN_LEFT;
					new_prop[1] = CSSPROPS_MARGIN_RIGHT;
					break;
				case CSSPROPS_PADDING_TOP_BOTTOM:
					new_prop[0] = CSSPROPS_PADDING_TOP;
					new_prop[1] = CSSPROPS_PADDING_BOTTOM;
					break;
				case CSSPROPS_PADDING_LEFT_RIGHT:
					new_prop[0] = CSSPROPS_PADDING_LEFT;
					new_prop[1] = CSSPROPS_PADDING_RIGHT;
					break;
				case CSSPROPS_BORDER_LEFT_RIGHT_WIDTH:
					new_prop[0] = CSSPROPS_BORDER_LEFT_WIDTH;
					new_prop[1] = CSSPROPS_BORDER_RIGHT_WIDTH;
					break;
				case CSSPROPS_BORDER_TOP_BOTTOM_WIDTH:
					new_prop[0] = CSSPROPS_BORDER_TOP_WIDTH;
					new_prop[1] = CSSPROPS_BORDER_BOTTOM_WIDTH;
					break;
				case CSSPROPS_WIDTH_HEIGHT:
					new_prop[0] = CSSPROPS_WIDTH;
					new_prop[1] = CSSPROPS_HEIGHT;
					break;
				case CSSPROPS_MIN_WIDTH_HEIGHT:
					new_prop[0] = CSSPROPS_MIN_WIDTH;
					new_prop[1] = CSSPROPS_MIN_HEIGHT;
					break;
				case CSSPROPS_MAX_WIDTH_HEIGHT:
					new_prop[0] = CSSPROPS_MAX_WIDTH;
					new_prop[1] = CSSPROPS_MAX_HEIGHT;
					break;
				default:
					OP_ASSERT(prop == CSSPROPS_TEXT_INDENT || prop == CSSPROPS_COLUMN_WIDTH || prop == CSSPROPS_COLUMN_RULE_WIDTH || prop == CSSPROPS_COLUMN_GAP || prop == CSSPROPS_FLEX_BASIS);
					OP_ASSERT(value[1].css_decl == NULL);
					new_prop[0] = prop;
					break;
				}

				for (i = 0; i < 2; i++)
					if (value[i].css_decl)
					{
						CssPropertyItem* pi = &props_array[prop_count];
						props_array[++prop_count].Init();

						pi->info.type = new_prop[i];

						pi->value.length_val.info.val_type = value[i].type;
						pi->value.length_val.info.val = value[i].ivalue;
						pi->info.info1.val1_decimal = value[i].is_decimal;
						if (value[i].type == CSS_LENGTH_percentage)
							pi->info.info1.val1_percentage = 1;
						if (new_prop[i] == CSSPROPS_MARGIN_TOP ||
							new_prop[i] == CSSPROPS_MARGIN_BOTTOM ||
							new_prop[i] == CSSPROPS_MARGIN_LEFT ||
							new_prop[i] == CSSPROPS_MARGIN_RIGHT ||
							new_prop[i] == CSSPROPS_PADDING_LEFT ||
							new_prop[i] == CSSPROPS_PADDING_RIGHT)
						{
							pi->info.info1.val1_default = value[i].css_decl->GetDefaultStyle();
							pi->info.info1.val1_unspecified = value[i].css_decl->GetUnspecified();
						}
					}
			}

			return;
		}
	}

	CssPropertyItem* pi = &props_array[prop_count];
	props_array[++prop_count].Init();

	pi->info.type = prop;

	if (count == 4)
	{
		if (value[0].type == CSS_LENGTH_percentage)
			pi->info.info1.val1_percentage = 1;
		else
			pi->value.length4_val.info.val1_type = value[0].type == CSS_LENGTH_keyword_px ? CSS_LENGTH_px : value[0].type;

		if (value[0].is_decimal)
			pi->info.info1.val1_decimal = 1;

		pi->value.length4_val.info.val1 = value[0].ivalue;

		if (value[1].type == CSS_LENGTH_percentage)
			pi->info.info1.val2_percentage = 1;
		else
			pi->value.length4_val.info.val2_type = value[1].type == CSS_LENGTH_keyword_px ? CSS_LENGTH_px : value[1].type;

		if (value[1].is_decimal)
			pi->info.info1.val2_decimal = 1;

		pi->value.length4_val.info.val2 = value[1].ivalue;

		if (value[2].type == CSS_LENGTH_percentage)
			pi->info.info1.val3_percentage = 1;
		else
			pi->value.length4_val.info.val3_type = value[2].type == CSS_LENGTH_keyword_px ? CSS_LENGTH_px : value[2].type;

		if (value[2].is_decimal)
			pi->info.info1.val3_decimal = 1;

		pi->value.length4_val.info.val3 = value[2].ivalue;

		if (value[3].type == CSS_LENGTH_percentage)
			pi->info.info1.val4_percentage = 1;
		else
			pi->value.length4_val.info.val4_type = value[3].type == CSS_LENGTH_keyword_px ? CSS_LENGTH_px : value[3].type;

		if (value[3].is_decimal)
			pi->info.info1.val4_decimal = 1;

		pi->value.length4_val.info.val4 = value[3].ivalue;
	}
	else
	{
		if (value[0].type == CSS_LENGTH_percentage)
			pi->info.info1.val1_percentage = 1;
		else
			pi->value.length2_val.info.val1_type = value[0].type;

		if (value[0].is_decimal)
			pi->info.info1.val1_decimal = 1;

		pi->value.length2_val.info.val1 = value[0].ivalue;

		if (value[1].type == CSS_LENGTH_percentage)
			pi->info.info1.val2_percentage = 1;
		else
			pi->value.length2_val.info.val2_type = value[1].type;

		if (value[1].is_decimal)
			pi->info.info1.val2_decimal = 1;

		pi->value.length2_val.info.val2 = value[1].ivalue;
	}

	if (prop == CSSPROPS_MARGIN ||
		prop == CSSPROPS_PADDING)
	{
		if (value[0].css_decl)
		{
			pi->info.info1.val1_default = value[0].css_decl->GetDefaultStyle();
			pi->info.info1.val1_unspecified = value[0].css_decl->GetUnspecified();
		}
		if (value[1].css_decl)
		{
			pi->info.info1.val2_default = value[1].css_decl->GetDefaultStyle();
			pi->info.info1.val2_unspecified = value[1].css_decl->GetUnspecified();
		}
		if (value[2].css_decl)
		{
			pi->info.info1.val3_default = value[2].css_decl->GetDefaultStyle();
			pi->info.info1.val3_unspecified = value[2].css_decl->GetUnspecified();
		}
		if (value[3].css_decl)
		{
			pi->info.info1.val4_default = value[3].css_decl->GetDefaultStyle();
			pi->info.info1.val4_unspecified = value[3].css_decl->GetUnspecified();
		}
	}
	else if (prop == CSSPROPS_MARGIN_TOP_BOTTOM ||
			 prop == CSSPROPS_MARGIN_LEFT_RIGHT ||
			 prop == CSSPROPS_PADDING_LEFT_RIGHT)
	{
		if (value[0].css_decl)
		{
			pi->info.info1.val1_default = value[0].css_decl->GetDefaultStyle();
			pi->info.info1.val1_unspecified = value[0].css_decl->GetUnspecified();
		}
		if (value[1].css_decl)
		{
			pi->info.info1.val2_default = value[1].css_decl->GetDefaultStyle();
			pi->info.info1.val2_unspecified = value[1].css_decl->GetUnspecified();
		}
	}
	else if (prop == CSSPROPS_LINE_HEIGHT_VALIGN)
	{
		if (value[1].css_decl)
		{
			// vertical-align

			pi->info.info1.val2_default = value[1].css_decl->GetDefaultStyle();
			pi->info.info1.val2_unspecified = value[1].css_decl->GetUnspecified();
		}
	}
}

void
CssPropertyItem::SetCssZIndex(HTML_Element* elm, const CSS_Value& value, CssPropertyItem* pi)
{
	if (pi)
	{
		pi->info.type = CSSPROPS_ZINDEX;

		if (value.type == CSS_VALUE_KEYWORD)
		{
			if (value.value.keyword == CSS_VALUE_inherit)
				pi->value.long_val = CSS_ZINDEX_inherit;
			else if (value.value.keyword == CSS_VALUE_auto)
				pi->value.long_val = CSS_ZINDEX_auto;
		}
		else
		{
			long nvalue = value.value.number;
			if (nvalue < CSS_ZINDEX_MIN_VAL)
				nvalue = CSS_ZINDEX_MIN_VAL;
			pi->value.long_val = nvalue;
		}
	}
}

void CssPropertyItem::LoadSVGCssProperties(const CSS_Properties &cssprops, CssPropertyItem* props_array, int &prop_count)
{
#ifdef SVG_SUPPORT
	SVGPropertyMapping svg_props[] = {
		{ CSS_PROPERTY_fill, CSSPROPS_FILL },
		{ CSS_PROPERTY_stroke, CSSPROPS_STROKE },
		{ CSS_PROPERTY_fill_opacity, CSSPROPS_FILL_OPACITY },
		{ CSS_PROPERTY_stroke_opacity, CSSPROPS_STROKE_OPACITY },
		{ CSS_PROPERTY_fill_rule, CSSPROPS_FILL_RULE },
		{ CSS_PROPERTY_stroke_dashoffset, CSSPROPS_STROKE_DASHOFFSET },
		{ CSS_PROPERTY_stroke_dasharray, CSSPROPS_STROKE_DASHARRAY },
		{ CSS_PROPERTY_stroke_miterlimit, CSSPROPS_STROKE_MITERLIMIT },
		{ CSS_PROPERTY_stroke_width, CSSPROPS_STROKE_WIDTH },
		{ CSS_PROPERTY_stroke_linecap, CSSPROPS_STROKE_LINECAP },
		{ CSS_PROPERTY_stroke_linejoin, CSSPROPS_STROKE_LINEJOIN },
		{ CSS_PROPERTY_text_anchor, CSSPROPS_TEXT_ANCHOR },
		{ CSS_PROPERTY_stop_color, CSSPROPS_STOP_COLOR },
		{ CSS_PROPERTY_stop_opacity, CSSPROPS_STOP_OPACITY },
		{ CSS_PROPERTY_flood_color, CSSPROPS_FLOOD_COLOR },
		{ CSS_PROPERTY_flood_opacity, CSSPROPS_FLOOD_OPACITY },
		{ CSS_PROPERTY_clip_rule, CSSPROPS_CLIP_RULE },
		{ CSS_PROPERTY_clip_path, CSSPROPS_CLIP_PATH },
		{ CSS_PROPERTY_lighting_color, CSSPROPS_LIGHTING_COLOR },
		{ CSS_PROPERTY_mask, CSSPROPS_MASK },
		{ CSS_PROPERTY_enable_background, CSSPROPS_ENABLE_BACKGROUND },
		{ CSS_PROPERTY_filter, CSSPROPS_FILTER },
		{ CSS_PROPERTY_pointer_events, CSSPROPS_POINTER_EVENTS },
		{ CSS_PROPERTY_alignment_baseline, CSSPROPS_ALIGNMENT_BASELINE },
		{ CSS_PROPERTY_baseline_shift, CSSPROPS_BASELINE_SHIFT },
		{ CSS_PROPERTY_dominant_baseline, CSSPROPS_DOMINANT_BASELINE },
		{ CSS_PROPERTY_writing_mode, CSSPROPS_WRITING_MODE },
		{ CSS_PROPERTY_color_interpolation, CSSPROPS_COLOR_INTERPOLATION },
		{ CSS_PROPERTY_color_interpolation_filters, CSSPROPS_COLOR_INTERPOLATION_FILTERS },
		{ CSS_PROPERTY_color_profile, CSSPROPS_COLOR_PROFILE },
		{ CSS_PROPERTY_color_rendering, CSSPROPS_COLOR_RENDERING },
		{ CSS_PROPERTY_marker, CSSPROPS_MARKER },
		{ CSS_PROPERTY_marker_end, CSSPROPS_MARKER_END },
		{ CSS_PROPERTY_marker_mid, CSSPROPS_MARKER_MID },
		{ CSS_PROPERTY_marker_start, CSSPROPS_MARKER_START },
		{ CSS_PROPERTY_shape_rendering, CSSPROPS_SHAPE_RENDERING },
		{ CSS_PROPERTY_text_rendering, CSSPROPS_TEXT_RENDERING },
		{ CSS_PROPERTY_glyph_orientation_vertical, CSSPROPS_GLYPH_ORIENTATION_VERTICAL },
		{ CSS_PROPERTY_glyph_orientation_horizontal, CSSPROPS_GLYPH_ORIENTATION_HORIZONTAL },
		{ CSS_PROPERTY_kerning, CSSPROPS_KERNING },
		{ CSS_PROPERTY_vector_effect, CSSPROPS_VECTOR_EFFECT },
		{ CSS_PROPERTY_audio_level, CSSPROPS_AUDIO_LEVEL },
		{ CSS_PROPERTY_solid_color, CSSPROPS_SOLID_COLOR },
		{ CSS_PROPERTY_display_align, CSSPROPS_DISPLAY_ALIGN },
		{ CSS_PROPERTY_solid_opacity, CSSPROPS_SOLID_OPACITY },
		{ CSS_PROPERTY_viewport_fill, CSSPROPS_VIEWPORT_FILL },
		{ CSS_PROPERTY_line_increment, CSSPROPS_LINE_INCREMENT },
		{ CSS_PROPERTY_viewport_fill_opacity, CSSPROPS_VIEWPORT_FILL_OPACITY },
		{ CSS_PROPERTY_buffered_rendering, CSSPROPS_BUFFERED_RENDERING },
	};

	struct {
		CSS_decl *decl;
		short p2;
	} svg_decls[ARRAY_SIZE(svg_props)];

	unsigned int num_svg_decls = 0;
	for (unsigned int svg_prop_counter = 0;
		 svg_prop_counter < ARRAY_SIZE(svg_props);
		 svg_prop_counter++)
	{
		if (cssprops.GetDecl(svg_props[svg_prop_counter].p1))
		{
			svg_decls[num_svg_decls].p2 = svg_props[svg_prop_counter].p2;
			svg_decls[num_svg_decls++].decl = cssprops.GetDecl(svg_props[svg_prop_counter].p1);
		}
	}

	for (unsigned int i = 0; i < num_svg_decls; i++)
	{
		props_array[prop_count].info.type = svg_decls[i].p2;
		props_array[prop_count].info.decl_is_referenced = 1;
		props_array[prop_count].value.css_decl = svg_decls[i].decl;
		props_array[++prop_count].Init();
	}
#endif // SVG_SUPPORT
}

#ifdef WEBKIT_OLD_FLEXBOX

/** Return TRUE if this element appears to be an old-spec flexbox item. */

/* static */ BOOL
CssPropertyItem::IsOldSpecFlexboxItem(HTML_Element* elm)
{
	if (HTML_Element* parent = elm->Parent())
	{
		if (parent->GetCssPropLen() > 0)
		{
			CssPropertyItem* pi = parent->GetCssProperties();

			// CommonProps are first, if specified at all. No need to search.

			if (pi->info.type == CSSPROPS_COMMON)
			{
				int display_type = pi->value.common_props.info.display_type;

				if (display_type == CSS_DISPLAY__webkit_box || display_type == CSS_DISPLAY__webkit_inline_box)
					return TRUE;
			}
		}
	}

	return FALSE;
}

/** Return TRUE if this element appears to be an old-spec flexbox. */

/* static */ BOOL
CssPropertyItem::IsOldSpecFlexbox(const CSS_Properties& cssprops)
{
	if (CSS_decl* cp = cssprops.GetDecl(CSS_PROPERTY_display))
	{
		OP_ASSERT(cp->GetDeclType() == CSS_DECL_TYPE);

		if (cp->TypeValue() == CSS_VALUE__webkit_box || cp->TypeValue() == CSS_VALUE__webkit_inline_box)
			return TRUE;
	}

	return FALSE;
}

#endif // WEBKIT_OLD_FLEXBOX

/** Set justify-content, align-content, align-items, align-self, flex-direction and flex-wrap. */

void
CssPropertyItem::SetFlexModes(const CSS_Properties& cssprops, HTML_Element* elm)
{
	FlexboxModes modes;
	CSS_decl* cp;

	modes.Reset();

#ifdef WEBKIT_OLD_FLEXBOX
	/* Make a feeble attempt at guessing whether we should use the old
	   (-webkit-) or new spec. */

	BOOL is_oldspec_box = IsOldSpecFlexbox(cssprops);
	BOOL is_oldspec_item = IsOldSpecFlexboxItem(elm);
#endif // WEBKIT_OLD_FLEXBOX

	// justify-content

#ifdef WEBKIT_OLD_FLEXBOX
	if (is_oldspec_box)
		cp = cssprops.GetDecl(CSS_PROPERTY__webkit_box_pack);
	else
#endif // WEBKIT_OLD_FLEXBOX
	cp = cssprops.GetDecl(CSS_PROPERTY_justify_content);

	if (cp)
	{
		OP_ASSERT(cp->GetDeclType() == CSS_DECL_TYPE);
		modes.SetJustifyContentCSSValue(cp->TypeValue());
	}

	// align-content

#ifdef WEBKIT_OLD_FLEXBOX
	if (is_oldspec_box)
		cp = NULL; // No such property in the old spec.
	else
#endif // WEBKIT_OLD_FLEXBOX
		cp = cssprops.GetDecl(CSS_PROPERTY_align_content);

	if (cp)
	{
		OP_ASSERT(cp->GetDeclType() == CSS_DECL_TYPE);
		modes.SetAlignContentCSSValue(cp->TypeValue());
	}

	// align-items

#ifdef WEBKIT_OLD_FLEXBOX
	if (is_oldspec_box)
		cp = cssprops.GetDecl(CSS_PROPERTY__webkit_box_align);
	else
#endif // WEBKIT_OLD_FLEXBOX
		cp = cssprops.GetDecl(CSS_PROPERTY_align_items);

	if (cp)
	{
		OP_ASSERT(cp->GetDeclType() == CSS_DECL_TYPE);
		modes.SetAlignItemsCSSValue(cp->TypeValue());
	}

	// align-self

#ifdef WEBKIT_OLD_FLEXBOX
	if (is_oldspec_item)
		cp = NULL; // No such property in the old spec.
	else
#endif // WEBKIT_OLD_FLEXBOX
		cp = cssprops.GetDecl(CSS_PROPERTY_align_self);

	if (cp)
	{
		OP_ASSERT(cp->GetDeclType() == CSS_DECL_TYPE);
		modes.SetAlignSelfCSSValue(cp->TypeValue());
	}

	// flex-direction

#ifdef WEBKIT_OLD_FLEXBOX
	if (is_oldspec_box)
	{
		CSSValue box_direction = CSS_VALUE_normal;
		CSSValue box_orient = CSS_VALUE_inline_axis;

		cp = cssprops.GetDecl(CSS_PROPERTY__webkit_box_direction);

		if (cp)
		{
			OP_ASSERT(cp->GetDeclType() == CSS_DECL_TYPE);
			box_direction = cp->TypeValue();
		}

		cp = cssprops.GetDecl(CSS_PROPERTY__webkit_box_orient);

		if (cp)
		{
			OP_ASSERT(cp->GetDeclType() == CSS_DECL_TYPE);
			box_orient = cp->TypeValue();
		}

		// Note: box-direction:inherit is treated as box-direction:normal

		switch (box_orient)
		{
		case CSS_VALUE_inherit:
			modes.SetDirection(FlexboxModes::DIR_INHERIT);
			break;
		case CSS_VALUE_inline_axis:
		case CSS_VALUE_horizontal: // Will not become LTR, but follow inline direction
			if (box_direction == CSS_VALUE_reverse)
				modes.SetDirection(FlexboxModes::DIR_ROW_REVERSE);
			else
				modes.SetDirection(FlexboxModes::DIR_ROW);
			break;
		case CSS_VALUE_block_axis:
		case CSS_VALUE_vertical:
			if (box_direction == CSS_VALUE_reverse)
				modes.SetDirection(FlexboxModes::DIR_COLUMN_REVERSE);
			else
				modes.SetDirection(FlexboxModes::DIR_COLUMN);
			break;
		};
	}
	else
#endif // WEBKIT_OLD_FLEXBOX
	{
		cp = cssprops.GetDecl(CSS_PROPERTY_flex_direction);

		if (cp)
		{
			OP_ASSERT(cp->GetDeclType() == CSS_DECL_TYPE);
			modes.SetDirectionCSSValue(cp->TypeValue());
		}
	}

	// flex-wrap

#ifdef WEBKIT_OLD_FLEXBOX
	if (is_oldspec_box)
	{
		cp = cssprops.GetDecl(CSS_PROPERTY__webkit_box_lines);

		if (cp)
		{
			OP_ASSERT(cp->GetDeclType() == CSS_DECL_TYPE);

			switch (cp->TypeValue())
			{
			case CSS_VALUE_inherit: modes.SetWrap(FlexboxModes::WRAP_INHERIT); break;
			case CSS_VALUE_single: modes.SetWrap(FlexboxModes::WRAP_NOWRAP); break;
			case CSS_VALUE_multiple: modes.SetWrap(FlexboxModes::WRAP_WRAP); break;
			}
		}
	}
	else
#endif // WEBKIT_OLD_FLEXBOX
	{
		cp = cssprops.GetDecl(CSS_PROPERTY_flex_wrap);

		if (cp)
		{
			OP_ASSERT(cp->GetDeclType() == CSS_DECL_TYPE);
			modes.SetWrapCSSValue(cp->TypeValue());
		}
	}

	info.type = CSSPROPS_FLEX_MODES;
	value.flexbox_modes = modes;
}

/* static */ void
CssPropertyItem::AddReferences(CssPropertyItem *new_props, int prop_count)
{
	for (int i=0; i<prop_count; i++)
		if (new_props[i].info.decl_is_referenced)
			new_props[i].value.css_decl->Ref();
}

/* static */ void
CssPropertyItem::RemoveReferences(CssPropertyItem *new_props, int prop_count)
{
	for (int i=0; i<prop_count; i++)
		if (new_props[i].info.decl_is_referenced)
			new_props[i].value.css_decl->Unref();
}


/* static */ void CssPropertyItem::AssignCSSDeclaration(CssPropertiesItemType prop, CssPropertyItem* props_array, int &prop_count, CSS_decl *decl)
{
	OP_ASSERT(decl != NULL);

	props_array[prop_count].info.type = prop;

	if (decl->GetDeclType() == CSS_DECL_TYPE && decl->TypeValue() == CSS_VALUE_inherit)
		props_array[prop_count].info.is_inherit = 1;
	else
	{
		props_array[prop_count].info.decl_is_referenced = 1;
		props_array[prop_count].value.css_decl = decl;
	}

	props_array[++prop_count].Init();
}

OP_STATUS CssPropertyItem::LoadCssProperties(HTML_Element* elm, double runtime_ms, HLDocProfile* hld_profile, CSS_MediaType media_type, int* changes/*= NULL*/)
{
	OP_PROBE4(OP_PROBE_CSSPROPERTYITEM_LOADCSSPROPERTIES);

	if (changes)
		*changes = 0;

	// Don't load css properties for Markup::HTE_DOC_ROOT. Also, if props are dirty on the Markup::HTE_DOC_ROOT
	// element, it means reload properties for all elements. That will be handled in
	// LayoutWorkplace::ReloadCssProperties. If this call comes from somewhere else where reload
	// for all elements is not ensured, the MarkPropsClean below would have caused properties not
	// to be loaded for all elements in ReloadCssProperties where they should have.
	if (elm->Type() == Markup::HTE_DOC_ROOT)
		return OpStatus::OK;

	// You are not allowed to load css properties for an element
	// disconnected from the document tree.
	OP_ASSERT(elm->Parent() != NULL);

	elm->MarkPropsClean();

	if (elm->IsText())
		// Text nodes don't have properties.

		return OpStatus::OK;

	if (elm->GetInserted() == HE_INSERTED_BY_LAYOUT && !elm->GetIsPseudoElement() ||
#ifdef SVG_SUPPORT
		elm->GetInserted() == HE_INSERTED_BY_SVG ||
#endif // SVG_SUPPORT
		elm->GetInserted() == HE_INSERTED_BY_CSS_IMPORT)
		return OpStatus::OK;

	BOOL had_pseudo_elm = elm->HasBeforeOrAfter() || elm->HasFirstLetter() || elm->HasFirstLine();

	elm->ResetCheckForPseudoElm();
	elm->SetHasCursorSettings(FALSE);

	CSS_Properties cssprops;

	RETURN_IF_ERROR(hld_profile->GetCSSCollection()->GetProperties(elm, runtime_ms, &cssprops, media_type));

	Markup::Type elm_type = elm->Type();
#if defined(SVG_SUPPORT) || defined(CSS_SCROLLBARS_SUPPORT)
	NS_Type elm_ns = elm->GetNsType();
#endif

	CssPropertyItem* props_array = g_props_array;

	// Needed to make the memcmp below produce sane results
	op_memset(props_array, 0, sizeof(CssPropertyItem) * CSSPROPS_ITEM_COUNT);

	OP_ASSERT(CSSPROPS_ITEM_COUNT <= 256); // See CssPropertiesItemType.

	CSS_ValueEx length_value[4];

	CSS_decl* cp;

#ifdef CSS_SCROLLBARS_SUPPORT
	if (elm_type == Markup::HTE_BODY && !hld_profile->IsInStrictMode() && elm_ns == NS_HTML)
	{
		ScrollbarColors* colors = hld_profile->GetScrollbarColors();

		if ((cp = cssprops.GetDecl(CSS_PROPERTY_scrollbar_base_color)) != NULL)
			colors->SetBase(GetColorFromCssDecl(cp));

		if ((cp = cssprops.GetDecl(CSS_PROPERTY_scrollbar_face_color)) != NULL)
			colors->SetFace(GetColorFromCssDecl(cp));

		if ((cp = cssprops.GetDecl(CSS_PROPERTY_scrollbar_arrow_color)) != NULL)
			colors->SetArrow(GetColorFromCssDecl(cp));

		if ((cp = cssprops.GetDecl(CSS_PROPERTY_scrollbar_track_color)) != NULL)
			colors->SetTrack(GetColorFromCssDecl(cp));

		if ((cp = cssprops.GetDecl(CSS_PROPERTY_scrollbar_shadow_color)) != NULL)
			colors->SetShadow(GetColorFromCssDecl(cp));

		if ((cp = cssprops.GetDecl(CSS_PROPERTY_scrollbar_3dlight_color)) != NULL)
			colors->SetLight(GetColorFromCssDecl(cp));

		if ((cp = cssprops.GetDecl(CSS_PROPERTY_scrollbar_highlight_color)) != NULL)
			colors->SetHighlight(GetColorFromCssDecl(cp));

		if ((cp = cssprops.GetDecl(CSS_PROPERTY_scrollbar_darkshadow_color)) != NULL)
			colors->SetDarkShadow(GetColorFromCssDecl(cp));
	}
#endif // CSS_SCROLLBARS_SUPPORT

#ifdef _WML_SUPPORT_
	if ((cp = cssprops.GetDecl(CSS_PROPERTY__wap_input_required)) != NULL)
	{
		if (cp->GetDeclType() == CSS_DECL_TYPE)
		{
			if (cp->TypeValue() == CSS_VALUE_true)
			{
				if (elm->SetBoolAttr(WA_EMPTYOK, FALSE, NS_IDX_WML) < 0)
					return OpStatus::ERR_NO_MEMORY;
			}
			else if (cp->TypeValue() == CSS_VALUE_false)
			{
				if (elm->SetBoolAttr(WA_EMPTYOK, TRUE, NS_IDX_WML) < 0)
					return OpStatus::ERR_NO_MEMORY;
			}

			hld_profile->WMLInit();
			if (hld_profile->GetIsOutOfMemory())
				return OpStatus::ERR_NO_MEMORY;
		}
	}

	if ((cp = cssprops.GetDecl(CSS_PROPERTY__wap_input_format)) != NULL)
	{
		if (cp->GetDeclType() == CSS_DECL_STRING)
		{
			if (cp->StringValue())
			{
				uni_char* format = SetStringAttr(cp->StringValue(), uni_strlen(cp->StringValue()), FALSE);
				if (!format || elm->SetAttr(WA_FORMAT, ITEM_TYPE_STRING, format, TRUE, NS_IDX_WML) == -1)
				{
					OP_DELETEA(format);
					return OpStatus::ERR_NO_MEMORY;
				}

				hld_profile->WMLInit();
				if (hld_profile->GetIsOutOfMemory())
					return OpStatus::ERR_NO_MEMORY;
			}
		}
	}
#endif // _WML_SUPPORT_

	if (elm->IsMatchingType(Markup::HTE_INPUT, NS_HTML) || elm->IsMatchingType(Markup::HTE_TEXTAREA, NS_HTML))
	{
		BOOL format_string_exists = FALSE;
		cp = cssprops.GetDecl(CSS_PROPERTY_input_format);
#ifdef CSS_CHARACTER_TYPE_SUPPORT
		if (cp == NULL)
			cp = cssprops.GetDecl(CSS_PROPERTY_character_type);
#endif // CSS_CHARACTER_TYPE_SUPPORT
		if (cp != NULL)
		{
			if (cp->GetDeclType() == CSS_DECL_STRING && cp->StringValue())
			{
				format_string_exists = TRUE;
				uni_char* format = SetStringAttr(cp->StringValue(), uni_strlen(cp->StringValue()), FALSE);
				if (!format || elm->SetSpecialAttr(FORMS_ATTR_CSS_INPUT_FORMAT, ITEM_TYPE_STRING, format, TRUE, SpecialNs::NS_FORMS) == -1)
				{
					OP_DELETEA(format);
					return OpStatus::ERR_NO_MEMORY;
				}
			}
		}

		if (!format_string_exists)
		{
			elm->RemoveSpecialAttribute(FORMS_ATTR_CSS_INPUT_FORMAT, SpecialNs::NS_FORMS);
		}
	}

	// These values are kept until the end of the property loading to find out if
	// a container has changed from/to a shrink-to-fit container.
	unsigned int position = CSS_POSITION_static;
	unsigned int float_type = CSS_FLOAT_value_not_set;
	unsigned int display_type = CSS_DISPLAY_value_not_set;
	BOOL width_auto(TRUE), left_auto(TRUE), right_auto(TRUE);
	BOOL width_inherit(FALSE), left_inherit(FALSE), right_inherit(FALSE);

	/* The Init()s are moved so everytime prop_count is increased
	   the props_array[prop_count].Init() is called. */
	int prop_count = 0;
	props_array[prop_count].Init();

	// always set common property first
	// (need to know 'position' and 'float' property before calculating 'containing_block_width'
	// in GetCssProperties)

	if (cssprops.GetDecl(CSS_PROPERTY_text_align) ||
		cssprops.GetDecl(CSS_PROPERTY_text_decoration) ||
		cssprops.GetDecl(CSS_PROPERTY_float) ||
		cssprops.GetDecl(CSS_PROPERTY_display) ||
		cssprops.GetDecl(CSS_PROPERTY_visibility) ||
		cssprops.GetDecl(CSS_PROPERTY_white_space) ||
		cssprops.GetDecl(CSS_PROPERTY_clear) ||
		cssprops.GetDecl(CSS_PROPERTY_position))
	{
		props_array[prop_count].info.type = CSSPROPS_COMMON;

		// text-align
		cp = cssprops.GetDecl(CSS_PROPERTY_text_align);
		unsigned int text_align = CSS_TEXT_ALIGN_value_not_set;
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch (cp->TypeValue())
			{
			case CSS_VALUE_left:
				text_align = CSS_TEXT_ALIGN_left; break;
			case CSS_VALUE_right:
				text_align = CSS_TEXT_ALIGN_right; break;
			case CSS_VALUE_justify:
				text_align = CSS_TEXT_ALIGN_justify; break;
			case CSS_VALUE_center:
				text_align = CSS_TEXT_ALIGN_center; break;
			case CSS_VALUE_inherit:
				text_align = CSS_TEXT_ALIGN_inherit; break;
			case CSS_VALUE_default:
				text_align = CSS_TEXT_ALIGN_default; break;
			}
			if (cp->GetDefaultStyle())
			{
				props_array[prop_count].info.info2.extra = 1;
			}
		}
		props_array[prop_count].value.common_props.info.text_align = text_align;

		// text-decoration
		cp = cssprops.GetDecl(CSS_PROPERTY_text_decoration);
		props_array[prop_count].value.common_props.info.inherit_text_decoration = 0;
		props_array[prop_count].value.common_props.info.text_decoration_set = 0;
		unsigned int text_dec = CSS_TEXT_DECORATION_none;
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			int dtype = cp->TypeValue();
			switch (dtype)
			{
			case CSS_VALUE_inherit:
				props_array[prop_count].value.common_props.info.inherit_text_decoration = 1;
				break;
			case CSS_VALUE_none:
				text_dec = CSS_TEXT_DECORATION_none;
				props_array[prop_count].value.common_props.info.text_decoration_set = 1;
				break;
			default:
				if (dtype & CSS_TEXT_DECORATION_underline)
					text_dec |= TEXT_DECORATION_UNDERLINE;
				if (dtype & CSS_TEXT_DECORATION_overline)
					text_dec |= TEXT_DECORATION_OVERLINE;
				if (dtype & CSS_TEXT_DECORATION_line_through)
					text_dec |= TEXT_DECORATION_LINETHROUGH;
				if (dtype & CSS_TEXT_DECORATION_blink)
					text_dec |= TEXT_DECORATION_BLINK;
				if (text_dec)
					props_array[prop_count].value.common_props.info.text_decoration_set = 1;
			}
		}
		props_array[prop_count].value.common_props.info.text_decoration = text_dec;

		// float
		cp = cssprops.GetDecl(CSS_PROPERTY_float);
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch (cp->TypeValue())
			{
			case CSS_VALUE_none:
				float_type = CSS_FLOAT_none; break;
			case CSS_VALUE_inherit:
				float_type = CSS_FLOAT_inherit; break;
			case CSS_VALUE_left:
				float_type = CSS_FLOAT_left; break;
			case CSS_VALUE_right:
				float_type = CSS_FLOAT_right; break;
			case CSS_VALUE__o_top:
				float_type = CSS_FLOAT_top; break;
			case CSS_VALUE__o_bottom:
				float_type = CSS_FLOAT_bottom; break;
			case CSS_VALUE__o_top_corner:
				float_type = CSS_FLOAT_top_corner; break;
			case CSS_VALUE__o_bottom_corner:
				float_type = CSS_FLOAT_bottom_corner; break;
			case CSS_VALUE__o_top_next_page:
				float_type = CSS_FLOAT_top_next_page; break;
			case CSS_VALUE__o_bottom_next_page:
				float_type = CSS_FLOAT_bottom_next_page; break;
			case CSS_VALUE__o_top_corner_next_page:
				float_type = CSS_FLOAT_top_corner_next_page; break;
			case CSS_VALUE__o_bottom_corner_next_page:
				float_type = CSS_FLOAT_bottom_corner_next_page; break;
			}
		}
		props_array[prop_count].value.common_props.info.float_type = float_type;

		// display
		cp = cssprops.GetDecl(CSS_PROPERTY_display);
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch (cp->TypeValue())
			{
			case CSS_VALUE_none:
				display_type = CSS_DISPLAY_none; break;
			case CSS_VALUE_inline:
				display_type = CSS_DISPLAY_inline; break;
			case CSS_VALUE_block:
				display_type = CSS_DISPLAY_block; break;
			case CSS_VALUE_inline_block:
				display_type = CSS_DISPLAY_inline_block; break;
			case CSS_VALUE_list_item:
				display_type = CSS_DISPLAY_list_item; break;
			case CSS_VALUE_run_in:
				display_type = CSS_DISPLAY_run_in; break;
			case CSS_VALUE_compact:
				display_type = CSS_DISPLAY_compact; break;
			case CSS_VALUE_table:
				display_type = CSS_DISPLAY_table; break;
			case CSS_VALUE_inline_table:
				display_type = CSS_DISPLAY_inline_table; break;
			case CSS_VALUE_table_row_group:
				display_type = CSS_DISPLAY_table_row_group; break;
			case CSS_VALUE_table_header_group:
				display_type = CSS_DISPLAY_table_header_group; break;
			case CSS_VALUE_table_footer_group:
				display_type = CSS_DISPLAY_table_footer_group; break;
			case CSS_VALUE_table_row:
				display_type = CSS_DISPLAY_table_row; break;
			case CSS_VALUE_table_column_group:
				display_type = CSS_DISPLAY_table_column_group; break;
			case CSS_VALUE_table_column:
				display_type = CSS_DISPLAY_table_column; break;
			case CSS_VALUE_table_cell:
				display_type = CSS_DISPLAY_table_cell; break;
			case CSS_VALUE_table_caption:
				display_type = CSS_DISPLAY_table_caption; break;
			case CSS_VALUE__wap_marquee:
				display_type = CSS_DISPLAY_wap_marquee; break;
			case CSS_VALUE_flex:
				display_type = CSS_DISPLAY_flex; break;
			case CSS_VALUE_inline_flex:
				display_type = CSS_DISPLAY_inline_flex; break;
#ifdef WEBKIT_OLD_FLEXBOX
			case CSS_VALUE__webkit_box:
				display_type = CSS_DISPLAY__webkit_box; break;
			case CSS_VALUE__webkit_inline_box:
				display_type = CSS_DISPLAY__webkit_inline_box; break;
#endif // WEBKIT_OLD_FLEXBOX
			case CSS_VALUE_inherit:
				display_type = CSS_DISPLAY_inherit; break;
			}
		}
		props_array[prop_count].value.common_props.info.display_type = display_type;

		// visibility
		cp = cssprops.GetDecl(CSS_PROPERTY_visibility);
		unsigned int visibility = CSS_VISIBILITY_value_not_set;
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch (cp->TypeValue())
			{
			case CSS_VALUE_visible:
				visibility = CSS_VISIBILITY_visible; break;
			case CSS_VALUE_hidden:
				visibility = CSS_VISIBILITY_hidden; break;
			case CSS_VALUE_collapse:
				visibility = CSS_VISIBILITY_collapse; break;
			case CSS_VALUE_inherit:
				visibility = CSS_VISIBILITY_inherit; break;
			}
		}
		props_array[prop_count].value.common_props.info.visibility = visibility;

		// white-space
		cp = cssprops.GetDecl(CSS_PROPERTY_white_space);
		unsigned int white_space = CSS_WHITE_SPACE_value_not_set;
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch (cp->TypeValue())
			{
			case CSS_VALUE_normal:
				white_space = CSS_WHITE_SPACE_normal; break;
			case CSS_VALUE_pre:
				white_space = CSS_WHITE_SPACE_pre; break;
			case CSS_VALUE_nowrap:
				white_space = CSS_WHITE_SPACE_nowrap; break;
			case CSS_VALUE_pre_wrap:
				white_space = CSS_WHITE_SPACE_pre_wrap; break;
			case CSS_VALUE_pre_line:
				white_space = CSS_WHITE_SPACE_pre_line; break;
			case CSS_VALUE_inherit:
				white_space = CSS_WHITE_SPACE_inherit; break;
			}
		}
		props_array[prop_count].value.common_props.info.white_space = white_space;

		// clear
		cp = cssprops.GetDecl(CSS_PROPERTY_clear);
		unsigned int clear = CSS_CLEAR_value_not_set;
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch (cp->TypeValue())
			{
			case CSS_VALUE_none:
				clear = CSS_CLEAR_none; break;
			case CSS_VALUE_left:
				clear = CSS_CLEAR_left; break;
			case CSS_VALUE_right:
				clear = CSS_CLEAR_right; break;
			case CSS_VALUE_both:
				clear = CSS_CLEAR_both; break;
			case CSS_VALUE_inherit:
				clear = CSS_CLEAR_inherit; break;
			}
		}
		props_array[prop_count].value.common_props.info.clear = clear;

		// position
		cp = cssprops.GetDecl(CSS_PROPERTY_position);
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch (cp->TypeValue())
			{
			case CSS_VALUE_relative:
				position = CSS_POSITION_relative; break;
			case CSS_VALUE_absolute:
				position = CSS_POSITION_absolute; break;
			case CSS_VALUE_fixed:
				position = CSS_POSITION_fixed; break;
			case CSS_VALUE_inherit:
				position = CSS_POSITION_inherit; break;
			}
		}
		props_array[prop_count].value.common_props.info.position = position;

		props_array[++prop_count].Init();
	}

	// writing-system (internal property)
	// need to appear before font size and font family, since they might depend on script
	cp = cssprops.GetDecl(CSS_PROPERTY_writing_system);
	if (cp && cp->GetDeclType() == CSS_DECL_LONG)
	{
		props_array[prop_count].info.type = CSSPROPS_WRITING_SYSTEM;
		props_array[prop_count].value.long_val = cp->LongValue();
		props_array[++prop_count].Init();
	}

	if (cssprops.GetDecl(CSS_PROPERTY_font_size) ||
		cssprops.GetDecl(CSS_PROPERTY_font_weight) ||
		cssprops.GetDecl(CSS_PROPERTY_font_variant) ||
		cssprops.GetDecl(CSS_PROPERTY_font_style))
	{
		props_array[prop_count].info.info2.extra = 0;
		props_array[prop_count].info.info2.val_percentage = 0;
		props_array[prop_count].info.type = CSSPROPS_FONT;

		int font_size = CSS_FONT_SIZE_value_not_set;
		int font_weight = CSS_FONT_WEIGHT_value_not_set;
		int font_variant = CSS_FONT_VARIANT_value_not_set;
		int font_style = CSS_FONT_STYLE_value_not_set;

		FontAtt font_att;
		BOOL font_att_loaded = FALSE;

# define LOAD_SYSTEM_FONT(kw) (font_att_loaded = font_att_loaded || g_op_ui_info->GetUICSSFont(kw, font_att))

		// font-size
		cp = cssprops.GetDecl(CSS_PROPERTY_font_size);
		if (cp)
		{
			if (cp->GetDeclType() == CSS_DECL_NUMBER)
			{
				font_size = int(cp->GetNumberValue(0));
				unsigned int decimal = (unsigned int) OpRound(cp->GetNumberValue(0) * LAYOUT_FIXED_POINT_BASE - font_size * LAYOUT_FIXED_POINT_BASE);
				props_array[prop_count].info.info2.extra = ((decimal & LAYOUT_FIXED_POINT_BITS) | (cp->GetDefaultStyle() ? LAYOUT_FIXED_POINT_BITS + 1 : 0));

				int cp_value_type = cp->GetValueType(0);
				if (cp_value_type == CSS_PERCENTAGE)
				{
					props_array[prop_count].info.info2.val_percentage = 1;
				}
				else
				{
					props_array[prop_count].value.font_props.info.size_type = LengthTypeConvert(cp_value_type);
				}

				if (font_size > CSS_FONT_SIZE_MAX)
					font_size = CSS_FONT_SIZE_MAX;
			}
			else if (cp->GetDeclType() == CSS_DECL_TYPE)
			{
				short font_size_type = cp->TypeValue();
				if (font_size_type == CSS_VALUE_inherit)
					font_size = CSS_FONT_SIZE_inherit;
				else if (font_size_type == CSS_VALUE_smaller)
					font_size = CSS_FONT_SIZE_smaller;
				else if (font_size_type == CSS_VALUE_larger)
					font_size = CSS_FONT_SIZE_larger;
				else if (font_size_type == CSS_VALUE_use_lang_def)
				{
					font_size = CSS_FONT_SIZE_use_lang_def;
					props_array[prop_count].value.font_props.info.size_type = CSS_LENGTH_px;
				}
				else if (CSS_is_fontsize_val(font_size_type))
				{
					props_array[prop_count].value.font_props.info.size_type = CSS_LENGTH_px;
					int size = font_size_type - CSS_VALUE_xx_small;
					const WritingSystem::Script script =
#if defined(FONTSWITCHING)
						hld_profile->GetPreferredScript();
#else // FONTSWITCHING
						WritingSystem::Unknown;
#endif // FONTSWITCHING
					FontAtt* font = styleManager->GetStyle(Markup::HTE_DOC_ROOT)->GetPresentationAttr().GetPresentationFont(script).Font;
					OP_ASSERT(font);
					font_size = styleManager->GetFontSize(font, size);
				}
				else if (CSS_is_font_system_val(font_size_type))
				{
					if (LOAD_SYSTEM_FONT(font_size_type))
					{
						font_size = font_att.GetSize();
						props_array[prop_count].value.font_props.info.size_type = CSS_LENGTH_px;
					}
				}
			}
		}
		props_array[prop_count].value.font_props.info.size = font_size;

		// font-weight
		cp = cssprops.GetDecl(CSS_PROPERTY_font_weight);
		if (cp)
		{
			if (cp->GetDeclType() == CSS_DECL_NUMBER)
			{
				font_weight = (int) cp->GetNumberValue(0);
				if (font_weight < 1)
					font_weight = 1;
				else if (font_weight > 9)
					font_weight = 9;
				props_array[prop_count].value.font_props.info.weight = font_weight;
			}
			else if (cp->GetDeclType() == CSS_DECL_TYPE)
			{
				switch (cp->TypeValue())
				{
				case CSS_VALUE_bold:
					font_weight = CSS_FONT_WEIGHT_bold; break;
				case CSS_VALUE_normal:
					font_weight = CSS_FONT_WEIGHT_normal; break;
				case CSS_VALUE_bolder:
					font_weight = CSS_FONT_WEIGHT_bolder; break;
				case CSS_VALUE_lighter:
					font_weight = CSS_FONT_WEIGHT_lighter; break;
				case CSS_VALUE_inherit:
					font_weight = CSS_FONT_WEIGHT_inherit; break;
				default:
					if (CSS_is_font_system_val(cp->TypeValue()))
					{
						if (LOAD_SYSTEM_FONT(cp->TypeValue()))
						{
							if (font_att.GetWeight() >= 1 && font_att.GetWeight() <= 9)
								font_weight = font_att.GetWeight();
						}
					}
				}
			}
		}
		props_array[prop_count].value.font_props.info.weight = font_weight;

		// font-variant
		cp = cssprops.GetDecl(CSS_PROPERTY_font_variant);
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			short variant = cp->TypeValue();
			switch (variant)
			{
			case CSS_VALUE_normal:
				font_variant = CSS_FONT_VARIANT_normal; break;
			case CSS_VALUE_small_caps:
				font_variant = CSS_FONT_VARIANT_small_caps; break;
			default:
				if (CSS_is_font_system_val(variant) && LOAD_SYSTEM_FONT(variant))
					font_variant = font_att.GetSmallCaps() ? CSS_FONT_VARIANT_small_caps : CSS_FONT_VARIANT_normal;
			}
		}
		props_array[prop_count].value.font_props.info.variant = font_variant;

		// font-style
		cp = cssprops.GetDecl(CSS_PROPERTY_font_style);
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			short fstyle = cp->TypeValue();
			switch (fstyle)
			{
			case CSS_VALUE_inherit:
				font_style = CSS_FONT_STYLE_inherit; break;
			case CSS_VALUE_normal:
				font_style = CSS_FONT_STYLE_normal; break;
			case CSS_VALUE_italic:
				font_style = CSS_FONT_STYLE_italic; break;
			case CSS_VALUE_oblique:
				font_style = CSS_FONT_STYLE_oblique; break;
			default:
				if (CSS_is_font_system_val(fstyle) && LOAD_SYSTEM_FONT(fstyle))
					font_style = font_att.GetItalic() ? CSS_FONT_STYLE_italic : CSS_FONT_STYLE_normal;
			}
		}
		props_array[prop_count].value.font_props.info.style = font_style;
		props_array[++prop_count].Init();

# undef LOAD_SYSTEM_FONT
	}

	// font-family
	if (cssprops.GetDecl(CSS_PROPERTY_font_family))
	{
		props_array[prop_count].info.type = CSSPROPS_FONT_NUMBER;
		props_array[prop_count].value.font_family_props.info.type = CSS_FONT_FAMILY_value_not_set;

		short font_number = -1;

		// font-family
		cp = cssprops.GetDecl(CSS_PROPERTY_font_family);
		if (cp)
		{
			if (cp->GetDeclType() == CSS_DECL_GEN_ARRAY)
			{
				const CSS_generic_value* arr = cp->GenArrayValue();
				short arr_len = cp->ArrayValueLen();
				for (int i=0; i<arr_len; i++)
				{
					if (arr[i].value_type == CSS_IDENT)
					{
						if (arr[i].value.type & CSS_GENERIC_FONT_FAMILY)
						{
							const uni_char* family_name = CSS_GetKeywordString(arr[i].value.type & CSS_GENERIC_FONT_FAMILY_mask);
							StyleManager::GenericFont gf = StyleManager::GetGenericFont(family_name);
							if (gf != StyleManager::UNKNOWN)
							{
								props_array[prop_count].value.font_family_props.info.type = CSS_FONT_FAMILY_generic;
								font_number = (short)gf;
								break;
							}
						}
						else if (styleManager->HasFont(arr[i].value.type))
						{
							font_number = arr[i].value.type;
							break;
						}
					}
					else if (arr[i].value_type == CSS_STRING_LITERAL)
					{
						short fnum = styleManager->LookupFontNumber(hld_profile, arr[i].value.string, media_type, TRUE);
						if (fnum >= 0)
						{
							font_number = fnum;
							break;
						}
						else if (fnum == -2)
							props_array[prop_count].value.font_family_props.info.awaiting_webfont = 1;
					}
				}
			}
			else if (cp->GetDeclType() == CSS_DECL_TYPE)
			{
				if (cp->TypeValue() == CSS_VALUE_inherit)
					props_array[prop_count].value.font_family_props.info.type = CSS_FONT_FAMILY_inherit;
				else if (cp->TypeValue() == CSS_VALUE_use_lang_def)
					props_array[prop_count].value.font_family_props.info.type = CSS_FONT_FAMILY_use_lang_def;
				else
				{
					FontAtt font_att;
					if (g_op_ui_info->GetUICSSFont(cp->TypeValue(), font_att))
					{
						short font_num = styleManager->GetFontNumber(font_att.GetFaceName());
						if (font_num >= 0)
							font_number = font_num;
					}
				}
			}
		}

		if (font_number >= 0 && props_array[prop_count].value.font_family_props.info.type == CSS_FONT_FAMILY_value_not_set)
			props_array[prop_count].value.font_family_props.info.type = CSS_FONT_FAMILY_font_number;
		props_array[prop_count].value.font_family_props.info.font_number = (unsigned long)font_number;

		props_array[++prop_count].Init();
	}

	// column-fill, column-span
	// These must be set early, because column-span affects the containing block width
	if (cssprops.GetDecl(CSS_PROPERTY_column_fill) ||
		cssprops.GetDecl(CSS_PROPERTY_column_span))
	{
		props_array[prop_count].info.type = CSSPROPS_MULTI_PANE_PROPS;

		cp = cssprops.GetDecl(CSS_PROPERTY_column_fill);

		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
			if (cp->TypeValue() == CSS_VALUE_inherit)
				props_array[prop_count].value.multi_pane_props.info.column_fill = CSS_COLUMN_FILL_inherit;
			else
				if (cp->TypeValue() == CSS_VALUE_balance)
					props_array[prop_count].value.multi_pane_props.info.column_fill = CSS_COLUMN_FILL_balance;
				else
					props_array[prop_count].value.multi_pane_props.info.column_fill = CSS_COLUMN_FILL_auto;
		else
			props_array[prop_count].value.multi_pane_props.info.column_fill = CSS_COLUMN_FILL_value_not_set;

		props_array[prop_count].value.multi_pane_props.info.column_span = CSS_COLUMN_SPAN_value_not_set;
		cp = cssprops.GetDecl(CSS_PROPERTY_column_span);

		if (cp)
			if (cp->GetDeclType() == CSS_DECL_TYPE)
				switch (cp->TypeValue())
				{
				case CSS_VALUE_inherit:
					props_array[prop_count].value.multi_pane_props.info.column_span = CSS_COLUMN_SPAN_inherit;
					break;

				default:
					OP_ASSERT(!"Unexpected value");

					// fall-through

				case CSS_VALUE_none:
					props_array[prop_count].value.multi_pane_props.info.column_span = CSS_COLUMN_SPAN_none;
					break;

				case CSS_VALUE_all:
					props_array[prop_count].value.multi_pane_props.info.column_span = CSS_COLUMN_SPAN_all;
					break;
				}
			else
			{
				long value = cp->GetDeclType() == CSS_DECL_LONG ? (int) cp->LongValue() : (int) cp->GetNumberValue(0);

				if (value > CSS_COLUMN_SPAN_max_value)
					value = CSS_COLUMN_SPAN_max_value;

				props_array[prop_count].value.multi_pane_props.info.column_span = value;
			}

		props_array[++prop_count].Init();
	}

	if (	cssprops.GetDecl(CSS_PROPERTY_opacity)
		||  cssprops.GetDecl(CSS_PROPERTY__o_object_fit)
		||	cssprops.GetDecl(CSS_PROPERTY_overflow_wrap)
		||	cssprops.GetDecl(CSS_PROPERTY_box_decoration_break)
		||	cssprops.GetDecl(CSS_PROPERTY_overflow_x)
		||	cssprops.GetDecl(CSS_PROPERTY_overflow_y))
	{
		props_array[prop_count].info.type = CSSPROPS_OTHER2;
		unsigned int overflow_wrap = CSS_OVERFLOW_WRAP_value_not_set;
		unsigned int object_fit = CSS_OBJECT_FIT_value_not_set;
		unsigned int box_decoration_break = CSS_BOX_DECORATION_BREAK_value_not_set;

		// opacity
		if ((cp = cssprops.GetDecl(CSS_PROPERTY_opacity)) != NULL)
		{
			props_array[prop_count].value.other_props2.info.opacity_set = 1;
			if (cp->GetDeclType() == CSS_DECL_NUMBER)
			{
				unsigned long opacity;
				float fopacity = (float) (cp->GetNumberValue(0) * 255.0);
				if (fopacity < 0.0)
					opacity = 0;
				else if (fopacity > 255.0)
					opacity = 255;
				else
					opacity = (unsigned long)fopacity;

				props_array[prop_count].value.other_props2.info.opacity = opacity;
			}
			else if (cp->GetDeclType() == CSS_DECL_TYPE && cp->TypeValue() == CSS_VALUE_inherit)
			{
				props_array[prop_count].value.other_props2.info.inherit_opacity = 1;
			}
		}

		// overflow-wrap
		if ((cp = cssprops.GetDecl(CSS_PROPERTY_overflow_wrap)) != NULL &&
			cp->GetDeclType() == CSS_DECL_TYPE)
			switch (cp->TypeValue())
			{
			case CSS_VALUE_normal:
				overflow_wrap = CSS_OVERFLOW_WRAP_normal; break;
			case CSS_VALUE_break_word:
				overflow_wrap = CSS_OVERFLOW_WRAP_break_word; break;
			case CSS_VALUE_inherit:
				overflow_wrap = CSS_OVERFLOW_WRAP_inherit; break;
			}

		props_array[prop_count].value.other_props2.info.overflow_wrap = overflow_wrap;

		// object-fit
		if ((cp = cssprops.GetDecl(CSS_PROPERTY__o_object_fit)) != NULL &&
			cp->GetDeclType() == CSS_DECL_TYPE)
			switch (cp->TypeValue())
			{
			case CSS_VALUE_fill:
				object_fit = CSS_OBJECT_FIT_fill; break;
			case CSS_VALUE_contain:
				object_fit = CSS_OBJECT_FIT_contain; break;
			case CSS_VALUE_cover:
				object_fit = CSS_OBJECT_FIT_cover; break;
			case CSS_VALUE_none:
				object_fit = CSS_OBJECT_FIT_none; break;
			case CSS_VALUE_auto:
				object_fit = CSS_OBJECT_FIT_auto; break;
			case CSS_VALUE_inherit:
				object_fit = CSS_OBJECT_FIT_inherit; break;
			}

		props_array[prop_count].value.other_props2.info.object_fit = object_fit;

		// box-decoration-break
		cp = cssprops.GetDecl(CSS_PROPERTY_box_decoration_break);
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch (cp->TypeValue())
			{
			case CSS_VALUE_inherit:
				box_decoration_break = CSS_BOX_DECORATION_BREAK_inherit;
				break;
			case CSS_VALUE_clone:
				box_decoration_break = CSS_BOX_DECORATION_BREAK_clone;
				break;
			case CSS_VALUE_slice:
			default:
				box_decoration_break = CSS_BOX_DECORATION_BREAK_slice;
				break;
			}
		}
		props_array[prop_count].value.other_props2.info.box_decoration_break = box_decoration_break;

		unsigned int overflowx = CSS_OVERFLOW_value_not_set;
		unsigned int overflowy = CSS_OVERFLOW_value_not_set;

		// overflow-x
		cp = cssprops.GetDecl(CSS_PROPERTY_overflow_x);
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
			overflowx = GetOverflowValue(cp->TypeValue());

		// overflow-y
		cp = cssprops.GetDecl(CSS_PROPERTY_overflow_y);
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
			overflowy = GetOverflowValue(cp->TypeValue());

		props_array[prop_count].value.other_props2.info.overflowx = overflowx;
		props_array[prop_count].value.other_props2.info.overflowy = overflowy;

		props_array[++prop_count].Init();
	}

	// margin
	if (cssprops.GetDecl(CSS_PROPERTY_margin_top) ||
		cssprops.GetDecl(CSS_PROPERTY_margin_left) ||
		cssprops.GetDecl(CSS_PROPERTY_margin_right) ||
		cssprops.GetDecl(CSS_PROPERTY_margin_bottom))
	{
		length_value[0].css_decl = cssprops.GetDecl(CSS_PROPERTY_margin_left);
		length_value[1].css_decl = cssprops.GetDecl(CSS_PROPERTY_margin_top);
		length_value[2].css_decl = cssprops.GetDecl(CSS_PROPERTY_margin_right);
		length_value[3].css_decl = cssprops.GetDecl(CSS_PROPERTY_margin_bottom);

		SetLengthPropertyNew(elm, CSSPROPS_MARGIN, 4, length_value, FALSE, props_array, prop_count);
	}

	// padding
	if (cssprops.GetDecl(CSS_PROPERTY_padding_top) ||
		cssprops.GetDecl(CSS_PROPERTY_padding_left) ||
		cssprops.GetDecl(CSS_PROPERTY_padding_right) ||
		cssprops.GetDecl(CSS_PROPERTY_padding_bottom))
	{
		length_value[0].css_decl = cssprops.GetDecl(CSS_PROPERTY_padding_left);
		length_value[1].css_decl = cssprops.GetDecl(CSS_PROPERTY_padding_top);
		length_value[2].css_decl = cssprops.GetDecl(CSS_PROPERTY_padding_right);
		length_value[3].css_decl = cssprops.GetDecl(CSS_PROPERTY_padding_bottom);

		SetLengthPropertyNew(elm, CSSPROPS_PADDING, 4, length_value, FALSE, props_array, prop_count);
	}

	// left, right
	if (cssprops.GetDecl(CSS_PROPERTY_left) ||
		cssprops.GetDecl(CSS_PROPERTY_right))
	{
		length_value[0].css_decl = cssprops.GetDecl(CSS_PROPERTY_left);
		length_value[1].css_decl = cssprops.GetDecl(CSS_PROPERTY_right);

		if (length_value[0].css_decl)
			if (length_value[0].css_decl->GetDeclType() == CSS_DECL_TYPE)
			{
				if (length_value[0].css_decl->TypeValue() != CSS_VALUE_auto)
				{
					left_auto = FALSE;
					left_inherit = length_value[0].css_decl->TypeValue() == CSS_VALUE_inherit;
				}
			}
			else
				left_auto = FALSE;

		if (length_value[1].css_decl)
			if (length_value[1].css_decl->GetDeclType() == CSS_DECL_TYPE)
			{
				if (length_value[1].css_decl->TypeValue() != CSS_VALUE_auto)
				{
					right_auto = FALSE;
					right_inherit = length_value[1].css_decl->TypeValue() == CSS_VALUE_inherit;
				}
			}
			else
				right_auto = FALSE;

		SetLengthPropertyNew(elm, CSSPROPS_LEFT_RIGHT, 2, length_value, FALSE, props_array, prop_count);
	}

	// top, bottom
	if (cssprops.GetDecl(CSS_PROPERTY_top) ||
		cssprops.GetDecl(CSS_PROPERTY_bottom))
	{
		length_value[0].css_decl = cssprops.GetDecl(CSS_PROPERTY_top);
		length_value[1].css_decl = cssprops.GetDecl(CSS_PROPERTY_bottom);

		SetLengthPropertyNew(elm, CSSPROPS_TOP_BOTTOM, 2, length_value, FALSE, props_array, prop_count);
	}

	// width, height
	if (cssprops.GetDecl(CSS_PROPERTY_width) ||	cssprops.GetDecl(CSS_PROPERTY_height))
	{
		length_value[0].css_decl = cssprops.GetDecl(CSS_PROPERTY_width);
		length_value[1].css_decl = cssprops.GetDecl(CSS_PROPERTY_height);

		if (length_value[0].css_decl)
			if (length_value[0].css_decl->GetDeclType() == CSS_DECL_TYPE)
			{
				if (length_value[0].css_decl->TypeValue() != CSS_VALUE_auto)
				{
					width_auto = FALSE;
					width_inherit = length_value[0].css_decl->TypeValue() == CSS_VALUE_inherit;
				}
			}
			else
				width_auto = FALSE;

		SetLengthPropertyNew(elm, CSSPROPS_WIDTH_HEIGHT, 2, length_value, FALSE, props_array, prop_count);
	}

	// min-width, min-height
	if (cssprops.GetDecl(CSS_PROPERTY_min_width) ||
		cssprops.GetDecl(CSS_PROPERTY_min_height))
	{
		length_value[0].css_decl = cssprops.GetDecl(CSS_PROPERTY_min_width);
		length_value[1].css_decl = cssprops.GetDecl(CSS_PROPERTY_min_height);

		SetLengthPropertyNew(elm, CSSPROPS_MIN_WIDTH_HEIGHT, 2, length_value, FALSE, props_array, prop_count);
	}

	// max-width, max-height
	if (cssprops.GetDecl(CSS_PROPERTY_max_width) ||
		cssprops.GetDecl(CSS_PROPERTY_max_height))
	{
		length_value[0].css_decl = cssprops.GetDecl(CSS_PROPERTY_max_width);
		length_value[1].css_decl = cssprops.GetDecl(CSS_PROPERTY_max_height);

		SetLengthPropertyNew(elm, CSSPROPS_MAX_WIDTH_HEIGHT, 2, length_value, FALSE, props_array, prop_count);
	}

	// border-width
	if (cssprops.GetDecl(CSS_PROPERTY_border_top_width) ||
		cssprops.GetDecl(CSS_PROPERTY_border_left_width) ||
		cssprops.GetDecl(CSS_PROPERTY_border_right_width) ||
		cssprops.GetDecl(CSS_PROPERTY_border_bottom_width))
	{
		length_value[0].css_decl = cssprops.GetDecl(CSS_PROPERTY_border_left_width);
		length_value[1].css_decl = cssprops.GetDecl(CSS_PROPERTY_border_top_width);
		length_value[2].css_decl = cssprops.GetDecl(CSS_PROPERTY_border_right_width);
		length_value[3].css_decl = cssprops.GetDecl(CSS_PROPERTY_border_bottom_width);

		SetLengthPropertyNew(elm, CSSPROPS_BORDER_WIDTH, 4, length_value, FALSE, props_array, prop_count);
	}

	// border-color
	if (cssprops.GetDecl(CSS_PROPERTY_border_top_color) ||
		cssprops.GetDecl(CSS_PROPERTY_border_left_color) ||
		cssprops.GetDecl(CSS_PROPERTY_border_right_color) ||
		cssprops.GetDecl(CSS_PROPERTY_border_bottom_color))
	{
		COLORREF border_color[] = { COLORREF(USE_DEFAULT_COLOR),
		                            COLORREF(USE_DEFAULT_COLOR),
		                            COLORREF(USE_DEFAULT_COLOR),
		                            COLORREF(USE_DEFAULT_COLOR) };

		if (cssprops.GetDecl(CSS_PROPERTY_border_top_color))
			 border_color[CP_TOP] = GetColorFromCssDecl(cssprops.GetDecl(CSS_PROPERTY_border_top_color));
		if (cssprops.GetDecl(CSS_PROPERTY_border_left_color))
			 border_color[CP_LEFT] = GetColorFromCssDecl(cssprops.GetDecl(CSS_PROPERTY_border_left_color));
		if (cssprops.GetDecl(CSS_PROPERTY_border_right_color))
			 border_color[CP_RIGHT] = GetColorFromCssDecl(cssprops.GetDecl(CSS_PROPERTY_border_right_color));
		if (cssprops.GetDecl(CSS_PROPERTY_border_bottom_color))
			 border_color[CP_BOTTOM] = GetColorFromCssDecl(cssprops.GetDecl(CSS_PROPERTY_border_bottom_color));

		if (border_color[0] == border_color[1] &&
			border_color[0] == border_color[2] &&
			border_color[0] == border_color[3])
		{
			props_array[prop_count].info.type = CSSPROPS_BORDER_COLOR;
			props_array[prop_count].value.color_val = border_color[0];
			props_array[++prop_count].Init();
		}
		else
		{
			if (cssprops.GetDecl(CSS_PROPERTY_border_top_color))
			{
				props_array[prop_count].info.type = CSSPROPS_BORDER_TOP_COLOR;
				props_array[prop_count].value.color_val = border_color[CP_TOP];
				props_array[++prop_count].Init();
			}
			if (cssprops.GetDecl(CSS_PROPERTY_border_left_color))
			{
				props_array[prop_count].info.type = CSSPROPS_BORDER_LEFT_COLOR;
				props_array[prop_count].value.color_val = border_color[CP_LEFT];
				props_array[++prop_count].Init();
			}
			if (cssprops.GetDecl(CSS_PROPERTY_border_right_color))
			{
				props_array[prop_count].info.type = CSSPROPS_BORDER_RIGHT_COLOR;
				props_array[prop_count].value.color_val = border_color[CP_RIGHT];
				props_array[++prop_count].Init();
			}
			if (cssprops.GetDecl(CSS_PROPERTY_border_bottom_color))
			{
				props_array[prop_count].info.type = CSSPROPS_BORDER_BOTTOM_COLOR;
				props_array[prop_count].value.color_val = border_color[CP_BOTTOM];
				props_array[++prop_count].Init();
			}
		}
	}

	// border-style
	if (cssprops.GetDecl(CSS_PROPERTY_border_top_style) ||
		cssprops.GetDecl(CSS_PROPERTY_border_left_style) ||
		cssprops.GetDecl(CSS_PROPERTY_border_right_style) ||
		cssprops.GetDecl(CSS_PROPERTY_border_bottom_style))
	{
		int border_style[] = { CSS_BORDER_STYLE_value_not_set, CSS_BORDER_STYLE_value_not_set, CSS_BORDER_STYLE_value_not_set, CSS_BORDER_STYLE_value_not_set};

		if (cssprops.GetDecl(CSS_PROPERTY_border_top_style))
			 border_style[CP_TOP] = GetBorderStyleFromCssDecl(cssprops.GetDecl(CSS_PROPERTY_border_top_style));
		if (cssprops.GetDecl(CSS_PROPERTY_border_left_style))
			 border_style[CP_LEFT] = GetBorderStyleFromCssDecl(cssprops.GetDecl(CSS_PROPERTY_border_left_style));
		if (cssprops.GetDecl(CSS_PROPERTY_border_right_style))
			 border_style[CP_RIGHT] = GetBorderStyleFromCssDecl(cssprops.GetDecl(CSS_PROPERTY_border_right_style));
		if (cssprops.GetDecl(CSS_PROPERTY_border_bottom_style))
			 border_style[CP_BOTTOM] = GetBorderStyleFromCssDecl(cssprops.GetDecl(CSS_PROPERTY_border_bottom_style));

		if (border_style[0] == border_style[1] &&
			border_style[0] == border_style[2] &&
			border_style[0] == border_style[3])
		{
			props_array[prop_count].info.type = CSSPROPS_BORDER_STYLE;
			props_array[prop_count].value.border_props.info.style = border_style[0];
			props_array[++prop_count].Init();
		}
		else
		{
			if (cssprops.GetDecl(CSS_PROPERTY_border_top_style))
			{
				props_array[prop_count].info.type = CSSPROPS_BORDER_TOP_STYLE;
				props_array[prop_count].value.border_props.info.style = border_style[CP_TOP];
				props_array[++prop_count].Init();
			}
			if (cssprops.GetDecl(CSS_PROPERTY_border_left_style))
			{
				props_array[prop_count].info.type = CSSPROPS_BORDER_LEFT_STYLE;
				props_array[prop_count].value.border_props.info.style = border_style[CP_LEFT];
				props_array[++prop_count].Init();
			}
			if (cssprops.GetDecl(CSS_PROPERTY_border_right_style))
			{
				props_array[prop_count].info.type = CSSPROPS_BORDER_RIGHT_STYLE;
				props_array[prop_count].value.border_props.info.style = border_style[CP_RIGHT];
				props_array[++prop_count].Init();
			}
			if (cssprops.GetDecl(CSS_PROPERTY_border_bottom_style))
			{
				props_array[prop_count].info.type = CSSPROPS_BORDER_BOTTOM_STYLE;
				props_array[prop_count].value.border_props.info.style = border_style[CP_BOTTOM];
				props_array[++prop_count].Init();
			}
		}
	}

	// border-radius
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_border_top_right_radius)) != NULL)
	{
		length_value[0].css_decl = length_value[1].css_decl = cp;
		SetLengthPropertyNew(elm, CSSPROPS_BORDER_TOP_RIGHT_RADIUS, 2, length_value, FALSE, props_array, prop_count);
	}

	if ((cp = cssprops.GetDecl(CSS_PROPERTY_border_bottom_right_radius)) != NULL)
	{
		length_value[0].css_decl = length_value[1].css_decl = cp;
		SetLengthPropertyNew(elm, CSSPROPS_BORDER_BOTTOM_RIGHT_RADIUS, 2, length_value, FALSE, props_array, prop_count);
	}

	if ((cp = cssprops.GetDecl(CSS_PROPERTY_border_bottom_left_radius)) != NULL)
	{
		length_value[0].css_decl = length_value[1].css_decl = cp;
		SetLengthPropertyNew(elm, CSSPROPS_BORDER_BOTTOM_LEFT_RADIUS, 2, length_value, FALSE, props_array, prop_count);
	}

	if ((cp = cssprops.GetDecl(CSS_PROPERTY_border_top_left_radius)) != NULL)
	{
		length_value[0].css_decl = length_value[1].css_decl = cp;
		SetLengthPropertyNew(elm, CSSPROPS_BORDER_TOP_LEFT_RADIUS, 2, length_value, FALSE, props_array, prop_count);
	}

	// outline-width and outline-offset
	if (cssprops.GetDecl(CSS_PROPERTY_outline_width) || cssprops.GetDecl(CSS_PROPERTY_outline_offset))
	{
		length_value[0].css_decl = cssprops.GetDecl(CSS_PROPERTY_outline_width);
		length_value[1].css_decl = cssprops.GetDecl(CSS_PROPERTY_outline_offset);

		SetLengthPropertyNew(elm, CSSPROPS_OUTLINE_WIDTH_OFFSET, 2, length_value, FALSE, props_array, prop_count);
	}

	// outline-color
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_outline_color)) != NULL)
	{
		props_array[prop_count].info.type = CSSPROPS_OUTLINE_COLOR;
		props_array[prop_count].value.color_val = GetColorFromCssDecl(cp);
		props_array[++prop_count].Init();
	}

	// outline-style
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_outline_style)) != NULL)
	{
		props_array[prop_count].info.type = CSSPROPS_OUTLINE_STYLE;
		props_array[prop_count].value.border_props.info.style = GetBorderStyleFromCssDecl(cp);
		props_array[++prop_count].Init();
	}

	// column-width
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_column_width)) != NULL)
		if (cp->GetDeclType() != CSS_DECL_TYPE || cp->TypeValue() != CSS_VALUE_auto)
		{
			length_value[0].css_decl = cp;
			SetLengthPropertyNew(elm, CSSPROPS_COLUMN_WIDTH, 1, length_value, FALSE, props_array, prop_count);
		}

	// column-count
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_column_count)) != NULL)
		if (cp->GetDeclType() != CSS_DECL_TYPE || cp->TypeValue() != CSS_VALUE_auto)
		{
			props_array[prop_count].info.type = CSSPROPS_COLUMN_COUNT;

			if (cp->GetDeclType() == CSS_DECL_TYPE)
			{
				OP_ASSERT(cp->TypeValue() == CSS_VALUE_inherit);
				props_array[prop_count].info.is_inherit = 1;
			}
			else
			{
				props_array[prop_count].value.length_val.info.val = cp->GetDeclType() == CSS_DECL_LONG ? (int) cp->LongValue() : (int) cp->GetNumberValue(0);
				props_array[prop_count].value.length_val.info.val_type = LengthTypeConvert(cp->GetValueType(0));
			}

			props_array[++prop_count].Init();
		}

	// column-gap
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_column_gap)) != NULL)
		if (cp->GetDeclType()  != CSS_DECL_TYPE || cp->TypeValue() != CSS_VALUE_normal)
		{
			length_value[0].css_decl = cp;
			SetLengthPropertyNew(elm, CSSPROPS_COLUMN_GAP, 1, length_value, FALSE, props_array, prop_count);
		}

	// column-rule-width
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_column_rule_width)) != NULL)
		if (cp->GetDeclType()  != CSS_DECL_TYPE || cp->TypeValue() != CSS_VALUE_medium)
		{
			length_value[0].css_decl = cp;
			SetLengthPropertyNew(elm, CSSPROPS_COLUMN_RULE_WIDTH, 1, length_value, FALSE, props_array, prop_count);
		}

	// column-rule-color
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_column_rule_color)))
	{
		props_array[prop_count].info.type = CSSPROPS_COLUMN_RULE_COLOR;
		props_array[prop_count].value.color_val = GetColorFromCssDecl(cp);
		props_array[++prop_count].Init();
	}

	// column-rule-style
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_column_rule_style)))
	{
		props_array[prop_count].info.type = CSSPROPS_COLUMN_RULE_STYLE;
		props_array[prop_count].value.border_props.info.style = GetBorderStyleFromCssDecl(cp);
		props_array[++prop_count].Init();
	}

	// flex-grow
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_flex_grow)))
	{
#ifdef WEBKIT_OLD_FLEXBOX
		if (!IsOldSpecFlexboxItem(elm))
#endif // WEBKIT_OLD_FLEXBOX
		{
			BOOL initial = FALSE;

			OP_ASSERT(cp->GetDeclType() == CSS_DECL_TYPE || cp->GetDeclType() == CSS_DECL_NUMBER);

			if (cp->GetDeclType() == CSS_DECL_TYPE)
			{
				OP_ASSERT(cp->TypeValue() == CSS_VALUE_inherit || cp->TypeValue() == CSS_VALUE_initial);

				if (cp->TypeValue() == CSS_VALUE_inherit)
					props_array[prop_count].info.is_inherit = 1;
				else
					initial = TRUE;
			}
			else
				props_array[prop_count].value.float_val = cp->GetNumberValue(0);

			if (!initial)
			{
				props_array[prop_count].info.type = CSSPROPS_FLEX_GROW;
				props_array[++prop_count].Init();
			}
		}
	}

	// flex-shrink
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_flex_shrink)))
	{
#ifdef WEBKIT_OLD_FLEXBOX
		if (!IsOldSpecFlexboxItem(elm))
#endif // WEBKIT_OLD_FLEXBOX
		{
			BOOL initial = FALSE;

			OP_ASSERT(cp->GetDeclType() == CSS_DECL_TYPE || cp->GetDeclType() == CSS_DECL_NUMBER);

			if (cp->GetDeclType() == CSS_DECL_TYPE)
			{
				OP_ASSERT(cp->TypeValue() == CSS_VALUE_inherit || cp->TypeValue() == CSS_VALUE_initial);

				if (cp->TypeValue() == CSS_VALUE_inherit)
					props_array[prop_count].info.is_inherit = 1;
				else
					initial = TRUE;
			}
			else
				props_array[prop_count].value.float_val = cp->GetNumberValue(0);

			if (!initial)
			{
				props_array[prop_count].info.type = CSSPROPS_FLEX_SHRINK;
				props_array[++prop_count].Init();
			}
		}
	}

	// flex-basis
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_flex_basis)))
	{
#ifdef WEBKIT_OLD_FLEXBOX
		if (!IsOldSpecFlexboxItem(elm))
#endif // WEBKIT_OLD_FLEXBOX
			if (cp->GetDeclType() != CSS_DECL_TYPE ||
				cp->TypeValue() != CSS_VALUE_auto && cp->TypeValue() != CSS_VALUE_initial)
			{
				length_value[0].css_decl = cp;
				SetLengthPropertyNew(elm, CSSPROPS_FLEX_BASIS, 1, length_value, FALSE, props_array, prop_count);
			}
	}

#ifdef WEBKIT_OLD_FLEXBOX
	// -webkit-box-flex
	if ((cp = cssprops.GetDecl(CSS_PROPERTY__webkit_box_flex)) && IsOldSpecFlexboxItem(elm))
		// Convert to modern long-hand properties.

		if (cp->GetDeclType() == CSS_DECL_TYPE)
		{
			OP_ASSERT(cp->TypeValue() == CSS_VALUE_inherit);

			props_array[prop_count].info.type = CSSPROPS_FLEX_GROW;
			props_array[prop_count].info.is_inherit = 1;
			props_array[++prop_count].Init();

			props_array[prop_count].info.type = CSSPROPS_FLEX_SHRINK;
			props_array[prop_count].info.is_inherit = 1;
			props_array[++prop_count].Init();

			props_array[prop_count].info.type = CSSPROPS_FLEX_BASIS;
			props_array[prop_count].info.is_inherit = 1;
			props_array[++prop_count].Init();
		}
		else
		{
			OP_ASSERT(cp->GetDeclType() == CSS_DECL_NUMBER);

			props_array[prop_count].info.type = CSSPROPS_FLEX_GROW;
			props_array[prop_count].value.float_val = cp->GetNumberValue(0);
			props_array[++prop_count].Init();

			props_array[prop_count].info.type = CSSPROPS_FLEX_SHRINK;
			props_array[prop_count].value.float_val = cp->GetNumberValue(0);
			props_array[++prop_count].Init();

			props_array[prop_count].info.type = CSSPROPS_FLEX_BASIS;
			props_array[prop_count].value.length_val.info.val = LENGTH_VALUE_auto;
			props_array[++prop_count].Init();
		}
#endif // WEBKIT_OLD_FLEXBOX

	// order
	if (cssprops.GetDecl(CSS_PROPERTY_order)
#ifdef WEBKIT_OLD_FLEXBOX
		|| cssprops.GetDecl(CSS_PROPERTY__webkit_box_ordinal_group)
#endif // WEBKIT_OLD_FLEXBOX
		)
	{
#ifdef WEBKIT_OLD_FLEXBOX
		if (IsOldSpecFlexbox(cssprops))
			cp = cssprops.GetDecl(CSS_PROPERTY__webkit_box_ordinal_group);
		else
#endif // WEBKIT_OLD_FLEXBOX
			cp = cssprops.GetDecl(CSS_PROPERTY_order);

		if (cp)
		{
			props_array[prop_count].info.type = CSSPROPS_ORDER;
			OP_ASSERT(cp->GetDeclType() == CSS_DECL_TYPE || cp->GetDeclType() == CSS_DECL_LONG);

			if (cp->GetDeclType() == CSS_DECL_TYPE)
			{
				OP_ASSERT(cp->TypeValue() == CSS_VALUE_inherit);
				props_array[prop_count].info.is_inherit = 1;
			}
			else
				props_array[prop_count].value.long_val = cp->LongValue();

			props_array[++prop_count].Init();
		}
	}

	// justify-content, align-content, align-items, align-self, flex-direction, flex-wrap
	if (
#ifdef WEBKIT_OLD_FLEXBOX
		cssprops.GetDecl(CSS_PROPERTY__webkit_box_align) ||
		cssprops.GetDecl(CSS_PROPERTY__webkit_box_direction) ||
		cssprops.GetDecl(CSS_PROPERTY__webkit_box_lines) ||
		cssprops.GetDecl(CSS_PROPERTY__webkit_box_orient) ||
		cssprops.GetDecl(CSS_PROPERTY__webkit_box_pack) ||
#endif // WEBKIT_OLD_FLEXBOX
		cssprops.GetDecl(CSS_PROPERTY_justify_content) ||
		cssprops.GetDecl(CSS_PROPERTY_align_content) ||
		cssprops.GetDecl(CSS_PROPERTY_align_items) ||
		cssprops.GetDecl(CSS_PROPERTY_align_self) ||
		cssprops.GetDecl(CSS_PROPERTY_flex_direction) ||
		cssprops.GetDecl(CSS_PROPERTY_flex_wrap))
	{
		props_array[prop_count].SetFlexModes(cssprops, elm);
		props_array[++prop_count].Init();
	}

	// cursor
	if (cssprops.GetDecl(CSS_PROPERTY_cursor))
	{
		//props_array[prop_count].Init();
		cp = cssprops.GetDecl(CSS_PROPERTY_cursor);
		props_array[prop_count].info.type = CSSPROPS_CURSOR;
		if (cp->GetDeclType() == CSS_DECL_TYPE)
		{
			if (cp->TypeValue() == CSS_VALUE_inherit)
			{
				props_array[prop_count].info.is_inherit = 1; // keyword value 'inherit'
				props_array[++prop_count].Init();
				elm->SetHasCursorSettings(FALSE);
			}
			else if (cp->TypeValue() != CSS_VALUE_auto)
			{
				props_array[prop_count].info.info2.extra = GetCursorValue(cp->TypeValue());
				props_array[++prop_count].Init();
				elm->SetHasCursorSettings(TRUE);
			}
		}
        else if (cp->GetDeclType() == CSS_DECL_GEN_ARRAY)
        {
            const CSS_generic_value* array = cp->GenArrayValue();
            short array_len = cp->ArrayValueLen();
            // the last value is the only one we can use because all the others are URLs
            if (array[array_len - 1].value.type == CSS_VALUE_inherit)
			{
				props_array[prop_count].info.is_inherit = 1; // keyword value 'inherit'
				props_array[++prop_count].Init();
				elm->SetHasCursorSettings(TRUE);
			}
			else if (array[array_len - 1].value.type != CSS_VALUE_auto)
			{
				props_array[prop_count].info.info2.extra = GetCursorValue(array[array_len - 1].value.type);
				props_array[++prop_count].Init();
				elm->SetHasCursorSettings(TRUE);
			}
        }
		else
		{
			AssignCSSDeclaration(CSSPROPS_CURSOR, props_array, prop_count, cp);
		}
	}

	// background-color
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_background_color)) != NULL)
	{
		props_array[prop_count].info.type = CSSPROPS_BG_COLOR;

		CSSDeclType type = cp->GetDeclType();
		if (type == CSS_DECL_LONG || type == CSS_DECL_COLOR)
		{
			props_array[prop_count].value.color_val = cp->LongValue();
			if (type == CSS_DECL_COLOR)
				props_array[prop_count].info.info2.extra = 1;
			props_array[++prop_count].Init();
		}
		else if (type == CSS_DECL_TYPE)
		{
			short type_value = cp->TypeValue();
			if (type_value == CSS_VALUE_inherit)
				props_array[prop_count].value.color_val = COLORREF(CSS_COLOR_inherit);
			else if (type_value == CSS_VALUE_transparent)
				props_array[prop_count].value.color_val = COLORREF(CSS_COLOR_transparent);
			else if (type_value == CSS_VALUE_currentColor)
				props_array[prop_count].value.color_val = COLORREF(CSS_COLOR_current_color);
			else
			{
				OP_ASSERT(FALSE);
			}
			props_array[++prop_count].Init();
		}
	}

	// background-position
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_background_position)) != NULL)
		AssignCSSDeclaration(CSSPROPS_BG_POSITION, props_array, prop_count, cp);

	// background-repeat
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_background_repeat)) != NULL)
		AssignCSSDeclaration(CSSPROPS_BG_REPEAT, props_array, prop_count, cp);

	// background-attachment
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_background_attachment)) != NULL)
		AssignCSSDeclaration(CSSPROPS_BG_ATTACH, props_array, prop_count, cp);

	// background-origin
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_background_origin)) != NULL)
		AssignCSSDeclaration(CSSPROPS_BG_ORIGIN, props_array, prop_count, cp);

	// background-clip
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_background_clip)) != NULL)
		AssignCSSDeclaration(CSSPROPS_BG_CLIP, props_array, prop_count, cp);

	// background-size
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_background_size)) != NULL)
		AssignCSSDeclaration(CSSPROPS_BG_SIZE, props_array, prop_count, cp);

	// object-position
	if ((cp = cssprops.GetDecl(CSS_PROPERTY__o_object_position)) != NULL)
		AssignCSSDeclaration(CSSPROPS_OBJECT_POSITION, props_array, prop_count, cp);

	// color
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_color)) != NULL ||
		(cp = cssprops.GetDecl(CSS_PROPERTY_font_color)) != NULL)
	{
		props_array[prop_count].info.type = CSSPROPS_COLOR;

		CSSDeclType type = cp->GetDeclType();

		if (type == CSS_DECL_LONG)
		{
			if (cp->GetProperty() == CSS_PROPERTY_font_color && !hld_profile->IsInStrictMode())
				props_array[prop_count].info.info2.extra = 2;
			props_array[prop_count].value.color_val = (COLORREF)cp->LongValue();
			props_array[++prop_count].Init();
		}
        else if (type == CSS_DECL_COLOR)
        {
			props_array[prop_count].value.color_val = (COLORREF)cp->LongValue();
            props_array[prop_count].info.info2.extra = 1;
			props_array[++prop_count].Init();
        }
		else if (type == CSS_DECL_TYPE)
		{
			short type_value = cp->TypeValue();
			if (type_value == CSS_VALUE_inherit)
			{
				props_array[prop_count].value.color_val = COLORREF(CSS_COLOR_inherit);
				props_array[++prop_count].Init();
			}
			else if (type_value == CSS_VALUE_transparent)
			{
				props_array[prop_count].value.color_val = COLORREF(CSS_COLOR_transparent);
				props_array[++prop_count].Init();
			}
		}
#ifdef SKIN_SUPPORT
		else if (type == CSS_DECL_STRING)
		{
			props_array[prop_count].value.color_val = GetColorFromCssDecl(cp);
			props_array[++prop_count].Init();
		}
#endif // SKIN_SUPPORT
	}

	if (cssprops.GetDecl(CSS_PROPERTY_text_transform) ||
		cssprops.GetDecl(CSS_PROPERTY_table_layout) ||
		cssprops.GetDecl(CSS_PROPERTY_caption_side) ||
		cssprops.GetDecl(CSS_PROPERTY_border_collapse) ||
		cssprops.GetDecl(CSS_PROPERTY_empty_cells) ||
#ifdef SUPPORT_TEXT_DIRECTION
		cssprops.GetDecl(CSS_PROPERTY_direction) ||
		cssprops.GetDecl(CSS_PROPERTY_unicode_bidi) ||
#endif
		cssprops.GetDecl(CSS_PROPERTY_list_style_position) ||
		cssprops.GetDecl(CSS_PROPERTY_list_style_type) ||
		cssprops.GetDecl(CSS_PROPERTY_text_overflow) ||
		cssprops.GetDecl(CSS_PROPERTY_resize) ||
		cssprops.GetDecl(CSS_PROPERTY_box_sizing))
	{
		props_array[prop_count].info.type = CSSPROPS_OTHER;

		// text-transform
		cp = cssprops.GetDecl(CSS_PROPERTY_text_transform);
		unsigned int text_transform = CSS_TEXT_TRANSFORM_value_not_set;
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch (cp->TypeValue())
			{
			case CSS_VALUE_none:
				text_transform = CSS_TEXT_TRANSFORM_none; break;
			case CSS_VALUE_capitalize:
				text_transform = CSS_TEXT_TRANSFORM_capitalize; break;
			case CSS_VALUE_uppercase:
				text_transform = CSS_TEXT_TRANSFORM_uppercase; break;
			case CSS_VALUE_lowercase:
				text_transform = CSS_TEXT_TRANSFORM_lowercase; break;
			case CSS_VALUE_inherit:
				text_transform = CSS_TEXT_ALIGN_inherit; break;
			}
		}
		props_array[prop_count].value.other_props.info.text_transform = text_transform;

		// table-layout
		cp = cssprops.GetDecl(CSS_PROPERTY_table_layout);
		unsigned long table_layout = CSS_TABLE_LAYOUT_auto;
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch (cp->TypeValue())
			{
			case CSS_VALUE_inherit:
				table_layout = CSS_TABLE_LAYOUT_inherit; break;
			case CSS_VALUE_fixed:
				table_layout = CSS_TABLE_LAYOUT_fixed; break;
			}
		}
		props_array[prop_count].value.other_props.info.table_layout = table_layout;

		// caption-side
		cp = cssprops.GetDecl(CSS_PROPERTY_caption_side);
		unsigned long caption_side = CSS_CAPTION_SIDE_value_not_set;
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch (cp->TypeValue())
			{
			case CSS_VALUE_top:
				caption_side = CSS_CAPTION_SIDE_top; break;
			case CSS_VALUE_bottom:
				caption_side = CSS_CAPTION_SIDE_bottom; break;
			case CSS_VALUE_left:
				caption_side = CSS_CAPTION_SIDE_left; break;
			case CSS_VALUE_right:
				caption_side = CSS_CAPTION_SIDE_right; break;
			case CSS_VALUE_inherit:
				caption_side = CSS_CAPTION_SIDE_inherit; break;
			}
		}
		props_array[prop_count].value.other_props.info.caption_side = caption_side;

		// border-collapse
		cp = cssprops.GetDecl(CSS_PROPERTY_border_collapse);
		unsigned long border_collapse = CSS_BORDER_COLLAPSE_value_not_set;
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch (cp->TypeValue())
			{
			case CSS_VALUE_collapse:
				border_collapse = CSS_BORDER_COLLAPSE_collapse; break;
			case CSS_VALUE_separate:
				border_collapse = CSS_BORDER_COLLAPSE_separate; break;
			case CSS_VALUE_inherit:
				border_collapse = CSS_BORDER_COLLAPSE_inherit; break;
			}
		}
		props_array[prop_count].value.other_props.info.border_collapse = border_collapse;

		// empty-cells
		cp = cssprops.GetDecl(CSS_PROPERTY_empty_cells);
		unsigned long empty_cells = CSS_EMPTY_CELLS_inherit;
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch (cp->TypeValue())
			{
			case CSS_VALUE_show:
				empty_cells = CSS_EMPTY_CELLS_show; break;
			case CSS_VALUE_hide:
				empty_cells = CSS_EMPTY_CELLS_hide; break;
			}
		}

		props_array[prop_count].value.other_props.info.empty_cells = empty_cells;

#ifdef SUPPORT_TEXT_DIRECTION
		// direction
		cp = cssprops.GetDecl(CSS_PROPERTY_direction);
		unsigned long direction = CSS_DIRECTION_value_not_set;
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch (cp->TypeValue())
			{
			case CSS_VALUE_ltr:
				direction = CSS_DIRECTION_ltr; break;
			case CSS_VALUE_rtl:
				direction = CSS_DIRECTION_rtl; break;
			case CSS_VALUE_inherit:
				direction = CSS_DIRECTION_inherit; break;
			}
		}
		props_array[prop_count].value.other_props.info.direction = direction;

		// unicode-bidi
		cp = cssprops.GetDecl(CSS_PROPERTY_unicode_bidi);
		unsigned long unicode_bidi = CSS_UNICODE_BIDI_value_not_set;
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch (cp->TypeValue())
			{
			case CSS_VALUE_embed:
				unicode_bidi = CSS_UNICODE_BIDI_embed; break;
			case CSS_VALUE_bidi_override:
				unicode_bidi = CSS_UNICODE_BIDI_bidi_override; break;
			case CSS_VALUE_inherit:
				unicode_bidi = CSS_UNICODE_BIDI_inherit; break;
			case CSS_VALUE_normal:
				unicode_bidi = CSS_UNICODE_BIDI_normal; break;
			}
		}
		props_array[prop_count].value.other_props.info.unicode_bidi = unicode_bidi;
#endif

		// list-style-position
		cp = cssprops.GetDecl(CSS_PROPERTY_list_style_position);
		unsigned long list_style_pos = CSS_LIST_STYLE_POS_value_not_set;
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch (cp->TypeValue())
			{
			case CSS_VALUE_inside:
				list_style_pos = CSS_LIST_STYLE_POS_inside; break;
			case CSS_VALUE_outside:
				list_style_pos = CSS_LIST_STYLE_POS_outside; break;
			case CSS_VALUE_inherit:
				list_style_pos = CSS_LIST_STYLE_POS_inherit; break;
			}
		}
		props_array[prop_count].value.other_props.info.list_style_pos = list_style_pos;

		// list-style-type
		cp = cssprops.GetDecl(CSS_PROPERTY_list_style_type);
		unsigned long list_style_type = CSS_LIST_STYLE_TYPE_value_not_set;
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch (cp->TypeValue())
			{
			case CSS_VALUE_inherit:
				list_style_type = CSS_LIST_STYLE_TYPE_inherit; break;
			case CSS_VALUE_disc:
				list_style_type = CSS_LIST_STYLE_TYPE_disc; break;
			case CSS_VALUE_circle:
				list_style_type = CSS_LIST_STYLE_TYPE_circle; break;
			case CSS_VALUE_square:
				list_style_type = CSS_LIST_STYLE_TYPE_square; break;
			case CSS_VALUE_box:
				list_style_type = CSS_LIST_STYLE_TYPE_box; break;
			case CSS_VALUE_decimal:
				list_style_type = CSS_LIST_STYLE_TYPE_decimal; break;
			case CSS_VALUE_decimal_leading_zero:
				list_style_type = CSS_LIST_STYLE_TYPE_decimal_leading_zero; break;
			case CSS_VALUE_lower_roman:
				list_style_type = CSS_LIST_STYLE_TYPE_lower_roman; break;
			case CSS_VALUE_upper_roman:
				list_style_type = CSS_LIST_STYLE_TYPE_upper_roman; break;
			case CSS_VALUE_lower_greek:
				list_style_type = CSS_LIST_STYLE_TYPE_lower_greek; break;
			case CSS_VALUE_lower_alpha:
				list_style_type = CSS_LIST_STYLE_TYPE_lower_alpha; break;
			case CSS_VALUE_lower_latin:
				list_style_type = CSS_LIST_STYLE_TYPE_lower_latin; break;
			case CSS_VALUE_upper_alpha:
				list_style_type = CSS_LIST_STYLE_TYPE_upper_alpha; break;
			case CSS_VALUE_upper_latin:
				list_style_type = CSS_LIST_STYLE_TYPE_upper_latin; break;
			case CSS_VALUE_hebrew:
				list_style_type = CSS_LIST_STYLE_TYPE_hebrew; break;
			case CSS_VALUE_armenian:
				list_style_type = CSS_LIST_STYLE_TYPE_armenian; break;
			case CSS_VALUE_georgian:
				list_style_type = CSS_LIST_STYLE_TYPE_georgian; break;
			case CSS_VALUE_cjk_ideographic:
				list_style_type = CSS_LIST_STYLE_TYPE_cjk_ideographic; break;
			case CSS_VALUE_hiragana:
				list_style_type = CSS_LIST_STYLE_TYPE_hiragana; break;
			case CSS_VALUE_katakana:
				list_style_type = CSS_LIST_STYLE_TYPE_katakana; break;
			case CSS_VALUE_hiragana_iroha:
				list_style_type = CSS_LIST_STYLE_TYPE_hiragana_iroha; break;
			case CSS_VALUE_katakana_iroha:
				list_style_type = CSS_LIST_STYLE_TYPE_katakana_iroha; break;
			case CSS_VALUE_none:
				list_style_type = CSS_LIST_STYLE_TYPE_none; break;
			}
		}
		props_array[prop_count].value.other_props.info.list_style_type = list_style_type;

		// text-overflow
		cp = cssprops.GetDecl(CSS_PROPERTY_text_overflow);
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch(cp->TypeValue())
			{
			case CSS_VALUE_clip:
				props_array[prop_count].value.other_props.info.text_overflow = CSS_TEXT_OVERFLOW_clip; break;
			case CSS_VALUE_ellipsis:
				props_array[prop_count].value.other_props.info.text_overflow = CSS_TEXT_OVERFLOW_ellipsis; break;
			case CSS_VALUE__o_ellipsis_lastline:
				props_array[prop_count].value.other_props.info.text_overflow = CSS_TEXT_OVERFLOW_ellipsis_lastline; break;
			case CSS_VALUE_inherit:
				props_array[prop_count].value.other_props.info.text_overflow = CSS_TEXT_OVERFLOW_inherit; break;
			}
		}

		// resize
		cp = cssprops.GetDecl(CSS_PROPERTY_resize);
		unsigned int resize = CSS_RESIZE_value_not_set;
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch (cp->TypeValue())
			{
			case CSS_VALUE_none:
				resize = CSS_RESIZE_none; break;
			case CSS_VALUE_both:
				resize = CSS_RESIZE_both; break;
			case CSS_VALUE_horizontal:
				resize = CSS_RESIZE_horizontal; break;
			case CSS_VALUE_vertical:
				resize = CSS_RESIZE_vertical; break;
			case CSS_VALUE_inherit:
				resize = CSS_RESIZE_inherit; break;
			}
		}
		props_array[prop_count].value.other_props.info.resize = resize;

		// box-sizing
		cp = cssprops.GetDecl(CSS_PROPERTY_box_sizing);
		unsigned int box_sizing = CSS_BOX_SIZING_value_not_set;

		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch (cp->TypeValue())
			{
			case CSS_VALUE_content_box:
				box_sizing = CSS_BOX_SIZING_content_box; break;
			case CSS_VALUE_border_box:
				box_sizing = CSS_BOX_SIZING_border_box; break;
			case CSS_VALUE_inherit:
				box_sizing = CSS_BOX_SIZING_inherit; break;
			}
		}
		props_array[prop_count].value.other_props.info.box_sizing = box_sizing;

		props_array[++prop_count].Init();
	}

	// border-spacing
	if (cssprops.GetDecl(CSS_PROPERTY_border_spacing))
	{
		length_value[0].css_decl = cssprops.GetDecl(CSS_PROPERTY_border_spacing);
		if (length_value[0].css_decl->GetDeclType() == CSS_DECL_TYPE && length_value[0].css_decl->TypeValue() == CSS_VALUE_inherit)
			length_value[1].css_decl = NULL;
		else
			if (length_value[0].css_decl->GetDeclType() == CSS_DECL_NUMBER || length_value[0].css_decl->GetDeclType() == CSS_DECL_NUMBER2)
				length_value[1].css_decl = length_value[0].css_decl;

		SetLengthPropertyNew(elm, CSSPROPS_BORDER_SPACING, 2, length_value, FALSE, props_array, prop_count);
	}

	// text-indent
	if (cssprops.GetDecl(CSS_PROPERTY_text_indent) && cssprops.GetDecl(CSS_PROPERTY_text_indent)->GetDeclType() == CSS_DECL_NUMBER)
	{
		length_value[0].css_decl = cssprops.GetDecl(CSS_PROPERTY_text_indent);
		SetLengthPropertyNew(elm, CSSPROPS_TEXT_INDENT, 1, length_value, FALSE, props_array, prop_count);
	}

	// z-index
	if (cssprops.GetDecl(CSS_PROPERTY_z_index))
	{
		SetCssZIndex(elm, CSS_Value(cssprops.GetDecl(CSS_PROPERTY_z_index), CSS_VALUE_auto), &props_array[prop_count]);
		props_array[++prop_count].Init();
	}

	// clip
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_clip)) != NULL)
		AssignCSSDeclaration(CSSPROPS_CLIP, props_array, prop_count, cp);

	// letter-spacing, word-spacing
	if (cssprops.GetDecl(CSS_PROPERTY_letter_spacing) || cssprops.GetDecl(CSS_PROPERTY_word_spacing))
	{
		length_value[0].css_decl = cssprops.GetDecl(CSS_PROPERTY_word_spacing);
		length_value[1].css_decl = cssprops.GetDecl(CSS_PROPERTY_letter_spacing);

		SetLengthPropertyNew(elm, CSSPROPS_LETTER_WORD_SPACING, 2, length_value, FALSE, props_array, prop_count);
	}

	// line-height, vertical-align
	if (cssprops.GetDecl(CSS_PROPERTY_line_height) || cssprops.GetDecl(CSS_PROPERTY_vertical_align))
	{
		length_value[0].css_decl = cssprops.GetDecl(CSS_PROPERTY_line_height);
		length_value[1].css_decl = cssprops.GetDecl(CSS_PROPERTY_vertical_align);

		SetLengthPropertyNew(elm, CSSPROPS_LINE_HEIGHT_VALIGN, 2, length_value, FALSE, props_array, prop_count);
	}

	// content
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_content)) != NULL)

		/* Non existing property means keyword value 'auto' or
		   'normal' which is default value.  If keyword value is
		   'none' pointer to css_decl is stored but no content will be
		   generated by layout engine. */

		AssignCSSDeclaration(CSSPROPS_CONTENT, props_array, prop_count, cp);

	// counter-increment
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_counter_increment)) != NULL)
		AssignCSSDeclaration(CSSPROPS_COUNTER_INCREMENT, props_array, prop_count, cp);

	// counter-reset
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_counter_reset)) != NULL)
		AssignCSSDeclaration(CSSPROPS_COUNTER_RESET, props_array, prop_count, cp);

	// quotes
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_quotes)) != NULL)
	{
		if (cp->GetDeclType() != CSS_DECL_TYPE || cp->TypeValue() == CSS_VALUE_none)
			AssignCSSDeclaration(CSSPROPS_QUOTES, props_array, prop_count, cp);

		// non existing property means keyword value 'inherit' which is default value
	}

#ifdef GADGET_SUPPORT
    // control regions
    if (cssprops.GetDecl(CSS_PROPERTY__apple_dashboard_region))
    {
        props_array[prop_count].info.type = CSSPROPS_CONTROL_REGION;
        cp = cssprops.GetDecl(CSS_PROPERTY__apple_dashboard_region);
        if (cp->GetDeclType() != CSS_DECL_TYPE || cp->TypeValue() == CSS_VALUE_none)
			AssignCSSDeclaration(CSSPROPS_CONTROL_REGION, props_array, prop_count, cp);
    }
#endif // GADGET_SUPPORT

	// text-shadow
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_text_shadow)) != NULL)
		AssignCSSDeclaration(CSSPROPS_TEXT_SHADOW, props_array, prop_count, cp);

	// box-shadow
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_box_shadow)) != NULL)
		AssignCSSDeclaration(CSSPROPS_BOX_SHADOW, props_array, prop_count, cp);

    // border-image
    if ((cp = cssprops.GetDecl(CSS_PROPERTY__o_border_image)) != NULL)
		AssignCSSDeclaration(CSSPROPS_BORDER_IMAGE, props_array, prop_count, cp);

	// background-image
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_background_image)) != NULL)
	{
		BOOL has_bg_image = FALSE;

		if (cp->GetDeclType() == CSS_DECL_TYPE && cp->TypeValue() == CSS_VALUE_inherit)
			has_bg_image = TRUE;
		else if (cp->GetDeclType() == CSS_DECL_GEN_ARRAY)
		{
			/* There is a background image declaration list, but it can be
			   a list of 'none's. */

			const CSS_generic_value* images_arr = cp->GenArrayValue();
			for (int i=0; i< cp->ArrayValueLen(); i++)
			{
#ifdef CSS_GRADIENT_SUPPORT
				OP_ASSERT(images_arr[i].GetValueType() == CSS_IDENT ||
						  images_arr[i].GetValueType() == CSS_FUNCTION_URL ||
						  images_arr[i].GetValueType() == CSS_FUNCTION_LINEAR_GRADIENT ||
						  images_arr[i].GetValueType() == CSS_FUNCTION_WEBKIT_LINEAR_GRADIENT ||
						  images_arr[i].GetValueType() == CSS_FUNCTION_O_LINEAR_GRADIENT ||
						  images_arr[i].GetValueType() == CSS_FUNCTION_RADIAL_GRADIENT ||
						  images_arr[i].GetValueType() == CSS_FUNCTION_REPEATING_LINEAR_GRADIENT ||
						  images_arr[i].GetValueType() == CSS_FUNCTION_REPEATING_RADIAL_GRADIENT ||
						  images_arr[i].GetValueType() == CSS_FUNCTION_SKIN);
#else
				OP_ASSERT(images_arr[i].GetValueType() == CSS_IDENT ||
						  images_arr[i].GetValueType() == CSS_FUNCTION_URL ||
						  images_arr[i].GetValueType() == CSS_FUNCTION_SKIN);
#endif // CSS_GRADIENT_SUPPORT

				/* An identifier is assumed to be 'none' */
				OP_ASSERT(images_arr[i].GetValueType() != CSS_IDENT ||
						  images_arr[i].GetType() == CSS_VALUE_none);

				if (images_arr[i].GetValueType() != CSS_IDENT)
				{
					has_bg_image = TRUE;
					break;
				}
			}
		}

		if (has_bg_image)
			AssignCSSDeclaration(CSSPROPS_BG_IMAGE, props_array, prop_count, cp);
	}

	// list-style-image
	if (cssprops.GetDecl(CSS_PROPERTY_list_style_image))
	{
		props_array[prop_count].info.type = CSSPROPS_LIST_STYLE_IMAGE;
		cp = cssprops.GetDecl(CSS_PROPERTY_list_style_image);
		if (cp->GetDeclType() == CSS_DECL_TYPE)
		{
			if (cp->TypeValue() == CSS_VALUE_none)
			{
				props_array[prop_count].value.url.Set(NULL); // keyword value 'none', value.string is NULL
				props_array[++prop_count].Init();
			}
		}
		else
		{
#ifdef CSS_GRADIENT_SUPPORT
			if (cp->GetDeclType() == CSS_DECL_GEN_ARRAY)
			{
				const CSS_generic_value* arr = cp->GenArrayValue();

				if (CSS_is_gradient(arr[0].GetValueType()))
				{
					props_array[prop_count].info.info2.image_is_gradient = 1;
					props_array[prop_count].value.gradient = arr[0].GetGradient();
					props_array[++prop_count].Init();
				}
			}
			else
#endif // CSS_GRADIENT_SUPPORT
			{
				OP_ASSERT(cp->GetDeclType() == CSS_DECL_STRING);
				props_array[prop_count].value.url.Set(cp->StringValue(), cp->GetUserDefined());
				props_array[++prop_count].Init();
			}
		}

		// non existing property means keyword value 'inherit' which is default value
	}

	if ((cp = cssprops.GetDecl(CSS_PROPERTY_nav_up)) != NULL)
		AssignCSSDeclaration(CSSPROPS_NAVUP, props_array, prop_count, cp);

	if ((cp = cssprops.GetDecl(CSS_PROPERTY_nav_down)) != NULL)
		AssignCSSDeclaration(CSSPROPS_NAVDOWN, props_array, prop_count, cp);

	if ((cp = cssprops.GetDecl(CSS_PROPERTY_nav_left)) != NULL)
		AssignCSSDeclaration(CSSPROPS_NAVLEFT, props_array, prop_count, cp);

	if ((cp = cssprops.GetDecl(CSS_PROPERTY_nav_right)) != NULL)
		AssignCSSDeclaration(CSSPROPS_NAVRIGHT, props_array, prop_count, cp);

	if (cssprops.GetDecl(CSS_PROPERTY_page_break_before) ||
		cssprops.GetDecl(CSS_PROPERTY_page_break_after) ||
		cssprops.GetDecl(CSS_PROPERTY_page_break_inside) ||
		cssprops.GetDecl(CSS_PROPERTY_break_before) ||
		cssprops.GetDecl(CSS_PROPERTY_break_after) ||
		cssprops.GetDecl(CSS_PROPERTY_break_inside) ||
		cssprops.GetDecl(CSS_PROPERTY_page) ||
		cssprops.GetDecl(CSS_PROPERTY_size) ||
		cssprops.GetDecl(CSS_PROPERTY_orphans) ||
		cssprops.GetDecl(CSS_PROPERTY_widows))
	{
		props_array[prop_count].info.type = CSSPROPS_PAGE;

		if (cssprops.GetDecl(CSS_PROPERTY_break_after))
			// break-after
			props_array[prop_count].value.page_props.info.break_after = GetBreakValue(cssprops.GetDecl(CSS_PROPERTY_break_after)->TypeValue(), CSS_BREAK_auto);
		else
			// page-break-after
			props_array[prop_count].value.page_props.info.break_after = GetBreakValue(cssprops.GetDecl(CSS_PROPERTY_page_break_after) ? cssprops.GetDecl(CSS_PROPERTY_page_break_after)->TypeValue() : 0, CSS_BREAK_auto);

		if (cssprops.GetDecl(CSS_PROPERTY_break_before))
			// break-before
			props_array[prop_count].value.page_props.info.break_before = GetBreakValue(cssprops.GetDecl(CSS_PROPERTY_break_before)->TypeValue(), CSS_BREAK_auto);
		else
			// page-break-before
			props_array[prop_count].value.page_props.info.break_before = GetBreakValue(cssprops.GetDecl(CSS_PROPERTY_page_break_before) ? cssprops.GetDecl(CSS_PROPERTY_page_break_before)->TypeValue() : 0, CSS_BREAK_auto);

		if (cssprops.GetDecl(CSS_PROPERTY_break_inside))
			// break-inside
			props_array[prop_count].value.page_props.info.break_inside = GetBreakValue(cssprops.GetDecl(CSS_PROPERTY_break_inside)->TypeValue(), CSS_BREAK_inherit);
		else
			// page-break-inside
			props_array[prop_count].value.page_props.info.break_inside = GetBreakValue(cssprops.GetDecl(CSS_PROPERTY_page_break_inside) ? cssprops.GetDecl(CSS_PROPERTY_page_break_inside)->TypeValue() : 0, CSS_BREAK_inherit);

		// orphans
		props_array[prop_count].value.page_props.info.orphans = CSS_ORPHANS_WIDOWS_inherit;
		cp = cssprops.GetDecl(CSS_PROPERTY_orphans);
		if (cp && cp->GetDeclType() == CSS_DECL_LONG)
			props_array[prop_count].value.page_props.info.orphans = MIN(MAX(cp->LongValue(), 0), CSS_ORPHANS_WIDOWS_VAL_MAX);

		// widows
		props_array[prop_count].value.page_props.info.widows = CSS_ORPHANS_WIDOWS_inherit;
		cp = cssprops.GetDecl(CSS_PROPERTY_widows);
		if (cp && cp->GetDeclType() == CSS_DECL_LONG)
			props_array[prop_count].value.page_props.info.widows = MIN(MAX(cp->LongValue(), 0), CSS_ORPHANS_WIDOWS_VAL_MAX);

		// page (this must be the last since it may generate a new property)
		cp = cssprops.GetDecl(CSS_PROPERTY_page);
		props_array[prop_count].value.page_props.info.page = CSS_PAGE_inherit;
		if (cp)
		{
			if (cp->GetDeclType() == CSS_DECL_TYPE && cp->TypeValue() == CSS_VALUE_auto)
				props_array[prop_count].value.page_props.info.page = CSS_PAGE_auto;
			else if (cp->GetDeclType() == CSS_DECL_STRING && cp->StringValue())
			{
				props_array[prop_count].value.page_props.info.page = CSS_PAGE_identifier;
				props_array[++prop_count].Init();
				props_array[prop_count].info.type = CSSPROPS_PAGE_IDENTIFIER;
				props_array[prop_count].value.string = cp->StringValue();
			}
		}

		props_array[++prop_count].Init();
	}

#ifdef _CSS_LINK_SUPPORT_
	if ((cp = cssprops.GetDecl(CSS_PROPERTY__o_link)) != NULL)
	{
		URL css_link_url = GetUrlFromCssDecl(cp, elm, &hld_profile->GetFramesDocument()->GetURL());
		if (!css_link_url.IsEmpty())
			// FIXME: This method should be moved to FramesDocument ....
			RETURN_IF_ERROR(hld_profile->SetCSSLink(css_link_url));
	}

	if ((cp = cssprops.GetDecl(CSS_PROPERTY__o_link_source)) != NULL)
	{
		if (cp->GetDeclType() == CSS_DECL_TYPE)
		{
			URL* css_url = NULL;
			if (cp->TypeValue() == CSS_VALUE_next)
				RETURN_IF_ERROR(hld_profile->GetNextCSSLink(&css_url));
			else if (cp->TypeValue() == CSS_VALUE_current)
				css_url = hld_profile->GetCurrentCSSLink();

			if (css_url)
				if (elm->SetSpecialAttr(ATTR_CSS_LINK, ITEM_TYPE_URL, (void*)css_url, FALSE, SpecialNs::NS_LOGDOC) < 0)
					return OpStatus::ERR_NO_MEMORY;
		}
	}
#endif // _CSS_LINK_SUPPORT_

	if (cssprops.GetDecl(CSS_PROPERTY__wap_marquee_style) ||
		cssprops.GetDecl(CSS_PROPERTY__wap_marquee_dir) ||
		cssprops.GetDecl(CSS_PROPERTY__wap_marquee_speed) ||
		cssprops.GetDecl(CSS_PROPERTY__wap_marquee_loop))
	{
		props_array[prop_count].info.type = CSSPROPS_WAP_MARQUEE;

		// wap-marquee-style
		cp = cssprops.GetDecl(CSS_PROPERTY__wap_marquee_style);
		unsigned int wap_marquee_style = CSS_WAP_MARQUEE_STYLE_scroll;
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch (cp->TypeValue())
			{
			case CSS_VALUE_slide:
				wap_marquee_style = CSS_WAP_MARQUEE_STYLE_slide; break;
			case CSS_VALUE_alternate:
				wap_marquee_style = CSS_WAP_MARQUEE_STYLE_alternate; break;
			}
		}
		props_array[prop_count].value.wap_marquee_props.info.style = wap_marquee_style;

		// wap-marquee-dir
		cp = cssprops.GetDecl(CSS_PROPERTY__wap_marquee_dir);
		unsigned int wap_marquee_dir = CSS_WAP_MARQUEE_DIR_rtl;
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch (cp->TypeValue())
			{
			case CSS_VALUE_ltr:
				wap_marquee_dir = CSS_WAP_MARQUEE_DIR_ltr;
				break;
			case CSS_VALUE_up:
				wap_marquee_dir = CSS_WAP_MARQUEE_DIR_up;
				break;
			case CSS_VALUE_down:
				wap_marquee_dir = CSS_WAP_MARQUEE_DIR_down;
				break;
			default:
				break;
			}
		}
		props_array[prop_count].value.wap_marquee_props.info.dir = wap_marquee_dir;

		// wap-marquee-speed
		cp = cssprops.GetDecl(CSS_PROPERTY__wap_marquee_speed);
		unsigned int wap_marquee_speed = CSS_WAP_MARQUEE_SPEED_value_not_set;
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch (cp->TypeValue())
			{
			case CSS_VALUE_slow:
				wap_marquee_speed = CSS_WAP_MARQUEE_SPEED_slow; break;
			case CSS_VALUE_fast:
				wap_marquee_speed = CSS_WAP_MARQUEE_SPEED_fast; break;
			case CSS_VALUE_normal:
				wap_marquee_speed = CSS_WAP_MARQUEE_SPEED_normal; break;
			}
		}
		props_array[prop_count].value.wap_marquee_props.info.speed = wap_marquee_speed;

		// wap-marquee-loop
		cp = cssprops.GetDecl(CSS_PROPERTY__wap_marquee_loop);
		int wap_marquee_loop = 1;
		if (cp)
		{
			if (cp->GetDeclType() == CSS_DECL_TYPE &&
				cp->TypeValue() == CSS_VALUE_infinite)
				wap_marquee_loop = CSS_WAP_MARQUEE_LOOP_infinite;
			else if (cp->GetDeclType() == CSS_DECL_NUMBER)
			{
				wap_marquee_loop = (int) cp->GetNumberValue(0);
				if (wap_marquee_loop < 0)
					wap_marquee_loop = 1;
				else if (wap_marquee_loop > CSS_WAP_MARQUEE_LOOP_VAL_MAX)
					wap_marquee_loop = CSS_WAP_MARQUEE_LOOP_VAL_MAX;
			}
		}
		props_array[prop_count].value.wap_marquee_props.info.loop = (unsigned long)wap_marquee_loop;

		props_array[++prop_count].Init();
	}

#ifdef _WML_SUPPORT_
# ifdef ACCESS_KEYS_SUPPORT
	if ((cp = cssprops.GetDecl(CSS_PROPERTY__wap_accesskey)) != NULL)
		AssignCSSDeclaration(CSSPROPS_ACCESSKEY, props_array, prop_count, cp);
	// non existing property means keyword value 'none' which is default value
# endif // ACCESS_KEYS_SUPPORT
#endif // _WML_SUPPORT_

	// -o-table-baseline
	if (cssprops.GetDecl(CSS_PROPERTY__o_table_baseline) || cssprops.GetDecl(CSS_PROPERTY_table_rules))
	{
		props_array[prop_count].info.type = CSSPROPS_TABLE;

		cp = cssprops.GetDecl(CSS_PROPERTY__o_table_baseline);
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE && cp->TypeValue() == CSS_VALUE_inherit)
			props_array[prop_count].value.long_val = CSS_TABLE_BASELINE_inherit;
		else if (cp && cp->GetDeclType() == CSS_DECL_LONG)
			props_array[prop_count].value.long_val = cp->LongValue();
		else
			props_array[prop_count].value.long_val = CSS_TABLE_BASELINE_not_set;

		cp = cssprops.GetDecl(CSS_PROPERTY_table_rules);
		if (cp && cp->GetDeclType() == CSS_DECL_LONG)
			props_array[prop_count].info.info2.extra = (unsigned short)(cp->LongValue());
		props_array[++prop_count].Init();
	}

	// -o-tab-size
	if ((cp = cssprops.GetDecl(CSS_PROPERTY__o_tab_size)) != NULL)
	{
		props_array[prop_count].info.type = CSSPROPS_TAB_SIZE;
		if (cp->GetDeclType() == CSS_DECL_TYPE && cp->TypeValue() == CSS_VALUE_inherit)
			props_array[prop_count].value.long_val = CSS_TAB_SIZE_inherit;
		else
			props_array[prop_count].value.long_val = cp->LongValue();
		props_array[++prop_count].Init();
	}

#ifdef CSS_TRANSFORMS
	// transform
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_transform)))
		AssignCSSDeclaration(CSSPROPS_TRANSFORM, props_array, prop_count, cp);

	// transform-origin
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_transform_origin)))
	{
		props_array[prop_count].info.type = CSSPROPS_TRANSFORM_ORIGIN;

		length_value[0].css_decl = cp;
		if (length_value[0].css_decl->GetDeclType() == CSS_DECL_TYPE && length_value[0].css_decl->TypeValue() == CSS_VALUE_inherit)
			length_value[1].css_decl = NULL;
		else
			if (length_value[0].css_decl->GetDeclType() == CSS_DECL_NUMBER2)
				length_value[1].css_decl = length_value[0].css_decl;

		SetLengthPropertyNew(elm, CSSPROPS_TRANSFORM_ORIGIN, 2, length_value, FALSE, props_array, prop_count);
	}
#endif // CSS_TRANSFORMS

	// image-rendering
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_image_rendering)) != NULL)
	{
		props_array[prop_count].info.type = CSSPROPS_IMAGE_RENDERING;
		short value = cp->TypeValue();
		if (value == CSS_VALUE_inherit && cp->GetDeclType() == CSS_DECL_TYPE)
			props_array[prop_count].info.is_inherit = 1;
		props_array[prop_count].value.long_val = value;
		props_array[++prop_count].Init();
	}

#ifdef CSS_TRANSITIONS
	// transition-delay
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_transition_delay)))
		AssignCSSDeclaration(CSSPROPS_TRANS_DELAY, props_array, prop_count, cp);

	// transition-duration
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_transition_duration)))
		AssignCSSDeclaration(CSSPROPS_TRANS_DURATION, props_array, prop_count, cp);

	// transition-timing-function
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_transition_timing_function)))
		AssignCSSDeclaration(CSSPROPS_TRANS_TIMING, props_array, prop_count, cp);

	// transition-property
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_transition_property)))
		AssignCSSDeclaration(CSSPROPS_TRANS_PROPERTY, props_array, prop_count, cp);
#endif // CSS_TRANSITIONS

#ifdef CSS_ANIMATIONS
	if ((cp = cssprops.GetDecl(CSS_PROPERTY_animation_name)))
		AssignCSSDeclaration(CSSPROPS_ANIMATION_NAME, props_array, prop_count, cp);

	if ((cp = cssprops.GetDecl(CSS_PROPERTY_animation_duration)))
		AssignCSSDeclaration(CSSPROPS_ANIMATION_DURATION, props_array, prop_count, cp);

	if ((cp = cssprops.GetDecl(CSS_PROPERTY_animation_timing_function)))
		AssignCSSDeclaration(CSSPROPS_ANIMATION_TIMING_FUNCTION, props_array, prop_count, cp);

	if ((cp = cssprops.GetDecl(CSS_PROPERTY_animation_iteration_count)))
		AssignCSSDeclaration(CSSPROPS_ANIMATION_ITERATION_COUNT, props_array, prop_count, cp);

	if ((cp = cssprops.GetDecl(CSS_PROPERTY_animation_direction)))
		AssignCSSDeclaration(CSSPROPS_ANIMATION_DIRECTION, props_array, prop_count, cp);

	if ((cp = cssprops.GetDecl(CSS_PROPERTY_animation_play_state)))
		AssignCSSDeclaration(CSSPROPS_ANIMATION_PLAY_STATE, props_array, prop_count, cp);

	if ((cp = cssprops.GetDecl(CSS_PROPERTY_animation_delay)))
		AssignCSSDeclaration(CSSPROPS_ANIMATION_DELAY, props_array, prop_count, cp);

	if ((cp = cssprops.GetDecl(CSS_PROPERTY_animation_fill_mode)))
		AssignCSSDeclaration(CSSPROPS_ANIMATION_FILL_MODE, props_array, prop_count, cp);
#endif // CSS_ANIMATIONS

#ifdef CSS_MINI_EXTENSIONS
	if (cssprops.GetDecl(CSS_PROPERTY__o_focus_opacity) ||
		cssprops.GetDecl(CSS_PROPERTY__o_mini_fold))
	{
		props_array[prop_count].info.type = CSSPROPS_MINI;

		// -o-mini-fold
		cp = cssprops.GetDecl(CSS_PROPERTY__o_mini_fold);
		unsigned long fold = CSS_MINI_FOLD_value_not_set;
		if (cp && cp->GetDeclType() == CSS_DECL_TYPE)
		{
			switch (cp->TypeValue())
			{
			case CSS_VALUE_folded:
				fold = CSS_MINI_FOLD_folded;
				break;
			case CSS_VALUE_unfolded:
				fold = CSS_MINI_FOLD_unfolded;
				break;
			default:
				fold = CSS_MINI_FOLD_none;
				break;
			}
		}
		props_array[prop_count].value.mini_props.info.fold = fold;

		// -o-focus-opacity
		cp = cssprops.GetDecl(CSS_PROPERTY__o_focus_opacity);
		if (cp && cp->GetDeclType() == CSS_DECL_NUMBER)
		{
			unsigned long opacity;
			float fopacity = (float) (cp->GetNumberValue(0) * 255.0);
			if (fopacity < 0.0)
				opacity = 0;
			else if (fopacity > 255.0)
				opacity = 255;
			else
				opacity = (unsigned long)fopacity;

			props_array[prop_count].value.mini_props.info.focus_opacity_set = 1;
			props_array[prop_count].value.mini_props.info.focus_opacity = opacity;
		}

		props_array[++prop_count].Init();
	}
#endif // CSS_MINI_EXTENSIONS

	cp = cssprops.GetDecl(CSS_PROPERTY_selection_color);
	if (cp)
	{
		props_array[prop_count].info.type = CSSPROPS_SEL_COLOR;

		CSSDeclType type = cp->GetDeclType();
		if (type == CSS_DECL_LONG || type == CSS_DECL_COLOR)
		{
			props_array[prop_count].value.color_val = (COLORREF)cp->LongValue();
			if (type == CSS_DECL_COLOR)
				props_array[prop_count].info.info2.extra = 1;
			props_array[++prop_count].Init();
		}
		else if (type == CSS_DECL_TYPE)
		{
			short type_value = cp->TypeValue();
			if (type_value == CSS_VALUE_inherit)
				props_array[prop_count].value.color_val = COLORREF(CSS_COLOR_inherit);
			else if (type_value == CSS_VALUE_transparent)
				props_array[prop_count].value.color_val = COLORREF(CSS_COLOR_transparent);
			props_array[++prop_count].Init();
		}
# ifdef SKIN_SUPPORT
		else if (type == CSS_DECL_STRING)
		{
			props_array[prop_count].value.color_val = GetColorFromCssDecl(cp);
			props_array[++prop_count].Init();
		}
# endif // SKIN_SUPPORT
	}

	cp = cssprops.GetDecl(CSS_PROPERTY_selection_background_color);
	if (cp)
	{
		props_array[prop_count].info.type = CSSPROPS_SEL_BGCOLOR;

		CSSDeclType type = cp->GetDeclType();
		if (type == CSS_DECL_LONG || type == CSS_DECL_COLOR)
		{
			props_array[prop_count].value.color_val = cp->LongValue();
			props_array[++prop_count].Init();
		}
		else if (type == CSS_DECL_TYPE)
		{
			short type_value = cp->TypeValue();
			if (type_value == CSS_VALUE_inherit)
				props_array[prop_count].value.color_val = COLORREF(CSS_COLOR_inherit);
			else if (type_value == CSS_VALUE_transparent)
				props_array[prop_count].value.color_val = COLORREF(CSS_COLOR_transparent);
			else if (type_value == CSS_VALUE_currentColor)
				props_array[prop_count].value.color_val = COLORREF(CSS_COLOR_current_color);
			props_array[++prop_count].Init();
		}
	}

	LoadSVGCssProperties(cssprops, props_array, prop_count);

	BOOL pseudo_elm_change = had_pseudo_elm || elm->HasBeforeOrAfter() || elm->HasFirstLetter() || elm->HasFirstLine();
	SharedCss* shared = NULL;
	CssPropertyItem* new_props = NULL;

	if (prop_count != 0)
	{
		new_props = g_sharedCssManager->GetSharedCssProperties(shared, props_array, prop_count * sizeof(CssPropertyItem));
		if (!new_props)
			return OpStatus::ERR_NO_MEMORY;
	}

	if (new_props == elm->css_properties && !pseudo_elm_change)
	{
		// If we can share old and new, nothing has changed.
		if (new_props)
		{
			// We already held one reference, and now we got a new one that we won't use, so we must release it.
			SharedCssManager::DecRef(shared);
		}
		return OpStatus::OK;
	}

	if (changes)
	{
		*changes = 0;
		if (pseudo_elm_change)
			*changes = PROPS_CHANGED_STRUCTURE;

		int i;
		int old_found = 0;

		for (i = 0; i<prop_count; i++)
		{
			CssPropertyItem* old = CssPropertyItem::GetCssPropertyItem(elm, props_array[i].info.type);
			if (!old || op_memcmp(old, &props_array[i], sizeof(props_array[0])))
			{
				*changes |= GetCssPropertyChangeBits(elm, &props_array[i], old);

#ifdef SVG_SUPPORT
				if (elm_ns == NS_SVG)
					*changes |= GetSVGCssPropertyChangeBits(elm, &props_array[i], old);
#endif
			}
			else if (old)
			{
				switch (old->info.type)
				{
				case CSSPROPS_BORDER_STYLE:
				case CSSPROPS_BORDER_LEFT_STYLE:
				case CSSPROPS_BORDER_RIGHT_STYLE:
				case CSSPROPS_BORDER_TOP_STYLE:
				case CSSPROPS_BORDER_BOTTOM_STYLE:
					if (old->value.border_props.info.style != CSS_BORDER_STYLE_none &&
						old->value.border_props.info.style != CSS_BORDER_STYLE_value_not_set)
						*changes |= PROPS_CHANGED_HAS_BORDER;
					break;
				}
			}

			if (old)
				++old_found;
		}

		int len = elm->GetCssPropLen();

		// If old_found is less than len, len-old_found properties were in elm->css_properties but not in props_array, i.e, were removed.

		for (i = 0; i<len && old_found < len; i++)
		{
			int type = elm->css_properties[i].info.type;

			BOOL not_found = TRUE;
			for (int j = 0; j<prop_count; j++)
				if (props_array[j].info.type == type)
				{
					not_found = FALSE;
					break;
				}

			if (not_found)
			{
				*changes |= GetCssPropertyChangeBits(elm, NULL/*new*/, &elm->css_properties[i]/*old*/);

#ifdef SVG_SUPPORT
				if (elm_ns == NS_SVG)
					*changes |= GetSVGCssPropertyChangeBits(elm, NULL/*new*/, &elm->css_properties[i]/*old*/);
#endif

				++old_found;
			}
		}

		if ((*changes & PROPS_CHANGED_HAS_BORDER))
		{
			if ((*changes & PROPS_CHANGED_BORDER_WIDTH))
				*changes |= PROPS_CHANGED_UPDATE_SIZE;
			if ((*changes & PROPS_CHANGED_BORDER_COLOR))
				*changes |= PROPS_CHANGED_UPDATE;
		}

		*changes &= ~(PROPS_CHANGED_BORDER_WIDTH|PROPS_CHANGED_HAS_BORDER|PROPS_CHANGED_BORDER_COLOR);

		if (Box* box = elm->GetLayoutBox())
			if (Container* container = box->GetContainer())
				if ((*changes & PROPS_CHANGED_STRUCTURE) == 0)
					if (NeedsNewContainer(container,
										  width_auto, left_auto, right_auto,
										  width_inherit, left_inherit, right_inherit,
										  display_type,
										  float_type,
										  position,
										  elm_type))
						*changes |= PROPS_CHANGED_STRUCTURE;
	}

	UNSHARE_CSS_PROPERTIES(elm);

	elm->css_properties = new_props;

	if (new_props)
	{
		elm->SetCssPropLen(prop_count);
		elm->SetSharedCss();
	}
	else
		elm->SetCssPropLen(0);

	OP_ASSERT(!hld_profile->GetIsOutOfMemory());
	return OpStatus::OK;
}

/** HTML_Elements may share CssPropertyItem arrays if the values are identical.

	The methods in this class are only used from one place, and are therefore
	intentionally inlined.

	@see define SHARED_CSS_HASHSIZE
*/

class SharedCss : public Link
{
	int refs;
	int size;
	UINT32 hash;

public:
	SharedCss(int item_size, UINT32 hash):
		refs(0),
		size(item_size),
		hash(hash)
	{
		OP_ASSERT(!(size&3));
	}

	inline void Inc()
	{
		++refs;
	}

	inline void Dec()
	{
		--refs;
		if (refs <= 0)
			Destroy();
	}

	inline void Destroy()
	{
		REPORT_MEMMAN_DEC(sizeof(SharedCss) + size);

		CssPropertyItem::RemoveReferences(GetItems(), GetItemCount());

		Out();
		op_free(this);
	}

	inline BOOL Equal(CssPropertyItem* x, int x_size, UINT32 x_hash)
	{
		OP_ASSERT(!((x_size & 3)));

		if (size != x_size || hash != x_hash)
			return FALSE;

		x_size >>= 2;

		// inline memcmp that assumes sizes evenly divisible by 4.
		CssPropertyItem* items = GetItems();

		for (int i = 0; i < x_size; i++)
			if (((int*)x)[i] != ((int*)items)[i])
				return FALSE;
		return TRUE;
	}

	CssPropertyItem* GetItems() { return (CssPropertyItem*) ((char*)this + sizeof(SharedCss)); }
	int GetItemCount() { return size / sizeof(CssPropertyItem); }

	void* operator new(size_t size, void* p) { return p; }
};


void
SharedCssManager::DeleteSharedCssProperties(CssPropertyItem* x, int size)
{
	if (x)
	{
		SharedCss* q = (SharedCss*) (((char*)x) - sizeof(SharedCss));

		OP_ASSERT(FindSharedCss(x, size, FALSE) == q);

		q->Dec();
	}
}

CssPropertyItem*
SharedCssManager::GetSharedCssProperties(SharedCss*& shared, CssPropertyItem* x, int size)
{
	if ((shared = FindSharedCss(x, size, 1)) != NULL)
	{
		shared->Inc();
		return shared->GetItems();
	}
	return NULL; // OOM. That's OK.
}

void SharedCssManager::DecRef(SharedCss* shared)
{
	shared->Dec();
}

SharedCssManager::SharedCssManager()
{
}

SharedCssManager* SharedCssManager::CreateL()
{
	SharedCssManager* shared_css_manager = OP_NEW_L(SharedCssManager, ());
	shared_css_manager->property_list = OP_NEWA(Head, SHARED_CSS_HASHSIZE);
	if (shared_css_manager->property_list == NULL)
	{
		OP_DELETE(shared_css_manager);
		LEAVE(OpStatus::ERR_NO_MEMORY);
	}
	return shared_css_manager;
}

SharedCssManager::~SharedCssManager()
{
	if (!property_list)
		return;

	for (int i=0; i < SHARED_CSS_HASHSIZE; i++)
	{
	    while (SharedCss* p = static_cast<SharedCss*>(property_list[i].First()))
		{
			OP_ASSERT(!"This should already have been freed");
			p->Destroy();
		}
	}

	OP_DELETEA(property_list);
}

SharedCss* SharedCssManager::FindSharedCss(CssPropertyItem* x, int size, int create)
{
	int hval = 0;
	UINT32 bits = 0;
	OP_ASSERT(x != 0);

	UINT32* uip = (unsigned int*)x;
	int count = size >> 2;
	for (int t=0; t < count; t++)
		bits = (bits << 11) ^ (bits >> 21) ^ *uip++ + 0xdeadbeef;
	hval = (bits & 0x0fffffff) % SHARED_CSS_HASHSIZE;

	Head& list = property_list[hval];

	SharedCss* y = (SharedCss*)list.First();

	while (y)
	{
		if (y->Equal(x, size, bits))
			return y;
		y = (SharedCss*)y->Suc();
	}

	if (!create)
		return 0;

	void* p = op_malloc(sizeof(SharedCss) + size);

	if (!p)
		return NULL;

	REPORT_MEMMAN_INC(sizeof(SharedCss) + size);

	SharedCss* shared = new (p) SharedCss(size, bits);
	CssPropertyItem* items = shared->GetItems();

	op_memcpy(items, x, size);

	CssPropertyItem::AddReferences(items, shared->GetItemCount());

	shared->Into(&list);
	return shared;
}
