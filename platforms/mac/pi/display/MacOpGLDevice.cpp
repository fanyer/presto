/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#if defined(VEGA_SUPPORT) && (defined(VEGA_3DDEVICE) || defined(CANVAS3D_SUPPORT))

#include "core/pch_system_includes.h"

#include "adjunct/desktop_util/boot/DesktopBootstrap.h"

#include "platforms/vega_backends/opengl/vegaglapi.h"
#include "platforms/vega_backends/opengl/vegagldevice.h"
#include "platforms/vega_backends/opengl/vegaglfbo.h"
#include "platforms/vega_backends/opengl/vegagltexture.h"
#include "platforms/vega_backends/opengl/vegaglbuffer.h"
#include "platforms/mac/pi/display/MacOpGLDevice.h"
#include "platforms/mac/pi/CocoaVEGAWindow.h"
#include "platforms/mac/pi/CocoaOpWindow.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "platforms/mac/util/macutils.h"
#include "platforms/crashlog/gpu_info.h"

#include "modules/libvega/src/vegabackend_hw3d.h"

#include <dlfcn.h>
#include <mach-o/dyld.h>

OP_STATUS VEGA3dDeviceFactory::Create(unsigned int device_type_index, VEGA3dDevice** dev VEGA_3D_NATIVE_WIN_DECL)
{
	*dev = NULL;
	MacOpGlDevice* mdev = OP_NEW(MacOpGlDevice, ());
	if (!mdev)
		return OpStatus::ERR_NO_MEMORY;
	*dev = mdev;
	OP_STATUS err = mdev->Init();
	
	if (OpStatus::IsError(err))
	{
		OP_DELETE(mdev);
		*dev = NULL;
	}
	
	return err;
}

const uni_char* VEGA3dDeviceFactory::DeviceTypeString(UINT16 device_type_index)
{
	if (device_type_index == 0)
		return UNI_L("Hardware accelerated (OpenGL) renderer"); // ismailp - get renderer name properly
	else if (device_type_index == 1)
		return UNI_L("Apple Software Renderer");
	return NULL;
}

unsigned int VEGA3dDeviceFactory::DeviceTypeCount()
{
	typedef void (*tglGetIntegerv)(GLenum pname, GLint *params);
	tglGetIntegerv gl_getIntegerv = (tglGetIntegerv)MacOpGlDevice::loadOpenGLFunction("glGetIntegerv");
	GLint maxSize = INT_MAX;
	if (gl_getIntegerv)
		gl_getIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);

	// OBS: After the switch to NSOpenGLLayer we no longer support hwa on 10.5
	if (g_desktop_bootstrap->GetHWAccelerationDisabled() || GetOSVersion() < 0x1060 || GetMaxScreenDimension() > maxSize)
		return 0;

	return 1;
}

MacOpGlDevice::~MacOpGlDevice()
{
	DeviceDeleted(this);
}

OP_STATUS MacOpGlDevice::Init()
{
	FillGpuInfo(this);
    
	return OpStatus::OK;
}

#ifdef VEGA_OPPAINTER_SUPPORT

OP_STATUS MacOpGlDevice::createWindow(VEGA3dWindow** win, VEGAWindow* nativeWin)
{
	MacGlWindow* glwin = OP_NEW(MacGlWindow, (nativeWin));
	if (!glwin)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	if (OpStatus::IsError(glwin->Construct()))
	{
		OP_DELETE(glwin);
		return OpStatus::ERR;
	}
	*win = glwin;
	
	return OpStatus::OK;
}
#endif // VEGA_OPPAINTER_SUPPORT

