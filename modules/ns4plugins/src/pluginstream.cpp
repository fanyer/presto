/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef _PLUGIN_SUPPORT_

#include "modules/formats/hdsplit.h"
#include "modules/util/str.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/doc/frm_doc.h"
#include "modules/util/handy.h"
#include "modules/util/filefun.h"
#include "modules/dochand/win.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/ns4plugins/src/pluginhandler.h"
#include "modules/ns4plugins/src/pluginstream.h"
#include "modules/ns4plugins/src/plugin.h"
#include "modules/ns4plugins/src/pluginexceptions.h"

PluginStream::PluginStream(Plugin* plugin, int nid, URL& curl, BOOL notify, BOOL loadInTarget)
	: m_plugin(plugin)
	, url_data_desc(NULL)
	, m_url(curl)
	, id(nid)
	, stream(NULL)
	, stype(0)
	, finished(FALSE)
	, notify(notify)
	, loadInTarget(loadInTarget)
	, m_lastposted(INIT)
	, m_lastcalled(INIT)
	, m_mimetype(NULL)
	, m_reason(NPRES_DONE)
	, m_js_eval(NULL)
	, m_js_eval_length(0)
	, m_notify_url_name(NULL)
	, m_msg_flush_counter(0)
{
}

PluginStream::~PluginStream()
{
	m_plugin->OnStreamDeleted(this);

	if (stream)
	{
		OP_DELETEA((char*)stream->url);

		if (stream->headers)
			OP_DELETEA((char*)stream->headers);
		if (m_mimetype)
			OP_DELETEA(m_mimetype);
		if (m_js_eval)
		{
			op_free(m_js_eval);
			m_js_eval = 0;
		}
		OP_DELETE(stream);
	}

	OP_DELETE(url_data_desc);

	if (m_notify_url_name)
		OP_DELETEA((char*)m_notify_url_name);
}

