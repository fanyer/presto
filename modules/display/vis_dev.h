/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef _VISUAL_DEVICE_H_
#define _VISUAL_DEVICE_H_

#ifdef PAN_SUPPORT
#define FEATURE_PAN_FIX_DECBITS 16
#endif // PAN_SUPPORT

#include "modules/dochand/win.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/util/simset.h"
#include "modules/display/FontAtt.h"

#include "modules/hardcore/timer/optimer.h"
#include "modules/pi/OpPainter.h"
#include "modules/pi/OpView.h"
#include "modules/pi/OpInputMethod.h"

#include "modules/display/coreview/coreviewlisteners.h"
#include "modules/display/coreview/coreviewclipper.h"
#include "modules/display/bg_clipping.h"
#include "modules/display/vis_dev_transform.h"
#include "modules/display/opscreenpropertiescache.h"
#include "modules/style/css_all_values.h"
#include "modules/style/css_types.h"
#include "modules/style/css_gradient.h"
#include "modules/windowcommander/OpViewportController.h"
#include "modules/idle/idle_detector.h"

#ifdef VEGA_OPPAINTER_SUPPORT
#include "modules/libvega/vegafixpoint.h"
#include "modules/libvega/vegapath.h"
#endif // VEGA_OPPAINTER_SUPPORT

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
#include "modules/display/pixelscaler.h"
#endif // PIXEL_SCALE_RENDERING_SUPPORT

#include "modules/inputmanager/inputcontext.h"

#ifdef SVG_SUPPORT
#include "modules/svg/svg_external_types.h"
#endif // SVG_SUPPORT

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
#include "modules/accessibility/opaccessibleitem.h"
class AccessibleDocument;
#endif

#define TEXT_FORMAT_CAPITALIZE					0x0001
#define TEXT_FORMAT_UPPERCASE					0x0002
#define TEXT_FORMAT_LOWERCASE					0x0004
//#define TEXT_FORMAT_COLLAPSE_WHITESPACE		0x0008
#define TEXT_FORMAT_REPLACE_NON_BREAKING_SPACE	0x0010
#define TEXT_FORMAT_SMALL_CAPS					0x0020
#define TEXT_FORMAT_IS_START_OF_WORD			0x0040
#define TEXT_FORMAT_REVERSE_WORD				0x0080 // used for bidi
#define TEXT_FORMAT_BIDI_PRESERVE_ORDER         0x0100 // ditto
#define TEXT_FORMAT_REMOVE_DIACRITICS			0x0200

#define FIX_PRINTBUG_NR125313 // temporary define so branches without this fix will compile.
#define DISPLAY_AVOID_OVERDRAW

class Window;
class DocumentWindow;
class DocumentManager;
class PrintDevice;
class OpScrollbar;
class OpWindowResizeCorner;
class WidgetContainer;
class VisualDevice;
class OpBitmap;
class ImageListener;
class CoreView;
class OpTimer;
class Image;
struct Border;
struct BorderEdge;
struct BorderImage;
struct BorderImageRects;
struct Shadow;
struct BackgroundProperties;
class HTMLayoutProperties;
#ifdef FEATURE_SCROLL_MARKER
class OpScrollMarker;
#endif //FEATURE_SCROLL_MARKER
struct SORTED_RECT;
struct OUTLINE_SCANLINE;
class VDSpotlight;
class VisualDeviceOutline;

/** Info about background, sent as parameter to background drawing functions in VisualDevice. */
struct BG_OUT_INFO {
	HTML_Element* element;
	const Border* border;
	const BackgroundProperties* background;
	const BorderImage* border_image;
	BOOL has_border_left, has_border_top, has_border_right, has_border_bottom;
	BOOL has_inset_box_shadow;
	short image_rendering;
};


/* Preprocessed border image rects, ready for painting.
 *
 * Destination coordinates are document coordinates relative
 * to the border box of the element.
 */

struct BorderImageRects
{
	/* Corner rectangles are represented with index 0 at top/left corner and continue clockwise. */

	/* Corner rectangles to cut out from the source image. */
	OpRect corner_source[4];
	/* Destinations for the corners. */
	OpRect corner_destination[4];


	/* Edge rectangles start with the top edge at index 0 and continue clockwise. */

	/* Edge rectangles to cut out from the source image. */
	OpRect edge_source[4];
	/* Destination rectangles for the edges. */
	OpRect edge_destination[4];
	/* CSS values representing the scaling of the respective edges (from the border-image-repeat property). */
	short edge_scale[4];
	/* The width of each tile in the destination output. */
	double edge_used_part_width[4]; // Not used when scale is CSS_VALUE_stretch
	/* The height each tile in the destination output. */
	double edge_used_part_height[4]; // Not used when scale is CSS_VALUE_stretch
	/* The start offset for tiling. For top and bottom edges this is a horizontal offset. For left and right edges it is a vertical offset. */
	int edge_start_ofs[4]; // Not used when scale is CSS_VALUE_stretch


	/* Metrics for the fill (center) part. */

	/* Source rectangle for the fill part. */
	OpRect fill;
	/* Destination rectangle for the fill part. */
	OpRect fill_destination;
	/* Horizontal start offset for tiling. */
	int fill_start_ofs_x;
	/* Vertical start offset for tiling. */
	int fill_start_ofs_y;
	/* Width of a destination tile. */
	double fill_part_width;
	/* Height of a destination tile. */
	double fill_part_height;
};

BOOL HasBorderRadius(const Border *border);
BOOL HasBorderImage(const BorderImage* border_image);
#define IS_BORDER_ON(be) (be.style != CSS_VALUE_none && be.width)
#define IS_BORDER_VISIBLE(be) (be.style != CSS_VALUE_none && be.width && be.color != CSS_COLOR_transparent)

/** Used to get darker and lighter color for 3d borders. */
void Get3D_Colors(COLORREF col, COLORREF &color_dark, COLORREF &color_light);

#ifdef VEGA_OPPAINTER_SUPPORT

class VEGAPath;

/** Helper class for creation of VEGAPath objects for use to draw borders and background with radius and box-shadow. */

class RadiusPathCalculator
{
public:
	VEGA_FIX scale;
	VEGA_FIX left_radius_start;
	VEGA_FIX left_radius_stop;
	VEGA_FIX top_radius_start;
	VEGA_FIX top_radius_stop;
	VEGA_FIX right_radius_start;
	VEGA_FIX right_radius_stop;
	VEGA_FIX bottom_radius_start;
	VEGA_FIX bottom_radius_stop;
public:
	RadiusPathCalculator(int box_width, int box_height, const Border *border, VEGA_FIX scale);

	void MakeBorderPath(VEGAPath *p, const Border *border, UINT16 border_id, VEGA_FIX x, VEGA_FIX y, VEGA_FIX width, VEGA_FIX height, VEGA_FIX border_width_extra = 0);
	void MakeBackgroundPath(VEGAPath *p, const Border *border, VEGA_FIX x, VEGA_FIX y, VEGA_FIX width, VEGA_FIX height);
	void MakeInvertedPath(VEGAPath *p, const Border *border, VEGA_FIX x, VEGA_FIX y, VEGA_FIX width, VEGA_FIX height);
	void MakeInvertedPath(VEGAPath *p, const Border *border, VEGA_FIX x, VEGA_FIX y, VEGA_FIX width, VEGA_FIX height,
										VEGA_FIX bounding_x, VEGA_FIX bounding_y, VEGA_FIX bounding_width, VEGA_FIX bounding_height);
	void MakeBackgroundPathInsideBorder(VEGAPath *p, const Border *border, VEGA_FIX x, VEGA_FIX y, VEGA_FIX width, VEGA_FIX height);
	void MakeInvertedPathInsideBorder(VEGAPath *p, const Border *border, VEGA_FIX x, VEGA_FIX y, VEGA_FIX width, VEGA_FIX height);

	/** Change any radius in border to compensate for edge offsets (If you shrink the rectangle, the radius also has to shrink to align properly).
		@param border The border in which the radius should change
		@param left The left edge offset
		@param top The top edge offset
		@param right The right edge offset
		@param bottom The bottom edge offset */
	static void InsetBorderRadius(Border *border, short left, short top, short right, short bottom);
};

#endif // VEGA_OPPAINTER_SUPPORT

/** Just holds info about a effect.
	Currently only blur. But more effects with other parameters might be added in the future.
	Used on backbuffer for the text-shadow feature (CSS3) in BeginEffect and EndEffect.
*/

class DisplayEffect
{
public:
	enum EFFECT_TYPE {
		EFFECT_TYPE_NONE,
		EFFECT_TYPE_BLUR,
		EFFECT_TYPE_SVG_FILTER
	};
#ifdef SVG_SUPPORT
	DisplayEffect(EFFECT_TYPE type, SVGURLReference ref, FramesDocument* doc) : type(type), ref(ref), doc(doc) {}
#endif // SVG_SUPPORT
	DisplayEffect(EFFECT_TYPE type, short radius) : type(type), radius(radius), doc(NULL) {}
	DisplayEffect(const DisplayEffect& de) : type(de.type), radius(de.radius),
#ifdef SVG_SUPPORT
											 ref(de.ref),
#endif // SVG_SUPPORT
											 doc(de.doc) {}
private:
	friend class VisualDevice;
	EFFECT_TYPE type;
	short radius;
#ifdef SVG_SUPPORT
	SVGURLReference ref;
#endif // SVG_SUPPORT
	FramesDocument* doc;
};

class BoxShadowCorner : public ListElement<BoxShadowCorner>
{
public:
	BoxShadowCorner();
	~BoxShadowCorner();

	OpBitmap*	bitmap;
	OpRect		shadow_rect;
	Border		border;

	int			blur_radius;
	BOOL		inset;
	COLORREF	color;
	int			scale;
};

#ifdef DISPLAY_SPOTLIGHT

/** Contains information about a part of a spotlight. */

class VDSpotlightInfo
{
public:
	VDSpotlightInfo() : color_fill(0), color_grid(0), color_inner_frame(0) {}

	void EnableFill(COLORREF color)			{ color_fill = color; }
	void EnableGrid(COLORREF color)			{ color_grid = color; }
	void EnableInnerFrame(COLORREF color)	{ color_inner_frame = color; }
	void SetRect(const OpRect &rect)		{ this->rect = rect; }

	BOOL HasFill() { return color_fill != 0; }
	BOOL HasGrid() { return color_grid != 0; }
	BOOL HasInnerFrame() { return color_inner_frame != 0; }
private:
	OP_STATUS Paint(VisualDevice *vd, VDSpotlightInfo *exclude_from_fill);
	friend class VDSpotlight;
	OpRect rect;
	COLORREF color_fill;
	COLORREF color_grid;
	COLORREF color_inner_frame;
};

/** Contains information about a spotlight that should be drawed on top of other content. */

class VDSpotlight
{
public:
	VDSpotlightInfo margin;
	VDSpotlightInfo border;
	VDSpotlightInfo padding;
	VDSpotlightInfo content;
	short props_margin[4]; // left, top, right, bottom
	short props_border[4]; // left, top, right, bottom
	short props_padding[4]; // left, top, right, bottom

	VDSpotlight();

	void SetProps(const HTMLayoutProperties& props);
	void SetID(unsigned int id) { this->id = id; }
private:
	friend class VisualDevice;
	/** Paint the outline of the combined rectangles in the region. */
	OP_STATUS Paint(VisualDevice *vd);
	OpRect GetBoundingRect(VisualDevice *vd);
	void SetElement(HTML_Element *element) { this->element = element; }
	void UpdateInfo(VisualDevice *vd, VisualDeviceOutline *outline);
	HTML_Element *element;
	unsigned int id;
};

#endif // DISPLAY_SPOTLIGHT

/** Type of text selection highlight that is being drawn. */
enum VD_TEXT_HIGHLIGHT_TYPE {
	VD_TEXT_HIGHLIGHT_TYPE_SELECTION,
	VD_TEXT_HIGHLIGHT_TYPE_ACTIVE_SEARCH_HIT,
	VD_TEXT_HIGHLIGHT_TYPE_SEARCH_HIT,
	VD_TEXT_HIGHLIGHT_TYPE_NOT_USED ///< Means that the VDTextHighlight object is not used.
};

/** Contains additional data needed on outlines that track search matches. */
class VDTextHighlight
{
public:
	VDTextHighlight() : m_type(VD_TEXT_HIGHLIGHT_TYPE_NOT_USED) {}

	VD_TEXT_HIGHLIGHT_TYPE m_type;
};

/** Backbuffer for layered drawing operations. (Opacity, effects and SVG's foreign object).
	Used by VisualDevice to temporarily redirect painting to a buffer, and when done manipulate that buffer and blit it to screen. */

class VisualDeviceBackBuffer : public Link
{
public:
	VisualDeviceBackBuffer();
	~VisualDeviceBackBuffer();

	OP_STATUS InitBitmap(const OpRect& rect, OpPainter* painter, int opacity, BOOL replace_color, BOOL copy_background = TRUE);

	OpBitmap* GetBitmap() { return bitmap; }
	OpRect GetBitmapRect() { return bitmap_rect; }

private:
	friend class VisualDevice;
	OpRect			rect;
	OpRect			doc_rect;
	int				opacity;
	OpBitmap*		bitmap;
	OpPainter*		old_painter;
	BOOL			has_background;
	BOOL			replace_color;
	BOOL			use_painter_opacity;

	DisplayEffect	display_effect;

	// Offset for painting in buffer. (backbuffers position relative to "real" buffer)
	int				ofs_diff_x;
	int				ofs_diff_y;

	/**
	   TRUE if buffer is in OOM fallback mode. this means the buffer
	   has no painter altough painter is not in opacity state due to
	   asuccessfull call to BeginOpacity. instead, a pre-alpha is set
	   in the painter, and used for drawing operations. this is not
	   perfect, but gives better results than having no fallback.
	 */
	BOOL			oom_fallback;

	// Rect in the bitmap where the user of this backbuffer should paint.
	OpRect			bitmap_rect;
};

/** Some drawing state info used in VisualDevice::PushState and VisualDevice::PopState.
*/

class VDState
{
public:
	VDState() {}
	VDState(const VDState& state)	{	translation_x = state.translation_x;
										translation_y = state.translation_y;
										color = state.color;
										char_spacing_extra = state.char_spacing_extra;
										logfont = state.logfont; }
	int				translation_x;
	int				translation_y;
	COLORREF		color;
	int				char_spacing_extra;
	FontAtt			logfont;
};

