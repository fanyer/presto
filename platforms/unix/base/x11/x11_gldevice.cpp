/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#include "modules/libvega/vegaconfig.h"
#if defined(VEGA_BACKEND_OPENGL)
#include "platforms/vega_backends/opengl/vegaglapi.h"
#include "platforms/vega_backends/opengl/vegagldevice.h"
#include "platforms/vega_backends/opengl/vegaglwindow.h"
#include "modules/libvega/vegawindow.h"

#include "adjunct/desktop_util/boot/DesktopBootstrap.h"
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"
#include "modules/pi/OpDLL.h"
#include "platforms/crashlog/crashlog.h"
#include "platforms/crashlog/gpu_info.h"
#include "platforms/quix/commandline/StartupSettings.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_gl.h"
#include "platforms/unix/base/x11/x11_mdescreen.h"
#include "platforms/unix/base/x11/x11_opmessageloop.h"
#include "platforms/unix/base/x11/x11_vegawindow.h"

#include "platforms/vega_backends/vega_blocklist_file.h"
#include <signal.h>
#include <sys/utsname.h>
#include <sys/wait.h>

/* We really should not include any OpenGL headers directly.  All
 * necessary symbols should be explicitly declared by vegaglapi.h (for
 * OpenGL) and x11_gl.h (for GLX).  This allows for better control of
 * what we're doing as well as providing a point to implement extra
 * debugging support.
 */
#ifdef GLX_H
#error Do not include glx.h!  x11_gl.h shall provide all necessary GLX symbols.
#endif
#if 0 // Until this define is removed from core
#ifdef __gl_h_
#error Do not include gl.h! platforms/vega_backends/opengl/vegaglapi.h shall provide all necessary GL symbols.
#endif
#endif

class X11_GlDevice;

class X11_GlDevice : public VEGAGlDevice
{
public:
	X11_GlDevice():
		m_lib_ok(MAYBE),
		m_gl_ok(MAYBE),
		m_libGL(0),
		m_altctx(NULL)
	{}
	virtual ~X11_GlDevice()
	{
		ShutdownGL();
	}
	OP_STATUS Init() { return OpStatus::OK; }
	OP_STATUS Construct(VEGA_3D_BLOCKLIST_ENTRY_DECL);

	virtual OP_STATUS GenerateSpecificBackendInfo(class OperaGPU* page, VEGABlocklistFile* blocklist, VEGABlocklistDevice::DataProvider* provider);

	virtual OP_STATUS createWindow(VEGA3dWindow** win, VEGAWindow* nativeWin);

	virtual unsigned int getDeviceType() const { return 0; }
	virtual BlocklistType GetBlocklistType() const { return UnixGL; }
	virtual DataProvider* CreateDataProvider() { return OP_NEW(DataProvider, (this)); }

	virtual OP_STATUS setRenderTarget(VEGA3dRenderTarget* rtex, bool changeViewport);
	virtual void* getGLFunction(const char* funcName);

	GLX::Context GetGLContext() const { return m_globalGLContext.renderContext; };

	/** Obtain a GLX rendering context to be used for ugly hacks and
	 * debugging.  See FlipFlopContexts() for an example.
	 */
	GLX::Context GetAltContext();

	/** Switch to a different context and then back again to the
	 * currently active context.  This can be used to try to force the
	 * driver to reset parts of the context data, and seems to protect
	 * against a crash on the nvidia driver version 270.41.06 (and
	 * probably nearby versions).
	 */
	void FlipFlopContexts();


	struct GLRenderContext
	{
		GLRenderContext():
			window(None),
			glxwindow(None),
			renderContext(0) {}
		/* Fallback window to be set as active window for
		 * 'renderContext' when no other window is available.  Will
		 * never be displayed.
		 */
		X11Types::Window window;
		/* The GLXWindow associated with 'window' (when applicable).
		 */
		GLX::Window glxwindow;
		GLX::Context renderContext;
	};

	static bool s_use_glxwindow;

private:
	GLRenderContext m_globalGLContext;

	bool IsOpenGLSafe();

	OP_STATUS InitGL();
	void ShutdownGL();

	// These are parts of InitGL()
	OP_STATUS LoadGLLibrary();
	OP_STATUS ChooseFBConfig(GLX::FBConfig * ret_fbconfig, XVisualInfo ** ret_vi);
	OP_STATUS CreateDefaultGLXObjects(GLX::FBConfig fbconfig, XVisualInfo * vi);
	OP_STATUS SetGlobalGLVisuals(GLX::FBConfig fbconfig, XVisualInfo * vi);

	BOOL3 m_lib_ok;
	BOOL3 m_gl_ok;

	class DataProvider: public VEGABlocklistDevice::DataProvider
	{
	public:
		DataProvider(X11_GlDevice* dev): m_dev(dev), m_utsname_ok(MAYBE) {}
		virtual OP_STATUS GetValueForKey(const uni_char* key, OpString& val);

	private:
		X11_GlDevice* m_dev;
		utsname m_utsname;
		BOOL3 m_utsname_ok;

		/* Returns the amount of "local" memory for the GPU.  This is
		 * used to as an estimate of the amount of memory the graphics
		 * driver can allocate for use by the GPU before performance
		 * starts to suffer.  For discrete cards, this should
		 * typically be the "memory size" given by vendors.  For other
		 * graphics solutions, it is as yet unclear what the correct
		 * value to report is.
		 *
		 * Returns -1 if the value is not reliably detected.
		 */
		int QueryDedicatedVideoMemory();

		/* Returns the total amount of memory available for use by the
		 * graphics driver.
		 *
		 * Returns -1 if the value is not reliably detected.
		 */
		int QueryTotalVideoMemory();
	};

	typedef const GLubyte* (*GetString_t)(GLenum);
	GetString_t m_GetString;

	OpDLL* m_libGL;

	/** Used to work around rendering and crash bugs in some drivers.
	 * E.g. see FlipFlopContexts().
	 *
	 * Can also be used for debugging (e.g. to render something
	 * without touching the "real" context.)
	 */
	GLX::Context m_altctx;

	static bool s_quirk_nvidia270resetcontext;
};
bool X11_GlDevice::s_use_glxwindow = true;
bool X11_GlDevice::s_quirk_nvidia270resetcontext = false;

