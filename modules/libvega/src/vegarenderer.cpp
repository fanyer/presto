/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef VEGA_SUPPORT

#include "modules/libvega/vegarenderer.h"
#include "modules/libvega/vegapath.h"
#include "modules/libvega/vegafill.h"
#include "modules/libvega/vegastencil.h"

#include "modules/libvega/src/vegagradient.h"
#include "modules/libvega/src/vegaimage.h"
#include "modules/libvega/src/vegarendertargetimpl.h"

#include "modules/libvega/src/vegafiltermerge.h"
#include "modules/libvega/src/vegafilterconvolve.h"
#include "modules/libvega/src/vegafiltermorphology.h"
#include "modules/libvega/src/vegafiltergaussian.h"
#include "modules/libvega/src/vegafilterlighting.h"
#include "modules/libvega/src/vegafiltercolortransform.h"
#include "modules/libvega/src/vegafilterdisplace.h"

#ifdef VEGA_OPPAINTER_SUPPORT
# include "modules/libvega/src/oppainter/vegaopbitmap.h"
# include "modules/libvega/src/oppainter/vegaopfont.h"

# include "modules/svg/svg_path.h"
#endif // VEGA_OPPAINTER_SUPPORT

#include "modules/pi/OpBitmap.h"

#include "modules/mdefont/processedstring.h"

#include "modules/libvega/src/vegabackend_sw.h"
#include "modules/libvega/src/vegabackend_hw2d.h"
#include "modules/libvega/src/vegabackend_hw3d.h"

#include "modules/libvega/vega3ddevice.h"
#ifdef VEGA_2DDEVICE
# include "modules/libvega/vega2ddevice.h"
#endif // VEGA_2DDEVICE

LibvegaModule::LibvegaModule() :
	vega3dDevice(NULL),
	vega2dDevice(NULL),
	rasterBackend(BACKEND_NONE)
#ifdef VEGA_USE_GRADIENT_CACHE
	, gradientCache(NULL)
#endif // VEGA_USE_GRADIENT_CACHE
#ifdef VEGA_OPPAINTER_SUPPORT
	, mainNativeWindow(NULL)
	, default_font(NULL)
	, default_serif_font(NULL)
	, default_sans_serif_font(NULL)
	, default_cursive_font(NULL)
	, default_fantasy_font(NULL)
	, default_monospace_font(NULL)
#endif // VEGA_OPPAINTER_SUPPORT
#ifdef VEGA_3DDEVICE
	, m_current_batcher(NULL)
	, shaderLocationsInitialized(false)
	, m_gradient_texture(NULL)
	, m_gradient_texture_next_line(0)
#endif // VEGA_3DDEVICE
#ifdef VEGA_USE_LINEDATA_POOL
	, linedata_pool(NULL)
#endif // VEGA_USE_LINEDATA_POOL
#ifdef VEGA_USE_ASM
    , dispatchTable(NULL)
#endif // VEGA_USE_ASM
{}

void LibvegaModule::InitL(const OperaInitInfo &info)
{
#ifdef VEGA_USE_ASM
	dispatchTable = OP_NEW_L(VEGADispatchTable, ());
#endif // VEGA_USE_ASM
}

void LibvegaModule::Destroy()
{
#ifdef VEGA_USE_GRADIENT_CACHE
	OP_DELETE(gradientCache);
#endif // VEGA_USE_GRADIENT_CACHE

#ifdef VEGA_3DDEVICE
	VEGARefCount::DecRef(m_gradient_texture);
#endif
#if defined(VEGA_3DDEVICE) || defined(CANVAS3D_SUPPORT)
	if (vega3dDevice)
		vega3dDevice->Destroy();
	OP_DELETE(vega3dDevice);
	vega3dDevice = NULL;
#endif // defined(VEGA_3DDEVICE) || defined(CANVAS3D_SUPPORT)

#ifdef VEGA_2DDEVICE
	if (vega2dDevice)
		vega2dDevice->Destroy();
	OP_DELETE(vega2dDevice);
	vega2dDevice = NULL;
#endif // VEGA_2DDEVICE

#ifdef VEGA_USE_LINEDATA_POOL
	VEGAPath::clearPool();
#endif // VEGA_USE_LINEDATA_POOL

#ifdef VEGA_USE_ASM
	OP_DELETE(dispatchTable);
#endif // VEGA_USE_ASM
}

#ifdef VEGA_3DDEVICE
# ifdef HW3D_COPY_LOC
#  error conflicting definition of HW3D_COPY_LOC
# endif // HW3D_COPY_LOC
# define HW3D_COPY_LOC(name) op_memcpy(name, this->name, VEGA3dShaderProgram::WRAP_MODE_COUNT*sizeof(int))
OP_STATUS LibvegaModule::getShaderLocations(int* worldProjMatrix2dLocationV,
                                            int* texTransSLocationV,
                                            int* texTransTLocationV,
                                            int* stencilCompBasedLocation,
                                            int* straightAlphaLocation)
{
	if (!shaderLocationsInitialized)
	{
		for (size_t i = 0; i < VEGA3dShaderProgram::WRAP_MODE_COUNT; ++i)
		{
			VEGA3dShaderProgram* prog;
			RETURN_IF_ERROR(vega3dDevice->createShaderProgram(&prog,
			                                                  VEGA3dShaderProgram::SHADER_VECTOR2D,
			                                                  (VEGA3dShaderProgram::WrapMode)i));
			this->worldProjMatrix2dLocationV[i] = prog->getConstantLocation("worldProjMatrix");
			VEGARefCount::DecRef(prog);

			VEGA3dShaderProgram* prog_tex;
			RETURN_IF_ERROR(vega3dDevice->createShaderProgram(&prog_tex,
			                                                  VEGA3dShaderProgram::SHADER_VECTOR2DTEXGEN,
			                                                  (VEGA3dShaderProgram::WrapMode)i));
			this->texTransSLocationV[i]       = prog_tex->getConstantLocation("texTransS");
			this->texTransTLocationV[i]       = prog_tex->getConstantLocation("texTransT");
			this->stencilCompBasedLocation[i] = prog_tex->getConstantLocation("stencilComponentBased");
			this->straightAlphaLocation[i]    = prog_tex->getConstantLocation("straightAlpha");
			VEGARefCount::DecRef(prog_tex);
		}
		shaderLocationsInitialized = true;
	}

	HW3D_COPY_LOC(worldProjMatrix2dLocationV);
	HW3D_COPY_LOC(texTransSLocationV);
	HW3D_COPY_LOC(texTransTLocationV);
	HW3D_COPY_LOC(stencilCompBasedLocation);
	HW3D_COPY_LOC(straightAlphaLocation);

	return OpStatus::OK;
}
# undef HW3D_COPY_LOC
#endif // VEGA_3DDEVICE

VEGARendererBackend* LibvegaModule::InstantiateRasterBackend(unsigned int w, unsigned int h, unsigned int q, unsigned int rasterBackend)
{
	VEGARendererBackend* backend = NULL;

	switch (rasterBackend)
	{
	case BACKEND_HW3D:
#ifdef VEGA_3DDEVICE
		backend = OP_NEW(VEGABackend_HW3D, ());
#endif // VEGA_3DDEVICE
		break;
	case BACKEND_HW2D:
#ifdef VEGA_2DDEVICE
		backend = OP_NEW(VEGABackend_HW2D, ());
#endif // VEGA_2DDEVICE
		break;
	case BACKEND_SW:
		backend = OP_NEW(VEGABackend_SW, ());
		break;
	default:
		OP_ASSERT(FALSE); // Nope, you didn't
	}

	if (!backend)
		return NULL;

	OP_STATUS status = backend->init(w, h, q);
	if (OpStatus::IsSuccess(status))
		return backend;

	OP_DELETE(backend);
	return NULL;
}

VEGARendererBackend* LibvegaModule::SelectRasterBackend(unsigned int w, unsigned int h, unsigned int q)
{
	// Try all configured backends in this order:
	// HW3D, HW2D, SW
	OP_ASSERT(rasterBackend == BACKEND_NONE);

	do
	{
		rasterBackend++;

		if (VEGARendererBackend* backend = InstantiateRasterBackend(w, h, q, rasterBackend))
			return backend;

	} while (rasterBackend != BACKEND_SW);

	return NULL;
}

VEGARendererBackend* LibvegaModule::CreateRasterBackend(unsigned int w, unsigned int h, unsigned int q, unsigned int forcedBackend)
{
	if (forcedBackend != BACKEND_NONE)
		return InstantiateRasterBackend(w, h, q, forcedBackend);
	else if (rasterBackend != BACKEND_NONE)
		return InstantiateRasterBackend(w, h, q, rasterBackend);
	else
		return SelectRasterBackend(w, h, q);
}


