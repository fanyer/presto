/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/img/src/imagemanagerimp.h"
#include "modules/img/imagedecoderfactory.h"
#include "modules/img/src/imagerep.h"
#include "modules/img/src/imagecontent.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/hardcore/cpuusagetracker/cpuusagetrackeractivator.h"

#ifdef IMG_TIME_LIMITED_CACHE
#include "modules/hardcore/mh/mh.h"
#endif // IMG_TIME_LIMITED_CACHE

#ifdef ASYNC_IMAGE_DECODERS_EMULATION
#include "modules/img/src/asyncemulationdecoder.h"
#endif // ASYNC_IMAGE_DECODERS_EMULATION

#ifdef CACHE_UNUSED_IMAGES
#include "modules/logdoc/urlimgcontprov.h"
#endif // CACHE_UNUSED_IMAGES

ImageManager* ImageManager::Create(ImageProgressListener* listener)
{
	ImageManagerImp* img_man = OP_NEW(ImageManagerImp, (listener));
	null_image_listener = OP_NEW(NullImageListener, ());
	if (img_man == NULL || null_image_listener == NULL || (img_man->Create() == OpStatus::ERR_NO_MEMORY))
	{
		OP_DELETE(img_man);
		OP_DELETE(null_image_listener);
		null_image_listener = NULL;
		return NULL;
	}
	return img_man;
}

ImageManagerImp::ImageManagerImp(ImageProgressListener* listener) : progress_listener(listener),
																	cache_size(0),
																	used_mem(0)
#ifdef IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
																	, anim_cache_size(0),
																	anim_used_mem(0)
#endif // IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
																	, cache_policy(IMAGE_CACHE_POLICY_SOFT)
#ifdef ASYNC_IMAGE_DECODERS
																	, async_factory(NULL)
#endif // ASYNC_IMAGE_DECODERS
																	, null_image_content(NULL)
#ifdef IMG_TIME_LIMITED_CACHE
																	, pending_cache_timeout(FALSE)
#endif // IMG_TIME_LIMITED_CACHE
#ifdef IMGMAN_USE_SCRATCH_BUFFER
							, scratch_buffer(NULL)
							, scratch_buffer_size(0)
#endif // IMGMAN_USE_SCRATCH_BUFFER
                                                                  , m_lock_count(0)
                                                                  , m_grace_lock_count(0)
                                                                  , m_suppressed_free_memory(FALSE)
                                                                  , m_current_grace_time(NULL)
#ifdef IMG_TOGGLE_CACHE_UNUSED_IMAGES
							, is_caching_unused_images(
#ifdef IMG_CACHE_UNUSED_IMAGES
							TRUE
#else // IMG_CACHE_UNUSED_IMAGES
							FALSE
#endif // IMG_CACHE_UNUSED_IMAGES
							)
#endif // IMG_TOGGLE_CACHE_UNUSED_IMAGES
{
}

ImageManagerImp::~ImageManagerImp()
{
#ifdef IMGMAN_USE_SCRATCH_BUFFER
	OP_DELETEA(scratch_buffer);
#endif // IMGMAN_USE_SCRATCH_BUFFER
#ifdef IMG_TIME_LIMITED_CACHE
	g_main_message_handler->UnsetCallBack(this, MSG_IMG_CLEAN_IMAGE_CACHE, (MH_PARAM_1)this);
#endif // IMG_TIME_LIMITED_CACHE
#ifdef ASYNC_IMAGE_DECODERS
	OP_DELETE(async_factory);
#endif // ASYNC_IMAGE_DECODERS
#ifdef HAS_ATVEF_SUPPORT
	OP_DELETE(atvef_image);
	atvef_image = NULL;
#endif // HAS_ATVEF_SUPPORT
	ClearImageList(&image_list);
	ClearImageList(&visible_image_list);
	ClearImageList(&free_image_list);
	bitmap_image_list.Clear();
	image_decoder_factory_list.Clear();
	OP_DELETE(progress_listener);
	OP_DELETE(null_image_content);
	null_image_content = NULL;
	OP_DELETE(null_image_listener);
	null_image_listener = NULL;
}

