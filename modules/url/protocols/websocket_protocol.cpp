/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Niklas Beischer <no@opera.com>, Erik Moller <emoller@opera.com>
**
*/

#include "core/pch.h"

#ifdef WEBSOCKETS_SUPPORT

#include "modules/auth/auth_elm.h"
#include "modules/dochand/win.h"
#include "modules/encodings/encoders/utf8-encoder.h"
#include "modules/encodings/decoders/utf8-decoder.h"
#include "modules/formats/uri_escape.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/upload/upload.h"
#include "modules/url/loadhandler/url_lh.h"
#include "modules/url/protocols/websocket_protocol.h"
#include "modules/url/protocols/websocket_manager.h"
#include "modules/url/protocols/websocket_listener.h"
#include "modules/url/protocols/common.h"
#include "modules/url/url_api.h"
#include "modules/url/url_man.h"
#include "modules/url/url_module.h"
#include "modules/url/tools/arrays.h"
#include "modules/libcrypto/include/OpRandomGenerator.h"
#include "modules/libcrypto/include/CryptoHash.h"
#include "modules/libcrypto/include/CryptoUtility.h"
#include "modules/util/str.h"

#define WS_NEW_DBG OP_NEW_DBG(__FUNCTION__, "url_websocket")
#define WEBSOCKET_VERSION "13"

extern BOOL Is_Restricted_Port(ServerName *server, unsigned short, URLType);

static const UINT WEBSOCKET_BUFFER_ALLOC_THRESHOLD = 64;

static const UINT WEBSOCKET_CONNECT_TIMEOUT	= 30000;
static const UINT WEBSOCKET_CLOSE_TIMEOUT = 3000;

class OutBufferFile : public WebSocketProtocol::OutBuffer
{
public:
	static OutBufferFile* Make(const uni_char* filename, WebSocketProtocol::Frame::FrameOperationCode data_type, BOOL finished)
	{
		OpAutoPtr<OutBufferFile> buffer(OP_NEW(OutBufferFile, (data_type, finished)));
		RETURN_VALUE_IF_NULL(buffer.get(), NULL);
		RETURN_VALUE_IF_ERROR(buffer->m_filename.Set(filename), NULL);
		RETURN_VALUE_IF_ERROR(buffer->m_file.Construct(buffer->m_filename, OPFILE_ABSOLUTE_FOLDER), NULL);
		RETURN_VALUE_IF_ERROR(buffer->m_file.Open(OPFILE_READ), NULL);
		RETURN_VALUE_IF_ERROR(buffer->m_file.GetFileLength(buffer->m_file_length), NULL);
		return buffer.release();
	}

	OpFileLength GetTotalSizeLeft(){OP_ASSERT(m_file.IsOpen()); return m_file_length;}

	WebSocketProtocol::Frame::FrameOperationCode GetDataType();

	virtual OP_STATUS StartSendingData()
	{
		OP_ASSERT(m_has_started == FALSE);
		m_has_started = TRUE;
		return OpStatus::OK;
	};

	UINT8* GetNextSlice(UINT max_size, UINT& size)
	{
		OP_ASSERT(m_has_started);
		size = 0;
		OpFileLength length = MIN(max_size, m_file_length);
		UINT8* slice = OP_NEWA(UINT8, static_cast<UINT>(length));
		ANCHOR_ARRAY(UINT8, slice);
		RETURN_VALUE_IF_NULL(slice, NULL);
		OpFileLength length_read;
		RETURN_VALUE_IF_ERROR(m_file.Read(slice, length, &length_read), NULL);
		m_file_length -= length_read;
		size = static_cast<UINT>(length_read);

		ANCHOR_ARRAY_RELEASE(slice);
		return slice;
	}
private:
	OutBufferFile(WebSocketProtocol::Frame::FrameOperationCode data_type, BOOL finished) : OutBuffer(data_type, finished) {}

	OpString m_filename;
	OpFile m_file;
	OpFileLength m_file_length;
};

class OutBufferMemory : public WebSocketProtocol::OutBuffer
{
public:
	static OutBufferMemory* Make(const UINT8* data, OpFileLength length, WebSocketProtocol::Frame::FrameOperationCode data_type, BOOL finished)
	{
		OpAutoPtr<OutBufferMemory> buffer(OP_NEW(OutBufferMemory, (data_type, finished)));
		if (buffer.get() && OpStatus::IsSuccess(buffer->m_buffer.SetCopyData(reinterpret_cast<const char*>(data), static_cast<size_t>(length))))
			return buffer.release();

		return NULL;
	};

	OpFileLength GetTotalSizeLeft()
	{
		OP_ASSERT(m_has_started);
		return m_buffer.Length();
	}

	virtual OP_STATUS StartSendingData()
	{
		OP_ASSERT(m_has_started == FALSE);
		m_has_started = TRUE;
		return OpStatus::OK;
	}

	UINT8* GetNextSlice(UINT max_size, UINT &size)
	{
		OP_ASSERT(m_has_started);
		size = 0;
		size_t length = MIN(max_size, m_buffer.Length());
		UINT8* slice = OP_NEWA(UINT8, length);
		RETURN_VALUE_IF_NULL(slice, NULL);

		op_memcpy(slice, m_buffer.Data(), length);
		m_buffer.Consume(length);
		size = length;

		return slice;
	}

private:
	OutBufferMemory(WebSocketProtocol::Frame::FrameOperationCode data_type, BOOL finished) : OutBuffer(data_type, finished) {}

	OpData m_buffer;
};

OP_STATUS WebSocketProtocol::Create(OpWebSocket** socket,OpWebSocketListener* listener, MessageHandler* mh)
{
	WS_NEW_DBG;
	*socket = NULL; // Just to make sure

	// No point in creating an OpWebSocket without a listener
	if (listener == NULL)
		return OpStatus::ERR_NOT_SUPPORTED;

	OpAutoPtr<WebSocketProtocol> new_socket (OP_NEW(WebSocketProtocol, (listener, mh)));
	if (!new_socket.get())
		return OpStatus::ERR_NO_MEMORY;

	*socket = new_socket.release();
	return OpStatus::OK;
}


WebSocketProtocol::~WebSocketProtocol()
{
	WS_NEW_DBG;
	OP_DELETE(m_header);
	m_header = NULL;

	OP_DELETE(m_connection);
	m_connection = NULL;

	OP_DELETEA(m_handshake);
	m_handshake = NULL;

	OP_DELETE(m_auth_data);
	m_auth_data = NULL;

	OP_DELETE(m_mh_list);
	m_mh_list = NULL;

	mh->RemoveCallBacks(this, Id());
}


OP_STATUS WebSocketProtocol::Open(const uni_char* url, const URL& origin, const OpVector<OpString>& sub_protocols)
{
	WS_NEW_DBG;

	// Do the relaxed URL parsing allowing for left out slashes etc.
	OpString urlNameUser, urlNameResolved;
	TRAPD(err, urlNameUser.SetL(url));
	RETURN_IF_ERROR(err);
	TRAP(err, g_url_api->ResolveUrlNameL(urlNameUser, urlNameResolved, false));
	RETURN_IF_ERROR(err);

	m_target = g_url_api->GetURL(urlNameResolved);
	if (m_target.GetRep())
		m_target.GetRep()->SetWebSocket(this);

	// URLs must not be empty or contain fragment identifiers
	if (m_target.IsEmpty() || origin.IsEmpty())
		return OpStatus::ERR_PARSING_FAILED;

	URLType type = (URLType)m_target.GetAttribute(URL::KType, URL::KNoRedirect);
	if (type == URL_WEBSOCKET_SECURE)
		m_info.secure = TRUE;
	else if (type != URL_WEBSOCKET)
		return OpStatus::ERR_PARSING_FAILED;

	// Fragments are not allowed in websockets urls
	if (m_target.GetAttribute(URL::KFragment_Name).Length() != 0)
		return OpStatus::ERR_PARSING_FAILED;

	URLType origin_type = (URLType)origin.GetAttribute(URL::KType);

	if (origin_type == URL_HTTPS && !m_info.secure)
		return OpStatus::ERR_NO_ACCESS;

	m_targetHost = (ServerName*)m_target.GetAttribute(URL::KServerName, NULL);
	RETURN_VALUE_IF_NULL(m_targetHost, OpStatus::ERR_PARSING_FAILED);

	m_targetPort = (unsigned short)m_target.GetAttribute(URL::KResolvedPort);

	if (Is_Restricted_Port(m_targetHost, m_targetPort, type))
		return OpStatus::ERR_NO_ACCESS;

	OpString8 temp;
	ANCHOR(OpString8, temp);
	TRAP(err, origin.GetAttributeL(URL::KHostNameAndPort_L, temp));
	if (!OpStatus::IsError(err) && temp.HasContent())
		err = m_origin.SetConcat(urlManager->MapUrlType(origin_type), "://", temp);
	else
		err = m_origin.Set("null");

	RETURN_IF_ERROR(err);

	for (unsigned index = 0; index < sub_protocols.GetCount(); index ++)
	{
		OpString *sub_protocol = sub_protocols.Get(index);

		UTF16toUTF8Converter enc;
		int byte_len = sub_protocol->Length() * sizeof(uni_char);


		int bytes_needed = enc.BytesNeeded(sub_protocol->CStr(), byte_len);
		enc.Reset();

		char* buf = OP_NEWA(char, bytes_needed + 1);
		RETURN_VALUE_IF_NULL(buf, OpStatus::ERR_NO_MEMORY);
		ANCHOR_ARRAY(char, buf);

		int read;
		int written = enc.Convert(sub_protocol->CStr(), byte_len, buf, bytes_needed, &read);

		if (read != byte_len || written != bytes_needed)
			return OpStatus::ERR_PARSING_FAILED;

		buf[bytes_needed] = '\0';

		if (m_sub_protocols.Contains(buf)) // Same protocol was listed twice.
			return OpStatus::ERR_PARSING_FAILED;

		OpAutoPtr<OpString8> protocol(OP_NEW(OpString8, ()));
		if (!protocol.get())
			return OpStatus::ERR_NO_MEMORY;

		protocol->TakeOver(buf);
		ANCHOR_ARRAY_RELEASE(buf);

		RETURN_IF_ERROR(m_sub_protocols.Add(protocol->CStr(), protocol.get()));
		protocol.release();
	}

	TRAPD(hserr, BuildHandshakeL());
	if (OpStatus::IsError(hserr))
		return hserr;

#ifdef SELFTEST
	// When we skip the rest of this function, to avoid actually opening any
	// socket when unit testing the protocol implementation.
	if (m_selftest_fake_connection)
	{
		m_state = WS_CONNECTING;
		return OpStatus::OK;
	}
#endif // SELFTEST

	RETURN_IF_ERROR(DetermineProxy());

	if (m_autoProxyLoadHandler)
		return OpStatus::OK;

	OP_STATUS status = OpenInternal();
	/* Communicate a COMM_FAILED outwards as ERR_NO_SUCH_RESOURCE; caller
	   must return a websocket object but immediately report it as closed. */
	if (OpStatus::IsError(status) && m_state == WebSocketProtocol::WS_INITIALIZING)
		return OpStatus::ERR_NO_SUCH_RESOURCE;
	else
		return status;
}

OP_STATUS WebSocketProtocol::OpenInternal()
{
	m_master = g_webSocket_Manager->FindServer(m_targetHost, m_targetPort, m_info.secure, TRUE);
	RETURN_VALUE_IF_NULL(m_master, OpStatus::ERR_NO_MEMORY);
	return m_master->AddSocket(this, mh, m_proxy, m_proxyPort);
}

OP_STATUS WebSocketProtocol::CreateChallenge(OpString8& challenge)
{
	UINT8 random_bytes[16]; /* ARRAY OK 2012-01-18 haavardm */
	g_libcrypto_random_generator->GetRandom(random_bytes, 16);

	return CryptoUtility::ConvertToBase64(random_bytes, 16, challenge);
}

