/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "Windows3dDevice.h"
#include "adjunct/desktop_util/boot/DesktopBootstrap.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/desktop_util/gpu/gpubenchmark.h"
#include "modules/about/operagpu.h"
#include "modules/libvega/vegawindow.h"
#include "modules/libvega/vegarenderer.h"
#include "modules/libvega/src/vegabackend_hw3d.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "platforms/crashlog/gpu_info.h"
#include "platforms/vega_backends/opengl/vegaglapi.h"
#include "platforms/windows/WinGLDataProvider.h"
#include "platforms/windows/user_fun.h"
#include "adjunct/quick/managers/CommandLineManager.h"

typedef BOOL (WINAPI *WGLDELETECONTEXT)(HGLRC hglrc);
typedef BOOL (WINAPI *WGLMAKECURRENT)(HDC hdc,HGLRC hglrc);
typedef HGLRC (WINAPI *WGLCREATECONTEXT)(HDC hdc);
typedef PROC (WINAPI *WGLGETPROCADDRESS)(LPCSTR lpszProc);

WGLDELETECONTEXT pwglDeleteContext;
WGLMAKECURRENT pwglMakeCurrent;
WGLCREATECONTEXT pwglCreateContext;
WGLGETPROCADDRESS pwglGetProcAddress;

#define GPU_BENCHMARK_TIMEOUT 5000
#define GPU_BENCHMARK_INTERVAL 100

typedef void (WINAPI *GLADDSWAPHINTRECTWIN)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef bool (APIENTRY *PFNWGLSWAPINTERVALFARPROC)(int);

PFNWGLSWAPINTERVALFARPROC wglSwapIntervalEXT = 0;
GLADDSWAPHINTRECTWIN glAddSwapHintRectWIN;

OP_STATUS SpawnGPUBenchmarkProcess(unsigned int backend)
{
	OpString opera_path, parameters;

	RETURN_IF_ERROR(GetExePath(opera_path));
	RETURN_IF_ERROR(parameters.AppendFormat(UNI_L("\"%s\" /gputest %d"), opera_path.CStr(), backend));

	// pass along -pd parameter
	CommandLineArgument* pd = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::PersonalDirectory);
	if (pd)
	{
		RETURN_IF_ERROR(parameters.AppendFormat(UNI_L(" /pd \"%s\""), pd->m_string_value));
	}

	STARTUPINFO info;
	op_memset(&info, 0, sizeof(STARTUPINFO));
	info.cb = sizeof(STARTUPINFO);
	PROCESS_INFORMATION process;

	int value = g_pccore->GetIntegerPref(PrefsCollectionCore::EnableHardwareAcceleration);
	if (value == PrefsCollectionCore::Disable)
	{
		RETURN_IF_LEAVE(g_pccore->WriteIntegerL(PrefsCollectionCore::EnableHardwareAcceleration, PrefsCollectionCore::Auto);
						g_prefsManager->CommitL());
	}

	if (CreateProcess(NULL, parameters, NULL, NULL, FALSE, 0, NULL, NULL, &info, &process))
	{
		for (int i=0; i<GPU_BENCHMARK_TIMEOUT / GPU_BENCHMARK_INTERVAL; i++)
		{
			if (WAIT_TIMEOUT != WaitForSingleObject(process.hThread, GPU_BENCHMARK_INTERVAL))
				break;

			DWORD exit_code;
			if (!GetExitCodeProcess(process.hProcess, &exit_code) || STILL_ACTIVE != exit_code)
				break;
		}
	}
	RETURN_IF_LEAVE(g_pccore->WriteIntegerL(PrefsCollectionCore::EnableHardwareAcceleration, value);
					g_prefsManager->CommitL());

	return OpStatus::OK;
}

#ifdef VEGA_BACKEND_DIRECT3D10

#include "platforms/vega_backends/d3d10/vegad3d10device.h"