/** Some state info about things needed to restore scale properly in EndNoScalePainting.
	It also contains a scaled and transformed destination-rectangle of the rect passed into BeginNoScalePainting.
*/

class VDStateNoScale
{
public:
	OpRect dst_rect; ///< The new destination rectangle to be used. Scaled and transformed for no scaling from the rect passed in to BeginNoScalePainting.
	int old_scale;
	OpRect old_rendering_viewport;
	int old_translation_x;
	int old_translation_y;
	OpRect old_doc_display_rect;
	OpRect old_doc_display_rect_not_enlarged;
};

#ifdef CSS_TRANSFORMS
/** State saved when starting transformed drawing */
class VDStateTransformed
{
public:
	int old_translation_x;
	int old_translation_y;
};
#endif // CSS_TRANSFORMS

/** State saved when starting drawing where translation or offset shouldn't be applied. */
class VDStateNoTranslationNoOffset
{
public:
	int old_translation_x;
	int old_translation_y;
	int old_offset_x;
	int old_offset_y;
	int old_view_x_scaled;
	int old_view_y_scaled;
};

/** Holds a region with the rectangles and info for a outline.
	VisualDevice is fed with outlines from layout via BeginOutline, PushOutlineRect and EndOutline so they can be painted later.
	Only the outline of the combined rects will be outlined.
*/

class VisualDeviceOutline : public Link
{
public:
	/** Used in addition to pen_style to give further info about the type.
		F.ex, if the pen is CSS_VALUE__o_highlight_border, this style is used
		to specify type of skin that should be used. */
	enum OUTLINE_TYPE {
		OUTLINE_TYPE_NORMAL,				///< Normal outline. This type of outline is removed between repaints.
		OUTLINE_TYPE_SPOTLIGHT,				///< Spotlight highlight (dragonfly debugging). This type of outline will be kept alive between repaints.
		//OUTLINE_TYPE_NAVIGATION_GUIDE		///< For spatnav guide arrows and such
	};

	VisualDeviceOutline(int width, COLORREF color, int pen_style, int offset, HTML_Element *element) :
						m_width(width), m_color(color),
						m_pen_style(pen_style), m_offset(offset), m_ignore(FALSE),
						m_contains_image(FALSE), m_contains_text(FALSE), m_need_update_on_scroll(FALSE), m_element(element)
#ifdef DISPLAY_SPOTLIGHT
						, m_spotlight(NULL)
#endif
						{}

	/** Add a rectangle to this outline region */
	OP_STATUS AddRect(const OpRect& rect);

	/** Extend bounding rect. Make sure the bounding box in the region includes rect. */
	void ExtendBoundingRect(const OpRect& rect);

	/** Paint the outline of the combined rectangles in the region. */
	OP_STATUS Paint(VisualDevice *vd);

	/** Get the bounding box of the region. */
	OpRect GetBoundingRect() { return m_region.bounding_rect; }

	/** Returns TRUE if there is no rectangles in this region. */
	BOOL IsEmpty() { return m_region.num_rects == 0; }

	/** Return the pen width of the outline. */
	int GetPenWidth() { return m_width; }

	/** Return the pen style of the outline */
	int GetPenStyle() { return m_pen_style; }

	/** Return the offset of the outline from the inside (positive is outwards from the center). */
	int GetOffset() { return m_offset; }

	/** Return the element for this outline. DO NOT follow this pointer! Use it only as a ID to match different outlines. */
	HTML_Element* GetHtmlElement() { return m_element; }

	/** Set the ignore flag to TRUE. This outline will not be painted. */
	void SetIgnore() { m_ignore = TRUE; }

	/** Set if the outlined element contains images */
	void SetContainsImage() { m_contains_image = TRUE; }

	/** Set if the outlined element contains text */
	void SetContainsText() { m_contains_text = TRUE; }

	/** Get the region */
	BgRegion *GetRegion() { return &m_region; }

	/** Is the outline overlapping the edge of the viewport on the VisualDevice. */
	BOOL IsOverlappingEdge(VisualDevice *vis_dev);

	/** If this outline needs to be repainted after scroll. Typically because the look depends on where it is on screen. */
	void SetNeedUpdateOnScroll(BOOL need_update_on_scroll) { m_need_update_on_scroll = need_update_on_scroll; }
	BOOL NeedUpdateOnScroll() { return m_need_update_on_scroll; }

#ifdef DISPLAY_SPOTLIGHT
	/** Set the spotlight associated with this outline. */
	void SetSpotlight(VDSpotlight *spotlight) { m_spotlight = spotlight; }
	VDSpotlight *GetSpotlight() { return m_spotlight; }
#else
	VDSpotlight *GetSpotlight() { return NULL; }
#endif
	void SetTextHighlight(VD_TEXT_HIGHLIGHT_TYPE highlight) { m_texthighlight.m_type = highlight; }
	VDTextHighlight *GetTextHighlight() { return m_texthighlight.m_type == VD_TEXT_HIGHLIGHT_TYPE_NOT_USED ? NULL : &m_texthighlight; }

private:
	/** Draw vertical or horizontal outline lines and corners. */
	void PaintOutlineScanlines(VisualDevice *vd, SORTED_RECT *order_x, SORTED_RECT *order_y, OUTLINE_SCANLINE *scanline, int num_sorted, BOOL x_is_y);
	/** If a skinned outline should be drawn using the image style element. */
	BOOL ShouldDrawImageStyle() { return m_contains_image && !m_contains_text; }

	int m_width;		///< Pen Width of the outline
	COLORREF m_color;	///< Pen Color of the outline
	int m_pen_style;	///< Pen style of the outline
	int m_offset;		///< Offset of the outline from the inside
	BgRegion m_region;	///< Region containing all rectangles.
	BOOL m_ignore;		///< If TRUE, we should not paint this outline.
	BOOL m_contains_image; ///< If TRUE, the outline contains image content.
	BOOL m_contains_text; ///< If TRUE, the outline contains text content.
	BOOL m_need_update_on_scroll; ///< See SetNeedUpdateOnScroll
	HTML_Element *m_element; ///< Used as a unique ID to reopen closed outlines, the pointer is never followed.
#ifdef DISPLAY_SPOTLIGHT
	VDSpotlight *m_spotlight;
#endif
	VDTextHighlight m_texthighlight;
};

/** The Visual Device.
    VisualDevice's main tasks:
     -It's the canvas that Opera draws on.
	 -Handles mouseinput.
	 -Handles scrollbars and scrolling/panning.
	 -Contains CoreViews that gets event about painting and input from the OpView interfaces.
	 -Handles scaling of the graphics and input for operas Zoom feature.
	  Drawing functions is in "document coordinates" if not documented something else.
	  They will automatically be converted to screen coordinates from current translation, scroll and offsets.
	 -Handles caching, drawing and updating of CSS outline.

	Coordinates:
	 "Document coordinate" is a nonscaled coordinate.
	 "Screen coordinate" is a scaled coordinate as it will be displayed on screen.
	 Coordinates can be relative to top of document or top of view or current translation.

	Translation:

	  The painting is always offset with the current translation. Layout and widgets use translation so that each layoutbox/widget has it's
	  painting relative to itself. (So painting something at 0, 0 in a box at 100, 100 will end up at 100, 100 because of current translation).

	  Translation is in document coordinates.
	  Read translation with GetTranslationX(), GetTranslationY()
	  Set translation with Translate().

	Scroll:

	  With three viewports (visual, layout and rendering viewport) the term "scrolling"
	  becomes a bit ambiguous. But here's the rule: Whenever scrolling is mentioned
	  together with VisualDevice, we are referring to the rendering viewport.

	  See http://projects/Core2profiles/specifications/mobile/rendering_modes.html#viewports

	  The painting is always offset with the current scroll.

	  Scroll is in document coordinates.

	  Read scrolling with GetRenderingViewX(), GetRenderingViewY() or GetRenderingViewport().

	  Set scrolling with SetRenderingViewPos(). This is typically done from
	  ViewportController on behalf of the UI. When core wants to scroll on its own,
	  FramesDocument's RequestSetVisualViewport() or RequestSetVisualViewPos() should be
	  used, which will leave it to the OpViewportListener to update rendering view
	  position as needed.

	  Note on painting:
	  The painting functions should scale first, and then apply scrolling. Otherwise the scrolling will have a effect on the pixelsnap caused by
	  scalerounding to nearest pixels and cause artifacts when scrolling.

	Offset:

	  The painting is always offsetted with the current offset.
	  The CoreView's of a VisualDevice may be OpView-less. offset_x and offset_y will then be the offset that has to be added before
	  painting anything with the OpPainter. It is the offset of the CoreView relative to the OpPainter's origo (which is the OpView).
	  This offset is in screen coordinates, so it should be added after the scaling.

	Outline:

	  The layout engine calls BeginOutline and EndOutline to signal if a element with outline is being traversed for painting.
	  In between those calls it will call PushOutlineRect for each rectangle that should be included in the outline.
	  The job for VisualDevice is to cache theese rects, and combine them to a region that is drawn after all layout has been drawn.
	  The type of content that is being traversed might also affect how the outline looks (See SetContentFound()).

	  The other job VisualDevice has to do with outlines, is to keep them updated on screen. If f.ex the element is moved/removed due to reflow,
	  layout will only update the elements area. VisualDevice must extend update areas with intersecting outlines so that there is no risk
	  of outlines being partly left on screen. All currently visible outlines are cached in VisualDevice so this can be done, and they are
	  removed when scrolled out of view, or in areas that will just be repainted by layout (so layout will add them to us again if still there).

	Avoid overdraw:

	  Avoid overdraw is a optimization handled by VisualDevice and some calls from layout.
	  The task is to avoid drawing large background areas that is covered by other large background areas, and thus didn't need to be
	  drawn in the first place.

	  Calls to BgColorOut, BgImgOut and BgGradientOut will put backgrounds to draw in a list, and perform some clipping on earlier backgrounds. The result
	  is that the backgrounds arent't drawn immediately, and that it might paint a lot less pixels in the end.
	  In some situations it must be guaranteed that earlier backgrounds are drawn, and that is forced by FlushBackgrounds.
	  CoverBackground is function that can minimize the area that needs to be flushed in those cases when we are forced to flush backgrounds
	  somewhere on screen.
*/

class VisualDevice : public MessageObject, public OpInputContext
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	, public OpAccessibleItem
#endif
#ifdef PLATFORM_FONTSWITCHING
	, public OpAsyncFontListener