OP_STATUS PluginStream::CreateStream(void* notify_data)
{
	stream = OP_NEW(NPStream, ());
	if (stream)
	{
		stream->pdata = NULL;
		stream->ndata = this; // identification of the stream when the plugin is calling NPN_RequestRead()
		if(!m_url->IsEmpty())
		{
			OpString8 url_name; ANCHOR(OpString8, url_name);
			RETURN_IF_ERROR(m_url->GetAttribute(URL::KName_With_Fragment_Username_Password_Hidden_Escaped, url_name, FALSE));
			stream->url = SetNewStr_NotEmpty(url_name.CStr());
			if (!stream->url)
			{
				OP_DELETE(stream);
				stream = NULL;
				return OpStatus::ERR_NO_MEMORY;
			}
		}
		else
		{
			stream->url = OP_NEWA(char, 1);
			if (!stream->url)
			{
				OP_DELETE(stream);
				stream = NULL;
				return OpStatus::ERR_NO_MEMORY;
			}
			((char*)stream->url)[0] = '\0';
		}
		stream->notifyData = notify_data;
		stream->headers = NULL;
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

OP_STATUS PluginStream::CreateNPStreamHeaders()
{
	UINT32 status = m_url->GetAttribute(URL::KHTTP_Response_Code, TRUE);
	if (status == HTTP_NO_RESPONSE)
		// we'll have no luck getting headers for this one (could be a local file)
		return OpStatus::OK;

	// add response header status, ending with '\n'
	OpString8 hdbuf;
	RETURN_IF_ERROR(hdbuf.Set("HTTP/1.1 "));

	char response_status [12];
	op_itoa(status, response_status, 10);
	RETURN_IF_ERROR(hdbuf.Append(response_status));
	RETURN_IF_ERROR(hdbuf.Append(" "));

	OpString8 statusText8;
	RETURN_IF_ERROR(m_url->GetAttribute(URL::KHTTPResponseText, statusText8, TRUE));
	RETURN_IF_ERROR(hdbuf.Append(statusText8));

	RETURN_IF_ERROR(hdbuf.Append("\n"));

	// add response header's name and value pairs, each separated with '\n'
	HeaderList header_list;
	URL moved_to_url = m_url->GetAttribute(URL::KMovedToURL, TRUE);
	TRAPD(stat, moved_to_url.IsValid() ? moved_to_url.CopyAllHeadersL(header_list) : m_url->CopyAllHeadersL(header_list));
	if (OpStatus::IsError(stat))
		return stat;

	HeaderEntry *hdr = header_list.First();
	while (hdr)
	{
		RETURN_IF_ERROR(hdbuf.Append(hdr->Name(),op_strlen(hdr->Name())));
		RETURN_IF_ERROR(hdbuf.Append(": "));
		RETURN_IF_ERROR(hdbuf.Append(hdr->Value(),op_strlen(hdr->Value())));
		RETURN_IF_ERROR(hdbuf.Append("\n"));

		hdr = hdr->Suc();
	}

	// add response header's buffer end '0'
	RETURN_IF_ERROR(hdbuf.Append("\0"));

	// store response header buffer in NPStream structure
	stream->headers = OP_NEWA(char, hdbuf.Length() + 1);
	RETURN_OOM_IF_NULL(stream->headers);

	op_strcpy((char*)stream->headers, hdbuf.CStr()); // hdbuf is null terminated

	return OpStatus::OK;
}

OP_STATUS PluginStream::SetMimeType(const uni_char* mime_type)
{
	OpString8 sb_mime_type;
	if (!mime_type)
		RETURN_IF_ERROR(m_url->GetAttribute(URL::KMIME_Type, sb_mime_type, TRUE));
	else
		RETURN_IF_ERROR(sb_mime_type.Set(mime_type, uni_strlen(mime_type)));

#if NS4P_INVALIDATE_INTERVAL > 0
	if (sb_mime_type.Compare( "video/x-flv")==0)
		m_plugin->SetIsFlashVideo(TRUE);
#endif // NS4P_INVALIDATE_INTERVAL
	int len = sb_mime_type.Length();
	m_mimetype = OP_NEWA(char, len+1);
	if (!m_mimetype)
		return OpStatus::ERR_NO_MEMORY;

	if (len)
		op_strcpy((char*)m_mimetype, sb_mime_type.CStr());
	else
		((char*)m_mimetype)[len] = '\0';

	return OpStatus::OK;
}

OP_STATUS PluginStream::New(Plugin* plugin, const uni_char* mime_type, const NPUTF8* result, unsigned length)
{
	OP_STATUS status = OpStatus::OK;

 	if (IsLoadingInTarget()) // data is loaded without streaming
 		return status;

	if (GetLastPosted() != NEWSTREAM && 
		(GetLastCalled() == INIT || GetLastCalled() == EMBEDSTREAMFAILED) && 
		OpNS4PluginHandler::GetHandler())
	{
		short http_response = m_url->GetAttribute(URL::KHTTP_Response_Code, TRUE);
		if (http_response >= HTTP_BAD_REQUEST && GetLastCalled() != EMBEDSTREAMFAILED) // check errors like HTTP_NOT_FOUND received from server
		{
			if (GetLastPosted() != EMBEDSTREAMFAILED)
			{
				if (OpStatus::IsSuccess(status = ((PluginHandler*)OpNS4PluginHandler::GetHandler())->PostPluginMessage(
										EMBEDSTREAMFAILED,
										plugin->GetInstance(),
										plugin->GetID(),
										id)))
				{
					SetLastPosted(EMBEDSTREAMFAILED);
					RETURN_IF_ERROR(SetMimeType(mime_type));
				}
			}
			return status;
		}

		if (GetLastPosted() != EMBEDSTREAMFAILED)
			RETURN_IF_ERROR(SetMimeType(mime_type));

		if (m_url->Type() == URL_FILE
#ifdef NS4P_WMP_LOCAL_FILES
			&& !plugin->IsWMPMimetype()
#endif // NS4P_WMP_LOCAL_FILES
			)
			stype = NP_ASFILEONLY;
		else
			stype = NP_NORMAL;

		if (!m_url->GetAttribute(URL::KMovedToURL, TRUE).IsEmpty())
		{
			// remember original url name for notification purposes
			m_notify_url_name = SetNewStr_NotEmpty(stream->url);
			if (!m_notify_url_name)
				return OpStatus::ERR_NO_MEMORY;

			// replace the original url with the redirected url
			OpString8 redir_url; ANCHOR(OpString8, redir_url);
			RETURN_IF_ERROR(m_url->GetAttribute(URL::KName_With_Fragment_Username_Password_Hidden_Escaped, redir_url, TRUE));
			OP_DELETEA((char*)stream->url);
			stream->url = SetNewStr_NotEmpty(redir_url.CStr());
			if (!stream->url)
				return OpStatus::ERR_NO_MEMORY;

			plugin->Redirect(m_url->Id());
		}

		OpFileLength content_size;
		m_url->GetAttribute(URL::KContentSize, &content_size, TRUE);

		if (sizeof(OpFileLength) > sizeof(uint32) && content_size > 0xFFFFFFFF)
			stream->end = 0;
#ifdef SYSTEM_OPFILELENGTH_IS_SIGNED
		else if (content_size < 0)
			stream->end = 0;
#endif // SYSTEM_OPFILELENGTH_IS_SIGNED
		else
			stream->end = static_cast<uint32>(content_size);

		time_t tmp = 0;
		m_url->GetAttribute(URL::KVLastModified, &tmp, TRUE);
		stream->lastmodified = (uint32)tmp;

		RETURN_IF_MEMORY_ERROR(CreateNPStreamHeaders());

		if (result)
		{
			// initialize script evaluation result and streaming variables
			m_js_eval = static_cast<NPUTF8 *>(op_malloc(length + 1));
			if (m_js_eval)
			{
				op_memcpy(static_cast<void*>(m_js_eval), result, length);
				m_js_eval[length] = 0;
				m_js_eval_length = length;
				stream->end = length;
				SetOffset(0);
				SetBytesWritten(0);
				SetMore(FALSE);
			}
			else
				return OpStatus::ERR_NO_MEMORY;
		}

		if (OpStatus::IsSuccess(status = ((PluginHandler*)OpNS4PluginHandler::GetHandler())->PostPluginMessage(
								NEWSTREAM,
								plugin->GetInstance(),
								plugin->GetID(),
								id)))
		{
			SetLastPosted(NEWSTREAM);
			RETURN_IF_ERROR(UpdateStatusRequest(plugin));
		}
	}
	return status;
}

OP_STATUS PluginStream::WriteReady(Plugin* plugin)
{
	OP_STATUS status = OpStatus::OK;
	if (GetJSEval() && GetJSEvalLength())
	{
		// GetJSEval() is the script evaluation result not yet streamed
		// GetJSEvalLength() is the full size of the script evaluation

		SetOffset(GetOffset() + GetBytesWritten());
		SetBufLength(GetJSEvalLength() - GetOffset());
		SetBytesLeft(GetBufLength());

		if (GetBytesWritten() > 0)
		{
			// NPP_WriteReady() and NPP_Write() have been called, but the plugin
			// couldn't receive the whole script evaluation and the
			// result must be streamed in smaller buffer sizes

			OpString8 tmp_buf;
			if (OpStatus::IsSuccess(status = tmp_buf.Set(GetJSEval())))
			{
				op_free(m_js_eval);
				m_js_eval = NULL;
				tmp_buf.Delete(0, GetBytesWritten());
				m_js_eval = static_cast<NPUTF8 *>(op_malloc(GetBytesLeft() + 1));
				if (m_js_eval)
				{
					op_memcpy(static_cast<void*>(m_js_eval), tmp_buf.CStr(), tmp_buf.Length());
					m_js_eval[tmp_buf.Length()] = 0;
				}
				else
					status = OpStatus::ERR_NO_MEMORY;
			}
		}

		if (OpStatus::IsSuccess(status))
		{
			SetBuf(static_cast<const char*>(GetJSEval()));
			if (OpNS4PluginHandler::GetHandler() && OpStatus::IsSuccess(status = ((PluginHandler*)OpNS4PluginHandler::GetHandler())->PostPluginMessage(
							WRITEREADY,
							plugin->GetInstance(),
							plugin->GetID(),
							id,
							NULL,
							m_msg_flush_counter)))
					SetLastPosted(WRITEREADY);
		}
		return status;
	}
	else if (!IsFinished() && GetLastPosted() != WRITEREADY && (GetLastCalled() == NEWSTREAM || GetLastCalled() == WRITE))
	{
		FramesDocument* frames_doc = plugin->GetDocument();
		if (!frames_doc)
			status = OpStatus::ERR;
		else if (stype == NP_SEEK || stype == NP_ASFILEONLY)
		{
			URLStatus url_stat = m_url->Status(TRUE);
			if (url_stat != URL_LOADING && url_stat != URL_UNLOADED)
				SetFinished();
			if (url_stat == URL_LOADED && stype == NP_ASFILEONLY)
				StreamAsFile(plugin);
		}
		else
		{
			if (!GetURLDataDescr())
#ifdef NS4P_GET_ORIGINAL_CONTENT
				SetURLDataDescr(m_url->GetDescriptor(frames_doc->GetMessageHandler(), TRUE, TRUE, FALSE, 0, URL_UNDETERMINED_CONTENT, 0, TRUE));
#else
				SetURLDataDescr(m_url->GetDescriptor(frames_doc->GetMessageHandler(), TRUE, TRUE, TRUE));
#endif // NS4P_GET_ORIGINAL_CONTENT
			if (!GetURLDataDescr())
			{
				URLStatus url_status = m_url->Status(TRUE);
				if (((url_status == URL_LOADED || url_status == URL_LOADING_FAILURE) && !m_url->GetAttribute(URL::KDataPresent, TRUE))
					|| (GetJSEval() && !GetJSEvalLength()))
				{
					if (url_status == URL_LOADING_FAILURE || (GetJSEval() && !GetJSEvalLength())) // Follow Mozilla
						SetReason(NPRES_NETWORK_ERR);
					SetFinished();
					StreamAsFile(plugin);
				}
				return OpStatus::OK;
			}

			BOOL more_data;
			unsigned long buf_len = 0;
			TRAPD(stat, buf_len = GetURLDataDescr()->RetrieveDataL(more_data));
			if (OpStatus::IsError(stat))
				return stat;

			SetBytesLeft(buf_len);
			SetMore(more_data);

			if (GetBytesLeft())
			{
				SetBuf((const char*) GetURLDataDescr()->GetBuffer());
				SetBufLength(GetBytesLeft());
				SetOffset((uint32)GetURLDataDescr()->GetPosition());
				UpdateStreamLength();
				if (OpNS4PluginHandler::GetHandler())
				{
#ifdef NS4P_WMP_STOP_STREAMING
					// src url streaming should be stopped and javascript url loading should be given priority, to solve WMP vulnerability
					if (plugin->IsWMPMimetype() && !IsJSUrl() &&
#ifndef NS4P_WMP_LOCAL_FILES
						m_url->Type() != URL_FILE && 
#endif // !NS4P_WMP_LOCAL_FILES
						plugin->GetEmbedSrcUrl() && plugin->GetEmbedSrcUrl()->url.Id(TRUE) == m_url->Id(TRUE))

					{
						if (GetLastPosted() != DESTROYSTREAM)
						{
							Interrupt(plugin, NPRES_USER_BREAK);
                            if (frames_doc->GetMessageHandler())
                                m_url->StopLoading(frames_doc->GetMessageHandler());						
						}
					}
					else
#endif // NS4P_WMP_STOP_STREAMING
					if (OpStatus::IsSuccess(status = ((PluginHandler*)OpNS4PluginHandler::GetHandler())->PostPluginMessage(
								WRITEREADY,
								plugin->GetInstance(),
								plugin->GetID(),
								id,
								NULL,
								m_msg_flush_counter)))
						SetLastPosted(WRITEREADY);
				}
			}
			else if (!GetMore())
			{
				SetFinished();
				StreamAsFile(plugin);
			}
		}
	}
	return status;
}

OP_STATUS PluginStream::Write(Plugin* plugin)
{
	OP_STATUS status = OpStatus::OK;
	FramesDocument* frames_doc = plugin->GetDocument();
	if (!frames_doc)
		status = OpStatus::ERR;
	else if (GetWriteReady() <= 0)
	{
		// Do not post second WRITEREADY message when one is already pending in message queue
		if (!((PluginHandler*)OpNS4PluginHandler::GetHandler())->HasPendingPluginMessage(
			WRITEREADY,
			plugin->GetInstance(),
			plugin->GetID(),
			id,
			NULL,
			m_msg_flush_counter))
		{
			// If the plug-in don't want any data, wait one second and try again
			if (OpStatus::IsSuccess(status = ((PluginHandler*)OpNS4PluginHandler::GetHandler())->PostPluginMessage(
				WRITEREADY,
				plugin->GetInstance(),
				plugin->GetID(),
				id,
				NULL,
				m_msg_flush_counter,
				NS4P_TIME_TO_NEXT_WRITEREADY_TRY)))
					SetLastPosted(WRITEREADY);
		}
	}
	else
	{
		// Don't write more than plugin will accept

		if (static_cast<int32>(GetBufLength()) > GetWriteReady())
			SetBufLength(GetWriteReady());

		if (GetLastPosted() != WRITE && 
			(GetLastCalled() == NEWSTREAM || GetLastCalled() == WRITEREADY) && 
			OpNS4PluginHandler::GetHandler())
		{
			if (OpStatus::IsSuccess(status = ((PluginHandler*)OpNS4PluginHandler::GetHandler())->PostPluginMessage(
						WRITE,
						plugin->GetInstance(),
						plugin->GetID(),
						id,
						NULL,
						m_msg_flush_counter)))

				SetLastPosted(WRITE);
		}
	}
	return status;
}

#ifdef NS4P_STREAM_FILE_WITH_EXTENSION_NAME
OP_STATUS PluginStream::SaveUrlAsCacheFile()
{
	OpString ext;
	OP_STATUS status = m_url->GetAttribute(URL::KSuggestedFileNameExtension_L, ext, TRUE);
	if (OpStatus::IsSuccess(status))
	{
		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_CACHE_FOLDER, m_url_cachefile_name));
		UINT32 id = m_url->Id();
		status = m_url_cachefile_name.AppendFormat(UNI_L("plugin\\opr_%u."), id, ext.CStr());
		if (OpStatus::IsSuccess(status))
			status = m_url->SaveAsFile(m_url_cachefile_name);
	}
	return status;
}
#endif // NS4P_STREAM_FILE_WITH_EXTENSION_NAME

