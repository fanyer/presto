/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef COCOA_MDE_SCREEN_H
#define COCOA_MDE_SCREEN_H

#include "modules/libgogi/pi_impl/mde_generic_screen.h"
#include "platforms/mac/pi/CocoaVEGAWindow.h"

// Uncomment to try to mutex lock CALayer updates (HIGHLY EXPERIMENTAL)
//#define MUTEX_LOCK_CALAYER_UPDATES

// Should be removed when we find a proper fix for DSK-372133
//#define VEGALAYER_DISPLAY_WORKAROUND	

// Logically more correct, but causes degraded scrolling performance
// http://t/platforms/desktop/performance/scrolling/empty_document.html
//#define USE_TIME_SINCE_VALIDATE

class CocoaOpWindow;
class CocoaVEGAWindow;
#ifdef __OBJC__
#define BOOL NSBOOL
#import <Foundation/NSDate.h>
#undef BOOL
#else
class NSDate;
#endif

class CocoaMDEScreen : public MDE_GenericScreen
{
public:
	CocoaMDEScreen(CocoaOpWindow* w, CocoaVEGAWindow*v);
	virtual ~CocoaMDEScreen();
	virtual OP_STATUS Init();
	virtual MDE_BUFFER *LockBuffer();
	virtual void UnlockBuffer(MDE_Region *update_region);
	virtual void OnValidate();
	virtual void OnInvalid();
	virtual void OnTimeOut();
	virtual void OnBeforeRectPaint(const MDE_RECT &rect);
	virtual bool UseTransparentBackground();
	virtual void ScrollBackground(const MDE_RECT &rect, int dx, int dy);

	// Refresh the backbuffer
	void UpdateImageAndResizeIfNeeded(int w, int h, BOOL background);
	// Copies the rectangle from the foreground (which goes on top of plugins)
	// to the background, and clears the rectangle in the foreground.
	void MoveToBackground(OpRect rect, bool transparent);
	// Creates a new subimage for painting on top of a plugin.
	CGImageRef CreateImageForRect(OpRect rect);

#ifdef MUTEX_LOCK_CALAYER_UPDATES
	void AddRef(CocoaMDEScreen** refAddr) {
		refs.Add(refAddr);
	}
	void RemoveRef(CocoaMDEScreen** refAddr) {
		refs.RemoveByItem(refAddr);
	}
	
	void SetMutex(pthread_mutex_t *mutex) { m_mutex = mutex; }
	bool IsDirty() const {return m_dirty;}
#endif

	void DoValidate(BOOL dont_display);
	void BypassUpdateLock() { m_bypass_update_lock = TRUE; }
	CocoaVEGAWindow* GetVegaWindow() {return m_vega;}

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	int GetPixelScale() const;
	void UpdatePainterScaleFactor();
#endif // PIXEL_SCALE_RENDERING_SUPPORT

private:
	CocoaOpWindow* m_window;
	CocoaVEGAWindow* m_vega;
	void *m_timer;				// Timer pointer (Cocoa)
	BOOL m_bypass_update_lock;
	BOOL m_dont_display;
	INT32 m_gradient_height;
	bool m_dirty;
	pthread_mutex_t *m_mutex;
	OpVector<CocoaMDEScreen*> refs;
#ifdef USE_TIME_SINCE_VALIDATE
	NSDate* m_last_valid;
#endif
};

#endif // COCOA_MDE_SCREEN_H
