/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2003-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#ifndef __X11_MDESCREEN_H__
#define __X11_MDESCREEN_H__

#include "modules/libgogi/pi_impl/mde_generic_screen.h"
#include "modules/util/adt/oplisteners.h"
#include "x11_opmessageloop.h"

class X11VegaWindow;
class X11MdeBuffer;

class X11MdeScreen : public MDE_GenericScreen,
					 X11LowLatencyCallback
{
public:
	class Listener
	{
	public:
		virtual void OnDeleted(X11MdeScreen* mde_screen) = 0;
	};

public:

	X11MdeScreen(int bufferWidth,
				 int bufferHeight,
				 int bufferStride,
				 MDE_FORMAT bufferFormat,
				 void* bufferData);
	~X11MdeScreen();

	/**
	 * Add a listener to this object. A listener will only be added once
	 * no matter how many times the function is called.
	 *
	 * @param listener The listener
	 *
	 * @return OpStatus::OK on succees, otherwise OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS AddListener(Listener* listener) { return m_listeners.Add(listener); }

	/**
	 * Remove a listener from this object.
	 *
	 * @param listener The listener
	 *
	 * @return OpStatus::OK even if listener was not registered
	 */
	OP_STATUS RemoveListener(Listener* listener) { return m_listeners.Remove(listener); }

	void setVegaWindow(X11VegaWindow* win) { m_vega_window = win; }
	void ValidateMDEScreen();
	void LowLatencyCallback(UINTPTR data);
	void OnPresentComplete();
	OP_STATUS PostValidateMDEScreen();
	void setMdeBuffer(X11MdeBuffer* mde_buffer) { m_mde_buffer = mde_buffer; }

	virtual void OnBeforeRectPaint(const MDE_RECT &rect);
	virtual void OnValidate();
	virtual void OnInvalid() { PostValidateMDEScreen(); }
	virtual void ScrollPixels(const MDE_RECT &rect, int dx, int dy);
	virtual bool ScrollPixelsSupported();

private:
	bool m_mde_validate_screen_posted;
	bool m_in_validate;
	bool m_oninvalid_while_in_validate;
	double m_last_validate_end;
	X11VegaWindow* m_vega_window;
	X11MdeBuffer* m_mde_buffer;
	OpListeners<Listener> m_listeners;
};

#endif // __X11_MDESCREEN_H__