#endif
	, public VisualDeviceTransform
{
public:
	enum ScrollType {
		VD_SCROLLING_NO = 0,	///< Scrollbars should always be insivible.
		VD_SCROLLING_YES,		///< Scrollbars should always be visible.
		VD_SCROLLING_AUTO		///< Scrollbars should turn on/off when needed.
		//VD_SCROLLING_HOR_AUTO
	};
protected:
	/** Offset of CoreView of this VisualDevice relative to the CoreViewContainer. In screen coordinates. */
	int				offset_x;
	int				offset_y;

	/** Extra Offset that should be added to offset_x and offset_y.
		Used to translate backbuffers positions created by f.ex opacity-support. In screen coordinates. */
	int				offset_x_ex;
	int				offset_y_ex;

	OP_STATUS		Init(OpWindow* parentWindow, DocumentManager* doc_man, ScrollType scrolltype);

	/** Initialize the visualdevice.
		Either parentWindow or parentview must be set!
		@param parentWindow a pointer to the OpWindow in which the view should be. If the visual device is a frame, then this parameter should be NULL.
		@param parentview a pointer to the CoreView in which the view should be.
	 */
	OP_STATUS		Init(OpWindow* parentWindow, CoreView* parentview = NULL);

#ifdef PAN_SUPPORT
	/**
	   called from PanMouseDown - initializes state and sets panning mode to MAYBE
	   @param x the x coord of the mouse - screen coords
	   @param y the y coord of the mouse - screen coords
	*/
	void StartPotentialPanning(INT32 x, INT32 y);
	/**
	   starts panning - mouse move events will move document
	   @param ow if non-NULL this window will be used to change mouse cursor
	*/
	void StartPanning(OpWindow* ow = 0);
	/**
	   called from PanMouseUp
	   @param ow if non-NULL this window will be used to restore mouse cursor
	*/
	void StopPanning(OpWindow* ow = 0);
#endif // PAN_SUPPORT

public:

	/** Update offset_x, offset_y */
	void UpdateOffset();

	/** Create a VisualDevice under parentWindow. */
	static VisualDevice* Create(OpWindow* parentWindow, DocumentManager* doc_man, ScrollType scrolltype);

	/** Create a VisualDevice under parentview. */
	static VisualDevice* Create(DocumentManager* doc_man, ScrollType scrolltype, CoreView* parentview);

					VisualDevice();
	virtual			~VisualDevice();

#ifdef PAN_SUPPORT
	BOOL PanDocument(int dx, int dy);
	/**
	 * called from OpWidget::TriggerOnMouseDown, in order to give the pan state a chance to
	 * decide if it wants panning on the widget after the widget is hooked but before it
	 * reveives mouse down.
	 */
	void PanHookedWidget();
	/**
	 * Register the current mouse position and prepares for possible panning
	 * after a specific threshold of moved pixels.
	 *
	 * @param sp mouse position in top-level VisualDevice coordinates, relative to rendering viewport.
	 * @param keystate The keyboard modifier keys that were pressed when the mouse button was pressed.
	 */
	void PanMouseDown(const OpPoint& sp, ShiftKeyState keystate);
	/**
	 * Register the current mouse position and prepares for possible touch panning
	 * after a specific threshold of moved pixels.
	 *
	 * @param sp mouse position in top-level VisualDevice coordinates, relative to rendering viewport.
	 */
	void TouchPanMouseDown(const OpPoint& sp);
	/**
	 * should be called from mouse listener on mouse move.
	 *
	 * @param sp mouse position in top-level VisualDevice coordinates, relative to rendering viewport.
	 * @param input_context the (first) input context to receive any pan action
	 * @param ow if non-zero, mouse pointer will be restored for the window.
	 * @return if TRUE, mouse down is swallowed by pan code and should not be used
	 * to trigger mouse event.
	 */
	BOOL PanMouseMove(const OpPoint& sp, class OpInputContext* input_context, class OpWindow* ow = 0);
	/**
	 * should be called from mouse listener on mouse up.
	 *
	 * @param ow if non-zero, mouse pointer will be restored for the window.
	 * @return if TRUE, mouse up is swallowed by pan code and should not be used
	 * to trigger mouse event.
	 */
	BOOL PanMouseUp(class OpWindow* ow = 0);
	// Updates the current mouse position.
	void SetPanMousePos(INT32 x, INT32 y);
	// returns the accumulated lost mouse delta
	INT32 GetPanMouseAccX();
	INT32 GetPanMouseAccY();
	// returns the document coordinate delta between the current mouse pos and the position when the last pan was
	// performed, taking into account any accumulated lost mouse delta
	INT32 GetPanDocDeltaX();
	INT32 GetPanDocDeltaY();
	// returns the difference between the current mouse position and the position when the last pan was performed
	INT32 GetPanMouseDeltaX();
	INT32 GetPanMouseDeltaY();
	// updates the mouse position of the last performed pan. called after successfull panning.
	void SetPanPerformedX();
	void SetPanPerformedY();
	BOOL3 GetPanState() { return isPanning; }
	BOOL IsPanning() { return isPanning == YES; }
#endif // PAN_SUPPORT

	/**
		If timed is TRUE, the area will be updated after a short delay instead of immediately. For use by layoutengine to decrease
		unneccesary updates that may occure on some pages. All timed updates that occure within that time will be updated at the same time.
		x, y, iwidth, iheight are in document coordinates. iwidth and/or iheight may be INT_MAX, which means "infinite".
	*/
	virtual void	Update(int x, int y, int width, int height, BOOL timed = FALSE);
	virtual void	Update(const OpRect& r, BOOL timed = FALSE) { Update(r.x, r.y, r.width, r.height, timed); }

	/** Update everything */
	virtual void	UpdateAll();

	/** Does the same as Update, but translates the coordinates with the current translation. */
	void			UpdateRelative(int x, int y, int width, int height, BOOL timed = TRUE)
	{
		OpRect r = ToBBox(OpRect(x, y, width, height));
		Update(r.x, r.y, r.width, r.height, timed);
	}

	/** Paint this VisualDevice on its current position, using the painter provided by vis_dev.
		rect is the rect that should be painted. It is relative to the parent VisualDevice's view in document coordinates. */
	void			PaintIFrame(OpRect rect, VisualDevice* vis_dev);

	/** Paint this VisualDevice on its current position, using the painter provided by vis_dev.
		rect is the rect that should be painted. It is relative to this VisualDevice in view coordinates. */
	void			Paint(OpRect rect, VisualDevice* vis_dev);

	/** Callback that paints the document (or default color if no document is set).
		Should never be called directly, only come as a result of Paint. */
	void			OnPaint(OpRect rect, OpPainter* painter);

	/** Use BeginPaint(painter, rect, paint_rect) instead! */
	void			DEPRECATED(BeginPaint(OpPainter* painter));

	/** Should be called to prepare for display with a external painter. (Not the ordinary one from OpView).
		Must not be called if the visualdevice is already painting. */
	void			BeginPaint(OpPainter* painter, const OpRect& rect, const OpRect& paint_rect);
	/** Like BeginPaint(painter, rect, paint_rect), but also takes an
		initial CTM that will be set on the visualdevice */
	void			BeginPaint(OpPainter* painter, const OpRect& rect, const OpRect& paint_rect, const AffinePos& ctm);
	/** Should be called after BeginPaint, when painting is done. */
	void			EndPaint();

	/** Should be called only by painter when OnMoved event is raised.
		Performs some actions when an external view is moved (for now repaints scrollbars) */
	void            OnMoved();

	/** Will prevent Sync of any CoreView belonging to the CoreViewContainer. */
	void			BeginPaintLock();
	void			EndPaintLock();

	/** Should be called after a reflow of the document. Will reset some optimizationflags because we don't know how the current layout is, yet. */
	void			OnReflow();

	/** Checks if this VisualDevice belongs to a overlapped iframe and updates the CoreViews overlapped status. */
	void			CheckOverlapped();

	/**
	 * Check how much of the text that can fit within max_width pixels with the current font and given text_format.
	 * The number of characters that fit is returned in result_char_count and the used width is returned in result_width.
	 */
	void			GetTxtExtentSplit(const uni_char *txt, unsigned int len, unsigned int max_width, int text_format, unsigned int &result_width, unsigned int &result_char_count);

	int				GetTxtExtent(const uni_char* txt, int len);
	void			TxtOut(int x, int y, const uni_char* txt, int len, short word_width = -1);
#ifdef DISPLAY_INTERNAL_DIACRITICS_SUPPORT
	/** Check if a UnicodePoint is a diacritic that we know how to handle in the internal diacritics
		drawing support. */
	static BOOL		IsKnownDiacritic(UnicodePoint up);
	void			DrawDiacritics(OpPainter* painter, int x, int y, const uni_char* str, int len, int glyph_width, int glyph_top, int glyph_bottom, int following_glyph_top, int following_glyph_width);
	void			TxtOut_Diacritics(int x, int y, const uni_char* txt, int len, BOOL isRTL);
#endif // DISPLAY_INTERNAL_DIACRITICS_SUPPORT
#ifdef OPFONT_GLYPH_PROPS
	/**
	 fetches the positional properties of the glyph representing the character ch using the current font.
	 The values will be in document coords. The will be scaled back from screen coords, unless
	 we are in true zoom mode (scaling according to layout true zoom scale is possible instead)
	 or inside some transform.
	 @param up the character whose glyph dimentions are to be obtained
	 @param props (out) will be filled with the properties of the glyph (scaled to document coords)
	 @return OpStatus::OK, OpStatus::ERR if the glyph's props couldn't be fetched
			 OpStatus::ERR_NO_MEMORY in case of OOM.
	 */
	OP_STATUS		GetGlyphProps(const UnicodePoint up, OpFont::GlyphProps* props);
#endif // OPFONT_GLYPH_PROPS

	int				MeasureNonCollapsingSpaceWord(const uni_char* word, int len, int char_spacing_extra);
	int				GetTxtExtentEx(const uni_char* txt, size_t len, int format_option);
	int				GetTxtExtentToEx(const uni_char* txt, size_t len, size_t to, int format_option);
	int				TxtOutSmallCaps(int x, int y, uni_char* txt, size_t len, BOOL draw = TRUE, short word_width = -1);
	void			TxtOutEx(int x, int y, const uni_char* txt, size_t len, int format_option, short word_width = -1);

#ifndef FONTSWITCHING
	int				GetTxtExtentEx(const uni_char* txt, int len, BOOL ws2space, BOOL collapse_ws);
	void			TxtOutEx(int x, int y, uni_char* txt, int len, BOOL ws2space, BOOL collapse_ws, short word_width = -1);
#endif

	void			DEPRECATED(ImageAltOut(int x, int y, int width, int height, const uni_char* txt, int len, OpFontInfo::CodePage preferred_codepage));
	void			ImageAltOut(int x, int y, int width, int height, const uni_char* txt, int len, WritingSystem::Script script);

	/**
		Blits img tiled inside dst. offset is the offset inside the img which the blit should start on.
		imgscale_x and imagescale_y has nothing to do with the currect zoom of the visualdevice. (It is a extra scale made for ERA
		and background-size. It scales the sourcebitmap, not the destination rect.)
		Applies image interpolation based on the value of image_rendering and the 'InterpolateImages'
		preference.
	*/
	OP_STATUS		ImageOutTiled(Image& img, const OpRect& dst, const OpPoint& offset, ImageListener* image_listener, double imgscale_x, double imgscale_y, int imgspace_x, int imgspace_y, BOOL translate = TRUE, short image_rendering = CSS_VALUE_auto);

	class ImageEffect {
	public:
		ImageEffect() : effect(0), effect_value(0), image_listener(0) {}
		INT32 effect;
		INT32 effect_value;
		ImageListener* image_listener;
	};

	/**
	   Configure and set the painters settings for image quality/interpolation.

	   Selects an interpolation mode for the painter based on the
	   'InterpolateImages' preference and the value passed in (which
	   is expected to originate from the 'image-rendering' CSS
	   property). The preference overrides any higher quality
	   argument. Methods that take a image_rendering argument handle
	   this themselves. There should generally be no need to call this
	   method unless you do something special.

	   @param image_rendering preferred way to perform interpolation of images.
	 */
	void			SetImageInterpolation(short image_rendering);

	/**
	   Same as SetImageInterpolation(short image_rendering), but
	   factors in the destination rectangle and decoding status of
	   the image.
	*/
	void			SetImageInterpolation(const Image& img, const OpRect& dest, short image_rendering);

	/** Reset painter quality settings to the pref default. */
	void			ResetImageInterpolation();
	/*
	   Translate border image cuts into border-box relative source and destination coordinates.
	   Also calculate repeat parameters when applicable.

	   @param border_image_rects [out] structure to fill in the resulting rectangles
	   @param border_image information about border image cuts and scale
	   @param border information about the border properties
	   @param imgw width of the source image
	   @param imgh height of the source image
	   @param border_box_width width of the border box of the destination (document coordinates)
	   @param border_box_height height of the border box of the destination (document coordinates)
	*/
	void			PrepareBorderImageRects(BorderImageRects& border_image_rects, const BorderImage* border_image, const Border* border, int imgw, int imgh, int border_box_width, int border_box_height);

#ifdef CSS_GRADIENT_SUPPORT

	/*
	  Paint a part of a border image gradient.

	  @param border_box_rect border box of the element, relative to the current translation (document coordinates)
	  @param gradient parameters for the gradient
	  @param src_rect rectangle in the source gradient to take the gradient part from
	  @param destination_rect destination of the gradient in document coordinates, relative to the border box
	  @param current_color the current foreground color.
	  @return OpStatus::ERR_NO_MEMORY on OOM, otherwise OpStatus::OK
	 */
	OP_STATUS		BorderImageGradientPartOut(const OpRect& border_box_rect, const CSS_Gradient& gradient, const OpRect& src_rect, const OpRect& destination_rect, COLORREF current_color);

	/*
	  Paint border image gradient corner, egdes and fill, given border and border image properties

	  @param border_box_rect rectangle of the border box, relative to the current translation (document coordinates)
	  @param border border information
	  @param border_image border image information
	  @param current_color the current foreground color
	  @return OpStatus::ERR_NO_MEMORY on OOM, OpStatus::OK otherwise
	 */
	void			PaintBorderImageGradient(const OpRect& border_box_rect, const Border* border, const BorderImage* border_image, COLORREF current_color);
#endif

	OP_STATUS		PaintBorderImage(Image& img, ImageListener* image_listener, const OpRect &rect, const Border *border, const BorderImage *border_image, const ImageEffect *effect = NULL, short image_rendering = CSS_VALUE_auto);

	OP_STATUS		ImageOutTiled(Image& img, const OpRect& dst, const OpPoint& offset, ImageListener* image_listener, double imgscale_x, double imgscale_y, BOOL translate = TRUE) { return VisualDevice::ImageOutTiled(img, dst, offset, image_listener, imgscale_x, imgscale_y, 0, 0, translate); }

#ifdef SKIN_SUPPORT
	OP_STATUS		ImageOutTiledEffect(Image& img, const OpRect& dst, const OpPoint& offset, INT32 effect, INT32 effect_value, INT32 imgscale_x, INT32 imgscale_y);
#endif // SKIN_SUPPORT

	OP_STATUS		ImageOut(Image& img, const OpRect& src, const OpRect& dst, ImageListener* image_listener = NULL, short image_rendering = CSS_VALUE_auto);
	OP_STATUS		ImageOut(Image& img, const OpRect& src, const OpRect& dst, ImageListener* image_listener, short image_rendering, BOOL translate);

#if defined DISPLAY_IMAGEOUTEFFECT || defined SKIN_SUPPORT
	OP_STATUS		ImageOutEffect(Image& img, const OpRect &src, const OpRect &dst, INT32 effect, INT32 effect_value, ImageListener* image_listener = NULL);
#endif // DISPLAY_IMAGEOUTEFFECT || SKIN_SUPPORT

	void			BitmapOut(OpBitmap* bitmap, const OpRect& src, const OpRect& dst, short image_rendering = CSS_VALUE_auto);

	OP_STATUS		BlitImage(OpBitmap* bitmap, const OpRect& src, const OpRect& dst, BOOL add_scrolling = FALSE);

	/**
		Find the total area occupied by tiles that fall within the painting area.

		@param offset the offset of the painting area relative to a tile. Must be a point within the painting area.
		@param paint    the painting area.
		@param viewport the current viewport.
		@param tile     the tile dimensions (position ignored).
	*/
	static OpRect	VisibleTileArea(const OpPoint& offset, OpRect paint, const OpRect& viewport, const OpRect& tile);

	/**
		If the offset is outside the image dimensions, transpose it by the image dimensions
		so that it is within the image.

		@param offset the offset to check.
		@param width  image width.
		@param height image height.
	*/
	static void		TransposeOffsetInsideImage(OpPoint& offset, int width, int height);

	/**
		Output a bitmap and tile it.

		@param cdst       the painting rectangle in document coordinates.
		@param coffset    the offset of the painting rectangle in relation to the desired position
		                  of the bitmap. Note that if the caller offsets the bitmap in relation to
		                  the painting rectangle, the offset's sign must be inverted.
		@param imgscale_x the desired scale of the image's x axis, where 100 implies no scaling.
		@param imgspace_x the desired horizontal space between the tiles.
	*/
	OP_STATUS		BlitImageTiled(OpBitmap* bitmap, const OpRect& cdst, const OpPoint& coffset, double imgscale_x, double imgscale_y, int imgspace_x, int imgspace_y);

	OP_STATUS		BlitImageTiled(OpBitmap* bitmap, const OpRect& cdst, const OpPoint& coffset, double imgscale_x, double imgscale_y) {
		return BlitImageTiled(bitmap, cdst, coffset, imgscale_x, imgscale_y, 0, 0);
	}

	static uni_char*TransformText(const uni_char* txt, uni_char* txt_out, size_t &len, int format_option, INT32 spacing = 0);
public:

	/** Draw a filled ellipse, in document coordinates */
	void	BulletOut(int x, int y, int width, int height);

	/** Draw a 1px thick outline of a rectangle with the given pen-style and current color (in document coordinates).
		If you just want a solid rectangle, DrawRect is a better alternative. */
	void	RectangleOut(int x, int y, int width, int height, int css_pen_style = CSS_VALUE_solid);

	/** Draws the outline of a ellipse, in document coordinates */
	void	EllipseOut(int x, int y, int width, int height, UINT32 line_width = 1);

	/** Draws a horizontal or vertical line (as left, right, top or bottom) with the given color, pen style and thickness.
	 *
	 * @param x The x coordinate for the point where the line is to start (in document coordinates).
	 * @param y The y coordinate for the point where the line is to start (in document coordinates).
	 * @param length The length in pixels of the line.
	 * @param width The width (thickness) of the line in pixels.

	 * @param css_pen_style The pen style which is a CSS_VALUE_xxxx (F.ex CSS_VALUE_dotted, CSS_VALUE_dashed....)
	 * @param color The COLORREF color of the line.
	 * @param horizontal TRUE if the line should be horizontal (top or bottom) or FALSE if it should be vertical (left or right)
	 * @param top_or_left TRUE if top or left, FALSE if bottom or right (depending on the horizontal value)
	 * @param top_or_left_w Width of the left or top connecting line. F.ex if this is a bottom line, this specify the left lines width so the angle of the corner is drawn correct.
	 * @param right_or_bottom_w Width of the right or bottom connecting line. F.ex if this is a top line, this specify the right lines width so the angle of the corner is drawn correct.
	 */
	void	LineOut(int x, int y, int length, int width, int css_pen_style, COLORREF color, BOOL horizontal, BOOL top_or_left, int top_or_left_w, int right_or_bottom_w);

	/** Call to get rectangles for all visible search hits in this VisualDevice.
	 * @param all_rects Vector of rects for all matches.
	 * @param active_rects Vector of rects for only the active match (if there is one).
	 * @return TRUE if successful. FALSE if there is no search.
	 */
	BOOL GetSearchMatchRectangles(OpVector<OpRect> &all_rects, OpVector<OpRect> &active_rects);

	/**
	 * A horizontal line that will be drawn with style CSS_VALUE_solid. Its width will be the
	 * same regardless of where on the screen it is. This makes this line different from LineOut
	 * which scales the width to be sure that there are no gaps between lines that should touch
	 * each other.
	 *
	 * @param x The x coordinate for the point where the line is to start.
	 * @param y The y coordinate for the point where the line is to start.
	 * @param length The length of the line.
	 * @param iwidth The width of the line. Should be at least 1.
	 * @param color The color of the line.
	 */
	void	DecorationLineOut(int x, int y, int length, int iwidth, COLORREF color);

	/** Invert a border inside rect.
		rect is in document coordinates *without scrolling applied*.
		border is the thickness and will grow towards the center of the rectangle.
		NOTE: the OpPainter implementations of 'invert' differ in what they actually do (See OpPainter::InvertRect). */
	void	InvertBorderRect(const OpRect &rect, int border);

	/** Deprecated. Remove anything that has to do with it. */
	void	DEPRECATED(DrawSplitter(int x, int y, int w, int h));

	/** Draw area with selection for images. */
	void	DrawSelection(const OpRect &rect);

	/** Draw a highlightrect. Will use skin if TWEAK_SKIN_HIGHLIGHTED_ELEMENT is enabled and the skin supports it.
		highlight_elm should be the element that is highlighted (if possible). Pointer will not be followed, only used as id. */
	void	DrawHighlightRect(OpRect rect, BOOL img = FALSE, HTML_Element *highlight_elm = NULL);

	/** Draw several highlighted rects.
		highlight_elm should be the element that is highlighted (if possible). Pointer will not be followed, only used as id. */
	BOOL	DrawHighlightRects(OpRect *rect, int num_rects, BOOL img = FALSE, HTML_Element *highlight_elm = NULL);

	/** Draw text selection background. */
	void	DrawTextBgHighlight(const OpRect& rect, COLORREF col, VD_TEXT_HIGHLIGHT_TYPE type);

#ifdef SVG_SUPPORT
	/** Update a highlighted area. This is like Update, but will manipulate rect to take skinhighlights borders into account */
	void	UpdateHighlightRect(OpRect rect);
#endif // SVG_SUPPORT

	/** Sets a flag that will make us repaint larger areas than requested. As much larger as needed to cover the highlight if a
	    repainted area has a highlight. */
	void	IncludeHighlightInUpdates(BOOL new_val) { m_include_highlight_in_updates = new_val; }

	/** Draw a caret. Either a inverted rect or a coloured rect (depending of TWEAK_DISPLAY_INVERT_CARET) */
	void	DrawCaret(const OpRect& rect);

	/** Draw line between 2 points with thickness width. (in document coordinates) */
	void	DrawLine(const OpPoint &from, const OpPoint &to, UINT32 width = 1);

	/** Draw a horizontal or vertical line with thickness width with the upper left corner on the given point. (in document coordinates) */
	void	DrawLine(const OpPoint &from, UINT32 length, BOOL horizontal, UINT32 width = 1);

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	/** Draw something (a wavy line) which indicates a misspelled word */
	void	DrawMisspelling(const OpPoint &from, UINT32 length, UINT32 wave_height, COLORREF color, UINT32 width = 1);
#endif // INTERNAL_SPELLCHECK_SUPPORT

	/** Fill rect (in document coordinates). */
	void	FillRect(const OpRect &rect);

	/** Draw rectangle outline (in document coordinates).
		The thickness of the rectangle will be zoomed. */
	void	DrawRect(const OpRect &rect);

	/** Clear the rectangle to the current color (in document coordinates). */
	void	ClearRect(const OpRect &rect);

	/** Draw a (dotted) focus rect (in document coordinates). */
	void	DrawFocusRect(const OpRect &rect);

	/** Invert rect (in document coordinates).
		NOTE: the OpPainter implementations of 'invert' differ in what they actually do (See OpPainter::InvertRect). */
	void	InvertRect(const OpRect &rect);

	/** Fill ellipse (in document coordinates). */
	void	FillEllipse(const OpRect &rect);

	/** Draw ellipse outline (in document coordinates). */
	void	DrawEllipse(const OpRect &rect, UINT32 line_width = 1) { EllipseOut(rect.x, rect.y, rect.width, rect.height, line_width); }

	BOOL			IsTxtItalic() const { return logfont.GetItalic(); }

	void			SetFontStyle(int style);

	void SetSmallCaps(BOOL small_caps);

	/** Set the blur radius in pixels to apply to glyphs of the current font.
	 * This is used as a faster implementaion of text shadow. It should
	 * only be used when IsCurrentFontBlurCapable returns TRUE for the radius
	 * being specified. */
	void			SetFontBlurRadius(int radius);

	/** Check if the current font is capable of applying a per glyph blur of
	 * the specified radius. */
	BOOL			IsCurrentFontBlurCapable(int radius);

	void			SetFontSize(int size);

	int				GetFontSize() const { return op_abs((int)logfont.GetHeight()); }

	int				GetLineHeight() const { return (int) (op_abs((int)logfont.GetHeight()) * NORMAL_LINE_HEIGHT_FACTOR); }

	int				GetFontWeight() const { return logfont.GetWeight(); }
	void			SetFontWeight(int weight);

	int				GetCharSpacingExtra() { return char_spacing_extra; }

	void			SetCharSpacingExtra(int extra);

	UINT32			GetFontAscent();			///< Get ascent of current font, in doc coordinates.
	UINT32			GetFontDescent();			///< Get descent of current font, in doc coordinates.
	UINT32			GetFontInternalLeading();	///< Get internal leading of current font, in doc coordinates.
	UINT32			GetFontHeight();			///< Get height of current font, in doc coordinates.
	UINT32			GetFontAveCharWidth();		///< Get average character width of current font, in doc coordinates.
	UINT32			GetFontOverhang();			///< Get overhang of current font, in doc coordinates.
	void			GetFontMetrics(short &font_ascent, short &font_descent, short &font_internal_leading, short &font_average_width);

	/** Get ascent of current font, in doc coordinates. */
	UINT32			GetFontStringWidth(const uni_char* str, UINT32 len);
    BOOL            InEllipse(int* coords, int x, int y);
    BOOL            InPolygon(int* point_array, int points, int x, int y);

	void			SetFont(int font_number);
	void			SetFont(const FontAtt& font);
	const OpFont*	GetFont();
	const FontAtt&	GetFontAtt() const;

	int				GetCurrentFontNumber() { return current_font_number; }

	/** FIX: We are treating UINT32 as a COLORREF. But the fact is that SetColor32 is used with colors returned as COLORREF. It's lots of mixups in the code still. */
	inline void		SetColor32(UINT32 col) { COLORREF cr = col; SetColor(OP_GET_R_VALUE(cr), OP_GET_G_VALUE(cr), OP_GET_B_VALUE(cr), OP_GET_A_VALUE(cr)); }

	void			SetColor(int r, int g, int b, int a = 255);
	void			SetColor(COLORREF col);
	COLORREF		GetColor() const { return color; }

	/** Returns the view the document is rendered in */
	CoreView*		GetView() const { return view; }

	/** Return the OpView that is the container for this VisualDevice.
		NOTE: Any drawing or other coordinate related use on the OpView related to this VisualDevice
		has to be transformeed correctly. First with scaling and scroll, then with OffsetToContainer. */
	OpView*			GetOpView();

	/** Returns the containerview that the view is placed in. This view contains the scrollbars.
		This is not necessarily a CoreViewContainer, and does not have the OpView that is returned with GetOpView. */
	CoreView*		GetContainerView() const;

	void			CalculateSize();

	/**
	 * In printpreview, we have 2 scalings. The printerscale and the 'zooming' of the printpreviewwindow.
	 *
	 * @returns A scale 1-100-, where 100 is "no scaling" and 1 is "very small".
	 */
	int				GetScale(BOOL printer_scale = FALSE) const;
	Window*			GetWindow() const;

	/** Scale to screen coordinates but minimum 1 as result */
	INT32			ScaleLineWidthToScreen(INT32 v) const;

	// Scalefunctions for scaling something in documentsize to screensize (All painting should use this).
	OpPoint			ScaleToScreen(const INT32 &x, const INT32 &y) const;

	/**
	 * This method scales width and height (as well as x and y) so that there
	 * are no gaps between things that should be adjacent. Because of that w
	 * and h might be scaled different depending on x and y.  If that's bad,
	 * see ScaleDecorationToScreen.
	 */
	OpRect			ScaleToScreen(const INT32 &x, const INT32 &y, const INT32 &w, const INT32 &h) const;

	/**
	 * Yeah, what...
	 */
	AffinePos		ScaleToScreen(const AffinePos& c) const;

	/**
	 * This method is as ScaleToScreen except that it
	 * scales width and height so that these get the same scale regardless
	 * of x and y. This can leave gaps between lines that were thought
	 * to touch eachother so it shouldn't be used for drawing for instance
	 * borders.
	 */
	OpRect			ScaleDecorationToScreen(INT32 x, INT32 y, INT32 w, INT32 h) const;
	INT32			ScaleToScreen(INT32 v) const;

	/* Same as ScaleToScreen but operates on a floating point value. */
	double			ScaleToScreenDbl(double v) const;

	inline OpRect	ScaleToScreen(const OpRect& rect) const { return ScaleToScreen(rect.x, rect.y, rect.width, rect.height); };
	inline OpPoint	ScaleToScreen(const OpPoint& point) const { return ScaleToScreen(point.x, point.y); };

#ifdef VEGA_OPPAINTER_SUPPORT
	OpDoublePoint	ScaleToScreenDbl(double x, double y) const;
	inline OpDoublePoint	ScaleToScreenDbl(const OpDoublePoint& point) const { return ScaleToScreenDbl(point.x, point.y); };
#endif // VEGA_OPPAINTER_SUPPORT

	/** Scalefunctions to scale something from screensize to documentsize (F.ex. a invalidated area should
		use this to get the invalidated area in the document) */
	INT32			ScaleToDoc(INT32 v) const;
	OpRect			ScaleToDoc(const OpRect& rect) const;
	AffinePos		ScaleToDoc(const AffinePos& c) const;
	OpPoint			ScaleToDoc(const OpPoint& p) const {
		return IsScaled() ? OpPoint(ScaleToDoc(p.x), ScaleToDoc(p.y)) : p;
	}

	/**
	 * Converts given document point to the screen coordinates. */
	OpPoint			ConvertToScreen(const OpPoint& doc_point) const;

	/**
	 * Converts screen to document units. Similar to ScaleToDoc,
	 * but always rounds down to the nearest whole document unit.
	 * Useful when aligning a maximum size for something in pixels
	 * (e.g. the maximum size of a window) on document coordinates.
	 */
	INT32			ScaleToDocRoundDown(INT32 v) const;

	/**
	 * Scales the rect (in screen coordinates) to a rect that will be
	 * at least so big that a ScreenToRect on it is guaranteed to contain
	 * the original rect. Use this when it's important not to lose any
	 * pixels in an area.
	 *
	 * @param rect A rect in screen coordinates.
	 * @returns A rect in document coordinates.
	 */
	OpRect			ScaleToEnclosingDoc(const OpRect& rect) const;

	// DEPRECATED!
	void			ApplyScaleRounding(INT32* val);

	INT32			ApplyScaleRoundingNearestUp(INT32 val);

	/** Offset x and y to the closest container (So that the coordinate is relative to the OpView) */
	inline OpPoint	OffsetToContainer(const INT32 &x, const INT32 &y) { return OpPoint(x + offset_x, y + offset_y); }
	inline OpPoint	OffsetToContainer(const OpPoint& point) { return OffsetToContainer(point.x, point.y); }
	inline OpRect	OffsetToContainer(const OpRect& rect) { return OpRect(rect.x + offset_x, rect.y + offset_y, rect.width, rect.height); }

	/** Offset x and y to the closest container and translate for the scroll position (So that the coordinate is relative to the OpView) */
	inline OpPoint	OffsetToContainerAndScroll(const INT32 &x, const INT32 &y) { return OpPoint(x + offset_x - view_x_scaled, y + offset_y - view_y_scaled); }
	inline OpPoint	OffsetToContainerAndScroll(const OpPoint& point) { return OffsetToContainerAndScroll(point.x, point.y); }
	inline OpRect	OffsetToContainerAndScroll(const OpRect& rect) { return OpRect(rect.x + offset_x - view_x_scaled, rect.y + offset_y - view_y_scaled, rect.width, rect.height); }

	DocumentManager*
					GetDocumentManager() const { return doc_manager; }
	void			SetDocumentManager(DocumentManager* dman) { doc_manager = dman; }

	void			SetBgColor(COLORREF col);

	/** Sets the visual device to use the default background colour. */
	void			SetDefaultBgColor();
	BOOL			UseDefaultBgColor() { return use_def_bg_color; }

	/** DEPRECATED! Use DrawBgColor(const OpRect& rect)! */
	virtual void	DrawBgColor(const RECT& rect);

	/** Fills the specified rectangle with the VisualDevice's background colour.
		It might be either the color set with SetBgColor, or the default color (if SetDefaultBgColor is set).
		@param rect      the rectangle to fill
		@param bg_color  the background color to use, or USE_DEFAULT_COLOR to use the bg_color member.
		@param translate whether to translate the rect. If FALSE, it is the caller's responsibility to translate appropriately.
	*/
	virtual void	DrawBgColor(const OpRect& rect, COLORREF bg_color = USE_DEFAULT_COLOR, BOOL translate = TRUE);

#ifdef CSS_GRADIENT_SUPPORT
private:

	/**
	   Draw border image gradients, tiled if needed.

	   @param gradient information about the gradient
	   @param gradient_size size of the gradient source (which corresponds to the size of the border box)
	   @param src_rect rectangle of the source inside the gradient
	   @param destination_rect destination of the gradient (document coordinates, relative to the border box)
	   @param dx advance in the horizontal direction for a tile
	   @param dy advance in the vertical direction for a tile
	   @param ofs_x horizontal start offset for tiling
	   @param ofs_y vertical start offset for tiling
	   @param current_color the current foreground color
	   @return OpStatus::ERR_NO_MEMORY on OOM, OpStatus::OK otherwise
	*/

	OP_STATUS		DrawBorderImageGradientTiled(const CSS_Gradient& gradient, const OpRect& gradient_size, const OpRect& src_rect, const OpRect& destination_rect, double dx, double dy, int ofs_x, int ofs_y, COLORREF current_color);

public:
	/**
		Draws a background gradient.

		It will draw the gradient (per background-* properties) in order to
		fill the box, clipping to the painting rectangle and viewport as it goes along.

		@param paint_rect    the background painting area in document coordinates.
		@param gradient_rect the gradient area as positioned and sized by background-* properties, in document coordinates.
		@param gradient      the gradient.
		@param repeat_space  the calculated width and height for the spaces when 'background-repeat: space'.
		@param current_color the value of CSS currentColor.
	*/
	void			DrawBgGradient(const OpRect& paint_rect, const OpRect& gradient_rect, const CSS_Gradient& gradient, const OpRect& repeat_space, COLORREF current_color);

	/**
	   Draw a gradient as an image

	   @param destination_rect	the size and position of the gradient. Document coordinates, relative to the current translation.
	   @param current_color	the value of CSS currentColor
	   @param gradient			the gradient
	*/
	void			GradientImageOut(const OpRect& destination_rect, COLORREF current_color, const CSS_Gradient& gradient);
#endif // CSS_GRADIENT_SUPPORT

#ifdef VEGA_OPPAINTER_SUPPORT
	void			MakeBackgroundPathWithRadius(VEGAPath *p, const OpRect &rect, const Border *border, BOOL inside_border);
	void			MakeBorderPathWithRadius(VEGAPath *p, const OpRect &rect, const Border *border, UINT16 border_id);
	void			MakeInvertedPathWithRadius(VEGAPath *p, const OpRect &rect, const Border *border, BOOL inside_border);
	void			MakeInvertedPathWithRadius(VEGAPath *p, const OpRect &rect, const OpRect &bounding_rect, const Border *border, BOOL inside_border);
	/** Draw background color with radius
		@param rect The rectangle to draw
		@param border Specifies the border radius of the rectangle
		@param clip If TRUE, only draw if the rect intersects with the current displayed area. Otherwise, it always draws. */
	void			DrawBgColorWithRadius(const OpRect& rect, const Border *border, BOOL clip = TRUE);
	void			DrawBorderWithRadius(int width, int height, const Border *border, int top_gap_x = 0, int top_gap_width = 0);
	void			ModifyAndDrawBorderOfSpecificStyle(int width, int height, const Border *border, short replace_style, CSSValue with_style,
														short b_left_width, short b_top_width, short b_right_width, short b_bottom_width,
														int top_gap_x = 0, int top_gap_width = 0);

	enum Corners { TOP_LEFT = 0, TOP_RIGHT, BOTTOM_RIGHT, BOTTOM_LEFT, NUM_CORNERS };
	struct CornerPaths { VEGAPath paths[NUM_CORNERS]; };

	void			PaintBoxShadow(const Shadow &shadow, const OpRect &border_box, const OpRect &padding_box, COLORREF bg_color, const Border *border);
	BOOL			PaintBoxShadowIfSimple(const Shadow &shadow, const OpRect &shadow_rect, const Border *border_with_spread, const OpRect &border_box, const Border *border, BOOL using_stencil = TRUE);
	OP_STATUS		GetCornerFill(VEGAFill **corner_fill, const Shadow &shadow, const OpRect &shadow_rect, const Border *border);
	OP_STATUS		GetCornerPaths(CornerPaths &corner_paths, const Shadow &shadow, const OpRect &shadow_rect, const OpRect &border_box, const Border *border, BOOL using_stencil = TRUE);
	OP_STATUS		DrawCorner(BoxShadowCorner *corner);

	COLORREF		GetBorderColor(const BorderEdge *border_edge, BOOL top_or_left);
#endif

	/** Get the width of the rendering viewport (in document coordinates). */
	virtual int		GetRenderingViewWidth() const;

	/** Get the height of the rendering viewport (in document coordinates). */
	virtual int		GetRenderingViewHeight() const;

	/** Get CoreView width (in screen coordinates). */
	virtual int		WinWidth() const { return win_width; }

	/** Get CoreView height (in screen coordinates). */
	virtual int		WinHeight() const { return win_height; }

	/** Same as WinWidth(), but with vertical scrollbar width subtracted, if present. */
	virtual int		VisibleWidth();

	/** Same as WinHeight(), but with horizontal scrollbar height subtracted, if present. */
	virtual int		VisibleHeight();

	/** Get space (number of pixels) occupied by vertical scrollbar, if visible. */
	int				GetVerticalScrollbarSpaceOccupied() const { return v_on ? GetVerticalScrollbarSize() : 0; }

	/** Get space (number of pixels) occupied by horizontal scrollbar, if visible. */
	int				GetHorizontalScrollbarSpaceOccupied() const { return h_on ? GetHorizontalScrollbarSize() : 0; }

	/** Get size (width) of vertical scrollbar. Its visibility does not affect the result. */
	int				GetVerticalScrollbarSize() const;

	/** Get size (height) of horizontal scrollbar. Its visibility does not affect the result. */
	int				GetHorizontalScrollbarSize() const;

	/** @return TRUE if the vertical scrollbar is visible, FALSE otherwise. */
	BOOL			IsVerticalScrollbarOn() const { return v_on; }

	/** @return TRUE if the horizontal scrollbar is visible, FALSE otherwise. */
	BOOL			IsHorizontalScrollbarOn() const { return h_on; }

	/** Get the visible rect using VisibleWidth and VisibleHeight. If there is a scrollbar on the left side
		the rect will not be at 0,0 */
	OpRect			VisibleRect();

	virtual BOOL	IsPrinter() const { return FALSE; }

	/** Draw inverted border inside this VisualDevice if it is a frame. The parameter is unused. */
	void			DrawWindowBorder(BOOL invert);

#ifdef _PLUGIN_SUPPORT_
	/**
	*
	* @param element. The element that holds this plugin. Passed along to the created CoreView
	*/
# ifdef USE_PLUGIN_EVENT_API
	OP_STATUS		GetNewPluginWindow(OpPluginWindow *&new_win, int x, int y, int w, int h, CoreView* parentview, BOOL windowless = FALSE
#  ifdef NS4P_USE_PLUGIN_NATIVE_WINDOW										   
										, Plugin *plugin = NULL
#  endif // NS4P_USE_PLUGIN_NATIVE_WINDOW										   
										, HTML_Element* element = NULL);
#else	
	OP_STATUS		GetNewPluginWindow(OpPluginWindow *&new_win, int x, int y, int w, int h, CoreView* parentview, BOOL transparent = FALSE, HTML_Element* element = NULL);
# endif // USE_PLUGIN_EVENT_API
    void            ShowPluginWindow(OpPluginWindow* plugin_window, BOOL show);
	/** As SetFixedPositionSubtree, but used for plugin windows.
	 * @see SetFixedPositionSubtree()
	 */
	void			SetPluginFixedPositionSubtree(OpPluginWindow* plugin_window, HTML_Element* fixed);
    void            DeletePluginWindow(OpPluginWindow* plugin_window);
# ifdef USE_PLUGIN_EVENT_API
	void			DetachPluginWindow(OpPluginWindow* plugin_window);
# endif // USE_PLUGIN_EVENT_API
	void			AttachPluginCoreView(OpPluginWindow* plugin_window, CoreView *parentview);
	void			DetachPluginCoreView(OpPluginWindow* plugin_window);
    void            ResizePluginWindow(OpPluginWindow* plugin_window, int x, int y, int w, int h, BOOL set_pos, BOOL update_pluginwindow = TRUE);
#endif // _PLUGIN_SUPPORT_

	/**
	   Create font from the current settings, using the fontcache.
	   @return the new font, or NULL if we for some reason fail. Don't forget to release it by calling g_font_cache->ReleaseFont!
	*/
	OpFont*			CreateFont();

	/**	Reset translation, color, font, and other dada that will affect how painting is done. */
	void			Reset();

	/** Push translation, color, font, and other data that will affect how painting is done.
		PushState if you need to change that, and then continue with the same state as before (restore it with PopState).
		PushState will always succeed.
	*/
	VDState			PushState();
	void			PopState(const VDState& state);

	/** Retrieve the horizontal and vertical DPI for the screen */
	virtual void    GetDPI(UINT32* horizontal, UINT32* vertical);

	/** Create new VisualDevice with the given documentmanager, scrolltype and parentview.
		If this VisualDevice is PrintDevice, the new one will also be a PrintDevice. */
	virtual OP_STATUS GetNewVisualDevice(VisualDevice *&visdev, DocumentManager* doc_man, ScrollType scroll_type, CoreView* parentview);

#ifdef _PRINT_SUPPORT_
	/** Create a new VisualDevice and set its scale to the printing scale. */
	OP_STATUS		CreatePrintPreviewVD(VisualDevice *&preview_vd, PrintDevice* pd);
#endif // _PRINT_SUPPORT_

#ifdef EXTENSION_SUPPORT
	/** Paints a part of this VisualDevice into painter with exclusion of
	 *  containers specified by exclude list. Used for taking a screenshot
	 *  while 'censoring' parts to which requester should have no access.
	 *  @param painter painter to which will be painted. MUST NOT be NULL.
	 *  @param rect Rectangle to paint.
	 *  @param exclude_list list of sub-VisualDevices to exclude from painting.
	 *                      The list may be moduified by this functions implementation
	 *                      so it should NOT be used by the caller again.
	 */
	void			PaintWithExclusion(OpPainter* painter, OpRect& rect, OpVector<VisualDevice>& exclude_list);
#endif // EXTENSION_SUPPORT
	/** @short Set position and size of rendering buffer (NOTE: in screen coordinates).
	 *
	 * @param rect Position and size of rendering buffer, in screen coordinates.
	 */
	void			SetRenderingViewGeometryScreenCoords(const AffinePos& pos, int width, int height);

	/* Same as above, but the position is always a translation */
	void			SetRenderingViewGeometryScreenCoords(const OpRect& rect)
	{
		AffinePos r_pos(rect.x, rect.y);
		SetRenderingViewGeometryScreenCoords(r_pos, rect.width, rect.height);
	}

	/** @short Show the view represented by this VisualDevice.
	 *
	 * If the view doesn't exist, it will be created as child of the
	 * specified parentview.
	 *
	 * @param parentview Parent of the view to be represented by this
	 * VisualDevice. May be NULL, in which case the view had better
	 * exist already.
	 */
	OP_STATUS		Show(CoreView* parentview);

	/** @short Hide the view represented by this VisualDevice.
	 *
	 * @param free If TRUE, delete the view and most other things
	 * owned by this VisualDevice. If FALSE, just hide the view.
	 */
	void			Hide(BOOL free = FALSE);

	/** @short Is the view represented by this VisualDevice visible?
	 *
	 * @return TRUE if the view is visible, FALSE otherwise.
	 */
	BOOL			GetVisible();

    ScrollType		GetScrollType() { return scroll_type; }
    void			SetScrollType(ScrollType scroll_type);

	/** @short Request scrollbar presence in automatic scrollbars mode.
	 *
	 * This only has an effect when scrollbar mode is VD_SCROLLING_AUTO. This
	 * method will update the CoreView size and scrollbars if the scrollbar
	 * situation changed.
	 *
	 * @param vertical_on TRUE if the vertical scrollbar should be displayed,
	 * FALSE if it should be hidden.
	 * @param horizontal_on TRUE if the vertical scrollbar should be displayed,
	 * FALSE if it should be hidden.
	 */
	void			RequestScrollbars(BOOL vertical_on, BOOL horizontal_on);

	void			GetScrollbarObjects(OpScrollbar** h_scr, OpScrollbar** v_scr) { *h_scr = h_scroll; *v_scr = v_scroll; }

#ifdef DISPLAY_SPOTLIGHT
	enum InsertionMethod {
		IN_FRONT = 1,	///< Painted in front of all existing spotlights.
		IN_BACK,		///< Painted undearneath all existing spotlights.
		ABOVE_TARGET,	///< Painted in front of the 'target' spotlight.
		BELOW_TARGET	///< Painted below the 'target' spotlight.
	};

	/** Adds the given spotlight to be displayed. The ownership is taken over.
	 *
	 * It will be deleted when the VisualDevice is deleted, or if
	 * RemoveSpotlight is called with the same element or id.
	 *
	 * @param spotlight The spotlight to be displayed.
	 * @param element The element that this spotlight should be associated with.
	 *                The pointer is never followed so it can be safely deleted
	 *                without updating. Can be NULL.
	 * @param insertion The stacking order that the spotlight should have when
	 *                  painted. By default painted on top of all other spotlights.
	 * @param target The target spotlight that must be set when inserting using
	 *               ABOVE_TARGET or BELOW_TARGET. Specifies the spotlight that
	 *               this spotlight should be painted relative to.
	 * @return OpStatus::OK on success. Spotlight ownership is taken over.
	 *         OpStatus::ERR_NO_MEMORY on OOM.
	 *         OpStatus::ERR_NO_SUCH_RESOURCE if inserting below or above another
	 *         spotlight and its ID was not in the list.
	 */
	OP_STATUS		AddSpotlight(VDSpotlight *spotlight, HTML_Element *element, InsertionMethod insertion = IN_FRONT, VDSpotlight *target = NULL);

	/** Remove the spotlight associated with the given element or id. */
	void			RemoveSpotlight(HTML_Element *element);
	void			RemoveSpotlight(unsigned int id);

	/** Get the spotlight associated with the given element or id. */
	VDSpotlight		*GetSpotlight(HTML_Element *element);
	VDSpotlight		*GetSpotlight(unsigned int id);

	/** Should be called when any property has been changed to update the spotlight. */
	void			UpdateSpotlight(HTML_Element *element);

	/** Removes all spotlights. */
	void			RemoveAllSpotlights() { spotlights.DeleteAll(); }
#endif

	/** Calculate a reasonable cliprect to use for the region:
	 *   (start <= x < start+length) isect (-Inf < y < Inf) */
	void			BeginHorizontalClipping(int start, int length);
	void			EndHorizontalClipping(BOOL exclude = FALSE) { EndClipping(exclude); }

	void			BeginClipping(const OpRect& rect);
	void			EndClipping(BOOL exclude = FALSE);
	OpRect			GetClipping();

	/** Begins a layer of opacity.
	 * @param rect A rect in document coordinates.
	 * @param opacity value 0-255. 0 is invisible, 255 is fully solid.
	 */
	OP_STATUS		BeginOpacity(const OpRect& rect, UINT8 opacity);
	/** End opacity layer. */
	void			EndOpacity();

	/** Begins a layer of effect.
		The rect may be adjusted so that the effect is fully visible. (F.ex blur will make rect bigger with the blur radius) */
	OP_STATUS		BeginEffect(const OpRect& rect, const DisplayEffect& display_effect);
	/** End effect layer. The effect will be executed on the backbuffer which is then painted, and deleted. */
	void			EndEffect();

	/** Add plugin area. The area will be tracked so any overlapping background drawing (or call to CoverPluginArea) will remove
		that part from the plugins visibility region (make that part of the plugin invisible).
		@param rect The rect of the plugin (in document coordinates)
		@param plugin_window The pluginwindow positioned at the given area */
	OP_STATUS		AddPluginArea(const OpRect& rect, OpPluginWindow* plugin_window);

	/** Cover any plugin area that intersects with rect.
		@param The rect of the area to cover (in document coordinates) */
	void			CoverPluginArea(const OpRect& rect);

	/** Apply the display effect on the backbuffer. Internal function! */
	void			ApplyEffectInternal(VisualDeviceBackBuffer *bb);

	/** Begins a layer that all painting will be done to until EndBackbuffer is called.
		A temporary backbuffer will be created or it will use OpBitmap/OpPainter support if available.
		This function is internal! Use BeginOpacity or BeginEffect instead.
		*/
	OP_STATUS		BeginBackbuffer(const OpRect& rect, UINT8 opacity, BOOL clip, BOOL copy_background, VisualDeviceBackBuffer*& backbuffer, int clip_rect_inset = 0);
	OP_STATUS		BeginBackbuffer(const OpRect& rect, UINT8 opacity, BOOL clip, VisualDeviceBackBuffer*& backbuffer)
									{ return BeginBackbuffer(rect, opacity, clip, TRUE, backbuffer); }

	/** End backbuffer. if paint is TRUE, it will be painted */
	void			EndBackbuffer(BOOL paint);

#ifdef VEGA_OPPAINTER_SUPPORT
	OP_STATUS		BeginStencil(const OpRect& rect);
	void			BeginModifyingStencil();
	void			EndModifyingStencil();
	void			EndStencil();
#endif

	/** Begin a clipping with rounded corners, given the radius defined by the border.
		If successful, it must be followed by a call to EndRadiusClipping when the clipping should end.
		@param border_box The border_box of the layoutbox.
		@param clip_rect The clip rectangle (Equal or a inset of border_box).
		@param border The border with the radius. */
	OP_STATUS		BeginRadiusClipping(const OpRect &border_box, const OpRect& clip_rect, const Border* border);
	void			EndRadiusClipping();

	/** Paint rect with background information from bg */
	void			PaintBg(const OpRect& rect, BgInfo *bg, int debug);

	/** Is the image solid or transparent. */
	BOOL			IsSolid(Image& img, ImageListener* image_listener);

	/** Flush all pending backgrounds behind rect */
	void			FlushBackgrounds(const OpRect& rect, BOOL only_if_intersecting_display_rect = TRUE);

	/** Flush all pending backgrounds for element */
	void			FlushBackgrounds(HTML_Element* element);

	/** rect will be covered completly by solid color and can be excluded from pending backgrounds.
		Or if rect must be flushed, use CoverArea with keep_hole = TRUE first, to minimize the area that needs to be flushed. */
	void			CoverBackground(const OpRect& rect, BOOL pending_to_add_inside = FALSE, BOOL keep_hole = FALSE);

	/**
		Looks at the context of the background and determines paint area, whether there is border-radius,
		and whether we must paint immediately.

		Painting must be done immediately if
		- background clips to border box
		- border-radius
		- border-image
		- border is not solid
		- transform
		- viewport is scaled

		@param[in]     info           context information.
		@param[in,out] rect           the border box, which will be turned into the content box.
		@param[out]    rounded        TRUE if border-radius > 0 is specified on a corner.
		@param[in,out] must_paint_now set to TRUE if we must paint immediately (see prose for details).
	*/
	void			PaintAnalysis(const BG_OUT_INFO *info, OpRect &rect, BOOL &rounded, BOOL &must_paint_now);
	void			BgColorOut(BG_OUT_INFO *info, const OpRect &cover_rect, const OpRect &border_box, const COLORREF &bg_color);
#ifdef CSS_GRADIENT_SUPPORT
	void			BgGradientOut(const BG_OUT_INFO *info, COLORREF current_color, const OpRect &border_box, OpRect paint_rect, OpRect gradient_rect, const CSS_Gradient &gradient, const OpRect &repeat_space);
#endif // CSS_GRADIENT_SUPPORT
	void			BgImgOut(BG_OUT_INFO *info, const OpRect &cover_rect, Image& img, OpRect& dst_rect, OpPoint& offset, ImageListener* image_listener, double imgscale_x, double imgscale_y)
	{ BgImgOut(info, cover_rect, img, dst_rect, offset, image_listener, imgscale_x, imgscale_y, 0, 0); }
	void			BgImgOut(BG_OUT_INFO *info, const OpRect &cover_rect, Image& img, OpRect& dst_rect, OpPoint& offset, ImageListener* image_listener, double imgscale_x, double imgscale_y, int imgspace_x, int imgspace_y);
	void			BgHandleNoBackground(BG_OUT_INFO *info, const OpRect &cover_rect);

	/** Returns TRUE if the VisualDevice can use RGBA colors for painting. */
	BOOL			SupportsAlpha();

	/** If the window is using truezoom, font measuring is done in 100% and then scaled.
		That can result in precision loss which is not wanted in some situations. Between BeginAccurateFontSize() and EndAccurateFontSize(),
		all font measuring will be done in the current scale. */
	void			BeginAccurateFontSize();
	void			EndAccurateFontSize();

private:
	/**
	   traverse backbuffer stack upwards and premultiply alpha for
	   backbuffers that failed to create a bitmap - see documentation
	   of VisualDeviceBackBuffer::oom_fallback
	 */
	UINT8 PreMulBBAlpha();

#ifdef OPFONT_GLYPH_PROPS
	/**
	 fetches the positional properties of the glyph representing the character ch using the current font.
	 The values will be in screen coords. However with the following exceptions:
	 1) LayoutIn100Percent() would return TRUE. In such case the values are in "screen coords",
	 but according to layout true zoom scale.
	 @see LayoutIn100Percent()
	 2) HasTransform() would return TRUE. In such case the scale 100 is used for fonts so
	 the values will be actually in doc coords. In this case scaling must be handled elsewhere.
	 @see HasTransform()
	 @param up the character whose glyph dimentions are to be obtained
	 @param props (out) will be filled with the properties of the glyph (in screen coords)
	 @param fill_zeros_if_error Will fill all props' fields with zeros if error.
	 @return OpStatus::OK, OpStatus::ERR if the glyph's props couldn't be fetched
			 OpStatus::ERR_NO_MEMORY in case of OOM.
	 */
	OP_STATUS		GetGlyphPropsInLocalScreenCoords(const UnicodePoint up, OpFont::GlyphProps* props, BOOL fill_zeros_if_error = FALSE);
#endif // OPFONT_GLYPH_PROPS

	BOOL LayoutIn100Percent() const { Window* w = GetWindow(); return w && w->GetTrueZoom() && !accurate_font_size; }
	void DrawString(OpPainter *painter, const OpPoint &point, const uni_char *str, int strlen, int char_spacing_extra = 0, short word_width = -1);
	void DrawStringInternal(OpPainter *painter, const OpPoint &point, const uni_char *str, int strlen, int char_spacing_extra = 0);

	/** Add pending colorbackground for rect. It will be delayed until it must be flushed. It may also be sliced in smaller areas. */
	void			AddBackground(HTML_Element* element, const COLORREF &bg_color, const OpRect& rect);
	/** Add pendingtiled backgroundimage for rect. It will be delayed until it must be flushed. It may also be sliced in smaller areas. */
	void			AddBackground(HTML_Element* element, Image& img, const OpRect& dst, const OpPoint& offset, ImageListener* image_listener, int imgscale_x, int imgscale_y, CSSValue image_rendering);
#ifdef CSS_GRADIENT_SUPPORT
	/** Add pending gradient background. Painting will be delayed until backgrounds must be flushed. */
	void			AddBackground(HTML_Element* element, const CSS_Gradient& gradient, OpRect box, const OpRect& gradient_rect, COLORREF current_color, const OpRect& repeat_space, CSSValue repeat_x, CSSValue repeat_y);
#endif // CSS_GRADIENT_SUPPORT
	/** Common functionality for all the AddBackgrounds */
	void			AddBackground(HTML_Element* element);

public:

#ifdef DEBUG_GFX
	void			TestSpeed(int test);
	void			TestSpeedScroll();
#endif

	/**
	 * Sets the scaling to use.
	 *
	 * @param scale A scale 1-100-<large number>, where 100 is "no scaling" and 1 is "very small".
	 */
	virtual void	SetScale(UINT32 scale, BOOL updatesize = TRUE);

	/** Set the text scale for this VisualDevice. VisualDevice only holds this value and does nothing itself.
		See FramesDocument::SetTextScale */
	void			SetTextScale(int scale) { m_text_scale = scale; }

	/** Get text scale for this VisualDevice. */
	int				GetTextScale() { return m_text_scale; }

	/** Use ONLY if something needs to be painted using another scale.
		Returns the current scale which should be restored immediately with another call to SetTemporaryScale.
		Does not update fonts, view, document or anything else than the scale used for graphics transformation. */
	UINT32			SetTemporaryScale(UINT32 scale);

	/** Some printing needs to be done as if in 100% scale instead of the current scale (so it won't appear with blocky pixels). SVG is one example.
		@param rect The rect that you want to draw something unscaled in. A scaled and translated version of this rect will be returned in the VDStateNoScale. */
	VDStateNoScale	BeginNoScalePainting(const OpRect &rect) { return BeginScaledPainting(rect, 100); }
	void			EndNoScalePainting(const VDStateNoScale &state) { EndScaledPainting(state); }

	/** Use if some painting needs to be done using another scale.
	  * @param rect The rect that you want to draw something scaled in. A scaled and translated version of this rect will be returned in the VDStateNoScale. */
	VDStateNoScale	BeginScaledPainting(const OpRect &rect, UINT32 scale);
	void			EndScaledPainting(const VDStateNoScale &state);

	virtual VDCTMState	SaveCTM();
	virtual void		RestoreCTM(const VDCTMState& state);

#if defined(PIXEL_SCALE_RENDERING_SUPPORT) && defined(VEGA_OPPAINTER_SUPPORT)
	/**
	 * Get view port scale, mainly for SVG/Skin rendering.
	 */
	const PixelScaler&	GetVPScale() const { return scaler; }
#endif // PIXEL_SCALE_RENDERING_SUPPORT && VEGA_OPPAINTER_SUPPORT

#ifdef CSS_TRANSFORMS
	/** Overrides for VisualDeviceTransform */
	OP_STATUS		PushTransform(const AffineTransform& afftrans);
	void			PopTransform();

private:
	void			AppendTranslation(int tx, int ty);
	AffineTransform	GetBaseTransform();
	void			UpdatePainterTransform(const AffineTransform& at);

	VDStateTransformed	EnterTransformMode();
	void				LeaveTransformMode();

	/** Save restore state for painting with transform */
	VDStateTransformed	BeginTransformedPainting();
	void				EndTransformedPainting(const VDStateTransformed &state);

	/** Helper for BeginScaledPainting, when a transform is active */
	VDStateNoScale		BeginTransformedScaledPainting(const OpRect &rect, UINT32 scale);

	VDStateTransformed	untransformed_state;
	AffineTransform		vd_screen_ctm;
	AffineTransform		offset_transform;
	bool				offset_transform_is_set;

public:
#endif // CSS_TRANSFORMS

	/** Save restore state for painting with no translation and no offset. */
	VDStateNoTranslationNoOffset	BeginNoTranslationNoOffsetPainting();
	void							EndNoTranslationNoOffsetPainting(const VDStateNoTranslationNoOffset &state);

	/** Scroll rect dx and dy pixels. Negative values moves the area left or up.
		rect is in doc coordinates *without scrolling applied* */
	void			ScrollRect(const OpRect &rect, INT32 dx, INT32 dy);

	/** Scroll clipviews that are children to parent. */
	void			ScrollClipViews(int dx, int dy, CoreView *parent);

	/** Get coordinates of the top left corner */
	void			GetPosition(AffinePos& pos) { pos = win_pos; }

	/** Get coordinates of the top left corner of the document's view.
	 *	The resulting point is relative to the parent view, which is either
	 *	the view of the frame or iframe or the view of the top document.
	 *  E.g. scrollbars' positions may affect the returned value to differ from (0,0). */
	OpPoint			GetInnerPosition();

	/** Get the available height of the screen used by this visualdevice. */
    int				GetScreenAvailHeight();

	/** Get the available width of the screen used by this visualdevice. */
    int				GetScreenAvailWidth();

	/** @return The total number of colors that the main screen is capable of,
		ie the number of bits per pixel times the number of colour planes. */
	int				GetScreenColorDepth();

	/** @returns the height of the screen used by this visualdevice. */
	int				GetScreenHeight();

	/** @returns the height of the screen used by this visual device in CSS (layout) coordinates. */
	int             GetScreenHeightCSS();

	/** Returns the width of the screen used by this visualdevice. */
    int				GetScreenWidth();

	/** @returns the width of the screen used by this visual device in CSS (layout) coordinates. */
	int             GetScreenWidthCSS();

	/** @return the number of bits per pixel for the main screen. */
    int				GetScreenPixelDepth();

	/** @return the position in pixels on the screen that this visualdevice belongs to. */
	OpPoint			GetPosOnScreen();

	/**
	 * Checks if the default printer is monochrome.
	 * This is useful to check before changing color in
	 * printer preview mode for example.
	 * @return TRUE if the default printer is monochrome.
	 */
	BOOL			HaveMonochromePrinter();

	/**
	 * Gets the visible representation of the given color on
	 * white background. This is used when printing on white paper.
	 * White will return black for example.
	 * @param color A color.
	 * @return New color which is visible on white background.
	 */
	static COLORREF	GetVisibleColorOnWhiteBg(COLORREF color);
	static COLORREF	GetVisibleColorOnBgColor(COLORREF color, COLORREF bg_color);
	/**
	 * Gets the visible representation of the given color on an
	 * arbitrary background. if all color component diffs are less
	 * than threshold white or black will be used, depdning on
	 * constrast, otherwize color will be returned unchanged.
	 * @param color A color.
	 * @param bg_color the background color.
	 * @param threshold the threshold value for when contrast is
	   considered too small to be visible.
	 * @return New color which is visible on white background.
	 */
	static UINT32 GetVisibleColorOnBgColor(UINT32 color, UINT32 bg_color, UINT8 threshold);
	/**
	 * Get the gray scale representation of the given color.
	 * @param color A RGB-color to convert.
	 * @return A RGB-color in gray scale.
	 */
	static COLORREF GetGrayscale(COLORREF color);

	/** sets the rendering viewport position (scroll offset within the document(web page being displayed)).
	  * @param x The horizontal position, in document coordinates.
	  * @param y The vertical position, in document coordinates.
	  * @param allow_sync Specify if the invalidated area should be drawed immediately (Using Sync()) or if it should be
	  *                   done next paint. Should only be TRUE if needed and you know that it is safe to do. Painting a
	  *                   document involves reflow and traversing which is probhited if we are already doing that on the stack.
	  * @param updated_area May be NULL. If non-NULL (and non-empty), it specifies an area, in document coordinates,
	  * that has been scheduled for repainting. This area should not be scrolled, since 1: that is a waste of time,
	  * since it has been scheduled for repaining anyway, and 2: that would cause flickering if the area contains
	  * fixed-positioned content.
	  */
	virtual void SetRenderingViewPos(INT32 x, INT32 y, BOOL allow_sync = FALSE, const OpRect* updated_area = NULL);

	/** Sets the size of the document (The big area to scroll within) */
	virtual void SetDocumentSize(UINT32 w, UINT32 h, int negative_overflow = 0);

	int				GetDocumentWidth() {return doc_width;}
	long			GetDocumentHeight() {return doc_height;}

	void			InvalidateScrollbars();

	/** Update the visibility status of the scrollbars according to current ScrollType and document size.
		This call may change the size of the document so that a reflow might occur eventually.
		If the document size had shrunk, the scrollbars values will be updated causing a scroll back into the allowed position. */
	void			UpdateScrollbars();

	/** Calculate the distance to scroll when the user wants to scroll one page
	 * downwards, upwards, leftwards or rightwards.
	 *
	 * @param page_size Width or height of visible area.
	 * @return Amount to scroll when scrolling one page.
	 */
	static int		GetPageScrollAmount(int page_size)
	{
		/* Browsers seem to agree that scrolling 87.5% of the page size is The
		   Right Thing (TM). */

		return page_size * 7 / 8;
	}

	/** Sets the view. Normal visualdevice's creates a view itself so this is needed rarely! */
	void			SetView(CoreView* view) { this->view = view; }

	/** The CoreView that contains and draws the document. There is also a CoreView in the container, which this view is a child of. That view contains the scrollbar. */
	CoreView*			view;

	/** Sends the message MSG_VISDEV_UPDATE to itself with the specified
	 * timeout. On receiving the message OnTimeOut() is called. If the message
	 * was already sent (and is waiting in the message-queue), then no new
	 * message is sent.
	 * @param time is the timeout to use.
	 * @param restart If the argument is TRUE, then this method ensures that the
	 *        message is scheduled for the specified timeout, i.e., if there is
	 *        already a message in the message-queue, it is rescheduled with the
	 *        specified \p time. If the argument is FALSE (default value) the
	 *        method only sends a new message with the specified timeout, if
	 *        there is no message waiting in the queue.
	 * @return TRUE if a message was successfully sent.
	 */
	BOOL			StartTimer(INT32 time, BOOL restart=FALSE);
	void			StopTimer();
	/** Informs the VisualDevice that loading has started. */
	void			StartLoading();
	/** Informs the VisualDevice that loading has stopped, thus it's a
	 * good time to unlock the display for updates, if locked. */
	void			StopLoading();
	/** Informs the VisualDevice that a document was just created,
	 * thus it's a good time to lock the display from updates. */
	void			DocCreated();

	/** Informs the Visual Device that all known styles were applied
	 * thus it's a good time to perform a styled display.
	 */
	void			StylesApplied();

	/** Informs the VisualDevice that a Javascript dialog is
	 * shown. Since there won't be a StopLoading() until user has
	 * replied to dialog, it might be a good time to unlock and
	 * display page. */
	void			JSDialogShown()
	{
#ifdef STYLED_FIRST_DISPLAY
		OnTimeOut();
#endif // STYLED_FIRST_DISPLAY
	}

	void			LoadingFinished();

	/**	Lock or unlock updating of this visualdevice.
		As long as update is locked, no Update-calls will invalidate the view. When the update is unlocked, the union of the Update-rects made
		when it was locked will be Invalidated on the view.
		Note: the visualdevice may still get paint events if something out of VisualDevice's control has invalidated the view (f.ex the system).
		May be called nested.
	*/
	void			LockUpdate(BOOL lock);
	BOOL			IsLocked() {return m_lock_count > 0;}

	/** Return TRUE if this VisualDevice is currently having a painter. */
	BOOL			IsPainting() { return painter ? TRUE : FALSE; }

	/** Return TRUE if this VisualDevice is currently being painted to screen. That means, OpView::OnPaint has been called
		and it is drawing. If the VisualDevice is being drawn into a OpBitmap outside a OnPaint, it will return FALSE. */
	BOOL			IsPaintingToScreen();

	/** return TRUE if this VisualDevice is currently in a scaled painting session (BeginScaledPainting has been called) */
	BOOL			IsPaintingScaled() { return painting_scaled > 0; }

	/** Update rect, and make sure it is included in the next OnPaint. */
	void			ForcePendingPaintRect(const OpRect& rect);

	/** Begin adding rects to this outline. To support nested outline, if an outline is already currently active, this function
		must push that ouline, and pop it again in EndAddOutline.
		offset should be the outline-offset value. Note that the rectangles to PushOutlineRect are not affected by offset automatically.
	 */
	OP_STATUS		BeginOutline(int width, COLORREF color, int pen_style, HTML_Element *element = NULL, int offset = 0);

	/** Stop adding rects to this outline. Must pop other active outlines, if there are. */
	void			EndOutline();

	/** Add rect to a currently open outline */
	OP_STATUS		PushOutlineRect(const OpRect& rect);

	/** Return TRUE if specified element has open outline and that outline
	    equals currently open outline. */
	BOOL			HasCurrentlyOpenOutline(HTML_Element* element) const;

	/** Content type used for SetContentFound. */
	enum DISPLAY_CONTENT {
		DISPLAY_CONTENT_TEXT,	///< Text content found
		DISPLAY_CONTENT_IMAGE	///< Image content found
	};

	/** Call when content of different types are traversed. Will be used to determine what kind of skin to draw for outlines with
		skinned style (if supported with FEATURE_HIGHLIGHT_BORDER). */
	void			SetContentFound(DISPLAY_CONTENT content);

	/** Draw open outlines on the screen */
	OP_STATUS		DrawOutlines();

	/** Enlarge the rect so that it includes any intersecting outline regions.
	   Returns TRUE if rect was enlarged, FALSE if nothing happened. */
	BOOL			EnlargeWithIntersectingOutlines(OpRect& rect);

	/** Remove intersecting or nonintersecting outline regions from the cache. */
	void			RemoveIntersectingOutlines(const OpRect& rect, BOOL remove_intersecting);

	void			UpdateAddedOrChangedOutlines(const OpRect& paint_rect);

	void			RemoveOutline(VisualDeviceOutline* o);
	void			RemoveAllOutlines();
	void			RemoveAllTemporaryOutlines();

	/** Traverse the tree to see if any coreviews have moved out of the screen after the
		previous reflow. */
	OP_STATUS		CheckCoreViewVisibility();

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibleDocument*			GetAccessibleDocument();
	virtual BOOL		AccessibilityIsReady() const {return TRUE;}
	virtual BOOL		AccessibilitySetFocus();
	virtual OP_STATUS	AccessibilityGetText(OpString& str);
	virtual OP_STATUS	AccessibilityGetAbsolutePosition(OpRect &rect);
	virtual Accessibility::ElementKind AccessibilityGetRole() const;
	virtual Accessibility::State AccessibilityGetState();
	virtual int	GetAccessibleChildrenCount();
	virtual OpAccessibleItem* GetAccessibleParent();
	virtual OpAccessibleItem* GetAccessibleChild(int);
	virtual int GetAccessibleChildIndex(OpAccessibleItem* child);
	virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);
	virtual OpAccessibleItem* GetNextAccessibleSibling();
	virtual OpAccessibleItem* GetPreviousAccessibleSibling();
	virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf();
	virtual OpAccessibleItem* GetLeftAccessibleObject();
	virtual OpAccessibleItem* GetRightAccessibleObject();
	virtual OpAccessibleItem* GetDownAccessibleObject();
	virtual OpAccessibleItem* GetUpAccessibleObject();

	virtual OP_STATUS GetAbsoluteViewBounds(OpRect& bounds);

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

