/** -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */
#include "core/pch.h"

#ifdef CORE_THUMBNAIL_SUPPORT

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/dochand/fdelm.h"
#include "modules/doc/frm_doc.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/zlib/zlib.h"
#include "modules/layout/traverse/traverse.h"
#include "modules/layout/box/box.h"
#include "modules/minpng/minpng_encoder.h"

#include "modules/thumbnails/src/thumbnail.h"

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
#include "modules/display/pixelscalepainter.h"
#endif // PIXEL_SCALE_RENDERING_SUPPORT

#include "modules/debug/debug.h"

#include "modules/logdoc/htm_elm.h"
#include "modules/layout/content/content.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/thumbnails/thumbnailmanager.h"

#include "modules/url/tools/url_util.h"

#include "modules/dochand/viewportcontroller.h"

/**
 * OpThumbnail
 */

OpThumbnail::OpThumbnail()
	: m_window(0),
	  m_buffer(NULL),
	  m_target_width(0),
	  m_target_height(0),
	  m_request_width(0),
	  m_request_height(0),
	  m_source_width(0),
	  m_source_height(0),
	  m_state(THUMBNAIL_INITIAL),
	  m_auth_callback(NULL),
	  m_decode_poll_attemps(0),
	  m_flags(0)
#ifdef THUMBNAILS_PREALLOCATED_BITMAPS
	  ,m_preallocated_bitmap(NULL)
#endif
	  ,m_preview_refresh(0)
	  ,m_best_icon(NULL)
{
    OP_NEW_DBG("OpThumbnail::OpThumbnail()", "thumbnail");
	m_generate_timer.SetTimerListener(this);
#ifdef THUMBNAILS_LOADING_TIMEOUT
	m_load_timer.SetTimerListener(this);
#endif // THUMBNAILS_LOADING_TIMEOUT
	m_auth_timer.SetTimerListener(this);
}

OpThumbnail::~OpThumbnail()
{
    OP_NEW_DBG("OpThumbnail::~OpThumbnail()", "thumbnail");
	Stop();
}

void OpThumbnail::ConfigureWindow()
{
	OpWindow* w  = const_cast<OpWindow *>(m_window->GetOpWindow());
	WindowCommander* wc = m_window->GetWindowCommander();

	w->SetInnerSize(m_source_width, m_source_height);
	m_window->SetWindowSize(m_source_width, m_source_height);
	if( wc->GetDocumentListener() )
		wc->GetDocumentListener()->OnInnerSizeRequest(wc, m_source_width, m_source_height);

	m_window->VisualDev()->SetRenderingViewGeometryScreenCoords(OpRect(0, 0, m_source_width, m_source_height));
	m_window->SetShowScrollbars(FALSE);

	m_window->GetViewportController()->SetVisualViewport(OpRect(0,0,m_source_width, m_source_height));
}

OP_STATUS OpThumbnail::Init(Window *window, const URL url, int source_width, int source_height, int flags, ThumbnailMode mode, BOOL suspend, int target_width, int target_height)
{
	m_url = url;
	OpStatus::Ignore(url.GetAttribute(URL::KUniName_With_Fragment_Username, 0, m_url_string));

    OP_NEW_DBG("OpThumbnail::Init()", "thumbnail");
    OP_DBG((UNI_L("this=%p, window=%p url='%s'"), this, window, m_url_string.CStr()));
	OP_STATUS ret_stat = OpStatus::OK;

    if (m_state != THUMBNAIL_INITIAL && m_state != THUMBNAIL_STOPPED)
    {
        OP_DBG(("error: unexpected state (%d)", m_state));
        return OpStatus::ERR;
    }

	m_mode = mode;

	m_flags = flags;

    m_state = THUMBNAIL_STARTED;
    m_window = window;
	m_window->SetType(WIN_TYPE_THUMBNAIL);

	WindowCommander* wc = m_window->GetWindowCommander();

    wc->SetLoadingListener(this);

	m_window->SetShowScrollbars(FALSE);

	m_source_width = source_width;
	m_source_height = source_height;

	m_target_width   = target_width;
	m_target_height  = target_height;
	m_request_width  = target_width;
	m_request_height = target_height;

	ConfigureWindow();

	if (!suspend)
	{
		OP_STATUS status = Start();
		if (OpStatus::IsError(status))
		{
			OnError(OpLoadingListener::LOADING_UNKNOWN);
			return status;
		}
	}

	// Note: After OnError is called the thumbnail object is destroyed.
	//		 Don't do anything after this line.

	return ret_stat;
}


OP_STATUS OpThumbnail::Start()
{
    OP_NEW_DBG("OpThumbnail::Start()", "thumbnail");
    OP_DBG(("this=%p", this));
    if (m_state != THUMBNAIL_STARTED)
    {
        OP_DBG(("error: unexpected state (%d)", m_state));
        return OpStatus::ERR;
    }
    // Note: if the state is THUMBNAIL_STARTED, then the m_window is
    // expected to be initialized correctly. Or if
    // RenderThumbnailInternal() sets m_window to 0, then the state is
    // also changed to some other value than THUMBNAIL_STARTED...
    OP_ASSERT(m_window);

#ifdef THUMBNAILS_LOADING_TIMEOUT
    OP_DBG(("Starting load-timer"));
	m_load_timer.Start(THUMBNAILS_LOADING_TIMEOUT_SECONDS * 1000);
#endif // THUMBNAILS_LOADING_TIMEOUT

#ifdef THUMBNAILS_PREALLOCATED_BITMAPS
	// Guess the size of the bitmap needed to render the thumbnail.
	BOOL find_logo = m_mode == OpThumbnail::FindLogo;
	int bitmap_width, bitmap_height;

	if (find_logo)
	{
		bitmap_width = m_target_width;
		bitmap_height = m_target_height;
	}
	else
	{
		bitmap_width = THUMBNAIL_INTERNAL_WINDOW_WIDTH;
		bitmap_height = static_cast<int>(THUMBNAIL_INTERNAL_WINDOW_WIDTH * (m_target_height * 1.0 / m_target_width));
	}

	RETURN_IF_ERROR(OpBitmap::Create(&m_preallocated_bitmap, bitmap_width,
		bitmap_height, FALSE, FALSE, 0, 0, TRUE));
#endif // THUMBNAILS_PREALLOCATED_BITMAPS

	if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ThumbnailRequestHeader))
	{
		URL_Custom_Header header_item;
		RETURN_IF_ERROR(header_item.name.Set("X-Purpose"));
		RETURN_IF_ERROR(header_item.value.Set("preview"));
		RETURN_IF_ERROR(m_url.SetAttribute(URL::KAddHTTPHeader, &header_item));
	}

	DocumentReferrer ref_ignore;

	return m_window->OpenURL(	m_url,					// core URL object
								ref_ignore,				// reference URL
								m_flags & 0x1,			// check if expired
								m_flags & 0x4,			// reload
								-1,						// sub_win_id
								TRUE,					// user initiated
								TRUE,					// open in background
								WasEnteredByUser,		// entered by user
								FALSE					// externally initiated
								);
}