/* static */
void* MacOpGlDevice::loadOpenGLFunction(const char* funcName)
{
	void *symbol = NULL;
	char symbolName[128] = {0}; // see any symbol longer than 128, make this dynamic.
	static const char *kKnownFormats[] = {
		"%s",
		"_%s",
		"%sARB",
		"%sEXT"
		"%_sARB",
		"%_sEXT"
	};

	for (UINT8 c = 0; c < ARRAY_SIZE(kKnownFormats) && !symbol; ++c)
	{
		symbolName[snprintf(symbolName, 127, kKnownFormats[c], funcName)] = '\0';
		symbol = dlsym(RTLD_DEFAULT, symbolName);
	}

	return symbol;
}

void* MacOpGlDevice::getGLFunction(const char* funcName)
{
    return loadOpenGLFunction(funcName);
}

MacGlWindow::MacGlWindow(VEGAWindow* w)
: window(w)
, tex(NULL)
, bg_tex(NULL)
, fbo(NULL)
, bg_fbo(NULL)
, resize_tex(NULL)
, resize_fbo(NULL)
, m_shader(NULL)
{
	width = w->getWidth();
	height = w->getHeight();
}

MacGlWindow::~MacGlWindow()
{
	VEGA3dShaderProgram::DecRef(m_shader);
	VEGARefCount::DecRef(fbo);
	VEGA3dTexture::DecRef(tex);
	VEGARefCount::DecRef(resize_fbo);
	VEGA3dTexture::DecRef(resize_tex);
}

OP_STATUS MacGlWindow::Construct()
{
	VEGA3dDevice* device = g_opera->libvega_module.vega3dDevice;

	RETURN_IF_ERROR(device->createShaderProgram(&m_shader, VEGA3dShaderProgram::SHADER_VECTOR2D, VEGA3dShaderProgram::WRAP_CLAMP_CLAMP));
	RETURN_IF_ERROR(resizeBackbuffer(width, height));
	if (!tex && width && height)
		return OpStatus::ERR_NO_MEMORY;
	
	return OpStatus::OK;
}

void MacGlWindow::present(const OpRect* updateRects, unsigned int numRects)
{
	present(updateRects, numRects, false);
}

