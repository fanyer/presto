/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef SCROLLABLE_H
#define SCROLLABLE_H

#include "modules/display/coreview/coreview.h"
#include "modules/dom/domeventtypes.h"
#include "modules/layout/layout_coord.h"
#include "modules/widgets/OpWidget.h"

class Content_Box;
class FramesDocument;
class HTMLayoutProperties;
class LayoutProperties;
class OpInputAction;
class OpScrollbar;
class VisualDevice;
struct AbsoluteBoundingBox;
struct LayoutInfo;

/** The CachedPosition class is for content that needs to remember
	their position for some reason. This replaces the previous use of
	saving absolute coordinates since this also works in transformed
	contexts. */

class CachedPosition
{
private:
	AffinePos		ctm;

public:

	/** Get the `edges' rectangle in document coordinates. */

	OpRect			GetEdgesInDocumentCoords(const OpRect& edges) const;

	/** Get position in document */

	AffinePos		GetPosInDocument() { return ctm; }

	/** Set the position in document */

	void			SetPosition(const AffinePos& m) { ctm = m; }

	/* Transform the rect `r' from local coordinates to document
	   coordinates. */

	void			ApplyCTM(OpRect& r) const { ctm.Apply(r); }

	/* Transform the point `p' from local coordinates to document
	   coordinates. */

	void			ApplyCTM(OpPoint& p) const { ctm.Apply(p); }
};

/** Mix-in class for scrollable content (with or without scrollbar).

	Also simply known as "a scrollable".

	This class can be inherited to provide scrolling functionality to
	containers or any other type of content. */

