/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_LAYOUT_PROPERTIES_H
#define SVG_LAYOUT_PROPERTIES_H

#ifdef SVG_SUPPORT

class HTML_Element;
class HLDocProfile;
class LayoutProperties;
class SVGFontSize;
class VisualDevice;

#include "modules/svg/svg_number.h"
#include "modules/svg/svg_external_types.h"

// for CSS_* used in SVGLength
#include "modules/style/css_value_types.h"

#include "modules/layout/layout_fixed_point.h"

/**
 * The SVG properties class
 *
 */
class SvgProperties
{
public:
	SVGPropertyReference*			fill;
	UINT32							fill_seq;
	SVGPropertyReference*			stroke;
	UINT32							stroke_seq;
	SVGPropertyReference*			dasharray;
	UINT32							dasharray_seq;
	SVGLength						dashoffset;
	SVGNumber						miterlimit;
	SVGLength						linewidth;
	SVGBaselineShift				baselineshift;
#ifdef SVG_SUPPORT_FILTERS
	SVGEnableBackground				enablebackground;
#endif // SVG_SUPPORT_FILTERS
	SVGColor						floodcolor;
	SVGColor						lightingcolor;
	SVGColor						viewportfill;
	SVGColor						solidcolor;
	SVGColor						stopcolor;
	SVGURLReference					mask;
	SVGURLReference					marker;
	SVGURLReference					markerstart;
	SVGURLReference					markermid;
	SVGURLReference					markerend;
	SVGURLReference					clippath;
	SVGURLReference					filter;
	SVGNumber						fontsize;
	SVGNumber						lineincrement;

	union
	{
		struct
		{
			unsigned int			pointerevents:4; // SVGPointerEvents
			unsigned int			fillrule:4; // SVGFillRule
			unsigned int			linecap:4; // SVGCapType
			unsigned int			linejoin:4; // SVGJoinType
			unsigned int			cliprule:4; // SVGFillRule
			unsigned int			vector_effect:4; // SVGVectorEffect
			unsigned int			color_interpolation_filters:2; // SVGColorInterpolation
			unsigned int			shaperendering:2; // SVGShapeRendering
			unsigned int			bufferedrendering:2; // SVGBufferedRendering
		} rendinfo; // 30 bits
		unsigned int rendinfo_init;
	};
	union
	{
		struct
		{
			unsigned int			textanchor:3; // SVGTextAnchor
			unsigned int			alignmentbaseline:4; // SVGAlignmentBaseline
			unsigned int			dominantbaseline:4; // SVGDominantBaseline
			unsigned int			writingmode:3; // SVGWritingMode
			unsigned int			textrendering:3; // SVGTextRendering
			unsigned int			displayalign:3; // SVGDisplayAlign
			unsigned int			lineincrement_is_auto:1;
			unsigned int			glyph_orientation_horizontal:2; // 0-3 for 0,90,180,270 degrees
			unsigned int			glyph_orientation_vertical:2; // 0-3 for 0,90,180,270 degrees
			unsigned int			glyph_orientation_vertical_is_auto:1;
		} textinfo; // 26 bits
		unsigned int textinfo_init;
	};

	union
	{
		struct
		{
			unsigned int	has_fill:1;
			unsigned int	has_stroke:1;
			unsigned int	has_dashoffset:1;
			unsigned int	has_linewidth:1;
			unsigned int	has_dasharray:1;
			unsigned int	has_textdecoration:1;

			// These are for error-handling
			unsigned int	had_hard_error:1;
			unsigned int	had_oom_error:1;
		} info; // 8 bits
		unsigned int	info_init;
	};

