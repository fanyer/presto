/** -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef CORE_THUMBNAIL_SUPPORT
#include "modules/thumbnails/src/thumbnail.h"
#include "modules/thumbnails/src/thumbnailview.h"
#include "modules/dochand/win.h"
#include "modules/formats/url_dfr.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/util/opguid.h"

#include "modules/thumbnails/thumbnailmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"

#include "modules/skin/OpSkinUtils.h"

#ifdef SUPPORT_GENERATE_THUMBNAILS

class ThumbnailManager::Task : public OpThumbnail
#ifdef THUMBNAILS_SEQUENTIAL_GENERATE
	, public Link
#endif
{
	public:
		virtual ~Task()
		{
#ifdef THUMBNAILS_SEQUENTIAL_GENERATE
			g_main_message_handler->RemoveDelayedMessage(MSG_THUMBNAILS_START_NEXT_REQUEST, (MH_PARAM_1)this, 0);
#endif // THUMBNAILS_SEQUENTIAL_GENERATE
		};

		Task(const URL &u, BOOL reload) : url(u), m_has_finished(FALSE), m_reload(reload) {}

		virtual void OnFinished(OpBitmap * buffer)
		{
			OP_NEW_DBG("ThumbnailManager::Task::OnFinished()", "thumbnail");
			m_has_finished = TRUE;
			g_thumbnail_manager->OnThumbnailFinished(url, ReleaseBuffer(buffer), GetPreviewRefresh(), m_request_width, m_request_height, m_mode, m_render_data.logo_rect);
		}

		virtual void OnError(OpLoadingListener::LoadingFinishStatus status)
		{
			OP_NEW_DBG("ThumbnailManager::Task::OnError()", "thumbnail");
			m_has_finished = TRUE;
			g_thumbnail_manager->OnThumbnailFailed(url, status);
		}

		virtual void OnCleanup()
		{
			OP_NEW_DBG("ThumbnailManager::Task::OnCleanup()", "thumbnail");
			g_thumbnail_manager->OnThumbnailCleanup(url);
		}

		BOOL IsFinished() { return m_has_finished; }
		BOOL IsReloadTask() { return m_reload; }

	private:
		URL url;
		BOOL m_has_finished;		// has OnFinished() been called at least once?
		BOOL m_reload;
};

OP_STATUS ThumbnailManager::Construct()
{
#ifdef THUMBNAILS_SEQUENTIAL_GENERATE
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_THUMBNAILS_START_NEXT_REQUEST, 0));
#endif
    RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_THUMBNAILS_FREE_LARGE_CACHED_IMAGES, 0));
	return OpStatus::OK;
}

ThumbnailManager::~ThumbnailManager()
{
	m_resizers.DeleteAll();

	g_main_message_handler->UnsetCallBacks(this);

	FreeCache();
}

void ThumbnailManager::FreeCache()
{
	if (m_cache)
	{
		OpHashIterator *iter = m_cache->GetIterator();
		if(iter)
		{
			OP_STATUS istatus;
			for (istatus = iter->First(); istatus == OpStatus::OK; istatus = iter->Next())
			{
				CacheEntry *cache_entry = static_cast<CacheEntry*>(iter->GetData());

				// We must remove it from the hash table before destroying it as
				// a task might still be in progress.  The CacheEntry destructor 
				// will otherwise abort it in which case the hash table will be accessed again,
				// finding the same entry again and hence crash. See DSK-374570.
				if(OpStatus::IsSuccess(m_cache->Remove(cache_entry->key, &cache_entry)))
					OP_DELETE(cache_entry);
			}
			OP_DELETE(iter);
		}
	}
	OP_DELETE(m_cache);
	m_cache = NULL;
}

OP_STATUS ThumbnailManager::Flush()
{
	OP_NEW_DBG("ThumbnailManager::Flush()", "thumbnail");
#ifdef SUPPORT_SAVE_THUMBNAILS
	if (m_cache)
	{
		OpHashIterator *iter = m_cache->GetIterator();
		if (!iter)
			return OpStatus::ERR_NO_MEMORY;
		OP_STATUS istatus;
		for (istatus = iter->First(); istatus == OpStatus::OK; istatus = iter->Next())
		{
			CacheEntry *cache_entry = static_cast<CacheEntry*>(iter->GetData());
			StopTimeout(cache_entry); // no more delayed loading beyond this point, bug 291144
			if (cache_entry->refcount == 0 && !cache_entry->filename.IsEmpty())
			{
				OpFile file;
				if (!OpStatus::IsError(file.Construct(cache_entry->filename.CStr(), OPFILE_THUMBNAIL_FOLDER)))
					OpStatus::Ignore(file.Delete());
				cache_entry->filename.Empty();
				m_index_dirty = true;
			}
		}
		OP_DELETE(iter);
	}
#endif // SUPPORT_SAVE_THUMBNAILS
	return SaveIndex();
}

OP_STATUS ThumbnailManager::Purge()
{
	OP_NEW_DBG("ThumbnailManager::Purge()", "thumbnail");
	RETURN_IF_ERROR(Flush());
	NotifyInvalidateThumbnails();
	FreeCache();

#ifdef THUMBNAILS_SEQUENTIAL_GENERATE
	OP_ASSERT(m_suspended_tasks.Cardinal() == 0);
	OP_ASSERT(m_active == FALSE);
#endif

	return OpStatus::OK;
}

OP_STATUS ThumbnailManager::GetThumbnailForWindow(Window *window, Image &thumbnail, BOOL force_rebuild, OpThumbnail::ThumbnailMode mode)
{
	OP_NEW_DBG("ThumbnailManager::GetThumbnailForWindow()", "thumbnail");
	OP_DBG(("") << "window: " << window << (force_rebuild ? " + force rebuild":""));
	const BOOL auto_width = TRUE;

	OpBitmap * bitmap = OpThumbnail::CreateThumbnail(window, THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT, auto_width, mode);
	if (!bitmap)
		return OpStatus::ERR_NO_MEMORY;

	thumbnail = imgManager->GetImage(bitmap);
	return OpStatus::OK;
}

OP_STATUS ThumbnailManager::ThumbnailResizer::Create(ThumbnailResizer** resizer, ThumbnailManager* manager, const URL &document_url, ThumbnailManager::CacheEntry* cache_entry, int target_width, int target_height)
{
	OP_ASSERT(g_thumbnail_manager->CacheLookup(document_url));
	OP_ASSERT(g_thumbnail_manager->CacheLookup(document_url) == cache_entry);

	*resizer = OP_NEW(ThumbnailResizer, (manager, document_url, cache_entry, target_width, target_height));

	if (NULL == *resizer)
	{
		return OpStatus::ERR;
	}
	else
	{
		if(!(*resizer)->StartDecoding())
		{
			// Only if the image decoder needs to do it's job in aync mode
			// Then we need to delete this resizer later
			OP_STATUS st = manager->m_resizers.Add(*resizer);
			if (OpStatus::IsError(st))
			{
				OP_DELETE(*resizer);
				return OpStatus::ERR;
			}
		}
	}
	return OpStatus::OK;
}

ThumbnailManager::ThumbnailResizer::ThumbnailResizer(ThumbnailManager* manager, const URL& document_url, CacheEntry* cache_entry, int target_width, int target_height)
{
	OP_ASSERT(target_width > 0);
	OP_ASSERT(target_height > 0);
	OP_ASSERT(cache_entry);
	OP_ASSERT(manager);
	OP_ASSERT(!document_url.IsEmpty());

	m_thumbnail_manager = manager;
	m_document_url = document_url;

	m_cache_entry = cache_entry;

	OP_ASSERT(g_thumbnail_manager->CacheLookup(m_document_url));
	OP_ASSERT(g_thumbnail_manager->CacheLookup(m_document_url) == m_cache_entry);

	m_retry_count = 0;

	m_target_width = target_width;
	m_target_height = target_height;
}

BOOL ThumbnailManager::ThumbnailResizer::StartDecoding()
{
	if (m_cache_entry->image.ImageDecoded())
	{
		OnImageDecoded();
		return TRUE;
	}
	else
	{
		m_cache_entry->SetImageDecodedListener(this);
		return FALSE;
	}
}

ThumbnailManager::ThumbnailResizer::~ThumbnailResizer()
{
	OP_ASSERT(g_thumbnail_manager->CacheLookup(m_document_url));
	OP_ASSERT(g_thumbnail_manager->CacheLookup(m_document_url) == m_cache_entry);

	m_cache_entry->SetImageDecodedListener(NULL);
}

OpBitmap* ThumbnailManager::ThumbnailResizer::GetResizedThumbnailBitmap()
{
	OP_ASSERT(g_thumbnail_manager->CacheLookup(m_document_url));
	OP_ASSERT(g_thumbnail_manager->CacheLookup(m_document_url) == m_cache_entry);

	OpBitmap* bitmap = NULL;
	OpBitmap* cached_bitmap = m_cache_entry->image.GetBitmap(m_cache_entry);

	if (!cached_bitmap)
		return NULL;

	OpRect src = OpRect(0, 0, cached_bitmap->Width(), cached_bitmap->Height());
	OpRect dst = OpRect(0, 0, m_target_width, m_target_height);
	OpRect scaled_src;
	OpRect src_clip;

	OpPoint padding(0, 0);

	float src_aspect = (float)src.width / src.height;
	float dst_aspect = (float)dst.width / dst.height;

	bool is_icon       = (OpThumbnail::IconThumbnail      == m_cache_entry->mode);
	bool is_minimized  = (OpThumbnail::MinimizedThumbnail == m_cache_entry->mode);
	bool has_logo_rect = (OpThumbnail::FindLogo           == m_cache_entry->mode && !m_cache_entry->logo_rect.IsEmpty());

	if (!is_icon)
	{
		if (has_logo_rect)
		{
			OpRect cache_rect = src;

			const int logo_margin = 10;

			int crop_width;
			int crop_height;

			OpRect logo_rect = m_cache_entry->logo_rect.InsetBy(-logo_margin);

			if (logo_rect.width  <= m_target_width &&
				logo_rect.height <= m_target_height)
			{
				// no scaling needed, just cropping
				crop_width  = m_target_width;
				crop_height = m_target_height;
			}
			else
			{
				// scaling needed, which means we need to surround logo_rect
				// with a rect of the appropriate aspect ratio
				if (logo_rect.width * m_target_height < logo_rect.height * m_target_width)
				{
					// we need to make it wider
					crop_width  = logo_rect.height * m_target_width / m_target_height;
					crop_height = logo_rect.height;
				}
				else
				{
					// we need to make it taller
					crop_width  = logo_rect.width;
					crop_height = logo_rect.width * m_target_height / m_target_width;
				}
			}

			src.width  = crop_width;
			src.height = crop_height;

			// center the logo
			src.x = logo_rect.x - (crop_width  - logo_rect.width ) / 2;
			src.y = logo_rect.y - (crop_height - logo_rect.height) / 2;

			// make sure src fits inside cache_rect, whence it is pulled:...
			// ...scoot up and left...
			if (src.x + src.width  > cache_rect.width)
				src.x = cache_rect.width  - src.width;
			if (src.y + src.height > cache_rect.height)
				src.y = cache_rect.height - src.height;
			// ...scoot down and right...
			if (src.x < 0)
				src.x = 0;
			if (src.y < 0)
				src.y = 0;
			// ...and clip to cache_rect if we still don't fit
			if (src.width  > cache_rect.width)
				src.width  = cache_rect.width;
			if (src.height > cache_rect.height)
				src.height = cache_rect.height;

			if (crop_width  == m_target_width &&
				crop_height == m_target_height)
			{
				// only cropping, so use cheap scaling
				bitmap = OpThumbnail::ScaleBitmap(cached_bitmap, m_target_width, m_target_height, dst, src);
			}
			else
			{
				// scaling, so make it nice
				OpAutoPtr<OpBitmap> cropped_bitmap(OpThumbnail::ScaleBitmap(cached_bitmap, m_target_width, m_target_height, dst, src));

				OP_STATUS st = OpBitmap::Create(&bitmap, m_target_width, m_target_height, TRUE, TRUE, 0, 0, TRUE);

				if (OpStatus::IsError(st))
					return NULL;

				OpSkinUtils::ScaleBitmap(cropped_bitmap.get(), bitmap);
			}
		}
		else if (is_minimized)
		{
			// SkinUtils seems to use a different scaling algoritm that does better for small zoom factor. Since this means
			// most probably than the scaling is slower, we only use it if we have to, like for view-mode: minimized thumbnails.
			OP_STATUS st = OpBitmap::Create(&bitmap, m_target_width, m_target_height, TRUE, TRUE, 0, 0, TRUE);

			if (OpStatus::IsError(st))
				return NULL;

			OpSkinUtils::ScaleBitmap(cached_bitmap, bitmap);
		}
		else
		{
			bitmap = OpThumbnail::ScaleBitmap(cached_bitmap, m_target_width, m_target_height, dst, src);
		}
	}
	else
	{
		OP_STATUS st = OpBitmap::Create(&bitmap, m_target_width, m_target_height, TRUE, TRUE, 0, 0, TRUE);

		if (OpStatus::IsError(st))
		{
			m_cache_entry->image.ReleaseBitmap();
			return NULL;
		}

		void* buffer_pointer = bitmap->GetPointer();
		if (buffer_pointer)
		{
			op_memset(buffer_pointer, 0, bitmap->GetBpp() * bitmap->Width() * bitmap->Height() / 8);
			bitmap->ReleasePointer();
		}

		OpPainter* painter = bitmap->GetPainter();
		if (!painter)
		{
			OP_DELETE(bitmap);
			m_cache_entry->image.ReleaseBitmap();
			return NULL;
		}

		painter->SetColor(255, 255, 255, 255);
		painter->FillRect(OpRect(0, 0, dst.width, dst.height));

		if (src_aspect > dst_aspect)
		{
			if (src.width > dst.width)
			{
				float scale = (float)dst.width / src.width;

				scaled_src.width = (int)op_ceil(src.width * scale);
				scaled_src.height = (int)op_ceil((float)scaled_src.width / src_aspect);

				scaled_src.x = (dst.width - scaled_src.width) / 2;
				scaled_src.y = (dst.height - scaled_src.height) / 2;

				painter->DrawBitmapScaledAlpha(cached_bitmap, src, scaled_src);
			}
			else if (src.width < dst.width)
			{
				padding.x = (dst.width - src.width) / 2;
				padding.y = (dst.height - src.height) / 2;
				painter->DrawBitmapAlpha(cached_bitmap, padding);
			}
			else
			{
				painter->DrawBitmapAlpha(cached_bitmap, OpPoint(0, 0));
			}
		}
		else
		{
			if (src.height > dst.height)
			{
				float scale = (float)dst.height / src.height;

				scaled_src.height = (int)op_ceil(src.height * scale);
				scaled_src.width = (int)op_ceil(scaled_src.height * src_aspect);

				scaled_src.x = (dst.width - scaled_src.width) / 2;
				scaled_src.y = (dst.height - scaled_src.height) / 2;

				painter->DrawBitmapScaledAlpha(cached_bitmap, src, scaled_src);
			}
			else if (src.height < dst.height)
			{
				padding.x = (dst.width - src.width) / 2;
				padding.y = (dst.height - src.height) / 2;
				painter->DrawBitmapAlpha(cached_bitmap, padding);
			}
			else
			{
				painter->DrawBitmapAlpha(cached_bitmap, OpPoint(0, 0));
			}
		}
		bitmap->ReleasePainter();
	}
	m_cache_entry->image.ReleaseBitmap();
	return bitmap;
}

void ThumbnailManager::ThumbnailResizer::OnImageDecodeFailed()
{
	OP_ASSERT(g_thumbnail_manager->CacheLookup(m_document_url));
	OP_ASSERT(g_thumbnail_manager->CacheLookup(m_document_url) == m_cache_entry);

	m_cache_entry->SetImageDecodedListener(NULL);
	m_thumbnail_manager->NotifyThumbnailFailed(m_document_url, OpLoadingListener::LOADING_UNKNOWN);

	DeleteThis();
}

void ThumbnailManager::ThumbnailResizer::OnImageDecoded()
{
	OP_ASSERT(g_thumbnail_manager->CacheLookup(m_document_url));
	OP_ASSERT(g_thumbnail_manager->CacheLookup(m_document_url) == m_cache_entry);

	m_cache_entry->SetImageDecodedListener(NULL);

	OpBitmap* bitmap = GetResizedThumbnailBitmap();

	if (bitmap)
	{
		if (!m_cache_entry->c_image.IsEmpty())
			m_cache_entry->c_image.DecVisible(m_cache_entry);

		m_cache_entry->c_image = imgManager->GetImage(bitmap);
		m_thumbnail_manager->NotifyThumbnailReady(m_document_url, m_cache_entry->c_image, m_cache_entry->title.CStr(), m_cache_entry->preview_refresh);
	}
	else
		m_thumbnail_manager->NotifyThumbnailFailed(m_document_url, OpLoadingListener::LOADING_UNKNOWN);

    m_cache_entry->free_msg_count++;
    g_main_message_handler->PostDelayedMessage(MSG_THUMBNAILS_FREE_LARGE_CACHED_IMAGES, reinterpret_cast<MH_PARAM_1>(m_cache_entry),
                                               0, THUMBNAILS_FREE_DELAY_MS);

	DeleteThis();
}

void ThumbnailManager::ThumbnailResizer::DeleteThis()
{
	// It might happen that this resizer is not yet in m_resizers since the image was not decoded in async mode
	OpStatus::Ignore(m_thumbnail_manager->m_resizers.RemoveByItem(this));
	OP_DELETE(this);
}

void ThumbnailManager::SetThumbnailSize(int base_width, int base_height, double max_zoom)
{
	ThumbnailSize size;

	size.base_thumbnail_width  = base_width;
	size.base_thumbnail_height = base_height;
	size.max_zoom              = max_zoom;

	m_thumbnail_size = size;
}

OP_STATUS ThumbnailManager::RequestThumbnail(const URL &document_url, BOOL check_if_expired, BOOL reload, OpThumbnail::ThumbnailMode mode, int target_width, int target_height)
{
	if (!document_url.IsValid())
		return OpStatus::ERR;

	OP_NEW_DBG("ThumbnailManager::RequestThumbnail()", "thumbnail");
#ifdef _DEBUG
	OpString8 url_str;
	document_url.GetAttribute(URL::KName_Escaped, url_str);
	OP_DBG(("") << "url: " << url_str << (check_if_expired ? " + check if expired":"") << (reload ? " + reload":"") << "; size: " << target_width << "x" << target_height);
#endif // _DEBUG

	CacheEntry* cache_entry = CacheLookup(document_url);
	if (cache_entry && !check_if_expired && !reload)
	{
		BOOL minimized_size_ok = FALSE;
		BOOL minimized_fixed_viewport = FALSE;

        // The small cached image is checked before calling CacheGetImage. That latter might cause the image to be reloaded from disk.
        Image cached_image = cache_entry->c_image;
        if (!cached_image.IsEmpty() &&
            static_cast<int>(cached_image.Width()) == target_width &&
            static_cast<int>(cached_image.Height()) == target_height)
        {
            NotifyThumbnailReady(document_url, cache_entry->c_image, cache_entry->title, cache_entry->preview_refresh);
            return OpStatus::OK;
        }

        OpThumbnail::ThumbnailMode mode = cache_entry->mode;
        Image image = CacheGetImage(cache_entry);

		if (OpThumbnail::MinimizedThumbnail == mode)
			if (static_cast<unsigned int>(target_width)  < cache_entry->target_width ||
				static_cast<unsigned int>(target_height) < cache_entry->target_height)
				minimized_fixed_viewport = TRUE;

		if (minimized_fixed_viewport && (int)image.Width() == m_thumbnail_size.base_thumbnail_width && (int)image.Height() == m_thumbnail_size.base_thumbnail_height)
			minimized_size_ok = TRUE;

		if (OpThumbnail::MinimizedThumbnail == mode && cache_entry->target_width == static_cast<unsigned int>(target_width) && cache_entry->target_height == static_cast<unsigned int>(target_height))
			minimized_size_ok = TRUE;

		if (OpThumbnail::MinimizedThumbnail != mode || minimized_fixed_viewport || minimized_size_ok)
		{
			if ( (OpThumbnail::MinimizedThumbnail == mode && minimized_size_ok) ||
				(OpThumbnail::MinimizedThumbnail != mode) )
			{
				if (!image.IsEmpty())
				{
					ThumbnailResizer* resizer;
					RETURN_IF_ERROR(ThumbnailResizer::Create(&resizer, this, document_url, cache_entry, target_width, target_height));

					if (cache_entry->task && !cache_entry->task->IsFinished())
						NotifyThumbnailRequestStarted(document_url, cache_entry->task->IsReloadTask());

					return OpStatus::OK;
				}
			}
		}
	}
	if (cache_entry && cache_entry->task)
	{
		if (!cache_entry->task->IsFinished())
			return OpStatus::OK; // the task has already been started
		StopTimeout(cache_entry);
		WidgetWindow *widget_window = cache_entry->widget_window;
		Task *task = cache_entry->task;
		OP_DBG(("Delete existing task ") << task);
		cache_entry->task = 0;
		cache_entry->widget_window = 0;
		cache_entry->view = 0;
		OP_DELETE(task);
		if (widget_window)
			widget_window->Close();
	}
	NotifyThumbnailRequestStarted(document_url, reload);
	OpAutoPtr<CacheEntry> new_cache_entry;
	if (!cache_entry)
	{
		new_cache_entry = CacheNew(document_url);
		cache_entry = new_cache_entry.get();
		RETURN_OOM_IF_NULL(cache_entry);
		OP_DBG(("Create new cache entry") << cache_entry);
	}
	cache_entry->task = OP_NEW(Task, (document_url, reload));
	RETURN_OOM_IF_NULL(cache_entry->task);
	OP_DBG(("Starting new task ") << cache_entry->task);
	cache_entry->widget_window = OP_NEW(WidgetWindow, ());
	RETURN_OOM_IF_NULL(cache_entry->widget_window);
	RETURN_IF_ERROR(cache_entry->widget_window->Init(OpWindow::STYLE_TOOLTIP));
	RETURN_IF_ERROR(OpThumbnailView::Construct(&cache_entry->view));
	cache_entry->widget_window->GetWidgetContainer()->GetRoot()->AddChild(cache_entry->view);
	RETURN_IF_ERROR(cache_entry->view->init_status);

	Window *window = cache_entry->view->GetWindow();
	window->SetIsScriptableWindow(TRUE);
	window->ForcePluginsDisabled(TRUE);
	window->ForceJavaDisabled(TRUE);
	window->SetFeatures(WIN_TYPE_THUMBNAIL);
	int flags = 0;
	if (cache_entry && cache_entry->mode == OpThumbnail::MinimizedThumbnail)
		flags |= 0x01;
	if (check_if_expired)
		flags |= 0x01;
	if (reload || cache_entry->error_page == TRUE)
		flags |= 0x04;

	BOOL suspend = FALSE;

#ifdef THUMBNAILS_SEQUENTIAL_GENERATE
	suspend = m_active;
	AddSuspendedTask(cache_entry->task);
#endif // THUMBNAILS_SEQUENTIAL_GENERATE

	OP_STATUS status = cache_entry->task->Init(window, document_url,
											   THUMBNAIL_INTERNAL_WINDOW_WIDTH, THUMBNAIL_INTERNAL_WINDOW_WIDTH * target_height / target_width,
											   flags, mode, suspend, target_width, target_height
											   );

	RETURN_IF_ERROR(status);
	RETURN_IF_ERROR(StartTimeout(cache_entry));
	RETURN_IF_ERROR(CachePut(document_url, cache_entry));
	new_cache_entry.release();
	return OpStatus::OK;
}

OP_STATUS ThumbnailManager::CancelThumbnail(const URL &document_url)
{
	OP_NEW_DBG("ThumbnailManager::CancelThumbnail()", "thumbnail");
	CacheEntry * cache_entry = CacheLookup(document_url);
	if (!cache_entry || !cache_entry->task)
		return OpStatus::OK; // the task has never been started

#ifdef _DEBUG
	OpString8 url_str;
	document_url.GetAttribute(URL::KName_Escaped, url_str);
	OP_DBG(("") << "url: " << url_str);
#endif // _DEBUG

	StopTimeout(cache_entry);
	OP_STATUS status = cache_entry->task->Stop();
	OP_DELETE(cache_entry->task);
	cache_entry->task = 0;
	OP_DELETE(cache_entry->widget_window);
	cache_entry->widget_window = 0;
	cache_entry->view = 0;
	NotifyThumbnailFailed(document_url, OpLoadingListener::LOADING_UNKNOWN);
	return status;
}

OP_STATUS ThumbnailManager::AddThumbnailRef(const URL &document_url)
{
	OP_NEW_DBG("ThumbnailManager::AddThumbnailRef()", "thumbnail");
#ifdef _DEBUG
	OpString8 url_str;
	document_url.GetAttribute(URL::KName_Escaped, url_str);
	OP_DBG(("") << "url: " << url_str);
#endif // _DEBUG

	CacheEntry * cache_entry = CacheLookup(document_url);
	OpAutoPtr<CacheEntry> new_cache_entry;
	if (!cache_entry)
	{
		new_cache_entry = CacheNew(document_url);
		cache_entry = new_cache_entry.get();
		OP_DBG(("New cache entry:" ) << cache_entry);
		RETURN_OOM_IF_NULL(cache_entry);
	}
#ifdef SUPPORT_SAVE_THUMBNAILS
	cache_entry->refcount++;
	OP_DBG(("refcount: ") << cache_entry->refcount);
#endif // SUPPORT_SAVE_THUMBNAILS
	RETURN_IF_ERROR(CachePut(document_url, cache_entry));
	new_cache_entry.release();
	return OpStatus::OK;
}

OP_STATUS ThumbnailManager::DelThumbnailRef(const URL &document_url)
{
	OP_NEW_DBG("ThumbnailManager::DelThumbnailRef()", "thumbnail");
#ifdef _DEBUG
	OpString8 url_str;
	document_url.GetAttribute(URL::KName_Escaped, url_str);
	OP_DBG(("") << "url: " << url_str);
#endif // _DEBUG

	CacheEntry * cache_entry = CacheLookup(document_url);
	if (!cache_entry)
		return OpStatus::OK;
#ifdef SUPPORT_SAVE_THUMBNAILS
	OP_ASSERT(cache_entry->refcount > 0);
	cache_entry->refcount--;
	OP_DBG(("refcount: ") << cache_entry->refcount);
#endif // SUPPORT_SAVE_THUMBNAILS
	RETURN_IF_ERROR(CachePut(document_url, cache_entry));
	return OpStatus::OK;
}

void ThumbnailManager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
    if (msg == MSG_THUMBNAILS_FREE_LARGE_CACHED_IMAGES)
    {
                CacheEntry *cache_entry = reinterpret_cast<CacheEntry*>(par1);

        // free_msg_count keeps track of all the free requests for one thumbnail
        // and only frees the memory on the last request (on the last zoom or resize made by the user)
        if (cache_entry->free_msg_count == 1)
        {
            if (!cache_entry->image.IsEmpty())
            {
                cache_entry->image.DecVisible(cache_entry);
                cache_entry->image.Empty();
                imgManager->ResetImage(cache_entry);
                cache_entry->Reset();
            }
        }
        cache_entry->free_msg_count--;
        return;
    }

#ifdef THUMBNAILS_SEQUENTIAL_GENERATE
	if (msg == MSG_THUMBNAILS_START_NEXT_REQUEST)
	{
		Task *t = (Task*)par1;
		t->Start();
	}
	else
#endif
	{
        CacheEntry *cache_entry = reinterpret_cast<CacheEntry *>(par1);
		if (!cache_entry)
			return;

		Task *task = cache_entry->task;

		if (!task)
		{
			StopTimeout(cache_entry);
		}
		else if (task->IsFinished())
		{
			WidgetWindow *widget_window = cache_entry->widget_window;
			cache_entry->task = 0;
			cache_entry->widget_window = 0;
			cache_entry->view = 0;
			OP_DELETE(task);
			if (widget_window)
				widget_window->Close();
		}
		else
		{
			// not done yet, give it some more time
			StopTimeout(cache_entry);
			StartTimeout(cache_entry);
		}
	}
}

void ThumbnailManager::OnThumbnailFinished(const URL &url, OpBitmap* buffer, long preview_refresh, int target_width, int target_height, OpThumbnail::ThumbnailMode mode, OpRect logo_rect)
{
	OP_NEW_DBG("ThumbnailManager::OnThumbnailFinished()", "thumbnail");
#ifdef _DEBUG
	OpString8 url_str;
	url.GetAttribute(URL::KName_Escaped, url_str);
	OP_DBG(("") << "url: " << url_str);
#endif // _DEBUG

	CacheEntry * cache_entry = CacheLookup(url);
	if (!cache_entry)
		return;
	OpString title;
	OpStatus::Ignore(title.Set(cache_entry->task->GetTitle()));
	Image image = imgManager->GetImage(buffer);
	CachePutImage(cache_entry, image);
	CachePutTitle(cache_entry, title.CStr());
	CachePutPreviewRefresh(cache_entry, preview_refresh);
	CachePutThumbnailMode(cache_entry, mode);
	CachePutLogoRect(cache_entry, logo_rect);
	CachePutRequestTargetSize(cache_entry, target_width, target_height);
	CachePut(url, cache_entry);
	cache_entry->error_page = FALSE;

	ThumbnailResizer* resizer;
	if (OpStatus::IsError(ThumbnailResizer::Create(&resizer, this, url, cache_entry, target_width, target_height)))
		NotifyThumbnailFailed(url, OpLoadingListener::LOADING_UNKNOWN);

#ifdef THUMBNAILS_SEQUENTIAL_GENERATE
	ResumeNextSuspendedTask();
#endif // THUMBNAILS_SEQUENTIAL_GENERATE
}

void ThumbnailManager::OnThumbnailFailed(const URL &url, OpLoadingListener::LoadingFinishStatus status)
{
	OP_NEW_DBG("ThumbnailManager::OnThumbnailFailed()", "thumbnail");
#ifdef _DEBUG
	OpString8 url_str;
	url.GetAttribute(URL::KName_Escaped, url_str);
	OP_DBG(("") << "url: " << url_str);
#endif // _DEBUG

	CacheEntry * cache_entry = CacheLookup(url);
	if (!cache_entry)
		return;
	Task *task = cache_entry->task;
	WidgetWindow *widget_window = cache_entry->widget_window;
	cache_entry->task = 0;
	cache_entry->widget_window = 0;
	cache_entry->view = 0;
	cache_entry->error_page = TRUE;
	CachePutImage(cache_entry, Image());

	OpString title;
	url.GetAttribute(URL::KUniName_Username, 0, title);
	CachePutTitle(cache_entry, title.CStr());

	CachePut(url, cache_entry);
	NotifyThumbnailFailed(url, status);
	OP_DELETE(task);
	cache_entry->task = NULL;
	if (widget_window)
		widget_window->Close();

#ifdef THUMBNAILS_SEQUENTIAL_GENERATE
	ResumeNextSuspendedTask();
#endif // THUMBNAILS_SEQUENTIAL_GENERATE
}

void ThumbnailManager::OnThumbnailCleanup(const URL &url)
{
	OP_NEW_DBG("ThumbnailManager::OnThumbnailCleanup()", "thumbnail");
#ifdef _DEBUG
	OpString8 url_str;
	url.GetAttribute(URL::KName_Escaped, url_str);
	OP_DBG(("") << "url: " << url_str);
#endif // _DEBUG

	CacheEntry * cache_entry = CacheLookup(url);
	if (!cache_entry)
		return;

	WidgetWindow *widget_window = cache_entry->widget_window;
	cache_entry->widget_window = 0;
	cache_entry->view = 0;
	if (widget_window)
		widget_window->Close(TRUE);
}

void ThumbnailManager::NotifyThumbnailRequestStarted(const URL &url, BOOL reload)
{
	OP_NEW_DBG("ThumbnailManager::NotifyThumbnailRequestStarted()", "thumbnail");
#ifdef _DEBUG
	OpString8 url_str;
	url.GetAttribute(URL::KName_Escaped, url_str);
	OP_DBG(("") << "url: " << url_str);
#endif // _DEBUG

	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
		m_listeners.GetNext(iterator)->OnThumbnailRequestStarted(url, reload);
}

void ThumbnailManager::NotifyThumbnailReady(const URL &url, const Image &thumbnail, const uni_char *title, long preview_refresh)
{
	OP_NEW_DBG("ThumbnailManager::NotifyThumbnailReady()", "thumbnail");
#ifdef _DEBUG
	OpString8 url_str;
	url.GetAttribute(URL::KName_Escaped, url_str);
	OP_DBG(("") << "url: " << url_str << "; title: " << title << "; image: "
		   << thumbnail.Width() << "x" << thumbnail.Height()
		   << (thumbnail.IsEmpty() ? " (is empty)" : ""));
#endif // _DEBUG

	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
		m_listeners.GetNext(iterator)->OnThumbnailReady(url, thumbnail, title, preview_refresh);
}

void ThumbnailManager::NotifyThumbnailFailed(const URL &url, OpLoadingListener::LoadingFinishStatus status)
{
	OP_NEW_DBG("ThumbnailManager::NotifyThumbnailFailed()", "thumbnail");
#ifdef _DEBUG
	OpString8 url_str;
	url.GetAttribute(URL::KName_Escaped, url_str);
	OP_DBG(("") << "url: " << url_str);
#endif // _DEBUG

	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
		m_listeners.GetNext(iterator)->OnThumbnailFailed(url, status);
}

void ThumbnailManager::NotifyInvalidateThumbnails()
{
	OP_NEW_DBG("ThumbnailManager::NotifyInvalidateThumbnails()", "thumbnail");
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
		m_listeners.GetNext(iterator)->OnInvalidateThumbnails();
}

ThumbnailManager::CacheEntry * ThumbnailManager::CacheLookup(const URL &url)
{
	OP_NEW_DBG("ThumbnailManager::CacheLookup()", "thumbnail");
	if (!m_cache)
		RETURN_VALUE_IF_ERROR(LoadIndex(), NULL);
	CacheEntry * cache_entry;

	OpString key;
	url.GetAttribute(URL::KUniName_Username, 0, key);
	RETURN_VALUE_IF_ERROR(m_cache->GetData(key.CStr(), &cache_entry), NULL);
	return cache_entry;
}

ThumbnailManager::CacheEntry * ThumbnailManager::CacheNew(const URL &url)
{
	CacheEntry *cache_entry = OP_NEW(CacheEntry, ());
	if (!cache_entry)
		return NULL;

	OpString key;
	url.GetAttribute(URL::KUniName_Username, 0, key);
	cache_entry->key.Set(key.CStr());
	if (cache_entry->key.IsEmpty())	// catch both OOM and empty url problems
	{
		OP_DELETE(cache_entry);
		return NULL;
	}
	OpStatus::Ignore(cache_entry->title.Set(cache_entry->key));
	return cache_entry;
}

OP_STATUS ThumbnailManager::CachePut(const URL &url, CacheEntry * cache_entry)
{
	if (!m_cache)
		RETURN_IF_ERROR(LoadIndex());
	RETURN_IF_MEMORY_ERROR(m_cache->Add(cache_entry->key.CStr(), cache_entry));

#ifdef SUPPORT_SAVE_THUMBNAILS
	OP_STATUS status = OpStatus::OK;
	if (cache_entry->refcount > 0)
	{
		do
		{
			if (cache_entry->filename.IsEmpty())
			{
				OpGuid guid;
				status = g_opguidManager->GenerateGuid(guid);
				if (OpStatus::IsError(status)) break;
				OpString8 temp;
				if (temp.Reserve(42) == NULL) // 37 /* GUID */ + 5 /* .png\0 */
					break;
				OpGUIDManager::ToString(guid, temp.DataPtr(), temp.Capacity());
				temp.Append(".png");
				status = cache_entry->filename.SetFromUTF8(temp.CStr());
				if (OpStatus::IsError(status)) break;
				m_index_dirty = TRUE;
			}
			OpFile file;
			status = file.Construct(cache_entry->filename.CStr(), OPFILE_THUMBNAIL_FOLDER);
			if (OpStatus::IsError(status)) break;
			if (!cache_entry->image.IsEmpty() && cache_entry->image.ImageDecoded())
			{
				int len = 0;
				char *data = OpThumbnail::EncodeImagePNG(cache_entry->image.GetBitmap(NULL), len, 1);
				cache_entry->image.ReleaseBitmap();
				if (!data)
				{
					status = OpStatus::ERR_NO_MEMORY;
					break;
				}
				status = file.Open(OPFILE_WRITE);
				if (OpStatus::IsSuccess(status))
					status = file.Write(data, len);
				if (OpStatus::IsSuccess(status))
					status = file.Close();
				OP_DELETEA(data);
			}
		} while (FALSE);
	}
	RETURN_IF_MEMORY_ERROR(status);
