/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2008-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifdef CORE_THUMBNAIL_SUPPORT

#ifndef MODULES_THUMBNAILS_MODULE_H
#define MODULES_THUMBNAILS_MODULE_H

#include "modules/hardcore/opera/module.h"

#ifdef SUPPORT_GENERATE_THUMBNAILS
class ThumbnailManager;
#endif // SUPPORT_GENERATE_THUMBNAILS

class ThumbnailsModule : public OperaModule
{
public:
	ThumbnailsModule();
  
	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();

#ifdef SUPPORT_GENERATE_THUMBNAILS
	ThumbnailManager* m_thumbnail_manager;
#endif
};

#define THUMBNAILS_MODULE_REQUIRED

#ifdef SUPPORT_GENERATE_THUMBNAILS
#define g_thumbnail_manager g_opera->thumbnails_module.m_thumbnail_manager
#endif

#endif // MODULES_THUMBNAILS_MODULE_H
#endif // CORE_THUMBNAIL_SUPPORT