protected:
	Head			backbuffers;		///< List of VisualDeviceBackBuffer
	Head			outlines;			///< List of VisualDeviceOutline
	OpPointerHashTable<HTML_Element, VisualDeviceOutline>
					outlines_hash;		///< Hashtable representation of outlines with element identifiers
	Head			temporary_outlines;	///< Outlines that will be removed but are saved for comparison
	OpPointerHashTable<HTML_Element, VisualDeviceOutline>
					temporary_outlines_hash;		///<Hashtable representation of the temporary outlines with a html element
#ifdef DISPLAY_SPOTLIGHT
	OpAutoVector<VDSpotlight>
					spotlights;
#endif // DISPLAY_SPOTLIGHT
	int				outlines_open_count;///< Number of currently open outlines
	VisualDeviceOutline *current_outline;///< Current outline (between its BeginOutline and EndOutline)
	BOOL            m_outlines_enabled;

	OpRect pending_paint_rect;					///< A paint rect that must be included in the next OnPaint. See ForcePendingPaintRect.
	OpRect pending_paint_rect_onbeforepaint;	///< The state of pending_paint_rect in OnBeforePaint.

	BgClipStack		bg_cs;
	BOOL			check_overlapped;
#ifdef _PLUGIN_SUPPORT_
	CoreViewClipper coreview_clipper; ///< contains all plugin clipviews owned directly by this visual device
	CoreViewClipper plugin_clipper; ///< contains all plugin clipviews owned by visual device "children"
	VisualDevice* m_clip_dev; ///< pointer to "parent" visual device holding copies of plugin clipviews