OP_STATUS OpThumbnail::Stop()
{
    OP_NEW_DBG("OpThumbnail::Stop()", "thumbnail");
    OP_DBG(("this=%p", this));
    // Stops the loading
	if (m_window)
	{
        OP_DBG(("Stop loading"));
        WindowCommander* wc = m_window->GetWindowCommander();
        wc->Stop();
        wc->SetLoadingListener(NULL);
	}
	m_state = THUMBNAIL_STOPPED;

    // Stop the timers:
#ifdef THUMBNAILS_LOADING_TIMEOUT
    OP_DBG(("Stop load-timer"));
    m_load_timer.Stop();
#endif // THUMBNAILS_LOADING_TIMEOUT
    OP_DBG(("Stop generate-timer"));
    m_generate_timer.Stop();

	OP_DELETE(m_buffer);
	m_buffer = NULL;

#ifdef THUMBNAILS_PREALLOCATED_BITMAPS
	OP_DELETE(m_preallocated_bitmap);
	m_preallocated_bitmap = NULL;
#endif

	return OpStatus::OK;
    // In this state, the thumbnail can be deleted.
}

OP_STATUS OpThumbnail::SetPreviewRefresh()
{
	FramesDocument *doc = m_window->GetCurrentDoc();
	if (doc && doc->GetTopDocument()->IsLoaded())
    {
		int url_value  = 0;
		int meta_value = 0;

		OpString8 header;

		RETURN_IF_ERROR(header.Set("Preview-refresh"));
		OpStatus::Ignore(m_url.GetAttribute(URL::KHTTPSpecificResponseHeaderL, header, TRUE));
		if (header.HasContent())
		{
			if (op_sscanf(header.CStr(), "%d", &url_value) != 1)
				url_value = -1;
		}

		LogicalDocument* logdoc = doc->GetLogicalDocument();
		if (logdoc)
		{
			HTML_Element* meta_element = logdoc->GetFirstHE(HE_META);
			while (meta_element)
			{
				OpStringC name = meta_element->GetStringAttr(ATTR_HTTP_EQUIV);
				if (!name.CompareI("preview-refresh"))
				{
					const uni_char *content = meta_element->GetStringAttr(ATTR_CONTENT);
					if (content)
					{
						if (uni_sscanf(content, UNI_L("%d"), &meta_value) != 1)
							meta_value = -1;
					}
					break;
				}
				do
				{
					meta_element = meta_element->NextActual();
				}
				while (meta_element && !meta_element->IsMatchingType(HE_META, NS_HTML));
			}
		}

		if (meta_value > 0)
			m_preview_refresh = meta_value;
		else if(url_value > 0)
			m_preview_refresh = url_value;
		else
			m_preview_refresh = 0;


	}
	return OpStatus::OK;
}

/* static */
OpBitmap* OpThumbnail::ScaleBitmap(OpBitmap* bitmap, int target_width, int target_height, const OpRect& dst, const OpRect& src)
{
    OP_NEW_DBG("OpThumbnail::ScaleBitmap()", "thumbnail");
    OP_DBG(("target-size: %dx%d; rect: (%d,%d)x%d+%d -> (%d,%d)x%d+%d", target_width, target_height, src.x, src.y, src.width, src.height, dst.x, dst.y, dst.width, dst.height));
	OP_ASSERT(dst.x+dst.width  <= target_width  && dst.x >= 0);
	OP_ASSERT(dst.y+dst.height <= target_height && dst.y >= 0);
	OP_ASSERT(src.x+src.width  <= int(bitmap->Width())  && src.x >= 0);
	OP_ASSERT(src.y+src.height <= int(bitmap->Height()) && src.y >= 0);

	// If the source bitmap has alpha or transparent pixels (limited
	// alpha - 0 || 255), then make sure that the result has that
	// too. Since the scale operation is likely to introduce
	// semi-transparent (0 < alpha < 255) pixels, we need to have
	// alpha even if the source is just transparent.
	BOOL needs_alpha = bitmap->IsTransparent() || bitmap->HasAlpha();

	OpBitmap *scaled_bitmap_p = NULL;
	if (OpStatus::IsError(OpBitmap::Create(&scaled_bitmap_p, target_width, target_height, TRUE, needs_alpha, 0, 0, TRUE)))
		return NULL;
	OpAutoPtr<OpBitmap> scaled_bitmap(scaled_bitmap_p);

#ifdef THUMBNAILS_INTERNAL_BITMAP_SCALE
	UINT32* dst_line = OP_NEWA(UINT32, (target_width));
	UINT16* dst_line_2 = OP_NEWA(UINT16, (target_width*4));
	UINT32* src_line = OP_NEWA(UINT32, (bitmap->Width()));
	if (!dst_line || !dst_line_2 || !src_line)
	{
		OP_DELETEA(dst_line);
		OP_DELETEA(dst_line_2);
		OP_DELETEA(src_line);
		return NULL;
	}
	op_memset(dst_line,   0xff, target_width * sizeof(UINT32));
	op_memset(dst_line_2, 0xff, target_width * 4 * sizeof(UINT16));
	int y;
	for (y = 0; y < dst.y; y++)
		scaled_bitmap->AddLine(dst_line, y);
	for (y = dst.y + dst.height; y < target_height; y++)
		scaled_bitmap->AddLine(dst_line, y);

	// Most calculations here are in 16.16 fixpoint format.
	// Most calculations also blatantly assume that the input values are nowhere near their limits
	const INT32 systep = (src.height << 16) / dst.height;
	const INT32 sxstep = (src.width << 16) / dst.width;

	// I'm sorry for inflicting this upon you, dear reader, but it seems unavoidable.
	for (int dy = dst.y; dy < dst.y + dst.height; dy++)
	{
		// Do the down-scaling into an array with 16-bit-per-channel precision (in 8.8 fixpoint format)
		// Each pixel gives a contribution of k*pixel{r,g,b,a}, where k is the inverse of the number of
		// pixels contributing to the output pixel. That is, after the down-scaling, each entry of this
		// array contains the average pixel value in 8.8 fixpoint format.
		op_memset(dst_line_2 + 4*dst.x, 0, dst.width*4*sizeof(UINT16));

		// Calculate the range of y coordinates to average
		const INT32 y1 = src.y + ((dy * systep) >> 16);
		// FIXME Eliminate unnecessary max/min:ing. It is imperative that at least one line be used (i.e. y2 > y1), otherwise we'll have no data to work on and that is clearly wrong
		const INT32 y2 = MIN(MAX(src.y+(((dy+1) * systep) >> 16), y1+1), src.y+src.height);

		// Part 1 of the magical averaging constant 'k' (1 / delta_y)
		const INT32 inv_nlines = (1 << 16) / (y2 - y1);

		for (INT32 sy = y1; sy < y2; sy++)
		{
			bitmap->GetLineData(src_line, sy);
			for (int dx = dst.x; dx < dst.x + dst.width; dx++)
			{
				// See comments for y1 and y2 above. The area between x1..x2 and y1..y2 is the area which will be averaged to produce the output pixel.
				const INT32 x1 = src.x + ((dx * sxstep) >> 16);
				const INT32 x2 = MIN(MAX(src.x+(((dx+1) * sxstep) >> 16), x1+1), src.x+src.width);

				// inv_fact (or 'k') is now 1 / (delta_y * delta_x), i.e. the inverse of the number of pixels.
				const INT32 inv_fact = inv_nlines / (x2 - x1);

				// Now, the actual loop. Each component is multiplied by k and converted from 16.16 format to 8.8 format by a downshift.
				for (INT32 sx = x1; sx < x2; sx++)
				{
					dst_line_2[4*dx+0] += (((unsigned char*)&(src_line[sx]))[0] * inv_fact) >> 8;
					dst_line_2[4*dx+1] += (((unsigned char*)&(src_line[sx]))[1] * inv_fact) >> 8;
					dst_line_2[4*dx+2] += (((unsigned char*)&(src_line[sx]))[2] * inv_fact) >> 8;
					dst_line_2[4*dx+3] += (((unsigned char*)&(src_line[sx]))[3] * inv_fact) >> 8;
				}
			}
		}
		for (int x=dst.x; x < dst.x + dst.width; x++)
		{
			((unsigned char*)&(dst_line[x]))[0] = dst_line_2[4*x+0] >> 8;
			((unsigned char*)&(dst_line[x]))[1] = dst_line_2[4*x+1] >> 8;
			((unsigned char*)&(dst_line[x]))[2] = dst_line_2[4*x+2] >> 8;
			((unsigned char*)&(dst_line[x]))[3] = dst_line_2[4*x+3] >> 8;
		}
		scaled_bitmap->AddLine(dst_line, dy);
	}
	OP_DELETEA(dst_line);
	OP_DELETEA(dst_line_2);
	OP_DELETEA(src_line);
#else // !THUMBNAILS_INTERNAL_BITMAP_SCALE
	OpPainter *scaled_painter = scaled_bitmap->GetPainter();
	scaled_painter->SetColor(0, 0, 0, 0);
	scaled_painter->ClearRect(dst);
	scaled_painter->DrawBitmapScaled(bitmap, src, dst);
	scaled_bitmap->ReleasePainter();
#endif

	return scaled_bitmap.release();
}

