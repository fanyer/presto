/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef COREVIEW_H
#define COREVIEW_H

#include "modules/inputmanager/inputcontext.h"
#include "modules/util/simset.h"
#include "modules/pi/OpView.h"
#include "modules/display/coreview/coreviewlisteners.h"
#include "modules/display/vis_dev_transform.h"
#include "modules/display/bg_clipping.h"

#ifdef SELFTEST
class CoreView;
class ST_CoreViewHacks // Use this class only inside selftests!
{
public:
	static void FakeCoreViewSize(CoreView *view, int w, int h);
};
#endif // SELFTEST

class OpWindow;
class HTML_Element;
class ClipViewEntry;

/** PluginAreaIntersection is a temporary object that is only used while painting.

	It holds region for a plugin intersection. Layout add this when traversing a plugin, and then overlapping
	elements (with higher Z) will cut out it's area from the plugin area. After traverse, CoreViewContainer
	will update the plugins visibility region with this region.
*/
class PluginAreaIntersection : public Link
{
public:
	ClipViewEntry *entry;		///< The entry for the plugin we're intersecting.
	BgRegion visible_region;	///< Region we cut out overlapping rectangles from.
	OpRect rect;				///< The display rect for the plugin.
};

/**
	A view similar to OpView but it will not contain any systemwidget.
	By default, it is automatically painted in the creation order, but
	that can be overridden so it can be painted on demand. (Used to paint
	iframes in the correct z-index order).
	Most functions is shortcuts to OpView which only transforms the CoreView position in the OpView.
*/

class CoreView : public OpInputContext, public Link
{
#ifdef SELFTEST
	friend class ST_CoreViewHacks;
#endif // SELFTEST

	friend class CoreViewContainer;
public:
	CoreView();
	virtual ~CoreView();

	static OP_STATUS Create(CoreView** view, CoreView* parent);

	/** Will initialize this view under parent. */
	OP_STATUS Construct(CoreView* parent);

	/** Will deinitialize this view (remove it from its parent). */
	void Destruct();

	/** Add this view as child under parent. Affects only the CoreView hierarchy and not eventual containers. */
	void IntoHierarchy(CoreView *parent);

	/** Remove this view from its parent. Affects only the CoreView hierarchy and not eventual containers. */
	void OutFromHierarchy();

	/** Sets the visibility of the view */
	virtual void SetVisibility(BOOL visible);

	/** Returns the visibility of the view */
	BOOL GetVisibility();

	/** Get the visible rect. The returned rect is relative to this view's container.
	 *	It intersects the rect with all parents up to container, not caring about the
	 *	visibility of the container itself or its window.
	 *	If it is not visible, the rect will be empty.
	 * @param skip_intersection_with_container if TRUE, then the result will not be intersected with
	 *										   the cointainer's rect (it will still be in the container's coordinate system). */
	OpRect GetVisibleRect(BOOL skip_intersection_with_container = FALSE);

	/**
	 * Invalidates the area in rect, causing it to be scheduled for redrawing
	 * (if all the ancestor CoreViews up to the CoreViewContainer inclusive
	 * are already visible).
	 * @param rect the area to invalidate.
	*/
	virtual void Invalidate(const OpRect& rect);

	virtual OpPainter* GetPainter(const OpRect &rect, BOOL nobackbuffer = FALSE);
	virtual void ReleasePainter(const OpRect &rect);

	/** Returns the containers OpView. */
	virtual OpView* GetOpView();

	/** Returns the container. */
	CoreViewContainer* GetContainer();

	/** Should be moved when scrolling or not. */
	BOOL GetFixedPosition()					{ return !!m_fixed_position_subtree; }
	/**
	 * Gets the first fixed positioned ancestor of the element owning this
	 * (including the owning element itself). NULL if not inside any fixed
	 * positioned element.
	 */
	HTML_Element* GetFixedPositionSubtree() const
											{ return m_fixed_position_subtree; }
	/**
	 * Updates the the first fixed positioned ancestor of the element owning this.
	 * Typically called during the reflows. However in case of ClipViews of
	 * plugins, it is also called during painting, because during the first reflow
	 * the plugin window may not exist yet.
	 */
	void SetFixedPositionSubtree(HTML_Element* fixed)
											{ m_fixed_position_subtree = fixed; }

	/** Set if this CoreView belongs to a layoutbox. Sometimes we have to separate those views from regular CoreView and CoreViewContainers since
		we might want to translate to nearest parent CoreView that is not a layoutbox and which might not be a CoreViewContainer. */
	void SetIsLayoutBox(BOOL status)		{ packed5.is_layoutbox = status; }
	BOOL GetIsLayoutBox()					{ return packed5.is_layoutbox; }

