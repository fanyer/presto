/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
#include "modules/display/pixelscaler.h"
#endif // PIXEL_SCALE_RENDERING_SUPPORT

#include "platforms/mac/pi/display/MacOpGLDevice.h"
#include "platforms/mac/pi/CocoaVEGAWindow.h"
#include "platforms/mac/pi/CocoaMDEScreen.h"
#include "platforms/mac/pi/CocoaOpWindow.h"
#include "platforms/mac/CocoaVegaDefines.h"

// DSK-290489 Text not being rendered when whole string is not fully visible after scrolling
#define PADDING_FOR_MISSING_TEXT	72

struct LockableBitmap
{
	LockableBitmap(void* ptr, CocoaMDEScreen* screen)
		: ptr(ptr)
#ifdef MUTEX_LOCK_CALAYER_UPDATES
		, screen(screen)
#endif
	{
#ifdef MUTEX_LOCK_CALAYER_UPDATES
		pthread_mutex_init(&mutex, NULL);
		screen->AddRef(&screen);
#endif
	}
	void* ptr;
#ifdef MUTEX_LOCK_CALAYER_UPDATES
	CocoaMDEScreen* screen;
	pthread_mutex_t mutex;
#endif
};

static const void * GetBytePointer(void *info)
{
	LockableBitmap* lb = (LockableBitmap*)info;
#ifdef MUTEX_LOCK_CALAYER_UPDATES
	pthread_mutex_lock(&lb->mutex);
#endif
	return lb->ptr;
}

static void ReleaseBytePointer(void *info, const void *pointer)
{
#ifdef MUTEX_LOCK_CALAYER_UPDATES
	LockableBitmap* lb = (LockableBitmap*)info;
	pthread_mutex_unlock(&lb->mutex);
	if (lb->screen && lb->screen->IsDirty())
	{
		lb->screen->Validate(true);
	}
#endif
}

static void FreeBitmap(void *info)
{
	LockableBitmap* lb = (LockableBitmap*)info;
	free(lb->ptr);
#ifdef MUTEX_LOCK_CALAYER_UPDATES
	if (lb->screen)
		lb->screen->RemoveRef(&lb->screen);
#endif
	OP_DELETE((LockableBitmap*)info);
}

CGDataProviderDirectCallbacks gProviderCallbacks = {
	0,
	GetBytePointer,
	ReleaseBytePointer,
	NULL,
	FreeBitmap
};

CocoaVEGAWindow::CocoaVEGAWindow(CocoaOpWindow* opwnd)
	: m_background_buffer(NULL)
	, m_calayer_buffer(NULL)
	, m_background_image(0)
	, m_image(0)
	, m_calayer_provider(0)
	, m_ctx(0)
#ifdef VEGA_USE_HW
	, m_3dwnd(NULL)
#endif
	, m_opwnd(opwnd)
#ifndef NS4P_COMPONENT_PLUGINS
# ifndef NP_NO_QUICKDRAW
    , m_qd_world(NULL)
# endif
#endif // NS4P_COMPONENT_PLUGINS
{
	m_store.buffer = NULL;
	m_store.stride = 0;
#ifdef VEGA_INTERNAL_FORMAT_RGBA8888
	m_store.format = VPSF_RGBA8888;
#else
	m_store.format = VPSF_BGRA8888;
#endif
	m_store.width = m_store.height = 0;
}

CocoaVEGAWindow::~CocoaVEGAWindow()
{
	if (m_ctx) {
		CGContextRelease(m_ctx);
	}
	if (m_image) {
		CGImageRelease(m_image);
	}
	if (m_background_image) {
		CGImageRelease(m_background_image);
	}
#ifndef NS4P_COMPONENT_PLUGINS
    DestroyGWorld();
#endif // NS4P_COMPONENT_PLUGINS
	if (m_calayer_provider) {
		CGDataProviderRelease(m_calayer_provider);
	}
}

CGContextRef CocoaVEGAWindow::getPainter()
{
#ifdef VEGA_USE_HW
	return (m_3dwnd?NULL:m_ctx);
#else
	return m_ctx;
#endif
}

