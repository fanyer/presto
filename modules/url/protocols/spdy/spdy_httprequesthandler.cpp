/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Kajetan Switalski
**
*/

#include "core/pch.h"

#ifdef USE_SPDY

#include "modules/url/url_tools.h"
#include "modules/url/protocols/spdy/spdy_httprequesthandler.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_common.h"
#include "modules/url/protocols/http_met.h"
#include "modules/url/protocols/spdy/spdy_connection.h"
#include "modules/url/url_man.h"
#include "modules/util/datefun.h"
#include "modules/content_filter/content_filter.h"
#ifdef SCOPE_RESOURCE_MANAGER
# include "modules/scope/scope_network_listener.h"
#endif // SCOPE_RESOURCE_MANAGER

SpdyHTTPRequestHandler::SpdyHTTPRequestHandler(HTTP_Request *req, MessageHandler *mh, SpdyConnection *master):
	ProtocolComm(mh, NULL, NULL), 
	request(req), 
	streamId(0), 
	headersLoaded(FALSE),
	master(master),
	postedUploadInfoMsg(FALSE),
	uploadProgressDelta(0)
{
	request->SetNewSink(this);
	if (HasDataToSend())
	{
		Upload_Base *body = request->GetElement();
		body->SetOnlyOutputBody(TRUE);
	}
	OpStatus::Ignore(mh->SetCallBack(this, MSG_SPDY_UPLOADINFO, reinterpret_cast<MH_PARAM_1>(this)));
}

SpdyHTTPRequestHandler::~SpdyHTTPRequestHandler()
{ 
	mh->UnsetCallBack(this, MSG_SPDY_UPLOADINFO);
	if (request)
		request->PopSink();
}

size_t SpdyHTTPRequestHandler::ComposeHeadersString(const SpdyHeaders &spdyHeaders, SpdyVersion version, char *target, BOOL measureOnly)
{
	char *targetIt = target;
	for (UINT32 i = 0; i < spdyHeaders.headersCount; ++i)
	{
		UINT16 nameLen = spdyHeaders.headersArray[i].name.length;
		const char *namePtr = spdyHeaders.headersArray[i].name.str;

		UINT16 valueLen = spdyHeaders.headersArray[i].value.length;
		const char *valuePtr = spdyHeaders.headersArray[i].value.str;

		//In SPDY duplicate header names are not allowed. To send two identically named headers, a header with two values is sent and
		//the values are separated by a single NUL (0) byte.
		const char *it2 = valuePtr, *valueEndPtr = valuePtr + valueLen;
		do
		{
			const char *nulCharPos = reinterpret_cast<const char*>(op_memchr(it2, 0, valueEndPtr - it2));
			if (!nulCharPos)
				nulCharPos = valueEndPtr;

			if (!measureOnly)
				op_memcpy(targetIt, namePtr, nameLen);
			targetIt += nameLen;

			if (!measureOnly)
				op_memcpy(targetIt, "=", 1);
			targetIt += 1;
			
			if (!measureOnly)
				op_memcpy(targetIt, it2, nulCharPos - it2);
			targetIt += nulCharPos - it2;
			
			if (!measureOnly)
				op_memcpy(targetIt, "\r\n", 2);
			targetIt += 2;

			it2 = nulCharPos + 1;
		} while (it2 < valueEndPtr);

		if (!measureOnly)
		{
			if (version == SPDY_V2 && op_strnicmp(namePtr, "status", 6) == 0 || version == SPDY_V3 && op_strnicmp(namePtr, ":status", 7) == 0)
			{
				ANCHORD(OpString8, status);
				SPDY_LEAVE_IF_ERROR(status.Set(valuePtr, valueLen));
				request->GetHeader()->response = op_atoi(status.CStr());
			}
			else if(op_strnicmp(namePtr, "content-length", 14) == 0)
			{
				ANCHORD(OpString8, contentLength);
				SPDY_LEAVE_IF_ERROR(contentLength.Set(valuePtr, valueLen));
				if(OpStatus::IsError(StrToOpFileLength(contentLength.CStr(), &request->GetHeader()->content_length)))
					request->GetHeader()->content_length = 0;
				else
					request->GetHeader()->info.received_content_length = TRUE;
			}
		}
	}
	if (!measureOnly)
		op_memcpy(targetIt, "\r\n\0", 3);
	targetIt += 3;
	return targetIt - target;
}