	/** Is the view overlapped so fast scroll cannot be used. We can also skip doing expensive
		intersection checks when finding mouse target if we know that the view is not overlapped. */
	BOOL GetIsOverlapped()
	{
		return packed5.overlapped || packed5.is_clipped
#ifdef CSS_TRANSFORMS
			|| m_pos.IsTransform()
#endif // CSS_TRANSFORMS
			;
	}
	void SetIsOverlapped(BOOL status)		{ packed5.overlapped = status; }

	/** Set if this view is clipped (some parts of it isn't painted so intersection handling might need to verify a hit. */
	void SetIsClipped(BOOL status)			{ packed5.is_clipped = status; }

	/** If the view should be painted automatically in creation order. */
	BOOL GetWantPaintEvents()				{ return packed5.want_paint_events; }
	void SetWantPaintEvents(BOOL status)	{ packed5.want_paint_events = status; }

	/** If the view and childviews should get mouse input. */
	BOOL GetWantMouseEvents()				{ return packed5.want_mouse_events; }
	void SetWantMouseEvents(BOOL status)	{ packed5.want_mouse_events = status; }

	/** Set a reference element for special transformation of input and invalidation. */
	void SetReference(FramesDocument* doc, HTML_Element* elm) { m_reference_doc = doc; m_reference_elm = elm; }
	BOOL IsRedirected() const				{ return m_reference_doc != NULL; }

	/** Make x and y (relative to this view) relative to the upper left corner of the CoreViewContainer. */
	void ConvertToContainer(int &x, int &y);

	/** Make x and y (relative to the upper left corner of the CoreViewContainer) relative to this view. */
	void ConvertFromContainer(int &x, int &y);

	/** Paint the rect part of the view with painter if visible. This is only needed if the automatical paint
		has been disabled with SetWantPaintEvents.
		translation will be added to the m_rect during painting (Affecting ConvertToContainer, painting etc.)
		paint_containers should normally be FALSE since each container will get its own paint from the system (from its OpView).
		clip_to_view should normally be TRUE since that is visually correct and faster. This parameter is not recursively sent to children.
	*/
	void Paint(const OpRect& rect, OpPainter* painter, int translation_x = 0, int translation_y = 0, BOOL paint_containers = FALSE, BOOL clip_to_view = TRUE);

	/** Will call BeforePaint on the paintlistener on this view and children. */
	BOOL CallBeforePaint();

	/** Call the listeners UpdateOverlappedStatus on childviews so we know it's updated. */
	void UpdateOverlappedStatusRecursive();

#ifndef MOUSELESS
	/** Returns the visible childview that wants mouse input and intersects x,y or NULL if no child does. Does NOT recurse to childrens children */
	CoreView *GetIntersectingChild(int x, int y);

	/** Returns the visible childview that wants mouse input and intersects x,y or this if no one did.
		Checks with with the listeners GetMouseHitView to get eventual z-index/overlapping views right.
		Recurse to childrens children. */
	CoreView *GetMouseHitView(int x, int y);

	/** Get the hit status of the given position in this view. A view should be hit if the coordinate is
	 * contained inside this view and not overlapped by the custom overlap region (SetCustomOverlapRegion).
	 *
	 * @param x The x coordinate relative to this OpView.
	 * @param y The y coordinate relative to this OpView.
	 * @return TRUE if the coordinate is inside and should hit this view.
	 */
	virtual BOOL GetHitStatus(INT32 x, INT32 y);

	/** Returns whether a mouse button is currently down.
	*/
	virtual BOOL GetMouseButton(MouseButton button);

#ifdef TOUCH_EVENTS_SUPPORT
	/** Sets a mouse button to be currently down or not. Needed for translating
	 * touch events to mouse events
	*/
	virtual void SetMouseButton(MouseButton button, bool set);
#endif // TOUCH_EVENTS_SUPPORT

	virtual void GetMousePos(INT32* xpos, INT32* ypos);

	/** Reset the current captured view so a new view can be captured from the current mouseposition. */
	void ReleaseMouseCapture();

	void MouseMove(const OpPoint &point, ShiftKeyState keystate);
	void MouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks, ShiftKeyState keystate);
	void MouseUp(const OpPoint &point, MouseButton button, ShiftKeyState keystate);
	void MouseLeave();
	void MouseSetCursor();
	BOOL MouseWheel(INT32 delta, BOOL vertical, ShiftKeyState keystate);