#endif // _PLUGIN_SUPPORT_
	WidgetContainer* container;		///< Handles input and display of the scrollbars. Also has view as child (that displays the document).
	OpScrollbar*	v_scroll;
	OpScrollbar*	h_scroll;
	OpWindowResizeCorner* corner;			///< The grooved corner at bottom right in the window which let the user resize it.
	int				doc_width;
	int				negative_overflow;
	long			doc_height;
	BOOL			v_on, h_on;		///< Wheter a scrollbar is currently on or off
	BOOL			pending_auto_v_on; ///< Whether vertical scrollbar should be turned on or off, if mode is VD_SCROLLING_AUTO
	BOOL			pending_auto_h_on; ///< Whether horizontal scrollbar should be turned on or off, if mode is VD_SCROLLING_AUTO
	INT				step;			///< only allow doc pos rounded to multiples of step
	int				painting_scaled;///< Whether we are within a BeginScaledPainting / EndScaledPainting block

	// window area
	AffinePos		win_pos;
	int				win_width;
	int				win_height;

	// Valid during display. It is the rect in doc coordinates that is being displayed.
	OpRect			doc_display_rect;

	// Valid during display. It is the rect in doc coordinates that is being displayed, not enlarged by outlines or forced pending paintrects.
	OpRect			doc_display_rect_not_enlarged;

	OpRect			rendering_viewport;

	// scaled scroll offset that should be used to translate painting.
	int				view_x_scaled;
	int				view_y_scaled;

	/** Update i_dx and i_dy */
	void			UpdateScaleOffset();

	ScrollType		scroll_type;

	DocumentManager* doc_manager;

	int				current_font_number;

	COLORREF		bg_color;
	COLORREF  		bg_color_light;
	COLORREF 		bg_color_dark;
	BOOL			use_def_bg_color;
	COLORREF 		color;

	int				char_spacing_extra;

	FontAtt			logfont;
	short			accurate_font_size;

	OP_STATUS		CheckFont();

	void			Free(BOOL destructing);

	BOOL			m_draw_focus_rect;

	int				m_text_scale;
	INT32			m_layout_scale_multiplier;
	INT32			m_layout_scale_divider;

	/** Try to lock the VisualDevice to prepare for a new page.
	 *
	 * @param delay delay in ms before lock should time out, unless released before
	 * @return TRUE if we locked the VisualDevice
	 */
	BOOL            TryLockForNewPage(int delay);

	VisualDeviceBackBuffer*
					m_cachedBB;

	List<BoxShadowCorner> box_shadow_corners;

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	/** Viewport scaler for font measurement */
	PixelScaler		scaler;