#ifdef VEGA_OPPAINTER_SUPPORT
# include "modules/about/operagpu.h"
// static
OP_STATUS VEGARenderer::GenerateBackendInfo(OperaGPU* gpu)
{
	OP_ASSERT(gpu);

	const bool using_2d =
# ifdef VEGA_2DDEVICE
		g_vegaGlobals.vega2dDevice && g_vegaGlobals.rasterBackend == LibvegaModule::BACKEND_HW2D;
# else  // VEGA_2DDEVICE
		false;
# endif // VEGA_2DDEVICE

	const bool using_3d =
# ifdef VEGA_3DDEVICE
		g_vegaGlobals.vega3dDevice && g_vegaGlobals.rasterBackend == LibvegaModule::BACKEND_HW3D;
# else  // VEGA_3DDEVICE
		false;
# endif // VEGA_3DDEVICE

	const bool using_hw = using_2d || using_3d;


# if defined(VEGA_2DDEVICE) || defined(VEGA_3DDEVICE) || defined(CANVAS3D_SUPPORT)
	// generic hw info:
	// * enabled/disabled
	// * active backend (if enabled)
	RETURN_IF_ERROR(gpu->Heading(OpStringC(UNI_L("Hardware acceleration status")), 1));
	RETURN_IF_ERROR(gpu->OpenDefinitionList());
	if (using_hw)
	{
		OP_ASSERT(using_2d != using_3d);
		RETURN_IF_ERROR(gpu->ListEntry(OpStringC(UNI_L("Hardware acceleration")), UNI_L("Enabled")));
# ifdef VEGA_2DDEVICE
		if (using_2d)
			RETURN_IF_ERROR(gpu->ListEntry(UNI_L("Graphics backend"), g_vegaGlobals.vega2dDevice->getDescription()));
		else
# endif // VEGA_2DDEVICE
# ifdef VEGA_3DDEVICE
		if (using_3d)
			RETURN_IF_ERROR(gpu->ListEntry(UNI_L("Graphics backend"), g_vegaGlobals.vega3dDevice->getDescription()));
		else
# endif // VEGA_3DDEVICE
			OP_ASSERT(!"unhandled backend, or multible backends active");
	}
	else
	{
		RETURN_IF_ERROR(gpu->ListEntry(OpStringC(UNI_L("Hardware acceleration")), UNI_L("Disabled")));
	}
	RETURN_IF_ERROR(gpu->CloseDefinitionList());
# endif // VEGA_2DDEVICE || VEGA_3DDEVICE || CANVAS3D_SUPPORT


# if defined(VEGA_3DDEVICE) || defined(CANVAS3D_SUPPORT)
	// any per-backend information (blocklist status, version strings, extensions, etc)
	return VEGA3dDevice::GenerateBackendInfo(gpu);
# else  // VEGA_3DDEVICE || CANVAS3D_SUPPORT
	if (!using_hw)
	{
		RETURN_IF_ERROR(gpu->OpenDefinitionList());
		RETURN_IF_ERROR(gpu->ListEntry(UNI_L("Graphics backend"), UNI_L("Software")));
		RETURN_IF_ERROR(gpu->CloseDefinitionList());
	}

	return OpStatus::OK;
# endif // VEGA_3DDEVICE || CANVAS3D_SUPPORT
}
#endif // VEGA_OPPAINTER_SUPPORT


VEGARenderer::VEGARenderer() : backend(NULL) {}

VEGARenderer::~VEGARenderer()
{
	if (backend)
	{
		backend->unbind();

		if (backend->renderTarget)
			backend->renderTarget->OnUnbind();
	}

	OP_DELETE(backend);
}

VEGARendererBackend::VEGARendererBackend() :
	renderer(NULL), width(0), height(0),
	cliprect_sx(0), cliprect_ex(0), cliprect_sy(0), cliprect_ey(0),
	xorFill(false), quality(VEGA_DEFAULT_QUALITY),
	r_minx(0), r_miny(0), r_maxx(0), r_maxy(0),
	renderTarget(NULL), has_image_funcs(false),
	supports_image_tiling(false)
{
	fillstate.fill = NULL;
	fillstate.color = 0;
	fillstate.ppixel = 0;
	fillstate.alphaBlend = true;
}

OP_STATUS VEGARenderer::Init(unsigned int w, unsigned int h, unsigned int q, unsigned int forcedBackend)
{
	if (!backend)
	{
		backend = g_vegaGlobals.CreateRasterBackend(w, h, q, forcedBackend);
		if (!backend)
			return OpStatus::ERR_NO_MEMORY;

		backend->renderer = this;
	}
	else
	{
		RETURN_IF_ERROR(backend->init(w, h, q));
	}

	backend->quality = q;
	backend->width = w;
	backend->height = h;

	backend->unbind();

	if (backend->renderTarget)
		backend->renderTarget->OnUnbind();

	backend->renderTarget = NULL;

	backend->setClipRect(0, 0, (int)w, (int)h);

	return OpStatus::OK;
}

#ifdef VEGA_3DDEVICE
unsigned int VEGARenderer::getNumberOfSamples()
{
	if (g_vegaGlobals.rasterBackend != LibvegaModule::BACKEND_HW3D)
		return 0;

	return MAX(1, MIN(VEGABackend_HW3D::qualityToSamples(backend->quality), g_vegaGlobals.vega3dDevice->getMaxQuality()));
}
#endif // VEGA_3DDEVICE

OP_STATUS VEGARenderer::setQuality(unsigned int q)
{
	backend->quality = q;

	return backend->updateQuality();
}

void VEGARenderer::flush(const OpRect* update_rects, unsigned int num_rects)
{
	if (!backend->renderTarget)
		return;

	backend->flush(update_rects, num_rects);
}

void VEGARendererBackend::flush(const OpRect* update_rects, unsigned int num_rects)
{
	renderTarget->flush(update_rects, num_rects);
}

OP_STATUS VEGARenderer::createBitmapRenderTarget(VEGARenderTarget** rt, OpBitmap* bmp)
{
	*rt = NULL;

	if (!bmp || (bmp->Width() > backend->width || bmp->Height() > backend->height))
		return OpStatus::ERR;

	return backend->createBitmapRenderTarget(rt, bmp);
}

#ifdef VEGA_OPPAINTER_SUPPORT
OP_STATUS VEGARenderer::createWindowRenderTarget(VEGARenderTarget** rt, VEGAWindow* window)
{
	*rt = NULL;

	return backend->createWindowRenderTarget(rt, window);
}

OP_STATUS VEGARenderer::createBufferRenderTarget(VEGARenderTarget** rt, VEGAPixelStore* ps)
{
	*rt = NULL;

	return backend->createBufferRenderTarget(rt, ps);
}
#endif // VEGA_OPPAINTER_SUPPORT

OP_STATUS VEGARenderer::createIntermediateRenderTarget(VEGARenderTarget** rt, unsigned int w, unsigned int h)
{
	if (!w)
		w = backend->width;
	if (!h)
		h = backend->height;

	*rt = NULL;

	return backend->createIntermediateRenderTarget(rt, w, h);
}

OP_STATUS VEGARenderer::createStencil(VEGAStencil** sten, bool component, unsigned int w, unsigned int h)
{
	if (!w)
		w = backend->width;
	if (!h)
		h = backend->height;

	*sten = NULL;

	return backend->createStencil(sten, component, w, h);
}

OP_STATUS VEGARenderer::createLinearGradient(VEGAFill** fill,
											 VEGA_FIX x1, VEGA_FIX y1, VEGA_FIX x2, VEGA_FIX y2,
											 unsigned int numStops, VEGA_FIX* stopOffsets,
											 UINT32* stopColors, bool premultiplied)
{
	*fill = NULL;

	VEGAGradient* grad = OP_NEW(VEGAGradient, ());
	if (!grad)
		return OpStatus::ERR_NO_MEMORY;

	if (numStops > VEGAGradient::MAX_STOPS)
		numStops = VEGAGradient::MAX_STOPS;

	OP_STATUS status = grad->initLinear(numStops, x1, y1, x2, y2, premultiplied);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(grad);
		return status;
	}
	for (unsigned int i = 0; i < numStops; ++i)
	{
		grad->setStop(i, stopOffsets[i], stopColors[i]);
	}
	*fill = grad;
	return OpStatus::OK;
}

OP_STATUS VEGARenderer::createRadialGradient(VEGAFill** fill,
											 VEGA_FIX x0, VEGA_FIX y0, VEGA_FIX r0,
											 VEGA_FIX x1, VEGA_FIX y1, VEGA_FIX r1,
											 unsigned int numStops, VEGA_FIX* stopOffsets,
											 UINT32* stopColors, bool premultiplied)
{
	*fill = NULL;

	VEGAGradient* grad = OP_NEW(VEGAGradient, ());
	if (!grad)
		return OpStatus::ERR_NO_MEMORY;

	if (numStops > VEGAGradient::MAX_STOPS)
		numStops = VEGAGradient::MAX_STOPS;

	OP_STATUS status = grad->initRadial(numStops, x1, y1, r1, x0, y0, r0, premultiplied);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(grad);
		return status;
	}
	for (unsigned int i = 0; i < numStops; ++i)
	{
		grad->setStop(i, stopOffsets[i], stopColors[i]);
	}
	*fill = grad;
	return OpStatus::OK;
}

OP_STATUS VEGARenderer::createImage(VEGAFill** fill, OpBitmap* bmp)
{
	*fill = NULL;

#if defined VEGA_USE_HW
	VEGAOpBitmap* vbmp = static_cast<VEGAOpBitmap*>(bmp);
# ifdef VEGA_LIMIT_BITMAP_SIZE
	if (vbmp->isTiled())
	{
		OP_ASSERT(!"this operation is not allowed on tiled bitmaps, check calling code");
		return OpStatus::ERR; // better than crashing
	}
# endif // VEGA_LIMIT_BITMAP_SIZE
	if (!backend->supportsStore(vbmp->GetBackingStore()))
		RETURN_IF_ERROR(vbmp->Migrate());

	OP_ASSERT(backend->supportsStore(vbmp->GetBackingStore()));
#endif // VEGA_USE_HW

	VEGAImage* img = OP_NEW(VEGAImage, ());
	if (!img)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = img->init(bmp);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(img);
		return status;
	}
	*fill = img;
	return OpStatus::OK;
}

