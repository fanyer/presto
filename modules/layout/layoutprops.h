/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _LAYOUTPROPS_H_
#define _LAYOUTPROPS_H_

#include "modules/display/color.h"
#include "modules/display/cursor.h"
#include "modules/style/css.h"
#include "modules/layout/layout.h"
#include "modules/display/vis_dev_transform.h"
#include "modules/windowcommander/WritingSystem.h"

#include "modules/prefs/prefsmanager/collections/pc_display.h"

class VisualDevice;
class FramesDocument;
class TableRowGroupBox;

#if defined(SVG_SUPPORT)
# include "modules/svg/SVGLayoutProperties.h"
#endif // SVG_SUPPORT

class CSS_decl;

#define LINE_HEIGHT_SQUEEZE_I		LayoutFixed(INT_MAX)
#define FONT_SIZE_NOT_SET			-1

#define HPOSITION_VALUE_AUTO		LAYOUT_COORD_MIN
#define HPOSITION_VALUE_MIN			(LAYOUT_COORD_MIN + LayoutCoord(1))
#define HPOSITION_VALUE_MAX			LAYOUT_COORD_MAX
#define VPOSITION_VALUE_AUTO		LAYOUT_COORD_MIN
#define VPOSITION_VALUE_MIN			(LAYOUT_COORD_MIN + LayoutCoord(1))
#define VPOSITION_VALUE_MAX			LAYOUT_COORD_MAX

#define CONTENT_SIZE_AUTO			LAYOUT_COORD_MIN
#define CONTENT_SIZE_O_SIZE			(LAYOUT_COORD_MIN+LayoutCoord(1))
#define CONTENT_SIZE_O_SKIN			(LAYOUT_COORD_MIN+LayoutCoord(2))
#define CONTENT_SIZE_SPECIAL		(LAYOUT_COORD_MIN+LayoutCoord(2))
#define CONTENT_SIZE_MIN			(LAYOUT_COORD_MIN+LayoutCoord(3))
#define CONTENT_SIZE_MAX			LAYOUT_COORD_MAX
#define CONTENT_WIDTH_AUTO			CONTENT_SIZE_AUTO
#define CONTENT_WIDTH_O_SIZE		CONTENT_SIZE_O_SIZE
#define CONTENT_WIDTH_O_SKIN		CONTENT_SIZE_O_SKIN
#define CONTENT_WIDTH_SPECIAL		CONTENT_SIZE_SPECIAL
#define CONTENT_WIDTH_MIN			CONTENT_SIZE_MIN
#define CONTENT_WIDTH_MAX			CONTENT_SIZE_MAX
#define CONTENT_HEIGHT_AUTO			CONTENT_SIZE_AUTO
#define CONTENT_HEIGHT_O_SIZE		CONTENT_SIZE_O_SIZE
#define CONTENT_HEIGHT_O_SKIN		CONTENT_SIZE_O_SKIN
#define CONTENT_HEIGHT_SPECIAL		CONTENT_SIZE_SPECIAL
#define CONTENT_HEIGHT_MIN			CONTENT_SIZE_MIN
#define CONTENT_HEIGHT_MAX			CONTENT_SIZE_MAX

/* Limit computed margin and padding values to something reasonable.
   Using the eighth of max avoids most integer overflows when
   computing total widths and height of the margin box . */

#define MARGIN_PADDING_VALUE_MIN	(LAYOUT_COORD_MIN / LayoutCoord(8))
#define MARGIN_PADDING_VALUE_MAX	(LAYOUT_COORD_MAX / LayoutCoord(8))

#define CLIP_NOT_SET			LAYOUT_COORD_MIN
#define CLIP_AUTO				(LAYOUT_COORD_MIN+LayoutCoord(1))
#define CLIP_MIN_VALUE			(LAYOUT_COORD_MIN+LayoutCoord(2))
#define CLIP_MAX_VALUE			LAYOUT_COORD_MAX

#define COLUMN_SPAN_ALL			SHRT_MAX

#define ATTR_WIDTH_SUPPORT		0x0001
#define ATTR_HEIGHT_SUPPORT		0x0002
#define ATTR_BACKGROUND_SUPPORT	0x0004

BOOL SupportAttribute(Markup::Type elm_type, long attr);

/** Constrain the font size that is in layout fixed format
	by the min/max font size prefs.

	@param font_size The font size to constrain.
	@return The constrained font size. */

LayoutFixed ConstrainFixedFontSize(LayoutFixed font_size);

/** Page/column specific properties.

	Most of the members are used by both paged layout and multicol layout. */

struct PageProperties
{
	short			break_before;
	short			break_after;
	short			break_inside;
	short			orphans;
	short			widows;
	const uni_char*	page;
};

/** The class provides a method to resolve CSS lengths to pixels or to layout
	fixed point. Stores all the base values needed to compute relative CSS
	lengths.

	One instance of this class can be used for multiple calculations
	of the values that have the same base. It is also allowed to modify
	the bases for each units separately. Holds a pointer to VisualDevice
	instance, so should live shorter than it.
	Currently supported relative CSS units are:
	CSS_EM, CSS_REM, CSS_EX, CSS_PERCENTAGE.
	@see modules/style/css_value_types.h */

class CSSLengthResolver
{
public:

	/** Explicit ctor to avoid using as VisualDevice -> CSSLengthResolver
		conversion. If used as CSSLengthResolver(VisualDevice*), keep in mind
		that all the relative lengths' bases are defaulted to zero.

		@param vis_dev The VisualDevice that should be used to fetch bases
			   for some units.
		@param result_in_layout_fixed If TRUE then the returned value from
			   GetLengthInPixels will be in layout fixed point format.
		@param percentage_base The value that the percentage will be taken from.
			   Float allows to keep subpixels. Also the single precision is enough
			   as long as GetLengthInPixels takes the param of the float type also.
		@param font_size The font size in layout fixed point format.
			   Base for em unit and needed for ex unit calculation also.
		@param font_number The font number. Used to determine ex unit length.
		@param root_font_size The root font size layout fixed point format.
			   Base for rem unit.
		@param constrain_root_font_size If TRUE, the object will take care of
			   constraining the root font size whenever it changes (or is set intially). */

					explicit CSSLengthResolver(VisualDevice* vis_dev,
											   BOOL result_in_layout_fixed = FALSE,
											   float percentage_base = 0.0f,
											   LayoutFixed font_size = LayoutFixed(0),
											   int font_number = -1,
											   LayoutFixed root_font_size = LayoutFixed(0),
											   BOOL constrain_root_font_size = TRUE)
						: vis_dev(vis_dev)
						, result_in_layout_fixed(result_in_layout_fixed)
						, percentage_base(percentage_base)
						, font_size(font_size)
						, font_number(font_number)
						, ex_length(-1)
						, constrain_root_font_size(constrain_root_font_size)
					{ this->root_font_size = constrain_root_font_size ? ConstrainFixedFontSize(root_font_size) : root_font_size; }

	/** Get the values needed for relative lengths calculations from the current state
		of the VisualDevice member.

		@note Percentage base remains unchanged. */

	void			FillFromVisualDevice();

	/** Set new percentage base and return ref to this.
		Can be used only when both current percentage base and the new one
		are integers (not layout fixed point).

		@param new_percentage New percentage base. */

	CSSLengthResolver&
					ChangePercentageBase(float new_percentage) { percentage_base = new_percentage; return *this;}

	void			SetPercentageBase(float new_percentage) { percentage_base = new_percentage; }

	void			SetFontSize(LayoutFixed new_font_size) { font_size = new_font_size; ex_length = -1; }

	void			SetRootFontSize(LayoutFixed new_root_font_size) { root_font_size = constrain_root_font_size ? ConstrainFixedFontSize(new_root_font_size) : new_root_font_size;  }

	void			SetResultInLayoutFixed(BOOL the_truth) { result_in_layout_fixed = the_truth; }
	BOOL			GetResultInLayoutFixed() const { return result_in_layout_fixed; }

	void			SetFontNumber(int new_font_number) { font_number = new_font_number; ex_length = -1;}

