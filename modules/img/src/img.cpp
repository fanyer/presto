/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/img/image.h"
#include "modules/img/src/imagerep.h"

Image::Image() : image_rep(NULL)
{
}

Image::Image(const Image &image) : ImageContentProviderListener()
{
    image_rep = image.image_rep;

	IncRefCount();
}

Image::~Image()
{
	DecRefCount();
}

Image& Image::operator=(const Image &image)
{
	SetImageRep(image.image_rep);

	return *this;
}

UINT32 Image::Width() const
{
	OP_NEW_DBG("Image::Width", "imageloading");

	return (image_rep ? image_rep->Width() : 0);
}

UINT32 Image::Height() const
{
	OP_NEW_DBG("Image::Height", "imageloading");

	return (image_rep ? image_rep->Height() : 0);
}

OP_STATUS Image::IncVisible(ImageListener* image_listener)
{
	if (image_rep)
	{
		return image_rep->IncVisible(image_listener);
	}
	return OpStatus::OK;
}

void Image::DecVisible(ImageListener* image_listener)
{
	if (image_rep)
	{
		image_rep->DecVisible(image_listener);
	}
}

void Image::PreDecode(ImageListener* image_listener)
{
	if (image_rep)
	{
		image_rep->PreDecode(image_listener);
	}
}

INT32 Image::CountListeners()
{
	if (image_rep)
	{
		image_rep->CountListeners();
	}
	return 0;
}

INT32 Image::GetRefCount()
{
	if (image_rep)
		return image_rep->GetRefCount();
	return 0;
}

/*BOOL Image::HeaderLoaded()
{
	return (image_source ? image_source->HeaderLoaded() : FALSE);
}*/

UINT32 Image::GetLastDecodedLine() const
{
	return (image_rep ? image_rep->GetLastDecodedLine() : 0);
}

BOOL Image::ImageDecoded() const
{
#ifdef ASYNC_IMAGE_DECODERS
	return image_rep && image_rep->IsDataLoaded();
#else
	return image_rep && image_rep->ImageLoaded();
#endif 
}

BOOL Image::IsTransparent() const
{
	return !image_rep || image_rep->IsTransparent();
}

BOOL Image::IsInterlaced() const
{
	return !image_rep || image_rep->IsInterlaced();
}

BOOL Image::IsContentRelevant() const
{
	return !image_rep || image_rep->IsContentRelevant();
}

BOOL Image::IsEmpty() const
{
	return (image_rep == NULL);
}

void Image::SetImageRep(ImageRep* imageRep)
{
	DecRefCount();

	image_rep = imageRep;

	IncRefCount();
}

int Image::GetBitsPerPixel()
{
	if (image_rep != NULL)
	{
		return image_rep->GetBitsPerPixel();
	}
	return 0;
}

int Image::GetFrameCount()
{
	if (image_rep != NULL)
	{
		return image_rep->GetFrameCount();
	}
	return 0;
}

OP_STATUS Image::ReplaceBitmap(OpBitmap* new_bitmap)
{
	if (image_rep != NULL)
	{
		return image_rep->ReplaceBitmap(new_bitmap);
	}
	return OpStatus::OK;
}

void Image::IncRefCount()
{
	if (image_rep != NULL)
	{
		image_rep->IncRefCount();
	}
}

void Image::DecRefCount()
{
	if (image_rep != NULL)
	{
		image_rep->DecRefCount();
        image_rep = NULL; // probably not neccesary
	}
}

void Image::SyncronizeAnimation(ImageListener* dest_listener, ImageListener* source_listener)
{
	if (image_rep != NULL)
	{
		image_rep->SyncronizeAnimation(dest_listener, source_listener);
	}
}

OpPoint Image::GetBitmapOffset()
{
	if (image_rep != NULL)
	{
		return image_rep->GetBitmapOffset();
	}
	return OpPoint();
}

OpBitmap* Image::GetBitmap(ImageListener* image_listener)
{
	if (image_rep != NULL)
	{
		return image_rep->GetBitmap(image_listener);
	}
	return NULL;
}

void Image::ReleaseBitmap()
{
	if (image_rep != NULL)
	{
		image_rep->ReleaseBitmap();
	}
}

