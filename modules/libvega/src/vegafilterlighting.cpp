/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef VEGA_SUPPORT
#include "modules/libvega/src/vegafilterlighting.h"
#ifdef VEGA_3DDEVICE
#include "modules/libvega/src/vegabackend_hw3d.h"
#endif // VEGA_3DDEVICE

#define VEGA_CLAMP_U8(v) (((v) <= 255) ? (((v) >= 0) ? (v) : 0) : 255)

VEGAFilterLighting::VEGAFilterLighting() :
	lightingType(VEGALIGHTING_DIFFUSE), lsType(VEGALS_POINT), light_color(0xffffffff),
	kd(VEGA_INTTOFIX(1)), ks(VEGA_INTTOFIX(1)), spec_exp(VEGA_INTTOFIX(1)),
	surfaceScale(VEGA_INTTOFIX(1))
{}

VEGAFilterLighting::VEGAFilterLighting(VEGALightingType lt) :
	lightingType(lt), lsType(VEGALS_POINT), light_color(0xffffffff),
	kd(VEGA_INTTOFIX(1)), ks(VEGA_INTTOFIX(1)), spec_exp(VEGA_INTTOFIX(1)),
	surfaceScale(VEGA_INTTOFIX(1))
{}

#ifdef VEGA_3DDEVICE

void VEGAFilterLighting::setupLightParams(VEGA3dShaderProgram* shader, VEGA3dTexture* srcTex)
{
	/* Setup light source parameters */
	float tmp[3] = { ((light_color >> 16) & 0xff) / 255.0f,
					 ((light_color >> 8) & 0xff) / 255.0f,
					 (light_color & 0xff) / 255.0f };
	shader->setVector3(shader->getConstantLocation("light_color"), tmp);

	tmp[0] = VEGA_FIXTOFLT(light_x);
	tmp[1] = VEGA_FIXTOFLT(light_y);
	tmp[2] = VEGA_FIXTOFLT(light_z);
	shader->setVector3(shader->getConstantLocation("light_position"), tmp);

	shader->setScalar(shader->getConstantLocation("light_ks"), VEGA_FIXTOFLT(ks));
	shader->setScalar(shader->getConstantLocation("light_kd"), VEGA_FIXTOFLT(kd));
	shader->setScalar(shader->getConstantLocation("light_specexp"), VEGA_FIXTOFLT(spec_exp));

	if (lsType == VEGALS_SPOT)
	{
		/* Setup spotlight parameters */
		tmp[0] = VEGA_FIXTOFLT(paX - light_x);
		tmp[1] = VEGA_FIXTOFLT(paY - light_y);
		tmp[2] = VEGA_FIXTOFLT(paZ - light_z);
		shader->setVector3(shader->getConstantLocation("spot_dir"), tmp);

		shader->setScalar(shader->getConstantLocation("spot_falloff"), VEGA_FIXTOFLT(cos_falloff));
		shader->setScalar(shader->getConstantLocation("spot_coneangle"), VEGA_FIXTOFLT(cos_cone_angle));
		shader->setScalar(shader->getConstantLocation("spot_specexp"), VEGA_FIXTOFLT(spot_spec_exp));
		shader->setScalar(shader->getConstantLocation("spot_has_cone"), has_cone_angle);
	}

	shader->setScalar(shader->getConstantLocation("surface_scale"), VEGA_FIXTOFLT(surfaceScale));

	float diffuse = lightingType == VEGALIGHTING_DIFFUSE ? 1.0f : 0.0f;
	shader->setScalar(shader->getConstantLocation("k1"), 0.0f); // diffuse * specular
	shader->setScalar(shader->getConstantLocation("k2"), diffuse); // diffuse
	shader->setScalar(shader->getConstantLocation("k3"), 1.0f - diffuse); // specular
	shader->setScalar(shader->getConstantLocation("k4"), 0.0f); // bias

	tmp[0] = 1.0f / srcTex->getWidth(); tmp[1] = 1.0f / srcTex->getHeight();
	shader->setVector2(shader->getConstantLocation("pixel_size"), tmp);
}

