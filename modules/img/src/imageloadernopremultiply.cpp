/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef IMGLOADER_NO_PREMULTIPLY

#include "modules/img/imageloadernopremultiply.h"
#include "modules/libvega/src/vegaswbuffer.h"

#ifdef EMBEDDED_ICC_SUPPORT
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#endif // EMBEDDED_ICC_SUPPORT


ImageLoaderNoPremultiply::ImageLoaderNoPremultiply()
	: m_contentProvider(NULL)
	, m_imageDecoder(NULL)
	, m_swBuffer(NULL)
	, m_palette(NULL)
	, m_scanlinesAdded(0) 
	, m_ignoreColorSpaceConversion(FALSE)
	, m_firstFrameLoaded(FALSE)
	, m_framesAdded(0)
#ifdef EMBEDDED_ICC_SUPPORT
	, m_color_transform(NULL)
#endif // EMBEDDED_ICC_SUPPORT
{
}

ImageLoaderNoPremultiply::~ImageLoaderNoPremultiply() 
{
	if(m_contentProvider)
		m_contentProvider->Reset();
	OP_DELETE(m_imageDecoder); 
#ifdef EMBEDDED_ICC_SUPPORT
	OP_DELETE(m_color_transform);
#endif // EMBEDDED_ICC_SUPPORT
}

void ImageLoaderNoPremultiply::OnLineDecoded(void* data, INT32 line, INT32 lineHeight)
{
	if (m_framesAdded != 1)
		return;
	if (*m_swBuffer && (*m_swBuffer)->IsBacked())
	{
		unsigned width = (*m_swBuffer)->width;
		UINT8 *src = (UINT8 *)data;
		VEGAPixelAccessor dst = (*m_swBuffer)->GetAccessor(0, line);

		if (m_palette)
		{
			for (unsigned j = 0; j < width; ++j)
			{
				unsigned idx = 3 * (*src);
				UINT8 r = m_palette[idx + 0];
				UINT8 g = m_palette[idx + 1];
				UINT8 b = m_palette[idx + 2];
				dst.StoreARGB(0xff, r, g, b);
				++dst;
				++src;
			}
			
		}
		else
		{
#ifdef EMBEDDED_ICC_SUPPORT
			if (m_color_transform)
			{
				ImageManagerImp* imgman_impl = static_cast<ImageManagerImp*>(imgManager);

				if (UINT32* tmp = imgman_impl->GetScratchBuffer(width))
				{
					m_color_transform->Apply(tmp, (UINT32*)src, width);
					src = (UINT8*)tmp;
				}
				else
				{
					OP_DELETE(m_color_transform);
					m_color_transform = NULL;
				}
			}
#endif // EMBEDDED_ICC_SUPPORT
			for (unsigned j = 0; j < width; ++j)
			{
#if defined(OPERA_BIG_ENDIAN)
				UINT8 a = src[0];
				UINT8 r = src[1];
				UINT8 g = src[2];
				UINT8 b = src[3];
#else
				UINT8 a = src[3];
				UINT8 r = src[2];
				UINT8 g = src[1];
				UINT8 b = src[0];
#endif
				dst.StoreARGB(a, r, g, b);
				++dst;
				src+=4;
			}
		}

		for (int i = 1; i < lineHeight; ++i)
		{
			VEGAPixelAccessor src = (*m_swBuffer)->GetAccessor(0, line);
			VEGAPixelAccessor dst = (*m_swBuffer)->GetAccessor(0, line + i);
			for (unsigned j = 0; j < width; ++j, ++src, ++dst)
				dst = src;
		}
	}
	m_scanlinesAdded += lineHeight;
}

BOOL ImageLoaderNoPremultiply::OnInitMainFrame(INT32 width, INT32 height)
{ 
	*m_swBuffer = OP_NEW(VEGASWBuffer, ());
	if (*m_swBuffer)
		if (OpStatus::IsError((*m_swBuffer)->Create(width, height)))
			return FALSE;
	return TRUE; 
}

void ImageLoaderNoPremultiply::OnNewFrame(const ImageFrameData& image_frame_data)
{
	m_palette = (UINT8 *)image_frame_data.pal;
	++m_framesAdded;
	if (m_framesAdded > 1)
		m_firstFrameLoaded = TRUE;
}

void ImageLoaderNoPremultiply::OnAnimationInfo(INT32 nrOfRepeats)
{
}

