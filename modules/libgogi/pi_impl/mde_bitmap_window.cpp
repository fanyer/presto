/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#if defined(MDE_BITMAP_WINDOW_SUPPORT)

#include "modules/libgogi/pi_impl/mde_bitmap_window.h"

#include "modules/libgogi/mde.h"
#include "modules/pi/OpBitmap.h"

BitmapMDE_Screen::BitmapMDE_Screen():m_bitmap(NULL)
{
}

OP_STATUS BitmapMDE_Screen::Init(int bufferWidth, int bufferHeight)
{
	RETURN_IF_ERROR(OpBitmap::Create(&m_bitmap,bufferWidth,bufferHeight,TRUE,TRUE,0U,FALSE,TRUE)); // support painter
	MDE_Screen::Init(bufferWidth,bufferHeight);
	return OpStatus::OK;
}

MDE_BUFFER* BitmapMDE_Screen::LockBuffer()
{
	OP_ASSERT(m_buffer.user_data == NULL);
	INT32 w,h;
	GetSize(w,h);
#ifdef VEGA_OPPAINTER_SUPPORT
	void * pixeldata = NULL;
#else
	/* Since this method can not report failure, an assert will have
	 * to suffice.
	 */
	OP_ASSERT(m_bitmap->Supports(OpBitmap::SUPPORTS_POINTER));
	void * pixeldata = m_bitmap->GetPointer();
#endif
	MDE_InitializeBuffer(w, h, w*MDE_GetBytesPerPixel(GetFormat()), GetFormat(), pixeldata, NULL, &m_buffer);
	m_buffer.method = MDE_METHOD_COPY;
#ifdef VEGA_OPPAINTER_SUPPORT
	/* Since this method can not report failure, an assert will have
	 * to suffice.
	 */
	OP_ASSERT(m_bitmap->Supports(OpBitmap::SUPPORTS_PAINTER));
	m_buffer.user_data = m_bitmap->GetPainter();
#endif
	return &m_buffer;
}
	
void BitmapMDE_Screen::UnlockBuffer(MDE_Region *update_region)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	m_bitmap->ReleasePainter();
#else
	m_bitmap->ReleasePointer();
#endif
}

VEGAOpPainter* BitmapMDE_Screen::GetVegaPainter()
{
	return (VEGAOpPainter*)m_bitmap->GetPainter();
}
	
BitmapMDE_Screen::~BitmapMDE_Screen()
{
	BroadcastOnDestroying();
	OP_DELETE(m_bitmap);
}
	
OpBitmap* BitmapMDE_Screen::GetBitmap()
{
	Validate(TRUE);		
	return m_bitmap;
}

void BitmapMDE_Screen::OnInvalid()
{
	BroadcastOnInvalid();
}

void  BitmapMDE_Screen::GetRenderingBufferSize(UINT32* width, UINT32* height)
{	
	 *width= GetWidth();
	 *height= GetHeight();
}
		

void BitmapMDE_Screen::GetSize(INT32& width, INT32& height)
{
	width = GetWidth();
	height = GetHeight();
}

void BitmapMDE_Screen::SetSize(INT32 new_w, INT32 new_h)
{
	MDE_RECT rect = { 0, 0, new_w, new_h };
	SetRect(rect);
}

void BitmapMDE_Screen::BroadcastOnInvalid()
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnInvalid();
	}
}


void BitmapMDE_Screen::BroadcastOnDestroying()
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnScreenDestroying();
	}
}

////////////////////////////////////////////////////////////


OP_STATUS BitmapOpWindow::Init(int width, int height)
{
	m_screen = OP_NEW(BitmapMDE_Screen, ());
	RETURN_OOM_IF_NULL(m_screen);

	OP_STATUS status = m_screen->Init(width,height);
	if (OpStatus::IsError(status))	
	{
		OP_DELETE(m_screen);
		m_screen = NULL;
		return status;
	}

	SetAllowAsActive(FALSE);

	return MDE_OpWindow::Init(OpWindow::STYLE_BITMAP_WINDOW,
				OpTypedObject::WINDOW_TYPE_DESKTOP,
				NULL,					// parent window
				NULL,					// parent view
				GetScreen());	// native handler
}

BitmapOpWindow::BitmapOpWindow():m_screen(NULL)
{
}

BitmapOpWindow::~BitmapOpWindow()
{
	OP_DELETE(m_screen);
	mdeWidget = NULL; // deleted with the screen. 
}

void BitmapOpWindow::GetRenderingBufferSize(UINT32* width, UINT32* height)
{
	m_screen->GetRenderingBufferSize(width,height);		
}

void BitmapOpWindow::OnGetInnerSize (OpWindowCommander* commander, UINT32* width, UINT32* height)
{
	m_screen->GetRenderingBufferSize(width,height);
}

void BitmapOpWindow::OnGetOuterSize(OpWindowCommander* commander, UINT32* width, UINT32* height)
{
	m_screen->GetRenderingBufferSize(width,height);
}


void BitmapOpWindow::SetSize(INT32 new_w, INT32 new_h)
{
	m_screen->SetSize(new_w,new_h);
	SetInnerSize(new_w,new_h);
}

#endif //MDE_BITMAP_WINDOW_SUPPORT 
