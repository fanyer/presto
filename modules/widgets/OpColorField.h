/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_COLOR_BUTTON_H
#define OP_COLOR_BUTTON_H

#include "modules/widgets/OpButton.h"
#include "modules/widgets/WidgetWindowHandler.h"
#include "modules/windowcommander/OpWindowCommander.h"

#define COLORBOX_ARROW_WIDTH 10

/** OpColorMatrix - Widget for showing a multiple colors. */

class OpColorMatrix : public OpWidget
{
protected:
	OpColorMatrix();
	virtual	~OpColorMatrix();
public:
	static OP_STATUS Construct(OpColorMatrix** obj);

	/** If this element belongs to a formobject which has the list attribute, it will fetch colors from it.
		@param widget_with_form The widget that might have a FormObject. */
	void PopulateFromDatalist(OpWidget *widget_with_form);

	/** Set the currently picked color */
	void SetPickedColor(COLORREF color);
	COLORREF GetPickedColor() { return m_picked_color; }

	/** Set the list of colors that should be shown in this color matrix */
	OP_STATUS SetColors(COLORREF *colors, int num_colors);

	void SetCellSize(int cell_size) { m_cell_size = cell_size; }

	int CalculateWidth();
	int CalculateHeight();

	// == Hooks ======================
	void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
#ifndef MOUSELESS
	void OnMouseMove(const OpPoint &point);
	void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	void OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
#endif // !MOUSELESS

	// == OpInputContext ======================
	virtual BOOL		OnInputAction(OpInputAction* action);

	// Implementing the OpTreeModelItem interface
	virtual Type		GetType() {return WIDGET_TYPE_COLOR_MATRIX;}
	virtual BOOL		IsOfType(Type type) { return type == WIDGET_TYPE_COLOR_MATRIX || OpWidget::IsOfType(type); }
	virtual BOOL		IsParentInputContextAvailabilityRequired();

private:
	/** Get which color index that is positioned at the given point, or -1 if none */
	int GetColorIndexFromPoint(const OpPoint& point);
	COLORREF m_default_colors[20];
	COLORREF *m_colors;
	COLORREF m_picked_color;
	int m_num_colors;
	int m_cell_size;
	int m_active_cell_index;
};

/** OpColorBox - Widget for showing and selecting a color. */

class OpColorBox : public OpWidget
#ifndef QUICK
			 , public OpWidgetListener
#endif // QUICK
{
protected:
	OpColorBox();
	virtual	~OpColorBox() {}
public:
	static OP_STATUS Construct(OpColorBox** obj);

	void SetColor(COLORREF color, bool force_no_onchange);
	COLORREF GetColor() { return m_color; }

	OP_STATUS GetText(OpString &str);
	OP_STATUS SetText(const uni_char* text);

	void SetReadOnly(BOOL readonly);
	BOOL IsReadOnly() { return m_is_readonly; }

	void OpenWindow();
	void CloseWindow() { m_window_handler.Close(); }

	void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

	// == Hooks ======================
	void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
#ifndef MOUSELESS
	void OnMouseMove(const OpPoint &point);
	void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	void OnMouseLeave();
#endif // !MOUSELESS
	void OnRemoving();
	void OnMove();
	void OnDeleted();
	void OnFocus(BOOL focus,FOCUS_REASON reason);
	void OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);

	// Implementing the OpTreeModelItem interface
	virtual Type		GetType() {return WIDGET_TYPE_COLOR_BOX;}
	virtual BOOL		IsOfType(Type type) { return type == WIDGET_TYPE_COLOR_BOX || OpWidget::IsOfType(type); }

	// == OpInputContext ======================
	virtual BOOL		OnInputAction(OpInputAction* action);
	virtual const char*	GetInputContextName() { return "Color Box Widget"; }

	// OpWidgetListener
	virtual void OnClick(OpWidget *widget, UINT32 id);
	virtual void OnChange(OpWidget *widget, BOOL changed_by_mouse);

#ifndef QUICK
	/* These seemingly meaningless overrides are implemented to avoid warnings
	   caused, by the overrides of same-named functions from OpWidget. */
	virtual void OnPaint(OpWidget *widget, const OpRect &paint_rect) {}
	virtual void OnMouseMove(OpWidget *widget, const OpPoint &point) {}
	virtual void OnMouseLeave(OpWidget *widget) {}
#endif // QUICK

private:
	void UpdateWindow();
	OP_STATUS UpdateEditField();
	COLORREF m_color;
	BOOL m_is_readonly;
	BOOL m_is_hovering_button;
	WidgetWindowHandler m_window_handler;

#ifdef WIDGETS_USE_NATIVECOLORSELECTOR
	OpColorSelectionListener::ColorSelectionCallback* m_callback;
public:
	void ResetColorSelectionCallback() { m_callback = NULL; }
#endif // WIDGETS_USE_NATIVECOLORSELECTOR
};

#endif // OP_COLOR_BUTTON_H