	/** Get the length in pixels (as integer or layout fixed point)
	    using this base, from a given CSS length.

		@param val The floating point length value.
		@param vtype The CSS length unit.
			   @see CSSValueType

		@return The length in pixels. Either full pixel, integer value or layout
				fixed point value (in such case it might be e.g. a pixel and half).
				Depends on on of this' params.
				@see CSSLengthResolver::SetResultInLayoutFixed
				@see CSSLengthResolver::CSSLengthResolver */

	PixelOrLayoutFixed
					GetLengthInPixels(float val, short vtype) const;

private:

	/** Gets ex unit length (height of 'x' glyph part that is above the baseline
		for the given font settings). Currently needs to use pixel font size,
		regardless of having decimal base or not.
		However, if the ex unit length couldn't be fetched, falls back to
		font_size / 2 (em base / 2).

		@return ex unit length in pixels. */

	int				GetExUnitLength() const;

	/** Needed for e.g. for fetching ex unit. */

	VisualDevice*	vis_dev;

	/** Used to control whether the result returned from
		GetLengthInPixels is in layout fixed point format
		or a full pixel amount.
		@see CSSLengthResolver::GetLengthInPixels */

	BOOL			result_in_layout_fixed;

	/** Base for percentage unit in pixels. */

	float			percentage_base;

	/** Base for em unit in layout fixed point format. Also used for ex. */

	LayoutFixed		font_size;

	/** Needed for ex unit calculation. */

	int				font_number;

	/** Temporary store for ex unit. Used to avoid multiple
		calls into gfx engine. */

	mutable int		ex_length;

	/** Base for rem unit in layout fixed point format. */

	LayoutFixed		root_font_size;

	/** Should constrain the new root font size using current minimum/maximum
		font size settings (either passed in ctor or SetRootFontSize() or set
		in FillFromVisualDevice()). */

	BOOL			constrain_root_font_size;
};


class BgImages
{
public:
	enum {
		UNKNOWN_LAYER_COUNT = -1
	};

	BgImages() : bg_images_cp(NULL), bg_repeats_cp(NULL), bg_attachs_cp(NULL),
				 bg_positions_cp(NULL), bg_origins_cp(NULL), bg_clips_cp(NULL), bg_sizes_cp(NULL) {}

	/** Get the number of background layers.

		The number of layers is determined by the number of values in
		the 'background-image' property. Note that a value of 'none'
		still creates a layer */

	int GetLayerCount() const;

	/** Get a specific background layer. */

	void GetLayer(const CSSLengthResolver& length_resolver, int i, BackgroundProperties &background_props) const;

	/** Reset the background images back to default state. */

	void Reset() {
		bg_images_cp = NULL; bg_repeats_cp = NULL; bg_attachs_cp = NULL; bg_positions_cp = NULL;
		bg_origins_cp = NULL; bg_clips_cp = NULL; bg_sizes_cp = NULL;
	}

	static CSS_gen_array_decl *AsGenArray(CSS_decl* decl) { return (decl && decl->GetDeclType() == CSS_DECL_GEN_ARRAY) ? static_cast<CSS_gen_array_decl*>(decl) : NULL; }

	/* Setters used for setting the value from the cascade. The
	   objects must have a life-span equal or longer than this
	   instance, or the need to be reset when the declarations is
	   deleted.

	   This is normally not a problem since BgImages and CSS_decl* are
	   both owned the cascade and handled there.  When BgImages is
	   used in other contexts (like for document root properties), the
	   declarations must be referenced before stored here. */

	void SetImages(CSS_decl *decl) { bg_images_cp = AsGenArray(decl); }
	void SetRepeats(CSS_decl *decl) { bg_repeats_cp = AsGenArray(decl); }
	void SetAttachs(CSS_decl *decl) { bg_attachs_cp = AsGenArray(decl); }
	void SetPositions(CSS_decl *decl) { bg_positions_cp = AsGenArray(decl); }
	void SetOrigins(CSS_decl *decl) { bg_origins_cp = AsGenArray(decl); }
	void SetClips(CSS_decl *decl) { bg_clips_cp = AsGenArray(decl); }
	void SetSizes(CSS_decl *decl) { bg_sizes_cp = AsGenArray(decl); }

	/* Getters and setters used for inheritance  */

	void SetImages(CSS_gen_array_decl *decl) { bg_images_cp = decl; }
	void SetRepeats(CSS_gen_array_decl *decl) { bg_repeats_cp = decl; }
	void SetAttachs(CSS_gen_array_decl *decl) { bg_attachs_cp = decl; }
	void SetPositions(CSS_gen_array_decl *decl) { bg_positions_cp = decl; }
	void SetOrigins(CSS_gen_array_decl *decl) { bg_origins_cp = decl; }
	void SetClips(CSS_gen_array_decl *decl) { bg_clips_cp = decl; }
	void SetSizes(CSS_gen_array_decl *decl) { bg_sizes_cp = decl; }

	CSS_gen_array_decl *GetImages() const { return bg_images_cp; }
	CSS_gen_array_decl *GetRepeats() const { return bg_repeats_cp; }
	CSS_gen_array_decl *GetAttachs() const { return bg_attachs_cp; }
	CSS_gen_array_decl *GetPositions() const { return bg_positions_cp; }
	CSS_gen_array_decl *GetOrigins() const { return bg_origins_cp; }
	CSS_gen_array_decl *GetClips() const { return bg_clips_cp; }
	CSS_gen_array_decl *GetSizes() const { return bg_sizes_cp; }

#ifdef SKIN_SUPPORT
	/** If this is a single skin background, return the name as a string */
	const uni_char *GetSingleSkinName();
#endif

	/** @return TRUE if there are no background images */

	BOOL IsEmpty() const { return !bg_images_cp; }

	/** Check if there is background positions with percentage
		positions.

		@param x Set to TRUE if there are x-wise percentage
		         values. Otherwise FALSE.
		@param y Set to TRUE if there are
		         x-wise percentage values. Otherwise FALSE. */
	void HasPercentagePositions(BOOL &x, BOOL &y) const;

	/** @return if there is a background position with percentage positions. */
	BOOL HasPercentagePositions() const;

	/** Load background images.  Normally done during Reflow to load
		background images, but is also used in rare occations during
		Paint to load background images that haven't been loaded yet.

		@param media_type The current media_type
		@param doc The current document
		@param element The element to load the element to.
		@param load_only_extra Load only extra background images which means all but the first background image.

		@return An OP_LOAD_INLINE_STATUS really as return from the doc
		        complex. */
	OP_STATUS LoadImages(int media_type, FramesDocument *doc, HTML_Element *element, BOOL load_only_extra = FALSE) const;

	/** Check if any image has been loaded. Used by 'forms' to know if
		'forms' or layout should draw the background. */

	BOOL IsAnyImageLoaded(URL base_url) const;

	/** Clone a property for use in getting a computed value from the cascade.

		@return NULL on OOM or internal error. */

	CSS_decl *CloneProperty(short property) const;

	/** Helper function to set the background size for a specific layer. */

	void SetBackgroundSize(const CSSLengthResolver& length_resolver, int num, BackgroundProperties &background) const;

	/** Helper function to set the background position for a specific layer. */

	void SetBackgroundPosition(const CSSLengthResolver& length_resolver, int num, BackgroundProperties &background) const;

private:

	CSS_gen_array_decl*		bg_images_cp;
	CSS_gen_array_decl*		bg_repeats_cp;
	CSS_gen_array_decl*		bg_attachs_cp;
	CSS_gen_array_decl*		bg_positions_cp;
	CSS_gen_array_decl*		bg_origins_cp;
	CSS_gen_array_decl*		bg_clips_cp;
	CSS_gen_array_decl*		bg_sizes_cp;
};

class Shadows
{
public:
	Shadows() : shadows_cp(NULL) { }

	void Reset() { shadows_cp = NULL; }

	/** @return the number of box shadows. */
	int				GetCount() const;

	/** @return the distance to the furthest box shadow from the
		box. Used for bounding box calculation. */
	LayoutCoord		GetMaxDistance(const CSSLengthResolver& length_resolver) const;

	void			Set(CSS_decl* decl) { shadows_cp = decl; }
	CSS_decl*		Get() const { return shadows_cp; }

	/** Iterator helper.

		Can move in forward or reverse directions (but not both at the same time.) */

	class Iterator
	{
	public:
		Iterator(const Shadows& shadow);

		/** Move the iterator to after the last declaration. */

		void MoveToEnd() { pos = arr_len; }