CGImageRef CocoaVEGAWindow::createImage(BOOL background)
{
	if (!m_image)
		return NULL;

	if (background) {
		if (m_background_image && m_bg_rgn.num_rects > 0)
			return CGImageCreate(m_store.width, m_store.height, 8, 32, CGImageGetBytesPerRow(m_background_image), CGImageGetColorSpace(m_background_image), CGImageGetBitmapInfo(m_background_image), CGImageGetDataProvider(m_background_image), NULL, 0, kCGRenderingIntentAbsoluteColorimetric);

		// No plugins so no background
		return NULL;
	}
	// Cannot return m_image, or another image with the same backing store,
	// because CALayer will copy the data asynchronously,
	// possibly overlapping with the next update.
	// Meaning, it may copy an unfinished bitmap.
#ifdef MUTEX_LOCK_CALAYER_UPDATES
	return CGImageCreate(m_store.width, m_store.height, 8, 32, CGImageGetBytesPerRow(m_image), CGImageGetColorSpace(m_image), CGImageGetBitmapInfo(m_image), CGImageGetDataProvider(m_image), NULL, 0, kCGRenderingIntentAbsoluteColorimetric);
#else
	if (m_calayer_provider) {
		// Update dirty
		UINT8* src = (UINT8*)m_store.buffer;
		UINT8* dst = (UINT8*)m_calayer_buffer;

		MDE_RECT union_rect = {0,0,0,0};
		for (int i = 0; i < m_dirty_rgn.num_rects; i++)
			union_rect = MDE_RectUnion(union_rect, m_dirty_rgn.rects[i]);

		if (m_store.stride == m_store.width*4 && (float)union_rect.w > 0.95*m_store.width)
			memcpy(dst+union_rect.y*m_store.stride, src+union_rect.y*m_store.stride, m_store.stride*union_rect.h);
		else
			for (int i = 0; i < m_dirty_rgn.num_rects; i++) {
				MDE_RECT r = m_dirty_rgn.rects[i];
				if (m_store.stride == m_store.width*4 && r.x == 0 && r.w == m_store.width)
					memcpy(dst+r.y*m_store.stride, src+r.y*m_store.stride, m_store.stride*r.h);
				else
					for (int y = r.y; y < r.y+r.h; y++)
						memcpy(dst+y*m_store.width*4+r.x*4, src+y*m_store.stride+r.x*4, r.w*4);
			}
	} else {
		m_calayer_buffer = (unsigned char*)calloc(1, m_store.width*m_store.height*4);
		if (!m_calayer_buffer)
			return NULL;
		UINT8* src = (UINT8*)m_store.buffer;
		UINT8* dst = (UINT8*)m_calayer_buffer;
		if (m_store.stride == m_store.width*4)
			memcpy(dst, src, m_store.width*m_store.height*4);
		else
			for (unsigned y = 0; y < m_store.height; y++)
				memcpy(dst+y*m_store.width*4, src+y*m_store.stride, m_store.width*4);
		LockableBitmap* info = OP_NEW(LockableBitmap, (m_calayer_buffer, m_opwnd->GetScreen()));
		m_calayer_provider = CGDataProviderCreateDirect(info, m_store.width*m_store.height*4, &gProviderCallbacks);
	}
	m_dirty_rgn.Reset();
	return CGImageCreate(m_store.width, m_store.height, 8, 32, m_store.width*4, CGImageGetColorSpace(m_image), CGImageGetBitmapInfo(m_image), m_calayer_provider, NULL, 0, kCGRenderingIntentAbsoluteColorimetric);
#endif // !MUTEX_LOCK_CALAYER_UPDATES
}

VEGAPixelStore* CocoaVEGAWindow::getPixelStore()
{
	return &m_store;
}

unsigned int CocoaVEGAWindow::getWidth()
{
	return m_store.width;
}

unsigned int CocoaVEGAWindow::getHeight()
{
	return m_store.height;
}

void CocoaVEGAWindow::present(const OpRect* update_rects, unsigned int num_rects)
{
	MDE_Region reg;
	for (unsigned int i = 0; i < num_rects; ++i)
	{
		OpRect rect = update_rects[i];

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		PixelScaler scaler(m_opwnd->GetPixelScale());
		rect = FROM_DEVICE_PIXEL(scaler, rect);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

		reg.AddRect(MDE_MakeRect(rect.x, rect.y, rect.width, rect.height));
	}
	m_opwnd->Invalidate(&reg);
}