char *OpThumbnail::EncodeImagePNG(OpBitmap *bitmap, int &result_len, int nodown)
{
	if (NULL == bitmap)
		return NULL;
	result_len = 0;

	// init png converter
	PngEncFeeder feeder;
	PngEncRes res;
	BOOL again = TRUE;

	minpng_init_encoder_result(&res);
	minpng_init_encoder_feeder(&feeder);
	feeder.has_alpha = 1;
	feeder.scanline = 0;
	feeder.compressionLevel = THUMBNAILS_COMPRESSION_LEVEL;

	feeder.xsize = bitmap->Width();
	feeder.ysize = bitmap->Height();
	feeder.scanline_data = OP_NEWA(UINT32, (bitmap->Width()));
	if (!feeder.scanline_data)
		return NULL;

	ImageBuffer image_buffer;
	if (OpStatus::IsError(image_buffer.Reserve(bitmap->Height()*bitmap->Width()/4)))
	{
		OP_DELETEA(feeder.scanline_data);
		return NULL;
	}

	bitmap->GetLineData(feeder.scanline_data, feeder.scanline);
	OP_STATUS result = OpStatus::OK;
	while (again)
	{
		switch (minpng_encode(&feeder, &res))
		{
		case PngEncRes::AGAIN:
			break;
		case PngEncRes::ILLEGAL_DATA:
			result = OpStatus::ERR;
			again = FALSE;
			break;
		case PngEncRes::OOM_ERROR:
			result = OpStatus::ERR_NO_MEMORY;
			again = FALSE;
			break;
		case PngEncRes::OK:
			again = FALSE;
			break;
		case PngEncRes::NEED_MORE:
			++feeder.scanline;
			if (feeder.scanline >= bitmap->Height())
			{
				OP_DELETEA(feeder.scanline_data);
				return NULL;
			}
			bitmap->GetLineData(feeder.scanline_data, feeder.scanline);
			break;
		}

		if (res.data_size)
		{
			if (OpStatus::IsError(image_buffer.Append(res.data, res.data_size)))
			{
				result = OpStatus::ERR_NO_MEMORY;
				again = FALSE;
			}
		}
		minpng_clear_encoder_result(&res);
	}
	OP_DELETEA(feeder.scanline_data);
	minpng_clear_encoder_feeder(&feeder);

	if (OpStatus::IsError(result))
	{
		return NULL;
	}

	result_len = image_buffer.ActualSize();
	return (char*)image_buffer.TakeOver();
}

/* static */
OpBitmap *OpThumbnail::CreateThumbnail(Window *window, long target_width, long target_height, BOOL auto_width, ThumbnailMode mode, BOOL keep_vertical_scroll)
{
    OP_NEW_DBG("OpThumbnail::CreateThumbnail()", "thumbnail");
	if (target_width <= 0 || target_height <= 0)
		return NULL;

	return CreateThumbnail(window, target_width, target_height,
				FindArea(window, target_width, target_height, auto_width, mode, keep_vertical_scroll));
}