#endif // SUPPORT_SAVE_THUMBNAILS

	return SaveIndex();
}

const Image & ThumbnailManager::CacheGetImage(CacheEntry* cache_entry)
{
	cache_entry->access_stamp = ++m_global_access_stamp;
#ifdef SUPPORT_SAVE_THUMBNAILS
	if (cache_entry->image.IsEmpty() && !cache_entry->filename.IsEmpty() && !OpStatus::IsError(cache_entry->ReadData()))
		CachePutImage(cache_entry, imgManager->GetImage(cache_entry));
#endif // SUPPORT_SAVE_THUMBNAILS
	return cache_entry->image;
}

OP_STATUS ThumbnailManager::CachePutImage(CacheEntry* cache_entry, const Image &image)
{
	if (cache_entry->image.IsEmpty() && !image.IsEmpty())
	{
		m_cached_images++;
		cache_entry->access_stamp = ++m_global_access_stamp;
		while (m_cached_images > THUMBNAILS_MAX_CACHED_IMAGES)
		{
			CacheEntry * lru = 0;
			int lru_stamp = cache_entry->access_stamp;
			OpHashIterator *iter = m_cache->GetIterator();
			if (!iter)
				return OpStatus::ERR_NO_MEMORY;
			OP_STATUS istatus;
			for (istatus = iter->First(); istatus == OpStatus::OK; istatus = iter->Next())
			{
				CacheEntry *entry = static_cast<CacheEntry*>(iter->GetData());
				if (entry->access_stamp < lru_stamp && !entry->image.IsEmpty())
					lru = entry;
			}
			OP_DELETE(iter);

			if (!lru)
				break;
			if (lru->refcount == 0 && lru->filename.IsEmpty())
			{
				CacheEntry *dummy;
				m_cache->Remove(lru->key.CStr(), &dummy);
				OP_DELETE(lru);
				m_cached_images--;
			} else if (OpStatus::IsError(CachePutImage(lru, Image())))
				break;
		}
	}
	else if (!cache_entry->image.IsEmpty() && image.IsEmpty())
	{
		m_cached_images--;
	}
	if (!cache_entry->image.IsEmpty())
		cache_entry->image.DecVisible(cache_entry);

	cache_entry->image = image;

	cache_entry->image.IncVisible(cache_entry);

	return OpStatus::OK;
}

