/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#include "core/pch.h"

#include "modules/libgogi/pi_impl/mde_generic_screen.h"

#if defined(PIXEL_SCALE_RENDERING_SUPPORT) && defined(VEGA_OPPAINTER_SUPPORT)
#include "modules/display/pixelscalepainter.h"
#endif // PIXEL_SCALE_RENDERING_SUPPORT && VEGA_OPPAINTER_SUPPORT

#ifdef SCREEN_WATERMARK
#ifdef VEGA_OPPAINTER_SUPPORT
#include "modules/pi/OpBitmap.h"
#include "modules/display/FontAtt.h"
#include "modules/display/styl_man.h"
#endif	//VEGA_OPPAINTER_SUPPORT
#endif	//SCREEN_WATERMARK




#ifdef MDE_CURSOR_SUPPORT

#ifdef VEGA_OPPAINTER_SUPPORT
#include "modules/libvega/src/oppainter/vegaoppainter.h"
#endif // VEGA_OPPAINTER_SUPPORT



#ifdef SCREEN_WATERMARK
#ifdef VEGA_OPPAINTER_SUPPORT
class MDE_WaterMark: public MDE_View
{
public:
    MDE_WaterMark() : MDE_View() ,
		m_bitmap(NULL)
	{
        SetTransparent(TRUE);
	}   


	virtual ~MDE_WaterMark()
	{
		OP_DELETE(m_bitmap);
	}


	virtual bool GetHitStatus(int x, int y)
	{
		return false;
	}


	OpBitmap* PrepareWatermarkBitmap()
	{
		if (m_bitmap)
			return m_bitmap;



		// Caculate rect size of watermark
		const int screen_width = m_screen->GetWidth();
		const int screen_height = m_screen->GetHeight();
		int font_height = 10;
		int padding_x = 10;
		int string_width = 0;
		OpFont* font = NULL;
		VEGAOpPainter *painter = NULL;
		uni_char* str = NULL;

		MDE_RECT watermark_rect;
		MDE_RectReset(watermark_rect);

		// rect height 
		watermark_rect.h = MIN(70, screen_height);
		if (watermark_rect.h == 70)
		{
			font_height = 50;
		}else
		{
			font_height = MAX(8, (watermark_rect.h - 2));
		}

		// string width
		FontAtt fontatt;
		painter = m_screen->GetVegaPainter();
		fontatt.SetHeight(font_height);
		int fontid = styleManager->GetGenericFontNumber(StyleManager::SERIF, WritingSystem::LatinWestern);
		fontatt.SetFontNumber(fontid);
		font = g_font_cache->GetFont(fontatt, 100);

		// try entire string
		str = const_cast<uni_char*>(UNI_L("Opera SDK Evaluation"));
		string_width = font->StringWidth(str, uni_strlen(str), painter);


		// try shorter string
		if (string_width > screen_width)
		{
			str = const_cast<uni_char*>(UNI_L("Opera SDK"));
			string_width = font->StringWidth(str, uni_strlen(str), painter);
		}


		if (string_width > screen_width)
		{
			str = const_cast<uni_char*>(UNI_L("Opera"));
			string_width = font->StringWidth(str, uni_strlen(str), painter);
		}

		if (string_width > screen_width)
		{
			str = const_cast<uni_char*>(UNI_L("O"));
			string_width = font->StringWidth(str, uni_strlen(str), painter);
		}


		//width of watermark
		if (string_width + 20 <= screen_width)
		{
			watermark_rect.w = string_width +20;
			padding_x = 10;
		}else
		{
			watermark_rect.w = MIN(string_width, screen_width);	//No need to set padding here
		}

		watermark_rect.x = (screen_width - watermark_rect.w)/2;
		watermark_rect.y = (screen_height - watermark_rect.h)/2;
		SetRect(watermark_rect);


		//create bitmap
		if (OpStatus::IsError(OpBitmap::Create(&m_bitmap, watermark_rect.w, watermark_rect.h, FALSE, TRUE, 0, 0, TRUE)))
		{
			g_font_cache->ReleaseFont(font);
			return NULL;
		}

		//clear background of bitmap
		painter =(VEGAOpPainter*)m_bitmap->GetPainter();
		painter->SetColor(0, 0, 0, 0);
		painter->ClearRect(OpRect(0, 0, watermark_rect.w, watermark_rect.h));

		//draw background
		painter->SetColor(0xFF, 0x0F, 0x0F, 128);
		painter->FillRect(OpRect(0, 0, watermark_rect.w, watermark_rect.h));

		//draw string
		painter->SetFont(font);
		painter->SetColor(0x00, 0xFF, 0xFF, 128);
		painter->DrawString(OpPoint(padding_x, 0), str, uni_strlen(str), 0, -1);

		//release resource
		g_font_cache->ReleaseFont(font);
		m_bitmap->ReleasePainter();
		
		return m_bitmap;

	}