// static
OP_STATUS VEGA3dDeviceFactory::Create(unsigned int device_type_index, VEGA3dDevice** dev VEGA_3D_NATIVE_WIN_DECL)
{
	OpAutoPtr<VEGA3dDevice> new_dev;

	switch (device_type_index)
	{
	case 0:
		new_dev.reset(OP_NEW(X11_GlDevice, ()));
		break;

	default:
		OP_ASSERT(!"unknown device index");
		return OpStatus::ERR;
	}

	RETURN_OOM_IF_NULL(new_dev.get());
	RETURN_IF_ERROR(new_dev->Init());
	*dev = new_dev.release();
	return OpStatus::OK;
}

// static
unsigned int VEGA3dDeviceFactory::DeviceTypeCount()
{
	return 1;
}

// static
const uni_char* VEGA3dDeviceFactory::DeviceTypeString(UINT16 device_type_index)
{
	switch (device_type_index)
	{
	case 0:
		return UNI_L("OpenGL");

	default:
		OP_ASSERT(!"unknown device index");
		return 0;
	}
}

OP_STATUS X11_GlDevice::DataProvider::GetValueForKey(const uni_char* key, OpString& val)
{
	OpStringC k(key);
	if (k.Compare(UNI_L("OpenGL-"), 7) == 0)
	{
		RETURN_IF_ERROR(m_dev->InitGL());
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
		const GLubyte* str = m_dev->m_GetString(name);
		RETURN_VALUE_IF_NULL(str, OpStatus::ERR);
		return val.Set(reinterpret_cast<const char*>(str));
	}
	else if (k.Compare(UNI_L("GLX-"), 4) == 0)
	{
		RETURN_IF_ERROR(m_dev->InitGL());
		if (k.Compare(UNI_L("GLX-client-"), 11))
		{
			int name;
			if (k == UNI_L("GLX-client-vendor"))
				name = GLX::VENDOR;
			else if (k == UNI_L("GLX-client-version"))
				name = GLX::VERSION;
			else if (k == UNI_L("GLX-client-extensions"))
				name = GLX::EXTENSIONS;
			else
				return OpStatus::ERR;
			const char* str = GLX::GetClientString(g_x11->GetDisplay(), name);
			RETURN_VALUE_IF_NULL(str, OpStatus::ERR);
			return val.Set(str);
		}
		else if (k.Compare(UNI_L("GLX-server-"), 11))
		{
			int name;
			if (k == UNI_L("GLX-server-vendor"))
				name = GLX::VENDOR;
			else if (k == UNI_L("GLX-server-version"))
				name = GLX::VERSION;
			else if (k == UNI_L("GLX-server-extensions"))
				name = GLX::EXTENSIONS;
			else
				return OpStatus::ERR;
			const char* str = GLX::QueryServerString(g_x11->GetDisplay(), g_x11->GetDefaultScreen(), name);
			RETURN_VALUE_IF_NULL(str, OpStatus::ERR);
			return val.Set(str);
		}
		else if (k == "GLX-version")
		{
			int major, minor;
			if (!GLX::QueryVersion(g_x11->GetDisplay(), &major, &minor))
				return OpStatus::ERR;
			val.Empty();
			return val.AppendFormat("%d.%d", major, minor);
		}
		else if (k == "GLX-direct")
		{
			RETURN_IF_ERROR(m_dev->InitGL());
			return val.Set(GLX::IsDirect(g_x11->GetDisplay(), m_dev->GetGLContext()) ? UNI_L("1") : UNI_L("0"));
		}
		else
			return OpStatus::ERR;
	}
	else if (k == "Dedicated Video Memory")
	{
		int mem = QueryDedicatedVideoMemory();
		if (mem >= 0)
		{
			val.Empty();
			return val.AppendFormat("%d", mem);
		}
		return OpStatus::ERR;
	}
	else if (k == "Total Video Memory")
	{
		int mem = QueryTotalVideoMemory();
		if (mem >= 0)
		{
			val.Empty();
			return val.AppendFormat("%d", mem);
		}
		return OpStatus::ERR;
	}
	else if (k.Compare(UNI_L("uname-"), 6) == 0)
	{
		if (m_utsname_ok == MAYBE)
			m_utsname_ok = uname(&m_utsname) == 0 ? YES : NO;
		if (m_utsname_ok == NO)
			return OpStatus::ERR;
		if (k == UNI_L("uname-sysname"))
			return val.Set(m_utsname.sysname);
		else if (k == UNI_L("uname-release"))
			return val.Set(m_utsname.release);
		else if (k == UNI_L("uname-version"))
			return val.Set(m_utsname.version);
		else if (k == UNI_L("uname-machine"))
			return val.Set(m_utsname.machine);
		else
			return OpStatus::ERR;
	}
	else
		return OpStatus::ERR;
}

static bool check_gl_extension(const GLubyte * haystack, const char * needle)
{
	if (haystack == NULL)
		return false;
	const char * c_haystack = (const char *)haystack;
	int needlelen = op_strlen(needle);
	const char * cand = op_strstr(c_haystack, needle);
	while (cand != NULL)
	{
		if ((cand == c_haystack || cand[-1] == ' ') &&
			(cand[needlelen] == 0 || cand[needlelen] == ' '))
			return true;
		cand = op_strstr(cand + 1, needle);
	}
	return false;
}

