/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGAFILTERLIGHTING_H
#define VEGAFILTERLIGHTING_H

#ifdef VEGA_SUPPORT

#include "modules/libvega/vegafilter.h"
#include "modules/libvega/src/vegapixelformat.h"

#ifdef VEGA_3DDEVICE
#include "modules/libvega/vega3ddevice.h"
#endif // VEGA_3DDEVICE

/**
 * Apply a lighting effect to source using its alpha channel as a bump map.
 *
 * Two different types of lighting are available:
 *
 * Diffuse:
 *
 * <pre>
 * dest = kd * (N dot L) * lightcolor
 *
 *  N = normal (unit) vector (from bump),
 *  L = vector from surface to light
 * </pre>
 *
 * The destination will be opaque.
 *
 * Specular:
 *
 * <pre>
 * dest = ks * pow(N dot H, specularExponent) * lightcolor
 *
 *  N = normal (unit) vector (from bump),
 *  H = "halfway" vector - (L + E)/||L+E||,
 *  L as above, and
 *  E = "eye vector" (always [0, 0, 1])
 * </pre>
 *
 * The destination will contain an alpha channel = MAX(dest.r, dest.g, dest.b)
 *
 * A number of different types of light sources can be used:
 *
 * <ul>
 * <li>Point light: specified as a point (x,y,z) in the pixel space</li>
 * <li>Distant light: specified as the direction of the light rays</li>
 * <li>Spotlight: specified as a point (x,y,z) in the pixel space,
 * 			  a target point, a specular exponent and an optional limiting angle</li>
 * </ul>
 */
class VEGAFilterLighting : public VEGAFilter
{
public:
	VEGAFilterLighting();
	VEGAFilterLighting(VEGALightingType lt);

	void setLightingType(VEGALightingType type) { lightingType = type; }

	/**
	 * Set a scale factor used to scale the bump map
	 */
	void setSurfaceScale(VEGA_FIX surf_scale) { surfaceScale = surf_scale; }

	void setDistantLight(VEGA_FIX x_dir, VEGA_FIX y_dir, VEGA_FIX z_dir)
	{
		lsType = VEGALS_DISTANT;
		light_x = x_dir;
		light_y = y_dir;
		light_z = z_dir;
	}
	void setPointLight(VEGA_FIX x_pos, VEGA_FIX y_pos, VEGA_FIX z_pos)
	{
		lsType = VEGALS_POINT;
		light_x = x_pos;
		light_y = y_pos;
		light_z = z_pos;
	}
	void setSpotLight(VEGA_FIX x_pos, VEGA_FIX y_pos, VEGA_FIX z_pos,
					  VEGA_FIX x_tgt, VEGA_FIX y_tgt, VEGA_FIX z_tgt,
					  VEGA_FIX spec_exponent, BOOL has_angle, VEGA_FIX angle = 0)
	{
		lsType = VEGALS_SPOT;
		light_x = x_pos; light_y = y_pos; light_z = z_pos;
		paX = x_tgt; paY = y_tgt; paZ = z_tgt;
		spot_spec_exp = spec_exponent;
		has_cone_angle = has_angle;
		cos_cone_angle = VEGA_FIXCOS(angle);
		cos_falloff = VEGA_FIXCOS(angle+1); /* could be made a parameter, make it one for now */
	}

	void setLightColor(UINT32 in_light_color) { light_color = in_light_color; }

	/**
	 * Set parameters for diffuse lighting
	 */
	void setDiffuseParams(VEGA_FIX diffuse_const) { kd = diffuse_const; }

	/**
	 * Set parameters for specular lighting
	 */
	void setSpecularParams(VEGA_FIX spec_const, VEGA_FIX spec_exponent)
	{ ks = spec_const; spec_exp = spec_exponent; }

private:
	virtual OP_STATUS apply(const VEGASWBuffer& dest, const VEGAFilterRegion& region);

#ifdef VEGA_3DDEVICE
	virtual OP_STATUS apply(VEGABackingStore_FBO* destStore, const VEGAFilterRegion& region, unsigned int frame);

	void setupLightParams(VEGA3dShaderProgram* shader, VEGA3dTexture* srcTex);

	OP_STATUS getShader(VEGA3dDevice* device, VEGA3dShaderProgram** out_shader, VEGA3dTexture* srcTex);
	void putShader(VEGA3dDevice* device, VEGA3dShaderProgram* out_shader);

	void setupSourceRect(unsigned int sx, unsigned int sy, unsigned int width, unsigned int height,
						 VEGA3dTexture* tex, VEGA3dDevice::Vega2dVertex* verts);
	void setupDestinationRect(unsigned int dx, unsigned int dy, unsigned int width, unsigned int height,
							  VEGA3dDevice::Vega2dVertex* verts);
	OP_STATUS preparePass(unsigned int passno, struct VEGAMultiPassParamsLight& params);
	OP_STATUS applyRecursive(struct VEGAMultiPassParamsLight& params);
	OP_STATUS applySpot(VEGABackingStore_FBO* destStore, const VEGAFilterRegion& region, unsigned int frame);
#endif // VEGA_3DDEVICE

	void calcSurfaceNormal(VEGAConstPixelAccessor& iptr, unsigned int srcPixelStride, int edge_flags, VEGA_FIX* nx, VEGA_FIX* ny, VEGA_FIX* nz);

	void applySpotlight(VEGA_FIX l_dot_s, int& lr, int& lg, int& lb);

	enum VEGALightSourceType {
		VEGALS_DISTANT,
		VEGALS_POINT,
		VEGALS_SPOT
	};

	VEGALightingType lightingType;
	
	VEGALightSourceType lsType;

	VEGA_FIX light_x, light_y, light_z; /* pos (or direction) of light */

	VEGA_FIX paX, paY, paZ; /* look-at point for spot */
	VEGA_FIX spot_spec_exp; /* specular exponent for spot */
	BOOL has_cone_angle; /* whether the spot's cone_angle is valid */
	VEGA_FIX cos_cone_angle; /* limiting cone angle for spot */
	VEGA_FIX cos_falloff; /* fall-off */

	UINT32 light_color;

	VEGA_FIX kd, ks, spec_exp; /* diffuse constant, specular constant, specular exponent */

	VEGA_FIX surfaceScale;
};

#endif // VEGA_SUPPORT
#endif // VEGAFILTERLIGHTING_H
