/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef QUICK_GHOST_INPUT_VIEW_H
#define QUICK_GHOST_INPUT_VIEW_H

#ifdef VEGA_OPPAINTER_SUPPORT

#include "modules/libgogi/mde.h"

class OpWidget;

class QuickGhostInputView;
class QuickGhostInputViewListener
{
public:
	~QuickGhostInputViewListener(){}
	virtual void OnGhostInputViewDestroyed(QuickGhostInputView* gv) = 0;
};

class QuickGhostInputView : public MDE_View
{
public:
	QuickGhostInputView(OpWidget* w, QuickGhostInputViewListener* l);
	~QuickGhostInputView();

	virtual void OnMouseDown(int x, int y, int button, int clicks, ShiftKeyState keystate);
	virtual void OnMouseUp(int x, int y, int button, ShiftKeyState keystate);
	virtual void OnMouseMove(int x, int y, ShiftKeyState keystate);
	virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);
	virtual bool IsType(const char* type);

	virtual bool IsLeftButtonPressed();

	void SetWidgetVisible(BOOL vis);
	void SetMargins(unsigned int xmarg, unsigned int ymarg);
	void WidgetRectChanged();

	// silence some asserts:
	void OnDragStart(int start_x, int start_y, ShiftKeyState modifiers, int current_x, int current_y);
	void OnDragDrop(int x, int y, ShiftKeyState modifiers);
	void OnDragEnd(int x, int y, ShiftKeyState modifiers);
	void OnDragMove(int x, int y, ShiftKeyState modifiers) { }
	void OnDragLeave(ShiftKeyState modifiers) { }
	void OnDragCancel(ShiftKeyState modifiers);
	void OnDragEnter(int x, int y, ShiftKeyState modifiers) { }

private:
	OpWidget* m_widget;
	QuickGhostInputViewListener* m_listener;
	unsigned int m_left_right_margin;
	unsigned int m_top_bottom_margin;
};

#endif // VEGA_OPPAINTER_SUPPORT

#endif // QUICK_GHOST_INPUT_VIEW_H