/* static */
OpBitmap* OpThumbnail::CreateThumbnail(Window *window, long target_width, long target_height, const OpRect &area, BOOL high_quality)
{
    OP_NEW_DBG("OpThumbnail::CreateThumbnail()", "thumbnail");
	RenderData rd;
	if (OpStatus::IsError(CalculateRenderData(window, target_width, target_height, area, rd, high_quality)))
		return NULL;

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	PixelScalePainter* painter = OP_NEW(PixelScalePainter, ());
	if (!painter)
		return NULL;

	OpAutoPtr<PixelScalePainter> painter_p(painter);
	OpWindow* opwindow = window->GetOpWindow();
	if (OpStatus::IsError(painter->Construct(rd.zoomed_width, rd.zoomed_height, NULL, opwindow->GetPixelScale())))
		return NULL;
#else
	OpBitmap *bitmap_p = NULL;
	if (OpStatus::IsError(OpBitmap::Create(&bitmap_p, rd.zoomed_width, rd.zoomed_height, TRUE, TRUE, 0, 0, TRUE)))
		return NULL;
	OpAutoPtr<OpBitmap> bitmap(bitmap_p);
	OpPainter *painter = bitmap->GetPainter();
	if (painter == NULL)
		return NULL;
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	painter->SetColor(0, 0, 0, 0);
	painter->ClearRect(OpRect(0, 0, rd.zoomed_width, rd.zoomed_height));
	Paint(window, painter, rd, FALSE);

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	rd.zoomed_width = TO_DEVICE_PIXEL(painter->GetScaler(), rd.zoomed_width);
	rd.zoomed_height = TO_DEVICE_PIXEL(painter->GetScaler(), rd.zoomed_height);
	rd.target_width = TO_DEVICE_PIXEL(painter->GetScaler(), rd.target_width);
	rd.target_height = TO_DEVICE_PIXEL(painter->GetScaler(), rd.target_height);

	OpBitmap *bitmap_p = NULL;
	if (OpStatus::IsError(OpBitmap::Create(&bitmap_p, rd.zoomed_width, rd.zoomed_height, FALSE, FALSE, 0, 0, TRUE)))
		return NULL;
	painter->GetRenderTarget()->copyToBitmap(bitmap_p);
	OpAutoPtr<OpBitmap> bitmap(bitmap_p);
#else
	bitmap->ReleasePainter();
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	if (rd.zoomed_width != rd.target_width)
		return ScaleBitmap(bitmap.get(), rd.target_width, rd.target_height, OpRect(0, 0, rd.target_width, rd.target_height), OpRect(0, 0, rd.zoomed_width, rd.zoomed_height));
	else
		return bitmap.release();
}

/*static*/
OpBitmap* OpThumbnail::CreateSnapshot(Window *window)
{
    OP_NEW_DBG("OpThumbnail::CreateSnapshot()", "thumbnail");
	VisualDevice *vis_dev = window->VisualDev();
	if (vis_dev->GetRenderingViewWidth() <= 0 || vis_dev->GetRenderingViewHeight() <= 0)
		return NULL;

	INT32 width = (vis_dev->GetRenderingViewWidth() * window->GetScale()) / 100;
	INT32 height = (vis_dev->GetRenderingViewHeight() * window->GetScale()) / 100;

	OpBitmap *bitmap_p = NULL;
	if (OpStatus::IsError(OpBitmap::Create(&bitmap_p, width, height, TRUE, TRUE, 0, 0, TRUE)))
		return NULL;

	OpAutoPtr<OpBitmap> bitmap(bitmap_p);

	OpPainter *painter = bitmap->GetPainter();

	Paint(window, painter, width, height);

	bitmap->ReleasePainter();

	return bitmap.release();
}

void OpThumbnail::OnUrlChanged(OpWindowCommander* commander, const uni_char* url)
{
}

void OpThumbnail::OnLoadingFinished(OpWindowCommander* commander, LoadingFinishStatus status)
{
    OP_NEW_DBG("OpThumbnail::OnLoadingFinished()", "thumbnail");
	FramesDocument *doc = m_window->GetCurrentDoc();

	ThumbnailManager::ThumbnailSize size = g_thumbnail_manager->GetThumbnailSize();

	// We won't be getting this for the icon anymore, so we can safely set the title whenever the document loads.
	OpStatus::Ignore(m_title.Set(commander->GetCurrentTitle()));

	if (status == LOADING_SUCCESS && doc && doc->GetTopDocument()->IsLoaded())
	{
		OP_DBG(("this=%p: LOADING_SUCCESS", this));
		SetPreviewRefresh();

		if (SpeedDial == m_mode || SpeedDialRefreshing == m_mode)
		{
			if (commander->IsDocumentSupportingViewMode(WINDOW_VIEW_MODE_MINIMIZED))
			{
				// If the document supports view-mode:minimized, use the document as is and don't try anything else.
				// Set a smaller rendering viewport size however so that the "minimized" content is visible.
				m_mode = OpThumbnail::MinimizedThumbnail;

				m_source_width  = MAX(m_target_width,  size.base_thumbnail_width);
				m_source_height = MAX(m_target_height, size.base_thumbnail_height);

				/* OK. Don't try to reinit the whole thumbnail, it's
				 * pointless, time consuming and silly.  All we need
				 * here is changing the viewport size for the window.
				 */
				ConfigureWindow();
				GenerateThumbnailAsync();
			}
			else if(SpeedDialRefreshing == m_mode)
			{
				// Don't try to be too smart when the speeddial cell is set to autorefresh.
				m_mode = ViewPort;
				GenerateThumbnailAsync();
			}
			else
			{
				// Only look for custom content
				// If a speeddial thumbnail was requested, we want to check first if the document has
				// an icon that would be possible to use.

				// These should perhaps be hardcoded to some minimal size that has sense.
				// Perhaps the apple-touch-icon size of 114x144 would be a good pick.
				const int icon_min_w = OpThumbnail::GetSettingValue(OpThumbnail::LOGO_ICON_MIN_W);
				const int icon_min_h = OpThumbnail::GetSettingValue(OpThumbnail::LOGO_ICON_MIN_H);

				if (m_icon_chooser.IsBusy())
					// We're already waiting for an icon scan result and we don't really care that the document wants to show new content
					return;

				// Initiate scanning the document for icon links
				if (OpStatus::IsError(m_icon_chooser.StartIconFinder(this, m_window, icon_min_w, icon_min_h)))
				{
					// Something went wrong while starting the icon download, try to generate the thumbnail from the
					// document that we just got, try to find logo and it will fallback to the ViewPort mode if unable
					// to find one.
					m_mode = FindLogo;
					GenerateThumbnailAsync();
				}

				// Don't generate a thumbnail, we don't know yet what the thumbnail should be
			}
		}
		else
			GenerateThumbnailAsync();
    }
	else if (status == LOADING_COULDNT_CONNECT || status == LOADING_UNKNOWN)
	{
        OP_DBG(("this=%p: %s", this, (status == LOADING_COULDNT_CONNECT ? "LOADING_COULDNT_CONNECT" : "LOADING_UNKNOWN")));
		// don't destruct when there's a confirm dialog,or it'll trigger a crash.(confirm dialog holds a pointer to window)
		// there will be another OnLoadingFinished message when the dialog ends.
		if(m_window->GetOnlineMode() != Window::USER_RESPONDING)
		{
			OnError(status);
		}
	}
    else
    {
        OP_DBG(("this=%p: status=%d", this, status));
    }
}

void OpThumbnail::OnAuthenticationRequired(OpWindowCommander* commander, OpAuthenticationCallback* callback)
{
	OP_ASSERT(!m_auth_callback);

	m_auth_callback = callback;
	m_auth_timer.Start(THUMBNAIL_AUTH_TIMEOUT);
}