int X11_GlDevice::DataProvider::QueryDedicatedVideoMemory()
{
	/* Candidates for getting this value:
	 * - GL_NVX_gpu_memory_info
	 *   - Only supported by nvidia proprietary driver, I think.
	 * - GL_ATI_meminfo
	 *   - Only supported by catalyst/fglrx, I think.
	 *   - I don't think it reports this value nor "total video memory" (only free memory)
	 * - GLX_AMD_gpu_association
	 *   - Only supported by catalyst/fglrx, I think.
	 *   - Does not seem to have a value for "total video memory".
	 * - NV-CONTROL
	 *   - Only supported by nvidia proprietary driver, I think.
	 *   - total video memory is probably NV_CTRL_VIDEO_RAM
	 * - ATIFGLEXTENSION
	 *   - Only supported by catalyst/fglrx, I think.
	 *   - License agreement needs examination.
	 * - lspci (or its underlying code)
	 *   - linux: sys/bus/pci?
	 *   - FreeBSD: /dev/pci?
	 *   - Hard to make sure we get the right card.
	 *   - Or that the gfx card is even in the list.
	 *   - The value itself is probably not available, so guesswork is needed.
	 * - Xorg.<screen>.log
	 *   - Hard to make sure we get the right log.
	 *   - Requires parsing non-standard data.  Ouch.
	 */
	RETURN_VALUE_IF_ERROR(m_dev->InitGL(), -1);
	/* This method is called before our GL layer is initialized.  So
	 * we need to manually grab the functions we need here.
	 */
	void (*getintv)(GLenum,GLint*) = (void(*)(GLenum,GLint*))GLX::GetProcAddress("glGetIntegerv");
	const GLubyte * exts = m_dev->m_GetString(GL_EXTENSIONS);
	if (check_gl_extension(exts, "GL_NVX_gpu_memory_info"))
	{
		const int local_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX = 0x9047;
		GLint mem=0;
		if (getintv)
			getintv(local_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &mem);
		if (mem > 0)
			return mem / 1024;
	}
	if (GLX::HasExtensionAMDGpuAssociation(g_x11->GetDisplay(), g_x11->GetDefaultScreen()))
	{
		unsigned int gpuid = GLX::GetContextGPUIDAMD(m_dev->GetGLContext());
		if (gpuid > 0)
		{
			GLuint mem[2];
			int got = GLX::GetGPUInfoAMD(gpuid, GLX::GPU_RAM_AMD, GL_UNSIGNED_INT, 2, mem);
			if (got > 0)
			{
				OP_ASSERT(got == 1); // I expect to get only a single answer here
				return mem[0];
			}
		}
	}
	return -1;
}

int X11_GlDevice::DataProvider::QueryTotalVideoMemory()
{
	/* Candidates for getting this value are much the same as for
	 * QueryDedicatedVideoMemory().
	 */
	RETURN_VALUE_IF_ERROR(m_dev->InitGL(), -1);
	/* This method is called before our GL layer is initialized.  So
	 * we need to manually grab the functions we need here.
	 */
	void (*getintv)(GLenum,GLint*) = (void(*)(GLenum,GLint*))GLX::GetProcAddress("glGetIntegerv");
	const GLubyte * exts = m_dev->m_GetString(GL_EXTENSIONS);
#if 0
	/* On my nVidia GeForce 8800GT this returns 512 MiB, which is the
	 * same as the on-board memory.  I doubt very much the driver
	 * limits itself to using only on-board memory.
	 */
	if (check_gl_extension(exts, "GL_NVX_gpu_memory_info"))
	{
		const int local_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX = 0x9048;
		GLint mem=0;
		if (getintv)
			getintv(local_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &mem);
		if (mem > 0)
			return mem / 1024;
	}
#endif
	return -1;
}

namespace {
/* Object that automatically sets (typically to reset) a signal
 * handler when being destroyed.
 */
class SigResetter
{
public:
	/* When the object is destroyed, the signal 'signum' will have its
	 * handler set to 'handler'.  This action can be triggered early
	 * by calling reset_signal().  The signal handler will only be set
	 * once, no matter how many calls are made to reset_signal().
	 *
	 * IMPORTANT: 'handler' is used as-is, so must have lifetime as long
	 * as it may be needed (that is, until either reset_signal() is
	 * called or the object is destroyed.  Whatever comes first.)
	 */
	SigResetter(int signum, struct sigaction * handler)
		: m_signum(signum)
		, m_handler(handler)
	{
		OP_ASSERT(m_signum != 0); // Because 0 is a magic value for m_signum.
	}

	~SigResetter()
	{
		reset_signal();
	}

	void reset_signal()
	{
		if (m_signum == 0)
			return;
		/* If this fails, it may be problematic, but I don't think
		 * there's much we can do about it.
		 */
		sigaction(m_signum, m_handler, NULL);
		m_signum = 0;
	}

	/* if m_signum is 0, it means that reset_signal() should have no
	 * effect.  Probably because it has been called early or has been
	 * cancelled.  Posix requires that for a call to kill() with
	 * signal number 0, "no signal is actually sent."  This probably
	 * means that 0 will never be a valid signal number.
	 */
	int m_signum;
	struct sigaction * m_handler;
};
} // anonymous namespace

