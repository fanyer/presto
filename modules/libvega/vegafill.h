/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef VEGAFILL_H
#define VEGAFILL_H

#ifdef VEGA_SUPPORT

#include "modules/libvega/vegafixpoint.h"
#include "modules/libvega/vegatransform.h"
#include "modules/libvega/src/vegapixelformat.h"
#ifdef VEGA_3DDEVICE
#include "modules/libvega/vega3ddevice.h"
#endif
/** A class used to calculate the fill color when filling a path.
  * Inherit this class and do whatever you want to create new fill effects. */
class VEGAFill
{
public:
	/** Spread method to use when reading outside the fill. */
	enum Spread {SPREAD_CLAMP, SPREAD_CLAMP_BORDER, SPREAD_REPEAT, SPREAD_REFLECT};

	/** Image quality to use for this fill. Linear is better than nearest. */
	enum Quality {QUALITY_NEAREST, QUALITY_LINEAR};

	VEGAFill() :
		xspread(SPREAD_CLAMP), yspread(SPREAD_CLAMP), quality(QUALITY_LINEAR), borderColor(0)
	{ reset(); pathTransform.loadIdentity(); invPathTransform.loadIdentity(); }

	virtual ~VEGAFill() {}

#ifdef VEGA_3DDEVICE
	/** Get a texture transform matrix used to convert a position (x, y) to
	 * a texture coordinate in the range 0, width/height. */
	virtual const VEGATransform& getTexTransform(){return invPathTransform;}
	/** Get the custom shader required to render this fill. If no custom
	 * shader is required this returns NULL. */
	virtual VEGA3dShaderProgram* getCustomShader(){return NULL;}
	virtual bool usePremulRenderState(){return false;}
	/** Get the vertex layout to use for rendering 2d content with the
	 * custom shader. If a custom shader is used this must always return 
	 * a valid layout. This function should not be called before calling
	 * getCustomShader. */
	virtual VEGA3dVertexLayout* getCustomVertexLayout(){return NULL;}
#endif // VEGA_3DDEVICE

	/** Prepare fill before use. (SW pipeline) */
	virtual OP_STATUS prepare() { return OpStatus::OK; }

	/** Called when fill completed. (SW pipeline) */
	virtual void complete() {}

	/** Apply this fill to a scanline. (SW pipeline)
	 * @param color the destination scanline color buffer.
	 * @param span span to fill.
	 */
	virtual void apply(VEGAPixelAccessor color, struct VEGASpanInfo& span) = 0;

	/** Set the opacity of the fill.
	  * @param fo the opacity. */
	void setFillOpacity(unsigned char fo){fillOpacity = fo;}

	/** Get the opacity of the fill.
	  * @returns the opacity. */
	unsigned char getFillOpacity(){return fillOpacity;}

	/** Set the transform used by this fill. */
	virtual void setTransform(const VEGATransform& pathTrans, const VEGATransform& invPathTrans){pathTransform.copy(pathTrans);invPathTransform.copy(invPathTrans);}

	/** Set the spread used by this fill. If x and y spread differs
	 * but different spreads are not supported the xspread is used. */
	void setSpread(Spread xspr, Spread yspr){xspread = xspr; yspread = yspr;}

	/** Set the spread used by this fill. */
	void setSpread(Spread spread){xspread = yspread = spread;}

	Spread getXSpread() { return xspread; }
	Spread getYSpread() { return yspread; }

	/** Set the border color used for the CLAMP_BORDER spread method. */
	void setBorderColor(UINT32 col){borderColor = col;}

	/** Set the quality to use for rendering this fill. */
	void setQuality(Quality q){quality = q;}

	/** Returns TRUE if the data returned from prepareScanline has
	 * premultipled alpha. */
	virtual bool hasPremultipliedAlpha() { return FALSE; }

	/** Returns true if this fill is a VEGAImage */
	virtual bool isImage() const { return false; }

	/** Returns a potentially optimized representation for this fill */
	virtual VEGAFill* getCache(class VEGARendererBackend* backend,
							   VEGA_FIX gx, VEGA_FIX gy, VEGA_FIX gw, VEGA_FIX gh) { return NULL; }

#ifdef VEGA_3DDEVICE
	/** Get the texture assosiated with this fill, if any. */
	virtual VEGA3dTexture* getTexture(){return NULL;}
#endif // VEGA_3DDEVICE
protected:
	/** Reset the general fill state.
	 * Resets the fill with initial values. */
	void reset() { fillOpacity = 255; }

private:
	unsigned char fillOpacity;
protected:
	Spread xspread, yspread;
	Quality quality;
	VEGATransform pathTransform;
	VEGATransform invPathTransform;
	UINT32 borderColor;
};

#endif // VEGA_SUPPORT
#endif // VEGAFILL_H