OP_STATUS PluginStream::StreamAsFile(Plugin* plugin)
{
	OP_STATUS status = OpStatus::OK;

	if (GetReason() == NPRES_DONE) // default value
	{
		if (m_url->Status(TRUE) == URL_LOADING_FAILURE)
		{
#if defined(_LOCALHOST_SUPPORT_) || !defined(RAMCACHE_ONLY)
			if (m_url->GetAttribute(URL::KType) == URL_FILE)
				SetReason(NPRES_NETWORK_ERR);
			else
#endif // _LOCALHOST_SUPPORT_ || !RAMCACHE_ONLY
			{	// if the server request failed, check if the plugin wants to receive the error result in a stream
				OP_STATUS plugin_wants_all_streams = plugin->GetPluginWantsAllNetworkStreams();
				RETURN_IF_ERROR(plugin_wants_all_streams);
				if (plugin_wants_all_streams == OpBoolean::IS_FALSE)
					SetReason(NPRES_NETWORK_ERR); 
			}
		}
		else if (m_url->Status(TRUE) == URL_LOADING_ABORTED || !plugin->GetDocument())
			SetReason(NPRES_USER_BREAK);
	}

#if !defined NS4P_DISABLE_STREAM_AS_FILE && (defined(_LOCALHOST_SUPPORT_) || !defined(RAMCACHE_ONLY))
	// make sure that the name of the decompressed file is fetched
	// and force fetching of the cached filename even if the user has disabled cache
	OpString fname;

#ifdef NS4P_STREAM_FILE_WITH_EXTENSION_NAME
	status = SaveUrlAsCacheFile();
	if (OpStatus::IsSuccess(status))
		status = fname.Set(GetUrlCacheFileName());
#else
	if (m_url->PrepareForViewing(TRUE, TRUE, TRUE, TRUE) == MSG_OOM_CLEANUP)
		status = OpStatus::ERR_NO_MEMORY;
	else
		status = m_url->GetAttribute(URL::KFilePathName_L, fname, TRUE);
#endif //NS4P_STREAM_FILE_WITH_EXTENSION_NAME
	if (OpStatus::IsSuccess(status))
	{
		if ((Type() == NP_ASFILE || Type() == NP_ASFILEONLY) && fname.HasContent() && GetReason() == NPRES_DONE)
			if (GetLastPosted() != STREAMASFILE && 
				(GetLastCalled() == NEWSTREAM || GetLastCalled() == WRITE) && 
				OpNS4PluginHandler::GetHandler())
			{
				if (OpStatus::IsSuccess(status = ((PluginHandler*)OpNS4PluginHandler::GetHandler())->PostPluginMessage(
					STREAMASFILE,
					plugin->GetInstance(),
					plugin->GetID(),
					id)))
					SetLastPosted(STREAMASFILE);
			}
	}

#endif // !NS4P_DISABLE_STREAM_AS_FILE && ((_LOCALHOST_SUPPORT_) || !(RAMCACHE_ONLY))

	if (GetLastPosted() != STREAMASFILE)
		Destroy(plugin);

	return status;
}