void MacGlWindow::present(const OpRect*, unsigned int, bool background)
{
	RESET_OPENGL_CONTEXT
	
	VEGA3dDevice* device = g_opera->libvega_module.vega3dDevice;

	VEGA3dBuffer* b = device->getTempBuffer(4*sizeof(VEGA3dDevice::Vega2dVertex));
	if (!b)
		return;
	VEGA3dTexture* tx = background ? bg_tex : tex;
	if (!tx)
		return;

	VEGA3dVertexLayout* layout;
	RETURN_VOID_IF_ERROR(device->createVega2dVertexLayout(&layout, VEGA3dShaderProgram::SHADER_VECTOR2D));

	// Set the shader and update the projection matrix before clearing render target
	device->setShaderProgram(m_shader);
	device->setRenderTarget(this);
	m_shader->setOrthogonalProjection();
	device->setRenderTarget(NULL);

	device->setRenderState(device->getDefault2dNoBlendNoScissorRenderState());

	VEGA3dDevice::Vega2dVertex verts[] = {
		{0, 0, 0, 1.f, ~0u},
		{(float)tx->getWidth(), 0, 1.f, 1.f, ~0u},
		{0, (float)tx->getHeight(), 0, 0, ~0u},
		{(float)tx->getWidth(), (float)tx->getHeight(), 1.f, 0, ~0u}
	};
	unsigned int start_index;
	b->writeAnywhere(4, sizeof(VEGA3dDevice::Vega2dVertex), verts, start_index);
	device->setTexture(0, tx);
	device->drawPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE_STRIP, layout, start_index, 4);

	// Draw the resize corner if needed
	if (resize_tex)
	{
#if SIMPLE_RESIZE_CORNER
		VEGA3dDevice::Vega2dVertex verts_resize1[] = {
			{(float)tex->getWidth()-11, 1, 0, 1.f, ~0u},
			{(float)tex->getWidth()-1, 11, 1.f, 1.f, ~0u},
		};
		VEGA3dDevice::Vega2dVertex verts_resize2[] = {
			{(float)tex->getWidth()-7, 1, 0, 1.f, ~0u},
			{(float)tex->getWidth()-1, 7, 1.f, 1.f, ~0u},
		};
		VEGA3dDevice::Vega2dVertex verts_resize3[] = {
			{(float)tex->getWidth()-3, 1, 0, 1.f, ~0u},
			{(float)tex->getWidth()-1, 3, 1.f, 1.f, ~0u},
		};
		device->setTexture(0, resize_tex);
		b->writeAnywhere(2, sizeof(VEGA3dDevice::Vega2dVertex), verts_resize1, start_index);
		device->drawPrimitives(VEGA3dDevice::PRIMITIVE_LINE, layout, start_index, 2);
		b->writeAnywhere(2, sizeof(VEGA3dDevice::Vega2dVertex), verts_resize2, start_index);
		device->drawPrimitives(VEGA3dDevice::PRIMITIVE_LINE, layout, start_index, 2);
		b->writeAnywhere(2, sizeof(VEGA3dDevice::Vega2dVertex), verts_resize3, start_index);
		device->drawPrimitives(VEGA3dDevice::PRIMITIVE_LINE, layout, start_index, 2);
#else
		// Resize corner must blend
		device->setRenderState(device->getDefault2dRenderState());

		VEGA3dDevice::Vega2dVertex verts_resize[] = {
			{(float)tx->getWidth()-16, 0, 0, 1.f, ~0u},
			{(float)tx->getWidth(), 0, 1.f, 1.f, ~0u},
			{(float)tx->getWidth()-16, (float)16, 0, 0, ~0u},
			{(float)tx->getWidth(), (float)16, 1.f, 0, ~0u}
		};
		b->writeAnywhere(4, sizeof(VEGA3dDevice::Vega2dVertex), verts_resize, start_index);
		device->setTexture(0, resize_tex);
		device->drawPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE_STRIP, layout, start_index, 4);
#endif
	}

	device->setTexture(0, NULL);

	VEGARefCount::DecRef(layout);
}

OP_STATUS MacGlWindow::resizeBackbuffer(unsigned int w, unsigned int h)
{
	RESET_OPENGL_CONTEXT

    if (w == width && h == height && tex)
        return OpStatus::OK;
    
    // when initializing this function could be called with 0 width/height
    if (!w || !h)
    {
        width = w;
        height = h;
        return OpStatus::OK;
    }

    VEGA3dDevice* device = g_opera->libvega_module.vega3dDevice;
    if (!fbo)
        RETURN_IF_ERROR(device->createFramebuffer(&fbo));
	if (bg_tex) {
		VEGA3dTexture::DecRef(bg_tex);
		bg_tex = NULL;
	}
    VEGA3dTexture* newtex;
    RETURN_IF_ERROR(device->createTexture(&newtex, w, h, VEGA3dTexture::FORMAT_RGBA8888));
    newtex->setFilterMode(VEGA3dTexture::FILTER_NEAREST, VEGA3dTexture::FILTER_NEAREST);
    // Make sure the window is not active render target when deleting the texture
    if (device->getRenderTarget() == this)
        device->setRenderTarget(NULL);

    if (OpStatus::IsError(fbo->attachColor(newtex)))
    {
        VEGA3dTexture::DecRef(newtex);
        return OpStatus::ERR;
    }
    VEGA3dTexture::DecRef(tex);
    tex = newtex;
    width = w;
    height = h;
	
	// Create the resize corner if needed
	if (!resize_fbo && ((CocoaVEGAWindow*)window)->GetNativeOpWindow()->GetStyle() == OpWindow::STYLE_DESKTOP)
        device->createFramebuffer(&resize_fbo);
	if (resize_fbo && !resize_tex) {
#if SIMPLE_RESIZE_CORNER
		device->createTexture(&resize_tex, 1, 1, VEGA3dTexture::FORMAT_RGBA8888);
		if (resize_tex)
		{
			resize_tex->setFilterMode(VEGA3dTexture::FILTER_NEAREST, VEGA3dTexture::FILTER_NEAREST);
			resize_fbo->attachColor(resize_tex);
			unsigned short rszpix = 0x7030;
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, &rszpix);
		}
#else
		device->createTexture(&resize_tex, 16, 16, VEGA3dTexture::FORMAT_RGBA8888);
		if (resize_tex)
		{
			resize_tex->setFilterMode(VEGA3dTexture::FILTER_NEAREST, VEGA3dTexture::FILTER_NEAREST);
			resize_fbo->attachColor(resize_tex);
			unsigned short* resize = (unsigned short*)op_calloc(1, 16*16*2);
			unsigned short rszpix[] = { 0x1200, 0x7400, 0x0000, 0x0000 };
			int x, y, i, j;
			// Make three lines with a bit of highlight over and under.
			// Let the rest be transparent.
			for (x = 4, y = 14; y >= 4; x++, y--) {
				for (j = 0, i = x; i < 15; i++) {
					resize[i+y*16] = rszpix[j];
					j++; if (j > 3) j = 0;
				}
			}
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, resize);
			op_free(resize);
		}