bool X11_GlDevice::IsOpenGLSafe()
{
	static bool is_ok = false;
	static bool test_performed = false;
	if (test_performed)
		return is_ok;
	test_performed = true;

	struct sigaction oldchldhandler;
	struct sigaction chldhandler;
	chldhandler.sa_flags = 0;
	chldhandler.sa_handler = SIG_DFL;

	/* Set up the SIGCHLD handler so we are sure we get told about the
	 * test process exiting.  This may introduce a race condition if
	 * other SIGCHLD handlers need to know about processes that die
	 * before we reset the handler.  Also, any other processes dying
	 * during this window may not be reaped until opera exits.
	 */
	if (sigaction(SIGCHLD, &chldhandler, &oldchldhandler) != 0)
	{
		/* In this case, there's no way for the child to report back. */
		/* Given that we know the parameters to sigaction() are
		 * correct and safe here, it should be pretty near impossible
		 * for it to fail, though.  So I won't waste the ~75 bytes it
		 * takes to print an error message here.
		 */
		return false;
	}
	SigResetter sigresetter(SIGCHLD, &oldchldhandler);

	pid_t child = fork();
	if (child < 0)
	{
		/* Failed to fork.  There are probably going to be plenty of
		 * other problems, so I think we'll avoid OpenGL this time.
		 */
		return false;
	}

	if (child > 0)
	{
		/* Parent.  Our SIGCHLD handler is currently set up to
		 * automatically reap zombies, so some magic must be done
		 * around the fork to ensure wait() will work correctly.
		 */
		while (true)
		{
			int status = 1;
			pid_t ended = waitpid(child, &status, 0);
			OP_ASSERT(ended == -1 || ended == child);
			if (ended < 0)
			{
				if (errno != EINTR)
				{
					/* Maybe printf should be controlled by a
					 * -debugsomething.  But then again, it will only
					 * show up if something actually fails, in which
					 * case printing the message unconditionally may
					 * be more useful.
					 */
					printf("opera: Failed to get result of OpenGL safety check.  OpenGL will be disabled  for this session.\n");
					return false;
				}
			}
			else if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
			{
				is_ok = true;
				return true;
			}
			else
			{
				/* Maybe printf should be controlled by a
				 * -debugsomething.  But then again, it will only show
				 * up if something actually fails, in which case
				 * printing the message unconditionally may be more
				 * useful.
				 */
				if (WIFEXITED(status))
					printf("opera: OpenGL safety check failed (error code %d).  OpenGL will be disabled for this session.\n", WEXITSTATUS(status));
				else if (WIFSIGNALED(status))
					printf("opera: OpenGL safety check failed (signal %d). OpenGL will be disabled for this session.\n", WTERMSIG(status));
				else
					printf("opera: OpenGL safety check failed (unknown reason). OpenGL will be disabled for this session.\n");
				return false;
			}
		}
	}
	OP_ASSERT(child == 0);

	/* In child.
	 *
	 *  Now see if OpenGL is safe to use.  Try to get as far as being
	 *  able to use the blocklist to inhibit further crashes.
	 */

	/* It is totally counterproductive to trigger the crash logger in
	 * the child process.
	 */
	RemoveCrashSignalHandler();

	/* In the child, we want to try to start OpenGL, so make sure
	 * IsOpenGLSafe replies OK in the child.
	 */
	is_ok = true;

	/* Also, InitGL() itself must not think it is already running. */
	m_gl_ok = MAYBE;

	/* But we don't want to use the same X11 connection.  Since we
	 * share the connection with the parent, if we mess up the
	 * connection's state it will be messed up for the parent to.
	 */
	g_x11 = NULL;
	if (OpStatus::IsError(X11Globals::Create()))
		_exit(1);

	if (OpStatus::IsError(InitGL()))
		_exit(2);

	/* Check that some basic function calls do not crash.  If these
	 * work, we can probably use the blocklist to protect against the
	 * worst problems.
	 */
	m_GetString(GL_VENDOR);
	m_GetString(GL_RENDERER);
	m_GetString(GL_VERSION);

	// I'm assuming the operating system will clean up any dangling resources

	_exit(0);
}


OP_STATUS X11_GlDevice::LoadGLLibrary()
{
	if (m_lib_ok == MAYBE) {

		m_lib_ok = NO;

		RETURN_IF_ERROR(OpDLL::Create(&m_libGL));
		if (OpStatus::IsError(m_libGL->Load(UNI_L("libGL.so.1"))))
		{
			OP_DELETE(m_libGL);
			m_libGL = 0;
			return OpStatus::ERR;
		}

		RETURN_IF_ERROR(g_x11->UnloadOnShutdown(m_libGL)); // no unloading on error because it can cause a crash

		m_GetString = reinterpret_cast<GetString_t>(m_libGL->GetSymbolAddress("glGetString"));
		if (!m_GetString || !GLX::LoadSymbols(m_libGL))
			return OpStatus::ERR;

		m_lib_ok = YES;
	}
	return m_lib_ok == YES ? OpStatus::OK : OpStatus::ERR;
}

OP_STATUS X11_GlDevice::ChooseFBConfig(GLX::FBConfig * ret_fbconfig, XVisualInfo ** ret_vi)
{
	/* If XRender is available, we may try to use semi-transparent
	 * windows.  In that case, chances are we will want to manipulate
	 * the alpha channel as well.  Otherwise, we don't need an alpha
	 * channel.
	 */
	int minimum_alpha_bits = 0;
	if (g_x11->IsXRenderAvailable(g_x11->GetDefaultScreen()) &&
		!g_startup_settings.no_argb)
		minimum_alpha_bits = 1;

	const int winAttr[] =
		{
			/* GLX_RGBA_BIT is the default, but in this case I think
			 * it is more readable to have it stated explicitly.
			 */
			GLX::RENDER_TYPE, GLX::RGBA_BIT,

			/* We need to require at least 1 bit for each channel we
			 * wish to be present.  Otherwise GLX is allowed to return
			 * an fbconfig with 0 bits in that channel and will also
			 * typically prefer to do so.
			 *
			 * For example, a 1-bit monochrome fbconfig would be
			 * preferred over a 24-bit RGB fbconfig.  The sorting
			 * proceeds as follows: Both fbconfigs have the same value
			 * for GLX_CONFIG_CAVEAT.  Both fbconfigs have the same
			 * number of bits in every channel where more than 0 bits
			 * are requested (when we request 0 bits for all
			 * channels).  The 1-bit config has fewer bits in total
			 * than the 24-bit fbconfig.  Thus the 1-bit config is
			 * preferred.
			 *
			 * (Also, note that pseudocolor and grayscale visuals are
			 * mapped to the red channel, and MAY have 0 bits in the
			 * other channels.  This could be seen as an argument for
			 * allowing 0 bits for blue and green in order to
			 * "support" grayscale rendering, but that would again
			 * mean that grayscale and pseudocolor would be preferred
			 * over RGB.  Which we certainly don't want.)
			 */
			GLX::RED_SIZE, 1,
			GLX::BLUE_SIZE, 1,
			GLX::GREEN_SIZE, 1,
			GLX::ALPHA_SIZE, minimum_alpha_bits,

			/* I'm not entirely sure we actually need to use double
			 * buffered windows, since we are generally rendering to
			 * offscreen buffers anyway.  But the current code expects
			 * it, so we need to require it here.
			 */
			GLX::DOUBLEBUFFER, True,

			/* Yes, I think we sometimes will want to render to some
			 * windows using plain X.  x11_icon and x11_dragwidget
			 * comes to mind.
			 */
			GLX::X_RENDERABLE, True,

			None
		};

	int fbcount = 0;
	GLX::FBConfig * fbconfigs = GLX::ChooseFBConfig(g_x11->GetDisplay(), g_x11->GetDefaultScreen(), winAttr, &fbcount);
	if (!fbconfigs)
		return OpStatus::ERR;
	GLX::FBConfig fallbackfbconfig = 0;
	XVisualInfo * fallbackvi = NULL;
	for (int fbconfnum = 0; fbconfnum < fbcount; fbconfnum += 1)
	{
		XVisualInfo * vi = GLX::GetVisualFromFBConfig(g_x11->GetDisplay(), fbconfigs[fbconfnum]);
		if (vi)
		{
			/* If we want an alpha channel, we also have to make sure
			 * the visual has an alpha channel.  GLX is perfectly
			 * happy to give us a GLXFBConfig supporting alpha with a
			 * visual that does not.
			 *
			 * I'm assuming that any visual with more than 30 bits per
			 * pixel has an alpha channel (and that the presence of an
			 * alpha channel gives a depth of more than 30 bits).
			 * This is based on an assumption of 8-10 bits per color
			 * channel and no extraneous bits accounted for by
			 * 'depth'.
			 */
			if (minimum_alpha_bits > 0 && vi->depth > 30)
			{
				*ret_fbconfig = fbconfigs[fbconfnum];
				*ret_vi = vi;
				if (fallbackvi)
					XFree(fallbackvi);
				XFree(fbconfigs);
				return OpStatus::OK;
			}
			else if (!fallbackvi)
			{
				/* However, if there are no such visuals, we'll have
				 * to make do with another.
				 */
				fallbackfbconfig = fbconfigs[fbconfnum];
				fallbackvi = vi;
				vi = NULL;
			}
			if (vi)
				XFree(vi);
		}
	}
	XFree(fbconfigs);
	if (!fallbackvi)
		return OpStatus::ERR;
	*ret_fbconfig = fallbackfbconfig;
	*ret_vi = fallbackvi;
	return OpStatus::OK;
}

