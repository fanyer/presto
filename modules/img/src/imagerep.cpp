/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/img/src/imagerep.h"

#include "modules/img/src/imagecontent.h"
#include "modules/img/imagedecoderfactory.h"
#include "modules/img/src/imagemanagerimp.h"

#ifdef CACHE_UNUSED_IMAGES
#include "modules/logdoc/urlimgcontprov.h"
#endif // CACHE_UNUSED_IMAGES

#include "modules/pi/OpBitmap.h"
#include "modules/pi/OpScreenInfo.h"

#ifdef EMBEDDED_ICC_SUPPORT
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#endif // EMBEDDED_ICC_SUPPORT

static int get_nr_of_bits(int nr_of_colors)
{
	if (nr_of_colors <= 16)
	{
		if (nr_of_colors <= 4)
		{
			if (nr_of_colors <= 2)
			{
				return 1;
			}
			else
			{
				return 2;
			}
		}
		else
		{
			if (nr_of_colors <= 8)
			{
				return 3;
			}
			else
			{
				return 4;
			}
		}
	}
	else
	{
		if (nr_of_colors <= 64)
		{
			if (nr_of_colors <= 32)
			{
				return 5;
			}
			else
			{
				return 6;
			}
		}
		else
		{
			if (nr_of_colors <= 128)
			{
				return 7;
			}
			else
			{
				return 8;
			}
		}
	}
}

ImageLoader* ImageLoader::Create(ImageRep* rep, ImageContentProvider* content_provider)
{
	// We might want to create a reader object from the content provider instead.
	ImageLoader* image_loader = OP_NEW(ImageLoader, (rep, content_provider));
	if (image_loader != NULL)
	{
#ifdef ASYNC_IMAGE_DECODERS
		AsyncImageDecoderFactory* factory = ((ImageManagerImp*)imgManager)->GetAsyncImageDecoderFactory();
#else
		ImageDecoderFactory* factory = ((ImageManagerImp*)imgManager)->GetImageDecoderFactory(image_loader->content_provider->ContentType());
#endif // ASYNC_IMAGE_DECODERS
		if (factory == NULL)
		{
			OP_DELETE(image_loader);
			return NULL;
		}
		ImageDecoder* image_decoder = factory->CreateImageDecoder(image_loader
#ifdef ASYNC_IMAGE_DECODERS_EMULATION
			, image_loader->content_provider->ContentType()
#endif // ASYNC_IMAGE_DECODERS_EMULATION
			);
		if (image_decoder == NULL)
		{
			OP_DELETE(image_loader);
			return NULL;
		}
		image_loader->image_decoder = image_decoder;
	}
	return image_loader;
}

ImageLoader::ImageLoader(ImageRep* rep, ImageContentProvider* content_provider) : rep(rep),
										  image_decoder(NULL), content_provider(content_provider),
										  current_frame_bitmap(NULL),
#ifdef EMBEDDED_ICC_SUPPORT
										  color_transform(NULL),
#endif // EMBEDDED_ICC_SUPPORT
										  rect(0, 0, 0, 0),
										  frame_nr(0), nr_of_repeats(1), load_status(OpStatus::OK),
										  is_transparent(FALSE), bits_per_pixel(0), scanlines_added(0)
{
}

ImageLoader::~ImageLoader()
{
	content_provider->Reset();
	OP_DELETE(image_decoder);
#ifdef EMBEDDED_ICC_SUPPORT
	OP_DELETE(color_transform);
#endif // EMBEDDED_ICC_SUPPORT
	if (frame_nr > 1 && !rep->ImageLoaded())
	{
		OP_DELETE(current_frame_bitmap);
	}
	if (frame_nr == 1)
	{
		// Will not delete the OpBitmap
		OP_DELETE(current_frame_bitmap);
	}
}

void ImageLoader::OnLineDecoded(void* data, INT32 line, INT32 lineHeight)
{
	if (OpStatus::IsSuccess(load_status))
	{
		// current_frame_bitmap is created in a call to OnNewFrame(),
		// which has to be called before calling OnLineDecoded():
		OP_ASSERT(current_frame_bitmap);

		if (frame_nr > 1 && lineHeight > 1)
		{
			lineHeight = 1;
		}

		INT32 bitmap_height = current_frame_bitmap->Height();
		if (lineHeight > bitmap_height - line)
		{
			lineHeight = bitmap_height - line;
		}
		if (line < 0)
		{
			lineHeight = 0;
		}


		INT32 i = 0;
		for (; i < lineHeight; i++)
		{
#ifdef SUPPORT_INDEXED_OPBITMAP
			if (is_current_indexed)
			{
				OpStatus::Ignore(current_frame_bitmap->AddIndexedLine(data, line + i)); // FIXME:OOM-kilsmo, check if we can trust the return value from Add...Line.
				rep->AnalyzeIndexedData(data, line + i);
			}
			else
#endif // SUPPORT_INDEXED_OPBITMAP
			{
#ifdef USE_PREMULTIPLIED_ALPHA
				if (current_frame_bitmap->HasAlpha())
				{
#ifdef EMBEDDED_ICC_SUPPORT
					if (color_transform)
					{
						int bitmap_width = current_frame_bitmap->Width();
						if (UINT32* tmp = color_transform->Apply(static_cast<UINT32*>(data), bitmap_width))
						{
							data = tmp;
						}
					}
#endif // EMBEDDED_ICC_SUPPORT

					OpStatus::Ignore(current_frame_bitmap->AddLineAndPremultiply(data, line + i)); // FIXME:OOM-kilsmo
				}
				else
#endif // USE_PREMULTIPLIED_ALPHA
#ifdef EMBEDDED_ICC_SUPPORT
				if (color_transform)
				{
					OpStatus::Ignore(current_frame_bitmap->AddLineWithColorTransform(data, line + i, color_transform));
				}
				else
#endif // EMBEDDED_ICC_SUPPORT
				{
					OpStatus::Ignore(current_frame_bitmap->AddLine(data, line + i)); // FIXME:OOM-kilsmo
				}
				rep->AnalyzeData(data, line + i);
			}
		}

		scanlines_added += lineHeight;

		if (lineHeight > 0 && frame_nr == 1)
		{
			rep->LineAdded(line + i);
		}
	}
}