OpBitmap* Image::GetTileBitmap(ImageListener* image_listener, int desired_width, int desired_height)
{
	if (image_rep == NULL)
		return NULL;
	return image_rep->GetTileBitmap(image_listener, desired_width, desired_height);
}

void Image::ReleaseTileBitmap()
{
	if (image_rep == NULL)
		return;
	image_rep->ReleaseTileBitmap();
}

OpBitmap* Image::GetEffectBitmap(INT32 effect, INT32 effect_value, ImageListener* image_listener)
{
	if (image_rep == NULL)
	{
		return NULL;
	}
	return image_rep->GetEffectBitmap(effect, effect_value, image_listener);
}

void Image::ReleaseEffectBitmap()
{
	if (image_rep != NULL)
		image_rep->ReleaseEffectBitmap();
}

OpBitmap* Image::GetTileEffectBitmap(INT32 effect, INT32 effect_value, int horizontal_count, int vertical_count)
{
	if (image_rep != NULL)
		return image_rep->GetTileEffectBitmap(effect, effect_value, horizontal_count, vertical_count);
	return NULL;
}

void Image::ReleaseTileEffectBitmap()
{
	if (image_rep != NULL)
		image_rep->ReleaseTileEffectBitmap();
}

void Image::OnMoreData(ImageContentProvider* content_provider)
{
	if (image_rep != NULL)
	{
		image_rep->OnMoreData(content_provider);
		if (image_rep->IsFailed())
		{
			image_rep->ReportError();
		}
	}
}

OP_STATUS Image::OnLoadAll(ImageContentProvider* content_provider)
{
	if (image_rep != NULL)
	{
		image_rep->OnLoadAll(content_provider);
	}
	return OpStatus::OK;
}

BOOL Image::IsAnimated()
{
	if (image_rep)
	{
		return image_rep->IsAnimated();
	}
	return FALSE;
}

INT32 Image::GetCurrentFrameDuration(ImageListener* image_listener)
{
	if (image_rep)
	{
		return image_rep->GetCurrentFrameDuration(image_listener);
	}
	return 0;
}

OpRect Image::GetCurrentFrameRect(ImageListener* image_listener)
{
	if (image_rep)
	{
		return image_rep->GetCurrentFrameRect(image_listener);
	}
	return OpRect(0, 0, 0, 0);
}

BOOL Image::Animate(ImageListener* image_listener)
{
	if (image_rep)
	{
		return image_rep->Animate(image_listener);
	}
	return FALSE;
}

void Image::ResetAnimation(ImageListener* image_listener)
{
	if (image_rep)
	{
		image_rep->ResetAnimation(image_listener);
	}
}

void Image::PeekImageDimension()
{
	if (image_rep)
	{
        OpStatus::Ignore(image_rep->PeekImageDimension());
	}
}

BOOL Image::IsBottomToTop()
{
    return image_rep && image_rep->IsBottomToTop();
}

UINT32 Image::GetLowestDecodedLine()
{
	return image_rep ? image_rep->GetLowestDecodedLine() : 0;
}

#ifdef IMG_GET_AVERAGE_COLOR
COLORREF Image::GetAverageColor(ImageListener* image_listener)
{
    return image_rep ? image_rep->GetAverageColor(image_listener) : 0;
}
#endif // IMG_GET_AVERAGE_COLOR

#ifdef HAS_ATVEF_SUPPORT

BOOL Image::IsAtvefImage()
{
    return image_rep && image_rep->IsAtvefImage();
}

#endif // HAS_ATVEF_SUPPORT

BOOL Image::IsFailed()
{
	return image_rep && image_rep->IsFailed();
}


BOOL Image::IsOOM()
{
    return image_rep && image_rep->IsOOM();
}

#ifdef IMAGE_METADATA_SUPPORT
char* Image::GetMetaData(ImageMetaData id)
{
	return image_rep ? image_rep->GetMetaData(id) : NULL;
}

BOOL Image::HasMetaData()
{
	return image_rep && image_rep->HasMetaData();
}
#endif // IMAGE_METADATA_SUPPORT

