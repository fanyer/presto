/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#include "core/pch.h"

#include "modules/libgogi/pi_impl/mde_dsmode.h"

#ifdef MDE_DS_MODE

#ifndef MDE_DSMODE_SCALE
#define MDE_DSMODE_SCALE 3
#endif // !MDE_DSMODE_SCALE
#ifndef MDE_DSMODE_MAXWIDTH
#define MDE_DSMODE_MAXWIDTH 256
#endif // !MDE_DSMODE_MAXWIDTH
#ifndef MDE_DSMODE_MAXHEIGHT
#define MDE_DSMODE_MAXHEIGHT 192*2
#endif // !MDE_DSMODE_MAXHEIGHT


MDE_DSModeScreen::MDE_DSModeScreen() : dsMode(0)
{}

void MDE_DSModeScreen::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{}

void MDE_DSModeScreen::OnInvalid()
{
	// Make sure the top level screen is invalid so it triggers a repaint
	// Invalidate the whole area when something has changed in opera and is invalid
	dsMode->InvalidateOperaArea(m_rect);
}

MDE_FORMAT MDE_DSModeScreen::GetFormat()
{
	OP_ASSERT(dsMode && dsMode->m_screen);
	return dsMode->m_screen->GetFormat();
}

MDE_BUFFER* MDE_DSModeScreen::LockBuffer()
{
	isOOM = FALSE;
	MDE_InitializeBuffer(m_rect.w, m_rect.h, m_rect.w*MDE_GetBytesPerPixel(GetFormat()),
			GetFormat(), dsMode->overview_buffer, NULL, &screen);
	return &screen;
}

void MDE_DSModeScreen::UnlockBuffer(MDE_Region *update_region)
{
	// FIXME: this does not include the linear scrolling from the Nintendo DS port. 
	// In order to make that code work well enough you also need some hacks to be 
	// able to fake a scrollrect. Talk to timj@opera.com if you need this functionality
}
	
MDE_DSModeView::MDE_DSModeView() : isBird(FALSE), dsScreen(0), grabbed(FALSE)
{}

void MDE_DSModeView::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
	dsScreen->dsMode->Repaint();
	MDE_BUFFER buf;
	MDE_InitializeBuffer(dsScreen->m_rect.w,dsScreen->m_rect.h,
					dsScreen->m_rect.w*MDE_GetBytesPerPixel(dsScreen->GetFormat()), 
					dsScreen->GetFormat(),dsScreen->dsMode->overview_buffer, 
					NULL, &buf);
	buf.method = MDE_METHOD_COPY;
	if (isBird)
	{
		MDE_DrawBufferStretch(&buf, MDE_MakeRect(0,0,m_rect.w,m_rect.h), 
						MDE_MakeRect(0,0,dsScreen->m_rect.w,dsScreen->m_rect.h), 
						screen);

		MDE_RECT sr = MDE_MakeRect((dsScreen->dsMode->frogOffsetX*m_rect.w)/dsScreen->m_rect.w,
			(dsScreen->dsMode->frogOffsetY*m_rect.h)/dsScreen->m_rect.h, 
			// FIXME: the following 2 lines assumes bird and frog view have the same size
			((dsScreen->dsMode->frogOffsetX+m_rect.w)*m_rect.w + dsScreen->m_rect.w-1)/dsScreen->m_rect.w,
			((dsScreen->dsMode->frogOffsetY+m_rect.h)*m_rect.h + dsScreen->m_rect.h-1)/dsScreen->m_rect.h);
		sr.w -= sr.x;
		sr.h -= sr.y;
		// FIXME: Nintendo DS used a hack to be able to fill a rect with translucent color
		//MDE_SetColor(MDE_RGBA(68,137,201,25), screen);
		//MDE_DrawRectFill(sr, screen);
		MDE_SetColor(MDE_RGB(68,137,201), screen);
		MDE_DrawRect(sr, screen);
	}
	else
	{
		MDE_DrawBuffer(&buf, rect, rect.x+dsScreen->dsMode->frogOffsetX, rect.y+dsScreen->dsMode->frogOffsetY, screen);
	}
	// Invalidate the whole area when something has changed and is invalid
	if (dsScreen->m_is_invalid)
		dsScreen->dsMode->InvalidateOperaArea(dsScreen->m_rect);
}

#ifdef MDE_SUPPORT_MOUSE