#ifdef CANVAS3D_SUPPORT
#include "modules/libvega/vega3ddevice.h"

OP_STATUS VEGARenderer::createImage(VEGAFill** fill, VEGA3dTexture* tex, bool isPremultiplied)
{
	*fill = NULL;
	VEGAImage* img = OP_NEW(VEGAImage, ());
	if (!img)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = backend->createImage(img, tex, isPremultiplied);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(img);
		return status;
	}
	*fill = img;
	return OpStatus::OK;
}
#endif // CANVAS3D_SUPPORT

OP_STATUS VEGARenderer::createMergeFilter(VEGAFilter** filter, VEGAMergeType type)
{
	*filter = NULL;

	VEGAFilterMerge* mfilter = OP_NEW(VEGAFilterMerge, (type));
	if (!mfilter)
		return OpStatus::ERR_NO_MEMORY;

	*filter = mfilter;
	return OpStatus::OK;
}

OP_STATUS VEGARenderer::createOpacityMergeFilter(VEGAFilter** filter, unsigned char opacity)
{
	RETURN_IF_ERROR(createMergeFilter(filter, VEGAMERGE_OPACITY));
	((VEGAFilterMerge*)*filter)->setOpacity(opacity);
	return OpStatus::OK;
}

OP_STATUS VEGARenderer::createArithmeticMergeFilter(VEGAFilter** filter, VEGA_FIX k1, VEGA_FIX k2, VEGA_FIX k3, VEGA_FIX k4)
{
	RETURN_IF_ERROR(createMergeFilter(filter, VEGAMERGE_ARITHMETIC));
	((VEGAFilterMerge*)*filter)->setArithmeticFactors(k1, k2, k3, k4);
	return OpStatus::OK;
}

OP_STATUS VEGARenderer::createConvolveFilter(VEGAFilter** filter, VEGA_FIX* kernel, unsigned int kernelWidth, unsigned int kernelHeight,
		unsigned int kernelCenterX, unsigned int kernelCenterY, VEGA_FIX divisor, VEGA_FIX bias, VEGAFilterEdgeMode edgeMode,
		bool preserveAlpha)
{
	*filter = NULL;

	VEGAFilterConvolve* cfilter = OP_NEW(VEGAFilterConvolve, ());
	if (!cfilter)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = cfilter->setKernel(kernel, kernelWidth, kernelHeight);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(cfilter);
		return status;
	}
	cfilter->setKernelCenter(kernelCenterX, kernelCenterY);
	cfilter->setDivisor(divisor);
	cfilter->setBias(bias);
	cfilter->setEdgeMode(edgeMode);
	cfilter->setPreserveAlpha(preserveAlpha);

	*filter = cfilter;
	return OpStatus::OK;
}

OP_STATUS VEGARenderer::createMorphologyFilter(VEGAFilter** filter, VEGAMorphologyType type, int radiusX, int radiusY, bool wrap)
{
	*filter = NULL;

	VEGAFilterMorphology* mfilter = OP_NEW(VEGAFilterMorphology, (type));
	if (!mfilter)
		return OpStatus::ERR_NO_MEMORY;

	mfilter->setRadius(radiusX, radiusY);
	mfilter->setWrap(wrap);

	*filter = mfilter;
	return OpStatus::OK;
}

OP_STATUS VEGARenderer::createGaussianFilter(VEGAFilter** filter, VEGA_FIX deviationX, VEGA_FIX deviationY, bool wrap)
{
	*filter = NULL;

	VEGAFilterGaussian* gfilter = OP_NEW(VEGAFilterGaussian, ());
	if (!gfilter)
		return OpStatus::ERR_NO_MEMORY;

	gfilter->setStdDeviation(deviationX, deviationY);
	gfilter->setWrap(wrap);

	*filter = gfilter;
	return OpStatus::OK;
}

OP_STATUS VEGARenderer::createDistantLightFilter(VEGAFilter** filter, const VEGALightParameter& type,
		VEGA_FIX xdir, VEGA_FIX ydir, VEGA_FIX zdir)
{
	*filter = NULL;

	VEGAFilterLighting* lfilter = OP_NEW(VEGAFilterLighting, (type.type));
	if (!lfilter)
		return OpStatus::ERR_NO_MEMORY;

	lfilter->setDistantLight(xdir, ydir, zdir);
	lfilter->setSurfaceScale(type.surfaceScale);
	lfilter->setLightColor(type.color);
	if (type.type == VEGALIGHTING_DIFFUSE)
		lfilter->setDiffuseParams(type.constant);
	else
		lfilter->setSpecularParams(type.constant, type.exponent);

	*filter = lfilter;
	return OpStatus::OK;
}

OP_STATUS VEGARenderer::createPointLightFilter(VEGAFilter** filter, const VEGALightParameter& type,
		VEGA_FIX xpos, VEGA_FIX ypos, VEGA_FIX zpos)
{
	*filter = NULL;

	VEGAFilterLighting* lfilter = OP_NEW(VEGAFilterLighting, (type.type));
	if (!lfilter)
		return OpStatus::ERR_NO_MEMORY;

	lfilter->setPointLight(xpos, ypos, zpos);
	lfilter->setSurfaceScale(type.surfaceScale);
	lfilter->setLightColor(type.color);
	if (type.type == VEGALIGHTING_DIFFUSE)
		lfilter->setDiffuseParams(type.constant);
	else
		lfilter->setSpecularParams(type.constant, type.exponent);

	*filter = lfilter;
	return OpStatus::OK;
}

OP_STATUS VEGARenderer::createSpotLightFilter(VEGAFilter** filter, const VEGALightParameter& type,
		VEGA_FIX xpos, VEGA_FIX ypos, VEGA_FIX zpos,
		VEGA_FIX xtgt, VEGA_FIX ytgt, VEGA_FIX ztgt,
		VEGA_FIX spotSpecExp, VEGA_FIX angle)
{
	*filter = NULL;

	VEGAFilterLighting* lfilter = OP_NEW(VEGAFilterLighting, (type.type));
	if (!lfilter)
		return OpStatus::ERR_NO_MEMORY;

	lfilter->setSpotLight(xpos, ypos, zpos, xtgt, ytgt, ztgt, spotSpecExp, angle<VEGA_INTTOFIX(360), angle);
	lfilter->setSurfaceScale(type.surfaceScale);
	lfilter->setLightColor(type.color);
	if (type.type == VEGALIGHTING_DIFFUSE)
		lfilter->setDiffuseParams(type.constant);
	else
		lfilter->setSpecularParams(type.constant, type.exponent);

	*filter = lfilter;
	return OpStatus::OK;
}

OP_STATUS VEGARenderer::createLuminanceToAlphaFilter(VEGAFilter** filter)
{
	*filter = NULL;

	VEGAFilterColorTransform* ctfilter = OP_NEW(VEGAFilterColorTransform,
												(VEGACOLORTRANSFORM_LUMINANCETOALPHA));
	if (!ctfilter)
		return OpStatus::ERR_NO_MEMORY;

	*filter = ctfilter;
	return OpStatus::OK;
}

OP_STATUS VEGARenderer::createMatrixColorTransformFilter(VEGAFilter** filter, VEGA_FIX* &matrix)
{
	*filter = NULL;

	VEGAFilterColorTransform* ctfilter = OP_NEW(VEGAFilterColorTransform,
												(VEGACOLORTRANSFORM_MATRIX, matrix));
	if (!ctfilter)
		return OpStatus::ERR_NO_MEMORY;

	if (!matrix)
		matrix = ctfilter->getMatrix();

	*filter = ctfilter;
	return OpStatus::OK;
}

static void SetupComponentTransfer(VEGAFilterColorTransform* ctfilter,
										  VEGAComponent comp, VEGACompFuncType func,
										  VEGA_FIX* table, unsigned int table_size)
{
	switch (func)
	{
	case VEGACOMPFUNC_TABLE:
		ctfilter->setCompTable(comp, table, table_size);
		break;
	case VEGACOMPFUNC_DISCRETE:
		ctfilter->setCompDiscrete(comp, table, table_size);
		break;
	case VEGACOMPFUNC_LINEAR:
		OP_ASSERT(table_size == 2);
		ctfilter->setCompLinear(comp, table[0], table[1]);
		break;
	case VEGACOMPFUNC_GAMMA:
		OP_ASSERT(table_size == 3);
		ctfilter->setCompGamma(comp, table[0], table[1], table[2]);
		break;
	default:
	case VEGACOMPFUNC_IDENTITY:
		ctfilter->setCompIdentity(comp);
		break;
	}
}
OP_STATUS VEGARenderer::createComponentColorTransformFilter(VEGAFilter** filter,
		VEGACompFuncType red_type, VEGA_FIX* red_table, unsigned int red_table_size,
		VEGACompFuncType green_type, VEGA_FIX* green_table, unsigned int green_table_size,
		VEGACompFuncType blue_type, VEGA_FIX* blue_table, unsigned int blue_table_size,
		VEGACompFuncType alpha_type, VEGA_FIX* alpha_table, unsigned int alpha_table_size)
{
	*filter = NULL;

	VEGAFilterColorTransform* ctfilter = OP_NEW(VEGAFilterColorTransform,
												(VEGACOLORTRANSFORM_COMPONENTTRANSFER));
	if (!ctfilter)
		return OpStatus::ERR_NO_MEMORY;

	SetupComponentTransfer(ctfilter, VEGACOMP_R, red_type, red_table, red_table_size);
	SetupComponentTransfer(ctfilter, VEGACOMP_G, green_type, green_table, green_table_size);
	SetupComponentTransfer(ctfilter, VEGACOMP_B, blue_type, blue_table, blue_table_size);
	SetupComponentTransfer(ctfilter, VEGACOMP_A, alpha_type, alpha_table, alpha_table_size);

	*filter = ctfilter;
	return OpStatus::OK;
}

