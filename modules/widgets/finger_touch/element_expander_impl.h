/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ELEMENT_EXPANDER_IMPL_H
#define ELEMENT_EXPANDER_IMPL_H

#include "modules/widgets/finger_touch/element_expander.h"
#include "modules/widgets/OpWidget.h"

class FramesDocument;
class HTML_Element;
class OpWindow;
class ElementExpanderContainer;
class ElementOfInterest;

/** Represents the state of the expander/fingertouch system.
 *  Use GetState to query it */
enum ElementExpanderState
{
	EE_EXPANDING,				//< Currently expanding links, do not try to interface with the links
	EE_EXPANDED,				//< Expanded, waiting for user to select a link to activate
	EE_HIDING_SLASH_ACTIVATING,	//< Hiding / activating element
	EE_HIDDEN,					//< All elements hidden
	EE_EXPANDING_TO_EDIT_MODE,	//< Expanding to edit mode (for form elements)
	EE_EDITMODE,				//< Currently in edit mode (user can interract with the expanded widget)
	EE_LAST_STATE_DO_NOT_USE	//< DO NOT USE
};

/**
 * Implements the ElementExpander interface, provides functionality used by ElementOfInterest
 * classes. Not to be exposed outside of the widget module.
 */
class ElementExpanderImpl :
	public ElementExpander,
	public OpWidgetListener,
	public MessageObject,
	public OpTimerListener
{
public:
	ElementExpanderImpl(FramesDocument* document, OpPoint origin, unsigned int radius);
	virtual ~ElementExpanderImpl();

	// Implements ElementExpander:
	OP_BOOLEAN Show();
	void Hide(BOOL animate, BOOL can_ignore = FALSE);
	void Scroll(int dx, int dy);

	/** An HTML element has been removed from the document. */
	void OnElementRemoved(HTML_Element* html_element);

	/** Get the first element of interest owned by this expande */
	ElementOfInterest* First() const { return (ElementOfInterest*) Head::First(); }

	/** Get the last element of interest owned by this expander */
	ElementOfInterest* Last() const { return (ElementOfInterest*) Head::Last(); }

	FramesDocument* GetDocument() const { return document; }
	const OpPoint& GetOrigin() const { return origin; }
	float GetScaleFactor() const { return scale_factor; }
	int GetElementMinimumSize() const { return element_minimium_size; }

	ElementExpanderState GetState() const { return state; }

	/** Activate an element (e.g. follow link). Leave expanded elements mode.
	 *
	 * For elements supporting edit mode (see ElementOfInterest#HasEditMode()),
	 * this will enter edit mode for the widget.
	 * @param element The element to activate
	 */
	void Activate(ElementOfInterest* element);

	/** Hide all except one element.
	 *
	 * Will NOT do anything with the element passed as activated_element.
	 *
	 * @param activated_element The element not to hide.
	 */
	void HideAllExceptOne(ElementOfInterest* activated_element);

	/** For link expansion timeout */
	void ExtendInactivityTimer() { ExtendTimeOut(); }
	void StopInactivityTimer()   { timer.Stop(); }

	/** Get the available rectangle for the browser window under the overlay */
	void GetAvailableRect(OpRect& avail) const;

	/** Animation of an element ended.
	 *
	 * Each elements that were requested to perform an animation will call this
	 * method when the animation is done.
	 */
	void OnAnimationEnded(ElementOfInterest* element);

	void OnAllAnimationsEnded();

	/** Schedule an element for animation.
	 *
	 * The element itself knows the type of animation.
	 */
	void ScheduleAnimation(ElementOfInterest* eoi);

	/** Get the active element, if any.
	 *
	 * This is the form element that was selected for interaction (aka. "second
	 * expanded mode").
	 */
	ElementOfInterest* GetActiveElement() const;

	/** Get the number of elements scheduled for animation. */
	unsigned int GetScheduledAnimationsCount();

	/** Start all scheduled animations. */
	void StartAnimation();

	enum Edge
	{
		EDGE_NORTH,
		EDGE_SOUTH,
		EDGE_EAST,
		EDGE_WEST
	};

	/** Position a inside b, prioritizing putting a inside of the prioritized edges.
	 *
	 * Rules:
	 * - If a fits inside of b in one direction, move it as little as possible
	 *   until it is positioned inside of b
	 * - If a does not fit inside b in one direction (horizontally or vertically),
	 *   move it as close as possible to fitting inside b in that direction
	 *   without content parts of a that previously was inside of b getting pushed out.
	 * @param pri_horiz_edge Prioritize either EDGE_NORTH or EDGE_SOUTH
	 * @param pri_vert_edge  Prioritize either EDGE_EAST or EDGE_WEST
	 */
	static void SmartPositionRectInRect(OpRect& a, const OpRect& b, Edge pri_horiz_edge, Edge pri_vert_edge);

private:
	// Implements OpWidgetListener:
	virtual void OnClick(OpWidget *widget, UINT32 id);
	virtual void OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);

	void TriggerCallback(OpMessage msg);

	// Implements MessageObject:
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/** Extend the timer by OVERLAY_HIDE_DELAY milliseconds */
	void ExtendTimeOut();

	// Implements OpTimerListener:
	virtual void OnTimeOut(OpTimer* timer);

	/** Lay out elements' target rectangles on the screen.
	 *
	 * The target rectangles are supposed to be used for animating the
	 * elements' content from the origin/source rectangles to the target
	 * rectangles.
	 *
	 * Grow a number of elements by the given scale factor, then lay them out
	 * inside a given rectangle on the view as nice as we can, given a padding
	 * and spacing in pixels. Overlapping of elements is not allowed, and they
	 * should appear as close to their origin position as possible. Test cases
	 * needs to be written for this, as it is kind of black magic. The padding
	 * is an area of the target rectangles which can be used for
	 * decoration/borders and is not to be considered part of the origin rect.
	 *
	 * After calling this function, the target rectangles can be retrieved by
	 * calling each element covering rects' GetTargetRect() function.
	 *
	 * The way it currently works is:
	 * - Grow all the links' covering rectangles by <code>scale</code> and
	 *   center on their origin
	 * - Expand them by the padding and half the spacing
	 * - Lay out links covering several rectangles internally
	 * - Expand the rects by the rest of the spacing
	 * - Move the elements' union rects so they don't overlap in the Y and X
	 *   direction
	 * - Inset the rectangles by the spacing added.
	 * - Rectangles larger than radius*2 in both dimensions result
	 *   in empty target rects and should not be visualized.
	 * - Translate all expanded elements so they are all inside of the edge
	 *   of the screen closes to the point the user clicked on
	 *
	 * @param dest The destination rectangle in which we try to place the
	 * elements' target rectangles IF they can fit
	 * @param spacing Spacing in pixels in all directions outside a target rect
	 * @param padding Padding in pixels for all the target rectangles. The
	 * padding area is added to the target rects in addition to growing the
	 * origin rects by the scale.
	 * @param scale Factor to grow the origin/source rectangles by
	 */
	OP_STATUS LayoutElements(const OpRect& dest, int spacing, int padding, float scale);

	/** Remove elements unsuitable for expansion. */
	void RemoveUnsuitableElements(const OpRect& visible_screen_rect, unsigned int max_elm_size);

	/** Adjust destination position of elements. Used during layout stage.
	 * @param offset_x The delta to offset the x property of the rectangle
	 * @param offset_y The delta to offset the y property of the rectangle
	 */
	void TranslateElementDestinations(int offset_x, int offset_y);

	/** Layout elements' containing rects (union of all the element fragments)
	 * so they don't overlap.
	 *
	 * @param tmp_elms An array of pointers to the elements to space out
	 * @param count The number of elements in tmp_elms
	 */
	OP_STATUS RemoveElementsOverlap(ElementOfInterest** tmp_elms, int count);

	/** Used for stdlib's qsort to compare the Y position of two elements.
	 *
	 * @param p1 A const ElementOfInterest* const * to compare with p2
	 * @param p2 A const ElementOfInterest* const *
	 * @return See the man page for qsort
	 */
	static int CmpXDir(const void* p1, const void* p2);

	/** Used for stdlib's qsort to compare the X position of two elements.
	 *
	 * @param p1 A const ElementOfInterest* const * to compare with p2
	 * @param p2 A const ElementOfInterest* const *
	 * @return See the man page for qsort
	 */
	static int CmpYDir(const void* p1, const void* p2);

	/** Expand all elements. */
	void ExpandElements();

	/** Change the state.
	 *
	 * This will invoke state-transition triggered functionality.
	 *
	 * @param s The new state
	 */
	void ChangeState(ElementExpanderState s);

	/** Advance from current transition state to next non-animated state. */
	void AdvanceState();

	/** @return TRUE if there are still elements of interest visible on screen, FALSE otherwise. */
	BOOL AnyElementsVisible() const;

	OpPoint OriginClickOnScreen() const;

	/** Was it ambiguous which element was clicked on? */
	BOOL HasAmbiguity() const;

	/** Find the horizontal and vertical edge of a rectangle which is closest to a point.
	 *
	 * @param horiz_edge Output parameter, will contain the horizontal edge of
	 *                   r which is closest to p.x
	 * @param vert_edge  Output parameter, will contain the vertical edge of
	 *                   r which is closest to p.y
	 * @param p          Point to find closest edge to
	 * @param r			 Rectangle which' edges to find which are closest to p
	 */
	static void GetNearestEdge(Edge& horiz_edge, Edge& vert_edge,
									 const OpPoint& p, const OpRect& r);

	/** returns the ElementOfInterest at the givien position (NULL if no ElementOfInterest at this position) */
	ElementOfInterest* GetEoiAtPosition(OpPoint pos);

	FramesDocument* document;

	OpWindow* overlay_window;

	ElementExpanderContainer* overlay_container;

	/** Origin point (where it was clicked).
	 *
	 * This is relative to the top left corner of the document, in document
	 * coordinates. */
	OpPoint origin;

	/** Overlay window rectangle, relative to the top left corner of the screen. */
	OpRect overlay_rect;

	/** Maximum distance for an element to be considered. */
	unsigned int radius;

	/** Scale factor for expanded elements. */
	float scale_factor;

	/** Minimum size of expanded window. */
	int element_minimium_size;

	/** The number of currently running animations */
	int running_animations;

	OpTimer timer;

	/** The finite state of the expander. */
	ElementExpanderState state;
};

#endif // ELEMENT_EXPANDER_IMPL_H