class Windows3d10Device : public VEGAD3d10Device
{
public:
	Windows3d10Device(VEGAWindow* nativeWin, D3D10_DRIVER_TYPE driver_type) : VEGAD3d10Device(nativeWin, driver_type) {}
	~Windows3d10Device() { DeviceDeleted(this); }

	void DeviceCreated() { FillGpuInfo(this); }

};

OP_STATUS CreateD3D10Device(VEGA3dDevice** dev VEGA_3D_NATIVE_WIN_DECL, D3D10_DRIVER_TYPE driver_type)
{
	if (VEGA_3D_NATIVE_WIN_NAME && !VEGA_3D_NATIVE_WIN_NAME->getNativeHandle())
		return OpStatus::ERR;

	*dev = NULL;
	Windows3d10Device* wdev = OP_NEW(Windows3d10Device, (VEGA_3D_NATIVE_WIN_NAME, driver_type));
	RETURN_OOM_IF_NULL(wdev);

	OP_STATUS status = wdev->Init();

	// Set text mode on the device (Aliased, Grayscale or Cleartype).
	if (OpStatus::IsSuccess(status))
	{
		BOOL font_smoothing;
		UINT font_smoothing_type;
		SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &font_smoothing, FALSE);
		SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, 0, &font_smoothing_type, FALSE);

		if (font_smoothing && font_smoothing_type == FE_FONTSMOOTHINGCLEARTYPE)
			wdev->SetTextMode(D2D1_TEXT_ANTIALIAS_MODE_DEFAULT);
		else if (font_smoothing)
			wdev->SetTextMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
		else
			wdev->SetTextMode(D2D1_TEXT_ANTIALIAS_MODE_ALIASED);
	}

	// Fail the initialization if the screen resolution is bigger than the max texture size,
	// as most of the UI currently depends on it being smaller.
	if (OpStatus::IsSuccess(status))
	{
		int screen_width = GetSystemMetrics(SM_CXFULLSCREEN);
		int screen_height = GetSystemMetrics(SM_CYFULLSCREEN);
		int max_texture_size = (int)wdev->getMaxTextureSize();

		if (screen_width > max_texture_size || screen_height > max_texture_size)
			status = OpStatus::ERR;
	}

	if (OpStatus::IsSuccess(status))
	{
		*dev = wdev;
		wdev->DeviceCreated();
	}
	else
	{
		OP_DELETE(wdev);
	}

	return status;
}

unsigned int VEGAD3d10Device::getDeviceType() const
{
	if (m_driverType == D3D10_DRIVER_TYPE_HARDWARE)
		return  PrefsCollectionCore::DirectX10;
	else if (m_driverType == D3D10_DRIVER_TYPE_WARP)
		return PrefsCollectionCore::DirectX10Warp;
	else
	{
		OP_ASSERT(!"This driver type is not implemented yet.");
		return PrefsCollectionCore::DirectX10;
	}
}

#endif // VEGA_BACKEND_DIRECT3D10

#ifdef VEGA_BACKEND_OPENGL

#include "platforms/vega_backends/opengl/vegaglwindow.h"

static LONG WINAPI WindowProc3D(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
    switch(uMsg){
    case WM_CLOSE:
		PostQuitMessage(0);
		return 0;
	}
	return (LONG)DefWindowProc(hwnd, uMsg, wParam, lParam);
}

VEGABlocklistDevice::DataProvider* WindowsGlDevice::CreateDataProvider()
{
	return OP_NEW(WinGLDataProvider, ());
}

