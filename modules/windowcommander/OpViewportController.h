/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef OPVIEWPORTCONTROLLER_H
#define OPVIEWPORTCONTROLLER_H

#include "modules/util/OpRegion.h"
#include "modules/util/simset.h"

class OpViewportRequestListener;
class OpViewportInfoListener;

/** @short Viewport controller.
 *
 * Opera works on three different viewports, when it comes to displaying web
 * pages:
 *
 * - Visual viewport: The peephole. This viewport is what the user can see on
 * the screen. The size of this viewport is initially locked to the window
 * size, but may be unlocked by calling by calling
 * LockVisualViewportSize(FALSE) and then resized independently of the window,
 * by calling SetVisualViewport().
 *
 * - Layout viewport: The initial containing block. Identical to the viewport
 * defined in the CSS 2.1 specification. All calculations in layout/CSS
 * relative to the viewport is relative to the layout viewport. That includes
 * initial containing block size and positioning of fixed elements. It is also
 * the viewport used for evaluating CSS3 Media Queries. The layout viewport is
 * only really interesting to core, and does not play a major part in this
 * class. It will be moved automatically, based on visual viewport position
 * changes, among other things. It will be resized depending on page content,
 * rendering modes and window size.
 *
 * - Rendering viewport: The canvas; paint area. This comprises the part of the
 * web page that core will paint, i.e. the OpView. This may be larger than the
 * two other viewports, so that content outside those viewports may be painted,
 * in order to improve panning/scrolling performance and smoothness. Its
 * position is set by calling SetRenderingViewportPos(). Its size is determined
 * by OpWindow::GetRenderingBufferSize().
 *
 * See also:
 * http://projects/Core2profiles/specifications/mobile/rendering_modes.html#viewports
 *
 * All viewports are specified in top-level logical document coordinates and
 * dimensions. This means that changing rendering scaling by calling
 * SetRenderingScale() (or SetScale() in OpWindowCommander, for that matter)
 * will not affect the viewports directly.
 *
 * Additionally, there is "window size", which is the (inner) size of an
 * OpWindow. This is the actual browser window size, in pixels. Under "normal"
 * desktop browsing circumstances the size of the visual, layout and rendering
 * viewport, and the window size, are all the same.
 *
 * OpViewportController is a class for the user interface to control the
 * viewports in a specific core window. Core owns the layout viewport, which
 * the user interface code may not affect directly. The user interface code
 * owns the visual viewport and rendering viewport. Core may not change the
 * visual viewport directly, but may request that it be changed (size and/or
 * position) by calling designated methods in OpViewportRequestListener. Then
 * it is up to the user interface whether or not to honor the request. It is
 * also up to the user interface to decide the position of the rendering
 * viewport, and to make sure that it is moved when as needed in order to keep
 * the visual viewport within it.
 *
 * If no viewport request listener is set, a default viewport request listener
 * implemented in core will be used. It will simply honor all visual viewport
 * change requests, and make sure that the rendering viewport follows the
 * visual viewport. In other words, it will behave like a desktop browser.
 */
class OpViewportController
{
public:
	virtual ~OpViewportController() { }

	/** @short Get viewport request listener.
	 *
	 * Gets the viewport request listener that core uses to notify the user
	 * interface about viewport change requests.
	 *
	 * @return The current viewport request listener. If no listener has been
	 * set using SetViewportRequestListener() the default listener is returned.
	 */
	virtual OpViewportRequestListener* GetViewportRequestListener() const = 0;

	/** @short Set viewport request listener.
	 *
	 * Sets the viewport request listener that core will use to notify the user
	 * interface about change requests.
	 *
	 * @param listener Listener. NULL is allowed, in which case the listener
	 * will be implemented inside of core, which will ensure
	 * desktop-browser-like behavior.
	 */
	virtual void SetViewportRequestListener(OpViewportRequestListener* listener) = 0;

	/** @short Set viewport info listener.
	 *
	 * Sets the viewport change listener that core will use to notify the user
	 * interface about viewport-related changes.
	 *
	 * @param listener Listener. NULL is allowed, if the user interface doesn't
	 * need any viewport-related information.
	 */
	virtual void SetViewportInfoListener(OpViewportInfoListener* listener) = 0;

	/** @short Lock or unlock visual viewport size.
	 *
	 * When the visual viewport size is locked, its size will automatically
	 * follow the window size, and size change requests via SetVisualViewport()
	 * will be ignored. Initially, an OpViewportController has the size locked,
	 * so that it always follows window size. Regardless of lockedness, visual
	 * viewport position may be changed.
	 *
	 * @param lock TRUE to lock the visual viewport size (initial state), FALSE
	 * to unlock it (so that visual viewport size will become independent on
	 * window size). When the visual viewport becomes unlocked, its size will
	 * correspond to the window size at the time of unlocking.
	 */
	virtual void LockVisualViewportSize(BOOL lock) = 0;