void SpdyHTTPRequestHandler::IfUnfinishedMoveToNewConnection()
{
	if (request)
	{
		ProtocolComm::SetProgressInformation(RESTART_LOADING,0,0);
		request->PopSink();
		request->Out();
		if(request->GetSendCount() >=5 || master->GetManager()->AddRequest(request) == COMM_REQUEST_FAILED)
		{
			request->EndLoading();
#ifdef SCOPE_RESOURCE_MANAGER
			SetProgressInformation(REPORT_LOAD_STATUS, OpScopeResourceListener::LS_RESPONSE_FAILED, request);
#endif // SCOPE_RESOURCE_MANAGER
			mh->PostMessage(MSG_COMM_LOADING_FAILED, request->Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
		}
		request = NULL;
		master->RemoveHandler(this);
	}
}

void SpdyHTTPRequestHandler::HandleTrailingHeadersL(const SpdyHeaders &headers, SpdyVersion version)
{
	size_t headersPayloadSize = ComposeHeadersString(headers, version, NULL, TRUE);
	char *headersPayload = OP_NEWA_L(char, headersPayloadSize);
	ANCHOR_ARRAY(char, headersPayload);
	ComposeHeadersString(headers, version, headersPayload, FALSE);

	request->GetHeader()->trailingheaders.SetKeywordList(KeywordIndex_HTTP_MIME);
	request->GetHeader()->trailingheaders.Sequence_Splitter::SetValueL(headersPayload, NVS_TAKE_CONTENT | NVS_SEP_CRLF_CONTINUED | NVS_VAL_SEP_ASSIGN | NVS_ONLY_SEP | NVS_ACCEPT_QUOTE_IN_NAME);
	request->GetHeader()->info.has_trailer = TRUE;
	ANCHOR_ARRAY_RELEASE(headersPayload);

#ifdef WEB_TURBO_MODE
	// Handle Turbo Proxy compression ratio indicator
	HeaderEntry* header = request->GetHeader()->trailingheaders.GetHeader("X-OC", NULL, HEADER_RESOLVE);
	if(header && header->Value())
	{
		op_sscanf(header->Value(), "%d/%d", &request->GetHeader()->turbo_transferred_bytes, &request->GetHeader()->turbo_orig_transferred_bytes);
#ifdef URL_NETWORK_DATA_COUNTERS
		g_network_counters.turbo_savings += (INT64)request->GetHeader()->turbo_orig_transferred_bytes - (INT64)request->GetHeader()->turbo_transferred_bytes;
#endif // URL_NETWORK_DATA_COUNTERS
	}
#endif // WEB_TURBO_MODE
	ProtocolComm::SetProgressInformation(HEADER_LOADED);
}

void SpdyHTTPRequestHandler::OnHeadersLoadedL(const SpdyHeaders &headers, SpdyVersion version)
{
	NormalCallCount blocker(this);
	if (!request || !request->GetHeader())
		return;

	// If headers are loading again then probably HEADERS frame has been loaded. We should treat it in the same way as trailing headers for chunked encoding in HTTP.
	if (headersLoaded)
	{
		HandleTrailingHeadersL(headers, version);
		return;
	}

	size_t headersPayloadSize = ComposeHeadersString(headers, version, NULL, TRUE);
	char *headersPayload = OP_NEWA_L(char, headersPayloadSize);
	ANCHOR_ARRAY(char, headersPayload);
	ComposeHeadersString(headers, version, headersPayload, FALSE);
	
	request->GetHeader()->headers.SetKeywordList(KeywordIndex_HTTP_MIME);
	request->GetHeader()->headers.Sequence_Splitter::SetValueL(headersPayload, NVS_TAKE_CONTENT | NVS_SEP_CRLF_CONTINUED | NVS_VAL_SEP_ASSIGN | NVS_ONLY_SEP | NVS_ACCEPT_QUOTE_IN_NAME);
	if(request->GetHeader()->response == HTTP_PARTIAL_CONTENT && request->GetHeader()->info.received_content_length && request->GetRangeStart() != FILE_LENGTH_NONE)
		request->GetHeader()->content_length += request->GetRangeStart();
	request->GetHeader()->info.spdy = TRUE;
	ANCHOR_ARRAY_RELEASE(headersPayload);
	headersLoaded = TRUE;
	headersBuffer.Clear();

#ifdef SCOPE_RESOURCE_MANAGER
	request->SetProgressInformation(REPORT_LOAD_STATUS, OpScopeResourceListener::LS_RESPONSE, request);
	HTTP_Request::LoadStatusData_Raw load_status_param(request, "SPDY DATA", sizeof("SPDY DATA"));
	request->SetProgressInformation(REPORT_LOAD_STATUS, OpScopeResourceListener::LS_RESPONSE_HEADER, &load_status_param);
#endif // SCOPE_RESOURCE_MANAGER

#ifdef WEB_TURBO_MODE
	if (master->GetTurboOptimized())
	{
		if (request->GetHeader()->response == HTTP_SERVICE_UNAVAILABLE)
			HandleBypassIfNeeded();

		BOOL shouldClearRequest = FALSE;
		master->HandleTurboHeadersL(request->GetHeader(), shouldClearRequest);

		if (request->GetBypassTurboProxy())
		{
			request->LoadingFinished(FALSE);
			request->PopSink();
			request = NULL;
			return;
		}

		if (shouldClearRequest)
		{
			request->Clear();
			request->PopSink();
			master->AddRequest(request);
			request = NULL;
			master->RemoveHandler(this);
		}
	}
	if (request)
#endif // WEB_TURBO_MODE
	{
		if(request->GetMethod() != HTTP_METHOD_CONNECT)
		{
			SecurityLevel sec_level = static_cast<SecurityLevel>(master->GetSecurityLevel());
			if(sec_level != SECURITY_STATE_UNKNOWN)
			{
				master->ChangeParent(request);
				master->UpdateSecurityInformation();
				master->ChangeParent(NULL);
			}
		}
		ProtocolComm::SetProgressInformation(HEADER_LOADED);
	}
}

void SpdyHTTPRequestHandler::OnDataLoadedL(const OpData &d)
{
	NormalCallCount blocker(this);
	SPDY_LEAVE_IF_ERROR(data.Append(d));
	ProtocolComm::ProcessReceivedData();
}

void SpdyHTTPRequestHandler::OnDataLoadingFinished()
{
	NormalCallCount blocker(this);
	if (request)
	{
		ProtocolComm::SetProgressInformation(REQUEST_FINISHED);
		request->LoadingFinished();
		request->PopSink();
		request->Out();
		request = NULL;
	}
	master->RemoveHandler(this);
}

unsigned int SpdyHTTPRequestHandler::ReadData(char* buf, unsigned blen)
{
	NormalCallCount blocker(this);
	unsigned int dataToRead = MIN(blen, data.Length());
	data.CopyInto(buf, dataToRead);
	data.Consume(dataToRead);
	return dataToRead;
}

UINT8 SpdyHTTPRequestHandler::GetPriority() const
{
	return request->GetPriority() << 3;
}

BOOL SpdyHTTPRequestHandler::HasDataToSend() const
{
	return GetMethodHasRequestBody(request->GetMethod()) && request->GetElement();
}

size_t SpdyHTTPRequestHandler::GetNextDataChunkL(char *buffer, size_t bufferLength, BOOL &more)
{
	OP_ASSERT(HasDataToSend());
	size_t len = request->GetElement()->GetNextContentBlock(reinterpret_cast<unsigned char*>(buffer), bufferLength, more);

#ifdef SCOPE_RESOURCE_MANAGER
	if (!more)
		request->SetProgressInformation(REPORT_LOAD_STATUS, OpScopeResourceListener::LS_REQUEST_FINISHED, request);
#endif // SCOPE_RESOURCE_MANAGER
	
	uploadProgressDelta += len;
	if (!postedUploadInfoMsg)
	{
		postedUploadInfoMsg = TRUE;
		mh->PostMessage(MSG_SPDY_UPLOADINFO, reinterpret_cast<MH_PARAM_1>(this), 0);
	}
	return len;
}

void SpdyHTTPRequestHandler::OnResetStream()
{
	NormalCallCount blocker(this);
	if (request)
	{
		request->LoadingFinished(FALSE);
		request->PopSink();
		request = NULL;
	}
	master->RemoveHandler(this);
}

void SpdyHTTPRequestHandler::OnGoAway()
{
	IfUnfinishedMoveToNewConnection();
}

int SpdyHTTPRequestHandler::AddHeadersL(SpdyVersion version, Header_List& headers, SpdyHeaders::HeaderItem *headersArray) const
{
	int headersCounter = 0;
	for (Header_Item *header_item = headers.First(); header_item; header_item = header_item->Suc())
	{
		if (!header_item->IsEnabled() || !header_item->HasParameters())
			continue;

		if (headersArray)
		{
			size_t nameLength = op_strlen(header_item->GetName());
			char *name = OP_NEWA_L(char, nameLength+1);
			op_strcpy(name, header_item->GetName());
			op_strlwr(name);
			headersArray[headersCounter].name.Init(name, FALSE, nameLength);

			const Header_Parameter_Collection &params = header_item->GetParameters();
			size_t paramsLength = params.CalculateLength() + 1;
			char* value = OP_NEWA_L(char, paramsLength);
			params.OutputParameters(value);
			if (op_strcmp("cookie", name) == 0)
				value[(paramsLength++)-1] = ';';
			headersArray[headersCounter].value.Init(value, FALSE, paramsLength-1);
		}
		++headersCounter;
	}
	return headersCounter;
}

SpdyHeaders *SpdyHTTPRequestHandler::GetHeadersL(SpdyVersion version) const
{
	SpdyHeaders *headers = OP_NEW_L(SpdyHeaders, ());
	ANCHOR_PTR(SpdyHeaders, headers);
	
	request->PrepareHeadersL();
	Header_List &requestHeaders = request->AccessHeaders();
	requestHeaders.RemoveHeader("Connection");
	requestHeaders.RemoveHeader("Keep-alive");
	if (version == SPDY_V3)
		requestHeaders.RemoveHeader("Host");

	if (version == SPDY_V2)
	{
		requestHeaders.ClearAndAddParameterL("method", request->GetMethodString());
		requestHeaders.ClearAndAddParameterL("scheme", request->GetSchemeString());
		requestHeaders.ClearAndAddParameterL("version", "HTTP/1.1");
		requestHeaders.ClearAndAddParameterL("url", request->GetAbsolutePath());
	}
	else
	{
		requestHeaders.ClearAndAddParameterL(":method", request->GetMethodString());
		requestHeaders.ClearAndAddParameterL(":scheme", request->GetSchemeString());
		requestHeaders.ClearAndAddParameterL(":version", "HTTP/1.1");
		requestHeaders.ClearAndAddParameterL(":path", request->GetAbsolutePath());
		if (request->GetIsSecure() && request->Origin_HostPort() == 443 || !request->GetIsSecure() && request->Origin_HostPort() == 80)
			requestHeaders.ClearAndAddParameterL(":host", request->Origin_HostName());
		else
		{
			ANCHORD(OpString8, hostport);
			SPDY_LEAVE_IF_ERROR(hostport.AppendFormat("%s:%d", request->Origin_HostName(), request->Origin_HostPort()));
			requestHeaders.ClearAndAddParameterL(":host", hostport);
		}
	}

	// count the headers without adding them to the structure yet
	int headersCounter = AddHeadersL(version, request->AccessHeaders(), NULL);
	if (HasDataToSend())
		headersCounter += AddHeadersL(version, request->GetElement()->AccessHeaders(), NULL);
	headers->InitL(headersCounter);
	
	// now fill the structure with headers
	headersCounter = AddHeadersL(version, request->AccessHeaders(), headers->headersArray);
	if (HasDataToSend())
		headersCounter += AddHeadersL(version, request->GetElement()->AccessHeaders(), headers->headersArray + headersCounter);

#ifdef SCOPE_RESOURCE_MANAGER
	HTTP_Request::LoadStatusData_Compose load_status_param1(request, request->GetRequestNumber());
	request->SetProgressInformation(REPORT_LOAD_STATUS, OpScopeResourceListener::LS_COMPOSE_REQUEST, &load_status_param1);
	HTTP_Request::LoadStatusData_Raw load_status_param2(request, "SPDY DATA", sizeof("SPDY DATA"));
	request->SetProgressInformation(REPORT_LOAD_STATUS, OpScopeResourceListener::LS_REQUEST, &load_status_param2);
#endif // SCOPE_RESOURCE_MANAGER

	ANCHOR_PTR_RELEASE(headers);
	return headers;
 }

#ifdef WEB_TURBO_MODE

void SpdyHTTPRequestHandler::HandleBypassIfNeeded()
{
	OP_ASSERT(request);

	// Find out the 503 reason
	HeaderEntry *header = request->GetHeader()->headers.GetHeader("X-Opera-Info", NULL, HEADER_RESOLVE);
	if(header && header->Value())
	{
		ParameterList *req_parameters = header->GetParameters(PARAM_SEP_COMMA);
		if(req_parameters)
		{
			if(request->GetHeader()->response == HTTP_SERVICE_UNAVAILABLE)
			{
				Parameters *key = req_parameters->GetParameter("e", PARAMETER_ASSIGNED_ONLY);
				if(key && key->Value())
					HandleBypassResponse(op_atoi(key->Value()));
			}
		}
	}

	// Disable the use of Turbo proxy for this domain for a while
	if(request->GetBypassTurboProxy())
	{
		header = request->GetHeader()->headers.GetHeader("Retry-After", NULL, HEADER_RESOLVE);
		if(header && header->Value())
		{
			OpStringC8 hdr_val(header->Value());
			time_t retry_time = 0;
			if(hdr_val.SpanOf("1234567890") == hdr_val.Length())
				retry_time = g_timecache->CurrentTime() + op_atoi(hdr_val.CStr());
			else
				retry_time = ::GetDate(hdr_val.CStr());

			if( retry_time != 0 )
				g_pcnet->OverridePrefL(request->Origin_HostUniName(), PrefsCollectionNetwork::UseWebTurbo, (int)retry_time, FALSE);
		}
	}
}

void SpdyHTTPRequestHandler::HandleBypassResponse(int errorCode)
{
	OP_ASSERT(request);

	if (errorCode == WEB_TURBO_BYPASS_RESPONSE)
	{
#ifdef URL_FILTER
		OpString site_url;
		// Add server to filter with wildcard
		if (OpStatus::IsSuccess(site_url.AppendFormat(UNI_L("http://%s/*"), request->Origin_HostUniName())))
		{
			TRAPD(err, g_urlfilter->AddFilterL(site_url.CStr()));
			OpStatus::Ignore(err);
		}
#endif // URL_FILTER
	}
	request->SetBypassTurboProxy(TRUE);
	mh->PostMessage(MSG_COMM_LOADING_FAILED, request->Id(), ERR_COMM_RELOAD_DIRECT);
}
#endif // WEB_TURBO_MODE

void SpdyHTTPRequestHandler::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg)
	{
	case MSG_SPDY_UPLOADINFO:
		if (request)
		{
			request->SetProgressInformation(UPLOAD_COUNT, MAKELONG(request->UploadedFileCount(), request->FileCount()), NULL);
			request->SetProgressInformation(UPLOADING_PROGRESS, uploadProgressDelta, request->HostName()->UniName());
			postedUploadInfoMsg = FALSE;
			uploadProgressDelta = 0;
		}
	}
}

#endif // USE_SPDY