OP_STATUS WindowsGlDevice::GenerateSpecificBackendInfo(OperaGPU* page, VEGABlocklistFile* blocklist, DataProvider* provider)
{
	// Error is safe to ignore as long as the code below doesn't rely upon glapi to initialize properly
	OpStatus::Ignore(VEGAGlDevice::GenerateSpecificBackendInfo(page, blocklist, provider));

	OpString s;
	RETURN_IF_ERROR(page->OpenDefinitionList());
	OpString vendor_id_str;
	if (OpStatus::IsSuccess(provider->GetValueForKey(UNI_L("vendor id"), vendor_id_str)) && !vendor_id_str.IsEmpty())
		RETURN_IF_ERROR(page->ListEntry(OpStringC(UNI_L("Vendor ID")), OpStringC(UNI_L("")), vendor_id_str.CStr()));
	if (OpStatus::IsSuccess(provider->GetValueForKey(UNI_L("device id"), s)) && !s.IsEmpty())
		RETURN_IF_ERROR(page->ListEntry(OpStringC(UNI_L("Device ID")), OpStringC(UNI_L("")), s.CStr()));
	if (OpStatus::IsSuccess(provider->GetValueForKey(UNI_L("driver version"), s)) && !s.IsEmpty())
		RETURN_IF_ERROR(page->ListEntry(OpStringC(UNI_L("Driver version")), OpStringC(UNI_L("")), s.CStr()));
	RETURN_IF_ERROR(page->CloseDefinitionList());

	return OpStatus::OK;
}

class WindowsGlWindow : public VEGAGlWindow
{
public:
	WindowsGlWindow(WindowsGlDevice* dev, VEGAWindow* nw, HWND hwin, HDC dc) : device(dev), native_win(nw), hwin(hwin), dc(dc), 
		tex(NULL), fbo(NULL), m_shader(NULL)
	{
		width = nw->getWidth();
		height = nw->getHeight();
	}

	~WindowsGlWindow()
	{
		VEGA3dShaderProgram::DecRef(m_shader);
		VEGARefCount::DecRef(fbo);
		VEGA3dTexture::DecRef(tex);
		device->activateDC(NULL);
		ReleaseDC(hwin, dc);
	}

	OP_STATUS Construct()
	{
		RETURN_IF_ERROR(device->createShaderProgram(&m_shader, VEGA3dShaderProgram::SHADER_VECTOR2D, VEGA3dShaderProgram::WRAP_CLAMP_CLAMP));
		RETURN_IF_ERROR(resizeBackbuffer(width, height));
		if (!tex && width && height)
			return OpStatus::ERR_NO_MEMORY; // FIXME: proper error code

		return OpStatus::OK;
	}

