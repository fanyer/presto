/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef SUPPORT_DATA_SYNC

#include "modules/util/str.h"
#include "modules/util/opstring.h"
#include "modules/sync/sync_transport.h"
#include "modules/sync/sync_factory.h"
#include "modules/sync/sync_parser.h"
#include "modules/sync/sync_parser_myopera.h"
#include "modules/opera_auth/opera_auth_module.h"
#include "modules/opera_auth/opera_auth_oauth.h"

OpSyncTransportProtocol::OpSyncTransportProtocol(OpSyncFactory* factory, OpInternalSyncListener* listener)
	: OpSyncTimeout(MSG_SYNC_LOADING_TIMEOUT)
	, m_factory(factory)
	, m_listener(listener)
	, m_parser(NULL)
	, m_port(0)
	, m_url_dd(NULL)
	, m_new_data(FALSE)
	, m_mh(NULL)
{
	OP_NEW_DBG("OpSyncTransportProtocol::OpSyncTransportProtocol()", "sync");
}

OpSyncTransportProtocol::~OpSyncTransportProtocol()
{
	OP_NEW_DBG("OpSyncTransportProtocol::~OpSyncTransportProtocol()", "sync");
	Abort();
	OP_DELETE(m_url_dd);
	if (m_mh)
	{
		m_mh->UnsetCallBacks(this);
		OP_DELETE(m_mh);
	}
}

OP_STATUS OpSyncTransportProtocol::Init(const OpStringC& server, UINT32 port, OpSyncParser* parser)
{
	OP_NEW_DBG("OpSyncTransportProtocol::Init()", "sync");
	m_parser = parser;
	RETURN_IF_ERROR(m_server.Set(server));
	m_port = port;
	if (!m_mh)
	{
		m_mh = OP_NEW(MessageHandler, (NULL));
		RETURN_OOM_IF_NULL(m_mh);
	}
	return OpStatus::OK;
}

OP_STATUS OpSyncTransportProtocol::Connect(OpSyncDataCollection& items_to_sync, OpSyncState& sync_state)
{
	OP_NEW_DBG("OpSyncTransportProtocol::Connect()", "sync");
	OP_ASSERT(m_mh);
	m_sync_state = sync_state;

	OpString8 xml;
	if (m_parser)
		RETURN_IF_ERROR(static_cast<MyOperaSyncParser*>(m_parser)->GenerateXML(xml, items_to_sync));

	m_host_url = g_url_api->GetURL(m_server.CStr());
	if (!m_host_url.IsEmpty())
	{
		m_host_url.SetHTTP_Method(HTTP_METHOD_POST);
		m_host_url.SetHTTP_Data(xml.CStr(), TRUE);
		m_host_url.SetHTTP_ContentType("text/xml; charset=utf-8");
		RETURN_IF_ERROR(g_opera_oauth_manager->OauthSignUrl(m_host_url, HTTP_METHOD_POST));

		/* Turn off UI interaction if the certificate is invalid and the
		 * servername is the default server. This means that on the default
		 * server, invalid certificates will not show a UI dialog but fail
		 * instead. */
#ifdef URL_DISABLE_INTERACTION
	    if (m_server == DEFAULT_OPERA_LINK_SERVER)
			RETURN_IF_ERROR(m_host_url.SetAttribute(URL::KBlockUserInteraction, TRUE));
#endif

		/* Initiate the download of the document pointed to by the URL, and
		 * forget about it. The HandleCallback function will deal with
		 * successful downloads and check whether a user notification is
		 * required. */
		m_mh->UnsetCallBacks(this);
		m_mh->SetCallBack(this, MSG_HEADER_LOADED, m_host_url.Id());
		m_mh->SetCallBack(this, MSG_URL_DATA_LOADED, m_host_url.Id());
		m_mh->SetCallBack(this, MSG_MULTIPART_RELOAD, m_host_url.Id());
		m_mh->SetCallBack(this, MSG_URL_LOADING_FAILED, m_host_url.Id());
		m_mh->SetCallBack(this, MSG_URL_INLINE_LOADING, m_host_url.Id());
		m_mh->SetCallBack(this, MSG_URL_LOADING_DELAYED, m_host_url.Id());
		m_mh->SetCallBack(this, MSG_URL_MOVED, m_host_url.Id());
		URL tmp;
		m_host_url.Load(m_mh, tmp, TRUE, FALSE);
		/* If we haven't received any parsing data within one minute, assume
		 * something is wrong and abort */
		SetTimeout(SYNC_LOADING_TIMEOUT);
		RETURN_IF_ERROR(OpSyncTimeout::RestartTimeout());
	}

	if (m_listener)
		m_listener->OnSyncStarted(items_to_sync.HasItems());
	return OpStatus::OK;
}

void OpSyncTransportProtocol::OnTimeout()
{
	OP_NEW_DBG("OpSyncTransportProtocol::OnTimeout()", "sync");
	Abort();
}