OP_STATUS WebSocketProtocol::CheckServerChallengeResponse(const char* challenge_response)
{
	// Let | denote append. The challenge code is calculated as:
	// Base_64_encoded(Sha1(challenge | 258EAFA5-E914-47DA-95CA-C5AB0DC85B11));

	OpAutoPtr<CryptoHash> hash(CryptoHash::CreateSHA1());
	if (!hash.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(hash->InitHash());
	hash->CalculateHash(m_websocket_key.CStr());
	hash->CalculateHash("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

	UINT8 digest[CRYPTO_MAX_HASH_SIZE]; // ARRAY OK 2012-4-26 haavardm
	hash->ExtractHash(digest);

	OpString8 correct_response;
	RETURN_IF_ERROR(CryptoUtility::ConvertToBase64(digest, hash->Size(), correct_response));


	if (correct_response.Compare(challenge_response) == 0)
		return OpStatus::OK;

	return OpStatus::ERR_PARSING_FAILED;
}

void WebSocketProtocol::UpdateCookieL(Header_List& headers)
{
	WS_NEW_DBG;
	int version;
	int max_version;
	BOOL have_password = FALSE;
	BOOL have_authentication = FALSE;
	const char *cookies = urlManager->GetCookiesL(m_target.GetRep(), version, max_version, FALSE, FALSE, have_password, have_authentication, m_target.GetContextId(), TRUE);
	if (cookies && op_strlen(cookies) > 0)
	{
		headers.ClearHeader("Cookie");

		if (max_version > 0)
		{
			OpString8 temp_var;
			temp_var.AppendFormat("%d", 1);
			headers.SetSeparatorL("Cookie", SEPARATOR_SEMICOLON);
		}

		headers.AddParameterL("Cookie", cookies);
	}
}

void WebSocketProtocol::BuildHandshakeL()
{
	WS_NEW_DBG;

	Header_List headers;

	OpString8 temp;
	ANCHOR(OpString8, temp);

	m_target.GetAttributeL(URL::KPathAndQuery_L, temp);

	OpStackAutoPtr<Header_Item> header(OP_NEW_L(Header_Item, ()));
	header->InitL(NULL);
	header->SetSeparator(SEPARATOR_SPACE);
	header->AddParameterL("GET");
	header->AddParameterL(temp);
	header->AddParameterL("HTTP/1.1");
	header->IntoStart(&headers);

	headers.ClearAndAddParameterL("Upgrade", "websocket"); // Must be lower case for compatible reasons
	headers.ClearAndAddParameterL("Connection", "Upgrade");

	m_target.GetAttributeL(URL::KHostNameAndPort_L, temp);
	headers.ClearAndAddParameterL("Host", temp);
	headers.ClearAndAddParameterL("Origin", m_origin);

	UpdateCookieL(headers);

	if (m_sub_protocols.GetCount() > 0)
	{
		OpAutoPtr<OpHashIterator> itr(m_sub_protocols.GetIterator());
		if (OpStatus::IsSuccess(itr->First()))
		{
			OpString8 protocols;
			BOOL cont = TRUE;
			do
			{
				protocols.AppendL(static_cast<OpString8*>(itr->GetData())->CStr());

				if (OpStatus::IsSuccess(itr->Next()))
					protocols.AppendL(", ");
				else
					cont = FALSE;

			} while (cont);

			headers.ClearAndAddParameterL("Sec-WebSocket-Protocol", protocols.CStr());
		}
	}

	LEAVE_IF_ERROR(CreateChallenge(m_websocket_key));

	headers.ClearAndAddParameterL("Sec-WebSocket-Key", m_websocket_key);

	headers.ClearAndAddParameterL("Sec-WebSocket-Version", WEBSOCKET_VERSION);

	UINT header_len = headers.CalculateLength();
	m_handshake_len = header_len + STRINGLENGTH("\r\n") + 1;

	OP_ASSERT(m_handshake == NULL);
	m_handshake = OP_NEWA_L(char, m_handshake_len);

	headers.OutputHeaders(m_handshake);
	m_handshake[header_len + 0] = '\r';
	m_handshake[header_len + 1] = '\n';
}

void WebSocketProtocol::SendHandshakeL()
{
	WS_NEW_DBG;

	m_info.handshake_sent = TRUE;
	SendData(m_handshake, m_handshake_len - 1);
	m_handshake = NULL;
}

void WebSocketProtocol::GetPreAuthentication()
{
	if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AllowPreSendAuthentication))
	{
		AuthElm *auth_elm = m_proxy->Get_Auth(NULL, m_proxyPort, NULL, URL_HTTP, AUTH_SCHEME_HTTP_PROXY, 0);
		if (auth_elm)
		{
			// Fill out a base request info struct.
			Base_request_st requestInfo;
			requestInfo.origin_host = m_targetHost;
			requestInfo.origin_port = m_targetPort;
			requestInfo.connect_host = m_proxy;
			requestInfo.connect_port = m_proxyPort;

			// An empty request digest data.
			HTTP_Request_digest_data rdd;

			// Get the auth string with the more specific call (compared to GetAuthString) that
			// should only be used with proxy connect requests.
			OP_STATUS err = auth_elm->GetProxyConnectAuthString(m_proxy_auth_str, m_target.GetRep(), &requestInfo, rdd);
			if (OpStatus::IsError(err))
			{
				m_listener->OnSocketError(this, err);
				CloseNow(FALSE, 0, NULL);
			}
		}
	}
}

void WebSocketProtocol::SendProxyHandshakeL()
{
	WS_NEW_DBG;

	Header_List headers;

	OpString8 temp;
	ANCHOR(OpString8, temp);

	m_target.GetAttributeL(URL::KPathAndQuery_L, temp);

	OpStackAutoPtr<Header_Item> header(OP_NEW_L(Header_Item, ()));
	header->InitL(NULL);
	header->SetSeparator(SEPARATOR_SPACE);
	header->AddParameterL("CONNECT");
	m_target.GetAttributeL(URL::KHostNameAndPort_L, temp);
	header->AddParameterL(temp);
	header->AddParameterL("HTTP/1.0");
	header->IntoStart(&headers);

	headers.ClearAndAddParameterL("Host", temp);
	headers.ClearAndAddParameterL("Proxy-Connection", "keep-alive");

	if (m_proxy_auth_str.HasContent())
		headers.ClearAndAddParameterL("Proxy-Authorization", m_proxy_auth_str);

	UINT header_len = headers.CalculateLength();
	int proxyHandshakeLen = header_len + STRINGLENGTH("\r\n") + 1;
	char *proxyHandshake = OP_NEWA_L(char, proxyHandshakeLen);

	headers.OutputHeaders(proxyHandshake);
	op_strcat(proxyHandshake, "\r\n");

	m_info.proxy_handshake_sent = 1;
	SendData(proxyHandshake, proxyHandshakeLen - 1);
}

void WebSocketProtocol::Close(unsigned short status, const uni_char* reason)
{
	WS_NEW_DBG;

	if (m_state == WS_CLOSED || m_state == WS_CLOSING)
		return;


	// If the connection is not yet established, stop the socket and signal a non-clean close.
	if (m_state == WS_INITIALIZING || m_state == WS_CONNECTING)
	{
		CloseNow(FALSE, status, reason);
		return;
	}


	int reason_length = 0;
	if (reason)
		reason_length = uni_strlen(reason);

	OP_STATUS send_status = OpStatus::ERR;

	UINT8 *close_frame_data = OP_NEWA(UINT8, reason_length + 2);
	if (close_frame_data)
	{
		ANCHOR_ARRAY(UINT8, close_frame_data);

		int frame_length = 0;
		if (close_frame_data)
		{
			close_frame_data[0] = status >> 8;
			close_frame_data[1] = status & 255;

			op_memmove(close_frame_data + 2, reason, reason_length);
			frame_length = reason_length + 2;
		}
		send_status = SendCloseToServer(TRUE, status, reason, close_frame_data, frame_length);
	}

	if (OpStatus::IsError(send_status))
	{
		CloseNow(FALSE, 0, NULL);
	}
}

void WebSocketProtocol::CloseHard()
{
	WS_NEW_DBG;

	// Start other connections
	mh->PostMessage(MSG_URL_LOAD_NOW, Id() ,0);

	Stop();
	SetUrlLoadStatus(URL_UNLOADED);
	// Remove the close handshake timeout.
	mh->RemoveDelayedMessage(MSG_COMM_TIMEOUT, Id(), 0);
}

OP_STATUS WebSocketProtocol::SendMessage(const uni_char* data, OpFileLength length, OpFileLength& buffered_amount)
{
	buffered_amount = 0;
	WS_NEW_DBG;
	if (m_state == WS_CLOSING || m_state == WS_CLOSED)
		return OpStatus::ERR_NOT_SUPPORTED;

	UTF16toUTF8Converter enc;
	int bytes_needed = enc.BytesNeeded(data, static_cast<unsigned int>(length) * sizeof(uni_char));
	enc.Reset();

	UINT8* message_utf8 = OP_NEWA(UINT8, bytes_needed + 1);
	RETURN_VALUE_IF_NULL(message_utf8, OpStatus::ERR_NO_MEMORY);
	ANCHOR_ARRAY(UINT8, message_utf8);

	int read;
	int message_utf8_length = enc.Convert(data, static_cast<unsigned int>(length) * sizeof(uni_char), message_utf8, bytes_needed, &read);

	return SendRawData(message_utf8, message_utf8_length, buffered_amount, Frame::OPCODE_TEXT_FRAME, TRUE);
}

OP_STATUS WebSocketProtocol::SendMessage(const UINT8* data, OpFileLength length, OpFileLength& buffered_amount)
{
	buffered_amount = 0;
	WS_NEW_DBG;
	if (m_state == WS_CLOSING || m_state == WS_CLOSED)
		return OpStatus::ERR_NOT_SUPPORTED;

	return SendRawData(data, length, buffered_amount, Frame::OPCODE_BINARY_FRAME, TRUE);
}

OP_STATUS WebSocketProtocol::SendFile(const uni_char* filename, BOOL binary, OpFileLength& buffered_amount)
{
	buffered_amount = 0;
	OP_ASSERT(filename);
	OutBufferFile *buffer = OutBufferFile::Make(filename, binary ? Frame::OPCODE_BINARY_FRAME : Frame::OPCODE_TEXT_FRAME, TRUE);
	if (!buffer)
		return OpStatus::ERR_NO_MEMORY;

	buffer->Into(&m_send_buffers);

	buffered_amount = buffer->GetTotalSizeLeft();

	RETURN_IF_ERROR(SendMessageSliceFromQueue());


	return OpStatus::OK;
}


OP_STATUS WebSocketProtocol::SendMessageSliceFromQueue()
{
	UINT space_left_in_network_layer = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize)*1024;

	OutBuffer *buffer = m_send_buffers.First();

	if (!buffer)
		return OpStatus::OK;

	while (buffer && space_left_in_network_layer)
	{
		BOOL first_send = !buffer->HasStartedSending();

		UINT slice_length;
		if (first_send)
		{
			RETURN_IF_ERROR(buffer->StartSendingData());
			OpFileLength frame_data_length = buffer->GetTotalSizeLeft();

			// Called with NULL will calculate the protocol overhead length but not create the frame.
			UINT frame_protocol_data_length = CreateFrameProtocolData(NULL, frame_data_length, buffer->GetDataType(), buffer->IsFinished());
			if (frame_protocol_data_length > space_left_in_network_layer)
				return OpStatus::OK;

			UINT8* slice = buffer->GetNextSlice(space_left_in_network_layer - frame_protocol_data_length, slice_length);
			RETURN_OOM_IF_NULL(slice);
			ANCHOR_ARRAY(UINT8, slice);

			UINT8 *frame = OP_NEWA(UINT8, frame_protocol_data_length + slice_length);
			RETURN_OOM_IF_NULL(frame);
			ANCHOR_ARRAY(UINT8, frame);

			CreateFrameProtocolData(frame, frame_data_length,  buffer->GetDataType(), buffer->IsFinished());

			op_memcpy(frame + frame_protocol_data_length, slice, slice_length);
			MaskFrameData(frame + frame_protocol_data_length, slice_length);

			// Increase the buffered amount less the framing.
			m_buffered_amount += frame_data_length;

			m_total_raw_buffered_amount += frame_protocol_data_length + frame_data_length;
			OpAutoPtr<BufferedAmount> buffered_amount(OP_NEW(BufferedAmount, (frame_protocol_data_length + frame_data_length, frame_data_length, TRUE)));

			if (!buffered_amount.get() || OpStatus::IsError(m_buffered_amount_tracker.Add(buffered_amount.get())))
				return OpStatus::ERR_NO_MEMORY;

			buffered_amount.release();

			ANCHOR_ARRAY_RELEASE(frame);
			OP_ASSERT(m_total_raw_buffered_amount >= frame_protocol_data_length + slice_length);
			SendData((char*)frame, frame_protocol_data_length + slice_length);
		}
		else
		{
			UINT8* slice = buffer->GetNextSlice(space_left_in_network_layer, slice_length);
			RETURN_OOM_IF_NULL(slice);
			ANCHOR_ARRAY(UINT8, slice);

			OP_ASSERT(m_total_raw_buffered_amount >= slice_length);

			MaskFrameData(slice, slice_length);
			SendData((char*)slice, slice_length);
			ANCHOR_ARRAY_RELEASE(slice);
		}

		// SendData() might close the connection via SetProgressInformation().
		// In that case "buffer" is invalidated (deleted).
		if (m_state == WS_CLOSED)
			break;

		OutBuffer *prev_buffer = buffer;

		if (prev_buffer->GetTotalSizeLeft() == 0)
		{
			buffer = prev_buffer->Suc();
			OP_DELETE(prev_buffer);
			prev_buffer = NULL;
		}

		OP_ASSERT(space_left_in_network_layer >= slice_length);
		space_left_in_network_layer -= slice_length;
	}
	return OpStatus::OK;
}