	/** @short Enable or disable buffered mode.
	 *
	 * Initially, buffered mode is disabled.
	 *
	 * In buffered mode, certain operations on the OpViewportController may be
	 * buffered, and not be carried out before FlushBuffer() is called. This
	 * may improve performance and prevent flickering. For example, if the API
	 * user wants to set both rendering viewport and visual viewport at the
	 * same time, this may cause flickering of fixed-positioned elements if the
	 * two operations are carried out separately. It is not defined which
	 * operations on an OpViewportController that are buffered, but the API
	 * user is required to call FlushBuffer() whenever it is necessary to carry
	 * out the operations. Before FlushBuffer() is called, the getters may well
	 * return outdated information; if for example GetVisualViewport() follows
	 * a call to SetVisualViewport(), without an intervening call to
	 * FlushBuffer(), it is not defined whether GetVisualViewport() returns the
	 * old or the new viewport.
	 *
	 * @param buffered_mode TRUE to enable buffered mode, FALSE to disable
	 * it. When disabling buffered mode, a FlushBuffer() is implied.
	 */
	virtual void SetBufferedMode(BOOL buffered_mode) = 0;

	/** @short Flush buffered operations.
	 *
	 * Has no effect if buffered mode is disabled.
	 *
	 * In buffered mode, any pending operations on this OpViewportController
	 * will now be carried out.
	 */
	virtual void FlushBuffer() = 0;

	/** @short Set desktop layout viewport size.
	 *
	 * Normally, for a web page, the layout viewport size is calculated from
	 * the window size, but this method can be used to override that for web
	 * pages designed for a desktop web browser. This allows for desktop-like
	 * layout on a device whose screen is smaller than a typical window in a
	 * desktop browser.
	 *
	 * Initially for an OpViewportController, desktop layout viewport width and
	 * height are both set to 0 -- auto -- which means that the desktop layout
	 * viewport size simply follows window size. Either width or height, or
	 * both width and height may be set to a non-zero value. If either
	 * dimension is zero, that zero value is resolved to a proper value by
	 * using the window's aspect ratio.
	 *
	 * The desktop layout viewport size will only be used on web pages that
	 * don't specify their own viewport size (via the viewport META tag). The
	 * desktop layout viewport will not be used on pages that are recognized as
	 * "mobile" web pages (WML, mobile XHTML, etc.). FlexRoot may also override
	 * desktop layout viewport size.
	 *
	 * @param width New desktop layout viewport width. 0 means auto.
	 * @param height New desktop layout viewport height. 0 means auto.
	 */
	virtual void SetDesktopLayoutViewportSize(unsigned int width, unsigned int height) = 0;

	/** @short Get the current visual viewport. */
	virtual OpRect GetVisualViewport() = 0;

	/** @short Set the visual viewport.
	 *
	 * Visual viewport size must not be locked (see LockVisualViewportSize() -
	 * initially, an OpViewportController has the size locked); otherwise
	 * viewport size change request will be ignored (positioning requests will
	 * still be honored, though).
	 *
	 * It is perfectly legal to position the visual viewport outside the
	 * rendering viewport, but that is typically a bad idea, since there will
	 * be nothing to see there. In other words, when changing the visual
	 * viewport, also make sure that the rendering viewport is changed if
	 * necessary, so that it contains the visual viewport.
	 *
	 * Core will automatically change the layout viewport position to better
	 * contain the new visual viewport.
	 *
	 * @param viewport New visual viewport.
	 */
	virtual void SetVisualViewport(const OpRect& viewport) = 0;

	/** @short Set visual viewport position.
	 *
	 * It is perfectly legal to position the visual viewport outside the
	 * rendering viewport, but that is typically a bad idea, since there will
	 * be nothing to see there. In other words, when changing the visual
	 * viewport, also make sure that the rendering viewport is changed if
	 * necessary, so that it contains the visual viewport.
	 *
	 * Core will automatically adjust the layout viewport position to better
	 * contain the new visual viewport, if necessary.
	 *
	 * @param pos New top left corner of the visual viewport.
	 */
	virtual void SetVisualViewportPos(const OpPoint& pos) = 0;

#ifdef PAGED_MEDIA_SUPPORT
	/** @short Set the current page number.
	 *
	 * Only useful in paged mode (paged overflow on viewport, presentation
	 * mode, etc.), not in continuous mode. Calling this method may trigger a
	 * call to OpViewportRequestListener::OnVisualViewportChangeRequest().
	 *
	 * @param page_number The new page number (the first page is page 0). If
	 * the specified page number is negative, go to the first page. If the
	 * specified page number is >= GetTotalPageCount(), go to the last page.
	 */
	virtual void SetCurrentPageNumber(int page_number) = 0;

	/** @short Get the current page number.
	 *
	 * Only useful in paged mode (paged overflow on viewport, presentation
	 * mode, etc.), not in continuous mode.
	 *
	 * @return The current page number (the first page is page 0), or -1 if we
	 * are not in paged mode.
	 */
	virtual int GetCurrentPageNumber() = 0;

