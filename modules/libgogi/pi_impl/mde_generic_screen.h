/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#ifndef MDE_GENERIC_SCREEN_H
#define MDE_GENERIC_SCREEN_H

#include "modules/pi/OpWindow.h"
#include "modules/libgogi/mde.h"
#ifdef VEGA_OPPAINTER_SUPPORT
#include "modules/libvega/src/oppainter/vegaoppainter.h"
#endif




#ifdef SCREEN_WATERMARK
#ifdef VEGA_OPPAINTER_SUPPORT
class MDE_WaterMark;
#endif	//VEGA_OPPAINTER_SUPPORT
#endif	//SCREEN_WATERMARK

class MDE_GenericScreen : public MDE_Screen, public Link
{
public:
	MDE_GenericScreen(int bufferWidth, int bufferHeight, int bufferStride, MDE_FORMAT bufferFormat, void *bufferData);
	virtual ~MDE_GenericScreen();



#ifdef SCREEN_WATERMARK
#ifdef VEGA_OPPAINTER_SUPPORT
	/* current watermark implementation only support VEGA backend */
	OP_STATUS InitWaterMark();
	void ShowWaterMark();
	void HideWaterMark();
#endif	//VEGA_OPPAINTER_SUPPORT
#endif	//SCREEN_WATERMARK


	virtual OP_STATUS Init();

	virtual bool IsType(const char* type) { return op_strcmp(type, "MDE_GENERICSCREEN") == 0 ? true : MDE_Screen::IsType(type); }

	virtual OP_STATUS Resize(int bufferWidth, int bufferHeight, int bufferStride, void *bufferData);

	virtual MDE_FORMAT GetFormat()		{ return bufferFormat; }
	int GetStride()						{ return bufferStride; }
	BOOL IsLocked()						{ return islocked; }
	BOOL IsPaintOOM()					{ return paint_oom; }
	MDE_RECT GetRect()					{ return MDE_MakeRect(0, 0, bufferWidth, bufferHeight); }
	MDE_View *GetOperaView()			{ return &operaView; }

	virtual MDE_BUFFER *LockBuffer();
	virtual void UnlockBuffer(MDE_Region *update_region);

#ifndef VEGA_OPPAINTER_SUPPORT
	/** Set the (optional) data for anything, that will be assigned to the MDE_BUFFER (screen->user_data) during paint. */
	void SetUserData(void *user_data)	{ this->user_data = user_data; }
#else
	virtual OP_STATUS CreateVegaPainter();
	virtual VEGAOpPainter* GetVegaPainter()		{ return painter; }
	virtual void DrawDebugRect(const MDE_RECT &rect, unsigned int col, MDE_BUFFER *dstbuf);

#ifdef MDE_CURSOR_SUPPORT
	void PaintSprite(MDE_BUFFER* screen, MDE_Sprite* spr);
	void UnpaintSprite(MDE_BUFFER* screen, MDE_Sprite* spr);
#endif // MDE_CURSOR_SUPPORT
#endif

	virtual void OutOfMemory();
	virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);
#ifdef DEBUG_FRAMEBUFFER_ALPHA
	virtual void OnRectPainted(const MDE_RECT &rect);
#endif

	void SetCursor(OpWindow* window, CursorType cursor);
	virtual void OnCursorChanged(OpWindow* window, CursorType cursor) {}

#ifdef MDE_CURSOR_SUPPORT
	OP_STATUS SetCursorData(int hot_x, int hot_y, int width, int height, int stride, MDE_FORMAT format, void* data, unsigned char *pal, MDE_METHOD method);
	OP_STATUS ShowCursor(BOOL show);
	void SetCursorPos(int x, int y)		{ if (cursor_sprite) cursor_sprite->SetPos(x, y); }
#endif
	void SetClearBackground(BOOL value) {clearbackground = value;}
	
private:
	MDE_BUFFER screen;
	int bufferWidth, bufferHeight, bufferStride;
	MDE_FORMAT bufferFormat;
	void *bufferData;
	
	CursorType cursor;
#ifdef MDE_CURSOR_SUPPORT
	class MDE_Sprite *cursor_sprite;
#endif

#ifdef MDE_NO_MEM_PAINT
	class OperaView : public MDE_View{
	public:
		void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen){}
	} operaView;
#else
	MDE_View operaView;
#endif

	BOOL paint_oom;
	BOOL islocked;
	BOOL clearbackground;

#ifdef VEGA_OPPAINTER_SUPPORT
	VEGAOpPainter* painter;
#endif // VEGA_OPPAINTER_SUPPORT

#ifdef SCREEN_WATERMARK
#ifdef VEGA_OPPAINTER_SUPPORT
	MDE_WaterMark* waterMark;
#endif	//VEGA_OPPAINTER_SUPPORT
#endif	//SCREEN_WATERMARK

	void* user_data;
public:
	/**
	 * LibgogiModule keeps a list of all MDE_GenericScreen
	 * instances. The MDE_GenericScreen constructor adds itself to
	 * that list and the MDE_GenericScreen destructor removes
	 * itself. If the deleted MDE_GenericScreen is equal to
	 * g_mde_screen, then the g_mde_screen is replaced with the first
	 * screen on LibgogiModule's list of screens.
	 */
	class InternalLink : public Link {
		MDE_GenericScreen* m_screen;
	public:
		InternalLink(MDE_GenericScreen* screen) : m_screen(screen) {}
		MDE_GenericScreen* Screen() { return m_screen; }
	} m_link;
};

#endif // MDE_GENERIC_SCREEN_H
