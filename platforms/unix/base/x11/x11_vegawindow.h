/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2003-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#ifndef __X11_VEGAWINDOW_H__
#define __X11_VEGAWINDOW_H__

#ifdef VEGA_OPPAINTER_SUPPORT

#include "modules/libvega/vegawindow.h"
#include "platforms/unix/base/x11/x11_mdebuffer.h"
#include "platforms/unix/base/x11/x11_widget.h"

#include "platforms/vega_backends/opengl/vegaglwindow.h"

class X11MdeScreen;

class X11VegaWindowStateListener
{
public:
	virtual ~X11VegaWindowStateListener() {}

	/** Called when the vega window is being destroyed.  The object
	 * should not access 'w' any more.  The vega window may already be
	 * partially destroyed when this method is called, so it is not
	 * safe to access 'w' inside this method either.
	 */
	virtual void OnVEGAWindowDead(VEGAWindow * w) = 0;
};

class X11VegaWindow : public VEGAWindow
{
public:
	X11VegaWindow(unsigned int w, unsigned int h, X11Widget * win) :
		m_mde_buffer(0), 
		m_width(w), 
		m_height(h), 
		m_window(win), 
		m_glWindow(NULL), 
		m_softwareBackend(false),
		m_is_present_in_progress(false)
	{
		op_memset(&pixelstore, 0, sizeof(VEGAPixelStore));
	}

	virtual ~X11VegaWindow()
	{
		for (UINT32 i = m_state_listeners.GetCount(); i > 0; i--)
			m_state_listeners.Get(i-1)->OnVEGAWindowDead(this);
		if (m_window)
			m_window->OnVEGAWindowDead(this);
	}

	/** Register a listener to be notified for state changes to this
	 * X11VegaWindow.
	 *
	 * @param listener The object that will be notified about state
	 * changes to this object.
	 */
	OP_STATUS AddStateListener(X11VegaWindowStateListener * listener)
	{
		return m_state_listeners.Add(listener);
	}

	/** Remove 'listener' from the list of objects to be notified
	 * about state changes to this X11VegaWindow.
	 *
	 * Once this method returns, 'listener' will not be accessed by
	 * this object again.
	 *
	 * @param listener The object that no longer wishes to be notified
	 * about state changes to this object.
	 */
	void RemoveStateListener(X11VegaWindowStateListener * listener)
	{
		UINT32 idx = m_state_listeners.GetCount();
		while (idx > 0)
		{
			idx -= 1;

			if (m_state_listeners.Get(idx) == listener)
				m_state_listeners.Remove(idx);
		}
	}

	/** Register a listener to be notified about things happening to
	 * X11 resources controlled by this object.
	 *
	 * This object calls the callback OnBeforeNativeX11WindowDestroyed()
	 * when the native X11 window represented by this object is
	 * destroyed.
	 *
	 * @param listener The object that wishes to be notified about X11
	 * resource events.
	 */
	OP_STATUS AddX11ResourceListener(X11ResourceListener * listener)
	{
		return m_x11_resource_listeners.Add(listener);
	}

	/** Remove 'listener' from the list of objects to be notified
	 * about things happening to X11 resources controlled by this
	 * object.
	 *
	 * Once this method returns, 'listener' will not be accessed by
	 * this object again.
	 *
	 * @param listener The object that no longer wishes to be notified
	 * about X11 resource events.
	 */
	void RemoveX11ResourceListener(X11ResourceListener * listener)
	{
		UINT32 idx = m_x11_resource_listeners.GetCount();
		while (idx > 0)
		{
			idx -= 1;

			if (m_x11_resource_listeners.Get(idx) == listener)
				m_x11_resource_listeners.Remove(idx);
		}
	}

	void OnXWindowDead(X11Widget * w)
	{
		OP_ASSERT(m_window == NULL || m_window == w);
		m_window = NULL;
	}

	void OnGlWindowDead(VEGA3dWindow * w)
	{
		OP_ASSERT(m_glWindow == NULL || m_glWindow == w);
		m_glWindow = NULL;
	}

	void OnBeforeNativeHandleDestroyed(X11Types::Window win)
	{
		for (UINT32 idx = m_x11_resource_listeners.GetCount(); idx > 0; idx --)
		{
			m_x11_resource_listeners.Get(idx-1)->OnBeforeNativeX11WindowDestroyed(win);
		}
	}

	unsigned int getWidth(){return m_width;}
	unsigned int getHeight(){return m_height;}
	VEGAPixelStore* getPixelStore(){return &pixelstore;}
	/* getNativeHandle() overrides a non-const method, so it can't be const. */
	void* getNativeHandle()
	{
		if (m_window)
			return reinterpret_cast<void*>(m_window->GetWindowHandle());
		return NULL;
	}

	OP_STATUS resize(unsigned int w, unsigned int h)
	{
		if (w == m_width && h == m_height)
			return OpStatus::OK;
		m_width = w;
		m_height = h;
		if (m_softwareBackend)
			return initSoftwareBackend();
		if (m_glWindow)
			return m_glWindow->resizeBackbuffer(w, h);
		return OpStatus::OK;
	}

