/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "platforms/mac/pi/plugins/MacOpPluginImage.h"
#include "modules/pi/OpBitmap.h"
#include "modules/pi/OpPainter.h"

#if defined(_PLUGIN_SUPPORT_)

// Set to turn on all the printfs to debug the PluginWrapper
//#define MAC_PLUGIN_WRAPPER_DEBUG

//////////////////////////////////////////////////////////////////////

OP_STATUS OpPluginImage::Create(OpPluginImage** out_image, OpPluginBackgroundImage** background, const OpRect& plugin_rect, OpWindow* target_opwindow)
{
	MacOpPluginImage* plugin_image = OP_NEW(MacOpPluginImage, ());
	RETURN_OOM_IF_NULL(plugin_image);

	OP_STATUS err = plugin_image->Init(plugin_rect);
	if (OpStatus::IsError(err))
	{
		OP_DELETE(plugin_image);
		return err;
	}

	*out_image = plugin_image;

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////

MacOpPluginImage::~MacOpPluginImage()
{
}

//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginImage::Init(const OpRect& plugin_rect)
{
	m_plugin_rect = plugin_rect;
#ifdef MAC_PLUGIN_WRAPPER_DEBUG
	fprintf(stderr, "MacOpPluginImage::Init %p, width:%d, height:%d\n", this, plugin_rect.width, plugin_rect.height);
#endif // MAC_PLUGIN_WRAPPER_DEBUG
	return m_mac_shm.Create(plugin_rect.width * 4 * plugin_rect.height);
}

//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginImage::Draw(OpPainter* painter, const OpRect& plugin_rect, const OpRect& paint_rect)
{
	OpBitmap* bitmap = 0;
	OP_STATUS status = OpBitmap::Create(&bitmap, plugin_rect.width, plugin_rect.height, TRUE, TRUE, 0, 0, TRUE);
	if (OpStatus::IsError(status))
		return status;

#ifdef MAC_PLUGIN_WRAPPER_DEBUG
	fprintf(stderr, "MacOpPluginImage::Draw %p, key:0x%x, width:%d, height:%d\n", this, m_mac_shm.GetKey(), plugin_rect.width, plugin_rect.height);
#endif // MAC_PLUGIN_WRAPPER_DEBUG

	// Copy into the memory....
	OP_ASSERT(bitmap->GetBytesPerLine() == (UINT32)4*plugin_rect.width);
	unsigned char *image_data = (unsigned char *)bitmap->GetPointer();
	
	op_memcpy(image_data, m_mac_shm.GetBuffer(), plugin_rect.width * 4 * plugin_rect.height);
	bitmap->ReleasePointer();

	painter->DrawBitmapAlpha(bitmap, OpPoint(plugin_rect.x, plugin_rect.y));
	OP_DELETE(bitmap);

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////

OpPluginImageID MacOpPluginImage::GetGlobalIdentifier()
{
	return (UINT64)m_mac_shm.GetKey();
}

//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginImage::Update(OpPainter* painter, const OpRect& plugin_rect, bool transparent)
{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG
	//printf("MacOpPluginImage::Update w:%d, h:%d\n", plugin_rect.width, plugin_rect.height);
#endif // MAC_PLUGIN_WRAPPER_DEBUG
	
	// No change so just exit
	if (m_plugin_rect.width == plugin_rect.width && m_plugin_rect.height == plugin_rect.height)
		return OpStatus::OK;

	// Re-create the shared memory
	m_mac_shm.Destroy();
    m_mac_shm.Create(plugin_rect.width * 4 * plugin_rect.height);

	// Save the new size
	m_plugin_rect = plugin_rect;

	return OpStatus::OK;
}


#endif // defined(_PLUGIN_SUPPORT_)

