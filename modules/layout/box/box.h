/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/** @file box.h
 *
 * Box class prototypes for document layout.
 *
 * @author Geir Ivarsoy
 * @author Karl Anders Oygard
 *
 */

#ifndef _BOX_H_
#define _BOX_H_

class BlockBox;
class AbsolutePositionedBox;
class Container;
class InlineBox;
class Line;
class Content;
class TableContent;
class FlexContent;
class SelectContent;
class TableRowGroupBox;
class ButtonContainer;
class ScrollableArea;
class TextSelection;
class SelectionBoundaryPoint;
class FontFaceElm;
class SpaceManager;
class StackingContext;
#ifdef SVG_SUPPORT
class SVGContent;
#endif // SVG_SUPPORT
class LayoutWorkplace;
class DocRootProperties;
class InlineVerticalAlignment;
struct AbsoluteBoundingBox;
class TransformContext;
class MultiColBreakpoint;
class TraverseInfo;
class BulletContent;

#ifdef FONTSWITCHING
# include "modules/display/styl_man.h"
#endif
#include "modules/layout/layout_coord.h"
#include "modules/layout/cascade.h"
#include "modules/layout/cssprops.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/style/css_all_values.h"
#include "modules/style/css_gradient.h"
#include "modules/util/simset.h"

struct FontSupportInfo
{
	OpFontInfo*		desired_font;
	OpFontInfo*		current_font;
	int				current_block;
	UnicodePoint	block_lowest;
	UnicodePoint	block_highest;

					FontSupportInfo(int font_number);
};

int	GetTextFormat(const HTMLayoutProperties& props, const WordInfo& word_info);

/** Find the length of the first-letter fragment of text.

@return The length of the prefix from the start of "text" which includes the
			text formatted as ::first-letter. Returns 0 if there are no characters
			in "text" that would be styled as ::first-letter. */
unsigned int GetFirstLetterLength(const uni_char* text, int length);

/* Returns the UnicodePoint before the one (starting) at str+pos,
   taking surrogate pairs into consideration. */
UnicodePoint GetPreviousUnicodePoint(const uni_char* str, const size_t pos, int& consumed);

/** Helper class to draw box shadows, backgrounds and border. */

class BackgroundsAndBorders
{
public:

	/* Structure filled in by BackgroundsAndBorders::GetImageInformation() */
	struct ImageInfo
	{
		ImageInfo() : invalidate_old_position(FALSE), is_svg(FALSE), override_img_width(-1), override_img_height(-1),
					  replaced_url(FALSE) {}

		Image img;
		HEListElm *helm;
		BOOL invalidate_old_position;
		BOOL is_svg;
		int override_img_width;
		int override_img_height;
		BOOL replaced_url;
	};

	/** Create a backgrounds and border helper.  Document and visual
		is mandatory.

		`cascade' and 'border' are optional. */

						BackgroundsAndBorders(FramesDocument *d, VisualDevice *vd, LayoutProperties *cascade, const Border *border);

	/** Return TRUE if there is no border to cause any visual effect. */

	BOOL				IsBorderAllTransparent() const { return computed_border.IsEmpty() && !border_image.HasBorderImage(); }

	/** Paint backgrounds.

	   @param current_color the current value of 'color'. This is used when
							painting gradients with currentColor in a stop.
							'background-color: currentColor' receives special
							treatment in GetCssProperties instead.

	   @param shadows Paint shadows to go with the
					  background. Optional and can be NULL. This is
					  currently tied to the background, so you cannot
					  get shadows without painting the background.

	   @param border_box The border box of the element. */

	void				PaintBackgrounds(HTML_Element* element, COLORREF bg_color, COLORREF current_color,
										 const BgImages& bg_images, const Shadows *shadows,
										 const OpRect &border_box);

	/** Paint borders.

		@param border Borders to paint
		@param border_box Where to paint the borders relative to the current translation
		@param flush_pending_backgrounds Flush backgrounds in the avoid overdraw stack before painting borders
		@param current_color the current value of 'color'. @see PaintBackgrounds.
	 */

	void				PaintBorders(HTML_Element *element, const OpRect &border_box, COLORREF current_color);

	/** Paint backgrounds and borders for an inline element. Inline
		elements are treated differently in several ways.

		Currently it controls the `expand_image_position' flag that is
		sent to SetImagePosition().  For inlines, we may call
		SetImagePosition several times for the same element, each time
		building up the total area of the inline element.

		@param first_area TRUE if this is the first area of the inline element

		@param virtual_width The width of the inline content in total (all lines)

		@param virtual_position The current x position relative to the virtual
		width, also call virtual position.  Ignored when the background
		is attached to the viewport (background-attachment: fixed). */

	void				SetInline(BOOL first_area, LayoutCoord virtual_width, LayoutCoord virtual_position)
		{ if (!first_area) expand_image_position = TRUE; inline_virtual_width = virtual_width; inline_virtual_position = virtual_position; }

	/** When painting the background element, the position and size of
		background and border images is recorded in the document (in
		the corresponding HEListElm).  When this flag is set, the
		position and size is _expanded_ to cover this area, instead of
		replacing the area (the default behavior). */

	void				SetExpandImagePosition() { expand_image_position = TRUE; }

	/** Normally the border-box (along with other background
		properties) is used for positioning the background image, but
		in certain situations (the doc root) we want to position the
		background image relative to a specific rectange without
		changing the area in which the background is painted.

		The positioning rect is relative to the current translation */

	void				OverridePositioningRect(const OpRect &pos_rect) { override_positioning_rect = pos_rect; has_positioning_area_override = TRUE; }

	void				OverrideProperties(const DocRootProperties &doc_root_props);

	void				SetScrollOffset(LayoutCoord x, LayoutCoord y) { scroll_x = x; scroll_y = y; }

	/** Reset visibility of associated background images of
		'element'. This involves letting the document code know that
		the background images no longer are visible. */

	static void			ResetBgImagesVisibility(FramesDocument* doc, HTML_Element *element);

	/** The element that represents the viewport root changed. This
		means that background images may have changed visibility.
		This function will report this to the document.

		@param bg_element_type The new background element
		@param doc_root The logical root element of the document
		@param body_elm The element representing the document body. May be the same as the doc_root in xml documents. */

	static void			HandleRootBackgroundElementChanged(FramesDocument* doc, Markup::Type bg_element_type, HTML_Element* doc_root, HTML_Element* body_elm);

protected:

	/** Use the cascade to adjust the backgrounds and borders.

		One example is for fieldset element, if there is a legend
		present. Then we draw the border in a way that the text is
		embedded in the border.

		@param cascade The tip of the cascade to extract values from */

	void				UseCascadedProperties(LayoutProperties *cascade);

	/** Handle box shadows.

		The 'inset' parameter controlls whether to draw outer or inner
		shadows. The shadows are drawn relative to the border box in
		the outer case and relative to the padding box for the inner
		case.

		@param shadows The list of shadows
		@param border_box A rectangle corresponding to the border box
		@param bg_info Used for border information
		@param with_inset TRUE means inner shadows. FALSE means outer shadows. */

	void				HandleBoxShadows(const Shadows *shadows, const OpRect &border_box,
										 BG_OUT_INFO *bg_info, BOOL with_inset);

	/** Paint background color */

	void				PaintBackgroundColor(COLORREF bg_color,
											 const BgImages &bg_images,
											 BG_OUT_INFO &bg_info,
											 const OpRect &border_box);

#ifdef CSS_GRADIENT_SUPPORT
	void				PaintBackgroundGradient(HTML_Element *elm,
											 const BG_OUT_INFO &bg_info,
											 COLORREF current_color,
											 const OpRect &border_box,
											 const CSS_Gradient &gradient);
#endif // CSS_GRADIENT_SUPPORT

	/** Paint all background images. */

	void				PaintBackgroundImages(const BgImages& bg_images, BG_OUT_INFO &bg_info,
											  COLORREF current_color,
											  const OpRect &border_box, HTML_Element *element);

	/** Check whether a background should be painted at all */

	BOOL				SkipBackground(HTML_Element *element);

	/** Check whether a background color should be painted for a given
		element */

	BOOL				SkipBackgroundColor(const BG_OUT_INFO &bg_info, const BgImages &bg_images, HTML_Element *element) const;

	/* Set up basic information in a BG_OUT_INFO structure, for
	   passing along to the VisualDevice layer during painting. */

	void				InitializeBgInfo(BG_OUT_INFO &bg_info, HTML_Element *element, Border &used_border);

#ifdef SVG_SUPPORT
	/** Get the Image for SVG documents.

		When SVG is enabled and allowed in backgrounds, this renders
		the background on-the-fly to the correct resolution and saves
		it in an Image that can be used to paint the background. */

	BOOL				GetSVGImage(URL bg_url, UrlImageContentProvider *conprov,
									const OpRect &border_box, Image &img,
									BOOL &invalidate_old_position,
									int &override_doc_width, int& override_doc_height);
#endif // SVG_SUPPORT

	/** Get the image information that PaintImage needs for painting.

		@param element The element we paint an image for.
		@param bg_img The URL of the image to paint.
		@param border_box The border box of the image. Used for generating SVG images on the fly.
		@param inline_type Whether it is the border image or the background image we are fetching information for
		@param bg_idx The background image index, only used for background images.
		@param image_info Where the image information is stored

		Returns FALSE when the image information isn't availible. */

	BOOL				GetImageInformation(HTML_Element *element, const CssURL &bg_img, const OpRect &border_box,
											InlineResourceType inline_type, int bg_idx, ImageInfo &image_info);

	/** Paint an individual background image. */

	void				PaintImage(Image img, HTML_Element *element,
								   HEListElm *helm, BG_OUT_INFO &bg_info,
								   const OpRect &border_box, BOOL is_svg,
								   int override_img_width, int override_img_height,
								   BOOL replaced_url);

	/** Sets up the background positioning rectangle according to background-origin.

		@param border_box The border edges of this box
		@param background Background properties
		@param border Border properties
		@param positioning_rect The positioning_rect to compute */

	void				ComputeBackgroundOrigin(const OpRect &border_box, const BackgroundProperties *background, const Border *border,
												OpRect &positioning_rect) const;

	/**
	 * Compute the spacing for 'background-repeat: space'.
	 *
	 * @param repeat_x              the value of background-repeat.
	 * @param positioning           the background positioning area.
	 * @param background_pos[inout] the calculated background-position. X and Y will be modified if
	 *                              there is only space for one image.
	 * @param img                   the image dimensions.
	 * @param space[out]            the calculated space between images.
	 *
	 * @return TRUE if there is room for at least one instance of the image. */

	static BOOL			CalculateRepeatSpacing(const BackgroundProperties *bg, const OpRect &positioning, OpPoint &background_pos, const OpRect &img, OpRect &space);

	/** Compute the drawing area as defined by background-clip.

		@param border_box The border edges of this box
		@param background Background properties
		@param border Border properties
		@param draw_rect (out) The computed drawing area is stored here */

	void				ComputeDrawingArea(const OpRect &border_box,
										   const BackgroundProperties *background,
										   const Border *border,
										   OpRect &draw_rect);

	/** Clip rect to the background-repeat value. */

	void				ClipDrawingArea(const OpRect &clip_rect,
										const OpPoint &background_pos,
										BOOL image_size_known,
										const BackgroundProperties *background,
										const Border *border,
										int img_width, int img_height,
										double img_scale_x, double img_scale_y,
										OpRect &draw_rect);

	/** Override the positioning area for fixed positioned backgrounds.

		@param background Background properties
		@param positioning_rect The positioning_rect */

	void				HandleBackgroundAttachment(const BackgroundProperties *background, OpRect &positioning_rect);

	void				AdjustScaleForRenderingMode(HTML_Element *element, double &img_scale_x, double &img_scale_y);

	BOOL				HandleSkinElement(const OpRect &border_box, const CssURL &bg_img);

	/** Compute the position of the background using the virtual rect
		(usually the border_box but with inlines it may be different).

		@param background Background properties
		@param positioning_rect The rect to use for positioning
		@param img_width The width of the image
		@param img_height The height of the image
		@param img_scale_x The scale of the image x-wise
		@param img_scale_y The sacle of the image y-wise
		@param background_pos The calculated background position */

	static void			ComputeImagePosition(const BackgroundProperties *background,
											 const OpRect &positioning_rect,
											 int img_width, int img_height,
											 int img_scale_x, int img_scale_y, OpPoint &background_pos);

	/** Computes the scale at which to render a background image. Takes 'background-size' and
		'background-repeat: round' into account.

		@param positioning_rect   the background positioning area.
		@param[inout] img_scale_x the percentage by which to scale horizontally. Note that this may be returned as 0, in which case the image should not be displayed.
		@param[inout] img_scale_y the percentage by which to scale vertically. Note that this may be returned as 0, in which case the image should not be displayed.
	*/
	static void			ComputeImageScale(const BackgroundProperties *background,
										  const OpRect &positioning_rect,
										  int img_width, int img_height,
										  double &img_scale_x, double &img_scale_y);

	static OpRect		GetPaddingBox(const Border *border, OpRect border_box);

	/** Set image position in document */

	void				SetDocumentPosition(HTML_Element* element, HEListElm *helm, const OpRect &image) const;

	/* For use in communication with the rest of core */

	FramesDocument *doc;
	VisualDevice *vis_dev;

	/* Local copy of the border structure so that we can assume that
	   the border pointer always points somewhere.

	   Note that this border may not be sent to the VisualDevice since
	   it may still contain percentage values. See
	   Border::GetUsedBorder(). */

	Border computed_border;

	/* Border image */

	BorderImage border_image;

	/* Flag used to expand the image position instead of replacing it.

	   Inline elements may be drawn in pieces and all but the first
	   should have this set.  Table rows are handled similarly.  */

	BOOL expand_image_position;

	/* The virtual position and width of an inline. */

	LayoutCoord inline_virtual_width;
	LayoutCoord inline_virtual_position;

	/* Part of the fieldset/legend fancy border feature. See
	   UseCascadedProperties() */

	LayoutCoord extra_top_margin;
	LayoutCoord top_gap_x;
	LayoutCoord top_gap_width;

	/* The positioning area can be overridden for the cases where it
	   doesn't match the background painting area. */

	BOOL has_positioning_area_override;
	OpRect override_positioning_rect;

	/* Properties copied from the cascade */
	COLORREF font_color;
	LayoutCoord padding_top;
	LayoutCoord padding_left;
	LayoutCoord padding_right;
	LayoutCoord padding_bottom;
	short box_decoration_break;
	short image_rendering;

	/* For use in background-attachement: local */
	LayoutCoord scroll_x;
	LayoutCoord scroll_y;

	/* Backgrounds and borders are linked together in the avoid overdraw optimization.
	   If a background is painted and it's area covers the entire border, avoid
	   overdraw may choose to clip away the part where the border will be painted from
	   the background and that way we don't need to flush the backgrounds before painting
	   the borders.

	   Note! If the borders are for example transparent or non-solid, or the document is
	   scaled, avoid overdraw will not clip the borders, but flushing the background will
	   be done when the background color is painted. In those cases the value of this
	   flag is irrelevant.*/

	BOOL border_area_covered_by_background;

	/** For CSS lengths computations. */

	CSSLengthResolver	css_length_resolver;
};

/** Vertical margin state. Used for margin collapsing. */

struct VerticalMargin
{
	VerticalMargin()
		: max_positive(0),
		  max_negative(0),
		  max_default(0),
		  max_positive_nonpercent(0),
		  max_negative_nonpercent(0),
		  max_default_nonpercent(0) { }

	VerticalMargin(LayoutCoord max_positive,
				   LayoutCoord max_negative,
				   LayoutCoord max_default,
				   LayoutCoord max_positive_nonpercent,
				   LayoutCoord max_negative_nonpercent,
				   LayoutCoord max_default_nonpercent)
		: max_positive(max_positive),
		  max_negative(max_negative),
		  max_default(max_default),
		  max_positive_nonpercent(max_positive_nonpercent),
		  max_negative_nonpercent(max_negative_nonpercent),
		  max_default_nonpercent(max_default_nonpercent) { }