BOOL OpThumbnail::OnLoadingFailed(OpWindowCommander* commander, int msg_id,	 const uni_char* url)
{
    OP_NEW_DBG("OpThumbnail::OnLoadingFailed()", "thumbnail");
    OP_DBG(("this=%p", this));
	OnError(OpLoadingListener::LOADING_UNKNOWN);
	return TRUE;
}

void OpThumbnail::OnTimeOut(OpTimer* timer)
{
    OP_NEW_DBG("OpThumbnail::OnTimeOut()", "thumbnail");
    OP_DBG(("this=%p", this));
    OP_ASSERT(m_window);

	if (timer == &m_generate_timer)
	{
        OP_DBG(("generate-timer; state=%d", m_state));
        OP_ASSERT(m_state == THUMBNAIL_STARTED ||
                  m_state == THUMBNAIL_ACTIVE ||
                  m_state == THUMBNAIL_FINISHED);
		OP_ASSERT(m_window->IsVisibleOnScreen());
		FramesDocument* document = m_window->GetCurrentDoc();
		if(document)
			document = document->GetTopDocument();
		if (!document)
			return;

		m_state = THUMBNAIL_ACTIVE;
		OP_DELETE(m_buffer);
        m_buffer = 0;

		// If this is the first paint attempt we do a "dry paint". This allows us to poll for
		// non-decoded images later on.
		if (m_decode_poll_attemps == 0)
		{
			// Get size, zoom etc.. for the rendering. These does not have to be recalculated
			// in later tries.
			OP_STATUS st = OpStatus::OK;

			ThumbnailManager::ThumbnailSize size = g_thumbnail_manager->GetThumbnailSize();

			if (m_mode == IconThumbnail)
			{
				// We don't need the render data for icon, we just take the whole icon area.
				// Fall-through.
			}
			else if(MinimizedThumbnail == m_mode)
				st = CalculateMinimizedRenderData(size.base_thumbnail_width, size.base_thumbnail_height, m_render_data);
			else
			{
				if (ViewPort == m_mode || FindLogo == m_mode)
				{
					m_target_width  = static_cast<int>(size.base_thumbnail_width  * size.max_zoom);
					m_target_height = static_cast<int>(size.base_thumbnail_height * size.max_zoom);
				}

				OpRect document_area = FindArea(m_window, m_target_width, m_target_height, FALSE, m_mode, m_source_width, m_source_height, &m_render_data.logo_rect);
				st = CalculateRenderData(m_window, THUMBNAIL_INTERNAL_WINDOW_WIDTH, THUMBNAIL_INTERNAL_WINDOW_WIDTH * m_target_height / m_target_width, document_area, m_render_data, m_mode != OpThumbnail::ViewPortLowQuality, FALSE);
			}

			if (OpStatus::IsError(st))
			{
				OnError(OpLoadingListener::LOADING_UNKNOWN);
				return;
			}
			RenderThumbnailDryPaint();
		}

		// Do the real paint if all images are decoded, otherwise exit and try again.
		if (m_decode_poll_attemps >= THUMBNAIL_DECODE_POLL_MAX_ATTEMPTS ||	// Give up if we don't make it in x attempts
			(document->IsLoaded() && document->ImagesDecoded() && !document->GetVisualDevice()->IsLocked()))
		{
			m_buffer = RenderThumbnailInternal();
			if(!m_buffer)
			{
				OnError(OpLoadingListener::LOADING_UNKNOWN);
				return;
			}

			m_state = THUMBNAIL_FINISHED;
			OnFinished(m_buffer);
		}
		else
		{
			m_decode_poll_attemps++;
            OP_DBG(("Restart generate-timer (poll-attempts=%d)", m_decode_poll_attemps));
			m_generate_timer.Start(THUMBNAIL_DECODE_POLL_TIMEOUT);
		}
	}
#ifdef THUMBNAILS_LOADING_TIMEOUT
	else if (timer == &m_load_timer)
	{
        OP_DBG(("load-timer; state=%d", m_state));

		// don't destruct when there's a confirm dialog,or it'll trigger a crash.(confirm dialog holds a pointer to window)
		if(m_window->GetOnlineMode() != Window::USER_RESPONDING)
		{
            if (m_state == THUMBNAIL_STARTED)
			{
				// It's possible that we're still loading icons
				if (SpeedDial == m_mode)
					m_mode = FindLogo;

                GenerateThumbnailAsync();
			}
		}
		else
		{
            OP_DBG(("Restart load-timer"));
			m_load_timer.Start(THUMBNAILS_LOADING_TIMEOUT_SECONDS * 1000);
		}
	}
#endif
	else if (timer == &m_auth_timer)
	{
		OP_ASSERT(m_auth_callback);

		m_auth_callback->CancelAuthentication();
		m_auth_callback = NULL;
	}
}

/* static */
OpRect OpThumbnail::FindDefaultArea(Window* window, long target_width, long target_height, BOOL auto_width, BOOL keep_vertical_scroll)
{
	OpRect ret;
	VisualDevice *vis_dev = window->VisualDev();

	FramesDocument* frames_doc = window->GetCurrentDoc();
	if (!DocumentOk(frames_doc))
		return ret;

	long left = 0;
	long right = vis_dev->GetRenderingViewWidth();

	if (auto_width)
		OpStatus::Ignore(GetDocumentWidth(frames_doc, left, right));

	// do not try to render more than fits in the original window (FlexRoot could cause this)
	long width = MIN(right - left, vis_dev->GetRenderingViewWidth());

	ret.x = left;
	ret.y =	keep_vertical_scroll ? frames_doc->GetLayoutViewY() : 0;
	ret.width = width;
	ret.height = width * target_height / target_width;
	return ret;
}

void OpThumbnail::OnIconCandidateChosen(ThumbnailIconFinder::IconCandidate *best)
{
	m_best_icon = best;
	// All URLs downloading icons have finished or timed out. We have the best candidate available now, or NULL if no icon was good enough.
	if (best)
	{
		// An icon was found that suits our needs, use it. Draw it using the bitmap it provides.
		m_mode = IconThumbnail;
		m_source_width = best->GetWidth();
		m_source_height = best->GetHeight();
		GenerateThumbnailAsync();
	}
	else
	{
		// No icon was found. Use the original page content to generate the thumbnail and find a logo there.
		m_mode = FindLogo;
		GenerateThumbnailAsync();
	}
}

