/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGAFILTER_H
#define VEGAFILTER_H

#ifdef VEGA_SUPPORT

#include "modules/libvega/vegafixpoint.h"

enum VEGAMergeType
{
	VEGAMERGE_NORMAL, VEGAMERGE_MULTIPLY, VEGAMERGE_SCREEN,
	VEGAMERGE_DARKEN, VEGAMERGE_LIGHTEN, VEGAMERGE_OVER, VEGAMERGE_IN,
	VEGAMERGE_ATOP, VEGAMERGE_OUT, VEGAMERGE_XOR, VEGAMERGE_OPACITY,
	VEGAMERGE_ARITHMETIC, VEGAMERGE_REPLACE, VEGAMERGE_PLUS
};

enum VEGAFilterEdgeMode
{
	VEGAFILTEREDGE_DUPLICATE, VEGAFILTEREDGE_WRAP, VEGAFILTEREDGE_NONE
};

enum VEGAMorphologyType
{
	VEGAMORPH_ERODE,
	VEGAMORPH_DILATE
};

enum VEGALightingType
{
	VEGALIGHTING_DIFFUSE,
	VEGALIGHTING_SPECULAR
};

enum VEGACompFuncType
{
	VEGACOMPFUNC_IDENTITY,
	VEGACOMPFUNC_TABLE,
	VEGACOMPFUNC_DISCRETE,
	VEGACOMPFUNC_LINEAR,
	VEGACOMPFUNC_GAMMA
};

enum VEGAComponent
{
	VEGACOMP_R,
	VEGACOMP_G,
	VEGACOMP_B,
	VEGACOMP_A
};

enum VEGAColorSpace
{
	VEGACOLORSPACE_SRGB,
	VEGACOLORSPACE_LINRGB
};

struct VEGALightParameter
{
	/// The type of light
	VEGALightingType type;
	/// Light parameter used for both specular and diffuse lights
	VEGA_FIX constant;
	/// Light parameter only used for specular lights
	VEGA_FIX exponent;

	/// The scaling of the normal map
	VEGA_FIX surfaceScale;

	/// The color of the light
	UINT32 color;
};

#include "modules/libvega/vegarendertarget.h"
#include "modules/libvega/src/vegaswbuffer.h"

class VEGAFill;

#ifdef VEGA_3DDEVICE
class VEGA3dShaderProgram;
class VEGA3dTexture;
class VEGA3dRenderTarget;
class VEGA3dFramebufferObject;
class VEGA3dBuffer;
class VEGA3dVertexLayout;
class VEGABackingStore_FBO;
#endif // VEGA_3DDEVICE

#ifdef VEGA_2DDEVICE
class VEGA2dSurface;
#endif // VEGA_2DDEVICE

struct VEGAFilterRegion
{
	unsigned sx, sy;		///< Source coordinates
	unsigned dx, dy;		///< Destination coordinates
	unsigned width, height;	///< Region dimensions
};

/** This is the filter class used in vega to implement all kinds of filters.
 * Simply inherit this class and add an implementation of the filter you want.
 * Care should however be taken not to create more classes than absolutly needed.
 * If you are not absolutly sure you need a new class for your filter, please
 * talk to timj@opera.com first. */
class VEGAFilter
{
public:
	VEGAFilter();
	virtual ~VEGAFilter();

	/** Set the source render target to use for this filter.
	 * @param srcRT the source render target.
	 * @param alphaOnly use only the alpha values from the source render target. */
	void setSource(VEGARenderTarget* srcRT, bool alphaOnly = false);
	void setSource(VEGAFill* srcImg, bool alphaOnly = false);

	bool hasSource() const { return sourceStore != NULL; }

	/** Help function which checks that the offsets are within bounds,
	 * and clamps the source and destination rectangles. */
	OP_STATUS checkRegion(VEGAFilterRegion& region, unsigned int destWidth, unsigned int destHeight);