	void present(const OpRect* update_rects, unsigned int num_rects)
	{
		if (m_glWindow)
		{
			m_is_present_in_progress = true;
			m_glWindow->present(update_rects, num_rects);
			return;
		}
		for (unsigned i = 0; i < num_rects; i++)
		{
			m_mde_buffer->UpdateShape(update_rects[i]);
			m_mde_buffer->Repaint(update_rects[i]);
		}
		OnPresentComplete();
	}

	/** Returns true if there is a present in progress, and 'listener'
	 * will be notified (through OnPresentComplete()) when the present
	 * has completed.
	 */
	bool IsPresentInProgressFor(X11MdeScreen * listener)
	{
		if (!m_glWindow)
			return false;
		if (!m_is_present_in_progress)
			return false;

		X11MdeScreen * screen = getMdeScreen();
		return screen == listener;
	}

	void OnPresentComplete();

	OP_STATUS initSoftwareBackend()
	{
		/* initSoftwareBackend() is called from resize(), so I can't
		 * OP_ASSERT(!isUsingSoftwareBackend()).
		 */
		OP_ASSERT(!isUsingOpenGLBackend());
		OP_ASSERT(m_window);
		OP_ASSERT(!m_window || !m_window->IsUsingOpenGL());
		if (m_mde_buffer->GetBitsPerPixel() != 16 && m_mde_buffer->GetBitsPerPixel() != 32)
			return OpStatus::ERR;
		m_softwareBackend = true;
		RETURN_IF_ERROR(m_mde_buffer->AllocateBuffer(m_width, m_height));
		pixelstore.width  = m_mde_buffer->GetWidth();
		pixelstore.height = m_mde_buffer->GetHeight();
		pixelstore.stride = m_mde_buffer->GetBytesPerLine();
		if (m_mde_buffer->GetBitsPerPixel() == 16)
		{
			/* WARNING: As vegapixelstore.h says: the component
			 * ordering (RGB vs BGR) for the VPSF_???565 formats are
			 * inverse compared to the other formats.  That's why this
			 * test seems to be backwards.
			 */
			pixelstore.format = m_mde_buffer->HasBGR565Format() ? VPSF_RGB565 : VPSF_BGR565;
		}
		else
			pixelstore.format = m_mde_buffer->HasBGRA8888Format() ? VPSF_BGRA8888 : VPSF_ARGB8888;

		pixelstore.buffer = m_mde_buffer->GetImageBuffer();

		return OpStatus::OK;
	}

	OP_STATUS initHardwareBackend(VEGA3dWindow* glw)
	{
		OP_ASSERT(!isUsingSoftwareBackend());
		OP_ASSERT(!isUsingOpenGLBackend());
		OP_ASSERT(m_window);
		OP_ASSERT(!m_window || m_window->IsUsingOpenGL());
		m_glWindow = glw;
		return OpStatus::OK;
	}

	bool isUsingSoftwareBackend()
	{
		return m_softwareBackend;
	}

	bool isUsingOpenGLBackend()
	{
		return m_glWindow != NULL;
	}

	void setMdeBuffer(X11MdeBuffer* mde_buffer) { m_mde_buffer = mde_buffer; }

	/** Set the back buffer that backs this window.  Can be called
	 * multiple times to switch between back buffers.  Can be called
	 * with NULL to switch to having no back buffer.
	 */
	void setBackbuffer(X11Backbuffer* backbuffer)
	{
		m_mde_buffer->SetExternalBackbuffer(backbuffer);
	}

	X11MdeBuffer* getMdeBuffer() { return m_mde_buffer; }

	/** Returns the visual used to create the X11 window.
	 */
	X11Types::Visual * getVisual() const
	{
		if (m_window)
			return m_window->GetVisual();
		return NULL;
	}
	/** Returns the screen the X11 window was created on.
	 */
	int getScreen() const
	{
		if (m_window)
			return m_window->GetScreen();
		return -1;
	}
	/** Return the depth of the X11 window.
	 */
	int getDepth() const
	{
		if (m_window)
			return m_window->GetDepth();
		return -1;
	}

private:
	OpVector<X11VegaWindowStateListener> m_state_listeners;
	OpVector<X11ResourceListener> m_x11_resource_listeners;
	VEGAPixelStore pixelstore;
	X11MdeBuffer* m_mde_buffer;
	unsigned int m_width;
	unsigned int m_height;
	X11Widget * m_window;
	VEGA3dWindow* m_glWindow;
	bool m_softwareBackend;
	bool m_is_present_in_progress;

	X11MdeScreen * getMdeScreen() const
	{
		X11Widget * widget = m_window;
		if (!widget)
			return NULL;
		X11MdeScreen * screen = widget->GetMdeScreen();
		while (widget && !screen)
		{
			widget = widget->Parent();
			screen = widget->GetMdeScreen();
		}
		return screen;
	}
};

#endif // VEGA_OPPAINTER_SUPPORT

#endif // __X11_VEGAWINDOW_H__
