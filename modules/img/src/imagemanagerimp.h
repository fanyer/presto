/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef IMAGEMANAGERIMP_H
#define IMAGEMANAGERIMP_H

#include "modules/img/image.h"

#include "modules/util/simset.h"
#include "modules/util/OpHashTable.h"

#include "modules/img/imagedecoderfactory.h"
#include "modules/img/src/imagerep.h"
#include "modules/img/src/imagecontent.h"

class LoadedImageElm;
class ImageRep;
class AnimatedImageContent;

#ifdef ASYNC_IMAGE_DECODERS_EMULATION
class AsyncEmulationDecoder;
#endif // ASYNC_IMAGE_DECODERS_EMULATION

#ifdef ASYNC_IMAGE_DECODERS
class AsyncImageDecoderFactory;

class AsyncImageElm : public Link
{
public:
	ImageRep* rep;
};

#endif // ASYNC_IMAGE_DECODERS

class DecoderFactoryElm : public Link
{
public:
	~DecoderFactoryElm() { OP_DELETE(factory); }

	ImageDecoderFactory* factory;
	INT32 type;
	BOOL check_header;
};

class LoadedImageElm : public Link
{
public:
	ImageRep* image_rep;
};

#if defined(USE_PREMULTIPLIED_ALPHA) && !defined(VEGA_OPPAINTER_SUPPORT) || defined(EMBEDDED_ICC_SUPPORT)
#define IMGMAN_USE_SCRATCH_BUFFER
#endif // USE_PREMULTIPLIED_ALPHA && !VEGA_OPPAINTER_SUPPORT || EMBEDDED_ICC_SUPPORT

class ImageManagerImp : public ImageManager
#ifdef IMG_TIME_LIMITED_CACHE
						, public MessageObject