OP_STATUS X11_GlDevice::CreateDefaultGLXObjects(GLX::FBConfig fbconfig, XVisualInfo * vi)
{
	X11Types::Colormap main_colormap = XCreateColormap(g_x11->GetDisplay(), RootWindow(g_x11->GetDisplay(), vi->screen), vi->visual, AllocNone);
	XSetWindowAttributes attr;
	attr.colormap = main_colormap;
	attr.border_pixel = 0;
	attr.event_mask = ExposureMask | StructureNotifyMask;

	m_globalGLContext.window = XCreateWindow(g_x11->GetDisplay(),
			RootWindow(g_x11->GetDisplay(), vi->screen),
			0, 0, 16, 16, 0, vi->depth, InputOutput, vi->visual,
			CWBorderPixel | CWColormap | CWEventMask, &attr);

	/* This will make the colours displayed by
	 * m_globalGLContext.window undefined.  But since that is a dummy
	 * window that is never displayed anyway, it shouldn't matter.
	 */
	XFreeColormap(g_x11->GetDisplay(), main_colormap);

	if (!m_globalGLContext.window)
		return OpStatus::ERR;

	int glx_major;
	int glx_minor;
	if (!GLX::QueryVersion(g_x11->GetDisplay(), &glx_major, &glx_minor))
	{
		/* Default to something "old", at least older than 1.3 */
		glx_major = 1;
		glx_minor = 1;
	}
	/* GLX 2 is not yet defined, and may be completely incompatible
	 * with this code.  It's safer to fall back to software in that
	 * case.
	 */
	if (glx_major != 1)
		return OpStatus::ERR;
	/* GLXWindows were introduced in GLX 1.3 */
	if (glx_minor < 3)
		s_use_glxwindow = false;

	if (s_use_glxwindow)
		m_globalGLContext.glxwindow = GLX::CreateWindow(g_x11->GetDisplay(), fbconfig, m_globalGLContext.window, 0);
	else
		m_globalGLContext.glxwindow = None;

#ifdef VEGA_GL_DEBUG_CONTEXT
	const int context_attribs [] =
		{
			GLX::CONTEXT_FLAGS_ARB, GLX::CONTEXT_DEBUG_BIT_ARB,
			None
		};
	if (GLX::HasExtensionARBCreateContext(g_x11->GetDisplay(), vi->screen))
		m_globalGLContext.renderContext = GLX::CreateContextAttribsARB(g_x11->GetDisplay(), fbconfig, NULL, True, context_attribs);
#endif // VEGA_GL_DEBUG_CONTEXT
	if (!m_globalGLContext.renderContext)
		m_globalGLContext.renderContext = GLX::CreateNewContext(g_x11->GetDisplay(), fbconfig, GLX::RGBA_TYPE, NULL, True);
	if (!m_globalGLContext.renderContext)
		return OpStatus::ERR;

	// Both GLXWindows and Windows are really XIDs...
	GLX::Window use_window = m_globalGLContext.glxwindow;
	if (!use_window)
		use_window = GLX::Window(m_globalGLContext.window);
	if (!GLX::MakeContextCurrent(g_x11->GetDisplay(), use_window, use_window, m_globalGLContext.renderContext))
		return OpStatus::ERR;

	return OpStatus::OK;
}

/* Set up the visuals to be used for rendering with OpenGL.
 *
 * I'm assuming that this method is called after g_x11 is initialized
 * but before any windows that will be rendered to with OpenGL has
 * been created.
 *
 * FIXME: This only works with the default screen, any OpenGL
 * rendering to other screens will fail.  This is dictated by the
 * design of OpenGL rendering in core, which assumes there is only a
 * single context.
 */
OP_STATUS X11_GlDevice::SetGlobalGLVisuals(GLX::FBConfig fbconfig, XVisualInfo * vi)
{
	int defaultscreen = g_x11->GetDefaultScreen();
	if (g_x11->GetGLARGBVisual(defaultscreen).visual == 0)
		if (!g_x11->SetGLVisual(defaultscreen, true, vi, fbconfig))
			return OpStatus::ERR;
	OP_ASSERT(g_x11->GetGLARGBVisual(defaultscreen).fbconfig == fbconfig);
	if (g_x11->GetGLVisual(defaultscreen).visual == 0)
		if (!g_x11->SetGLVisual(defaultscreen, false, vi, fbconfig))
			return OpStatus::ERR;
	OP_ASSERT(g_x11->GetGLVisual(defaultscreen).fbconfig == fbconfig);
	return OpStatus::OK;
}

