/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/doc/imageprogresshandler.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/mh.h"

#ifdef ASYNC_IMAGE_DECODERS_EMULATION
#include "modules/img/src/imagemanagerimp.h"
#endif // ASYNC_IMAGE_DECODERS_EMULATION

ImageProgressHandler* ImageProgressHandler::Create()
{
	ImageProgressHandler* handler = OP_NEW(ImageProgressHandler, ());
	if (handler != NULL)
	{
		if (OpStatus::IsError(g_main_message_handler->SetCallBack(handler, MSG_IMG_CONTINUE_DECODING, 0)))
		{
			OP_DELETE(handler);
			return NULL;
		}
	}
	return handler;
}

ImageProgressHandler::~ImageProgressHandler()
{
	g_main_message_handler->UnsetCallBacks(this);
}

void ImageProgressHandler::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	m_pending_progress = FALSE;
	OP_ASSERT(msg == MSG_IMG_CONTINUE_DECODING);
	if (msg == MSG_IMG_CONTINUE_DECODING
#ifdef ASYNC_IMAGE_DECODERS_EMULATION
		&& par1 == 0
#endif // ASYNC_IMAGE_DECODERS_EMULATION
		)
	{
		imgManager->Progress();
	}
#ifdef ASYNC_IMAGE_DECODERS_EMULATION
	else if (msg == MSG_IMG_CONTINUE_DECODING && par1 == 1)
	{
		((ImageManagerImp*)imgManager)->MoreBufferedData();
	}
	else if (msg == MSG_IMG_CONTINUE_DECODING && par1 == 2)
	{
		((ImageManagerImp*)imgManager)->MoreDeletedDecoders();
	}
#endif // ASYNC_IMAGE_DECODERS_EMULATION
}

void ImageProgressHandler::OnProgress()
{
	if (!m_pending_progress)
	{
		m_pending_progress = TRUE;
		g_main_message_handler->PostMessage(MSG_IMG_CONTINUE_DECODING, 0, 0);
	}
}

#ifdef ASYNC_IMAGE_DECODERS_EMULATION
void ImageProgressHandler::OnMoreBufferedData()
{
	g_main_message_handler->PostMessage(MSG_IMG_CONTINUE_DECODING, 1, 0);
}

void ImageProgressHandler::OnMoreDeletedDecoders()
{
	g_main_message_handler->PostMessage(MSG_IMG_CONTINUE_DECODING, 2, 0);
}

#endif // ASYNC_IMAGE_DECODERS_EMULATION

ImageProgressHandler::ImageProgressHandler()
	: m_pending_progress(FALSE)
{
}