/*static*/
int OpThumbnail::GetSettingValue(SettingValue which)
{
#ifdef THUMBNAILS_LOGO_TUNING_VIA_PREFS
	PrefsCollectionDoc::integerpref pref;
	switch(which)
	{
	case LOGO_SCORE_THRESHOLD:
		pref = PrefsCollectionDoc::ThumbnailLogoScoreThreshold;
		break;
	case LOGO_SCORE_X:
		pref = PrefsCollectionDoc::ThumbnailLogoScoreX;
		break;
	case LOGO_SCORE_Y:
		pref = PrefsCollectionDoc::ThumbnailLogoScoreY;
		break;
	case LOGO_SCORE_LOGO_URL:
		pref = PrefsCollectionDoc::ThumbnailLogoScoreLogoURL;
		break;
	case LOGO_SCORE_LOGO_ALT:
		pref = PrefsCollectionDoc::ThumbnailLogoScoreLogoALT;
		break;
	case LOGO_SCORE_SITE_URL:
		pref = PrefsCollectionDoc::ThumbnailLogoScoreSiteURL;
		break;
	case LOGO_SCORE_SITE_ALT:
		pref = PrefsCollectionDoc::ThumbnailLogoScoreSiteALT;
		break;
	case LOGO_SCORE_SITE_LINK:
		pref = PrefsCollectionDoc::ThumbnailLogoScoreSiteLink;
		break;
	case LOGO_SCORE_BANNER:
		pref = PrefsCollectionDoc::ThumbnailLogoScoreBanner;
		break;
	case LOGO_SIZE_MIN_X:
		pref = PrefsCollectionDoc::ThumbnailLogoSizeMinX;
		break;
	case LOGO_SIZE_MIN_Y:
		pref = PrefsCollectionDoc::ThumbnailLogoSizeMinY;
		break;
	case LOGO_SIZE_MAX_X:
		pref = PrefsCollectionDoc::ThumbnailLogoSizeMaxX;
		break;
	case LOGO_SIZE_MAX_Y:
		pref = PrefsCollectionDoc::ThumbnailLogoSizeMaxY;
		break;
	case LOGO_POS_MAX_X:
		pref = PrefsCollectionDoc::ThumbnailLogoPosMaxX;
		break;
	case LOGO_POS_MAX_Y:
		pref = PrefsCollectionDoc::ThumbnailLogoPosMaxY;
		break;
	case LOGO_ICON_MIN_W:
		pref = PrefsCollectionDoc::ThumbnailIconMinW;
		break;
	case LOGO_ICON_MIN_H:
		pref = PrefsCollectionDoc::ThumbnailIconMinH;
		break;
	default:
		OP_ASSERT(!"Unknown setting!");
		return -1;
		break;
	}
	return g_pcdoc->GetIntegerPref(pref);
#else // THUMBNAILS_LOGO_TUNING_VIA_PREFS
	switch(which)
	{
	case LOGO_SCORE_THRESHOLD:
		return 75;
	case LOGO_SCORE_X:
		return 20;
	case LOGO_SCORE_Y:
		return 35;
	case LOGO_SCORE_LOGO_URL:
		return 75;
	case LOGO_SCORE_LOGO_ALT:
		return 70;
	case LOGO_SCORE_SITE_URL:
		return 20;
	case LOGO_SCORE_SITE_ALT:
		return 35;
	case LOGO_SCORE_SITE_LINK:
		return 20;
	case LOGO_SCORE_BANNER:
		return 90;
	case LOGO_SIZE_MIN_X:
		return 20;
	case LOGO_SIZE_MIN_Y:
		return 15;
	case LOGO_SIZE_MAX_X:
		return 800;
	case LOGO_SIZE_MAX_Y:
		return 199;
	case LOGO_POS_MAX_X:
		return 1200;
	case LOGO_POS_MAX_Y:
		return 300;
	case LOGO_ICON_MIN_W:
		return 114;
	case LOGO_ICON_MIN_H:
		return 114;
	default:
		OP_ASSERT(!"Unknown setting!");
		return -1;
	}
#endif // THUMBNAILS_LOGO_TUNING_VIA_PREFS
}

/* static */
OpRect OpThumbnail::FindArea(Window* window, long target_width, long target_height, BOOL auto_width, ThumbnailMode mode, long source_width, long source_height, OpRect* logo_rect)
{
	OP_ASSERT(window);
	OpRect ret(0, 0, 0, 0);

	switch (mode)
	{
#ifdef THUMBNAILS_LOGO_FINDER
	case OpThumbnail::FindLogo:
		{
			ThumbnailLogoFinder logo_finder;

			if (OpStatus::IsSuccess(logo_finder.Construct(window->GetCurrentDoc())))
				logo_finder.FindLogoRect(target_width, target_height, logo_rect);

			ret = FindDefaultArea(window, target_width, target_height, auto_width, FALSE);

			break;
		}
#endif
	case OpThumbnail::SpeedDial:
		// We don't know a way to render a "SpeedDial", if it's not an icon and not a logo, then
		// render in the ViewPort mode and complain.
		OP_ASSERT(!"Shouldn't happen!");
	case OpThumbnail::ViewPort:
	case OpThumbnail::ViewPortLowQuality:
		{
			ret = FindDefaultArea(window, target_width, target_height, auto_width, FALSE);
			break;
		}
	case OpThumbnail::IconThumbnail:
	case OpThumbnail::MinimizedThumbnail:
		{
			ret = OpRect(0, 0, source_width, source_height);
			break;
		}
	default:
		{
			OP_ASSERT(!"Case not handled, shouldn't get here");
			break;
		}
	}

	return ret;
}

#ifdef THUMBNAILS_LOGO_FINDER
/* static */
OpRect OpThumbnail::FindLogoRect(Window* window, long width, long height)
{
	OpRect logo_rect;
	long left, right;
	ThumbnailLogoFinder logo_finder;
	if (!DocumentOk(window->GetCurrentDoc()) || OpStatus::IsError(GetDocumentWidth(window->GetCurrentDoc(), left, right)))
	{
		left = 0;
		right = 1024;
	}

	if (OpStatus::IsSuccess(logo_finder.Construct(window->GetCurrentDoc())))
		logo_finder.FindLogoRect(width, height, &logo_rect);

	return logo_rect;
}
#endif // THUMBNAILS_LOGO_FINDER

OpBitmap *OpThumbnail::ReleaseBuffer(OpBitmap *buffer)
{
	OP_ASSERT(buffer == m_buffer);
	m_buffer = NULL;
	return buffer;
}