void MDE_DSModeView::OnMouseDown(int x, int y, int button, int clicks)
{
	lastPos = OpPoint(x,y);
	if (isBird)
	{
		x = (x*dsScreen->m_rect.w)/m_rect.w;
		y = (y*dsScreen->m_rect.h)/m_rect.h;
		grab_x = x - dsScreen->dsMode->frogOffsetX;
		grab_y = y - dsScreen->dsMode->frogOffsetY;
		if (grab_x < 0 || grab_x >= m_rect.w || grab_y < 0 || grab_y >= m_rect.h)
		{
			grab_x = m_rect.w/2;
			grab_y = m_rect.h/2;
		}
	
		grabbed = TRUE;
		dsScreen->dsMode->MoveFrogRect(x-grab_x, y-grab_y);
	}
	else
	{
		dsScreen->TrigMouseMove(x+dsScreen->dsMode->frogOffsetX, y+dsScreen->dsMode->frogOffsetY);
		dsScreen->TrigMouseDown(x+dsScreen->dsMode->frogOffsetX, y+dsScreen->dsMode->frogOffsetY, button, clicks);
	}
}
void MDE_DSModeView::OnMouseUp(int x, int y, int button)
{
	lastPos = OpPoint(x,y);
	grabbed = FALSE;
	if (!isBird)
	{
		dsScreen->TrigMouseMove(x+dsScreen->dsMode->frogOffsetX, y+dsScreen->dsMode->frogOffsetY);
		dsScreen->TrigMouseUp(x+dsScreen->dsMode->frogOffsetX, y+dsScreen->dsMode->frogOffsetY, button);
	}
}
void MDE_DSModeView::OnMouseMove(int x, int y)
{
	lastPos = OpPoint(x,y);
	if (isBird)
	{
		if (grabbed)
		{
			x = (x*dsScreen->m_rect.w)/m_rect.w;
			y = (y*dsScreen->m_rect.h)/m_rect.h;
			dsScreen->dsMode->MoveFrogRect(x-grab_x, y-grab_y);
		}
	}
	else
	{
		dsScreen->TrigMouseMove(x+dsScreen->dsMode->frogOffsetX, y+dsScreen->dsMode->frogOffsetY);
	}
}

bool MDE_DSModeView::OnMouseWheel(int delta)
{
	int x = lastPos.x;
	int y = lastPos.y;
	if (isBird)
	{
		x = (x*dsScreen->m_rect.w)/m_rect.w;
		y = (y*dsScreen->m_rect.h)/m_rect.h;
	}
	else
	{
		x += dsScreen->dsMode->frogOffsetX;
		y += dsScreen->dsMode->frogOffsetY;
	}
	dsScreen->TrigMouseWheel(x, y, delta, true, SHIFTKEY_NONE);
	// FIXME: this assumes opera handled the mouse wheel event
	return true;
}

#endif // MDE_SUPPORT_MOUSE

MDE_DSMode::MDE_DSMode() : overview_buffer(0), frogOffsetX(0), frogOffsetY(0), dsEnabled(FALSE)
{
	dsBirdView.SetBird(TRUE);
	dsFrogView.SetBird(FALSE);
	dsBirdView.SetScreen(&dsScreen);
	dsFrogView.SetScreen(&dsScreen);
	dsScreen.SetDSModeInstance(this);

	AddChild(&dsBirdView);
	AddChild(&dsFrogView);
}

MDE_DSMode::~MDE_DSMode()
{
	for (MDE_View* c = dsScreen.m_first_child; c; c = c->m_next)
		dsScreen.RemoveChild(c);
	if (m_parent)
		m_parent->RemoveChild(this);
	RemoveChild(&dsBirdView);
	RemoveChild(&dsFrogView);
	OP_DELETEA(overview_buffer);
}
	