	// will be callded when screen size changes,
	// We don't call PrepareWatermarkBitmap here, 
	// Because of StyleManager is not initialied when OnScrrenReszie is called first time.
	void OnScreenResize(int screen_width, int screen_height)
	{


		if (NULL == m_screen)
			return;

		OP_DELETE(m_bitmap);
		m_bitmap = NULL;

		//Make sure OnPaint() will be called when screen size changes to smaller and orignal rect goes out of screen 
		SetRect(MDE_MakeRect(0, 0, screen_width, screen_height));

		return;
	}



    virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
    {   

		int need_redraw_all = 0;

		if (NULL == m_bitmap && PrepareWatermarkBitmap())
		{
			need_redraw_all = 1;
		}

		if (NULL == m_bitmap)
		{
			return;
		}


		VEGAOpPainter *pt = m_screen->GetVegaPainter();

		int x = 0;
		int y = 0;
		ConvertToScreen(x, y);
	 	pt->SetVegaTranslation(x, y);

		OpRect srect(0, 0, m_rect.w, m_rect.h);

		if (!need_redraw_all)
		{
			srect.IntersectWith(OpRect(rect.x, rect.y, rect.w, rect.h));
		}


		pt->DrawBitmapClipped(m_bitmap, srect, srect.TopLeft());

		return;

	}


private:
	OpBitmap* m_bitmap;

};
#endif	//VEGA_OPPAINTER_SUPPORT
#endif	//SCREEN_WATERMARK





class CursorSprite : public MDE_Sprite
{
public:
	BOOL Init(MDE_Screen *screen, int width, int height);
	BOOL ChangeData(int hot_x, int hot_y, int width, int height, int stride, MDE_FORMAT format, void* data, unsigned char *pal, MDE_METHOD method);
	void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);
#ifdef VEGA_OPPAINTER_SUPPORT
	virtual ~CursorSprite();
	VEGASprite* m_sprite;
	MDE_GenericScreen* m_screen;
#else // !VEGA_OPPAINTER_SUPPORT
private:
	MDE_BUFFER m_cursor_buf;
#endif // VEGA_OPPAINTER_SUPPORT
};

#ifdef VEGA_OPPAINTER_SUPPORT
CursorSprite::~CursorSprite()
{
	if (m_sprite && m_screen && m_screen->GetVegaPainter())
		m_screen->GetVegaPainter()->DestroySprite(m_sprite);
}

BOOL CursorSprite::Init(MDE_Screen *screen, int width, int height)
{
	m_screen = (MDE_GenericScreen*)screen;
	m_sprite = NULL;
	m_rect.w = width;
	m_rect.h = height;
	return TRUE;
}