void ImageLoaderNoPremultiply::OnDecodingFinished()
{
	m_firstFrameLoaded = TRUE;
	OP_ASSERT(*m_swBuffer && (*m_swBuffer)->height == m_scanlinesAdded);
}

#ifdef IMAGE_METADATA_SUPPORT
void ImageLoaderNoPremultiply::OnMetaData(ImageMetaData id, const char* data) 
{
}
#endif // IMAGE_METADATA_SUPPORT

#ifdef EMBEDDED_ICC_SUPPORT
void ImageLoaderNoPremultiply::OnICCProfileData(const UINT8* data, unsigned datalen)
{
	if (IgnoreColorSpaceConversions() ||
		!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::ImagesUseICC))
	{
		return;
	}

	ICCProfile* image_profile;
	if (OpStatus::IsSuccess(g_color_manager->CreateProfile(&image_profile, data, datalen)))
	{
		OpStatus::Ignore(g_color_manager->CreateTransform(&m_color_transform, image_profile));

		OP_DELETE(image_profile);
	}
}
#endif // EMBEDDED_ICC_SUPPORT

BOOL ImageLoaderNoPremultiply::IgnoreColorSpaceConversions() const
{
	return m_ignoreColorSpaceConversion;
}

OP_STATUS ImageLoaderNoPremultiply::Load(ImageContentProvider* contentProvider, VEGASWBuffer **swBuffer, BOOL ignoreColorSpaceConversion)
{
	ImageLoaderNoPremultiply imageLoader;
	imageLoader.m_ignoreColorSpaceConversion = ignoreColorSpaceConversion;
	ImageDecoderFactory* factory = ((ImageManagerImp*)imgManager)->GetImageDecoderFactory(contentProvider->ContentType());
	if (factory == NULL)
		return OpStatus::ERR;

	ImageDecoder* imageDecoder = factory->CreateImageDecoder(&imageLoader);
	if (imageDecoder == NULL)
		return OpStatus::ERR;

	imageLoader.m_imageDecoder = imageDecoder;
	imageLoader.m_contentProvider = contentProvider;
	imageLoader.m_swBuffer = swBuffer;
	while (!imageLoader.m_firstFrameLoaded)
	{
		RETURN_IF_ERROR(imageLoader.DoLoad());
		const char* data;
		INT32 data_len;
		BOOL more;
		OP_STATUS ret_val_loc = imageLoader.m_contentProvider->GetData(data, data_len, more);
		if (!imageLoader.m_firstFrameLoaded && (OpStatus::IsError(ret_val_loc) || data_len == 0))
			return OpStatus::ERR;
	}
	return OpStatus::OK;
}

OP_STATUS ImageLoaderNoPremultiply::DoLoad()
{
	OP_STATUS loadStatus = OpStatus::OK;
	int resendBytes = 0;
	INT32 oldDataLen = 0;
	while (TRUE)
	{
		const char *data;
		INT32 dataLen;
		BOOL more;
		OP_STATUS retVal = m_contentProvider->GetData(data, dataLen, more);
		if (OpStatus::IsSuccess(retVal))
		{
			if (dataLen > 0)
			{
				if (dataLen == oldDataLen)
				{
					// We couldn't grow, even if we thought we could.
					return OpStatus::OK;
				}
				BOOL moreData = more || !m_contentProvider->IsLoaded();
				OP_STATUS retValDecode = m_imageDecoder->DecodeData((unsigned char*)data, dataLen, moreData, resendBytes);

				if (OpStatus::IsError(retValDecode))
				{
					if(retValDecode != OpStatus::ERR_NO_MEMORY && *m_swBuffer && m_scanlinesAdded == (*m_swBuffer)->height)
						OnDecodingFinished();
					loadStatus = retValDecode;
					return loadStatus;
				}
				if (!moreData)
				{
					m_contentProvider->ConsumeData(dataLen);
				}
				else
				{
					m_contentProvider->ConsumeData(dataLen - resendBytes);
				}
			}
			if (more && dataLen > 0 && dataLen == resendBytes)
			{
				oldDataLen = dataLen;
				RETURN_IF_ERROR(m_contentProvider->Grow());
				continue;
			}
		}
		else if (OpStatus::IsMemoryError(retVal) || !more)
		{
			loadStatus = retVal;
		}
		else
		{
		}
		// Else, OpStatus::ERR, we haven't got any data yet, silent wait.
		break;
	}
	return loadStatus;
}

#endif //IMGLOADER_NO_PREMULTIPLY
