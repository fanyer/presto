/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef COCOA_VEGA_LAYER_H
#define COCOA_VEGA_LAYER_H

#include "modules/libvega/vegalayer.h"

#define BOOL NSBOOL
#import <AppKit/NSView.h>
@class CanvasCALayer;
#undef BOOL

class CocoaVEGALayer : public VEGALayer
{
public:
	CocoaVEGALayer(VEGAWindow* win);

	virtual ~CocoaVEGALayer();

	/* Set the texture to be drawn. */
	virtual void Update(VEGA3dTexture *tex);

	/* Let the vega layer know that it is time to paint.
	* Usually the layer will respond by clearing the corresponding pixels in the vega backend.
	* @param bounds the update rectangle in layer-relative coordinates.
	*/
	virtual void OnPaint();

	/* Set the position of the layer.
	* @param x horizontal location in window-relative coordinates.
	* @param y vertical location in window-relative coordinates.
	*/
	virtual void setPosition(INT32 x, INT32 y);

	virtual void setSize(UINT32 width, UINT32 height);
private:
	class CocoaVEGAWindow* m_window;
	VEGA3dTexture *m_texture;
	NSView *m_view;
	CanvasCALayer *m_layer;
};

#endif // COCOA_VEGA_LAYER_H