void ImageManagerImp::LockImageCache()
{
    if (!m_lock_count)
    {
        OP_ASSERT(!m_grace_lock_count);

        // find first unused grace time
        OP_ASSERT(!m_current_grace_time);
        ImageGraceTimeSlot* gt = static_cast<ImageGraceTimeSlot*>(m_grace_times.First());
        for (; gt; gt = static_cast<ImageGraceTimeSlot*>(gt->Suc()))
        {
            if (gt->Empty())
                break;
        }
        // no unused grace time available - create new entry
        if (!gt)
        {
            // on OOM this in effect will mean that images will have
            // no grace time even if marked no longer visible between
            // call to BeginGraceTime and EndGraceTime, which is
            // probably not too bad
            gt = OP_NEW(ImageGraceTimeSlot, ());
            if (gt)
            {
                gt->Into(&m_grace_times);
            }
        }
        OP_ASSERT(!gt || gt->Empty());
        m_current_grace_time = gt;

        m_suppressed_free_memory = FALSE;
    }

    ++ m_lock_count;
}
void ImageManagerImp::UnlockImageCache()
{
    OP_ASSERT(m_lock_count);
    -- m_lock_count;

    if (!m_lock_count)
    {
        OP_ASSERT(!m_grace_lock_count);

        // set grace time - images made no longer visible between call
        // to BeginGraceTime and EndGraceTime will not be purged until
        // this time has passed
        if (m_current_grace_time)
        {
            m_current_grace_time->m_time =
                m_current_grace_time->Empty() ? 0 : g_op_time_info->GetRuntimeMS() + IMG_GRACE_TIME;
            m_current_grace_time = NULL;
        }

        // image cache is no longer locked - purge now
        if (m_suppressed_free_memory)
        {
            FreeMemory();
            m_suppressed_free_memory = FALSE;
        }
    }
}
void ImageManagerImp::BeginGraceTime()
{
    OP_ASSERT(m_lock_count);
    ++ m_grace_lock_count;
}
void ImageManagerImp::EndGraceTime()
{
    OP_ASSERT(m_lock_count);
    OP_ASSERT(m_grace_lock_count);
    -- m_grace_lock_count;
}
BOOL ImageManagerImp::StateOfGrace(ImageRep* rep, double now) const
{
    ImageGraceTimeSlot* gt = static_cast<ImageGraceTimeSlot*>(rep->GraceTimeSlot());
    if (!gt)
        return FALSE;

    // this function may be called before the grace time has been set,
    // in which case the image is in state of grace
    if (m_lock_count && gt == m_current_grace_time)
        return TRUE;
    OP_ASSERT(gt->m_time);
    return gt->m_time >= now;
}

#ifdef IMG_TOGGLE_CACHE_UNUSED_IMAGES
void ImageManagerImp::CacheUnusedImages(BOOL strategy)
 {
     if (is_caching_unused_images == strategy)
         return;

     is_caching_unused_images = strategy;

     // ir->SetCacheUnusedImage(FALSE) may result in ir being
     // deleted, so next will have to be fetched before calling
     ImageRep* next;
     for (ImageRep* ir = (ImageRep*)image_list.First(); ir; ir = next)
     {
         next = (ImageRep*)ir->Suc();
         ir->SetCacheUnusedImage(is_caching_unused_images);
     }
     for (ImageRep* ir = (ImageRep*)visible_image_list.First(); ir; ir = next)
     {
         next = (ImageRep*)ir->Suc();
         ir->SetCacheUnusedImage(is_caching_unused_images);
     }
 }
#endif // IMG_TOGGLE_CACHE_UNUSED_IMAGES

void ImageManagerImp::ClearImageList(Head* list)
{
#ifdef CACHE_UNUSED_IMAGES
    const double now = g_op_time_info->GetRuntimeMS();
	// Free all images first since they might be in the cache but not in a document, 
	// in which case they will be deleted by this and also delete their UrlImageContentProvider
	ImageRep* nextir;
	for (ImageRep* ir = (ImageRep*)list->First(); ir; ir = nextir)
	{
		nextir = (ImageRep*)ir->Suc();
		if (!ir->IsFreed() && IsFreeable(ir, now))
			ir->Clear();
	}
#endif // CACHE_UNUSED_IMAGES
	ImageRep* imageRep = (ImageRep*)list->First();
	while (imageRep != NULL)
	{
		RemoveFromImageList(imageRep);
		RemoveLoadedImage(imageRep);
#ifdef ASYNC_IMAGE_DECODERS
		RemoveAsyncDestroyImage(imageRep);
#endif // ASYNC_IMAGE_DECODERS
		OP_DELETE(imageRep);
		imageRep = (ImageRep*)list->First();
	}
}