#endif // PIXEL_SCALE_RENDERING_SUPPORT

public:

	BOOL			GetDrawFocusRect() {return m_draw_focus_rect;}
	void			SetDrawFocusRect(BOOL draw_focus_rect) {m_draw_focus_rect = draw_focus_rect;}

	INT32			LayoutScaleToDoc(INT32 v) const;
	INT32			LayoutScaleToScreen(INT32 v) const;
	UINT32			GetLayoutScale() const { return m_layout_scale_multiplier * 100 / m_layout_scale_divider; }
	void			GetLayoutScale(INT32 *num, INT32 *denom) const { *num = m_layout_scale_multiplier; *denom = m_layout_scale_divider; }
	void			SetLayoutScale(UINT32 scale);

private:
	/**
	 * Returns TRUE if the scale is 100%, i.e. no scale operations are
	 * done. Faster than "GetScale() == 100"
	 */
	inline BOOL IsScaled() const { return scale_multiplier != scale_divider; }

	/** Move scrollbars to the correct place in the container (Should be done if it has been resized) */
	void			MoveScrollbars();

	/** update a edge of the inverted border drawed by DrawWindowBorder */
	void			UpdateWindowBorderPart(BOOL left, BOOL top, BOOL right, BOOL bottom);

	/** Adds enough margin around rect to also cover a highlight area around something with the same size as the initial rect. */
	void	PadRectWithHighlight(OpRect& rect);

	/** @short Resize CoreView.
	 *
	 * Will NOT trigger javascript events or reflows. VisualDevice should never
	 * do that.
	 */
	void			ResizeViews();

	/** Returns TRUE if vertical scrollbars should be placed to the left, takes preferences and the associated logical document Right-To-Left style into account */
	BOOL			LeftHandScrollbar();

	void			HideIfFrame();
	void			UnhideIfFrame();

	BOOL			m_hidden_by_lock;
	BOOL			m_hidden;
	INT32			m_lock_count;
	OpRect			m_update_rect;
	BOOL			m_update_all;
	BOOL			m_include_highlight_in_updates;

	// X-position of view has been increased due to left-side scrollbar on Right-To-Left document
	BOOL			m_view_pos_x_adjusted;

	void			OnTimeOut();
	BOOL            m_posted_msg_update;
	BOOL            m_posted_msg_mousemove;
	void			RemoveMsgUpdate();
	void			RemoveMsgMouseMove();
	double			m_post_msg_update_ms; /**< The GetRuntimeMS time the update message was posted. */
	INT32			m_current_update_delay; /**< ms delay of the last paint timer. */

	BOOL			GetShouldAutoscrollVertically() const;
	BOOL			m_autoscroll_vertically;

