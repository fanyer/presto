#ifndef MAC_GL_DEVICE_H
#define MAC_GL_DEVICE_H

#ifdef VEGA_SUPPORT

#include "modules/libvega/vegaconfig.h"

#if defined(VEGA_3DDEVICE) || defined(CANVAS3D_SUPPORT)

#include "platforms/vega_backends/opengl/vegagldevice.h"
#include "platforms/vega_backends/opengl/vegaglwindow.h"
#include "platforms/vega_backends/opengl/vegaglfbo.h"
#include "platforms/vega_backends/opengl/vegagltexture.h"

#include "platforms/mac/util/OpenGLContextWrapper.h"

#include "modules/libvega/vegawindow.h"
#include "modules/pi/OpWindow.h"
#include "modules/libgogi/mde.h"

// Use a simplistic resize corner (just drawing three lines),
// or use a proper texture (with transparency) to get a slight highlight
// around the lines?
// There are problems with flickering if we do not use the simple one.
#define SIMPLE_RESIZE_CORNER 1

class MacOpGlDevice : public VEGAGlDevice
{
public:
	MacOpGlDevice() {}
	virtual ~MacOpGlDevice();
#ifdef VEGA_BACKENDS_USE_BLOCKLIST
	virtual VEGABlocklistDevice::BlocklistType GetBlocklistType() const;
	virtual VEGABlocklistDevice::DataProvider* CreateDataProvider();
#endif
	virtual OP_STATUS Init();
#ifdef VEGA_OPPAINTER_SUPPORT
	/** Create a window which can be used as a target for 3d rendering. */
	virtual OP_STATUS createWindow(VEGA3dWindow** win, VEGAWindow* nativeWin);
#endif // VEGA_OPPAINTER_SUPPORT
	
	virtual void* getGLFunction(const char* funcName);
	virtual unsigned int getDeviceType() const { return 0; }
	
	static void* loadOpenGLFunction(const char* funcName);
};

class MacGlWindow : public VEGAGlWindow
{
public:
	MacGlWindow(VEGAWindow* w);
	~MacGlWindow();

	OpWindow::Style GetStyle() const;

	/** @returns the width of the render target. */
	virtual unsigned int getWidth() { return window->getWidth(); }
	/** @returns the width of the render target. */
	virtual unsigned int getHeight() { return window->getHeight(); }
	/** Make the rendered content visible. Swap for double buffering. */
	virtual void present(const OpRect*, unsigned int);
	/** Erases the window, i.e draws draws transparent black (#00000000). */
	void Erase(const MDE_RECT& rect, bool background = false); 

	/**
	 * Resize the backbuffer.
	 * @param [in] w New width of the backbuffer.
	 * @param [in] h New height of the backbuffer.
	 **/
	virtual OP_STATUS resizeBackbuffer(unsigned int w, unsigned int h);

	virtual VEGA3dFramebufferObject* getWindowFBO() { return fbo; }

	/**
	 * Constructs VEGA3dDevice implementation for Mac OS X.
	 * @return See #resizeBackbuffer return values.
	 */
	OP_STATUS Construct();

	void MoveToBackground(const MDE_RECT& rect); 
	void present(const OpRect*, unsigned int, bool background);

private:
	bool draw_bg;
	VEGAWindow *window;
	VEGA3dTexture *tex;                             ///< Window's texture. This is what we see on the screen.
	VEGA3dTexture *bg_tex;								///< Window's texture. This is what we see on the screen.
	VEGA3dFramebufferObject *fbo;					///< Window's framebuffer object.
	VEGA3dFramebufferObject *bg_fbo;					///< Window's framebuffer object.
	VEGA3dTexture *resize_tex;
	VEGA3dFramebufferObject *resize_fbo;
	VEGA3dShaderProgram *m_shader;					///< Generic shader for drawing quads.
	float width, height;                            ///< Back buffer width and height. Updated from resizeBackbuffer.
};

#endif // VEGA_3DDEVICE || CANVAS3D_SUPPORT
#endif // VEGA_SUPPORT
#endif // MAC_GL_DEVICE_H
