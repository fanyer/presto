/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2001 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/** @file opscreenpropertiescache.h
 *
 * Caching-class for screen properties
 * Caches properties for a window,
 * the cache is 'dirtyfied' by direct
 * call from 'Window' class.
 *
 * At intruction only used by VisualDevice
 *
 * The class is supposed to be as thin as possible,
 * and should require as little interaction as
 * possible
 *
 * @author Anders Oredsson
 *
 */
#ifndef OP_SCREENPROPERTIESCACHE_H
#define OP_SCREENPROPERTIESCACHE_H

#include "modules/pi/OpScreenInfo.h"
#include "modules/pi/OpWindow.h"

class OpScreenPropertiesCache{
private:
	OpScreenProperties  cachedProperties;
	OpWindow*			cachedForWindow;//if null, we are listening for the 'main screen'
	bool				dirty;
public:
	OpScreenPropertiesCache();	//Starts to listen for messages

	/**Get properties,
	 * if window is the same as usual, AND
	 * no point is given, and not 'marked as dirty', we simply
	 * return the cached values
	 *
	 * if window = null , we listen to and cache the 'main screen' properties
	 * if point != null , we cant use cached values, since window might span
	 * two screens, and we want our popup menu to show in the right screen!
	 */
	OpScreenProperties* getProperties(OpWindow* window, OpPoint* point = NULL);

	/**Get DPI property, same
	 * as get properties, exept that DPI is
	 * sometimes optimized to static global
	 *
	 * if window = null , we listen to and cache the 'main screen' properties
	 * if point != null , we cant use cached values, since window might span
	 * two screens, and we want our popup menu to show in the right screen!
	 */
	UINT32 getHorizontalDpi(OpWindow* window, OpPoint* point = NULL);

	/**Get DPI property, same
	 * as get properties, exept that DPI is
	 * sometimes optimized to static global
	 *
	 * if window = null , we listen to and cache the 'main screen' properties
	 * if point != null , we cant use cached values, since window might span
	 * two screens, and we want our popup menu to show in the right screen!
	 */
	UINT32 getVerticalDpi(OpWindow* window, OpPoint* point = NULL);

	/**Tell cache that the currently cachhed properties are dirt
	 * and need to be re-collected
	 */
	void	markPropertiesAsDirty();
};

#endif // OP_SCREENPROPERTIESCACHE_H