	/** Reset margin state. */

	void Reset() { max_positive = max_negative = max_default = max_positive_nonpercent = max_negative_nonpercent = max_default_nonpercent = LayoutCoord(0); }

	/** Stop margin collapsing.

		Any margins added from now on will be eaten and ignored, and GetHeight() will return 0. */

	void Ignore() { max_positive = max_negative = max_positive_nonpercent = max_negative_nonpercent = LAYOUT_COORD_MAX; }

	/** Collapse this margin with the other margin and return difference. */

	void Collapse(const VerticalMargin& other, LayoutCoord* y_offset = 0, LayoutCoord* y_offset_nonpercent = 0);

	/** Get collapsed margin height. */

	LayoutCoord GetHeight() const { return max_positive - max_negative; }

	/** Get collapsed margin height not resolved from percentage values. */

	LayoutCoord GetHeightNonPercent() const { return max_positive_nonpercent - max_negative_nonpercent; }

	/** Set margin based on the margin-top property. */

	void CollapseWithTopMargin(const HTMLayoutProperties& props, BOOL ignore_default = FALSE);

	/** Set margin based on the margin-bottom property. */

	void CollapseWithBottomMargin(const HTMLayoutProperties& props, BOOL ignore_default = FALSE);

	/** Apply default margin, so that it participates in margin collapsing. */

	void ApplyDefaultMargin();

	/** Largest positive margin. */

	LayoutCoord max_positive;

	/** Largest negative margin. */

	LayoutCoord max_negative;

	/** Largest default margin. Used in quirks mode. */

	LayoutCoord max_default;

	/** Largest positive margin that was not resolved from a percentage value. */

	LayoutCoord max_positive_nonpercent;

	/** Largest negative margin that was not resolved from a percentage value. */

	LayoutCoord max_negative_nonpercent;

	/** Largest default margin that was not resolved from a percentage value. Used in quirks mode. */

	LayoutCoord max_default_nonpercent;
};

BOOL	GetNextTextFragment(const uni_char*& txt,
							int length,
							UnicodePoint& previous_character,
							CharacterClass& last_base_character_class,
							BOOL leading_whitespace,
							WordInfo& word_info,
							CSSValue white_space,
							BOOL stop_at_whitespace_separated_words,
							BOOL treat_nbsp_as_space,
							FontSupportInfo& font_info,
							FramesDocument* doc,
							WritingSystem::Script script);

BOOL	GetNextTextFragment(const uni_char*& txt,
							int length,
							UnicodePoint& previous_character,
							BOOL leading_whitespace,
							WordInfo& word_info,
							CSSValue white_space,
							BOOL stop_at_whitespace_separated_words,
							BOOL treat_nbsp_as_space,
							FontSupportInfo& font_info,
							FramesDocument* doc,
							WritingSystem::Script script);

BOOL	GetNextTextFragment(const uni_char*& txt, int length,
							WordInfo& word_info,
							CSSValue white_space,
							BOOL stop_at_whitespace_separated_words,
							BOOL treat_nbsp_as_space,
							FontSupportInfo& font_info,
							FramesDocument* doc,
							WritingSystem::Script script);

/** Current mode of pagination.

	Any of these are valid initial values when starting a reflow, but only the
	following transitions are allowed _during_ reflow:

	PAGEBREAK_ON -> PAGEBREAK_OFF
	PAGEBREAK_FIND -> PAGEBREAK_ON

	At the end of a reflow pass, mode can be anything but PAGEBREAK_FIND (which
	would be a failure). */

enum PM_MODE
{
	/** Page breaking disabled.

		This is always the case in non-paged media. In paged media it also
		used; when a pending page break has been inserted, and we just want to
		do regular layout for the rest of the reflow pass. */

	PAGEBREAK_OFF,

	/** Page breaking in progress.

		Will insert explicit page breaks where requested, and at most one
		pending page break, if needed. If the elements reflowed while in this
		mode have page breaks from earlier reflow passes, all will be
		removed. If a pending page break is inserted, we switch to
		PAGEBREAK_OFF for the rest of the reflow pass. */

	PAGEBREAK_ON,

	/** Looking for pending page break.

		Layout will search until it finds the pending page break inserted in
		the previous reflow pass (there should be only once such break at any
		given time), and replace it with a proper implicit page break, and then
		switch to PAGEBREAK_ON. Until it finds this pending page break, it will
		just keep previously inserted page breaks (i.e. add new pages). */

	PAGEBREAK_FIND,

	/** Keep previously inserted page breaks. Don't insert any new ones.

		In this mode we will not attempt to insert new page breaks (neither
		implicit or explicit), but keep all previously inserted page breaks
		(i.e. add new pages). This is essentially a state very similar to
		PAGEBREAK_FIND, only that it's not looking for a pending page break.
		We will never change from this to any other state during reflow. */

	PAGEBREAK_KEEP
};

enum ColumnStretch
{
	COLUMN_STRETCH_NORMAL = 0,
	COLUMN_STRETCH_MAXIMIZE	= 1,
	COLUMN_STRETCH_CENTER = 2
};

enum TableStrategy
{
	TABLE_STRATEGY_SINGLE_COLUMN = 0,
	TABLE_STRATEGY_TRUE_TABLE = 1,
	TABLE_STRATEGY_NORMAL = 2
};

enum FlexibleReplacedWidth
{
	FLEXIBLE_REPLACED_WIDTH_NO = 0,
	FLEXIBLE_REPLACED_WIDTH_SCALE = 1,
	FLEXIBLE_REPLACED_WIDTH_SCALE_MIN = 2
};

struct LayoutInfo
{
	LayoutWorkplace*
					workplace;
	FramesDocument*	doc;
	VisualDevice*	visual_device;
	HLDocProfile*	hld_profile;
#ifdef LAYOUT_YIELD_REFLOW
	BOOL			force_layout_changed;
#endif
#ifdef PAGED_MEDIA_SUPPORT
	PM_MODE			paged_media;
	BOOL			pending_pagebreak_found;
#endif
	HTML_Element*	stop_before;
	CSS_MediaType	media_type;
	TextSelection*	text_selection;
	BOOL			start_selection_done;

	/** Is this reflow pass known to be triggered by something external?

		This is set when a reflow pass is caused by content or style
		changes, or by layout viewport size changes. */

	BOOL			external_layout_change;

	ColumnStretch	column_stretch;
	TableStrategy	table_strategy;
	BOOL			flexible_columns;
	FlexibleReplacedWidth
					flexible_width;
	BOOL			treat_nbsp_as_space;
	BOOL			allow_word_split;
#ifdef CONTENT_MAGIC
	BOOL			content_magic;
#endif // CONTENT_MAGIC
	BOOL			using_flexroot;

	/*	Counter to make sure we are not asking for gettimeofday too often */
	short			yield_count;

	LayoutCoord		translation_x;
	LayoutCoord		root_translation_x;
	LayoutCoord		fixed_pos_x;
	LayoutCoord		inline_translation_x;
	LayoutCoord		nonpos_scroll_x; ///< X scroll amount that doesn't affect abspos descendants.

	LayoutCoord		translation_y;
	LayoutCoord		root_translation_y;
	LayoutCoord		fixed_pos_y;
	LayoutCoord		inline_translation_y;
	LayoutCoord		nonpos_scroll_y; ///< Y scroll amount that doesn't affect abspos descendants.

	/** Quote level. Used to decide which quote type to use for
		open-quote/close-quote in generated content. */
	unsigned int	quote_level;

					LayoutInfo(LayoutWorkplace* wp);

	/** Get root translation relative to the document root or the
		nearest transformed element */
	void			GetRootTranslation(LayoutCoord& x, LayoutCoord& y) const { x = root_translation_x; y = root_translation_y; }

	/** Get root translation relative to the current translation */
	void			GetRelativeRootTranslation(LayoutCoord &x, LayoutCoord &y) { x = root_translation_x - translation_x; y = root_translation_y - translation_y; }
	/** Set root translation.  Root translation is the translation to
		the containing block for absolutly positioned boxes.  */
	void			SetRootTranslation(LayoutCoord x, LayoutCoord y) { root_translation_x = x; root_translation_y = y; }

	/** Translate root by specified amount.  @see SetRootTranslation */
	void			TranslateRoot(LayoutCoord x, LayoutCoord y) { root_translation_x += x; root_translation_y += y; }

	/** Get translation on the x-axis from the document root or the
		nearest transformed element, if any. */
	LayoutCoord		GetTranslationX() const { return translation_x; }

	/** Get translation on the y-axis from the document root or the
		nearest transformed element, if any. */
	LayoutCoord		GetTranslationY() const { return translation_y; }

	/** Get translation from the document root or the nearest
		transformed element, if any. */
	void			GetTranslation(LayoutCoord& x, LayoutCoord& y) const { x = translation_x; y = translation_y; }

	/** Set translation */
	void			SetTranslation(LayoutCoord x, LayoutCoord y) { translation_x = x; translation_y = y; }

	/** Set fixed positioned offset */
	void			SetFixedPos(LayoutCoord x, LayoutCoord y) { fixed_pos_x = x; fixed_pos_y = y; }

	/** Get fixed positioned offset */
	void			GetFixedPos(LayoutCoord &x, LayoutCoord &y) const { x = fixed_pos_x; y = fixed_pos_y; }

	LayoutCoord		GetFixedPositionedX() const { return fixed_pos_x; }

	LayoutCoord		GetFixedPositionedY() const { return fixed_pos_y; }

	/** Add translation. */
	void			Translate(LayoutCoord x, LayoutCoord y) { visual_device->Translate(x, y); translation_x += x; translation_y += y; }

	/** Retrieve current inline translation. */
	void			GetInlineTranslation(LayoutCoord& x, LayoutCoord& y) { x = inline_translation_x; y = inline_translation_y; }

	/** Set the current inline translation. Untranslate the inline_translation that was already set. */
	void			SetInlineTranslation(LayoutCoord x, LayoutCoord y) { Translate(x - inline_translation_x, y - inline_translation_y); inline_translation_x = x; inline_translation_y = y; }

	/** Increase quote level. */
	void			IncQuoteLevel() { quote_level++; }

	/** Decrease quote level.
		@return TRUE if the quote level was decreased, FALSE if already 0. */
	BOOL			DecQuoteLevel() { if (quote_level > 0) { --quote_level; return TRUE; } else return FALSE; }

	/** @return The current quote level. */
	unsigned int	GetQuoteLevel() { return quote_level; }

#ifdef PAGED_MEDIA_SUPPORT
	/** Return TRUE if we should keep previously inserted page breaks. */
	BOOL			KeepPageBreaks() { return paged_media == PAGEBREAK_FIND || paged_media == PAGEBREAK_KEEP; }
#endif // PAGED_MEDIA_SUPPORT
};

enum LAYST
{
	LAYOUT_STOP = 0,
	LAYOUT_CONTINUE = 1,
	LAYOUT_END_FIRST_LINE = 2,
	LAYOUT_OUT_OF_MEMORY = 3,
	LAYOUT_YIELD = 4
};

/** Return TRUE if the specified break policy means a forced break. */

inline BOOL BreakForced(BREAK_POLICY policy)
{
	return policy == BREAK_ALWAYS || policy == BREAK_LEFT || policy == BREAK_RIGHT;
}

/** Return TRUE if the two adjoining specified break policies constitute a forced break. */

inline BOOL BreakForcedBetween(BREAK_POLICY policy1, BREAK_POLICY policy2)
{
	return BreakForced(policy1) || BreakForced(policy2);
}

/** Return TRUE if the two adjoining specified break policies allow (or force) breaking. */

inline BOOL BreakAllowedBetween(BREAK_POLICY policy1, BREAK_POLICY policy2)
{
	if (BreakForcedBetween(policy1, policy2))
		return TRUE;
	else
		if (policy1 == BREAK_AVOID || policy2 == BREAK_AVOID)
			return FALSE;
		else
			return TRUE;
}

/** Combine two break policies (decomposed from break-before and/or break-after) into one.

	When two break properties meet at a potential break location, the CSS 2.1
	spec loosely defines which of them "wins". With the additional property
	values defined by the multicol spec, it gets a bit more complicated, in
	that combining value A and B may not necessarily result in either A or B,
	but in a new value (e.g. if A says 'page' and B says 'column', the result
	should be 'always'). No spec defines exactly how to do this, but here's an
	attempt at applying common sense. */

inline BREAK_POLICY
CombineBreakPolicies(BREAK_POLICY policy1, BREAK_POLICY policy2)
{
	/* Note: The spec doesn't say which of 'left' and 'right' should win. So we
	   just do whatever. */

	if (policy1 == BREAK_LEFT || policy2 == BREAK_LEFT)
		return BREAK_LEFT;

	if (policy1 == BREAK_RIGHT || policy2 == BREAK_RIGHT)
		return BREAK_RIGHT;

	if (policy1 == BREAK_ALWAYS || policy2 == BREAK_ALWAYS)
		return BREAK_ALWAYS;

	if (policy1 == BREAK_AVOID || policy2 == BREAK_AVOID)
		return BREAK_AVOID;

	return BREAK_ALLOW;
}

/** Calculate a page and column breaking policies based on the specified CSS property value. */

inline void
CssValueToBreakPolicy(CSSValue cssval, BREAK_POLICY& page, BREAK_POLICY& column)
{
	switch (cssval)
	{
	default:
		OP_ASSERT(!"Unhandled value");
	case CSS_VALUE_auto:
		page = column = BREAK_ALLOW;
		break;
	case CSS_VALUE_always:
		page = BREAK_ALWAYS;
		column = BREAK_ALLOW;
		break;
	case CSS_VALUE_left:
		page = BREAK_LEFT;
		column = BREAK_ALLOW;
		break;
	case CSS_VALUE_right:
		page = BREAK_RIGHT;
		column = BREAK_ALLOW;
		break;
	case CSS_VALUE_avoid:
		page = column = BREAK_AVOID;
		break;
	case CSS_VALUE_page:
		page = BREAK_ALWAYS;
		column = BREAK_ALLOW;
		break;
	case CSS_VALUE_column:
		page = BREAK_ALLOW;
		column = BREAK_ALWAYS;
		break;
	case CSS_VALUE_avoid_page:
		page = BREAK_AVOID;
		column = BREAK_ALLOW;
		break;
	case CSS_VALUE_avoid_column:
		page = BREAK_ALLOW;
		column = BREAK_AVOID;
		break;
	}
}

/** Set breaking policies based on the value of the 'break-before' and 'break-after' properties. */

void
CssValuesToBreakPolicies(const LayoutInfo& info,
						 LayoutProperties* cascade,
						 BREAK_POLICY& page_break_before,
						 BREAK_POLICY& page_break_after,
						 BREAK_POLICY& column_break_before,
						 BREAK_POLICY& column_break_after);

#ifdef PAGED_MEDIA_SUPPORT

enum BREAKST
{
	BREAK_FOUND = 0,
	BREAK_NOT_FOUND = 1,
	BREAK_KEEP_LOOKING = 2,
	BREAK_OUT_OF_MEMORY = 3
};

#endif // PAGED_MEDIA_SUPPORT

/** RelativeBoundingBox */

struct RelativeBoundingBox
{
protected:

	/** Left overflow */

	LayoutCoord		left;

	/** Right overflow */

	LayoutCoord		right;

	/** Top overflow */

	LayoutCoord		top;

	/** Bottom overflow */

	LayoutCoord		bottom;

	/** Content right overflow */

	LayoutCoord		content_right;

	/** Content bottom overflow */

