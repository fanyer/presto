/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef __MACOPPLUGINIMAGE_H__
#define __MACOPPLUGINIMAGE_H__

#if defined(_PLUGIN_SUPPORT_)

#include "modules/pi/OpPluginImage.h"
#include "platforms/mac/util/MacSharedMemoryCreator.h"

/**
 * The OpPluginImage represents a single frame from the plugin.
 *
 */
class MacOpPluginImage : public OpPluginImage
{
public:
	MacOpPluginImage() {}
	~MacOpPluginImage();

	OP_STATUS Init(const OpRect& plugin_rect);

	virtual OP_STATUS Draw(OpPainter* painter, const OpRect& plugin_rect, const OpRect& paint_rect);
	virtual OpPluginImageID GetGlobalIdentifier();
    virtual OP_STATUS Update(OpPainter* painter, const OpRect& plugin_rect, bool transparent);
private:
	MacSharedMemoryCreator m_mac_shm;
	
	OpRect	m_plugin_rect;
};

#endif // defined(_PLUGIN_SUPPORT_)

#endif // __MACOPPLUGINIMAGE_H__