BOOL CursorSprite::ChangeData(int hot_x, int hot_y, int width, int height, int stride, MDE_FORMAT format, void* data, unsigned char *pal, MDE_METHOD method)
{
	if (m_rect.w == width && m_rect.h == height)
	{
		SetHotspot(hot_x, hot_y);
		if (!m_sprite)
		{
			if (!m_screen || !m_screen->GetVegaPainter())
			{
				if (!m_screen || OpStatus::IsError(m_screen->CreateVegaPainter()))
				{
					return FALSE;
				}
			}
			m_sprite = m_screen->GetVegaPainter()->CreateSprite(width, height);
		}
		OP_ASSERT(format == MDE_FORMAT_BGRA32);
		m_screen->GetVegaPainter()->SetSpriteData(m_sprite, (UINT32*)data, stride);
		Invalidate();
		return TRUE;
	}
	return FALSE;
}

void CursorSprite::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
}
#else // !VEGA_OPPAINTER_SUPPORT
BOOL CursorSprite::Init(MDE_Screen *screen, int width, int height)
{
	MDE_InitializeBuffer(0, 0, 0, (MDE_FORMAT) 0, NULL, NULL, &m_cursor_buf);
	return MDE_Sprite::Init(width, height, screen);
}

BOOL CursorSprite::ChangeData(int hot_x, int hot_y, int width, int height, int stride, MDE_FORMAT format, void* data, unsigned char *pal, MDE_METHOD method)
{
	if (m_rect.w == width && m_rect.h == height)
	{
		SetHotspot(hot_x, hot_y);
		MDE_InitializeBuffer(width, height, stride, format, data, pal, &m_cursor_buf);
		m_cursor_buf.method = method;
		Invalidate();
		return TRUE;
	}
	return FALSE;
}

void CursorSprite::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
	if (m_cursor_buf.data)
	{
		MDE_DrawBuffer(&m_cursor_buf, MDE_MakeRect(0, 0, m_rect.w, m_rect.h), 0, 0, screen);
	}
	else
	{
		// Draw ugly testcursor
		MDE_SetColor(MDE_RGB(205, 0, 0), screen);
		MDE_DrawEllipseFill(MDE_MakeRect(0, 0, m_rect.w, m_rect.h), screen);
		MDE_SetColor(MDE_RGB(128, 0, 0), screen);
		MDE_DrawEllipse(MDE_MakeRect(0, 0, m_rect.w, m_rect.h), screen);
	}
}
#endif // !VEGA_OPPAINTER_SUPPORT

#endif // MDE_CURSOR_SUPPORT

MDE_GenericScreen::MDE_GenericScreen(int bufferWidth, int bufferHeight, int bufferStride, MDE_FORMAT bufferFormat, void *bufferData)
	: m_link(this)
{
#ifdef SCREEN_WATERMARK
#ifdef VEGA_OPPAINTER_SUPPORT
	waterMark = NULL;
#endif	//VEGA_OPPAINTER_SUPPORT
#endif	//SCREEN_WATERMARK
	this->bufferWidth = bufferWidth;
	this->bufferHeight = bufferHeight;
	this->bufferFormat = bufferFormat;
	this->bufferData = bufferData;
	this->bufferStride = bufferStride;

	user_data = NULL;

	islocked = FALSE;
	clearbackground = TRUE;
	m_rect.w = bufferWidth;
	m_rect.h = bufferHeight;

	paint_oom = FALSE;

	cursor = CURSOR_DEFAULT_ARROW;
#ifdef MDE_CURSOR_SUPPORT
	cursor_sprite = NULL;
#endif

#ifdef VEGA_OPPAINTER_SUPPORT
	painter = NULL;
#endif // VEGA_OPPAINTER_SUPPORT

	m_link.Into(&(g_opera->libgogi_module.m_list_of_screens));
}

OP_STATUS MDE_GenericScreen::Init()
{
	MDE_Screen::Init(bufferWidth, bufferHeight);

	MDE_RECT mr;
	mr.x = 0;
	mr.y = 0;
	mr.w = bufferWidth;
	mr.h = bufferHeight;
	operaView.SetRect(mr);
	AddChild(&operaView);

	operaView.SetTransparent(UseTransparentBackground());

#ifdef SCREEN_WATERMARK
#ifdef VEGA_OPPAINTER_SUPPORT
	OP_STATUS ret = InitWaterMark();
	if(ret != OpStatus::OK)
		return ret;

	ShowWaterMark();
#endif	//VEGA_OPPAINTER_SUPPORT
#endif	//SCREEN_WATERMARK

	return OpStatus::OK;
}