	LayoutCoord		content_bottom;

public:
					RelativeBoundingBox()
					  : left(0),
						right(0),
						top(0),
						bottom(0),
						content_right(0),
						content_bottom(0) { }

	/** Compare a RelativeBoundingBox with this one. Return TRUE if they are equal, FALSE otherwise. */

	BOOL			operator ==(const RelativeBoundingBox& X) const { return left == X.left && right == X.right && top == X.top && bottom == X.bottom; }

	/** Compare a RelativeBoundingBox with this one. Return TRUE if they not are equal, FALSE otherwise. */

	BOOL			operator !=(const RelativeBoundingBox& X) const { return left != X.left || right != X.right || top != X.top || bottom != X.bottom; }

	/** Check if the bounding box extends in any direction outside the box it is attached to. */

	BOOL			IsEmpty() const { return !left && !right && !top && !bottom; }

	/** Union 'other' bounding box with this one.

		@param other The bounding box to be added
		@param box_width Width of the box that owns this bounding box
		@param box_height Height of the box that owns this bounding box
		@param skip_content do not update content overflow - used when content comes from abspos box and we're not right containing element */

	void			UnionWith(const AbsoluteBoundingBox &other, LayoutCoord box_width, LayoutCoord box_height, BOOL skip_content);

	/** Reset the bounding box to a specific value. */

	void			Reset(LayoutCoord value, LayoutCoord content_value);

	/** Reduce right and bottom part of relative bounding box. */

	void			Reduce(LayoutCoord b, LayoutCoord r, LayoutCoord min_value);

	/** Return the total height of the bounding box based on the height of the box this bounding box is attached to. */

	LayoutCoord		TotalHeight(LayoutCoord content_height) const { return TopIsInfinite() || BottomIsInfinite() ? LAYOUT_COORD_MAX : content_height + top + bottom; }

	/** Return the total width of the bounding box based on the width of the box this bounding box is attached to. */

	LayoutCoord		TotalWidth(LayoutCoord content_width) const { return LeftIsInfinite() || RightIsInfinite() ? LAYOUT_COORD_MAX : content_width + left + right; }

	/** Return the total content height of the bounding box based on the height of the box this bounding box is attached to. */

	LayoutCoord		TotalContentHeight(LayoutCoord content_height) const { return TopIsInfinite() || BottomIsInfinite() ? LAYOUT_COORD_MAX : content_height + top + content_bottom; }

	/** Return the total content width of the bounding box based on the width of the box this bounding box is attached to. */

	LayoutCoord		TotalContentWidth(LayoutCoord content_width) const { return LeftIsInfinite() || RightIsInfinite() ? LAYOUT_COORD_MAX : content_width + left + content_right; }

	/** Return how much the bounding box extends to the left of the box it is attached to. */

	LayoutCoord		Left() const { return left; }

	/** Return how much the bounding box extends to the right of the box it is attached to. */

	LayoutCoord		Right() const { return right; }

	/** Return how much the bounding box extends to the top of the box it is attached to. */

	LayoutCoord		Top() const { return top; }

	/** Return how much the bounding box extends to the bottom of the box it is attached to. */

	LayoutCoord		Bottom() const { return bottom; }

	/** Return how much the content extends to the right of the box it is attached to. */

	LayoutCoord		ContentRight() const { return content_right; }

	/** Return how much the content extends to the bottom of the box it is attached to. */

	LayoutCoord		ContentBottom() const { return content_bottom; }

	/** Check if the bounding box extends an infinite amount to the left. */

	BOOL			LeftIsInfinite() const { return left == LAYOUT_COORD_MAX; }

	/** Check if the bounding box extends an infinite amount to the right. */

	BOOL			RightIsInfinite() const { return right == LAYOUT_COORD_MAX; }

	/** Check if the bounding box extends an infinite amount to the top. */

	BOOL			TopIsInfinite() const { return top == LAYOUT_COORD_MAX; }

	/** Check if the bounding box extends an infinite amount to the bottom. */

	BOOL			BottomIsInfinite() const { return bottom == LAYOUT_COORD_MAX; }

	/** Invalidate the area of this bounding box.

		The visual device is normally translated to the point of
		origin of the container of the box that owns this bounding
		box.

		@param visual_device The visual device to invalidate
		@param x The x offset of the invalidation
		@param y The y offset of the invalidation
		@param box_width Width of the box that owns this bounding box
		@param box_height Height of the box that owns this bounding box. */

	void			Invalidate(VisualDevice *visual_device, LayoutCoord x, LayoutCoord y, LayoutCoord box_width, LayoutCoord box_height) const {
		visual_device->UpdateRelative(x - left, y - top, TotalWidth(box_width), TotalHeight(box_height), TRUE);
	}
};

/** AbsoluteBoundingBox - bounding box enclosing box and all it's visible descendants.
	Relative to parent element top left border edge */

struct AbsoluteBoundingBox
{
protected:

	/** Left offset of bounding box */

	LayoutCoord		x;

	/** Bounding box width - may include boxes of absolutely positioned descendants */

	LayoutCoord		width;

	/** Content's bounding box width. This bounding box encloses the box itself and content
		bounding boxes of visible descendants (except positioned descendants, unless they
		are contained by this block). It is stored as relative to the main bounding box's
		left offset and is used to calculate scrollbars. */

	LayoutCoord		content_width;

	/** Top offset of bounding box */

	LayoutCoord		y;

	/** Bounding box height - may include boxes of absolutely positioned descendants */

	LayoutCoord		height;

	/** Content's bounding box height. This bounding box encloses the box itself and content
		bounding boxes of visible descendants (except positioned descendants, unless they
		are contained by this block). It is stored as relative to the main bounding box's
		top offset and is used to calculate scrollbars. */

	LayoutCoord		content_height;

public:

					AbsoluteBoundingBox()
					  : x(0),
					    width(0),
						content_width(0),
						y(0),
						height(0),
						content_height(0) {}

	/** Calculate the absolute bounding box.
	 *
	 * @param box Will be used to set the absolute bounding box
	 * @param box_width Width of the box that owns this bounding box
	 * @param box_height Height of the box that owns this bounding box
	 */

	void			Set(const RelativeBoundingBox& box, LayoutCoord box_width, LayoutCoord box_height);

	/** Sets new bounding box from given parameters */

	void			Set(LayoutCoord new_x, LayoutCoord new_y, LayoutCoord new_width, LayoutCoord new_height);

	/** Sets new content box size from given parameters - width and height should be
	 * relative to bounding box left/top edge.
	 */

	void			SetContentSize(LayoutCoord new_content_width, LayoutCoord new_content_height) { content_width = new_content_width; content_height = new_content_height; }

	/** Grows bounding box to given max size. Will grow content box as well */

	void			Grow(LayoutCoord max_width, LayoutCoord max_height);

	/** Translates bounding box by given offset */

	void			Translate(LayoutCoord offset_x, LayoutCoord offset_y);

	/** Union this bounding box with another one. */

	void			UnionWith(const AbsoluteBoundingBox& other);

	/** Is the bounding box empty? */

	BOOL			IsEmpty() const { return !width || !height; }

	/** Get bounding box left offset */

	LayoutCoord		GetX() const { return x; }

	/** Get bounding box top offset */

	LayoutCoord		GetY() const { return y; }

	/** Get bounding box width */

	LayoutCoord		GetWidth() const { return width; }

	/** Get bounding box height */

	LayoutCoord		GetHeight() const { return height; }

	/** Get content box width */

	LayoutCoord		GetContentWidth() const { return content_width; }

	/** Get content box height */

	LayoutCoord		GetContentHeight() const { return content_height; }

	/** Get RECT for bunding box */

	void			GetBoundingRect(RECT &rect) const { rect.left = x; rect.top = y; rect.right = x + width; rect.bottom = y + height; }

	/** Get OpRect for bunding box */

	void			GetBoundingRect(OpRect &rect) const { rect.x = x; rect.y = y; rect.width = width; rect.height = height; }

	/** Get OpRect for content box */

	void			GetContentRect(OpRect &rect) const { rect.x = x; rect.y = y; rect.width = content_width; rect.height = content_height; }

	/** Tests if given point is inside bounding box */

	BOOL			Intersects(LayoutCoord tx, LayoutCoord ty) const { return tx >= x && tx < x + width && ty >= y && ty < y + height; }

	/** Intersect this bounding box with another one. */

	void			IntersectWith(const AbsoluteBoundingBox& other);

	/** Clip bounding box to a given rect */

	void			ClipTo(LayoutCoord clip_left, LayoutCoord clip_top, LayoutCoord clip_right, LayoutCoord clip_bottom);

};

/* Performance note: WORD_INFO_WIDTH_MAX should be significantly larger than WORD_INFO_LENGTH_MAX,
   so that when we need to split words, they will normally be split because they contain too many
   characters (not because the word is too wide). This way we avoid the expensive measuring of
   width of wide words over and over again (VisualDevice::GetTxtExtent() & co). */

#define WORD_INFO_WIDTH_MAX 32767
#define WORD_INFO_START_MAX 65535
#define WORD_INFO_LENGTH_MAX 1023
#define WORD_INFO_FONT_NUMBER_MAX 4095

/** Word or word fragment.

	An array of WordInfo is built to divide a text string into whole words or
	word fragments. One word will be split into several fragments when we need
	to switch font in the middle of the word, or when the word is too long
	(characters) or wide (pixels) to fit in one WordInfo, and also when
	aggressive line breaking is enabled (ERA).

	The actual text data is not part of this structure. */

class WordInfo
{
private:

	union
	{
		struct
		{
			unsigned int
					start:16;		// Make sure WORD_INFO_START_MAX is changed if the number of bits is changed.
			unsigned int
					length:10;		// Make sure WORD_INFO_LENGTH_MAX is changed if the number of bits is changed.
			unsigned int
					is_start_of_word:1;
			unsigned int
					collapsed:1;
			unsigned int
					can_linebreak_before:1;
			unsigned int
					is_tab_character:1;
			unsigned int
					has_trailing_whitespace:1;
			unsigned int
					has_ending_newline:1;
		} packed;
		unsigned int
					packed_init;
	};

	union
	{
		struct
		{
			unsigned int
					width:15;		// Make sure WORD_INFO_WIDTH_MAX is changed if the number of bits is changed.
			unsigned int
					first_line_width:1;
			signed int
					font_number:13;	// Make sure WORD_INFO_FONT_NUMBER_MAX is changed if the number of bits is changed.
			unsigned int
					is_misspelling:1;
			unsigned int
					has_baseline:1;

			// 1 bit unused.

		} packed2;
		unsigned int
					packed2_init;
	};

public:

	void			Reset() { packed_init = 0; packed2_init = 0; }

	/** Set start offset in the text string for this word or word fragment. */

	void			SetStart(unsigned int start) { OP_ASSERT(start <= WORD_INFO_START_MAX); packed.start = start; }

	/** Get start offset in the text string for this word or word fragment. */

	unsigned int	GetStart() const { return packed.start; }

	/** Set number of characters in this word or word fragment. */

	void			SetLength(unsigned int length) { OP_ASSERT(length <= WORD_INFO_LENGTH_MAX);  packed.length = length; }

	/** Get number of characters in this word or word fragment. */

	unsigned int	GetLength() const { return packed.length; }

	/** Specify whether this word fragment starts at the start of the word. */

	void			SetIsStartOfWord(BOOL is_start_of_word) { packed.is_start_of_word = !!is_start_of_word; }

	/** Return TRUE if this word fragment starts at the start of the word. */

	BOOL			IsStartOfWord() const { return packed.is_start_of_word; }

	/** Specify whether this word consists entirely of collapsed white-space. */

	void			SetCollapsed(BOOL collapsed) { packed.collapsed = !!collapsed; }

	/** Does this word consist entirely of collapsed white-space? */

	BOOL			IsCollapsed() const { return packed.collapsed; }

	/** Specify whether the line can be broken before this word. */

	void			SetCanLinebreakBefore(BOOL can_linebreak_before) { packed.can_linebreak_before = !!can_linebreak_before; }

	/** Can the line be broken before this word? */

	BOOL			CanLinebreakBefore() const { return packed.can_linebreak_before; }

	/** Specify whether this "word" is a tab character. */

	void			SetIsTabCharacter(BOOL is_tab_character) { packed.is_tab_character = !!is_tab_character; }

	/** Is this "word" a tab character? */

	BOOL			IsTabCharacter() const { return packed.is_tab_character; }

	/** Specify whether this word ends with trailing white-space. */

	void			SetHasTrailingWhitespace(BOOL has_trailing_whitespace) { packed.has_trailing_whitespace = !!has_trailing_whitespace; }

	/** Does this word end with trailing white-space? */

	BOOL			HasTrailingWhitespace() const { return packed.has_trailing_whitespace; }

	/** Specify whether this word ends with a new-line. */

	void			SetHasEndingNewline(BOOL has_ending_newline) { packed.has_ending_newline = !!has_ending_newline; }

	/** Does this word end with a new-line? */

	BOOL			HasEndingNewline() const { return packed.has_ending_newline; }

	/** Set the width of this word or word fragment, in pixels. */

	void			SetWidth(unsigned int width) { OP_ASSERT(width <= WORD_INFO_WIDTH_MAX); packed2.width = width; }

	/** Get the width of this word or word fragment, in pixels. */

	LayoutCoord		GetWidth() const { return LayoutCoord(packed2.width); }

	/** Specify whether the width of this word was measured with ::first-line properties applied. */

	void			SetIsFirstLineWidth(BOOL first_line_width) { packed2.first_line_width = first_line_width; }

	/** Return TRUE if the width of this word was measured with ::first-line properties applied. */

	BOOL			IsFirstLineWidth() const { return packed2.first_line_width; }

	/** Specify what font face is used for this word. */

	void			SetFontNumber(int font_number) { OP_ASSERT(font_number <= WORD_INFO_FONT_NUMBER_MAX); packed2.font_number = font_number; }

	/** What font face is used for this word? */

	int				GetFontNumber() const { return packed2.font_number; }

	/** Specify whether this word is misspelled or not. Used with the spell checker. */

	void			SetIsMisspelling(BOOL is_misspelling) { packed2.is_misspelling = !!is_misspelling; }

	/** Return TRUE if the word is misspelled. Used with the spell checker. */

	BOOL			IsMisspelling() const { return packed2.is_misspelling; }

	/** Specify whether this word has a baseline. Words consisting entirely of CJK characters typically don't have a baseline. */

	void			SetHasBaseline(BOOL has_baseline) { packed2.has_baseline = !!has_baseline; }

	/** Does this word have a baseline? Words consisting entirely of CJK characters typically don't have a baseline. */

	BOOL			HasBaseline() const { return packed2.has_baseline; }
};


/** State data used when traversing a Line.
 * When traversing a Line object with BiDi, the Line traversal job may be split
 * into several jobs, with one segment of the Line at a time.
 */

struct LineSegment
{
	const Line*		line; ///< The Line object to which this segment belongs

	LayoutProperties*
					container_props;

	HTML_Element*	start_element; ///< Start element for traversing the current segment
	LayoutCoord		start; ///< Start position on the virtual line for the current segment

	HTML_Element*	stop_element; ///< Stop element for traversing the current segment
	LayoutCoord		stop; ///< Stop position on the virtual line for the current segment

	LayoutCoord		length; ///< Width of this segment, not counting justified space
	short			word_number; ///< Number of the word currently being traversed, counted visually from left to right on the Line
	CSSValue		white_space; ///< Value of the CSS white-space property
	BOOL			justify; ///< Is this text justified (CSS property text-align == justify)?
	LayoutCoord		justified_space_extra_accumulated; ///< Sum of justified space added so far during traversal of this Line
	LayoutCoord		x; ///< X position relative to the container of this segment
#ifdef SUPPORT_TEXT_DIRECTION
	BOOL			left_to_right; ///< Is this segment drawn from left to right?
	LayoutCoord		offset; ///< The offset on the line where this segment starts
#endif //SUPPORT_TEXT_DIRECTION

