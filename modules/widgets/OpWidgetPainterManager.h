/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_WIDGETPAINTERMANAGER_H
#define OP_WIDGETPAINTERMANAGER_H

class VisualDevice;
class OpStringItem;
class OpBitmap;
class OpWidgetString;
class OpWidget;
class OpPainter;
class OpWidgetInfo;


enum ARROW_DIRECTION { ARROW_UP, ARROW_DOWN, ARROW_LEFT, ARROW_RIGHT };
enum SCROLLBAR_TYPE
{
	SCROLLBAR_NONE,
	SCROLLBAR_CSS,
	SCROLLBAR_HORIZONTAL,
	SCROLLBAR_VERTICAL,
	SCROLLBAR_BOTH,
};

/***********************************************************************************
**
**	OpWidgetPainter interface specifies a high level widget drawing
**	interface that widget may use to draw themselves in the OnPaint method.
**
**	Before OnPaint is called, the widget engine has already called InitPaint
**	on the OpWidgetPainter, giving the widget painter a chance to set up various
**	stuff, including saving the VisualDevice pointer which will not be passed
**	in drawing calls later.
**
**	Similiarly, general widget state info like being enabled/disabled and focused/unfocused
**	is not passed in drawing calls, so the widget painter should store these
**	in InitPaint or query them at draw time through the Widget pointer it got at InitPaint().
**
**	Every OpWidget is in OnPaint() free to mix calls to the
**	widget painter and manually paint using visual device, but the best thing is to
**	let the OpWidgetPainter interface cover our every need.
**
**
**  Note: If you override something and paint with a native systemfunction, you have to scale the
**  destinationrectangle. (Normal LineOut's and DrawRects etc in VisualDevice is automatically scaled for you)
**  Easiest way to do it:  drawrect = vd->ScaleToScreen(drawrect);
**
***********************************************************************************/

class OpWidgetPainter
{
	public:
		virtual ~OpWidgetPainter() {}

		/** Called every time before painting a widget */
		virtual void InitPaint(VisualDevice* vd, OpWidget* widget) = 0;

		/** Return TRUE if the painter draws the widget as it is specified with its colours and settings, instead of with some custom look. */
		virtual BOOL CanHandleCSS() = 0;

		/** Return the OpWidgetInfo class to be used for widget. */
		virtual OpWidgetInfo* GetInfo(OpWidget* widget) = 0;

		/** Draw the arrow-button of a scrollbar.
		 * @param drawrect The rectangle where it should be drawn, in VisualDevice document coordinates.
		 * @param direction ARROW_UP, ARROW_DOWN, ARROW_LEFT or ARROW_RIGHT
		 * @param is_down TRUE if the button is pressed
		 * @param is_hovering TRUE if the button is hovered by the mouse
		 */
		virtual BOOL DrawScrollbarDirection(const OpRect &drawrect, int direction, BOOL is_down, BOOL is_hovering) = 0;

		/** Draw the background of a scrollbar.
		 * @param drawrect The rectangle where it should be drawn, in VisualDevice document coordinates.
		 */
		virtual BOOL DrawScrollbarBackground(const OpRect &drawrect) = 0;

		/** Draw the knob of a scrollbar (The buttonbox that is moving and can be dragged).
		 * @param drawrect The rectangle where it should be drawn, in VisualDevice document coordinates.
		 * @param is_down TRUE if the button is pressed
		 * @param is_hovering TRUE if the button is hovered by the mouse
		 */
		virtual BOOL DrawScrollbarKnob(const OpRect &drawrect, BOOL is_down, BOOL is_hovering) = 0;

		/** Draw entire scrollbar. This is not needed and can return FALSE. Then the different parts will be drawn
		 * separately using DrawScrollbarDirection/DrawScrollbarKnob etc.
		 * @param drawrect The rectangle where it should be drawn, in VisualDevice document coordinates.
		 */
		virtual BOOL DrawScrollbar(const OpRect &drawrect) = 0;

#ifdef PAGED_MEDIA_SUPPORT
		/** Draw the background of a page control.
		 * @param drawrect The rectangle where it should be drawn, in VisualDevice document coordinates.
		 */
		virtual BOOL DrawPageControlBackground(const OpRect &drawrect) = 0;