OP_STATUS PluginStream::Interrupt(Plugin* plugin, NPReason reason)
{
	OP_STATUS status = OpStatus::OK;
	if (OpNS4PluginHandler::GetHandler())
	{
		SetReason(reason);
		if (OpStatus::IsSuccess(status = ((PluginHandler*)OpNS4PluginHandler::GetHandler())->PostPluginMessage(
					DESTROYSTREAM,
					plugin->GetInstance(),
					plugin->GetID(),
					id)))
		{
			SetLastPosted(DESTROYSTREAM);
			SetFinished();
			RETURN_IF_ERROR(UpdateStatusRequest(plugin, TRUE));
		}
	}
	return status;
}

OP_STATUS PluginStream::Destroy(Plugin* plugin)
{
	OP_ASSERT(IsFinished());
	OP_STATUS status = OpStatus::OK;
	if (GetLastPosted() != DESTROYSTREAM && 
		(GetLastCalled() == NEWSTREAM || GetLastCalled() == WRITE || GetLastCalled() == STREAMASFILE) && 
		OpNS4PluginHandler::GetHandler())
	{
		if (OpStatus::IsSuccess(status = ((PluginHandler*)OpNS4PluginHandler::GetHandler())->PostPluginMessage(
					DESTROYSTREAM,
					plugin->GetInstance(),
					plugin->GetID(),
					id)))
		{
			SetLastPosted(DESTROYSTREAM);
			RETURN_IF_ERROR(UpdateStatusRequest(plugin, TRUE));
		}
	}
	return status;
}