	/** @short Get the total number of pages.
	 *
	 * Only useful in paged mode (paged overflow on viewport, presentation
	 * mode, etc.), not in continuous mode.
	 *
	 * @return The total number of pages (the first page is page 0), or -1 if
	 * we are not in paged mode.
	 */
	virtual int GetTotalPageCount() = 0;
#endif // PAGED_MEDIA_SUPPORT

	/** @short Make sure no text paragraphs get wider than visual viewport width.
	 *
	 * In this mode, Opera will override the spec for text layout, if following
	 * the spec would give a text paragraph wider than the specified maximum
	 * width.
	 *
	 * Calling this method will typically trigger a layout reflow.
	 *
	 * @param max_paragraph_width Maximum text paragraph width. This could be
	 * the current visual viewport width, for instance.
	 *
	 * @param pos Current point of interest. Whatever content is at this point
	 * should remain within the visual viewport after reflow. A reflow will
	 * typically cause content to be moved around. Specifying a point here will
	 * give core a hint about what the user is currently looking at, so that
	 * core can request a new visual viewport position after reflow. This point
	 * could typically be the point where the user clicked/tapped last, the
	 * current "mouse" cursor position, or something like that. If this
	 * parameter is NULL, core will still attempt to guess what the user was
	 * looking at, but this is obviously not always going to be successful. In
	 * other words, if possible, specifying a point is highly recommended.
	 */
	virtual void SetTextParagraphWidthLimit(unsigned int max_paragraph_width, const OpPoint* pos = NULL) = 0;

	/** @short Wrap text to according to spec.
	 *
	 * This is the initial behavior. FIXME: conflicts with "Limited Paragraph
	 * Width" preference.
	 *
	 * Calling this method will typically trigger a layout reflow.
	 *
	 * @param pos Current point of interest. Whatever content is at this point
	 * should remain within the visual viewport after reflow. A reflow will
	 * typically cause content to be moved around. Specifying a point here will
	 * give core a hint about what the user is currently looking at, so that
	 * core can request a new visual viewport position after reflow. This point
	 * could typically be the point where the user clicked/tapped last, the
	 * current "mouse" cursor position, or something like that. If this
	 * parameter is NULL, core will still attempt to guess what the user was
	 * looking at, but this is obviously not always going to be successful. In
	 * other words, if possible, specifying a point is highly recommended.
	 */
	virtual void DisableTextParagraphWidthLimit(const OpPoint* pos = NULL) = 0;

	/** @short Get the current rendering viewport. */
	virtual OpRect GetRenderingViewport() = 0;

	/** @short Set rendering viewport position.
	 *
	 * When the visual viewport is changed, the rendering viewport position
	 * should be changed if necessary, to keep the visual viewport contained by
	 * the rendering viewport. Apart from that, positioning of the rendering
	 * viewport is up to the platform. Changing the rendering viewport will
	 * cause the rendering buffer (OpView) to be scrolled and repainted.
	 *
	 * @param pos New top left corner of the rendering viewport.
	 */
	virtual void SetRenderingViewportPos(const OpPoint& pos) = 0;

	/** @short Set rendering buffer scale.
	 *
	 * Calling this method is the same as calling OpWindowCommander::SetScale()
	 * with "true zoom" enabled. Changing the rendering buffer scale should not
	 * affect the other properties of this API (such as visual viewport size,
	 * document size, and so on, since those values are in document coordinates
	 * and dimensions). Calling this method will automatically enable "true
	 * zoom".
	 *
	 * @param scale_percentage New scale level, in percent. 100% means
	 * one-to-one mapping between logical document pixels and pixels in the
	 * rendering buffer. 200% means that a 1x1 area in the document maps to a
	 * 2x2 area in the rendering buffer.
	 */
	virtual void SetRenderingScale(unsigned int scale_percentage) = 0;

	/** @short Set scale used for layout calculations in TrueZoom.
	 *
	 * Other zoom levels will get a font size proportional to this size.
	 *
	 * This scale is also used as a base scale when resolving layout
	 * values that are relative to the viewport.
	 *
	 * Along with the appropriate zoom level, this acts as a way to set
	 * the relation between physical pixels and CSS pixels, also known as
	 * the device pixel ratio.
	 *
	 * @param scale_percentage Base scale for physical pixels vs CSS pixels as
	 *        as a percentage value.
	 */
	virtual void SetTrueZoomBaseScale(unsigned int scale_percentage) = 0;

	/** @short Get the position and size of the layout viewport.
	 *
	 * This information may for instance be used to set a suitable visual
	 * viewport when "zoomed out".
	 *
	 * @return The layout viewport
	 */
	virtual OpRect GetLayoutViewport() = 0;

	/** @short Get the size of the document.
	 *
	 * This will get the size of the content in the document. This information
	 * may for instance be used to adjust the scrollbars (displayed by the UI,
	 * not by core).
	 *
	 * @param[out] width Set to the width of the document's content
	 * @param[out] height Set to the height of the document's content
	 */
	virtual void GetDocumentSize(unsigned int* width, unsigned int* height) = 0;