BOOL WebSocketProtocol::ReceivedLegalCloseCode(Frame::FrameCloseCodes code)
{
	// Accepted Pre-defined close codes as defined in http://tools.ietf.org/html/rfc6455#section-7.4.1
	if ((code >= Frame::CLOSE_NORMAL && code <= Frame::CLOSE_UNEXPECTED_CONDITION) &&
		(code < Frame::CLOSE_RESERVED ||  code > Frame::CLOSE_RESERVED_APPLICATION_LAYER_CLOSED_ABNORMALLY))
		return TRUE;

	// Accepted custom close codes as defined in http://tools.ietf.org/html/rfc6455#section-7.4.2
	if (code >= Frame::CLOSE_START_UNDEFINED_CUSTOM_CODES && code <= Frame::CLOSE_MAX_CLOSE_CODE)
		return TRUE;

	return FALSE;
}

OP_STATUS WebSocketProtocol::ParseCloseMessage(const UINT8* server_buffer, int server_buffer_length,  Frame::FrameCloseCodes& code, OpString& reason)
{
	code = Frame::CLOSE_NO_CODE;

	reason.Empty();

	if (server_buffer_length == 0)
		return OpStatus::OK;

	if (server_buffer_length == 1)
		return OpStatus::ERR;

	code = (Frame::FrameCloseCodes)((server_buffer[0] << 8) + server_buffer[1]);

	if (!ReceivedLegalCloseCode(code))
		return OpStatus::ERR;

	UTF8toUTF16Converter conv;
	if (reason.Reserve(sizeof(uni_char)*(server_buffer_length - 2)))
	{
		if (conv.Convert(reinterpret_cast<const char*>(server_buffer + 2), server_buffer_length - 2, reason.CStr(), sizeof(uni_char) * server_buffer_length - 2, 0) != -1)
		{
			if (conv.GetNumberOfInvalid() == 0 && conv.IsValidEndState())
				OpStatus::Ignore(reason.Set(reinterpret_cast<const char*>(server_buffer + 2), server_buffer_length - 2));
			else
				return OpStatus::ERR;
		}
	}
	return OpStatus::OK;
}

OP_STATUS WebSocketProtocol::SendRawData(const UINT8* data, OpFileLength length, OpFileLength& buffered_amount, WebSocketProtocol::Frame::FrameOperationCode data_type, BOOL finish)
{
	buffered_amount = 0;
	OutBufferMemory *buffer = OutBufferMemory::Make(data, length, data_type, finish);
	if (!buffer)
		return OpStatus::ERR_NO_MEMORY;

	buffer->Into(&m_send_buffers);

	buffered_amount = length;
	RETURN_IF_ERROR(SendMessageSliceFromQueue());

	return OpStatus::OK;
}


BOOL WebSocketProtocol::MoreFrames(Frame::FrameOperationCode type)
{
	FrameBuffer* first_buf = m_recv_buffers.First();
	if (first_buf)
	{
		Frame *first_frame = first_buf->frames.First();
		if (first_frame && (first_frame->operation_code == type || first_frame->operation_code == Frame::OPCODE_CONTINUATION_FRAME))
			return TRUE;
	}
	return FALSE;
}

OP_STATUS WebSocketProtocol::ReadRawFrame(UINT8* buffer, OpFileLength buffer_length, OpFileLength& read_len, Frame::FrameOperationCode type, BOOL& websocket_frame_finished)
{
	WS_NEW_DBG;
	read_len = 0;
	if (buffer == NULL)
		return OpStatus::ERR_NOT_SUPPORTED;

	websocket_frame_finished = FALSE;
	FrameBuffer* read_buf = m_recv_buffers.First();

	if (read_buf)
	{
		Frame *frame = read_buf->frames.First();

		OP_ASSERT((frame->operation_code & Frame::OPERATION_CODE_CONTROL_FRAME) == 0);

		if (frame)
		{
			OP_ASSERT(!frame->spool_to_disk);
			OP_ASSERT(frame->operation_code == type || frame->operation_code == Frame::OPCODE_CONTINUATION_FRAME);

			if (frame->operation_code != type && frame->operation_code != Frame::OPCODE_CONTINUATION_FRAME || frame->spool_to_disk)
				return OpStatus::ERR;

			/* If websocket frame is longer than buffer_size, frame->end will be bigger
			 * than buffer_size. In that case buffer_size is used for end point.
			 * frame->start might be -1 to signal that this buffer was continued
			 * from previous buffer. In that case 0 is used for start point. */
			UINT len = static_cast<UINT>(MIN(read_buf->buffer_size, frame->payload_end) - frame->payload_start);
			UINT8 *buf = read_buf->buffer + frame->payload_start;

			if (len > buffer_length)
			{
				OP_ASSERT(!"SHOULD NOT HAPPEN");
				return OpStatus::ERR;
			}

			op_memmove(buffer, buf, len);

			read_len += len;
			websocket_frame_finished = frame->finished && (frame->frame_control_bits & Frame::FIN);

			frame->Out();
			OP_DELETE(frame);
			frame = NULL;
		}

		if (read_buf->IsConsumed())
		{
			// All data in this buffer has been parsed, and there are no frames in it.
			if (read_buf->Suc())
			{
				// There are more data in a successive buffer. We delete this buffer, as all has been consumed and the buffer will no longer be used.
				OP_DELETE(read_buf);
				read_buf = NULL;
			}
			else if (read_buf->IsFull()) // This frame is full and it's the last frame. We can re-use it.
				read_buf->Reset();

			return OpStatus::OK;
		}
	}

	return OpStatus::OK;
}

WebSocketProtocol::Frame *WebSocketProtocol::GetSpoolNextFrameToDisk(BOOL must_be_finished)
{
	FrameBuffer *curr = m_recv_buffers.Last();

	if (curr && curr->frames.Empty())
		curr = curr->Pred();

	if (curr)
	{
		Frame *frame =  curr->frames.Last();
		if (frame && frame->spool_to_disk && (!must_be_finished || frame->finished))
			return frame;
	}
	return NULL;
}

OP_STATUS WebSocketProtocol::BounceControlFrame(FrameBuffer* buffer, Frame* frame)
{
	OP_ASSERT(frame->payload_end < buffer->buffer_size);

	if (frame->payload_end > buffer->buffer_size || frame->payload_end - frame->payload_start != frame->data_left_to_receive)
		return OpStatus::ERR_OUT_OF_RANGE;

	unsigned size = static_cast<unsigned>(frame->payload_end - frame->payload_start);

	if (size > 125)
	{
		CloseNow(FALSE, Frame::CLOSE_PROTOCOL_ERROR, UNI_L("Protocol Error"));
		return OpStatus::ERR_OUT_OF_RANGE;
	}

	UINT8* bounce_data = OP_NEWA(UINT8, size);
	if (bounce_data == NULL)
		return OpStatus::ERR_NO_MEMORY;

	ANCHOR_ARRAY(UINT8, bounce_data);

	op_memcpy(bounce_data, buffer->buffer + frame->payload_start, size);
	UINT data_read = size;

	Frame::FrameOperationCode out_frame_type;

	switch (frame->operation_code) {
		case Frame::OPCODE_CONNECTION_PING:
			out_frame_type = Frame::OPCODE_CONNECTION_PONG;
			break;
		case Frame::OPCODE_CONNECTION_PONG:
			// we do not respond to pong
			return OpStatus::OK;

		case Frame::OPCODE_CONNECTION_CLOSE:
			out_frame_type = Frame::OPCODE_CONNECTION_CLOSE;
			break;

		default:
			out_frame_type = Frame::INTERNALCODE_INVALID_FRAME;
			data_read  = 0;
			break;
	}

	OpFileLength buffered_amount;
	if (out_frame_type == Frame::OPCODE_CONNECTION_CLOSE)
	{
		Frame::FrameCloseCodes code;
		OpString reason;
		OP_STATUS status = ParseCloseMessage(bounce_data, size, code, reason);
		if (OpStatus::IsSuccess(status))
		{
			RETURN_IF_ERROR(SendCloseToServer(TRUE, code, reason.CStr(), bounce_data, data_read));
		}
		else
		{
			Close(Frame::CLOSE_PROTOCOL_ERROR, UNI_L("protocol error"));
		}
	}
	else if (out_frame_type == Frame::INTERNALCODE_INVALID_FRAME)
	{
		CloseNow(FALSE, 0, NULL);
		return OpStatus::OK;
	}
	else
	{
		OpStatus::Ignore(SendRawData(bounce_data, data_read, buffered_amount, out_frame_type, TRUE));
	}
	return OpStatus::OK;
}

void WebSocketProtocol::HandleError(OP_STATUS status)
{
	m_listener->OnSocketError(this, status);

	if (OpStatus::IsMemoryError(status))
	{
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		CloseNow(FALSE, Frame::CLOSE_NO_CODE, NULL);
		return;
	}

	if (m_state == WS_CONNECTING)
	{
		if (status == OpStatus::ERR_PARSING_FAILED)
			m_listener->OnSocketInfo(this, OpWebSocketListener::WSHandshakeParsing);
		else if (status == OpStatus::ERR_OUT_OF_RANGE)
			m_listener->OnSocketInfo(this, OpWebSocketListener::WSHeaderParsing);
		else if (status == OpStatus::ERR_NOT_SUPPORTED)
			m_listener->OnSocketInfo(this, OpWebSocketListener::WSInvalidFrameType);

		CloseNow(FALSE, Frame::CLOSE_PROTOCOL_ERROR, UNI_L("Protocol Error"));
	}
	else if (m_state == WS_CONNECTED)
	{
		if (status == OpStatus::ERR_PARSING_FAILED)
			m_listener->OnSocketInfo(this, OpWebSocketListener::WSFrameParsing);
		else if (status == OpStatus::ERR_OUT_OF_RANGE)
			m_listener->OnSocketInfo(this, OpWebSocketListener::WSFrameTooLong);
		else if (status == OpStatus::ERR_NOT_SUPPORTED)
			m_listener->OnSocketInfo(this, OpWebSocketListener::WSInvalidFrameType);
		CloseNow(FALSE, Frame::CLOSE_PROTOCOL_ERROR, UNI_L("Protocol Error"));
	}
}

UINT WebSocketProtocol::CreateFrameProtocolData(UINT8* frame_protocol_data, UINT64 frame_length, Frame::FrameOperationCode frame_operation_code, BOOL finish)
{
	UINT16 frame_control_bits = Frame::NOT_BITS_SET;

	int extension_payload_length = 0;
	//	Add future extensions here.

	frame_control_bits |=  frame_operation_code;
	frame_control_bits |= Frame::MASKED;

	if (finish)
		frame_control_bits |= Frame::FIN;

	UINT64 extended_encoded_length = 0;
	UINT extended_encoded_length_size = 0;

	if (frame_length < 126)
	{
		extended_encoded_length_size = 0;
		frame_control_bits |= frame_length;
	}
	else if (frame_length < 0x10000)
	{
		// 16 bit;
		extended_encoded_length_size = 2;
		frame_control_bits |= 126; // signals 16 bit length
		extended_encoded_length = frame_length;
	}
	else
	{
		// 64 bit
		extended_encoded_length_size = 8;
		frame_control_bits |= 127; // signals 64 bit length

		extended_encoded_length = frame_length;
	}

	int mask_key_pos = 2 + extended_encoded_length_size;

	int frame_control_data_length = mask_key_pos + 4 /* mask */;

	UINT protocol_data_length = frame_control_data_length + extension_payload_length;

	if (!frame_protocol_data) // If we return here, the caller only wanted to calculate frame length
		return protocol_data_length;

	frame_protocol_data[0] = frame_control_bits >> 8;
	frame_protocol_data[1] = frame_control_bits & 0xff;

	UINT64 length = extended_encoded_length;
	for (int i = extended_encoded_length_size - 1 ; i >= 0; i --)
	{
		frame_protocol_data[2 + i] = length & 0xff;
		length >>= 8;
	}

	OP_ASSERT(2 + extended_encoded_length_size - 1 < protocol_data_length);

	g_libcrypto_random_generator->GetRandom(m_current_mask_data, 4);
	op_memcpy(frame_protocol_data + mask_key_pos, m_current_mask_data, 4);

	// We copy in extension data,
	//	Fixme : add extensions here
	m_current_message_position = 0;
	return protocol_data_length;
}