#ifdef SCREEN_WATERMARK
#ifdef VEGA_OPPAINTER_SUPPORT
OP_STATUS MDE_GenericScreen::InitWaterMark()
{

	MDE_RECT rect = GetRect();

	waterMark = OP_NEW(MDE_WaterMark, ());

	if(!waterMark)
		return OpStatus::ERR_NO_MEMORY;
	


	AddChild(waterMark);

	return OpStatus::OK;

}

void MDE_GenericScreen::HideWaterMark()
{
	if (waterMark)
	{
		waterMark->SetVisibility(FALSE);
		RemoveChild(waterMark);
		OP_DELETE(waterMark);
		waterMark = NULL;
	}

}


void MDE_GenericScreen::ShowWaterMark()
{
	if (!waterMark && OpStatus::OK != InitWaterMark())
	{
		return;
	}

	waterMark->SetVisibility(TRUE);


	MDE_RECT r;

	r.x = 50;
	r.y = (m_rect.h - 70)/2;

	r.w = m_rect.w - 50 - 60;
	r.h = 70;

	waterMark->SetRect(r);

	waterMark->SetZ(MDE_Z_TOP);
}

#endif	//VEGA_OPPAINTER_SUPPORT
#endif	//SCREEN_WATERMARK




MDE_GenericScreen::~MDE_GenericScreen()
{
	m_link.Out();

#ifdef MDE_CURSOR_SUPPORT
	if (cursor_sprite)
	{
		RemoveSprite(cursor_sprite);
		OP_DELETE(cursor_sprite);
	}
#endif // MDE_CURSOR_SUPPORT
	RemoveChild(&operaView);
#ifdef VEGA_OPPAINTER_SUPPORT
	OP_DELETE(painter);
#endif // VEGA_OPPAINTER_SUPPORT
}

void MDE_GenericScreen::SetCursor(OpWindow* window, CursorType cursor)
{
	if (this->cursor == cursor)
		return;
	this->cursor = cursor;
	OnCursorChanged(window, cursor);
}

#ifdef MDE_CURSOR_SUPPORT

OP_STATUS MDE_GenericScreen::ShowCursor(BOOL show)
{
	if (show)
	{
		if (!cursor_sprite)
			RETURN_IF_ERROR(SetCursorData(2, 2, 5, 5, 5, (MDE_FORMAT)0, NULL, NULL, (MDE_METHOD)0));
		AddSprite(cursor_sprite);
	}
	else if (cursor_sprite)
	{
		RemoveSprite(cursor_sprite);
	}
	return OpStatus::OK;
}