OP_STATUS PluginStream::Notify(Plugin* plugin)
{
	OP_STATUS status = OpStatus::OK;

	if (IsLoadingInTarget() && GetReason() == NPRES_DONE)
	{
		if (m_url->Status(TRUE) == URL_LOADING_FAILURE)
			SetReason(NPRES_NETWORK_ERR);
		else if (m_url->Status(TRUE) == URL_LOADING_ABORTED || !plugin->GetDocument())
			SetReason(NPRES_USER_BREAK);
	}

	if (GetLastPosted() != URLNOTIFY && 
		(GetLastCalled() == DESTROYSTREAM || IsLoadingInTarget() || GetReason() != NPRES_DONE) &&
		OpNS4PluginHandler::GetHandler())
	{
		if (OpStatus::IsSuccess(status = ((PluginHandler*)OpNS4PluginHandler::GetHandler())->PostPluginMessage(
					URLNOTIFY,
					plugin->GetInstance(),
					plugin->GetID(),
					id)))
			SetLastPosted(URLNOTIFY);
	}
	return status;
}

OP_STATUS PluginStream::UpdateStatusRequest(Plugin* plugin, BOOL terminate)
{
	OP_STATUS status = OpStatus::OK;
	FramesDocument* frames_doc = plugin->GetDocument();

	if (plugin->GetPluginUrlRequestsDisplayed() && m_url->Type() != URL_JAVASCRIPT &&
		frames_doc && frames_doc->GetWindow())
	{	// update the status bar with the plugin initiated url request
		if (terminate)
			RETURN_IF_ERROR(frames_doc->GetWindow()->SetMessage(UNI_L("")));
		else
		{
			OpStringC servername = m_url->GetAttribute(URL::KUniHostName);
			if (servername.HasContent())
			{
				OpString msg, format; ANCHOR(OpString, msg); ANCHOR(OpString, format);
				TRAPD(status, g_languageManager->GetStringL(Str::S_PLUGIN_TRANSFER_STATUS, format));
				if (!format.IsEmpty())
				{
					RETURN_IF_ERROR(msg.AppendFormat(format.CStr(), servername.CStr()));
					RETURN_IF_ERROR(frames_doc->GetWindow()->SetMessage(msg.CStr()));
				}
			}
		}
	}
	return status;
}