	/** Get horizontal distance from segment's left edge to container's left border edge. */

	LayoutCoord		GetHorizontalOffset() const
	{
		return
#ifdef SUPPORT_TEXT_DIRECTION
			offset +
#endif // SUPPORT_TEXT_DIRECTION
			x;
	}
};

/** Entry in a stacking context.

	Each box that is positioned and/or establishes a new stacking context on
	its own gets such an entry. */

class ZElement
  : public Link
{
private:

	/** Element this list belongs to. */

	HTML_Element*	element;

	/** Z index of this list. */

	long			z_index;

	/** Used value of the 'order' property.

		This property doesn't apply to all elements, but when it does apply,
		the primary key for sorting the stacking context is 'z-index', while
		the secondary key is this one - 'order'. In other words, 'order' may
		only make a difference in the sorting of two Z elements if they have
		the same 'z-index'. */

	long			order;

	/** Previous logical element. */

	ZElement*		pred_logical;

	/** Next logical element */

	ZElement*		suc_logical;

	/** Used to cache the result of HasSameClippingStack().

		It is reset to 'MAYBE' when restarting the StackingContext that this belongs
		to.

		@see HasSameClippingStack(). */

	mutable BOOL3	has_same_clipping_stack;

public:

					ZElement(HTML_Element* html_element)
					  : element(html_element),
						z_index(0),
						order(0),
						pred_logical(NULL),
						suc_logical(NULL),
						has_same_clipping_stack(MAYBE) {}
					~ZElement() { Remove(); }

	/** Remove element from lists. */

	void			Remove();

	/** Get HTML element. */

	HTML_Element*	GetHtmlElement() const { return element; }

	/** Get Z index. */

	long			GetZIndex() const { return z_index; }

	/** Set Z index. */

	void			SetZIndex(long index) { z_index = (index == CSS_ZINDEX_auto ? 0 : index); }

	/** Get the used value of the 'order' property. */

	long			GetOrder() const { return order; }

	/** Set the used value of the 'order' property. */

	void			SetOrder(long order) { this->order = order; }

	/** Does this ZElement have the same clipping stack as its predecessor on the
		StackingContext between the first common ancestor and z root ?

		Clipping stack "between the first common ancestor and z root" means that all
		the ScrollableArea clip rects introduced by the elements on that path either
		apply to both adjacent ZElements or none of them.

		Asserts having predecessor and that the predecessor has same z-index and
		shouldn't be used when the tree is dirty.

		NOTE: Currently we don't check any elements that are ancestors of the z root
		(owning StackingContext) of this. And that is part of CORE-42313.

		@return TRUE when The predecessor has the same clipping stack, FALSE otherwise. */

	BOOL			HasSameClippingStack() const;

	/** Clear the cached result of HasSameClippingStack().

		@see HasSameClippingStack() */

	void			ResetSameClippingStack() { has_same_clipping_stack = MAYBE; }

	/** Return TRUE if the Z elements are in the same painting stack.

		This is true when they have the same value of 'z-index' and 'order'. */

	BOOL			InSamePaintingStack(ZElement* other) const { return z_index == other->z_index && order == other->order; }

	/** Get previous logical element. */

	ZElement*		LogicalPred() const { return pred_logical; }

	/** Get next logical element. */

	ZElement*		LogicalSuc() const { return suc_logical; }

	/** Set previous logical element. */

	void			SetLogicalPred(ZElement* pred) { pred_logical = pred; }

	/** Set next logical element. */

	void			SetLogicalSuc(ZElement* suc) { suc_logical = suc; }
};

class ZElementList
  : public Head
{
public:

	/** Is this the stacking context? */

	virtual BOOL	IsStackingContext() const { return FALSE; }
};

/** Z-ordered list of HTML elements.

	This class implements a list of HTML elements that have z-indices
	different from 0.  Elements are ordered in increasing z-index
	values. */

class StackingContext
  : public ZElementList
{
private:

	/** List of pending elements. */

	ZElementList	pending_elements;

	/** Last added Z element (i.e. logical last). */

	ZElement*		last;

	/** Should we trigger UpdateBottomAligned() inside this stacking context? */

	BOOL			needs_bottom_aligned_update;

	/** TRUE if this or any descendant sub-stacking-context has at least one fixed
		positioned box on itself, FALSE otherwise.

		NOTE: May be invalid if the root element is currently dirty. */

	BOOL			has_fixed_element;

public:

					StackingContext()
					  : last(NULL),
						needs_bottom_aligned_update(FALSE),
						has_fixed_element(FALSE) {}

					~StackingContext();

	/** @return TRUE if the stacking context contains no elements. */

	BOOL			IsEmpty() const { return last == NULL; }

	/** Restart list. */

	void			Restart();

	/** Add element that has z-index different from 0 to list. */

	void			AddZElement(ZElement* element);

	/** Skip over an element. */

	BOOL			SkipElement(HTML_Element* element, LayoutInfo& info);

	/** Specify that this stacking context has at least one fixed-positioned box.

		Should be called when a fixed positioned box is added to this. */

	void			SetHasFixedElement() { has_fixed_element = TRUE; }

	/** Translate children of element, because of collapsing margins on container. */

	void			TranslateChildren(HTML_Element* element, LayoutCoord offset, LayoutInfo& info);

	/** Update bottom aligned elements. */

	void			UpdateBottomAligned(LayoutInfo& info);

	/** Traverse elements with z-index between given interval. */

	void			Traverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops, Box* parent_box, BOOL positive_z_index, BOOL handle_overflow_clip);

	/** Traverse box with children. */

	void			LineTraverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops, Content* parent_content, BOOL positive_z_index, const LineSegment& segment, LayoutCoord baseline, LayoutCoord x, BOOL handle_overflow_clip);

	/** Is this the stacking context? */

	virtual BOOL	IsStackingContext() const { return TRUE; }

	/** Remove element from list. */

	void			Remove(ZElement* remove_element);

	/** Find the normal right edge of the rightmost absolute positioned box. */

	LayoutCoord		FindNormalRightAbsEdge(HLDocProfile* hld_profile, LayoutProperties* parent_cascade);

	/** @return TRUE if there are any fixed positioned elements in this stacking context
				or any of its sub-stacking-contexts. */

	BOOL			HasFixedElements() const { return has_fixed_element; }

	/** @return TRUE if there are any absolutely positioned elements in this stacking context
				or any of its sub-stacking-contexts. */

	BOOL			HasAbsolutePositionedElements();

	/** Has z children with negative z-index */

	BOOL			HasNegativeZChildren();

	/** Invalidate fixed descendants of a given HTML_Element. This is done
		as part of invalidation of the ancestor element itself. The bounding
		box for the HTML_Element does not include fixed positioned descendants,
		so this is where we invalidate the fixed descendants.
		@param doc Pointer to the FramesDocument where this stacking context lives.
		@param ancestor The element being invalidated. No need to invalidate elements
						that are not descendants of ancestor. If ancestor is NULL,
						all fixed positioned elements are unconditionally invalidated. */

	void			InvalidateFixedDescendants(FramesDocument* doc, HTML_Element* ancestor);

    /** Signal that this StackingContext has bottom aligned descendants that still need to
		propagate their bboxes */

	void			SetNeedsBottomAlignedUpdate() { needs_bottom_aligned_update = TRUE; }

	/** Finish this StackingContext.

		Typically called when the owner has finished layout.

		@param cascade The cascade of the owner. */

	void			FinishLayout(LayoutProperties* cascade);
};

/** Absolutely positioned box affected by its containing block */

class AffectedAbsPosElm
{
private:
	AbsolutePositionedBox*
					box;

	AffectedAbsPosElm*
					pred;

	BOOL			skipped;

public:
	AffectedAbsPosElm(AbsolutePositionedBox *box, AffectedAbsPosElm *pred, BOOL skipped) : box(box), pred(pred), skipped(skipped) { }
	~AffectedAbsPosElm() { OP_DELETE(pred); }

	AbsolutePositionedBox*
					GetLayoutBox() { return box; }

	/** Was layout of this element skipped? */

	BOOL			WasSkipped() { return skipped; }

	AffectedAbsPosElm*
					Unlink() { AffectedAbsPosElm *e = pred; pred = 0; return e; }
};

/** List of boxes that are affected by the size of their containing block
	size. */

class AffectedAbsPosDescendants
{
private:
	AffectedAbsPosElm*
					last;

public:
					AffectedAbsPosDescendants() : last(0) { }

					~AffectedAbsPosDescendants() { OP_DELETE(last); }

	/** Add a box that is affected by the size of its containing block. */

	BOOL			AddAffectedBox(AbsolutePositionedBox *box, BOOL skipped);

	/** The size or position of the containing block established by this owner
	 * box of this box list has changed.
	 * @param info Layout information structure
	 * @param containing_block Containing block to which this list belongs
	 */

	void			ContainingBlockChanged(LayoutInfo &info, Box *containing_block);
};

/** Positioned element. Keeps track of stacking order, positioning offset and containing block calculation. */

class PositionedElement
	: public ZElement
{
protected:

	/** Y offset (as specified by top/bottom CSS properties) */

	LayoutCoord		y_offset;

	/** Height of the containing block established by this box. */

	LayoutCoord		containing_block_height;

	/** X offset (as specified by left/right CSS properties) */

	LayoutCoord		x_offset;

	/** Width of the containing block established by this box. */

	LayoutCoord		containing_block_width;

	union
	{
		struct
		{
			/** Did containing block width change in the last reflow pass? */

			unsigned int
					containing_block_width_changed:1;

			/** Has containing block height been calculated? */

			unsigned int
					containing_block_height_calculated:1;

			/** Did containing block height change in the last reflow pass? */

			unsigned int
					containing_block_height_changed:1;
		} poselm_packed;
		UINT32		poselm_packed_init;
	};

public:

					PositionedElement(HTML_Element* element)
						: ZElement(element),
						  y_offset(0),
						  containing_block_height(0),
						  x_offset(0),
						  containing_block_width(0),
						  poselm_packed_init(0) {}

	/** Calculate and set offset caused by relative positioning. */

	void			Layout(LayoutProperties* cascade);

	/** Get offset caused by relative positioning. */

	void			GetPositionedOffset(LayoutCoord& x, LayoutCoord& y) const { x = x_offset; y = y_offset; }

	/** Set new containing block width and check if it changed since last time. */

	void			SetNewContainingBlockWidth(LayoutCoord width) { poselm_packed.containing_block_width_changed = containing_block_width != width; containing_block_width = width; }

	/** Has the containing block height been calculated in this reflow pass? */

	BOOL			IsContainingBlockHeightCalculated() const { return poselm_packed.containing_block_height_calculated; }

	/** Set new containing block height and check if it changed since last time. */

	void			SetNewContainingBlockHeight(LayoutCoord height) { poselm_packed.containing_block_height_calculated = 1; if (containing_block_height != height) poselm_packed.containing_block_height_changed = 1; containing_block_height = height; }

	/** Return TRUE if the containing block size changed in the last reflow pass. */

	BOOL			HasContainingBlockSizeChanged() const { return poselm_packed.containing_block_width_changed || poselm_packed.containing_block_height_changed; }

	/** Get the value to use when calculating the containing block width for
		absolutely positioned children. The value will include border and
		padding. */

	LayoutCoord		GetContainingBlockWidth() { return containing_block_width; }

	/** Get the value to use when calculating the containing block height for
		absolutely positioned children. The value will include border and
		padding. */

	LayoutCoord		GetContainingBlockHeight() { return containing_block_height; }
};

#ifdef CSS_TRANSFORMS

/** A translation state is creates before entering a transformed
	context.  Inside all translations are reset back to zero.  When
	the transformed context is exited, the translations are reset to
	the saved state. */

struct TranslationState
{
	TranslationState() : precise_clipping(TRUE) {}

	LayoutCoord translation_x;
	LayoutCoord root_translation_x;
	LayoutCoord fixed_pos_x;

	LayoutCoord translation_y;
	LayoutCoord root_translation_y;
	LayoutCoord fixed_pos_y;

	/** The old_transform_root is used during (hit testing) traversal
		passes, to stack transform roots and thus help the traverse
		object to keep track of the current transform root. @see
		TraversalObject::GetTransformRoot() */

	AffinePos old_transform_root;

	/** Used by HitTestingTraversalObject to store the data related with
		the current clippig state before a new transform push. */

	OpRect old_clip_rect;
	BOOL is_real_clip_rect;
	RECT old_area;
	BOOL precise_clipping;

	/** Used by ElementSearchObject. */

	RECT old_main_area;

	void Read(const TraversalObject *traversal_object);
	void Write(TraversalObject *traversal_object) const;

	void Read(const LayoutInfo &info);
	void Write(LayoutInfo &info) const;
};

/** Data needed during reflow of a transformed box. */

class TransformedReflowState
{
public:
	TransformedReflowState(TransformContext *context) : transform_context(context) {}

	/** Cached pointer to the transform context. Should always be
		equal to GetTransformContext() of the box this reflow state is
		attached to.*/

	TransformContext *transform_context;

	/** The old transform. Used to check if the transform has changed
		during layout, so that we know when to invalidate area of the
		bounding box. */

	AffineTransform old_transform;

	/** Pointer to the previous transform for inline boxes. Used by
		inline boxes to implement a stack for the container. The
		previous transform is the state before the inline box is laid
		out and is stored here before the transform of this inline box
		is applied. That way it can be restored after the layout of
		this inline box has finished. */

	AffineTransform *previous_transform;

	/** Translation state before entering a transform. A new
		translation state is used inside the transformed box. */

	TranslationState translation_state;
};

#endif // CSS_TRANSFORMS

/** Reflow state. */

class ReflowState
{
public:

	HTML_Element*	html_element;

	LayoutProperties*
					cascade;

#ifdef CSS_TRANSFORMS
	TransformedReflowState *
					transform;
#endif

					ReflowState()
					  : html_element(NULL),
						cascade(NULL)
#ifdef CSS_TRANSFORMS
					  , transform(NULL)
#endif
					{}

	virtual			~ReflowState() {
#ifdef CSS_TRANSFORMS
		OP_DELETE(transform);
#endif
	}
};

/** Vertical box reflow state. */

class VerticalBoxReflowState
  : public ReflowState
{
public:

	/** Old y position of box. */

	LayoutCoord		old_y;

	/** Old height of box. */

	LayoutCoord		old_height;

	/** Previous y root established by positioned box. */

	LayoutCoord		previous_root_y;

	/** Previous Y scroll amount that doesn't affect absolutely positioned descendants. */

	LayoutCoord		previous_nonpos_scroll_y;

	/** Old minimum Y (estimated Y position if maximum width is satisfied). */

	LayoutCoord		old_min_y;

	/** Old minimum height (estimated height if maximum width is satisfied). */

	LayoutCoord		old_min_height;

	/** Old maximum width. */

	LayoutCoord		old_max_width;

	/** Old x position of box. */

	LayoutCoord		old_x;

	/** Old width of box. */

	LayoutCoord		old_width;

	/** Previous x root established by positioned box. */

	LayoutCoord		previous_root_x;

	/** Previous X scroll amount that doesn't affect absolutely positioned descendants. */

	LayoutCoord		previous_nonpos_scroll_x;

	RelativeBoundingBox
					old_bounding_box;

#ifdef PAGED_MEDIA_SUPPORT

	/** Value of info.paged_media when entering this box. */

	PM_MODE			old_paged_media;

#endif // PAGED_MEDIA_SUPPORT

	/** Is the CSS computed height value of this box auto? */

	BOOL			has_auto_height;

	/** Has the box established a root translation? */

	BOOL			has_root_translation;

	/** Needs to update screen after layout */

	BOOL			needs_update;

	/** Absolutely positioned descendants that are affected by the containing block established by this box. */

	AffectedAbsPosDescendants
					abs_pos_descendants;

					VerticalBoxReflowState()
					  : old_y(0),
						old_height(0),
						previous_root_y(0),
						previous_nonpos_scroll_y(0),
						old_min_y(LAYOUT_COORD_MIN),
						old_min_height(0),
						old_max_width(LAYOUT_COORD_MAX),
						old_x(0),
						old_width(0),
						previous_root_x(0),
						previous_nonpos_scroll_x(0),
						has_auto_height(FALSE),
						has_root_translation(FALSE),
						needs_update(FALSE) {}

	void*			operator new(size_t nbytes) OP_NOTHROW { return g_verticalbox_reflow_state_pool->New(sizeof(VerticalBoxReflowState)); }
	void			operator delete(void* p, size_t nbytes) { g_verticalbox_reflow_state_pool->Delete(p); }
};