OP_STATUS X11_GlDevice::InitGL()
{
	if (m_gl_ok == MAYBE)
	{
		OP_ASSERT(g_x11->GetDisplay());
		if (!g_x11->GetDisplay())
			return OpStatus::ERR;

		m_gl_ok = NO;

		{ // local scope for OP_PROFILE_METHOD
			OP_PROFILE_METHOD("OpenGL safety check");
			/* This safety check costs about 0.34 seconds on my test
			 * system: Pentium D 3GHz, Radeon HD3450, fglrx driver, Ubuntu
			 * 11.04.
			 *
			 * On the other hand, it takes about 0.02 seconds on my Core
			 * i7 2.8GHz, Radeon HD3450, Mesa 7.10.3 r600g, Debian
			 * unstable.
			 *
			 * Timings seems to be similar to that reported by 'time
			 * glxinfo > /dev/null'
			 */
			if (!IsOpenGLSafe())
				return OpStatus::ERR;
		}

		RETURN_IF_ERROR(LoadGLLibrary());

		GLX::FBConfig fbconfig = 0;
		XVisualInfo * vi = 0;
		RETURN_IF_ERROR(ChooseFBConfig(&fbconfig, &vi));
		OP_STATUS err = CreateDefaultGLXObjects(fbconfig, vi);
		if (OpStatus::IsSuccess(err))
			err = SetGlobalGLVisuals(fbconfig, vi);
		if (OpStatus::IsSuccess(err))
			m_gl_ok = YES;

		if (vi)
			XFree(vi);

		if (m_gl_ok == NO)
		{
			if (m_globalGLContext.renderContext)
			{
				GLX::DestroyContext(g_x11->GetDisplay(), m_globalGLContext.renderContext);
				m_globalGLContext.renderContext = 0;
			}
			if (m_globalGLContext.glxwindow)
			{
				GLX::DestroyWindow(g_x11->GetDisplay(), m_globalGLContext.glxwindow);
				m_globalGLContext.glxwindow = None;
			}
			if (m_globalGLContext.window)
			{
				XDestroyWindow(g_x11->GetDisplay(), m_globalGLContext.window);
				m_globalGLContext.window = None;
			}
		}

		if (m_gl_ok == YES)
			FillGpuInfo(this);
	}

	return m_gl_ok == YES ? OpStatus::OK : OpStatus::ERR;
}

void X11_GlDevice::ShutdownGL()
{
	if (m_gl_ok == YES)
	{
		if (m_altctx)
		{
			GLX::DestroyContext(g_x11->GetDisplay(), m_altctx);
			m_altctx = NULL;
		}
		GLX::DestroyContext(g_x11->GetDisplay(), m_globalGLContext.renderContext);
		if (m_globalGLContext.glxwindow)
			GLX::DestroyWindow(g_x11->GetDisplay(), m_globalGLContext.glxwindow);
		XDestroyWindow(g_x11->GetDisplay(), m_globalGLContext.window);

		DeviceDeleted(this);
	}
}


OP_STATUS X11_GlDevice::Construct(VEGA_3D_BLOCKLIST_ENTRY_DECL)
{
	if (g_desktop_bootstrap->GetHWAccelerationDisabled())
	{
		g_vega_backends_module.SetCreationStatus(UNI_L("Disabled by command-line argument"));
		return OpStatus::ERR;
	}
	if (blocklist_entry)
	{
		if (blocklist_entry->string_entries.Contains(UNI_L("quirks.noglxwindow")))
			s_use_glxwindow = false;
		if (blocklist_entry->string_entries.Contains(UNI_L("quirks.nvidia270resetcontext")))
			s_quirk_nvidia270resetcontext = true;
	}
	RETURN_IF_ERROR(InitGL());
	return VEGAGlDevice::Construct(VEGA_3D_BLOCKLIST_ENTRY_PASS);
}

OP_STATUS X11_GlDevice::GenerateSpecificBackendInfo(OperaGPU* page, VEGABlocklistFile* blocklist, VEGABlocklistDevice::DataProvider* provider)
{
	RETURN_IF_ERROR(InitGL());
	return VEGAGlDevice::GenerateSpecificBackendInfo(page, blocklist, provider);
}