NPError PluginStream::CallProc(PluginMsgType msgty, Plugin* plugin)
{
	OP_NEW_DBG("PluginStream::CallProc", "ns4p");

	if (plugin->IsFailure())
		return NPERR_INVALID_INSTANCE_ERROR;

	NPError ret = NPERR_NO_ERROR;
	switch (msgty)
	{
		case DESTROYSTREAM:
		{
			if (GetLastCalled() != INIT) // if stream has been interrupted before calling npp_newstream, avoid calling NPP_DestroyStream
			{
				OP_DBG(("Plugin [%d] stream [%d] DESTROYSTREAM", plugin->GetID(), Id()));
				TRY
					ret = plugin->GetPluginfuncs()->destroystream(plugin->GetInstance(),
										      Stream(),
										      GetReason());
				CATCH_PLUGIN_EXCEPTION(ret = NPERR_GENERIC_ERROR; plugin->SetException();)
				OP_DBG(("Plugin [%d] stream [%d] DESTROYSTREAM returned", plugin->GetID(), Id()));
			}
			break;
		}
		case URLNOTIFY:
		{
			OP_DBG(("Plugin [%d] stream [%d] URLNOTIFY", plugin->GetID(), Id()));
			TRY
				plugin->GetPluginfuncs()->urlnotify(plugin->GetInstance(),
								    m_notify_url_name ? m_notify_url_name : GetStreamUrl(),
								    GetReason(),
								    GetStreamNotifyData());
			CATCH_PLUGIN_EXCEPTION(ret = NPERR_GENERIC_ERROR; plugin->SetException();)
			OP_DBG(("Plugin [%d] stream [%d] URLNOTIFY returned", plugin->GetID(), Id()));
			break;
		}
	}
	return ret;
}