void WebSocketProtocol::MaskFrameData(UINT8 *frame_data, UINT data_length)
{
	// We copy in message data, and masks if needed.
	for (UINT i = 0; i < data_length; i++)
		frame_data[i] ^= m_current_mask_data[(m_current_message_position++) %4];
}

void WebSocketProtocol::CheckBufferStatus(FrameBuffer*& read_buf, Frame* frame)
{
	WS_NEW_DBG;

	if (!read_buf->IsConsumed())
		return;

	// If there's no current data in the buffer and it's the last buffer, reset it to save some memory allocations.
	if (read_buf->frames.Empty() && read_buf->Suc() == NULL)
		read_buf->Reset();
	// If we just removed the last frame in the buffer and it's full, delete the buffer.
	// It'll always be full if it's not the last buffer.
	else if (!frame && read_buf->IsFull())
	{
		FrameBuffer* consumed_buf = read_buf;
		read_buf = read_buf->Suc();
		consumed_buf->Out();
		OP_DELETE(consumed_buf);
	}
}


OP_STATUS WebSocketProtocol::ReadMessage(uni_char* buffer, OpFileLength buffer_length, OpFileLength& read_len)
{
	WS_NEW_DBG;
	OP_ASSERT(GetSpoolNextFrameToDisk(TRUE) == NULL);

	BOOL websocket_frame_finished = FALSE;
	read_len = 0;
	OpFileLength temp_buffer_length = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize)*1024;

	UINT8 *temp_buffer = OP_NEWA(UINT8, static_cast<size_t>(buffer_length + 1));
	if (!temp_buffer)
		return OpStatus::ERR_NO_MEMORY;
	ANCHOR_ARRAY(UINT8, temp_buffer);

	UTF8toUTF16Converter conv;

	while (MoreFrames(Frame::OPCODE_TEXT_FRAME) && !websocket_frame_finished)
	{
		OpFileLength current_frame_read = 0;

		RETURN_IF_ERROR(ReadRawFrame(temp_buffer, temp_buffer_length, current_frame_read, Frame::OPCODE_TEXT_FRAME, websocket_frame_finished));
		int source_read;
		int conv_len = conv.Convert((char*)temp_buffer, static_cast<int>(current_frame_read), buffer + static_cast<unsigned int>(read_len), sizeof(uni_char)*static_cast<int>(buffer_length - read_len), &source_read);

		read_len += conv_len / sizeof(uni_char);
	};

	// A whole message was not received, thus readMessage() was called to early.
	if (!websocket_frame_finished)
	{
		OP_ASSERT(!"CALLED TO EARLY. NOT ENOUGH DATA");
		return OpStatus::ERR;
	}

	if (conv.GetNumberOfInvalid() > 0 || !conv.IsValidEndState())
	{
		CloseNow(FALSE, Frame::CLOSE_PROTOCOL_ERROR, UNI_L("Protocol Error"));
		return OpStatus::ERR_PARSING_FAILED;
	}

	return OpStatus::OK;
}