OP_STATUS VEGAFilterLighting::getShader(VEGA3dDevice* device, VEGA3dShaderProgram** out_shader, VEGA3dTexture* srcTex)
{
	VEGA3dShaderProgram::ShaderType shdtype;

	switch (lsType)
	{
	case VEGALS_DISTANT:
		shdtype = VEGA3dShaderProgram::SHADER_LIGHTING_DISTANTLIGHT;
		break;
	case VEGALS_POINT:
		shdtype = VEGA3dShaderProgram::SHADER_LIGHTING_POINTLIGHT;
		break;
	case VEGALS_SPOT:
	default:
		OP_ASSERT(FALSE); // You shouldn't be here
		return OpStatus::ERR;
	}

	VEGA3dShaderProgram* shader = NULL;
	RETURN_IF_ERROR(device->createShaderProgram(&shader, shdtype, VEGA3dShaderProgram::WRAP_CLAMP_CLAMP));

	device->setShaderProgram(shader);

	setupLightParams(shader, srcTex);

	device->setTexture(0, srcTex);

	*out_shader = shader;

	return OpStatus::OK;
}

void VEGAFilterLighting::putShader(VEGA3dDevice* device, VEGA3dShaderProgram* shader)
{
	OP_ASSERT(device && shader);

	VEGARefCount::DecRef(shader);
}

struct VEGAMultiPassParamsLight
{
	VEGA3dDevice* device;
	VEGA3dShaderProgram* shader;
	VEGA3dBuffer* vtxbuf;
	unsigned int startIndex;
	VEGA3dVertexLayout* vtxlayout;

	VEGA3dTexture* temporary1;
	VEGA3dFramebufferObject* temporary1RT;
	VEGA3dRenderTarget* destination;

	VEGA3dTexture* source;
	VEGA3dRenderTarget* target;

	VEGA3dDevice::Vega2dVertex* verts;

	VEGAFilterRegion region;
	unsigned int num_passes;
};

void VEGAFilterLighting::setupSourceRect(unsigned int sx, unsigned int sy,
										 unsigned int width, unsigned int height,
										 VEGA3dTexture* tex, VEGA3dDevice::Vega2dVertex* verts)
{
	verts[0].s = (float)sx / tex->getWidth();
	verts[0].t = (float)sy / tex->getHeight();
	verts[1].s = (float)sx / tex->getWidth();
	verts[1].t = (float)(sy+height) / tex->getHeight();
	verts[2].s = (float)(sx+width) / tex->getWidth();
	verts[2].t = (float)sy / tex->getHeight();
	verts[3].s = (float)(sx+width) / tex->getWidth();
	verts[3].t = (float)(sy+height) / tex->getHeight();
}

void VEGAFilterLighting::setupDestinationRect(unsigned int dx, unsigned int dy,
											  unsigned int width, unsigned int height,
											  VEGA3dDevice::Vega2dVertex* verts)
{
	verts[0].x = (float)dx;
	verts[0].y = (float)dy;
	verts[0].color = 0xffffffff;
	verts[1].x = (float)dx;
	verts[1].y = (float)(dy+height);
	verts[1].color = 0xffffffff;
	verts[2].x = (float)(dx+width);
	verts[2].y = (float)dy;
	verts[2].color = 0xffffffff;
	verts[3].x = (float)(dx+width);
	verts[3].y = (float)(dy+height);
	verts[3].color = 0xffffffff;
}

