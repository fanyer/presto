/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef IMAGEREP_H
#define IMAGEREP_H

#include "modules/util/simset.h"

#include "modules/img/image.h"
#include "modules/img/src/imagecontent.h"
#ifdef EMBEDDED_ICC_SUPPORT
#include "modules/img/imagecolormanager.h"
#endif // EMBEDDED_ICC_SUPPORT

class ImgContent;
class OpBitmap;
class ImageListener;
class ImageContentProvider;
class ImageListenerElm;
class ImageRep;

class ImageLoader : public ImageDecoderListener
{
public:
	static ImageLoader* Create(ImageRep* rep, ImageContentProvider* content_provider);

	~ImageLoader();

	void OnLineDecoded(void* data, INT32 line, INT32 lineHeight);

	BOOL OnInitMainFrame(INT32 width, INT32 height);

	void OnNewFrame(const ImageFrameData& image_frame_data);

	void OnAnimationInfo(INT32 nrOfRepeats);
	void OnDecodingFinished();

	void ReportProgress();
	void ReportFailure(OP_STATUS reason);

#ifdef IMAGE_METADATA_SUPPORT
	void OnMetaData(ImageMetaData id, const char* data);
#endif // IMAGE_METADATA_SUPPORT
#ifdef EMBEDDED_ICC_SUPPORT
	void OnICCProfileData(const UINT8* data, unsigned datalen);
#endif // EMBEDDED_ICC_SUPPORT

#ifdef ASYNC_IMAGE_DECODERS
	OpBitmap* GetCurrentBitmap();
	void OnPortionDecoded();
	void OnDecodingFailed(OP_STATUS reason);
	INT32 GetContentType();
	INT32 GetContentSize();
#endif // ASYNC_IMAGE_DECODERS

	OP_STATUS OnMoreData(ImageContentProvider* content_provider, BOOL load_all);

private:
	ImageLoader(ImageRep* rep, ImageContentProvider* content_provider);
	int GetNrOfBits(UINT32 nr);
	BOOL IsSizeTooLarge(UINT32 width, UINT32 height);

	ImageRep* rep;
	ImageDecoder* image_decoder;
	ImageContentProvider* content_provider;
	ImageFrameBitmap* current_frame_bitmap;
#ifdef EMBEDDED_ICC_SUPPORT
	ImageColorTransform* color_transform;
#endif // EMBEDDED_ICC_SUPPORT
	BOOL is_current_indexed;
	OpRect rect;
	INT32 frame_nr;
	INT32 nr_of_repeats;
	OP_STATUS load_status;
	BOOL is_transparent;
	INT32 bits_per_pixel;
	DisposalMethod disposal_method;
	INT32 duration;
	BOOL dont_blend_prev;
	UINT32 scanlines_added;
};

enum
{
	IMAGE_REP_FLAG_LOADED = 0x01,
	IMAGE_REP_FLAG_KNOWN_TYPE = 0x02,
	IMAGE_REP_FLAG_OOM = 0x04,
	IMAGE_REP_FLAG_FAILED_TYPE = 0x08,
	IMAGE_REP_FLAG_FAILED_SIZE = 0x10,
	IMAGE_REP_FLAG_FAILED_LOADING = 0x20,
	IMAGE_REP_FLAG_ATVEF_IMAGE = 0x40,
	IMAGE_REP_FLAG_DATA_LOADED = 0x80,
	IMAGE_REP_FLAG_PREDECODING = 0x100
};

class ImageListenerElm : public Link
{
public:
	ImageListener* listener;
};

class ImageRep : public Link
{
public:

	static ImageRep* Create(OpBitmap* bitmap);
	static ImageRep* Create(ImageContentProvider* content_provider);
	static ImageRep* CreateAtvefImage(ImageManager* imageManager);

	~ImageRep();

	INT32 Width() {	return image_content->Width(); }
	INT32 Height() { return image_content->Height(); }
	BOOL IsTransparent() { return image_content->IsTransparent(); }
	BOOL IsInterlaced() { return image_content->IsInterlaced(); }
	BOOL IsContentRelevant();
	INT32 GetFrameCount() { return image_content->GetFrameCount(); }

