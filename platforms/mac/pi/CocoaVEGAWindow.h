/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef COCOA_VEGA_WINDOW_H
#define COCOA_VEGA_WINDOW_H

#include "modules/libvega/vegawindow.h"
#include "modules/libgogi/mde.h"
//#define VEGA_CORE_ANIMATION

class CocoaOpWindow;

class CocoaVEGAWindow : public VEGAWindow
{
public:
	explicit CocoaVEGAWindow(CocoaOpWindow* opwnd);
	virtual ~CocoaVEGAWindow();
	void resize(int w, int h, BOOL background);
	CGImageRef getImage(BOOL background=FALSE) {return background ? m_background_image : m_image;}
	CGDataProviderRef getProvider() {return m_image?CGImageGetDataProvider(m_image):NULL;}
	int getBytesPerLine();
	int width() {return getWidth();}
	int height() {return getHeight();}
	CocoaOpWindow* GetNativeOpWindow() { return m_opwnd; }
	
	// The context's backing store may be released as stack unwinds,
	// DO NOT retain beyond the scope you got it from.
	CGContextRef getPainter();

	// Create a new image with the exact coordinates of store. Useful for CALayer.contents
	// The image returned by getImage may include unwanted paddings (which will be scaled).
	CGImageRef createImage(BOOL background);

#ifndef NS4P_COMPONENT_PLUGINS
# ifndef NP_NO_QUICKDRAW
	GWorldPtr getGWorld();
# endif
#endif // NS4P_COMPONENT_PLUGINS

	// Copies the rectangle from the foreground (which goes on top of plugins)
	// to the background, and clears the rectangle in the foreground.
	void MoveToBackground(OpRect rect, bool transparent);

	// Scroll the pixels in our background buffer
	void ScrollBackground(const MDE_RECT &rect, int dx, int dy);

	// Creates a new subimage for painting on top of a plugin.
	CGImageRef CreateImageForRect(OpRect rect);

	// Clears the internal buffer (makes gadgets transparent)
	void ClearBuffer();

	// Erase the rect, turning the area transparent.
	void ClearBufferRect(const MDE_RECT &rect);
	
	void CreatePainter(CGContextRef& painter, CGDataProviderRef& provider);
	static void ReleasePainter(CGContextRef& painter, CGDataProviderRef& provider);

#ifdef VEGA_USE_HW
	VEGA3dWindow* GetVEGA3dWindow() { return m_3dwnd; }
	const VEGA3dWindow* GetVEGA3dWindow() const { return const_cast<CocoaVEGAWindow*>(this)->GetVEGA3dWindow(); }
#endif

	/** @returns a pixel store structure suitably filled in to
	 * represent the capabilities of the window */
 	virtual VEGAPixelStore* getPixelStore();

	/** @returns the width of the window. */
	virtual unsigned int getWidth();

	/** @returns the height of the window. */
	virtual unsigned int getHeight();

	/** Present the content of the window after it has been updated by
	 * vega. */
	virtual void present(const OpRect* update_rects, unsigned int num_rects);

	virtual OP_STATUS initSoftwareBackend();
#ifdef VEGA_USE_HW
	/** Function used to notify the platform window that the hardware 
	 * backend will be used for this window. */
	virtual OP_STATUS initHardwareBackend(VEGA3dWindow* win);
#endif // VEGA_USE_HW

private:
#ifndef NS4P_COMPONENT_PLUGINS
    void DestroyGWorld();
#endif // NS4P_COMPONENT_PLUGINS

	VEGAPixelStore m_store;
	unsigned char* m_background_buffer;
	unsigned char* m_calayer_buffer;
	CGImageRef m_background_image;
	CGImageRef m_image;
	CGDataProviderRef m_calayer_provider;
	CGContextRef m_ctx;
#ifdef VEGA_USE_HW
	VEGA3dWindow* m_3dwnd;
#endif
	CocoaOpWindow* m_opwnd;
#ifndef NS4P_COMPONENT_PLUGINS
# ifndef NP_NO_QUICKDRAW
	GWorldPtr m_qd_world;
# endif
#endif // NS4P_COMPONENT_PLUGINS
	MDE_Region m_bg_rgn;
	MDE_Region m_dirty_rgn;
};

#endif // COCOA_VEGA_WINDOW_H