OP_STATUS ImageManagerImp::Create()
{
#ifdef IMG_TIME_LIMITED_CACHE
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_IMG_CLEAN_IMAGE_CACHE, (MH_PARAM_1)this));
#endif // IMG_TIME_LIMITED_CACHE
	null_image_content = OP_NEW(NullImageContent, ());
	if (null_image_content == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
#ifdef HAS_ATVEF_SUPPORT
 	atvef_image = OP_NEW(Image, ());
	if (!atvef_image)
		return OpStatus::ERR_NO_MEMORY;

 	ImageRep* rep = ImageRep::CreateAtvefImage(this);
 	if (!rep)
	{
		OP_DELETE(atvef_image);
		atvef_image = NULL;
 		return OpStatus::ERR_NO_MEMORY;
	}

 	atvef_image->SetImageRep(rep);
#endif
 	return OpStatus::OK;
}

OP_STATUS ImageManagerImp::AddImageDecoderFactory(ImageDecoderFactory* factory, INT32 type, BOOL check_header)
{
	DecoderFactoryElm* elm = OP_NEW(DecoderFactoryElm, ());
	if (elm == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	elm->factory = factory;
	elm->type = type;
	elm->check_header = check_header;
	elm->Into(&image_decoder_factory_list);
	return OpStatus::OK;
}

ImageDecoderFactory* ImageManagerImp::GetImageDecoderFactory(int type)
{
	for (DecoderFactoryElm* elm = (DecoderFactoryElm*)image_decoder_factory_list.First(); elm != NULL; elm = (DecoderFactoryElm*)elm->Suc())
	{
		if (elm->type == type)
		{
			return elm->factory;
		}
	}
	return NULL;
}

BOOL ImageManagerImp::IsFreeable(ImageRep* image_rep, double now)
{
	if (cache_policy == IMAGE_CACHE_POLICY_SOFT)
	{
		return (!image_rep->IsLocked() && !image_rep->IsVisible() && !StateOfGrace(image_rep, now));
	}
	else if (cache_policy == IMAGE_CACHE_POLICY_STRICT)
	{
		return (!image_rep->IsLocked());
	}
	return FALSE;
}

#ifdef IMG_TIME_LIMITED_CACHE
void ImageManagerImp::ScheduleCacheTimeout()
{
	if (pending_cache_timeout)
		g_main_message_handler->RemoveDelayedMessage(MSG_IMG_CLEAN_IMAGE_CACHE, (MH_PARAM_1)this, 0);
	pending_cache_timeout = FALSE;
	// Find the time for the oldest image
	ImageRep* image_rep = (ImageRep*)image_list.First();
    const double now = g_op_time_info->GetRuntimeMS();
	while (image_rep && !IsFreeable(image_rep, now))
	{
		// Make sure the current image is used more recently than the next
		OP_ASSERT(!image_rep->Suc() || image_rep->GetLastUsed() <= ((ImageRep*)image_rep->Suc())->GetLastUsed());
		image_rep = (ImageRep*)image_rep->Suc();
	}
	if (image_rep)
	{
		unsigned int tickdiff = g_op_time_info->GetRuntimeTickMS()-image_rep->GetLastUsed();
		if (tickdiff > IMG_CACHE_TIME_LIMIT*1000)
			tickdiff = 0;
		else
			tickdiff = IMG_CACHE_TIME_LIMIT*1000 - tickdiff;
		// Add 10 seconds to group some messages together better
		tickdiff += 10000;
		g_main_message_handler->PostDelayedMessage(MSG_IMG_CLEAN_IMAGE_CACHE, (MH_PARAM_1)this, 0, tickdiff);
		pending_cache_timeout = TRUE;
	}
}

void ImageManagerImp::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	pending_cache_timeout = FALSE;
	FreeMemory();
}
#endif // IMG_TIME_LIMITED_CACHE

