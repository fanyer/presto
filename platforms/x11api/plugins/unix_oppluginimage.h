/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-12 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef UNIX_OPPLUGINIMAGE_H_
#define UNIX_OPPLUGINIMAGE_H_

#include "modules/pi/OpPluginImage.h"
#include "platforms/utilix/x11_all.h"

/** Unix implementation of OpPluginBackgroundImage, see documention for this class. */
class UnixOpPluginBackgroundImage : public OpPluginBackgroundImage
{
public:
	inline UnixOpPluginBackgroundImage();

	/// Copy a portion of the bitmap. No action will take place if they turn
	/// out to be equal. Automatically performs an XSync so you don't have to.
	OP_STATUS Copy(X11Types::Display* dpy, X11Types::Visual* vis, OpBitmap* bitmap, const OpRect& plugin_rect, unsigned int depth);

	/// Disable use of background image. This will tell the object to return
	/// 0 as its global identifier to signify that we're in opaque mode.
	inline void Disable();

	/// Enable use of background image. This will tell the object to return the
	/// actual drawable as its global identifier. This also means we're in
	/// transparent mode.
	inline void Enable();

	/// Return whether or not use of background image has been disabled.
	inline bool IsDisabled() const;

	/// Resize background image. This is strictly only necessary if the plug-in
	/// has been enlarged because the internal buffer will contain a smaller
	/// image.
	OP_STATUS Resize(X11Types::Display* dpy, int screen_number, const OpRect& plugin_rect, unsigned int depth);

	virtual OpPluginImageID GetGlobalIdentifier();

private:
	bool m_disabled;
	X11Types::Drawable m_drawable;
	uint32_t* m_bitmap;
	OpRect m_plugin_rect;
};

/** Unix implementation of OpPluginImage, see documention for this class. */
class UnixOpPluginImage : public OpPluginImage
{
public:
	static const unsigned int kDepth        = 24;  ///< Image colour depth
	static const unsigned int kBitsPerPixel = 32;  ///< Number of bits per pixel

	UnixOpPluginImage(UnixOpPluginBackgroundImage*, X11Types::Display* display, int screen_num);
	virtual ~UnixOpPluginImage();

	OP_STATUS Resize(const OpRect& plugin_rect, unsigned int depth);

	virtual OP_STATUS Draw(OpPainter* painter, const OpRect& plugin_rect, const OpRect& paint_rect);
	virtual OpPluginImageID GetGlobalIdentifier();
	virtual OP_STATUS Update(OpPainter* painter, const OpRect& plugin_rect, bool transparent);

private:
	X11Types::Display* m_display;
	int m_screen_num;
	X11Types::Drawable m_drawable;
	UnixOpPluginBackgroundImage* m_background;
	OpRect m_plugin_rect;
};

UnixOpPluginBackgroundImage::UnixOpPluginBackgroundImage() :
	m_disabled(false), m_drawable(0), m_bitmap(0) { }

void UnixOpPluginBackgroundImage::Disable()
{
	OP_ASSERT(m_disabled || m_drawable || !"UnixOpPluginBackgroundImage::Disable: No drawable to disable.");
	m_disabled = true;
}

void UnixOpPluginBackgroundImage::Enable()
{
	OP_ASSERT(m_drawable || !"UnixOpPluginBackgroundImage::Enable: No drawable to enable.");
	m_disabled = false;
}

bool UnixOpPluginBackgroundImage::IsDisabled() const
{
	return m_disabled;
}

#endif // UNIX_OPPLUGINIMAGE_H_