BOOL ImageLoader::OnInitMainFrame(INT32 width, INT32 height)
{
	OP_NEW_DBG("ImageLoader::OnInitMainFrame", "imageloadbug");
	OP_DBG(("ImageRep: %p, width: %d, height: %d", rep, width, height));

	OP_ASSERT(width > 0);
	OP_ASSERT(height > 0);
	if (OpStatus::IsSuccess(load_status))
	{
		load_status = rep->GotMainFrame(width, height);
	}
	return TRUE;
}

int ImageLoader::GetNrOfBits(UINT32 nr)
{
	int count = 0;
	while (nr >= 1)
	{
		count++;
		nr /= 2;
	}
	return count;
}

BOOL ImageLoader::IsSizeTooLarge(UINT32 width, UINT32 height)
{
	if (width < 16384 && height < 16384)
	{
		return FALSE;
	}
	return GetNrOfBits(width) + GetNrOfBits(height) > 30;
}

void ImageLoader::OnNewFrame(const ImageFrameData& image_frame_data)
{
	OP_NEW_DBG("ImageLoader::OnNewFrame", "imageloadbug");
	OP_DBG(("ImageRep: %p", rep));

	scanlines_added = 0;

	if (OpStatus::IsSuccess(load_status))
	{
		if (IsSizeTooLarge(image_frame_data.rect.width, image_frame_data.rect.height))
		{
			load_status = OpStatus::ERR_NO_MEMORY;
			return;
		}
		if (frame_nr == 0)
		{
			OpBitmap* bmp;
			load_status = OpBitmap::Create(&bmp,
											image_frame_data.rect.width,
											image_frame_data.rect.height,
											image_frame_data.transparent,
											image_frame_data.alpha,
											image_frame_data.transparent_index,
											image_frame_data.pal != NULL ? get_nr_of_bits(image_frame_data.num_colors) : 0);
			if (OpStatus::IsError(load_status))
			{
				return;
			}
			// The bitmap is owned by the image content, which is currently not an animated image content
			current_frame_bitmap = OP_NEW(ImageFrameBitmap, (bmp, FALSE));
			if (!current_frame_bitmap)
			{
				OP_DELETE(bmp);
				load_status = OpStatus::ERR_NO_MEMORY;
				return;
			}
			rep->IncMemUsed(image_frame_data.rect.width,
							image_frame_data.rect.height,
							image_frame_data.transparent,
							image_frame_data.alpha,
							image_frame_data.pal != NULL,
							FALSE);
#ifdef SUPPORT_INDEXED_OPBITMAP
			if (image_frame_data.pal && bmp->Supports(OpBitmap::SUPPORTS_INDEXED))
			{
				bmp->SetPalette(image_frame_data.pal, image_frame_data.num_colors);
			}
#endif // SUPPORT_INDEXED_OPBITMAP
			load_status = rep->AddFirstFrame(bmp,
											 image_frame_data.rect,
											 image_frame_data.transparent,
											 image_frame_data.alpha,
											 image_frame_data.interlaced,
											 image_frame_data.bits_per_pixel,
											 image_frame_data.bottom_to_top);
			if (OpStatus::IsError(load_status))
			{
				OP_DELETE(bmp);
				OP_DELETE(current_frame_bitmap);
				current_frame_bitmap = NULL;
				return;
			}
		}
		else if (frame_nr == 1)
		{
			// will not delete the Opbitmap
			OP_DELETE(current_frame_bitmap);
			current_frame_bitmap = NULL;
			rect = image_frame_data.rect;
#ifdef ASYNC_IMAGE_DECODERS
			load_status = g_factory->CreateOpBitmap(&current_bitmap,
													image_frame_data.rect.width,
													image_frame_data.rect.height,
													image_frame_data.transparent,
													image_frame_data.alpha,
													image_frame_data.transparent_index,
													image_frame_data.pal != NULL ? get_nr_of_bits(image_frame_data.num_colors) : 0);
			if (OpStatus::IsError(load_status))
			{
				return;
			}
			rep->IncMemUsed(image_frame_data.rect.width,
							image_frame_data.rect.height,
							image_frame_data.transparent,
							image_frame_data.alpha,
							image_frame_data.pal != NULL,
							FALSE);
#else
#ifdef SUPPORT_INDEXED_OPBITMAP
			if (image_frame_data.pal)
			{
				UINT8* frameData = OP_NEWA(UINT8, image_frame_data.rect.width*image_frame_data.rect.height);
				if (frameData)
				{
					current_frame_bitmap = OP_NEW(ImageFrameBitmap, (frameData, image_frame_data.rect.width, image_frame_data.rect.height,
						image_frame_data.transparent, image_frame_data.alpha, image_frame_data.transparent_index));
					if (!current_frame_bitmap)
						OP_DELETEA(frameData);
					else if (OpStatus::IsError(current_frame_bitmap->SetPalette(image_frame_data.pal, image_frame_data.num_colors)))
					{
						OP_DELETE(current_frame_bitmap);
						current_frame_bitmap = NULL;
					}
				}
			}
			else
#endif // SUPPORT_INDEXED_OPBITMAP
			{
				UINT32* frameData = OP_NEWA(UINT32, image_frame_data.rect.width*image_frame_data.rect.height);
				if (frameData)
				{
					current_frame_bitmap = OP_NEW(ImageFrameBitmap, (frameData, image_frame_data.rect.width, image_frame_data.rect.height,
						image_frame_data.transparent, image_frame_data.alpha, image_frame_data.transparent_index));
					if (!current_frame_bitmap)
						OP_DELETEA(frameData);
				}
			}

			rep->IncMemUsed(image_frame_data.rect.width,
							image_frame_data.rect.height,
							image_frame_data.transparent,
							image_frame_data.alpha,
							image_frame_data.pal != NULL,
							TRUE);

			if (current_frame_bitmap == NULL)
			{
				load_status = OpStatus::ERR_NO_MEMORY;
				return;
			}
#endif // ASYNC_IMAGE_DECODERS
			load_status = rep->AddAnimationInfo(nr_of_repeats, this->disposal_method, this->duration, this->dont_blend_prev, image_frame_data.pal_is_shared);
			if (OpStatus::IsError(load_status))
			{
				OP_DELETE(current_frame_bitmap);
				current_frame_bitmap = NULL;
				return;
			}
		}
		else
		{
			load_status = rep->AddAnimationFrame(current_frame_bitmap, rect, is_transparent, bits_per_pixel,
												 this->disposal_method, this->duration, this->dont_blend_prev);
			if (OpStatus::IsError(load_status))
			{
				OP_DELETE(current_frame_bitmap);
				current_frame_bitmap = NULL;
				return;
			}
			rect = image_frame_data.rect;
#ifdef ASYNC_IMAGE_DECODERS // FIXME:IMG-KILSMO no inc mem used here?
			load_status = g_factory->CreateOpBitmap(&current_bitmap,
													image_frame_data.rect.width,
													image_frame_data.rect.height,
													image_frame_data.transparent,
													image_frame_data.alpha,
													image_frame_data.transparent_index,
													image_frame_data.pal != NULL ? get_nr_of_bits(image_frame_data.num_colors) : 0);
			if (OpStatus::IsError(load_status))
			{
				return;
			}
#else
			current_frame_bitmap = NULL;
#ifdef SUPPORT_INDEXED_OPBITMAP
			if (image_frame_data.pal)
			{
				UINT8* frameData = OP_NEWA(UINT8, image_frame_data.rect.width*image_frame_data.rect.height);
				if (frameData)
				{
					current_frame_bitmap = OP_NEW(ImageFrameBitmap, (frameData, image_frame_data.rect.width, image_frame_data.rect.height,
						image_frame_data.transparent, image_frame_data.alpha, image_frame_data.transparent_index));
					if (!current_frame_bitmap)
						OP_DELETEA(frameData);
					else if (OpStatus::IsError(current_frame_bitmap->SetPalette(image_frame_data.pal, image_frame_data.num_colors)))
					{
						OP_DELETE(current_frame_bitmap);
						current_frame_bitmap = NULL;
					}
				}
			}
			else
#endif // SUPPORT_INDEXED_OPBITMAP
			{
				UINT32* frameData = OP_NEWA(UINT32, image_frame_data.rect.width*image_frame_data.rect.height);
				if (frameData)
				{
					current_frame_bitmap = OP_NEW(ImageFrameBitmap, (frameData, image_frame_data.rect.width, image_frame_data.rect.height,
						image_frame_data.transparent, image_frame_data.alpha, image_frame_data.transparent_index));
					if (!current_frame_bitmap)
						OP_DELETEA(frameData);
				}
			}


			if (current_frame_bitmap == NULL)
			{
				load_status = OpStatus::ERR_NO_MEMORY;
				return;
			}
#endif // ASYNC_IMAGE_DECODERS
		}
		disposal_method = image_frame_data.disposal_method;
		duration = image_frame_data.duration;
		dont_blend_prev = image_frame_data.dont_blend_prev;
		frame_nr++;
	}
	is_current_indexed = (image_frame_data.pal != NULL);
}