		/** Get the next shadow declaration in the shadow array. */

		BOOL GetNext(const CSSLengthResolver& length_resolver, Shadow& shadow);

		/** Get the previous shadow declaration in the shadow array. */

		BOOL GetPrev(const CSSLengthResolver& length_resolver, Shadow& shadow);

	private:
		void HandleValue(const CSS_generic_value* value,
						 const CSSLengthResolver& length_resolver, Shadow& shadow);

		const CSS_generic_value* arr;
		int arr_len;
		int pos;
		int length_count;
	};

private:
	CSS_decl*		shadows_cp;
};

/** Stores the various components of the object-position property. Contains
  * algorithms for both object-position and object-fit.
  *
  * Also used for communication between layout and svg (which is the reason
  * for some of the extra constructors).
  *
  *	@author Leif Arne Storset
  */

struct ObjectFitPosition
{
	LayoutCoord x;
	LayoutCoord y;

	signed short fit:16;
	unsigned int x_percent:1;
	unsigned int y_percent:1;

	ObjectFitPosition(short fit, LayoutCoord x, LayoutCoord y, unsigned int x_percent, unsigned int y_percent) :
		x(x), y(y), fit(fit), x_percent(x_percent), y_percent(y_percent) {}
	ObjectFitPosition() : x(0), y(0), fit(CSS_VALUE_UNSPECIFIED), x_percent(FALSE), y_percent(FALSE) {}

	/** Calculate height and width of the painted replaced content (that is,
		not the box, but the actual content). Used with property object-fit.

		If 'object-fit: auto', callers should override it with the appropriate
		value, but this function will fall back to 'object-fit: fill'.

	 * \warning This code is duplicated in SVGObjectFitPosition::CalculateObjectFit
	 *			(with SVGNumbers). Any modifications must be consistent.
	 *
	 * @param intrinsic			the "intrinsic" size of the content being processed.
	 * @param inner				size of the content box.
	 * @param dst				resulting size of the painted content.
	 */

	void	CalculateObjectFit(short fit_fallback, OpRect intrinsic, OpRect inner, OpRect& dst) const;

	/** Resolve percentages to find out where the top and left edges of replaced
		content go.
	 *
	 * Imitated from BackgroundsAndBorder::ComputeImagePosition.
	 *
	 * \warning This code is duplicated in SVGObjectFitPosition::CalculateObjectPosition
	 * (with SVGRects). Any modifications must be consistent.
	 *
	 * @param pos_rect          the rectangle (such as border box or content box) within
	 *                          which to position the content.
	 * @param[inout] content    provide the size of the content to position (in) and receive
	 *                          the calculated position of the content (out).
	 */
	void	CalculateObjectPosition(const OpRect& pos_rect, OpRect& content) const;
};


#ifdef CSS_TRANSFORMS

/** CSSTransforms

	The CSSTransforms is responsible for computing a final matrix from
	basically a transformed area (box width and box height), transform
	origin and a transform list. */

class CSSTransforms
{
public:
	CSSTransforms() : transform_origin_x(50), transform_origin_y(50) {
		packed_init = 0;
		packed.transform_origin_x_is_percent = 1;
		packed.transform_origin_y_is_percent = 1;
		transform.LoadIdentity();
	}

	/** Apply the final transform to a affine transformation. */

	void			ApplyToAffineTransform(AffineTransform &t) const { t.PostMultiply(transform); }

	/** Get the final transform as a AffineTransform */

	void			GetTransform(AffineTransform &t) const { t = transform; }

	/** Compute the transform origin. */

	void			ComputeTransformOrigin(LayoutCoord box_width, LayoutCoord box_height,
										   float &origin_x, float &origin_y);

	/** Compute the final transform, including transform-origin from
		the layout properties.  The visual device is used for
		resolving lengths. */

	BOOL			ComputeTransform(FramesDocument* doc,
									 const HTMLayoutProperties &props,
									 LayoutCoord box_width,
									 LayoutCoord box_height);

	/** Set transform origin x. */

	void			SetOriginX(LayoutCoord origin_x, BOOL is_percent, BOOL is_decimal) {
						transform_origin_x = origin_x;
						packed.transform_origin_x_is_percent = is_percent;
						packed.transform_origin_x_is_decimal = is_decimal;
					}

	/** Set transform origin y. */

	void			SetOriginY(LayoutCoord origin_y, BOOL is_percent, BOOL is_decimal) {
						transform_origin_y = origin_y;
						packed.transform_origin_y_is_percent = is_percent;
						packed.transform_origin_y_is_decimal = is_decimal;
					}

	/** Set transform from CSS declaration. */

	void			SetTransformFromProps(const HTMLayoutProperties &props,
										  FramesDocument* doc,
										  short box_width, long box_height);

protected:

	/** Reset transform. */

	void			Reset() {	transform.LoadIdentity();
								transform_origin_x = LayoutCoord(50);
								transform_origin_y = LayoutCoord(50);
								packed.has_non_translate = FALSE; }

private:

	/** A list of operations */

	AffineTransform	transform;

	/* The transform origin x */

	LayoutCoord		transform_origin_x;

	/* The transform origin y */

	LayoutCoord		transform_origin_y;

	union {
		struct {

			/** There is a small optimization that avoids involving the
				transform origin if it isn't necessary, i.e. it has only
				translation operations. */

			unsigned int has_non_translate:1;

			/** Transform origin x is percentage */

			unsigned int transform_origin_x_is_percent:1;

			/** Transform origin y is percentage */

			unsigned int transform_origin_y_is_percent:1;

			/** Transform origin x is percentage */

			unsigned int transform_origin_x_is_decimal:1;

			/** Transform origin y is percentage */

			unsigned int transform_origin_y_is_decimal:1;
		} packed;
		unsigned int packed_init;
	};
};

#endif // CSS_TRANSFORMS

class Container;
class LayoutProperties;

#ifdef CURRENT_STYLE_SUPPORT
class HTMLayoutPropertiesTypes
{
public:

	short	m_top;
	short	m_left;
	short	m_right;
	short	m_bottom;

	short	m_margin_top;
	short	m_margin_left;
	short	m_margin_right;
	short	m_margin_bottom;

	short	m_padding_top;
	short	m_padding_left;
	short	m_padding_right;
	short	m_padding_bottom;

	short	m_min_width;
	short	m_max_width;
	short	m_min_height;
	short	m_max_height;

	short	m_brd_top_width;
	short	m_brd_left_width;
	short	m_brd_right_width;
	short	m_brd_bottom_width;

	short	m_outline_width;
	short	m_outline_offset;

	short	m_brd_spacing_hor;
	short	m_brd_spacing_ver;

	short	m_text_indent;

	short	m_width;
	short	m_height;

	short	m_font_weight;
	short	m_font_size;

	short	m_line_height;
	short	m_vertical_align;

	short	m_word_spacing;
	short	m_letter_spacing;

	short	m_bg_xpos;
	short	m_bg_ypos;

	short	m_bg_xsize;
	short	m_bg_ysize;

	short	m_clip_top;
	short	m_clip_right;
	short	m_clip_bottom;
	short	m_clip_left;

	short	m_column_width;
	short	m_column_gap;
	short	m_column_rule_width;

	short	m_flex_basis;

	void	Reset(HTMLayoutPropertiesTypes *parent_types);
};
#endif // CURRENT_STYLE_SUPPORT

class HTMLayoutProperties
{
public:

	enum CurrentFontType
	{
		CurrentFontTypeDefault,
		CurrentFontTypeGeneric,
		CurrentFontTypeDefined
	};
#ifdef CURRENT_STYLE_SUPPORT
	HTMLayoutPropertiesTypes*
					types; // used for current style to get the type of values
#endif // CURRENT_STYLE_SUPPORT

	LayoutFixed		line_height_i;

	short			text_decoration;
	short			text_transform;
	short			text_indent;
	short			text_align;
	short			vertical_align;
	short			vertical_align_type;
	short			small_caps;

	short			letter_spacing;
	LayoutFixed		word_spacing_i;

	short			list_style_type;
	short			list_style_pos;
	CssURL			list_style_image;

#ifdef CSS_GRADIENT_SUPPORT
	CSS_Gradient*	list_style_image_gradient;
#endif // CSS_GRADIENT_SUPPORT

