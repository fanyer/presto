/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ELEMENT_OF_INTEREST_H
#define ELEMENT_OF_INTEREST_H

#include "modules/widgets/finger_touch/element_expander_impl.h"
#include "modules/widgets/finger_touch/anchor_region.h"
#include "modules/display/FontAtt.h"
#include "modules/pi/OpWindow.h"
#include "modules/util/simset.h"

class HTML_Element;
class VisualDevice;
class ElementExpanderContainer;

/** Representation of one element of interest (EOI).
 *
 * An element of interest can be an anchor link, a form element or some other
 * user-interactable object that originally may be not be big enough for the
 * user to easily hit it with a finger. An element of interest will be expanded
 * when it is within a given radius of a "mouse" click, initiated by
 * ElementExpander.
 *
 * Some elements may have a second expanded mode, hereby called 'edit mode',
 * which will allow the user, after having selected it, to further interact
 * with it easily using a finger. Elements enter edit mode (or simply get
 * activated) by calling their Activate() function.
 *
 * When expanded, an element of interest gets its own window (OpWindow). Note
 * that the window isn't created initially; PrepareForDisplay() must be called.
 *
 * This class uses the NVI (non virtual interface idiom). This means that we
 * keep all virtual functions private. This way the public interface is kept
 * stable, while there is more freedom to change the implementation See for
 * example http://www.gotw.ca/publications/mill18.htm
 */