	/** @short Get a list of rectangles representing interesting paragraphs within a rectangle.
	 *
	 * Gets a list of rectangles representing the interesting paragraphs in the
	 * rectangle supplied. Coordinates are relative to the top left corner of
	 * the document.
	 *
	 * @param rect The rectangle to find paragraphs in
	 * @param[out] paragraph_rects A list of paragraph areas located within
	 * rect. Each element in the list is an OpRectListItem.
	 */
	virtual void GetParagraphRects(const OpRect& rect, Head* paragraph_rects) = 0;

	/**
	 * @return TRUE if the visual viewport size is currently locked, FALSE otherwise.
	 *
	 * @see LockVisualViewportSize()
	 */
	virtual BOOL IsVisualViewportSizeLocked() const = 0;

#ifdef RESERVED_REGIONS
	/** @short Obtain the region defined by the union of all reserved rectangles.
	  *
	  * Given a rectangle, this method will obtain all rectangles within this area
	  * that may be considered reserved. The definition of a reserved rectangle is
	  * a region of the document where any input events should be sent to Core for
	  * processing before being acted upon by the platform.
	  *
	  * Both arguments use document coordinates.
	  *
	  * @param[in] rect The region to look for reserved rectangles in.
	  * @param[out] reserved_region Region defined by found reserved rectangles.
	  *
	  * @return OpStatus::OK on success or OpStatus::ERR_NO_MEMORY on memory error.
	  */
	virtual OP_STATUS GetReservedRegion(const OpRect& rect, OpRegion& reserved_region) = 0;
#endif // RESERVED_REGIONS
};

/** @short Reason for a visual viewport change request. */
enum OpViewportChangeReason
{
	VIEWPORT_CHANGE_REASON_NEW_PAGE,
	VIEWPORT_CHANGE_REASON_JUMP_TO_RELATIVE_POS,
	VIEWPORT_CHANGE_REASON_HISTORY_NAVIGATION,
	VIEWPORT_CHANGE_REASON_INPUT_ACTION,
	VIEWPORT_CHANGE_REASON_FIND_IN_PAGE,
	VIEWPORT_CHANGE_REASON_SPATIAL_NAVIGATION,
#ifdef CONTENT_MAGIC
	VIEWPORT_CHANGE_REASON_CONTENT_MAGIC,
#endif // CONTENT_MAGIC
	VIEWPORT_CHANGE_REASON_REFLOW,
	VIEWPORT_CHANGE_REASON_DOCUMENT_SIZE,
	VIEWPORT_CHANGE_REASON_SCRIPT_SCROLL,
	VIEWPORT_CHANGE_REASON_FORM_FOCUS,
	VIEWPORT_CHANGE_REASON_FRAME_FOCUS,
	VIEWPORT_CHANGE_REASON_DOCUMENTEDIT,
	VIEWPORT_CHANGE_REASON_SCOPE,
	VIEWPORT_CHANGE_REASON_HIGHLIGHT,
	/** The window size has changed. */
	VIEWPORT_CHANGE_REASON_WINDOW_SIZE,
	/** This reason is used on adjusting the zoom-level when the Device to CSS pixel ratio has changed.
	 * Opera has calculated a new zoom-level according to the constraints given by the meta viewport tag.
	 * @see OnZoomLevelChangeRequest()
	 * @see OpViewportController::SetTrueZoomBaseScale()
	 * @see OpViewportInfoListener::OnTrueZoomBaseScaleChanged()
	 */
	VIEWPORT_CHANGE_REASON_BASE_SCALE
#ifdef SELFTEST
	, VIEWPORT_CHANGE_REASON_SELFTEST
#endif // SELFTEST
};

enum { ZoomLevelNotSet = 0 };

/** @short Viewport request listener.
 *
 * Methods in this class are called by core to notify the user interface about
 * viewport-related change requests.
 *
 * It is up to the user interface to honor these requests. The user interface
 * is free to modify, delay, or even ignore the requests.
 *
 * This interface aims to provide the user interface with enough information to
 * operate in parallel with core, so that it is possible to implement the user
 * interface in a separate thread and/or use hardware acceleration for zooming
 * and scrolling/panning. This means that the implementation may allow the user
 * to pan and zoom the visual viewport around in the rendering viewport without
 * asking core to repaint directly.
 */
class OpViewportRequestListener
{
public:
	/** @short Horizontal edge specifier.
	 *
	 * Used together with a point to specify which viewport edge the point is
	 * with respect to, horizontally.
	 */
	enum HEdge
	{
		EDGE_LEFT,
		EDGE_RIGHT
	};

	/** @short Vertical edge specifier.
	 *
	 * Used together with a point to specify which viewport edge the point is
	 * with respect to, vertically.
	 */
	enum VEdge
	{
		EDGE_TOP,
		EDGE_BOTTOM
	};

	/** @short Viewport position specifier. */
	struct ViewportPosition
	{
		ViewportPosition(OpPoint point, HEdge hor_edge, VEdge vert_edge) :
			point(point),
			hor_edge(hor_edge),
			vert_edge(vert_edge) { }