	short			float_type;
	short			display_type;
	short			clear;
	short			visibility;
	short			resize;

	short			position;
	LayoutCoord		top;
	LayoutCoord		left;
	LayoutCoord		right;
	LayoutCoord		bottom;
	long			z_index;
	long			table_baseline;

	short			computed_overflow_x; ///< Computed value of overflow-x
	short			computed_overflow_y; ///< Computed value of overflow-y
	short			overflow_x; ///< Actual value of overflow-x
	short			overflow_y; ///< Actual value of overflow-y

#ifdef SUPPORT_TEXT_DIRECTION
	short			direction;
	short			unicode_bidi;
#endif

	short			text_overflow;

	short			white_space;
	short			overflow_wrap;
	short			table_layout;
	short			caption_side;
	short			box_decoration_break;

	// margin and padding
	LayoutCoord		margin_top;
	LayoutCoord		margin_left;
	LayoutCoord		margin_right;
	LayoutCoord		margin_bottom;
	LayoutCoord		padding_top;
	LayoutCoord		padding_left;
	LayoutCoord		padding_right;
	LayoutCoord		padding_bottom;

	// dimension
	short			box_sizing;

	/** @name content width and height.

		Content width and height from CSS. Negative values mean percentages stored
		as layout fixed point. So 100% is equal to -100 * LAYOUT_FIXED_POINT_BASE.
		Maximum allowed percentage is then
		(LENGTH_PROPS_VAL_MAX / LAYOUT_FIXED_POINT_BASE)%

		LENGTH_PROPS_VAL_MAX is defined in modules/layout/cssprops.h
		It is divided by LAYOUT_FIXED_POINT_BASE to ensure enough room
		when multiplying a non fixed point percentage. */

	//@{
	LayoutCoord		content_width;
	LayoutCoord		content_height;
	//@}

	LayoutCoord		min_width;
	LayoutCoord		max_width;
	LayoutCoord		normal_max_width; // Computed value of max-width, not affected by ERA
	LayoutCoord		min_height;
	LayoutCoord		max_height;

	LayoutCoord		era_max_width;

	// font
	COLORREF		font_color;
	short			font_weight;
	short			font_italic;
	short			font_number;
	short			font_size;
	/* decimal_font_size constrained by min/max settings and possibly modified by
	   flexible font system in ERA, in such case it might be equal
	   to font_size * LAYOUT_FIXED_POINT_BASE. */
	LayoutFixed		decimal_font_size_constrained;
	LayoutFixed		decimal_font_size; // not limited by any min/max font size boundaries
	short			unflex_font_size; // used by the flexible font system in ERA (the font size that would be used if not flexed)

	// border
	Border			border;

	/** outline
		NOTE: If the outline-style is -o-highlight-border, then outline.width is
		really a sum of the width of the painted solid border and internal skin
		offset (skin padding). The outline-offset property also matters, so the
		visual outline offset is a sum of the skin padding and the specified
		offset.
		@see OpSkinElement::GetPadding() */

	BorderEdge		outline;

	short			outline_offset; // computed value of outline-offset property

	// multi-column
	BorderEdge		column_rule;
	LayoutCoord		column_width;
	short			column_count;
	LayoutCoord		column_gap;
	short			column_fill;
	short			column_span;

	// flexbox
	FlexboxModes	flexbox_modes;
	int				order;
	float			flex_grow;
	float			flex_shrink;
	LayoutCoord		flex_basis; ///< May be CONTENT_SIZE_AUTO or fixed-point percentage. @see content_width

	// cursor
	CursorType		cursor;

	// opacity
	UINT8			opacity;

	short			border_collapse;
	short			border_spacing_horizontal;
	short			border_spacing_vertical;
	short			empty_cells;

	/* Background properties */

	COLORREF		bg_color;
	BgImages		bg_images;

	ObjectFitPosition object_fit_position;

	LayoutCoord		containing_block_width;
	LayoutCoord		containing_block_height;
	short			font_ascent;
	short			font_descent;
	short			font_internal_leading;
	short			font_average_width;
	short			font_underline_position;
	short			font_underline_width;

	short			parent_font_ascent;
	short			parent_font_descent;
	short			parent_font_internal_leading;

	COLORREF		current_bg_color;

	COLORREF		text_bg_color; // used for color contrasting in era

	short			align_block_elements;

	short			table_rules;

	short			tab_size;

	// Baseline for text decoration. Only set and used during painting.
	short			textdecoration_baseline;

	// image-rendering
	short			image_rendering;

	// text-decoration colors
	COLORREF		overline_color;
	COLORREF		linethrough_color;
	COLORREF		underline_color;

	// text-selection colors
	COLORREF		selection_color;
	COLORREF		selection_bgcolor;

	union
	{
		struct
		{
			unsigned long   bidi_align_specified:1;
			unsigned long   align_specified:1;
			unsigned long	vertical_align_specified:1;
			unsigned long	margin_top_specified:1;
			unsigned long	margin_left_specified:1;
			unsigned long	padding_left_specified:1;
			unsigned long	margin_right_specified:1;
			unsigned long	padding_right_specified:1;
			unsigned long	margin_bottom_specified:1;
			unsigned long	margin_left_auto:1;
			unsigned long	margin_right_auto:1;
			unsigned long	margin_top_auto:1;
			unsigned long	margin_bottom_auto:1;
			unsigned long	was_inline:1;
			unsigned long	form_font_not_default:1;
			unsigned long   left_is_percent:1;
			unsigned long	margin_left_is_percent:1;
			unsigned long	padding_left_is_percent:1;
			unsigned long	margin_right_is_percent:1;
			unsigned long	padding_right_is_percent:1;
			unsigned long	font_size_specified:1;
			unsigned long	horizontal_props_with_percent:1;
			unsigned long	vertical_props_with_percent:1;
			unsigned long	decoration_ancestors:1; // Is this element is affected by text-decoration set on ancestors?
			unsigned long	height_is_percent:1;
			unsigned long	text_indent_is_percent:1;
			unsigned long	awaiting_webfont:1;
			// unused:5
		} info;
		UINT32 info_init;
	};

	union
	{
		// still room for 4 more bits here
		struct
		{
			unsigned long	margin_left_explicitly_set:1;
			unsigned long	margin_right_explicitly_set:1;
			unsigned long	margin_top_explicitly_set:1;
			unsigned long	margin_bottom_explicitly_set:1;
			unsigned long	apply_default_vertical_padding_not_margin:1;
			unsigned long	apply_default_horizontal_padding_not_margin:1;
			unsigned long	min_width_is_percent:1;
			unsigned long	max_width_is_percent:1;
			unsigned long	min_height_is_percent:1;
			unsigned long	max_height_is_percent:1;
			unsigned long	is_inside_horizontal_marquee:1; // Hints to limit paragraph width
			unsigned long	skip_empty_cells_border:1; // In quirks mode, paint the background but not the border of empty cells
			unsigned long	is_in_outline:1; // this element is inside an outline;
			unsigned long	is_outline_none_specified:1; // TRUE iff the outline style is none AND it is not an initial value.
			unsigned long	bg_is_transparent:1; // Moved from BackgroundProperties
			unsigned long	is_inside_replaced_content:1; // The cascade is inside replaced content. Prevent things like overriding box type.
			unsigned long	is_inside_position_fixed:1;
#ifdef CSS_TRANSFORMS
			unsigned long   is_inside_transform:1;
			unsigned long	transform_origin_x_is_percent:1;
			unsigned long	transform_origin_y_is_percent:1;
			unsigned long	transform_origin_x_is_decimal:1;
			unsigned long	transform_origin_y_is_decimal:1;
#endif
			unsigned long	margin_top_is_percent:1;
			unsigned long	padding_top_is_percent:1;
			unsigned long	margin_bottom_is_percent:1;
			unsigned long	padding_bottom_is_percent:1;
			unsigned long	is_flex_item:1; // Is this a flexbox child?
#ifdef WEBKIT_OLD_FLEXBOX
			unsigned long	is_oldspec_flexbox:1;
#endif // WEBKIT_OLD_FLEXBOX
		} info2;
		UINT32 info2_init;
	};