OP_STATUS VEGAFilterLighting::preparePass(unsigned int passno, VEGAMultiPassParamsLight& params)
{
	VEGA3dDevice::Vega2dVertex* verts = params.verts;
	if (passno == 0) // Bump generation pass
	{
		VEGA3dTexture* srcTex = static_cast<VEGA3dFramebufferObject*>(sourceRT)->getAttachedColorTexture();
		setupSourceRect(params.region.sx, params.region.sy, params.region.width, params.region.height,
						srcTex, verts);
		setupDestinationRect(0, 0, params.region.width, params.region.height, verts);

		float tmp[2] = { 1.0f / srcTex->getWidth(), 1.0f / srcTex->getHeight() };
		params.shader->setVector2(params.shader->getConstantLocation("pixel_size"), tmp);

		params.source = srcTex;
		params.target = params.temporary1RT;
	}
	else if (passno == params.num_passes - 1) // Lighting pass
	{
		VEGA3dTexture* tmpTex = params.temporary1;
		setupSourceRect(0, 0, params.region.width, params.region.height, tmpTex, verts);
		setupDestinationRect(params.region.dx, params.region.dy, params.region.width, params.region.height,
							 verts);

		// And now for the unorthodox part...
		// Create a new shader and ditch the old one
		VEGA3dShaderProgram* spot_shader = NULL;
		RETURN_IF_ERROR(params.device->createShaderProgram(&spot_shader, VEGA3dShaderProgram::SHADER_LIGHTING_SPOTLIGHT, VEGA3dShaderProgram::WRAP_CLAMP_CLAMP));

		VEGARefCount::DecRef(params.shader);
		params.shader = spot_shader;

		// And now do the same for the vertex layout
		VEGA3dVertexLayout* vlayout;
		RETURN_IF_ERROR(params.device->createVega2dVertexLayout(&vlayout, params.shader->getShaderType()));
		VEGARefCount::DecRef(params.vtxlayout);
		params.vtxlayout = vlayout;

		params.device->setShaderProgram(spot_shader);

		setupLightParams(spot_shader, tmpTex);

		params.source = params.temporary1;
		params.target = params.destination;
	}

	return params.vtxbuf->writeAnywhere(4, sizeof(VEGA3dDevice::Vega2dVertex), verts, params.startIndex);
}

OP_STATUS VEGAFilterLighting::applyRecursive(VEGAMultiPassParamsLight& params)
{
	OP_STATUS err = OpStatus::OK;
	VEGA3dDevice* device = params.device;

	device->setShaderProgram(params.shader);

	for (unsigned i = 0; i < params.num_passes; ++i)
	{
		err = preparePass(i, params);
		if (OpStatus::IsError(err))
			break;

		device->setTexture(0, params.source);
		device->setRenderTarget(params.target);
		params.shader->setOrthogonalProjection();

		err = device->drawPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE_STRIP, params.vtxlayout, params.startIndex, 4);
	}

	return err;
}

OP_STATUS VEGAFilterLighting::applySpot(VEGABackingStore_FBO* destStore, const VEGAFilterRegion& region, unsigned int frame)
{
	if (sourceRT->getType() != VEGA3dRenderTarget::VEGA3D_RT_TEXTURE ||
		!static_cast<VEGA3dFramebufferObject*>(sourceRT)->getAttachedColorTexture())
		return OpStatus::ERR;

	VEGAMultiPassParamsLight params;
	params.region = region;
	params.num_passes = 2;

	params.device = g_vegaGlobals.vega3dDevice;
	params.shader = NULL;
	params.vtxbuf = NULL;
	params.vtxlayout = NULL;
	params.temporary1 = NULL;
	params.temporary1RT = NULL;
	params.destination = destStore->GetWriteRenderTarget(frame);

	params.temporary1 = params.device->getTempTexture(region.width, region.height);
	params.temporary1RT = params.device->getTempTextureRenderTarget();
	if (!params.temporary1 || !params.temporary1RT)
		return OpStatus::ERR;

	OP_STATUS err = params.device->createShaderProgram(&params.shader, VEGA3dShaderProgram::SHADER_LIGHTING_MAKE_BUMP, VEGA3dShaderProgram::WRAP_CLAMP_CLAMP);
	if (OpStatus::IsSuccess(err))
	{
		params.vtxbuf = params.device->getTempBuffer(4*sizeof(VEGA3dDevice::Vega2dVertex));
		if (!params.vtxbuf)
			err = OpStatus::ERR;
		else
			VEGARefCount::IncRef(params.vtxbuf);
		if (OpStatus::IsSuccess(err))
			err = params.device->createVega2dVertexLayout(&params.vtxlayout, params.shader->getShaderType());
		if (OpStatus::IsSuccess(err))
		{
			params.device->setRenderState(params.device->getDefault2dNoBlendNoScissorRenderState());
			VEGA3dDevice::Vega2dVertex verts[4];
			params.verts = verts;

			err = applyRecursive(params);
		}
	}

	// Free allocated resources
	VEGARefCount::DecRef(params.vtxlayout);
	VEGARefCount::DecRef(params.vtxbuf);

	VEGARefCount::DecRef(params.shader);

	return err;
}

