/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/SVGLayoutProperties.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/SVGAttributeParser.h"
#include "modules/svg/src/SVGPaint.h"
#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/SVGFontSize.h"

#include "modules/dochand/fdelm.h"
#include "modules/dochand/win.h"
#include "modules/layout/cssprops.h"
#include "modules/layout/layout_workplace.h"
#include "modules/layout/box/box.h"
#include "modules/layout/cascade.h"
#include "modules/layout/layoutpool.h"
#include "modules/layout/layoutprops.h"

#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/logdoc.h"
#include "modules/style/css_all_properties.h"
#include "modules/style/css_media.h"
#include "modules/style/css_decl.h"

#include "modules/pi/OpScreenInfo.h"

#define CONTENT_WIDTH_IS_DEFINED(X) ((X) != CONTENT_WIDTH_AUTO && (X) != CONTENT_WIDTH_O_SIZE)
#define CONTENT_HEIGHT_IS_DEFINED(X) ((X) != CONTENT_HEIGHT_AUTO && (X) != CONTENT_HEIGHT_O_SIZE)

/**
 * Helper structure for storing the content dimensions
 */
struct ContentDimensions
{
	ContentDimensions() : wunit(0), hunit(0), w(0), h(0) {}
	short wunit;
	short hunit;
	short w;
	long h;
};

static BOOL SetUrlReference(const uni_char* str_val, unsigned str_len, SVGURLReference& url_ref)
{
	if (!str_val || str_len == 0)
	{
		return FALSE;
	}
	else if (uni_strncmp(str_val, "none", str_len) == 0)
	{
		url_ref.info.is_none = TRUE;
		return TRUE;
	}
	else
	{
		const uni_char* url_part_start = NULL;
		unsigned url_part_length = 0;
		if (OpStatus::IsSuccess(SVGAttributeParser::ParseURL(str_val, str_len, url_part_start, url_part_length)))
		{
			url_ref.url_str = url_part_start;
			url_ref.info.url_str_len = url_part_length;
			url_ref.info.is_none = FALSE;
			return TRUE;
		}
	}
	return FALSE;
}

static OP_STATUS
NewPaintProp(SvgProperties& svg, SVGPropertyReference*& outref, const SVGPaint& p, BOOL fill)
{
	SVGPaint* pcopy = OP_NEW(SVGPaint, ());
	if (!pcopy)
	{
		svg.info.had_hard_error = 1;
		svg.info.had_oom_error = 1;
		outref = fill ? &g_svg_manager_impl->default_fill_prop : &g_svg_manager_impl->default_stroke_prop;
		return OpStatus::ERR_NO_MEMORY;
	}
	else
	{
		outref = SVGPropertyReference::IncRef(pcopy);
		return pcopy->Copy(p); // Hanging onto the object - can't be all bad
	}
}

static OP_STATUS
NewDashArrayProp(SvgProperties& svg, const SVGVector* da_vec)
{
	svg.dasharray = &g_svg_manager_impl->dasharray_isnone_prop;

	SVGDashArray* da = OP_NEW(SVGDashArray, ());
	if (!da)
		return OpStatus::ERR_NO_MEMORY;

	SVGPropertyReference::IncRef(da);

	da->dash_array_size = da_vec->GetCount();
	da->dash_array = OP_NEWA(SVGLength, da->dash_array_size);
	if(!da->dash_array)
	{
		SVGPropertyReference::DecRef(da);
		svg.info.had_oom_error = 1;
		svg.info.had_hard_error = 1;
		return OpStatus::ERR_NO_MEMORY;
	}

	svg.dasharray = da;

	for (UINT32 i = 0; i < da->dash_array_size; ++i)
	{
		SVGObject* obj = da_vec->Get(i);
		if(obj->Type() != SVGOBJECT_LENGTH)
		{
			OP_ASSERT(!"Unexpected dash-array itemtype");
			svg.info.had_hard_error = 1;
			return OpStatus::ERR;
		}
		da->dash_array[i] = static_cast<SVGLengthObject*>(obj)->GetLength();
	}
	return OpStatus::OK;
}

/* static */ BOOL
HTMLayoutProperties::AllocateSVGProps(SvgProperties *&current_svg_props,
									  const SvgProperties *parent_svg_props)
{
	if (!current_svg_props)
	{
		current_svg_props = new SvgProperties();
		if (!current_svg_props)
			return FALSE;

		current_svg_props->Reset(NULL);
		if (parent_svg_props != NULL)
			current_svg_props->Reset(parent_svg_props);
	}

	OP_ASSERT(current_svg_props != NULL);
	return TRUE;
}