	BOOL            GetAlignSpecified() const { return info.align_specified; }
	BOOL            GetBidiAlignSpecified() const { return info.bidi_align_specified; }
	BOOL            GetVerticalAlignSpecified() const { return info.vertical_align_specified; }
	BOOL			GetMarginTopSpecified() const { return info.margin_top_specified; }
	BOOL			GetMarginLeftSpecified() const { return info.margin_left_specified; }
	BOOL			GetPaddingLeftSpecified() const { return info.padding_left_specified; }
	BOOL			GetMarginRightSpecified() const { return info.margin_right_specified; }
	BOOL			GetPaddingRightSpecified() const { return info.padding_right_specified; }
	BOOL			GetMarginBottomSpecified() const { return info.margin_bottom_specified; }
	BOOL			GetMarginLeftAuto() const { return info.margin_left_auto; }
	BOOL			GetMarginRightAuto() const { return info.margin_right_auto; }
	BOOL			GetMarginTopAuto() const { return info.margin_top_auto; }
	BOOL			GetMarginBottomAuto() const { return info.margin_bottom_auto; }
	BOOL			GetWasInline() const { return info.was_inline; }
	BOOL			GetLeftIsPercent() const { return info.left_is_percent; }
	BOOL			GetMarginLeftIsPercent() const { return info.margin_left_is_percent; }
	BOOL			GetPaddingLeftIsPercent() const { return info.padding_left_is_percent; }
	BOOL			GetMarginRightIsPercent() const { return info.margin_right_is_percent; }
	BOOL			GetPaddingRightIsPercent() const { return info.padding_right_is_percent; }
	BOOL			GetFontSizeSpecified() const { return info.font_size_specified; }
	BOOL			GetHasHorizontalPropsWithPercent() const { return info.horizontal_props_with_percent; }
	BOOL			GetHasVerticalPropsWithPercent() const { return info.vertical_props_with_percent; }
	BOOL			GetHasDecorationAncestors() const { return info.decoration_ancestors; }
	BOOL			GetHeightIsPercent() const { return info.height_is_percent; }
	BOOL			GetTextIndentIsPercent() const { return info.text_indent_is_percent; }
	BOOL			GetAwaitingWebfont() const { return info.awaiting_webfont; }
	BOOL			GetMarginLeftExplicitlySet() const { return info2.margin_left_explicitly_set; }
	BOOL			GetMarginRightExplicitlySet() const { return info2.margin_right_explicitly_set; }
	BOOL			GetMarginTopExplicitlySet() const { return info2.margin_top_explicitly_set; }
	BOOL			GetMarginBottomExplicitlySet() const { return info2.margin_bottom_explicitly_set; }
	BOOL			GetApplyDefaultVerticalPaddingNotMargin() const { return info2.apply_default_vertical_padding_not_margin; }
	BOOL			GetApplyDefaultHorizontalPaddingNotMargin() const { return info2.apply_default_horizontal_padding_not_margin; }
	BOOL			GetMinWidthIsPercent() const { return info2.min_width_is_percent; }
	BOOL			GetMaxWidthIsPercent() const { return info2.max_width_is_percent; }
	BOOL			GetMinHeightIsPercent() const { return info2.min_height_is_percent; }
	BOOL			GetMaxHeightIsPercent() const { return info2.max_height_is_percent; }
	BOOL			IsInsideHorizontalMarquee() const { return info2.is_inside_horizontal_marquee; } // Hints to limit paragraph width
	BOOL			GetSkipEmptyCellsBorder() const { return info2.skip_empty_cells_border; }
	BOOL			GetIsInOutline() const { return info2.is_in_outline; }

	/** TRUE iff outline-style is none AND is not an initial value
		In other words - the outline-style:none was specified in some stylesheet in
		a selector that matches the element and it became a computed value.

		This is hackish thing related with CORE-43426. Specified outline:none
		can be used to disable the element highlight just as the outline with zero
		width.

		@see LayoutWorkplace::DrawHighlight. */

	BOOL			GetIsOutlineNoneSpecified() const { return info2.is_outline_none_specified; }

	/** Returns TRUE if outline is specified and has non-0 width. */

	BOOL			IsOutlineVisible() const { return outline.style != CSS_VALUE_none && outline.width > 0; }

	/** Are the outline props such that they should disable the skin highlight?

		Skin highlight is the way of highlight we do with TWEAK_SKIN_HIGHLIGHTED_ELEMENT
		enabled. Such way includes also one of the standard outline types.

		Currently this is a bit more tricky than !IsOutlineVisible(). The exception
		is that initial "outline:none" values should not disable highlighting. */

	BOOL			ShouldOutlineDisableHighlight() const { return GetIsOutlineNoneSpecified() || outline.style != CSS_VALUE_none && outline.width == 0; }

	BOOL			GetBgIsTransparent() const { return info2.bg_is_transparent; }
	BOOL			IsInsideReplacedContent() const { return info2.is_inside_replaced_content; }
	BOOL			IsInsidePositionFixed() const { return info2.is_inside_position_fixed; }

#ifdef CSS_TRANSFORMS
	BOOL			IsInsideTransform() const { return info2.is_inside_transform; } // TRUE if one of our ancestors have a transform
	BOOL			GetTransformOriginXIsPercent() const { return info2.transform_origin_x_is_percent; }
	BOOL			GetTransformOriginYIsPercent() const { return info2.transform_origin_y_is_percent; }
	BOOL			GetTransformOriginXIsDecimal() const { return info2.transform_origin_x_is_decimal; }
	BOOL			GetTransformOriginYIsDecimal() const { return info2.transform_origin_y_is_decimal; }
#endif
	BOOL			GetMarginTopIsPercent() const { return info2.margin_top_is_percent; }
	BOOL			GetPaddingTopIsPercent() const { return info2.padding_top_is_percent; }
	BOOL			GetMarginBottomIsPercent() const { return info2.margin_bottom_is_percent; }
	BOOL			GetPaddingBottomIsPercent() const { return info2.padding_bottom_is_percent; }
	BOOL			GetIsFlexItem() const { return info2.is_flex_item; }
#ifdef WEBKIT_OLD_FLEXBOX
	BOOL			GetIsOldspecFlexbox() const { return info2.is_oldspec_flexbox; }
#endif // WEBKIT_OLD_FLEXBOX