OP_STATUS VEGAFilterLighting::apply(VEGABackingStore_FBO* destStore, const VEGAFilterRegion& region, unsigned int frame)
{
	OP_STATUS status;
	if (lsType == VEGALS_SPOT)
	{
		status = applySpot(destStore, region, frame);
	}
	else
	{
		status = applyShader(destStore, region, frame);
	}

	if (OpStatus::IsError(status))
		status = applyFallback(destStore, region);

	return status;
}
#endif // VEGA_3DDEVICE

enum EdgeFlags {
	TOP_EDGE = 0x1,
	BOTTOM_EDGE  = 0x2,
	LEFT_EDGE = 0x4,
	RIGHT_EDGE = 0x8
};

inline void VEGAFilterLighting::calcSurfaceNormal(VEGAConstPixelAccessor& iptr, unsigned int srcPixelStride,
												  int edge_flags,
												  VEGA_FIX* nx, VEGA_FIX* ny, VEGA_FIX* nz)
{
#define IPIX(ofs) ((int)VEGA_UNPACK_A(iptr.LoadRel(ofs)))
// Macros for relevant pixels:
// P00 P01 P02
// P10 P11 P12
// P20 P21 P22
#define P00 (IPIX(-1-(int)srcPixelStride))
#define P01 (IPIX(-(int)srcPixelStride))
#define P02 (IPIX(1-(int)srcPixelStride))
#define P10 (IPIX(-1))
#define P11 (IPIX(0))
#define P12 (IPIX(1))
#define P20 (IPIX(srcPixelStride-1))
#define P21 (IPIX(srcPixelStride))
#define P22 (IPIX(srcPixelStride+1))

	int sum_x, sum_y;
	int div_x, div_y;
	switch (edge_flags)
	{
	default:
	case 0: /* interior */
		sum_x = (P02 + 2*P12 + P22) - (P00 + 2*P10 + P20);
		sum_y = (P20 + 2*P21 + P22) - (P00 + 2*P01 + P02);
		div_x = div_y = 4*255;
		break;

	case (TOP_EDGE|LEFT_EDGE):
		sum_x = 2*((2*P12 + P22) - (2*P11 + P21));
		sum_y = 2*((2*P21 +	P22) - (2*P11 + P12));
		div_x = div_y = 3*255;
		break;
	case TOP_EDGE:
		sum_x = (2*P12 + P22) - (2*P10 + P20);
		sum_y = (P20 + 2*P21 + P22) - (P10 + 2*P11 + P12);
		div_x = 3*255;
		div_y = 2*255;
		break;
	case (TOP_EDGE|RIGHT_EDGE):
		sum_x = 2*((2*P11 + P21) - (2*P10 + P20));
		sum_y = 2*((2*P21 +	P20) - (2*P11 + P10));
		div_x = div_y = 3*255;
		break;

	case LEFT_EDGE:
		sum_x = (P02 + 2*P12 + P22) - (P01 + 2*P11 + P21);
		sum_y = (2*P21 + P22) - (2*P01 + P02);
		div_x = 2*255;
		div_y = 3*255;
		break;
	case RIGHT_EDGE:
		sum_x = (P01 + 2*P11 + P21) - (P00 + 2*P10 + P20);
		sum_y = (P20 + 2*P21) - (P00 + 2*P01);
		div_x = 2*255;
		div_y = 3*255;
		break;

	case (BOTTOM_EDGE|LEFT_EDGE):
		sum_x = 2*((P02 + 2*P12) - (P01 + 2*P11));
		sum_y = 2*((2*P11 + P12) - (2*P01 + P02));
		div_x = div_y = 3*255;
		break;
	case BOTTOM_EDGE:
		sum_x = (P02 + 2*P12) - (P00 + 2*P10);
		sum_y = (P10 + 2*P11 + P12) - (P00 + 2*P01 + P02);
		div_x = 3*255;
		div_y = 2*255;
		break;
	case (BOTTOM_EDGE|RIGHT_EDGE):
		sum_x = 2*((P01 + 2*P11) - (P00 + 2*P10));
		sum_y = 2*((P10 + 2*P11) - (P00 + 2*P01));
		div_x = div_y = 3*255;
		break;
	}
#undef P00
#undef P01
#undef P02
#undef P10
#undef P11
#undef P12
#undef P20
#undef P21
#undef P22
#undef IPIX
	*nx = -(surfaceScale * sum_x) / div_x;
	*ny = -(surfaceScale * sum_y) / div_y;

	/* not using normalize() here, because this is a special case */
	VEGA_FIX n_norm = VEGA_FIXDIV(VEGA_INTTOFIX(1),
								  VEGA_FIXSQRT(VEGA_FIXMUL(*nx, *nx) +
											   VEGA_FIXMUL(*ny, *ny) +
											   VEGA_INTTOFIX(1) /* nz^2 */));

	*nx = VEGA_FIXMUL(*nx, n_norm);
	*ny = VEGA_FIXMUL(*ny, n_norm);
	*nz = n_norm;
}

