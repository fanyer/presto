/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
*/


#include "core/pch.h"

#include "adjunct/quick/speeddial/BitmapButton.h"

#include "modules/display/vis_dev.h"
#include "modules/libgogi/pi_impl/mde_bitmap_window.h"

BitmapButton::BitmapButton()
	: OpButton(TYPE_CUSTOM, STYLE_IMAGE)
	, m_bitmap_window(NULL)
{
}

OP_STATUS BitmapButton::Init(BitmapOpWindow* window)
{
	if (!window || !window->GetScreen())
		return OpStatus::ERR;

	m_bitmap_window = window;
	RETURN_IF_ERROR(m_bitmap_window->GetScreen()->AddListener(this));
	
	return OpStatus::OK;
}

BitmapButton::~BitmapButton()
{
	if (m_bitmap_window)
		m_bitmap_window->GetScreen()->RemoveListener(this);
}

void BitmapButton::OnInvalid()
{
	INT32 left = 0, top = 0, right = 0, bottom = 0;
	GetPadding(&left, &top, &right, &bottom);
	OpRect content_rect(left, top, rect.width - left - right, rect.height - top - bottom);
	Invalidate(content_rect);
}

void BitmapButton::OnScreenDestroying()
{
	if (m_bitmap_window && m_bitmap_window->GetScreen())
	{
		m_bitmap_window->GetScreen()->RemoveListener(this);		
	}
	m_bitmap_window = NULL;
}

void BitmapButton::OnResize(INT32* new_w, INT32* new_h) 
{
	if(m_bitmap_window)
	{
		INT32 left = 0, top = 0, right = 0, bottom = 0;

		GetPadding(&left, &top, &right, &bottom);

		m_bitmap_window->SetSize(*new_w - left - right, *new_h - top - bottom);
	}
}

void BitmapButton::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	if (!m_bitmap_window)
		return OpButton::OnPaint(widget_painter, paint_rect);

	INT32 left = 0, top = 0, right = 0, bottom = 0;

	GetPadding(&left, &top, &right, &bottom);

	OpRect content_rect(left, top, rect.width - left - right, rect.height - top - bottom);

	BitmapMDE_Screen* screen =  m_bitmap_window->GetScreen();
	if (!screen)
		return;

	OpBitmap* bitmap = screen->GetBitmap();
	if(!bitmap)
		return;

	INT32 w,h;
	screen->GetSize(w,h);
	OpRect src_rect(0, 0, w,h);

	vis_dev->BeginClipping(content_rect);
	vis_dev->BitmapOut(bitmap, src_rect,content_rect);
	vis_dev->EndClipping();	
}
