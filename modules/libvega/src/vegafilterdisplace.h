/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGAFILTERDISPLACE_H
#define VEGAFILTERDISPLACE_H

#ifdef VEGA_SUPPORT
#include "modules/libvega/vegafilter.h"

/**
 * Displace the source using a displacement map.
 *
 * Fetch a colorvalue from the source image, by first fetching a
 * colorvalue from the displacement (bit)map, and using one or
 * several of its color components to displace the current position in the source
 * bitmap 
 *
 * <pre>
 * dx = channel(displacementmap(x,y), xsel)
 * dy = channel(displacementmap(x,y), ysel)
 * dest(x,y) = src(x + scale * (dx - .5), y + scale * (dy - .5))
 * </pre>
 *
 */
class VEGAFilterDisplace : public VEGAFilter
{
public:
	VEGAFilterDisplace();

	void setDisplacementMap(VEGABackingStore* dmapstore)
	{
		mapstore = dmapstore;
	}

	void setScale(VEGA_FIX scl) { scale = scl; }

	enum Component
	{
		COMP_A = 0,
		COMP_R = 1,
		COMP_G = 2,
		COMP_B = 3
	};

	void setDispInX(Component x_component) { x_comp = x_component; }
	void setDispInY(Component y_component) { y_comp = y_component; }

private:
	virtual OP_STATUS apply(const VEGASWBuffer& dest, const VEGAFilterRegion& region);

	VEGABackingStore* mapstore;

	VEGA_FIX scale;

	unsigned int x_comp;
	unsigned int y_comp;

#ifdef VEGA_3DDEVICE
	virtual OP_STATUS apply(VEGABackingStore_FBO* destStore, const VEGAFilterRegion& region, unsigned int frame);

	OP_STATUS setupVertexBuffer(VEGA3dDevice* device, VEGA3dBuffer** out_vbuf, VEGA3dVertexLayout** out_vlayout, unsigned int* out_startIndex, VEGA3dTexture* tex, VEGA3dShaderProgram* sprog,
								const VEGAFilterRegion& region);

	OP_STATUS getShader(VEGA3dDevice* device, VEGA3dShaderProgram** out_shader, VEGA3dTexture* srcTex);
	void putShader(VEGA3dDevice* device, VEGA3dShaderProgram* shader);

	VEGA3dTexture* dispmapTex;
#endif // VEGA_3DDEVICE
	VEGASWBuffer dispmap;
};

#endif // VEGA_SUPPORT
#endif // VEGAFILTERDISPLACE_H
