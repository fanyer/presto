/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef TOOLKIT_WIDGET_PAINTER_H
#define TOOLKIT_WIDGET_PAINTER_H

#include "platforms/quix/toolkits/NativeSkinElement.h"

/** @brief Override functions in this class to draw a complete widget using the toolkit
 */
class ToolkitWidgetPainter
{
public:
	virtual ~ToolkitWidgetPainter() {}

	/** @return true if painter can draw a scrollbar, otherwise false
	 */
	virtual bool SupportScrollbar() const = 0;

	/** @return true if painter can draw a slider, otherwise false
	 */
	virtual bool SupportSlider() const = 0;

	/** @return A slider object
	  * if non-NULL, the caller is responsible for the deletion of this object
	  */
	virtual class ToolkitSlider* CreateSlider() = 0;

	/** @return A scrollbar object
	  * if non-NULL, the caller is responsible for the deletion of this object
	  */
	virtual class ToolkitScrollbar* CreateScrollbar() = 0;

	/** @return width of vertical scrollbars in pixels
	  */
	virtual int GetVerticalScrollbarWidth() = 0;

	/** @return height of horizontal scrollbars in pixels
	 */
	virtual int GetHorizontalScrollbarHeight() = 0;

	/** @return size of first scrollbar button
	  */
	virtual int GetScrollbarFirstButtonSize() = 0;

	/** @return size of second scrollbar button
	 */
	virtual int GetScrollbarSecondButtonSize() = 0;

};

class ToolkitWidget
{
public:
	virtual ~ToolkitWidget() {}

	/** Draw a widget onto a bitmap
	  * @param bitmap Where to place bitmap data in ARGB format, ordered by line first (top to bottom) and by column second (left to right)
	  * @param width Width of bitmap
	  * @param height Height of bitmap
	  */
	virtual void Draw(uint32_t* bitmap, int width, int height) = 0;

	/** Instruct the caller to initialize the background of the widget.
	  * A widget implemetation should set this to false only if the widget
	  * is opaque and fills the entire bitmap.
	  *
	  * @return true (initialize the background) or false (do not initialize the background)
	  */
	virtual bool RequireBackground() = 0;
};

class ToolkitScrollbar : public ToolkitWidget
{
public:
	enum Orientation
	{
		HORIZONTAL,
		VERTICAL
	};

	/** Set orientation of scrollbar */
	virtual void SetOrientation(Orientation orientation) = 0;

	/** @param value Current position of scrollbar
	  * @param min Minimum position of scrollbar
	  * @param max Maximum position of scrollbar
	  * @param visible Viewport size (size of slider relative to total document)
	  */
	virtual void SetValueAndRange(int value, int min, int max, int visible) = 0;

	enum HitPart
	{
		NONE,
		ARROW_SUBTRACT,
		ARROW_ADD,
		TRACK_SUBTRACT,
		TRACK_ADD,
		KNOB
	};

	/** Determine which part of a scrollbar has been hit
	 * @param x, y Coordinates of hit
	 * @param width, height Size of scrollbar to test
	 * @return Which part has been hit
	 */
	virtual HitPart GetHitPart(int x, int y, int width, int height) = 0;

	/** Determine the position and size of the 'knob'
	  * @param x, y, width, height Coordinates output
	  */
	virtual void GetKnobRect(int& x, int& y, int& width, int& height) = 0;

	virtual void SetHitPart(HitPart part) = 0;
	virtual void SetHoverPart(HitPart part) = 0;

	// From ToolkitWidget
	virtual bool RequireBackground() { return false; }
};


class ToolkitSlider : public ToolkitWidget
{
public:
	enum Orientation
	{
		HORIZONTAL,
		VERTICAL
	};

	enum HitPart
	{
		NONE,
		KNOB
	};

	enum State
	{
		ENABLED = 0x01,
		FOCUSED = 0x02,
		ACTIVE  = 0x04,
		RTL		= 0x08
	};

	virtual void SetOrientation(Orientation orientation) = 0;
	virtual void SetValueAndRange(int value, int min, int max, int num_tick_values) = 0;
	virtual void SetState(int state) = 0;
	virtual void SetHoverPart(HitPart part) = 0;

	/** Determine the position and size of the 'knob'
	  * @param x, y, width, height Coordinates output
	  */
	virtual void GetKnobRect(int& x, int& y, int& width, int& height) = 0;

	virtual void GetTrackPosition(int& start_x, int& start_y, int& stop_x, int &stop_y) = 0;

	// From ToolkitWidget
	virtual bool RequireBackground() { return true; }
};



#endif // TOOLKIT_WIDGET_PAINTER_H