void ImageLoader::OnAnimationInfo(INT32 nrOfRepeats)
{
	nr_of_repeats = nrOfRepeats;
}

void ImageLoader::OnDecodingFinished()
{
	OP_NEW_DBG("ImageLoader::OnDecodingFinished", "imageloadbug");
	OP_DBG(("ImageRep: %p", rep));
	if (OpStatus::IsSuccess(load_status))
	{
		if (frame_nr > 1)
		{
			load_status = rep->AddAnimationFrame(current_frame_bitmap, rect, is_transparent, bits_per_pixel,
												 disposal_method, duration, dont_blend_prev);
			if (OpStatus::IsError(load_status))
			{
				return;
			}
		}
 		else
 			OP_DELETE(current_frame_bitmap);
		current_frame_bitmap = NULL;
		rep->DecodingFinished();
	}
#ifdef ASYNC_IMAGE_DECODERS
	rep->ReportMoreData();
	((ImageManagerImp*)imgManager)->AddToAsyncDestroyList(rep);
	((ImageManagerImp*)imgManager)->FreeUnusedAsyncDecoders();
#endif // ASYNC_IMAGE_DECODERS
}

void ImageLoader::ReportProgress()
{
	rep->ReportMoreData();
}

void ImageLoader::ReportFailure(OP_STATUS reason)
{
	if (OpStatus::IsMemoryError(reason))
	{
		rep->SetOOM();
	}
	else
	{
		rep->SetLoadingFailed();
	}
	rep->ReportError();
}

#ifdef IMAGE_METADATA_SUPPORT
void ImageLoader::OnMetaData(ImageMetaData id, const char* data)
{
	// FIXME: OOM
	OpStatus::Ignore(rep->SetMetaData(id, data));
}
#endif // IMAGE_METADATA_SUPPORT

#ifdef EMBEDDED_ICC_SUPPORT
void ImageLoader::OnICCProfileData(const UINT8* data, unsigned datalen)
{
	if (!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::ImagesUseICC))
		return;

	ICCProfile* image_profile;
	if (OpStatus::IsSuccess(g_color_manager->CreateProfile(&image_profile, data, datalen)))
	{
		OpStatus::Ignore(g_color_manager->CreateTransform(&color_transform, image_profile));

		OP_DELETE(image_profile);
	}
}
#endif // EMBEDDED_ICC_SUPPORT

#ifdef ASYNC_IMAGE_DECODERS

OpBitmap* ImageLoader::GetCurrentBitmap()
{
	return current_bitmap;
}

void ImageLoader::OnPortionDecoded()
{
	rep->ReportMoreData();
}

void ImageLoader::OnDecodingFailed(OP_STATUS reason)
{
	if (OpStatus::IsMemoryError(reason))
	{
		rep->SetOOM();
	}
	else
	{
		rep->SetLoadingFailed();
	}
	((ImageManagerImp*)imgManager)->AddToAsyncDestroyList(rep);
	((ImageManagerImp*)imgManager)->FreeUnusedAsyncDecoders();
}

INT32 ImageLoader::GetContentType()
{
	return content_provider->ContentType();
}

INT32 ImageLoader::GetContentSize()
{
	return content_provider->ContentSize();
}

#endif // ASYNC_IMAGE_DECODERS