void PluginStream::NonPostingInterrupt(Plugin* plugin)
{
	if (GetLastCalled() != URLNOTIFY && !plugin->GetLockEnabledState())
	{
		if (GetLastCalled() != DESTROYSTREAM)
		{
			if (GetURL().Status(TRUE) == URL_LOADED) // cache request
				OpStatus::Ignore(GetURL().PrepareForViewing(TRUE));

			SetReason(NPRES_USER_BREAK);
			plugin->PushLockEnabledState();
			CallProc(DESTROYSTREAM, plugin);
			plugin->PopLockEnabledState();
		}
		if (GetNotify())
		{
#if defined(NS4P_WMP_STOP_STREAMING) || defined(NS4P_WMP_CONVERT_CODEBASE)
			// avoid calling NPP_URLNotify() for a JS url in a cleanup situation because WMP might hang 
			// for unknown reason and return to a deleted environment. Fixes bug #360757.
			if (plugin->IsWMPMimetype() && IsJSUrl())
				return; 
#endif // NS4P_WMP_STOP_STREAMING || NS4P_WMP_CONVERT_CODEBASE
			plugin->PushLockEnabledState();
			CallProc(URLNOTIFY, plugin);
			plugin->PopLockEnabledState();
		}
	}
}

NPError PluginStream::RequestRead(NPByteRange* rangeList)
{
	if (GetLastPosted() == DESTROYSTREAM || GetLastCalled() == DESTROYSTREAM)
		return NPERR_INVALID_INSTANCE_ERROR;

	if (rangeList->length || rangeList->next) // not supported
		return NPERR_INVALID_PARAM;

	if (!url_data_desc || OpStatus::IsError(url_data_desc->SetPosition(rangeList->offset)))
		return NPERR_GENERIC_ERROR;

	// Flush pending writes.
	m_msg_flush_counter++;

	// Rewind the state.
	SetLastPosted(NEWSTREAM);

	return NPERR_NO_ERROR;
}

/*virtual*/
BOOL PluginStream::OnURLHeaderLoaded(URL &url)
{
	// FIXME: No way to return OOM situation.
# ifdef _DEBUG
	OP_NEW_DBG("PluginStream::OnURLHeaderLoaded", "pluginloading");
	OP_DBG((" (%d/%d) Header loaded for %s", m_plugin->m_loading_streams, m_plugin->m_loaded_streams, url.GetName(FALSE, PASSWORD_SHOW)));
# endif // _DEBUG
	OpStatus::Ignore(New(m_plugin, NULL, 0, 0));

	return TRUE;
}