OP_STATUS VEGARenderer::createColorSpaceConversionFilter(VEGAFilter** filter,
														 VEGAColorSpace from, VEGAColorSpace to)
{
	*filter = NULL;

	if ((from == VEGACOLORSPACE_SRGB && to == VEGACOLORSPACE_LINRGB) ||
		(from == VEGACOLORSPACE_LINRGB && to == VEGACOLORSPACE_SRGB))
	{
		VEGAFilterColorTransform* ctfilter = OP_NEW(VEGAFilterColorTransform,
													(VEGACOLORTRANSFORM_COLORSPACECONV));
		if (!ctfilter)
			return OpStatus::ERR_NO_MEMORY;

		if (from == VEGACOLORSPACE_SRGB && to == VEGACOLORSPACE_LINRGB)
			ctfilter->setColorSpaceConversion(VEGACSCONV_SRGB_TO_LINRGB);
		else if (from == VEGACOLORSPACE_LINRGB && to == VEGACOLORSPACE_SRGB)
			ctfilter->setColorSpaceConversion(VEGACSCONV_LINRGB_TO_SRGB);

		*filter = ctfilter;
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

OP_STATUS VEGARenderer::createDisplaceFilter(VEGAFilter** filter, VEGARenderTarget* displaceMap,
											 VEGA_FIX scale, VEGAComponent xComp, VEGAComponent yComp)
{
	*filter = NULL;
	VEGAFilterDisplace* dfilter = OP_NEW(VEGAFilterDisplace, ());
	if (!dfilter)
		return OpStatus::ERR_NO_MEMORY;

	dfilter->setDisplacementMap(displaceMap->GetBackingStore());
	dfilter->setScale(scale);

	switch (xComp)
	{
	case VEGACOMP_R:
		dfilter->setDispInX(VEGAFilterDisplace::COMP_R);
		break;
	case VEGACOMP_G:
		dfilter->setDispInX(VEGAFilterDisplace::COMP_G);
		break;
	case VEGACOMP_B:
		dfilter->setDispInX(VEGAFilterDisplace::COMP_B);
		break;
	default:
	case VEGACOMP_A:
		dfilter->setDispInX(VEGAFilterDisplace::COMP_A);
		break;
	}
	switch (yComp)
	{
	case VEGACOMP_R:
		dfilter->setDispInY(VEGAFilterDisplace::COMP_R);
		break;
	case VEGACOMP_G:
		dfilter->setDispInY(VEGAFilterDisplace::COMP_G);
		break;
	case VEGACOMP_B:
		dfilter->setDispInY(VEGAFilterDisplace::COMP_B);
		break;
	default:
	case VEGACOMP_A:
		dfilter->setDispInY(VEGAFilterDisplace::COMP_A);
		break;
	}

	*filter = dfilter;
	return OpStatus::OK;
}

void VEGARenderer::setRenderTarget(VEGARenderTarget* rt)
{
	if (!backend)
		return;

	if (!rt || rt->getWidth() > backend->width || rt->getHeight() > backend->height)
		rt = NULL;

	if (backend->renderTarget == rt)
		return;

	backend->unbind();

	if (backend->renderTarget)
		backend->renderTarget->OnUnbind();

	if (rt && backend->bind(rt))
	{
		backend->renderTarget = rt;
		backend->renderTarget->OnBind(this);
	}
	else
	{
		backend->renderTarget = NULL;
	}
}

// FIXME: could use same code as fillrect
void VEGARenderer::clear(int x, int y, unsigned int w, unsigned int h, unsigned int color, VEGATransform* transform)
{
	if (!backend->renderTarget)
		return;

	backend->clear(x, y, w, h, color, transform);
}

void VEGARendererBackend::setClipRect(int x, int y, int w, int h)
{
	cliprect_sx = x;
	cliprect_sy = y;
	cliprect_ex = x + w;
	cliprect_ey = y + h;

	if (cliprect_sx < 0)
		cliprect_sx = 0;
	if (cliprect_ex > (int)width)
		cliprect_ex = width;
	if (cliprect_sy < 0)
		cliprect_sy = 0;
	if (cliprect_ey > (int)height)
		cliprect_ey = height;
}

void VEGARendererBackend::getClipRect(int &x, int &y, int &w, int &h)
{
	x = cliprect_sx;
	y = cliprect_sy;
	w = cliprect_ex - cliprect_sx;
	h = cliprect_ey - cliprect_sy;
}

void VEGARendererBackend::setColor(unsigned long col)
{
	fillstate.color = (UINT32)col;
	fillstate.ppixel = ColorToVEGAPixel(fillstate.color);
	fillstate.fill = NULL;
}

void VEGARendererBackend::setFill(VEGAFill *f)
{
	fillstate.fill = f;
}

OP_STATUS VEGARenderer::applyFilter(VEGAFilter* filter, VEGAFilterRegion& region)
{
	if (!backend->renderTarget)
		return OpStatus::ERR;

	RETURN_IF_ERROR(filter->checkRegion(region, backend->renderTarget->getWidth(), backend->renderTarget->getHeight()));

	OP_STATUS status = backend->applyFilter(filter, region);

	backend->renderTarget->markDirty(region.dx, region.dx + region.width - 1,
									 region.dy, region.dy + region.height - 1);
	return status;
}

#ifdef VEGA_OPPAINTER_SUPPORT
#ifdef SVG_PATH_TO_VEGAPATH
OP_STATUS VEGARenderer::drawTransformedString(VEGAFont* font, int x, int y,
											  const uni_char* _text, UINT32 len, INT32 extra_char_space, short adjust,
											  VEGATransform* transform, VEGAStencil* stencil)
{
	OP_ASSERT(transform || font->forceOutlinePath());
	OP_ASSERT(backend->renderTarget &&
			  backend->cliprect_ex > backend->cliprect_sx && backend->cliprect_ey > backend->cliprect_sy);

	// FIXME: Ignoring <adjust> for now.
	// FIXME: This should be made into a backend-specific operation,
	// with a generic fallback (which is essentially this one)

	SVGNumber advance;
	UINT32 curr_pos = 0;
	if (font->GetOutline(_text, len, curr_pos, curr_pos, TRUE, advance, NULL) == OpStatus::ERR_NOT_SUPPORTED)
		return OpStatus::ERR; // Sorry, no outlines...

	VEGATransform gtrans;
	gtrans.loadTranslate(VEGA_INTTOFIX(x), VEGA_INTTOFIX(y + (INT32)font->Ascent()));
	gtrans[4] = VEGA_INTTOFIX(-1); // Flip

	const VEGA_FIX flatness = VEGA_INTTOFIX(1) / 10; // Hardcoded for now...

	UINT32 last_pos = 0;
	while (curr_pos < len)
	{
		SVGPath* outline = NULL;
		UINT32 prev_pos = curr_pos;

		// Fetch glyph (uncached, slow, et.c et.c)
		OP_STATUS status = font->GetOutline(_text, len, curr_pos, last_pos, TRUE,
											advance, &outline);
		if (OpStatus::IsError(status))
		{
			// This is a bit confused, but ERR_NOT_SUPPORTED in this
			// case means that there was no outline (according to
			// documentation in pi/OpFont.h)
			if (OpStatus::IsMemoryError(status) ||
				status == OpStatus::ERR_NOT_SUPPORTED)
				return status;

			curr_pos++;
			continue;
		}

		last_pos = prev_pos;

		// Draw glyph
		VEGAPath voutline;
		status = ConvertSVGPathToVEGAPath(outline, 0, 0, flatness, &voutline);

		OP_DELETE(outline);

		RETURN_IF_ERROR(status);

		VEGATransform trans_copy;
		if (transform)
			trans_copy.copy(*transform);
		else
			trans_copy.loadIdentity();
		trans_copy.multiply(gtrans);

		voutline.transform(&trans_copy);

		fillPath(&voutline, stencil);

		gtrans[2] += advance.GetVegaNumber() + VEGA_INTTOFIX(extra_char_space);
	}

	return OpStatus::OK;
}
#endif // SVG_PATH_TO_VEGAPATH

OP_STATUS VEGARenderer::drawString(VEGAFont* font, int x, int y, const uni_char* _text, UINT32 len, INT32 extra_char_space, short adjust, VEGAStencil* stencil, VEGATransform* transform)
{
	if (!backend->renderTarget)
		return OpStatus::ERR;

	if (backend->cliprect_ex <= backend->cliprect_sx ||
		backend->cliprect_ey <= backend->cliprect_sy)
		return OpStatus::OK;

# ifdef SVG_PATH_TO_VEGAPATH
	if (transform || font->forceOutlinePath())
	{
		if (!transform || !transform->isIntegralTranslation())
			return drawTransformedString(font, x, y, _text, len, extra_char_space, adjust, transform, stencil);

		// No need to go down the transformed path for a simple translation.
		x += VEGA_TRUNCFIXTOINT((*transform)[2]);
		y += VEGA_TRUNCFIXTOINT((*transform)[5]);
	}
# endif // SVG_PATH_TO_VEGAPATH

#ifdef VEGA_NATIVE_FONT_SUPPORT
	if (font->nativeRendering())
	{
		return backend->drawString(font, x, y, _text, len, extra_char_space, adjust, stencil);
	}
	else
#endif // VEGA_NATIVE_FONT_SUPPORT
	{
		ProcessedString processed_string;
		RETURN_IF_ERROR(font->ProcessString(&processed_string, _text, len, extra_char_space, adjust));

		return backend->drawString(font, x, y, &processed_string, stencil);
	}
}
#endif // VEGA_OPPAINTER_SUPPORT

OP_STATUS VEGARenderer::moveRect(int x, int y, unsigned int width, unsigned int height, int dx, int dy)
{
	if (!backend->renderTarget)
		return OpStatus::ERR;

	return backend->moveRect(x, y, width, height, dx, dy);
}

void VEGARenderer::validateStencil(VEGAStencil*& stencil)
{
	if (!stencil)
		return;

	OP_ASSERT(stencil->getOffsetX() <= 0 && stencil->getOffsetY() <= 0);
	OP_ASSERT(stencil->getWidth() + stencil->getOffsetX() >= backend->renderTarget->getWidth());
	OP_ASSERT(stencil->getHeight() + stencil->getOffsetY() >= backend->renderTarget->getHeight());

	// Make sure the stencil cover the entire render target
	if (stencil->getOffsetX() > 0 || stencil->getOffsetY() > 0 ||
		(stencil->getWidth()+stencil->getOffsetX() < backend->renderTarget->getWidth()) ||
		(stencil->getHeight()+stencil->getOffsetY() < backend->renderTarget->getHeight()))
	{
		stencil = NULL;
	}
}

#ifdef VEGA_OPPAINTER_SUPPORT
OP_STATUS VEGARenderer::drawImageFallback(VEGAImage* img, const VEGADrawImageInfo& imginfo, VEGAStencil* stencil)
{
	// Setup fill
	VEGATransform trans, itrans;
	VEGAImage::calcTransforms(imginfo, trans, itrans);
	img->setTransform(trans, itrans);

	if (imginfo.type == VEGADrawImageInfo::REPEAT)
		img->setSpread(VEGAFill::SPREAD_REPEAT);
	else
		img->setSpread(VEGAFill::SPREAD_CLAMP);

	img->setFillOpacity(imginfo.opacity);
	img->setQuality(imginfo.quality);

	VEGAFill* old_fill = backend->fillstate.fill;
	backend->fillstate.fill = img;

	OP_STATUS status = backend->fillRect(imginfo.dstx, imginfo.dsty, imginfo.dstw, imginfo.dsth, stencil, true);

	backend->fillstate.fill = old_fill;
	return status;
}

OP_STATUS VEGARenderer::drawImage(VEGAOpBitmap* vbmp, const VEGADrawImageInfo& imginfo, VEGAStencil* stencil)
{
	if (!backend->renderTarget)
		return OpStatus::ERR;

	validateStencil(stencil);

	VEGAFill* fill = NULL;
	RETURN_IF_ERROR(vbmp->getFill(this, &fill));
	VEGAImage* img = static_cast<VEGAImage*>(fill);

	if (backend->has_image_funcs && !stencil)
		return backend->drawImage(img, imginfo);

	return drawImageFallback(img, imginfo, stencil);
}
#endif // VEGA_OPPAINTER_SUPPORT

OP_STATUS VEGARenderer::fillRect(int x, int y, unsigned int width, unsigned int height, VEGAStencil* stencil, bool blend)
{
	OP_ASSERT(blend || (stencil == NULL));

	if (!backend->renderTarget)
		return OpStatus::ERR;

	validateStencil(stencil);

	return backend->fillRect(x, y, width, height, stencil, blend);
}

OP_STATUS VEGARenderer::invertRect(int x, int y, unsigned int width, unsigned int height)
{
	if (!backend->renderTarget)
		return OpStatus::ERR;

	return backend->invertRect(x, y, width, height);
}

static OpRect GetShapeBBox(VEGAPath& p)
{
	VEGA_FIX bx, by, bw, bh;
	p.getBoundingBox(bx, by, bw, bh);

	int ib_minx = VEGA_TRUNCFIXTOINT(VEGA_FLOOR(bx));
	int ib_miny = VEGA_TRUNCFIXTOINT(VEGA_FLOOR(by));
	int ib_maxx = VEGA_TRUNCFIXTOINT(VEGA_CEIL(bx+bw));
	int ib_maxy = VEGA_TRUNCFIXTOINT(VEGA_CEIL(by+bh));

	return OpRect(ib_minx, ib_miny, ib_maxx - ib_minx, ib_maxy - ib_miny);
}

OP_STATUS VEGARenderer::invertPath(VEGAPath* path)
{
	if (!backend->renderTarget)
		return OpStatus::ERR;

	OpRect shape_rect = GetShapeBBox(*path);

	int sx = backend->cliprect_sx;
	int sy = backend->cliprect_sy;
	int ex = backend->cliprect_ex;
	int ey = backend->cliprect_ey;

	OpRect clip_rect(sx,sy, ex - sx, ey - sy);

	shape_rect.IntersectWith(clip_rect);
	if (shape_rect.IsEmpty())
		return OpStatus::OK;

	if (OpStatus::IsSuccess(backend->invertPath(path)))
		return OpStatus::OK;

	// Backend can't draw it, let's fallback
	VEGARenderTarget* old_rt = backend->renderTarget;
	// Make the renderer draw without anti-aliasing
	const unsigned int old_quality = backend->quality;
	RETURN_IF_ERROR(setQuality(0));

	VEGARenderTarget* tmp_rt;
	OP_STATUS status = createIntermediateRenderTarget(&tmp_rt, shape_rect.width, shape_rect.height);
	VEGAAutoRenderTarget _tmp_rt(tmp_rt);
	if (OpStatus::IsError(status))
	{
		RETURN_IF_ERROR(setQuality(old_quality));
		setRenderTarget(old_rt);
		return status;
	}

	// Copy affected region
	VEGAFilter* copy_filter;
	status = createMergeFilter(&copy_filter, VEGAMERGE_REPLACE);
	if (OpStatus::IsError(status))
	{
		RETURN_IF_ERROR(setQuality(old_quality));
		setRenderTarget(old_rt);
		return status;
	}

	VEGAFilterRegion region;
	region.sx = shape_rect.x;
	region.sy = shape_rect.y;
	region.dx = region.dy = 0;
	region.width = shape_rect.width;
	region.height = shape_rect.height;

	copy_filter->setSource(old_rt);
	setRenderTarget(tmp_rt);
	applyFilter(copy_filter, region);

	OP_DELETE(copy_filter);

	region.sx = region.sy = 0;

	// Clear
	setRenderTarget(old_rt);
	clear(shape_rect.x, shape_rect.y, shape_rect.width, shape_rect.height, 0);

	// Draw shape
	setColor(0xffffffff);
	fillPath(path);

	// Clear shape
	VEGAFilter* out_filter;
	status = createMergeFilter(&out_filter, VEGAMERGE_OUT);
	if (OpStatus::IsError(status))
	{
		RETURN_IF_ERROR(setQuality(old_quality));
		setRenderTarget(old_rt);
		return status;
	}

	region.dx = shape_rect.x;
	region.dy = shape_rect.y;

	out_filter->setSource(tmp_rt);
	applyFilter(out_filter, region);

	OP_DELETE(out_filter);

	// Invert
	VEGAFilter* inv_filter;
	VEGA_FIX invTab[] = {VEGA_INTTOFIX(-1), VEGA_INTTOFIX(1)};
	status = createComponentColorTransformFilter(&inv_filter,
	                                             VEGACOMPFUNC_LINEAR, invTab, 2,
	                                             VEGACOMPFUNC_LINEAR, invTab, 2,
	                                             VEGACOMPFUNC_LINEAR, invTab, 2,
	                                             VEGACOMPFUNC_IDENTITY, NULL, 0);
	if (OpStatus::IsError(status))
	{
		RETURN_IF_ERROR(setQuality(old_quality));
		setRenderTarget(old_rt);
		return status;
	}

	region.dx = region.dy = 0;

	inv_filter->setSource(tmp_rt);
	setRenderTarget(tmp_rt);
	applyFilter(inv_filter, region);

	OP_DELETE(inv_filter);

	// Paint shape with inverted buffer as fill
	setRenderTarget(old_rt);

	VEGAFill* inv_fill = NULL;
	status = tmp_rt->getImage(&inv_fill);
	if (OpStatus::IsSuccess(status))
	{
		VEGATransform trans, itrans;
		trans.loadTranslate(VEGA_INTTOFIX(shape_rect.x), VEGA_INTTOFIX(shape_rect.y));
		itrans.loadTranslate(VEGA_INTTOFIX(-shape_rect.x), VEGA_INTTOFIX(-shape_rect.y));

		inv_fill->setTransform(trans, itrans);

		setFill(inv_fill);
		fillPath(path);
		setFill(NULL);
	}

	// Reset to draw with default quality
	RETURN_IF_ERROR(setQuality(old_quality));
	setRenderTarget(old_rt);

	return status;
}



// Check if the path is simple enough to be filled using lesser
// means (fillRect, drawImage), and perform the operation if so.
// Return true if handled, false if not.
bool VEGARenderer::fillIfSimple(VEGAPath* path, VEGAStencil *stencil)
{
	// We handle rectangles - that's what we do. (FIXME: closed-test not strictly necessary)
	if (path->getNumLines() != 4 || !path->isClosed())
		return false;

	const VEGA_FIX* lines = path->getLine(0);

	VEGA_FIX prev_x = lines[VEGALINE_STARTX];
	VEGA_FIX prev_y = lines[VEGALINE_STARTY];

	if (!IsGridAligned(prev_x, prev_y))
		return false;

	// Move to next point
	lines += 2;

	int minx, miny, maxx, maxy;
	minx = maxx = VEGA_TRUNCFIXTOINT(prev_x);
	miny = maxy = VEGA_TRUNCFIXTOINT(prev_y);

	bool prev_horizontal = false;

	for (unsigned i = 0; i < 4; ++i)
	{
		if (path->isLineWarp(i))
			return false;

		VEGA_FIX curr_x = lines[VEGALINE_STARTX];
		VEGA_FIX curr_y = lines[VEGALINE_STARTY];

		if (!IsGridAligned(curr_x, curr_y))
			return false;

		bool is_horizontal = VEGA_EQ(curr_y - prev_y, 0);
		bool is_vertical = VEGA_EQ(curr_x - prev_x, 0);

		// Either or - not both
		if (!(is_horizontal ^ is_vertical))
			return false;

		// We expect every line to be orthogonal to the previous line.
		if (i != 0 && !(prev_horizontal ^ is_horizontal))
			return false;

		int icurr_x = VEGA_TRUNCFIXTOINT(curr_x);
		int icurr_y = VEGA_TRUNCFIXTOINT(curr_y);

		// Update "bounding box" (which should equal the shape)
		minx = MIN(minx, icurr_x);
		maxx = MAX(maxx, icurr_x);

		miny = MIN(miny, icurr_y);
		maxy = MAX(maxy, icurr_y);

		prev_x = curr_x;
		prev_y = curr_y;
		prev_horizontal = is_horizontal;

		// Next point
		lines += 2;
	}

	// Very small?
	if (maxx - minx == 0 || maxy - miny == 0)
		return false;

#ifdef VEGA_OPPAINTER_SUPPORT
	// If a fill is set and it is a VEGAImage - call drawImage if possible
	if (backend->has_image_funcs && !stencil &&
		backend->fillstate.fill && backend->fillstate.fill->isImage())
	{
		VEGADrawImageInfo diinfo;
		diinfo.dstx = minx;
		diinfo.dsty = miny;
		diinfo.dstw = maxx - minx;
		diinfo.dsth = maxy - miny;

		VEGAImage* image_fill = static_cast<VEGAImage*>(backend->fillstate.fill);
		if (image_fill->simplifiesToBlit(diinfo, backend->supports_image_tiling))
			return !!OpStatus::IsSuccess(backend->drawImage(image_fill, diinfo));
	}
#endif // VEGA_OPPAINTER_SUPPORT

	return !!OpStatus::IsSuccess(backend->fillRect(minx, miny, maxx - minx, maxy - miny, stencil, true));
}

OP_STATUS VEGARenderer::fillPath(VEGAPath *path, VEGAStencil *stencil)
{
	if (!backend->renderTarget)
		return OpStatus::ERR;

	validateStencil(stencil);

	if (path->getNumLines() == 0)
		return OpStatus::OK;

	// We currently only attempt to do simplification when the
	// rendertarget is a color-buffer (because the fillRect operation
	// does not really handle other rendertargets at the moment).
	if (backend->renderTarget->getColorFormat() == VEGARenderTarget::RT_RGBA32 &&
		fillIfSimple(path, stencil))
		return OpStatus::OK;

	return backend->fillPath(path, stencil);
}

void VEGARendererBackend::clearTransformed(int x, int y, unsigned int w, unsigned int h,
										   unsigned int color, VEGATransform* transform)
{
	// FIXME: far from optimal
	FillState saved_fillstate = fillstate; // Save current fill state

	setColor(color);
	fillstate.alphaBlend = false;

	VEGAPath p;
	RETURN_VOID_IF_ERROR(p.moveTo(VEGA_INTTOFIX(x), VEGA_INTTOFIX(y)));
	RETURN_VOID_IF_ERROR(p.lineTo(VEGA_INTTOFIX(x+(int)w), VEGA_INTTOFIX(y)));
	RETURN_VOID_IF_ERROR(p.lineTo(VEGA_INTTOFIX(x+(int)w), VEGA_INTTOFIX(y+(int)h)));
	RETURN_VOID_IF_ERROR(p.lineTo(VEGA_INTTOFIX(x), VEGA_INTTOFIX(y+(int)h)));
	RETURN_VOID_IF_ERROR(p.close(true));
#ifdef VEGA_3DDEVICE
	p.forceConvex();
#endif

	if (transform)
		p.transform(transform);

	fillPath(&p, NULL);

	fillstate = saved_fillstate;
}

VEGARendererBackend::FillOpacityState VEGARendererBackend::saveFillOpacity()
{
	FillOpacityState saved;

	// Save relevant parts of fillstate
	if (fillstate.fill)
	{
		saved.fillopacity = fillstate.fill->getFillOpacity();
		saved.color = 0; // Shouldn't be used, just to silence warning
	}
	else
	{
		saved.fillopacity = 0; // Shouldn't be used, just to silence warning
		saved.color = fillstate.color;
	}

	return saved;
}

void VEGARendererBackend::restoreFillOpacity(const FillOpacityState& saved)
{
	// Restore relevant parts of fillstate
	if (fillstate.fill)
		fillstate.fill->setFillOpacity(saved.fillopacity);
	else
		setColor(saved.color);
}

void VEGARendererBackend::emitRect(FractRect* rects, unsigned& num_rects,
								   int x, int y, unsigned w, unsigned h, unsigned weight)
{
	rects[num_rects].x = x;
	rects[num_rects].y = y;
	rects[num_rects].w = w;
	rects[num_rects].h = h;
	rects[num_rects].weight = (UINT8)weight;

	num_rects++;
}

// Emit rectangles for a span [x, x+w] * [y, y+h] with an area based on [miny, maxy]
void VEGARendererBackend::emitFractionalSpan(FractRect* rects, unsigned& num_rects,
											 VEGA_FIX x, int y, VEGA_FIX w, int h,
											 VEGA_FIX miny, VEGA_FIX maxy)
{
	unsigned row_coverage = calculateArea(0, miny, VEGA_INTTOFIX(1), maxy);
	if (row_coverage == 0)
		return;

	int left_sx = VEGA_TRUNCFIXTOINT(VEGA_FLOOR(x));
	int right_sx = VEGA_TRUNCFIXTOINT(VEGA_FLOOR(x+w));

	unsigned qarea;

	if (right_sx - left_sx == 0)
	{
		qarea = calculateArea(x, miny, x + w, maxy);
		if (qarea != 0)
			emitRect(rects, num_rects, left_sx, y, 1, h, qarea);
	}
	else
	{
		int mid_sx = VEGA_TRUNCFIXTOINT(VEGA_CEIL(x));

		qarea = calculateArea(x, miny, VEGA_INTTOFIX(mid_sx), maxy);
		if (qarea != 0)
			emitRect(rects, num_rects, left_sx, y, 1, h, qarea);

		if (right_sx - mid_sx > 0)
			emitRect(rects, num_rects, mid_sx, y, right_sx - mid_sx, h, row_coverage);

		qarea = calculateArea(VEGA_INTTOFIX(right_sx), miny, x + w, maxy);
		if (qarea != 0)
			emitRect(rects, num_rects, right_sx, y, 1, h, qarea);
	}
}

#ifdef VEGA_USE_BLITTER_FOR_NONPIXELALIGNED_IMAGES
OP_STATUS VEGARendererBackend::fillFractImages(FractRect* rects, unsigned num_rects)
{
	OP_ASSERT(has_image_funcs && fillstate.fill && fillstate.fill->isImage());

	VEGAImage* image_fill = static_cast<VEGAImage*>(fillstate.fill);

	OP_ASSERT(num_rects < 10);
	VEGADrawImageInfo diinfos[9]; /* ARRAY OK 2010-06-14 rogerj */

	for (unsigned i=0; i<num_rects; i++)
	{
		VEGADrawImageInfo* diinfo = diinfos + i;
		FractRect* fr = rects + i;
		diinfo->dstx = fr->x;
		diinfo->dsty = fr->y;
		diinfo->dstw = fr->w;
		diinfo->dsth = fr->h;

		int width, height;
		if (!image_fill->prepareDrawImageInfo(*diinfo, width, height, false))
			return OpStatus::ERR;

		OP_ASSERT(height>0 && width>0);

		if (fr->weight < 255)
			diinfo->opacity = (UINT8)((int)diinfo->opacity * fr->weight / 255);

		// Clamp src values so that we at least get something with width=1, used for antialiasing non-grid aligned images.
		// The strategy is to repeat the outermost row of pixels for antialiasing if the src rectangle ends up outside of 
		// the image data.
		if (diinfo->srcx < 0)
			diinfo->srcx=0;
		else if (diinfo->srcx >= width)
			diinfo->srcx=width-1;

		if (diinfo->srcy < 0)
			diinfo->srcy=0;
		else if (diinfo->srcy >= height)
			diinfo->srcy=height-1;

		if (diinfo->srcx + diinfo->srcw > (unsigned)width)
			diinfo->srcw=width-diinfo->srcx;

		if (diinfo->srcy + diinfo->srch > (unsigned)height)
			diinfo->srch=height-diinfo->srcy;

		diinfo->srch = MAX(diinfo->srch, 1);
		diinfo->srcw = MAX(diinfo->srcw, 1);
	}

	for (unsigned i = 0; i < num_rects; i++)
		RETURN_IF_ERROR(drawImage(image_fill, diinfos[i]));

	return OpStatus::OK;
}

VEGAImage* VEGARendererBackend::getDrawableImageFromFill(VEGAFill* fill, VEGA_FIX dst_x, VEGA_FIX dst_y, VEGA_FIX dst_w, VEGA_FIX dst_h)
{
	if (!fill)
		return NULL;

	VEGAImage* img = NULL;
	if (fill->isImage())
	{
		img = static_cast<VEGAImage*>(fill);
	}
	else if (useCachedImages())
	{
		VEGAFill* cached = fill->getCache(this, dst_x, dst_y, dst_w, dst_h);
		if (cached && cached->isImage())
			img = static_cast<VEGAImage*>(cached);
	}

	if (img && !img->allowBlitter(dst_x, dst_y, dst_w, dst_h))
		return NULL;

	return img;
}
#endif // VEGA_USE_BLITTER_FOR_NONPIXELALIGNED_IMAGES

OP_STATUS VEGARendererBackend::fillFractRects(FractRect* rects, unsigned num_rects, VEGAStencil* stencil)
{
	OP_STATUS status = OpStatus::OK;

	FillOpacityState saved = saveFillOpacity();

	unsigned old_fo = fillstate.fill ? saved.fillopacity : (saved.color >> 24);

	for (unsigned i = 0; i < num_rects; ++i)
	{
		FractRect* fr = rects + i;
		
		OP_ASSERT(fr->weight != 0);

		UINT8 fo;
		if (fr->weight < 255)
			fo = (UINT8)(old_fo * fr->weight / 255);
		else
			fo = (UINT8)old_fo;

		if (VEGAFill* fill = fillstate.fill)
			fill->setFillOpacity(fo);
		else
			setColor((fo << 24) | (saved.color & 0xffffff));

		status = fillRect(fr->x, fr->y, fr->w, fr->h, stencil, true);

		if (OpStatus::IsError(status))
			break;
	}

	restoreFillOpacity(saved);

	return status;
}

// 'Sliced' rounded rectangle
OP_STATUS VEGARendererBackend::fillSlicedRoundedRect(VEGAPrimitive* prim, VEGAStencil* stencil)
{
	VEGA_FIX vf_flat = prim->flatness;

	VEGA_FIX x = prim->data.rrect.x;
	VEGA_FIX y = prim->data.rrect.y;
	VEGA_FIX w = prim->data.rrect.width;
	VEGA_FIX h = prim->data.rrect.height;
	VEGA_FIX rx = prim->data.rrect.corners[0];
	VEGA_FIX ry = prim->data.rrect.corners[1];

	if (prim->transform)
	{
		const VEGATransform& t = *prim->transform;

		// Apply transform to values - transform is assumed to be of
		// the form [ a 0 c ; 0 e f ] {a, e > 0} when we get here.
		t.apply(x, y);
		t.applyVector(w, h);
		t.applyVector(rx, ry);

		// We want the flatness to be in device space
		vf_flat = VEGA_FIXMUL(vf_flat, VEGA_FIXSQRT(VEGA_FIXMUL(t[0], t[4])));
	}

	// Reject primitives that we chose not to handle
	if (h - 2 * ry < VEGA_INTTOFIX(8))
		return OpStatus::ERR;

	VEGA_FIX cospi2 = VEGA_FIXCOS(VEGA_INTTOFIX(45));
	VEGA_FIX rxf1 = VEGA_FIXMUL(rx, cospi2);
	VEGA_FIX rxf2 = VEGA_FIXMUL(rx, 2 * cospi2 - VEGA_INTTOFIX(1));
	VEGA_FIX ryf1 = VEGA_FIXMUL(ry, cospi2);
	VEGA_FIX ryf2 = VEGA_FIXMUL(ry, 2 * cospi2 - VEGA_INTTOFIX(1));

	// Slice calculation
	VEGA_FIX slice_sy = VEGA_CEIL(y + ry);
	VEGA_FIX slice_ey = VEGA_FLOOR(y + h - ry);

	// Top slice
	{
		VEGAPath path; 
		RETURN_IF_ERROR(path.prepare(1 + 2 + 4*16 + 1));

		RETURN_IF_ERROR(path.moveTo(x + rx, y));

		VEGA_FIX curr_x = x + w - rx;
		VEGA_FIX curr_y = y;

		RETURN_IF_ERROR(path.lineTo(curr_x, curr_y));

		// Top-Right corner
		RETURN_IF_ERROR(path.appendCorner(curr_x + rxf2, curr_y,
										  curr_x + rxf1, curr_y + ry - ryf1,
										  curr_x + rx, curr_y + ry - ryf2,
										  curr_x + rx, curr_y + ry,
										  vf_flat));

		// Upper split line
		RETURN_IF_ERROR(path.lineTo(x + w, slice_sy));
		RETURN_IF_ERROR(path.lineTo(x, slice_sy));

		curr_x = x;
		curr_y = y + ry;

		RETURN_IF_ERROR(path.lineTo(curr_x, curr_y));

		// Top-Left corner
		RETURN_IF_ERROR(path.appendCorner(curr_x, curr_y - ryf2,
										  curr_x + rx - rxf1, curr_y - ryf1,
										  curr_x + rx - rxf2, curr_y - ry,
										  curr_x + rx, curr_y - ry,
										  vf_flat));

		RETURN_IF_ERROR(path.close(true));
#ifdef VEGA_3DDEVICE
		path.forceConvex();
#endif

		RETURN_IF_ERROR(fillPath(&path, stencil));
	}

	// Bottom slice
	{
		VEGAPath path;
		RETURN_IF_ERROR(path.prepare(1 + 2 + 4*16 + 1));

		VEGA_FIX curr_x = x + w;
		VEGA_FIX curr_y = y + h - ry;

		RETURN_IF_ERROR(path.moveTo(curr_x, curr_y));

		// Bottom-Right corner
		RETURN_IF_ERROR(path.appendCorner(curr_x, curr_y + ryf2,
										  curr_x - rx + rxf1, curr_y + ryf1,
										  curr_x - rx + rxf2, curr_y + ry,
										  curr_x - rx, curr_y + ry,
										  vf_flat));

		curr_x = x + rx;
		curr_y = y + h;

		RETURN_IF_ERROR(path.lineTo(curr_x, curr_y));

		// Bottom-Left corner
		RETURN_IF_ERROR(path.appendCorner(curr_x - rxf2, curr_y,
										  curr_x - rxf1, curr_y - ry + ryf1,
										  curr_x - rx, curr_y - ry + ryf2,
										  curr_x - rx, curr_y - ry,
										  vf_flat));

		// Lower split line
		RETURN_IF_ERROR(path.lineTo(x, slice_ey));
		RETURN_IF_ERROR(path.lineTo(x + w, slice_ey));

		RETURN_IF_ERROR(path.close(true));
#ifdef VEGA_3DDEVICE
		path.forceConvex();
#endif

		RETURN_IF_ERROR(fillPath(&path, stencil));
	}

	// Middle slice
	{
		// Fill rectangular area [x, x+w] [slice_sy, slice_ey]

		// We know that the y-extent is pixel aligned, so just truncate it
		int sy = VEGA_TRUNCFIXTOINT(slice_sy);
		int ey = VEGA_TRUNCFIXTOINT(slice_ey);

		OP_ASSERT(ey - sy > 0);

		FractRect rects[3]; /* ARRAY OK 2010-03-30 fs */
		unsigned num_rects = 0;

		emitFractionalSpan(rects, num_rects, x, sy, w, ey - sy, 0, VEGA_INTTOFIX(1));

#ifdef VEGA_USE_BLITTER_FOR_NONPIXELALIGNED_IMAGES
		if (has_image_funcs && fillstate.fill && fillstate.fill->isImage())
			RETURN_IF_ERROR(fillFractImages(rects, num_rects));
		else
#endif // VEGA_USE_BLITTER_FOR_NONPIXELALIGNED_IMAGES
			RETURN_IF_ERROR(fillFractRects(rects, num_rects, stencil));
	}

	return OpStatus::OK;
}

OP_STATUS VEGARendererBackend::fillFractionalRect(VEGAPrimitive* prim, VEGAStencil* stencil)
{
	VEGA_FIX x = prim->data.rect.x;
	VEGA_FIX y = prim->data.rect.y;
	VEGA_FIX w = prim->data.rect.width;
	VEGA_FIX h = prim->data.rect.height;

	if (prim->transform)
	{
		const VEGATransform& t = *prim->transform;

		// Apply transform to values - transform is assumed to be of
		// the form [ a 0 c ; 0 e f ] {a, e > 0} when we get here.
		t.apply(x, y);
		t.applyVector(w, h);
	}

	FractRect rects[9]; /* ARRAY OK 2010-04-14 fs */
	unsigned num_rects = 0;

	int top_sy = VEGA_TRUNCFIXTOINT(VEGA_FLOOR(y));
	int bot_sy = VEGA_TRUNCFIXTOINT(VEGA_FLOOR(y+h));

	if (bot_sy - top_sy == 0)
	{
		emitFractionalSpan(rects, num_rects, x, top_sy, w, 1, y, y + h);
	}
	else
	{
		int mid_sy = VEGA_TRUNCFIXTOINT(VEGA_CEIL(y));

		// [y, mid_sy]
		emitFractionalSpan(rects, num_rects, x, top_sy, w, 1, y, VEGA_INTTOFIX(mid_sy));

		// [mid_sy, bot_sy]
		if (bot_sy - mid_sy > 0)
			emitFractionalSpan(rects, num_rects, x, mid_sy, w, bot_sy - mid_sy, 0, VEGA_INTTOFIX(1));

		// [bot_sy, y + h]
		emitFractionalSpan(rects, num_rects, x, bot_sy, w, 1, VEGA_INTTOFIX(bot_sy), y + h);
	}

#ifdef VEGA_USE_BLITTER_FOR_NONPIXELALIGNED_IMAGES
	if (has_image_funcs && fillstate.fill && fillstate.fill->isImage())
		return fillFractImages(rects, num_rects);
#endif // VEGA_USE_BLITTER_FOR_NONPIXELALIGNED_IMAGES

	return fillFractRects(rects, num_rects, stencil);
}

OP_STATUS VEGARenderer::fillPrimitive(VEGAPrimitive* prim, VEGAStencil* stencil)
{
	if (!backend->renderTarget)
		return OpStatus::ERR;

	validateStencil(stencil);


	OP_STATUS status = OpStatus::OK;

	// Since nothing but the fillPath-operation can be expected to
	// handle rendertargets that are not color buffers, just skip the
	// fillPrimitive hook if we encounter that case.
	if (backend->renderTarget->getColorFormat() == VEGARenderTarget::RT_RGBA32)
	{
		status = backend->fillPrimitive(prim, stencil);

		// OK if handled, ERR if not handled, ERR_NO_MEMORY if OOM while trying
		if (OpStatus::IsSuccess(status) || OpStatus::IsMemoryError(status))
			return status;
	}

	switch (prim->type)
	{
	case VEGAPrimitive::RECTANGLE:
	case VEGAPrimitive::ROUNDED_RECT_UNIFORM:
	{
		VEGAPath path;
		RETURN_IF_ERROR(path.appendPrimitive(prim));

		if (prim->transform)
			path.transform(prim->transform);

#ifdef VEGA_3DDEVICE
		path.forceConvex();
#endif
		status = backend->fillPath(&path, stencil);
	}
	break;

	default:
		OP_ASSERT(!"Unknown primitive");
	}

	return status;
}

OP_STATUS VEGABackingStore::FallbackCopyRect(const OpPoint& dstp, const OpRect& srcr,
											 VEGABackingStore* store)
{
	// RGBA -> RGBA and Indexed -> RGBA is ok
	// RGBA -> Indexed won't work
	// Indexed -> Indexed is questionable, but we allow it
	if (IsIndexed() && !store->IsIndexed())
		return OpStatus::ERR;

	VEGASWBuffer* srcbuf = store->BeginTransaction(srcr, VEGABackingStore::ACC_READ_ONLY);
	if (srcbuf)
	{
		OpRect dstr(dstp.x, dstp.y, srcr.width, srcr.height);

		VEGASWBuffer* dstbuf = BeginTransaction(dstr, VEGABackingStore::ACC_WRITE_ONLY);
		if (dstbuf)
		{
			unsigned dstPixelStride = dstbuf->GetPixelStride();

			if (dstbuf->IsIndexed())
			{
				OP_ASSERT(srcbuf->IsIndexed());

				UINT8* idst = dstbuf->GetIndexedPtr(0, 0);

				for (int line = 0; line < srcr.height; ++line)
				{
					const UINT8* isrc = srcbuf->GetIndexedPtr(0, line);

					op_memcpy(idst, isrc, srcr.width);

					idst += dstPixelStride;
				}
			}
			else
			{
				VEGAPixelAccessor dst = dstbuf->GetAccessor(0, 0);

				if (srcbuf->IsIndexed())
				{
					const VEGA_PIXEL* pal = srcbuf->GetPalette();

					for (int line = 0; line < srcr.height; ++line)
					{
						const UINT8* isrc = srcbuf->GetIndexedPtr(0, line);

						for (int i = 0; i < srcr.width; i++)
						{
							VEGA_PIXEL px = pal[isrc[i]];

							unsigned a = VEGA_UNPACK_A(px);
							unsigned r = VEGA_UNPACK_R(px);
							unsigned g = VEGA_UNPACK_G(px);
							unsigned b = VEGA_UNPACK_B(px);

							dst.StoreARGB(a, r, g, b);
							dst++;
						}

						dst -= srcr.width;
						dst += dstPixelStride;
					}
				}
				else
				{
					VEGAConstPixelAccessor src = srcbuf->GetConstAccessor(0, 0);
					unsigned srcPixelStride = srcbuf->GetPixelStride();

					for (int line = 0; line < srcr.height; ++line)
					{
						dst.CopyFrom(src.Ptr(), srcr.width);

						dst += dstPixelStride;
						src += srcPixelStride;
					}
				}
			}

			EndTransaction(TRUE);
		}

		store->EndTransaction(FALSE);
	}
	return OpStatus::OK;
}

#ifdef VEGA_LIMIT_BITMAP_SIZE
unsigned VEGARendererBackend::maxBitmapSide()
{
	switch (g_vegaGlobals.rasterBackend)
	{
	case LibvegaModule::BACKEND_NONE:
	case LibvegaModule::BACKEND_SW:
		return VEGABackend_SW::maxBitmapSide();

#ifdef VEGA_2DDEVICE
	case LibvegaModule::BACKEND_HW2D:
		return VEGABackend_HW2D::maxBitmapSide();
#endif // VEGA_2DDEVICE

#ifdef VEGA_3DDEVICE
	case LibvegaModule::BACKEND_HW3D:
		return VEGABackend_HW3D::maxBitmapSide();
#endif // VEGA_3DDEVICE
	}

	return 0;
}
#endif // VEGA_LIMIT_BITMAP_SIZE

/* static */
OP_STATUS VEGARendererBackend::createBitmapStore(VEGABackingStore** store,
												 unsigned width, unsigned height,
												 bool is_indexed, bool is_opaque)
{
	// Create a suitable backing store - if the backend hasn't yet
	// been selected, create one backed by a SW-buffer.
	switch (g_vegaGlobals.rasterBackend)
	{
	case LibvegaModule::BACKEND_NONE:
	case LibvegaModule::BACKEND_SW:
		return VEGABackend_SW::createBitmapStore(store, width, height, is_indexed, is_opaque);

#ifdef VEGA_2DDEVICE
	case LibvegaModule::BACKEND_HW2D:
		return VEGABackend_HW2D::createBitmapStore(store, width, height, is_indexed);
#endif // VEGA_2DDEVICE

#ifdef VEGA_3DDEVICE
	case LibvegaModule::BACKEND_HW3D:
		return VEGABackend_HW3D::createBitmapStore(store, width, height, is_indexed);
#endif // VEGA_3DDEVICE
	}

	OP_ASSERT(!"Inconsistent backend selection/configuration");
	return OpStatus::ERR;
}

#ifdef VEGA_USE_HW
VEGASWBuffer* VEGABackingStore_Lockable::BeginTransaction(const OpRect& rect, AccessType acc)
{
	// Is a 'lazy' transaction in progress?
	if (m_transaction_deferred)
	{
		// The rect is the same and the access type is the same ->
		// reuse buffer
		if (rect.Equals(m_transaction_rect) && m_acc_type == acc)
			return &m_lock_buffer;

		// Differing accesstype -> need to restart
		OpStatus::Ignore(UnlockRect(m_acc_commit));

		// NOTE: Choosing to ignore any locking failures, and hope
		// that they are not fatal.
	}

	m_transaction_deferred = false;

	m_transaction_rect = rect;

	RETURN_VALUE_IF_ERROR(LockRect(rect, acc), NULL);

	m_acc_type = acc;

	return &m_lock_buffer;
}

void VEGABackingStore_Lockable::EndTransaction(BOOL commit)
{
	m_transaction_deferred = true;
	m_acc_commit = commit && (m_acc_type != VEGABackingStore::ACC_READ_ONLY);
}

void VEGABackingStore_Lockable::Validate()
{
	FlushTransaction();
}
#endif // VEGA_USE_HW

#endif // VEGA_SUPPORT