	/** Apply the filter to a specific region in software. The region
	 * passed in is assumed to be correct (using for instance checkRegion). */
	virtual OP_STATUS apply(const VEGASWBuffer& dest, const VEGAFilterRegion& region) = 0;

#ifdef VEGA_3DDEVICE
	/** Apply the filter to a specific region in hardware. The region
	 * passed in is assumed to be correct (using for instance checkRegion). */
	virtual OP_STATUS apply(VEGABackingStore_FBO* destStore, const VEGAFilterRegion& region, unsigned int frame) = 0;
#endif // VEGA_3DDEVICE
#ifdef VEGA_2DDEVICE
	virtual OP_STATUS apply(VEGA2dSurface* destRT, const VEGAFilterRegion& region);
#endif // VEGA_2DDEVICE

#ifdef VEGA_3DDEVICE
	static OP_STATUS createClampTexture(VEGA3dDevice* device, VEGA3dTexture* copyTex,
	                                    unsigned int& dx, unsigned int& dy,
	                                    unsigned int width, unsigned int height,
	                                    unsigned int borderWidth, VEGAFilterEdgeMode clampMode,
	                                    VEGA3dFramebufferObject* clampTexRT, VEGA3dTexture** clampTex);
#endif // VEGA_3DDEVICE

protected:
#ifdef VEGA_3DDEVICE
	/* Check if the (source) region represents a subregion (used for clamping decisions) */
	static bool isSubRegion(const VEGAFilterRegion& region, VEGA3dTexture* tex);

	OP_STATUS applyFallback(VEGABackingStore_FBO* destStore, const VEGAFilterRegion& region);
	OP_STATUS applyShader(VEGABackingStore_FBO* destStore, const VEGAFilterRegion& region, unsigned int frame,
						  unsigned int borderWidth = 0, VEGAFilterEdgeMode edgeMode = VEGAFILTEREDGE_NONE);

	OP_STATUS cloneToTempTexture(VEGA3dDevice* device, VEGA3dRenderTarget* copyRT,
							 unsigned int dx, unsigned int dy,
							 unsigned int width, unsigned int height,
							 bool keepPosition,
							 VEGA3dTexture** texclone);
	static OP_STATUS updateClampTexture(VEGA3dDevice* device, VEGA3dTexture* copyTex,
	                                    unsigned int& dx, unsigned int& dy,
	                                    unsigned int width, unsigned int height,
	                                    unsigned int borderWidth, VEGAFilterEdgeMode clampMode,
	                                    VEGA3dFramebufferObject* clampTexRT, unsigned int minBorder = 0);

	OP_STATUS createVertexBuffer_Unary(VEGA3dDevice* device, VEGA3dBuffer** out_vbuf, VEGA3dVertexLayout** out_vlayout,
									   unsigned int* out_startIndex,
									   VEGA3dTexture* tex, VEGA3dShaderProgram* sprog,
									   const VEGAFilterRegion& region, UINT32 color = 0xffffffff);
	OP_STATUS createVertexBuffer_Binary(VEGA3dDevice* device, VEGA3dBuffer** out_vbuf, VEGA3dVertexLayout** out_vlayout,
										unsigned int* out_startIndex,
										VEGA3dTexture* tex0, VEGA3dTexture* tex1, VEGA3dShaderProgram* sprog,
										const VEGAFilterRegion& region);
	virtual OP_STATUS setupVertexBuffer(VEGA3dDevice* device, VEGA3dBuffer** out_vbuf, VEGA3dVertexLayout** out_vlayout, unsigned int* out_startIndex, VEGA3dTexture* tex, VEGA3dShaderProgram* sprog,
										const VEGAFilterRegion& region);

	virtual OP_STATUS getShader(VEGA3dDevice* device, VEGA3dShaderProgram** shader, VEGA3dTexture* srcTex) { return OpStatus::ERR; }
	virtual void putShader(VEGA3dDevice* device, VEGA3dShaderProgram* shader) {}
#endif // VEGA_3DDEVICE

	void setBackendSource();

	// Used for implicit passing of source
	VEGASWBuffer source;
#ifdef VEGA_2DDEVICE
	class VEGA2dSurface* sourceSurf;
#endif // VEGA_2DDEVICE
#ifdef VEGA_3DDEVICE
	class VEGA3dRenderTarget* sourceRT;
	bool releaseSourceRT;
#endif // VEGA_3DDEVICE

	VEGABackingStore* sourceStore;
	bool sourceAlphaOnly;
};

inline unsigned int VEGAround_pot(unsigned int x)
{
	x = x - 1;
	x = x | (x >> 1);
	x = x | (x >> 2);
	x = x | (x >> 4);
	x = x | (x >> 8);
	x = x | (x >> 16);
	return x + 1;
}

#endif // VEGA_SUPPORT
#endif // VEGAFILTER_H