OP_STATUS WebSocketProtocol::ReadMessage(UINT8* buffer, OpFileLength buffer_length, OpFileLength& read_len)
{
	WS_NEW_DBG;

	BOOL websocket_frame_finished = FALSE;
	read_len = 0;
	while (MoreFrames(Frame::OPCODE_BINARY_FRAME) && !websocket_frame_finished)
	{
		OpFileLength current_frame_read = 0;
		RETURN_IF_ERROR(ReadRawFrame(buffer + read_len, buffer_length - read_len, current_frame_read, Frame::OPCODE_BINARY_FRAME, websocket_frame_finished));
		read_len += current_frame_read;
	}

	// A whole message was not received, thus readMessage() was called to early.
	if (buffer_length != read_len || !websocket_frame_finished)
	{
		OP_ASSERT(!"Called to early. Not enough data.");
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

OP_STATUS WebSocketProtocol::GetMessageFilePath(OpString& full_path)
{
	WS_NEW_DBG;

	FrameBuffer* first_buf = m_recv_buffers.First();
	OpAutoPtr<Frame> first_frame;
	if (first_buf)
		first_frame.reset(first_buf->frames.First());

	if (!first_frame.get())
		return OpStatus::ERR;

	OP_ASSERT(first_frame->spool_to_disk);

	RETURN_IF_ERROR(full_path.Set(first_frame->file.GetFullPath()));

	if (full_path.IsEmpty())
		return OpStatus::ERR;

	OpFileLength frame_file_length;
	RETURN_IF_ERROR(first_frame->file.GetFileLength(frame_file_length));

	if (first_frame->data_left_to_receive != 0)
	{
		OP_ASSERT(!"file length does not match frame length");
		return OpStatus::ERR;
	}

	CheckBufferStatus(first_buf, first_frame->Suc());

	// The caller of this function now has the responsibility to delete the file.
	first_frame->delete_file = FALSE;

	return OpStatus::OK;
}

BOOL WebSocketProtocol::HasMessages()
{
	WS_NEW_DBG;
	FrameBuffer* read_buf = m_recv_buffers.First();
	if (read_buf)
	{
		Frame *frame = read_buf->frames.First();
		if (frame)
		{
			if (frame->finished && (frame->frame_control_bits & Frame::FIN))
				return TRUE;

			// The first frame spans multiple buffers, or the first frame contains a full websocket frame that is continued in a later frame.
			else if (frame->payload_end > read_buf->buffer_size || !(frame->frame_control_bits & Frame::FIN))
			{
				while (read_buf)
				{
					while (frame)
					{
						if (frame->finished && (frame->frame_control_bits & Frame::FIN))
							return TRUE;
						frame = frame->Suc();
					}
					read_buf = read_buf->Suc();
					frame = read_buf ? read_buf->frames.First() : NULL;
				}
			}
		}
	}
	return FALSE;
}


OpFileLength WebSocketProtocol::GetReceivedMessageBytes()
{
	WS_NEW_DBG;

	FrameBuffer* read_buf = m_recv_buffers.First();
	if (read_buf)
	{
		Frame *frame = read_buf->frames.First();
		if (frame)
		{
			if (frame->spool_to_disk)
			{
				OpFileLength file_length = 0;
				if (OpStatus::IsSuccess(frame->file.GetFileLength(file_length)))
					return file_length;
				else
					return 0;
			}

			if (frame->finished && (frame->frame_control_bits & Frame::FIN))
				return frame->payload_end - frame->payload_start;
			// The first frame spans multiple buffers, or the first frame is a is contains a full websocket frame that is continued in a later frame.
			else if (frame->payload_end > read_buf->buffer_size || !(frame->frame_control_bits & Frame::FIN))
			{
				UINT bytes = 0;
				while (read_buf)
				{
					while (frame)
					{
						bytes += static_cast<UINT>(MIN(read_buf->buffer_size, frame->payload_end) - frame->payload_start);
						if (frame->finished && (frame->frame_control_bits & Frame::FIN))
							return bytes;
						frame = frame->Suc();
					}
					read_buf = read_buf->Suc();
					frame = read_buf ? read_buf->frames.First() : NULL;
				}
			}
		}
	}
	return 0;
}

OP_STATUS WebSocketProtocol::GetURL(OpString& url)
{
	return m_target.GetAttribute(URL::KUniName_With_Fragment_Username, 0, url, URL::KNoRedirect);
}

void WebSocketProtocol::Stop()
{
	WS_NEW_DBG;
	ProtocolComm::CloseConnection();
	ProtocolComm::Stop();
	SetNewSink(NULL);
	m_state = WS_CLOSED;

	m_send_buffers.Clear();

	m_buffered_amount = 0;
	m_total_raw_buffered_amount = 0;
	m_buffered_amount_tracker.Clear();

	// Will trigger loading of waiting connections
	if (m_state == WS_CONNECTING)
		mh->PostMessage(MSG_URL_LOAD_NOW, Id() ,0);
}


void WebSocketProtocol::CloseConnection()
{
	WS_NEW_DBG;
	Close(0, NULL);
}


void WebSocketProtocol::DoConnect()
{
	if (m_proxy && !m_info.proxy_handshake_sent)
	{
		if (!m_info.doing_proxy_auth)
			GetPreAuthentication();
		TRAPD(err, SendProxyHandshakeL());
		if (OpStatus::IsError(err))
		{
			Stop();
			m_listener->OnSocketError(this, err);
		}
	}
	else if (!m_info.handshake_sent && (!m_proxy || m_info.proxy_connected))
	{
		TRAPD(err, SendHandshakeL());
		if (OpStatus::IsError(err))
		{
			Stop();
			m_listener->OnSocketError(this, err);
		}
	}
}

OP_STATUS  WebSocketProtocol::CheckFrame(Frame* frame)
{
	Frame::FrameExtensionId extension = frame->GetExtensionCode();

	if (extension != Frame::NO_EXTENSIONS && m_negotiated_extensions.Find(extension) == -1)
		return OpStatus::ERR_NOT_SUPPORTED;


	if (
			frame->operation_code != Frame::OPCODE_CONTINUATION_FRAME &&
			frame->operation_code != Frame::OPCODE_TEXT_FRAME &&
			frame->operation_code != Frame::OPCODE_BINARY_FRAME &&
			frame->operation_code != Frame::OPCODE_CONNECTION_CLOSE &&
			frame->operation_code != Frame::OPCODE_CONNECTION_PING &&
			frame->operation_code != Frame::OPCODE_CONNECTION_PONG
		)
	{
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	if (frame->operation_code == Frame::OPCODE_CONTINUATION_FRAME)
	{
		if (!m_info.waiting_for_continuation_frame)
			return OpStatus::ERR_NOT_SUPPORTED;
	}

	// If we expect an continuation frame, we can only accept control frames (ping/pong etc) or continuation frames.
	if (!(frame->operation_code & Frame::OPERATION_CODE_CONTROL_FRAME) && m_info.waiting_for_continuation_frame && frame->operation_code != Frame::OPCODE_CONTINUATION_FRAME)
		return OpStatus::ERR_NOT_SUPPORTED;

	if (frame->operation_code & Frame::OPERATION_CODE_CONTROL_FRAME)
	{
		OP_ASSERT(frame->finished && "CONTROL FRAMES MUST NEVER BE SPLIT ACROSS BUFFERS");

		if (!(frame->frame_control_bits & Frame::FIN))
			return OpStatus::ERR_NOT_SUPPORTED;

		if (frame->data_left_to_receive > 125)
			return OpStatus::ERR_OUT_OF_RANGE;
	}

	return OpStatus::OK;
}

OP_STATUS WebSocketProtocol::HandlePrioritizedControlFramesAndCheckForCompleteMessages(BOOL& has_complete_messages)
{
	WS_NEW_DBG;
	has_complete_messages = FALSE;

	if (m_state != WS_CONNECTED) // Ignore the rest of control frames if we are closing or closed.
		return OpStatus::OK;

	FrameBuffer* read_buf = m_recv_buffers.First();
	while (read_buf)
	{
		Frame *seen_control_frame_before_finished_message_frame = NULL;

		Frame *frame = read_buf->frames.First();
		BOOL expect_continuation_frame = FALSE;

		while (frame)
		{
			if (m_state != WS_CONNECTED) // Ignore the rest of control frames if we are closing or closed.
				return OpStatus::OK;

			if (frame->finished)
			{
				if (
						expect_continuation_frame &&
						!(frame->frame_control_bits & Frame::OPERATION_CODE_CONTROL_FRAME) &&
						frame->operation_code != Frame::OPCODE_CONTINUATION_FRAME
					)
				{
					// We expected an continuation frame or control frame, since last message frame was not sent with FIN flag.
					return OpStatus::ERR_NOT_SUPPORTED;
				}

				if (frame->frame_control_bits & Frame::FIN)
				{
					if (frame->frame_control_bits & Frame::OPERATION_CODE_CONTROL_FRAME)
					{
						// We saw an control frame in-between a sequence of continuation frames. Control frames must be handled before
						// the continued frame sequence is finished.
						seen_control_frame_before_finished_message_frame = frame;
						break;
					}
					else
					{
						// We found a sequence of message/continuation frames that ended with FIN flag. We can now handle those.
						has_complete_messages = TRUE;
						return OpStatus::OK;
					}
				}
				else
				{
					expect_continuation_frame = TRUE;
				}
			}

			frame = frame->Suc();
		}


		FrameBuffer* next_read_buf = read_buf->Suc();

		if (seen_control_frame_before_finished_message_frame)
		{
			// We saw an control frame in between un-finished message frames. We bounce the control frame.
			seen_control_frame_before_finished_message_frame->Out();
			OpAutoPtr<Frame> frame_destructor(seen_control_frame_before_finished_message_frame);

			RETURN_IF_ERROR(CheckFrame(seen_control_frame_before_finished_message_frame));
			RETURN_IF_ERROR(BounceControlFrame(read_buf, seen_control_frame_before_finished_message_frame));

			if (read_buf->frames.Empty())
			{
				if (read_buf->IsFull() && read_buf->IsConsumed())
				{
					read_buf->Out();
					OP_DELETE(read_buf);
				}
				read_buf = next_read_buf;
			}
		}
		else
			read_buf = next_read_buf;
	}

	return OpStatus::OK;
}

void WebSocketProtocol::RequestMoreData()
{
	WS_NEW_DBG;
	switch (m_state)
	{
	case WS_CONNECTING:
		DoConnect();
		break;
	case WS_CONNECTED:
		break;
	case WS_CLOSING:
		break;
	default:
		OP_ASSERT(!"This should not happen");
		break;
	}
}


void WebSocketProtocol::ProcessReceivedData()
{
	WS_NEW_DBG;
	switch (m_state)
	{
	case WS_CONNECTING:
		{
			if ((m_info.handshake_sent && !m_info.handshake_done) || (m_info.proxy_handshake_sent && !m_info.proxy_connected))
			{
				if (m_header != NULL)
					mh->RemoveDelayedMessage(MSG_COMM_TIMEOUT, Id(), 0);

				OP_STATUS status = ParseHandshake();
				if (OpStatus::IsError(status))
				{
					HandleError(status);
					return;
				}
			}
		}
		break;
	case WS_CONNECTED:
	case WS_CLOSING:
		{
			mh->RemoveDelayedMessage(MSG_COMM_TIMEOUT, Id(), 0);
			OP_STATUS status = PrepareReadData();
			if (OpStatus::IsError(status))
			{
				HandleError(status);
				return;
			}
		}
		break;
	case WS_CLOSED:
			// Discard data
		break;
	}
}


OP_STATUS WebSocketProtocol::ParseHandshake()
{
	WS_NEW_DBG;

	// Static tempbuf, make sure we have one.
	if (HTTP_TmpBuf == NULL)
	{
		HTTP_TmpBufSize = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize) * 1024;
		HTTP_TmpBuf = OP_NEWA(char, HTTP_TmpBufSize);
		if (HTTP_TmpBuf == NULL)
		{
			HTTP_TmpBufSize = 0;
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	char* buf = HTTP_TmpBuf;
	UINT buf_size = HTTP_TmpBufSize;

	// m_header will be nonzero if we didn't complete the handshake in one read.
	if (m_header != NULL)
	{
		// If there's less than one kB left in the buffer increase it's size by a network buffer size
		// and copy the data across.
		if ((m_header->buffer_size - m_header->used) < 1028)
		{
			UINT new_size = m_header->used + g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize) * 1024;
			UINT8* tmp_buf = OP_NEWA(UINT8, new_size);
			if (tmp_buf == NULL)
				return OpStatus::ERR_NO_MEMORY;

			op_memcpy(tmp_buf, m_header->buffer, m_header->used);
			UINT old_len = m_header->used;
			OP_DELETEA(m_header->buffer);

			m_header->buffer = tmp_buf;
			m_header->buffer_size = new_size;
			m_header->used = old_len;
		}

		// Point to the right place in the new buffer.
		buf = (char*)m_header->buffer + m_header->used;
		buf_size = m_header->buffer_size - m_header->used;
	}

	UINT read_len = ReadData(buf, buf_size);
	OP_DBG(("Read %d bytes.", read_len));
	//UINT read_len = ReadData(buf, MIN(16, buf_size)); // Make sure we get the data in small chunks for debugging.

	// If we read into the stored header data, set the pointer back to the start
	if (m_header != NULL)
	{
		buf = (char*)m_header->buffer;
		m_header->used += read_len;
		read_len = m_header->used;
	}

	BOOL complete_handshake = FALSE;
	UINT buf_pos = 0;
	BOOL has_line = FALSE;
	UINT line_len = 0;
	UINT num_spaces = 0;

	// Check if we have read the first line and count spaces while we're at it.
	for (; line_len < read_len; ++line_len)
		if (buf[line_len] == '\n')
		{
			++line_len;
			has_line = TRUE;
			break;
		}
		else if ((char)buf[line_len] == ' ')
			++num_spaces;

	enum ResponseType { kWebSocketResponse, kProxyResponse, kProxyNeedAuthResponse };
	ResponseType response = kWebSocketResponse;

	// Only process it if we've seen the first full line.
	if (has_line)
	{
		if (line_len < 7 || buf[line_len - 2] != 0x0D || buf[line_len - 1] != 0x0A || num_spaces < 2)
			return OpStatus::ERR_PARSING_FAILED;

		UINT code_start = 0;
		for (UINT i = 0; i < line_len; ++i)
			if (buf[i] == ' ')
			{
				code_start = i + 1;
				break;
			}

		UINT code_len = 0;
		for (;; ++code_len)
			if (buf[code_start + code_len] == ' ')
				break;

		if (code_len == 0)
			return OpStatus::ERR_PARSING_FAILED;
		else if (code_len != 3)
		{
			OpString rcode;
			RETURN_IF_ERROR(UriEscape::AppendEscaped(rcode, buf + code_start, code_len, UriEscape::Ctrl));
			m_listener->OnSocketInfo(this, OpWebSocketListener::WSWrongResponseCode, rcode.CStr());
			return OpStatus::ERR_NOT_SUPPORTED;
		}

		if (!op_strncmp(buf + code_start, "200", 3))
		{
			if (m_proxy)
				response = kProxyResponse;
			else
			{
				m_listener->OnSocketInfo(this, OpWebSocketListener::WS200WithNoProxy);
				return OpStatus::ERR_NOT_SUPPORTED;
			}
		}
		else if (!op_strncmp(buf + code_start, "407", 3))
		{
			if (m_proxy)
				response = kProxyNeedAuthResponse;
			else
			{
				m_listener->OnSocketInfo(this, OpWebSocketListener::WS407WithNoProxy);
				return OpStatus::ERR_NOT_SUPPORTED;
			}
		}
		else if (!op_strncmp(buf + code_start, "403", 3))
		{
			m_listener->OnSocketInfo(this, m_proxy ? OpWebSocketListener::WS403WithProxy : OpWebSocketListener::WS403WithNoProxy);
			return OpStatus::ERR_NOT_SUPPORTED;
		}
		else if (op_strncmp(buf + code_start, "101", 3))
		{
			OpString rcode;
			RETURN_IF_ERROR(UriEscape::AppendEscaped(rcode, buf + code_start, code_len, UriEscape::Ctrl));
			m_listener->OnSocketInfo(this, OpWebSocketListener::WSWrongResponseCode, rcode.CStr());

			return OpStatus::ERR_NOT_SUPPORTED;
		}

		// Check if this is a complete handshake.
		buf_pos = line_len + 1;
		while (buf_pos + 4 <= read_len)
		{
			// Headers are delimited by the double empty line as usual.
			if (buf[buf_pos] == '\r' && buf[buf_pos+1] == '\n' && buf[buf_pos+2] == '\r' && buf[buf_pos+3] == '\n')
			{
				buf_pos += 4;
				// Depending on the response type we need a few extra bytes to have a complete handshake.
				if (read_len >= buf_pos)
				{
					complete_handshake = TRUE;
					mh->RemoveDelayedMessage(MSG_COMM_TIMEOUT, Id(), 0);

					// Check if we got any additional data in the first received buffer.
					UINT additional_data = read_len - (buf_pos);
					if (additional_data && response == kWebSocketResponse)
					{
						while (additional_data > 0)
						{
							RETURN_IF_ERROR(PrepareFrameBuffer());

							// Read the additional data into the current frame buffer.
							FrameBuffer *curr = m_recv_buffers.Last();
							UINT amt_to_copy = MIN(curr->buffer_size - curr->used, additional_data);
							op_memcpy(curr->buffer + curr->used, buf + buf_pos, amt_to_copy);
							curr->used += amt_to_copy;
							additional_data -= amt_to_copy;
						}
					}
				}
				break;
			}
			buf_pos++;
		}
	}

	if (!complete_handshake)
	{
		// If this is the first attempt, create the header buffer and copy the partial handshake.
		if (m_header == NULL)
		{
			UINT new_size = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize)*1024;
			UINT8* new_buf = OP_NEWA(UINT8, new_size);
			if (new_buf == NULL)
				return OpStatus::ERR_NO_MEMORY;

			m_header = OP_NEW(FrameBuffer, (new_buf, read_len, new_size));
			if (m_header == NULL)
			{
				OP_DELETEA(new_buf);
				return OpStatus::ERR_NO_MEMORY;
			}

			op_memcpy(m_header->buffer, buf, read_len);
		}

		// Return and try again when there's more data available.
		return OpStatus::OK;
	}

	// Allocate a buffer that holds the entire header.
	UINT header_str_len = buf_pos - line_len;
	char *header_str = OP_NEWA(char, header_str_len);
	ANCHOR_ARRAY(char, header_str);
	if (header_str == NULL)
		return OpStatus::ERR_NO_MEMORY;

	// Copy and null-terminate buffer.
	op_memcpy(header_str, buf + line_len, header_str_len-1);
	header_str[header_str_len-1] = '\0';

	if (response == kProxyResponse)
	{
		OP_DELETE(m_header);
		m_header = NULL;

		m_info.proxy_connected = 1;
		RequestMoreData();
		return OpStatus::OK;
	}
	else if (response == kProxyNeedAuthResponse)
	{
		if (m_info.doing_proxy_auth)
		{
			m_listener->OnSocketInfo(this, OpWebSocketListener::WSProxyAuthFailed);
			return OpStatus::ERR;
		}

		if (!m_auth_data)
		{
			m_auth_data = OP_NEW(authdata_st, ());
			if (!m_auth_data || !m_auth_data->auth_headers)
				return OpStatus::ERR_NO_MEMORY;
			m_auth_data->auth_headers->SetKeywordList(KeywordIndex_HTTP_MIME);
		}

		m_auth_data->connect_host = m_proxy;
		m_auth_data->connect_port = m_proxyPort;
		if (OpStatus::IsError(m_auth_data->auth_headers->SetValue(header_str, NVS_COPY_CONTENT)))
			return OpStatus::ERR_NOT_SUPPORTED;

		if (!m_mh_list)
		{
			m_mh_list = OP_NEW(MsgHndlList, ());
			if (!m_mh_list)
				return OpStatus::ERR_NO_MEMORY;
			m_mh_list->Add(mh, TRUE, TRUE, TRUE, TRUE, TRUE); // ToDo: Need to figure out what these bools do
		}

		m_info.doing_proxy_auth = TRUE;

		m_target.GetRep()->SetAuthInterface(this);
		BOOL res = urlManager->HandleAuthentication(m_target.GetRep(), TRUE);
		return res ? OpStatus::OK : OpStatus::ERR;
	}

	// Reuse the default header parsing for any cookie headers.
	m_cookie_headers.SetKeywordList(KeywordIndex_HTTP_MIME);
	m_cookie_headers.SetValue(header_str, NVS_COPY_CONTENT);

	// Do the stricter header parsing on the websockets headers.
	OP_STATUS parse_err = ParseHeaders(header_str, header_str_len-1);
	if (parse_err == OpStatus::ERR_PARSING_FAILED)
	{
		m_listener->OnSocketInfo(this, OpWebSocketListener::WSHeaderParsing);
		return OpStatus::ERR_NOT_SUPPORTED;
	}
	RETURN_IF_ERROR(parse_err);

	Header *header = GetHeader("upgrade");
	if (!header || op_strnicmp(header->value, "WebSocket", header->valueLen))
	{
		OpString headerStr, expectedStr, foundStr;
		RETURN_IF_ERROR(UriEscape::AppendEscaped(foundStr, header->value, UriEscape::Ctrl));
		RETURN_IF_ERROR(headerStr.Set(UNI_L("upgrade")));
		RETURN_IF_ERROR(expectedStr.Set(UNI_L("WebSocket")));
		m_listener->OnSocketInfo(this, OpWebSocketListener::WSHeaderMismatch, headerStr.CStr(), expectedStr.CStr(), foundStr.CStr());
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	header = GetHeader("connection");
	if (!header || op_strnicmp(header->value, "Upgrade", header->valueLen))
	{
		OpString headerStr, expectedStr, foundStr;
		RETURN_IF_ERROR(UriEscape::AppendEscaped(foundStr, header->value, UriEscape::Ctrl));
		RETURN_IF_ERROR(headerStr.Set(UNI_L("connection")));
		RETURN_IF_ERROR(expectedStr.Set(UNI_L("Upgrade")));
		m_listener->OnSocketInfo(this, OpWebSocketListener::WSHeaderMismatch, headerStr.CStr(), expectedStr.CStr(), foundStr.CStr());
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	header = GetHeader("sec-websocket-accept");
	if (header)
		RETURN_IF_ERROR(CheckServerChallengeResponse(header->value));
	else
		return OpStatus::ERR_PARSING_FAILED;

	if (m_sub_protocols.GetCount() > 0)
	{
		header = GetHeader("sec-websocket-protocol");
		if (header)
		{
			OpString8 prot;
			RETURN_IF_ERROR(prot.Set(header->value, header->valueLen));

			if (m_sub_protocols.Contains(prot.CStr()))
			{
				RETURN_IF_ERROR(m_negotiated_sub_protocol.SetFromUTF8(prot.CStr()));
			}
			else
			{
				OpString prot_utf16;
				RETURN_IF_ERROR(prot_utf16.SetFromUTF8(prot.CStr()));
				m_listener->OnSocketInfo(this, OpWebSocketListener::WSWrongSubprotocol, prot_utf16.CStr());
				return OpStatus::ERR_NOT_SUPPORTED;

			}
		}
	}

	if (m_header != NULL && complete_handshake)
	{
		OP_DELETE(m_header);
		m_header = NULL;

		// Clear out the header entries since the memory they're pointing into is
		// deleted.
		m_headers.Clear();
	}

	if (complete_handshake)
	{
		m_info.handshake_done = TRUE;
		m_state = WS_CONNECTED;
		RETURN_IF_ERROR(m_listener->OnSocketOpen(this));
		mh->RemoveDelayedMessage(MSG_COMM_TIMEOUT, Id(), 0);
		mh->PostMessage(MSG_URL_LOAD_NOW, Id() ,0);

		// Parse the additional data (or store for later if there are no full frames in it)
		RETURN_IF_ERROR(ParseReceivedData());
	}

	return OpStatus::OK;
}

void WebSocketProtocol::SetCookies()
{
	// TODO: Deal with 3rd party cookies?
	if (!m_cookie_headers.Empty())
		urlManager->HandleCookiesL(m_target.GetRep(), m_cookie_headers, m_target.GetContextId());
}

void WebSocketProtocol::Authenticate(AuthElm* auth_elm)
{
	// Fill out a base request info struct.
	Base_request_st requestInfo;
	requestInfo.origin_host = m_targetHost;
	requestInfo.origin_port = m_targetPort;
	requestInfo.connect_host = m_proxy;
	requestInfo.connect_port = m_proxyPort;

	// Zero out a request digest data.
	HTTP_Request_digest_data rdd;
	op_memset(&rdd, 0, sizeof(rdd));

	// Get the auth string with the more specific call (compared to GetAuthString) that
	// should only be used with proxy connect requests.
	OP_STATUS err = auth_elm->GetProxyConnectAuthString(m_proxy_auth_str, m_target.GetRep(), &requestInfo, rdd);
	if (OpStatus::IsError(err))
	{
		m_listener->OnSocketError(this, err);
		CloseNow(FALSE, 0, NULL);
	}
	else
	{
		m_info.proxy_handshake_sent = FALSE;
		m_info.proxy_connected = FALSE;
		m_proxy_auth_id = auth_elm->GetId();
		Load();
	}
	rdd.ClearAuthentication();
}

void WebSocketProtocol::FailAuthenticate(int mode)
{
	m_listener->OnSocketInfo(this, OpWebSocketListener::WSProxyAuthFailed);
	m_proxy_auth_id = false;
	CloseNow(FALSE, 0, NULL);
}

BOOL WebSocketProtocol::CheckSameAuthorization(AuthElm* auth_elm, BOOL proxy)
{
	return proxy && auth_elm && m_proxy_auth_id == auth_elm->GetId();
}

authdata_st *WebSocketProtocol::GetAuthData()
{
	return m_auth_data;
}

MsgHndlList *WebSocketProtocol::GetMessageHandlerList()
{
	return m_mh_list;
}

OP_STATUS WebSocketProtocol::ParseHeaders(char* str, unsigned int len)
{
	WS_NEW_DBG;
	char *end = str + len;
	while (str != end)
	{
		// Add a new header to the list (if the parsing is successful the last
		// one will be removed and deleted again).
		Header *header = OP_NEW(Header, ());
		if (!header)
			return OpStatus::ERR_NO_MEMORY;
		header->Into(&m_headers);

		// Parse the header name.
		while (str != end)
		{
			if (*str == '\x0D')
			{
				++str;
				break;
			}
			else if (*str == '\x0A')
				return OpStatus::ERR_PARSING_FAILED;
			else if (*str == '\x3A')
			{
				*str++ = '\0';
				break;
			}
			else
			{
				if (!header->name)
					header->name = str;
				++header->nameLen;
				// Convert any uppercase letters to lowercase.
				if ('\x41' <= *str && *str <= '\x5A')
					*str = *str + 0x20;
				++str;
			}
		}

		// If the name is empty we've either successfully parsed the entire buffer
		// or the headers are ill-formed.
		if (!header->name)
		{
			header->Out();
			OP_DELETE(header);
			if (str[0] != '\0')
				return OpStatus::ERR_PARSING_FAILED;
		}
		else
		{
			// Early out if we've parsed a header with an empty name.
			if (header->nameLen == 0)
				return OpStatus::ERR_PARSING_FAILED;

			// Parse the header value.
			int count = 0;

			while (str != end)
			{
				++count;
				if (*str == '\x20' && count == 1)
					++str;
				else if (*str == '\x0D')
				{
					*str++ = '\0';
					break;
				}
				else if (*str == '\x0A')
					return OpStatus::ERR_PARSING_FAILED;
				else
				{
					if (!header->value)
						header->value = str;
					++header->valueLen;
					++str;
				}
			}

			if (*str++ != '\x0A')
				return OpStatus::ERR_PARSING_FAILED;
		}
	}

	// Check the required headers and make sure there are exactly one of each.
#ifdef HAS_COMPLEX_GLOBALS
	const char *required_headers[] = { "upgrade", "connection", "sec-websocket-accept" };
#else //HAS_COMPLEX_GLOBALS
	const char required_headers[3][23] = { "upgrade", "connection", "sec-websocket-accept" }; /* ARRAY OK 2012-04-26 haavardm */
#endif //HAS_COMPLEX_GLOBALS
	for (unsigned int j = 0; j < ARRAY_SIZE(required_headers); ++j)
	{
		int count = 0;
		for (Header *h = m_headers.First(); h; h = h->Suc())
			if (!op_strnicmp(h->name, required_headers[j], h->nameLen) && op_strlen(required_headers[j]) == h->nameLen)
				++count;

		if (count != 1)
		{
			OpString str;
			RETURN_IF_ERROR(str.Set(required_headers[j]));
			if (count == 0)
				m_listener->OnSocketInfo(this, OpWebSocketListener::WSHeaderMissing, str.CStr());
			else
				m_listener->OnSocketInfo(this, OpWebSocketListener::WSHeaderDuplicate, str.CStr());
			return OpStatus::ERR_NOT_SUPPORTED;
		}
	}

	// Check the optional headers and make sure there are at most one of each.
#ifdef HAS_COMPLEX_GLOBALS
	const char *optional_headers[] = { "sec-websocket-protocol", "sec-websocket-extensions", "sec-websocket-version", "origin" };
#else //HAS_COMPLEX_GLOBALS
	const char optional_headers[4][26] = { "sec-websocket-protocol", "sec-websocket-extensions", "sec-websocket-version", "origin" }; /* ARRAY OK 2012-04-26 haavardm */
#endif //HAS_COMPLEX_GLOBALS
	for (unsigned int j = 0; j < ARRAY_SIZE(optional_headers); ++j)
	{
		int count = 0;
		for (Header *h = m_headers.First(); h; h = h->Suc())
			if (!op_strnicmp(h->name, optional_headers[j], h->nameLen) && op_strlen(optional_headers[j]) == h->nameLen)
				++count;

		if (count > 1)
		{
			OpString str;
			RETURN_IF_ERROR(str.Set(required_headers[j]));
			m_listener->OnSocketInfo(this, OpWebSocketListener::WSHeaderDuplicate, str.CStr());
			return OpStatus::ERR_NOT_SUPPORTED;
		}
	}

	return OpStatus::OK;
}

WebSocketProtocol::Header *WebSocketProtocol::GetHeader(const char* name) const
{
	WS_NEW_DBG;
	for (Header *h = m_headers.First(); h; h = h->Suc())
		if (!op_strncmp(h->name, name, h->nameLen))
			return h;
	return NULL;
}

OP_STATUS WebSocketProtocol::FrameBuffer::Create()
{
	WS_NEW_DBG;
	int buf_len = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize)*1024;
	buffer = OP_NEWA(UINT8, buf_len);

	RETURN_VALUE_IF_NULL(buffer, OpStatus::ERR_NO_MEMORY);
	buffer_size = buf_len;
	return OpStatus::OK;
}

void WebSocketProtocol::FrameBuffer::Reset()
{
	WS_NEW_DBG;
	OP_ASSERT(frames.Empty());
	consumed = 0;
	used = 0;
}


// If we have a completed frame the buffer is considered full when it's within a threshold
// of the allocated size. If the buffer ends with an incomplete frame we use up the whole
// buffer before it's considered full.
BOOL WebSocketProtocol::FrameBuffer::IsFull()
{
	WS_NEW_DBG;
	if (!frames.Empty() && frames.Last()->finished)
		return used + WEBSOCKET_BUFFER_ALLOC_THRESHOLD > buffer_size;
	else
		return buffer_size == used;
}

// Check if the last frame of this buffer is continued in another buffer.
BOOL WebSocketProtocol::FrameBuffer::IsContinued()
{
	WS_NEW_DBG;
	if (!frames.Empty())
	{
		Frame *frame = frames.Last();
		return !frame->finished;
	}
	return false;
}

BOOL WebSocketProtocol::FrameBuffer::IsConsumed()
{
	return consumed == used && frames.Cardinal() == 0;
}

// Find frames in the current buffer.
OP_STATUS WebSocketProtocol::FindFrames(FrameBuffer* frame_buffer)
{
	WS_NEW_DBG;

	while (frame_buffer->consumed < frame_buffer->used)
	{
		UINT index = frame_buffer->consumed;
		// If there's no "open" frame, parse new frame header
		if (frame_buffer->frames.Empty() || frame_buffer->frames.Last()->finished)
		{
			INT frame_start = index;
			if (frame_buffer->used < 2)
				return OpStatus::OK; // Not enough data has been read

			BOOL invalid_frame = FALSE;

			UINT32 frame_control_bits = (frame_buffer->buffer[index] << 8) | frame_buffer->buffer[index + 1] ;
			index += 2;

			// Servers are not allowed to mask data.
			if (frame_control_bits & Frame::MASKED)
				invalid_frame = TRUE;

			// Calculate correct data length;
			UINT seven_bit_length = (frame_control_bits & Frame::LENGTH_BIT_MASK);

			INT64 frame_data_length = 0;
			int num_extended_length_bytes = 0;

			if (seven_bit_length < 126)
				frame_data_length = seven_bit_length;
			else if (seven_bit_length == 126)
				num_extended_length_bytes = 2; // 16 bit length.
			else if (seven_bit_length == 127)
				num_extended_length_bytes = 8; // 64 bit length.

			if (index + num_extended_length_bytes > frame_buffer->used)
				return OpStatus::OK; // Not enough data has been read

			// If we have num_extended_length_bytes > 0,
			// that is we have a 16 or 64 bit length, set length from next extended_length_bytes bytes.
			for (int idx = 0; idx < num_extended_length_bytes; idx++)
			{
				frame_data_length <<= 8;
				frame_data_length |= frame_buffer->buffer[index++];
			}

			// Websockets allow up to 63 bit frame lengths.
			// We never expect this kind of data lengths, but it's important
			// to check the size given by the frame to avoid integer overflow
			// and signed/unsigned issues.
			if (frame_data_length & (static_cast<UINT64>(1) << 63))
				return OpStatus::ERR_OUT_OF_RANGE;

			// Also check if we are able to store this size on file.
			if (frame_data_length > FILE_LENGTH_MAX)
				return OpStatus::ERR_OUT_OF_RANGE;

			// Make sure controls frames never are split across buffers. Control frames have max length 125.
			if (frame_control_bits & Frame::OPERATION_CODE_CONTROL_FRAME)
			{
				if (frame_data_length > 125)
					return OpStatus::ERR_OUT_OF_RANGE;
				else if (index + frame_data_length > frame_buffer->buffer_size)
				{
					// This buffer could not hold a full control frame, we create a new buffer and copies the rest.
					FrameBuffer* new_frame_buffer = OP_NEW(FrameBuffer, ());
					if (new_frame_buffer == NULL)
						return OpStatus::ERR_NO_MEMORY;

					OP_STATUS err = new_frame_buffer->Create();
					if (err != OpStatus::OK)
					{
						OP_DELETE(new_frame_buffer);
						return err;
					}

					op_memcpy(new_frame_buffer->buffer, frame_buffer->buffer + frame_buffer->consumed, frame_buffer->used - frame_buffer->consumed);
					new_frame_buffer->used = frame_buffer->used - frame_buffer->consumed;

					frame_buffer->buffer_size = frame_buffer->consumed; // we limit the buffer size, to signal buffer is full
					frame_buffer->used = frame_buffer->consumed;
					return OpStatus::OK;
				}
				else if (index + frame_data_length > frame_buffer->used)
					return OpStatus::OK; // Not enough data, wait until we get more.
			}

			frame_buffer->consumed = index;

			Frame::FrameOperationCode operation_code;
			if (!invalid_frame)
				operation_code = static_cast<Frame::FrameOperationCode>(frame_control_bits & Frame::OPERATION_CODE_BIT_MASK);
			else
				operation_code = Frame::INTERNALCODE_INVALID_FRAME;

			OpAutoPtr<Frame> frame(OP_NEW(Frame, ()));
			if (frame.get() == NULL)
				return OpStatus::ERR_NO_MEMORY;

			frame->operation_code = operation_code;

			frame->payload_start = index;
			frame->frame_start = frame_start;
			frame->data_left_to_receive = frame_data_length;
			frame->frame_control_bits = frame_control_bits & (~Frame::LENGTH_BIT_MASK);

			frame->delete_file = TRUE; // In case the connection is closed, this will automatically delete the file.

			if (operation_code == Frame::OPCODE_CONTINUATION_FRAME && m_current_disk_frame)
			{
				frame->spool_to_disk = TRUE;
			}
			else if (operation_code == Frame::OPCODE_BINARY_FRAME && frame_data_length > g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize)*1024)
			{
				frame->spool_to_disk = TRUE;
				OpString name;
				RETURN_IF_ERROR(g_libcrypto_random_generator->GetRandomHexString(name, 24));

				RETURN_IF_ERROR(frame->file.Construct(name, OPFILE_TEMP_FOLDER));
				RETURN_IF_ERROR(frame->file.Open(OPFILE_WRITE | OPFILE_READ));

				// When storing to disk, we only keep the first frame if the message is fragmented over several frames.
				// Thus we discard all data from successive frames after writing the frame data to the file in m_current_disk_frame.
				m_current_disk_frame = frame.get();
			}

			frame.release()->Into(&frame_buffer->frames);
		}

		// Parse the frame data.
		// Get the current open frame.
		Frame *frame = frame_buffer->frames.Last();
		if (frame)
		{
			// If we found a complete frame, close it and mark the data as consumed.
			if (static_cast<UINT64>(frame->data_left_to_receive) + index <= frame_buffer->used)
			{
				index += static_cast<UINT>(frame->data_left_to_receive);
				frame->payload_end = index;
				frame->finished = TRUE;
				frame_buffer->consumed = index;

			}
			// If we hit the end of the buffer, close the frame and mark it as continued
			// in the next buffer. Consume all data.
			else if (frame_buffer->used == frame_buffer->buffer_size)
			{
				frame->payload_end = frame_buffer->buffer_size + 1;
				frame_buffer->consumed = frame_buffer->used;
			}
			else
				// We haven't received enough data to close this frame yet.
				return OpStatus::OK;

			if (frame->spool_to_disk)
			{
				UINT frame_payload_end = MIN(frame->payload_end, frame_buffer->buffer_size);
				UINT frame_payload_start = frame->payload_start;
				UINT frame_start = static_cast<UINT>(MAX(frame->frame_start, 0));

				RETURN_IF_ERROR(m_current_disk_frame->file.Write(frame_buffer->buffer + frame_payload_start, frame_payload_end - frame_payload_start));
				frame->data_left_to_receive -= (frame_payload_end - frame_payload_start);

				UINT data_left_in_buffer = frame_buffer->buffer_size - frame_buffer->consumed;
				op_memmove(frame_buffer->buffer + frame_start, frame_buffer->buffer + frame_buffer->consumed, data_left_in_buffer);
				frame_buffer->used -= frame_buffer->consumed;

				BOOL delete_frame = FALSE;
				/* If the message has been fragmented over several frames, and this is not the first frame (the disk frame),
				 * and message is finished,then we delete this frame, as all data has been saved to the file in the first frame of the message.
				 */
				if (frame != m_current_disk_frame && frame->finished)
					delete_frame = TRUE;

				if (frame->data_left_to_receive == 0 && frame->frame_control_bits & Frame::FIN)
				{
					m_current_disk_frame->finished = TRUE;
					m_current_disk_frame->frame_control_bits |= Frame::FIN;
					m_current_disk_frame->file.Close();
					m_current_disk_frame = NULL;
				}

				/* If all frames (previous to the disk frame has been handled,
				  * we move the start pointer to the beginning to make room for
				  * more data.
				  */
				if (data_left_in_buffer == 0 && (m_current_disk_frame == frame || m_current_disk_frame->Suc() == frame))
					frame_start = 0;

				frame->frame_start = frame_start;
				// We remove the consumed data
				frame->payload_start = frame_start;
				frame->payload_end = frame_start;
				frame_buffer->consumed = frame_start;

				OP_ASSERT(frame_buffer->used < frame_buffer->buffer_size);

				OP_ASSERT(frame->data_left_to_receive || (!frame->data_left_to_receive && frame->finished));

				// Since we only keep the first frame of a message when writing
				// to disk, we write successive frames to disk and delete them.
				if (delete_frame)
					OP_DELETE(frame);
			}
		}
	}
	return OpStatus::OK;
}

OP_STATUS WebSocketProtocol::PrepareFrameBuffer()
{
	WS_NEW_DBG;
	// Make sure we have a FrameBuffer at the end of the list that we can read data into.
	if (m_recv_buffers.Empty() || (m_recv_buffers.Last()->IsFull() && !m_current_disk_frame))
	{
		FrameBuffer* frame_buffer = OP_NEW(FrameBuffer, ());
		if (frame_buffer == NULL)
			return OpStatus::ERR_NO_MEMORY;

		OP_STATUS err = frame_buffer->Create();
		if (err != OpStatus::OK)
		{
			OP_DELETE(frame_buffer);
			return err;
		}

		// If the last frame from the previous buffer is continued, mark the new frame to be a continuation.
		if (!m_recv_buffers.Empty())
		{
			FrameBuffer* prev_buffer = m_recv_buffers.Last();
			if (prev_buffer->IsContinued())
			{
				Frame* prev_frame = prev_buffer->frames.Last();

				Frame* frame = OP_NEW(Frame, ());
				if (frame == NULL)
				{
					OP_DELETE(frame_buffer);
					return OpStatus::ERR_NO_MEMORY;
				}

				frame->frame_start = -1;
				frame->payload_start = 0;
				frame->operation_code = prev_frame->operation_code;
				frame->frame_control_bits = prev_frame->frame_control_bits;

				OP_ASSERT(prev_frame->spool_to_disk == FALSE);
				frame->spool_to_disk = prev_frame->spool_to_disk;

				UINT prev_len = static_cast<UINT>(MIN(prev_frame->payload_end, prev_buffer->buffer_size) - prev_frame->payload_start);

				OP_ASSERT(prev_len <= prev_buffer->used);

				frame->data_left_to_receive = prev_frame->data_left_to_receive - prev_len;

				frame->Into(&frame_buffer->frames);
			}
		}

		frame_buffer->Into(&m_recv_buffers);
	}
	return OpStatus::OK;
}


OP_STATUS WebSocketProtocol::ParseReceivedData()
{
	WS_NEW_DBG;
	// Parse the data and find frames.
	FrameBuffer *curr = m_recv_buffers.Last();
	if (curr == NULL)
		return OpStatus::OK;

	RETURN_IF_ERROR(FindFrames(curr));

	BOOL has_complete_messages = FALSE;

	// We use this to check if the previous frame has been read (different from new frame).
	Frame *prev_frame = NULL;
	do
	{
		RETURN_IF_ERROR(HandlePrioritizedControlFramesAndCheckForCompleteMessages(has_complete_messages));

		if (m_state != WS_CONNECTED) // Ignore the rest of control frames if we are closing or closed.
		{
			m_recv_buffers.Clear();
			return OpStatus::OK;
		}

		FrameBuffer* read_buf = m_recv_buffers.First();
		if (read_buf && has_complete_messages)
		{
			Frame *frame = read_buf->frames.First();
			if (frame)
			{
				RETURN_IF_ERROR(CheckFrame(frame));

				// If we get the same frame twice, the previous frame was not read. If we continue we'll
				// end up in a never ending loop. We return error.
				if (frame == prev_frame)
				{
					OP_ASSERT(!"Previous frame was not read");
					return OpStatus::ERR;
				}

				prev_frame = frame;

				// We copy expected frame type, so we can reset m_info. Note that this info only makes sense if previous
				// frame was sent without Frame::FIN flag.
				m_info.waiting_for_continuation_frame = FALSE;
				m_info.expected_continuation_frame_type = Frame::INTERNALCODE_INVALID_FRAME;

				// All control frames should have been handled by HandlePrioritizedControlFrames() above.
				OP_ASSERT((frame->frame_control_bits & Frame::OPERATION_CODE_CONTROL_FRAME) == 0);

				switch (frame->operation_code)
				{
				case Frame::OPCODE_CONTINUATION_FRAME:
					OP_ASSERT(!"SHOULD NOT HAPPEN. A message has been fragmented into many continuation frames. Only first frame of a fragmented message should end up here.");
					return OpStatus::ERR_PARSING_FAILED;

				case Frame::OPCODE_TEXT_FRAME:
					RETURN_IF_ERROR(m_listener->OnSocketMessage(this, FALSE, FALSE));
					continue; // Continue as the frame will be consumed by the listener.

				case Frame::OPCODE_BINARY_FRAME:
					RETURN_IF_ERROR(m_listener->OnSocketMessage(this, TRUE, frame->spool_to_disk));
					continue; // Continue as the frame will be consumed by the listener.

				default:
					{
					if (frame->finished)
						m_listener->OnSocketError(this, OpStatus::ERR);

					// Report an error about the unsupported frame type.
					OpString str;
					RETURN_IF_ERROR(str.AppendFormat(UNI_L("0x%04x"), frame->operation_code));
					m_listener->OnSocketInfo(this, OpWebSocketListener::WSInvalidFrameType, str.CStr());
					}
					break;
				}

				if ((frame->frame_control_bits & Frame::FIN) == 0)
				{
					m_info.waiting_for_continuation_frame = TRUE;
					m_info.expected_continuation_frame_type = frame->operation_code;
				}
				else
				{
					m_info.waiting_for_continuation_frame = FALSE;
					m_info.expected_continuation_frame_type = Frame::INTERNALCODE_INVALID_FRAME;
				}

				Frame *consumed_frame = frame;
				frame = frame->Suc();
				consumed_frame->Out();
				OP_DELETE(consumed_frame);
				CheckBufferStatus(read_buf, frame);
			}
		}
	} while (has_complete_messages);

	return OpStatus::OK;
}


OP_STATUS WebSocketProtocol::PrepareReadData()
{
	WS_NEW_DBG;

	// Make sure we have buffers to read to.
	RETURN_IF_ERROR(PrepareFrameBuffer());

	// Read some data into the current frame buffer.
	FrameBuffer *curr = m_recv_buffers.Last();
	int read_len = ReadData((char*)curr->buffer + curr->used, curr->buffer_size - curr->used);
	OP_DBG(("Read %d bytes.", read_len));
	if (read_len == 0)
		return OpStatus::OK;

	curr->used +=read_len;

	return ParseReceivedData();
}


unsigned int WebSocketProtocol::ReadData(char* buf, unsigned blen)
{
	WS_NEW_DBG;
	if (m_state != WS_CONNECTED && !(m_state == WS_CONNECTING && !m_info.handshake_done) && m_state != WS_CLOSING)
	{
		OP_DBG(("Called when not connected! m_state: %d", m_state));
		m_listener->OnSocketError(this, OpStatus::ERR_NOT_SUPPORTED);
		return 0;
	}

	return ProtocolComm::ReadData(buf, blen);
}


void WebSocketProtocol::SendData(char* buf, unsigned blen)
{
	WS_NEW_DBG;
	if (m_state != WS_CONNECTED && !(m_state == WS_CONNECTING && !m_info.handshake_done) && m_state !=  WS_CLOSING)
	{
		OP_DBG(("Called when not connected! m_state: %d", m_state));
		OP_DELETEA(buf);
		m_listener->OnSocketError(this, OpStatus::ERR_NOT_SUPPORTED);
		return;
	}

	ProtocolComm::SendData(buf, blen);
}


OP_STATUS WebSocketProtocol::SetCallbacks(MessageObject* master, MessageObject* parent)
{
	WS_NEW_DBG;
	static const OpMessage messages[] =
	{
		MSG_COMM_LOADING_FINISHED,
		MSG_COMM_LOADING_FAILED,
		MSG_URL_LOAD_NOW
	};

	RETURN_IF_ERROR(mh->SetCallBackList((parent ? parent : master), Id(), messages, ARRAY_SIZE(messages)));
	RETURN_IF_ERROR(mh->SetCallBack(this, MSG_COMM_TIMEOUT, Id()));
	RETURN_IF_ERROR(mh->SetCallBack(this, MSG_URL_WEBSOCKET_SEND_NEXT_SLICE, (MH_PARAM_1)this));

	return ProtocolComm::SetCallbacks(master,this);
}


void WebSocketProtocol::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	WS_NEW_DBG;
	OP_DBG(("Message: %d, par1: %x, par2: %x", msg, par1, par2));

	switch (msg)
	{
	case MSG_URL_WEBSOCKET_SEND_NEXT_SLICE:
		{
			if (OpStatus::IsError(SendMessageSliceFromQueue()))
			{
				CloseNow(FALSE, 0, NULL);
				mh->PostOOMCondition(TRUE);
			}
		}
		break;
	case MSG_COMM_LOADING_FINISHED:
		{
			OP_DBG(("MSG_COMM_LOADING_FINISHED"));
			if (Connected())
			{
				if (!m_info.doing_proxy_auth)
					CloseNow(FALSE, 0, NULL);
			}
		}
		break;
	case MSG_COMM_LOADING_FAILED:
		{
			OP_DBG(("MSG_COMM_LOADING_FAILED"));

			// If this is the APC load handler failing we just clean up after it and proceed without APC.
			if (m_autoProxyLoadHandler)
			{
				OpenInternal();

				// Clean up after the APC load handler.
				g_main_message_handler->RemoveCallBacks(this, m_autoProxyLoadHandler->Id());
				SComm *temp = m_autoProxyLoadHandler;
				m_autoProxyLoadHandler = NULL;
				SComm::SafeDestruction(temp);
				break;
			}

			m_listener->OnSocketError(this, OpStatus::ERR);

			// ToDo: This could be Autoproxy failing to load.
			if (par1 == ERR_COMM_CONNECT_FAILED)
				Close(0, NULL);
			else
			{
				CloseNow(FALSE, 0, NULL);

				// Report that we failed to connect.
				OpString str;
				RETURN_VOID_IF_ERROR(GetURL(str));
				m_listener->OnSocketInfo(this, OpWebSocketListener::WSFailedToConnect, str.CStr());
			}

		}
		break;
	case MSG_COMM_PROXY_DETERMINED:
		{
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
			// Fill out a base request structure and parse the apc string.
			Base_request_st requestInfo;
			requestInfo.origin_host = m_targetHost;
			requestInfo.origin_port = m_targetPort;
			GetAutoProxyL(m_auto_proxy.CStr(), &requestInfo);

			// If we should use a proxy, set the proxy info before calling OpenInternal.
			if (requestInfo.proxy != NO_PROXY)
			{
				m_proxy = requestInfo.connect_host;
				m_proxyPort = requestInfo.connect_port;
			}
#endif //SUPPORT_AUTO_PROXY_CONFIGURATION
			OpenInternal();

			// Clean up after the APC load handler.
			g_main_message_handler->RemoveCallBacks(this, m_autoProxyLoadHandler->Id());
			SComm *temp = m_autoProxyLoadHandler;
			m_autoProxyLoadHandler = NULL;
			SComm::SafeDestruction(temp);
		}
		break;
	case MSG_COMM_TIMEOUT:
		{
			OP_DBG(("MSG_COMM_TIMEOUT"));
			CloseNow(FALSE, 0, NULL);
		}
		break;
	default:
		OP_ASSERT(!"This should not happen");
		break;
	}
}


void WebSocketProtocol::SetProgressInformation(ProgressState progress_level, unsigned long progress_info1, const void* progress_info2)
{
	WS_NEW_DBG;

	if (START_REQUEST == progress_level && m_state == WS_CONNECTING)
	{
		mh->RemoveDelayedMessage(MSG_COMM_TIMEOUT, Id(), 0);
		if (!mh->PostDelayedMessage(MSG_COMM_TIMEOUT, Id(), 0, WEBSOCKET_CONNECT_TIMEOUT))
		{
			Stop();
			m_listener->OnSocketError(this, OpStatus::ERR_NO_MEMORY);
		}
	}
	else if (UPLOADING_PROGRESS == progress_level && (m_state == WS_CONNECTED || m_state == WS_CLOSING))
	{
		OpFileLength size_of_progress_data = (OpFileLength)progress_info1;

		m_total_raw_buffered_amount -= size_of_progress_data;
		OP_ASSERT(m_total_raw_buffered_amount >= 0);

		mh->PostMessage(MSG_URL_WEBSOCKET_SEND_NEXT_SLICE, (MH_PARAM_2)this ,0);

		BufferedAmount *amount = m_buffered_amount_tracker.Get(0);
		OP_ASSERT(amount);

		OpFileLength leftover_progress_data_ = size_of_progress_data;
		while (amount && leftover_progress_data_)
		{
			if (amount->m_raw_frame_buffered_amount - leftover_progress_data_ < 0 )
			{
				// We received progress on data than this frame contains.
				// We set buffered amount  for this frame to zero, and continues
				// on next frame with the leftover data (temp_raw_len).

				leftover_progress_data_ -= amount->m_raw_frame_buffered_amount; // Calculate leftover;
				amount->m_raw_frame_buffered_amount = 0;

				OP_ASSERT(m_buffered_amount_tracker.Get(1) && "We got progress about more data than has been sent. Something muffins is going on.");
			}
			else
			{
				amount->m_raw_frame_buffered_amount -= leftover_progress_data_;
				leftover_progress_data_ = 0;
			}

			if (amount->m_raw_frame_length - amount->m_raw_frame_buffered_amount >= amount->m_over_head)
			{
				// All over overhead protocol data has been sent, we can start lowering the buffered frame data.
				m_buffered_amount -= amount->m_frame_data_buffered_amount - amount->m_raw_frame_buffered_amount;
				amount->m_frame_data_buffered_amount = amount->m_raw_frame_buffered_amount;
			}

			if (amount->m_raw_frame_buffered_amount == 0)
			{
				if (amount->m_message_finished)
				{
					// Put in any finished callbacks here
				}
				// A full frame has been accounted for. We'll start on the next frame.
				m_buffered_amount_tracker.Delete(0,1);
				amount = m_buffered_amount_tracker.Get(0);
			}
			else
			{
				// There is more data to send for this frame. All progress
				// data must  have been accounted for at this stage. We break
				// of the loop, and wait for more progress later.
				OP_ASSERT(leftover_progress_data_ == 0);
				amount = 0;
				break;
			}
		}

		OP_ASSERT(leftover_progress_data_ == 0); // all progress data must have been accounted for;

		if (m_total_raw_buffered_amount <= 0)
		{
			OP_ASSERT(m_buffered_amount_tracker.GetCount() == 0);
			OP_ASSERT(m_buffered_amount == 0);
			m_buffered_amount = 0;
			m_total_raw_buffered_amount = 0;
		}

		if (m_state == WS_CLOSING && m_total_raw_buffered_amount <= 0)
		{
			CloseNow(m_closing_info.clean, m_closing_info.status_code, m_closing_info.reason.CStr());
		}
	}
#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_)
	else if (progress_level == GET_APPLICATIONINFO)
	{
		URL *target = (URL *) progress_info2;
		if(m_target.GetRep())
		{
			URL temp(m_target.GetRep(), (char *)NULL);
			*target = temp;
		}
	}
	else if (progress_level == GET_ORIGINATING_WINDOW)
	{
		*((const OpWindow **)progress_info2) = mh->GetWindow() ? mh->GetWindow()->GetOpWindow() : NULL;
	}
	else if (progress_level == RESTART_LOADING && m_state == WS_CONNECTING)
	{
		if(progress_info2)
			*((BOOL *) progress_info2) = TRUE;
	}
#endif // _SSL_SUPPORT_ && !_EXTERNAL_SSL_SUPPORT_

}

OP_STATUS WebSocketProtocol::SendCloseToServer(BOOL callback_clean, unsigned short callback_code, const uni_char* callback_reason, const UINT8* data_to_server, UINT data_to_server_length)
{
	WS_NEW_DBG;
	// Start the closing handshake.

	OP_ASSERT(m_state == WS_CONNECTED);

	m_state = WS_CLOSING;
	m_closing_info.clean = callback_clean;
	m_closing_info.status_code = callback_code;
	OpStatus::Ignore(m_closing_info.reason.Set(callback_reason));
	if (!m_info.closing_signaled)
	{
		RETURN_IF_ERROR(m_listener->OnSocketClosing(this));
		m_info.closing_signaled = TRUE;
	}

	OpFileLength buffered_amount = 0;
	RETURN_IF_ERROR(SendRawData(data_to_server, data_to_server_length, buffered_amount, Frame::OPCODE_CONNECTION_CLOSE, TRUE));

	if (m_state != WS_CLOSED)
	{
		// If not all data as been sent out on network, we set a timeout
		if (!mh->PostDelayedMessage(MSG_COMM_TIMEOUT, Id(), 0, WEBSOCKET_CLOSE_TIMEOUT))
		{
			Stop();
			m_listener->OnSocketError(this, OpStatus::ERR_NO_MEMORY);
		}
	}
	return OpStatus::OK;
}

void WebSocketProtocol::CloseNow(BOOL clean, unsigned short code, const uni_char* reason)
{
	WS_NEW_DBG;
	Stop();
	SetUrlLoadStatus(URL_UNLOADED);
	if (!m_info.close_signaled)
	{
		m_info.close_signaled = TRUE;
		if (code == 0)
			code = Frame::CLOSE_RESERVED_APPLICATION_LAYER_NO_STATUS;

		m_listener->OnSocketClosed(this, clean, code, reason);
	}
	// Remove the close handshake timeout.
	mh->RemoveDelayedMessage(MSG_COMM_TIMEOUT, Id(), 0);

}

#endif // WEBSOCKETS_SUPPORT