		BOOL IsLeftEdge() const { return hor_edge == EDGE_LEFT; }
		BOOL IsRightEdge() const { return hor_edge == EDGE_RIGHT;  }
		BOOL IsTopEdge() const { return vert_edge == EDGE_TOP; }
		BOOL IsBottomEdge() const { return vert_edge == EDGE_BOTTOM; }

		OpPoint point;
		HEdge hor_edge;
		VEdge vert_edge;
	};

	/** @short Horizontal direction hint.
	 *
	 * Used to provide a hint about the general horizontal direction of the
	 * visual viewport position requests, so that the platform may adjust the
	 * rendering viewport position accordingly.
	 */
	enum HDirectionHint
	{
		DIR_H_UNKNOWN,
		DIR_H_LEFT,
		DIR_H_RIGHT,
	};

	/** @short Vertical direction hint.
	 *
	 * Used to provide a hint about the general vertical direction of the
	 * visual viewport position requests, so that the platform may adjust the
	 * rendering viewport position accordingly.
	 */
	enum VDirectionHint
	{
		DIR_V_UNKNOWN,
		DIR_V_DOWN,
		DIR_V_UP
	};

	/** @short Alignment specifier. */
	enum Align
	{
		VIEWPORT_ALIGN_NONE         = 0,
		VIEWPORT_ALIGN_LEFT         = 1,
		VIEWPORT_ALIGN_HCENTER      = 2,
		VIEWPORT_ALIGN_RIGHT        = 4,
		VIEWPORT_ALIGN_TOP          = 8,
		VIEWPORT_ALIGN_VCENTER      = 16,
		VIEWPORT_ALIGN_BOTTOM       = 32,
		VIEWPORT_ALIGN_JUSTIFY      = 64,
		VIEWPORT_ALIGN_TOPLEFT      = VIEWPORT_ALIGN_TOP | VIEWPORT_ALIGN_LEFT,
		VIEWPORT_ALIGN_BOTTOMLEFT   = VIEWPORT_ALIGN_BOTTOM | VIEWPORT_ALIGN_LEFT,
		VIEWPORT_ALIGN_CENTER       = VIEWPORT_ALIGN_VCENTER | VIEWPORT_ALIGN_HCENTER,
		VIEWPORT_ALIGN_TOPRIGHT     = VIEWPORT_ALIGN_TOP | VIEWPORT_ALIGN_RIGHT,
		VIEWPORT_ALIGN_BOTTOMRIGHT  = VIEWPORT_ALIGN_BOTTOM | VIEWPORT_ALIGN_RIGHT,
	};

	/** @short Element types. */
	enum ElementType
	{
		/** Element is an image. */
		ELEMENT_TYPE_IMAGE,
		/** Element is a paragraph. */
		ELEMENT_TYPE_PARAGRAPH,
		/** Element is a (non-paragraph) container element. A container element
		 * is an element capable of hosting child elements. */
		ELEMENT_TYPE_CONTAINER,
		/** Any other element type. */
		ELEMENT_TYPE_OTHER
	};

	/** @short Direction hint.
	 *
	 * Used to provide a hint about the general direction of the visual
	 * viewport position requests, so that the platform may adjust the
	 * rendering viewport position accordingly.
	 */
	struct DirectionHint
	{
		DirectionHint(HDirectionHint horizontal, VDirectionHint vertical) :
			horizontal(horizontal),
			vertical(vertical) { }

		HDirectionHint horizontal;
		VDirectionHint vertical;
	};

	virtual ~OpViewportRequestListener() { }

	/** @short Request that the visual viewport position and size be changed.
	 *
	 * Core calls this method when it wants to change the visual viewport. It
	 * is up to the platform whether or not to honor this request, and whether
	 * or not to make modifications to the requested viewport. In most cases it
	 * would make a lot of sense if the platform simply honor the request, by
	 * calling controller->SetVisualViewport().
	 *
	 * @param controller The viewport controller.
	 *
	 * @param viewport Requested new visual viewport.
	 *
	 * @param priority_rect Priority rectangle; the essential part of the
	 * requested new visual viewport. Always a subset of 'viewport'. If the
	 * platform decides that 'viewport' is "too large", and therefore would
	 * like to show only parts of it, this rectangle can be used to tell what
	 * part of the document that really should be within the visual
	 * viewport. The implementor should also make sure, if the visual viewport
	 * is changed, that the rendering viewport position is changed as well if
	 * necessary, so that it contains the visual viewport.
	 *
	 * @param reason Reason for requesting the visual viewport to be changed.
	 *
	 * @param dir_hint Direction hint. May be NULL. If non-NULL, this specifies
	 * what is likely to happen to the visual viewport next, if anything.
	 */
	virtual void OnVisualViewportChangeRequest(OpViewportController* controller, const OpRect& viewport, const OpRect& priority_rect, OpViewportChangeReason reason, DirectionHint* dir_hint = NULL) = 0;

