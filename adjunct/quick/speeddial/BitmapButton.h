/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */

#ifndef BITMAP_BUTTON_H
#define BITMAP_BUTTON_H

#include "modules/libgogi/pi_impl/mde_bitmap_window.h"

#include "modules/widgets/OpButton.h"


class BitmapButton : public OpButton, public OpBitmapScreenListener
{
public:
	BitmapButton();
	virtual ~BitmapButton();

	OP_STATUS Init(BitmapOpWindow* window);

	// OpButton
	virtual void OnPaint(OpWidgetPainter* widget_painter, const OpRect& paint_rect);
	virtual void OnResize(INT32* new_w, INT32* new_h);

	// OpBitmapScreenListener	
	virtual void OnInvalid();
	virtual void OnScreenDestroying();

private:
	BitmapOpWindow *m_bitmap_window;
};

#endif // BITMAP_BUTTON_H