	virtual void present(const OpRect* update_rects, unsigned int num_rects)
	{
		device->activateDC(dc);

		VEGA3dBuffer* b = device->getTempBuffer(4*sizeof(VEGA3dDevice::Vega2dVertex));
		if (!b)
			return;
		VEGA3dVertexLayout* layout;
		RETURN_VOID_IF_ERROR(device->createVega2dVertexLayout(&layout, VEGA3dShaderProgram::SHADER_VECTOR2D));

		// Set the shader and update the projection matrix before clearing render target
		device->setShaderProgram(m_shader);
		m_shader->setOrthogonalProjection();
		device->setRenderTarget(NULL);

		VEGA3dRenderState* state = device->getDefault2dNoBlendNoScissorRenderState();
		device->setRenderState(state);
		device->setTexture(0, tex);

		if (device->hasSwapCopy())
		{
			unsigned numVerts = 4*num_rects;
			OpAutoArray<VEGA3dDevice::Vega2dVertex> verts = OP_NEWA(VEGA3dDevice::Vega2dVertex, numVerts);
			for (unsigned int i=0; i<num_rects; i++)
			{
				verts[4*i].x = update_rects[i].Left();
				verts[4*i].y = tex->getHeight() - update_rects[i].Bottom();
				verts[4*i].s = (float)update_rects[i].Left() / tex->getWidth();
				verts[4*i].t = (float)update_rects[i].Bottom() / tex->getHeight();
				verts[4*i].color = ~0u;
				verts[4*i+1].x = update_rects[i].Right();
				verts[4*i+1].y = tex->getHeight() - update_rects[i].Bottom();
				verts[4*i+1].s = (float)update_rects[i].Right() / tex->getWidth();
				verts[4*i+1].t = (float)update_rects[i].Bottom() / tex->getHeight();
				verts[4*i+1].color = ~0u;
				verts[4*i+2].x = update_rects[i].Right();
				verts[4*i+2].y = tex->getHeight() - update_rects[i].Top();
				verts[4*i+2].s = (float)update_rects[i].Right() / tex->getWidth();
				verts[4*i+2].t = (float)update_rects[i].Top() / tex->getHeight();
				verts[4*i+2].color = ~0u;
				verts[4*i+3].x = update_rects[i].Left();
				verts[4*i+3].y = tex->getHeight() - update_rects[i].Top();
				verts[4*i+3].s = (float)update_rects[i].Left() / tex->getWidth();
				verts[4*i+3].t = (float)update_rects[i].Top() / tex->getHeight();
				verts[4*i+3].color = ~0u;
			};
			unsigned int start_index;
			b->writeAnywhere(numVerts, sizeof(VEGA3dDevice::Vega2dVertex), verts.get(), start_index);
			unsigned int firstInd = (start_index/4)*6;
			device->drawIndexedPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE, layout, device->getQuadIndexBuffer(firstInd + (numVerts/4)*6), 2, firstInd, (numVerts/4)*6);

			if (glAddSwapHintRectWIN)
			{
				for (unsigned int i=0; i<num_rects; i++)
					glAddSwapHintRectWIN(update_rects[i].x, tex->getHeight() - update_rects[i].y - update_rects[i].height, update_rects[i].width, update_rects[i].height);
			}
		}
		else
		{
			VEGA3dDevice::Vega2dVertex verts[] = {
				{0, 0, 0, 1.f, ~0u},
				{(float)tex->getWidth(), 0, 1.f, 1.f, ~0u},
				{0, (float)tex->getHeight(), 0, 0, ~0u},
				{(float)tex->getWidth(), (float)tex->getHeight(), 1.f, 0, ~0u}
			};
			unsigned int start_index;
			b->writeAnywhere(4, sizeof(VEGA3dDevice::Vega2dVertex), verts, start_index);
			device->drawPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE_STRIP, layout, start_index, 4);
		}

		device->setTexture(0, NULL);

		SwapBuffers(dc);
		VEGARefCount::DecRef(layout);
	}

	virtual VEGA3dFramebufferObject* getWindowFBO()
	{
		return fbo;
	}

	virtual OP_STATUS resizeBackbuffer(unsigned int width, unsigned int height)
	{
		if (width == this->width && height == this->height && tex)
			return OpStatus::OK;

		// when initializing this function could be called with 0 width/height
		if (!width || !height)
		{
			this->width = width;
			this->height = height;
			return OpStatus::OK;
		}

		if (!fbo)
			RETURN_IF_ERROR(device->createFramebuffer(&fbo));
		VEGA3dTexture* nt;
		RETURN_IF_ERROR(device->createTexture(&nt, width, height, VEGA3dTexture::FORMAT_RGBA8888));
		nt->setFilterMode(VEGA3dTexture::FILTER_NEAREST, VEGA3dTexture::FILTER_NEAREST);
		// Make sure the window is not active render target when deleting the texture
		if (g_vegaGlobals.vega3dDevice->getRenderTarget() == this)
			g_vegaGlobals.vega3dDevice->setRenderTarget(NULL);

		if (OpStatus::IsError(fbo->attachColor(nt)))
		{
			VEGA3dTexture::DecRef(nt);
			return OpStatus::ERR;
		}
		VEGA3dTexture::DecRef(tex);
		tex = nt;
		this->width = width;
		this->height = height;

		return OpStatus::OK;
	}

	virtual unsigned int getWidth(){return width;}
	virtual unsigned int getHeight(){return height;}

	virtual OP_STATUS readBackbuffer(VEGAPixelStore* pixel_store)
	{
		if(pixel_store && pixel_store->buffer 
				&& pixel_store->width == width && pixel_store->height == height
				&& pixel_store->stride == width*4)
		{
			device->setRenderTarget(fbo);
			VEGA3dRenderState* state = device->getDefault2dNoBlendNoScissorRenderState();
			device->setRenderState(state);
// GL_BGRA was added in OpenGL 1.2. It is desktop GL only, but that is OK in desktop code
#define GL_BGRA                           0x80E1
			glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, pixel_store->buffer);
		}
		else
		{
			OP_ASSERT(!"Layered windows is not going to work!");
			return OpStatus::ERR;
		}
		return OpStatus::OK;
	}