inline void VEGAFilterLighting::applySpotlight(VEGA_FIX l_dot_s,
											   int& lr, int& lg, int& lb)
{
	VEGA_FIX spot_fact;

	/* l_dot_s is negative */
	l_dot_s = -l_dot_s;

	if (has_cone_angle)
	{
		if (l_dot_s <= cos_falloff)
		{
			lr = lg = lb = 0;
			return;
		}
		else
		{
			// An alternative version might be to use a different attenuation factor
			// Ex: attenuation = 1 - (cos(limitingConeAngle) / (-L*S)) ^ 64
			spot_fact = VEGA_FIXPOW(l_dot_s, spot_spec_exp);

			if (l_dot_s < cos_cone_angle)
			{
				spot_fact = VEGA_FIXMULDIV(spot_fact,
										   l_dot_s - cos_falloff,
										   cos_cone_angle - cos_falloff);
			}
		}
	}
	else
	{
		spot_fact = VEGA_FIXPOW(l_dot_s, spot_spec_exp);
	}

	lr = VEGA_FIXTOINT(spot_fact * lr);
	lg = VEGA_FIXTOINT(spot_fact * lg);
	lb = VEGA_FIXTOINT(spot_fact * lb);
}

inline void normalize(VEGA_FIX& x, VEGA_FIX& y, VEGA_FIX& z)
{
	VEGA_FIX norm = VEGA_FIXMUL(x,x) + VEGA_FIXMUL(y,y) + VEGA_FIXMUL(z,z);

	if (norm == 0)
		return;
	// Fixpoint overflow check (|x| or |y| or |z| too large)
	if (norm < 0)
		norm = VEGA_INFINITY;

	norm = VEGA_FIXSQRT(norm);
	norm = VEGA_FIXDIV(VEGA_INTTOFIX(1), norm);

	x = VEGA_FIXMUL(x, norm);
	y = VEGA_FIXMUL(y, norm);
	z = VEGA_FIXMUL(z, norm);
}