	/** @short Request that the visual viewport position be changed.
	 *
	 * Core calls this method when it wants to change the visual viewport
	 * position. It is up to the platform whether or not to honor this request,
	 * and whether or not to make modifications to the requested position. In
	 * most cases it would make a lot of sense if the platform simply honor the
	 * request, by calling controller->SetVisualViewport() or
	 * controller->SetVisualViewportPos(). The implementor should also make
	 * sure, if the visual viewport is changed, that the rendering viewport
	 * position is changed as well if necessary, so that it contains the visual
	 * viewport.
	 *
	 * @param controller The viewport controller.
	 *
	 * @param pos Requested new visual viewport position.
	 *
	 * @param reason Reason for requesting the visual viewport to be changed.
	 *
	 * @param dir_hint Direction hint. May be NULL. If non-NULL, this specifies
	 * what is likely to happen to the visual viewport next, if anything.
	 */
	virtual void OnVisualViewportEdgeChangeRequest(OpViewportController* controller, const ViewportPosition& pos, OpViewportChangeReason reason, DirectionHint* dir_hint = NULL) = 0;

	/** @short Request that the zoom level be changed.
	 *
	 * Changing the zoom-level results in a change of the visual
	 * viewport. An implementation of this method usually calls
	 * OpViewportController::SetRenderingScale() and
	 * OpViewportController::SetVisualViewport() to adjust both the
	 * rendering scale and the visual viewport to the requested
	 * zoom-level and priority-rectangle. The implementation of this
	 * method should ensure that the requested priority rectangle is
	 * moved into the new visual viewport. A possible implementation
	 * is to center the specified rectangle in the new visual
	 * viewport.
	 *
	 * Example:
	 * \code
	 * void MyViewportRequestListener::OnZoomLevelChangeRequest(OpViewportController* controller, double zoom_level, const OpRect* priority_rect, OpViewportChangeReason reason)
	 * {
	 *     OpRect viewport = controller->GetVisualViewport();
	 *     viewport.width = (int)(my_window_width / zoom_level);
	 *     viewport.height = (int)(my_window_height / zoom_level);
	 *     if (priority_rect)
	 *     {
	 *         viewport.x = priority_rect->Center().x - viewport.width/2;
	 *         viewport.y = priority_rect->Center().y - viewport.height/2;
	 *         if (viewport.x < 0) viewport.x = 0;
	 *         if (viewport.y < 0) viewport.y = 0;
	 *     }
	 *     // keep the top-left position fixed if no priority_rect is
	 *     // specified...
	 *     controller->SetRenderingScale((int)(100.0*zoom_level));
	 *     controller->SetVisualViewport(viewport);
	 * }
	 * \endcode
	 *
	 * @note It is valid to specify a priority_rect which is larger
	 *  than the new visual viewport or which is outside the current
	 *  visual viewport.
	 *
	 * @param controller is the viewport controller.
	 *
	 * @param zoom_level is the requested zoom level. This is a
	 *  positive value. 1.0 means that one logical document pixel
	 *  should be displayed according to the device pixel ratio (the
	 *  scale set with OpViewportController::SetTrueZoomBaseScale()).
	 *
	 * @param priority_rect is a pointer to a rectangle that should be
	 *  visible after zooming to the requested zoom-level. If the
	 *  pointer is 0, the implementation of this method can decide
	 *  about the new visual viewport position, e.g. it may decide to
	 *  keep the top-left position or the center of the current
	 *  viewport or the current mouse-position fixed.
	 *
	 * @param reason is the reason for requesting the zoom-level to be
	 *  changed. Common reasons used:
	 * - VIEWPORT_CHANGE_REASON_WINDOW_SIZE: a document with meta
	 *   viewport has had the initial-scale recalculated due to a
	 *   change in window size (e.g. when the screen is rotated).
	 *   Normally, the platform would want to change scale to follow
	 *   the initial-scale in this case.
	 */
	virtual void OnZoomLevelChangeRequest(OpViewportController* controller, double zoom_level, const OpRect* priority_rect, OpViewportChangeReason reason) = 0;

#ifdef DOC_SEND_POIMOVED
	/** @short Data for OnPOIMoved
	 *
	 * Used to provide data for element under point of interest, when
	 * OnPOIMoved is called.
	 */
	struct POIData
	{
		/** Delta movement of element (new - old). */
		OpPoint movement;
		/** Rectangle representing the Container of the element, after movement. */
		OpRect container;
		/** Element type. */
		ElementType element_type;
		/** Alignment of text in paragraph. */
		Align text_align;
	};

	/** @short The element under a point of interest was moved.
	 *
	 * Core calls this method when an element under a specific point
	 * of interest was moved during a reflow, e.g. during or after
	 * call to SetTextParagraphWidthLimit().
	 *
	 * @param controller The viewport controller.
	 * @param element_data data for the element under the point of interest
	 */
	virtual void OnPOIMoved(OpViewportController* controller, const POIData& element_data) = 0;
#endif // DOC_SEND_POIMOVED
};

enum { HistoryIdNotSet = 0 };