OP_STATUS ThumbnailManager::CachePutTitle(CacheEntry * cache_entry, const uni_char *title)
{
	if (cache_entry->title.Compare(title) != 0)
		m_index_dirty = TRUE;
	return cache_entry->title.Set(title);
}

void ThumbnailManager::CachePutPreviewRefresh(CacheEntry * cache_entry, long preview_refresh)
{
	if (cache_entry->preview_refresh != preview_refresh)
		m_index_dirty = TRUE;
	cache_entry->preview_refresh = preview_refresh;
}

void ThumbnailManager::CachePutThumbnailMode(CacheEntry * cache_entry, OpThumbnail::ThumbnailMode mode)
{
	if (cache_entry->mode != mode)
		m_index_dirty = TRUE;
	cache_entry->mode = mode;
}

void ThumbnailManager::CachePutLogoRect(CacheEntry* cache_entry, OpRect logo_rect)
{
	if (!cache_entry->logo_rect.Equals(logo_rect))
		m_index_dirty = TRUE;
	cache_entry->logo_rect = logo_rect;
}


void ThumbnailManager::CachePutRequestTargetSize(CacheEntry* cache_entry, int target_width, int target_height)
{
	OP_ASSERT(target_width > 0 && target_height > 0);
	if (cache_entry->target_width != (unsigned int)target_width || cache_entry->target_height != (unsigned int)target_height)
		m_index_dirty = TRUE;
	cache_entry->target_width = target_width;
	cache_entry->target_height = target_height;
}