#endif // !MOUSELESS

#ifdef TOUCH_EVENTS_SUPPORT
	/** Returns the visible childview that wants mouse input and intersects x,y or this if no one did.
		Checks with with the listeners GetTouchHitView to get eventual z-index/overlapping views right.
		Recurse to childrens children. */
	CoreView *GetTouchHitView(int x, int y);

	/** Release this view from any touch that may have captured it. */
	void ReleaseTouchCapture(BOOL include_ancestor_containers = FALSE);

	void TouchDown(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, void *user_data);
	void TouchUp(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, void *user_data);
	void TouchMove(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, void *user_data);
#endif // TOUCH_EVENTS_SUPPORT

	void SetPaintListener(CoreViewPaintListener* listener) { m_paint_listener = listener; }
	void SetMouseListener(CoreViewMouseListener* listener) { m_mouse_listener = listener; }
#ifdef TOUCH_EVENTS_SUPPORT
	void SetTouchListener(CoreViewTouchListener* listener) { m_touch_listener = listener; }
#endif // TOUCH_EVENTS_SUPPORT
#ifdef DRAG_SUPPORT
	void SetDragListener(CoreViewDragListener* listener) { m_drag_listener = listener; }
#endif
	/** Add a CoreViewScrollListener. Must be removed before this view is deleted. */
	void AddScrollListener(CoreViewScrollListener *listener) { listener->Out(); listener->Into(&m_scroll_listeners); }
	/** Remove a CoreViewScrollListener. */
	void RemoveScrollListener(CoreViewScrollListener *listener) { listener->Out(); }

	/** Called when this view or a parent has been moved. It will call OnMoved on
		children, so if overridden, call the defaultimplementation. */
	virtual void OnMoved();

	/** Called on a view after SetVisibility is called and the value is changed. Will recurse into children.
		If overridden, it should call baseimplementation to keep recursing into children. */
	virtual void OnVisibilityChanged(BOOL visible);

	/** Called when this view or a parent has been resized. It will call OnResized on
		children, so if overridden, call the defaultimplementation. */
	virtual void OnResized();

	/** Sets the position of the view */
	virtual void SetPos(const AffinePos& pos, BOOL onmove = TRUE, BOOL invalidate = TRUE);

	/** Returns the pos of the view */
	virtual void GetPos(AffinePos* pos);

	/** Sets the size of the view (including scrollbars) */
	virtual void SetSize(int w, int h);

	/** Returns the size of the view (including scrollbars) */
	virtual void GetSize(int* w, int* h);

	/** Gets this view's rectangle in screen coordinates.
	 *	The rectangle is limited only to the part that is visible through top view (may be empty). */
	virtual OpRect GetScreenRect();

	/** Get the extents of this CoreView (relative to its parent) */
	OpRect GetExtents() const;
	CoreView* GetParent() { return m_parent; }
	CoreView* GetFirstChild() { return (CoreView*)m_children.First(); }
	BOOL IsContainer() { return packed5.is_container; }

	void MoveChildren(INT32 dx, INT32 dy, BOOL onmove);
	virtual void ScrollRect(const OpRect &rect, INT32 dx, INT32 dy);
	virtual void Scroll(INT32 dx, INT32 dy);
	virtual void Sync();
	virtual void LockUpdate(BOOL lock);

	// == Shortcuts to OpView to make life easier ===========
	virtual OpPoint ConvertToScreen(const OpPoint &point);
	virtual ShiftKeyState GetShiftKeys();

	/** Converts given screen rect to this view's coordinates. */
	OpRect ConvertFromScreen(const OpRect &rect);

	/** Set the HTML_Element that owns this CoreView. Note: The "ownership" is a indirect connection and doesn't mean the HTML_Element can delete the CoreView! */
	void SetOwningHtmlElement(HTML_Element* el) { m_owning_element = el; }

	/** Return NULL or the HTML_Element that (indirectly) owns this CoreView. */
	HTML_Element* GetOwningHtmlElement() { return m_owning_element; }

	void GetTransformToContainer(AffinePos& tot);
	void GetTransformFromContainer(AffinePos& tot);

	/** Notify scroll listeners on this view and its children. */
	void NotifyScrollListeners(INT32 dx, INT32 dy, CoreViewScrollListener::SCROLL_REASON reason, BOOL parent);

