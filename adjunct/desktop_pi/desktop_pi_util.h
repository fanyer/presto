/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef __DESKTOP_PI_UTIL_H__
# define __DESKTOP_PI_UTIL_H__

class OpBitmap;

namespace DesktopPiUtil
{
	/**
	 * Create and return an OpBitmap object representing the icon for
	 * file_path. The icon returned depends on the file type and, in some
	 * cases, on its content.
	 *
	 * @param bitmap (out) Set to the new OpBitmap created, or NULL if creation
	 * failed.
	 * @param file_path Path to the file to create the icon bitmap for.
	 * @return OK if successful, ERR_NO_MEMORY on OOM, ERR if any of the
	 * parameters were unacceptable for the implementation.
	 */
	OP_STATUS CreateIconBitmapForFile(OpBitmap **bitmap, const uni_char* file_path);

	/**
	 * Saves the image specified in the image folder (and transcodes it if necessary),
	 * and sets it as the Desktop background image.
	 * 
	 * @param url URI to the image that is to be set as background image.
	 * @return OK if successful, ERR if the image info couldn't retrieved from URL, or
	 * the image couldn't be set for some other reason.
	 */
	OP_STATUS SetDesktopBackgroundImage(URL& url);
}

#endif //__DESKTOP_PI_UTIL_H__