/** List item used in RectList */

class RectListItem : public Link
{
public:
	RectListItem(const RECT &r) : rect(r) {}
	RECT rect;

	RectListItem *Suc() const { return static_cast<RectListItem *>(Link::Suc()); }
};

/** List used when fetching a series of rectangles from Box::GetRectList. */

class RectList : public Head
{
public:
	RectListItem *First() const { return static_cast<RectListItem *>(Head::First()); }
};

/** Bounding box propagation type */

typedef enum {

	/** It was a floating box that triggered this bounding box propagation */

	PROPAGATE_FLOAT,

	/** It was an absolute positioned box that triggered this bounding box propagation */

	PROPAGATE_ABSPOS,

	/** It was an absolute positioned box that triggered this bounding box propagation
		and we have already reached its containing element (where we usualy stop propagation).
		Because containing element had no reflow state (this was a propagation triggered
		by SkipBranch() call) we had to propagate bbox past it. Unlike PROPAGATE_ABSPOS,
		PROPAGATE_ABSPOS_SKIPBRANCH allows propagated bbox to be clipped when necessary */

	PROPAGATE_ABSPOS_SKIPBRANCH,

	/** Do not propagate bounding box. Instead, notify all stacking contexts between triggering
		abspos and its containing element that UpdateBottomAligned() needs to be called on that
		branch */

	PROPAGATE_ABSPOS_BOTTOM_ALIGNED

} PropagationType;

/** Bounding box propagation options.

	Defines an origin of propagation (abspos or float) and in case of propagation originating
	in abspos box gives additional information about coordinates of propagated
	bounding box (whether they are static or non-static) */

struct PropagationOptions
{
						PropagationOptions(PropagationType t, BOOL h_pos = TRUE, BOOL v_pos = TRUE)
						  : type(t),
							has_static_h_position(h_pos),
							has_static_v_position(v_pos) {}

	/** Propagation type */

	PropagationType		type;

	/** Is horizontal position of propagated bounding box static? */

	BOOL				has_static_h_position;

	/** Is vertical position of propagated bounding box static? */

	BOOL				has_static_v_position;

	/** Are both: horizontal end vertical offsets static? */

	BOOL				IsStatic() const { return has_static_h_position && has_static_v_position; }
};

enum BoxSignalReason
{
	BOX_SIGNAL_REASON_IMAGE_DATA_LOADED,	///< New image data or SVG image finished loading.
	BOX_SIGNAL_REASON_UNKNOWN				///< Default. Feel free to add more reasons when needed.
};

/** Layout box. */

class Box
{
	OP_ALLOC_ACCOUNTED_POOLING
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT

private:

	/** Delete reflow state.

		This is called on FinishLayout(). */

	virtual void	DeleteReflowState() {}

	/** Get a rectangle and optionally a rectangle list from the box.

		Output is a transform along with a rect in the coordinate
		system of the transform.  If there are no tranformed ancestor
		or CSS Transforms is disabled, the transform will be an empty
		translation.

	    This is used from the public methods GetRect() and
	    GetRectList().

	    @see note on SCROLL_BOX at GetRect(FramesDocument*, BoxRectType, RECT&, int, int) . */
	BOOL			GetRect(FramesDocument* doc, BoxRectType type,
							AffinePos &ctm, RECT& rect, RectList *rect_list,
							int element_text_start, int element_text_end);

	/** Get the rectangle and optionally a rectangle list from the box.

		Output is a rectangle in document coordinates big enough to
		hold the area coverted by the box, as specified by the
		BoxRectType. */

	BOOL			GetBBox(FramesDocument* doc, BoxRectType type, RECT& rect,
							RectList *rect_list, int element_text_start, int element_text_end);

protected:

	/** Helper for adding offsets to the rects in GetRect. rect_list can be NULL. */

	static void		TranslateRects(RECT &rect, RectList *rect_list, long x, long y);

	/** Iterates through children elements of the element that owns this box,
		recursively calling CountWords on the boxes of children elements.
		@see Box::CountWords().
		Stops, if some recursive call returns FALSE.

		@param[in] segment The line segment where we count the words in.
		@param[out] word The number of words, passed during recursive calls.
		@return FALSE if any recursive call of CountWords returns FALSE,
				TRUE otherwise. */

	BOOL			CountWordsOfChildren(LineSegment& segment, int& words) const;

	/** Iterates through children elements of the element that owns this box,
		recursively calling AdjustVirtualPos on the boxes of children elements.
		@see Box::AdjustVirtualPos().

		@param offset The offset to adjust the virtual position by. */

	void			AdjustVirtualPosOfChildren(LayoutCoord offset);

	/** Get the list item marker pseudo element of this box, or NULL if there is none. */

	HTML_Element*	GetListMarkerElement() const;

public:

	/** Clean out reflow states. */

	virtual void	CleanReflowState() { DeleteReflowState(); }

	/** Command flags that may be passed to @ref GetOffsetFromAncestor(). */

	enum GetPositionCommandFlags
	{
		/** Ignore scroll position for boxes with overflow set to auto or
			scroll. */

		GETPOS_IGNORE_SCROLL = 1,

		/** Inline boxes are problematic, since their correct position cannot
			be known without the cascade (text-align, vertical-align, left and
			such). Set this flag to give up if an inline box is encountered. */

		GETPOS_ABORT_ON_INLINE = 2,

		/** When computing offsets for offsetTop and offsetLeft we
			must send special flags to GetContaingElmenet that
			indicate that we want to traverse the parent chain as
			specified by offsetParent. Set this flag to indicate that
			the values should be used by offsetLeft and offsetTop. */

		GETPOS_OFFSET_PARENT = 4
	};

	/** Result flags that may be returned by @ref GetOffsetFromAncestor(). */

	enum GetPositionResultFlags
	{
		/** An inline box was encountered. Note that this may be set even if
			GETPOS_ABORT_ON_INLINE wasn't set. */

		GETPOS_INLINE_FOUND = 1,

		/** A box with position fixed was encountered. */

		GETPOS_FIXED_FOUND = 2

#ifdef CSS_TRANSFORMS
		/** A box with a transform was encountered. */

		, GETPOS_TRANSFORM_FOUND = 4
#endif
	};

	virtual			~Box() {}

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE

	/** Signal that the HTML_Element associated with this layout box about to be deleted. */

	virtual void	SignalHtmlElementDeleted(FramesDocument* doc);

#endif // LAYOUT_TRAVERSE_DIRTY_TREE

	virtual const HTMLayoutProperties*
					GetHTMLayoutProperties() const { return NULL; }

	/** Get container established by this box, if any. */

	virtual Container*
					GetContainer() const { return NULL; }

	/** Returns a pointer to the content if any otherwise NULL */

	virtual Content*
					GetContent() const { return NULL; }

	/** Get table content of this box, if it has one. */

	virtual TableContent*
					GetTableContent() const { return NULL; }

	/** Get flex content (also known as flexbox or flex container) established by this box, if it has one. */

	virtual FlexContent*
					GetFlexContent() const { return NULL; }

	/** Get selectcontent of this box, if it has one. */

	virtual SelectContent*
					GetSelectContent() const { return NULL; }

	/** Get scrollable, if it is one. */

	virtual ScrollableArea*
					GetScrollable() const { return NULL; }

	/** Get button container of this box, if it has one. */

	virtual ButtonContainer*
					GetButtonContainer() const { return NULL; }

#ifdef SVG_SUPPORT
	/** Get svg content of this box, if it has one. */

	virtual SVGContent*
					GetSVGContent() const { return NULL; }
#endif // SVG_SUPPORT

	/** Signal that content may have changed. */

	virtual void	SignalChange(FramesDocument* doc, BoxSignalReason reason = BOX_SIGNAL_REASON_UNKNOWN) {}

	/** Recalculate containing block size and schedule the specified descendant box for reflow if needed. */

	virtual void	RecalculateContainingBlock(FramesDocument* doc, AbsolutePositionedBox* box) { OP_ASSERT(!"Missing re-implementation of this method"); }

	/** Recalculate the top margin after a new block box has been added to a container's layout
		stack. Collapse the margin with preceding adjacent margins if appropriate. If the top
		margin of this block is adjacent to an ancestor's top margin, it may cause the ancestor's
		Y position to change. If the top margin of this block is adjacent to a preceding sibling's
		bottom margin, this block may change its Y position.

		@return TRUE if the Y position of any element was changed. */

	virtual BOOL	RecalculateTopMargins(LayoutInfo& info, const VerticalMargin* top_margin, BOOL has_bottom_margin = FALSE) { return FALSE; }

	/** Layout of this box is finished (or skipped). Propagate changes (bottom margins,
		bounding-box) to parents. This may grow the box, which in turn may cause its parents to be
		grown. Bottom margins may participate in margin collapsing with successive content, but
		only if this box is part of the normal flow. In that case, propagate the bottom margin to
		the reflow state of the container of this box. Since this member function is used to
		propagate bounding boxes as well, we may need to call it even when box is empty and
		is not part of the normal flow. To protect against margins being propagated and parent
		container reflow position updated in such case, 'has_inflow_content' flag has been introduced
		in order to notify container that this box is not a part of normal flow but has bounding box
		that still needs to be propagated. Separation of bounding box propagation and margin/reflow state
		update should be considered. */

	virtual void	PropagateBottomMargins(LayoutInfo& info, const VerticalMargin* top_margin = 0, BOOL has_inflow_content = TRUE) {}

	/** Expand relevant parent containers and their bounding-boxes to contain floating and
		absolutely positioned boxes. A call to this method is only to be initiated by floating or
		absolutely positioned boxes.

		@param info Layout information structure
		@param bottom Only relevant for floats: The bottom margin edge of the float, relative to
		top border-edge of this box.
		@param min_bottom Only relevant for floats: The bottom margin edge of the float if
		maximum width of its container is satisfied
		@param child_bounding_box Bounding-box of the propagating descendant, joined with the
		bounding-boxes of the elements up the parent chain towards this element. Relative to the
		top border-edge of this box.
		@param opts Bounding box propagation options */

	virtual void	PropagateBottom(const LayoutInfo& info, LayoutCoord bottom, LayoutCoord min_bottom, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts) {}

	/** Grow width of inline box. */

	virtual void	GrowInlineWidth(LayoutCoord increment) {}

	/** Get position on the virtual line the box was laid out on. */

	virtual LayoutCoord
					GetVirtualPosition() const { return LAYOUT_COORD_MIN; }

	/** Get the distance between this box and an ancestor (number of pixels
		below and to the right of the ancestor, measured between top/left border
		edges).

		NOTE: This method doesn't work in some cases (the list below may be incomplete):
		- fixed positioned elements with specified offsets
		- fixed positioned elements with unspecified offsets computed offsets do
		  not include the layout viewport position
		- we are trying to get an offset to some ancestor from an absolute positioned
		  box, whose containing block is established by an inline positioned box
		- all the cases described in @ref GetPositionResultFlags.

		@param x Will be set to the horizontal distance to the ancestor. The
		value is not reliable if GETPOS_INLINE_FOUND is part of the return
		value, especially if GETPOS_ABORT_ON_INLINE was part of the command
		flags.
		@param y Will be set to the vertical distance to the ancestor. The
		value is not reliable if GETPOS_INLINE_FOUND is part of the return
		value, especially if GETPOS_ABORT_ON_INLINE was part of the command
		flags.
		@param ancestor The ancestor element. This MUST be an ancestor of this
		box, or NULL. If it is NULL, the ancestor is the root element.
		@param flags Flags to tune the behavior -- see @ref
		GetPositionCommandFlags
		@return result flags -- see @ref GetPositionResultFlags. */

	int				GetOffsetFromAncestor(LayoutCoord& x, LayoutCoord& y, HTML_Element* ancestor=0, int flags=0);

	/** Mark all boxes with fixed (non-percentage, non-auto) width in this subtree dirty. */

	void			MarkBoxesWithAbsoluteWidthDirty(FramesDocument* doc);

	/** Mark all containers in this subtree dirty. */

	void			MarkContainersDirty(FramesDocument* doc);

	/** Mark all content with percentage height in this subtree dirty. */

	void			MarkContentWithPercentageHeightDirty(FramesDocument* doc);

	/** Clear 'use old row heights' mark on all tables in this subtree. */

	void			ClearUseOldRowHeights();

	/** Return the z element list of the object, if it has any. */

	virtual StackingContext*
					GetLocalStackingContext() { return NULL; }

	/** Return the z element of the object, if it has any. */

	virtual ZElement*
					GetLocalZElement() { return NULL; }

	/** Return the space manager of the object, if it has any. */

	virtual SpaceManager*
					GetLocalSpaceManager() { return NULL; }

#ifdef CSS_TRANSFORMS

	/** Return the transform context, if it has any. */

	virtual TransformContext*
					GetTransformContext() { return NULL; }

	virtual const TransformContext*
					GetTransformContext() const { return NULL; }

	/** Has this box a transform context? */

	BOOL			HasTransformContext() const { return GetTransformContext() != NULL; }

#else

	BOOL			HasTransformContext() const { return FALSE; }

#endif

	/** Get height. */

	virtual LayoutCoord
					GetHeight() const { return LayoutCoord(0); }

	/** Get width of box. */

    virtual LayoutCoord
					GetWidth() const { return LayoutCoord(0); }

	/** Get the value to use when calculating the containing block width for
		absolutely positioned children. The value will include border and
		padding. */

	virtual LayoutCoord
					GetContainingBlockWidth() { return LayoutCoord(0); }

	/** Get the value to use when calculating the containing block height for
		absolutely positioned children. The value will include border and
		padding. */

	virtual LayoutCoord
					GetContainingBlockHeight() { return LayoutCoord(0); }

	/** Get a rectangle representing the border edges in the local
		coordinate system. */

	OpRect			GetBorderEdges() const { return OpRect(0, 0, GetWidth(), GetHeight()); }

	/** Returns a rectangle representing the content edges in the local
		coordinate system and current scroll offset for scrollable content. */

	void			GetContentEdges(const HTMLayoutProperties &props, OpRect& rect, OpPoint& offset) const;

	/** Get a rectangle representing the padding edges in the local
		coordinate system. */

	OpRect			GetPaddingEdges(const HTMLayoutProperties &props) const;

	/** Get the rectangles for each line a box may be split into, as
	    in the case for line-breaked inline content. Add these
	    rectangles to a list. Note: the caller is responsible for the
	    rectangles in the list after the call has been made. A simple
	    Clear() operation, when done with the list, is often
	    sufficient.

		Returns FALSE if an OOM condition was raised, TRUE otherwise.
	*/

	BOOL			GetRectList(FramesDocument* doc, BoxRectType type, RectList &rect_list);

