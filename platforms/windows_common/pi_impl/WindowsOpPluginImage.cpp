/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef NS4P_COMPONENT_PLUGINS

#include "modules/libvega/src/oppainter/vegaoppainter.h"

#include "platforms/windows_common/pi_impl/WindowsOpPluginImage.h"

/* static */
OP_STATUS OpPluginImage::Create(OpPluginImage** out_image, OpPluginBackgroundImage** out_background, const OpRect& plugin_rect, OpWindow* target_opwindow)
{
	OpAutoPtr<WindowsOpPluginImage> plugin_image = OP_NEW(WindowsOpPluginImage, ());
	RETURN_OOM_IF_NULL(plugin_image.get());
	RETURN_IF_ERROR(plugin_image->Init(plugin_rect));

	OpAutoPtr<WindowsOpPluginBackgroundImage> background_image = OP_NEW(WindowsOpPluginBackgroundImage, ());
	RETURN_OOM_IF_NULL(background_image.get());

	/* Store background image in a plugin image. */
	plugin_image->SetBackground(background_image.get());

	*out_image = plugin_image.release();
	*out_background = background_image.release();
	return OpStatus::OK;
}

OP_STATUS WindowsOpPluginImage::Init(const OpRect& plugin_rect)
{
	WindowsSharedBitmapManager* bitmap_manager = WindowsSharedBitmapManager::GetInstance();
	RETURN_OOM_IF_NULL(bitmap_manager);

	m_image_data = bitmap_manager->CreateMemory(m_identifier, plugin_rect.width, plugin_rect.height);
	RETURN_OOM_IF_NULL(m_image_data);

	m_plugin_rect = plugin_rect;

	return OpStatus::OK;
}

OP_STATUS WindowsOpPluginImage::Draw(OpPainter* painter, const OpRect& plugin_rect, const OpRect& paint_rect)
{
	if (!m_plugin_bitmap)
		RETURN_IF_ERROR(OpBitmap::Create(&m_plugin_bitmap, m_plugin_rect.width, m_plugin_rect.height, FALSE, FALSE, 0, 0, FALSE));

	/* Get the pointer to the image data of created bitmap. */
	UINT32* bmp_data = static_cast<UINT32*>(m_plugin_bitmap->GetPointer(OpBitmap::ACCESS_WRITEONLY));
	RETURN_OOM_IF_NULL(bmp_data);

	/* Get the pointer to the image data in the shared memory. */
	UINT32* paint_data = m_image_data->GetDataPtr();

	INT32 x, y;
	INT32 right = paint_rect.Right();
	INT32 bottom = paint_rect.Bottom();

	/* Copy only the changed rectangle. */
	for (y = paint_rect.y; y < bottom; y++)
	{
		x = paint_rect.x;

		for (unsigned int ptr = m_plugin_rect.width * y + x; x < right; x++)
		{
			bmp_data[ptr] = paint_data[ptr];
			ptr++;
		}
	}

	m_plugin_bitmap->ReleasePointer(TRUE);
	/* Draw the bitmap. */
	painter->DrawBitmapClipped(m_plugin_bitmap, paint_rect, OpPoint(m_plugin_rect.x + paint_rect.x, m_plugin_rect.y + paint_rect.y));
	return OpStatus::OK;
}

OP_STATUS WindowsOpPluginImage::Update(OpPainter* painter, const OpRect& plugin_rect, bool transparent)
{
	m_plugin_rect.x = plugin_rect.x;
	m_plugin_rect.y = plugin_rect.y;

	if (plugin_rect.width > m_plugin_rect.width || plugin_rect.height > m_plugin_rect.height)
	{
		/* Round up new dimensions to nearest 32 bytes and add 32 more bytes.
		   In case plugin rapidly changes size, we'll do less allocations. */
		int width = plugin_rect.width + plugin_rect.width % 32 + 32;
		int height = plugin_rect.height + plugin_rect.height % 32 + 32;
		RETURN_IF_ERROR(Resize(width, height));
	}

	if (transparent)
	{
		/* Update background bitmap. */
		RETURN_IF_ERROR(m_background->Update(painter, m_plugin_rect));
		/* Copy background into shared memory. */
		void* background_data = m_background->GetBitmap()->GetPointer(OpBitmap::ACCESS_READONLY);
		op_memcpy(m_image_data->GetDataPtr(), background_data, m_plugin_rect.width * m_plugin_rect.height * sizeof(UINT32));
		m_background->GetBitmap()->ReleasePointer(FALSE);
	}

	return OpStatus::OK;
}

OP_STATUS WindowsOpPluginImage::Resize(int width, int height)
{
	/* Recreate bitmap with bigger size. */
	OP_DELETE(m_plugin_bitmap);
	m_plugin_bitmap = NULL;
	RETURN_IF_ERROR(OpBitmap::Create(&m_plugin_bitmap, width, height, FALSE, FALSE, 0, 0, FALSE));

	/* Resize shared memory. */
	RETURN_IF_ERROR(m_image_data->ResizeMemory(width, height));

	/* Update local rect dimensions. */
	m_plugin_rect.width = width;
	m_plugin_rect.height = height;

	return OpStatus::OK;
}

OP_STATUS WindowsOpPluginBackgroundImage::Update(OpPainter* painter, const OpRect& plugin_rect)
{
	m_plugin_rect = plugin_rect;

	/* Retrieve background from underneath the plugin. */
	OP_DELETE(m_bitmap);
	m_bitmap = painter->CreateBitmapFromBackground(m_plugin_rect);
	RETURN_OOM_IF_NULL(m_bitmap);

	return OpStatus::OK;
}

#endif // NS4P_COMPONENT_PLUGINS