	void SyncronizeAnimation(ImageListener* dest_listener, ImageListener* source_listener)
	{
		image_content->SyncronizeAnimation(dest_listener, source_listener);
	}

	INT32 GetBitsPerPixel() { return image_content->GetBitsPerPixel(); }
	
	// INLINE-CALLED-ONCE
	INT32 GetLastDecodedLine()
	{
#ifdef ASYNC_IMAGE_DECODERS
		return image_content->Height(); // For opaque images, this will not look good when we decode
										// just one bit, since the background color will not shine
										// through. pw told me that Symbian team wanted this solution
										// anyway, but a better solution is needed.
#else
		return image_content->GetLastDecodedLine();
#endif
	}
	
	OpPoint GetBitmapOffset() { return image_content->GetBitmapOffset(); }

	OpBitmap* GetBitmap(ImageListener* image_listener);

	void ReleaseBitmap()
	{
		OP_ASSERT(!IsAtvefImage());
		DecLockCount();
	}

	OpBitmap* GetTileBitmap(ImageListener* image_listener, int desired_width, int desired_height)
	{
		OP_ASSERT(!IsAtvefImage());
		OP_ASSERT(!lock_count);
		if (lock_count)
			return NULL;
		return image_content->GetTileBitmap(image_listener, desired_width, desired_height);
	}

	void ReleaseTileBitmap()
	{
		OP_ASSERT(!IsAtvefImage());
		image_content->ReleaseTileBitmap();
	}

	OpBitmap* GetEffectBitmap(INT32 effect, INT32 effect_value, ImageListener* image_listener = NULL)
	{
		OP_ASSERT(!IsAtvefImage());
		OpBitmap* bitmap = GetBitmap(image_listener);
		OpBitmap* effect_bitmap = image_content->GetEffectBitmap(bitmap, effect, effect_value, image_listener);
		return effect_bitmap ? effect_bitmap : bitmap;
	}

	// INLINE-CALLED-ONCE
	void ReleaseEffectBitmap()
	{
		OP_ASSERT(!IsAtvefImage());
		image_content->ReleaseEffectBitmap();
		ReleaseBitmap();
	}
	OpBitmap* GetTileEffectBitmap(INT32 effect, INT32 effect_value, int horizontal_count, int vertical_count)
	{
		OP_ASSERT(!IsAtvefImage());
		return image_content->GetTileEffectBitmap(effect, effect_value, horizontal_count, vertical_count);
	}

	void ReleaseTileEffectBitmap()
	{
		OP_ASSERT(!IsAtvefImage());
		image_content->ReleaseTileEffectBitmap();
	}

	void OnMoreData(ImageContentProvider* content_provider, BOOL load_all = FALSE);

	// INLINE-CALLED-ONCE
	void OnLoadAll(ImageContentProvider* content_provider)
	{
		OP_ASSERT(!IsAtvefImage());
		if (content_provider->IsLoaded())
		{
			OnMoreData(content_provider, TRUE);
			if (IsFailed())
			{
				ReportError();
			}
		}
	}

	OP_STATUS IncVisible(ImageListener* image_listener);

	void DecVisible(ImageListener* image_listener);

	void PreDecode(ImageListener* image_listener);

	INT32 CountListeners()
	{
		if (IsAtvefImage()) return 0;
		return listener_list.Cardinal();
	}

	BOOL ImageLoaded()
	{
		return IsLoaded();
	}

	BOOL IsAnimated()
	{
		return (image_content->Type() == ANIMATED_IMAGE_CONTENT);
	}

	// INLINE-CALLED-ONCE
	INT32 GetCurrentFrameDuration(ImageListener* image_listener)
	{
		OP_ASSERT(!IsAtvefImage());
		if (IsAnimated())
		{
			AnimatedImageContent* animated_image = (AnimatedImageContent*)image_content;
			return animated_image->GetCurrentFrameDuration(image_listener);
		}
		return 0;
	}