		/** Draw previous/next page control button.
		 * @param drawrect The rectangle where it should be drawn, in VisualDevice document coordinates.
		 * @param direction ARROW_UP, ARROW_DOWN, ARROW_LEFT or ARROW_RIGHT
		 * @param is_down TRUE if the button is pressed
		 * @param is_hovering TRUE if the button is hovered by the mouse
		 */
		virtual BOOL DrawPageControlButton(const OpRect& drawrect, ARROW_DIRECTION direction, BOOL is_down, BOOL is_hovering) = 0;
#endif // PAGED_MEDIA_SUPPORT

		/** Draw dropdown widget (the part that is not the popupmenu).
		 * @param drawrect The rectangle where it should be drawn, in VisualDevice document coordinates.
		 */
		virtual BOOL DrawDropdown(const OpRect &drawrect) = 0;

		/** Draw the dropdown widget button.
		 * @param drawrect The rectangle where it should be drawn, in VisualDevice document coordinates.
		 * @param is_down TRUE if the button is pressed
		 */
		virtual BOOL DrawDropdownButton(const OpRect &drawrect, BOOL is_down) = 0;

		/** Draw Listbox widget.
		 * @param drawrect The rectangle where it should be drawn, in VisualDevice document coordinates.
		 */
		virtual BOOL DrawListbox(const OpRect &drawrect) = 0;

		/** Draw OpButton widget.
		 * @param drawrect The rectangle where it should be drawn, in VisualDevice document coordinates.
		 * @param value TRUE if the button is pressed.
		 * @param is_default TRUE if the button is a default button (The default is usually marked somehow and is invoked by pressing the ENTER key).
		 */
		virtual BOOL DrawButton(const OpRect &drawrect, BOOL value, BOOL is_default) = 0;

		/** Draw OpEdit widget.
		 * @param drawrect The rectangle where it should be drawn, in VisualDevice document coordinates.
		 */
		virtual BOOL DrawEdit(const OpRect &drawrect) = 0;

		/** Draw DropDownEdit widget (only hint part).
		 * @param drawrect The rectangle where it should be drawn, in VisualDevice document coordinates.
		 */
		virtual BOOL DrawDropDownHint(const OpRect &drawrect) = 0;

		/** Draw OpMultilineEdit widget.
		 * @param drawrect The rectangle where it should be drawn, in VisualDevice document coordinates.
		 */
		virtual BOOL DrawMultiEdit(const OpRect &drawrect) = 0;

		/** Draw radiobutton (OpButton with type TYPE_RADIO).
		 * @param drawrect The rectangle where it should be drawn, in VisualDevice document coordinates.
		 * @param value TRUE if the button is pressed.
		 */
		virtual BOOL DrawRadiobutton(const OpRect &drawrect, BOOL value) = 0;

		/** Draw checkbox (OpButton with type TYPE_CHECKBOX).
		 * @param drawrect The rectangle where it should be drawn, in VisualDevice document coordinates.
		 * @param value TRUE if the button is pressed.
		 */
		virtual BOOL DrawCheckbox(const OpRect &drawrect, BOOL value) = 0;

		/** Draw item in listbox or dropdown menu (or any kind of view that contains items).
		 * @param drawrect The rectangle where it should be drawn, in VisualDevice document coordinates.
		 * @param item Pointer to the item to draw.
		 * @param is_listbox TRUE if it is drawn in a listbox.
		 * @param is_focused_item TRUE if it is the focused item.
		 * @param indent Horizontal indentation in pixels.
		 */
		virtual BOOL DrawItem(const OpRect &drawrect, OpStringItem* item, BOOL is_listbox, BOOL is_focused_item, int indent = 0) = 0;

		/** Draw OpResizeCorner.
		 * @param drawrect The rectangle where it should be drawn, in VisualDevice document coordinates.
		 * @param active TRUE if the corner is enabled for resizing. If it is FALSE it can't be used to resize anything.
		 */
		virtual BOOL DrawResizeCorner(const OpRect &drawrect, BOOL active) = 0;

		/** Draw OpWidgetResizeCorner.
		 *
		 * @param drawrect The rectangle where it should be drawn, in VisualDevice document coordinates.
		 * @param is_active TRUE if the corner is enabled, i.e. can be used for resizing.
		 * @param scrollbar Indicates the type of scrollbar that is visible (if any).
		*/
		virtual BOOL DrawWidgetResizeCorner(const OpRect &drawrect, BOOL is_active, SCROLLBAR_TYPE scrollbar) = 0;

#ifdef QUICK
		virtual BOOL DrawTreeViewRow(const OpRect &drawrect, BOOL focused) = 0;