OP_STATUS VEGAFilterLighting::apply(const VEGASWBuffer& dest, const VEGAFilterRegion& region)
{
	unsigned int xp, yp;
	VEGAConstPixelAccessor src = source.GetConstAccessor(region.sx, region.sy);
	VEGAPixelAccessor dst = dest.GetAccessor(region.dx, region.dy);

	unsigned int srcPixelStride = source.GetPixelStride();
	unsigned int dstPixelStride = dest.GetPixelStride();

	/* These only need to be calculated if the light source is a spotlight */
	VEGA_FIX spot_x = 0, spot_y = 0, spot_z = 0;

	if (lsType == VEGALS_SPOT)
	{
		spot_x = paX - light_x;
		spot_y = paY - light_y;
		spot_z = paZ - light_z;

		normalize(spot_x, spot_y, spot_z);
	}

	int base_lr = (light_color >> 16) & 0xff;
	int base_lg = (light_color >> 8) & 0xff;
	int base_lb = light_color & 0xff;

	int edge_flags;
	switch (lightingType)
	{
	case VEGALIGHTING_DIFFUSE:
		// Starting at the top edge
		edge_flags = TOP_EDGE;
		for (yp = 0; yp < region.height; ++yp, edge_flags = 0)
		{
			if (yp == region.height - 1)
				edge_flags |= BOTTOM_EDGE;

			// Starting at the left edge
			edge_flags |= LEFT_EDGE;
			for (xp = 0; xp < region.width; ++xp, edge_flags &= (TOP_EDGE | BOTTOM_EDGE))
			{
				VEGA_FIX nx, ny, nz;

				if (xp == region.width - 1)
					edge_flags |= RIGHT_EDGE;

				calcSurfaceNormal(src, srcPixelStride, edge_flags, &nx, &ny, &nz);

				VEGA_FIX lx, ly, lz;
				if (lsType == VEGALS_POINT || lsType == VEGALS_SPOT)
				{
					VEGA_FIX zval = surfaceScale * VEGA_UNPACK_A(src.Load()) / 255;

					lx = light_x - VEGA_INTTOFIX(xp);
					ly = light_y - VEGA_INTTOFIX(yp);
					lz = light_z - zval;

					/* Normalize light vector */
					normalize(lx, ly, lz);
				}
				else /* VEGALS_DISTANT */
				{
					/* Should be normalized already */
					lx = light_x;
					ly = light_y;
					lz = light_z;
				}

				VEGA_FIX n_dot_l = VEGA_FIXMUL(nx, lx) +
					VEGA_FIXMUL(ny, ly) + VEGA_FIXMUL(nz, lz);

				if (n_dot_l <= 0)
				{
					dst.StoreRGB(0, 0, 0);

					++src;
					++dst;
					continue;
				}

				int lr = base_lr;
				int lg = base_lg;
				int lb = base_lb;

				if (lsType == VEGALS_SPOT)
				{
					/* Spotlight calculations */
					VEGA_FIX l_dot_s = VEGA_FIXMUL(lx, spot_x) +
						VEGA_FIXMUL(ly, spot_y) + VEGA_FIXMUL(lz, spot_z);

					if (l_dot_s > 0)
					{
						dst.StoreRGB(0, 0, 0);

						++src;
						++dst;
						continue;
					}

					applySpotlight(l_dot_s, lr, lg, lb);
				}

				VEGA_FIX l_fact = VEGA_FIXMUL(kd, n_dot_l);
				lr = VEGA_FIXTOINT(l_fact * lr);
				lg = VEGA_FIXTOINT(l_fact * lg);
				lb = VEGA_FIXTOINT(l_fact * lb);

				// Clip color components
				lr = VEGA_CLAMP_U8(lr);
				lg = VEGA_CLAMP_U8(lg);
				lb = VEGA_CLAMP_U8(lb);

				dst.StoreRGB(lr, lg, lb);

				++src;
				++dst;
			}

			src += srcPixelStride - region.width;
			dst += dstPixelStride - region.width;
		}
		break;

	case VEGALIGHTING_SPECULAR:
		// Starting at the top edge
		edge_flags = TOP_EDGE;
		for (yp = 0; yp < region.height; ++yp, edge_flags = 0)
		{
			if (yp == region.height - 1)
				edge_flags |= BOTTOM_EDGE;

			// Starting at the left edge
			edge_flags |= LEFT_EDGE;
			for (xp = 0; xp < region.width; ++xp, edge_flags &= (TOP_EDGE | BOTTOM_EDGE))
			{
				VEGA_FIX nx, ny, nz;

				if (xp == region.width - 1)
					edge_flags |= RIGHT_EDGE;

				calcSurfaceNormal(src, srcPixelStride, edge_flags, &nx, &ny, &nz);

				VEGA_FIX lx, ly, lz;
				if (lsType == VEGALS_POINT || lsType == VEGALS_SPOT)
				{
					VEGA_FIX zval = surfaceScale * VEGA_UNPACK_A(src.Load()) / 255;

					lx = light_x - VEGA_INTTOFIX(xp);
					ly = light_y - VEGA_INTTOFIX(yp);
					lz = light_z - zval;

					/* Normalize light vector */
					normalize(lx, ly, lz);
				}
				else /* VEGALS_DISTANT */
				{
					/* Should be normalized already */
					lx = light_x;
					ly = light_y;
					lz = light_z;
				}

				// H = (L + E) / |(L + E)| where E = (0 0 1)
				VEGA_FIX hx = lx;
				VEGA_FIX hy = ly;
				VEGA_FIX hz = lz + VEGA_INTTOFIX(1);

				normalize(hx, hy, hz);

				VEGA_FIX n_dot_h = VEGA_FIXMUL(nx, hx) +
					VEGA_FIXMUL(ny, hy) + VEGA_FIXMUL(nz, hz);

				if (n_dot_h <= 0)
				{
					dst.Store(0);

					++src;
					++dst;
					continue;
				}

				int lr = base_lr;
				int lg = base_lg;
				int lb = base_lb;

				if (lsType == VEGALS_SPOT)
				{
					/* Spotlight calculations */
					VEGA_FIX l_dot_s = VEGA_FIXMUL(lx, spot_x) +
						VEGA_FIXMUL(ly, spot_y) + VEGA_FIXMUL(lz, spot_z);

					if (l_dot_s > 0)
					{
						dst.Store(0);

						++src;
						++dst;
						continue;
					}

					applySpotlight(l_dot_s, lr, lg, lb);
				}

				VEGA_FIX l_fact = VEGA_FIXMUL(ks, VEGA_FIXPOW(n_dot_h, spec_exp));

				lr = VEGA_FIXTOINT(l_fact * lr);
				lg = VEGA_FIXTOINT(l_fact * lg);
				lb = VEGA_FIXTOINT(l_fact * lb);

				// Clip color components
				lr = VEGA_CLAMP_U8(lr);
				lg = VEGA_CLAMP_U8(lg);
				lb = VEGA_CLAMP_U8(lb);

				int da = MAX(lr, MAX(lg, lb));

#ifdef USE_PREMULTIPLIED_ALPHA
				lr = (lr * (da + 1)) >> 8;
				lg = (lg * (da + 1)) >> 8;
				lb = (lb * (da + 1)) >> 8;
#endif // USE_PREMULTIPLIED_ALPHA
				
				dst.StoreARGB(da, lr, lg, lb);

				++src;
				++dst;
			}

			src += srcPixelStride - region.width;
			dst += dstPixelStride - region.width;
		}
		break;
	}

	return OpStatus::OK;
}

#undef VEGA_CLAMP_U8

#endif // VEGA_SUPPORT
