/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef TVMANAGER_H
#define TVMANAGER_H

#ifdef HAS_ATVEF_SUPPORT

#include "modules/hardcore/base/op_types.h"
#include "modules/windowcommander/OpTv.h"
#include "modules/util/simset.h"

class URL;

/** The global TvManager object. */
#define g_tvManager g_opera->display_module.m_tvManager

class TvManager : public OpTv{

public:

	TvManager();

	virtual ~TvManager();

    /** SetDisplayRect is called by Opera when a TV window has moved
      * to a different position on the screen.
      * The coordinates are relative to the operawidgets upper left corner.
	  *
	  * @param url URL object corresponding to the window.
	  * @param tvRect Coordinates of the window.
      */
    void SetDisplayRect(const URL *url, const OpRect& tvRect);

    /** Signal from core when a TV window becomes available or unavailable.
	  *
	  * @param url URL object corresponding to the window.
	  * @param available Whether the window is available or not.
	  */
	void OnTvWindowAvailable(const URL *url, BOOL available);

#ifdef MEDIA_SUPPORT
	/** As above, but for <video>.
	  *
	  * @param handle Handle to the core media resource.
	  * @param tvRect Coordinates of the window.
	  * @param clipRect Coordinates of the clip rectangle.
	  */
	void SetDisplayRect(OpMediaHandle handle, const OpRect& tvRect, const OpRect& clipRect);

	/** Signal from core when a TV window becomes available or unavailable.
	  *
	  * @param handle Handle to the core media resource.
	  * @param available Whether the window is available or not.
	  */
	void OnTvWindowAvailable(OpMediaHandle handle, BOOL available);
#endif // MEDIA_SUPPORT

    /** Get the bleed through color.
      * This one is called every time the TV frame is about to
      * be rendered.
      */
    void GetChromakeyColor(UINT8* red, UINT8* green, UINT8* blue, UINT8* alpha);

	// from OpTv

    virtual void SetChromakeyColor(UINT8 red, UINT8 green, UINT8 blue, UINT8 alpha);

	virtual OP_STATUS RegisterTvListener(OpTvWindowListener* listener);

	virtual void UnregisterTvListener(OpTvWindowListener* listener);

  private:
	/** Helper function to find a OpTvWindowListener in the list of listeners.
	  *
	  * @param which The OpTvWindowListener to look for.
	  * @return The requested object, NULL if not found.
	  */
	class TvListenerContainer *GetContainerFor(const OpTvWindowListener *which);
	Head m_listeners;
	UINT8 chroma_red;
	UINT8 chroma_green;
	UINT8 chroma_blue;
	UINT8 chroma_alpha;
};

#endif //HAS_ATVEF_SUPPORT
#endif //TVMANAGER_H
