/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_IMG_IMG_MODULE_H
#define MODULES_IMG_IMG_MODULE_H

#include "modules/hardcore/opera/module.h"

#include "modules/img/image.h"

#ifdef EMBEDDED_ICC_SUPPORT
class ImageColorManager;
#endif // EMBEDDED_ICC_SUPPORT

class ImgModule : public OperaModule
{
public:
	ImgModule() :
		img_manager(NULL),
#ifdef EMBEDDED_ICC_SUPPORT
		img_color_manager(NULL),
#endif // EMBEDDED_ICC_SUPPORT
		null_listener(NULL) {}

	void InitL(const OperaInitInfo& info);

	void Destroy();

	ImageManager* img_manager;
#ifdef EMBEDDED_ICC_SUPPORT
	ImageColorManager* img_color_manager;
#endif // EMBEDDED_ICC_SUPPORT
	NullImageListener* null_listener;
};

#define imgManager (g_opera->img_module.img_manager)
#ifdef EMBEDDED_ICC_SUPPORT
#define g_color_manager (g_opera->img_module.img_color_manager)
#endif // EMBEDDED_ICC_SUPPORT
#define null_image_listener (g_opera->img_module.null_listener)

#define IMG_MODULE_REQUIRED

#endif // !MODULES_IMG_IMG_MODULE_H