OP_STATUS ImageLoader::OnMoreData(ImageContentProvider* content_provider, BOOL load_all)
{
	OP_NEW_DBG("ImageLoader::OnMoreData", "imageloadbug");
	if (OpStatus::IsSuccess(load_status))
	{
		OP_DBG(("Status ok"));
		const char* data;
		INT32 data_len;
		BOOL more;
		OP_STATUS ret_val_decode = OpStatus::OK;
		int resendBytes = 0;
		OpStatus::Ignore(ret_val_decode);
		INT32 old_data_len = 0;
		while (TRUE)
		{
			OP_STATUS ret_val = content_provider->GetData(data, data_len, more);
#ifdef ASYNC_IMAGE_DECODERS
			// we have to do this here to not miss out on OnLoad()
			if(!more)
				rep->SetDataLoaded();
#endif			
			if (OpStatus::IsSuccess(ret_val))
			{
				OP_DBG(("GetData ok"));
				if (data_len > 0)
				{
					if (data_len == old_data_len)
					{
						// We couldn't grow, even if we thought we could.
						return OpStatus::OK;
					}
					OP_DBG(("Call DecodeData with len %d", data_len));
					BOOL more_data = more || !content_provider->IsLoaded();
					ret_val_decode = image_decoder->DecodeData((unsigned char*)data, data_len, more_data, resendBytes, load_all);
					rep->ReportMoreData();
					if (OpStatus::IsError(ret_val_decode))
					{
						OP_DBG(("DecodeData failed"));
						if(ret_val_decode != OpStatus::ERR_NO_MEMORY && current_frame_bitmap && scanlines_added == current_frame_bitmap->Height())
							OnDecodingFinished();
						load_status = ret_val_decode;
						return load_status;
					}
					if (!more_data)
					{
						content_provider->ConsumeData(data_len);
					}
					else
					{
						content_provider->ConsumeData(data_len - resendBytes);
					}
				}
				if( content_provider->IsLoaded() && !more )
				{
					OP_DBG(("Call SetDataLoaded"));
					rep->SetDataLoaded();
#ifdef ASYNC_IMAGE_DECODERS
					int resendBytesAsync;
					image_decoder->DecodeData(NULL, 0, FALSE, resendBytesAsync);
#endif // ASYNC_IMAGE_DECODERS
				}
				if (more && data_len > 0 && data_len == resendBytes)
				{
					old_data_len = data_len;
					RETURN_IF_ERROR(content_provider->Grow());
					continue;
				}
			}
			else if (OpStatus::IsMemoryError(ret_val) || !more)
			{
				load_status = ret_val;
			}
			else
			{
				OP_DBG(("No data yet, silent wait"));
			}
			// Else, OpStatus::ERR, we haven't got any data yet, silent wait.
			break;
		}
	}
	return load_status;
}

////////////////////////////////////////////////////////////////////////////////
// ImageRep ////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

ImageRep* ImageRep::Create(OpBitmap* bitmap)
{
	OP_ASSERT(bitmap != NULL);

	ImageRep* rep = OP_NEW(ImageRep, (TRUE, NULL));
	if (rep != NULL)
	{
		rep->image_content = OP_NEW(BitmapImageContent, (bitmap));
		if (rep->image_content == NULL)
		{
			OP_DELETE(rep);
			return NULL;
		}
	}
	return rep;
}

OP_STATUS ImageRep::ReplaceBitmap(OpBitmap* new_bitmap)
{
	BOOL create_new = (image_content->Type() == NULL_IMAGE_CONTENT || image_content->Type() == EMPTY_IMAGE_CONTENT);
	if(create_new)
	{
		StaticImageContent* bitmap_content = OP_NEW(StaticImageContent, (new_bitmap, 
																		new_bitmap->Width(), 
																		new_bitmap->Height(), 
																		OpRect(0,0,new_bitmap->Width(),new_bitmap->Height()),
																		new_bitmap->IsTransparent(), 
																		new_bitmap->HasAlpha(), 
																		FALSE, 
																		new_bitmap->GetBpp(),
																		FALSE));
		if(!bitmap_content)
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		SetTypeKnown(); // We know the type now, otherwise IsFailed will return false
		bitmap_content->LineAdded(new_bitmap->Height()); // Update last-decoded-line

		if(image_content->Type() == EMPTY_IMAGE_CONTENT)
		{
			OP_DELETE(image_content);
		}

		image_content = bitmap_content;
	}
	else
	{
		image_content->ReplaceBitmap(new_bitmap);
	}

	return OpStatus::OK;
}


ImageRep* ImageRep::Create(ImageContentProvider* content_provider)
{
	ImageRep* rep = OP_NEW(ImageRep, (FALSE, content_provider));
	if (rep != NULL)
	{

		rep->image_content = ((ImageManagerImp*)imgManager)->GetNullImageContent();
		if (OpStatus::IsMemoryError(rep->PeekImageDimension()))
		{
			OP_DELETE(rep);
			rep = NULL;
		}
	}
	return rep;
}


#ifdef HAS_ATVEF_SUPPORT
ImageRep* ImageRep::CreateAtvefImage(ImageManager *imageManager)
{
 	ImageRep* rep = OP_NEW(ImageRep, (TRUE,NULL));
 	if (rep != NULL)
 	{
 		rep->SetAtvefImage();
 		rep->SetLoaded();
 		rep->image_content = ((ImageManagerImp *)imageManager)->GetNullImageContent();
 	}
 	return rep;
}
#endif

ImageRep::ImageRep(BOOL loaded, ImageContentProvider* content_provider) : content_provider(content_provider),
																		  image_content(NULL),
																		  image_loader(NULL),
																		  flags(0),
																		  ref_count(0),
																		  lock_count(0),
																		  mem_used(0)
#ifdef IMAGE_METADATA_SUPPORT
																		  , metadata(NULL)
#endif // IMAGE_METADATA_SUPPORT
#ifdef CACHE_UNUSED_IMAGES
																		  , cached_unused_image(FALSE)
#endif // CACHE_UNUSED_IMAGES
#ifdef IMG_TIME_LIMITED_CACHE
																		  , last_used(0)
#endif // IMG_TIME_LIMITED_CACHE
																		  , image_color(0)
																		  , image_description(IMAGE_CONTENT_DESC_NOT_KNOWN)
#ifdef ENABLE_MEMORY_DEBUGGING
																		  , m_lock_leaks(NULL)
																		  , m_vis_leaks(NULL)
#endif // ENABLE_MEMORY_DEBUGGING
{
	if (loaded)
	{
		flags = IMAGE_REP_FLAG_LOADED;
	}
}

ImageRep::~ImageRep()
{
#ifdef CACHE_UNUSED_IMAGES
	OP_ASSERT(!cached_unused_image);
#endif // CACHE_UNUSED_IMAGES
#ifdef IMAGE_METADATA_SUPPORT
	if (metadata)
	{
		for (int i = 0; i < IMAGE_METADATA_COUNT; ++i)
		{
			op_free(metadata[i]);
		}
		OP_DELETEA(metadata);
	}
#endif // IMAGE_METADATA_SUPPORT
	// If this assert triggers there is an unbalanced IncVisible/DecVisible somehwhere.
	OP_ASSERT(listener_list.Empty());
}