private:
	WindowsGlDevice* device;
	VEGAWindow* native_win;
	unsigned int width;
	unsigned int height;

	HWND hwin;
	HDC dc;

	VEGA3dTexture* tex;
	VEGA3dFramebufferObject* fbo;

	VEGA3dShaderProgram* m_shader;
};


OP_STATUS CreateGLDevice(VEGA3dDevice** dev VEGA_3D_NATIVE_WIN_DECL)
{
	*dev = NULL;
	WindowsGlDevice* wdev = OP_NEW(WindowsGlDevice, (VEGA_3D_NATIVE_WIN_NAME));
	if (!wdev)
		return OpStatus::ERR_NO_MEMORY;
	*dev = wdev;
	OP_STATUS err = wdev->Init();

	if (OpStatus::IsError(err))
	{
		OP_DELETE(wdev);
		*dev = NULL;
		return err;
	}

	if (!CommandLineManager::GetInstance()->GetArgument(CommandLineManager::GPUTest))
	{
		VEGABlocklistDevice::DataProvider* provider = wdev->CreateDataProvider();
		OpAutoPtr<VEGABlocklistDevice::DataProvider> p_provider(provider);
		OpString identifier;
		RETURN_IF_ERROR(GetBackendIdentifier(identifier, wdev->getDeviceType(), provider));
		BOOL3 status = GetBackendStatus(identifier);

		if (status == MAYBE)
		{
			RETURN_IF_ERROR(WriteBackendStatus(identifier, false));
			RETURN_IF_ERROR(SpawnGPUBenchmarkProcess(wdev->getDeviceType()));
			status = GetBackendStatus(identifier);
		}

		if (status == NO)
		{
			OP_DELETE(wdev);
			*dev = NULL;
			return OpStatus::ERR;
		}
	}

	FillGpuInfo(wdev);

	return err;
}

unsigned int WindowsGlDevice::getDeviceType() const
{
	return PrefsCollectionCore::OpenGL;
}

WindowsGlDevice::WindowsGlDevice(VEGAWindow* nativeWin)
{
	m_window = VEGA_3D_NATIVE_WIN_NAME;
	m_hwin = NULL;
	m_dc = NULL;
	m_rc = NULL;
	m_activeDC = NULL;
	
	m_initialized = false;
	m_has_swapcopy = false;

	WNDCLASSEX windowClass;
	op_memset(&windowClass, 0, sizeof (WNDCLASSEX));
	windowClass.cbSize = sizeof (WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC; // redraw for move and resize.
	windowClass.lpfnWndProc = (WNDPROC)(WindowProc3D);
	windowClass.hInstance = GetModuleHandle(NULL); // hInstance
	windowClass.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE);
	windowClass.hIconSm = LoadIcon(NULL,IDI_APPLICATION);
	windowClass.lpszClassName = UNI_L("OperaGLDevice");
	RegisterClassEx(&windowClass);
}

WindowsGlDevice::~WindowsGlDevice()
{
	DeviceDeleted(this);

	if (m_rc)
	{
		pwglMakeCurrent(NULL, NULL);
		pwglDeleteContext(m_rc);
	}
	if (m_dc)
		ReleaseDC(m_hwin, m_dc);
	if (m_hwin)
		DestroyWindow(m_hwin);

	UnregisterClass(UNI_L("OperaGLDevice"), GetModuleHandle(NULL));
}