		virtual BOOL DrawProgressbar(const OpRect &drawrect, double fillpercent, INT32 progress_when_total_unknown, OpWidgetString* string, const char *empty_skin = NULL, const char *full_skin = NULL) = 0;
#endif

		/** Return the given color with the RGB components inverted by factor f.
			If color is white and f is 1, the returned color will be black.
			If color is black and f is 1, the returned color will be white. */
		static UINT32 GetColorInvRGB(UINT32 color, float f);

		/** Return the given with the alpha multiplied with f */
		static UINT32 GetColorMulAlpha(UINT32 color, float f);

		/**
		 * Draws a string that has a popup button. Used to popup a calendar or color box.
		 * @param drawrect The place to draw the string.
		 * @param is_hovering_button TRUE if the button is hovered
		 */
		virtual BOOL DrawPopupableString(const OpRect &drawrect, BOOL is_hovering_button) = 0;

		/**
		 * Draws a slider with a knob and returns the size of the track and the 
		 * knob so that the slider widget can know where the users clicks.
		 *
		 * @param drawrect The place to draw the slider.
		 * @param horizontal TRUE for a horizontal slider, FALSE for a vertical one.
		 * @param min The value for the min position on the widget.
		 * @param max The value for the max position on the widget.
		 * @param pos The current value. This determines where the knob ends up.
		 * @param highlighted TRUE if there should be a focus border.
		 * @param pressed_knob TRUE if the knob should look "pressed".
		 * @param out_knob_position Out parameter that will contain the position
		 * of the drawn knob when the function returns successfully.
		 * @param out_start_track Out parameter that will contain the position
		 * of the start of track when the function returns successfully.
		 * @param out_start_track Out parameter that will contain the position
		 * of the end of track when the function returns successfully.
		 *
		 * @returns TRUE if this method could paint the slider or FALSE if it
		 * didn't do anything.
		 *
		 */
		virtual BOOL DrawSlider(const OpRect& drawrect, BOOL horizontal, double min, 
			double max, double pos, BOOL highlighted, BOOL pressed_knob, 
			OpRect& out_knob_position, OpPoint& out_start_track, 
			OpPoint& out_end_track) = 0;

		/** Draw slider focus if needed */
		virtual BOOL DrawSliderFocus(const OpRect& slider_rect, const OpRect& track_rect, const OpRect& focus_rect) = 0;
		/** Draw slider track */
		virtual BOOL DrawSliderTrack(const OpRect& slider_rect, const OpRect& track_rect) = 0;
		/** Draw slider knob */
		virtual BOOL DrawSliderKnob(const OpRect& slider_rect, const OpRect& track_rect, const OpRect& knob_rect) = 0;

		virtual BOOL DrawMonthView(const OpRect& month_view, const OpRect& header_area)
			{ return FALSE; }
			// XXX This method should be pure virtual but isn't to allow the slider
			// to compile and be used before the method is in IndpWidgetPainter

		virtual BOOL DrawColorBox(const OpRect& drawrect, COLORREF color) { return FALSE; }
};

/***********************************************************************************
**
**	OpWidgetPainterManager implements the OpWidgetPainter interface, but
**	acts only as a redirecting unit, sending the painting calls to a
**	primary or default painter, depending on what either supports.
**
**	Every OpWidget gets a OpWidgetPainter interface passed in OnPaint,
**	and that interface is really an OpWidgetPainterManager.
**
***********************************************************************************/

class OpWidgetPainterManager : public OpWidgetPainter
{
public:
	OpWidgetPainterManager();
	virtual ~OpWidgetPainterManager();

	void SetPrimaryWidgetPainter(OpWidgetPainter* widget_painter);

	/* implmenting the OpWidgetPainter interface */

	void InitPaint(VisualDevice* vd, OpWidget* widget);

	BOOL CanHandleCSS() { return TRUE; } // Not used in paintermanager. ignore.

	OpWidgetInfo* GetInfo(OpWidget* widget);