	// INLINE-CALLED-ONCE
	OpRect GetCurrentFrameRect(ImageListener* image_listener)
	{
		OP_ASSERT(!IsAtvefImage());
		OpRect rect(0, 0, 0, 0);
		if (IsAnimated())
		{
			AnimatedImageContent* animated_image = (AnimatedImageContent*)image_content;
			rect = animated_image->GetCurrentFrameRect(image_listener);
		}
		return rect;
	}

	// INLINE-CALLED-ONCE
	BOOL Animate(ImageListener* image_listener)
	{
		OP_ASSERT(!IsAtvefImage());
		BOOL status = FALSE;
		IncLockCount();
		if (IsAnimated())
		{
			AnimatedImageContent* animated_image = (AnimatedImageContent*)image_content;
			status = animated_image->Animate(image_listener);
		}
		DecLockCount();
		return status;
	}

	void ResetAnimation(ImageListener* image_listener);
	void Reset();
	void Clear();

	OP_STATUS ReplaceBitmap(OpBitmap* new_bitmap);

	// INLINE-CALLED-ONCE
	OP_STATUS AddFirstFrame(OpBitmap* bitmap, const OpRect& rect,
							BOOL transparent, BOOL alpha, BOOL interlaced,
							INT32 bits_per_pixel, BOOL bottom_to_top)
	{
		OP_ASSERT(!IsAtvefImage());
		OP_ASSERT(image_content->Type() == EMPTY_IMAGE_CONTENT);

		ImgContent* static_image_content = OP_NEW(StaticImageContent,
												  (bitmap,
												   image_content->Width(),
												   image_content->Height(),
												   rect, transparent, alpha,
												   interlaced,
												   bits_per_pixel,
												   bottom_to_top));
		if (static_image_content == NULL)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		if (image_content->Type() != NULL_IMAGE_CONTENT)
		{
			OP_DELETE(image_content);
		}
		image_content = static_image_content;
		return OpStatus::OK;
	}

	OP_STATUS AddAnimationFrame(ImageFrameBitmap* bitmap, const OpRect& rect, BOOL transparent, INT32 bits_per_pixel,
								DisposalMethod disposal_method, INT32 duration, BOOL dont_blend_prev);

	// INLINE-CALLED-ONCE
	OP_STATUS AddAnimationInfo(INT32 nr_of_repeats, DisposalMethod disposal_method, INT32 duration, BOOL dont_blend_prev, BOOL pal_is_shared)
	{
		OP_ASSERT(!IsAtvefImage());
		OP_ASSERT(image_content->Type() == STATIC_IMAGE_CONTENT);
		INT32 width = image_content->Width();
		INT32 height = image_content->Height();
		AnimatedImageContent* animated_image_content = OP_NEW(AnimatedImageContent, (width, height, nr_of_repeats, pal_is_shared));
		if (animated_image_content == NULL)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		StaticImageContent* static_image_content = (StaticImageContent*)image_content;
		StaticImageInfo image_info;
		static_image_content->GetStaticImageInfo(image_info);
		ImageFrameBitmap* frame_bitmap = OP_NEW(ImageFrameBitmap, (image_info.bitmap, TRUE));
		if (!frame_bitmap)
		{
			OP_DELETE(animated_image_content);
			return OpStatus::ERR_NO_MEMORY;
		}
		OP_STATUS ret_val = animated_image_content->AddFrame(frame_bitmap, image_info.rect, duration, disposal_method, dont_blend_prev);
		if (OpStatus::IsError(ret_val))
		{
			// Make sure the bitmap is not deleted as it is still owned by the static image content. Fix for bug #270970
			animated_image_content->ClearBitmapPointer(frame_bitmap);
			OP_DELETE(animated_image_content);
			frame_bitmap->ClearBitmapPointer();
			OP_DELETE(frame_bitmap);
			return ret_val;
		}
		for (ImageListenerElm* elm = (ImageListenerElm*)listener_list.First(); elm != NULL; elm = (ImageListenerElm*)elm->Suc())
		{
			OP_STATUS ret_val = animated_image_content->IncVisible(elm->listener);
			if (OpStatus::IsError(ret_val))
			{
				// Make sure the bitmap is not deleted as it is still owned by the static image content. Fix for bug #270970
				animated_image_content->ClearBitmapPointer(frame_bitmap);
				OP_DELETE(animated_image_content);
				frame_bitmap->ClearBitmapPointer();
				OP_DELETE(frame_bitmap);
				return ret_val;
			}
		}
		static_image_content->ClearBitmapPointer();
		OP_DELETE(image_content);
		image_content = animated_image_content;
		return OpStatus::OK;
	}