#endif
	}
	
	return OpStatus::OK;
}

OpWindow::Style MacGlWindow::GetStyle() const
{
	if (window)
	{
		OpWindow* opwnd = static_cast<OpWindow*>(static_cast<CocoaVEGAWindow*>(window)->GetNativeOpWindow());
		if (opwnd)
			return opwnd->GetStyle();
	}

	return OpWindow::STYLE_UNKNOWN;
}

void MacGlWindow::Erase(const MDE_RECT& rect, bool background)
{
	RESET_OPENGL_CONTEXT

	VEGA3dDevice* device = g_opera->libvega_module.vega3dDevice;
	VEGA3dFramebufferObject* fb = background ? bg_fbo : fbo;
	if (!fb)
		return;

	// We MUST flush before changing the render target or we get pixel vommit
	if (g_vegaGlobals.m_current_batcher)
		g_vegaGlobals.m_current_batcher->flushBatches();

	device->setRenderTarget(fb);
	
	// Code copied from VEGABackend_HW3D::clear() and X11GlWindow::present()
	
	VEGA3dDevice::Vega2dVertex verts[] = {
		{float(rect.x), float(rect.y), 0, 0, 0},
		{float(rect.x + rect.w), float(rect.y), 1, 0, 0},
		{float(rect.x), float(rect.y + rect.h), 0, 1, 0},
		{float(rect.x + rect.w), float(rect.y + rect.h), 1, 1, 0}
	};

	VEGA3dBuffer* b = device->getTempBuffer(sizeof(verts));
	if (!b)
		return;

	VEGA3dVertexLayout* layout = 0;
	RETURN_VOID_IF_ERROR(device->createVega2dVertexLayout(&layout, VEGA3dShaderProgram::SHADER_VECTOR2D));
	
	VEGA3dRenderState state;
	state.enableBlend(false);
	device->setRenderState(&state);
	unsigned int bufidx = 0;
	b->writeAnywhere(4, sizeof(VEGA3dDevice::Vega2dVertex), verts, bufidx);
	device->setShaderProgram(m_shader);
	m_shader->setOrthogonalProjection();
	device->setTexture(0, NULL);
	device->drawPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE_STRIP, layout, bufidx, 4);
	VEGARefCount::DecRef(layout);
}