protected:
	friend void DebugCoreView(CoreView* view, int level);
	// Data
	CoreView* m_parent;
	AffinePos m_pos;
	int m_width, m_height;
	Head m_children;
	FramesDocument* m_reference_doc;	///< Reference used for special transformation
	HTML_Element* m_reference_elm;		///< Reference used for special transformation

	/**
	 * Invalidates the area in rect, causing it to be scheduled for redrawing.
	 * @param rect the area to invalidate.
	*/
	virtual void InvalidateInternal(OpRect& rect);

	void ConvertToParent(OpRect& r);
	void ConvertToParent(int& x, int& y);
	void ConvertFromParent(OpRect& r);
	void ConvertFromParent(int& x, int& y);

#ifdef SVG_SUPPORT_FOREIGNOBJECT
	OpPoint Redirect(const OpPoint& p);
	BOOL IsInRedirectChain() const;
	OpPoint ApplyRedirection(const OpPoint& p);
#endif // SVG_SUPPORT_FOREIGNOBJECT

	OpPoint ToLocal(const OpPoint& p);

	CoreViewPaintListener* m_paint_listener;
	CoreViewMouseListener* m_mouse_listener;
#ifdef TOUCH_EVENTS_SUPPORT
	CoreViewTouchListener* m_touch_listener;
#endif // TOUCH_EVENTS_SUPPORT
#ifdef DRAG_SUPPORT
	CoreViewDragListener* m_drag_listener;
#endif
	Head m_scroll_listeners; ///< List of CoreViewScrollListener

	HTML_Element* m_owning_element; ///< The element that holds this coreview. Iframe or plugin/applet element for example.

	/** The element that is the closest fixed positioned ancestor of
	 * m_owning_element inclusive. NULL, if not inside any fixed positioned subtree.
	 */
	HTML_Element* m_fixed_position_subtree;

	union {
		struct {
			unsigned int	visible : 1;
			unsigned int	want_paint_events : 1;
			unsigned int	want_mouse_events : 1;
			unsigned int	overlapped : 1;
			unsigned int	is_container : 1;
			unsigned int	is_layoutbox : 1;
			unsigned int	is_clipped : 1;
		} packed5;
		unsigned long packed5_init;
	};
};

/**
	A CoreViewContainer is a CoreView you should create at the top (Under the OpWindow). It will
	create a OpView and take care of painting and input and redirect to child-CoreView's.
*/