OpBitmap* ImageRep::GetBitmap(ImageListener* image_listener)
{
	OP_ASSERT(!IsAtvefImage());
	OpBitmap* bitmap = image_content->GetBitmap(image_listener);
	if (bitmap != NULL)
	{
		IncLockCount();
	}
	return bitmap;
}

void ImageRep::OnMoreData(ImageContentProvider* content_provider, BOOL load_all)
{
	OP_NEW_DBG("ImageRep::OnMoreData", "imageloadbug");
	OP_DBG(("ImageRep: %p", this));
	OP_ASSERT(!IsAtvefImage());
	if (listener_list.Empty() && !IsPredecoding())
	{
		OP_DBG(("Calling PeekImageDimension"));
		if (OpStatus::IsMemoryError(PeekImageDimension()))
			SetOOM();
	}
	else
	{
		if (!IsLoaded() && !IsFailed())
		{
			OP_DBG(("!IsLoaded && !IsFailed"));
			if (!IsTypeKnown())
			{
				OP_DBG(("Calling PeekImageDimenstion"));
				OP_BOOLEAN ret_val = PeekImageDimension();
				if (ret_val != OpBoolean::IS_TRUE)
				{
					OP_DBG(("Failing PeekImageDimenstion"));
					if (OpStatus::IsMemoryError(ret_val))
						SetOOM();
					return;
				} // FIXME:IMG
			}
			if (IsTypeFailed())
			{
				OP_DBG(("Type is failed"));
				return;
			}
			if (image_loader == NULL)
			{
				OP_DBG(("Create image loader"));
				// Make sure the content provider is at the 
				// start of the stream when creating the 
				// image loader. Seems to fix bug #258148
				content_provider->Rewind();
				image_loader = ImageLoader::Create(this, content_provider);
				if (image_loader == NULL)
				{
					SetOOM();
					return;
				}
			}
			IncLockCount();
			while (TRUE)
			{
				OP_DBG(("Calling image_loader->OnMoreData"));
				OP_STATUS ret_val = image_loader->OnMoreData(content_provider, load_all);
				if (OpStatus::IsError(ret_val))
				{
					OP_DBG(("image_loader->OnMoreData failed"));
					if (OpStatus::IsMemoryError(ret_val))
					{
						SetOOM();
					}
					else
					{
						SetLoadingFailed();
					}
					OP_DELETE(image_loader);
					image_loader = NULL;
				}
				else
				{ 
					if (IsLoaded())
					{
						OP_DBG(("IsLoaded, delete image_loader"));
						OP_DELETE(image_loader);
						image_loader = NULL;
					}
					else
					{
						if (IsDataLoaded())
						{
							OP_DBG(("IsDataLoader, calling ImageManagerImp::RemoveLoadedImage"));
							((ImageManagerImp*)imgManager)->RemoveLoadedImage(this);
						}
						else
						{
							OP_DBG(("!IsDataLoaded"));
							if (!load_all && content_provider->IsLoaded())
							{
								OP_DBG(("content_provider->IsLoaded"));
								OP_STATUS ret_val = ((ImageManagerImp*)imgManager)->AddLoadedImage(this);
								if (OpStatus::IsError(ret_val))
								{
									SetOOM();
									OP_DELETE(image_loader);
									image_loader = NULL;
								}
							}
							else if (load_all && content_provider->IsLoaded())
							{
								// load_all should not load all frames, only the first one.
								// When it IsAnimated, we have reached the second frame.
								const char* data;
								INT32 data_len;
								BOOL more;
								OP_STATUS ret_val_loc = content_provider->GetData(data, data_len, more);
								if (OpStatus::IsSuccess(ret_val_loc) && data_len > 0)
								{
									if (!IsAnimated())
										continue;
								}
							}
						}
					}
				}
				break;
			}
			DecLockCount();
		}
	}
}