	void DecodingFinished();

	void AnalyzeData(void* data,  INT32 line);
	void AnalyzeIndexedData(void* data,  INT32 line);

	void LineAdded(INT32 line)
	{
		OP_ASSERT(!IsAtvefImage());
		OP_ASSERT(image_content->Type() == STATIC_IMAGE_CONTENT);
		StaticImageContent* static_image_content = (StaticImageContent*)image_content;
		static_image_content->LineAdded(line);
	}

	// INLINE-CALLED-ONCE
	OP_STATUS GotMainFrame(INT32 width, INT32 height)
	{
		OP_ASSERT(!IsAtvefImage());
		OP_ASSERT((image_content->Width() == 0 || image_content->Width() == width) &&
				  (image_content->Height() == 0 || image_content->Height() == height));

		if (image_content->Type() == NULL_IMAGE_CONTENT)
		{
			EmptyImageContent* content = OP_NEW(EmptyImageContent, (width, height));
			if (content == NULL)
			{
				return OpStatus::ERR_NO_MEMORY;
			}
			image_content = content;
		}
		return OpStatus::OK;
	}

	void ReportMoreData();
	void ReportError();

	BOOL IsLocked() { return (lock_count > 0); }

	BOOL IsVisible()
	{
		OP_ASSERT(!IsAtvefImage());
		return (!listener_list.Empty());
	}

	INT32 GetRefCount() { return ref_count; }

	ImageContentProvider* GetContentProvider() { return content_provider; }

	void IncRefCount() { ref_count++; }

	void DecRefCount();

	/**
	   calculates memory usage for a frame
	 */
	UINT32 GetMemFor(INT32 width, INT32 height, BOOL transparent, BOOL alpha, INT32 indexed, BOOL use_memorybitmap);
	void IncMemUsed(INT32 width, INT32 height, BOOL transparent, BOOL alpha, INT32 indexed, BOOL use_memorybitmap);
	
	BOOL IsAtvefImage() { return !!(flags & IMAGE_REP_FLAG_ATVEF_IMAGE); }

	void SetOOM() { flags |= IMAGE_REP_FLAG_OOM; }
	void SetLoadingFailed() { flags |= IMAGE_REP_FLAG_FAILED_LOADING; }

#ifdef ASYNC_IMAGE_DECODERS
	// INLINE-CALLED-ONCE
	void FreeAsyncDecoder()
	{
		OP_DELETE(image_loader);
		image_loader = NULL;
	}
#endif // ASYNC_IMAGE_DECODERS

	void SetDataLoaded() { flags |= IMAGE_REP_FLAG_DATA_LOADED; }
	BOOL IsDataLoaded() { return !!(flags & IMAGE_REP_FLAG_DATA_LOADED); }

	BOOL IsFailed();

	ImageContentProvider* GetImageContentProvider() { return content_provider; }

	BOOL IsPredecoding() { return !!(flags & IMAGE_REP_FLAG_PREDECODING); }

	BOOL IsOOM() { return !!(flags & IMAGE_REP_FLAG_OOM); }

	BOOL IsFreed() { return mem_used == 0; }

	OP_BOOLEAN PeekImageDimension();

	BOOL IsBottomToTop() { return image_content->IsBottomToTop(); }
	UINT32 GetLowestDecodedLine() { return image_content->GetLowestDecodedLine(); }

#ifdef IMG_GET_AVERAGE_COLOR
	COLORREF GetAverageColor(ImageListener* image_listener);
#endif // IMG_GET_AVERAGE_COLOR

#ifdef IMAGE_METADATA_SUPPORT
	char* GetMetaData(ImageMetaData id);
	OP_STATUS SetMetaData(ImageMetaData id, const char* data);
	BOOL HasMetaData();
#endif // IMAGE_METADATA_SUPPORT

#ifdef IMG_TIME_LIMITED_CACHE
	unsigned int GetLastUsed(){return last_used;}
	void UpdateLastUsed(unsigned int time){last_used = time;}
#endif // IMG_TIME_LIMITED_CACHE

#ifdef CACHE_UNUSED_IMAGES
	void SetCacheUnusedImage(BOOL cache);
	BOOL GetCachedUnusedImage() const { return cached_unused_image; }
#endif // CACHE_UNUSED_IMAGES