OP_STATUS MDE_GenericScreen::SetCursorData(int hot_x, int hot_y, int width, int height, int stride, MDE_FORMAT format, void* data, unsigned char *pal, MDE_METHOD method)
{
	if (!cursor_sprite)
	{
		cursor_sprite = OP_NEW(CursorSprite, ());
		if (!cursor_sprite || !((CursorSprite*)cursor_sprite)->Init(this, width, height))
		{
			OP_DELETE(cursor_sprite);
			cursor_sprite = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	if (!((CursorSprite*)cursor_sprite)->ChangeData(hot_x, hot_y, width, height, stride, format, data, pal, method))
		return OpStatus::ERR;
	return OpStatus::OK;
}

#endif // MDE_CURSOR_SUPPORT

OP_STATUS MDE_GenericScreen::Resize(int bufferWidth, int bufferHeight, int bufferStride, void *bufferData)
{
	// If this happens your code must be changed!
	// You should not resize the screen while it's being painted
	OP_ASSERT(!islocked);

#ifdef VEGA_OPPAINTER_SUPPORT
	if (painter)
		painter->Resize(bufferWidth, bufferHeight);
#endif // VEGA_OPPAINTER_SUPPORT


#ifdef SCREEN_WATERMARK
#ifdef VEGA_OPPAINTER_SUPPORT
	if (waterMark) 
		waterMark->OnScreenResize(bufferWidth, bufferHeight);
#endif	//VEGA_OPPAINTER_SUPPORT
#endif	//SCREEN_WATERMARK

	// Delta change
	int dw = bufferWidth - this->bufferWidth;
	int dh = bufferHeight - this->bufferHeight;

	this->bufferWidth = bufferWidth;
	this->bufferHeight = bufferHeight;
	this->bufferStride = bufferStride;
	this->bufferData = bufferData;
	m_rect.w = bufferWidth;
	m_rect.h = bufferHeight;

	// Platform code likes to call Resize even when the size doesn't change.
	// Some platforms also modify the rect of operaView so we should change it
	// relatively to it's current size, not enforce an absolute rect since that
	// would cause excessive invalidations.
	MDE_RECT mr = operaView.m_rect;
	operaView.SetRect(MDE_MakeRect(mr.x, mr.y, mr.w + dw, mr.h + dh));
	return OpStatus::OK;
}

void MDE_GenericScreen::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
#ifndef MDE_NO_MEM_PAINT
	MDE_SetColor(MDE_RGBA(0,0,0,0), screen);
	MDE_DrawRectFill(rect, screen, false);
#endif // !MDE_NO_MEM_PAINT
}

void MDE_GenericScreen::OutOfMemory()
{
	paint_oom = TRUE;
}

MDE_BUFFER* MDE_GenericScreen::LockBuffer()
{
	islocked = TRUE;
	paint_oom = FALSE;
	MDE_InitializeBuffer(bufferWidth, bufferHeight, bufferStride,
						 bufferFormat, bufferData, NULL, &screen);
#ifdef VEGA_OPPAINTER_SUPPORT
	screen.user_data = (void*)painter;
#else
	screen.user_data = user_data;
#endif // VEGA_OPPAINTER_SUPPORT
	if (clearbackground == FALSE) {
		// Buffers with alpha need to set METHOD_ALPHA to blit alpha on alpha correct
		if ((bufferFormat == MDE_FORMAT_BGRA32) ||
			(bufferFormat == MDE_FORMAT_RGBA16) ||
			(bufferFormat == MDE_FORMAT_BGRA32) ||
#ifdef MDE_SUPPORT_ARGB32             
			(bufferFormat == MDE_FORMAT_ARGB32) ||                
#endif
			(bufferFormat == MDE_FORMAT_RGBA32)) {
			screen.method = MDE_METHOD_ALPHA;
		}
	}
	return &screen;
}

#ifdef MDE_NATIVE_WINDOW_SUPPORT
#include "modules/libgogi/pi_impl/mde_native_window.h"
#endif // MDE_NATIVE_WINDOW_SUPPORT
void MDE_GenericScreen::UnlockBuffer(MDE_Region *update_region)
{
	islocked = FALSE;

	// FIXME: else, some kind of error?
#if defined(VEGA_OPPAINTER_SUPPORT)

	if (painter && update_region->num_rects > 0)
	{
		// Use stack allocation unless we go above 'heap_stack_crossover'
		// rectangles

		const int heap_stack_crossover = 16;
		const BOOL use_heap_alloc = (update_region->num_rects > heap_stack_crossover);

		OpRect update_rects_stack[heap_stack_crossover];

		OpRect *update_rects = use_heap_alloc ?
			OP_NEWA(OpRect, update_region->num_rects) :
			update_rects_stack;

		// In case of OOM, simply do not paint the rects
		if (update_rects)
		{
			for (int i = 0; i < update_region->num_rects; ++i)
			{
				const MDE_RECT mde_rect = update_region->rects[i];
				update_rects[i].Set(mde_rect.x, mde_rect.y, mde_rect.w, mde_rect.h);
			}
			painter->present(update_rects, update_region->num_rects);

			if (use_heap_alloc)
				OP_DELETEA(update_rects);
		}
	}

#endif // VEGA_OPPAINTER_SUPPORT
#ifdef MDE_NATIVE_WINDOW_SUPPORT
	MDENativeWindow::UpdateRegionsForScreen(this);
#endif // MDE_NATIVE_WINDOW_SUPPORT
}


#ifdef DEBUG_FRAMEBUFFER_ALPHA

static inline void SetPixel_BGRA32( unsigned char *dst8, unsigned char *src8, unsigned char srca )
{
	if (srca == 0)
		return;
	dst8[MDE_COL_OFS_B] = dst8[MDE_COL_OFS_B] + (srca * (src8[MDE_COL_OFS_B] - dst8[MDE_COL_OFS_B]) >> 8);
	dst8[MDE_COL_OFS_G] = dst8[MDE_COL_OFS_G] + (srca * (src8[MDE_COL_OFS_G] - dst8[MDE_COL_OFS_G]) >> 8);
	dst8[MDE_COL_OFS_R] = dst8[MDE_COL_OFS_R] + (srca * (src8[MDE_COL_OFS_R] - dst8[MDE_COL_OFS_R]) >> 8);
}

void MDE_GenericScreen::OnRectPainted(const MDE_RECT &rect)
{
	if (bufferFormat != MDE_FORMAT_BGRA32)
		return;
	UINT8 *data8 = (UINT8 *) screen.data;
	int x, y;
	for(y = 0; y < rect.h; y++)
	{
		UINT8 *row8 = &data8[rect.x * screen.ps + (y + rect.y) * screen.stride];
		for(x = 0; x < rect.w; x++)
		{
			UINT32 dstcol = MDE_RGB(255, 70, 70);
			SetPixel_BGRA32((unsigned char *)&dstcol, row8, row8[MDE_COL_OFS_A]);
			*((UINT32*)row8) = dstcol;
			row8 += 4;
		}
	}
}
#endif // DEBUG_FRAMEBUFFER_ALPHA

#ifdef VEGA_OPPAINTER_SUPPORT

OP_STATUS MDE_GenericScreen::CreateVegaPainter()
{
	if (!painter)
	{
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		PixelScalePainter* oppainter = OP_NEW(PixelScalePainter, ());
#else
		VEGAOpPainter* oppainter = OP_NEW(VEGAOpPainter, ());
#endif // PIXEL_SCALE_RENDERING_SUPPORT

		if (!oppainter)
			return OpStatus::ERR_NO_MEMORY;

		OP_STATUS err = oppainter->Construct(bufferWidth, bufferHeight, (VEGAWindow*)bufferData);
		if (OpStatus::IsError(err))
		{
			OP_DELETE(oppainter);
			oppainter = NULL;
			return err;
		}

		painter = oppainter;
	}
	return OpStatus::OK;
}

#ifdef MDE_CURSOR_SUPPORT
void MDE_GenericScreen::PaintSprite(MDE_BUFFER* screen, MDE_Sprite* spr)
{
	OP_ASSERT(!spr->m_view);
	painter->AddSprite(((CursorSprite*)spr)->m_sprite, spr->m_rect.x, spr->m_rect.y);
}

void MDE_GenericScreen::UnpaintSprite(MDE_BUFFER* screen, MDE_Sprite* spr)
{
	OP_ASSERT(!spr->m_view);
	painter->RemoveSprite(((CursorSprite*)spr)->m_sprite);
}
#endif // MDE_CURSOR_SUPPORT

void MDE_GenericScreen::DrawDebugRect(const MDE_RECT &rect, unsigned int col, MDE_BUFFER *dstbuf)
{
	VEGAOpPainter* painter = GetVegaPainter();
	painter->SetVegaTranslation(dstbuf->ofs_x, dstbuf->ofs_y);
	painter->SetColor(MDE_COL_R(col), MDE_COL_G(col), MDE_COL_B(col), MDE_COL_A(col));
	painter->DrawRect(OpRect(rect.x, rect.y, rect.w, rect.h), 1);
}

#endif // VEGA_OPPAINTER_SUPPORT