/* static */
OP_STATUS OpThumbnail::CalculateRenderData(Window* window, long target_width, long target_height, OpRect area, RenderData &rd, BOOL high_quality, BOOL shrink_width_if_needed)
{
	if (target_width <= 0 || target_height <= 0 || area.IsEmpty())
		return OpStatus::ERR;

	rd.target_width  = target_width;
	rd.target_height = target_height;

	VisualDevice *vis_dev = window->VisualDev();
	if (vis_dev->GetRenderingViewWidth() <= 0 || vis_dev->GetRenderingViewHeight() <= 0)
		return OpStatus::ERR;

	FramesDocument* frames_doc = window->GetCurrentDoc();
	if (!DocumentOk(frames_doc))
		return OpStatus::ERR;

	rd.left = area.x;
	rd.top  = area.y;
	// do not try to render more than fits in the original window
	long width = MIN(area.width, vis_dev->GetRenderingViewWidth());

	// choose a zoom level so that the final scale is even
	int factor;

	if (width % rd.target_width == 0) // we'll just scale down evenly from 100%
		factor = width / rd.target_width;
	else
		factor = width / rd.target_width + 1; // use the smallest zoom above 100% that scales down evenly

	rd.zoomed_width  = MIN(vis_dev->VisibleWidth(),  rd.target_width  * factor);
	rd.zoomed_height = MIN(vis_dev->VisibleHeight(), rd.target_height * factor);

	// shrink the source dimensions if necessary to keep the aspect ratio
	if (rd.zoomed_height * rd.target_width > rd.zoomed_width  * rd.target_height)
		rd.zoomed_height = rd.zoomed_width * rd.target_height / rd.target_width;
	else if (shrink_width_if_needed)
		rd.zoomed_width  = rd.zoomed_height * rd.target_width / rd.target_height;

	return OpStatus::OK;
}

OP_STATUS OpThumbnail::CalculateMinimizedRenderData(long target_width, long target_height, RenderData &rd)
{
	OP_ASSERT(target_width > 0 && target_height > 0);

	ThumbnailManager::ThumbnailSize size = g_thumbnail_manager->GetThumbnailSize();

	if (m_request_width  < size.base_thumbnail_width ||
		m_request_height < size.base_thumbnail_height)
	{
		// if this assertion fails, we have some aspect ratio issues we need to fix
		OP_ASSERT(m_request_width  < size.base_thumbnail_width &&
		          m_request_height < size.base_thumbnail_height);

		// Use the fixed viewport
		rd.target_width  = size.base_thumbnail_width;
		rd.target_height = size.base_thumbnail_height;

		rd.zoomed_width  = size.base_thumbnail_width;
		rd.zoomed_height = size.base_thumbnail_height;
	}
	else
	{
		rd.target_width  = target_width;
		rd.target_height = target_height;

		rd.zoomed_width  = m_request_width;
		rd.zoomed_height = m_request_height;
	}

	rd.left     = 0;
	rd.top      = 0;
	rd.offset_x = 0;
	rd.offset_y = 0;

	return OpStatus::OK;
}

/*static*/
void OpThumbnail::Paint(Window *window, OpPainter *painter, RenderData &rd, BOOL dry_paint)
{
	VisualDevice *vis_dev = window->VisualDev();

#ifdef _PRINT_SUPPORT_
	if (window->GetPreviewMode())
	{
		// Print preview use a different VisualDevice
		vis_dev = window->DocManager()->GetPrintPreviewVD();
	}
#endif

	OpRect clip_rect;
	if (!dry_paint)
		clip_rect.Set(0, 0, rd.zoomed_width, rd.zoomed_height);

	// Set the zoom for each iframe, fixes document that have frames and need a zoom on the logo at the same time.
	FramesDocElm* fd_elm = window->GetCurrentDoc()->GetIFrmRoot();

	int old_iframe_scale = -1;
	while (fd_elm)
	{
		if (fd_elm->GetVisualDevice())
		{
			// The scale should (?) be the same among all iframes
			old_iframe_scale = fd_elm->GetVisualDevice()->GetScale();
			fd_elm->GetVisualDevice()->SetScale(100);
		}
		fd_elm = fd_elm->Next();
	}
	VDStateNoScale old_scale = vis_dev->BeginScaledPainting(OpRect(), 100);
	int old_view_x = vis_dev->GetRenderingViewX(), old_view_y = vis_dev->GetRenderingViewY();

	vis_dev->TranslateView(rd.left - old_view_x, rd.top - old_view_y);

	const Window_Type old_window_type = window->GetType();
	window->SetType(WIN_TYPE_THUMBNAIL);

	vis_dev->GetContainerView()->Paint(clip_rect, painter, 0, 0, TRUE, FALSE);

	window->SetType(old_window_type);
	vis_dev->TranslateView(old_view_x - rd.left, old_view_y - rd.top);
	vis_dev->EndScaledPainting(old_scale);

	// Restore the old scale, see DSK-334857
	fd_elm = window->GetCurrentDoc()->GetIFrmRoot();
	if (old_iframe_scale != -1)
		while (fd_elm)
		{
			if (fd_elm->GetVisualDevice())
				fd_elm->GetVisualDevice()->SetScale(old_iframe_scale);
			fd_elm = fd_elm->Next();
		}
}

/*static*/
void OpThumbnail::Paint(Window *window, OpPainter *painter, INT32 width, INT32 height)
{
	VisualDevice *vis_dev = window->VisualDev();

	OpRect clip_rect;

	clip_rect.Set(0, 0, width, height);

	const Window_Type old_window_type = window->GetType();
	window->SetType(WIN_TYPE_THUMBNAIL);

	vis_dev->GetContainerView()->Paint(clip_rect, painter, 0, 0, TRUE, FALSE);

	window->SetType(old_window_type);

}