#endif // IMG_TIME_LIMITED_CACHE
{
public:

	ImageManagerImp(ImageProgressListener* listener);
	~ImageManagerImp();

	OP_STATUS Create();

	/**
	 * In core interface
	 */

	void LockImageCache();
	void UnlockImageCache();
	void BeginGraceTime();
	void EndGraceTime();

	void FreeMemory();

	void SetCacheSize(INT32 cache_size, ImageCachePolicy cache_policy);

	OP_STATUS AddImageDecoderFactory(ImageDecoderFactory* factory, INT32 type, BOOL check_header);

#ifdef ASYNC_IMAGE_DECODERS
	void SetAsyncImageDecoderFactory(AsyncImageDecoderFactory* factory);
#endif // ASYNC_IMAGE_DECODERS

	Image GetImage(ImageContentProvider* content_provider);

	Image GetImage(OpBitmap* bitmap);

	void ResetImage(ImageContentProvider* content_provider);

	int CheckImageType(const unsigned char* data_buf, UINT32 buf_len);

	BOOL IsImage(int type);

	void Progress();

#ifdef ASYNC_IMAGE_DECODERS
	void FreeUnusedAsyncDecoders();
#endif // ASYNC_IMAGE_DECODERS

#ifdef HAS_ATVEF_SUPPORT
	Image GetAtvefImage();
#endif

#ifdef CACHE_UNUSED_IMAGES
	void FreeImagesForContext(URL_CONTEXT_ID id);
#endif // CACHE_UNUSED_IMAGES

#ifdef IMG_TOGGLE_CACHE_UNUSED_IMAGES
	void CacheUnusedImages(BOOL strategy);
	BOOL IsCachingUnusedImages() const { return is_caching_unused_images; }
#endif // IMG_TOGGLE_CACHE_UNUSED_IMAGES

	/**
	 * Not in core interface
	 */

	void Touch(ImageRep* imageRep);

#ifdef IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
	void TouchAnimated(AnimatedImageContent* animated_image_content);
#endif // IMG_CACHE_MULTIPLE_ANIMATION_FRAMES

	OP_STATUS AddLoadedImage(ImageRep* image_rep);

	void RemoveLoadedImage(ImageRep* image_rep);

	ImageDecoderFactory* GetImageDecoderFactory(int type);

#ifdef ASYNC_IMAGE_DECODERS
	/**
	 * INLINE-GETTER
	 */
	AsyncImageDecoderFactory* GetAsyncImageDecoderFactory() { return async_factory; }
#endif // ASYNC_IMAGE_DECODERS

	/**
	 * INLINE-ASSIGN-MODIFY-PROXY-FUNCTION
	 */
	void IncMemUsed(INT32 size)
	{
		used_mem += size;
		FreeMemory();
	}

	/**
	 * INLINE-ASSIGN-MODIFY
	 */
	void DecMemUsed(INT32 size)
	{
		used_mem -= size;
		OP_ASSERT(used_mem >= 0);
	}

	/**
	 * INLINE-CALLED-ONCE
	 */
	void IncAnimMemUsed(AnimatedImageContent* animated_image_content, INT32 size)
	{
#ifdef IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
		anim_used_mem += size;
		FreeAnimMemory(animated_image_content);
#else
		IncMemUsed(size);
#endif // IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
	}

	/**
	 * INLINE-PROXY-FUNCTION
	 * INLINE-ASSIGN-MODIFY
	 */
	void DecAnimMemUsed(INT32 size)
	{
#ifdef IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
		OP_ASSERT(anim_used_mem >= size);
		anim_used_mem -= size;
#else
		DecMemUsed(size);
#endif // IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
	}

#ifdef IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
	void RemoveAnimatedImage(AnimatedImageContent* animated_image_content) { animated_image_content->Out(); }
#endif // IMG_CACHE_MULTIPLE_ANIMATION_FRAMES

#ifdef ASYNC_IMAGE_DECODERS
	void AddToAsyncDestroyList(ImageRep* image_rep);
	void RemoveAsyncDestroyImage(ImageRep* image_rep);
#endif // ASYNC_IMAGE_DECODERS

	void RemoveFromImageList(ImageRep* image_rep);

	void ImageRepMoveToRightList(ImageRep* image_rep);

#ifdef ASYNC_IMAGE_DECODERS_EMULATION
	void AddBufferedImageDecoder(AsyncEmulationDecoder* decoder);
	void MoreBufferedData();
	void MoreDeletedDecoders();
#endif // ASYNC_IMAGE_DECODERS_EMULATION

	/**
	 * INLINE-COMPARE-VALUES
	 */
	BOOL MoreToFree() { return used_mem > cache_size; }
	/**
	   @param size the expected size of the image
	   @return TRUE if the cache can hold the image without purging, FALSE otherwise
	 */
	BOOL CanHold(INT32 size) { return used_mem + size <= cache_size; }

	ImgContent* GetNullImageContent() { return null_image_content; }

#ifdef IMG_TIME_LIMITED_CACHE
	void ScheduleCacheTimeout();
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
#endif // IMG_TIME_LIMITED_CACHE

	virtual INT32 GetCacheSize(){return cache_size;}
	virtual INT32 GetUsedCacheMem(){return used_mem;}
#ifdef IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
	virtual INT32 GetAnimCacheSize(){return anim_cache_size;}
	virtual INT32 GetUsedAnimCacheMem(){return anim_used_mem;}
#endif // IMG_CACHE_MULTIPLE_ANIMATION_FRAMES

#ifdef IMGMAN_USE_SCRATCH_BUFFER
	UINT32* GetScratchBuffer(unsigned int width);
#endif // IMGMAN_USE_SCRATCH_BUFFER

	/**
	   see documentation of BeginGraceTime
	   @return if locked and in grace time mode the currently active
	   grace time slot, otherwise 0.
	 */
	Head* CurrentGraceTimeSlot() { return m_grace_lock_count ? m_current_grace_time : NULL; }

private:
	/**
	   determines whether an image is in state of grace or not
	   @param rep the ImageRep to test
	   @param now the current time
	   @return TRUE if the image is in state of grace, false otherwise
	 */
	BOOL StateOfGrace(ImageRep* rep, double now) const;

	/**
	 * Get the image source with the url.
	 */
	ImageRep* GetImageRep(ImageContentProvider* content_provider);

	LoadedImageElm* GetLoadedImageElm(ImageRep* image_rep);

	/**
	 * INLINE-CALLED-ONCE
	 */
	ImageRep* AddToImageList(ImageContentProvider* content_provider)
	{
		ImageRep* rep = ImageRep::Create(content_provider);
		if (rep != NULL)
		{
			OP_STATUS ret_val = image_hash_table.Add(content_provider, rep);
			if (OpStatus::IsError(ret_val))
			{
				OP_DELETE(rep);
				return NULL;
			}
			ImageRepIntoImageList(rep);
		}
		return rep;
	}

#ifdef IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
	/*
	 * We do not want to inline this one, since it is only called
	 * from an inlined function.
	 */
	void FreeAnimMemory(AnimatedImageContent* animated_image_content);
#endif // IMG_CACHE_MULTIPLE_ANIMATION_FRAMES

	/**
	 * Posts a message to itself to know that we still have
	 * images that are loading, and that we should load one
	 * of them.
	 *
	 * INLINE-PROXY-FUNCTION
	 */
	void PostContinueLoading()
	{
		progress_listener->OnProgress();
	}

	BOOL IsFreeable(ImageRep* image_rep, double now);

	void ImageRepIntoImageList(ImageRep* image_rep);

	void ClearImageList(Head* list);

	/**
	 * INLINE-CALLED-ONCE
	 */
	void StopAllPredecodingImages()
	{
#ifdef IMAGE_TURBO_MODE
		LoadedImageElm* elm = (LoadedImageElm*)loaded_image_list.First();
		while(elm)
		{
			LoadedImageElm* nextelm = (LoadedImageElm*) elm->Suc();
			if (elm->image_rep->IsPredecoding())
			{
				elm->Out();
				OP_DELETE(elm);
			}
			elm = nextelm;
		}
#endif // IMAGE_TURBO_MODE
	}

#ifdef ASYNC_IMAGE_DECODERS
	/**
	 * INLINE-CALLED-ONCE
	 */
	AsyncImageElm* GetAsyncImageElm(ImageRep* image_rep)
	{
		for (AsyncImageElm* elm = (AsyncImageElm*)loaded_image_list.First();
			 elm != NULL;
			 elm = (AsyncImageElm*)elm->Suc())
		{
			if (elm->rep == image_rep)
			{
				return elm;
			}
		}

		return NULL;
	}
	AsyncImageElm* GetAsyncDestroyElm(ImageRep* image_rep);
#endif // ASYNC_IMAGE_DECODERS

	/**
	 * The list of images (ImageSource) that is in the system.
	 * The image itself can be empty and not use any memory to save
	 * the bitmap.
	 */
	Head image_list;
	Head visible_image_list;
	Head free_image_list;
	Head bitmap_image_list;

	Head loaded_image_list;

	Head image_decoder_factory_list;

#ifdef IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
	Head animated_image_list;
#endif // IMG_CACHE_MULTIPLE_ANIMATION_FRAMES

	ImageProgressListener* progress_listener;

	INT32 cache_size;
	INT32 used_mem;
#ifdef IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
	INT32 anim_cache_size;
	INT32 anim_used_mem;
#endif // IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
	ImageCachePolicy cache_policy;
	OpHashTable image_hash_table;

#ifdef ASYNC_IMAGE_DECODERS
	AsyncImageDecoderFactory* async_factory;
	Head destroyed_async_decoders;
#endif // ASYNC_IMAGE_DECODERS

#ifdef HAS_ATVEF_SUPPORT

	Image* atvef_image;

#endif

#ifdef ASYNC_IMAGE_DECODERS_EMULATION
	Head buffered_image_decoder_list;
#endif // ASYNC_IMAGE_DECODERS_EMULATION

	NullImageContent* null_image_content;
#ifdef IMG_TIME_LIMITED_CACHE
	BOOL pending_cache_timeout;
#endif // IMG_TIME_LIMITED_CACHE

#ifdef IMGMAN_USE_SCRATCH_BUFFER
	UINT32* scratch_buffer;
	unsigned int scratch_buffer_size;
#endif // IMGMAN_USE_SCRATCH_BUFFER

	UINT32 m_lock_count; ///< lock count for LockImageCache / UnlockImageCache
	UINT32 m_grace_lock_count; ///< lock count for BeginGraceTime / EndGraceTime
	/**
	   TRUE if a call to FreeMemory was made when locked - if TRUE,
	   will call FreeMemory from UnlockImageCache
	*/
	BOOL m_suppressed_free_memory;

	/**
	   a grace time slot. these are stored as links in the list
	   m_grace_times. created as needed in LockImageCache, removed
	   from FreeMemory. a couple (IMG_GRACE_TIME_SLOT_COUNT) are kept
	   around to avoid excessive allocation/deletion. each holds a
	   list of links owned by ImageReps placed in state of grace.
	 */
	class ImageGraceTimeSlot : public Head, public Link
	{
	public:
		ImageGraceTimeSlot() : m_time(0) {}
		~ImageGraceTimeSlot() { OP_ASSERT(Empty()); }
		/** grace time - images associated with this slot will be kept
		 * in cache until this time is exceeded */
		double m_time;
	};
	ImageGraceTimeSlot* m_current_grace_time; ///< current grace time slot - 0 when image cache not locked
	AutoDeleteHead m_grace_times; ///< list of grace time slots
#ifdef IMG_TOGGLE_CACHE_UNUSED_IMAGES
	BOOL is_caching_unused_images;
#endif // IMG_TOGGLE_CACHE_UNUSED_IMAGES
};

#endif // IMAGEMANAGERIMP_H