#ifdef FEATURE_SCROLL_MARKER
	OpScrollMarker*	m_scroll_marker;
#endif //FEATURE_SCROLL_MARKER

    /**
	 * Scale factor stored as a fraction so that we can work with
	 * small numbers that makes overflowing multiplications less
	 * likely. Signed numbers to make it simpler to work with when
	 * calculating scaled negative numbers. scale_multiplier ==
	 * scale_divider == 1 signifies no scaling.
	 *
	 * The scale factor is used to go from document size to screen
	 * size so that screen_size =
	 * (document_size*scale_multiplier)/scale_divider.
	 */
	INT32 scale_multiplier;
	INT32 scale_divider;

#ifdef PAN_SUPPORT
	INT32 panLastX, panNewX;
	INT32 panLastY, panNewY;
	INT32 rdx, crdx;
	INT32 rdy, crdy;
	BOOL3 isPanning;
	int panOldCursor;
#endif // PAN_SUPPORT

	//Cache for screen properties
	OpScreenPropertiesCache m_screen_properties_cache;

	/** Keeps a local 'is-active' state for all painting, uses BeginPaint and EndPaint.
	    Used to detect if opera is idle, important for testing */
	OpActivity activity_paint;

public:

	/** Calls to Update may delay the actual Invalidate on the underlying CoreViewContainer (in the end OpView).
		Calling this function will make sure those Invalidates are done to the View. */
	void			SyncDelayedUpdates();

	/** Call windowcommander with OnUpdatePopupProgress */
	void			UpdatePopupProgress();

	/** @short Get rendering viewport X position, in document coordinates. */
	int             GetRenderingViewX() const { return rendering_viewport.x; }
	/** @short Get rendering viewport Y position, in document coordinates. */
	int             GetRenderingViewY() const { return rendering_viewport.y; }
	/** @short Get rendering viewport, in document coordinates. */
	const OpRect&	GetRenderingViewport() const { return rendering_viewport; }

	 /** Gets the rectangle (in document coordinates) of the document currently handled by this VisualDevice
	  *	 that is visible on physical screen, may be empty.
	  *	 In other words, the method returns only that part of the current document that is inside the top document's visual viewport. */
	OpRect			GetVisibleDocumentRect();

	/** Gets the rectangle (in document coordinates) of the document currently handled by this VisualDevice
	 *	that has the following properties (with the current core view hierarchy state):
	 *	1) If we intersect every rectangle inside the document with the returned rectangle,
	 *	the remaing part won't be painted over any ancestor or sibling view.
	 *	2) The returned rectangle is as big as possible and we don't care about hyphotetical painting outside the screen. */
	OpRect			GetDocumentInnerViewClipRect();

	void			TranslateView(int x, int y);
	void            ClearViewTranslation();
	void			ClearTranslation();

	/** Return the x offset this VisualDevice (it's CoreView) has in the OpView (nearest parent CoreViewContainer). */
	int				GetOffsetX() const { return offset_x; }
	/** Return the y offset this VisualDevice (it's CoreView) has in the OpView (nearest parent CoreViewContainer). */
	int				GetOffsetY() const { return offset_y; }

	BOOL			size_ready;

	/** Mark this visual device as fixed positioned inside parent.
	 * @param fixed The first fixed positioned ancestor in the ancestor document.
	 *		  NULL means not fixed positioned.
	 */
	void			SetFixedPositionSubtree(HTML_Element* fixed);

	/** Handle message. */
	void			HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