OpBitmap* OpThumbnail::RenderThumbnailInternal()
{
    OP_NEW_DBG("OpThumbnail::RenderThumbnailInternal()", "thumbnail");
    OP_DBG(("this=%p", this));
    OP_ASSERT(m_window);
    OP_ASSERT(m_state == THUMBNAIL_ACTIVE);
	OpBitmap *bitmap_p = NULL;

	int zoomed_width = m_render_data.zoomed_width;
	int zoomed_height = m_render_data.zoomed_height;
	int target_width = m_render_data.target_width;
	int target_height = m_render_data.target_height;
	int bitmap_width = m_render_data.zoomed_width;
	int bitmap_height = m_render_data.zoomed_height;

	if (m_best_icon)
	{
		bitmap_width = m_best_icon->GetWidth();
		bitmap_height = m_best_icon->GetHeight();
	}

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	PixelScalePainter* painter = OP_NEW(PixelScalePainter, ());
	if (!painter)
		return NULL;

	OpAutoPtr<PixelScalePainter> painter_p(painter);
	if (OpStatus::IsError(painter->Construct(bitmap_width, bitmap_height, NULL, m_window->GetPixelScale())))
		return NULL;
#else
#ifdef THUMBNAILS_PREALLOCATED_BITMAPS
	if (m_preallocated_bitmap)
	{
		if (0 <= m_render_data.zoomed_width &&
            (long)m_preallocated_bitmap->Width() == zoomed_width &&
            0 <= m_render_data.zoomed_height &&
            (long)m_preallocated_bitmap->Height() == zoomed_height)
			bitmap_p = m_preallocated_bitmap;
		else
			OP_DELETE(m_preallocated_bitmap);
		m_preallocated_bitmap = NULL;
	}

	if (!bitmap_p)
#endif
		if (OpStatus::IsError(OpBitmap::Create(&bitmap_p, bitmap_width, bitmap_height, TRUE, TRUE, 0, 0, TRUE)))
			return NULL;

	OpAutoPtr<OpBitmap> bitmap(bitmap_p);
	OpPainter* painter = bitmap->GetPainter();

	if (NULL == painter)
		return NULL;
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	painter->SetColor(255, 255, 255, 255);
	painter->FillRect(OpRect(0, 0, bitmap_width, bitmap_height));

	if (m_best_icon)
		painter->DrawBitmapAlpha(m_best_icon->GetBitmap(), OpPoint(0, 0));
	else
		Paint(m_window, painter, m_render_data, FALSE);

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	bitmap_width = TO_DEVICE_PIXEL(painter->GetScaler(), bitmap_width);
	bitmap_height = TO_DEVICE_PIXEL(painter->GetScaler(), bitmap_height);
	zoomed_width = TO_DEVICE_PIXEL(painter->GetScaler(), zoomed_width);
	zoomed_height = TO_DEVICE_PIXEL(painter->GetScaler(), zoomed_height);
	target_width = TO_DEVICE_PIXEL(painter->GetScaler(), target_width);
	target_height = TO_DEVICE_PIXEL(painter->GetScaler(), target_height);
	if (OpStatus::IsError(OpBitmap::Create(&bitmap_p, bitmap_width, bitmap_height,
										   FALSE, FALSE, 0, 0, TRUE)))
		return NULL;

	painter->GetRenderTarget()->copyToBitmap(bitmap_p);
	OpAutoPtr<OpBitmap> bitmap(bitmap_p);
#else
	bitmap->ReleasePainter();
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	if (m_mode != MinimizedThumbnail && m_mode != IconThumbnail && bitmap_width > target_width)
	{
        OP_DBG(("Need to scale the bitmap: %dx%d -> %dx%d", zoomed_width, zoomed_height, target_width, target_height));
#ifdef THUMBNAILS_PREALLOCATED_BITMAPS
# ifdef THUMBNAILS_LOADING_TIMEOUT
        // ensure that we don't get any OnLoadingFinished() or
        // OnLoadingFailed() messages any more.
        OP_DBG(("Clear loading listener"));
        m_window->GetWindowCommander()->SetLoadingListener(NULL);
        // stop the load-timer, so OnTimeOut() is not called anymore
        OP_DBG(("Stop load-timer"));
        m_load_timer.Stop();
# endif // THUMBNAILS_LOADING_TIMEOUT
        OP_DBG(("Call OnCleanup()"));
        // Clean up document to hopefully get some more memory to do
        // the scale (see e.g. ThumbnailManager::OnThumbnailCleanup())
        OnCleanup();
        OP_DBG(("reset window"));
        m_window = 0;
#endif // THUMBNAILS_PREALLOCATED_BITMAPS

		return ScaleBitmap(bitmap_p, target_width, target_height,
                           OpRect(0, 0, target_width, target_height),
                           OpRect(0, 0, zoomed_width, zoomed_height));
    }
	else
		return bitmap.release();
}

void OpThumbnail::RenderThumbnailDryPaint()
{
    OP_NEW_DBG("OpThumbnail::RenderThumbnailDryPaint()", "thumbnail");
    OP_DBG(("this=%p", this));
    OP_ASSERT(m_window);
    OP_ASSERT(m_state == THUMBNAIL_ACTIVE);
    OpBitmap* bitmap;
    if (OpStatus::IsSuccess(OpBitmap::Create(&bitmap, 0, 0, TRUE, TRUE, 0, 0, TRUE)))
    {
		OpPainter* painter = bitmap->GetPainter();
		if (painter == NULL)
			return;

		Paint(m_window, painter, m_render_data, TRUE);
		bitmap->ReleasePainter();
		OP_DELETE(bitmap);
	}
}

void OpThumbnail::GenerateThumbnailAsync()
{
    OP_NEW_DBG("OpThumbnail::GenerateThumbnailAsync()", "thumbnail");
    OP_DBG(("this=%p", this));
    OP_ASSERT(m_window);

	// Use a timer to generate the screenshots since VisDev optimizations
	// caused some problems with frames. Bug #360427.
	m_decode_poll_attemps = 0;
    OP_DBG(("Start generate-timer"));
	m_generate_timer.Start(0);
}

/*static*/
BOOL OpThumbnail::DocumentOk(FramesDocument* frames_doc)
{
	if (frames_doc)
		frames_doc = frames_doc->GetTopDocument();

	if (!frames_doc)
		return FALSE;

	if (!frames_doc->GetLogicalDocument() || !frames_doc->GetLogicalDocument()->GetRoot())
		return FALSE;

	return TRUE;
}

/*static*/
OP_STATUS OpThumbnail::GetDocumentWidth(FramesDocument* doc, long &left, long &right)
{
	if (!doc->GetFrmDocRoot()) // don't try width detection on framed documents
	{
		// restrict our interest to this area
		RECT area = { 0, 0, 1024, 768 };

		WidthTraversalObject wto(doc, area);

		RETURN_IF_ERROR(wto.Traverse(doc->GetLogicalDocument()->GetRoot()));

		if (wto.GetRecommendedHorizontalRange(left, right))
			return OpStatus::OK;
	}

	return OpStatus::ERR;
}

/**
 * OpThumbnail::ImageBuffer
 */

OP_STATUS OpThumbnail::ImageBuffer::Reserve(unsigned int size)
{
	OP_DELETEA(m_image_buffer);

	m_image_buffer = OP_NEWA(unsigned char, (size));
	if (m_image_buffer)
	{
		m_actual_size = 0;
		m_buffer_size = size;
		return OpStatus::OK;
	}

	m_buffer_size = m_actual_size = 0;
	return OpStatus::ERR_NO_MEMORY;
}

OP_STATUS OpThumbnail::ImageBuffer::Append(const unsigned char* data, unsigned int data_size)
{
	if (m_actual_size + data_size > m_buffer_size)
	{
		unsigned char* new_buf = OP_NEWA(unsigned char, (m_actual_size + data_size));
		if (new_buf)
		{
			m_buffer_size = m_actual_size + data_size;
			op_memcpy(new_buf, m_image_buffer, m_actual_size);

			OP_DELETEA(m_image_buffer);
			m_image_buffer=new_buf;
		}
		else
			return OpStatus::ERR_NO_MEMORY;
	}

	op_memcpy(m_image_buffer + m_actual_size, data, data_size);
	m_actual_size += data_size;

	return OpStatus::OK;
}

unsigned char* OpThumbnail::ImageBuffer::TakeOver()
{
	unsigned char* ret = m_image_buffer;

	m_buffer_size = 0;
	m_actual_size = 0;
	m_image_buffer = NULL;

	return ret;
}
#endif // CORE_THUMBNAIL_SUPPORT