/** @short Viewport info listener.
 *
 * Methods in this class are called by core to notify the user interface about
 * viewport-related changes.
 *
 * This interface aims to provide the user interface with enough information to
 * operate in parallel with core, so that it is possible to implement the user
 * interface in a separate thread and/or use hardware acceleration for zooming
 * and scrolling/panning. This means that the implementation may allow the user
 * to pan and zoom the visual viewport around in the rendering viewport without
 * asking core to repaint directly.
 */
class OpViewportInfoListener
{
public:
	virtual ~OpViewportInfoListener() { }

	/**
	 * @short The Browser has switched to a new page
	 *
	 * This listener method is called whenever the Browser has
	 * switched to a new page. A "new page" can mean one of three different
	 * things:
	 * - it can be a new document that is loaded;
	 * - it can be a document in history that has been activated;
	 * - it can be a different history position on the same document,
	 *   e.g. when the user clicked on an anchor link.
	 *
	 * Note that it might not be possible to render the new document
	 * yet. OnNewPageReady() will signal when that happens.
	 *
	 * On loading a new document, the reason should be set to
	 * VIEWPORT_CHANGE_REASON_NEW_PAGE. It is guaranteed that this is
	 * the first method called for a new document. This means that all
	 * information received about the current document (e.g., the
	 * layout viewport size, the document size, the zoom level limits,
	 * the content magic rectangle, the reserved reason rectangles and
	 * the area of interest rectangles) up to this event belong to the
	 * old document, while all information received after belong to
	 * the new document. The implementation of this class can expect
	 * to receive new values as soon as they are available,
	 * i.e. OnLayoutViewportSizeChanged(), OnDocumentSizeChanged(),
	 * etc. and the associated
	 * OpViewportRequestListener::OnVisualViewportEdgeChangeRequest()
	 * or OpViewportRequestListener::OnVisualViewportChangeRequest()
	 * and OpViewportRequestListener::OnZoomLevelChangeRequest() are
	 * called.
	 *
	 * On jumping to a different location on the same document, the
	 * reason should be set to
	 * VIEWPORT_CHANGE_REASON_JUMP_TO_RELATIVE_POS. This indicates,
	 * that
	 * - the user e.g. clicked on an anchor link;
	 * - or the user navigated in history between two anchor links on
	 *   the same document;
	 * Only the visual viewport of the document changes, other
	 * information about the document (like layout viewport size,
	 * document size or zoom level limits) remain valid. A call with
	 * this reason will be followed by a call to
	 * OpViewportRequestListener::OnVisualViewportEdgeChangeRequest()
	 * or OpViewportRequestListener::OnVisualViewportChangeRequest()
	 * to adjust the visual viewport to the new position.
	 *
	 * An ID will be included to identify the history position for the
	 * new document. Usually each new document that generates a new
	 * history position will get a unique (within this core window)
	 * ID. The exception is when several history positions share the
	 * same top document (for example in the case of iframes, but note
	 * that OnNewPage() will not be called in this case).  This ID is
	 * typically used together with
	 * OpWindowCommander::SetHistoryUserData() or
	 * OpWindowCommander::GetHistoryUserData().
	 *
	 * @param controller is the associated OpViewportController.
	 * @param reason is the reason for this call; there are currently
	 *  only two valid reasons:
	 *  - VIEWPORT_CHANGE_REASON_NEW_PAGE if a new document is loaded
	 *  - VIEWPORT_CHANGE_REASON_JUMP_TO_RELATIVE_POS if the document
	 *    remains the same, but the user navigates between anchor
	 *    links on the same document.
	 * @param id ID (within this core window) for the history
	 * position that the new page will have, or HistoryIdNotSet if no
	 * ID could be supplied for this history position for some reason
	 * (e.g. OOM).
	 */
	virtual void OnNewPage(OpViewportController* controller, OpViewportChangeReason reason, int id) = 0;

	/** @short A new page is ready for rendering.
	 *
	 * A new page, previously signaled with OnNewPage(), is
	 * now ready to be rendered.
	 *
	 * @param controller is the associated OpViewportController.
	 */
	virtual void OnNewPageReady(OpViewportController* controller) = 0;

	/** @short Layout viewport size changed.
	 *
	 * This information may for instance be used to adjust the zoom level
	 * during loading.
	 */
	virtual void OnLayoutViewportSizeChanged(OpViewportController* controller, unsigned int width, unsigned int height) = 0;

	/** @short Document size changed.
	 *
	 * This information may for instance be used to adjust the scrollbars
	 * (displayed by the UI, not by core).
	 */
	virtual void OnDocumentSizeChanged(OpViewportController* controller, unsigned int width, unsigned int height) = 0;