	void            SetAlignSpecified() { info.align_specified = 1;}
	void            SetBidiAlignSpecified() {info.bidi_align_specified = 1;}
	void            SetVerticalAlignSpecified() {info.vertical_align_specified = 1;}
	void			SetMarginTopSpecified() { info.margin_top_specified = 1; }
	void			SetMarginLeftSpecified() { info.margin_left_specified = 1; }
	void			SetPaddingLeftSpecified() { info.padding_left_specified = 1; }
	void			SetMarginRightSpecified() { info.margin_right_specified = 1; }
	void			SetPaddingRightSpecified() { info.padding_right_specified = 1; }
	void			SetMarginBottomSpecified() { info.margin_bottom_specified = 1; }
	void			SetMarginLeftAuto(BOOL val) { if (val) info.margin_left_auto = 1; else info.margin_left_auto = 0; }
	void			SetMarginRightAuto(BOOL val) { if (val) info.margin_right_auto = 1; else info.margin_right_auto = 0; }
	void			SetMarginTopAuto(BOOL val) { if (val) info.margin_top_auto = 1; else info.margin_top_auto = 0; }
	void			SetMarginBottomAuto(BOOL val) { if (val) info.margin_bottom_auto = 1; else info.margin_bottom_auto = 0; }
	void			SetWasInline(BOOL val) { if (val) info.was_inline = 1; else info.was_inline = 0; }
	void			SetLeftIsPercent() { info.left_is_percent = 1; }
	void			SetMarginLeftIsPercent() { info.margin_left_is_percent = 1; }
	void			SetPaddingLeftIsPercent() { info.padding_left_is_percent = 1; }
	void			SetMarginRightIsPercent() { info.margin_right_is_percent = 1; }
	void			SetPaddingRightIsPercent() { info.padding_right_is_percent = 1; }
	void			SetFontSizeSpecified() { info.font_size_specified = 1; }
	void			SetHasHorizontalPropsWithPercent() { info.horizontal_props_with_percent = 1; }
	void			SetHasVerticalPropsWithPercent() { info.vertical_props_with_percent = 1; }
	void			SetHasDecorationAncestors(BOOL val) { info.decoration_ancestors = val ? 1 : 0; }
	void			SetHeightIsPercent() { info.height_is_percent = 1; }
	void			SetTextIndentIsPercent() { info.text_indent_is_percent = 1; }
	void			SetAwaitingWebfont(BOOL val) { if (val) info.awaiting_webfont = 1; else info.awaiting_webfont = 0; }
	void			SetMarginLeftExplicitlySet() { info2.margin_left_explicitly_set = 1; }
	void			SetMarginRightExplicitlySet() { info2.margin_right_explicitly_set = 1; }
	void			SetMarginTopExplicitlySet() { info2.margin_top_explicitly_set = 1; }
	void			SetMarginBottomExplicitlySet() { info2.margin_bottom_explicitly_set = 1; }
	void			SetApplyDefaultVerticalPaddingNotMargin()  { info2.apply_default_vertical_padding_not_margin = 1; }
	void			SetApplyDefaultHorizontalPaddingNotMargin()  { info2.apply_default_horizontal_padding_not_margin = 1; }
	void			SetMinWidthIsPercent() { info2.min_width_is_percent = 1; }
	void			SetMaxWidthIsPercent() { info2.max_width_is_percent = 1; }
	void			SetMinHeightIsPercent() { info2.min_height_is_percent = 1; }
	void			SetMaxHeightIsPercent() { info2.max_height_is_percent = 1; }
	void			SetIsInsideHorizontalMarquee() { info2.is_inside_horizontal_marquee = 1; } // Hints to limit paragraph width
	void			SetSkipEmptyCellsBorder(BOOL val) { info2.skip_empty_cells_border = val; }
	void			SetIsInOutline(BOOL val) { info2.is_in_outline = val ? 1 : 0; }
	void			SetOutlineNoneSpecified() { info2.is_outline_none_specified = 1; }
	void			SetBgIsTransparent(BOOL val) { info2.bg_is_transparent = val ? 1 : 0; }
	void			SetIsInsideReplacedContent(BOOL val) { info2.is_inside_replaced_content = val ? 1 : 0; }
	void			SetIsInsidePositionFixed(BOOL val) { info2.is_inside_position_fixed = val ? 1 : 0; }
#ifdef CSS_TRANSFORMS
	void			SetIsInsideTransform(BOOL val) { info2.is_inside_transform = val ? 1 : 0; }
#endif
	void			SetMarginTopIsPercent() { info2.margin_top_is_percent = 1; }
	void			SetPaddingTopIsPercent() { info2.padding_top_is_percent = 1; }
	void			SetMarginBottomIsPercent() { info2.margin_bottom_is_percent = 1; }
	void			SetPaddingBottomIsPercent() { info2.padding_bottom_is_percent = 1; }
	void			SetIsFlexItem() { info2.is_flex_item = 1; }
#ifdef WEBKIT_OLD_FLEXBOX
	void			SetIsOldspecFlexbox() { info2.is_oldspec_flexbox = 1; }
#endif // WEBKIT_OLD_FLEXBOX

	BOOL			MarginsAdjoining(long css_height) const { return min_height == 0 &&
															  (content_height == 0 || content_height == CONTENT_HEIGHT_AUTO || css_height == CONTENT_HEIGHT_AUTO)  &&
															  !padding_top && !padding_bottom &&
															  !border.top.width && !border.bottom.width; }

	BOOL			WidthIsPercent() const { return content_width < 0 && content_width > CONTENT_WIDTH_SPECIAL; }

	/** Returns TRUE if content's height is relative.
		For non positioned boxes (abspos is FALSE) only percentage height is considered.
		For positioned boxes (abspos is TRUE) content's height is considered relative if height is either percentage based or top/bottom offsets are set. */

	BOOL			HeightIsRelative(BOOL abspos) const { return GetHeightIsPercent() || abspos && content_height == CONTENT_HEIGHT_AUTO && top != VPOSITION_VALUE_AUTO && bottom != VPOSITION_VALUE_AUTO; }

	/** Constrain a width value to min-width and max-width.

		Percentage min-width and max-width are ignored if 'ignore_percent' is TRUE. */

	LayoutCoord		CheckWidthBounds(LayoutCoord test_width, BOOL ignore_percent = FALSE) const;

	/** Constrain a height value to min-height and max-height.

		Percentage min-height and max-height are ignored if 'ignore_percent' is TRUE. */

	LayoutCoord		CheckHeightBounds(LayoutCoord test_height, BOOL ignore_percent = FALSE) const;

	BOOL			HasBorderRadius() const;
	BOOL			HasBorderImage() const;

	/** Finds how far shadows and outlines extend outside the border box in each direction, and returns
		the maximum value.

		For example, given <code>box-shadow: 10px 10px 10px</code>, the maximum extent is 15 (even if
		the extent is 0 above and to the left of the box).
		Given <code>outline: 2px; outline-offset: 3px</code>, the extent on every side, and thus the
		maximum extent, is 5px (in other words, the outline is counted only once, not for each side it
		appears on).

		@return the extent of the decoration; this is never negative even if outline-offset is. */

	LayoutCoord		DecorationExtent(FramesDocument *doc) const;

	/** Check whether a list-item marker is specified. Even if list-style-type is 'none',
		list-style-image can override this.

		@return TRUE if list-style-type is not 'none', or list-style-image is not 'none'. */

	BOOL			HasListItemMarker() const;

	PageProperties	page_props;

	LayoutCoord		clip_left;
	LayoutCoord		clip_right;
	LayoutCoord		clip_top;
	LayoutCoord		clip_bottom;

	CSS_decl*		content_cp;
	CSS_decl*		counter_reset_cp;
	CSS_decl*		counter_inc_cp;
	CSS_decl*		quotes_cp;

#ifdef GADGET_SUPPORT
    CSS_decl*		control_region_cp;
#endif // GADGET_SUPPORT

#ifdef CSS_TRANSFORMS
	CSS_decl*		transforms_cp;
	LayoutCoord		transform_origin_x;
	LayoutCoord		transform_origin_y;
#endif

	Shadows			text_shadows;
	Shadows			box_shadows;
	BorderImage		border_image;

	CSS_decl*		navup_cp;
	CSS_decl*		navdown_cp;
	CSS_decl*		navleft_cp;
	CSS_decl*		navright_cp;

#ifdef ACCESS_KEYS_SUPPORT
	CSS_decl*		accesskey_cp;
#endif // ACCESS_KEYS_SUPPORT

#ifdef CSS_TRANSITIONS
	CSS_decl*		transition_delay;
	CSS_decl*		transition_duration;
	CSS_decl*		transition_timing;
	CSS_decl*		transition_property;

	union
	{
		/** Contains bits to say if a computed value for a property comes
			from a transition or not. */
		struct
		{
			/* inherited:yes properties - 12 bits */
			unsigned int border_spacing:1;
			unsigned int font_color:1;
			unsigned int font_size:1;
			unsigned int font_weight:1;
			unsigned int letter_spacing:1;
			unsigned int line_height:1;
			unsigned int selection_color:1;
			unsigned int selection_bgcolor:1;
			unsigned int text_indent:1;
			unsigned int text_shadow:1;
			unsigned int visibility:1;
			unsigned int word_spacing:1;

			/* inherited:no properties - 20 bits */
			unsigned int bg_color:1;
			unsigned int bg_pos:1;
			unsigned int bg_size:1;
			unsigned int border_bottom_color:1;
			unsigned int border_bottom_left_radius:1;
			unsigned int border_bottom_right_radius:1;
			unsigned int border_bottom_width:1;
			unsigned int border_left_color:1;
			unsigned int border_left_width:1;
			unsigned int border_right_color:1;
			unsigned int border_right_width:1;
			unsigned int border_top_color:1;
			unsigned int border_top_left_radius:1;
			unsigned int border_top_right_radius:1;
			unsigned int border_top_width:1;
			unsigned int bottom:1;
			unsigned int box_shadow:1;
			unsigned int clip:1;
			unsigned int column_count:1;
			unsigned int column_gap:1;
		} transition_packed;
		UINT32 transition_packed_init;
	};

