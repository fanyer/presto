/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

/**
   @author mg@opera.com
   @author rikard@opera.com
*/

#ifndef OP_TV_H
#define OP_TV_H

#ifdef HAS_ATVEF_SUPPORT

#include "modules/pi/OpMediaPlayer.h"
#include "modules/hardcore/base/op_types.h"

/**
 * Listener API for TV window display.
 */
class OpTvWindowListener
{
  public:

	virtual ~OpTvWindowListener() {}

	/**
	 * A TV window has moved. If the window becomes hidden, the
	 * passed rectangle has a height and width of zero.
	 * @param url URL associated with the TV window.
	 * @param rect Coordinates of the TV window.
	 */
	virtual void SetDisplayRect(const uni_char *url, const OpRect& rect) = 0;

	/**
	 * A TV window has been brought into or out of display.
	 * @param url URL associated with the TV window.
	 * @param available TRUE if the window has been brought into display.
	 */
	virtual void OnTvWindowAvailable(const uni_char *url, BOOL available) = 0;

#ifdef MEDIA_SUPPORT
	/**
	 * As above, but for <video>.
	 * @param handle Handle to the core media resource.
	 * @param rect Coordinates of the TV window.
	 * @param clipRect Coordinates of the clip rectangle.
	 */
	virtual void SetDisplayRect(OpMediaHandle handle, const OpRect& rect, const OpRect &clipRect) = 0;

	/**
	 * As above, but for <video>.
	 * @param handle Handle to the core media resource.
	 * @param available TRUE if the window has been brought into display.
	 */
	virtual void OnTvWindowAvailable(OpMediaHandle handle, BOOL available) = 0;
#endif // MEDIA_SUPPORT
};



class OpTv
{
public:

	virtual ~OpTv() {}

	/** register a listener to changes in the tv window
	 @param listener the listener to set
	 @return OK on success, an error code otherwise
	*/

	virtual OP_STATUS RegisterTvListener(OpTvWindowListener* listener) = 0;

	/** unregister a tv listener
	 @param listener the listener to unregister
	*/

	virtual void UnregisterTvListener(OpTvWindowListener* listener) = 0;

	/** set the current chromakey color
	 @param red the red colour component 0-255
	 @param green the green colour component 0-255
	 @param blue the blue colour component 0-255
	 @param alpha the alpha colour component 0-255
	*/

	virtual void SetChromakeyColor(UINT8 red, UINT8 green, UINT8 blue, UINT8 alpha) = 0;
};

#endif
#endif /* OP_TV_H */