void ImageManagerImp::FreeMemory()
{
    if (m_lock_count)
    {
        m_suppressed_free_memory = TRUE;
        return;
    }

    const double now = g_op_time_info->GetRuntimeMS();

	// Free enough freeable images excluding predecoding images.
	ImageRep* image_rep = (ImageRep*)image_list.First();
#ifdef IMG_TIME_LIMITED_CACHE
	unsigned int nowTick = g_op_time_info->GetRuntimeTickMS();
	BOOL deleted_oldest = FALSE;
#endif // IMG_TIME_LIMITED_CACHE
#ifdef IMAGE_TURBO_MODE
	BOOL stop_predecoding_images = MoreToFree();
#endif // IMAGE_TURBO_MODE
	while (image_rep != NULL && (MoreToFree()
#ifdef IMG_TIME_LIMITED_CACHE
				|| nowTick-image_rep->GetLastUsed() > IMG_CACHE_TIME_LIMIT*1000
#endif // IMG_TIME_LIMITED_CACHE
				))
	{
		ImageRep* next_image_rep = (ImageRep*)image_rep->Suc();
#ifdef IMG_TIME_LIMITED_CACHE
		deleted_oldest = TRUE;
		// Make sure the current image is used more recently than the next
		OP_ASSERT(!next_image_rep || image_rep->GetLastUsed() <= next_image_rep->GetLastUsed());
#endif // IMG_TIME_LIMITED_CACHE
		if (IsFreeable(image_rep, now))
		{
			image_rep->Clear();
		}
		image_rep = next_image_rep;
	}
#ifdef IMAGE_TURBO_MODE
	// Stop all ongoing predecoding images if the cacheend was reached.
	if (stop_predecoding_images)
		StopAllPredecodingImages();
#endif // IMAGE_TURBO_MODE
#ifdef IMG_TIME_LIMITED_CACHE
	if (deleted_oldest || !pending_cache_timeout)
		ScheduleCacheTimeout();
#endif // IMG_TIME_LIMITED_CACHE

    // go through grace time slots and delete/clear the ones no longer relevant
    UINT32 unused_grace_time_slots = 0;
    ImageGraceTimeSlot* next;
    for (ImageGraceTimeSlot* gt = static_cast<ImageGraceTimeSlot*>(m_grace_times.First()); gt; gt = next)
    {
        next = static_cast<ImageGraceTimeSlot*>(gt->Suc());

        // slot no longer used
        if (gt->m_time < now || gt->Empty())
        {
            gt->RemoveAll();

            ++ unused_grace_time_slots;
            if (unused_grace_time_slots > IMG_GRACE_TIME_SLOT_COUNT)
            {
                gt->Out();
                OP_DELETE(gt);
            }
            else
            {
                gt->m_time = 0;
            }
        }
    }
}

void ImageManagerImp::SetCacheSize(INT32 cache_size, ImageCachePolicy cache_policy)
{
	this->cache_size = cache_size;
	this->cache_policy = cache_policy;
#ifdef IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
	this->anim_cache_size = (1024 * 1024) + cache_size / 4;
#endif // IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
	FreeMemory();
}

void ImageManagerImp::Touch(ImageRep* imageRep)
{
	imageRep->Out();
	ImageRepIntoImageList(imageRep);
}

#ifdef IMG_CACHE_MULTIPLE_ANIMATION_FRAMES

void ImageManagerImp::TouchAnimated(AnimatedImageContent* animated_image_content)
{
	animated_image_content->Out();
	animated_image_content->Into(&animated_image_list);
}

#endif // IMG_CACHE_MULTIPLE_ANIMATION_FRAMES

Image ImageManagerImp::GetImage(ImageContentProvider* content_provider)
{
	ImageRep* imageRep = GetImageRep(content_provider);
	if (imageRep == NULL)
	{
		imageRep = AddToImageList(content_provider);

		if (imageRep == NULL)
		{
			// FIXME:OOM-KILSMO
			return Image();
		}
	}

	Image image;
	image.SetImageRep(imageRep);

	return image;
}

Image ImageManagerImp::GetImage(OpBitmap* bitmap)
{
	ImageRep* imageRep = ImageRep::Create(bitmap);
	if (imageRep == NULL)
	{
		// FIXME:OOM-KILSMO
		return Image();
	}
	imageRep->Into(&bitmap_image_list);
	Image image;
	image.SetImageRep(imageRep);
	return image;
}

void ImageManagerImp::ResetImage(ImageContentProvider* content_provider)
{
	ImageRep* imageRep = GetImageRep(content_provider);

	if (imageRep != NULL)
	{
		imageRep->Reset();
	}
}

ImageRep* ImageManagerImp::GetImageRep(ImageContentProvider* content_provider)
{
	void* image_rep = NULL;
	OP_STATUS ret_val = image_hash_table.GetData(content_provider, &image_rep);
	if (OpStatus::IsError(ret_val))
	{
		return NULL;
	}
	OP_ASSERT(image_rep);
	return (ImageRep*)image_rep;
}