void OpSyncTransportProtocol::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_NEW_DBG("OpSyncTransportProtocol::HandleCallback()", "sync");
    OP_ASSERT(static_cast<URL_ID>(par1) == m_host_url.Id());

    OpSyncError ret = SYNC_OK;
    BOOL more = TRUE;

    switch (msg)
    {
    case MSG_URL_DATA_LOADED:
		OP_DBG(("MSG_URL_DATA_LOADED"));
		if (m_new_data)
		{
			m_new_data = FALSE;
			ret = m_parser->PrepareNew(m_host_url);
			if (ret != SYNC_OK)
				break;
		}

		if (m_host_url.GetAttribute(URL::KLoadStatus) != URL_LOADED)
			return;
		else
		{
			TempBuffer data;
			data.SetCachedLengthPolicy(TempBuffer::TRUSTED);
			if (!m_url_dd)
			{
				m_url_dd = m_host_url.GetDescriptor(NULL, TRUE);
				if (!m_url_dd)
				{
					more = FALSE;
					ret = SYNC_ERROR_COMM_FAIL;
				}
			}

			while (more)
			{
				TRAPD(err, m_url_dd->RetrieveDataL(more));
				if (OpStatus::IsError(err))
				{
					if (OpStatus::IsMemoryError(err))
						ret = SYNC_ERROR_MEMORY;
					else
						ret = SYNC_ERROR_COMM_FAIL;
					break;
				}
				UINT32 length = m_url_dd->GetBufSize();
				if (OpStatus::IsError(data.Append((uni_char*)m_url_dd->GetBuffer(), UNICODE_DOWNSIZE(length))))
				{
					ret = SYNC_ERROR_MEMORY;
					break;
				}
				m_url_dd->ConsumeData(UNICODE_SIZE(UNICODE_DOWNSIZE(length)));
			}

			// call parser
			if (m_parser && ret == SYNC_OK)
			{
				if (!m_parser->HasParsedNewState())
					m_parser->SetSyncState(m_sync_state);

				ret = m_parser->Parse(data.GetStorage(), data.Length(), m_host_url);
			}

			OP_DELETE(m_url_dd);
			m_url_dd = NULL;
		}
		break;

    case MSG_HEADER_LOADED:
		OP_DBG(("MSG_HEADER_LOADED"));
		m_new_data = TRUE;
		// FALLTHROUGH
    case MSG_MULTIPART_RELOAD:
    case MSG_URL_INLINE_LOADING:
    case MSG_URL_LOADING_DELAYED:
    case MSG_URL_MOVED:
		return;

    case MSG_URL_LOADING_FAILED:
		OP_DBG(("MSG_URL_LOADING_FAILED"));
		ret = SYNC_ERROR_COMM_FAIL;
		break;

    default:
		OpSyncTimeout::HandleCallback(msg, par1, par2);
		return;
    }

    OpSyncTimeout::StopTimeout();
    /* Send the error at the end otherwise the OnSyncError can try and do stuff
	 * before the transfer item is cleaned up */
	NotifyParseResult(ret);
}

void OpSyncTransportProtocol::NotifyParseResult(OpSyncError ret)
{
    OP_ASSERT(m_parser);
    if (m_listener && m_parser)
    {
		if (ret == SYNC_OK)
		{
			OpSyncAllocator* allocator;
			if (OpStatus::IsSuccess(m_factory->GetAllocator(&allocator)))
			{
				OpSyncCollection sync_items_out;
				if (OpStatus::IsSuccess(allocator->CreateSyncItemCollection(*m_parser->GetIncomingSyncItems(), sync_items_out)))
				{
					OpSyncState state;
					m_parser->GetSyncState(state);
					OpSyncServerInformation server_info;
					m_parser->GetSyncServerInformation(server_info);
					m_listener->OnSyncCompleted(&sync_items_out, state, server_info);
				}
				else
				{
					m_listener->OnSyncError(SYNC_ERROR_MEMORY, NULL);
					sync_items_out.Clear();
				}
			}
			else
				m_listener->OnSyncError(SYNC_ERROR_MEMORY, NULL);
		}
		else
		{
			OpString error_msg;
			error_msg.TakeOver(*m_parser->GetErrorMessage());
			m_listener->OnSyncError(ret, error_msg);
		}
    }
}

void OpSyncTransportProtocol::Abort()
{
	OP_NEW_DBG("OpSyncTransportProtocol::Abort()", "sync");
    OpSyncTimeout::StopTimeout();

    if (!m_host_url.IsEmpty() && m_host_url.GetAttribute(URL::KLoadStatus) == URL_LOADING)
    {
		m_host_url.StopLoading(m_mh);
		OP_DELETE(m_url_dd);
		m_url_dd = NULL;

		m_mh->UnsetCallBacks(this);
		if (m_listener)
		{
			m_listener->OnSyncError(SYNC_ERROR_COMM_TIMEOUT, *m_parser->GetErrorMessage());
			// Don't tell about these items next time
			m_parser->ClearIncomingSyncItems(FALSE);
		}
    }
}

#endif // SUPPORT_DATA_SYNC