OP_STATUS ThumbnailManager::LoadIndex()
{
	OP_NEW_DBG("ThumbnailManager::LoadIndex()", "thumbnail");
	OP_ASSERT(!m_cache);
	m_cache = OP_NEW(OpAutoStringHashTable<CacheEntry>, ());
	if (!m_cache)
		return OpStatus::ERR_NO_MEMORY;

#ifdef SUPPORT_SAVE_THUMBNAILS
	OP_STATUS status = OpStatus::OK;

	do {
		OpFile *llfile = OP_NEW(OpFile, ());
		if (!llfile)
			return OpStatus::ERR_NO_MEMORY;

		OpAutoPtr<OpFile> llfile_ap(llfile);
		OP_DBG(("Opening opthumb.dat"));
		status = llfile->Construct(UNI_L("opthumb.dat"), OPFILE_HOME_FOLDER);
		if (OpStatus::IsError(status)) break;
		status = llfile->Open(OPFILE_READ);
		if (OpStatus::IsError(status)) break;
		DataFile file(llfile);
		llfile_ap.release();

		OpString8 temp;

		TRAP(status,
			file.InitL();
			OpStackAutoPtr<DataFile_Record> record(file.GetNextRecordL());
			for (; record.get(); record.reset(file.GetNextRecordL()))
			{
				record->SetRecordSpec(file.GetRecordSpec());
				record->IndexRecordsL();
				if (record->GetTag() == TAG_THUMBNAIL_ENTRY)
				{
					OP_DBG(("Found TAG_THUMBNAIL_ENTRY"));
					record->GetIndexedRecordStringL(TAG_BLOCK_URL, temp);
					if (temp.HasContent())	/* don't crash on corrupted file */
					{
						OP_DBG((" url: ") << temp);
						OpStackAutoPtr<CacheEntry> cache_entry(OP_NEW_L(CacheEntry, ()));
						cache_entry->key.SetFromUTF8L(temp.CStr());
						record->GetIndexedRecordStringL(TAG_BLOCK_FILENAME, temp);
						OP_DBG((" filename: ") << temp);
						cache_entry->filename.SetFromUTF8L(temp.CStr());
						record->GetIndexedRecordStringL(TAG_BLOCK_TITLE, temp);
						OP_DBG((" title: ") << temp);
						cache_entry->title.SetFromUTF8L(temp.CStr());
						cache_entry->preview_refresh = record->GetIndexedRecordUIntL(TAG_BLOCK_PREVIEW_REFRESH);
						cache_entry->mode = (OpThumbnail::ThumbnailMode)record->GetIndexedRecordUIntL(TAG_BLOCK_MODE);
						cache_entry->logo_rect.x      = record->GetIndexedRecordUIntL(TAG_BLOCK_LOGO_RECT_X);
						cache_entry->logo_rect.y      = record->GetIndexedRecordUIntL(TAG_BLOCK_LOGO_RECT_Y);
						cache_entry->logo_rect.width  = record->GetIndexedRecordUIntL(TAG_BLOCK_LOGO_RECT_W);
						cache_entry->logo_rect.height = record->GetIndexedRecordUIntL(TAG_BLOCK_LOGO_RECT_H);
						cache_entry->target_width     = record->GetIndexedRecordUIntL(TAG_BLOCK_TARGET_WIDTH);
						cache_entry->target_height    = record->GetIndexedRecordUIntL(TAG_BLOCK_TARGET_HEIGHT);
						m_cache->AddL(cache_entry->key.CStr(), cache_entry.get());

						CacheGetImage(cache_entry.get());

						cache_entry.release();
					}
				} else
				{
					status = OpStatus::ERR;
					break;
				}
			}
		);
	} while (FALSE);

	RETURN_IF_MEMORY_ERROR(status);
#endif // SUPPORT_SAVE_THUMBNAILS

	m_index_dirty = FALSE;
	return OpStatus::OK;
}