	/** Get the rectangle corresponding to a box.

		SUBSTRING MATCHING

		The parameters 'element_text_start' and 'element_text_end'
		works on boxes corresponding to text elements. They allow the
		caller to get the rectangle of a substring of the text, thus
		ignoring parts outside the start and end indexes. Character
		resolution including whitespaces is supported.

		See tests in modules/layout/selftest/box.ot for examples.

		Should deprecate this in favor of GetBBox()

		@note If BoxRectType is set to SCROLL_BOX, the out argument
		      rect will actually represent two different things:
		      - rect.left and rect.top refer to the _position_
		        of the box's top-left corner relative to the content
		        when scrolled. This is DOM's scrollLeft and scrollTop.
		      - rect.right and rect.bottom refer to the _dimensions_
		        of the box's content (the scrollable area).
		        If you subtract rect.left and rect.top you get
		        DOM's scrollWidth and scrollHeight. */

	BOOL			GetRect(FramesDocument* doc, BoxRectType type, RECT& rect, int element_text_start = 0, int element_text_end = -1);

	BOOL			GetBBox(FramesDocument* doc, BoxRectType type, RECT& rect, int element_text_start = 0, int element_text_end = -1);

	BOOL			GetRect(FramesDocument* doc, BoxRectType type, AffinePos &position, RECT &rect, int element_text_start = 0, int element_text_end = -1);

	/** Lay out box.

		This is called when the element is first created, and during
		reflowing of the parent. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Count the number of words in segment that overlap with this box.

		Used for justified text.

		@param[in] segment The line segment where we count the words in.
		@param[out] words Gets increased by number of words that overlap with
						  this box and the segment.
		@return TRUE if there may be more words in the segment after this box, FALSE otherwise. */

	virtual BOOL	CountWords(LineSegment& segment, int& words) const { return TRUE; }

	/** Adjust the virtual position on the line of this box and its descendants placed
		on the same line.

		The caller must be sure that the call of this method will not affect used space
		on the line and make sure the bounding box of the line will be correct after
		such operation. The method cannot be called on the boxes of elements
		that span more than one line.

		@param offset The offset to adjust the virtual position by. */

	virtual void	AdjustVirtualPos(LayoutCoord offset) {}

	/** Get underline for elements on the specified line. */

	virtual BOOL	GetUnderline(const Line* line, short& underline_position, short& underline_width) const { return FALSE; }

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info) { return LAYOUT_CONTINUE; }

	/** Update screen. */

	virtual void	UpdateScreen(LayoutInfo& info) {}

	/** Invalidate the screen area that the box uses. */

	virtual void	Invalidate(LayoutProperties* parent_cascade, LayoutInfo& info) const {}

	/** Is this box a run-in box? */

	virtual BOOL	IsInlineRunInBox() const { return FALSE; }

	/** Is this box a vertical box? */

	virtual BOOL	IsVerticalBox() const { return FALSE; }

	/** Is this box a block box? */

	virtual BOOL	IsBlockBox() const { return FALSE; }

	/** Is this box a floating box? */

	virtual BOOL	IsFloatingBox() const { return FALSE; }

	/** Is this box an inline box? */

	virtual BOOL	IsInlineBox() const { return FALSE; }

	/** Is this box an inline block box? */

	virtual BOOL	IsInlineBlockBox() const { return FALSE; }

	/** Does this box have inline content? */

	virtual BOOL	IsInlineContent() const { return FALSE; }

	/** Is this box a compact box? */

	virtual BOOL	IsInlineCompactBox() const { return FALSE; }

	/** Is this box a compact box? */

	virtual BOOL	IsBlockCompactBox() const { return FALSE; }

	/** Is this box a text box? */

	virtual BOOL	IsTextBox() const { return FALSE; }

	/** Is this box an empty text box (white space only)? */

	virtual BOOL	IsEmptyTextBox() const { return FALSE; }

	/** Is this box one of the table boxes or a box with table content? */

	virtual BOOL	IsTableBox() const { return FALSE; }

	/** Is this box a table cell? */

	virtual BOOL	IsTableCell() const { return FALSE; }

	/** Is this box a table row? */

	virtual BOOL	IsTableRow() const { return FALSE; }

	/** Is this box a table row group? */

	virtual BOOL	IsTableRowGroup() const { return FALSE; }

	/** Is this box a table column group? */

	virtual BOOL	IsTableColGroup() const { return FALSE; }

	/** Is this box a table caption? */

	virtual BOOL	IsTableCaption() const { return FALSE; }

	/** Is this box a flex item? */

	virtual BOOL	IsFlexItemBox() const { return FALSE; }

	/** Is this box a positioned box? */

	virtual BOOL	IsPositionedBox() const { return FALSE; }

	/** Is this box an absolute positioned box? */

	virtual BOOL	IsAbsolutePositionedBox() const { return FALSE; }

	/** Is this box a fixed positioned box? */

	virtual BOOL	IsFixedPositionedBox(BOOL include_transformed = FALSE) const { return FALSE; }

	/** Return TRUE if this is a FloatedPaneBox. */

	virtual BOOL	IsFloatedPaneBox() const { return FALSE; }

	/** Is this box a content box? */

	virtual BOOL	IsContentBox() const { return FALSE; }

	/** Is this box an option box? */

	virtual BOOL	IsOptionBox() const { return FALSE; }

	/** Is this box an option group box? */

	virtual BOOL	IsOptionGroupBox() const { return FALSE; }

	/** Is this box a dummy box with no content? */

	virtual BOOL	IsNoDisplayBox() const { return FALSE; }

	/** Is content of this box considered a containing element?

		This may e.g. be a container, table or flexbox. */

	virtual BOOL	IsContainingElement() const { return FALSE; }

	/** Is content of this box replaced content? */

	virtual BOOL	IsContentReplaced() const { return FALSE; }

	/** Is content of this inline box weak? */

	virtual BOOL	IsWeakContent() const { return FALSE; }

	/** Is content of this box image content? */

	virtual BOOL	IsContentImage() const { return FALSE; }

	/** Is content of this box embed content? */

	virtual BOOL	IsContentEmbed() const { return FALSE; }

	/** Get bullet content of this box if it has one. */

	virtual BulletContent*
					GetBulletContent() const { return NULL; }

	/** Is this box (and potentially also its children) columnizable?

		A box may only be columnizable if it lives in a paged or multicol container.

		A descendant of a box that isn't columnizable can never be columnizable (unless
		there's a new paged or multicol container between that box and its descendant).

		Table-rows, table-captions, table-row-groups and block-level descendant boxes of
		a paged or multicol container are typically columnizable. Being columnizable
		means that they need to be taken into account by the ancestor paged or multicol
		container somehow. They may become a start or stop element of a column or
		page. It may also be possible to split them over multiple columns or pages, but
		that depends on details about the box type, and that is where the
		"require_children_columnizable" parameter comes into play.

		@param require_children_columnizable If TRUE, only return TRUE if this box is of
		such a type that not only the box itself is columnizable, but also that it
		doesn't prevent its children from becoming columnizable. A table-row, block with
		scrollbars, absolutely-positioned box or float prevents children from being
		columnizable (FALSE will be returned it this parameter is TRUE), but the box
		itself may be very well be columnizable. Example: an absolutely positioned box
		may live inside of a paged or multicol container (so that its Y position is
		stored as a virtual Y position within the paged or multicol container), but its
		descendants do not have to worry about the multi-columnness or multi-pagedness.

		@return TRUE if this box (and possibly its children too) is columnizable. Return
		FALSE if this box isn't columnizable, or if require_children_columnizable is set
		and the children aren't columnizable. */

	virtual BOOL	IsColumnizable(BOOL require_children_columnizable = FALSE) const;

	/** Get HTML element this box belongs to. */

	virtual HTML_Element*
					GetHtmlElement() const = 0;

	/** Get height, minimum line height and base line. */

	virtual void	GetVerticalAlignment(InlineVerticalAlignment* valign);

	/** Propagate vertical alignment values up to nearest loose subtree root. */

	virtual void	PropagateVerticalAlignment(const InlineVerticalAlignment& valign, BOOL only_bounds) { }

	/** Create form, plugin and iframe objects */

	virtual OP_STATUS
		EnableContent(FramesDocument* doc) { return OpStatus::OK; }

	/** Mark content as disabled, will remove form content at reflow */

	virtual void	MarkDisabledContent(FramesDocument* doc) {}

    /** Remove form, plugin and iframe objects */

	virtual void	DisableContent(FramesDocument* doc) {}

	/** Stores the formobject in the HTML_Element (for backup during reflow)  */

	virtual void	StoreFormObject() {}

	/** Add option element to 'select' content */

	virtual OP_STATUS
					AddOption(HTML_Element* option_element, unsigned int &index) { index = UINT_MAX; return OpStatus::ERR; }

	/** Remove option element from 'select' content */

	virtual void	RemoveOption(HTML_Element* option_element) { }

	/** Remove optiongroup element from 'select' content */

	virtual void	RemoveOptionGroup(HTML_Element* optiongroup_element) { }

	/** Begin optiongroup in 'select' content */

	virtual void	BeginOptionGroup(HTML_Element* optiongroup_element) { }

	/** End optiongroup in 'select' content */

	virtual void	EndOptionGroup(HTML_Element* optiongroup_element) { }

	/** Layout option element in 'select' content */

	virtual OP_STATUS
					LayoutOption(LayoutWorkplace* workplace, unsigned int index) { return OpStatus::OK; }

	/** Lay out form content. */

	virtual OP_STATUS
					LayoutFormContent(LayoutWorkplace* workplace) { return OpStatus::OK; }

	/** Yield layout of this currently reflowing element. */

	virtual void	YieldLayout(LayoutInfo& info) {}

	/** Lay out children of this box. */

	LAYST			LayoutChildren(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Inline-traverse children of this box. */

	BOOL			LineTraverseChildren(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline);

	/** Get form object. */

	virtual FormObject*
					GetFormObject() const { return NULL; }

	/** Traverse box with children.

		This method will traverse this inline element and its children.  It
		will only traverse the part of the virtual line that the elements have been
		laid out on indicated by position and length. */

	virtual BOOL	LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline) { return TRUE; }

	/** Traverse the content of this box, i.e. its children. */

	virtual void	TraverseContent(TraversalObject* traversal_object, LayoutProperties* layout_props) {}

	/** Get the logical top and bottom of a loose subtree of inline boxes limited by end_position.

		A subtree is defined as 'loose' if the root has vertical-aligment 'top' or 'bottom'
		and consist of all descendants that are not loose themselves. */

	virtual BOOL	GetLooseSubtreeTopAndBottom(TraversalObject* traversal_object, LayoutProperties* cascade, LayoutCoord current_baseline, LayoutCoord end_position, HTML_Element* start_element, LayoutCoord& logical_top, LayoutCoord& logical_bottom) { return TRUE; }

	/** Remove cached info. */

	virtual BOOL	RemoveCachedInfo() { return FALSE; }

	/** Remove pending page break info. */

	virtual void	RemovePendingPagebreak() {}

	/** Clear min/max width. */

	virtual void	ClearMinMaxWidth() {}

	/** Check if box type matches given properties. */

	virtual BOOL	CheckType(LayoutProperties* cascade) const { return TRUE; }

	/** Element was skipped. Reposition if necessary, and check if it needs reflow after all. */

	virtual BOOL	SkipZElement(LayoutInfo& info);

	/** Update bottom aligned absolutepositioned boxes. */

	virtual void	UpdateBottomAligned(LayoutInfo& info) {}

	/** Is some part of this box selected */

	BOOL			GetSelected() const;

	/** Set that some part of this box is selected */

	void			SetSelected(BOOL value);

	/** Get the percent of normal width when page is shrunk in ERA. */

	virtual int		GetPercentOfNormalWidth() const { return 100; }

	/** Find the normal right edge of the rightmost absolute positioned box. */

	virtual LayoutCoord
					FindNormalRightAbsEdge(HLDocProfile* hld_profile, LayoutProperties* parent_cascade) { return LayoutCoord(0); }

	/** Get used border widths, including collapsing table borders */

	virtual void	GetBorderWidths(const HTMLayoutProperties& props, short& top, short& left, short& right, short& bottom, BOOL inner) const;

	/** Is this box traversed within a line? */

	virtual BOOL	TraverseInLine() const { return FALSE; }

	/** Has this box specified an absolute width? */

	virtual BOOL    HasAbsoluteWidth() const { return FALSE; }

	/** Skip words because they belong to a different traversal pass. Needed
		when text-align is justify. */

	void			SkipJustifiedWords(LineSegment& segment);

	/** Did the width of the containing block established by this box change
	 * during reflow?
	 * @param include_changed_before Return TRUE even if the width only
	 * changed before reflow (and not necessarily during reflow). Skipped
	 * elements need to know about this change.
	 */

	virtual BOOL	HasContainingWidthChanged(BOOL include_changed_before) { OP_ASSERT(!"Missing re-implementation of this method"); return FALSE; }

	/** Did the height of the containing block established by this box change
	 * during reflow?
	 * @param include_changed_before Return TRUE even if the height only
	 * changed before reflow (and not necessarily during reflow). Skipped
	 * elements need to know about this change.
	 */

	virtual BOOL	HasContainingHeightChanged(BOOL include_changed_before) { OP_ASSERT(!"Missing re-implementation of this method"); return FALSE; }

	/** Update root translation state for this box.

		See VerticalBox::UpdateRootTranslationState() */

	virtual BOOL	UpdateRootTranslationState(BOOL changed_root_translation, LayoutCoord x, LayoutCoord y) { return FALSE; }

	/** Should overflow clipping of this box affect clipping of the target box?

		If the target box is a descendant of an absolute positioned box whose containing element
		an ancestor of this element, it should not.

		@param target_element The target element
		@param pane_clip TRUE if we're checking column or page overflow, FALSE if
		we're checking overflow on the box itself. */

	BOOL			GetClipAffectsTarget(HTML_Element* target_elemet, BOOL pane_clip = FALSE);

	/** Invalidate fixed descendants of this Box. */

	void			InvalidateFixedDescendants(FramesDocument* doc);

	/** Should this box layout its children? */

	virtual BOOL	ShouldLayoutChildren() { return TRUE; }

	/** Handle event that occurred on this element.
	 *
	 * @param doc The document
	 * @param ev The type of event
	 * @param offset_x horizontal offset of the event relative to the content edge of the box,
	 *                 in the box's coordinate system.
	 * @param offset_y vertical offset of the event relative to the content edge of the box,
	 *                 in the box's coordinate system.
	 */

	virtual void	HandleEvent(FramesDocument* doc, DOM_EventType ev, int offset_x, int offset_y) {}

	/** Return the TableContent for the closest table ascendant of this Box. */

	TableContent*	GetTable() const;

	/** Convenience method that calls GetContainingElement for the html element of this Box. */

	HTML_Element*	GetContainingElement(BOOL absolutely_positioned = FALSE, BOOL fixed = FALSE) const { return GetContainingElement(GetHtmlElement(), absolutely_positioned, fixed); }

	/** Find the html element that forms the containing block[1] for the passed
		html element parameter.

		When a replaced content element is encountered, that element is returned
		instead. If that replaced content is an SvgContent, and the element
		parameter is an svg foreignObject, NULL is returned. If the element
		parameter is the element for the initial containing block (Markup::HTE_DOC_ROOT),
		NULL is also returned.

		Note that the positioning type of element is passed explicitly and not
		retrieved from the elements layout box. The reason is that sometimes the
		element does not have a layout box yet, and sometimes you want to find
		the containing block of an absolute positioned element as if it were
		statically positioned.

		From the spec:

		"1. The containing block in which the root element lives is a rectangle
			called the initial containing block.
		 2. For other elements, if the element's position is 'relative' or 'static',
			the containing block is formed by the content edge of the nearest block
			container ancestor box."

		This is the behaviour when called with default aguments. (1) is just a
		specialization of (2) where the initial containing block is represented
		as the Markup::HTE_DOC_ROOT parent of the root element.

		"3. If the element has 'position: fixed', the containing block is established
			by the viewport in the case of continuous media or the page area in the
			case of paged media."

		Passing fixed=TRUE.

		There's currently a twist to containing blocks for fixed positioned elements
		specified in the CSS 2D Transforms spec:

		"Any value other than 'none' for the transform results in the creation of
		 both a stacking context and a containing block. The object acts as a
		 containing block for fixed positioned descendants. Need to go into more
		 detail here about why fixed positioned objects should do this, i.e., that
		 it's much harder to implement otherwise."

		"4. If the element has 'position: absolute', the containing block is
			established by the nearest ancestor with a 'position' of 'absolute',
			'relative' or 'fixed'."

		Passing absolute=TRUE

		@param element The element to find the containing element for.
		@param absolutely_positioned The element is absolutely positioned.
		@param fixed The element is fixed positioned. When fixed is TRUE
					 absolutely_positioned must also be set to TRUE.
		@return The element that forms the containing block. NULL if there
				isn't one.

		[1] http://www.w3.org/TR/CSS2/visudet.html#containing-block-details */

	static HTML_Element*
					GetContainingElement(HTML_Element* element, BOOL absolutely_positioned = FALSE, BOOL fixed = FALSE);

	/** Get current page and number of pages.

		If this box is not a paged container, current page is defined to be 0
		and page count is 1. */

	void			GetPageInfo(unsigned int& current_page, unsigned int& page_count);

	/** Set current page number.

		If this box is not a paged container, this call has no effect. */

	void			SetCurrentPageNumber(unsigned int current_page);

	/** Set scroll offset, if applicable for this type of box / content.

		@param x If non-NULL, the new X position. If NULL, leave current X position as is.
		@param y If non-NULL, the new Y position. If NULL, leave current Y position as is. */

	virtual void	SetScrollOffset(int* x, int* y) {}

	/** TRUE when this box has list item marker pseudo element that has a box, FALSE otherwise. */

	BOOL			HasListMarkerBox() const;

	/** Return TRUE if this box is an anonymous wrapper box inserted by layout.

		This may be e.g. a table object or a flex item, inserted by the layout
		engine to fulfill some kind of box model sanity requirement. */

	BOOL			IsAnonymousWrapper() const;
};