class ScrollableArea
	: private OpWidgetListener,
	  public CachedPosition,
	  public CoreView
{
private:

	// BEGIN - implementation of OpWidgetListener
	virtual void	OnScroll(OpWidget* widget, INT32 old_val, INT32 new_val, BOOL caused_by_input);
	// END - implementation of CoreView

	// BEGIN - implementation of CoreView
	virtual BOOL	OnInputAction(OpInputAction* action);
	virtual BOOL	GetBoundingRect(OpRect& rect);
	// END - implementation of CoreView

protected:

	/** FramesDocument to which this container belongs.

		Needs to be stored, because we implement
		OpInputContext::GetBoundingRect(), and for other reasons as well. */

	FramesDocument* doc;

	/** Horizontal scrollbar, if any. */

	OpScrollbar*	horizontal_scrollbar;

	/** Vertical scrollbar, if any. */

	OpScrollbar*	vertical_scrollbar;

	/** Distance between rightmost and leftmost possible scroll position. */

	LayoutCoord		total_horizontal_scroll_amount;

	/** Distance between topmost and bottommost possible scroll position. */

	LayoutCoord		total_vertical_scroll_amount;

	/** X scroll position. Will be ignored if 'horizontal_scrollbar' is present.

		During a layout phase (between Layout and FinishLayout) it serves as
		temporary storage for a current horizontal scroll position in order to
		restore original scroll value after layout has finished */

	LayoutCoord		scroll_x;

	/** Unconstrained (not limited by min/max scroll range) X scroll position.

		Will be ignored if 'horizontal_scrollbar' is present. */

	LayoutCoord		unconstrained_scroll_x;

	/** Y scroll position. Will be ignored if 'vertical_scrollbar' is present.

		During a layout phase (between Layout and FinishLayout) it serves as
		temporary storage for a current vertical scroll position in order to
		restore original scroll value after layout has been finished */

	LayoutCoord		scroll_y;

	/** Unconstrained (not limited by min/max scroll range) Y scroll position.

		Will be ignored if 'vertical_scrollbar' is present. */

	LayoutCoord		unconstrained_scroll_y;

	/** Real width of padding box area. */

	LayoutCoord		scroll_padding_box_width;

	/** Real height of padding box area. */

	LayoutCoord		scroll_padding_box_height;

	union
	{
		struct
		{
			/** Vertical scrollbar was on at the start of this reflow pass. */

			unsigned int
					vertical_was_on:1;

			/** Vertical scrollbar is on. */

			unsigned int
					vertical_on:1;

			/** Horizontal scrollbar was on at the start of this reflow pass. */

			unsigned int
					horizontal_was_on:1;

			/** Horizontal scrollbar is on. */

			unsigned int
					horizontal_on:1;

			/** If set, do not remove scrollbars, even if the size of the container would
				suggest that it would be appropriate. This is necessary to avoid infinite
				reflow loops. This flag is reset when content changes, so that we can
				re-evaluate the need for scrollbars then. */

			unsigned int
					lock_auto_scrollbars:1;

			/** Set when we don't want auto scrollbars to become visible.

				Only used during reflow (could be moved to a reflow state
				structure, had this class had one). */

			unsigned int
					prevent_auto_scrollbars:1;

			/** The border-top-width CSS property in pixels. */

			unsigned int
					border_top:12;

			/** The border-left-width CSS property in pixels. */

			unsigned int
					border_left:12;

			/** TRUE if vertical scrollbar is on the left (RTL text direction containers) */

			unsigned int
					left_hand_scrollbar:1;

			// room for 1 more bit.

		} sc_packed;
		UINT32		sc_packed_init;
	};

					ScrollableArea() :
						doc(NULL),
						horizontal_scrollbar(NULL),
						vertical_scrollbar(NULL),
						total_horizontal_scroll_amount(0),
						total_vertical_scroll_amount(0),
						scroll_x(0),
						unconstrained_scroll_x(0),
						scroll_y(0),
						unconstrained_scroll_y(0),
						scroll_padding_box_width(0),
						scroll_padding_box_height(0),
						sc_packed_init(0) {}

					~ScrollableArea();

	/** Attach to CoreView hierarchy. */

	void			InitCoreView(HTML_Element* html_element, VisualDevice* vis_dev);

	/** Detach from CoreView hierachy. */

	void			DisableCoreView();

	/** Set up scrollable area and scrollbars, and store current state.

		Call this to prepare for layout. */

	OP_STATUS		SetUpScrollable(LayoutProperties* cascade, LayoutInfo& info);

	/** Set scrollbars' position, dimension and ranges.

		Call this when finishing layout. */

	void			FinishScrollable(const HTMLayoutProperties& props, LayoutCoord border_width, LayoutCoord border_height);

	/** Save scroll position in the HTML element. */

	void			SaveScrollPos(HTML_Element* html_element, LayoutCoord sx, LayoutCoord sy);

	/** Restore previously saved (if any) scroll position.

		Restored value is removed from HTML element and deleted.

		@param html_element The HTML element
		@param x saved horizontal scroll position
		@param y saved vertical scroll position

		@return IS_TRUE if there was a previously saved value available,
		IS_FALSE if not, or ERR_NO_MEMORY in case of OOM. */

	OP_BOOLEAN		GetSavedScrollPos(HTML_Element* html_element, LayoutCoord& x, LayoutCoord& y);

	/** Update the content size. Fed by child content. Used to calculate the scrollbars. */

	void			UpdateContentSize(const AbsoluteBoundingBox& child_bounding_box);

	/** Based on new layout, re-evaluate the need for scrollbars and enable and disable them as appropriate. */

	void			DetermineScrollbars(const LayoutInfo& info, const HTMLayoutProperties& props, LayoutCoord border_box_width, LayoutCoord border_box_height);

	/** Return TRUE if a horizontal or vertical scrollbar was added or removed in this reflow pass. */

	BOOL			HasScrollbarVisibilityChanged() const { return sc_packed.vertical_on != sc_packed.vertical_was_on || sc_packed.horizontal_on != sc_packed.horizontal_was_on; }

	/** Set unconstrained horizontal scroll offset */

	void			SetUnconstrainedScrollX(LayoutCoord new_x) { unconstrained_scroll_x = new_x; }

	/** Set unconstrained vertical scroll offset */

	void			SetUnconstrainedScrollY(LayoutCoord new_y) { unconstrained_scroll_y = new_y; }

	/** Move the area within this content with dx and dy pixels. */

	void			ScrollContent(FramesDocument* doc, LayoutCoord dx, LayoutCoord dy, BOOL caused_by_input);

#ifndef MOUSELESS

	/** Generate a widget (scrollbar) event based on the DOM event. */

	void			GenerateWidgetEvent(DOM_EventType event, int offset_x, int offset_y);

#endif // !MOUSELESS

public:

	/** Get the layout box that owns this ScrollableArea.

		The member methods of this mix-in should aim to avoid calling this
		method at all, to maintain a separation of the concerns "scrollable
		area" and "content/box". Users of this class, on the other hand, may
		find this method necessary. */

	virtual Content_Box*
					GetOwningBox() = 0;

	/** Return TRUE if the horizontal scrollbar is currently on. */

	BOOL			HasHorizontalScrollbar() const { return sc_packed.horizontal_on; }

	/** Return TRUE if the vertical scrollbar is currently on. */

	BOOL			HasVerticalScrollbar() const { return sc_packed.vertical_on; }

	/** Return TRUE if the vertical scrollbar should be placed to the left.

		The vertical scrollbar may be left-aligned in right-to-left writing
		mode if preferences settings allow it, and also if the UI is
		left-handed. */

	BOOL			LeftHandScrollbar() const { return sc_packed.left_hand_scrollbar; }

	/** Allow automatic scrollbars that are currently on to be turned off in the next reflow pass.

		This method should only be called when content has changed. */

	void			UnlockAutoScrollbars() { sc_packed.lock_auto_scrollbars = 0; }

	/** Prevent auto scrollbars from being turned on at the end of layout.

		This method should only be called when we're sure there'll be another
		reflow pass (because of min/max width changes, for instance). */

	void			PreventAutoScrollbars() { sc_packed.prevent_auto_scrollbars = 1; }

	/** Get the width of the currently displayed horizontal scrollbar, if any, otherwise 0. */

	LayoutCoord		GetHorizontalScrollbarWidth() const;

	/** Get the width of the currently displayed vertical scrollbar, if any, otherwise 0. */

	LayoutCoord		GetVerticalScrollbarWidth() const;

	/** Get width of the padding box area of the element that is shown in the scrollable. */

    LayoutCoord		GetScrollPaddingBoxWidth() const { return scroll_padding_box_width; }

	/** Get height of the padding box area of the element that is shown in the scrollable. */

    LayoutCoord		GetScrollPaddingBoxHeight() const { return scroll_padding_box_height; }

	/** Return TRUE if the specified point (in document coordinates) is over a scrollbar. */

	BOOL			IsOverScrollbar(const OpPoint& pos);

	/** Return TRUE if the content overflowed horizontally. */

	BOOL			IsOverflowedX() const { return total_horizontal_scroll_amount > 0; }

	/** Return TRUE if the content overflowed vertically. */

	BOOL			IsOverflowedY() const { return total_vertical_scroll_amount > 0; }

	/** Return the current horizontal scroll value. */

	LayoutCoord		GetViewX(BOOL constrained = TRUE) const;

	/** Return the current vertical scroll value. */

	LayoutCoord		GetViewY(BOOL constrained = TRUE) const;

	/** Set horizontal scroll value. */

	void			SetViewX(LayoutCoord new_x, BOOL caused_by_input, BOOL by_layout = FALSE);

	/** Set vertical scroll value. */

	void			SetViewY(LayoutCoord new_y, BOOL caused_by_input, BOOL by_layout = FALSE);

	/** Update and report current position of CoreView and scrollbars, relative to the document.

		The CoreView and widget systems need this. */

	void			UpdatePosition(const HTMLayoutProperties& props, VisualDevice* visual_device, LayoutCoord visible_width, LayoutCoord visible_height);

	/** Paint the scrollbars. */

	void			PaintScrollbars(const HTMLayoutProperties& props, VisualDevice* visual_device, LayoutCoord visible_width, LayoutCoord visible_height);

	/** Trigger scroll operation based on the given input action.

		@param action The action to perform
		@param use_scrollbars If TRUE, scrollbars will be used for scrolling
		(if scrollbars are not available, the action will be ignored). If
		FALSE, scroll operation will be performed directly on the view.

		@return TRUE if a scroll operation was performed, FALSE otherwise. */

	BOOL			TriggerScrollInputAction(OpInputAction* action, BOOL use_scrollbars = TRUE);
};

#endif // SCROLLABLE_H