OP_STATUS ImageRep::IncVisible(ImageListener* image_listener)
{
	// Reset predecoded state.
	flags &= ~IMAGE_REP_FLAG_PREDECODING;

	OP_ASSERT(!IsAtvefImage());
	ImageListenerElm* elm = OP_NEW(ImageListenerElm, ());
	if (elm == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	elm->listener = image_listener;
	if (IsAnimated())
	{
		AnimatedImageContent* animated_image = (AnimatedImageContent*)image_content;
		OP_STATUS ret_val = animated_image->IncVisible(image_listener);
		if (OpStatus::IsError(ret_val))
		{
			OP_DELETE(elm);
			return ret_val;
		}
	}
	BOOL was_empty = listener_list.Empty();
	elm->Into(&listener_list);
	if (was_empty)
	{
		OP_STATUS ret_val = MadeVisible();
		if (OpStatus::IsError(ret_val))
		{
			if (IsAnimated())
			{
				AnimatedImageContent* animated_image = (AnimatedImageContent*)image_content;
				animated_image->DecVisible(image_listener);
			}
			elm->Out();
			OP_DELETE(elm);
			OP_DELETE(image_loader);
			image_loader = NULL;
			return ret_val;
		}
		((ImageManagerImp*)imgManager)->ImageRepMoveToRightList(this);
	}
#ifdef ENABLE_MEMORY_DEBUGGING
	ImageRepVisLeak* leak = OP_NEW(ImageRepVisLeak,());
	if (leak)
	{
		leak->next = m_vis_leaks;
		m_vis_leaks = leak;
	}
#endif // ENABLE_MEMORY_DEBUGGING
	return OpStatus::OK;
}

void ImageRep::DecVisible(ImageListener* image_listener)
{
	OP_ASSERT(!IsAtvefImage());
	ImageListenerElm* elm = GetImageListenerElm(image_listener);
	if (elm != NULL)
	{
		elm->Out();
		OP_DELETE(elm);

		if (IsAnimated())
		{
			AnimatedImageContent* animated_image = (AnimatedImageContent*)image_content;
			animated_image->DecVisible(image_listener);
		}
	}

	if (listener_list.Empty())
	{
		ImageManagerImp* imageManager = static_cast<ImageManagerImp*>(imgManager);

		OP_ASSERT(!m_grace_time.InList());
		Head* slot = imageManager->CurrentGraceTimeSlot();
		if (slot && !m_grace_time.InList())
			m_grace_time.Into(slot);

		imageManager->RemoveLoadedImage(this);
#ifdef ASYNC_IMAGE_DECODERS
		imageManager->RemoveAsyncDestroyImage(this);
#endif // ASYNC_IMAGE_DECODERS
		OP_DELETE(image_loader);
		image_loader = NULL;
		if (!IsLoaded())
		{
			Clear();
		}
		imageManager->ImageRepMoveToRightList(this);
#ifdef ENABLE_MEMORY_DEBUGGING
		while (m_vis_leaks)
		{
			ImageRepVisLeak* leak = m_vis_leaks;
			m_vis_leaks = m_vis_leaks->next;
			OP_DELETE(leak);
		}
#endif // ENABLE_MEMORY_DEBUGGING
		if (imageManager->MoreToFree())
			imageManager->FreeMemory();
	}
}

void ImageRep::PreDecode(ImageListener* image_listener)
{
#ifdef IMAGE_TURBO_MODE
	ImageManagerImp* imageManager = (ImageManagerImp*)imgManager;
	if (imageManager->MoreToFree())
		return;

	if (!IsAtvefImage() && !IsPredecoding() && !IsLoaded() && !IsFailed())
	{
		// don't pre-decode images that are too big to be kept in
		// cache. pre-decoded images are not marked as being visible,
		// so they may be thrown out of the cache at any time.
		if (!imageManager->CanHold(mem_used + GetFrameCount() ? 0 : GetMemFor(Width(), Height(), TRUE, TRUE, FALSE, FALSE)))
			return;

		SetPredecoding();
		OP_STATUS ret_val = MadeVisible();
		if (OpStatus::IsError(ret_val))
		{
			OP_DELETE(image_loader);
			image_loader = NULL;
			return; // FIXME: OOM? Report this to memman or not? A predecoding image doesn't matter at all.
		}
	}
#endif // IMAGE_TURBO_MODE
}

OP_STATUS ImageRep::MadeVisible()
{
	OP_NEW_DBG("ImageRep::MadeVisible", "imageloadbug");
	OP_ASSERT(!IsAtvefImage());

	m_grace_time.Out();

	if (!IsLoaded() && !IsFailed())
	{
        const char *data;
		INT32 data_len;
		BOOL more;
		OP_STATUS status = content_provider->GetData(data, data_len, more);
		if (OpStatus::IsSuccess(status) && data_len > 0)
		{
			return ((ImageManagerImp*)imgManager)->AddLoadedImage(this);
		}
		else if (OpStatus::IsMemoryError(status))
		{
			return status;
		}
	}
	return OpStatus::OK;
}

void ImageRep::ResetAnimation(ImageListener* image_listener)
{
	OP_ASSERT(!IsAtvefImage());
	IncLockCount();
	if (IsAnimated())
	{
		AnimatedImageContent* animated_image = (AnimatedImageContent*)image_content;
		animated_image->ResetAnimation(image_listener);
	}
	DecLockCount();
}

void ImageRep::Reset()
{
	m_grace_time.Out();

	if (IsAtvefImage()) return;
	((ImageManagerImp*)imgManager)->DecMemUsed(mem_used);
	mem_used = 0;
	((ImageManagerImp*)imgManager)->ImageRepMoveToRightList(this);
	OP_DELETE(image_loader);
	image_loader = NULL;
	flags = 0;
	if (image_content->Type() != NULL_IMAGE_CONTENT)
	{
		if (IsAnimated())
		{
			// Remove all references to the animated content before it is deleted. 
			// If the image is reloaded and still animated ImageRep::AddAnimationInfo 
			// will add the lsiteners again. 
			for (ImageListenerElm* lelm = (ImageListenerElm*)listener_list.First(); lelm; lelm = (ImageListenerElm*)lelm->Suc())
			{
				AnimatedImageContent* animated_image = (AnimatedImageContent*)image_content;
				animated_image->DecVisible(lelm->listener);
			}
		}
		OP_DELETE(image_content);
		image_content = ((ImageManagerImp*)imgManager)->GetNullImageContent();
	}
#ifdef CACHE_UNUSED_IMAGES
	SetCacheUnusedImage(FALSE);
#endif // CACHE_UNUSED_IMAGES
}

void ImageRep::Clear()
{
	m_grace_time.Out();

	((ImageManagerImp*)imgManager)->DecMemUsed(mem_used);
	mem_used = 0;
	((ImageManagerImp*)imgManager)->ImageRepMoveToRightList(this);
	OP_DELETE(image_loader);
	image_loader = NULL;
	if (image_content->Type() != NULL_IMAGE_CONTENT &&
		image_content->Type() != EMPTY_IMAGE_CONTENT)
	{
		INT32 width = image_content->Width();
		INT32 height = image_content->Height();
		ImgContent* empty_image_content = OP_NEW(EmptyImageContent, (width, height));
		OP_DELETE(image_content);
		if (empty_image_content != NULL)
		{
			image_content = empty_image_content;
		}
		else
		{
			image_content = ((ImageManagerImp*)imgManager)->GetNullImageContent();
		}
	}
	flags &= ~IMAGE_REP_FLAG_LOADED;
	flags &= ~IMAGE_REP_FLAG_OOM;
	flags &= ~IMAGE_REP_FLAG_FAILED_LOADING;
	flags &= ~IMAGE_REP_FLAG_DATA_LOADED;
	flags &= ~IMAGE_REP_FLAG_PREDECODING;
#ifdef CACHE_UNUSED_IMAGES
	SetCacheUnusedImage(FALSE);
#endif // CACHE_UNUSED_IMAGES
}

OP_BOOLEAN ImageRep::PeekImageDimension()
{
	OP_BOOLEAN ret = OpBoolean::IS_FALSE;
	OpStatus::Ignore(ret);

	if (IsAtvefImage() || IsTypeFailed() || IsSizeFailed())
	{
		return OpBoolean::IS_FALSE;
	}
	if (!image_loader && (image_content->Width() <= 0 || image_content->Height() <= 0))
	{
		while (TRUE)
		{
			const char* data;
			INT32 data_len;
			BOOL more;
			OP_STATUS ret_val = content_provider->GetData(data, data_len, more);
			OP_ASSERT(OpStatus::IsError(ret_val) || data != NULL || content_provider->IsLoaded());
			if (OpStatus::IsSuccess(ret_val) && data && data_len)
			{
				if (CheckImageType((unsigned char*)data, data_len))
				{
					SetTypeKnown();
					CheckSize((unsigned char*)data, data_len);
					if (!IsSizeFailed() &&
						(image_content->Width() <= 0 || image_content->Height() <= 0) &&
						more)
					{
						if (content_provider->IsLoaded() && OpStatus::IsSuccess(content_provider->Grow()))
							continue;
					}
					if (Width() > 0 && Height() > 0)
					{
						ret = OpBoolean::IS_TRUE;
					}
					else if (content_provider->IsLoaded())
						SetSizeFailed();
				}
				else if (content_provider->IsLoaded())
					SetTypeFailed();
			}
			else if (OpStatus::IsMemoryError(ret_val))
				ret = OpStatus::ERR_NO_MEMORY;
			else if (content_provider->IsLoaded())
				SetTypeFailed();
			break;
		}
		content_provider->Rewind();
	}
	return ret;
}

#ifdef IMG_GET_AVERAGE_COLOR
COLORREF ImageRep::GetAverageColor(ImageListener* image_listener)
{
	/* Note that this doesn't really work for animations. What we'd really like
	   to do here, is get the first frame of the animation. */

	// If this method causes performance problems, consider adding OpBitmap::GetAverageColor().

	COLORREF average = OP_RGB(0xff, 0xff, 0xff); // or should that be "transparent"?
	OpBitmap* bitmap = image_content->GetBitmap(image_listener);

	if (!bitmap)
		return average;

	UINT32 width = bitmap->Width();
	UINT32 height = bitmap->Height();

	if (!width || !height)
		return average;

	UINT32* buffer = OP_NEWA(UINT32, width);

	if (!buffer)
		return average; // FIXME: OOM

	unsigned long r_total = 0;
	unsigned long g_total = 0;
	unsigned long b_total = 0;

	for (UINT32 y = 0; y < height; y++)
	{
		unsigned long r_line_total = 0;
		unsigned long g_line_total = 0;
		unsigned long b_line_total = 0;

		if (!bitmap->GetLineData(buffer, y))
			break; // something failed. Fall back to "default" color.

		for (UINT32 x = 0; x < width; x++)
		{
			UINT32 pixel = buffer[x];

			r_line_total += (pixel & 0xff0000) >> 16;
			g_line_total += (pixel & 0xff00) >> 8;
			b_line_total += pixel & 0xff;
		}

		r_total += r_line_total / width;
		g_total += g_line_total / width;
		b_total += b_line_total / width;
	}

	OP_DELETEA(buffer);

	average = OP_RGB(r_total / height, g_total / height, b_total / height);

	return average;
}
#endif // IMG_GET_AVERAGE_COLOR

void ImageRep::CheckSize(UCHAR* data, INT32 len)
{
	OP_ASSERT(!IsAtvefImage());
	ImageDecoderFactory* factory = ((ImageManagerImp*)imgManager)->GetImageDecoderFactory(content_provider->ContentType());
	OP_ASSERT(factory != NULL);
	INT32 width, height;
	BOOL3 ret = factory->CheckSize(data, len, width, height);
	if (ret == YES)
	{
		ImgContent* empty_image_content = OP_NEW(EmptyImageContent, (width, height));
		if (empty_image_content != NULL)
		{
			image_content = empty_image_content;
		}
	}
	else if (ret == NO)
	{
		SetSizeFailed();
	}
}

BOOL ImageRep::CheckImageType(UCHAR* data, INT32 len)
{
	OP_ASSERT(!IsAtvefImage());

	if (content_provider->ContentType() == URL_UNDETERMINED_CONTENT)
		flags &= ~IMAGE_REP_FLAG_KNOWN_TYPE;

	if (IsTypeKnown())
	{
		return TRUE;
	}

	ImageDecoderFactory* factory = ((ImageManagerImp*)imgManager)->GetImageDecoderFactory(content_provider->ContentType());
	if (factory != NULL)
	{
		BOOL3 res = factory->CheckType(data, len);
		if (res == YES)
		{
			return TRUE;
		}
		else if (res == MAYBE)
		{
			return FALSE;
		}
	}

	INT32 type = ((ImageManagerImp*)imgManager)->CheckImageType(data, len);
	if (type == -1)
	{
		// ico decoding is required in some instances. 
		type = content_provider->CheckImageType(data, len);
	}
	if (type == -1)
	{
		SetTypeFailed();
	}
	else if (type > 0)
	{
		content_provider->SetContentType(type);
		return TRUE;
	}
	return FALSE;
}

OP_STATUS ImageRep::AddAnimationFrame(ImageFrameBitmap* bitmap, const OpRect& rect, BOOL transparent, INT32 bits_per_pixel,
									  DisposalMethod disposal_method, INT32 duration, BOOL dont_blend_prev)
{
	OP_ASSERT(!IsAtvefImage());
	OP_ASSERT(IsAnimated());
	AnimatedImageContent* animated_image_content = (AnimatedImageContent*)image_content;
	return animated_image_content->AddFrame(bitmap, rect, duration, disposal_method, dont_blend_prev);
}

void ImageRep::DecodingFinished()
{
	OP_ASSERT(!IsAtvefImage());
	OP_ASSERT(!IsLoaded() && !IsFailed());
	SetLoaded();
	if (IsAnimated())
	{
		AnimatedImageContent* anim_content = (AnimatedImageContent*) image_content;
		anim_content->SetDecoded();
	}
}

BOOL ImageRep::IsContentRelevant()
{
	if (IsAnimated() || !ImageLoaded())
		return TRUE;
	return image_description != IMAGE_CONTENT_DESC_ONECOLOR;
}

void ImageRep::AnalyzeData(void* data,  INT32 line)
{
	if (image_description == IMAGE_CONTENT_DESC_DIFFERENT || IsAnimated())
		return;

	UINT32 *data32 = (UINT32 *) data;

	if (image_description == IMAGE_CONTENT_DESC_NOT_KNOWN)
	{
		image_color = *data32;
		image_description = IMAGE_CONTENT_DESC_ONECOLOR;
	}

	int width = Width();
	for (int i = 0; i < width; i++)
	{
		if (data32[i] != image_color)
		{
			image_description = IMAGE_CONTENT_DESC_DIFFERENT;
			return;
		}
	}
}

void ImageRep::AnalyzeIndexedData(void* data,  INT32 line)
{
	if (image_description == IMAGE_CONTENT_DESC_DIFFERENT || IsAnimated())
		return;

	UINT8 *data8 = (UINT8 *) data;

	if (image_description == IMAGE_CONTENT_DESC_NOT_KNOWN)
	{
		image_color = *data8;
		image_description = IMAGE_CONTENT_DESC_ONECOLOR;
	}

	int width = Width();
	for (int i = 0; i < width; i++)
	{
		if (data8[i] != image_color)
		{
			image_description = IMAGE_CONTENT_DESC_DIFFERENT;
			return;
		}
	}
}

BOOL ImageRep::IsFailed()
{
	return (IsOOM() || IsTypeFailed() || IsSizeFailed() || IsLoadingFailed());
}

void ImageRep::ReportMoreData()
{
	OP_ASSERT(!IsAtvefImage());
	for (ImageListenerElm* elm = (ImageListenerElm*)listener_list.First(); elm != NULL; elm = (ImageListenerElm*)elm->Suc())
	{
		elm->listener->OnPortionDecoded();
	}
}

void ImageRep::ReportError()
{
	OP_ASSERT(!IsAtvefImage());

	OP_STATUS error_val = IsOOM() ? OpStatus::ERR_SOFT_NO_MEMORY : OpStatus::ERR;
	OpStatus::Ignore(error_val); // Needed to silence debugging OpStatus when no one is listening.

	for (ImageListenerElm* elm = (ImageListenerElm*)listener_list.First(); elm != NULL; elm = (ImageListenerElm*)elm->Suc())
	{
		elm->listener->OnError(error_val);
	}
}

void ImageRep::DecRefCount()
{
	OP_ASSERT(ref_count > 0);
	ref_count--;
	if (ref_count <= 0)
	{
		if (image_content->Type() != BITMAP_IMAGE_CONTENT)
		{
			((ImageManagerImp*)imgManager)->RemoveLoadedImage(this);
#ifdef ASYNC_IMAGE_DECODERS
			((ImageManagerImp*)imgManager)->RemoveAsyncDestroyImage(this);
#endif // ASYNC_IMAGE_DECODERS
		}
		Reset();
		((ImageManagerImp*)imgManager)->RemoveFromImageList(this);
		OP_DELETE(this);
	}
}

void ImageRep::IncLockCount()
{
	OP_ASSERT(!IsAtvefImage());
	lock_count++;
	if (lock_count == 1 && image_content->Type() != BITMAP_IMAGE_CONTENT)
	{
		((ImageManagerImp*)imgManager)->Touch(this);
	}
#ifdef ENABLE_MEMORY_DEBUGGING
	ImageRepLockLeak* leak = OP_NEW(ImageRepLockLeak,());
	if (leak)
	{
		leak->next = m_lock_leaks;
		m_lock_leaks = leak;
	}
#endif // ENABLE_MEMORY_DEBUGGING
}

void ImageRep::DecLockCount()
{
	OP_ASSERT(!IsAtvefImage());
	OP_ASSERT(lock_count > 0);
	lock_count--;
#ifdef ENABLE_MEMORY_DEBUGGING
	if (lock_count == 0)
	{
		while (m_lock_leaks)
		{
			ImageRepLockLeak* leak = m_lock_leaks;
			m_lock_leaks = m_lock_leaks->next;
			OP_DELETE(leak);
		}
	}
#endif // ENABLE_MEMORY_DEBUGGING
	if (lock_count == 0 && ((ImageManagerImp*)imgManager)->MoreToFree())
		((ImageManagerImp*)imgManager)->FreeMemory();
}

#ifdef CACHE_UNUSED_IMAGES
void ImageRep::SetCacheUnusedImage(BOOL cache)
{
	if (cache == cached_unused_image)
		return;

	if (!content_provider || !content_provider->IsUrlProvider())
	{
		OP_ASSERT(!cached_unused_image);
		return;
	}

	cached_unused_image = cache;

	// When an image is decoded the content provider's ref count is
	// increased to make sure it is not deleted when it is in the
	// cache.
	if (cached_unused_image)
		((UrlImageContentProvider*)content_provider)->IncRef();
	// When the image data is cleared, it is safe to delete it again.
	else
		((UrlImageContentProvider*)content_provider)->DecRef();
}
#endif // CACHE_UNUSED_IMAGES

UINT32 ImageRep::GetMemFor(INT32 width, INT32 height, BOOL transparent, BOOL alpha, INT32 indexed, BOOL use_memorybitmap)
{
	UINT32 memsize;
	if(use_memorybitmap)
	{
#ifdef ASYNC_IMAGE_DECODERS
		OP_ASSERT(FALSE);
#else
		memsize = width*height;
		if (!indexed)
			memsize *= 4;
		//memsize = MemoryOpBitmap::GetAllocationSize(width, height, transparent, alpha, indexed);
#endif // !ASYNC_IMAGE_DECODERS
	}
	else
	{
		memsize = g_op_screen_info->GetBitmapAllocationSize(width, height, transparent, alpha, indexed);
	}
	return memsize;
}

void ImageRep::IncMemUsed(INT32 width, INT32 height, BOOL transparent, BOOL alpha, INT32 indexed, BOOL use_memorybitmap)
{
	OP_ASSERT(!IsAtvefImage());
	const UINT32 memsize = GetMemFor(width, height, transparent, alpha, indexed, use_memorybitmap);

#ifdef CACHE_UNUSED_IMAGES
	// When an image is decoded it is entered in the cache. Increase the content provider's reference
	// count to make sure the image is kept in the cache even if there are no more references to it
	OP_ASSERT(mem_used || !cached_unused_image);

#ifdef IMG_TOGGLE_CACHE_UNUSED_IMAGES
	if (imgManager->IsCachingUnusedImages())
#endif // IMG_TOGGLE_CACHE_UNUSED_IMAGES
	if (!mem_used && content_provider)
		SetCacheUnusedImage(TRUE);
#endif // CACHE_UNUSED_IMAGES

	mem_used += memsize;
	((ImageManagerImp*)imgManager)->IncMemUsed(memsize);
	((ImageManagerImp*)imgManager)->ImageRepMoveToRightList(this);
}

#ifdef IMAGE_METADATA_SUPPORT
char* ImageRep::GetMetaData(ImageMetaData id)
{
	return metadata?metadata[id]:NULL;
}

OP_STATUS ImageRep::SetMetaData(ImageMetaData id, const char* data)
{
	if (!metadata)
	{
		metadata = OP_NEWA(char*, IMAGE_METADATA_COUNT);
		if (!metadata)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		for (int i = 0; i < IMAGE_METADATA_COUNT; ++i)
		{
			metadata[i] = NULL;
		}
	}
	op_free(metadata[id]);
	metadata[id] = op_strdup(data);
	if (!metadata[id])
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

BOOL ImageRep::HasMetaData()
{
	return metadata!=NULL;
}
#endif // IMAGE_METADATA_SUPPORT