void MacGlWindow::MoveToBackground(const MDE_RECT& rect)
{
	RESET_OPENGL_CONTEXT

	// Create background texure
	VEGA3dDevice* device = g_opera->libvega_module.vega3dDevice;
	if (!bg_fbo) {
		RETURN_VOID_IF_ERROR(device->createFramebuffer(&bg_fbo));
	}
	if (!bg_tex) {
		VEGA3dTexture* nt;
		RETURN_VOID_IF_ERROR(device->createTexture(&nt, width, height, VEGA3dTexture::FORMAT_RGBA8888));
		nt->setFilterMode(VEGA3dTexture::FILTER_NEAREST, VEGA3dTexture::FILTER_NEAREST);
		bg_tex = nt;
		bg_fbo->attachColor(bg_tex);
		MDE_RECT r = {0, 0, width, height};
		Erase(r, true);
	}

	// We MUST flush to make sure the current fbo is correct (i.e. the content has been painted!!!)
	if (g_vegaGlobals.m_current_batcher)
		g_vegaGlobals.m_current_batcher->flushBatches();

	// Copy the rect from the foreground/content texture to the background texture
	glBindFramebuffer(GL_FRAMEBUFFER, static_cast<VEGAGlFramebufferObject*>(fbo)->fbo);
	glBindFramebuffer(non_gles_GL_DRAW_FRAMEBUFFER, static_cast<VEGAGlFramebufferObject*>(bg_fbo)->fbo);
	glBlitFramebuffer(rect.x, rect.y, rect.x+rect.w, rect.y+rect.h, rect.x, rect.y, rect.x+rect.w, rect.y+rect.h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, static_cast<VEGAGlFramebufferObject*>(fbo)->fbo);

	// Erase the foreground/content
	Erase(rect);
}

#ifdef VEGA_BACKENDS_USE_BLOCKLIST
VEGABlocklistDevice::BlocklistType MacOpGlDevice::GetBlocklistType() const
{
	return VEGABlocklistDevice::MacGL;
}

class MacBlacklistDataProvider : public VEGABlocklistDevice::DataProvider {
	MacOpGlDevice* device_;
	const GLubyte * (*gl_getString)(GLenum name);
public:
	explicit MacBlacklistDataProvider(MacOpGlDevice* dev)
	: device_(dev)
	{
		gl_getString = (const GLubyte*(*)(GLenum))device_->getGLFunction("glGetString");
	}
	virtual OP_STATUS GetValueForKey(const uni_char* key, OpString& val) {
		OpStringC k(key);
		if (k.Compare(UNI_L("OpenGL-"), 7) == 0)
		{
			GLenum name;
			if (k == UNI_L("OpenGL-vendor"))
				name = GL_VENDOR;
			else if (k == UNI_L("OpenGL-renderer"))
				name = GL_RENDERER;
			else if (k == UNI_L("OpenGL-version"))
				name = GL_VERSION;
			else if (k == UNI_L("OpenGL-shading-language-version"))
				name = GL_SHADING_LANGUAGE_VERSION;
			else if (k == UNI_L("OpenGL-extensions"))
				name = GL_EXTENSIONS;
			else
				return OpStatus::ERR;
			const GLubyte* str = gl_getString(name);
			RETURN_VALUE_IF_NULL(str, OpStatus::ERR);
			return val.Set(reinterpret_cast<const char*>(str));
		}
		else if (k == UNI_L("OS-kernel-version"))
		{
			int major, minor, bugfix;
			GetDetailedOSVersion(major, minor, bugfix);
			return val.AppendFormat(UNI_L("%d.%d.%d"), major, minor, bugfix);
		}
		else if (k == UNI_L("Dedicated Video Memory"))
		{
			int vram = GetVideoRamSize();
			return val.AppendFormat(UNI_L("%d"), vram/(1024*1024));
		}

		return OpStatus::ERR;
	}
};

VEGABlocklistDevice::DataProvider* MacOpGlDevice::CreateDataProvider()
{
	return OP_NEW(MacBlacklistDataProvider, (this));
}
#endif

#endif // VEGA_SUPPORT && (VEGA_3DDEVICE || CANVAS3D_SUPPORT)