/*virtual*/
BOOL PluginStream::OnURLDataLoaded(URL &url, BOOL finished, OpAutoPtr<URL_DataDescriptor> &stored_desc)
{
# ifdef _DEBUG
	OP_NEW_DBG("PluginStream::OnURLDataLoaded", "pluginloading");
	if (finished)
		m_plugin->m_loaded_streams++;
	OP_DBG((" (%d/%d) Data loaded for %s", m_plugin->m_loading_streams, m_plugin->m_loaded_streams, url.GetName(FALSE, PASSWORD_SHOW)));
# endif // _DEBUG

	PluginMsgType msgtype = GetLastCalled();
	if (!IsFinished())
	{
		switch (msgtype)
		{
			case INIT:
				if (m_plugin->GetLifeCycleState() > Plugin::UNINITED) // The plugin has called NPP_New()
				{
					if (IsLoadingInTarget()) // data is loaded without streaming
					{
						if (GetURL().Status(TRUE) != URL_LOADING)
							OpStatus::Ignore(Notify(m_plugin));
					}
					else
						OpStatus::Ignore(New(m_plugin, NULL, NULL, 0));
				}
				break;

			case NEWSTREAM:
			case WRITE:
				OpStatus::Ignore(WriteReady(m_plugin));
				break;

			case WRITEREADY:
				OpStatus::Ignore(Write(m_plugin));
				break;

			case STREAMASFILE:
			case DESTROYSTREAM:
			case URLNOTIFY:
				OP_ASSERT(FALSE);
				break;
		}
	}
	else
	{
		switch (msgtype)
		{
			case INIT:
				if (GetReason() != NPRES_DONE && GetNotify())
					OpStatus::Ignore(Notify(m_plugin));
				break;

			case NEWSTREAM:
				break;

			case WRITE:
				OpStatus::Ignore(StreamAsFile(m_plugin));
				break;

			case STREAMASFILE:
				OpStatus::Ignore(Destroy(m_plugin));
				break;

			case DESTROYSTREAM:
				OpStatus::Ignore(Notify(m_plugin));
				break;

			case WRITEREADY:
			case URLNOTIFY:
				OP_ASSERT(FALSE);
				break;
		}
	}

	return TRUE;
}

/*virtual*/
void PluginStream::OnURLLoadingFailed(URL &url, Str::LocaleString error, OpAutoPtr<URL_DataDescriptor> &stored_desc)
{
	// FIXME: No way to return OOM situation.
	OP_BOOLEAN plugin_wants_all_streams = m_plugin->GetPluginWantsAllNetworkStreams();
	OpStatus::Ignore(plugin_wants_all_streams);

	if (plugin_wants_all_streams == OpBoolean::IS_TRUE)
		OnURLDataLoaded(url, TRUE, stored_desc);
	else if (plugin_wants_all_streams == OpBoolean::IS_FALSE)
	{
# ifdef _DEBUG
		OP_NEW_DBG("PluginStream::OnURLLoadingFailed", "pluginloading");
		m_plugin->m_loaded_streams++;
		OP_DBG((" (%d/%d) Failed loading of %s", m_plugin->m_loading_streams, m_plugin->m_loaded_streams, url.GetName(FALSE, PASSWORD_SHOW)));
# endif // _DEBUG
		OpStatus::Ignore(Interrupt(m_plugin, NPRES_NETWORK_ERR));
	}
}

/*virtual*/
void PluginStream::OnURLLoadingStopped(URL &url, OpAutoPtr<URL_DataDescriptor> &stored_desc)
{
	// FIXME: No way to return OOM situation.
	OP_BOOLEAN plugin_wants_all_streams = m_plugin->GetPluginWantsAllNetworkStreams();
	OpStatus::Ignore(plugin_wants_all_streams);

	if (plugin_wants_all_streams == OpBoolean::IS_TRUE)
		OnURLDataLoaded(url, TRUE, stored_desc);
	else if (plugin_wants_all_streams == OpBoolean::IS_FALSE)
	{
# ifdef _DEBUG
		OP_NEW_DBG("PluginStream::OnURLLoadingStopped", "pluginloading");
		m_plugin->m_loaded_streams++;
		OP_DBG((" (%d/%d) Stopped loading of %s", m_plugin->m_loading_streams, m_plugin->m_loaded_streams, url.GetName(FALSE, PASSWORD_SHOW)));
# endif // _DEBUG
		OpStatus::Ignore(Interrupt(m_plugin, NPRES_USER_BREAK));
	}
}

#endif // _PLUGIN_SUPPORT_