/** Box that does not have a content box, but may have children. */

class Contentless_Box
  : public Box
{
public:

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);
};

/** Content box. */

class Content_Box
  : public Box
{
protected:

	union
	{
		/** State of reflow in box.

			When LSB is clear, it points to the html element of this box.
			Otherwise it points at the reflow state.  This is deleted
			when reflow is complete. */

		UINTPTR
					data;

		HTML_Element*
					html_element;
	} un;

	/** Pointer to content of this box. */

	Content*		content;

	/** Restart z element list. */

	virtual void	RestartStackingContext() {}

	/** Delete reflow state.

		This is called on FinishLayout(). */

	virtual void	DeleteReflowState();

	/** Clean out reflow states. */

	virtual void	CleanReflowState();

public:

					Content_Box(HTML_Element* element, Content* content)
						: content(content) { un.html_element = element; }

	virtual			~Content_Box();

	/** Returns a pointer to the content if any otherwise NULL */

	virtual Content*
					GetContent() const { return content; }

	/** Get reflow state. */

	ReflowState*	GetReflowState() const { if (un.data & 1) return (ReflowState*) (un.data & ~1); else return NULL; }

	/** Get HTML element this box belongs to. */

	virtual HTML_Element*
					GetHtmlElement() const { if (un.data & 1) return ((ReflowState*) (un.data & ~1))->html_element; else return un.html_element; }

	/** Get cached css properties */

	LayoutProperties*
					GetCascade() const { OP_ASSERT(un.data & 1); return ((ReflowState*) (un.data & ~1))->cascade; }

	virtual const HTMLayoutProperties*
					GetHTMLayoutProperties() const { OP_ASSERT(un.data & 1); return ((ReflowState*) (un.data & ~1))->cascade->GetProps(); }

	/** Signal that content may have changed. */

	virtual void	SignalChange(FramesDocument* doc, BoxSignalReason reason = BOX_SIGNAL_REASON_UNKNOWN);

	/** Mark the element of this box as dirty, and prepare for reflow. */

	void			MarkDirty(FramesDocument* doc, BOOL delete_minmax_widths=TRUE) { GetHtmlElement()->MarkDirty(doc, delete_minmax_widths); }

	/** Get width available for the margin box. */

	virtual LayoutCoord
					GetAvailableWidth(LayoutProperties* cascade);

	/** Get width of box. */

	virtual LayoutCoord
					GetWidth() const;

	/** Get height. */

	virtual LayoutCoord
					GetHeight() const;

	/** Get container established by this box, if any. */

	virtual Container*
					GetContainer() const;

	/** Get table content of this box, if it has one. */

	virtual TableContent*
					GetTableContent() const;

	/** Get flex content (also known as flexbox or flex container) established by this box, if it has one. */

	virtual FlexContent*
					GetFlexContent() const;

	/** Get selectcontent of this box, if it has one. */

	virtual SelectContent*
					GetSelectContent() const;

	/** Get scrollable, if it is one. */

	virtual ScrollableArea*
					GetScrollable() const;

	/** Get button container of this box, if it has one. */

	virtual ButtonContainer*
					GetButtonContainer() const;

#ifdef SVG_SUPPORT
	/** Get svg content of this box, if it has one. */

	virtual SVGContent*
					GetSVGContent() const;
#endif

	/** Get bullet content of this box if it has one. */

	virtual BulletContent*
					GetBulletContent() const;

	/** Does this box have inline content? */

	virtual BOOL	IsInlineContent() const;

	/** Is this box one of the table boxes or a box with table content? */

	virtual BOOL	IsTableBox() const;

	/** Is content of this box considered a containing element?

		This may e.g. be a container, table or flexbox. */

	virtual BOOL	IsContainingElement() const;

	/** Is content of this box replaced content? */

	virtual BOOL	IsContentReplaced() const;

	/** Is content of this box image content? */

	virtual BOOL	IsContentImage() const;

	/** Is content of this box embed content? */

	virtual BOOL	IsContentEmbed() const;

	/** Is this box a content box? */

	virtual BOOL	IsContentBox() const { return TRUE; }

	/** Is there typically a CoreView associated with this box ?

		@see class CoreView */

	virtual BOOL	HasCoreView() const;

	/** Propagate the right edge of absolute positioned boxes. */

	virtual void	PropagateRightEdge(const LayoutInfo& info, LayoutCoord right, LayoutCoord noncontent, LayoutFixed percentage) { /* FIXME: Missing reimplementation in many classes. */ }

	/** Do we need to calculate min/max widths of this box's content? */

	virtual BOOL	NeedMinMaxWidthCalculation(LayoutProperties* cascade) const;

	/** Propagate widths to container / table. */

	virtual void	PropagateWidths(const LayoutInfo& info, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width) {}

	/** Propagate a break point caused by break properties or a spanned element.

		These are discovered during layout, and propagated to the nearest
		multicol container when appropriate. They are needed by the columnizer
		to do column balancing.

		@param virtual_y Virtual Y position of the break (relative to the top
		border edge of this box)
		@param break_type Type of break point
		@param breakpoint If non-NULL, it will be set to the MultiColBreakpoint
		created, if any.
		@return FALSE on OOM, TRUE otherwise. */

	virtual BOOL	PropagateBreakpoint(LayoutCoord virtual_y, BREAK_TYPE break_type, MultiColBreakpoint** breakpoint) { return TRUE; }

	/** Create form, plugin and iframe objects */

	virtual OP_STATUS
					EnableContent(FramesDocument* doc);

	/** Mark content as disabled, will remove form content at reflow */

	virtual void	MarkDisabledContent(FramesDocument* doc);

	/** Remove form, plugin and iframe objects */

	virtual void	DisableContent(FramesDocument* doc);

	/** Stores the formobject in the HTML_Element (for backup during reflow)  */

	virtual void	StoreFormObject();

	/** Add option element to 'select' content */

	virtual OP_STATUS
					AddOption(HTML_Element* option_element, unsigned int &index);

	/** Remove option element from 'select' content */

	virtual void	RemoveOption(HTML_Element* option_element);

	/** Remove optiongroup element from 'select' content */

	virtual void	RemoveOptionGroup(HTML_Element* optiongroup_element);

	/** Begin optiongroup in 'select' content */

	virtual void	BeginOptionGroup(HTML_Element* optiongroup_element);

	/** End optiongroup in 'select' content */

	virtual void	EndOptionGroup(HTML_Element* optiongroup_element);

	/** Layout option element in 'select' content */

	virtual OP_STATUS
					LayoutOption(LayoutWorkplace* workplace, unsigned int index);

	/** Lay out form content. */

	virtual OP_STATUS
					LayoutFormContent(LayoutWorkplace* workplace);

	/** Get form object. */

	virtual FormObject*
					GetFormObject() const;

	/** Get min/max width for box.

		Returns FALSE if box has no min/max width (usually due to percentage
	    content_width). */

	BOOL			GetMinMaxWidth(LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width) const;

#ifdef PAGED_MEDIA_SUPPORT

	/** Insert a page break. */

	virtual BREAKST	InsertPageBreak(LayoutInfo& info, int strength) { return BREAK_NOT_FOUND; }

#endif // PAGED_MEDIA_SUPPORT

	/** Update bounding box. */

	virtual void	UpdateBoundingBox(const AbsoluteBoundingBox& box, BOOL skip_content) { }

	/** Reduce right and bottom part of relative bounding box. */

	virtual void	ReduceBoundingBox(LayoutCoord bottom, LayoutCoord right, LayoutCoord min_value) {}

	/** Calculate used vertical CSS properties (height and margins).

		Some box types need to override the regular height (and margin)
		calculation algorithms. This method will allow such boxes to do so.

		@return TRUE if height and vertical margins have been calculated. This
		means that the caller should refrain from calculating them in the
		default way. If FALSE is returned, heights and vertical margins should
		be calculated in the default way. */

	virtual BOOL	ResolveHeightAndVerticalMargins(LayoutProperties* cascade, LayoutCoord& content_height, LayoutCoord& margin_top, LayoutCoord& margin_bottom, LayoutInfo& info) const { return FALSE; }

	/** Clear min/max width. */

	virtual void	ClearMinMaxWidth();

	/** Helper function for calculating clipping area. */

	void			GetClippedBox(AbsoluteBoundingBox& bounding_box, const HTMLayoutProperties& props, BOOL clip_content) const;

	/** @see Content_Box::GetClippedBox
		Calls the above and returns the OpRect of clipped box.

		@param props The props of the box.
		@param clip_content Should we clip the content to the padding box.
		@return The clipped rect. */

	OpRect			GetClippedRect(const HTMLayoutProperties& props, BOOL clip_content) const
	{
		AbsoluteBoundingBox bounding_box;
		OpRect result;
		GetClippedBox(bounding_box, props, clip_content);
		bounding_box.GetBoundingRect(result);
		return result;
	}

	/** Get the percent of normal width when page is shrunk in ERA. */

	virtual int		GetPercentOfNormalWidth() const;

	/** Remove cached info */

	virtual BOOL	RemoveCachedInfo();

	/** Yield layout of this currently reflowing element */
	virtual void	YieldLayout(LayoutInfo& info);

	/** Traverse the content of this box, i.e. its children. */

	virtual void	TraverseContent(TraversalObject* traversal_object, LayoutProperties* layout_props);

	/** Specify whether the computed CSS height value is auto or not. */

	virtual void	SetHasAutoHeight(BOOL has_auto_height) {}

	/** Get bounding box relative to top/left border edge of this box. Overflow may include overflowing content as well as
	absolutely positioned descendants */

	virtual void	GetBoundingBox(AbsoluteBoundingBox& box, BOOL include_overflow = TRUE, BOOL adjust_for_multipane = FALSE, BOOL apply_clip = TRUE) const;

	/** Handle event that occurred on this element. */

	virtual void	HandleEvent(FramesDocument* doc, DOM_EventType ev, int offset_x, int offset_y);

	/** Should this box layout its children? */

	virtual BOOL	ShouldLayoutChildren();

	/** Apply clipping */

	void			PushClipRect(TraversalObject* traversal_object, LayoutProperties* cascade, TraverseInfo& info);

	/** Remove clipping */

	void			PopClipRect(TraversalObject* traversal_object, TraverseInfo& info);

	/** Should TraversalObject let Box handle clipping/overflow on its own?

		Some boxes, depending on traversed content, may need to differentiate
		between clipping and overflow. Clipping rectangle should include
		overflow (even if overflow is hidden) for some descendants (ie. objects
		on StackingContext) therefore clipping must be applied on-demand during
		traverse rather than in Enter*Box. */

	virtual BOOL	HasComplexOverflowClipping() const { return FALSE; }

	/** Return TRUE if the actual value of 'overflow' is 'visible'. */

	BOOL			IsOverflowVisible();

	/** Set scroll offset, if applicable for this type of box / content.

		@param x If non-NULL, the new X position. If NULL, leave current X position as is.
		@param y If non-NULL, the new Y position. If NULL, leave current Y position as is. */

	virtual void	SetScrollOffset(int* x, int* y);
};

/** Common base class for boxes with vertical layout. */

class VerticalBox
  : public Content_Box
{
protected:

	/** Initialise reflow state. */

	VerticalBoxReflowState*
					InitialiseReflowState();

	/** Bounding box. */

	RelativeBoundingBox
					bounding_box;

	/** Go through absolutely positioned descendants and schedule them for reflow as needed.

		Check if the containing block established by this box changed in such a way that
		absolutely positioned descendants need to be reflowed. */

	void			CheckAbsPosDescendants(LayoutInfo& info);

public:

					VerticalBox(HTML_Element* element, Content* content)
					  : Content_Box(element, content) {}
	virtual			~VerticalBox() { DeleteReflowState(); }

	/** Is this box a vertical box? */

	virtual BOOL	IsVerticalBox() const { return TRUE; }

	/** Get reflow state. */

	VerticalBoxReflowState*
					GetReflowState() const { return (VerticalBoxReflowState*) Content_Box::GetReflowState(); }

	/** Recalculate containing block size and schedule the specified descendant box for reflow if needed. */

	virtual void	RecalculateContainingBlock(FramesDocument* doc, AbsolutePositionedBox* box);

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE

	/** Signal that the HTML_Element associated with this layout box about to be deleted. */

	virtual void	SignalHtmlElementDeleted(FramesDocument* doc);

#endif // LAYOUT_TRAVERSE_DIRTY_TREE

	/** Paint background and border of this box on screen. */

	virtual void	PaintBgAndBorder(LayoutProperties* cascade, FramesDocument* doc, VisualDevice* visual_device) const;

	/** Update bounding box. */

	virtual void	UpdateBoundingBox(const AbsoluteBoundingBox& box, BOOL skip_content);

	/** Reduce right and bottom part of relative bounding box. */

	virtual void	ReduceBoundingBox(LayoutCoord bottom, LayoutCoord right, LayoutCoord min_value);

	/** Get width of bounding box. */

	long			GetBoundingWidth() const { return GetWidth() + bounding_box.Right(); }

	/** Get width that has overflowed to the left of the box */

	long			GetNegativeOverflow() const { return bounding_box.Left(); }

	/** Get bounding box relative to top/left border edge of this box */

	virtual void	GetBoundingBox(AbsoluteBoundingBox& box, BOOL include_overflow = TRUE, BOOL adjust_for_multipane = FALSE, BOOL apply_clip = TRUE) const;

	/** Has this box any content that overflows? */

	BOOL			HasContentOverflow() const { return !bounding_box.IsEmpty(); }

	/** Get the value to use when calculating the containing block width for
		absolutely positioned children. The value will include border and
		padding. */

	virtual LayoutCoord
					GetContainingBlockWidth();

	/** Get the value to use when calculating the containing block height for
		absolutely positioned children. The value will include border and
		padding. */

	virtual LayoutCoord
					GetContainingBlockHeight();

	/** Add an absolutely positioned box to the list of boxes that may be
	 * affected when their containing block size/position changes.
	 * @param box The layout box to add
	 * @param skipped Was layout skipped (via StackingContext::SkipElement())
	 * for this box?
	 * @return FALSE on memory allocation error, TRUE otherwise
	 */

	BOOL			AddAffectedAbsPosDescendant(AbsolutePositionedBox *box, BOOL skipped) { OP_ASSERT(IsPositionedBox()); VerticalBoxReflowState *state = (VerticalBoxReflowState *)GetReflowState(); return !state || state->abs_pos_descendants.AddAffectedBox(box, skipped); }

	/** Did the width of the containing block established by this box change
	 * during reflow?
	 * @param include_changed_before Return TRUE even if the width only
	 * changed before reflow (and not necessarily during reflow). Skipped
	 * elements need to know about this change.
	 */

	virtual BOOL	HasContainingWidthChanged(BOOL include_changed_before);

	/** Did the height of the containing block established by this box change
	 * during reflow?
	 * @param include_changed_before Return TRUE even if the height only
	 * changed before reflow (and not necessarily during reflow). Skipped
	 * elements need to know about this change.
	 */

	virtual BOOL	HasContainingHeightChanged(BOOL include_changed_before);

	/** Update root translation state for this box.

		When collapsing top margins, a box that has already been laid
		out, but hasn't been finished, may move down and may cause
		descendants to be moved down with it.

		In case there are positioned descendants, the root translation
		may be affected. The mechanism, to which
		UpdateRootTranslationState belongs, goes through the
		descendant chain using the cascade and in turn calls
		UpdateRootTranslationState on them.

		'x' and 'y' is the offset that has been applied to the normal
		translation.

		If 'changed_root_translation' is FALSE, it means that the root
		translation hasn't been updated yet; the caller hasn't found
		any affected root translation. By returning TRUE from this
		function, we signal that the root translation must be updated.

		If 'changed_root_translation' is TRUE, it means that the root
		translation already has been changed, and that descendants
		must update all state relating to the previous root
		translation.

		Returns TRUE when the root translation should be updated,
		FALSE otherwise. */

	virtual BOOL	UpdateRootTranslationState(BOOL changed_root_translation, LayoutCoord x, LayoutCoord y);

	/** Specify whether the computed CSS height value is auto or not. */

	virtual void	SetHasAutoHeight(BOOL has_auto_height) { VerticalBoxReflowState *state = (VerticalBoxReflowState *)GetReflowState(); state->has_auto_height = has_auto_height; }
};