OP_STATUS WindowsGlDevice::createGLWindow(HWND& hwin, HDC& dc, HGLRC& rc, const uni_char* winclass, unsigned int width, unsigned int height, bool visible)
{
	HINSTANCE hInstance = GetModuleHandle(NULL);
	DWORD dwStyle, dwExStyle;

	dwExStyle = WS_EX_TOOLWINDOW;
	if (visible)
		dwExStyle = WS_EX_APPWINDOW | WS_EX_OVERLAPPEDWINDOW;
	else
		dwExStyle = WS_EX_TOOLWINDOW;
	if (visible)
		dwStyle = WS_OVERLAPPEDWINDOW;
	else
		dwStyle = WS_POPUP;




	RECT windowRect = {0, 0, width, height};
	AdjustWindowRectEx(&windowRect, dwStyle, 0, dwExStyle);
	windowRect.left -= windowRect.right;
	windowRect.right = 0;

	hwin = CreateWindowEx(dwExStyle,
		  winclass, 
		  UNI_L("Opera GL Window"), 
		  dwStyle,
		  0, 0, 
		  windowRect.right - windowRect.left, 
		  windowRect.bottom - windowRect.top, 
		  HWND_DESKTOP, 
		  NULL, 
		  hInstance, 
		  NULL);

	if (!hwin)
		return OpStatus::ERR;
	RETURN_IF_ERROR(initializeGLWindow(hwin, dc));
	rc = pwglCreateContext(dc);
	if (!rc)
		return OpStatus::ERR;
	return OpStatus::OK;
}
OP_STATUS WindowsGlDevice::initializeGLWindow(HWND hwin, HDC& dc)
{
	dc = GetDC(hwin);
	if (!dc)
		return OpStatus::ERR;

	int bitdepth = GetDeviceCaps(dc, BITSPIXEL);

	PIXELFORMATDESCRIPTOR pfd;
	int count = DescribePixelFormat(dc, 1, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

	int pf_index = 0;
	DWORD pf_flags = 0;
	int points = 0;
	for (int i=0; i<count; i++)
	{
		if (DescribePixelFormat(dc, i+1, sizeof(PIXELFORMATDESCRIPTOR), &pfd)
			&& pfd.dwFlags & PFD_SUPPORT_OPENGL
			&& pfd.dwFlags & PFD_DRAW_TO_WINDOW
			&& !(pfd.dwFlags & PFD_GENERIC_FORMAT)
			&& pfd.cColorBits == bitdepth
			&& pfd.iPixelType == PFD_TYPE_RGBA
			&& pfd.iLayerType == PFD_MAIN_PLANE)
		{
			int new_point = (pfd.dwFlags & PFD_SWAP_COPY ? 1 : 0)
				+ (pfd.dwFlags & PFD_DOUBLEBUFFER ? 2 : 0)
				+ (pfd.dwFlags & PFD_SUPPORT_COMPOSITION ? 4 : 0);
			if (new_point > points)
			{
				pf_index = i+1;
				points = new_point;
				pf_flags = pfd.dwFlags;
			}
		}
	}

	if(pf_index == 0)
		return OpStatus::ERR;

	m_has_swapcopy = !!(pf_flags & PFD_SWAP_COPY);

	// Now set the pixel format.
	if (!SetPixelFormat(dc, pf_index, &pfd))
		return OpStatus::ERR;

	return OpStatus::OK;
}

void WindowsGlDevice::activateDC(HDC dc)
{
	if (!dc)
		dc = m_dc;
	if (dc != m_activeDC)
	{
		m_activeDC = dc;
		pwglMakeCurrent(m_activeDC, m_rc);
	}
}

OP_STATUS WindowsGlDevice::Construct(VEGABlocklistFileEntry* blocklist_entry)
{
	RETURN_IF_ERROR(InitDevice());
	return VEGAGlDevice::Construct(blocklist_entry);
}

OP_STATUS WindowsGlDevice::InitDevice()
{
	if (m_initialized)
		return OpStatus::OK;

	m_opengl32_dll = WindowsUtils::SafeLoadLibrary(UNI_L("opengl32.dll"));

	if (!pwglDeleteContext)
	{
		pwglDeleteContext	= (WGLDELETECONTEXT)::GetProcAddress(m_opengl32_dll, "wglDeleteContext");
		pwglMakeCurrent		= (WGLMAKECURRENT)::GetProcAddress(m_opengl32_dll, "wglMakeCurrent");
		pwglCreateContext	= (WGLCREATECONTEXT)::GetProcAddress(m_opengl32_dll, "wglCreateContext");
		pwglGetProcAddress	= (WGLGETPROCADDRESS)::GetProcAddress(m_opengl32_dll, "wglGetProcAddress");
	}
	if (!pwglDeleteContext || !pwglMakeCurrent || !pwglCreateContext || !pwglGetProcAddress)
		return OpStatus::ERR;

	RETURN_IF_ERROR(createGLWindow(m_hwin, m_dc, m_rc, UNI_L("OperaGLDevice"), 16, 16, false));

	m_activeDC = m_dc;
	pwglMakeCurrent(m_dc, m_rc);

	if (!m_glapi.get())
		RETURN_IF_ERROR(InitAPI());

	glAddSwapHintRectWIN = (GLADDSWAPHINTRECTWIN)pwglGetProcAddress("glAddSwapHintRectWIN");

#ifndef VEGA_BACKEND_VSYNC
	wglSwapIntervalEXT = (PFNWGLSWAPINTERVALFARPROC)pwglGetProcAddress("wglSwapIntervalEXT");

	if (wglSwapIntervalEXT)
		wglSwapIntervalEXT(0);
#endif // !VEGA_BACKEND_VSYNC

	m_initialized = true;
	return OpStatus::OK;
}

OP_STATUS WindowsGlDevice::createWindow(VEGA3dWindow** win, VEGAWindow* nativeWin)
{
	HWND hwin;
	HDC hdc;
	hwin = (HWND)nativeWin->getNativeHandle();
	initializeGLWindow(hwin, hdc);

	WindowsGlWindow* glwin = OP_NEW(WindowsGlWindow, (this, nativeWin, hwin, hdc));
	if (!glwin)
	{
		ReleaseDC(hwin, hdc);
		return OpStatus::ERR_NO_MEMORY;
	}
	if (OpStatus::IsError(glwin->Construct()))
	{
		OP_DELETE(glwin);
		return OpStatus::ERR; // FIXME: error type?
	}
	*win = glwin;

	return OpStatus::OK;
}

void* WindowsGlDevice::getGLFunction(const char* funcName)
{
	// This will load all OpenGL extension functions or OpenGL core function.
	void* f = pwglGetProcAddress(funcName);

	// If it is an OpenGL 1.1 or earlier function, we have to load them using GetProcAddress.
	if (!f)
		f = GetProcAddress(m_opengl32_dll, funcName);

	return f;
}

#endif // VEGA_BACKEND_OPENGL

// static
OP_STATUS VEGA3dDeviceFactory::Create(unsigned int device_type_index, VEGA3dDevice** dev VEGA_3D_NATIVE_WIN_DECL)
{
	// If --nohwaccel is passed as an argument. We never end up here,
	// because DeviceTypeCount will return 0 devices.

	CommandLineArgument* gpu_test = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::GPUTest);
	if (gpu_test)
	{
#ifdef VEGA_BACKEND_OPENGL
		if (gpu_test->m_int_value == PrefsCollectionCore::OpenGL)
		{
			return CreateGLDevice(dev VEGA_3D_NATIVE_WIN_PASS);
		}
#endif
#ifdef VEGA_BACKEND_DIRECT3D10
		if (gpu_test->m_int_value == PrefsCollectionCore::DirectX10)
		{
			return CreateD3D10Device(dev VEGA_3D_NATIVE_WIN_PASS, D3D10_DRIVER_TYPE_HARDWARE);
		}

		if (gpu_test->m_int_value == PrefsCollectionCore::DirectX10Warp)
		{
			return CreateD3D10Device(dev VEGA_3D_NATIVE_WIN_PASS, D3D10_DRIVER_TYPE_WARP);
		}
#endif
		return OpStatus::ERR;
	}

	//
	// Initialize either the the renderer the calling function asks for, or the preferred one if chosen.
	//

	switch (device_type_index)
	{
	case PrefsCollectionCore::OpenGL:
		return CreateGLDevice(dev VEGA_3D_NATIVE_WIN_PASS);
		break;
	case PrefsCollectionCore::DirectX10:
		return CreateD3D10Device(dev VEGA_3D_NATIVE_WIN_PASS, D3D10_DRIVER_TYPE_HARDWARE);
		break;
	case PrefsCollectionCore::DirectX10Warp:
		// We do not want to fall back on the warp device. But if the user choses it explicitly, then we should not refuse.
		if (PrefsCollectionCore::DirectX10Warp == g_pccore->GetIntegerPref(PrefsCollectionCore::PreferredRenderer))
			return CreateD3D10Device(dev VEGA_3D_NATIVE_WIN_PASS, D3D10_DRIVER_TYPE_WARP);
		break;
	default:
		OP_ASSERT(!"Back-end index out of range. Either fix the calling code, or add the new back-end here if there is one.");
		break;
	}

	return OpStatus::ERR;
}