	BOOL DrawScrollbarDirection(const OpRect &drawrect, int direction, BOOL is_down, BOOL is_hovering);
	BOOL DrawScrollbarBackground(const OpRect &drawrect);
	BOOL DrawScrollbarKnob(const OpRect &drawrect, BOOL is_down, BOOL is_hovering);
	BOOL DrawScrollbar(const OpRect &drawrect);

#ifdef PAGED_MEDIA_SUPPORT
	BOOL DrawPageControlBackground(const OpRect &drawrect);
	BOOL DrawPageControlButton(const OpRect& drawrect, ARROW_DIRECTION direction, BOOL is_down, BOOL is_hovering);
#endif // PAGED_MEDIA_SUPPORT

	BOOL DrawDropdown(const OpRect &drawrect);
	BOOL DrawDropdownButton(const OpRect &drawrect, BOOL is_down);

	BOOL DrawListbox(const OpRect &drawrect);

	BOOL DrawButton(const OpRect &drawrect, BOOL value, BOOL is_default);

	BOOL DrawEdit(const OpRect &drawrect);

	BOOL DrawDropDownHint(const OpRect &);

	BOOL DrawMultiEdit(const OpRect &drawrect);

	BOOL DrawRadiobutton(const OpRect &drawrect, BOOL value);

	BOOL DrawCheckbox(const OpRect &drawrect, BOOL value);

	BOOL DrawItem(const OpRect &drawrect, OpStringItem* item, BOOL is_listbox, BOOL is_focused_item, int indent = 0);

	BOOL DrawResizeCorner(const OpRect &drawrect, BOOL active);

	BOOL DrawWidgetResizeCorner(const OpRect &drawrect, BOOL is_active, SCROLLBAR_TYPE scrollbar);

#ifdef QUICK
	BOOL DrawTreeViewRow(const OpRect &drawrect, BOOL focused);

	BOOL DrawProgressbar(const OpRect &drawrect, double percent, INT32 progress_when_total_unknown, OpWidgetString* string, const char *empty_skin = NULL, const char *full_skin = NULL);
#endif

	BOOL DrawPopupableString(const OpRect &drawrect, BOOL is_hovering_button);
	BOOL DrawSlider(const OpRect& rect, BOOL horizontal, double min, double max, double pos, BOOL highlighted, BOOL pressed_knob, OpRect& out_knob_position, OpPoint& out_start_track, OpPoint& out_end_track);
	BOOL DrawSliderFocus(const OpRect& slider_rect, const OpRect& track_rect, const OpRect& focus_rect);
	BOOL DrawSliderTrack(const OpRect& slider_rect, const OpRect& track_rect);
	BOOL DrawSliderKnob(const OpRect& slider_rect, const OpRect& track_rect, const OpRect& knob_rect);
	BOOL DrawMonthView(const OpRect& drawrect, const OpRect& header_area);
	BOOL DrawColorBox(const OpRect& drawrect, COLORREF color);

	void SetUseFallbackPainter(BOOL status);
	BOOL GetUseFallbackPainter() { return use_fallback; }

	BOOL NeedCssPainter(OpWidget* widget);

#ifdef SKIN_SUPPORT
	/**
	   determines the amount of extra space that needs to be taken
	   from the margins in order to draw the widget using the skin
	   painter, if skin doesn't fit without using margins and there's
	   enough margins to do this.

	   @param widget the widget
	   @param margin_left the left margin
	   @param margin_top the top margin
	   @param margin_right the right margin
	   @param margin_bottom the bottom margin
	   @param left (out) the amount of space used from the left margin
	   @param top (out) the amount of space used from the top margin
	   @param right (out) the amount of space used from the right margin
	   @param bottom (out) the amount of space used from the bottom margin
	   @return TRUE if it would make sense to use parts of the margins
	   to draw skin, FALSE if margins are not needed or it wouldn't
	   make sense to use them.
	 */
	BOOL UseMargins(OpWidget* widget,
					short margin_left, short margin_top, short margin_right, short margin_bottom,
					UINT8& left, UINT8& top, UINT8& right, UINT8& bottom);
#endif // SKIN_SUPPORT

private:
	OpWidgetPainter* painter[2];
	OpWidgetPainter* use_painter[2];
	BOOL use_fallback;
};

#endif // OP_WIDGETPAINTERMANAGER_H