/** Text box. */

class Text_Box
  : public Box
{
private:

	/** Html element that this box belongs to. */

	HTML_Element*	html_element;

	/** Width of box. */

	LayoutCoord		width;

	/** Position on the virtual line the box was laid out on. */

	LayoutCoord		x;

	/** Info about size and font number of words. */

	WordInfo*		word_info_array;

	union
	{
		struct
		{
			/** Cached width of space. */

			unsigned int
					word_spacing:13;

			/** Cached width of space if css :first-line pseudo element is used. */

			unsigned int
					first_line_word_spacing:13;

			/** Word info needs to be recalculated. */

			unsigned int
					remove_word_info:1;

		} packed;
		UINT32		packed_init;
	};

	union
	{
		struct
		{
			/** Number of words (and size of word_info_array) */

			unsigned short
					word_count:16;

			/** Largest underline position of the text. */

			unsigned short
					underline_position:8;

			/** Largest underline width of the text. */

			unsigned short
					underline_width:8;
		} packed2;
		UINT32		 packed2_init;
	};

	/** Allocate word_info_array, and update word_count. Precondition: empty array. */

	BOOL			AllocateWordInfoArray(unsigned int new_word_count);

	/** Delete word_info_array, and update word_count. */

	void			DeleteWordInfoArray();

public:

					Text_Box(HTML_Element* element)
					  : html_element(element),
						width(0),
						x(0),
						word_info_array(NULL)
						{ packed_init = 0; packed2_init = 0; }

	virtual			~Text_Box() { DeleteWordInfoArray(); }

	/** Get HTML element this box belongs to. */

	virtual HTML_Element*
					GetHtmlElement() const { return html_element; }

	/** Get position on the virtual line the box was laid out on. */

	virtual LayoutCoord
					GetVirtualPosition() const { return x; }

	/** Count the number of words in segment that are in this text box.

		Used for justified text.

		@param[in] segment The line segment where we count the words in.
		@param[out] words Is increased by the number of words that overlap with
						  this box and the segment.
		@return TRUE if the end of the segment was not encountered, FALSE otherwise. */

	virtual BOOL	CountWords(LineSegment& segment, int& words) const;

	/** Adjust the virtual position on the line of this box.

		The caller must be sure that the call of this method will not affect used space
		on the line and make sure the bounding box of the line will be correct after
		such operation. The method cannot be called on the boxes of elements
		that span more than one line.

		@param offset The offset to adjust the virtual position by. */

	virtual void	AdjustVirtualPos(LayoutCoord offset) { x += offset; }

	/** Get underline for elements on the specified line. */

	virtual BOOL	GetUnderline(const Line* line, short& underline_position, short& underline_width) const;

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Is this box a text box? */

	virtual BOOL	IsTextBox() const { return TRUE; }

	/** Is this box an empty text box?
	  *
	  * @return TRUE if this is a text box and it is empty.
	  *         FALSE if this isn't a text box or it contains at least one word or non-collapsed whitespace.
	  */

	virtual BOOL	IsEmptyTextBox() const { return word_info_array == NULL; }

	/** Is this text box empty or does it contain only collapsable whitespace? */

	BOOL			IsWhitespaceOnly() const { return word_info_array == NULL || (packed2.word_count == 1 && word_info_array[0].GetLength() == 0 && word_info_array[0].HasTrailingWhitespace() == 1); }

	/** Set text selection point to start of this text */

	void			SetVisualStartOfSelection(SelectionBoundaryPoint* selection
#ifdef SUPPORT_TEXT_DIRECTION
										, BOOL is_rtl_element = FALSE
#endif
										);

	/** Set text selection point to end of this text */

	void			SetVisualEndOfSelection(SelectionBoundaryPoint* selection
#ifdef SUPPORT_TEXT_DIRECTION
										, BOOL is_rtl_element = FALSE
#endif
										);

	/** Traverse box. */

	virtual BOOL	LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline);

	/** Is this box traversed within a line? */

	virtual BOOL	TraverseInLine() const { return TRUE; }

	/** Remove cached info */

	virtual BOOL	RemoveCachedInfo();

	/** Returns number of words in box. */

	int             GetWordCount() { return packed2.word_count; }

	/** Returns word array. */

	WordInfo*		GetWords() { return word_info_array; }

	/** Get width of space character. */

	LayoutCoord		GetWordSpacing(BOOL first_line) const { return LayoutCoord(first_line ? packed.first_line_word_spacing : packed.word_spacing); }

	/** Get the width of the box */

	virtual LayoutCoord
					GetWidth() const { return width; }

	/** Split a word so that the first part is less or equal 'max_width'.
		Return FALSE if first character is wider then 'max_width' */

	OP_BOOLEAN		SplitWord(short word_idx, LayoutCoord max_width, VisualDevice* vd, const HTMLayoutProperties& props, BOOL start_of_line);

	/** Get number of (non-whitespace) characters in this box. */

	int				GetCharCount() const;
};

/** Option box. */

class OptionBox
  : public Contentless_Box
{
private:

	/** Html element that this box belongs to. */

	HTML_Element*	html_element;

	/** Index of this option in select element; invalid if UINT_MAX. */

	unsigned int	index;

public:

					OptionBox(HTML_Element* element)
						: html_element(element) {}

	/** Construct and layout an OptionBox */

	OP_STATUS		Construct(Box* select_box, LayoutWorkplace* workplace);

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE

	/** Signal that the HTML_Element associated with this layout box about to be deleted. */

	virtual void	SignalHtmlElementDeleted(FramesDocument* doc);

#endif // LAYOUT_TRAVERSE_DIRTY_TREE

	/** Get HTML element this box belongs to. */

	virtual HTML_Element*
					GetHtmlElement() const { return html_element; }

	/** Is this box an option box? */

	virtual BOOL	IsOptionBox() const { return TRUE; }

	/** Lay out form content. */

	virtual OP_STATUS
					LayoutFormContent(LayoutWorkplace* workplace);

	/* Get this option's index. */

	short			GetIndex() { return index; }

	/* Update this option's index. */

	virtual void	SetIndex(short i) { index = i; }
};

/** Optiongroup box. */

class OptionGroupBox
  : public Contentless_Box
{
private:

	/** Html element that this box belongs to. */

	HTML_Element*	html_element;

public:

					OptionGroupBox(HTML_Element* element, Box* select_box);

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE

	/** Signal that the HTML_Element associated with this layout box about to be deleted. */

	virtual void	SignalHtmlElementDeleted(FramesDocument* doc);

#endif // LAYOUT_TRAVERSE_DIRTY_TREE

	virtual BOOL	IsOptionGroupBox() const { return TRUE; }

	/** Get HTML element this box belongs to. */

	virtual HTML_Element*
					GetHtmlElement() const { return html_element; }

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);
};

/** BR box. */

class BrBox
  : public Box
{
private:

	/** Html element that this box belongs to. */

	HTML_Element*	html_element;

public:

					BrBox(HTML_Element* element)
					  : html_element(element)
					  {}

	/** Get HTML element this box belongs to. */

	virtual HTML_Element*
					GetHtmlElement() const { return html_element; }

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Traverse box with children.

		This method will traverse this inline element and its children.  It
		will only traverse the part of the virtual line that the elements have been
		laid out on indicated by position and length. */

	virtual BOOL	LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline);

	/** Is this box traversed within a line? */

	virtual BOOL	TraverseInLine() const { return TRUE; }
};

/** WBR box */

class WBrBox
  : public BrBox
{
public:
					WBrBox(HTML_Element* element)
					: BrBox(element)
					{}

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Traverse box with children.

		This method will traverse this inline element and its children.  It
		will only traverse the part of the virtual line that the elements have been
		laid out on indicated by position and length. */

	virtual BOOL	LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline) { return TRUE; }

	/** Should this box layout its children? */

	virtual BOOL	ShouldLayoutChildren() { return FALSE; }
};

/** Dummy box with no content (but its descendants may have content).

	Used for Markup::HTE_TEXTGROUP elements as well as for any svg
	elements that exist between an svg fragment root and a foreignObject
	child element of that fragment root. */

class NoDisplayBox
  : public Contentless_Box
{
private:

	/** Html element that this box belongs to. */

	HTML_Element*	html_element;

public:

					NoDisplayBox(HTML_Element* element)
					  : html_element(element) {}

	/** Get HTML element this box belongs to. */

	virtual HTML_Element*
					GetHtmlElement() const { return html_element; }

	/** Traverse box with children.

		This method will traverse this inline element and its children.  It
		will only traverse the part of the virtual line that the elements have been
		laid out on indicated by position and length. */

	virtual BOOL	LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline);

	/** Is this box traversed within a line? */

	virtual BOOL	TraverseInLine() const { return TRUE; }

	/** Is this box a dummy box with no content? */

	virtual BOOL	IsNoDisplayBox() const { return TRUE; }

	/** Count the number of words in a segment that overlap with this box.
		Iterates through children's boxes passing the 'words' param,
		until some recursive call returns FALSE.

		Used for justified text.

		@param[in] segment The line segment where we count the words in.
		@param[out] words Gets increased by number of words that overlap with
						  this box and the segment.
		@return TRUE if there may be more words in the segment after this box, FALSE otherwise. */

	virtual BOOL	CountWords(LineSegment& segment, int& words) const { return CountWordsOfChildren(segment, words); }

	/** Adjust the virtual position of the children boxes that lie on the same line
		as the element that owns this NoDisplayBox logically does.

		The caller must be sure that the call of this method will not affect used space
		on the line and make sure the bounding box of the line will be correct after
		such operation. The method cannot be called on the boxes of elements
		that span more than one line.

		@param offset The offset to adjust the virtual position by. */

	virtual void	AdjustVirtualPos(LayoutCoord offset) { AdjustVirtualPosOfChildren(offset); }
};

#ifdef CSS_TRANSFORMS

/** The transform context.

	Each box type corresponding to a transformed element has a
	transform context.  The transform context is a wrapper for the
	CSSTransform class that computes the final transform from a
	transform list and a transform origin. The main difference
	between the TransformContext and the CSSTransform is that
	TransformContext knows about traversal and reflow, CSSTransforms
	does not. */

class TransformContext
{
private:
	CSSTransforms	css_transforms;

public:

	/** Allocate a reflow state for transformed boxes. */

	inline BOOL		InitialiseReflowState(ReflowState *reflow_state) {
						reflow_state->transform = OP_NEW(TransformedReflowState, (this));
						return reflow_state->transform != NULL;
					}

	/** Push transforms to traversal object.
	 * 	@return OpBoolean::IS_TRUE - ok, traverse into the transform context
	 *			OpBoolean::IS_FALSE - do not traverse into the transform context
	 *			OpBoolean::ERR_NO_MEMORY - in case of OOM. */

	OP_BOOLEAN		PushTransforms(TraversalObject *traversal_object, TranslationState &state) const;

	/** Push transforms with extra offset. For example, inline boxes need extra translation.
	 * 	@return OpBoolean::IS_TRUE - ok, traverse into the transform context
	 *			OpBoolean::IS_FALSE - do not traverse into the transform context
	 *			OpBoolean::ERR_NO_MEMORY - in case of OOM. */

	OP_BOOLEAN		PushTransforms(TraversalObject *traversal_object, LayoutCoord x, LayoutCoord y, TranslationState &state) const;

	/** Pop transforms from traversal object */

	void			PopTransforms(TraversalObject *traversal_object, const TranslationState &state) const;

	/** Push transforms, for use in reflow. */

	BOOL			PushTransforms(LayoutInfo &info, TranslationState &state) const;

	/** Pop transforms, for use in reflow. */

	void			PopTransforms(LayoutInfo &info, const TranslationState &state) const;

	/** Get current transform */

	AffineTransform GetCurrentTransform() const;

	/** Finish transforms. Called during reflow to recompute the
		transform context.

		Must be done after the box has determined its size, in order
		to be able to resolve percentage values, but before the
		bounding box is propagated to its parent.

		Returns TRUE if the computed transform changed */

	BOOL			Finish(const LayoutInfo &info, LayoutProperties *cascade, LayoutCoord box_width, LayoutCoord box_height);

	/** Apply transform to bounding box */

	void			ApplyTransform(AbsoluteBoundingBox& bounding_box);

	/** Apply transform to OpRect */

	void			ApplyTransform(OpRect& rect);
};

# define TRANSFORMED_BOX(BoxType)										\
	class Transformed##BoxType :										\
		public BoxType													\
	{																	\
		TransformContext transform_context;								\
																		\
	public:																\
		Transformed##BoxType(HTML_Element *element,						\
							 Content *content)							\
			: BoxType(element, content) {}								\
																		\
		virtual TransformContext *										\
			GetTransformContext() { return &transform_context; }		\
																		\
		virtual const TransformContext *								\
			GetTransformContext() const { return &transform_context; }	\
																		\
	}

# define TRANSFORMED_CONTENTLESS_BOX(BoxType)							\
	class Transformed##BoxType :										\
		public BoxType													\
	{																	\
		TransformContext transform_context;								\
																		\
	public:																\
		Transformed##BoxType(HTML_Element *element)						\
			: BoxType(element) {}										\
																		\
		virtual TransformContext *										\
			GetTransformContext() { return &transform_context; }		\
		virtual const TransformContext *								\
			GetTransformContext() const { return &transform_context; }	\
	}

#endif

#endif // _BOX_H_