LoadedImageElm* ImageManagerImp::GetLoadedImageElm(ImageRep* image_rep)
{
	for (LoadedImageElm* elm = (LoadedImageElm*)loaded_image_list.First(); elm != NULL; elm = (LoadedImageElm*)elm->Suc())
	{
		if (elm->image_rep == image_rep)
		{
			return elm;
		}
	}

	return NULL;
}

OP_STATUS ImageManagerImp::AddLoadedImage(ImageRep* image_rep)
{
	LoadedImageElm* elm = GetLoadedImageElm(image_rep);
	if (elm != NULL)
	{
		return OpStatus::OK;
	}
	elm = OP_NEW(LoadedImageElm, ());
	if (elm == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	elm->image_rep = image_rep;

	if (loaded_image_list.Empty())
	{
		PostContinueLoading(); // FIXME:IMG
	}

	elm->Into(&loaded_image_list);
	return OpStatus::OK;
}

void ImageManagerImp::RemoveLoadedImage(ImageRep* image_rep)
{
	LoadedImageElm* elm = GetLoadedImageElm(image_rep);
	if (elm)
	{
		elm->Out();
		OP_DELETE(elm);
	}
}

void ImageManagerImp::Progress()
{
	LoadedImageElm* elm = (LoadedImageElm*)loaded_image_list.First();

#ifdef IMAGE_TURBO_MODE
	// Skip invisible images so visible images get higher priority
	while (elm && elm->Suc() && elm->image_rep->IsPredecoding())
		elm = (LoadedImageElm*) elm->Suc();
#endif

	if (elm != NULL)
	{
		ImageRep* image_rep = elm->image_rep;
		OP_ASSERT(image_rep);
		TRACK_CPU_PER_TAB(image_rep->GetContentProvider()->GetCPUUsageTracker());
		elm->Out();
		OP_DELETE(elm);
		image_rep->OnMoreData(image_rep->GetContentProvider()); // FIXME:OOM-KILSMO
		if (image_rep->IsFailed())
		{
			image_rep->ReportError();
		}
		if (!loaded_image_list.Empty())
		{
			PostContinueLoading();
		}
	}
}

int ImageManagerImp::CheckImageType(const unsigned char* data_buf, UINT32 buf_len)
{
	OP_ASSERT(data_buf != 0 || buf_len == 0);
	for (DecoderFactoryElm* elm = (DecoderFactoryElm*)image_decoder_factory_list.First(); elm != NULL; elm = (DecoderFactoryElm*)elm->Suc())
	{
		if (elm->check_header)
		{
			BOOL3 ret = elm->factory->CheckType(data_buf, buf_len);
			if (ret == YES)
			{
				return elm->type;
			}
			if (ret == MAYBE)
			{
				return 0;
			}
		}
	}
	return -1;
}

BOOL ImageManagerImp::IsImage(int type)
{
	ImageDecoderFactory* factory  = GetImageDecoderFactory(type);
	return factory != NULL;
}


#ifdef IMG_CACHE_MULTIPLE_ANIMATION_FRAMES

void ImageManagerImp::FreeAnimMemory(AnimatedImageContent* animated_image_content)
{
	if (anim_used_mem > anim_cache_size)
	{
		AnimatedImageContent* elm = (AnimatedImageContent*)animated_image_list.First();
		while (elm)
		{
			AnimatedImageContent* next_elm = (AnimatedImageContent*)elm->Suc();
			if (elm != animated_image_content)
			{
				elm->ClearBuffers();
				elm->Out();
				if (anim_cache_size >= anim_used_mem)
				{
					return;
				}
			}
			elm = next_elm;
		}
	}
}

#endif // IMG_CACHE_MULTIPLE_ANIMATION_FRAMES

#ifdef ASYNC_IMAGE_DECODERS

void ImageManagerImp::SetAsyncImageDecoderFactory(AsyncImageDecoderFactory* factory)
{
	async_factory = factory;
}

void ImageManagerImp::FreeUnusedAsyncDecoders()
{
	AsyncImageElm* elm = (AsyncImageElm*)destroyed_async_decoders.First();
	while (elm != NULL)
	{
		elm->Out();
		elm->rep->FreeAsyncDecoder();
		OP_DELETE(elm);

		elm = (AsyncImageElm*)destroyed_async_decoders.First();
	}
}

void ImageManagerImp::AddToAsyncDestroyList(ImageRep* image_rep)
{
	AsyncImageElm* elm = OP_NEW(AsyncImageElm, ()); // FIXME:OOM
	if (elm != NULL)
	{
		elm->rep = image_rep;
		elm->Into(&destroyed_async_decoders);
	}
}

void ImageManagerImp::RemoveAsyncDestroyImage(ImageRep* image_rep)
{
	AsyncImageElm* elm = GetAsyncImageElm(image_rep);
	if (elm)
	{
		elm->Out();
		OP_DELETE(elm);
	}

	elm = GetAsyncDestroyElm(image_rep);
	if (elm)
	{
		elm->Out();
		elm->rep->FreeAsyncDecoder();
		OP_DELETE(elm);
	}
}

AsyncImageElm* ImageManagerImp::GetAsyncDestroyElm(ImageRep* image_rep)
{
	for (AsyncImageElm* elm = (AsyncImageElm*)destroyed_async_decoders.First(); elm != NULL; elm = (AsyncImageElm*)elm->Suc())
	{
		if (elm->rep == image_rep)
		{
			return elm;
		}
	}

	return NULL;
}

#endif // ASYNC_IMAGE_DECODERS


#ifdef HAS_ATVEF_SUPPORT
Image ImageManagerImp::GetAtvefImage()
{
	return *atvef_image;
}
#endif

void ImageManagerImp::ImageRepIntoImageList(ImageRep* image_rep)
{
	if (image_rep->IsVisible())
	{
		image_rep->Into(&visible_image_list);
	}
	else
	{
		if (image_rep->IsFreed())
		{
			image_rep->Into(&free_image_list);
		}
		else
		{
			image_rep->Into(&image_list);
#ifdef IMG_TIME_LIMITED_CACHE
			image_rep->UpdateLastUsed(g_op_time_info->GetRuntimeTickMS());
			if (!pending_cache_timeout)
				ScheduleCacheTimeout();
#endif // IMG_TIME_LIMITED_CACHE
}
	}
}

void ImageManagerImp::RemoveFromImageList(ImageRep* image_rep)
{
	ImageContentProvider* content_provider = image_rep->GetImageContentProvider();
	void* dummy;
	OP_STATUS ret_val = image_hash_table.Remove(content_provider, &dummy);
	OpStatus::Ignore(ret_val);
	image_rep->Out();
}

void ImageManagerImp::ImageRepMoveToRightList(ImageRep* image_rep)
{
	image_rep->Out();
	ImageRepIntoImageList(image_rep);
}

#ifdef ASYNC_IMAGE_DECODERS_EMULATION

void ImageManagerImp::AddBufferedImageDecoder(AsyncEmulationDecoder* decoder)
{
	BOOL post_continue = FALSE;

	if (!decoder->InList())
	{
		post_continue = buffered_image_decoder_list.Empty();
		decoder->Into(&buffered_image_decoder_list);
	}

	if (post_continue)
	{
		progress_listener->OnMoreBufferedData();
	}
}

void ImageManagerImp::MoreBufferedData()
{
	AsyncEmulationDecoder* decoder = (AsyncEmulationDecoder*)buffered_image_decoder_list.First();
	if (decoder != NULL)
	{
		decoder->Out();
		decoder->Decode();
		if (!buffered_image_decoder_list.Empty())
		{
			progress_listener->OnMoreBufferedData();
		}
	}
}

void ImageManagerImp::MoreDeletedDecoders()
{
}

#endif // ASYNC_IMAGE_DECODERS_EMULATION

#ifdef CACHE_UNUSED_IMAGES
void ImageManagerImp::FreeImagesForContext(URL_CONTEXT_ID id)
{
	ImageRep* image_rep = (ImageRep*)image_list.First();
	while (image_rep != NULL)
	{
		ImageRep* next_image_rep = (ImageRep*)image_rep->Suc();
		if (image_rep->GetCachedUnusedImage() &&
			((UrlImageContentProvider*)image_rep->GetContentProvider())->GetUrl()->GetContextId() == id)
		{
			image_rep->Clear();
		}

		image_rep = next_image_rep;
	}
}
#endif // CACHE_UNUSED_IMAGES

#ifdef IMGMAN_USE_SCRATCH_BUFFER
UINT32* ImageManagerImp::GetScratchBuffer(unsigned int width)
{
	if (width <= scratch_buffer_size)
		return scratch_buffer;
	UINT32* pmb = OP_NEWA(UINT32, width);
	if (!pmb)
		return NULL;
	OP_DELETEA(scratch_buffer);
	scratch_buffer = pmb;
	scratch_buffer_size = width;
	return scratch_buffer;
}
#endif // IMGMAN_USE_SCRATCH_BUFFER