void CocoaVEGAWindow::resize(int w, int h, BOOL background)
{
#ifdef VEGA_USE_HW
	if (m_3dwnd)
	{
		m_3dwnd->resizeBackbuffer(w, h);
		m_store.width = w;
		m_store.height = h;
		return;
	}
#endif
	m_bg_rgn.Reset();
	if (m_calayer_provider) {
		CGDataProviderRelease(m_calayer_provider);
		m_calayer_buffer = NULL;
		m_calayer_provider = NULL;
	}
	if (m_image) {
		if (CGImageGetWidth(m_image) >= static_cast<unsigned int>(w)
			&& CGImageGetHeight(m_image) >= static_cast<unsigned int>(h))
		{
			m_store.width = w;
			m_store.height = h;
			return;
		}
		CGContextRelease(m_ctx);
		CGImageRelease(m_image);

		if (m_background_image)
			CGImageRelease(m_background_image);
		m_background_buffer = NULL;
		m_background_image = NULL;
	}

	int bpl= (w*4+3) & ~3;
	int bitmap_height = h+PADDING_FOR_MISSING_TEXT;
	void *image_data = calloc(1, bitmap_height*bpl);

	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
	LockableBitmap* info = OP_NEW(LockableBitmap, (image_data, m_opwnd->GetScreen()));
#ifdef MUTEX_LOCK_CALAYER_UPDATES
	m_opwnd->GetScreen()->SetMutex(&info->mutex);
#endif
	CGDataProviderRef provider = CGDataProviderCreateDirect(info, bitmap_height*bpl, &gProviderCallbacks);
	CGBitmapInfo alpha = kCGBitmapByteOrderVegaInternal;
	m_image = CGImageCreate(w, bitmap_height, 8, 32, bpl, colorSpace, (CGImageAlphaInfo)alpha, provider, NULL, 0, kCGRenderingIntentAbsoluteColorimetric);
	CGDataProviderRelease(provider);
	CGColorSpaceRelease(colorSpace);
	m_store.stride = bpl;
	m_store.width = w;
	m_store.height = h;
	m_store.buffer = image_data;
	m_ctx = CGBitmapContextCreate(m_store.buffer, CGImageGetWidth(m_image), CGImageGetHeight(m_image), 8, CGImageGetBytesPerRow(m_image), CGImageGetColorSpace(m_image), (CGImageAlphaInfo)CGImageGetBitmapInfo(m_image));

#ifndef NS4P_COMPONENT_PLUGINS
    DestroyGWorld();
#endif // NS4P_COMPONENT_PLUGINS
}

void CocoaVEGAWindow::CreatePainter(CGContextRef& painter, CGDataProviderRef& provider)
{
	painter = CGBitmapContextCreate(m_store.buffer, CGImageGetWidth(m_image), CGImageGetHeight(m_image), 8, CGImageGetBytesPerRow(m_image), CGImageGetColorSpace(m_image), (CGImageAlphaInfo)CGImageGetBitmapInfo(m_image));
	provider = CGImageGetDataProvider(m_image);
	CGDataProviderRetain(provider);
}

