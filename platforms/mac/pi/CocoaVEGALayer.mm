/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#if defined(VEGA_SUPPORT) && defined(VEGA_OPPAINTER_SUPPORT) && defined(CANVAS3D_HW_COMPOSITING)

#include "platforms/mac/pi/CocoaVEGALayer.h"
#include "platforms/mac/pi/CocoaVEGAWindow.h"
#include "platforms/mac/pi/CocoaOpWindow.h"
#include "platforms/mac/model/OperaNSView.h"

#include "modules/libvega/vega3ddevice.h"
#include "platforms/vega_backends/opengl/vegagltexture.h"
#include "platforms/vega_backends/opengl/vegagldevice.h"

#include "platforms/mac/model/CanvasCALayer.h"

/* static */
OP_STATUS VEGALayer::Create(VEGALayer** layer, VEGAWindow* win)
{
	*layer = new CocoaVEGALayer(win);
	return *layer ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

CocoaVEGALayer::CocoaVEGALayer(VEGAWindow* win)
{
	m_window = static_cast<CocoaVEGAWindow*>(win);
	m_view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 0, 0)];
	m_texture = NULL;

	OperaNSView *view = (OperaNSView *)m_window->GetNativeOpWindow()->GetContentView();
	[view addSubview:m_view positioned:NSWindowBelow relativeTo:[view vegaView]];
	m_layer = [CanvasCALayer layer];
	[m_view setLayer:m_layer];
	[m_view setWantsLayer:YES];
}

/* virtual */
CocoaVEGALayer::~CocoaVEGALayer()
{
	[m_view setLayer:nil];
	[m_view removeFromSuperview];
	[m_view release];
}

/* virtual */
void CocoaVEGALayer::Update(VEGA3dTexture *tex)
{

	if (tex != m_texture) {
		if (m_texture)
			VEGARefCount::DecRef(m_texture);
		m_texture = tex;
		if (m_texture)
			VEGARefCount::IncRef(m_texture);
	}
	[m_layer setTexture:tex?static_cast<VEGAGlTexture*>(tex)->getTextureId():0];

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, static_cast<VEGAGlTexture*>(tex)->getTextureId());
GLenum err = glGetError();
if (err) {
	printf("Bind failed %x\n", err);
}

	[m_layer setNeedsDisplay];
}

/* virtual */
void CocoaVEGALayer::OnPaint()
{
	NSRect frame = [m_view frame];
	float w = frame.size.width, h=frame.size.height;
	m_window->MoveToBackground(OpRect(0,0,w,h), true);
}

/* virtual */
void CocoaVEGALayer::setPosition(INT32 x, INT32 y)
{
	[m_view setFrameOrigin:NSMakePoint(x, [[m_view superview] frame].size.height-[m_view frame].size.height-y)];
}

/* virtual */
void CocoaVEGALayer::setSize(UINT32 width, UINT32 height)
{
	[m_view setFrameSize:NSMakeSize(width, height)];
	[m_layer setBounds:CGRectMake(0, 0, width, height)];
	[m_layer setSize:CGSizeMake(width, height)];
}

#endif // VEGA_SUPPORT && VEGA_OPPAINTER_SUPPORT && CANVAS3D_HW_COMPOSITING
