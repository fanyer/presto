/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef WINDOWS_OP_PLUGIN_IMAGE_H
#define WINDOWS_OP_PLUGIN_IMAGE_H

#ifdef NS4P_COMPONENT_PLUGINS

#include "modules/pi/OpPluginImage.h"

#include "platforms/windows/IPC/SharedPluginBitmap.h"

class WindowsOpPluginBackgroundImage : public OpPluginBackgroundImage
{
public:
	WindowsOpPluginBackgroundImage()
		: m_identifier(reinterpret_cast<OpPluginImageID>(this)),
		  m_bitmap(NULL) { }


	virtual ~WindowsOpPluginBackgroundImage()
	{
		if (WindowsSharedBitmapManager* bitmap_manager = WindowsSharedBitmapManager::GetInstance())
			bitmap_manager->CloseMemory(m_identifier);

		OP_DELETE(m_bitmap);
	}

	/* Implementing OpPluginBackgroundImage. */

	virtual OpPluginImageID	GetGlobalIdentifier() { return m_identifier; }

	OpBitmap*				GetBitmap() { return m_bitmap; }
	OP_STATUS				Update(OpPainter* painter, const OpRect& plugin_rect);
	void					SetPluginRect(const OpRect& rect) { m_plugin_rect.Set(rect); }

private:
	OpPluginImageID			m_identifier;
	OpBitmap*				m_bitmap;
	OpRect					m_plugin_rect;
};

class WindowsOpPluginImage : public OpPluginImage
{
public:
	WindowsOpPluginImage()
		: m_identifier(reinterpret_cast<OpPluginImageID>(this)),
		  m_image_data(NULL),
		  m_background(NULL),
		  m_plugin_bitmap(NULL) { }

	virtual ~WindowsOpPluginImage()
	{
		if (WindowsSharedBitmapManager* bitmap_manager = WindowsSharedBitmapManager::GetInstance())
			bitmap_manager->CloseMemory(m_identifier);

		OP_DELETE(m_plugin_bitmap);
	}

	OP_STATUS				Init(const OpRect& paint_rect);

	/* Implementing OpPluginImage. */

	virtual OP_STATUS		Draw(OpPainter* painter, const OpRect& plugin_rect, const OpRect& paint_rect);
	virtual OpPluginImageID	GetGlobalIdentifier() { return m_identifier; }
	virtual OP_STATUS		Update(OpPainter* painter, const OpRect& plugin_rect, bool transparent);

	void					SetBackground(WindowsOpPluginBackgroundImage* background) { m_background = background; }
	OP_STATUS				Resize(int width, int height);

private:
	OpPluginImageID			m_identifier;
	WindowsSharedBitmap* 	m_image_data;
	WindowsOpPluginBackgroundImage*
							m_background;
	OpBitmap*				m_plugin_bitmap;
	OpRect					m_plugin_rect;
};

#endif // NS4P_COMPONENT_PLUGINS
#endif // WINDOWS_OP_PLUGIN_IMAGE_H