OP_STATUS ThumbnailManager::SaveIndex()
{
	OP_NEW_DBG("ThumbnailManager::SaveIndex()", "thumbnail");
	if (!m_cache || !m_index_dirty)
		return OpStatus::OK;
	m_index_dirty = FALSE;

#ifdef SUPPORT_SAVE_THUMBNAILS
	OP_STATUS status = OpStatus::OK;

	do {
		OpFile *llfile = OP_NEW(OpFile, ());
		if (!llfile)
			return OpStatus::ERR_NO_MEMORY;

		OpAutoPtr<OpFile> llfile_ap(llfile);
		status = llfile->Construct(UNI_L("opthumb.dat"), OPFILE_HOME_FOLDER);
		if (OpStatus::IsError(status)) break;
		status = llfile->Open(OPFILE_WRITE);
		if (OpStatus::IsError(status)) break;
		DataFile file(llfile, INDEX_FILE_VERSION, 1, 2);
		llfile_ap.release();

		OpHashIterator *iter = m_cache->GetIterator();
		if (!iter)
			return OpStatus::ERR_NO_MEMORY;

		OpString8 temp;

		TRAP(status,
			file.InitL();
			OpStackAutoPtr<DataFile_Record> record(NULL);

			OP_STATUS istatus;
			for (istatus = iter->First(); OpStatus::IsSuccess(istatus); istatus = iter->Next())
			{
				const uni_char *url = static_cast<const uni_char*>(iter->GetKey());
				const CacheEntry *cache_entry = static_cast<CacheEntry*>(iter->GetData());
				if (cache_entry->refcount > 0 && !cache_entry->filename.IsEmpty())
				{
					record.reset(OP_NEW_L(DataFile_Record, (TAG_THUMBNAIL_ENTRY)));
					record->SetRecordSpec(file.GetRecordSpec());
					record->AddRecordL(TAG_BLOCK_URL, temp.SetUTF8FromUTF16L(url).CStr());
					record->AddRecordL(TAG_BLOCK_FILENAME, temp.SetUTF8FromUTF16L(cache_entry->filename.CStr()).CStr());
					record->AddRecordL(TAG_BLOCK_TITLE, temp.SetUTF8FromUTF16L(cache_entry->title.CStr()).CStr());
					record->AddRecordL(TAG_BLOCK_PREVIEW_REFRESH, cache_entry->preview_refresh);
					record->AddRecordL(TAG_BLOCK_MODE, cache_entry->mode);
					record->AddRecordL(TAG_BLOCK_TARGET_WIDTH, cache_entry->target_width);
					record->AddRecordL(TAG_BLOCK_TARGET_HEIGHT, cache_entry->target_height);
					record->AddRecordL(TAG_BLOCK_LOGO_RECT_X, cache_entry->logo_rect.x);
					record->AddRecordL(TAG_BLOCK_LOGO_RECT_Y, cache_entry->logo_rect.y);
					record->AddRecordL(TAG_BLOCK_LOGO_RECT_W, cache_entry->logo_rect.width);
					record->AddRecordL(TAG_BLOCK_LOGO_RECT_H, cache_entry->logo_rect.height);
					record->WriteRecordL(&file);
				}
			}
		);
		OP_DELETE(iter);
	} while (FALSE);

	RETURN_IF_MEMORY_ERROR(status);
#endif

	return OpStatus::OK;
}