	union
	{
		/** Contains bits to say if a computed value for a property comes
			from a transition or not. */
		struct
		{
			/* inherited:no properties - 29 bits */
			unsigned int column_rule_color:1;
			unsigned int column_rule_width:1;
			unsigned int column_width:1;
			unsigned int height:1;
			unsigned int left:1;
			unsigned int margin_bottom:1;
			unsigned int margin_left:1;
			unsigned int margin_right:1;
			unsigned int margin_top:1;
			unsigned int max_height:1;
			unsigned int max_width:1;
			unsigned int min_height:1;
			unsigned int min_width:1;
			unsigned int object_position:1;
			unsigned int opacity:1;
			unsigned int outline_color:1;
			unsigned int outline_offset:1;
			unsigned int outline_width:1;
			unsigned int padding_bottom:1;
			unsigned int padding_left:1;
			unsigned int padding_right:1;
			unsigned int padding_top:1;
			unsigned int right:1;
			unsigned int top:1;
			unsigned int transform:1;
			unsigned int transform_origin:1;
			unsigned int valign:1;
			unsigned int width:1;
			unsigned int z_index:1;
			/* 3 bits left */
		} transition_packed2;
		UINT32 transition_packed2_init;
	};

	union
	{
		/** Contains bits to say if a computed value for a property comes
			from a transition or not. */
		struct
		{
			/* inherited:no properties - 4 bits */
			unsigned int order:1;
			unsigned int flex_grow:1;
			unsigned int flex_shrink:1;
			unsigned int flex_basis:1;
			/* 28 bits left */
		} transition_packed3;
		UINT32 transition_packed3_init;
	};

	/** Check if a property is being transitioned.

		@param prop The property to check for.
		@returns TRUE if the computed style for the passed property in this
				 object is the current value of a transition for that property. */
	BOOL			IsTransitioning(CSSProperty prop) const;

#endif // CSS_TRANSITIONS

#ifdef CSS_ANIMATIONS
	CSS_decl*		animation_name;
	CSS_decl*		animation_duration;
	CSS_decl*		animation_timing_function;
	CSS_decl*		animation_iteration_count;
	CSS_decl*		animation_direction;
	CSS_decl*		animation_play_state;
	CSS_decl*		animation_delay;
	CSS_decl*		animation_fill_mode;
#endif // CSS_ANIMATIONS

#ifdef _CSS_LINK_SUPPORT_
	CSS_decl*		set_link_source_cp;
	short			use_link_source;
#endif

	short			marquee_dir;
	long			marquee_loop;
	short			marquee_scrolldelay;
	short			marquee_style;

	short			indent_count; // used by ssr to limit the indentations
	short			font_size_base; // used by ssr. Font size should indicate a good base unit for many values.

#ifdef CSS_MINI_EXTENSIONS
	short			mini_fold;
	short			focus_opacity;
#endif // CSS_MINI_EXTENSIONS

#if defined(SVG_SUPPORT)
	SvgProperties*  svg;
#endif // SVG_SUPPORT

	WritingSystem::Script current_script;

	CurrentFontType	current_font_type;
	// Set if the current font is a generic font. If it isnt this flag can not be trusted.
	GenericFont		current_generic_font;

	short			max_paragraph_width;

	void			Reset(const HTMLayoutProperties *parent_hlp = NULL, HLDocProfile* hld_profile = NULL
#ifdef CURRENT_STYLE_SUPPORT
						  , BOOL use_types = FALSE
#endif // CURRENT_STYLE_SUPPORT
		);

		/** Sets the font and its size for VisualDevice.

		@param vd The VisualDevice to set font and its size for.
		@param font_number The number of the font to set.
		@param font_size The size of the font to set.
		@param honor_text_scale Should we adjust the font size according to current
			   text scale of the vd. */

	static void		SetFontAndSize(VisualDevice* vd, int font_number, int font_size, BOOL honor_text_scale);

	void			SetDisplayProperties(VisualDevice* vd) const;

	LayoutFixed		GetCalculatedLineHeightFixedPoint(VisualDevice* vd = NULL) const;

	LayoutCoord		GetCalculatedLineHeight(VisualDevice* vd = NULL) const
	{
		return LayoutCoord(LayoutFixedToIntNonNegative(GetCalculatedLineHeightFixedPoint(vd)));
	}

	void			SetTextMetrics(VisualDevice* vd);

	LayoutCoord		ResolvePercentageWidth(LayoutCoord base) const
	{
		OP_ASSERT(content_width < 0);
		return PercentageToLayoutCoord(LayoutFixed(-content_width), base);
	}

	LayoutCoord		ResolvePercentageHeight(LayoutCoord base) const
	{
		OP_ASSERT(GetHeightIsPercent());
		return PercentageToLayoutCoord(LayoutFixed(-content_height), base);
	}

	/** Get line dimensions to use in strict line height mode. */

	void			GetStrictLineDimensions(VisualDevice* vd, LayoutCoord& line_height, LayoutCoord& line_ascent) const
	{
		line_height = GetCalculatedLineHeight(vd);
		LayoutCoord line_leading = line_height - LayoutCoord(font_ascent + font_descent);

		/* Negative line half leading should be rounded towards -infinity.
		   See GetHalfLeading(). */
		if (line_leading < 0) line_leading--;
		line_ascent = LayoutCoord(font_ascent) + line_leading / LayoutCoord(2);
	}

	/** Get half leading for the current font and line height props.
		Half leading is defined as (line_height - (A+D)) / 2. Where
		D is font descent and A is font ascent.

		@param line_height The computed line height.
		@param ascent The font ascent including internal leading.
		@param descent The font descent.
		@return The calculated half leading.
			It is always rounded towards -infinity (can have half pixels).
			That is because, if the line height is smaller than the font height,
			we prefer that more of the font descent part fits in the linebox bounds
			over internal leading part. In case of the line height being greater than
			font height, we prefer that the glyph is positioned as close to the middle
			as possible, so round half leading down, as the glyph is already pushed
			down a but by internal leading. */

	static LayoutCoord
					GetHalfLeading(LayoutCoord line_height, LayoutCoord ascent, LayoutCoord descent)
	{
		LayoutCoord line_leading = line_height - (ascent + descent);
		return (line_leading < 0 ? line_leading - LayoutCoord(1) : line_leading) / LayoutCoord(2);
	}

	/** Get half leading from these props.

		Gets calculated line height and calls the overload with appropriate params.

		@param vd The visual device that should be used in the calculation.
		@return The calculated half leading.

		@overload static LayoutCoord HTMLayoutProperties::GetHalfLeading(LayoutCoord, LayoutCoord, LayoutCoord). */

	LayoutCoord		GetHalfLeading(VisualDevice* vd) const
	{
		return GetHalfLeading(GetCalculatedLineHeight(vd), LayoutCoord(font_ascent), LayoutCoord(font_descent));
	}

	/** Makes certain properties modifications that are special for
		a list item marker element. These are:
		- vertical-align, which is baseline always
		- inner margin (right), which is equal to the offset between
		the marker and the follow up line content
		- cancels small_caps and text_transform

		@param doc the document where the marker element belongs to. */

	void			SetListMarkerProps(FramesDocument *doc);

	OP_STATUS		SetCssPropertiesFromHtmlAttr(HTML_Element* element, const HTMLayoutProperties& parent_props, HLDocProfile* hld_profile);
	OP_STATUS		GetCssProperties(HTML_Element* element, LayoutProperties* parent_cascade, HLDocProfile* hld_profile, Container* container, BOOL ignore_transitions);
	void			SetERA_FontSizes(HTML_Element* element,
									 const HTMLayoutProperties& parent_props,
									 PrefsCollectionDisplay::RenderingModes rendering_mode,
									 PrefsCollectionDisplay::RenderingModes prev_rendering_mode,
									 int flexible_fonts,
									 int flex_font_scale);

	static CSS_decl*
					GetDeclFromPropertyItem(const CssPropertyItem &pi, CSS_decl *inherit_decl);

#ifdef CSS_TRANSFORMS
	void			SetTransformOrigin(const CssPropertyItem &pi, const HTMLayoutProperties &parent_props, const CSSLengthResolver& length_resolver);
#endif

	void			SetBorderRadius(CssPropertyItem* pi, const HTMLayoutProperties& parent_props, const CSSLengthResolver& length_resolver);

	void			SetCellVAlignFromTableColumn(HTML_Element* element, LayoutProperties* parent_cascade);

	/** Return TRUE if this is an element whose overflow properties control the root scrollbar behavior. */

	static BOOL		IsViewportMagicElement(HTML_Element* element, HLDocProfile* hld_profile);