class CoreViewContainer : public CoreView, public OpPaintListener
#ifndef MOUSELESS
,public OpMouseListener
#endif // !MOUSELESS
#ifdef TOUCH_EVENTS_SUPPORT
,public OpTouchListener
#endif // TOUCH_EVENTS_SUPPORT
#ifdef DRAG_SUPPORT
,public OpDragListener
#endif // DRAG_SUPPORT
{
	friend class CoreView;
public:
	CoreViewContainer();
	virtual ~CoreViewContainer();

	static OP_STATUS Create(CoreView** view, OpWindow* parentwindow, OpView* parentopview, CoreView* parentview);
	OP_STATUS Construct(OpWindow* parentwindow, OpView* parentopview, CoreView* parentview);

	virtual OpPainter* GetPainter(const OpRect &rect, BOOL nobackbuffer = FALSE);
	virtual void ReleasePainter(const OpRect &rect);

	virtual OpView* GetOpView() { return m_opview; }
#ifndef MOUSELESS
	CoreView*		GetHoverView() { return m_hover_view; }
	CoreView*		GetCapturedView() { return m_captured_view; }
#endif

	void ConvertToParentContainer(int &x, int &y);

	OpPoint ConvertFromScreen(const OpPoint &point) { return m_opview->ConvertFromScreen(point);}

	/** Gets this view's rectangle in screen coordinates.
	 *	The rectangle is limited only to the part that is visible through top view (may be empty). */
	virtual OpRect GetScreenRect();

	virtual void Invalidate(const OpRect& rect) {m_opview->Invalidate(rect);}

	/** Returns TRUE if this coreview is currently being painted. */
	BOOL			IsPainting() { return m_painter ? TRUE : FALSE; }

	/** Will prevent Sync of any CoreView belonging to this container. */
	void			BeginPaintLock();
	void			EndPaintLock();

	// == OpPaintListener ======================================
	BOOL			BeforePaint();
	void 			OnPaint(const OpRect &rect, OpView* view);

	// == OpMouseListener ======================================
#ifndef MOUSELESS
	virtual void OnMouseMove(const OpPoint &point, ShiftKeyState keystate, OpView* view);
	virtual void OnMouseDown(MouseButton button, UINT8 nclicks, ShiftKeyState keystate, OpView* view);
	virtual void OnMouseUp(MouseButton button, ShiftKeyState keystate, OpView* view);
	virtual void OnMouseLeave();
	virtual BOOL OnMouseWheel(INT32 delta,BOOL vertical, ShiftKeyState keystate);
	virtual void OnSetCursor();
	virtual void OnMouseCaptureRelease();
#endif // !MOUSELESS

	// == OpTouchListener ======================================
#ifdef TOUCH_EVENTS_SUPPORT
	virtual void OnTouchDown(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, OpView *view, void *user_data);
	virtual void OnTouchUp(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, OpView *view, void *user_data);
	virtual void OnTouchMove(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, OpView *view, void *user_data);
#endif // TOUCH_EVENTS_SUPPORT

	// == OpDragListener ======================================
#ifdef DRAG_SUPPORT
	virtual void OnDragStart(OpView* view, const OpPoint& start_point, ShiftKeyState modifiers, const OpPoint& current_point);
	virtual void OnDragEnter(OpView* view, const OpPoint& point, ShiftKeyState modifiers);
	virtual void OnDragCancel(OpView* view, ShiftKeyState modifiers);
	virtual void OnDragLeave(OpView* view, ShiftKeyState modifiers);
	virtual void OnDragMove(OpView* view, const OpPoint& point, ShiftKeyState modifiers);
	virtual void OnDragDrop(OpView* view, const OpPoint& point, ShiftKeyState modifiers);
	virtual void OnDragEnd(OpView* view, const OpPoint& point, ShiftKeyState modifiers);
#endif // DRAG_SUPPORT

	// == Override some of CoreViews functions, to update the OpView properly. =============

	virtual void OnMoved();
	virtual void OnVisibilityChanged(BOOL visible);

	virtual void SetPos(const AffinePos& pos, BOOL onmove = TRUE, BOOL invalidate = TRUE);
	virtual void SetSize(int w, int h);

	virtual void ScrollRect(const OpRect &rect, INT32 dx, INT32 dy);
	virtual void Scroll(INT32 dx, INT32 dy);
	virtual void Sync();
	virtual void LockUpdate(BOOL lock);

#ifdef _PLUGIN_SUPPORT_
	/** Add plugin area. The area will be tracked so any overlapping background drawing (or call to CoverPluginArea) will remove
		that part from the plugins visibility region (make that part of the plugin invisible).
		@param rect The rect of the plugin (in document coordinates)
		@param plugin_window The pluginwindow positioned at the given area */
	OP_STATUS		AddPluginArea(const OpRect& rect, ClipViewEntry* entry);

	/** Remove a plug-in area by clip entry. Called when the associated plug-in window is detached.
	    @param entry A clip view previously passed as an argument to AddPluginArea(). */
	void            RemovePluginArea(ClipViewEntry* entry);

	/** Cover any plugin area that intersects with rect.
		@param The rect of the area to cover (in document coordinates) */
	void			CoverPluginArea(const OpRect& rect);

	/** Return TRUE if there is 1 or more plugin areas currently added */
	BOOL HasPluginArea();
#endif // _PLUGIN_SUPPORT_

protected:
	OpView* m_opview;
	OpPainter* m_painter;
	int m_getpainter_count;
	BOOL m_before_painting;
	int m_paint_lock;
#ifndef MOUSELESS
	CoreView* m_drag_view;
	CoreView* m_hover_view;
	CoreView* m_captured_view;
	MouseButton m_captured_button;
	OpPoint m_mouse_move_point;		///< Point sent to last OnMouseMoved call.
	OpPoint m_mouse_down_point;		///< Point sent to last OnMouseDown call.
	double m_last_click_time;		///< Time in last OnMouseDown call.
	MouseButton m_last_click_button;///< Button of the last click
	int m_emulated_n_click;			///< Which sequence number the last click had.
#endif
#ifdef TOUCH_EVENTS_SUPPORT
	CoreView* m_touch_captured_view[LIBGOGI_MULTI_TOUCH_LIMIT];
#endif // TOUCH_EVENTS_SUPPORT
	Head			plugin_intersections;///< List of PluginAreaIntersection

	virtual void	InvalidateInternal(OpRect& rect) {m_opview->Invalidate(rect);}

	/** Update the calculated visible region for the given plugin area. */
	void			UpdatePluginArea(const OpRect &rect, PluginAreaIntersection* info);

	/** Go through all plugin areas, update the calculated visible region and delete them. */
	void			UpdateAndDeleteAllPluginAreas(const OpRect &rect);

	/** Remove all plugin areas */
	void			RemoveAllPluginAreas();
};

#endif // COREVIEW_H
