/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef QUICK_SCROLL_CONTAINER
#define QUICK_SCROLL_CONTAINER

#include "adjunct/quick_toolkit/widgets/OpMouseEventProxy.h"
#include "adjunct/quick_toolkit/widgets/QuickWidget.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetContainer.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetContent.h"

class OpScrollbar;

class QuickScrollContainerListener
{
public:
	virtual ~QuickScrollContainerListener() {}

	/** Notifies observers when content is scrolled.
	  * @param scroll_current_value identifies current scroll position.
	  * @param min The minimum value; typically 0.
	  * @param max typically the length of a document.
	  */
	virtual void OnContentScrolled(int scroll_current_value, int min, int max) = 0;
};

/** @brief Create a scrollable view on a widget
 *
 * The default scrolling parameters of this class is set to mimic the
 * scrolling behaviour of web pages.
 */
class QuickScrollContainer
  : public QuickWidget
  , public QuickWidgetContainer
  , public OpWidgetListener
  , public OpWidgetExternalListener
{
	IMPLEMENT_TYPEDOBJECT(QuickWidget);
public:
	enum Orientation
	{
		HORIZONTAL,
		VERTICAL
	};

	enum Scrollbar
	{
		SCROLLBAR_NONE,  ///< No scrollbar
		SCROLLBAR_AUTO,  ///< Scrollbar appears when necessary
		SCROLLBAR_ALWAYS ///< Scrollbar always appears
	};

	/** Constructor
	  * @param orientation Whether the container should scroll horizontally or vertically
	  * @param scrollbar_behavior Controls if scrollbars will be visible
	  */
	QuickScrollContainer(Orientation orientation = VERTICAL, Scrollbar scrollbar_behavior = SCROLLBAR_AUTO);

	virtual ~QuickScrollContainer();

	/** Initialize scroll container
	  */
	OP_STATUS Init();

	void SetListener(QuickScrollContainerListener* listener) { m_listener = listener; }

	/** Set a scroll offset to use (content should be shifted by this many pixels).
	  * @return The return value indicates whether a scroll was actually performed.
	  */
	BOOL SetScroll(int offset, BOOL smooth_scroll);
	void ScrollToEnd();
	/** Set the contents of the container
	  * @param contents Valid pointer to contents to use in this scroll container
	  *                 Always takes ownership of contents
	  */
	void SetContent(QuickWidget* contents);

	/** Get the contents of the container
	  */
	QuickWidget* GetContent() const { return m_content.GetContent(); }

	/** Get the orientation of the scroll container
	  * @return Vertical or horizontal orientation
	  */
	Orientation GetOrientation() const { return m_orientation; }

	/** Set when scrollbar should appear
	  * @param behavior New behavior
	  */
	void SetScrollbarBehavior(Scrollbar behavior) { m_scrollbar_behavior = behavior; }

	/** Set whether there should be a margin between the scrollbar and the container content
	  * Can be disabled to get a more web page-like behavior. On by default.
	  * @param respect Whether to respect scrollbar margin
	  */
	void SetRespectScrollbarMargin(bool respect) { m_respect_scrollbar_margin = respect; }

	/** @return whether the last call to Layout() caused the scrollbar to
	  *		become visible or hidden
	  */
	bool IsScrollbarVisible() const;

	/** Get scrollbar's size.
	 * @return scrollbar's vertical width or horizontal height (depending on scrollbar's orientation)
	 */
	unsigned GetScrollbarSize() const;

	/** Returns screen-rect of scroll view.
	  */
	OpRect GetScreenRect();

	// Implementing QuickWidget
	virtual BOOL HeightDependsOnWidth() { return FALSE; }
	virtual OP_STATUS Layout(const OpRect& rect);
	virtual void SetParentOpWidget(OpWidget* parent);
	virtual void Show();
	virtual void Hide();
	virtual BOOL IsVisible();
	virtual void SetEnabled(BOOL enabled);

	// Implementing QuickWidgetContainer
	virtual void OnContentsChanged() { BroadcastContentsChanged(); }

	// Implementing OpWidgetListener
	virtual void OnScroll(OpWidget *widget, INT32 old_val, INT32 new_val, BOOL caused_by_input);

	// Implementing OpWidgetExternalListener
	virtual void OnFocus(OpWidget *widget, BOOL focus, FOCUS_REASON reason);

private:
	// Implementing QuickWidget
	virtual unsigned GetDefaultMinimumWidth();
	virtual unsigned GetDefaultMinimumHeight(unsigned width);
	virtual unsigned GetDefaultNominalWidth();
	virtual unsigned GetDefaultNominalHeight(unsigned width);
	virtual unsigned GetDefaultPreferredWidth();
	virtual unsigned GetDefaultPreferredHeight(unsigned width);
	virtual void GetDefaultMargins(WidgetSizes::Margins& margins) { margins = m_content->GetMargins(); }

	OpRect LayoutScrollbar(const OpRect& rect);
	unsigned AddScrollbarWidthTo(unsigned width);
	unsigned AddScrollbarHeightTo(unsigned height);

	class ScrollView;

	QuickWidgetContent m_content;
	Orientation m_orientation;
	Scrollbar m_scrollbar_behavior;
	OpScrollbar* m_scrollbar;
	OpMouseEventProxy* m_canvas;			///< Complete canvas
	ScrollView* m_scrollview;	///< View on canvas
	bool m_respect_scrollbar_margin;
	QuickScrollContainerListener* m_listener;
};

#endif // QUICK_SCROLL_CONTAINER