OP_STATUS MDE_DSMode::EnableDS(BOOL en, MDE_View* win)
{
	if (en == dsEnabled)
		return OpStatus::OK;
	if (en)
	{
		OP_ASSERT(!m_parent);
		SetRect(win->m_rect);
		win->m_parent->AddChild(this, win);
		win->m_parent->RemoveChild(win);
		OP_STATUS err = Resize(m_rect.w, m_rect.h);
		if (OpStatus::IsError(err))
		{
			m_parent->AddChild(win, this);
			m_parent->RemoveChild(this);
			return err;
		}
		win->SetRect(dsScreen.m_rect);
		dsScreen.AddChild(win);
	}
	else
	{
		OP_ASSERT(m_parent);
		win->SetRect(m_rect);
		dsScreen.RemoveChild(win);
		m_parent->AddChild(win, this);
		m_parent->RemoveChild(this);
		// by adding these two lines the buffer will be freed when leaving ds mode
		//delete[] overview_buffer;
		//overview_buffer = 0;
	}
	dsEnabled = en;
	return OpStatus::OK;
}
OP_STATUS MDE_DSMode::Resize(int w, int h)
{
	// Limit width and height to avoid allocating huge buffers
	if (w > MDE_DSMODE_MAXWIDTH)
		w = MDE_DSMODE_MAXWIDTH;
	if (h > MDE_DSMODE_MAXHEIGHT)
		h = MDE_DSMODE_MAXHEIGHT;
	int border = 2;
	if (h&1)
		++border;
	h -= border;
	if (w*MDE_DSMODE_SCALE != dsScreen.m_rect.w || (h/2)*MDE_DSMODE_SCALE != dsScreen.m_rect.h || !overview_buffer)
	{
		int num_bytes = w*MDE_DSMODE_SCALE*(h/2)*MDE_DSMODE_SCALE*MDE_GetBytesPerPixel(dsScreen.GetFormat());
		unsigned char* nb = OP_NEWA(unsigned char, num_bytes);
		if (!nb)
			return OpStatus::ERR_NO_MEMORY;
		OP_DELETEA(overview_buffer);
		overview_buffer = nb;
		dsScreen.Init(w*MDE_DSMODE_SCALE, (h/2)*MDE_DSMODE_SCALE);
		dsScreen.SetRect(MDE_MakeRect(0,0,w*MDE_DSMODE_SCALE, (h/2)*MDE_DSMODE_SCALE));
	}
	dsFrogView.SetRect(MDE_MakeRect(0,0,w, h/2));
	dsBirdView.SetRect(MDE_MakeRect(0,h/2+border,w, h/2));
	for (MDE_View* c = dsScreen.m_first_child; c; c = c->m_next)
		c->SetRect(dsScreen.m_rect);
	return OpStatus::OK;
}

void MDE_DSMode::SwapViews()
{
	if (dsBirdView.m_rect.y < 192)
	{
		dsFrogView.SetRect(MDE_MakeRect(0,0,m_rect.w, m_rect.h/2));
		dsBirdView.SetRect(MDE_MakeRect(0,m_rect.h/2,m_rect.w, m_rect.h/2));
	}
	else
	{
		dsBirdView.SetRect(MDE_MakeRect(0,0,m_rect.w, m_rect.h/2));
		dsFrogView.SetRect(MDE_MakeRect(0,m_rect.h/2,m_rect.w, m_rect.h/2));
	}
}
	
void MDE_DSMode::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
	MDE_SetColor(MDE_RGB(0,0,0), screen);
	MDE_DrawRectFill(rect, screen);	
}
	
void MDE_DSMode::Repaint()
{
	if (dsEnabled)
	{
		// FIXME: oom paint code is not merged fomr ds
//		MDE_RECT invalid;
//		MDE_RectReset(invalid);
//		dsScreen.MaxInvalidRect(invalid);
		dsScreen.Validate(true);
/*		if (dsScreen.IsOOM())
		{
			dsScreen.Invalidate(invalid);
			dsScreen.ValidateOOM(true);
		}*/
	}
}
	
void MDE_DSMode::InvalidateOperaArea(const MDE_RECT &r)
{
	MDE_RECT br = MDE_MakeRect((r.x*dsBirdView.m_rect.w)/dsScreen.m_rect.w, 
			(r.y*dsBirdView.m_rect.h)/dsScreen.m_rect.h,
			((r.x+r.w)*dsBirdView.m_rect.w + (dsScreen.m_rect.w-1))/dsScreen.m_rect.w,
			((r.y+r.h)*dsBirdView.m_rect.h + (dsScreen.m_rect.h-1))/dsScreen.m_rect.h);
	br.w -= br.x;
	br.h -= br.y;
	dsBirdView.Invalidate(br);
	dsFrogView.Invalidate(MDE_MakeRect(r.x-frogOffsetX, r.y-frogOffsetY, r.w,r.h));
}
void MDE_DSMode::MoveFrogRect(int x, int y)
{
	InvalidateOperaArea(MDE_MakeRect(frogOffsetX, frogOffsetY, dsFrogView.m_rect.w,dsFrogView.m_rect.h));

	frogOffsetX = x;
	frogOffsetY = y;

	if (frogOffsetX < 0)
		frogOffsetX = 0;
	if (frogOffsetY < 0)
		frogOffsetY = 0;
	if (frogOffsetX + dsFrogView.m_rect.w > dsScreen.m_rect.w)
		frogOffsetX = dsScreen.m_rect.w-dsFrogView.m_rect.w;
	if (frogOffsetY + dsFrogView.m_rect.h > dsScreen.m_rect.h)
		frogOffsetY = dsScreen.m_rect.h-dsFrogView.m_rect.h;

	InvalidateOperaArea(MDE_MakeRect(frogOffsetX, frogOffsetY, dsFrogView.m_rect.w,dsFrogView.m_rect.h));
}

BOOL MDE_DSMode::GetInvalid()
{
	return dsEnabled && dsScreen.m_is_invalid;	
}
	
#endif // MDE_DS_MODE