	/** @short Reason codes for OnDocumentContentChanged.
	 *
	 * Data used together with GOGI_OPERA_EVT_DOCUMENT_CONTENT_CHANGED.
	 */
	enum CONTENT_CHANGED_REASON
	{
#ifdef RESERVED_REGIONS
		/**
		 * There has been a change to the reserved regions, either new ones have
		 * been added, or removed.
		 */
		REASON_RESERVED_REGIONS_CHANGED,
#endif // RESERVED_REGIONS
		/**
		 * Document size has changed.
		 */
		REASON_DOCUMENT_SIZE_CHANGED,
		/**
		 * A reflow has happened.
		 */
		REASON_REFLOW
	};

	/** @short The contents of the document has changed.
	 *
	 * This happens e.g. when the document layout is reflowed. This means that
	 * elements of interest may have moved, been added or removed, or otherwise
	 * incurred changes.
	 *
	 * This function MUST NOT lead to a synchronous call back into Core, and
	 * should make every effort to limit the time spent handling the message.
	 */
	virtual void OnDocumentContentChanged(OpViewportController* controller, CONTENT_CHANGED_REASON reason) = 0;

	/** @short Proposed zoom level limits changed.
	 *
	 * @param controller The viewport controller.
	 *
	 * @param min_zoom_level Minimum zoom level. This is a positive
	 * value, or ZoomLevelNotSet if there is no minimum zoom
	 * level. 1.0 means that one logical document pixel should be
	 * displayed according to the device pixel ratio (the scale set
	 * with OpViewportController::SetTrueZoomBaseScale()).
	 *
	 * @param max_zoom_level Maximum zoom level. This is a positive
	 * value, or ZoomLevelNotSet if there is no maximum zoom
	 * level. 1.0 means that one logical document pixel should be
	 * displayed according to the device pixel ratio (the scale set
	 * with OpViewportController::SetTrueZoomBaseScale()).
	 *
	 * @param user_zoomable TRUE if the zoom level can be changed by
	 * user interaction, FALSE if not. If not changable, this
	 * typically means to respond only to
	 * OnZoomLevelChangeRequest(OpViewportChangeReason::VIEWPORT_CHANGE_REASON_NEW_PAGE).
	 */
	virtual void OnZoomLevelLimitsChanged(OpViewportController* controller, double min_zoom_level, double max_zoom_level, BOOL user_zoomable) = 0;

#ifdef CONTENT_MAGIC
	/** @short Content magic found.
	 *
	 * This information may for instance be used to mark the position on the
	 * scrollbars, or even just scroll automatically to the given position.
	 */
	virtual void OnContentMagicFound(OpViewportController* controller, const OpRect& content_magic_rect) = 0;
#endif // CONTENT_MAGIC

	/** @short Reserved region changed.
	 *
	 * Pointing device events on some parts of the document should not be
	 * filtered on the UI side.
	 */
	virtual void OnReservedRegionChanged(OpViewportController* controller, const OpRect* rect_array, unsigned int num_rects) = 0;

#ifdef ADAPTIVE_ZOOM_SUPPORT
	/** @short Area of interest changed. */
	virtual void OnAreaOfInterestChanged(OpViewportController* controller, const OpRect& primary_rect, const OpRect& secondary_rect) = 0;
#endif // ADAPTIVE_ZOOM_SUPPORT

	/** @short Notification whether TrueZoom is overriden or not.
	 *
	 * It is possible for author content to change the device/css pixel
	 * ratio through the viewport meta property target-densityDpi.
	 * When that happens, the TrueZoom base scale will change and the
	 * TrueZoom base scale set by the platform through
	 * OpViewportController::SetTrueZoomBaseScale is overridden.
	 *
	 * This method will tell the platform that the base scale is
	 * overridden, or when it's not overriden, and it will also give
	 * the current TrueZoom base scale that is used. See the
	 * OpViewportController::SetTrueZoomBaseScale documentation for
	 * further description of the base scale.
	 *
	 * @param scale_percentage Base scale for physical pixels vs CSS
	 *		  pixels as a percentage value.
	 *
	 */
	virtual void OnTrueZoomBaseScaleChanged(OpViewportController* controller, unsigned int scale_percentage) = 0;

#ifdef PAGED_MEDIA_SUPPORT
	/** @short Current page situation (current page, total number of pages) changed.
	 *
	 * This method will be called when entering paged documents, and when they
	 * are reflowed in a way that page count / number changes, or when scripts
	 * or users change the page number. It is also called when leaving paged
	 * documents.
	 *
	 * @param new_page_number The new page number (0 is the first page). Will
	 * be -1 if we're (no longer) in paged mode.
	 * @param new_page_count New total number of pages. Will be -1 if we're (no
	 * longer) in paged mode.
	 */
	virtual void OnPageChanged(int new_page_number, int new_page_count) = 0;
#endif // PAGED_MEDIA_SUPPORT
};

/** @short A linked list of OpRect objects. */
class OpRectListItem : public Link
{
public:
	OpRectListItem(const OpRect &r) : rect(r) {}
	OpRect rect;

	OpRectListItem* Suc() const { return (OpRectListItem*)Link::Suc(); }
	OpRectListItem* Pred() const { return (OpRectListItem*)Link::Pred(); }
};

#endif // OPVIEWPORTCONTROLLER_H