class ElementOfInterest :
	public Link,
	public OpWidgetListener,
	public OpWindowAnimationListener
{
public:
	ElementOfInterest(HTML_Element* html_element) :
		window(0),
		widget_container(0),
		background_color(USE_DEFAULT_COLOR),
		animation_scheduled(FALSE),
		animation_running(FALSE),
		html_element(html_element),
		text_color(USE_DEFAULT_COLOR),
		distance(UINT_MAX),
		padding(0),
		active(FALSE),
#ifdef SUPPORT_TEXT_DIRECTION
		is_rtl(FALSE),
#endif
		hidden(FALSE)
		{ OP_ASSERT(html_element); }

	virtual ~ElementOfInterest();

	/** Get the previous element owned by the expander, or NULL if there is no previous. */
	ElementOfInterest* Suc() const { return (ElementOfInterest*) Link::Suc(); }

	/** Get the next element owned by the expander, or NULL if there is no next */
	ElementOfInterest* Pred() const { return (ElementOfInterest*) Link::Pred(); }

	/** Is the element suitable for expansion? */
	BOOL IsSuitable(unsigned int max_elm_size) { return DoIsSuitable(max_elm_size); }

	/** @return TRUE if this EOI can enter edit mode, i.e. a second state after
	 * Activate() is called where the user can interact with the element
	 */
	BOOL HasEditMode() const { return DoHasEditMode(); }

	BOOL IsActive() const { return active; }

	void GenerateInitialDestRect(float scale, unsigned int max_elm_size, const OpPoint& click_origin)
	{
		DoGenerateInitialDestRect(scale, max_elm_size, click_origin);
	}

	/** Translate the destination rectangle by a given offset. Used during the layout stage. */
	void TranslateDestination(int offset_x, int offset_y) { DoTranslateDestination(offset_x, offset_y); }

	/** Move the window by the specified number of pixels.
	 *
	 * This will move both destination and origin rectangles.
	 */
	virtual void Move(int dx, int dy);

	/** Activate the element.
	 *
	 * Follows a link, opens up form elements like dropdown lists, opens edit
	 * boxes in 'edit mode', selects check boxes and radio buttons. Can be
	 * called in "expanded" mode or directly from "hidden" state.
	 *
	 * ElementOfInterest implementations must specify what to do when activating
	 * them through overriding DoActivate().
	 */
	void Activate();

	/** Expand an element.
	 *
	 * Can be called while the expander is in "hidden" mode. Triggers the
	 * element getting displayed using its' destination rectangle in 'expanded'
	 * state.
	 */
	void Expand();

	/** Prepare this element for display. Show its window. */
	OP_STATUS PrepareForDisplay();

    /** Hide the element.
	 *
	 * The hiding should be animated, by fading it out or something similar.
	 */
	void Hide() { if (!hidden) DoHide(); }

	/** Prepare this element for layout. Create its window. */
	OP_STATUS PrepareForLayout(OpWindow* parent_window);

	/** Get the square of the shortest distance from the origin point (click
	 * center) to the element, in screen values. */
	unsigned int GetDistance() const { return distance; }

	/** Set the square of the shortest distance from the origin point (click
	 * center) to the element, in screen values. */
	void SetDistance(unsigned int dist) { distance = dist; }

	/** Get the EOI's anchor/form main HTML-element.
	 *
	 * Note that an EOI can contain several HTML elements.
	 *
	 * @return For links, the anchor element. For form elements, the widget
	 * element.
	 */
	HTML_Element* GetHtmlElement() const { return html_element; }

	/** Returns the original position of this element of interest on screen */
	const OpRect GetOriginalRect() const { return orig_rect; }

	/** Extend original rectangle to include r, used during layout */
	void UnionOriginalRectWith(const OpRect& r) { orig_rect.UnionWith(r); }

	/** Move the original rectangle.
	 *
	 * @param dx Number of pixels to move the rectangle rightwards. Negative values will move it leftwards.
	 * @param dy Number of pixels to move the rectangle downwards. Negative values will move it upwards.
	 */
	void OffsetOriginalRectBy(INT32 dx, INT32 dy) { orig_rect.OffsetBy(dx, dy); };

	/** Get the destination rectangle for this element of interest */
	const OpRect GetDestination() const { return dest_rect; }

	/** Set the destination rectangle's padding.
	 *
	 * Will expand the destination by the padding amount of pixels in each
	 * direction
	 */
	void SetDestinationPadding(unsigned int padding);

	/** @return The activated widget for this element, if applicable, otherwise NULL. */
	OpWidget* GetActivatedWidget() const { return IsActive() ? DoGetWidget() : 0; }

	void SetFont(const FontAtt& f);
	void SetTextColor(COLORREF text_color) { this->text_color = text_color; }
	void SetBackgroundColor(COLORREF background_color) { this->background_color = background_color; }
	void AppendFragment(AnchorFragment* fragment) { fragment->Into(&region); }
#ifdef SUPPORT_TEXT_DIRECTION
	void SetRTL(BOOL rtl) { is_rtl = rtl; }
#endif

	BOOL HasScheduledAnimation() const { return animation_scheduled; }

	/** Start scheduled animation, if any. */
	void StartAnimation();

protected:
	static OpRect ScaleRectFromCenter(const OpRect& orig_rect, float scale, const OpPoint& click_origin);
	ElementExpanderImpl* GetElementExpander() const { return static_cast<ElementExpanderImpl*>(GetList()); }

	/** Schedule element for animation. */
	void ScheduleAnimation(const OpRect& origin, float origin_opacity, const OpRect& dest, float dest_opacity, int duration_in_ms);

	// From OpWindowAnimationListener:
	void OnAnimationFinished(OpWindow* window);

	void AdjustSize();
	int GetMaxWidth();

	OP_STATUS PaintFragments(const OpRect& paint_rect, VisualDevice* vis_dev);

	/** Expanded element position and size, relative to the top left corner of
	 *  the screen. */
	OpRect dest_rect;
	AnchorRegion region;
	OpWindow* window;
	ElementExpanderContainer* widget_container;
	/** Original element position and size (in the document), relative to the
	 * top left corner of the view, in screen coordinates. */
	OpRect orig_rect;
	COLORREF background_color;
	FontAtt font;
#ifdef SKIN_SUPPORT
	OpWidgetImage skin;
#endif

private:
	virtual OP_STATUS InitContent() { return OpStatus::OK; }

	/** Implementation for IsSuitable() */
	virtual BOOL DoIsSuitable(unsigned int max_elm_size) { return TRUE; }

	/** Implementation for HasEditMode(). Override it for elements with editmode. */
	virtual BOOL DoHasEditMode() const { return FALSE; }

	/** Implementation for Activate() */
	virtual void DoActivate() = 0;

	/** Implementation for GenerateInitialDestRect() */
	virtual void DoGenerateInitialDestRect(float scale, unsigned int max_elm_size, const OpPoint& click_origin);

	/** Implementation for TranslateDestination() */
	virtual void DoTranslateDestination(int offset_x, int offset_y);

	/** Implementation for Hide() */
	virtual void DoHide();

	/** Return the element's clone widget if any. must be overridden by form elements. */
	virtual OpWidget* DoGetWidget() const { return 0; }

	OpWindowAnimation animation;

	BOOL animation_scheduled;

	BOOL animation_running;

	HTML_Element* html_element;

	COLORREF text_color;

	/** The square of the shortest distance from the origin point (click
	 * center) to the element, in screen values. */
	unsigned int distance;

	/** Padding of this element. */
	unsigned int padding;

	/** Has Activate() been called for this widget? */
	BOOL active;

#ifdef SUPPORT_TEXT_DIRECTION
	BOOL is_rtl;
#endif // SUPPORT_TEXT_DIRECTION

	BOOL hidden;
};

#endif // ELEMENT_OF_INTEREST_H
