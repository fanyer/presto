/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#if defined(INTERNAL_PNG_SUPPORT) || defined(ASYNC_IMAGE_DECODERS_EMULATION)

#include "modules/img/src/imagedecoderpng.h"

#include "modules/minpng/minpng.h"
#include "modules/minpng/minpng_capabilities.h"
#include "modules/probetools/probepoints.h"

OP_STATUS ImageDecoderPng::DecodeData(const BYTE* data, INT32 numBytes, BOOL more, int& resendBytes, BOOL/* load_all = FALSE*/)
{
	OP_PROBE6(OP_PROBE_IMG_PNG_DECODE_DATA);
	resendBytes = 0;
	OP_STATUS ret = OpStatus::OK;

	m_state->ignore_gamma = m_imageDecoderListener->IgnoreColorSpaceConversions();
	m_state->data = data;
	m_state->len = numBytes;
	BOOL again = TRUE;
	if( numBytes > 0 && !finished )
	{
		while( again )
		{
			int ret_val = minpng_decode( m_state, m_result );
			switch( ret_val )
			{
				case PngRes::OK:
					finished = TRUE;
					/* fall through */
				case PngRes::NEED_MORE:
					again=FALSE;
					/* fall through */
				case PngRes::AGAIN:
					if( m_result->lines && m_result->draw_image )
					{
#ifdef OPERA_BIG_ENDIAN
						int *line_data = OP_NEWA(int, m_result->rect.width);
						if( !line_data )
							return OpStatus::ERR_NO_MEMORY;
#endif // OPERA_BIG_ENDIAN
						if( !main_sent )
						{
							main_sent = TRUE;
							m_imageDecoderListener->OnInitMainFrame( m_result->mainframe_x, m_result->mainframe_y );
#if defined(APNG_SUPPORT)
							if(m_result->num_frames > 1)
								m_imageDecoderListener->OnAnimationInfo(m_result->num_iterations);
#endif // APNG_SUPPORT
#ifdef EMBEDDED_ICC_SUPPORT
							if(m_result->icc_data)
								m_imageDecoderListener->OnICCProfileData(m_result->icc_data,
																		 m_result->icc_data_len);
#endif // EMBEDDED_ICC_SUPPORT
						}
						if(m_result->frame_index != frame_index)
						{
							ImageFrameData image_frame_data;
							image_frame_data.rect.Set(0, 0, m_result->mainframe_x, m_result->mainframe_y);
							if(!image_frame_data.rect.Intersecting(m_result->rect))
								image_frame_data.rect.Empty();
							else
								image_frame_data.rect.IntersectWith(m_result->rect);
							frame_index = m_result->frame_index;
#if defined(APNG_SUPPORT)
							image_frame_data.duration = m_result->duration;
							image_frame_data.disposal_method = m_result->disposal_method;
							image_frame_data.dont_blend_prev = !m_result->blend;
#endif // APNG_SUPPORT
							image_frame_data.interlaced = m_result->interlaced;
							is_decoding_indexed = !!m_result->image_frame_data_pal;
							image_frame_data.alpha = !!m_result->has_alpha;
							image_frame_data.bits_per_pixel = m_result->depth;
							image_frame_data.transparent = m_result->has_transparent_col;
							image_frame_data.transparent_index = m_result->transparent_index;
							image_frame_data.pal = m_result->image_frame_data_pal;
							image_frame_data.num_colors = m_result->pal_colors;
							m_imageDecoderListener->OnNewFrame(image_frame_data);
						}
						int lines_start_x = m_result->rect.x;
						int lines_start_y = m_result->rect.y + m_result->first_line;
						OpRect mainframe_rect = OpRect(0,0,m_result->mainframe_x,m_result->mainframe_y);
						OpRect lines_rect(m_result->rect);
						lines_rect.y = lines_start_y;
						lines_rect.height = m_result->lines;
						lines_rect.IntersectWith(mainframe_rect);
						if(lines_rect.width <= 0)
							lines_rect.height = 0;
						int first_line_report = lines_rect.y - m_result->rect.y;
						lines_rect.x -= lines_start_x;
						lines_rect.y -= lines_start_y;
						int img_width = m_result->rect.width;

						int col_bytes = is_decoding_indexed ? sizeof(UINT8) : sizeof(RGBAGroup);
						void *sl = m_result->image_data.data;
						sl = (void*)((UINT8*)sl + ((lines_rect.y * img_width + lines_rect.x) * col_bytes));
						for (int y = 0; y < lines_rect.height; y++)
						{
#ifdef OPERA_BIG_ENDIAN
							if(!is_decoding_indexed)
							{
								RGBAGroup *_sl = (RGBAGroup*)sl;
								for (int x = 0; x < lines_rect.width; x++)
									line_data[x] = ((_sl[x].a<<24) | (_sl[x].r<<16) | (_sl[x].g<<8) | _sl[x].b);
								m_imageDecoderListener->OnLineDecoded(line_data, first_line_report+y, 1);
							}
							else
#endif  // OPERA_BIG_ENDIAN
							{
								m_imageDecoderListener->OnLineDecoded(sl, first_line_report+y, 1);
							}
							sl = (void*)((UINT8*)sl + img_width*col_bytes);
						}
						minpng_clear_result(m_result);
#ifdef OPERA_BIG_ENDIAN
						OP_DELETEA(line_data);
#endif // OPERA_BIG_ENDIAN
					}
					break;
				case PngRes::OOM_ERROR:
					finished = TRUE;
					return OpStatus::ERR_NO_MEMORY;

				case PngRes::ILLEGAL_DATA:
					finished = TRUE;
					return  OpStatus::ERR;
			}

			if (finished)
				m_imageDecoderListener->OnDecodingFinished();
		}
	}
	return ret;
}

ImageDecoderPng::ImageDecoderPng()
{
	frame_index = -1;
	is_decoding_indexed = FALSE;
	m_imageDecoderListener = NULL;
	main_sent = finished = FALSE;
	m_state = NULL;
	m_result = NULL;

	m_state = OP_NEW(PngFeeder, ());
	if (!m_state)
		return;
	minpng_init_feeder(m_state);

	m_result = OP_NEW(PngRes, ());
	if (!m_result)
		return;
	minpng_init_result(m_result);

#ifndef MINPNG_NO_GAMMA_SUPPORT
	screen_gamma = 2.2;
	m_state->screen_gamma = screen_gamma;
#endif
}


ImageDecoderPng::~ImageDecoderPng()
{
	if (m_result)
	{
		minpng_clear_result( m_result );
		OP_DELETE(m_result);
	}

	if (m_state)
	{
		minpng_clear_feeder( m_state );
		OP_DELETE(m_state);
	}
}



void ImageDecoderPng::SetImageDecoderListener(ImageDecoderListener* imageDecoderListener)
{
	m_imageDecoderListener = imageDecoderListener;
}

#endif // INTERNAL_PNG_SUPPORT