	union
	{
		struct
		{
			unsigned int	has_fill:1;
			unsigned int	has_stroke:1;
			unsigned int	has_stopcolor:1;
			unsigned int	has_stopopacity:1;
			unsigned int	has_pointerevents:1;
			unsigned int	has_fillrule:1;
			unsigned int	has_fillopacity:1;
			unsigned int	has_textanchor:1;
			unsigned int	has_strokeopacity:1;
			unsigned int	has_floodopacity:1;
			unsigned int	has_cliprule:1;
			unsigned int	has_dashoffset:1;
			unsigned int	has_linewidth:1;
			unsigned int	has_miterlimit:1;
			unsigned int	has_floodcolor:1;
			unsigned int	has_lightingcolor:1;
			unsigned int	has_linecap:1;
			unsigned int	has_linejoin:1;
			unsigned int	has_clippath:1;
			unsigned int	has_filter:1;
			unsigned int	has_mask:1;
			unsigned int	has_dasharray:1;
			unsigned int	has_enablebackground:1;
			unsigned int	has_alignmentbaseline:1;
			unsigned int	has_baselineshift:1;
			unsigned int	has_dominantbaseline:1;
			unsigned int	has_writingmode:1;
			unsigned int	has_imagerendering:1;
			unsigned int	has_marker:1;
			unsigned int	has_markerstart:1;
			unsigned int	has_markermid:1;
			unsigned int	has_markerend:1;
		} override; // 32 bits
		unsigned int	override_init;
	};
	union
	{
		struct
		{
			unsigned int	has_textrendering:1;
			unsigned int	has_color_interpolation_filters:1;
			unsigned int	has_vector_effect:1;
			unsigned int	has_viewportfill:1;
			unsigned int	has_viewportfillopacity:1;
			unsigned int	has_solidcolor:1;
			unsigned int	has_solidopacity:1;
			unsigned int	has_audio_level:1;
			unsigned int	has_displayalign:1;
			unsigned int	has_lineincrement:1;
			unsigned int	has_shaperendering:1;
			unsigned int	has_bufferedrendering:1;
		} override2;
		unsigned int	override2_init;
	};

	SVGNumber						audiolevel;
	SVGNumber						computed_audiolevel;

	UINT8							stopopacity;
	UINT8							fillopacity;
	UINT8							strokeopacity;
	UINT8							floodopacity;
	UINT8							viewportfillopacity;
	UINT8							solidopacity;

#if 0 // These are settable via css
	//SVGColorInterpolation			color_interpolation;
	//SVGColorProfile				color_profile;
	//SVGColorRendering				color_rendering;
	//SVGKerning					kerning;
#endif

public:
	SvgProperties();
	SvgProperties(const SvgProperties& src);

	~SvgProperties();

	void	Reset(const SvgProperties *parent_sp);
	void	operator=(const SvgProperties& src) { Reset(&src); }

	// Perhaps make these svg internal
	BOOL	HasFill() const { return info.has_fill; }
	BOOL	HasStroke() const { return info.has_stroke; }
	BOOL	HasDashArray() const { return info.has_dasharray; }
	BOOL	HasLineWidth() const { return info.has_linewidth; }
	BOOL	HasDashOffset() const { return info.has_dashoffset; }

	BOOL	HasOverriddenImageRendering() const { return !!override.has_imagerendering; }

	OP_STATUS	GetError() const;

	const SVGPaint*			GetFill() const { return static_cast<SVGPaint*>(fill); }
	const SVGPaint*			GetStroke() const { return static_cast<SVGPaint*>(stroke); }
	const SVGDashArray* 	GetDashArray() const { return static_cast<SVGDashArray*>(dasharray); }
	SVGTextAnchor			GetTextAnchor() const { return (SVGTextAnchor)textinfo.textanchor; }
	SVGWritingMode			GetWritingMode() const { return (SVGWritingMode)textinfo.writingmode; }
	SVGShapeRendering		GetShapeRendering() const { return (SVGShapeRendering)rendinfo.shaperendering; }
	SVGBufferedRendering	GetBufferedRendering() const { return (SVGBufferedRendering)rendinfo.bufferedrendering; }
#ifdef SVG_SUPPORT_FILTERS
	const SVGEnableBackground* GetEnableBackground() const { return &enablebackground; }
#endif // SVG_SUPPORT_FILTERS

	SVGNumber			GetFontSize(HTML_Element *element) const;
	SVGNumber			GetLineIncrement() const;

	// Utility function
	static void ResolveFontSizeLength(HLDocProfile* hld_profile,
									  const SVGFontSize& font_size,
									  SVGNumber parent_fontsize,
									  LayoutFixed parent_decimal_font_size,
									  int parent_font_number,
									  SVGNumber &resolved_font_size,
									  LayoutFixed &decimal_font_size);
	// End internal

	static void Resolve(class HTMLayoutProperties &props);

	OP_STATUS SetFromCSSDecl(CSS_decl* cd, LayoutProperties* parent_cascade,
							 HTML_Element* element, HLDocProfile* hld_profile);

	OP_STATUS GetCSSDecl(CSS_decl** out_cd, HTML_Element* element, short property,
						 short pseudo, HLDocProfile* hld_profile, BOOL is_current_style = FALSE) const;

	void*			operator new(size_t size) OP_NOTHROW;
	void			operator delete(void* ptr, size_t size);
};

#endif // SVG_SUPPORT
#endif // SVG_LAYOUT_PROPERTIES_H