#ifdef THUMBNAILS_SEQUENTIAL_GENERATE

void ThumbnailManager::AddSuspendedTask(Task* task)
{
	if (m_active && !m_suspended_tasks.HasLink(task))
		task->Into(&m_suspended_tasks);
	m_active = TRUE;
}

void ThumbnailManager::ResumeNextSuspendedTask()
{
	if (m_suspended_tasks.Cardinal() == 0)
	{
		m_active = FALSE;
	}
	else
	{
		Task *t = (Task*)m_suspended_tasks.First();
		g_main_message_handler->PostMessage(MSG_THUMBNAILS_START_NEXT_REQUEST, (MH_PARAM_1)t, 0);
		t->Out();
	}
}

#endif // THUMBNAILS_SEQUENTIAL_GENERATE


#ifdef SUPPORT_SAVE_THUMBNAILS
OP_STATUS ThumbnailManager::CacheEntry::GetData(const char*& buf, INT32& len, BOOL& more)
{
	if (!data)
		RETURN_IF_ERROR(ReadData());
	buf = data + pos;
	len = INT32(datalen - pos);
	more = FALSE;
	return OpStatus::OK;
}

OP_STATUS ThumbnailManager::CacheEntry::ReadData()
{
	OP_NEW_DBG("ThumbnailManager::CacheEntry::ReadData()", "thumbnail");
	OpFile file;
	OpFileLength len;
	RETURN_IF_ERROR(file.Construct(filename.CStr(), OPFILE_THUMBNAIL_FOLDER));
	OP_DBG(("Opening file ") << filename);
	RETURN_IF_ERROR(file.Open(OPFILE_READ));
	RETURN_IF_ERROR(file.GetFileLength(datalen));
	data = OP_NEWA(char, (size_t)datalen);
	if (!data)
		return OpStatus::ERR_NO_MEMORY;
	RETURN_IF_ERROR(file.Read(data, datalen, &len));
	if (len < datalen)
		return OpStatus::ERR;
	OP_DBG(("") << "Read " << (int)len << " bytes");
	RETURN_IF_ERROR(file.Close());
	return OpStatus::OK;
}