class X11GlWindow : public VEGAGlWindow,
					public X11Backbuffer,
					public X11VegaWindowStateListener,
					public X11ResourceListener,
					public X11LowLatencyCallback
{
public:
	X11GlWindow(VEGAWindow* w) :
		window(w),
		glx_window(None),
		texture(NULL),
		glContext(NULL),
		fbo(NULL),
		m_shader(NULL),
		m_last_swap(0),
		m_present_fence(NULL),
		m_frontbuffer_invalid(true)
	{
	}
	~X11GlWindow()
	{
		X11OpMessageLoop::CancelLowLatencyCallbacks(this);
		if (window)
		{
			static_cast<X11VegaWindow*>(window)->RemoveStateListener(this);
			static_cast<X11VegaWindow*>(window)->RemoveX11ResourceListener(this);
			static_cast<X11VegaWindow*>(window)->OnGlWindowDead(this);
			static_cast<X11VegaWindow*>(window)->setBackbuffer(NULL);
		}
		VEGARefCount::DecRef(m_shader);
		VEGARefCount::DecRef(fbo);
		VEGARefCount::DecRef(texture);
		if (current == this)
		{
			// Both GLXWindows and Windows are really XIDs...
			GLX::Window use_window = glContext->glxwindow;
			if (!use_window)
				use_window = glContext->window;
			GLX::MakeContextCurrent(g_x11->GetDisplay(), use_window, use_window, glContext->renderContext);
			current = NULL;
		}
		if (glx_window)
			GLX::DestroyWindow(g_x11->GetDisplay(), glx_window);
	}

	OP_STATUS Construct(X11_GlDevice::GLRenderContext* global_glcontext)
	{
		RETURN_IF_ERROR(static_cast<X11VegaWindow*>(window)->AddStateListener(this));
		RETURN_IF_ERROR(static_cast<X11VegaWindow*>(window)->AddX11ResourceListener(this));
		static_cast<X11VegaWindow*>(window)->setBackbuffer(this);
		glContext = global_glcontext;
		RETURN_IF_ERROR(g_opera->libvega_module.vega3dDevice->createShaderProgram(&m_shader, VEGA3dShaderProgram::SHADER_VECTOR2D, VEGA3dShaderProgram::WRAP_CLAMP_CLAMP));
		if (texture)
		{
			/* This texture will only be used for plain pixel copies.
			 * FILTER_NEAREST will protect against small rounding
			 * errors.
			 */
			texture->setFilterMode(VEGA3dTexture::FILTER_NEAREST, VEGA3dTexture::FILTER_NEAREST);
		}
		return resizeBackbuffer(window->getWidth(), window->getHeight());
	}

	void OnBeforeNativeX11WindowDestroyed(X11Types::Window win)
	{
		if (glx_window)
		{
			GLX::DestroyWindow(g_x11->GetDisplay(), glx_window);
			glx_window = None;
		}
	}

	void OnVEGAWindowDead(VEGAWindow * w)
	{
		if (window == w)
			window = NULL;
	}

	void Erase(const MDE_RECT &rect)
	{
		if (texture && fbo)
		{
			if (current != this)
			{
				GLX::MakeContextCurrent(g_x11->GetDisplay(), getNativeGLXHandle(), getNativeGLXHandle(), glContext->renderContext);
				current = this;
			}
			VEGA3dDevice* device = g_opera->libvega_module.vega3dDevice;
			device->setRenderTarget(fbo);

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

	}

	UINT32 GetWidth()
	{
		if (texture)
			return texture->getWidth();
		return 0;
	}

	UINT32 GetHeight()
	{
		if (texture)
			return texture->getHeight();
		return 0;
	}

	GLX::Window getNativeGLXHandle() const
	{
		if (!glx_window && !X11_GlDevice::s_use_glxwindow)
		{
			// Both GLXWindows and Windows are really XIDs...
			return GLX::Window(window->getNativeHandle());
		}
		else if (!glx_window)
		{
			// FIXME: What to do with errors?
			X11VegaWindow * x11vw = static_cast<X11VegaWindow*>(window);
			OP_ASSERT(x11vw->isUsingOpenGLBackend());
			X11Types::Visual * visual = x11vw->getVisual();
			X11Types::VisualID visid = XVisualIDFromVisual(visual);
			int screen = x11vw->getScreen();
			OP_ASSERT(screen >= 0);
			OP_ASSERT(screen < g_x11->GetScreenCount());
			X11Visual * screendata = &g_x11->GetGLVisual(screen);
			if (screendata->visual == 0 || screendata->id != visid)
				screendata = &g_x11->GetGLARGBVisual(screen);
			OP_ASSERT(screendata->visual != 0 && screendata->id == visid);
			GLX::FBConfig fbconfig = screendata->fbconfig;
			glx_window = GLX::CreateWindow(g_x11->GetDisplay(), fbconfig, X11Types::Window(window->getNativeHandle()), 0);
		};
		return glx_window;
	}

	virtual VEGA3dFramebufferObject* getWindowFBO()
	{
		return fbo;
	}

	virtual OP_STATUS resizeBackbuffer(unsigned int w, unsigned int h)
	{
		VEGA3dDevice* device = g_opera->libvega_module.vega3dDevice;
		if (texture && texture->getWidth() == w && texture->getHeight() == h)
			return OpStatus::OK;
		if (!w || !h)
			return OpStatus::OK;
		if (!fbo)
			RETURN_IF_ERROR(device->createFramebuffer(&fbo));
		VEGA3dTexture* newtex;
		RETURN_IF_ERROR(device->createTexture(&newtex, w, h, VEGA3dTexture::FORMAT_RGBA8888));
		/* This texture will only be used for plain pixel copies.
		 * FILTER_NEAREST will protect against small rounding errors.
		 */
		newtex->setFilterMode(VEGA3dTexture::FILTER_NEAREST, VEGA3dTexture::FILTER_NEAREST);
		// Make sure the window is not active render target when deleting the texture
		if (g_opera->libvega_module.vega3dDevice->getRenderTarget() == this)
			g_opera->libvega_module.vega3dDevice->setRenderTarget(NULL);
		fbo->attachColor(newtex);
		VEGARefCount::DecRef(texture);
		texture = newtex;
		if (m_present_fence)
		{
			/* Finish the currently outstanding presentation, so we
			 * can get started on the next one.  At this point the
			 * texture we use as a backbuffer is probably also
			 * invalid, so the frontbuffer will still be invalid after
			 * present_really() returns.
			 */
			m_frontbuffer_invalid = true;
			present_really();
		}
		m_frontbuffer_invalid = true;
		return OpStatus::OK;
	}

	virtual void present(const OpRect*, unsigned int)
	{
		OP_ASSERT(m_present_fence == NULL);
		if (m_present_fence)
		{
			// This should not happen, but avoid a resource leak when
			// it does.
			glDeleteSync(m_present_fence);
			m_present_fence = NULL;
		}
		if (m_frontbuffer_invalid)
		{
			// No waiting in this case
			present_really();
			return;
		}
		if (glFenceSync)
			m_present_fence = glFenceSync(non_gles_GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		glFlush();
		double target = m_last_swap + 15;
		if (target <= g_op_time_info->GetRuntimeMS())
		{
			// It is already time to swap, so go ahead.
			present_really();
			return;
		}
		if (OpStatus::IsError(X11OpMessageLoop::PostLowLatencyCallbackAbsoluteTime(this, 0, target)))
			present_really();
	}

	void copy_backbuffer_to_windowbuffer()
	{
		if (texture)
		{
			if (current != this)
			{
				GLX::MakeContextCurrent(g_x11->GetDisplay(), getNativeGLXHandle(), getNativeGLXHandle(), glContext->renderContext);
				current = this;
			}
			VEGA3dDevice* device = g_opera->libvega_module.vega3dDevice;

			VEGA3dDevice::Vega2dVertex verts[] = {
				{0, 0, 0, 1.f, ~0u},
				{float(texture->getWidth()), 0, 1.f, 1.f, ~0u},
				{0, float(texture->getHeight()), 0, 0, ~0u},
				{float(texture->getWidth()), float(texture->getHeight()), 1.f, 0, ~0u}
			};

			VEGA3dBuffer* b = device->getTempBuffer(sizeof(verts));
			if (!b)
				return;
			VEGA3dVertexLayout* layout = 0;
			RETURN_VOID_IF_ERROR(device->createVega2dVertexLayout(&layout, VEGA3dShaderProgram::SHADER_VECTOR2D));

			device->setRenderTarget(NULL);
			device->setViewport(0, 0, texture->getWidth(), texture->getHeight(), 0, 1.f);
			device->setShaderProgram(m_shader);
			/* m_shader->setOrthogonalProjection(void) does not work
			 * for rendertarget NULL, as it does not know the
			 * dimensions of that rendertarget.  Thus the version of
			 * the method taking explicit dimensions must be used
			 * instead.  And we know the back buffer ('texture') is
			 * the same dimensions as the window.
			 */
			m_shader->setOrthogonalProjection(texture->getWidth(), texture->getHeight());

			VEGA3dRenderState state;
			state.enableBlend(false);
			device->setRenderState(&state);

			unsigned int bufidx = 0;
			b->writeAnywhere(4, sizeof(VEGA3dDevice::Vega2dVertex), verts, bufidx);

			device->setTexture(0, texture);

			device->drawPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE_STRIP, layout, bufidx, 4);

			device->setTexture(0, NULL);
			VEGARefCount::DecRef(layout);
		}
	}

	void present_really()
	{
		OP_ASSERT(glFenceSync == NULL || m_present_fence != NULL || m_frontbuffer_invalid);
		if (m_frontbuffer_invalid)
		{
			// Force painting, block if necessary.
			if (m_present_fence)
			{
				glDeleteSync(m_present_fence);
				m_present_fence = NULL;
			}
			m_frontbuffer_invalid = false;
		}
		else if (m_present_fence)
		{
			GLsizei response_length = 0;
			GLint value = non_gles_GL_SIGNALED;
			glGetSynciv(m_present_fence, non_gles_GL_SYNC_STATUS, 1, &response_length, &value);
			if (value != non_gles_GL_SIGNALED)
			{
				if (OpStatus::IsSuccess(X11OpMessageLoop::PostLowLatencyCallbackDelayed(this, 0, 3)))
					return;
			}
			glDeleteSync(m_present_fence);
			m_present_fence = NULL;
		}
		m_last_swap = g_op_time_info->GetRuntimeMS();
		copy_backbuffer_to_windowbuffer();

		GLX::SwapBuffers(g_x11->GetDisplay(), getNativeGLXHandle());

		if (window)
			static_cast<X11VegaWindow*>(window)->OnPresentComplete();
	}

	void LowLatencyCallback(UINTPTR data)
	{
		present_really();
	}

	virtual void repaint()
	{
		if (current != this)
		{
			GLX::MakeContextCurrent(g_x11->GetDisplay(), getNativeGLXHandle(), getNativeGLXHandle(), glContext->renderContext);
			current = this;
		}
		GLX::SwapBuffers(g_x11->GetDisplay(), getNativeGLXHandle());
	}

	virtual VEGA3dTexture* getWindowTexture(){return texture;}
	virtual unsigned int getWidth(){return window->getWidth();}
	virtual unsigned int getHeight(){return window->getHeight();}

	/** First, make 'ctx' current.  Then make this window's context
	 * current.  In both cases using this window as read and write
	 * buffers.
	 *
	 * This method is used by X11_GlDevice::FlipFlopContexts(), see
	 * that method for details.
	 */
	void RebindWindowAfterTemporarySwitchToContext(GLX::Context ctx)
	{
		GLX::Window win = getNativeGLXHandle();
		GLX::MakeContextCurrent(g_x11->GetDisplay(), win, win, ctx);
		GLX::MakeContextCurrent(g_x11->GetDisplay(), win, win, glContext->renderContext);
	}

	/** The window that is currently bound to the active GLX context.
	 * Returns NULL if there is no such window or it is unknown which
	 * window is bound.
	 *
	 * That a window is bound to the active GLX context means that it
	 * is the default framebuffer.  It does not mean that it is the
	 * currently active framebuffer.
	 */
	static X11GlWindow * GetCurrentlyActiveWindow() { return current; };

private:
	VEGAWindow* window;
	mutable GLX::Window glx_window;
	VEGA3dTexture* texture;
	X11_GlDevice::GLRenderContext* glContext;
	static X11GlWindow* current;
	VEGA3dFramebufferObject* fbo;
	VEGA3dShaderProgram* m_shader;
	double m_last_swap;
	GLsync m_present_fence;
	bool m_frontbuffer_invalid;
};
X11GlWindow* X11GlWindow::current = NULL;

OP_STATUS X11_GlDevice::createWindow(VEGA3dWindow** win, VEGAWindow* nativeWin)
{
	X11GlWindow* glw = OP_NEW(X11GlWindow, (nativeWin));
	if (!glw)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS err = glw->Construct(&m_globalGLContext);
	if (OpStatus::IsError(err))
	{
		OP_DELETE(glw);
		return err;
	}
	*win = glw;
	return OpStatus::OK;
}

OP_STATUS X11_GlDevice::setRenderTarget(VEGA3dRenderTarget* rtex, bool changeViewport)
{
	if (s_quirk_nvidia270resetcontext && rtex && !renderTarget)
		FlipFlopContexts();
	return VEGAGlDevice::setRenderTarget(rtex, changeViewport);
}

void* X11_GlDevice::getGLFunction(const char* funcName)
{
	return (void*)GLX::GetProcAddress(funcName);
}

GLX::Context X11_GlDevice::GetAltContext()
{
	if (m_altctx)
		return m_altctx;

	int defaultscreen = g_x11->GetDefaultScreen();
	X11Visual &visinfo = g_x11->GetGLARGBVisual(defaultscreen);
	m_altctx = GLX::CreateNewContext(g_x11->GetDisplay(), visinfo.fbconfig, GLX::RGBA_TYPE, NULL, True);
	return m_altctx;
}

void X11_GlDevice::FlipFlopContexts()
{
	X11GlWindow * current_window = X11GlWindow::GetCurrentlyActiveWindow();
	if (!current_window)
		return;
	GLX::Context ctx = GetAltContext();
	if (!ctx)
		return;
	current_window->RebindWindowAfterTemporarySwitchToContext(ctx);
}

#endif // VEGA_BACKEND_OPENGL
