/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2008-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#include "core/pch.h"

#ifdef CORE_THUMBNAIL_SUPPORT

#include "modules/thumbnails/thumbnailmanager.h"
#include "modules/thumbnails/thumbnails_module.h"

ThumbnailsModule::ThumbnailsModule()
#ifdef SUPPORT_GENERATE_THUMBNAILS
	: m_thumbnail_manager(NULL)
#endif
{
}

void ThumbnailsModule::InitL(const OperaInitInfo& info)
{
#ifdef SUPPORT_GENERATE_THUMBNAILS
	m_thumbnail_manager = OP_NEW_L(ThumbnailManager, ());
	LEAVE_IF_ERROR(m_thumbnail_manager->Construct());
#endif
}

void ThumbnailsModule::Destroy()
{
#ifdef SUPPORT_GENERATE_THUMBNAILS
	OP_DELETE(m_thumbnail_manager);
	m_thumbnail_manager = 0;
#endif
}

#endif // CORE_THUMBNAIL_SUPPORT
