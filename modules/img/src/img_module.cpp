/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/doc/imageprogresshandler.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/img/decoderfactorygif.h"
#include "modules/img/img_module.h"
#include "modules/url/url_enum.h"

#ifdef EMBEDDED_ICC_SUPPORT
#include "modules/img/imagecolormanager.h"
#endif // EMBEDDED_ICC_SUPPORT

#ifdef _JPG_SUPPORT_
#include "modules/img/decoderfactoryjpg.h"
#endif // _JPG_SUPPORT_

#ifdef _PNG_SUPPORT_
#include "modules/img/decoderfactorypng.h"
#endif // _PNG_SUPPORT_

#ifdef ICO_SUPPORT
#include "modules/img/decoderfactoryico.h"
#endif // _ICO_SUPPORT_

#ifdef _XBM_SUPPORT_
#include "modules/img/decoderfactoryxbm.h"
#endif // _XBM_SUPPORT_

#ifdef _BMP_SUPPORT_
#include "modules/img/decoderfactorybmp.h"
#endif // _BMP_SUPPORT_

#ifdef WBMP_SUPPORT
#include "modules/img/decoderfactorywbmp.h"
#endif // WBMP_SUPPORT

#ifdef WEBP_SUPPORT
#include "modules/img/decoderfactorywebp.h"
#endif // WEBP_SUPPORT

static void ImgModuleAddImageFactoryL(ImageDecoderFactory* factory, INT32 type, BOOL check_header)
{
	OP_STATUS ret_val = imgManager->AddImageDecoderFactory(factory, type, check_header);
	if (OpStatus::IsError(ret_val))
	{
		OP_DELETE(factory);
		LEAVE(ret_val);
	}
}

void ImgModule::InitL(const OperaInitInfo& info)
{
	ImageProgressHandler* progressHandler = ImageProgressHandler::Create();
	if (progressHandler == NULL)
	{
		LEAVE(OpStatus::ERR_NO_MEMORY);
	}
	imgManager = ImageManager::Create(progressHandler);
	if (imgManager == NULL)
	{
		OP_DELETE(progressHandler);
		LEAVE(OpStatus::ERR_NO_MEMORY);
	}

#ifdef EMBEDDED_ICC_SUPPORT
	g_color_manager = ImageColorManager::Create();
	if (g_color_manager == NULL)
	{
		LEAVE(OpStatus::ERR_NO_MEMORY);
	}
#endif // EMBEDDED_ICC_SUPPORT

	ImageDecoderFactory* gif_factory = OP_NEW_L(DecoderFactoryGif, ());
	ImgModuleAddImageFactoryL(gif_factory, URL_GIF_CONTENT, TRUE);
#if defined(_JPG_SUPPORT_)
	ImageDecoderFactory* jpg_factory = OP_NEW_L(DecoderFactoryJpg, ());
	ImgModuleAddImageFactoryL(jpg_factory, URL_JPG_CONTENT, TRUE);
#endif // _JPG_SUPPORT_
#if defined(_PNG_SUPPORT_)
	ImageDecoderFactory* png_factory = OP_NEW_L(DecoderFactoryPng, ());
	ImgModuleAddImageFactoryL(png_factory, URL_PNG_CONTENT, TRUE);
#endif // _PNG_SUPPORT_
#if defined(ICO_SUPPORT)
	ImageDecoderFactory* ico_factory = OP_NEW_L(DecoderFactoryIco, ());
	ImgModuleAddImageFactoryL(ico_factory, URL_ICO_CONTENT, TRUE);
#endif // ICO_SUPPORT
#if defined(_XBM_SUPPORT_)
	ImageDecoderFactory* xbm_factory = OP_NEW_L(DecoderFactoryXbm, ());
	ImgModuleAddImageFactoryL(xbm_factory, URL_XBM_CONTENT, TRUE);
#endif // _XBM_SUPPORT_
#if defined(_BMP_SUPPORT_)
	ImageDecoderFactory* bmp_factory = OP_NEW_L(DecoderFactoryBmp, ());
	ImgModuleAddImageFactoryL(bmp_factory, URL_BMP_CONTENT, TRUE);
#endif // _BMP_SUPPORT_
#if defined(WBMP_SUPPORT)
	ImageDecoderFactory* wbmp_factory = OP_NEW_L(DecoderFactoryWbmp, ());
	ImgModuleAddImageFactoryL(wbmp_factory, URL_WBMP_CONTENT, TRUE);
#endif // WBMP_SUPPORT
#ifdef WEBP_SUPPORT
	ImageDecoderFactory* webp_factory = OP_NEW_L(DecoderFactoryWebP, ());
	ImgModuleAddImageFactoryL(webp_factory, URL_WEBP_CONTENT, TRUE);
#endif // WEBP_SUPPORT
#ifdef ASYNC_IMAGE_DECODERS
# ifdef ASYNC_IMAGE_DECODERS_EMULATION
	AsyncImageDecoderFactory* async_factory = OP_NEW_L(AsyncEmulationDecoderFactory, ());
# else
	AsyncImageDecoderFactory* async_factory = NULL;
	LEAVE_IF_ERROR(AsyncImageDecoderFactory::Create(&async_factory));
# endif // ASYNC_IMAGE_DECODERS_EMULATION
	imgManager->SetAsyncImageDecoderFactory(async_factory);
#endif // ASYNC_IMAGE_DECODERS
	if (g_memory_manager != NULL)
	{
		imgManager->SetCacheSize(g_memory_manager->GetMaxImgMemory(), IMAGE_CACHE_POLICY_SOFT); // FIXME:IMG-KILSMO
	}
}

void ImgModule::Destroy()
{
	OP_DELETE(imgManager);
	imgManager = NULL;

#ifdef EMBEDDED_ICC_SUPPORT
	OP_DELETE(g_color_manager);
	g_color_manager = NULL;
#endif // EMBEDDED_ICC_SUPPORT
}

