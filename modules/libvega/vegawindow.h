/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGAWINDOW_H
#define VEGAWINDOW_H

#if defined(VEGA_SUPPORT) && defined(VEGA_OPPAINTER_SUPPORT)

#include "modules/libvega/vegapixelstore.h"
#ifdef VEGA_2DDEVICE
#include "modules/libvega/vega2ddevice.h"
#endif // VEGA_2DDEVICE
#ifdef VEGA_3DDEVICE
#include "modules/libvega/vega3ddevice.h"
#endif // VEGA_3DDEVICE
/** A porting interface representing a platform window. This is used for 
 * creating the main window when using OpPainter support. */
class VEGAWindow
{
public:
	/** Create a new platform window. */
	static OP_STATUS Create(VEGAWindow** win, unsigned int width, unsigned int height);

	virtual ~VEGAWindow(){}

	/** @returns a pixel store structure suitably filled in to
	 * represent the capabilities of the window */
	virtual VEGAPixelStore* getPixelStore() = 0;

	/** @returns the width of the window. */
	virtual unsigned int getWidth() = 0;

	/** @returns the height of the window. */
	virtual unsigned int getHeight() = 0;

	/** Present the content of the window after it has been updated by
	 * vega. 
	 * @param update_rects is an array of dirty rects.
	 * @param num_rects is the number of items in the update_rects array. */
	virtual void present(const OpRect* update_rects, unsigned int num_rects) = 0;

	/** Get a native handle to the window. This is only used by the 
	 * platforms pi implementation so the pointer can be anything as far as
	 * vega is concerned. */
	virtual void* getNativeHandle(){return NULL;}

	/** Function used to tell the window implementation that the software
	 * buffer will be used. */
	virtual OP_STATUS initSoftwareBackend(){return OpStatus::OK;}

#ifdef VEGA_2DDEVICE
	/** Function used to notify the platform window that the hardware 
	 * backend will be used for this window. */
	virtual OP_STATUS initHardwareBackend(VEGA2dWindow* win){return OpStatus::OK;}
#endif // VEGA_2DDEVICE

#ifdef VEGA_3DDEVICE
	/** Function used to notify the platform window that the hardware 
	 * backend will be used for this window. */
	virtual OP_STATUS initHardwareBackend(VEGA3dWindow* win){return OpStatus::OK;}
#endif // VEGA_3DDEVICE

	/** A hint to the various backends if this window requires read access to
	 * the backbuffer or not. Mainly useful for the hardware backends. */
	virtual bool requiresBackbufferReading(){return false;}
};

#endif // VEGA_SUPPORT && VEGA_OPPAINTER_SUPPORT
#endif // !VEGAWINDOW_H