	BOOL			IsShrinkToFit(Markup::Type element_type) const;

	BOOL			IsMultiColumn(Markup::Type element_type) const { return (column_count > 0 || column_width > 0 && !IsShrinkToFit(element_type)) && display_type != CSS_VALUE_flex && display_type != CSS_VALUE_inline_flex; }

	/** Return TRUE if an element of this type (and the properties of this object) need to create a SpaceManager. */

	BOOL			NeedsSpaceManager(Markup::Type element_type) const;

	/** Is the computed value of text-shadow different from 'none'? */
	BOOL			HasTextShadow() const { return !!text_shadows.GetCount(); }

	/** Is the computed value of box-shadow different from 'none'? */
	BOOL			HasBoxShadow() const { return !!box_shadows.GetCount(); }

	/** Return TRUE if box-shadow specified and any of the shadows uses inset keyword. */
	BOOL			HasInsetBoxShadow(VisualDevice* vd) const;

	/** Get the sum of non-percentage based left border and padding. */

	LayoutCoord		GetNonPercentLeftBorderPadding() const { return LayoutCoord(border.left.width) + (GetPaddingLeftIsPercent() ? LayoutCoord(0) : padding_left); }

	/** Get the sum of non-percentage based right border and padding. */

	LayoutCoord		GetNonPercentRightBorderPadding() const { return LayoutCoord(border.right.width) + (GetPaddingRightIsPercent() ? LayoutCoord(0) : padding_right); }

	/** Get the sum of non-percentage based left and right border and padding. */

	LayoutCoord		GetNonPercentHorizontalBorderPadding() const { return GetNonPercentLeftBorderPadding() + GetNonPercentRightBorderPadding(); }

	/** Get the sum of non-percentage based top border and padding. */

	LayoutCoord		GetNonPercentTopBorderPadding() const { return LayoutCoord(border.top.width) + (GetPaddingTopIsPercent() ? LayoutCoord(0) : padding_top); }

	/** Get the sum of non-percentage based bottom border and padding. */

	LayoutCoord		GetNonPercentBottomBorderPadding() const { return LayoutCoord(border.bottom.width) + (GetPaddingBottomIsPercent() ? LayoutCoord(0) : padding_bottom); }

	/** Get the sum of non-percentage based top and bottom border and padding. */

	LayoutCoord		GetNonPercentVerticalBorderPadding() const { return GetNonPercentTopBorderPadding() + GetNonPercentBottomBorderPadding(); }

	/** Helper function to get the left and top offset with regards
		to both left, top, right and bottom properties. */

	void			GetLeftAndTopOffset(LayoutCoord& left_offset, LayoutCoord& top_offset) const;

#ifdef SVG_SUPPORT
	/** Allocate svg properties.  This method is defined in the svg
	    module. Return FALSE on OOM, TRUE otherwise.
	 */
	static BOOL		AllocateSVGProps(SvgProperties *&current_svg_props, const SvgProperties *parent_svg_props);

	/** This method is defined in the svg module */
	OP_STATUS		SetSVGCssPropertiesFromHtmlAttr(HTML_Element* element, const HTMLayoutProperties& parent_props, HLDocProfile* hld_profile);

#endif // SVG_SUPPORT

#ifdef M2_SUPPORT
	void			SetOMFCssPropertiesFromHtmlAttr(HTML_Element* element, const HTMLayoutProperties& parent_props, HLDocProfile* hld_profile);
#endif // M2_SUPPORT

	static LayoutCoord
					GetClipValue(CSS_number4_decl* clip_value, int index, const CSSLengthResolver& length_resolver
#ifdef CURRENT_STYLE_SUPPORT
								 , short* use_type = NULL
#endif // CURRENT_STYLE_SUPPORT
								 );

#ifdef PAGED_MEDIA_SUPPORT

	/** Extract margin property values from @page rule.

		@param workplace The visual device to use
		@param page_props List of @page rule declarations.
		@param cb_width Containing block width
		@param cb_height Containing block height
		@param allow_width_and_height If TRUE, the 'width' and 'height' properties are honored.
		@param[out] margin_top Set to the value of margin-top if present; otherwise left untouched.
		@param[out] margin_right Set to the value of margin-right if present; otherwise left untouched.
		@param[out] margin_bottom Set to the value of margin-bottom if present; otherwise left untouched.
		@param[out] margin_left Set to the value of margin-left if present; otherwise left untouched. */

	static void		GetPageMargins(class LayoutWorkplace* workplace,
								   CSS_Properties* page_props,
								   LayoutCoord cb_width,
								   LayoutCoord cb_height,
								   BOOL allow_width_and_height,
								   LayoutCoord& margin_top,
								   LayoutCoord& margin_right,
								   LayoutCoord& margin_bottom,
								   LayoutCoord& margin_left);

	/** Is the value of the 'overflow' property some sort of '*paged*'? */

	static BOOL		IsPagedOverflowValue(short val)
	{
		return val == CSS_VALUE__o_paged_x_controls ||
			val == CSS_VALUE__o_paged_x ||
			val == CSS_VALUE__o_paged_y_controls ||
			val == CSS_VALUE__o_paged_y;
	}

#endif // PAGED_MEDIA_SUPPORT

	/** Find the container of an inline run-in.

		Will return the next sibling block. It is assumed that 'elm' is a valid inline run-in candidate. */

	static HTML_Element*
					FindInlineRunInContainingElm(const HTML_Element* elm);

	BOOL ExtractBorderImage(FramesDocument *doc, CSSLengthResolver& length_resolver, CSS_decl *decl, BorderImage &border_image, Border &border) const;

	/** Find the layout parent.

		Same as DocTree::Parent(), but takes run-ins into consideration. */

	static HTML_Element*
					FindLayoutParent(const HTML_Element* elm);

	/** Is 'parent' a layout ancestor of 'element'?

		Same as DocTree::IsAncestorOf(), but takes run-ins into consideration. */

	static BOOL		IsLayoutAncestorOf(const HTML_Element* parent, const HTML_Element* element);

#ifdef CSS_TRANSITIONS

	/** Return TRUE if a transition may be started for the element because of
		its computed styles for transition-* properties. */

	BOOL			MayStartTransition() const { return ((!transition_property ||
														  transition_property->GetDeclType() != CSS_DECL_TYPE ||
														  transition_property->TypeValue() != CSS_VALUE_none) &&
														 (transition_delay || transition_duration)); }
#endif // CSS_TRANSITIONS

#ifdef CURRENT_STYLE_SUPPORT
	HTMLayoutPropertiesTypes*
					CreatePropTypes();

					~HTMLayoutProperties() { OP_DELETE(types);
# ifdef SVG_SUPPORT
						OP_DELETE(svg);
# endif
					}
					HTMLayoutProperties()
						: types(NULL)
# ifdef SVG_SUPPORT
						,svg(NULL)
# endif
						,current_script(WritingSystem::Unknown) {}
#else // !CURRENT_STYLE_SUPPORT
					HTMLayoutProperties() :
# ifdef SVG_SUPPORT
										  svg(NULL),
# endif
										  current_script(WritingSystem::Unknown)
		{}

					~HTMLayoutProperties() {
# ifdef SVG_SUPPORT
						OP_DELETE(svg);
# endif
					}
#endif // !CURRENT_STYLE_SUPPORT

	/** Copy HTMLayoutProperties, return FALSE on OOM
	 */
	BOOL			Copy(const HTMLayoutProperties &other);

	void*			operator new(size_t size) OP_NOTHROW;
	void			operator delete(void* ptr, size_t size);

private:

	/** Wrapper for GetLengthInPixels. Used internally. */

	static PixelOrLayoutFixed
					GetLengthInPixelsFromProp(int value_type,
											  long value,
											  BOOL is_percentage,
											  BOOL is_decimal,
											  const CSSLengthResolver& length_resolver,
											  int min_value,
											  int max_value
#ifdef CURRENT_STYLE_SUPPORT
											  , short* use_type = NULL
#endif // CURRENT_STYLE_SUPPORT
											  );
};

short			CheckRealSizeDependentCSS(HTML_Element* element, FramesDocument* doc);

TableRowGroupBox* GetTableRowGroup(HTML_Element* elm);

#endif /* _LAYOUTPROPS_H_ */