void ThumbnailManager::CacheEntry::OnPortionDecoded()
{
	if (image.ImageDecoded())
		if (m_image_decoded_listener)
			m_image_decoded_listener->OnImageDecoded();
}

void ThumbnailManager::CacheEntry::OnError(OP_STATUS status)
{
	if (m_image_decoded_listener)
		m_image_decoded_listener->OnImageDecodeFailed();
}

ThumbnailManager::CacheEntry::~CacheEntry()
{
	OP_NEW_DBG("ThumbnailManager::CacheEntry::~CacheEntry()", "thumbnail");
	OP_DBG(("") << "this: " << this << "; window: " << widget_window);

	if (task && !task->IsFinished())
		task->OnError(OpLoadingListener::LOADING_UNKNOWN);

	if (!image.IsEmpty())
		image.DecVisible(this);

	if (!resized_image.IsEmpty())
		resized_image.DecVisible(this);

	OP_DELETE(task);
	if (widget_window)
		widget_window->Close(TRUE);
#ifdef SUPPORT_SAVE_THUMBNAILS
	OP_DELETEA(data);
#endif // SUPPORT_SAVE_THUMBNAILS
}

#endif // SUPPORT_SAVE_THUMBNAILS

#endif // SUPPORT_GENERATE_THUMBNAILS

#endif // CORE_THUMBNAIL_SUPPORT