OP_STATUS
HTMLayoutProperties::SetSVGCssPropertiesFromHtmlAttr(HTML_Element* element, const HTMLayoutProperties& parent_props,
													 HLDocProfile* hld_profile)
{
	/* Instead of looking at this->svg and parent_props.svg, which may
	   be pointers to heap-allocated SvgProperties or parts of the
	   HTMLayoutProperties structures depending on the layout module,
	   use these pointers. */

	SvgProperties *current_svg_props = NULL;
	const SvgProperties *parent_svg_props = NULL;

	SvgProperties *parent_svg_props_dealloc = NULL;
	current_svg_props = svg;
	parent_svg_props = parent_props.svg;

	if (!parent_svg_props)
	{
		if (!AllocateSVGProps(parent_svg_props_dealloc, NULL))
			return OpStatus::ERR_NO_MEMORY;

		/* Normally svg font-size and html font-size are synced
		   during the cascade. Since we have made a SvgProperties up,
		   we must simulate this relationship here. */
		parent_svg_props_dealloc->fontsize = parent_props.font_size;

		parent_svg_props = parent_svg_props_dealloc;
	}

	OP_ASSERT(parent_svg_props != NULL);
	if (!AllocateSVGProps(this->svg, parent_svg_props))
	{
		delete parent_svg_props_dealloc;
		return OpStatus::ERR_NO_MEMORY;
	}

	current_svg_props = this->svg;

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(element);
	if (!doc_ctx)
	{
		// We don't want to treat elements as SVG elements unless they
		// are in a svg:svg element. See bug 185336.
		// This gets O(k^2) where k is the depth of the tree. Maybe a bit
		// in the props that says if we've seen a svg:svg on the way.
		delete parent_svg_props_dealloc;
		return OpStatus::OK;
	}

	OP_ASSERT(current_svg_props != NULL);
	OP_ASSERT(parent_svg_props != NULL);

	NS_Type nstype = element->GetNsType();
	Markup::Type type = element->Type();
	BOOL has_svg_presentation_attributes = FALSE;
	SVGElementContext* elm_ctx = NULL;

	// Specification:
	// The initial value for 'overflow' as defined in [CSS2-overflow] is
	// 'visible'; however, SVG's user agent style sheet overrides this
	// initial value and set the 'overflow' property on elements that
	// establish new viewports (e.g., 'svg' elements), 'pattern' elements
	// and 'marker' elements to the value 'hidden'
	//
	// and
	//
	//  The following elements establish new viewports: The 'svg' element
	// A 'symbol' element ...
	// An 'image' element ... A 'foreignObject' element ...
	if(nstype == NS_SVG)
	{
		if (type == Markup::SVGE_VIDEO)
			object_fit_position.fit = CSS_VALUE_contain;

		switch (type)
		{
		case Markup::SVGE_SVG:
			if (doc_ctx->IsExternalAnimation() &&
				element == SVGUtils::GetTopmostSVGRoot(element))
				break;
			// else fall through

		case Markup::SVGE_SYMBOL:
		case Markup::SVGE_IMAGE:
		case Markup::SVGE_FOREIGNOBJECT:
		case Markup::SVGE_MARKER:
		case Markup::SVGE_PATTERN:
		case Markup::SVGE_ANIMATION:
		case Markup::SVGE_VIDEO:
			overflow_x = overflow_y = CSS_VALUE_hidden;
		}

		elm_ctx = AttrValueStore::GetSVGElementContext_Unsafe(element);
		has_svg_presentation_attributes = !elm_ctx || (elm_ctx->HasPresentationAttributes() != NO);
	}

	if (has_svg_presentation_attributes)
	{
		int attr_name;
		int ns_idx;
		BOOL is_special;
		SVGObject* obj;
		BOOL found_pres_attrs = !elm_ctx; // if we don't have anywhere to store the flag, don't waste time calling IsPresentationAttribute

		SVGAttributeField field_type = g_svg_manager_impl->GetPresentationValues() ? SVG_ATTRFIELD_DEFAULT : SVG_ATTRFIELD_BASE;
		SVGAttributeIterator iterator(element, field_type);

		while (iterator.GetNext(attr_name, ns_idx, is_special, obj))
		{
			if (obj == NULL || obj->HasError())
			  continue;

			if (is_special)
			{
				// Handle relevant special attrs (currently only one)
				if (ns_idx == SpecialNs::NS_SVG &&
					attr_name == Markup::SVGA_ANIMATED_MARKER_PROP)
				{
					SVGString* url_str_val = static_cast<SVGString*>(obj);
					if(obj->Type() == SVGOBJECT_STRING)
					{
						if (url_str_val->Flag(SVGOBJECTFLAG_INHERIT))
						{
							current_svg_props->marker = parent_svg_props->marker;
							current_svg_props->override.has_marker = 1;
						}
						else if (SetUrlReference(url_str_val->GetString(), url_str_val->GetLength(), current_svg_props->marker))
						{
							current_svg_props->override.has_marker = 1;
						}

						found_pres_attrs = TRUE;
					}
				}
				continue;
			}

			NS_Type attr_ns = g_ns_manager->GetNsTypeAt(element->ResolveNsIdx(ns_idx));
			if (attr_ns == NS_SVG)
			{
				found_pres_attrs = found_pres_attrs || SVGUtils::IsPresentationAttribute(static_cast<Markup::AttrType>(attr_name), type);

				switch(attr_name)
				{
					case Markup::SVGA_FONT_FAMILY:
					{
						// shared property
						SVGVector* ffm = static_cast<SVGVector*>(obj);
						if (obj->Type() == SVGOBJECT_VECTOR)
						{
							if(ffm->Flag(SVGOBJECTFLAG_INHERIT))
							{
								font_number = parent_props.font_number;
							}
							else
							{
								// Pick first valid font in the list
								for(UINT32 i = 0; i < ffm->GetCount(); i++)
								{
									SVGObject* obj = ffm->Get(i);
									if (obj->Type() != SVGOBJECT_STRING)
										continue;

									const uni_char* family_name = static_cast<SVGString*>(obj)->GetString();
									if(family_name)
									{
										short fnum = styleManager->LookupFontNumber(hld_profile, family_name, hld_profile->GetFramesDocument()->GetMediaType());
										if(fnum != -1)
										{
											font_number = fnum;
											break;
										}

										StyleManager::GenericFont gf = styleManager->GetGenericFont(family_name);
										if (gf != StyleManager::UNKNOWN)
										{
											fnum = styleManager->GetGenericFontNumber(gf, parent_props.current_script);
											if (fnum != -1)
											{
												font_number = fnum;
												break;
											}
										}
									}
								}
							}
						}
					}
					break;
					case Markup::SVGA_FONT_SIZE:
					{
						// shared property
						SVGFontSizeObject* svgsize = static_cast<SVGFontSizeObject*>(obj);
						if (obj->Type() == SVGOBJECT_FONTSIZE)
						{
							if (svgsize->Flag(SVGOBJECTFLAG_INHERIT))
							{
								font_size = parent_props.font_size;
								current_svg_props->fontsize = parent_svg_props->fontsize;
							}
							else
							{
								const SVGFontSize& font_size = svgsize->font_size;
								SvgProperties::ResolveFontSizeLength(hld_profile,
																	font_size,
																	parent_svg_props->fontsize,
																	parent_props.decimal_font_size,
																	parent_props.font_number,
																	current_svg_props->fontsize,
																	decimal_font_size);

								switch(font_size.Mode())
								{
								case SVGFontSize::MODE_RESOLVED:
									SetFontSizeSpecified();
									break;
								case SVGFontSize::MODE_LENGTH:
								{
									const SVGLength& len = font_size.Length();
									int unit = len.GetUnit();
									if (unit != CSS_LENGTH_em && unit != CSS_LENGTH_ex && unit != CSS_LENGTH_rem)
										SetFontSizeSpecified();
								}
								break;
								}
							}
						}
					}
					break;
#if 0 // FIXME: Handle!
					case Markup::SVGA_FONT_STRETCH:
					{
						OP_ASSERT(!"Implement font-stretch attribute -> layoutprop");
						// NOTE: This is for SVG Full only
					}
					break;
#endif // 0/1
					case Markup::SVGA_FONT_STYLE:
					{
						// shared property
						SVGEnum* fontstyle = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							fontstyle->EnumType() == SVGENUM_FONT_STYLE)
						{
							if (fontstyle->Flag(SVGOBJECTFLAG_INHERIT))
							{
								font_italic = parent_props.font_italic;
							}
							else
							{
								switch(fontstyle->EnumValue())
								{
								case SVGFONTSTYLE_NORMAL:
									font_italic = FONT_STYLE_NORMAL;
									break;
								case SVGFONTSTYLE_ITALIC:
								case SVGFONTSTYLE_OBLIQUE:
									font_italic = FONT_STYLE_ITALIC;
									break;
								}
							}
						}
					}
					break;
					case Markup::SVGA_FONT_VARIANT:
					{
						// shared property
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_FONT_VARIANT)
						{
							if (enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								small_caps = parent_props.small_caps;
							}
							else
							{
								switch(enum_value->EnumValue())
								{
								case SVGFONTVARIANT_SMALLCAPS:
									small_caps = CSS_VALUE_small_caps;
									break;
								case SVGFONTVARIANT_NORMAL:
									small_caps = CSS_VALUE_normal;
									break;
								}
							}
						}
					}
					break;
					case Markup::SVGA_FONT_WEIGHT:
					{
						// shared property
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_FONT_WEIGHT)
						{
							if (enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								font_weight = parent_props.font_weight;
							}
							else
							{
								if(enum_value->EnumValue() != SVGFONTWEIGHT_UNKNOWN)
								{
									const int FontWeightMap[] = { 4, 7, CSS_VALUE_bolder, CSS_VALUE_lighter, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
									font_weight = FontWeightMap[enum_value->EnumValue()];
									if (font_weight == CSS_VALUE_bolder || font_weight == CSS_VALUE_lighter)
									{
										font_weight = styleManager->GetNextFontWeight(parent_props.font_weight, font_weight == CSS_VALUE_bolder);
									}
								}
							}
						}
					}
					break;
#ifdef SUPPORT_TEXT_DIRECTION
					case Markup::SVGA_DIRECTION:
					{
						// shared property
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if(obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_DIRECTION)
						{
							if (enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								direction = parent_props.direction;
							}
							else
							{
								switch(enum_value->EnumValue())
								{
								case CSS_VALUE_rtl:
								case CSS_VALUE_ltr:
									direction = enum_value->EnumValue();
									break;
								}
							}
						}
					}
					break;
#endif // SUPPORT_TEXT_DIRECTION
					case Markup::SVGA_LETTER_SPACING:
					{
						// shared property
						SVGNumberObject* number_value = static_cast<SVGNumberObject*>(obj);
						if(obj->Type() == SVGOBJECT_NUMBER)
						{
							if(number_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								letter_spacing = parent_props.letter_spacing;
							}
							else
							{
								letter_spacing = number_value->value.GetIntegerValue();
							}
						}
					}
					break;
					case Markup::SVGA_TEXT_DECORATION:
					{
						// shared property
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_TEXTDECORATION)
						{
							if(enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								text_decoration = parent_props.text_decoration;
							}
							else
							{
								switch(enum_value->EnumValue())
								{
								case SVGTEXTDECORATION_NONE:
									text_decoration = TEXT_DECORATION_NONE;
									break;
								case SVGTEXTDECORATION_UNDERLINE:
									text_decoration |= TEXT_DECORATION_UNDERLINE;
									break;
								case SVGTEXTDECORATION_OVERLINE:
									text_decoration |= TEXT_DECORATION_OVERLINE;
									break;
								case SVGTEXTDECORATION_LINETHROUGH:
									text_decoration |= TEXT_DECORATION_LINETHROUGH;
									break;
								case SVGTEXTDECORATION_BLINK:
									text_decoration |= TEXT_DECORATION_BLINK;
									break;
								}
							}
							current_svg_props->info.has_textdecoration = 1;
						}
					}
					break;
#ifdef SUPPORT_TEXT_DIRECTION
					case Markup::SVGA_UNICODE_BIDI:
					{
						// shared property
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_UNICODE_BIDI)
						{
							if (enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								unicode_bidi = parent_props.unicode_bidi;
							}
							else
							{
								switch(enum_value->EnumValue())
								{
								case CSS_VALUE_normal:
								case CSS_VALUE_embed:
								case CSS_VALUE_bidi_override:
									unicode_bidi = enum_value->EnumValue();
									break;
								}
							}
						}
					}
					break;
#endif // SUPPORT_TEXT_DIRECTION
					case Markup::SVGA_WORD_SPACING:
					{
						// shared property
						SVGNumberObject* number_value = static_cast<SVGNumberObject*>(obj);
						if(obj->Type() == SVGOBJECT_NUMBER)
						{
							if(number_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								word_spacing_i = parent_props.word_spacing_i;
							}
							else
								word_spacing_i = FloatToLayoutFixed(number_value->value.GetFloatValue());
						}
					}
					break;
					case Markup::SVGA_CLIP:
					{
						// shared property
						SVGRectObject* shape = static_cast<SVGRectObject*>(obj);
						if (obj->Type() == SVGOBJECT_RECT)
						{
							if (shape->Flag(SVGOBJECTFLAG_INHERIT))
							{
								clip_top = parent_props.clip_top;
								clip_right = parent_props.clip_right;
								clip_bottom = parent_props.clip_bottom;
								clip_left = parent_props.clip_left;
							}
							else
							{
								SVGRect* rect = SVGRectObject::p(shape);
								// In CSS 2.1, the only valid <shape> value is: rect(<top>, <right>, <bottom>, <left>)
								// yes, it looks a bit confusing here, but it's just how we store the attribute
								clip_top = LayoutCoord(rect->x.GetRoundedIntegerValue());
								clip_right = LayoutCoord(rect->y.GetRoundedIntegerValue());
								clip_bottom = LayoutCoord(rect->width.GetRoundedIntegerValue());
								clip_left = LayoutCoord(rect->height.GetRoundedIntegerValue());
							}
						}
					}
					break;
					case Markup::SVGA_COLOR:
					{
						// shared property
						SVGColorObject* color = static_cast<SVGColorObject*>(obj);
						if(obj->Type() == SVGOBJECT_COLOR)
						{
							if(color->Flag(SVGOBJECTFLAG_INHERIT))
							{
								font_color = parent_props.font_color;
							}
							else
							{
								UINT32 colorrep = color->GetColor().GetRGBColor();
								font_color = OP_RGB(GetSVGColorRValue(colorrep),
													GetSVGColorGValue(colorrep),
													GetSVGColorBValue(colorrep));
							}
						}
					}
					break;
					case Markup::SVGA_CURSOR:
					{
						// shared property
						SVGVector* vector = static_cast<SVGVector*>(obj);
						if(obj->Type() == SVGOBJECT_VECTOR)
						{
							for(unsigned int i = 0; i < vector->GetCount(); i++)
							{
								SVGEnum* enum_value = static_cast<SVGEnum*>(vector->Get(i));
								if (enum_value->Type() == SVGOBJECT_ENUM &&
									enum_value->EnumType() == SVGENUM_CURSOR)
								{
									element->SetHasCursorSettings(TRUE);
									cursor = (CursorType)enum_value->EnumValue();
									break;
								}
							}
						}
					}
					break;
					case Markup::SVGA_DISPLAY:
					{
						// shared property
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_DISPLAY)
						{
							if (enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								display_type = parent_props.display_type;
							}
							else
							{
								switch(enum_value->EnumValue())
								{
								case SVGDISPLAY_NONE:
									display_type = CSS_VALUE_none;
									break;
								case SVGDISPLAY_BLOCK:
									display_type = CSS_VALUE_block;
									break;
								case SVGDISPLAY_LISTITEM:
									display_type = CSS_VALUE_list_item;
									break;
								case SVGDISPLAY_RUNIN:
									display_type = CSS_VALUE_run_in;
									break;
								case SVGDISPLAY_COMPACT:
									display_type = CSS_VALUE_compact;
									break;
								case SVGDISPLAY_TABLE:
									display_type = CSS_VALUE_table;
									break;
								case SVGDISPLAY_INLINETABLE:
									display_type = CSS_VALUE_inline_table;
									break;
								case SVGDISPLAY_TABLEROWGROUP:
									display_type = CSS_VALUE_table_row_group;
									break;
								case SVGDISPLAY_TABLEHEADERGROUP:
									display_type = CSS_VALUE_table_header_group;
									break;
								case SVGDISPLAY_TABLEFOOTERGROUP:
									display_type = CSS_VALUE_table_footer_group;
									break;
								case SVGDISPLAY_TABLEROW:
									display_type = CSS_VALUE_table_row;
									break;
								case SVGDISPLAY_TABLECOLUMNGROUP:
									display_type = CSS_VALUE_table_column_group;
									break;
								case SVGDISPLAY_TABLECOLUMN:
									display_type = CSS_VALUE_table_column;
									break;
								case SVGDISPLAY_TABLECELL:
									display_type = CSS_VALUE_table_cell;
									break;
								case SVGDISPLAY_TABLECAPTION:
									display_type = CSS_VALUE_table_caption;
									break;
								case SVGDISPLAY_UNKNOWN:
								case SVGDISPLAY_MARKER: // has no corresponding CSS_VALUE
								case SVGDISPLAY_INLINE:
								default:
									display_type = CSS_VALUE_inline;
									break;
								}
							}
						}
					}
					break;
					case Markup::SVGA_OVERFLOW:
					{
						// shared property
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_OVERFLOW)
						{
							if (enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								overflow_x = parent_props.overflow_x;
								overflow_y = parent_props.overflow_y;
							}
							else
							{
								switch(enum_value->EnumValue())
								{
								case CSS_VALUE_visible:
								case CSS_VALUE_hidden:
								case CSS_VALUE_scroll:
								case CSS_VALUE_auto:
									overflow_x = overflow_y = enum_value->EnumValue();
									break;
								}
							}
						}
					}
					break;
					case Markup::SVGA_TEXT_OVERFLOW:
					{
						// shared property
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_TEXT_OVERFLOW)
						{
							switch (enum_value->EnumValue())
							{
							case CSS_VALUE_clip:
							case CSS_VALUE_ellipsis:
								text_overflow = enum_value->EnumValue();
								break;
							}
						}
					}
					break;
					case Markup::SVGA_VISIBILITY:
					{
						// shared property
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_VISIBILITY)
						{
							if (enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								visibility = parent_props.visibility;
							}
							else
							{
								switch(enum_value->EnumValue())
								{
								case SVGVISIBILITY_VISIBLE:
									visibility = CSS_VALUE_visible;
									break;
								case SVGVISIBILITY_HIDDEN:
									visibility = CSS_VALUE_hidden;
									break;
								case SVGVISIBILITY_COLLAPSE:
									visibility = CSS_VALUE_collapse;
									break;
								default:
									break;
								}
							}
						}
					}
					break;
					case Markup::SVGA_OPACITY:
					{
						// shared property
						SVGNumberObject* number_value = static_cast<SVGNumberObject*>(obj);
						if(obj->Type() == SVGOBJECT_NUMBER)
						{
							if(number_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								opacity = parent_props.opacity;
							}
							else
							{
								SVGNumber alpha_num = number_value->value;
								if (alpha_num > 1)
									alpha_num = 1;
								else if (alpha_num < 0)
									alpha_num = 0;
								opacity = (UINT8)(alpha_num * 255).GetIntegerValue();
							}
						}
					}
					break;
					case Markup::SVGA_TEXT_ALIGN:
					{
						// shared property
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_TEXT_ALIGN)
						{
							if (enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								text_align = parent_props.text_align;
							}
							else
							{
								// Should map 1:1
								text_align = (short)enum_value->EnumValue();
							}
							SetAlignSpecified();
						}
					}
					break;
					/***
					* End shared css properties.
					*
					* Here starts properties that are SVG only. The override_css flag makes us choose the attribute value
					* instead of the corresponding cssdeclaration (see SvgProperties::SetFromCSSDecl below).
					***/
					case Markup::SVGA_CLIP_PATH:
					{
						SVGString* url_str_val = static_cast<SVGString*>(obj);
						if(obj->Type() == SVGOBJECT_STRING)
						{
							if (url_str_val->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->clippath = parent_svg_props->clippath;
								current_svg_props->override.has_clippath = url_str_val->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
							}
							else if (SetUrlReference(url_str_val->GetString(), url_str_val->GetLength(), current_svg_props->clippath))
							{
								current_svg_props->override.has_clippath = url_str_val->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
							}
						}
					}
					break;
					case Markup::SVGA_CLIP_RULE:
					{
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_FILL_RULE)
						{
							if (!enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->rendinfo.cliprule = (unsigned int)enum_value->EnumValue();
							}
							current_svg_props->override.has_cliprule = enum_value->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_MASK:
					{
						SVGString* url_str_val = static_cast<SVGString*>(obj);
						if(obj->Type() == SVGOBJECT_STRING)
						{
							if (url_str_val->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->mask = parent_svg_props->mask;
								current_svg_props->override.has_mask = url_str_val->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
							}
							else if (SetUrlReference(url_str_val->GetString(), url_str_val->GetLength(), current_svg_props->mask))
							{
								current_svg_props->override.has_mask = url_str_val->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
							}
						}
					}
					break;
#ifdef SVG_SUPPORT_FILTERS
					case Markup::SVGA_ENABLE_BACKGROUND:
					{
						SVGEnableBackgroundObject* eb = static_cast<SVGEnableBackgroundObject*>(obj);
						if(obj->Type() == SVGOBJECT_ENABLE_BACKGROUND)
						{
							if(eb->Flag(SVGOBJECTFLAG_INHERIT))
								current_svg_props->enablebackground = parent_svg_props->enablebackground;
							else
								current_svg_props->enablebackground.Copy(eb->GetEnableBackground());
							current_svg_props->override.has_enablebackground = eb->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_FILTER:
					{
						SVGString* url_str_val = static_cast<SVGString*>(obj);
						if(obj->Type() == SVGOBJECT_STRING)
						{
							if (url_str_val->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->filter = parent_svg_props->filter;
								current_svg_props->override.has_filter = url_str_val->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
							}
							else if (SetUrlReference(url_str_val->GetString(), url_str_val->GetLength(), current_svg_props->filter))
							{
								current_svg_props->override.has_filter = url_str_val->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
							}
						}
					}
					break;
					case Markup::SVGA_FLOOD_COLOR:
					{
						SVGColorObject* cv = static_cast<SVGColorObject*>(obj);
						if(obj->Type() == SVGOBJECT_COLOR)
						{
							if(cv->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->floodcolor = parent_svg_props->floodcolor;
							}
							else
							{
								current_svg_props->floodcolor.Copy(cv->GetColor());
							}
							current_svg_props->override.has_floodcolor = cv->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_FLOOD_OPACITY:
					{
						SVGNumberObject* number_value = static_cast<SVGNumberObject*>(obj);
						if(obj->Type() == SVGOBJECT_NUMBER)
						{
							if(number_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->floodopacity = parent_svg_props->floodopacity;
							}
							else
							{
								SVGNumber alpha_num = number_value->value;
								if (alpha_num > 1)
									alpha_num = 1;
								else if (alpha_num < 0)
									alpha_num = 0;
								current_svg_props->floodopacity = (UINT8)(alpha_num * 255).GetIntegerValue();
							}
							current_svg_props->override.has_floodopacity = number_value->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_LIGHTING_COLOR:
					{
						SVGColorObject* cv = static_cast<SVGColorObject*>(obj);
						if(obj->Type() == SVGOBJECT_COLOR)
						{
							if(cv->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->lightingcolor = parent_svg_props->lightingcolor;
							}
							else
							{
								current_svg_props->lightingcolor.Copy(cv->GetColor());
							}
							current_svg_props->override.has_lightingcolor = cv->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
#endif // SVG_SUPPORT_FILTERS
					case Markup::SVGA_VIEWPORT_FILL:
					{
						SVGColorObject* cv = static_cast<SVGColorObject*>(obj);
						if(obj->Type() == SVGOBJECT_COLOR)
						{
							if(cv->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->viewportfill = parent_svg_props->viewportfill;
							}
							else
							{
								current_svg_props->viewportfill.Copy(cv->GetColor());
							}
							current_svg_props->override2.has_viewportfill = cv->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_VIEWPORT_FILL_OPACITY:
					{
						SVGNumberObject* number_value = static_cast<SVGNumberObject*>(obj);
						if(obj->Type() == SVGOBJECT_NUMBER)
						{
							if(number_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->viewportfillopacity = parent_svg_props->viewportfillopacity;
							}
							else
							{
								SVGNumber alpha_num = number_value->value;
								if (alpha_num > 1)
									alpha_num = 1;
								else if (alpha_num < 0)
									alpha_num = 0;
								current_svg_props->viewportfillopacity = (UINT8)(alpha_num * 255).GetIntegerValue();
							}
							current_svg_props->override2.has_viewportfillopacity = number_value->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_STOP_COLOR:
					{
						SVGColorObject* cv = static_cast<SVGColorObject*>(obj);
						if(obj->Type() == SVGOBJECT_COLOR)
						{
							if(cv->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->stopcolor.Copy(parent_svg_props->stopcolor);
							}
							else
							{
								current_svg_props->stopcolor.Copy(cv->GetColor());
							}
							current_svg_props->override.has_stopcolor = cv->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_STOP_OPACITY:
					{
						SVGNumberObject* number_value = static_cast<SVGNumberObject*>(obj);
						if(obj->Type() == SVGOBJECT_NUMBER)
						{
							if(number_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->stopopacity = parent_svg_props->stopopacity;
							}
							else
							{
								SVGNumber alpha_num = number_value->value;
								if (alpha_num > 1)
									alpha_num = 1;
								else if (alpha_num < 0)
									alpha_num = 0;
								current_svg_props->stopopacity = (UINT8)(alpha_num * 255).GetIntegerValue();
							}
							current_svg_props->override.has_stopopacity = number_value->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_SOLID_COLOR:
					{
						SVGColorObject* cv = static_cast<SVGColorObject*>(obj);
						if(obj->Type() == SVGOBJECT_COLOR)
						{
							if(cv->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->solidcolor.Copy(parent_svg_props->solidcolor);
							}
							else
							{
								current_svg_props->solidcolor.Copy(cv->GetColor());
							}
							current_svg_props->override2.has_solidcolor = cv->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_SOLID_OPACITY:
					{
						SVGNumberObject* number_value = static_cast<SVGNumberObject*>(obj);
						if(obj->Type() == SVGOBJECT_NUMBER)
						{
							if(number_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->solidopacity = parent_svg_props->solidopacity;
							}
							else
							{
								SVGNumber alpha_num = number_value->value;
								if (alpha_num > 1)
									alpha_num = 1;
								else if (alpha_num < 0)
									alpha_num = 0;
								current_svg_props->solidopacity = (UINT8)(alpha_num * 255).GetIntegerValue();
							}
							current_svg_props->override2.has_solidopacity = number_value->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_POINTER_EVENTS:
					{
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_POINTER_EVENTS)
						{
							if (!enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->rendinfo.pointerevents = (unsigned int)enum_value->EnumValue();
							}
							current_svg_props->override.has_pointerevents = enum_value->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;

					/***
					'color-interpolation'
					'color-profile'
					'color-rendering'
					***/

					case Markup::SVGA_COLOR_INTERPOLATION_FILTERS:
					{
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_COLOR_INTERPOLATION)
						{
							if (!enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->rendinfo.color_interpolation_filters = (unsigned int)enum_value->EnumValue();
							}
							current_svg_props->override2.has_color_interpolation_filters = enum_value->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_FILL:
					{
						SVGPaintObject *p = static_cast<SVGPaintObject*>(obj);
						if(obj->Type() == SVGOBJECT_PAINT)
						{
							OP_STATUS status = OpStatus::OK;
							SVGPropertyReference::DecRef(current_svg_props->fill);
							if (p->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->fill = SVGPropertyReference::IncRef(parent_svg_props->fill);
							}
							else
							{
								// Set error flags itself (on OOM)
								status = NewPaintProp(*current_svg_props, current_svg_props->fill, p->GetPaint(), TRUE);
							}
							if (OpStatus::IsSuccess(status))
							{
								current_svg_props->info.has_fill = 1;
								current_svg_props->fill_seq++;
								current_svg_props->override.has_fill = p->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
							}
						}
					}
					break;
					case Markup::SVGA_FILL_OPACITY:
					{
						SVGNumberObject* number_value = static_cast<SVGNumberObject*>(obj);
						if(obj->Type() == SVGOBJECT_NUMBER)
						{
							if(!number_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								SVGNumber alpha_num = number_value->value;
								if (alpha_num > 1)
									alpha_num = 1;
								else if (alpha_num < 0)
									alpha_num = 0;
								current_svg_props->fillopacity = (UINT8)(alpha_num * 255).GetIntegerValue();
							}
							current_svg_props->override.has_fillopacity = number_value->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_FILL_RULE:
					{
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_FILL_RULE)
						{
							if (!enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->rendinfo.fillrule = (unsigned int)enum_value->EnumValue();
							}
							current_svg_props->override.has_fillrule = enum_value->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_IMAGE_RENDERING:
					{
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_IMAGE_RENDERING)
						{
							if (enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								image_rendering = parent_props.image_rendering;
							}
							else
							{
								switch (enum_value->EnumValue())
								{
								default:
								case SVGIMAGERENDERING_AUTO:
									image_rendering = CSS_VALUE_auto;
									break;
								case SVGIMAGERENDERING_OPTIMIZESPEED:
									image_rendering = CSS_VALUE_optimizeSpeed;
									break;
								case SVGIMAGERENDERING_OPTIMIZEQUALITY:
									image_rendering = CSS_VALUE_optimizeQuality;
									break;
								}
							}

							current_svg_props->override.has_imagerendering = enum_value->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;

					case Markup::SVGA_SHAPE_RENDERING:
					{
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_SHAPE_RENDERING)
						{
							if (!enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->rendinfo.shaperendering = (unsigned int)enum_value->EnumValue();
							}
							current_svg_props->override2.has_shaperendering = enum_value->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;

					case Markup::SVGA_BUFFERED_RENDERING:
					{
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_BUFFERED_RENDERING)
						{
							if (!enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->rendinfo.bufferedrendering = (unsigned int)enum_value->EnumValue();
							}
							current_svg_props->override2.has_bufferedrendering = enum_value->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_MARKER_START:
					{
						SVGString* url_str_val = static_cast<SVGString*>(obj);
						if(obj->Type() == SVGOBJECT_STRING)
						{
							if (url_str_val->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->markerstart = parent_svg_props->markerstart;
								current_svg_props->override.has_markerstart = url_str_val->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
							}
							else if (SetUrlReference(url_str_val->GetString(), url_str_val->GetLength(), current_svg_props->markerstart))
							{
								current_svg_props->override.has_markerstart = url_str_val->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
							}
						}
					}
					break;
					case Markup::SVGA_MARKER_MID:
					{
						SVGString* url_str_val = static_cast<SVGString*>(obj);
						if(obj->Type() == SVGOBJECT_STRING)
						{
							if (url_str_val->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->markermid = parent_svg_props->markermid;
								current_svg_props->override.has_markermid = url_str_val->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
							}
							else if (SetUrlReference(url_str_val->GetString(), url_str_val->GetLength(), current_svg_props->markermid))
							{
								current_svg_props->override.has_markermid = url_str_val->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
							}
						}
					}
					break;
					case Markup::SVGA_MARKER_END:
					{
						SVGString* url_str_val = static_cast<SVGString*>(obj);
						if(obj->Type() == SVGOBJECT_STRING)
						{
							if (url_str_val->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->markerend = parent_svg_props->markerend;
								current_svg_props->override.has_markerend = url_str_val->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
							}
							else if (SetUrlReference(url_str_val->GetString(), url_str_val->GetLength(), current_svg_props->markerend))
							{
								current_svg_props->override.has_markerend = url_str_val->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
							}
						}
					}
					break;
					case Markup::SVGA_STROKE:
					{
						SVGPaintObject *p = static_cast<SVGPaintObject*>(obj);
						if(obj->Type() == SVGOBJECT_PAINT)
						{
							OP_STATUS status = OpStatus::OK;
							SVGPropertyReference::DecRef(current_svg_props->stroke);
							if (p->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->stroke = SVGPropertyReference::IncRef(parent_svg_props->stroke);
							}
							else
							{
								// Set error flags itself (on OOM)
								status = NewPaintProp(*current_svg_props, current_svg_props->stroke, p->GetPaint(), FALSE);
							}
							if (OpStatus::IsSuccess(status))
							{
								current_svg_props->info.has_stroke = 1;
								current_svg_props->stroke_seq++;
								current_svg_props->override.has_stroke = p->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
							}
						}
					}
					break;
					case Markup::SVGA_STROKE_DASHARRAY:
					{
						// Copy the from the attribute value of
						// Markup::SVGA_STROKE_DASHARRAY into a SVGLength* array and put it
						// into the layout properties structure.

						// FIXME: This part wants to be able to throw OOM.
						SVGVector* da = static_cast<SVGVector*>(obj);
						if(obj->Type() == SVGOBJECT_VECTOR)
						{
							OP_STATUS status = OpStatus::OK;
							SVGPropertyReference::DecRef(current_svg_props->dasharray);
							if(da->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->dasharray = SVGPropertyReference::IncRef(parent_svg_props->dasharray);
							}
							else if (da->IsNone())
							{
								current_svg_props->dasharray = &g_svg_manager_impl->dasharray_isnone_prop;
							}
							else
							{
								status = NewDashArrayProp(*current_svg_props, da);
							}

							if (OpStatus::IsSuccess(status))
							{
								current_svg_props->dasharray_seq++;
								current_svg_props->info.has_dasharray = 1;
								current_svg_props->override.has_dasharray = da->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
							}
						}
					}
					break;
					case Markup::SVGA_STROKE_DASHOFFSET:
					{
						SVGLengthObject* svgdo = static_cast<SVGLengthObject*>(obj);
						if(obj->Type() == SVGOBJECT_LENGTH)
						{
							if(!svgdo->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->dashoffset = svgdo->GetLength();
							}

							current_svg_props->info.has_dashoffset = 1;
							current_svg_props->override.has_dashoffset = svgdo->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_STROKE_LINECAP:
					{
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_STROKE_LINECAP)
						{
							if (!enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->rendinfo.linecap = (unsigned int)enum_value->EnumValue();
							}
							current_svg_props->override.has_linecap = enum_value->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_STROKE_LINEJOIN:
					{
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_STROKE_LINEJOIN)
						{
							if (!enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->rendinfo.linejoin = (unsigned int)enum_value->EnumValue();
							}
							current_svg_props->override.has_linejoin = enum_value->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_STROKE_MITERLIMIT:
					{
						SVGNumberObject* number_value = static_cast<SVGNumberObject*>(obj);
						if(obj->Type() == SVGOBJECT_NUMBER)
						{
							if(!number_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->miterlimit = number_value->value;
							}
							current_svg_props->override.has_miterlimit = number_value->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_STROKE_OPACITY:
					{
						SVGNumberObject* number_value = static_cast<SVGNumberObject*>(obj);
						if(obj->Type() == SVGOBJECT_NUMBER)
						{
							if(!number_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								SVGNumber alpha_num = number_value->value;
								if (alpha_num > 1)
									alpha_num = 1;
								else if (alpha_num < 0)
									alpha_num = 0;
								current_svg_props->strokeopacity = (UINT8)(alpha_num * 255).GetIntegerValue();
							}
							current_svg_props->override.has_strokeopacity = number_value->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_STROKE_WIDTH:
					{
						SVGLengthObject* length = static_cast<SVGLengthObject*>(obj);
						if(obj->Type() == SVGOBJECT_LENGTH)
						{
							if(!length->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->linewidth = length->GetLength();
							}

							current_svg_props->info.has_linewidth = 1;
							current_svg_props->override.has_linewidth = length->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;

					case Markup::SVGA_TEXT_RENDERING:
					{
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_TEXT_RENDERING)
						{
							if (!enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->textinfo.textrendering = (unsigned int)enum_value->EnumValue();
							}
							current_svg_props->override2.has_textrendering = enum_value->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;

					case Markup::SVGA_GLYPH_ORIENTATION_HORIZONTAL:
					case Markup::SVGA_GLYPH_ORIENTATION_VERTICAL:
					{
						SVGOrient* orient = static_cast<SVGOrient *>(obj);
						if (obj->Type() == SVGOBJECT_ORIENT)
						{
							if (!orient->Flag(SVGOBJECTFLAG_INHERIT))
							{
								unsigned angle_quadrant = 0;
								BOOL is_auto = TRUE;

								if (orient->GetOrientType() == SVGORIENT_ANGLE)
								{
									SVGNumber angle_deg;
									if (SVGAngle* angle = orient->GetAngle())
										angle_deg = angle->GetAngleInUnits(SVGANGLE_DEG);

									angle_quadrant = SVGAngle::QuantizeAngle90Deg(angle_deg.GetFloatValue());
									is_auto = FALSE;
								}
								// 'auto' is the default for vertical,
								// and we don't care about it for
								// horizontal.

								if (attr_name == Markup::SVGA_GLYPH_ORIENTATION_HORIZONTAL)
								{
									current_svg_props->textinfo.glyph_orientation_horizontal = angle_quadrant;
								}
								else
								{
									current_svg_props->textinfo.glyph_orientation_vertical = angle_quadrant;
									current_svg_props->textinfo.glyph_orientation_vertical_is_auto = !!is_auto;
								}
							}
							// inherited by default, no need for has-flag
							// not animatable, doesn't need override flag
						}
					}
					break;

					/*
					'kerning'
					***/
					case Markup::SVGA_ALIGNMENT_BASELINE:
					{
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_ALIGNMENT_BASELINE)
						{
							if (enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->textinfo.alignmentbaseline = parent_svg_props->textinfo.alignmentbaseline;
							}
							else
							{
								current_svg_props->textinfo.alignmentbaseline = (unsigned int)enum_value->EnumValue();
							}
						}
					}
					break;
					case Markup::SVGA_DOMINANT_BASELINE:
					{
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_DOMINANT_BASELINE)
						{
							if (enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->textinfo.dominantbaseline = parent_svg_props->textinfo.dominantbaseline;
							}
							else
							{
								current_svg_props->textinfo.dominantbaseline = (unsigned int)enum_value->EnumValue();
							}
						}
					}
					break;
					case Markup::SVGA_BASELINE_SHIFT:
					{
						SVGBaselineShiftObject* bls = static_cast<SVGBaselineShiftObject*>(obj);
						if (obj->Type() == SVGOBJECT_BASELINE_SHIFT)
						{
							if (bls->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->baselineshift = parent_svg_props->baselineshift;
							}
							else
							{
								current_svg_props->baselineshift = bls->GetBaselineShift();
							}
							current_svg_props->override.has_baselineshift = bls->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_TEXT_ANCHOR:
					{
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_TEXT_ANCHOR)
						{
							if (!enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->textinfo.textanchor = (unsigned int)enum_value->EnumValue();
							}
							current_svg_props->override.has_textanchor = enum_value->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_WRITING_MODE:
					{
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_WRITING_MODE)
						{
							if (!enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->textinfo.writingmode = (unsigned int)enum_value->EnumValue();
							}
							current_svg_props->override.has_writingmode = enum_value->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_VECTOR_EFFECT:
					{
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_VECTOR_EFFECT)
						{
							if (enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->rendinfo.vector_effect = parent_svg_props->rendinfo.vector_effect;
							}
							else
							{
								current_svg_props->rendinfo.vector_effect = (unsigned int)enum_value->EnumValue();
							}
							current_svg_props->override2.has_vector_effect = enum_value->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_AUDIO_LEVEL:
					{
						SVGNumberObject* number_value = static_cast<SVGNumberObject*>(obj);
						if(obj->Type() == SVGOBJECT_NUMBER)
						{
							if(number_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->audiolevel = parent_svg_props->audiolevel;
							}
							else
							{
								current_svg_props->audiolevel = number_value->value;
							}
							current_svg_props->computed_audiolevel *= current_svg_props->audiolevel;
							current_svg_props->override2.has_audio_level = number_value->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_LINE_INCREMENT:
					{
						if (obj->Flag(SVGOBJECTFLAG_INHERIT))
						{
							current_svg_props->override2.has_lineincrement = obj->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
						else if(obj->Type() == SVGOBJECT_PROXY)
						{
							SVGObject* real_obj = static_cast<SVGProxyObject*>(obj)->GetRealObject();
							if (!real_obj)
								break;

							if (real_obj->Type() == SVGOBJECT_NUMBER)
							{
								current_svg_props->lineincrement = static_cast<SVGNumberObject*>(real_obj)->value;
								current_svg_props->textinfo.lineincrement_is_auto = 0;
							}
							else if (real_obj->Type() == SVGOBJECT_ENUM &&
									static_cast<SVGEnum*>(real_obj)->EnumType() == SVGENUM_AUTO)
							{
								current_svg_props->lineincrement = 0; // Or someone will surely complain...
								current_svg_props->textinfo.lineincrement_is_auto = 1;
							}
							current_svg_props->override2.has_lineincrement = obj->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
					case Markup::SVGA_DISPLAY_ALIGN:
					{
						SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
						if (obj->Type() == SVGOBJECT_ENUM &&
							enum_value->EnumType() == SVGENUM_DISPLAY_ALIGN)
						{
							if (enum_value->Flag(SVGOBJECTFLAG_INHERIT))
							{
								current_svg_props->textinfo.displayalign = parent_svg_props->textinfo.displayalign;
							}
							else
							{
								current_svg_props->textinfo.displayalign = (unsigned int)enum_value->EnumValue();
							}
							current_svg_props->override2.has_displayalign = enum_value->Flag(SVGOBJECTFLAG_IS_CSSPROP) ? 1 : 0;
						}
					}
					break;
				}
			}
		}

		if (elm_ctx && g_svg_manager_impl->GetPresentationValues())
		{
		  elm_ctx->SetHasTrustworthyPresAttrs(TRUE);
		  elm_ctx->SetHasPresentationAttributes(found_pres_attrs);
		}
	}

	delete parent_svg_props_dealloc;

	return OpStatus::OK;
}

void*
SvgProperties::operator new(size_t nbytes) OP_NOTHROW
{
	return g_svg_properties_pool->New(sizeof(SvgProperties));
}

void
SvgProperties::operator delete(void *p, size_t nbytes)
{
	g_svg_properties_pool->Delete(p);
}

SvgProperties::SvgProperties()
	: fill_seq(0)
	, stroke_seq(0)
	, dasharray_seq(0)
	, rendinfo_init(0)
	, textinfo_init(0)
	, info_init(0)
	, override_init(0)
	, override2_init(0)
{
	fill = &g_svg_manager_impl->default_fill_prop;
	stroke = &g_svg_manager_impl->default_stroke_prop;
	dasharray = &g_svg_manager_impl->dasharray_isnone_prop;
}

SvgProperties::SvgProperties(const SvgProperties& src)
{
	fill = &g_svg_manager_impl->default_fill_prop;
	stroke = &g_svg_manager_impl->default_stroke_prop;
	dasharray = &g_svg_manager_impl->dasharray_isnone_prop;

	fill_seq = stroke_seq = dasharray_seq = 0;
	Reset(&src);
}

SvgProperties::~SvgProperties()
{
	SVGPropertyReference::DecRef(fill);
	SVGPropertyReference::DecRef(stroke);
	SVGPropertyReference::DecRef(dasharray);
}

OP_STATUS
SvgProperties::GetError() const
{
	if(info.had_hard_error)
	{
		if(info.had_oom_error)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		else
		{
			return OpStatus::ERR;
		}
	}

	return OpStatus::OK;
}

//
// Inherited properties:
// http://www.w3.org/TR/SVG11/propidx.html [1.1]
// http://www.w3.org/TR/SVGMobile12/attributeTable.html#PropertyTable [1.2T]
//
// clip-rule
// color
// color-interpolation
// color-interpolation-filters
// color-profile
// color-rendering
// cursor
// direction
// display-align [1.2T]
// fill
// fill-opacity
// fill-rule
// font
// font-family
// font-size (computed value)
// font-size-adjust
// font-stretch
// font-style
// font-variant
// font-weight
// glyph-orientation-horizontal
// glyph-orientation-vertical
// image-rendering
// kerning
// letter-spacing
// line-increment [1.2T]
// marker
// marker-*
// pointer-events
// shape-rendering
// stroke
// stroke-dasharray
// stroke-dashoffset
// stroke-linecap
// stroke-linejoin
// stroke-miterlimit
// stroke-opacity
// stroke-width
// text-anchor
// text-rendering
// visibility
// word-spacing
// writing-mode
//

void SvgProperties::Reset(const SvgProperties *parent_sp)
{
	SVGPropertyReference::DecRef(fill);
	SVGPropertyReference::DecRef(stroke);
	SVGPropertyReference::DecRef(dasharray);

	if(parent_sp)
	{
		fill = SVGPropertyReference::IncRef(parent_sp->fill);
		stroke = SVGPropertyReference::IncRef(parent_sp->stroke);
		fillopacity = parent_sp->fillopacity;
		strokeopacity = parent_sp->strokeopacity;

		dasharray = SVGPropertyReference::IncRef(parent_sp->dasharray);
		dashoffset = parent_sp->dashoffset;

		miterlimit = parent_sp->miterlimit;
		linewidth = parent_sp->linewidth;

		marker = parent_sp->marker;
		markerstart = parent_sp->markerstart;
		markermid = parent_sp->markermid;
		markerend = parent_sp->markerend;

		fontsize = parent_sp->fontsize;

		lineincrement = parent_sp->lineincrement;

		rendinfo_init = parent_sp->rendinfo_init;
		textinfo_init = parent_sp->textinfo_init;

		info_init = parent_sp->info_init;
		info.has_textdecoration = 0;
		override_init = parent_sp->override_init;
		override2_init = parent_sp->override2_init;

		fill_seq = parent_sp->fill_seq;
		stroke_seq = parent_sp->stroke_seq;
		dasharray_seq = parent_sp->dasharray_seq;

		computed_audiolevel = parent_sp->computed_audiolevel;

		// Not inherited
		rendinfo.vector_effect = SVGVECTOREFFECT_NONE;
		rendinfo.bufferedrendering = SVGBUFFEREDRENDERING_AUTO;

		override.has_filter = 0;
		override.has_clippath = 0;
		override.has_mask = 0;
		override.has_alignmentbaseline = 0;
		override.has_dominantbaseline = 0;
		override.has_baselineshift = 0;
		override.has_floodcolor = 0;
		override.has_floodopacity = 0;
		override.has_lightingcolor = 0;
		override.has_stopcolor = 0;
		override.has_stopopacity = 0;
		override.has_enablebackground = 0;
		override2.has_vector_effect = 0;
		override2.has_audio_level = 0;
		override2.has_viewportfill = 0;
		override2.has_viewportfillopacity = 0;
		override2.has_solidcolor = 0;
		override2.has_solidopacity = 0;
		override2.has_bufferedrendering = 0;
	}
	else
	{
		fill = &g_svg_manager_impl->default_fill_prop;
		stroke = &g_svg_manager_impl->default_stroke_prop;
		dasharray = &g_svg_manager_impl->dasharray_isnone_prop;

		fill_seq = stroke_seq = dasharray_seq = 0;

		rendinfo_init = textinfo_init = 0;

		info_init = 0;
		override_init = override2_init = 0;

		clippath = g_svg_manager_impl->default_url_reference_prop;
		mask = g_svg_manager_impl->default_url_reference_prop;
		filter = g_svg_manager_impl->default_url_reference_prop;

		marker = g_svg_manager_impl->default_url_reference_prop;
		markerstart = g_svg_manager_impl->default_url_reference_prop;
		markermid = g_svg_manager_impl->default_url_reference_prop;
		markerend = g_svg_manager_impl->default_url_reference_prop;

		textinfo.textrendering = SVGTEXTRENDERING_AUTO;
		textinfo.alignmentbaseline = SVGALIGNMENTBASELINE_AUTO;
		textinfo.dominantbaseline = SVGDOMINANTBASELINE_AUTO;
		textinfo.writingmode = SVGWRITINGMODE_LR_TB;
		textinfo.textanchor = SVGTEXTANCHOR_START;
		textinfo.displayalign = SVGDISPLAYALIGN_AUTO;
		textinfo.lineincrement_is_auto = 1;
		textinfo.glyph_orientation_horizontal = 0;
		textinfo.glyph_orientation_vertical = 0;
		textinfo.glyph_orientation_vertical_is_auto = 1;

		rendinfo.pointerevents = SVGPOINTEREVENTS_VISIBLEPAINTED;
		rendinfo.linecap = SVGCAP_BUTT;
		rendinfo.linejoin = SVGJOIN_MITER;
		rendinfo.cliprule = SVGFILL_NON_ZERO;
		rendinfo.fillrule = SVGFILL_NON_ZERO;
		rendinfo.color_interpolation_filters = SVGCOLORINTERPOLATION_LINEARRGB;
		rendinfo.vector_effect = SVGVECTOREFFECT_NONE;
		rendinfo.shaperendering = SVGSHAPERENDERING_AUTO;
		rendinfo.bufferedrendering = SVGBUFFEREDRENDERING_AUTO;

		miterlimit = 4;

		dashoffset.SetScalar(0);
		dashoffset.SetUnit(CSS_NUMBER);
		linewidth.SetScalar(1);
		linewidth.SetUnit(CSS_NUMBER);

		baselineshift.SetShiftType(SVGBaselineShift::SVGBASELINESHIFT_BASELINE);
#ifdef SVG_SUPPORT_FILTERS
		enablebackground.SetType(SVGEnableBackground::SVGENABLEBG_ACCUMULATE);
#endif // SVG_SUPPORT_FILTERS

		fontsize = 12;

		floodcolor.SetColorType(SVGColor::SVGCOLOR_RGBCOLOR);
		floodcolor.SetRGBColor(0, 0, 0); // Black
		lightingcolor.SetColorType(SVGColor::SVGCOLOR_RGBCOLOR);
		lightingcolor.SetRGBColor(255, 255, 255); // White
		stopcolor.SetColorType(SVGColor::SVGCOLOR_RGBCOLOR);
		stopcolor.SetRGBColor(0,0,0); // Black
		viewportfill.SetColorType(SVGColor::SVGCOLOR_NONE);
		solidcolor.SetColorType(SVGColor::SVGCOLOR_RGBCOLOR);
		solidcolor.SetRGBColor(0,0,0); // Black

		stopopacity = 255;
		fillopacity = 255;
		strokeopacity = 255;
		floodopacity = 255;
		viewportfillopacity = 255;
		solidopacity = 255;

		audiolevel = 1;
		computed_audiolevel = audiolevel;

		lineincrement = 0;
	}
}

static BOOL SetOpacity(CSS_decl* cd, UINT8& res, UINT8 parentopacity)
{
	if(cd->GetDeclType() == CSS_DECL_NUMBER)
	{
		float opacity = cd->GetNumberValue(0);
		res = UINT8(opacity*255);
		return TRUE;
	}
	else if(cd->GetDeclType() == CSS_DECL_TYPE && cd->TypeValue() == CSS_VALUE_inherit)
	{
		res = parentopacity;
		return TRUE;
	}

	return FALSE;
}

static BOOL SetFillRule(CSS_decl* cd, SVGFillRule& res, SVGFillRule parentfill)
{
	if(cd->GetDeclType() == CSS_DECL_TYPE)
	{
		switch(cd->TypeValue())
		{
			case CSS_VALUE_evenodd:
				res = SVGFILL_EVEN_ODD;
				return TRUE;
			case CSS_VALUE_nonzero:
				res = SVGFILL_NON_ZERO;
				return TRUE;
			case CSS_VALUE_inherit:
				res = parentfill;
				return TRUE;
		}
	}
	return FALSE;
}

static CSS_decl* GetFillRule(short property, SVGFillRule in_type)
{
	switch(in_type)
	{
		case SVGFILL_EVEN_ODD:
			return OP_NEW(CSS_type_decl, (property, CSS_VALUE_evenodd));
		case SVGFILL_NON_ZERO:
			return OP_NEW(CSS_type_decl, (property, CSS_VALUE_nonzero));
		default:
			return NULL;
	}
}

static BOOL SetLength(CSS_decl* cd, SVGLength& res, const SVGLength& parent)
{
	if(cd->GetDeclType() == CSS_DECL_NUMBER)
	{
		res.SetScalar(cd->GetNumberValue(0));
		res.SetUnit(cd->GetValueType(0));
		return TRUE;
	}
	else if(cd->GetDeclType() == CSS_DECL_TYPE && cd->TypeValue() == CSS_VALUE_inherit)
	{
		res = parent;
		return TRUE;
	}
	return FALSE;
}

static BOOL SetNumber(CSS_decl* cd, SVGNumber& res, SVGNumber parent)
{
	if(cd->GetDeclType() == CSS_DECL_NUMBER)
	{
		res = cd->GetNumberValue(0);
		return TRUE;
	}
	else if(cd->GetDeclType() == CSS_DECL_TYPE && cd->TypeValue() == CSS_VALUE_inherit)
	{
		res = parent;
		return TRUE;
	}
	return FALSE;
}

static BOOL SetURLReference(const URL& root_url, CSS_decl* cd, SVGURLReference& urlref, const SVGURLReference& parent)
{
	if(cd->GetDeclType() == CSS_DECL_TYPE)
	{
		if(cd->TypeValue() == CSS_VALUE_none)
			urlref.info.is_none = TRUE;
		else if(cd->TypeValue() == CSS_VALUE_inherit)
			urlref = parent;
		return TRUE;
	}
	else if(cd->GetDeclType() == CSS_DECL_STRING)
	{
		urlref.info.is_none = FALSE; // Just to make sure.
		urlref.url_str = cd->StringValue();
		urlref.info.url_str_len = urlref.url_str ? uni_strlen(urlref.url_str) : 0;
		return TRUE;
	}
	return FALSE;
}

static CSS_decl* GetURLReference(short property, const SVGURLReference& urlref)
{
	if(urlref.info.is_none)
	{
		return OP_NEW(CSS_type_decl, (property, CSS_VALUE_none));
	}
	else
	{
		CSS_string_decl *out_cd = OP_NEW(CSS_string_decl, (property, CSS_string_decl::StringDeclUrl));
		if (out_cd && OpStatus::IsMemoryError(out_cd->CopyAndSetString(urlref.url_str, urlref.info.url_str_len)))
		{
			OP_DELETE(out_cd);
			out_cd = NULL;
		}
		return out_cd;
	}
}

static unsigned GetQuantizedAngleFromCSSDecl(CSS_decl* cd)
{
	OP_ASSERT(cd->GetDeclType() == CSS_DECL_NUMBER);
	// The CSS parser normalizes degrees/grads/turns/et.c to radians
	OP_ASSERT(cd->GetValueType(0) == CSS_NUMBER || cd->GetValueType(0) == CSS_RAD);

	float angle_deg = cd->GetNumberValue(0);
	if (cd->GetValueType(0) == CSS_RAD)
		angle_deg = (float)(angle_deg * 180 / M_PI);

	return SVGAngle::QuantizeAngle90Deg(angle_deg);
}

/**
* This method is called for svg-specific css properties.
*/
OP_STATUS SvgProperties::SetFromCSSDecl(CSS_decl* cd, LayoutProperties* parent_cascade,
										HTML_Element* element, HLDocProfile* hld_profile)
{
	const HTMLayoutProperties& parent_props = *parent_cascade->GetProps();

	const SvgProperties *parent_svg_props = NULL;

	SvgProperties *parent_svg_props_dealloc = NULL;
	parent_svg_props = parent_props.svg;

	if (!parent_svg_props)
	{
		if (!HTMLayoutProperties::AllocateSVGProps(parent_svg_props_dealloc, NULL))
			return OpStatus::ERR_NO_MEMORY;
		parent_svg_props = parent_svg_props_dealloc;
	}

	OP_STATUS status = OpStatus::OK;

	if(cd->GetProperty() == CSS_PROPERTY_fill && !override.has_fill)
	{
		SVGPropertyReference::DecRef(fill);
		status = SVGPaint::CloneFromCSSDecl(cd, fill, parent_svg_props->GetFill());
		if (OpStatus::IsSuccess(status))
		{
			info.has_fill = 1;
			fill_seq++;
		}
	}
	else if(cd->GetProperty() == CSS_PROPERTY_stroke && !override.has_stroke)
	{
		SVGPropertyReference::DecRef(stroke);
		status = SVGPaint::CloneFromCSSDecl(cd, stroke, parent_svg_props->GetStroke());
		if (OpStatus::IsSuccess(status))
		{
			info.has_stroke = 1;
			stroke_seq++;
		}
	}
	else if(cd->GetProperty() == CSS_PROPERTY_fill_opacity && !override.has_fillopacity)
	{
		SetOpacity(cd, fillopacity, parent_svg_props->fillopacity);
	}
	else if(cd->GetProperty() == CSS_PROPERTY_stroke_opacity && !override.has_strokeopacity)
	{
		SetOpacity(cd, strokeopacity, parent_svg_props->strokeopacity);
	}
	else if(cd->GetProperty() == CSS_PROPERTY_stroke_width && !override.has_linewidth)
	{
		if(SetLength(cd, linewidth, parent_svg_props->linewidth))
			info.has_linewidth = 1;
	}
	else if(cd->GetProperty() == CSS_PROPERTY_fill_rule && !override.has_fillrule)
	{
		SVGFillRule tmp_fillrule;
		if(SetFillRule(cd, tmp_fillrule, (SVGFillRule)parent_svg_props->rendinfo.fillrule))
		{
			rendinfo.fillrule = tmp_fillrule;
		}
	}
	else if(cd->GetProperty() == CSS_PROPERTY_stroke_dashoffset && !override.has_dashoffset)
	{
		if(SetLength(cd, dashoffset, parent_svg_props->dashoffset))
			info.has_dashoffset = 1;
	}
	else if(cd->GetProperty() == CSS_PROPERTY_stroke_dasharray && !override.has_dasharray)
	{
		SVGPropertyReference::DecRef(dasharray);
		dasharray = &g_svg_manager_impl->dasharray_isnone_prop;
 		if(cd->GetDeclType() == CSS_DECL_GEN_ARRAY)
		{
			short num = cd->ArrayValueLen();
			const CSS_generic_value* gen_arr = cd->GenArrayValue();

			if(num > 0)
			{
				SVGDashArray* da = OP_NEW(SVGDashArray, ());
				if (!da)
				{
					status = OpStatus::ERR_NO_MEMORY;
					goto cleanup_props;
				}

				SVGPropertyReference::IncRef(da);

				da->dash_array = OP_NEWA(SVGLength, num);

				if (!da->dash_array)
				{
					SVGPropertyReference::DecRef(da);
					status = OpStatus::ERR_NO_MEMORY;
					goto cleanup_props;
				}

				short j = 0;
				for(short i = 0; i < num; i++)
				{
					if(CSS_is_number(gen_arr[i].value_type))
					{
						da->dash_array[j++] = SVGLength(gen_arr[i].value.real, gen_arr[i].value_type);
					}
				}

				da->dash_array_size = j;

				info.has_dasharray = 1;

				dasharray_seq++;

				dasharray = da;
			}
		}
		else if(cd->GetDeclType() == CSS_DECL_TYPE)
		{
			if(cd->TypeValue() == CSS_VALUE_none)
			{
				dasharray = &g_svg_manager_impl->dasharray_isnone_prop;
			}
			else if(cd->TypeValue() == CSS_VALUE_inherit)
			{
				dasharray = SVGPropertyReference::IncRef(parent_svg_props->dasharray);
			}

			info.has_dasharray = 1;
			dasharray_seq++;
		}
	}
	else if(cd->GetProperty() == CSS_PROPERTY_stroke_miterlimit && !override.has_miterlimit)
	{
		SetNumber(cd, miterlimit, parent_svg_props->miterlimit);
	}
	else if(cd->GetProperty() == CSS_PROPERTY_stroke_linecap && !override.has_linecap)
	{
		if(cd->GetDeclType() == CSS_DECL_TYPE)
		{
			switch(cd->TypeValue())
			{
				case CSS_VALUE_round:
					rendinfo.linecap = SVGCAP_ROUND;
					break;
				case CSS_VALUE_butt:
					rendinfo.linecap = SVGCAP_BUTT;
					break;
				case CSS_VALUE_square:
					rendinfo.linecap = SVGCAP_SQUARE;
					break;
				case CSS_VALUE_inherit:
					rendinfo.linecap = parent_svg_props->rendinfo.linecap;
					break;
			}
		}
	}
	else if(cd->GetProperty() == CSS_PROPERTY_stroke_linejoin && !override.has_linejoin)
	{
		if(cd->GetDeclType() == CSS_DECL_TYPE)
		{
			switch(cd->TypeValue())
			{
				case CSS_VALUE_round:
					rendinfo.linejoin = SVGJOIN_ROUND;
					break;
				case CSS_VALUE_miter:
					rendinfo.linejoin = SVGJOIN_MITER;
					break;
				case CSS_VALUE_bevel:
					rendinfo.linejoin = SVGJOIN_BEVEL;
					break;
				case CSS_VALUE_inherit:
					rendinfo.linejoin = parent_svg_props->rendinfo.linejoin;
					break;
			}
		}
	}
	else if(cd->GetProperty() == CSS_PROPERTY_text_anchor && !override.has_textanchor)
	{
		if(cd->GetDeclType() == CSS_DECL_TYPE)
		{
			if(cd->TypeValue() == CSS_VALUE_start)
			{
				textinfo.textanchor = SVGTEXTANCHOR_START;
			}
			else if(cd->TypeValue() == CSS_VALUE_middle)
			{
				textinfo.textanchor = SVGTEXTANCHOR_MIDDLE;
			}
			else if(cd->TypeValue() == CSS_VALUE_end)
			{
				textinfo.textanchor = SVGTEXTANCHOR_END;
			}
			else if(cd->TypeValue() == CSS_VALUE_inherit)
			{
				textinfo.textanchor = parent_svg_props->textinfo.textanchor;
			}
		}
	}
	else if(cd->GetProperty() == CSS_PROPERTY_stop_color && !override.has_stopcolor)
	{
		OpStatus::Ignore(stopcolor.SetFromCSSDecl(cd, (SVGColor*)&parent_svg_props->stopcolor));
	}
	else if(cd->GetProperty() == CSS_PROPERTY_stop_opacity && !override.has_stopopacity)
	{
		SetOpacity(cd, stopopacity, parent_svg_props->stopopacity);
	}
	else if(cd->GetProperty() == CSS_PROPERTY_flood_color && !override.has_floodcolor)
	{
		OpStatus::Ignore(floodcolor.SetFromCSSDecl(cd, (SVGColor*)&parent_svg_props->floodcolor));
	}
	else if(cd->GetProperty() == CSS_PROPERTY_flood_opacity && !override.has_floodopacity)
	{
		SetOpacity(cd, floodopacity, parent_svg_props->floodopacity);
	}
	else if(cd->GetProperty() == CSS_PROPERTY_clip_rule && !override.has_cliprule)
	{
		SVGFillRule tmp_cliprule;
		if(SetFillRule(cd, tmp_cliprule, (SVGFillRule)parent_svg_props->rendinfo.cliprule))
		{
			rendinfo.cliprule = tmp_cliprule;
		}
	}
	else if(cd->GetProperty() == CSS_PROPERTY_lighting_color && !override.has_lightingcolor)
	{
		OpStatus::Ignore(lightingcolor.SetFromCSSDecl(cd, (SVGColor*)&parent_svg_props->lightingcolor));
	}
	else if(cd->GetProperty() == CSS_PROPERTY_clip_path && !override.has_clippath)
	{
		SetURLReference(LOGDOC_GETURL(hld_profile->GetURL()), cd, clippath, parent_svg_props->clippath);
	}
	else if(cd->GetProperty() == CSS_PROPERTY_mask && !override.has_mask)
	{
		SetURLReference(LOGDOC_GETURL(hld_profile->GetURL()), cd, mask, parent_svg_props->mask);
	}
#ifdef SVG_SUPPORT_FILTERS
	else if(cd->GetProperty() == CSS_PROPERTY_color_interpolation_filters &&
			!override2.has_color_interpolation_filters)
	{
		if(cd->GetDeclType() == CSS_DECL_TYPE)
		{
			switch(cd->TypeValue())
			{
			case CSS_VALUE_linearRGB:
				rendinfo.color_interpolation_filters = SVGCOLORINTERPOLATION_LINEARRGB;
				break;
			case CSS_VALUE_sRGB:
				rendinfo.color_interpolation_filters = SVGCOLORINTERPOLATION_SRGB;
				break;
			case CSS_VALUE_auto:
				rendinfo.color_interpolation_filters = SVGCOLORINTERPOLATION_AUTO;
				break;
			case CSS_VALUE_inherit:
				rendinfo.color_interpolation_filters = parent_svg_props->rendinfo.color_interpolation_filters;
				break;
			}
		}
	}
	else if(cd->GetProperty() == CSS_PROPERTY_enable_background && !override.has_enablebackground)
	{
		if(cd->GetDeclType() == CSS_DECL_TYPE)
		{
			switch(cd->TypeValue())
			{
			case CSS_VALUE_new:
				enablebackground = SVGEnableBackground(SVGEnableBackground::SVGENABLEBG_NEW);
				break;
			case CSS_VALUE_inherit:
				enablebackground = parent_svg_props->enablebackground;
				break;
			case CSS_VALUE_accumulate:
				enablebackground = SVGEnableBackground(SVGEnableBackground::SVGENABLEBG_ACCUMULATE);
				break;
			}
		}
		else if(cd->GetDeclType() == CSS_DECL_GEN_ARRAY)
		{
			short num = cd->ArrayValueLen();
			const CSS_generic_value* gen_arr = cd->GenArrayValue();

			if(num == 4)
			{
				enablebackground = SVGEnableBackground(SVGEnableBackground::SVGENABLEBG_NEW,
													   gen_arr[0].value.real, gen_arr[1].value.real,
													   gen_arr[2].value.real, gen_arr[3].value.real);
			}
		}
	}
	else if(cd->GetProperty() == CSS_PROPERTY_filter && !override.has_filter)
	{
		SetURLReference(LOGDOC_GETURL(hld_profile->GetURL()), cd, filter, parent_svg_props->filter);
	}
#endif // SVG_SUPPORT_FILTERS
	else if(cd->GetProperty() == CSS_PROPERTY_pointer_events && !override.has_pointerevents)
	{
		if(cd->GetDeclType() == CSS_DECL_TYPE)
		{
			switch(cd->TypeValue())
			{
				case CSS_VALUE_inherit:
					rendinfo.pointerevents = parent_svg_props->rendinfo.pointerevents;
					break;
				case CSS_VALUE_visiblePainted:
					rendinfo.pointerevents = SVGPOINTEREVENTS_VISIBLEPAINTED;
					break;
				case CSS_VALUE_visibleFill:
					rendinfo.pointerevents = SVGPOINTEREVENTS_VISIBLEFILL;
					break;
				case CSS_VALUE_visibleStroke:
					rendinfo.pointerevents = SVGPOINTEREVENTS_VISIBLESTROKE;
					break;
				case CSS_VALUE_visible:
					rendinfo.pointerevents = SVGPOINTEREVENTS_VISIBLE;
					break;
				case CSS_VALUE_painted:
					rendinfo.pointerevents = SVGPOINTEREVENTS_PAINTED;
					break;
				case CSS_VALUE_fill:
					rendinfo.pointerevents = SVGPOINTEREVENTS_FILL;
					break;
				case CSS_VALUE_stroke:
					rendinfo.pointerevents = SVGPOINTEREVENTS_STROKE;
					break;
				case CSS_VALUE_all:
					rendinfo.pointerevents = SVGPOINTEREVENTS_ALL;
					break;
				case CSS_VALUE_none:
					rendinfo.pointerevents = SVGPOINTEREVENTS_NONE;
					break;
			}
		}
	}
	else if(cd->GetProperty() == CSS_PROPERTY_alignment_baseline && !override.has_alignmentbaseline)
	{
		if(cd->GetDeclType() == CSS_DECL_TYPE)
		{
			switch(cd->TypeValue())
			{
			case CSS_VALUE_inherit:
				textinfo.alignmentbaseline = parent_svg_props->textinfo.alignmentbaseline;
				break;
			case CSS_VALUE_auto:
				textinfo.alignmentbaseline = SVGALIGNMENTBASELINE_AUTO;
				break;
			case CSS_VALUE_baseline:
				textinfo.alignmentbaseline = SVGALIGNMENTBASELINE_BASELINE;
				break;
			case CSS_VALUE_before_edge:
				textinfo.alignmentbaseline = SVGALIGNMENTBASELINE_BEFORE_EDGE;
				break;
			case CSS_VALUE_text_before_edge:
				textinfo.alignmentbaseline = SVGALIGNMENTBASELINE_TEXT_BEFORE_EDGE;
				break;
			case CSS_VALUE_middle:
				textinfo.alignmentbaseline = SVGALIGNMENTBASELINE_MIDDLE;
				break;
			case CSS_VALUE_central:
				textinfo.alignmentbaseline = SVGALIGNMENTBASELINE_CENTRAL;
				break;
			case CSS_VALUE_after_edge:
				textinfo.alignmentbaseline = SVGALIGNMENTBASELINE_AFTER_EDGE;
				break;
			case CSS_VALUE_text_after_edge:
				textinfo.alignmentbaseline = SVGALIGNMENTBASELINE_TEXT_AFTER_EDGE;
				break;
			case CSS_VALUE_ideographic:
				textinfo.alignmentbaseline = SVGALIGNMENTBASELINE_IDEOGRAPHIC;
				break;
			case CSS_VALUE_alphabetic:
				textinfo.alignmentbaseline = SVGALIGNMENTBASELINE_ALPHABETIC;
				break;
			case CSS_VALUE_hanging:
				textinfo.alignmentbaseline = SVGALIGNMENTBASELINE_HANGING;
				break;
			case CSS_VALUE_mathematical:
				textinfo.alignmentbaseline = SVGALIGNMENTBASELINE_MATHEMATICAL;
				break;
			}
		}
	}
	else if(cd->GetProperty() == CSS_PROPERTY_baseline_shift && !override.has_baselineshift)
	{
		if (cd->GetDeclType() == CSS_DECL_TYPE)
		{
			switch(cd->TypeValue())
			{
				case CSS_VALUE_inherit:
					baselineshift = parent_svg_props->baselineshift;
					break;
				case CSS_VALUE_baseline:
					baselineshift = SVGBaselineShift(SVGBaselineShift::SVGBASELINESHIFT_BASELINE);
					break;
				case CSS_VALUE_sub:
					baselineshift = SVGBaselineShift(SVGBaselineShift::SVGBASELINESHIFT_SUB);
					break;
				case CSS_VALUE_super:
					baselineshift = SVGBaselineShift(SVGBaselineShift::SVGBASELINESHIFT_SUPER);
					break;
			}
		}
		else if(cd->GetDeclType() == CSS_DECL_NUMBER)
		{
			SVGLength len_value(cd->GetNumberValue(0), cd->GetValueType(0));
			baselineshift = SVGBaselineShift(SVGBaselineShift::SVGBASELINESHIFT_VALUE,
											 len_value);
		}
	}
	else if(cd->GetProperty() == CSS_PROPERTY_dominant_baseline && !override.has_dominantbaseline)
	{
		if(cd->GetDeclType() == CSS_DECL_TYPE)
		{
			switch(cd->TypeValue())
			{
			case CSS_VALUE_inherit:
				textinfo.dominantbaseline = parent_svg_props->textinfo.dominantbaseline;
				break;
			case CSS_VALUE_auto:
				textinfo.dominantbaseline = SVGDOMINANTBASELINE_AUTO;
				break;
			case CSS_VALUE_use_script:
				textinfo.dominantbaseline = SVGDOMINANTBASELINE_USE_SCRIPT;
				break;
			case CSS_VALUE_no_change:
				textinfo.dominantbaseline = SVGDOMINANTBASELINE_NO_CHANGE;
				break;
			case CSS_VALUE_reset_size:
				textinfo.dominantbaseline = SVGDOMINANTBASELINE_RESET_SIZE;
				break;
			case CSS_VALUE_ideographic:
				textinfo.dominantbaseline = SVGDOMINANTBASELINE_IDEOGRAPHIC;
				break;
			case CSS_VALUE_alphabetic:
				textinfo.dominantbaseline = SVGDOMINANTBASELINE_ALPHABETIC;
				break;
			case CSS_VALUE_hanging:
				textinfo.dominantbaseline = SVGDOMINANTBASELINE_HANGING;
				break;
			case CSS_VALUE_mathematical:
				textinfo.dominantbaseline = SVGDOMINANTBASELINE_MATHEMATICAL;
				break;
			case CSS_VALUE_central:
				textinfo.dominantbaseline = SVGDOMINANTBASELINE_CENTRAL;
				break;
			case CSS_VALUE_middle:
				textinfo.dominantbaseline = SVGDOMINANTBASELINE_MIDDLE;
				break;
			case CSS_VALUE_text_after_edge:
				textinfo.dominantbaseline = SVGDOMINANTBASELINE_TEXT_AFTER_EDGE;
				break;
			case CSS_VALUE_text_before_edge:
				textinfo.dominantbaseline = SVGDOMINANTBASELINE_TEXT_BEFORE_EDGE;
				break;
			}
		}
	}
	else if(cd->GetProperty() == CSS_PROPERTY_writing_mode && !override.has_writingmode)
	{
		if(cd->GetDeclType() == CSS_DECL_TYPE)
		{
			switch(cd->TypeValue())
			{
				case CSS_VALUE_inherit:
					textinfo.writingmode = parent_svg_props->textinfo.writingmode;
					break;
				case CSS_VALUE_lr:
					textinfo.writingmode = SVGWRITINGMODE_LR;
					break;
				case CSS_VALUE_lr_tb:
					textinfo.writingmode = SVGWRITINGMODE_LR_TB;
					break;
				case CSS_VALUE_rl:
					textinfo.writingmode = SVGWRITINGMODE_RL;
					break;
				case CSS_VALUE_rl_tb:
					textinfo.writingmode = SVGWRITINGMODE_RL_TB;
					break;
				case CSS_VALUE_tb:
					textinfo.writingmode = SVGWRITINGMODE_TB;
					break;
				case CSS_VALUE_tb_rl:
					textinfo.writingmode = SVGWRITINGMODE_TB_RL;
					break;
				default:
					textinfo.writingmode = SVGWRITINGMODE_LR_TB;
					break;
			}
		}
	}
	else if(cd->GetProperty() == CSS_PROPERTY_shape_rendering && !override2.has_shaperendering)
	{
		if(cd->GetDeclType() == CSS_DECL_TYPE)
		{
			switch(cd->TypeValue())
			{
				case CSS_VALUE_inherit:
					rendinfo.shaperendering = parent_svg_props->rendinfo.shaperendering;
					break;
				case CSS_VALUE_auto:
					rendinfo.shaperendering = SVGSHAPERENDERING_AUTO;
					break;
				case CSS_VALUE_optimizeSpeed:
					rendinfo.shaperendering = SVGSHAPERENDERING_OPTIMIZESPEED;
					break;
				case CSS_VALUE_crispEdges:
					rendinfo.shaperendering = SVGSHAPERENDERING_CRISPEDGES;
					break;
				case CSS_VALUE_geometricPrecision:
					rendinfo.shaperendering = SVGSHAPERENDERING_GEOMETRICPRECISION;
					break;
			}
		}
	}
	else if(cd->GetProperty() == CSS_PROPERTY_marker && !override.has_marker)
	{
		SetURLReference(LOGDOC_GETURL(hld_profile->GetURL()), cd, marker, parent_svg_props->marker);
	}
	else if(cd->GetProperty() == CSS_PROPERTY_marker_start && !override.has_markerstart)
	{
		SetURLReference(LOGDOC_GETURL(hld_profile->GetURL()), cd, markerstart, parent_svg_props->markerstart);
	}
	else if(cd->GetProperty() == CSS_PROPERTY_marker_mid && !override.has_markermid)
	{
		SetURLReference(LOGDOC_GETURL(hld_profile->GetURL()), cd, markermid, parent_svg_props->markermid);
	}
	else if(cd->GetProperty() == CSS_PROPERTY_marker_end && !override.has_markerend)
	{
		SetURLReference(LOGDOC_GETURL(hld_profile->GetURL()), cd, markerend, parent_svg_props->markerend);
	}
	else if(cd->GetProperty() == CSS_PROPERTY_glyph_orientation_vertical)
	{
		if(cd->GetDeclType() == CSS_DECL_TYPE)
		{
			if(cd->TypeValue() == CSS_VALUE_inherit)
			{
				textinfo.glyph_orientation_vertical = parent_svg_props->textinfo.glyph_orientation_vertical;
				textinfo.glyph_orientation_vertical_is_auto = parent_svg_props->textinfo.glyph_orientation_vertical_is_auto;
			}
			else if(cd->TypeValue() == CSS_VALUE_auto)
			{
				textinfo.glyph_orientation_vertical = 0;
				textinfo.glyph_orientation_vertical_is_auto = 1;
			}
		}
		else if(cd->GetDeclType() == CSS_DECL_NUMBER)
		{
			textinfo.glyph_orientation_vertical = GetQuantizedAngleFromCSSDecl(cd);
			textinfo.glyph_orientation_vertical_is_auto = 0;
		}
	}
	else if(cd->GetProperty() == CSS_PROPERTY_glyph_orientation_horizontal)
	{
		if(cd->GetDeclType() == CSS_DECL_TYPE)
		{
			if(cd->TypeValue() == CSS_VALUE_inherit)
			{
				textinfo.glyph_orientation_horizontal = parent_svg_props->textinfo.glyph_orientation_horizontal;
			}
		}
		else if(cd->GetDeclType() == CSS_DECL_NUMBER)
		{
			textinfo.glyph_orientation_horizontal = GetQuantizedAngleFromCSSDecl(cd);
		}
	}
	else if(cd->GetProperty() == CSS_PROPERTY_text_rendering && !override2.has_textrendering)
	{
		if(cd->GetDeclType() == CSS_DECL_TYPE)
		{
			switch(cd->TypeValue())
			{
				case CSS_VALUE_inherit:
					textinfo.textrendering = parent_svg_props->textinfo.textrendering;
					break;
				case CSS_VALUE_auto:
					textinfo.textrendering = SVGTEXTRENDERING_AUTO;
					break;
				case CSS_VALUE_optimizeSpeed:
					textinfo.textrendering = SVGTEXTRENDERING_OPTIMIZESPEED;
					break;
				case CSS_VALUE_optimizeLegibility:
					textinfo.textrendering = SVGTEXTRENDERING_OPTIMIZELEGIBILITY;
					break;
				case CSS_VALUE_geometricPrecision:
					textinfo.textrendering = SVGTEXTRENDERING_GEOMETRICPRECISION;
					break;
			}
		}
	}
	else if(cd->GetProperty() == CSS_PROPERTY_vector_effect && !override2.has_vector_effect)
	{
		if(cd->GetDeclType() == CSS_DECL_TYPE)
		{
			switch(cd->TypeValue())
			{
				case CSS_VALUE_inherit:
					rendinfo.vector_effect = parent_svg_props->rendinfo.vector_effect;
					break;
				case CSS_VALUE_none:
					rendinfo.vector_effect = SVGVECTOREFFECT_NONE;
					break;
				case CSS_VALUE_non_scaling_stroke:
					rendinfo.vector_effect = SVGVECTOREFFECT_NON_SCALING_STROKE;
					break;
			}
		}
	}
	else if(cd->GetProperty() == CSS_PROPERTY_audio_level && !override2.has_audio_level)
	{
		SetNumber(cd, audiolevel, parent_svg_props->audiolevel);
		computed_audiolevel *= audiolevel;
	}
	else if(cd->GetProperty() == CSS_PROPERTY_display_align && !override2.has_displayalign)
	{
		if(cd->GetDeclType() == CSS_DECL_TYPE)
		{
			switch(cd->TypeValue())
			{
				case CSS_VALUE_inherit:
					textinfo.displayalign = parent_svg_props->textinfo.displayalign;
					break;
				case CSS_VALUE_auto:
					textinfo.displayalign = SVGDISPLAYALIGN_AUTO;
					break;
				case CSS_VALUE_before:
					textinfo.displayalign = SVGDISPLAYALIGN_BEFORE;
					break;
				case CSS_VALUE_center:
					textinfo.displayalign = SVGDISPLAYALIGN_CENTER;
					break;
				case CSS_VALUE_after:
					textinfo.displayalign = SVGDISPLAYALIGN_AFTER;
					break;
			}
		}
	}
	else if(cd->GetProperty() == CSS_PROPERTY_solid_color && !override2.has_solidcolor)
	{
		OpStatus::Ignore(solidcolor.SetFromCSSDecl(cd, (SVGColor*)&parent_svg_props->solidcolor));
	}
	else if(cd->GetProperty() == CSS_PROPERTY_solid_opacity && !override2.has_solidopacity)
	{
		SetOpacity(cd, solidopacity, parent_svg_props->solidopacity);
	}
	else if(cd->GetProperty() == CSS_PROPERTY_viewport_fill && !override2.has_viewportfill)
	{
		OpStatus::Ignore(viewportfill.SetFromCSSDecl(cd, (SVGColor*)&parent_svg_props->viewportfill));
	}
	else if(cd->GetProperty() == CSS_PROPERTY_viewport_fill_opacity && !override2.has_viewportfillopacity)
	{
		SetOpacity(cd, viewportfillopacity, parent_svg_props->viewportfillopacity);
	}
	else if(cd->GetProperty() == CSS_PROPERTY_line_increment && !override2.has_lineincrement)
	{
		if(cd->GetDeclType() == CSS_DECL_TYPE)
		{
			switch(cd->TypeValue())
			{
				case CSS_VALUE_inherit:
					textinfo.lineincrement_is_auto = parent_svg_props->textinfo.lineincrement_is_auto;
					lineincrement = parent_svg_props->lineincrement;
					break;
				case CSS_VALUE_auto:
					textinfo.lineincrement_is_auto = 1;
					lineincrement = 0;
					break;
			}
		}
		else if(cd->GetDeclType() == CSS_DECL_NUMBER)
		{
			textinfo.lineincrement_is_auto = 0;
			lineincrement = cd->GetNumberValue(0);
		}
	}

	if(cd->GetProperty() == CSS_PROPERTY_buffered_rendering && !override2.has_bufferedrendering)
	{
		if(cd->GetDeclType() == CSS_DECL_TYPE)
		{
			switch(cd->TypeValue())
			{
				case CSS_VALUE_inherit:
					rendinfo.bufferedrendering = parent_svg_props->rendinfo.bufferedrendering;
					break;
				case CSS_VALUE_auto:
					rendinfo.bufferedrendering = SVGBUFFEREDRENDERING_AUTO;
					break;
				case CSS_VALUE_static:
					rendinfo.bufferedrendering = SVGBUFFEREDRENDERING_STATIC;
					break;
				case CSS_VALUE_dynamic:
					rendinfo.bufferedrendering = SVGBUFFEREDRENDERING_DYNAMIC;
					break;
			}
		}
	}
cleanup_props:

	delete parent_svg_props_dealloc;

	return status;
}

OP_STATUS SvgProperties::GetCSSDecl(CSS_decl** out_cd, HTML_Element* element, short property, short pseudo,
					 HLDocProfile* hld_profile, BOOL is_current_style) const
{
	switch(property)
	{
		case CSS_PROPERTY_fill:
			return GetFill()->GetCSSDecl(out_cd, element, property, is_current_style);

		case CSS_PROPERTY_stroke:
			return GetStroke()->GetCSSDecl(out_cd, element, property, is_current_style);

		case CSS_PROPERTY_stop_color:
			return stopcolor.GetCSSDecl(out_cd, element, property, is_current_style);

		case CSS_PROPERTY_flood_color:
			return floodcolor.GetCSSDecl(out_cd, element, property, is_current_style);

		case CSS_PROPERTY_lighting_color:
			return lightingcolor.GetCSSDecl(out_cd, element, property, is_current_style);

		case CSS_PROPERTY_stop_opacity:
			*out_cd = OP_NEW(CSS_number_decl, (property, stopopacity / 255.f, CSS_NUMBER));
			break;

		case CSS_PROPERTY_stroke_opacity:
			*out_cd = OP_NEW(CSS_number_decl, (property, strokeopacity / 255.f, CSS_NUMBER));
			break;

		case CSS_PROPERTY_fill_opacity:
			*out_cd = OP_NEW(CSS_number_decl, (property, fillopacity / 255.f, CSS_NUMBER));
			break;

		case CSS_PROPERTY_flood_opacity:
			*out_cd = OP_NEW(CSS_number_decl, (property, floodopacity / 255.f, CSS_NUMBER));
			break;

		case CSS_PROPERTY_buffered_rendering:
			{
				CSSValue val;
				switch (rendinfo.bufferedrendering)
				{
				default:
					OP_ASSERT(!"Outside range of 'enum SVGBufferedRendering'.");
				case SVGBUFFEREDRENDERING_AUTO:
					val = CSS_VALUE_auto;
					break;
				case SVGBUFFEREDRENDERING_STATIC:
					val = CSS_VALUE_static;
					break;
				case SVGBUFFEREDRENDERING_DYNAMIC:
					val = CSS_VALUE_dynamic;
					break;
				}
				*out_cd = OP_NEW(CSS_type_decl, (property, val));
			}
			break;

		case CSS_PROPERTY_alignment_baseline:
			{
				CSSValue val;
				switch(textinfo.alignmentbaseline)
				{
				default:
					OP_ASSERT(!"The cascade contains a non-computed value, which is an error.");
				case SVGALIGNMENTBASELINE_AUTO:
					val = CSS_VALUE_auto;
					break;
				case SVGALIGNMENTBASELINE_BASELINE:
					val = CSS_VALUE_baseline;
					break;
				case SVGALIGNMENTBASELINE_BEFORE_EDGE:
					val = CSS_VALUE_before_edge;
					break;
				case SVGALIGNMENTBASELINE_TEXT_BEFORE_EDGE:
					val = CSS_VALUE_text_before_edge;
					break;
				case SVGALIGNMENTBASELINE_MIDDLE:
					val = CSS_VALUE_middle;
					break;
				case SVGALIGNMENTBASELINE_CENTRAL:
					val = CSS_VALUE_central;
					break;
				case SVGALIGNMENTBASELINE_AFTER_EDGE:
					val = CSS_VALUE_after_edge;
					break;
				case SVGALIGNMENTBASELINE_TEXT_AFTER_EDGE:
					val = CSS_VALUE_text_after_edge;
					break;
				case SVGALIGNMENTBASELINE_IDEOGRAPHIC:
					val = CSS_VALUE_ideographic;
					break;
				case SVGALIGNMENTBASELINE_ALPHABETIC:
					val = CSS_VALUE_alphabetic;
					break;
				case SVGALIGNMENTBASELINE_HANGING:
					val = CSS_VALUE_hanging;
					break;
				case SVGALIGNMENTBASELINE_MATHEMATICAL:
					val = CSS_VALUE_mathematical;
					break;
				}
				*out_cd = OP_NEW(CSS_type_decl, (property, val));
			}
			break;

		case CSS_PROPERTY_clip_rule:
			*out_cd = GetFillRule(property, (SVGFillRule)rendinfo.cliprule);
			break;

		case CSS_PROPERTY_fill_rule:
			*out_cd = GetFillRule(property, (SVGFillRule)rendinfo.fillrule);
			break;

		case CSS_PROPERTY_color_interpolation:
			// FIXME: not yet supported
			break;

		case CSS_PROPERTY_color_interpolation_filters:
			{
				CSSValue val;
				switch(rendinfo.color_interpolation_filters)
				{
				default:
					OP_ASSERT(!"The cascade contains a non-computed value, which is an error.");
				case SVGCOLORINTERPOLATION_LINEARRGB:
					val = CSS_VALUE_linearRGB;
					break;
				case SVGCOLORINTERPOLATION_SRGB:
					val = CSS_VALUE_sRGB;
					break;
				case SVGCOLORINTERPOLATION_AUTO:
					val = CSS_VALUE_auto;
					break;
				}
				*out_cd = OP_NEW(CSS_type_decl, (property, val));
			}
			break;

		case CSS_PROPERTY_color_rendering:
			// FIXME: not yet supported
			break;

		case CSS_PROPERTY_shape_rendering:
			{
				CSSValue val;
				switch(rendinfo.shaperendering)
				{
				default:
					OP_ASSERT(!"The cascade contains a non-computed value, which is an error.");
				case SVGSHAPERENDERING_AUTO:
					val = CSS_VALUE_auto;
					break;
				case SVGSHAPERENDERING_OPTIMIZESPEED:
					val = CSS_VALUE_optimizeSpeed;
					break;
				case SVGSHAPERENDERING_CRISPEDGES:
					val = CSS_VALUE_crispEdges;
					break;
				case SVGSHAPERENDERING_GEOMETRICPRECISION:
					val = CSS_VALUE_geometricPrecision;
					break;
				}
				*out_cd = OP_NEW(CSS_type_decl, (property, val));
			}
			break;

		case CSS_PROPERTY_dominant_baseline:
			{
				CSSValue val;
				switch(textinfo.dominantbaseline)
				{
				default:
					OP_ASSERT(!"The cascade contains a non-computed value, which is an error.");
				case SVGDOMINANTBASELINE_AUTO:
					val = CSS_VALUE_auto;
					break;
				case SVGDOMINANTBASELINE_USE_SCRIPT:
					val = CSS_VALUE_use_script;
					break;
				case SVGDOMINANTBASELINE_NO_CHANGE:
					val = CSS_VALUE_no_change;
					break;
				case SVGDOMINANTBASELINE_RESET_SIZE:
					val = CSS_VALUE_reset_size;
					break;
				case SVGDOMINANTBASELINE_IDEOGRAPHIC:
					val = CSS_VALUE_ideographic;
					break;
				case SVGDOMINANTBASELINE_ALPHABETIC:
					val = CSS_VALUE_alphabetic;
					break;
				case SVGDOMINANTBASELINE_HANGING:
					val = CSS_VALUE_hanging;
					break;
				case SVGDOMINANTBASELINE_MATHEMATICAL:
					val = CSS_VALUE_mathematical;
					break;
				case SVGDOMINANTBASELINE_CENTRAL:
					val = CSS_VALUE_central;
					break;
				case SVGDOMINANTBASELINE_MIDDLE:
					val = CSS_VALUE_middle;
					break;
				case SVGDOMINANTBASELINE_TEXT_AFTER_EDGE:
					val = CSS_VALUE_text_after_edge;
					break;
				case SVGDOMINANTBASELINE_TEXT_BEFORE_EDGE:
					val = CSS_VALUE_text_before_edge;
					break;
				}
				*out_cd = OP_NEW(CSS_type_decl, (property, val));
			}
			break;

		case CSS_PROPERTY_pointer_events:
			{
				CSSValue val;
				switch(rendinfo.pointerevents)
				{
				default:
					OP_ASSERT(!"The cascade contains a non-computed value, which is an error.");
				case SVGPOINTEREVENTS_VISIBLEPAINTED:
					val = CSS_VALUE_visiblePainted;
					break;
				case SVGPOINTEREVENTS_VISIBLEFILL:
					val = CSS_VALUE_visibleFill;
					break;
				case SVGPOINTEREVENTS_VISIBLESTROKE:
					val = CSS_VALUE_visibleStroke;
					break;
				case SVGPOINTEREVENTS_VISIBLE:
					val = CSS_VALUE_visible;
					break;
				case SVGPOINTEREVENTS_PAINTED:
					val = CSS_VALUE_painted;
					break;
				case SVGPOINTEREVENTS_FILL:
					val = CSS_VALUE_fill;
					break;
				case SVGPOINTEREVENTS_STROKE:
					val = CSS_VALUE_stroke;
					break;
				case SVGPOINTEREVENTS_ALL:
					val = CSS_VALUE_all;
					break;
				case SVGPOINTEREVENTS_NONE:
					val = CSS_VALUE_none;
					break;
				}
				*out_cd = OP_NEW(CSS_type_decl, (property, val));
			}
			break;

		case CSS_PROPERTY_stroke_linejoin:
			{
				CSSValue val;
				switch(rendinfo.linejoin)
				{
				case SVGJOIN_ROUND:
					val = CSS_VALUE_round;
					break;
				default:
					OP_ASSERT(!"The cascade contains a non-computed value, which is an error.");
				case SVGJOIN_MITER:
					val = CSS_VALUE_miter;
					break;
				case SVGJOIN_BEVEL:
					val = CSS_VALUE_bevel;
					break;
				}
				*out_cd = OP_NEW(CSS_type_decl, (property, val));
			}
			break;

		case CSS_PROPERTY_stroke_linecap:
			{
				CSSValue val;
				switch(rendinfo.linecap)
				{
				case SVGCAP_ROUND:
					val = CSS_VALUE_round;
					break;
				default:
					OP_ASSERT(!"The cascade contains a non-computed value, which is an error.");
				case SVGCAP_BUTT:
					val = CSS_VALUE_butt;
					break;
				case SVGCAP_SQUARE:
					val = CSS_VALUE_square;
					break;
				}
				*out_cd = OP_NEW(CSS_type_decl, (property, val));
			}
			break;

		case CSS_PROPERTY_text_anchor:
			{
				CSSValue val;
				switch(textinfo.textanchor)
				{
				default:
					OP_ASSERT(!"The cascade contains a non-computed value, which is an error.");
				case SVGTEXTANCHOR_START:
					val = CSS_VALUE_start;
					break;
				case SVGTEXTANCHOR_MIDDLE:
					val = CSS_VALUE_middle;
					break;
				case SVGTEXTANCHOR_END:
					val = CSS_VALUE_end;
					break;
				}
				*out_cd = OP_NEW(CSS_type_decl, (property, val));
			}
			break;

		case CSS_PROPERTY_text_rendering:
			{
				CSSValue val;
				switch(textinfo.textrendering)
				{
				default:
					OP_ASSERT(!"The cascade contains a non-computed value, which is an error.");
				case SVGTEXTRENDERING_AUTO:
					val = CSS_VALUE_auto;
					break;
				case SVGTEXTRENDERING_OPTIMIZESPEED:
					val = CSS_VALUE_optimizeSpeed;
					break;
				case SVGTEXTRENDERING_OPTIMIZELEGIBILITY:
					val = CSS_VALUE_optimizeLegibility;
					break;
				case SVGTEXTRENDERING_GEOMETRICPRECISION:
					val = CSS_VALUE_geometricPrecision;
					break;
				}
				*out_cd = OP_NEW(CSS_type_decl, (property, val));
			}
			break;

		case CSS_PROPERTY_vector_effect:
			{
				CSSValue val;
				switch(rendinfo.vector_effect)
				{
				default:
					OP_ASSERT(!"The cascade contains a non-computed value, which is an error.");
				case SVGVECTOREFFECT_NONE:
					val = CSS_VALUE_none;
					break;
				case SVGVECTOREFFECT_NON_SCALING_STROKE:
					val = CSS_VALUE_non_scaling_stroke;
					break;
				}
				*out_cd = OP_NEW(CSS_type_decl, (property, val));
			}
			break;
		case CSS_PROPERTY_writing_mode:
			{
				CSSValue val;
				switch(textinfo.writingmode)
				{
				case SVGWRITINGMODE_LR:
					val = CSS_VALUE_lr;
					break;
				default:
					OP_ASSERT(!"The cascade contains a non-computed value, which is an error.");
				case SVGWRITINGMODE_LR_TB:
					val = CSS_VALUE_lr_tb;
					break;
				case SVGWRITINGMODE_RL:
					val = CSS_VALUE_rl;
					break;
				case SVGWRITINGMODE_RL_TB:
					val = CSS_VALUE_rl_tb;
					break;
				case SVGWRITINGMODE_TB:
					val = CSS_VALUE_tb;
					break;
				case SVGWRITINGMODE_TB_RL:
					val = CSS_VALUE_tb_rl;
					break;
				}
				*out_cd = OP_NEW(CSS_type_decl, (property, val));
			}
			break;

		case CSS_PROPERTY_color_profile:
			// FIXME: not yet supported
			break;

		case CSS_PROPERTY_glyph_orientation_horizontal:
			*out_cd = OP_NEW(CSS_number_decl, (property, textinfo.glyph_orientation_horizontal * 90.0f, CSS_DEG));
			break;
		case CSS_PROPERTY_glyph_orientation_vertical:
			if (textinfo.glyph_orientation_vertical_is_auto)
			{
				*out_cd = OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto));
			}
			else
			{
				*out_cd = OP_NEW(CSS_number_decl, (property, textinfo.glyph_orientation_vertical * 90.0f, CSS_DEG));
			}
			break;

		case CSS_PROPERTY_baseline_shift:
			switch(baselineshift.GetShiftType())
			{
				case SVGBaselineShift::SVGBASELINESHIFT_BASELINE:
					*out_cd = OP_NEW(CSS_type_decl, (property, CSS_VALUE_baseline));
					break;
				case SVGBaselineShift::SVGBASELINESHIFT_SUB:
					*out_cd = OP_NEW(CSS_type_decl, (property, CSS_VALUE_sub));
					break;
				case SVGBaselineShift::SVGBASELINESHIFT_SUPER:
					*out_cd = OP_NEW(CSS_type_decl, (property, CSS_VALUE_super));
					break;
				default:
					OP_ASSERT(!"The cascade contains a non-computed value, which is an error.");
			}
			break;

		case CSS_PROPERTY_clip_path:
			*out_cd = GetURLReference(property, clippath);
			break;

		case CSS_PROPERTY_filter:
			*out_cd = GetURLReference(property, filter);
			break;

		case CSS_PROPERTY_marker:
			*out_cd = GetURLReference(property, marker);
			break;

		case CSS_PROPERTY_marker_end:
			*out_cd = GetURLReference(property, markerend);
			break;

		case CSS_PROPERTY_marker_mid:
			*out_cd = GetURLReference(property, markermid);
			break;

		case CSS_PROPERTY_marker_start:
			*out_cd = GetURLReference(property, markerstart);
			break;

		case CSS_PROPERTY_mask:
			*out_cd = GetURLReference(property, mask);
			break;

#ifdef SVG_SUPPORT_FILTERS
		case CSS_PROPERTY_enable_background:
			switch(enablebackground.GetType())
			{
				case SVGEnableBackground::SVGENABLEBG_NEW:
					*out_cd = OP_NEW(CSS_type_decl, (property, CSS_VALUE_new));
					// FIXME: what about the x,y,width,height?
					break;
				case SVGEnableBackground::SVGENABLEBG_ACCUMULATE:
					*out_cd = OP_NEW(CSS_type_decl, (property, CSS_VALUE_accumulate));
					break;
				default:
					OP_ASSERT(!"The cascade contains a non-computed value, which is an error.");
			}
			break;
#endif // SVG_SUPPORT_FILTERS

		case CSS_PROPERTY_kerning:
			// FIXME: not yet supported
			break;

		case CSS_PROPERTY_stroke_width:
			*out_cd = OP_NEW(CSS_number_decl, (property, linewidth.GetScalar().GetFloatValue(), linewidth.GetUnit()));
			break;

		case CSS_PROPERTY_stroke_miterlimit:
			*out_cd = OP_NEW(CSS_number_decl, (property, miterlimit.GetFloatValue(), CSS_NUMBER));
			break;

		case CSS_PROPERTY_stroke_dashoffset:
			*out_cd = OP_NEW(CSS_number_decl, (property, dashoffset.GetScalar().GetFloatValue(), dashoffset.GetUnit()));
			break;

		case CSS_PROPERTY_stroke_dasharray:
			{
				SVGDashArray* da = static_cast<SVGDashArray*>(dasharray);
				if(da->is_none)
				{
					*out_cd = OP_NEW(CSS_type_decl, (property, CSS_VALUE_none));
				}
				else
				{
					CSS_generic_value* vals = OP_NEWA(CSS_generic_value, da->dash_array_size);
					*out_cd = CSS_heap_gen_array_decl::Create(property, vals, da->dash_array_size);
					if(!*out_cd)
						return OpStatus::ERR_NO_MEMORY;

					for(unsigned int i = 0; i < da->dash_array_size; i++)
					{
						SVGLength* item = &da->dash_array[i];
						vals[i].SetValueType(item->GetUnit());
						vals[i].SetReal(item->GetScalar().GetFloatValue());
					}

				}
			}
			break;

		case CSS_PROPERTY_audio_level:
			*out_cd = OP_NEW(CSS_number_decl, (property, audiolevel.GetFloatValue(), CSS_NUMBER));
			break;
		case CSS_PROPERTY_solid_color:
			return solidcolor.GetCSSDecl(out_cd, element, property, is_current_style);
		case CSS_PROPERTY_display_align:
			{
				CSSValue val;
				switch(textinfo.textrendering)
				{
				default:
					OP_ASSERT(!"The cascade contains a non-computed value, which is an error.");
				case SVGDISPLAYALIGN_AUTO:
					val = CSS_VALUE_auto;
					break;
				case SVGDISPLAYALIGN_BEFORE:
					val = CSS_VALUE_before;
					break;
				case SVGDISPLAYALIGN_CENTER:
					val = CSS_VALUE_center;
					break;
				case SVGDISPLAYALIGN_AFTER:
					val = CSS_VALUE_after;
					break;
				}
				*out_cd = OP_NEW(CSS_type_decl, (property, val));
			}
			break;
		case CSS_PROPERTY_solid_opacity:
			*out_cd = OP_NEW(CSS_number_decl, (property, solidopacity / 255.f, CSS_NUMBER));
			break;
		case CSS_PROPERTY_viewport_fill:
			return viewportfill.GetCSSDecl(out_cd, element, property, is_current_style);
		case CSS_PROPERTY_line_increment:
			if (textinfo.lineincrement_is_auto)
				*out_cd = OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto));
			else
				*out_cd = OP_NEW(CSS_number_decl, (property, lineincrement.GetFloatValue(), CSS_NUMBER));
			break;
		case CSS_PROPERTY_viewport_fill_opacity:
			*out_cd = OP_NEW(CSS_number_decl, (property, viewportfillopacity / 255.f, CSS_NUMBER));
			break;
	}

	if(!*out_cd)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

/*
 * From a SVGLength and a VisualDevice, with parents resolved
 * fontsizes (short and "float"), compute the resolved font-sizes
 * (short and "float").
 */
void
SvgProperties::ResolveFontSizeLength(HLDocProfile* hld_profile,
									 const SVGFontSize& font_size,
									 SVGNumber parent_fontsize,
									 LayoutFixed parent_decimal_font_size,
									 int parent_font_number,
									 SVGNumber &resolved_fontsize,
									 LayoutFixed &decimal_font_size)
{
	switch(font_size.Mode())
	{
	case SVGFontSize::MODE_RESOLVED:
	{
		decimal_font_size = FloatToLayoutFixed(font_size.ResolvedLength().GetFloatValue());
		resolved_fontsize = font_size.ResolvedLength();
	}
	break;
	case SVGFontSize::MODE_LENGTH:
	{
		const SVGLength& len = font_size.Length();
		int unit = len.GetUnit();
		if (unit == CSS_NUMBER)
			unit = CSS_PX;

		VisualDevice* vd = hld_profile->GetVisualDevice();
		LayoutFixed root_font_size = hld_profile->GetLayoutWorkplace()->GetDocRootProperties().GetRootFontSize();
		CSSLengthResolver length_resolver(vd,
										  TRUE,
										  LayoutFixedToFloat(parent_decimal_font_size),
										  parent_decimal_font_size,
										  parent_font_number,
										  root_font_size,
										  FALSE);
		decimal_font_size = length_resolver.GetLengthInPixels(len.GetScalar().GetFloatValue(), unit);
		decimal_font_size = LayoutFixed(MIN(MAX(LayoutFixedAsInt(decimal_font_size), SHORT_AS_LAYOUT_FIXED_POINT_MIN), SHORT_AS_LAYOUT_FIXED_POINT_MAX));

		/* Using parent_fontsize as viewport_width param and passing later
		   SVGLength::SVGLENGTH_X into SVGUtils::ResolveLength is a shortcut to
		   compute percentage font size. */

		SVGValueContext value_ctx(parent_fontsize,
								  0,
								  parent_fontsize,
								  SVGUtils::GetExHeight(vd, parent_fontsize, parent_font_number),
								  LayoutFixedToSVGNumber(root_font_size));

		resolved_fontsize = SVGUtils::ResolveLength(len, SVGLength::SVGLENGTH_X, value_ctx);
	}
	break;
	case SVGFontSize::MODE_ABSOLUTE:
		//
		// This is what the CSS2 spec says: "On a computer
		// screen a scaling factor of 1.2 is suggested
		// between adjacent indexes; if the 'medium' font
		// is 12pt, the 'large' font could be
		// 14.4pt. Different media may need different
		// scaling factors. Also, the user agent should
		// take the quality and availability of fonts into
		// account when computing the table. The table may
		// be different from one font family to another."
		//
		// We don't obey it really since the table is
		// hardcoded in the SVGAbsoluteFontSize enumeration
		decimal_font_size = IntToLayoutFixed(int(font_size.AbsoluteFontSize()));
		resolved_fontsize = font_size.AbsoluteFontSize();
		break;
	case SVGFontSize::MODE_RELATIVE:
		switch(font_size.RelativeFontSize())
		{
		case SVGRELATIVEFONTSIZE_SMALLER:
			decimal_font_size = LayoutFixed(parent_decimal_font_size * 0.8);
			resolved_fontsize = parent_fontsize*SVGNumber(0.8);
			break;
		case SVGRELATIVEFONTSIZE_LARGER:
			decimal_font_size = LayoutFixed(parent_decimal_font_size * 1.2);
			resolved_fontsize = parent_fontsize*SVGNumber(1.2);
			break;
		}
		break;
	default:
		OP_ASSERT(!"Not reached");
		break;
	}
}

SVGNumber
SvgProperties::GetFontSize(HTML_Element *element) const
{
	// Textcommons can never have attributes so we skip asking
	// AttrValueStore
	if (element->Type() != Markup::HTE_TEXT)
	{
		SVGFontSizeObject *font_size_object = NULL;
		if (OpStatus::IsSuccess(AttrValueStore::GetFontSizeObject(element, font_size_object)) &&
			font_size_object != NULL)
		{
			if(font_size_object->Flag(SVGOBJECTFLAG_IS_CSSPROP))
			{
				const SVGFontSize& font_size = font_size_object->font_size;
				if (font_size.Mode() == SVGFontSize::MODE_RESOLVED)
				{
					return font_size.ResolvedLength();
				}
			}
		}
	}

	return fontsize;
}

SVGNumber
SvgProperties::GetLineIncrement() const
{
	if (textinfo.lineincrement_is_auto)
		return fontsize * 1.1;
	else
		return lineincrement;
}

/* static */ void
SvgProperties::Resolve(HTMLayoutProperties &props)
{
	if (!props.svg)
		return;

	SvgProperties *svg_props = props.svg;

	if(svg_props->GetFill()->GetPaintType() == SVGPaint::CURRENT_COLOR)
	{
		SVGPaint* paint = static_cast<SVGPaint*>(svg_props->fill);
		paint->SetColorRef(props.font_color);
		paint->SetPaintType(SVGPaint::RGBCOLOR);
	}
	if(svg_props->GetStroke()->GetPaintType() == SVGPaint::CURRENT_COLOR)
	{
		SVGPaint* paint = static_cast<SVGPaint*>(svg_props->stroke);
		paint->SetColorRef(props.font_color);
		paint->SetPaintType(SVGPaint::RGBCOLOR);
	}
	if(svg_props->GetFill()->GetPaintType() == SVGPaint::URI_CURRENT_COLOR)
	{
		SVGPaint* paint = static_cast<SVGPaint*>(svg_props->fill);
		paint->SetColorRef(props.font_color);
		paint->SetPaintType(SVGPaint::URI_RGBCOLOR);
	}
	if(svg_props->GetStroke()->GetPaintType() == SVGPaint::URI_CURRENT_COLOR)
	{
		SVGPaint* paint = static_cast<SVGPaint*>(svg_props->stroke);
		paint->SetColorRef(props.font_color);
		paint->SetPaintType(SVGPaint::URI_RGBCOLOR);
	}
	if(svg_props->stopcolor.GetColorType() == SVGColor::SVGCOLOR_CURRENT_COLOR)
	{
		svg_props->stopcolor.SetColorRef(props.font_color);
		svg_props->stopcolor.SetColorType(SVGColor::SVGCOLOR_RGBCOLOR);
	}
	if(svg_props->floodcolor.GetColorType() == SVGColor::SVGCOLOR_CURRENT_COLOR)
	{
		svg_props->floodcolor.SetColorRef(props.font_color);
		svg_props->floodcolor.SetColorType(SVGColor::SVGCOLOR_RGBCOLOR);
	}
	if(svg_props->lightingcolor.GetColorType() == SVGColor::SVGCOLOR_CURRENT_COLOR)
	{
		svg_props->lightingcolor.SetColorRef(props.font_color);
		svg_props->lightingcolor.SetColorType(SVGColor::SVGCOLOR_RGBCOLOR);
	}
	if(svg_props->viewportfill.GetColorType() == SVGColor::SVGCOLOR_CURRENT_COLOR)
	{
		svg_props->viewportfill.SetColorRef(props.font_color);
		svg_props->viewportfill.SetColorType(SVGColor::SVGCOLOR_RGBCOLOR);
	}
	if(svg_props->solidcolor.GetColorType() == SVGColor::SVGCOLOR_CURRENT_COLOR)
	{
		svg_props->solidcolor.SetColorRef(props.font_color);
		svg_props->solidcolor.SetColorType(SVGColor::SVGCOLOR_RGBCOLOR);
	}
	if (props.display_type == CSS_VALUE_none)
	{
		svg_props->computed_audiolevel = 0;
	}
}

#endif // SVG_SUPPORT