void CocoaVEGAWindow::MoveToBackground(OpRect rect, bool transparent)
{
#ifdef VEGA_USE_HW
	if (m_3dwnd)
	{
		((MacGlWindow*)m_3dwnd)->MoveToBackground(MDE_MakeRect(rect.x, rect.y, rect.width, rect.height));
	}
	else
	{
#endif
		MDE_RECT mderect={rect.x,rect.y,rect.width,rect.height};
		if (transparent)
			m_bg_rgn.IncludeRect(mderect);
		unsigned char* buffer = (unsigned char*)m_store.buffer;
		rect.IntersectWith(OpRect(0,0,m_store.width,m_store.height));
		if (transparent && !m_background_buffer)
		{
			m_background_buffer = (unsigned char*)calloc(1, CGImageGetHeight(m_image)*CGImageGetBytesPerRow(m_image));
			LockableBitmap* info = OP_NEW(LockableBitmap, (m_background_buffer, m_opwnd->GetScreen()));
			CGDataProviderRef background_provider = CGDataProviderCreateDirect(info, CGImageGetHeight(m_image)*CGImageGetBytesPerRow(m_image), &gProviderCallbacks);
			m_background_image = CGImageCreate(m_store.width,m_store.height, 8, 32, CGImageGetBytesPerRow(m_image), CGImageGetColorSpace(m_image), (CGImageAlphaInfo)CGImageGetBitmapInfo(m_image), background_provider, NULL, 0, kCGRenderingIntentAbsoluteColorimetric);
			CGDataProviderRelease(background_provider);
		}

		for (int i = rect.y; i<rect.y+rect.height; i++)
		{
			int offset = i*m_store.stride+rect.x*4;
			if (transparent)
				op_memcpy(m_background_buffer+offset, buffer+offset,rect.width*4);
			op_memset(buffer+offset, 0, rect.width*4);
		}
#ifdef VEGA_USE_HW
	}
#endif
}

void CocoaVEGAWindow::ScrollBackground(const MDE_RECT &rect, int dx, int dy)
{
	m_dirty_rgn.IncludeRect(rect);
	if (m_background_buffer && m_bg_rgn.num_rects > 0)
	{
		unsigned char* src, *dst;
		src = dst = m_background_buffer;
		if (dy < 0)
			for (int y = rect.y; y < rect.y+rect.h; y++)
				memcpy(dst+y*m_store.stride+rect.x*4, src+(y-dy)*m_store.stride+(rect.x+dx)*4, rect.w*4);
		else if (dy > 0)
			for (int y = rect.y+rect.h-1; y >= rect.y; y--)
				memcpy(dst+y*m_store.stride+rect.x*4, src+(y-dy)*m_store.stride+(rect.x+dx)*4, rect.w*4);
		else if (dx)
			for (int y = rect.y; y < rect.y+rect.h; y++)
				memmove(dst+y*m_store.stride+rect.x*4, src+(y-dy)*m_store.stride+(rect.x+dx)*4, rect.w*4);
	}
}

CGImageRef CocoaVEGAWindow::CreateImageForRect(OpRect rect)
{
	return CGImageCreateWithImageInRect(m_image, CGRectMake(rect.x, rect.y, rect.width, rect.height));
}

/* static */
void CocoaVEGAWindow::ReleasePainter(CGContextRef& painter, CGDataProviderRef& provider)
{
	CGContextRelease(painter);
	CGDataProviderRelease(provider);
}

int CocoaVEGAWindow::getBytesPerLine()
{
	if (m_image) {
		return CGImageGetBytesPerRow(m_image);
	}
	return 0;
}

void CocoaVEGAWindow::ClearBuffer()
{
	m_bg_rgn.Reset();
	MDE_RECT rect = {0,0,m_store.width,m_store.height};
	m_dirty_rgn.IncludeRect(rect);
	if (m_store.buffer)
	{
		op_memset(m_store.buffer, 0, m_store.stride * m_store.height);
		if (m_background_buffer)
			op_memset(m_background_buffer, 0, m_store.stride * m_store.height);
	}
}

void CocoaVEGAWindow::ClearBufferRect(const MDE_RECT &rect)
{
#ifdef VEGA_USE_HW
	if (m_3dwnd)
	{
		((MacGlWindow*)m_3dwnd)->Erase(rect, true);
	}
	else
#endif
	{
		m_bg_rgn.ExcludeRect(rect);
		m_dirty_rgn.IncludeRect(rect);
		if (m_background_buffer)
		{
			for (int y = rect.y; y<rect.y+rect.h; y++)
			{
				int offset = y*m_store.stride+rect.x*4;
				op_memset(m_background_buffer+offset, 0, rect.w*4);
			}
		}
	}
}

OP_STATUS CocoaVEGAWindow::initSoftwareBackend()
{
	return OpStatus::OK;
}

#ifdef VEGA_USE_HW
OP_STATUS CocoaVEGAWindow::initHardwareBackend(VEGA3dWindow* win)
{
	m_3dwnd = win;
	return OpStatus::OK;
}
#endif // VEGA_USE_HW