protected:

	void			DisplayCenteredString(int x, int y, int extent, int width, const uni_char* txt, int len);

#ifdef FONTSWITCHING
	void			TxtOut_FontSwitch(int x, int y, int iwidth, uni_char* txt, int len, const WritingSystem::Script script, BOOL draw=TRUE);
#else
	/** @return The needed height for the string. */
	int				TxtOut_WordBreakAndCenter(int x, int y, int iwidth, uni_char* txt, int len, BOOL draw=TRUE);
#endif // FONTSWITCHING

	void			SetPainter(OpPainter* painter);

public:
	/** Determine if an image with the displayed width/height will
		need to be interpolated when painted. */
	BOOL			ImageNeedsInterpolation(const Image& img, int img_doc_width, int img_doc_height) const;

	friend class ImageSource; // Uses the painter to draw bitmaps
	friend class PaintListener;
	friend class FlattenerObject; // Mini renderserver
	friend class VisualDeviceOutline; // for painter SetClipRect
	friend class CSS_Gradient; // Needs ToPainterExtent
	friend class CSS_RadialGradient; // Needs ToPainterExtent
	friend class CSS_LinearGradient; // Needs ToPainterExtent
	friend BOOL ScrollDocument(FramesDocument* doc, OpInputAction::Action action, int times, OpInputAction::ActionMethod method);

	OpPainter*		painter;

	OpFont*			currentFont;

	CoreViewPaintListener* paint_listener;

#ifndef MOUSELESS
	CoreViewMouseListener* mouse_listener;
#endif // !MOUSELESS

#ifdef TOUCH_EVENTS_SUPPORT
	CoreViewTouchListener* touch_listener;
#endif // TOUCH_EVENTS_SUPPORT

	class ScrollListener* scroll_listener;

#ifdef DRAG_SUPPORT
	CoreViewDragListener* drag_listener;
#endif

private:
	// Painting coordinate system helpers
	OpRect ToPainter(const OpRect& r)
	{
#ifdef CSS_TRANSFORMS
		if (HasTransform())
			return r;
#endif // CSS_TRANSFORMS
		return OffsetToContainerAndScroll(ScaleToScreen(r));
	}
	OpPoint ToPainter(const OpPoint& p)
	{
#ifdef CSS_TRANSFORMS
		if (HasTransform())
			return p;
#endif // CSS_TRANSFORMS
		return OffsetToContainerAndScroll(ScaleToScreen(p));
	}
	OpRect ToPainterNoScroll(const OpRect& r)
	{
#ifdef CSS_TRANSFORMS
		// FIXME: Should we really expect this function to be called when in transform context?
		if (HasTransform())
			return r;
#endif // CSS_TRANSFORMS
		return OffsetToContainer(ScaleToScreen(r));
	}
	OpPoint ToPainterNoScale(const OpPoint& p)
	{
#ifdef CSS_TRANSFORMS
		// FIXME: Should we really expect this function to be called when in transform context?
		if (HasTransform())
			return p;
#endif // CSS_TRANSFORMS
		return OffsetToContainerAndScroll(p);
	}
	OpRect ToPainterNoScale(const OpRect& r)
	{
#ifdef CSS_TRANSFORMS
		// FIXME: Should we really expect this function to be called when in transform context?
		if (HasTransform())
			return r;
#endif // CSS_TRANSFORMS
		return OffsetToContainerAndScroll(r);
	}
	OpRect ToPainterNoScaleNoScroll(const OpRect& r)
	{
#ifdef CSS_TRANSFORMS
		// FIXME: Should we really expect this function to be called when in transform context?
		if (HasTransform())
			return r;
#endif // CSS_TRANSFORMS
		return OffsetToContainer(r);
	}
	OpRect ToPainterDecoration(const OpRect& r)
	{
#ifdef CSS_TRANSFORMS
		if (HasTransform())
			return r;
#endif // CSS_TRANSFORMS
		return OffsetToContainerAndScroll(ScaleDecorationToScreen(r.x, r.y, r.width, r.height));
	}

	INT32 ToPainterExtent(INT32 v)
	{
#ifdef CSS_TRANSFORMS
		if (HasTransform())
			return v;
#endif // CSS_TRANSFORMS
		return ScaleToScreen(v);
	}
	OpPoint ToPainterExtent(const OpPoint& p)
	{
#ifdef CSS_TRANSFORMS
		if (HasTransform())
			return p;
#endif // CSS_TRANSFORMS
		return ScaleToScreen(p);
	}
	OpRect ToPainterExtent(const OpRect& r)
	{
#ifdef CSS_TRANSFORMS
		if (HasTransform())
			return r;
#endif // CSS_TRANSFORMS
		return ScaleToScreen(r);
	}

	double ToPainterExtentDbl(double v)
	{
#ifdef CSS_TRANSFORMS
		if (HasTransform())
			return v;
#endif // CSS_TRANSFORMS
		return ScaleToScreenDbl(v);
	}

#ifdef VEGA_OPPAINTER_SUPPORT
	OpDoublePoint ToPainterExtentDbl(const OpDoublePoint& p)
	{
#ifdef CSS_TRANSFORMS
		if (HasTransform())
			return p;
#endif // CSS_TRANSFORMS
		return ScaleToScreenDbl(p);
	}
#endif // VEGA_OPPAINTER_SUPPORT

	INT32 ToPainterExtentLineWidth(INT32 line_width)
	{
#ifdef CSS_TRANSFORMS
		if (HasTransform())
			return line_width;
#endif // CSS_TRANSFORMS
		return ScaleLineWidthToScreen(line_width);
	}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibleDocument*			m_accessible_doc;
#endif

public:
	// Implementing OpInputContext interface

	virtual void			SetFocus(FOCUS_REASON reason);

	virtual BOOL			OnInputAction(OpInputAction* action);
	virtual const char*		GetInputContextName() { return "Browser Widget"; }

	virtual void			OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);
	virtual void			OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);
#ifdef PLATFORM_FONTSWITCHING
	virtual void 			OnAsyncFontLoaded();
#endif

	virtual BOOL			IsInputContextAvailable(FOCUS_REASON reason);
	virtual Type            GetType() {return VISUALDEVICE_TYPE;}

	// Returns a sane value for blur-radius.
	short LimitBlurRadius(short radius)
	{
		radius = MAX(1, radius);
		radius = MIN(radius, 100);
		return radius;
	}

	/** Return the width that the 2 borders should have that make up a "double" border.
		Note that for widths less than 3 pixels, there should be only one border (border_w is just returned) */
	int GetDoubleBorderSingleWidth(int border_w);

	/** tell visual device that screen has changed, could be that window is moved
	 * from a screen to another, OR that the properties of the current
	 * screen has changed
	 */
	void ScreenPropertiesHaveChanged();

	/** Should be called when the window's visibility has changed. */
	void OnVisibilityChanged(BOOL vis);
};

// Declarations of depricated inlines.
inline void VisualDevice::DrawSplitter(int x, int y, int w, int h) {}

#endif // _VISUAL_DEVICE_H_