	/**
	   @return the grace time slot associated with the image, or 0 if
	   not in state of grace
	 */
	Head* GraceTimeSlot() { return m_grace_time.Slot(); }

private:
	ImageRep(BOOL loaded, ImageContentProvider* content_provider);

	// INLINE-CALLED-ONCE
	ImageListenerElm* GetImageListenerElm(ImageListener* listener)
	{
		OP_ASSERT(!IsAtvefImage());
		for (ImageListenerElm* elm = (ImageListenerElm*)listener_list.First(); elm != NULL; elm = (ImageListenerElm*)elm->Suc())
		{
			if (elm->listener == listener)
			{
				return elm;
			}
		}
		return NULL;
	}

	void CheckSize(UCHAR* data, INT32 len);

	BOOL CheckImageType(UCHAR* data, INT32 len);

	OP_STATUS MadeVisible();

	BOOL IsLoaded() { return !!(flags & IMAGE_REP_FLAG_LOADED); }
	BOOL IsTypeKnown() { return !!(flags & IMAGE_REP_FLAG_KNOWN_TYPE); }
	BOOL IsTypeFailed() { return !!(flags & IMAGE_REP_FLAG_FAILED_TYPE); }
	BOOL IsSizeFailed() { return !!(flags & IMAGE_REP_FLAG_FAILED_SIZE); }
	BOOL IsLoadingFailed() { return !!(flags & IMAGE_REP_FLAG_FAILED_LOADING); }

	void SetLoaded() { flags |= IMAGE_REP_FLAG_LOADED; }
	void SetTypeFailed() { flags |= IMAGE_REP_FLAG_FAILED_TYPE; }
	void SetSizeFailed() { flags |= IMAGE_REP_FLAG_FAILED_SIZE; }
	void SetTypeKnown() { flags |= IMAGE_REP_FLAG_KNOWN_TYPE; }
	void SetAtvefImage() { flags |= IMAGE_REP_FLAG_ATVEF_IMAGE; }
	void SetPredecoding() { flags |= IMAGE_REP_FLAG_PREDECODING; }
	
	void IncLockCount();

	void DecLockCount();

	Head listener_list;
	ImageContentProvider* content_provider;
	ImgContent* image_content;
	ImageLoader* image_loader;
	INT32 flags;
	INT32 ref_count;
	INT32 lock_count;
	INT32 mem_used;
#ifdef IMAGE_METADATA_SUPPORT
	char** metadata;
#endif // IMAGE_METADATA_SUPPORT
#ifdef CACHE_UNUSED_IMAGES
	BOOL cached_unused_image;
#endif // CACHE_UNUSED_IMAGES
#ifdef IMG_TIME_LIMITED_CACHE
	unsigned int last_used;
#endif // IMG_TIME_LIMITED_CACHE

	enum IMAGE_CONTENT_DESCRIPTION {
		IMAGE_CONTENT_DESC_NOT_KNOWN,
		IMAGE_CONTENT_DESC_DIFFERENT,
		IMAGE_CONTENT_DESC_ONECOLOR
	};

	UINT32 image_color;
	IMAGE_CONTENT_DESCRIPTION image_description;

#ifdef ENABLE_MEMORY_DEBUGGING
	struct ImageRepLockLeak
	{
		ImageRepLockLeak* next;
	};
	struct ImageRepVisLeak
	{
		ImageRepVisLeak* next;
	};
	ImageRepLockLeak* m_lock_leaks;
	ImageRepVisLeak* m_vis_leaks;
#endif // ENABLE_MEMORY_DEBUGGING

	class ImageGraceTime : public Link
	{
	public:
		Head* Slot() { return GetList(); }
	};
	ImageGraceTime m_grace_time;
};

#endif // !IMAGEREP_H
