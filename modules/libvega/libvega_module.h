/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_LIBVEGA_LIBVEGA_MODULE_H
#define MODULES_LIBVEGA_LIBVEGA_MODULE_H

#ifdef VEGA_SUPPORT

#define VEGA_USE_LINEDATA_POOL

#ifdef VEGA_3DDEVICE
# include "modules/libvega/vega3ddevice.h"
#endif // VEGA_3DDEVICE

/** The global objects used by the libvega module. */
class LibvegaModule : public OperaModule
{
public:
	LibvegaModule();
	void InitL(const OperaInitInfo& info);
	void Destroy();

	static class VEGARendererBackend* InstantiateRasterBackend(unsigned int w, unsigned int h, unsigned int q, unsigned int rasterBackend);
	class VEGARendererBackend* CreateRasterBackend(unsigned int w, unsigned int h, unsigned int q, unsigned int forcedBackend);
	class VEGARendererBackend* SelectRasterBackend(unsigned int w, unsigned int h, unsigned int q);

	class VEGA3dDevice* vega3dDevice;
	class VEGA2dDevice* vega2dDevice;

	enum
	{
		BACKEND_NONE,
		BACKEND_HW3D,
		BACKEND_HW2D,
		BACKEND_SW
	};
	unsigned int rasterBackend;

#ifdef VEGA_USE_GRADIENT_CACHE
	class VEGAGradientCache* gradientCache;
#endif // VEGA_USE_GRADIENT_CACHE

#ifdef VEGA_OPPAINTER_SUPPORT
	class VEGAWindow* mainNativeWindow;
	const uni_char* default_font;
	const uni_char* default_serif_font;
	const uni_char* default_sans_serif_font;
	const uni_char* default_cursive_font;
	const uni_char* default_fantasy_font;
	const uni_char* default_monospace_font;
#endif // VEGA_OPPAINTER_SUPPORT

#ifdef VEGA_3DDEVICE
	/** The 3d hardware backend batches together multiple draw calls
	 * and flush the batches as needed.
	 * If more than one renderer batches at the same time there can
	 * be an issue with bitmaps being modified by a renderer while
	 * they are still in a pending batch in other renderer.
	 * To get around this we limit the batching to only allow batching
	 * of one renderer at the time. */
	class VEGABackend_HW3D* m_current_batcher;

	OP_STATUS getShaderLocations(int* worldProjMatrix2dLocationV,
	                             int* texTransSLocationV,
	                             int* texTransTLocationV,
	                             int* stencilCompBasedLocation,
	                             int* straightAlphaLocation);

	bool shaderLocationsInitialized;
	int worldProjMatrix2dLocationV[VEGA3dShaderProgram::WRAP_MODE_COUNT];
	int texTransSLocationV[VEGA3dShaderProgram::WRAP_MODE_COUNT];
	int texTransTLocationV[VEGA3dShaderProgram::WRAP_MODE_COUNT];
	int stencilCompBasedLocation[VEGA3dShaderProgram::WRAP_MODE_COUNT];
	int straightAlphaLocation[VEGA3dShaderProgram::WRAP_MODE_COUNT];

	/** This is a texture atlas used by VEGAGradient to avoid creating
	 * (and binding) a new texture for every gradient needed.
	 */
	class VEGA3dTexture* m_gradient_texture;

	/** m_gradient_texture is filled in from the top with full lines.
	 * This variable holds the number of lines filled in so far.
	 */
	unsigned int m_gradient_texture_next_line;
#endif // VEGA_3DDEVICE

#ifdef VEGA_USE_LINEDATA_POOL
	void* linedata_pool;
#endif // VEGA_USE_LINEDATA_POOL

#ifdef VEGA_USE_ASM
	class VEGADispatchTable* dispatchTable;
#endif // VEGA_USE_ASM
};

// We can only enable support the Canvas Text APIs if SVG is supported.
#if defined(SVG_SUPPORT) && defined(SVG_PATH_TO_VEGAPATH)
# define CANVAS_TEXT_SUPPORT
#endif // SVG_SUPPORT && SVG_PATH_TO_VEGAPATH

#if defined(VEGA_2DDEVICE) || defined(VEGA_3DDEVICE)
# define VEGA_USE_HW
#endif // VEGA_2DDEVICE || VEGA_3DDEVICE

#define g_vegaGlobals (g_opera->libvega_module)

#ifdef VEGA_USE_ASM
#define g_vegaDispatchTable (g_vegaGlobals.dispatchTable)
#endif // VEGA_USE_ASM

#define LIBVEGA_MODULE_REQUIRED

#endif // VEGA_SUPPORT
#endif // !MODULES_LIBVEGA_LIBVEGA_MODULE_H

