/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/widgetruntime/GadgetHelpLoader.h"
#include "modules/pi/system/OpPlatformViewers.h"
#include "modules/url/url_man.h"


GadgetHelpLoader::~GadgetHelpLoader()
{
	StopLoading();
}


OP_STATUS GadgetHelpLoader::Init(const OpStringC& topic)
{
	StopLoading();
	
	const OpMessage messages[] = {
#ifdef _DEBUG
		MSG_URL_MOVED,
#endif // _DEBUG
		MSG_HEADER_LOADED,
		MSG_URL_LOADING_FAILED
	};
	RETURN_IF_ERROR(g_main_message_handler->SetCallBackList(
				this, 0, messages, ARRAY_SIZE(messages)));

	RETURN_IF_ERROR(StartLoading(topic));

	return OpStatus::OK;
}


OP_STATUS GadgetHelpLoader::StartLoading(const OpStringC& topic)
{
	OpString url_name;
	RETURN_IF_ERROR(url_name.SetConcat(
				UNI_L("opera:/help/"), topic));

	m_url = urlManager->GetURL(url_name);
	RETURN_IF_ERROR(m_url.SetAttribute(URL::KHTTP_Method, HTTP_METHOD_GET));
	m_url.Load(g_main_message_handler, URL(), TRUE, FALSE, FALSE, FALSE);

	return OpStatus::OK;
}


void GadgetHelpLoader::StopLoading()
{
	if (IsLoading())
	{
		m_url.StopLoading(g_main_message_handler);
	}

	g_main_message_handler->UnsetCallBacks(this);
}


BOOL GadgetHelpLoader::IsLoading()
{
	return m_url.Status(TRUE) == URL_LOADING;
}


void GadgetHelpLoader::HandleCallback(OpMessage msg, MH_PARAM_1 par1,
		MH_PARAM_2 par2)
{
	switch (msg)
	{
#ifdef _DEBUG
		case MSG_URL_MOVED:
			printf("Help: moved to %s\n",
					m_url.GetAttribute(URL::KName, TRUE).CStr());
			break;
#endif // _DEBUG

		case MSG_HEADER_LOADED:
		case MSG_URL_LOADING_FAILED:
		{
			StopLoading();

			const OpStringC& final_url_name =
					m_url.GetAttribute(URL::KUniName, TRUE);
			OpStatus::Ignore(g_op_platform_viewers->OpenInDefaultBrowser(
						final_url_name.CStr()));
			break;
		}

		default:
			OP_ASSERT(!"Unexpected message");
	}
}


#endif // WIDGET_RUNTIME_SUPPORT
