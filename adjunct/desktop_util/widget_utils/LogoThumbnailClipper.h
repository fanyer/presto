/** -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
*
* Copyright (C) 2009 Opera Software AS.  All rights reserved.
*
* This file is part of the Opera web browser.
* It may not be distributed under any circumstances.
*
* @author Authors: Haraldur Karlsson (haralkar), Arjan van Leeuwen (arjanl), Petter Nilsen (pettern)
*
*/

#ifndef LOGO_THUMBNNAIL_CLIPPER_
#define LOGO_THUMBNNAIL_CLIPPER_

/** @brief Calculate clipping based on the position of a logo.
 */
class LogoThumbnailClipper
{
public:
	/** Set the original size of the thumbnail.
	 */
	void SetOriginalSize(int width, int height);

	/** Set the resulting size.
	 */
	void SetClippedSize(int width, int height);

	/** Set the top left coordinates of logo position
	 */
	void SetLogoStart(const OpPoint &topLeft);

	/** Calculate appropriate clipping.
	 */
	OpRect ClipRect();

private:
	OpRect m_originalRect;
	OpRect m_clippedRect;
	OpPoint m_logoStart;
};

/** @brief Calculate scaling based cropping on the sides, and scaling otherwise
 */

class SmartCropScaling
{
public:
	void SetOriginalSize(INT32 width, INT32 height);
	void GetCroppedRects(OpRect& src_rect, OpRect& dst_rect);

private:
	INT32 m_width;
	INT32 m_height;
};
#endif


