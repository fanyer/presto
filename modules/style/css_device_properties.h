/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef CSS_DEVICE_PROPERTIES_H
#define CSS_DEVICE_PROPERTIES_H

#ifdef CSS_VIEWPORT_SUPPORT

/** A struct for storing the properties of the browser/device
	used for evaluating media queries and constraining viewport
	properties. */
struct CSS_DeviceProperties
{
	/** The width of the device in CSS pixels. */
	unsigned int device_width;

	/** The height of the device in CSS pixels. */
	unsigned int device_height;

	/** The width of the viewport in CSS pixels. */
	unsigned int viewport_width;

	/** The height of the viewport in CSS pixels. */
	unsigned int viewport_height;

	/** The virtual desktop width, in CSS pixels, used as
		initial containing block width when laying out pages
		designed for a desktop browser window.*/
	unsigned int desktop_width;

	/** The initial font-size in pixels. */
	unsigned int font_size;
};

#endif // CSS_VIEWPORT_SUPPORT

#endif // CSS_DEVICE_PROPERTIES_H