// static
unsigned int VEGA3dDeviceFactory::DeviceTypeCount()
{
	// If Hardware acceleration is turned off, then do not initialize any of the HW back-ends.
	if (g_desktop_bootstrap->GetHWAccelerationDisabled())
		return 0;

	if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::GPUTest))
		return 1;

	// We do not want to encourage users to use the Warp device for normal browsing. So hide it from opera:gpu as long as
	// the user has not chosen it explicitly.
	// It should still be possible to enable it. But the user has to set PreferredRenderer to exactly 2, as it can still
	// be useful for WebGL devs to have a reference back-end.
	if (PrefsCollectionCore::DirectX10Warp != g_pccore->GetIntegerPref(PrefsCollectionCore::PreferredRenderer))
		return PrefsCollectionCore::DeviceCount - 1;

	return PrefsCollectionCore::DeviceCount;
}

// static
const uni_char* VEGA3dDeviceFactory::DeviceTypeString(UINT16 device_type_index)
{
	switch (device_type_index)
	{
#ifdef VEGA_BACKEND_OPENGL
	case PrefsCollectionCore::OpenGL:
		return UNI_L("OpenGL");
		break;
#endif // VEGA_BACKEND_OPENGL
#ifdef VEGA_BACKEND_DIRECT3D10
	case PrefsCollectionCore::DirectX10:
		return UNI_L("Direct3D 10");
		break;
	case PrefsCollectionCore::DirectX10Warp:
		return UNI_L("Direct3D 10 Warp");
		break;
#endif // VEGA_BACKEND_DIRECT3D10
	}

	OP_ASSERT(!"unhandled 3d device index");
	return 0;
}

OP_STATUS GetBackendIdentifier(OpString& section, unsigned int backend, OpStringC device_id, OpStringC driver_version)
{
	return section.AppendFormat(UNI_L("%s %s %s %s"), g_op_system_info->GetPlatformStr(),
		VEGA3dDeviceFactory::DeviceTypeString(backend), device_id, driver_version);
}

OP_STATUS GetBackendIdentifier(OpString& section, unsigned int backend, VEGABlocklistDevice::DataProvider* provider)
{
	OpString device_id, driver_version;
	provider->GetValueForKey(UNI_L("device id"), device_id);
	provider->GetValueForKey(UNI_L("driver version"), driver_version);
	return GetBackendIdentifier(section, backend, device_id, driver_version);
}
